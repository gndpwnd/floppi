# fc_tool - Todo

> Last updated: 2026-02-10

## In Progress

- [ ] Plotter discussion — partially complete (low priority, not blocking)
  - Decisions captured: cursor system, scaling controls, pause mode, visual style, trigger vs period mode
  - Open questions remain: Q6, Q7, Q11-Q14, Q16, Q18-Q20, Q23-Q25, plus cursor/visual Qs
  - See [plotter_discussion.md](plotter_discussion.md), [cursor-interaction-discussion.md](cursor-interaction-discussion.md), [signal-analysis-discussion.md](signal-analysis-discussion.md)

## Up Next

- [ ] Test with real Teensy hardware (serial + plotter visualization) — hardware now connected and available for testing
- [ ] Validate cross-platform builds (Windows, macOS)
- [ ] Trigger Mode cursor system (neon yellow/blue intercept lines)
- [ ] X-axis scaling controls (independent zoom/pan)

## Backlog (Post-v0.1)

### Enhanced Plotter — Decided Features (ready to implement)

- [ ] Pause/freeze mode — stop collecting by default, "Keep recording" toggle
- [ ] Font size controls — [+] [-] for serial monitor
- [ ] Show data points toggle — circles at actual data points
- [ ] Measurement cursors — 2 yellow verticals + 2 blue horizontals, draggable + input fields
- [ ] Per-plot mode selector — Continuous / Period Mode (N) / Single period / Frozen

### Signal Analysis Features (research complete, needs discussion)

- [ ] Period Mode — detect repetitive data, show N periods standing still
- [ ] Period detection algorithms — zero-crossing, autocorrelation, FFT (see findings)
- [ ] Anomaly detection overlay — EMA, CUSUM, peak/trough tracking (see findings)
- [ ] Signal statistics readout — period, frequency, min, max, RMS
- [ ] Sparkline mini-graphs — trend display for peak/mean/trough

### Modular Architecture

- [ ] Extract charting into dedicated JS modules:
  - chart-manager.js, cursor-system.js, readout-panel.js
  - period-detector.js, anomaly-tracker.js, trigger-mode.js, color-palette.js

### Serial Monitor Enhancements (research complete)

- [ ] ANSI escape code rendering — bold, dim, underline, 16 colors as styled HTML spans
- [ ] Live Dashboard Mode — fixed-position panel showing key=value data updating in place (not scrolling)
- [ ] Companion Arduino library (floppi_serial) — plotVar(), ANSI macros, atomic line buffering

### Serial Features

- [ ] Raw data logging to file (timestamped)

### Platform Validation

- [ ] Validate Windows dev_setup scripts on real Windows machine
- [ ] Validate macOS dev_setup scripts on real macOS machine

### Nice to Have (Low Priority)

- [ ] 3D attitude visualization (WebGL orientation cube)
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism

## Recently Completed

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
