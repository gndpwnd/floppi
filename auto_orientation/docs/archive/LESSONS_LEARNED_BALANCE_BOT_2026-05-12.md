# Lessons Learned — Balance-Bot Reference App (2026-05-12)

A durable record of what went wrong, what we learned, and rules to follow so future sessions don't repeat the same mistakes. Written at the end of a long bench-iteration day that produced ~15 KB of source code changes, 6 new vision docs, and 18+ research findings.

> **Read this BEFORE iterating on the balance-bot reference app again.**

## The single biggest lesson

> **Hand-tuning PID gains for an unstable plant in a tight iteration loop is a trap.** Each tweak feels like progress; the cumulative behaviour is a random walk. The right move is to stop, research what's been published in the field for the specific problem (inverted pendulum control), and implement an established algorithm. We spent most of a day in the trap before recognising it.

Symptoms of being in the trap:
- "Let me bump Kp slightly..." (this happened seven times)
- Editing `kDefaultInitialK*` constants in `balance_app.cpp` more than twice without rebuilding the conceptual model.
- Adding a new safety state machine (HELD, FALLEN, slew limiter, dead-band, saturation timeout) to compensate for the unstable behaviour rather than fix the underlying gain choice.
- Feeling like the bot is *almost* there.

## The most important second lesson

> **Hand-tuning is itself a per-bot calibration step that contradicts the framework's stated vision.**

Per `docs/UNIVERSAL_BALANCE_BOT_VISION.md`: the goal is "drop the firmware on any small inverted pendulum bot with a calibrated IMU and it works." If we have to hand-tune Kp/Ki/Kd for every operator, we've failed the vision — regardless of whether any individual bot balances.

This is why Phase 4.10 (RLS plant identifier) is the actual answer, not "let me tune the gains one more time."

---

## Concrete gotchas to remember

### 1. Don't trust the .ino reference's gains

The archived `SelfBallancingRobot3.ino` uses `Kp=65 Ki=12 Kd=38`. **Those numbers were tuned for one specific bot** (heavier Mega-based chassis with hand-tuned `PITCH_OFFSET=-8.6°`). Plugging them into a different bot saturates the controller into nonlinear behaviour at any small disturbance.

→ When seeding a new bot's gains: **start at half the reference values and adjust upward**, not the reference. Or: skip to Phase 4.10 RLS, which discovers the right scale.

### 2. NDOF latency is real and matters more than you think

BNO055 NDOF mode has 20-40 ms of group delay. At a small inverted pendulum's ~600 ms natural period, that's 12-25° of phase lag. **Numerical differentiation of fused pitch makes it worse** because differentiating a quantised + LP-filtered signal adds another ~10-15 ms of effective latency through the 0.05° quantisation × Kd amplification path.

→ The fix is to use **raw gyro as the D-term source directly** (Phase 4 Item 1, now landed). Don't differentiate the fused angle.

### 3. Auto-recovery from FALLEN is a death trap

The "settled and level" gate looks identical to "held still in mid-air at level pitch". Every time the operator picked up the fallen bot to lift it back to upright, the firmware fired the motors. This bug took multiple iterations to identify.

→ FALLEN is sticky if defined at all; soft-cutoff (motors silent, PID still ticking) is the better default. **Don't add auto-recovery state machines on unstable plants.**

### 4. The OnlineMountingEstimator placeholders were a silent killer

`balance_app.cpp:418-423` passed `windup_active=false, gyro_pitch_dps=0.0f` as hardcoded placeholders. The estimator's disturbance-freeze gates have been broken **the entire session** because of this. Every pickup contaminated the mount-offset estimate. Compounded over time, the bot's perceived "zero" drifted further from true balance.

→ When wiring an adaptive estimator: **never pass placeholder zeros to its freeze gates.** Plumb the real signals from day one, even if they're noisy.

### 5. Stiction floor is a tradeoff, not a tuning knob

The L298N + cheap brushed DC motors need ~15-25 PWM to break friction. Setting stiction floor = 25 made tiny corrections over-shoot (motor jumps from 0 → 25 in one tick); setting it to 0 means small corrections produce nothing.

→ The right answer for a balance bot is probably **dither** (high-frequency PWM noise that shakes the motor through the dead zone) OR a **one-shot-per-direction-reversal stiction kick** (not unconditional rounding). We didn't implement either; current default is 0 floor with Kp high enough to cross the threshold naturally.

