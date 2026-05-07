# EKF Tuning Guide

## Introduction

The Extended Kalman Filter (EKF) is powerful, but it's only as good as its tuning parameters. This guide provides a step-by-step procedure to tune the EKF for your specific application.

**Key Parameters to Tune**:
- **Q matrix** (process noise): How much you trust the motion model
- **R matrix** (measurement noise): How much you trust the sensors
- **P0 matrix** (initial covariance): Starting uncertainty about state

---

## Quick Start

### For Most Applications

```cpp
// Reasonable default tuning
ekf.setProcessNoise(0.01f, 0.1f, 0.001f, 0.0001f);
ekf.setMeasurementNoise(4.0f, 4.0f);  // Assumes 2m GPS std dev
ekf.setInitialCovariance(0.1f, 1.0f, 100.0f, 1.0f);
```

Then adjust based on observed behavior (see "Tuning Procedure" below).

---

## Understanding the Parameters

### Q Matrix (Process Noise)

Represents "how much you trust your motion model"

```
Q = [Q_attitude  0          0         0        ]
    [0           Q_velocity 0         0        ]
    [0           0          Q_position 0        ]
    [0           0          0         Q_bias   ]
```

**Each component**:

| Parameter | Units | Meaning | Typical | Effect of Increasing |
|-----------|-------|---------|---------|----------------------|
| Q_attitude | rad²/s² | Trust in quaternion kinematics | 0.01 | Responds faster to attitude changes |
| Q_velocity | m²/s³ | Trust in velocity kinematics | 0.1 | Follows IMU acceleration more |
| Q_position | m²/s² | Trust in position integration | 0.001 | Follows measured velocity more |
| Q_bias | m²/s⁵ | Trust in constant bias assumption | 0.0001 | Bias estimate changes faster |

**Intuition**:
- **Large Q**: "I don't trust my model, follow measurements closely"
  - Filter responds quickly to GPS corrections
  - Output can be noisy/jumpy
- **Small Q**: "I trust my model, smooth trajectory"
  - Filter trusts IMU predictions
  - Output is smooth but might lag behind truth

### R Matrix (Measurement Noise)

Represents "how much you trust your sensors"

```
R = [R_h   0    0  ]
    [0    R_h  0  ]
    [0     0   R_v ]
```

Where:
- **R_h**: Horizontal GPS measurement noise (m²)
- **R_v**: Vertical GPS measurement noise (m²)

**Relationship to GPS accuracy**:
```
R_h = (GPS horizontal std dev)²
R_v = (GPS vertical std dev)²

Example:
GPS std dev = 2 meters
R_h = (2 m)² = 4 m²
```

**With HDOP/VDOP**:
```
R_h = (HDOP × expected_error_at_HDOP_1.0)²
R_v = (VDOP × expected_error_at_VDOP_1.0)²

Typical expected errors:
R_h = (HDOP × 2m)²
R_v = (VDOP × 2m)²
```

**Intuition**:
- **Large R**: "GPS is noisy, ignore measurement changes"
  - Filter smooths GPS noise effectively
  - Slower to detect real position changes
- **Small R**: "GPS is accurate, follow it"
  - Filter responds quickly to GPS
  - Can jump around if GPS has spikes

### P0 Matrix (Initial Covariance)

Represents "how uncertain are we about the initial state"

```
P0 = [P_attitude   0          0         0      ]
     [0            P_velocity 0         0      ]
     [0            0          P_position 0      ]
     [0            0          0         P_bias ]
```

**Each component**:

| Parameter | Units | Meaning | Cold Start | Known State |
|-----------|-------|---------|------------|------------|
| P_attitude | rad² | Attitude uncertainty | 0.1 | 0.01 |
| P_velocity | m²/s² | Velocity uncertainty | 1.0 | 0.1 |
| P_position | m² | Position uncertainty | 100.0 | 10.0 |
| P_bias | m²/s⁴ | Bias uncertainty | 1.0 | 0.1 |

**Intuition**:
- **Large P0**: Start with low confidence
  - Takes time to converge
  - Robust if initial state is wrong
  - Useful for cold start (unknown position)
