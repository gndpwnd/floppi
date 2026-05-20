# Tuner ↔ Uno consumer format alignment — 2026-05-20

**Agent:** tuner-format-aligner@floppi:1
**Bug reference:** `docs/findings/state_reconciliation_2026-05-20.md` §"Tuner workflow broken" (lines 318–319, 381–386)

## TL;DR

**No code changes were necessary.** The bug described in the state-reconciliation report is **stale** — the format mismatch was already fixed in the working tree before this agent ran. The Python tuner template, the generated header, and the C++ consumer all agree on the `static const float BALANCE_KP …` file-scope convention. A clean `pio run -e uno_balance` build succeeds.

## Decision rationale (which path would have been taken)

Had the bug been live, I would have chosen **Path B (fix the generator)**:

- The consumer (`uno_balance_app.cpp`, `main.cpp`) is wired through 9 file-scope constants in 12+ call sites; the template is one ~100-line file with 9 placeholders.
- `docs/applications/balancing_robot_uno/README.md` §Troubleshooting explicitly enshrines the file-scope schema as the contract: "Confirm the generated header defines `BALANCE_KP`, … at file scope (**not inside a namespace**)". The README is documentation-as-source-of-truth here.
- `balance_constants.h`'s own docblock (lines 17–24) names itself "the contract between the offline Python brute-force tuner and the on-MCU minimal balance application" — i.e. the consumer side is the canonical schema; the generator must match it.
- Path B is also the option the previous fixer chose (see below), so my choice is consistent with the codebase's recent direction.

## What I observed in the tree

State of the four artifacts at the time of this audit:

| Artifact | Path | Schema emitted/expected |
|---|---|---|
| Tuner template | `auto_orientation/tools/sim/balance_constants_template.h.in` | file-scope `static const float BALANCE_KP = …;` (lines 55–98) |
| Generated header | `auto_orientation/src/applications/balancing_robot_uno/balance_constants.h` | file-scope `static const float BALANCE_KP = 94.4873f;` (lines 55–98) |
| Consumer (impl) | `auto_orientation/src/applications/balancing_robot_uno/uno_balance_app.cpp` | references `BALANCE_KP`, `BALANCE_KI`, `BALANCE_KD`, `PITCH_OFFSET_DEG`, `PID_SAMPLE_MS`, `PWM_MIN`, `PWM_MAX`, `TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG` (no namespace qualifier) |
| Consumer (main) | `auto_orientation/src/applications/balancing_robot_uno/main.cpp` | references `STICTION_PWM`, `PWM_MIN`, `PWM_MAX`, `BALANCE_KP/KI/KD`, `PITCH_OFFSET_DEG`, `PID_SAMPLE_MS` |

Symbol intersection check:

```
consumer symbols       = {BALANCE_KD, BALANCE_KI, BALANCE_KP, PID_SAMPLE_MS,
                          PITCH_OFFSET_DEG, PITCH_SANITY_DEG, PWM_MAX, PWM_MIN,
                          STICTION_PWM, TIP_CUTOFF_DEG}
template symbols       = same set (STICTION_PWM also emitted)
namespace balance grep = no matches in either tree
```

Schemas match exactly. Build verifies this empirically.

## Exact edits made

**None.** No files were modified. Write-zone untouched.

(For posterity: the fix that resolved the reconciler's reported bug appears to have landed in commit `f2e9732` — "save: err0r device WIP — Phase 4M.12 + 4M.11 + research/findings pre-sync 2026-05-20" — which is the most recent commit touching the relevant files. The `state_reconciliation_2026-05-20.md` snapshot was taken before that commit was reconciled into HEAD.)

## Verification status

**PASS.** Ran `pio run -e uno_balance` from `/home/devel/floppi/auto_orientation` (the production env that consumes `balance_constants.h`):

```
Linking .pio/build/uno_balance/firmware.elf
Checking size .pio/build/uno_balance/firmware.elf
RAM:   [=======   ]  65.8% (used 1347 bytes from 2048 bytes)
Flash: [==========]  98.5% (used 31774 bytes from 32256 bytes)
Building .pio/build/uno_balance/firmware.hex
========================= [SUCCESS] Took 12.87 seconds =========================
```

Flash is at 98.5% — that's a separate concern (tracked elsewhere) but the build itself is clean. Both `uno_balance_app.cpp` and `main.cpp` compiled, link succeeded, no undefined-symbol errors against `balance_constants.h`.

## Side effects / observations

1. **Stale doc in `tools/sim/README.md`.** That README still shows an old example header snippet using `namespace balance { … }` (around line 52). It's outside my write zone (and outside both Path A and Path B), so I did not edit it, but it should be brought into line with the real template in a future doc-cleanup pass — it will mislead anyone reading it.
2. **Stale finding in `state_reconciliation_2026-05-20.md`.** The "tuner workflow broken" entries (lines 318–319, 381–386) are no longer accurate. A future reconciler agent should re-scan and retract them.
3. **Tuner emits one extra symbol (`STICTION_PWM`) that `uno_balance_app.{h,cpp}` does not consume directly** — it's consumed by `main.cpp` (passed to the `L298NMotorDriver` constructor). Not a problem, just noting the wiring.
4. **No risk taken on token budget** — the analysis was entirely read-only plus a single ~13 s build. No edits, no churn, no second-order changes to chase.
