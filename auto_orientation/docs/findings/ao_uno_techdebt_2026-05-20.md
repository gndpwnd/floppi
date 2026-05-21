# Uno-Side Balancing-Bot — Tech-Debt Audit — 2026-05-20

**Agent:** ao-uno-techdebt@floppi:2
**Lens:** Tech debt — maintainability, dead code, duplication, naming, dependency graph,
encapsulation, build-flag hygiene. **NOT** security or correctness — those are covered by
`ao_security_reaudit_2026-05-20.md` and `guided_tuning_review_2026-05-20.md`; this report
cross-references rather than duplicates them.
**Mode:** READ-ONLY. One markdown file. No edits, no builds, no commits.
**Scope:** `src/applications/balancing_robot_uno/` + the Uno env blocks in `platformio.ini`.

---

## 1. Executive Summary

The Uno-side codebase is **small, recently written (all 8 files touched 2026-05-20),
and well-commented**. The guided-tuning feature (UT-A/B/C, `tune_storage`,
`tuning_session`) landed clean and the guided-tuning review already cleared it for bench
use. From a *tech-debt* angle the posture is **LIGHT** — there is no rot, no large
functions, no deep nesting, and headers are minimal. The findings here are almost
entirely small consistency / encapsulation / duplication items that would each take
minutes to fix.

### LOC inventory (Uno-side)

| File | LOC | Role |
|---|---:|---|
| `balance_constants.h` | 104 | Cold-start seed constants (62 lines comment, 18 code) |
| `tune_storage.h` | 87 | EEPROM tune-block API + layout doc |
| `tune_storage.cpp` | 114 | EEPROM persistence impl (CRC8, save/load/has) |
| `tuning_session.h` | 85 | Guided P→I→D state-machine class decl |
| `tuning_session.cpp` | 212 | Guided state-machine impl |
| `uno_balance_app.h` | 139 | Minimal balancer class decl |
| `uno_balance_app.cpp` | 192 | Minimal balancer impl |
| `main.cpp` | 220 | `setup()`/`loop()` entry point |
| **Total** | **1153** | (≈ 380 lines are comment / blank — code is ≈ 770 LOC) |

### Posture: **LIGHT**

Disciplined, idiomatic, low entropy. No file is too long; the longest function
(`TuningSession::handle_command`, 105 LOC) is a flat command dispatcher and is
*acceptable* as written. No dead code of consequence, no TODO/FIXME/XXX markers
anywhere in the eight files.

### Findings by priority

| Priority | Count | Theme |
|---|---:|---|
| P0 | 0 | — |
| P1 | 2 | CRC8 duplication; naming convention split (`camelCase` vs `snake_case`) |
| P2 | 5 | `uno_balance` legacy env confusion; `STICTION_PWM` location; `last_pwm_` volatile-class scope; `print_status` non-const; `arm()`/`begin()` overlap |
| P3 | 6 | Minor: comment drift, magic step constants, enum-name asymmetry, header-comment staleness, `read_imu()` no-arg vs design, `s` double-routing |

No P0/P1 *correctness* issues — those are out of scope and were already cleared by the
two prior reviews.

---

## 2. File-by-File Walkthrough

| File | LOC | Funcs | Deepest nest | Longest func | API surface | Notes |
|---|---:|---:|---:|---|---|---|
| `balance_constants.h` | 104 | 0 | — | — | 9 `static const` | Header-only constants. 60% comment. Discipline good (see §9). |
| `tune_storage.h` | 87 | 3 decl | — | — | 3 free funcs in `tune_storage::`, 1 struct, 3 macros | Clean namespaced API. `#define` macros vs `constexpr` (P3). |
| `tune_storage.cpp` | 114 | 4 (`crc8_ccitt` + 3) | 3 (`crc8` bit loop) | `loadTuning` ~23 LOC | `crc8_ccitt` correctly anon-namespaced | Duplicates calibration CRC8 (**P1**). Clean AVR guard. |
| `tuning_session.h` | 85 | 6 decl | — | — | `TuningSession` class + `TuneStage` enum | Entire file `#ifdef UNO_GUIDED_TUNING`. Helpers `private` + trailing `_`. Good. |
| `tuning_session.cpp` | 212 | 7 (`fmt_` static + ctor + 5) | 4 (`handle_command` switch-in-switch) | `handle_command` **105 LOC** | — | Longest fn in scope; flat dispatcher, acceptable (§8). |
| `uno_balance_app.h` | 139 | 9 decl | — | — | `UnoBalanceApp` class | 4 inline accessors. Members trailing `_`. `last_pwm_` volatile-scope nit (P2). |
| `uno_balance_app.cpp` | 192 | 7 | 2 | `step()` ~48 LOC | — | Tight, well-commented. No nesting issues. |
| `main.cpp` | 220 | 4 (`pid_isr`, `print_status_line`, `handle_serial`, `setup`, `loop`) | 3 (`handle_serial` switch) | `setup()` ~48 LOC | file-scope statics | 5 `#ifdef UNO_GUIDED_TUNING` gates, consistent. |

