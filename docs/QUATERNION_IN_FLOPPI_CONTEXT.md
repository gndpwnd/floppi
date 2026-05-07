# Quaternion Mathematics in the Floppi Project: Integration Guide

**Purpose:** Apply quaternion mathematics to specific systems in the floppi drone.  
**Context:** Connects theory to actual code locations in the floppi codebase.  
**Date:** 2026-05-07

---

## 1. Current Systems Using Quaternions

### 1.1 BNO085 Sensor (Primary: `/auto_orientation`)

**File:** `/home/devel/floppi/auto_orientation/src/sensors/bno085.cpp`

**What it does:**
- Reads absolute orientation quaternion from BNO085 IMU
- Quaternion represents world-to-body rotation
- Automatically calibrates (updates calibration status)
- Stores calibration in EEPROM

**Key quaternion operations:**

```cpp
// Line ~150-170: Quaternion to Euler angles
roll_euler = atan2(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.2957795;
pitch_euler = asin(-2.0f*(q1*q3 - q0*q2)) * 57.2957795;
yaw_euler = atan2(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.2957795;
```

**How to use in code:**

```cpp
#include "auto_orientation/src/sensors/bno085.h"

BNO085 bno;
bno.begin();

// Each loop:
if (bno.hasNewData()) {
    float qw = bno.getQuaternion().w;
    float qx = bno.getQuaternion().x;
    float qy = bno.getQuaternion().y;
    float qz = bno.getQuaternion().z;
    
    // Use for attitude control, frame transformations, etc.
}
```

**Integration Points:**
- ✅ I2C communication (100 kHz clock, 500ms startup delay)
- ✅ Quaternion output (magnitude ≈ 1.0)
- ✅ Calibration persistence (EEPROM)
- ⚠️ Frame convention: NWU (North-West-Up), world-to-body rotation

---

### 1.2 Madgwick Filter (Alternative: `flight_controller`)

**File:** `/home/devel/floppi/flight_controller/src/imu.cpp` (lines ~198-265)

**What it does:**
- Implements 6DOF Madgwick filter (gyro + accel, no mag)
- Used with MPU6050 IMU when BNO085 not available
- Outputs quaternion + Euler angles
- Updates at loop frequency (2000 Hz)

**Key quaternion operations:**

```cpp
// Quaternion kinematics from gyro (lines ~209-212)
qDot1 = 0.5f * (-q1*gx - q2*gy - q3*gz);
qDot2 = 0.5f * (q0*gx + q2*gz - q3*gy);
qDot3 = 0.5f * (q0*gy - q1*gz + q3*gx);
qDot4 = 0.5f * (q0*gz + q1*gy - q2*gx);

// Accelerometer correction (lines ~234-243)
s0 = _4q0*q2q2 + _2q2*ax + _4q0*q1q1 - _2q1*ay;
s1 = _4q1*q3q3 - _2q3*ax + 4.0f*q0q0*q1 - _2q0*ay - _4q1 + _8q1*q1q1 + _8q1*q2q2 + _4q1*az;
s2 = 4.0f*q0q0*q2 + _2q0*ax + _4q2*q3q3 - _2q3*ay - _4q2 + _8q2*q1q1 + _8q2*q2q2 + _4q2*az;
s3 = 4.0f*q1q1*q3 - _2q1*ax + 4.0f*q2q2*q3 - _2q2*ay;

// Quaternion normalization (lines ~256-260)
recipNorm = invSqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
q0 *= recipNorm;
q1 *= recipNorm;
q2 *= recipNorm;
q3 *= recipNorm;

// Quaternion to Euler angles (lines ~262-264)
roll_IMU = atan2(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.2957795;
pitch_IMU = asin(-2.0f*(q1*q3 - q0*q2)) * 57.2957795;
yaw_IMU = atan2(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.2957795;
```

**How to use in code:**

```cpp
// From flight_controller code
Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt);

// After call: q0, q1, q2, q3 are updated
// Also: roll_IMU, pitch_IMU, yaw_IMU are updated (degrees)
```

