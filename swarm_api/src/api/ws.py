"""
WebSocket endpoint for the dashboard.
Bridges telemetry from drones to browser clients and forwards commands.
"""

import asyncio
import json
import logging

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

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
        except Exception:
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
    """
    await ws.accept()
    _dashboard_clients.add(ws)
    logger.info("Dashboard client connected (%d total)", len(_dashboard_clients))

    try:
        while True:
            raw = await ws.receive_text()
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            if msg.get("type") == "command":
                mac = msg.get("mac")
                channels = msg.get("channels", {})
                if mac and channels:
                    manager = ws.app.state.manager
                    drone = manager.get_drone(mac)
                    if drone and drone.state.online:
                        await drone.send_command(channels)

    except WebSocketDisconnect:
        pass
    finally:
        _dashboard_clients.discard(ws)
        logger.info("Dashboard client disconnected (%d remaining)", len(_dashboard_clients))