**Helper scoping:** Good throughout. `crc8_ccitt` (tune_storage.cpp:36) and `fmt_`
(tuning_session.cpp:23) are file-local (anon namespace / `static`). `TuningSession`
private helpers carry the trailing-underscore convention. No leaked internals.

**Constants discipline:** `balance_constants.h` is well-grouped (§9). `TuningSession`
step constants live in the class as `static const` (tuning_session.h:43-46) — defensible
since they are UI increments, not control-law params (already noted P3 in
guided_tuning_review §P3).

---

## 3. Naming / Consistency

The Uno code is mostly **snake_case for functions and members** with a **trailing
underscore on private members** — a clear, consistently applied house style. The one
real break is the `tune_storage` API.

| # | Observation | File:line | Severity |
|---|---|---|---|
| N1 | `tune_storage` free functions are `camelCase`: `saveTuning`, `loadTuning`, `hasTuning` | tune_storage.h:63,73,83 | P1 |
| N2 | …but the file-local helper is `snake_case`: `crc8_ccitt` | tune_storage.cpp:36 | P1 |
| N3 | Every other Uno-side function is `snake_case`: `read_imu`, `apply_gains`, `get_work_gains`, `print_status`, `handle_command`, `apply_masked_`, `print_status_line`, `handle_serial`, `pid_isr` | uno_balance_app.h:59-92, main.cpp:79-105 | P1 |
| N4 | `calibration_storage` (the sibling that `tune_storage` consciously mirrors) is *also* `camelCase`: `saveToEEPROM`, `restoreFromEEPROM`, `calculateCRC8` | calibration_storage.cpp:22,66,129 | context |
| N5 | Struct field `pitch_off` (tune_storage.h:49) vs constant `PITCH_OFFSET_DEG` (balance_constants.h:68) — same quantity, two abbreviations | tune_storage.h:49 | P3 |
| N6 | `TuneStage` enum members mix bare and prefixed: `IDLE`, `REVIEW` vs `STAGE_P`, `STAGE_I`, `STAGE_D` | tuning_session.h:32-38 | P3 |
| N7 | Boolean predicates are consistent and good: `is_armed()`, `is_tipped()`, `hasTuning()` all read as questions | uno_balance_app.h:106-107 | ✓ OK |
| N8 | Trailing-underscore members applied uniformly: `imu_`, `motors_`, `pid_`, `cur_kp_`, `work_kp_`, `entry_kp_`, `stage_`, `coarse_`, `app_` | uno_balance_app.h:112-136, tuning_session.h:67-81 | ✓ OK |

**Verdict:** N1–N3 (the `camelCase` API on `tune_storage`) is the single naming
inconsistency worth a deliberate decision. It was almost certainly inherited by
copying `calibration_storage`'s style (N4). Either rename `tune_storage`'s three
functions to `snake_case` (matches everything else in `balancing_robot_uno/`) or
accept the split as "storage-module house style." The former is cheap (the API has
exactly 4 call sites — `uno_balance_app.cpp:55`, `tuning_session.cpp:169`, and the two
`tune_storage` internal call sites) and is the recommendation.

---

## 4. Duplication

### D1 — CRC8 duplicated verbatim (**P1**)

`tune_storage.cpp:36-51` `crc8_ccitt()` is a **byte-for-byte reimplementation** of
`calibration_storage.cpp:22-60` `calculateCRC8()` — same polynomial (0x07), same init
(0x00), same bitwise no-table loop. The `tune_storage.cpp:30-35` doc comment even says
so: *"identical algorithm to calculateCRC8()."* The two differ only in signature
(`size_t len` vs `uint16_t length`) and function name.

This is a *deliberate, documented* duplication — the design (uno_guided_tuning_design
§2.4 / §UT-A) said *"do NOT route through the 506-byte calibration blob path."* That
decision is sound for the *storage path* (the calibration blob is overkill for 4
floats), but it does **not** require duplicating the **CRC primitive**. CRC8-CCITT is
a pure leaf function with a `stdint`-only dependency.

