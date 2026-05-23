# Auto-Calibration & Auto-Tuning Verification — "Is it working?"

**Date**: 2026-05-22
**Author**: reviewer agent (`reviewer@auto_orientation:verify`), read-only over `src/`, single new findings file
**Question from owner**: *"auto calibration and auto tuning and other things are working?"*
**Method**: cross-read research/design docs vs. the live implementation; checked the math by hand; ran the native test suite and two firmware builds read-only. **No source changed. No commits.**

---

## TL;DR verdict

The auto-calibration and auto-tuning subsystems are **real, mathematically sound, and test-covered at the unit level. They compile clean and are wired end-to-end.** What they are **NOT** is *bench-validated as a stable closed loop* — the last recorded hardware run (2026-05-18 PM late) had the bot twitch and fall in ~1 s, and there has been no successful balance run on record since. So:

- **(a) compiles** — YES, both `mega_balance` and `arduino_uno_minimal` build clean.
- **(b) logic/math sound & test-covered** — YES, with caveats (see correctness gaps + design divergences below). 18/18 host-buildable test binaries pass.
- **(c) bench-validated (balances for real)** — **NO / UNKNOWN.** Hardware-gated. The control-loop tuning has never been confirmed to hold the bot upright.

**The honest one-liner for the owner**: *"The auto-cal/auto-tune code is built, the math checks out, and every unit test passes — but the robot has not yet been shown to actually balance on the bench, so 'working' is true at the code level and unproven at the physical level."*

---

## Test suite results (exact counts)

`bash tools/build_tests.sh`:

```
total:  21
passed: 18
failed: 3
```

The **3 failures are pre-existing and unrelated** to auto-cal/auto-tune. Each is a header-style test file with a trailing `#endif` and zero `#if`/`#ifdef`/`#ifndef` directives (confirmed by grep: 1 `#endif`, 0 openers in each):

| Failing file | Error |
|---|---|
| `tests/test_bno085_extensions.cpp:489` | `#endif without #if` |
| `tests/integration_test_math_pipeline.cpp:389` | `#endif without #if` |
| `tests/benchmark_math.cpp:405` | `#endif without #if` |

These are math-pipeline / BNO085 tests; they do not exercise the control or calibration code. **Not fixed (per task scope), confirmed as reported.**

The auto-cal/auto-tune-relevant binaries that **PASS**: `test_plant_identifier` (~56 assertions), `test_online_mounting_estimator` (~68), `test_position_loop`, `test_position_gain_derivation` (~7), `test_mounting_calibration`, `test_balance_app`, `test_balance_app_bootstrap` (~10), `test_balance_app_encoder`, `test_balance_app_pwm_discovery`, `test_noise_floor_estimator` (~10), `test_held_state_machine`, `test_balance_telemetry`, `test_balance_app_soft_cutoff`, `test_balance_app_collision`, `test_uno_balance_app`, `test_l298n_motor`.

Note: Unity-framework tests (`test_pid_controller`, `test_relay_feedback`, `test_wheel_encoder`) are **skipped** by the host script (no `unity.h` on host include path) — they require `pio test -e native_test`. Their coverage of the PID core and relay tuner is therefore **not exercised by this run**; treat PID-core unit coverage as unconfirmed in this session.

## Build results (read-only)

| Env | Status | Flash | RAM |
|---|---|---|---|
| `mega_balance` | **SUCCESS** | 15.8% (40180 / 253952 B) | 18.6% (1527 / 8192 B) |
| `arduino_uno_minimal` | **SUCCESS** | 51.9% (16748 / 32256 B) | 35.0% (716 / 2048 B) |

Both clean, comfortable headroom. (Mega RAM 18.6% — the 2026-05-19 overflow is well and truly fixed.)

---

## Per-subsystem verdict table

