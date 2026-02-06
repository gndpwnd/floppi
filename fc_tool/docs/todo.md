# fc_tool - Todo

> Last updated: 2026-02-06

## In Progress

- [ ] Test serial monitor with hardware (verify connect/send/receive works)

## Blocked

- [ ] Define serial telemetry protocol — **Blocked by**: flight_controller firmware needs a structured output format (currently uses ad-hoc Serial.print() debug output). Can start with raw serial display in the meantime.

## Up Next

- [ ] Test with real Teensy hardware (serial + IMU plots)
- [ ] Validate Windows build on real Windows machine

## Backlog (Post-v0.1)

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

- Serial telemetry protocol TBD — start with raw serial monitor, add structured parsing later
- PlatformIO is optional; fc_tool must work standalone for serial monitoring
- The flight_controller project has a calibration workflow (calibrate → hard-code → flash → fly) — fc_tool should support this
- See [flight_controller/docs/findings/auto-calibration-research.md](/flight_controller/docs/findings/auto-calibration-research.md) for calibration approach
- Windows/macOS scripts marked with `# TODO: UNTESTED` until validated on real hardware
- Teensy 4.x USB VID:PID is 16c0:0483 (when using USB Serial)

---

*Update every session: start by reading, end by updating.*
