# Flight Controller Firmware - Todo

> Last updated: 2026-02-12

## In Progress

_Focus: Get everything working on real hardware. Feature development is paused — ~90% of target features are implemented._

- [ ] Bench test: IMU sensor validation — **Working.** MPU6050 reads data. AccZ=-0.08g (mounted sideways). Run orientation detection (`o`) next.
- [ ] Bench test: SBUS receiver communication — **Pending.** SBUS configured but receiver not connected during testing.
- [ ] Bench test: Motor/ESC response — **Pending.** Hardware assembled, not yet tested.

## Up Next

_Priority queue for immediate work_

### Immediate: Test Infrastructure (High Priority)

- [ ] Modular test runner — refactor `test_calibration.sh` into harness + suites (see roadmap)
- [ ] ESP32 test support — board detection, RTS/DTR reset, pyserial-compatible reads
- [ ] Auto-flash-and-test — `./test_runner.sh --board teensy40 --suite imu --flash`

### Immediate: Hardware Validation

- [ ] Run orientation detection (`o` command) — auto-detect MPU6050 mounting, generate axis transforms
- [ ] Complete IMU calibration with orientation correction — verify corrected AccZ ≈ 1.0g
- [ ] Complete radio calibration — full channel mapping when transmitter available
- [ ] Complete motor/ESC bench test — verify PWM output, ESC calibration routine

### Next: Tuning & Flight

- [ ] PID tuning on real hardware — use `g` command in calibration mode
- [ ] First hover test — tethered/constrained flight, validate stability
- [ ] OLED context-aware calibration display — progress indicator per step

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Low battery voltage monitoring (ADC)
- [ ] fc_tool WebSocket integration (connect to floppi.local/ws) — deferred, fc_tool works fine over serial

## Blocked

_Tasks waiting on something (include reason)_

_No blocked tasks_

## Recently Completed

_For context; clear periodically_

- [x] Calibration library modularization — 2026-02-10
  - calibration.cpp (1753 lines) split into 5 focused files in lib/Calibration/:
    - calibration.cpp (287 lines): core helpers, CalibrationResults, dump command
    - calibration_imu.cpp (444 lines): IMU offset + 6-position calibration
    - calibration_orientation.cpp (285 lines): orientation auto-detection
    - calibration_radio.cpp (375 lines): radio channel mapping
    - calibration_hardware.cpp (347 lines): failsafe, ESC, magnetometer
  - main.cpp (1091→490 lines): extracted calibration mode into calibration_mode.cpp (614 lines) + calibration_mode.h
  - CalibrationResults struct moved to calibration.h (shared across all files)
  - Both teensy40 and teensy40_calibration builds verified (identical binary sizes)
- [x] Calibration value dump command (`d`) — 2026-02-10
  - Serial command `d` prints ALL calibration values from current session in one config.h-ready block
  - CalibrationResults accumulator stores results as each calibration completes
  - Sections: IMU offsets/scales, orientation, radio mapping, failsafe, magnetometer, PID gains, filters/limits
  - Only prints sections that were actually calibrated (skips uncalibrated)
  - Sequential workflow (`a`) now suggests typing `d` at completion
- [x] Modular WiFi configuration — 2026-02-11
  - WPA2-Personal, Open, WPA2-Enterprise PEAP, WPA2-Enterprise TLS all supported
  - wifi_credentials.h: configurable auth method (WPA2_AUTH_PEAP / WPA2_AUTH_TLS)
  - wifi_certs.h: optional PEM certificate storage for enterprise networks
  - Network diagnostics command (`n`) in calibration mode for ESP32 WiFi testing
  - See [features/wifi-configuration.md](features/wifi-configuration.md)
- [x] Firmware telemetry multi-graph format — 2026-02-11
  - debug.cpp telemetry functions output `name@plotId:value` for fc_tool multi-graph plotting
  - Plot assignment: 0=Accel, 1=Gyro, 2=Attitude, 3=Motors
  - Backward compatible — fc_tool still handles plain `name:value` and CSV formats
