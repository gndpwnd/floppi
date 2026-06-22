# Applications — Index

Reference applications built on top of the Auto Orientation framework. Each application is a fully-wired end-user build that exercises a real combination of sensors, control loops, calibration UX, and feedback hardware.

Applications are compile-time gated through flags in [`src/config/mode.h`](../../src/config/mode.h) (see [findings/MASTER_DESIGN.md §Compile flag inventory](../findings/MASTER_DESIGN.md)). A build only pulls in the application its flag selects.

---

## Start here — pick your tier

**New operators**: open [`CHOOSE_YOUR_TIER.md`](CHOOSE_YOUR_TIER.md) first. It is the routing doc — given the hardware you have on the bench, it points you at the right `FIRST_SUCCESS_*.md` (Mega or Uno) and the right build env. Each `FIRST_SUCCESS_*.md` is a 10-step, ~30–60 min checklist from hardware-in-hand to first balanced release.

- [`balancing_robot/FIRST_SUCCESS_MEGA.md`](balancing_robot/FIRST_SUCCESS_MEGA.md) — Mega 2560 + BNO055 + L298N (+ wheel encoders) bring-up.
- [`balancing_robot_uno/FIRST_SUCCESS_UNO.md`](balancing_robot_uno/FIRST_SUCCESS_UNO.md) — Arduino Uno + BNO055 + L298N two-build flow (`arduino_uno_tuning` SETUP MODE → `arduino_uno_minimal` OPERATIONAL MODE).
- [`balancing_robot_uno/CHEATSHEET.md`](balancing_robot_uno/CHEATSHEET.md) — Uno-minimal operator bench card (one-page command/LED/buzzer reference for live tuning).

---

## Bench

- [`../findings/bench_validation_runbook_2026-05-27.md`](../findings/bench_validation_runbook_2026-05-27.md) — When you have hardware — consolidated 24-item runbook for a single bench session (covers both Mega and Uno tiers).

---

## Available applications

| Application | Compile flag | Build env | Status |
|-------------|--------------|-----------|--------|
| [Self-balancing robot (Mega universal)](balancing_robot/INDEX.md) | `USE_BALANCING_ROBOT` | `mega_balance` | Phase 4 reference — universal/adaptive stack (BOOTSTRAP, RLS, mounting estimator, collision detector, planned encoders + position containment) |
| [Self-balancing robot (Uno minimal — flight build)](balancing_robot_uno/INDEX.md) | `USE_BALANCING_ROBOT_UNO` | `arduino_uno_minimal` | Shipped — small hardcoded PID balancer; gains generated offline via `tools/sim/brute_tune.py` |
| [Self-balancing robot (Uno guided tuning bench)](balancing_robot_uno/INDEX.md) | `USE_BALANCING_ROBOT_UNO` + `UNO_GUIDED_TUNING` | `arduino_uno_tuning` | Shipped — flight build PLUS interactive on-device BNO055 cal (`'c'`) + guided P→D→I tune (`'t'`) over serial, persisted to EEPROM |

**Platform bifurcation (2026-05-19)**: The universal/adaptive vision now targets Mega-class hardware only. The Uno gets a smaller, single-purpose program. See [../scope.md §Platform bifurcation](../scope.md) and [../roadmap.md §Phase 4U](../roadmap.md) for the pivot framing.

Future applications planned in [findings/application_catalog.md](../findings/application_catalog.md): multirotor bridge, camera mount / gimbal, photogrammetry rig, educational kit, robot-arm pose feedback.

The Mega and Uno balancers are sibling applications that target different ends of the cost / capability spectrum. See [`../MEGA_UNIVERSAL_PLAN.md`](../MEGA_UNIVERSAL_PLAN.md) for the Mega path and [`balancing_robot_uno/README.md`](balancing_robot_uno/README.md) for the Uno path.

---

## What goes in this folder

- **User-facing setup guides** — how to wire it, how to flash it, what to expect.
- **Calibration and tuning workflows** — the hands-off UX that the application exposes.
- **Troubleshooting** — common issues specific to that application's hardware and control loop.

Design rationale, algorithm choices, and cross-cutting research stay in [findings/](../findings/INDEX.md). Implementation walk-throughs of the underlying framework modules stay in [implementation/](../implementation/INDEX.md).

---

## Related documentation

- [findings/MASTER_DESIGN.md](../findings/MASTER_DESIGN.md) — the canonical decision log and phase ordering
- [findings/application_catalog.md](../findings/application_catalog.md) — the list of candidate applications and priority order
- [scope.md](../scope.md) — what the framework is and is not for
- [roadmap.md](../roadmap.md) — current phase, what is next

---

*Last updated: 2026-06-21 (wave-12 crosslink cleanup — added Bench section linking `findings/bench_validation_runbook_2026-05-27.md`; surfaced `balancing_robot_uno/CHEATSHEET.md` under Start-here; prior: crosslink audit — surfaced `CHOOSE_YOUR_TIER.md` + per-tier `FIRST_SUCCESS_*.md` as the operator entry points; nav-polish pass — list current envs `mega_balance` / `arduino_uno_minimal` / `arduino_uno_tuning`; remove stub markers — both Uno envs have shipped).*
