# PID Tuning Guide

Practical guide for tuning the flight controller's PID loops using the calibration-mode `g` command. This is a runtime-mutable workflow: gains are adjusted live over serial while tethered, then dumped (`d`) and pasted into `include/config.h`.

This guide documents what already exists in the firmware. It does not propose new features. For the firmware-wide calibration story see `docs/2_calibration_guide.md`; for symptom-based diagnostics see `docs/3_troubleshooting.md`.

---

## 1. When to tune

PID gains are physical-system properties. They change with the airframe, not with the firmware. Re-tune after:

| Trigger | Why |
|---------|-----|
| New frame, new arms | Different inertia changes rate response |
| Repositioned battery or payload | Shifts CG, changes pitch/roll inertia ratio |
| New props | Different thrust-to-RPM curve, more/less D-term noise |
| New motors or ESCs | Different torque, response time |
| IMU re-mounted or replaced | Different mounting compliance changes D-term noise floor |
| Frame stiffness change (arm flex, soft mounts) | Adds resonant modes that interact with D |

Do **not** re-tune after every flight. The saved values in `config.h` are the source of truth. Once the airframe is stable, leave them alone.

---

## 2. Pre-flight safety

PID tuning happens with motors spinning. Treat every session like a real flight.

1. **Props OFF first.** Get the firmware responding to `g` commands on the bench. Verify the PID loop is updating gains live (see §3).
2. **Tethered hover with props ON.** Use a tether or bench cradle that restrains the airframe but allows enough freedom for the loop to exhibit roll/pitch behavior. The airframe must be free enough to oscillate when P is pushed too high — that's the whole point.
3. **Battery securely strapped.** A loose battery shifts CG mid-flight and invalidates the tune.
4. **Receiver bound + failsafe verified.** Run `f` (failsafe auto-detection, see `2_calibration_guide.md`) before tuning. CH5 arming switch must cut motors instantly.
5. **Calibration build flashed.** PID tuning only works in calibration builds; the live build hard-codes gains from `config.h`.

```bash
# Teensy 4.0
pio run -e teensy40_calibration -t upload

# ESP32 (or esp32s3_calibration)
pio run -e esp32_calibration -t upload
```

6. **IMU calibrated.** Tuning on an uncalibrated IMU is fighting the sensor, not the airframe. Run `i` or `m` first.

---

## 3. The `g` workflow

All tuning happens through the serial command interface in the calibration build. There are three forms of the `g` command:

| Form | Effect |
|------|--------|
| `g` | Print all 9 PID gains and the active controller mode |
| `g <name>` | Print one gain |
| `g <name> <value>` | Set one gain (live, takes effect within the next loop tick) |

### 3.1 Open the serial monitor

```bash
# Recommended — auto-detects port, drains boot output
python3 tools/serial_monitor.py /dev/ttyACM0

# Or via the menu wrapper
./tools/calibrate.sh

# Or PlatformIO's built-in monitor (no boot drain, no termios safety)
pio device monitor
```

To capture a tuning session to a file (useful for later review):

```bash
python3 tools/serial_monitor.py /dev/ttyACM0 --output sessions/tune_2026-MM-DD.txt
```

### 3.2 Print current gains

Type `g` and press Enter. Expected output (angle mode, default values):

```
=== PID GAINS ===
Mode: ANGLE
  kp_roll=0.200000  ki_roll=0.000000  kd_roll=0.050000
  kp_pitch=0.200000  ki_pitch=0.000000  kd_pitch=0.050000
  kp_yaw=0.300000  ki_yaw=0.050000  kd_yaw=0.000150
Set: g <name> <value>  (e.g. g kp_roll 0.2)
```

In rate mode (`USE_RATE_CONTROLLER`) the `Mode:` line reads `RATE` and the roll/pitch defaults come from the rate-mode block in `config.h` (Kp=0.15, Ki=0.15, Kd=0.0004).

### 3.3 Recognized gain names

