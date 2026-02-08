# Flight Controller Firmware - Todo

> Last updated: 2026-02-07

## In Progress

_No tasks in progress_

## Recently Implemented (2026-02-07)

- [x] Modular feature system
  - All features selectable via `#define` flags in config.h
  - USE_WEB_SERVER: live value display in browser (calibration/diagnostics)
  - USE_API_SERVER: HTTP POST telemetry for remote control/swarm
  - USE_OPTIMIZATION: noise reduction filters for cheap hardware (placeholder params in config.h)
  - USE_RACING: Betaflight-style performance features (placeholder params in config.h)
  - Web server and API server split from monolithic USE_WIFI into separate toggles
  - Config.h has parameter sections for each tier (only compiled when enabled)
  - See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md) for feature tier research

- [x] Library vendoring for standalone builds
  - All external PlatformIO libraries vendored into project
  - Shared libs (U8g2, ArduinoJson) in lib/
  - ESP32-only libs (AsyncTCP, ESPAsyncWebServer) in lib_esp32/
  - platformio.ini updated: no external downloads, lib_extra_dirs for ESP32
  - Project builds offline without internet connection

- [x] Timing calculator modularization
  - Split from single 1,074-line file into `tools/timing/` package (platforms, operations, calc, scanner, report)
  - Auto-detects enabled features from config.h (scanner module)
  - Calculates minimum clock speed required and recommended (+10% margin)
  - Pass/fail output: "NOT ENOUGH +X%" when clock < required
  - Clean tabular output, no prose/insights
  - CLI: `-p <platform>`, `--all`, `--breakdown`, `--clock N`
  - See [tools/timing/](../tools/timing/) and roadmap "Timing Calculator Improvements" for future plans

- [x] Timing calculator simplified inputs
  - Platform reduced to clock/cores/FPU (removed per-platform cycle costs, flash/RAM specs)
  - Two FPU profiles: hardware FPU vs software float cycle costs
  - CLI flags: `--cores N`, `--fpu`/`--no-fpu` for arbitrary hardware input
  - Always outputs min/recommended clock speeds and pass/fail status
  - Platform presets remain as convenience shortcuts

- [x] Display module abstraction layer
  - Created display.h, display_data.h, display.cpp with U8g2 + SW I2C
  - Compile-time display selection (SSD1306 128x32, 128x64, SH1106 128x64)
  - Producer-consumer: DisplayData_t struct filled by flight control, rendered by display module
  - Screens: startup, calibrating, idle, armed, network info
  - See [findings/display-module-architecture.md](findings/display-module-architecture.md)

- [x] ESP32 dual-core architecture
  - Core 0: flight control (FreeRTOS task, priority 3, real-time)
  - Core 1: display + WiFi (Arduino loop)
  - Queue-based data transfer: xQueueOverwrite for Core 0 → Core 1
  - Startup feedback on OLED during boot sequence

- [x] WiFi STA mode (connect to existing WiFi)
  - Architecture: drones connect to existing infrastructure WiFi, NOT create own AP
  - Vision: swarm of drones on same network, API POST/GET to centralized computers
  - Supports WPA2-Personal and WPA2-Enterprise (eduroam via PEAP)
  - Credentials in include/wifi_credentials.h (edit directly, tracked with placeholder values)
  - Non-blocking connection with 15s timeout, background reconnection every 5s
  - AP mode code archived to [docs/archive/wifi_ap_mode_implementation.md](archive/wifi_ap_mode_implementation.md)

- [x] ESP32 network info display on OLED
  - Shows MAC, SSID, IP address, RSSI on network screen
  - Connection status feedback ("WiFi: Starting..." -> connected info)

- [x] Web status server (ESPAsyncWebServer)
  - JSON endpoint: GET /api/status (full telemetry)
  - WebSocket: /ws (real-time streaming at 10Hz)
  - mDNS: http://floppi-XXXX.local
  - Runs on Core 1, async callbacks
  - Now gated by USE_WEB_SERVER (independently disableable)

