# Balancing Robot Reference — Index

User's working self-balancing-robot sketch + dissection notes. Source material for the upcoming `src/applications/balancing_robot/` Phase 4 work.

| File | Purpose |
|------|---------|
| [SelfBallancingRobot3.ino](SelfBallancingRobot3.ino) | User's working sketch: Arduino Mega + BNO055 + L298N + PID_v1 + manual PITCH_OFFSET |
| [DISSECTION_NOTES.md](DISSECTION_NOTES.md) | Reverse-engineered breakdown, pin map, BNO055↔BNO085 mapping table, and the refactor backbone (which lines move into which new module) |

**Do not edit `SelfBallancingRobot3.ino`** — it is preserved as a baseline for a regression test in `tests/scenario_test_balancing.cpp` (forthcoming).

---

## Related research (what we're building on top of this reference)

All in [`../../findings/`](../../findings/INDEX.md):

| Finding | Topic |
|---------|-------|
| [balance_point_and_mounting_research.md](../../findings/balance_point_and_mounting_research.md) | Self-balancing robot dynamics, one-shot mounting-angle capture, balance-loop estimator (2-state Kalman vs 16-state EKF trade-off) |
| [auto_pid_tuning_research.md](../../findings/auto_pid_tuning_research.md) | Auto-PID-tuning algorithm comparison (relay-feedback, Z-N, twiddle, RLS, ESC, fuzzy) — relay-feedback recommended as the default for pendulums |
| [online_adaptive_balance_tracking.md](../../findings/online_adaptive_balance_tracking.md) | Online drift handling: cable tether, battery sag, payload changes |
| [disturbance_compensation_research.md](../../findings/disturbance_compensation_research.md) | Push detection, IMU-accel feedforward, cascade control with optional encoders |
| [tetherless_operation_strategy.md](../../findings/tetherless_operation_strategy.md) | Workflow without USB tether (button + LED + buzzer + battery) |
| [application_catalog.md](../../findings/application_catalog.md) | The balancing robot's place in the broader application catalog |
| [MASTER_DESIGN.md](../../findings/MASTER_DESIGN.md) | Synthesis: 20-decision table + Phase 4-7 ordered task list |

And the planning docs at [`../../scope.md`](../../scope.md), [`../../roadmap.md`](../../roadmap.md), [`../../todo.md`](../../todo.md).

*Archived: 2026-05-12 from `auto_orientation/SelfBallancingRobot3.ino`.*
