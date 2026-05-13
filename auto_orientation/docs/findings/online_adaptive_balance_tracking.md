# Online Adaptive Balance-Point Tracking

Research notes for `auto_orientation/`. Extends the one-shot capture
in `balance_point_and_mounting_research.md` with continuous,
slowly-adaptive estimation of the *dynamic-equivalent zero point*.

---

## Recommendation summary

- **AVR Mega:** *slow LPF of the inner-loop integral term* (TC ~60 s),
  gated by stillness and anti-windup predicates. ~24 B SRAM, ~400 B
  flash, converges in ~30 s, dead-simple to reason about.
- **ESP32-class:** extend the 2-state Kalman (pitch + gyro-bias) to a
  **3-state Kalman** with mounting-offset as a slow random-walk state
  (process noise ~1e-7 rad²/s). ~60 B extra SRAM, ~1.5 KB flash; the
  covariance entry doubles as the published confidence value.
- **Both:** expose a `MountingCalibrationStatus` struct over the Phase 6
  dashboard API. Hard-bound the live estimate to ±5° of the one-shot
  reference; beyond that, freeze and require user re-capture.

---

## 1. Why one-shot capture is insufficient

The Phase 4.3 hybrid capture pins the *initial* mounting quaternion to
~0.3°. But the true balance point depends on where the centre of mass
sits over the wheel axle in the gravity field, and that moves during
operation. Concrete numbers for a 500 g chassis, 100 mm wheelbase,
CoM ~8 cm above axle:

- **Tether torque.** 10 g of USB cable hanging 5 cm off-axis →
  `τ = 0.010·9.81·0.05 ≈ 4.9 mN·m`. Equivalent static lean
  `Δθ = τ/(m·g·h_com) ≈ 0.013 rad ≈ 0.7°`; cable slack and motion
  realistically double the lever, so **~1° of constant lean** is
  typical — 20 % of a ±5° operating envelope, silently lost to a wire.
- **Battery SoC.** An 18650 sags 4.20 → 3.40 V; after the buck
  regulator and motor driver, torque-per-PWM drops ~15 % at low charge.
  The closed loop absorbs this via the I-term, equivalent to ~0.5° of
  dynamic offset shift even though IMU pose did not change.
- **Payload.** A 50 g add-on board 6 cm above axle shifts both CoM and
  the inverted-pendulum natural frequency by several percent.
- **Wheels, bearings, surface.** Tyre compression, bearing grease
  redistribution, and carpet-vs-tile rolling resistance each contribute
  0.1–0.3°. They aggregate.

Geometry is captured one-shot; *dynamics* must be tracked online.

## 2. What "online estimation" means

We estimate the **dynamic-equivalent zero point** `θ*`: the pitch
angle at which the steady-state inner-PID output is zero. With PID
error `e = θ − θ*`, in quasi-steady state `ū = K_i·∫e dt = 0`
requires `E[θ] = θ*`. The naive estimator is therefore a slow LPF of
measured pitch — *only* when the bot is upright and stable, otherwise
transients contaminate the average. That gating is the non-trivial
part. (Åström & Wittenmark, *Adaptive Control* 2nd ed. 1995,
ch. 1–2 on certainty-equivalence, ch. 5 on parameter-tracking with
regressor excitation.)

## 3. Algorithm candidates

| Algorithm | RAM | Flash | Conv. | Notes |
|---|---|---|---|---|
| **Slow LPF of I-term** | ~24 B | ~400 B | ~30 s | One float state + TC. Reuses existing PID. |
| **RLS on (u_cmd, θ)** | ~120 B | ~1.5 KB | ~10 s | Needs persistent excitation; not naturally present in a station-keeping balancer (covariance wind-up risk). |
| **Sliding-window mean over stable windows** | ~80 B | ~600 B | ~60 s | Very robust, trivially explainable. |
| **3-state Kalman (θ, b_gyro, θ\*)** | ~60 B | ~1.5 KB | ~20 s | Extends the Lauszus 2-state filter; covariance gives confidence for free. |
| **MRAC (MIT rule)** | ~250 B | ~3–4 KB | variable | Overkill for one offset; Åström-Wittenmark ch. 5, Ioannou-Sun *Robust Adaptive Control* (1996) ch. 6. |

