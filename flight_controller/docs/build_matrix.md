# Build Matrix

> Last updated: 2026-05-27 (post-audit SERVO_COUNT fix — wave 6 fc-W8)
> Status: Active — updated per session

---

## Purpose

This matrix tracks which PlatformIO environments (and any associated `USE_*` build flag combinations) have been **actually built and verified** in the most recent multi-agent session vs. those that remain **unverified this session**. The goal is to keep operator and agent expectations honest: green elsewhere in the docs (scope, roadmap, session synthesis) describes intent; this table describes what has been compiled on this checkout, with size deltas and warnings, in the most recent wave.

Source of truth for the current row of "verified" results is the most recent session record under `docs/archive/session_records/`. Older results are not carried forward — re-verify, then update this table.

## Current matrix (as of 2026-05-27 wave 6 fc-W8 — post-audit SERVO_COUNT fix)

**Recent changes:** 2026-05-27: esp32 + esp32_calibration re-verified after post-audit SERVO_COUNT default 5 fix; teensy40 re-verified unchanged.

| Env | Last verified | Flash | RAM | Warnings | Notes |
|---|---|---|---|---|---|
| `esp32` | 2026-05-27 | 581,053 B (44.3%) | 35,676 B (10.9%) | none | post-audit SERVO_COUNT fix landed; SERVO `#error` guards on `pin_definitions_esp32.h` prevent silent LEDC double-claim. |
| `teensy40` | 2026-05-27 | code 26,372 B + data 7,352 B | RAM1 vars 9,248 B | none | unaffected by SERVO fix (Teensy uses `pin_definitions.h`, not `pin_definitions_esp32.h`). |
| `esp32_calibration` | 2026-05-27 | 628,425 B (47.9%) | 35,940 B (11.0%) | none | post-audit SERVO_COUNT fix landed; SERVO `#error` guards on `pin_definitions_esp32.h` prevent silent LEDC double-claim. |
| `teensy41` | unverified this session | — | — | — | Defined in `platformio.ini`; not built in wave 6. Last broad coverage: 2026-05-20 project_recon (10/10 envs green pre-`USE_GPS`). |
| `teensy36` | unverified this session | — | — | — | Defined in `platformio.ini` (legacy support); not built in wave 6. |
| `teensy40_calibration` | unverified this session | — | — | — | Defined in `platformio.ini`; not built in wave 6. Hardware-validate the calibration-storage restore-on-boot path here (see 2026-05-26 §9). |
| `teensy41_calibration` | unverified this session | — | — | — | Defined in `platformio.ini`; not built in wave 6. |
| `teensy36_calibration` | unverified this session | — | — | — | Defined in `platformio.ini`; not built in wave 6. |
| `esp32s3` | unverified this session | — | — | — | Defined in `platformio.ini`; not built in wave 6. |
| `esp32s3_calibration` | unverified this session | — | — | — | Defined in `platformio.ini`; not built in wave 6. |

**Counts:** 3 verified this session / 7 unverified this session / 10 envs total.

"Unverified this session" does not mean broken — most of these envs were green in earlier sessions (see `findings/project_recon_2026-05-20.md` for the 10/10 pre-`USE_GPS` baseline and `findings/qa_review_2026-05-22.md` for the 3/3 + flag combos green at that gate). It means the current working tree has not been re-built against them after the wave-6 calibration_storage port, so the size/warning columns are intentionally empty rather than carried forward from a stale baseline.

## How to add an env to this matrix

When you add a new PlatformIO environment to `platformio.ini` (or land a new `USE_*` build-flag combination worth tracking as its own row), build it locally with `pio run -e <env>`, record the resulting flash / RAM numbers and any new warnings, and add a row above. Reference the session record that contains the build run in the **Last verified** column, not just the date — `2026-05-26 (wave 6)` is more useful than a bare date. If a previously-verified env is touched by a change but not re-built, demote it to **unverified this session** rather than carrying old numbers forward; the whole point of this table is to stay honest about what was actually compiled on the current tree.
