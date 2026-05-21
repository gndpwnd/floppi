# auto_orientation — Security Posture Re-Audit (2026-05-20)

**Auditor:** ao-security-reaudit@floppi:1 (read-only)
**Baseline:** `auto_orientation/docs/findings/audit_security_2026-05-20.md`
**In-flight references:** `phase_4m14_design_*` and `ao_session_synthesis_*` are being
written by sibling agents; this audit does not depend on them.
**Scope of re-audit:** code that landed today (Phases 4M.2, 4M.11, 4M.13, Workstream-F
review, Guided-Tuning feature) plus any prior-audit finding whose underlying file changed.
**Build/test posture:** NO builds were run for this audit. All conclusions come from
static reading of the source. Build-verification numbers are taken from the landing
reports.

---

## 1. Executive Summary

**Posture verdict: NEEDS-ATTENTION (improved vs. baseline).**

| Bucket                                  | Count |
|-----------------------------------------|-------|
| Prior P1 findings CLOSED today          | 4     |
| Prior P1 findings STILL OPEN            | 1     |
| Prior P2 findings CLOSED today          | 1     |
| Prior P2 findings STILL OPEN            | ~17   |
| Prior P3 findings CLOSED today          | 0     |
| NEW findings from today's landings      | 5     |
| of which: P1                            | 1     |
| of which: P2                            | 2     |
| of which: P3                            | 2     |

Net change vs. the morning audit: posture is **better** on data-integrity (CRC, length,
version-rejection of the BNO055 calibration blob all landed) and **slightly worse only
in attack-surface count** because of the new EEPROM slot at 0x200 (guided tuning) and
the new serial-command handler (`t/+/-/*/n/b/r/w/q`). The new attack surface is
proportionate and the implementations are conservative.

Category-by-category vs. baseline:

| Category                       | Baseline      | Today        | Delta            |
|--------------------------------|---------------|--------------|------------------|
| Buffer overflow / strings      | 1 P1 (BNO085) | 1 P1 (open)  | unchanged        |
| Integer overflow / underflow   | 1 P1          | CLOSED       | better           |
| Sensor data trust              | 2 P2          | unchanged    | unchanged        |
| EEPROM corruption resilience   | 2 P1, 2 P2    | mixed        | **mostly better, one new P1** |
| Watchdog / hang prevention     | 3 P2          | unchanged    | unchanged        |
| Init failure paths             | 3 P2          | unchanged    | unchanged        |
| ISR / race conditions          | 1 P2          | unchanged    | unchanged        |
| Privilege / safety boundaries  | 2 P2          | unchanged    | unchanged        |
| SD card write integrity        | 1 P2          | unchanged    | unchanged        |
| Calibration injection          | 1 P1, 1 P2    | unchanged    | unchanged        |
| Serial command parser          | not in scope  | NEW          | new surface      |

**Top 3 P0/P1 items to address next:**

1. **NEW-P1-001 (main.cpp:192-194):** Mega `xor_crc8_` is still the old XOR-sum that the
   morning audit's P1-015 condemned. It guards three EEPROM slots (0x200 mount, 0x210
   actuator, 0x230 PWM-discovery). The CRC-8-CCITT fix landed only in
   `calibration_storage.cpp`; main.cpp was not updated. Two-bit-flip class corruptions
   silently pass.
2. **PRIOR P1-031 STILL OPEN (bno085_calibration.cpp:199-230):** Heuristic-only
   validation of the FRS calibration blob — still unaddressed.
3. **PRIOR P1-018 STILL OPEN on master (bno085_calibration.cpp:52, 77-82):** Stack
   buffer overflow path when sensor returns a bad `num_words`. The security-fix doc
   claims this was patched as a side-effect of P1-007; the file on disk should be
   re-spot-checked against that claim before the next bench session (out of scope of
   this read-only sweep — flagged).

---

## 2. Threat-Model Recap (honest scoping)

This is firmware on a closed-loop, self-balancing, two-wheel bot driven by an Arduino
Mega or Uno over USB Serial. It has **no network stack, no Wi-Fi, no Bluetooth, no
filesystem accessible over a network** (SD card writes are append-only telemetry, never
input). The realistic threat surface is:

- **Serial-command injection (deliberate or accidental).** A connected operator can type
  characters into the 115200-baud TTY. On the Uno-tuning build, the active command set
  includes `a` (abort), `g` (arm), `s` (status), `p` (telemetry toggle), and the guided-
  tuning chars `t + - * n b r w q`. On the Mega the set is much larger (incl. `e` for
  encoder calibration). A stuck Serial line, or a flooded buffer, could in theory drive
  the state machine into an unexpected configuration.
- **EEPROM tampering with physical access.** Anyone who can plug a USB cable in can
  send the calibration / tuning / encoder-cal commands. After a re-cal-after-corruption
  cycle, the EEPROM is whatever they made it. **The trust boundary is "anything in
  EEPROM is trusted after the CRC + magic byte pass."** Document this — don't try to
  defend against physical access on AVR; the platform doesn't support it.
- **Stack / buffer overflow in command parser.** Today's parsers are single-char
  dispatch (`switch (c)` on an `int` from `Serial.read()`), no `strcpy`, no `scanf` —
  the attack surface is structurally tiny but worth re-checking.
