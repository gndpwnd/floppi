# Implementation Plan — Balancing Robot Reference App
Last updated: 2026-05-12

## Goal

The user props the bot upright and releases it: the bot balances. The user picks the bot up: motors stop. The user sets the bot back down, upright on a surface: balance resumes. The user knocks the bot over past the tilt limit: motors stay off until the user explicitly restarts via the button, the `c` serial command, or the `R` serial command. No tuning UI, no per-boot input, no menus.

Three tiers. Tier 1 is parameter changes — high probability of unblocking balance on the existing bot. Tier 2 is the operator-experience fix (HELD vs. FALLEN) using real motion signals; only worth doing once Tier 1 proves the controller can hold the bot. Tier 3 is the deep sensor-pipeline replacement (drop BNO055 NDOF, run our own 2-state Kalman); only worth doing if Tier 1+2 still wobble.

## Hard constraints

The plan must respect these eight preferences. Numbered for reference elsewhere.

1. **Prop-and-go UX.** Power on, set bot upright, it balances. No serial commands required on a configured bot.
2. **PID gains are DYNAMIC, not persisted.** Hardcoded defaults on every boot; online adaptation refines them while running. Persisting gains defeats the framework's premise.
3. **BNO055 sensor cal persists** (it's a hardware property; takes minutes to redo). **Mount offset persists** (it's physical mounting geometry). **Nothing else persists.**
4. **No motor slew limiter.** The user dislikes the rate-of-change abstraction. Reach for "lower the PWM cap" instead.
5. **Full ±255 PWM range is reserved for non-balance modes** (motor test, future driving / remote-control). Balance mode caps lower (≤ ±100).
6. **No auto-recovery from FALLEN.** Operator must press button / send `c` / `R` to restart. The current sticky `SAFE_FALL` already implements this; keep that semantics under the new name.
7. **Distinguish HELD from FALLEN.** Picking the bot up must NOT trigger auto-restart. Setting it back down WHILE PROPPED UPRIGHT should resume balance with no input.
8. **Hardware is fixed.** Uno + BNO055 + L298N + cheap brushed DC motors. ESP32 / Teensy / WiFi / encoders are future-phase work, not in this plan.

## Strategy

Three tiers. Each tier names a single hypothesis, a measurable test, and a rollback procedure if the test fails. **Tier `N+1` does not start until Tier `N` is verified on the bench** — and verification means a pass/fail measurement, not a feeling.

The plan is biased toward stopping early. If Tier 1 succeeds we ship and move on. The framework's purpose is to *find tunings dynamically*, not to require ever-more-sophisticated infrastructure to balance one robot. Adding the 2-state Kalman before knowing whether the loop can balance at all would be putting Tier-3 cost behind a Tier-1 problem.

## Tier 1: Conservative gains + dead-band, slew limiter removed

**Hypothesis.** The .ino's legacy gains were tuned for a heavier Mega-based bot with a different chassis; the cheap L298N has a stiction floor of ~20–30 PWM below which the wheels do not turn at all. Together these explain ~80 % of the failures: at any small tilt the PID either commands a value below the stiction floor (no motion) or above the linear region (slam). Fixing those two and removing the slew limiter that masked the chatter near zero will produce a controller that holds the bot upright long enough for the rest of the system (online estimator, eventually auto-tune) to work.

Estimated effort: **2–3 hours** including bench verification.

### Tier 1 tasks

**1.1 Conservative default gains.** Two locations hold the hardcoded defaults and they disagree:

- `balance_app.cpp:56–59` — constants `kDefaultInitialKp=65 / Ki=12 / Kd=38` (still the .ino values).
- `main.cpp:316` — `balance_pid.set_tunings(18.0f, 0.0f, 22.0f)` (conservative-agent values, overriding the .ino values after `app.begin()`).

Agent recommendations conflict: conservative agent says Kp=18 / Ki=0 / Kd=22 ±120; diagnosis agent says Kp=15 / Ki=0 / Kd=8 ±100.

**Pick the diagnosis-agent's `Kp=15 / Ki=0 / Kd=8`.** Rationale: the diagnosis agent specifically called out the BNO055-NDOF quantization × Kd noise pathway (*balance_failure_diagnosis* §1c) which the conservative agent did not. With NDOF still in place (Tier 1 does not touch sensor pipeline), Kd=22 amplifies ~250 PWM/quantum of differentiated quantization noise; Kd=8 brings that to ~80. Raise Kd back toward 22 in Tier 3 once the Kalman gives a clean rate.

