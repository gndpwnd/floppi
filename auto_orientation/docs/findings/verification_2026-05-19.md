# Verification — Wave 4 / commit c3c0c6b ("save progress")

**Date**: 2026-05-19
**Scope**: Post-`c3c0c6b` snapshot of `auto_orientation/`.
**Verifier**: read-only verification agent (no source / no commits touched).
**Sibling agents in-flight**: collision detection, wheel encoder driver, `docs/MEGA_UNIVERSAL_PLAN.md`. Anything those agents own is treated as "in flight" rather than "missing".

---

## 1. Build matrix

| Env | Status | Flash % | RAM % | Free B | Notes |
|---|---|---|---|---|---|
| `arduino_uno_minimal` | **PASS** | 49.7 % (16028 / 32256) | 34.7 % (710 / 2048) | 16228 flash / 1338 RAM | New Phase-4U minimal balancer. Under target (<50 %). |
| `uno_balance` | **FAIL** | — | — | — | Linker: `multiple definition of 'setup'` / `'loop'`. `src/main.cpp` AND `src/applications/balancing_robot_uno/main.cpp` are both pulled in by the `balance_src_filter` ( `+<*>` ). |
| `mega_balance` | **FAIL** | — | — | — | Same duplicate-`setup`/`loop` linker failure as `uno_balance`. |
| `mega_orientation` | **FAIL** | — | — | — | Compile-time: `#include <MsTimer2.h>` not found while building `src/applications/balancing_robot_uno/main.cpp` — the orientation env lacks the `MsTimer2` lib dep AND should not be compiling the balancer app at all. |
| `native_test` (pio test runner) | **ERRORED** | — | — | — | Compile failures in legacy tests `scenario_test_ekf.cpp`, `integration_test_math_pipeline.cpp`, `benchmark_math.cpp` (stale APIs: `getVelocity` vs `get_velocity`, multi-arg `initialize`/`predict`/`update` no longer in `ExtendedKalmanFilter`). These are pre-existing legacy-test rot; `pio test` aborts before running the balance/uno tests. |

**One-line summary**: 1/4 board envs pass (`arduino_uno_minimal`); the new `applications/balancing_robot_uno/main.cpp` collides with the legacy `src/main.cpp` setup/loop in every env that uses the universal `balance_src_filter` (`+<*>`), and also gets dragged into `mega_orientation` where its MsTimer2 dependency is missing.

Root cause is one shared bug in `platformio.ini`: every env except `arduino_uno_minimal` and `native_test` uses `+<*>` and pulls in the new Uno main. Either the new sub-app dir needs to be excluded from those filters (`-<applications/balancing_robot_uno/>`), or `arduino_uno_minimal` needs to be the only env that compiles it.

---

## 2. Native test results

The Unity-/pio-managed runner errored before running anything, but the project ships **pre-built standalone test ELFs** under `tests/`. Ran each directly. Also compiled and ran `test_uno_balance_app.cpp` on the fly (per the recipe in its header comment) since no binary ships yet.

| Test (binary) | Passed | Failed | Notes |
|---|---|---|---|
| `tests/test_balance_app` | 28 | 0 | Built 2026-05-19 16:55. Clean. |
| `tests/test_balance_app_bootstrap` | 40 | 0 | Built 2026-05-19 16:54. Clean. |
| `tests/test_balance_app_collision` | 27 | 0 | Built 2026-05-19 16:42. **Collision API is present in source** (`balance_app.{h,cpp}` — `collision_latched_`, `clear_collision()`, three-gate detector), so the "currently dead" warning in the task brief is outdated. Tests pass. |
| `tests/test_balance_app_soft_cutoff` | 13 | 0 | Built 2026-05-19 16:55. Clean. |
| `tests/test_held_state_machine` | 3 | **3** | Pre-existing failure noted in memory. First failure: "RUN -> HELD on ext_motion (cmd quiet + pitch_gyro fast)" — never reaches HELD. Cascades into next two tests. Binary built 2026-05-19 16:55; investigation file claims root cause is a *stale-binary* compiled without `-DUSE_BALANCE_HELD_DETECTION`. Since the binary IS recent and still fails, either the cited investigation is wrong or the rebuild was done without the right `-D` flag. **Worth re-checking after the in-flight Wave 4 work settles.** |
| `tests/test_mounting_calibration` | 20 | 0 | Built 2026-05-18 19:22. Clean. |
| `tests/test_online_mounting_estimator` | 14 | 0 | Built 2026-05-18 19:16. Clean. |
| `tests/test_plant_identifier` | 13 | 0 | Built 2026-05-19 16:55. Clean. |
| `tests/test_uno_balance_app` (compiled live by verifier; output to `/tmp/test_uno_balance_app`) | 17 | 0 | New file `test_uno_balance_app.cpp` (2026-05-19 17:05) — no binary yet. Compiled with `g++ -std=c++11 -O2 -DUNIT_TEST ...` per the file's own header. All 7 scenarios × multiple asserts = 17 PASSes. |
| **Aggregate (excl. legacy `pio test` casualties)** | **175** | **3** | 98.3 % pass rate; only `test_held_state_machine` regresses, and it was already red. |

