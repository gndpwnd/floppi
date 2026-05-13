# Auto Orientation: Math & Applications Master Guide

**Status**: Research phase complete  
**Date**: 2026-05-07  
**Scope**: Comprehensive theory for BNO085 IMU + GPS → camera extrinsic calibration  
**Purpose**: Enable absolute orientation computation for drones, cameras, and multi-sensor robotics

---

## The Big Picture

You have:
- **BNO085 IMU**: Outputs quaternion attitude (w, x, y, z) at ~100 Hz + calibration level
- **GPS module** (NEO-M9N capable): Provides latitude, longitude, altitude at 1-10 Hz
- **Camera**: Needs extrinsic calibration (rotation + translation from body frame)

Goal: **Determine camera orientation in world frame to enable 3D reconstruction, geo-tagging, autonomous landing**

---

## Mathematics Pipeline: Sensor → World Coordinates

```
┌─────────────────┐
│   BNO085 IMU    │ Outputs: quaternion (w,x,y,z) = attitude in body frame
└────────┬────────┘
         │
         ├─→ Convert Quaternion → Rotation Matrix
         │   (How: See QUATERNION_REFERENCE.md)
         │
         v
┌──────────────────────┐
│  Body Frame Attitude │ Defines: which direction is "forward, up, right" relative to world
└────────┬─────────────┘
         │
         ├─→ Apply camera extrinsics (fixed offset from body)
         │   (How: See CAMERA_EXTRINSIC_CALIBRATION.md)
         │
         v
┌──────────────────────┐
│  Camera Frame        │ Now know where camera is pointing in world coordinates
└────────┬─────────────┘
         │
         ├─→ Fuse with GPS to know absolute position
         │   (How: See IMU_GPS_SENSOR_FUSION.md)
         │
         v
┌──────────────────────┐
│  World Coordinates   │ Image pixels → 3D world points
└──────────────────────┘
         │
         ├─→ Apply applications
         │   - Autonomous landing (detect, align, descend)
         │   - Geo-tagging (pixel → GPS coordinate)
         │   - 3D reconstruction (multiple frames → 3D model)
         │   - Visual odometry (camera motion → position update)
         │
         v
┌──────────────────────┐
│  Navigation Output   │
└──────────────────────┘
```

---

## Research Documents: What to Read

### Phase 1: Foundation Theory (Read in order)

1. **[QUATERNION_REFERENCE.md](../../../docs/QUATERNION_REFERENCE.md)**
   - **What**: Quaternion math basics
   - **Why**: BNO085 outputs quaternions; rotation matrices are industry standard
   - **Key sections**:
     - Basics: w, x, y, z components
     - Conversions: Quaternion ↔ Rotation Matrix ↔ Euler Angles
     - BNO085 specifics: NWU frame, output format
   - **Time**: ~45 minutes
   - **Next**: Implement quaternion class and rotation functions

2. **[GPS_GEODETIC_COORDINATE_SYSTEMS.md](../../../GPS_GEODETIC_COORDINATE_SYSTEMS.md)**
   - **What**: GPS and coordinate frame theory
   - **Why**: GPS gives lat/long/alt; need to convert to local tangent plane (NED/ENU)
   - **Key sections**:
     - GPS data: What does NMEA output contain?
     - WGS84 → ECEF → NED/ENU transformations
     - Altitude systems: MSL vs HAE vs AGL
     - Magnetic declination: Convert mag heading to true north
   - **Time**: ~60 minutes
   - **Next**: Implement coordinate frame conversions

3. **[IMU_GPS_SENSOR_FUSION.md](../../../docs/IMU_GPS_SENSOR_FUSION.md)**
   - **What**: Kalman filter theory for combined IMU+GPS
   - **Why**: IMU drifts, GPS is slow/noisy; fuse them for optimal estimate
   - **Key sections**:
     - Why fusion works: complementary strengths
     - EKF framework: state vector, predict/update cycle
     - Error sources: BNO085 drift, GPS multipath
     - Asynchronous sensors: how to sync 100 Hz IMU with 1 Hz GPS
   - **Time**: ~90 minutes
   - **Next**: Implement EKF for position/velocity estimation

