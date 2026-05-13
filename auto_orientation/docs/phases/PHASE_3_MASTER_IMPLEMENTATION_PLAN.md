# Phase 3: Extended Kalman Filter (EKF) Sensor Fusion
**Date**: 2026-05-07  
**Phase 2 Status**: ✅ COMPLETE (131/133 tests passing)  
**Phase 3 Status**: Ready for full execution  
**Target Duration**: 1-2 weeks continuous work  
**Complexity**: High (16D state space, matrix math, numerical stability)

---

## Scope: What Will Be Completed

### Primary Deliverables

1. **Extended Kalman Filter Implementation** (`src/navigation/ekf.h/cpp`)
   - 16-dimensional state vector:
     - Orientation: [qw, qx, qy, qz] (quaternion)
     - Velocity: [vn, ve, vd] (NED frame, m/s)
     - Position: [pn, pe, pd] (NED frame, meters)
     - Accel biases: [ax_bias, ay_bias, az_bias] (m/s²)
   - Full covariance matrix P (16×16)
     - Represents uncertainty in all state dimensions
     - Grows during predict, shrinks during update
   - Process noise Q (16×16) - IMU uncertainty
   - Measurement noise R (3×3) - GPS uncertainty
   - Predict step: Propagate state + covariance with IMU at 100 Hz
   - Update step: Correct state + covariance with GPS at ~1 Hz
   - Numerical stability: Handle singular matrices, clamp covariance
   - Memory efficiency: Pre-allocated matrices, no dynamic allocation

2. **GPS Dropout Detection & Dead Reckoning**
   - Monitor GPS update frequency (should be ~1 Hz)
   - Detect stale GPS (>1000ms without update)
   - When GPS drops: Continue EKF predict-only (dead reckoning)
   - Covariance grows unbounded during GPS loss (uncertainty increases)
   - Recovery: Resume GPS updates when signal returns
   - Timeout: Limit dead reckoning to ~30 seconds, flag as unreliable

3. **Sensor Integration in Main Loop**
   - Read BNO085 orientation at ~100 Hz
   - Read GPS position at ~1 Hz (asynchronous)
   - Call ekf.predict() after each IMU read
   - Call ekf.update() after each GPS read
   - Output fused state from EKF (not raw sensors)
   - Handle edge cases: sensor not ready, GPS loss, timestamp gaps

4. **Extended JSON Output with EKF Results**
   - Fused state: attitude (quaternion + Euler), velocity, position
   - Uncertainty: covariance diagonal (standard deviation for each state)
   - GPS status: lock, satellite count, HDOP, last update time
   - EKF health: covariance magnitude, innovation magnitude
   - Data quality: combined uncertainty metric
   - Comparison: raw GPS vs fused position (debug mode)

5. **Covariance Tuning Parameters** (`src/config/ekf_config.h`)
   - Q matrix (process noise): Based on BNO085 specs
     - Gyro noise: 0.005 rad/s (from datasheet)
     - Accel noise: 0.05 m/s²
   - R matrix (measurement noise): Based on GPS specs
     - Position uncertainty: HDOP × 1.5 meters
     - Velocity uncertainty: ~1 m/s (from RMC parsing)
   - Initial covariance P: Large uncertainty at startup
   - Tuning validation: Field tests with known trajectories

6. **State Transition Dynamics** (`src/navigation/state_dynamics.h/cpp`)
   - Quaternion kinematics: dq/dt = 0.5 * q ⊗ ω (omega from gyro)
   - Velocity kinematics: dv/dt = R(q) * a_accel + g (gravity)
   - Position kinematics: dp/dt = v
   - Accel bias model: constant (first-order Gauss-Markov could be added)
   - Jacobian matrices (F, H) for linearization
   - Numerical stability in Jacobian computation

7. **GPS Measurement Model**
   - Map fused NED position to GPS measurement space (lat/lon/alt)
   - Innovation: z_meas - h(x_fused)
   - Measurement Jacobian H (3×16)
   - Handle coordinate transformations in measurement update

8. **Testing Infrastructure**
   - Unit tests: Individual EKF components (predict, update, Jacobians)
   - Integration tests: Full EKF with simulated IMU + GPS data
   - Scenario tests: Stationary, constant velocity, GPS dropouts
   - Performance tests: Timing, memory usage
   - Accuracy tests: Compare fused vs ground truth

9. **Comprehensive Documentation**
   - EKF Theory reference (Kalman filter basics, extended Kalman filter)
   - State space model documentation
   - API reference with examples
   - Tuning guide for Q, R matrices
   - Troubleshooting common EKF issues

### Test Coverage

