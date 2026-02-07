# fc_tool - Todo

> Last updated: 2026-02-07

## In Progress

- [ ] Plotter discussion — partially complete, resume next session
  - Decisions captured: cursor system, scaling controls, pause mode, visual style, trigger vs period mode
  - Open questions remain: Q6, Q7, Q11-Q14, Q16, Q18-Q20, Q23-Q25, plus cursor/visual Qs
  - See [plotter_discussion.md](plotter_discussion.md), [cursor-interaction-discussion.md](cursor-interaction-discussion.md), [signal-analysis-discussion.md](signal-analysis-discussion.md)
- [x] Collect remaining research agent results (cursor-crosshair, dark-theme) — completed

## Up Next

- [ ] Finish plotter discussion (answer remaining open questions)
- [ ] Implement enhanced serial plotter with dynamic multi-graph support
- [ ] Toggle between Monitor and Plotter views (button to show/hide plotter)
- [ ] Parse `name@plotId:value` format for multi-graph assignment
- [ ] Dynamic plot creation (sparse IDs: 1,3,11 → 3 plots)
- [ ] Dark theme for plots (dark background, bright data colors)
- [ ] Passive hover crosshair (grey dotted lines + readout panel)
- [ ] Trigger Mode cursor system (neon yellow/blue intercept lines)
- [ ] Test with real Teensy hardware (serial + IMU plots)

## Backlog (Post-v0.1)

### Enhanced Plotter — Decided Features (ready to implement)

- [ ] Scaling controls — [+] [-] both axes, independent X/Y, auto-fit toggle
- [ ] Pause/freeze mode — stop collecting by default, "Keep recording" toggle
- [ ] Separate clear buttons — plots vs serial monitor
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

### Serial Features

- [ ] Auto-reconnect on disconnect/unplug
- [ ] Raw data logging to file (timestamped)
- [ ] Copy terminal output to clipboard
- [ ] Search/filter terminal output

### PlatformIO Integration

- [ ] Detect PlatformIO installation
- [ ] Compile firmware (`pio run`)
- [ ] Flash firmware (`pio run --target upload`)
- [ ] Build output display

### Calibration Interface

- [ ] Parse calibration values from serial
- [ ] Display current calibration values
- [ ] Generate config.h snippet
- [ ] Save/load calibration profiles

### Platform Validation

- [ ] Validate Windows dev_setup scripts on real Windows machine
- [ ] Validate macOS dev_setup scripts on real macOS machine

### Nice to Have (Low Priority)

- [ ] 3D attitude visualization (WebGL orientation cube)
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism

## Recently Completed

- [x] Plotter discussion — major progress on feature design — 2026-02-07
  - Created cursor-interaction-discussion.md (full cursor/interaction system design)
  - Created signal-analysis-discussion.md (period mode + anomaly detection)
  - Research completed: signal period detection, anomaly detection, Chart.js visualization
  - 5 research documents produced in findings/
  - Decisions: trigger mode vs period mode, scaling controls, pause behavior, visual style
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

## Research Documents (for next session)

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

---

*Update every session: start by reading, end by updating.*
