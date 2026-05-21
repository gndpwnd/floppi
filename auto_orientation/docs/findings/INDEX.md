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

## Platform-bifurcation pivot (2026-05-19) — Mega-universal vs Uno-minimal

Research and investigation outputs that landed alongside the 2026-05-19 pivot. These inform the Mega-universal feature set (collision detection, wheel encoders, IMU-only position containment) and the diagnosis of why the previous universal stack was not converging.

### Investigations

- [investigation_held_state_machine_failure_2026-05-19.md](investigation_held_state_machine_failure_2026-05-19.md) — Root-cause analysis of the HELD state-machine failing to trigger / triggering spuriously during the 2026-05-18 PM-late bench session. Informs the state-machine cleanup work and the collision-detection design.

### Mega-universal research

- [research_collision_signature_bno055.md](research_collision_signature_bno055.md) — Collision-signature detection using BNO055 linear-acceleration / gyro spikes. Referenced from roadmap.md Phase 4M.0 as the implementation basis for the collision-detection module.
- [research_wheel_encoders_mega_2026-05-19.md](research_wheel_encoders_mega_2026-05-19.md) — Wheel-encoder integration strategy on Arduino Mega (interrupt pin budget, encoder type tradeoffs, position-loop design). Referenced from roadmap.md Phase 4M.1.
- [research_imu_only_position_containment.md](research_imu_only_position_containment.md) — IMU-only position-containment strategy for the Mega target before wheel encoders are added: integrating linear-acceleration estimates with drift management.

---

## Audits & quality reviews

Cross-cutting audits of the documentation tree and the code/control stack. Each audit produces a dated report with prioritized findings; sibling agents own the follow-up fixes.

### 2026-05-19 audit batch

- [audit_documentation_2026-05-19.md](audit_documentation_2026-05-19.md) — Documentation-tree audit landed alongside the platform-bifurcation pivot. Identifies stale INDEXes, orphaned findings files, broken cross-references.
- [audit_code_quality_balance_stack_2026-05-19.md](audit_code_quality_balance_stack_2026-05-19.md) — Code-quality audit of the balance-stack source (controllers, estimators, state machine, motor driver). Reviewed for theoretical soundness, defensive checks, and scope-violation accounting.

### 2026-05-20 audit batch (post-merge)

- [audit_documentation_2026-05-20.md](audit_documentation_2026-05-20.md) — Post-merge documentation audit. 41 findings (P0: 8 broken links, P1: 13 orphaned/missing, P2: 12 inconsistencies, P3: 8 gaps). Drives this index update.
- [audit_code_quality_2026-05-20.md](audit_code_quality_2026-05-20.md) — Post-merge code-quality audit of the auto_orientation source tree. 19 findings (1 P0 — collision regression, found already-fixed by err0r `f2e9732`).
- [audit_test_coverage_2026-05-20.md](audit_test_coverage_2026-05-20.md) — Post-merge test-coverage audit. 26 findings; 4 critical untested modules identified.
- [audit_security_2026-05-20.md](audit_security_2026-05-20.md) — Post-merge security audit. 32 findings, 0 P0; drove the 4 P1 calibration-storage hardening fixes.
- [audit_build_system_2026-05-20.md](audit_build_system_2026-05-20.md) — Post-merge build-system audit. 30 findings, 4 P0 (most found already-fixed by err0r `f2e9732`).

---

## 2026-05-20 sync session — architecture, reconciliation, fixes

Multi-agent sync day after the err0r two-clone divergence merge (`ec4ef53`). Session record: [`../archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md`](../archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md).

- [architecture_plan_2026-05-20.md](architecture_plan_2026-05-20.md) — Joint scaffolding plan partitioning remaining work into workstreams A–F plus INFRA and DOC tracks; the partition mapped onto the parallel-agent dispatch boundaries.
- [state_reconciliation_2026-05-20.md](state_reconciliation_2026-05-20.md) — Source-vs-doc truth check. Confirmed Phases 4M.0 / 4M.1 / 4M.12 all already landed via err0r `f2e9732`; caught the recurring stale-audit pattern (audits trusted doc claims over source).
- [mega_ram_fix_2026-05-20.md](mega_ram_fix_2026-05-20.md) — F2 EKF state-size reclaim (~1024 B); `mega_orientation` builds at 74.5% RAM.
- [security_fix_calibration_2026-05-20.md](security_fix_calibration_2026-05-20.md) — 4 P1 calibration-storage fixes (CRC-8-CCITT, integer overflow guard, uint16 length field, version reject) + `CAL_FORMAT_VERSION` bump 0x01→0x02 (old EEPROM blobs now rejected — operator must re-calibrate).
- [tuner_format_alignment_2026-05-20.md](tuner_format_alignment_2026-05-20.md) — Verification that the tuner output format is already aligned with the firmware consumer (no-op).

