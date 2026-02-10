# Flight Controller Firmware - Todo

> Last updated: 2026-02-10

## In Progress

_No tasks in progress_

## Up Next

_Priority queue for immediate work_

- [ ] RadioComm universal command layer — I2C commands (`USE_I2C_COMMANDS`) remaining
- [ ] Command source arbitration — priority logic when multiple command sources are active (RC = primary, serial/I2C/WiFi = override)
- [ ] OLED-guided calibration — show calibration prompts and progress on display
- [ ] Hardware testing when hardware is available — follow [calibration-guide.md](features/calibration-guide.md) stages
- [ ] fc_tool WebSocket integration (connect to floppi.local/ws) — **deferred**, fc_tool still in development

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Low battery voltage monitoring (ADC)

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Recently Completed

_For context; clear periodically_

- [x] WiFi API command routing — 2026-02-10
  - POST /api/commands + WebSocket /ws on ESP32 web server
  - Spinlock-protected cross-core buffer (Core 1 web server → Core 0 flight control)
  - Works as sole command source when no receiver defined (ESP32 + Web API path)
- [x] Serial command input (`USE_SERIAL_COMMANDS`) — 2026-02-10
  - Binary protocol: 15-byte frames (2 header + 12 channel data + 1 XOR checksum)
  - 115200 baud, 8N1. External flight computer sends channel values over UART.
- [x] iBUS receiver support (`USE_IBUS_RECEIVER`) — 2026-02-10
  - Inline parser, 115200 baud, 8N1, 14 channels, checksum validation
  - Voltage divider required (5V → 3.3V)
- [x] Wiring diagrams reorganized — 2026-02-10
  - `docs/wiring_diagrams/` with build-specific guides (teensy/esp32 × receiver × vtol)
  - Hardware architecture vision documented in scope.md
- [x] Configurable pin definitions from config.h — 2026-02-10
  - All pins in pin_definitions.h/pin_definitions_esp32.h now use `#ifndef` guards
  - Config.h has PIN OVERRIDES section for user customization
- [x] Calibration guide with hardware requirements — 2026-02-10
  - Stage-based setup: MCU-only → +IMU → +receiver → +ESCs → full drone
  - Feature tier calibration requirements (base/optimization/racing)
  - See [features/calibration-guide.md](features/calibration-guide.md)
- [x] Calibration reset tool — 2026-02-10
  - `python3 tools/calibration_reset.py` resets all calibration values to defaults
  - Dry-run, confirmation, custom config path options
- [x] Sequential calibration workflow (`a` command) — 2026-02-10
  - Guided stage-by-stage workflow, skips already-calibrated stages
  - `c` command shows calibration status checklist
  - CALIBRATED_* markers in config.h track completion per stage
  - Reset tool re-comments markers on reset
- [x] Config.h calibration status markers — 2026-02-10
  - CALIBRATED_* #define markers (uncomment after each stage)
  - STATUS: UNCALIBRATED comments with stage references and serial commands
- [x] Failsafe auto-detection (`f` command) — 2026-02-09
  - Measures receiver failsafe PWM outputs (TX off), outputs FAILSAFE_* defines
- [x] ESC endpoint calibration (`e` command) — 2026-02-09
  - Guided min/max PWM routine for ESC range calibration
- [x] Runtime filter/limits tuning (`p` command) — 2026-02-09
  - Serial tuning for B_ACCEL, B_GYRO, B_DTERM, MADGWICK_BETA, max rates/angles
- [x] Magnetometer calibration (MPU9250, sphere calibration) — 2026-02-09
  - 30s rotation sampling, hard-iron offsets + soft-iron scale factors
- [x] Wiring diagrams updated with OLED display connections — 2026-02-09
  - Both Teensy and ESP32 wiring docs now include OLED display in mermaid diagrams and checklists
- [x] Serial PID tuning in calibration mode — 2026-02-09
  - Runtime-tunable PID gains (9 variables: kp/ki/kd × roll/pitch/yaw)
  - Non-blocking line-buffered serial parser (no flight loop impact)
  - Commands: `g` (show gains), `g kp_roll 0.2` (set gain)
  - Gains initialized from config.h macros, adjust live, copy back when done
