# Self-Balancing Robot — Troubleshooting (Mega tier)

Symptom → diagnostic → remedy for the `mega_balance` build. Failure modes are organised around the **real state machine** (`IDLE` / `CAPTURE_MOUNTING` / `BOOTSTRAP` / `RUN` / `HELD`) and the diagnostic codes the firmware actually surfaces (`BootstrapResult.failure_reason`, `held_entry_reason_`, `PwmDiscoveryResult.failure_reason`, `posgains_failure_reason_`).

> Honest reminder: the Mega bot has not yet successfully balanced on hardware. Many of the issues below are *expected* on a first bring-up attempt; the troubleshooting steps below describe the **diagnostic procedure**, not a guarantee that working through them will produce a balancing bot today. See the STATUS BANNER in [`USER_GUIDE.md`](USER_GUIDE.md) and [`../../KNOWN_ISSUES.md`](../../KNOWN_ISSUES.md) for the unresolved issues.

For framework-wide issues (build failures, serial port problems, IMU-level calibration), see [`../../getting_started/GETTING_STARTED.md`](../../getting_started/GETTING_STARTED.md) and [`../../calibration/CALIBRATION_GUIDE.md`](../../calibration/CALIBRATION_GUIDE.md).

---

## Table of contents

1. [Boot and IMU](#1-boot-and-imu)
2. [BOOTSTRAP failure_reasons](#2-bootstrap-failure_reasons)
3. [HELD entry reasons and recovery](#3-held-entry-reasons-and-recovery)
4. [RUN-state hangs and stuck-motor cut](#4-run-state-hangs-and-stuck-motor-cut)
5. [FALLEN — note on default-off in this build](#5-fallen--note-on-default-off-in-this-build)
6. [Mount capture and recapture criteria](#6-mount-capture-and-recapture-criteria)
7. [Encoder issues](#7-encoder-issues)
8. [PWM discovery failures](#8-pwm-discovery-failures)
9. [Outer-loop gain derivation fallback](#9-outer-loop-gain-derivation-fallback)
10. [Power and brown-out](#10-power-and-brown-out)
11. [Last resort — clean reflash](#11-last-resort--clean-reflash)

---

## 1. Boot and IMU

### Boot prints `BF` then halts

`BF` is `imu.begin()` returning false — the BNO055 isn't on the I²C bus. Almost always wiring:

- Confirm SDA → pin 20, SCL → pin 21. Easy to swap.
- Confirm the BNO055 ADR/SA0 pin is tied to GND (default address `0x28`); a floating ADR can land the chip at `0x29` instead, which the driver doesn't probe.
- Run an I²C scanner sketch to confirm the chip responds at all.
- If you bought a generic (non-Adafruit) BNO055 board with no on-board crystal, build with `-D BNO055_NO_EXT_CRYSTAL` — otherwise the fusion pipeline silently freezes and `cal_*` stays at 0 forever.

### Boot prints `B` then nothing

Either Serial baud mismatch (firmware is hardcoded to 115200) or the IMU init succeeded but no further setup completed — usually the BNO055 cal wizard waiting for `g=3 a=3`. See [`CALIBRATION_WORKFLOW.md §3`](CALIBRATION_WORKFLOW.md#3-bno055-calibration-wizard).

### Cal blob CRC fails on every boot

`restoreFromEEPROM` returns false → wizard re-runs every power-cycle.

- First boot: expected.
- Persistent across reboots: brown-out during EEPROM writes. Confirm the boost converter is steady ≥ 4.7 V under motor load. See [§10](#10-power-and-brown-out).
- If a recent firmware change moved the EEPROM layout, the old slot may not match the new magic/version — re-run the cal wizard once and the new format takes over.

---

## 2. BOOTSTRAP failure_reasons

Every BOOTSTRAP exit prints (via the deferred state log) `[state] -> IDLE` and the per-pulse `bs#…` lines preceding it. The drain order is: `bs#0…bs#3` (pulse outcomes), `bs#0xFD` (K cross-check summary, Mega only), `bs#0xFF` (baseline-noisy sentinel, only when reason=6), `bs#0xFE` (collision sentinel, only when reason=5). Then `[state] -> IDLE`.

You can also read the result struct via `s`'s state field (`BOOT` → bot still bootstrapping; `IDLE` after a fail).

### `failure_reason = 1` — `pitch_out_of_range`

The bot's corrected pitch left ±10° during BASELINE or ±15° during a pulse.

Diagnostic:
- Was the bot upright when you triggered BOOTSTRAP? The pre-condition is `|corrected_pitch| < BOOTSTRAP_MAX_INIT_PITCH = 10°`.
- Did the bot tip during a pulse? The 180/240 PWM pulses are strong enough to noticeably rotate an unrestrained, top-heavy chassis.

Remedy:
- Prop the bot more squarely upright before sending `b`.
- If the bot tips mid-pulse: hold it lightly between thumb and finger during BOOTSTRAP — enough to prevent tipping, not enough to fight the pulse (the `baseline_noisy` gate will catch you if you grip too hard).
- If prop-and-go fires at boot but BOOTSTRAP immediately bails with reason=1, the saved mount offset is probably wrong for the current physical setup. Recapture with `c`.

### `failure_reason = 2` — `no_response`

Fewer than `N_PULSES / 2 = 2` pulses produced a |Δω| above the noise threshold (`3 ×` baseline range). The motors didn't excite the chassis enough to measure K.

Diagnostic:
- Check the `bs#0`/`bs#1`/`bs#2`/`bs#3` lines: were the `ok=` fields all 0?
- Check `m=` (the measured |Δω|): if it's literally 0 across all pulses, motors aren't running. Check L298N power, motor leads, fuse.
- Check `thr=` (the threshold): if it's > 15 dps the baseline was noisy (this should have tripped reason=6 instead — file a bug).
- If `m=` is non-zero but small (~2-5 dps) on all pulses, motors are running but extremely weak. Battery sag, undersized boost converter, or motors not making mechanical contact.

Remedy:
- Confirm both motors actually spin: send `p` to drive the discovery ramp and watch the encoders tick.
- If only one motor moves: L298N IN-pin wired wrong, or motor lead broken. Recall the pin map: ENA=5, IN1=7, IN2=6, ENB=10, IN3=8, IN4=9 (IN1/IN2 deliberately swapped from the legacy `.ino`).
- If both motors are weak: voltage sag. Probe the L298N's logic pin while a pulse is running.

### `failure_reason = 3` — `k_out_of_bounds`

Measured K was clamped by ≥ 50 % when handed to `PlantIdentifier::seed_k_motor()`. Means the measurement was so wildly outside class-typical bounds that it had to be limited to a reasonable value.

Diagnostic: see the per-pulse `m=` values. If they're all huge (~hundreds of dps) the IMU axis is probably swapped — pulse-induced gyro is appearing on the wrong axis and pickup-from-cross-coupling produces nonsense.

Remedy: verify the BNO055 mounting orientation. The firmware assumes `raw_gyro_dps_[1]` (Y-axis) is the pitch-rate axis (`balance_app.cpp:858` carries a TODO to bench-verify this). If the breakout is mounted with X facing forward, you need to rotate it 90° about the vertical (and re-cal IMU + remount).

### `failure_reason = 4` — `user_abort`

Operator sent `a` (or `t` / long-press) during BOOTSTRAP. No remedy needed; this is the explicit exit path.

### `failure_reason = 5` — `collision`

The three-gate LIA detector (peak / sustain / kick) latched during BASELINE or a pulse. The bot was bumped or set down hard during identification.

Diagnostic: the `bs#0xFE` sentinel line carries `m=` = the LIA magnitude (m/s²) that tripped, `thr=` = the PEAK threshold (`COLLISION_PEAK_MPS2 = 12.0`).

Remedy: set the bot down more gently, then re-trigger `b`. If reason=5 fires even on a stationary bot, something is mechanically transmitting motion (loose IMU breakout, wobbly desk).

### `failure_reason = 6` — `baseline_noisy`

During the 300 ms BASELINE window the peak-to-peak gyro pitch-rate exceeded `BOOTSTRAP_NOISE_FLOOR_MAX_DPS = 5.0 dps`. The implied 3× threshold would have been > 15 dps and no real pulse could clear it — fail fast and tell the operator instead of letting all 4 pulses spuriously fail with reason=2.

Diagnostic: the `bs#0xFF` sentinel line shows `m=` = measured baseline range, `thr=` = the 5.0 dps cap.

Remedy: the operator was almost certainly still touching / placing the bot during the baseline window. Release the bot fully ≥ 500 ms before sending `b`, then keep hands off until you see `[state] -> RUN` (or another failure_reason).

### `failure_reason = 7` — `k_disagreement` (Mega only)

`K_encoder` and `K_gyro`, computed over the same set of pulses, differ by > 30 %. Either:

- The wheels are slipping (high `K_gyro`, low `K_encoder` — chassis moves but encoders don't see the same motion).
- The drivetrain is binding (low `K_gyro`, high `K_encoder` — encoders register motor rotation but chassis barely moves).
- The encoder calibration is wrong (wrong wheel radius scales `K_encoder` wrongly).

Diagnostic: the `bs#0xFD` summary line shows `g0=` = `K_gyro × 10`, `m=` = `K_encoder × 10`, `thr=` = `30`. Compare them — which is too high?

Remedy:
- Re-run encoder cal `e` (Section [`CALIBRATION_WORKFLOW.md §5`](CALIBRATION_WORKFLOW.md#5-encoder-calibration--e-mega-only)). A bad radius is the most common cause.
- Check for physical bind: do both wheels spin freely with the motors off? Listen for grinding.
- Check for slip: smooth/dusty surface, or wheels not making firm contact? Bench on tape or grip surface.

### `failure_reason = 8` — `pwm_discovery_timeout` (reserved)

Defined in the `PwmDiscoveryResult` failure_reason enum (Phase 4M.12) — never set on a BOOTSTRAP result. Only relevant when reading `get_pwm_discovery_result()`. See [§8](#8-pwm-discovery-failures).

---

## 3. HELD entry reasons and recovery

When RUN→HELD fires, `held_entry_reason_` is stamped to one of:

| Value | Name | What it means | What to do |
|-------|------|---------------|------------|
| 1 | `COLLISION` | Three-gate LIA detector latched during RUN (peak > 12 m/s², sustained > 8 m/s² for 3 ticks, or kick > 6 m/s² + |gyro_pitch| > 200 dps) | Set the bot back down level and quiet. HELD→RUN dwell is 200 ms (40 ticks at 5 ms PID). If nothing's actually wrong, the bot resumes on its own. |
| 2 | `GYRO_ANOMALY` | Encoder stall (Mega only) — `cmd_pwm > 100` for `> 300 ms` but encoder reads no motion. Wheel is restrained or stuck. | Free the wheel. HELD will auto-resume. If it re-stalls immediately, mechanical binding or a dead motor. |
| 3 | `OPERATOR_HANDLING` | Either `ext_motion` (cmd_pwm small but |gyro_pitch| > 30 dps — bot is being moved without motor authority) or `lift_detected` (|accel|−g > 6 m/s² — bot was picked up) | Expected when you handle the bot. Set down level. |

Read it via the `s` command's state field — when HELD, you'll see `HELD` in field 1 and can correlate with the most recent `[state] -> HELD` log line and any preceding pulse / sensor events.

Recovery paths from HELD:
- **Auto-resume**: bot quiet (`g_lateral_lpf < 12 dps` AND `a_dev_lpf < 1.5 m/s²`) and level (|pitch| < 8°) for 200 ms.
- **Operator force-resume**: send `c` or short-press the button — skips the dwell.
- **Abort to IDLE**: send `a` or long-press.

There is no HELD timeout. The bot stays HELD indefinitely if motion stays above the gates (or the bot sits at > 8° pitch on a shelf). This is intentional — operator preference is "always be ready to balance, motors off until conditions warrant".

If HELD oscillates rapidly with RUN (entering and exiting every second or two), the lateral-gyro threshold is too tight for your chassis. The 2026-05-18 PM bench removed the 90 dps lateral-gyro trigger entirely for exactly this reason — it was firing on every wheel-engaged recovery transient. If you re-introduce it, expect to tune.

---

## 4. RUN-state hangs and stuck-motor cut

### Bot enters RUN, drives motors at full PWM, doesn't move

The stuck-motor detector catches this. After `|output| ≥ 180 PWM` AND `|gyro_pitch| < 5 dps` for ≥ 1500 ms, the firmware stops motors and requests abort (→ IDLE). Diagnostic in `s` will show the abort took effect.

This usually means the bot is mechanically restrained (operator holding it, chassis pinned against an obstacle) or one wheel is stalled. The PID is correctly trying to recover from an apparent pitch error but the plant is unresponsive.

Remedy: free the bot, send `b` to re-bootstrap. If it happens again immediately on the same wheel: mechanical issue (gearbox stripped, wheel slipping on shaft).

### Bot enters RUN, immediately tips and lies on its side

`USE_BALANCE_FALL_DETECTION` is OFF on this build (the default — see `platformio.ini`). The PID keeps computing through tipover but `step_run_`'s soft-cutoff zeros the motor output when `|pitch| > 25°`, so the bot just lies there with motors quiet. This is intentional ("no acceleration → no motors" — see `MINIMIZE_ACCELERATIONS_PHILOSOPHY.md`).

Recovery: set the bot upright; the PID will re-engage once `|pitch| < 25°`. If the prior K_motor was bad, send `b` to re-bootstrap.

### Bot starts in RUN, balances for a few seconds, then enters HELD repeatedly

Most likely `held_entry_reason_ = 1 (COLLISION)` — the LIA detector firing on aggressive recovery transients that exceed the 12 m/s² peak gate. Bench-tune by either:

- Re-running `b` for a fresh K_motor + softer gains (battery sag often raises gains).
- Adjusting the collision-detection constants in `balance_app.h` (`COLLISION_PEAK_MPS2`, `COLLISION_SUSTAIN_MPS2`, `COLLISION_KICK_MPS2`). The 12 m/s² peak was set conservatively to catch real impacts on the small Mega chassis.

If `held_entry_reason_ = 3 (OPERATOR_HANDLING)` keeps firing, the `lift_detected` gate (|accel|−g > 6 m/s²) is tripping on intrinsic balance motion. Same fix: re-bootstrap, or relax the gate.

### Pitch kill-switch fires (`p > 20° or < −20°`)

Belt-and-suspenders backstop in `main.cpp` `loop()`. Cuts motors and requests abort regardless of state when the absolute pitch exceeds ±20° (also catches NaN/Inf — see audit P1-001). Recovers via the normal IDLE path.

### Watchdog starvation cuts motors

Also in `main.cpp` `loop()`: if the 200 ms tick watchdog goes hungry, motors stop and abort fires. This catches a wedged `MsTimer2` ISR. If it fires repeatedly without obvious cause, suspect ISR contention with the I²C driver — try increasing the watchdog timeout in `BalanceSafety` for diagnosis.

---

## 5. FALLEN — note on default-off in this build

The `mega_balance` env does NOT define `USE_BALANCE_FALL_DETECTION`. The FALLEN state is **unreachable** in normal operation — `step_run_` never transitions to FALLEN; it relies on the soft-cutoff and the absolute-pitch kill-switch instead.

If you enable `-D USE_BALANCE_FALL_DETECTION` at build time:

- RUN → FALLEN on `safety_.is_tipover(pitch)` (default tilt limit 10°).
- FALLEN is **sticky** — motors stay off until operator explicitly restarts:
  - Short-press → BOOTSTRAP (re-measure K_motor; battery / surface may have changed).
  - `c` → CAPTURE_MOUNTING.
  - Long-press → abort to IDLE (no auto-restart).

The current default behaviour (no FALLEN, PID keeps computing through tipover) was preferred over sticky FALLEN because the operator complained the bot was "dead until you touch the serial monitor".

---

## 6. Mount capture and recapture criteria

### `[state] -> CAP` then `[state] -> IDLE` without auto-chaining to BOOTSTRAP

The Welford stillness gate (σ_pitch ≤ 0.5° over 2 s) failed. Causes:

- Bot wobbling on a soft surface (rubber mat, foam). Move to a hard surface.
- Operator's hand still on the bot during the window. Release and step back.
- IMU breakout physically loose on its header pins. Re-flow / tape down.
- Background vibration (washing machine, foot traffic). Wait it out or relocate.

Send `c` again to retry — capture is idempotent.

### Prop-and-go prints `stale_mount p=… m=…` and stays in IDLE

The saved mount (`m=`) is more than 5° off the current pitch (`p=`). Either:

- Operator propped the bot at a different angle than they did during the last capture.
- Bot was physically reassembled (IMU shifted on the chassis).
- Saved mount is genuinely stale (different chassis configuration).

Remedy: recapture with `c`. The new mount auto-saves and the next boot's prop-and-go should fire normally.

### Mount drifts to ±90° clamp during RUN

`OnlineMountingEstimator` tracks slow drift. The clamp is `±90°` at boot-load time (`load_mount_offset_`); the in-RUN drift cap is `set_max_drift_rate_dps(2.0)` (2 dps). If you ever observe the saved mount reaching ±90°, the load-time guard is what catches it.

Causes:
- Persistent physical disturbance the estimator interprets as mounting shift (someone constantly pushing the bot in one direction).
- A bug in the estimator's freeze gates — see `KNOWN_ISSUES.md` KI-4.

Recapture with `c` to reset to the operator-defined upright.

---

## 7. Encoder issues

### `e` wizard prints `enc_cal: no ticks - not saved`

One or both encoders read exactly 0 after rolling. Either:
- Wires disconnected (check pins 18/19 for left A/B, 2/3 for right A/B).
- Encoder ISR not attached. `enc_*.begin()` happens in `setup()` after `imu.begin()`; if IMU init hangs, encoder ISRs never attach.
- One wheel didn't actually roll (operator drag-shoved the bot, only the contact wheel rotated). Verify the live `L_ticks=… R_ticks=…` stream during rolling — both should change.

Remedy: re-wire / re-roll / retry.

### Live stream shows ticks going the wrong direction

A/B leads on that encoder are swapped. Either:
- Physically swap the A and B wires on that encoder.
- OR change the pin pair in `balance_app.h`'s `ENC_*_*` constants (this requires a re-flash).

Wheel direction matters because the K cross-check and the outer loop's velocity input both assume forward motion = positive ticks.

### Encoder stall HELD fires every few seconds in RUN

`held_entry_reason_ = 2 (GYRO_ANOMALY)`. The `ENCODER_STALL_PWM_THRESHOLD = 100` and `ENCODER_STALL_TIME_MS = 300` gates are tripping. Causes:

- One wheel is consistently restrained or stuck.
- Motor brushes intermittent — produces real motor command but no torque.
- Encoder ISR is dropping pulses (bus contention, interrupt latency). Less likely on Mega with dedicated INT pins, but possible if you've added other interrupt-heavy code.

Diagnostic: send `p` (PWM discovery) — watch the per-step `pd#` lines to see if either wheel's velocity stays at 0 even at high PWM.

---

## 8. PWM discovery failures

`PwmDiscoveryResult.failure_reason` values (from `balance_app.h`):

| Value | Name | Cause | Remedy |
|-------|------|-------|--------|
| 0 | `ok` | Both MIN and MAX locked within timeout | Result auto-saves to EEPROM 0x230. |
| 4 | `user_abort` | Operator sent `a` | No fix needed. |
| 8 | `pwm_discovery_timeout` | Reached `PWM_DISC_TIMEOUT_MS = 8000` ms without finding MIN or MAX | Motors very weak (battery sag) or one wheel completely stalled. Recharge battery; verify both wheels spin freely off the ground. |
| 9 | `pwm_discovery_collision` | LIA detector latched during the ramp — impact during discovery contaminates encoder/gyro readings | Set the bot down more gently when it's not held off the ground. Or hold it more steadily during the ramp. |

On failure, the EEPROM slot is **not** updated — the previous-run value (if any) remains the boot-time stiction floor.

---

## 9. Outer-loop gain derivation fallback

`posgains_failure_reason_` (Mega only, in `BalanceApp`):

| Value | Meaning |
|-------|---------|
| 0 | Derivation ran and produced sane gains. They're installed in `PositionLoop`. |
| 9 | `derived_gains_oob` — at least one of `K_POS` / `K_VEL` / `POS_LEAK` fell outside the sanity envelope (`K_POS ∈ [1, 30]`, `K_VEL ∈ [0.5, 15]`, `POS_LEAK ∈ [0.990, 0.9999]`), OR the wheel radius wasn't trusted (no encoder cal or `r ≥ 1 m`). The conservative fallback gains `K_POS = 6`, `K_VEL = 3`, `POS_LEAK = 0.999` are installed. **BOOTSTRAP still succeeds; the bot still balances.** |

This is a **degraded session, not a failure**. The fallback gains are intentionally conservative (the Phase 4M.13 hand-picks) — the bot will balance and hold station, just with less authority than the derivation would provide.

To return to the derived path: re-run encoder cal (`e`) so the radius is fresh and trusted, then trigger BOOTSTRAP again.

Read `posgains_failure_reason_` via the `g` telemetry line: the `k_pos` / `k_vel` / `pos_leak` fields will be the fallback values (6.0 / 3.0 / 0.999) on the rejection path, and the derived values otherwise.

---

## 10. Power and brown-out

### Mega resets when motors engage

Motor current is dropping the 5 V rail below the Mega's brown-out threshold (~4.5 V). Same fixes that have worked historically:

- Confirm the boost converter output is set to 5.0 V before connecting; verify under load.
- Add bulk capacitance (1000 µF / 16 V) across the L298N's motor supply pin.
- Add 100 µF across the Mega's 5V/GND for ride-through.
- Upgrade to a higher-capacity 18650 cell or a 2S Li-Ion pack — lower internal resistance = less sag.

### EEPROM corruption after every bench session

CRC fails on slot 0x200 / 0x210 / 0x220 / 0x230 across reboots. Almost always brown-out during a write. Same fixes as above.

### Battery dies under 1 hour

Probably motor stiction much higher than expected (continuous current to hold the wheels at the dead-band threshold). Run `p` to measure actual MIN_PWM — if it's > 80, the motors are taking serious current to just sit at stiction. Consider:
- Cleaner motor brushes.
- Different motors (these TT motors vary wildly between batches).
- Stronger battery.

---

## 11. Last resort — clean reflash

If multiple things are misbehaving and you suspect EEPROM corruption or a build state inconsistency:

```bash
cd auto_orientation
pio run -e mega_balance -t clean
pio run -e mega_balance -t upload
```

EEPROM survives a reflash (PlatformIO doesn't wipe it by default). To explicitly wipe a slot, write a known-bad CRC to it via an EEPROM debug sketch — or wait for the AO-FIN-05 `B` command which will (per its design) include a reverse import path that can also issue resets.

After a clean flash:
1. Watch for the cal wizard at boot — confirm EEPROM 0x000 either loaded cleanly or the wizard re-ran.
2. `s` — check the live mount / stiction values look sane.
3. `g` — confirm the outer-loop gains are either derived or the fallback (no NaN / weird values).
4. Walk through [`CALIBRATION_WORKFLOW.md §10`](CALIBRATION_WORKFLOW.md#10-calibration-order-on-a-fresh-board) end-to-end.

---

## Still stuck

1. Re-read [`USER_GUIDE.md`](USER_GUIDE.md) STATUS BANNER — make sure your expectation matches the firmware's bench-validation state.
2. Read [`../../KNOWN_ISSUES.md`](../../KNOWN_ISSUES.md) — the issue you're hitting may be listed as a known limitation rather than a bug.
3. Capture a full serial transcript covering: boot → `READY` → the failure event, and grep for `[state] ->`, `bs#`, `pd#`, `ch#`, and the cross-check sentinels (`0xFD`, `0xFE`, `0xFF`).
4. Cross-reference the failure_reason number you observed against [§2](#2-bootstrap-failure_reasons) or [§3](#3-held-entry-reasons-and-recovery) — every numeric code in the firmware is named here.
5. If the issue isn't covered, check [`../../findings/bootstrap_protocol_unstable_plant.md`](../../findings/bootstrap_protocol_unstable_plant.md) §11 AS-BUILT for the authoritative current state machine semantics.

---

*Last updated: 2026-06-21 — full rewrite. Failure modes reorganised around real `failure_reason` / `held_entry_reason_` / `posgains_failure_reason_` codes. AUTO_TUNE / `RELAY_AMPLITUDE_PWM` / "no_oscillation" content removed; mount-recapture criteria added; encoder pulse loss and PWM-discovery failures documented.*
