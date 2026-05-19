# Documentation Audit — 2026-05-19

Read-only audit of `auto_orientation/docs/`. Doc files updated in-place where safe; source files not touched; no git commits made.

---

## 1. Summary

| Metric | Count |
|--------|-------|
| Markdown files audited | 180 (43 active + 137 in `archive/`) |
| Broken relative `.md` links found | 91 |
| Broken links fixed | 87 |
| Broken links left for human review | 4 |
| INDEX.md gaps fixed | 4 entries added to `findings/INDEX.md` |
| Stale "next-session" pointers traced | 9 session records; all actioned or carried forward except 1 stale guidance |
| Phase/date drift fixes | 8 field updates across 4 files |
| Cross-doc contradictions | 4 (1 fixed structurally, 1 history left intact, 1 partially fixed, 1 flagged for human) |
| Duplicate / near-duplicate findings pairs | 3 (no merges per operator policy) |

---

## 2. Broken links

Cause: 2026-05-12 docs reorg moved files into themed subfolders but many inbound links were not updated. Per-file rollup:

| File | Pattern fixed | Count |
|------|---------------|-------|
| `README.md` | flat paths → subfolder paths (BUILD_GUIDE, QUATERNION_API, ARCHITECTURE, MATH_AND_APPLICATIONS, PHASE_1_*, SNAPSHOT, FEATURE_FLAGS, COORDINATE_CONVERSION, `docs/scope.md` → `scope.md`, `docs/features/*` → `guides/ADDING_NEW_SENSORS.md`) | 22 |
| `getting_started/FAQS.md` | `guides/*` → `../guides/*`; `QUICK_START_GETTING_STARTED.md` → `QUICK_START.md`; CALIBRATION_GUIDE → `../calibration/...` | 9 |
| `guides/README.md` | `QUICK_START_GETTING_STARTED.md` → `QUICK_START.md`; `../FAQS.md` → `../getting_started/FAQS.md`; `../ARCHITECTURE.md` → `../getting_started/ARCHITECTURE.md`; CALIBRATION_GUIDE → `../calibration/...` | 24 |
| `guides/FIRST_CALIBRATION.md:10`, `guides/QUICK_START.md:428` | path corrections | 2 |
| `reference/COORDINATE_FRAME_API_REFERENCE.md:642-643` | BUILD_GUIDE_PHASE2 → `../build/...`; PHASE_2_MASTER → `../phases/...` | 2 |
| `reference/BNO085_QUICK_REFERENCE.md:235` | BNO085_ALGORITHM_AND_REPLICATION → `../theory/...` | 1 |
| `reference/GPS_DRIVER_API_REFERENCE.md` | GPS hardware/build/phases path corrections | 6 |
| `hardware/GPS_HARDWARE_SETUP.md`, `hardware/GPS_TROUBLESHOOTING.md` | sibling-folder path corrections | 9 |
| `build/BUILD_GUIDE_PHASE2.md` | sibling-folder path corrections | 9 |
| `testing/TEST_README.md:5,7,9,241` | nonexistent `TESTING_QUICKSTART.md` removed; `docs/integration_test_guide.md` → `integration_test_guide.md`; `INTEGRATION_TEST_SUMMARY.md` → `../archive/...` | 4 |
| `archive/session_records/2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md:36` | `../findings/...` → `../../findings/...` | 1 |

Result: 91 → 4 remaining.

### Left unfixed (4)

| File:line | Target | Why not fixed |
|-----------|--------|---------------|
| `theory/MATH_AND_APPLICATIONS_MASTER_GUIDE.md:80` | `../../../GPS_GEODETIC_COORDINATE_SYSTEMS.md` | "Recommended reading list" pointing at floppi-root docs that do not exist anywhere in the repo. Rewrite needed, not a link fix. |
| `theory/MATH_AND_APPLICATIONS_MASTER_GUIDE.md:104` | `../../../CAMERA_EXTRINSIC_CALIBRATION.md` | Same. |
| `theory/MATH_AND_APPLICATIONS_MASTER_GUIDE.md:128` | `../../../GPS_COORDINATE_QUICK_REFERENCE.md` | Same. |
| `archive/session_records/2026-05-12_evening_balance_iteration.md:90` | `../../../../.claude/projects/.../memory/project_balance_bot_state_2026-05-12.md` | **Intentional** pointer to user's agent memory file; out-of-tree by design. |

