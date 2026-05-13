# Disturbance Rejection and Feedforward Compensation

Research notes for the self-balancing-robot layer in `auto_orientation/`,
complementary to the online adaptive balance-point work. Adaptive
estimation tracks *slow* drift of the setpoint; this layer rejects *fast,
known, or observable* disturbances. Target: Arduino Mega 2560 + BNO085 +
L298N, with eventual reuse in Phase 7 gimbal / multirotor.

---

## Recommendation summary

- **Ship a minimal push-recovery state machine in Phase 4** (gyro-spike +
  innovation-mismatch detector, 2 s gain bump, freeze adaptation while
  recovering). Costs ~60 B RAM, ~400 B flash, no new hardware.
- **Defer wheel-encoder feedforward and cascade velocity control to
  Phase 7** (or a 4.5 hardware revision). Correct in principle but
  hardware + retune cost; v1 must first prove pitch-only is stable.
- **Use BNO085 linear-acceleration as the v1 feedforward channel.** It
  is already produced by the on-chip fusion and lets the inner loop
  pre-empt shocks rather than wait for I-term windup.

---

## 1. Disturbance taxonomy

| Class | Frequency | Observability | Cheapest layer |
| --- | --- | --- | --- |
| Tether / cable drag | DC | steady I-term | feedforward (static offset) |
| Battery / payload shift | DC after change | new equilibrium | adaptive setpoint |
| Thermal IMU bias | very slow | Kalman bias state | Kalman estimator |
| Push / shove | step | gyro + accel spike | feedback (gain bump) |
| Foot bump / tile edge | transient | accel impulse | derivative anti-windup |
| Floor unevenness | low-band noise | accel Z, pitch jitter | LP-filter the reference |
| Mechanical resonance | periodic narrow-band | pitch-error PSD | notch filter |
| Wind / gust (Phase 7) | step + slow | unmeasured | feedback + estimator |
| Wheel slip | event | encoder vs PWM mismatch | requires encoders |

Goodwin–Graebe–Salgado, *Control System Design* (Prentice Hall, 2001),
§10.4 makes the central point: feedforward only works for *measurable or
modellable* disturbances; everything else falls back on feedback
robustness and adaptive estimation.

## 2. Compensation strategies per class

- **DC tether torque.** Static torque offset added to the controller
  output, initialised from the steady-state I-term during a "tether
  capture" gesture (Goodwin–Graebe–Salgado §10.4 static disturbance
  estimator).
- **Step recovery.** Derivative anti-windup (clamp Kd when gyro
  saturates) + a brief P/D gain boost — §3.
- **Push detection.** Gyro spike + innovation mismatch → `RECOVERY` for
  ~2 s — §3.
- **Periodic resonance.** Drop a 2nd-order IIR notch at the offending
  frequency. The flight controller's `filters.h` biquad primitive ports
  directly.
- **Floor unevenness.** Low-pass the *reference* (not the measurement)
  at 3–5 Hz so micro-bumps do not look like setpoint changes.

## 3. Push-detection algorithm

```text
state = NORMAL
if |gyro_pitch| > 50 deg/s AND duration > 50 ms AND
   |measured_pitch_rate - predicted_pitch_rate| > 30 deg/s:
    state = RECOVERY
    t_recovery_start = now
    Kp_eff = 1.5 * Kp
    Kd_eff = 1.3 * Kd
    freeze_adaptive_balance_offset = true
    log_event(PUSH, magnitude=peak_gyro, t=now)

if state == RECOVERY and (now - t_recovery_start) > 2000 ms:
    state = NORMAL
    restore Kp, Kd, freeze flag
```

**False-positive concerns.**

- *User picks the bot up.* Lateral / vertical accel sustains near 1 g
  while motor PWM stays nonzero — qualitatively unlike a push. Add a
  gate: only enter `RECOVERY` if armed and in `RUNNING` substate.
- *Outer-loop commanded turn.* The innovation gate
  (`measured − predicted`) suppresses anomalies the controller itself
  caused.
- *Auto-tune relay.* Suppress detection during `TUNING`.

Cost: ~60 B RAM (4 floats + 2 timestamps + enum), ~400 B flash.

## 4. Wheel-encoder feedforward

L298N is open-loop: PWM in, no speed out. Adding optical-disc or
Hall-effect encoders unlocks three capabilities:

1. **Tether-drag estimation.** Required-PWM-for-zero-velocity ≠ 0 ⇒
   drag torque sign and magnitude. Feeds the static feedforward term.
2. **Cascade control** — outer wheel-velocity loop around inner pitch,
   §7. Eliminates the translational drift Lauszus / Balanduino fix with
   an integral of encoder counts.
3. **Slip detection.** PWM up but encoder flat ⇒ low traction; reduce
   commanded acceleration, warn the dashboard.

**Recommendation:** Phase 7 (or a "Phase 4.5 hardware revision").
Encoders need chassis modification, ISR-driven counting on a Mega timer,
and a second tuning pass. Phase 4 v1 must first prove pitch-only +
adaptive setpoint + push recovery is stable; that is the irreducible
core. Counter-argument: encoders are ~$3 and unlock nBot's topology — a
legitimate case for promoting them earlier if user testing demands it.

## 5. IMU-acceleration feedforward

