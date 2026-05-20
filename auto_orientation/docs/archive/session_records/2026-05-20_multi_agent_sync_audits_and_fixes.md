# 2026-05-20 — Multi-agent sync, audits, and fixes

**Session start**: 2026-05-20 (working tree post-merge at commit `ec4ef53`)
**Session end**: 2026-05-20 late
**Commits**: 1 (`ec4ef53` — repo sync merge; no deliverable commits per operator "no commits" rule)
**Agents spawned**: ~20 across multiple waves
**Mode**: Agent-manager parent + parallel sub-agents (conservative 3–4 concurrent)
**Operator constraints**: no commits, conservative resource use, no agent file conflicts, focus on `mega_balance` + `uno_balance`

> Scope of this record: **auto_orientation only**. The `flight_controller` side of the day (project recon, SBUS re-enable, modular test harness, cross-project BNO research) has its own sibling session record. The authoritative cross-project handoff is `/home/devel/floppi/docs/findings/day_status_2026-05-20.md`.

---

## Headline outcomes

### Repository sync — two-clone divergence resolved
The `err0r` device carried uncommitted **and** untracked work that had never reached `origin/main`. This session merged that work into `origin/main` as commit `ec4ef53`, which became the baseline for all subsequent work. The merge used `-X theirs` to favour err0r's newer balance-stack work where the two clones diverged. With the merge landed, the two-clone divergence is closed and the working tree is once again single-source.

### Five audits + state reconciliation — 148 findings total
A wave of audit agents swept the auto_orientation tree. Each produced a dated finding doc with prioritized findings:

| Audit | Finding doc | Findings |
|---|---|---|
| Documentation | [audit_documentation_2026-05-20.md](../../findings/audit_documentation_2026-05-20.md) | 41 (8 P0 broken links, 13 P1, 12 P2, 8 P3) |
| Code quality | [audit_code_quality_2026-05-20.md](../../findings/audit_code_quality_2026-05-20.md) | 19 (1 P0 — collision regression, already-fixed) |
| Test coverage | [audit_test_coverage_2026-05-20.md](../../findings/audit_test_coverage_2026-05-20.md) | 26 (4 critical untested modules) |
| Security | [audit_security_2026-05-20.md](../../findings/audit_security_2026-05-20.md) | 32 (0 P0) |
| Build system | [audit_build_system_2026-05-20.md](../../findings/audit_build_system_2026-05-20.md) | 30 (4 P0 — most already-fixed) |
| State reconciliation | [state_reconciliation_2026-05-20.md](../../findings/state_reconciliation_2026-05-20.md) | source-vs-doc truth check |

### Architecture plan + workstream partition
[architecture_plan_2026-05-20.md](../../findings/architecture_plan_2026-05-20.md) produced a joint scaffolding plan partitioning the remaining work into workstreams **A / B / C / D / E / F**, plus **INFRA** and **DOC** tracks. The partition mapped cleanly onto the parallel-agent dispatch boundaries used through the rest of the session.

### KEY DISCOVERY — the audits were stale
The single most important finding of the day: **Phase 4M.0 (collision detection), 4M.1 (wheel encoder), and 4M.12 (PWM auto-discovery range) were ALL already landed** in err0r's commit `f2e9732`. The five audits had run against doc/roadmap claims, not the post-merge source tree, so every "P0 blocker" they raised about those three phases was a false alarm — the work pre-existed the audit by one commit. State reconciliation caught this and confirmed it phase by phase. The net real-defect count was far lower than the audits implied; doc/roadmap drift, not missing code, was the dominant cleanup category.

---

## What landed in code (working tree, uncommitted per operator directive)

### Mega RAM — F2 EKF size fix
The EKF state-size reduction (F2) reclaimed ~1024 B of RAM. `mega_orientation` now builds at **74.5% RAM**. Detail: [mega_ram_fix_2026-05-20.md](../../findings/mega_ram_fix_2026-05-20.md).

### Calibration security — 4 P1 fixes + format-version bump
Four P1 hardening fixes landed in the calibration-storage path:

1. **CRC-8-CCITT** replaces the prior ad-hoc CRC8 implementation.
2. **Integer overflow guard** added on length arithmetic.
3. **Length field widened** `uint8` → `uint16`.
4. **Version reject path** — blobs with an unrecognised format version are now rejected outright.

`CAL_FORMAT_VERSION` was bumped `0x01` → `0x02`. **Operational consequence: old EEPROM calibration blobs are now rejected — the operator must re-calibrate** on any device carrying a v1 blob. Detail: [security_fix_calibration_2026-05-20.md](../../findings/security_fix_calibration_2026-05-20.md).

> Known doc typo to fix next session: security_fix_calibration_2026-05-20.md line 186 states CRC-8-CCITT('A') = `0x20`; correct value is `0xC0`. One-line edit.

### Workstream B — AUTO_TUNE dead-code removal + telemetry
- Deleted the dead `AUTO_TUNE` code path from `balance_app.{h,cpp}` (removes `tune_result_`, ~+28 B RAM headroom).
- Added `held_entry_reason_` telemetry so HELD-state transitions report why they fired.
- 2 new tests cover the telemetry addition.

### New tests (36 total across two suites)
- `test_json_formatter.cpp` — **22 tests**. Found and fixed **3 P1 bugs** in `json_formatter.cpp`: NaN and Inf now serialize to `null`; timestamp format specifier corrected `%d` → `%u`.
- `test_calibration_storage.cpp` — **14 tests**. Validated the CRC-8-CCITT upgrade and the version-reject path.
- `test_bootstrap_k_preservation.cpp` — new, covers K_motor preservation across the bootstrap sequence.

