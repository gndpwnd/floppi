# Calibration Guide

Complete guide for calibrating your flight controller firmware.

> **Two calibration docs — which one?** This page is the **per-command procedure reference**: what each calibration command does, the menu workflow, and how to read each output. For the **hardware-staging / bring-up order** (which component to add and test first when assembling a new build), see [`features/calibration-guide.md`](features/calibration-guide.md).

---

## Overview

### Calibration Workflow

The firmware uses a **two-build workflow**:

1. **Calibration build** — includes calibration routines, serial command interface, debug output
2. **Live build** — lean flight firmware with hard-coded calibration values

**Workflow:**
```
Flash calibration build → Run calibration → Copy values to config.h → Flash live build → Fly
```

### What Needs Calibration?

| Component | Purpose | When to Calibrate |
|-----------|---------|-------------------|
| **IMU Offsets** | Zero out accelerometer/gyroscope biases | Initial setup, after crashes, temperature changes |
| **IMU Scale Factors** | Correct accelerometer gain errors | Optional, for higher accuracy |
| **IMU Orientation** | Detect how IMU is mounted on aircraft | Initial setup, if IMU is rotated/tilted |
| **Radio Channels** | Map transmitter controls to firmware | Initial setup, new transmitter |

### IMU Calibration Options

| Option | Command | Positions | What it calibrates | Use when |
|--------|---------|-----------|-------------------|----------|
| **Single-position** | `i` | 1 (level) | Offsets only | Quick calibration, good enough for most cases |
| **6-position** | `m` | 6 | Offsets + scale factors | Higher accuracy needed, or sensor seems off |

---

## Part 1: Build and Flash Calibration Firmware

### Step 1: Build Calibration Firmware

```bash
cd ~/floppi/flight_controller
pio run -e teensy40_calibration
```

Expected output:
```
Environment           Status    Duration
--------------------  --------  ------------
teensy40_calibration  SUCCESS   00:00:06.xxx
```

### Step 2: Upload to Teensy

```bash
pio run -e teensy40_calibration -t upload
```

### Step 3: Open Calibration Tool

**Recommended** — use the interactive calibration wrapper:
```bash
cd ~/floppi/flight_controller
./tools/calibrate.sh
```

This provides a menu-driven interface with all calibration commands, prerequisite checks (ModemManager, port detection), and interactive pass-through for y/n prompts.

**Alternative** — direct serial monitor:
```bash
python3 tools/serial_monitor.py /dev/ttyACM0
```

**Fallback** — PlatformIO serial monitor:
```bash
pio device monitor
```

**On startup, you'll see:**
```
========================================
  FLIGHT CONTROLLER READY!
========================================

=== CALIBRATION MODE ===
Serial commands (type in monitor):
  h - Help (full command list)
```

---

## Part 2: IMU Calibration

### Option A: Serial Command (Recommended)

1. Place aircraft **flat and perfectly level** on a stable surface
2. Ensure **no vibration** (turn off fans, step away from desk)
3. Type `i` in serial monitor and press Enter
4. Wait for calibration to complete (~10 seconds)

**Output:**
```
>>> IMU Calibration requested via serial

=== CALIBRATION MODE ACTIVATED ===
Running IMU Calibration (with quality checks)

Keep board FLAT and STILL for 10 seconds...
Checking stability...
Stability check passed! Starting calibration...
Collecting 2000 samples...
....................

=== IMU CALIBRATION RESULTS ===
Copy these lines to include/config.h:

#define IMU_ACC_ERROR_X 0.012345f
#define IMU_ACC_ERROR_Y -0.008765f
#define IMU_ACC_ERROR_Z 0.023456f
#define IMU_GYRO_ERROR_X 0.456789f
#define IMU_GYRO_ERROR_Y -0.234567f
#define IMU_GYRO_ERROR_Z 0.123456f

After editing config.h, rebuild with:
pio run -e teensy40 -t upload

=== CALIBRATION COMPLETE ===
```

### Option B: CH6 Switch

