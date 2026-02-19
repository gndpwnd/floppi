# fc_tool - Todo

> Last updated: 2026-02-18 (session 9)

## Up Next

- [ ] Test with real Teensy hardware (serial + plotter visualization) — USB ports occupied
- [ ] Validate cross-platform builds (Windows, macOS)

## Backlog (Post-v0.1)

- [ ] Plotter discussion — partially complete (low priority, not blocking)
  - See [plotter_discussion.md](plotter_discussion.md), [cursor-interaction-discussion.md](cursor-interaction-discussion.md), [signal-analysis-discussion.md](signal-analysis-discussion.md)

### Enhanced Plotter — Decided Features

- [x] Pause/freeze mode — "Keep recording" toggle buffers data while paused, flushes on unpause — **DONE** 2026-02-17
- [x] Font size controls — [+] [-] buttons for serial monitor (8–24px range) — **DONE** 2026-02-17
- [x] Show data points toggle — "Points" checkbox toggles circles at sample points — **DONE** 2026-02-17
- [x] Measurement cursors — 2 yellow verticals + 2 blue horizontals, draggable, per-plot trigger button — **DONE** 2026-02-17
- [x] X/Y axis zoom + pan — symmetric controls for both axes (+/−/A/pan) — **DONE** 2026-02-17
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
- [x] Timestamped log lines — `[epoch.millis]` prefix on all log file lines — **DONE** 2026-02-17

### Platform Validation

- [ ] Validate Windows dev_setup scripts on real Windows machine
- [ ] Validate macOS dev_setup scripts on real macOS machine

### Nice to Have (Low Priority)

- [ ] 3D attitude visualization (WebGL orientation cube)
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism

## Recently Completed

- [x] Serial protocol lexicon & port monitoring research — 2026-02-18 (session 9)
  - Rewrote `docs/features/serial-telemetry-protocol.md` as definitive protocol reference
  - Covers all 4 plotter formats, dashboard protocol, ANSI codes, Arduino IDE compatibility
  - Web research: Arduino Serial Plotter official spec, Teleplot, IDE 1.x vs 2.x differences
  - Port monitoring research: `docs/findings/serial-port-filtering-research.md` (serialport-rs internals, platform differences, how Arduino IDE/PlatformIO filter ports)
  - Port activity research: `docs/findings/serial-port-activity-detection.md` (cannot detect activity without opening port — VID/PID + recency is industry standard)
  - Arduino compatibility research: `docs/findings/arduino-serial-plotter-compatibility.md`
  - Decision: modes/colors via GUI only (not serial protocol). Protocol stays data-only.
  - Decision: no axis specification needed — X is always time, Y label = variable name
- [x] Dynamic plot management — 2026-02-18 (session 8)
  - Individual plot close button (×) in each plot header
  - `removePlot(plotId)` method in PlotterManager — auto-recreates when data returns
  - Ctrl+Shift+C keyboard shortcut to clear all plots
- [x] Serial protocol simulation & testing expansion — 2026-02-18 (session 7)
  - **240 tests** total (up from 183): unit (156), integration (45), e2e (33), performance (6)
  - **Phase 1**: Modularized `simulate_serial.py` into `tests/simulator/` package (core.py, __init__.py, __main__.py, 8 scenario modules)
  - **Phase 2**: 7 new scenarios — ramp, intermittent, burst, calibration, multi_plot, mixed_dashboard, high_channel (15 scenarios total)
  - **Phase 3**: 3 new test files — test_simulator_new.py (25 integration), test_serial_pipeline.py (8 e2e), test_serial_resilience.py (4 e2e)
  - **Phase 4**: `RunningStats` class + `cursor_delta()` in parsers.py, test_statistics.py (20 unit tests)
  - Old bash tests (29/29) still pass alongside pytest
  - New markers: `slow` for long-running tests (>10s)
- [x] pytest test framework — 2026-02-18 (session 5-6)
  - **183 tests** across 3 tiers: unit (136), integration (20), e2e (21), performance (6)
  - `parsers.py` — Python ports of JS regexes (parse_line, parse_ansi, parse_dashboard_line, format_val)
  - `conftest.py` — shared fixtures: fc_tool_bin discovery, socat_pair, run_simulator_stdout, run_headless
  - 10 test files: test_parse_line, test_parse_ansi, test_parse_dashboard, test_cursors, test_simulator, test_edge_cases, test_headless, test_headless_log, test_cli_args, test_performance
  - Markers: `unit` (<0.2s), `integration` (~15s), `e2e` (~43s), `performance` (~4s), `edge_case`
  - Run: `pytest` (all), `pytest -m unit` (fast), `pytest test_parse_line.py::TestNamedPlotFormat::test_single_named_plot` (single)
  - Old bash tests (29/29) continue working alongside pytest
- [x] Session 4 features — 2026-02-17 (session 4)
  - Measurement cursors — cursors.js module: 2 yellow verticals + 2 blue horizontals per plot, draggable, "Trigger" toggle button, right-click axis switch, delta readout
  - X/Y axis symmetric zoom + pan — both axes: [+] [−] [A] + directional pan buttons (▲▼ for Y, ◀▶ for X)
  - USB hot-plug detection — polling-based port watcher (2s interval), auto-refresh port list, system messages for connect/disconnect
  - Timestamped log lines — `[epoch.millis]` prefix in both GUI and headless log files
  - Modular JS split — main.js (674 lines) → ansi.js + dashboard.js + connection.js + cursors.js + main.js coordinator (331 lines)
  - All 29/29 tests passing
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
| serial-port-filtering-research.md | Complete | serialport-rs internals, platform differences (Linux/Win/macOS), port list best practices |
| serial-port-activity-detection.md | Complete | Cannot detect activity without opening port, VID/PID + recency approach, industry comparison |
| arduino-serial-plotter-compatibility.md | Complete | Arduino IDE protocol spec, IDE 1.x vs 2.x, fc_tool compatibility matrix, Teleplot comparison |

Also in `docs/literature/findings/`:

| Document | Status | Summary |
|----------|--------|---------|
| serial-rich-text-formatting.md | Complete | ANSI escape codes, terminal support matrix, performance at 115200 baud |

---

*Update every session: start by reading, end by updating.*
