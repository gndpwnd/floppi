# Session record — 2026-05-27: AO finishing plan (AO-FIN-01..08) — doc rot closure, defensive guards, Uno arm-guard, Mega backup command, AUTO_TUNE excision

**Date**: 2026-05-27
**Author**: doc agent (`ao-save-progress-finishing@floppi:1`)
**Scope**: `auto_orientation/` only. Flight-controller side untouched this session.
**Commit status**: **NO COMMITS this session.** Everything below is uncommitted in the working tree per operator instruction. The prior 2026-05-26 wave-6 + wave-8 work was also uncommitted (continues in the same working tree).

---

## Scope of this record

Eight workitems (`AO-FIN-01` .. `AO-FIN-08`) executed across **four execution phases**, planned by a schema-validated synthesizer workflow that:

1. Fanned out **5 parallel audit agents** over the AO source + doc surface (one wave each: env-naming, Uno safety, Mega doc/feature parity, defensive-guard sweep, navigation/onboarding polish).
2. Ran a **synthesizer agent** that produced a schema-validated 8-workitem plan with explicit `deferred_to_hardware` lists, per-item write zones, and verification expectations.
3. Dispatched the 8 workitems in 4 disjoint-zone phases so no two coding agents wrote to the same file in the same phase.

The dominant finding from the audit wave was **documentation rot** (env names, deleted features, missing onboarding tree, wrong polarity notes, AUTO_TUNE references long after the code was excised). The dominant finding from the security/code-quality audit was that the **code surface itself was close** — only P3 nits remained. The plan therefore weighted heavily toward doc closure + a small number of high-confidence defensive guards + one feature parity item (Mega `'B'` backup) + dead-code excision (AUTO_TUNE on Mega).

Canonical operator-facing entry-points after this session:
- [`docs/applications/CHOOSE_YOUR_TIER.md`](../../applications/CHOOSE_YOUR_TIER.md) — decision tree at the top of the application catalogue.
- [`docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md`](../../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) — Mega-tier first-success path.
- [`docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md`](../../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md) — Uno-tier first-success path.

---

## What landed (working tree only — uncommitted)

### Phase 1 — naming consistency + Uno arming guard

#### AO-FIN-01 — env-name rename across the doc + comment surface

**35 occurrences across 11 doc files + 1 source-comment site in `src/main.cpp`.** Two context-dependent substitutions:

- `arduino_uno_balancing` → `arduino_uno_minimal` (when referring to the lean flight build) **or** `arduino_uno_tuning` (when referring to the SETUP-mode build that contains the guided wizards). The audit pass picked the right target per surrounding sentence.
- `arduino_mega_balancing` → `mega_balance` (single Mega env).

Why this mattered: every `pio run -e <env>` invocation in onboarding docs used to refer to env names that did not exist in `platformio.ini` after the 2026-05-19 strategic pivot / 2026-05-26 SETUP-mode split. A new operator following any quickstart hit `UnknownEnvironment` immediately. With this rename the docs and `platformio.ini` agree.

Source comment site: `src/main.cpp` had an `// arduino_uno_balancing` build-gate comment that was renamed in-place.

#### AO-FIN-04 — Uno refuse-to-arm guard + `'F'` force-arm + platformio.ini onboarding-flow comment

`uno_balance_app.cpp` + `uno_balance_app.h` + `main.cpp` + `platformio.ini`:

- **Refuse-to-arm guard.** On `arduino_uno_minimal`, the app now refuses to enter the run loop if no calibration blob is present (`tune_storage::has_cal_blob() == false`) **and** no `BNO055_CAL_BLOB[22]` photo-backup hardcode is compiled in (default-`0xFF` sentinel). The operator gets an explicit `WARN: refusing to arm — no calibration available (run arduino_uno_tuning '\''c'\'' first, or paste BNO055_CAL_BLOB[22] into balance_constants.h)` message instead of silently running with zero offsets.
- **`'F'` force-arm override.** Single uppercase-`F` keystroke bypasses the guard for known-good bench scenarios where the operator deliberately wants to run unblessed. Mirrors the existing `'A'` arm command's keystroke shape; the uppercase-`F` is deliberate to make it harder to fat-finger.
- **`platformio.ini` onboarding-flow comment** at the top of the `[env:arduino_uno_minimal]` block points the reader at `arduino_uno_tuning` as the prerequisite + at `FIRST_SUCCESS_UNO.md` as the canonical walkthrough.

This closes the longest-standing latent safety issue on the Uno tier — that an empty-EEPROM flight build could arm and twitch with garbage offsets.

---

### Phase 2 — Mega doc rewrite (BOOTSTRAP-honest) + Mega backup + AUTO_TUNE excision

#### AO-FIN-02 — Mega operator docs rewritten around BOOTSTRAP

Three doc files rewritten end-to-end so the operator-facing surface matches the as-built Mega state machine:

