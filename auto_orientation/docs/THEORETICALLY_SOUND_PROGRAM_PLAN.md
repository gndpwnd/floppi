# Theoretically Sound Program — Validation Plan
Status: ACTIVE. Triggered 2026-05-12 evening after diagnosing that the bench iteration had been confounded by (1) a stuck BNO055 due to crystal-flag mismatch and (2) motors that were not powered. The user direction: "make a theoretically sound program before we mess with motors."
Last updated: 2026-05-12

## Why this plan exists

For a full day, we iterated PID gains on hardware. Two pre-existing bugs (BNO055 fusion frozen by wrong crystal flag; mount offset captured against frozen sensor) plus a hardware-setup miss (motors disconnected) prevented ANY meaningful balance dynamics from happening. **We literally could not have observed whether the algorithm was working — there was no real plant to control.**

The framework's algorithm (raw-gyro D-term + soft-cutoff + HELD detection + Phase 4.10 RLS auto-tune + closed-form PD-from-K_motor) is structurally well-grounded per the five research agents. But "structurally well-grounded" is not "validated on a real plant." Until we can:

1. **Prove the algorithm is mathematically sound** (stability bounds, well-posed updates, no degenerate edge cases)
2. **Demonstrate it stabilises a simulated inverted pendulum** (cart-pole sim with realistic IMU noise and motor delay)
3. **Confirm the firmware matches the math** (native test suite covering each module + end-to-end scenario)

…the next bench session is just another guess. The goal of this plan is to do all three before the motors come on again.

## Three parallel workstreams

### Workstream 1 — Python cart-pole simulator (sim-first validation)

Build `tools/sim/balance_bot_sim.py`:
- Numerical integrator (RK4) of the inverted-pendulum equations of motion
- Realistic parameters: 0.5 kg mass, 0.20 m height, brushed DC motor with first-order dynamics + dead-band
- IMU noise model: BNO055-NDOF group delay (~25 ms), 0.05° quantization, gyro bias drift
- Hooks the actual C++ algorithm logic (replicated in Python) so we can validate the EXACT control law before flashing
- Configurable plant parameters → tests robustness across the "bench-scale class"

Pass criterion: the simulated bot balances within ±5° for ≥60 s of disturbance-rejection test under the same seed gains the firmware ships with. RLS converges to true K_motor within 2%.

### Workstream 2 — Theoretical soundness audit

Walk through `plant_identifier.cpp`, `pid_controller.cpp`, and `balance_app.cpp::step_run_` line by line, checking:
- **Stability**: closed-loop poles in the LHP for the seed gains and the rate-limited gains during ramp
- **Boundedness**: integral, gain ramp, K_motor estimate all provably bounded
- **Non-degeneracy**: no division by zero in steady state, no NaN propagation, RLS regressor magnitude has a floor that prevents collapse
- **Liveness**: PID always produces output in finite time, ISR latency budget fits, no priority inversion
- **Sign conventions**: pitch vs gyro vs motor PWM polarity is consistent throughout

Produce `findings/theoretical_audit_balance_stack.md`.

### Workstream 3 — Native test suite hardening

The existing `tests/test_plant_identifier.cpp` (7 tests, all pass) and `tests/test_pid_controller.cpp` are good but cover unit behaviour, not closed-loop behaviour. Add:
- **`tests/scenario_test_balancing.cpp`** that runs the actual `BalanceApp::step_run_` against synthetic α-data and verifies stable behaviour
- **`tests/test_balance_recovery.cpp`** that injects step disturbances and checks recovery time
- Fix any pre-existing broken-test infrastructure that makes `pio test -e native_test` fail

Pass criterion: `pio test -e native_test` runs all balance-stack tests with 100% pass.

## Pre-conditions before the next bench session

Before powering motors back on:

1. ✅ BNO055 crystal flag — fixed (`-D BNO055_NO_EXT_CRYSTAL` in default `arduino_uno_balancing` build)
2. ✅ Mount offset re-captured at 0.95° (against working sensor)
3. ⏳ Sim shows the algorithm stabilises an idealised plant
4. ⏳ Sim shows the algorithm survives realistic IMU + motor noise
5. ⏳ Audit identifies no theoretical defects
6. ⏳ Native test suite passes for the balance stack
7. ⏳ **Operator pre-flight checklist** added to USER_GUIDE: motor-power verification step BEFORE running balance test

## What we are NOT doing in this round

- No bench iteration on motors. The bot stays disconnected from its motor battery.
- No more hand-tuning of Kp/Ki/Kd. Anything that comes from the simulator is bench-validated math, not vibe.
- No new firmware features (no Phase 4.11 multi-orientation, no anomaly detector). Validate first, expand second.
- No changes to the EEPROM-persisted mount offset. The newly-captured 0.95° stands.

## Exit criteria

This plan is complete when:
- The Python sim demonstrates stable balance under the firmware's algorithm (with the same gain mapping, RLS, freeze gates, soft-cutoff).
- The theoretical audit produces no blocking defects.
- The native test suite is green for the balance stack.
- A motor-power pre-flight check is documented for the operator.

After that, the next bench session is genuinely a hardware-validation exercise — not another tuning trial.

## Operator checklist for the next bench session (preview)

When motors come back on:

1. **Verify motor battery is connected and switched on.** Visual check.
2. **Run motor test command (`m`)** with bot held off the ground or wheels not touching anything. Confirm each motor spins both directions.
3. **Place bot on a smooth surface** with wheels touching.
4. **Power Arduino via USB** (separate from motor battery for safety).
5. **Run tilt test (`a` to abort to IDLE then prompt for ±30° tilts)** — verify pitch tracks physical orientation.
6. **Re-capture mount offset (`c`) at the bot's current rest position.**
7. **Trigger RUN (auto on boot, or send `R`).** Bot should attempt to balance.
8. **Monitor with `s` every 5 s** for 90 seconds. K_motor should converge, Kp should stabilise (not ramp to infinity).
9. If the bot can't balance after K_motor converges → hardware/chassis issue, not control.

## See also

- [archive/session_records/2026-05-12_BENCH_BNO055_FROZEN_DIAGNOSIS.md](archive/session_records/2026-05-12_BENCH_BNO055_FROZEN_DIAGNOSIS.md) — the bench session that prompted this plan
- [archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) — the durable lessons doc
- [UNIVERSAL_BALANCE_BOT_VISION.md](UNIVERSAL_BALANCE_BOT_VISION.md)
- [AUTO_TUNING_REALITY_CHECK.md](AUTO_TUNING_REALITY_CHECK.md)
- [findings/dynamic_pwm_accel_learning.md](findings/dynamic_pwm_accel_learning.md) — the Phase 4.10 design being validated
- [findings/bootstrap_protocol_unstable_plant.md](findings/bootstrap_protocol_unstable_plant.md)
