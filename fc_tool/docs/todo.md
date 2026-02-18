# fc_tool - Todo

> Last updated: 2026-02-17 (session 3)

## In Progress

- [ ] Plotter discussion — partially complete (low priority, not blocking)
  - Decisions captured: cursor system, scaling controls, pause mode, visual style, trigger vs period mode
  - Open questions remain: Q6, Q7, Q11-Q14, Q16, Q18-Q20, Q23-Q25, plus cursor/visual Qs
  - See [plotter_discussion.md](plotter_discussion.md), [cursor-interaction-discussion.md](cursor-interaction-discussion.md), [signal-analysis-discussion.md](signal-analysis-discussion.md)

## Up Next

- [x] ~~Install system deps and verify Rust compiles~~ — DONE (cargo check passes, rustc 1.93.0)
- [x] ~~Install socat and run full test suite~~ — DONE (29/29 tests pass)
- [ ] Test with real Teensy hardware (serial + plotter visualization) — USB ports occupied
- [ ] Validate cross-platform builds (Windows, macOS)

## Backlog (Post-v0.1)

### Enhanced Plotter — Decided Features

- [x] Pause/freeze mode — "Keep recording" toggle buffers data while paused, flushes on unpause — **DONE** 2026-02-17
- [x] Font size controls — [+] [-] buttons for serial monitor (8–24px range) — **DONE** 2026-02-17
- [x] Show data points toggle — "Points" checkbox toggles circles at sample points — **DONE** 2026-02-17
- [ ] Measurement cursors — 2 yellow verticals + 2 blue horizontals, draggable + input fields
- [ ] Per-plot mode selector — Continuous / Period Mode (N) / Single period / Frozen

### Signal Analysis Features (research complete, needs discussion)

- [ ] Period Mode — detect repetitive data, show N periods standing still
- [ ] Period detection algorithms — zero-crossing, autocorrelation, FFT (see findings)
- [ ] Anomaly detection overlay — EMA, CUSUM, peak/trough tracking (see findings)
- [x] Signal statistics readout — min, max, avg per variable per plot — **DONE** 2026-02-17
- [ ] Sparkline mini-graphs — trend display for peak/mean/trough

### Modular Architecture

- [ ] Extract charting into dedicated JS modules:
  - chart-manager.js, cursor-system.js, readout-panel.js
  - period-detector.js, anomaly-tracker.js, trigger-mode.js, color-palette.js

### Serial Monitor Enhancements (research complete)

- [x] ANSI escape code rendering — bold, dim, underline, 16 colors as styled HTML spans — **DONE** 2026-02-11
- [x] Live Dashboard Mode — fixed-position panel showing key=value data updating in place (not scrolling) — **DONE** 2026-02-17
- [ ] Companion Arduino library (floppi_serial) — plotVar(), ANSI macros, atomic line buffering

### Serial Features

- [x] Raw data logging from GUI mode — "Log" button starts/stops logging to timestamped file — **DONE** 2026-02-17
- [ ] Timestamped log lines (prefix each line with ISO timestamp)

### Platform Validation

- [ ] Validate Windows dev_setup scripts on real Windows machine
- [ ] Validate macOS dev_setup scripts on real macOS machine

### Nice to Have (Low Priority)

- [ ] 3D attitude visualization (WebGL orientation cube)
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism

## Recently Completed

- [x] New features — 2026-02-17 (session 3)
  - Live Dashboard Mode — toggleable `key=value` grid panel, updates in place, change flash highlight, Ctrl+Shift+D shortcut
  - Signal statistics readout — min/max/avg per variable displayed below each plot
  - GUI-mode logging — "Log" button in terminal controls, writes serial data to `fc_tool_<timestamp>.log`
  - Backend: `start_log` / `stop_log` Tauri commands, reader thread writes to log file when active
  - All 29/29 tests passing after implementation
- [x] Plotter enhancements — 2026-02-17 (session 2)
  - Font size controls [+] [-] for terminal (8–24px, default 12px)
  - Show data points toggle — "Points" checkbox in plotter toolbar
  - Pause with "Keep recording" — buffer data while paused, flush on unpause (capped at 2x window size)
  - All 29/29 tests passing after implementation
- [x] Test infrastructure — 2026-02-17
  - `tests/simulate_serial.py` — Python data simulator with 8 scenarios (imu, sine, mixed, ansi, stress, dashboard, protocol, noise)
  - `tests/test_plotter.sh` — Plotter test suite (12 tests: format verification, stress test, headless+socat)
  - `tests/test_monitor.sh` — Monitor test suite (9 tests: ANSI codes, dashboard format, headless echo)
  - Virtual serial ports via socat for testing without real hardware
  - `tests/results/` gitignored
- [x] Code review fixes — 2026-02-17
  - Removed unused `tokio` dependency from Cargo.toml (saves compile time + ~1MB binary)
  - Fixed Cargo.toml and README.md description (said "firmware management", now "data visualization")
  - Removed redundant `use std::io::Write` in lib.rs
  - Added max plots cap (10) in PlotterManager — logs console warning when exceeded
