# Implementation Notes — Index

Per-component implementation walk-throughs. Reading order: pair each note with the corresponding source file under `src/`.

| File | Maps to source | One-liner |
| --- | --- | --- |
| [persistent_storage.md](persistent_storage.md) | `src/storage/persistent_storage.{h,cpp}` + 4 backends | Byte-addressable HAL over AVR EEPROM / Teensy flash-emul / ESP32 NVS / native heap (fixes KI-1). |
| [pid_controller.md](pid_controller.md) | `src/control/pid_controller.{h,cpp}` | Generic single-axis PID with D-on-measurement, integral-clamp anti-windup; PID_v1-compatible API. |
| [auto_pid_tuner.md](auto_pid_tuner.md) | `src/control/auto_pid_tuner.{h,cpp}` + `tuning_strategy.h` + `tuners/relay_feedback.{h,cpp}` | Auto-PID coordinator + relay-feedback strategy. **AS-BUILT: relay is compiled out / unwired; primary auto-tune is pole-placement in `plant_identifier.cpp`.** |
| [online_mounting_estimator.md](online_mounting_estimator.md) | `src/navigation/online_mounting_estimator.{h,cpp}` | Online drift tracker (AVR path). **AS-BUILT: LPFs *mean pitch*, not the I-term; tc overridden 300 s→8 s in main.cpp.** ±5° bound, rate limit, 4 freeze gates. |
| (no note yet) | `src/control/plant_identifier.{h,cpp}` | **Scalar RLS K_motor estimator + critically-damped pole-placement PD mapping (`Kp=ωₙ²/K`, `Kd=2ζωₙ/K`); σ-modification. The live auto-tune engine.** |
| (no note yet) | `src/applications/balancing_robot/noise_floor_estimator.h` | **Welford running σ of gyro-rate / accel-deviation over a 200-sample quiet window. Observation-only (unconsumed); "measure-don't-hardcode" feedstock that unblocks the 5 noise-cited scope violations (bench-gated).** |
| [bno055_driver.md](bno055_driver.md) | `src/sensors/bno055.{h,cpp}` | Adafruit_BNO055 wrapper; quaternion-first (avoids Euler 90° discontinuity); 22 B cal blob (fixes KI-2). |
| [l298n_motor_driver.md](l298n_motor_driver.md) | `src/actuators/motor_driver.h` + `l298n_motor_driver.{h,cpp}` | Abstract dual-motor interface + L298N H-bridge; signed ±255 speed; stiction-floor + free-coast/brake. |
| [mounting_calibration.md](mounting_calibration.md) | `src/navigation/mounting_calibration.{h,cpp}` | One-shot mounting capture: gyro-stillness gate + accel LPF + shortest-arc quaternion + 24 B record. |
| [neo_m9n_driver_implementation.md](neo_m9n_driver_implementation.md) | `src/sensors/gps.{h,cpp}` | NEO-M9N NMEA driver: GPGGA/GPRMC, checksum, DDMM→decimal, HDOP→accuracy. |
| [sensor_output_manager.md](sensor_output_manager.md) | `src/output/sensor_output_manager.{h,cpp}` | Dual-rate multiplexer: 10 Hz IMU + 1 Hz GPS → JSON/CSV with GPS-freshness gating. |

**Conventions**: Each implementation note covers (1) data flow through the module, (2) state machine or core algorithm, (3) buffer-size / RAM costs, (4) integration points with other modules, (5) tests.

For decision-level rationale (D1–D20), see [`../findings/MASTER_DESIGN.md`](../findings/MASTER_DESIGN.md). For deep-dive theory, see the linked research notes under [`../findings/`](../findings/).

---

*Last updated: 2026-05-12.*
