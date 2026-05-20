# Mega Universal Balance Stack — Cleanup & Retarget Plan

Status: PLAN (2026-05-19, updated PM after multi-agent landing wave). Written immediately after the strategic pivot that bifurcated the auto-balance code into a **Mega-only universal stack** and a **Uno-only minimal hardcoded program**. This document tells the next coding session how to clean up the universal stack now that the Uno flash budget no longer constrains it, and how to land wheel encoders + position containment + outstanding audit fixes in the right order.

Audience: the coding agent or operator who picks up `mega_balance` next.

---

## Session landings post-plan (2026-05-19 PM)

The multi-agent wave that followed this plan's first draft has landed most of items 1-3 from §9 plus a chunk of §6/§7 work. Working-tree state at end-of-session (no new commits):

1. **Collision detection re-landed** in `balance_app.{h,cpp}` — 27/27 native tests pass. 3-gate detector live: PEAK 12 m/s² single-tick / SUSTAIN 8 m/s² for 3 ticks / KICK 6 m/s² with |gyro| > 200 dps. Constants in `balance_app.h:178-182`; detector loop at `balance_app.cpp:1639-1648`. Matches [findings/research_collision_signature_bno055.md](findings/research_collision_signature_bno055.md) §6 checklist row-for-row.
2. **Wheel encoder driver** in `src/sensors/wheel_encoder.{h,cpp}` — 17 native tests pass. PJRC Encoder library added to `mega_balance` env in `platformio.ini`.
3. **Encoder integration into `balance_app`** — 25 new tests pass. Pin map fixed in `src/config/pins.h`: `L_ENC_A=18`, `L_ENC_B=19`, `R_ENC_A=2`, `R_ENC_B=3`. Stall detection wired to HELD with `failure_reason=7`.
4. **Phase 4M.12 PWM auto-discovery** — code landed (49 `PWM_DISC*` refs split across `balance_app.h`/`balance_app.cpp`). Mega flash 14.1 % → 14.7 % (+0.6 % for this feature). Native test file is being written by a sibling verification agent.
5. **`src_filter` duplicate-symbol fix** — all four envs (`uno_balance`, `mega_balance`, `mega_orientation`, `arduino_uno_minimal`) link cleanly. Mega is unblocked.
6. **Uno minimal — P0 fixes** — startup delay + `ATOMIC_BLOCK` + `<stdint.h>` include. 17/17 native tests still green.
7. **Uno minimal — P1 top-5 fixes** — 33/33 native tests pass. New operator commands: `g` (arm-after-abort) and `p` (periodic telemetry on/off).
8. **Constants P0 fix** — Python tuner template emits a header in the file-scope `BALANCE_KP/KI/KD + PWM_MIN/MAX + STICTION_PWM + TIP_CUTOFF_DEG + PITCH_SANITY_DEG` shape that `uno_balance_app.cpp` actually consumes. End-to-end `brute_tune.py --output ... && pio run -e arduino_uno_minimal` workflow now builds.
9. **Tuner Kd accuracy** — random-search Kd now lands at ~62 vs reference 38 (was ~16). Verdict: keep — overshoots reference, but stable region was widened; tuner-side derivation discussed in [findings/tuner_kd_accuracy_2026-05-19.md](findings/tuner_kd_accuracy_2026-05-19.md).
10. **Phase 4.11a position containment — DESIGN landed** in [findings/phase_4_11a_design_2026-05-19.md](findings/phase_4_11a_design_2026-05-19.md). Encoder-primary outer-loop spec with IMU-only fallback. Implementation deferred (would have conflicted with the Phase 4M.12 PWM-discovery agent that was simultaneously touching `balance_app.cpp`).
11. **`mega_orientation` RAM overflow diagnosed** in [findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md](findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md). Root cause: EKF stub. ~2257 B reclaimable across three Phase A fixes. Not yet executed.
12. **Mega encoder bench bring-up guide** in [guides/encoder_bench_bringup.md](guides/encoder_bench_bringup.md) (~450 lines) — operator-facing wiring + verification recipe for the encoder driver.

The §6/§7/§8 sections below are kept in their original "tracking" wording but cross-link the landed state. §9 (recommended next-session task list) has been rewritten because most of its early items are now done.

