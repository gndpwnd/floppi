# Session record — 2026-05-22: Safety failsafes, correctness fixes, architecture docs, as-built reconciliation

**Date**: 2026-05-22
**Author**: doc-writer (`doc-writer@auto_orientation:session-record`)
**Scope**: `auto_orientation/` only. Multi-agent session: security audit, auto-cal/auto-tune
verification, scope-violation triage, NaN-safety failsafes, a noise-floor measurement layer,
a production quaternion bug fix, native-test repairs, and a documentation pass (ASCII→Mermaid,
new layered architecture docs, as-built vs as-designed reconciliation).
**Commit status**: **NO COMMITS this session. Everything below is in the working tree only.**

This record was written after verifying every load-bearing claim against the live source and
by re-running the native suite and both focus builds. Where a finding doc's claim no longer
matches the current tree, that is called out explicitly (see the Uno-build regression).

---

## What landed (working tree)

### 1. Documentation
- **ASCII→Mermaid conversion** across the docs corpus, including fixing **4 genuinely-invalid
  diagrams** (not just reformatting — diagrams that did not parse / were wrong).
- **New layered architecture docs** under `docs/architecture/`:
  - `LEVEL_0_SYSTEM_OVERVIEW.md`
  - `LEVEL_1_SUBSYSTEMS.md`
  - `LEVEL_2_COMPONENTS.md`
  - `INDEX.md`
- **AS-BUILT vs AS-DESIGNED reconciliation** in `docs/findings/MASTER_DESIGN.md` and the
  `docs/implementation/` docs. The implementation has evolved past the design corpus in four
  documented places (all defensible engineering calls; the docs were stale):
  1. **Mount estimator tracks mean-pitch, not the I-term LPF** the design specified
     (`online_mounting_estimator.cpp`). The I-term path could only shift ±0.4° and averaged to
     zero under oscillation; the dead `i_term` / `gain_to_angle_` params survive as `(void)`
     casts for API stability (tech-debt).
  2. **No `BootstrapStage` enum / staged protocol.** The doc's `{SEED, MOUNT_CONVERGED,
     PLANT_IDENTIFIED, GAINS_REFINED, ADAPTIVE}` state machine was never built; the live design
     collapses to a discrete `BOOTSTRAP` app-state doing direct ±PWM pulse identification, then
     `RUN` with continuous RLS + 5%/s gain ramp + mount estimator.
  3. **Pole-placement, not AMIGO**, is the primary auto-tune gain mapping
     (`PlantIdentifier::recompute_targets_`): `Kp=ωₙ²/K`, `Kd=2ζωₙ/K`, `ωₙ=4/ts`. AMIGO survives
     only in the now-unwired `relay_feedback.cpp`.
  4. **Position-loop uses `g_lean = 9.81` (g)**, not the spec's `g_eff·(π/180)·r` — the spec
     formula yields K_POS≈2000 (~300× too high); the small-angle pendulum gain lands K_POS≈5.8.
     Self-documented in the code comment; the design doc formula is the one that's wrong.

### 2. Code — NaN-safety failsafes (all verified present in the tree)
The unifying defect class: **NaN comparisons are always false, so every `if (x > limit)`
clamp/gate in the motor path silently no-ops on a NaN.** The fixes plug that across the motor
path:
- **NaN pitch trips the kill-switch.** `src/main.cpp:755` —
  `if (isnan(p) || isinf(p) || p > 20.0f || p < -20.0f)` now force-stops + aborts (was NaN-blind).
- **PID rejects non-finite gains.** `src/control/pid_controller.cpp:84-86` —
  `kp_/ki_/kd_ = (isnan(x)||isinf(x)||x<0) ? 0 : x` (previously only `<0` rejected, so a NaN gain
  was stored). Output clamp `pid_controller.cpp:294` now returns `0.0f` on `isnan/isinf` —
  closes the NaN→`(int16_t)out`→motor UB-cast path for *every* output, not just gains.
- **Mount-offset / EEPROM finiteness guard.** `src/main.cpp:209-223` (`load_mount_offset_`) —
  after the CRC-checked `memcpy`, `if (isnan(v)||isinf(v)||v<-90.0f||v>90.0f) return false;`
  (mounting offset was the only EEPROM-loaded float without a range guard; a CRC-valid garbage
  blob previously poisoned `corrected_pitch_` and disabled the mount-estimator clamp).
- **Pitch NaN guard at the IMU boundary.** `balance_app.cpp:1811` — `read_imu_` now holds
  last-good on `isnan/isinf` new_pitch (mirrors the Uno app's `isnan(raw)` guard).
- **Watchdog wired into `loop()`.** `src/main.cpp:767` — `if (safety.watchdog_starved(now))`
  now actually consumes the watchdog result (it was fed every tick but never read).
- **Output finiteness check** at `balance_app.cpp:583` (`isnan(out)||isinf(out)`).

### 3. Code — noise-floor measurement layer
- **New `src/applications/balancing_robot/noise_floor_estimator.h`** — Welford online mean+σ of
  a scalar signal over a fixed quiet window. **PURE OBSERVATION**: touches no motors / gains /
  thresholds / transitions; nothing downstream consumes it yet. Built deliberately as the
  "measure, don't hardcode" feedstock. This is the **single highest-leverage static move** from
  the triage doc — it is the missing measurement layer that **doubly-blocked 5 scope-violation
  rows** (`STUCK_GYRO_DPS`, `ext_motion gyro>30`, HELD `a_dev_lpf_>6`, the HELD→RUN quiet gate,
  and the HELD-dwell "sigma"). Those rows are now unblocked on the *measurement* side — they
  become bench-validatable the next time the bot balances.

### 4. Code — Uno minimal P1 cleanup
- **Sensor-health `read_fail_count_` (rdfail) telemetry** and **atomic `last_pwm_`** access in
  `src/applications/balancing_robot_uno/uno_balance_app.cpp` — `ATOMIC_BLOCK`-guarded multi-byte
  reads/writes of state shared with the ISR; `read_fail_count_` increments (capped at 255) on a
  failed IMU read.

### 5. Code — quaternion gimbal-lock PRODUCTION bug fix
- **`src/math/quaternion_conversions.cpp` `toEuler`** — normalize-before-`asin` (`qn.normalize()`)
  plus an explicit `[-1, 1]` clamp on `sinp`. The ZYX/Tait-Bryan formulas assume `|q|=1`; near
  the ±90° gimbal-lock singularity `asin`'s derivative diverges, so a ~1e-7 norm error amplified
  into a ~0.03° pitch error (broke a 0.01° tolerance at exactly 90°). Renormalizing pins the
  singular case onto `|sinp|≥1`, handled exactly by the clamp.

### 6. Tests
- **3 broken native test files fixed** (stray trailing `#endif` with no opener):
  `tests/benchmark_math.cpp`, `tests/integration_test_math_pipeline.cpp`,
  `tests/test_bno085_extensions.cpp`.