- [x] API client for swarm coordination
  - HTTP POST telemetry to configurable centralized server
  - Server URL configured via API_SERVER_URL in wifi_credentials.h
  - Non-blocking on Core 1, 2Hz default rate
  - Now gated by USE_API_SERVER (independently disableable)

- [x] Bare-bones FC features research
  - Comprehensive research on what a bare-bones FC needs vs doesn't need
  - Feature tier design: Base / USE_OPTIMIZATION / USE_RACING
  - See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)

## Research Completed

- [x] ESP32 dual-core architecture research — 2026-02-06
  - See [findings/esp32-dual-core-research.md](findings/esp32-dual-core-research.md)
- [x] FC timing requirements research — 2026-02-06
  - See [findings/fc-timing-requirements.md](findings/fc-timing-requirements.md)
- [x] ESP32 FC feasibility analysis — 2026-02-06
  - See [findings/esp32-fc-feasibility.md](findings/esp32-fc-feasibility.md)
- [x] OLED display options documented — 2026-02-06
  - See [findings/oled-display-options.md](findings/oled-display-options.md)
- [x] Timing calculator tool — 2026-02-06, modularized 2026-02-07
  - See [tools/timing/](../tools/timing/)
  - Usage: `python3 tools/timing_calculator.py` (default), `-p teensy40`, `--all`, `--breakdown`, `--clock N`
- [x] Bare-bones FC features & algorithms research — 2026-02-07
  - See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)
- [x] ESP32 WiFi connectivity research — 2026-02-07
  - See [findings/esp32-wifi-connectivity.md](findings/esp32-wifi-connectivity.md)

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Up Next

_Priority queue for immediate work_

- [ ] D-term low-pass filter (highest-impact PID improvement, prevents motor oscillation)
  - See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)
- [ ] Rate mode derivative on measurement (prevents derivative kick)
- [ ] Implement USE_OPTIMIZATION features (biquad filters, notch filter)
- [ ] Implement USE_RACING features (feed-forward, TPA, expo, air mode)
- [ ] Hardware testing when hardware is available
- [ ] OTA firmware updates
- [ ] fc_tool WebSocket integration (connect to floppi.local/ws)

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Serial command interface for PID tuning in calibration mode
- [ ] Low battery voltage monitoring (ADC)

## Recently Completed

_For context; clear periodically_

- [x] Modular feature system (config.h flags) — 2026-02-07
- [x] Library vendoring (standalone builds) — 2026-02-07
- [x] Timing calculator modularized (scanner, min clock, clean output) — 2026-02-07
- [x] Timing calculator simplified (clock/cores/FPU input, auto min/recommended output) — 2026-02-07
- [x] Display module, dual-core, WiFi STA, web server, API client — 2026-02-07
- [x] 6-position accelerometer calibration — 2026-02-06
- [x] Modularize main.cpp — 2026-02-06
- [x] Serial command interface for calibration — 2026-02-06
- [x] Build target separation implemented — 2026-02-05

---

## Notes

- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **fc_tool will help** — visual diagnostics during calibration (separate project at /fc_tool/)
- **Modular architecture** — code split into imu, control, motors, debug modules + feature flags
- **Platform support**: Teensy 4.x (recommended), ESP32/S3 (WiFi-enabled)
- **NOT supported**: Arduino Uno/Mega (16MHz + no FPU = max 302Hz loop rate)
- **Feature modularity**: Users enable features in config.h based on their MCU capabilities. Use `python3 tools/timing_calculator.py --all` to check feasibility across platforms.
- **Timing calculator**: Simplified inputs (clock/cores/FPU) implemented. Source code scanning planned. See roadmap "Timing Calculator Improvements" section. Usage: `python3 tools/timing_calculator.py --clock 240 --cores 2 --fpu` or `-p esp32`.

---

*Update every session: start by reading, end by updating.*