---

## 3. INDEX gaps fixed

### `findings/INDEX.md` — added 4 missing entries

These were sitting in `findings/` un-indexed:

- `midrange_balance_gains.md` — indexed under "Diagnosis & tuning", tagged **superseded by BOOTSTRAP**.
- `bno055_latency_and_pitch_fusion.md` — indexed under "Diagnosis & tuning".
- `theoretical_audit_balance_stack.md` — indexed under "Diagnosis & tuning".
- `phase2_characterise_final_plan.md` — new "Phase 2 — CHARACTERISE actuator (planning)" subsection.

`conservative_balance_gains_recommendation.md` was already indexed but its entry now carries the same "superseded by BOOTSTRAP" tag since BOOTSTRAP retired hand-picked-gain recommendations.

### All other folders — 0 gaps

Every other subfolder's `INDEX.md` references every `.md` (and the lone `.ino`) file in its directory.

---

## 4. Stale "next session" items

| Session record | Claim → Status |
|----------------|----------------|
| `2026-05-12_evening_phase4_landing.md` "first 5 s after RUN is bootstrap … don't tune Kp by hand" | **STALE.** Phase 4.10c replaced timer-based bootstrap with pulse-measurement *before* RUN entry. Session record left intact (history), but anyone reading it without also reading the 2026-05-18 PM evening record will form an incorrect mental model. |
| `2026-05-12_drone_vs_bot_cross_project_research.md` § research handoff | Actioned — three research docs landed in `findings/`. |
| `2026-05-12_framework-planning.md` § open work | Actioned — Phases 4.1–4.7 + 4.10 + 4.10c all landed by 2026-05-18 PM evening. |
| `2026-05-12_uno_balancing_hardware.md` § what to try next | Actioned — conservative gains + lenient HELD landed, both since superseded by Phase 2.5/2.6 + BOOTSTRAP. |
| `2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md` § re-plan | **Partially actioned.** 2.1 measured noise-floor landed; per-wheel CHARACTERISE sweep per `phase2_characterise_final_plan.md` did **not** land — still open. |
| `2026-05-18_PHASE2_FLASH_TRIMS_AND_HEURISTICS.md` § Phase 2.7 deferral | Carried forward correctly in `todo.md`. |
| `2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md` § BOOTSTRAP deferral | Actioned same evening. |
| `2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md` § bench validation | Actioned same evening. |
| `2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md` § four open problems | All four reflected in current `todo.md` "Next session (priorities)". |

---

## 5. Date/phase drift fixed

| File | Field | Old | New | Evidence |
|------|-------|-----|-----|----------|
| `roadmap.md:4` | `Last updated` header | `2026-05-18` | `2026-05-19 (doc audit — Phase 4.10c BOOTSTRAP landed)` | commit 7a4d27f |
| `roadmap.md:330` | `Last updated` footer | `2026-05-12` | `2026-05-19 (doc audit)` | same |
| `roadmap.md:21-23` | "Top priority" — described BOOTSTRAP as pending | "is the single highest-leverage piece of remaining work" | "LANDED 2026-05-18 PM evening (commit 7a4d27f)…" + first-bench outcome | session records 2026-05-18_PM_BOOTSTRAP, 2026-05-18_PM_LATE |
| `roadmap.md:60-61` | Phase 4 sub-phases done | "4.10c (designed but not coded)" | "4.10c LANDED 2026-05-18 PM evening" + Phase 2.1/2.5/2.6 added | same |
| `todo.md:308` | `Last updated` | `2026-05-12` | `2026-05-19 (doc audit — Phase 4.10c BOOTSTRAP landed)` | same |
| `todo.md:182-193` | Phase 4.10c row | unchecked | checked, with commit ref + 27/27 test results | same |
| `INDEX.md:72` | `Last updated` footer | `2026-05-12 (late evening — Phase 4 + 4.10 RLS auto-tune landed)` | `2026-05-19 (doc audit; Phase 4.10c BOOTSTRAP landed 2026-05-18 PM evening per 7a4d27f)` | same |
| `findings/INDEX.md:106` | `Last updated` footer | `2026-05-18` | `2026-05-19 (doc audit — indexed 4 missing findings; marked balance-gain recs as superseded)` | this audit |