- **Small P0**: Start with high confidence
  - Converges quickly
  - Risky if initial state is wrong
  - Useful if you know the initial state well

---

## Tuning Procedure (Step by Step)

### Step 1: Verify Your Sensor Specifications

**For GPS**:
1. Collect 100+ GPS measurements at a fixed location (no motion)
2. Compute mean and standard deviation
3. Use std dev as R values

```
Example procedure:
lat_samples = []
lon_samples = []
for each GPS reading:
    lat_samples.append(latitude)
    lon_samples.append(longitude)

lat_std = std(lat_samples)
lon_std = std(lon_samples)

Convert std dev in degrees to meters:
# At 47°N latitude: 1° latitude ≈ 111 km, 1° longitude ≈ 74 km
lat_std_m = lat_std * 111000
lon_std_m = lon_std * 74000 * cos(47°)

R_h = max(lat_std_m, lon_std_m)²
```

**For IMU**:
1. Keep device stationary (no motion)
2. Read accelerometer for 30 seconds
3. Compute standard deviation

```
Example:
accel_x_samples = []
while stationary:
    accel_x_samples.append(imu.accel_x)
    
accel_std = std(accel_x_samples)
# This is IMU measurement noise (not used directly, but useful to know)
```

### Step 2: Set Measurement Noise (R)

```cpp
// From your measurements above
float gps_horizontal_std = 2.0f;  // meters (measured)
float gps_vertical_std = 3.0f;    // meters (measured)

ekf.setMeasurementNoise(
    gps_horizontal_std * gps_horizontal_std,
    gps_vertical_std * gps_vertical_std
);
```

**Sanity Check**:
- After a few GPS measurements, uncertainty should shrink toward GPS std dev
- If not, R values might be wrong

### Step 3: Choose Initial Covariance (P0)

**Option A: Cold Start (Unknown Position)**
```cpp
ekf.setInitialCovariance(
    0.1f,     // Large attitude uncertainty
    1.0f,     // Don't know velocity
    1000.0f,  // Very uncertain position (20m std dev)
    1.0f      // Don't know bias
);
```

**Option B: Known State (From Previous Flight)**
```cpp
ekf.setInitialCovariance(
    0.01f,    // Good estimate of attitude
    0.1f,     // Roughly know velocity
    10.0f,    // Good estimate of position
    0.1f      // Roughly know bias
);
```

### Step 4: Set Process Noise (Q) - Default

Start with default values:
```cpp
ekf.setProcessNoise(
    0.01f,     // Q_attitude
    0.1f,      // Q_velocity
    0.001f,    // Q_position
    0.0001f    // Q_bias
);
```

### Step 5: Test and Observe

Collect data with these parameters:

```cpp
while (running) {
    ekf.predict(gx, gy, gz, ax, ay, az, dt);
    
    if (gps_available) {
        ekf.update(lat, lon, alt);
    }
    
    // Log everything
    log_state(ekf);
    log_covariance(ekf);
    log_innovation(ekf);
}
```

Log fields:
- Position (x, y, z)
- Velocity (vx, vy, vz)
- Position uncertainty
- Innovation magnitude
- GPS dropout status

### Step 6: Analyze Results

#### Check 1: Convergence

```
Initial uncertainty: 100 m
After 10 GPS updates: < 10 m  ✓ Good
After 10 GPS updates: > 50 m  ✗ Converges too slow

Fix: Decrease P_position or increase R values
```

#### Check 2: Smoothness

```
Raw GPS variance: 4.0 m²
Filter variance: 1.0 m²
Variance reduction: 75%  ✓ Good

Raw GPS variance: 4.0 m²
Filter variance: 3.9 m²
Variance reduction: 2.5%  ✗ Filter not smoothing enough

Fix: Increase Q_position (trust model more)
```

#### Check 3: Responsiveness

```
True motion: Instantaneous 5 m jump
Filter delay: < 2 seconds  ✓ Good
Filter delay: > 5 seconds  ✗ Too slow to respond

Fix: Decrease Q values (trust measurements more)
```

