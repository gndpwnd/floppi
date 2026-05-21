# Session Synthesis — auto_orientation — 2026-05-21

**Date:** 2026-05-21
**Session type:** Multi-agent, orchestrator-managed (Claude Code Orchestra).
**Status:** UNCOMMITTED — every change below is in the working tree only; nothing
has been committed to git. The last commit is `bf9a402 save progress`.

> **Canonical record:** the full narrative, decisions, and blockers for this
> session live in
> [../archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](../archive/session_records/2026-05-21_multi_agent_workstream_g_security.md).
> This synthesis is the short per-area companion — refer to the session record
> for decisions and the bench-gated next steps.

---

## Summary

A multi-agent landing wave focused on **Workstream G (bench-tuning support)**,
**test-coverage closure for Phase 4M.14**, **two P1 security hardening fixes**,
and a round of **build/doc hygiene**. The Mega two-stage cascade is feature-
complete through Phase 4M.14; this session made it *observable* on the bench
(telemetry accessors + `g` command + host plotting) and *safe* (CRC + buffer-
overflow fixes), without changing the control behaviour. No new control phase
was started — the remaining 4M.14/Workstream-G items are bench-hardware-gated.

---

## Per-area breakdown

### Test coverage — Phase 4M.14 now has executed coverage
- `tools/build_tests.sh`: wired in `test_position_loop.cpp` and
  `test_position_gain_derivation.cpp` (the 4M.14 gain-derivation suite — the
  files existed but were never built/run) and the new `test_balance_telemetry.cpp`.
- New `tests/test_balance_telemetry.cpp` covers the Workstream-G `BalanceApp`
  telemetry accessors under the native encoder build profile.
- Native suite: **17 tests, 17/17 pass** (3 Unity-framework tests skipped — host
  lacks `unity.h`).

### Workstream G — bench-tuning support (codeable items)
- **G1** — telemetry accessors on `BalanceApp` (`balance_app.h`): const
  pass-through getters (`get_pitch_setpoint_deg`, `get_position_m`,
  `get_pos_nudge_deg`, `get_k_pos/_k_vel/_pos_leak`) plus a non-const
  `get_wheel_velocity_mps(now_ms)`. Pure read-through over existing state — no
  new ISR-written members, no behaviour change.
- **G2** — `'g'` serial command in `main.cpp` emitting a single CSV telemetry
  line: `G,millis,pitch_deg,pitch_sp_deg,wheel_vel_mps,position_m,nudge_deg,k_pos,k_vel,pos_leak`.
- **Gap-3** — PWM-discovery results now wired live into `stiction_min_pwm`
  (previously discovered but not consumed).
- **G3** — new host script `tools/plot_bench_run.py` to plot a captured `g`-run.
- New docs: `docs/findings/workstream_g_bench_protocol_2026-05-21.md` (bench
  protocol) and `docs/guides/gain_logbook_template.md` (gain logbook template).
- **TD-7** — windup-handling clarification comment in `control/position_loop.h`.

### Security — two P1 fixes
- `navigation/mounting_calibration.cpp`: the fake CRC (a weak XOR-sum) was
  replaced with the shared `util::crc8_ccitt`; the mounting record version was
  bumped (old EEPROM blobs are now rejected — operator must re-capture mounting).
- `config/calibration_storage.{h,cpp}` + `test_calibration_storage.cpp`:
  `restoreFromEEPROM()` stack-buffer-overflow closed by adding a `buf_capacity`
  parameter and a pre-read length check; all 16 call sites updated.
- `sensors/bno085.cpp`: a P3 null-guard added alongside.

### Code quality
- P3 comment polish in `balance_app.cpp` (audit findings F-2 / F-6 / F-8, an
  ISR-safety note on `raw_gyro_dps_[]`, and a mislabeled-failure-reason fix).

### Build / platformio.ini hygiene
- `default_envs` retargeted off the legacy `uno_balance` env (which sat at
  ~93.6% Uno flash) to `mega_balance` — the actively-developed env.
- The two lean Uno envs (`arduino_uno_minimal`, `arduino_uno_tuning`) were
  de-duplicated via a shared `[uno_minimal_base]` section; they now differ only
  by the `-D UNO_GUIDED_TUNING` flag.
- `uno_balance` retained but explicitly annotated LEGACY/DEAD.

### Documentation drift fixes
- `scope.md` — env table + dates corrected.
- `applications/balancing_robot/INDEX.md` — stale env name + relay-tuner refs.
- `applications/balancing_robot_uno/README.md` — API names corrected.
- `tools/README.md` — bench-tuning tooling entries added.
- `docs/findings/INDEX.md` — this synthesis doc added.
- `ao_roadmap_post_4m14_2026-05-20.md` — superseded banner refreshed.

---

## Verification status

- **Builds:** all 6 AO firmware envs build clean.
- **Tests:** native suite is 17 tests, 17/17 pass (the two 4M.14 suites + the
  new telemetry suite wired in); 3 Unity-framework tests skipped on this host.

## Uncommitted — not yet committed to git

All of the above is in the working tree only. New untracked files:
`docs/findings/workstream_g_bench_protocol_2026-05-21.md`,
`docs/guides/gain_logbook_template.md`, `tests/test_balance_telemetry.cpp`,
`tools/plot_bench_run.py`. A commit pass should run a clean build + the
17-test native suite first.

## What's next / bench-gated

The remaining Workstream-G and Phase 4M.14 follow-ups need physical bench
hardware and cannot be advanced statically:
- **F-3 — `K_VEL` bench observation:** confirm the derived velocity gain
  behaves on the real plant; the analytical derivation is unvalidated against
  hardware.
- **Regression-baseline capture:** record a known-good `g`-telemetry run with
  `tools/plot_bench_run.py` as the regression reference.
- **Real-motor PWM-discovery validation:** verify the Gap-3 `stiction_min_pwm`
  wiring against actual motor stiction on the bench.

Cross-references: `phase_4m14_landed_2026-05-20.md`,
`workstream_g_bench_protocol_2026-05-21.md`,
`ao_roadmap_post_4m14_2026-05-20.md`, `security_fix_calibration_2026-05-20.md`.
