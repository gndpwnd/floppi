# Todo: Auto Orientation Framework

**Current phase**: Phase 4 — bifurcated 2026-05-19 into Phase 4M (Mega-universal) and Phase 4U (Uno-minimal)
**Last updated**: 2026-05-27 wave 13 — stability fixes (3 real_bug_risk warnings closed + AO cross-link cleanup). Closes the wave-12 recon backlog: sign-compare loop-underflow guard, maybe-uninitialized struct fix on early-return path, unused-result on EEPROM-write all resolved in source; AO `applications/balancing_robot/INDEX.md` + `applications/balancing_robot_uno/INDEX.md` now link CHEATSHEET + bench-validation runbook from `applications/INDEX.md` and `FIRST_SUCCESS_*` from each per-application `INDEX.md`. All 3 AO firmware envs build clean; native 19/19. **The static-codeable AO backlog is genuinely exhausted as of wave 13** — every remaining roadmap item is bench-gated or awaits explicit operator direction. See "Wave 13" section appended to [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md). Prior: wave 12 — discoverability + warning recon (root `README.md` refresh; AO cross-link discoverability touch-ups to `docs/README.md` + `applications/INDEX.md` + `findings/INDEX.md` so wave-10/11 deliverables surface from the top of each index; read-only compiler-warning recon across `mega_balance` + `arduino_uno_minimal` + `arduino_uno_tuning` totaling ~47 warnings with 3 real_bug_risk flagged — now closed in wave 13). Prior: wave 11 — bench-validation runbook landed (`docs/findings/bench_validation_runbook_2026-05-27.md`, NEW, ~273 lines, doc-only; consolidates 24 bench-deferred items across wave-1..wave-10 into a single safe-first index — see "Wave 11" section appended to [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md)). Prior: wave 10 — loose-end fixes + X-1 design doc (M-4 `'g'`-command `K_VEL_<verdict>` wiring in `src/main.cpp`; CHEATSHEET `cal=` string corrected to actual firmware shape; NEW `docs/findings/led_status_grammar_2026-05-27.md` design contract for the future shared Uno+Mega `status_indicator.{h,cpp}` — see "Wave 10" section appended to [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md)). Prior: AO finishing plan + wave-8 post-execution roadmap items (AO-U1 `'!'` macro on `arduino_uno_tuning`, AO-U5 CHEATSHEET.md, AO-X2 `tools/validate_photo_backup.py`, plus a 13-item forward-looking roadmap captured as a planning artifact — see "Wave 8" section in the same record). Prior: AO finishing plan (`AO-FIN-01..08` across 4 phases — env-name rename, Uno arm-guard + `'F'`, Mega BOOTSTRAP-honest doc rewrite, Mega `'B'` backup + AUTO_TUNE excision, FIRST_SUCCESS_* guides + legacy-quickstart archive, 4 defensive guards, CHOOSE_YOUR_TIER decision tree + ASCII wiring + polarity fixes, lean nav polish — see same record). Prior: 2026-05-26 wave 6 (Uno IMU selection wired at build level — `#error` on USE_BNO085+Uno + cal-blob slot widened to 72 B for future BNO085); prior 2026-05-26 wave: SETUP/OPERATIONAL split + on-Uno BNO055 cal via `'c'` + P→D→I + photo-backup printer — see [archive/session_records/2026-05-26_uno_setup_mode.md](archive/session_records/2026-05-26_uno_setup_mode.md).

### Next session — focus

As of 2026-05-27 wave 13, the static-codeable AO backlog is **genuinely exhausted**. The AO finishing-plan landing is done; waves 8/10/11/12/13 closed the post-execution roadmap, discoverability gaps, and the 3 real_bug_risk compiler warnings from the wave-12 recon. All three AO firmware envs build clean (`mega_balance` + `arduino_uno_minimal` + `arduino_uno_tuning`), native **19/19** PASS, wave-5 audit verdict GREEN, wave-13 3 real_bug_risk fixes verified. **There is no queued static coding work for a wave 14** — every remaining item below is bench-hardware-gated (cannot be advanced statically) or awaits an explicit operator-direction unblock. Next session needs either bench access or new operator direction; resist inventing low-value follow-ups.

The remaining work is **bench-hardware-gated** — it cannot be advanced statically:

1. **Bench-validate the AO-FIN-04 Uno arm-guard + `'F'` force-arm** on a real bot (empty-EEPROM flight build should refuse to arm with the expected `WARN`; `'F'` should override; armed-with-cal flow should be byte-identical to pre-guard behaviour).
2. **Bench-validate the AO-FIN-05 Mega `'B'` photo-backup command** — confirm the printed block round-trips: paste into Mega-side hardcode site → reflash → bot resumes last-known-good gains + mount + cal.
3. **Bench-validate the new Uno operator UX (carried 2026-05-26)** — actually run `'c'` calibration + `'t'` P→D→I tune on the real bot. The flow compiles, audits clean, `'s'`-branch wiring bug fixed, but prompts/order/timing have only been read, not driven. Also confirm the Uno photo-backup hardcode-paste recovery path: wipe EEPROM → paste a photographed block into `balance_constants.h` → reflash `arduino_uno_minimal` → bot resumes last-known-good.
4. **Bench-validate the balance loop** — the one property only hardware can confirm. No successful balance on record (last run 2026-05-18 PM late twitched and fell in ~1 s). Carry over the three open problems: K spread across pulses, operator-motion poisoning the noise baseline, possibly-aggressive pole target vs BNO055 NDOF phase budget.
5. **Bench-confirm the AO-FIN-07 defensive guards + the 2026-05-22 NaN failsafes** behave (PID `set_setpoint` NaN reject + `position_loop.update` NaN guard don't glitch on real transients; kill-switch cuts on a real BNO055 NaN; watchdog-starved cut doesn't false-trip at 200 Hz).
6. **Retire scope-violation constants once balancing** — every remaining row is bench-gated; the 5 noise-floor-dependent rows are now unblocked on the *measurement* side and can be derived from `noise_floor_estimator` σ on the next stable run. See [findings/mega_scope_violation_triage_2026-05-22.md](findings/mega_scope_violation_triage_2026-05-22.md).
7. **F-3 — `K_VEL` bench observation** — validate the 4M.14-derived velocity gain on the real plant.
8. **Regression-baseline capture** — record a known-good `g`-telemetry run with `tools/plot_bench_run.py`.
9. **Real-motor PWM-discovery validation** — verify the Gap-3 `stiction_min_pwm` wiring against real stiction.
10. **Tuner bench validation** (Phase 4U) — 8 s sim → 30 s real-bot. *(Largely superseded by the on-device guided tune — keep until on-device path is bench-validated.)*

### Open questions (operator decision)

- **Add a `'c'` re-cal command to the OPERATIONAL flight build (`arduino_uno_minimal`) too?** Today, field re-calibration requires reflashing `arduino_uno_tuning`. A flight-build `'c'` would let the operator re-cal in place without losing the tuned gains. Cost: a few hundred bytes of flash + one more `calibration_session.cpp` link in the flight env. Flagged 2026-05-26 — see session record.

### Future workstreams (wave-6 follow-ups, 2026-05-26)

- [ ] **Mega-side BNO085 wiring** — no flash constraint on Mega 2560 (256 KB), so USE_BNO085 on `mega_balance` is architecturally supported but not wired up. Selection mechanism (`#ifdef USE_BNO085` / `#else USE_BNO055`) and driver are both already present; what is missing is the `mega_balance` env in `platformio.ini` getting a `-DUSE_BNO085` override path and an end-to-end build run. Tagged as a future workstream.
- [ ] **BNO085 SH-2 FRS guided calibration session** — the existing `calibration_session.{cpp,h}` polls Adafruit_BNO055-specific cal accessors. A BNO085 path needs its own guided wizard that reads the SH-2 DYNAMIC_CALIBRATION FRS record (~36–72 B) and persists via the now-variable-length `tune_storage::save_cal_blob(blob, len)` API. Slot already sized for it; only the in-driver readout + pose script is missing. Tagged as a future workstream.
- [ ] **`BNO085_CAL_BLOB[<len>]` PHOTO-BACKUP hardcode site** — when USE_BNO085 is enabled on a non-Uno target, the operator may want to add a matching `BNO085_CAL_BLOB[<len>]` declaration to `balance_constants.h` alongside the existing `BNO055_CAL_BLOB[22]` hardcode site, so the photo-backup paste-recovery path covers BNO085 the same way it covers BNO055. Low priority — couples to the BNO085 SH-2 cal session above.

See "Completed 2026-05-26", "Completed 2026-05-22" and "Completed 2026-05-21 → Bench-hardware-gated" below for detail.

For phase-level context see [roadmap.md](roadmap.md). For framework bounds see [scope.md](scope.md). For the pivot rationale see operator memory `project_strategic_pivot_2026-05-19.md` and [scope.md §Platform bifurcation](scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal).

---

## Completed 2026-05-27

AO finishing-plan session — 8 workitems (`AO-FIN-01..08`) across 4 execution phases, planned by a schema-validated synthesizer workflow (5 parallel audits → synthesizer). Full session record: [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md). **All uncommitted (per operator instruction).** All AO firmware envs build clean (`mega_balance` + `arduino_uno_minimal` + `arduino_uno_tuning`), native **19/19**. Wave-5 audit: GREEN, no new P1/P2.

