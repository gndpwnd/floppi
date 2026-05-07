# Auto Orientation: Implementation Checklist

**Project**: Extend BNO085 + GPS data to camera extrinsic calibration  
**Status**: Ready for Phase 1  
**Last Updated**: 2026-05-07

---

## Phase 1: Foundation Math Library (Weeks 1-2)

### Quaternion Implementation

- [ ] Create `src/math/quaternion.h`
  - [ ] Struct with w, x, y, z members
  - [ ] Constructor from components
  - [ ] Constructor from rotation matrix
  - [ ] Constructor from Euler angles (roll, pitch, yaw)
  - [ ] Getter: `magnitude()`
  - [ ] Method: `normalize()`
  - [ ] Method: `conjugate()`

- [ ] Create `src/math/quaternion.cpp`
  - [ ] Implement all getters/methods
  - [ ] Implement quaternion multiplication (q1 * q2)
  - [ ] Implement quaternion-vector rotation: `q * v`
  - [ ] Add validation: check magnitude ≈ 1.0 (with tolerance)

- [ ] Create `src/math/quaternion_conversions.h/cpp`
  - [ ] `quaternion_to_matrix(q)` → 3×3 rotation matrix
    - Test: Compare against hand-calculated values
    - Test: Round-trip (quaternion → matrix → quaternion)
  - [ ] `quaternion_to_euler(q)` → roll, pitch, yaw (in degrees and radians)
    - Test: Verify against MATLAB/Python reference
  - [ ] `euler_to_quaternion(roll, pitch, yaw)` → quaternion
  - [ ] `matrix_to_quaternion(R)` → quaternion (handle numerical stability)

- [ ] Unit tests: `tests/test_quaternion.cpp`
  - [ ] Test normalization
  - [ ] Test multiplication associativity: (q1*q2)*q3 = q1*(q2*q3)
  - [ ] Test vector rotation commutes with matrix multiply
  - [ ] Test identity quaternion (1, 0, 0, 0)
  - [ ] Test inverse: q * conj(q) = identity
  - [ ] Test round-trip conversions
  - [ ] Test with known angles (90° rotations, etc.)

### Coordinate Frame Implementation

- [ ] Create `src/math/coordinates.h`
  - [ ] Constants: WGS84 parameters (a, b, e²)
  - [ ] Struct `GPS_Data`: lat, lon, alt_m, hdop, vdop
  - [ ] Struct `LocalFrame`: ned or enu coordinates (x_m, y_m, z_m)
  - [ ] Function: `gps_to_ecef(lat, lon, alt)` → (x, y, z) ECEF
  - [ ] Function: `ecef_to_gps(x, y, z)` → (lat, lon, alt)
  - [ ] Function: `ecef_to_ned(ecef_point, ned_origin_ecef, origin_lat)` → (n, e, d)
  - [ ] Function: `ned_to_ecef(ned_point, ned_origin_ecef, origin_lat)` → (x, y, z)

- [ ] Create `src/math/coordinates.cpp`
  - [ ] Implement all conversion functions
  - [ ] Add comments showing formulas from GPS_GEODETIC_COORDINATE_SYSTEMS.md
  - [ ] Validation: Check units (degrees vs. radians)
  - [ ] Performance: Avoid recomputing sin(lat), cos(lat) in loops

- [ ] Create `src/math/magnetic_declination.h/cpp`
  - [ ] Function: `get_magnetic_declination(lat, lon, year)` → declination (degrees)
  - [ ] Use simplified IGRF model or lookup table
  - [ ] Function: `apply_declination(mag_heading_deg, declination_deg)` → true_heading_deg

- [ ] Unit tests: `tests/test_coordinates.cpp`
  - [ ] Test GPS ↔ ECEF round-trip (precision: ~1 meter)
  - [ ] Test with known coordinates (Munich, Los Angeles, etc.)
  - [ ] Test ECEF ↔ NED round-trip
  - [ ] Test altitude conversions (MSL vs. HAE)
  - [ ] Test magnetic declination at different locations
  - [ ] Edge cases: equator, poles, antimeridian

### BNO085 Extension

- [ ] Extend `src/sensors/bno085.cpp`
  - [ ] Add function: `get_rotation_matrix()` → convert quaternion to matrix on-demand
  - [ ] Add function: `get_euler_angles()` → roll, pitch, yaw in degrees
  - [ ] Add validation: check quaternion magnitude
  - [ ] Add debug output: log if magnitude deviates from 1.0

