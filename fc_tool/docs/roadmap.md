# fc_tool - Roadmap

> Last updated: 2026-02-10

## Overview

This roadmap tracks project-level features and milestones. For immediate tasks, see `todo.md`.

**Note**: No time estimates. Focus on WHAT needs to be done, not WHEN.

---

## Goal Horizons

### Midterm Goal: v0.1 — First Stable Release

**"Deployable/testable" means:** A user can download a single executable, connect a board via USB, open a serial monitor, and see live data plots. No internet required.

**Must-have for v0.1:**

- [x] Serial port detection and connection
- [x] Serial monitor (send/receive terminal)
- [x] Real-time data plotting (dynamic multi-graph plotter)
- [x] Auto-reconnect on serial disconnect
- [ ] Cross-platform builds (manual local builds on each platform)

**Nice-to-have (defer if needed):**

- [ ] PlatformIO compile/flash integration
- [ ] Calibration parameter display
- [ ] macOS build

### Long-term Goal: v1.0 — Full Developer Workflow

**Means:** Complete loop from parameter editing to compile to flash to monitor to calibrate, all from one tool.

---

## Core Features

### Serial Communication

- [x] Auto-detect available serial ports
  - Description: Scan and list USB serial devices, identify board type when possible
  - Implementation: `list_serial_ports` command in lib.rs, shows VID/PID and product name
- [x] Serial monitor terminal
  - Description: Bidirectional text terminal (send commands, receive output)
  - Implementation: Terminal UI with RX (green) / TX (blue) / system (gray) color coding
- [x] Connection management
  - Description: Port selection, baud rate, connect/disconnect
  - Implementation: `open_serial_port`, `close_serial_port`, `send_serial_data` commands; background reader thread with Tauri events
- [x] Auto-reconnect on disconnect
  - Description: Detect device disconnect, automatically retry connection
  - Implementation: Backend reader thread detects EOF/error, emits `serial-disconnected` event; frontend retries every 2s, max 15 attempts
- [ ] Raw data logging
  - Description: Save serial output to file for offline analysis

### Dynamic Serial Plotter

- [x] Toggle between Monitor and Plotter views
  - Description: Serial Monitor shows first; "Show Plotter" button reveals charts
  - Implementation: Hidden plotter section toggled by button in section header
- [x] Dynamic multi-graph support
  - Description: Variables assigned to different plots using `name@plotId:value` format
  - Implementation: PlotterManager class in src/plotter.js, creates Chart.js instances on demand
- [x] Sparse plot IDs
  - Description: Only create plots for referenced IDs (e.g., 1,3,11 → 3 plots)
  - Implementation: Map-based plot tracking, charts created as new IDs arrive
- [x] Variable-to-plot assignment
  - Description: `temp@1:25.5 humidity@2:65` assigns temp to plot #1, humidity to plot #2
  - Implementation: Regex parser handles `name@plotId:value`, `name:value`, `name=value`
- [x] Backward-compatible with Arduino Serial Plotter
  - Description: Plain `name:value` and CSV formats work (default plot 0)
  - Implementation: Fallback parser splits plain numbers by whitespace/comma
- [x] Dark theme plots
  - Description: Dark canvas background (`#1a1a2e`/`#0d1117`), bright neon data colors
  - Implementation: canvasBackgroundPlugin, NEON_PALETTE (8 colors: lime, coral, cyan, orange, magenta, white, lavender, pink)
- [x] Passive hover crosshair
  - Description: Grey dotted X/Y lines follow mouse, readout panel below plot
  - Implementation: Per-chart inline crosshair plugin via factory function
- [x] Separate clear buttons
  - Description: Independent clear for plots vs serial monitor
  - Implementation: Two separate clear buttons in their respective sections

**Plot interaction & measurement (future):**

- [ ] Trigger Mode — place neon yellow vertical (Y-intercept) and neon blue horizontal (X-intercept) lines for measurement
- [ ] Axis toggle — [Axis: ON/OFF] per plot; right-click switches Y-intercept/X-intercept mode
- [ ] Measurement readout — dedicated panel showing mouse position + delta between placed lines
- [ ] Show data points toggle — small circles at actual data points on/off
- [ ] Measurement cursors — 2 yellow verticals + 2 blue horizontals per plot, draggable AND input fields

Reference: [cursor-interaction-discussion.md](cursor-interaction-discussion.md)

**Plot controls & modes (future):**

- [ ] Pause/freeze mode — stop collecting by default; "Keep recording when paused" toggle (global)
- [x] Y-axis zoom controls — [+] [-] [A] per plot, zoom in/out/auto-fit
- [ ] X-axis scaling — independent zoom/pan for time axis
- [ ] Per-plot mode selector — Continuous / Period Mode (N) / Single period / Frozen
- [ ] Font size controls — [+] [-] for serial monitor text size

Reference: [plotter_discussion.md](plotter_discussion.md)

**Signal / pattern analysis (future):**

- [ ] Period Mode — detect repetitive data, show N periods "standing still"
- [ ] Per-plot mode selection — each plot independently: continuous, period, single, frozen
- [ ] Period detection algorithms — zero-crossing, autocorrelation, FFT
- [ ] Anomaly detection overlay — track max/min/critical points, detect signal changes over time
- [ ] Signal statistics readout — period, frequency, min, max, RMS below each plot

Reference: [signal-analysis-discussion.md](signal-analysis-discussion.md)

**Modular architecture (future):**