> **2026-05-18 update:** the "Kp high enough to cross the threshold naturally" assumption was wrong for the current bench bot. Canned `m` motor test at PWM 90 produced *no* wheel motion; PWM 200 spun cleanly. Real threshold today is ~100 PWM, not 15-25. With stiction floor 0 + low initial Kp, the bot's *entire operating range* of small PID outputs was a dead zone. This single fact explains the whole 2026-05-12 tuning frustration — see [session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md](session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md). **The estimate "won't move below ~15 PWM" must be measured on the current hardware, not assumed.** Add `m`-at-realistic-PWM to the pre-tune diagnostic protocol.

### 6. Slew rate limiting masks the real problem

Adding a slew limiter to the motor command makes the bot feel smoother for a moment, but it adds phase lag to the control loop and delays the inevitable. **If your controller is too aggressive, lower Kp — don't add slew.** The user explicitly told us this and was right.

### 7. The chicken-and-egg of auto-tune

The relay-feedback auto-tuner (Phase 4.5b, already implemented) requires the bot to already be balancing within ±10°. It refines existing gains; it doesn't bootstrap from nothing. **The genuinely hard problem is the cold-start auto-tune** — that's what Phase 4.10's RLS solves, because it can learn during normal operation (any motion produces excitation), not in a dedicated tuning experiment.

### 8. PID loop should run in a hardware-timer ISR, not in loop()

When PID ticks inside `loop()` next to `Serial.println`, the `dt` jitters by 1-3 ms. PID's time-scaling math is correct but the D-term noise dominates because differentiation amplifies dt noise. Every successful balance-bot reference (Osoyoo, Lauszus, Balanduino) uses a hardware timer ISR. We landed MsTimer2 in Phase 4 Item 3.

→ **MsTimer2, not TimerOne, on Uno** — TimerOne owns PWM pins 9 & 10, and the L298N uses pin 10 for ENB. This bit us during the Phase 4 implementation.

### 9. Trust the prior research before iterating

We had 12+ research docs in `docs/findings/` written by parallel research agents on 2026-05-12 morning. We didn't read them carefully before bench iteration started. Most of what went wrong in the iteration loop was already documented (e.g., `balance_failure_diagnosis_2026-05-12.md` called out the gain × NDOF noise pathway BEFORE we hit it on the bench).

→ **When starting a bench session, re-read the relevant `findings/` docs first.** Don't trust your own ad-hoc PID intuition for an unstable plant.

### 10. The drone doesn't auto-tune either

The flight_controller in `/home/devel/floppi/flight_controller/` uses **hardcoded PID gains** from dRehmFlight (KP_ROLL_RATE=0.15, KP_PITCH_ANGLE=0.20, etc., all in `include/config.h`). It "just works" because quads are open-loop *neutral* in attitude (not unstable) and have 4 motors of redundant authority. A balance bot is open-loop *unstable* and has 2 motors of ground-friction-limited authority — that's a categorically harder control problem. **We can't just copy the drone's "use community-tuned constants" approach because the constants for THIS bot don't exist (yet).**

The drone's auto-cal research (`flight_controller/docs/findings/auto-calibration-research.md`) is excellent and applies to sensor calibration (gyro bias, accel offset). It does NOT auto-tune the PID controller. That's the gap Phase 4.10 fills for the balance bot.

---

## Rules for the next session

1. **Read this doc, then `AUTO_TUNING_REALITY_CHECK.md`, then `UNIVERSAL_BALANCE_BOT_VISION.md` BEFORE touching code.**
2. **If you find yourself editing `kDefaultInitialK*` for the third time, STOP. The PID gains aren't the problem.** Look upstream: BNO055 cal health, motor wiring, mount offset accuracy.
3. **Never claim "the bot can't balance" until Phase 4.10's RLS estimator has converged** (covariance P below ~0.01). If it has converged and gains haven't stabilized the bot, the problem is hardware, not control.
4. **Don't add another state machine.** We already have IDLE / CAPTURE_MOUNTING / AUTO_TUNE / RUN / HELD / FALLEN — that's the limit. If you're tempted to add another, ask: can this be a sensor check + a single `if` statement instead?
5. **Don't persist PID gains.** They're meant to be learned from operating data per the framework's vision. Persisting them is admitting defeat.
6. **Always check `s` output first when debugging.** It prints state, pitch, mount offset, output, gains, K_motor, adaptation status. If `s` shows pitch frozen at 0.00, the BNO055 isn't being read — that's a different problem from PID tuning.
7. **Watch flash usage.** The Uno build is at 99.9% as of 2026-05-12. Any new feature must come with a corresponding savings somewhere.
8. **Hardware validation is what counts.** Native tests show the algorithm works on synthetic data. They don't show the bot balances. Only a bench session does that. Tests are a lower bar than working firmware.

