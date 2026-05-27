# Session record — 2026-05-26: Uno SETUP/OPERATIONAL mode + on-Uno BNO055 calibration + P→D→I + photo-backup printer

**Date**: 2026-05-26
**Author**: doc agent (`ao-save-progress-setup-mode@floppi:1`)
**Scope**: `auto_orientation/` only — Uno-minimal application surface. Flight-controller side untouched.
**Commit status**: **NO COMMITS this session.** Everything below is uncommitted in the working tree per operator instruction. The prior 2026-05-22 session was committed in a previous turn (`045fe7c`).

---

## Scope of this record

This was a multi-agent, orchestrator-managed wave on the Uno-minimal balance app:

1. Reframed the Mega-vs-Uno split as **memory-driven (capability tier)** rather than IMU-driven.
2. Shipped a **SETUP MODE / OPERATIONAL MODE** split (two build envs).
3. Made BNO055 calibration **self-contained on the Uno** (no Mega-path dependency).
4. Reordered the guided PID stages from **P→I→D to P→D→I**.
5. Made photo-backup a first-class principle — every EEPROM-written value is printed in a copy-paste-ready form so the operator can hardcode it back into source after EEPROM loss.

Canonical operator-facing walkthrough: [`docs/applications/balancing_robot_uno/README.md` §3–§4](../../applications/balancing_robot_uno/README.md).
Canonical scope/framing: [`docs/scope.md` §Platform bifurcation (clarified 2026-05-26)](../../scope.md#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal).
Superseded prior design (P→I→D order, Mega-path cal): [`docs/findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md) — banner at top of file.

---

## What landed (working tree only — uncommitted)

### 1. Build envs — SETUP vs OPERATIONAL
- **`arduino_uno_tuning`** (SETUP MODE) — defines `UNO_GUIDED_TUNING`; pulls in `tuning_session.cpp`, `calibration_session.cpp`, prompt strings, and the photo-backup printer. **23540 B (73 % flash)**, +3058 B vs prior baseline; 27 % flash headroom remains.
- **`arduino_uno_minimal`** (OPERATIONAL MODE) — lean flight build. Reads PID gains + 22-byte BNO055 cal from EEPROM at boot; falls back to the `balance_constants.h` seed if EEPROM is empty (prints which source it used). **20342 B (63 % flash)**, +3642 B (cal restore + photo-backup printer linked).
- New SETUP / OPERATIONAL boot banners in `main.cpp` distinguish the two builds out-of-the-box; on-boot cal restore + clear missing-EEPROM `WARN` in both.
- `mega_balance` unchanged from baseline (this session was Uno-only).

### 2. On-Uno BNO055 guided calibration (`'c'`)
- New files: `src/applications/balancing_robot_uno/calibration_session.{h,cpp}` (SETUP-build only).
- `'c'` disarms the bot, prints a pose script, polls the BNO055's own per-channel cal-status bytes, and saves the 22-byte blob via the new `tune_storage::save_cal_blob`.
- The Uno **no longer depends on the Mega calibration path**. Operator can fully calibrate on the Uno itself; this is the change behind the README §6 troubleshooting note updated 2026-05-26.

### 3. Guided PID stage order — P→D→I
- `src/applications/balancing_robot_uno/tuning_session.{h,cpp}` enum, transitions, masks, and prompts reordered.
- **Rationale**: D damps Kp's oscillation first; then a small Ki removes residual drift without re-exciting it. The prior P→I→D order tended to push the operator to over-integrate before damping was in place.
- Walkthrough: [README §4.3](../../applications/balancing_robot_uno/README.md#43--walk-p--d--i-t).

### 4. Photo-backup printer (value-robustness principle)
- New `tune_storage::print_photo_backup()` emits a copy-paste-ready block bounded by
  `==== PHOTO-BACKUP -- paste into balance_constants.h ====` /
  `==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ====`,
  containing 4 float lines (`BALANCE_KP/KI/KD`, `PITCH_OFFSET_DEG`) and the
  `BNO055_CAL_BLOB[22]` hex array.
- Printed on **both** `'w'` (save) at REVIEW **and** `'s'` (status snapshot) — operator photographs the scrollback as a paper backup.
- `balance_constants.h` gained a **PHOTO-BACKUP HARDCODE SITE** comment block and a `BNO055_CAL_BLOB[22]` declaration (default = 22 × `0xFF` = no seed cal). The hardcode-paste site is the canonical recovery path if EEPROM is wiped or the chip is replaced.
- First-class principle, documented in [README §4.7](../../applications/balancing_robot_uno/README.md#47--value-robustness--every-persisted-value-is-photographable) and [scope.md §value-robustness](../../scope.md).

### 5. Docs
- `docs/scope.md` — Mega-vs-Uno reframing (memory-driven capability tier, IMU choice orthogonal to MCU choice; both BNO055 and BNO085 valid on either MCU, current code defaults to BNO055) + SETUP/OPERATIONAL framing + value-robustness subsection.
- `docs/applications/balancing_robot/INDEX.md` — universal-auto framing reinforced.
- `docs/applications/balancing_robot_uno/README.md` — §3 first-boot, §4 setup-mode walkthrough (exact prompts/banners), §4.3 P→D→I order, §4.7 value-robustness, §5 file tree updated for `calibration_session.{h,cpp}`.
- `docs/findings/uno_guided_tuning_design_2026-05-20.md` — superseded-in-two-ways banner (P→I→D order + Mega-path cal dependency), body retained as historical context.
- `docs/architecture/LEVEL_1_SUBSYSTEMS.md` — Mermaid label fix.
- `docs/findings/INDEX.md` — superseded note added to the wave-4 hygiene pass.

### 6. Wave-3 bug fix caught by the verifier agent
- `main.cpp` `'s'` branch was incorrectly wired and never reached `TuningSession::handle_command('s')`, so the photo-backup snapshot was unreachable — verifier agent flagged the dead code, fix re-routed the `'s'` dispatch through the tuning session. Worth flagging because it would have looked fine in any single-keystroke happy-path test; only the wiring inspection caught it.

---

## API-freeze pattern used to run parallel agents

To dispatch coding agents in parallel without file-lock conflicts, the orchestrator **froze the public API of `tune_storage` first** (signatures only — `save_cal_blob(const uint8_t*)`, `load_cal_blob(uint8_t*)`, `print_photo_backup()`), then split the work:

- One agent implemented `calibration_session.{h,cpp}` against the frozen `tune_storage` header.
- A second agent implemented the photo-backup printer body inside `tune_storage.cpp`.
- A third agent reordered the stages in `tuning_session.{h,cpp}`.
- `balance_constants.h`, `main.cpp`, and the docs were edited by their own agents against the frozen API.

This kept the per-agent write zones disjoint and let the wave land in one pass. The bug at the end (Wave-3 `'s'` mis-wire) came from `main.cpp` being touched by two waves — a reminder that **wiring code** is the natural collision site even when module APIs are clean.

---

## Verification

| Artifact | Result | Δ vs baseline |
|---|---|---|
| `arduino_uno_tuning` build | **SUCCESS** | 23540 B (73 % flash), +3058 B for setup-mode features |
| `arduino_uno_minimal` build | **SUCCESS** | 20342 B (63 % flash), +3642 B (cal restore + photo-backup linked) |
| `mega_balance` build | **SUCCESS** | unchanged |
| Native test suite | **19/19 PASS** | — |

All GREEN.

### Audit — all 8 categories GREEN
CRC/EEPROM, `calibration_session`, photo-backup, banners, P→D→I reorder, on-boot restore, simplicity, flash headroom — no P1/P2 issues. No standalone audit findings doc was written; the verdict lives here.

---

## State of the working tree (per `git status --short`)

Modified:
- `auto_orientation/docs/applications/balancing_robot/INDEX.md`
- `auto_orientation/docs/applications/balancing_robot_uno/README.md`
- `auto_orientation/docs/architecture/LEVEL_1_SUBSYSTEMS.md`
- `auto_orientation/docs/findings/INDEX.md`
- `auto_orientation/docs/findings/uno_guided_tuning_design_2026-05-20.md`
- `auto_orientation/docs/scope.md`
- `auto_orientation/src/applications/balancing_robot_uno/balance_constants.h`
- `auto_orientation/src/applications/balancing_robot_uno/main.cpp`
- `auto_orientation/src/applications/balancing_robot_uno/tune_storage.{cpp,h}`
- `auto_orientation/src/applications/balancing_robot_uno/tuning_session.{cpp,h}`
- `auto_orientation/src/applications/balancing_robot_uno/uno_balance_app.cpp`

Untracked (new):
- `auto_orientation/src/applications/balancing_robot_uno/calibration_session.{cpp,h}`

**No commits this session.** Operator does not want a commit yet.

---

## What's next

All next steps are **bench-hardware-gated**. All carried-forward items from the 2026-05-22 record still apply (F-3 K_VEL observation, regression-baseline capture, real-motor PWM-discovery validation on Mega, NaN-failsafe behaviour on real hardware), **plus now**:

- **Run the new Uno `'c'` calibration + `'t'` P→D→I tuning on a real bot** to validate the operator UX end-to-end. The flow compiles, audits clean, and the photo-backup wiring is fixed — but the prompts/order/timing have only been read, not driven.
- **Confirm the photo-backup hardcode-paste recovery path** by wiping EEPROM, pasting a photographed block into `balance_constants.h`, reflashing `arduino_uno_minimal`, and verifying the bot resumes its last-known-good state.

Open question (file under operator-policy): **does the OPERATIONAL flight build want its own `'c'` re-cal command** so field re-calibration doesn't require reflashing `arduino_uno_tuning`? Flagged in `docs/todo.md`.

---

## Cross-references

- Operator walkthrough — [`docs/applications/balancing_robot_uno/README.md` §3–§4.7](../../applications/balancing_robot_uno/README.md)
- Platform framing — [`docs/scope.md` §Platform bifurcation](../../scope.md)
- Prior design (now partially superseded) — [`docs/findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md)
- Prior session record — [`2026-05-22_safety_correctness_docs.md`](2026-05-22_safety_correctness_docs.md)

---

## Wave 6 — Uno IMU selection wiring (2026-05-26, late)

A follow-on wave landed the same day, after the SETUP/OPERATIONAL split above was done. Two coding workstreams ran in parallel — T1 on the AO side (this section) and T2 on the FC side (see [`flight_controller/docs/archive/session_records/2026-05-26_calibration_storage_port.md`](../../../../flight_controller/docs/archive/session_records/2026-05-26_calibration_storage_port.md)).

### What T1 landed

The architectural memory-tier story — "Uno is the small/cheap tier; BNO085 needs Mega/Teensy/ESP32" — was made concrete at the build level:

- **`src/applications/balancing_robot_uno/main.cpp`** — compile-time IMU selection via `#ifdef USE_BNO085` / `#else default USE_BNO055`. **Three `#error` guards**, ordered so the most-diagnostic message wins:
  1. Both `USE_BNO055` and `USE_BNO085` defined → reject (e.g. a stale `-DUSE_BNO055` left over when a developer appends `-DUSE_BNO085`).
  2. `USE_BNO085` defined on `__AVR_ATmega328P__` → reject with: *"USE_BNO085 not supported on AVR ATmega328P (Uno) — BNO085 library exceeds Uno's 32 KB flash. Use BNO055 on Uno, or BNO085 on Mega/Teensy/ESP32."* The Adafruit_BNO08x / SH-2 library is ~15–20 KB flash, which the existing minimal app can't absorb on a 32 KB part.
  3. Neither flag defined → auto-default to `USE_BNO055` (matches the reference `SelfBallancingRobot3.ino`).
- **`tune_storage` cal-blob API — variable-length.** New `save_cal_blob(const uint8_t*, uint16_t len)` / `load_cal_blob(uint8_t* out, uint16_t out_capacity, uint16_t* out_len)` / `has_cal_blob()`. On-disk layout at base `0x220` is `marker(1) | version(1) | len_lo(1) | len_hi(1) | payload[1..72] | crc8`. The 72 B ceiling is sized for **BNO085 SH-2 DYNAMIC_CALIBRATION FRS worst case** (~36–72 B); BNO055's `adafruit_bno055_offsets_t` is still 22 B and continues to fit comfortably. **Version byte bumped `0x02 → 0x03`** — old 22-byte fixed-layout v2 blobs reject-on-load cleanly (operator re-runs `'c'` to re-cal). The block-format version is the right gate here: mis-reading a fixed-22 payload as if it had a length header would corrupt the calibration.
- **`calibration_session` stays BNO055-specific.** Kept the `BNO055&` parameter; double-gated on `defined(USE_BNO055) && defined(UNO_GUIDED_TUNING)`. Virtualizing the BNO055-specific cal accessors to support both IMUs would have been heavier than warranted; the BNO085 SH-2-FRS cal flow is a separate future workstream and gets its own session.

### Non-byte-identical trade-off — accepted

The widened slot is **not** byte-identical with the prior 22-byte-only layout: the variable-length API added ~300–400 B per env (build deltas: `arduino_uno_minimal` +292 B → 20634 / 716; `arduino_uno_tuning` +412 B → 23952 / 744; Mega unchanged; `USE_BNO085` on Uno trips `#error` cleanly per design). T1 flagged this explicitly and chose the cleaner abstraction over byte-identicality. This matches the project's [value-loss-robustness principle](../../scope.md) — the cal-blob slot is a persisted-value site, and a future-IMU-aware layout that survives a hardware refresh is worth a few hundred bytes today.

### Out-of-zone edit — flagged with rationale

`tuning_session.cpp` (2 call sites) consumes the new cal-blob API. The T1 agent flagged this as out-of-zone (`tuning_session.{cpp,h}` is not in T1's nominal write zone) but justified it as **mechanical and required for the build to link** — the API change had to ripple to every caller in one commit, otherwise the linker would break. The change is purely a signature update on the existing call sites; no new logic was added to `tuning_session.cpp`. Worth recording because the API-freeze pattern (see the wave-4 section above) is exactly what's supposed to prevent out-of-zone touches — this is the exception that proves the rule: when the **shape** of an API changes (here: fixed-arg → variable-len), the callers must move with it in the same wave.

### #error story — the memory-tier principle made concrete

Before wave 6, "Uno doesn't get BNO085" was a doc statement (in `scope.md`'s platform-bifurcation section). After wave 6 it is **a build-time fact** — a developer who appends `-DUSE_BNO085` to a Uno env build gets an immediate, actionable diagnostic instead of a 6 KB-over-flash link failure that requires hunting down which library blew the budget. The `#error` text names the constraint *and* names the alternative ("Use BNO055 on Uno, or BNO085 on Mega/Teensy/ESP32"). This is the kind of small concretization that pays off the next time someone hits it, and it's the pattern to follow for any architectural constraint that wants to survive contact with a hurried developer.

### Verification

| Artifact | Result | Δ vs wave-5 baseline |
|---|---|---|
| `arduino_uno_minimal` build | **SUCCESS** | 20634 B / 716 RAM (+292 B for variable-length cal-blob API) |
| `arduino_uno_tuning` build | **SUCCESS** | 23952 B / 744 RAM (+412 B for variable-length cal-blob API) |
| `mega_balance` build | **SUCCESS** | unchanged |
| `arduino_uno_minimal` + `-DUSE_BNO085` (override) | **FAILS** with `#error` (per design) | n/a |

### State of the working tree after wave 6

Additional uncommitted changes on top of the wave-5 working tree:
- `src/applications/balancing_robot_uno/main.cpp` (IMU selection block — `#ifdef USE_BNO085` / 3 `#error` guards)
- `src/applications/balancing_robot_uno/tune_storage.{cpp,h}` (variable-length cal-blob API; version 0x02 → 0x03; slot widened to 72 B)
- `src/applications/balancing_robot_uno/tuning_session.cpp` (out-of-zone — 2 call-site updates for the new API)
- `docs/scope.md`, `docs/applications/balancing_robot_uno/README.md`, `docs/todo.md`, this session record (this section)

Wave 6 also dispatched a parallel FC-side workstream (T2) that vendored AO's `calibration_storage` into `flight_controller/lib/CalibrationStorage/` and wired it into `imu.cpp` + `calibration_mode.cpp` — that work lives in its own FC session record at [`flight_controller/docs/archive/session_records/2026-05-26_calibration_storage_port.md`](../../../../flight_controller/docs/archive/session_records/2026-05-26_calibration_storage_port.md).

### What's next (wave-6 additions)

Carrying forward the existing bench-validation list, plus:

- The new `#error` guards are static-checked-by-design but the **Mega-side BNO085 path is not yet wired** — when a workstream needs it, the `mega_balance` env in `platformio.ini` will need a `-DUSE_BNO085` override path. Tagged as a future workstream in `docs/todo.md`.
- The variable-length cal-blob slot is provisioned for **BNO085 SH-2 DYNAMIC_CALIBRATION FRS** (~36–72 B) but the **BNO085 guided cal session does not exist yet** — `calibration_session` polls BNO055-specific cal-status bytes. Tagged as a future workstream in `docs/todo.md`.
- A future operator on a BNO085-capable target may want to add a `BNO085_CAL_BLOB[<len>]` declaration to `balance_constants.h` alongside the existing `BNO055_CAL_BLOB[22]` hardcode site, so the photo-backup paste-recovery path covers BNO085 the same way it covers BNO055. Low priority; tagged in `docs/todo.md`.

---

## Wave 8 — defensive imu_tag selection (2026-05-26, later)

Closes the wave-6 audit's only P2 finding: `tuning_session.cpp:185` and `:215` both passed a hard-coded `imu_tag="BNO055"` to `tune_storage::print_photo_backup()`. The wave-6 comment correctly noted that "when BNO085 wiring lands the tag will branch on `USE_BNO085`", and today this is consistent with the build — `USE_BNO085` on AVR is rejected by `main.cpp`'s `#error`, and even on a future non-AVR target `calibration_session` is double-gated on `defined(USE_BNO055) && defined(UNO_GUIDED_TUNING)` so no BNO085 blob can reach the photo-backup printer. The fix is therefore **purely defensive**: it ensures that the day a BNO085 guided-cal session lands on Mega/Teensy/ESP32, a developer who forgets to update these two call sites cannot silently mislabel a BNO085 blob as `BNO055_CAL_BLOB[<len>]`.

Implementation: added a small `static const char* imu_tag_for_print_()` helper near the top of `tuning_session.cpp` that mirrors the `#if defined(USE_BNO085)` / `#else` pattern already in `uno_balance_app.cpp:123-127`, and replaced the two literal `"BNO055"` arguments with calls to it. Single helper avoids repeating the `#if/#else/#endif` block at both call sites. Build: `arduino_uno_tuning` **SUCCESS, 23952 B / 744 RAM — byte-identical to the wave-6 baseline** (as expected — the default branch resolves to the same `"BNO055"` literal at compile time). No other files touched; `arduino_uno_minimal` and `mega_balance` not rebuilt (unaffected, wave-7 already swept them).