---

## Backlog & cross-reference

- [operator_ideas_backlog.md](operator_ideas_backlog.md) — Durable index of every operator-suggested feature or design principle, with date, source link, status, and one-line technical translation. The source-of-truth for which ideas are deferred, in-progress, or done. Snapshot tables in session records are point-in-time copies of this index.

---

## 2026-05-19 PM session — landings + design + diagnoses

Multi-agent landing wave that followed the 2026-05-19 AM bifurcation pivot. Collision detection re-landed; wheel encoder driver + integration LIVE; Phase 4M.12 PWM auto-discovery code LANDED. These docs scope what shipped and what's next.

- [phase_4_11a_design_2026-05-19.md](phase_4_11a_design_2026-05-19.md) — Encoder-first position containment outer-loop design (~590 lines). Encoder odometry primary, IMU-only pitch double-integration as fallback via `USE_IMU_ONLY_OUTER_LOOP` runtime gate. Specifies math, cascade structure, slew limits, EEPROM persistence, bench validation. Implementation queued for next session (would have collided with the simultaneous Phase 4M.12 agent in `balance_app.cpp`). Counterpart to [research_imu_only_position_containment.md](research_imu_only_position_containment.md).
- [tuner_kd_accuracy_2026-05-19.md](tuner_kd_accuracy_2026-05-19.md) — Diagnoses the random-search tuner's chronic Kd underestimate (was ~16 vs reference 38). After fix, Kd lands ~62 — overshoots reference but stable region widened. Stress-plant preset still under-tunes; mechanical-damping-model TODO captured in [todo.md](../todo.md).
- [mega_orientation_ram_overflow_diagnosis_2026-05-19.md](mega_orientation_ram_overflow_diagnosis_2026-05-19.md) — Root-causes the `mega_orientation` build's RAM overflow to the EKF stub. Identifies ~2257 B reclaimable across three Phase A fixes. Not yet executed; queued in [todo.md](../todo.md) and [MEGA_UNIVERSAL_PLAN.md §9](../MEGA_UNIVERSAL_PLAN.md) item 3.
- [audit_uno_minimal_2026-05-19.md](audit_uno_minimal_2026-05-19.md) — Audit of the Uno-minimal application scaffold. P0 fixes (startup delay, `ATOMIC_BLOCK`, `<stdint.h>`) and P1 top-5 (incl. new operator commands `g`/`p`) landed; P1 #6-15 + P2 12 findings queued.
- [phase_4m12_landed_2026-05-19.md](phase_4m12_landed_2026-05-19.md) — Verification record for the Phase 4M.12 PWM auto-discovery code landing in `balance_app.{h,cpp}`.
- [verification_2026-05-19.md](verification_2026-05-19.md) — Cross-cutting 2026-05-19 multi-agent landing-wave verification report.
- [brute_tune_simplification_design_2026-05-19.md](brute_tune_simplification_design_2026-05-19.md) — Design for simplifying the `tools/sim/brute_tune.py` search/scoring pipeline.

---

## 2026-05-20 session — Mega cascade completion, Uno guided tuning, audits

Multi-agent landing wave that completed the Mega two-stage cascade controller (Phases 4M.2 → 4M.14) and shipped the Uno on-device guided P→I→D tuning feature. Each phase has a landing report; reviews and audits scope what shipped and what's next.

### Mega cascade landings (Workstream F)

