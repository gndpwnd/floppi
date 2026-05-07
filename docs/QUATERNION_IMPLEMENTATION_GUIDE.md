# Quaternion Implementation Guide: Code-Friendly Reference

**Purpose:** Pseudocode and working code for all quaternion operations needed in drone control systems.  
**Target Audience:** C++/Arduino developers implementing attitude estimation and control.  
**Date:** 2026-05-07

---

## Table of Contents

1. [Data Structures](#data-structures)
2. [Basic Operations](#basic-operations)
3. [Conversions](#conversions)
4. [Rotation Operations](#rotation-operations)
5. [Integration with BNO085](#integration-with-bno085)
6. [Integration with Madgwick Filter](#integration-with-madgwick-filter)
7. [Frame Transformations](#frame-transformations)
8. [Testing & Validation](#testing--validation)

---

## Data Structures

### Quaternion Class (C++)

```cpp
// Simple quaternion structure
struct Quaternion {
    float w, x, y, z;  // w is scalar, x,y,z are vector parts
    
    // Default: identity rotation (no rotation)
    Quaternion() : w(1.0f), x(0.0f), y(0.0f), z(0.0f) {}
    
    Quaternion(float w_, float x_, float y_, float z_) 
        : w(w_), x(x_), y(y_), z(z_) {}
    
    // Get magnitude
    float magnitude() const {
        return sqrt(w*w + x*x + y*y + z*z);
    }
    
    // Check if normalized (should be close to 1.0)
    bool isNormalized(float tolerance = 0.01f) const {
        float mag = magnitude();
        return fabs(mag - 1.0f) < tolerance;
    }
};
```

### Vector3D Structure

```cpp
struct Vector3D {
    float x, y, z;
    
    Vector3D() : x(0), y(0), z(0) {}
    Vector3D(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    float magnitude() const {
        return sqrt(x*x + y*y + z*z);
    }
    
    Vector3D normalized() const {
        float mag = magnitude();
        if (mag < 1e-6) return Vector3D(1, 0, 0);  // degenerate case
        return Vector3D(x/mag, y/mag, z/mag);
    }
    
    float dot(const Vector3D& v) const {
        return x*v.x + y*v.y + z*v.z;
    }
    
    Vector3D cross(const Vector3D& v) const {
        return Vector3D(
            y*v.z - z*v.y,
            z*v.x - x*v.z,
            x*v.y - y*v.x
        );
    }
};
```

---

## Basic Operations

### 1. Normalize Quaternion

**When to use:** After any arithmetic operation, before conversion to Euler angles, after integration in Madgwick filter.

```cpp
void normalizeQuaternion(Quaternion& q) {
    float mag = q.magnitude();
    
    if (mag < 1e-6) {
        // Degenerate case: magnitude is zero (shouldn't happen)
        q = Quaternion(1, 0, 0, 0);  // Reset to identity
        return;
    }
    
    float inv_mag = 1.0f / mag;
    q.w *= inv_mag;
    q.x *= inv_mag;
    q.y *= inv_mag;
    q.z *= inv_mag;
}

// Alternative: SIMD-friendly fast normalize (not always faster on small systems)
void normalizeQuaternionFast(Quaternion& q) {
    float mag_sq = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
    float inv_mag = 1.0f / sqrtf(mag_sq);
    q.w *= inv_mag;
    q.x *= inv_mag;
    q.y *= inv_mag;
    q.z *= inv_mag;
}
```

### 2. Quaternion Conjugate (Inverse)

**When to use:** Reversing rotations, computing error quaternions.

```cpp
Quaternion quaternionConjugate(const Quaternion& q) {
    return Quaternion(q.w, -q.x, -q.y, -q.z);
}

// In-place version
void conjugateInPlace(Quaternion& q) {
    q.x = -q.x;
    q.y = -q.y;
    q.z = -q.z;
    // q.w unchanged
}
```

**Property:**
```
q * q^(-1) = [1, 0, 0, 0]  (identity rotation)
```

### 3. Quaternion Multiplication (Composition)

**When to use:** Combining two rotations, applying rotation to body-frame vectors.

```cpp
Quaternion quaternionMultiply(const Quaternion& q1, const Quaternion& q2) {
    // Result = q1 * q2
    // Represents: first apply q2, then apply q1
    
    return Quaternion(
        q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z,
        q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
        q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
        q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w
    );
}

// In-place multiply: q *= other  (q = q * other)
void multiplyInPlace(Quaternion& q, const Quaternion& other) {
    Quaternion result = quaternionMultiply(q, other);
    q = result;
}
```

**Important:** q1 * q2 ≠ q2 * q1 (non-commutative)

### 4. Dot Product

**When to use:** Detecting antipodal equivalence, measuring quaternion similarity.

```cpp
float quaternionDot(const Quaternion& q1, const Quaternion& q2) {
    return q1.w*q2.w + q1.x*q2.x + q1.y*q2.y + q1.z*q2.z;
}
```

**Usage for antipodal detection:**

```cpp
void ensureShortestPath(Quaternion& target, const Quaternion& current) {
    // Make sure target and current have the same sign of w-component
    // This avoids the "unwinding" phenomenon
    
    float dot = quaternionDot(target, current);
    if (dot < 0) {
        // Antipodal: negate target to take the short way
        target.w = -target.w;
        target.x = -target.x;
        target.y = -target.y;
        target.z = -target.z;
    }
}
```

---

## Conversions

### Quaternion to Euler Angles (ZYX Convention)

**Pseudocode:**

```
Input: q = [w, x, y, z] (unit quaternion)
Output: roll, pitch, yaw (radians)

roll = atan2(2*(w*x + y*z), 1 - 2*(x² + y²))
pitch = asin(constrain(2*(w*y - z*x), -1, 1))
yaw = atan2(2*(w*z + x*y), 1 - 2*(y² + z²))
```

**C++ Implementation:**

```cpp
struct EulerAngles {
    float roll, pitch, yaw;  // radians
};

EulerAngles quaternionToEuler(const Quaternion& q) {
    EulerAngles euler;
    
    // Roll (x-axis rotation)
    float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    euler.roll = atan2(sinr_cosp, cosr_cosp);
    
    // Pitch (y-axis rotation)
    float sinp = 2 * (q.w * q.y - q.z * q.x);
    // Guard against out-of-range value due to numerical errors
    if (fabs(sinp) >= 1)
        euler.pitch = copysign(M_PI / 2, sinp);  // Use 90 degrees
    else
        euler.pitch = asin(sinp);
    
    // Yaw (z-axis rotation)
    float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    euler.yaw = atan2(siny_cosp, cosy_cosp);
    
    return euler;
}

// Convert to degrees
EulerAngles quaternionToEulerDegrees(const Quaternion& q) {
    EulerAngles euler = quaternionToEuler(q);
    const float RAD_TO_DEG = 57.29577951f;  // 180/π
    
    euler.roll *= RAD_TO_DEG;
    euler.pitch *= RAD_TO_DEG;
    euler.yaw *= RAD_TO_DEG;
    
    return euler;
}
```

### Euler Angles to Quaternion

**Pseudocode (ZYX order):**

```
Input: roll, pitch, yaw (radians)
Output: q = [w, x, y, z] (unit quaternion)

Compute half-angles:
  cr = cos(roll/2),   sr = sin(roll/2)
  cp = cos(pitch/2),  sp = sin(pitch/2)
  cy = cos(yaw/2),    sy = sin(yaw/2)

w = cr*cp*cy + sr*sp*sy
x = sr*cp*cy - cr*sp*sy
y = cr*sp*cy + sr*cp*sy
z = cr*cp*sy - sr*sp*cy
```

**C++ Implementation:**

```cpp
Quaternion eulerToQuaternion(float roll, float pitch, float yaw) {
    // Compute half angles
    float roll_half = roll / 2.0f;
    float pitch_half = pitch / 2.0f;
    float yaw_half = yaw / 2.0f;
    
    float cr = cos(roll_half);
    float sr = sin(roll_half);
    float cp = cos(pitch_half);
    float sp = sin(pitch_half);
    float cy = cos(yaw_half);
    float sy = sin(yaw_half);
    
    Quaternion q;
    q.w = cr*cp*cy + sr*sp*sy;
    q.x = sr*cp*cy - cr*sp*sy;
    q.y = cr*sp*cy + sr*cp*sy;
    q.z = cr*cp*sy - sr*sp*cy;
    
    return q;
}

// Convenience: accepts degrees
Quaternion eulerToQuaternionDegrees(float roll_deg, float pitch_deg, float yaw_deg) {
    const float DEG_TO_RAD = 0.0174532925f;  // π/180
    
    return eulerToQuaternion(
        roll_deg * DEG_TO_RAD,
        pitch_deg * DEG_TO_RAD,
        yaw_deg * DEG_TO_RAD
    );
}
```

### Axis-Angle to Quaternion

**Pseudocode:**

```
Input: axis = [nx, ny, nz] (unit vector), angle (radians)
Output: q = [w, x, y, z]

half_angle = angle / 2
w = cos(half_angle)
sin_half = sin(half_angle)
x = nx * sin_half
y = ny * sin_half
z = nz * sin_half
```

**C++ Implementation:**

```cpp
Quaternion axisAngleToQuaternion(const Vector3D& axis, float angle) {
    Vector3D normalizedAxis = axis.normalized();
    
    float half_angle = angle / 2.0f;
    float sin_half = sin(half_angle);
    
    Quaternion q;
    q.w = cos(half_angle);
    q.x = normalizedAxis.x * sin_half;
    q.y = normalizedAxis.y * sin_half;
    q.z = normalizedAxis.z * sin_half;
    
    return q;
}
```

### Quaternion to Axis-Angle

**Pseudocode:**

```
Input: q = [w, x, y, z] (unit quaternion)
Output: axis = [nx, ny, nz], angle (radians)

angle = 2 * acos(constrain(w, -1, 1))

if sin(angle/2) ≈ 0:
    axis = [1, 0, 0]  (arbitrary axis for zero rotation)
else:
    sin_half = sin(angle/2)
    nx = x / sin_half
    ny = y / sin_half
    nz = z / sin_half
    axis = [nx, ny, nz]
```

**C++ Implementation:**

```cpp
void quaternionToAxisAngle(const Quaternion& q, Vector3D& axis, float& angle) {
    // Clamp w to [-1, 1] to avoid acos domain errors
    float w_clamped = fmax(-1.0f, fmin(1.0f, q.w));
    angle = 2.0f * acos(w_clamped);
    
    float sin_half = sin(angle / 2.0f);
    
    if (fabs(sin_half) < 1e-6) {
        // Zero rotation or very small angle
        axis = Vector3D(1, 0, 0);  // arbitrary axis
        angle = 0;
    } else {
        float inv_sin = 1.0f / sin_half;
        axis = Vector3D(q.x * inv_sin, q.y * inv_sin, q.z * inv_sin);
    }
}
```

---

## Rotation Operations

### Rotate a Vector Using Quaternion

**Method 1: Using rotation matrix (most intuitive)**

```cpp
Vector3D rotateVectorByQuaternion_Matrix(const Quaternion& q, const Vector3D& v) {
    // Build rotation matrix from quaternion
    float w2 = q.w*q.w;
    float x2 = q.x*q.x;
    float y2 = q.y*q.y;
    float z2 = q.z*q.z;
    
    float wx2 = 2*q.w*q.x;
    float wy2 = 2*q.w*q.y;
    float wz2 = 2*q.w*q.z;
    float xy2 = 2*q.x*q.y;
    float xz2 = 2*q.x*q.z;
    float yz2 = 2*q.y*q.z;
    
    // Apply rotation matrix: R(q) * v
    Vector3D rotated;
    rotated.x = (w2 + x2 - y2 - z2)*v.x + (xy2 - wz2)*v.y + (xz2 + wy2)*v.z;
    rotated.y = (xy2 + wz2)*v.x + (w2 - x2 + y2 - z2)*v.y + (yz2 - wx2)*v.z;
    rotated.z = (xz2 - wy2)*v.x + (yz2 + wx2)*v.y + (w2 - x2 - y2 + z2)*v.z;
    
    return rotated;
}
```

**Method 2: Using quaternion formula (more compact)**

```cpp
Vector3D rotateVectorByQuaternion_Formula(const Quaternion& q, const Vector3D& v) {
    // v' = q * [0, v] * q^*
    // This is more compact but slightly slower
    
    Quaternion v_quat(0, v.x, v.y, v.z);
    Quaternion q_conj = quaternionConjugate(q);
    
    Quaternion result = quaternionMultiply(q, quaternionMultiply(v_quat, q_conj));
    
    return Vector3D(result.x, result.y, result.z);
}
```

**Preferred:** Use Method 1 (matrix form) — it's more efficient and avoids two quaternion multiplications.

### Compose Three Rotations

**Goal:** Apply rotations R1, then R2, then R3 in sequence

```cpp
// Given three axis-angle rotations
Vector3D axis1(1, 0, 0);  // X-axis
float angle1 = 30.0f * M_PI / 180.0f;  // 30 degrees in radians
Quaternion q1 = axisAngleToQuaternion(axis1, angle1);

Vector3D axis2(0, 0, 1);  // Z-axis
float angle2 = 45.0f * M_PI / 180.0f;
Quaternion q2 = axisAngleToQuaternion(axis2, angle2);

Vector3D axis3(0, 1, 0);  // Y-axis
float angle3 = 20.0f * M_PI / 180.0f;
Quaternion q3 = axisAngleToQuaternion(axis3, angle3);

// Compose: apply q1 first, then q2, then q3
// Order: q_result = q3 * q2 * q1
Quaternion q_temp = quaternionMultiply(q2, q1);
Quaternion q_result = quaternionMultiply(q3, q_temp);
normalizeQuaternion(q_result);
```

### Inverse Rotation (Reverse a Rotation)

**Goal:** Find the quaternion that reverses rotation q

```cpp
Quaternion inverseRotation(const Quaternion& q) {
    // For unit quaternion: q^(-1) = q^*
    return quaternionConjugate(q);
}

// Example: if q rotates A -> B, then q^(-1) rotates B -> A
Quaternion q_world_to_body = bno085.getQuaternion();
Quaternion q_body_to_world = inverseRotation(q_world_to_body);
```

---

## Integration with BNO085

### Reading Quaternion from BNO085

```cpp
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x bno08x;
Quaternion attitude;

void setupBNO085() {
    Wire.begin();
    if (!bno08x.begin_I2C()) {
        Serial.println("ERROR: BNO085 not found");
        while(1);
    }
    
    // Enable rotation vector (absolute orientation)
    bno08x.enableReport(SH2_ROTATION_VECTOR);
    delay(100);
}

void readBNO085() {
    if (bno08x.getSensorEvent()) {
        // BNO085 outputs normalized quaternion
        attitude.w = bno08x.quaternion.w;
        attitude.x = bno08x.quaternion.x;
        attitude.y = bno08x.quaternion.y;
        attitude.z = bno08x.quaternion.z;
        
        // Verify normalization (diagnostic)
        float mag = attitude.magnitude();
        if (mag < 0.95f || mag > 1.05f) {
            Serial.print("WARNING: Quaternion magnitude = ");
            Serial.println(mag);
        }
    }
}

// Convert to Euler angles for display
void printAttitude() {
    EulerAngles euler = quaternionToEulerDegrees(attitude);
    
    Serial.print("Roll: ");
    Serial.print(euler.roll);
    Serial.print(" Pitch: ");
    Serial.print(euler.pitch);
    Serial.print(" Yaw: ");
    Serial.println(euler.yaw);
}
```

### Handling BNO085 Reset

```cpp
bool checkBNO085Reset() {
    // BNO085 resets quaternion to identity when it loses lock
    // This causes attitude to jump unexpectedly
    
    if (bno08x.wasReset()) {
        Serial.println("BNO085 was reset!");
        
        // Reinitialize
        bno08x.enableReport(SH2_ROTATION_VECTOR);
        delay(100);
        
        return true;
    }
    return false;
}
```

---

## Integration with Madgwick Filter

### Madgwick 6DOF (Gyro + Accelerometer)

```cpp
class MadgwickFilter {
private:
    Quaternion q;  // Current attitude estimate
    float beta = 0.04f;  // Filter gain
    
public:
    MadgwickFilter() : q(1, 0, 0, 0) {}  // Start at identity
    
    void update(const Vector3D& gyro_dps,    // Gyroscope in deg/s
                const Vector3D& accel_g,     // Accelerometer in G's
                float dt) {                  // Time step in seconds
        
        // 1. Convert gyroscope to rad/s
        const float DEG_TO_RAD = 0.0174532925f;
        Vector3D gyro_rads = Vector3D(
            gyro_dps.x * DEG_TO_RAD,
            gyro_dps.y * DEG_TO_RAD,
            gyro_dps.z * DEG_TO_RAD
        );
        
        // 2. Quaternion kinematics from gyroscope
        float qDot1 = 0.5f * (-q.x*gyro_rads.x - q.y*gyro_rads.y - q.z*gyro_rads.z);
        float qDot2 = 0.5f * (q.w*gyro_rads.x + q.y*gyro_rads.z - q.z*gyro_rads.y);
        float qDot3 = 0.5f * (q.w*gyro_rads.y - q.x*gyro_rads.z + q.z*gyro_rads.x);
        float qDot4 = 0.5f * (q.w*gyro_rads.z + q.x*gyro_rads.y - q.y*gyro_rads.x);
        
        // 3. Accelerometer correction
        Vector3D a_norm = accel_g.normalized();
        
        if (a_norm.magnitude() > 1e-6) {
            // Compute gradient of objective function
            float a_x = a_norm.x;
            float a_y = a_norm.y;
            float a_z = a_norm.z;
            
            // Expected gravity direction in body frame
            float _2q0 = 2*q.w, _2q1 = 2*q.x, _2q2 = 2*q.y, _2q3 = 2*q.z;
            float _4q0 = 4*q.w, _4q1 = 4*q.x, _4q2 = 4*q.y;
            float _8q1 = 8*q.x, _8q2 = 8*q.y;
            float q0q0 = q.w*q.w, q1q1 = q.x*q.x, q2q2 = q.y*q.y, q3q3 = q.z*q.z;
            
            float s0 = _4q0*q2q2 + _2q2*a_x + _4q0*q1q1 - _2q1*a_y;
            float s1 = _4q1*q3q3 - _2q3*a_x + 4*q0q0*q.x - _2q0*a_y - _4q1 + 
                       _8q1*q1q1 + _8q1*q2q2 + _4q1*a_z;
            float s2 = 4*q0q0*q.y + _2q0*a_x + _4q2*q3q3 - _2q3*a_y - _4q2 + 
                       _8q2*q1q1 + _8q2*q2q2 + _4q2*a_z;
            float s3 = 4*q1q1*q.z - _2q1*a_x + 4*q2q2*q.z - _2q2*a_y;
            
            // Normalize gradient
            float norm_s = sqrt(s0*s0 + s1*s1 + s2*s2 + s3*s3);
            if (norm_s > 1e-6) {
                s0 /= norm_s;
                s1 /= norm_s;
                s2 /= norm_s;
                s3 /= norm_s;
            }
            
            // Apply correction
            qDot1 -= beta * s0;
            qDot2 -= beta * s1;
            qDot3 -= beta * s2;
            qDot4 -= beta * s3;
        }
        
        // 4. Integrate quaternion
        q.w += qDot1 * dt;
        q.x += qDot2 * dt;
        q.y += qDot3 * dt;
        q.z += qDot4 * dt;
        
        // 5. Normalize
        normalizeQuaternion(q);
    }
    
    Quaternion getAttitude() const {
        return q;
    }
};
```

### Usage in Main Loop

```cpp
MadgwickFilter filter;
Vector3D gyro_rate, accel;
float dt = 0.01f;  // 100 Hz loop

void loop() {
    // Read sensors
    imu.readGyroscope(gyro_rate.x, gyro_rate.y, gyro_rate.z);  // deg/s
    imu.readAccelerometer(accel.x, accel.y, accel.z);  // G's
    
    // Update filter
    filter.update(gyro_rate, accel, dt);
    
    // Get attitude
    Quaternion attitude = filter.getAttitude();
    EulerAngles euler = quaternionToEulerDegrees(attitude);
    
    // Use for control
    controlDrone(euler, gyro_rate);
}
```

---

## Frame Transformations

### Transform Body-Frame Vector to World-Frame

**Use case:** Convert accelerometer reading (body-frame) to world-frame acceleration

```cpp
Vector3D transformBodyToWorld(const Quaternion& attitude_world_to_body,
                               const Vector3D& body_vector) {
    // attitude_world_to_body = rotation from world to body
    // To transform body -> world, use inverse (conjugate)
    
    Quaternion q_body_to_world = quaternionConjugate(attitude_world_to_body);
    
    return rotateVectorByQuaternion_Matrix(q_body_to_world, body_vector);
}

// Example: gravity check
Vector3D accel_body = imu.getAcceleration();  // [0, 0, 1] when level
Vector3D accel_world = transformBodyToWorld(bno085.getAttitude(), accel_body);
// If level: accel_world ≈ [0, 0, -1] (gravity points down in world frame)
```

### Transform World-Frame Vector to Body-Frame

**Use case:** Convert desired wind direction (world-frame) to body-frame for wind compensation

```cpp
Vector3D transformWorldToBody(const Quaternion& attitude_world_to_body,
                               const Vector3D& world_vector) {
    // attitude_world_to_body already rotates world -> body
    
    return rotateVectorByQuaternion_Matrix(attitude_world_to_body, world_vector);
}
```

### GPS Position + Antenna Offset

**Use case:** Compute true antenna position from GPS (body-frame offset)

```cpp
struct Position3D {
    double lat, lon, alt;
};

Position3D computeAntennaPosition(const Position3D& gps_position,
                                  const Vector3D& antenna_offset_body,
                                  const Quaternion& attitude) {
    // Rotate antenna offset from body to world frame
    Quaternion q_body_to_world = quaternionConjugate(attitude);
    Vector3D antenna_offset_world = rotateVectorByQuaternion_Matrix(
        q_body_to_world, antenna_offset_body);
    
    // Add offset to GPS position
    // Note: this is approximate (assumes small offsets, flat earth)
    Position3D antenna_pos;
    antenna_pos.lat = gps_position.lat + antenna_offset_world.y / 111320.0;  // 1 degree ≈ 111.32 km
    antenna_pos.lon = gps_position.lon + antenna_offset_world.x / (111320.0 * cos(gps_position.lat * M_PI / 180.0));
    antenna_pos.alt = gps_position.alt + antenna_offset_world.z;
    
    return antenna_pos;
}
```

---

## Testing & Validation

### Test 1: Quaternion Normalization

```cpp
void test_quaternionNormalization() {
    Quaternion q(1.5f, 0.3f, 0.4f, 0.2f);  // Not normalized
    
    float mag_before = q.magnitude();
    Serial.print("Before: magnitude = ");
    Serial.println(mag_before);
    
    normalizeQuaternion(q);
    
    float mag_after = q.magnitude();
    Serial.print("After: magnitude = ");
    Serial.println(mag_after);
    
    assert(fabs(mag_after - 1.0f) < 0.001f);
}
```

### Test 2: Identity Rotation

```cpp
void test_identityRotation() {
    Quaternion q_identity(1, 0, 0, 0);
    Vector3D v(1, 2, 3);
    
    Vector3D v_rotated = rotateVectorByQuaternion_Matrix(q_identity, v);
    
    // Should be unchanged
    assert(fabs(v_rotated.x - v.x) < 1e-6);
    assert(fabs(v_rotated.y - v.y) < 1e-6);
    assert(fabs(v_rotated.z - v.z) < 1e-6);
    
    Serial.println("PASS: Identity rotation");
}
```

### Test 3: 90-Degree Rotation

```cpp
void test_90degreeRotation() {
    // Rotate 90° about Z-axis: x -> y
    Quaternion q = axisAngleToQuaternion(Vector3D(0, 0, 1), M_PI / 2);
    Vector3D v(1, 0, 0);
    
    Vector3D v_rotated = rotateVectorByQuaternion_Matrix(q, v);
    
    // Should get [0, 1, 0]
    assert(fabs(v_rotated.x - 0) < 1e-5);
    assert(fabs(v_rotated.y - 1) < 1e-5);
    assert(fabs(v_rotated.z - 0) < 1e-5);
    
    Serial.println("PASS: 90-degree rotation");
}
```

### Test 4: Euler Round-Trip

```cpp
void test_eulerRoundTrip() {
    float roll_in = 15.0f * M_PI / 180;
    float pitch_in = 30.0f * M_PI / 180;
    float yaw_in = 45.0f * M_PI / 180;
    
    // Euler -> Quaternion -> Euler
    Quaternion q = eulerToQuaternion(roll_in, pitch_in, yaw_in);
    EulerAngles euler_out = quaternionToEuler(q);
    
    // Check round-trip accuracy
    assert(fabs(euler_out.roll - roll_in) < 1e-5);
    assert(fabs(euler_out.pitch - pitch_in) < 1e-5);
    assert(fabs(euler_out.yaw - yaw_in) < 1e-5);
    
    Serial.println("PASS: Euler round-trip");
}
```

### Test 5: Quaternion Multiplication Order

```cpp
void test_quaternionMultiplicationOrder() {
    Quaternion q1 = axisAngleToQuaternion(Vector3D(1, 0, 0), M_PI / 2);  // 90° about X
    Quaternion q2 = axisAngleToQuaternion(Vector3D(0, 1, 0), M_PI / 2);  // 90° about Y
    
    // q1 * q2 should NOT equal q2 * q1
    Quaternion r1 = quaternionMultiply(q1, q2);
    Quaternion r2 = quaternionMultiply(q2, q1);
    
    float dot_prod = quaternionDot(r1, r2);
    assert(fabs(dot_prod) < 0.99f);  // Should be different
    
    Serial.println("PASS: Quaternion multiplication non-commutative");
}
```

### Test 6: BNO085 Consistency Check

```cpp
void test_bno085Consistency() {
    readBNO085();
    
    // Check magnitude
    float mag = attitude.magnitude();
    if (mag < 0.95f || mag > 1.05f) {
        Serial.print("ERROR: Bad magnitude = ");
        Serial.println(mag);
        return;
    }
    
    // Convert to Euler
    EulerAngles euler = quaternionToEulerDegrees(attitude);
    
    // Sanity checks
    if (fabs(euler.roll) > 180 || fabs(euler.pitch) > 90 || fabs(euler.yaw) > 180) {
        Serial.println("ERROR: Euler angles out of range!");
        return;
    }
    
    Serial.println("PASS: BNO085 consistency");
}
```

### Automated Test Suite

```cpp
void runAllTests() {
    Serial.println("\n=== QUATERNION TEST SUITE ===\n");
    
    test_quaternionNormalization();
    test_identityRotation();
    test_90degreeRotation();
    test_eulerRoundTrip();
    test_quaternionMultiplicationOrder();
    test_bno085Consistency();
    
    Serial.println("\n=== ALL TESTS COMPLETED ===\n");
}
```

---

## Quick Reference Table

| Operation | Function | Input | Output | Use Case |
|-----------|----------|-------|--------|----------|
| Normalize | `normalizeQuaternion()` | q | q normalized | After arithmetic |
| Multiply | `quaternionMultiply()` | q1, q2 | q1 * q2 | Compose rotations |
| Conjugate | `quaternionConjugate()` | q | q* | Reverse rotation |
| Dot Product | `quaternionDot()` | q1, q2 | float | Similarity, antipodal |
| To Euler | `quaternionToEulerDegrees()` | q | roll, pitch, yaw | Display |
| From Euler | `eulerToQuaternionDegrees()` | roll, pitch, yaw | q | User input |
| From Axis-Angle | `axisAngleToQuaternion()` | axis, angle | q | Known rotation |
| Rotate Vector | `rotateVectorByQuaternion_Matrix()` | q, v | v' | Transform frames |
| Transform Body→World | `transformBodyToWorld()` | q, v_body | v_world | Sensor fusion |
| Transform World→Body | `transformWorldToBody()` | q, v_world | v_body | Control |

---

**End of Document**

All code is pseudocode/C++ — adapt to your specific compiler and numeric library (Arduino, STM32, ROS, etc.). For production code, consider using established libraries like Eigen, Quaternion (by various authors), or the Adafruit quaternion class.