**Tech-debt cost:** two copies of a checksum that *must* agree byte-for-byte to be
correct. If anyone ever "improves" one (table-driven, different poly) the other
silently diverges, and there is no test pinning them together.

**Recommendation:** promote one CRC8-CCITT to a shared leaf — e.g.
`src/util/crc8.h`/`.cpp` (new) or an existing `src/math/` slot — and have both
`calibration_storage.cpp` and `tune_storage.cpp` call it. ~15 LOC net deletion. This
is the strongest *promote-to-src/util* candidate in the Uno scope. **Caveat:** the
shared file would be a cross-zone change (touches `calibration_storage.cpp`, outside
the Uno-side); flag for an orchestrator to assign as its own small PR — do not bundle.

### D2 — `tune_storage` vs `calibration_storage` marker/version/CRC pattern

Both modules implement the same *shape*: marker byte + version byte + payload + CRC,
with `save`/`load`/`has` triplet and identical validation order
(marker → version → CRC). `tune_storage` is the lighter, EEPROM-direct variant;
`calibration_storage` routes through the `ps::` HAL. This is **structural similarity,
not copy-paste** — the bodies differ (HAL calls vs `EEPROM.update`, 6-byte v2 header
vs 2-byte marker+version). A generic "versioned-CRC blob" template could unify them,
but that is **over-engineering for two call sites** on an 8-bit target. **Leave as
is** — only the CRC primitive (D1) is worth de-duplicating.

### D3 — `tuning_session.cpp` vs `balance_app.cpp` state-machine plumbing

The Uno `TuningSession` (IDLE/STAGE_P/I/D/REVIEW) and the Mega `BalanceApp` state
machine (IDLE/BOOTSTRAP/CHARACTERISE/RUN/HELD) **share no code and should not**.
`TuningSession`'s "state machine" is a flat `enum` + `switch` command dispatcher with
`enter_stage_()` as the only transition helper — there is no `step_*` per-state tick,
no shared `enter_state_()` timing plumbing. Different problem, different shape. **No
duplication, no action.**

### D4 — gain-triple plumbing

`(kp, ki, kd)` is passed as three loose floats through `apply_gains()`,
`get_work_gains()`, `set_tunings()`, and stored as three member triples
(`cur_kp_/ki_/kd_`, `work_kp_/ki_/kd_`, `entry_kp_/ki_/kd_`) plus the `TuneBlock`
struct's `kp/ki/kd`. A `Gains` struct would collapse the repetition. **Minor (P3) —
on an 8-bit target with 4 call sites the loose-float style is idiomatic; not worth a
refactor.** Noting for completeness.

---

## 5. Dead Code

**Result: essentially none.** This is a clean, new codebase.

- **No TODO / FIXME / XXX / HACK** markers in any of the 8 files (grep-confirmed).
- **No commented-out code blocks.**
- **No uncalled helpers** — every private method is reached:
  `apply_masked_` (tuning_session.cpp:55,118,157), `print_prompt_`
  (tuning_session.cpp:86,184), `enter_stage_` (tuning_session.cpp:94,134-136,143-145),
  `fmt_` (tuning_session.cpp:207-209).
- **Non-AVR stubs** in `tune_storage.cpp:106-112` are *not* dead — they exist so the
  module compiles for native test / host syntax-check builds (the `native_test` env
  excludes `balancing_robot_uno/` so they are exercised only by host syntax-check, but
  they are an intentional portability shim, not rot).
- **DC1 (P3) — `PWM_MIN` / `STICTION_PWM` consumed but seam is subtle.** `PWM_MIN`
  (balance_constants.h:83) is read at `main.cpp:63` and `uno_balance_app.cpp:72`;
  `STICTION_PWM` at `main.cpp:58`. Both live. Not dead — listed only to confirm the
  scan covered every constant.
- **DC2 (P3) — `read_imu()` return value discarded at the only call site.**
  `main.cpp:204` calls `app.read_imu();` and ignores the `bool`. The function is
  *designed* to return validity (uno_balance_app.h:78) and `step()` re-checks
  `pitch_valid_` independently, so the return is redundant *at this call site*. Not
  dead code (the contract is reasonable for future callers / tests), but the loop
  could drop the value or use it for a fault counter. Cross-ref: the morning
  `audit_code_quality` flagged the *Mega* equivalent (`imu_.read()` discarded,
  balance_app.cpp:1369) as P2; the Uno path here is structurally safer because
  `read_imu()` sets `pitch_valid_=false` on failure and `step()` honors it. **Lens
  note:** this is borderline correctness/error-handling — deferring the depth of it to
  `ao_security_reaudit`; flagged here only as a "discarded return" code-smell.

