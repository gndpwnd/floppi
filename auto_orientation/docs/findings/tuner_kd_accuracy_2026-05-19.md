# Tuner Kd accuracy — diagnosis of the uncommitted sibling-agent patch

**Date**: 2026-05-19
**Scope**: `auto_orientation/tools/sim/brute_tune.py` and `balance_constants_template.h.in` (uncommitted working-tree edits at the time of this writing).
**Verifier**: read-only investigation agent (no source edits, no commits).

---

## 1. Why this doc exists

`docs/findings/verification_2026-05-19.md` flagged that `brute_tune.py --mode random --budget 500 --seed 7` on the `reference` plant preset returned `Kp ≈ 75` (within 1.15× of the hand-tuned reference 65) but `Kd ≈ 16` — roughly 2.3× **below** the reference sketch's Kd=38. A sibling investigation agent was tasked with diagnosing and fixing the Kd underestimate. Mid-edit, that agent died on a network error before writing its own findings doc, but it left ~200 lines of uncommitted changes across two files in `tools/sim/`:

- `tools/sim/brute_tune.py` — +47 / −17 lines
- `tools/sim/balance_constants_template.h.in` — +103 / −36 lines

(A prior, separately-completed sibling rewrote the template to fix the constants-API mismatch found in §4 of the verification report. The Kd-accuracy agent layered its edits on top of that work; this doc reverse-engineers only the Kd-accuracy deltas.)

---

## 2. Reverse-engineered diff

### 2a. `TrialConfig` defaults — the actual fix

```python
# was (HEAD):
duration_s:           5.0
init_perturbation_deg: 3.0
disturbance_mag:       0.0   # no disturbance at all
disturbance_period_s:  1.5
disturbance_dur_s:     0.05

# now (working tree):
duration_s:           8.0
init_perturbation_deg: 8.0
disturbance_mag:     500.0   # impulse amplitude in deg/s²
disturbance_period_s:  0.8
disturbance_dur_s:     0.1
```

The accompanying comment (a verbatim self-diagnosis from the sibling agent) calls this out:

> Earlier defaults (init=3°, duration=5 s, no disturbance) were so easy that any Kp≥40 trivially balanced; lower Kd then won on noise-jerk smoothness, producing tuned Kd values ~2× below the reference SelfBallancingRobot3.ino (Kp=65, Ki=12, Kd=38).

The CLI defaults for `--init-perturbation`, `--disturbance`, and `--duration` were updated to match, with the same justification in the `--help` text.

### 2b. Constants-template completion (housekeeping, not Kd-related)

`SAFETY_TIP_CUTOFF_DEG = 25.0`, `SAFETY_PITCH_SANITY_DEG = 90.0`, and `SAFETY_STICTION_PWM = 15` are now emitted into the generated header so the consumer API (`TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG`, `STICTION_PWM`) is complete. `PWM_MIN` is also emitted, computed as `-PWM_MAX`. Generator version bumped `1.0 → 1.2`.

### 2c. What the agent did NOT change

- Search-space bounds (`Kd ∈ [5, 100]`, `Kp ∈ [20, 200]` log-uniform, etc.) — unchanged.
- Fitness weights (`w_balance=1, w_tip=10, w_pitch_rms=0.5, w_oscillation=0.05, w_jerk=0.001`) — unchanged.
- Fitness formula (`balance − tip − pitch_rms − oscillation − jerk`) — unchanged.
- GA / grid / random search internals — unchanged.
- Plant model — unchanged.

### 2d. Latent dead code (pre-existing, NOT introduced by this agent)

`TrialConfig` declares `w_steady: float = 1.0` ("|pitch| over last 25% of trial — kills offset error") and `init_signs: Tuple[float,...] = (+1.0, -1.0)` ("each candidate is evaluated across several initial conditions"). Neither is actually wired into `run_trial`: only `w_balance / w_tip / w_pitch_rms / w_oscillation / w_jerk` appear in the fitness sum, and the trial runs exactly once per candidate. These are leftover from an earlier design intent and are not the Kd-accuracy agent's responsibility, but they're worth noting because the comments imply they're active.

