"""
API routes for drone management and control.
"""

from typing import Optional

from fastapi import APIRouter, Depends, HTTPException, Request
from pydantic import BaseModel, Field, field_validator

from ..config import (
    MAC_RE, MDNS_RE, _validate_name, _validate_tags,
)
from .auth import require_auth

router = APIRouter(prefix="/api", tags=["drones"])


class CommandPayload(BaseModel):
    """Channel values to send to a drone. 1000-2000 microseconds."""
    ch1: int = Field(default=1500, ge=1000, le=2000)  # Roll
    ch2: int = Field(default=1500, ge=1000, le=2000)  # Pitch
    ch3: int = Field(default=1000, ge=1000, le=2000)  # Throttle
    ch4: int = Field(default=1500, ge=1000, le=2000)  # Yaw
    ch5: int = Field(default=1000, ge=1000, le=2000)  # Aux1 (arm/disarm)
    ch6: int = Field(default=1000, ge=1000, le=2000)  # Aux2 (mode)


class AddDronePayload(BaseModel):
    mac: str
    name: str
    mdns_hostname: Optional[str] = None
    group: Optional[str] = None
    tags: list[str] = []

    # F-07 / F-05: validate identity + metadata at the API boundary too, so
    # malformed registrations are rejected before they reach the manager/config.
    @field_validator("mac")
    @classmethod
    def _check_mac(cls, v: str) -> str:
        if not MAC_RE.match(v):
            raise ValueError("mac must be of the form AA:BB:CC:DD:EE:FF")
        return v.upper()

    @field_validator("name")
    @classmethod
    def _check_name(cls, v: str) -> str:
        return _validate_name(v, "name")

    @field_validator("group")
    @classmethod
    def _check_group(cls, v: Optional[str]) -> Optional[str]:
        return _validate_name(v, "group")

    @field_validator("mdns_hostname")
    @classmethod
    def _check_mdns(cls, v: Optional[str]) -> Optional[str]:
        if v is None:
            return v
        if not MDNS_RE.match(v):
            raise ValueError(
                "mdns_hostname must be a bare DNS label (no dots/scheme/port)")
        return v

    @field_validator("tags")
    @classmethod
    def _check_tags(cls, v: list[str]) -> list[str]:
        return _validate_tags(v) or []


class UpdateDronePayload(BaseModel):
    name: Optional[str] = None
    group: Optional[str] = None
    tags: Optional[list[str]] = None

    @field_validator("name")
    @classmethod
    def _check_name(cls, v: Optional[str]) -> Optional[str]:
        return _validate_name(v, "name")

    @field_validator("group")
    @classmethod
    def _check_group(cls, v: Optional[str]) -> Optional[str]:
        return _validate_name(v, "group")

    @field_validator("tags")
    @classmethod
    def _check_tags(cls, v: Optional[list[str]]) -> Optional[list[str]]:
        return _validate_tags(v)


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


@router.post("/drones/{mac}/command", dependencies=[Depends(require_auth)])
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


@router.put("/drones/{mac}", dependencies=[Depends(require_auth)])
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


@router.post("/drones", dependencies=[Depends(require_auth)])
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


@router.delete("/drones/{mac}", dependencies=[Depends(require_auth)])
async def remove_drone(mac: str, request: Request):
    """Remove a drone from the fleet."""
    manager = request.app.state.manager
    removed = await manager.remove_drone(mac)
    if not removed:
        raise HTTPException(status_code=404, detail=f"Drone {mac} not found")
    return {"ok": True, "removed": mac}
