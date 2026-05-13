# Dynamic PWM → Acceleration Learning
Status: PROPOSAL — not implemented. Tier-4 work, after the 2-state Kalman and after the balance loop is rock-solid with hand-tuned gains.
Last updated: 2026-05-12

## 1. The user's framing

> "the robot should know how much accelerations in what directions are caused by how much PWM to each motor"

In control-theory terms: **online closed-loop system identification** of the plant's input-output gain, optionally inverted into a **feedforward inverse model**. The robot continuously fits `α = f(pwm_L, pwm_R, state)` and uses that fit to issue better commands on the next tick.

This generalises hand-tuned PID. Classical tuning (Ziegler-Nichols, AMIGO, relay-feedback per [`auto_pid_tuning_research.md`](auto_pid_tuning_research.md)) implicitly fits a linear plant `α = K · u` and picks Kp/Ki/Kd from `K` and a time constant. Hand-tuning is the operator doing that fit with a screwdriver. The proposal here is to do it in firmware, continuously, with a recursive estimator — so gains track the plant instead of freezing at one operator's guess from one battery state.

This dovetails with [`../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md): if the controller's job is to drive IMU-observed lateral acceleration to zero, then learning *how* PWM causes acceleration is the missing link between the cost (minimise `a_x`) and the action (set `pwm`).

## 2. Why bother (when conservative PID works)

Right now ([`balance_failure_diagnosis_2026-05-12.md`](balance_failure_diagnosis_2026-05-12.md) §5 Tier 1) the answer is `Kp=15 Ki=0 Kd=8` with a ±80 PWM cap. Gains are conservative enough to stay linear. So why add learning?

- **Portability.** Gains tuned for *this* Uno + L298N + cheap motors do not transfer to the next chassis. A learned mapping eliminates the per-bot tuning step.
- **Drift.** Battery sag 8.4 V → 7.0 V drops torque-per-PWM by ~15% ([`online_adaptive_balance_tracking.md`](online_adaptive_balance_tracking.md) §1). Wear, payload, grease — all move the plant on minutes-to-weeks scales. Fixed Kp drifts out of optimum; a learned `K_motor` follows.
- **Feedforward.** Pure PID is reactive — it only commands torque after an error exists. A learned inverse model `pwm = K_motor⁻¹ · α_desired` lets the controller anticipate, with PID as a small trim layer. Smaller gains, less overshoot, faster settle.
- **Diagnosis.** A live `K_motor` estimate is a free hardware-fault indicator: if it suddenly halves, a wheel is slipping or a lead is loose. Conservative PID can't tell you that — it just slams harder.

**Do not build this until the conservative loop balances.** Learning on an already-oscillating system amplifies the oscillation. Order is non-negotiable: Tier 1 conservative PID → Tier 2 2-state Kalman → Tier 3 mounting drift estimator → **Tier 4 this proposal**.

## 3. What signal we have

From [`src/sensors/sensor_base.h`](../../src/sensors/sensor_base.h):

- `getRawAccel(xyz)` — body-frame m/s², 100 Hz on BNO055 AMG mode.
- `getRawGyro(xyz)` — body-frame deg/s, same rate.
- `getOrientation()` — fused pitch/roll/yaw via quaternion (MASTER_DESIGN D4).

Plus: motor PWM commands (signed −255..+255), known to firmware. Pitch and pitch-rate from the future `balance_kalman` filter.

What we **don't** have: no wheel encoders (operator preference), no current sense, no battery telemetry (unless an analog divider is added — strongly recommended; see §6).

**Observable from IMU alone:**

- **Pitch-axis angular acceleration** `α_pitch` — differentiate gyro Y, or take the residual of `gyro_Y_dot − (m·g·d/I)·sin(pitch)` from the rigid-body equation.
- **Body-X translational acceleration** — directly from `getRawAccel()[0]` minus the gravity projection.
- **Stiction band** — bins where `|pwm| < X` produce no observable α.
- **Direction asymmetry** — mean α at PWM=+150 vs −150 over stationary windows.

**Not observable from IMU alone:**

- **Per-wheel torque** — both wheels commanded together produce one combined α. Must inject a yaw-producing differential (`pwm_L = +K, pwm_R = −K`) and read `gyro_Z` to disambiguate.
- Wheel slip vs traction, surface friction, battery voltage. All fundamentally outside the IMU signal set.

## 4. The learned model — three feasible architectures

### 4a. Linear regression (simplest, what we build first)

Scalar recursive least-squares on the pitch equation of motion:

```
α_pitch ≈ K_motor · (pwm_L + pwm_R)/2  +  (m·g·d/I) · sin(pitch)
```

`(m·g·d/I)` is known a priori from chassis geometry — hardcode it, don't learn it. That leaves one unknown: `K_motor`.

```
K[n+1] = K[n] + L[n] · (α_measured − K[n] · u)
L[n]   = P[n] · u / (λ + P[n] · u²)
P[n+1] = (P[n] − L[n] · u · P[n]) / λ
```

Three floats and a multiply-add per tick. ~30 µs on AVR at 200 Hz. Forgetting factor `λ ≈ 0.995–0.999` (5–30 s time constant). See Åström & Wittenmark, *Adaptive Control* 2nd ed. (1995) Ch. 2 derivation, Ch. 5 closed-loop stability.

### 4b. Inverse model with feedforward (next step)

Once 4a is stable, invert: `pwm_required = α_desired / K_motor`. Run as feedforward; PID becomes a small trim correcting model error and disturbances. As `K_motor` converges, the feedforward gets accurate, PID error shrinks, PID contribution decays. This is the classical self-tuning regulator (STR) — Åström & Wittenmark Ch. 5.

Caveat: PID and feedforward must share `α_desired`. Best implemented by having the PID emit `α_desired` and a separate `inverse_model_block` produce PWM, with PID correction added on top.

### 4c. Reinforcement learning / neural-net (not on AVR)

A small MLP (~100 parameters) could in principle learn `(pitch, gyro, battery_v) → pwm` directly. On a 2 KB SRAM ATmega328P with no FPU, forward inference alone is several ms per layer, and online training won't fit. Sutton & Barto 2018 Ch. 6–9 covers the algorithms. Revisit on Teensy 4.0 / ESP32-S3 as Phase 7 research. Not now.

## 5. The L298N + cheap DC motor reality

PWM → α is emphatically not one number:

- **Stiction.** Below ~18–25 PWM, no motion. Inside the dead-band, RLS will fit `K_motor → 0`, which is true for those samples but tells the inverse model "infinite PWM needed" — wrong. Dead-band samples must be excluded.
- **Saturation.** Above ~200 PWM (drops to ~150 at low battery), wheel hits max RPM; further PWM yields no extra α. Saturated samples also must be excluded.
- **Direction asymmetry.** Cheap brushed motors typically show 10–25% gain difference per direction (brush wear, bearing pre-load). Fix: learn separate `K_motor⁺` and `K_motor⁻`.
- **Battery state.** Linear to first order: `K_motor(V) ≈ K_nom · V/V_nom`. Adding an `A0` divider and including `V_batt` in the regressor turns drift-tracking into explicit modelling. Strongly recommended.
- **Stall on sudden load.** 0→200 PWM step into a stationary wheel may stall (no back-EMF). Stalled samples bias the estimate down — exclude them, or use slew-limited feedforward only.

Minimum viable form is piecewise:

```
α_pitch = 0                                if |pwm| < stiction
        = K_motor⁺ · (pwm − stiction)       if pwm > 0, |pwm| < saturation
        = K_motor⁻ · (pwm + stiction)       if pwm < 0, |pwm| < saturation
        = (excluded)                        otherwise
```

Three parameters plus one threshold. ~40 bytes of state. The full RLS runs on a 2-element regressor `[u_above_stiction, sign(u)]`.

## 6. Identifiability concerns

**Can identify from IMU alone:**

- Combined-wheel `K_motor` (assumes both wheels equal — usually within ~10% on a balanced chassis).
- Stiction band — by binning samples with `α ≈ 0`.
- Direction asymmetry on combined torque — by sign-splitting.
- Sensor mounting bias — already handled by `OnlineMountingEstimator` ([`online_adaptive_balance_tracking.md`](online_adaptive_balance_tracking.md)). This proposal handles **gain**; that one handles **offset**. They coexist and **must not feed each other** (couple them and you get classical two-time-scale STR bursting; see Anderson 2005).

**Cannot identify from IMU alone:**

- **Per-wheel torque** — requires deliberate differential excitation in IDLE and observation of yaw. Yaw is far cleaner than pitch because gravity does not couple to yaw at small tilts.
- Wheel slip, surface friction — fundamentally unobservable without encoders.
- Absolute battery voltage — unless you add a 2-resistor `A0` divider (~1 hour of work).

**Explicit recommendation:** for per-wheel learning, schedule a 200 ms differential burst (`pwm_L = +80, pwm_R = −80`) once per IDLE entry or every 5 min of continuous balance. Gated on `velocity ≈ 0 && upright`, with a hard watchdog so a stuck command can't escape the window.

## 7. Where this fits in the BalanceApp state machine

Per [`MASTER_DESIGN.md`](MASTER_DESIGN.md) §4.7, `IDLE → CAPTURE_MOUNTING → AUTO_TUNE → RUN → SAFE_FALL → IDLE`. Insertion points:

- **IDLE** — deliberate ID experiments: 0.5 Hz chirp `pwm = 50·sin(2π·0.5·t)` for 4 s while held upright; plus the per-wheel differential burst from §6. Best SNR.
- **CAPTURE_MOUNTING** — no PWM commanded; nothing to learn. Skip.
- **AUTO_TUNE** (relay-feedback) — high-SNR limit-cycle data. RLS converges in <5 s and forms the bootstrap `K_motor` from which run-time tracking proceeds. **Best on-line data we ever get.**
- **RUN** — read-only-ish updates with `λ ≈ 0.999` (60 s time constant). PID provides only small corrections in the linear region, so the regressor has poor excitation; slow forgetting is mandatory to avoid covariance wind-up on quiet data (Åström & Wittenmark Ch. 11).
- **SAFE_FALL** — suspend. Saturated PWM and large tilts violate the linear-model assumption.

`OnlineMountingEstimator` tracks offset; this proposal tracks gain. Both run continuously in RUN. They must not feed each other.

## 8. Concrete minimum-viable implementation

Phase 4.10 (after 4.7 balance reference is stable), in priority order:

1. **Scalar RLS on `K_motor`.**
   - Files: `src/control/plant_identifier.{h,cpp}`
   - Cost: ~120 LOC, ~24 B RAM, ~30 µs/tick.
   - API: `update(pwm_mean, alpha_measured, dt_s)`, `get_K_motor()`, `get_confidence()` (= 1/P[n]).
   - Risk: low. Closed-form scalar update, well-behaved with forgetting factor.
   - Success: after 30 s of conservative-PID balancing, estimate within ±20% of relay-tuner's `K_u = 4d/(π·a)` ([`auto_pid_tuning_research.md`](auto_pid_tuning_research.md) §2.1).

2. **Wire `K_motor` into adaptive Kp.**
   - Modify [`src/control/pid_controller.cpp`](../../src/control/pid_controller.cpp) to accept a `gain_scale` multiplier (default 1.0). `Kp_eff = Kp_nom · K_nom / K_observed`. Battery sag → `K` shrinks → Kp rises proportionally.
   - Cost: ~10 LOC, no new state.
   - Risk: moderate — a bad `K_motor` now affects the loop. Mitigate with hard bounds (`0.5 < K/K_nom < 2.0`, freeze and fall back outside) and slew limit (`d(K)/dt < 5%/s`).
   - Success: bot still balances with a discharged 7.0 V battery without re-flashing. Quantify: settling time after 5° poke stays within ±50% of fresh-battery baseline.

3. **Dynamic stiction-band detection.**
   - Same module bins `(pwm, α)` in 5-wide bins from −50 to +50. Bins around zero with `|mean α| < noise_floor` define the dead-band edges.
   - Replace hardcoded `±25` in `BalanceApp` with the learned value.
   - Cost: ~40 B RAM (20 bins × 2 floats), ~50 LOC.
   - Risk: low. Worst case: too-wide dead-band makes the bot lazy; bound to `[10, 40]` PWM.
   - Success: estimate stabilises within 60 s, changes by <5 PWM across battery levels.

4. **Direction-asymmetry learning.**
   - Split `K_motor` into `K⁺` and `K⁻`. Two parallel scalar RLS estimators.
   - Cost: ~48 B RAM total, same CPU.
   - Risk: low.
   - Success: observed asymmetry matches a manual measurement (commanded ±150 PWM held 200 ms, peak α) within 15%.

Total Phase 4.10 budget: ~300 LOC, ~100 B RAM, ~80 µs/tick. Well inside Uno limits.

## 9. Alternatives we're explicitly NOT doing

- **Adding encoders.** Operator preference — IMU only.
- **Neural networks on AVR.** §4c.
- **LQR / pole placement.** Requires an accurate dynamic model up front; defeats the "learn it" goal. Revisit as Phase 7 on Teensy/ESP32 once §4a + §4b are reliable.
- **MRAC (MIT rule).** ~250 B RAM, ~3 KB flash, and the existing online estimator already covers the offset case MRAC would address.

## 10. Done criteria

The bot retunes itself, with zero operator action, after each of:

- **Battery swap** (fresh → depleted 18650): settling time after 5° poke within ±50% of factory-fresh baseline.
- **100 g payload added 6 cm above axle**: settling time within ±50%. Mounting estimator absorbs the static offset; gain estimator absorbs the inertia change.
- **30 days of operation** (motor wear, brush dust): settling time within ±50%. Direction asymmetry compensated by separate `K⁺`/`K⁻`.

Failure mode: if `K_motor / K_nom` exits `[0.5, 2.0]`, estimator freezes, framework reverts to nominal gains, dashboard reports `plant_identification_status = "out_of_bounds"`. Operator alerted; bot stays upright on conservative defaults.

## 11. References

### Textbooks

- **Åström, K. J. and Wittenmark, B.** *Adaptive Control*, 2nd ed., Addison-Wesley, 1995. Ch. 2 (RLS), Ch. 5 (self-tuning regulators — the §4b architecture), Ch. 11 (covariance wind-up, supervision).
- **Slotine, J.-J. E. and Li, W.** *Applied Nonlinear Control*, Prentice Hall, 1991. Ch. 8 — adaptive control of mechanical systems.
- **Ljung, L.** *System Identification: Theory for the User*, 2nd ed., Prentice Hall, 1999. RLS with forgetting factor, excitation sufficiency.
- **Anderson, B. D. O.** "Failures of adaptive control theory and their resolution," *Communications in Information and Systems* 5(1), 2005. Bursting; relevant once two adaptive loops coexist.
- **Sutton, R. and Barto, A.** *Reinforcement Learning: An Introduction*, 2nd ed., MIT Press, 2018. Background only — §4c.

### In-repo

- [`MASTER_DESIGN.md`](MASTER_DESIGN.md) §4.7 — balancing-robot reference application slot where this work lives.
- [`online_adaptive_balance_tracking.md`](online_adaptive_balance_tracking.md) — offset-tracking sibling. Coexists, does not couple.
- [`auto_pid_tuning_research.md`](auto_pid_tuning_research.md) — relay-feedback tuner that produces the bootstrap `K_motor` (§7 AUTO_TUNE).
- [`balance_failure_diagnosis_2026-05-12.md`](balance_failure_diagnosis_2026-05-12.md) — why conservative PID comes first.
- [`../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — cost-function framing.
- [`balance_point_and_mounting_research.md`](balance_point_and_mounting_research.md) §3 — the rigid-body equation regressed against.
- [`../../src/sensors/sensor_base.h`](../../src/sensors/sensor_base.h) — `getRawGyro` / `getRawAccel` input.
- [`../../src/control/pid_controller.h`](../../src/control/pid_controller.h) — loop where learned `K_motor` plugs in as `gain_scale` (§8 step 2).