---

## 1. Executive summary

The bifurcation is partly landed: `arduino_uno_minimal` builds at ~50% Uno flash; `mega_balance` is the new home for **BOOTSTRAP, RLS PlantIdentifier, OnlineMountingEstimator, collision detection, wheel encoders, Phase 4.11a position containment**. Done: P0 atomicity fixes, K-quality gate, baseline cap, Uno-minimal env, Python brute-force tuner, wheel-encoder + collision research. Open: re-landing collision detection (reverted by audit-fix wave; sibling agent in this wave), implementing the encoder driver (sibling), reverting Uno-flash workarounds that hurt Mega clarity, chewing through 14 P1 + 15 P2 audit items. **Next session's #1 task: land the wheel-encoder driver + serial `e` calibration** — unblocks the brute-force tuner AND replaces the option-B pitch-integrator before it's even merged.

---

## 2. `platformio.ini` reorganization recommendations

Do **NOT** apply now — the sibling encoder agent may touch `platformio.ini` in this same wave. These are recommended diffs for the next session that owns the file uncontested.

### 2a. New build flags on `mega_balance`

```ini
[env:mega_balance]
extends = base_balance, balance_src_filter
platform = atmelavr
board = megaatmega2560
build_flags =
    ${base_balance.build_flags}
    -D USE_WHEEL_ENCODERS
    -D USE_COLLISION_DETECTION
    -D USE_POSITION_CONTAINMENT
    -D USE_PWM_RANGE_DISCOVERY
lib_deps =
    ${base_balance.lib_deps}
    paulstoffregen/MsTimer2 @ ^1.1
```

Rationale for each flag:

| Flag | Default ON for env | Why a flag at all (vs. always-compiled) |
|---|---|---|
| `USE_WHEEL_ENCODERS` | `mega_balance` | Uno-minimal MUST not pull this in (Uno has 2 INT pins, can drive at most 1 encoder; Uno-minimal is hardcoded anyway). `arduino_uno_minimal` MUST NOT define it. |
| `USE_COLLISION_DETECTION` | `mega_balance` | Sensor scaffolding (`bno055::getLinearAccel`) is already always-compiled; the *detector logic + state-machine integration* gate behind this flag so a regression cost can be isolated by toggling. |
| `USE_POSITION_CONTAINMENT` | `mega_balance` | Lets Phase 4.11a (option B pitch-integration) ship as an **IMU-only fallback** even on Mega when encoders are absent or fail calibration. |
| `USE_PWM_RANGE_DISCOVERY` | `mega_balance` | New `CHAR_PWM_RANGE` state from wheel-encoder research §6; gated so old `mega_orientation` builds don't pull in actuator-sweep code. |

### 2b. Uno-specific restrictions to relax on Mega

Keep `balance_src_filter` shared with Uno-balance, but add `[mega_balance_src_filter]` extending it to selectively re-include `features/snapshot/` and `output/serial_telemetry.cpp` (Mega has the room). Don't re-add BNO085/GPS/EKF until those become first-class on Mega.

### 2c. Move `mega_balance` to `default_envs`

Once Mega is the lead bench platform, change line 22 from `default_envs = uno_balance` to `default_envs = mega_balance`. One-liner; do with operator confirmation since Uno-minimal bench-test is still active.

---

## 3. Mega flash / RAM headroom analysis

Mega ATmega2560 specs vs. Uno ATmega328P:

| | Uno | Mega | Ratio |
|---|---|---|---|
| Flash | 32 KB | 256 KB | **8×** |
| SRAM | 2 KB | 8 KB | **4×** |
| EEPROM | 1 KB | 4 KB | **4×** |
| External INT pins | 2 | 6 | 3× |
| Hardware timers (16-bit) | 1 | 4 | 4× |

Uno baseline (2026-05-18 memory): flash **94.9%** = 30.4 KB; RAM ~700 B used. Mega builds run 1.5-2× larger for the same code (wider PC, RAMPZ, bigger IVT): projected total **~50-60 KB flash (~20-25%)** and **~750 B RAM (~9%)**. **Net Mega headroom: ~200 KB flash, ~7 KB RAM.** Encoder driver (~600 B), Phase 4.11a (~150 B), and audit P1 fixes (~200 B) are noise.

