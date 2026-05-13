# Known Issues — Auto Orientation / Balancing Robot Reference App
Last updated: 2026-05-12 (Uno + BNO055 bench session)

> **Design direction**: see [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) for the project's current direction — KI-11 (no HELD state) and KI-13 (slew limiter wrong primitive) are addressed there.

## Status Summary

The mechanical layer works. Motors spin both directions and respond linearly to PWM. The IMU streams a fused quaternion, BNO055 calibration persists across reboots, the mounting offset persists, and the state machine boots cleanly into auto-RUN. Despite this, the robot **does not stably balance**. The failure is not a single bug — it is approximately a dozen stacked issues, ranging from PID gains tuned for a different chassis, to ~30 ms of sensor latency we cannot tolerate, to a state machine that cannot distinguish a bot being held in a hand from a bot that has just rocked back upright on its own. The issues are listed below by severity. KI-1 is the AVR-only EEPROM bug already noted in repo memory; new issues from this session start at KI-2.

---

## Critical (blocks balance from working at all)

### KI-2: PID gains hardcoded for a different physical bot

- **Symptom:** From the first tick after `RUN` is entered the motors slam to the output cap on any tilt larger than ~2°. The bot never enters a linear control region.
- **Root cause:** `Kp=65 / Ki=12 / Kd=38` were copied wholesale from the archived `SelfBallancingRobot3.ino` (`docs/archive/balancing_robot_reference/`), which was a Mega-based bot with a different chassis, different motors, different mass and different mounting. At Kp=65 and the current `±100` RUN cap, P-term alone saturates at `100/65 = 1.54°` of pitch error. The controller has essentially zero linear region.
- **Evidence:**
  - `auto_orientation/src/main.cpp:90` — `PIDController balance_pid(65.0f, 12.0f, 38.0f, -255.0f, 255.0f);`
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:57-59` — `kDefaultInitialKp/Ki/Kd` also baked at 65/12/38.
  - `auto_orientation/src/main.cpp:316` — boot path tries `set_tunings(18.0f, 0.0f, 22.0f)` (gentler) but `enter_run_with_current_gains` at `main.cpp:340` is hit only when a mount offset was loaded, and the auto-RUN message at `main.cpp:337` still claims "default gains (Kp=65 Ki=12 Kd=38)". The hardcoded-default story is muddled across two layers.
  - `'R'` serial command at `main.cpp:377` reinstates the legacy gains in-session.
- **Why fixing this matters:** Without gains scaled to this chassis the bot cannot stay in the linear region long enough for the relay-feedback auto-tuner or the online mounting estimator to produce useful signal. Every other fix is downstream of this.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §1a; `docs/findings/conservative_balance_gains_recommendation.md` (recommends `Kp=18 Ki=0 Kd=22` with `±120` cap).

### KI-3: BNO055 NDOF group delay is 20-40 ms, balance loop needs <10 ms

- **Symptom:** By the time the PID sees a 5° lean the bot is already at ~7°. By the time the motor command propagates back through the bridge, the bot is at SAFE_FALL. The slamming the user observes is the controller correctly responding to an emergency the sensor reported too late.
- **Root cause:** The on-chip BNO055 NDOF fusion has an internal moving-average plus MEMS-fusion pipeline with a documented ~20-40 ms group delay (Bosch BST-BNO055-DS000 §3.6.5). Adding I2C transfer at 100 kHz (~5 ms per quaternion read) brings the sensor-to-actuator path to ~25-45 ms. A small bot has a natural inverted-pendulum period of ~500-700 ms; 30 ms of lag is ~18° of phase, which halves the achievable Kd before noise overwhelms signal.
- **Evidence:**
  - `auto_orientation/src/sensors/bno055.cpp:88` — `Wire.begin()` runs at framework-default 100 kHz; 400 kHz is supported by the part but not enabled.
  - `auto_orientation/src/sensors/bno055.cpp:92` — `bno_->begin(OPERATION_MODE_NDOF)` puts the part into on-chip-fusion mode.
  - `auto_orientation/src/sensors/bno055.cpp:145` — `getQuat()` returns the fused quaternion, which is what `BalanceApp` consumes.
- **Why fixing this matters:** This is the architectural reason the bot cannot balance even with correct gains. Reference balancers (Lauszus / Balanduino / Brokking YABR) achieve ~5 ms sensor-to-actuator using raw IMU registers plus a 2-state Kalman on the host. We are 3-5x over budget.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §1b and §2 (latency budget table); `docs/findings/MASTER_DESIGN.md` §4.7 ("2-state Kalman, NOT the 16-state EKF").

### KI-4: Online mounting estimator is fed fake signals, contaminating its average

- **Symptom:** The "balance point" the estimator converges to is the running mean of all the bot's tipovers, not the true mount offset. `corrected_pitch_()` then subtracts this poisoned offset from the measurement, biasing the PID against a moving target that wanders further every fall.
- **Root cause:** The online estimator design requires it to be **frozen** during `|I| > 0.7·I_max`, `|θ̇| > 1°/s`, or motor saturation. The `BalanceApp` skeleton acknowledges this with a TODO but currently passes `windup_active=false` and `gyro_pitch_dps=0.0f` unconditionally.
- **Evidence:** `auto_orientation/src/applications/balancing_robot/balance_app.cpp:374-381`:
  ```
  online_est_.update(i_term, pitch_deg_,
                     /*tipover_active=*/false,
                     /*windup_active=*/false,
                     /*gyro_pitch_dps=*/0.0f,
                     now_ms);
  ```
- **Why fixing this matters:** Until the estimator's freeze gates work, the long-term "I-channel" the user wants in place of a persisted Ki cannot function. The estimator is meant to be the slow integrator; right now it's a slow corrupter.
- **Linked finding:** `docs/findings/online_adaptive_balance_tracking.md` §5 (freeze gates spec); `docs/findings/balance_failure_diagnosis_2026-05-12.md` §1d.

### KI-5: `OrientationSensor` base class exposes no raw gyro

- **Symptom:** The balance loop cannot see angular velocity. The HELD-vs-FALLEN discriminator the user wants (and the 2-state Kalman the architecture wants) both need raw gyro. We cannot build either against the current sensor abstraction.
- **Root cause:** `OrientationSensor` was designed for a fused output (quaternion + Euler + cal status). `bno055.cpp` already implements both `getRawAccel(float[3])` and `getRawGyro(float[3])` as concrete methods, but they are not virtuals on the abstract base, so `BalanceApp` (which holds an `OrientationSensor&` reference) cannot call them.
- **Evidence:**
  - `auto_orientation/src/sensors/bno055.cpp:179-188` — `getRawAccel` exists on the concrete class.
  - `auto_orientation/src/sensors/bno055.cpp:190-199` — `getRawGyro` also exists on the concrete class.
  - `auto_orientation/src/applications/balancing_robot/balance_app.h:196` — `BalanceApp` holds `OrientationSensor& imu_`, not `BNO055& imu_`.
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:11-17` — implementation note labels this as a Phase 4.6.5 deliverable.
- **Why fixing this matters:** Without raw gyro exposed on the base class, every other diagnostic that needs angular rate — HELD/FALLEN discrimination, windup detector for KI-4, direct rate-feedback for the D-term, future 2-state Kalman — has to either downcast to BNO055 (breaks the abstraction) or fake the rate from numerical differentiation (re-injects all the NDOF latency).
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §4(1); `docs/findings/balance_held_fallen_state_machine.md` §1.

