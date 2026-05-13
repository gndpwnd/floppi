# MPU6050 9-DOF Sensor Fusion Research Compilation

**Complete research on implementing absolute orientation with MPU6050 + magnetometer**  
**Alternative to BNO085 - lower cost, requires firmware implementation**

---

## Executive Summary

To replicate BNO085's absolute orientation with an open-system approach:

1. **Hardware:** MPU6050 (6-DOF: accel + gyro) + QMC5883L magnetometer (3-DOF) = 9-DOF total
2. **Algorithm:** Madgwick quaternion filter (best balance of accuracy, speed, simplicity)
3. **Cost:** ~$5-8 total (vs. $30-50 for BNO085)
4. **Development:** ~20-36 hours integration + testing (vs. plug-and-play with BNO085)
5. **Accuracy:** ±10-15° typical (vs. ±5° for BNO085)
6. **Yaw Stability:** Requires proper magnetometer calibration (hard iron + soft iron)

---

## Part 1: Hardware Overview

### MPU6050 Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Gyro Range** | ±250–2000°/s | 4 programmable ranges |
| **Accel Range** | ±2g–16g | 4 programmable ranges |
| **Gyro Drift** | ~30°/hour uncorrected | ~0.5°/min with bias correction |
| **Output Format** | 16-bit 2's complement | I2C register access |
| **I2C Address** | 0x68 or 0x69 | AD0 pin selectable |
| **Sample Rate** | Up to 8 kHz raw | Typically 200-512 Hz for fusion |
| **Power** | 4-5 mA operating | 1.4 mA sleep mode |
| **Temperature Sensitivity** | Drifts with T | Requires continuous bias correction |
| **Cost** | ~$3-5 | GY-521 module widely available |

### Magnetometer Options

| Sensor | I2C Address | Accuracy | Cost | Notes |
|--------|-------------|----------|------|-------|
| **HMC5883L** | 0x1E | ±0.3 mG | $3-4 | Original choice, well-documented |
| **QMC5883L** | 0x0D | ±0.3 mG | $2-3 | Modern replacement, lower cost |
| **LIS3MDL** | 0x1C/0x1D | ±0.4 mG | $4-5 | High quality, I2C or SPI |

**Recommendation:** QMC5883L (best value, modern, good library support)

### Wiring Diagram

```
Arduino Mega
├── I2C Bus (SDA=Pin 20, SCL=Pin 21)
│   ├── MPU6050 (Address 0x68)
│   │   ├── SDA → Pin 20
│   │   ├── SCL → Pin 21
│   │   ├── GND → GND
│   │   └── 3.3V → 3.3V
│   │
│   └── QMC5883L (Address 0x0D)
│       ├── SDA → Pin 20
│       ├── SCL → Pin 21
│       ├── GND → GND
│       └── 3.3V → 3.3V
│
└── [Optional] LED on Pin 13 for status
```

Both sensors share the same I2C bus (standard practice).

---

## Part 2: Algorithm - Madgwick Quaternion Filter

### What It Does

The Madgwick filter combines gyroscope (fast, but drifts), accelerometer (slow, but stable gravity reference), and magnetometer (stable heading reference) into a single orientation estimate as a normalized quaternion `[w, x, y, z]`.

**Output:** Quaternion representing complete 3D rotation without gimbal lock

### How It Works (Simplified)

1. **Gyroscope integration** (fast loop at 200-500 Hz)
   - Integrate angular velocity to predict next quaternion
   - Fast but drifts over time

2. **Error calculation** (from accel + mag measurements)
   - Measure gravity direction from accelerometer
   - Measure magnetic north from magnetometer
   - Calculate error between prediction and measurements

3. **Correction** (gradient descent)
   - Adjust quaternion to minimize error
   - Gain controlled by `beta` parameter

4. **Gyro bias estimation** (slow feedback)
   - Estimate and subtract gyroscope DC bias
   - Prevents unbounded yaw drift

### Key Parameters

**Beta (β) - Main tuning parameter**
- Controls trust in sensors vs. gyroscope
- Empirical rule: `(beta * sampleFreq) ≈ 50-60`
- For 100 Hz sampling: beta ≈ 0.033 (IMU) to 0.041 (MARG)
- Higher β = faster response, more noise
- Lower β = smoother output, slower drift correction

**Zeta (ζ) - Gyro drift compensation** (optional)
- Typical range: 0.001-0.1
- Usually left at default (0.0) for most applications

**Sample rate** - Critical for beta tuning
- Measure actual rate with `micros()`
- Must be consistent (use interrupt-based timing)
- 100 Hz typical for robotics/drone applications

### Madgwick vs. Alternatives

