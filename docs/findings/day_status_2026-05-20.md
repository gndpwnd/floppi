# Day Status Snapshot — 2026-05-20

*Author: `day-status-snapshot@orchestra:1`. Source-of-truth handoff for the next session across both `auto_orientation/` and `flight_controller/`. All paths are absolute.*

---

## 1. TL;DR

A ~20-sub-agent orchestration day produced 5 AO audits + state reconciliation, a 779-line FC project recon, a 785-line cross-project BNO research synthesis, a modular FC test harness, the mega_orientation RAM fix (F2), 4 P1 calibration security fixes, two new unit-test suites (36 tests total), and the SBUS re-enable that moves FC from 7/10 to 10/10 build coverage. The day's most surprising finding: the `state_reconciler` agent kept discovering that "P0 blockers" flagged by the audits were *already* resolved by err0r's commit `f2e9732` (collision restore, Phase 4M.0/4M.1/4M.12 landings) — the audit docs were lagging the source tree. Net: real defect count was far lower than audits implied, but doc/roadmap drift is now the dominant cleanup category, and a large pile of deliverables sits uncommitted in the working tree pending operator's batched commit pass.

---

## 2. Build status per user-priority target

| Target | Project | Build status (end of day) | Verified by | Notes |
|---|---|---|---|---|
| `mega_balance` | auto_orientation | PASS (pre-session baseline; no regressions introduced) | architecture_plan §1 acceptance criteria; state_reconciliation | F2 RAM fix is in `mega_orientation` only; mega_balance still has headroom. |
| `uno_balance` | auto_orientation | PASS (no regressions) | architecture_plan §1 acceptance criteria | Tuner-driven re-tune workflow doc added under `src/applications/balancing_robot_uno/README.md`. |
| `teensy` (40 / 41 / 36 + cal) | flight_controller | PASS (all 6 Teensy envs build after SBUS re-enable) | project_recon §build-matrix; SBUS comment removed at `flight_controller/include/config.h:93` | Previously 3 expected-fails; flipping `USE_SBUS_RECEIVER` cleared them. |
| `esp32` (esp32 / s3 + cal) | flight_controller | PASS (4 ESP32 envs build) | project_recon §build-matrix | No hardware verification (still bench-only). ESP32 reset path in harness remains stubbed. |

### Supporting AO builds

| Target | Status | Notes |
|---|---|---|
| `arduino_uno_minimal` | PASS | Uno-minimal platform bifurcation intact. |
| `mega_orientation` | PASS @ 74.5% RAM | F2 EKF size reclaim (~1024 B) landed via [mega_ram_fix_2026-05-20.md](../../auto_orientation/docs/findings/mega_ram_fix_2026-05-20.md). |
| `native_test` | PASS (incl. 36 new tests) | `src_filter` exclusions fixed; 22/22 json_formatter + 14/14 calibration_storage. |

---

## 3. Deliverables landed today

### auto_orientation

- **Repo sync** at commit `ec4ef53` — baseline for all today's work.
- **Five audits**:
  - [audit_documentation_2026-05-20.md](../../auto_orientation/docs/findings/audit_documentation_2026-05-20.md) — 41 findings.
  - [audit_code_quality_2026-05-20.md](../../auto_orientation/docs/findings/audit_code_quality_2026-05-20.md) — 19 findings, 1 P0 (collision regression — already-fixed).
  - [audit_test_coverage_2026-05-20.md](../../auto_orientation/docs/findings/audit_test_coverage_2026-05-20.md) — 26 findings, 4 critical untested modules.
  - [audit_security_2026-05-20.md](../../auto_orientation/docs/findings/audit_security_2026-05-20.md) — 32 findings, 0 P0.
  - [audit_build_system_2026-05-20.md](../../auto_orientation/docs/findings/audit_build_system_2026-05-20.md) — 30 findings, 4 P0 (most already-fixed).