- **Unit Tests** (25+ cases)
  - State transition: Q → Q+dQ at dt
  - Covariance evolution: P_new = F*P*F^T + Q
  - Jacobian computation: F and H matrices
  - Measurement update: Kalman gain, state correction
  - Numerical stability: Matrix inversion, SVD fallback
  - Edge cases: Singular matrices, near-zero innovation

- **Integration Tests** (30+ cases)
  - Full predict-update cycle
  - GPS dropout detection and recovery
  - Simulated trajectories (straight line, circle, figure-8)
  - Covariance evolution over time
  - Dead reckoning accuracy vs time
  - State output validation

- **Scenario Tests** (10+ cases)
  - Static (no motion): Covariance should stabilize
  - Constant velocity: Position should match velocity × time
  - GPS loss & recovery: Dead reckoning then re-lock
  - Noisy measurements: Filter should smooth
  - Initial transients: Convergence time

---

## Implementation Architecture

### State Space Formulation

**State Vector (16D):**
```
x = [qw, qx, qy, qz,    // orientation (quaternion)
     vn, ve, vd,         // velocity NED (m/s)
     pn, pe, pd,         // position NED (m)
     ax_bias, ay_bias, az_bias]  // accel biases (m/s²)
```

**Process Model (Continuous):**
```
dq/dt = 0.5 * q ⊗ ω         (quaternion kinematics)
dv/dt = R(q) * a + [0, 0, g] (dynamics with gravity)
dp/dt = v                     (kinematic)
d(bias)/dt = 0               (constant bias model)
```

**Discrete Update (Δt = 0.01s at 100 Hz):**
```
x_new = f(x_old, u_imu, Δt)   (nonlinear state transition)
P_new = F * P_old * F^T + Q    (covariance propagation)
```

**Measurement Model:**
```
z_gps = [lat, lon, alt]^T              (GPS observation)
h(x) = gps_from_ned(pn, pe, pd)        (measurement function)
H = ∂h/∂x (measurement Jacobian)       (3×16 matrix)
```

**Update Equations:**
```
innovation = z_gps - h(x)
S = H * P * H^T + R            (innovation covariance)
K = P * H^T * S^{-1}           (Kalman gain)
x_new = x + K * innovation     (state correction)
P_new = (I - K*H) * P          (covariance update)
```

### Numerical Stability Considerations

1. **Matrix Inversion**: Use SVD-based pseudoinverse for near-singular S
2. **Covariance Symmetry**: Enforce P_new = 0.5*(P_new + P_new^T)
3. **Positive Definiteness**: Clamp negative eigenvalues to machine epsilon
4. **Quaternion Normalization**: Normalize after predict step
5. **Scale Invariance**: Use normalized state for better conditioning

---

## File Structure

```
auto_orientation/
├── src/
│   ├── navigation/
│   │   ├── ekf.h (NEW - core EKF class)
│   │   ├── ekf.cpp (NEW - implementation)
│   │   ├── state_dynamics.h (NEW - state transition)
│   │   ├── state_dynamics.cpp (NEW - Jacobians)
│   │   ├── measurement_model.h (NEW - GPS measurement)
│   │   ├── measurement_model.cpp (NEW - h(x) and H)
│   │   ├── covariance_manager.h (NEW - numerical stability)
│   │   ├── covariance_manager.cpp
│   │   └── coordinate_frame.h/cpp (extend for NED↔GPS)
│   ├── config/
│   │   └── ekf_config.h (NEW - Q, R tuning parameters)
│   ├── output/
│   │   └── sensor_output_manager.h/cpp (extend for EKF output)
│   └── main.cpp (update main loop for predict/update)
├── tests/
│   ├── test_ekf.cpp (NEW - 25+ unit tests)
│   ├── test_state_dynamics.cpp (NEW - state transition)
│   ├── test_measurement_model.cpp (NEW - GPS measurement)
│   ├── integration_test_ekf_full.cpp (NEW - 30+ integration tests)
│   └── scenario_test_ekf.cpp (NEW - 10+ scenario tests)
└── docs/
    ├── EKF_THEORY.md (NEW - Kalman filter theory)
    ├── EKF_API_REFERENCE.md (NEW - API documentation)
    ├── EKF_TUNING_GUIDE.md (NEW - Q/R parameter tuning)
    ├── PHASE_3_TEST_RESULTS.md (NEW - test summary)
    └── STATE_SPACE_MODEL.md (NEW - mathematical model)
```

---

## Covariance Tuning Strategy

### Process Noise Q (16×16)

Based on IMU error specs (BNO085 datasheet):
```
Q_gyro = (0.005 rad/s)² = 2.5e-5 rad²/s²    (gyro white noise)
Q_accel = (0.05 m/s²)² = 2.5e-3 m²/s⁴        (accel white noise)
Q_bias = 1e-8 m²/s⁶                          (slow bias drift)
```

