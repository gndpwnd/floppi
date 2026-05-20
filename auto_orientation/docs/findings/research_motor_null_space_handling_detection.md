# Motor-Null-Space Handling Detection (Phase 2.7)

Status: RESEARCH — supports [operator_ideas_backlog.md row 12](operator_ideas_backlog.md). The current HELD detector uses body-frame `sqrt(gx² + gz²)`. This proposal replaces it with a projection test in the *motor-null acceleration subspace* — directions a 2-wheel inverted pendulum cannot accelerate via wheel torque. Motion there, with idle motor command, is necessarily externally imposed. The framework is universal: nothing about mounting orientation may be hardcoded.

---

## 1. Flight-controller prior art

**Innovation gating already exists in this repo** — applied to the wrong subsystem. The 16-state EKF computes `innovation = z_measured − h(x_predicted)` at `src/navigation/measurement_model.cpp:128-152` and `compute_innovation_magnitude()` at `src/navigation/covariance_manager.cpp:149-152`. The header comment notes verbatim *"Large innovation indicates outlier (bad GPS fix) or filter divergence."* The motor-null-space detector is the same shape applied to motor-command vs IMU-acceleration instead of GPS-vs-prediction.

**dRehmFlight has none.** `flight_controller/src/control.cpp:107-153` is plain PID with `integral = constrain(integral, -I_LIMIT, I_LIMIT)`. No GLR, no residual, no fault detector. Safety is operator-managed: arm via channel 5 (`control.cpp:343-379`). Betaflight/Cleanflight follow the same philosophy (per `research_flight_controller_pid_lessons.md` §4.1). **ArduPilot** is the only mainstream FC with formal fault detection; it uses EKF-innovation-magnitude gates exactly like our `navigation/` code already does. Lesson: copy the EKF-innovation pattern that lives next door to the balance app, not the bare-PID pattern from dRehmFlight.

---

## 2. BNO055 Linear-Acceleration register

The fusion engine subtracts estimated gravity from raw accel and outputs body-frame gravity-removed acceleration at registers `0x28–0x2D` (X/Y/Z LSB+MSB). Source: `Adafruit_BNO055.h:140-145`, datasheet §3.6.5.

- **Scaling**: 100 LSB = 1 m/s² (same as raw accel) — `Adafruit_BNO055.cpp:445-450`.
- **Axes**: Body frame, +X forward, +Y left, +Z up on Adafruit breakout (`bno055.h:60-67`).
- **Mode requirements**: Any fusion mode produces LIA — `NDOF`, `NDOF_FMC_OFF`, `IMUPLUS`. **`AMG` does NOT produce LIA.** If the magnetometer is unreliable, the right "no-mag" mode is `IMUPLUS` (gyro+accel fusion only), not `AMG`.
- **Latency**: ~20-40 ms group delay (same fusion filter as the quaternion, per `latency_budget_2026-05-12.md`). Raw accel is faster (~5 ms) but includes gravity, which would have to be subtracted by rotating world-gravity through the fused quat — same latency, more code.
- **API call**: `bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL)`. Returns `imu::Vector<3>` in m/s². The existing wrapper `bno055.cpp:184-193` (raw accel) is the template; adding `getLinearAccel(float xyz[3])` costs ~30 B plus one virtual in `sensor_base.h`. **Not currently wrapped — Phase 2.7 step 1.**

---

## 3. Algorithm

State (slowly-learned per chassis):

```text
body_heading_unit  ∈ R³, |·|=1   // forward in body frame
body_up_unit       ∈ R³, |·|=1   // gravity-anti in body frame
```

Each tick (200 Hz):

```text
1. a_lin = getLinearAccel()          // body frame, gravity-removed
2. ω     = getRawGyro()              // body frame, deg/s

3. # Motor-controllable acceleration is along heading; project it out:
   a_along    = (a_lin · body_heading_unit)
   a_residual = a_lin − a_along · body_heading_unit       // null-space

4. # Motor-controllable angular rate is pitch (about wheel axle):
   body_pitch_axis = body_heading_unit × body_up_unit     // axle direction
   ω_pitch = ω · body_pitch_axis
   ω_null  = ω − ω_pitch · body_pitch_axis                // roll + yaw

5. alarm = sqrt(|a_residual|² + k_g · |ω_null|²)
```

