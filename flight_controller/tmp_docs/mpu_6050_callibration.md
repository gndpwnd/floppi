# MPU6050 Calibration & Denoising Guide

## 🎯 Overview

This guide shows you how to calibrate and tune your MPU6050 IMU using Serial Monitor and Serial Plotter. The process is built into main.cpp using comment/uncomment debug functions.

---

## 📋 STEP 1: Basic IMU Test

### Enable IMU Debug Output

In `main.cpp`, find the debug section in `loop()` and uncomment:

```cpp
void loop() {
    // ... core flight controller code ...
    
    // ========== DEBUG OUTPUT (UNCOMMENT WHAT YOU WANT TO SEE) ==========
    
    printGyroData();         // ← Uncomment this
    printAccelData();        // ← Uncomment this
    //printRollPitchYaw();   // Keep commented for now
```

### Upload and View

```bash
pio run -e teensy40 -t upload
pio device monitor
```

### Expected Output

```
GyroX:-0.05 GyroY:0.12 GyroZ:-0.03
AccX:-0.01 AccY:0.00 AccZ:1.01
GyroX:-0.04 GyroY:0.11 GyroZ:-0.02
AccX:-0.01 AccY:0.01 AccZ:1.00
```

### What to Look For

**Stationary board (not moving):**
- GyroX/Y/Z should be near 0 (±0.5 deg/s)
- AccX/Y should be near 0 (±0.05 g)
- AccZ should be near 1.0 (gravity)

**If values are drifting:**
- Large gyro drift → Needs better calibration
- Noisy accelerometer → Increase filter coefficient
- Sudden spikes → Vibration or EMI interference

---

## 📊 STEP 2: Serial Plotter Visualization

Serial Plotter is PERFECT for seeing IMU data graphically!

### Setup for Plotter

1. **Uncomment print functions:**
   ```cpp
   printGyroData();      // Shows gyro drift
   printAccelData();     // Shows vibration/noise
   printRollPitchYaw();  // Shows attitude stability
   ```

2. **Open Serial Plotter:**
   ```
   Tools → Serial Plotter
   OR
   Ctrl+Shift+L (Windows/Linux)
   Cmd+Shift+L (Mac)
   ```

3. **Set baud rate:** 115200

### Reading the Plotter

**Gyro Graph (stationary board):**
```
Ideal:   _______________  (flat lines near 0)
Bad:     ~~~~~~~~~~~~~~~  (wavy = drift)
Worse:   ^\/\/\/\/\/\/^  (spiky = noise)
```

**Accelerometer Graph:**
```
AccX: _________ (flat at 0)
AccY: _________ (flat at 0)
AccZ: _________ (flat at 1.0)
```

**Roll/Pitch/Yaw Graph:**
```
roll:  _________ (should stay stable when level)
pitch: _________ (should stay stable when level)
yaw:   /////////  (slowly drifts is OK, no mag sensor)
```

### Rotate Board Test

Move the board slowly:
- Roll graph should go up/down smoothly (no jitter)
- Pitch graph should go up/down smoothly
- Yaw graph may drift (normal without magnetometer)

---

## 🔧 STEP 3: Calibration

### Automatic Calibration (Recommended)

The automatic calibration runs during `setup()`:

```cpp
void setup() {
    // ...
    Serial.println(F("\nCalibrating IMU..."));
    Serial.println(F("Keep board FLAT and STILL!"));
    delay(2000);
    calculate_IMU_error();  // ← This does the calibration
    // ...
}
```

**What it does:**
- Takes 2000 samples (about 2 seconds)
- Calculates average gyro and accel offsets
- Stores in `AccErrorX/Y/Z` and `GyroErrorX/Y/Z`
- Automatically subtracts these in `getIMUdata()`

**Output example:**
```
Calibrating IMU...
Keep board FLAT and STILL!
AccError: X=0.0123 Y=-0.0087 Z=0.0234
GyroError: X=0.4567 Y=-0.2345 Z=0.1234
IMU calibration complete
```

### Manual Calibration (if needed)

If automatic calibration isn't good enough, manually set offsets in `config.h`:

