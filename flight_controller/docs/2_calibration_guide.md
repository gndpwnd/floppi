# 🎯 CALIBRATION GUIDE

Complete guide for calibrating your flight controller - both automatic and manual methods.

---

## 📋 Calibration Overview

### What Needs Calibration?

1. **MPU6050 IMU** - Zero out sensor biases
2. **Radio channels** - Map transmitter controls to flight controller
3. **PID gains** - Tune flight characteristics (separate guide)

### Calibration Methods

| Component | Auto Method | Manual Method | Time Required |
|-----------|-------------|---------------|---------------|
| **IMU Offsets** | ✅ CH6 switch trigger | ✅ Serial Monitor | 30 seconds |
| **IMU Orientation** | ✅ Position detection | ❌ Manual code | 2 minutes |
| **Radio Mapping** | ⚠️ Under development | ✅ Testing sticks | 5 minutes |

**Recommendation:** Use automatic methods when available - faster and less error-prone.

---

## 🤖 Part 1: Automatic IMU Calibration

### Method A: Startup Calibration (Easiest)

**When to use:** Initial setup, after IMU replacement, after crashes.

**Procedure:**

1. Place aircraft **flat and perfectly level** on desk/table
2. Verify **no vibration** (turn off fans, AC, etc.)
3. **Hold CH6 switch in HIGH position** (top position on 3-pos switch)
4. **Power on Teensy** via USB
5. **Wait 5 seconds** - you'll see LED blink pattern
6. **Release CH6 switch** when LED returns to slow blink
7. **Open Serial Monitor** (115200 baud)

**Expected output:**
```
Calibrating IMU...
Keep board FLAT and STILL!
..................
AccError: X=0.012345 Y=-0.008765 Z=0.023456
GyroError: X=0.456789 Y=-0.234567 Z=0.123456
IMU calibration complete
```

**Next steps:**

8. **Copy the printed values**
9. **Open `include/config.h`**
10. **Paste into IMU calibration section:**

```cpp
//=============================================================================
// IMU Calibration Values
//=============================================================================
#define IMU_ACC_ERROR_X 0.012345
#define IMU_ACC_ERROR_Y -0.008765
#define IMU_ACC_ERROR_Z 0.023456
#define IMU_GYRO_ERROR_X 0.456789
#define IMU_GYRO_ERROR_Y -0.234567
#define IMU_GYRO_ERROR_Z 0.123456
```

11. **Save config.h**
12. **Re-upload code:** `pio run -e teensy40 -t upload`

---

### Method B: In-Flight Calibration Trigger

**When to use:** Field recalibration, flying at different locations, temperature changes.

**Procedure:**

1. **Land and disarm** aircraft
2. Place aircraft **flat and level** on ground
3. **Throttle stick to minimum**
4. **CH5 switch to disarm position** (low)
5. **Hold CH6 in MID position** (middle of 3-pos switch) for **3 seconds**
6. **LED blinks rapidly** - calibration in progress
7. **Wait 5 seconds** - LED returns to normal
8. **Calibration complete!** (values saved in memory)

**⚠️ Important:** Values are stored in memory only! They're lost on power cycle. For permanent storage, follow Method A and update config.h.

---

### Method C: Attitude Filter Warm-Up

**When to use:** After code upload, when attitude seems inaccurate.

**Procedure:**

1. Aircraft flat and level
2. **Throttle minimum, CH5 disarm**
3. **Hold CH6 in HIGH position** for 3 seconds
4. **Wait 10 seconds** - Madgwick filter warms up
5. Done! Attitude should be more stable

---

### Calibration Quality Check

After auto-calibration, verify results:

**In `main.cpp`, uncomment:**
```cpp
printGyroData();
printAccelData();
printRollPitchYaw();
```

**Upload and open Serial Monitor:**

```
GyroX:0.02 GyroY:-0.05 GyroZ:0.03     ← Should be near 0
AccX:0.00 AccY:0.01 AccZ:1.00         ← AccZ should be ~1.0
roll:0.5 pitch:-0.3 yaw:12.4          ← Roll/pitch near 0
```

**Quality criteria:**