1. Place aircraft flat and level
2. **Disarm** (throttle low, CH5 low)
3. **Hold CH6 in MID position** for 3 seconds
4. LED blinks rapidly during calibration
5. Results print to serial monitor

### Step 5: Apply Calibration Values

1. **Copy** the printed `#define` lines
2. **Open** `include/config.h`
3. **Paste** into the IMU calibration section (replacing old values)
4. **Save** config.h

---

## Part 2B: 6-Position IMU Calibration (Optional)

**Use this for higher accuracy.** This calibration measures gravity in 6 orientations to calculate both offset AND scale factor for each accelerometer axis.

### When to use 6-position vs single-position

| Scenario | Recommendation |
|----------|----------------|
| First-time setup | Single-position (`i`) is sufficient |
| Drift issues after single-position cal | Try 6-position (`m`) |
| Precision application | Use 6-position |
| Quick recalibration | Use single-position |

### Run 6-Position Calibration

1. Type `m` in serial monitor
2. Follow prompts to hold aircraft in 6 positions:
   - Level (top up)
   - Upside down
   - Nose up
   - Nose down
   - Right side up
   - Left side up
3. Hold each position steady for ~2 seconds during measurement

**Output includes:**
- Accelerometer offsets (same as single-position)
- Accelerometer scale factors (new)
- Gyroscope offsets

```
#define IMU_ACC_ERROR_X 0.012345f
#define IMU_ACC_ERROR_Y -0.008765f
#define IMU_ACC_ERROR_Z 0.023456f
#define IMU_ACC_SCALE_X 1.002345f
#define IMU_ACC_SCALE_Y 0.998765f
#define IMU_ACC_SCALE_Z 1.001234f
#define IMU_GYRO_ERROR_X 0.456789f
#define IMU_GYRO_ERROR_Y -0.234567f
#define IMU_GYRO_ERROR_Z 0.123456f
```

---

## Part 3: IMU Orientation Detection

**Use this if your IMU is mounted at a non-standard angle** (rotated 90°, upside down, etc.)

### Run Orientation Detection

Type `o` in serial monitor, then follow the prompts:

```
>>> IMU + Orientation Calibration requested via serial

=== ORIENTATION DETECTION ===
This will detect how your IMU is mounted.

Position 1 of 3: LEVEL
Place aircraft flat and level, nose pointing forward.
Type 'y' when ready (timeout 30 seconds):
```

**You'll be guided through 3 positions:**
1. **Level** — aircraft flat on table
2. **Nose up** — aircraft tilted 90° nose pointing up
3. **Right side up** — aircraft rolled 90° to the right

**Output includes:**
- IMU offset calibration values
- Axis transformation code for `getIMUdata()`

---

## Part 4: Radio Calibration

**Use this to auto-detect your transmitter's channel mapping.**

### Run Radio Calibration

Type `r` in serial monitor:

```
>>> Radio Calibration requested via serial

=== RADIO CHANNEL CALIBRATION ===
This will auto-detect your transmitter channel mapping.

Step 1: Center all sticks and switches
Type 'y' when ready:
```

**You'll be guided through:**
1. Centering all controls
2. Moving each stick to identify throttle, roll, pitch, yaw
3. Toggling switches to identify AUX channels

**Output includes channel mapping for config.h.**

---

## Part: Failsafe Detection

**Use this to capture the PWM values your receiver outputs when the transmitter is OFF.** The firmware falls back to these values when RC link is lost — they must be safe (motors off, switches in a disarmed state).

> Related: [`0_quickstart.md`](0_quickstart.md) **Part 4a: Failsafe Bench Test** for the abbreviated bench-verify step. This section is the full procedure; 4a is the checklist.

### Preconditions

- TX bound to RX (complete the FlySky bind from [`0_quickstart.md`](0_quickstart.md) Part 1 first)
- RX powered (FC USB power is sufficient — no battery needed)
- **PROPS OFF** (no motors will spin during this routine, but keep the airframe inert)
- Calibration build flashed and serial monitor connected (see Part 1)

### Run Failsafe Auto-Detection

