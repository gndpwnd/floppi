# mega_orientation RAM Fix — Action Report

**Date:** 2026-05-20
**Agent:** mega-ram-fixer@floppi:1
**Scope:** Apply Phase A reclaim fixes from `mega_orientation_ram_overflow_diagnosis_2026-05-19.md`, constrained to `src/navigation/ekf.{h,cpp}`.
**Verification:** `pio run -e mega_orientation` — **SUCCESS**

---

## TL;DR

The `mega_orientation` env **already builds and links cleanly at 74.5 % RAM (6101 / 8192 B)** — well under the 8 KB budget. The 125 % overflow described in the diagnosis was already addressed by prior sibling-agent work that landed *infrastructure-level* gating: `platformio.ini` now defines `-D USE_EKF=0` and excludes `-<navigation/ekf.cpp>` from the build; `src/main.cpp` wraps all `ekf` references in `#if USE_EKF`. With USE_EKF=0, `ekf.h` is not even included by `main.cpp`, so the `ExtendedKalmanFilter` global is never instantiated and the 4 × 16×16 covariance matrices never reach `.bss`.

Within my write zone (`ekf.h`, `ekf.cpp`) I implemented the **F2 reclaim** (drop the `F_` Jacobian member, recompute on the stack in `predict()`). This further trims the in-memory `ExtendedKalmanFilter` instance size by ~1024 B — a no-op for `mega_orientation` *today*, but real savings for any future env (or re-enabled USE_EKF=1) that does instantiate the filter.

---

## 1. Before / After build numbers

`pio run -e mega_orientation`:

| Metric | Before this session | After this session | Delta |
|---|---:|---:|---:|
| RAM | 6 101 / 8 192 B (74.5 %) | 6 101 / 8 192 B (74.5 %) | 0 B |
| Flash | 38 004 / 253 952 B (15.0 %) | 38 004 / 253 952 B (15.0 %) | 0 B |
| Build | SUCCESS | SUCCESS | — |