- `docs/applications/balancing_robot/USER_GUIDE.md` — top-to-bottom rewrite. Removes the **legacy AUTO_TUNE / `'t'` relay-tuner walkthrough** that hasn't been the live code path since BOOTSTRAP landed on 2026-05-18 PM. Frames the user journey as **IDLE → BOOTSTRAP → RUN** (pole-placement K-pulse, then closed-form Kp/Kd, then balance) and tells the operator what to do at each banner.
- `docs/applications/balancing_robot/CALIBRATION_WORKFLOW.md` — rewritten around the BOOTSTRAP K-pulse + mount-offset capture sequence. Adds an **honest bench-validation banner** at the top noting that the Mega has **never balanced successfully on the bench** (last attempt 2026-05-18 PM late twitched in ~1 s); the calibration sequence is therefore "what the code does, audit-verified, not what the operator has seen the bot do."
- `docs/applications/balancing_robot/TROUBLESHOOTING.md` — every AUTO_TUNE / relay-tuner / `RelayFeedbackStrategy` reference removed. New top-level section keyed on the BOOTSTRAP state machine's actual failure modes: K spread across pulses, baseline-window operator-motion poisoning, possibly-aggressive ω_n target vs BNO055 NDOF phase budget (carried over from the 2026-05-18 PM late session). Also adds the BNO055 frozen-pitch trap (fixed by `-D BNO055_NO_EXT_CRYSTAL` per 2026-05-12 bench session) at the top as the #1 mystery to suspect when the bot sees a constant pitch.

The bench-validation banner is repeated near the top of each of the three docs so the operator cannot read one in isolation and come away thinking the bot has been confirmed.

#### AO-FIN-05 — Mega `'B'` photo-backup command + AUTO_TUNE/AutoPIDTuner/RelayFeedbackStrategy dead-code excision

`balance_app.{h,cpp}` + `platformio.ini`:

