# auto_orientation — Session Synthesis (2026-05-20)

**Agent:** ao-synthesis-audit@floppi:2 (read-only synthesis; re-run after a usage-limit loss)
**Scope:** everything that landed in `auto_orientation/` on 2026-05-20.
**Posture:** static synthesis of landing reports + reviews + spot-checks. NO builds, NO commits, ONE file written.
**In-flight siblings (referenced, not depended on):** `phase_4m14_design_2026-05-20.md` (absent on disk at write time).

---

## 1. Executive summary

A single multi-agent session took the Mega balance stack from a reverted collision
detector to a **code-complete two-stage cascade controller**, and shipped a complete
on-device **guided P→I→D tuning** feature on the Uno. Both trees build green; both
landed with their own reviews. A P1 EEPROM-integrity gap surfaced by the
re-audit (`NEW-P1-001`) was **closed today** in `src/main.cpp`.

| Phase / feature | Verdict (one line) |
|---|---|
| 4M.0 collision restore | DONE — 3-gate detector restored, 27/27 collision tests pass (`state_reconciliation`). |
| 4M.1 wheel-encoder driver | DONE — `wheel_encoder.{h,cpp}` quadrature driver, tested. |
| 4M.2 K cross-check | LANDED, review-sound — encoder/gyro K agreement gate in BOOTSTRAP, `failure_reason=7`. |
| 4M.11 `e` encoder calibration | LANDED, review-sound — blocking wizard, EEPROM slot 0x220, CRC-8-CCITT. |
| 4M.12 PWM-range discovery | LANDED (partial) — happy/abort/timeout/EEPROM solid; 3 spec-vs-impl gaps open (`phase_4m12_landed`). |
| 4M.13 velocity/position outer loop | LANDED, review-sound — `position_loop.{h,cpp}` cascade; **5 gains hardcoded pending 4M.14**. |
| Uno guided P→I→D tuning | LANDED, CLEARED FOR BENCH — 0 P0/P1; `tune_storage` + `tuning_session` + env split. |
| NEW-P1-001 EEPROM CRC upgrade | **CLOSED** — `xor_crc8_` replaced with `calculateCRC8` for slots 0x200/0x210/0x230. |

**Overall verdict:** Mega cascade is *code-complete and review-sound* but **not
bench-tunable** — Phase 4M.14 is a hard procedural gate. Uno guided tuning is
fully ready for operator bench trials.