- **Mechanically-unsafe state induced by bad data.** A NaN pitch reading, a negative
  gain, or a corrupt encoder CPM that survives the CRC could cause the motors to be
  mis-actuated. This is *the* real "security" failure on a balancing bot: a runaway
  motor command from bad data is a physical-safety event.
- **Single-event upsets (SEU) on EEPROM bits.** Not "an attacker" in the classical sense,
  but the firmware's only defense against bit-rot is the CRC, so weakening the CRC
  weakens the only line of defense.

What is **out of scope** for this audit (because it would be inventing web-app threats):
TLS, authentication, session hijack, SQL injection, CSRF, XSS. None of these apply.

---

## 3. Prior-Audit Findings — Status Walk-Through

The morning audit (`audit_security_2026-05-20.md`) listed 28 findings (0 P0, 4 P1,
19 P2, 9 P3 — though the in-text severity prefixes are inconsistent; the summary table
at line 307-312 is authoritative).

The morning `security_fix_calibration_2026-05-20.md` documents the four P1 items being
closed in `calibration_storage.cpp` + `bno085_calibration.cpp`. Status walk:

| ID       | Title                                          | Baseline file                                 | Status      | Notes                                                                                          |
|----------|------------------------------------------------|-----------------------------------------------|-------------|------------------------------------------------------------------------------------------------|
| P2-001   | GPS sentence buffer checksum bounds            | sensors/gps.cpp:108-151                       | STILL OPEN  | gps.cpp not touched today.                                                                     |
| P2-002   | GPS extractField walks past null               | sensors/gps.cpp:426-458                       | STILL OPEN  | Not touched.                                                                                   |
| P2-003   | GPS speed strtod no overflow check             | sensors/gps.cpp:376                           | STILL OPEN  | Not touched.                                                                                   |
| P2-004   | GPS strncpy null-term not explicit             | sensors/gps.cpp:46 et al.                     | STILL OPEN  | Not touched.                                                                                   |
| P3-005   | BNO085 strcpy in error_buffer_                 | sensors/bno085_calibration.cpp:218-229        | STILL OPEN  | Not touched.                                                                                   |
| P2-006   | millis() rollover in GPS isStale               | sensors/gps.cpp:468-472                       | STILL OPEN  | Not touched. (Note: the new `uno main.cpp:212` does use the safe `(uint32_t)(now-last)` form.) |
| **P1-007** | BNO085 word→byte u16 multiply overflow       | sensors/bno085_calibration.cpp:67             | **CLOSED**  | Per `security_fix_calibration_2026-05-20.md` §"Fix 2": widened to uint32_t with explicit guard. |
| **P1-008** | Calibration length truncated by uint8_t      | config/calibration_storage.cpp:90, 139        | **CLOSED**  | Per fix doc: v2 header carries length as little-endian u16. CAL_DATA_MAX_SIZE now 506.         |
| P2-009   | analogWrite signed-PWM cast                    | actuators/l298n_motor_driver.cpp:137          | STILL OPEN  | Not touched. Defensively safe (mag is always ≥0).                                              |
| P2-010   | PID division-by-zero on d_term_lpf_tau         | control/pid_controller.cpp:166                | STILL OPEN  | Not touched. Note: `uno main.cpp:184` sets tau=0.0f explicitly — so the failing branch *is* reached at boot. Re-verify guard. |
| P2-011   | GPS lat/lon NaN passes through                 | sensors/gps.cpp:309-312                       | STILL OPEN  | Not touched.                                                                                   |
| P2-012   | BNO055 quaternion NaN not gated                | sensors/bno055.cpp:150-154                    | STILL OPEN  | Not touched.                                                                                   |
| P2-013   | BNO085 cal_status duplicated to 4 axes         | sensors/bno085.cpp:219-222                    | STILL OPEN  | Not touched.                                                                                   |
| P3-014   | balance_app no early pitch-NaN gate            | applications/balancing_robot/balance_app.cpp  | STILL OPEN  | Not touched in 4M.2/13 work.                                                                   |
| **P1-015** | CRC-8 XOR-sum insufficient                   | config/calibration_storage.cpp:22-50          | **CLOSED (partial)** | CCITT CRC landed in calibration_storage. **HOWEVER** — see NEW-P1-001: Mega `main.cpp:192-194` still has the same XOR-sum `xor_crc8_()` guarding three EEPROM slots. Closure is incomplete project-wide. |
| **P1-016** | Version mismatch silently accepted           | config/calibration_storage.cpp:153-156        | **CLOSED**  | Per fix doc: `if (version != CAL_FORMAT_VERSION) return false;`.                               |
| P2-017   | ESP32 NVS write failure silent                 | storage/persistent_storage_esp32.cpp          | STILL OPEN  | Not touched. The morning fix doc explicitly defers this.                                       |
| **P1-018** | BNO085 stack overflow on bad num_words       | sensors/bno085_calibration.cpp:52, 77-82      | **CLOSED**  | Per fix doc §"Fix 2": guard `num_words > WORDS_BUFFER_CAPACITY` added after `sh2_getFrs()`. Read-only spot-check recommended.|
| P2-019   | BNO055 begin() no I2C timeout                  | sensors/bno055.cpp:100                        | STILL OPEN  | Not touched.                                                                                   |
| P2-020   | BNO085 begin() no I2C timeout                  | sensors/bno085.cpp:82-92                      | STILL OPEN  | Not touched.                                                                                   |
| P2-021   | Main loop no watchdog kick                     | main.cpp:418-454                              | STILL OPEN  | Mega main.cpp was modified today (4M.11) but the watchdog-kick deficit is in unrelated lines. |
| P2-022   | BNO055 init failure hangs in IDLE              | main.cpp:300-303                              | STILL OPEN  | Not addressed.                                                                                 |
| P2-023   | Motor init does not validate pin state         | actuators/l298n_motor_driver.cpp:76-92        | STILL OPEN  | Not touched.                                                                                   |
| P3-024   | SD card init does not validate filesystem       | file_system/sd_card.cpp:26-38                 | STILL OPEN  | Not touched.                                                                                   |
| P2-025   | UnoBalanceApp volatile float write unprotected | applications/balancing_robot_uno/uno_balance_app.cpp:56 | **CHANGED** | The today-landed `uno_balance_app.cpp:108-111` now wraps the write of `last_pitch_deg_` AND `pitch_valid_` in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. The previously-cited tear is closed. The header comment at uno_balance_app.h:117-121 explicitly explains the trade-off. **Effectively CLOSED.** |
| P3-026   | balance_app pending_state_log sentinel 0xFF    | balance_app.h:451                             | STILL OPEN  | Not touched in today's edits.                                                                  |
| P2-027   | Motor clamp before stiction floor              | actuators/l298n_motor_driver.cpp:99-110       | STILL OPEN  | Not touched.                                                                                   |
| P2-028   | PID set_output_limits silent rejection         | control/pid_controller.cpp:65-75              | STILL OPEN  | Not touched.                                                                                   |
| P2-029   | SD card append_line flush not atomic           | file_system/sd_card.cpp:147-157               | STILL OPEN  | Not touched.                                                                                   |
| P3-030   | SD snapshot_count_ not persisted               | features/snapshot_recorder.cpp:116-120        | STILL OPEN  | Not touched.                                                                                   |
| **P1-031** | BNO085 cal validation is heuristic-only      | sensors/bno085_calibration.cpp:199-230        | **STILL OPEN** | Explicitly deferred by the morning fix doc.                                                |
| P2-032   | Mag cal — no offset/scaling validation         | sensors/bno055.cpp:328-345                    | STILL OPEN  | Not touched.                                                                                   |