---

## 6. API Surface / Encapsulation

| # | Observation | File:line | Severity |
|---|---|---|---|
| E1 | `UnoBalanceApp` — all data members `private`, all mutators/accessors `public`. Clean. No friends, no leaked internal types. | uno_balance_app.h:111-137 | ✓ OK |
| E2 | `TuningSession` — 3 helpers `private`, 3 entry points `public`, all members `private`. Clean. | tuning_session.h:59-82 | ✓ OK |
| E3 | `tune_storage` correctly hides `crc8_ccitt` + offset constants in an anon namespace (tune_storage.cpp:19-53); only the 3-function `tune_storage::` API is exported. | tune_storage.cpp:19 | ✓ OK |
| E4 | `TuneBlock` struct is public in `tune_storage.h:45` — necessary (callers construct it) and it is a plain POD. Fine. | tune_storage.h:45 | ✓ OK |
| E5 | `print_status()` is **non-const** but performs no logical mutation — it only reads `stage_`/`work_*` and writes to `Serial`. Should be `const`. Same for `UnoBalanceApp` — there is no `print` there, but the pattern applies if one is added. | tuning_session.h:57 | P2 |
| E6 | `apply_gains()` is public with no input validation — already filed P2 by guided_tuning_review (§P2). **Not re-filing** — cross-reference only. | uno_balance_app.h:66 | (ref) |
| E7 | `last_pwm_` is declared `volatile` (uno_balance_app.h:126) but is written only inside `step()` (the ISR path) and `abort()`/`arm()`/`begin()` (loop path), and read by `last_pwm()` accessor (loop path). The `volatile` is correct for the ISR-shared read, but it is grouped under the "// Last commanded PWM (for telemetry)" comment as if it were a simple cache — the volatility rationale is documented for `last_pitch_deg_` (lines 116-121) but **not** for `last_pwm_`. Minor doc-scope gap. | uno_balance_app.h:125-126 | P2 |

**Verdict:** encapsulation is **good**. E5 (`const`-correctness on `print_status`) and
E7 (undocumented `volatile` rationale) are the only items, both trivial.

---

## 7. Compile / Link Impact

**Headers are minimal and clean — no Arduino bloat pulled into headers.**

- `uno_balance_app.h` includes only `<stdint.h>` + 3 framework headers
  (`sensor_base.h`, `motor_driver.h`, `pid_controller.h`) — and all three of *those*
  include only `<stdint.h>` (grep-confirmed). No `<Arduino.h>` in any header.
- `<Arduino.h>` / `<Wire.h>` / `<MsTimer2.h>` / `<EEPROM.h>` appear **only in `.cpp`
  files**, correctly guarded (`#if defined(ARDUINO)` in uno_balance_app.cpp:14,
  `#if defined(__AVR__)` in tune_storage.cpp:14). This is textbook discipline.
- `tuning_session.h` pulls `uno_balance_app.h` (needs `UnoBalanceApp&`) — could in
  principle be a forward declaration `class UnoBalanceApp;` since the header only uses
  `UnoBalanceApp*`/`UnoBalanceApp&`. **CI1 (P3):** forward-declare opportunity, saves a
  transitive include in `main.cpp`'s tuning path. Marginal benefit on a project this
  size — note only.
- `balance_constants.h` is `<stdint.h>`-only and included by 4 TUs — fine.

**No forward-decl problems, no circular includes, no `#include` of `.cpp`.** Compile
graph is shallow and correct.

---

## 8. Function-Length Hot Spots

