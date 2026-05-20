# Brute-tune simplification — design

**Date**: 2026-05-19
**Author**: architect agent (design-only, no implementation, no commits)
**Scope**: replace `tools/sim/brute_tune.py` (single-file, ~880 LOC, 8 CLI flags) with a smaller, modular, deterministic tuner targeted at bench operators who want to type `python3 tune.py` and get balance gains.
**Status**: DESIGN ONLY. Reviewer + operator sign-off required before any code is written.

---

## 1. Operator pain points (from feedback)

Verbatim feedback (truncated):

> "please try to make it intuitive… the whole point of this program is tuning P, I, and D so it really shouldn't be that complicated in terms of controls. we want it to be really easy to use for humans and just let the program do all the work and calculations make sense? so why use different seeds why can't we just have algorithmic approaches?"

> "if you need to make the brute tune tool modular into multiple files that is fine"

Distilled:

1. **Too many knobs.** Eight flags (`--mode --budget --plant --seed --init-perturbation --disturbance --duration --output`) when the operator only wants three answers (Kp, Ki, Kd).
2. **RNG seeds expose implementation, not intent.** The operator should never have to think about reproducibility — the algorithm should be deterministic by construction.
3. **Single-file growth is becoming opaque.** ~880 LOC mixes CLI parsing, fitness, three search strategies, header writing, and presets. The 2026-05-19 PM `w_steady`/`init_signs` polish proved it's hard to extend without re-reading the whole file.
4. **Defaults must be right.** The default invocation must yield Uno-deployable gains for the reference bot — no flag combinations required.

The operator does NOT want a re-design of the fitness function (that work landed today and verified: Kd accuracy 25→62, see `tuner_kd_accuracy_2026-05-19.md`). They want a thinner skin around the same math.

---

## 2. Proposed CLI surface

Target: at most three flags, and zero flags must work.

```bash
# Default — tune for the bench reference bot, write the Uno header.
python3 tune.py

# Tune for a different physical chassis preset.
python3 tune.py --plant uno_small

# Spend longer searching for better gains (use when the bot wobbles after a default-run tune).
python3 tune.py --thorough

# Combine: thorough tune for the small chassis.
python3 tune.py --plant uno_small --thorough

# Override the output path (rare; CI / experiments).
python3 tune.py --output /tmp/probe.h
```

Full flag list (4 total):

| Flag | Type | Default | Purpose |
|---|---|---|---|
| `--plant` | choice (`reference`, `uno_small`, `stress`) | `reference` | physical-chassis preset; matches today's `PLANT_PRESETS` keys |
| `--thorough` | bool flag | off | switches search budget from FAST (~1 min) to THOROUGH (~5 min); see §3 |
| `--output` | path | `src/applications/balancing_robot_uno/balance_constants.h` (relative to `auto_orientation/`) | header destination |
| `--dry-run` | bool flag | off | print best gains but do NOT write the header (used by tests) |

Removed flags (with rationale):

| Removed | Why |
|---|---|
| `--mode` | algorithm choice is internal to the tool; operator picks results, not algorithms |
| `--seed` | new algorithm is deterministic; reproducibility is automatic |
| `--budget` | `--thorough` is the only knob; raw numbers are an implementation detail |
| `--init-perturbation` | trial scenario is fixed; the 2026-05-19 PM values are correct and frozen |
| `--disturbance` | same as above |
| `--duration` | same as above |
| `--quiet` | progress is concise by default; no flag needed |
| `--no-write` | replaced by `--dry-run` for clearer intent |

Old `brute_tune.py` keeps its CLI verbatim and stays in the repo as the "advanced" path (see §7).

---

## 3. Recommended search algorithm

**Primary recommendation: 5-D Sobol low-discrepancy sequence + local pattern-search refinement.**

Sobol gives the random-style space coverage operators expect from "brute force" without an RNG seed; pattern search (deterministic hill-climb) tightens the winner. Both are deterministic by construction: same code in, same gains out.

### Algorithm comparison

