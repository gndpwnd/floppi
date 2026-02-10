"""
Drone client — communicates with a single ESP32 drone over WiFi.

Handles:
- Telemetry polling (GET /api/status)
- WebSocket connection (bidirectional: telemetry in, commands out)
- Command sending (POST /api/commands or via WebSocket)
- Connection health tracking
"""

import asyncio
import json
import logging
import time
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

import httpx
import websockets

logger = logging.getLogger("swarm_api.drone")


@dataclass
class DroneState:
    """Latest known state of a drone."""
    online: bool = False
    last_seen: float = 0.0
    last_telemetry: dict = field(default_factory=dict)
    latency_ms: float = 0.0
    ip: Optional[str] = None
    ws_connected: bool = False


class DroneClient:
    """Client for communicating with a single ESP32 drone."""

    def __init__(self, mac: str, name: str, ip: Optional[str] = None,
                 mdns_hostname: Optional[str] = None, timeout_s: float = 2.0):
        self.mac = mac
        self.name = name
        self.ip = ip
        self.mdns_hostname = mdns_hostname
        self.timeout_s = timeout_s
        self.state = DroneState(ip=ip)

        self._ws: Optional[websockets.WebSocketClientProtocol] = None
        self._ws_task: Optional[asyncio.Task] = None
        self._telemetry_callbacks: list[Callable] = []
        self._http = httpx.AsyncClient(timeout=timeout_s)

    @property
    def base_url(self) -> Optional[str]:
        """HTTP base URL for this drone, or None if no address known."""
        if self.ip:
            return f"http://{self.ip}"
        if self.mdns_hostname:
            return f"http://{self.mdns_hostname}.local"
        return None

    @property
    def ws_url(self) -> Optional[str]:
        """WebSocket URL for this drone."""
        if self.ip:
            return f"ws://{self.ip}/ws"
        if self.mdns_hostname:
            return f"ws://{self.mdns_hostname}.local/ws"
        return None

    def on_telemetry(self, callback: Callable[[str, dict], None]) -> None:
        """Register a callback for telemetry updates. callback(mac, data)."""
        self._telemetry_callbacks.append(callback)

    async def poll_status(self) -> Optional[dict]:
        """Poll GET /api/status. Returns telemetry dict or None on failure."""
        url = self.base_url
        if not url:
            return None

        try:
            t0 = time.monotonic()
            resp = await self._http.get(f"{url}/api/status")
            latency = (time.monotonic() - t0) * 1000

            if resp.status_code == 200:
                data = resp.json()
                self.state.online = True
                self.state.last_seen = time.time()
                self.state.last_telemetry = data
                self.state.latency_ms = latency

                # Update IP from telemetry if available
                net = data.get("net", {})
                if net.get("ip"):
                    self.ip = net["ip"]
                    self.state.ip = net["ip"]

                for cb in self._telemetry_callbacks:
                    cb(self.mac, data)

                return data
        except (httpx.RequestError, Exception) as e:
            logger.debug("Poll failed for %s: %s", self.name, e)
            self.state.online = False
        return None

    async def send_command(self, channels: dict[str, int]) -> bool:
        """
        Send channel values to the drone.
        channels: {"ch1": 1500, "ch2": 1500, "ch3": 1000, "ch4": 1500, "ch5": 1000, "ch6": 1000}
        Values 1000-2000 microseconds.

        Prefers WebSocket if connected, falls back to POST /api/commands.
        """
        # Clamp values to valid range
        payload = {}
        for key in ("ch1", "ch2", "ch3", "ch4", "ch5", "ch6"):
            val = channels.get(key, 1500 if key != "ch3" else 1000)
            payload[key] = max(1000, min(2000, val))

        # Try WebSocket first (lower latency, no HTTP overhead)
        if self._ws and self.state.ws_connected:
            try:
                await self._ws.send(json.dumps(payload))
                return True
            except Exception:
                self.state.ws_connected = False

        # Fall back to POST
        url = self.base_url
        if not url:
            return False

        try:
            resp = await self._http.post(f"{url}/api/commands", json=payload)
            return resp.status_code == 200
        except (httpx.RequestError, Exception) as e:
            logger.debug("Command send failed for %s: %s", self.name, e)
            return False

    async def connect_ws(self) -> None:
        """Start persistent WebSocket connection for real-time telemetry + commands."""
        url = self.ws_url
        if not url:
            return

        try:
            self._ws = await websockets.connect(url, ping_interval=20, ping_timeout=10)
            self.state.ws_connected = True
            logger.info("WebSocket connected to %s (%s)", self.name, url)

            async for message in self._ws:
                try:
                    data = json.loads(message)
                    self.state.online = True
                    self.state.last_seen = time.time()
                    self.state.last_telemetry = data
                    for cb in self._telemetry_callbacks:
                        cb(self.mac, data)
                except json.JSONDecodeError:
                    pass

        except Exception as e:
            logger.debug("WebSocket error for %s: %s", self.name, e)
        finally:
            self.state.ws_connected = False
            self._ws = None

    async def start_ws_loop(self) -> None:
        """Run WebSocket connection with auto-reconnect."""
        while True:
            await self.connect_ws()
            await asyncio.sleep(5)  # Reconnect delay

    async def close(self) -> None:
        """Clean up connections."""
        if self._ws:
            await self._ws.close()
            self._ws = None
        await self._http.aclose()

    def summary(self) -> dict[str, Any]:
        """Return drone status summary for the dashboard."""
        return {
            "mac": self.mac,
            "name": self.name,
            "ip": self.state.ip,
            "online": self.state.online,
            "ws_connected": self.state.ws_connected,
            "last_seen": self.state.last_seen,
            "latency_ms": round(self.state.latency_ms, 1),
            "armed": self.state.last_telemetry.get("armed", False),
            "telemetry": self.state.last_telemetry,
        }
