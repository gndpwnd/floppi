# Flight Controller Firmware - Todo

> Last updated: 2026-02-05

## In Progress

_None_

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Up Next

_Priority queue for immediate work_

- [ ] Implement build target separation (calibration vs live) — see [features/build-targets.md](features/build-targets.md)
  - Add `_calibration` PlatformIO environments using `extends`
  - Wrap calibration code in `#ifdef CALIBRATION_MODE` guards in main.cpp
  - Guard `RUN_*` config flags so they only apply in calibration builds
- [ ] Fix calibration output format mismatch
  - calibration.cpp prints `float AccErrorX = ...;` but config.h uses `#define IMU_ACC_ERROR_X ...f`
  - Fix `printIMUCalibrationResults()` output to match config.h `#define` format
- [ ] Unify calibration paths
  - Make CH6-triggered calibration call the better calibration.cpp routines (with quality checks)
  - Remove duplicate simple calibration functions from main.cpp
- [ ] Review and improve radio calibration workflow

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

---

## Notes

- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **Calibration workflow is the key differentiator** — focus on making calibrate → hard-code → flash → fly as seamless as possible
- **Don't over-engineer** — iterate carefully, get basic features working well before adding complexity
- **fc_tool will help** — visual diagnostics during calibration development (separate project at /fc_tool/)
- **Known bugs**: calibration.cpp output format doesn't match config.h `#define` syntax; two overlapping calibration code paths (main.cpp simple vs calibration.cpp advanced)

---

*Update every session: start by reading, end by updating.*