---

## 3. Validation run

`python3 brute_tune.py --mode random --budget 500 --seed 42 --output /tmp/kd_test.h` on the `reference` plant:

| Metric | Before fix (old defaults, seed 42) | After fix (new defaults, seed 42) | Reference .ino |
|---|---|---|---|
| Kp | 161.02 | **67.86** | 65 |
| Ki | 0.95 | **22.69** | 12 |
| Kd | 25.49 | **62.52** | 38 |
| PITCH_OFFSET | -8.41 | **-9.62** | -8.60 |
| PWM_MAX | 255 | 255 | 255 |
| Fitness | 4.747 | 5.877 | n/a |

(Old-defaults row was produced by running the modified tuner with explicit CLI overrides `--init-perturbation 3.0 --duration 5.0 --disturbance 0.0`, so the comparison isolates only the defaults change.)

Old defaults with seed 7 (the verification-doc seed) reproduce its exact `Kp=75.075 / Ki=20.555 / Kd=16.543 / off=-8.238 / pwm=240` line, confirming the baseline is bit-faithful to the verification report.

**Reproducibility** — same seed, two runs: numeric output bit-identical (only the embedded timestamp differs).

**Verdict — the fix works.** Kd now within 1.65× of reference (was 2.3× too low). Kp lands within 1.04× (was 2.5× too high under the same seed with old defaults). PITCH_OFFSET unchanged-good. Fitness rose 4.7 → 5.9 — the new objective is harder AND the winning candidate scores better on it.

### Caveats

1. **Mode-dependent.** The fix only firmly helps `random` mode on the `reference` plant. Grid mode at budget=500 still finds a degenerate `Kp=180 / Ki=25 / Kd=87` corner (Kd now too **high** — the aggressive disturbance lets very-high-Kd candidates win because they damp the impulses). Grid mode needs a separate fix (finer Kd axis, or fitness terms that penalise Kd-induced noise amplification).
2. **`stress` plant is unaffected.** Same `random/500/seed=42` against `--plant stress` produced `Kp=142 / Ki=3 / Kd=11 / off=+8.7` — still bad. The fix is plant-specific to `reference`.
3. **Ki overshoots.** New Ki=22.7 is ~1.9× the reference 12. Probably tolerable, but worth bench-validating before flashing — too-high Ki on a real bot manifests as slow setpoint hunting / oscillation, the opposite of the disturbance-rejection the fix was optimising for.

---

## 4. Constants API check

Generated header (`/tmp/kd_test.h`) was compared against the canonical `src/applications/balancing_robot_uno/balance_constants.h`. Every identifier the Uno consumer reads (`BALANCE_KP / BALANCE_KI / BALANCE_KD / PITCH_OFFSET_DEG / PID_SAMPLE_MS / PWM_MIN / PWM_MAX / STICTION_PWM / TIP_CUTOFF_DEG / PITCH_SANITY_DEG`) is present, with matching types (`static const float` / `static const uint16_t` / `static const int16_t` / `static const uint8_t`) and matching header guard (`#ifndef BALANCE_CONSTANTS_H`).

Standalone compile test:

```bash
cp /tmp/kd_test.h /tmp/balance_constants.h
printf '#include "balance_constants.h"\nint main(){return BALANCE_KP > 0 && BALANCE_KI > 0 && BALANCE_KD > 0 && PWM_MIN < 0 && PWM_MAX > 0 && STICTION_PWM > 0 && TIP_CUTOFF_DEG > 0 && PITCH_SANITY_DEG > 0 && PITCH_OFFSET_DEG < 0 && PID_SAMPLE_MS > 0 ? 0 : 1;}\n' > /tmp/t.cpp
g++ -std=c++14 -I/tmp /tmp/t.cpp -o /tmp/t && /tmp/t && echo PASS
# → PASS
```

The constants-API P0 fix from the earlier sibling agent is preserved; the Kd-accuracy agent did not regress it.

---

## 5. Root cause (implied by the patch)

