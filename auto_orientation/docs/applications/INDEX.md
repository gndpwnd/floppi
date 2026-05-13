# Applications — Index

Reference applications built on top of the Auto Orientation framework. Each application is a fully-wired end-user build that exercises a real combination of sensors, control loops, calibration UX, and feedback hardware.

Applications are compile-time gated through flags in [`src/config/mode.h`](../../src/config/mode.h) (see [findings/MASTER_DESIGN.md §Compile flag inventory](../findings/MASTER_DESIGN.md)). A build only pulls in the application its flag selects.

---

## Available applications

| Application | Compile flag | Build env | Status |
|-------------|--------------|-----------|--------|
| [Self-balancing robot](balancing_robot/INDEX.md) | `USE_BALANCING_ROBOT` | `arduino_mega_balancing` | Phase 4 reference application |

Future applications planned in [findings/application_catalog.md](../findings/application_catalog.md): multirotor bridge, camera mount / gimbal, photogrammetry rig, educational kit, robot-arm pose feedback.

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
