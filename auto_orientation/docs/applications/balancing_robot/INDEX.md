# Self-Balancing Robot — Documentation Index

End-user documentation for the Phase 4 self-balancing robot reference application.

This is the first full-stack application the framework ships: an Arduino Mega + IMU + wheel encoders + L298N + 2 DC motors that learns its own mounting offset and PID gains on the bench, then balances autonomously over USB-untethered battery power.

Gains are not relay-feedback auto-tuned. The app derives them at boot: the **BOOTSTRAP** state pulses the motors to measure K_motor and derives the inner-loop PID gains analytically (Phase 4M.2 adds an encoder-driven K cross-check that aborts BOOTSTRAP on slip/bind disagreement). On encoder-equipped builds a **position/velocity cascade** (Phase 4M.13) adds an outer station-keeping loop whose gains are themselves auto-derived in closed form via pole-placement at BOOTSTRAP finalise (Phase 4M.14). No hand-tuned or relay-tuned constants on the operating path.

This Mega application is the **universal "plug-and-play" auto** tier of the [platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal): the Mega's flash + RAM headroom is what makes the BOOTSTRAP / K cross-check / analytical gain auto-derivation stack possible. The sibling [Uno-minimal application](../balancing_robot_uno/README.md) is the **manual operator-guided** tier (IMU calibration + guided P→D→I tuning) for the small/cheap target where that stack does not fit — different capability tier, same project family. **IMU choice is orthogonal to MCU choice**: both BNO055 and BNO085 are valid on either MCU (current envs default to BNO055).

---

## Read in order

| File | Purpose | Read when |
|------|---------|-----------|
| [FIRST_SUCCESS_MEGA.md](FIRST_SUCCESS_MEGA.md) | **Recommended entry point** — 10-step, ~30–60 min checklist from hardware-in-hand to first balanced release on a Mega 2560 | You have the hardware on the bench and want first success |
| [USER_GUIDE.md](USER_GUIDE.md) | Top-level setup + first-flash walkthrough | You are starting from scratch |
| [HARDWARE_SETUP.md](HARDWARE_SETUP.md) | Bill of materials, pin map, wiring | You are wiring the bot |
| [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md) | Hands-off button/LED/buzzer flow | You have it wired and want to balance |
| [TROUBLESHOOTING.md](TROUBLESHOOTING.md) | Symptom → fix lookup | Something is misbehaving |

---

## Reference and rationale (deeper reads)

- [../../findings/MASTER_DESIGN.md](../../findings/MASTER_DESIGN.md) — Phase 4 task plan, including this application
- [../../findings/balance_point_and_mounting_research.md](../../findings/balance_point_and_mounting_research.md) — why the one-shot mounting capture works
- [../../findings/online_adaptive_balance_tracking.md](../../findings/online_adaptive_balance_tracking.md) — the slow drift compensator that runs alongside the main loop
- [../../findings/phase_4m14_landed_2026-05-20.md](../../findings/phase_4m14_landed_2026-05-20.md) — analytical auto-derivation of the outer-loop (position/velocity cascade) gains
- [../../findings/workstream_f_review_2026-05-20.md](../../findings/workstream_f_review_2026-05-20.md) — review of the BOOTSTRAP K cross-check + velocity outer-loop landing
- [../../findings/tetherless_operation_strategy.md](../../findings/tetherless_operation_strategy.md) — button/LED/buzzer state machine
- [../../findings/disturbance_compensation_research.md](../../findings/disturbance_compensation_research.md) — push-detect + accel feedforward
- [../../findings/application_catalog.md](../../findings/application_catalog.md) — where this application sits in the overall application priority list

---

## Build at a glance

```bash
cd auto_orientation
pio run -e mega_balance -t upload
```

For everything else, start with [USER_GUIDE.md](USER_GUIDE.md).

---

*Last updated: 2026-06-21 (wave-12 crosslink cleanup — added `FIRST_SUCCESS_MEGA.md` as the recommended entry point in the Read-in-order table; prior: 2026-05-26 added bifurcation cross-reference: Mega = universal-auto tier, Uno = manual-guided tier; IMU orthogonal to MCU).*