Type `f` in serial monitor. The firmware command handler lives in `src/calibration_mode.cpp` (case `'f'` → `CALIB_FAILSAFE` → `calibrateFailsafe()` in `lib/Calibration/calibration_hardware.cpp`).

The routine walks through these steps:

1. **(a)** Type `f` and press Enter. You'll see:
   ```
   >>> Failsafe Auto-Detection requested via serial

   +-----------------------------------------------------------+
   |     FAILSAFE AUTO-DETECTION                               |
   +-----------------------------------------------------------+

   STEP 1: First, let's read NORMAL values with transmitter ON.
   ```

2. **(b)** Confirm with `y`. The firmware averages 50 samples (~1 second) of each channel with TX on, then prints:
   ```
   Normal values (TX on):
     CH1: 1500
     CH2: 1500
     CH3: 1000
     CH4: 1500
     CH5: 1000
     CH6: 1500
   ```
   Verify these match your stick positions (centered sticks ~1500us, throttle low ~1000us).

3. **(c)** When prompted (`STEP 2: Now TURN OFF your transmitter.`), **power off the TX completely** and confirm with `y`. The firmware waits 3 seconds for the receiver to enter failsafe mode.

4. **(d)** The firmware averages 50 more samples and prints failsafe values with deltas:
   ```
   Failsafe values (TX off):
     CH1: 1500  (unchanged)
     CH2: 1500  (unchanged)
     CH3: 1000  (unchanged)
     CH4: 1500  (unchanged)
     CH5: 2000  (changed by 1000us)
     CH6: 1500  (unchanged)
   ```

5. **(e)** Copy the printed `#define FAILSAFE_*` block into `include/config.h`:
   ```
   #define FAILSAFE_THROTTLE 1000
   #define FAILSAFE_ROLL 1500
   #define FAILSAFE_PITCH 1500
   #define FAILSAFE_YAW 1500
   #define FAILSAFE_AUX1 2000
   #define FAILSAFE_AUX2 1500
   ```
   Replace the existing Failsafe Values section. Uncomment `#define CALIBRATED_FAILSAFE` in the calibration markers block.