| Name | Variable | Source default (angle mode) | Source default (rate mode) |
|------|----------|------------------------------|-----------------------------|
| `kp_roll` | `tune_kp_roll` | `KP_ROLL_ANGLE` (0.20) | `KP_ROLL_RATE` (0.15) |
| `ki_roll` | `tune_ki_roll` | `KI_ROLL_ANGLE` (0.00) | `KI_ROLL_RATE` (0.15) |
| `kd_roll` | `tune_kd_roll` | `KD_ROLL_ANGLE` (0.05) | `KD_ROLL_RATE` (0.0004) |
| `kp_pitch` | `tune_kp_pitch` | `KP_PITCH_ANGLE` (0.20) | `KP_PITCH_RATE` (0.15) |
| `ki_pitch` | `tune_ki_pitch` | `KI_PITCH_ANGLE` (0.00) | `KI_PITCH_RATE` (0.15) |
| `kd_pitch` | `tune_kd_pitch` | `KD_PITCH_ANGLE` (0.05) | `KD_PITCH_RATE` (0.0004) |
| `kp_yaw` | `tune_kp_yaw` | `KP_YAW_RATE` (0.30) | `KP_YAW_RATE` (0.30) |
| `ki_yaw` | `tune_ki_yaw` | `KI_YAW_RATE` (0.05) | `KI_YAW_RATE` (0.05) |
| `kd_yaw` | `tune_kd_yaw` | `KD_YAW_RATE` (0.00015) | `KD_YAW_RATE` (0.00015) |

Yaw always uses rate control, even in angle mode. There is no `kp_yaw_angle`.

### 3.4 Set a gain

```
g kp_roll 0.25
```

Expected echo:

```
  kp_roll = 0.250000
```

The new value is written directly to the runtime variable and takes effect on the next PID computation (~1 ms on ESP32, ~0.5 ms on Teensy). If the name is unrecognized:

```
Unknown gain: kp_rol
```

### 3.5 Save the tune

There is no in-firmware "save to flash" — the calibration build is intentionally volatile so a bad tune cannot persist across power cycles. To make a tune permanent:

1. Type `d` to dump all calibration values:

```
>>> Dumping all calibration values

// ===== CALIBRATION VALUES -- copy into config.h =====
...
//=============================================================================
// PID Gains (current runtime values)
//=============================================================================
// Angle mode gains
#define KP_ROLL_ANGLE 0.250000
#define KI_ROLL_ANGLE 0.000000
#define KD_ROLL_ANGLE 0.060000
...
#define KP_YAW_RATE 0.300000
#define KI_YAW_RATE 0.050000
#define KD_YAW_RATE 0.000150
```

The dump format matches `config.h`'s rate-mode or angle-mode block depending on which controller is compiled in.

2. Copy the `KP_*`/`KI_*`/`KD_*` lines into `include/config.h` (replace existing values).
3. Uncomment `#define CALIBRATED_PID` in the calibration-status markers block of `config.h`.
4. Re-flash. For verification, flash the calibration build again and confirm `c` shows `[x] PID gains`. For flight, flash the live build:

```bash
pio run -e teensy40 -t upload
```

---

## 4. Conservative starting values

The defaults in `include/config.h` were chosen to be safe for a typical 5" quad X. Treat them as the starting point — they may already fly, in which case "tuning" means small adjustments rather than a full sweep.

| Airframe class | Suggested starting Kp (roll/pitch) | Ki | Kd | Notes |
|----------------|-------------------------------------|----|----|-------|
| Quad X, 5" (default) | `KP_ROLL_RATE` / `KP_ROLL_ANGLE` defaults (0.15 rate / 0.20 angle) | 0 (angle) or 0.15 (rate) | `KD_ROLL_RATE` / `KD_ROLL_ANGLE` defaults | Stock defaults are aimed here |
| Quad X, 7" (heavier, slower) | Start at 70% of default Kp | 0 | 50% of default Kd | More inertia, less aggressive needed |
| Tricopter / Y3 | Start at default Kp roll/pitch, reduce Kd yaw to half default | 0 | Default Kd roll/pitch | Yaw mixer interacts with tail servo; high Kd_yaw amplifies servo jitter |
| Other VTOL (hex, octo, fixed-wing, hybrid) | Not covered by this guide | — | — | Mixer math is user-supplied; see `scope.md` for the FC's role as a stabilizer only |