### Phase 2: Application-Specific (Read as needed)

4. **[CAMERA_EXTRINSIC_CALIBRATION.md](../../../CAMERA_EXTRINSIC_CALIBRATION.md)**
   - **What**: Camera calibration mathematics
   - **When to read**: After understanding quaternions and coordinate frames
   - **Key sections**:
     - Extrinsic parameters: 3×3 rotation + 3×1 translation
     - Transformation chain: Pixel → Camera → Body → World
     - Calibration methods: ArUco markers, self-calibration, GPS+IMU validation
     - Use cases: VIO, autonomous landing, geo-tagging
   - **Time**: ~75 minutes
   - **Next**: Measure/calibrate camera offset from drone body

5. **[DRONE_APPLICATIONS_REFERENCE.md](../../../docs/DRONE_APPLICATIONS_REFERENCE.md)**
   - **What**: Real-world drone applications
   - **When to read**: After understanding fusion and camera calibration
   - **Key sections**:
     - Autonomous landing: detection → alignment → descent
     - Visual odometry: how camera + IMU enables GPS-free navigation
     - Structure-from-Motion: multiple frames → 3D model
     - Geo-tagging: pixel → world coordinates
     - Obstacle avoidance: depth from camera + IMU
   - **Time**: ~60 minutes (skim first, read details for your use case)

### Quick References

- **[GPS_COORDINATE_QUICK_REFERENCE.md](../../../GPS_COORDINATE_QUICK_REFERENCE.md)** — cheat sheet for coordinate conversions
- **[GPS_CHEAT_SHEET.txt](../../../GPS_CHEAT_SHEET.txt)** — printable field card
- **[QUATERNION_MASTER_INDEX.md](../../../docs/QUATERNION_MASTER_INDEX.md)** — navigation guide for quaternion docs
- **[GPS_IMPLEMENTATION_EXAMPLES.py](../../../GPS_IMPLEMENTATION_EXAMPLES.py)** — Python reference implementations

---

## Implementation Roadmap for auto_orientation Project

### Stage 1: Foundation (Weeks 1-2)

**Goal**: Implement quaternion math and coordinate frame conversions

**Tasks**:
1. Create `src/math/quaternion.cpp/h`
   - Quaternion class with operations (multiply, conjugate, normalize)
   - Conversion functions: quaternion → rotation matrix, → Euler angles
   - Reference: QUATERNION_IMPLEMENTATION_GUIDE.md, section "C++ Pseudocode"

2. Create `src/math/coordinates.cpp/h`
   - WGS84 → ECEF conversion (Appendix A of GPS_GEODETIC_COORDINATE_SYSTEMS.md)
   - ECEF → NED/ENU conversion
   - Reference: GPS_IMPLEMENTATION_EXAMPLES.py for validation

3. Extend `src/sensors/bno085.cpp`
   - Already outputs quaternion; add validation checks
   - Convert quaternion → rotation matrix on-demand
   - Add Euler angle output (for visualization/debugging)

4. **Testing**: Unit tests for all math functions
   - Compare against reference implementations (Python/MATLAB)
   - Round-trip tests (quaternion → matrix → quaternion)
   - Example coordinates (Munich, known GPS location)

### Stage 2: GPS Integration (Weeks 2-3)

**Goal**: Incorporate GPS data and establish coordinate frames

**Tasks**:
1. Connect GPS module (NEO-M9N) via UART
   - Parse NMEA sentences (or UBX if using Ublox library)
   - Extract: lat, lon, altitude, accuracy metrics (HDOP/VDOP)
   - Reference: GPS_GEODETIC_COORDINATE_SYSTEMS.md, "GPS Data Outputs"

2. Create `src/navigation/coordinate_frame.cpp/h`
   - Maintain local NED/ENU origin (initialized at first GPS fix)
   - Convert GPS → local frame continuously
   - Track magnetic declination for heading correction
   - Reference: GPS_COORDINATE_QUICK_REFERENCE.md

3. Compute body frame position in world coordinates
   - Input: BNO085 attitude + current GPS position
   - Output: body frame location (x, y, z) in NED/ENU
   - Formula: See CAMERA_EXTRINSIC_CALIBRATION.md, "Transformation Chain"