#### Check 4: Innovation Magnitude

```
Typical innovation: 0.5-1.0 m  ✓ Normal
Occasional spikes: > 5 m      ✓ Normal (GPS noise)
Frequent > 5 m spikes:        ✗ Model/sensor mismatch

Fix: Check for GPS errors, verify initial state, increase R
```

#### Check 5: No Negative Variance

```
Check covariance diagonal periodically:
if ekf.isCovariancePositiveDefinite():
    ✓ Good
else:
    ✗ Numerical instability! Reset EKF

If this happens, reduce Q values or increase R values
```

### Step 7: Adjust Based on Observations

#### Symptom: Filter Lags Behind Motion

**Observation**:
- GPS shows position change, but filter estimate lags behind
- Position uncertainty stays high

**Fix**:
- Increase Q values (trust IMU more)
- Decrease R values (trust GPS more)

**Example adjustment**:
```cpp
// Old values (too conservative)
ekf.setProcessNoise(0.001f, 0.01f, 0.0001f, 0.00001f);
ekf.setMeasurementNoise(16.0f, 16.0f);  // R too high

// New values (more responsive)
ekf.setProcessNoise(0.01f, 0.1f, 0.001f, 0.0001f);
ekf.setMeasurementNoise(4.0f, 4.0f);    // R more reasonable
```

#### Symptom: Filter Output Is Jumpy/Noisy

**Observation**:
- Position changes abruptly with each GPS update
- Looks like it's following GPS exactly

**Fix**:
- Decrease Q values (trust model more)
- Increase R values (trust GPS less)

**Example adjustment**:
```cpp
// Old values (too trusting of GPS)
ekf.setProcessNoise(0.1f, 1.0f, 0.01f, 0.001f);
ekf.setMeasurementNoise(0.1f, 0.1f);   // R too low

// New values (smoother)
ekf.setProcessNoise(0.01f, 0.1f, 0.001f, 0.0001f);
ekf.setMeasurementNoise(9.0f, 9.0f);   // R higher (3m std dev)
```

#### Symptom: Convergence Too Slow

**Observation**:
- From cold start, uncertainty decreases very slowly
- Takes > 1 minute to reach useful accuracy

**Fix**:
- Decrease P0_position (start with higher confidence)
- Increase R values (measurements more reliable)

**Example adjustment**:
```cpp
// Old values (very conservative)
ekf.setInitialCovariance(0.5f, 5.0f, 1000.0f, 5.0f);

// New values (reasonable confidence)
ekf.setInitialCovariance(0.1f, 1.0f, 100.0f, 1.0f);
```

#### Symptom: Numerical Instability

**Observation**:
- Covariance matrix contains negative values
- `isCovariancePositiveDefinite()` returns false

**Fix**:
- Reduce Q values significantly
- Increase R values
- Reset EKF periodically

**Example**:
```cpp
// Conservative values (more stable)
ekf.setProcessNoise(0.001f, 0.01f, 0.0001f, 0.00001f);
ekf.setMeasurementNoise(16.0f, 25.0f);

// Add stability enforcement
ekf.enforceCovarianceSymmetry();
ekf.clampCovarianceDiagonal(1e-8f);

// If still unstable, reset
if (!ekf.isCovariancePositiveDefinite()) {
    ekf.reset();
}
```

---

## Scenario-Specific Tuning

### Scenario 1: Urban GPS (Multipath, Dropout)

**Characteristics**:
- GPS is unreliable, frequent dropouts
- Buildings block signal
- Want smooth motion estimate despite GPS noise

**Tuning**:
```cpp
// Trust IMU more, GPS less
ekf.setProcessNoise(0.001f,   // Small, trust quaternion kinematics
                    0.01f,    // Small, trust velocity integration
                    0.0001f,  // Very small, trust position integration
                    0.00001f);// Tiny bias change

ekf.setMeasurementNoise(16.0f, 25.0f);  // High R (4m, 5m std dev)
ekf.setInitialCovariance(0.5f, 5.0f, 1000.0f, 5.0f);
```