| Metric | Good | Acceptable | Bad (redo calibration) |
|--------|------|------------|------------------------|
| GyroX/Y/Z | ±0.1°/s | ±0.5°/s | >1.0°/s |
| AccX/Y | ±0.03g | ±0.05g | >0.1g |
| AccZ | 0.95-1.05g | 0.90-1.10g | <0.90 or >1.10g |
| Roll/Pitch | ±1° | ±3° | >5° |

---

## 📝 Part 2: Manual IMU Calibration

### When to Use Manual Method

- Auto-calibration not working
- Debugging calibration issues
- Understanding what auto-calibration does
- Using Serial Plotter for visualization

---

### Step 1: Record Calibration Data

**Code setup:**

In `main.cpp`, uncomment:
```cpp
printGyroData();
printAccelData();
```

**Upload and run:**

```bash
pio run -e teensy40 -t upload
pio device monitor
```

**Procedure:**

1. Place aircraft **flat and level**
2. **Do not move** for 30 seconds
3. **Observe Serial Monitor output:**

```
GyroX:0.45 GyroY:-0.23 GyroZ:0.12
AccX:0.02 AccY:-0.01 AccZ:1.02
GyroX:0.46 GyroY:-0.24 GyroZ:0.11
AccX:0.02 AccY:-0.01 AccZ:1.03
... (repeat for 30 seconds)
```

4. **Copy 10-20 samples** to spreadsheet or text file

---

### Step 2: Calculate Averages

**Manual calculation:**

```
Average GyroX = Sum of all GyroX / Number of samples
Average GyroY = Sum of all GyroY / Number of samples
Average GyroZ = Sum of all GyroZ / Number of samples
Average AccX = Sum of all AccX / Number of samples
Average AccY = Sum of all AccY / Number of samples
Average AccZ = Sum of all AccZ / Number of samples - 1.0
```

**Example:**
```
Samples:
GyroX: 0.45, 0.46, 0.44, 0.47, 0.45
Average GyroX = (0.45+0.46+0.44+0.47+0.45)/5 = 0.454
```

**Or use Serial Plotter method:**

1. Open Tools → Serial Plotter
2. Let run for 30 seconds
3. Visually estimate average (centerline of graph)

---

### Step 3: Apply Calibration

**Update `config.h`:**

```cpp
#define IMU_ACC_ERROR_X [your calculated AccX average]
#define IMU_ACC_ERROR_Y [your calculated AccY average]
#define IMU_ACC_ERROR_Z [your calculated AccZ average - 1.0]
#define IMU_GYRO_ERROR_X [your calculated GyroX average]
#define IMU_GYRO_ERROR_Y [your calculated GyroY average]
#define IMU_GYRO_ERROR_Z [your calculated GyroZ average]
```

**Re-upload and verify** - values should now be close to ideal.

---

## 📡 Part 3: Radio Channel Calibration

### Automatic Radio Calibration (Future Feature)

**⚠️ Under development** - will auto-detect which stick controls which channel.

**When available:**
1. Uncomment `#define RUN_RADIO_CALIBRATION` in config.h
2. Upload and follow prompts
3. Copy generated code to config.h

---

### Manual Radio Testing (Current Method)

**Step 1: Verify Channel Mapping**

In `main.cpp`, uncomment:
```cpp
printRadioData();
```

Upload and open Serial Monitor.

**Expected output (sticks centered):**
```
CH1:1500 CH2:1500 CH3:1000 CH4:1500 CH5:1000 CH6:1000
```

**Test each control:**

| Control | Expected Change | Default Channel |
|---------|-----------------|-----------------|
| Right stick LEFT | CH1 decreases to ~1000 | Roll |
| Right stick RIGHT | CH1 increases to ~2000 | Roll |
| Right stick UP | CH2 increases to ~2000 | Pitch |
| Right stick DOWN | CH2 decreases to ~1000 | Pitch |
| Left stick UP | CH3 increases to ~2000 | Throttle |
| Left stick DOWN | CH3 decreases to ~1000 | Throttle |
| Left stick LEFT | CH4 decreases to ~1000 | Yaw |
| Left stick RIGHT | CH4 increases to ~2000 | Yaw |
| CH5 switch | Toggles 1000 ↔ 2000 | Arm/Disarm |
| CH6 switch | Changes 1000/1500/2000 | Flight mode |

---

### Step 2: Update Channel Mapping (if needed)

**If your transmitter has different mapping:**

**Example: Throttle is on CH2 instead of CH3**