```cpp
// In calculate_IMU_error(), note the printed errors:
// AccError: X=0.0123 Y=-0.0087 Z=0.0234
// GyroError: X=0.4567 Y=-0.2345 Z=0.1234

// Then in config.h, you could add:
#define MANUAL_GYRO_OFFSET_X 0.4567
#define MANUAL_GYRO_OFFSET_Y -0.2345
#define MANUAL_GYRO_OFFSET_Z 0.1234
```

---

## 🎛️ STEP 4: Filter Tuning (Denoising)

The MPU6050 has built-in filtering, plus we add software filters.

### Filter Coefficients (in config.h)

```cpp
// Low-pass filter coefficients (0.0 to 1.0)
#define B_ACCEL 0.14  // Accelerometer filter
#define B_GYRO  0.10  // Gyroscope filter
```

**How it works:**
- 0.0 = No new data (maximum filtering)
- 1.0 = All new data (no filtering)
- 0.1-0.2 = Typical values (good balance)

### Tuning Process

1. **Start with defaults** (B_ACCEL=0.14, B_GYRO=0.10)

2. **Observe in Serial Plotter:**
   ```cpp
   printAccelData();
   printGyroData();
   ```

3. **If too noisy (jittery graphs):**
   - DECREASE B_ACCEL (try 0.10 or 0.08)
   - DECREASE B_GYRO (try 0.08 or 0.05)
   - Lower = more filtering = smoother

4. **If too laggy (slow response):**
   - INCREASE B_ACCEL (try 0.18 or 0.20)
   - INCREASE B_GYRO (try 0.15 or 0.18)
   - Higher = less filtering = faster response

5. **Test by moving board:**
   - Quick rotation should show in graphs within 50-100ms
   - No overshoot or ringing

### Example Tuning Session

**Problem:** Accelerometer graph is jittery
```
Before (B_ACCEL=0.14):
AccX: \/\/\/\/\/  (lots of noise)

Change to B_ACCEL=0.08:
AccX: ~~~~~~~~    (smoother, still responsive)
```

**Problem:** Attitude drifts during hover
```
Before (B_GYRO=0.10):
roll: \_/\_/\_/   (unstable)

Change to B_GYRO=0.06:
roll: ________    (more stable)
```

---

## 🧪 STEP 5: Vibration Testing

Motors cause vibration → bad for IMU!

### Test Procedure

1. **Mount IMU on frame**
2. **Enable debug output:**
   ```cpp
   printAccelData();
   printRollPitchYaw();
   ```

3. **Run motors at different throttles:**
   - No props, motors OFF → baseline
   - Motors at 30% → check vibration
   - Motors at 50% → check vibration
   - Motors at 75% → check vibration

4. **Watch Serial Plotter:**
   - Should see small increase in noise
   - Should NOT see huge spikes
   - Attitude should remain stable

### Vibration Solutions

If you see large spikes:
- ✓ Add soft-mount foam under IMU
- ✓ Balance propellers
- ✓ Check motor bearings
- ✓ Use vibration dampening pads
- ✓ Move IMU further from motors

---

## 📈 STEP 6: Attitude Verification

### Test Pitch and Roll

1. **Enable attitude output:**
   ```cpp
   printRollPitchYaw();
   ```

2. **Tilt board 45°:**
   - Roll 45° right → should show ~45°
   - Pitch 45° forward → should show ~45°
   - Should be within ±2-3° of actual

3. **Return to level:**
   - Should return to 0° within 1 second
   - No oscillation or drift

### Test Yaw (No Magnetometer)

**Expected behavior:**
- Yaw will DRIFT slowly (normal without mag)
- Rate of change should be proportional to rotation
- Don't rely on yaw for navigation

**If using MPU9250:**
- Yaw should hold (magnetometer)
- Needs calibration away from metal/magnets

---

## 🎯 STEP 7: Combined Plotter Test

### Final Verification Setup

```cpp
void loop() {
    // Core flight controller ...
    
    // Enable all IMU debug:
    printGyroData();
    printAccelData();
    printRollPitchYaw();
}
```

### Watch Serial Plotter While:

1. **Board stationary:**
   - All lines flat
   - Gyros at 0
   - Accels at 0,0,1
   - Attitude stable

2. **Slow roll left/right:**
   - Roll angle follows motion smoothly
   - Gyro X shows rate
   - Returns to level accurately

