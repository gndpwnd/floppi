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
      "mdns_hostname": "floppi-EEFF"
    }
  ],
  "network": {
    "interface": "auto",
    "command_rate_hz": 10
  }
}
```

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
