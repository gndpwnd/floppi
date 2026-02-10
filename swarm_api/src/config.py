"""
Configuration management for swarm_api.
Loads/saves config.json, validates drone entries, handles defaults.
"""

import json
import logging
from pathlib import Path
from typing import Optional

from pydantic import BaseModel, Field

logger = logging.getLogger("swarm_api.config")

CONFIG_PATH = Path(__file__).parent.parent / "config.json"


class DroneEntry(BaseModel):
    mac: str
    name: str
    last_ip: Optional[str] = None
    mdns_hostname: Optional[str] = None
    last_seen: Optional[str] = None


class NetworkConfig(BaseModel):
    interface: str = "auto"
    command_rate_hz: int = Field(default=10, ge=1, le=50)
    telemetry_poll_interval_ms: int = Field(default=500, ge=100, le=5000)
    connection_timeout_ms: int = Field(default=2000, ge=500, le=10000)


class ServerConfig(BaseModel):
    host: str = "0.0.0.0"
    port: int = 8080


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
    """Save config to JSON file."""
    with open(path, "w") as f:
        json.dump(config.model_dump(), f, indent=2)
    logger.info("Config saved to %s", path)
