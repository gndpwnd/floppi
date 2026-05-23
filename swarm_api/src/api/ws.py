"""
WebSocket endpoint for the dashboard.
Bridges telemetry from drones to browser clients and forwards commands.
"""

import json
import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from pydantic import ValidationError

from .auth import ws_origin_allowed, ws_token_valid

router = APIRouter()
logger = logging.getLogger("swarm_api.ws")

# Connected dashboard clients
_dashboard_clients: set[WebSocket] = set()


async def broadcast_telemetry(mac: str, data: dict) -> None:
    """Broadcast drone telemetry to all connected dashboard clients."""
    if not _dashboard_clients:
        return

    message = json.dumps({"type": "telemetry", "mac": mac, "data": data})
    dead = set()
    for client in _dashboard_clients:
        try:
            await client.send_text(message)
        except (WebSocketDisconnect, RuntimeError, OSError):
            # Client gone / socket closed mid-send — drop it from the set.
            dead.add(client)
    _dashboard_clients -= dead


@router.websocket("/ws/dashboard")
async def dashboard_ws(ws: WebSocket):
    """
    Dashboard WebSocket.

    Server → Client messages:
        {"type": "telemetry", "mac": "AA:BB:CC:DD:EE:FF", "data": {...}}
        {"type": "status", "drones": [...]}

    Client → Server messages:
        {"type": "command", "mac": "AA:BB:CC:DD:EE:FF", "channels": {"ch1":1500,...}}

    Auth (F-01/F-02): when server.auth_token is configured, the connection
    requires the token as the `token` query parameter (?token=...) and the
    Origin header must be same-origin or in server.ws_allowed_origins. When no
    token is configured, the WS accepts unauthenticated as before.
    """
    config = ws.app.state.config
    auth_token = config.server.auth_token

    # Origin + token are only enforced when auth is enabled (backward compat).
    if auth_token:
        origin = ws.headers.get("origin")
        host = ws.headers.get("host")
        if not ws_origin_allowed(origin, host, config.server.ws_allowed_origins):
            logger.warning("Rejecting WS: disallowed Origin %r", origin)
            await ws.close(code=1008)  # policy violation
            return
        presented = ws.query_params.get("token")
        if not ws_token_valid(auth_token, presented):
            logger.warning("Rejecting WS: missing/invalid token")
            await ws.close(code=1008)
            return

    await ws.accept()
    _dashboard_clients.add(ws)
    logger.info("Dashboard client connected (%d total)", len(_dashboard_clients))

    # Imported lazily to avoid a circular import (drones -> auth -> ... ).
    from .drones import CommandPayload

    try:
        while True:
            raw = await ws.receive_text()
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if not isinstance(msg, dict):
                continue

            if msg.get("type") == "command":
                mac = msg.get("mac")
                # F-06: validate the channel frame through the same Pydantic
                # model the REST path uses; reject malformed frames explicitly.
                try:
                    cmd = CommandPayload(**(msg.get("channels") or {}))
                except (ValidationError, TypeError):
                    logger.debug("Dropping malformed WS command frame")
                    continue
                if mac:
                    manager = ws.app.state.manager
                    drone = manager.get_drone(mac)
                    if drone and drone.state.online:
                        await drone.send_command(cmd.model_dump())

    except WebSocketDisconnect:
        pass
    finally:
        _dashboard_clients.discard(ws)
        logger.info("Dashboard client disconnected (%d remaining)", len(_dashboard_clients))