### Headroom budget for future work

| Item | Est. flash | Est. RAM | Notes |
|---|---|---|---|
| Wheel encoder driver (`src/sensors/wheel_encoder.{h,cpp}`) | ~600 B | ~80 B (2 instances × ~40 B) | Per research_wheel_encoders §3 |
| CHAR_PWM_RANGE state + `p` command | ~300 B | ~16 B | New state handler + ramp accumulator |
| Phase 4.11a `PositionContainment` module | ~150 B | ~20 B | Option B pitch-integration; lives even when encoders present (fallback) |
| Phase 4.11b/c encoder cascade integration | ~250 B | ~16 B | Outer loop in step_run_ |
| Collision detection re-implementation | ~150 B | ~28 B | Already costed in collision research |
| All 14 audit P1 fixes (combined) | ~200 B | +0 to +30 B | Mostly `noInterrupts/interrupts` pairs |
| **Subtotal new** | **~1.65 KB** | **~190 B** | |
| Mega budget remaining after subtotal | **~198 KB** | **~6.8 KB** | Plenty for telemetry + WiFi + dashboard |

**Bold decision: stop optimising for flash on Mega.** Future work (WiFi, dashboard, multi-IMU, EKF) all comfortably fit. Code clarity wins over byte-shaving.

---

## 4. Flash optimizations to REVERT for clarity on Mega

Uno-driven optimisations that hurt readability. Most survive the Uno-minimal path untouched (different src tree) so reverting is clarity-only.

