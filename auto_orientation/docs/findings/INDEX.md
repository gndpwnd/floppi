# Findings — Index

Research notes that inform design decisions. Each finding answers "what did we learn, and what does it mean for the code?".

---

## BNO085 sensor

- [bno085_communication_modes.md](bno085_communication_modes.md) — UART vs I2C vs SPI tradeoffs for BNO085
- [bno085_i2c_implementation.md](bno085_i2c_implementation.md) — I2C-specific implementation notes
- [bno085_i2c_hang_diagnosis.md](bno085_i2c_hang_diagnosis.md) — Diagnosis of an I2C hang issue
- [bno085_pin_diagrams.md](bno085_pin_diagrams.md) — Pinout diagrams
- [bno085_test_sketches.ino](bno085_test_sketches.ino) — Working BNO085 test sketches (kept for reproduction)

## Calibration

- [CALIBRATION-IMPLEMENTATION-STATUS.md](CALIBRATION-IMPLEMENTATION-STATUS.md) — Status of the BNO085 EEPROM calibration feature
- [calibration-implementation-guide.md](calibration-implementation-guide.md) — Implementation guide

## Output formatting

- [FORMATTERS_IMPLEMENTATION.md](FORMATTERS_IMPLEMENTATION.md) — Sensor output formatter implementation notes

## Auto-orientation framework (Phase 4 planning, 2026-05-12 session)

All findings landed during the 2026-05-12 framework re-scoping session. Each is the output of a focused research agent.

### Sensors & drivers

- [bno055_driver_and_multi_imu_strategy.md](bno055_driver_and_multi_imu_strategy.md) — BNO055 driver design + runtime-vs-compile-time IMU selection + sensor-tagged calibration blob format. Discovers KI-1 (EEPROM-on-ESP32 silent fail).
- [mpu6050_external_mag_pipeline.md](mpu6050_external_mag_pipeline.md) — MPU6050 + HMC/QMC/LIS3MDL magnetometer stack with Madgwick fusion; hybrid host-side ellipsoid calibration; declination table strategy.

### Calibration & orientation tracking

- [balance_point_and_mounting_research.md](balance_point_and_mounting_research.md) — One-shot mounting capture: hybrid accel+gyro stillness gating; shortest-arc quaternion; 24-byte EEPROM record; 2-state Kalman for the balance loop (existing 16-state EKF is appropriate for GPS fusion but too heavy here).
- [online_adaptive_balance_tracking.md](online_adaptive_balance_tracking.md) — Online drift tracking: handles cable tether, battery sag, payload changes. Recommends slow-LPF-of-I-term for AVR; 3-state Kalman extension for Teensy/ESP32. Includes `MountingCalibrationStatus` API.

### Control & tuning

- [auto_pid_tuning_research.md](auto_pid_tuning_research.md) — Algorithm comparison (relay, revised relay, Z-N, ESC, fuzzy, RLS, twiddle) with concrete AVR RAM/flash. Recommends relay-feedback (Åström-Hägglund) as default with `ITuningStrategy` interface for compile-time selection.
- [disturbance_compensation_research.md](disturbance_compensation_research.md) — Push detection, IMU-accel feedforward, cascade control with optional wheel encoders, gain-bump recovery mode. Phase 4 ships push-recovery + accel feedforward; cascade deferred to Phase 7.

### Portability & infrastructure

- [multi_mcu_port_strategy.md](multi_mcu_port_strategy.md) — MCU matrix (Nano / Mega / Teensy 4.0 / Teensy 4.1 / ESP32 / ESP32-S3), `persistent_storage` HAL, per-platform pin split, FPU performance projections, ESP32 dual-core layout.
- [wifi_telemetry_integration_design.md](wifi_telemetry_integration_design.md) — `USE_WIFI` flag cascade (mirroring flight_controller), `src/network/` module subtree, REST + WebSocket endpoints, OTA strategy, dual-core ESP32 distribution.
- [browser_dashboard_architecture.md](browser_dashboard_architecture.md) — Vanilla HTML/JS + Three.js + LittleFS asset pipeline, 7 pages with their endpoints, WebSocket protocol with tagged-JSON multiplexing, mobile UX considerations.
- [tetherless_operation_strategy.md](tetherless_operation_strategy.md) — Workflow without USB tether per MCU class: on-bot button + LED + buzzer (Nano/Mega), HM-10 BLE module (Teensy), WiFi STA (ESP32). Battery topology, fallbacks, state-machine diagram.

