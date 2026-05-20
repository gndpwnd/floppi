# 2026-05-18 PM — Bench validation + violations audit

Continuation of [2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md](2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md). That AM session landed Phase 2.1 / 2.5 / 2.6 with the bot only verified in IDLE. This session put motors on, ran release-and-balance tests, found the real failure modes, made one architectural fix to the mount estimator, and ended with a strict policy audit listing every remaining hardcoded value with its replacement plan.

---

## Bench sequence and findings

Six rounds of testing, each producing a concrete data finding. Recorded in order so the reasoning chain is preserved.

### Round A — Sensor alive check
5 s of pitch polling with the bot at rest. Pitch read 1.13° within 10 mdeg over the window. Initially looked suspicious (frozen?) but the next test resolved it as normal BNO055 NDOF quantization on a stationary bot.

### Round B — Tilt test
Operator tilted the bot ±20°. Pitch tracked from -2.71° to +3.33° (6° range observed). **BNO055 confirmed alive.** The static reading from Round A was real — the bot was simply still and the sensor's quantization step is ~0.5° at rest.

### Round C — CHARACTERISE attempt (failed — wrong state)
Sent `k` while bot was auto-RUN'ing. CHARACTERISE only triggers from IDLE, so `k` was ignored. Bot continued in RUN, hit HELD twice, transitioned RUN↔HELD. **Accidentally informative:** confirmed the legacy lateral-gyro HELD trigger was firing on recovery transients (not real handling), exactly as the AM session record predicted.

### Round D — Full balance observation (first real release test)
Pre-iteration firmware. **75.9% RUN, longest contiguous 10.8 s, 9 transitions.** Inside the 10.8 s window the bot actually balanced — pitch +0.6° → +0.4° with output +12 → +2 PWM for ~1.3 s. First time the gain scheduling could be seen working in real data. But the HELD transitions were spurious: every one occurred while motor command was large (-88 to -255 PWM), meaning Phase 2.5 `ext_motion` (cmd quiet) was NOT firing. **The legacy lateral-gyro 90 dps threshold was misclassifying recovery transients as handling.**

### Iteration 1 — three universal-system fixes
Based on Round D findings:

1. **Removed the lateral-gyro HELD trigger entirely.** Kept Phase 2.5 ext_motion (catches falls / handle-without-motion) and accel-deviation (catches lifts). Lateral-gyro will return when Phase 2.7 motor-null-space projection lands — that one is mathematically incapable of false-firing on a pitch recovery because the recovery is in the motor-controllable subspace.
2. **Tightened Phase 2.6 soft zone from 2° → 1°.** At pitch 1° the output went from 50% authority to 100% authority. The wider zone was scaling recovery commands too far down to catch real falls.
3. **Speed up OnlineMountingEstimator from 20 s → 8 s time constant.** Round D showed the estimator stayed frozen at 0.87° for the entire 25 s test. A 20 s tc on the LPF couldn't track within typical run lengths.

Flashed 95.7% flash (1384 B free).

### Round 2 — verified iteration 1
**1 transition vs 9. HELD oscillation eliminated.** Mount estimator climbed 0.87° → 1.04° during 8.7 s of RUN (vs 0.16° before). Both core findings from Round D were resolved. New observation: STUCK detector correctly fired at t=8.73 s after 4 s of saturated output with no rotation.