| Algorithm | Accuracy | CPU Cost | Complexity | 9-DOF Support | Best For |
|-----------|----------|----------|------------|--------------|----------|
| **Madgwick** | ~0.4° RMSE | Low (~277 ops) | Medium | Excellent | **General purpose, recommended** |
| **Mahony** | ~0.4° RMSE | Lowest (~20% less) | Medium | Good | Extreme resource constraints |
| **Complementary Filter** | ~1-2° | Lowest | Simplest | Okay | 6-DOF only (no magnetometer) |
| **Extended Kalman** | ~0.4° RMSE | High (matrix ops) | High | Excellent | Maximum accuracy needed, lots of RAM |

**Verdict:** Madgwick best balance for Arduino with 9-DOF sensors

### Initialization & Tuning

**First-time calibration:**
```cpp
// Initialize with identity quaternion (no rotation)
float q[4] = {1.0, 0.0, 0.0, 0.0};

// Or better: compute from first sensor reading
// accel tells you pitch/roll, mag tells you yaw
float accelRoll = atan2(ay, sqrt(ax*ax + az*az));
float accelPitch = atan2(-ax, sqrt(ay*ay + az*az));
float magHeading = atan2(my, mx);
// Convert Euler → quaternion
quatFromEuler(q, accelRoll, accelPitch, magHeading);
```

**Parameter tuning process:**
1. Start with beta = 0.033 (safe default)
2. Run in actual motion environment (yaw drifts too much? increase beta to 0.05)
3. Check for noise/oscillation (too jittery? decrease beta to 0.02)
4. Final tuning: observe 1-hour operation (yaw drift < 5°?)

---

## Part 3: Magnetometer Calibration

### Hard Iron vs. Soft Iron

**Hard Iron** - Permanent magnet bias
- Constant additive offset to all magnetometer readings
- Caused by: permanent magnets, DC currents, ferromagnetic permanent state
- Effect: Shifts measurement data away from origin
- Correction: Subtract fixed offset vector

**Soft Iron** - Ferromagnetic distortion
- Multiplicative (scale) and cross-coupling distortion
- Caused by: steel brackets, ferromagnetic PCB materials
- Effect: Stretches/rotates measurement ellipsoid
- Correction: Apply 3×3 transformation matrix

### Quantified Errors Without Calibration

- **Uncalibrated heading error: 40-150+ degrees** (typical range)
- One case study: 150° error → 2.5° after calibration
- Error depends on ferromagnetic environment (indoor with metal: 90°+; outdoor: 10-30°)

### Calibration Procedure

**Physical phase:**
1. Rotate sensor through all orientations (figure-8 pattern, all 3 axes)
2. Collect 100+ magnetometer readings
3. Record min/max values for each axis

**Calculation phase:**
```
For each axis:
  offset[axis] = (max[axis] + min[axis]) / 2.0
  range[axis] = max[axis] - min[axis]
  avgRange = (range[X] + range[Y] + range[Z]) / 3.0
  scale[axis] = avgRange / range[axis]
```

**Runtime application:**
```cpp
calX = (rawX - offset[X]) * scale[X];
calY = (rawY - offset[Y]) * scale[Y];
calZ = (rawZ - offset[Z]) * scale[Z];
```

### Advanced Calibration (Ellipsoid Fitting)

For <2.5° heading accuracy, use ellipsoid fitting:
1. Collect 100-200 calibration samples across all orientations
2. Use Python library (see tools section)
3. Get 9-parameter transformation matrix (3×3 rotation + 3-element offset)
4. Apply at runtime: `cal_vector = matrix^-1 × (raw_vector - offset)`

### Tools for Calibration

