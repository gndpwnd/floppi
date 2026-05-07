# Extended Kalman Filter (EKF) API Reference

Complete API documentation for the `ExtendedKalmanFilter` class used in Phase 3 GPS-IMU sensor fusion.

**Location**: `auto_orientation/src/navigation/ekf.h`
**State Size**: 16 elements (quaternion, velocity, position, accelerometer bias)
**Matrix Operations**: 16×16 covariance, 256 float arrays, pre-allocated (no dynamic memory)

---

## Table of Contents

1. [Class Overview](#class-overview)
2. [Initialization & Reset](#initialization--reset)
3. [Process Steps](#process-steps)
4. [State Accessors (Getters)](#state-accessors-getters)
5. [State Accessors (Setters)](#state-accessors-setters)
6. [GPS Dropout Handling](#gps-dropout-handling)
7. [Covariance & Tuning](#covariance--tuning)
8. [Diagnostics](#diagnostics)
9. [Example Code](#example-code)
10. [State Vector Reference](#state-vector-reference)
11. [Common Pitfalls](#common-pitfalls)

---

## Class Overview

```cpp
class ExtendedKalmanFilter {
 public:
    // Construction and initialization
    ExtendedKalmanFilter();
    void initialize(const Quaternion& quat, ...);
    void reset();
    
    // Process steps
    void predict(float gyro_x, float gyro_y, float gyro_z,
                 float accel_x, float accel_y, float accel_z, float dt);
    bool update(double lat_deg, double lon_deg, double alt_m,
                float hdop = 1.0f, float vdop = 1.0f);
    
    // State access (getters)
    Quaternion getAttitude() const;
    void getVelocity(float& v_n, float& v_e, float& v_d) const;
    float getSpeed() const;
    void getPosition(float& p_n, float& p_e, float& p_d) const;
    float getDistance() const;
    float getHorizontalDistance() const;
    void getAccelerometerBias(float& b_x, float& b_y, float& b_z) const;
    void getState(float state_out[16]) const;
    void getUncertainty(float uncertainty[16]) const;
    float getUncertaintyForState(int state_index) const;
    
    // State access (setters)
    void setAttitude(const Quaternion& quat);
    void setVelocity(float v_north, float v_east, float v_down);
    void setPosition(float p_north, float p_east, float p_down);
    void setAccelerometerBias(float bias_x, float bias_y, float bias_z);
    
    // GPS dropout handling
    void setGpsDropout(bool dropout);
    bool isGpsValid() const;
    float getGpsDropoutDuration() const;
    
    // Covariance and tuning
    void setProcessNoise(float q_attitude, float q_velocity,
                         float q_position, float q_bias);
    void setMeasurementNoise(float r_gps_h, float r_gps_v);
    void setInitialCovariance(float p_att, float p_vel,
                              float p_pos, float p_bias);
    void getCovariance(float P_out[256]) const;
    bool isCovariancePositiveDefinite() const;
    void enforceCovarianceSymmetry();
    void clampCovarianceDiagonal(float min_value = 1e-8f);
    
    // Diagnostics
    bool isStateValid() const;
    bool isCovarianceValid() const;
    float getLastInnovationMagnitude() const;
    uint32_t getUpdateCount() const;
    uint32_t getPredictCount() const;
    
    // Constants
    static constexpr float GRAVITY = 9.81f;
};
```

---

## Initialization & Reset

### Constructor

```cpp
ExtendedKalmanFilter ekf;
```

**Creates** an EKF instance with default state and covariance.

**Default State**:
- Quaternion: [1, 0, 0, 0] (identity, no rotation)
- Velocity: [0, 0, 0] (stationary)
- Position: [0, 0, 0] (origin)
- Accelerometer bias: [0, 0, 0] (no bias)

**Default Covariance**:
- Attitude uncertainty: 0.1 rad²
- Velocity uncertainty: 1.0 m²/s²
- Position uncertainty: 100.0 m²
- Bias uncertainty: 1.0 m²/s⁴

### Initialize with Custom Values

```cpp
Quaternion q_initial(1.0f, 0.0f, 0.0f, 0.0f);  // Identity
float vel_north = 0.5f;      // 0.5 m/s north
float vel_east = 0.0f;
float vel_down = 0.0f;
float pos_north = 10.0f;     // 10 meters north
float pos_east = 5.0f;       // 5 meters east
float pos_down = 0.0f;
float bias_x = 0.0f;
float bias_y = 0.0f;
float bias_z = 0.0f;

ekf.initialize(q_initial, vel_north, vel_east, vel_down,
               pos_north, pos_east, pos_down,
               bias_x, bias_y, bias_z);
```

**Parameters**:
- `quat`: Initial attitude (normalized automatically)
- `vel_*`: Initial velocity components
- `pos_*`: Initial position components
- `bias_*`: Initial accelerometer bias (often zero)

### Reset

```cpp
ekf.reset();
```

Resets EKF to default state and covariance. Useful for:
- Starting a new flight
- Recovering from error conditions
- Testing/debugging

---

## Process Steps

### Predict (IMU Update)

```cpp
float gyro_x = 0.1f;    // rad/s
float gyro_y = 0.05f;
float gyro_z = -0.02f;
float accel_x = 0.1f;   // m/s²
float accel_y = -0.05f;
float accel_z = 0.0f;   // ~gravity is -9.81 m/s² in body frame
float dt = 0.01f;       // 10 ms (100 Hz IMU)

ekf.predict(gyro_x, gyro_y, gyro_z,
            accel_x, accel_y, accel_z, dt);
```

**Called at**: IMU sample rate (typically 100 Hz)

**What it does**:
1. Propagates state using kinematic equations
2. Computes Jacobian matrix F
3. Predicts covariance: P = F*P*F^T + Q
4. Normalizes quaternion
5. Updates internal counters

**Parameters**:
- `gyro_*`: Angular velocity in rad/s (from gyroscope)
- `accel_*`: Acceleration in m/s² (from accelerometer, body frame)
- `dt`: Time since last prediction in seconds

**Notes**:
- Should be called at consistent rate for best results
- Accelerometer reading should include gravity
- Gyroscope bias (if tracked in state) is automatically subtracted

### Update (GPS Position)

```cpp
double lat_deg = 47.3667;      // degrees
double lon_deg = 11.1833;
double alt_m = 520.0;          // meters
float hdop = 1.5f;             // horizontal dilution of precision
float vdop = 2.0f;             // vertical dilution of precision

bool success = ekf.update(lat_deg, lon_deg, alt_m, hdop, vdop);
```

**Called at**: GPS update rate (typically 1-10 Hz)

**What it does**:
1. Converts GPS (lat/lon/alt) to NED position
2. Computes innovation (measurement residual)
3. Computes Kalman gain K
4. Updates state: x = x + K * innovation
5. Updates covariance: P = (I - K*H)*P
6. Enforces numerical stability

**Parameters**:
- `lat_deg`, `lon_deg`: GPS coordinates in degrees (WGS84)
- `alt_m`: Altitude in meters (MSL)
- `hdop`: Horizontal Dilution of Precision (optional, default 1.0)
- `vdop`: Vertical Dilution of Precision (optional, default 1.0)

**Return Value**:
- `true`: Update succeeded
- `false`: Invalid GPS coordinate (e.g., out of range)

**HDOP/VDOP Effect**:
If provided, HDOP/VDOP are used to scale measurement noise R:
```
R_horizontal = (HDOP × 2.0 m)²
R_vertical = (VDOP × 2.0 m)²
```

If not provided (defaults to 1.0), uses nominal values:
```
R_horizontal = 4.0 m²  (2m std dev)
R_vertical = 4.0 m²    (2m std dev)
```

---

## State Accessors (Getters)

### Quaternion (Attitude)

```cpp
// Get as Quaternion object
Quaternion q = ekf.getAttitude();
printf("Quaternion: [%.4f, %.4f, %.4f, %.4f]\n", 
       q.w, q.x, q.y, q.z);

// Get individual components
float w = ekf.getQuatW();
float x = ekf.getQuatX();
float y = ekf.getQuatY();
float z = ekf.getQuatZ();
```

**Interpretation**:
- Represents body-frame to NED-frame rotation
- Always normalized (magnitude ≈ 1)
- Components: [w, x, y, z]

### Velocity

```cpp
float v_north, v_east, v_down;
ekf.getVelocity(v_north, v_east, v_down);
printf("Velocity: North=%.2f m/s, East=%.2f m/s, Down=%.2f m/s\n",
       v_north, v_east, v_down);

// Get speed magnitude
float speed = ekf.getSpeed();
printf("Speed: %.2f m/s\n", speed);
```

**Interpretation**:
- NED frame: North, East, Down directions
- Positive Down = moving downward
- Speed = sqrt(v_n² + v_e² + v_d²)

### Position

```cpp
float p_north, p_east, p_down;
ekf.getPosition(p_north, p_east, p_down);
printf("Position: N=%.2f m, E=%.2f m, D=%.2f m\n",
       p_north, p_east, p_down);

// Get distance from origin
float dist = ekf.getDistance();
printf("Distance from origin: %.2f m\n", dist);

// Get horizontal distance
float horiz_dist = ekf.getHorizontalDistance();
printf("Horizontal distance: %.2f m\n", horiz_dist);
```

**Interpretation**:
- Position relative to GPS reference point (origin)
- NED frame: North, East, Down
- Positive Down = below reference altitude
- Both relative to frame origin set by `CoordinateFrame`

### Accelerometer Bias

```cpp
float bias_x, bias_y, bias_z;
ekf.getAccelerometerBias(bias_x, bias_y, bias_z);
printf("Bias: X=%.4f, Y=%.4f, Z=%.4f m/s²\n",
       bias_x, bias_y, bias_z);
```

**Interpretation**:
- Residual acceleration not due to motion
- Evolves slowly during operation
- Can be used to improve sensor calibration

### Complete State Vector

```cpp
float state[16];
ekf.getState(state);

// State layout:
// [0-3]:   Quaternion [w, x, y, z]
// [4-6]:   Velocity [v_n, v_e, v_d]
// [7-9]:   Position [p_n, p_e, p_d]
// [10-12]: Accelerometer bias [b_x, b_y, b_z]
// [13-15]: Reserved
```

### Uncertainty (Standard Deviation)

```cpp
float uncertainty[16];
ekf.getUncertainty(uncertainty);

// uncertainty[i] = sqrt(P[i,i])
// = standard deviation of state[i]

printf("Position std dev: %.2f m\n", uncertainty[7]);
printf("Velocity std dev: %.2f m/s\n", uncertainty[4]);

// Or get single component
float pos_unc = ekf.getUncertaintyForState(7);  // North position
printf("North position uncertainty: %.2f m\n", pos_unc);

// Or get combined uncertainties
float pos_unc_total = ekf.getPositionUncertainty();
float vel_unc_total = ekf.getVelocityUncertainty();
float att_unc_deg = ekf.getAttitudeUncertainty();

printf("Position uncertainty: %.2f m\n", pos_unc_total);
printf("Velocity uncertainty: %.2f m/s\n", vel_unc_total);
printf("Attitude uncertainty: %.2f degrees\n", att_unc_deg);
```

**Interpretation**:
- Standard deviation (1-sigma uncertainty)
- Larger value = less confident in that state
- Useful for decision-making and logging

---

## State Accessors (Setters)

### Set Attitude

```cpp
Quaternion q_level(1.0f, 0.0f, 0.0f, 0.0f);  // Level orientation
ekf.setAttitude(q_level);
```

**Use cases**:
- Initialize with known attitude (e.g., from magnetometer or inclinometer)
- Correct attitude if it drifts too far
- Handle discontinuities in sensor data

**Note**: Quaternion is automatically normalized.

### Set Velocity

```cpp
float v_north = 5.0f;   // m/s
float v_east = 0.0f;
float v_down = 0.0f;

ekf.setVelocity(v_north, v_east, v_down);
```

**Use cases**:
- Initialize with known velocity
- Reset velocity estimate after discontinuity
- Seed filter from other navigation system

### Set Position

```cpp
float p_north = 100.0f;  // meters relative to frame origin
float p_east = 50.0f;
float p_down = 0.0f;

ekf.setPosition(p_north, p_east, p_down);
```

**Use cases**:
- Initialize with known position (e.g., from high-accuracy GPS)
- Reset after discontinuity
- Seed from other navigation source

### Set Accelerometer Bias

```cpp
float bias_x = 0.05f;   // m/s²
float bias_y = -0.03f;
float bias_z = 0.02f;

ekf.setAccelerometerBias(bias_x, bias_y, bias_z);
```

**Use cases**:
- Initialize with pre-calibrated bias values
- Correct systematic bias from accelerometer
- Transfer bias estimate between flights

---

## GPS Dropout Handling

### Set GPS Dropout Flag

```cpp
// GPS signal lost
ekf.setGpsDropout(true);

// In dead-reckoning mode, predict() still works
// but update() calls are ignored
for (int i = 0; i < 100; i++) {
    ekf.predict(gyro_x, gyro_y, gyro_z,
                accel_x, accel_y, accel_z, dt);
    // No update() calls during dropout
}

// GPS signal regained
ekf.setGpsDropout(false);

// Now update() calls resume
ekf.predict(gyro_x, gyro_y, gyro_z,
            accel_x, accel_y, accel_z, dt);
ekf.update(lat, lon, alt);  // Works again
```

**When GPS Dropout = True**:
- `predict()` still propagates state (dead reckoning)
- `update()` calls are silently ignored (no effect)
- Covariance grows without measurement corrections
- No error flags or exceptions

**When GPS Dropout = False**:
- Both `predict()` and `update()` work normally

### Check GPS Status

```cpp
if (ekf.isGpsValid()) {
    printf("GPS is valid\n");
} else {
    printf("GPS is in dropout\n");
}
```

### Get Dropout Duration

```cpp
float dropout_time = ekf.getGpsDropoutDuration();
printf("GPS has been out for %.1f seconds\n", dropout_time);

// Practical use: decide when to switch to backup navigation
if (dropout_time > 60.0f) {
    printf("GPS out for >60s, switching to compass-only mode\n");
}
```

---

## Covariance & Tuning

### Set Process Noise (Q Matrix)

```cpp
float q_attitude = 0.01f;    // rad²/s²
float q_velocity = 0.1f;     // m²/s³
float q_position = 0.001f;   // m²/s²
float q_bias = 0.0001f;      // m²/s⁵

ekf.setProcessNoise(q_attitude, q_velocity, q_position, q_bias);
```

**Effect on Filter**:
- **Larger Q**: Filter trusts IMU measurements less, responds to GPS more quickly
- **Smaller Q**: Filter trusts IMU predictions more, smoother output

**Typical Values** (for 100 Hz IMU, 1 Hz GPS):
```cpp
// Conservative (trusts GPS)
ekf.setProcessNoise(0.1f, 1.0f, 0.01f, 0.001f);

// Balanced (default)
ekf.setProcessNoise(0.01f, 0.1f, 0.001f, 0.0001f);

// Aggressive (trusts IMU)
ekf.setProcessNoise(0.001f, 0.01f, 0.0001f, 0.00001f);
```

### Set Measurement Noise (R Matrix)

```cpp
float r_gps_horizontal = 4.0f;   // m² (GPS std dev = 2m)
float r_gps_vertical = 9.0f;     // m² (GPS std dev = 3m)

ekf.setMeasurementNoise(r_gps_horizontal, r_gps_vertical);
```

**Effect on Filter**:
- **Larger R**: Filter trusts GPS less, smoother output, slower to respond
- **Smaller R**: Filter trusts GPS more, faster response, can be jumpy

**Practical Formula**:
```cpp
float gps_std_h = 2.0f;  // meters
float gps_std_v = 3.0f;  // meters

ekf.setMeasurementNoise(gps_std_h * gps_std_h,
                        gps_std_v * gps_std_v);
```

**With HDOP/VDOP**:
```cpp
float hdop = 1.5f;  // horizontal dilution from GPS
float vdop = 2.0f;  // vertical dilution from GPS

ekf.setMeasurementNoise((hdop * 2.0f) * (hdop * 2.0f),
                        (vdop * 2.0f) * (vdop * 2.0f));
```

### Set Initial Covariance

```cpp
float p_attitude = 0.1f;     // rad²
float p_velocity = 1.0f;     // m²/s²
float p_position = 400.0f;   // m² (20m std dev)
float p_bias = 1.0f;         // m²/s⁴

ekf.setInitialCovariance(p_attitude, p_velocity, 
                         p_position, p_bias);
```

**Effect**:
- **Large initial P**: Filter trusts nothing initially, takes longer to converge
- **Small initial P**: Filter converges quickly, risky if initial state is wrong

**Use Cases**:
```cpp
// Cold start (unknown position)
ekf.setInitialCovariance(0.1f, 1.0f, 1000.0f, 1.0f);

// Known state (e.g., from previous flight)
ekf.setInitialCovariance(0.01f, 0.1f, 10.0f, 0.1f);

// Manual initialization (large uncertainty)
ekf.setInitialCovariance(0.5f, 5.0f, 1000.0f, 5.0f);
```

### Get Covariance Matrix

```cpp
float P[256];  // 16×16 covariance matrix
ekf.getCovariance(P);

// Access element: P[i*16 + j]
float pos_var = P[7*16 + 7];      // Variance of North position
float vel_std = sqrt(P[4*16 + 4]); // Std dev of North velocity
```

### Check Covariance Quality

```cpp
// Verify positive-definite
if (ekf.isCovariancePositiveDefinite()) {
    printf("Covariance is valid\n");
} else {
    printf("ERROR: Covariance is not positive-definite!\n");
}
```

### Enforce Numerical Stability

```cpp
// Enforce symmetry (already done automatically)
ekf.enforceCovarianceSymmetry();

// Clamp diagonal to prevent negative variance
ekf.clampCovarianceDiagonal(1e-8f);  // minimum variance = 1e-8
```

---

## Diagnostics

### Check State Validity

```cpp
if (ekf.isStateValid()) {
    printf("State is valid (no NaN/Inf)\n");
} else {
    printf("ERROR: State contains NaN or Inf!\n");
}
```

### Check Covariance Validity

```cpp
if (ekf.isCovarianceValid()) {
    printf("Covariance is valid\n");
} else {
    printf("ERROR: Covariance contains NaN/Inf!\n");
}
```

### Get Innovation Magnitude

```cpp
float innovation = ekf.getLastInnovationMagnitude();
printf("Last GPS innovation: %.2f m\n", innovation);

// Innovation should typically be < 2m (low residual error)
if (innovation > 5.0f) {
    printf("WARNING: Large GPS measurement innovation\n");
}
```

### Get Operation Counts

```cpp
uint32_t predict_count = ekf.getPredictCount();
uint32_t update_count = ekf.getUpdateCount();

printf("Predict steps: %lu\n", predict_count);
printf("Update steps: %lu\n", update_count);
printf("Ratio: %.1f predictions per update\n",
       (float)predict_count / update_count);
```

---

## Example Code

### Example 1: Basic GPS-IMU Fusion Loop

```cpp
#include "ekf.h"

ExtendedKalmanFilter ekf;

void setup() {
    // Initialize EKF
    Quaternion q_level(1.0f, 0.0f, 0.0f, 0.0f);
    ekf.initialize(q_level, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    
    // Set tuning parameters
    ekf.setProcessNoise(0.01f, 0.1f, 0.001f, 0.0001f);
    ekf.setMeasurementNoise(4.0f, 4.0f);  // 2m std dev GPS
}

void loop() {
    // IMU update (100 Hz)
    static uint32_t last_imu = millis();
    if (millis() - last_imu >= 10) {  // 10 ms = 100 Hz
        last_imu = millis();
        
        float gyro[3], accel[3];
        readIMU(gyro, accel);
        
        float dt = 0.01f;  // seconds
        ekf.predict(gyro[0], gyro[1], gyro[2],
                    accel[0], accel[1], accel[2], dt);
    }
    
    // GPS update (1 Hz)
    static uint32_t last_gps = millis();
    if (millis() - last_gps >= 1000) {
        last_gps = millis();
        
        double lat, lon, alt;
        float hdop, vdop;
        if (readGPS(lat, lon, alt, hdop, vdop)) {
            ekf.update(lat, lon, alt, hdop, vdop);
        } else {
            ekf.setGpsDropout(true);  // No fix
        }
    }
    
    // Output estimated state
    float p_n, p_e, p_d;
    ekf.getPosition(p_n, p_e, p_d);
    
    float v_n, v_e, v_d;
    ekf.getVelocity(v_n, v_e, v_d);
    
    float p_unc = ekf.getPositionUncertainty();
    
    printf("Pos: (%.1f, %.1f, %.1f) Vel: (%.1f, %.1f, %.1f) Unc: %.1f m\n",
           p_n, p_e, p_d, v_n, v_e, v_d, p_unc);
}
```

### Example 2: Logging with Uncertainty

```cpp
void log_state() {
    float state[16];
    ekf.getState(state);
    
    float uncertainty[16];
    ekf.getUncertainty(uncertainty);
    
    printf("TIME_MS,POS_N,POS_E,POS_D,"
           "UNC_POS_N,UNC_POS_E,UNC_POS_D,"
           "VEL_N,VEL_E,VEL_D,PRED,UPD\n");
    
    printf("%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%lu,%lu\n",
           millis(),
           state[7], state[8], state[9],
           uncertainty[7], uncertainty[8], uncertainty[9],
           state[4], state[5], state[6],
           ekf.getPredictCount(), ekf.getUpdateCount());
}
```

### Example 3: Handling GPS Dropout

```cpp
float gps_timeout = 30.0f;  // seconds

void check_gps_health() {
    if (!ekf.isGpsValid()) {
        // GPS in dropout
        float dropout_duration = ekf.getGpsDropoutDuration();
        
        if (dropout_duration > gps_timeout) {
            // Extended outage - switch to backup navigation
            printf("GPS timeout! Using backup navigation\n");
            switch_to_compass_only();
        } else {
            // Short outage - dead reckoning sufficient
            float pos_unc = ekf.getPositionUncertainty();
            printf("GPS dropout %.1f s, position uncertainty %.1f m\n",
                   dropout_duration, pos_unc);
        }
    } else {
        // GPS valid
        float innovation = ekf.getLastInnovationMagnitude();
        
        if (innovation > 5.0f) {
            // Large measurement residual
            printf("Large GPS residual: %.1f m\n", innovation);
            // Could indicate GPS error or bad initial state
        }
    }
}
```

### Example 4: Tuning for Different Scenarios

```cpp
enum Scenario {
    URBAN,      // GPS multipath, lots of dropout
    HIGHWAY,    // Good GPS, high speed
    RURAL,      // Good GPS, low speed
};

void configure_ekf(Scenario scenario) {
    switch (scenario) {
    case URBAN:
        // Trusts IMU more (GPS unreliable)
        ekf.setProcessNoise(0.001f, 0.01f, 0.0001f, 0.00001f);
        ekf.setMeasurementNoise(16.0f, 16.0f);  // 4m std dev
        ekf.setInitialCovariance(0.5f, 5.0f, 1000.0f, 5.0f);
        break;
        
    case HIGHWAY:
        // Balanced (fast motion, reliable GPS)
        ekf.setProcessNoise(0.01f, 0.1f, 0.001f, 0.0001f);
        ekf.setMeasurementNoise(4.0f, 4.0f);   // 2m std dev
        ekf.setInitialCovariance(0.1f, 1.0f, 100.0f, 1.0f);
        break;
        
    case RURAL:
        // Trusts GPS more (IMU reliable, good GPS)
        ekf.setProcessNoise(0.1f, 1.0f, 0.01f, 0.001f);
        ekf.setMeasurementNoise(1.0f, 1.0f);   // 1m std dev
        ekf.setInitialCovariance(0.01f, 0.1f, 10.0f, 0.1f);
        break;
    }
}
```

---

## State Vector Reference

### Complete State Definition

```
State vector x[16]:

Index  Component          Meaning                    Units
─────────────────────────────────────────────────────────────
0      q_w               Quaternion (scalar)        -
1      q_x               Quaternion (vector X)      -
2      q_y               Quaternion (vector Y)      -
3      q_z               Quaternion (vector Z)      -
4      v_north           Velocity North             m/s
5      v_east            Velocity East              m/s
6      v_down            Velocity Down              m/s
7      p_north           Position North             m
8      p_east            Position East              m
9      p_down            Position Down              m
10     b_accel_x         Accel bias X               m/s²
11     b_accel_y         Accel bias Y               m/s²
12     b_accel_z         Accel bias Z               m/s²
13-15  (reserved)        Future use                 -
```

### Quaternion Representation

The quaternion q = [w, x, y, z] represents a 3D rotation:
- **w**: Scalar (real) part
- **x, y, z**: Vector part
- **Magnitude**: sqrt(w² + x² + y² + z²) = 1 (normalized)
- **Meaning**: Rotates vectors from body frame to NED frame

### Coordinate Frames

**Body Frame** (vehicle-fixed):
- **+X**: Forward (nose direction)
- **+Y**: Right wing
- **+Z**: Down

**NED Frame** (Earth-fixed, local):
- **+N**: North
- **+E**: East
- **+D**: Down (toward center of Earth)

### Velocity & Position Interpretation

**Velocity** [v_n, v_e, v_d]:
- Time derivative of position
- Positive v_d = moving downward
- Measured in NED frame

**Position** [p_n, p_e, p_d]:
- Relative to GPS reference point (frame origin)
- Positive p_d = below reference altitude
- Set by CoordinateFrame::set_reference()

### Uncertainty Interpretation

For each state component x_i, the uncertainty is:

```
std_dev_i = sqrt(P[i*16 + i])
```

**Example**:
```cpp
float P[256];
ekf.getCovariance(P);

float pos_std = sqrt(P[7*16 + 7]);  // North position std dev

// Interpretation:
// Position estimate is p_n ± pos_std (68% confidence, 1-sigma)
// Position estimate is p_n ± 2*pos_std (95% confidence, 2-sigma)
// Position estimate is p_n ± 3*pos_std (99.7% confidence, 3-sigma)
```

---

## Common Pitfalls

### Pitfall 1: Forgetting to Set Reference Frame

```cpp
// WRONG: No GPS reference set
CoordinateFrame frame;  // Not initialized!
ekf.update(47.3667, 11.1833, 520.0);  // Where is (0,0,0)?

// CORRECT: Set reference first
CoordinateFrame frame;
frame.set_reference(47.3667, 11.1833, 520.0);
ekf.update(47.3667, 11.1833, 520.0);
```

### Pitfall 2: Wrong Time Step in predict()

```cpp
// WRONG: Forgetting to convert ms to seconds
uint32_t last_time = millis();
// ...
uint32_t dt_ms = millis() - last_time;
ekf.predict(gx, gy, gz, ax, ay, az, dt_ms);  // Treats ms as seconds!

// CORRECT: Convert to seconds
ekf.predict(gx, gy, gz, ax, ay, az, dt_ms / 1000.0f);
```

### Pitfall 3: Inconsistent Accelerometer Units

```cpp
// WRONG: IMU returns acceleration in g's, filter expects m/s²
float accel_g[3];  // in units of 1g
imu.read_accel(accel_g);
ekf.predict(gx, gy, gz, accel_g[0], accel_g[1], accel_g[2], dt);

// CORRECT: Convert to m/s²
float accel_m_s2[3];
for (int i = 0; i < 3; i++) {
    accel_m_s2[i] = accel_g[i] * 9.81f;
}
ekf.predict(gx, gy, gz, accel_m_s2[0], accel_m_s2[1], accel_m_s2[2], dt);
```

### Pitfall 4: Not Handling GPS Dropout

```cpp
// WRONG: Ignoring GPS errors
ekf.update(lat, lon, alt);  // Silently fails if invalid GPS

// CORRECT: Check validity and set dropout flag
if (gps.is_valid()) {
    ekf.setGpsDropout(false);
    ekf.update(lat, lon, alt);
} else {
    ekf.setGpsDropout(true);
}
```

### Pitfall 5: Tuning Covariance Values Without Units

```cpp
// WRONG: Guessing values without understanding units
ekf.setProcessNoise(0.5f, 0.5f, 0.5f, 0.5f);  // What units?

// CORRECT: Understand the units and set appropriately
// Q_attitude: rad²/s²
// Q_velocity: m²/s³
// Q_position: m²/s²
// Q_bias: m²/s⁵
ekf.setProcessNoise(0.01f,    // 0.01 rad²/s²
                    0.1f,     // 0.1 m²/s³
                    0.001f,   // 0.001 m²/s²
                    0.0001f); // 0.0001 m²/s⁵
```

### Pitfall 6: Calling update() Without Calling predict()

```cpp
// WRONG: Only updates, no predictions
ekf.update(lat, lon, alt);
ekf.update(lat, lon, alt);  // State never evolved!

// CORRECT: Predict then update
ekf.predict(gx, gy, gz, ax, ay, az, dt);
ekf.update(lat, lon, alt);
```

### Pitfall 7: Negative Covariance (Numerical Instability)

```cpp
// Can happen after many cycles without clamping
// Indicate covariance diverged

// FIX: Add periodically (after batches of updates)
ekf.enforceCovarianceSymmetry();
ekf.clampCovarianceDiagonal(1e-8f);

if (!ekf.isCovariancePositiveDefinite()) {
    printf("ERROR: Covariance not positive-definite, resetting\n");
    ekf.reset();
}
```

---

## Summary

The EKF API is designed for embedded systems:
- **No dynamic memory**: All allocations pre-sized
- **No exceptions**: Returns bool or void, uses status checks
- **Flexible tuning**: Full access to Q, R, P matrices
- **Comprehensive diagnostics**: Validation and monitoring functions
- **GPS dropout handling**: Graceful dead reckoning mode

Key methods for typical use:
1. `ExtendedKalmanFilter()` - Create instance
2. `initialize()` - Set initial state
3. `setProcessNoise()`, `setMeasurementNoise()` - Tune filter
4. `predict()` - IMU measurements (100 Hz)
5. `update()` - GPS measurements (1-10 Hz)
6. `get*()` - Access estimated state and uncertainty

See EKF_TUNING_GUIDE.md for practical tuning procedures.

