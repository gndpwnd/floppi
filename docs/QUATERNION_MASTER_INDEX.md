# Quaternion Mathematics for Attitude Representation: Complete Documentation Index

**Purpose:** Central hub for all quaternion-related documentation created for the floppi drone project.  
**Total Documentation:** ~73 KB across 4 files  
**Date:** 2026-05-07  
**Author:** Research & Documentation

---

## Quick Navigation

### If You Want to... → Read This

| Need | File | Length | Read Time |
|------|------|--------|-----------|
| **Understand quaternion theory** | `QUATERNION_REFERENCE.md` | 32 KB | 45 min |
| **Write quaternion code** | `QUATERNION_IMPLEMENTATION_GUIDE.md` | 24 KB | 35 min |
| **Apply to floppi systems** | `QUATERNION_IN_FLOPPI_CONTEXT.md` | 17 KB | 25 min |
| **Quick formulas** | This file (Appendix) | 5 KB | 10 min |

---

## Document Descriptions

### 1. QUATERNION_REFERENCE.md (Primary Theory)

**What:** Complete mathematical reference for quaternion-based 3D rotation

**Contents:**
- Section 1: What are quaternions? (w, x, y, z components, geometric interpretation)
- Section 2: Why better than Euler angles? (Gimbal lock, computational efficiency, stability)
- Section 3: BNO085 quaternion output (What it outputs, reference frame, calibration)
- Section 4: Quaternion ↔ Rotation Matrix conversion (With formulas)
- Section 5: Quaternion ↔ Euler Angles conversion (ZYX convention, both directions)
- Section 6: Quaternion algebra (Multiplication, conjugate, normalization, dot product)
- Section 7: Composing rotations (Multiple rotations, unwinding phenomenon)
- Section 8: Frame transformations (Body ↔ World, with GPS example)
- Section 9: Practical code examples (BNO085, Madgwick, composition, transformations)
- Section 10: Summary & recommendations (Checklist, textbooks, online resources)
- Appendix: Quick reference formulas (All math on 2-3 pages)

**Best for:** Understanding the "why" and "what," not the "how to code it"

**Key insights:**
- Quaternions avoid gimbal lock (Euler angles fail at pitch = ±90°)
- BNO085 outputs w, x, y, z normalized to magnitude ≈ 1.0
- Two quaternions represent same rotation: q and -q (antipodal equivalence)
- ZYX Euler convention: Sequential Z (yaw) → Y (pitch) → X (roll) rotations

---

### 2. QUATERNION_IMPLEMENTATION_GUIDE.md (Practical Code)

**What:** C++/Arduino pseudocode and working implementations for all quaternion operations

**Contents:**
- Section 1: Data structures (Quaternion, Vector3D classes)
- Section 2: Basic operations (normalize, conjugate, multiply, dot product)
- Section 3: Conversions (Quaternion ↔ Euler, axis-angle, etc.)
- Section 4: Rotation operations (Rotate vectors, compose rotations, inverse)
- Section 5: BNO085 integration (Reading, reset handling)
- Section 6: Madgwick filter integration (6DOF implementation, main loop usage)
- Section 7: Frame transformations (Body→World, World→Body, GPS offset)
- Section 8: Testing & validation (6 test functions, automated test suite)
- Quick reference table (All operations in one table)

**Best for:** "Copy-paste ready" code, testing your implementation

**Code style:** C++ with clear variable names, function signatures, examples

**Key functions:**
```cpp
normalizeQuaternion(q)
quaternionMultiply(q1, q2)
quaternionToEuler(q) → [roll, pitch, yaw]
eulerToQuaternion(roll, pitch, yaw) → q
rotateVectorByQuaternion(q, v) → v'
```

---

### 3. QUATERNION_IN_FLOPPI_CONTEXT.md (Integration Guide)

**What:** How to use quaternions specifically in the floppi drone codebase

**Contents:**
- Section 1: Current systems using quaternions
  - BNO085 sensor (auto_orientation/)
  - Madgwick filter (flight_controller/)
- Section 2: How to use for drone control
  - Attitude representation (no gimbal lock)
  - Frame transformations (body → world)
  - Rotation composition (waypoint guidance)