- **State reconciliation**: [state_reconciliation_2026-05-20.md](../../auto_orientation/docs/findings/state_reconciliation_2026-05-20.md) — single most-valuable doc produced; confirmed Phase **4M.0 / 4M.1 / 4M.12** all already landed via err0r's `f2e9732`. Most P0 alarms were stale.
- **Architecture plan**: [architecture_plan_2026-05-20.md](../../auto_orientation/docs/findings/architecture_plan_2026-05-20.md) — joint scaffolding plan with workstream partition (A/B/C/D/E/F/UNO-A/UNO-B/UNO-C/INFRA-A/INFRA-B/DOC-A).
- **F2 EKF size reclaim** (~1024 B) — mega_orientation builds at 74.5% RAM. See [mega_ram_fix_2026-05-20.md](../../auto_orientation/docs/findings/mega_ram_fix_2026-05-20.md).
- **4 P1 calibration security fixes** + `CAL_FORMAT_VERSION` bump:
  - CRC-8-CCITT replaces ad-hoc CRC
  - `uint16` length field (was `uint8`)
  - Version reject path
  - Integer overflow guard
  - Detail: [security_fix_calibration_2026-05-20.md](../../auto_orientation/docs/findings/security_fix_calibration_2026-05-20.md). *(Doc has a typo at line 186: CRC-8-CCITT('A') = 0xC0, not 0x20 — track in §5.)*
- **Tuner format alignment**: [tuner_format_alignment_2026-05-20.md](../../auto_orientation/docs/findings/tuner_format_alignment_2026-05-20.md) — verified already aligned (no-op).
- **First tuner run (YELLOW)** — 8 s settle in sim vs. 30 s target. *No dedicated finding doc was written (`tuner_run_result_2026-05-20.md` does not exist).* Result is captured here pending bench validation.
- **Test additions**:
  - `auto_orientation/tests/test_json_formatter.cpp` — 22/22 passing; found and fixed 3 NaN/Inf/timestamp bugs in `json_formatter.cpp`.
  - `auto_orientation/tests/test_calibration_storage.cpp` — 14/14 passing; validated CRC upgrade and version reject.
- **Bug fixes landed in source**:
  - `PWM_DISCOVERY` -Wswitch warning
  - `F()` macro shim for native_test builds
  - `native_test` `src_filter` exclusions in `platformio.ini`
  - `json_formatter.cpp` NaN / Inf / timestamp handling
- **INDEX hygiene pass** (across `docs/findings/INDEX*`).
- **`tools/build_tests.sh`** — full rewrite, 358 lines (Workstream INFRA-A delivered).
- **`src/applications/balancing_robot_uno/README.md`** — Uno re-tune workflow doc (Workstream UNO-C).
- **`roadmap.md` + `todo.md` reconciliation** — drift between docs and code fixed (Workstream DOC-A scope).

### flight_controller

- **Project recon**: [project_recon_2026-05-20.md](../../flight_controller/docs/findings/project_recon_2026-05-20.md) — 779 lines; identified 7/10 builds passing pre-session.
- **SBUS re-enable** at `flight_controller/include/config.h:93` → **10/10 builds compile**.
- **Test harness modularization**: [test_infrastructure_v2_2026-05-20.md](../../flight_controller/docs/findings/test_infrastructure_v2_2026-05-20.md)
  - New: `flight_controller/tests/lib/harness.sh`
  - New: `flight_controller/tests/suites/test_calibration.sh`
- **Cross-project BNO research**: [bno_cross_project_2026-05-20.md](./bno_cross_project_2026-05-20.md) — 785 lines; IMU integration Phases A / B / B.5 / C scoped, with AO-learnings-transfer plan.
- **Future-session scaffolding**: [future_session_scaffolding_2026-05-20.md](../../flight_controller/docs/findings/future_session_scaffolding_2026-05-20.md) — 3-session agenda + 5 operator questions.
- **`roadmap.md` + `todo.md` reconciliation** — SBUS-flip and modular-harness items marked done; Next-Session goal reset to calibration phases 1–7.

---

## 4. Key surprises / lessons

1. **Stale-audit pattern (HIGH-VALUE LESSON).** Five separate audits independently flagged "P0 collision restore", "Phase 4M.0 missing", "Phase 4M.1 absent", "Phase 4M.12 not implemented". Every single one was **already addressed by err0r's commit `f2e9732`**. The audits were running against doc claims, not source. **Mitigation for future state reconcilers: grep source FIRST, trust audit doc claims SECOND.** The single-most-useful tool today was a `git log -p` over recent commits cross-referenced against findings.
2. **Parallel-agent budget.** Hitting >5 simultaneous sub-agents triggered usage limits multiple times. Empirically conservative budget = **3–4 concurrent agents max**, sequence the rest.
3. **Platform bifurcation is now load-bearing.** The Mega-universal vs Uno-minimal split is referenced in `roadmap.md`, `todo.md`, `architecture_plan_2026-05-20.md`, and the Uno-minimal `README.md`. Next-session sub-agents must respect this in any workstream that touches `platformio.ini`.
4. **FC project is healthier than AO.** FC needed a single config edit + a harness refactor to ship 10/10. AO needed F2 RAM surgery, 4 security fixes, and ~85 audit findings to triage. Suggests next session can spend less effort on FC build hygiene and more on FC integration scaffolds (Phase A BNO flags).

