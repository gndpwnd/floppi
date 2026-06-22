# Minimize-Accelerations Philosophy
Last updated: 2026-05-12

## TL;DR

The balancing robot's job is exactly one thing: **keep the gravity vector pointed through the wheel axis**. A correctly-tuned PID on pitch already does this. Fall detection, recovery state machines, special tipped-over modes — all of it is layered on top, and all of it tends to misbehave more than it helps.

So we don't do those things by default. The bot just balances, forever, with conservative gains.

## Why this is the same thing as "minimize accelerations"

For a two-wheel inverted pendulum:

- The body's only externally-applied torque (with motors off) is gravity acting through the chassis center of mass.
- That torque is zero ↔ gravity passes through the wheel axis ↔ pitch is at the balance point.
- The IMU measures total acceleration. When the bot is stationary at balance, the IMU sees only gravity pointing down through body-Z. When the bot is tilted, the same gravity vector projects partially onto body-X (the pitch axis), and that projection is non-zero acceleration.
- **Minimizing pitch error == minimizing lateral acceleration == minimizing the gravity-induced torque.**

The PID loop is doing this directly. When pitch is small, PID output is small, motors barely move. When pitch is large (a disturbance pushed the bot), PID output is large, motors react in proportion to bring pitch back to zero. That's the whole controller.

## What we removed (and why)

| Thing we removed | Reason |
|---|---|
| Auto-recovery from FALLEN | Misfired every time the operator picked up the fallen bot. The "settled and level" gate looks identical to "held still in mid-air at level pitch". |
| Slew rate limiter on motor command | Operator preference. Slew limits delay the inevitable; the right answer is a tighter PWM cap. Removed the dynamic-slew ladder entirely. |
| FALLEN → tipover auto-transition (default) | Per session of 2026-05-12: with conservative gains and a ±80 PWM cap, the bot tipped over spins benignly on its side (no traction → no damage). A fall-detect state machine that locks out motors is more annoying than useful. |
| Persisted PID gains | Persisting gains defeats the framework's premise (gains should adapt online). What persists: BNO055 calibration (hardware), mount offset (mounting geometry). Gains: hardcoded defaults every boot. |

## What we kept

| Kept | Why |
|---|---|
| Conservative gains: Kp=15, Ki=0, Kd=8 | Diagnosed root cause of "motors slam" was aggressive gains tuned for a different bot, not architecture. PD-first (Ki=0) until balance is proven — adding I before balance just integrates the wrong error and launches the wheels. |
| PWM cap ±80 during RUN | Below the L298N+motor saturation point on this chassis. Full ±255 still reserved for non-balance modes (motor test, future driving). |
| Stiction dead-band ±25 PWM | Cheap brushed DC motors won't turn below ~20–30 PWM. The dead-band feed-forward breaks stiction without growing the I-term. |
| D-term measurement LPF (15 ms τ) | Kills BNO055 fused-Euler quantization noise (~0.05°/step) that would be amplified into 250 PWM/quantum spikes by Kd at 200 Hz. (Superseded for the Uno reference build by Item 1's raw-gyro D-term — see Phase 4 §Item 1.) |
| HELD state (gated by `USE_BALANCE_HELD_DETECTION`, default ON) | Operator picks up bot → motors stop. Operator sets bot back down upright → balance resumes. No manual restart command needed. |
| Saturation timeout (3 s @ ≥70 PWM) | Hardware-fault guard — catches stuck wheel, wiring problem, mount-offset way off. Not a fall guard. |
| **Soft-cutoff at ±25° pitch in RUN** (Phase 4 Item 4, 2026-05-12) | When the bot tips past the linear region the PID still computes (keeping I-term and OnlineMountingEstimator state coherent) but `motors.stop()` zeroes the output. Bot auto-recovers the moment it's righted — no state transition, no operator restart. Replaces the old saturation-timeout→FALLEN transition (which was a sticky-FALLEN-era hack now obsoleted by soft-cutoff). Mirrors `osoyoo_abc.ino` `if (angle > 30) pwm = 0` — see `findings/research_osoyoo_reference_implementation.md` §5f. |

## Compile-time switches

In `auto_orientation/platformio.ini` under `[env:arduino_uno_minimal]`:

```
build_flags = ... -D USE_BALANCE_HELD_DETECTION
              # to re-enable fall detection, add:
              # -D USE_BALANCE_FALL_DETECTION
```

| Flag | Default | Effect when defined |
|---|---|---|
| `USE_BALANCE_HELD_DETECTION` | ON | Picking up bot → HELD state (motors off). Setting bot back down upright → auto-resume. |
| `USE_BALANCE_FALL_DETECTION` | OFF | Pitch beyond `tilt_limit_deg` (10°) → FALLEN state. FALLEN is sticky (operator must press button / send `c` / `R` to restart). |

`USE_BALANCE_FALL_DETECTION` exists because some chassis configurations are dangerous on their side (top-heavy, exposed connectors, large props on a multirotor frame). It's a compile-time choice per build, not a runtime toggle — keeps the firmware behavior unambiguous.

## What the bot looks like in operation

**Boot → balance:** power on → BNO055 cal restored from EEPROM → mount offset restored from EEPROM → 2 s grace → enters RUN. No serial command needed.

**Steady-state:** PID running at 200 Hz, motors very quiet, occasional ±20 PWM corrections.

**Operator pokes bot at ~5° lean:** PID output spikes to ~75 PWM briefly, bot recovers in <1.5 s, returns to quiet steady-state.

**Operator picks up bot (gyro magnitude rises):** within 150 ms, HELD state. Motors stop. Operator carries bot around. Sets bot back down upright. After 800 ms of "still + level + on a surface", RUN resumes. No commands.

**Operator knocks bot past 10°** (default config, fall-detect *off*): PID output saturates at ±80, bot completes its fall, motors spin uselessly on the side, no harm. Stand the bot back up → PID grabs it again. No restart command needed.

**Operator knocks bot past 10°** (fall-detect *on*): FALLEN state, motors stop. Sticky — stays in FALLEN until operator presses the button or sends `c` / `R`.

## Why this took us so long to figure out

The session of 2026-05-12 spent a lot of cycles chasing complexity (slew limiters, dynamic output caps, fall recovery heuristics) when the root cause was simply that the .ino's `Kp=65 Ki=12 Kd=38` gains — designed for a heavier Mega-based bot — saturate the controller into nonlinear behavior at any small disturbance on this chassis. Once gains were brought into the linear region, the rest of the layered safety machinery became visible as the noise it was.

This document exists so the next time someone is tempted to add a state machine on top of a PID, they read it first.

## See also

- [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) — three-tier plan (we executed Tier 1 + Tier 2; Tier 3 deferred until needed).
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) — KI-2…KI-20, severity-ranked. KI-13 (slew limiter wrong primitive) and KI-11 (no HELD state) addressed in this session.
- [findings/balance_failure_diagnosis_2026-05-12.md](findings/balance_failure_diagnosis_2026-05-12.md) — root-cause analysis of "motors slam" behavior.
- [findings/conservative_balance_gains_recommendation.md](findings/conservative_balance_gains_recommendation.md) — gain numbers we landed on.
- [findings/balance_held_fallen_state_machine.md](findings/balance_held_fallen_state_machine.md) — HELD/FALLEN design.
- [archive/session_records/2026-05-12_uno_balancing_hardware.md](archive/session_records/2026-05-12_uno_balancing_hardware.md) — bench session that produced this philosophy.