- **2 ill-posed gimbal-lock test assertions corrected** (in the math-pipeline / extension tests
  above, alongside the `#endif` repair).
- **2 new test files**: `tests/test_noise_floor_estimator.cpp`, `tests/test_pid_nan_safety.cpp`.
- `tools/build_tests.sh` extended (+16 lines) to pick up the new binaries.

---

## Verification results (re-run this session)

### Native test suite — **22/22 PASS** (`bash tools/build_tests.sh`)
```
total:  22
passed: 22
failed: 0
```
Up from the 18/22 the verification doc saw mid-session (the 3 `#endif` files were broken at
that time and have since been fixed; +2 new binaries added). Unity-framework tests
(`test_pid_controller`, `test_relay_feedback`, `test_wheel_encoder`) are still **skipped** by
the host script — they need `pio test -e native_test`, so PID-core unit coverage is unconfirmed
in the host run.

### Builds
| Env | Result this session | Notes |
|---|---|---|
| `mega_balance` | **SUCCESS** | Flash 16.4% (41522 / 253952 B). NaN guards + watchdog wiring add ~1.3 KB vs the verification doc's 40180 B. RAM headroom fine. |
| `arduino_uno_minimal` | **SUCCESS** | Flash 54.1% (17446 / 32256 B), RAM 35.0% (716 / 2048 B). Was failing to link mid-session (see below) — now FIXED. |

