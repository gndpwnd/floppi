# 2026-05-20 — Project recon, 10/10 builds, and IMU scaffolding

**Session start**: 2026-05-20 (working tree at commit `1ea9a61` "auto orientation work")
**Session end**: 2026-05-20
**Commits**: 0 (operator directive: no commits — deliverables left in working tree)
**Agents spawned**: ~20 across the day (joint AO + FC orchestration)
**Mode**: Agent-manager parent + parallel sub-agents
**Operator constraints**: no commits, keep FC simple, conservative resource use, no agent file conflicts

> First session record for `flight_controller/`. Format follows the `auto_orientation/` house style. Master cross-project snapshot for this day: `/home/devel/floppi/docs/findings/day_status_2026-05-20.md`.

---

## Headline outcomes

### FC is now 10/10 on the build matrix
The project recon found 7/10 build environments passing pre-session. The single blocker was `USE_SBUS_RECEIVER` being commented out in `include/config.h:93` (left disabled for bench testing). Re-enabling SBUS cleared all 3 expected-fails — **all 10 build envs now compile**: teensy36 / teensy40 / teensy41 + esp32 / esp32s3, each with a calibration variant.

### What landed this session
| Deliverable | Output | Impact |
|---|---|---|
| Project recon | `docs/findings/project_recon_2026-05-20.md` (779 lines) | First comprehensive map of the FC project; build matrix established |
| SBUS re-enable | `include/config.h:93` (one-line uncomment) | 7/10 → 10/10 build coverage |
| Test harness modularization | `tests/lib/harness.sh` (246 lines) + `tests/suites/test_calibration.sh` (304 lines) | 480-line monolith split; original kept as 21-line exec wrapper |
| BNO Phase A scaffolding | `USE_BNO055` / `USE_BNO085` flags in `config.h` + I2C-detect stubs in `imu.cpp` | Flags OFF by default — zero behavior change |
| PID tuning guide | `docs/pid-tuning-guide.md` (310 lines) | Documents the existing `g` calibration command workflow |
| Wiring guide audit | `docs/findings/wiring_guide_audit_2026-05-20.md` | 5 wiring docs cross-checked; 2 hard fixes + 3 `[VERIFY]` flags |
| WiFi onboarding doc | `docs/esp32_wifi_onboarding.md` (143 lines) | ESP32 AP-mode onboarding walkthrough |
| Diagnose decision tree | `docs/diagnose_decision_tree.md` (225 lines) | Troubleshooting decision tree |
| Cross-project IMU research | `docs/findings/bno_cross_project_2026-05-20.md` (785 lines) | Phased plan to port AO's BNO drivers + calibration HAL into FC |
| Future-session scaffolding | `docs/findings/future_session_scaffolding_2026-05-20.md` | 3-session agenda + 5 operator questions |

### Build matrix (end of session — all green)
All 10 environments compile after the SBUS re-enable.

| env family | variants | status |
|---|---|---|
| teensy36 | base + calibration | SUCCESS |
| teensy40 | base + calibration | SUCCESS |
| teensy41 | base + calibration | SUCCESS |
| esp32 | base + calibration | SUCCESS |
| esp32s3 | base + calibration | SUCCESS |

Reference figures: `teensy40_calibration` = 88612 B FLASH; `esp32_calibration` = 47.2% Flash / 11.0% RAM.

---

## Detail: test harness modularization

The monolithic `tests/test_calibration.sh` (480 lines) was split into a reusable library + a focused suite:

- `tests/lib/harness.sh` (246 lines) — shared test harness primitives (assertion helpers, build invocation, result reporting).
- `tests/suites/test_calibration.sh` (304 lines) — the calibration suite proper, now built on the library.
- `tests/test_calibration.sh` — preserved as a **21-line exec wrapper** that forwards to the suite, so the operator's existing entry point and workflow are unchanged.

All **18 tests / 42 assertions** are preserved verbatim — the split is a pure refactor with no coverage change. The ESP32 reset path is **stubbed** (not functional) — it needs hardware to implement properly.

See `docs/findings/test_infrastructure_v2_2026-05-20.md` for the full design rationale.

---

## Detail: BNO055 / BNO085 Phase A scaffolding

Phase A of the cross-project IMU plan landed as **scaffolding only**:

- `USE_BNO055` and `USE_BNO085` compile flags added to `include/config.h`, **OFF by default**.
- I2C-detect stubs added to `imu.cpp`.
- Zero behavior change with the flags off. Both `teensy40_calibration` and `esp32_calibration` build clean with the scaffolding present.

