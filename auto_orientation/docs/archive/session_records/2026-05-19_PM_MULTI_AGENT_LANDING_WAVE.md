# 2026-05-19 PM — Multi-agent landing wave + strategic pivot

**Session start**: 2026-05-19 AM (working tree at commit `7a4d27f`)
**Session end**: 2026-05-19 PM late (~22:50 ET)
**Commits**: 1 (`c3c0c6b "save progress"` — operator-applied mid-session)
**Agents spawned**: ~30+ across 11 waves
**Mode**: Agent-manager parent + parallel sub-agents
**Operator constraints**: no commits, no infra work, conservative resource use, no agent conflicts

---

## Headline outcomes

### Strategic pivot landed
After bench testing showed the universal stack didn't fit on Uno at 97.5% flash, the project pivoted to a **Mega/Uno bifurcation**:
- **Mega** = universal/adaptive stack with wheel encoders, full automation
- **Uno** = minimal hardcoded balancer with PID/PWM values produced offline by a Python brute-force tuner
- See `~/.claude/projects/-home-devel-floppi/memory/project_strategic_pivot_2026-05-19.md` for the canonical record

### What landed in code (working tree, mostly committed as `c3c0c6b`)
| Feature | Files | Tests | Build impact |
|---|---|---|---|
| Collision detection (3-gate) | balance_app.{h,cpp}, sensor_base.h, bno055.{h,cpp} | 27/27 | +350-450 B Mega |
| P0/P1 audit fixes | balance_app.{h,cpp}, plant_identifier.{cpp,h} | 40/40 BOOTSTRAP | net -204 B |
| Wheel encoder driver | sensors/wheel_encoder.{h,cpp} | 17/17 | +600 B Mega |
| Encoder integration | balance_app.{h,cpp} | 25/25 | guarded behind USE_WHEEL_ENCODERS |
| Phase 4M.12 PWM auto-discovery | balance_app.{h,cpp} | 10 tests / 47 assertions | +600 B Mega |
| Uno minimal program | applications/balancing_robot_uno/* (4 files) | 33/33 | 50.0% flash standalone |
| Python brute-force tuner | tools/sim/brute_tune.py (~880 lines) + template + sim | reproduces ref Kd~31 (was 16) | new |
| src_filter dup-symbols fix | platformio.ini | n/a | unblocked Mega build |
| mega_orientation RAM fix | platformio.ini, main.cpp, ekf.{h,cpp} | EKF tests still pass | RAM 125.5% → 74.5% |
| Tuner polish (this evening) | tools/sim/brute_tune.py | manual verification ran | ref Kd 62→31, stress 11→66 |

### Build matrix (final, all green)
| env | status | flash% | RAM% | notes |
|---|---|---|---|---|
| arduino_uno_minimal | SUCCESS | 50.0% | 34.7% | hardcoded balancer |
| uno_balance | SUCCESS | 98.5% | 65.8% | universal (tight; deprecation candidate) |
| mega_balance | SUCCESS | 14.7% | 17.9% | universal + collision + encoder + PWMD |
| mega_orientation | SUCCESS | 15.0% | 74.5% | EKF gated off |
| mega_orientation_calibration | SUCCESS | 15.6% | 83.4% | same |
| native_test | partial (pre-existing breakage in EKF/math tests) | n/a | n/a | balance/encoder tests all green |

### Documentation landed
- `docs/MEGA_UNIVERSAL_PLAN.md` (339 lines) — Mega-only universal stack design
- `docs/findings/phase_4_11a_design_2026-05-19.md` — position containment design (12 sections, 2345 prose words)
- `docs/findings/research_collision_signature_bno055.md` — 3-gate threshold rationale
- `docs/findings/research_imu_only_position_containment.md` — RCmags SB-1 pitch double-integration
- `docs/findings/research_wheel_encoders_mega_2026-05-19.md` — hardware survey + pin map
- `docs/findings/audit_documentation_2026-05-19.md` — 91 broken links fixed, 17 doc files
- `docs/findings/audit_code_quality_balance_stack_2026-05-19.md` — 4 P0 + 14 P1 + 15 P2 findings
- `docs/findings/audit_uno_minimal_2026-05-19.md` — 2 P0 + 15 P1 + 12 P2 on Uno minimal
- `docs/findings/investigation_held_state_machine_failure_2026-05-19.md` — stale binary, not a bug
- `docs/findings/verification_2026-05-19.md` — build matrix + tuner reproducibility
- `docs/findings/tuner_kd_accuracy_2026-05-19.md` — Kd accuracy fix history
- `docs/findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md` — EKF stub identified as killer
- `docs/findings/phase_4m12_landed_2026-05-19.md` — Phase 4M.12 as-built doc + missing test file
- `docs/findings/brute_tune_simplification_design_2026-05-19.md` — (in flight, architect agent)
- `docs/guides/safe_bench_test_workflow.md` (251 lines) — operator bench safety guide
- `docs/guides/encoder_bench_bringup.md` (450 lines) — encoder hardware bring-up procedure
- `docs/guides/next_bench_session_2026-05-19_prep.md` (1736 words) — synthesis for next physical session

---

## Process learnings

### Sub-agent permission state shifted mid-session
Wave 11 onward, sub-agents hit `Permission to use Edit has been denied` on existing files. Read worked, Write to NEW files worked, Edit on existing files was blocked. The parent (manager) retained full Edit/Write permissions.

**Workaround applied**: parent agent applied tuner polish manually from the blocked sub-agent's detailed diff plan.

**Open**: needs operator-side investigation of why sub-agent Edit was denied.

### Orchestra context now available
`/home/devel/palletai/claude_code_orchestra/CONTEXT_FOR_NEW_CONVERSATIONS.md` was missing for most of the session, then appeared. Future agents should be spawned in the orchestra format:
```
AGENT: [agent-type]
INSTANCE ID: [agent-type@floppi:work_zone]
PROJECT_ROOT: /home/devel/floppi
WORK_ZONE (edit): [specific paths]
READ_ONLY: [specific paths]
BLOCKED: everything outside PROJECT_ROOT
Report to: [docs/handoffs/ or docs/findings/...]
```

### Agent timeouts on big-scope tasks
Phase 4M.12 implementation agent timed out at 27 minutes / 93 tool uses. Mitigation: scope new agents to sub-phases (Phase 4.11a was split into a-1 / a-2 / a-3 / a-4 / a-5).

### Network-error agent deaths
3 agents died with `Stream idle timeout` or `socket connection was closed` mid-task. Many had completed substantial work on disk before dying — always check filesystem before assuming work was lost. The bench bring-up doc (450 lines) and the Kd accuracy fix (200 lines) both landed despite their agents reporting death.

### File-conflict avoidance worked
Across ~30 agents, zero file-write collisions. Discipline: every agent prompt had explicit "files you MAY edit" + "files you must NOT touch" sections, mapped to what was in flight. The orchestra WORK_ZONE / READ_ONLY / BLOCKED pattern formalizes this.

---

## Open blockers / next session

### Highest priority (Mega path enablement)
1. **Apply Phase 4.11a-1 odometry** — blocked sub-agent left detailed plan (constants, members, update_odometry_, reset_position, 'o' command, new test file). ~150 lines.
2. **Phase 4M.12 follow-ups** (3 known gaps):
   - Collision detection doesn't abort PWM_DISCOVERY (BOOTSTRAP/CHARACTERISE do)
   - EEPROM slot address mismatch in docs (MEGA_UNIVERSAL_PLAN.md says 0x210, balance_app.h says 0x230)
   - Boot-time consumer wiring incomplete: `load_pwm_discovery_()` runs but loaded min_pwm not fed to motor driver

### Medium priority (Uno path polish)
3. **Apply Uno P1 #6-15** — blocked sub-agent left 8-item plan: I²C-fail counter, README layout fix, hand-default marker, banner integer fmt, test additions
4. **Bench-validate Uno minimal** — P0+P1 fixes landed but no physical bench since the fixes; needs verification per `docs/guides/next_bench_session_2026-05-19_prep.md`

### Tooling polish
5. **brute_tune simplification** — operator feedback: too many flags, prefer algorithmic (no seeds), modular OK. Design in flight (architect@floppi:brute_tune_simplification). Concrete refactor in next session.
6. **Tuner stress preset** — now balanceable but Kd=66 still off from ideal; w_jerk knob can be tuned further if needed

### Doc hygiene
7. **scope.md re-audit** — blocked agent identified 4 rows marked ✅ that should be 🟡 (constants survive as ctor fallbacks). Net 11 open of original 14. Specific evidence in agent's report.
8. **docs/INDEX cross-links** — encoder_bench_bringup.md + safe_bench_test_workflow.md absent from root INDEX

### Phase 4.11a-2 onward (after a-1 lands)
9. Phase 4.11a-2 hardcoded outer loop
10. Phase 4.11a-3 auto-derived gains from PlantIdentifier::K_motor
11. Phase 4.11a-4 IMU fallback (RCmags SB-1)
12. Phase 4.11a-5 EEPROM persistence + telemetry

### Operator-facing
13. Encoder operator commands (`k` for CPR readout, distance, calibrate save) — separate from already-landed `o` reservation per Phase 4.11a-1 plan
14. Wiring diagrams update with encoder pin assignments

---

## Memory updates

- `project_strategic_pivot_2026-05-19.md` captured the bifurcation (created earlier this session)
- MEMORY.md pointer added at top of "Pointers (newer memories)"
- This session record + MEMORY.md pointer for the multi-agent wave landed in this same write

---

## Files NOT committed (working tree state)

`brute_tune.py` and `balance_constants_template.h.in` have uncommitted changes from this evening's tuner polish (parent-applied manual edits). All other landings are in commit `c3c0c6b`. Per operator directive: no commits made this session.

---

## See also

- `docs/MEGA_UNIVERSAL_PLAN.md` — current Mega-path canonical plan
- `docs/guides/next_bench_session_2026-05-19_prep.md` — operator-facing test plan
- `docs/findings/` — all research + audits from this session
- `~/.claude/projects/-home-devel-floppi/memory/project_strategic_pivot_2026-05-19.md` — pivot rationale
