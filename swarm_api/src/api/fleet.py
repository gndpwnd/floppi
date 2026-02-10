"""
Fleet-level API routes for scripting and automation.

Endpoints for operating on the fleet as a whole: batch commands,
fleet status overview, group/tag filtering, emergency disarm.
"""

from typing import Optional

from fastapi import APIRouter, HTTPException, Request
from pydantic import BaseModel

router = APIRouter(prefix="/api/fleet", tags=["fleet"])


class BatchCommandPayload(BaseModel):
    """Send a command to multiple drones at once."""
    ch1: int = 1500
    ch2: int = 1500
    ch3: int = 1000
    ch4: int = 1500
    ch5: int = 1000
    ch6: int = 1000
    # Target selection (mutually exclusive, checked in order)
    macs: Optional[list[str]] = None  # Specific MAC addresses
    group: Optional[str] = None       # All drones in a group
    tag: Optional[str] = None         # All drones with a tag
    # If none of the above: send to all online drones


@router.get("/status")
async def fleet_status(request: Request):
    """Fleet overview: total/online/offline/armed counts, group breakdown."""
    manager = request.app.state.manager
    return manager.fleet_status()


@router.get("/drones")
async def fleet_drones(request: Request, group: Optional[str] = None,
                       tag: Optional[str] = None, online: Optional[bool] = None):
    """List drones with optional filtering by group, tag, or online status."""
    manager = request.app.state.manager

    drones = list(manager.drones.values())

    if group is not None:
        drones = [d for d in drones if d.group == group]
    if tag is not None:
        drones = [d for d in drones if tag in d.tags]
    if online is not None:
        drones = [d for d in drones if d.state.online == online]

    return [d.summary() for d in drones]


@router.post("/command")
async def batch_command(cmd: BatchCommandPayload, request: Request):
    """
    Send the same command to multiple drones.

    Target selection (checked in order):
    - macs: specific MAC addresses
    - group: all drones in that group
    - tag: all drones with that tag
    - (none): all online drones

    Returns {mac: true/false} for each targeted drone.
    """
    manager = request.app.state.manager
    channels = {"ch1": cmd.ch1, "ch2": cmd.ch2, "ch3": cmd.ch3,
                "ch4": cmd.ch4, "ch5": cmd.ch5, "ch6": cmd.ch6}

    if cmd.macs:
        targets = cmd.macs
    elif cmd.group:
        targets = [d.mac for d in manager.get_drones_by_group(cmd.group)]
    elif cmd.tag:
        targets = [d.mac for d in manager.get_drones_by_tag(cmd.tag)]
    else:
        targets = [d.mac for d in manager.get_online_drones()]

    if not targets:
        return {"results": {}, "sent": 0, "failed": 0}

    results = await manager.send_command_to_many(targets, channels)
    sent = sum(1 for v in results.values() if v)
    failed = sum(1 for v in results.values() if not v)
    return {"results": results, "sent": sent, "failed": failed}


@router.post("/disarm")
async def disarm_all(request: Request):
    """Emergency disarm all online drones. Sends ch3=1000 (throttle min) + ch5=1000 (disarm)."""
    manager = request.app.state.manager
    results = await manager.disarm_all()
    return {
        "results": results,
        "disarmed": sum(1 for v in results.values() if v),
        "failed": sum(1 for v in results.values() if not v),
    }


@router.get("/groups")
async def list_groups(request: Request):
    """List all drone groups with member counts."""
    manager = request.app.state.manager
    groups = {}
    for d in manager.drones.values():
        g = d.group or "ungrouped"
        if g not in groups:
            groups[g] = {"drones": [], "online": 0, "total": 0}
        groups[g]["drones"].append(d.mac)
        groups[g]["total"] += 1
        if d.state.online:
            groups[g]["online"] += 1
    return groups


@router.get("/tags")
async def list_tags(request: Request):
    """List all tags in use across the fleet."""
    manager = request.app.state.manager
    tags = {}
    for d in manager.drones.values():
        for t in d.tags:
            if t not in tags:
                tags[t] = []
            tags[t].append(d.mac)
    return tags