### Applications & testing

- [application_catalog.md](application_catalog.md) — 9 applications profiled (balance bot, multirotor bridge, camera mount, VTOL, photogrammetry, AR/VR, robot arm, marine, edu kit) with metric tables and prioritization. Top 3 to build: balance robot → photogrammetry polish → flight-controller I2C bridge.
- [test_infrastructure_expansion.md](test_infrastructure_expansion.md) — 6-tier test taxonomy, scenario regression for the .ino reference, multi-MCU compile matrix (local + CI), HIL deferred, tooling extensions (5 scripts), `tests/data/` fixtures, coverage philosophy.

### Master design (synthesis)

- [MASTER_DESIGN.md](MASTER_DESIGN.md) — Synthesis of the above into actionable design plan: phase ordering, decision table, file/class/flag specs, cross-cutting concerns.

---

## Balance-bot bench session (2026-05-12, second half)

Phase 4 implementation and diagnosis. Five parallel research agents and two coding agents converged on the universal RLS auto-tune answer.

### Diagnosis & tuning

- [balance_failure_diagnosis_2026-05-12.md](balance_failure_diagnosis_2026-05-12.md) — Root-cause analysis of "motors slam during balance" — legacy gains, NDOF latency, Kd × quantization noise, contaminated online estimator.
- [conservative_balance_gains_recommendation.md](conservative_balance_gains_recommendation.md) — Specific Kp/Ki/Kd starting points for an under-instrumented small inverted pendulum. **Superseded by BOOTSTRAP (Phase 4.10c) — kept for historical context.**
- [midrange_balance_gains.md](midrange_balance_gains.md) — Middle-ground Kp/Ki/Kd between the conservative and legacy regimes. **Also superseded by BOOTSTRAP — kept for historical context.**
- [latency_budget_2026-05-12.md](latency_budget_2026-05-12.md) — End-to-end sensor→actuator latency breakdown; BNO055 NDOF group delay is the dominant 20-40 ms contributor.
- [bno055_latency_and_pitch_fusion.md](bno055_latency_and_pitch_fusion.md) — Uno-specific BNO055 latency analysis + pitch fusion options (NDOF vs gyro-integration + accel-fusion).
- [theoretical_audit_balance_stack.md](theoretical_audit_balance_stack.md) — Full theoretical-soundness audit of the balance control stack (2026-05-18).

### State machine

- [balance_held_fallen_state_machine.md](balance_held_fallen_state_machine.md) — Original HELD/FALLEN design using lateral-gyro detector. Lenient resume variant landed in firmware.
- [multi_axis_anomaly_handling_detection.md](multi_axis_anomaly_handling_detection.md) — Phase 4.7c proposal: Welford z-scores → Mahalanobis upgrade for true multi-axis anomaly detection.
- [research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md) — Phase 2.7 proposal: project BNO055 linear-acceleration into the motor-null subspace via a learned `body_heading_unit`; detects handling without conflating sensor mounting skew or non-level ground. Includes flash/RAM budget for Uno.

### Universal auto-tune (Phase 4.10)

- [dynamic_pwm_accel_learning.md](dynamic_pwm_accel_learning.md) — The system-ID design: scalar RLS for K_motor + closed-form PD-from-K_motor. **Coded and shipped 2026-05-12.**
- [bootstrap_protocol_unstable_plant.md](bootstrap_protocol_unstable_plant.md) — 6-stage sequenced bootstrap (SEED → MOUNT_CONVERGED → PLANT_IDENTIFIED → GAINS_REFINED → ADAPTIVE). Implemented as Phase 4.10c BOOTSTRAP state (2026-05-18 PM evening).