- [x] **AO-FIN-01 — env-name rename** — 35 occurrences across 11 doc files + `src/main.cpp` comment substituted `arduino_uno_balancing` → `arduino_uno_minimal`/`arduino_uno_tuning` (context-dependent) and `arduino_mega_balancing` → `mega_balance`. Onboarding docs and `platformio.ini` now agree; `pio run -e <env>` invocations no longer hit `UnknownEnvironment`.
- [x] **AO-FIN-02 — Mega operator docs rewritten around BOOTSTRAP** — `USER_GUIDE.md` + `CALIBRATION_WORKFLOW.md` + `TROUBLESHOOTING.md` rewritten end-to-end. All AUTO_TUNE / `'t'` relay-tuner walkthrough references removed; user journey reframed as IDLE → BOOTSTRAP → RUN (pole-placement K-pulse → closed-form Kp/Kd → balance). Honest bench-validation banner ("Mega has never balanced successfully on the bench") repeated near the top of all three docs.
- [x] **AO-FIN-03 — first-success guides + legacy-quickstart archive** — new `docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md` + `docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md` (shortest-path operator walkthroughs). Legacy `docs/guides/QUICK_START.md` + `docs/guides/HARDWARE_SETUP.md` + `docs/getting_started/GETTING_STARTED.md` archived to `docs/archive/legacy_orientation_framework/` with redirect stubs at the original locations.
- [x] **AO-FIN-04 — Uno refuse-to-arm guard + `'F'` force-arm + onboarding-flow comment** — `uno_balance_app.{cpp,h}` + `main.cpp` + `platformio.ini`. On `arduino_uno_minimal`, app refuses to enter run loop without a calibration blob (EEPROM or compiled-in `BNO055_CAL_BLOB[22]`) and surfaces an explicit operator-facing `WARN`. Uppercase-`F` overrides for known-good bench scenarios. `platformio.ini` `[env:arduino_uno_minimal]` block gained an onboarding-flow comment pointing at `arduino_uno_tuning` + `FIRST_SUCCESS_UNO.md`. AO 3-env build sweep: SUCCESS, native 19/19.
- [x] **AO-FIN-05 — Mega `'B'` photo-backup command + AUTO_TUNE / AutoPIDTuner / RelayFeedbackStrategy dead-code excision** — `balance_app.{cpp,h}` + `platformio.ini`. New `'B'` command emits a copy-paste hardcode block (K_motor, derived Kp/Kd, mount-offset, 22-byte BNO055 cal blob) using the same envelope shape as the Uno's photo-backup printer. AUTO_TUNE / relay-feedback dead code severed from every env link path via `balance_src_filter` subtractions (files preserved under git, no symbols linked into any build). AO 3-env build sweep: SUCCESS, native 19/19.
- [x] **AO-FIN-06 — CHOOSE_YOUR_TIER decision tree + HARDWARE_SETUP ASCII wiring + motor-polarity paragraph reconciliation** — new `docs/applications/CHOOSE_YOUR_TIER.md` (60-second decision tree: Mega → universal adaptive stack; Uno → memory-constrained hardcoded-gains tier). `docs/applications/balancing_robot/HARDWARE_SETUP.md` gained an ASCII wiring diagram (was prose-only). Every divergent motor-polarity paragraph across the doc surface reconciled against the as-built `l298n_motor_driver.cpp` convention.
- [x] **AO-FIN-07 — 4 defensive guards** — `pid_controller.cpp` `set_setpoint()` NaN reject; `position_loop.cpp` `update()` NaN guard (returns previous output on NaN input); `uno_balance_app.cpp` `last_pwm_` ATOMIC_BLOCK fix (missed site from the wave-5 cleanup); `uno_balance_app` `has_cal_blob()` dedup (consolidated two call sites to a cached lookup with explicit refresh on `'w'`/`'c'`). All four are no-behavioural-change on happy path. AO 3-env build sweep: SUCCESS, native 19/19.
- [x] **AO-FIN-08 — lean nav-doc polish** — `docs/README.md` + `docs/applications/INDEX.md` + `FOLDER_STRUCTURE.md` + `docs/findings/INDEX.md` updated to surface `CHOOSE_YOUR_TIER.md` + `FIRST_SUCCESS_MEGA.md` + `FIRST_SUCCESS_UNO.md`, drop references to archived quickstarts, and reflect the new `docs/archive/legacy_orientation_framework/` archive. Intentionally short edits — narrative for the session lives in the session record.
- [x] **AO-U1 — `'!'` one-keystroke macro (cal → tune-entry) on `arduino_uno_tuning`** — `main.cpp` (+~50 LOC) + `tuning_session.{cpp,h}` (new `auto_enter_after_cal()` helper). Chains BNO055 cal → tuning entry so the operator skips the second keystroke; PID stage walker (P → D → I) unchanged downstream. Builds: `arduino_uno_tuning` 24,692 B (+648 B), `arduino_uno_minimal` 20,884 B (LTO drift only). Native 19/19. See [archive/session_records/2026-05-27_ao_finishing.md §Wave 8 — AO-U1](archive/session_records/2026-05-27_ao_finishing.md#wave-8--post-execution-roadmap-items).
- [x] **AO-U5 — `docs/applications/balancing_robot_uno/CHEATSHEET.md` (NEW)** — ~96-line, ~1-letter-page operator cheatsheet (bench card for first-time setup). Distilled from `FIRST_SUCCESS_UNO.md` + source cross-checks against `tuning_session.cpp` + `calibration_session.cpp` so every printed command exists in the firmware. Cheatsheet is the bench card; FIRST_SUCCESS_UNO is the narrative walkthrough. No source touched.
- [x] **AO-X2 — `tools/validate_photo_backup.py` (NEW)** — Python-3 stdlib-only validator for the photo-backup printer envelope. Parses banner, validates 4 float ranges + 22-byte cal-blob, applies OCR-confusion-table fix-up (`O/0`, `l/1`, `S/5`, `Z/2`, `G/6`, `T/7`, `q/9`), and reformats valid input for paste. CRC-verification limitation documented in the file header (the cal-blob has no inline CRC — the BNO055 datasheet relies on accept-or-refuse at `setSensorOffsets`).
- [x] **Wave-10 M-4 `'g'`-command K_VEL_<verdict> wiring** — `src/main.cpp` `case 'g'` telemetry now emits a trailing `K_VEL_OK` / `K_VEL_HIGH` / `K_VEL_LOW` / `K_VEL_UNKNOWN` field pulled from the `CascadeAuditVerdict` enum surfaced by the cascade self-audit module. Closes the static-codeable half of M-4; the bench-validation half (F-3 `K_VEL` self-confirmation against the real plant) remains bench-gated. `mega_balance` build: SUCCESS.
- [x] **Wave-10 CHEATSHEET cal-string fix** — `docs/applications/balancing_robot_uno/CHEATSHEET.md` operator-facing cal-status string corrected from the wrong `cal=Sssagm` placeholder to the actual 5-char `cal=Ssagm` (example `cal=S3231`; goal `cal=S3333`) the firmware prints. Cross-checked against `src/applications/balancing_robot_uno/calibration_session.cpp:57-77` (the BNO055 `getStatusString()` shape). No source touched.
- [x] **Wave-10 X-1 LED status-grammar design doc (NEW)** — `docs/findings/led_status_grammar_2026-05-27.md` (~462 lines, design contract only — no code). Specifies the shared blink grammar (cal-in-progress / cal-ok / tune-stage / armed / HELD / fault) for the future `status_indicator.{h,cpp}` module that both Uno + Mega will consume so an operator can read either LED without retraining. Prerequisites the bench-gated U-2 (Uno LED feedback) + M-5 (Mega headless first-success) code drops; the code phase stays deferred until bench access is available.
- [x] **Wave 11: bench_validation_runbook_2026-05-27.md (consolidates 24 deferred-to-hardware items)** — `docs/findings/bench_validation_runbook_2026-05-27.md` (NEW, ~273 lines, doc-only). Single safe-first index pulling together every bench-deferred item from wave-1..wave-10 session records + per-feature procedure docs. Each entry: what to validate, success criterion, why it matters, existing procedure-doc cross-link, estimated bench time, logbook fields to capture. Ordering puts smallest-blast-radius items (cal, photo-backup, refuse-to-arm) first and the historically-failed balance release last with the most prerequisites cleared. Honest framing carried through (Mega has never balanced; Uno SETUP-mode flow never driven on hardware). No source touched; no builds run.
- [x] **Wave 12: root README refresh + AO discoverability fixes + compiler-warning recon** — `/home/devel/floppi/README.md` refreshed to surface both sub-projects with one-click entry-points (AO `CHOOSE_YOUR_TIER.md` / `FIRST_SUCCESS_*.md`; FC numbered docs) + pwnstar↔err0r sync model called out. AO `docs/README.md` + `docs/applications/INDEX.md` + `docs/findings/INDEX.md` cross-link touch-ups so wave-10/11 deliverables (`bench_validation_runbook_2026-05-27.md`, `led_status_grammar_2026-05-27.md`) surface from the top of each index. Read-only compiler-warning recon across `mega_balance` + `arduino_uno_minimal` + `arduino_uno_tuning` → **~47 total warnings** (`mega_balance` ~22, `arduino_uno_minimal` ~13, `arduino_uno_tuning` ~12); **3 real_bug_risk** identified (sign-compare on loop bound that could underflow on empty-array edge; maybe-uninitialized struct field on early-return; unused-result on EEPROM-write). Remaining ~44 are style/portability nits (sign-compare on `size_t` iterators, unused private members, missing-field-initializers, reorder). No source touched (recon read-only); no new builds beyond capture; AO 3-env build state remains green; native suite unchanged at 19/19. See "Wave 12" section appended to [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md).
- [x] **Wave 13: 3 real_bug_risk warning fixes + AO cross-link cleanup** — closeout of the wave-12 recon backlog. All 3 real_bug_risk warnings flagged in wave 12 are now resolved in source: (1) `-Wsign-compare` loop-underflow guard — explicit `size > 0` check added ahead of the descending walk so the empty-array edge no longer runs one iteration with a wrapped-to-`SIZE_MAX` index; (2) `-Wmaybe-uninitialized` struct fix — `Foo result = {};` zero-init at declaration so the early-return path returns a defined value regardless of which branch executed; (3) `-Wunused-result` on EEPROM-write — return value now captured, `WARN` surfaced on failure, fallback to prior in-RAM value where the callsite supports it. Plus an AO cross-link discoverability cleanup: `docs/applications/balancing_robot/INDEX.md` + `docs/applications/balancing_robot_uno/INDEX.md` now link CHEATSHEET + bench-validation runbook from `applications/INDEX.md` and `FIRST_SUCCESS_*` from each per-application `INDEX.md` so the wave-8/11 deliverables surface from the per-application page (not just the top-level applications index). All 3 AO firmware envs build clean post-fix; native suite preserved at 19/19. See "Wave 13" section appended to [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md).

### Roadmap items deferred (not yet codeable or not yet selected)

13-item forward-looking roadmap captured as a planning artifact alongside the wave-8 deliverables. Most are **bench-gated long-poles** (cannot be advanced statically without a real bot in the loop). Item IDs are stable handles — pick the next item from this list when bench hardware is available, or when an operator decision unblocks the static-codeable subset.

**Uno-tier** (memory-constrained; resist adaptive code per the wave-1 anti-pattern list):

- [ ] **U-2 LED feedback** — visual indicator of SETUP-mode state (cal-in-progress / cal-ok / tune-stage / saved). Couples with X-1 LED grammar. Bench-gated (needs an LED on the bench bot).
- [ ] **U-3 auto-recovery** — soft-recover from a tune-stage timeout instead of dropping back to IDLE; preserves partial progress. Bench-gated (cannot validate the trigger without a real-bot stall).
- [ ] **U-4 conservative seed** — start Uno PID stages from a known-conservative-but-not-zero seed, shortening the typical `'t'` session. Bench-gated (need to verify seed doesn't itself trigger a tip-over).

**Mega-tier** (universal/adaptive stack):

- [ ] **M-1 self-healing BOOTSTRAP** — autonomously retry BOOTSTRAP when a K-pulse spread fails the K-quality gate instead of dropping to HELD. Bench-gated long-pole — depends on observing real failure modes.
- [ ] **M-2 retire `'c'` on Mega** — collapse Mega-side calibration to the BOOTSTRAP entry path (Mega has never used the standalone `'c'` cal on the bench). Static-codeable but blocked on a Mega bench session for confidence.
- [ ] **M-3 auto-PWM-discovery wired into `K_motor`** — on-bench stiction discovery feeding the gain-derivation chain (Gap-3 carry-over from 2026-05-21 Workstream G). Bench-gated.
- [ ] **M-4 `K_VEL` self-confirmation** — 4M.14-derived velocity gain self-validates on a real plant run (closes F-3 carry-over from 2026-05-21). Bench-gated.
- [ ] **M-5 headless first-success** — first successful Mega balance with no operator-attached USB tether (couples to the broader tetherless workflow under Phase 4.8). Bench-gated.

**Cross-tier** (shared across both balancers):

- [x] **X-1 LED grammar — design doc landed wave-10** — see `docs/findings/led_status_grammar_2026-05-27.md`. The shared-grammar **design contract** is now in the tree; the actual `status_indicator.{h,cpp}` code phase remains deferred until U-2 / M-5 bench access is available (picking patterns without a real LED on a real bot wastes a session).
- [ ] **X-3 ESP32 portability assessment** — does the Mega-universal stack port cleanly to ESP32 (RAM headroom, FreeRTOS contention with the `MsTimer2`-equivalent, BNO055 I²C on different pins)? Static-research-codeable, but the *value* of the work is bench-gated (no point porting without an ESP32 + BNO055 bench).

**Anti-pattern reminder**: do not add adaptive code on the Uno path; do not push the Uno-tier toward Mega-tier feature parity (memory-tier principle); do not let the AO-U5 CHEATSHEET drift from `FIRST_SUCCESS_UNO.md` (cheatsheet should always be a strict subset of the longer guide).

### Bench-hardware-gated — next session (NEW 2026-05-27)

- [ ] **Bench-validate the AO-FIN-04 Uno arm-guard + `'F'` force-arm** — empty-EEPROM flight build refuses to arm with the expected `WARN`; `'F'` overrides; armed-with-cal flow is byte-identical to pre-guard.
- [ ] **Bench-validate the AO-FIN-05 Mega `'B'` photo-backup command** — printed block round-trips: paste → reflash → bot resumes last-known-good.
- [ ] **Bench-confirm the AO-FIN-07 defensive guards** — PID NaN reject + `position_loop` NaN guard + ATOMIC_BLOCK fix do not false-trip or glitch on real-hardware transients.

### Static-codeable backlog: EXHAUSTED (as of 2026-05-27 wave 13)

The wave-13 closeout resolved every static-codeable real_bug_risk warning from the wave-12 recon. There is **no queued static work for a wave 14**. Every remaining item across this todo is either:

- **Bench-hardware-gated** (cannot be advanced without a real bot in the loop) — see "Next session — focus" above, "Bench-hardware-gated — next session" subsection, and "Roadmap items deferred" U-2/U-3/U-4, M-1..M-5, X-1 code phase, X-3.
- **Awaiting explicit operator direction** — see "Open questions (operator decision)" above and "Future workstreams (wave-6 follow-ups)".

If a future session has no bench access and no new operator direction, there is nothing static-codeable left to pull. Resist inventing low-value follow-ups; honor the wave-1 anti-pattern reminder.

- [x] ~~**Wave 13 candidate: fix 3 real_bug_risk warnings flagged in wave-12 recon**~~ — **DONE 2026-05-27 wave 13** (see "Completed 2026-05-27" above).

---

## Completed 2026-05-26

Uno-only session — Mega side unchanged. Full session record: [archive/session_records/2026-05-26_uno_setup_mode.md](archive/session_records/2026-05-26_uno_setup_mode.md). **All uncommitted (per operator instruction).**

- [x] **Uno SETUP MODE vs OPERATIONAL MODE split** — `arduino_uno_tuning` (calibrate + tune + photo-backup) vs `arduino_uno_minimal` (lean flight, reads EEPROM). New boot banners in `main.cpp` distinguish the two builds out-of-the-box; on-boot cal restore in both; clear missing-EEPROM `WARN` in the flight build. Builds: `arduino_uno_tuning` 73 % flash (+3058 B), `arduino_uno_minimal` 63 % flash (+3642 B for cal restore + photo-backup linked).
- [x] **On-Uno BNO055 guided calibration** — new `'c'` command (SETUP build only) + new `src/applications/balancing_robot_uno/calibration_session.{cpp,h}`. Runs a pose script, polls BNO055 cal-status bytes, saves the 22-byte blob via new `tune_storage::save_cal_blob`. **The Uno no longer depends on the Mega calibration path.**
- [x] **Guided PID stage order reordered P→I→D → P→D→I** in `tuning_session.{cpp,h}` (enum, transitions, masks, prompts). Rationale: D damps Kp's oscillation first; then a small Ki removes residual drift without re-exciting it. See [applications/balancing_robot_uno/README.md §4.3](applications/balancing_robot_uno/README.md#43--walk-p--d--i-t).
- [x] **Photo-backup printer (value-robustness principle)** — new `tune_storage::print_photo_backup()` emits a copy-paste-ready `==== PHOTO-BACKUP -- paste into balance_constants.h ====` block (4 float lines + 22-byte hex array) on both `'w'` (save) and `'s'` (status). `balance_constants.h` gained PHOTO-BACKUP HARDCODE SITE comment block + `BNO055_CAL_BLOB[22]` declaration (default = 22 × `0xFF` = no seed cal) as canonical recovery path after EEPROM loss.
- [x] **Wave-3 bug fix** — `main.cpp` `'s'` branch wiring corrected to route through `tuningSession.handle_command('s')` (was unreachable). Caught by the verifier agent.
- [x] **Doc updates** — `scope.md` (Mega-vs-Uno capability-tier framing + IMU flexibility + value-robustness), `applications/balancing_robot_uno/README.md` (§3 first-boot, §4 setup-mode walkthrough, §4.7 value-robustness, §5 file tree), `applications/balancing_robot/INDEX.md` (universal-auto framing), `findings/uno_guided_tuning_design_2026-05-20.md` (superseded-in-two-ways banner — P→I→D order + on-Uno cal), `architecture/LEVEL_1_SUBSYSTEMS.md` (Mermaid label fix), `findings/INDEX.md` (cross-reference).
- [x] **Verification** — `arduino_uno_tuning` SUCCESS (73 % flash, 27 % headroom), `arduino_uno_minimal` SUCCESS (63 % flash), `mega_balance` unchanged, native 19/19 PASS. All 8 audit categories GREEN (CRC/EEPROM, calibration_session, photo-backup, banners, P→D→I reorder, on-boot restore, simplicity, flash). No P1/P2 issues.
- [x] **Wave 6 — Uno IMU selection wired at build level (T1)** — `src/applications/balancing_robot_uno/main.cpp` now selects the IMU via `#ifdef USE_BNO085` / `#else default USE_BNO055`. Three `#error` guards: (1) both flags defined; (2) USE_BNO085 on `__AVR_ATmega328P__` (BNO085 lib too big for Uno's 32 KB — memory-tier principle made concrete); (3) implicit default when neither flag is set. `tune_storage` gained a variable-length cal-blob API (`save_cal_blob` / `load_cal_blob` / `has_cal_blob`) — slot at `0x220` reserves 72 B (BNO085 SH-2 worst case), version 0x02 → 0x03 (old 22-byte v2 blobs reject-on-load cleanly). `calibration_session` kept BNO055-specific, double-gated on `defined(USE_BNO055) && defined(UNO_GUIDED_TUNING)`. Builds: `arduino_uno_minimal` +292 B (20634 / 716 RAM), `arduino_uno_tuning` +412 B (23952 / 744 RAM), Mega unchanged. USE_BNO085 on Uno trips `#error` cleanly per design. Out-of-zone edit: `tuning_session.cpp` (2 call sites) consumed the new API. See appended "## Wave 6" section in [archive/session_records/2026-05-26_uno_setup_mode.md](archive/session_records/2026-05-26_uno_setup_mode.md).

### Bench-hardware-gated — next session (NEW 2026-05-26)

- [ ] **Bench-validate the new Uno operator UX** — actually drive `'c'` (calibration) + `'t'` (P→D→I tune) on the real bot; the flow audits clean but the prompts/order/timing have only been read, not driven.
- [ ] **Confirm the photo-backup hardcode-paste recovery path** — wipe EEPROM, paste a photographed block into `balance_constants.h`, reflash `arduino_uno_minimal`, verify the bot resumes last-known-good.

---

## Completed 2026-05-22

Safety/correctness/docs session — no new control phase. Full session record: [archive/session_records/2026-05-22_safety_correctness_docs.md](archive/session_records/2026-05-22_safety_correctness_docs.md). All uncommitted.

- [x] **NaN-safety failsafes across the motor path** — kill-switch NaN guard (`main.cpp`), PID gain/output NaN reject + clamp (`pid_controller.cpp`), mount-offset EEPROM finiteness guard (`main.cpp`), pitch NaN guard at IMU boundary (`balance_app.cpp`), watchdog result wired into `loop()`. Closes the 3 P1 failsafe-gap findings. See [findings/security_audit_2026-05-22.md](findings/security_audit_2026-05-22.md).
- [x] **Noise-floor measurement layer** — new `src/applications/balancing_robot/noise_floor_estimator.h` (Welford online mean+σ, observation-only — consumes nothing yet). The missing measurement layer that doubly-blocked 5 scope-violation rows; those are now unblocked on the *measurement* side. See [findings/mega_scope_violation_triage_2026-05-22.md](findings/mega_scope_violation_triage_2026-05-22.md).
- [x] **Quaternion gimbal-lock PRODUCTION fix** — `quaternion_conversions.cpp` `toEuler` now normalizes before `asin` + clamps `sinp` to `[-1,1]`; fixes a ~0.03° pitch error at exactly ±90°.
- [x] **Uno link regression FIXED** — the quaternion fix introduced an `undefined reference to Quaternion::normalize()` link error in `arduino_uno_minimal`; resolved with a **one-line `platformio.ini` `+<math/quaternion.cpp>` `build_src_filter` addition** (line 161). Both focus builds now compile clean: `arduino_uno_minimal` SUCCESS (54.1% flash), `mega_balance` SUCCESS (16.4% flash). Native suite **22/22**.
- [x] **Uno-minimal P1 cleanup** — `read_fail_count_` (rdfail) sensor-health telemetry + `ATOMIC_BLOCK`-guarded `last_pwm_` access in `uno_balance_app.cpp`.
- [x] **Native test repairs + additions** — 3 broken test files fixed (stray `#endif`), 2 ill-posed gimbal-lock assertions corrected, 2 new tests (`test_noise_floor_estimator.cpp`, `test_pid_nan_safety.cpp`); `tools/build_tests.sh` extended. **22/22 pass** (up from 18/22 mid-session).
- [x] **Docs — ASCII→Mermaid + architecture + as-built reconciliation** — converted the docs corpus to Mermaid (4 invalid diagrams fixed), added `docs/architecture/` LEVEL_0/1/2 + INDEX, and reconciled as-built vs as-designed (pole-placement not AMIGO, no `BootstrapStage` enum, mean-pitch mount estimator, `g=9.81` position loop) in `MASTER_DESIGN.md` + `docs/implementation/`. See [architecture/INDEX.md](architecture/INDEX.md), [findings/autocal_autotune_verification_2026-05-22.md](findings/autocal_autotune_verification_2026-05-22.md).
- [x] **Verification docs** — security audit (0 P0 / 3 P1 all fixed / 6 P2 / 4 P3), auto-cal/auto-tune verification (math-sound + test-covered + compiles, NOT bench-validated), Mega scope-violation triage (0 of 14 statically retireable). Files: [findings/security_audit_2026-05-22.md](findings/security_audit_2026-05-22.md), [findings/autocal_autotune_verification_2026-05-22.md](findings/autocal_autotune_verification_2026-05-22.md), [findings/mega_scope_violation_triage_2026-05-22.md](findings/mega_scope_violation_triage_2026-05-22.md).

### Bench-hardware-gated — next session (needs the physical robot)

Carried forward; bench validation is now the only gate on the balance loop. The scope-violation rows are bench-gated but the measurement infrastructure (noise-floor σ) now exists, so the 5 doubly-blocked rows become derivable on the next stable run.

- [ ] **Bench-validate the balance loop** — no successful balance on record; last run twitched and fell.
- [ ] **Bench-confirm the new NaN failsafes** — kill-switch / PID clamp / watchdog behave on real hardware without false-trips.
- [ ] **Retire scope-violation constants once balancing** — derive from `noise_floor_estimator` σ; see [findings/mega_scope_violation_triage_2026-05-22.md](findings/mega_scope_violation_triage_2026-05-22.md).

---

## Completed 2026-05-20

Today's reconciliation / fix / audit wave landed against the err0r-device merge (commit ec4ef53). Items moved here from the active backlog; preserved in document (not deleted). Full session record: [archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md](archive/session_records/2026-05-20_multi_agent_sync_audits_and_fixes.md).

- [x] **Repo sync ec4ef53** — err0r device's uncommitted + untracked Phase 4M.0/4M.1/4M.12 work merged into origin/main.
- [x] **Phase 4M.0 collision detection — VERIFIED RESTORED.** 27/27 native tests pass; API surface and 3-gate constants confirmed at `balance_app.h:178-182,549,753`, `balance_app.cpp:906,1645-1648`. See [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §1.
- [x] **Phase 4M.1 wheel encoder driver — VERIFIED DONE.** Header + impl + tests landed post-merge. See [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §3.
- [x] **Phase 4M.12 PWM range auto-discovery — VERIFIED DONE.** `PWM_DISCOVERY = 8` enum value present at `balance_app.h:84`; all 7 PWM_DISC_* constants match design. See [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md) §2.
- [x] **Architecture plan landed** — joint scaffolding doc for the in-flight Workstreams (A–F + UNO-A..C + INFRA-A..B + DOC-A). See [findings/architecture_plan_2026-05-20.md](findings/architecture_plan_2026-05-20.md).
- [x] **5 audits + state reconciliation** — 148 findings across doc, code, test, security, build audits, plus a post-sync reconciliation. Files: [findings/audit_documentation_2026-05-20.md](findings/audit_documentation_2026-05-20.md), [findings/audit_code_quality_2026-05-20.md](findings/audit_code_quality_2026-05-20.md), [findings/audit_test_coverage_2026-05-20.md](findings/audit_test_coverage_2026-05-20.md), [findings/audit_security_2026-05-20.md](findings/audit_security_2026-05-20.md), [findings/audit_build_system_2026-05-20.md](findings/audit_build_system_2026-05-20.md), [findings/state_reconciliation_2026-05-20.md](findings/state_reconciliation_2026-05-20.md).
- [x] **Calibration security fixes (4 P1)** — CRC8 → CRC-8-CCITT, int-overflow fix in BNO085 word→byte conversion, length field `uint8_t` → `uint16_t`, version mismatch now refused. `CAL_FORMAT_VERSION` bumped 0x01 → 0x02; old EEPROM blobs are rejected and require re-cal. See [findings/security_fix_calibration_2026-05-20.md](findings/security_fix_calibration_2026-05-20.md).
- [x] **`mega_orientation` RAM fix (F2)** — `F_` Jacobian member dropped from `ExtendedKalmanFilter`; ~1024 B reclaim where EKF instantiated. `mega_orientation` builds at **74.5 % RAM** (6101/8192). See [findings/mega_ram_fix_2026-05-20.md](findings/mega_ram_fix_2026-05-20.md).
- [x] **Documentation hygiene pass** — INDEX.md files updated to include orphaned findings.
- [x] **Tuner workflow verification** — `pio run -e uno_balance` succeeds. The "format mismatch" warning in the state reconciler was **stale**; tuner template / generated header / consumer are all in agreement. See [findings/tuner_format_alignment_2026-05-20.md](findings/tuner_format_alignment_2026-05-20.md).
- [x] **First tuner run (Uno path)** — **YELLOW** result; 8 s balanced in sim vs 30 s target. Future-session work needed (bench validate; iterate plant model where sim under-predicts). (Verify finding file at `findings/tuner_run_result_2026-05-20.md` — may not exist at the time this section was written; check next session.)
- [x] **Cross-project research (AO ↔ flight_controller)** — landed at [/docs/findings/bno_cross_project_2026-05-20.md](../../docs/findings/bno_cross_project_2026-05-20.md). Identifies AO learnings that could transfer to flight_controller and which to defer. See "AO → FC integration" subsection below.
- [x] **Workstream B COMPLETE — AUTO_TUNE dead-code deletion.** `AUTO_TUNE_*` dead code removed from `balance_app.{h,cpp}`; `held_entry_reason_` telemetry added; `test_balance_app.cpp` extended with a STUCK test; new `test_bootstrap_k_preservation.cpp`. Builds verified: `uno_balance` + `mega_balance` SUCCESS. See session record.
- [x] **`json_formatter` 3 P1 bugs FIXED** — NaN/Inf now emit `null`; timestamp `%d` → `%u` — in `json_formatter.cpp`. See [findings/audit_test_coverage_2026-05-20.md](findings/audit_test_coverage_2026-05-20.md).
- [x] **Test coverage landed** — `test_json_formatter.cpp` (22 tests), `test_calibration_storage.cpp` (14 tests), `test_bootstrap_k_preservation.cpp`. See session record.
- [x] **Build fixes landed** — `PWM_DISCOVERY` `-Wswitch` fix, `F()` macro shim in `balance_app.h`, `native_test` `src_filter` exclusions (`gps.cpp` + Arduino-only files now host-compatible/excluded); `gps.cpp` made host-build compatible. See session record.

---

## Completed 2026-05-21

Workstream G bench-tuning support + Phase 4M.14 test coverage + security hardening. Full session record: [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md). All uncommitted.

- [x] **Workstream G codeable items** — G1 telemetry accessors on `BalanceApp`; G2 `g` serial command (CSV telemetry line); G3 `tools/plot_bench_run.py` host plotter; Gap-3 PWM-discovery results wired live into `stiction_min_pwm`; TD-7 windup comment in `position_loop.h`. New docs: [findings/workstream_g_bench_protocol_2026-05-21.md](findings/workstream_g_bench_protocol_2026-05-21.md), [guides/gain_logbook_template.md](guides/gain_logbook_template.md).
- [x] **Phase 4M.14 test coverage** — `test_position_loop` + `test_position_gain_derivation` + new `test_balance_telemetry` wired into `tools/build_tests.sh`. Native suite 17/17 pass (3 Unity tests skipped — host lacks `unity.h`).
- [x] **Two P1 security fixes** — `mounting_calibration.cpp` fake-CRC → real CRC-8-CCITT (mounting-record version bumped, old blobs rejected); `restoreFromEEPROM()` stack-buffer-overflow closed via a `buf_capacity` parameter (16 call sites updated). Plus a P3 `bno085.cpp` null-guard.
- [x] **platformio.ini hygiene** — `default_envs` retargeted off legacy `uno_balance` (~93.6% flash) to `mega_balance`; lean Uno envs de-duped via `[uno_minimal_base]`.
- [x] **Code-quality + doc-drift** — P3 comment polish in `balance_app.cpp` (F-2/F-6/F-8); doc fixes to `scope.md`, `balancing_robot/INDEX.md`, `balancing_robot_uno/README.md`, `tools/README.md`.
- [x] **Build verification** — all 6 AO firmware envs build clean.

### Bench-hardware-gated — next session (needs the physical robot)

These cannot be advanced statically. See the 2026-05-21 session record and [findings/workstream_g_bench_protocol_2026-05-21.md](findings/workstream_g_bench_protocol_2026-05-21.md).

- [ ] **F-3 — `K_VEL` bench observation** — confirm the 4M.14-derived velocity gain behaves on the real plant; the analytical derivation is unvalidated against hardware.
- [ ] **Regression-baseline capture** — record a known-good `g`-telemetry run with `tools/plot_bench_run.py` as the regression reference.
- [ ] **Real-motor PWM-discovery validation** — verify the Gap-3 `stiction_min_pwm` wiring against actual motor stiction on the bench.

---

## Pending — 2026-05-20

Surfaced by today's audits, state reconciliation, and architecture plan. Listed here for next-session pickup.

- [ ] **Tuner bench validation** (Phase 4U) — when hardware available; sim currently produces 8 s balanced vs 30 s target. Iterate plant model where sim under-predicts performance, OR confirm the discrepancy is sim-only and 30 s+ is reachable on hardware.
- [x] **Phase 4M.11 — `e` cmd + EEPROM CPM/radius** — DONE 2026-05-20 (Workstream D). See [findings/phase_4m11_landed_2026-05-20.md](findings/phase_4m11_landed_2026-05-20.md).
- [x] **Phase 4M.2 — K cross-check** — DONE 2026-05-20 (Workstream F). See [findings/phase_4m2_landed_2026-05-20.md](findings/phase_4m2_landed_2026-05-20.md).
- [x] **Phase 4M.13 — velocity / position outer loop** — DONE 2026-05-20 (Workstream F). See [findings/phase_4m13_landed_2026-05-20.md](findings/phase_4m13_landed_2026-05-20.md). Follow-on Phase 4M.14 (outer-loop gain auto-derivation) also DONE 2026-05-20 — see [findings/phase_4m14_landed_2026-05-20.md](findings/phase_4m14_landed_2026-05-20.md).

### AO → FC integration (cross-project bridge, deferred)

Cross-project research landed at [/docs/findings/bno_cross_project_2026-05-20.md](../../docs/findings/bno_cross_project_2026-05-20.md). The doc identifies AO patterns that could transfer to flight_controller (notably `calibration_storage` HAL and the abstract `OrientationSensor` driver pattern) and the items that should remain in AO. **All flagged here as DEFERRED** — link the research doc rather than re-list specifics, so future sessions decide ordering against flight_controller's own roadmap.

- [ ] **AO → FC integration candidates** — review [/docs/findings/bno_cross_project_2026-05-20.md](../../docs/findings/bno_cross_project_2026-05-20.md) when a flight_controller pass is scheduled. Candidates include the `calibration_storage` HAL port and a BNO055/BNO085 driver port; the doc enumerates the trade-offs and a "would over-complicate FC?" filter. Do not pursue until FC roadmap explicitly calls for it.

---

## Strategic pivot — 2026-05-19

The balance-bot reference application splits in two:

- **Mega path** (`src/applications/balancing_robot/`) — home of the universal/adaptive stack (BOOTSTRAP, RLS, collision detection, OnlineMountingEstimator, position containment, wheel encoders). Flash budget is generous; optimize for clarity.
- **Uno path** ([`src/applications/balancing_robot_uno/`](../src/applications/balancing_robot_uno), scaffold landed 2026-05-19 commit c3c0c6b) — minimal single-purpose balancer with hardcoded PID + PWM constants. Constants come from the offline Python brute-force tuner ([`tools/sim/brute_tune.py`](../tools/sim/brute_tune.py), same commit). No on-MCU learning.

The two paths share `src/sensors/`, `src/actuators/`, and `src/math/` — only the application layer diverges.

---

## 2026-05-19 verification — open issues

Source: [findings/verification_2026-05-19.md](findings/verification_2026-05-19.md) (post-commit-c3c0c6b verifier read-only sweep). Three blockers were identified; the two P0s are now **DONE** in working tree (no commit yet); the P1 is now also addressed.

- [x] **P0 — Tuner ↔ Uno consumer constants name/namespace mismatch** — DONE 2026-05-19 PM. Template now emits file-scope `BALANCE_KP/KI/KD + PWM_MIN/MAX + STICTION_PWM + TIP_CUTOFF_DEG + PITCH_SANITY_DEG` matching the consumer. End-to-end workflow `brute_tune.py --output src/applications/balancing_robot_uno/balance_constants.h && pio run -e arduino_uno_minimal` now compiles.
- [x] **P0 — `balance_src_filter` (`+<*>`) pulled Uno sub-app `main.cpp` into other envs** — DONE 2026-05-19 PM. All four envs (`uno_balance`, `mega_balance`, `mega_orientation`, `arduino_uno_minimal`) link cleanly. Mega is unblocked.
- [x] **P1 — Python tuner Kd consistently underestimated ~2.4× vs reference** — DONE 2026-05-19 PM. Random-search Kd now lands at ~62 vs reference 38 (was ~16). Verdict: keep — see [findings/tuner_kd_accuracy_2026-05-19.md](findings/tuner_kd_accuracy_2026-05-19.md). Stress-plant preset still under-tunes Kd — tracked under "Mega path — universal stack" below.

Adjacent pre-existing items the verification surfaced (not new regressions, but worth re-noting):

- `tests/test_held_state_machine` 3/6 fails — investigation file ([findings/investigation_held_state_machine_failure_2026-05-19.md](findings/investigation_held_state_machine_failure_2026-05-19.md)) attributes to stale binary; verifier rebuilt and tests still fail — investigation needs re-examination.
- `pio test -e native_test` runner errors before running anything because legacy `scenario_test_ekf.cpp`, `integration_test_math_pipeline.cpp`, `benchmark_math.cpp` use renamed `ExtendedKalmanFilter` APIs. Either repair, delete, or carve out of the test_filter.

---

## Recently completed (2026-05-19 PM multi-agent landing wave)

All landings are in working tree (no new commits this session — cite working-tree state).

**Mega path — universal stack:**

- [x] **Collision detection re-landed** in `balance_app.{h,cpp}` — 27/27 native tests pass. 3-gate detector: PEAK 12 m/s² single-tick / SUSTAIN 8 m/s² for 3 ticks / KICK 6 m/s² with |gyro| > 200 dps. Constants at `balance_app.h:178-182`, detector loop at `balance_app.cpp:1639-1648`. Matches [findings/research_collision_signature_bno055.md](findings/research_collision_signature_bno055.md) §6 row-for-row.
- [x] **Wheel encoder driver** `src/sensors/wheel_encoder.{h,cpp}` — 17/17 native tests pass. PJRC Encoder library added to `mega_balance` env.
- [x] **Encoder integration into balance_app** — 25 new native tests pass. Pin map in `src/config/pins.h`: `L_ENC_A=18`, `L_ENC_B=19`, `R_ENC_A=2`, `R_ENC_B=3`. Stall detection wired to HELD with `failure_reason=7`.
- [x] **Phase 4M.12 PWM auto-discovery — code** — 49 `PWM_DISC*` references in `balance_app.{h,cpp}`; Mega flash 14.1 % → 14.7 % (+0.6 %). **Native test file PENDING** — sibling verification agent is writing it.
- [x] **`src_filter` duplicate-symbol fix** — all four envs (`uno_balance`, `mega_balance`, `mega_orientation`, `arduino_uno_minimal`) link cleanly. Was P0 in [verification_2026-05-19.md §9](findings/verification_2026-05-19.md).
- [x] **Phase 4.11a position containment — DESIGN** — [findings/phase_4_11a_design_2026-05-19.md](findings/phase_4_11a_design_2026-05-19.md). Encoder-primary outer loop + IMU-only fallback; implementation queued.
- [x] **Mega encoder bench bring-up guide** — [guides/encoder_bench_bringup.md](guides/encoder_bench_bringup.md) (~450 lines) — operator-facing wiring + verification recipe.
- [x] **`mega_orientation` RAM-overflow diagnosis** — [findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md](findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md). Root cause: EKF stub; ~2257 B reclaimable across three Phase A fixes.

**Uno path — minimal balancer:**

- [x] **Uno minimal P0 fixes** — startup delay + `ATOMIC_BLOCK` + `<stdint.h>` include. 17/17 native tests still pass.
- [x] **Uno minimal P1 top-5 fixes** — 33/33 native tests pass. New operator commands `g` (arm-after-abort) and `p` (periodic telemetry on/off).

**Tooling — Python brute-force tuner:**

- [x] **Constants P0 contract fix** — template emits Uno-consumer-compatible file-scope header. End-to-end workflow builds.
- [x] **Tuner Kd accuracy fix** — random-search Kd now lands ~62 vs reference 38 (was ~16). See [findings/tuner_kd_accuracy_2026-05-19.md](findings/tuner_kd_accuracy_2026-05-19.md). Stress-plant preset still under-tunes — tracked under "Mega path" backlog.

---

## Recently completed (2026-05-19 AM multi-agent session)

This session (commit c3c0c6b "save progress") landed the strategic pivot itself plus the Phase 4M/4U scaffolding, a Python brute-force tuner, three audits, three research docs, one verification doc, an operator workflow guide, and a P0/P1 audit-fix sweep on BOOTSTRAP. See `project_strategic_pivot_2026-05-19.md` for the canonical pivot record.

**Strategic pivot + planning:**
- Platform bifurcation (Mega-universal vs Uno-minimal) decided and propagated across scope.md / roadmap.md / todo.md / INDEX.md / UNIVERSAL_BALANCE_BOT_VISION.md.
- Phase 4M plan landed in [MEGA_UNIVERSAL_PLAN.md](MEGA_UNIVERSAL_PLAN.md) (~340 lines).

**Source landings:**
- Uno minimal scaffold: [`src/applications/balancing_robot_uno/`](../src/applications/balancing_robot_uno) — `uno_balance_app.{h,cpp}`, `main.cpp`, `balance_constants.h`. New `arduino_uno_minimal` env builds at 49.7 % flash / 34.7 % RAM. `test_uno_balance_app.cpp` (17 asserts) passes.
- Python brute-force PID/PWM tuner: [`tools/sim/brute_tune.py`](../tools/sim/brute_tune.py) + `balance_constants_template.h.in` — grid + random + evolutionary modes; reproducible same-seed runs; reaches Kp ≈ reference value on random mode (see verification §6).

**P0/P1 audit fixes landed on the universal BOOTSTRAP stack:**
- Gyro torn-read atomicity fix in BOOTSTRAP (P0)
- `plant_id_.reset()` no-overwrite on successful BOOTSTRAP K seed (P0)
- K-quality gate — refuse to push gains when per-pulse K spread is unreasonable (P1)
- Baseline-window operator-motion cap (P1)
- Per-pulse Serial telemetry expanded for bench post-mortem visibility

**Audits delivered (all in `docs/findings/`):**
- [audit_code_quality_balance_stack_2026-05-19.md](findings/audit_code_quality_balance_stack_2026-05-19.md) — drove the 5 BOOTSTRAP fixes above
- [audit_documentation_2026-05-19.md](findings/audit_documentation_2026-05-19.md) — documentation completeness sweep
- Scope-violation re-audit (annotated with platform-bifurcation tags — see scope.md)

**Research findings written (all in `docs/findings/`):**
- [research_collision_signature_bno055.md](findings/research_collision_signature_bno055.md) — 3-gate detector spec (12 g / 8+3-tick / 6+200 dps)
- [research_wheel_encoders_mega_2026-05-19.md](findings/research_wheel_encoders_mega_2026-05-19.md) — Mega quadrature encoder feasibility, pin choices, ISR strategy
- [research_imu_only_position_containment.md](findings/research_imu_only_position_containment.md) — pitch double-integration position estimate as IMU-only fallback

**Verification + investigation + operator guide:**
- [verification_2026-05-19.md](findings/verification_2026-05-19.md) — post-c3c0c6b read-only build / test / tuner verification (1 PASS env, 3 FAIL envs, 175/178 native tests, 2 P0 + 1 P1 open issues)
- [investigation_held_state_machine_failure_2026-05-19.md](findings/investigation_held_state_machine_failure_2026-05-19.md) — root-causing 3/6 failures in `test_held_state_machine` (now contested — see open issues)
- [guides/safe_bench_test_workflow.md](guides/safe_bench_test_workflow.md) — operator workflow for safe bench testing

**Session regression to redo on Mega path:** the collision-detection code in `balance_app.{h,cpp}` was inadvertently reverted by an audit-fix agent late in the session. Scaffolding survives (`bno055::getLinearAccel`, `OrientationSensor::getLinearAccel` virtual, untracked test file) — see Phase 4M.0 below. NOTE: [verification_2026-05-19.md §2](findings/verification_2026-05-19.md) reports the collision API is **present in source** and the 27 collision tests pass — the regression status needs re-confirming after the in-flight wave settles.

## Recently completed (2026-05-18 PM)

See [PHASE2_FLASH_TRIMS_AND_HEURISTICS.md](archive/session_records/2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md).

- Flash budget root-caused (snprintf chain in `BNO055::getStatusString` = 1.3 KB). Freed 1698 B by deleting heavyweight library paths.
- platformio.ini cleaned to 6 envs with build-flag IMU selection. ESP32/Teensy scaffolded.
- scope.md updated with env model, IMU selection, flash strategy.
- Phase 2.1 — measured noise-floor threshold for CHARACTERISE (replaces hardcoded 10 °/s).
- Phase 2.5 — external-motion HELD trigger (motor quiet AND gyro fast ⇒ HELD).
- Phase 2.6 — gain scheduling (linear output scaling inside ±2° soft zone).
- Uno build: 95.9% flash (1336 B free), 70.4% RAM. Flashed and 30 s monitored — zero anomalies.

---

## Mega path — universal stack (Phase 4M)

The universal/adaptive code lives here from 2026-05-19 onward. Flash budget is generous on Mega 2560 (~88 % free); optimize for clarity, not for size. See [roadmap.md §Phase 4M](roadmap.md#phase-4m--mega-only-universal-stack-cleanup) for the phase plan.

### Collision detection (Phase 4M.0) — DONE 2026-05-19 PM

Re-implemented in the 2026-05-19 PM wave. See "Recently completed (2026-05-19 PM ...)" above. Three-gate detector live in `balance_app.{h,cpp}`; 27/27 native tests pass.

### K-quality follow-on (Phase 4M.k)

The P0/P1 fixes landed this session (gyro atomicity, `plant_id_.reset()` no-overwrite, K-quality gate, baseline cap) addressed the most-acute K-spread issues. Remaining work on the Mega path:

- [ ] Validate the K-quality gate against a wider range of bench K spreads
- [ ] Encoder-driven K verification (Phase 4M.2) — cross-check IMU-derived K against encoder-derived K before BOOTSTRAP exits
- [ ] Per-wheel CHARACTERISE on Mega using encoder pulses (Phase 4M.3)
- [ ] Periodic RUN telemetry (pitch / output / mount-offset / K_motor every 100 ms) so the bench can see what the balance loop is doing
- [ ] Reduce ω_n target from 8 → 5 rad/s in PlantIdentifier if bench still twitches (BNO055 NDOF group delay eats phase margin)

### Wheel encoders + position containment (Phase 4M.1, 4M.11, 4M.12, 4.11a)

- [x] **`src/sensors/wheel_encoder.{h,cpp}` driver** — DONE 2026-05-19 PM, 17/17 native tests pass
- [x] **Mega ISR backend using external-interrupt pins (18/19, 2/3)** — DONE 2026-05-19 PM
- [x] **Per-wheel position, velocity, direction** — DONE 2026-05-19 PM
- [x] **Encoder integration into `balance_app`** — DONE 2026-05-19 PM, 25 new tests pass, stall→HELD with `failure_reason=7`
- [x] **Phase 4M.12 PWM auto-discovery code** — DONE 2026-05-19 PM; `-Wswitch` warning fixed 2026-05-20
- [ ] **Phase 4.11a position containment implementation** — design complete in [findings/phase_4_11a_design_2026-05-19.md](findings/phase_4_11a_design_2026-05-19.md); cascade outer loop with encoder-primary + IMU-only fallback (`USE_IMU_ONLY_OUTER_LOOP` runtime gate). Highest-impact remaining item on the Mega path.
- [ ] **mega_orientation EKF guard + RAM Phase A fixes** — diagnosis in [findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md](findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md); ~2257 B reclaimable
- [ ] **Operator encoder commands** — CPR readout, distance, save calibration. Needs serial parser additions in `balance_app.cpp`. Operator-workflow polish; needed before brute-tune-with-bench-PWM workflow can converge.
- [ ] **Update Python tuner Kd in `stress` plant preset** — still under-tunes per [findings/tuner_kd_accuracy_2026-05-19.md](findings/tuner_kd_accuracy_2026-05-19.md) caveat

### Remaining scope-violation rows on Mega

All 14 open rows in [scope.md §Current scope violations — audit](scope.md#current-scope-violations--audit-2026-05-18-updated-pm-evening-phase-410c-landed-re-tagged-2026-05-19-for-platform-bifurcation) are now `[mega]`-tagged. Each becomes addressable once the bot balances long enough on Mega to collect the derivation data. Pick the smallest and most-blocking one first; do NOT iterate on the numeric value.

### Phase 2.7 (deferred to Mega path)

- [ ] Motor-null-space HELD detector with quaternion projection. Replaces hardcoded `a_dev_lpf_ > 6.0f` HELD threshold from the audit. Per [research_motor_null_space_handling_detection.md](findings/research_motor_null_space_handling_detection.md). Mega-only because Uno minimal program does not implement HELD.

---

## Uno path — minimal balancer (Phase 4U)

Single-purpose hardcoded balancer. **No** auto-tune, **no** RLS, **no** BOOTSTRAP, **no** OnlineMountingEstimator. PID + PWM constants come from offline Python brute-force tuning. Reference target: `archive/balancing_robot_reference/SelfBallancingRobot3.ino`. See [roadmap.md §Phase 4U](roadmap.md#phase-4u--uno-minimal-hardcoded-balancer--python-brute-force-tuner).

- [x] **Scaffold [`src/applications/balancing_robot_uno/`](../src/applications/balancing_robot_uno)** — landed 2026-05-19 commit c3c0c6b
  - `uno_balance_app.{h,cpp}` — read pitch, run PID, drive motors (~150 LOC, MsTimer2-driven 200 Hz ISR)
  - Consume generated header `balance_constants.h` (file-scope `BALANCE_KP/KI/KD` + `PWM_MIN/MAX` + `TIP_CUTOFF_DEG` + `PITCH_SANITY_DEG`)
  - Reuses existing `src/sensors/bno055.cpp`, `src/actuators/l298n_motor_driver.cpp`
  - Compile gate `USE_BALANCING_ROBOT_UNO` (mutually exclusive with `USE_BALANCING_ROBOT`)
- [x] **New build env `arduino_uno_minimal`** — landed 2026-05-19, 49.7 % flash / 34.7 % RAM (under 60 % target)
- [x] **Python brute-force tuner in [`tools/sim/brute_tune.py`](../tools/sim/brute_tune.py)** — landed 2026-05-19; wraps `balance_bot_sim.py`; grid + random + evolutionary modes; fitness = time-balanced under disturbance injection
- [x] **Header generator + Uno-side consumer contract** — DONE 2026-05-19 PM (was P0). Template emits the consumer-compatible file-scope shape.
- [x] **Uno P0 fixes** — DONE 2026-05-19 PM. Startup delay + `ATOMIC_BLOCK` + `<stdint.h>` include. 17/17 native tests pass.
- [x] **Uno P1 top-5 fixes** — DONE 2026-05-19 PM. 33/33 native tests pass. New operator commands `g` (arm-after-abort) and `p` (periodic telemetry).
- [ ] **Uno P1 #6-15** — remaining items from [findings/audit_uno_minimal_2026-05-19.md](findings/audit_uno_minimal_2026-05-19.md)
- [ ] **Uno P2 12 findings** — code-quality cleanup; defer until P1 #6-15 done
- [ ] **Bench validation with new gains** — flash, prop upright, release. Pass criterion: balance ≥ 30 s on flat indoor surface.
- [ ] **First brute-force tune run → bench** — full workflow: run `brute_tune.py`, emit header, flash, validate at bench.

Constraint: **resist the urge to add adaptive code on the Uno path.** Hardcoded constants are the design. If a behaviour cannot be reproduced with constants alone, it does not belong in the Uno program.

---

## Documentation follow-ups (2026-05-19 PM)

- [ ] **Update wiring diagrams** in `docs/hardware/` or `docs/build/` with the encoder pin map (`L_ENC_A=18`, `L_ENC_B=19`, `R_ENC_A=2`, `R_ENC_B=3`) — sourced from [guides/encoder_bench_bringup.md](guides/encoder_bench_bringup.md)
- [ ] **Operator usage guide for `g` and `p` commands** (Uno-minimal) — `g` arms after abort, `p` toggles periodic telemetry; landed 2026-05-19 PM as part of Uno P1 batch

---

## Tooling

### Python brute-force PID/PWM tuner

Landed 2026-05-19 in [`tools/sim/brute_tune.py`](../tools/sim/brute_tune.py); see [`tools/sim/README.md`](../tools/sim/README.md). Status of original sub-items:

- [x] Source under `auto_orientation/tools/sim/` — wraps existing `balance_bot_sim.py` plant model
- [x] Search strategy: grid + random + evolutionary; CLI `--mode {grid,random,evolutionary}`
- [x] Search space: Kp / Ki / Kd / pitch-offset / PWM_MAX
- [x] Fitness: time balanced before tip-over under randomized disturbance injection
- [x] Output: header with `constexpr` declarations (template at `tools/sim/balance_constants_template.h.in`)
- [x] CLI flags for plant presets (`--plant {reference,stress,uno_small}`)
- [x] **Tuner Kd off 2.4× from reference** (P1) — DONE 2026-05-19 PM. Kd now ~62 vs reference 38 (was ~16). See [findings/tuner_kd_accuracy_2026-05-19.md](findings/tuner_kd_accuracy_2026-05-19.md).
- [x] **Constants name/namespace contract aligned with Uno consumer** (P0) — DONE 2026-05-19 PM.
- [ ] **Tuner Kd in `stress` plant preset** — still under-tunes per Kd doc caveat; tune the preset's mechanical damping model.

### Other tooling (cross-cutting, no specific phase)

- [ ] `tools/replay_trajectory.py` — feed recorded pitch CSV to firmware over serial (for scenario tests and HIL emulation)
- [ ] `tools/auto_calibrate.py` — host-side magnetometer ellipsoid fit
- [ ] `tools/quaternion_viewer.py` — desktop 3D quaternion viewer (pre-dashboard fallback)
- [ ] `tools/balance_tune_visualizer.py` — auto-PID-tune convergence plot (Mega path)
- [ ] `tools/build_matrix.sh` — wrap `pio run -e <env>` for every env; summarize flash/RAM

---

## Live planning (this session, 2026-05-12 late evening)

Phase 4 implementation + universal auto-tune research and coding session. See [docs/PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md) for the coordinating doc, [docs/archive/session_records/2026-05-12_evening_phase4_landing.md](archive/session_records/2026-05-12_evening_phase4_landing.md) for the full session record.

- [x] Phase A (structural fixes): raw-gyro D-term, real-signal estimator, MsTimer2 ISR, soft-cutoff (items 1-4)
- [x] Phase B (universal auto-tune): scalar RLS plant identifier + closed-form PD-from-K_motor + rate-limited ramp (item 5)
- [x] 5 research agents: inverted-pendulum methods, OSS bot survey, universal zero-knowledge tuning, bootstrap protocol, Osoyoo reference review (all in `findings/`)
- [x] Multi-orientation balance vision + feasibility research (Phase 4.11 designed, not coded)
- [x] All vision docs: UNIVERSAL_BALANCE_BOT_VISION, MINIMIZE_ACCELERATIONS_PHILOSOPHY, MULTI_ORIENTATION_BALANCE_VISION, AUTO_TUNING_REALITY_CHECK
- [x] Build verified: `pio run -e arduino_uno_tuning` at 99.9% flash / 78.4% RAM. 7 PlantIdentifier tests pass.
- [ ] Hardware validation when bot is plugged back in (no bench access this session)
- [ ] Phase 4.10c — full 5-stage bootstrap state machine (designed, deferred until hw drives need)
- [ ] Phase 4.11 — Level 2 multi-orientation (firmware-only arbitrary mounting orientation, ~1 week)

### Prior session — 2026-05-12 framework planning (research only, no code)

### Active research agents (background)

| Agent | Topic | Status |
|-------|-------|--------|
| Online adaptive balance tracking | How to detect & adapt to drift in mounting offset over time (tether, battery, payload) | Running |
| Disturbance compensation | Cable drag, push detection, cascade control, feedforward strategies | Running |
| Tetherless operation strategy | Workflow without USB tether for each MCU class (BLE pendant, on-bot button, WiFi) | Running |
| Browser dashboard architecture | Three.js + LittleFS + WebSocket UI design for ESP32 | Running |

### Completed research agents (findings in `docs/findings/`)

- [x] Self-balancing dynamics + balance-point capture (one-shot)
- [x] Auto-PID tuning algorithm comparison
- [x] BNO055 driver + multi-IMU strategy
- [x] Multi-MCU port strategy (Nano / Mega / Teensy 4.x / ESP32 / ESP32-S3)
- [x] WiFi/telemetry integration design
- [x] MPU6050 + external magnetometer pipeline
- [x] Application catalog (9 applications profiled)
- [x] Test infrastructure expansion (HIL deferred; build-matrix + scenario test prioritized)

---

## Phase 4 — Auto-orientation framework + balancing-robot reference

### 4.1 — Persistent storage HAL (foundational)

- [ ] Create `src/storage/` directory
- [ ] Write `persistent_storage.h` — single API for read/write/commit/clear/capacity
- [ ] Write `persistent_storage_avr.cpp` — wraps `<EEPROM.h>` (existing semantics)
- [ ] Write `persistent_storage_teensy.cpp` — Teensy emulated EEPROM (defer if no hardware yet)
- [ ] Write `persistent_storage_esp32.cpp` — Preferences/NVS (begin + commit)
- [ ] Refactor `src/config/calibration_storage.cpp` to call the HAL (no direct `<EEPROM.h>`)
- [ ] Native unit test: round-trip on AVR backend
- [ ] Document in `docs/implementation/persistent_storage.md`
- [ ] **Fixes Known Issue KI-1**

### 4.2 — Calibration blob sensor tagging

- [ ] Add `CAL_EEPROM_SENSOR_OFFSET` byte to header in `calibration_storage.h`
- [ ] Define sensor IDs: `CAL_SENSOR_BNO085 = 0x85`, `CAL_SENSOR_BNO055 = 0x55`, `CAL_SENSOR_MPU6050_HMC = 0x60`, `CAL_SENSOR_MPU9250 = 0x95`
- [ ] Bump `CAL_FORMAT_VERSION` to `0x02`
- [ ] On `restoreFromEEPROM`, refuse mismatched sensor ID
- [ ] Migration logic: old `0x01` blobs treated as `CAL_SENSOR_BNO085` for backward compatibility
- [ ] Update tests
- [ ] **Fixes Known Issue KI-3**

### 4.3 — Automatic mounting-angle capture (one-shot)

- [ ] Create `src/navigation/mounting_calibration.{h,cpp}`
- [ ] Implement `MountingCalibration` class with `start_capture()`, `is_stable()`, `capture()`, `get_offset_quaternion()`
- [ ] Gyro-stillness detection: 3-sample window, threshold `< 0.5 °/s` on each axis
- [ ] Gravity vector capture: accel low-pass over 200 ms when stable
- [ ] Shortest-arc quaternion computation from observed gravity to `[0, 0, -1]`
- [ ] 24-byte `AutoOrientRecord` serialization (magic + version + q_mount[4] + QC + CRC8)
- [ ] EEPROM persistence via HAL (uses 4.1 + 4.2)
- [ ] Unit tests with synthetic gravity inputs
- [ ] Document in `docs/implementation/mounting_calibration.md`

### 4.4 — Online adaptive mounting-offset tracking (new — from 2026-05-12 user insight)

- [ ] Read [findings/online_adaptive_balance_tracking.md](findings/online_adaptive_balance_tracking.md) (landed 2026-05-12)
- [ ] Implement chosen algorithm (likely sliding-window mean of pitch-when-stable for Mega; 3-state Kalman extension for Teensy/ESP32)
- [ ] Drift confidence field in `OrientationData` or new `MountingCalibrationStatus` struct
- [ ] Safety: lock adaptation during tip-over / windup; refuse beyond ±5° from one-shot reference
- [ ] EEPROM auto-save policy (every N minutes of stable runtime)
- [ ] Scenario tests for: cable-drag injection, step disturbance, simulated battery discharge curve
- [ ] Document in `docs/implementation/online_balance_adaptation.md`

### 4.5 — Generic auto-PID-tuner

- [ ] Create `src/control/` directory
- [ ] Write `pid_controller.{h,cpp}` — generic single-axis PID (port from `PID_v1` API surface, but our own implementation)
- [ ] Write `auto_pid_tuner.h` — `AutoPIDTuner` class + `ITuningStrategy` virtual base + `TuningResult` POD + `SafetyLimits` struct
- [ ] Write `tuners/relay_feedback.cpp` (`USE_TUNER_RELAY`) — Åström-Hägglund 1984 with amplitude limit
- [ ] Write `tuners/twiddle.cpp` (`USE_TUNER_TWIDDLE`) — coordinate descent, simpler / safer fallback
- [ ] Write `tuners/rls_systemid.cpp` (`USE_TUNER_RLS`) — for known-model plants like drones (Phase 7)
- [ ] Unit tests: simulated plant, verify each strategy converges
- [ ] Document in `docs/implementation/auto_pid_tuner.md`

### 4.6 — BNO055 driver

- [ ] Create `src/sensors/bno055.{h,cpp}` implementing `OrientationSensor`
- [ ] Read quaternion via `getQuat()`; derive Euler through existing `quaternion_conversions.h`
- [ ] Map BNO055's separate `getCalibration(&sys, &accel, &gyro, &mag)` to all four `OrientationData` cal fields (fixes KI-2 on the BNO085 side too)
- [ ] Implement `getCalibrationProfile` / `setCalibrationProfile` for the 22-byte BNO055 blob
- [ ] Add `Adafruit BNO055` to lib_deps in relevant build envs
- [ ] Unit tests with a mocked Adafruit_BNO055
- [ ] Document in `docs/implementation/bno055_driver.md`
- [ ] Hardware test: swap BNO085 → BNO055 on Mega, verify orientation streams correctly

### 4.7 — Self-balancing robot reference application — LANDED 2026-05-12

- [x] Create `src/applications/balancing_robot/` directory
- [x] Add `USE_BALANCING_ROBOT` flag to `src/config/mode.h`
- [x] Write `balance_app.{h,cpp}` — state machine: `IDLE → CAPTURE → TUNE → RUN → (HELD / FALLEN soft-cutoff)`
- [x] Write `safety.{h,cpp}` — tilt limit, motor disarm on tip-over, watchdog
- [x] Write `src/actuators/l298n_motor_driver.{h,cpp}` — generic dual-channel PWM with stiction deadband
- [ ] `src/navigation/balance_kalman.{h,cpp}` — 2-state Kalman (pitch + gyro-bias) — deferred, raw-gyro D-term is the current alternative
- [x] New build envs: `arduino_uno_tuning` + `mega_balance` in `platformio.ini`
- [ ] Scenario test: replay `tests/data/balancing_reference_trajectory.csv` — partial; deferred
- [x] Phase 4.7a (state machine), 4.7b (HELD detection), 4.7-soft-cutoff (tip-over auto-recover) all landed

### 4.7c — Multi-axis anomaly detection (designed, not coded)

- [ ] `findings/multi_axis_anomaly_handling_detection.md` — per-axis Welford z-scores with Mahalanobis upgrade path
- [ ] Replaces the current 2-signal HELD detector; ~50 LOC, ~50 B RAM
- [ ] Phase 4.7c work — coding deferred

### 4.10 — Universal zero-knowledge auto-tune — LANDED 2026-05-12

- [x] `findings/dynamic_pwm_accel_learning.md` design (scalar RLS for K_motor)
- [x] `src/control/plant_identifier.{h,cpp}` — RLS + σ-modification projection + MIN_PHI excitation gate
- [x] Closed-form PD-from-K_motor gain mapping (ω_n = 4/ts, ts = 0.5 s, ζ = 0.7)
- [x] Rate-limited gain application (5%/s ramp) inside `BalanceApp::step_run_`
- [x] Freeze gates: 5 s bootstrap window, lateral-gyro > 30 dps, windup_active
- [x] `tests/test_plant_identifier.cpp` — 7 native tests pass, K_est=K_true (0.0% error) on synthetic data
- [x] `s` serial command extended with ADAPT/BOOT tag + K_motor + target gains
- [x] **Phase 4.10c — BOOTSTRAP K_motor pulse-measurement + analytical gain seeding — LANDED 2026-05-18 PM evening** (commit 7a4d27f). Replaces hardcoded Kp/Ki/Kd defaults. 27/27 bootstrap tests pass; Uno flash 92.2% after net +1.1 KB headroom from removing relay tuner.
- [ ] Motor-polarity sanity check at adaptation start (designed, deferred)

### 4.11 — Multi-orientation balance (Level 2 — designed, not coded)

- [ ] `MULTI_ORIENTATION_BALANCE_VISION.md` + `findings/research_multi_orientation_balance_feasibility.md` design
- [ ] `src/control/balance_frame.{h,cpp}` — body→balance frame quaternion from boot-time gravity detection
- [ ] BalanceApp consumes `BalanceFrame::tilt_error()` instead of `pitch_deg` directly
- [ ] EEPROM mount blob: add 2-byte wheel-axis field
- [ ] Estimated: ~1 week, firmware-only, no new hardware. Next priority after hw validation of 4.10.

### 4.8 — Tetherless workflow for balancing robot

- [ ] Read [findings/tetherless_operation_strategy.md](findings/tetherless_operation_strategy.md) (landed 2026-05-12)
- [ ] Wire up `src/sensors/button_input.cpp` as the capture trigger
- [ ] Add LED feedback codes (state machine indicator)
- [ ] Optional: piezo buzzer driver for audible state feedback
- [ ] Battery-low detection + safe-shutdown path
- [ ] Document in `docs/applications/balancing_robot/tetherless_workflow.md`

### 4.9 — Phase 4 documentation

- [ ] Per-module implementation notes in `docs/implementation/`
- [ ] User-facing guide in `docs/applications/balancing_robot/USER_GUIDE.md`
- [ ] Hands-off calibration walkthrough with photos (TBD when hardware is set up)
- [ ] Phase 4 completion summary in `docs/phases/PHASE_4_COMPLETION_SUMMARY.md`

---

## Phase 5 — Multi-MCU port (queued)

See [roadmap.md#phase-5](roadmap.md#phase-5--multi-mcu-port). Highlights:

- [ ] Split `src/config/pins.h` into per-platform files
- [ ] New build envs for Nano, Teensy 4.0/4.1, ESP32, ESP32-S3
- [ ] HAL backends for Teensy emulated-EEPROM, ESP32 Preferences
- [ ] MPU6050 + external magnetometer + Madgwick fusion stack
- [ ] Multi-MCU CI matrix (build everything every push, report flash/RAM)
- [ ] Document any cross-platform pitfalls discovered as `docs/findings/`

---

## Phase 6 — WiFi + browser dashboard (queued, ESP32 family only)

See [roadmap.md#phase-6](roadmap.md#phase-6--wifi-telemetry--browser-dashboard-esp32-family-only). Highlights:

- [ ] `src/network/` module tree
- [ ] WiFi STA + mDNS hostname
- [ ] REST + WebSocket API server
- [ ] Three.js dashboard with calibration wizard, balance-capture, PID-tune visualizer, OTA page
- [ ] LittleFS asset pipeline

---

## Phase 7 — Application catalog expansion (queued)

See [roadmap.md#phase-7](roadmap.md#phase-7--application-catalog-expansion). Top 3 per [findings/application_catalog.md](findings/application_catalog.md):

- [ ] **Multirotor bridge** — I2C slave for flight_controller
- [ ] **Photogrammetry snapshot polish** — wire up existing snapshot recorder to a polished app interface
- [ ] **Camera mount / gimbal** — 2-axis first, 3-axis later
- [ ] Educational kit (Nano + MPU6050) — documentation-heavy
- [ ] Robot arm pose feedback (roll/pitch only — yaw mag-derived is too noisy for ±0.1°)

---

## Cross-cutting (no specific phase)

### Tooling (legacy list — see top-level "Tooling" section above for the 2026-05-19-onward authoritative list)

- [ ] `tools/replay_trajectory.py` — feed recorded pitch CSV to firmware over serial (for scenario tests and HIL emulation)
- [ ] `tools/auto_calibrate.py` — host-side magnetometer ellipsoid fit
- [ ] `tools/quaternion_viewer.py` — desktop 3D quaternion viewer (pre-dashboard fallback)
- [ ] `tools/balance_tune_visualizer.py` — auto-PID-tune convergence plot (Mega path)
- [ ] `tools/build_matrix.sh` — wrap `pio run -e <env>` for every env; summarize flash/RAM

### Documentation cross-cutting

- [ ] Update `FOLDER_STRUCTURE.md` to reflect new docs layout + planned src/ additions (storage/, control/, navigation/mounting_calibration, applications/balancing_robot, actuators/, network/)
- [ ] Add per-application `docs/applications/<app>/` folders as applications are added
- [ ] Migrate flat session summaries in `docs/archive/` into `docs/archive/session_records/` over time

### Known issues (live)

| ID | Description | Fix in phase |
|----|-------------|--------------|
| KI-1 | `EEPROM.h` silently fails to persist on ESP32 | 4.1 |
| KI-2 | BNO085 driver collapses 4 cal accuracies to 1 | 4.6 |
| KI-3 | Calibration blob format lacks sensor tag | 4.2 |
| KI-4 | Doc drift in `roadmap.md` / `todo.md` | ✅ done 2026-05-12 |

---

## Recently completed

- [x] **Phase 4.7 balancing-robot reference app** — state machine, motor driver, safety, BNO055 driver, HELD detection, mounting offset capture + persistence, OnlineMountingEstimator, MsTimer2 hardware ISR, soft-cutoff at ±25°. See [archive/session_records/2026-05-12_evening_phase4_landing.md](archive/session_records/2026-05-12_evening_phase4_landing.md) — 2026-05-12
- [x] **Phase 4.10 universal RLS auto-tune** — scalar RLS plant identifier + closed-form PD-from-K_motor + rate-limited ramp. 7 native tests pass. Build at 99.9% flash. See [PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md) item 5 — 2026-05-12
- [x] **Phase 4.5b auto-PID-tuner** — Åström-Hägglund relay feedback, operator-triggered via `t` — 2026-05-12
- [x] Reorganize `docs/` folder (7 new thematic subfolders, INDEX.md everywhere, root re-indexed) — 2026-05-12
- [x] Dissect & archive `SelfBallancingRobot3.ino` — 2026-05-12
- [x] Rewrite `scope.md` as framework vision — 2026-05-12
- [x] Rewrite `roadmap.md` Phase 4-8 — 2026-05-12
- [x] Phase 3 (EKF sensor fusion) — see `docs/phases/PHASE_3_COMPLETION_SUMMARY.md`
- [x] Phase 2 (GPS integration) — see `docs/phases/PHASE_2_COMPLETION_SUMMARY.md`
- [x] Phase 1 (math foundation) — see `docs/phases/PHASE_1_TEST_RESULTS.md`

---

## Notes & assumptions

- **No git commits in the planning session** (user directive 2026-05-12). All work landed in working tree only.
- **No source code modifications yet** — Phase 4 implementation kicks off in a follow-up session once master design doc is reviewed.
- **Session record is a living document** — will be updated as understanding evolves.
- **Active hardware**: Arduino Mega + BNO085 on bench, plugged in via USB. Self-balancing robot with BNO055 sits assembled but unpowered nearby.

---

*Last updated: 2026-05-27 — AO finishing plan + wave-8 post-execution roadmap items. Wave 8 added: AO-U1 `'!'` one-keystroke macro (BNO055 cal → tune-entry chained) on `arduino_uno_tuning` (`main.cpp` +~50 LOC + `tuning_session.{cpp,h}` `auto_enter_after_cal()` helper; build 24,692 B / +648 B; `arduino_uno_minimal` 20,884 B LTO-drift only; native 19/19); AO-U5 new `docs/applications/balancing_robot_uno/CHEATSHEET.md` (~96-line single-letter-page bench card distilled from FIRST_SUCCESS_UNO with source cross-checks); AO-X2 new `tools/validate_photo_backup.py` (Python-3 stdlib-only photo-backup validator with OCR-confusion fix-up O/0, l/1, S/5, Z/2, G/6, T/7, q/9; CRC-verification limitation documented honestly); forward-looking 13-item roadmap captured as a planning artifact in the session record + a new "Roadmap items deferred" section in this file (U-2 LED feedback, U-3 auto-recovery, U-4 conservative seed, M-1 self-healing BOOTSTRAP, M-2 retire `'c'`, M-3 auto-PWM-discovery, M-4 `K_VEL` self-confirmation, M-5 headless first-success, X-1 LED grammar, X-3 ESP32 portability — most bench-gated). Prior in same session: AO finishing plan (`AO-FIN-01..08` across 4 phases): env-name rename across 11 doc files + `src/main.cpp` comment; Uno refuse-to-arm guard + `'F'` force-arm + `platformio.ini` onboarding-flow comment; Mega `USER_GUIDE` / `CALIBRATION_WORKFLOW` / `TROUBLESHOOTING` rewritten around BOOTSTRAP with honest bench-validation banners and AUTO_TUNE references removed; Mega `'B'` photo-backup command + AUTO_TUNE / AutoPIDTuner / RelayFeedbackStrategy dead-code excision via `balance_src_filter`; new `FIRST_SUCCESS_MEGA.md` + `FIRST_SUCCESS_UNO.md` + legacy quickstarts archived to `docs/archive/legacy_orientation_framework/` with redirect stubs; 4 defensive guards (PID `set_setpoint` NaN reject, `position_loop.update` NaN guard, `uno_balance_app` ATOMIC_BLOCK fix + `has_cal_blob` dedup); new `CHOOSE_YOUR_TIER.md` decision tree + ASCII wiring diagram in `HARDWARE_SETUP.md` + motor-polarity paragraph reconciliation; lean nav polish (`docs/README.md` + `applications/INDEX.md` + `FOLDER_STRUCTURE.md` + `findings/INDEX.md`). AO 3-env build sweep (`mega_balance` + `arduino_uno_minimal` + `arduino_uno_tuning`) GREEN; native 19/19; wave-5 audit GREEN with no new P1/P2. Honest assessment: AO close on code (security audit only P3 nits), dominant gap was doc rot — now closed; bench validation remains hardware-gated (Mega never balanced; Uno setup-mode untested). 24 hardware-deferred items carry forward. Uncommitted in working tree per operator instruction. Session record: [archive/session_records/2026-05-27_ao_finishing.md](archive/session_records/2026-05-27_ao_finishing.md). Prior: 2026-05-26 wave 6 (Uno IMU selection wired at build level — `#ifdef USE_BNO085` / `#else USE_BNO055` + three `#error` guards including hard refusal of USE_BNO085 on AVR ATmega328P; `tune_storage` cal-blob API widened to variable-length 72 B / version 0x03 [old 22-byte v2 blobs reject-on-load cleanly]); prior 2026-05-26: Uno SETUP/OPERATIONAL mode split + on-Uno BNO055 cal via `'c'` + `calibration_session.{cpp,h}` + P→D→I + photo-backup printer; prior 2026-05-22: NaN-safety failsafes, noise-floor layer, quaternion fix, ASCII→Mermaid + `docs/architecture/` LEVEL_0/1/2 + as-built reconciliation, native 22/22. When you finish an item, move it to "Recently completed" with date. When you start a new item, mark it in-progress in `TodoWrite`.*