**Key differences from BNO085:**
- ❌ No absolute heading (gyro drifts over time)
- ❌ Susceptible to vibration (affects accel correction)
- ✅ Lower cost, no external sensor needed
- ✅ Works with MPU6050 (standard IMU)

---

## 2. How to Use Quaternions for Drone Control

### 2.1 Attitude Representation (Instead of Euler Angles)

**Problem:** Euler angles have gimbal lock at pitch = ±90°

**Solution:** Store and compute attitude as quaternion

```cpp
// Current approach (Euler angles)
float roll_deg, pitch_deg, yaw_deg;  // Can't control all axes at pitch ≈ 90°

// Better approach (Quaternion)
struct Quaternion {
    float w, x, y, z;  // Always valid, no singularities
};

Quaternion attitude = bno.getQuaternion();  // From BNO085
// Can compute attitude error without gimbal lock:
// q_error = q_desired * conjugate(q_current)
```

**Where to implement:** Control loop in `flight_controller/src/control.cpp`

---

### 2.2 Frame Transformations (Body ↔ World)

**Problem:** Sensors output in body frame (aircraft axes), control needs world frame (navigation axes)

**Example: Accelerometer to World Frame**

```cpp
// Current code (from flight_controller/src/imu.cpp):
// Madgwick filter outputs roll_IMU, pitch_IMU, yaw_IMU
// Then converts back to matrix for rotation

// Better approach using quaternions directly:
Quaternion q_attitude = bno.getQuaternion();  // [w, x, y, z]

Vector3D accel_body = imu.getAcceleration();   // Body-frame accel
Vector3D accel_world = rotateByQuaternion(
    conjugate(q_attitude),  // Body-to-world rotation
    accel_body
);

// Now accel_world is in world frame (navigation coordinates)
// Should be [0, 0, -1] when level (gravity points down)
```

**Application:** Zero out gravity in accelerometer before using for position integration

---

### 2.3 Composing Rotations (Waypoint Guidance)

**Problem:** Drone needs to rotate from current attitude to waypoint heading

**Solution:** Use quaternion multiplication

```cpp
Quaternion q_current = bno.getQuaternion();   // Current attitude
Quaternion q_desired = eulerToQuaternion(     // Desired attitude
    roll_des, pitch_des, yaw_des
);

// Ensure shortest path (avoid unwinding)
if (dot(q_desired, q_current) < 0) {
    q_desired.w = -q_desired.w;
    q_desired.x = -q_desired.x;
    q_desired.y = -q_desired.y;
    q_desired.z = -q_desired.z;
}

// Compute error quaternion
Quaternion q_error = q_desired * conjugate(q_current);

// q_error now represents the rotation needed to go from current to desired
// Use magnitude to gauge how far off we are
```

**File to modify:** `flight_controller/src/control.cpp` (attitude controller section)

---

## 3. Practical Integration Examples

### Example 1: Replace Euler-Based Attitude Control with Quaternion Error Control

**Current code (Euler angles):**

```cpp
// flight_controller/src/control.cpp (controlANGLE function)
error = roll_des - roll_IMU;  // Euler angle error
```

**New code (Quaternion error):**

```cpp
// Quaternion-based error
Quaternion q_current = bno.getQuaternion();
Quaternion q_desired = eulerToQuaternion(roll_des * DEG_TO_RAD, 
                                          pitch_des * DEG_TO_RAD, 
                                          yaw_des * DEG_TO_RAD);

// Error quaternion represents rotation from current to desired
Quaternion q_error = q_desired * conjugate(q_current);

// Extract error magnitude (how far off we are)
float error_magnitude = 2 * acos(constrain(q_error.w, -1, 1));

// Use error_magnitude in PID controller instead of angle error
// Benefits: Works at gimbal-lock angles (pitch = 90°)
```

**Benefit:** Works correctly even during aggressive 3D maneuvers (vertical climbs, inverted flight)

