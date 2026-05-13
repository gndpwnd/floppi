# Session Record — 2026-05-12 Evening — Balance-Bot Iteration

Continuation of the same-day bench session (`2026-05-12_uno_balancing_hardware.md`). Hardware: Arduino Uno + BNO055 + L298N + cheap brushed DC motors.

> See also: [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) for the design direction this session locked in.

## What this session was

The morning session left the bot with: Kp=65/Ki=12/Kd=38 (legacy .ino), slew limiter, sticky FALLEN at 10°, ±100 cap, dynamic slew ladder, mount offset persistence working. The bot didn't balance — motors slammed during any small tilt, falls were "caught" but recovery misfired when the operator picked the bot up.

This session: replaced all of that with conservative PID + no state-machine pauses. Ended with the bot under iteration — current state captured below.

## Direction change

Mid-session the user articulated the design direction explicitly, after a chain of incremental fixes that kept missing:

> "we don't need a motor slew or something we need to just do lower PWM in general"
>
> "we want the the robot to start in any orientation and find where no or little accelerations happen"
>
> "constant autobalancing so if there is not alot of acceleration we don't need alot of motor activity make sense? and the robot simply wants to just stop all or as many accelerations as possible"
>
> "the robot should know how much accelerations in what directions are caused by how much PWM to each motor"

That framing became `MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`. The downstream consequences:
- Fall detection off by default (no FALLEN state in the happy path).
- HELD detection off entirely at first; later re-enabled but made *lenient* (no surface-alignment, no dwell).
- PID gains hardcoded, not persisted — adaptation is online, not in EEPROM.
- Future Phase 4.10 will literally implement "the robot knows PWM → acceleration" via online system ID (scalar RLS).

## Code changes that landed

### Tier 1 (conservative PID + dead-band)
- `balance_app.cpp` — kDefaultInitial gains 65/12/38 → eventually 18/3/10 after iteration.
- `pid_controller.{h,cpp}` — added measurement IIR LPF on D-term (configurable τ via `set_d_term_lpf_tau_sec`). Default τ = 5 ms.
- `main.cpp` — L298N stiction_min_pwm 15 → 25 → 18 (iterated based on bench feel).
- `balance_app.cpp` — removed dynamic slew ladder entirely. Output cap raised from ±100 → ±150 → ±200 → ±255 (no balance-mode cap).

### Snags from the IMPLEMENTATION_PLAN agent
- Saturation-timeout `static uint32_t sat_start_ms` → class member `sat_run_start_ms_` reset on RUN entry.
- Comment/code mismatch fixed at `enter_run_with_current_gains` (was "Cap to ±150" with code at ±100).
- Constructor-time stale `PIDController(65, 12, 38, ...)` aligned with the default-config values.

### Tier 2 (HELD/FALLEN state machine)
- `sensor_base.h` — added `virtual bool getRawGyro(float xyz[3])` and `getRawAccel(...)` with "not supported" defaults.
- `bno055.{h,cpp}` — promoted existing methods to override the base. Return type `void → bool`.
- `balance_app.cpp` `read_imu_` — computes `g_lateral_dps_lpf_`, `a_dev_lpf_`, `a_align_` via one-pole IIR per tick (~120 ms τ).
- Enum: added `HELD = 4`, renamed `SAFE_FALL → FALLEN = 5`.
- New `step_held_()`. `step_fallen_()` rewritten as sticky (no auto-recovery).
- `on_short_press` updated: HELD → force RUN (skip dwell), FALLEN → restart RUN directly.

### Compile flags
- `USE_BALANCE_FALL_DETECTION` — gates tipover→FALLEN in `step_run_`. DEFAULT OFF.
- `USE_BALANCE_HELD_DETECTION` — gates RUN→HELD in `step_run_` AND HELD-related code paths. DEFAULT ON.
- Default build flags string in `platformio.ini` updated accordingly.

### HELD logic lenience (iteration)
- Initial: strict 4-condition gate (g, accel, pitch, a_align), 800 ms dwell, 30 s timeout to FALLEN.
- After user feedback "the motors stop and don't do anything until set back against the counter": dropped a_align check, dropped timeout, dropped dwell from 800 → 200 ms, raised thresholds 8→12 dps and 0.8→1.5 m/s².

### Latency-targeting
- BNO055 `Wire.setClock(400000UL)` — 4× faster I²C (~3 ms saved per read).
- D-term LPF τ dropped 15 ms → 5 ms (~10 ms less phase lag).
- Ki = 0 → 3 (small integral so slow drift is chased before becoming a visible "delay").