### Phase 2 — CHARACTERISE actuator (planning)

- [phase2_characterise_final_plan.md](phase2_characterise_final_plan.md) — Final implementation plan for Phase 2 CHARACTERISE state (per-wheel stiction + saturation sweep). Pending implementation.

### Background research (5 parallel agents)

- [research_inverted_pendulum_control_methods.md](research_inverted_pendulum_control_methods.md) — Survey of academic methods (LQR, MRAC, L1, SMC, STR, ILC, RL, fuzzy, GA/PSO, NN). Self-Tuning Regulator (Åström-Wittenmark 1973) recommended.
- [research_open_source_balance_bots.md](research_open_source_balance_bots.md) — 8 OSS projects surveyed. Best reference: TKJElectronics Balanduino. Zero of 8 auto-tune on hardware.
- [research_universal_zero_knowledge_tuning.md](research_universal_zero_knowledge_tuning.md) — Deep-dive on the "is universal zero-knowledge feasible" question. Yes within a declared bench-scale class.
- [research_osoyoo_reference_implementation.md](research_osoyoo_reference_implementation.md) — Code review of the Osoyoo balancing-car kit (local copy at `~/tmp/osoyoo/`). Three concrete fixes informed Phase A.
- [research_multi_orientation_balance_feasibility.md](research_multi_orientation_balance_feasibility.md) — Feasibility of balancing past 90° / arbitrary orientation. Level 2 firmware-only (Phase 4.11) is the recommended next step.

### Cross-project synthesis (drone ↔ balance bot, 2026-05-12 evening final round)

Three parallel agents triggered by the user's question "how does the flight_controller keep the drone stable without flipping over? Can we use the same approach?"

- [research_flight_controller_pid_lessons.md](research_flight_controller_pid_lessons.md) — Deep-dive into `flight_controller/`'s PID architecture (controlANGLE / controlRATE), gain choices, and existing auto-cal research. Concludes: drone uses hardcoded dRehmFlight constants — no auto-tune. Cross-project lessons that DO transfer (sensor cal hygiene, cascade architecture, TPA-as-K_motor-scaling) vs. those that DON'T (mixer, trim-hover bootstrap, decoupled multi-axis).
- [research_bno055_calibration_audit.md](research_bno055_calibration_audit.md) — End-to-end audit of the BNO055 cal flow in auto_orientation. Identifies 6 failure modes ranked by likelihood. Concludes: the cal flow is structurally sound; the likely real problem was the OnlineMountingEstimator placeholder-zero bug (Phase A Item 2, now fixed). Includes a 5-step diagnostic protocol for the next bench session.
- [research_drone_vs_balance_bot_stability.md](research_drone_vs_balance_bot_stability.md) — Control-theory answer to "why is a drone simpler?" Drone is open-loop *neutrally stable* in attitude (linearised hover = pure double integrator); balance bot is open-loop *unstable* (gravity contributes +m·g·L·θ → exponential growth at ~120 ms time constant). That one sign difference is the root cause of every downstream tuning asymmetry.

---

## Backlog & cross-reference

- [operator_ideas_backlog.md](operator_ideas_backlog.md) — Durable index of every operator-suggested feature or design principle, with date, source link, status, and one-line technical translation. The source-of-truth for which ideas are deferred, in-progress, or done. Snapshot tables in session records are point-in-time copies of this index.

---

*Last updated: 2026-05-19 (doc audit — indexed midrange_balance_gains, bno055_latency_and_pitch_fusion, theoretical_audit_balance_stack, phase2_characterise_final_plan; marked balance-gain rec docs as superseded by BOOTSTRAP). Add new findings as you discover them.*