**Per platform.** AVR Mega → *slow LPF of the integral term* with
TC = 60 s, gated by `|θ̇| < 1°/s` for 200 ms and `|I| < 0.7·I_max`.
A 3·TC ≈ 3 min settling is comfortable: cable drag and battery sag
move on tens-of-minutes scales. ESP32 → *3-state Kalman* with `θ*`
as random walk, process noise σ² chosen so the 1σ drift over 60 s is
~0.02°. The diagonal covariance entry `P[2,2]` is published as
`confidence`.

## 4. Drift detection and confidence reporting

Adaptation must never be silent. Proposed JSON, served alongside
`OrientationData`:

```json
{
  "mounting_calibration": {
    "state": "tracking",
    "reference_offset_deg": -8.60,
    "live_offset_deg":      -8.93,
    "drift_estimate_deg":   -0.33,
    "drift_rate_dps_per_min": 0.012,
    "confidence":            0.87,
    "seconds_since_capture": 482,
    "adaptation_locked":     false,
    "lock_reason":           "",
    "eeprom_writes":         3
  }
}
```

Thresholds (defaults; tuneable):

| abs(drift) from ref | abs(drift_rate) /min | State | Confidence | Behaviour |
| --- | --- | --- | --- | --- |
| < 0.5° | < 0.05° | `fresh` | > 0.9 | Normal. |
| 0.5–2° | < 0.1° | `tracking` | 0.6–0.9 | Normal aging / battery sag. |
| 2–5° | < 0.5° | `creeping` | 0.3–0.6 | Warn on dashboard. |
| 2–5° | ≥ 0.5° | `unstable` | < 0.3 | Freeze; alert (snagged cable? loose payload?). |
| > 5° | any | `out_of_bounds` | 0 | Hard freeze; require manual re-capture. |

## 5. Anti-windup interaction

Adapting while the I-term is wound up is the failure mode: the bot
has tipped, the I-term is railed recovering, and the LPF would ingest
the recovery transient as a new steady state. Guard rails:

1. **I-term gate:** suspend while `|I_term| > 0.7·I_max`.
2. **Rate gate:** suspend while `|θ̇| > 1°/s` (covers tipovers).
3. **Acceleration gate:** suspend while `|a_horizontal| > 0.3 g`
   (covers pickups, kicks, threshold bumps).
4. **Output saturation gate:** suspend after motor PWM rails > 100 ms.

When any gate trips the estimator *freezes* — it does not reset. The
Kalman variant additionally inflates `P[2,2]` slightly each ticked
gate-open frame to reflect the lost information (σ-modification, per
Ioannou-Sun ch. 8).

## 6. Safety constraints

- Hard bound `|live − reference| ≤ 5°`. Five degrees is ~10 % of the
  chassis's static-stability cone; further drift almost certainly
  means something physical is wrong.
- Rate-limit applied offset to ≤ 0.5°/s, bounding how fast a sensor
  glitch or computation fault can drag the setpoint.
- Dead-man: if `unstable` for > 30 s → commanded motor stop + fault.

## 7. Tetherless considerations

The user's observation is operationally important: a USB cable on a
500 g bot is ~1° of fake lean that the one-shot capture will bake in
if performed tethered. Documented procedure (no firmware enforcement
needed for v1):

1. Flash firmware over USB.
2. **Unplug USB.**
3. Place bot in balance pose by hand.
4. Connect to bot's WiFi dashboard.
5. Press *Calibrate* — tetherless one-shot capture.
6. Online estimator handles drift from there.

The Phase 6 dashboard "Calibrate" button is the right UX hook; label
it with a tooltip reminding the user to perform it untethered after a
fresh flash.