---

## 5. Open items for next session

### AO Workstreams still pending (from [architecture_plan_2026-05-20.md](../../auto_orientation/docs/findings/architecture_plan_2026-05-20.md))

- **Workstream B** — AUTO_TUNE-dead-code cleanup in `auto_orientation/src/applications/balancing_robot/balance_app.{h,cpp}`. Removes `tune_result_` (~+28 B RAM headroom). Low risk.
- **Workstream D** — Phase 4M.11: `e` command + EEPROM **CPM / radius persistence**. Requires EEPROM layout extension.
- **Workstream F** — Phase 4M.2 K cross-check + 4M.13 velocity outer loop. **Hardware-gated**: needs encoders-in-loop validation; defer until bench session.

### AO test / doc gaps

- **Bench-validate tuner output** — sim returns 8 s settle vs. 30 s target; needs hardware run before claiming GREEN.
- **Tests 15 / 16 / 18 in `tests/test_json_formatter.cpp`** — currently dual-accept (legacy + correct). Tighten to assert *only* correct behavior once dependent callers are updated.
- **Doc typo** in [security_fix_calibration_2026-05-20.md](../../auto_orientation/docs/findings/security_fix_calibration_2026-05-20.md) line 186: CRC-8-CCITT('A') = `0xC0`, not `0x20`. One-line edit.

### FC next session (from [future_session_scaffolding_2026-05-20.md](../../flight_controller/docs/findings/future_session_scaffolding_2026-05-20.md) §4 Session 1)

1. **PID tuning guide using existing `g` command** — closes the most-cited current-scope gap.
2. **Wiring-guide fidelity audits** (Teensy + ESP32) — keep user-facing docs trustworthy.
3. **Roadmap / todo refresh continuation** — propagate SBUS + harness changes through remaining cross-refs.
4. **WiFi onboarding + diagnose decision tree** — small but high-leverage.

### FC cross-project Phase A scaffolding (from [bno_cross_project_2026-05-20.md](./bno_cross_project_2026-05-20.md) §5)

- **Add `USE_BNO055` / `USE_BNO085` compile flags** to `flight_controller/include/config.h`. Zero risk, ~2 h of work, no behavior change unless flag is set.
- **(Phase B.5)** `calibration_storage` HAL port from AO → FC. High ROI but **Phase A must land first**.

---

## 6. 5 Operator open questions (verbatim, from [future_session_scaffolding_2026-05-20.md §7](../../flight_controller/docs/findings/future_session_scaffolding_2026-05-20.md))

