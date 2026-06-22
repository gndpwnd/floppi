# Balancing Robot — Uno Minimal — Index

Reference application: small, single-purpose, hardcoded-PID balancer for the Arduino Uno. The Mega-universal sibling lives at [`../balancing_robot/INDEX.md`](../balancing_robot/INDEX.md).

**Status**: Stub (Phase 4U opening, 2026-05-19). Scaffold owned by sibling agents. See [`../../UNIVERSAL_BALANCE_BOT_VISION.md`](../../UNIVERSAL_BALANCE_BOT_VISION.md) for the platform-bifurcation rationale and [`../../scope.md`](../../scope.md) for the pivot framing.

---

## Available documents

| File | Purpose |
|------|---------|
| [FIRST_SUCCESS_UNO.md](FIRST_SUCCESS_UNO.md) | **Recommended entry point** — 10-step, ~30–60 min checklist from hardware-in-hand to first balanced release; covers the two-build flow (`arduino_uno_tuning` SETUP MODE → `arduino_uno_minimal` OPERATIONAL MODE) |
| [CHEATSHEET.md](CHEATSHEET.md) | Operator bench card — one-page command / LED / buzzer reference for live tuning sessions |
| [README.md](README.md) | Overview: why a separate Uno program exists, build, brute-force tuning workflow, expected behaviour, limitations, source layout, related docs |

---

## Planned documents

These slots are reserved for content added as Phase 4U progresses. Drop the file in and add a one-line entry to the table above when each one lands.

| File | Trigger to write it | Owner |
|------|---------------------|-------|
| `USER_GUIDE.md` | First successful bench run on the Uno target — capture the unplug/upright/release sequence and what "good" looks like on operator video | Operator + scribe agent |
| `TUNING_LOG.md` | After each `brute_tune.py` re-run — record search mode, budget, plant preset, fitness, resulting constants, and whether the bot balanced | Tuner-runner |
| `FLASH_PROCEDURE.md` | Once the `arduino_uno_minimal` build env is finalised — the step-by-step flash + verify sequence including `balance_constants.h` regeneration | Build-env owner |
| `TROUBLESHOOTING.md` | After two failure modes have been observed and resolved on the bench — common symptoms, diagnostic commands, fixes | Bench operator |
| `HARDWARE_SETUP.md` | If the Uno wiring diverges from the Mega-balancer wiring (different motor driver, different IMU bus, different power topology) | Hardware-change agent |

---

## Related documentation

- [`../balancing_robot/INDEX.md`](../balancing_robot/INDEX.md) — Mega-universal sibling
- [`../INDEX.md`](../INDEX.md) — Application catalog (lists both balancers side-by-side)
- [`../../MEGA_UNIVERSAL_PLAN.md`](../../MEGA_UNIVERSAL_PLAN.md) — Mega path companion to the Uno path
- [`../../UNIVERSAL_BALANCE_BOT_VISION.md`](../../UNIVERSAL_BALANCE_BOT_VISION.md) — The universal vision (Mega-only as of 2026-05-19)
- `tools/sim/README.md` — Brute-force tuner reference (drives the `balance_constants.h` consumed by this build)

---

*Last updated: 2026-06-21 (wave-12 crosslink cleanup — added `FIRST_SUCCESS_UNO.md` as the recommended entry point and `CHEATSHEET.md` as the operator bench card in the Available documents table; prior: 2026-05-19 created at Phase 4U opening, alongside README.md stub).*