4. **Testing**: Log sensor data to SD card or serial
   - Verify GPS conversion is smooth (no jumps)
   - Check quaternion → body frame attitude makes sense
   - Walk around with prototype, verify outputs

### Stage 3: Sensor Fusion (Weeks 3-4)

**Goal**: Implement EKF to fuse IMU + GPS

**Tasks**:
1. Create `src/navigation/ekf.cpp/h`
   - 16-dimensional state vector (attitude, velocity, position, accel bias)
   - Predict phase: propagate with IMU at 100 Hz
   - Update phase: correct with GPS at 1 Hz
   - Reference: IMU_GPS_SENSOR_FUSION.md, "Extended Kalman Filter Framework"

2. Implement covariance matrix propagation
   - Process noise (Q): tune based on IMU specs
   - Measurement noise (R): tune based on GPS accuracy
   - Reference: IMU_GPS_SENSOR_FUSION.md, "Error Sources"

3. Handle GPS dropouts
   - Detect when GPS signal lost
   - Continue dead-reckoning with IMU only
   - Limit odometry trust to ~30 seconds without GPS
   - Reference: IMU_GPS_SENSOR_FUSION.md, "Real-World Challenges"

4. **Testing**: Compare fused position vs. raw GPS
   - Fused should be smoother
   - Accuracy should match or exceed raw GPS
   - Test GPS dropout recovery

### Stage 4: Camera Calibration (Weeks 4-5)

**Goal**: Measure camera extrinsics and apply transformations

**Tasks**:
1. **Measure camera offset from IMU**
   - Use calibration frame: place ArUco marker on known surface
   - Rotate drone, observe where marker appears in camera image
   - Compute rotation matrix from observed image points
   - Reference: CAMERA_EXTRINSIC_CALIBRATION.md, "Marker-Based Calibration"

2. Create `src/vision/camera_calibration.cpp/h`
   - Store extrinsic matrix (4×4 homogeneous transformation)
   - Functions to transform points: body frame → camera frame
   - Functions to project 3D world points → image pixels
   - Reference: CAMERA_EXTRINSIC_CALIBRATION.md, "Practical Implementation"

3. Implement full transformation pipeline
   - BNO085 quaternion → body frame attitude (rotation matrix)
   - Fused position (x, y, z) in world frame
   - Apply camera extrinsics
   - Result: camera pose in world coordinates
   - Reference: CAMERA_EXTRINSIC_CALIBRATION.md, "Transformation Chain"

4. **Testing**: Geo-reference image pixels
   - Point camera at known world feature (landmark)
   - Compute where that feature should appear in image
   - Verify camera projection matches actual image
   - Repeat at different altitudes/orientations

### Stage 5: Applications (Weeks 5-8)

Pick one to start:

**Option A: Geo-Tagging**
- Tag image pixels with GPS coordinates
- Reference: DRONE_APPLICATIONS_REFERENCE.md, "Geo-tagging & Pixel-to-World"
- Use case: precision agriculture, inspection

**Option B: Autonomous Landing**
- Detect landing marker, estimate distance, descend
- Reference: DRONE_APPLICATIONS_REFERENCE.md, "Autonomous Landing"
- Use case: drone return-to-home, rescue

**Option C: 3D Reconstruction**
- Record multiple frames with pose estimates
- Run SfM (Structure-from-Motion) post-flight
- Reference: DRONE_APPLICATIONS_REFERENCE.md, "Structure-from-Motion"
- Use case: 3D mapping, volumetric analysis

---

## Key Equations Reference

### 1. Quaternion → Rotation Matrix

```
Given quaternion q = (w, x, y, z), where q·q = 1

Rotation matrix R =
┌                                      ┐
│ 1-2y²-2z²    2xy-2wz     2xz+2wy    │
│ 2xy+2wz      1-2x²-2z²   2yz-2wx    │
│ 2xz-2wy      2yz+2wx     1-2x²-2y²  │
└                                      ┘

This matrix rotates vectors from one frame to another.
```

**Reference**: QUATERNION_REFERENCE.md, "Rotation Matrix Conversion"

