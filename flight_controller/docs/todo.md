# Flight Controller Firmware - Todo

> Last updated: 2026-02-05

## In Progress

- [ ] Verify PlatformIO compilation — need to run `pio run -e teensy40` and `pio run -e teensy40_calibration` on a machine with PlatformIO installed (not available in current WSL env)

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Up Next

_Priority queue for immediate work_

- [ ] Add radio calibration trigger mechanism — currently `calibrateRadio()` has no CH6 trigger (all 3 positions are used by IMU cal). Needs serial command trigger or startup sequence.
- [ ] Update old user guides (0_quickstart.md through 3_troubleshooting.md) — reference removed `RUN_*` flags and old workflow. Rewrite when calibration system is tested on hardware.

## Backlog

_Lower priority, do when time permits_

- [ ] Multi-position accelerometer calibration (6-position)
- [ ] Define serial protocol for fc_tool integration
- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250
- [ ] Serial command interface for PID tuning in calibration mode

## Recently Completed

_For context; clear periodically_

- [x] dRehmFlight port to PlatformIO — Pre-2026
- [x] MPU6050 + SBUS integration — Pre-2026
- [x] Basic IMU auto-calibration via CH6 — Pre-2026
- [x] Radio auto-mapping calibration routine (lib/Calibration/) — Pre-2026
- [x] IMU orientation auto-detection routine (lib/Calibration/) — Pre-2026
- [x] Project structure bootstrap (scope.md, roadmap.md, todo.md, README.md) — 2026-02-05
- [x] Auto-calibration research documented — 2026-02-05 (see [findings/auto-calibration-research.md](findings/auto-calibration-research.md))
- [x] Code review and plan for build target separation — 2026-02-05 (see [features/build-targets.md](features/build-targets.md))
- [x] Build target separation implemented — 2026-02-05
  - platformio.ini: `_calibration` environments using `extends` + `-D CALIBRATION_MODE`
  - config.h: `RUN_*` flags guarded behind `#ifdef CALIBRATION_MODE`
  - main.cpp: all calibration code, debug prints, state machine wrapped in `#ifdef CALIBRATION_MODE`
- [x] Calibration output format fixed — 2026-02-05
  - calibration.cpp now outputs `#define IMU_ACC_ERROR_X 0.123456f` (matches config.h)
  - Radio and orientation results updated with correct build instructions
- [x] Calibration paths unified — 2026-02-05
  - CH6 switch and calibration state machine now call calibration.cpp routines directly (calibrateIMU, calibrateIMUWithOrientation, calibrateRadio)
  - Old simple functions guarded as dead code (removal pending)
- [x] Dead calibration code removed from main.cpp — 2026-02-05
  - Removed: runAccelGyroCalibration, runAttitudeCalibration, runRadioCalibration, calculate_IMU_error, calibrateAttitude
  - Removed: RUN_* config flags (now dead since CH6 triggers directly)
  - ~194 lines of dead code eliminated
- [x] Radio calibration bug fixes — 2026-02-05
  - Fixed: AUX1/AUX2 detection now excludes yaw channel (was missing)
  - Fixed: Removed `String` type from ChannelData struct (heap fragmentation risk on Teensy)
  - Fixed: detectMovedChannel expanded to support 5 exclude channels

---

## Notes

- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **Calibration workflow is the key differentiator** — focus on making calibrate → hard-code → flash → fly as seamless as possible
- **Don't over-engineer** — iterate carefully, get basic features working well before adding complexity
- **fc_tool will help** — visual diagnostics during calibration development (separate project at /fc_tool/)
- **Needs PlatformIO build verification** — static analysis of `#ifdef` guards looks correct, but actual compilation not yet tested (PlatformIO not installed in WSL)

---

*Update every session: start by reading, end by updating.*
