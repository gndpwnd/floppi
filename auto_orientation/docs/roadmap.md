# Roadmap: Auto Orientation Framework

**Current phase**: Phase 4 — Auto-orientation framework + balancing-robot reference application (now bifurcated into 4M Mega-universal and 4U Uno-minimal — see below)
**Last updated**: 2026-05-21 (Workstream G bench-tuning support + Phase 4M.14 test-coverage + two P1 security fixes landed — see [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md))

### 2026-05-21 status snapshot

| Item | Status | Reference |
|---|---|---|
| Phase 4M.0 — collision detection | **LANDED + VERIFIED** (27/27 tests) | [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §1 |
| Phase 4M.1 — wheel encoder driver | **LANDED + VERIFIED** | [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §3 |
| Phase 4M.12 — PWM range auto-discovery (code) | **LANDED + VERIFIED** | [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §2 |
| Phase 4M.11 — `e` cmd + EEPROM CPM/radius | **LANDED** (Workstream D, 2026-05-20) | [findings/phase_4m11_landed_2026-05-20.md](findings/phase_4m11_landed_2026-05-20.md) |
| Phase 4M.2 — K cross-check (IMU vs encoder) | **LANDED** (Workstream F, 2026-05-20) | [findings/phase_4m2_landed_2026-05-20.md](findings/phase_4m2_landed_2026-05-20.md) |
| Phase 4M.13 — velocity / position outer loop | **LANDED** (Workstream F, 2026-05-20) | [findings/phase_4m13_landed_2026-05-20.md](findings/phase_4m13_landed_2026-05-20.md) |
| Phase 4M.14 — outer-loop gain auto-derivation | **LANDED** (Workstream F.3, 2026-05-20; native test-covered 2026-05-21) | [findings/phase_4m14_landed_2026-05-20.md](findings/phase_4m14_landed_2026-05-20.md) |
| Workstream G — bench-tuning support (telemetry + `g` cmd + host plotting) | **LANDED — codeable items** (2026-05-21); bench-validation gated | [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md), [findings/workstream_g_bench_protocol_2026-05-21.md](findings/workstream_g_bench_protocol_2026-05-21.md) |
| Calibration security (CRC8-CCITT + length + version + overflow) | **LANDED** — `CAL_FORMAT_VERSION` 0x01→0x02 | [findings/security_fix_calibration_2026-05-20.md](findings/security_fix_calibration_2026-05-20.md) |
| Mounting CRC + `restoreFromEEPROM()` buffer-overflow (2 P1 fixes) | **LANDED** (2026-05-21) — mounting-record version bumped, old blobs rejected | [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md) |
| `mega_orientation` RAM | **GREEN** at 74.5 % (F2 reclaim landed) | [findings/mega_ram_fix_2026-05-20.md](findings/mega_ram_fix_2026-05-20.md) |
| Tuner workflow (Python tuner ↔ Uno consumer) | **GREEN** — `pio run -e uno_balance` clean; stale warning resolved | [findings/tuner_format_alignment_2026-05-20.md](findings/tuner_format_alignment_2026-05-20.md) |
| Phase 4U scaffolding (Uno-minimal app + brute_tune.py + README) | **DONE** — first tuner run YELLOW: 8 s sim vs 30 s target | (next-session work: bench-validate) |
| Architecture plan + 5 audits + state reconciliation | **LANDED** (148 total findings) | [findings/architecture_plan_2026-05-20.md](findings/architecture_plan_2026-05-20.md), `findings/audit_*_2026-05-20.md`, [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) |
| Workstream B — AUTO_TUNE dead-code deletion + `held_entry_reason_` telemetry | **COMPLETE** — `uno_balance` + `mega_balance` builds SUCCESS | [archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md](archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md) |
| `json_formatter` 3 P1 bugs + new test coverage (json_formatter / calibration_storage / bootstrap_k_preservation) + build fixes | **LANDED** | [archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md](archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md) |

This roadmap describes the framework's evolution from the just-completed BNO085 + GPS + EKF stack (Phase 3) toward a multi-MCU, multi-IMU, optionally-WiFi-connected platform with a catalog of reference applications.

For project bounds and rationale, see [scope.md](scope.md).
For current actionable items, see [todo.md](todo.md).

---

## Sequencing discipline

**This project has a recurring failure mode**: skipping ahead to bench-iterate on hardcoded gains/thresholds when the planned phase work is to *eliminate the hardcoded value* in the first place. Every session that produces a "new tuned constant" instead of "a new measurement-driven replacement for a constant" is a session that regressed the universal vision.

The cure is sequencing discipline. When a phase says "implement CHARACTERISE state" and you find yourself instead changing `stiction_min_pwm = 30` to `stiction_min_pwm = 80`, **stop**. The next move is always: *(a)* land the planned measurement infrastructure, *(b)* then validate at the bench. Bench iteration *before* the infrastructure is in produces session-specific patches, not framework progress.

See [scope.md §Process doctrine](scope.md) for the full rule, the [scope.md §Current scope violations — audit](scope.md#current-scope-violations--audit-2026-05-18) for every remaining hardcoded value with its replacement plan, and [archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) for the source incidents. Phase entries below are ordered by the right sequence to follow this discipline — *do not skip ahead*.

### Top priority (2026-05-18 PM late — updated 2026-05-19 pivot)

**Strategic pivot landed 2026-05-19: the balance-bot reference application splits into two builds.** The universal/adaptive stack (BOOTSTRAP, RLS, OnlineMountingEstimator, collision detection, position containment) becomes **Mega-only** because it needs wheel encoders and flash headroom that Uno cannot host. The Uno gets a **separate minimal hardcoded program** with PID + PWM constants brute-forced offline via a new Python tuner. New roadmap structure: **Phase 4M** (Mega-universal cleanup) and **Phase 4U** (Uno-minimal + Python tuner) replace what used to be Phase 4.10c+4.11 as the active fronts. See [scope.md §Platform bifurcation](scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal) and the operator-memory note `project_strategic_pivot_2026-05-19.md`.

**Phase 4.10c — BOOTSTRAP state for K_motor identification: LANDED 2026-05-18 PM evening (commit 7a4d27f).** BOOTSTRAP measures K_motor from ±PWM pulses, derives Kp/Kd/Ki via pole-placement, pushes gains to PID before RUN. Hardcoded Kp=50/Ki=2/Kd=20 + `R` command + relay tuner all REMOVED. 7/21 scope violations retired. First bench validation (2026-05-18 PM late) showed bot transitioned IDLE → BOOTSTRAP → RUN with measured K≈0.38 but twitched and fell within ~1 s. The pivot decision came from this outcome: rather than continue debugging BOOTSTRAP on a Uno that's flash-constrained and lacks encoders, BOOTSTRAP work moves to Phase 4M (Mega) where wheel-encoder odometry can validate K_motor directly. See [findings/bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md), [archive/session_records/2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md](archive/session_records/2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md), and [archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md](archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md).

---

## Completed phases

### Phase 1 — Math foundation ✅
- Quaternion algebra (multiply, rotate, normalize, conjugate, slerp)
- Coordinate conversions (GPS ↔ ECEF ↔ NED)
- Magnetic declination math
- Snapshot recorder primitives
- **100+ unit tests passing**

### Phase 2 — GPS integration ✅
- Ublox NEO-M9N UART driver
- NMEA + (optional) UBX parsing
- Coordinate frame manager with local NED origin
- JSON output format
- GPS + orientation fusion at the output layer
- **42+ unit tests passing**

### Phase 3 — EKF sensor fusion ✅
- 16-state Extended Kalman Filter (quaternion + velocity + position + accel-bias + gyro-bias)
- State dynamics + Jacobians
- GPS measurement model + Jacobians
- Numerical stability (symmetry, covariance monitoring)
- GPS dropout / dead-reckoning handling
- **143+ tests passing total (cumulative)**

---

## Phase 4 — Auto-orientation + auto-PID + balancing-robot reference

**Goal**: Replace every hand-tuned constant with an automated capture or tuning routine. Deliver a working self-balancing robot reference application that anyone can build and have running after a single hands-off calibration session.

**Status (2026-05-12 late evening)**: **Phase 4.1–4.7 + 4.10 LANDED in firmware**. Builds clean on `arduino_uno_balancing` (99.9% flash, 78.4% RAM, 7 PlantIdentifier tests pass). Hardware validation deferred — bot not currently plugged in.

- **Sub-phases done**: 4.1 (persistent storage HAL), 4.2 (cal blob versioning), 4.3 (mounting capture), 4.4 (online estimator), 4.5 (BNO055 driver), 4.5a (PIDController), 4.5b (relay-feedback tuner — DELETED 2026-05-18 PM evening), 4.6 (BNO055 + raw-gyro accessors), 4.7 (balance app), 4.7a (state machine), 4.7b (HELD detection), 4.10 (universal RLS auto-tune), **4.10c (BOOTSTRAP K_motor measurement — LANDED 2026-05-18 PM evening)**, 2.1 (CHARACTERISE measured noise-floor), 2.5 (external-motion HELD), 2.6 (gain scheduling).
- **Designed but not coded**: 4.7c (multi-axis anomaly detector), 4.11 (multi-orientation Level 2), 2.7 (motor-null-space HELD).

See: [PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md), [archive/session_records/2026-05-12_evening_phase4_landing.md](archive/session_records/2026-05-12_evening_phase4_landing.md), [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md), [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md), [MULTI_ORIENTATION_BALANCE_VISION.md](MULTI_ORIENTATION_BALANCE_VISION.md), [AUTO_TUNING_REALITY_CHECK.md](AUTO_TUNING_REALITY_CHECK.md).

### 4.1 — Persistent storage HAL

Underlying many of the other Phase 4 items: introduce a `persistent_storage` HAL so calibration blobs work the same on AVR, Teensy, and ESP32.

- Header: `src/storage/persistent_storage.h` — `begin / read / write / commit / clear / capacity`
- Backends: `persistent_storage_avr.cpp` (uses `<EEPROM.h>`), `persistent_storage_teensy.cpp` (emulated EEPROM), `persistent_storage_esp32.cpp` (Preferences/NVS)
- Migration: refactor `src/config/calibration_storage.cpp` to call the HAL instead of `<EEPROM.h>` directly. **Fixes Known Issue KI-1**.
- Tests: round-trip read/write per backend (native test for AVR backend; hardware tests for Teensy/ESP32).

See: [findings/multi_mcu_port_strategy.md](findings/multi_mcu_port_strategy.md).

### 4.2 — Calibration blob versioning + sensor tagging

- Add a sensor-ID byte to the EEPROM header (`CAL_EEPROM_SENSOR_OFFSET`).
- Bump `CAL_FORMAT_VERSION` to invalidate any pre-existing blobs.
- On restore, refuse blobs from a different sensor and trigger fresh calibration.
- **Fixes Known Issue KI-3**.

### 4.3 — Automatic mounting-angle capture

- New module: `src/navigation/mounting_calibration.{h,cpp}`.
- Captures the gravity vector from accel (gated by gyro-stillness detection) on user trigger.
- Computes the shortest-arc quaternion from observed gravity to `[0, 0, −1]`.
- Stores as 24-byte `AutoOrientRecord` in EEPROM (magic + version + `q_mount[4]` + QC fields + CRC8).
- Replaces the .ino's manual `PITCH_OFFSET = -8.6` constant.

See: [findings/balance_point_and_mounting_research.md](findings/balance_point_and_mounting_research.md).

### 4.4 — Generic auto-PID-tuner

- New module: `src/control/auto_pid_tuner.{h,cpp}`.
- Strategy interface `ITuningStrategy` with three compile-selectable concrete strategies:
  - `USE_TUNER_RELAY`: amplitude-limited relay feedback (Åström-Hägglund 1984); default for pendulums and pure-PID loops
  - `USE_TUNER_TWIDDLE`: twiddle / coordinate-descent; fallback when relay is unsafe
  - `USE_TUNER_RLS`: recursive least-squares system-ID + analytical PID; for drones / known-model plants
- Reports tuning result (Kp, Ki, Kd, achieved phase margin, settling time) for storage to EEPROM.
- Safety-tripwire matrix per application (tilt limits, output clamps, divergence detector, user-abort).

See: [findings/auto_pid_tuning_research.md](findings/auto_pid_tuning_research.md).

### 4.5 — BNO055 driver

- New module: `src/sensors/bno055.{h,cpp}` implementing `OrientationSensor`.
- Reads quaternion via `getQuat()`, derives Euler through existing `quaternion_conversions.h` (avoids the 90° discontinuity bug in direct VECTOR_EULER).
- Supports `getSensorOffsets()` / `setSensorOffsets()` for the 22-byte BNO055 calibration blob.
- Reports 4 independent accuracies (sys/accel/gyro/mag) — also wire these up for BNO085 (**fixes KI-2**).

See: [findings/bno055_driver_and_multi_imu_strategy.md](findings/bno055_driver_and_multi_imu_strategy.md).

### 4.6 — Self-balancing robot reference application

- New tree: `src/applications/balancing_robot/`.
  - `balance_app.{h,cpp}` — top-level state machine (IDLE → CAPTURE → TUNE → RUN → SAFE_FALL)
  - `safety.{h,cpp}` — tilt limit, motor-disarm-on-tipover, watchdog
- New actuator module: `src/actuators/l298n_motor_driver.{h,cpp}` — generic dual-channel PWM motor driver
- Compile gate: `USE_BALANCING_ROBOT` in `src/config/mode.h`
- 2-state Kalman filter for the balance loop (Lauszus-style: pitch + gyro-bias), not the heavy 16-state EKF (which stays for GPS-fusion paths)
- New build env: `arduino_mega_balancing` with `-DUSE_BALANCING_ROBOT -DUSE_BNO055 -DUSE_COMMAND_ARBITRATION`

> **Design direction for 4.7**: see [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — minimize-accelerations framing, `USE_BALANCE_HELD_DETECTION` / `USE_BALANCE_FALL_DETECTION` compile switches, conservative-gain rationale.

### 4.7 — Scenario regression test

- `tests/scenario_test_balancing.cpp` — replays a synthetic pitch trajectory CSV through the new PID + auto-mounting code; asserts motor commands match the archived `.ino` reference within ±5 PWM units when given the same offset.
- Proves architectural equivalence before introducing auto-tune behavior.

### Phase 4 success metrics
- BNO055 driver compiles, swap test (BNO085 → BNO055) works on Mega
- Auto-mounting-angle saves & restores; running on Mega without hand-tuned offset
- AutoPIDTuner relay-feedback strategy compiles + passes unit tests with simulated plant
- `arduino_mega_balancing` env builds clean; scenario test passes
- All previous 143+ tests still pass
- Phase 4 completion summary doc written to `docs/phases/`

---

## Phase 4M — Mega-only universal-stack cleanup

**Goal**: Finish the universal/adaptive balance-bot vision on Mega-class hardware, free from the Uno flash ceiling that derailed the 2026-05-18 bench session. This is the home of BOOTSTRAP, RLS, OnlineMountingEstimator, collision detection, position containment, wheel-encoder odometry, and every future "the bot learns its own dynamics" feature.

**Status**: opened 2026-05-19 by the platform-bifurcation pivot. See `project_strategic_pivot_2026-05-19.md`. The detailed plan lives in [MEGA_UNIVERSAL_PLAN.md](MEGA_UNIVERSAL_PLAN.md) (landed 2026-05-19, ~340 lines); this section is the roadmap anchor.

**Working assumption**: Mega's flash budget is generous (~88 % free even with current universal stack). Optimize new code for **clarity**, not size. Don't repeat the Uno-driven micro-optimization patterns that produced unreadable `snprintf`-replacement chains.

### 4M.0 — Restore reverted collision detection — LANDED 2026-05-19 PM (VERIFIED 2026-05-20)

Late in the 2026-05-19 morning, a P0/P1 audit-fix agent inadvertently reverted the just-landed collision-detection code in `balance_app.{h,cpp}`. **Re-implemented during the 2026-05-19 PM multi-agent wave.** Three-gate detector live with constants in `balance_app.h:178-182`: PEAK 12 m/s² single-tick / SUSTAIN 8 m/s² for 3 ticks / KICK 6 m/s² with |gyro| > 200 dps. 27/27 native tests in `tests/test_balance_app_collision.cpp` pass. Detector loop at `balance_app.cpp:1639-1648` matches [findings/research_collision_signature_bno055.md](findings/research_collision_signature_bno055.md) §6 row-for-row. **2026-05-20 verification:** post-merge (commit ec4ef53), API surface and constants confirmed present in source (`balance_app.h:178-182, :549, :753`; `balance_app.cpp:906, :1645-1648`); 27/27 tests still pass — see [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §1.

### 4M.1 — Wheel-encoder hardware abstraction — LANDED 2026-05-19 PM (VERIFIED 2026-05-20)

- New module: `src/sensors/wheel_encoder.{h,cpp}` (PJRC Encoder lib added to `mega_balance` env). 17/17 native tests pass.
- Mega backend uses external-interrupt pins. Pin map fixed in `src/config/pins.h`: `L_ENC_A=18`, `L_ENC_B=19`, `R_ENC_A=2`, `R_ENC_B=3` — Uno cannot host this because it only has two external-interrupt pins, and one is needed for the BNO055 INT line.
- Per-wheel position (ticks), velocity (ticks/s), and direction.
- Reference: [findings/research_wheel_encoders_mega_2026-05-19.md](findings/research_wheel_encoders_mega_2026-05-19.md) (landed 2026-05-19 AM) for hardware recommendations and pin choices.
- Operator bring-up: [guides/encoder_bench_bringup.md](guides/encoder_bench_bringup.md) (landed 2026-05-19 PM, ~450 lines).
- **2026-05-20 verification:** header + impl + tests confirmed present after sync ec4ef53 — see [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §3.

### 4M.2 — Encoder-driven K_motor verification — LANDED 2026-05-20 (Workstream F)

- Replace BOOTSTRAP's IMU-only K_motor measurement (Δgyro / ΔPWM) with an encoder-driven version (Δwheel_velocity / ΔPWM) — direct, no plant-coupling noise.
- Keep IMU-only measurement as the fallback when encoders are absent.
- Cross-check: encoder K and IMU K must agree within a tolerance before BOOTSTRAP exits to RUN.
- **LANDED 2026-05-20** — see [findings/phase_4m2_landed_2026-05-20.md](findings/phase_4m2_landed_2026-05-20.md); audited in [findings/workstream_f_review_2026-05-20.md](findings/workstream_f_review_2026-05-20.md).

### 4M.3 — Stiction / saturation per-wheel from encoder pulses

- Phase 2 CHARACTERISE upgrade: pulse each wheel independently with ramping PWM; first PWM that produces a non-zero encoder tick = stiction floor; PWM where tick rate stops growing = saturation.
- Per-wheel asymmetry (left vs right) was speculated in [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md); encoders make it measurable.

### 4M.11 — Wheel-encoder integration into balance_app — LANDED (sampling 2026-05-19 PM; `e` cmd + EEPROM 2026-05-20, Workstream D)

- Sampling + bookkeeping wired into `balance_app::tick()`. 25 new native tests pass.
- Stall detection wired to HELD with `failure_reason = 7`.
- **LANDED 2026-05-20 (Workstream D):** operator `e` command + EEPROM wheel-encoder calibration wizard — see [findings/phase_4m11_landed_2026-05-20.md](findings/phase_4m11_landed_2026-05-20.md).

### 4M.12 — PWM auto-discovery — LANDED 2026-05-19 PM (VERIFIED 2026-05-20)

- 49 `PWM_DISC*` references in `balance_app.{h,cpp}`; new `CHAR_PWM_RANGE`-style sweep state.
- Mega flash 14.1 % → 14.7 % (+0.6 % for this feature). Plenty of room.
- Output feeds `L298NMotorDriver::stiction_min_pwm` and the Python brute-force tuner's PWM bounds.
- **2026-05-20 verification:** `PWM_DISCOVERY = 8` enum value present (`balance_app.h:84`); all 7 PWM_DISC_* constants match design — see [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §2.
- **2026-05-20:** pre-existing `-Wswitch` warning on the `PWM_DISCOVERY` case **FIXED** — see [archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md](archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md).

### 4M.13 / 4.11a — Position containment (was Phase 4.11) — LANDED 2026-05-20 (Workstream F)

The Phase 4.11 position-containment work that was queued for Uno via pitch double-integration now becomes a Mega job with **wheel-encoder odometry preferred** over IMU-only pitch double-integration.

- Full design landed in [findings/phase_4_11a_design_2026-05-19.md](findings/phase_4_11a_design_2026-05-19.md): odometry math, outer-loop cascade structure, runtime gate for encoder-vs-IMU fallback, slew limits, EEPROM persistence, bench-validation steps.
- New module (queued): `src/control/position_loop.{h,cpp}` — cascade outer loop: position error (encoder ticks) → pitch setpoint command → inner balance loop.
- Slow restoring action (~0.5 Hz bandwidth) so the bot drifts back toward the start position without fighting the balance loop.
- Runtime `USE_IMU_ONLY_OUTER_LOOP` gate selects encoder-primary vs pitch-double-integration fallback.
- Reference: 2026-05-19 operator observation that the bot wanders during testing and collides with stuff — encoder odometry is the robust fix.
- **LANDED 2026-05-20 (Workstream F)** — `position_loop.{h,cpp}` cascade outer loop landed; see [findings/phase_4m13_landed_2026-05-20.md](findings/phase_4m13_landed_2026-05-20.md).
- **Phase 4M.14 follow-on (LANDED 2026-05-20):** the outer-loop gains (`K_POS`, `K_VEL`, `POS_LEAK`) are now auto-derived at BOOTSTRAP finalise, retiring the 4M.13 hardcoded constants — see [findings/phase_4m14_landed_2026-05-20.md](findings/phase_4m14_landed_2026-05-20.md). Native test coverage wired in 2026-05-21.
- **Workstream G (bench-tuning support, LANDED 2026-05-21):** telemetry accessors + `g` serial command + `tools/plot_bench_run.py` make the cascade observable on the bench. Codeable items done; bench validation (F-3 K_VEL observation, regression-baseline capture, real-motor PWM-discovery) is hardware-gated. See [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md) and [findings/workstream_g_bench_protocol_2026-05-21.md](findings/workstream_g_bench_protocol_2026-05-21.md).

### 4M.k — K-quality + audit-fix follow-through

The P0/P1 audit fixes landed in this session (gyro torn-read atomicity, `plant_id_.reset()` no-overwrite, K-quality gate, baseline cap, BOOTSTRAP per-pulse telemetry) all apply to the Mega-universal code path. Continue retiring the remaining 14 audit rows in [scope.md §Current scope violations — audit](scope.md#current-scope-violations--audit-2026-05-18-updated-pm-evening-phase-410c-landed-re-tagged-2026-05-19-for-platform-bifurcation) — each one becomes addressable once the bot balances long enough on Mega to collect the derivation data.

### Phase 4M success metrics

- Mega build with full universal stack + wheel encoders + collision detection compiles and flashes
- BOOTSTRAP K_motor agrees IMU vs encoder within tolerance
- Bot balances for >60 s without collision-detector false positives
- Position containment keeps the bot within a configurable radius of the start point for >2 min
- Remaining audit rows in scope.md drop below 5

---

## Phase 4U — Uno minimal hardcoded balancer + Python brute-force tuner

**Goal**: A small, single-purpose Uno program that does one thing — balance — using hardcoded PID + PWM constants generated by an offline Python tuner. **No on-MCU learning. No auto-tune. No BOOTSTRAP.** The reference target is `archive/balancing_robot_reference/SelfBallancingRobot3.ino`. When the operator wants to re-tune (new battery, new wheels, new surface), they run the Python tool offline and reflash.

**Status**: opened 2026-05-19 by the platform-bifurcation pivot. Source scaffold landed in [`src/applications/balancing_robot_uno/`](../src/applications/balancing_robot_uno) (4 files, commit c3c0c6b) and Python tooling in [`tools/sim/`](../tools/sim) (`brute_tune.py` + `balance_constants_template.h.in`, same commit). See [verification_2026-05-19.md](findings/verification_2026-05-19.md) for the post-landing build/test matrix.

**2026-05-20 status:** Scaffolding **DONE** — Uno-minimal app exists, `brute_tune.py` exists, README workflow doc landed today. Tuner ↔ Uno consumer format alignment confirmed clean (`pio run -e uno_balance` succeeds — the prior "format mismatch" warning in the state reconciler was stale; see [findings/tuner_format_alignment_2026-05-20.md](findings/tuner_format_alignment_2026-05-20.md)). First tuner run is **YELLOW**: 8 s balanced in sim vs 30 s target. Next-session task: **bench-validate** the tuner output on hardware and iterate the plant model where the sim under-predicts performance.

### 4U.1 — Source scaffold

- New tree: `src/applications/balancing_robot_uno/`
  - `balance_uno_app.{h,cpp}` — read pitch, run PID, drive motors. ~150 LOC ceiling.
  - Consumes a generated header `balance_constants.h` (landed in `src/applications/balancing_robot_uno/balance_constants.h`, regenerated by `tools/sim/brute_tune.py`).
- Reuses existing `src/sensors/bno055.cpp`, `src/actuators/l298n_motor_driver.cpp` — no new sensor or actuator code.
- Compile gate: `USE_BALANCING_ROBOT_UNO` (mutually exclusive with `USE_BALANCING_ROBOT`).

### 4U.2 — Build environment

- New env (or repurpose `uno_balance`): `arduino_uno_minimal` targeting Uno with `USE_BALANCING_ROBOT_UNO` flag.
- Aggressive flash trimming is unnecessary because the universal stack is absent — the program should fit easily under 60 % flash.

### 4U.3 — Python brute-force tuner

- Lives in `auto_orientation/tools/sim/brute_tune.py` (landed 2026-05-19 commit c3c0c6b; see [tools/sim/README.md](../tools/sim/README.md)).
- Wraps existing `balance_bot_sim.py` plant model.
- Grid-search or evolutionary search across PID gain space + PWM scaling space.
- Fitness = time balanced before tip-over under randomized disturbance injection.
- Output: a generated header `balance_constants_uno.h` with `constexpr` Kp/Ki/Kd + stiction_min_pwm + saturation_pwm.

### 4U.4 — Header generator + Uno-side consumer

- Tool emits the header on every tuner run; build script copies it into `src/applications/balancing_robot_uno/generated/`.
- Uno code includes it directly — no JSON, no runtime config, no EEPROM.

### 4U.5 — First brute-force run + bench validation

- Run the Python tuner to convergence on the simulated plant.
- Flash Uno, prop bot upright, release.
- Pass criterion: bot balances ≥ 30 s on a flat indoor surface without operator intervention.

### Phase 4U success metrics

- Uno minimal build at <60 % flash with full balance program
- Python tuner produces stable gains in <10 min of search
- Bot balances ≥ 30 s on first bench try after fresh tune
- Re-tune workflow is documented in `docs/applications/balancing_robot_uno/README.md`

---

## Phase 5 — Multi-MCU port

**Goal**: Support Nano, Mega, Teensy 4.0, Teensy 4.1, ESP32, ESP32-S3 from a single source tree. Make any feature optional based on what the target MCU can support.

**Status**: Plan documented; implementation pending Phase 4 completion.

### 5.1 — Pin assignment split

- Refactor `src/config/pins.h` into a dispatcher + per-platform files (mirroring `flight_controller/include/pin_definitions.h`).
- Per-platform: `pins_avr.h`, `pins_teensy.h`, `pins_esp32.h`.
- Critical pins documented per MCU: I2C SDA/SCL (primary + secondary bus where present), Serial1, BNO085 INT/RST, SPI CS for SD card.

### 5.2 — Teensy 4.0 / 4.1 ports

- New envs: `teensy40`, `teensy40_calibration`, `teensy41`, `teensy41_calibration`.
- Use emulated EEPROM via the new HAL.
- Move EKF and Madgwick filter to FPU floats — expected 20-100× lift over Mega soft-float.
- Teensy 4.1 only: SD card via SDIO at full speed.

### 5.3 — Arduino Nano (budget build)

- New envs: `arduino_nano`, `arduino_nano_calibration`.
- Disabled features: EKF (too much RAM), SD card, snapshot recorder, fast GPS, verbose logging.
- MPU6050 + Madgwick only — see Phase 5.5.
- Target: minimum-viable educational kit, classroom-grade.

### 5.4 — ESP32 + ESP32-S3 ports (no WiFi yet)

- New envs: `esp32_dev`, `esp32_dev_cal`, `esp32s3`, `esp32s3_cal`.
- Persistent storage via Preferences/NVS backend of the HAL.
- Dual-core foundation: imu/ekf on core 0, gps/output on core 1 (FreeRTOS queues + `xQueueOverwrite` depth-1 pattern from flight_controller).
- WiFi opt-in is Phase 6.

### 5.5 — MPU6050 + external magnetometer stack

- New modules: `src/sensors/mpu6050.{h,cpp}`, `src/sensors/external_magnetometer.{h,cpp}` (abstract) + `hmc5883l.{h,cpp}`, `qmc5883l.{h,cpp}`, `lis3mdl.{h,cpp}`.
- New fusion adapter: `src/sensors/fused_imu.{h,cpp}` — implements `OrientationSensor` by running Madgwick on raw gyro+accel+mag.
- Magnetometer ellipsoid calibration: capture in firmware, fit on host via `tools/auto_calibrate.py`, upload back.
- WMM-2025 (or coarse city-table) magnetic-declination lookup for true-north heading.

See: [findings/mpu6050_external_mag_pipeline.md](findings/mpu6050_external_mag_pipeline.md) (landed 2026-05-12).

### 5.6 — Multi-MCU CI matrix

- `tools/build_matrix.sh` — wraps `pio run -e <env>` for every env, summarizes flash/RAM use.
- GitHub Actions workflow (or local equivalent) — compiles every env on every push.
- Catches "this header doesn't compile on AVR" early.

See: [findings/test_infrastructure_expansion.md](findings/test_infrastructure_expansion.md) (landed 2026-05-12).

### Phase 5 success metrics
- Clean compile on all 6 MCU families
- Flash/RAM usage report committed to repo per env
- Persistent-storage HAL round-trip test passes on all backends
- MPU6050 + magnetometer stack passes scenario test against recorded BNO085 reference

---

## Phase 6 — WiFi telemetry + browser dashboard (ESP32 family only)

**Goal**: On ESP32/ESP32-S3 builds, expose a browser dashboard that handles calibration, balance-point capture, auto-PID-tune progress visualization, and OTA — over WiFi, with no serial cable.

**Status**: Designed; implementation pending Phase 5.

### 6.1 — WiFi manager

- `src/network/wifi_manager.{h,cpp}` — STA mode, AP-fallback for first-time setup, mDNS hostname (`autoorient-XXXX.local`)
- `src/network/wifi_credentials.h` — placeholder template, gitignored at runtime
- Compile gate: `USE_WIFI` (auto-cascades from `USE_ESP32` + a build flag, mirroring `flight_controller/`)

### 6.2 — Web + API server

- `src/network/web_server.{h,cpp}` — static HTML/JS/CSS from LittleFS
- `src/network/api_server.{h,cpp}` — REST endpoints + WebSocket stream
- Endpoints aligned with `swarm_api/` contract: `GET /api/status`, `POST /api/commands`, `WS /ws`
- Plus framework-specific: `POST /api/calibration/{start,capture,save}`, `POST /api/pid_tune/{start,abort}`, `GET /api/pid_tune/status`
- Auto-cascades from `USE_WIFI`: `USE_WEB_SERVER`, `USE_API_SERVER`

### 6.3 — Browser dashboard

- Single page, vanilla HTML/JS + Three.js (vendored ~150 KB)
- Pages: home (live orientation 3D), calibrate (magnetometer wizard), balance-capture, pid-tune, telemetry, ota, settings
- Stored in LittleFS partition; uploaded via `pio run --target uploadfs`
- Mobile-friendly layout for hands-on calibration UX

### 6.4 — OTA updates

- `src/network/ota.{h,cpp}` — ArduinoOTA + HTTP-pull fallback
- Compile gate: `USE_OTA` (auto-cascades from `USE_WIFI`)

### Phase 6 success metrics
- ESP32 build joins WiFi and is reachable at mDNS hostname
- Live orientation visible in browser at 30 Hz
- End-to-end calibration via browser (no serial cable)
- OTA update from browser succeeds
- Dashboard works in landscape and portrait on mobile

See: [findings/wifi_telemetry_integration_design.md](findings/wifi_telemetry_integration_design.md), [findings/browser_dashboard_architecture.md](findings/browser_dashboard_architecture.md) (both landed 2026-05-12).

---

## Phase 7 — Application catalog expansion

**Goal**: Beyond the balancing-robot reference, add reference applications that exercise the framework in different ways. Each lives under `src/applications/<app>/` and is gated by `USE_<APP>`.

**Status**: Designed; ordering will be set when Phase 4-6 work concretizes.

### 7.1 — Multirotor bridge (I2C slave to `flight_controller/`)
- `src/applications/multirotor_bridge/`
- Exposes orientation + cal status to flight_controller over I2C (using flight_controller's existing arbitration patterns)
- Use case: external sensor head when flight_controller's onboard IMU is constrained

### 7.2 — Camera mount / 2- or 3-axis gimbal
- `src/applications/camera_mount/`
- Sub-modules: `gimbal_2axis.cpp`, `gimbal_3axis.cpp`
- Servo or brushless motor outputs; orientation-feedback closed loop
- Zero-axis pointing-direction calibration UX

### 7.3 — Photogrammetry / 3D-scanner snapshot rig
- `src/applications/photogrammetry/`
- Triggers snapshot recorder + GPS capture per button press
- Stores quaternion + position metadata in JSON per image

### 7.4 — Educational kit (Nano + MPU6050)
- `src/applications/edu_kit/`
- Minimum-viable build: pitch/roll on a 4-line OLED or via serial
- Designed for classroom use; emphasis on documentation, not features
- Companion docs: a beginner-friendly walkthrough in `docs/guides/`

See: [findings/application_catalog.md](findings/application_catalog.md) (landed 2026-05-12).

---

## Phase 8 — Advanced applications

**Goal**: Applications that stretch the framework in new directions. Not committed; depends on Phase 4-7 outcomes.

- AR/VR head tracker (latency-critical, ESP32-S3 + BLE?)
- Robot-arm end-effector pose feedback
- Autonomous surface vehicle attitude (marine; rugged calibration)
- Advanced VTOL transition tracking (back-feed to flight_controller VTOL work)

---

## Cross-cutting work (parallel to all phases)

### Test infrastructure
- HIL harness for balance-robot (cost/value evaluation in Phase 5)
- Scenario test catalog (one per application + one per known-failure mode)
- Pre-merge checklist doc

### Tooling
- `tools/replay_trajectory.py` — feed recorded CSV to firmware over serial
- `tools/auto_calibrate.py` — host-side magnetometer ellipsoid fit
- `tools/quaternion_viewer.py` — desktop 3D viewer (pre-Phase-6 fallback)
- `tools/balance_tune_visualizer.py` — auto-PID-tune convergence plot
- `tools/build_matrix.sh` — multi-MCU compile wrapper

### Documentation
- Per-application user guides in `docs/guides/`
- Per-finding "what we learned" follow-up notes
- Living session records in `docs/archive/session_records/YYYY-MM-DD_topic.md`

---

## Known issues to track

See [scope.md](scope.md#known-issues-active-as-of-2026-05-12). Currently:

- KI-1: `EEPROM.h` does not persist on ESP32 (fixed by Phase 4.1)
- KI-2: BNO085 driver collapses 4 cal accuracies to 1 (fixed by Phase 4.5)
- KI-3: Calibration blob format lacks sensor tag (fixed by Phase 4.2)
- KI-4: Doc drift in roadmap/todo (addressed in this 2026-05-12 session)

---

## Out of band (research / not committed)

These show up in agent findings but are not yet on a phase plan. Promoted to a phase when the case for them is clear.

- Eigen / `BasicLinearAlgebra` for EKF math on ESP32-S3 / Teensy 4.x
- ICM-20948 / LSM9DS1 driver
- BLE telemetry (lower-latency alternative to WiFi for AR/VR head tracking)
- Online (continuous) magnetometer recalibration
- Magnetic-anomaly detection (warn user when local field is corrupted)
- Multi-rate filter banks (raw IMU at 1 kHz on ESP32-S3 + downsample to 200 Hz for control)

---

*Last updated: 2026-05-21 (Mega cascade Phases 4M.2 / 4M.11 / 4M.13 / 4M.14 confirmed LANDED — Workstream F complete; Workstream G bench-tuning support codeable items landed; Phase 4M.14 native test coverage wired in; two P1 calibration-storage security fixes (mounting CRC + `restoreFromEEPROM()` buffer-overflow) landed; all 6 firmware envs build clean, native suite 17/17. All uncommitted. Bench-gated next steps: F-3 K_VEL observation, regression-baseline capture, real-motor PWM-discovery. Full session record: [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md)). When a phase enters or exits, update both this file and `todo.md`. Per-session work logs go to `docs/archive/session_records/`.*