**Expected behavior**:
- Smooth motion even with noisy GPS
- Dead reckoning works well during dropout
- Position uncertainty grows predictably

### Scenario 2: High-Speed Highway

**Characteristics**:
- Fast motion (highway speeds)
- Good GPS signal
- Need responsive filter

**Tuning**:
```cpp
// Balanced approach
ekf.setProcessNoise(0.01f,   // Reasonable
                    0.1f,    // Reasonable
                    0.001f,  // Reasonable
                    0.0001f);// Standard

ekf.setMeasurementNoise(1.0f, 1.0f);  // Low R (1m, 1m std dev)
ekf.setInitialCovariance(0.01f, 0.1f, 10.0f, 0.1f);
```

**Expected behavior**:
- Responsive to motion changes
- Accurate position tracking
- GPS measurements effective

### Scenario 3: Slow Movements (Robot, UAV Hover)

**Characteristics**:
- Slow velocity
- High accuracy required
- Lots of time to estimate correctly

**Tuning**:
```cpp
// Trust GPS heavily for position
ekf.setProcessNoise(0.1f,    // Higher, can't predict exact motion
                    1.0f,    // Higher, motion unpredictable
                    0.01f,   // Higher
                    0.001f); // Higher

ekf.setMeasurementNoise(0.25f, 0.25f);  // Very low R (0.5m, 0.5m std dev)
ekf.setInitialCovariance(0.1f, 1.0f, 100.0f, 1.0f);
```

**Expected behavior**:
- Accurate position from GPS
- Low-drift velocity estimate
- Can detect small movements

---

## Validation Checklist

After tuning, verify:

- [ ] Convergence time < 30 seconds from cold start
- [ ] Position uncertainty stabilizes to GPS-level accuracy
- [ ] Covariance remains positive-definite throughout
- [ ] Innovation magnitude typically < 2m (GPS std dev)
- [ ] Filter output is smooth (not jumpy)
- [ ] Filter is responsive (detects motion within 2 seconds)
- [ ] GPS dropout handled gracefully (smooth dead reckoning)
- [ ] No NaN or Inf in state or covariance
- [ ] Operation is deterministic (same results for same input)

---

## Advanced Tuning: Adaptive Noise

For even better performance, you can make Q and R adaptive:

### Adaptive R (Based on HDOP/VDOP)

```cpp
void update_measurement_noise(float hdop, float vdop) {
    float r_h = (hdop * 2.0f) * (hdop * 2.0f);  // 2m baseline
    float r_v = (vdop * 2.0f) * (vdop * 2.0f);
    
    ekf.setMeasurementNoise(r_h, r_v);
}

// In main loop:
if (gps_available) {
    update_measurement_noise(gps.hdop, gps.vdop);
    ekf.update(gps.lat, gps.lon, gps.alt);
}
```

### Adaptive Q (Based on Innovation)

```cpp
void update_process_noise(float innovation) {
    float innovation_factor = 1.0f;
    
    if (innovation > 5.0f) {
        // Large innovation = model mismatch, increase Q
        innovation_factor = 2.0f;
    } else if (innovation < 0.1f) {
        // Small innovation = model good, decrease Q
        innovation_factor = 0.5f;
    }
    
    ekf.setProcessNoise(0.01f * innovation_factor,
                        0.1f * innovation_factor,
                        0.001f * innovation_factor,
                        0.0001f * innovation_factor);
}

// In main loop:
ekf.predict(...);
ekf.update(...);
update_process_noise(ekf.getLastInnovationMagnitude());
```

---

## Practical Tips

### Tip 1: Log Everything During Initial Tuning

```cpp
void log_debug_data() {
    float state[16];
    float P[256];
    ekf.getState(state);
    ekf.getCovariance(P);
    
    printf("TIME_MS,P_N,P_E,P_D,UNC_P_N,UNC_P_E,UNC_P_D,"
           "V_N,V_E,V_D,INNOVATION,GPS_VALID,PRED,UPD\n");
    
    printf("%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%lu,%lu\n",
           millis(),
           state[7], state[8], state[9],
           sqrt(P[7*16+7]), sqrt(P[8*16+8]), sqrt(P[9*16+9]),
           state[4], state[5], state[6],
           ekf.getLastInnovationMagnitude(),
           ekf.isGpsValid(),
           ekf.getPredictCount(),
           ekf.getUpdateCount());
}
```