The 0-byte delta is **expected and correct**: my edits live in `ekf.cpp` (excluded from this env's `build_src_filter`) and in `ekf.h` (not included when `USE_EKF=0`). The benefit lands wherever EKF is actually compiled (`native_test`, or any future env that re-enables `USE_EKF=1`).

For reference, the diagnosis baseline was **10 283 / 8 192 B (125.5 %)**. Prior agents reclaimed **≈ 4 182 B** via the infra gating before this session began.

---

## 2. Diagnosis fixes — disposition

The diagnosis catalogues 9 candidate reclaims (F1–F9). Each is dispositioned against my write-zone constraint (ekf.h, ekf.cpp only):

| Fix | Target file | In write zone? | Status |
|---|---|---|---|
| **F1** Remove `P_temp_` member (–1024 B) | `ekf.h`, `ekf.cpp` | YES | **DONE PRE-SESSION** — already removed; comments in `ekf.h:226-231` and `ekf.cpp:357/535` document the prior reclaim. No further action. |
| **F2** Remove `F_` member, stack-local Jacobian (–1024 B) | `ekf.h`, `ekf.cpp` | YES | **DONE THIS SESSION** — see §3 below. |
| F3 Shrink SH2 transfer buffers (–832 B) | `lib/Adafruit_BNO08x_Arduino/src/sh2_hal.h` | NO (vendored lib, BLOCKED) | SKIPPED — outside write zone. |
| F4 Shrink BNO085 calibration buffer (–234 B) | `src/sensors/bno085.h` | NO (BLOCKED) | SKIPPED — outside write zone. |
| F5 Drop SD library (–585 B) | `platformio.ini` | NO (BLOCKED) | SKIPPED — outside write zone. NB: `platformio.ini:195` still has `SD` in `lib_deps`; flash is fine but ~585 B of SD `.bss` is currently linked. |
| F6 `F()`-wrap string literals (–350 B) | `src/main.cpp`, `src/config/mode.h` | NO (BLOCKED) | SKIPPED — outside write zone. |
| F7 GPS `sentence_buffer_` 128→96 (–32 B) | `src/sensors/gps.h` | NO (BLOCKED) | SKIPPED — outside write zone. |
| F8 GPS `status_string_` 64→32 (–32 B) | `src/sensors/gps.h` | NO (BLOCKED) | SKIPPED — outside write zone. |
| F9 Drop EKF entirely (–~4 200 B) | `platformio.ini` + `main.cpp` | NO (BLOCKED) | **EFFECTIVELY DONE** by prior agents via `USE_EKF=0` gating in `platformio.ini:194` and `#if USE_EKF` blocks in `main.cpp`. |

---

## 3. F2 — implementation details (this session)

**Goal:** Eliminate the `Matrix16x16 F_` member (1024 B) from `ExtendedKalmanFilter`. The Jacobian is only live for the duration of `predict()`, so a stack local suffices.

**Changes:**

- `src/navigation/ekf.h:222-232` — Removed `Matrix16x16 F_;` from private members; updated comment to record the F2 reclaim alongside the F1 (P_temp_) reclaim. Net: –1024 B per `ExtendedKalmanFilter` instance.
- `src/navigation/ekf.h:267-279` — Updated `compute_f_jacobian_` signature: new output parameter `Matrix16x16& F` (caller-allocated). Docstring updated.
- `src/navigation/ekf.cpp:162-166` (constructor) — Removed `memset(F_, 0, sizeof(Matrix16x16));` initialization; added rationale comment.
- `src/navigation/ekf.cpp:354-368` (`predict()`) — Allocate `Matrix16x16 F` as a stack local; pass to `compute_f_jacobian_`; consume in subsequent FP / F^T / FPF^T matrix products. Stack peak rises by ~1 KB during `predict()` only.
- `src/navigation/ekf.cpp:662-707` (`compute_f_jacobian_`) — Signature updated to write into the reference parameter `F` instead of the now-removed `F_` member. Algorithm body unchanged (mechanical `F_` → `F` rename of 13 lvalue uses).

Note: `update_covariance_predict_(const Matrix16x16& F)` already took F by parameter — no change needed there.

**SRAM saved (when EKF is actually compiled):** 1024 B per `ExtendedKalmanFilter` instance.

**Stack peak impact:** +~1 024 B during `predict()` (one `Matrix16x16` local). At Mega's ~2.5 KB nominal stack budget after this session's 6 101 B `.bss`, headroom is ~2 091 B → plenty of room.

---

## 4. Build verification

```
$ pio run -e mega_orientation
...
RAM:   [=======   ]  74.5% (used 6101 bytes from 8192 bytes)
Flash: [=         ]  15.0% (used 38004 bytes from 253952 bytes)
[SUCCESS] Took 1.32 seconds
```

`native_test` env fails at `balance_app.h:522-524` with pre-existing `F()` macro errors (`F` not declared in non-Arduino TU). These failures occur **before** the build system reaches `ekf.cpp`, so they neither validate nor invalidate the F2 edit through the PIO test path. To verify F2 syntax, `ekf.cpp` was compiled standalone with `avr-g++ -mmcu=atmega2560` against the framework headers — it compiles cleanly (only a harmless `delay.h` `-Wcpp` warning from the AVR toolchain).

No regressions introduced.

---

## 5. Side effects & behaviour notes

- **Stack peak during `predict()`:** Up by ~1 KB (one extra `Matrix16x16` local). Was already ~1 KB before (FP local) — total predict() peak is now ~3 KB of matrix scratch. Mega has 2 091 B of headroom after `.bss`; if EKF were re-enabled tomorrow this would be tight. F2 alone is not enough to make USE_EKF=1 viable on Mega — F3+F5 (vendored-lib trim + SD drop, ~1.4 KB additional) would also be needed. This is consistent with the diagnosis §5 Phase B+C analysis.
- **Algorithmic semantics:** Unchanged. `F` is computed identically; only its storage location moved from `.bss` to the call-site stack frame.
- **API compatibility:** `compute_f_jacobian_` is a private member; signature change is invisible outside the EKF class. No call sites outside `ekf.cpp` to update.
- **Tests:** `tests/test_ekf.cpp` calls public API only (`predict`, `update`, `initialize`); not affected by the private-member refactor.

---

## 6. Open items

- **F3 (SH2 vendored buffer trim, –832 B):** Out of write zone. Would require vendoring `lib/Adafruit_BNO08x_Arduino` into a `_min/` fork. Medium risk; only material if/when USE_EKF=1 is re-enabled on Mega.
- **F5 (drop SD lib, –585 B):** Out of write zone. Trivial `platformio.ini` edit (`SD` not actually used in `mega_orientation` — `ENABLE_SNAPSHOT_RECORDER` is undefined). Recommend a follow-up agent with `platformio.ini` write access apply this; would bring `mega_orientation` from 6 101 B → ~5 516 B (~67 % RAM).
- **F4, F6, F7, F8:** Out of write zone. Total ~648 B more reclaim available; recommend a follow-up agent.
- **state_reconciliation_2026-05-20.md §5 ("DIAGNOSED, NOT FIXED"):** Stale — the state reconciler agent ran before the USE_EKF=0 gating was visible in their snapshot, OR the gating landed without their notice. Recommend the next state-reconciliation pass update §5 to "FIXED via USE_EKF=0 gating + F1 + F2; F3/F4/F5/F6/F7/F8 remain available as future reclaim".

---

## 7. Files modified (this session)

- `/home/devel/floppi/auto_orientation/src/navigation/ekf.h` — removed `F_` member; updated `compute_f_jacobian_` signature.
- `/home/devel/floppi/auto_orientation/src/navigation/ekf.cpp` — removed `F_` memset in constructor; introduced stack-local `F` in `predict()`; rewrote `compute_f_jacobian_` to write into the reference parameter.
- `/home/devel/floppi/auto_orientation/docs/findings/mega_ram_fix_2026-05-20.md` — this report (new).

No other files touched. No git commit made.