- [phase_4m2_landed_2026-05-20.md](phase_4m2_landed_2026-05-20.md) — Phase 4M.2 (F.1): encoder-driven `K_motor` cross-check in BOOTSTRAP; measures `K_gyro` pitch-rate response per unit PWM during the four-pulse sequence.
- [phase_4m11_landed_2026-05-20.md](phase_4m11_landed_2026-05-20.md) — Phase 4M.11 (Workstream D): `e` serial command + EEPROM wheel-encoder calibration wizard (operator rolls bot 1.000 m by hand). Implements RWE §5.
- [phase_4m13_landed_2026-05-20.md](phase_4m13_landed_2026-05-20.md) — Phase 4M.13 (F.2): velocity/position outer loop — the balance stack becomes a two-stage cascade. Five outer-loop gains landed hardcoded pending 4M.14 auto-derivation.
- [phase_4m14_design_2026-05-20.md](phase_4m14_design_2026-05-20.md) — Phase 4M.14 (F.3) design spec: analytical auto-derivation of the outer-loop gains; contract for the `4M.14-impl` workstream (re-run after a usage-limit loss).
- [phase_4m14_landed_2026-05-20.md](phase_4m14_landed_2026-05-20.md) — Phase 4M.14 (F.3) landing: the three dynamic `PositionLoop` gains (`K_POS`, `K_VEL`, `POS_LEAK`) are now derived at runtime, retiring the Phase 4M.13 hardcoded constants; both builds green.
- [phase_4m14_review_2026-05-20.md](phase_4m14_review_2026-05-20.md) — Read-only review of Phase 4M.14: the implementation deviated from its design spec — assesses whether the deviation is physically sound or a reverse-fit.
- [workstream_f_review_2026-05-20.md](workstream_f_review_2026-05-20.md) — Read-only audit of the three Workstream-F landings (4M.2, 4M.11, 4M.13) — EEPROM slot map, cascade soundness. Verdict: sound for bench deployment.

### Uno guided tuning

- [uno_guided_tuning_design_2026-05-20.md](uno_guided_tuning_design_2026-05-20.md) — Design for the Uno on-device interactive serial-driven guided P→I→D tuning experience (Option B hybrid, build-env split); operator-driven scope pivot.
- [guided_tuning_review_2026-05-20.md](guided_tuning_review_2026-05-20.md) — Review of the guided-tuning feature against design §2 and embedded-safety criteria. Verdict: feature sound, cleared for bench use, no must-fix items.

### Audits & synthesis

- [ao_security_reaudit_2026-05-20.md](ao_security_reaudit_2026-05-20.md) — Security posture re-audit of the code that landed today (Phases 4M.2/4M.11/4M.13, Workstream-F review, guided tuning) plus prior findings whose files changed; static read, no builds.
- [ao_uno_techdebt_2026-05-20.md](ao_uno_techdebt_2026-05-20.md) — Tech-debt audit of `balancing_robot_uno/` — maintainability, dead code, duplication, naming, build-flag hygiene; cross-references the security and guided-tuning reviews rather than duplicating them.
- [ao_session_synthesis_2026-05-20.md](ao_session_synthesis_2026-05-20.md) — Static synthesis of everything that landed in `auto_orientation/` on 2026-05-20 (landing reports + reviews + spot-checks).
- [ao_roadmap_post_4m14_2026-05-20.md](ao_roadmap_post_4m14_2026-05-20.md) — Forward-looking roadmap scoping the workstreams after Phase 4M.14, with a 2-session dispatch proposal. Carries a SUPERSEDED banner — 4M.14 has since landed; live next step is Workstream G.

---

## 2026-05-21 session — Workstream G, 4M.14 test coverage, security hardening

Multi-agent, orchestrator-managed wave. Made the Mega cascade observable on the
bench (telemetry accessors + `g` command + host plotting) and safe (two P1
calibration-storage hardening fixes), plus test-coverage closure for Phase
4M.14. No new control phase — remaining items are bench-hardware-gated.

Canonical session record: [../archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](../archive/session_records/2026-05-21_multi_agent_workstream_g_security.md).

- [session_synthesis_2026-05-21.md](session_synthesis_2026-05-21.md) — Synthesis of everything that landed in `auto_orientation/` on 2026-05-21: Workstream-G G1/G2/G3/Gap-3 codeable items, the 4M.14 gain-derivation suite wired into `build_tests.sh` (native 14→18), two P1 security fixes (mounting CRC + `restoreFromEEPROM()` buffer-overflow), platformio.ini de-dup, and doc-drift fixes. Uncommitted. Bench-gated next steps: F-3 K_VEL observation, regression-baseline capture, real-motor PWM-discovery validation.
- [workstream_g_bench_protocol_2026-05-21.md](workstream_g_bench_protocol_2026-05-21.md) — Bench-tuning protocol for capturing and interpreting `g`-command telemetry runs with `tools/plot_bench_run.py`.

---

*Last updated: 2026-05-21 (2026-05-21 multi-agent session findings added). Prior: 2026-05-20 (post-sync hygiene pass — doc-fixer compact entries integrated above + err0r 2026-05-19 PM session additions kept; the 2026-05-19 AM-session detail block was de-duplicated into the compact entries in "Platform-bifurcation pivot" + "Audits & quality reviews" sections above; 2026-05-20 sync-session findings — architecture_plan, state_reconciliation, mega_ram_fix, security_fix_calibration, tuner_format_alignment — added and the four "in progress" audit placeholders resolved).*
