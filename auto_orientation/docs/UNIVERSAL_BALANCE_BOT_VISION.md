# Universal Self-Balancing Robot — The Vision
Last updated: 2026-05-18

## Control philosophy: "more / less" — not "set to N"

The single sharpest reframe of this project. The controller does **not** reason in absolute units. It reasons in **deltas**:

- *"need more torque than I'm giving"* — increase PWM toward the saturation limit
- *"need less torque than I'm giving"* — decrease PWM toward zero or reverse
- *"need to reverse direction"* — cross zero

A specific number ("set PWM to 80") is never the right framing in production. The number only matters as a **landmark on a map** that the bot learned during calibration:

- **Stiction floor** = the smallest PWM that produces ANY wheel motion. Below it, "more" of zero is still zero — the controller knows to skip past it.
- **Saturation point** = the PWM beyond which response stops growing. Above it, "more" doesn't help — the controller knows to give up and rely on geometry.
- **K_motor** = the local slope of "PWM in" vs "angular acceleration out." Tells the controller how much "more" actually buys you.

These three numbers — stiction floor, saturation point, K_motor — are the bot's **map of its own actuator**. CHARACTERISE (Phase 2) and the RLS plant identifier (Phase 4.10) discover them. The active balance loop just navigates within them via gradients.

### What this means in code

| Old (absolute) framing | New (delta) framing |
|---|---|
| `stiction_min_pwm = 80` | "if commanded PWM < stiction_floor, snap to stiction_floor in the commanded direction" |
| `Kp = 65` | "current Kp produced overshoot — try less / try more" (RLS adapts) |
| `tilt_limit_deg = 8` | "tilt is past the operating envelope the bot has demonstrated balance over" |
| `HELD_threshold = 90 dps` | "lateral motion exceeds the motor-null-space residual we've established as normal" |

The right side never has a hardcoded literal. It refers to a *measured* quantity, learned by the bot.

### Why this matters operationally

When the bot oscillates wildly (a real failure mode the 2026-05-18 bench session caught), the diagnosis is NEVER "the gain value is wrong." It's:

- *"the controller is asking for more than the actuator can deliver"* (saturation: gain too high, can't catch fall) — solution: ramp gain DOWN
- *"the controller is asking for less than the plant requires"* (under-response: gain too low, bot tips before output engages) — solution: ramp gain UP
- *"the controller doesn't know where balance actually is"* (mount offset stuck) — solution: estimator needs better evidence integration

The RLS-based plant identifier (Phase 4.10) implements exactly this loop: observe what the plant does in response to commands, adjust gain *targets*, rate-limit-ramp live gains toward those targets. Never set a number from outside; always derive it from the gradient of response-to-command.

---

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
- [findings/operator_ideas_backlog.md](findings/operator_ideas_backlog.md) — durable cross-reference index of all operator-suggested ideas (the table here is a snapshot; that file is the index).

---

## 2026-05-18 additions

Reinforced and extended during the bench session ([session record](archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md)). Three new operator-proposed ideas joined the design backlog. Each is tracked in detail in [findings/operator_ideas_backlog.md](findings/operator_ideas_backlog.md).

### Idea 1 — Cleaner FALLEN heuristic: motion without commanded motion

> "If the robot is moving without intentional motor command, then it is falling."

Translation: `|last_commanded_output_pwm| < SMALL (≈20)` for ≥100 ms AND `|gyro_pitch_dps| > LARGE (≈30)` ⇒ state transition to FALLEN. Physics rationale: with motors silent and the bot still upright, the only force producing pitch rotation is gravity acting on the imbalanced CoM. Complements the existing lateral-gyro HELD detector — adds a *complementary* signal that distinguishes "falling because controller couldn't recover" from "I am being held". Cost: ~30-50 bytes of code. Status: **deferred to Phase 2.5**, after CHARACTERISE lands. Source: [session record §Operator-proposed FALLEN heuristic](archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md).

### Idea 2 — Nonlinear gain scaling near balance (gain scheduling)

> "Variable motor speed: gentle near balance, aggressive far from it."

Translation: the linear PID drives the motors just as hard for sub-degree noise wobbles as it does for the same fractional displacement during a real fall. Fix is **gain scheduling**: low effective gain inside a small "soft zone" around 0°, higher gain outside. Two compact formulations (dead-zoned proportional, quadratic scaling); see [session record §Operator-proposed idea — nonlinear gain scaling](archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md) for the math. Real control-engineering technique (Khalil §13 gain scheduling). Cost: ~20 bytes of code and one float parameter. Status: **deferred to Phase 2.6**, after CHARACTERISE and the FALLEN heuristic land.

### Idea 3 — Auto-discover min/max PWM via sensor feedback

> "Find the stiction floor and saturation point from sensor data, not from a constant."

Translation: on boot (or via `k` command), pulse PWM through {30,50,70,90,110,130,150,200} for 200 ms each (alternating direction), accumulate `|gyro_pitch|` per pulse, identify the first PWM that exceeds the response threshold (= stiction floor) and the PWM where response stops growing (= saturation point). Save to EEPROM at 0x210, apply via new runtime setter `L298NMotorDriver::set_stiction_min_pwm()`. Status: **in progress — being implemented now as Phase 2 CHARACTERISE state**. Source: [session record §Phase 2 plan](archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md). Prior research: [findings/dynamic_pwm_accel_learning.md §8 step 3](findings/dynamic_pwm_accel_learning.md).

### Why these matter to the vision

All three close gaps where bot-specific knowledge was leaking into either source code (`stiction_min_pwm = 0` → 30 → ?) or fixed thresholds (lateral-gyro magnitude, linear PID gains). Each replaces a hardcoded constant or assumed behaviour with a measured / online-learned signal. They are not feature creep — they are the next concrete instances of the "nothing bot-specific that could be measured" constraint codified in [scope.md](scope.md).