- [x] Command source arbitration implemented (`USE_COMMAND_ARBITRATION`) — 2026-02-11
  - CommandBuffer struct + CommandSource enum in radioComm.h
  - Priority: Serial > I2C > WiFi (overrides), RC (primary fallback)
  - Compile-time validation (max 1 RC protocol, multi-source requires arbitration flag)
  - Zero overhead for single-source builds (identical behavior when arbitration disabled)
- [x] RadioComm library modularization — 2026-02-11
  - radioComm.cpp (663 lines) split into 3 focused files:
    - radioComm.cpp (core): channels, dispatch, arbitration, failsafe (~200 lines)
    - radioComm_rc.cpp: RC protocols (SBUS, iBUS, DSM, PPM, PWM) (~280 lines)
    - radioComm_ext.cpp: external sources (serial, I2C, WiFi) (~180 lines)
  - Each protocol has setup() + read() function pair
  - Per-source CommandBuffer with timeout-based activity tracking
- [x] OLED startup build info + no-delay — 2026-02-11
  - Startup screen shows "FLOPPI FC" + build info (e.g., "T40 SBUS CAL", "ESP32 WiFi")
  - Compile-time BUILD_INFO_STR from platform + receiver + mode tags
  - No delay() — startup message stays on screen naturally until renderDisplay() overwrites
- [x] OLED I2C address auto-detection and calibration input improvements — 2026-02-11
  - Software I2C scan for OLED address (0x3C/0x3D) with auto-selection
  - waitForConfirmation() now blocks indefinitely (no timeout) with LED blink
  - CH6 calibration trigger debounce (must go LOW before re-trigger)
- [x] Hardware bench testing started — 2026-02-10
  - Firmware built and flashed successfully (teensy40_calibration)
  - Serial connection working (/dev/ttyACM0 at 115200)
  - IMU (MPU6050) initialized OK, 6-position calibration started
  - SBUS receiver (X8R on Serial5/Pin 21) initialized OK
  - OLED (DSD TECH 0.91" SSD1306 128x32) diagnosed: SDA/SCL pin swap fixed (SDA=16, SCL=17)
  - setup_permissions.sh created at repo root (udev rules + user groups)
- [x] I2C command input (`USE_I2C_COMMANDS`) — 2026-02-10
  - FC as I2C slave on Wire1 (0x42), master writes 12 bytes (6x uint16 LE)
  - Separate from IMU bus (Wire). ISR-based receive, noInterrupts for read.
- [x] Command source arbitration design doc — 2026-02-10
  - Design doc: [findings/command-arbitration-design.md](findings/command-arbitration-design.md)
  - USE_COMMAND_ARBITRATION flag, CommandBuffer struct, priority logic
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
- **RadioComm universal command layer** — all command sources implemented (SBUS, iBUS, DSM, PPM, PWM, serial, I2C, WiFi) + command source arbitration (USE_COMMAND_ARBITRATION). RadioComm modularized: radioComm.cpp (core), radioComm_rc.cpp (RC protocols), radioComm_ext.cpp (external sources).
- **Hardware testing is in progress** — firmware flashed, IMU and receiver validated, motor/ESC testing next
- **fc_tool will help** — visual diagnostics during calibration (separate project at /fc_tool/)
- **Modular architecture** — code split into imu, control, motors, debug modules + feature flags. Calibration library split into 5 focused files. RadioComm split into 3 focused files. Calibration mode extracted from main.cpp.
- **Platform support**: Teensy 4.x (recommended), ESP32/S3 (WiFi-enabled)
- **NOT supported**: Arduino Uno/Mega (16MHz + no FPU = max 302Hz loop rate)
- **Feature modularity**: Users enable features in config.h based on their MCU capabilities. Use `python3 tools/complexity_calculator.py --all` to check feasibility across platforms.
- **Feature tiers fully implemented**: Base (always), USE_OPTIMIZATION (biquad/notch), USE_RACING (FF/TPA/expo/air mode). All compile-tested in all combinations.

---

*Update every session: start by reading, end by updating.*
