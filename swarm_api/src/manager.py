"""
Drone fleet manager — manages multiple DroneClients.

Handles:
- Fleet lifecycle (create/remove clients from config)
- mDNS discovery with IP fallback
- Periodic health checks
- Telemetry aggregation for dashboard
"""

import asyncio
import logging
import socket
import time
from typing import Optional

from .config import AppConfig, DroneEntry, save_config
from .drone import DroneClient

logger = logging.getLogger("swarm_api.manager")


class DroneManager:
    """Manages a fleet of drone clients."""

    def __init__(self, config: AppConfig):
        self.config = config
        self.drones: dict[str, DroneClient] = {}  # mac -> DroneClient
        self._tasks: list[asyncio.Task] = []
        self._telemetry_listeners: list = []

        # Create clients from config
        for entry in config.drones:
            self._add_drone(entry)

    def _add_drone(self, entry: DroneEntry) -> DroneClient:
        """Create a DroneClient from a config entry."""
        # Per-drone token overrides the fleet-wide default; either may be None.
        token = entry.command_token or self.config.network.command_token
        client = DroneClient(
            mac=entry.mac,
            name=entry.name,
            ip=entry.last_ip,
            mdns_hostname=entry.mdns_hostname,
            timeout_s=self.config.network.connection_timeout_ms / 1000.0,
            group=entry.group,
            tags=list(entry.tags),
            command_token=token,
        )
        # Wire up telemetry forwarding to all listeners
        client.on_telemetry(self._on_drone_telemetry)
        self.drones[entry.mac] = client
        logger.info("Registered drone: %s (%s)", entry.name, entry.mac)
        return client

    def _on_drone_telemetry(self, mac: str, data: dict) -> None:
        """Forward telemetry from individual drones to manager-level listeners."""
        for listener in self._telemetry_listeners:
            listener(mac, data)

    def on_telemetry(self, callback) -> None:
        """Register a fleet-level telemetry callback."""
        self._telemetry_listeners.append(callback)

    def get_drone(self, mac: str) -> Optional[DroneClient]:
        """Get a drone client by MAC address."""
        return self.drones.get(mac)

    def list_drones(self) -> list[dict]:
        """Get status summary for all drones."""
        return [d.summary() for d in self.drones.values()]

    def fleet_status(self) -> dict:
        """Get fleet-level status summary."""
        total = len(self.drones)
        online = sum(1 for d in self.drones.values() if d.state.online)
        armed = sum(1 for d in self.drones.values()
                    if d.state.last_telemetry.get("armed", False))
        groups = {}
        for d in self.drones.values():
            g = d.group or "ungrouped"
            if g not in groups:
                groups[g] = {"total": 0, "online": 0}
            groups[g]["total"] += 1
            if d.state.online:
                groups[g]["online"] += 1

        return {
            "total": total,
            "online": online,
            "offline": total - online,
            "armed": armed,
            "groups": groups,
        }

    def get_drones_by_group(self, group: str) -> list[DroneClient]:
        """Get all drones in a given group."""
        return [d for d in self.drones.values() if d.group == group]

    def get_drones_by_tag(self, tag: str) -> list[DroneClient]:
        """Get all drones with a given tag."""
        return [d for d in self.drones.values() if tag in d.tags]

    def get_online_drones(self) -> list[DroneClient]:
        """Get all currently online drones."""
        return [d for d in self.drones.values() if d.state.online]

    async def send_command_to_many(self, macs: list[str], channels: dict) -> dict:
        """Send the same command to multiple drones. Returns {mac: success_bool}."""
        results = {}
        tasks = []
        for mac in macs:
            drone = self.drones.get(mac)
            if not drone:
                results[mac] = False
                continue
            if not drone.state.online:
                results[mac] = False
                continue
            tasks.append((mac, drone.send_command(channels)))

        if tasks:
            coros = [t[1] for t in tasks]
            outcomes = await asyncio.gather(*coros, return_exceptions=True)
            for (mac, _), outcome in zip(tasks, outcomes):
                results[mac] = outcome is True

        return results

    async def disarm_all(self) -> dict:
        """Emergency disarm: send ch5=1000 to ALL online drones."""
        safe = {"ch1": 1500, "ch2": 1500, "ch3": 1000, "ch4": 1500, "ch5": 1000, "ch6": 1000}
        online_macs = [d.mac for d in self.drones.values() if d.state.online]
        return await self.send_command_to_many(online_macs, safe)

    async def update_drone_metadata(self, mac: str, updates: dict) -> bool:
        """Update drone identity metadata from a dict of field:value.
        Only updates fields present in the dict. Returns False if drone not found."""
        client = self.drones.get(mac)
        if not client:
            return False

        if "name" in updates:
            client.name = updates["name"]
        if "group" in updates:
            client.group = updates["group"]
        if "tags" in updates:
            client.tags = updates["tags"]

        # Sync back to config entry
        for entry in self.config.drones:
            if entry.mac == mac:
                if "name" in updates:
                    entry.name = updates["name"]
                if "group" in updates:
                    entry.group = updates["group"]
                if "tags" in updates:
                    entry.tags = updates["tags"]
                break

        save_config(self.config)
        return True

    async def resolve_drone(self, drone: DroneClient) -> Optional[str]:
        """
        Resolve a drone's IP address.
        Strategy: try mDNS first, fall back to last known IP.
        """
        # Try mDNS resolution
        if drone.mdns_hostname:
            hostname = f"{drone.mdns_hostname}.local"
            try:
                loop = asyncio.get_event_loop()
                result = await loop.getaddrinfo(hostname, 80, family=socket.AF_INET)
                if result:
                    ip = result[0][4][0]
                    drone.ip = ip
                    drone.state.ip = ip
                    logger.info("mDNS resolved %s → %s", hostname, ip)
                    return ip
            except (socket.gaierror, OSError) as e:
                logger.debug("mDNS resolution failed for %s: %s", hostname, e)

        # Fall back to last known IP
        if drone.ip:
            logger.debug("Using last known IP for %s: %s", drone.name, drone.ip)
            return drone.ip

        logger.warning("No address available for %s", drone.name)
        return None

    async def discover_all(self) -> None:
        """Resolve addresses for all registered drones."""
        tasks = [self.resolve_drone(d) for d in self.drones.values()]
        await asyncio.gather(*tasks)

    async def poll_all(self) -> None:
        """Poll telemetry from all drones that have a known address."""
        tasks = []
        for drone in self.drones.values():
            if drone.base_url:
                tasks.append(drone.poll_status())
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)

    async def _poll_loop(self) -> None:
        """Background task: periodically poll all drones for telemetry."""
        interval = self.config.network.telemetry_poll_interval_ms / 1000.0
        while True:
            await self.poll_all()
            await asyncio.sleep(interval)

    async def _health_loop(self) -> None:
        """Background task: periodic health checks and IP re-resolution."""
        while True:
            await asyncio.sleep(30)  # Check every 30s
            for drone in self.drones.values():
                if not drone.state.online:
                    await self.resolve_drone(drone)

            # Update config with latest IPs
            self._sync_config_ips()

    def _sync_config_ips(self) -> None:
        """Update config entries with latest known IPs from drone clients."""
        changed = False
        for entry in self.config.drones:
            client = self.drones.get(entry.mac)
            if client and client.state.ip and client.state.ip != entry.last_ip:
                entry.last_ip = client.state.ip
                changed = True
            if client and client.state.last_seen:
                seen = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime(client.state.last_seen))
                if seen != entry.last_seen:
                    entry.last_seen = seen
                    changed = True
        if changed:
            save_config(self.config)

    async def start(self) -> None:
        """Start background tasks (polling, health checks, WebSocket connections)."""
        # Resolve addresses first
        await self.discover_all()

        # Start WebSocket connections for all drones
        for drone in self.drones.values():
            if drone.ws_url:
                task = asyncio.create_task(drone.start_ws_loop())
                self._tasks.append(task)

        # Start polling loop (fallback for drones without WebSocket)
        self._tasks.append(asyncio.create_task(self._poll_loop()))

        # Start health check loop
        self._tasks.append(asyncio.create_task(self._health_loop()))

        logger.info("Drone manager started with %d drone(s)", len(self.drones))

    async def stop(self) -> None:
        """Stop all background tasks and close connections."""
        for task in self._tasks:
            task.cancel()
        self._tasks.clear()

        for drone in self.drones.values():
            await drone.close()

        self._sync_config_ips()
        logger.info("Drone manager stopped")

    async def add_drone(self, mac: str, name: str, mdns_hostname: Optional[str] = None,
                        group: Optional[str] = None, tags: Optional[list[str]] = None) -> DroneClient:
        """Add a new drone to the fleet and config."""
        entry = DroneEntry(mac=mac, name=name, mdns_hostname=mdns_hostname,
                           group=group, tags=tags or [])
        self.config.drones.append(entry)
        client = self._add_drone(entry)
        save_config(self.config)

        # Resolve and start WebSocket
        await self.resolve_drone(client)
        if client.ws_url:
            task = asyncio.create_task(client.start_ws_loop())
            self._tasks.append(task)

        return client

    async def remove_drone(self, mac: str) -> bool:
        """Remove a drone from the fleet and config."""
        client = self.drones.pop(mac, None)
        if not client:
            return False

        await client.close()
        self.config.drones = [d for d in self.config.drones if d.mac != mac]
        save_config(self.config)
        logger.info("Removed drone: %s (%s)", client.name, mac)
        return True
