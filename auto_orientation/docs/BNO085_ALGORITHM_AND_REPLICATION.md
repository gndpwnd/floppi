# BNO085 Algorithm Analysis & MPU6050 Replication Guide

**Purpose:** Understand how the BNO085 achieves absolute orientation, and replicate this with MPU6050 + magnetometer  
**Last Updated:** 2026-05-07  
**Author:** Research & Analysis

---

## Part 1: How the BNO085 Works

### Architecture

The BNO085 is a **System-in-Package (SiP)** that contains:
- **Sensors:** Triaxial accelerometer + triaxial gyroscope + magnetometer
- **Processor:** 32-bit ARM Cortex-M0+ microcontroller
- **Firmware:** CEVA's **SH-2 MotionEngine** (proprietary sensor fusion)

This is why it works so well—it's not just sensors, it's a complete computer running sophisticated algorithms in real-time.

### Communication Protocol

**SHTP** (Sensor Hub Transport Protocol):
- Proprietary protocol defined by CEVA
- Allows host (Arduino) to configure and read sensor reports
- Data sent as binary packets with report types
- Our code requests `SH2_ROTATION_VECTOR` (report type 0x05)

See: [BNO080/085 Datasheet](https://docs.sparkfun.com/SparkFun_VR_IMU_Breakout_BNO086_QWIIC/assets/component_documentation/BNO080_085-Datasheet_v1.16.pdf)

### The MotionEngine: What Happens Inside

The SH-2 firmware runs at ~200 Hz internally and performs:

1. **Raw Sensor Acquisition** (10 kHz)
   - Accelerometer: measures gravity (9.8 m/s²) + linear motion
   - Gyroscope: measures rotation rate (degrees/second)
   - Magnetometer: measures Earth's magnetic field

2. **Sensor Fusion** (proprietary algorithm, likely Kalman-based)
   - **Problem:** Each sensor has issues
     - Accelerometer: noisy, only works if stationary
     - Gyroscope: integrates error (drift), no absolute reference
     - Magnetometer: affected by local interference (hard/soft iron)
   - **Solution:** Combine them to cancel weaknesses
     - Accel gives accurate gravity vector (for pitch/roll)
     - Mag gives accurate north (for yaw) if calibrated
     - Gyro fills gaps when moving fast

3. **Calibration Processing**
   - Monitors sensor quality in real-time
   - Detects magnetic disturbances
   - Adjusts filter gains based on calibration level
   - Stores calibration coefficients in NVM (non-volatile memory)

4. **Output Generation**
   - Outputs quaternion (4 numbers: w, x, y, z)
   - Quaternion represents complete 3D rotation
   - Magnitude ≈ 1.0 always (normalized)

---

## Part 2: Understanding Quaternions & Absolute Orientation

### Why Quaternions?

A quaternion is a 4-element vector `[w, x, y, z]` that represents a 3D rotation:
- `w` = scalar part (rotation angle component)
- `x, y, z` = vector part (rotation axis component)

**Advantages over Euler angles (roll/pitch/yaw):**
1. **No gimbal lock:** Euler angles break at certain orientations (e.g., 90° pitch)
2. **Smooth interpolation:** Can blend between orientations
3. **Computational efficiency:** Fewer trigonometric operations
4. **Complete representation:** Single, unambiguous rotation

**Constraint:** `w² + x² + y² + z² = 1` (always normalized)

### From Quaternion to Euler Angles

Our firmware converts quaternion → Euler angles for visualization:

```
// Roll (rotation around X-axis)
roll = atan2(2(wy + xz), 1 - 2(x² + y²))

// Pitch (rotation around Y-axis)  
pitch = asin(2(wy - zx))

// Yaw (rotation around Z-axis)
yaw = atan2(2(wz + xy), 1 - 2(y² + z²))
```

These formulas are in our `bno085.cpp` line 150-170.

### What "Absolute Orientation" Means

**Absolute:** Referenced to fixed world axes
- **Up/Down:** Gravity (always vertical)
- **North/South:** Magnetic north (Earth's field)
- **East/West:** Perpendicular to north

**Unlike relative orientation** (gyro-only), absolute orientation never drifts because it has external references (gravity + magnetic field).

---

## Part 3: Replicating with MPU6050 + Magnetometer

### The Challenge

BNO085 has:
- ✅ Integrated sensor fusion firmware
- ✅ Calibration storage
- ✅ Sophisticated algorithm (proprietary)

MPU6050 + magnetometer has:
- ❌ No firmware (just raw sensors)
- ❌ You must implement the fusion algorithm
- ⚠️ Harder to calibrate magnetometer properly

### The Solution: Madgwick or Mahony Filter

**Madgwick Filter** (recommended for accuracy):
- Developed by Sebastian Madgwick (University of Bristol)
- Uses quaternion-based gradient descent
- Runs at ~100+ Hz on Arduino
- Includes magnetic field weighting
- Reference: [GitHub kriswiner/MPU6050HMC5883AHRS](https://github.com/kriswiner/MPU6050HMC5883AHRS)

**How it works:**
1. Start with initial quaternion estimate (from accel + mag)
2. Each time step:
   - Predict new quaternion based on gyro rotation rate
   - Measure accel direction (estimate gravity)
   - Measure mag direction (estimate north)
   - Calculate error between prediction and measurements
   - Adjust quaternion to reduce error (gradient descent)
3. Output final quaternion

**Pseudo-code:**
```cpp
void updateIMU() {
  // 1. Read sensors
  float ax, ay, az = readAccelerometer();  // gravity vector
  float gx, gy, gz = readGyroscope();      // rotation rate
  float mx, my, mz = readMagnetometer();   // magnetic field
  
  // 2. Normalize vectors to unit length
  normalize(ax, ay, az);
  normalize(mx, my, mz);
  
  // 3. Predict next quaternion from gyro
  // (integrate rotation rate over dt)
  updateQuaternionFromGyro(q, gx, gy, gz, dt);
  
  // 4. Calculate accel & mag errors
  vec3 accelError = calculateAccelError(q, ax, ay, az);
  vec3 magError = calculateMagError(q, mx, my, mz);
  
  // 5. Correct quaternion (feedback)
  q = q + (accelError + magError) * Kp * dt;
  
  // 6. Re-normalize
  normalizeQuaternion(q);
}
```

### Key Parameters

**Sample rate:** Must be consistent (e.g., 100 Hz = 0.01s per sample)

**Madgwick constants:**
- `beta` (0.1-1.0): Filter strength
  - High = trusts sensors more, faster response
  - Low = trusts gyro more, smoother output
- `zeta` (0.0-1.0): Gyro drift compensation
  - Higher = corrects drift faster

**Magnetometer calibration:** CRITICAL
- Must measure local magnetic disturbances (hard iron + soft iron)
- Store offsets and scaling matrix
- Without this: yaw heading completely wrong
- See: BNO085's auto-calibration as reference

---

## Part 4: Practical Implementation for MPU6050

### Hardware Requirements

```
MPU6050 (6 DOF):
  - I2C interface (same as BNO085)
  - 3-axis accel + 3-axis gyro
  - ~$3 cost

Magnetometer (add-on for 9 DOF):
  - HMC5883L (3-axis magnetic field)
  - OR QMC5883L (cheaper alternative)
  - OR LIS3MDL (higher quality)
  - ~$1-5 cost
  - I2C or SPI interface

Power: 3.3V from Arduino (same as BNO085)
```

### Code Structure

```cpp
// File: mpu6050_fusion.h/cpp
class IMUFusion {
private:
  Quaternion q;          // Current orientation
  float beta;            // Madgwick filter gain
  float zeta;            // Gyro drift compensation
  
  // Sensor objects
  MPU6050 mpu;
  HMC5883L mag;
  
public:
  void initialize();
  void read();           // Read all 9 DOF sensors
  void update(float dt); // Madgwick filter update
  Quaternion getOrientation() { return q; }
  void calibrateMagnetometer(); // Critical!
};
```

### Integration into Our Project

1. **Parallel implementation:**
   - Keep BNO085 as primary reference
   - Add MPU6050+mag as secondary
   - Compare outputs in JSON

2. **Swap support:**
   - Abstraction layer (extend `OrientationSensor` base class)
   - Config option to select sensor
   - Identical JSON output format

3. **Testing:**
   - Run both sensors simultaneously
   - Log quaternion differences
   - Verify Euler angles match
   - Identify drift over time

---

## Part 5: Why BNO085 is Better (and Why it Costs More)

| Feature | BNO085 | MPU6050+Mag |
|---------|--------|------------|
| **Accuracy** | ±5° | ±10-15° (depends on implementation) |
| **Drift over time** | None (refs to gravity/mag) | Gyro drift if mag noisy |
| **Calibration** | Automatic | Manual (critical step) |
| **Code complexity** | Zero (firmware does it) | Significant (implement Madgwick) |
| **Computational cost** | Internal processor | ~5-10% CPU on Arduino |
| **Cost** | ~$30 | ~$5-10 total |
| **Setup time** | Plug and play | 2-3 weeks development |

---

## Part 6: BNO085 Firmware: What's Really Happening

When you enable `SH2_ROTATION_VECTOR` and move the sensor:

1. **Hardware processor wakes up** (on-die Cortex-M0+)
2. **Firmware collects sensor data at 10 kHz**
3. **Madgwick-style filter runs at ~200 Hz** internally
4. **Calibration monitoring** (checks mag field quality, detects hard iron)
5. **Quaternion output** ready at your requested rate (we use 10 Hz)
6. **Calibration coefficients** stored to EEPROM when level improves

The `SH-2 MotionEngine` is decades of research by CEVA compressed into firmware.

---

## Part 7: Recommendations for Your Project

### Immediate (BNO085 - Production Ready)
✅ **Done:**
- I2C initialization with 500ms startup delay
- Quaternion reading at 10 Hz
- Auto-calibration to EEPROM
- Euler angle conversion
- JSON output format

✅ **Ready to deploy:** System works correctly

### Next Phase (MPU6050 Research)
1. **Create `src/sensors/mpu6050_fusion.h/cpp`**
   - Implement Madgwick filter
   - Add magnetometer support
   - Match BNO085 interface (extend `OrientationSensor`)

2. **Document calibration process:**
   - Magnetometer hard/soft iron calibration
   - Madgwick parameter tuning
   - Comparison against BNO085 reference

3. **Test extensively:**
   - Both sensors running simultaneously
   - Log quaternion differences
   - Document where MPU6050 drifts

### If You Want to Go Deep
- Study CEVA's MotionEngine algorithm (proprietary, but papers exist)
- Investigate Mahony filter variant
- Add gyro bias estimation
- Implement full Kalman filter version

---

## References

- [Adafruit BNO055/085 Guide](https://learn.adafruit.com/adafruit-bno055-absolute-orientation-sensor)
- [BNO080/085 Official Datasheet](https://docs.sparkfun.com/SparkFun_VR_IMU_Breakout_BNO086_QWIIC/assets/component_documentation/BNO080_085-Datasheet_v1.16.pdf)
- [Madgwick Filter Implementation](https://github.com/kriswiner/MPU6050HMC5883AHRS)
- [Mahony Filter Alternative](https://github.com/Reefwing-Software/Reefwing-AHRS)
- [MATLAB Sensor Fusion Guide](https://www.mathworks.com/help/fusion/ug/Estimating-Orientation-Using-Inertial-Sensor-Fusion-and-MPU-9250.html)
- [CEVA MotionEngine](https://www.ceva-ip.com/)

---

**Status:** Research complete. Ready for MPU6050 implementation phase.
