# Swarm API - Scope

> Last updated: 2026-02-10
> Status: Draft

---

## Overview

Python FastAPI application for controlling ESP32-based drones over WiFi. Provides a browser-based dashboard for manual drone control (throttle, roll, pitch, yaw) and telemetry monitoring, plus a scripting-friendly REST API for automation. Manages a fleet of known ESP32 drones via configuration file, resolves their network addresses, tracks drone identity (groups, tags, metadata), and sends flight commands. Designed as the ground-station counterpart to the floppi flight controller firmware's WiFi API.

## Objectives

- Provide a web-based dashboard for real-time drone control and telemetry display
- Manage a fleet of known ESP32 drones (MAC address registry, IP resolution)
- Send flight commands to ESP32 drones via HTTP/WebSocket
- Receive and display telemetry from multiple drones simultaneously
- Work on both Linux and Windows without platform-specific dependencies
- Expose a scripting-friendly REST API for automation (curl, httpie, Python scripts)
- Keep it simple: one Python project, minimal dependencies, easy to run

## Requirements

### Functional Requirements

- [ ] Web dashboard accessible via browser (served by FastAPI)
- [ ] Manual drone controls: throttle, roll, pitch, yaw, arm/disarm
- [ ] Live telemetry display per drone (attitude, motor outputs, signal strength, loop timing)
- [ ] Drone registry in config.json (MAC addresses, friendly names, last known IPs)
- [ ] Drone discovery: mDNS resolution (floppi-XXXX.local) with config.json IP fallback
- [ ] Network interface selection in config (bind to specific adapter)
- [ ] Multi-drone support: select active drone, view fleet status
- [ ] Connection status indicators (online/offline/latency)

### Technical Requirements

- [ ] Python 3.10+ with FastAPI + uvicorn
- [ ] JSON config file for drone registry and app settings
- [ ] Platform independent (Linux + Windows)
- [ ] No database required — config.json is the only persistent storage
- [ ] WebSocket for real-time dashboard updates
- [ ] Minimal frontend (HTML + vanilla JS or lightweight framework)

### Resource Requirements

- [ ] Python 3.10+ installed on host machine
- [ ] WiFi network shared with ESP32 drones
- [ ] ESP32 drones running floppi firmware with USE_WIFI + USE_API_SERVER enabled

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| Python + FastAPI | User preference, rapid development | No |
| No database | Keep simple, config.json only | No |
| No network scanning | Direct requests to known drones only | No |
| Platform independent | Must work Linux + Windows | No |
| No cloud services | Local network only (for now) | Yes |
| Web UI dashboard | Browser-based, accessible from any device | No |

## Assumptions

- [VERIFIED] ESP32 drones expose `/api/commands` (POST) for receiving flight commands (implemented in firmware web_server.cpp)
- [ASSUMED] ESP32 drones expose `/api/status` (GET) and `/ws` (WebSocket) for telemetry (verified in firmware code)
- [ASSUMED] mDNS resolution works on the host network (requires avahi on Linux, Bonjour on Windows)
- [ASSUMED] All drones and the host are on the same WiFi network / subnet
- [VERIFIED] Each ESP32 drone is identified by MAC address and advertises as `floppi-XXXX.local`
- [VERIFIED] Telemetry JSON includes: armed, roll, pitch, yaw, imu, motors, loop_us, net (mac, hostname, ip, rssi)

## Boundaries

### In Scope

- FastAPI backend serving REST API + WebSocket + static web dashboard
- Drone registry management (add/remove/rename/tag/group drones in config.json)
- Drone discovery via mDNS with config.json IP fallback
- Manual flight control input (virtual sticks / sliders → channel values 1000-2000us)
- Telemetry reception and display (polling GET or WebSocket from each drone)
- Command transmission to drones (POST/WebSocket channel values)
- Fleet-level operations: batch commands, fleet status, emergency disarm-all
- Drone identity tracking: groups, tags, metadata, network identity (hostname, rssi, uptime)
- Scripting-friendly API: all operations accessible via REST for curl/httpie/Python automation
- System/config API: read and update server configuration programmatically
- Network interface selection for multi-NIC hosts
- Connection health monitoring (ping, latency, online/offline status)
- Fleet overview (which drones are online, group/tag breakdowns)

### Out of Scope (Exclusions)