`scope.md` body date `2026-05-18` left intact — its substantive content (the 21-row violations audit) was last edited that day.

---

## 6. Contradiction sweep

1. **Phase 4.10c status (todo ↔ roadmap ↔ MEMORY).** Roadmap+todo treated BOOTSTRAP as pending; commit 7a4d27f landed it; MEMORY pointers were already correct. **Fixed** in roadmap + todo.
2. **Violations count (session record 19 vs scope.md 21 vs MEMORY 19).** Session record was correct at the time (19 rows). Subsequent BOOTSTRAP session added 2 rows. todo.md "14/21 still open" matches scope.md. **Left as honest history.**
3. **Phase 1 test counts (README 113 vs scope/roadmap 143+ vs current 123 native + 27 BOOTSTRAP).** README.md is stuck on the Phase 1 era. **Left for a human-driven README rewrite** — single number changes won't fix the section's framing.
4. **Findings gain-recommendation docs vs new BOOTSTRAP reality.** `conservative_balance_gains_recommendation.md` and `midrange_balance_gains.md` still read as if they're current advice. **Partially fixed** by annotating both as superseded in `findings/INDEX.md`. Files themselves left intact per "keep history" policy.

**Most important for the next coding session**: the `2026-05-12_evening_phase4_landing.md` "next session" advice block describes a timer-based bootstrap that no longer exists. Phase 4.10c (2026-05-18 PM evening) replaced it with a pulse-measurement bootstrap that runs *before* RUN entry. Anyone consulting that session record alone will form an incorrect mental model of how the bot starts.

---

## 7. Duplicate / near-duplicate findings

| Pair | Overlap | Recommendation |
|------|---------|----------------|
| `findings/CALIBRATION-IMPLEMENTATION-STATUS.md` + `findings/calibration-implementation-guide.md` + `calibration/CALIBRATION_IMPLEMENTATION_GUIDE.md` | All three describe BNO085 calibration persistence implementation, written within days of each other. `calibration/` copy reads canonical; `findings/` copies look like superseded research notes. | Merge the two `findings/` versions into a single research-notes doc. Not done — operator's "keep history" policy. |
| `findings/conservative_balance_gains_recommendation.md` + `findings/midrange_balance_gains.md` | Both prescribe Kp/Ki/Kd for the same chassis; both obsolete after Phase 4.10c. | Tagged superseded in INDEX. A future "move to archive" pass would be cleaner. |
| `findings/balance_held_fallen_state_machine.md` + `findings/multi_axis_anomaly_handling_detection.md` + `findings/research_motor_null_space_handling_detection.md` | Sequential design iterations for the HELD/INFLUENCED/STUCK detector. | **Not duplicates** — iterative refinement. Cross-references are already adequate. |

---

## 8. Files modified

```
auto_orientation/docs/INDEX.md
auto_orientation/docs/README.md
auto_orientation/docs/roadmap.md
auto_orientation/docs/todo.md
auto_orientation/docs/findings/INDEX.md
auto_orientation/docs/findings/audit_documentation_2026-05-19.md  (this file)
auto_orientation/docs/getting_started/FAQS.md
auto_orientation/docs/guides/FIRST_CALIBRATION.md
auto_orientation/docs/guides/QUICK_START.md
auto_orientation/docs/guides/README.md
auto_orientation/docs/reference/BNO085_QUICK_REFERENCE.md
auto_orientation/docs/reference/COORDINATE_FRAME_API_REFERENCE.md
auto_orientation/docs/reference/GPS_DRIVER_API_REFERENCE.md
auto_orientation/docs/hardware/GPS_HARDWARE_SETUP.md
auto_orientation/docs/hardware/GPS_TROUBLESHOOTING.md
auto_orientation/docs/build/BUILD_GUIDE_PHASE2.md
auto_orientation/docs/testing/TEST_README.md
auto_orientation/docs/archive/session_records/2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md
```

18 files modified (17 edited + this report).

---

*Generated 2026-05-19 by documentation audit. No source files touched; no git commits made.*