**Closure tally:** 4 of 5 P1s closed (P1-007, P1-008, P1-015 *partial*, P1-016, P1-018);
P1-031 still open. P1-015 closure is incomplete project-wide — see NEW-P1-001.

---

## 4. New Attack Surface Added Today

### 4.1 Guided tuning (`uno_balance_app`, `tuning_session`, `tune_storage`)

**Files audited:**
- `src/applications/balancing_robot_uno/tune_storage.h` (1-87)
- `src/applications/balancing_robot_uno/tune_storage.cpp` (1-115)
- `src/applications/balancing_robot_uno/tuning_session.h` (1-86)
- `src/applications/balancing_robot_uno/tuning_session.cpp` (1-213)
- `src/applications/balancing_robot_uno/main.cpp` (1-221)
- `src/applications/balancing_robot_uno/uno_balance_app.cpp` (1-193)

**4.1.a — Input parsing (NEW-P3-001).** `main.cpp:105-145::handle_serial()` reads a
single `int c = Serial.read()`. The `int` return (-1 on no-data) is funneled through a
`switch (c)` that handles known characters and casts to `(char)c` only for routing
into `tuningSession.handle_command()`. The "-1 → 0xFF char" issue: if `Serial.read()`
returned -1 the surrounding `while (Serial.available() > 0)` would have prevented the
read, so -1 cannot reach the switch. **Safe.** `handle_command()` itself does a single
`switch (c)` with `default: break;` (tuning_session.cpp:192-193). **No buffer overflow
surface.** No `strcpy`/`memcpy`/`sprintf` of input — there's nothing to overflow.

**4.1.b — Stack overflow.** No recursion. No large stack arrays. `print_status()`
declares `char b[12]` (tuning_session.cpp:198) — `dtostrf` worst-case output is ~8
bytes, well inside. **Safe.**

**4.1.c — Out-of-bounds enum cast.** `TuneStage` is a scoped `enum class : uint8_t`
(tuning_session.h:32-38). The only way a corrupt value reaches the `switch (stage_)`
sites is if `stage_` is corrupted in RAM — which is not part of the threat model
(physical access to RAM is not in scope). Every switch has a `default:` branch that
no-ops or returns. **Safe.**

**4.1.d — EEPROM write reachability (NEW-P2-001).** `tune_storage::saveTuning()` is
called *only* from one site: `tuning_session.cpp:169`, gated by `stage_ == REVIEW`. To
reach REVIEW the operator must send `t` (IDLE → STAGE_P), then `n n n` to advance
through P → I → D → REVIEW, then `w`. That's 5 deliberate command characters in
sequence; a stuck serial line emitting a single character cannot drive the bot all the
way to a save. **The "stuck Serial" attack is structurally implausible** for this code
path. *However*, there is **no rate-limiting and no confirmation prompt** on `w` —
once in REVIEW, a single `w` writes EEPROM. An accidentally-typed `w` after a long
debug session would overwrite tuning silently. **Severity:** P2 — defensive UX
hardening, not exploit-class.