- **Autonomous flight logic** — this is a manual control dashboard, not a mission planner
- **Path planning, waypoints, geofencing** — flight computer territory
- **Network scanning or auto-discovery** — only contact known drones from config
- **Video streaming** — drones don't have cameras in this scope
- **ESP32 firmware changes** — Python side only; firmware API contract is documented, not modified
- **Mobile app** — web dashboard works on mobile browsers, no native app
- **Internet connectivity** — local network only; internet relay is a future project
- **Encryption/authentication** — trusted local network assumption (for now)
- **Swarm algorithms** — coordinated multi-drone behavior is future scope; this is individual drone control with fleet management
- **Drone simulation** — no simulated drones; requires real hardware

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Framework | FastAPI | Async, WebSocket support, auto-docs, user preference | 2026-02-10 |
| Config storage | JSON file | Simple, no database, human-editable | 2026-02-10 |
| Dashboard | Browser-based (HTML+JS served by FastAPI) | Platform independent, accessible from any device on network | 2026-02-10 |
| Discovery | mDNS + config.json IP fallback | mDNS for zero-config, stored IPs as backup when mDNS fails | 2026-02-10 |
| Drone communication | HTTP + WebSocket | Matches ESP32 firmware API (GET /api/status, WS /ws) | 2026-02-10 |
| Repository location | Inside floppi monorepo (./swarm_api/) | Shared history with flight_controller, single repo for whole project | 2026-02-10 |

## ESP32 API Contract

The swarm_api is built against these ESP32 endpoints. All endpoints are implemented in the firmware (web_server.cpp).

### Endpoints (all implemented)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | JSON telemetry snapshot |
| `/ws` | WebSocket | Bidirectional: telemetry stream out (~10Hz), commands in |
| `/api/commands` | POST | Send channel values to drone |
| `/` | GET | Simple text status page |

### Command Format (POST /api/commands and WebSocket /ws)

```json
{"ch1": 1500, "ch2": 1500, "ch3": 1000, "ch4": 1500, "ch5": 1000, "ch6": 1000}
```

| Channel | Function | Default | Range |
|---------|----------|---------|-------|
| ch1 | Roll | 1500 (center) | 1000-2000 us |
| ch2 | Pitch | 1500 (center) | 1000-2000 us |
| ch3 | Throttle | 1000 (min) | 1000-2000 us |
| ch4 | Yaw | 1500 (center) | 1000-2000 us |
| ch5 | Aux1 (arm) | 1000 (disarmed) | 1000-2000 us |
| ch6 | Aux2 (mode) | 1000 | 1000-2000 us |

Missing fields default to safe values (center for sticks, minimum for throttle/aux).

### Command Response

POST /api/commands returns `{"ok": true}` on success, `{"error": "invalid json"}` (400) on bad input.

### Telemetry Format (GET /api/status and WebSocket broadcast)

```json
{
  "armed": false,
  "calibrating": false,
  "roll": 0.00, "pitch": 0.00, "yaw": 0.00,
  "imu": {"ax": 0.0, "ay": 0.0, "az": -9.81, "gx": 0.0, "gy": 0.0, "gz": 0.0},
  "motors": {"m1": 0.0, "m2": 0.0, "m3": 0.0, "m4": 0.0},
  "loop_us": 4250,
  "net": {"mac": "AA:BB:CC:DD:EE:FF", "hostname": "floppi-EEFF", "ssid": "...", "ip": "192.168.1.105", "rssi": -67, "connected": true},
  "heap": 125432,
  "uptime_ms": 123456
}
```

### Cross-Core Safety

Commands go through a spinlock-protected buffer: web server (Core 1) writes, flight controller (Core 0) reads. This is thread-safe and zero-copy on the flight loop side.

### Failsafe

The ESP32 has a 500ms failsafe timeout. If no commands arrive within 500ms, channels revert to safe defaults (throttle minimum, sticks centered). The swarm_api sends commands at 10Hz when actively controlling, which keeps the connection alive.

## Integration Points

- **flight_controller firmware** (ESP32): WiFi API endpoints for telemetry and commands. swarm_api is the client, ESP32 is the server.
- **config.json**: Central configuration for drone fleet, network settings, app preferences
- **Host network**: WiFi network shared between host machine and all drones

## Open Questions

- [ ] Should the dashboard support gamepad/joystick input for drone control?
- [ ] What's the minimum viable command rate for responsive drone control via WiFi? (10Hz? 20Hz? 50Hz?)
- [x] ~~Should config.json support drone groups/tags for future swarm grouping?~~ — Yes, implemented. DroneEntry has group + tags fields.

## Critical Notes

- **Safety**: Never arm a drone from the dashboard without physical line-of-sight. Always have a physical RC receiver as backup during early testing.
- **Latency**: WiFi command latency (~10-50ms) is acceptable for slow maneuvers but NOT for aggressive FPV flying. This is a development/testing tool.
- **IP addresses change**: mDNS is the primary resolution method. Config.json IPs are fallback only and may go stale.
- **No authentication**: Trusted local network. Anyone on the WiFi can control the drones. Be aware of this during testing.

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-10 | Initial scope draft | LLM + User |
| 2026-02-10 | Added fleet/scripting API, drone identity tracking, system API | LLM + User |

---

*This document evolves as the project develops. Major scope changes should be discussed before implementation.*
