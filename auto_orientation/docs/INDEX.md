# Auto Orientation — Documentation Index

Navigation map for `auto_orientation/docs/`. Each subfolder is themed; open its `INDEX.md` for a per-file listing.

---

## Start here

- [README.md](README.md) — Project overview, status, capabilities
- [architecture/](architecture/INDEX.md) — **Layered architecture diagrams** (Level 0 system overview → Level 1 subsystems → Level 2 components), reflecting the Mega-universal vs Uno-minimal split
- [getting_started/](getting_started/INDEX.md) — Onboarding, FAQ, architecture overview
- [build/BUILD_GUIDE.md](build/INDEX.md) — How to compile/flash
- [../FOLDER_STRUCTURE.md](../FOLDER_STRUCTURE.md) — Top-level project file tree

---

## Strategic notes

| File | Purpose |
|------|---------|
| [MEGA_UNIVERSAL_PLAN.md](MEGA_UNIVERSAL_PLAN.md) | **2026-05-19 pivot** — detailed plan for the Mega-only universal/adaptive balance stack (BOOTSTRAP, RLS, encoders, position containment) |
| [applications/balancing_robot_uno/README.md](applications/balancing_robot_uno/README.md) | **2026-05-19 pivot** — Uno-minimal hardcoded balancer + Python brute-force tuner workflow |

---

## Planning (top-level, edited frequently)

| File | Purpose |
|------|---------|
| [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md) | The design north star — universal code, no per-bot config, learn dynamics from operating data. **Now Mega-only as of 2026-05-19** |
| [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) | Controller-architecture companion to the vision — what was removed, what was kept, compile-switch matrix |
| [MULTI_ORIENTATION_BALANCE_VISION.md](MULTI_ORIENTATION_BALANCE_VISION.md) | Phase 4.11+ direction — balance past 90°, arbitrary mounting orientation, ballbot future |
| [AUTO_TUNING_REALITY_CHECK.md](AUTO_TUNING_REALITY_CHECK.md) | Why universal dynamic auto-tuning is sequential, not one-shot; the bootstrap-protocol explanation |
| [PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md) | Phase 4 coordinating doc — items 1-5, all landed 2026-05-12 |
| [THEORETICALLY_SOUND_PROGRAM_PLAN.md](THEORETICALLY_SOUND_PROGRAM_PLAN.md) | ACTIVE — sim-first validation plan triggered by 2026-05-12 bench session: crystal flag + motors-not-powered confounded all PID iteration. Validate algorithm in sim before re-engaging motors. |
| [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) | Tiered execution plan for the balancing-robot reference app |
| [KNOWN_ISSUES.md](KNOWN_ISSUES.md) | 19 issues (KI-2..KI-20) ranked by severity |
| [roadmap.md](roadmap.md) | Project roadmap, milestones (Phase 0 -> v1.0 -> v1.1 -> future) |
| [scope.md](scope.md) | In-scope / out-of-scope boundaries |
| [todo.md](todo.md) | Current task list |
| [findings/operator_ideas_backlog.md](findings/operator_ideas_backlog.md) | Durable index of operator-suggested ideas (status, source, technical translation) |

---

## Subfolder navigation

| Subfolder | Purpose |
|-----------|---------|
| [architecture/](architecture/INDEX.md) | Layered architecture diagrams (L0 system → L1 subsystems → L2 components), grounded in source |
| [getting_started/](getting_started/INDEX.md) | Onboarding: getting started, FAQ, architecture overview |
| [theory/](theory/INDEX.md) | Conceptual / math background (quaternions, NED frame, BNO085 algorithm) |
| [build/](build/INDEX.md) | Build guides, feature flags, snapshot feature |
| [hardware/](hardware/INDEX.md) | Wiring, GPS module status, hardware troubleshooting |
| [calibration/](calibration/INDEX.md) | End-user calibration procedure + implementation notes |
| [phases/](phases/INDEX.md) | Phase plans, test results, completion summaries, release checklists |
| [applications/](applications/INDEX.md) | Reference applications built on the framework (balancing robot, future apps) |
| [reference/](reference/INDEX.md) | API references and protocol specs (quaternion, GPS, BNO085, EKF, SH-2) |
| [guides/](guides/INDEX.md) | Task-oriented how-to guides (quick start, deployment, adding sensors) |
| [implementation/](implementation/INDEX.md) | Per-component implementation walk-throughs |
| [setup/](setup/INDEX.md) | Initial-setup and next-step planning notes |
| [testing/](testing/INDEX.md) | Test manifests, READMEs, integration test guide |
| [research/](research/INDEX.md) | Long-form research compilations (MPU6050, etc.) |
| [findings/](findings/INDEX.md) | Short focused research notes that inform design decisions |
| [todo/](todo/INDEX.md) | In-progress checklists and session-status notes |
| [archive/](archive/INDEX.md) | Old session records, superseded docs, reference sketches |

---

## Common entry points

- New to the project? -> [getting_started/GETTING_STARTED.md](getting_started/GETTING_STARTED.md)
- Need to build/flash? -> [build/BUILD_GUIDE.md](build/BUILD_GUIDE.md)
- API lookup? -> [reference/INDEX.md](reference/INDEX.md)
- Concept question? -> [theory/INDEX.md](theory/INDEX.md)
- Hardware wiring? -> [hardware/INDEX.md](hardware/INDEX.md) and [guides/HARDWARE_SETUP.md](guides/HARDWARE_SETUP.md)
- Phase status / test results? -> [phases/INDEX.md](phases/INDEX.md)
- Calibration procedure? -> [calibration/CALIBRATION_GUIDE.md](calibration/CALIBRATION_GUIDE.md)
- Want to build a balancing robot? -> [applications/balancing_robot/USER_GUIDE.md](applications/balancing_robot/USER_GUIDE.md)
- Looking for the framework's compositional flags? -> `src/config/mode.h` + [findings/MASTER_DESIGN.md](findings/MASTER_DESIGN.md) §"Compile flag inventory"

---

*Last updated: 2026-05-21 (Workstream G + Phase 4M.14 test coverage + security-hardening session — see [archive/session_records/2026-05-21_multi_agent_workstream_g_security.md](archive/session_records/2026-05-21_multi_agent_workstream_g_security.md)). Maintained by: project planning sessions. If you add a doc, add a one-line entry to the appropriate subfolder INDEX.md.*
