# 2026-05-21 — Multi-agent: Workstream G, 4M.14 test coverage, security hardening

**Session start**: 2026-05-21 (working tree post `bf9a402 save progress`)
**Session end**: 2026-05-21
**Commits**: 0 — all deliverables are in the working tree only, per operator no-commit rule
**Mode**: Multi-agent, orchestrator-managed (Claude Code Orchestra); conservative 3–4 concurrent
**Operator constraints**: no commits, no builds/Docker by doc agents, exclusive work zones

> Scope of this record: **auto_orientation only**. The `flight_controller` side of
> the day (Teensy parity, barometer driver, native test harness) has its own
> sibling session record under `flight_controller/docs/`.

This is the **canonical detailed record** for the 2026-05-21 auto_orientation
session. The per-area detail lives in the findings docs cross-referenced below;
this record is the narrative + decisions + blockers index.

---

## Headline

The Mega two-stage cascade controller was feature-complete through Phase 4M.14
at the start of this session. This session did **not** start a new control
phase. Instead it made the cascade **observable** on the bench (Workstream G
telemetry + host plotting) and **safe** (two P1 calibration-storage hardening
fixes), and closed the executed-test gap for Phase 4M.14. The remaining
Workstream-G / 4M.14 follow-ups are bench-hardware-gated and could not be
advanced statically.

---

## What landed in code (working tree, uncommitted)

### Workstream G — bench-tuning support (codeable items)
- **G1** — telemetry accessors on `BalanceApp` (`balance_app.h`): const
  pass-through getters (`get_pitch_setpoint_deg`, `get_position_m`,
  `get_pos_nudge_deg`, `get_k_pos/_k_vel/_pos_leak`) + a non-const
  `get_wheel_velocity_mps(now_ms)`. Pure read-through over existing state — no
  new ISR-written members, no behaviour change.
- **G2** — `'g'` serial command in `main.cpp` emitting one CSV telemetry line:
  `G,millis,pitch_deg,pitch_sp_deg,wheel_vel_mps,position_m,nudge_deg,k_pos,k_vel,pos_leak`.
- **Gap-3** — PWM-discovery results now wired live into `stiction_min_pwm`
  (previously discovered but never consumed).
- **G3** — new host script `tools/plot_bench_run.py` to plot a captured
  `g`-run.
- **TD-7** — windup-handling clarification comment in `control/position_loop.h`.
- New docs: `docs/findings/workstream_g_bench_protocol_2026-05-21.md` (bench
  protocol) and `docs/guides/gain_logbook_template.md` (gain logbook template).

### Security — two P1 fixes + one P3
- `navigation/mounting_calibration.{h,cpp}`: the fake CRC (a weak XOR-sum) was
  replaced with the real `util::crc8_ccitt`; the mounting-record version was
  bumped — old EEPROM mounting blobs are now rejected and the operator must
  re-capture mounting.
- `config/calibration_storage.{h,cpp}` + `tests/test_calibration_storage.cpp`:
  `restoreFromEEPROM()` stack-buffer-overflow closed by adding a `buf_capacity`
  parameter and a pre-read length check; **16 call sites updated**.
- `sensors/bno085.cpp`: a P3 null-guard added alongside.

### Test wiring — Phase 4M.14 now has executed coverage
- `tools/build_tests.sh`: wired in `test_position_loop` +
  `test_position_gain_derivation` (the 4M.14 gain-derivation suite — the files
  existed but were never built/run) and the new `test_balance_telemetry`.
- New `tests/test_balance_telemetry.cpp` covers the Workstream-G `BalanceApp`
  telemetry accessors under the native encoder build profile.
- **Native suite: 17 tests, 17/17 pass.** (3 Unity-framework tests are skipped
  — the host lacks `unity.h`; they still run under `pio test -e native_test`.)

