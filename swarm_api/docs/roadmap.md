# Swarm API - Roadmap

> Last updated: 2026-02-10

## Overview

This roadmap tracks project-level features and milestones. For immediate tasks, see `todo.md`.

**Note**: No time estimates. Focus on WHAT needs to be done, not WHEN.

---

## Goal Horizons

### First Stable Release (Midterm Goal)

**"Deployable/testable" means:** A user can `pip install`, run one command, open a browser, see connected drones' telemetry, and send basic control commands to a selected drone. Config is a single JSON file.

**Must-have for first release:**
- FastAPI server runs and serves web dashboard
- Config.json with drone registry (MAC, name, last IP)
- Drone discovery (mDNS + IP fallback)
- Telemetry display for connected drones
- Command sending UI (virtual sticks/sliders)
- Works on Linux and Windows

**Nice-to-have (defer past first release):**
- Gamepad/joystick input
- Drone groups/tags
- Telemetry logging/history
- Multi-drone simultaneous control

### Long-term Vision

Full swarm control platform: coordinate multiple drones simultaneously, define group behaviors, integrate with external flight computers for autonomous operations. Internet relay for remote control.

---

## Infrastructure / Setup

- [x] Python project structure (FastAPI, uvicorn, dependencies) ✓
- [x] Config management (load/save config.json, Pydantic validation) ✓
- [x] Logging setup (structured logging, configurable levels) ✓
- [x] .gitignore, requirements.txt ✓

---

## Core Features

### Drone Registry

- [x] Config.json schema (drones list with MAC, name, last_ip, mdns_hostname, last_seen) ✓
- [x] Add/remove drones via API endpoints (POST/DELETE /api/drones) ✓
- [x] Persist changes to config.json on add/remove ✓
- [ ] Network interface selection in config

### Drone Discovery & Connection

- [x] mDNS resolution (floppi-XXXX.local → IP) ✓
- [x] Config.json IP fallback when mDNS fails ✓
- [x] Connection health check (periodic poll + background re-resolution) ✓
- [x] Online/offline/latency tracking per drone ✓
- [x] Auto-update last_ip in config when drone responds ✓

### Telemetry Reception

- [x] Poll drone GET /api/status for telemetry snapshot ✓
- [x] WebSocket connection to drone /ws for real-time stream ✓
- [x] Parse and store latest telemetry per drone ✓
- [x] Expose telemetry via swarm_api WebSocket (/ws/dashboard) to browser ✓

### Command Sending

- [x] Send channel values via WebSocket (preferred) or POST /api/commands (fallback) ✓
- [x] Channel value construction (1000-2000us from UI inputs) ✓
- [x] Arm/disarm command (ch5 toggle with confirmation) ✓
- [x] Command rate: 10Hz while actively controlling ✓
- [x] Failsafe: stop sending → drone's 500ms timeout activates ✓

### Web Dashboard

- [x] FastAPI serves static HTML/JS dashboard at / ✓
- [x] Fleet panel (all drones, online/offline indicators) ✓
- [x] Single drone control view (sliders, telemetry cards) ✓
- [x] Real-time telemetry display (attitude, motors, signal, loop time, latency) ✓
- [x] Arm/disarm buttons with confirmation dialog ✓
- [x] Connection status indicators (green/red dots) ✓
- [ ] Responsive layout polish (works on tablet/phone browsers)

---

## Nice to Have (Lower Priority)

- [ ] Gamepad/joystick input mapping (browser Gamepad API)
- [ ] Telemetry recording/playback (log to file)
- [ ] Drone groups/tags for fleet organization
- [ ] Multi-drone simultaneous command sending
- [ ] Configuration editor in dashboard (edit config.json via UI)
- [ ] Notification system (drone disconnected, low signal, etc.)

---

## Future (Out of Current Scope)

- [ ] Swarm coordination algorithms (formation, follow-leader)
- [ ] Internet relay for remote control
- [ ] Video feed integration
- [ ] Authentication and access control
- [ ] Mission planning / waypoint editor
- [ ] Telemetry database (time-series storage)

---

## Completed

> Features moved here when done, for historical reference.

### Infrastructure (2026-02-10)
- [x] Project initialization — structure, docs, dependencies, config
- [x] Config management with Pydantic validation
- [x] FastAPI app with lifespan, logging, static file serving

### Core (2026-02-10)
- [x] DroneClient — HTTP + WebSocket communication with individual drones
- [x] DroneManager — fleet lifecycle, discovery, health monitoring, telemetry aggregation
- [x] REST API — CRUD for drones, telemetry retrieval, command sending
- [x] Dashboard WebSocket bridge — telemetry from drones → browser, commands from browser → drones
- [x] Web dashboard — fleet panel, drone control view, sliders, telemetry cards, arm/disarm

---

## Notes

- The ESP32 `/api/commands` endpoint IS implemented in firmware (web_server.cpp). Both POST and WebSocket commands work, routed through spinlock buffer to Core 0 RadioComm.
- Telemetry features can be tested with any ESP32 running floppi firmware with WiFi enabled.
- This roadmap supports future LLMs: if the project is unstable, prioritize stability mode (fix bugs, clean up). If stable, work on next unchecked feature.

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
