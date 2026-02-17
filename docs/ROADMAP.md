# floppi - Roadmap

> Last updated: 2026-02-17

## Overview

floppi is an open-source bare-bones flight controller platform: embedded firmware, a desktop companion tool, and a WiFi-based swarm control API. The firmware does one thing well — stabilize a drone (read sensors, filter, PID, output motors). Complex logic belongs on external systems. See [scope.md](scope.md) for project boundaries.

**Design philosophy**: Bare-bones flight stabilizer, not a full autopilot. Not trying to be Betaflight/ArduPilot. Compile-time feature gating, zero runtime overhead for unused features.

---

## Sub-Projects

| Sub-Project | Description | Status | Roadmap |
|-------------|-------------|--------|---------|
| `flight_controller/` | PlatformIO firmware (Teensy + ESP32) — PID, IMU, calibration, WiFi | Hardware validation phase | [roadmap](../flight_controller/docs/roadmap.md) |
| `fc_tool/` | Tauri 2 desktop app (Rust + JS) — serial monitor, plotter | Functional, minor additions planned | [roadmap](../fc_tool/docs/roadmap.md) |
| `swarm_api/` | Python FastAPI server — fleet control, telemetry dashboard | Core features complete | [roadmap](../swarm_api/docs/roadmap.md) |

Each sub-project has its own `docs/` directory with roadmap, scope, todo, and findings.

---

## Progression Path

The firmware supports a staged hardware progression. Each stage builds on the previous:

1. **Teensy + RC receiver** — Manual flight with FlySky FS-iA6B (SBUS/iBUS). Bare-bones stabilizer. Calibrate, tune PIDs, fly.
2. **ESP32 + RC receiver** — Same flight control, adds WiFi telemetry, OLED display, OTA updates. Drone joins existing WiFi network (STA mode).
3. **ESP32 + WiFi API** — No RC receiver. Commands arrive over HTTP/WebSocket from swarm_api or any REST client. Enables multi-drone coordination.

All three configurations use the same codebase. Platform and command source are compile-time selected via `config.h` flags.

---

## Project Status

**Firmware** (`flight_controller/`): ~90% of target features implemented. Feature development paused. All calibration routines, command sources, and platform support complete. Calibration tooling complete: `calibrate.sh` (menu-driven wrapper), `serial_monitor.py` (raw termios backend), `test_calibration.sh` (19 automated tests). Current focus: hardware bench testing, calibration on real hardware, PID tuning, and first flight.

**fc_tool** (`fc_tool/`): Functional. Serial monitor with ANSI rendering, dynamic multi-graph plotter, board detection, auto-reconnect, CLI arguments (`--port`/`--baud`), and headless mode (`--headless`) all working. Plotter enhancements (cursors, signal analysis) are lower priority.

**swarm_api** (`swarm_api/`): Core features complete. Fleet API, drone registry, telemetry bridging, web dashboard with control sliders. Needs testing with real ESP32 hardware.

---

## Key References

- [Project scope](scope.md) — boundaries, philosophy, technical decisions
- [Flight controller scope](../flight_controller/docs/scope.md) — firmware boundaries
- [fc_tool scope](../fc_tool/docs/scope.md) — desktop app boundaries
- [swarm_api scope](../swarm_api/docs/scope.md) — fleet API boundaries
- **engineering360** (separate repo) — physical drone design, component selection, structural analysis

---

*Detailed feature tracking, milestones, and task lists are in each sub-project's roadmap and todo documents.*