### Build / platformio.ini hygiene
- `default_envs` retargeted off the legacy `uno_balance` env (which sat at
  ~93.6% Uno flash) to `mega_balance` — the actively-developed env.
- The lean Uno envs were de-duplicated via a shared `[uno_minimal_base]`
  section.

### Code quality + doc drift
- P3 comment polish in `balance_app.cpp` (audit findings F-2 / F-6 / F-8).
- Doc-drift fixes: `scope.md` env tables/dates, `applications/balancing_robot/INDEX.md`,
  `applications/balancing_robot_uno/README.md` API names, `tools/README.md`.

---

## Verification status

- **Builds**: all 6 AO firmware envs build clean (per the orchestrator's
  end-of-session verification).
- **Tests**: native suite **17/17 pass**; 3 Unity tests skipped on this host.

> Note on a count discrepancy: `findings/session_synthesis_2026-05-21.md` was
> written mid-session and says "14 → 18 tests"; the authoritative end-of-session
> count after the build-script wiring settled is **17 (17/17 pass, 3 Unity
> skipped)**. Treat this record's count as canonical.

---

## Decisions made

- **Mounting-record version bump over dual-CRC** — same rationale as the
  2026-05-20 `CAL_FORMAT_VERSION` bump: a rejected blob triggers a known, safe
  re-capture workflow; a dual-CRC path would carry the weak CRC indefinitely.
- **`restoreFromEEPROM()` gets a `buf_capacity` parameter** rather than a
  fixed internal cap — keeps the caller in control of its own buffer and makes
  the bound explicit at all 16 call sites.
- **`default_envs` → `mega_balance`** — the legacy `uno_balance` env was at
  ~93.6% flash and is not the active development target; defaulting builds to
  it was misleading.
- **No new control phase** — the remaining 4M.14 / Workstream-G items are
  bench-gated; starting another phase would have outrun hardware validation.

---

## Blockers / bench-hardware-gated next steps

These need the physical robot and cannot be advanced statically:

- **F-3 — `K_VEL` bench observation**: confirm the derived velocity gain
  behaves on the real plant; the analytical derivation is unvalidated against
  hardware.
- **Regression-baseline capture**: record a known-good `g`-telemetry run with
  `tools/plot_bench_run.py` as the regression reference.
- **Real-motor PWM-discovery validation**: verify the Gap-3 `stiction_min_pwm`
  wiring against actual motor stiction on the bench.

**Working tree uncommitted** — all deliverables (source fixes, new tests, new
findings/guide docs) sit in the working tree pending a batched operator commit.
A commit pass should run a clean build + the 17-test native suite first.

---

## See also

- [findings/session_synthesis_2026-05-21.md](../../findings/session_synthesis_2026-05-21.md) — per-area synthesis (short companion to this record)
- [findings/workstream_g_bench_protocol_2026-05-21.md](../../findings/workstream_g_bench_protocol_2026-05-21.md) — bench-tuning protocol for `g`-telemetry runs
- [guides/gain_logbook_template.md](../../guides/gain_logbook_template.md) — gain logbook template
- [findings/phase_4m14_landed_2026-05-20.md](../../findings/phase_4m14_landed_2026-05-20.md) — the 4M.14 gain auto-derivation this session test-covers
- [findings/workstream_f_review_2026-05-20.md](../../findings/workstream_f_review_2026-05-20.md) — Workstream-F audit (4M.2/4M.11/4M.13)
- [findings/security_fix_calibration_2026-05-20.md](../../findings/security_fix_calibration_2026-05-20.md) — prior calibration security work (context for this session's hardening)
- [findings/ao_roadmap_post_4m14_2026-05-20.md](../../findings/ao_roadmap_post_4m14_2026-05-20.md) — forward roadmap; Workstream G was the live next step
- [2026-05-20_multi_agent_sync_audits_and_fixes.md](2026-05-20_multi_agent_sync_audits_and_fixes.md) — prior session record
</content>
</invoke>
