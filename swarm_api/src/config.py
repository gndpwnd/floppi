"""
Configuration management for swarm_api.
Loads/saves config.json, validates drone entries, handles defaults.
"""

import json
import logging
import os
import re
import stat
from pathlib import Path
from typing import Optional

from pydantic import BaseModel, Field, field_validator

logger = logging.getLogger("swarm_api.config")

CONFIG_PATH = Path(__file__).parent.parent / "config.json"

# Shared validation rules for drone identity/metadata (F-07). These also close
# the stored-XSS vector: free-form name/group/tag strings were persisted to
# config.json and reflected back into GET /api/drones and the dashboard DOM.
MAC_RE = re.compile(r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")
# mDNS hostnames are expected to be the firmware's "floppi-XXXX" label: a bare
# DNS label (letters/digits/hyphen), no dots, scheme, or port. Restricting this
# also limits the SSRF surface (F-05) since it is concatenated into a URL.
MDNS_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9\-]{0,62}$")
TAG_RE = re.compile(r"^[A-Za-z0-9 _\-]{1,32}$")
NAME_MAX = 64
GROUP_MAX = 64
MAX_TAGS = 16


def _validate_name(v: Optional[str], field: str) -> Optional[str]:
    if v is None:
        return v
    if not isinstance(v, str):
        raise ValueError(f"{field} must be a string")
    v = v.strip()
    if not v:
        raise ValueError(f"{field} must not be empty")
    if len(v) > NAME_MAX:
        raise ValueError(f"{field} must be <= {NAME_MAX} characters")
    # Disallow control chars / angle brackets to block stored-XSS / log injection.
    if any(ord(c) < 32 for c in v) or "<" in v or ">" in v:
        raise ValueError(f"{field} contains disallowed characters")
    return v


def _validate_tags(v: Optional[list[str]]) -> Optional[list[str]]:
    if v is None:
        return v
    if not isinstance(v, list):
        raise ValueError("tags must be a list")
    if len(v) > MAX_TAGS:
        raise ValueError(f"at most {MAX_TAGS} tags allowed")
    out = []
    for t in v:
        if not isinstance(t, str):
            raise ValueError("each tag must be a string")
        t = t.strip()
        if not TAG_RE.match(t):
            raise ValueError(
                "tags may only contain letters, digits, space, _ or - "
                "(1-32 chars)")
        out.append(t)
    return out


class DroneEntry(BaseModel):
    mac: str
    name: str
    last_ip: Optional[str] = None
    mdns_hostname: Optional[str] = None
    last_seen: Optional[str] = None
    group: Optional[str] = None
    tags: list[str] = []

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
                "mdns_hostname must be a bare DNS label (letters/digits/-, "
                "no dots, scheme, or port)")
        return v

    @field_validator("tags")
    @classmethod
    def _check_tags(cls, v: list[str]) -> list[str]:
        return _validate_tags(v) or []
    # Optional shared-token for the firmware command surface (USE_API_AUTH).
    # Must match the drone's FLOPPI_CMD_TOKEN. If None, the global
    # network.command_token (if any) is used; if neither is set, commands are
    # sent unauthenticated (backward-compatible with firmware that has auth OFF).
    command_token: Optional[str] = None


class NetworkConfig(BaseModel):
    interface: str = "auto"
    command_rate_hz: int = Field(default=10, ge=1, le=50)
    telemetry_poll_interval_ms: int = Field(default=500, ge=100, le=5000)
    connection_timeout_ms: int = Field(default=2000, ge=500, le=10000)
    # Optional fleet-wide default command token. A per-drone command_token
    # overrides this. None = no token attached (open command surface).
    command_token: Optional[str] = None


class ServerConfig(BaseModel):
    # Bind host for the uvicorn server (F-11). Default "0.0.0.0" = listen on
    # every interface (backward-compatible; reachable from the whole LAN). Set
    # to "127.0.0.1" to restrict the unauthenticated-by-default control plane to
    # localhost, or to a specific interface IP for one trusted network. Honored
    # by `python -m src.main` and by deploy/service.sh (deploy/common.sh:get_host).
    host: str = "0.0.0.0"
    port: int = 8080
    # Opt-in server auth (F-01/F-02). DEFAULT OFF for backward compatibility:
    # when None/empty, every endpoint behaves exactly as before (a startup
    # warning is logged). When set, command-bearing and mutating endpoints —
    # plus the /ws/dashboard connection — require this token. This is the
    # CLIENT -> swarm_api credential and is distinct from network.command_token
    # (the swarm_api -> drone-firmware credential).
    auth_token: Optional[str] = None
    # Allow runtime config mutation (PUT /api/system/config/network). When
    # False, the endpoint is refused (503) even with a valid auth token (F-04).
    config_mutation_enabled: bool = True
    # Origin allowlist for the dashboard WebSocket handshake (F-02). Only
    # enforced when auth_token is set. Empty list = same-origin only.
    ws_allowed_origins: list[str] = []
    # Rate limiting (F-08) — ASSESSED, intentionally NOT implemented.
    # This is a bare-bones LAN ground station. The command/disarm flood vector
    # is already addressed by the opt-in auth layer (auth_token): once a token
    # is set, only the holder can reach command/batch/disarm. A per-IP in-memory
    # limiter is a poor fit here: the dashboard streams commands at ~10 Hz by
    # design, so any threshold useful against a flood risks throttling the real
    # operator, and rate-limiting the emergency-disarm endpoint is actively
    # undesirable (it must always go through). Adding a heavy dep (slowapi) is
    # rejected per the bare-bones scope. If abuse controls are ever needed,
    # enforce them at the reverse proxy / firewall, not in this app.


class AppConfig(BaseModel):
    drones: list[DroneEntry] = []
    network: NetworkConfig = NetworkConfig()
    server: ServerConfig = ServerConfig()


def load_config(path: Path = CONFIG_PATH) -> AppConfig:
    """Load and validate config from JSON file."""
    if not path.exists():
        logger.warning("Config file not found at %s, using defaults", path)
        return AppConfig()

    with open(path) as f:
        raw = json.load(f)

    config = AppConfig(**raw)
    logger.info("Loaded config: %d drone(s) registered", len(config.drones))
    return config


def save_config(config: AppConfig, path: Path = CONFIG_PATH) -> None:
    """Save config to JSON file.

    Writes with restrictive 0600 permissions (F-09): config.json may hold
    command_token / auth_token secrets, so it should be owner-read/write only.
    """
    with open(path, "w") as f:
        json.dump(config.model_dump(), f, indent=2)
    try:
        os.chmod(path, stat.S_IRUSR | stat.S_IWUSR)  # 0600
    except OSError as e:  # pragma: no cover - platform/permission dependent
        logger.warning("Could not chmod 0600 on %s: %s", path, e)
    logger.info("Config saved to %s", path)
