# Bench Validation Runbook — 2026-05-27

**Author**: ao-bench-runbook@floppi:1
**Status**: index + ordering — to be walked once the operator has hardware on the bench.
**Working-tree posture**: every item below references existing per-feature procedure docs; this runbook does not duplicate them.

---

## 1. Intro — what this doc is and is not

This is the **consolidated bench-validation runbook**. As of 2026-05-27 the Auto Orientation working tree has accumulated **~24 bench-deferred items** spread across session records, findings, and per-feature procedure docs. None of them can be advanced statically — they all require a real bot, on a real bench, with the operator at the keyboard. Walking them one-at-a-time out of a single index avoids two failure modes that the doc surface has hit before:

- the operator picks a bench-deferred item from one session record and misses three coupled items from an adjacent session record;
- the operator validates a risky item (e.g. balance release) before validating its prerequisites (e.g. IMU cal, refuse-to-arm guard) and either falsely-fails the risky check or damages the bot.

For each item below: **what to validate**, **success criterion**, **why it matters**, the **existing procedure doc** to follow, the **estimated bench time**, and the **logbook fields** to capture. Read this doc once before powering on, then tick items off in order. The per-feature procedure docs remain the canonical detail; this runbook only **indexes and orders** them.

**Honest framing.** The Mega bot has **never balanced successfully on the bench** (last attempt 2026-05-18 PM late: twitch-and-fall in ~1 s). The Uno SETUP-mode `'c'` + `'t'` flow has **never been driven on real hardware** (added to the bench gate 2026-05-26; carried through 2026-05-27). Bench-validation banners surface this in `USER_GUIDE.md`, `CALIBRATION_WORKFLOW.md`, and `TROUBLESHOOTING.md`. Do **not** read a single guide in isolation and come away thinking the bot has been confirmed.

---

## 2. Pre-bench checklist

Before plugging anything in:

- [ ] Bot wired per [`docs/applications/balancing_robot/HARDWARE_SETUP.md`](../applications/balancing_robot/HARDWARE_SETUP.md) (ASCII wiring diagram is the canonical reference; reconciled against `l298n_motor_driver.cpp` for motor polarity in AO-FIN-06).
- [ ] Battery topped — under stationary load the boost rail must hold **≥ 4.7 V** at the L298N logic pin. Measure with a multimeter before flashing anything.
- [ ] **PROPS OFF** (no wheels on the motor shafts) for every item flagged "wheels-off" below. Most of the SETUP-mode items run with the bot in your hands or on a bench cradle.
- [ ] USB tether + serial monitor at **115200 baud** (`pio device monitor -b 115200 | tee bench_YYYY-MM-DD.log`). Tee to a file from the start — half of bench-validation is post-session log review.
- [ ] Multimeter for voltage sanity (logic-pin rail under load; motor-pin rail under stall).
- [ ] D4 button wired to GND on the Mega (used by encoder calibration). Uno tier does not need a button.
- [ ] **`-D BNO055_NO_EXT_CRYSTAL` is set** on the env you are flashing — the BNO055 frozen-pitch trap (per the 2026-05-12 bench session). If pitch ever comes back as a constant non-zero value, this is the **#1 suspect**.
- [ ] Operator within reach of `a` (emergency abort) for **every** powered run. Encoders / kill-switch are backstops, not substitutes.
- [ ] Gain logbook ready ([`docs/guides/gain_logbook_template.md`](../guides/gain_logbook_template.md)) — a bench session with no logbook entry produced no result.

---

## 3. Choose your tier

Decide before flashing. One page: [`docs/applications/CHOOSE_YOUR_TIER.md`](../applications/CHOOSE_YOUR_TIER.md).

- **Mega 2560 in hand** → §5 Mega-universal runbook. Universal adaptive stack, BOOTSTRAP-driven gain derivation, encoders, position outer loop. Long-form first-success path: [`FIRST_SUCCESS_MEGA.md`](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md).
- **Uno R3 in hand** → §4 Uno-minimal runbook. Memory-constrained tier, hardcoded gains + on-device guided tune. Long-form first-success path: [`FIRST_SUCCESS_UNO.md`](../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md). Bench card: [`CHEATSHEET.md`](../applications/balancing_robot_uno/CHEATSHEET.md).
- **Both** → walk the Uno path first. It is intrinsically safer (smaller code surface, no encoders to break, no adaptive loop to mis-derive), and its photo-backup workflow exercises the value-robustness contract that the Mega `'B'` printer mirrors. §6 lists cross-tier items either run satisfies.