BNO085 emits `linear_acceleration` (gravity-removed, body frame) as a
free SH-2 report. Pseudocode:

```text
read linear_accel (a_x, a_y, a_z)
a_horiz = a_x   # forward axis after mounting rotation
if |a_horiz| > A_THRESH:        # e.g., 2 m/s^2
    u_ff = K_a * (-a_horiz)     # push back against the shove
    u_total = u_pid + u_ff
```

`K_a` is the only new gain — small, since feedforward is a helper not
the primary path. Bryson–Ho, *Applied Optimal Control* (Hemisphere
1975), §5.3 shows the optimal disturbance-feedforward gain has a closed
form once the plant model (K_u, T_u from relay tune) is known.

## 6. Wind / drag rejection

Aerodynamic drag scales as `0.5 ρ C_d A v²`. For a tabletop balance bot
at ~0.5 m/s indoors, drag is < 0.01 N — negligible. Matters for the
**Phase 7 gimbal**: hand-held or vehicle-mounted gimbals see 1–10 m/s
relative air. Cheapest path is augmenting the Kalman state with a
wind-torque term driven by the IMU acceleration residual (Anderson–Moore
*Optimal Filtering*, Prentice Hall 1979, ch. 10). Pitot probes are
overkill. Defer until the gimbal application is funded.

## 7. Cascade control architecture

```text
[ wheel_vel_ref ]──►(+)──►[ outer PI ]──► pitch_ref ─►(+)──►[ inner PID ]──► PWM ──► motors ──► chassis
                     ▲                                    ▲                                       │
                     │                                    │                                       │
                  encoder───────────────────────────  pitch_meas ◄─────── BNO085 ◄────────────────┘
```

The outer loop's authority is clipped (e.g., ±2° pitch_ref) so it can
never command an unrecoverable lean.

- **Single-loop** (Brokking YABR 2017): simpler, no encoders, drifts
  translationally — the bot wanders unless someone nudges the offset.
- **Cascade** (Segway / nBot / Balanduino): holds station; tether drag
  shows up as wheel-velocity error and is integrated out automatically.
  Cost: encoders + one extra PI.

## 8. Limits of feedforward

Feedforward rejects only disturbances we can measure or model. It
cannot help with random unmeasured pushes — those need feedback
bandwidth and recovery-mode logic. The framework should communicate
this honestly:

| Layer | Handles |
| --- | --- |
| Feedback PID | Everything, eventually, within bandwidth |
| Adaptive estimation | Slow setpoint drift (mounting, payload) |
| Feedforward | Known / modelled disturbances (tether, lateral accel) |
| Recovery mode | Detected transient events (push, foot bump) |

Relying solely on feedforward against an unknown disturbance violates
the internal model principle (Goodwin–Graebe–Salgado §10.4) and is
provably non-robust. Marketing copy must not overpromise.

## 9. API extensions

Additions to the planned `BalanceApp` interface:

- `disable_adaptation()` / `enable_adaptation()` — debug aid; lets a
  tuner observe pure feedback without the setpoint moving under it.
- `report_disturbance_event(type, magnitude, t)` — emitted on every
  push / resonance / slip detection. Dashboard plots events as markers
  on the pitch trace.
- `set_recovery_mode_gains(kp_mult, kd_mult, duration_ms)` — exposes
  the three knobs of §3; default `(1.5, 1.3, 2000)`.
- `capture_tether_offset()` / `set_tether_offset(torque_nm)` —
  measures or sets the static feedforward bias.

All serialise to the existing JSON output mode; no new dependencies.

## 10. Phase fit

**Phase 4 (v1):** ship only the push-recovery state machine (§3) and
the linear-acceleration feedforward term (§5). Both are zero-hardware
cost and directly address the user's cable / push scenarios. RAM < 100
B on Mega.

**Phase 7+:** wheel-encoder feedforward, cascade velocity loop, notch
filter for resonance (needs a spectrum estimator on AVR — feasible but
invasive), wind estimator. These pay off most for gimbal / multirotor
reuse, not for a tabletop balancer.

The opposing argument is real: encoders are cheap and the cascade
topology is the genuine industry standard. The deciding factor is the
project's stated bare-bones principle — prove the irreducible core
first, then add only what user testing demands. Åström–Wittenmark,
*Adaptive Control* (2nd ed., Addison-Wesley 1995), ch. 7 explicitly
warns against stacking adaptive layers before the underlying feedback
loop is provably stable in isolation.

---

## References

- Goodwin, G., Graebe, S., Salgado, M. *Control System Design.*
  Prentice Hall, 2001 — chs. 9–10 (disturbance models, feedforward,
  internal model principle).
- Bryson, A., Ho, Y.-C. *Applied Optimal Control.* Hemisphere, 1975 —
  §5.3 (optimal feedforward gain).
- Åström, K., Wittenmark, B. *Adaptive Control.* 2nd ed.,
  Addison-Wesley, 1995 — ch. 7 (gain scheduling, layering).
- Anderson, B., Moore, J. *Optimal Filtering.* Prentice Hall, 1979 —
  ch. 10 (Kalman with augmented disturbance state).
- Brokking, J. *YABR — Yet Another Balancing Robot*, 2017 (single-loop
  reference).
- Lauszus, K. *Balanduino source*, 2012 (cascade reference).