Diagonal blocks:
```
Q = diag([Q_gyro_x, Q_gyro_y, Q_gyro_z,     // attitude
          Q_accel_x, Q_accel_y, Q_accel_z,  // velocity
          0, 0, 0,                             // position (kinematic)
          Q_bias_x, Q_bias_y, Q_bias_z])     // bias (slow)
```

### Measurement Noise R (3×3)

Based on GPS error specs:
```
σ_position = HDOP × 1.5 meters               (typical: 0.75 × 1.5 = 1.125m)
σ_velocity = 0.5 m/s                         (from RMC accuracy)
```

But measurement is position only:
```
R = diag([σ_position², σ_position², σ_position²])
  = diag([1.27, 1.27, 1.27]) m²              (HDOP-dependent)
```

### Initial Covariance P (16×16)

Start with large uncertainty:
```
P_orientation = 1.0 rad² (large uncertainty in angle)
P_velocity = 10² m²/s² (large uncertainty in velocity)
P_position = 100² m² (large uncertainty in absolute position)
P_bias = 0.5² m²/s⁴ (moderate bias uncertainty)
```

### Adaptive Tuning

1. **Offline tuning**: Compare fused vs ground truth, adjust Q/R
2. **Online adaptation**: Monitor innovation magnitude, auto-scale R if GPS quality changes
3. **Field validation**: Test with known trajectories, measure convergence time

---

## Main Loop Architecture

```cpp
void loop() {
  // HIGH FREQUENCY: BNO085 orientation at ~100 Hz
  if (imu.read()) {
    // Extract gyro, accel, quaternion from BNO085
    const auto& imu_data = imu.getIMUData();
    
    // Call EKF predict step
    ekf.predict(imu_data.gyro_rad_s, imu_data.accel_m_s2, dt_s);
    
    // Prepare output (can be either raw or fused based on mode)
  }
  
  // LOW FREQUENCY: GPS position at ~1 Hz
  static uint32_t last_gps_update = 0;
  if (gps.read() && gps.hasLock()) {
    // Call EKF update step
    const auto& gps_data = gps.getPosition();
    ekf.update(gps_data.latitude, gps_data.longitude, gps_data.altitude);
    last_gps_update = millis();
  }
  
  // MONITOR: GPS dropout detection
  uint32_t gps_age_ms = millis() - last_gps_update;
  if (gps_age_ms > 2000) {
    // GPS stale for >2 seconds
    ekf.set_gps_dropout(true);
    if (gps_age_ms > 30000) {
      // Dead reckoning timeout - flag unreliable
      ekf.set_dead_reckoning_valid(false);
    }
  } else {
    ekf.set_gps_dropout(false);
  }
  
  // OUTPUT: Generate JSON with fused state
  auto fused_state = ekf.getState();
  auto fused_uncertainty = ekf.getUncertainty();
  output_manager.updateFusedState(fused_state, fused_uncertainty);
  output_manager.updateGPSStatus(gps.getPosition(), gps_dropout);
  
  output_manager.sendJSON();
}
```

---

## Success Criteria

✅ **EKF Implementation**
- 16D state vector fully implemented
- Predict step working (quaternion kinematics, velocity/position update)
- Update step working (Kalman gain, state/covariance correction)
- Numerical stability verified (no NaN/Inf in covariance)
- All 25+ unit tests passing

✅ **GPS Dropout Handling**
- Dropout detection working (>1s without update)
- Dead reckoning for up to 30 seconds
- Covariance growth during GPS loss
- Recovery on GPS reacquisition
- All 10+ scenario tests passing

✅ **Integration**
- Main loop calling predict/update at correct frequencies
- Output format includes fused state + uncertainty
- No performance regression (still <5ms per loop)
- All 30+ integration tests passing

✅ **Testing**
- 65+ total tests passing (25 unit + 30 integration + 10 scenario)
- Accuracy validation (fused vs ground truth)
- Performance validation (<5ms per predict/update)
- Stability validation (covariance positive definite)

✅ **Documentation**
- EKF theory reference (10+ pages)
- API reference with examples (15+ pages)
- Tuning guide (10+ pages)
- Test results summary (10+ pages)

---

## Estimated Effort

- **EKF Core Implementation**: 4-6 hours
- **State Transition Dynamics**: 2-3 hours
- **Measurement Model**: 1-2 hours
- **GPS Dropout Handling**: 1-2 hours
- **Unit Tests**: 2-3 hours
- **Integration Tests**: 2-3 hours
- **Scenario Tests**: 1-2 hours
- **Tuning & Validation**: 2-3 hours
- **Documentation**: 2-3 hours
- **Code Review & Cleanup**: 1-2 hours