HELD trigger: `alarm > A_THRESH` for ≥150 ms while `|cmd_output_pwm| < CMD_QUIET`. Dwell-timer + `CMD_QUIET` gate are borrowed directly from `balance_app.cpp:394-405`.

**Why this captures the operator's intent.** A 2-wheel bot's only motor-explainable accel is along heading; only motor-explainable angular rate is pitch. Lateral translation, vertical lift, roll, yaw are *all* null-space. Critically, the projection uses *learned* body-frame vectors, not the chip's nominal axes — mounting skew and non-level ground drop out as long as §4 converges. In a perfectly-mounted upright bot, `body_heading_unit ≈ (1,0,0)`, `body_up_unit ≈ (0,0,1)`, `body_pitch_axis ≈ (0,1,0)`. The learner converges toward whatever it actually is.

---

## 4. Learning `body_heading_unit` from operating data

When the bot commands forward thrust, it accelerates along heading. Average that direction.

Trigger condition (all must hold): bot in RUN, `|cmd_output_pwm| > 80` (well above the ~30 PWM stiction floor from `2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md`, avoiding stiction-band samples), `|pitch_deg| < 5°` (gravity-projection error stays small), `|ω_null| < 30 deg/s` (no handling underway), bot not upside-down (gate via `body_up_unit · world_z > 0`).

Update rule (slow exponential mean):

```text
sign = sign(cmd_output_pwm)
a_dir = a_lin / |a_lin|
body_heading_unit = (1−α) · body_heading_unit + α · sign · a_dir
body_heading_unit /= |body_heading_unit|
```

Time constant: α = 0.001 → ~5 s half-life at 200 Hz. Mounting is geometric and slow-varying — never needs sub-second tracking.

**`body_up_unit` comes free** from the fused quaternion: rotate world-gravity through `quat_world_to_body` each tick. The quaternion is already the slow-learner.

