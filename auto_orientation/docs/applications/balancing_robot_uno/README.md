# Balancing Robot — Uno Minimal

**Status**: Phase 4U scaffold landed 2026-05-19 commit c3c0c6b. Source at [`src/applications/balancing_robot_uno/`](../../../src/applications/balancing_robot_uno) (`uno_balance_app.{h,cpp}` + `main.cpp` + `balance_constants.h`). Python brute-force tuner at [`tools/sim/brute_tune.py`](../../../tools/sim/brute_tune.py). Open contract bug: tuner header symbols don't match consumer expectations — see [verification_2026-05-19.md §9](../../findings/verification_2026-05-19.md).
**Last updated**: 2026-05-19 (created at platform-bifurcation pivot)
**Sibling doc**: [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — the Mega-universal sibling.

---

## Why a separate Uno program exists

After the 2026-05-18 PM-late bench session left the Uno at 97.5 % flash with the universal/adaptive balance stack still unable to balance reliably, the project pivoted (2026-05-19). The universal vision (BOOTSTRAP, RLS, OnlineMountingEstimator, collision detection, position containment, wheel encoders) moves to **Mega-class hardware only** where flash headroom and GPIO/interrupts are available. The Uno gets a **separate, smaller, single-purpose program** that does one thing: balance, using hardcoded constants generated offline.

Authoritative pivot record: `project_strategic_pivot_2026-05-19.md` (operator memory).
Pivot framing: [../../scope.md §Platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal).
Roadmap: [../../roadmap.md §Phase 4U](../../roadmap.md#phase-4u--uno-minimal-hardcoded-balancer--python-brute-force-tuner).

---

## Overview

The Uno minimal balancer is the smallest program that can make a two-wheel self-balancing robot stand up. It:

- Reads pitch from a calibrated BNO055 (via the existing `src/sensors/bno055.cpp`).
- Runs a fixed-gain PID loop with `Kp`, `Ki`, `Kd` from a generated header (`balance_constants_uno.h`).
- Drives a dual-channel L298N motor driver (via the existing `src/actuators/l298n_motor_driver.cpp`) with fixed stiction-floor and saturation PWM values from the same generated header.

It does **not**:

- Auto-tune at runtime.
- Identify its own plant via RLS.
- Track mount drift online.
- Detect collisions or HELD states.
- Implement position containment.
- Self-bootstrap K_motor via pulse experiments.

If any of those behaviours are needed, the Mega-universal program is the right target.

The reference for the algorithm shape and the operator UX is the archived `archive/balancing_robot_reference/SelfBallancingRobot3.ino` — a hand-tuned Mega + BNO055 + L298N + PID balance bot known to balance on the operator's bench.

---

## Build

> The build environment is owned by the sibling agent scaffolding this Phase. Expected name: `arduino_uno_minimal` (or the existing `uno_balance` repurposed).

```bash
# From auto_orientation/
pio run -e arduino_uno_minimal
pio run -e arduino_uno_minimal -t upload
```

The Uno target should fit comfortably under 60 % flash because all the universal-stack machinery is gone. If it doesn't, something universal has leaked in — delete it.

Compile gate: `USE_BALANCING_ROBOT_UNO` (mutually exclusive with `USE_BALANCING_ROBOT`).

---

## Brute-force tuning workflow

The Uno does not learn its own gains. The Python brute-force tuner does, offline, before flashing.

> The tuner is owned by the sibling agent scaffolding `tools/sim/`. The workflow below is the planned interface.

1. **Pick a plant model**. The default uses the nominal bot parameters baked into `tools/sim/balance_bot_sim.py`. To tune for a different bot (different mass, different wheel radius, different motor K), pass overrides via CLI flags.
2. **Run the tuner**. Grid-search converges in a few minutes; evolutionary refinement (CMA-ES or similar) finishes the gains. Fitness function = time balanced before tip-over, averaged across randomized disturbance injections.
3. **Inspect the result**. The tuner reports best Kp / Ki / Kd / stiction_min_pwm / saturation_pwm and the simulated time-to-fall under disturbances.
4. **Generate the header**. The tuner emits `balance_constants_uno.h` as a `constexpr`-only header.
5. **Drop it in place**. The build script copies the header into `src/applications/balancing_robot_uno/generated/`.
6. **Reflash the Uno**. The Uno picks up the new constants on next boot — no EEPROM, no runtime config.
7. **Bench validation**. Prop bot upright, release, time to first tip-over. Pass criterion: ≥ 30 s on flat indoor surface.

When the operator changes the battery chemistry, wheels, or surface in a way that breaks balance, the cycle repeats — re-tune offline, reflash. No on-MCU adaptation is ever attempted.

---

## Expected behaviour

Once flashed and propped upright, the bot should:

- Detect upright orientation from the calibrated BNO055 within ~100 ms.
- Begin balancing immediately with no operator interaction.
- Hold balance on a flat indoor surface for at least 30 s under nominal conditions.
- Tip over gracefully (motors stop) when pitch exceeds a hardcoded fallen-angle threshold.

There is no calibration UX in the Uno program — sensor calibration is assumed to have happened beforehand via the Mega calibration build (`mega_orientation_calibration`) or any other host-side calibration path that writes the BNO055 22-byte calibration blob to EEPROM.

---

## Limitations

This program is intentionally limited. The limitations are the design.

- **Not adaptive.** Battery sag, surface grip changes, payload changes, motor wear — all require an offline re-tune. There is no online compensation.
- **No collision detection.** A bump during balancing is treated like any other pitch disturbance. If the disturbance exceeds the PID's ability to recover, the bot tips.
- **No HELD detection.** Picking up the bot mid-balance produces undefined behaviour — the PID will keep driving the motors.
- **No position containment.** The bot will drift as the controller fights small pitch errors. There is no encoder, no position estimate, no outer loop.
- **No safe-restart logic.** A tipped bot does not recover by itself — it just sits with motors off.
- **No serial CLI for tuning.** The constants are compiled in. The only way to change them is to re-tune via Python and reflash.

If you need any of those behaviours, you are looking for the Mega-universal program — [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — not this one.

---

## Source layout

> Actual layout as landed 2026-05-19 (commit c3c0c6b):

```text
src/applications/balancing_robot_uno/
├── uno_balance_app.h     # Top-level API + volatile-double-buffered pitch
├── uno_balance_app.cpp   # PID loop (~150 LOC)
├── main.cpp              # Arduino setup() + loop() + MsTimer2 200 Hz ISR
└── balance_constants.h   # file-scope BALANCE_KP/KI/KD + PWM_MIN/MAX + TIP/SANITY (regenerated by tools/sim/brute_tune.py once contract is aligned)
```

Note: the Python tuner currently emits `namespace balance { constexpr float KP; ... }` while the consumer reads file-scope `BALANCE_KP / BALANCE_KI / BALANCE_KD / PWM_MIN / PWM_MAX / STICTION_PWM / TIP_CUTOFF_DEG / PITCH_SANITY_DEG`. Pending fix tracked in `todo.md` under "2026-05-19 verification — open issues" (P0).

Reused modules (no Uno-specific code added):

- `src/sensors/bno055.{h,cpp}` — pitch from calibrated BNO055
- `src/actuators/l298n_motor_driver.{h,cpp}` — dual-channel PWM motor driver
- `src/math/quaternion.h`, `src/math/quaternion_conversions.h` — orientation math

---

## When to fill in this document

This doc is a stub on day 1 of Phase 4U. Real content gets filled in as the program is built:

- **Build section** — once the build env is finalized and the flash usage is measured.
- **Brute-force tuning workflow** — once the Python tool stabilizes (CLI flags, output format, search strategy).
- **Expected behaviour** — once the first bench validation passes and the operator records what "good" looks like.
- **Limitations** — refine as bench testing exposes new failure modes worth documenting.

Cross-references will resolve once the sibling agents land their scaffolds.

---

## Related documentation

- [../../UNIVERSAL_BALANCE_BOT_VISION.md](../../UNIVERSAL_BALANCE_BOT_VISION.md) — the universal vision (Mega-only as of 2026-05-19) — explains what the Uno program is **not** doing and why
- [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — the Mega-universal sibling application
- [../../scope.md §Platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal) — the pivot framing
- [../../roadmap.md §Phase 4U](../../roadmap.md#phase-4u--uno-minimal-hardcoded-balancer--python-brute-force-tuner) — phase plan
- `archive/balancing_robot_reference/SelfBallancingRobot3.ino` — the .ino reference this program echoes in spirit

---

*Last updated: 2026-05-19 (stub at Phase 4U opening).*