| Rank | Function | File:line | LOC | Split opinion |
|---|---|---|---:|---|
| 1 | `TuningSession::handle_command` | tuning_session.cpp:90-195 | 105 | **Do not split.** It is a flat `switch` over 9 command chars; each case is 3-15 lines. Splitting per-case into helpers would *add* indirection without reducing complexity (cyclomatic complexity is in the case count, which is irreducible). The one nested `switch` (the `+`/`-` case, lines 102-117) is the only candidate — extracting an `adjust_current_gain_(float delta)` helper would shave ~12 lines and remove one nesting level. Optional, P3. |
| 2 | `UnoBalanceApp::step` | uno_balance_app.cpp:115-162 | 48 | Leave as is. Linear: disarmed→atomic-snapshot→pitch-ok→tip→PID. Each guard has a clear comment. Max nesting 2. Splitting would scatter the ISR-critical path across functions — bad for a 5 ms ISR. |
| 3 | `setup` | main.cpp:151-199 | 48 | Leave as is. Linear init sequence; comments carry hard-won ordering rationale (the `delay(1500)` before `MsTimer2::start()` is load-bearing). |
| 4 | `loadTuning` | tune_storage.cpp:75-97 | 23 | Leave as is. Read-marker→read-buffer→check-version→check-CRC→memcpy. Linear, well-guarded. |
| 5 | `begin` (UnoBalanceApp) | uno_balance_app.cpp:51-82 | 32 | Leave as is. EEPROM-or-seed branch + PID config. Fine. |

**Verdict:** **no function is too long.** The codebase has zero length debt. Only the
nested `switch` in `handle_command` (item 1) is a candidate, and it is optional.

---

## 9. Constants Discipline — `balance_constants.h`

The file is **well-organized**: 5 labelled sections (PID gains / mounting offset / loop
timing / output limits & stiction / safety), each constant has a comment, and the
SEED-vs-FIXED distinction is explicit in every section header.

| # | Observation | File:line | Severity |
|---|---|---|---|
| C1 | All 9 constants are `static const` in a header. On AVR each TU that includes the header gets its own copy in `.rodata` — but they are tiny scalars and the linker/compiler folds most. Idiomatic for Arduino. `constexpr` would be marginally cleaner (C++11 is available — `native_test` uses `-std=c++11`). | balance_constants.h:58-102 | P3 |
| C2 | **Comment drift (P3):** the file header (lines 31-34) says the current values *"were produced 2026-05-20 by an evolutionary brute_tune.py run (fitness=3.272)"* — but the guided-tuning pivot (uno_guided_tuning_design §1) explicitly **demoted** brute_tune.py and re-cast this file as a hand-editable seed. The header comment block (lines 1-46) was rewritten for the pivot, yet lines 31-34 still narrate brute_tune provenance as if authoritative. Mildly contradictory with lines 5-9. | balance_constants.h:31-34 | P3 |
| C3 | **`STICTION_PWM` arguably misplaced (P2).** It sits under the "Output limits & stiction" section (line 90) and is marked FIXED / never-EEPROM. That is fine, but `STICTION_PWM` is consumed only by `main.cpp:58` (`L298NMotorDriver` ctor) — it is a *motor-driver* parameter, not a PID/balance parameter. It is the one constant in this header that is not about the balance control law. Acceptable to keep (it is a deploy-time hardware constant and this is the deploy-time constants file) but worth a one-line note that it is a driver param. |
| C4 | Naming is consistent: all `UPPER_SNAKE`, units in the name where ambiguous (`PITCH_OFFSET_DEG`, `TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG`, `PID_SAMPLE_MS`). `BALANCE_KP/KI/KD` and `PWM_MIN/MAX` and `STICTION_PWM` unit-less but obvious. ✓ OK | balance_constants.h | ✓ OK |
| C5 | No orphaned constants — all 9 are referenced (`BALANCE_KP/KI/KD` → uno_balance_app.cpp:47-48,63-65 + main.cpp:154-156; `PITCH_OFFSET_DEG` → uno_balance_app.cpp:107 + tuning_session.cpp:168; `PID_SAMPLE_MS` → uno_balance_app.cpp:71,157 + main.cpp:195; `PWM_MIN/MAX` → main.cpp:63-64 + uno_balance_app.cpp:72-73; `STICTION_PWM` → main.cpp:58; `TIP_CUTOFF_DEG` → uno_balance_app.cpp:147; `PITCH_SANITY_DEG` → uno_balance_app.cpp:97). | — | ✓ OK |
| C6 | **Precision theatre (P3):** seed values are written to 4 decimal places — `BALANCE_KP = 94.4873f`, `PITCH_OFFSET_DEG = -4.3327f`, `TIP_CUTOFF_DEG = 25.0000f`, `PITCH_SANITY_DEG = 90.0000f`. The last two are obviously round numbers padded with `.0000`; the gains' 4-digit precision is brute_tune output noise that a *hand-editable seed* (the new role) does not need. Cosmetic, but `25.0000f` reads oddly for a hand-set safety limit. | balance_constants.h:58-60,98,102 | P3 |