| Algorithm | Determinism | Coverage quality | Complexity | LOC est. | Recommendation |
|---|---|---|---|---|---|
| **Sobol + pattern search** | yes (no seed) | excellent (low-discrepancy) | moderate (scipy.stats.qmc.Sobol exists; hand-code is ~60 LOC) | ~200 | **PRIMARY** |
| Grid + iterative refinement | yes | mediocre (axis-aligned bias) | low | ~150 | fallback if Sobol unavailable |
| Halton sequence + pattern search | yes | good (slightly worse than Sobol) | low | ~150 | second-choice |
| Multi-start Nelder-Mead | yes (deterministic starts) | excellent on convex basins, weak globally | moderate (scipy.optimize) | ~200 | rejected — fitness is rough, NM gets stuck |
| Bayesian optimisation (GP) | yes | excellent | high (~600 LOC or scikit-optimize dep) | n/a | rejected — overkill, breaks numpy-only constraint |
| Pure grid (no refinement) | yes | bad in 5-D (4⁵=1024 misses Kd basins) | low | ~80 | rejected — produced the Kd=87 degenerate winner today |

### Why Sobol + pattern search wins

- **No seed.** Sobol is purely indexed (`sobol[i]` is a function of `i`, not RNG state). The whole point of low-discrepancy sequences is reproducible space-filling.
- **Better than random.** For the same budget, Sobol's star-discrepancy is asymptotically better than uniform random — operator gets random-style coverage with a stronger convergence guarantee.
- **scipy is already in the toolchain.** `balance_bot_sim.py` already imports numpy + matplotlib; `scipy.stats.qmc.Sobol` is a 1-line addition. The README's "stdlib + numpy only" hard constraint should be revisited — scipy is a single transitive dep on a developer machine and the alternative (~60 LOC hand-rolled Sobol) is well-trodden (Joe-Kuo direction numbers, public domain).
- **Pattern search closes the gap.** Sobol explores; pattern search (Hooke-Jeeves-style: ±step on each axis, accept improvement, halve step when stuck) refines without RNG. Deterministic.
- **Matches today's two-phase structure.** The existing `search_grid` already does "coarse → refine"; we keep the bones, swap coarse-grid for Sobol, swap random-refinement for pattern-search.

### Budget table