Per the operator's "keep FC simple" directive, Phases B / B.5 / C (actual driver port, calibration HAL port, full integration) are **deferred**. The cross-project research (`bno_cross_project_2026-05-20.md`, 785 lines) lays out the phased plan for porting `auto_orientation`'s mature BNO drivers and `calibration_storage` HAL into FC when the operator chooses to proceed.

---

## Detail: wiring guide fidelity audit

5 wiring docs were cross-checked against the `pin_definitions` headers:

- **2 hard fixes applied**: a DSM baud-rate typo, and a PWM pin listing correction.
- **3 `[VERIFY]` flags raised** on ESP32 GPIO conflicts (see Open items below).

See `docs/findings/wiring_guide_audit_2026-05-20.md`.

---

## Decisions made

1. **Keep FC simple (operator directive).** Phase A is scaffolding-only — flags OFF, no behavior change. Phase B/C deferred until the operator explicitly opts in.
2. **BNO055 integration is feasible without over-complicating FC** — provided it is phased carefully. The cross-project research confirms the AO drivers port cleanly under the phased plan.
3. **Test modularization preserved the original entry point.** Keeping `tests/test_calibration.sh` as an exec wrapper means no operator workflow breakage from the refactor.

---

## Discoveries / open items

### ESP32 GPIO conflicts (3 found in wiring audit)
The wiring audit surfaced 3 GPIO double-assignments on ESP32 that need bench-side resolution or `config.h` `PIN_*` overrides:

- **GPIO 16 / 17** — `SBUS_RX` / `SBUS_TX` collide with `SERVO_4` / `SERVO_5`.
- **GPIO 4** — `IBUS` / `DSM` / command-RX collide with `SERVO_3`.

These are flagged `[VERIFY]` in the wiring docs pending bench-side resolution.

### Phase B prep item
`Wire.begin()` is currently gated under `USE_MPU6050`. For BNO-only builds (a Phase B scenario) it must be **hoisted** out of that guard so I2C is initialized regardless of which IMU is selected.

### ESP32 reset path stub
The ESP32 reset path in the test harness is **stubbed, not functional**. Implementing it properly requires hardware on the bench.

---

## What's next (FC)

Per `docs/findings/future_session_scaffolding_2026-05-20.md` §4, a 3-session agenda:

- **Session 1** — close out current scope. Wiring audits: done this session. PID guide: done this session. Roadmap refresh: done this session. (Session 1 is effectively complete.)
- **Session 2** — motor / ESC test framework spec; swarm-API contract.
- **Session 3** — barometer / magnetometer / GPS specs.

**5 operator open questions** await answers before concrete scope decisions can be made (see scaffolding doc §7): default IMU for FC v2, GPS scope clarification, hardware availability through Q2 2026, swarm_api integration appetite, ResearchHub readiness.

---

## Blockers

- **Anthropic API usage limits** were hit twice when 5+ sub-agents ran concurrently. Empirical safe budget: **3–4 concurrent agents max**, sequence the rest.
- **Hardware-dependent items deferred** — bench smoke test, ESP32 reset-path implementation, and motor/ESC testing all need physical hardware that was not on the bench this session.

---

## Uncommitted state at end of session

Per the operator's no-commit rule, all deliverables sit in the working tree:

- **New findings docs**: `project_recon_2026-05-20.md`, `test_infrastructure_v2_2026-05-20.md`, `future_session_scaffolding_2026-05-20.md`, `wiring_guide_audit_2026-05-20.md`.
- **New user-facing docs**: `pid-tuning-guide.md`, `esp32_wifi_onboarding.md`, `diagnose_decision_tree.md`.
- **Modified source**: `include/config.h` (SBUS re-enable + BNO Phase A flags), `imu.cpp` (I2C-detect stubs).
- **New / modified test infrastructure**: `tests/lib/harness.sh`, `tests/suites/test_calibration.sh`, `tests/test_calibration.sh` (now exec wrapper).
- **Reconciled docs** (by a sibling agent): `roadmap.md`, `todo.md` — SBUS-flip and modular-harness items marked done; Next-Session goal reset to calibration phases 1–7.

---

## See also

- `/home/devel/floppi/docs/findings/day_status_2026-05-20.md` — cross-project master snapshot for this day
- `docs/findings/project_recon_2026-05-20.md` — FC current-state map
- `docs/findings/future_session_scaffolding_2026-05-20.md` — FC future agenda + operator questions
- `docs/findings/test_infrastructure_v2_2026-05-20.md` — test harness modularization design
- `docs/findings/wiring_guide_audit_2026-05-20.md` — wiring fidelity audit results
- `/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md` — joint IMU integration strategy
- `docs/pid-tuning-guide.md` — `g`-command PID tuning workflow

---

*End of session record. Next session: open with "What's next (FC)" above; answer the 5 operator questions in the scaffolding doc §7 to unblock Session 2 scope.*
