# Auto-Tuning an Unstable Plant — Reality Check
Last updated: 2026-05-12

## What the user asked

> "I want to just have everything be automatically tuned and figured out and perhaps more research is needed for automatic PID tuning dynamically — or are there too many variables, does it still need to be a step by step process? Make sense?"

Honest answer: **yes, fully dynamic auto-tuning of an inverted pendulum is achievable, but it must be sequential / step-by-step. There is no single-pass "figure everything out at once" algorithm that works on an unstable plant.** Here's why, and what the practical paths are.

## The chicken-and-egg problem

A self-balancing robot is an **unstable open-loop plant**: with no control, it falls. To tune a controller, you typically need data from operation. But to get operation data, you need a controller that can keep the bot upright. Catch-22.

This is fundamentally different from tuning, say, a temperature controller (open-loop stable) or a motor velocity loop (open-loop neutral). For those, you can do experiments with the loop open, gather data, fit a model, design a controller. For an inverted pendulum, you can't — the plant won't sit still while you measure it.

## What that means in practice

Auto-tuning an inverted pendulum requires either:

1. **A prior** — model-based control where mass / length / geometry are known a priori, dynamics are computed from physics, controller is designed analytically. This contradicts the **UNIVERSAL_BALANCE_BOT_VISION.md** goal (no per-bot config values).

2. **Bootstrap from a safe seed** — start with a known-conservative gain that won't damage anything, get the bot into a minimally-balanced regime, then adapt from there. This is the practical path. It's step-by-step.

3. **Hand-held data collection** — operator holds the bot stably while motors are commanded with chirps / steps; system ID runs while bot is physically supported. Then deploy the learned controller. Requires an extra operator step.

The user's preference (universal, just-works, no operator dependencies) points to **option 2** — bootstrap from a safe seed.

## The bootstrap protocol

Here's what "step-by-step auto-tuning" looks like for an inverted pendulum:

### Step 1: Safe seed gains
- Hardcoded Kp, Ki, Kd that work "okay" for a class of small inverted pendulums.
- Goal: keep the bot upright for at least 5 seconds without damaging the motors.
- Currently: Kp=50, Ki=1, Kd=20 with I-term clamped at 40 PWM. Iterated by hand 2026-05-12.

### Step 2: Mount-offset adaptation
- The bot's TRUE balance point is rarely where the operator captured it. A few degrees off → forward drift.
- `OnlineMountingEstimator` reads the PID's I-term and slowly shifts the mount offset to drive the I-term toward zero.
- Time constant: 20 s. Bias should converge in 1-2 minutes of upright operation.
- **This is already implemented and active.** Needs Ki>0 to have signal — that's why Ki=1 is kept rather than zero.

### Step 3: Plant identification (Phase 4.10, NOT yet implemented)
- Once the bot is balancing somewhat reliably, observe `α_pitch` vs `pwm_total` over time.
- Online recursive least-squares (RLS) estimator learns `K_motor` and `g_eff`.
- See `findings/dynamic_pwm_accel_learning.md` for full design.

### Step 4: Gain refinement from learned plant
- From K_motor and g_eff, compute "ideal" Kp/Ki/Kd via classical methods (pole placement, LQR with weights inferred from settling-time target).
- Apply slowly (rate-limited) so a bad estimate doesn't destabilize the bot.

### Step 5: Continuous online adaptation
- Battery sags, payload changes, wear → K_motor drifts. RLS keeps updating.
- Gains track K_motor changes automatically.

Steps 1-2 are running NOW. Steps 3-5 are the Phase 4.10 work — designed, not implemented.

## Why this isn't all-at-once

It's tempting to imagine a single "auto-tune button" that figures everything out at once. For an inverted pendulum, that would require:

- **Knowing the bot's natural period** before tuning — but the bot falls if you don't control it.
- **Knowing the motor PWM→torque mapping** before applying control — but the only way to measure it is to apply torque and observe response, which requires the bot to be alive.
- **Solving multiple unknowns simultaneously** when the controller's response is shaped by all of them at once — gain × geometry × motor × sensor latency all confound each other.