### Tests
- `test_balance_app.cpp` SAFE_FALL → FALLEN rename.
- Two auto-recovery tests deleted (behaviour removed).
- New tests: `test_fallen_is_sticky`, `test_short_press_restarts_from_fallen`, all gated by `#ifdef USE_BALANCE_FALL_DETECTION`.
- `test_pid_controller.cpp` — added `set_d_term_lpf_tau_sec(0.0f)` to the PID_v1 strict-parity test (LPF defeats parity; disabling restores it).

## Docs that landed

| Doc | Purpose |
|---|---|
| [KNOWN_ISSUES.md](../../KNOWN_ISSUES.md) | 19 issues KI-2..KI-20, severity-ranked. KI-11 (no HELD) and KI-13 (slew wrong primitive) addressed in this session. |
| [IMPLEMENTATION_PLAN.md](../../IMPLEMENTATION_PLAN.md) | 3-tier plan. Tier 1 fully landed. Tier 2 landed with the lenient-HELD modification. Tier 3 deferred (Kalman). |
| [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) | Design direction. Why state machines were removed. Compile-flag matrix. |
| [findings/balance_failure_diagnosis_2026-05-12.md](../../findings/balance_failure_diagnosis_2026-05-12.md) | Root cause: legacy gains + NDOF latency. |
| [findings/conservative_balance_gains_recommendation.md](../../findings/conservative_balance_gains_recommendation.md) | Numbers we landed on. |
| [findings/balance_held_fallen_state_machine.md](../../findings/balance_held_fallen_state_machine.md) | Original strict state machine; superseded by the lenient version in code. |
| [findings/latency_budget_2026-05-12.md](../../findings/latency_budget_2026-05-12.md) | Bottleneck: BNO055 NDOF group delay 20–40 ms. Next: AMG mode + atan2. |
| [findings/dynamic_pwm_accel_learning.md](../../findings/dynamic_pwm_accel_learning.md) | Phase 4.10 — scalar RLS on K_motor as v1. |
| [findings/multi_axis_anomaly_handling_detection.md](../../findings/multi_axis_anomaly_handling_detection.md) | Phase 4.7c — replaces 2-signal HELD detector with novelty detection. |

Cross-references to the philosophy doc added across 11 entry-point documents by background agent.

## What's flashed at end of session

See [`project_balance_bot_state_2026-05-12`](../../../../.claude/projects/-home-devel-floppi/memory/project_balance_bot_state_2026-05-12.md) (in agent memory) for the canonical state.

Headline: Kp=18 Ki=3 Kd=10, ±255 cap, 18 PWM stiction, 5 ms D-term LPF, 400 kHz I²C, HELD on (lenient), FALLEN compile-disabled.

## Open / pending

1. **BNO055 calibration UX feedback** — user reports the cal values change over time and the process "is a bit of a mess". This is expected BNO055 NDOF behavior (the chip continuously re-evaluates cal during operation; values DO go up and down). User explicitly didn't want to recalibrate. Documenting as a known UX concern; the actual cal blob saved to EEPROM is from a moment when all four fields hit 3.
2. **Bench feedback on the latest flash** — pending. Looking for: does the bot now balance? Is HELD still pausing too long when picked up? Are motors finally moving with enough authority?
3. **Phase 4.7c novelty detector** — designed (`multi_axis_anomaly_handling_detection.md` by agent). Not implemented; would replace the current 2-signal HELD detector with a learned-distribution version per the user's "merge between multiple axes" framing.
4. **Phase 4.10 system ID** — designed (`dynamic_pwm_accel_learning.md`). Not implemented; this is the long-term answer to the user's "robot should know how much PWM produces how much acceleration".
5. **Latency-bottleneck fix** — switch BNO055 to `OPERATION_MODE_AMG` and compute pitch from `atan2(ax, az)`. ~1 hour. Recommended by latency agent. Not done — held until user confirms current flash before applying another change.

## Hardware observations from this session

- BNO055 calibration values fluctuate during operation. Normal. The 22-byte EEPROM blob is what matters.
- Motors at 18 PWM stiction floor may not be enough for one of the user's motors due to manufacturing asymmetry. User may need 22-25.
- Battery sag is suspected but not measured. Phase 4.10 system ID would auto-compensate.
- Bot's natural period is ~600 ms (small chassis), so every 10 ms of latency = 6° of phase lag.