---

## High (degrades balance significantly)

### KI-6: PID anti-windup is plain clamping, not back-calculation (the comment lies)

- **Symptom:** When `set_output_limits()` shrinks the range mid-run, any already-accumulated I-term silently truncates — legitimate integration history is lost. When `ki` is changed (e.g. by the auto-tuner), the clamp rescales the limit and can momentarily fight itself.
- **Root cause:** The implementation header comment claims back-calculation but the code is integral clamping. Back-calculation requires a `u_clamped - u_unclamped` term fed back into the integrator; that term does not exist.
- **Evidence:**
  - `auto_orientation/src/control/pid_controller.cpp:1-16` — header comment: *"Anti-windup strategy: integral clamping (back-calculation style)."* The phrasing is contradictory, and the body is clamping-only.
  - `auto_orientation/src/control/pid_controller.cpp:130-132` — `integral_ += error * dt_seconds; clamp_integral_(); const float i_term = ki_ * integral_;`
  - `auto_orientation/src/control/pid_controller.cpp:193-204` — `clamp_integral_()` bounds `integral_` such that `ki * integral` fits the output span, then returns. There is no anti-windup term subtracted from the integral on saturation.
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:475` — `pid_.set_output_limits(-100.0f, 100.0f)` on entering RUN narrows the range from the constructor's ±255, which silently truncates any accumulated integral on every state entry.
- **Why fixing this matters:** Until anti-windup is real, the auto-tuner cannot trust the I-channel during relay feedback, and the online estimator (KI-4) cannot use the I-term as a clean drift signal.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §4(3).

### KI-7: D-term differentiates a quantized, latent fused signal — no measurement LPF

- **Symptom:** Near-zero pitch, the D-term chatters at the BNO055 Euler quantization step (~0.05°). At Kd=38 and dt=5 ms, one quant step is ~380 PWM. The slew limiter visually masks this near zero but the noise adds to genuine rate during real recovery, roughly doubling apparent dθ/dt.
- **Root cause:** `pid_controller.cpp` differentiates the raw `measurement` directly with no low-pass filter. Reference balancers low-pass the rate signal before differentiating; we lost that primitive when we adopted on-chip NDOF fusion.
- **Evidence:**
  - `auto_orientation/src/control/pid_controller.cpp:140-149` — D-term takes `(measurement - last_measurement_) / dt_seconds` with no IIR / biquad / one-pole filter.
  - `auto_orientation/src/sensors/bno055.cpp:154-157` — `quaternion_to_euler_degrees` rounds the fused quaternion to float degrees; the per-step jitter floor is ~0.05°.
- **Why fixing this matters:** Even with cut gains (KI-2), Kd amplifies quantization noise into measurable PWM chatter. A 15 ms IIR LP would be four lines and cut this without touching the genuine recovery bandwidth.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §1c and §3c.

### KI-8: I2C bus runs at 100 kHz instead of 400 kHz

- **Symptom:** Quaternion read takes ~5 ms instead of ~1.25 ms. That's ~3.75 ms of avoidable latency added to the already-bloated sensor pipeline (KI-3).
- **Root cause:** The driver follows the legacy `.ino`'s lazy default. BNO055 supports 400 kHz Fast Mode and the Arduino Wire library accepts `Wire.setClock(400000)`.
- **Evidence:** `auto_orientation/src/sensors/bno055.cpp:85-88`:
  ```
  // BNO055 supports 400 kHz, but we follow the reference
  // sketch and let Wire run at its framework default (100 kHz). Faster clocks
  // can be enabled in a follow-up build flag if needed.
  Wire.begin();
  ```
- **Why fixing this matters:** Free 4x speedup on the most expensive recurring transaction in the loop. Cannot fix KI-3 alone but reduces the unavoidable I2C portion.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §2 (latency table).

### KI-9: No 2-state Kalman pitch filter — relying on vendor fusion

- **Symptom:** The inner balance loop runs on the BNO055's on-chip fused Euler, inheriting its full latency budget and offering no gyro-bias tracking.
- **Root cause:** `MASTER_DESIGN.md §4.7` calls out `src/navigation/balance_kalman.{h,cpp}` as a planned module. It does not exist. The plan is the Lauszus 2-state filter on raw `accel.y / accel.z` plus `gyro.x`-as-rate — ~60 LOC, ~40 B RAM, ~150 µs/tick on AVR.
- **Evidence:** No file matches `balance_kalman` under `auto_orientation/src/`. The intent is documented but unimplemented.
- **Why fixing this matters:** This is the architectural fix the research has been pointing at since day one. Pairs with KI-5 (need raw gyro exposed) and is the prerequisite for switching BNO055 from `OPERATION_MODE_NDOF` to `OPERATION_MODE_AMG`, which removes the dominant latency term in KI-3.
- **Linked finding:** `docs/findings/MASTER_DESIGN.md` §4.7 last bullet; `docs/findings/balance_point_and_mounting_research.md` §3.

### KI-10: PID output cap is inconsistent across the code/docs/session log

- **Symptom:** Different parts of the system disagree on the RUN-state output cap. This makes "is the bot saturating?" un-diagnosable without re-reading source each time.
- **Root cause:** The number drifted across session iterations without converging.
- **Evidence:**
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:475` — `set_output_limits(-100.0f, 100.0f)` on RUN entry. Code is currently at ±100.
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:472-474` — code comment claims "Cap motor power during balance to ±150" while the next line caps at ±100.
  - `auto_orientation/docs/archive/session_records/2026-05-12_uno_balancing_hardware.md` line 38 — *"PID output capped at ±150 during RUN"*.
  - `auto_orientation/docs/findings/conservative_balance_gains_recommendation.md` line 37 — recommends ±120.
- **Why fixing this matters:** Whatever the right number is, it needs to match in one place and propagate to docs. Right now `balance_failure_diagnosis_2026-05-12.md §3e` had to call out the mismatch explicitly to nobody's benefit.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §3e (notes the discrepancy).

---

## Medium (affects UX or robustness, not core balance)

### KI-11: SAFE_FALL state has no HELD-vs-FALLEN distinction

- **Symptom:** When the user picks the fallen bot up to put it back on the ground, the bot briefly traverses near-zero pitch in the user's hand. The previous auto-recovery path interpreted this as recovery and spooled motors on while the operator was still holding the bot. The current (stop-gap) fix is to make SAFE_FALL "sticky" — but that means the user **cannot** lift the bot back to upright at all; they have to short-press or send `c`/`R` over serial first.
- **Root cause:** A pitch-only state machine literally cannot tell "user is lifting fallen bot" apart from "bot rocked back upright". The discriminator is **motion** — lateral gyro (roll/yaw axes) and accel-magnitude deviation from 1 g — neither of which the framework currently exposes (see KI-5).
- **Evidence:**
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:386-402` — `step_safe_fall_()` is now sticky; only `safety_.abort_requested()` returns it to IDLE.
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:391-393` — code comment explicitly identifies this as a known issue: *"the previous auto-recovery (pitch back to level + dwell) misfired whenever the operator picked the fallen bot up... A proper HELD/FALLEN distinction needs gyro motion sensing"*.
  - `auto_orientation/src/applications/balancing_robot/balance_app.h:74-75` — enum has only `RUN` and `SAFE_FALL`; no `HELD`.
- **Why fixing this matters:** The current behavior puts the burden of recovery entirely on the operator. The user explicitly wants prop-and-go UX; that is incompatible with "you must touch the serial monitor to restart after every fall".
- **Linked finding:** `docs/findings/balance_held_fallen_state_machine.md` — entire document is the design for the fix (proposes `g_lateral`, `a_dev`, 800 ms HELD→RUN dwell).

### KI-12: SAFE_FALL stickiness — no operator-friendly recovery path on hardware

- **Symptom:** After a fall, the bot is dead until the operator types something into a serial monitor. There is a button on pin D4 but its recovery semantics are partial.
- **Root cause:** `on_short_press` from SAFE_FALL sets `recovery_count_` to the threshold so the *next* in-bounds sample transitions — but `step_safe_fall_()` no longer checks `recovery_count_` after the stickiness rewrite. The button's stated semantics and the state handler's actual code don't align.
- **Evidence:**
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:412-420` — `on_short_press` in `SAFE_FALL` sets `recovery_count_ = cfg_.recovery_consecutive_samples` and comments that *"the next step() observes recovery_count_ already at threshold and... transitions to RUN"*.
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:386-402` — but `step_safe_fall_()` reads neither `recovery_count_` nor `pitch_deg_`. It only checks `safety_.abort_requested()`. The button's effect is dead code.
- **Why fixing this matters:** The mismatch makes the bot effectively unrecoverable without a serial monitor, which the user has called out as the worst part of the current UX.
- **Linked finding:** Same as KI-11.

### KI-13: Motor slew limiter is the wrong primitive

- **Symptom:** Slew bands govern "how fast PWM can change", not "how loud the motors are". User explicitly stated: *"we don't need slew, just lower PWM in general"*. The current 6 PWM/cycle slew in the linear band (3°) is in fact **slower than the plant's own dynamics** through that region — the controller cannot apply the command it actually computed.
- **Root cause:** Slew rate was used as a soft "don't slam motors" knob because it was the quickest thing to add, but the right knob is the output cap (KI-10) plus a deadband-compensated PWM scale.
- **Evidence:**
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:333-342` — current bands: `<3°: 6`, `<8°: 20`, `else: 50`.
  - `auto_orientation/docs/archive/session_records/2026-05-12_uno_balancing_hardware.md` §7 — slew limiter introduced as a fix; user later said it was the wrong fix.
  - `auto_orientation/docs/findings/balance_failure_diagnosis_2026-05-12.md` §3d — *"Slew=6 in 5 ms = 1200 PWM/s — slower than the plant's own dynamics through the linear region."*