- [x] CLI arguments and headless mode — 2026-02-11
  - `--port /dev/ttyACM0 --baud 115200` for auto-connect on launch
  - `--headless` flag for raw serial to stdout (no GUI window)
  - Headless mode includes stdin→serial forwarding for interactive use
  - Frontend queries startup args via `get_startup_args` command, auto-connects
- [x] ANSI escape code rendering — 2026-02-11
  - SGR parser: bold, dim, underline, 8+8 foreground colors (standard + bright)
  - Compound codes (e.g., `\033[1;31m` = bold red)
  - ANSI toggle checkbox in toolbar (enabled by default)
  - CSS color classes matching dark terminal theme
  - No ANSI codes? Falls through to plain text (zero overhead)
- [x] Copy All button renamed for clarity — 2026-02-10
- [x] Autoscroll fix: pauses during text selection (mousedown/mouseup) — 2026-02-11
- [x] fc_tool Rust backend debug build verified — 2026-02-10
- [x] PIO serial monitor fallback documented in README.md — 2026-02-10
- [x] Plotter improvements — 2026-02-10
  - Fixed sample count bug (was counting per-event, now per-line)
  - Data window size control (50-5000, adjustable in toolbar)
  - Y=0 origin line (subtle solid line when zero in view)
  - Legend click to toggle series visibility
  - Plot headers show variable names (e.g. "Plot 0: ax, ay, az")
  - Y-axis zoom controls per plot ([+] [-] [A] buttons)
  - Auto-fit grid layout (1 plot = full width, 2+ = columns)
  - Data rate display in status bar (Hz)
- [x] Terminal improvements — 2026-02-10
  - Copy to clipboard button (with "Copied!" feedback)
  - Terminal buffer limit (5000 lines max, prevents memory growth)
  - Search/filter input (Ctrl+F to focus, Esc to clear)
  - Keyboard shortcuts: Ctrl+L clear, Ctrl+Shift+P plotter toggle, Ctrl+Shift+Space pause
- [x] Documentation updated — 2026-02-09
  - Root docs/scope.md rewritten
  - fc_tool docs/scope.md, roadmap.md, todo.md updated
- [x] Enhanced serial plotter — dynamic multi-graph support — 2026-02-09
  - PlotterManager class in src/plotter.js
  - Protocol: `name@plotId:value`, `name:value`, `name=value`, plain CSV
  - Dynamic plot creation (sparse IDs: 1,3,11 → 3 plots)
  - Dark theme (dark canvas background, bright neon data palette)
  - Passive hover crosshair (grey dotted lines + readout below plot)
  - Toggle "Show Plotter" button (hidden by default)
  - Separate clear buttons for plots vs serial monitor
  - Replaced old hardcoded IMU accel/gyro charts
- [x] Auto-reconnect on serial disconnect — 2026-02-09
  - Backend detects disconnect via reader thread exit, emits `serial-disconnected` event
  - Frontend retries every 2s, max 15 attempts
  - Shows "Reconnecting..." status during attempts
- [x] Research agents completed (cursor-crosshair, dark-theme-neon) — 2026-02-09
- [x] Plotter discussion — major progress on feature design — 2026-02-07
- [x] Deploy script created (deploy.sh) — 2026-02-06
- [x] Multi-graph plotter protocol designed — 2026-02-06
- [x] Serial telemetry protocol defined and implemented — 2026-02-06
- [x] Board identification by VID/PID (Teensy, Arduino, ESP32) — 2026-02-06
- [x] Serial monitor tested with Teensy hardware — 2026-02-06
- [x] Terminal CSS: 33% taller, text selection enabled — 2026-02-06
- [x] IMU visualization with Chart.js — 2026-02-06
- [x] Serial monitor UI + backend — 2026-02-06
- [x] Linux build scripts validated — 2026-01-27
- [x] Project initialized (Rust + Tauri 2) — 2026-01-27

---

## Research Documents

All in `fc_tool/docs/findings/`:

| Document | Status | Summary |
|----------|--------|---------|
| signal-period-detection-research.md | Complete | 5 detection techniques, JS code, library comparison |
| anomaly-detection-research.md | Complete | 6 approaches, AnomalyEngine, Chart.js visualization |
| chartjs-signal-visualization-research.md | Complete | TriggerEngine, PlotController, mode switching, sparklines |
| cursor-crosshair-research.md | Complete | Custom crosshair plugin, measurement lines, multi-chart state |
| dark-theme-neon-research.md | Complete | Dark theme setup, neon palette, origin axes, segment coloring |
| chartjs-oscilloscope-research.md | Complete | Chart.js plugins for oscilloscope features |
| multi-graph-plotter-research.md | Complete | Protocol design, Arduino compatibility |
| board-vid-pid-reference.md | Complete | USB VID/PID reference |
| serial-formatting-libraries-research.md | Complete | Arduino libs, MSP/MAVLink/Firmata, protocol design patterns, ESP32/Teensy specifics |

Also in `docs/literature/findings/`:

| Document | Status | Summary |
|----------|--------|---------|
| serial-rich-text-formatting.md | Complete | ANSI escape codes, terminal support matrix, performance at 115200 baud |

---

*Update every session: start by reading, end by updating.*