| Subsystem | (a) Compiles | (b) Math/logic sound & tested | (c) Bench-validated | Notes |
|---|---|---|---|---|
| **One-shot mounting capture** (`mounting_calibration`) | YES | YES (test passes; shortest-arc-from-gravity matches design D5) | partial — capture worked on bench historically | Solid. |
| **Online mounting estimator** (`online_mounting_estimator`) | YES | YES (68 assertions) — **but tracks mean-pitch, NOT the I-term LPF the design specifies** | NO | See Divergence #1. Algorithm is sound *as built*, just not what the doc says. |
| **BOOTSTRAP K_motor measurement** (`step_bootstrap_`) | YES | YES (units check out; pulse Δω/τ/pwm form correct) | **NO — last run twitched & fell** | Direct-pulse measurement, not the doc's RLS-from-natural-disturbance. See Divergence #2. |
| **Pole-placement gain derivation** (`PlantIdentifier::recompute_targets_`) | YES | YES — Kp=ωₙ²/K, Kd=2ζωₙ/K is textbook-correct | NO | **Diverges from doc's AMIGO Stage-4 method.** See Divergence #3. |
| **Scalar RLS plant-ID** (`PlantIdentifier::update`) | YES | YES — scalar-RLS recursion matches Åström/Ljung exactly; σ-modification present | NO | Best-verified piece. 56 assertions. |
| **Continuous gain adaptation** (`run_plant_id_` + `ramp_gain_`) | YES | YES — 5%/s ramp matches design | NO | Wired RUN→RLS→ramp→`set_tunings`. |
| **Position outer loop** (`position_loop` + `derive_position_gains_`) | YES | YES (cascade pole-placement) — **documented intentional deviation from spec's g_lean formula** | NO | See Divergence #4 (self-documented in code). |
| **Noise-floor estimator** (`noise_floor_estimator.h`) | YES | YES (Welford, 10 assertions) — **pure observation, nothing consumes it yet** | NO | Correctly built as the "measure don't hardcode" feedstock; by design nothing downstream reads it. |
| **PID core** (`pid_controller`) | YES | host test skipped this session (Unity) | NO | Anti-windup + D-on-measurement + i-term clamp all present and read clean. |
| **Relay-feedback tuner** (`relay_feedback`) | YES | host test skipped (Unity) | NO | Optional/legacy path; no longer wired to long-press. |

---

## Math cross-check (research vs. implementation)

I verified the core equations line-by-line. **The math is correct.** Specific checks:

1. **RLS recursion** (`plant_identifier.cpp:214-238`) — `L = Pφ/(λ+φPφ)`, `θ += L(y−φθ)`, `P = (P−LφP)/λ`. This is the scalar specialization of matrix RLS, identical to `dynamic_pwm_accel_learning.md §4a` and Åström & Wittenmark Ch. 2. **Correct**, including the `denom ≥ λ > 0` divide-safety argument.

2. **Gravity-strip linearization** (`plant_identifier.cpp:199`) — `y = α − g_eff·pitch_deg·DEG_TO_RAD`. The design eq is `α = K·pwm + g_eff·sin(pitch)`. Code uses `sin(pitch_rad) ≈ pitch_rad`. **Units are consistent**: g_eff [deg/s²] × dimensionless ≈ deg/s², matching α [deg/s²]. Small-angle error <0.5% inside the ±10° linear envelope, as documented. **Correct.**

3. **BOOTSTRAP K formula** (`balance_app.cpp:1374-1378`) — `K = |Δω| / (pulse_sec · pwm_total)` = (deg/s ÷ s) ÷ PWM = deg/s²/PWM. Matches the RLS θ units exactly, so `seed_k_motor()` is dimensionally coherent with the RLS that continues from it. **Correct.**