- **Why fixing this matters:** Removing or relaxing slew is a one-line change. Keeping the cap mechanism (KI-10) and the dead-band primitive (KI-14) is where the real "lower PWM" effect should come from.
- **Linked finding:** `docs/findings/balance_failure_diagnosis_2026-05-12.md` §3d.

### KI-14: Motor stiction floor at 15 PWM is too low for cheap DC + L298N

- **Symptom:** Small PID commands (~5-25 PWM) produce no motion. The PD loop has to grow the integrator until it crosses the real stiction threshold, by which point it has overshot.
- **Root cause:** `L298NMotorDriver` constructor takes `stiction_min_pwm = 15` by default, but cheap DC motors with brushed gearboxes on a 6-7 V battery typically have a ~25-30 PWM stiction floor, asymmetric per direction (see session-log §6, "right wheel reverse sluggish").
- **Evidence:**
  - `auto_orientation/src/main.cpp:87` — `L298NMotorDriver motors(motor_pins, /*stiction_min_pwm=*/15);`
  - `auto_orientation/src/actuators/l298n_motor_driver.h:71` — default 15.
  - `auto_orientation/docs/findings/conservative_balance_gains_recommendation.md` line 45-48 — recommends "add a 5-PWM motor dead-band compensation (add ±5 to any non-zero command) so the L298N actually starts turning at small commands".
  - `auto_orientation/docs/archive/session_records/2026-05-12_uno_balancing_hardware.md` §6 — right motor reverse is sluggish (asymmetric stiction), mitigated only by bumping test PWM from 50 to 90.