6. **(f)** Rebuild and flash:
   ```bash
   pio run -e teensy40_calibration -t upload
   ```
   (Re-flash the calibration build to bench-verify, then later the live build per [Part 5](#part-5-flash-live-build).)

7. **(g)** **Bench-verify**: with PROPS still OFF, arm the FC, set throttle to ~30%, then power off the transmitter. The motors must drop to a safe state within a few seconds (throttle to FAILSAFE_THROTTLE, AUX1 forcing disarm). If motors keep spinning at the previous throttle, the failsafe values are wrong — re-run calibration.

### Verification

| Channel | Expected on TX-off | Why |
|---------|--------------------|-----|
| FAILSAFE_THROTTLE | ~1000us (or below arm threshold) | Motors must idle/stop |
| FAILSAFE_AUX1 (arm switch) | Position that DISARMS | FC must disarm on signal loss |
| Other channels | Centered (~1500us) is safe | Avoid roll/pitch/yaw kick on loss |

### Common Failure Modes

| Symptom | Cause | Fix |
|---------|-------|-----|
| TX-off values match TX-on values (no change) | Radio-side failsafe not configured on the TX | **UNSAFE — fix on TX before continuing.** On FlySky, set failsafe per-channel in TX menu (System → RX Setup → Failsafe), then re-run `f`. |
| `FAILSAFE_THROTTLE` matches normal throttle | No radio-side failsafe on throttle channel | **UNSAFE.** Set throttle failsafe explicitly to ~1000us on the TX, then re-run `f`. |
| Channels read full-range jitter when TX off | Receiver not entering failsafe (lost bind, or wrong protocol) | Re-bind RX, verify SBUS/DSM/etc. matches `config.h`, retry. |
| Some normal values out of 800-2200us range | Receiver not connected, or TX not bound | Firmware will warn and ask to continue — fix wiring/bind first. |

---

## Part: ESC Endpoint Calibration

**Use this to teach your ESCs the FC's PWM range (1000-2000us).** Most ESCs ship pre-calibrated for a standard range, but if motors arm unevenly or won't reach full throttle, run this once per ESC set.

> Related: [`0_quickstart.md`](0_quickstart.md) **Part 4b: ESC Endpoint Bench Test** for the abbreviated bench-verify step. This section is the full procedure; 4b is the checklist.

### Preconditions

- **PROPS OFF — visually verify each motor. This is non-negotiable.** Motors WILL spin at full throttle during this routine.
- Battery **DISCONNECTED** at the start (you will connect it when prompted)
- ESCs wired to the motor pins defined in `config.h` (`MOTOR_PIN_1..4`), common ground with the FC
- FC has 5V power (USB is fine — the FC does NOT need the flight battery to send PWM)
- Calibration build flashed and serial monitor connected (see Part 1)

### Run ESC Endpoint Calibration

Type `e` in serial monitor. The firmware command handler lives in `src/calibration_mode.cpp` (case `'e'` → `CALIB_ESC` → `calibrateESC()` in `lib/Calibration/calibration_hardware.cpp`).

The routine walks through these steps:

1. **(a)** Type `e` and press Enter. You'll see:
   ```
   >>> ESC Calibration requested via serial

   +-----------------------------------------------------------+
   |     ESC ENDPOINT CALIBRATION                               |
   +-----------------------------------------------------------+

   WARNING: REMOVE ALL PROPELLERS BEFORE CONTINUING!
   Motors WILL spin during this procedure.
   ```
   The firmware asks `Are propellers removed? Type 'y' to confirm.` — **physically look at each motor again** before typing `y`.

2. **(b)** Follow the on-screen tone prompts. The FC walks the ESCs through the standard endpoint sequence:
   - FC sends **MAX throttle (2000us)** to all motors.
   - Firmware prints: `Now connect battery power to ESCs.` — connect the flight battery now.
   - ESCs emit **ascending beeps** confirming max endpoint captured.
   - Confirm with `y`. FC then sends **MIN throttle (1000us)**.
   - ESCs emit **descending beeps** confirming min endpoint captured.
   - Firmware prints `ESC calibration complete!`

3. **(c)** **Verify motors arm cleanly**: disconnect and reconnect battery (or re-arm), set throttle low + arm switch to armed. All motors should sit silent at idle (no rogue spin). Raise throttle just past the arm point — every motor should start spinning at the same throttle value with no stutter.

4. **(d)** **Bench-test linear response** across throttle 0-100% with PROPS OFF:
   - Slowly walk throttle from 0% to 100% over ~10 seconds.
   - All four motors should spool up and down together at matching RPM.
   - There should be no "dead zone" near the bottom and no "saturation step" near the top.

### Verification

| Check | Pass | Fail → Action |
|-------|------|---------------|
| Motors arm at the same throttle | Yes | Re-run `e`, ensure all ESCs got both endpoint signals |
| Idle RPM matches across motors | Yes | Re-run `e`; if persistent, suspect a damaged ESC |
| Full throttle spools all motors evenly | Yes | Re-run `e` |
| No config.h changes after this routine | Correct | ESC endpoints are stored in the ESCs themselves, not in firmware |

Uncomment `#define CALIBRATED_ESC` in `include/config.h` to mark this stage complete for the sequential workflow (`a`).

### Common Failure Modes

| Symptom | Cause | Fix |
|---------|-------|-----|
| Only some motors beep | Loose signal wire, or ESC not powered | Power down, check wiring, retry. |
| ESCs never beep at all | Battery not connected at the prompted step, or wrong ESC protocol | Most ESCs require standard PWM (1000-2000us). DSHOT-only ESCs cannot be calibrated this way. |
| Motors spin at min throttle after calibration | Endpoint set too low | Re-run `e` — ensure the MAX step happened (ascending beeps heard) before continuing. |
| Motors won't reach full speed | MAX endpoint captured too low | Re-run `e`; ensure FC is sending 2000us (`pio device monitor` should not show errors). |
| Any motor moves with props removed and starts catching on the frame | You forgot to remove props | **STOP. Disconnect battery immediately.** Remove props, then restart calibration. |

---

## Part 5: Flash Live Build

After all calibrations are complete:

### Step 1: Update config.h

Paste all calibration values into `include/config.h`.

### Step 2: Build Live Firmware

```bash
pio run -e teensy40
```

### Step 3: Upload Live Firmware

```bash
pio run -e teensy40 -t upload
```

**The live build:**
- Does NOT include calibration code
- Uses hard-coded values from config.h
- Smaller and faster (no debug overhead)

---

## Verification

### Quick Status Check

In calibration build, type `s` for status:

```
=== STATUS ===
CH1: 1500  CH2: 1500  CH3: 1000  CH4: 1500  CH5: 1000  CH6: 1500
Armed: NO
```

### IMU Quality Check

After applying calibration, verify:

| Metric | Good | Acceptable | Re-calibrate |
|--------|------|------------|--------------|
| GyroX/Y/Z (still) | ±0.1°/s | ±0.5°/s | >1.0°/s |
| AccX/Y (level) | ±0.03g | ±0.05g | >0.1g |
| AccZ (level) | 0.95-1.05g | 0.90-1.10g | <0.90 or >1.10g |

---

## Troubleshooting

### Calibration Values Look Wrong

| Problem | Cause | Solution |
|---------|-------|----------|
| Gyro values > 1.0 | Aircraft moved during calibration | Keep perfectly still, retry |
| AccZ not near 1.0 | Surface not level | Use carpenter's level |
| Values jump around | Vibration | More stable surface |

### Serial Commands Not Working

| Problem | Cause | Solution |
|---------|-------|----------|
| No response to commands | Wrong build | Flash calibration build, not live |
| Commands ignored | Calibration in progress | Wait for current calibration to finish |

### CH6 Switch Not Triggering

| Problem | Cause | Solution |
|---------|-------|----------|
| No calibration starts | Throttle too high | Lower throttle below 1050μs |
| No calibration starts | Aircraft armed | Disarm first (CH5 low) |
| No calibration starts | Didn't hold long enough | Hold for 3 full seconds |

---

## Command Reference

### Serial Commands

| Command | Action | Type |
|---------|--------|------|
| `h` | Help (show all commands) | Display |
| `c` | Calibration status (what's done/pending) | Display |
| `s` | Channel status (CH1-6 + Armed) | Display |
| `g` | Show PID gains | Display |
| `p` | Show filter & limits | Display |
| `d` | Dump ALL calibration values (config.h block) | Display |
| `t` | Toggle telemetry (off/IMU/full) | Display |
| `i` | IMU calibration (single-position, offsets only) | Interactive |
| `m` | IMU calibration (6-position, offsets + scale) | Interactive |
| `o` | IMU + Orientation detection | Interactive |
| `r` | Radio calibration (channel mapping) | Interactive |
| `f` | Failsafe auto-detection | Interactive |
| `e` | ESC endpoint calibration | Interactive |
| `n` | Network diagnostics (ESP32 only) | Display |
| `a` | Sequential calibration (guided workflow) | Interactive |
| `g <name> <value>` | Set PID gain (e.g. `g kp_roll 0.25`) | Tuning |
| `p <name> <value>` | Set filter param (e.g. `p b_accel 0.12`) | Tuning |

### calibrate.sh (recommended)

```bash
./tools/calibrate.sh                          # Auto-detect port, launch menu
./tools/calibrate.sh /dev/ttyACM0             # Specific port, launch menu
./tools/calibrate.sh /dev/ttyACM0 imu         # Run IMU calibration directly
./tools/calibrate.sh /dev/ttyACM0 dump        # Dump values directly
./tools/calibrate.sh help                     # Show all CLI commands
```

### CH6 Switch (no serial required)

| CH6 Position | Action (hold 3s) |
|--------------|------------------|
| Low (<1200) | Normal flight (no calibration) |
| Mid (1200-1800) | IMU calibration |
| High (>1800) | IMU + Orientation calibration |

---

## Next Steps

After calibration:

1. **Ground test** — Verify arming, motor direction
2. **Hover test** — First flight with conservative settings
3. **PID tuning** — Optimize flight characteristics

**Ready to fly!**
