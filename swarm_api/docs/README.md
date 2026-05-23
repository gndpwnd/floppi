# Swarm API

Ground-station control application for floppi ESP32 drones over WiFi.

## Overview

Swarm API is a Python FastAPI application that provides a browser-based dashboard for controlling and monitoring ESP32-based drones running the floppi flight controller firmware. It communicates with drones over a shared WiFi network using HTTP and WebSocket connections.

**Key capabilities:**
- Web dashboard for manual drone control (throttle, roll, pitch, yaw)
- Real-time telemetry display (attitude, motors, signal strength)
- Drone fleet management via config.json (MAC addresses, names, IPs)
- Drone discovery via mDNS with IP fallback
- Platform independent (Linux + Windows)

## Quick Start

```bash
# Install dependencies
pip install -r requirements.txt

# Run the server
python -m uvicorn src.main:app --host 0.0.0.0 --port 8080

# Open dashboard
# http://localhost:8080
```

## Configuration

Edit `config.json` to register your drones:

```json
{
  "drones": [
    {
      "mac": "AA:BB:CC:DD:EE:FF",
      "name": "drone-1",
      "last_ip": "192.168.1.105",
      "mdns_hostname": "floppi-EEFF",
      "command_token": null
    }
  ],
  "network": {
    "interface": "auto",
    "command_rate_hz": 10,
    "command_token": null
  }
}
```

### Command authentication (`command_token`)

The firmware can opt into a shared-token gate on its command surface
(`POST /api/commands` and the `/ws` WebSocket) via its `USE_API_AUTH` build
flag. To match it, set a `command_token` that equals the drone's
`FLOPPI_CMD_TOKEN`:

- Per-drone `command_token` (inside a `drones[]` entry) takes precedence.
- A fleet-wide default may be set as `network.command_token`.
- If neither is set (`null`), commands are sent unauthenticated — this is the
  default and is backward-compatible with firmware that has auth **off**.

When a token is configured, swarm_api attaches it as the `X-Floppi-Token`
header on the HTTP POST and as a `"token"` field on WebSocket command frames.
A `401` response is logged once as a clear auth error (it does not crash or
spam). **Caveat:** traffic is plaintext HTTP/WS, so the token is a control-plane
gate, not a confidentiality control — run the fleet on an isolated SSID. Enable
`USE_API_AUTH` in firmware only after the token is configured here, or commands
will start returning 401.

### Server authentication (`server.auth_token`)

Separate from `command_token` (which is swarm_api -> drone), the server has an
**opt-in** client -> swarm_api gate via `server.auth_token` in `config.json`.
It is **default off**: when unset/empty, every endpoint behaves exactly as
before and a startup warning is logged.

When set, it is required on command-bearing and mutating endpoints
(`POST /api/drones/{mac}/command`, `PUT/POST/DELETE /api/drones`,
`POST /api/fleet/command`, `POST /api/fleet/disarm`,
`PUT /api/system/config/network`) and on the `/ws/dashboard` handshake.
Read-only/telemetry/health endpoints stay open.

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "auth_token": "change-me-to-a-strong-random-secret",
    "ws_allowed_origins": []
  }
}
```

- **REST:** send `Authorization: Bearer <token>` (or `X-Auth-Token: <token>`).
  Rejected with `401`.
- **WebSocket:** connect to `/ws/dashboard?token=<token>`; the browser `Origin`
  must be same-origin or listed in `ws_allowed_origins`. Rejected with close
  code `1008`.
- **Dashboard:** paste the token into the header field (stored in
  `localStorage`). It is auto-attached to mutating REST calls and the WS URL.
  On `401`/`1008` the dashboard shows an auth banner and prompts for a token.

**Caveat:** traffic is plaintext HTTP/WS unless you front the server with TLS —
put it behind HTTPS so the bearer token is not exposed.

## Project Structure

```
swarm_api/
├── docs/
│   ├── README.md          # This file
│   ├── scope.md           # Project boundaries
│   ├── roadmap.md         # Feature roadmap
│   ├── todo.md            # Current tasks
│   ├── features/          # Feature documentation
│   └── findings/          # Research and discoveries
├── src/
│   ├── main.py            # FastAPI application entry point
│   ├── api/               # API route modules
│   ├── static/            # Dashboard HTML/CSS/JS
│   └── templates/         # Jinja2 templates (if needed)
├── tests/                 # Test scripts
├── config.json            # Drone registry and app settings
├── requirements.txt       # Python dependencies
└── .gitignore
```

## Documentation

- [architecture.md](architecture.md) - High-level architecture with diagrams
- [scope.md](docs/scope.md) - What this project is and isn't
- [roadmap.md](docs/roadmap.md) - Feature progress
- [todo.md](docs/todo.md) - Current tasks

## Requirements

- Python 3.10+
- ESP32 drones running floppi firmware with WiFi enabled
- Shared WiFi network between host and drones

## Related

- [flight_controller/](../flight_controller/) - ESP32/Teensy flight controller firmware
- [fc_tool/](../fc_tool/) - Tauri desktop app for calibration and telemetry

## License

MIT (same as floppi flight controller)