**Persistence.** Save `body_heading_unit` to EEPROM every 60 s during RUN, next to the mounting offset (`src/config/calibration_storage.cpp`). Reload on boot. Recompute from scratch if `cal_accel` or `cal_gyro` drops below 2 (sensor degraded → don't trust prior). Mirrors the operator preference at [feedback_balance_bot_preferences.md] — *measured plant data persists; tuned controller state does not.*

**Cold-start gate.** Until 30 s of valid heading-update samples have accumulated, fall back to the legacy `sqrt(gx² + gz²)` detector. Same belt-and-braces pattern as `multi_axis_anomaly_handling_detection.md` §5.

---

## 5. Cheap alternative — "motion without motor command" (backlog row 9)

Operator's Phase 2.5 heuristic: `|cmd_output_pwm| < 20 for ≥100 ms AND |gyro_pitch_dps| > 30 ⇒ FALLEN`. Same physical idea applied in body frame without projection math.

**Cheap version is sufficient when:**

- BNO055 mounted within ~10° of true vertical on roughly-level floor. Body-frame `gyro.y` ≈ true pitch axis; lateral leakage during recoveries is small.
- The handling event is a fall (large pitch rate + idle motors) — exactly what the cheap rule was designed for.

**Cheap version fails when:**

- BNO055 mounted at non-trivial angle (operator explicit: *"the BNO055 will never be completely level"*). Body-frame `gyro.x`/`gyro.z` then contain a constant slice of true pitch motion; recovery transients leak into the lateral channel and false-trip HELD. This is the failure mode observed at the bench (35 HELD vs 12 RUN samples in 30 s).
- Floor non-level — same leakage, different cause.
- Operator picks bot up *without rotating* (pure vertical lift). Cheap rule sees no pitch → no detection. Projection rule sees `a_residual_z ≫ 0` → fires.
- Operator restrains without moving. Both `gyro.y` and `cmd_output` are quiet; projection rule still sees the operator decelerating gravitational fall on `a_residual`.

**Recommendation:** ship the cheap rule first (Phase 2.5, ~30-50 B). Use the projection rule (Phase 2.7) as a strict superset once Phase 2 flash savings land. The cheap rule survives in code as a low-cost guard during the cold-start window before `body_heading_unit` converges.

---

## 6. Flash + RAM cost on Uno

Order-of-magnitude (verify with `pio run -e arduino_uno_balancing` size report after implementing):

| Item | Flash | RAM |
|------|-------|-----|
| `getLinearAccel()` wrapper (6-byte I²C burst + scaling) | ~30 B | 0 |
| Three unit vectors: heading, up, pitch-axis (3 floats each) | 0 | 36 B |
| Heading update rule (renormalise + 3-mult + 3-add) | ~80 B | 0 |
| Two vec3 dot products + scalar-vec projection per tick | ~60 B | 0 |
| `sqrtf` for residual magnitude (libm already linked) | ~0 B | 0 |
| HELD threshold + dwell counter | ~30 B | 4 B |
| **Subtotal** | **~200 B** | **~40 B** |

Uno currently has 258 B flash free (`2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md` §"Phase 2 — re-plan"). 200 B is *barely* feasible *if* the legacy 2-signal detector is removed simultaneously (saves ~100 B). Realistic path:

- **Uno**: only viable after Phase 2 flash savings land (~150 B from removing `r` command, packing `s` status, dropping the periodic mount-save). Until then, ship Phase 2.5 cheap rule.
- **Teensy 4.0 / ESP32-S3**: trivial; >900 KB free. Implement the full projection detector unconditionally.

**Bottom line: Phase 2.7 is feasible on Uno only after Phase 2 lands.** Don't fight the flash budget twice in one session. Teensy/ESP32 should get the full detector immediately; Uno catches up once headroom is available.

---

## 7. Phase 2.7 implementation plan

1. Wrap `VECTOR_LINEARACCEL` in `src/sensors/bno055.{cpp,h}` as `getLinearAccel(float xyz[3])`. Add no-op virtual default in `sensor_base.h` next to `getRawGyro`/`getRawAccel`.
2. Header-comment the NDOF / IMUPLUS / FMC_OFF compatibility, explicit note: **AMG mode does NOT produce LIA** — fall back to `IMUPLUS` if mag is disabled, not `AMG`.
3. Add `body_heading_unit` learner + EEPROM persistence (next to mounting offset, save every 60 s during RUN). Skip the save call if Phase 2 flash work hasn't freed budget.
4. Implement the projection detector behind a new `USE_BALANCE_HELD_PROJECTION` flag in `balance_app.cpp` `step_run_()`. Keep `USE_BALANCE_HELD_DETECTION` (lateral-gyro) as cold-start fallback.
5. Cold-start handover: lateral-gyro detector until 30 s of valid heading-update samples accumulated; then switch to projection.
6. Tune `A_THRESH` on the bench. Log `|a_residual|`, `|ω_null|`, `alarm` during normal RUN, slow-lift, fast-grab, and fall events. Pick threshold at 99th percentile of RUN-only distribution.
7. Unit-test the projection math in `tests/test_held_state_machine.cpp` with synthetic streams: pure pitch (must NOT trigger), pure lateral lift (must trigger), 30°-mounted pure pitch (must NOT trigger — proves universality).
8. Keep `multi_axis_anomaly_handling_detection.md` (Welford/Mahalanobis) deferred to Phase 4.7c. It is a structurally different design — learned feature-space novelty vs. known physics projection. Both have a place; projection is cheaper and more interpretable, so go first.

---

## See also

- [operator_ideas_backlog.md](operator_ideas_backlog.md) row 12 — the index entry that triggered this research.
- [balance_held_fallen_state_machine.md](balance_held_fallen_state_machine.md) — current lateral-gyro detector; this proposal replaces §3 of that doc.
- [multi_axis_anomaly_handling_detection.md](multi_axis_anomaly_handling_detection.md) — sibling proposal using learned multi-axis statistics. Complementary, not competing.
- [research_flight_controller_pid_lessons.md](research_flight_controller_pid_lessons.md) §7d — confirms dRehmFlight has no command-residual reasoning.
- `src/navigation/measurement_model.cpp:128-152` — existing innovation-residual primitive (GPS). Same shape, different subsystem.
- `src/sensors/bno055.cpp:184-204` — template for the new `getLinearAccel()` wrapper.
- [`docs/archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md`](../archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md) §"HELD-detection refinement (operator-proposed)" — where the operator articulated this design.
- Bosch BNO055 datasheet §3.6.5 — Linear Acceleration register map and definition.