---

## 4. UNO-MINIMAL bench runbook

Order is safe-to-first. The operator UX items run with the bot in your hand or on the bench; only the final "fly" item lets the bot stand on its own.

### U1 — Uno setup-mode `'c'` calibration UX (wheels-off)

- **Procedure**: [`FIRST_SUCCESS_UNO.md` §4–5](../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md) + [`CHEATSHEET.md` §3](../applications/balancing_robot_uno/CHEATSHEET.md).
- **Success**: `cal=S3333` reached within 60 s of starting the pose script; `CAL OK: 22-byte blob saved to EEPROM` printed; PHOTO-BACKUP block follows.
- **Why**: First-ever hardware drive of the on-Uno BNO055 guided cal session (landed 2026-05-26, untested on the bench as of 2026-05-27). The pose script, the 2 Hz `cal=` ticker, and the EEPROM save are all unconfirmed in real-time.
- **Bench time**: ~5 min.
- **Logbook**: time-to-S3333, final `cal=` string, whether any pose was unusually slow to converge (G/A/M which?), photo of the PHOTO-BACKUP block.

### U2 — Uno setup-mode `'t'` P→D→I tuning UX (live, wheels-off then on)

- **Procedure**: [`FIRST_SUCCESS_UNO.md` §6–7](../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md) + [`CHEATSHEET.md` §4](../applications/balancing_robot_uno/CHEATSHEET.md).
- **Success**: walked through STAGE_P → STAGE_D → STAGE_I → REVIEW with no crash, no false tip-cutoff trip, prompts arrive in the documented order (P first, **not** P→I→D — the 2026-05-26 wave-1 reorder is one of the things being validated).
- **Why**: First-ever hardware drive of the guided tune. The walker compiles + audits clean, but prompt timing, `+/-/n/b/r/*` keystrokes, and the live PID stage transitions have only been **read**, never **driven**.
- **Bench time**: ~15–25 min depending on operator familiarity.
- **Logbook**: which stage was hardest to judge, did `*` coarse/fine toggle behave, did `r` reset cleanly, final Kp/Kd/Ki at REVIEW entry.

### U3 — Uno `'!'` macro end-to-end (cal → tune → save → reboot → fly)