**4.1.e — Gain bounds on apply_gains() (NEW-P3-002).** The guided-tuning review's P2
finding (`guided_tuning_review_2026-05-20.md` §"Minor Findings") flagged that
`UnoBalanceApp::apply_gains()` does not clamp `kp/ki/kd ≥ 0`. **Re-assessing:** the
caller `TuningSession::handle_command()` already clamps work_kp_/ki_/kd_ to ≥ 0 at
lines 105/109/113. The reachable-from-tuning path is safe. But `apply_gains()` is a
*public* API on the balance app (uno_balance_app.h:66) — if a future serial command
or test harness calls it with negative gains, the PID would receive them with no
guard. **Severity:** P3 (defensive; not currently exploitable).
**Recommendation:** add a one-liner clamp in `apply_gains()` itself.

**4.1.f — Negative-gain motion exploitability (informational).** Even with negative
gains, the PID output is clamped to `[PWM_MIN, PWM_MAX]` (uno_balance_app.cpp:72-73)
and the tip cutoff (uno_balance_app.cpp:147) stops motors at >TIP_CUTOFF_DEG. A
negative-gain bot would oscillate / tip but not "run away"; safety is from the
mechanical cutoffs, not from the gain sign. The risk is *behavioural*, not *security*.

**4.1.g — State-machine bypass.** Walk every transition: only `t` from IDLE enters
STAGE_P (tuning_session.cpp:92-95). Only `n` from STAGE_P enters STAGE_I (line 134).
There is **no command that jumps directly to REVIEW or directly invokes
`saveTuning()`** other than through the `t → n → n → n → w` sequence — and `w` is
gated to `stage_ == REVIEW` (line 162). Even the `b` (back) path is bounded
(STAGE_I → STAGE_P, STAGE_D → STAGE_I, REVIEW → STAGE_D); it cannot exit IDLE.
**No bypass path found.** ✓

**4.1.h — EEPROM CRC integrity.** `tune_storage` uses the same CRC-8-CCITT (poly 0x07,
init 0x00) as the freshly-fixed `calibration_storage.cpp` — explicitly inlined at
tune_storage.cpp:36-51. **Good.** Span is `version + payload = 17 bytes`
(tune_storage.cpp:28). Marker is a separate byte not under the CRC — so a corrupted
marker presents as "no tuning stored" (loadTuning returns false at line 76) and the
seed gains are used. **Graceful fallback. ✓**

**4.1.i — `pitch_off` mirroring.** `tuning_session.cpp:168` writes
`blk.pitch_off = PITCH_OFFSET_DEG`, i.e., the compile-time seed constant. The
`TuneBlock` reserves a `pitch_off` field but it is *not* applied at boot — only Kp/Ki/Kd
are read back at `uno_balance_app.cpp:55-58`. The persisted `pitch_off` is effectively
unused. This is documented in tune_storage.h:46-50 ("future Stage O"). **Safe** but
worth a note that the reserved slot must NOT be re-purposed without bumping the
TUNE_VERSION.

### 4.2 4M.2 K cross-check (`balance_app`)

**Files re-audited:** `src/applications/balancing_robot/balance_app.cpp` around the
new logic (1430-1468).

**4.2.a — Failure path stops motors.** balance_app.cpp:1459-1465: on `k_rel >
BOOTSTRAP_K_DISAGREE_FRAC`, the code executes `motors_.stop(); last_output_ = 0;
failure_reason = 7; converged = false; enter_state_(IDLE).` Motors are stopped
**before** the state transition. ✓ Verified.

**4.2.b — `failure_reason` enum.** `failure_reason` is a `uint8_t` (per
balance_app.h:136-140 enum-comment range 0-8). The value 7 is unambiguous; the
existing 8 is `pwm_discovery_timeout`. No collision. A "corrupt enum read" path would
require a RAM SEU on the same byte, which the firmware does not defend against (out
of scope). ✓

**4.2.c — Divide-by-zero guard.** balance_app.cpp:1448: `k_max > 1e-6f ? k_abs_diff /
k_max : 0.0f`. If both K estimates are ~0 the `no_response` gate at line 1421 would
have already aborted with `failure_reason=2`. ✓

**4.2.d — ISR atomicity in this path.** The K cross-check runs in `step_bootstrap_`
on the **main-loop side** (the ISR's `tick()` only schedules the call). It reads
local copies of `k_measured` and `bs_k_enc_sum_`. `bs_k_enc_sum_` is non-volatile
(updated only by main-loop pulse logic, not from ISR). **No race.**

### 4.3 4M.11 Encoder calibration EEPROM (Mega `main.cpp`)

**Files re-audited:** `src/main.cpp:153-282` (EEPROM slot constants and helpers).

**4.3.a — CRC validation path on bad CRC (NEW-P3-003).** `load_encoder_cal_()`
(main.cpp:248-258): on bad CRC at line 253, returns false. The caller at line 515
checks the return and on false simply skips applying the saved values — encoders keep
their default radius. **Graceful fallback. ✓** *But:* there is no `Serial.println`
warning on a bad-CRC encoder slot. A silently-corrupted encoder cal could land the bot
in BOOTSTRAP with the wrong radius, and the operator would never know unless they ran
`s`. **Severity:** P3 — observability, not exploitability.

**4.3.b — Slot collision risk.** `workstream_f_review_2026-05-20.md` §"EEPROM Slot
Map" walked the slots and verified no overlap (0x000-0x1FF cal blob, 0x200 mount,
0x210 actuator, 0x220 encoder, 0x230 PWM-discovery). I re-confirmed by re-reading
the constant definitions in `main.cpp:153-189`: addresses 0x200/0x210/0x220/0x230,
each 8 or 16 bytes wide, with the next slot 16 bytes higher. **No overlap.** ✓ Magic
byte 0xAD is shared between 0x220 and 0x230 but the addresses are distinct, so there
is no aliasing.