**Total**: 18-25 hours (2-3 days with parallel agents)

---

## Parallel Agent Task Breakdown

### Agent 1: EKF Core Implementation
- Create `src/navigation/ekf.h` - Class definition with predict/update methods
- Create `src/navigation/ekf.cpp` - Full implementation
- Implement state vector, covariance matrix, process/measurement noise
- Implement predict: quaternion kinematics, velocity/position updates
- Implement update: Kalman gain computation, state/covariance correction
- Create unit tests (test_ekf.cpp) - 25+ test cases
- Validate numerical stability (positive definite covariance)

### Agent 2: State Dynamics & Measurement Model
- Create `src/navigation/state_dynamics.h/cpp` - State transition & Jacobians
- Create `src/navigation/measurement_model.h/cpp` - GPS measurement & Jacobian
- Create `src/config/ekf_config.h` - Q, R tuning parameters
- Implement quaternion kinematics (dq/dt = 0.5 * q ⊗ ω)
- Implement velocity/position kinematics (dv/dt, dp/dt)
- Implement F matrix (Jacobian of state transition)
- Implement H matrix (Jacobian of measurement function)
- Create unit tests (test_state_dynamics.cpp, test_measurement_model.cpp)

### Agent 3: GPS Dropout & Integration
- Implement GPS dropout detection in main loop
- Implement dead reckoning logic (covariance growth without updates)
- Implement dropout recovery (GPS re-lock)
- Create `src/navigation/covariance_manager.h/cpp` for numerical stability
- Update `src/main.cpp` for EKF predict/update loop at correct frequencies
- Integrate with existing BNO085 + GPS sensors
- Handle timestamp synchronization
- Create integration tests (integration_test_ekf_full.cpp)

### Agent 4: Output & Configuration
- Extend `src/output/sensor_output_manager` for fused state output
- Add EKF state to JSON: quaternion, velocity, position, uncertainty
- Add uncertainty visualization (covariance diagonal as std dev)
- Add GPS status (lock, satellites, dropout flag)
- Add EKF health metrics (innovation magnitude, covariance magnitude)
- Create JSON schema documentation
- Test JSON output with multiple scenarios
- Create test_json_ekf_output.cpp

### Agent 5: Testing, Validation & Documentation
- Create scenario tests (scenario_test_ekf.cpp) - 10+ test cases
- Scenario 1: Static (no motion) - covariance should stabilize
- Scenario 2: Constant velocity - verify kinematic model
- Scenario 3: GPS loss & recovery - dead reckoning accuracy
- Scenario 4: Noisy measurements - filter smoothing
- Create performance benchmarks (timing, memory)
- Create accuracy validation (compare fused vs ground truth)
- Create comprehensive documentation (5 guide files)
- Create PHASE_3_TEST_RESULTS.md summary
- Final verification: All tests passing, 0 warnings, stable compilation

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Covariance becomes non-positive-definite | Use SVD-based pseudoinverse, enforce symmetry, clamp eigenvalues |
| Quaternion denormalization | Normalize after each predict step |
| Singular matrices in measurement update | Use SVD fallback, increase R if S becomes singular |
| Divergence due to Q/R tuning | Start conservative (high uncertainty), validate with field tests |
| Numerical precision loss | Use double for intermediate calculations, pre-allocate |
| Main loop timing | Pre-allocate all matrices, avoid dynamic allocation, profile timing |
| GPS dropout not detected | Set timeout threshold, monitor update frequency |
| Dead reckoning drift | Limit to 30s, flag as unreliable after timeout |

---

## Next Phase (After Phase 3 Complete)

- **Phase 4**: Camera Calibration (1 week)
  - Measure camera extrinsic offset from body frame
  - Use ArUco markers or GPS+IMU ground truth
  - Integrate camera pose into output

- **Phase 5**: Applications (2-3 weeks)
  - Choose: Geo-tagging, Autonomous Landing, or 3D Reconstruction
  - Full end-to-end application with field testing
  - Accuracy validation with real-world data

---

## Git Strategy

- One commit per agent completion
- Final squash commit: "PHASE 3: COMPLETE - EKF sensor fusion with GPS dropout handling"
- Tag: `phase-3-complete`
- Push to origin/main when done

---

**Status**: Ready for agent execution  
**Approval**: Awaiting go signal  
**Start Date**: 2026-05-07 (now)  
**Expected Completion**: 2026-05-09 (48 hours with parallel agents)

---

*Master Implementation Plan for Phase 3: Extended Kalman Filter Sensor Fusion*  
*Date: 2026-05-07*  
*Project: auto_orientation (IMU + GPS + EKF Fusion)*
