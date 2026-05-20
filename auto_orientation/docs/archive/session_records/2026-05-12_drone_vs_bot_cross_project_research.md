# Session Record — 2026-05-12 Late Evening — Drone vs. Balance Bot Cross-Project Research

Continuation of the Phase 4 landing session. The user asked a sharp question: *"the flight_controller keeps the drone stable in the air without flipping over so how does it do its PID tuning? Can we not use the same idea? I am now questioning why our PID has become so complicated... is BNO055 calibration off or something?"*

Three parallel agents were dispatched. Results captured here for future reference.

## The user's question, mapped to three angles

1. **What does the drone actually do?** — Code review of `flight_controller/` PID architecture.
2. **Is the BNO055 cal broken?** — End-to-end audit of the cal flow.
3. **Why is a drone simpler?** — Control-theory comparison drone vs. balance bot.

## Headline answers

### 1. What does the drone actually do?

The flight_controller uses **hardcoded PID gains from dRehmFlight** (`KP_ROLL_RATE=0.15`, `KP_PITCH_ANGLE=0.20`, etc. in `flight_controller/include/config.h:353+`). **There is no online auto-tuning.** Operator can override via `tune_kp_*` globals if they want. The auto-calibration that DOES exist is **sensor-side only** (gyro bias at boot, accel offsets, ESC range) per `flight_controller/docs/findings/auto-calibration-research.md`.

The reason these hardcoded constants "just work":
- Quad attitude dynamics are **open-loop neutral** — not unstable.
- 4-motor mixer gives wide tolerance to mistuning.
- dRehmFlight's defaults have been community-tuned on similar small-quad classes for years.

### 2. Is BNO055 cal off?

**Most likely no.** The cal flow (restore from EEPROM → wizard if missing → push offsets into chip) is structurally sound. The audit identified 6 failure modes; the most likely real problem in our session was **failure mode 4f**: the BNO055 is reading fine, but the captured mount offset is biased by several degrees, and the OnlineMountingEstimator can't correct it because its disturbance-freeze gates were broken (the placeholder-zero bug, identified during Phase 4 and now fixed in Item 2).

**First bench test:** power up, send `s`, tip the bot ±10° by hand and watch pitch in subsequent `s` outputs. If pitch tracks smoothly within ~1°, cal isn't the problem.

### 3. Why is a drone simpler?

**The single most important difference**: drone linearised hover dynamics are a **pure double integrator**:

  θ̈_drone ≈ (1/I) · τ_motor

Balance bot linearised pendulum dynamics have a **gravity term that destabilises**:

  θ̈_bot ≈ (1/I) · τ_motor + (m·g·L/I) · θ

That second term — `+(m·g·L/I)·θ` — is the killer. It makes any pitch tilt grow exponentially with a time constant ≈ 120 ms on bench-scale hardware. Every downstream asymmetry (gain forgiveness, data gathering, latency tolerance, disturbance authority) traces back to that one sign in the equation.

## User's intuition rating

> "Can we not use the same idea?"

| Aspect | Transfers? | Why |
|---|---|---|
| Sensor cal automation (gyro bias, accel offsets, BNO055 cal at boot) | **Yes** | Same control-theory problem in both domains |
| Cascade architecture (rate inside angle loop) | **Yes** | Already partially in our raw-gyro-D-term work |
| Anti-windup + integral clamping | **Yes** | Standard practice |
| Operator UX (status command, EEPROM cal persistence) | **Yes** | Direct steal |
| Hardcoded community-tuned defaults | **Partially** | Drones have decades of community tuning; balance bots don't. The seed gains we've picked are best-effort but not battle-tested across chassis |
| Hand-tune by observing flight | **NO** | Drones survive mediocre gains; balance bots fall instantly |
| Trim-hover bootstrap for auto-tune | **NO** | Balance bot has no stable bootstrap regime |
| Multi-axis decoupled control / mixer | **NO** | Different actuator topology |
| The "no auto-tune needed" assumption | **NO** | Phase 4.10 RLS is the right structural answer for the balance-bot problem class |

## What landed in this round

**New documents (all under `auto_orientation/docs/`):**

1. `archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md` — 10 gotchas + 8 rules + the explicit "Read this BEFORE iterating" callout
2. `findings/research_flight_controller_pid_lessons.md` — 2760-word synthesis
3. `findings/research_bno055_calibration_audit.md` — 2100-word code-path audit + diagnostic protocol
4. `findings/research_drone_vs_balance_bot_stability.md` — 2179-word control-theory comparison

**Indexes updated:**
- `docs/archive/INDEX.md` — prominent "READ BEFORE ITERATING" pointer to lessons doc at top
- `docs/findings/INDEX.md` — new "Cross-project synthesis" subsection with the 3 new findings

## What this means for the next session

Three concrete things to do at the next bench session, in priority order:

1. **Validate BNO055 cal health** with the 5-step diagnostic protocol (audit doc §5). 30-second test, decides whether to focus on sensor or controller.

2. **Watch Phase 4.10 RLS converge.** Power up, prop bot, send `s` every 10 s. Should see ADAPT mode within 5 s, K_motor converging within 30-60 s. If K_motor never stabilises, the freeze gates are still wrong.

3. **Don't tune gains by hand.** Per `LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md` rule #2: if you're editing `kDefaultInitialK*` for the third time, stop and look upstream.

## What's been intentionally NOT changed

- No code modifications this round — pure research / docs / synthesis.
- No commits.
- No changes to compile flags.
- Build state unchanged from end of prior session (99.9% flash, 78.4% RAM).

## See also

- [LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](../LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) — durable insights
- [2026-05-12_evening_phase4_landing.md](2026-05-12_evening_phase4_landing.md) — Phase A + Phase B code work
- [PHASE_4_STRUCTURAL_FIXES.md](../../PHASE_4_STRUCTURAL_FIXES.md) — coordinating doc for the code work
- [findings/research_flight_controller_pid_lessons.md](../../findings/research_flight_controller_pid_lessons.md)
- [findings/research_bno055_calibration_audit.md](../../findings/research_bno055_calibration_audit.md)
- [findings/research_drone_vs_balance_bot_stability.md](../../findings/research_drone_vs_balance_bot_stability.md)