### ✅ Uno-build regression — DISCOVERED then FIXED this session
The auto-cal/auto-tune verification doc claimed both focus builds are clean. That was true *when
that doc was written* (read-only, before the quaternion change), then briefly **untrue** after the
gimbal-lock fix: the fix added `qn.normalize()` to `quaternion_conversions.cpp`, and
`Quaternion::normalize()` is defined in `src/math/quaternion.cpp`, which was **not linked into the
`arduino_uno_minimal` build**, so the link failed:
```
undefined reference to `Quaternion::normalize()'
```
Confirmed causal at the time: stashing only `quaternion_conversions.cpp` restored a clean
`arduino_uno_minimal` SUCCESS; un-stashing re-broke it. The `mega_balance` build was unaffected
(it already links `quaternion.cpp`).

**Resolution (this session):** a **one-line `platformio.ini` fix** — added
`+<math/quaternion.cpp>` to the `arduino_uno_minimal` `build_src_filter` (line 161, directly
after the existing `+<math/quaternion_conversions.cpp>` entry). This pulls the `Quaternion`
member definitions into the Uno link without touching any source. Both focus builds now compile
clean:
- `arduino_uno_minimal` — **SUCCESS**, 54.1% flash (17446 / 32256 B), 35.0% RAM.
- `mega_balance` — **SUCCESS**, 16.4% flash (unchanged).

Native suite re-run after the fix: **22/22 PASS**. The "builds clean on both target MCUs" claim
is now true again in the working tree.

### Math verification (from the verification doc, spot-checked against source)
Auto-cal/auto-tune math **checks out** — no sign or unit errors. Confirmed: RLS recursion
(`L=Pφ/(λ+φPφ)`, `θ+=L(y−φθ)`, `P=(P−LφP)/λ`), gravity-strip linearization (units consistent
in deg/s²), BOOTSTRAP `K=|Δω|/(pulse_sec·pwm_total)` (deg/s²/PWM, coherent with RLS θ),
pole-placement (`Kp=ωₙ²/K`, `Kd=2ζωₙ/K`), σ-modification leak, mount-estimator LPF
discretization+clamp+rate-limit. The "working" verdict means **compiles + logic-sound +
test-covered**, NOT bench-validated — the closed-loop balance is hardware-gated.

### Security/safety audit (from the audit doc, fixes verified in tree)
- 0 P0, 3 P1, 6 P2, 4 P3. No runaway-motor path reachable in normal operation.
- The three P1s were all **failsafe-completeness gaps** (NaN bypassing a downstream catch).
  All three P1 remediations are now present in the tree (kill-switch NaN guard P1-001,
  PID `set_tunings`/clamp NaN reject P1-003, mount-offset finiteness guard P1-002).
- Prior 2026-05-20 calibration-storage fixes re-confirmed present (CRC-8-CCITT, uint16 length,
  version reject, `restoreFromEEPROM` `buf_capacity` guard).
- P2-006 watchdog-consumer gap is now closed (`safety.watchdog_starved()` wired into `loop()`).

### Mega scope-violation triage
- **0 of 14** control-loop constants are unconditionally safe to retire in a static session.
  4 are *mechanically* derivable (#3 `SAT_THRESHOLD_PWM`, #5 `STUCK_TIMEOUT_MS`,
  #6 `ext_motion cmd_mag`, #13 online-est LPF tc) but each touches a safety detector /
  state-transition / large-factor latency change → HOLD. The remaining 10 are BENCH-GATED;
  5 of those were **doubly blocked** by the absent noise-floor layer.
- **Top unblocking move = build the noise-floor measurement layer** — **DONE this session**
  (`noise_floor_estimator.h`). The 5 doubly-blocked rows now wait only on a balancing bot.

---

## Status of the two focus builds
- **`mega_balance`** — compile-clean (16.4% flash), all NaN failsafes + watchdog wired in.
- **`arduino_uno_minimal`** — **compile-clean (54.1% flash, 35.0% RAM)**. The mid-session link
  regression (`Quaternion::normalize()` undefined) was FIXED by the one-line `platformio.ini`
  `+<math/quaternion.cpp>` `build_src_filter` addition (see the regression section above).
- **Native suite** — **22/22 PASS** (re-run after the Uno fix).
- **Balance loop** — still **NOT bench-validated**. Last hardware run (2026-05-18 PM late):
  IDLE→BOOTSTRAP→RUN, K≈0.38, but **twitched and fell in ~1 s**. No successful balance on record.

---

## What's next

**The gate is bench validation.** Every remaining static (no-hardware) item is done: both focus
builds compile clean, the native suite is 22/22, the three P1 safety remediations are in the tree,
and the noise-floor measurement layer exists. Nothing further can be advanced *statically* on the
balance loop — the next move requires the physical robot.

1. ~~**FIX the Uno link regression**~~ — **DONE this session** (one-line `platformio.ini`
   `+<math/quaternion.cpp>` addition; `arduino_uno_minimal` SUCCESS at 54.1% flash).
2. **Bench-validate the balance loop** — the one property only hardware can confirm. Carry over
   the three open problems from 2026-05-18 PM late (K spread across pulses, operator-motion
   poisoning the noise baseline, possibly-aggressive pole target vs BNO055 NDOF phase budget).
3. **Bench-confirm the new NaN failsafes** behave (kill-switch cuts on a real BNO055 NaN; the
   NaN→0 PID clamp doesn't glitch on a transient; watchdog-starved cut doesn't false-trip at
   200 Hz).
4. **Retire scope-violation constants** once balancing — every remaining row is bench-gated; the
   5 noise-floor-dependent rows are now unblocked on the measurement side and can be derived from
   `noise_floor_estimator` σ on the next stable run.
5. **Optional static cleanup**: remove/deprecate the dead `i_term`/`gain_to_angle_` mount-estimator
   params; document the online-est 8 s-vs-300 s tc override; replace the
   `run_entered_ms_ = now_ms - 10000` freeze-bypass magic number.

---

*New findings docs this session (read for detail):*
*`docs/findings/security_audit_2026-05-22.md`,
`docs/findings/autocal_autotune_verification_2026-05-22.md`,
`docs/findings/mega_scope_violation_triage_2026-05-22.md`.*

*No commits — all changes are in the working tree.*