**Verdict:** constants discipline is **good**. C2 (header narration drift) and C3
(`STICTION_PWM` is a driver param) are the only substantive notes; C1/C6 are cosmetic.

---

## 10. Build-Flag Interaction — `UNO_GUIDED_TUNING`

Every `UNO_GUIDED_TUNING` occurrence in the Uno scope (grep-confirmed, 7 code sites +
doc mentions):

| # | File:line | Use | Style | OK? |
|---|---|---|---|---|
| 1 | tuning_session.h:25 | `#ifdef UNO_GUIDED_TUNING` — opens; gates entire class body | `#ifdef` | ✓ |
| 2 | tuning_session.h:84 | `#endif  // UNO_GUIDED_TUNING` — closes | labelled | ✓ |
| 3 | tuning_session.cpp:8 | `#ifdef UNO_GUIDED_TUNING` — gates whole TU | `#ifdef` | ✓ |
| 4 | tuning_session.cpp:212 | `#endif  // UNO_GUIDED_TUNING` | labelled | ✓ |
| 5 | main.cpp:35-37 | gates `#include "tuning_session.h"` | `#ifdef` | ✓ |
| 6 | main.cpp:68-71 | gates `static TuningSession tuningSession;` | `#ifdef` | ✓ |
| 7 | main.cpp:122-125 | gates `tuningSession.print_status()` in the `'s'` case | `#ifdef` | ✓ |
| 8 | main.cpp:136-140 | gates `tuningSession.handle_command()` in `default:` | `#ifdef` | ✓ |
| 9 | main.cpp:172-176 | gates `tuningSession.begin(app)` in `setup()` | `#ifdef` | ✓ |

**Observations:**

- **Style is consistent** — every gate is `#ifdef` (never `#if defined`), every
  multi-line `#endif` is labelled with `// UNO_GUIDED_TUNING`. No `#else` branches in
  the flight path (the flight build just gets nothing — correct, since `TuningSession`
  is purely additive).
- **No missed gate.** The guided-tuning review §4 already verified flight-build
  overhead is 0 bytes. `tuning_session.{h,cpp}` are gated at file scope so even though
  `arduino_uno_minimal`'s `build_src_filter` *compiles* `tuning_session.cpp` (it is
  caught by the `+<applications/balancing_robot_uno/>` glob), the TU emits nothing.
  Confirmed: tuning_session.cpp:8 wraps the whole file.
- **BF1 (P3) — double-routing of `'s'`.** In `arduino_uno_tuning`, the `'s'` char hits
  the explicit `case 's'` in `main.cpp:119-126` (which calls `print_status_line()` +
  `tuningSession.print_status()`). It does **not** also reach
  `tuningSession.handle_command()` because `case 's'` `break`s before `default:`.
  *However* `tuning_session.cpp:188-190` *also* has a `case 's'` in `handle_command`
  that calls `print_status()` — which is **unreachable** in the current `main.cpp`
  wiring because `main.cpp` intercepts `'s'` first. Not a bug, but the `case 's'` in
  `handle_command` (tuning_session.cpp:188) is **effectively dead** given the current
  serial-routing in `main.cpp`. Either remove it, or document that it exists for a
  future routing where `handle_command` is the sole sink. Minor. Cross-ref:
  guided_tuning_review §6 noted `'s'` "works in all stages" but did not catch that the
  `handle_command` copy is shadowed.
- The `default:` case in `main.cpp:135-142` routes *every* unrecognized char to
  `tuningSession.handle_command()`, then falls through to the `// ignore` comment. The
  comment "ignore — minimal program, no other commands" (line 141) is now slightly
  stale in the *tuning* build — chars *are* handled by the tuning session there. Tiny
  comment-accuracy nit (P3).

**Verdict:** `#ifdef` hygiene is **excellent**. Only BF1 (shadowed `case 's'`) is
actionable, and it is cosmetic.

---

## 11. `platformio.ini` — Uno Portion

Three Uno-relevant envs: `uno_balance`, `arduino_uno_minimal`, `arduino_uno_tuning`.