### Integration Tests

- [ ] Create `tests/integration_test_math_pipeline.cpp`
  - [ ] Read BNO085 quaternion → compute rotation matrix
  - [ ] Verify matrix is orthonormal (R × R^T = I)
  - [ ] Verify determinant = 1 (proper rotation, not reflection)

- [ ] Manual testing
  - [ ] Place prototype on table, verify Euler angles ≈ (0°, 0°, 0°)
  - [ ] Rotate 90° around X axis, verify Roll ≈ 90°
  - [ ] Rotate 90° around Y axis, verify Pitch ≈ 90°
  - [ ] Rotate 90° around Z axis, verify Yaw ≈ 90°

---

## Phase 2: GPS Integration (Weeks 2-3)

### GPS Hardware/Firmware

- [ ] Verify GPS module connected and responding
  - [ ] Check serial baud rate (typically 9600 or 115200)
  - [ ] Decode NMEA sentences or UBX binary frames
  - [ ] Confirm getting valid lat/lon/alt/satellites

- [ ] Create `src/sensors/gps.h/cpp`
  - [ ] Function: `read_gps()` → GPS_Data struct
  - [ ] Function: `is_gps_valid()` → check satellites, HDOP
  - [ ] Parse NMEA $GPGGA sentence (or UBX-NAV-PVT)
  - [ ] Handle timeout if GPS data stale (>1 second old)

### Coordinate Frame Manager

- [ ] Create `src/navigation/coordinate_frame.h/cpp`
  - [ ] Maintain state: origin_lat, origin_lon, origin_alt_m
  - [ ] Initialize on first valid GPS fix
  - [ ] Function: `gps_to_local(lat, lon, alt)` → NED (north, east, down) in meters
  - [ ] Function: `local_to_gps(north_m, east_m, down_m)` → lat, lon, alt
  - [ ] Account for magnetic declination

- [ ] Body frame position computation
  - [ ] Function: `compute_body_position(attitude_q, gps_ned_position)` → position in body frame
  - [ ] Or: given body-frame offset, compute world position

- [ ] Unit tests: `tests/test_coordinate_frame.cpp`
  - [ ] Test GPS → local conversions
  - [ ] Test local → GPS round-trip
  - [ ] Walk around with prototype, log positions, verify smoothness

### JSON Output Extension

- [ ] Extend `src/output/json_formatter.cpp`
  - [ ] Add GPS fields: lat, lon, alt, hdop, vdop, num_sats
  - [ ] Add position fields: ned_north_m, ned_east_m, ned_down_m
  - [ ] Add velocity fields: v_north_mps, v_east_mps, v_down_mps

- [ ] Update `tools/serial_monitor.py`
  - [ ] Parse extended JSON format
  - [ ] Option to filter/display specific fields

---

## Phase 3: Sensor Fusion (Weeks 3-4)

### EKF Implementation

- [ ] Create `src/navigation/ekf.h`
  - [ ] State vector: [qw, qx, qy, qz, vn, ve, vd, pn, pe, pd, ax_bias, ay_bias, az_bias] (16D)
  - [ ] Covariance matrix P (16×16)
  - [ ] Noise matrices Q (process), R (measurement)
  - [ ] Method: `predict(imu_data, dt)`
  - [ ] Method: `update(gps_data)`
  - [ ] Getter: `get_state()`, `get_covariance()`

- [ ] Create `src/navigation/ekf.cpp`
  - [ ] Implement predict: quaternion + velocity + position updates
  - [ ] Implement Jacobian (F matrix) of state transition
  - [ ] Implement measurement model (h function) for GPS
  - [ ] Implement Jacobian (H matrix) of measurement model
  - [ ] Update covariance: P_new = F × P × F^T + Q
  - [ ] Update state: K = P × H^T × (H × P × H^T + R)^{-1}
  - [ ] Kalman gain, state correction: x = x + K × (z - h(x))

- [ ] Covariance Tuning
  - [ ] Set Q (process noise) based on BNO085 specs
    - Gyro noise: ~0.005 rad/s
    - Accel noise: ~0.05 m/s²
  - [ ] Set R (measurement noise) based on GPS
    - Position uncertainty: HDOP × 1.5 meters (typical)
    - Velocity uncertainty: ~1 m/s
  - [ ] Start conservative (high uncertainty), tune after testing

