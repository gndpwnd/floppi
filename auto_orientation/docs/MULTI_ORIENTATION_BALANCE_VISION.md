# Multi-Orientation Balance Vision
Last updated: 2026-05-12

## The user's framing

> "Orientation can go past 90 degrees and axis can change. We want to be able to handle this — like if the robot goes onto its side we want it to be able to balance or we could add more motors on the side. Make sense? It's more complicated but a start for another idea for self-balancing stuff."

That is a meaningful generalization beyond the classic single-axis inverted pendulum. The framework already represents orientation as a quaternion (no gimbal lock), so the underlying math doesn't break at 90° — what breaks is the assumption that "pitch is the controlled axis." This document captures the vision; a sibling research finding evaluates feasibility.

## What "multi-orientation balance" means concretely

Three increasingly ambitious levels:

### Level 1 — Axis swap on rollover
- Bot starts upright on two wheels (pitch axis = Y).
- Operator lays the bot on its side. Now the bot can balance on a different pair of "wheels" / contact points — say, on its side faces, with a different pair of actuators.
- The controller detects the rollover, **remaps which physical axis is the "balance axis"** and which motors are "the wheels", and continues to balance.
- Requires hardware: at least 4 contact points or motors arranged so that more than one configuration is balanceable.

### Level 2 — Arbitrary-orientation single-pendulum balance
- The bot has a single set of wheels but the operator can re-mount the IMU / chassis in any orientation.
- Boot from any pitch / roll / yaw — the controller figures out which axis gravity is loading and balances about THAT axis.
- Requires: gravity-direction detection from accelerometer at boot, mapping to a "balance frame" rotation. Magnetometer NOT required (yaw is unconstrained for a 2-wheel balancer).

### Level 3 — Omnidirectional balance (3D inverted pendulum)
- Bot has motors / contact points arranged so it can balance on a single point (think a basketball, a "ballbot").
- Two degrees of unstable freedom (pitch AND roll), control vector is a 3-axis force/torque application.
- Genuinely hard. State-of-the-art research territory. CMU's "Ballbot" (Hollis 2006) is the seminal reference.

## Why this fits the framework

The auto_orientation framework was designed quaternion-first:

- `OrientationData::quaternion` is the canonical state, with Euler angles as derived views.
- The BNO055 / BNO085 drivers report full quaternion; pitch/roll/yaw are accessors.
- The control layer (PID) currently consumes `pitch_deg` as a scalar — but extending it to consume a quaternion and produce a per-axis motor command is a natural generalization, NOT a rewrite.

So Level 1 and Level 2 are mostly bookkeeping. Level 3 is a different control problem (multi-input multi-output, MIMO) and would require new infrastructure (LQR or MPC in the right state space).

## Hardware implications

| Level | Wheels / motors | IMU | New hardware? |
|---|---|---|---|
| 1 (rollover axis swap) | 2 wheels + 2 secondary motors / wheels on side faces | Same BNO055 | Yes — extra motors, structural side-balance contact |
| 2 (any-orientation pendulum) | Same 2 wheels | Same BNO055 | No — purely firmware |
| 3 (ballbot) | 3-4 omniwheels on a sphere | Same BNO055 | Major chassis rebuild |

Level 2 is implementable on the existing Uno + BNO055 + L298N + two-motor hardware. Levels 1 and 3 require chassis changes.

## What needs to change in the firmware

### For Level 2 (the closest reachable target):

1. **Boot-time balance-axis detection.**
   - On power-up, read accelerometer for ~1 s to determine gravity direction in body frame.
   - From gravity direction, compute the **rotation matrix from body frame to "balance frame"** where Z is "up" and the wheel axis is X.
   - Store this as the "operating frame" until next boot.

2. **Quaternion-derived balance error** (replace `pitch_deg`).
   - Express orientation in the operating frame.
   - The "tilt off balance" is the angle between body-Z (in operating frame) and world-up. Equivalently, the magnitude of the rotation that brings body-Z to world-up.
   - This is **scalar**, like pitch, but works at any orientation including past 90°.

3. **Gyro-axis selection.**
   - The current code reads `gyro_y` as "pitch rate". After balance-axis detection, the relevant gyro axis is whichever points along the rotation axis in the operating frame.
   - Implementation: rotate the raw gyro vector by the body→operating transform, take the component perpendicular to gravity AND to the wheel axis.

4. **Mount-offset capture in the operating frame.**
   - Same procedure as today, but the captured "balance angle" is in the operating frame rather than absolute pitch.
   - EEPROM blob keeps the same size; semantics change.

### For Level 1 (rollover axis swap):

Builds on Level 2 plus:

5. **Rollover detection.**
   - When the bot tips PAST recoverable (e.g. body-Z rotates more than 60° away from operating-frame-up), recompute the operating frame.
   - Operator-triggered alternative: button-press recaptures the balance axis from current orientation.

6. **Motor mux.**
   - Two pairs of motors are wired to the L298N. A small mux selects which pair is the "wheels" based on the active operating frame.
   - Hardware: 4 motors + 2 L298N (or one with extra driver), structural contact patches on multiple faces.

### For Level 3 (ballbot):

7. **MIMO controller** (probably LQR with quaternion state). Out of scope until 1 and 2 are real.

## What CAN'T cleanly extend

- **The relay-feedback auto-tuner** assumes scalar setpoint tracking. Generalizes only with substantial rework for Level 3.
- **The OnlineMountingEstimator** assumes scalar offset. Generalizes to a 3-vector offset for Level 2 with some bookkeeping; redesign for Level 3.
- **The "minimize accelerations" philosophy** still holds — but "accelerations" become 3-vectors, and the controller goal is "minimize the magnitude of body acceleration not aligned with gravity".

## Reachable target — what to build first

**Level 2 firmware-only** is the next milestone. Concrete plan:

- Phase 4.11 (after Phase 4.10 universal auto-tune lands): introduce `BalanceFrame` — a quaternion describing the body→operating frame rotation, captured at boot via accel-only gravity detection.
- All control math runs in operating frame. The existing single-axis PID + Phase 4.10 plant identifier work unchanged because they see scalar tilt + scalar gyro rate in the operating frame.
- User experience: bot can be mounted any way up. Power on, prop upright (in any orientation), it self-detects which axis is the wheel axis, balances.

This is genuinely **universal in orientation as well as in chassis** — the bot doesn't care which way it's pointing at boot.

## Open questions

1. **Calibration in non-canonical orientations.** BNO055 cal wizard currently expects the operator to rotate through axes. Does the cal procedure need to be operating-frame-aware? (Probably no — cal is intrinsic to the chip, not the chassis.)
2. **Mounting-offset capture procedure** — does the operator still hold the bot at balance and press `c`? Yes, but the captured value is the operating-frame offset, not absolute pitch.
3. **When to re-detect operating frame?** Only on boot, or also on a button-press (rollover handling)? Operator preference.
4. **Battery / wiring constraints** — for Level 1 (multiple wheel pairs), the L298N has only two H-bridges. A 4-motor design needs additional hardware.

## See also

- [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md) — the no-per-bot-config vision.
- [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — controller architecture.
- [PHASE_4_STRUCTURAL_FIXES.md](PHASE_4_STRUCTURAL_FIXES.md) — current Phase 4 work.
- (TBD) `findings/research_multi_orientation_balance_feasibility.md` — sibling research agent's evaluation of which level is realistically reachable.
