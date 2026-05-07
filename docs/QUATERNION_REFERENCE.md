# Quaternion Mathematics for Attitude Representation

**Purpose:** Complete mathematical reference for quaternion-based 3D rotation representation, with focus on drone/robotics applications.  
**Version:** 1.0  
**Date:** 2026-05-07  
**Audience:** Firmware engineers, sensor fusion developers, robotics students

---

## Table of Contents

1. [Quaternion Basics](#quaternion-basics)
2. [Why Quaternions Are Better Than Euler Angles](#why-quaternions-are-better)
3. [BNO085 Quaternion Output](#bno085-quaternion-output)
4. [Conversion: Quaternion ↔ Rotation Matrix](#quaternion-rotation-matrix-conversion)
5. [Conversion: Quaternion ↔ Euler Angles](#quaternion-euler-angles)
6. [Quaternion Algebra (Multiplication, Conjugation)](#quaternion-algebra)
7. [Composing Rotations with Quaternion Multiplication](#composing-rotations)
8. [Frame Transformations with Attitude + Position](#frame-transformations)
9. [Practical Code Examples](#practical-code-examples)
10. [Summary & Recommendations](#summary)

---

## 1. Quaternion Basics

### What Is a Quaternion?

A **quaternion** is a 4-component number that represents a 3D rotation without gimbal lock:

```
q = [w, x, y, z]  or  q = w + xi + yj + zk
```

Where:
- **w** = scalar (real) part — represents the rotation angle
- **x, y, z** = vector (imaginary) parts — represents the rotation axis

### The Unit Quaternion Constraint

For a quaternion to represent a valid rotation, it **must be normalized** (unit magnitude):

```
||q||² = w² + x² + y² + z² = 1.0
```

This constraint ensures it lies on a 3-sphere in 4D space.

### Geometric Interpretation

A unit quaternion represents a rotation of **θ** radians about an axis **n = (nx, ny, nz)**:

```
w = cos(θ/2)
x = nx * sin(θ/2)
y = ny * sin(θ/2)
z = nz * sin(θ/2)
```

**Key insight:** The angle is **halved** (θ/2) in the quaternion representation. This is why:
- A rotation of 0° has q = [1, 0, 0, 0]
- A rotation of 180° about Z-axis has q = [0, 0, 0, 1]
- A rotation of 360° has q = [-1, 0, 0, 0]
- A rotation of 720° has q = [1, 0, 0, 0] (same as 0°)

### Antipodal Equivalence (Critical!)

**Two quaternions represent the same rotation:**

```
q = [w, x, y, z]  and  -q = [-w, -x, -y, -z]
```

Both represent the same physical orientation because:

```
θ_1 = 2 * arccos(w)    represents rotation of θ_1 about axis n
θ_2 = 2 * arccos(-w)   represents rotation of (360° - θ_1) about axis -n

Result: identical final orientation
```

This antipodal equivalence is a source of bugs in control systems — see ["Unwinding Phenomenon"](#unwinding) below.

---

## 2. Why Quaternions Are Better Than Euler Angles

### The Euler Angle Problem: Gimbal Lock

**Euler angles** (Roll, Pitch, Yaw) describe rotation as three sequential rotations about fixed or rotating axes. They are intuitive but suffer from **gimbal lock**.

**Gimbal lock** occurs when two rotation axes align, reducing the system from 3 DOF to 2 DOF. For example:

```
At Pitch = ±90°:
  Roll and Yaw become equivalent (cannot control one independently)
```

**Example:** A drone pitched at 90° (vertical nosedive) cannot distinguish between rolling and yawing.

### Quaternion Advantage 1: Global Non-Singularity

Quaternions have **no singularities**. They smoothly represent any rotation, including:
- Pitch = 90° (vertical)
- Pitch = -90° (inverted)
- Multiple full rotations (720°)

### Quaternion Advantage 2: Efficient Computation

Quaternions require **fewer trigonometric operations** than Euler angles:

| Operation | Euler Angles | Quaternions |
|-----------|--------------|------------|
| Extract angles from orientation | 1 atan2, 1 asin, 1 atan2 | 0 (direct) |
| Rotate a vector | 9 multiplications, 6 additions (via matrix) | 16 multiplications (quaternion multiplication) |
| Compose two rotations | Matrix multiply (27 ops) | Quaternion multiply (16 ops) |

### Quaternion Advantage 3: Smooth Interpolation

Quaternions interpolate smoothly (SLERP — Spherical Linear Interpolation):

```
q_interp(t) = slerp(q_1, q_2, t)  where t ∈ [0, 1]
```

Euler angles create jerky non-linear paths when interpolated linearly.

### Quaternion Advantage 4: Numerical Stability

Quaternions maintain numerical stability when composed (multiplied) repeatedly. Euler angles accumulate rounding errors and "gimbal lock" artifacts.

### Trade-off: Quaternions Are Less Intuitive

Humans think in Euler angles (roll, pitch, yaw). Quaternions are abstract. **Solution:** Store/compute as quaternions, **display** as Euler angles.

---

## 3. BNO085 Quaternion Output

### What the BNO085 Outputs

The **BNO085** is an IMU with an on-board processor that runs sensor fusion internally. It outputs:

```
q = [w, x, y, z]
```

This quaternion represents the **absolute orientation** of the sensor in the **world reference frame**.

### The Reference Frame Convention

The BNO085 uses the standard **NWU frame** (North-West-Up):

```
World Frame:
  +X = North (magnetic field direction)
  +Y = West  (perpendicular to north, horizontally)
  +Z = Up    (opposite gravity)
```

The quaternion **q** represents the **rotation from world frame to sensor (body) frame**:

```
q_world_to_body = BNO085.quaternion
```

### Quaternion Property: Magnitude ≈ 1.0

The BNO085 outputs normalized quaternions:

```
||q||² = w² + x² + y² + z² ≈ 1.0
```

If you read a quaternion with magnitude 0.5 or 2.0, either:
1. The sensor is not calibrated
2. There's a communication error
3. The quaternion is being corrupted

**Diagnostic check:**

```cpp
float magnitude = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
if (magnitude < 0.95 || magnitude > 1.05) {
    Serial.println("ERROR: Quaternion not normalized!");
}
```

### Calibration Status

The BNO085 provides **calibration_status** (0-3 on each axis):

| Level | Meaning |
|-------|---------|
| 0 | Unreliable — sensor not calibrated |
| 1 | Low — some calibration, but not confident |
| 2 | Medium — calibration good |
| 3 | High — excellent calibration |

**For accurate orientation:** All three axes should reach level 2-3. The sensor auto-calibrates by:
1. Moving the sensor in figure-8 patterns (magnetometer auto-cal)
2. Rotating through complete orientations (gyro/accel auto-cal)

### From Quaternion to Euler Angles (on BNO085)

The BNO085 outputs raw quaternions, but you often want **Roll, Pitch, Yaw**. Conversion formulas:

```
Given q = [w, x, y, z]:

Roll = atan2(2*(w*x + y*z), 1 - 2*(x² + y²))           [radians, -π to π]
Pitch = asin(2*(w*y - z*x))                             [radians, -π/2 to π/2]
Yaw = atan2(2*(w*z + x*y), 1 - 2*(y² + z²))           [radians, -π to π]

In degrees: multiply by 180/π ≈ 57.3
```

These are the **ZYX Tait-Bryan angles** (intrinsic rotations: Z, then Y, then X).

---

## 4. Quaternion ↔ Rotation Matrix Conversion

### Quaternion to Rotation Matrix

A unit quaternion **q = [w, x, y, z]** represents a rotation. The equivalent **3×3 rotation matrix R** is:

```
       | 1-2(y²+z²)    2(xy-wz)      2(xz+wy)   |
R(q) = | 2(xy+wz)      1-2(x²+z²)    2(yz-wx)   |
       | 2(xz-wy)      2(yz+wx)      1-2(x²+y²) |
```

**Derivation:** A rotation matrix must satisfy R^T * R = I (orthonormality). The quaternion form automatically satisfies this due to the unit constraint.

**Use this formula when you need to:**
- Rotate a vector (multiply R by the vector)
- Convert to formats that expect rotation matrices (some aerospace software)
- Verify quaternion correctness by checking orthonormality

**Example in pseudocode:**

```cpp
struct Quaternion { float w, x, y, z; };

// Rotate a vector by a quaternion via matrix
void quaternionRotateVector(
    const Quaternion& q, 
    float& vx, float& vy, float& vz) {
    
    // Build rotation matrix
    float r11 = 1 - 2*(q.y*q.y + q.z*q.z);
    float r12 = 2*(q.x*q.y - q.w*q.z);
    float r13 = 2*(q.x*q.z + q.w*q.y);
    float r21 = 2*(q.x*q.y + q.w*q.z);
    float r22 = 1 - 2*(q.x*q.x + q.z*q.z);
    float r23 = 2*(q.y*q.z - q.w*q.x);
    float r31 = 2*(q.x*q.z - q.w*q.y);
    float r32 = 2*(q.y*q.z + q.w*q.x);
    float r33 = 1 - 2*(q.x*q.x + q.y*q.y);
    
    // Apply R * v
    float vx_new = r11*vx + r12*vy + r13*vz;
    float vy_new = r21*vx + r22*vy + r23*vz;
    float vz_new = r31*vx + r32*vy + r33*vz;
    
    vx = vx_new;
    vy = vy_new;
    vz = vz_new;
}
```

### Rotation Matrix to Quaternion

Given a **3×3 rotation matrix R**, extract the quaternion **q = [w, x, y, z]**:

**Numerically stable method** (Shepperd's method):

```
Compute trace T = R[0,0] + R[1,1] + R[2,2]

If T > 0:
    S = 0.5 / sqrt(T + 1.0)
    w = 0.25 / S
    x = (R[2,1] - R[1,2]) * S
    y = (R[0,2] - R[2,0]) * S
    z = (R[1,0] - R[0,1]) * S

Else if R[0,0] > R[1,1] and R[0,0] > R[2,2]:
    S = 2.0 * sqrt(1.0 + R[0,0] - R[1,1] - R[2,2])
    w = (R[2,1] - R[1,2]) / S
    x = 0.25 * S
    y = (R[0,1] + R[1,0]) / S
    z = (R[0,2] + R[2,0]) / S

... (similar logic for other cases)
```

**Why this method?** The naive formula can produce NaN when determinant is near zero. Shepperd's method chooses the denominator with largest magnitude to avoid division by very small numbers.

---

## 5. Quaternion ↔ Euler Angles

### Quaternion to Euler Angles (3-2-1 ZYX Convention)

Given quaternion **q = [w, x, y, z]**, extract **Roll, Pitch, Yaw** (ZYX order):

```
Roll  = atan2(2*(w*x + y*z), 1 - 2*(x² + y²))       [rad, ±π]
Pitch = asin(constrain(2*(w*y - z*x), -1, 1))      [rad, ±π/2]
Yaw   = atan2(2*(w*z + x*y), 1 - 2*(y² + z²))      [rad, ±π]

Convert to degrees: * 180/π
```

**Why the atan2(numerator, denominator) form?**
- Gives correct angle and quadrant (not just 0° to 180°)
- Avoids division by zero (handled internally by atan2)

**Why constrain the asin argument?**
- Floating-point errors can make the argument slightly > 1 or < -1
- asin(1.1) returns NaN, breaking the entire attitude calculation
- Constraining to [-1, 1] gives valid pitch at cost of tiny error

**Code:**

```cpp
float roll = atan2(2*(q.w*q.x + q.y*q.z), 1 - 2*(q.x*q.x + q.y*q.y));
float pitch = asin(constrain(2*(q.w*q.y - q.z*q.x), -1.0f, 1.0f));
float yaw = atan2(2*(q.w*q.z + q.x*q.y), 1 - 2*(q.y*q.y + q.z*q.z));

// Convert to degrees
roll_deg = roll * 57.29577951f;   // 180/π
pitch_deg = pitch * 57.29577951f;
yaw_deg = yaw * 57.29577951f;
```

### Euler Angles to Quaternion

Given **Roll (φ), Pitch (θ), Yaw (ψ)** in radians, compute **q = [w, x, y, z]**:

```
Using half angles:
φ_h = φ/2, θ_h = θ/2, ψ_h = ψ/2

cos_φ = cos(φ_h), sin_φ = sin(φ_h)
cos_θ = cos(θ_h), sin_θ = sin(θ_h)
cos_ψ = cos(ψ_h), sin_ψ = sin(ψ_h)

w = cos_φ * cos_θ * cos_ψ + sin_φ * sin_θ * sin_ψ
x = sin_φ * cos_θ * cos_ψ - cos_φ * sin_θ * sin_ψ
y = cos_φ * sin_θ * cos_ψ + sin_φ * cos_θ * sin_ψ
z = cos_φ * cos_θ * sin_ψ - sin_φ * sin_θ * cos_ψ
```

**Code:**

```cpp
void eulerToQuaternion(float roll, float pitch, float yaw,
                       float& qw, float& qx, float& qy, float& qz) {
    float roll_h = roll / 2.0f;
    float pitch_h = pitch / 2.0f;
    float yaw_h = yaw / 2.0f;
    
    float cos_r = cos(roll_h), sin_r = sin(roll_h);
    float cos_p = cos(pitch_h), sin_p = sin(pitch_h);
    float cos_y = cos(yaw_h), sin_y = sin(yaw_h);
    
    qw = cos_r * cos_p * cos_y + sin_r * sin_p * sin_y;
    qx = sin_r * cos_p * cos_y - cos_r * sin_p * sin_y;
    qy = cos_r * sin_p * cos_y + sin_r * cos_p * sin_y;
    qz = cos_r * cos_p * sin_y - sin_r * sin_p * cos_y;
}
```

### When to Use Each Representation

| Representation | Best For | Avoid When |
|---|---|---|
| **Quaternion** | Storage, rotation, composition, sensor fusion | Human visualization, intuition |
| **Euler Angles** | User interface, tuning, understanding, control (pitch/roll setpoints) | Computing rotations, gimbal lock situations (pitch ≈ 90°) |
| **Rotation Matrix** | Rotating vectors, transformations, linear algebra | Storage (redundant, 9 numbers), composition (more multiplications) |

---

## 6. Quaternion Algebra

### Quaternion Multiplication (Non-Commutative!)

To compose two rotations represented by quaternions **q₁** and **q₂**, multiply them:

```
q_result = q₁ * q₂  (NOT the same as q₂ * q₁)
```

The product represents the combined rotation: **first apply q₂, then apply q₁**.

**Formula:**

```
Given:
  q₁ = [w₁, x₁, y₁, z₁]
  q₂ = [w₂, x₂, y₂, z₂]

q₁ * q₂ = [
    w₁*w₂ - x₁*x₂ - y₁*y₂ - z₁*z₂,
    w₁*x₂ + x₁*w₂ + y₁*z₂ - z₁*y₂,
    w₁*y₂ - x₁*z₂ + y₁*w₂ + z₁*x₂,
    w₁*z₂ + x₁*y₂ - y₁*x₂ + z₁*w₂
]
```

**Code (16 multiplications, 12 additions):**

```cpp
void quaternionMultiply(
    float w1, float x1, float y1, float z1,
    float w2, float x2, float y2, float z2,
    float& w_out, float& x_out, float& y_out, float& z_out) {
    
    w_out = w1*w2 - x1*x2 - y1*y2 - z1*z2;
    x_out = w1*x2 + x1*w2 + y1*z2 - z1*y2;
    y_out = w1*y2 - x1*z2 + y1*w2 + z1*x2;
    z_out = w1*z2 + x1*y2 - y1*x2 + z1*w2;
}
```

### Quaternion Conjugate

The **conjugate** (or **inverse**) of a quaternion **q = [w, x, y, z]** is:

```
q* = q^(-1) = [w, -x, -y, -z]   (for unit quaternions)

Property: q * q* = q* * q = [1, 0, 0, 0]  (identity rotation)
```

**Use:** To reverse a rotation, multiply by the conjugate.

**Code:**

```cpp
void quaternionConjugate(float& qw, float& qx, float& qy, float& qz) {
    qx = -qx;
    qy = -qy;
    qz = -qz;
    // qw unchanged
}
```

### Quaternion Normalization

After numerical operations, a quaternion may drift from unit length. Re-normalize:

```
q_normalized = q / ||q||

Where ||q|| = sqrt(w² + x² + y² + z²)
```

**Code:**

```cpp
void quaternionNormalize(float& qw, float& qx, float& qy, float& qz) {
    float mag = sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
    if (mag < 1e-6) {
        // Handle zero magnitude (shouldn't happen)
        qw = 1.0; qx = qy = qz = 0.0;
        return;
    }
    float inv_mag = 1.0 / mag;
    qw *= inv_mag;
    qx *= inv_mag;
    qy *= inv_mag;
    qz *= inv_mag;
}
```

### Dot Product (for interpolation/blending)

The dot product of two quaternions measures how aligned they are:

```
dot(q₁, q₂) = w₁*w₂ + x₁*x₂ + y₁*y₂ + z₁*z₂
```

Used in SLERP (Spherical Linear Interpolation) and for detecting antipodal equivalence:

```
If dot < 0: one quaternion is the antipode of the other
           Negate one to avoid the long way around the sphere
```

---

## 7. Composing Rotations with Quaternion Multiplication

### Example: Three Sequential Rotations

Suppose you want to apply three rotations in sequence:
1. **R₁**: 30° rotation about X-axis
2. **R₂**: 45° rotation about Z-axis
3. **R₃**: 20° rotation about Y-axis

**Steps:**

```
1. Convert each to quaternion:
   q₁ = quaternionFromAxisAngle([1, 0, 0], 30°*π/180)
   q₂ = quaternionFromAxisAngle([0, 0, 1], 45°*π/180)
   q₃ = quaternionFromAxisAngle([0, 1, 0], 20°*π/180)

2. Compose (apply in reverse order for intuitive "first-to-last"):
   q_result = q₃ * q₂ * q₁

3. This quaternion now represents the combined rotation
   (first apply R₁, then R₂, then R₃)
```

**Code:**

```cpp
float ax, ay, az;   // Axis (should be unit length)
float angle;        // Angle in radians

// Quaternion from axis-angle
float half_angle = angle / 2.0f;
float sin_h = sin(half_angle);

qw = cos(half_angle);
qx = ax * sin_h;
qy = ay * sin_h;
qz = az * sin_h;

// Compose three rotations: q_result = q3 * q2 * q1
float qtemp_w, qtemp_x, qtemp_y, qtemp_z;
quaternionMultiply(q2.w, q2.x, q2.y, q2.z, q1.w, q1.x, q1.y, q1.z,
                   qtemp_w, qtemp_x, qtemp_y, qtemp_z);
quaternionMultiply(q3.w, q3.x, q3.y, q3.z, qtemp_w, qtemp_x, qtemp_y, qtemp_z,
                   q_result.w, q_result.x, q_result.y, q_result.z);
```

### The "Unwinding Phenomenon" {#unwinding}

**Critical bug:** Due to antipodal equivalence (q and -q represent the same rotation), naive quaternion multiplication can cause unexpected large rotations.

**Example:**

```
Current attitude: q_current = [0.7, 0.1, 0.2, 0.66]
Target attitude: q_target = [-0.7, -0.1, -0.2, -0.66]  (same physical rotation!)

Error quaternion (naive):
q_error = q_target * conjugate(q_current)

This gives a rotation that goes the LONG way around (360° - shortest_path).

Fix: Ensure target and current have the same sign of w-component:
if (dot(q_current, q_target) < 0) {
    q_target.w = -q_target.w;
    q_target.x = -q_target.x;
    q_target.y = -q_target.y;
    q_target.z = -q_target.z;
}
```

---

## 8. Frame Transformations with Attitude + Position

### The Problem: Body-Frame to World-Frame

Drones operate with sensor measurements in the **body frame** (accelerometer, gyroscope axes aligned with the aircraft). But control and navigation happen in the **world frame** (North-East-Down or similar).

**Given:**
- **Attitude quaternion:** q = rotation from world to body frame
- **Position in world frame:** P_world = [x, y, z]
- **Vector in body frame:** V_body = [Bx, By, Bz] (e.g., accelerometer reading)

**Want:**
- **Vector in world frame:** V_world = ?

### Solution: Rotate Using Quaternion

To rotate a vector using a quaternion, use the formula:

```
V_world = q * V_body_as_quaternion * q*

Where:
  V_body_as_quaternion = [0, Bx, By, Bz]    (pure quaternion, w=0)
  q* = conjugate of q = [qw, -qx, -qy, -qz]
```

**Efficient implementation (no full quaternion multiplication needed):**

```cpp
void rotateVectorByQuaternion(
    float qw, float qx, float qy, float qz,
    float& vx, float& vy, float& vz) {
    
    // v' = q * v * q*
    // This is equivalent to v' = R(q) * v where R is the rotation matrix
    
    // Option 1: Build rotation matrix and multiply (shown above)
    // Option 2: Direct quaternion rotation (more efficient)
    
    // Compute 2*q_vector ⊗ q_scalar terms
    float x = 2.0f * (qw * vy + qy * vz - qz * vy);  // Wait, let me recalculate...
    
    // Actually, the most efficient form for rotating v by q:
    float w2 = qw * qw;
    float x2 = qx * qx;
    float y2 = qy * qy;
    float z2 = qz * qz;
    float wx2 = 2 * qw * qx;
    float wy2 = 2 * qw * qy;
    float wz2 = 2 * qw * qz;
    float xy2 = 2 * qx * qy;
    float xz2 = 2 * qx * qz;
    float yz2 = 2 * qy * qz;
    
    float vx_rot = (w2 + x2 - y2 - z2) * vx + (xy2 - wz2) * vy + (xz2 + wy2) * vz;
    float vy_rot = (xy2 + wz2) * vx + (w2 - x2 + y2 - z2) * vy + (yz2 - wx2) * vz;
    float vz_rot = (xz2 - wy2) * vx + (yz2 + wx2) * vy + (w2 - x2 - y2 + z2) * vz;
    
    vx = vx_rot;
    vy = vy_rot;
    vz = vz_rot;
}
```

### Example: Transform Accelerometer to World Frame

**Scenario:** The drone is tilted 30° roll, 20° pitch. The accelerometer reads [0, 0, 1] G (gravity in body frame).

```
Step 1: Get attitude quaternion from IMU (BNO085 or Madgwick filter)
  q = [w, x, y, z]

Step 2: Rotate accelerometer vector
  a_body = [0, 0, 1]   (gravity reading in body frame)
  a_world = rotate(q, a_body)

Step 3: a_world should be approximately [0, 0, -1]
  (gravity points DOWN in world frame, -Z direction)
  
If a_world is something else, the drone has rotated!
```

**Code (practical drone example):**

```cpp
// Assume bno085 has quaternion output
float qw = bno.quaternion.w;
float qx = bno.quaternion.x;
float qy = bno.quaternion.y;
float qz = bno.quaternion.z;

// Body-frame accelerometer reading
float ax_body = imu.accelX;
float ay_body = imu.accelY;
float az_body = imu.accelZ;

// Rotate to world frame
rotateVectorByQuaternion(qw, qx, qy, qz, ax_body, ay_body, az_body);
// Now ax_body, ay_body, az_body are in world frame

// Expected: az_body ≈ -1.0 G (gravity)
// If ax_body and ay_body are non-zero, drone is tilted
```

### GPS Position + Attitude = World Position of Body-Frame Points

**Application:** Computing position of GPS antenna relative to drone's center-of-mass.

**Given:**
- **Drone GPS position:** P_gps_world = [x_gps, y_gps, z_gps] (latitude, longitude, altitude)
- **Antenna offset in body frame:** V_antenna_body = [0.1, 0, 0.05]  (10 cm forward, 5 cm up)
- **Drone attitude:** q = [qw, qx, qy, qz]

**Want:** Antenna position in world frame

```
Step 1: Rotate antenna offset from body to world frame
  V_antenna_world = rotate(q, V_antenna_body)

Step 2: Add to GPS position
  P_antenna_world = P_gps_world + V_antenna_world
```

**Code:**

```cpp
float ant_offset_body[3] = {0.1, 0, 0.05};  // 10cm forward, 5cm up
float ant_offset_world[3];
ant_offset_world[0] = ant_offset_body[0];
ant_offset_world[1] = ant_offset_body[1];
ant_offset_world[2] = ant_offset_body[2];

// Rotate to world frame
rotateVectorByQuaternion(q.w, q.x, q.y, q.z,
                         ant_offset_world[0], ant_offset_world[1], ant_offset_world[2]);

// Add to GPS position
float ant_pos_world[3];
ant_pos_world[0] = gps.latitude + ant_offset_world[0];
ant_pos_world[1] = gps.longitude + ant_offset_world[1];
ant_pos_world[2] = gps.altitude + ant_offset_world[2];
```

---

## 9. Practical Code Examples

### Example 1: BNO085 Quaternion to Euler Angles

```cpp
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x bno08x;

void setup() {
    Serial.begin(115200);
    if (!bno08x.begin_I2C()) {
        Serial.println("BNO085 not found!");
        while (1);
    }
    bno08x.enableReport(SH2_ROTATION_VECTOR);
}

void loop() {
    if (bno08x.getSensorEvent()) {
        float qw = bno08x.quaternion.w;
        float qx = bno08x.quaternion.x;
        float qy = bno08x.quaternion.y;
        float qz = bno08x.quaternion.z;
        
        // Quaternion to Euler
        float roll = atan2(2*(qw*qx + qy*qz), 1 - 2*(qx*qx + qy*qy));
        float pitch = asin(constrain(2*(qw*qy - qz*qx), -1.0f, 1.0f));
        float yaw = atan2(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz));
        
        // Convert to degrees
        float roll_deg = roll * 57.29577951f;
        float pitch_deg = pitch * 57.29577951f;
        float yaw_deg = yaw * 57.29577951f;
        
        Serial.print("Roll: "); Serial.print(roll_deg);
        Serial.print(" Pitch: "); Serial.print(pitch_deg);
        Serial.print(" Yaw: "); Serial.println(yaw_deg);
    }
}
```

### Example 2: Madgwick Filter (6DOF IMU + Gyro)

```cpp
// From floppi codebase: flight_controller/src/imu.cpp
void Madgwick6DOF(float gx, float gy, float gz, float ax, float ay, float az, float invSampleFreq) {
    // Convert gyroscope to radians/sec
    gx *= 0.0174533;
    gy *= 0.0174533;
    gz *= 0.0174533;

    // Quaternion derivative from gyroscope (gyro-only integration)
    float qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    float qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    float qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    float qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    // Accelerometer correction via gradient descent
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        // Normalize accelerometer
        float recipNorm = 1.0f / sqrt(ax*ax + ay*ay + az*az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Compute gradient (objective function error)
        float _2q0 = 2.0f * q0, _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3;
        float _4q0 = 4.0f * q0, _4q1 = 4.0f * q1, _4q2 = 4.0f * q2;
        float _8q1 = 8.0f * q1, _8q2 = 8.0f * q2;
        float q0q0 = q0*q0, q1q1 = q1*q1, q2q2 = q2*q2, q3q3 = q3*q3;

        float s0 = _4q0*q2q2 + _2q2*ax + _4q0*q1q1 - _2q1*ay;
        float s1 = _4q1*q3q3 - _2q3*ax + 4.0f*q0q0*q1 - _2q0*ay - _4q1 + _8q1*q1q1 + _8q1*q2q2 + _4q1*az;
        float s2 = 4.0f*q0q0*q2 + _2q0*ax + _4q2*q3q3 - _2q3*ay - _4q2 + _8q2*q1q1 + _8q2*q2q2 + _4q2*az;
        float s3 = 4.0f*q1q1*q3 - _2q1*ax + 4.0f*q2q2*q3 - _2q2*ay;

        // Normalize gradient
        recipNorm = 1.0f / sqrt(s0*s0 + s1*s1 + s2*s2 + s3*s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        // Apply correction
        qDot1 -= MADGWICK_BETA * s0;
        qDot2 -= MADGWICK_BETA * s1;
        qDot3 -= MADGWICK_BETA * s2;
        qDot4 -= MADGWICK_BETA * s3;
    }

    // Integrate quaternion
    q0 += qDot1 * invSampleFreq;
    q1 += qDot2 * invSampleFreq;
    q2 += qDot3 * invSampleFreq;
    q3 += qDot4 * invSampleFreq;

    // Normalize
    float recipNorm = 1.0f / sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;

    // Convert to Euler angles
    roll_IMU = atan2(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.2957795;
    pitch_IMU = asin(-2.0f*(q1*q3 - q0*q2)) * 57.2957795;
    yaw_IMU = atan2(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.2957795;
}
```

### Example 3: Compose Two Rotations

```cpp
struct Quaternion {
    float w, x, y, z;
    
    // Multiply: this * other
    Quaternion multiply(const Quaternion& other) const {
        return Quaternion{
            w*other.w - x*other.x - y*other.y - z*other.z,
            w*other.x + x*other.w + y*other.z - z*other.y,
            w*other.y - x*other.z + y*other.w + z*other.x,
            w*other.z + x*other.y - y*other.x + z*other.w
        };
    }
    
    // Conjugate (inverse for unit quaternions)
    Quaternion conjugate() const {
        return Quaternion{w, -x, -y, -z};
    }
    
    // Normalize to unit length
    void normalize() {
        float mag = sqrt(w*w + x*x + y*y + z*z);
        if (mag < 1e-6) {
            w = 1; x = 0; y = 0; z = 0;
            return;
        }
        float inv_mag = 1.0f / mag;
        w *= inv_mag; x *= inv_mag; y *= inv_mag; z *= inv_mag;
    }
};

// Example: Apply first a 45° rotation about Z, then a 30° rotation about X
int main() {
    // Create rotation quaternions
    auto rot_z45 = Quaternion{
        cos(45.0f * 3.14159f / 180.0f / 2.0f),  // w
        0,                                        // x
        0,                                        // y
        sin(45.0f * 3.14159f / 180.0f / 2.0f)   // z
    };
    
    auto rot_x30 = Quaternion{
        cos(30.0f * 3.14159f / 180.0f / 2.0f),
        sin(30.0f * 3.14159f / 180.0f / 2.0f),
        0,
        0
    };
    
    // Compose: apply Z rotation first, then X
    auto rot_combined = rot_x30.multiply(rot_z45);
    rot_combined.normalize();
    
    printf("Combined rotation: w=%f x=%f y=%f z=%f\n", 
           rot_combined.w, rot_combined.x, rot_combined.y, rot_combined.z);
    
    return 0;
}
```

---

## 10. Summary & Recommendations

### Key Takeaways

| Concept | Key Point |
|---------|-----------|
| **Quaternion** | 4-component [w,x,y,z] that represents 3D rotation without gimbal lock. Must satisfy w² + x² + y² + z² = 1 |
| **Better than Euler** | No singularities, efficient computation, smooth interpolation, numerically stable |
| **BNO085 Output** | Provides normalized quaternion (magnitude ≈ 1.0) representing world-to-body rotation. Absolute orientation (not drift). |
| **To Euler** | Use atan2 and asin formulas. Include constraint on asin to avoid NaN. |
| **From Euler** | Use half-angle formulas with cos/sin of φ/2, θ/2, ψ/2 |
| **To Rotation Matrix** | Use 3×3 formula with quaternion components. Matrix must be orthonormal. |
| **Multiplication** | q₁ * q₂ composes rotations (non-commutative!). 16 multiplications, 12 additions. |
| **Inverse** | Conjugate q* reverses rotation. For unit quaternions: q * q* = identity |
| **Unwinding** | Antipodal quaternions -q and q are identical. Check dot product sign to avoid long rotations. |
| **Vector Rotation** | To rotate vector v by quaternion q: v' = q * [0, vx, vy, vz] * q* OR use rotation matrix |
| **Frame Transform** | Rotate body-frame vectors using attitude quaternion to get world-frame vectors. |

### Best Practices for Drones

1. **Store/Compute:** Use quaternions internally (sensor fusion, control, composition)
2. **Display/Tune:** Convert to Euler angles for human visualization
3. **Always Normalize:** After any computation, normalize the quaternion
4. **Check Magnitude:** Validate ||q|| ≈ 1.0 to catch errors
5. **Use BNO085:** It handles sensor fusion automatically. Preferred over manual Madgwick on resource-constrained systems
6. **Avoid Euler for Control:** Don't directly control yaw rate from Euler yaw (singularity at pitch = 90°)
7. **Use Quaternion Error:** For attitude control, compute error as quaternion (not Euler angle difference)
8. **SLERP for Smooth Paths:** When interpolating between two attitude waypoints, use spherical linear interpolation, not linear Euler interpolation

### Implementation Checklist

- [ ] Understand quaternion constraint (w² + x² + y² + z² = 1)
- [ ] Test quaternion-to-Euler conversion with known inputs (identity, 90° rotations)
- [ ] Test Euler-to-quaternion round-trip (convert back and forth, should match)
- [ ] Implement quaternion multiplication and test with axis-angle examples
- [ ] Validate BNO085 quaternion magnitude ≈ 1.0 on startup
- [ ] Test frame transformation (body accel to world accel) with manual rotation
- [ ] Log quaternions + Euler angles side-by-side to verify consistency
- [ ] Check for gimbal lock workarounds in existing Madgwick filter

### Further Reading

**Textbooks:**
- "Robotics: Modelling, Planning and Control" by Siciliano, Sciavicco, Villani, Oriolo (Ch. 2: Kinematics and Geometry)
- "Quaternion Calculus and Fast Animation" by Ken Shoemake (classic graphics reference)

**Papers:**
- Madgwick et al., "An Efficient Orientation Filter for Inertial and Inertial/Magnetic Sensor Arrays" (2010) — the Madgwick filter paper
- Shepperd, "Quaternion from Rotation Matrix" — numerically stable conversion

**Online Resources:**
- GeoGebra 3D Quaternion visualization: [https://www.geogebra.org](https://www.geogebra.org)
- 3Blue1Brown "Quaternions and 3D Rotations": https://www.youtube.com/watch?v=zjMuIxRvygQ

---

## Appendix: Quick Reference Formulas

### Quaternion from Axis-Angle

```
q = [cos(θ/2), nx*sin(θ/2), ny*sin(θ/2), nz*sin(θ/2)]
where [nx, ny, nz] is unit rotation axis, θ is angle in radians
```

### Quaternion to Axis-Angle

```
θ = 2 * acos(w)
[nx, ny, nz] = [x, y, z] / sin(θ/2)  (if sin(θ/2) ≠ 0, else [1, 0, 0])
```

### Normalize Quaternion

```
q_norm = q / ||q|| = [w, x, y, z] / sqrt(w² + x² + y² + z²)
```

### Quaternion Conjugate

```
q* = [w, -x, -y, -z]
```

### Quaternion Dot Product

```
dot(q₁, q₂) = w₁w₂ + x₁x₂ + y₁y₂ + z₁z₂
```

### Check Antipodal

```
If dot(q₁, q₂) < 0, then q₂ is the antipode of q₁
Solution: negate one quaternion
```

### Quaternion to Euler (ZYX)

```
Roll  = atan2(2(qwqx + qyqz), 1 - 2(qx² + qy²))
Pitch = asin(constrain(2(qwqy - qzqx), -1, 1))
Yaw   = atan2(2(qwqz + qxqy), 1 - 2(qy² + qz²))
```

### Euler to Quaternion (ZYX)

```
φ_h = roll/2, θ_h = pitch/2, ψ_h = yaw/2
qw = cos(φ_h)*cos(θ_h)*cos(ψ_h) + sin(φ_h)*sin(θ_h)*sin(ψ_h)
qx = sin(φ_h)*cos(θ_h)*cos(ψ_h) - cos(φ_h)*sin(θ_h)*sin(ψ_h)
qy = cos(φ_h)*sin(θ_h)*cos(ψ_h) + sin(φ_h)*cos(θ_h)*sin(ψ_h)
qz = cos(φ_h)*cos(θ_h)*sin(ψ_h) - sin(φ_h)*sin(θ_h)*cos(ψ_h)
```

### Rotate Vector by Quaternion

```
v' = q * [0, v] * q*

Efficient matrix form:
v'x = (1-2(qy²+qz²))*vx + 2(qxqy-qwqz)*vy + 2(qxqz+qwqy)*vz
v'y = 2(qxqy+qwqz)*vx + (1-2(qx²+qz²))*vy + 2(qyqz-qwqx)*vz
v'z = 2(qxqz-qwqy)*vx + 2(qyqz+qwqx)*vy + (1-2(qx²+qy²))*vz
```

### Quaternion Multiplication

```
q₁ * q₂ = [w₁w₂ - x₁x₂ - y₁y₂ - z₁z₂,
           w₁x₂ + x₁w₂ + y₁z₂ - z₁y₂,
           w₁y₂ - x₁z₂ + y₁w₂ + z₁x₂,
           w₁z₂ + x₁y₂ - y₁x₂ + z₁w₂]
```

---

**End of Document**

For questions or corrections, see the BNO085 implementation in `/home/devel/floppi/auto_orientation/src/sensors/bno085.cpp` and the Madgwick filter in `/home/devel/floppi/flight_controller/src/imu.cpp`.