- [ ] GPS Dropout Handling
  - [ ] Detect when GPS data haven't updated for >2 seconds
  - [ ] Continue predicting with IMU only (dead reckoning)
  - [ ] Limit trust in dead reckoning to ~30 seconds
  - [ ] Log warning when running on IMU alone

- [ ] Unit tests: `tests/test_ekf.cpp`
  - [ ] Test predict step with zero noise
  - [ ] Test update step with perfect measurement
  - [ ] Test covariance grows during predict, shrinks during update
  - [ ] Test GPS dropout recovery
  - [ ] Simulate 1 minute of IMU+GPS data, compare against hand calculations

### Integration with BNO085 & GPS

- [ ] Modify `src/main.cpp`
  - [ ] Create EKF instance
  - [ ] In loop: read BNO085 (100 Hz) → call `ekf.predict()`
  - [ ] In loop: read GPS (1 Hz) → call `ekf.update()`
  - [ ] Output fused state: attitude + velocity + position

### Visualization/Debugging

- [ ] Extend JSON output
  - [ ] Add `ekf_state`: fused attitude, position, velocity
  - [ ] Add `ekf_uncertainty`: covariance diagonal (std dev)
  - [ ] Add `gps_status`: satellites, HDOP, last_update_ms

- [ ] Create diagnostic tool (Python)
  - [ ] Plot: GPS vs. fused position over time
  - [ ] Plot: BNO085 attitude vs. fused attitude
  - [ ] Plot: covariance (uncertainty) vs. time
  - [ ] Plot: velocity estimates

---

## Phase 4: Camera Calibration (Weeks 4-5)

### Camera Extrinsics Measurement

- [ ] Set up calibration environment
  - [ ] ArUco markers on known surfaces, or
  - [ ] Checkerboard pattern on flat surface
  - [ ] Known reference distances (measure with tape)

- [ ] Calibration procedure (manual or automated)
  - [ ] Option A: Use ArUco detection
    - [ ] Print ArUco marker (e.g., 5×5 cm)
    - [ ] Rotate drone, record where marker appears in images
    - [ ] Compute best-fit camera pose
  - [ ] Option B: GPS+IMU as ground truth
    - [ ] Place drone at known GPS location
    - [ ] Point camera at known landmark
    - [ ] Compute camera extrinsics from known feature location

- [ ] Create `src/vision/camera_calibration.h`
  - [ ] Struct `CameraIntrinsics`: focal length, principal point, distortion
  - [ ] Struct `CameraExtrinsics`: rotation matrix, translation vector
  - [ ] Function: `load_extrinsics(filename)` → read from file
  - [ ] Function: `save_extrinsics(filename)` → write to file

- [ ] Create `src/vision/camera_calibration.cpp`
  - [ ] Function: `body_to_camera(point_body)` → point_camera
  - [ ] Function: `world_to_camera(point_world, attitude_q, position_world, extrinsics)` → point_camera
  - [ ] Function: `camera_to_pixel(point_camera, intrinsics)` → image pixel (x, y)
  - [ ] Function: `pixel_to_camera_ray(pixel, intrinsics)` → ray in camera frame

- [ ] Store extrinsics
  - [ ] YAML or JSON file: rotation (9 values), translation (3 values)
  - [ ] Include date, calibration method, confidence
  - [ ] Example:
    ```yaml
    calibration_date: 2026-05-10
    method: ArUco_marker
    rotation_matrix: [...]  # 3x3
    translation: [0.05, 0.0, -0.1]  # meters from body center
    confidence: high
    ```

### Integration

- [ ] Extend `src/navigation/ekf.cpp`
  - [ ] Compute camera position in world frame
  - [ ] Formula: p_camera = p_body + R_body_to_world × R_body_to_camera × offset_camera

- [ ] Extend JSON output
  - [ ] Add `camera_pose`: rotation matrix + position in world frame
  - [ ] Add `camera_is_calibrated`: true/false

---

## Phase 5: Application Testing (Weeks 5-8)

Choose one application to implement fully:

### Option A: Geo-Tagging

- [ ] Create `src/applications/geo_tagging.h/cpp`
  - [ ] Function: `pixel_to_world(image_pixel, camera_extrinsics, drone_pose)` → (x, y, z) in world
  - [ ] Assume pixel projects onto ground plane
  - [ ] Formula: Ray casting from camera through pixel to ground

- [ ] Testing
  - [ ] Take photo of landmark
  - [ ] Compute where landmark should appear in image
  - [ ] Verify computed position matches actual landmark
  - [ ] Accuracy metric: pixel error in image coordinates