Edits:

- `balance_app.cpp:57–59` — set the three `kDefaultInitial*` constants to `Kp=15.0f / Ki=0.0f / Kd=8.0f`.
- `main.cpp:316` — change `set_tunings(18.0f, 0.0f, 22.0f)` to `set_tunings(15.0f, 0.0f, 8.0f)`. Both locations need updating: the constants are also consumed by native tests via `default_config()`.

Acceptance: fresh boot, `s` reports `Kp=15.00 Ki=0.00 Kd=8.00`.

**1.2 Replace slew limiter with a tighter output cap (Constraint 4).** `step_run_` (`balance_app.cpp:328–342`) implements a dynamic slew rate scaling with `|pitch|`. Delete it; rely on the PID's `set_output_limits()` clamp instead.

Edits:

- `balance_app.cpp:328–345` — delete the `max_slew` ladder and `delta` clamp. Replace with `motors_.set_speed(target_pwm); last_output_ = target_pwm;`. The PID has already clamped `target_pwm`.
- `balance_app.cpp:475` — `enter_run_with_current_gains` does `pid_.set_output_limits(-100.0f, 100.0f)`. **Change to ±80.** Leaves margin below worst-case L298N stiction asymmetry; the 10° FALLEN trip handles tipover catch.
- `balance_app.cpp:70–71` — leave `kDefaultOutputMin/Max` at ±255 (Constraint 5: full range reserved for non-balance modes; balance-mode cap lives in the `enter_run_with_current_gains()` path).

Replace the deleted slew-ladder comment block with one line: `// No slew limiter — operator preference. Output is clamped to ±80 by enter_run_with_current_gains().`

Acceptance: in RUN, `out=` in `s` status never exceeds ±80.

**1.3 Add a measurement IIR low-pass inside PID.compute().** BNO055 fused-Euler quantization (~0.05°) differentiated against `dt = 5 ms` injects noise that climbs with Kd. Diagnosis agent §3c calls for τ ≈ 15 ms (~10 Hz cutoff) one-pole IIR on the *measurement* feeding the D-term only — the P-term stays on raw measurement so step response is not delayed.

Edits:

- `pid_controller.h:131` — add two private members: `float measurement_lpf_;` and `bool measurement_lpf_init_;`.
- `pid_controller.cpp:25–48` — initialize: `measurement_lpf_(0.0f), measurement_lpf_init_(false)`.
- `pid_controller.cpp:140–149` — before the `d_on_measurement_` branch, run `alpha = dt_seconds / (0.015f + dt_seconds);` (≈0.25 at dt=5 ms), arm on first sample, then `measurement_lpf_ += alpha * (measurement - measurement_lpf_);`. Feed `measurement_lpf_` (not raw `measurement`) into the D-on-measurement branch. Set `last_measurement_ = measurement_lpf_` at function bottom.
- `pid_controller.cpp:165–175` — `reset()` sets `measurement_lpf_init_ = false`.

Acceptance: at rest in RUN, std-dev of `get_d_term()` < ~5 PWM over 10 s (pre-Tier-1 baseline shows 20–40).

**1.4 Bump the L298N stiction floor to 25 PWM.** Today `main.cpp:87` constructs the driver with `stiction_min_pwm=15`. `L298NMotorDriver::apply_stiction_` already implements `sign(speed) * stiction_min_pwm` for any non-zero command below the floor — exactly the ±25 dead-band feed-forward both agents recommend (conservative agent §"Other tweak"; diagnosis agent §3). Implementing it via the existing parameter is one number change and applies uniformly across RUN and motor-test paths.

Edit: `main.cpp:87` — change `15` to `25`.

Acceptance: bot can hold itself against small lean perturbations rather than dribbling below stiction floor.

### Tier 1 validation

**Test 1.A — Fresh-boot balance.** Power-cycle (no serial), wait for `READY` + 2 s grace, prop upright at saved mount-offset, release. **Pass:** bot stays within ±10° (FALLEN trip) for ≥ 10 s; motors stay within ±80 PWM throughout.

**Test 1.B — Quietness at rest.** Hold bot rigid at captured offset; run `s` 4–5 times over 20 s. **Pass:** `out=` < ±15 magnitude. (Pre-Tier-1 baseline: chatter hits ±40–60 even at rest.)

