# Conservative Balance Gains — Drop-In Recommendation

> See also: [../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — current project direction; the conservative-gains argument here is the foundation it builds on.

Target: ~500 g, 2 wheels, L298N + cheap brushed DC, BNO055 NDOF (~100 Hz fused),
PID @ 200 Hz (dt = 5 ms), current cap ±150 PWM during RUN. Symptom: legacy
gains `Kp=65 Ki=12 Kd=38` saturate the bridge at ±3–5° tilt, so the bot never
lingers long enough for the online mounting estimator (which observes the
PID I-term) to converge on the true balance point.

The strategy is **PD-first**: hold the bot upright with stiff-but-bounded
position feedback and well-damped rate feedback, leaving I=0 until the
mounting estimator (or a manual capture) has nailed the balance point.
Adding I before the offset is known just integrates a known-wrong error and
launches the wheels. Restore a small Ki only after the bot can survive
~10 s upright. This follows the Åström–Hägglund robustness school
(AMIGO, *Advanced PID Control*, 2006): begin with the most-robust
controller that stabilises, then sharpen.

D is left on **measurement** (Arduino `PID_v1` default) so a future
setpoint nudge from the outer wheel-velocity loop does not produce a
derivative kick into a saturated bridge.

```
RECOMMENDED:
  Kp = 18  (rationale: ~28% of legacy 65. With ±150 cap, saturation
            now begins at ~8.3° tilt instead of ~2.3° — bot stays in
            the linear region long enough for I-term observation.)
  Ki = 0   (rationale: PD-only until mounting estimator converges.
            Re-enable Ki = 30 only AFTER bot survives 10 s upright
            and the balance-point offset has been captured.)
  Kd = 22  (rationale: ~0.58·Kp. Gives damping ratio ~0.7 on the
            linearised pendulum (~3 Hz natural freq for this mass/
            geometry). Lower than legacy 38 because cheap DC + L298N
            adds its own velocity damping; over-damping wastes PWM.)
  D-on:    measurement  (rationale: avoids derivative kick when the
            outer velocity loop or auto-tuner nudges the setpoint,
            and dampens BNO055 fusion jitter symmetrically.)
  Output cap during RUN: ±120
            (rationale: 20% headroom below the ±150 motor cap so the
            slew limiter, not the PWM clip, governs behaviour. Clip
            = nonlinearity = phase loss = oscillation.)
  Slew rate caps:
    |pitch| < 3°:  4 PWM/cycle   (~800 PWM/s — silky in the linear zone)
    |pitch| < 8°:  15 PWM/cycle  (~3000 PWM/s — firm catch, no slam)
    else:          40 PWM/cycle  (~8000 PWM/s — full authority before fall)
  Other tweak: add a 5-PWM motor dead-band compensation (add ±5 to any
            non-zero command) so the L298N actually starts turning at
            small commands — otherwise the PD loop fights stiction and
            the I-term has to grow large to break free.
```

## Verification protocol (10 minutes)

1. Flash with the block above (Ki=0, cap ±120).
2. Place on stand, run. Expect a slow ±2° wobble — that's correct for PD.
3. If wobble grows (sustained oscillation), Kd is too low: try Kd=28.
4. If bot feels mushy and falls slowly, Kp is too low: try Kp=24.
5. Once it survives ~10 s, capture mounting offset (button gesture in
   `balance_point_and_mounting_research.md`), then set Ki=30 and verify
   wobble decays toward zero over ~3 s.

## Why these numbers, briefly

Legacy `Kp=65` was tuned against a heavier chassis on a stiffer bridge
(legacy cap ±255). Scaling for the ±150 cap and lighter mass: the linear
region of a PD law is `|pitch| < u_cap / Kp`. At Kp=65, ±150 saturates
at 2.3°. At Kp=18, ±120 saturates at 6.7° — comfortably wider than the
mounting estimator's working envelope (±3° while observing I-term
drift). Kd=22 places the closed-loop damping near 0.7 for a 500 g
inverted-pendulum model with wheel radius ~30 mm; classic ZN would
suggest Kd ≈ Kp·T_u/8 ≈ 0.6·Kp, matching our 22/18 ratio.

References used: `auto_pid_tuning_research.md` §2.1 (AMIGO conservative
rules, relay-feedback safety envelope), `balance_point_and_mounting_research.md`
§3 (cascaded PID structure, 200 Hz tick rationale, 2-state Kalman).
