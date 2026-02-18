# fc_tool - Roadmap

> Last updated: 2026-02-17 (session 4)

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

- [ ] macOS build

### Long-term Goal: v1.0 — Full Developer Workflow

**Means:** Complete serial monitoring, data visualization, and analysis workflow from one tool.

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
- [x] CLI arguments for port and baud
  - Completed: 2026-02-11
  - Description: Launch fc_tool with `--port /dev/ttyACM0 --baud 115200` to skip manual selection.
  - Implementation: `std::env::args()` in main.rs, `StartupArgs` struct as Tauri managed state, `get_startup_args` command for frontend. Frontend auto-connects on matching startup args.
- [x] Headless mode (raw serial output)
  - Completed: 2026-02-11
  - Description: `--headless` flag runs fc_tool without the GUI window. Prints all serial data (including plotter data) to stdout. Supports stdin→serial forwarding for interactive use.
  - Implementation: Bypasses Tauri entirely — pure Rust serial loop. Lists available ports if --port not specified.
- [x] Force-release serial connections
  - Completed: 2026-02-17
  - Description: `--kill-port /dev/ttyACM0` forcibly releases a serial port held by stale processes. Uses `fuser -k` on Linux.
  - Implementation: `kill_port()` in lib.rs. Checks port exists, identifies holder PIDs, kills them, waits for release.
- [x] Raw data logging
  - Completed: 2026-02-17
  - Description: `--log <file>` in headless mode tees all serial data to a file while also printing to stdout. GUI mode: "Log" button starts/stops logging to `fc_tool_<timestamp>.log`.
  - Implementation: Headless: optional `log_writer` in `run_headless()`. GUI: `start_log`/`stop_log` Tauri commands, `log_writer` in SerialState, reader thread writes when active.
- [x] Timestamped log lines
  - Completed: 2026-02-17
  - Description: All log file lines prefixed with `[epoch.millis]` timestamp (e.g., `[1740000000.123] data...`). Uses `std::time::SystemTime` (no chrono dependency).
  - Implementation: `format_timestamp()` in lib.rs. Applied in both GUI reader thread and headless mode.

### Serial Monitor Enhancements

- [x] ANSI escape code rendering
  - Completed: 2026-02-11
  - Description: Parse ANSI SGR sequences (bold, dim, underline, 8/16 foreground colors) and render as styled HTML spans. Toggle checkbox in toolbar.
  - Notes: Regex-based parser in appendRx(). CSS classes for each color, optimized for dark terminal theme. Zero overhead when no ANSI codes present.
- [x] CLI arguments for port and baud — **DONE** (2026-02-11, see Serial Communication above)
- [x] Headless mode — **DONE** (2026-02-11, see Serial Communication above)
- [x] Live Dashboard Mode
  - Completed: 2026-02-17
  - Description: Toggleable panel showing key=value data updating in place (not scrolling). Each unique key gets a cell in a responsive CSS grid. Values update live with brief white flash on change. Coexists alongside scrolling terminal.
  - Implementation: `processSerialForDashboard()` in main.js. Regex parser `([\w.]+)=(\S+)` matches `key=value` pairs. Dashboard Map tracks latest values. Toggle via "Show Dashboard" button or Ctrl+Shift+D.
- [ ] Companion Arduino library (floppi_serial)
  - Description: Lightweight Arduino library providing helpers for fc_tool's protocol features — multi-graph plotting (`plotVar()`), ANSI color macros, structured telemetry output, atomic line buffering
  - Notes: Not required — fc_tool works with raw `Serial.print()`. The library just makes it more convenient. Must be <5KB flash, zero-allocation. Compile-time ANSI toggle. See research findings.
  - Reference: [serial-formatting-libraries-research.md](findings/serial-formatting-libraries-research.md)

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

**Plot interaction & measurement:**

- [x] Trigger Mode — place neon yellow vertical (Y-intercept) and neon blue horizontal (X-intercept) lines for measurement
  - Completed: 2026-02-17
  - Implementation: "Trigger" toggle button per plot. Click to place cursor lines (max 2 per axis). Right-click toggles between vertical/horizontal mode. Lines are draggable with 8px grab threshold.
- [x] Axis toggle — right-click switches Y-intercept/X-intercept mode
  - Completed: 2026-02-17
- [x] Measurement readout — dedicated panel showing cursor positions + delta between placed lines
  - Completed: 2026-02-17
  - Implementation: Delta readout below crosshair readout. Shows Y1/Y2/ΔY (yellow) and X1/X2/ΔX (blue).
- [x] Show data points toggle — "Points" checkbox in plotter toolbar, toggles circles at sample points
  - Completed: 2026-02-17