**Test 1.C — Recovery from 5° push.** Bot in RUN; gently push to ~5° lean and release. **Pass:** returns to within ±2° in ≤ 1.5 s with no overshoot past 8°.

### Tier 1 rollback

If Test 1.A fails:

1. Try `Kp=24, Kd=12` (conservative-agent mid-range) — Kp=15 may be too soft for a heavier-than-expected chassis.
2. Reinstate the slew limiter *temporarily* as a diagnostic. If reinstating fixes the failure, the slew was masking a different problem (sensor latency or actuator noise) — escalate to Tier 2 / Tier 3 rather than re-tuning Tier 1 indefinitely.
3. Send `R` to load legacy `Kp=65 / Ki=12 / Kd=38` for comparison. If those don't balance either, the problem is mechanical (battery sag, CoM above where we think it is, wheel slip) — software cannot rescue it; pivot to physical inspection.

## Tier 2: HELD/FALLEN state machine + raw gyro on sensor base

**Do this tier only if Tier 1 lands** (i.e., the bot can balance for ≥ 10 s) **AND** the user reports the operator-experience problems are now the dominant frustration: picking the bot up causes motors to spool, setting it down doesn't auto-resume, recovery requires too many commands.

**Hypothesis.** Pitch alone cannot distinguish "lying on its side" from "being lifted across the room then put back down upright." A held-up balanced-bot briefly traverses the upright pitch range while in the user's hand, and the current code (`balance_app.cpp:309`) would fire motors. The fix is *gyro magnitude + accel-magnitude deviation from 1 g* as the disambiguator. See *balance_held_fallen_state_machine.md* for the design — this tier just executes that doc.

Estimated effort: **5–7 hours** (1.5 hours for the abstraction change, 1 hour for the rename, 2 hours for the state machine itself, 2 hours for bench verification).

### Tier 2 tasks

**2.1 Promote `getRawGyro()` / `getRawAccel()` to `OrientationSensor` base.** Both methods exist on the concrete `BNO055` class (`bno055.cpp:179–199`, `bno055.h:128–135`) but not on the base. Lift them so `BalanceApp` can call polymorphically.

- `sensor_base.h:99–107` — extend `OrientationSensor`. Add `virtual bool getRawGyro(float xyz[3])` and `virtual bool getRawAccel(float xyz[3])` with defaulted-out implementations (`xyz = {0,0,0}; return false;`) so unchanged drivers (BNO085) still compile.
- `bno055.h:128–135` and `bno055.cpp:179–199` — promote to `override`, change return type `void` → `bool` (`true` on real read, `false` if `!initialized_`).
- BNO085 inherits the "not supported" stubs (raw access for BNO085 is Phase 5.5 work).

**2.2 Compute motion signals in `read_imu_`.**

- `balance_app.h:208–219` — add private members:
  - `float g_lateral_dps_lpf_;`   (lateral-axis gyro magnitude, LP)
  - `float a_dev_lpf_;`           (`|accel|−9.81`, LP)
  - `float a_align_;`             (`accel_z / |accel|`; 1 ≈ upright on surface)
  - `bool motion_filters_init_;`
  - `uint16_t hold_enter_count_; uint16_t hold_exit_count_; uint32_t held_entered_ms_;`
- `balance_app.cpp:535–542` — extend `read_imu_` per *state-machine doc* §5. Use `g_lat = sqrt(gx² + gz²)` rather than `|gyro|` — that's the trick (§3) suppressing false HELD entries during aggressive recoveries (intrinsic pitch-axis recovery has nearly zero roll/yaw component).

**2.3 Add `HELD` to `BalanceAppState`; rename `SAFE_FALL` → `FALLEN`.** Pure rename + one new state — no semantic change to existing FALLEN behaviour.

- `balance_app.h:69–75` — enum becomes `IDLE / CAPTURE_MOUNTING / AUTO_TUNE / RUN / HELD / FALLEN`.
- `balance_app.cpp:192–198` — extend dispatch switch with `case HELD: step_held_(now_ms); break;`.
- `balance_app.cpp:386–402` — rename `step_safe_fall_` → `step_fallen_`; behaviour unchanged.
- `balance_app.cpp:449–457` — update `state_name()` strings.
- `balance_app.cpp:481–532` — update log line + side-effect switch. Change log string at line 494 to `"FALLEN (motors off — press button or 'c'/'R' to restart)"`.
- `balance_app.h:225–229` — rename prototype.
- A `grep -rn SAFE_FALL src/` confirms only the references above plus the saturation-timeout log at `balance_app.cpp:360`.