**Open-finding counts (consolidated across all of today's reviews):**

| Priority | Count | Notes |
|---|---|---|
| P0 | 0 | No runaway-motor / hardware-damage path found. |
| P1 | 2 OPEN | Both prior-audit carryovers (`P1-031`, `P1-018` re-spot-check). The 4 Workstream-F P1s are informational/already-defended. NEW-P1-001 is **CLOSED**. |
| P2 | ~21 OPEN | ~17 prior-audit carryovers + 4 new today (2 Workstream-F docs, 2 security). |
| P3 | ~13 OPEN | Prior carryovers + 4 new today (security observability). |

---

## 2. Per-phase recap

### Phase 4M.0 — Collision-detection restoration
3-gate LIA detector (PEAK ≥12, SUSTAIN ≥8 for 3 ticks, KICK ≥6 m/s² + ≥200 dps)
restored per `architecture_plan §1`. `collision_detected()`/`clear_collision()`
public API; `collision_latched_` volatile, computed in `read_imu_` ATOMIC_BLOCK.
**Files:** `balance_app.{h,cpp}`. **Tests:** `test_balance_app_collision` 27/27 pass
(`state_reconciliation §1`). **Open:** none — fully integrated.

### Phase 4M.1 — Wheel-encoder driver
New `wheel_encoder.{h,cpp}` quadrature driver (Left A/B = Mega 18/19; Right A/B = 2/3).
**Tests:** `test_wheel_encoder.cpp`. **Open:** none flagged.

### Phase 4M.2 — Encoder-driven K cross-check (Workstream F.1)
BOOTSTRAP measures a second, independent plant gain `K_encoder` from wheel velocity
over the *same* pulse set as `K_gyro`. FINALISE aborts if `|K_g−K_e|/max > 0.30`
(`BOOTSTRAP_K_DISAGREE_FRAC`) → `failure_reason=7`, `motors_.stop()`, IDLE.
**Files:** `balance_app.{h,cpp}` (+2 gated members, ~70 LOC, all `#ifdef USE_WHEEL_ENCODERS`).
**Flash/RAM:** mega +8 B RAM (2 floats); uno byte-identical.
**Open:** P1-NUM-1 (divide-by-zero — already guarded, defended), P1-ISR-1 (`raw_gyro_dps_[]`
read — defended at write side), P2-DOC-1 (threshold-rationale comment = TD-3).

### Phase 4M.11 — `e` command + EEPROM encoder calibration (Workstream D)
Operator `e` runs a blocking 1.000 m roll-out wizard; computes counts-per-metre and
wheel radius, persists to EEPROM slot 0x220 (CRC-8-CCITT). Boot read-back applies
saved radius. **Files:** `src/main.cpp` only (~150 LOC gated).
**Flash/RAM (per landing):** mega Flash 14.4%, RAM 17.9%; uno byte-identical.
**Open:** none from Workstream-F review; NEW-P3-003 (silent bad-CRC fallback — no
Serial warning).

### Phase 4M.12 — PWM-range auto-discovery (Workstream E)
`PWM_DISCOVERY` state (value 8), `p` serial command, plateau detector, EEPROM slot
0x230. **LANDED PARTIAL** — happy/abort/timeout/telemetry/EEPROM-save all sound;
`test_balance_app_pwm_discovery.cpp` 10 tests/47 assertions pass.
**Flash:** ~+600 B vs encoder-only baseline (mega 14.7%).
**Open (3 gaps, `phase_4m12_landed §4`):**
- **Gap 1** — collision does NOT abort PWM_DISCOVERY (Test 9 pins behaviour-(b)); decide intent.
- **Gap 2** — EEPROM slot doc/code mismatch: plan said 0x210, enum comment said 0x230, code uses `EE_PWMDISC_ADDR`. Resolved by Workstream-F review + today's CRC fix: **slot is 0x230**, version now 0x02. Doc should be aligned.
- **Gap 3** — `load_pwm_discovery_()` runs at boot but its `min_pwm` is not wired into `stiction_min_pwm`.

### Phase 4M.13 — Velocity/position outer loop (Workstream F.2)
Two-stage cascade: encoder wheel velocity → `PositionLoop` → pitch-setpoint nudge →
inner pitch PID → motor PWM. `PositionLoop` does leaky position integration, control
law `nudge = -K_POS·pos − K_VEL·v`, magnitude clamp ±2°, slew limit 2°/s. Inner loop
unchanged — now tracks a slow-moving setpoint instead of fixed 0.0.
**Files:** NEW `position_loop.{h,cpp}` (~66 LOC, no `<Arduino.h>`/STL); `balance_app.{h,cpp}`
(+1 gated member, ~20 LOC in `step_run_`). `position_loop_.reset()` on every RUN entry.
**Flash/RAM (per landing):** mega 38192 B Flash (15.0%), 1484 B RAM (18.1%), +594 B/+8 B
vs 4M.2 baseline; uno byte-identical.
**Open:** P2-SEQ-1 (5 hardcoded gains — the 4M.14 gate); P2-NUM-1 (encoder-bias windup,
TD-7); P2-DOC-1 (POS_LEAK math comment, TD-4); NEW-P2-002 (stuck-encoder steady-state
lean — no runaway-recovery abort).

### Uno guided P→I→D tuning
On-device interactive serial tuning: `TuningSession` state machine walks
IDLE→STAGE_P→STAGE_I→STAGE_D→REVIEW, bot live and balancing throughout, stage masking
forces the other terms. New `arduino_uno_tuning` build env; `arduino_uno_minimal`
remains the lean flight build (reads tuned gains from EEPROM, falls back to
`balance_constants.h` seed). `brute_tune.py` demoted to optional seed generator.
**Files:** NEW `tune_storage.{h,cpp}`, `tuning_session.{h,cpp}`; `uno_balance_app.{h,cpp}`,
`main.cpp`, `platformio.ini`. **Tests:** `test_tune_storage.cpp`, `test_tuning_session.cpp`.
**Flash/RAM (per `guided_tuning_review §5`):** tuning build 62.8% Flash (20252 B), 615 B
BSS; flight build 51.4% (16572 B). Tuning overhead 3.68 KB (budget 3.0–4.5 KB).
**Open:** P2 (`apply_gains()` input clamp, = TD-5 / NEW-P3-002); P3 (hardcoded UI step
sizes — non-issue by design); NEW-P2-001 (no confirmation prompt before `w` save).

---

## 3. EEPROM slot map (consolidated, both targets)

The Mega and Uno trees use **different layouts at 0x200** — a deliberate divergence,
flagged as a latent footgun (TD-6).

### Mega (`src/applications/balancing_robot/` + `src/main.cpp`)

| Slot | Size | Owner / phase | Magic | Ver | CRC | Posture |
|---|---|---|---|---|---|---|
| 0x000–0x0FF | 256 B | BNO055 calibration blob | 0xCB | 0x02 | CRC-8-CCITT (fixed in morning) | Strong |
| 0x200 | 8 B | Mounting offset | 0xAB | **0x02** | **CRC-8-CCITT (upgraded today)** | Strong |
| 0x210 | 8 B | Actuator / stiction | 0xAC | **0x02** | **CRC-8-CCITT (upgraded today)** | Strong |
| 0x220 | 16 B | Encoder cal (4M.11) | 0xAD | 0x01 | CRC-8-CCITT (new today) | Strong |
| 0x230 | 8 B | PWM-discovery (4M.12) | 0xAD | **0x02** | **CRC-8-CCITT (upgraded today)** | Strong |
| 0x238–0xFFF | 3528 B | Free | — | — | — | — |

### Uno (`src/applications/balancing_robot_uno/`)

| Slot | Size | Owner | Magic | Ver | CRC |
|---|---|---|---|---|---|
| 0x000–0x1FF | 512 B | Calibration blob region (`CAL_EEPROM_SIZE`) | — | — | — |
| 0x200 | 19 B | Guided-tuning block (`TuneBlock`: marker, ver, Kp/Ki/Kd/pitch_off, CRC) | 0xB5 | 0x01 | CRC-8-CCITT |

**Collision verdict:** ✅ **ZERO collisions on either target.** Within Mega: slots
0x200/0x210/0x220/0x230 are each ≤16 B with the next slot ≥16 B higher; magic 0xAD
shared by 0x220/0x230 but addresses distinct (no aliasing). ~3.5 KB free above 0x238.
**Cross-tree:** Mega 0x200 = mounting offset, Uno 0x200 = PID-tune block — distinct
trees, distinct programs, no shared code touches both today; **TD-6 flags this so a
future shared module never aliases it.**

**NEW-P1-001 — CLOSED today.** Spot-checked `src/main.cpp:156-279`: `EE_MOUNT_VER`,
`EE_ACT_VER`, `EE_PWMDISC_VER` all `= 0x02` ("CRC-8-CCITT (was XOR-sum)"); all three
slots' save/load now call `calculateCRC8(buf, 7)` instead of the deprecated XOR-sum.
Version bump 0x01→0x02 invalidates pre-upgrade records (operator re-cal required —
the morning fixer's standard policy). The morning re-audit's top P1 is now retired;
the encoder slot 0x220 already used `calculateCRC8` from day one. Both `mega_balance`
and `uno_balance` rebuilt SUCCESS after the fix (per orchestrator note).

---

## 4. `failure_reason` enum state

Cited from `balance_app.h:132-141` (`BootstrapResult::failure_reason`, `uint8_t`):

| Value | Meaning | Notes |
|---|---|---|
| 0 | ok | — |
| 1 | pitch_out_of_range | — |
| 2 | no_response | — |
| 3 | k_out_of_bounds | — |
| 4 | user_abort | shared by `PwmDiscoveryResult` |
| 5 | collision | LIA spike during baseline/pulse |
| 6 | baseline_noisy | operator handled bot during baseline |
| 7 | k_disagreement | **Phase 4M.2** — Mega-only, `USE_WHEEL_ENCODERS` only |
| 8 | pwm_discovery_timeout | `PwmDiscoveryResult::failure_reason` (`balance_app.h:113`) |

**Values 1–8 are all used.** `BootstrapResult` and `PwmDiscoveryResult` share the
numeric space without collision. **Next free value: 9** — reserved for
`pwm_discovery_collision` if `phase_4m12_landed` Gap 1 is resolved by wiring collision
into PWM_DISCOVERY (option-a). Note motor-stall during RUN routes to HELD via
`held_entry_reason_`, NOT a `failure_reason` code (TD-2 clarifies a stale comment).

---

## 5. Cross-cutting open findings (ADDRESSED / OPEN / DEFERRED)

Walking `workstream_f_review` + `guided_tuning_review` + `ao_security_reaudit` buckets:

| ID / source | Item | Disposition |
|---|---|---|
| WF P1-ISR-1 | `raw_gyro_dps_[]` read outside ATOMIC_BLOCK in `step_bootstrap_` | ADDRESSED (defended at write side; informational; comment = TD-1) |
| WF P1-ISR-2 | `collision_latched_` bool read | ADDRESSED (single-byte read atomic on AVR) |
| WF P1-NUM-1 | K-disagreement divide-by-zero | ADDRESSED (guarded `>1e-6f`; `no_response` gate fires earlier) |
| WF P1-SM-1 | BOOTSTRAP→IDLE on k_disagreement | ADDRESSED (motors stopped before transition) |
| WF P2-GAT/HYG/SEQ-1 | gating, `failure_reason=7` comment, hardcoded gains | OPEN (SEQ-1 = the 4M.14 gate; HYG-1 = TD-2) |
| WF P2-DOC-1/-2 | threshold + POS_LEAK comments | OPEN (TD-3, TD-4 — comment-only) |
| WF P2-NUM-1 (4M.13-9) | encoder-bias integrator windup | OPEN/DEFERRED (informational; surfaces under Workstream K = TD-7) |
| GT P2 | Uno `apply_gains()` no input clamp | OPEN (TD-5 / NEW-P3-002 — defensive; not exploitable today) |
| GT P3 | hardcoded UI step sizes | DEFERRED (non-issue by design — UI increments, not control-law) |
| SEC NEW-P1-001 | Mega `xor_crc8_` weak checksum | **ADDRESSED — CLOSED today** (see §3) |
| SEC NEW-P2-001 | no confirmation prompt before `w` save | OPEN (UX hardening) |
| SEC NEW-P2-002 | stuck-encoder steady-state lean — no runaway-recovery | OPEN (graceful-degradation gap; recommend HELD abort on stuck encoder) |
| SEC NEW-P3-001/-003/-004 | input-parse safety / silent bad-CRC / silent unknown-char | NEW-P3-001 ADDRESSED (safe by structure); -003/-004 OPEN (observability) |
| SEC P1-031 (prior) | BNO085 FRS cal heuristic-only validation | OPEN (explicitly DEFERRED by morning fixer; not in today's workstreams) |
| SEC P1-018 (prior) | BNO085 stack overflow on bad `num_words` | claimed CLOSED — **re-spot-check recommended** (out of today's scope) |
| SEC P1-007/-008/-015/-016 | calibration-blob integrity P1s | CLOSED today (P1-015 in `calibration_storage.cpp` + NEW-P1-001 completes it project-wide) |
| SEC ~17 prior P2s | GPS bounds, I²C timeouts, watchdog, NaN gates, etc. | OPEN (no edits to those files today — carried forward) |
| SEC P2-025 | Uno `last_pitch_deg_` volatile-float tear | ADDRESSED — both reader and writer now `ATOMIC_BLOCK`-wrapped |

---

## 6. ISR-safety audit

The re-audit treats a torn ISR-boundary read as a *security* property (a phantom
value on a Kp=65 PID is a runaway-actuation event). Re-stated conclusions:

- **New tuning modules** (`tune_storage`, `tuning_session`) — **zero** `volatile` /
  `ATOMIC_BLOCK` / `cli()` references; both live entirely on the main-loop side.
  ✅ No ISR-shared state.
- **`apply_gains()` (uno)** — the one place tuning touches PID gain state. `cur_kp_/
  ki_/kd_` are non-volatile floats; a torn read by the ISR PID compute costs at most
  one tick of off-gain output, absorbed by the inner-loop filter + PWM clamp. Both
  reviews flagged and **accepted** the trade-off (locking the ISR would skip a 5 ms
  tick — worse). ✅ Accepted.
- **`last_pitch_deg_` / `pitch_valid_` (uno)** — formerly P2-025; today both writer
  (`uno_balance_app.cpp:108-111`) and reader (`132-135`) wrapped in
  `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. ✅ Closed.
- **4M.2 / 4M.13 (Mega cascade)** — `step_bootstrap_`/`step_run_` run main-loop side;
  `WheelEncoder` velocity reads are windowed counter differences (no I²C, no heap),
  the counter being ISR-updated atomically. `bs_k_enc_sum_` is main-loop-only. ✅ No race.
- **`raw_gyro_dps_[]`** — read in `step_bootstrap_`/`step_run_` outside a local
  ATOMIC_BLOCK; the *write* side in `read_imu_` is ATOMIC_BLOCK-wrapped. Established
  pattern (P1-ISR-1) — recommend an explanatory code comment (TD-1).

**New ISR-shared variable introduced today: none.** All new state (`PositionLoop`,
`bs_*`, tuning modules) is main-loop-owned.

---

## 7. Test coverage state

**Pre-today (representative existing suite):** `test_balance_app*` family
(`_collision` 27/27, `_bootstrap`, `_encoder`, `_pwm_discovery`, `_soft_cutoff`),
`test_pid_controller`, `test_held_state_machine`, `test_calibration_storage`,
`test_l298n_motor`, `test_relay_feedback`, EKF/quaternion/GPS suites.
`audit_test_coverage_2026-05-20` flagged **17 source files with no dedicated test**
(8 non-trivial) and test-framework fragmentation (5 frameworks).

**New today (all present on disk per git status):**
`test_position_loop.cpp`, `test_tune_storage.cpp`, `test_tuning_session.cpp`,
plus `test_safety.cpp`, `test_auto_pid_tuner.cpp`, `test_plant_identifier.cpp`,
`test_wheel_encoder.cpp` (the last four either new or refreshed today).
`test_safety.cpp` uses the `Tests run / passed / failed` harness convention.

**Still uncovered (carried from `audit_test_coverage`):** the 17-file gap is largely
un-retired — sensor drivers (`bno055.cpp`, `bno085.cpp`, `gps.cpp`), `sd_card.cpp`,
`snapshot_recorder.cpp`, `persistent_storage_esp32.cpp` lack dedicated tests.

**Top 3 tests to add:**
1. `test_outer_loop_gain_derivation.cpp` — the 4M.14 derivation (pole-placement from
   verified `K_motor`); known-input → closed-form `K_POS`/`K_VEL`, plus small-`K`/NaN
   edge cases. Required exit criterion for 4M.14 (TD-8 folds the `dt<=0` /
   extreme-`wheel_vel` `position_loop` regression test in here).
2. A native test exercising **PWM_DISCOVERY MIN/MAX convergence** — currently
   bench-only because `BalanceApp`'s encoders are private with no test hook
   (`phase_4m12_landed §5` — add `friend`/`#ifdef NATIVE_TEST` accessor).
3. An EEPROM-integrity regression for the **upgraded Mega slots** — assert a v1
   record (old XOR-sum) is rejected and a v2 round-trips, locking in NEW-P1-001.

---

## 8. Documentation drift

- **`docs/scope.md`** — updated today; §"Platform bifurcation" now describes the
  guided-tuning pivot, the `arduino_uno_tuning` env, and `brute_tune.py`'s demotion.
  README §7's old "DO NOT add serial commands for runtime gain editing / DO NOT
  introduce a new build env" was deliberately overridden by operator clarification
  (`uno_guided_tuning_design §1`). ✅ Aligned with shipped reality.
- **`docs/applications/balancing_robot_uno/README.md`** — updated today (Workstream
  UT-D); describes the two-env split and guided P→I→D workflow. ✅ Aligned.
- **`tools/sim/README.md`** — updated with the `brute_tune.py` demotion note. ✅.
- **DRIFT — `phase_4m12_landed` Gap 2:** the 4M.12 EEPROM slot was variously
  documented as 0x210 (MEGA_UNIVERSAL_PLAN §7d) and 0x230 (`balance_app.h` enum
  comment). Today's CRC fix confirms the **as-built slot is 0x230, version 0x02**;
  `MEGA_UNIVERSAL_PLAN.md` still says 0x210 and should be corrected.
- **DRIFT — `roadmap.md` / `todo.md`:** Workstream DOC-A (the docs-reconcile pass)
  was scheduled to land LAST in the architecture plan; verify it reflects 4M.2/
  4M.11/4M.12/4M.13 + guided tuning as landed, not "planned".
- **`scope.md` violation table** still lists Mega `[deferred-to-mega]` hardcodes
  (`SOFT_ZONE_DEG`, `SAT_THRESHOLD_PWM`, STUCK_* etc.) — accurate, not drift; those
  retirements are genuinely still pending.

---

## 9. Roadmap

### Must-do before a clean bench session

| # | Item | Why it gates the bench |
|---|---|---|
| **1** | **Phase 4M.14 — auto-derive `K_POS`/`K_VEL`** from the encoder-verified `K_motor` (pole-placement). | THE gate. `position_loop.h:47` and `workstream_f_review §4M.13-13` are explicit: **nobody may bench-tune 4M.13 standalone until 4M.14 lands.** The 5 hardcoded gains are a *mechanism*, not a tuned *value*. A permanent hardcode is not an acceptable fallback (scope.md §"The rule"). |
| 2 | Resolve **4M.12 Gap 1** — decide whether collision aborts PWM_DISCOVERY (option-a `failure_reason=9` + test, or option-b documented). | PWM_DISCOVERY behaviour must be defined before the operator runs `p` on the bench. |
| 3 | Wire **4M.12 Gap 3** — `load_pwm_discovery_()` output into `stiction_min_pwm`. | Otherwise discovered PWM bounds are saved but never consumed; the bench result is inert. |

4M.14 carries TD-1/TD-2/TD-3/TD-4/TD-8 along for free (it already opens
`balance_app.{h,cpp}` and rewrites `position_loop.{h,cpp}`).

### Optional polish (not bench-blocking)

- The **3 comment-only P2 cleanups** — TD-1 (`raw_gyro_dps_` safety comment), TD-3
  (K-disagreement rationale), TD-4 (POS_LEAK time-constant math). Bundle with 4M.14.
- **`apply_gains()` input clamp** (TD-5 / GT-P2 / NEW-P3-002) — one-liner, defensive;
  bundle with a Uno cleanup pass or Workstream M.
- NEW-P2-001 (`w`-save confirmation prompt), NEW-P3-003/-004 (Serial warnings on
  bad-CRC / unknown char) — UX/observability, low priority.

---

## 10. Beyond the bench (forward look)

From `architecture_plan §4/§7` and `ao_roadmap_post_4m14`:

- **Session N+1 — "Land the gate":** Workstream 4M.14-impl (single focused agent;
  the in-flight `phase_4m14_design` doc is the contract) + Workstream N (build-matrix
  CI, flash/RAM budget asserts — fully orthogonal, pure infra).
- **Session N+2 — "Make the bench real":** Workstream G (bench-tuning protocol +
  telemetry pipeline) — gated by operator decision #1 (is the auto-derived gain
  *authoritative*, or is operator override permitted? — doctrine strongly favours
  authoritative). Alongside: Workstream M (cross-bot config-unification audit, doc-only,
  carries TD-5/TD-6) and Workstream J (failure-mode black-box recorder, code half).
- **Session N+3+:** Workstream K (disturbance-rejection benchmark — bench-only),
  Workstream O (IMU cal UX review).
- **Deferred:** Workstream H (auto-BOOTSTRAP on power-up — a real safety decision,
  operator-arm is the safe default) and Workstream L (Uno autonomous self-tune —
  re-merging it would violate scope.md §"Platform bifurcation").
- **Long-range stretch:** fall recovery / self-righting; lateral/heading cascade
  (Phase 4M.16); ESP32 LAN-only WiFi telemetry (Phase 6). Permanently out of scope:
  trajectory planning, autonomy, multi-bot coordination.

---

## 11. Risks & open questions

1. **4M.14 derivation may be intractable** (HIGH impact). If pole-placement cannot
   produce stable outer gains across the plausible `K_motor` range, the §3 gate stays
   open *indefinitely*. The in-flight design doc must state the valid `K_motor` range;
   a bench-characterised outer gain is the on-philosophy fallback — a permanent
   hardcode is not.
2. **Bench access timing** (MED). Workstreams G/K and the validation halves of
   J need a confirmed operator bench session; without one the un-validated queue
   grows. Code-only halves still land green.
3. **4M.12 is "landed partial"** — three open spec-vs-impl gaps. The bench session
   cannot trust the `p` command's full behaviour until Gap 1 (collision intent) and
   Gap 3 (consumer wiring) are closed; Gap 2 is now resolvable (slot = 0x230 v2).
4. **Cross-tree EEPROM 0x200 divergence** (TD-6) is a latent footgun — Mega 0x200 =
   mounting offset, Uno 0x200 = PID-tune block. Safe today (no shared code), but any
   future shared module that aliases 0x200 is a bug. Flag persists until Workstream M.
5. **Prior-audit P1-018 / P1-031 unverified** — the morning fixer claims P1-018
   (BNO085 stack overflow) closed; the re-audit could not re-spot-check it. P1-031
   (FRS heuristic validation) is explicitly deferred. Both should be confirmed before
   the next bench session if BNO085 is in the loop.
6. **Operator decision #1 (bench-tuning policy)** is unanswered and gates Workstream
   G's entire design. Recommended default (auto-derived authoritative) is strongly
   indicated by scope.md doctrine but should be explicitly confirmed.

---

*Synthesis complete. The Phase 4M.14 gate is the critical path: the Mega cascade is
code-complete and review-sound, but bench-tuning is procedurally blocked until 4M.14
retires the hardcoded outer-loop gains. The Uno guided-tuning feature is independently
cleared for bench use. No commits, no builds. One file written.*
