# fc_tool - Todo

> Last updated: 2026-02-06

## In Progress

- [ ] Review [plotter_discussion.md](plotter_discussion.md) and finalize feature decisions (NEXT SESSION)

## Up Next

- [ ] Implement enhanced serial plotter with dynamic multi-graph support
- [ ] Toggle between Monitor and Plotter views (button to show/hide plotter)
- [ ] Parse `name@plotId:value` format for multi-graph assignment
- [ ] Dynamic plot creation (sparse IDs: 1,3,11 → 3 plots)
- [ ] Test with real Teensy hardware (serial + IMU plots)
- [ ] Validate Windows build on real Windows machine

## Backlog (Post-v0.1)

### Enhanced Plotter Features

- [ ] Plot Y-axis auto-scaling per plot
- [ ] Plot labels and legends
- [ ] Color assignment per variable
- [ ] Configurable time window per plot

### Serial Features

- [ ] Auto-reconnect on disconnect/unplug
- [ ] Raw data logging to file (timestamped)
- [ ] Copy terminal output to clipboard
- [ ] Search/filter terminal output
- [ ] Multiple baud rate presets for different firmware modes

### IMU Features

- [ ] Magnetometer plot (when available)
- [ ] Data export to CSV
- [ ] Pause/resume plotting
- [ ] Configurable sample rate display
- [ ] Statistics panel (min/max/avg)

### PlatformIO Integration

- [ ] Detect PlatformIO installation (check `pio` in PATH)
- [ ] Show/hide PlatformIO section based on availability
- [ ] Compile firmware (`pio run` integration)
- [ ] Flash firmware (`pio run --target upload`)
- [ ] Build output display in GUI
- [ ] Environment selector (teensy41, esp32, etc.)

### Calibration Interface

- [ ] Parse calibration values from serial stream
- [ ] Display current calibration values
- [ ] Calibration quality indicator
- [ ] Generate config.h snippet (copy-pasteable)
- [ ] Save/load calibration profiles

### Board Management

- [ ] Teensy 4.0/4.1 detection by USB VID/PID (16c0:0483)
- [ ] ESP32 detection (future)
- [ ] Board info display (type, port, firmware version if available)
- [ ] Auto-select correct baud rate per board type

### Platform Validation

- [ ] Validate Windows dev_setup scripts on real Windows machine
- [ ] Validate macOS dev_setup scripts on real macOS machine
- [ ] Remove UNTESTED markers after validation

### Nice to Have (Low Priority)

- [ ] 3D attitude visualization (WebGL orientation cube)
- [ ] Dark/light theme toggle
- [ ] Multiple simultaneous serial connections
- [ ] Plugin system for custom telemetry parsers
- [ ] Auto-update mechanism

## Recently Completed

- [x] Serial telemetry protocol defined and implemented — 2026-02-06
  - Protocol documented in [features/serial-telemetry-protocol.md](features/serial-telemetry-protocol.md)
  - Firmware support added: `t` command toggles telemetry mode (off/IMU/full)
  - Format: `ax=X ay=Y az=Z gx=X gy=Y gz=Z` (key-value, fc_tool parser compatible)
- [x] Board identification by VID/PID (Teensy, Arduino, ESP32) — 2026-02-06
- [x] Serial monitor tested with Teensy hardware — 2026-02-06
- [x] Board VID/PID reference document created — 2026-02-06
- [x] IMU visualization with Chart.js (accelerometer + gyroscope plots) — 2026-02-06
- [x] Telemetry parser (JSON, key-value, CSV formats supported) — 2026-02-06
- [x] Real-time scrolling charts with 100-sample window — 2026-02-06
- [x] Build serial monitor release binary (compiles clean) — 2026-02-06
- [x] Serial monitor UI implemented (port selector, baud selector, terminal, send/receive) — 2026-02-06
- [x] Serial connection backend (open, close, send, events via Tauri) — 2026-02-06
- [x] Linux system dependencies installed (libwebkit2gtk, etc.) — 2026-02-06
- [x] Project scope defined (architecture boundary, cross-platform strategy) — 2026-01-27
- [x] Roadmap created with goal horizons — 2026-01-27
- [x] Rust + Tauri project initialized — 2026-01-27
- [x] Serial port listing command (serialport-rs) — 2026-01-27
- [x] Frontend scaffolding (index.html, main.js, styles.css) — 2026-01-27
- [x] Linux build scripts validated — 2026-01-27
- [x] Windows/macOS scripts written (UNTESTED) — 2026-01-27
- [x] Linux release build verified (13 MB binary, .deb, .rpm) — 2026-01-27
- [x] Project status check and docs update — 2026-02-05
- [x] Created docs/features/, docs/findings/, docs/archive/ directories — 2026-02-05

---

## Notes

- Serial telemetry protocol DONE — firmware supports `t` command for fc_tool-compatible output
- PlatformIO is optional; fc_tool must work standalone for serial monitoring
- The flight_controller project has a calibration workflow (calibrate → hard-code → flash → fly) — fc_tool should support this
- See [flight_controller/docs/findings/auto-calibration-research.md](/flight_controller/docs/findings/auto-calibration-research.md) for calibration approach
- Windows/macOS scripts marked with `# TODO: UNTESTED` until validated on real hardware
- Teensy 4.x USB VID:PID is 16c0:0483 (when using USB Serial)

---

*Update every session: start by reading, end by updating.*