- [ ] Output: GeoJSON with pixel locations tagged with GPS coordinates

### Option B: Autonomous Landing

- [ ] Create `src/applications/autonomous_landing.h/cpp`
  - [ ] Detect landing marker (AprilTag or cross pattern)
  - [ ] Estimate marker pose in camera frame
  - [ ] Compute drone pose relative to marker
  - [ ] Generate descent trajectory

- [ ] Testing
  - [ ] Place marker on ground
  - [ ] Fly drone overhead
  - [ ] Verify marker detected consistently
  - [ ] Verify pose estimate is accurate
  - [ ] Simulate descent (don't actually land yet)

- [ ] Accuracy metric: position error (cm level desirable)

### Option C: 3D Reconstruction

- [ ] Create `src/applications/structure_from_motion.h/cpp`
  - [ ] Capture multiple frames with pose estimates from EKF
  - [ ] Store: image + camera pose for each frame
  - [ ] Export to SfM software (COLMAP, Meshroom)

- [ ] Testing
  - [ ] Fly around a small object, capture images
  - [ ] Run SfM post-flight
  - [ ] Verify 3D model is geometrically correct
  - [ ] Compare SfM accuracy vs. ground truth measurements

- [ ] Accuracy metric: 3D point cloud error (mm to cm level typical)

---

## Code Quality Standards

- [ ] All C++ code compiles with `-Wall -Wextra -Werror`
- [ ] All functions have comments (what, inputs, outputs)
- [ ] All public methods have unit tests
- [ ] Code follows project style (see `flight_controller/STYLE.md` if exists)
- [ ] Memory: No dynamic allocation in real-time loops (pre-allocate)
- [ ] Performance: All math in <5 ms per loop iteration

---

## Documentation Checklist

- [ ] Each new .cpp file has header comments (purpose, theory reference)
- [ ] Each math function has equation reference (which PDF, which page)
- [ ] README updated with new modules
- [ ] CHANGELOG.md updated
- [ ] Code examples added to docs/ showing how to use new APIs
- [ ] Git commits with clear messages referencing research docs

---

## Testing Progression

### Unit Tests (First)
```bash
platformio test --environment=arduino_mega
```

### Integration Tests (Second)
```bash
# Flash prototype with calibration mode
platformio run -e arduino_mega_calibration -t upload
# Run serial monitor and collect test data
python3 tests/collect_integration_test_data.py
# Analyze results
python3 tests/analyze_integration_results.py
```

### Field Tests (Third)
```
1. Static test (on table, known orientation)
2. Motion test (walk around, log data)
3. GPS test (outdoor, verify fusion)
4. Application test (actual use case)
```

---

## Rollback Plan

If any phase is blocked:

- [ ] Phase 1 blocked?
  - Fallback: Use reference implementations (Python) for validation
  - Continue: Test math separately, integrate C++ later

- [ ] Phase 2 blocked?
  - Fallback: Use simulated GPS data from file
  - Continue: Implement coordinate frames with synthetic data

- [ ] Phase 3 blocked?
  - Fallback: Use simple complementary filter instead of EKF
  - Continue: Get something working, refine later

- [ ] Phase 4 blocked?
  - Fallback: Use manual camera calibration from OpenCV
  - Continue: Build applications with fixed extrinsics

- [ ] Phase 5 blocked?
  - Fallback: Pick simpler application (geo-tagging vs. landing)
  - Continue: Full application scope can expand later

---

## Success Criteria

✓ Phase 1 complete: Math library tested, accurate to ±0.01° rotation  
✓ Phase 2 complete: GPS position smooth, ±5 m accuracy, no jumps  
✓ Phase 3 complete: EKF fusion smooth and stable, position uncertainty reasonable  
✓ Phase 4 complete: Camera extrinsics calibrated, ±2 pixel accuracy in projection  
✓ Phase 5 complete: One application working end-to-end with real hardware  

---

## Estimated Timeline

- **Phase 1**: 2 weeks (math intensive, lots of testing)
- **Phase 2**: 1 week (mostly glue code)
- **Phase 3**: 1.5 weeks (EKF tuning takes time)
- **Phase 4**: 1 week (measurement + integration)
- **Phase 5**: 2-3 weeks (depends on application complexity)

**Total**: ~8-9 weeks from start to application deployment

---

**Last Updated**: 2026-05-07  
**Status**: Ready to begin Phase 1  
**Approval**: Awaiting start signal