Update `config.h`:
```cpp
#define THROTTLE_CHANNEL 2  // Changed from 3
#define PITCH_CHANNEL 3     // Changed from 2
```

**Verify** after upload - controls should work correctly.

---

### Step 3: Check Calibration Range

**All channels should span 1000-2000μs:**

| Check | Good | Needs Adjustment |
|-------|------|------------------|
| Min value | ~1000μs | <950 or >1050 |
| Max value | ~2000μs | <1950 or >2050 |
| Center | ~1500μs | <1450 or >1550 |

**If out of range, adjust transmitter EPA (End Point Adjustment):**

1. TX Menu → Function → EPA/Travel Adj
2. Select channel
3. Adjust travel: 100% = 1000μs, -100% = 2000μs range
4. Fine-tune until 1000-2000μs range achieved

---

## 🧮 Part 4: Filter Tuning

### What Are Filters?

**Filters smooth out sensor noise but add lag.**

**Trade-off:**
- **More filtering** → Smoother data, slower response
- **Less filtering** → Faster response, noisier data

---

### Filter Coefficients (config.h)

```cpp
#define B_ACCEL 0.14   // Accelerometer filter (0.0-1.0)
#define B_GYRO  0.10   // Gyroscope filter (0.0-1.0)
#define MADGWICK_BETA 0.04  // Attitude filter (0.02-0.08)
```

**How it works:**
- `B_ACCEL = 0.14` means 14% new data, 86% old data
- Lower = more filtering = smoother but slower
- Higher = less filtering = faster but noisier

---

### When to Tune Filters

**Signs you need to adjust:**

| Symptom | Problem | Solution |
|---------|---------|----------|
| Attitude jitters/twitches | Accel too noisy | Decrease B_ACCEL (try 0.10) |
| Slow to level out | Accel over-filtered | Increase B_ACCEL (try 0.18) |
| High-frequency oscillation | Gyro too noisy | Decrease B_GYRO (try 0.08) |
| Sluggish PID response | Gyro over-filtered | Increase B_GYRO (try 0.15) |
| Attitude drifts in flight | Beta too low | Increase MADGWICK_BETA (try 0.06) |
| Attitude wobbles slowly | Beta too high | Decrease MADGWICK_BETA (try 0.02) |

---

### Filter Tuning Procedure

**Using Serial Plotter:**

1. Uncomment in `main.cpp`:
   ```cpp
   printGyroData();
   printAccelData();
   printRollPitchYaw();
   ```

2. Upload and open Serial Plotter

3. **Observe graphs:**
   - **Ideal:** Smooth lines, minimal noise, quick response to movement
   - **Too much noise:** Jagged lines, spikes
   - **Over-filtered:** Smooth but lags behind actual movement

4. **Adjust config.h values incrementally (±0.02 at a time)**

5. **Re-upload and test** until satisfied

---

## 🎚️ Part 5: IMU Orientation Detection (Advanced)

### When Is This Needed?

**If your IMU is mounted at an angle or rotated:**
- Upside down
- 90° rotated
- 45° tilted
- Non-standard orientation

**Signs of incorrect orientation:**
- Roll and pitch swapped
- Incorrect direction (tilt right, aircraft thinks left)
- Unstable flight despite good calibration

---

### Automatic Orientation Detection

**⚠️ Feature under development**

When available:
1. Uncomment `#define RUN_IMU_ORIENTATION` in config.h
2. Upload code
3. Follow prompts to position aircraft:
   - Nose up (vertical)
   - Right side up (rolled 90°)
   - Top up (normal level)
4. System auto-detects which IMU axis = which aircraft axis
5. Generates axis transformation code
6. Paste code into `getIMUdata()` function

---

### Manual Orientation Correction

**If IMU is rotated 90° clockwise:**

In `main.cpp`, add to `getIMUdata()` function after filtering:

```cpp
// Swap and invert axes for 90° CW rotation
float AccX_temp = AccY;
float AccY_temp = -AccX;
float AccZ_temp = AccZ;

AccX = AccX_temp;
AccY = AccY_temp;
AccZ = AccZ_temp;

float GyroX_temp = GyroY;
float GyroY_temp = -GyroX;
float GyroZ_temp = GyroZ;

GyroX = GyroX_temp;
GyroY = GyroY_temp;
GyroZ = GyroZ_temp;
```