- **`'B'` command** (Mega-side, parallels Uno's existing `'w'`+`'s'` photo-backup printer) emits a copy-paste-ready hardcode block for the Mega's persisted values: K_motor (post-BOOTSTRAP), the derived Kp/Kd, mount-offset, and the 22-byte BNO055 cal blob. Same `==== PHOTO-BACKUP -- paste into ... ====` envelope shape as the Uno's printer for operator-muscle-memory parity.
- **AUTO_TUNE / AutoPIDTuner / RelayFeedbackStrategy dead-code excision.** The relay-feedback tuner has not been the live tuning path on Mega since BOOTSTRAP landed (2026-05-18 PM). The header/impl files were still in the tree and pulled in by the default `balance_src_filter`. Excision was done via `platformio.ini` **`balance_src_filter`** subtractions (`-<control/auto_pid_tuner.cpp>` + `-<control/tuners/relay_feedback.cpp>` style entries) rather than deleting the files, so the historical code remains read-able under `git log` but is no longer linked into any env. Header declarations for the unused virtual base were left in place to avoid touching unrelated includes; only the link paths were severed.

Rationale for filter-vs-delete: the goal was to make the **build** honest (no dead symbols, no dead code-path branches operators could discover and follow), not to rewrite history. Future cleanup can delete the files when convenient.

---

### Phase 3 — onboarding tree, defensive guards, tier-decision doc

#### AO-FIN-03 — first-success guides + legacy quickstart archive

Two new operator-facing first-success guides:

- `docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md` — the **shortest path from box-opening to a working Mega-tier balance bot**: hardware checklist, `platformio.ini` env, BOOTSTRAP banner script, calibration sequence, BNO055-crystal trap, bench-validation banner.
- `docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md` — the **shortest path to a working Uno-tier balance bot**: SETUP-mode `arduino_uno_tuning` flash, `'c'` cal, `'t'` P→D→I tune, `'w'` save + photo-backup, switch to `arduino_uno_minimal` flight build.

Legacy `docs/guides/QUICK_START.md` and `docs/guides/HARDWARE_SETUP.md` + `docs/getting_started/GETTING_STARTED.md` were archived to `docs/archive/legacy_orientation_framework/` with **redirect notes** at the top of the new locations pointing the reader at the FIRST_SUCCESS_* guides. The old files are kept in the archive so existing inbound links don't 404 and so the historical orientation-framework framing remains discoverable. Old in-tree copies were collapsed to short stubs that redirect to the new location.

#### AO-FIN-07 — four defensive guards

Small, high-confidence, no-behavioural-change-on-happy-path guards added across the control path:

1. **`PID::set_setpoint()` NaN reject.** `pid_controller.cpp` now rejects a NaN setpoint instead of writing it through to the internal state. Mirrors the existing NaN-reject-and-clamp on the gain setters that landed 2026-05-22.
2. **`PositionLoop::update()` NaN guard.** `position_loop.cpp` now rejects a NaN input (`pitch_deg` or `velocity`) and returns the previous output rather than propagating NaN downstream to the inner-loop setpoint. Pairs with #1 — even if a NaN slips through one layer the other layer holds.
3. **`uno_balance_app.cpp` ATOMIC_BLOCK on the last-PWM read.** A read of `last_pwm_` outside the `ATOMIC_BLOCK` was found by the defensive-sweep audit (one missed site beyond the wave-5 cleanup); now atomic. Single-byte race window, low likelihood of corruption on AVR, but no reason not to fix.
4. **`uno_balance_app` `has_cal_blob()` dedup.** Two call sites were independently invoking `tune_storage::has_cal_blob()` (one in the arming path, one in the periodic-status print). Consolidated to a single cached lookup at app-construction time + an explicit refresh on `'w'` / `'c'`. Avoids two paths drifting and prevents the periodic-status print from being a per-frame EEPROM-read hotspot.

None of the four change happy-path behaviour. All four close a specific "what if X is NaN" or "what if two readers race" hole that a future change could otherwise re-open.

#### AO-FIN-06 — CHOOSE_YOUR_TIER.md + HARDWARE_SETUP.md + polarity paragraph fixes

- **New `docs/applications/CHOOSE_YOUR_TIER.md`** — a single-page decision tree the operator hits first: "Do you have a Mega 2560 or an Uno?" → "Mega → universal adaptive stack, see FIRST_SUCCESS_MEGA.md. Uno → memory-constrained tier, hardcoded gains + on-device guided tune, see FIRST_SUCCESS_UNO.md." Captures the 2026-05-26 reframing ("Mega-vs-Uno split is memory-driven, not IMU-driven; both BNO055 and BNO085 valid on either MCU") in one place an operator can read in 60 s.
- **`docs/applications/balancing_robot/HARDWARE_SETUP.md`** — added an **ASCII wiring diagram** for the L298N + BNO055 + battery layout (was prose-only). The ASCII version is robust to Markdown renderers that mishandle Mermaid in `<details>` blocks.
- **Polarity paragraph fixes.** Multiple docs had drifted on motor-polarity convention (some claimed `forward = high PWM on IN1`, others the opposite). Reconciled against the code in `l298n_motor_driver.cpp` and fixed every divergent paragraph to the as-built convention. The wave-3 audit flagged this as the #1 source of "wired everything up, bot just falls over" support-question time.

---

### Phase 4 — navigation polish + this session record + todo update

#### AO-FIN-08 — nav-doc polish (LEAN)

Four index/navigation docs touched, kept deliberately lean — the canonical detailed source for this session is **this record**:

- `docs/README.md` — top-of-tree doc index. Surfaces `CHOOSE_YOUR_TIER.md` + `FIRST_SUCCESS_MEGA.md` + `FIRST_SUCCESS_UNO.md` at the top of the "Start here" section; renames the env references inline (see AO-FIN-01); drops references to archived `getting_started/` and `guides/` legacy quickstarts.
- `docs/applications/INDEX.md` — application catalog index. Promotes `CHOOSE_YOUR_TIER.md` to the top row; updates the Mega + Uno entries to link to their respective FIRST_SUCCESS_* guides.
- `FOLDER_STRUCTURE.md` (repo root for AO) — folder tree updated to reflect: new `docs/archive/legacy_orientation_framework/` archive, new top-level applications/CHOOSE_YOUR_TIER.md, new FIRST_SUCCESS_* guides under each application's folder.
- `docs/findings/INDEX.md` — cross-reference touch-up to surface the small handful of findings cited in this session.

These four files are **intentionally short edits**. The narrative for this session lives here, not in the nav docs.

---

## Verification

| Workitem | Verification | Result |
|---|---|---|
| AO-FIN-04 (Uno arm-guard + `'F'`) | `pio run -e mega_balance && pio run -e arduino_uno_minimal && pio run -e arduino_uno_tuning` | **All three SUCCESS** |
| AO-FIN-05 (Mega `'B'` + AUTO_TUNE excision via balance_src_filter) | Same AO 3-env sweep | **All three SUCCESS** |
| AO-FIN-07 (4 defensive guards) | Same AO 3-env sweep + native suite | **All three SUCCESS**, native **19/19 PASS** |
| AO-FIN-01/02/03/06/08 (doc-only) | n/a (no build impact) | n/a |

Native test suite remained **19/19** throughout — no regressions from any of the three code-touching workitems. Each of the three code-touching workitems (AO-FIN-04, AO-FIN-05, AO-FIN-07) ran the AO 3-env `pio run` sweep at landing time as a per-workitem gate.

**Audit verdict (wave-5 audit agent's report):** all eight workitems landed clean, doc-rot category retired, no new P1/P2 issues opened, code-touching items each verified-green by the sweep. The two code-touching items that crossed audited zones (AO-FIN-04 = arm-guard + force-arm; AO-FIN-05 = `'B'` + balance_src_filter) had their out-of-zone touches flagged + justified in their respective per-agent handoffs.

---

## Honest assessment (from the synthesizer plan)

- **AO is close on code.** The wave-1 security audit surfaced only **P3 nits** (no P0, no P1, no P2 newly opened). The 4 defensive guards in AO-FIN-07 are belt-and-braces, not bug-fixes.
- **The dominant gap was doc rot — now closed.** The plan's own framing was that operators following any prior quickstart hit a wall (wrong env name, missing onboarding tree, AUTO_TUNE walkthroughs for code that no longer existed). This session's deliverable was therefore primarily a documentation honesty pass + a small number of feature/safety polish items, not new functionality.
- **Bench validation remains hardware-gated.** The Mega has **never balanced successfully** on the bench (last attempt 2026-05-18 PM late: twitch-and-fall in ~1 s). The Uno SETUP-mode `'c'` + `'t'` flow has **never been driven on real hardware** (added to the bench gate 2026-05-26; still pending). The bench-validation banners added in AO-FIN-02 surface this honestly to the operator.

---

## State of the working tree

`git status --short` after this session shows the following AO-side files modified or added (per `git diff --stat` reproduced verbatim from this session's run):

**Modified (AO):**
- `auto_orientation/FOLDER_STRUCTURE.md`
- `auto_orientation/QUICKSTART_BALANCE_ROBOT.md`
- `auto_orientation/docs/MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`
- `auto_orientation/docs/PHASE_4_STRUCTURAL_FIXES.md`
- `auto_orientation/docs/README.md`
- `auto_orientation/docs/THEORETICALLY_SOUND_PROGRAM_PLAN.md`
- `auto_orientation/docs/applications/INDEX.md`
- `auto_orientation/docs/applications/balancing_robot/CALIBRATION_WORKFLOW.md`
- `auto_orientation/docs/applications/balancing_robot/HARDWARE_SETUP.md`
- `auto_orientation/docs/applications/balancing_robot/TROUBLESHOOTING.md`
- `auto_orientation/docs/applications/balancing_robot/USER_GUIDE.md`
- `auto_orientation/docs/getting_started/GETTING_STARTED.md` (stub-with-redirect)
- `auto_orientation/docs/guides/HARDWARE_SETUP.md` (stub-with-redirect)
- `auto_orientation/docs/guides/QUICK_START.md` (stub-with-redirect)
- `auto_orientation/docs/implementation/bno055_driver.md`
- `auto_orientation/docs/roadmap.md`
- `auto_orientation/docs/scope.md`
- `auto_orientation/docs/setup/SETUP_AND_STATUS.md`
- `auto_orientation/docs/todo.md`
- `auto_orientation/platformio.ini`
- `auto_orientation/src/applications/balancing_robot/balance_app.{cpp,h}`
- `auto_orientation/src/applications/balancing_robot_uno/main.cpp`
- `auto_orientation/src/applications/balancing_robot_uno/uno_balance_app.{cpp,h}`
- `auto_orientation/src/control/pid_controller.cpp`
- `auto_orientation/src/control/position_loop.cpp`
- `auto_orientation/src/main.cpp`

**Untracked (AO, new):**
- `auto_orientation/docs/applications/CHOOSE_YOUR_TIER.md`
- `auto_orientation/docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md`
- `auto_orientation/docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md`
- `auto_orientation/docs/archive/legacy_orientation_framework/` (archived legacy quickstart trio)

**No commits this session.** Operator does not want a commit yet.

---

## Hardware-deferred items (24 — per the synthesizer plan's `deferred_to_hardware`)

The full list lives in `docs/todo.md` "Bench-hardware-gated" section. Headline items:

1. Bench-validate the Mega balance loop (never balanced; last run twitch-and-fall in ~1 s).
2. Bench-validate the Uno SETUP-mode `'c'` + `'t'` operator UX.
3. Confirm Uno photo-backup hardcode-paste recovery path (wipe EEPROM → paste → reflash flight → resume).
4. Confirm the new Mega `'B'` photo-backup output round-trips (paste → reflash → resume).
5. Bench-confirm the four AO-FIN-07 defensive guards do not false-trip on real hardware.
6. Bench-confirm the AO-FIN-04 Uno refuse-to-arm guard surfaces correctly on an empty-EEPROM flight build.
7. Bench-confirm the AO-FIN-04 Uno `'F'` force-arm override actually overrides.
8. Bench-confirm `-D BNO055_NO_EXT_CRYSTAL` is set on the current bench bot (frozen-pitch trap — per 2026-05-12 record).
9. F-3 `K_VEL` bench observation against the real plant.
10. Regression-baseline `g`-telemetry capture with `tools/plot_bench_run.py`.
11. Real-motor PWM-discovery validation (Gap-3 `stiction_min_pwm`).
12. Retire scope-violation constants once the bot balances long enough to collect derivation data (5 noise-floor-σ-derivable rows now unblocked on the *measurement* side via `noise_floor_estimator`).
13. Validate the K-quality gate against a wider range of bench K spreads.
14. Encoder-driven K verification (Phase 4M.2) — cross-check IMU-K vs encoder-K before BOOTSTRAP exits.
15. Per-wheel CHARACTERISE using encoder pulses (Phase 4M.3).
16. Periodic RUN telemetry (pitch / output / mount-offset / K_motor @ 100 ms) so the bench can see what the loop is doing.
17. Reduce ω_n target from 8 → 5 rad/s in PlantIdentifier if the bench still twitches (BNO055 NDOF phase budget).
18. Phase 4.11a position-containment implementation (cascade outer loop, encoder-primary + IMU-only fallback per the 2026-05-19 design).
19. `mega_orientation` EKF guard + RAM Phase A fixes (~2257 B reclaimable per the diagnosis doc).
20. Operator encoder commands (CPR readout, distance, save calibration).
21. Update Python tuner Kd in `stress` plant preset (still under-tunes per the Kd accuracy caveat).
22. Tuner bench validation (Phase 4U) — sim → real-bot.
23. Uno P1 #6-15 from the 2026-05-19 audit + Uno P2 12 findings (code-quality cleanup).
24. Mega-side BNO085 wiring + BNO085 SH-2 FRS guided cal + `BNO085_CAL_BLOB[<len>]` hardcode-paste site (wave-6 follow-ups from 2026-05-26).

---

## Cross-references

- Operator entry point — [`docs/applications/CHOOSE_YOUR_TIER.md`](../../applications/CHOOSE_YOUR_TIER.md)
- Mega first-success — [`docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md`](../../applications/balancing_robot/FIRST_SUCCESS_MEGA.md)
- Uno first-success — [`docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md`](../../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md)
- Prior session — [`2026-05-26_uno_setup_mode.md`](2026-05-26_uno_setup_mode.md) (wave-6 + wave-8 also uncommitted; this session continues the same working tree)
- Platform framing — [`docs/scope.md` §Platform bifurcation](../../scope.md)
- Bench-gate carry-overs from previous sessions — [`2026-05-22_safety_correctness_docs.md`](2026-05-22_safety_correctness_docs.md), [`2026-05-21_multi_agent_workstream_g_security.md`](2026-05-21_multi_agent_workstream_g_security.md)

---

## Wave 8 — post-execution roadmap items

A small post-execution wave landed on top of `AO-FIN-01..08` (above). Three code/doc deliverables (AO-U1, AO-U5, AO-X2) plus a forward-looking 13-item roadmap planning artifact. Same working-tree, no commits. All AO firmware envs still build clean; native **19/19** preserved.

### AO-U1 — `'!'` one-keystroke macro (cal → tune-entry) on `arduino_uno_tuning`

`src/applications/balancing_robot_uno/main.cpp` (+~50 LOC) + `tuning_session.{cpp,h}` (new `auto_enter_after_cal()` helper). Chains the existing BNO055 guided cal (`'c'`) → tuning entry without forcing the operator to type the second keystroke. Operator still drives the PID stages manually after entry (P → D → I order unchanged from the wave-1 reorder noted in the 2026-05-26 wave-6 session). Builds: `arduino_uno_tuning` 24,692 B (+648 B vs pre-wave-8), `arduino_uno_minimal` 20,884 B (LTO drift only — no new symbols linked into the flight build). Native suite 19/19. Closes the keystroke-count complaint surfaced by reading the wave-1..wave-5 operator UX flow end-to-end, without touching the underlying P → D → I walker.

### AO-U5 — `docs/applications/balancing_robot_uno/CHEATSHEET.md` (NEW)

New ~96-line, ~1-letter-page operator cheatsheet (single bench card for first-time setup). Distilled from `FIRST_SUCCESS_UNO.md` (created in AO-FIN-03) + cross-checks against `tuning_session.cpp` + `calibration_session.cpp` so every printed command actually exists in the firmware. Sits next to the long-form FIRST_SUCCESS guide; the cheatsheet is the bench card, the FIRST_SUCCESS guide remains the narrative walkthrough. No source touched.

### AO-X2 — `tools/validate_photo_backup.py` (NEW)

New Python-3 stdlib-only validator for the photo-backup printer output (the `==== PHOTO-BACKUP -- paste into ... ====` envelope landed in the 2026-05-26 wave-6 session + extended Mega-side as `'B'` in AO-FIN-05). Parses the printed banner, validates the four float ranges + the 22-byte cal-blob, and applies an OCR-confusion-table fix-up pass (`O/0`, `l/1`, `S/5`, `Z/2`, `G/6`, `T/7`, `q/9`) so a human-typed transcription from a photograph round-trips cleanly. On valid input, reformats it for paste back into `balance_constants.h`. **Limitation documented in the file header**: cannot CRC-verify the cal-blob (the blob itself has no inline CRC; the BNO055 datasheet relies on the sensor's accept-or-refuse on `setSensorOffsets`). Tool surfaces this honestly rather than printing a green "validated" without doing the check.

### AO forward-looking roadmap — 13 prioritized initiatives (planning artifact)

Not a code change — captured here as the planning artifact from the prior planning-agent turn. The 13 items span Uno / Mega / Cross-tier, with explicit anti-patterns. Most are **bench-gated long-poles** (cannot be advanced statically; need real hardware in the loop). Item IDs reused in `docs/todo.md` "Roadmap items deferred" section so an operator can pick the next item from either surface.

- **U-2 LED feedback** — visual indicator of SETUP-mode state (cal-in-progress / cal-ok / tune-stage / saved). Bench-gated (needs an LED on the bench bot).
- **U-3 auto-recovery** — soft-recover from a tune-stage timeout instead of dropping back to IDLE; preserves partial progress. Bench-gated.
- **U-4 conservative seed** — start Uno PID stages from a known-conservative-but-not-zero seed (vs current strict zero start), shortening the typical `'t'` session. Bench-gated.
- **M-1 self-healing BOOTSTRAP** — Mega side; retry BOOTSTRAP autonomously when a K-pulse spread fails the quality gate instead of HELDing. Bench-gated long-pole.
- **M-2 retire `'c'`** — Mega side; collapse Mega-side calibration to the BOOTSTRAP entry path (Mega has never used the standalone `'c'` cal on the bench). Static-codeable but blocked on a Mega bench session for confidence.
- **M-3 auto-PWM-discovery** — Mega side; on-bench stiction discovery wired into `K_motor` derivation (Gap-3 carry-over from the 2026-05-21 Workstream G). Bench-gated.
- **M-4 `K_VEL` self-confirmation** — Mega side; the 4M.14-derived velocity gain self-validates on a real plant run (closes the F-3 carry-over from 2026-05-21). Bench-gated.
- **M-5 headless first-success** — Mega side; first successful balance with no operator-attached USB tether. Bench-gated.
- **X-1 LED grammar** — Cross-tier; shared LED-blink grammar so an operator who learned the Uno LED can read the Mega LED. Couples to U-2; bench-gated.
- **X-3 ESP32 portability** — Cross-tier; assess whether the Mega-universal stack ports cleanly to ESP32 (RAM headroom, FreeRTOS contention with `MsTimer2`-equivalent, BNO055 I²C on different pins). Static-research-codeable, but the *value* of the work is bench-gated.
- (3 additional items folded into existing wave-1..wave-5 entries — see `docs/todo.md` for the merged list).

**Anti-pattern list (explicit)**: do not add adaptive code on the Uno path; do not push the Uno-tier toward Mega-tier feature parity (memory-tier principle); do not let the cheatsheet (AO-U5) drift from `FIRST_SUCCESS_UNO.md` (cheatsheet should always be a strict subset of the longer guide).

### Wave-8 verification

| Item | Verification | Result |
|---|---|---|
| AO-U1 (`'!'` macro) | `pio run -e arduino_uno_tuning -e arduino_uno_minimal -e mega_balance` + native | 3/3 SUCCESS, native **19/19** |
| AO-U5 (CHEATSHEET) | n/a (doc-only) | n/a — content cross-checked against firmware |
| AO-X2 (`validate_photo_backup.py`) | `python3 tools/validate_photo_backup.py --self-test` (stdlib-only) | green |
| Forward-looking roadmap | n/a (planning artifact, not a code change) | captured here + in `docs/todo.md` |

No regressions from wave 8 against the wave-1..wave-5 deliverables. The `arduino_uno_tuning` +648 B is accounted-for by the `'!'` macro + `auto_enter_after_cal()` helper; flash headroom remains comfortable (well under the 95 %-flash Uno ceiling).

### Cross-references to wave-1..wave-5

- `'!'` macro chains the wave-3 `'c'` (AO-FIN — cal) + the wave-3 `'t'` (P → D → I tuning) — see [§Phase 1 (above)](#phase-1--naming-consistency--uno-arming-guard) and the 2026-05-26 wave-6 P → I → D → P → D → I reorder.
- CHEATSHEET is a strict subset of the wave-3 [`FIRST_SUCCESS_UNO.md`](../../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md) (AO-FIN-03).
- `validate_photo_backup.py` consumes the printer envelope landed in the 2026-05-26 wave-6 session (`tune_storage::print_photo_backup()`) + extended Mega-side in AO-FIN-05 (`'B'`).
- The forward-looking roadmap respects the wave-1..wave-5 dominant finding ("AO is close on code; doc rot was the gap — now closed") — every bench-gated item explicitly waits on hardware rather than papering over with more static work.

---

## Wave 10 — loose-end fixes + X-1 design doc

**APPEND ONLY** — prior sections (Phases 1-4 + Wave 8) untouched. Three small post-wave-8 deliverables landed on top of the same uncommitted working tree. Same working-tree-only posture; no commits. `mega_balance` still builds clean; native suite preserved at 19/19.

### M-4 — `'g'` command K_VEL self-audit verdict wiring

`src/main.cpp` (around line 792, inside the existing `case 'g'` telemetry print): the Mega-tier `'g'` CSV line now emits a trailing `K_VEL_<verdict>` field (`K_VEL_OK` / `K_VEL_HIGH` / `K_VEL_LOW` / `K_VEL_UNKNOWN`) pulled from the `CascadeAuditVerdict` enum surfaced by the cascade self-audit module. Closes the M-4 roadmap item's static-codeable half (the bench-validation half — F-3 `K_VEL` self-confirmation against the real plant — remains bench-gated). Build: `mega_balance` SUCCESS.

### CHEATSHEET cal-string fix

`docs/applications/balancing_robot_uno/CHEATSHEET.md` (line 26-27 + line 76): operator-facing cal-status string corrected from the wrong `cal=Sssagm` placeholder to the actual 5-char format the firmware prints (`cal=Ssagm`, e.g. `cal=S3231`; goal `cal=S3333`). Cross-checked against `src/applications/balancing_robot_uno/calibration_session.cpp:57-77` (the BNO055 `getStatusString()` shape). No source touched; the cheatsheet now matches what an operator actually sees on the wire.

### X-1 — LED status-grammar design doc (NEW)

`docs/findings/led_status_grammar_2026-05-27.md` (~462 lines, NEW): design contract for the future shared `status_indicator.{h,cpp}` module that both Uno + Mega will consume. **Design doc only — no source landed.** Specifies the blink grammar (cal-in-progress / cal-ok / tune-stage / armed / HELD / fault) so an operator who learned the Uno LED can read the Mega LED without retraining. Explicitly prerequisites the bench-gated U-2 (Uno LED feedback) + M-5 (Mega headless first-success) code drops — picking blink patterns without a real LED on a real bot wastes a session. This wave lands the contract; the code phase stays deferred until U-2/M-5 bench access is available.

### Wave-10 verification

| Item | Verification | Result |
|---|---|---|
| M-4 `K_VEL` wiring | `pio run -e mega_balance` | SUCCESS |
| CHEATSHEET cal-string fix | n/a (doc-only); cross-checked against `calibration_session.cpp` | green |
| X-1 LED grammar doc | n/a (design doc; no code) | n/a |

No regressions. M-4 code phase static-half done; bench-validation half + X-1 implementation phase + U-2/M-5 stay on the bench-gated queue.

---

## Wave 11 — bench validation runbook

**APPEND ONLY** — Waves 1-10 untouched. Single doc-only deliverable landed on top of the uncommitted working tree. No source touched; no builds run; no native suite delta. Purpose: consolidate the **~24 bench-deferred items** that accumulated across the wave-1..wave-10 session records + per-feature procedure docs into a single safe-first runbook the operator can walk top-to-bottom once hardware is on the bench.

### `docs/findings/bench_validation_runbook_2026-05-27.md` (NEW, 273 lines)

Consolidated bench-validation **index** — not a procedure. Each item carries: what to validate, success criterion, why it matters, the existing procedure doc to follow, estimated bench time, and the logbook fields to capture. The per-feature procedure docs (`USER_GUIDE.md`, `CALIBRATION_WORKFLOW.md`, `TROUBLESHOOTING.md`, `FIRST_SUCCESS_*.md`, `HARDWARE_SETUP.md`, `gain_logbook_template.md`) remain canonical detail; this runbook only **indexes and orders** them.

**Safe-first ordering.** Pre-bench checklist → tier choice → SETUP-mode items (cal, photo-backup, refuse-to-arm) → defensive-guard confirmations → balance loop release (the one property only hardware can confirm). Items with the smallest blast radius go first; the historically-failed balance release sits last with the most prerequisites cleared.

**Honest framing carried through.** Top-of-doc bench-validation banner repeats what `USER_GUIDE.md` / `CALIBRATION_WORKFLOW.md` / `TROUBLESHOOTING.md` already say: the Mega bot has never balanced successfully on the bench (last attempt 2026-05-18 PM late: twitch-and-fall in ~1 s); the Uno SETUP-mode `'c'` + `'t'` flow has never been driven on real hardware. The runbook does not paper over either gap.

### Wave-11 verification

| Item | Verification | Result |
|---|---|---|
| Bench-validation runbook (NEW) | doc-only; cross-link rendering verified against existing procedure docs | green |

No source touched. No builds run. AO 3-env build state unchanged from Wave 10. Native suite unchanged at 19/19.

---

## Wave 12 — discoverability + warning recon

**APPEND ONLY** — Waves 1-11 untouched. Two doc-only fix deliverables + a compiler-warning recon (read-only audit, no source touched). No builds invoked beyond the recon-time `pio run` captures used to enumerate warnings. Same uncommitted working-tree posture; no commits.

### Root README refresh (`/home/devel/floppi/README.md`)

Top-of-repo `README.md` refreshed to surface both sub-projects clearly and link straight into each project's canonical entry-point docs. Two-project layout (auto_orientation, flight_controller) is now legible from the root without spelunking; AO's `CHOOSE_YOUR_TIER.md` / `FIRST_SUCCESS_*.md` and FC's `docs/0_quickstart.md` are linked from the root README's "Where to start" section. Pwnstar ↔ err0r sync model called out so an operator picking up either device knows the working-tree convention.

### AO cross-link discoverability fixes

`docs/README.md` + `docs/applications/INDEX.md` + `docs/findings/INDEX.md` cross-link touch-ups to make wave-10/11 deliverables reachable from the top of each index instead of buried mid-list. Specifically: `bench_validation_runbook_2026-05-27.md` and `led_status_grammar_2026-05-27.md` are now surfaced under "Latest findings" at the top of `findings/INDEX.md`; `docs/README.md` "Start here" block now references the runbook for hardware-gated work. No content rewritten — just link placement and short anchor lines so an operator landing on the doc index can find the wave-10/11 outputs in one click.

### AO compiler-warning recon (read-only)

`pio run -e mega_balance -e arduino_uno_minimal -e arduino_uno_tuning` captured + warnings categorized by severity (real-bug-risk vs style/sign-compare/unused-variable noise). Recon is read-only — no source touched, no fixes attempted. Captured for wave-13 triage:

- **Per-project totals**: AO total warnings across the 3 envs = **~47 warnings** (`mega_balance` ~22, `arduino_uno_minimal` ~13, `arduino_uno_tuning` ~12). Many are duplicate header-driven warnings counted per env.
- **real_bug_risk count**: **3 warnings** flagged as real_bug_risk (potential silent correctness/UB rather than style): `-Wsign-compare` in a loop bound that could underflow on empty-array edge, an `-Wmaybe-uninitialized` on a struct field read in an early-return path, and a `-Wunused-result` on an EEPROM-write return-value drop. The remaining ~44 are style/portability nits (sign-compare on `size_t` iterators, unused private members, `-Wmissing-field-initializers` on struct literals, `-Wreorder` on member-init order).

No code changes landed from the recon — wave-12 was strictly capture + categorize. Wave 13 (or whenever bench access frees up static-codeable time) can pick up the 3 real_bug_risk items.

### Wave-12 verification

| Item | Verification | Result |
|---|---|---|
| Root README refresh | doc-only; cross-link rendering verified | green |
| AO discoverability link fixes | doc-only; cross-link rendering verified | green |
| AO compiler-warning recon | read-only `pio run` capture + manual categorization | 3 real_bug_risk identified out of ~47 total |

No source touched (recon was read-only). No new builds beyond the recon-time captures. AO 3-env build state remains green per the wave-1..wave-11 sweep. Native suite unchanged at 19/19.

---

## Wave 13 — stability fixes (3 real_bug_risk warnings + AO cross-link cleanup)

**APPEND ONLY** — Waves 1-12 untouched. Closeout of the wave-12 recon backlog: the 3 real_bug_risk warnings flagged by the wave-12 read-only capture are now resolved in source, plus a small cross-link discoverability touch-up that surfaced the wave-8 CHEATSHEET and the wave-11 bench-validation runbook from each per-application `INDEX.md` instead of relying on the top-level `applications/INDEX.md` alone. Same uncommitted working-tree posture; no commits.

### Real-bug-risk fix 1 — sign-compare loop-underflow guard

A `-Wsign-compare` warning on a descending loop bound (signed loop variable compared against an unsigned `size_t` size) was identified in the wave-12 recon as a real underflow hazard on the empty-array edge: if `size == 0`, the loop ran one iteration with a wrapped-to-`SIZE_MAX` index before the bounds check caught it. Fix replaces the comparison with an explicit `size > 0` guard ahead of the descending walk, mirroring the defensive pattern already used elsewhere in the same TU. Happy-path behaviour unchanged.

### Real-bug-risk fix 2 — maybe-uninitialized struct field on early-return path

A `-Wmaybe-uninitialized` warning on a struct field read in an early-return code path was traced to a path where one branch populated the field but a sibling branch returned a partially-initialized struct that the caller then read. Fix zero-initializes the struct at declaration (`Foo result = {};`) so the early-return path returns a defined value regardless of which branch executed. Cost is one extra zero-write per call — negligible vs the silent-UB hazard.

### Real-bug-risk fix 3 — unused-result on EEPROM-write

A `-Wunused-result` warning on an `EEPROM.update()` / `EEPROM.write()` style return-value drop was identified as a real correctness gap: if the underlying write fails, the caller never sees it and continues as if the persist succeeded. Fix captures the return value, surfaces a `WARN` on failure, and (where the call site can usefully fall back) reverts to the prior in-RAM value rather than silently advancing past a half-committed state. Happy-path behaviour is byte-identical.

### AO cross-link discoverability cleanup

`docs/applications/balancing_robot/INDEX.md` + `docs/applications/balancing_robot_uno/INDEX.md` cross-link touch-ups so wave-8/11 deliverables surface from the **per-application** `INDEX.md` (not just the top-level `applications/INDEX.md`). Specifically: CHEATSHEET + bench-validation runbook now linked from `applications/INDEX.md` top-block; per-application `FIRST_SUCCESS_*` files linked from their respective per-application `INDEX.md` so an operator landing on the Uno page or the Mega page finds the canonical first-success entry-point in one click. No content rewritten — link placement only.

### Wave-13 verification

| Item | Verification | Result |
|---|---|---|
| Sign-compare loop-underflow guard | `pio run` on the affected env + native suite | SUCCESS, native 19/19 |
| Maybe-uninitialized struct fix | `pio run` on the affected env + native suite | SUCCESS, native 19/19 |
| EEPROM-write unused-result | `pio run` on the affected env + native suite | SUCCESS, native 19/19 |
| AO cross-link cleanup | doc-only; cross-link rendering verified | green |

All three AO firmware envs (`mega_balance` + `arduino_uno_minimal` + `arduino_uno_tuning`) build clean post-fix; native suite preserved at 19/19. The 3 real_bug_risk warnings flagged in wave-12 are now closed; the remaining ~44 style/portability nits stay deferred (low-value vs the diff cost).

### Backlog state after wave 13

The static-codeable AO backlog is **genuinely exhausted** as of wave 13. Every remaining roadmap item is either bench-gated (U-2/U-3/U-4, M-1..M-5, X-1 code phase, X-3 ESP32 portability — all require hardware) or awaits an explicit operator-direction unblock. There is no "Wave 14 candidate" follow-up queued; the next coding session needs either bench access or new operator direction.

