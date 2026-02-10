"""
Swarm API - Ground station control for floppi ESP32 drones.

Run with: python -m uvicorn src.main:app --host 0.0.0.0 --port 8080
"""

import logging
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from .config import load_config
from .manager import DroneManager
from .api.drones import router as drones_router
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
async def health():
    """Health check."""
    return {"status": "ok"}