### 2. GPS → NED Coordinates

```
1. Convert GPS to ECEF (Earth-Centered, Earth-Fixed)
   x = (R + h) cos(lat) cos(lon)
   y = (R + h) cos(lat) sin(lon)
   z = (R + h - e² R) sin(lat)
   
   where:
   - R = WGS84 radius of curvature
   - h = altitude (meters)
   - lat, lon = latitude, longitude (radians)
   - e = WGS84 eccentricity

2. Compute local NED origin
   origin_ecef = GPS_to_ECEF(origin_lat, origin_lon, origin_alt)

3. Convert any GPS to local NED
   Δ_ecef = ECEF_point - origin_ecef
   Δ_ned = Rot_ecef_to_ned × Δ_ecef
   
   where Rot_ecef_to_ned is a 3×3 matrix computed from origin lat/lon
```

**Reference**: GPS_GEODETIC_COORDINATE_SYSTEMS.md, "ECEF to NED Transformation"

### 3. Body Frame → World Frame

```
Given:
- Attitude quaternion q (from BNO085)
- Position p_world = (x, y, z) in NED (from fused GPS+IMU)
- Point p_body in body frame coordinates

Compute position in world frame:
R_body_to_world = quaternion_to_matrix(q)
p_world_point = p_world + R_body_to_world × p_body
```

**Reference**: QUATERNION_REFERENCE.md, "Frame Transformations"

### 4. Camera Projection

```
Given:
- Camera intrinsics K (3×3 calibration matrix from OpenCV)
- Camera pose in world frame:
  - R_world_to_cam: rotation matrix
  - t_world_to_cam: translation vector
- 3D point p_world in world coordinates

Compute image pixel:
p_cam = R_world_to_cam × p_world + t_world_to_cam
p_homogeneous = K × p_cam
pixel = p_homogeneous[0:2] / p_homogeneous[2]
```

**Reference**: CAMERA_EXTRINSIC_CALIBRATION.md, "Camera Projection"

### 5. EKF Predict (IMU)

```
State vector x = [q, v, p, a_bias]ᵀ (16 dimensions)

Predict:
x_new = f(x_old, u)
- Update quaternion using gyroscope: q_new = q_old * ω_quat(gyro, dt)
- Update velocity: v_new = v_old + (accel - a_bias) × dt + gravity
- Update position: p_new = p_old + v_old × dt + 0.5 × accel × dt²
- a_bias unchanged (constant bias assumption)

Covariance:
P_new = F × P_old × F^T + Q
where F is Jacobian of f, Q is process noise covariance
```

**Reference**: IMU_GPS_SENSOR_FUSION.md, "EKF Framework"

---

## Testing Strategy

### Unit Tests
```cpp
// tests/test_quaternion.cpp
test_quaternion_multiplication();
test_quaternion_to_matrix();
test_quaternion_normalization();

// tests/test_coordinates.cpp
test_gps_to_ecef();
test_ecef_to_ned();
test_magnetic_declination();

// tests/test_camera_projection.cpp
test_world_to_camera();
test_camera_to_pixel();
test_extrinsic_composition();
```

### Integration Tests
```cpp
// tests/integration_test_orientation_pipeline.cpp
- Read BNO085 quaternion
- Convert to rotation matrix
- Apply camera extrinsics
- Verify result makes geometric sense
- Compare against hand-calculated expectations
```

### Field Tests
```
1. Static test (on table):
   - Place known reference marker
   - Point camera at marker
   - Verify projected pixel matches actual marker in image

2. Motion test (walk around with prototype):
   - Move to known GPS location
   - Verify fused position matches
   - Rotate through all axes, verify quaternion tracks motion
   - Temporarily block GPS, verify IMU dead-reckoning

3. Application test (e.g., geo-tagging):
   - Take photo of known landmark
   - Compute where landmark should appear in image
   - Verify matches actual image content
```

---

## File Organization in auto_orientation/

