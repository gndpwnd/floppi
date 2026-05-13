# Osoyoo Balancing Car — Reference Implementation Review
Last updated: 2026-05-12
Source: https://osoyoo.com/2018/07/18/osoyoo-balancing-car/
Local copy: `~/tmp/osoyoo/` (osoyoo_abc/osoyoo_abc.ino, libraries/BalanceCar/*, libraries/KalmanFilter/*)

## 1. The kit

The Osoyoo Two Wheel Self Balancing Car ships with a deliberately matched, "works out of the box" hardware bundle. Every piece below shows up in the .ino, the libraries, or the wiring diagram on the product page:

- **Controller:** Osoyoo UNO Board (ATmega328P clone, 16 MHz, 2 KB SRAM, 32 KB flash).
- **IMU:** MPU6050 (6-DoF — 3-axis accel + 3-axis gyro, NO magnetometer). Talks I²C at the default address.
- **Motor driver:** TB6612FNG — a far better part than the L298N. Lower R_DS(on) of the H-bridge (~0.5 Ω vs ~2 Ω on the L298N), built-in standby pin (`STBY`), and crisper PWM response. The TB6612FNG dissipates much less heat at the same current and produces noticeably less voltage drop into the motors, which means the same PWM number actually delivers more torque to the wheels.
- **Motors:** "GM37" 6 V geared DC motors with integrated **quadrature encoders** (the `PinA_left` / `PinA_right` pins and `count_left` / `count_right` interrupt counters in the sketch). This is the single most important piece of hardware on the kit that the developer does NOT have — wheel-speed feedback closes a second control loop that the developer's hardware cannot.
- **Wheels:** ~65 mm rubber wheels on aluminium hubs — moderate diameter, real rubber, good traction.
- **Power:** 2× 18650 cells in series (≈8.4 V fresh, ≈7.4 V nominal) feeding an LM2596S buck regulator to 5 V for the logic. Motors driven from the raw pack voltage. The pack voltage is high enough (>5 × motor stall) that the controller never starves the motors of supply rail.
- **Bluetooth:** HC-06 module on hardware Serial — the Android app sends `$1#`, `$2#`, etc. for drive commands and PID-edit packets.
- **Chassis:** three precision-cut aluminium decks separated by standoffs. The IMU is screwed flat to the middle deck — meaningful for our discussion below.

The kit has no ultrasonic sensor by default (`chaoshengbo`, `tingzhi`, `jishi` are reserved but unused).

## 2. The code overview

Three files compile into one binary:

- **`osoyoo_abc.ino`** (471 lines) — main sketch, ISR setup, Bluetooth state machine, encoder pulse counter.
- **`libraries/BalanceCar/BalanceCar.{h,cpp}`** (44 + 144 lines) — speed PI loop, turn PD, motor mixer, low-PWM dead-band.
- **`libraries/KalmanFilter/KalmanFilter.{h,cpp}`** (27 + 62 lines) — 2-state Kalman filter for pitch (angle + gyro-bias) plus a first-order complementary filter as a backup `angle6`.

**Architecture:** there is **no state machine**. There is a single 5 ms ISR that does everything:

```c
MsTimer2::set(5, inter);   // osoyoo_abc.ino:251
MsTimer2::start();
```

Inside `inter()` (osoyoo_abc.ino:144-166):

1. Re-enable interrupts (`sei()`) so encoder pulse interrupts can fire.
2. `countpluse()` — read & reset encoder pulse counters, apply motion-direction sign.
3. `mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz)` — single I²C burst read.
4. `kalmanfilter.Angletest(...)` — runs the Kalman filter on accel+gyro.
5. `angleout()` — PD on pitch angle.
6. Every 10th tick (≈50 ms): `speedpiout(...)` — PI on wheel speed.
7. Every 4th tick (20 ms): `turnspin(...)` — turn PD.
8. `pwma(...)` — mix angle/speed/turn outputs, clamp to ±255, and write motor pins.

`loop()` runs at indeterminate speed and only handles Bluetooth parsing and high-level mode flags (`front`, `back`, `turnl`, `turnr`). The ISR is the entire control system.

**Control rate:** 200 Hz pitch PD (5 ms), 20 Hz speed PI (50 ms), 50 Hz turn PD (20 ms). These are derived from a single hardware timer 2 ISR — never drift, never miss a tick. **Critical**: the Kalman filter `dt` is hard-coded to `timeChange * 0.001 = 0.005` seconds and matches the ISR period exactly (osoyoo_abc.ino:49-50). No `millis()` shenanigans.

**Library dependencies:** `MsTimer2`, `PinChangeInt`, `Wire`, `I2Cdev`, `MPU6050_6Axis_MotionApps20`. They are vendored in the libraries.zip the user installs.

## 3. Control algorithm

The Osoyoo bot runs **three cascaded loops** — not one PID like the developer's setup. From osoyoo_abc.ino:32-34:

```c
double kp = 40,    ki = 0.0,  kd = 0.6;       // angle PD
double kp_speed =5.20, ki_speed = 0.25, kd_speed = 0.0;  // speed PI
double kp_turn = 23,   ki_turn = 0,    kd_turn = 0.3;     // turn PD
```

### 3a. Angle loop — PD, NOT PID

```c
balancecar.angleoutput = kp * (kalmanfilter.angle + angle0)
                       + kd * kalmanfilter.Gyro_x;          // BalanceCar.cpp via .ino:141
```

That single line is the entire angle controller. Three things to notice:

1. **No integral term.** `ki = 0.0` is hard-coded and never used in `angleout()`. Their PID is a **PD** controller.
2. **D term is gyro, not numerical derivative.** `kalmanfilter.Gyro_x` is the bias-corrected gyro pitch rate directly from the Kalman filter (the filter exposes `q_bias` and uses gyro−bias). They get a clean derivative for free, no numerical differentiation, no LPF, no phase lag.
3. **Setpoint correction is via `angle0`**, the "mechanical balance angle". Default `0.0`, but the user is told in the tutorial to nudge it if the bot drifts forward/backward at rest. It is an open-loop offset, not learned online.

Units: `angle` is in degrees (from `atan2(ay, az) * 180 / PI`, KalmanFilter.cpp:51), `Gyro_x` is in deg/s. So Kp ≈ 40 PWM/°, Kd ≈ 0.6 PWM·s/°.

### 3b. Speed loop — PI on encoder pulses (BalanceCar.cpp:8-26)

```c
double speedpiout(...) {
  speeds = pulseleft + pulseright;
  pulseright = pulseleft = 0;
  speeds_filterold *= 0.7;
  speeds_filter = speeds_filterold + speeds * 0.3;   // first-order LPF, α=0.3
  speeds_filterold = speeds_filter;
  positions += speeds_filter;
  positions += f;          // forward command bias
  positions += b;          // backward command bias
  positions = constrain(positions, -3550, 3550);
  output = kis * (p0 - positions) + kps * (p0 - speeds_filter);
  return output;
}
```

This is a position+velocity controller that **acts on the angle setpoint indirectly** — its output is summed into the motor command, but its physical effect is to nudge the bot to lean a tiny bit forward or backward to make the wheels stop drifting. It's the secret to "Osoyoo doesn't slide off the table while balancing."

### 3c. Anti-windup

Position-style: `positions = constrain(positions, -3550, 3550)` (BalanceCar.cpp:18). They clamp the **integrator state itself**, not the output of the I term. Same family as the developer's `set_i_term_limit(40)` but bounded by motor saturation calculation rather than a manually-picked PWM cap.

### 3d. Filter / smoothing

- 2-state Kalman filter on pitch (`angle`, `q_bias`). `Q_angle = 0.001`, `Q_gyro = 0.005`, `R_angle = 0.5` — trust the gyro a lot, trust the accel a little.
- First-order complementary on encoder speed (α = 0.3), BalanceCar.cpp:13.
- **NO derivative LPF** — they don't need one because the D term is `Gyro_x`, already filtered by the Kalman.

### 3e. Sample rates (recap)

- Pitch PD: **200 Hz** (5 ms ISR).
- Speed PI: **20 Hz** (every 10th ISR tick).
- Turn PD: **50 Hz** (every 4th tick).
- Kalman update: 200 Hz, locked to the ISR.

## 4. Sensor handling

### 4a. IMU

MPU6050 (6-DoF, no mag) at default I²C address. Initialized with the stock `mpu.initialize()` from the I2Cdev / MPU6050_6Axis_MotionApps20 library. They do NOT use the on-chip DMP quaternion output — they pull raw `getMotion6(&ax,&ay,&az,&gx,&gy,&gz)` and fuse in software with the Kalman filter.

### 4b. Fusion approach — explicit 2-state Kalman

KalmanFilter.cpp:17-43 is a textbook Kalman with state `[angle, gyro_bias]` and measurement `angle_m` (the accel-derived angle):

```
angle += (gyro_m - q_bias) * dt        // predict using gyro
angle_err = angle_m - angle             // innovation
... covariance update ...
angle += K_0 * angle_err                // correct using accel
q_bias += K_1 * angle_err
angle_dot = gyro_m - q_bias             // bias-corrected gyro = clean derivative
```

The genius is in the last line. `angle_dot` (called `Gyro_x` after axis rename) feeds directly into the angle PD. **The derivative term gets a noise-free, bias-corrected gyro rate at 200 Hz with no numerical differentiation.** No `(error[n] - error[n-1]) / dt`, no LPF, no `set_d_term_lpf_tau_sec(...)`. This is fundamentally different from the developer's PIDController which numerically differentiates the BNO055 fused pitch.

### 4c. Conversion to pitch (KalmanFilter.cpp:51-52)

```c
Angle  = atan2(ay, az) * 180 / PI;     // pitch from accelerometer
Gyro_x = (gx - 128.1) / 131;           // gyro to deg/s
```

The `128.1` is a hard-coded gyro bias (raw LSB). The `131` is the MPU6050 ±250 dps sensitivity factor (131 LSB/dps). They divide instead of multiply to convert raw int16 to deg/s.

### 4d. Sample rate

200 Hz fusion update, no resampling, no jitter — driven by the timer 2 ISR.

## 5. Motor handling

### 5a. Driver

TB6612FNG (better than L298N). STBY pin held HIGH to enable the bridge. Direction set by `IN1M / IN2M` per channel; PWM duty on `PWMA / PWMB`. Configurable from osoyoo_abc.ino:15-21.

### 5b. Stiction / dead-band

**They have one, but it's mixer-style, not driver-style.** BalanceCar.cpp:88-94 just `constrain(pwm, -255, 255)`. No minimum-PWM kick. They get away with this because:

1. The Kalman filter delivers clean state at 200 Hz.
2. The TB6612FNG doesn't have the L298N's 1.4 V output saturation, so 30 PWM actually moves the wheels.
3. The GM37 motors with their built-in 1:30 gearbox have far less stiction than cheap brushed motors.
4. The speed PI loop integrates pulse error: when the wheels DON'T move at low PWM the encoders report zero, `positions` accumulates, and the controller ramps `Outputs` up until they do move. Stiction is **automatically compensated** by the second loop.

### 5c. Direction asymmetry

BalanceCar.cpp:88-89:

```c
pwm1 = -angleoutput - speedoutput - rotationoutput;   // Left motor
pwm2 = -angleoutput - speedoutput + rotationoutput;   // Right motor
```

Both motors get the same magnitude with the same sign, BUT — and this is important — the constructor `digitalWrite(IN1M,0); digitalWrite(IN2M,1); digitalWrite(IN3M,1); digitalWrite(IN4M,0);` (osoyoo_abc.ino:231-234) sets the two channels **opposite by default** so a positive pwm value rolls the bot forward. Direction asymmetry handled at the H-bridge wiring, not in software.

### 5d. PWM range

Full ±255 (8-bit analogWrite). No artificial cap. Clamping at ±255 happens in `pwma()` (BalanceCar.cpp:91-94).

### 5e. PID-to-PWM mapping

Direct sum, then clamp:

```c
pwm1 = -angleoutput - speedoutput - rotationoutput
```

The minus sign in front of `angleoutput` is the sign flip that converts "lean angle in degrees" into "PWM that drives the wheels to catch the lean." If you flip the IMU mounting orientation you have to also flip this sign or the loop becomes unstable (positive feedback).

### 5f. The "angle too big — stop" cutoff (BalanceCar.cpp:96-100)

```c
if (angle > 30 || angle < -30) {
  pwm1 = 0;
  pwm2 = 0;
}
```

Above ±30° they cut PWM to zero. Same idea as the developer's `is_tipover()` / FALLEN state but **without sticky-state behaviour**. The moment angle goes back below 30° the motors come on again. This is materially different from auto_orientation's sticky FALLEN.

## 6. Calibration / mounting

Honest answer: **there is no calibration step** in the Osoyoo code. None.

- The accel-to-angle bias `128.1` is a magic constant (KalmanFilter.cpp:52), hand-tuned for the typical batch of MPU6050 modules. They don't measure it per unit.
- The mechanical mounting offset `angle0` (osoyoo_abc.ino:44) defaults to `0.0` and is only adjusted by the user editing the source.
- The speed-loop setpoint `setp0` is hardcoded to `0` (osoyoo_abc.ino:37).
- The MPU6050 does not need magnetometer calibration (it doesn't have one).
- There is no EEPROM persistence of anything. Power cycle, defaults reload.

The user's "calibration" workflow per the tutorial is: prop the bot up, observe drift, adjust `angle0` by ±0.5° in source, recompile, repeat. The Kalman filter handles gyro-bias drift online (`q_bias` state), which is the only "calibration" that happens at runtime.

This is a very different philosophy from auto_orientation: Osoyoo treats mounting offset as a static compile-time constant the user accepts; auto_orientation treats it as a dynamic property worth learning online with `OnlineMountingEstimator` and persisting to EEPROM.

## 7. Safety / edge cases

- **Fall detection:** soft (`if (angle > 30) pwm = 0`). NOT sticky — re-engages immediately when angle drops back into the linear region. (BalanceCar.cpp:96-100.)
- **Pickup detection:** none. If you pick up the bot, the motors spin freely. Encoders still count, `positions` accumulates, but with the bot mid-air the loop is harmless.
- **Battery monitoring:** none. The bot just balances worse as voltage sags.
- **Tip-over behaviour:** angle cutoff kills PWM. As soon as the bot is righted (manually or by impact), motors re-engage. There is a second nested cutoff (BalanceCar.cpp:102-110) for "the bot has been at >10° AND `stopl+stopr` accumulated past 1500 pulses" → cut motors and set `flag1`. This is their "the bot has been knocked over and the encoders are spinning uselessly" guard, sticky until the bot is upright.
- **Watchdog:** none.
- **Saturation timeout:** none. The encoders eventually unwind it via the speed PI loop.

## 8. Side-by-side comparison

| Subsystem | Osoyoo | auto_orientation | Why the difference matters |
|---|---|---|---|
| **IMU** | MPU6050 (6-DoF), software Kalman | BNO055 (9-DoF), on-chip NDOF fusion | BNO055 is much more capable but its 100 Hz NDOF output has ~10-20 ms latency through the fusion engine. The raw-Kalman approach has none. |
| **Loop driver** | Timer 2 ISR @ 200 Hz, deterministic | `millis()`-polled `if (now - last_step >= 5)` in `loop()` (main.cpp:439-443) | Loop-polled timing in a busy `Serial.print()` / button-polling loop drifts. Tick jitter wrecks the derivative term. |
| **Pitch controller** | **PD only** (`ki = 0.0`), D term = gyro | **PID** Kp=50 Ki=1 Kd=20, D term = numerical derivative of fused pitch w/ 3 ms LPF | Osoyoo has zero phase-lag in D. auto_orientation has numerical-derivative noise + LPF phase lag + integrator wind-up. |
| **D-term source** | Bias-corrected gyro (KalmanFilter.cpp:42, `angle_dot`) | `measurement_lpf_` derivative (pid_controller.cpp:185-186) | Gyro is a physical measurement; numerical derivative of a fused angle is a numerical artefact. |
| **Anti-windup** | Position clamp (`constrain(positions, -3550, 3550)`) | I-term limit 40 PWM, plus integral clamp | Both work. The developer's is fine. |
| **Speed loop** | 20 Hz PI on encoder pulses, drives bot to position | **NONE** | Without encoders the bot will *always* drift in one direction even when "balanced" because mechanical imperfections aren't observable to the controller. |
| **Mounting calibration** | `angle0` hand-set in source code | OnlineMountingEstimator + EEPROM persist + CAPTURE state machine | auto_orientation has the better answer if it works — but it adds complexity. With Osoyoo the user edits one float. |
| **Motor driver** | TB6612FNG | L298N | TB6612FNG has ~75% lower bridge drop. Same PWM = more torque, less heat. The L298N's voltage drop also makes the dead-band wider. |
| **Motors** | GM37 with **integrated encoders** | "cheap brushed DC motors", no encoders | Encoders enable the speed loop. Without them, the developer has only the angle loop, which is open-loop in position. |
| **Stiction handling** | None (speed loop integrates through it) | `stiction_min_pwm` (currently 0), comments call out "kick-up creates chunky 18 PWM step" | Right call by the developer to zero it for now — without encoders, kick-up just causes oscillation. |
| **Tip-over behaviour** | Soft cutoff at ±30°, instant re-engage | Sticky FALLEN state, requires button-press to recover | Osoyoo "just keeps working" when you stand it back up. auto_orientation requires manual reset. |
| **HELD detection** | None | Lateral-gyro + accel-deviation, 150 ms entry / 200 ms exit (balance_app.cpp:362-372) | A "feature" that introduces new failure modes. Osoyoo doesn't need it. |
| **State machine** | One ISR, no states | IDLE / CAPTURE / TUNE / RUN / HELD / FALLEN | Adds branches, mode bugs, and "stand it up and it does nothing" failures. |
| **PWM clamp** | ±255 | ±255 (was ±80 earlier in branch; comments show recent un-clamping in main.cpp:566) | Both are OK once the PID gains are right. With wrong gains a small cap saves the motors. |
| **EEPROM** | None | Mount offset + BNO055 cal blob | Useful long-term, but not why the bot does or doesn't balance. |
| **Setpoint / mech offset** | Static `angle0 = 0` (compile-time) | `corrected_pitch_() = pitch_deg_ - online_est_.get_estimate_deg()` | The developer's runtime-learned offset is more elegant but introduces a moving target the PD has to chase. |
| **Update determinism** | Hardware timer | Software polling + serial logging mid-loop | A `Serial.println` in the hot path can delay the next PID step by 5-30 ms, silently corrupting `dt`. |

## 9. What Osoyoo does that we don't

1. **Hardware timer ISR for the control loop.** Osoyoo's PID never runs late. `MsTimer2::set(5, inter)` (osoyoo_abc.ino:251-252) guarantees a 5 ms cadence regardless of what the main loop is doing. auto_orientation's `if (now - last_step >= 5) { app.step(now); last_step = now; }` (main.cpp:439-443) runs *at least* every 5 ms, but can run later if `Serial.println` or `poll_button_` takes longer. With logging enabled, real intervals of 8-15 ms are entirely plausible.

2. **D-term from a physical gyro measurement, not numerical differentiation.** `kalmanfilter.Gyro_x` (the bias-corrected gyro rate) feeds directly into the angle controller (`angleout()`, .ino:141). The developer's `pid_.compute()` takes `measurement` (filtered pitch) and differences it numerically (pid_controller.cpp:185-186). With BNO055 NDOF latency + numerical differentiation + 3 ms LPF, the developer's D term has 10-20 ms of effective phase lag that Osoyoo simply doesn't have. This is almost certainly the asymmetry the developer is seeing — phase lag on D makes the controller late-correcting in one direction and over-correcting in the other depending on the lean dynamics.

3. **Wheel encoders + a 20 Hz speed PI loop.** This is the single piece of hardware that does the most work. The angle PD alone cannot tell whether the bot is drifting forward at a constant lean angle — only the encoders see that. The Osoyoo speed loop accumulates pulse error and nudges the angle setpoint so the wheels stop moving. Without it the bot **always drifts** unless `angle0` is set exactly right for that exact build, exact battery level, exact floor surface.

4. **PD instead of PID for the angle loop.** `ki = 0.0` (osoyoo_abc.ino:32). They rely on the **outer** loop (speed PI) for steady-state correction. The developer's setup runs Ki=1 with a 40-PWM hard limit in the angle loop itself — which is a sound choice in the absence of a speed loop, but it puts the burden of mechanical-offset correction on an inherently under-damped pole.

5. **Soft, non-sticky tip-over cutoff.** `if (angle > 30) pwm = 0` (BalanceCar.cpp:96). The instant the bot is righted, motors re-engage. No state machine, no FALLEN, no button press. The developer's RUN→FALLEN is sticky.

6. **Hardcoded gyro bias offset (`-128.1` LSB)** in KalmanFilter.cpp:52. Cheap but it works because the Kalman estimates `q_bias` online to eat any residual. The developer's BNO055 NDOF mode hides the raw gyro behind the fusion engine — you can't tap into a raw bias-corrected rate.

7. **First-order complementary filter on encoder speed** (BalanceCar.cpp:13, α=0.3). Used to feed the speed loop. Tiny, simple, no biquad, no tau-tuning, no init flag.

8. **Mode-of-operation flags are integers (`front`, `back`, `turnl`)**, set directly in the BT-receive switch and read directly in the ISR. No state machine ceremony.

9. **Direction reversal is handled at the H-bridge pin level** (`digitalWrite(IN1M, 0); digitalWrite(IN2M, 1); ...` osoyoo_abc.ino:231-234), not by swapping software direction conventions. main.cpp:83-86 in auto_orientation does the swap by re-numbering IN1/IN2 vs IN3/IN4 in the L298NPins struct — fine, but harder to discover.

## 10. What we do that Osoyoo doesn't

| Feature | Need / cost |
|---|---|
| `OnlineMountingEstimator` 20 s LPF | The right idea, but it tries to learn the offset while the bot is unstable. Osoyoo bypasses with `angle0` set once in code. |
| EEPROM persistence (mount + BNO cal) | Genuinely useful long-term. Not load-bearing for "does it balance today?" |
| `CAPTURE_MOUNTING` Welford variance state | Sound — but adds another mode the operator has to remember to enter. |
| `AUTO_TUNE` relay-feedback | Excellent in theory; never run by the developer in the current logs. |
| `HELD` lift-detect state | Re-enabled to fix overreact-on-pickup, but the entry/exit thresholds (30 dps lateral / 12 dps quiet, 150/200 ms) are tuned blind. |
| Sticky FALLEN | Causes the "motors keep going after fall" symptom if entry condition fires intermittently. |
| Saturation timeout (3 s at 180+ PWM) | Defensive — good, low-risk. |
| Per-state logging (`Serial.println(F("[state] ->"))`) | **Costs ~1-3 ms per transition.** During fast HELD→RUN→HELD oscillations these printlns delay the very next PID tick. |
| Slew limiter (currently disabled) | Operator preference; correct call. |

Verdict: the developer's framework features are not the problem **except** when they introduce mid-loop latency (logging) or hide a fundamental issue (no encoders → drift → mounting estimator fighting the absence of a speed loop).

## 11. The "why does Osoyoo work flawlessly" answer

It is **not the gains.** It is **not the Kalman filter.** It is **not even the chassis.** It is the combination of:

1. **A 6-DoF IMU with raw gyro you can use directly as the D term**, giving zero-phase-lag derivative damping.
2. **A hardware-timer ISR control loop** that runs at exactly 200 Hz with deterministic `dt`.
3. **Wheel encoders feeding a speed loop** that automatically corrects any mounting-offset drift without the controller needing to know about it.
4. **A clean motor driver** (TB6612FNG) that doesn't impose a 1+ V drop and a 15-PWM dead-band.

The Osoyoo *control law* is plain — PD on angle, PI on speed, mixed and clamped. The Osoyoo *gains* are not magical — 40 / 0 / 0.6 for the angle loop is honestly modest. What makes it "work flawlessly" is that **every measurement the controller acts on is fresh, the loop tick is exact, and there is a second loop that catches the long-term drift the first loop cannot.**

The developer's controller has none of these structural properties:

- D term is a numerically-differentiated, LPF-delayed fused angle.
- Loop tick is polled with a `Serial.println` in the path.
- No encoders → the angle loop is fighting steady-state drift open-loop, which is why Ki=1 and the mounting estimator both exist.
- L298N adds 1-2 V drop and a wider dead-band, so the same PID number produces wildly different torque depending on direction (asymmetry between forward and backward).

That last point — L298N voltage drop being direction-asymmetric due to body-diode conduction differences — is probably the proximate cause of the reported "somewhat balances backward, falls forward" asymmetry.

## 12. Concrete recommendations for the developer

Ranked by impact on the reported symptoms.

### Tier 1 — fix the structural problems

1. **Replace numerical D-term with raw gyro pitch rate.** The BNO055 in NDOF mode does NOT expose a clean bias-corrected gyro, but `BNO055::getRawGyro()` (already called in `balance_app.cpp:654`) does return the raw rate. Bias-correct it once on boot (average for 2 s while motionless), then use `gyro_pitch_rad_per_s` directly as the D-term feed instead of running `pid.compute()`'s internal numerical differentiation. Files: pid_controller.cpp:175-192 (add a `compute_with_explicit_derivative(measurement, gyro_rate, dt_ms)` entry point), balance_app.cpp:378-379 (call the new entry point).

2. **Move the control loop into a hardware timer ISR.** Use TimerOne (Uno) or IntervalTimer (Teensy) at exactly 5 ms. Currently main.cpp:439-443 polls `millis()` in a loop that also does `Serial.println` and `poll_button_`. With a USB CDC printline costing 0.5-2 ms, tick jitter can exceed 30% — invisible in logs, fatal to a PD controller. The simplest first cut: gate `app.step(now)` so it's the only thing in the timer ISR; do everything else in `loop()`.

3. **Strip the `Serial.println` from every state transition.** balance_app.cpp:579-587 prints a state name every transition. During HELD↔RUN flapping that's hundreds of ms of stalled loop. Replace with a single byte enqueued to a ring buffer and drained from `loop()`, OR remove during tuning.

### Tier 2 — match the architecture

4. **Make tipover non-sticky.** The Osoyoo `if (angle > 30) pwm = 0` re-engages instantly. balance_app.cpp:467-485 keeps `FALLEN` sticky until a button press. Symptomatic match: "after falling over, motors keep going even though bot is stationary" — that's the saturation-timeout path (balance_app.cpp:395-409), not FALLEN. Recommendation: in the RUN loop, if `abs(pitch) > tilt_limit_deg`, write `motors.stop()` and `last_output_ = 0`, but **stay in RUN**. Let the bot self-recover when the operator stands it up.

5. **Set `angle0` instead of relying on the OnlineMountingEstimator during bring-up.** Add a build flag `USE_ONLINE_MOUNTING_ESTIMATOR` that defaults OFF for new builds. Replace `corrected_pitch_()` (balance_app.cpp:693-695) with `pitch_deg_ - kStaticMountOffsetDeg` where `kStaticMountOffsetDeg` is a hand-set constant initially. Once the bot balances reliably with the static offset, re-enable the estimator. (You'll have a baseline to know whether the estimator is helping or hurting.)

6. **Try the Osoyoo-style gains.** With raw-gyro D-term in place, set `Kp = 40`, `Ki = 0`, `Kd = 0.6` (osoyoo_abc.ino:32). The developer's current Kd=20 is acting on numerical derivative which is unitfully equivalent to ~5× smaller on raw gyro rate. If the developer truly switches to gyro-rate D, Kd needs to drop by an order of magnitude.

### Tier 3 — close the loop

7. **Add encoders to the motors, even cheap optical ones.** A pair of US$3 photo-interrupters on the wheel spokes gives ~20 pulses per revolution. Wire to D2/D3 (Uno) for interrupts. Implement the Osoyoo `speedpiout` verbatim (BalanceCar.cpp:8-26): accumulate pulses, LPF (α=0.3), feed `Kp=5.2`, `Ki=0.25`. This is the change that will eliminate the "drifts forward" failure mode permanently.

8. **Disable HELD for now.** Cleaner falsification of the underlying control loop. balance_app.cpp:362-372 — comment out the HELD entry test. Re-enable only after the bot balances without it.

### Tier 4 — replace hardware if affordable

9. **Swap the L298N for a TB6612FNG breakout** (~$3). The asymmetric forward/backward behaviour the developer reports is almost certainly the L298N's body-diode conduction asymmetry under PWM, plus its 1.5 V voltage drop variance with current draw.

10. **Better motors.** Geared brushed DC motors with metal gears (Pololu HP series, GM37 from any vendor, N20 with 50:1 metal gearbox) have far less stiction and back-drive resistance than the no-name yellow motors typical on cheap Uno kits. The "small motor movements over-correct" symptom is partly mechanical: a tiny PWM moves the wheel a fraction of a degree then the gear teeth slip into the next backlash position.

## 13. Cautions

Things in the Osoyoo code that work for *that exact kit* but should not be blindly copied:

- **Magic gyro bias `128.1`** (KalmanFilter.cpp:52). That's the average bias of the particular batch of MPU6050s Osoyoo bought. Different chips will have different biases. The Kalman's `q_bias` estimator masks it, but the developer's BNO055 NDOF already does bias estimation internally, so this constant has no equivalent.

- **No safety bounds on the speed-loop integrator** beyond `±3550`. If you flip the wires or the bot is forced to spin its wheels (carried by hand), `positions` rails out and takes seconds to unwind. Acceptable on Osoyoo because the bot is small and harmless; not acceptable on a heavier robot.

- **`String` class for serial parsing** (osoyoo_abc.ino:82-83 and throughout). The Arduino `String` class fragments the heap on a 2 KB ATmega328. On a long run with many BT packets it can run out of RAM. Don't copy this.

- **The 30° angle cutoff is a hard threshold with no hysteresis** (BalanceCar.cpp:96). Right at the boundary it flaps motors on/off. Osoyoo gets away with it because the bot is rarely *near* 30° for long. A real product would add ±2° hysteresis.

- **No watchdog timer.** If the ISR hangs (rare on bare metal but possible), nothing rescues the system. auto_orientation's `safety_.feed_watchdog()` is the right call; do not give it up.

- **`MsTimer2` shares timer 2 with PWM pins 3 and 11**, so analogWrite on those pins becomes broken. They route motor PWM to pins 9 and 10 (timer 1) to avoid the conflict (osoyoo_abc.ino:19-20). On other Arduinos (Mega, Teensy) the timer/PWM mapping is different — pick the timer carefully.

- **Bluetooth packet validation is by index** (`inputString[3] == '1'`, .ino:294). One missed byte and the whole switch goes wrong. Brittle. Not what we want in our framework.

## 14. Links / references

- Tutorial page: https://osoyoo.com/2018/07/18/osoyoo-balancing-car/
- Source zip (Arduino sketch): https://github.com/osoyoo/Osoyoo-development-kits/blob/master/OSOYOO%202WD%20Balance%20Car%20Robot/osoyoo_abc.zip
- Required libraries zip: https://osoyoo.com/wp-content/uploads/2018/03/libraries.zip
- Android control app: https://github.com/osoyoo/Osoyoo-development-kits/blob/master/OSOYOO%202WD%20Balance%20Car%20Robot/Balance%20Car_v1.6.apk
- TB6612FNG datasheet: https://www.sparkfun.com/datasheets/Robotics/TB6612FNG.pdf (motor-driver part the developer should consider)
- GM37-520 motor with encoder typical spec: ~6 V, ~110 RPM, 11 pulses/rev raw × 30 gear ratio = 330 pulses/rev
- For a reference comparison of the same control law on similar hardware see also: https://github.com/kuabhish/Self-Balancing-Robot (Arduino + MPU6050 + L298N + encoders, very close in spirit to the Osoyoo)
- MPU6050 ±250 dps sensitivity factor `131 LSB/dps` referenced in datasheet section 6.1.
