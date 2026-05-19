# Archive — Index

Old documents superseded by newer ones, session summaries, completion notes, and reference sketches. Kept for historical context, not actively maintained.

---

## Durable lessons (READ BEFORE ITERATING)

- [LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) — **REQUIRED READING before working on the balance-bot reference app.** Hard-won insights from the 2026-05-12 bench iteration: don't hand-tune unstable plants, NDOF latency gotchas, mounting estimator placeholder bug, why a drone is categorically different, 10 concrete gotchas + 8 rules for the next session.

---

## Subfolders

| Folder | Contents |
|--------|----------|
| [balancing_robot_reference/](balancing_robot_reference/INDEX.md) | User's working `SelfBallancingRobot3.ino` + dissection notes (source for the upcoming `src/applications/balancing_robot/`) |
| [session_records/](session_records/INDEX.md) | Dated full-session work logs (`YYYY-MM-DD_topic.md`) |

---

## Session summaries (flat archive — to migrate into session_records/)

- [SESSION_SUMMARY_2026-05-06_FINAL.md](SESSION_SUMMARY_2026-05-06_FINAL.md) — End-of-day 2026-05-06
- [SESSION_SUMMARY_2026-05-07_BNO_COMPLETE.md](SESSION_SUMMARY_2026-05-07_BNO_COMPLETE.md) — BNO085 work complete
- [session_2026-05-05_cleanup-and-bootloader-fix.md](session_2026-05-05_cleanup-and-bootloader-fix.md) — Bootloader-fix session
- [session_2026-05-06_i2c_migration_and_diagnostics.md](session_2026-05-06_i2c_migration_and_diagnostics.md) — I2C migration session
- [session_status_gps_and_bno085_working.md](session_status_gps_and_bno085_working.md) — Status snapshot when GPS + BNO085 both worked

## Project initialization

- [project_init_prompt.md](project_init_prompt.md) — Original kick-off prompt
- [compere_init.eml](compere_init.eml) — Initialization email (raw)
- [compere_init.md](compere_init.md) — Initialization email (parsed)

## Old reference sketches

- [BN085_I2C_Adafruit.ino](BN085_I2C_Adafruit.ino) — Early Adafruit-based BNO085 sketch
- (See also `balancing_robot_reference/SelfBallancingRobot3.ino` for the BNO055 balance-bot reference.)

## Superseded implementation notes

- [BNO085_EXTENSIONS_USAGE.md](BNO085_EXTENSIONS_USAGE.md) — Pre-final BNO085 extensions usage
- [bno085_implementation_status.md](bno085_implementation_status.md) — Pre-final BNO085 status
- [bno085_i2c_compatibility_analysis.md](bno085_i2c_compatibility_analysis.md) — I2C compatibility research
- [bno085_sh2_protocol_analysis.md](bno085_sh2_protocol_analysis.md) — SH-2 protocol deep-dive
- [bno085-calibration-persistence.md](bno085-calibration-persistence.md) — BNO085 calibration persistence research
- [calibration_persistence_qa.md](calibration_persistence_qa.md) — Calibration persistence Q&A
- [sh2_api_investigation.md](sh2_api_investigation.md) — SH-2 API investigation notes
- [COORDINATE_FRAME_IMPLEMENTATION.md](COORDINATE_FRAME_IMPLEMENTATION.md) — Coordinate-frame implementation notes
- [GPS_DRIVER_IMPLEMENTATION_SUMMARY.md](GPS_DRIVER_IMPLEMENTATION_SUMMARY.md) — GPS driver implementation summary
- [QUATERNION_IMPLEMENTATION_SUMMARY.md](QUATERNION_IMPLEMENTATION_SUMMARY.md) — Quaternion implementation summary
- [gps_lock_troubleshooting.md](gps_lock_troubleshooting.md) — GPS-lock troubleshooting
- [gps-accuracy-improvement.md](gps-accuracy-improvement.md) — GPS accuracy research
- [mpu6050-yaw-estimation.md](mpu6050-yaw-estimation.md) — MPU6050 yaw-without-magnetometer research
- [flight-controller-patterns.md](flight-controller-patterns.md) — Pattern notes borrowed from sister `flight_controller/` project
- [serial_port_permissions_fix.md](serial_port_permissions_fix.md) — Linux serial-port permissions fix
- [READY_TO_USE.md](READY_TO_USE.md) — Old "ready" gate doc

## Phase plans (snapshots)

- [PHASE_1_MASTER_IMPLEMENTATION_PLAN.md](PHASE_1_MASTER_IMPLEMENTATION_PLAN.md) — Phase 1 plan snapshot
- [PHASE_3_MASTER_IMPLEMENTATION_PLAN.md](PHASE_3_MASTER_IMPLEMENTATION_PLAN.md) — Phase 3 plan snapshot
- (Currently active phase plans live one level up in `docs/`.)

## Implementation milestones

- [TASK_7_COMPLETION.md](TASK_7_COMPLETION.md) — Task 7 completion notes
- [TASK_16_COMPLETION.md](TASK_16_COMPLETION.md) — Task 16 completion notes
- [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md) — Generic implementation notes
- [IMPLEMENTATION_LOG.md](IMPLEMENTATION_LOG.md) — Running log
- [IMPLEMENTATION_SUMMARY_5.1-5.5.md](IMPLEMENTATION_SUMMARY_5.1-5.5.md) — Tasks 5.1-5.5 summary
- [INTEGRATION_TEST_SUMMARY.md](INTEGRATION_TEST_SUMMARY.md) — Integration-test summary
- [README.md](README.md) — Archive folder README (older format)

---

*Last updated: 2026-05-12. New session records should go in `session_records/`, not flat here.*