```
auto_orientation/
├── src/
│   ├── main.cpp (existing)
│   ├── config/
│   │   ├── mode.h (existing)
│   │   └── calibration_storage.h (existing)
│   ├── math/                          ← NEW
│   │   ├── quaternion.h
│   │   ├── quaternion.cpp
│   │   ├── coordinates.h
│   │   └── coordinates.cpp
│   ├── navigation/                    ← NEW
│   │   ├── coordinate_frame.h
│   │   ├── coordinate_frame.cpp
│   │   ├── ekf.h
│   │   ├── ekf.cpp
│   │   └── gps_reader.h/cpp (if needed)
│   ├── vision/                        ← NEW
│   │   ├── camera_calibration.h
│   │   └── camera_calibration.cpp
│   ├── output/
│   │   └── sensor_output_manager.cpp (expand for new fields)
│   └── sensors/
│       ├── bno085.h/cpp (existing, extend for camera)
│       └── gps.h/cpp (new)
├── tests/
│   ├── test_quaternion.cpp            ← NEW
│   ├── test_coordinates.cpp           ← NEW
│   ├── test_ekf.cpp                   ← NEW
│   ├── test_camera_projection.cpp     ← NEW
│   ├── integration_test_orientation.cpp ← NEW
│   └── calibration_diagnostic.ino (existing)
└── docs/
    ├── CALIBRATION_IMPLEMENTATION_GUIDE.md (existing)
    ├── MATH_AND_APPLICATIONS_MASTER_GUIDE.md (this file)
    ├── IMPLEMENTATION_CHECKLIST.md (next: create this)
    └── FIELD_TESTING_GUIDE.md (next: create this)
```

---

## JSON Output Format (Extended)

Currently outputs only orientation. Extend to include world position and camera pose:

```json
{
  "timestamp": 1234,
  "orientation": {
    "quaternion": {"w": 0.707, "x": 0, "y": 0, "z": 0.707},
    "euler": {"roll_deg": 0, "pitch_deg": 0, "yaw_deg": 90},
    "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 3}
  },
  "gps": {
    "latitude": 47.360,
    "longitude": 11.180,
    "altitude_m": 520,
    "hdop": 1.2,
    "vdop": 2.5,
    "num_satellites": 12
  },
  "position": {
    "ned": {
      "north_m": 0.0,
      "east_m": 0.0,
      "down_m": 0.0,
      "uncertainty_m": 2.5
    },
    "velocity": {
      "north_mps": 0.1,
      "east_mps": 0.05,
      "down_mps": 0.0
    }
  },
  "camera": {
    "rotation_matrix": [...],
    "position_ned": [0.1, 0.05, -0.2],
    "is_calibrated": true
  }
}
```

---

## Next Steps

1. **Create implementation checklist** (based on Stage 1-5 above)
2. **Create field testing guide** (procedures, expected results)
3. **Start Stage 1 development** (quaternion math library)
4. **Parallel work**: Measure/calibrate camera offset
5. **Document as you code**: Add code examples to research docs

---

## References to Research Documents

All detailed theory in:
- `/home/devel/floppi/GPS_GEODETIC_COORDINATE_SYSTEMS.md` (197 pages equivalent)
- `/home/devel/floppi/docs/QUATERNION_REFERENCE.md` (32 KB)
- `/home/devel/floppi/docs/IMU_GPS_SENSOR_FUSION.md` (53 KB)
- `/home/devel/floppi/CAMERA_EXTRINSIC_CALIBRATION.md` (40+ KB)
- `/home/devel/floppi/docs/DRONE_APPLICATIONS_REFERENCE.md` (51 KB)

Quick references:
- `/home/devel/floppi/GPS_COORDINATE_QUICK_REFERENCE.md` (11 KB)
- `/home/devel/floppi/GPS_IMPLEMENTATION_EXAMPLES.py` (25 KB, working code)
- `/home/devel/floppi/docs/QUATERNION_MASTER_INDEX.md` (14 KB)

**Total research**: ~500 KB, 10,000+ lines, 200+ equations

---

**Status**: Ready for Stage 1 implementation  
**Estimated time to completion**: 8 weeks (5 stages)  
**Validation**: Field tested with GPS + camera + IMU on actual drone/platform  
**Applicability**: Any drone, robot, or camera platform with IMU + GPS

