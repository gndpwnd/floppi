"""
Swarm API - Ground station control for floppi ESP32 drones.

Run with the configured bind host/port (reads server.host / server.port from
config.json):
    python -m src.main

Or pass an explicit bind (the config host/port are the defaults if you use
uvicorn directly):
    python -m uvicorn src.main:app --host 0.0.0.0 --port 8080

Bind host (F-11): server.host defaults to "0.0.0.0" (reachable on every
interface — preserves prior behaviour). Set it to "127.0.0.1" to restrict the
unauthenticated-by-default control plane to localhost, or to a specific LAN
interface IP for a single trusted network.
"""

import logging
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, Request
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from .config import load_config
from .manager import DroneManager
from .api.drones import router as drones_router
from .api.fleet import router as fleet_router
from .api.system import router as system_router
from .api.ws import router as ws_router, broadcast_telemetry

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("swarm_api")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application startup and shutdown."""
    config = load_config()
    app.state.config = config

    # Create and start drone manager
    manager = DroneManager(config)

    # Wire telemetry from drones → dashboard WebSocket clients
    def on_telemetry(mac: str, data: dict):
        import asyncio
        try:
            loop = asyncio.get_event_loop()
            if loop.is_running():
                loop.create_task(broadcast_telemetry(mac, data))
        except RuntimeError:
            pass

    manager.on_telemetry(on_telemetry)
    app.state.manager = manager

    await manager.start()
    logger.info("Swarm API started — %d drone(s)", len(config.drones))

    # Security posture warning (F-01/F-11). When no server auth token is set the
    # control plane is open to anyone who can reach the bind address.
    if not config.server.auth_token:
        logger.warning(
            "SERVER AUTH DISABLED — every control endpoint (commands, "
            "batch-command, disarm, config mutation, /ws/dashboard) is "
            "UNAUTHENTICATED. Bound to %s:%d. Set server.auth_token in "
            "config.json and run only on a trusted/isolated network.",
            config.server.host, config.server.port,
        )
    else:
        logger.info("Server auth ENABLED — control endpoints require a token.")

    yield

    await manager.stop()
    logger.info("Swarm API shut down")


app = FastAPI(
    title="Swarm API",
    description="Ground station control for floppi ESP32 drones",
    version="0.1.0",
    lifespan=lifespan,
)

# Include API routes
app.include_router(drones_router)
app.include_router(fleet_router)
app.include_router(system_router)
app.include_router(ws_router)

# Serve static files (CSS, JS, etc.)
static_dir = Path(__file__).parent / "static"
if static_dir.exists():
    app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")


@app.get("/")
async def root():
    """Serve the dashboard."""
    index = Path(__file__).parent / "static" / "index.html"
    if index.exists():
        return FileResponse(index)
    return {"name": "swarm_api", "version": "0.1.0", "dashboard": "no index.html found"}


@app.get("/health")
async def health(request: Request):
    """Health check with fleet summary."""
    manager = request.app.state.manager
    fleet = manager.fleet_status()
    return {
        "status": "ok",
        "version": "0.1.0",
        "drones_total": fleet["total"],
        "drones_online": fleet["online"],
    }


def main() -> None:
    """Launch uvicorn bound to the configured host/port (F-11).

    server.host defaults to "0.0.0.0" for backward compatibility. Override it
    in config.json (e.g. "127.0.0.1") to restrict the bind interface.
    """
    import uvicorn

    config = load_config()
    uvicorn.run(
        "src.main:app",
        host=config.server.host,
        port=config.server.port,
    )


if __name__ == "__main__":
    main()