- [ ] Dedicated JS modules for plot analytics (chart-manager, cursor-system, readout-panel, period-detector, anomaly-tracker, trigger-mode, color-palette)

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
  - Description: Read and show calibration parameters from serial stream
  - Related: [flight_controller/docs/findings/auto-calibration-research.md](/flight_controller/docs/findings/auto-calibration-research.md)
- [ ] Calibration visualization
  - Description: Visual feedback showing calibration quality/progress
- [ ] Generate config.h snippet from calibration data
  - Description: Export calibration values in copy-pasteable config.h format
  - Notes: Supports the calibrate → hard-code → flash → fly workflow
- [ ] Save/load calibration profiles
  - Description: Store calibration snapshots for different boards or configurations

### Board Management

- [x] Teensy 4.0/4.1 detection
  - Description: Identify Teensy boards by USB VID/PID (0x16C0:0x0483)
  - Reference: [board-vid-pid-reference.md](findings/board-vid-pid-reference.md)
  - Implementation: `identify_board()` in lib.rs maps VID/PID to friendly name
- [x] Arduino detection
  - Description: Identify Arduino boards by VID/PID (0x2341:*)
  - Implementation: Uno, Mega, Leonardo, Micro, Due, R4 all recognized
- [x] ESP32 detection
  - Description: Identify ESP32 boards (CP2102: 0x10C4:0xEA60, CH340: 0x1A86:0x7523, native: 0x303A:*)
  - Implementation: CP2102, CH340, FTDI, ESP32-S2/S3/C3 native all recognized
- [x] Board info display
  - Description: Show connected board type, port, status in dropdown
  - Implementation: Port dropdown shows "portname — BoardName" format
- [ ] USB hot-plug detection
  - Description: Detect device connect/disconnect without manual refresh
  - Notes: Use libudev on Linux, IOKit on macOS, WMI/SetupAPI on Windows
- [ ] Port activity monitoring
  - Description: Show which ports have active data (activity indicator)

### Multi-Device Support

- [ ] Multiple simultaneous connections
  - Description: Open multiple serial monitor windows, one per device
  - Implementation: Tauri multi-window or tabbed interface
- [ ] Device session persistence
  - Description: Remember port/baud settings per device by serial number
- [ ] Device-specific UI state
  - Description: Each connection has its own terminal, charts, and settings

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
- [x] Windows: install-system-deps.bat, setup-dev.bat, build.bat — UNTESTED (converted from PowerShell to .bat)
- [x] macOS: install-system-deps.sh, setup-dev.sh, build.sh — UNTESTED (scripts written, marked with TODO)
- [x] dev_setup/README.md with per-platform quick start
- [x] PlatformIO install via official get-platformio.py in all setup-dev scripts
- [x] PlatformIO PATH setup (`~/.platformio/penv/bin/` or `Scripts\`) in all build scripts

### Platform Validation (future)

- [ ] Clone repo on a real Windows machine, run all three scripts, verify build produces .msi/.exe
- [ ] Clone repo on a real macOS machine, run all three scripts, verify build produces .dmg/.app
- [ ] Remove `# TODO: UNTESTED` markers from Windows scripts after validation
- [ ] Remove `# TODO: UNTESTED` markers from macOS scripts after validation

---

## Nice to Have (Lower Priority)

- [ ] 3D attitude visualization (orientation cube/model using WebGL)
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism (fetch new firmware versions from GitHub releases)

---

## Completed

> Features moved here when done, for historical reference.

- [x] Plotter polish: Y-axis zoom, data rate display, auto-fit grid, origin line, legend toggle — 2026-02-10
- [x] Terminal polish: clipboard copy, filter (Ctrl+F), buffer limit, keyboard shortcuts — 2026-02-10
- [x] Enhanced serial plotter (PlotterManager, dark theme, crosshair, multi-graph protocol) — 2026-02-09
- [x] Auto-reconnect on serial disconnect (backend event + frontend retry loop) — 2026-02-09
- [x] Serial monitor UI with terminal (port selector, baud rate, connect/disconnect, send/receive) — 2026-02-06
- [x] Serial connection backend (open_serial_port, close_serial_port, send_serial_data, events) — 2026-02-06
- [x] Project initialized (Rust + Tauri 2, vanilla JS frontend) — 2026-01-27
- [x] Serial port listing backend command (list_serial_ports via serialport-rs) — 2026-01-27
- [x] Linux build scripts (install-system-deps.sh, setup-dev.sh, build.sh) — 2026-01-27
- [x] Linux release build verified: 13 MB binary, .deb and .rpm bundles — 2026-01-27
- [x] Windows build scripts (install-system-deps.bat, setup-dev.bat, build.bat) — 2026-01-27 (converted from .ps1 to .bat 2026-02-05)
- [x] macOS build scripts (install-system-deps.sh, setup-dev.sh, build.sh) — 2026-01-27
- [x] Reorganized scripts into dev_setup/linux/, dev_setup/windows/, dev_setup/macos/ — 2026-01-27

---

## Notes

- The serial plotter uses a flexible protocol parser that handles `name@plotId:value`, `name:value`, `name=value`, and plain CSV/space-separated numbers
- MVP focuses on serial + plotter; PlatformIO integration and calibration UI come after
- Stability mode applies: if serial monitoring + plotting works, ship it before adding more features
- The flight_controller has a calibration mode vs live mode workflow — fc_tool's calibration interface should help users go from raw calibration output to hard-coded config.h values seamlessly
- See [flight_controller/docs/scope.md](/flight_controller/docs/scope.md) and [flight_controller/docs/roadmap.md](/flight_controller/docs/roadmap.md) for firmware context

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
