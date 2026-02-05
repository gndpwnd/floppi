# fc_tool - Todo

> Last updated: 2026-02-05

## In Progress

_None_

## Blocked

- [ ] Define serial telemetry protocol — **Blocked by**: flight_controller firmware needs a structured output format (currently uses ad-hoc Serial.print() debug output). Can start with raw serial display in the meantime.

## Up Next

- [ ] Create serial monitor UI (connect to port, send commands, receive output)
- [ ] Define initial telemetry parsing format (or start with raw serial display)
- [ ] IMU real-time plotting (Chart.js or Plotly integration)
- [ ] Install xdg-utils on dev machine (fixes AppImage bundling error)

## Backlog

- [ ] PlatformIO CLI detection in fc_tool GUI (check if `pio` is on PATH, enable/disable compile/flash)
- [ ] PlatformIO compile integration (call `pio run` from fc_tool)
- [ ] PlatformIO flash integration (call `pio run --target upload`)
- [ ] Teensy board detection by USB VID/PID
- [ ] Calibration parameter display (read from serial stream)
- [ ] Generate config.h snippet from calibration values (supports calibrate → hard-code → flash workflow)
- [ ] Data logging / CSV export
- [ ] Validate Windows dev_setup scripts on a real Windows machine
- [ ] Validate macOS dev_setup scripts on a real macOS machine
- [ ] Test GitHub Actions CI workflow (first real push)

## Recently Completed

- [x] Project scope defined (architecture boundary, cross-platform strategy) — 2026-01-27
- [x] Roadmap created with goal horizons — 2026-01-27
- [x] Rust + Tauri project initialized — 2026-01-27
- [x] Serial port listing command (serialport-rs) — 2026-01-27
- [x] Frontend scaffolding (index.html, main.js, styles.css) — 2026-01-27
- [x] Linux build scripts validated — 2026-01-27
- [x] Windows/macOS scripts written (UNTESTED) — 2026-01-27
- [x] GitHub Actions workflow (manual trigger, multi-platform release) — 2026-01-27
- [x] Linux release build verified (13 MB binary, .deb, .rpm) — 2026-01-27
- [x] Project status check and docs update (aligned with flight_controller bootstrap) — 2026-02-05
- [x] Created docs/features/, docs/findings/, docs/archive/ directories — 2026-02-05

---

## Notes

- Serial telemetry protocol TBD — start with raw serial monitor, add structured parsing later
- PlatformIO is optional; fc_tool must work standalone for serial monitoring
- The flight_controller project now has a defined calibration workflow (calibrate → hard-code → flash → fly) — fc_tool should support this by making calibration value export easy
- See [flight_controller/docs/findings/auto-calibration-research.md](/flight_controller/docs/findings/auto-calibration-research.md) for calibration approach research
- Windows/macOS scripts marked with `# TODO: UNTESTED` until validated on real hardware

---

*Update every session: start by reading, end by updating.*