- **Procedure**: send `'!'` on `arduino_uno_tuning`; macro chains `'c'` → tuning entry. Then continue manually through P/D/I/REVIEW and `'w'`. Reboot, reflash `arduino_uno_minimal`, send `'g'` to arm. Background: [2026-05-27 session record §Wave 8 — AO-U1](../archive/session_records/2026-05-27_ao_finishing.md#wave-8--post-execution-roadmap-items).
- **Success**: macro chains cleanly (no operator second-keystroke needed between `c` end and `t` entry); tuned gains survive the reflash and produce a clean `ARMED` on the OPERATIONAL build.
- **Why**: AO-U1 (landed 2026-05-27) is the most operator-visible UX polish of the wave-8 work. Confirms the macro doesn't drop a keystroke / skip a state.
- **Bench time**: ~20 min (run after U1+U2 once familiar).
- **Logbook**: did the macro require any manual second keystroke, did REVIEW reach the same state as in U2.

### U4 — Refuse-to-arm guard verification (AO-FIN-04, no cal)

- **Procedure**: with EEPROM cleared **and** `BNO055_CAL_BLOB[22]` left as the default `0xFF` sentinel in `balance_constants.h`, flash `arduino_uno_minimal` and observe boot.
- **Success**: explicit `WARN: refusing to arm — no calibration available (run arduino_uno_tuning 'c' first, or paste BNO055_CAL_BLOB[22] into balance_constants.h)` printed; bot does **not** enter the run loop; `arm() rejected: cal missing.` on any `'g'` attempt.
- **Why**: This closes the longest-standing latent Uno safety issue — an empty-EEPROM flight build silently running with garbage offsets and twitching on release. Reference: 2026-05-27 session record AO-FIN-04.
- **Bench time**: ~10 min (includes an EEPROM-wipe step).
- **Logbook**: exact `WARN` text observed, behaviour of `'g'` after the WARN.

### U5 — `'F'` force-arm override verification (AO-FIN-04)

- **Procedure**: with the same empty-EEPROM state as U4, send uppercase `F` instead of `g`.
- **Success**: arm proceeds despite missing cal; operator-acknowledged "drift expected" path; bot does what it would have done before AO-FIN-04 landed (twitch on release is *expected* here — that's why the guard exists).
- **Why**: The deliberate uppercase-F design (vs lowercase `f`) is to prevent fat-fingering; bench confirmation that the keystroke shape is right, and that the override actually overrides.
- **Bench time**: ~5 min (run immediately after U4).
- **Logbook**: did override fire on first uppercase-F, did the bot behave as pre-AO-FIN-04.

### U6 — Photo-backup recovery test (wipe EEPROM, paste hardcoded values, reflash, fly)

- **Procedure**: from the U2 PHOTO-BACKUP block (or a photo of it), paste `BALANCE_KP/KI/KD`, `PITCH_OFFSET_DEG`, `BNO055_CAL_BLOB[22]` into the PHOTO-BACKUP HARDCODE SITE in `balance_constants.h`. Wipe EEPROM. Reflash `arduino_uno_minimal`. Arm and fly.
- **Success**: bot arms cleanly (no refuse-to-arm WARN — the compiled-in cal blob satisfies the guard), behaves indistinguishably from the EEPROM-resident path. Optional polish: validate the PHOTO-BACKUP block against [`tools/validate_photo_backup.py`](../../tools/validate_photo_backup.py) (AO-X2) before pasting to catch any OCR-typo (`O/0`, `l/1`, etc.).
- **Why**: This is the **value-robustness** contract — every persisted value is photographable, every persisted value can survive an EEPROM wipe. Without this validated, the "photograph the scrollback" instruction across CHEATSHEET / FIRST_SUCCESS is unfounded.
- **Bench time**: ~30 min (includes typing or OCR of the photo).
- **Logbook**: how the cal blob was transcribed (typed / OCR / pasted from a `tee`d log), validator output, behaviour parity vs U3.

### U7 — Defensive guard observation (no false-trips on Uno path)

- **Procedure**: during U2 (live tuning, will see real transients) watch for any spurious PID-NaN-reject lines, any spurious `position_loop` NaN-guard returns, any `last_pwm_` glitch. Reference: AO-FIN-07 (2026-05-27).
- **Success**: zero spurious defensive-guard trips during a clean U2 session.
- **Why**: AO-FIN-07 added four belt-and-braces guards. They are no-op on happy path **in theory**; bench is the only place to confirm they are also no-op on real noisy hardware.
- **Bench time**: piggybacks on U2 (no extra session).
- **Logbook**: yes/no any guard fired during U2; if yes, the surrounding telemetry context.

### U8 — IMU drift over time (cross-tier, run on whichever bot is built first)

- **Procedure**: with bot stationary and disarmed (or held in cradle), capture `s` once a minute for 30 minutes. Reference: cross-tier §6.
- **Success**: pitch reading drifts < 0.5° over 30 min (BNO055 NDOF spec; treat anything > 1° as a flag).
- **Why**: Long-term IMU stability is unmeasured on this bot. Couples to the BNO055 frozen-pitch trap (pre-bench checklist) — a frozen pitch will *look* like zero drift, so cross-check `cal=` is still ticking too.
- **Bench time**: 30 min (runs in the background while doing other items).
- **Logbook**: pitch min/max/range over the window, did `cal=` digits move at all.

---

## 5. MEGA-UNIVERSAL bench runbook

Order is safe-to-first. The first balance attempt is the **hardest, riskiest, highest-value** item — it is **not first** here because almost every other item is a prerequisite. Walking M1–M6 before M7 dramatically increases the odds of M7 telling you something useful.

### M1 — `'c'` mount-capture validation (wheels-off, bot held upright)

- **Procedure**: [`FIRST_SUCCESS_MEGA.md` §4](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) — `c` command with bot propped perfectly upright.
- **Success**: `[state] -> CAP` immediately, `[state] -> BOOT` after 2 s, `sv m=<value>` on the next BOOTSTRAP→IDLE transition (or fresh capture). `<value>` is small (a few degrees max for a well-mounted IMU).
- **Why**: Mount offset feeds the BOOTSTRAP pole-placement derivation. A bad mount = bad K = bad balance. First gate.
- **Bench time**: ~10 min.
- **Logbook**: captured mount value, σ_pitch during the 2 s window (if surfaced in telemetry).

### M2 — `'e'` encoder calibration (bot rolled by hand, wheels on)

- **Procedure**: [`FIRST_SUCCESS_MEGA.md` §5](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) + [`encoder_bench_bringup.md`](../guides/encoder_bench_bringup.md) §3, §6.
- **Success**: `enc_cal: L=<cpm> R=<cpm> r=<radius> saved`; CPM values within ±5 % left-vs-right; hand-rolled 1.000 m reads back within ±5 %.
- **Why**: Encoder geometry feeds the 4M.14 position-gain derivation. A bad radius forces the cascade onto the conservative fallback gains (`posgains_failure_reason_=9`) — BOOTSTRAP still succeeds but the run is "degraded session" not "derived session." See [`workstream_g_bench_protocol_2026-05-21.md` §1.3](workstream_g_bench_protocol_2026-05-21.md).
- **Bench time**: ~15 min.
- **Logbook**: L CPM, R CPM, radius, ±% on the hand-roll distance check.

### M3 — `'p'` PWM-discovery validation (BOT LIFTED OFF GROUND)

- **Procedure**: [`FIRST_SUCCESS_MEGA.md` §6](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) — `p` command, bot **off the ground** for the entire ramp.
- **Success**: `sv pd min=<min> max=<max>` printed; `<min>` per-motor matches your eye-test for "this is the lowest PWM that produces movement" (real stiction floor); no `pd fail r=<4|8|9>`.
- **Why**: First-ever real-motor validation of the Phase 4M.12 auto-discovery (Gap-3 carry-over from 2026-05-21 Workstream G). The Gap-3 `stiction_min_pwm` wiring is what makes the derived K_motor honest about what the actuator can do.
- **Bench time**: ~10 min.
- **Logbook**: discovered min/max per motor, did values match the eye-test, any `pd fail` reasons.

### M4 — `'b'` BOOTSTRAP validation per failure_reason

- **Procedure**: [`FIRST_SUCCESS_MEGA.md` §7](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) + [`USER_GUIDE.md` §5.1](../applications/balancing_robot/USER_GUIDE.md) (the failure_reason 0–8 table).
- **Success**: a clean BOOTSTRAP with `failure_reason=0`; gains derived (read back via `g`); transition to RUN. If a failure_reason fires, follow the per-reason remedy in `TROUBLESHOOTING.md` and re-run; the goal is *one* clean BOOTSTRAP from which RUN can launch.
- **Why**: BOOTSTRAP is the pole-placement K-pulse derivation that produces every adaptive gain. The two open problems from 2026-05-18 PM late carry over: K spread across pulses, baseline-window operator-motion poisoning. M3 (real stiction) feeds in here; if M3 produced surprising values, expect M4 to surface them as K-spread.
- **Bench time**: 15–45 min (depends on how many failure_reasons fire and need remediation).
- **Logbook**: per attempt — which failure_reason (if any), the `bs#0..3` per-pulse K values, final derived `k_pos`/`k_vel`/`pos_leak` if successful, `posgains_failure_reason_`.

### M5 — K_VEL bench observation via `'g'` self-audit verdict (F-3 / M-4)

- **Procedure**: after a clean M4, capture `g` once per second for 30 s into `tee` log. The wave-10 `'g'`-line now emits a trailing `K_VEL_OK` / `K_VEL_HIGH` / `K_VEL_LOW` / `K_VEL_UNKNOWN` verdict (`src/main.cpp` ~line 792, see 2026-05-27 session record Wave 10). Cross-reference with [`workstream_g_bench_protocol_2026-05-21.md` §6 (F-3)](workstream_g_bench_protocol_2026-05-21.md).
- **Success**: the captured 30 s of `g` lines show **majority `K_VEL_OK`** verdict. F-3 verdict written into the logbook: Confirmed / Suspect / Inconclusive.
- **Why**: Closes the bench-validation half of the M-4 roadmap item. The 4M.14 derivation produced `K_VEL ≈ 11.68` (~3.9× the old 4M.13 hand-pick of 3.0); the analytical justification has been on the doc surface since 2026-05-20 — this is when it gets confirmed on real hardware.
- **Bench time**: ~10 min (post-M4).
- **Logbook**: per-second verdict distribution, F-3 verdict (Confirmed/Suspect/Inconclusive), the derived K_VEL value for this bot.

### M6 — Photo-backup recovery via `'B'` (AO-FIN-05)

- **Procedure**: after M5, send `'B'`. Photograph the printed PHOTO-BACKUP block. Validate it through [`tools/validate_photo_backup.py`](../../tools/validate_photo_backup.py) (AO-X2). Wipe EEPROM, paste hardcoded values into the Mega-side hardcode site (envelope shape matches the Uno's per AO-FIN-05), reflash, confirm bot resumes last-known-good gains + mount + cal.
- **Success**: post-reflash behaviour byte-identical to the M5 state (same K, same mount, same cal). Bot is ready to balance without re-walking M1–M4.
- **Why**: First-ever validation of the Mega-side photo-backup path. The Uno's photo-backup landed 2026-05-26; this is the Mega analogue from 2026-05-27 — it has never round-tripped on real hardware.
- **Bench time**: ~45 min (includes EEPROM-wipe and reflash).
- **Logbook**: validator output, behaviour parity vs M5 (any drift in K / mount / cal across the round-trip).

### M7 — First Mega balance attempt (THE bring-up gate)

- **Procedure**: [`FIRST_SUCCESS_MEGA.md` §8–9](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) — once `s` reports `RUN`, slowly lower the bot to release; watch for sustained balance vs. immediate tip. Capture full `g` telemetry stream.
- **Success criterion**: **any** balance ≥ 5 s without operator intervention is a brand-new bench result for this project. ≥ 30 s is the roadmap §Phase 4M target. The 2026-05-18 PM late "twitch-and-fall in ~1 s" is the regression baseline.
- **Why**: **No Mega balance has ever succeeded.** Every prior item in §5 was a prerequisite for this attempt being interpretable. Three known open problems carry over from 2026-05-18 PM late: (a) K spread across pulses, (b) baseline-window operator-motion poisoning, (c) possibly-aggressive ω_n target (8 rad/s) vs BNO055 NDOF phase budget. If M7 twitches-and-falls, the next move is **not** "run M7 again" — it is to capture the `g` log, escalate against the three open problems, and consider reducing ω_n from 8 → 5 rad/s in `PlantIdentifier` (carried over from 2026-05-27 todo §item 17).
- **Bench time**: 30–60 min (release attempts, log capture, post-mortem).
- **Logbook**: duration of longest sustained balance, fall-mode (immediate tip / oscillation grows / drift away), full `g` log file path, which of the three open problems looks most suspect, the established pass-bands B-1..B-7 from [`workstream_g_bench_protocol_2026-05-21.md` §5](workstream_g_bench_protocol_2026-05-21.md) (first good run *is* the band).

### M8 — Periodic mount-offset save EEPROM wear test

- **Procedure**: only if M7 produces a sustained balance — let the bot run for an extended session (target 10+ min). Confirm the periodic-mount-offset-save 60 s window + threshold-gate fires at the expected rate. Reference: 2026-05-27 deferred_to_hardware item.
- **Success**: EEPROM writes count is bounded and proportional to *real* mount drift, not the 60 s window itself (the threshold gate should suppress no-change writes).
- **Why**: Bench duration test for the EEPROM wear-leveling assumption. Cannot run this until M7 produces a long enough sustained balance to make the test meaningful.
- **Bench time**: 15+ min (after M7).
- **Logbook**: EEPROM save count over the window, observed mount drift, save-count-vs-real-drift ratio.

### M9 — NaN failsafe observation on real BNO055 (AO-FIN-07 + 2026-05-22 NaN failsafes)

- **Procedure**: throughout M4–M7 watch for spurious PID-NaN-reject, position_loop NaN-guard returns, watchdog cuts, kill-switch cuts. Reference: AO-FIN-07 (2026-05-27) + 2026-05-22 NaN failsafe sweep.
- **Success**: zero spurious NaN-failsafe trips on real BNO055 over the full M4–M7 sequence; if a real NaN does appear (e.g. BNO055 hot-loop hang), kill-switch cuts within 5 s.
- **Why**: AO-FIN-07 and the 2026-05-22 NaN failsafes are belt-and-braces guards. Bench is the only place to confirm they are also no-op on happy-path real-hardware and *do* trip when the BNO055 actually misbehaves.
- **Bench time**: piggybacks on M4–M7.
- **Logbook**: yes/no any NaN failsafe fired, whether the trigger was real-NaN or false-trip, surrounding telemetry.

### M10 — 14 Mega scope-violation hardcoded constants (retirement, not validation)

- **Procedure**: only after M7+M8 succeed and have produced a stable enough run to derive σ from `noise_floor_estimator` (see [`mega_scope_violation_triage_2026-05-22.md`](mega_scope_violation_triage_2026-05-22.md)). For the 5 noise-floor-σ-derivable rows in that triage doc: collect σ from a clean stable run, derive the constants, replace the hardcodes. The other 9 rows remain bench-deferred without further unblock.
- **Success**: 5 of the 14 scope-violation rows retired (replaced by derived values); native suite still passes; next bench run shows no regression vs M7.
- **Why**: The measurement side (`noise_floor_estimator`) landed 2026-05-22; the derivation has been blocked on a stable enough run to collect σ. M7 is what unblocks it.
- **Bench time**: ~2 hr (post-M7+M8, includes coding the derivations and a regression bench run).
- **Logbook**: σ values from the clean run, the 5 rows retired with derived values, regression check vs M7 logbook entry.

---

## 6. Cross-tier items

These bench tests apply to both tiers. Run once on whichever bot is being built first; capture the result in the logbook and cross-link.

- **BNO055 cal blob round-trip**: validated implicitly by U6 (Uno) and M6 (Mega). If only one tier is being built, run the photo-backup recovery on that tier; the round-trip property is the same.
- **Photo-backup workflow (PHOTO-BACKUP envelope shape parity)**: confirm the printed envelope on Mega (`'B'`, AO-FIN-05) and Uno (`'w'` / `'s'`, 2026-05-26 wave-6) are visually similar enough that operator muscle memory transfers. Single eyeball check.
- **IMU drift over time**: see U8.
- **BNO055 frozen-pitch trap (`-D BNO055_NO_EXT_CRYSTAL`)**: covered in pre-bench checklist; bench confirmation is "pitch responds to chassis tilt during U1/M1."

---

## 7. Abort criteria — when to STOP

Stop the bench session immediately if **any** of the following:

- props go on by mistake at any point before M7 (the only step where the bot is intended to be a free-standing balanced system; for U1–U7, U8, M1–M6 the bot stays in your hand or on a cradle);
- motors get hot to the touch (over-current; check stiction discovery / PWM-discovery output);
- IMU NaN persists > 5 s without the kill-switch firing (failsafe failure — escalate, do not retry);
- any smoke, any burning smell, any unexpected current draw on the multimeter;
- the bot is moving in a way you don't understand and `a` is not responding within 1 s.

Power-off, log the abort condition, escalate to a fresh session with the issue surfaced in `TROUBLESHOOTING.md` before re-attempting.

---

## 8. Post-bench — what to record

- **Gain logbook**: fill in [`docs/guides/gain_logbook_template.md`](../guides/gain_logbook_template.md). One row per item walked. The B-1..B-7 acceptance bands from [`workstream_g_bench_protocol_2026-05-21.md` §5](workstream_g_bench_protocol_2026-05-21.md) are *established* by the first good run; subsequent runs are judged against the established bands.
- **Session record**: create `docs/archive/session_records/YYYY-MM-DD_bench_<session_id>.md` modelled on [`2026-05-27_ao_finishing.md`](../archive/session_records/2026-05-27_ao_finishing.md). One per bench session, even if the session was a 5-minute abort — partial results are still results.
- **Telemetry logs**: keep the full `tee`d serial log for each item (named `bench_YYYY-MM-DD_<item_id>.log`). The non-`G`-tag lines carry state-transition + failure_reason context that the `g`-only plot misses.
- **Update `docs/todo.md`** "Bench-hardware-gated" section: tick off the items that landed; carry forward the items that didn't; **be honest** about items that were attempted-but-inconclusive vs items that were skipped.

---

## 9. Troubleshooting

Per-symptom pointers live in [`docs/applications/balancing_robot/TROUBLESHOOTING.md`](../applications/balancing_robot/TROUBLESHOOTING.md) (Mega) and [`docs/applications/balancing_robot_uno/README.md` §6](../applications/balancing_robot_uno/README.md) (Uno). The Mega troubleshooting doc was rewritten BOOTSTRAP-honest in AO-FIN-02 (2026-05-27) — every AUTO_TUNE / relay-tuner reference removed; failure modes keyed on the actual state machine.

The BNO055 frozen-pitch trap (constant pitch reading, won't respond to chassis tilt) is the **#1 mystery to suspect** when something is wrong with orientation; the fix is `-D BNO055_NO_EXT_CRYSTAL` per the 2026-05-12 bench session.

---

## 10. Cross-references

This runbook leans on:

- [`docs/applications/CHOOSE_YOUR_TIER.md`](../applications/CHOOSE_YOUR_TIER.md) — tier decision
- [`docs/applications/balancing_robot/HARDWARE_SETUP.md`](../applications/balancing_robot/HARDWARE_SETUP.md) — wiring (ASCII diagram per AO-FIN-06)
- [`docs/applications/balancing_robot/FIRST_SUCCESS_MEGA.md`](../applications/balancing_robot/FIRST_SUCCESS_MEGA.md) — Mega first-success walkthrough
- [`docs/applications/balancing_robot/USER_GUIDE.md`](../applications/balancing_robot/USER_GUIDE.md) — Mega state-machine reference (BOOTSTRAP-honest rewrite per AO-FIN-02)
- [`docs/applications/balancing_robot/CALIBRATION_WORKFLOW.md`](../applications/balancing_robot/CALIBRATION_WORKFLOW.md) — Mega calibration sequence (AO-FIN-02)
- [`docs/applications/balancing_robot/TROUBLESHOOTING.md`](../applications/balancing_robot/TROUBLESHOOTING.md) — Mega per-failure-mode remedies (AO-FIN-02)
- [`docs/applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md`](../applications/balancing_robot_uno/FIRST_SUCCESS_UNO.md) — Uno first-success walkthrough
- [`docs/applications/balancing_robot_uno/CHEATSHEET.md`](../applications/balancing_robot_uno/CHEATSHEET.md) — Uno bench card (AO-U5, cal-string fix wave-10)
- [`docs/applications/balancing_robot_uno/README.md`](../applications/balancing_robot_uno/README.md) — Uno reference (§3 first-boot, §4 setup-mode walkthrough, §4.7 value-robustness, §6 troubleshooting)
- [`docs/guides/encoder_bench_bringup.md`](../guides/encoder_bench_bringup.md) — Mega encoder calibration prerequisites
- [`docs/guides/gain_logbook_template.md`](../guides/gain_logbook_template.md) — the logbook this runbook feeds
- [`docs/findings/workstream_g_bench_protocol_2026-05-21.md`](workstream_g_bench_protocol_2026-05-21.md) — `'g'` telemetry capture, acceptance bands B-1..B-7, F-3 K_VEL verdict
- [`docs/findings/mega_scope_violation_triage_2026-05-22.md`](mega_scope_violation_triage_2026-05-22.md) — the 14-row hardcoded-constants triage (M10)
- [`docs/findings/phase_4m14_landed_2026-05-20.md`](phase_4m14_landed_2026-05-20.md) — auto-derivation nominal-chassis K_VEL ≈ 11.68 reference
- [`docs/findings/balance_failure_diagnosis_2026-05-12.md`](balance_failure_diagnosis_2026-05-12.md) — BNO055 frozen-pitch trap
- [`docs/archive/session_records/2026-05-27_ao_finishing.md`](../archive/session_records/2026-05-27_ao_finishing.md) — AO-FIN-04 (arm-guard + `'F'`), AO-FIN-05 (`'B'` photo-backup), AO-FIN-07 (4 defensive guards), AO-U1 (`'!'` macro), AO-X2 (`validate_photo_backup.py`), Wave 10 M-4 (`K_VEL_<verdict>`)
- [`docs/archive/session_records/2026-05-26_uno_setup_mode.md`](../archive/session_records/2026-05-26_uno_setup_mode.md) — Uno SETUP/OPERATIONAL split, `'c'` cal, P→D→I tune, photo-backup printer
- [`docs/archive/session_records/2026-05-22_safety_correctness_docs.md`](../archive/session_records/2026-05-22_safety_correctness_docs.md) — NaN failsafes
- [`tools/validate_photo_backup.py`](../../tools/validate_photo_backup.py) — AO-X2 validator with OCR-confusion-table fixups
- [`docs/todo.md`](../todo.md) — Bench-hardware-gated section (source list this runbook indexes)

---

*This is the index + ordering, not the procedure detail. Per-feature procedure docs remain canonical. Walk the items in order; tick them off in the gain logbook; record the session per §8. The Mega balance attempt M7 is the project's bring-up gate — every prior item exists to make M7 interpretable.*
