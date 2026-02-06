# Calibration Guide

Complete guide for calibrating your flight controller firmware.

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

### Step 3: Open Serial Monitor

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
  r - Radio calibration
  i - IMU calibration
  o - IMU + Orientation
  s - Status
  h - Help

CH6 switch (hold 3s):
  Mid:  IMU cal
  High: IMU + Orientation
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

| Command | Action |
|---------|--------|
| `r` | Radio calibration (channel mapping) |
| `i` | IMU calibration (single-position, offsets only) |
| `m` | IMU calibration (6-position, offsets + scale factors) |
| `o` | IMU + Orientation detection |
| `s` | Status (show current channel values) |
| `h` | Help (show command menu) |

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
