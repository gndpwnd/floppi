# Flight Controller Firmware - Todo

> Last updated: 2026-02-09

## In Progress

_No tasks in progress_

## Up Next

_Priority queue for immediate work_

- [ ] Configurable pin definitions from config.h — move pin overrides so users configure everything in one file (**top priority**)
- [ ] RadioComm universal command layer — serial commands (`USE_SERIAL_COMMANDS`), I2C commands (`USE_I2C_COMMANDS`), WiFi API command routing through RadioComm
- [ ] Command source arbitration — priority logic when multiple command sources are active (RC = primary, serial/I2C/WiFi = override)
- [ ] OLED-guided calibration — show calibration prompts and progress on display
- [ ] Sequential calibration workflow (`a` command) — run all calibration routines in one guided session
- [ ] Hardware testing when hardware is available
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
- [x] Timing calculator tool — 2026-02-06, modularized 2026-02-07
  - See [tools/timing/](../tools/timing/)
- [x] Bare-bones FC features & algorithms research — 2026-02-07
  - See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)
- [x] ESP32 WiFi connectivity research — 2026-02-07
  - See [findings/esp32-wifi-connectivity.md](findings/esp32-wifi-connectivity.md)

---

## Notes

- **Calibration automation complete** — all config.h hardware-dependent values now have auto-calibration routines
- **RadioComm universal command layer** — next architectural milestone. All command sources (RC, serial, I2C, WiFi) should flow through RadioComm. See roadmap for details.
- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **fc_tool will help** — visual diagnostics during calibration (separate project at /fc_tool/)
- **Modular architecture** — code split into imu, control, motors, debug modules + feature flags
- **Platform support**: Teensy 4.x (recommended), ESP32/S3 (WiFi-enabled)
- **NOT supported**: Arduino Uno/Mega (16MHz + no FPU = max 302Hz loop rate)
- **Feature modularity**: Users enable features in config.h based on their MCU capabilities. Use `python3 tools/timing_calculator.py --all` to check feasibility across platforms.
- **Feature tiers fully implemented**: Base (always), USE_OPTIMIZATION (biquad/notch), USE_RACING (FF/TPA/expo/air mode). All compile-tested in all combinations.

---

*Update every session: start by reading, end by updating.*
