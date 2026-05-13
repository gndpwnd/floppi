# Self-Balancing Robot — Documentation Index

End-user documentation for the Phase 4 self-balancing robot reference application.

This is the first full-stack application the framework ships: an Arduino Mega + IMU + L298N + 2 DC motors that learns its own mounting offset and PID gains on the bench, then balances autonomously over USB-untethered battery power.

---

## Read in order

| File | Purpose | Read when |
|------|---------|-----------|
| [USER_GUIDE.md](USER_GUIDE.md) | Top-level setup + first-flash walkthrough | You are starting from scratch |
| [HARDWARE_SETUP.md](HARDWARE_SETUP.md) | Bill of materials, pin map, wiring | You are wiring the bot |
| [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md) | Hands-off button/LED/buzzer flow | You have it wired and want to balance |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Symptom → fix lookup | Something is misbehaving |

---

## Reference and rationale (deeper reads)

- [../../findings/MASTER_DESIGN.md](../../findings/MASTER_DESIGN.md) — Phase 4 task plan, including this application
- [../../findings/balance_point_and_mounting_research.md](../../findings/balance_point_and_mounting_research.md) — why the one-shot mounting capture works
- [../../findings/online_adaptive_balance_tracking.md](../../findings/online_adaptive_balance_tracking.md) — the slow drift compensator that runs alongside the main loop
- [../../findings/auto_pid_tuning_research.md](../../findings/auto_pid_tuning_research.md) — the relay-feedback auto-tuner this app uses
- [../../findings/tetherless_operation_strategy.md](../../findings/tetherless_operation_strategy.md) — button/LED/buzzer state machine
- [../../findings/disturbance_compensation_research.md](../../findings/disturbance_compensation_research.md) — push-detect + accel feedforward
- [../../findings/application_catalog.md](../../findings/application_catalog.md) — where this application sits in the overall application priority list

---

## Build at a glance

```bash
cd auto_orientation
pio run -e arduino_mega_balancing -t upload
```

For everything else, start with [USER_GUIDE.md](USER_GUIDE.md).

---

*Last updated: 2026-05-12.*
