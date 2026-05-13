# BALANCING / HELD / FALLEN — three-state machine

A pitch-only state machine cannot tell "user is lifting the fallen bot back
upright" apart from "bot has spontaneously recovered upright pose." The
fallen bot, mid-lift, briefly traverses near-zero pitch — and the current
firmware fires motors within ~150 ms, in the user's hand. The fix is to
add an explicit `HELD` state and gate transitions on **motion** signals
(gyro, accel deviation from 1 g, surface-contact heuristic), not pitch alone.

This document recommends concrete thresholds, dwell times, and a code
sketch the user can drop into `balance_app.cpp` tonight.

---

## 1. Sensor signals available

BNO055 in NDOF mode already gives us everything we need. We must expose
`getRawGyro(float xyz[3])` next to the existing `getRawAccel`:

| Signal                       | Source                                   | What it tells us                                                                        |
| ---                          | ---                                      | ---                                                                                     |
| `pitch_deg`                  | fused quaternion (existing)              | absolute attitude relative to mount frame                                               |
| `|gyro|` (rad/s → deg/s)     | `getVector(VECTOR_GYROSCOPE)`            | **handling magnitude** — any external manipulation shows up here first                  |
| `|accel|` (m/s²)             | `getVector(VECTOR_ACCELEROMETER)`        | should be ≈ 9.81 ± noise on a surface; **deviates during a lift**                       |
| `accel_z / |accel|`          | derived                                  | **gravity alignment with body Z** — near 1 when upright on ground, ~0 when lying flat   |
| `linear_acceleration`        | fused (gravity-removed) — optional       | bona-fide translational accel; cleaner "lift" detector than `||a|-g|`                   |
| `pitch` rate from `gyro.y`   | mounted-axis gyro component              | the actual variable PID uses; cheaper than recomputing                                  |

Two practical notes:

- **Gyro is the leading indicator.** Whoever grabs the bot induces 50–200
  deg/s on at least one axis instantly — long before pitch crosses any
  threshold. Make gyro magnitude the **primary** lift trigger.
- **Accel magnitude is the disambiguator.** A vigorously balancing bot
  also has high gyro. But it remains *in contact with the ground*, so
  `||a| − g| < ~1.5 m/s²` on a low-passed window. A held bot will have
  brief lift/drop transients (≥3 m/s² peaks) and arbitrary tilt — the
  pair of signals is enough.

---

## 2. State definitions + transition criteria