- [x] OTA firmware updates (ArduinoOTA, Core 1, safety-gated) — 2026-02-09
  - ota.h/cpp: ArduinoOTA on Core 1, only processes when disarmed
  - Auto-enabled with USE_WIFI, individually disableable via USE_OTA in config.h
  - Upload: `pio run -t upload --upload-port floppi-XXXX.local`
- [x] USE_OPTIMIZATION features (biquad gyro LP, biquad D-term, gyro notch, accel 2nd stage LP) — 2026-02-09
  - filters.h/cpp: Biquad filter module (Butterworth LPF + notch, Direct Form II Transposed)
  - imu.cpp: Biquad LP on gyro (2nd stage after PT1), notch filter (configurable center/width), accel 2nd stage PT1
  - control.cpp: Biquad replaces PT1 for D-term filtering when USE_OPTIMIZATION enabled
  - Config params: GYRO_LPF_CUTOFF_HZ, DTERM_LPF_CUTOFF_HZ, GYRO_NOTCH_CENTER_HZ, GYRO_NOTCH_WIDTH_HZ, B_ACCEL_STAGE2
- [x] USE_RACING features (feed-forward, TPA, expo, air mode, setpoint smoothing) — 2026-02-09
  - getDesState(): Expo curves (cubic blend), setpoint smoothing (PT1 with configurable cutoff)
  - controlRATE()/controlANGLE(): Feed-forward on setpoint derivative, TPA from breakpoint
  - controlMixer(): Air mode shifts motor outputs to preserve PID at zero throttle
  - Config params: FF_ROLL/PITCH/YAW, TPA_BREAKPOINT, TPA_RATE, SETPOINT_SMOOTH_CUTOFF_HZ, EXPO_ROLL/PITCH/YAW, USE_AIRMODE
- [x] Build scripts dynamic environments — 2026-02-09
  - build.bat: Complete rewrite — dynamically parses [env:xxx] from platformio.ini
  - build.sh: Already dynamic (no changes needed)
  - Both support: build/upload/clean any env, interactive menu, CLI arguments
- [x] PID improvements (D-term LP filter + derivative on measurement) — 2026-02-07
- [x] build.bat replacing build.ps1.txt — 2026-02-07
- [x] Modular feature system (config.h flags) — 2026-02-07
- [x] Library vendoring (standalone builds) — 2026-02-07
- [x] Timing calculator modularized + simplified — 2026-02-07
- [x] Display module, dual-core, WiFi STA, web server, API client — 2026-02-07

## Research Completed

- [x] ESP32 dual-core architecture research — 2026-02-06
  - See [findings/esp32-dual-core-research.md](findings/esp32-dual-core-research.md)
- [x] FC timing requirements research — 2026-02-06
  - See [findings/fc-timing-requirements.md](findings/fc-timing-requirements.md)
- [x] ESP32 FC feasibility analysis — 2026-02-06
  - See [findings/esp32-fc-feasibility.md](findings/esp32-fc-feasibility.md)
- [x] OLED display options documented — 2026-02-06
  - See [findings/oled-display-options.md](findings/oled-display-options.md)
- [x] Timing calculator tool — 2026-02-06, replaced by complexity_calculator 2026-02-10
  - See [tools/complexity/](../tools/complexity/)
- [x] Bare-bones FC features & algorithms research — 2026-02-07
  - See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)
- [x] ESP32 WiFi connectivity research — 2026-02-07
  - See [findings/esp32-wifi-connectivity.md](findings/esp32-wifi-connectivity.md)

---

## Notes

- **Calibration automation complete** — all config.h hardware-dependent values now have auto-calibration routines
- **RadioComm universal command layer** — serial commands and WiFi API routing done. Remaining: I2C commands, command source arbitration. See roadmap for details.
- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **fc_tool will help** — visual diagnostics during calibration (separate project at /fc_tool/)
- **Modular architecture** — code split into imu, control, motors, debug modules + feature flags
- **Platform support**: Teensy 4.x (recommended), ESP32/S3 (WiFi-enabled)
- **NOT supported**: Arduino Uno/Mega (16MHz + no FPU = max 302Hz loop rate)
- **Feature modularity**: Users enable features in config.h based on their MCU capabilities. Use `python3 tools/complexity_calculator.py --all` to check feasibility across platforms.
- **Feature tiers fully implemented**: Base (always), USE_OPTIMIZATION (biquad/notch), USE_RACING (FF/TPA/expo/air mode). All compile-tested in all combinations.

---

*Update every session: start by reading, end by updating.*