- Section 3: Practical integration examples
  - Replace Euler attitude control with quaternion error
  - GPS + attitude = antenna world position
  - Sensor fusion (blend BNO085 + GPS heading)
- Section 4: Known issues & gotchas
  - BNO085 quaternion magnitude ≠ 1.0
  - Gimbal lock in Euler conversion
  - Unwinding phenomenon (360° rotation bug)
- Section 5: Testing & validation
  - Unit tests (add to flight_controller/tests/)
  - BNO085 integration test
  - Data log comparison (BNO085 vs Madgwick)
- Section 6: Migration path (6-week plan to convert control system)
- Section 7: Files to create/modify (Specific file list)

**Best for:** Deciding what to implement, understanding the current system, planning changes

**Key actions:**
- Create `/flight_controller/src/quaternion_math.h/cpp`
- Modify `/flight_controller/src/control.cpp` for quaternion error
- Add validation checks to `/auto_orientation/src/sensors/bno085.cpp`

---

## How These Documents Relate

```
QUATERNION_REFERENCE.md
  ├─ Explains the theory (what quaternions are, why they work)
  └─ Used by: Anyone learning, writing papers, understanding design

QUATERNION_IMPLEMENTATION_GUIDE.md
  ├─ Takes theory and shows exact C++ code
  ├─ Contains pseudocode for all operations
  └─ Used by: Firmware developers writing the actual code

QUATERNION_IN_FLOPPI_CONTEXT.md
  ├─ Links theory & code to floppi's specific systems
  ├─ Shows where BNO085 and Madgwick are currently used
  ├─ Explains integration points and gotchas
  └─ Used by: Floppi developers planning changes

THIS FILE (MASTER INDEX)
  └─ Quick reference for finding information
```

---

## Quick Answer: Common Questions

### Q1: What is a quaternion?

**Answer:** A 4-component number [w, x, y, z] representing a 3D rotation without gimbal lock.

**Where:** QUATERNION_REFERENCE.md, Section 1

**Visual:**
```
w = scalar (rotation angle component)
x, y, z = vector (rotation axis component)
Constraint: w² + x² + y² + z² = 1 (unit quaternion)
```

---

### Q2: Why use quaternions instead of Euler angles (roll, pitch, yaw)?

**Answer:** 
1. No gimbal lock (works at pitch = ±90°)
2. Faster to compose (multiply vs matrix multiply)
3. Smoother to interpolate
4. Numerically stable

**Where:** QUATERNION_REFERENCE.md, Section 2 + QUATERNION_IN_FLOPPI_CONTEXT.md, Section 2.1

**Trade-off:** Quaternions are less intuitive to humans. Solution: Store as quaternion, display as Euler.

---

### Q3: What does BNO085 output?

**Answer:** Normalized quaternion [w, x, y, z] with magnitude ≈ 1.0, representing absolute orientation (world-to-body rotation). Uses NWU frame (North-West-Up).

**Where:** QUATERNION_REFERENCE.md, Section 3 + QUATERNION_IN_FLOPPI_CONTEXT.md, Section 1.1

**Code file:** `/auto_orientation/src/sensors/bno085.cpp` (lines ~150-170 for Euler extraction)

---

### Q4: How do I convert quaternion to Euler angles?

**Answer:** Use these formulas (ZYX convention):
```
Roll  = atan2(2*(w*x + y*z), 1 - 2*(x² + y²))
Pitch = asin(constrain(2*(w*y - z*x), -1, 1))
Yaw   = atan2(2*(w*z + x*y), 1 - 2*(y² + z²))
```

**Where:** 
- Theory: QUATERNION_REFERENCE.md, Section 5
- Code: QUATERNION_IMPLEMENTATION_GUIDE.md, Section 3 (function `quaternionToEuler`)

**Key detail:** Constrain asin argument to [-1, 1] to avoid NaN

---

### Q5: How do I rotate a vector by a quaternion?

**Answer:** Use the rotation matrix form (most efficient):
```cpp
v' = R(q) * v
where R(q) is the 3×3 rotation matrix derived from q
```

**Where:** 
- Theory: QUATERNION_REFERENCE.md, Section 4
- Code: QUATERNION_IMPLEMENTATION_GUIDE.md, Section 4 (function `rotateVectorByQuaternion_Matrix`)