Yaw rate defaults (`KP_YAW_RATE=0.30`, `KI_YAW_RATE=0.05`, `KD_YAW_RATE=0.00015`) are conservative and usually need less attention than roll/pitch. Tune roll/pitch first.

> **All numeric ranges above are derived from `include/config.h` defaults.** Do not invent gain values for airframes the firmware has not been calibrated on. If your frame is none of the rows above, start at the default and follow §5 carefully.

---

## 5. Tuning loop

Tune **one axis at a time** — typically roll first, then pitch (they should be near-identical on a symmetric quad X), then yaw if needed. For each axis:

1. **Start conservative.** Begin at the table value from §4 (or current `config.h` value). Set Ki and Kd to zero or near-zero so you can see pure P behavior:

   ```
   g ki_roll 0
   g kd_roll 0
   ```

2. **Push Kp until oscillation appears.** Hover tethered, gently nudge the stick on the axis under test. Increase Kp in ~10% steps:

   ```
   g kp_roll 0.22
   ```

   The bot's response should get crisper. Eventually the airframe will start to oscillate at a fast frequency (5–10 Hz typical for a small quad). That is the Kp ceiling.

3. **Back off 20–30%.** Reduce Kp to 70–80% of the value that caused oscillation:

   ```
   g kp_roll 0.18
   ```

4. **Add Kd to dampen overshoot.** Increase Kd in small steps from its starting value. Each axis nudge should now settle without overshoot ringing:

   ```
   g kd_roll 0.06
   ```

   If the motors start sounding rough (high-pitched whine that pulses with airframe vibration), Kd is too high — back it off and consider lowering the D-term LPF cutoff (see §7).