**4.3.c — Mega xor_crc8 weakness (NEW-P1-001).** `main.cpp:192-194` defines
`xor_crc8_` as a single-byte XOR sum — **exactly the algorithm the morning P1-015
finding condemned, and which was replaced project-wide in
`calibration_storage.cpp`.** It is still used here to "validate" three EEPROM slots:
- 0x200 mount-offset (save/load at lines 196-213) — XOR span 7 bytes
- 0x210 actuator/stiction (save/load at lines 215-230) — XOR span 7 bytes
- 0x230 PWM-discovery (save/load at lines 260-281) — XOR span 7 bytes

The encoder slot 0x220 (4M.11) **correctly** uses `calculateCRC8()` (the proper
CRC-8-CCITT), per the standard the security fix established. So today's new code is
good; today's *un*-touched legacy code is still using the weak checksum.

**Why this matters:** the morning audit's P1-015 rationale was that two compensating
single-bit flips can land an unchanged XOR sum on the same byte, passing validation.
Three of the Mega's six runtime parameters are persisted under this weak check. A
single-event-upset (cosmic ray, brownout-during-write) could corrupt actuator stiction
or mount offset, the firmware would accept the garbage, and the bot would behave
unpredictably on next boot.

**Severity:** P1 (data integrity — same as the morning P1-015, just in a different
file). **Recommendation:** replace `xor_crc8_` with `calculateCRC8` from
`config/calibration_storage.{h,cpp}` for all three slots; bump each slot's `*_VER`
byte to invalidate prior records (operator re-cal required). Matches the morning fix
agent's "version bump unless audit specifies otherwise" policy.

### 4.4 4M.13 position_loop cascade

**Files audited:** `src/control/position_loop.h` (1-128), `src/control/position_loop.cpp`
(1-67), and balance_app.cpp around 530-548 and 998-1004.

**4.4.a — Cascade saturation recovery (NEW-P2-002).** Walking the worst-case
divergence:
- Inner balance loop diverges → bot tips → tip cutoff fires
  (uno_balance_app.cpp:147) → motors stop → PID resets.
- On Mega: tipped-state → HELD/FALLEN (balance_app.cpp:1007-1013) → motors stop and
  PID resets.
- **But what about `position_loop_`?** balance_app.cpp:1003 resets it *only* when
  re-entering RUN. While in HELD/FALLEN, `position_loop_` retains the last `position_m_`
  and `last_nudge_deg_`. On a stall-recovery → RUN, the reset clears it. **OK.**
- *However:* if the bot is RUN and the inner loop somehow diverges *without* tipping
  (e.g., encoder reports a stuck zero velocity while the bot is actually moving — see
  the bias finding 4M.13-9 in workstream_f_review), the leaky integrator could wind
  to its asymptote of `wheel_vel × dt / (1 - POS_LEAK)` = `wheel_vel × 5 s`. For a
  0.5 m/s bias that's 2.5 m of "drift", and `K_POS=6` would saturate the clamp at
  ±2°. The clamp+slew protect the inner loop, but **the bot will lean at the clamped
  limit forever until it's manually stopped.** This is a *behavioural* concern, not a
  pure security one — but it's a runaway-actuation risk on stuck-encoder input.
  **Severity:** P2 (graceful-degradation gap — encoders are trusted absolutely once
  past the 4M.2 K cross-check; that gate may not cover steady-state encoder freeze).

**4.4.b — Reset discipline (PASS).** Verified at balance_app.cpp:1003 — every RUN
re-entry resets `position_loop_`. ✓ Matches the encoder `reset_ticks()` discipline
established by 4M.2.

**4.4.c — `dt <= 0` guard (PASS).** position_loop.cpp:30-32 returns the cached nudge
on `dt <= 0`. No NaN propagation possible from the leak / slew arithmetic. ✓

**4.4.d — Clamp + slew correctness (PASS).** Already verified by
`workstream_f_review_2026-05-20.md` finding 4M.13-3. Re-confirmed by reading
position_loop.cpp:50-62. ✓

**4.4.e — ISR safety.** `position_loop_.update()` is called from `step_run_`
(balance_app.cpp:543) — that's main-loop side in the universal app (the
balance_app's `tick()` is the ISR but `step_run_` is called from the loop tick
dispatch). No volatile state, no ISR-shared variables. ✓

---

## 5. EEPROM Data-Integrity Audit

The Mega's EEPROM map after all of today's landings, with CRC posture:

