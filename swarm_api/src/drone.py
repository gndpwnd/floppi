"""
Drone client — communicates with a single ESP32 drone over WiFi.

Handles:
- Telemetry polling (GET /api/status)
- WebSocket connection (bidirectional: telemetry in, commands out)
- Command sending (POST /api/commands or via WebSocket)
- Connection health tracking
"""

import asyncio
import ipaddress
import json
import logging
import time
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

import httpx
import websockets

logger = logging.getLogger("swarm_api.drone")


def _valid_ip(value: Any) -> Optional[str]:
    """Return the value if it is a valid IPv4/IPv6 address, else None.

    Defense-in-depth (audit F-10): telemetry is attacker-influenced — a spoofed
    or compromised drone could otherwise inject arbitrary markup via net.ip
    (reflected to the dashboard) or redirect future polls/commands. We only
    adopt net.ip when it parses as a real IP; anything else is dropped.
    """
    if not isinstance(value, str):
        return None
    try:
        ipaddress.ip_address(value)
        return value
    except ValueError:
        return None


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
                 mdns_hostname: Optional[str] = None, timeout_s: float = 2.0,
                 group: Optional[str] = None, tags: Optional[list[str]] = None,
                 command_token: Optional[str] = None):
        self.mac = mac
        self.name = name
        self.ip = ip
        self.mdns_hostname = mdns_hostname
        self.timeout_s = timeout_s
        self.group = group
        self.tags = tags or []
        # Shared token for the firmware command surface (USE_API_AUTH). When
        # set, it is attached to every command via the X-Floppi-Token header
        # (HTTP POST) and a "token" field (WebSocket). When None, commands go
        # out unauthenticated — backward-compatible with auth-OFF firmware.
        self.command_token = command_token
        self.state = DroneState(ip=ip)

        self._ws: Optional[websockets.WebSocketClientProtocol] = None
        self._ws_task: Optional[asyncio.Task] = None
        self._telemetry_callbacks: list[Callable] = []
        self._http = httpx.AsyncClient(timeout=timeout_s)
        # Throttle repeated 401 logs so a misconfigured token doesn't spam.
        self._auth_warned = False

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

                # Update IP from telemetry if available. Validate server-side
                # before adopting it (audit F-10) — never trust a drone-reported
                # address verbatim. Invalid values are simply not adopted.
                net = data.get("net", {})
                valid_ip = _valid_ip(net.get("ip"))
                if valid_ip:
                    self.ip = valid_ip
                    self.state.ip = valid_ip

                for cb in self._telemetry_callbacks:
                    cb(self.mac, data)

                return data
        except (httpx.RequestError, json.JSONDecodeError, ValueError) as e:
            # Network failure or a drone returning non-JSON / bad JSON.
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

        # Try WebSocket first (lower latency, no HTTP overhead).
        # WS frames have no HTTP headers, so the token (if any) rides in the
        # JSON body as a "token" field, per the firmware auth contract.
        if self._ws and self.state.ws_connected:
            ws_payload = payload
            if self.command_token:
                ws_payload = {"token": self.command_token, **payload}
            try:
                await self._ws.send(json.dumps(ws_payload))
                return True
            except (websockets.WebSocketException, OSError) as e:
                logger.debug("WS send failed for %s: %s", self.name, e)
                self.state.ws_connected = False

        # Fall back to POST. Token (if any) goes in the X-Floppi-Token header
        # (preferred per the firmware contract).
        url = self.base_url
        if not url:
            return False

        headers = {}
        if self.command_token:
            headers["X-Floppi-Token"] = self.command_token

        try:
            resp = await self._http.post(f"{url}/api/commands", json=payload,
                                         headers=headers)
            if resp.status_code == 401:
                if not self._auth_warned:
                    logger.error(
                        "Command rejected by %s: auth required/invalid token "
                        "(HTTP 401). Check command_token in config.json matches "
                        "the drone's FLOPPI_CMD_TOKEN.", self.name)
                    self._auth_warned = True
                return False
            # Recovered (or never failed) — allow a future 401 to log again.
            self._auth_warned = False
            return resp.status_code == 200
        except httpx.RequestError as e:
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

        except (websockets.WebSocketException, OSError) as e:
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
        net = self.state.last_telemetry.get("net", {})
        return {
            "mac": self.mac,
            "name": self.name,
            "group": self.group,
            "tags": self.tags,
            "ip": self.state.ip,
            "mdns_hostname": self.mdns_hostname,
            "online": self.state.online,
            "ws_connected": self.state.ws_connected,
            "last_seen": self.state.last_seen,
            "latency_ms": round(self.state.latency_ms, 1),
            "armed": self.state.last_telemetry.get("armed", False),
            "rssi": net.get("rssi"),
            "hostname": net.get("hostname"),
            "uptime_ms": self.state.last_telemetry.get("uptime_ms"),
            "telemetry": self.state.last_telemetry,
        }
