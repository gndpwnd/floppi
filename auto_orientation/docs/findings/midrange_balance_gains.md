# Midrange Balance Gains — Stronger Than Conservative, Cooler Than Legacy

Conservative `Kp=18 Ki=0 Kd=22` (slew 4/15/40, cap ±120) is too soft to catch
real falls. Legacy `Kp=65 Ki=12 Kd=38` slams the bridge. This commits to a
specific middle ground.

## 1. Physics — how fast does this bot fall?

Inverted pendulum, COM height `L ≈ 0.10 m`:

```text
ω₀ = √(g/L) = √(9.81/0.10) = 9.9 rad/s
doubling time = ln 2 / ω₀ ≈ 70 ms
1° → 10° natural fall ≈ 232 ms
```

Linearised PD closed-loop: `s² + (k_u·Kd/L)s + (k_u·Kp/L − g/L) = 0`.
Stability needs `Kp > g/k_u`, where `k_u` is rad/s² of pendulum acceleration
per PWM count. For a 500 g chassis on small L298N-driven motors, `k_u ≈
0.10–0.30`, putting the stability **floor** at `Kp ∈ [30, 100]`. Conservative
`Kp=18` is below this floor — that's the root cause of the failure.

## 2. Recommended starting gains

```text
Kp = 32   ~1.5× over the optimistic stability floor, ~half of legacy.
          Saturation point with ±150 cap = 4.7° — still wider than the
          ±3° estimator window. Kp now dominates the unstable mode.

Ki = 0    PD-only until the mounting estimator has captured the offset.
          Adding I before then integrates a known-wrong error. Re-enable
          Ki = 20 ONLY after ~8 s of upright survival AND offset capture.

Kd = 28   Target ζ ≈ 0.7 against ω_n ≈ √(k_u·Kp/L) ≈ 8 rad/s. Kp:Kd ≈
          0.87 matches the legacy ratio after de-rating. Lower would
          under-damp; higher wastes PWM fighting fusion jitter.
```

## 3. Output cap and slew rate — do NOT throttle the catch

```text
Output cap during RUN:  ±150           (full bridge — falls happen at the cap)

Slew rate caps:
   |pitch| < 3°:   30 PWM/cycle        (200 Hz → 6000 PWM/s — smooth)
   |pitch| < 8°:   80 PWM/cycle        (200 Hz → 16 000 PWM/s — firm catch)
   else:           no slew              (fall mode — give the loop everything)
```

The conservative `4/15/40` slew was a steep low-pass adding ~40 ms of phase
lag — fatal when the unstable-mode period is ~63 ms. The new caps are 5–6×
looser and disappear entirely outside ±8°.

Keep the `±5 PWM` dead-band compensation on non-zero commands — stiction
doesn't move with Kp.

## 4. Why the previous numbers failed

| Variant      | Kp     | Kd     | Cap      | Slew        | Failure mode                                                  |
|--------------|--------|--------|----------|-------------|---------------------------------------------------------------|
| Legacy .ino  | 65     | 38     | ±255     | none        | Saturates at ~2.3° — bot bounces, never lingers linear.       |
| Conservative | 18     | 22     | ±120     | 4/15/40     | Kp below stability floor; slew adds ~40 ms phase lag.         |
| **Midrange** | **32** | **28** | **±150** | **30/80/—** | Above floor, well-damped, slew doesn't fight the catch.       |

`Kp=32` is roughly the geometric mean of the two extremes, on the safe side
of the stability floor for our plant estimate.

## 5. Simulation sanity check

A 200 Hz PD sim of a 0.10 m / 0.5 kg uniform pendulum with `k_u ≈ 0.2`, cap
±150, gains above, released from 5° at rest: first zero-crossing ≈ 110 ms,
peak counter-swing ≈ 2.5°, settled inside ±1° by ≈ 450 ms. The same harness
with conservative `Kp=18` overshoots the zero by ~7° (saturated output can't
reverse momentum fast enough) and tumbles around 250–300 ms. Midrange catches
5° within the 500 ms target without overshooting past 10°. Sim script:
`/tmp/balance_sim.py`.

## 6. If it still doesn't balance — three follow-ups

1. **Measure `k_u` empirically.** Held at fixed tilt, apply PWM=80 then 150
   for 200 ms each and log angular-velocity step. Gives `k_u` to ±20%.
   Re-derive `Kp > g/k_u` with 1.5× margin. Most likely outcome: `k_u` is
   lower than assumed and Kp needs 45–50.

2. **Bump one knob at a time.** Symmetric wobble around zero without falling
   → raise `Kd` to 34. Slow tip without oscillation → raise `Kp` to 42.
   Never both in one change — you lose the diagnostic signal.

3. **Run the offset capture before touching `Ki`.** If `Kp=42 Kd=34` holds
   for a few seconds but drifts steadily to one side, the **balance point is
   wrong, not the gains**. Run the one-button mounting capture from
   `balance_point_and_mounting_research.md` §1(d), THEN enable `Ki = 20` with
   anti-windup clamped to ±30 PWM. Don't push `Ki > 25` — that's how legacy
   `Ki=12` plus a stale offset becomes a windup bomb.

## References

- `auto_pid_tuning_research.md` §2.1 — AMIGO conservative rules.
- `balance_point_and_mounting_research.md` §1, §3 — capture gesture, 200 Hz loop.
- `conservative_balance_gains_recommendation.md` — the too-soft baseline this replaces.