**2.4 Implement `step_held_` per *balance_held_fallen_state_machine.md* §5.** Critical thresholds (numbers from doc §2, §4 — use verbatim):

- **RUN→HELD**: (`g_lateral_dps_lpf_ > 30` OR `a_dev_lpf_ > 3.0`) sustained 150 ms (30 ticks). Use a counter pattern matching the existing `recovery_count_`.
- **HELD→RUN**: `g_lateral_dps_lpf_ < 8` AND `|pitch_deg_| < 4°` AND `a_dev_lpf_ < 0.8` AND `a_align_ > 0.95`, sustained **800 ms** (160 ticks). On transition, call `pid_.reset()` to discharge any stale I-term.
- **HELD→FALLEN**: `|pitch_deg_| > 25°` sustained 200 ms.
- **HELD timeout**: 30 s in HELD without becoming RUN → FALLEN.

**2.5 Wire the RUN→HELD edge at the top of `step_run_`.** `balance_app.cpp:309–319` — after the existing tipover check (which must remain first; Constraint 6, falls are sticky), insert the HELD entry gate before the PID compute, so an in-progress lift suppresses motors within ~150 ms.

**2.6 Log + LED updates.** Extend the `enter_state_` log switch at `balance_app.cpp:489–495` with a HELD branch. Future LED driver should make HELD distinct from FALLEN (e.g., blue-pulse vs. red-solid).

### Tier 2 validation

**Test 2.A — Lift-and-replace.** Bot in RUN; operator lifts off surface, carries ~30 cm, sets back down upright. **Pass:** motors stop within ~200 ms on lift; stay off while carried; resume RUN within ~1 s of set-down. No restart command.

**Test 2.B — Tipover stays sticky.** Bot in RUN; push past ±15°. **Pass:** enters FALLEN; motors stay off even if operator stands the bot upright. Only `c` / `R` / button restores RUN.

**Test 2.C — Aggressive recovery doesn't false-trigger HELD.** Bot in RUN; poke to induce ±5° excursion + several oscillations. **Pass:** recovers without HELD transition (lateral-gyro gate works — see *state-machine doc* §3).

**Test 2.D — Held perfectly still doesn't false-recover.** Operator holds fallen bot upright in hand, still, for 5 s. **Pass:** bot stays in HELD; motors don't spool (`a_align > 0.95` gate keeps a hand-held chassis off).

### Tier 2 rollback

- Test 2.A slow: drop RUN→HELD dwell from 150 ms → 100 ms → 60 ms. Below 60 ms, accept that a 5 ms PID overshoot may trigger HELD — a brief motor pause is invisible; a slam while being held is dangerous.
- Test 2.C false-triggers: raise lateral-gyro threshold from 30 → 45 deg/s. If still false-triggering, suspect axis-mapping bug (pitch axis being read on `gyro_x`/`gyro_z`); verify `bno055.cpp:190–199` body-frame assignment before tuning.

## Tier 3: Drop NDOF for a 2-state Kalman pitch filter

**Do this tier only if Tier 1 lands AND Tier 2 lands** and there is still visible latency-induced wobble — i.e., the bot can balance and the operator-experience is right, but the motion is "tense" or "always overshooting by a tiny bit." Tier 3 is a 6-hour-plus piece of work with real risk of breaking things; do not start it speculatively.

**Hypothesis.** The BNO055's NDOF on-chip fusion adds ~20–40 ms of group delay (see *balance_failure_diagnosis* §1b and *bno055_latency_and_pitch_fusion*). At a ~600 ms natural-period pendulum, 30 ms is ~18° of phase lag, which roughly halves the achievable Kd before noise dominates signal. Replacing NDOF with raw gyro+accel feeding a 2-state Kalman (pitch + gyro_bias) cuts the sensor→actuator latency from ~30 ms to ~5 ms.

### Tier 3 tasks (sketch only — flesh out at the time)

