# fc_tool - Todo

> Last updated: 2026-01-27

## In Progress

_None_

## Blocked

_None_

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
- [ ] Calibration parameter display
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
- [x] Windows PowerShell scripts written (UNTESTED) — 2026-01-27
- [x] macOS bash scripts written (UNTESTED) — 2026-01-27
- [x] Reorganized scripts into dev_setup/linux/, dev_setup/windows/, dev_setup/macos/ — 2026-01-27
- [x] GitHub Actions workflow (manual trigger, multi-platform release) — 2026-01-27
- [x] PlatformIO install added to all setup-dev scripts (get-platformio.py) — 2026-01-27
- [x] Install conventions documented (default home dir paths, env vars, validation status) — 2026-01-27
- [x] Linux release build verified (13 MB binary, .deb, .rpm) — 2026-01-27

---

## Notes

- Serial telemetry protocol TBD — start with raw serial monitor, add structured parsing later
- PlatformIO is optional; fc_tool must work standalone for serial monitoring
- Cross-platform builds via GitHub Actions CI (cannot cross-compile Tauri locally)
- Windows and macOS are primary end-user platforms
- All tools install to default home directory locations — no custom paths
- Windows/macOS scripts marked with `# TODO: UNTESTED` until validated on real hardware

---

*Update every session: start by reading, end by updating.*
