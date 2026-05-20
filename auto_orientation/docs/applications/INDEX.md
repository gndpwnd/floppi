# Applications — Index

Reference applications built on top of the Auto Orientation framework. Each application is a fully-wired end-user build that exercises a real combination of sensors, control loops, calibration UX, and feedback hardware.

Applications are compile-time gated through flags in [`src/config/mode.h`](../../src/config/mode.h) (see [findings/MASTER_DESIGN.md §Compile flag inventory](../findings/MASTER_DESIGN.md)). A build only pulls in the application its flag selects.

---

## Available applications

| Application | Compile flag | Build env | Status |
|-------------|--------------|-----------|--------|
| [Self-balancing robot (Mega universal)](balancing_robot/INDEX.md) | `USE_BALANCING_ROBOT` | `arduino_mega_balancing` | Phase 4 reference — universal/adaptive stack (BOOTSTRAP, RLS, mounting estimator, collision detector, planned encoders + position containment) |
| [Self-balancing robot (Uno minimal)](balancing_robot_uno/INDEX.md) | `USE_BALANCING_ROBOT_UNO` | `arduino_uno_minimal` (planned) | **2026-05-19 pivot — stub** — small hardcoded PID balancer; gains generated offline via `tools/sim/brute_tune.py` |

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

*Last updated: 2026-05-12.*