### Tip 2: Test Against Ground Truth

If possible, collect data with known truth (e.g., from RTK-GPS or surveyed positions):

```cpp
float compute_error(float x_est, float x_truth) {
    return fabs(x_est - x_truth);
}

void validate_tuning(float p_n_est, float p_n_truth) {
    float error = compute_error(p_n_est, p_n_truth);
    float unc = ekf.getUncertaintyForState(7);
    
    if (error > 3 * unc) {
        printf("ERROR exceeds 3-sigma: inconsistent model\n");
    } else if (error < 0.5 * unc) {
        printf("Uncertainty overestimated, could increase Q\n");
    } else {
        printf("Tuning consistent with ground truth\n");
    }
}
```

### Tip 3: Use Batch Re-tuning

If tuning is difficult, run the system multiple times and compare:

```cpp
struct TuningResult {
    float q_att, q_vel, q_pos, q_bias;
    float r_h, r_v;
    float mean_error;
    float mean_uncertainty;
    float convergence_time;
};

// Try multiple parameter sets, evaluate each
for (int trial = 0; trial < 10; trial++) {
    ekf.reset();
    configure_ekf_trial(trial);
    
    TuningResult result = run_test_scenario();
    
    if (result.mean_error < best_error) {
        best_error = result.mean_error;
        best_params = current_params;
    }
}
```

---

## Troubleshooting

### "Filter always predicts, never updates"

**Symptoms**:
- `ekf.getUpdateCount()` stays at 0
- Covariance grows unbounded
- Filter diverges

**Cause**:
- GPS dropout flag always set
- GPS initialization never called

**Fix**:
```cpp
// Ensure GPS is initialized first
CoordinateFrame frame;
frame.set_reference(lat_ref, lon_ref, alt_ref);

// Then call update
ekf.setGpsDropout(false);  // Make sure this is set
ekf.update(lat, lon, alt);
```

### "Filter stays in dropout forever"

**Symptoms**:
- `ekf.isGpsValid()` always false
- No uncertainty reduction

**Cause**:
- `setGpsDropout(false)` never called
- GPS validity checking broken

**Fix**:
```cpp
// Periodically check and reset GPS status
if (gps.has_fix() && gps_dropout_timer > GPS_LOCK_TIMEOUT) {
    printf("GPS re-acquired, resuming updates\n");
    ekf.setGpsDropout(false);
    gps_dropout_timer = 0;
}

if (!gps.has_fix() && !ekf.isGpsValid()) {
    ekf.setGpsDropout(true);
    gps_dropout_timer++;
}
```

### "Uncertainty goes to zero then becomes negative"

**Symptoms**:
- `isCovariancePositiveDefinite()` returns false
- Variance values are negative (impossible!)

**Cause**:
- Numerical instability in matrix operations
- Parameters are extreme (very high or very low Q/R)

**Fix**:
```cpp
// Enforce stability periodically (e.g., every 100 updates)
if (ekf.getUpdateCount() % 100 == 0) {
    ekf.enforceCovarianceSymmetry();
    ekf.clampCovarianceDiagonal(1e-8f);
    
    if (!ekf.isCovariancePositiveDefinite()) {
        printf("WARNING: Covariance not PSD, resetting\n");
        ekf.reset();
    }
}

// Also adjust parameters to be less extreme
// Use default values and adjust slightly if needed
```

---

## Summary

Good EKF tuning is iterative:
1. Measure sensor noise (GPS, IMU)
2. Set R based on sensor noise
3. Start with default Q values
4. Test and observe behavior
5. Adjust Q, R, P0 based on observations
6. Validate against ground truth if possible
7. Repeat for different scenarios

The key is understanding what each parameter does and having good logging to see what's happening. With this guide and the example code, you should be able to achieve good performance in most scenarios.

