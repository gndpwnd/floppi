# Universal Self-Balancing Robot — The Vision
Last updated: 2026-05-12

## The user's framing

> "I am trying to make it so that we don't need to know config values for motors or robot weight or center of gravity or mass and stuff. I just want universal code to plop onto any self-balancing robot with a calibrated IMU and magnetometer. And make the magnetometer optional."

That is the design north star. Everything else flows from it.

## What "universal" means concretely

Drop the firmware on any bot built from:

- Two independently-driven wheels (any motor type, any driver chip).
- A 6-DoF IMU (accelerometer + gyroscope), calibrated.
- Optionally a magnetometer for yaw — but not required for balancing.
- Any chassis geometry, any mass, any wheel diameter, any center of gravity.

…and the firmware figures out the rest. No `MASS_KG = 0.45f`. No `CHASSIS_HEIGHT_M = 0.12f`. No `WHEEL_RADIUS_MM = 32`. No `MOTOR_KV`. None of it. The bot identifies its own dynamics from operating data.

## Why this is achievable

Self-balancing dynamics reduce to one degree of freedom (pitch) plus one control input per wheel (PWM). The only quantities a controller actually needs are:

1. **The mapping from PWM to angular acceleration about the wheel axis** — `α_pitch ≈ K_motor * pwm_total − g_eff * sin(pitch)`. K_motor and g_eff are scalars.
2. **The mounting offset between sensor body-frame and chassis-balance-point** — already solved by `OnlineMountingEstimator` (Phase 4.4) and `MountingCalibration` (Phase 4.3).
3. **The IMU's measurement frame** — solved by BNO055 calibration (saved to EEPROM) and the framework's coordinate-frame layer.

Mass, height, CoG, wheel radius all collapse into K_motor and g_eff. We don't need them individually — we need their downstream consequence on the plant, which is just two numbers.

**Those two numbers are learnable from operating data with the IMU alone.** That's what `dynamic_pwm_accel_learning.md` (Phase 4.10) implements via scalar recursive least squares.

## The roadmap that delivers it

Phase | Title | What it contributes to universality
---|---|---
4.3 | `MountingCalibration` | Removes need to pre-measure IMU mounting angle. ✅ done.
4.4 | `OnlineMountingEstimator` | Removes need for static balance point — tracks drift (battery, payload, wear). ✅ done.
4.5 | `PIDController` + `AutoPIDTuner` (relay feedback) | Removes need for per-bot hand-tuning of Kp/Ki/Kd. ✅ done; relay tuner present.
4.7 | Balancing-robot reference app | The application that uses all of the above. ✅ in iteration.
4.7c | Multi-axis anomaly detector (`multi_axis_anomaly_handling_detection.md`) | Removes need for hand-coded HELD thresholds; learns the balance-motion manifold during operation. 🔬 designed, not implemented.
4.10 | Dynamic PWM→acceleration learning (`dynamic_pwm_accel_learning.md`) | Removes need to know motor characteristics; identifies K_motor online via scalar RLS. 🔬 designed, not implemented.

When 4.7c and 4.10 land, the bot is **truly universal**: it boots, observes its own dynamics for a few minutes, and self-tunes. Currently we're at "universal *after* a one-time auto-tune button-press" — the relay-feedback tuner exists but the operator has to explicitly trigger it.

## Magnetometer-optional design

The control loop NEVER needs magnetometer data. Pitch comes from gravity + gyro fusion (NDOF on BNO055; would be IMU-mode on BNO055, or Madgwick on raw 6-DoF for other sensors). Yaw IS magnetometer-derived, but yaw doesn't enter the balance equation.

The framework already handles this: `OrientationSensor::getOrientation()` returns pitch regardless of magnetometer presence. Drivers without a magnetometer simply report yaw as undefined. Balance code uses pitch only — no change needed.

For sensors that have a magnetometer but you want to skip its calibration: add a `MAG_OPTIONAL=1` build flag (TBD) so the calibration wizard accepts gyro=3 AND accel=3 without waiting for mag=3. The current BNO055 wizard already does this — it's the right behavior.

## What the operator interacts with

Two things, exactly:

1. **One-time sensor calibration.** Wave the bot through some poses. ~1 minute. Saved to EEPROM. Never repeated unless the sensor is swapped.
2. **One-time mounting capture.** Prop the bot at its natural balance. Press the button. ~2 seconds. Saved to EEPROM.

After that: power on, prop upright, release. Bot balances. No config files. No tuning UI. No magic numbers.

## What the operator should NEVER have to interact with

- Mass, weight, center of gravity, moment of inertia.
- Wheel radius, motor Kv, gearbox ratio.
- Battery voltage (auto-detected if voltage divider is present; otherwise auto-compensated by online K_motor estimator).
- PID gains.
- Filter time constants.
- State-machine thresholds.

If any of those appear in user-visible config, the universality goal is leaking. Treat that as a bug, not a feature.

## What's true RIGHT NOW (2026-05-12)

We are **partially there**:

- ✅ Mass / height / CoG / wheel radius — not present in user-visible config.
- ✅ Motor characteristics — not present in user-visible config (just `stiction_min_pwm` which is universally ~20-25 for cheap brushed motors via L298N).
- ✅ Magnetometer — already optional.
- ⚠️ PID gains — hardcoded `Kp=50 Ki=5 Kd=15` defaults. Hand-tuned per session. **Phase 4.10 fixes this.**
- ⚠️ HELD / FALLEN thresholds — currently hand-coded; **Phase 4.7c replaces with learned manifold**.
- ⚠️ Tilt limit — hardcoded 10°. Could be removed entirely (operator preference: "balance forever, no FALLEN") — and that's the current default build.

## The bridge between vision and now

Until Phase 4.10 lands, the bot DOES have one per-bot "config value": its hand-tuned PID gains. Three ways to deal with this in the meantime:

1. **Document tuning as the only manual step.** Operator does `t` (auto-tune via relay feedback) once on a new bot, gains live in EEPROM for the session, gone on reboot. Per-boot tuning = manual.
2. **Persist gains to EEPROM anyway.** Violates the "dynamic gains" preference but gets us closer to "plop on and go". Strong objection on file (memory entry).
3. **Implement Phase 4.10 now.** ~2 days of work. Real fix.

The recommended path: option 1 today, option 3 within a week. Don't do option 2 — it's a local optimum that blocks the real solution.

## Why we're documenting this NOW

The 2026-05-12 evening session kept iterating tactically — bump Kp, drop Kd, lower stiction, raise cap. Without an explicit "universal" goal each change risked being a local fix that hardcoded another assumption. This document is the goal post that future tactical changes get measured against.

## See also

- [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — the controller-level companion to this document.
- [findings/dynamic_pwm_accel_learning.md](findings/dynamic_pwm_accel_learning.md) — the implementation plan for the "robot learns its own dynamics" piece.
- [findings/multi_axis_anomaly_handling_detection.md](findings/multi_axis_anomaly_handling_detection.md) — same idea for handling detection.
- [findings/auto_pid_tuning_research.md](findings/auto_pid_tuning_research.md) — the existing relay-feedback auto-tuner (already implemented; operator just needs to press `t`).
- [MASTER_DESIGN.md](findings/MASTER_DESIGN.md) — the overall framework design that this vision sits on top of.