## 8. Storage

EEPROM endurance is 100 k writes/byte. "Save every minute" exhausts
that in ~70 days of continuous runtime. Recommendation: **write-back
policy**.

- Save when (a) state has been `fresh` or `tracking` for ≥ 5 min,
  AND (b) `|live − last_saved| ≥ 0.25°`.
- Wear-level across two backup slots (alternate magic byte) in the
  Phase 4.3 record.
- Typical full-day session: 4–10 writes per slot, comfortably inside
  endurance. Wild-drift worst case is bounded by the §6 rate limit.

On boot, restore persisted `live_offset` as the starting estimate but
mark `state = "tracking"` (not `fresh`) so the dashboard still
encourages a re-capture if the bot was physically moved.

## 9. API surface

A separate struct, not embedded in `OrientationData`: different update
rates (orientation 200 Hz, mounting 1 Hz) and different consumers
(control loop reads only `live_offset_rad`; dashboard reads the rest).

```cpp
namespace floppi::auto_orientation {

enum class MountingState : uint8_t {
    Fresh = 0, Tracking, Creeping, Unstable, OutOfBounds
};

struct MountingCalibrationStatus {
    float         reference_offset_rad;  // one-shot capture
    float         live_offset_rad;       // reference + drift
    float         drift_estimate_rad;
    float         drift_rate_rad_per_s;
    float         confidence;            // 0.0 – 1.0
    uint32_t      seconds_since_capture;
    MountingState state;
    uint8_t       lock_reason;           // bitfield: ITERM|RATE|ACC|SAT
    uint16_t      eeprom_writes;
};

} // namespace
```

## 10. Test plan

All on-stand, BNO085 100 Hz, control loop 200 Hz.

1. **Cable-drag injection.** Tape 10 g 5 cm off-axis at `t = 30 s`.
   Assert `live_offset` converges within 0.2° of the new equilibrium
   by `t = 60 s` (≤ 30 s after disturbance).
2. **Transient rejection.** At `t = 30 s` apply a 5° step setpoint
   for 1 s, then revert. Assert total `live_offset` excursion across
   the event is < 0.1° — the anti-windup gate did its job.
3. **Long-discharge soak.** Replay a recorded 18650 voltage curve
   over 30 min via programmable supply. Assert tracking within 0.3°
   of an offline-fit reference, never `unstable`.
4. **Bound enforcement.** Inject a synthetic 7° firmware bias.
   Assert clamp at 5° and API state `out_of_bounds`.
5. **Power-cycle persistence.** Run scenario 1 to convergence,
   power-cycle, assert restored offset within 0.1° of pre-cycle.
6. **EEPROM endurance.** Replay 30 days of typical use in fast-time;
   assert total writes per slot < 5 000.

---

## References

- Åström, K. J. and Wittenmark, B. *Adaptive Control*, 2nd ed.,
  Addison-Wesley, 1995. Ch. 1–2 (certainty-equivalence), ch. 3
  (deterministic self-tuners), ch. 5 (MIT rule and MRAC), ch. 11
  (supervision and practical issues).
- Ioannou, P. A. and Sun, J. *Robust Adaptive Control*, Prentice Hall,
  1996. Ch. 6 (MRAC robustness), ch. 8 (dead-zone and σ-modification
  — the theoretical basis for our gating).
- Ljung, L. *System Identification: Theory for the User*, 2nd ed.,
  Prentice Hall, 1999. RLS with forgetting factor and covariance
  resetting; relevant if we later upgrade to the RLS strategy.
- Lauszus, K. *A practical approach to Kalman filter*, TKJ Electronics
  / Balanduino source, 2012. Baseline 2-state filter we extend to 3
  states for the ESP32 variant.
- Anderson, B. D. O. "Failures of adaptive control theory and their
  resolution," *Communications in Information and Systems* 5(1), 2005.
  Cautionary tale on bursting and parameter drift — motivation for the
  hard bound in §6.