**Python-based (recommended):**
- [lundeen06/magnetometer-calibration-tool](https://github.com/lundeen06/magnetometer-calibration-tool) - Ellipsoid fitting with visualization
- [italocjs/magnetometer_calibration](https://github.com/italocjs/magnetometer_calibration) - Supports Arduino data collection
- Adafruit SensorLab - Interactive Jupyter notebook

**Arduino-based:**
- [mprograms/QMC5883LCompass](https://github.com/mprograms/QMC5883LCompass) - Includes calibration sketch
- Data collection: Log raw values to file, process offline with Python

### Recalibration Requirements

**Must recalibrate when:**
- Moving to new physical location (new hard/soft iron environment)
- Temperature change >30°C from calibration
- Adding ferromagnetic equipment nearby
- Yaw drift >10° from expected heading

**Frequency:** Usually once per location; some environments need quarterly checks

---

## Part 4: Reference Implementations

### Arduino Libraries

**Primary recommendation:**
```cpp
// Arduino IDE Library: MadgwickAHRS (official)
#include "MadgwickAHRS.h"
Madgwick filter;

// Setup
filter.begin(100); // 100 Hz sample rate

// In loop, after reading sensors
filter.update(ax, ay, az, gx, gy, gz, mx, my, mz);

// Get results
float roll = filter.getRoll();
float pitch = filter.getPitch();
float yaw = filter.getYaw();

// Or get quaternion directly
float q[4];
filter.getQuaternion(q);
```

**Alternative libraries:**
- [jrowberg/i2cdevlib](https://github.com/jrowberg/i2cdevlib) - Comprehensive I2C device drivers + examples
- [kriswiner/MPU9250](https://github.com/kriswiner/MPU9250) - Production-grade with tuned parameters
- [Mayitzin/ahrs](https://github.com/Mayitzin/ahrs) - Python + C++ implementations

### Reference Code Structure

```cpp
#include <Wire.h>
#include "MadgwickAHRS.h"
#include "MPU6050.h"
#include "QMC5883L.h"

MPU6050 mpu;
QMC5883L mag;
Madgwick filter;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Initialize sensors
  mpu.initialize();
  mag.setCalibrationOffsets(-540, -65, 165); // From calibration
  mag.setCalibrationScales(0.97, 1.02, 1.02);
  
  // Set filter sample rate
  filter.begin(100); // 100 Hz
}

void loop() {
  // Read all sensors
  float ax, ay, az, gx, gy, gz, mx, my, mz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  mag.read(&mx, &my, &mz);
  
  // Convert to proper units (accel: g, gyro: deg/s, mag: Gauss)
  ax /= 16384.0; ay /= 16384.0; az /= 16384.0; // ±2g
  gx /= 131.0; gy /= 131.0; gz /= 131.0; // ±250°/s
  mx *= 0.00304882; my *= 0.00304882; mz *= 0.00304882; // To mG
  
  // Update filter
  filter.update(ax, ay, az, gx, gy, gz, mx, my, mz);
  
  // Get output
  float yaw = filter.getYaw();
  float pitch = filter.getPitch();
  float roll = filter.getRoll();
  
  // Output as quaternion (for compatibility with BNO085 format)
  float q[4];
  filter.getQuaternion(q);
  
  // Send JSON like BNO085
  Serial.print("{\"quaternion\":{\"w\":");
  Serial.print(q[0], 6);
  Serial.print(",\"x\":");
  Serial.print(q[1], 6);
  // ... etc
}
```

---

## Part 5: Implementation Roadmap

### Phase 1: Hardware Setup (2 hours)
- [ ] Wire MPU6050 and QMC5883L to I2C bus
- [ ] Verify I2C scanner detects both at 0x68 and 0x0D
- [ ] Test raw sensor readings (print serial output)

### Phase 2: Magnetometer Calibration (4 hours)
- [ ] Collect raw magnetometer data (rotate in all directions)
- [ ] Run Python ellipsoid fitting tool
- [ ] Extract calibration offsets and scales
- [ ] Verify heading accuracy (<5° error)

### Phase 3: Sensor Fusion (8 hours)
- [ ] Install MadgwickAHRS library
- [ ] Implement basic 9-DOF fusion
- [ ] Tune beta parameter for your environment
- [ ] Compare quaternion output vs. BNO085 (if available)

### Phase 4: Integration (6 hours)
- [ ] Create `MPU6050_Fusion` class extending `OrientationSensor`
- [ ] Match BNO085 JSON output format
- [ ] Add calibration persistence (EEPROM)
- [ ] Implement health checks and status monitoring

### Phase 5: Testing & Validation (8 hours)
- [ ] Compare with BNO085 reference
- [ ] Long-duration test (4+ hours)
- [ ] Yaw drift measurement
- [ ] Temperature stability test
- [ ] Document accuracy limits

---

## Part 6: Performance Expectations

### Accuracy

| Metric | BNO085 | MPU6050+Mag | Notes |
|--------|--------|-------------|-------|
| **Pitch/Roll** | ±2-3° | ±5-10° | Limited by accel measurement noise |
| **Yaw Heading** | ±3-5° | ±10-15° | Depends on mag calibration quality |
| **Drift Rate** | <3°/hour | ~2-5°/hour | MPU gyro bias correction helps |
| **Latency** | <100ms | ~10-50ms | Software filtering adds delay |

### Computational Cost

- **Madgwick update:** ~200 µs per cycle (on ATmega328)
- **CPU usage:** ~3-5% on 16 MHz Arduino (plenty of headroom)
- **Memory:** ~100 bytes RAM, ~3 KB program space
- **Feasible on:** Any Arduino (Uno, Mega, Nano)

### Power Consumption

- **MPU6050:** 4.5 mA operating
- **QMC5883L:** ~5 mA during measurement
- **Total system:** ~10 mA (vs. BNO085 ~10 mA)
- **Similar power envelope** to BNO085

---

## Part 7: Cost-Benefit Analysis

### Cost Breakdown

| Component | Unit Cost | Qty | Total |
|-----------|-----------|-----|-------|
| MPU6050 module | $4 | 1 | $4 |
| QMC5883L module | $3 | 1 | $3 |
| Wiring/connectors | $1 | - | $1 |
| **Total Hardware** | - | - | **$8** |

**BNO085:** ~$35-50

**Savings:** 85-90% lower hardware cost

### Time Investment

| Phase | Hours | Comments |
|-------|-------|----------|
| Hardware setup | 2 | Soldering + I2C verification |
| Calibration | 4 | Collection + processing |
| Fusion implementation | 8 | Code + library setup |
| Integration | 6 | Matching BNO085 interface |
| Testing | 8 | Validation + tuning |
| **Total** | **28** | One developer, ~1 week part-time |

**BNO085:** Zero development time (plug and play)

### When to Choose MPU6050+Mag vs. BNO085

**Choose MPU6050+Mag when:**
- ✅ Cost critical (consumer products, many units)
- ✅ Have 1-2 weeks development time
- ✅ Need ±10-15° accuracy (sufficient)
- ✅ Want learning experience (understand sensor fusion)
- ✅ Need customization (different algorithms, debug visibility)

**Choose BNO085 when:**
- ✅ Need ±5° accuracy (demanding applications)
- ✅ Time-critical (launch next week)
- ✅ One-off project (cost doesn't matter much)
- ✅ Want proven stability (firmware-based reliability)
- ✅ Need minimal support burden (plug-and-play)

---

## Part 8: Troubleshooting Guide

### Problem: Yaw drifts 20-30° per hour

**Causes:**
1. Magnetometer not calibrated → check hard/soft iron calibration
2. Beta too high → reduce to 0.02-0.03
3. Gyro bias not converging → check for vibration, motion

**Solution:**
```cpp
// Force re-calibration
mag.calibrate(); // Run your calibration routine
// Then re-tune beta
```

### Problem: Output oscillates ±10° constantly

**Causes:**
1. Beta too high (trusting measurements too much)
2. Magnetometer noise (bad calibration)
3. Accelerometer seeing vibration

**Solution:**
```cpp
// Reduce beta
filter.setBeta(0.02); // From 0.04

// Check magnetometer readings are smooth
// If jittery: improve shielding, recalibrate
```

### Problem: Cannot maintain yaw lock (jumps around)

**Causes:**
1. Magnetometer completely uncalibrated
2. Large ferromagnetic object nearby
3. I2C communication errors

**Solution:**
```cpp
// Check calibration data
Serial.print("Calibration offsets: ");
Serial.print(offsetX); Serial.print(", ");
Serial.print(offsetY); Serial.print(", ");
Serial.println(offsetZ);

// If all zeros or very small: recalibrate

// Check raw magnitude consistency
float mag = sqrt(mx*mx + my*my + mz*mz);
// Should be ~48 µT; if wildly varying: environment changed
```

---

## References & Resources

### Documentation
- [Adafruit MadgwickAHRS Library](https://github.com/arduino-libraries/MadgwickAHRS)
- [x-io Technologies AHRS Algorithms](https://x-io.co.uk/open-source-imu-and-ahrs-algorithms/)
- [kriswiner 9-DOF Sensor Fusion Wiki](https://github.com/kriswiner/MPU6050/wiki/Affordable-9-DoF-Sensor-Fusion)

### Tools
- [lundeen06/magnetometer-calibration-tool](https://github.com/lundeen06/magnetometer-calibration-tool)
- [italocjs/magnetometer_calibration](https://github.com/italocjs/magnetometer_calibration)
- [mprograms/QMC5883LCompass](https://github.com/mprograms/QMC5883LCompass)

### Research Papers
- Madgwick et al. "An efficient orientation filter for inertial and inertial/magnetic sensor arrays"
- Sabatini et al. "Quaternion-based extended Kalman filter for determining orientation"

---

## Summary Table

| Aspect | BNO085 | MPU6050+Mag | Winner |
|--------|--------|-------------|--------|
| **Accuracy** | ±5° | ±10-15° | BNO085 |
| **Cost** | $35-50 | $8 | MPU6050 |
| **Setup time** | 10 min | 28 hours | BNO085 |
| **Maintenance** | Minimal | Recal as needed | BNO085 |
| **Learning value** | None | High | MPU6050 |
| **Flexibility** | Limited | Full | MPU6050 |
| **Production readiness** | Immediate | After testing | BNO085 |

**Recommendation for your project:**
- **Short term:** Use BNO085 (already working, documented, production-ready)
- **Research phase:** Implement MPU6050 in parallel (understand the math, validate feasibility)
- **Future:** MPU6050 for cost-sensitive production, BNO085 for accuracy-critical applications

---

**Status:** Research compilation complete. Ready for implementation phase.