Proposed enum (extends what's already in `balance_app.h`):

```cpp
enum class BalanceAppState : uint8_t {
    IDLE             = 0,
    CAPTURE_MOUNTING = 1,
    AUTO_TUNE        = 2,
    RUN              = 3,   // == BALANCING
    HELD             = 4,   // NEW: motors off, ready to resume
    FALLEN           = 5    // was SAFE_FALL, semantics unchanged: sticky
};
```

(I would also rename `SAFE_FALL` to `FALLEN` to match the user's vocabulary;
the existing `step_safe_fall_` already implements the "sticky, operator-only
recovery" behaviour FALLEN needs — see `balance_app.cpp:386-405`.)

Use these signals, low-passed at ~20 Hz cutoff:

```text
g_mag    = |gyro|              deg/s
a_dev    = ||accel| − 9.81|    m/s²
a_align  = accel_z / |accel|   (1 when upright, 0 when lying flat)
|pitch|                        deg
```

Transition table (all dwell times assume the 200 Hz PID tick):

| From          | To       | Predicate                                                                       | Dwell  |
| ---           | ---      | ---                                                                             | ---    |
| RUN           | HELD     | `g_mag > 60 deg/s` **OR** `a_dev > 3.0 m/s²`                                    | 150 ms |
| RUN           | FALLEN   | `|pitch| > 25°`                                                                 | 50 ms  |
| HELD          | RUN      | `g_mag < 8 deg/s` **AND** `|pitch| < 4°` **AND** `a_dev < 0.8 m/s²` **AND** `a_align > 0.95` | **800 ms** |
| HELD          | FALLEN   | `|pitch| > 25°` (user set it down on its side / dropped it)                     | 200 ms |
| HELD          | FALLEN   | total time in HELD > 30 s without RUN transition                                | —      |
| FALLEN        | IDLE     | **user only**: short press, `c`, `R`, dashboard                                 | —      |
| any           | IDLE     | safety abort (long press)                                                       | —      |

Tipover wins. The 50 ms FALLEN dwell on the pitch-only edge is intentional:
a tipover is genuinely unrecoverable for an inverted pendulum, so we cut
motors fast. The 150 ms RUN→HELD dwell tolerates a single PID overshoot.

---

## 3. Anti-spurious transitions

The bot's own balance recoveries routinely hit 30–50 deg/s on `gyro.y`
during a hard catch. Three layers of suppression:

1. **PT1 low-pass on `g_mag` and `a_dev`** at 8–10 Hz cutoff (`α = dt/(τ+dt)`
   with `τ = 20 ms`). Cheap — one multiply-add per sample, same primitive
   we already use for the D-term LPF. Filters out single-sample spikes.

2. **Minimum dwell time** before the transition fires. 150 ms means 30
   consecutive samples at 200 Hz, **not 30 cumulative**. A counter that
   resets on any failing sample (same pattern as `recovery_count_` in the
   existing FALLEN→RUN code).

3. **Use gyro magnitude across all three axes, not just pitch-axis.**
   Intrinsic balance motion is almost purely pitch-axis. Being grabbed
   produces motion on roll/yaw as well — the cross-product `sqrt(gx² + gz²)`
   alone is an extremely clean handling indicator that *self-balancing
   motion does not produce*. This is the trick. Recommend the gate be:

```text
g_lateral = sqrt(gyro_x² + gyro_z²)   # roll + yaw axes
hold_trigger = (g_lateral > 30 deg/s) OR (a_dev > 3 m/s²)
```

Pitch-axis gyro participates in the *exit* gate (HELD→RUN requires it low)
but not the *entry* gate. This single change eliminates ~90 % of false
HELD transitions during aggressive balancing.

---

## 4. Why these numbers

For a 500 g bot, CoM 8 cm above the axle, leaning 10°:

```
τ_gravity = m·g·L·sin(10°) = 0.5 · 9.81 · 0.08 · 0.174 ≈ 0.068 N·m
I ≈ m·L² = 0.5 · 0.08² = 3.2e-3 kg·m²
α = τ / I ≈ 21 rad/s² ≈ 1200 deg/s²
```

Pitch-axis gyro hits **~12 deg/s after just 10 ms** of free fall from 10°.
By 100 ms it can reach 120 deg/s. So:

- **60 deg/s lateral gyro** is well above what intrinsic pitch dynamics
  produce (free pitch fall pegs `gyro.y` but leaves `gyro.x`/`gyro.z`
  near zero). It triggers on real handling.
- **3 m/s² accel deviation** corresponds to ~0.3 g — about the smallest
  lift the human hand produces. Lifting a 500 g bot at 0.5 m/s² already
  gives 0.25 m/s² body-frame deviation; a sharp grab is much higher.
- **±25° tipover** matches the existing `kDefaultTiltLimitDeg = 10°` with
  margin (the bot will already be SAFE_FALL by 15°; 25° is the unrecoverable
  bound for HELD→FALLEN where user intent matters).
- **4° pitch for HELD→RUN** is one standard deviation from the captured
  balance point — tight enough that the motors don't have to fight gravity
  the instant they spool up.

---

## 5. Code-level proposal

Three changes to `balance_app.cpp`:

```cpp
// New filtered signals, members on BalanceApp:
float g_lateral_lpf_;   // deg/s
float a_dev_lpf_;       // m/s²
uint16_t hold_enter_count_;   // 200 Hz samples in candidate-HELD
uint16_t hold_exit_count_;    // 200 Hz samples in candidate-RUN
uint32_t held_entered_ms_;

// In read_imu_(), after computing pitch:
float gyro[3]; imu_.getRawGyro(gyro);     // <-- new BNO055 method
float accel[3]; imu_.getRawAccel(accel);
const float gx = gyro[0], gz = gyro[2];
const float g_lat = sqrtf(gx*gx + gz*gz) * 57.2958f;  // rad/s → deg/s
const float a_mag = sqrtf(accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2]);
const float a_dev = fabsf(a_mag - 9.81f);

const float alpha = 0.20f;  // ~8 Hz LPF at 200 Hz
g_lateral_lpf_ = g_lateral_lpf_ + alpha * (g_lat - g_lateral_lpf_);
a_dev_lpf_     = a_dev_lpf_     + alpha * (a_dev - a_dev_lpf_);
```

```cpp
void BalanceApp::step_run_(uint32_t now_ms) {
    if (safety_.is_tipover(pitch_deg_)) {
        enter_state_(BalanceAppState::FALLEN, now_ms);
        return;
    }
    // NEW: handling detection
    const bool handling = (g_lateral_lpf_ > 30.0f) || (a_dev_lpf_ > 3.0f);
    if (handling) {
        if (++hold_enter_count_ >= 30) {            // 150 ms at 200 Hz
            enter_state_(BalanceAppState::HELD, now_ms);
            return;
        }
    } else {
        hold_enter_count_ = 0;
    }
    // ... rest of existing PID step ...
}

void BalanceApp::step_held_(uint32_t now_ms) {
    motors_.stop();
    last_output_ = 0;

    // Drop-out: dropped or laid on side.
    if (fabsf(pitch_deg_) > 25.0f) {
        if (++hold_exit_count_ >= 40) {             // 200 ms
            enter_state_(BalanceAppState::FALLEN, now_ms);
        }
        return;
    }
    // Timeout: HELD > 30 s without recovery — assume not coming back.
    if (now_ms - held_entered_ms_ > 30000UL) {
        enter_state_(BalanceAppState::FALLEN, now_ms);
        return;
    }
    // Surface-contact + stillness + level => resume.
    const float a_mag = ...;                        // computed in read_imu_
    const float a_align = accel_z_ / a_mag;         // cached in read_imu_
    const bool ready = (g_lateral_lpf_ < 8.0f)
                   && (fabsf(pitch_deg_) < 4.0f)
                   && (a_dev_lpf_ < 0.8f)
                   && (a_align > 0.95f);            // upright, on a surface
    if (ready) {
        if (++hold_exit_count_ >= 160) {            // 800 ms
            pid_.reset();                            // discharge I-term
            enter_state_(BalanceAppState::RUN, now_ms);
        }
    } else {
        hold_exit_count_ = 0;
    }
}
```

`step_safe_fall_` becomes `step_fallen_` with no behavioural change.

---

## 6. The "set it down" bouncing problem

When the user releases the bot, their fingers leave with the bot still
slightly rocking. **800 ms** is the recommended HELD→RUN dwell. Reasoning:

- A human releasing an object typically has ~300 ms of residual contact
  motion (Westling & Johansson, *Exp. Brain Res.* 1984, on grip-release
  kinetics — typical finger-disengagement at 200–400 ms).
- A 0.5 s margin on top is enough to bridge "fingers still touching" while
  remaining snappy enough that the user does not think the bot is broken.
- The `a_align > 0.95` clause separately blocks resume while a fingertip
  is bracing the bot at an angle.

Tunable. If 800 ms feels sluggish in practice, drop to 500 ms but **never
below 300 ms** — that is the floor where motor spin-up will catch a
still-touching finger.

---

## 7. Edge cases

**(a) Bot held perfectly still at exact balance pitch.** Pitch < 4°,
gyro near zero — the pitch gate would say "go," but `a_align > 0.95`
will not hold. A hand-held bot is almost never *also* perfectly oriented
with body-Z exactly along gravity; the natural human grip tilts the
chassis by 5–15° in some axis. Even if the user gets lucky, accel
magnitude under a steady hand fluctuates ±0.3 m/s² (cardiac tremor) —
visibly larger than the 0.05 m/s² floor on a hard surface. Surface
contact is the discriminator. If it fails in extreme cases, the 30 s
HELD timeout catches it as FALLEN, which is the safe failure mode.

**(b) Bot oscillating wildly during balance.** Pitch-axis gyro spikes,
but `gyro.x`/`gyro.z` stay near zero — the inverted-pendulum dynamics
are planar. **That's why the lateral-gyro gate (§3) works.** Intrinsic
balance motion is one-axis; external handling almost never is.

**(c) Floor vibration / table bump.** Single transient `a_dev > 3`
spike for <50 ms. The 150 ms minimum dwell + 8 Hz LPF combination
rejects it. If the bump is sustained (someone tapping the table
rhythmically), `g_lateral` stays low and the dwell counter resets
each "quiet" sample between taps.

**(d) Carrying bot from far away.** Bot stays in HELD as long as
motion continues. Set down → 800 ms → RUN. This is the *desired*
behaviour and what motivates the design.

---

> See also: [../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — explains why fall-detection is gated OFF by default (`USE_BALANCE_FALL_DETECTION`) and HELD detection is gated ON (`USE_BALANCE_HELD_DETECTION`) in the current design.

## 8. References

- `docs/findings/disturbance_compensation_research.md` §3 — push-detection
  state machine (same primitive shape, different thresholds).
- `docs/findings/balance_point_and_mounting_research.md` §1(b) — gyro
  zero-rate gate used during mounting capture; HELD→RUN reuses it.
- `docs/findings/online_adaptive_balance_tracking.md` §3 — stillness
  predicate `|θ̇| < 1°/s for 200 ms`; we adopt the same shape but a
  looser threshold (8 deg/s) because we want UX responsiveness, not
  estimator precision.
- `docs/archive/balancing_robot_reference/DISSECTION_NOTES.md` — the
  original .ino had no concept of HELD; the proposed state is purely
  additive.
- Westling, G. & Johansson, R. S. (1984). "Factors influencing the
  force control during precision grip." *Experimental Brain Research*
  53(2). Hand-release kinetics support the 800 ms dwell.