### Round 3 — STUCK cause confirmed
Operator clarified: **motors were not powered during Round 2.** PWM was being commanded, no motion was occurring because the L298N V+ rail was off. STUCK detector did exactly the right thing. This is the third time this session that "motors off without firmware knowing" caused confusion — re-raising the urgency of [operator backlog #15](../../findings/operator_ideas_backlog.md) (A0 voltage divider on L298N V+).

### Round 5/6 — motors confirmed on
**81.2% RUN, longest 8.3 s, 8 transitions.** Bot oscillated -1.6° to +4.9° pitch with output swings -255 to +255. **Mount estimator stayed flat at 0.87°.** Identified the architectural bug in the estimator.

### Architectural fix — mount estimator targets pitch directly
`src/navigation/online_mounting_estimator.cpp:177` — the LPF target was:

```cpp
target_deg = reference_deg + i_term * 0.01;
```

With `i_term` capped at 40 PWM, the maximum possible target shift was ±0.4° from reference. AND when the bot oscillates around balance, `i_term` itself oscillates between positive and negative, so the LPF averages to no movement. **The estimator was structurally incapable of learning the true balance point under realistic balance conditions.**

Replaced with:

```cpp
target_deg = pitch_deg;  // The mean pitch during balancing IS the balance point
```

Existing freeze gates (`windup_active`, `gyro > 60 dps`) prevent garbage data from polluting the estimate during recoveries.

### Round 7 — mount-estimator fix tested, caught a NEW failure
On reflash, the saved mount offset was `-1.21°` from an earlier session, while the bot's true upright pitch is `+0.3°`. PID computed corrected_pitch ≈ +1.5°, drove backward hard, kill switch fired at -22°. **The bot can't recover from a stale EEPROM-saved mount.** This is a separate scope-relevant issue: the auto-RUN at boot trusts whatever was saved, but the saved value can be far wrong, leading to immediate failure with no recovery path.

### Mount recapture
Operator held bot vertical, sent `c`, captured pitch +0.64° as new mount offset (replacing the stale -1.21°). Did not proceed to another balance test — discussion shifted to the architectural realization below.

---

## The architectural realization

After Round 7's near-instant kill-switch failure, the operator paused testing and asked the question that had been latent all session:

> "what needs to happen to make the bot not fall over? everything should be automated or handled dynamically..."

The honest answer: **the PID gains are hardcoded.** `Kp = 50`, `Ki = 1`, `Kd = 10` in `main.cpp:323`. These were inherited from a reference .ino on a different chassis. With wrong gains, the bot oscillates so violently it falls before any automation can adapt.

Worse, the RLS plant identifier (Phase 4.10) — the supposed universal auto-tune — freezes during high-gyro events (every recovery). The bot oscillates → high gyro → RLS frozen → no learning → gains stay wrong → bot oscillates. **The automation cannot bootstrap itself from a falling bot.**

This same structural gap exists for every hardcoded value: HELD thresholds, soft zone width, STUCK timeout, mount estimator clamp, etc. Each is justified in code as a "stopgap until the proper measurement lands," but the proper measurement requires the bot to balance, and the bot can't balance because the values are wrong.

The unblocking move is **Phase 4.10c BOOTSTRAP** — a state between mount capture and RUN that applies controlled PWM pulses, measures K_motor analytically from gyro acceleration response, derives PID gains from K_motor via pole-placement (Kp = ω²/K_motor, Kd = 2ζω/K_motor with universal ω=10, ζ=0.7), and enters RUN already tuned to this specific bot. Reference design: [bootstrap_protocol_unstable_plant.md](../../findings/bootstrap_protocol_unstable_plant.md).

---

## Strict-policy update

The operator's framing crystallised:

> "all parameters that need tuning should never be hardcoded.... they should be found through the calibration or earlier stages or otherwise dynamically handled... was this not clear enough in the scope file? make sure all relevant documentation reflects this... this is precisely what I want to avoid - manual or hardcoded tuning..."

The scope.md already had the policy ("the ONLY values that may be hardcoded are MCU pin assignments") but the firmware was full of stopgaps that contradicted it. **Updated scope.md with an explicit violations audit table** listing every hardcoded value still in the codebase along with its concrete replacement plan. 19 violations catalogued. Each one names its source file/line, why it must go (specific to the bot/chassis/battery), and how it will be derived.

The table puts the policy under live enforcement: any PR adding a numeric constant outside pin assignments must either (a) be a structural value that's not a tuning, or (b) include a row in the audit explaining its replacement plan. Otherwise the PR is rejected.

Updates landed in this session:

- [`scope.md`](../../scope.md) — added the violations audit and the "control philosophy: more / less, not absolutes" section. The latter codifies the operator's reframe that the controller reasons in deltas, not absolute units.
- [`UNIVERSAL_BALANCE_BOT_VISION.md`](../../UNIVERSAL_BALANCE_BOT_VISION.md) — added the same "more / less" framing at the top of the doc with concrete code-level translation tables.
- [`roadmap.md`](../../roadmap.md) — added "Top priority (2026-05-18 PM)" section calling out BOOTSTRAP as the unblocking work.
- [`todo.md`](../../todo.md) — same prioritisation; concrete pointer to bootstrap research doc.

---

## Build state at session end

```
RAM:   [=======   ]  70.4% (used 1441 bytes from 2048 bytes)
Flash: [==========]  95.6% (used 30842 bytes from 32256 bytes)
```

1414 B headroom — comfortably enough for BOOTSTRAP (estimated 400-500 B).

Source diffs vs AM session:
- `balance_app.cpp:411` — HELD trigger refactored: dropped lateral-gyro, kept ext_motion + accel-dev
- `balance_app.cpp:477` — Phase 2.6 SOFT_ZONE_DEG: 2.0f → 1.0f
- `main.cpp:343` — OnlineMountingEstimator LPF tc: 20 s → 8 s
- `online_mounting_estimator.cpp:177` — target_deg: I-term-based → pitch_deg direct
- `docs/scope.md` — violations audit + control philosophy
- `docs/UNIVERSAL_BALANCE_BOT_VISION.md` — control philosophy
- `docs/roadmap.md` + `docs/todo.md` — BOOTSTRAP prioritised

EEPROM state on bot: stiction = 30 PWM, mount = 0.68° (recaptured this session, replaces stale -1.21°).

---

## What did NOT happen this session (and why)

- **BOOTSTRAP state**: deferred to next session per operator's "stop and think" directive. The policy audit had to land first so BOOTSTRAP is implemented under strict enforcement (no hardcoded gains creeping in via its own implementation).
- **Bench-validated balance**: the bot fell over every test. Even with the estimator fix, gains were wrong → oscillation. Will land after BOOTSTRAP.
- **Motor-power-state sensor** (backlog #15): three rounds of confusion this session came from motors-off-without-firmware-knowing. Operator deferred the wiring; nothing actionable in firmware yet.
- **Removing the relay-feedback tuner** (1.3 KB flash savings, backlog #7): RLS already obsoletes it, but we don't need the savings yet. After BOOTSTRAP lands.

---

## Lessons baked in

1. **Operator framing is always more precise than the implementation.** Each session, the operator articulates a constraint ("don't hardcode tuning," "more/less not absolute," "balance forever no FALLEN") that the code then has to scramble to honour. Treating these framings as the spec — not as feedback on a finished feature — produces faster convergence than the alternative.
2. **A working stopgap is more dangerous than a missing feature.** `stiction_min_pwm = 80` worked well enough that we shipped two more sessions on it before treating it as the scope violation it was. Each session of "well enough" is a session not spent on the universal replacement.
3. **The cheap fix and the right fix are often the same code-cost.** BOOTSTRAP (~500 B) was deferred for two sessions in favour of "just lower the gains a bit" tweaks (15 B each). Today's audit makes clear that no number of small tweaks adds up to a universal solution.
4. **Bench tests illuminate failure modes, not victories.** Every round this session that "worked" generated less learning than the rounds that failed in surprising ways. The mount-stuck-at-0.87 finding (Round D) and the kill-switch-from-stale-mount finding (Round 7) are worth more than a successful balance would have been.

---

## See also

- AM session: [2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md](2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md)
- Reference design for BOOTSTRAP: [findings/bootstrap_protocol_unstable_plant.md](../../findings/bootstrap_protocol_unstable_plant.md)
- Scope violations audit: [scope.md §Current scope violations](../../scope.md#current-scope-violations--audit-2026-05-18)
- Operator backlog (with newly-marked rows): [findings/operator_ideas_backlog.md](../../findings/operator_ideas_backlog.md)