**For other orientations, use this mapping:**

| IMU Mounting | AccX_aircraft | AccY_aircraft | AccZ_aircraft |
|--------------|---------------|---------------|---------------|
| Normal (default) | AccX | AccY | AccZ |
| 90° CW | AccY | -AccX | AccZ |
| 180° rotation | -AccX | -AccY | AccZ |
| 90° CCW | -AccY | AccX | AccZ |
| Upside down | -AccX | AccY | -AccZ |

---

## ✅ Post-Calibration Verification

### Complete System Test

After all calibrations complete:

1. **IMU Test:**
   ```cpp
   printGyroData();
   printAccelData();
   printRollPitchYaw();
   ```
   - Gyros near 0 when still
   - AccZ near 1.0 when level
   - Roll/pitch accurate when tilted

2. **Radio Test:**
   ```cpp
   printRadioData();
   ```
   - All channels 1000-2000μs range
   - Correct control mapping
   - Smooth stick response

3. **Integration Test:**
   ```cpp
   printRadioData();
   printRollPitchYaw();
   printPIDoutput();
   ```
   - Move sticks → PID outputs change
   - Tilt aircraft → PID responds
   - No lag or jumps

4. **Arming Test:**
   - Throttle low + CH5 low = Armed
   - CH5 high = Disarmed
   - LED indicates state correctly

**✅ All tests passing? → Ready for PID tuning!**

---

## 💾 Calibration Data Backup

### Saving Your Calibration

**After successful calibration:**

1. **Create backup file:**
   ```bash
   cp include/config.h config_backup_$(date +%Y%m%d).h
   ```

2. **Document in notebook:**
   ```
   Date: 2024-12-16
   Aircraft: Quadcopter #1
   IMU: MPU6050 S/N: 12345
   
   Calibration values:
   AccErrorX: 0.012345
   AccErrorY: -0.008765
   AccErrorZ: 0.023456
   GyroErrorX: 0.456789
   GyroErrorY: -0.234567
   GyroErrorZ: 0.123456
   
   Notes: Calibrated indoors, 22°C
   ```

3. **Keep multiple backups:**
   - Indoor calibration (stable temp)
   - Outdoor calibration (field conditions)
   - Cold weather calibration (<10°C)
   - Hot weather calibration (>30°C)

---

### When to Re-Calibrate

**Mandatory re-calibration:**
- After IMU replacement
- After hard crash
- After firmware update
- After PID gains changed significantly

**Optional re-calibration:**
- Flying at very different temperature (>20°C change)
- Flying at very different altitude (>1000m change)
- Notice drift or instability
- Every 6 months (preventive maintenance)

---

## ❓ Calibration Troubleshooting

### IMU Won't Calibrate

**Problem:** Auto-calibration fails or gives bad results

**Causes & Solutions:**

| Symptom | Cause | Solution |
|---------|-------|----------|
| Values jump around | Vibration | Use more stable surface |
| AccZ ≠ 1.0 | Surface not level | Use carpenter's level |
| Large gyro offsets | Aircraft moved | Keep perfectly still |
| Calibration hangs | Code issue | Check Serial Monitor for errors |

---

### Radio Not Responding

**Problem:** Channels stuck at 1500μs or not changing

**Causes & Solutions:**

| Symptom | Cause | Solution |
|---------|-------|----------|
| All channels 1500 | Not bound | Bind receiver to TX |
| No signal | Wrong pin | Check Pin 21 (RX5) |
| Inverted values | Wrong protocol | Verify TX is SBUS mode |
| Erratic values | Loose wire | Check SBUS wiring |

---

### Filters Not Helping

**Problem:** Still noisy despite filter tuning

**Possible causes:**
- Hardware vibration (mount IMU on foam)
- EMI interference (move away from motors/ESCs)
- Defective IMU (try another unit)
- Power supply noise (add capacitors)

---

## 📚 Next Steps

**After calibration:**

1. **PID Tuning** - See [PID_TUNING_GUIDE.md](./PID_TUNING_GUIDE.md)
2. **First Flight** - See [QUICKSTART.md](./QUICKSTART.md) Part 5
3. **Advanced Features** - Enable additional sensors, GPS, etc.

**Calibration complete! ✅ Time to fly! 🚁**