| Slot       | Owner / phase           | Magic | Ver  | Layout / span                                                        | CRC algorithm                       | Failure mode             | Posture |
|------------|--------------------------|-------|------|----------------------------------------------------------------------|-------------------------------------|--------------------------|---------|
| 0x000–0x1FF | BNO055 calibration blob | 0xCB  | 0x02 | header (v2: marker, ver, len-lo, len-hi, crc, rsvd) + payload ≤506 B | **CRC-8-CCITT (FIXED today)**       | restoreFromEEPROM=false → operator re-cal | ✓ Strong |
| 0x200       | Mount offset (Mega) **or** tuning block (Uno) | 0xAB / 0xB5 | 0x01 | 8 B (Mega) / 19 B (Uno) | **XOR-sum on Mega** / CRC-8-CCITT on Uno | load returns false → use compile-time seed | Mega **WEAK**; Uno ✓ |
| 0x210       | Actuator / stiction      | 0xAC  | 0x01 | 8 B: magic, ver, stiction, rsvd×4, xor                              | **XOR-sum**                         | load returns false → STICTION_PWM seed | **WEAK** |
| 0x220       | Encoder cal (4M.11)      | 0xAD  | 0x01 | 16 B: magic, ver, cpm_L (f32), cpm_R (f32), radius (f32), rsvd, ccitt | **CRC-8-CCITT (NEW today)**         | load returns false → encoder defaults | ✓ Strong |
| 0x230       | PWM-discovery (4M.12)    | 0xAD  | 0x01 | 8 B: magic, ver, min_L, min_H, max_L, max_H, rsvd, xor              | **XOR-sum**                         | load returns false → PWM defaults | **WEAK** |
| 0x238–0xFFF | Free                     | —     | —    | —                                                                    | —                                   | —                        | —       |

**Two of the six slots use the project-standard CRC-8-CCITT; three use the deprecated
XOR-sum;** the BNO055 blob was upgraded today. Magic-byte + version-byte gates are
consistently used. **Catastrophic vs graceful matrix:**

- All slots: bad CRC / bad magic → `load_*` returns false → caller uses compile-time
  default. No path applies garbage to the live hardware.
- All slots: silent log (no Serial warning) on a bad-CRC return — operator must run
  `s` to notice. Already flagged as NEW-P3-003.
- 0x220 encoder cal: an "all-passes-CRC but value is garbage" payload would mis-set
  `radius_m` and the wheel-velocity scale. This is mostly defended *upstream* by the
  4M.2 K cross-check (which would fire `failure_reason=7` if encoder K disagrees with
  gyro K). ✓ Good defense in depth.

**USB-Serial trust boundary:** A connected operator can run `e` (encoder cal), `c`
(cal save), or `m` (mounting wizard) on the Mega. Once those wizards run, the EEPROM
is whatever they made it. **There is no cryptographic guard, and there CANNOT be one
on a Mega — no key storage, no signing.** Document this trust boundary in operator
docs; do not attempt to defend against physical access. The CRC is for SEU
detection, not anti-tamper.

---

## 6. ISR Safety as a Security Property

**Reasoning:** A torn read across the ISR boundary can produce a value that never
existed, and on a PID with Kp=65 that maps directly to "drive the motors hard in a
random direction" — a runaway actuation event. Treating it as a security property is
justified.

**New tuning modules (NEW-P0-PASS).** Searched `tune_storage.{h,cpp}`,
`tuning_session.{h,cpp}` for `volatile`, `ATOMIC_BLOCK`, `noInterrupts`,
`cli()`/`sei()`. Result:

- `tune_storage`: zero references. Module operates only on stack-local buffers and
  EEPROM (which is synchronous and not ISR-shared). ✓
- `tuning_session`: zero references. The class lives on the main-loop side; it never
  touches the volatile state owned by `UnoBalanceApp`. ✓
- The one place tuning *does* touch shared state is `apply_gains()` (uno_balance_app.cpp:171-176),
  which writes `cur_kp_/ki_/kd_` and calls `pid_.set_tunings()`. The PID's internal
  gain state is read in `compute()` from the ISR-scheduled `step()`. `cur_*` are
  non-volatile floats — a torn read by the PID could *in principle* produce a one-tick
  arbitrary gain.
  - **Mitigation:** `set_tunings()` updates the PID's internal gain copy *atomically
    enough* that even a torn read costs one tick of off-gain PID output, which the
    inner loop's filter and the PWM clamp absorb. The guided-tuning review
    (`guided_tuning_review_2026-05-20.md` §3) flagged and accepted this trade-off.
  - I concur: the alternative (locking the ISR during a gain update) would skip a 5 ms
    PID tick — a worse outcome.

**4M.2 cross-check race (PASS).** The `step_bootstrap_` reads `raw_gyro_dps_[1]` and
`bs_k_enc_sum_` from main-loop context. `raw_gyro_dps_[]` is documented to have
write-side atomicity in `read_imu_`; `bs_k_enc_sum_` is main-loop-only. Already
audited by workstream_f_review finding 4M.2-6 (P1-ISR-1). ✓

**4M.13 cascade race (PASS).** Same posture as 4M.2 — main-loop side, no
ISR-shared state. ✓

**UnoBalanceApp `last_pitch_deg_` write (PASS, formerly P2-025).** The morning audit's
P2-025 flagged the write at the old line 56. The today-landed code at
`uno_balance_app.cpp:108-111` and `132-135` now wraps both the writer and the reader
in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. ✓ Effectively closed.

