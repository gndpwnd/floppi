# Architecture — layered diagrams

A drill-down map of the `auto_orientation` framework. Read **top to bottom**: each level adds detail, so reviewers can stop at the depth they need without being overwhelmed.

Every diagram here is grounded in actual source (`src/control/`, `src/applications/`, `src/sensors/`, `src/navigation/`, `tools/sim/`). Where a diagram simplifies, it says so.

> **Read [`../scope.md`](../scope.md) §"Platform bifurcation" first.** The framework forks by MCU class:
>
> - **Mega-balance** (`src/applications/balancing_robot/`) — the *universal / adaptive* stack: BOOTSTRAP K-measurement, RLS plant-ID, inner PID + position outer-loop cascade, collision detection, online mounting estimator, wheel encoders. New code lands here.
> - **Uno-balance** (`src/applications/balancing_robot_uno/`) — a *minimal hardcoded* PID balancer. No on-MCU adaptation. Gains come from an on-device guided session or, for a brand-new chassis, the offline Python brute-force tuner (`tools/sim/brute_tune.py`).

---

## Levels

| Level | Doc | What it shows | Stop here if… |
|-------|-----|---------------|---------------|
| **0** | [LEVEL_0_SYSTEM_OVERVIEW.md](LEVEL_0_SYSTEM_OVERVIEW.md) | The framework's major layers (config → sensors → math → navigation → control → actuators → applications, optional network on ESP32) and where the Mega/Uno fork sits | You want the one-page mental model. |
| **1** | [LEVEL_1_SUBSYSTEMS.md](LEVEL_1_SUBSYSTEMS.md) | One diagram each for: (a) Mega adaptive control stack + HELD/BOOTSTRAP state machine, (b) Uno minimal path + Python tuner, (c) sensor + odometry pipeline, (d) persistent-storage HAL | You're reviewing a single subsystem. |
| **2** | [LEVEL_2_COMPONENTS.md](LEVEL_2_COMPONENTS.md) | The two highest-value internals: BOOTSTRAP K-measurement pulse sequence + pole-placement gain derivation, and the position outer-loop cascade structure | You're modifying these specific components. |

---

## Source map (where the boxes live)

| Layer | Directory | Key files |
|-------|-----------|-----------|
| config | `src/config/` | `pins.h`, `mode.h`, `calibration_storage.{h,cpp}`, `ekf_config.h` |
| sensors | `src/sensors/` | `sensor_base.h`, `bno085.*`, `bno055.*`, `gps.*`, `wheel_encoder.*`, `button_input.*` |
| math | `src/math/` | `quaternion.*`, `coordinates.*`, `magnetic_declination.*` |
| navigation | `src/navigation/` | `ekf.*`, `coordinate_frame.*`, `mounting_calibration.*`, `online_mounting_estimator.*` |
| control | `src/control/` | `pid_controller.*`, `plant_identifier.*`, `position_loop.*`, `auto_pid_tuner.*`, `tuners/` |
| actuators | `src/actuators/` | `motor_driver.h`, `l298n_motor_driver.*` |
| applications | `src/applications/` | `balancing_robot/` (Mega), `balancing_robot_uno/` (Uno) |
| storage (HAL) | `src/storage/` | `persistent_storage.h` + `_avr/_teensy/_esp32/_native.cpp` |
| offline tooling | `tools/sim/` | `brute_tune.py`, `balance_bot_sim.py`, `balance_constants_template.h.in` |

---

## Related component walk-throughs

These per-component docs already carry Mermaid diagrams and are the next stop after Level 2:

- [implementation/pid_controller.md](../implementation/pid_controller.md)
- [implementation/auto_pid_tuner.md](../implementation/auto_pid_tuner.md)
- [implementation/online_mounting_estimator.md](../implementation/online_mounting_estimator.md)
- [implementation/mounting_calibration.md](../implementation/mounting_calibration.md)
- [implementation/l298n_motor_driver.md](../implementation/l298n_motor_driver.md)
- [implementation/persistent_storage.md](../implementation/persistent_storage.md)
- [getting_started/ARCHITECTURE.md](../getting_started/ARCHITECTURE.md) — the original sensor-fusion (BNO085 + GPS) architecture, fully Mermaid.

---

## Provenance & as-built reconciliation

These LEVEL_0/1/2 diagrams were authored in the 2026-05-22 session, which also reconciled the
design corpus against the live source. Where the implementation has evolved past the design docs
(pole-placement not AMIGO, no `BootstrapStage` enum, mean-pitch mount estimator, `g=9.81`
position-loop gain), the **as-built/as-designed reconciliation** is the source of truth — see
[../findings/MASTER_DESIGN.md](../findings/MASTER_DESIGN.md) and the session record
[../archive/session_records/2026-05-22_safety_correctness_docs.md](../archive/session_records/2026-05-22_safety_correctness_docs.md).
Companion 2026-05-22 findings: [../findings/autocal_autotune_verification_2026-05-22.md](../findings/autocal_autotune_verification_2026-05-22.md),
[../findings/security_audit_2026-05-22.md](../findings/security_audit_2026-05-22.md),
[../findings/mega_scope_violation_triage_2026-05-22.md](../findings/mega_scope_violation_triage_2026-05-22.md).