**Example:** Rotate accelerometer (body frame) to world frame for gravity compensation.

---

### Q6: How do I combine two rotations?

**Answer:** Multiply the quaternions:
```cpp
q_result = q1 * q2  // Apply q2 first, then q1
```

**Where:** 
- Theory: QUATERNION_REFERENCE.md, Section 6-7
- Code: QUATERNION_IMPLEMENTATION_GUIDE.md, Section 4 (function `quaternionMultiply`)

**⚠️ Important:** Quaternion multiplication is **non-commutative** (q1*q2 ≠ q2*q1)

---

### Q7: What is the "unwinding phenomenon"?

**Answer:** Due to antipodal equivalence (q and -q represent the same rotation), naive error calculation q_target * q_current^(-1) might go the long way (360°) instead of short way (90°).

**Where:** QUATERNION_REFERENCE.md, Section 2 + Section 7

**Fix:** Ensure q_target and q_current have same sign of w-component before multiplying.

---

### Q8: How do I integrate quaternions with floppi's current system?

**Answer:** See QUATERNION_IN_FLOPPI_CONTEXT.md, which shows:
- Where BNO085 quaternions are currently read (auto_orientation/)
- Where Madgwick filter runs (flight_controller/)
- How to replace Euler-based attitude control with quaternion-based control
- 6-week migration plan

---

## Key Formulas at a Glance

### Quaternion from Axis-Angle
```
q = [cos(θ/2), n_x*sin(θ/2), n_y*sin(θ/2), n_z*sin(θ/2)]
where n = [n_x, n_y, n_z] is unit rotation axis, θ in radians
```

### Quaternion to Euler (ZYX)
```
Roll  = atan2(2*(qw*qx + qy*qz), 1 - 2*(qx² + qy²))
Pitch = asin(constrain(2*(qw*qy - qz*qx), -1, 1))
Yaw   = atan2(2*(qw*qz + qx*qy), 1 - 2*(qy² + qz²))
```

### Euler to Quaternion (ZYX)
```
φ_h = roll/2, θ_h = pitch/2, ψ_h = yaw/2
cr = cos(φ_h), sr = sin(φ_h)
cp = cos(θ_h), sp = sin(θ_h)
cy = cos(ψ_h), sy = sin(ψ_h)

qw = cr*cp*cy + sr*sp*sy
qx = sr*cp*cy - cr*sp*sy
qy = cr*sp*cy + sr*cp*sy
qz = cr*cp*sy - sr*sp*cy
```

### Quaternion Multiplication
```
q1 * q2 = [
  q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z,
  q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
  q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
  q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w
]
```

### Rotate Vector by Quaternion
```
v' = q * [0, v] * q^*

Efficient matrix form (see QUATERNION_IMPLEMENTATION_GUIDE.md):
v'_x = (1-2(qy²+qz²))*v_x + 2(qxqy-qwqz)*v_y + 2(qxqz+qwqy)*v_z
v'_y = 2(qxqy+qwqz)*v_x + (1-2(qx²+qz²))*v_y + 2(qyqz-qwqx)*v_z
v'_z = 2(qxqz-qwqy)*v_x + 2(qyqz+qwqx)*v_y + (1-2(qx²+qy²))*v_z
```

### Normalize Quaternion
```
q_norm = q / ||q|| = [qw, qx, qy, qz] / sqrt(qw² + qx² + qy² + qz²)
```

### Quaternion Conjugate
```
q^* = [qw, -qx, -qy, -qz]  (inverse for unit quaternions)
```

---

## Related Documentation in Floppi

### Existing References

| File | Content | Relevant Section |
|------|---------|------------------|
| `/docs/flight-controller/math_and_algorithms.md` | Madgwick filter details | Section 2 (Madgwick 6DOF) |
| `/flight_controller/src/imu.cpp` | Madgwick implementation | Lines 198-265 |
| `/auto_orientation/src/sensors/bno085.cpp` | BNO085 driver | Lines 150-170 (Euler conversion) |
| `/docs/flight-controller/sensor_data_pipeline.md` | IMU data flow | Complete sensor chain |
| `/auto_orientation/docs/BNO085_ALGORITHM_AND_REPLICATION.md` | Algorithm analysis | MotionEngine explanation |