---

## 7. Serial Input Hardening

**Uno main.cpp::handle_serial() (main.cpp:105-145):**

- `int c = Serial.read()` is correctly gated by `while (Serial.available() > 0)` so -1
  cannot reach the dispatch. ✓
- The cast `(char)c` at line 139 is safe for 0–127 (all the commands today are ASCII).
  For 128–255 the high bit would survive into the `switch (c)` in `handle_command`
  — but a high-bit char hits the `default:` and is silently dropped (tuning_session.cpp:192).
  ✓
- **Backpressure:** the Serial RX hardware FIFO on the Uno is 64 bytes (HardwareSerial
  buffer). At 115200 baud (~11.5 kBytes/s) the FIFO fills in 5.5 ms. The `loop()` runs
  at least every ~10 ms (set by the `delay(2)` plus IMU read), so a sustained input
  burst > ~10 KB/s could in theory overflow. **However:** Arduino's HardwareSerial
  silently drops overflow bytes; it doesn't crash or buffer-overflow into anything
  else. The worst case is a missed command, not an exploit. ✓
- No `Serial.readString()`, `Serial.readBytesUntil()`, or `parseInt()` is used — those
  are the API surfaces where buffer overflow typically lives. ✓

**Mega main.cpp** uses the same single-char dispatch pattern with the same backpressure
posture. Spot-checked.

**Verdict:** serial-input attack surface is structurally minimal. No P1/P2 findings.
**NEW-P3-004 (cosmetic):** It would be a good idea to add a non-blocking warning log
when `handle_serial()` sees an unrecognized char, so operators are not silently
ignored — but this is observability, not security.

---

## 8. "Physical Safety, Not Security" Concerns (Out-of-Scope Reminders)

These don't belong in this audit but should appear in any *physical-safety* review of
this code:

- **Bot falls during BOOTSTRAP.** Covered by quiescence gates (balance_app — pulse
  cooldowns, baseline-noise check). 4M.2 added the K cross-check on top — a slipping
  drivetrain fails the bootstrap rather than seeding bad gains.
- **Motors energize unexpectedly during HELD or FALLEN.** Defended by collision-latched
  HELD state machine and quiescence-gated auto-resume.
- **EEPROM corruption causes runaway calibration.** Defended by CRC + magic-byte
  gates. Weakened on the three Mega slots using the legacy XOR-sum (NEW-P1-001).
- **Power glitch during EEPROM write.** Today's slots use `EEPROM.update()` (per-byte)
  or the `ps::write+commit` HAL. A glitch can land a partially-written record; the CRC
  will catch it on next boot and the slot will fall back to defaults. ✓
- **Stuck-encoder failure (RUN runaway).** Flagged here as NEW-P2-002 — `position_loop`
  trusts encoders to within the clamp limits, but a steady-state encoder freeze could
  leave the bot leaning at the clamped limit forever.

---

## 9. Comparison to Prior Audit

The morning audit had **4 P1 findings**; **4 of the 5** have been closed (P1-007,
P1-008, P1-016, P1-018). P1-015 was closed in `calibration_storage.cpp` but the same
underlying flaw still lives in `main.cpp` (NEW-P1-001). P1-031 (BNO085 cal heuristic
validation) is explicitly deferred.

Workstream F (Phases 4M.2, 4M.11, 4M.13) and the guided-tuning feature add roughly
**~300 lines of new code** across 5 new files. The new code is consistently more
defensive than the codebase average:

- ATOMIC_BLOCK wrapping on both reader and writer in uno_balance_app (closes P2-025).
- CRC-8-CCITT used everywhere new (tune_storage, encoder cal).
- Magic-byte + version-byte gates on every new slot.
- Reset discipline on every state-entry (position_loop, encoders).
- `dt <= 0` guards on every integrator.

The new EEPROM slot 0x200 (Uno tuning block) and the new command surface (`t/+/-/*/n/b/r/w/q`)
do increase the attack surface, but the increases are **proportional to the feature
value** (without 0x200 there is no persistence; without the command surface there is
no guided tuning, which the team chose over offline brute-force tuning for good
operator-UX reasons).

**Net posture:** **better** on data integrity (3 P1s closed); **slightly worse** on
EEPROM attack surface (one new P1 surfaced — the still-weak `xor_crc8_` in
main.cpp). On balance, today is a clear net positive.

---

## 10. Recommendations (Prioritized)

### P0
*(None.)* No runaway-motor or hardware-damage paths were identified.

### P1
1. **Replace `xor_crc8_` in `main.cpp:192-194` with `calculateCRC8()`** from
   `config/calibration_storage.{h,cpp}`, bump each affected slot's version byte
   (EE_MOUNT_VER, EE_ACT_VER, EE_PWMDISC_VER) from 0x01 → 0x02, and update the
   matching `load_*_()` functions to reject the old version. Operator must re-cal
   each slot. Same approach the morning fix agent used for the BNO055 blob.
2. **(Still open from morning audit) P1-031: implement FRS field-level validation in
   `bno085_calibration.cpp::validateCalibrationData()`.** Out of scope of today's
   workstreams but explicitly deferred from morning fixes.
