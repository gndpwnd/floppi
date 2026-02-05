# Flight Controller Firmware - Todo

> Last updated: 2026-02-05

## In Progress

_Tasks actively being worked on_

- [ ] Bootstrap project structure (scope, roadmap, docs organization)

## Blocked

_Tasks waiting on something (include reason)_

- [ ] Bench test: IMU sensor validation — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: SBUS receiver communication — **Blocked by**: Hardware not yet assembled
- [ ] Bench test: Motor/ESC response — **Blocked by**: Hardware not yet assembled

## Up Next

_Priority queue for immediate work_

- [ ] Design firmware state machine (calibration mode vs live mode)
- [ ] Define build targets in platformio.ini for calibration vs live builds
- [ ] Improve calibration value export (output copy-pasteable config.h snippets)
- [ ] Research and document auto-calibration approaches (store in docs/findings/)
- [ ] Review and improve radio calibration workflow

## Backlog

_Lower priority, do when time permits_

- [ ] Implement IMU orientation auto-detection
- [ ] Implement radio channel auto-mapping
- [ ] Multi-position accelerometer calibration (6-position)
- [ ] Define serial protocol for fc_tool integration
- [ ] Create example configurations for common VTOL types
- [ ] Implement full 9DOF Madgwick filter for MPU9250

## Recently Completed

_For context; clear periodically_

- [x] dRehmFlight port to PlatformIO — Pre-2026
- [x] MPU6050 + SBUS integration — Pre-2026
- [x] Basic IMU auto-calibration via CH6 — Pre-2026
- [x] Project structure bootstrap (scope.md, roadmap.md, todo.md) — 2026-02-05

---

## Notes

- **Hardware testing is the critical path** — firmware is ready, need physical drone to validate
- **Calibration workflow is the key differentiator** — focus on making calibrate → hard-code → flash → fly as seamless as possible
- **Don't over-engineer** — iterate carefully, get basic features working well before adding complexity
- **fc_tool will help** — visual diagnostics during calibration development (separate project at /fc_tool/)

---

*Update every session: start by reading, end by updating.*