4. **Pole-placement** (`plant_identifier.cpp:262-282`) — `Kp = ωₙ²/K`, `Kd = 2ζωₙ/K`, `ωₙ = 4/ts`. Standard 2nd-order placement for `α = K·u`. **Correct** — but this is *critically-damped pole-placement*, NOT the AMIGO formulas the bootstrap doc Stage 4 calls for (Divergence #3).

5. **σ-modification leak** (`plant_identifier.cpp:232-238`) — soft projection toward `K_PRIOR` when θ exits the band, then hard-clamp backstop. Matches Ioannou & Sun §8 intent. **Correct and robust** (prevents the RLS-bursting failure mode Anderson 2005 warns about).

6. **Online mount estimator LPF** (`online_mounting_estimator.cpp:189-211`) — discrete `x += (1/tc)·dt·(target−x)`, then hard clamp to `ref±max_dev`, then rate limit. The discretization, clamp, and rate-limit are all **correct**. The *target* is the divergence (see #1).

No sign errors, no unit mismatches found in the control math.

---

## Design divergences (implementation has evolved past the docs)

These are **not necessarily bugs** — several are deliberate, well-reasoned evolutions documented in code comments. But they mean the docs no longer describe what runs, which matters for "does it check out vs the research."

**Divergence #1 — Online mount estimator tracks mean-pitch, not I-term LPF.**
`MASTER_DESIGN.md D7` and `bootstrap_protocol §2 Stage 2` specify `mount_offset += alpha·(i_term_LP·gain_to_angle)`. The implementation (`online_mounting_estimator.cpp:112-189`) explicitly **abandoned** that: a 2026-05-18 PM bench finding showed the I-term path could only shift ±0.4° and averaged to zero under oscillation. It now LPFs **mean pitch** directly (`target_deg = pitch_deg`). `i_term` and `gain_to_angle_` are now dead `(void)`-cast parameters kept for API stability. **Sound reasoning, but the design docs are stale and the dead parameters are tech-debt.** Default `tc` is also 300 s in the estimator vs. the doc's 60 s — overridden to 8 s from `main.cpp` (the triage doc #13 flags 8 s as itself questionable).

**Divergence #2 — No `BootstrapStage` enum / staged protocol.**
`bootstrap_protocol §5` specifies an explicit `BootstrapStage { SEED, MOUNT_CONVERGED, PLANT_IDENTIFIED, GAINS_REFINED, ADAPTIVE }` enum and a `try_advance_bootstrap_()` orchestrator with per-stage completion rules (the §3 detection table). **None of that exists.** Instead the implementation collapses to: a discrete `BOOTSTRAP` app-state that does **direct ±PWM pulse identification** (replacing the doc's "Stage 3 = RLS from natural disturbance"), then `RUN` runs continuous RLS + 5%/s ramp + mount estimator. This is arguably *better* (direct pulse K is cleaner than waiting for natural excitation on an unstable plant), but the documented staged protocol with its completion-rule state machine was **not built as described**. The `b`-command stage reporting, LED color encoding, and EEPROM write-back of refined K/offset described in §5 are not present in the form specified.

**Divergence #3 — Stage-4 gain mapping uses pole-placement, not AMIGO.**
`bootstrap_protocol §2 Stage 4` and `MASTER_DESIGN D9` specify AMIGO (`Kp=0.45·Ku`, `Ki=Kp/(0.85·Tu)`, `Kd=Kp·0.125·Tu`). The live path (`PlantIdentifier::recompute_targets_`) uses critically-damped pole-placement instead. AMIGO survives only in the now-unwired `relay_feedback.cpp` tuner. Both methods are individually valid; the divergence is that the **primary** auto-tune path silently uses a different family than the design committed to. The pole-placement choice is reasonable for this plant but is undocumented as the canonical method.

**Divergence #4 — Position-loop `g_lean` derivation (self-documented).**
`derive_position_gains_` (`balance_app.cpp:1557-1573`) **explicitly deviates** from `phase_4m14_design §2.2`: the spec's literal `g_lean = g_eff·(π/180)·r` yields K_POS≈2000 (~300× the fallback), so the code uses the physically-correct small-angle pendulum gain `g_lean = 9.81` instead, landing K_POS≈5.8. This is **the right call and is thoroughly documented in the code comment** — but it means the spec formula is wrong and should be corrected in the design doc.

---

## Prioritized statically-fixable correctness issues (for a coding agent)

These are safe for a no-hardware session — none change the closed-loop tuning numbers, they fix coherence/clarity/robustness. Ordered by value.

1. **[DOC/HIGH] Reconcile the design docs with the implemented protocol.** `MASTER_DESIGN D7/D9`, `bootstrap_protocol §2-§5`, and `phase_4m14_design §2.2` all describe algorithms that are no longer what runs (Divergences #1-#4). A reader trusting the research docs would mis-understand the live system. Update the docs (or add a "as-built" addendum) to describe: mean-pitch mount tracking, the collapsed BOOTSTRAP-pulse + continuous-RLS architecture, pole-placement (not AMIGO) as the primary mapping, and the corrected `g_lean=g`. Pure documentation; zero closed-loop risk.

2. **[CODE/MED] Remove or formally deprecate the dead `i_term` / `gain_to_angle_` parameters** in `OnlineMountingEstimator::update()` and its setters (`online_mounting_estimator.cpp:127, 188`). They are `(void)`-cast no-ops kept "for API stability." Either delete them (and the `set_gain_to_angle` setter) or add a `[[deprecated]]`/comment block at the public API so a future maintainer doesn't wire them back up. Behaviorally inert, so safe statically. Verify `main.cpp` / tests don't depend on the signature first.

3. **[DOC/MED] Document the `online_est` default-vs-applied time-constant mismatch.** Estimator default `DEFAULT_LPF_TC_SEC = 300.0f` (5 min) but `main.cpp:578` overrides to 8 s. The triage doc (#13) already flags 8 s as possibly wrong for a *slow-drift* tracker. At minimum, add a code comment at the `main.cpp` call site explaining why 8 s overrides the 300 s default, so the discrepancy is intentional-on-purpose, not an accident. (Changing the value itself is **bench-gated** — do not retire statically.)

4. **[CODE/LOW] Noise-floor estimator is observation-only and unconsumed.** This is *correct by design* (the triage doc deliberately built it as feedstock with nothing reading it yet). No fix needed — but flag in a TODO that the 5 doubly-blocked scope-violation constants (`STUCK_GYRO_DPS`, `ext_motion gyro>30`, HELD `a_dev_lpf_>6`, HELD→RUN gate, HELD dwell sigma) are now *unblocked on the measurement side* and become bench-validatable the next time the bot balances. This is the highest-leverage *next* step but is itself bench-gated.

5. **[CODE/LOW] `run_entered_ms_ = now_ms - 10000` back-dating hack** (`balance_app.cpp:1535`) to expire the freeze window is a magic-number trick that depends on `BOOTSTRAP_FREEZE_MS=5000 < 10000`. If anyone raises `BOOTSTRAP_FREEZE_MS` past 10 s the bypass silently breaks. Replace with an explicit `adaptive_active_ = true` + a `freeze_bypassed_` flag, or derive the back-date from `BOOTSTRAP_FREEZE_MS`. Low risk, improves robustness.

**Explicitly NOT statically fixable** (per `mega_scope_violation_triage_2026-05-22.md`, which I confirm): none of the 14 control-loop constants should be retired in a static session — every one is either bench-gated or touches a safety/transition threshold that needs hardware to confirm it didn't destabilize the loop. The triage's "build the noise-floor layer first" recommendation has **already been executed** (the layer exists), so that item is done; what remains is genuinely hardware-gated.

---

## Bottom line

The auto-calibration (mounting capture + online mount tracking) and auto-tuning (BOOTSTRAP K_motor measurement → pole-placement gains → continuous RLS adaptation → position-loop gain derivation) **are implemented, the math is correct, they build clean on both target MCUs, and 18/18 host tests pass** (the 3 failures are unrelated pre-existing `#endif` typos in math-pipeline tests). The work has clearly evolved *past* its design docs in four places — all defensible engineering decisions, but the docs are now stale and should be reconciled. The single thing that cannot be claimed is that the robot **actually balances**: the most recent bench evidence is a 1-second twitch-and-fall, and stability is the one property that only a hardware session can confirm.