| Change | Where | Revert? | Rationale |
|---|---|---|---|
| snprintf-chain deletes in BNO055 (2026-05-18 AM, freed ~1.7 KB) | `src/sensors/bno055.cpp` | **Y** | `snprintf` far cleaner than 20 lines of `print()` chains. Mega has flash. Uno-minimal doesn't pull these verbose paths. |
| Stack-allocated `Adafruit_BNO055` | `bno055.cpp` ctor | N | Idiomatic on AVR; no clarity cost. |
| Aggressive `F()` macros | `main.cpp`, `drain_state_log`, telemetry | N | PROGMEM strings are free RAM; keep. |
| Manual inlining in hot loops | `balance_app.cpp:280-300, 400-500` | Partial | Refactor into static helpers off the ISR path. Keep inlining inside `tick()` 5 ms body. |
| `static const float` instead of `constexpr` | `balance_app.cpp:69-100` | Y | Cleanup pass — `constexpr` is more idiomatic, same code size. |
| `uint16_t` overflow accumulators (P1-NUM-1) | `balance_app.cpp:478` | Y | Promote to `uint32_t`. +2 B RAM irrelevant on Mega. |
| Cast-tricks for int math | scope.md "pending" row | **N (don't apply)** | Proposed Uno opt; don't introduce on Mega. |
| **Remove `AUTO_TUNE` enum + `step_tune_()`** (~500 B) | `balance_app.cpp:280, 362-400, 854-867` | **Y** | Unreachable since BOOTSTRAP took over. Removes `step_tune_`, case label, `kDefaultTune*` constants, `tune_result_` member. Saves ~500 B flash + 28 B RAM + eliminates a misleading state. |
| **Stale `kDefaultInitial{Kp,Ki,Kd}`** | `balance_app.cpp:84-86` | **Y** | scope.md marks retired but still in source as ctor fallbacks. Replace with BOOTSTRAP-required gate: refuse RUN unless BOOTSTRAP ran (return `failure_reason = no_bootstrap`). |
| `tune_result_` member | `balance_app.h:441-442` | Y | Dies with AUTO_TUNE removal. |

**Order**: AUTO_TUNE removal first (biggest win + clarity). Then BNO055 snprintf revert. Then constexpr + uint32 promotion. Defer Adafruit alloc + F() until dashboard/WiFi forces the trade-off.

---

## 5. Audit-fix forward plan

Reference: `docs/findings/audit_code_quality_balance_stack_2026-05-19.md`. P0 status updated to reflect Coding B's fixes already landed.

### P0 (must-fix, blocks correctness)

| ID | Title | Status | Mega? | Flash est. |
|---|---|---|---|---|
| P0-ISR-1 | `pitch_deg_` torn read | **FIXED** (Coding B) | N/A | — |
| P0-ISR-2 | `raw_gyro_dps_[3]` torn read | **FIXED** (Coding B) | N/A | — |
| P0-ISR-3 | `last_output_` torn read | Demoted (ISR-local) | N/A | — |
| P0-ISR-4 | stale-mount kill-switch read | Reclassified (not P0) | N/A | — |

### P1 (latent bugs) — 14 items, ordered by recommended sequence

| # | ID | Title | Flash | Notes |
|---|---|---|---|---|
| 1 | P1-SM-1 | `enter_state_(RUN)` clobbers BOOTSTRAP K via `plant_id_.reset()` | +10 B | **Critical, ~3 lines.** Restores BOOTSTRAP value. |
| 2 | P1-COV-1 | No test for BOOTSTRAP→RUN K preservation | 0 | Write FIRST, watch red→green after fix 1. |
| 3 | P1-NUM-1 | `ch_gyro_acc_x10_` uint16 overflow | +2 B RAM | Promote to `uint32_t`. |
| 4 | P1-NUM-2 | RLS covariance can go negative | ~30 B | Clamp `P_` to `[1e-6, 1e6]`. |
| 5 | P1-ISR-1 | `collision_latched_` ISR vs loop race | ~20 B | Re-lands with collision PR. |
| 6 | P1-ISR-2 | `pulse_log_.seq` torn read in `drain_pulse_log` | ~30 B | Local copy under `noInterrupts()`. |
| 7 | P1-SM-3 | HELD doesn't record collision reason | ~15 B | Add `held_entry_reason_`. |
| 8 | P1-SM-4 | FALLEN short-press dead-code | 0 | Wrap in `#ifdef USE_BALANCE_FALL_DETECTION`. |
| 9-11 | P1-COV-2..4 | Test gaps: `clear_collision`, STUCK, freeze-gate priority | 0 | Test-only. |
| 12 | P1-DOC-1 | Stale "Iterated gains 2026-05-12" comment | 0 | Comment fix. |
| 13-14 | P1-SM-2, P1-NUM-3 | Withdrawn (not bugs) | — | — |

**Fix items 1+2 together** — highest-impact pair; only test of the BOOTSTRAP value proposition.

### P2 (code-quality) — 15 items, deferred batch

| Cluster | Items | Action |
|---|---|---|
| Dead AUTO_TUNE | P2-SM-1, P2-NUM-3, P2-DOC-1 | Roll into AUTO_TUNE removal (§4) |
| Float-heavy `read_imu_` | P2-NUM-1, P2-NUM-2 | Defer (CPU tax, not correctness) |
| Stale doc comments | P2-DOC-2, P2-DOC-3, P2-DOC-4 | Sweep with AUTO_TUNE removal |
| Collision header → cpp | audit §6 row 8 | Move `constexpr` thresholds to anonymous namespace |
| Test coverage gaps | P2-COV-1, P2-COV-2 | Next test session |
| scope.md rows 8-21 | derivation reminders | Address as subsystems get re-touched |

**Net flash impact of P1+P2 cleanup on Mega: ~-400 B** (AUTO_TUNE removal dwarfs atomic-block additions). Tiny.

---

## 6. Collision detection re-implementation tracking — LIVE 2026-05-19 PM

**Status: LANDED in working tree (no commit yet).** Re-implementation done by sibling agent during the 2026-05-19 PM wave. **27/27 native tests pass** (`tests/test_balance_app_collision.cpp`). All §3/§6 checklist rows from `research_collision_signature_bno055.md` verified row-for-row:

| Check | Expected | Status |
| --- | --- | --- |
| 3-gate detector (`PEAK \|\| SUSTAIN \|\| KICK`) | grep `COLLISION_PEAK_MPS2`, `COLLISION_SUSTAIN_MPS2`, `COLLISION_KICK` | OK — `balance_app.cpp:1645-1648` |
| PEAK / SUSTAIN / KICK thresholds | 12 / 8 (for 3 ticks) / 6 (with \|gyro\|>200 dps) m/s² | OK — constants in `balance_app.h:178-182` |
| Burst-read 6 bytes 0x28-0x2D | Single `Wire.requestFrom(addr, 6)` in `bno055::getLinearAccel` — F1 register-tear guard | OK |
| `isnan` guard + clamp ±40 m/s² | Top of consumer paths | OK |
| Cooldown 200 ms + `cal_accel ≥ 2` gate | Re-arm timer + fusion-jump (F7) suppression | OK |
| ATOMIC_BLOCK around cached LIA float vector | Same pattern as P0-ISR-1 for `pitch_deg_` | OK |
| 27 collision native tests pass | `pio test -e native_test --filter test_balance_app_collision` | OK |

**Encoder integration shares the LIA read path** — both features now live cleanly. No follow-up verification needed at the start of next session; pre-existing tests catch any regression.

---

## 7. Wheel encoder integration into `balance_app` — DRIVER + INTEGRATION LIVE 2026-05-19 PM

**Status table (working tree state — no commit yet):**

| Layer | Status | Tests |
| --- | --- | --- |
| `src/sensors/wheel_encoder.{h,cpp}` driver | LANDED | 17/17 native pass |
| `balance_app` integration (init, sampling, stall detection wiring) | LANDED | 25 new tests pass |
| Pin map (`L_ENC_A=18`, `L_ENC_B=19`, `R_ENC_A=2`, `R_ENC_B=3`) | LANDED in `src/config/pins.h` | n/a |
| Stall→HELD route with `failure_reason=7` | LANDED | covered in encoder tests |
| Phase 4M.12 `CHAR_PWM_RANGE` state + `p`-style trigger | CODE LANDED (49 `PWM_DISC*` references in `balance_app.{h,cpp}`); Mega flash 14.1 % → 14.7 % | TEST FILE PENDING (sibling agent writing now) |
| Phase 4M.13 encoder velocity outer-loop cascade | NOT STARTED (now Phase 4.11a, see §8) | — |

This section's original wording (below) still describes **how** the integration is wired and what the three integration points are. Keep it as a reference for the cascade/outer-loop work pending in Phase 4.11a.

### 7a. Three integration sites

| # | Site | Signal in | Effect | Phase |
|---|---|---|---|---|
| 1 | **Stall / stiction detection** in `step_run_` | `enc_L.read_velocity_dps()`, `enc_R.read_velocity_dps()` | If `|enc_velocity_dps| < 2.0f` while `|pwm_command| > stiction_min_pwm` for >100 ms → set `motor_stalled_` flag. Routes to HELD + soft-cutoff in next tick. | 4M.13 |
| 2 | **Outer position cascade** in `step_run_` (replaces option B) | `0.5 * (enc_L.vel_mps + enc_R.vel_mps)` | Integrate to position; emit `pid_.set_setpoint(nudge_deg)` per research_wheel_encoders §6. | 4M.13 |
| 3 | **PWM range auto-discovery** = new `CHAR_PWM_RANGE` state | `enc_L.vel_dps`, `enc_R.vel_dps` | Ramp PWM 0→255 in steps of 5 every 200 ms; first non-zero velocity = `PWM_MIN`, velocity plateau = `PWM_MAX`. Save to EEPROM `0x210`. | 4M.12 |

Recommended order: 4M.12 first (one-shot setup, no controller change), then 4M.13 (replaces existing setpoint=0 line).

### 7b. Stall detection

Existing STUCK detector (`balance_app.cpp:560-576`) fires on `|out|>180 PWM` ∧ `|gyro|<5dps` for 1500ms. With encoders, replace with `|enc_vel_dps|<2 ∧ |pwm|>stiction_min_pwm` for 100ms → `enter_state_(HELD)` with `held_entry_reason_ = HELD_REASON_STALL`. Keep gyro-based STUCK as `!USE_WHEEL_ENCODERS` fallback.

### 7c. Outer position loop

Per research_wheel_encoders §6: integrate `v_mps = 0.5 * (enc_L + enc_R)` with 20 s leak, emit `nudge = -K_POS_M*position - K_VEL_MPS*v` clamped to `±MAX_NUDGE_DEG`, slew-limit, pass to `pid_.set_setpoint(nudge)`. Constants hand-tuned at Phase 4M.13b; auto-derived from `K_motor` via pole-placement at Phase 4M.14.

### 7d. PWM range auto-discovery (new Phase 4M.12)

New state `CHAR_PWM_RANGE = 8` triggered by `p` command, bot on stand:
1. 500 ms quiet window, verify `vel=0`.
2. Ramp PWM 0→255 in steps of 5, hold each 200 ms.
3. First step with `|vel|>5 dps` → `PWM_MIN_{L,R}`. 3 consecutive non-increasing steps → `PWM_MAX_{L,R}`.
4. Save to EEPROM `0x210` (extend actuator slot). Print summary.

Feeds (a) `L298NMotorDriver::stiction_min_pwm = max(PWM_MIN_L, PWM_MIN_R)` (replaces hardcoded 80) and (b) Python brute-force tuner with realistic bounds. **This is the unblock for the brute-force tuner to converge in reasonable wall-clock time.**

---

## 8. Phase 4.11a position containment — encoder-first design

**Status: DESIGN COMPLETE 2026-05-19 PM.** Full design landed in [findings/phase_4_11a_design_2026-05-19.md](findings/phase_4_11a_design_2026-05-19.md) (~590 lines): odometry math, outer-loop cascade structure, runtime gate for encoder-vs-IMU fallback, slew limits, EEPROM persistence, bench-validation steps. **Implementation deferred to next session** — Phase 4M.12 PWM-discovery agent was simultaneously touching `balance_app.cpp` and a parallel outer-loop edit would have collided. Next session: implement per the design doc; no further research needed.

`research_imu_only_position_containment.md` ships option B (pitch double-integration) as the universal answer. With encoders on Mega, **invert it**: encoder odometry is primary; IMU-only is the fallback.

**Runtime gate (preferred over compile-time)** for graceful degradation when encoders fail mid-mission:

```cpp
const bool encoders_ok = enc_L_.is_healthy() && enc_R_.is_healthy()
                      && (now_ms - last_encoder_cal_ms_ < ENCODER_STALE_MS);
if (encoders_ok && !USE_IMU_ONLY_OUTER_LOOP) {
    nudge = encoder_outer_loop_(now_ms);
} else {
    nudge = pos_containment_.update(meas, dt_s, freeze);   // option B fallback
}
```

`is_healthy()` returns false if `tick_count` hasn't moved despite PWM. Default `USE_IMU_ONLY_OUTER_LOOP=false`. Ship both paths because (1) encoder hardware fails (magnet, wires, ESD), (2) cal goes stale (surface change, slip, wrong CPM), (3) flag-flip A/B comparison is the only way to tell if encoder tuning is actually better than IMU baseline.

---

## 9. Risk + ordering — recommended next-session task list (UPDATED 2026-05-19 PM)

The original task list (collision verify, BOOTSTRAP K-clobber fix, encoder driver, encoder integration, PWM auto-discovery code) is **almost entirely done in working tree**. What remains, ordered for impact + dependency. Effort estimates: **S** = <2 h, **M** = half day, **L** = full day.

| # | Task | Effort | Why this order | Risk |
| --- | --- | --- | --- | --- |
| 1 | **Land Phase 4M.12 PWM-discovery native test** (sibling agent writing now; double-check on next session start) | S | Closes the loop on the code already in `balance_app.{h,cpp}` | Low |
| 2 | **Implement Phase 4.11a position containment** per [findings/phase_4_11a_design_2026-05-19.md](findings/phase_4_11a_design_2026-05-19.md) — encoder-primary outer loop + IMU-only fallback via `USE_IMU_ONLY_OUTER_LOOP` runtime gate | L | The "stop wandering" payoff; biggest remaining behavioural deficit on the Mega path. Design is complete. | Med — gain tuning is the bench unknown; start gentle per design §5 |
| 3 | **`mega_orientation` EKF RAM-overflow Phase A fixes** per [findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md](findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md) | M | Frees ~2.2 KB so `mega_orientation` env builds again | Low — diagnosis already pinpoints the bytes |
| 4 | **Uno-minimal P1 follow-ups (#6-#15 from audit)** | M | Continue retiring the P1 batch begun this session | Low |
| 5 | **Uno-minimal P2 batch (12 findings)** | M | Code-quality cleanup; defer until P1 done | Low |
| 6 | **Telemetry polish on Mega**: periodic RUN telemetry (pitch / output / mount-offset / K_motor every 100 ms) | S | Visibility into the balance loop during the Phase 4.11a bench bring-up | Low |
| 7 | **Operator encoder commands** — CPR readout, distance, save calibration. Needs serial parser additions in `balance_app.cpp` | S | Operator workflow polish; needed before brute-tune-with-bench-PWM workflow can converge | Low |
| 8 | **Retire `kDefaultInitial{Kp,Ki,Kd}` + remove AUTO_TUNE dead code** (§4 cleanup wave) | M | Clarity wins; gates RUN-entry on BOOTSTRAP-success | Low — pure cleanup, tests catch regressions |
| 9 | **Update Python tuner stress-plant Kd** per [findings/tuner_kd_accuracy_2026-05-19.md](findings/tuner_kd_accuracy_2026-05-19.md) caveat — under-tunes on the stress preset | S | Quality of the tuner output | Low |
| 10 | **Remaining Phase 4M audit P1 batch (items 3-8 from §5)** | M | Numerical safety + ISR-race fixes | Low |
| 11 | **K_motor cross-verification via encoder during BOOTSTRAP** (Phase 4M.14) | M | Independent confirmation of plant gain. Adds `failure_reason=7` (K_disagreement). | Low — additive sanity check |

**Estimated total**: ~3 days bench + dev (split across two sessions).

**Phase 4.11a (item 2) is the single highest-impact remaining item** — encoder driver and integration are in place; the outer loop is what turns those raw ticks into "the bot doesn't wander into walls".

---

## 10. Cross-references

- `docs/findings/research_collision_signature_bno055.md` — 3-gate spec (12 / 8+3-tick / 6+200dps), F1-F10 FPs, §6 recipe. Detector now LIVE — see §6.
- `docs/findings/research_imu_only_position_containment.md` — option B (IMU-only fallback on Mega when encoders fail).
- `docs/findings/research_wheel_encoders_mega_2026-05-19.md` — pin allocation, driver, calibration, integration, bench tests. Driver + integration now LIVE — see §7.
- `docs/findings/phase_4_11a_design_2026-05-19.md` — encoder-primary position containment design. Implementation queued — see §8 + §9 item 2.
- `docs/findings/tuner_kd_accuracy_2026-05-19.md` — Kd accuracy investigation + stress-plant caveat.
- `docs/findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md` — EKF-stub RAM diagnosis; Phase A fixes queued in §9 item 3.
- `docs/findings/audit_code_quality_balance_stack_2026-05-19.md` — full P0/P1/P2 audit (reorganised in §5).
- `docs/findings/audit_uno_minimal_2026-05-19.md` — Uno-minimal audit (P1 #6-#15 and P2 batch tracked in §9).
- `docs/guides/encoder_bench_bringup.md` — operator wiring + verification recipe for the encoder driver.
- Memory `project_strategic_pivot_2026-05-19.md` — bifurcation rationale.
- Memory `project_universal_balance_vision.md` — "no per-bot config" north star (Mega-only now).
- Memory `project_balance_bot_state_2026-05-18_pm_late.md` — last bench before pivot.
- Memory `feedback_balance_bot_preferences.md` — operator hard constraints.
- `src/applications/balancing_robot/balance_app.{h,cpp}` — universal stack; **Mega-default after this plan lands**.
- `src/applications/balancing_robot_uno/` — minimal Uno program (generated constants).
- `src/sensors/wheel_encoder.{h,cpp}` — quadrature encoder driver (LIVE).
- `src/config/pins.h` — encoder pin map: `L_ENC_A=18`, `L_ENC_B=19`, `R_ENC_A=2`, `R_ENC_B=3`.
- `tools/sim/brute_tune.py` — consumes PWM bounds from Phase 4M.12.
- `platformio.ini` — env definitions; diffs in §2. PJRC Encoder lib added to `mega_balance`.
- `docs/scope.md` — 21-row table; updates pending after AUTO_TUNE removal + `kDefaultInitial*` retirement.

---

*Plan complete. Source files untouched. No git operations performed.*