| Mode | Sobol points | Pattern-search starts | Steps per start | Total trials | Wall-clock (today's evaluate_candidate ≈ 80 ms × 2 init_signs ≈ 160 ms) |
|---|---|---|---|---|---|
| FAST (default) | 256 | top 4 | ~30 | ~376 | ≈ 60 s |
| THOROUGH (`--thorough`) | 1024 | top 8 | ~60 | ~1500 | ≈ 4 min |

These budgets are sized so FAST < today's `--budget 500` default wall-clock and THOROUGH ≈ today's `--budget 1500`. If bench validation shows FAST is too aggressive, bump to 512 Sobol points (still <2 min).

### Determinism by construction

- **Sobol**: `sobol = Sobol(d=5, scramble=False)`; `sobol.random(N)` returns the same matrix every time.
- **Pattern search**: pure deterministic descent — axes scanned in fixed order, step halved on stagnation, terminated at step-tolerance or iteration cap.
- **`evaluate_candidate`**: passes a FIXED seed (e.g. 0) into `np.random.default_rng(0)` for IMU noise + plant disturbances. This is an implementation seed (frozen), not an operator knob.

Two consecutive `python3 tune.py` runs produce bit-identical headers (modulo embedded timestamp). The whole motivation for ditching `--seed` is to make this property obvious.

---

## 4. File-split structure

Target: package `tools/sim/tune/`, ~6 files, ~750 LOC total (down from ~880 in one file). Drop-in next to `brute_tune.py` so the old path keeps working.

| File | Responsibilities | Line budget | Key exports |
|---|---|---|---|
| `tune/__init__.py` | mark as package; expose `tune()` for tests | ~20 | `from .orchestrator import tune` |
| `tune.py` (entry point at `tools/sim/tune.py`) | CLI parser, orchestration, console output formatting | ~150 | `main()`, `_build_parser()`, `_print_progress()` |
| `tune/presets.py` | `PLANT_PRESETS` table + `BUDGET_PRESETS` (FAST/THOROUGH); the only file an operator who is adding a new chassis touches | ~80 | `PLANT_PRESETS`, `BUDGET_PRESETS`, `Candidate`, `clamp_candidate()` |
| `tune/fitness.py` | `TrialConfig`, `TrialScore`, `run_trial()`, `evaluate_candidate()` — port of today's lines 168–403 unchanged | ~250 | `run_trial`, `evaluate_candidate`, `TrialScore`, `TrialConfig`, `SAFETY_*` constants |
| `tune/search.py` | Sobol generator + pattern-search loop + top-K selection | ~180 | `search(eval_fn, budget) -> (best_cand, best_score, log)` |
| `tune/output.py` | `write_header()` + template loader; insulates the package from the `.h.in` format | ~70 | `write_header`, `DEFAULT_OUTPUT`, `TEMPLATE_NAME`, `GENERATOR_VERSION` |
| `tune/orchestrator.py` | wires presets → fitness → search → output; the only file that knows about all four | ~120 | `tune(plant, thorough, output_path) -> (Candidate, TrialScore)` |

Total: ~870 LOC across 7 files (similar size, but distributed). The two largest files (`fitness.py`, `search.py`) are independently understandable and individually under 300 LOC.

### Why this split

- **`presets.py` is the only file an operator who is adding a new chassis touches.** Today they have to read 880 lines to find the right tuple.
- **`fitness.py` is the only file an algorithms person touches when fitness needs polish.** Today's `w_steady` wiring + `init_signs` change touched 7 different sections of one file.
- **`search.py` is the only file the search-algorithm research touches.** Today switching from random to Sobol would require edits across 5 functions in one file.
- **`orchestrator.py` is small enough to read top-to-bottom in 30 seconds** and explains the whole flow.
- **`tune.py` (entry) is the only file the CLI surface lives in.** Help-text changes don't touch any algorithmic code.

### Inter-file dependency graph

```text
tune.py  →  orchestrator.py  →  presets.py
                            →  fitness.py  →  balance_bot_sim.py (existing)
                            →  search.py   →  fitness.py
                            →  output.py
```

No circular imports. `presets.py` and `output.py` are leaves.

---

## 5. Fitness function — what stays, what's hidden

**Keep verbatim (this is today's polish, do not regress):**

- `TrialConfig` defaults (`duration_s=8`, `init_perturbation_deg=8`, `disturbance_mag=500`, `init_signs=(+1,-1)`, all weights including `w_steady=0.5` and `w_jerk=0.003`).
- `run_trial()` body (lines 238–370).
- `evaluate_candidate()` body (lines 373–403) — runs each candidate across both init signs, takes worst-case fitness.
- `TIP_THRESHOLD_DEG=25`, `SAFETY_*` constants.
- Mounting-offset semantics (IMU reports plant pitch + mounting offset; candidate subtracts its `pitch_offset_deg` guess).

**Hide from operator (was CLI, becomes internal):**

- `init_perturbation` (was `--init-perturbation`) — frozen at 8° in `TrialConfig`.
- `disturbance` (was `--disturbance`) — frozen at 500 deg/s² impulses.
- `duration` (was `--duration`) — frozen at 8 s.
- `seed` (was `--seed`) — frozen at 0 in `evaluate_candidate`; deterministic.
- Search-internal hyperparameters (Sobol N, pattern-search step, top-K) — controlled by `--thorough` flag only.

**Remove entirely:**

- All RNG seed plumbing through `search_random`, `search_evolutionary`, etc. — replaced by Sobol's deterministic index.
- `--mode` choice infrastructure — only one search algorithm exposed.

**Net effect:** the same fitness math the operator validated today produces the same Kp/Kd/Ki, but the operator no longer touches it.

---

## 6. Determinism notes

### Sobol determinism

`scipy.stats.qmc.Sobol(d=5, scramble=False)` is deterministic by definition — Sobol points are a property of the dimension and index, not random state. Two invocations of `Sobol(d=5, scramble=False).random(256)` return identical 256×5 matrices. No seed parameter exists; the only randomness option (`scramble=True`) is explicitly off.

Fallback if scipy is unwanted: hand-roll Sobol with Joe-Kuo direction numbers (~60 LOC, public-domain reference implementation linked from Wikipedia). The 5-D sequence is precomputable as a static table.

### Pattern-search determinism

Hooke-Jeeves variant: for each axis i in fixed order [Kp, Ki, Kd, pitch_offset, pwm_max], try (+step_i, −step_i); if either improves, accept and continue from there. When neither improves on any axis, halve all step sizes. Terminate when all step sizes fall below tolerance or trial budget exhausted. No RNG anywhere. Step sizes start at 30% of axis range, terminate at 1%.

### Fitness determinism

`evaluate_candidate` calls `np.random.default_rng(0)` inside `run_trial`. The fixed seed lives inside `TrialConfig.rng_seed=0` — operator never sees it. Same candidate, same plant, same trial config → bit-identical fitness.

### End-to-end determinism check

The first task in §9 is a test that asserts two consecutive `tune()` invocations produce bit-identical `(Candidate, TrialScore)`. If anyone introduces a hidden RNG path later, this test catches it.

---

## 7. Migration plan

### Phase 1 — write new modular package alongside the existing tool

- Create `tools/sim/tune/` package with the 6 files from §4.
- Create `tools/sim/tune.py` entry point.
- Add `tools/sim/tests/test_determinism.py` — asserts two consecutive `tune("reference", thorough=False)` invocations produce identical gains.
- Add `tools/sim/tests/test_reference_quality.py` — asserts default `tune()` against `reference` plant produces Kp ∈ [50, 80], Ki ∈ [5, 25], Kd ∈ [30, 70] (the bench-validated reference window).
- `brute_tune.py` untouched.
- Existing `balance_constants.h` consumer untouched.
- Operator runs `python3 tools/sim/tune.py` and compares output to `brute_tune.py --mode random --budget 500`. Both produce a usable Uno header.

### Phase 2 — deprecate `brute_tune.py` to "advanced" status

- Add a deprecation banner to `brute_tune.py`'s `--help` output: "this is the advanced tuner; most operators want `tune.py` instead."
- Update `tools/sim/README.md` so the headline "How to run" section recommends `tune.py`; demote `brute_tune.py` to an "Advanced: full knobs" subsection that lists the old flags.
- Update `docs/applications/balancing_robot_uno/README.md` §"Brute-force tuning workflow" steps 1–6 to use `tune.py`.

### Phase 3 — bench-validate + retire

- Operator runs `python3 tune.py` on the bench bot. Gains land within the validated window from Phase 1 test 2.
- Flash + bench-test for at least one balance session lasting ≥ 30 s.
- If bench-validation passes, decide whether to delete `brute_tune.py` or keep it as an algorithm-research sandbox. Recommendation: **keep** — it has three search strategies that are useful for future research even if operators don't touch them.
- If the operator decides to delete `brute_tune.py`, move its evolutionary-search code into `tune/search.py` as a `--search ga` option (would require breaking the §2 promise of 4 flags; defer to operator preference).

---

## 8. Risks + open questions

### Risks

1. **Sobol convergence may be slower than random + GA on the headline case.** Today's `--mode random --budget 500` finds Kp≈68 / Kd≈62 on the reference plant; a 256-point Sobol may land further from this if the basin is narrow. Mitigation: Phase 1 test 2 catches this before commit; if it fails, bump default Sobol budget to 512.
2. **Pattern search can stall in flat regions of the fitness landscape.** Mitigation: termination criterion is `min(step) < tol OR iter > budget`, so it can't run forever; the Sobol seed it starts from is already in a good basin (top-K of 256).
3. **scipy dependency.** Today's `tools/sim/README.md` line 18 says "stdlib + numpy only. No DEAP, no scipy.optimize." Adding scipy.stats.qmc breaks this contract. Mitigation: either (a) get operator sign-off to relax the constraint (scipy is already on every dev machine that runs matplotlib), or (b) hand-roll Sobol from Joe-Kuo tables (~60 LOC, no new dep).
4. **Old `brute_tune.py` flag combinations CI might break.** Mitigation: Phase 1 does not touch `brute_tune.py`; Phase 2 only adds a deprecation banner, doesn't change CLI behaviour. Existing CI scripts (if any) keep working.
5. **`uno_small` and `stress` plant presets still produce bad gains today** (per `tuner_kd_accuracy_2026-05-19.md` §3 caveats 1 + 2). The new tuner inherits this. Mitigation: deferred — the operator's near-term goal is `reference` plant ≈ bench bot; non-reference presets are stress-test fodder.

### Open questions for the operator

1. **Is scipy acceptable?** Direct answer determines whether `search.py` is ~180 LOC (scipy) or ~240 LOC (hand-roll Sobol).
2. **Is `--thorough` the right name?** Alternatives: `--slow`, `--careful`, `--polish`. The verb should signal "spend more time", not "be more aggressive".
3. **Should default output still be the in-tree header path, or `tools/sim/output/balance_constants.h`?** Today's default writes into `src/...` which couples the tuner run to a working-tree edit. Operator preference: keep coupling (faster bench iteration) or break it (cleaner separation).
4. **Wall-clock budget for FAST.** Today's `--budget 500` ≈ 80 s. The §3 estimate for the new FAST mode is ~60 s. Is that acceptable, or does the operator want it under 30 s (would require dropping to 128 Sobol points)?

---

## 9. Concrete next-coding-session task list

Each task is sized to <2 hours of focused work. Tasks 1–4 can be done in one session; 5–8 in a second session.

1. **Stand up the package skeleton.** Create `tools/sim/tune/__init__.py`, `presets.py`, `fitness.py`, `search.py`, `output.py`, `orchestrator.py` as empty stubs with docstrings + signatures matching §4. Add `tools/sim/tune.py` entry point with `--help` working but `main()` raising `NotImplementedError`. (≈ 30 min)

2. **Port `fitness.py` from `brute_tune.py` lines 71–403.** Move `SAFETY_*` constants, `PLANT_PRESETS` (to `presets.py`), `Candidate`, `TrialScore`, `TrialConfig`, `_disturbance_fn`, `run_trial`, `evaluate_candidate` verbatim. No semantic changes. Add a unit test (`tests/test_fitness_parity.py`) asserting `run_trial` produces bit-identical output to `brute_tune.run_trial` on a fixed candidate. (≈ 90 min)

3. **Port `output.py` from `brute_tune.py` lines 632–671.** Move `write_header`, `GENERATOR_VERSION`, `DEFAULT_OUTPUT`, `TEMPLATE_NAME`. Update template-call signature: drop `mode`, `seed`, `budget` arguments; pass `search_algorithm="sobol+pattern"` and `thorough` flag instead. Update `balance_constants_template.h.in` placeholders (separate template edit — list in task 6). (≈ 45 min)

4. **Implement `search.py` Sobol generator.** If scipy permitted: 20-line wrapper around `scipy.stats.qmc.Sobol`. If not: hand-roll Joe-Kuo Sobol (~60 LOC; reference impl at https://web.maths.unsw.edu.au/~fkuo/sobol/). Add unit test asserting `sobol_5d(256)` returns identical matrix on two calls. (≈ 75 min)

5. **Implement `search.py` pattern-search refinement.** Hooke-Jeeves variant from §6. Add unit test on a 5-D quadratic bowl asserting convergence to within 1% of true minimum in <100 iterations. (≈ 90 min)

6. **Update `balance_constants_template.h.in`.** Replace `mode={mode} budget={budget} seed={seed}` with `search=sobol+pattern thorough={thorough}`. Bump `generator_version` to `2.0`. Verify the consumer compile-test from `tuner_kd_accuracy_2026-05-19.md` §4 still passes. (≈ 30 min)

7. **Wire `orchestrator.py` + `tune.py` CLI.** Match §2 flag surface exactly. Console output: progress every 10% with "Looking for best gains… 30%… Best so far: Kp=65 Ki=12 Kd=38"; final summary like today's lines 814–833 but without the seed/mode line. (≈ 60 min)

8. **Land determinism + reference-quality tests.** `tests/test_determinism.py` runs `tune("reference", thorough=False)` twice and asserts bit-equal `Candidate`. `tests/test_reference_quality.py` asserts the §7 Phase 1 windows (Kp ∈ [50, 80], Ki ∈ [5, 25], Kd ∈ [30, 70]). Both must pass before Phase 2 of the migration plan. (≈ 60 min)

After task 8 lands, the operator can run `python3 tune.py` on the bench bot; the migration moves to Phase 2 (docs update, deprecation banner). Phase 3 (bench validation) requires hardware time and falls outside the coding-session scope.

---

*Doc author: architect agent. Scope: design only. Files in `tools/sim/` and `src/applications/` were read-only inputs; no source files were modified. The single file written by this agent is the present design doc.*