3. **Slow pitch forward/back:**
   - Pitch angle follows motion smoothly
   - Gyro Y shows rate
   - Returns to level accurately

4. **Quick rotations:**
   - Attitude updates quickly (50-100ms)
   - No overshoot
   - Settles accurately

### Success Criteria

✓ Gyros within ±0.5 deg/s when stationary  
✓ Accels within ±0.05g when stationary  
✓ Attitude within ±2° of actual angle  
✓ Smooth graphs (no spikes)  
✓ Quick response (<100ms)  
✓ Stable under vibration  

---

## 🔍 Common Issues & Fixes

| Problem | Symptom | Solution |
|---------|---------|----------|
| **Large gyro drift** | Gyros not at 0 when still | Re-run calibration, board must be PERFECTLY still |
| **Noisy accelerometer** | AccX/Y/Z jumping around | Decrease B_ACCEL (more filtering) |
| **Attitude drift** | Roll/pitch slowly changes | Check calibration, check for vibration |
| **Laggy response** | Slow to follow movement | Increase B_ACCEL and B_GYRO |
| **Random spikes** | Sudden jumps in data | Check I2C wires, add capacitor to IMU power |
| **Incorrect angles** | 45° tilt shows 30° | Check sensor orientation, may need to swap axes |

---

## 📊 Serial Plotter Tips

### Good Plotter Practices

1. **Scale matters:**
   - Gyros: ±500 deg/s range
   - Accels: ±2g range
   - Attitude: ±180° range

2. **Update rate:**
   - Use 10ms update (100Hz) for smooth graphs
   - Too fast → choppy
   - Too slow → miss transients

3. **Clear labels:**
   ```cpp
   Serial.print(F("roll:"));  // Label before value
   Serial.print(roll_IMU);
   ```

4. **Consistent format:**
   - Always print same number of variables
   - Always in same order
   - Plotter auto-scales to data

### Example Plotter Output Interpretation

```
Perfect IMU (stationary):
GyroX: ━━━━━━━━━━━  (flat at 0)
GyroY: ━━━━━━━━━━━  (flat at 0)
GyroZ: ━━━━━━━━━━━  (flat at 0)
roll:  ━━━━━━━━━━━  (flat at 0)
pitch: ━━━━━━━━━━━  (flat at 0)

Noisy IMU (needs tuning):
GyroX: ～～～～～～～  (wavy, decrease B_GYRO)
AccX:  ∧∨∧∨∧∨∧∨∧  (spiky, decrease B_ACCEL)
roll:  ／＼／＼／＼  (unstable, check vibration)
```

---

## 🎓 Advanced: Understanding the Data

### Gyroscope

**What it measures:** Angular velocity (deg/s)
**Range:** ±250, ±500, ±1000, or ±2000 deg/s
**Noise:** ~0.1-0.5 deg/s typical
**Drift:** Integrating gyro = accumulated error over time

### Accelerometer

**What it measures:** Linear acceleration + gravity (g)
**Range:** ±2g, ±4g, ±8g, or ±16g
**Noise:** ~0.01-0.05g typical
**Limitations:** Can't distinguish tilt from acceleration

### Madgwick Filter

**What it does:** Fuses gyro + accel → accurate attitude
**Key parameter:** MADGWICK_BETA (in config.h)
- Higher = trusts accel more (fights drift)
- Lower = trusts gyro more (less noise)
- Default: 0.04 (good balance)

**How to tune MADGWICK_BETA:**
```cpp
// In config.h:
#define MADGWICK_BETA 0.04  // Default

// If attitude drifts during flight:
#define MADGWICK_BETA 0.06  // Trusts accel more

// If attitude is jittery:
#define MADGWICK_BETA 0.02  // Trusts gyro more
```

---

## ✅ Final Checklist

Before flight:
- [ ] IMU calibrated (board flat and still for 2 seconds)
- [ ] Gyros near 0 when stationary
- [ ] Accelerometer shows 0,0,1 when level
- [ ] Attitude within ±2° when rotated
- [ ] No drift over 10 seconds
- [ ] Stable under motor vibration
- [ ] Quick response (<100ms lag)

---

**Next:** Once IMU is tuned, move on to receiver testing and PID tuning!