# IMU+GPS Sensor Fusion for Aerial Platforms

## Table of Contents

1. [Overview: Why Sensor Fusion?](#overview-why-sensor-fusion)
2. [Sensor Characteristics and Complementary Strengths](#sensor-characteristics-and-complementary-strengths)
3. [Extended Kalman Filter (EKF) Framework](#extended-kalman-filter-ekf-framework)
4. [Error Sources and Uncertainties](#error-sources-and-uncertainties)
5. [Handling Asynchronous Sensors](#handling-asynchronous-sensors)
6. [Coupling Architectures](#coupling-architectures)
7. [Real-World Challenges](#real-world-challenges)
8. [Initialization and Alignment](#initialization-and-alignment)
9. [Practical Implementation Guide](#practical-implementation-guide)
10. [References and Further Reading](#references-and-further-reading)

---

## Overview: Why Sensor Fusion?

### The Problem with Single Sensors

**IMU (Inertial Measurement Unit) Alone:**
- Fast orientation updates (100 Hz+)
- Provides accurate short-term angular velocity and acceleration
- Exhibits unbounded drift over time—gyroscope bias causes heading to drift ~0.5°/minute without correction
- No absolute position information

**GPS Alone:**
- Provides absolute position (when signals are available)
- Slower update rate (1-10 Hz typically)
- Multipath errors cause meter-scale position jumps
- Subject to dropouts and signal loss
- Dilution of precision (DOP) varies with satellite geometry

### Why Fusion Works

Sensor fusion exploits **complementary strengths**:

| Aspect | IMU | GPS |
|--------|-----|-----|
| Update Rate | 100 Hz | 1-10 Hz |
| Drift | Yes (unbounded) | No |
| Short-term Accuracy | Excellent | Poor (noise) |
| Long-term Accuracy | Poor (drift) | Good (absolute reference) |
| Availability | Always | Intermittent |
| Latency | Minimal | ~200-500ms |

A well-designed Kalman filter uses:
- **IMU**: High-frequency state prediction (propagation phase)
- **GPS**: Drift correction and absolute position updates (correction phase)

This combination provides continuous, accurate position and attitude estimates with high temporal resolution.

---

## Sensor Characteristics and Complementary Strengths

### IMU Fundamentals

An IMU typically contains:
- **Triaxial Accelerometer**: Measures linear acceleration (~14-16 bit resolution)
- **Triaxial Gyroscope**: Measures angular velocity
- **Triaxial Magnetometer**: Provides heading reference (subject to interference)

#### BNO085 Specifications

The BNO085 is a 9-DOF sensor with internal sensor fusion:

**Sensor Quality:**
- Gyroscope: 16-bit closed-loop design (low noise)
- Accelerometer: 14-bit resolution
- Magnetometer: BMM150 (subject to interference)

**Rotation Vector Performance:**
- Dynamic error: 3.5°
- Static error: 2.0°
- Heading accuracy: ±3° (with calibration)
- Tilt accuracy: ±1° (with calibration)

**Heading Drift Rate:**
- Without magnetometer: ~0.5°/minute
- This is primarily due to gyroscope bias drift

**Output Formats:**
- Rotation Vector (quaternion)
- Gaming Rotation Vector (gyro + accelerometer only, no magnetic correction)
- Euler angles (roll, pitch, yaw)
- Calibrated angular velocity and acceleration

### GPS Fundamentals

#### Error Sources in GPS/GNSS

**Multipath Error:**
- Signal reflects off buildings, terrain, or obstacles before reaching receiver
- Can cause position errors of 1+ meters
- More severe near ground level; less at altitude
- Difficult to eliminate completely; mitigation through receiver design and data quality filters

**Dilution of Precision (DOP):**
- Geometric quality of satellite distribution
- GDOP (Geometric DOP) combines all dimensions
  - **Ideal**: GDOP < 4
  - **Good**: GDOP 4-6
  - **Acceptable**: GDOP 6-8
  - **Poor**: GDOP > 8
- Cannot be corrected, only monitored and avoided

**Atmospheric Effects:**
- Ionospheric delay: varies with solar activity (seconds in time = meters in position)
- Tropospheric delay: dependent on humidity and pressure
- Somewhat predictable; can be modeled

**Signal Blockage:**
- Complete loss in tunnels, deep canyons, urban environments
- Partial blockage near structure edges

**Typical Accuracy:**
- Standard GPS: 5-10 meters (95% confidence)
- DGPS/RTK: 10 cm - 1 meter
- Good conditions with WAAS/SBAS: 2-5 meters

#### GPS Characteristics for Fusion

- **Position only**: Standard GPS receivers output position (no velocity derivative)
- **Velocity available**: Some receivers can output velocity vectors
- **Update rate**: Typically 1-10 Hz (much slower than IMU)
- **Latency**: 200-500 ms for positioning solution
- **Intermittent availability**: Dropouts during obstructions

---

## Extended Kalman Filter (EKF) Framework

### Why EKF for Aerial Platforms?

The Extended Kalman Filter is the standard choice for IMU+GPS fusion because:

1. **Nonlinear system**: Attitude kinematics and Earth-reference transformations are nonlinear
2. **Computational efficiency**: Real-time feasible on embedded platforms (vs. UKF)
3. **Well-established**: Decades of aviation/aerospace experience
4. **Tunable**: Covariance matrices can be calibrated empirically
5. **Handles asynchronous updates**: Can predict between GPS fixes and correct at GPS rate

**Trade-off**: UKF is more accurate for highly nonlinear problems but requires 3-4x more computation.

### EKF State Vector Design

For aerial platforms, a typical state vector includes:

```
x = [
    q_x, q_y, q_z, q_w,          # Attitude (quaternion) [4]
    v_x, v_y, v_z,               # Velocity (NED frame) [3]
    p_x, p_y, p_z,               # Position (NED frame) [3]
    a_bias_x, a_bias_y, a_bias_z,# Accelerometer bias [3]
    g_bias_x, g_bias_y, g_bias_z,# Gyroscope bias [3]
    m_bias_x, m_bias_y, m_bias_z # Magnetometer bias [3]
]
# Total: 16-19 states depending on configuration
```

**Note on Quaternion Representation:**
- Quaternions avoid gimbal lock and singularities present in Euler angles
- Unit quaternion constraint: q_x² + q_y² + q_z² + q_w² = 1
- This constraint complicates the math but improves numerical stability

### EKF Predict Phase (IMU Update)

Runs at high frequency (~100 Hz):

```
Prediction equations (continuous-time):

dq/dt = 0.5 * q ⊗ ω_meas
      where ω_meas = ω - g_bias (gyroscope - bias)
      and ⊗ is quaternion multiplication

dv/dt = R(q) * a_meas - g
      where a_meas = a - a_bias (accelerometer - bias)
      R(q) = rotation matrix from quaternion q
      g = [0, 0, 9.81] (gravity vector in NED)

dp/dt = v

db_a/dt = n_a (noise in accelerometer bias)
db_g/dt = n_g (noise in gyroscope bias)

Discrete update (using Runge-Kutta or simple integration):

q_{k+1} = q_k + dq/dt * dt
v_{k+1} = v_k + dv/dt * dt
p_{k+1} = p_k + dp/dt * dt
b_{k+1} = b_k + n_process_noise * dt
```

**Covariance Prediction:**

```
P_{k+1|k} = F * P_{k|k} * F^T + Q

where:
  F = Jacobian of state transition (typically 16x16 or 19x19)
  Q = Process noise covariance (characterizes IMU noise)
  P = State error covariance (uncertainty in estimates)
```

The Jacobian F is computed numerically or analytically:

```
F ≈ I + (∂f/∂x) * dt

where f is the state transition function and ∂f/∂x is the Jacobian matrix.
```

### EKF Update Phase (GPS Correction)

Runs at GPS frequency (~1-10 Hz):

```
GPS measurement (position):
z_gps = [p_x_gps, p_y_gps, p_z_gps]^T
      (or [lat, lon, altitude] converted to NED)

Innovation (residual):
y = z_gps - h(x)
  = [p_x_meas - p_x_est, p_y_meas - p_y_est, p_z_meas - p_z_est]^T

Measurement Jacobian (how measurement relates to state):
H = ∂h/∂x = [0 0 0 0  |  0 0 0  |  1 0 0  |  ... ]
             (only position elements are nonzero)

Innovation covariance:
S = H * P * H^T + R

where R = measurement noise covariance (GPS accuracy)

Kalman gain:
K = P * H^T * S^-1

State correction:
x = x + K * y

Covariance update:
P = (I - K * H) * P

```

**Key Insight**: The Kalman gain K automatically weights:
- High uncertainty in state estimate → large K (trust measurement)
- High measurement noise → small K (trust prediction)
- This is optimal in a least-squares sense

### Complete EKF Cycle Pseudocode

```python
class EKF:
    def __init__(self, initial_state, initial_covariance, process_noise, measurement_noise):
        self.x = initial_state          # 16-19D state vector
        self.P = initial_covariance    # Error covariance matrix
        self.Q = process_noise         # IMU noise covariance
        self.R = measurement_noise     # GPS noise covariance
        self.dt_imu = 0.01             # 100 Hz IMU
        self.dt_gps = 0.1              # 10 Hz GPS (example)
    
    def predict(self, accel, gyro, magnetometer, dt=None):
        """
        Runs at high frequency from IMU readings.
        Propagates state and covariance forward.
        """
        if dt is None:
            dt = self.dt_imu
        
        # Remove bias
        accel_unbiased = accel - self.x['a_bias']
        gyro_unbiased = gyro - self.x['g_bias']
        
        # Integrate state (quaternion, velocity, position)
        self.x['q'] = quaternion_integrate(self.x['q'], gyro_unbiased, dt)
        R = quaternion_to_rotation_matrix(self.x['q'])
        self.x['v'] += (R @ accel_unbiased - gravity) * dt
        self.x['p'] += self.x['v'] * dt
        
        # Compute Jacobian F and integrate covariance
        F = compute_state_jacobian(self.x, accel_unbiased, gyro_unbiased)
        self.P = F @ self.P @ F.T + self.Q
        
        # Optional: Normalize quaternion to unit norm
        self.x['q'] = self.x['q'] / norm(self.x['q'])
    
    def update(self, gps_position, gps_covariance):
        """
        Runs at GPS frequency (~10 Hz).
        Corrects state and covariance using GPS measurement.
        """
        # Innovation (measurement residual)
        z = gps_position
        h = self.x['p']  # Predicted position
        y = z - h
        
        # Measurement Jacobian (extract position from state)
        H = zeros((3, 16))
        H[0:3, 9:12] = eye(3)  # Position is states 9-11
        
        # Innovation covariance
        S = H @ self.P @ H.T + gps_covariance
        
        # Kalman gain
        K = self.P @ H.T @ inv(S)
        
        # Correct state
        self.x += K @ y
        
        # Correct covariance
        self.P = (eye(16) - K @ H) @ self.P
        
        # Normalize quaternion
        self.x['q'] = self.x['q'] / norm(self.x['q'])
    
    def get_state(self):
        """Return current best estimate"""
        return {
            'attitude': quaternion_to_euler(self.x['q']),
            'velocity': self.x['v'],
            'position': self.x['p'],
            'covariance': self.P
        }
```

### Numerical Integration Methods

**Simple Euler (first-order):**
```
x_{k+1} = x_k + f(x_k, u_k) * dt
```
- Fast, low computational cost
- Accumulates integration error over time

**Runge-Kutta 4 (RK4, fourth-order):**
```
k1 = f(x_k, u_k)
k2 = f(x_k + 0.5*k1*dt, u_k)
k3 = f(x_k + 0.5*k2*dt, u_k)
k4 = f(x_k + k3*dt, u_k)
x_{k+1} = x_k + (k1 + 2*k2 + 2*k3 + k4) * dt / 6
```
- More accurate (errors ~O(dt^5))
- Recommended for high precision aerial systems

**Coning and Sculling Corrections (Advanced):**
- Account for IMU rotation during integration interval
- Standard in high-performance INS systems
- Significant improvement at 100+ Hz sampling rates

---

## Error Sources and Uncertainties

### Process Noise Covariance (Q Matrix)

Characterizes uncertainty in the prediction model:

```
Q = diag([σ_q², ..., σ_v², ..., σ_p², ..., σ_bias²])
```

**Typical values:**

| State | Noise Source | Typical σ |
|-------|--------------|-----------|
| Quaternion | Integration error, gyro noise | ~0.001 rad |
| Velocity | Accel noise, integration error | ~0.01 m/s |
| Position | Velocity integration error | ~0.01 m |
| Accel bias | Slow thermal/electronic drift | ~0.0005 m/s² per update |
| Gyro bias | Slow thermal drift | ~0.001 °/s per update |

**Setting Q:**
- Too small: Filter trusts prediction too much → diverges when GPS unavailable
- Too large: Filter trusts GPS too much → high-frequency noise in estimates
- Tune empirically by observing residuals during GPS availability

### BNO085 Error Characteristics

**Noise Sources:**
- Gyroscope noise: ~10-20 °/s/√Hz (white noise)
- Accelerometer noise: ~20-50 mg/√Hz
- Magnetometer subject to environmental interference

**Drift Mechanisms:**
1. **Gyroscope bias drift**: ~0.5°/min over minutes to hours
   - Temperature-dependent
   - Partially corrected by BNO085 internal firmware
   
2. **Accelerometer bias**: ~5-20 mg (gravity-sensitive)
   - Changes with temperature
   - Less critical than gyro bias for brief flights
   
3. **Heading drift without magnetometer**: ~0.5°/min
   - Gyroscope integration error
   - Mitigated by GPS corrections

**In Kalman filter context:**
- Process noise accounts for gyro bias drift rate
- State vector includes bias terms that are estimated and corrected
- Over time, EKF learns the actual bias values

### GPS Error Characteristics

**Measurement Covariance (R Matrix):**

```
R = diag([σ_x², σ_y², σ_z²])
```

**Typical values (standard GPS):**
- Horizontal (X, Y): 5-10 meters σ
- Vertical (Z): 10-20 meters σ
- Higher when DOP is poor (>5)
- Much better with DGPS/RTK (~0.1 meters)

**Error Characteristics:**
1. **Multipath errors**: 
   - Cause 1-5 meter jumps
   - Correlated over short timescales
   - Difficult to model; use outlier rejection
   
2. **Dilution of precision (DOP):**
   - σ_position ≈ σ_baseline × DOP
   - Monitor DOP; inflate R or gate measurements when DOP > 8
   
3. **Atmospheric biases:**
   - Slower, more predictable
   - Mostly characterized by position offset, not noise

**Handling Variable Measurement Covariance:**

```python
def update_with_variable_r(self, gps_position, hdop, vdop):
    """Adjust measurement covariance based on DOP"""
    baseline_accuracy = 5.0  # meters
    
    # Inflate covariance based on DOP
    sigma_h = baseline_accuracy * hdop
    sigma_v = baseline_accuracy * (vdop / hdop)  # Approximate
    
    R = diag([sigma_h**2, sigma_h**2, sigma_v**2])
    
    # Proceed with standard update using this R
    self.update(gps_position, R)
```

### Uncertainty Propagation

The EKF maintains a **covariance matrix P** that quantifies uncertainty in each state:

**Meaning of covariance elements:**
- Diagonal elements: Variance of each state estimate
  - P[i,i] = σ_i² (uncertainty in state i)
- Off-diagonal elements: Correlation between states
  - P[i,j] describes how errors in states i and j are correlated

**During predict phase:**
```
P_{predict} = F * P_{previous} * F^T + Q
```
- Uncertainty grows due to IMU noise (Q term)
- Correlations between states evolve based on Jacobian F

**During update phase:**
```
P_{updated} = (I - K*H) * P_{predicted}
```
- Uncertainty shrinks (GPS provides external information)
- Amount of correction weighted by Kalman gain K

**Steady-state covariance (with regular GPS fixes):**
- Reaches equilibrium where prediction uncertainty growth = GPS correction
- Position uncertainty dominated by GPS accuracy, then smoothed by IMU
- Attitude uncertainty grows between GPS updates but is corrected by accelerometer/magnetometer

---

## Handling Asynchronous Sensors

### The Timing Challenge

- **IMU**: 100 Hz (10 ms updates)
- **GPS**: 1-10 Hz (100-1000 ms updates)
- **Magnetometer**: Often merged into IMU signal
- **Barometer** (if present): Variable, often 20-50 Hz

Most sensors do NOT update synchronously. A well-designed system must handle:
1. IMU updates arriving regularly
2. GPS updates arriving intermittently
3. Mixed update buffers (GPS behind several IMU updates)

### Continuous-Discrete EKF

The standard solution: **continuous IMU model with discrete GPS updates**

```
State dynamics (continuous in time):
  x_dot = f(x, u)        # f depends on IMU measurements
  
Measurement model (discrete at GPS times):
  z_k = h(x_k) + v_k     # v_k ~ N(0, R)

Implementation:
  1. IMU arrives: Integrate state continuously using RK4
     Integrate covariance using Jacobian: P_dot = F*P + P*F^T + Q
  2. GPS arrives: Apply discrete correction update
     (Standard EKF update equations)
```

**Pseudocode:**

```python
class AsyncEKF:
    def __init__(self, state, covariance, process_noise, measurement_noise):
        self.x = state
        self.P = covariance
        self.Q = process_noise
        self.R = measurement_noise
        self.last_imu_time = 0
    
    def imu_callback(self, accel, gyro, timestamp):
        """Called at high frequency (~100 Hz)"""
        dt = timestamp - self.last_imu_time
        self.last_imu_time = timestamp
        
        # Predict phase using continuous integration
        self._predict(accel, gyro, dt)
    
    def _predict(self, accel, gyro, dt):
        """Integrate state and covariance forward"""
        # RK4 integration for state
        self.x = rk4_integrate(
            state_derivative, 
            self.x, 
            (accel, gyro), 
            dt
        )
        
        # Continuous covariance propagation
        F = compute_jacobian(self.x, accel, gyro)
        # P_dot = F*P + P*F^T + Q
        self.P += (F @ self.P + self.P @ F.T + self.Q) * dt
    
    def gps_callback(self, gps_position, gps_covariance, timestamp):
        """Called at low frequency (~10 Hz)"""
        # Correction phase using discrete update
        self._update(gps_position, gps_covariance)
    
    def _update(self, z, R):
        """Standard EKF correction"""
        h = self.x['position']
        H = eye(3, 16)  # Extract position from state
        H = H[0:3, :]
        
        y = z - h
        S = H @ self.P @ H.T + R
        K = self.P @ H.T @ inv(S)
        
        self.x += K @ y
        self.P = (eye(16) - K @ H) @ self.P
```

### Handling GPS Dropouts

When GPS is unavailable, the filter operates in **dead-reckoning mode**:

```
With GPS available:
  Position uncertainty = GPS accuracy + IMU integration error
  
During GPS dropout:
  Position uncertainty = grows at rate determined by velocity uncertainty
  = σ_v * time_without_gps + IMU integration error
  
Example:
  σ_v = 1 m/s, duration = 5 seconds
  Position error growth = 5 m + integration error (~0.5 m)
  Total ≈ 5.5 meters per second of dropout
```

**Detection and handling:**

```python
def handle_gps_dropout(self, dropout_duration):
    """
    GPS is unavailable for dropout_duration seconds.
    During this time, only IMU predicts (standard predict phase).
    """
    # No GPS update occurs
    # Covariance P grows continuously due to Q term
    # When GPS returns, large innovation expected but Kalman gain will be low
    # (low trust in GPS due to high prediction uncertainty)
    
    if dropout_duration > LONG_DROPOUT_THRESHOLD:
        # Optional: inflate process noise to prevent filter locking
        self.Q *= adaptive_factor(dropout_duration)

def gps_returns(self, gps_position, gps_covariance):
    """GPS signal recovers after dropout"""
    # Standard update with possibly large innovation
    innovation = gps_position - self.x['position']
    
    # Kalman gain will be small because:
    # S = H*P*H^T + R
    # P has grown large (no GPS corrections)
    # So K = P*H^T / (large_S) is small
    # This provides smooth re-entry after dropout
    
    self._update(gps_position, gps_covariance)
```

### Interleaved Updates (Advanced)

When multiple sensors have different rates, use **staggered measurement models**:

```
At 100 Hz (IMU):
  predict(accel, gyro)

At 50 Hz (Barometer):
  predict(accel, gyro)
  update_with_altitude(baro_height, baro_covariance)

At 10 Hz (GPS):
  predict(accel, gyro)
  update_with_position(gps_pos, gps_covariance)

At 1 Hz (Magnetometer correction):
  predict(accel, gyro)
  update_with_heading(mag_heading, mag_covariance)
```

**Key principle**: Process every sensor in its own update cycle, but maintain a single consistent state vector and covariance matrix.

---

## Coupling Architectures

### Loosely Coupled (Loose Integration)

**Architecture:**
```
IMU → [INS Processor] → Position/Velocity
GPS → [GNSS Receiver] → Position/Velocity
      ↓                      ↓
      [Kalman Filter combining position/velocity outputs]
```

**Characteristics:**
- GPS output (already filtered by GPS receiver) is fed to Kalman filter
- Filter operates on positions and velocities, not raw measurements
- Easier to implement (no need for raw GPS pseudorange data)
- Lower computational demand

**Advantages:**
- Simple to integrate with standard GPS receivers
- Lower computational cost
- Modular (GPS receiver is a black box)

**Disadvantages:**
- Cannot use partial GPS data (fewer than 4 satellites)
- Complete GPS outage = no correction signal
- Throws away information (GPS receiver applies its own filtering)
- Less optimal information fusion (information already partially compressed)

**When GPS is unavailable:**
```
Less than 4 satellites → GPS receiver outputs nothing
Kalman filter enters pure dead-reckoning mode
Covariance grows unchecked by GPS data
```

### Tightly Coupled (Tight Integration)

**Architecture:**
```
IMU → [INS Processor] → State estimate (position, velocity, attitude)
GPS → [Raw pseudoranges] ↓
      [Single Kalman Filter] → Fused state
```

**Characteristics:**
- Raw GPS pseudorange measurements (not processed position) feed the Kalman filter
- Filter operates on all raw measurements simultaneously
- Requires GPS receiver to output raw data (common but not universal)
- Higher computational load (more measurements to process)

**Advantages:**
- Can use partial GPS data (even with <4 satellites)
- Better information fusion (no pre-filtering losses)
- More robust to GPS errors (Kalman filter sees raw data)
- Better performance in GPS-denied environments

**Disadvantages:**
- More complex to implement
- Higher computational cost
- Requires GPS receiver outputting pseudoranges
- More tuning needed (more parameters to set)

**When GPS is partially available:**
```
2-3 satellites → Can correct some dimensions
Pseudorange measurements can partially constrain position/velocity
Kalman filter benefits even from partial data
Better than loosely coupled (which would output nothing)
```

### Ultra-Tight Coupling

**Architecture:**
```
GPS signals (raw code/phase) → [Correlators]
                            ↓
IMU → [Integrated INS/GNSS filter with code/phase loops]
      → Fused state (attitude, position, velocity)
```

**Characteristics:**
- GPS tracking loops are inside the Kalman filter
- Filter outputs control feedback to GPS receiver
- Highest computational demand
- Most complex to implement

**Advantages:**
- Best possible robustness to GPS signal degradation
- Can track weak signals
- Optimal fusion of all information

**Disadvantages:**
- Very high complexity
- Requires specialized GPS hardware
- Most computationally demanding

### Recommendation for Aerial Platforms

**For most UAV applications: Start with loosely coupled**
- Simpler to implement and debug
- Sufficient for GPS-available conditions
- Lower computational load on embedded systems

**For GPS-challenged environments: Use tightly coupled**
- Necessary for reliable performance in urban/indoor areas
- Better graceful degradation during partial GPS outages
- Worth the extra complexity for demanding applications

**For high-performance systems: Consider ultra-tight coupling**
- Advanced UAVs operating in challenging environments
- If CPU headroom is available

---

## Real-World Challenges

### 1. Magnetic Interference

**Sources of Interference:**
- UAV motor electromagnetic fields
- Electronic components (power supplies, ESCs)
- Nearby metal structures
- Ground-based ferrous objects (buildings, power lines)

**Effects:**
- Heading error (magnetometer measures wrong field direction)
- Rapid heading drift when using magnetometer for correction
- In BNO085: Gaming Rotation Vector mode eliminates magnetometer → heading drifts ~0.5°/min

**Mitigation Strategies:**

```
a) Physical isolation:
   - Mount IMU away from motors and power electronics
   - Use booms or tethers to separate sensors from main body
   - Keep minimum ~0.5 m separation from ferrous materials

b) Calibration:
   - Perform heading calibration flights at operating altitude
   - Fly figure-8 patterns in all three axes
   - Typical procedure: 4 headings (0°, 90°, 180°, 270°) at high altitude (>800 m)
   - BNO085 has onboard calibration; verify status register

c) Software compensation:
   - Use accelerometer/GPS for heading correction
   - Reduce reliance on magnetometer in EKF
   - When GPS has good velocity data: heading = atan2(v_y, v_x)
   - This requires good GPS quality (>1 m/s velocity resolution)

d) Mode selection:
   - Use "Game Rotation Vector" (no magnetometer) for initial flight
   - Switch to full "Rotation Vector" (with magnetometer) only after calibration
   - Accept ~0.5°/min heading drift during flight (corrected by GPS)
```

**Kalman Filter Approach:**

```python
# In update phase, add optional heading constraint from velocity
if gps_velocity_valid and gps_speed > 1.0:  # m/s
    heading_from_gps = atan2(gps_v_y, gps_v_x)
    heading_innovation = wrap_angle(heading_from_gps - heading_est)
    
    # Create heading measurement with covariance
    # (uncertainty proportional to velocity uncertainty)
    velocity_sigma = 0.5  # m/s
    heading_sigma = atan(velocity_sigma / gps_speed)
    
    # Add to measurement update with low weight if magnetic
    # interference is suspected
    ...
```

### 2. GPS Dropouts

**Common Scenarios:**
- Traversing trees or dense foliage: 100-500 ms dropouts
- Flying between buildings: 1-10 second dropouts
- Tunnels or caves: indefinite loss of signal
- Intentional GPS denial (testing, e-warfare): indefinite

**Handling Strategy:**

```python
class RobustEKF:
    def __init__(self, ...):
        ...
        self.gps_dropout_detected = False
        self.last_gps_time = 0
        self.max_gps_gap = 5.0  # seconds
    
    def gps_callback(self, gps_data, timestamp):
        """Called when GPS is available"""
        gps_gap = timestamp - self.last_gps_time
        
        if gps_gap > self.max_gps_gap:
            # Long dropout detected
            self.on_gps_dropout_recovery(gps_gap)
        
        self.last_gps_time = timestamp
        self.gps_dropout_detected = False
        self._update(gps_data.position, gps_data.covariance)
    
    def on_gps_dropout_recovery(self, dropout_duration):
        """Called when GPS reappears after long gap"""
        # Adaptive Q scaling for smooth re-entry
        Q_inflation_factor = min(1.0 + dropout_duration / 10.0, 2.0)
        self.Q *= Q_inflation_factor
        
        # Log the event for telemetry
        self.log_event(f"GPS recovered after {dropout_duration:.1f}s dropout")
    
    def on_gps_timeout(self, timeout_threshold=0.5):
        """Called periodically to detect GPS loss"""
        current_time = system_time()
        gps_gap = current_time - self.last_gps_time
        
        if gps_gap > timeout_threshold and not self.gps_dropout_detected:
            self.gps_dropout_detected = True
            self.on_dropout_detected()
    
    def on_dropout_detected(self):
        """Called when dropout is detected"""
        # Increase process noise to allow position to drift further
        # (less confident in prediction without GPS)
        self.Q_dropout_mode = self.Q * 5.0
        
        # Start dead-reckoning
        self.log_event("GPS dropout detected, entering dead-reckoning mode")
```

**Position Quality During Dropouts:**

| Time without GPS | Position Uncertainty | Remarks |
|------------------|----------------------|---------|
| 0-1 second       | 1-2 meters           | Very good, IMU integration solid |
| 1-5 seconds      | 5-20 meters          | Acceptable, velocity uncertainty compounds |
| 5-30 seconds     | 30-100+ meters       | Poor, requires constant height assumption |
| 30+ seconds      | Unbounded            | Only viable if flying in straight line |

**Practical Limits:**
- For controlled hover: useful dead-reckoning ~5-10 seconds (depending on velocity)
- For forward flight (constant velocity): can extend to 30+ seconds with good initial conditions
- For maneuvering flight: 5-10 seconds maximum reliable dead-reckoning

### 3. Initialization Challenges

**Starting from Power-On:**

```
IMU at power-on: Does NOT know its attitude
GPS at power-on: Needs time to acquire (cold start 30+ seconds)
Magnetometer: Needs calibration before reliable

Goal: Determine initial attitude q_0 and position p_0

Standard procedure:

Step 1: Gravity Alignment (first 10-30 seconds)
  - Keep vehicle stationary
  - Accelerometer measures gravity vector
  - Assume no rotation: q = [0, 0, 0, 1] (identity)
  - Use accelerometer to infer roll/pitch
  - Initialize Kalman filter with attitude uncertainty

Step 2: Magnetic Heading Alignment
  - Magnetometer provides heading reference
  - Requires motion or calibration (see magnetic interference section)
  - Alternative: Wait for GPS and use velocity direction

Step 3: GPS Initialization
  - Wait for GPS fix (typically 30-60 seconds cold start)
  - Initialize position p_0 = GPS position
  - Set initial velocity uncertainty high

Alternative: In-Motion Alignment
  - Start flying with initial attitude guess
  - Allow filter to converge during first 30-60 seconds of flight
  - Requires aircraft to be well-behaved during convergence period
```

**Initialization Pseudocode:**

```python
def initialize_filter(stationary_accel_measurements, gps_fix):
    """
    Initialize Kalman filter from stationary accelerometer and GPS.
    
    Args:
        stationary_accel_measurements: List of accel vectors during stationary phase
        gps_fix: First valid GPS position
    """
    
    # Step 1: Estimate attitude from gravity vector
    # Average accelerations to reduce noise
    mean_accel = mean(stationary_accel_measurements)
    # In body frame, gravity is -9.81 m/s² in Z direction
    
    # Roll/Pitch from accelerometer
    roll = atan2(mean_accel[1], mean_accel[2])
    pitch = atan2(-mean_accel[0], 
                  sqrt(mean_accel[1]**2 + mean_accel[2]**2))
    
    # Yaw is undetermined from accel alone
    yaw = 0  # Or wait for GPS velocity vector
    
    # Convert to quaternion
    q_init = euler_to_quaternion(roll, pitch, yaw)
    
    # Step 2: Position from GPS
    p_init = gps_fix.position
    
    # Step 3: Velocity from GPS
    v_init = gps_fix.velocity  # Or [0, 0, 0] if not available
    
    # Step 4: Initialize filter state
    x_init = {
        'q': q_init,
        'v': v_init,
        'p': p_init,
        'a_bias': [0, 0, 0],  # Unbiased initially
        'g_bias': [0, 0, 0],
    }
    
    # Step 5: Initialize covariance
    # High uncertainty in roll/pitch (±5 degrees typical)
    # Very high uncertainty in yaw (±180 degrees if not from velocity)
    # High uncertainty in position (GPS accuracy)
    
    P_init = diag([
        0.003, 0.003, 0.3,      # Attitude uncertainty (rad²)
        0.25, 0.25, 1.0,        # Velocity uncertainty (m/s²)
        100, 100, 100,          # Position uncertainty (m²)
        0.001, 0.001, 0.001,    # Accel bias
        0.001, 0.001, 0.001,    # Gyro bias
    ])
    
    return EKF(x_init, P_init, Q, R)
```

### 4. Temperature Effects

**Temperature-Dependent Errors:**
- Gyroscope bias: ~0.01°/s/°C (varies by component)
- Accelerometer bias: ~0.1 mg/°C
- Oscillator frequency (affects GPS timing): ~0.01 ppm/°C

**Mitigation:**
1. **Thermally isolate** IMU if possible (add insulation)
2. **Temperature compensation** in firmware
3. **In-flight calibration** (periodic re-zeroing of bias estimates)

**Kalman Filter Perspective:**
- Process noise Q already accounts for slow bias drift
- For flights <30 minutes in stable temperature: not critical
- For extended missions: implement temperature-based bias scaling

---

## Initialization and Alignment

### Static Alignment Procedure

**Best for ground-based initialization before flight:**

```
Step 1: Position vehicle on level ground
Step 2: Wait 30-60 seconds for gyroscope bias to stabilize
Step 3: Collect 100-200 accelerometer samples
        - Average to get gravity vector with noise reduction
Step 4: Use gravity vector to determine roll and pitch
Step 5: Magnetometer provides heading (after calibration)
Step 6: GPS provides absolute position
Step 7: Initialize Kalman filter covariance with known uncertainties
```

**Advantages:**
- Simple, deterministic
- Low computational load
- Well-understood procedure (aviation standard)

**Disadvantages:**
- Requires stationary phase before flight
- Yaw from magnetometer (subject to interference)

### In-Motion Alignment

**For launching from moving vehicles or without prior calibration:**

```
Step 1: Start with default attitude (level platform assumption)
Step 2: Fly straight line for 30-60 seconds
Step 3: Filter converges on attitude using:
        - Accelerometer: provides tilt correction
        - Magnetometer: provides heading (if usable)
        - GPS velocity: if speed >2 m/s, heading = atan2(v_y, v_x)
Step 4: Covariance shrinks during convergence
Step 5: Ready for maneuvering after convergence
```

**Practical Implementation:**

```python
class InMotionAlignment:
    def __init__(self, flight_controller):
        self.fc = flight_controller
        self.alignment_phase = True
        self.time_in_alignment = 0
        self.min_alignment_time = 30.0  # seconds
    
    def update(self, ekf_state, elapsed_time):
        self.time_in_alignment = elapsed_time
        
        if not self.alignment_phase:
            return
        
        # Check convergence criteria
        attitude_covariance = ekf_state.P[0:3, 0:3]  # Quaternion error variance
        position_covariance = ekf_state.P[9:12, 9:12]  # Position variance
        
        # Convergence achieved when:
        # 1. Enough time elapsed
        # 2. Attitude uncertainty is low
        # 3. Position uncertainty is below threshold
        
        attitude_std = sqrt(trace(attitude_covariance))
        position_std = sqrt(trace(position_covariance))
        
        if (self.time_in_alignment > self.min_alignment_time and
            attitude_std < 0.05 and  # ~2.9 degrees
            position_std < 5.0):     # meters
            
            self.alignment_complete()
    
    def alignment_complete(self):
        self.alignment_phase = False
        print(f"In-motion alignment complete after {self.time_in_alignment:.1f}s")
        self.fc.on_alignment_complete()
```

---

## Practical Implementation Guide

### Minimal Working Example (Python)

```python
import numpy as np
from scipy.linalg import expm
from scipy.spatial.transform import Rotation as R

class MinimalEKF:
    """Minimal IMU+GPS Kalman filter implementation"""
    
    def __init__(self, initial_pos, initial_attitude):
        # State vector: [q, v, p, a_bias, g_bias]
        self.q = initial_attitude  # Quaternion [w, x, y, z]
        self.v = np.zeros(3)       # Velocity [vx, vy, vz]
        self.p = initial_pos       # Position [px, py, pz]
        self.a_bias = np.zeros(3)  # Accelerometer bias
        self.g_bias = np.zeros(3)  # Gyroscope bias
        
        # State covariance
        self.P = np.eye(16) * 0.1
        
        # Process noise (tune these)
        self.sigma_accel = 0.01    # m/s²
        self.sigma_gyro = 0.01     # rad/s
        self.sigma_accel_bias_drift = 0.0001
        self.sigma_gyro_bias_drift = 0.0001
        
        # Measurement noise (tune these)
        self.sigma_gps_pos = 5.0   # meters
        self.sigma_gps_vel = 1.0   # m/s
    
    def state_to_vector(self):
        """Pack state into 16D vector"""
        return np.hstack([self.q, self.v, self.p, 
                         self.a_bias, self.g_bias])
    
    def vector_to_state(self, x):
        """Unpack 16D vector into state"""
        self.q = x[0:4] / np.linalg.norm(x[0:4])
        self.v = x[4:7]
        self.p = x[7:10]
        self.a_bias = x[10:13]
        self.g_bias = x[13:16]
    
    def quaternion_multiply(self, q1, q2):
        """Quaternion multiplication q1 ⊗ q2"""
        w1, x1, y1, z1 = q1
        w2, x2, y2, z2 = q2
        
        return np.array([
            w1*w2 - x1*x2 - y1*y2 - z1*z2,
            w1*x2 + x1*w2 + y1*z2 - z1*y2,
            w1*y2 - x1*z2 + y1*w2 + z1*x2,
            w1*z2 + x1*y2 - y1*x2 + z1*w2,
        ])
    
    def quaternion_to_rotation_matrix(self, q):
        """Convert quaternion to 3x3 rotation matrix"""
        w, x, y, z = q
        return np.array([
            [1-2*(y*y+z*z), 2*(x*y-w*z), 2*(x*z+w*y)],
            [2*(x*y+w*z), 1-2*(x*x+z*z), 2*(y*z-w*x)],
            [2*(x*z-w*y), 2*(y*z+w*x), 1-2*(x*x+y*y)],
        ])
    
    def predict(self, accel, gyro, dt):
        """IMU prediction step"""
        # Remove biases
        a_unbiased = accel - self.a_bias
        w_unbiased = gyro - self.g_bias
        
        # Quaternion update: dq/dt = 0.5 * q ⊗ [0, w]
        w_quat = np.array([0, w_unbiased[0], w_unbiased[1], w_unbiased[2]])
        dq = 0.5 * self.quaternion_multiply(self.q, w_quat)
        self.q = self.q + dq * dt
        self.q = self.q / np.linalg.norm(self.q)
        
        # Velocity update: dv/dt = R * a - g
        R = self.quaternion_to_rotation_matrix(self.q)
        gravity = np.array([0, 0, 9.81])
        self.v = self.v + (R @ a_unbiased - gravity) * dt
        
        # Position update: dp/dt = v
        self.p = self.p + self.v * dt
        
        # Bias update (random walk): db/dt = noise
        # (biases change slowly; no update needed in basic implementation)
        
        # Covariance update (simplified)
        # P = F*P*F^T + Q (using first-order linearization)
        F = self._compute_jacobian(a_unbiased)
        Q = self._compute_process_noise()
        self.P = F @ self.P @ F.T + Q
    
    def _compute_jacobian(self, a):
        """Compute state transition Jacobian (simplified)"""
        F = np.eye(16)
        # Fill in nonzero Jacobian elements
        # (Full implementation would compute all 256 elements)
        R = self.quaternion_to_rotation_matrix(self.q)
        F[4:7, 0:4] = 0.5 * R  # Velocity depends on attitude
        F[7:10, 4:7] = np.eye(3)  # Position depends on velocity
        return F
    
    def _compute_process_noise(self):
        """Build process noise covariance matrix"""
        Q = np.diag([
            self.sigma_gyro**2, self.sigma_gyro**2, self.sigma_gyro**2, 0,
            self.sigma_accel**2, self.sigma_accel**2, self.sigma_accel**2,
            0, 0, 0,
            self.sigma_accel_bias_drift**2, self.sigma_accel_bias_drift**2, self.sigma_accel_bias_drift**2,
            self.sigma_gyro_bias_drift**2, self.sigma_gyro_bias_drift**2, self.sigma_gyro_bias_drift**2,
        ])
        return Q
    
    def update_position(self, gps_pos):
        """GPS position measurement update"""
        # Innovation
        z = gps_pos
        h = self.p  # Predicted position
        y = z - h
        
        # Measurement Jacobian
        H = np.zeros((3, 16))
        H[0:3, 7:10] = np.eye(3)
        
        # Measurement covariance
        R = np.eye(3) * self.sigma_gps_pos**2
        
        # Standard EKF update
        S = H @ self.P @ H.T + R
        K = self.P @ H.T @ np.linalg.inv(S)
        
        x_corr = K @ y
        self.vector_to_state(self.state_to_vector() + x_corr)
        
        self.P = (np.eye(16) - K @ H) @ self.P
    
    def get_state(self):
        """Return current estimates"""
        return {
            'attitude': self.q,
            'velocity': self.v,
            'position': self.p,
            'covariance': self.P
        }

# Usage example:
ekf = MinimalEKF(
    initial_pos=np.array([0, 0, 0]),
    initial_attitude=np.array([1, 0, 0, 0])  # Identity quaternion
)

# IMU loop (100 Hz)
for imu_data in imu_stream:
    accel, gyro = imu_data
    ekf.predict(accel, gyro, dt=0.01)

# GPS loop (10 Hz)
for gps_data in gps_stream:
    ekf.update_position(gps_data.position)

# Get current estimate
state = ekf.get_state()
print(f"Position: {state['position']}")
print(f"Velocity: {state['velocity']}")
print(f"Attitude (quaternion): {state['attitude']}")
```

### Integration with BNO085

```python
class BNO085_EKF_Integration:
    """Integrate BNO085 with EKF"""
    
    def __init__(self):
        self.bno085 = BNO085Device()  # Adafruit library
        self.ekf = MinimalEKF(
            initial_pos=np.array([0, 0, 0]),
            initial_attitude=np.array([1, 0, 0, 0])
        )
        self.gps = GPSDevice()  # Some GPS library
        self.last_gps_time = 0
    
    def run(self):
        """Main loop"""
        while True:
            # Check BNO085 for data
            if self.bno085.has_imu_data():
                accel = self.bno085.get_acceleration()
                gyro = self.bno085.get_gyroscope()
                
                # Optionally use BNO085's internal orientation
                # bno_quaternion = self.bno085.get_quaternion()
                # But for fusion, we use raw IMU data
                
                self.ekf.predict(accel, gyro, dt=0.01)
            
            # Check GPS for data
            current_time = time.time()
            if self.gps.has_new_fix():
                gps_position = self.gps.get_position()
                self.ekf.update_position(gps_position)
                self.last_gps_time = current_time
            
            # Check for GPS dropout
            if current_time - self.last_gps_time > 5.0:
                print("GPS dropout detected!")
                # Optionally inflate process noise or alert user
            
            # Output current estimate
            state = self.ekf.get_state()
            self.publish_state(state)
```

---

## References and Further Reading

### Academic Papers and Technical Resources

- [Full article: A multi-sensor fusion-based UAV autonomous localization system](https://www.tandfonline.com/doi/full/10.1080/17445760.2025.2602166) - Recent comprehensive UAV fusion system with LiDAR, IMU, stereo vision, and GNSS

- [Performance of GPS and IMU sensor fusion using unscented Kalman filter for precise i-Boat navigation](https://www.sciencedirect.com/science/article/pii/S1674984722000969) - Comparison of Kalman filter approaches

- [Sensor Fusion based UAVs for GPS denied Environments](https://xray.greyb.com/drones/sensor-fusion-navigation) - Navigation without GPS

- [Multi Sensor Fusion For Enhanced 3D Pose Estimation Of Unmanned Aerial Vehicle](https://ieeexplore.ieee.org/iel8/11039313/11039325/11039401.pdf) - IEEE conference paper on multi-sensor fusion

- [Sensor Fusion Design by Extended and Unscented Kalman Filter Approaches for Position and Attitude Estimation](https://ieeexplore.ieee.org/document/9799879/) - EKF vs UKF comparison

- [Design of an adapted extended Kalman filter for enhancing UAV navigation with optical flow](https://journals.sagepub.com/doi/10.1177/09596518241300660) - Recent 2025 work on EKF enhancement

- [GitHub: Extended-Kalman-Filter-GPS_IMU](https://github.com/Janudis/Extended-Kalman-Filter-GPS_IMU) - Implementation reference

- [An Indoor UAV Localization Framework with ESKF Tightly-Coupled Fusion and Multi-Epoch UWB Outlier Rejection](https://pmc.ncbi.nlm.nih.gov/articles/PMC12737027/) - Tight coupling with non-GNSS sensors

### IMU and BNO085 Resources

- [Adafruit BNO055 & BNO085 IMU Sensor Guide: Setup, Calibration & Applications](https://pcbsync.com/adafruit-bno055-bno085-imu-sensor/) - Practical BNO085 guide

- [BNO08X Datasheet Revision 1.17](https://www.ceva-ip.com/wp-content/uploads/BNO080_085-Datasheet.pdf) - Official specification

- [Adafruit 9-DOF Orientation IMU Fusion Breakout - BNO085](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-9-dof-orientation-imu-fusion-breakout-bno085.pdf) - Adafruit tutorial

### GPS and GNSS Error Analysis

- [Understanding and mitigating GNSS multipath interference and error](https://novatel.com/tech-talk/an-introduction-to-gnss/resources/understanding-and-mitigating-gnss-multipath-interference-and-error) - Multipath fundamentals

- [GPS Overview Part 2: What Creates GNSS Positioning Error?](https://eos-gnss.com/knowledge-base/gps-overview-2-what-creates-gnss-positioning-error/) - Comprehensive error sources

- [Learn more about pseudorange errors and dilution of precision](https://www.vectornav.com/resources/inertial-navigation-primer/specifications--and--error-budgets/specs-gnsserrorbudget) - DOP and error budgets

- [Chapter 4: GNSS error sources](https://novatel.com/an-introduction-to-gnss/gnss-error-sources) - Technical details

- [5.3 GPS Error Sources](https://www.e-education.psu.edu/geog160/node/1924) - Educational resource

- [Surveying with RTK – What Does Multipath Mean?](https://rtkgpssurveyequipment.com/surveying-with-rtk-what-does-multipath-mean/) - RTK perspective

- [Robust Positioning in the Presence of Multipath and NLOS GNSS Signals](https://ntrs.nasa.gov/api/citations/20190034171/downloads/20190034171.pdf) - NASA research on robust positioning

- [Error analysis for the Global Positioning System](https://en.wikipedia.org/wiki/Error_analysis_for_the_Global_Positioning_System) - Wikipedia technical reference

### Asynchronous Sensor Fusion

- [Asynchronous Sensor Fusion of GPS, IMU and CAN-Based Odometry for Heavy-Duty Vehicles](https://core.ac.uk/download/pdf/517409175.pdf) - PDF on asynchronous fusion

- [IMU and GPS Fusion for Inertial Navigation - MATLAB & Simulink](https://www.mathworks.com/help/nav/ug/imu-and-gps-fusion-for-inertial-navigation.html) - MATLAB tutorial

- [Inertial Sensor Fusion - MATLAB & Simulink](https://www.mathworks.com/help/fusion/inertial-sensor-fusion.html) - Sensor fusion toolbox

- [Pose Estimation from Asynchronous Sensors - MATLAB & Simulink](https://www.mathworks.com/help/fusion/ug/pose-estimation-from-asynchronous-sensors.html) - Handling asynchronous updates

- [GPS-IMU Sensor Fusion for Reliable Autonomous Vehicle Position Estimation](https://arxiv.org/html/2405.08119v1) - Recent arXiv paper

- [Asynchronous Multi-Sensor Fusion for 3D Mapping and Localization](https://udel.edu/~ghuang/papers/tr_async.pdf) - University of Delaware research

### Attitude Estimation and Quaternions

- [I Wrote an Extended Kalman Filter for UAV Attitude Estimation — From Scratch in Rust](https://medium.com/@opinoquintana/i-wrote-an-extended-kalman-filter-for-uav-attitude-estimation-from-scratch-in-rust-b8748ff33b12) - Medium article with implementation details

- [Kalman Filtering for Attitude Estimation with Quaternions and Concepts from Manifold Theory](https://pmc.ncbi.nlm.nih.gov/articles/PMC6339217/) - Theoretical foundations

- [A Kalman Filter for Nonlinear Attitude Estimation Using Time Variable Matrices and Quaternions](https://pmc.ncbi.nlm.nih.gov/articles/PMC7728053/) - Advanced quaternion filtering

- [An Extended Kalman Filter for Quaternion-Based Attitude Estimation](https://apps.dtic.mil/sti/tr/pdf/ADA384973.pdf) - DTIC technical report

- [Extended Kalman Filter — AHRS 0.4.0 documentation](https://ahrs.readthedocs.io/en/latest/filters/ekf.html) - AHRS library documentation

- [Attitude estimation for UAV using extended Kalman filter](https://ieeexplore.ieee.org/document/7979077/) - IEEE conference paper

- [Robust attitude estimation using an adaptive unscented Kalman filter](https://guilhermepereira.faculty.wvu.edu/files/d/e6d8ec11-e8a5-4442-8f17-1b5333f059ec/icra2019.pdf) - WVU research on robust attitude

### Kalman Filter Fundamentals

- [Kalman filter](https://en.wikipedia.org/wiki/Kalman_filter) - Wikipedia comprehensive overview

- [Getting up to speed with Kalman filters](https://www.vectornav.com/resources/inertial-navigation-primer/math-fundamentals/math-kalman) - VectorNav tutorial (excellent)

- [On the evaluation of uncertainties for state estimation with the Kalman filter](https://arxiv.org/abs/1605.01235) - Uncertainty propagation theory

- [Process uncertainty in Kalman filtering: A novel adaptive filter for vehicle dynamic state estimation](https://www.sciencedirect.com/science/article/abs/pii/S0888327026003882) - Adaptive covariance

- [The Kalman Filter](https://engineeringmedia.com/controlblog/the-kalman-filter) - Engineering Media tutorial

- [Kalman Filtering with Uncertain and Asynchronous Measurement Epochs](https://navi.ion.org/content/71/3/navi.652) - ION journal on asynchronous measurements

- [Uncertainty 3: State Space Kalman Filters](https://www.cs.cmu.edu/~motionplanning/papers/sbp_papers/kalman/unc/unc_ekf_tut.pdf) - CMU tutorial

- [Kalman Filter Explained Through Examples](https://kalmanfilter.net/) - Interactive tutorial

### GPS Dropout and Robustness

- [Handling method for GPS outages based on PSO-LSTM and fading adaptive Kalman filtering](https://www.nature.com/articles/s41598-025-95716-1) - Recent 2025 research on GPS outages

- [Practical IMU-GPS Fusion with Kalman Filters](https://beefed.ai/en/imu-gps-kalman-fusion-practical-guide) - Practical implementation guide

- [Optimal Kalman filtering with random sensor delays, packet dropouts and missing measurements](https://ieeexplore.ieee.org/document/5160216/) - IEEE paper on handling missing data

- [Sensor failure detection with a bank of Kalman filters](https://ieeexplore.ieee.org/document/520920/) - Fault detection approach

- [Innovation-based Kalman filter fault detection and exclusion method](https://link.springer.com/article/10.1007/s10291-024-01623-9) - GNSS/INS/Vision fault detection

### Magnetic Interference and Compensation

- [Magnetometer and Magnetic Field Sensors](https://www.unmannedsystemstechnology.com/expo/magnetometer/) - Overview

- [Magnetic Surveys With Unmanned Aerial Systems](https://agupubs.onlinelibrary.wiley.com/doi/full/10.1029/2021GC009745) - Wiley publication on UAV magnetometry

- [Unmanned Aerial Vehicles for Magnetic Surveys: A Review on Platform Selection and Interference Suppression](https://www.mdpi.com/2504-446X/5/3/93) - Comprehensive MDPI review

- [A Drone-mounted magnetometer system for automatic interference removal](https://arxiv.org/html/2510.01417v1) - Recent arXiv paper on interference removal

### Coupling Architectures

- [Loose and Tight GNSS/INS Integrations: Comparison of Performance Assessed in Real Urban Scenarios](https://pmc.ncbi.nlm.nih.gov/articles/PMC5335985/) - Direct comparison

- [The Design of GNSS/IMU Loosely-Coupled Integration Filter for Wearable EPTS of Football Players](https://pmc.ncbi.nlm.nih.gov/articles/PMC9965289/) - Practical loosely-coupled example

- [Loosely Coupled & Tightly Coupled INS & GNSS [2024 Guide]](https://pointonenav.com/news/loose-vs-tight-coupling-gnss/) - Point One Navigation guide

- [Real-time tightly coupled GNSS and IMU integration via Factor Graph Optimization](https://arxiv.org/html/2603.03556v1) - Modern factor graph approach

- [Ground Vehicle Pose Estimation for Tightly Coupled IMU and GNSS - MATLAB & Simulink](https://www.mathworks.com/help/nav/ug/ground-vehicle-pose-estimation-for-tightly-coupled-imu-gnss.html) - MATLAB example

### INS Initialization

- [Inertial navigation system](https://en.wikipedia.org/wiki/Inertial_navigation_system) - Wikipedia overview

- [Chapter 9.4.2: INERTIAL SYSTEMS TECHNOLOGIES: Initialization and Alignment](https://www.globalspec.com/reference/14800/160210/chapter-9-4-2-inertial-systems-technologies-initialization-and-alignment) - GlobalSpec reference

- [Automatic alignment and calibration of an inertial navigation system](https://www.researchgate.net/publication/3571539_Automatic_alignment_and_calibration_of_an_inertial_navigation_system) - ResearchGate paper

- [Study on Initial Alignment for Inertial Navigation System](https://ieeexplore.ieee.org/document/5364370/) - IEEE study

- [Chapter 10: Inertial Navigation System Alignment](https://www.globalspec.com/reference/26566/203279/chapter-10-inertial-navigation-system-alignment) - GlobalSpec detailed chapter

---

## Summary

This document provides a comprehensive foundation for understanding and implementing IMU+GPS sensor fusion for aerial platforms:

1. **Why fusion matters**: IMU and GPS provide complementary information that is optimally combined through Kalman filtering.

2. **EKF framework**: Extended Kalman Filter is the practical choice for real-time aerial systems, balancing accuracy with computational efficiency.

3. **State vector design**: 16-19 dimensional state including attitude (quaternion), velocity, position, and bias terms.

4. **Asynchronous sensors**: Continuous-discrete EKF handles high-frequency IMU and low-frequency GPS naturally.

5. **Real-world challenges**: Magnetic interference, GPS dropouts, and initialization require careful handling in practice.

6. **Implementation path**: Start with loosely-coupled architecture, then migrate to tightly-coupled if needed for GPS-challenged environments.

For the floppi project specifically:
- BNO085 provides excellent attitude estimates with internal fusion
- GPS provides absolute position and heading correction
- Combining them in an EKF gives robust, continuous navigation
- Process noise and measurement covariance tuning critical for good performance
- Regular recalibration of magnetometer helps with heading accuracy

---

**Document created**: May 2026  
**Target audience**: Developers implementing aerial navigation systems  
**Assumed knowledge**: Control theory basics, linear algebra, some C/Python experience