You can do it iteratively (cheap, sequential, robust). You can NOT do it in a single pass (no algorithm exists that's both safe and one-shot on an unstable plant).

## The relay-feedback auto-tuner (Phase 4.5b) — what it does and doesn't

There's already an auto-tuner implemented: `AutoPIDTuner` with `RelayFeedbackStrategy`. It uses Åström-Hägglund relay feedback to identify ultimate gain (Ku) and ultimate period (Pu), then applies Ziegler-Nichols.

**What it does well:** Once the bot is somewhat balancing, pressing `t` runs a 30 second relay-amplitude experiment and produces tuned gains.

**What it doesn't do:** It REQUIRES the bot to already be balancing within ±10°. It's not a cold-start tuner — it's a refinement step. So it's part of Step 4 above, not Step 1.

## Today's failure modes, mapped to the bootstrap protocol

| Symptom | Root cause | Step that's failing |
|---|---|---|
| "Balances ~1s, then motors max, falls" | Integral windup driving output to saturation before bot can recover | Step 1 — seed gains needed I-term clamp (now: 40 PWM cap, active) |
| "Always falls forward" | Mount-offset error, estimator hasn't converged yet | Step 2 — operator needs to keep bot upright long enough for the 20 s LPF to converge |
| "Robot shakes" | Kp too high or D-term noise leaking through | Step 1 — seed gains need adjustment |
| "Picked up → motors max" | No HELD detection | Solved by HELD state (now active, lenient) — independent of tuning |

## What "more research is needed" looks like

The user is right that this needs more research. The specific gaps:

1. **Better seed gains** that work for a wider class of bots without manual iteration. Could be derived from a generic small-inverted-pendulum model that requires only a single "size class" parameter (e.g. "phone-sized" / "lawn-mower-sized" / "Segway-sized"). Operator picks the class at first boot, never again.

2. **Faster convergence of the OnlineMountingEstimator.** Current 20 s time constant means an off-by-3° mount offset takes a full minute to half-correct. If the bot can't survive that long, the estimator never converges. Could use the PID I-term value AND the PID output direction-bias as two signals to converge faster.

3. **Online plant ID without breaking balance** (Phase 4.10). Requires careful design — RLS with forgetting factor, rate-limited gain updates.

4. **Bootstrap mode detection.** First N seconds after boot are different — operator is propping the bot up, mounting offset is uncertain. The control loop should be in a "high-tolerance" mode (loose gain bounds, fast adaptation) until first stable balance is observed, then transition to "stable" mode.

5. **A better answer to "what if the bot just can't balance with these motors?"** Sometimes the right answer is "lower the chassis CoG" or "buy stronger motors" — not "tune harder". The framework should be able to detect this case (e.g. K_motor learned to be too small for the observed g_eff) and surface it to the operator instead of failing silently.

## Bottom line

- **Hand-tuning is a temporary state.** It's where we are today only because Steps 3-5 of the bootstrap protocol haven't been built yet.
- **The plan to get out of hand-tuning exists.** See `dynamic_pwm_accel_learning.md` (Phase 4.10) and the bootstrap protocol above.
- **Step-by-step is the only way.** An inverted pendulum doesn't sit still long enough for a single-pass tuner. The bot has to be alive throughout the tuning process, so the tuning has to be incremental, layered, and safety-bounded.
- **The user is right to want this.** The current "hand-tune three numbers, recompile, flash" loop is exactly the friction the framework was designed to eliminate. We just haven't reached that part of the framework yet.

## See also

- [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md) — the design goal that frames this discussion.
- [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — the controller-architecture companion.
- [findings/dynamic_pwm_accel_learning.md](findings/dynamic_pwm_accel_learning.md) — Phase 4.10, the system-ID step.
- [findings/auto_pid_tuning_research.md](findings/auto_pid_tuning_research.md) — Phase 4.5b relay-feedback tuner that's already implemented.
- [findings/multi_axis_anomaly_handling_detection.md](findings/multi_axis_anomaly_handling_detection.md) — Phase 4.7c, separate from tuning but uses same online-adaptation philosophy.
- [findings/balance_failure_diagnosis_2026-05-12.md](findings/balance_failure_diagnosis_2026-05-12.md) — root-cause analysis of why hand-tuning isn't getting us there.
- [findings/latency_budget_2026-05-12.md](findings/latency_budget_2026-05-12.md) — latency cap on how aggressively we can tune today.
