"""
API routes for drone management and control.
"""

from typing import Optional

from fastapi import APIRouter, HTTPException, Request
from pydantic import BaseModel

router = APIRouter(prefix="/api", tags=["drones"])


class CommandPayload(BaseModel):
    """Channel values to send to a drone. 1000-2000 microseconds."""
    ch1: int = 1500  # Roll
    ch2: int = 1500  # Pitch
    ch3: int = 1000  # Throttle
    ch4: int = 1500  # Yaw
    ch5: int = 1000  # Aux1 (arm/disarm)
    ch6: int = 1000  # Aux2 (mode)


class AddDronePayload(BaseModel):
    mac: str
    name: str
    mdns_hostname: Optional[str] = None
    group: Optional[str] = None
    tags: list[str] = []


class UpdateDronePayload(BaseModel):
    name: Optional[str] = None
    group: Optional[str] = None
    tags: Optional[list[str]] = None


@router.get("/drones")
async def list_drones(request: Request):
    """List all registered drones with their current status."""
    manager = request.app.state.manager
    return manager.list_drones()


@router.get("/drones/{mac}")
async def get_drone(mac: str, request: Request):
    """Get status for a specific drone."""
    manager = request.app.state.manager
    drone = manager.get_drone(mac)
    if not drone:
        raise HTTPException(status_code=404, detail=f"Drone {mac} not found")
    return drone.summary()


@router.get("/drones/{mac}/telemetry")
async def get_telemetry(mac: str, request: Request):
    """Get latest telemetry for a specific drone."""
    manager = request.app.state.manager
    drone = manager.get_drone(mac)
    if not drone:
        raise HTTPException(status_code=404, detail=f"Drone {mac} not found")
    return drone.state.last_telemetry or {"error": "no telemetry yet"}


@router.post("/drones/{mac}/command")
async def send_command(mac: str, cmd: CommandPayload, request: Request):
    """Send channel values to a specific drone."""
    manager = request.app.state.manager
    drone = manager.get_drone(mac)
    if not drone:
        raise HTTPException(status_code=404, detail=f"Drone {mac} not found")
    if not drone.state.online:
        raise HTTPException(status_code=503, detail=f"Drone {drone.name} is offline")

    ok = await drone.send_command(cmd.model_dump())
    if not ok:
        raise HTTPException(status_code=502, detail="Failed to send command to drone")
    return {"ok": True}


@router.put("/drones/{mac}")
async def update_drone(mac: str, payload: UpdateDronePayload, request: Request):
    """Update drone metadata (name, group, tags). Only sends fields present in body."""
    manager = request.app.state.manager
    drone = manager.get_drone(mac)
    if not drone:
        raise HTTPException(status_code=404, detail=f"Drone {mac} not found")

    updates = payload.model_dump(exclude_unset=True)
    if not updates:
        return drone.summary()

    await manager.update_drone_metadata(mac, updates)
    return drone.summary()


@router.post("/drones")
async def add_drone(payload: AddDronePayload, request: Request):
    """Register a new drone."""
    manager = request.app.state.manager
    if manager.get_drone(payload.mac):
        raise HTTPException(status_code=409, detail=f"Drone {payload.mac} already registered")

    client = await manager.add_drone(
        mac=payload.mac, name=payload.name, mdns_hostname=payload.mdns_hostname,
        group=payload.group, tags=payload.tags,
    )
    return client.summary()


@router.delete("/drones/{mac}")
async def remove_drone(mac: str, request: Request):
    """Remove a drone from the fleet."""
    manager = request.app.state.manager
    removed = await manager.remove_drone(mac)
    if not removed:
        raise HTTPException(status_code=404, detail=f"Drone {mac} not found")
    return {"ok": True, "removed": mac}