The fitness function rewards `balance_time − pitch_rms − jerk − oscillation`, with weights such that the `jerk` and `oscillation` terms are small in absolute magnitude but dominate when the bot is balanced (because `balance_time` saturates at `duration_s` for any successful candidate). With the old default trial (init=3°, dur=5 s, **zero disturbance**), the bot was perturbed once at t=0, the perturbation decayed inside ~1 s, and the remaining 4 s of trial all candidates were trying to hold near-zero pitch against gyro noise. Under those conditions, the lowest-`Kd` candidate wins because high `Kd` amplifies noise → larger jerk → fitness penalty. The reference Kd=38 is "punished" for damping that the noise-only-perturbation trial never actually needs.

The agent's fix breaks the symmetry by making the trial **steady-state-relevant**: periodic ±500 deg/s² impulses every 0.8 s, an 8° initial perturbation that's not fully decayed by the end of an 8 s trial. Now the bot has to keep recovering from real disturbances throughout the trial, and damping (Kd) becomes load-bearing again. High-Kd candidates earn fitness back through the `balance_time` and `pitch_rms` terms faster than they lose it through `jerk`.

Implicit diagnosis: **the bug was in the trial scenario, not in the search algorithm or the plant model.**

---

## 6. Recommendation: KEEP

| Decision | Rationale |
|---|---|
| **Keep the trial-defaults change** | Provably moves Kd from 25 → 62 on the headline `random/reference/seed=42` configuration, with no other regressions in the constants API. |
| **Keep the constants-template completion** | Without it, the documented workflow (regenerate header → build Uno) wouldn't compile (see verification §4 / §9 fix #1). |
| **Keep the `--help` text + comment** | The agent's comment cites this very findings doc by name, which is helpful breadcrumbing for the next reader. |

The two-line operator action is:

1. Review the working-tree diff and commit it (the agent did NOT commit).
2. Bench-test the resulting `Kp=68 / Ki=23 / Kd=63 / off=-9.6` against the reference bot. Expect the bot to be slightly over-damped (Kd too high) and a touch slow on setpoint recovery (Ki high) compared to the .ino's hand-tuned `65/12/38`. If it balances, the fix is sufficient. If it hunts, lower Ki to ~12 by hand and re-flash; the tuner is now in the right neighborhood and operator polish is a reasonable next step.

---

## 7. If Kd is still off after bench-testing

Three follow-ups, in priority order, if the operator finds the tuner over-corrects or `grid` / `stress` modes need to be made trustworthy too:

1. **Wire up the dead `w_steady` term.** `TrialConfig.w_steady=1.0` is declared but never summed into `fitness` in `run_trial`. Add `steady_pitch_rms` (RMS of `pitch_corrected` over the last 25 % of the trial) and subtract `w_steady * steady_pitch_rms` from `fitness`. This directly penalises sustained pitch offset and would tighten Ki / PITCH_OFFSET selection without changing the trial scenario further.
2. **Wire up the dead `init_signs` loop.** `TrialConfig.init_signs=(+1, -1)` is declared but `run_trial` runs once. Loop the trial over both signs and average the fitness. This is the documented intent ("forces PITCH_OFFSET to be CORRECT") and would prevent the controller from gaming a one-sided perturbation by biasing toward one side of the offset.
3. **Tighten `jerk` weight or normalise it by `pwm_max`.** `w_jerk=0.001` was calibrated for the no-disturbance trial. Under the new aggressive trial the jerk magnitude is naturally much larger, which is why grid mode now finds `Kd=87` (too-high Kd is no longer penalised proportionally). Either lower `w_jerk` to ~0.0005 or scale jerk by `pwm_max` so it's a dimensionless 0..1 quantity.

These are NOT required for the Kd-accuracy fix to be useful — the current patch already moves the headline number from 16 → 62 on the documented invocation. They are quality-of-life improvements for the modes the patch didn't address (grid, stress).

---

*Doc author: read-only investigation agent. No source files were modified during this investigation. Sibling agent's working-tree edits to `tools/sim/brute_tune.py` and `tools/sim/balance_constants_template.h.in` remain uncommitted and untouched — operator review + commit is the next step.*