---

## What still needs hardware validation

Until the bot is plugged back in and operated, we don't know if Phase 4.10 actually works in practice. The pre-conditions we've already verified:

- ✅ Build clean at 99.9% flash, 78.4% RAM.
- ✅ Synthetic α-data convergence test: K_est = K_true, 0.0% error after 20 s.
- ✅ Native test suite for the PID controller passes parity test with PID_v1.
- ✅ Code review by 5 parallel research agents converged on the same algorithm.

What we DON'T know until bench validation:

- ❓ Does RLS converge on a real bot with real motor noise + IMU drift in <60 s?
- ❓ Does the closed-form Kp/Kd from K_motor actually balance the bot? Or does the linear-pendulum approximation break down for this chassis?
- ❓ Does the 5%/s rate-limited gain ramp produce smooth behaviour, or does the bot fall during the ramp-up window?
- ❓ Does the bootstrap-window 5 s freeze let the bot recover from prop-up before adapting?
- ❓ Are the σ-modification bounds (0.02..5.0) right for this bot?

The next bench session is about answering those questions, not tuning more gains.

---

## Files written this session (for the historical record)

Vision / direction:
- `docs/UNIVERSAL_BALANCE_BOT_VISION.md`
- `docs/MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`
- `docs/MULTI_ORIENTATION_BALANCE_VISION.md`
- `docs/AUTO_TUNING_REALITY_CHECK.md`

Plans / coordinating:
- `docs/IMPLEMENTATION_PLAN.md`
- `docs/PHASE_4_STRUCTURAL_FIXES.md`
- `docs/KNOWN_ISSUES.md`

Findings (this session):
- `findings/balance_failure_diagnosis_2026-05-12.md`
- `findings/conservative_balance_gains_recommendation.md`
- `findings/balance_held_fallen_state_machine.md`
- `findings/latency_budget_2026-05-12.md`
- `findings/dynamic_pwm_accel_learning.md`
- `findings/multi_axis_anomaly_handling_detection.md`
- `findings/bootstrap_protocol_unstable_plant.md`
- `findings/research_osoyoo_reference_implementation.md` (Osoyoo source at `~/tmp/osoyoo/`)
- `findings/research_inverted_pendulum_control_methods.md`
- `findings/research_open_source_balance_bots.md`
- `findings/research_universal_zero_knowledge_tuning.md`
- `findings/research_multi_orientation_balance_feasibility.md`
- `findings/research_flight_controller_pid_lessons.md` (forthcoming, sibling cross-project synthesis)

Source:
- `src/control/plant_identifier.{h,cpp}` (Phase 4.10 RLS auto-tune)
- `src/control/pid_controller.{h,cpp}` (compute_with_rate, i_term_limit, d_term_lpf_tau additions)
- `src/applications/balancing_robot/balance_app.{h,cpp}` (HELD/FALLEN/soft-cutoff/RLS integration)
- `src/sensors/sensor_base.h` (raw-gyro/raw-accel virtuals)
- `src/sensors/bno055.{h,cpp}` (override the virtuals)
- `src/main.cpp` (MsTimer2 ISR, dependency injection of PlantIdentifier)
- `tests/test_plant_identifier.cpp` (7 passing native tests)

Session records:
- `archive/session_records/2026-05-12_uno_balancing_hardware.md`
- `archive/session_records/2026-05-12_evening_balance_iteration.md`
- `archive/session_records/2026-05-12_evening_phase4_landing.md`

---

## Acknowledgments

The breakthrough came when the user pushed back: *"you are basically just making your own PID auto tuning algorithm but it wont work when put on a completely different robot make sense? shouldn't you research Automated PID tuning with inverted pendulums and stuff?"* That stopped the iteration loop and triggered the research-first pivot. Without that intervention this would still be a hand-tuning session.

> *If the user has to stop you because you're hand-tuning an unstable plant, you've already failed at this. Read this doc next session.*