1. Switch the BNO055 to `OPERATION_MODE_AMG` (raw mode, no on-chip fusion) — `bno055.cpp:92`. Loses the fused quaternion; gains raw accel + gyro + mag at register-read latency.
2. Implement `src/navigation/balance_kalman.{h,cpp}` per *MASTER_DESIGN* §4.7 and *balance_point_and_mounting_research* §3. Lauszus-pattern 2-state Kalman, ~60 LOC, ~40 bytes of RAM, ~150 µs per tick on AVR.
3. Feed the Kalman output into `BalanceApp::pitch_deg_` (replacing the current `imu_.getOrientation().pitch_deg` source). The D-term now operates on the Kalman's gyro-rate state directly — no differentiation of a quantized fused angle. Reset the PID measurement-LPF (Tier 1.3) to a much shorter τ or remove it entirely.
4. Re-run Tier 1's validation tests, expecting to raise Kd back toward the conservative agent's `Kd=22` (or higher) with the cleaner signal.
5. Optionally bump I2C clock from 100 kHz to 400 kHz (one-line change at `bno055.cpp:88`) — shaves another ~3 ms.

**Not included in this plan**: Madgwick on raw gyro+accel. If Tier 3 with a 2-state Kalman is insufficient (highly unlikely for a 2-wheel balancer), then Madgwick is the next escalation, but it's overkill for pitch-only and overhead for the AVR. See *balance_failure_diagnosis* §5 Tier 3.

## Out of scope (explicit non-goals for this plan)

- **Madgwick / Mahony on raw gyro+accel.** Tier-3 extended; only if 2-state Kalman is insufficient. Unlikely.
- **WiFi telemetry / ESP32 dashboard.** Different MCU target, different plan. Phase 6.
- **Auto-tune as part of boot flow.** Auto-tune is an explicit operator action only — `t` over serial or long-press the button on a tuning stand. Booting into AUTO_TUNE is dangerous (motors at relay amplitude before the operator is ready).
- **Persisting PID gains across reboots.** Constraint 2. Defeats the framework's premise.
- **Driving / remote-control mode.** Phase 4.8 (per *tetherless_operation_strategy.md*). Reuses the full ±255 motor range that Constraint 5 reserves.
- **I2C-bridge to the flight_controller project.** Phase 7.
- **Online estimator freeze-gate wiring with real gyro / windup signals.** This is a bug today (*balance_failure_diagnosis* §1d says the estimator is contaminated by every fall). Two-line fix to pass real signals into `online_est_.update(…)` at `balance_app.cpp:376–381`. Worth doing alongside Tier 2 (since Tier 2 already exposes the gyro magnitude as a member). **Defer until Tier 2 lands**; the right gating signal is `g_lateral_dps_lpf_` and `pid_.get_p_term() + pid_.get_i_term() > 0.7 * output_max`, both of which become available as a side effect of Tier 2's work.

## How to validate each tier (consolidated)

Each tier has its own validation tests in the sections above. Cross-tier observations:

- **Telemetry-log every test.** Capture `pitch`, `out`, `state`, `Kp/Ki/Kd`, and (Tier 2+) `g_lat`, `a_dev` over the test window. The `s` command's once-a-second snapshot is not enough — bump it to a periodic 50 Hz line during validation. Save to a file with the bench-test date.
- **Pre-Tier-1 baseline.** Before changing anything, run the same Tier 1 tests on the current code so we have a "before" telemetry trace to compare against. The diagnosis agent's claim "the controller has essentially zero linear region" is empirically verifiable: log the current code's `out=` value at small pitch perturbations and confirm saturation begins below ±3°.
- **One change at a time.** Within Tier 1, prefer landing gain change (1.1) + cap change (1.2) together as one bench test, then dead-band (1.4) separately, then LPF (1.3) separately. If a bench test fails, only the last change needs rolling back. Bunching all four into one flash means a regression is impossible to attribute.

## Open questions for the user before starting Tier 2

1. **HELD timeout fallback.** State-machine doc recommends 30 s in HELD without resumption → FALLEN. Alternative: → IDLE (lets the operator request auto-tune). Plan default: FALLEN. Override?
2. **HELD→RUN dwell.** Default 800 ms. 500 ms is snappier but risks finger contact; below 300 ms is unsafe. Operator preference?
3. **Lateral-gyro threshold.** Default 30 deg/s (state-machine doc §4 recommendation). Heavier chassis with aggressive recoveries may need 45. Default 30 (sensitive) or 45 (tolerant)?
4. **Tier-3 entry criterion.** What does "insufficient wobble" mean concretely? Suggested objective bar: pitch std-dev over a 60 s steady-state RUN > 1.0°. Confirm.