---

## Testing Quaternion Code

### Quick Validation Checklist

- [ ] Unit tests pass (Section 5 of QUATERNION_IMPLEMENTATION_GUIDE.md)
- [ ] BNO085 quaternion magnitude ≈ 1.0
- [ ] Quaternion→Euler→Quaternion round-trip accuracy < 0.01°
- [ ] 90° rotation test: [1,0,0] rotated about Z becomes [0,1,0]
- [ ] Identity rotation: v' = v for q = [1,0,0,0]
- [ ] Gimbal lock test: Works correctly at pitch = 90°

### Files to Add

```
/flight_controller/tests/test_quaternion.cpp         (Create)
/auto_orientation/tests/test_bno_quaternion.ino      (Create)
/flight_controller/src/quaternion_math.h             (Create)
/flight_controller/src/quaternion_math.cpp           (Create)
```

---

## Reading Guide by Role

### For Robotics/Aerospace Students

1. Start: QUATERNION_REFERENCE.md (all sections)
2. Then: QUATERNION_IMPLEMENTATION_GUIDE.md (sections 1-4)
3. Resources: Textbooks mentioned in QUATERNION_REFERENCE.md

**Time estimate:** 2-3 hours to grasp concepts + code

---

### For Firmware Engineers

1. Start: QUATERNION_IMPLEMENTATION_GUIDE.md (all sections)
2. Reference: QUATERNION_REFERENCE.md (sections 1, 5, 6, 7 as needed)
3. Apply: QUATERNION_IN_FLOPPI_CONTEXT.md (sections 1-3)

**Time estimate:** 1-2 hours to understand; 1-2 days to implement

---

### For Floppi Developers

1. Start: QUATERNION_IN_FLOPPI_CONTEXT.md (section 1: current systems)
2. Then: QUATERNION_IMPLEMENTATION_GUIDE.md (test section 5.8)
3. For new features: QUATERNION_IN_FLOPPI_CONTEXT.md (sections 2-3)
4. Reference: QUATERNION_REFERENCE.md as needed

**Time estimate:** 30 min to understand current state; 2-4 weeks to migrate to quaternion control

---

### For Control System Designers

1. Start: QUATERNION_REFERENCE.md (sections 1-2, 6-7)
2. Then: QUATERNION_IN_FLOPPI_CONTEXT.md (sections 2.1-2.3, 5.3)
3. For implementation: QUATERNION_IMPLEMENTATION_GUIDE.md (sections 2, 4)

**Time estimate:** 1 hour concepts; 2-3 days to redesign control law

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total pages | 80+ (4 documents) |
| Total size | 73 KB |
| Code examples | 40+ |
| Formulas | 30+ |
| Test cases | 10+ |
| Figures/diagrams | Mermaid flowcharts in REFERENCE.md |
| Textbook references | 4 (Siciliano, Shoemake, Madgwick, etc.) |

---

## Updates & Maintenance

**Last updated:** 2026-05-07  
**Version:** 1.0  
**Planned updates:**
- Add SLERP (spherical linear interpolation) example once tested
- Add Mahony filter alternative documentation
- Add hardware-specific optimizations (ARM NEON, etc.)
- Real-world flight test results

---

## Questions or Issues?

**For theory questions:** See QUATERNION_REFERENCE.md  
**For implementation questions:** See QUATERNION_IMPLEMENTATION_GUIDE.md  
**For floppi-specific questions:** See QUATERNION_IN_FLOPPI_CONTEXT.md  
**For code examples:** See QUATERNION_IMPLEMENTATION_GUIDE.md sections 1-8

**File locations in floppi repo:**
```
/home/devel/floppi/docs/QUATERNION_REFERENCE.md
/home/devel/floppi/docs/QUATERNION_IMPLEMENTATION_GUIDE.md
/home/devel/floppi/docs/QUATERNION_IN_FLOPPI_CONTEXT.md
/home/devel/floppi/docs/QUATERNION_MASTER_INDEX.md (this file)
```

---

**End of Master Index**

Happy quaternion-ing! 🎯
