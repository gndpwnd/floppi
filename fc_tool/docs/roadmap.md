# fc_tool - Roadmap

> Last updated: 2026-01-27

## Overview

This roadmap tracks project-level features and milestones. For immediate tasks, see `todo.md`.

**Note**: No time estimates. Focus on WHAT needs to be done, not WHEN.

---

## Goal Horizons

### Midterm Goal: v0.1 — First Stable Release

**"Deployable/testable" means:** A user can download a single executable, connect a Teensy board via USB, open a serial monitor, and see live IMU data plots. No internet required.

**Must-have for v0.1:**
- [ ] Serial port detection and connection
- [ ] Serial monitor (send/receive terminal)
- [ ] Real-time IMU data parsing and plotting (accel + gyro)
- [ ] Cross-platform builds (at least Linux + Windows)

**Nice-to-have (defer if needed):**
- [ ] PlatformIO compile/flash integration
- [ ] Calibration parameter display
- [ ] macOS build

### Long-term Goal: v1.0 — Full Developer Workflow

**Means:** Complete loop from parameter editing to compile to flash to monitor to calibrate, all from one tool.

---

## Core Features

### Serial Communication

- [ ] Auto-detect available serial ports
  - Description: Scan and list USB serial devices, identify board type when possible
- [ ] Serial monitor terminal
  - Description: Bidirectional text terminal (send commands, receive output)
- [ ] Connection management
  - Description: Port selection, baud rate, connect/disconnect, reconnect on drop
- [ ] Raw data logging
  - Description: Save serial output to file for offline analysis

### IMU Data Visualization

- [ ] Telemetry data parser
  - Description: Parse structured serial output into sensor values
  - Dependencies: Serial communication, telemetry protocol definition
- [ ] Real-time accelerometer plot
  - Description: Live X/Y/Z acceleration graph
- [ ] Real-time gyroscope plot
  - Description: Live X/Y/Z angular rate graph
- [ ] Magnetometer plot
  - Description: Live X/Y/Z magnetic field graph (when available)
- [ ] Data export
  - Description: Export captured sensor data to CSV

### PlatformIO Integration

- [ ] Detect PlatformIO installation
  - Description: Verify PlatformIO CLI is available, show version
- [ ] Compile firmware
  - Description: Trigger `pio run` for the flight_controller project
  - Dependencies: PlatformIO installed, flight_controller project path configured
- [ ] Flash firmware
  - Description: Trigger `pio run --target upload` to flash connected board
  - Dependencies: Compile, board connected
- [ ] Build output display
  - Description: Show PlatformIO compile/upload output in GUI

### Calibration Interface

- [ ] Display current calibration values
  - Description: Read and show IMU calibration parameters from serial stream
- [ ] Calibration visualization
  - Description: Visual feedback showing calibration quality/progress
- [ ] Save/load calibration profiles
  - Description: Store calibration snapshots for different boards or configurations

### Board Management

- [ ] Teensy 4.0/4.1 detection
  - Description: Identify Teensy boards by USB VID/PID
- [ ] ESP32 detection (future)
  - Description: Add when flight_controller ESP32 build is ready
- [ ] Board info display
  - Description: Show connected board type, port, status

---

## Infrastructure / Setup

- [x] Initialize Rust + Tauri project structure
- [x] Set up frontend scaffolding (HTML/CSS/JS)
- [x] Verify Linux release build compiles (.deb, .rpm produced)
- [x] Set up serial port abstraction layer in Rust backend (list_serial_ports command)

### Dev Environment Scripts (dev_setup/)

All scripts install to default home directory locations (`~/.cargo/`, `~/.nvm/`, `~/.platformio/`).
Each setup-dev script installs: Rust, Node.js, PlatformIO (optional), and npm dependencies.
Each build script sources the required env vars before compiling.

- [x] Linux: install-system-deps.sh, setup-dev.sh, build.sh — **VALIDATED**
- [x] Windows: install-system-deps.ps1, setup-dev.ps1, build.ps1 — UNTESTED (scripts written, marked with TODO)
- [x] macOS: install-system-deps.sh, setup-dev.sh, build.sh — UNTESTED (scripts written, marked with TODO)
- [x] dev_setup/README.md with per-platform quick start
- [x] PlatformIO install via official get-platformio.py in all setup-dev scripts
- [x] PlatformIO PATH setup (`~/.platformio/penv/bin/` or `Scripts\`) in all build scripts

### Platform Validation (future)

- [ ] Clone repo on a real Windows machine, run all three scripts, verify build produces .msi/.exe
- [ ] Clone repo on a real macOS machine, run all three scripts, verify build produces .dmg/.app
- [ ] Remove `# TODO: UNTESTED` markers from Windows scripts after validation
- [ ] Remove `# TODO: UNTESTED` markers from macOS scripts after validation

### CI/CD & Releases

- [x] GitHub Actions workflow: manually triggered, builds on Linux + Windows + macOS runners
- [ ] Test GitHub Actions workflow on first real push (validate all three platform builds)
- [ ] Verify CI produces .msi/.exe (Windows), .dmg/.app (macOS), .deb/.AppImage (Linux)
- [ ] First tagged pre-release published to GitHub Releases

---

## Nice to Have (Lower Priority)

- [ ] 3D attitude visualization (orientation cube/model using WebGL)
- [ ] Dark mode / theme support
- [ ] Multiple simultaneous serial connections
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism (fetch new firmware versions from GitHub releases)

---

## Completed

> Features moved here when done, for historical reference.

- [x] Project initialized (Rust + Tauri 2, vanilla JS frontend) — 2026-01-27
- [x] Serial port listing backend command (list_serial_ports via serialport-rs) — 2026-01-27
- [x] Linux build scripts (install-system-deps.sh, setup-dev.sh, build.sh) — 2026-01-27
- [x] Linux release build verified: 13 MB binary, .deb and .rpm bundles — 2026-01-27
- [x] Windows build scripts (install-system-deps.ps1, setup-dev.ps1, build.ps1) — 2026-01-27
- [x] macOS build scripts (install-system-deps.sh, setup-dev.sh, build.sh) — 2026-01-27
- [x] Reorganized scripts into dev_setup/linux/, dev_setup/windows/, dev_setup/macos/ — 2026-01-27
- [x] GitHub Actions workflow (manual trigger, multi-platform release) — 2026-01-27

---

## Notes

- The serial telemetry protocol is undefined — fc_tool should be flexible enough to handle format changes as firmware matures
- MVP focuses on serial + IMU plots; PlatformIO integration and calibration UI come after
- Stability mode applies: if serial monitoring works, ship it before adding more features

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