---

### Example 2: GPS + Attitude = World Position of Body-Frame Points

**Use case:** Compute antenna position from GPS + drone attitude + antenna offset

```cpp
// inputs
double gps_lat = 37.4419, gps_lon = -122.143, gps_alt = 50.0;  // meters
float antenna_x_body = 0.1, antenna_y_body = 0, antenna_z_body = 0.05;  // meters
Quaternion attitude = bno.getQuaternion();

// Rotate antenna offset from body to world frame
Vector3D antenna_offset_body(antenna_x_body, antenna_y_body, antenna_z_body);
Vector3D antenna_offset_world = rotateByQuaternion(
    conjugate(attitude),  // Body-to-world
    antenna_offset_body
);

// Add to GPS position
double antenna_lat = gps_lat + antenna_offset_world.y / 111320.0;
double antenna_lon = gps_lon + antenna_offset_world.x / (111320.0 * cos(gps_lat * M_PI / 180));
double antenna_alt = gps_alt + antenna_offset_world.z;

// Now antenna_lat, antenna_lon, antenna_alt are true antenna position
```

**File to implement:** `swarm_api/src/gps_handler.cpp` or new `positioning.cpp`

---

### Example 3: Sensor Fusion: Blend BNO085 with GPS-Based Heading

**Scenario:** BNO085 yaw drifts over time. GPS-based heading estimate becomes available (from RTK, compass, etc.).

```cpp
// Get attitude from BNO085
Quaternion q_bno = bno.getQuaternion();

// Convert to Euler to extract yaw
EulerAngles euler_bno = quaternionToEuler(q_bno);
float yaw_bno = euler_bno.yaw;

// Get yaw estimate from GPS/RTK/magnetometer
float yaw_gps = computeHeadingFromGPS(prev_position, curr_position);

// Blend the two yaw estimates (complementary filter)
float alpha = 0.1;  // 10% GPS, 90% BNO085
float yaw_fused = (1 - alpha) * yaw_bno + alpha * yaw_gps;

// Reconstruct quaternion with fused yaw
Quaternion q_fused = eulerToQuaternion(
    euler_bno.roll,
    euler_bno.pitch,
    yaw_fused
);

// Use q_fused for navigation
```

**File to implement:** New `src/sensors/sensor_fusion.cpp`

---

## 4. Known Issues & Gotchas

### Issue 1: BNO085 Quaternion Magnitude ≠ 1.0

**Symptom:** Quaternion from BNO085 has magnitude 0.5 or 2.0

**Cause:**
- Sensor not calibrated (move in figure-8 patterns)
- I2C communication error
- Firmware bug

**Fix:**

```cpp
float mag = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
if (mag < 0.95 || mag > 1.05) {
    Serial.println("WARNING: BNO085 quaternion not normalized!");
    // Normalize manually
    q.w /= mag;
    q.x /= mag;
    q.y /= mag;
    q.z /= mag;
}
```

**File:** Add diagnostic check in `auto_orientation/src/sensors/bno085.cpp`, `begin()` function

---

### Issue 2: Gimbal Lock in Euler Angle Conversion

**Symptom:** Pitch = 90°, roll and yaw become undefined or coupled

**Where it happens:**

```cpp
// This formula is unstable at pitch = ±90°:
pitch = asin(2*(w*y - z*x));  // Returns ±π/2
roll = atan2(...);             // Returns inconsistent values
yaw = atan2(...);              // Returns inconsistent values
```

**Fix:** Don't use Euler angles for control. Use quaternion error instead.

---

### Issue 3: Unwinding Phenomenon

**Symptom:** Drone suddenly does a 360° rotation instead of 90°

**Cause:** Quaternion error q_error = q_target * conjugate(q_current) doesn't ensure shortest path

**Fix:**

