# Research: MPU 6050 Yaw-Only Orientation without Magnetometer

**Status**: Not Started  
**Priority**: Medium (v1.1 feature, enables MPU 6050 support)  
**Last Updated**: 2026-05-05

---

## Problem Statement

MPU 6050 has gyroscope + accelerometer but **no magnetometer**. This means:
- **Pitch/Roll**: Easily derived from accelerometer (gravity vector)
- **Yaw**: Cannot be determined without external reference (no magnetometer, gravity doesn't indicate yaw)

For applications that need only pitch/roll (e.g., stabilizing a quadcopter), this is fine. But for absolute orientation, yaw is critical.

**Goal**: Research and document approaches for yaw estimation when magnetometer is unavailable.

---

## Questions to Answer

1. Can we use external magnetometer (separate from IMU) to give yaw reference?
2. Can we integrate gyro over time (dead reckoning)? What's the drift rate?
3. Are there visual/compass alternatives for yaw reference?
4. How do other IMU libraries handle this (ROS, flight control libraries)?
5. For dual-IMU setups (BNO085 + MPU 6050), how do we fuse yaw?
6. What's the accuracy/drift acceptable for our applications?

---

## Approaches to Investigate

### 1. Dead Reckoning (Gyro Integration)
- Integrate gyro around Z-axis over time
- **Pros**: No external reference needed
- **Cons**: Drift accumulates (gyro bias); unusable after ~minutes

### 2. External Magnetometer
- Add separate magnetometer IC (e.g., HMC5883L)
- **Pros**: Absolute yaw reference
- **Cons**: Additional hardware; must be calibrated; affected by local magnetic disturbances

### 3. Visual/Camera Reference
- If device has camera, extract yaw from feature tracking
- **Pros**: Works indoors / without magnetic reference
- **Cons**: Computationally expensive; requires ML/vision pipeline

### 4. IMU Fusion (Multiple Units)
- Use BNO085 (with magnetometer) as reference for yaw
- MPU 6050 provides high-freq gyro/accel data
- Kalman filter to fuse both
- **Pros**: Leverages both sensors' strengths
- **Cons**: Complex filter design; requires tuning

### 5. Application-Specific Constraints
- For drone: Yaw can be inferred from motor commands + gyro integration (over short windows)
- For camera: Yaw can come from pan/tilt motor feedback or visual features

---

## Research Progress

- [ ] Review MPU 6050 datasheet and typical yaw-only approaches
- [ ] Research ROS / flight control libraries (ArduPilot, PX4) for multi-IMU handling
- [ ] Investigate gyro drift rates (empirical data)
- [ ] Research Kalman filter design for IMU fusion
- [ ] Check whether external magnetometer + MPU is a practical option
- [ ] Test on actual hardware if feasible
- [ ] Document trade-offs and recommended approach per application

---

## Key References

- MPU 6050 Datasheet
- ROS sensor_fusion libraries
- ArduPilot/PX4 IMU handling code
- Kalman Filter tutorials for IMU fusion
- HMC5883L Magnetometer (if external route taken)

---

## Findings

(To be filled in during development)

---

## Recommendations

(To be filled in after research)