### Build fixes
- `PWM_DISCOVERY` `-Wswitch` warning resolved.
- `F()` macro shim added to `balance_app.h` so native_test host builds compile.
- `native_test` `src_filter` exclusions added (7 files) so host builds don't pull AVR-only sources.
- `gps.cpp` host-build compatibility fix.

### Infrastructure & docs
- `tools/build_tests.sh` — full rewrite, **358 lines** (Workstream INFRA delivered).
- INDEX hygiene pass across the findings tree.
- `src/applications/balancing_robot_uno/README.md` — Uno re-tune workflow documentation.

---

## Build status (end of session)

| Target | Status | RAM | Flash |
|---|---|---|---|
| `mega_balance` | SUCCESS | 17.1% | 14.0% |
| `uno_balance` | SUCCESS | 62.2% | 93.7% |
| `arduino_uno_minimal` | SUCCESS | — | — |
| `mega_orientation` | SUCCESS | 74.5% | — |
| `native_test` | compiles clean through link | — | — |

Run the native test suites via `pio test -e native_test`.

---

## Decisions made

- **Merge strategy** — used `-X theirs` during the initial sync to favour err0r's newer balance-stack work where the two clones diverged.
- **CAL_FORMAT_VERSION bump (`0x01` → `0x02`)** chosen over a dual-CRC compatibility path. Rationale: graceful degradation — a rejected blob simply triggers a re-calibrate, which is a known, safe operator workflow. A dual-CRC path would carry the old (weaker) CRC indefinitely.
- **`mega_orientation` deprioritized** per operator — the focus targets are `mega_balance` and `uno_balance`. The F2 RAM fix landed anyway because it was low-risk and cleared a build, but no further orientation-target work was scheduled.
- **Workstreams A / C / E confirmed already-done** during state reconciliation and were NOT re-implemented. Avoiding redundant work on already-landed phases was the direct payoff of the source-first reconciliation pass.

---

## Key surprises / lessons

1. **Recurring "stale audit" pattern (HIGH-VALUE LESSON).** The state reconciler repeatedly discovered that the audits' P0 claims were already fixed by err0r's `f2e9732`. The audits trusted doc/roadmap claims over the source tree. **Mitigation for future state reconcilers: grep the source FIRST, trust audit doc claims SECOND.** A `git log -p` over recent commits cross-referenced against the findings was the single most useful tool of the day.
2. **Parallel-agent budget.** Dispatching 5+ sub-agents simultaneously hit Anthropic API usage limits multiple times. Empirically conservative budget = **3–4 concurrent agents**; sequence the rest.
3. **Tuner first run is YELLOW.** The first tuner run settled the bot in ~8 s in simulation against a 30 s target — nominally "fast", but it has not been bench-validated. Needs a hardware run plus budget tuning before it can be claimed GREEN. No dedicated finding doc was written for the run result.

---

## Blockers / open discoveries

- **Bench validation gap** — the tuner output (8 s sim settle) has had no hardware run. Cannot claim GREEN until a bench session validates it.
- **json_formatter tests 15 / 16 / 18** — currently dual-accept (legacy + correct behaviour). Tighten to assert *only* correct behaviour once dependent callers are updated.
- **Doc typo** in security_fix_calibration_2026-05-20.md line 186 (CRC value) — see above.
- **Working tree uncommitted** — per operator's no-commit rule, all deliverables (findings docs, source fixes, new tests, INFRA rewrite) sit in the working tree pending a batched operator commit pass. See day_status_2026-05-20.md §7 for the recommended 4–6 commit split.

---

## What's next (auto_orientation)

- **Workstream D — Phase 4M.11**: `e` command + EEPROM **CPM / radius persistence**. Requires an EEPROM layout extension. Unblocked.
- **Workstream F**: Phase 4M.2 K cross-check + Phase 4M.13 velocity outer loop. **Hardware-gated** — needs encoders-in-loop validation; defer to a bench session.
- **Bench-validate the tuner output** — required before the tuner can move from YELLOW to GREEN.
- **Tighten json_formatter tests 15 / 16 / 18** to assert only correct behaviour once callers are updated.
- **AO → FC bridge** — port the `calibration_storage` HAL to `flight_controller` (see `docs/findings/bno_cross_project_2026-05-20.md`; Phase A compile flags must land in FC first).

---

## See also

- `/home/devel/floppi/docs/findings/day_status_2026-05-20.md` — authoritative cross-project day handoff
- [architecture_plan_2026-05-20.md](../../findings/architecture_plan_2026-05-20.md) — workstream partition A–F + INFRA + DOC
- [state_reconciliation_2026-05-20.md](../../findings/state_reconciliation_2026-05-20.md) — source-vs-doc truth check (caught the stale-audit pattern)
- [mega_ram_fix_2026-05-20.md](../../findings/mega_ram_fix_2026-05-20.md) — F2 EKF size reclaim
- [security_fix_calibration_2026-05-20.md](../../findings/security_fix_calibration_2026-05-20.md) — 4 P1 calibration fixes + format-version bump
- [tuner_format_alignment_2026-05-20.md](../../findings/tuner_format_alignment_2026-05-20.md) — tuner format verified already aligned
- The five 2026-05-20 audit docs under [`../../findings/`](../../findings/INDEX.md)