5. **Add Ki last, only if needed.** Ki corrects steady-state drift (the airframe settles with a small angular error and doesn't recover). Most angle-mode quads do not need Ki at all (default is 0). Add it sparingly:

   ```
   g ki_roll 0.05
   ```

   If hover starts to "wobble" slowly (<1 Hz), Ki is too high.

6. **Test progressively harder maneuvers.** Once a single axis is stable in hover, test fast stick movements, then deliberate disturbances (tap the airframe lightly). The tune should hold.

7. **Repeat for the other axis.** Pitch gains should typically match roll on a symmetric quad. Set `kp_pitch`, `ki_pitch`, `kd_pitch` to the same values as roll, then fine-tune from there.

8. **Save.** Run `d`, copy the output, paste into `config.h`, mark `CALIBRATED_PID`, re-flash.

---

## 6. Reading serial output

While tuning, the `t` command toggles telemetry, which can help diagnose tuning state visually:

| Mode | Rate | Content |
|------|------|---------|
| `t` once (off → IMU) | 50 Hz | Accel + gyro values; watch gyro for oscillation magnitude |
| `t` twice (→ FULL) | 20 Hz | Accel + gyro + attitude + motor outputs; full picture |
| `t` three times (→ QUAT) | 50 Hz | Quaternion + gyro for gimbal-lock-free output |
| `t` four times (→ off) | — | Back to silent |

The telemetry feed is the line printed once per `DEBUG_PRINT_INTERVAL` (10 ms by default, see `config.h`). Visualize it by piping into a plotter such as fc_tool, or by reviewing the captured `--output` file later.

The `s` command prints channel state and arming state:

```
=== STATUS ===
CH1: 1500  CH2: 1500  CH3: 1000  CH4: 1500  CH5: 1000  CH6: 1500
Armed: NO
```

The `c` command shows which calibration stages are complete; once you save and re-flash with `CALIBRATED_PID` uncommented, this should read `[x] PID gains (g)`.

---

## 7. Common pitfalls

### Kd amplifies gyro noise

**Symptom:** Motors whine, airframe feels "buzzy", high-frequency vibration audible. Worse at high throttle.

**Cause:** D-term derivative of a noisy gyro signal blows up the motor output.

**Cure:** Reduce Kd, or lower the D-term low-pass filter cutoff. The base filter is controlled by `B_DTERM` (defaults to 0.15 in `config.h`); lower values filter more aggressively. Adjust live with the `p` command:

```
p b_dterm 0.10
```

If `USE_OPTIMIZATION` is enabled, there is also a biquad D-term filter with `DTERM_LPF_CUTOFF_HZ` (default 80 Hz). Lower it (e.g. 60 Hz) for noisier setups. This is a compile-time value — edit `config.h` and reflash the calibration build.

### Ki windup on slow movements

**Symptom:** After a long, gentle correction, the airframe overshoots and oscillates slowly before settling.

**Cause:** Integral term accumulating beyond the bounds enforced by `I_LIMIT_ROLL` / `I_LIMIT_PITCH` / `I_LIMIT_YAW` (all default 25.0 in `config.h`).

**Cure:** Reduce Ki. The integral limits are compile-time only; they bound runaway but do not change the rate of accumulation.

### Setpoint smoothing interactions with `USE_RACING`

If `USE_RACING` is enabled and `SETPOINT_SMOOTH_CUTOFF_HZ > 0`, stick input is low-pass filtered before reaching the PID loop. This adds phase lag — gains tuned without smoothing will feel sluggish when smoothing is added later, and vice versa. Pick smoothing first, then tune. Default is 0 (disabled).

### TPA (Throttle PID Attenuation)

When `USE_RACING` is enabled, PID outputs are reduced above the `TPA_BREAKPOINT` throttle level (default 0.65) by up to `TPA_RATE` (default 0.5 = 50%). This prevents high-throttle oscillation but means gains tuned at hover throttle will feel weaker at full throttle. Tune at hover throttle first; if oscillation appears only at full throttle, increase `TPA_RATE` rather than dropping Kp globally. Both values are compile-time in `config.h`.

### Tuning angle mode then switching to rate mode (or vice versa)

The controller is compile-time-selected (`USE_ANGLE_CONTROLLER` vs `USE_RATE_CONTROLLER`). The gain ranges are different (angle Kp ≈ 0.20, rate Kp ≈ 0.15; angle Kd ≈ 0.05, rate Kd ≈ 0.0004 — three orders of magnitude). **Do not copy gains across modes.** When switching modes, start fresh from §4.

### Yaw needs less attention

The default yaw gains (`KP_YAW_RATE=0.30`, `KI_YAW_RATE=0.05`, `KD_YAW_RATE=0.00015`) are robust across airframe sizes. If yaw is loose, increase `kp_yaw` slightly. If it twitches at hover, reduce `kd_yaw`. Avoid heroics on yaw.

---

## 8. When to re-tune

See §1. The short version: physical changes to the airframe invalidate the tune. Firmware changes (new build flags, optimization toggles) generally do not — but enabling `USE_OPTIMIZATION` or `USE_RACING` introduces new filter and response behavior that may shift the optimum, so re-validate with a hover after enabling them.

---

## 9. References

- `docs/2_calibration_guide.md` — overall calibration workflow and command reference (the `g` command appears in the command table there)
- `docs/3_troubleshooting.md` — symptom-driven diagnostics: "oscillates rapidly", "wobbles slowly", "drifts in one direction", "sluggish response" all cross-reference this guide
- `docs/findings/auto-calibration-research.md` — historical research context on auto-tuning approaches considered for this firmware
- `docs/scope.md` — the bare-bones philosophy that explains why this guide documents a manual workflow rather than an autotune feature
- `include/config.h` — `PID Controller Gains - RATE MODE` and `PID Controller Gains - ANGLE MODE` blocks; `OPTIMIZATION PARAMETERS` and `RACING PARAMETERS` for filter and TPA settings
- `src/calibration_mode.cpp` — implementation of the `g`, `p`, `d`, `c`, `t` serial commands (`processSerialLine`, `findGain`, `printGains`)
- `lib/Calibration/calibration.cpp` — `printAllCalibrationValues()` (the `d` command output)
- `src/control.cpp` — where `tune_kp_roll` and friends are consumed by the PID loop; also where TPA and setpoint smoothing are applied
