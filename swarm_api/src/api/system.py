"""
System and configuration API routes.

Endpoints for reading/updating server configuration, server info,
and programmatic access to config.json settings.
"""

import time

from fastapi import APIRouter, Request
from pydantic import BaseModel, Field

from ..config import NetworkConfig, save_config

router = APIRouter(prefix="/api/system", tags=["system"])

_start_time = time.time()


class NetworkConfigUpdate(BaseModel):
    """Update network configuration settings."""
    command_rate_hz: int = Field(default=None, ge=1, le=50)
    telemetry_poll_interval_ms: int = Field(default=None, ge=100, le=5000)
    connection_timeout_ms: int = Field(default=None, ge=500, le=10000)


@router.get("/info")
async def system_info(request: Request):
    """Server information: version, uptime, drone count, connected dashboard clients."""
    manager = request.app.state.manager
    from ..api.ws import _dashboard_clients

    fleet = manager.fleet_status()
    return {
        "version": "0.1.0",
        "uptime_s": round(time.time() - _start_time, 1),
        "drones_total": fleet["total"],
        "drones_online": fleet["online"],
        "drones_armed": fleet["armed"],
        "dashboard_clients": len(_dashboard_clients),
        "config": {
            "host": request.app.state.config.server.host,
            "port": request.app.state.config.server.port,
            "command_rate_hz": request.app.state.config.network.command_rate_hz,
            "telemetry_poll_interval_ms": request.app.state.config.network.telemetry_poll_interval_ms,
        },
    }


@router.get("/config")
async def get_config(request: Request):
    """Read current configuration (all sections)."""
    config = request.app.state.config
    return config.model_dump()


@router.get("/config/network")
async def get_network_config(request: Request):
    """Read network configuration."""
    return request.app.state.config.network.model_dump()


@router.put("/config/network")
async def update_network_config(update: NetworkConfigUpdate, request: Request):
    """Update network configuration settings. Only updates fields present in body."""
    config = request.app.state.config
    changes = update.model_dump(exclude_unset=True, exclude_none=True)

    if not changes:
        return config.network.model_dump()

    for key, value in changes.items():
        setattr(config.network, key, value)

    save_config(config)
    return config.network.model_dump()