```cpp
// Before computing error, ensure target and current are on same hemisphere
if (dot(q_target, q_current) < 0) {
    // They're antipodes: negate one
    q_target = Quaternion(-q_target.w, -q_target.x, -q_target.y, -q_target.z);
}

// Now q_error will go the short way
Quaternion q_error = q_target * conjugate(q_current);
```

**File:** Add to `flight_controller/src/control.cpp` before attitude error calculation

---

## 5. Testing & Validation in Floppi

### 5.1 Unit Tests for Quaternion Math

**Add to:** `flight_controller/tests/test_quaternion.cpp` (create if doesn't exist)

```cpp
#include <gtest/gtest.h>
#include "../src/quaternion.h"

TEST(QuaternionNormalization, IdentityRemains1) {
    Quaternion q(1, 0, 0, 0);
    normalizeQuaternion(q);
    EXPECT_FLOAT_EQ(1.0f, q.magnitude());
}

TEST(QuaternionConversion, EulerRoundTrip) {
    float roll_in = 15.0f * M_PI / 180;
    float pitch_in = 30.0f * M_PI / 180;
    float yaw_in = 45.0f * M_PI / 180;
    
    Quaternion q = eulerToQuaternion(roll_in, pitch_in, yaw_in);
    EulerAngles euler_out = quaternionToEuler(q);
    
    EXPECT_NEAR(euler_out.roll, roll_in, 1e-5);
    EXPECT_NEAR(euler_out.pitch, pitch_in, 1e-5);
    EXPECT_NEAR(euler_out.yaw, yaw_in, 1e-5);
}

TEST(QuaternionRotation, 90DegreeZ) {
    // Rotate 90° about Z-axis: [1,0,0] -> [0,1,0]
    Quaternion q = axisAngleToQuaternion(Vector3D(0, 0, 1), M_PI / 2);
    Vector3D v(1, 0, 0);
    
    Vector3D v_rot = rotateByQuaternion(q, v);
    
    EXPECT_NEAR(0.0f, v_rot.x, 1e-5);
    EXPECT_NEAR(1.0f, v_rot.y, 1e-5);
    EXPECT_NEAR(0.0f, v_rot.z, 1e-5);
}
```

**Run tests:**

```bash
cd /home/devel/floppi/flight_controller
g++ -std=c++11 tests/test_quaternion.cpp src/quaternion.cpp -o test_quat -lm
./test_quat
```

---

### 5.2 Integration Test: BNO085 → Euler Angles

**Add to:** `auto_orientation/tests/test_bno_quaternion.ino`

```cpp
#include "Adafruit_BNO08x.h"
#include "../src/quaternion_math.h"

void testBNO085Quaternion() {
    // Setup
    Wire.begin();
    Adafruit_BNO08x bno08x;
    if (!bno08x.begin_I2C()) {
        Serial.println("BNO085 not found!");
        return;
    }
    bno08x.enableReport(SH2_ROTATION_VECTOR);
    
    // Test loop
    unsigned long count = 0;
    unsigned long start_time = millis();
    
    while (count < 100 && millis() - start_time < 10000) {  // 10 seconds
        if (bno08x.getSensorEvent()) {
            // Get quaternion
            float qw = bno08x.quaternion.w;
            float qx = bno08x.quaternion.x;
            float qy = bno08x.quaternion.y;
            float qz = bno08x.quaternion.z;
            
            // Check magnitude
            float mag = sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
            if (mag < 0.95 || mag > 1.05) {
                Serial.print("ERROR: Magnitude = ");
                Serial.println(mag);
                return;
            }
            
            // Convert to Euler
            float roll = atan2(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy));
            float pitch = asin(constrain(2*(qw*qy - qz*qx), -1, 1));
            float yaw = atan2(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz));
            
            Serial.print("R:");
            Serial.print(roll * 57.3, 1);
            Serial.print(" P:");
            Serial.print(pitch * 57.3, 1);
            Serial.print(" Y:");
            Serial.print(yaw * 57.3, 1);
            Serial.println();
            
            count++;
        }
    }
    
    Serial.println("PASS: BNO085 quaternion test");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    testBNO085Quaternion();
}

void loop() {}
```

---

### 5.3 Data Log Comparison: BNO085 vs Madgwick

**Purpose:** Verify BNO085 and Madgwick filters give similar results under normal conditions

```cpp
// In flight_controller/src/main.cpp, during flight:

// Log both quaternions
unsigned long log_timer = 0;
const unsigned long LOG_INTERVAL = 100;  // Every 100ms

if (millis() - log_timer > LOG_INTERVAL) {
    log_timer = millis();
    
    // BNO085 quaternion (if available)
    Quaternion q_bno = bno.getQuaternion();
    float dot_quat = q_bno.w * q0 + q_bno.x * q1 + q_bno.y * q2 + q_bno.z * q3;
    
    // Log to SD card or telemetry
    telemetry.printf("QUAT_DOT,%f,%f,%f,%f,%f,%f\n",
                     millis(),
                     q_bno.w, q_bno.x, q_bno.y, q_bno.z,
                     dot_quat);  // Dot product (should be close to 1)
}
```

**Analysis:**
- If dot_quat consistently ≈ 1.0: Filters agree, both working well
- If dot_quat ≈ 0.0 or negative: Major disagreement, debug sensors
- If dot_quat drifts over time: Likely gyro drift in Madgwick, BNO085 more stable

---

## 6. Migration Path: Moving to Quaternion-Based Control

### Phase 1: Preparation (Week 1)

- [ ] Create `/flight_controller/src/quaternion_math.h/cpp` with basic quaternion ops
- [ ] Add unit tests from section 5.1
- [ ] Verify BNO085 quaternion output (section 5.2)
- [ ] Document current Euler-based control system

### Phase 2: Parallel Implementation (Weeks 2-3)

- [ ] Implement quaternion error in control loop (alongside existing Euler error)
- [ ] Log both methods' outputs side-by-side
- [ ] Verify quaternion method works at gimbal-lock angles (pitch = 90°)

### Phase 3: Gradual Transition (Weeks 4-5)

- [ ] Replace attitude error calculation with quaternion error
- [ ] Update PID gains if needed
- [ ] Test in simulation first, then live flights
- [ ] Keep fallback to Euler angles in case of issues

### Phase 4: Cleanup (Week 6)

- [ ] Remove old Euler-based attitude code
- [ ] Simplify control logic
- [ ] Document final system

---

## 7. Files to Create/Modify

### New Files (Create)

```
/flight_controller/src/quaternion_math.h
/flight_controller/src/quaternion_math.cpp
/flight_controller/tests/test_quaternion.cpp
/docs/QUATERNION_REFERENCE.md (this file)
/docs/QUATERNION_IMPLEMENTATION_GUIDE.md
/docs/QUATERNION_IN_FLOPPI_CONTEXT.md
```

### Existing Files (Modify)

```
/flight_controller/src/imu.cpp           - Add comments, improve normalization
/flight_controller/src/control.cpp       - Replace Euler error with quaternion error
/auto_orientation/src/sensors/bno085.cpp - Add magnitude validation
/flight_controller/src/main.cpp          - Update telemetry for quaternions
```

---

## 8. Further Resources

**See also:**
- `/docs/QUATERNION_REFERENCE.md` — Theory and formulas
- `/docs/QUATERNION_IMPLEMENTATION_GUIDE.md` — Code implementations
- `/flight_controller/docs/math_and_algorithms.md` — Existing Madgwick documentation

**External references:**
- BNO085 Datasheet: `/literature/` (check for BNO datasheet PDF)
- Madgwick filter: Original paper in literature/
- Quaternion visualization: GeoGebra 3D online tool

---

**End of Document**

For questions, see the existing implementations:
- Madgwick: `/flight_controller/src/imu.cpp` (lines 198-265)
- BNO085: `/auto_orientation/src/sensors/bno085.cpp` (lines 150-170)
- Math reference: `/flight_controller/docs/math_and_algorithms.md`
