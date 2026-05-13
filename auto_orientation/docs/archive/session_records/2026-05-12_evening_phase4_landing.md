# Session Record — 2026-05-12 Late Evening — Phase 4 Structural Fixes + Universal Auto-Tune

Continuation of `2026-05-12_evening_balance_iteration.md`. The bench session that started this morning ended in frustration with hand-tuning. This session pivoted to research-driven structural work — five research agents, two coding agents, all delivered.

## What this session was

The user's framing mid-session became the turning point:

> "you are basically just making your own PID auto tuning algorithm but it wont work when put on a completely different robot make sense? shouldn't you research Automated PID tuning with inverted pendulums and stuff make sense?"

That stopped the hand-tuning loop and triggered a switch to first-principles research.

## Research agents

Five parallel agents, all returned with convergent findings:

| Agent | Doc | Key takeaway |
|---|---|---|
| Inverted-pendulum control methods | `findings/research_inverted_pendulum_control_methods.md` | Self-Tuning Regulator (STR, Åström-Wittenmark 1973) is the AVR-feasible answer. Dismissed LQR (needs plant model), SMC (chattering), RL (no AVR), GA/PSO (offline only). |
| Open-source bot survey | `findings/research_open_source_balance_bots.md` | **Zero of 8 surveyed projects auto-tune on hardware.** Best reference: TKJElectronics Balanduino. Common gaps across all projects: no on-robot auto-tune, no fall/pickup recovery, stiction compensation often removed. |
| Universal zero-knowledge tuning | `findings/research_universal_zero_knowledge_tuning.md` | Within a declared bench-scale class (0.1-1 kg, 0.1-1 m, brushed DC), zero-knowledge universal IS achievable: scalar RLS + closed-form PD-from-K_motor + σ-modification projection + rate-limited update. ~300 LOC, ~100 B RAM, ~2 KB flash, 2 days work. |
| Bootstrap protocol | `findings/bootstrap_protocol_unstable_plant.md` | 6-stage sequenced protocol. Highest-risk discovery: `OnlineMountingEstimator` has been receiving placeholder zeros for its freeze-gate inputs the entire session — the disturbance freeze was broken. |
| Osoyoo reference review | `findings/research_osoyoo_reference_implementation.md` | Single biggest difference: Osoyoo uses **raw gyro** as D-term (physical measurement), not numerical derivative of fused pitch (laggy). They run PID in a hardware-timer ISR (fixed 5 ms dt), not polled. FALLEN is soft-cutoff, not sticky. |
| Multi-orientation feasibility | `findings/research_multi_orientation_balance_feasibility.md` | Three levels. Level 2 (firmware-only arbitrary mounting orientation) achievable in ~1 week. Level 1 (rollover) hardware-blocked. Level 3 (ballbot) deferred — AVR borderline. |

## Coding agents

**Phase A** — structural fixes (4 items, single coding agent, ~4 hours of agent work):
1. `PIDController::compute_with_rate()` — raw gyro D-term overload.
2. Real signals into `OnlineMountingEstimator.update()` — placeholder zeros gone.
3. MsTimer2 hardware-timer ISR at 200 Hz (TimerOne avoided because it owns Uno PWM pin 10 = L298N ENB). Split `BalanceApp::step()` into `read_sensors()` (loop-side I²C) and `tick()` (ISR-safe).
4. Soft-cutoff at ±25° pitch — motors silent, PID still ticks (I-term + estimator coherent), auto-recovers when righted.

**Phase B** — universal auto-tune (item 5, separate coding agent, ~21 minutes wall-clock):
- `PlantIdentifier` class with scalar RLS estimating `α_pitch = K_motor·pwm_total + g_eff·pitch_rad` (linearised — soft-cutoff keeps regression in linear region).
- Closed-form gain mapping: `Kp = ω_n²/K_motor`, `Kd = 2ζω_n/K_motor` (ω_n = 4/ts, ts = 0.5 s, ζ = 0.7).
- σ-modification soft projection (0.02..5.0 deg/s²/PWM), MIN_PHI=10 excitation gate, λ=0.998 forgetting factor.
- Integrated into BalanceApp with 5%/s rate-limited gain ramp, freeze gates (bootstrap window, lateral gyro, windup_active).
- 7 native unit tests, all pass. Convergence test: K_est=0.400 vs K_true=0.400 (0.0% error).
- `s` serial command extended with ADAPT/BOOT tag + K_motor + target gains.

## Build state

```
pio run -e arduino_uno_balancing
Flash: 99.9% (32216 / 32256 bytes)   28 B headroom
RAM:   78.4% (1605 / 2048 bytes)
```

Verbose log strings trimmed from boot/cal/status paths to fit. Functional code paid for by string compression.

## What's left

| Item | Status |
|---|---|
| Hardware validation | NOT done — user didn't have bot plugged in. |
| Full bootstrap stage machine (5-state) | Designed; using single 5 s timer instead. Defer until hw drives the need. |
| Motor-polarity sanity check at adapt-start | Designed; flagged as a follow-up. |
| Level 2 multi-orientation (Phase 4.11) | Designed; not coded. Next phase after hw validation of 4.10. |
| EEPROM K_motor warm-start | Possible enhancement; gains-NOT-persisted policy says no. |
| Native test suite repair | Pre-existing failures (unrelated); deferred per user "test on hardware not CI". |

## Notable design decisions captured in user feedback

1. **MsTimer2, not TimerOne.** Timer1 owns Uno PWM 9 & 10; L298N uses pin 10 for ENB. Documented at `platformio.ini` build_flags comment and in `main.cpp` near the MsTimer2 include.
2. **Linearised gravity term in RLS** (sin(x) → x). Soft-cutoff at ±25° keeps regression in linear region; <0.5% error vs full sin within the active range; saves ~250 B avoiding `sinf` import.
3. **PID gains NOT persisted** to EEPROM — per user preference. K_motor estimate is recomputed every boot from the seed plus first ~30 s of operation.
4. **Default build flags** (`platformio.ini`): `USE_BNO055 USE_BALANCING_ROBOT USE_TUNER_RELAY USE_BALANCE_HELD_DETECTION`. Fall detection off, HELD on.
5. **Quaternion-first architecture preserved** — for forward-compatibility with future Level 2 multi-orientation work. `OrientationData` continues to ship full quaternion; Euler is a derived view.

## See also

- [PHASE_4_STRUCTURAL_FIXES.md](../../PHASE_4_STRUCTURAL_FIXES.md) — coordinating doc with Item-by-item summary.
- [AUTO_TUNING_REALITY_CHECK.md](../../AUTO_TUNING_REALITY_CHECK.md) — captures the "is auto-tune even possible" question and answer.
- [MULTI_ORIENTATION_BALANCE_VISION.md](../../MULTI_ORIENTATION_BALANCE_VISION.md) — the next vision after 4.10 ships.
- Memory: `project_phase4_landed_2026-05-12.md`.

## What to tell the user next session

When they plug the bot back in:

1. The current build has fundamentally different control architecture from this morning's. Don't expect the same gains/behaviour.
2. First 5 seconds after RUN entry is bootstrap (identifier observing, seed gains active).
3. Then ADAPT mode — `s` shows learned K_motor and target gains. Watch how gains evolve over the first minute.
4. If the bot doesn't balance after K_motor has converged (P drops below ~0.01), the problem is hardware. Don't tune Kp by hand.