Legacy tests that won't compile under `pio test -e native_test` (not regressions from c3c0c6b; predate it):
- `scenario_test_ekf.cpp` — `ExtendedKalmanFilter` API renamed (`getVelocity` → `get_velocity`, etc.)
- `integration_test_math_pipeline.cpp` — same
- `benchmark_math.cpp` — same

These don't affect the balance stack but they DO mean `pio test` is unusable end-to-end — a developer running `pio test -e native_test` will see a red bar immediately and the balance tests never run. Worth either fixing the legacy tests, deleting them, or carving them out of the `native_test` env's test-filter.

---

## 3. Python tuner CLI verification

```
$ python3 brute_tune.py --help
usage: brute_tune.py [-h] [--mode {grid,random,evolutionary}]
                     [--budget BUDGET] [--output OUTPUT]
                     [--plant {reference,stress,uno_small}]
                     [--init-perturbation INIT_PERTURBATION]
                     [--disturbance DISTURBANCE] [--seed SEED]
                     [--duration DURATION] [--no-write] [--quiet]
```

All advertised flags are accepted; help text matches the README pattern in the brute-force-tuner workflow. (One ignorable stderr warning about Axes3D / matplotlib — cosmetic, doesn't affect tuning.)

---

## 4. Grid-search output (50 trials)

`python3 brute_tune.py --mode grid --budget 50 --output /tmp/brute_test_output.h` → 0.8s, 50 trials, **`/tmp/brute_test_output.h`** written. Header excerpt:

```cpp
// AUTO-GENERATED by tools/sim/brute_tune.py — DO NOT HAND-EDIT
// Regenerate with:
//   python3 tools/sim/brute_tune.py --mode grid --budget 50 \
//                                   --output <this-file>
//
// Generated:    2026-05-19 21:25:53 EDT
// Generator:    brute_tune.py v1.0
// Search:       mode=grid  budget=50  seed=42
// Plant preset: reference
// Fitness:      4.263   (5.000s balanced, tip=no)
// Trial:        init_perturbation=3.0deg,
//               disturbance=none
//
// Reference for context (SelfBallancingRobot3.ino, manually-tuned working bot):
//   Kp=65  Ki=12  Kd=38  PITCH_OFFSET=-8.6  PWM_MAX=255
#pragma once

namespace balance {

constexpr float KP = 20.0000f;
constexpr float KI = 0.0000f;
constexpr float KD = 68.3333f;
constexpr float PITCH_OFFSET_DEG = -12.0000f;
constexpr int PWM_MAX = 150;
constexpr int PID_SAMPLE_MS = 5;
constexpr int MIN_PWM = 15;

}  // namespace balance
```

Banner has the expected fields: timestamp, generator version, search mode/budget/seed, plant preset, fitness, trial config, reference values. Uses `#pragma once`. **All five tuned constants are present and the type signatures match the template.**

### Critical finding: API mismatch between tuner output and Uno consumer

The on-MCU consumer (`src/applications/balancing_robot_uno/uno_balance_app.cpp` + `main.cpp`) reads:

```cpp
pid_.set_tunings(BALANCE_KP, BALANCE_KI, BALANCE_KD);    // bare names
pid_.set_output_limits(static_cast<float>(PWM_MIN), static_cast<float>(PWM_MAX));
last_pitch_deg_ = raw - PITCH_OFFSET_DEG;
if (fabs(pitch) > TIP_CUTOFF_DEG) { ... }
motors(motor_pins, STICTION_PWM);
```

…i.e. it expects `BALANCE_KP / BALANCE_KI / BALANCE_KD / PITCH_OFFSET_DEG / PWM_MIN / PWM_MAX / STICTION_PWM / TIP_CUTOFF_DEG / PITCH_SANITY_DEG` at file scope (the in-tree `balance_constants.h` provides exactly that as `static const float ...`).

The tuner-generated header instead produces:

```cpp
namespace balance {
constexpr float KP = ...;
constexpr float KI = ...;
constexpr float KD = ...;
constexpr float PITCH_OFFSET_DEG = ...;
constexpr int PWM_MAX = ...;
constexpr int PID_SAMPLE_MS = ...;
constexpr int MIN_PWM = ...;
}
```

**If a user actually ran the documented workflow** (`brute_tune.py --output src/applications/balancing_robot_uno/balance_constants.h` then `pio run -e arduino_uno_minimal`), the build **would not compile** — every identifier the application uses would become undeclared. There are also names the application needs that the tuner does not emit at all (`BALANCE_KI`, `BALANCE_KD`, `PWM_MIN`, `STICTION_PWM`, `TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG`). This is the **single most concerning finding** — see §9.

Diff in concrete form (in-tree vs tuner output):

| In-tree (`balance_constants.h`) | Tuner template (`balance_constants_template.h.in`) |
|---|---|
| `BALANCE_KP`, `BALANCE_KI`, `BALANCE_KD` (file scope) | `balance::KP`, `balance::KI`, `balance::KD` |
| `PITCH_OFFSET_DEG` (file scope) | `balance::PITCH_OFFSET_DEG` |
| `PWM_MIN`, `PWM_MAX` (file scope, int16_t) | `balance::PWM_MAX` only (int) — no PWM_MIN |
| `STICTION_PWM` (uint8_t) | `balance::MIN_PWM` (int) — different name |
| `TIP_CUTOFF_DEG` (file scope, float) | *not emitted* |
| `PITCH_SANITY_DEG` (file scope, float) | *not emitted* |
| `PID_SAMPLE_MS` (uint16_t) | `balance::PID_SAMPLE_MS` (int) — different namespace |

---

## 5. Reproducibility (same seed → same output)

```
$ python3 brute_tune.py --mode random --budget 100 --seed 42 --output /tmp/repro1.h
$ python3 brute_tune.py --mode random --budget 100 --seed 42 --output /tmp/repro2.h
$ diff /tmp/repro1.h /tmp/repro2.h
6c6
< // Generated:    2026-05-19 21:25:57 EDT
---
> // Generated:    2026-05-19 21:25:59 EDT
```

**PASS** — only the embedded timestamp differs (expected). All numeric constants and metadata are bit-identical. Both runs report `Kp=153.167  Ki=20.319  Kd=63.963  off=-8.291  pwm=175`.

---

## 6. Reference reproduction (does tuner find ~Kp=65?)

The reference sketch (`SelfBallancingRobot3.ino`) uses **Kp=65, Ki=12, Kd=38, PITCH_OFFSET=-8.6, PWM_MAX=255**.

Ran two 500-trial searches on the `reference` plant preset:

| Mode | Budget | Seed | Kp | Ki | Kd | PITCH_OFFSET | PWM_MAX | Fitness |
|---|---|---|---|---|---|---|---|---|
| random | 500 | 7 | **75.075** | **20.555** | 16.543 | -8.238 | 240 | 4.712 |
| grid | 500 | 42 | 20.436 | 0.000 | 68.708 | -11.508 | 141 | 4.311 |

**Verdict — mixed.**

- The **random** run found `Kp≈75`, which is within 1.15× of the reference 65 (excellent). `Ki≈20` is ~1.7× the reference 12 (acceptable). `PITCH_OFFSET≈-8.24` is essentially spot-on to the reference -8.6. **Kd≈16 is well off the reference 38** (2.3× too low). This is consistent with the simulator settling on a high-Kp / low-Kd compromise — the reference bot has a slower mechanical Kd / damping than the simulator probably assumes.
- The **grid** run found a degenerate corner: `Ki=0`, very low `Kp=20`, very high `Kd=68`. This balances the toy simulator but is unlikely to balance the real bot (no integral term → won't reject sustained tilt, very high derivative → noise amplification). Grid resolution at budget=500 is probably too coarse to find the reference neighborhood, and the fitness function doesn't penalize Ki=0 strongly enough.

**Summary**: random search reaches the reference neighborhood for Kp + offset; Kd is consistently underestimated; grid mode is currently too coarse. Tuner is in the right ballpark but **not a substitute for bench tuning** — operator should treat its output as a starting point and expect to adjust Kd upward.

---

## 7. Documentation completeness

| Path | Status | Notes |
|---|---|---|
| `docs/MEGA_UNIVERSAL_PLAN.md` | **MISSING** | Sibling agent in-flight per task brief — expected. |
| `docs/applications/balancing_robot_uno/README.md` | PRESENT (147 lines) | Clear "why / what / what-not", links to vision + scope + roadmap. Good. |
| `docs/applications/balancing_robot_uno/INDEX.md` | PRESENT (41 lines) | Good — has "Available", "Planned" (with triggers + owners), "Related" sections. Task brief expected this to be missing. |
| `docs/applications/INDEX.md` | PRESENT (41 lines) | Catalog. |
| `docs/findings/INDEX.md` | PRESENT (131 lines) | Lists `audit_documentation_2026-05-19.md`, `audit_code_quality_balance_stack_2026-05-19.md`, `application_catalog.md`, `investigation_held_state_machine_failure_2026-05-19.md`, `research_collision_signature_bno055.md`, `research_motor_null_space_handling_detection.md`, `research_imu_only_position_containment.md`. New files **are** indexed. |
| `docs/guides/INDEX.md` | PRESENT (20 lines) | Lists `safe_bench_test_workflow.md` — good. |
| `tools/sim/README.md` | **MISSING** | The `docs/applications/balancing_robot_uno/INDEX.md` Related-docs section explicitly links to it. Either the link is aspirational or the file was meant to land in this wave. |

---

## 8. Uno minimal program — code review

Spot-checked `src/applications/balancing_robot_uno/{uno_balance_app.h, uno_balance_app.cpp, main.cpp, balance_constants.h}`.

| Check | Result | Detail |
|---|---|---|
| Constants come from `balance_constants.h`, not hardcoded | **PASS** | `uno_balance_app.cpp:26-29` uses `BALANCE_KP/KI/KD/PWM_MIN/PWM_MAX`; tip cutoff + sanity gate use `TIP_CUTOFF_DEG`/`PITCH_SANITY_DEG`. `main.cpp` prints them at boot. |
| PID structure mirrors SelfBallancingRobot3.ino intent | **PASS** | Setpoint = 0, error = corrected pitch, output clamped to ±PWM_MAX, sample time 5 ms, integrator reset on tip. Same shape as the reference sketch. |
| Safety cutoff at ±25° | **PASS** | `uno_balance_app.cpp:82` (`if (fabs(pitch) > TIP_CUTOFF_DEG)`). Sets `tipped_`, stops motors, calls `pid_.reset()` to prevent integrator windup, returns 0. Constant defined in `balance_constants.h:79` as 25.0f. |
| Serial 'a' emergency stop handler | **PASS** | `main.cpp:78-83`. Calls `app.abort()` (latches `armed_=false`); also `'s'` handler prints status. Other input ignored. |
| MsTimer2-driven balance ISR at 200 Hz | **PASS** | `main.cpp:124-125` (`MsTimer2::set(PID_SAMPLE_MS, pid_isr); MsTimer2::start();`) — `PID_SAMPLE_MS=5` so 200 Hz. ISR is `pid_isr()` which calls `app.step()`. `read_imu()` runs in `loop()` (not ISR) to avoid blocking I²C inside the timer — good. |
| Volatile pitch double-buffering | **PASS** | `uno_balance_app.h:98-106` marks `last_pitch_deg_`, `pitch_valid_`, `last_pwm_`, `armed_`, `tipped_` as `volatile`. Header comment documents the tear-acceptance trade-off explicitly. |
| Disarm/no-data behaviour | **PASS** | `step()` returns 0 + stops motors when `!armed_` OR `!pitch_valid_`. |
| Direction-swap done at driver level | **PASS** | `main.cpp:48-51` pinout matches reference .ino (no driver-level swap in the Uno-minimal version — note `src/main.cpp:88-91` does swap for the universal stack). Reasonable: Uno-minimal stays faithful to the reference. |

**Code reads cleanly. No bugs spotted in the Uno minimal source itself.** The blockers are external (build env collisions, constants-API mismatch with tuner).

---

## 9. Overall PASS / FAIL summary + top fixes

### Summary

| Area | Verdict |
|---|---|
| Uno minimal build (`arduino_uno_minimal`) | **PASS** — 49.7 % flash, 34.7 % RAM, builds clean. |
| Universal balance builds (`uno_balance`, `mega_balance`) | **FAIL** — `setup`/`loop` symbol collision with the new Uno sub-app. |
| `mega_orientation` build | **FAIL** — pulls in Uno sub-app, missing MsTimer2 dep. |
| Native balance / uno test suite (run binaries directly) | **PASS** — 175 / 178 (98.3 %). `test_held_state_machine` 3/6 fails — pre-existing. |
| `pio test -e native_test` | **FAIL** — legacy tests don't compile (EKF/math). Pre-existing, but it means the integrated runner is unusable. |
| Python brute-force tuner CLI / reproducibility | **PASS** — all flags work, same-seed runs are bit-identical (ignoring timestamp). |
| Python tuner reference reproduction | **PARTIAL** — random search reaches Kp / offset neighborhood; Kd consistently underestimated; grid mode degenerate at this budget. Useful seed values; not a substitute for bench tuning. |
| Uno minimal source quality | **PASS** — all 7 spot-checks green. |
| Documentation completeness | **MOSTLY PASS** — new application doc folder + indices + research/audit docs all present and indexed. `docs/MEGA_UNIVERSAL_PLAN.md` and `tools/sim/README.md` missing (former is in-flight; latter is unowned). |

**Overall: CONDITIONAL PASS** — the Uno minimal target works end-to-end at the source level, and the test suite is healthy. But the universal envs are all red, and the tuner → consumer contract is broken (mismatched names + namespaces).

### Top 3 fixes (in priority order)

1. **Tuner-output / Uno-consumer API contract mismatch** (most concerning). `balance_constants_template.h.in` emits `namespace balance { constexpr float KP ...; }` with names like `KP / KI / KD / MIN_PWM`; the Uno application (`uno_balance_app.cpp`, `main.cpp`) reads `BALANCE_KP / BALANCE_KI / BALANCE_KD / PWM_MIN / PWM_MAX / STICTION_PWM / TIP_CUTOFF_DEG / PITCH_SANITY_DEG` at file scope. Running the documented workflow (`brute_tune.py --output src/applications/balancing_robot_uno/balance_constants.h && pio run -e arduino_uno_minimal`) **will fail to compile**. Pick one side and align: either (a) extend the template to emit the file-scope `BALANCE_KP/etc.` names plus the missing constants (`PWM_MIN`, `STICTION_PWM`, `TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG`) with defaults preserved across regenerations, or (b) rewrite the Uno app to consume `balance::KP / KI / KD / PITCH_OFFSET_DEG / PWM_MAX / MIN_PWM` and hardcode the safety constants (`TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG`) in a separate non-generated header.

2. **`balance_src_filter` pulls in the new Uno sub-app's `main.cpp`** (causes 3/4 board envs to fail to link with duplicate `setup`/`loop`). Add `-<applications/balancing_robot_uno/>` to `balance_src_filter`'s exclusion list, OR scope the new directory the same way `arduino_uno_minimal` does (positive filter only). Fixes `uno_balance`, `mega_balance`, and indirectly `mega_orientation` (which currently fails for the same reason + a missing MsTimer2 dep).

3. **`pio test -e native_test` runner is unusable** due to legacy `scenario_test_ekf.cpp` / `integration_test_math_pipeline.cpp` / `benchmark_math.cpp` using long-renamed `ExtendedKalmanFilter` APIs (`getVelocity` / multi-arg `initialize`). Either repair them (mechanical rename to `get_velocity` etc. and refit the multi-arg calls to the current 3-arg `predict` / 2-arg `update` / 1-arg `initialize`), delete them, or carve them out of the native_test `test_filter`. Until this is fixed, any developer running `pio test` sees a red bar and the balance-stack tests never even compile via the integrated path — only the pre-built ELFs save the day.

---

*Verifier: read-only check executed 2026-05-19. No source files modified, no git operations performed. One new file written: this report. Sibling-agent in-flight work (`MEGA_UNIVERSAL_PLAN.md`, collision detection, wheel encoders) NOT inspected; if those landings change the build matrix or test results, re-run §1 + §2.*
