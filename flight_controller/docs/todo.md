# Flight Controller Firmware - Todo

> Last updated: 2026-02-06

## In Progress

_No tasks in progress_

## Research Completed

- [x] ESP32 dual-core architecture research — 2026-02-06
  - See [findings/esp32-dual-core-research.md](findings/esp32-dual-core-research.md)
- [x] FC timing requirements research — 2026-02-06
  - See [findings/fc-timing-requirements.md](findings/fc-timing-requirements.md)
- [x] ESP32 FC feasibility analysis — 2026-02-06
  - See [findings/esp32-fc-feasibility.md](findings/esp32-fc-feasibility.md)
- [x] OLED display options documented — 2026-02-06
  - See [findings/oled-display-options.md](findings/oled-display-options.md)
- [x] Timing calculator tool created — 2026-02-06
  - See [tools/timing_calculator.py](../tools/timing_calculator.py)
  - See [findings/timing-calculator-analysis.md](findings/timing-calculator-analysis.md)
  - Interactive mode: `python3 tools/timing_calculator.py --check`
  - Supports clock speed input with k/m/g suffixes (e.g., 600m, 1G, 16m)
  - Shows CPU utilization % and FPU vs non-FPU comparison
  - Arduino Uno proven NOT FEASIBLE (max 302 Hz, no FPU)
- [x] ESP32 port implemented — 2026-02-06
  - Added esp32, esp32_calibration, esp32s3, esp32s3_calibration environments
  - Created pin_definitions_esp32.h with ESP32/S3 GPIO mappings
  - Updated motors.cpp with ESP32 LEDC PWM support
  - Updated imu.cpp with ESP32 Wire initialization
  - Updated radioComm.h/cpp with ESP32 serial ports
  - All 4 builds verified: esp32, esp32_calibration, teensy40, teensy40_calibration
- [x] fc_tool telemetry integration — 2026-02-06
  - Added printIMUTelemetry(), printAttitudeTelemetry(), printFullTelemetry() to debug.cpp
  - Output format matches fc_tool protocol: `ax=X ay=Y az=Z gx=X gy=Y gz=Z`
  - Serial command 't' toggles telemetry modes: off/IMU(50Hz)/full(20Hz)
  - See [fc_tool protocol](../../../fc_tool/docs/features/serial-telemetry-protocol.md)
- [x] Documentation improvements — 2026-02-06
  - Created README.md with build commands, workflow diagram, configuration guide
  - Created docs/teensy_wiring.md with Mermaid diagrams and pin tables
  - Created docs/esp32_wiring.md with Mermaid diagrams and pin tables
  - Supports all receiver types: SBUS, iBUS, DSM, PPM, PWM

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Up Next

_Priority queue for immediate work_

- [ ] Hardware testing when hardware is available

## Backlog

_Lower priority, do when time permits_

- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Serial command interface for PID tuning in calibration mode

## Recently Completed

_For context; clear periodically_

- [x] 6-position accelerometer calibration — 2026-02-06
  - Serial command 'm' triggers 6-position calibration
  - Outputs offset + scale factor for all 3 axes
  - Added IMU_ACC_SCALE_X/Y/Z defines to config.h
  - Updated imu.cpp to apply scale factors
  - Updated calibration guide with new option
- [x] Modularize main.cpp — 2026-02-06
  - Split into 5 modules: imu.cpp, control.cpp, motors.cpp, debug.cpp, globals.h
  - main.cpp reduced from ~1199 lines to ~448 lines (63% reduction)
  - Both live and calibration builds verified passing
- [x] Serial command interface for calibration — 2026-02-06
  - Added `checkSerialCommands()` function to main.cpp
  - Commands: r (radio), i (IMU), o (orientation), s (status), h (help)
  - Radio calibration now triggerable via serial (was missing CH6 trigger)
  - Updated startup message to show available commands
- [x] User guides updated — 2026-02-06
  - Rewrote 0_quickstart.md for new two-build workflow
  - Rewrote 2_calibration_guide.md with serial commands and CH6 triggers
  - Removed references to old RUN_* flags
- [x] PlatformIO build verification — 2026-02-06
  - Live build (`pio run -e teensy40`): SUCCESS — 26KB code, 9KB RAM
  - Calibration build (`pio run -e teensy40_calibration`): SUCCESS — 36KB code, 17KB RAM
  - Fixed: calibration.h linkage and extern declarations
  - Fixed: platformio.ini lib_deps for Calibration library
- [x] Build target separation implemented — 2026-02-05
- [x] Calibration output format fixed — 2026-02-05
- [x] Dead calibration code removed — 2026-02-05

---

## Notes

- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **fc_tool will help** — visual diagnostics during calibration (separate project at /fc_tool/)
- **Modular architecture** — code now split into imu, control, motors, debug modules
- **Platform support**: Teensy 4.x (recommended), ESP32/S3 (WiFi-enabled), STM32F4
- **NOT supported**: Arduino Uno/Mega (16MHz + no FPU = max 302Hz loop rate)

---

*Update every session: start by reading, end by updating.*
