# Balance Failure Diagnosis — Uno + BNO055 + L298N

**Date**: 2026-05-12 · **Status**: Diagnostic, not yet validated on hardware

**Inputs**: [balance_point_and_mounting_research.md](balance_point_and_mounting_research.md), [auto_pid_tuning_research.md](auto_pid_tuning_research.md), [online_adaptive_balance_tracking.md](online_adaptive_balance_tracking.md), [disturbance_compensation_research.md](disturbance_compensation_research.md), [MASTER_DESIGN.md](MASTER_DESIGN.md), [DISSECTION_NOTES.md](../archive/balancing_robot_reference/DISSECTION_NOTES.md), [2026-05-12_uno_balancing_hardware.md](../archive/session_records/2026-05-12_uno_balancing_hardware.md), plus `pid_controller.cpp`, `balance_app.cpp`, `bno055.cpp`, `main.cpp`.

---

## 1. Root cause — why the motors slam

Three failures are stacked. The controller is fighting yesterday's news with gains tuned for a different bot.

**1a. Gains too hot for an unknown plant.** `Kp=65 / Ki=12 / Kd=38` came from a *Mega* bot with a *different* chassis, motors, battery, CoM and mounting (DISSECTION_NOTES.md "Key constants"). The .ino comments themselves show prior iterations of `Kp 55/45/150`, `Kd 32/28/2.5` — the source gains were never canonical. Concretely, at `pitch = 2°` the P-term alone is `65×2 = 130`, already past the ±100 cap. The controller has essentially **zero linear region** before saturating; above 2° error the loop is open. That is the slamming.

**1b. BNO055 NDOF latency vs. balance bandwidth.** NDOF runs the on-chip fusion at 100 Hz with **~20–40 ms group delay** (Bosch BST-BNO055-DS000 §3.6.5 + §3.4.1 — internal MA filter plus MEMS fusion). We poll at 200 Hz but only get fresh samples every 10 ms, reflecting motion ~20–40 ms old. balance_point_and_mounting_research.md §3 is blunt: *"For balancing use the standard 2-state Kalman... ~150 µs per tick on AVR."* We are doing exactly what the research told us not to: relying on the vendor's fused stream for the inner loop. A 30 ms phase lag on a ~600 ms natural-period pendulum is **~18° of phase**, halving the achievable Kd before noise overtakes signal.

**1c. Kd on a quantized, low-rate signal is noise amplification.** `pid_controller.cpp:143` differentiates `measurement`, which is BNO055-fused Euler with ~0.05° quantization. With `dt=0.005`, one quant step → `Kd × 0.05 / 0.005 = Kd × 10` ≈ **380 PWM per quantum** at Kd=38. The slew limiter masks the chatter near zero; it does nothing during a real lean, where this noise *adds* to the genuine rate and roughly doubles apparent dθ/dt. There is no measurement LP in `pid_controller.cpp` — Brokking YABR (cited in our research) explicitly LPs the rate before differentiating; we lost that when we adopted on-chip fusion.