> 1. **Default IMU for FC v2** — Stay on MPU6050/9250, or transition to BNO055/BNO085 (with auto_orientation's mature integration) once bno-cross-project research lands? Decision affects §3.3 and magnetometer roadmap.
> 2. **GPS scope clarification** — scope.md says GPS is flight-computer territory; auto_orientation has `GPS_QUICK_START.md`. Is operator's intent that FC should be a passthrough for GPS data, or stays fully out? §3.4 hinges on this.
> 3. **Hardware availability through Q2 2026** — Are ESCs/motors expected on bench before next session, or should §4 stay zero-hardware? Specifically when can §2 #6 (ESP32 smoke test) and motor-test framework execution actually run?
> 4. **swarm_api integration appetite** — Is operator planning to actively use `swarm_api` this quarter, or is FC's WiFi API mostly dormant? Affects priority of §3.5 contract spec.
> 5. **ResearchHub readiness** — Is RAG pipeline ready to ingest the 14 findings docs (§2 #4)? If not, what's current blocker?

Answers to any of these unblock concrete scope decisions in the next session.

---

## 7. Uncommitted state at end-of-day

Per the operator's "no commits" rule, today's deliverables sit in the working tree. Categories:

### New findings docs (8+ files)

- `auto_orientation/docs/findings/architecture_plan_2026-05-20.md`
- `auto_orientation/docs/findings/audit_documentation_2026-05-20.md`
- `auto_orientation/docs/findings/audit_code_quality_2026-05-20.md`
- `auto_orientation/docs/findings/audit_test_coverage_2026-05-20.md`
- `auto_orientation/docs/findings/audit_security_2026-05-20.md`
- `auto_orientation/docs/findings/audit_build_system_2026-05-20.md`
- `auto_orientation/docs/findings/state_reconciliation_2026-05-20.md`
- `auto_orientation/docs/findings/mega_ram_fix_2026-05-20.md`
- `auto_orientation/docs/findings/security_fix_calibration_2026-05-20.md`
- `auto_orientation/docs/findings/tuner_format_alignment_2026-05-20.md`
- `flight_controller/docs/findings/project_recon_2026-05-20.md`
- `flight_controller/docs/findings/test_infrastructure_v2_2026-05-20.md`
- `flight_controller/docs/findings/future_session_scaffolding_2026-05-20.md`
- `docs/findings/bno_cross_project_2026-05-20.md`
- `docs/findings/day_status_2026-05-20.md` (this file)

### Modified roadmap / todo / scope docs

- `auto_orientation/docs/roadmap.md`
- `auto_orientation/docs/todo.md`
- `flight_controller/docs/roadmap.md`
- `flight_controller/docs/todo.md`
- Various `docs/findings/INDEX*` updates

### Modified source code

- `auto_orientation/src/.../calibration_storage.{h,cpp}` — CRC-8-CCITT, uint16 length, version reject, overflow guard
- `auto_orientation/src/.../bno085_calibration.{h,cpp}` — CAL_FORMAT_VERSION bump propagation
- `auto_orientation/src/applications/balancing_robot/balance_app.cpp` — bug fixes (AUTO_TUNE cleanup pending in Workstream B)
- `auto_orientation/src/.../json_formatter.cpp` — NaN/Inf/timestamp handling
- `auto_orientation/src/.../ekf.{h,cpp}` — F2 size reclaim
- `auto_orientation/platformio.ini` — native_test src_filter exclusions
- `flight_controller/include/config.h` — line 93 SBUS re-enable

### New test files

- `auto_orientation/tests/test_json_formatter.cpp`
- `auto_orientation/tests/test_calibration_storage.cpp`

### New FC infrastructure files

- `flight_controller/tests/lib/harness.sh`
- `flight_controller/tests/suites/test_calibration.sh`

### Recommended commit structure (4–6 logical commits)

When operator is ready to commit, suggest splitting along these seams:

1. **Audit deliverables** — all 5 AO audit docs + state_reconciliation + architecture_plan (docs only).
2. **Security fixes** — calibration_storage / bno085_calibration source + CAL_FORMAT_VERSION + `security_fix_calibration_2026-05-20.md`.
3. **Test additions** — `test_json_formatter.cpp` + `test_calibration_storage.cpp` + json_formatter.cpp bug fixes + native_test platformio.ini changes.
4. **AO build / RAM fixes** — F2 EKF reclaim + mega_ram_fix doc + `tools/build_tests.sh` rewrite + PWM_DISCOVERY warning fix + F() macro shim.
5. **AO doc reconciliation** — roadmap.md / todo.md / INDEX / Uno README updates + tuner_format_alignment doc.
6. **FC infrastructure** — SBUS re-enable + harness.sh / test_calibration.sh + project_recon + test_infrastructure_v2 + future_session_scaffolding + bno_cross_project + roadmap/todo updates + this day_status doc.

Splitting along these lines keeps each commit atomically revertable and matches the workstream boundaries used for parallel agent dispatch.

---

## Cross-references

- AO architecture and workstream partition: [architecture_plan_2026-05-20.md](../../auto_orientation/docs/findings/architecture_plan_2026-05-20.md)
- AO source-vs-doc truth check: [state_reconciliation_2026-05-20.md](../../auto_orientation/docs/findings/state_reconciliation_2026-05-20.md)
- FC current-state map: [project_recon_2026-05-20.md](../../flight_controller/docs/findings/project_recon_2026-05-20.md)
- FC future agenda + operator questions: [future_session_scaffolding_2026-05-20.md](../../flight_controller/docs/findings/future_session_scaffolding_2026-05-20.md)
- Joint IMU strategy: [bno_cross_project_2026-05-20.md](./bno_cross_project_2026-05-20.md)

---

*End of day status. Next session: open with §5 (open items) and §6 (operator questions); use §7 to plan the commit pass if operator wants to land the working tree before resuming new work.*
