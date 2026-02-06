# Flight Controller Firmware - Todo

> Last updated: 2026-02-06

## In Progress

_No tasks in progress_

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Up Next

_Priority queue for immediate work_

- [ ] Define serial protocol for fc_tool integration

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

---

*Update every session: start by reading, end by updating.*