- **Why fixing this matters:** Stiction is a nonlinearity in the plant. Either the driver compensates (with per-direction floors) or the PID has to. Right now neither does effectively, and the controller has to overshoot to break free.
- **Linked finding:** `docs/findings/conservative_balance_gains_recommendation.md` "dead-band compensation".

### KI-15: Auto-tune at startup is the wrong UX (already partly fixed but still wrong on long-press)

- **Symptom:** User wants prop-and-go on boot, NOT "run a 30-second motor-thrashing tune every time the chip resets". Capture-to-auto-tune was decoupled mid-session (good), but the long-press button gesture in IDLE still drops directly into AUTO_TUNE.
- **Root cause:** The user's mental model is that auto-tune is an explicit operator action on a tuning stand, not a startup behavior or an idle-time gesture.
- **Evidence:**
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:430-438` — `on_long_press` from IDLE: *"the user, on a long press, jump straight into AUTO_TUNE using whatever offset is currently loaded"*.
  - `auto_orientation/src/applications/balancing_robot/balance_app.cpp:256-261` — capture path explicitly comments *"SAFETY: do NOT auto-transition to AUTO_TUNE here"* — the recently-added fix.
  - `auto_orientation/docs/archive/session_records/2026-05-12_uno_balancing_hardware.md` §4 — *"User triggered auto-tune before connecting motor power, then connected battery during tune → motors slammed."*
- **Why fixing this matters:** The user has stated 'prop-and-go' UX is non-negotiable. Tune-as-IDLE-long-press is a footgun.
- **Linked finding:** Session record §4 (the slamming incident); session record "User preferences captured" #7 (*"Auto-tune is on-demand, not on-boot — `t` only fires when operator says"*).

---

## Low / Backlog

### KI-16: Boot path message mismatches actual applied gains

- **Symptom:** Serial log says `"auto-RUN with default gains (Kp=65 Ki=12 Kd=38)"` but the gains actually applied just three lines earlier are `(18.0f, 0.0f, 22.0f)`. Confuses anyone reading the log to verify what's running.
- **Evidence:** `auto_orientation/src/main.cpp:316` applies (18, 0, 22). `auto_orientation/src/main.cpp:337` prints "(Kp=65 Ki=12 Kd=38)".
- **Why fixing this matters:** Pure log-quality bug. Documentation accuracy.

### KI-17: `'R'` serial command resets gains to the bad legacy values

- **Symptom:** Operator presses `R` thinking it "restarts" — actually applies `Kp=65 Ki=12 Kd=38` from the .ino, the gains documented as unstable for this bot.
- **Evidence:** `auto_orientation/src/main.cpp:374-378` — `R` calls `balance_pid.set_tunings(65.0f, 12.0f, 38.0f); app.enter_run_with_current_gains(now);`.
- **Why fixing this matters:** A "reset" command that applies known-bad gains is a footgun.

### KI-18: BNO055 calibration accept threshold reduced to `gyro=3 && accel=3` only

- **Symptom:** Calibration completes without `mag=3` or `sys=3`. The relaxation was made because indoor magnetic environments rarely yield `mag=3` and balance only needs pitch. But `sys` being below 3 indicates the chip's internal fusion still considers itself uncalibrated — which is exactly the condition that adds variance to the fused Euler we feed the PID.
- **Evidence:** `auto_orientation/src/main.cpp:240-244` — gate is `if (d.cal_gyro == 3 && d.cal_accel == 3)`. Comment: *"saving (mag/sys don't care for balance)"*.
- **Why fixing this matters:** Possible interaction with KI-7 (D-term noise on a fused signal that the chip itself flags as uncertain). Worth measuring whether `sys` quality correlates with pitch jitter.

### KI-19: Capture path has no path through `MountingCalibration` — uses Welford on pitch instead

- **Symptom:** The Phase 4.3 mounting capture logic in `navigation/mounting_calibration.h` is unused. Capture instead runs a stillness gate on the Euler pitch and synthesizes a pitch-axis offset.
- **Evidence:** `auto_orientation/src/applications/balancing_robot/balance_app.cpp:215-267` — `step_capture_()` uses a Welford running variance on `pitch_deg_` and writes the mean directly to `online_est_.reset_to_reference()`. `mounting_.start_capture()` is never called.
- **Why fixing this matters:** Once KI-5 is fixed, the proper `MountingCalibration::feed_sample` path (with raw accel + gyro) becomes available. The current path is a stand-in.

### KI-20: Periodic mount-offset save every 60 s burns EEPROM cycles

- **Symptom:** Every 60 s of RUN time the host saves the current offset back to EEPROM, regardless of whether it has changed.
- **Evidence:** `auto_orientation/src/main.cpp:443-448` — unconditional save once per minute of RUN.
- **Why fixing this matters:** AVR EEPROM is rated for ~100k writes. At one write/minute, 100k cycles = ~70 days of continuous balance. Not blocking but easy to gate on `|offset_now - offset_last_saved| > threshold`.

---

## Cross-cutting observations

These are not bugs — they are design tensions worth being aware of when planning fixes.

- **The `OrientationSensor` abstraction hides raw gyro from the balance loop by design.** That design choice was made when the framework's only consumer was the IMU+GPS+EKF telemetry application, which legitimately only wants the fused output. The balancing-robot application is the first consumer that needs more, and the abstraction needs to grow a virtual `getRawGyro` / `getRawAccel` to accommodate it (KI-5). This is the right design tension to resolve in the abstraction's favour, not by downcasting.
- **"PID gains MUST be dynamic, never persisted" is in productive tension with "prop-and-go must work on every boot".** The way the user reconciled these in session: the *slow* I-channel lives in the online mounting estimator (persisted as the mount offset) and the *fast* PID gains reset to defaults on boot. That works iff the defaults are appropriate for the chassis — which they are not today (KI-2). The persistence policy is sound; the default values need to land on a tuned baseline.
- **The framework treats "BNO055 NDOF" as the canonical sensor mode.** Switching to AMG (raw, no on-chip fusion) on the same physical part is the actual fix for the latency problem, but it changes what `OrientationSensor::getOrientation()` returns to all consumers. Either the balancing app needs a different sensor mode (per-application config) or the framework needs a second OrientationSensor implementation that uses Madgwick / 2-state Kalman on AMG samples.
- **The BNO055 driver allocates the Adafruit library on the heap (`new` / `delete`).** Not a bug today but worth flagging when porting to platforms with no heap or stricter memory budgets.

---

## User preferences that constrain solutions

Pulled from the session log and session-end discussion. Any fix must respect these.

- **Prop-and-go UX:** power-on, the bot balances. No per-boot setup. No "run calibration first" prompt for normal operation.
- **PID gains MUST be dynamic, never persisted.** Saving them defeats the point. Defaults are hardcoded; the online mounting estimator is the long-term adaptation channel.
- **Persisted: hardware/physical properties only.** BNO055 cal blob (22 B) and mounting offset (8 B). Nothing else.
- **Balance modes cap PWM. Driving / remote-control modes get full ±255.** Two motor authority profiles, switched at the state-machine level.
- **No motor slew limit — just lower PWM in general.** The slew bands are the wrong primitive (KI-13). Output cap + dead-band compensation are the right primitives.
- **No auto-recovery from FALLEN — must be operator-triggered.** Sticky SAFE_FALL is intentional given the current sensor situation. The HELD state (KI-11) is a different problem — when implemented, HELD→RUN can be automatic because motion sensing disambiguates "held in hand" from "stable on ground".
- **Auto-tune is on-demand only.** Not on boot, not on idle-long-press by default. The operator deliberately triggers it on a tuning stand.
- **Calibration values that are lost on reboot are unacceptable.** Anything expensive to redo must persist.
- **Tight safety:** 10° tilt limit; motors must never run "full blast for more than 3 seconds during balance".
- **Visible state transitions:** every state change logged to serial.

---

## Open questions

Things we genuinely don't know yet that need answering before fixing.

1. **Does the bot balance at all with `Kp=15-18 / Ki=0 / Kd=8-22` and ±100-120 cap?** Tier-1 of `balance_failure_diagnosis_2026-05-12.md` claims 60-80% probability on a sound chassis. We haven't run the experiment.
2. **What is the actual sensor-to-actuator latency on this Uno + BNO055 setup?** The 20-40 ms NDOF figure is from the datasheet. We have not instrumented it. A `micros()`-stamped round-trip test against a known gyro impulse would settle it.
3. **What is the right HELD-trigger threshold for `g_lateral` and `a_dev` on this specific chassis?** `balance_held_fallen_state_machine.md` proposes 30 deg/s and 3 m/s² based on first-principles calc; real bots vary by mass and grip kinetics.
4. **Is the BNO055 we have an Adafruit (32 kHz ext crystal) or a generic clone (no crystal)?** Forcing `setExtCrystalUse(true)` on a board without the crystal silently freezes fusion. We default to `true` and override per-build with `BNO055_NO_EXT_CRYSTAL`. The current build env's setting and what's actually wired need cross-checking.
5. **Is the right-motor stiction asymmetry severe enough to need per-direction stiction floors?** Session §6 noted reverse is sluggish even at PWM 50. We mitigated by testing at 90. Live balance regularly commands ±10-40 PWM. Unknown whether per-direction `stiction_min_pwm` solves it or whether the motor itself is the wrong part.
6. **At what point in development should we abandon NDOF and switch BNO055 to AMG + 2-state Kalman (KI-9)?** Tier-1 conservative gains might be enough to balance; Tier-2 (Kalman) is a 6-hour build. If Tier-1 works the Kalman becomes "nice to have", not "must have". Empirical question.