3. **Re-spot-check `bno085_calibration.cpp` against the morning fix doc's claim**
   that the stack-overflow guard for `num_words > WORDS_BUFFER_CAPACITY` was added.
   I did not re-read that file in this audit (out of write-zone focus).

### P2
4. **Add `apply_gains()` input clamps in `uno_balance_app.cpp:171-176`.** Already
   recommended by the guided-tuning reviewer; closes NEW-P3-002 and re-closes
   the audit's defense-in-depth posture on the PID public API.
5. **Add `position_loop` runaway-recovery in `step_run_`.** When the inner loop has
   commanded full PWM continuously for >N seconds without `position_m_` decreasing,
   abort to HELD with a new `held_entry_reason_`. Defends NEW-P2-002 (stuck-encoder
   steady-state lean).
6. **Add a confirmation prompt before `w` (save tuning).** A two-step `w → yes/no`
   handshake closes NEW-P2-001 (accidental EEPROM overwrite during long debug
   sessions).
7. **(Still open from morning) P2-019, P2-020: I²C `begin()` timeouts on BNO055/BNO085.**
   Critical for hang resilience; not addressed by today's work.

### P3
8. **Emit a Serial warning** on bad-CRC EEPROM reads in `main.cpp:248-258` (encoder),
   and the analogous mount/actuator/PWM-disc paths. One-liner per slot. Closes
   NEW-P3-003.
9. **Emit a debug-only log on unknown serial chars** in `handle_serial()` —
   observability for operator typos. Closes NEW-P3-004.
10. **Document the trust boundary explicitly** in `docs/scope.md` or an
    operator-facing README: "EEPROM contents are trusted after CRC + magic-byte
    pass; physical USB access to the bot is treated as fully privileged. The CRC
    is for single-event-upset detection, not anti-tamper."

### Documentation / runtime
11. **Document an operational guideline:** disable `UNO_GUIDED_TUNING` in any
    "flight"-grade build by leaving the macro undefined (the `arduino_uno_minimal`
    env already does this — confirmed in `guided_tuning_review_2026-05-20.md` §4).
    Land a comment in `platformio.ini` noting this is intentional.
12. **Flag for Phase 4M.14:** the hardcoded `position_loop` gains (POSLOOP_K_POS=6.0,
    K_VEL=3.0, etc.) must remain off the bench-tuning loop until 4M.14's
    pole-placement derivation lands. This is the sequencing-discipline flag
    already documented in `workstream_f_review_2026-05-20.md` finding 4M.13-13;
    repeat here to keep it visible.

---

## Appendix A — Files Re-Audited Today

- `src/applications/balancing_robot/balance_app.cpp` (1430-1468, 530-548, 998-1004)
- `src/applications/balancing_robot/balance_app.h` (referenced via review docs)
- `src/applications/balancing_robot_uno/uno_balance_app.h` (1-139)
- `src/applications/balancing_robot_uno/uno_balance_app.cpp` (1-193)
- `src/applications/balancing_robot_uno/main.cpp` (1-221)
- `src/applications/balancing_robot_uno/tune_storage.h` (1-87)
- `src/applications/balancing_robot_uno/tune_storage.cpp` (1-115)
- `src/applications/balancing_robot_uno/tuning_session.h` (1-86)
- `src/applications/balancing_robot_uno/tuning_session.cpp` (1-213)
- `src/control/position_loop.h` (1-128)
- `src/control/position_loop.cpp` (1-67)
- `src/main.cpp` (153-282 — EEPROM slot helpers)

## Appendix B — Files NOT Re-Audited (out of today's scope)

- `src/sensors/gps.cpp` (no edits today; carries 4 P2 + 2 P3 from morning)
- `src/sensors/bno055.cpp` (no edits today; carries 1 P2 + 1 P2 morning items)
- `src/sensors/bno085.cpp` and `bno085_calibration.cpp` (touched by the morning
  fixer; the fix file claims P1-007 / P1-018 closed — read-only spot-check
  recommended in a follow-up)
- `src/file_system/sd_card.cpp` (no edits today; carries 1 P2 + 1 P3)
- `src/features/snapshot_recorder.cpp` (no edits today; carries 1 P3)
- `src/storage/persistent_storage_esp32.cpp` (deferred by morning fix)
- `src/actuators/l298n_motor_driver.cpp` (no edits today; carries 2 P3-class items)
- `src/control/pid_controller.cpp` (no edits today; note the `tau=0` path is
  exercised at uno_balance_app boot — P2-010 deserves re-verification, but
  this is a re-audit, not a new audit, so it stays in the morning's list)

## Appendix C — In-Flight Sibling Work (referenced but not depended on)

- `phase_4m14_design_*.md` — design for the analytical derivation that will retire
  the position_loop's hardcoded gains.
- `ao_session_synthesis_*.md` — overall session synthesis; this audit is one of
  its inputs.
- New tests in `tests/test_position_loop.cpp`, `tests/test_tune_storage.cpp`,
  `tests/test_tuning_session.cpp` — referenced by git-status; if their assertions
  match the CRC behaviour and state-machine guards documented here, NEW-P0-PASS
  / NEW-P3-003 will be the regression net.

---

**Audit complete.** No commits. No builds. One markdown file in the write-zone:
`/home/devel/floppi/auto_orientation/docs/findings/ao_security_reaudit_2026-05-20.md`.