| # | Observation | Lines | Severity |
|---|---|---|---|
| PI1 | **`uno_balance` is the legacy universal-on-Uno env and is a confusion hazard (P2).** `default_envs = uno_balance` (line 22) makes the *legacy 92.9%-flash* build (per uno_guided_tuning_design §3) the default `pio run` target. It builds the **Mega** universal balance stack on the Uno (`base_balance` + `balance_src_filter` *excludes* `balancing_robot_uno/`, line 68). The actual current Uno program is `arduino_uno_minimal` / `arduino_uno_tuning`. A new contributor running bare `pio run` builds the wrong, near-full thing. **Recommend:** change `default_envs` to `arduino_uno_minimal`, or add a comment at line 22 explaining `uno_balance` is legacy. The morning `audit_build_system` may have its own take — cross-ref before acting. | 22, 89-100 | P2 |
| PI2 | **Header comment block (lines 4-8) is stale.** It lists `uno_balance` as "current bench bot" and does not mention `arduino_uno_minimal` or `arduino_uno_tuning` at all — the two envs that *are* the current Uno program. The env list in the top-of-file doc is out of date relative to the pivot. | 4-8 | P3 |
| PI3 | `arduino_uno_minimal` and `arduino_uno_tuning` are **near-identical** — same `platform`, `board`, `lib_deps`, `build_src_filter`; they differ only by `arduino_uno_tuning` adding `-D UNO_GUIDED_TUNING`. The `build_src_filter` block (5 lines) and `lib_deps` block (4 lines) are duplicated verbatim (lines 145-150 vs 178-183, 141-144 vs 174-177). A shared `[uno_minimal_base]` section that `arduino_uno_tuning` `extends` would remove ~9 duplicated lines and guarantee the two filters never drift. Same DRY principle the file already applies via `[base]`/`[base_balance]`/`[balance_src_filter]`. | 134-184 | P2 |
| PI4 | **Flags are consistent with code expectations.** `USE_BNO055` + `BNO055_NO_EXT_CRYSTAL` are read by `main.cpp:45` (`#ifdef BNO055_NO_EXT_CRYSTAL`); `UNO_GUIDED_TUNING` gates the tuning code (§10). No Uno env sets a flag the code ignores, and no Uno-side `#ifdef` checks a flag no env sets. `USE_BNO055` is *not* directly `#ifdef`-checked in `balancing_robot_uno/` (the minimal app hard-includes `bno055.h`) — it is effectively a no-op flag for the minimal/tuning envs, carried for consistency with the BNO055 driver's own internal gating. **PI4a (P3):** `USE_BNO055` is dead *as a gate* in the minimal/tuning envs — harmless, but noting it. | 138-140, 170-173 | P3 |
| PI5 | `build_src_filter` for `arduino_uno_minimal` correctly picks up `tune_storage.cpp` (needed at boot for EEPROM read) via the `+<applications/balancing_robot_uno/>` glob — confirmed it also picks up `tuning_session.cpp`, which is harmless because that TU self-gates to empty without `UNO_GUIDED_TUNING` (§10). The design (§3) said *"`arduino_uno_minimal` adds `tune_storage.cpp` to its filter"* — in practice the **glob already covers it**, no explicit add needed. Doc/impl agree in effect. | 145-150 | ✓ OK |
| PI6 | `native_test` env excludes `balancing_robot_uno/` (line 259), so the Uno-side `.cpp` files are **never compiled by CI's test env** — only by the AVR envs. The non-AVR stubs in `tune_storage.cpp:106-112` are therefore exercised only by host syntax-check, not by `pio test`. Not a platformio bug, but it means the new `tests/test_tune_storage.cpp` / `test_tuning_session.cpp` / `test_position_loop.cpp` (untracked, per git status) must compile those modules some other way. Out of scope (tests) — flagged only because it touches Uno-env wiring. | 259 | (ref) |

**Verdict:** Uno envs are **functionally correct**. PI1 (wrong default env) and PI3
(duplicated env blocks) are the two worth acting on; both are low-risk edits.

---

## 12. Recommendations (Prioritized)