**1d. The online estimator cannot bootstrap.** `balance_app.cpp:374–381` updates the estimator from `pid_.get_i_term()` but currently passes `windup_active=false, gyro_pitch_dps=0.0f` (the skeleton's own TODO). online_adaptive_balance_tracking.md §5 is explicit that the estimator MUST be **frozen** during `|I| > 0.7·I_max`, `|θ̇| > 1°/s` or saturated motors, warning: *"the bot has tipped, the I-term is railed recovering, and the LPF would ingest the recovery transient as a new steady state."* We are ingesting every fall. The "balance point" the estimator converges to is the average of all the bot's tipovers — which then offsets `corrected_pitch_()` and biases the PID against a moving target.

---

## 2. Latency budget

| Stage | Latency | Source |
| --- | --- | --- |
| BNO055 NDOF fusion (on-chip) | **20–40 ms** | DS §3.6.5, dominant term |
| I2C `getQuat()` @ 100 kHz | ~5 ms | `bno055.cpp:88` runs Wire at default 100 kHz; 400 kHz is a 1-line fix |
| Euler conversion + PID compute | <0.5 ms | trivial |
| `analogWrite` | <0.05 ms | timer register |
| **Sensor → actuator** | **~25–45 ms** | |

Target for a small bot (natural period ~500–700 ms): **< 10 ms.** Lauszus/Balanduino with raw MPU6050 register reads achieves ~5 ms.

**We are 3–5× over budget.** By the time we *see* a 5° lean the bot is already at ~7°; by the time motors respond it is past the 10° SAFE_FALL. The slamming is the controller correctly responding to an emergency the sensor reported too late.

---

## 3. Immediate fixes (no new modules)

**3a. Gains.** Cut by ~4×, kill the integral:

```text
Kp = 15    Ki = 0    Kd = 8
```

At |pitch|=5°, `P = 75` — inside the ±100 RUN cap with room for D. Kd cut hardest because the NDOF quantization noise scales with Kd (§1c). These are starting points, not targets; the relay tuner is supposed to find the right values — we are buying enough margin to stay in the linear region long enough for it to run.

**3b. Disable Ki for now.** auto_pid_tuning_research.md §2.1 derives `Ki = Kp / (0.85·T_u)` from the *tuned* period. Until we have a real `T_u`, any Ki is a guess and a wrong one winds the integrator on every fall. The online mounting estimator is the long-term I-channel; the fast I is redundant until the bot is provably stable. When Ki goes back: start `Ki ≈ Kp/2 ≈ 7` and back off.

**3c. Derivative-on-measurement — keep it.** `pid_controller.cpp:34, 141–144` is already correct (`d_on_measurement_ = true`). D-on-error would inject spikes when the online estimator moves the zero — exactly wrong during recovery. **But add a one-pole IIR LP (τ ≈ 15 ms, ~10 Hz cutoff) on `measurement` inside `compute()` before the D term consumes it.** Four lines, two floats of state. Cuts the Kd-amplified quantization without affecting genuine recovery (bot's natural frequency is ~1.5 Hz).

**3d. Slew bands.** Current code (`balance_app.cpp:335–337`): `<3°: 6`, `<8°: 20`, `else: 50`. Slew=6 in 5 ms = 1200 PWM/s — **slower than the plant's own dynamics** through the linear region. Recommended:

```text
|pitch| < 1° : slew =  8   (deadband / chatter)
|pitch| < 4° : slew = 30   (linear)
|pitch| < 8° : slew = 80   (catch)
else         : slew = 200  (SAFE_FALL kicks in at 10° anyway)
```

**3e. Output cap.** Code actually has ±100 (`balance_app.cpp:481`) despite the session log saying ±150. **Keep ±100** with Kp=15 — that's ~6.7° before P alone saturates. Raise to ±150 only once Ki is back and the bot reliably balances.

---

## 4. What the framework is missing

In priority order, with the research citation:

1. **Raw gyro/accel access on `OrientationSensor`.** `bno055.cpp` exposes `getRawAccel()` but no raw gyro; the abstract base only promises fused `OrientationData`. `balance_app.cpp:11–17` is a Phase-4.6.5 placeholder admitting this. *balance_point_and_mounting_research.md §3:* **"Minimum useful state... pitch rate `theta_dot` — gyro Y minus bias."** Without raw access, no path to a 2-state Kalman.

2. **2-state Kalman pitch filter.** *MASTER_DESIGN.md D6:* **"2-state Kalman (pitch + gyro-bias). NOT the 16-state EKF — too heavy at 200 Hz."** Planned in MASTER_DESIGN §4.7 last bullet (`src/navigation/balance_kalman.{h,cpp}`) but does not exist yet. The inner loop currently runs on raw BNO055 fused Euler — vendor-fusion latency *plus* no bias tracking.

3. **Anti-windup verification.** `pid_controller.cpp:193–204` claims back-calculation but is actually plain clamping. Two real bugs: (a) `set_output_limits()` shrinking the range mid-run (the `set_output_limits(-100, 100)` at `balance_app.cpp:481`) silently truncates legitimate accumulated I; (b) changing `Ki` rescales the effective limit and the clamp can briefly fight itself. Half-day to implement actual back-calculation (`u_clamped - u_unclamped` term).

4. **Push-recovery + linear-accel feedforward.** *disturbance_compensation_research.md §10:* **"Phase 4 v1: ship the push-recovery state machine (§3) and linear-acceleration feedforward (§5). Both are zero-hardware cost."** Neither exists. Without §3, every disturbance winds the I during the event and unwinds during recovery — the path that destabilizes the online estimator.

5. **Wheel-velocity proxy outer loop.** *balance_point_and_mounting_research.md §3.4:* **"Cascaded PID: inner loop on (theta − 0)... outer loop on wheel-velocity error."** No encoders, but a *commanded-PWM-as-velocity-proxy* outer loop is ~30 lines and solves "bot wanders into a fall" without hardware.

---

## 5. Recommended action order

### Tier 1 — Conservative tune (next session, ~2 h)

Pure parameter changes, no architecture. **Do this before anything else.** Probability of balancing: 60–80% on a sound chassis (less if the right-motor stiction asymmetry noted in session_records §6 is severe).

1. `balance_app.cpp` defaults → `Kp=15, Ki=0, Kd=8`.
2. Slew bands per §3d, ±100 cap unchanged.
3. Add a 15-ms IIR LP on `measurement` inside `pid_controller.cpp::compute()` ahead of the D-term — four lines.
4. Gate `online_est_.update()` off whenever `|pitch_deg_| > 3°` in `step_run_()`. This stops the estimator from poisoning itself on tipovers.
5. Flash, prop, observe. Bot oscillates → drop Kp 30%. Bot drifts → raise Kp 30%. Only then re-introduce `Ki ≈ Kp/2`.

Acceptance: stays vertical ≥5 s unaided, motors never peg above 80 PWM during balance.

### Tier 2 — Drop NDOF, build the real pitch filter (1–2 sessions, ~6 h)

The architecture the research has been calling for since day one.

1. Add `getRawGyro()`/`getRawAccel()` virtuals to `OrientationSensor`; BNO055 impl uses `getVector(VECTOR_GYROSCOPE / VECTOR_ACCELEROMETER)`.
2. Switch BNO055 from `OPERATION_MODE_NDOF` to `OPERATION_MODE_AMG` (raw, no on-chip fusion). Latency drops to ~3 ms.
3. Create `src/navigation/balance_kalman.{h,cpp}` per MASTER_DESIGN §4.7 — Lauszus 2-state. ~60 LOC, ~40 B RAM, ~150 µs/tick (numbers from research §3).
4. Feed its output to `BalanceApp::pitch_deg_`; the D-term now operates on `−gyro_y` directly, no differentiation of a quantized fused angle.
5. Re-run Tier 1 acceptance; expect to raise Kd noticeably with the cleaner signal.

This is the actual fix. Tier 1 is a stopgap.

### Tier 3 — Madgwick on raw BNO055 (only if Tier 2 is insufficient)

Swap the 2-state Kalman for Madgwick on raw gyro+accel from AMG mode. *mpu6050_external_mag_pipeline.md D12:* **"Madgwick (not Mahony)."** ~600 µs/tick, ~120 B RAM. Overkill for a 2-wheel balancer specifically — only do this if a future application on the same flow needs 3-axis attitude.

---

## 6. One paragraph

Gains are too hot, the D-term is amplifying BNO055 quantization, NDOF fusion costs ~30 ms of lag the controller cannot afford, the online estimator is contaminated by every fall it observes, and the slew rate near zero is tighter than the plant's own dynamics. Cut Kp/Kd by ~4×, set Ki=0, add a 15 ms LP on the PID measurement, gate the online estimator off when `|pitch|>3°`, and you will probably balance. Then build the 2-state Kalman the research has been waiting for and stop trusting the BNO055 to do your fusion for you.

---

See also: [../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](../MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) — the project's current design direction that grew out of this diagnosis.
