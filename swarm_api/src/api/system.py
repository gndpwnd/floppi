"""
System and configuration API routes.

Endpoints for reading/updating server configuration, server info,
and programmatic access to config.json settings.
"""

import time

from fastapi import APIRouter, Depends, HTTPException, Request, status
from pydantic import BaseModel, Field

from ..config import save_config
from .auth import require_auth

router = APIRouter(prefix="/api/system", tags=["system"])

# Secret fields redacted from any config read (F-09).
_REDACT = "command_token"


def _redact_config(data: dict) -> dict:
    """Return a copy of a config dump with secret tokens removed."""
    net = data.get("network")
    if isinstance(net, dict):
        net.pop(_REDACT, None)
    for d in data.get("drones", []):
        if isinstance(d, dict):
            d.pop(_REDACT, None)
    server = data.get("server")
    if isinstance(server, dict):
        # auth_token is the client->server secret; never expose it.
        server.pop("auth_token", None)
    return data

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
    """Read current configuration (all sections), with secrets redacted (F-09)."""
    config = request.app.state.config
    return _redact_config(config.model_dump())


@router.get("/config/network")
async def get_network_config(request: Request):
    """Read network configuration (command_token redacted — F-09)."""
    net = request.app.state.config.network.model_dump()
    net.pop(_REDACT, None)
    return net


@router.put("/config/network", dependencies=[Depends(require_auth)])
async def update_network_config(update: NetworkConfigUpdate, request: Request):
    """
    Update network configuration settings. Only updates fields present in body.

    F-04: this mutates and persists config.json, so it now requires the server
    auth token (when configured) and can be globally disabled via
    server.config_mutation_enabled. Never an unauthenticated config rewrite when
    auth is enabled; never any rewrite when mutation is disabled.
    """
    config = request.app.state.config

    if not config.server.config_mutation_enabled:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Runtime config mutation is disabled "
                   "(server.config_mutation_enabled = false)",
        )

    changes = update.model_dump(exclude_unset=True, exclude_none=True)

    if not changes:
        net = config.network.model_dump()
        net.pop(_REDACT, None)
        return net

    for key, value in changes.items():
        setattr(config.network, key, value)

    save_config(config)
    net = config.network.model_dump()
    net.pop(_REDACT, None)
    return net