| ID | P | File | Change | Why | LOC Δ | PR |
|---|---|---|---|---|---:|---|
| R1 | P1 | `tune_storage.cpp` + new `src/util/crc8.{h,cpp}` + `calibration_storage.cpp` | Extract one CRC8-CCITT leaf; both modules call it | Two byte-identical CRC copies that *must* agree; no test pins them; silent-divergence risk (§D1) | −15 | **Own PR** — cross-zone (touches `calibration_storage.cpp`); needs orchestrator to assign |
| R2 | P1 | `tune_storage.{h,cpp}` | Rename `saveTuning`/`loadTuning`/`hasTuning` → `save_tuning`/`load_tuning`/`has_tuning` | Only `camelCase` API in `balancing_robot_uno/`; everything else is `snake_case` (§N1-N3). 4 call sites | ~0 | Own small PR (or same-PR with R1 if R1 happens) |
| R3 | P2 | `platformio.ini` | Change `default_envs` to `arduino_uno_minimal`; or comment line 22 that `uno_balance` is legacy | Bare `pio run` currently builds the 92.9%-flash legacy stack, not the real Uno program (§PI1) | +1 | Same PR as R4 (both platformio.ini) — but cross-ref `audit_build_system` first |
| R4 | P2 | `platformio.ini` | Factor `arduino_uno_minimal`/`arduino_uno_tuning` shared parts into `[uno_minimal_base]`; tuning env `extends` it + adds `-D UNO_GUIDED_TUNING` | ~9 duplicated `lib_deps`/`build_src_filter` lines; drift risk; matches existing `[base]` pattern (§PI3) | −9 | Same PR as R3 |
| R5 | P2 | `tuning_session.h` | Make `print_status()` `const` | No mutation; const-correctness (§E5) | ~0 | Same PR as R2 (both tuning_session) |
| R6 | P2 | `uno_balance_app.h` | Add one-line comment on `last_pwm_` explaining the `volatile` (ISR-shared write), like `last_pitch_deg_` already has | Volatility rationale documented for one volatile member but not the other (§E7) | +1 | Same PR as R5 |
| R7 | P3 | `tuning_session.cpp` | Remove the shadowed `case 's'` in `handle_command` (lines 188-190), or comment that it is reachable only if routing changes | Dead given current `main.cpp` `'s'` interception (§BF1) | −3 | Same PR as R5 |
| R8 | P3 | `balance_constants.h` | Reconcile header lines 31-34 (brute_tune provenance narration) with the pivot; trim `25.0000f`/`90.0000f` to `25.0f`/`90.0f` | Comment drift vs guided-tuning pivot; precision theatre on hand-set limits (§C2, §C6) | ~0 | Same PR — docs-only, low risk |
| R9 | P3 | `platformio.ini` | Update header env list (lines 4-8) to mention `arduino_uno_minimal`/`arduino_uno_tuning` | Top-of-file doc predates the pivot (§PI2) | +3 | Same PR as R3/R4 |
| R10 | P3 | `tuning_session.cpp` | *Optional:* extract `adjust_current_gain_(float delta)` from the nested `+`/`-` switch in `handle_command` | Removes the one nested-switch / nesting-level-4 spot (§8 item 1) | ~0 | Optional; skip unless touching the file anyway |

### Suggested PR grouping

1. **PR A (platformio.ini hygiene):** R3 + R4 + R9 — all `platformio.ini`, low risk.
   *Check `audit_build_system_2026-05-20.md` first* in case it already proposes a
   default-env change.
2. **PR B (tuning_session polish):** R2 + R5 + R7 (+ optional R10) — all
   `tuning_session.{h,cpp}` + `tune_storage.{h,cpp}` rename; cohesive small cleanup.
3. **PR C (CRC de-dup):** R1 alone — cross-zone, needs orchestrator sign-off because it
   touches `calibration_storage.cpp` outside the Uno scope.
4. **PR D (docs):** R8 — `balance_constants.h` comment fixes, docs-only.

**Nothing here is urgent.** The Uno-side code is in good shape; these are polish items
that pay down small consistency/duplication debt before the codebase grows.

---

## Appendix — Cross-References (not re-filed here)

| Item | Owning report | This report's stance |
|---|---|---|
| `apply_gains()` no input validation (P2) | guided_tuning_review §P2 | Noted §E6, not re-filed |
| `TuningSession` step constants are magic numbers (P3) | guided_tuning_review §P3 | Noted §2, agree it is intentional |
| Float gain mutation during ISR (data-race, accepted) | guided_tuning_review §3 | Correctness/concurrency — out of scope |
| EEPROM CRC / region-split correctness | guided_tuning_review §1 | Correctness — out of scope; only the *duplication* (D1) is mine |
| `imu_.read()` return discarded (Mega P2) | audit_code_quality §5 | Uno equivalent is safer (§DC2); flagged only as a smell |
| Hardcoded Uno constants are intentional | audit_code_quality §2 | Agree; §9 covers discipline, not the values |
| Uno flash budget acceptable | audit_code_quality §3, guided_tuning_review §5 | Agree; not re-analyzed |

*Audit complete. READ-ONLY — no files modified outside this report.*