- [x] Measurement cursors — 2 yellow verticals + 2 blue horizontals per plot, draggable
  - Completed: 2026-02-17
  - Implementation: cursors.js module (createMeasurementState, createMeasurementPlugin, attachMeasurementEvents). Chart.js inline plugin + native DOM events. Colors: yellow (#FFFF00) vertical, blue (#00BFFF) horizontal.

Reference: [cursor-interaction-discussion.md](cursor-interaction-discussion.md)

**Plot controls & modes:**

- [x] Pause/freeze mode — "Keep recording when paused" toggle buffers data, flushes on unpause
  - Completed: 2026-02-17
- [x] Y-axis zoom/pan controls — [Y +] [-] [A] [▲] [▼] per plot: zoom in/out/auto-fit/pan up/pan down
  - Updated: 2026-02-17 — added pan up/down buttons for symmetric X/Y controls
- [x] X-axis zoom/pan controls — [X +] [−] [A] [◀] [▶] per plot: zoom in/out/auto-fit/pan left/pan right
  - Completed: 2026-02-17
  - Implementation: xZoom (fraction of data window visible), xPan (offset from right edge). Applied via Chart.js x scale min/max. Pan disabled when auto-fit.
- [ ] Per-plot mode selector — Continuous / Period Mode (N) / Single period / Frozen
- [x] Font size controls — [+] [-] buttons for serial monitor text size (8–24px)
  - Completed: 2026-02-17

Reference: [plotter_discussion.md](plotter_discussion.md)

**Signal / pattern analysis (future):**

- [ ] Period Mode — detect repetitive data, show N periods "standing still"
- [ ] Per-plot mode selection — each plot independently: continuous, period, single, frozen
- [ ] Period detection algorithms — zero-crossing, autocorrelation, FFT
- [ ] Anomaly detection overlay — track max/min/critical points, detect signal changes over time
- [x] Signal statistics readout — min, max, avg per variable below each plot
  - Completed: 2026-02-17

Reference: [signal-analysis-discussion.md](signal-analysis-discussion.md)

**Modular architecture:**

- [x] JS module split — main.js split into focused modules
  - Completed: 2026-02-17
  - Implementation: ansi.js (ANSI parser), dashboard.js (Dashboard class), connection.js (Connection class), cursors.js (measurement cursors), plotter.js (PlotterManager), main.js (coordinator)
- [ ] Dedicated JS modules for advanced plot analytics (period-detector, anomaly-tracker)

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
- [x] USB hot-plug detection
  - Completed: 2026-02-17
  - Description: Detect device connect/disconnect without manual refresh
  - Implementation: Polling-based (2s interval). Background Rust thread compares `serialport::available_ports()` snapshots, emits `ports-changed` event with added/removed arrays. Frontend auto-refreshes port list and shows system messages. Cross-platform, no extra dependencies.
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

All scripts install to default home directory locations (`~/.cargo/`, `~/.nvm/`).
Each setup-dev script installs: Rust, Node.js, and npm dependencies.
Each build script sources the required env vars before compiling.

- [x] Linux: install-system-deps.sh, setup-dev.sh, build.sh — **VALIDATED**
- [x] Windows: install-system-deps.bat, setup-dev.bat, build.bat — UNTESTED (converted from PowerShell to .bat)
- [x] macOS: install-system-deps.sh, setup-dev.sh, build.sh — UNTESTED (scripts written, marked with TODO)
- [x] dev_setup/README.md with per-platform quick start
- [x] PlatformIO install via official get-platformio.py in all setup-dev scripts
- [x] PlatformIO PATH setup (`~/.platformio/penv/bin/` or `Scripts\`) in all build scripts

### Test Infrastructure

- [x] Serial data simulator (`tests/simulate_serial.py`)
  - Completed: 2026-02-17
  - Description: Python script generating fake serial data in all 4 protocol formats. 8 scenarios: imu, sine, mixed, ansi, stress, dashboard, protocol, noise.
  - Implementation: Derivative of flight_controller/tools/serial_monitor.py. Writes to virtual serial port (socat pty pair) or stdout.
- [x] Plotter test suite (`tests/test_plotter.sh`)
  - Completed: 2026-02-17
  - Description: Tests all simulator scenarios, protocol format verification, stress test (max plots cap), headless mode with virtual serial.
- [x] Monitor test suite (`tests/test_monitor.sh`)
  - Completed: 2026-02-17
  - Description: Tests ANSI escape code generation, dashboard format, headless echo round-trip, headless ANSI passthrough.
- [x] Max plots cap (10)
  - Completed: 2026-02-17
  - Description: PlotterManager limits to 10 simultaneous plots. Logs console warning when cap reached.

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

- [x] Measurement cursors, X/Y zoom+pan, USB hot-plug, timestamped logs, JS module split — 2026-02-17 (session 4)
- [x] Live Dashboard Mode, signal statistics readout, GUI-mode logging — 2026-02-17
- [x] Plotter enhancements: font size controls, show data points toggle, pause with keep-recording — 2026-02-17
- [x] Force-release serial port (`--kill-port`), raw data logging (`--log`), stale Hz fix — 2026-02-17
- [x] Test infrastructure: simulator, plotter tests, monitor tests, virtual serial via socat — 2026-02-17
- [x] Max plots cap (10), removed unused tokio dep, code review fixes — 2026-02-17
- [x] Plotter polish: Y-axis zoom, data rate display, auto-fit grid, origin line, legend toggle — 2026-02-10
- [x] Terminal polish: clipboard copy ("Copy All"), filter (Ctrl+F), buffer limit, keyboard shortcuts — 2026-02-10
- [x] Enhanced serial plotter (PlotterManager, dark theme, crosshair, multi-graph protocol) — 2026-02-09
- [x] CLI arguments (--port, --baud) + headless mode (--headless) — 2026-02-11
- [x] ANSI escape code rendering (bold, dim, underline, 16 colors) — 2026-02-11
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
- MVP focuses on serial + plotter — ship it before adding more features
- Firmware compilation and flashing are handled via existing PlatformIO scripts (`pio run`), not fc_tool
- **Testing policy:** All testing via bash scripts in `tests/`, never manual terminal commands. LLM/agents must use test scripts, not ad-hoc hardware commands. See scope.md Testing Policy for details.
- **Test commands:** `sudo apt-get install socat` (once), then `./tests/test_plotter.sh && ./tests/test_monitor.sh`. See [README.md Testing](README.md#testing) for full reference.
- See [flight_controller/docs/scope.md](/flight_controller/docs/scope.md) and [flight_controller/docs/roadmap.md](/flight_controller/docs/roadmap.md) for firmware context

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