## Anticipated code-level snags (not flagged in agent findings)

- **Stale construction-time gains at `main.cpp:90`.** `PIDController balance_pid(65.0f, 12.0f, 38.0f, …)` is overwritten by `app.begin()` (→ `default_config()` → `set_tunings()`) and again by `main.cpp:316`. Three locations need to stay aligned. Tier 1 updates the constants AND `main.cpp:316`; the constructor values are cosmetic.
- **Comment/code mismatch at `balance_app.cpp:471–475`.** Comment says "Cap motor power during balance to ±150"; code calls `set_output_limits(-100, 100)`. Tier 1.2 fixes both when it changes the limit to ±80.
- **`step_run_` saturation timeout uses function-static state** (`balance_app.cpp:352–367`, `static uint32_t sat_start_ms = 0;`). Wrong on FALLEN→RUN re-entry — the static accumulates across the whole power cycle. Move to a class member and reset on `enter_state_(RUN)`. Two-line correctness fix; recommend folding into Tier 1.
- **`BalanceAppConfig::output_min/max` is ±255** in `default_config()`. `step_tune_` (`balance_app.cpp:269–306`) uses these directly, not the ±80 RUN cap. Benign today because the relay amplitude is 50 (`main.cpp:93`), but if amplitude rises the auto-tune path has no balance-mode cap. Worth noting; not in scope for Tier 1.
- **Periodic mount-save blocks the main loop.** `ps::commit()` on AVR/EEPROM is ~3.4 ms/byte; the 60 s save at `main.cpp:444–448` writes 8 bytes ≈ 27 ms ≈ 5 PID ticks. If the bot becomes twitchy at exactly the minute marks, this is why; deferred-write would fix it.

## Document references

- **Design direction (read first)**: [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — current project direction; what got removed (slew limiter, auto-recovery from FALLEN, persisted gains), what got kept, and the `USE_BALANCE_HELD_DETECTION` / `USE_BALANCE_FALL_DETECTION` compile-time switches.
- **Tier 1 gains and dead-band**: [docs/findings/balance_failure_diagnosis_2026-05-12.md](findings/balance_failure_diagnosis_2026-05-12.md) §3, §5 Tier 1. Numbers used: Kp=15 Ki=0 Kd=8, ±100→±80 cap, dead-band recommendation. **Conflict with** [docs/findings/conservative_balance_gains_recommendation.md](findings/conservative_balance_gains_recommendation.md) (Kp=18 Kd=22 ±120) — chose the diagnosis values because of the NDOF-quantization × Kd noise argument. Conservative agent's slew rates (4/15/40) discarded per Hard Constraint 4.
- **Tier 1 PID measurement LPF**: [docs/findings/balance_failure_diagnosis_2026-05-12.md](findings/balance_failure_diagnosis_2026-05-12.md) §3c (15 ms τ on D-term measurement).
- **Tier 2 HELD/FALLEN state machine**: [docs/findings/balance_held_fallen_state_machine.md](findings/balance_held_fallen_state_machine.md) — entire document. Thresholds and dwell times used verbatim.
- **Tier 2 raw-gyro abstraction**: [docs/findings/balance_failure_diagnosis_2026-05-12.md](findings/balance_failure_diagnosis_2026-05-12.md) §4(1).
- **Tier 3 2-state Kalman**: [docs/findings/MASTER_DESIGN.md](findings/MASTER_DESIGN.md) §4.7 D6 + [docs/findings/balance_point_and_mounting_research.md](findings/balance_point_and_mounting_research.md) §3.
- **Background — why the bot doesn't balance today**: [docs/findings/balance_failure_diagnosis_2026-05-12.md](findings/balance_failure_diagnosis_2026-05-12.md) §1, §2.
- **User preferences (Hard Constraints)**: [docs/archive/session_records/2026-05-12_uno_balancing_hardware.md](archive/session_records/2026-05-12_uno_balancing_hardware.md) §"User preferences captured in this session".
- **Online estimator freeze-gate bug context**: [docs/findings/online_adaptive_balance_tracking.md](findings/online_adaptive_balance_tracking.md) §3, §5.
- **Auto-tune scope rationale**: [docs/findings/auto_pid_tuning_research.md](findings/auto_pid_tuning_research.md) §2.1 (why Ki=0 until balance is proven).
