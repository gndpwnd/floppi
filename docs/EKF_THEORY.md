# Extended Kalman Filter (EKF) Theory Reference

## Table of Contents

1. [Fundamentals](#fundamentals)
2. [Linear Kalman Filter](#linear-kalman-filter)
3. [Extended Kalman Filter](#extended-kalman-filter)
4. [State Space Model](#state-space-model)
5. [Predict and Update Equations](#predict-and-update-equations)
6. [Covariance Propagation](#covariance-propagation)
7. [Numerical Stability](#numerical-stability)
8. [EKF vs Complementary Filter](#ekf-vs-complementary-filter)
9. [Practical Tuning Guidelines](#practical-tuning-guidelines)
10. [Implementation Notes](#implementation-notes)

---

## Fundamentals

### What is a Kalman Filter?

The Kalman Filter is a recursive algorithm for estimating the state of a dynamic system from noisy measurements. It is optimal for linear systems with Gaussian noise, meaning it minimizes the mean squared error.

**Key Properties**:
- **Recursive**: Process one measurement at a time
- **Optimal**: Minimum variance estimate for linear systems
- **Probabilistic**: Outputs both estimate and uncertainty
- **Real-time**: Suitable for embedded systems
- **Robust**: Handles noisy sensors gracefully

### Basic Concept

Imagine a moving vehicle with:
- IMU (accelerometer, gyroscope) → tells you how it's accelerating
- GPS → tells you where it is

Problem: Both are noisy!
- IMU: drift accumulates over time
- GPS: jumpy, noisy position updates

Solution: **Fuse both sensors** using the Kalman Filter
- Trust IMU for short-term motion estimation
- Trust GPS for long-term position correction
- Blend them based on measured noise levels

---

## Linear Kalman Filter

Before we can understand the Extended Kalman Filter, we need the simpler linear case.

### Linear System Model

```
State transition (predict):    x_{k+1} = F x_k + w_k
Measurement model (update):     z_k = H x_k + v_k
```

Where:
- **x_k**: State vector (position, velocity, etc.)
- **F**: State transition matrix (how state evolves)
- **z_k**: Measurement vector (what we observe)
- **H**: Measurement matrix (maps state to measurement)
- **w_k**: Process noise (uncertainty in dynamics)
- **v_k**: Measurement noise (sensor uncertainty)

### Assumptions

1. **Gaussian Noise**
   - Process noise: w_k ~ N(0, Q)
   - Measurement noise: v_k ~ N(0, R)

2. **Linearity**
   - State transition is linear: x_{k+1} = F x_k + w_k
   - Measurement is linear: z_k = H x_k + v_k

3. **Independence**
   - Process and measurement noise are uncorrelated
   - Noise is white (uncorrelated across time)

---

## Extended Kalman Filter

### Why Extended?

The Extended Kalman Filter handles **nonlinear** systems by linearizing around the current estimate.

Real systems are often nonlinear:
- **Attitude dynamics**: Quaternion kinematics are nonlinear
- **Rotation effects**: Accelerometer output depends on vehicle attitude
- **Gravity**: Gravity vector must be rotated using quaternion

### Nonlinear System Model

```
State transition (predict):    x_{k+1} = f(x_k, u_k) + w_k
Measurement model (update):     z_k = h(x_k) + v_k
```

Where:
- **f(·)**: Nonlinear state transition function
- **h(·)**: Nonlinear measurement function
- Everything else the same as linear case

### EKF Linearization

The EKF linearizes around the current estimate using Jacobian matrices:

**Process Jacobian**:
```
F[i,j] = ∂f_i / ∂x_j  (computed at current state estimate)
```

**Measurement Jacobian**:
```
H[i,j] = ∂h_i / ∂x_j  (computed at current state estimate)
```

This allows us to use linear Kalman equations with nonlinear systems!

### EKF Optimality

Important: **The EKF is NOT optimal for nonlinear systems**
- Linear KF is optimal for linear systems
- EKF is suboptimal for nonlinear systems (especially high nonlinearity)
- Better alternatives exist: Unscented KF, Particle Filter
- But EKF is a good practical choice: simple and works well for many systems

---

## State Space Model

### State Vector (16 elements)

Our GPS-IMU fusion system uses a 16-dimensional state:

```
x = [q_w, q_x, q_y, q_z,      ← Quaternion (attitude)
     v_n, v_e, v_d,            ← Velocity [North, East, Down] (m/s)
     p_n, p_e, p_d,            ← Position [North, East, Down] (m)
     b_ax, b_ay, b_az]         ← Accelerometer bias (m/s²)
```

**Coordinate Frames**:
- **NED** (North-East-Down): Local navigation frame
  - North: tangent to Earth (positive northward)
  - East: perpendicular to North (positive eastward)
  - Down: downward along gravity (positive downward)

- **Body**: Vehicle-fixed frame
  - X: forward
  - Y: right wing
  - Z: down

### Covariance Matrix

The uncertainty in each state component is represented by the covariance matrix P:

```
P = E[(x - x_hat)(x - x_hat)^T]
```

**Properties**:
- **Diagonal** P[i,i] = variance of state[i]
- **Symmetric**: P[i,j] = P[j,i]
- **Positive-definite**: All eigenvalues > 0
- **16×16** = 256 elements

**Interpretation**:
- sqrt(P[i,i]) = standard deviation (uncertainty) in state[i]
- Larger P[i,i] = more uncertain about that state component

### Process Noise (Q matrix)

Represents uncertainty in our motion model:

```
Q = E[w w^T]  (16×16 matrix)
```

**Typical structure**:
- Diagonal: Different noise for each state
- Zero off-diagonal: Noise in different states uncorrelated
- Larger Q = "predict step less trusted"

### Measurement Noise (R matrix)

Represents sensor measurement uncertainty:

```
R = E[v v^T]  (3×3 matrix for GPS position)
```

**For GPS**:
- R[0,0] = (horizontal std dev)²
- R[1,1] = (horizontal std dev)²
- R[2,2] = (vertical std dev)²
- Larger R = "measurement less trusted"

---

## Predict and Update Equations

### Predict Step (IMU)

Runs at IMU rate (typically 100 Hz)

**1. Propagate State**

```
x_pred = f(x_hat, u_k, dt)
```

Expanding for our system:

```
Quaternion:
  q_new = q_old + 0.5 * dt * (q_old ⊗ ω_measured)
  where ω_measured = gyro - gyro_bias

Velocity:
  v_new = v_old + dt * (R(q) * (a_measured - a_bias) + [0, 0, g])
  where R(q) rotates body-frame acceleration to NED
  g = 9.81 m/s²

Position:
  p_new = p_old + dt * v

Bias (constant model):
  bias_new = bias_old
```

**2. Compute Jacobian F**

F is the linearization of f() around current state:

```
F[i,j] = ∂f_i / ∂x_j
```

Block structure:
```
F = [∂q/∂q   0     0     0   ]
    [∂v/∂q  I     0    ∂v/∂b]
    [0      I     I     0   ]
    [0      0     0     I   ]
```

**3. Predict Covariance**

```
P_pred = F * P_hat * F^T + Q
```

This propagates uncertainty:
- F matrix: How uncertainty in each state affects future uncertainty
- Q matrix: Uncertainty added by imperfect motion model
- Result: Covariance grows without measurements (expected!)

### Update Step (GPS)

Runs at GPS rate (typically 1-10 Hz)

**1. Compute Innovation (Measurement Residual)**

```
y = z_measured - h(x_pred)
```

Example for GPS:
```
z_measured = [GPS_North, GPS_East, GPS_Down]^T
h(x_pred) = [p_n, p_e, p_d]^T
y = z_measured - h(x_pred)
```

The innovation is the "surprise" - how different the measurement is from what we predicted.

**2. Compute Measurement Jacobian H**

```
H[i,j] = ∂h_i / ∂x_j
```

For GPS (simple case):
```
H = [1  0  0  0   0  0  0   0  0  0   0  0  0]
    [0  0  0  0   0  0  0   1  0  0   0  0  0]
    [0  0  0  0   0  0  0   0  1  0   0  0  0]
```

(Only position states affect GPS measurements)

**3. Compute Kalman Gain**

```
K = P_pred * H^T / (H * P_pred * H^T + R)
```

The Kalman gain determines how much to trust the measurement:
- Large K: Trust measurement a lot
- Small K: Trust prediction more than measurement

**4. Update State**

```
x_hat = x_pred + K * y
```

Adjust predicted state based on measurement innovation and Kalman gain.

**5. Update Covariance**

```
P_hat = (I - K * H) * P_pred
```

or (more numerically stable):

```
P_hat = (I - K*H) * P_pred * (I - K*H)^T + K*R*K^T
```

The covariance shrinks when we get a measurement (expected!).

---

## Covariance Propagation

### Understanding Covariance Growth

During the predict step without measurements:

```
P_new = F * P * F^T + Q
```

**Two components**:
1. **F * P * F^T**: How uncertainty in x propagates to uncertainty in x_new
2. **Q**: Model uncertainty

**Intuition**:
- Without measurements, uncertainty always grows (covariance increases)
- Growth rate depends on system dynamics and Q matrix
- GPS loss causes covariance to "blow up" (dead reckoning)

### Understanding Covariance Reduction

During the update step with measurements:

```
P_new = (I - K*H) * P
```

The factor (I - K*H) is always < 1, so P shrinks.

**How much shrinkage?**
- Depends on Kalman gain K
- Depends on measurement quality (R)
- If measurement is very noisy (large R), K is small → small shrinkage
- If measurement is very accurate (small R), K is large → big shrinkage

### Steady State Uncertainty

For a stationary system, covariance converges to a steady state where:

```
F * P_ss * F^T + Q = P_ss  (during predict only)
(I - K*H) * P_ss = P_ss    (update offsets predict)
```

Balance between:
- Model uncertainty (Q) growing during predict
- Measurement correction (K*y) reducing uncertainty during update

---

## Numerical Stability

### Key Challenges

1. **Covariance Matrix Can Lose Positive-Definiteness**
   - Numerical errors accumulate
   - Results in negative variances (impossible!)
   - Filter diverges

2. **Symmetry Loss**
   - Covariance should be symmetric: P = P^T
   - But numerical errors break symmetry
   - Causes inconsistent behavior

3. **Matrix Inversion**
   - Computing K = P*H^T*(H*P*H^T + R)^-1 is numerically sensitive
   - Ill-conditioned matrices amplify errors
   - Can cause division by zero

### Mitigation Strategies

**1. Joseph Form Update** (Most Numerically Stable)

Instead of:
```
P_new = (I - K*H) * P
```

Use:
```
P_new = (I - K*H) * P * (I - K*H)^T + K*R*K^T
```

This guarantees P remains positive-definite.

**2. Enforce Symmetry**

After every covariance update:
```
P_new = 0.5 * (P_new + P_new^T)
```

Correction is tiny but crucial.

**3. Clamp Diagonal**

Prevent negative variances:
```
for i in 0..15:
    P[i,i] = max(P[i,i], epsilon)
```

Where epsilon = 1e-8 (tiny but positive).

**4. Normalize Quaternion**

After every predict step:
```
q_new = q_new / ||q_new||
```

Prevents quaternion from drifting away from unit magnitude.

**5. Carefully Order Computations**

Avoid accumulating small differences:
```
# Good: compute P - K*H*P directly
P_new = P - K * (H * P)

# Bad: compute (I - K*H) first (unnecessary intermediate)
I_KH = I - K*H
P_new = I_KH * P
```

### Practical Implementation

In auto_orientation, we use:
- Joseph form covariance update
- Enforcement of P symmetry after updates
- Clamping of P diagonal
- Quaternion normalization after predictions
- Matrix computations ordered for stability

---

## EKF vs Complementary Filter

### Complementary Filter

Simple approach that "complements" two sensors:

```
output = low_pass(gyro) + high_pass(accel/GPS)
```

**How it works**:
- Gyro: Fast, drifts over time → use high-frequency content
- Accelerometer: Slow drifts, but accurate long-term → use low-frequency content
- Combine with crossover at ~0.5 Hz

**Advantages**:
- Very simple (single equation)
- Fast computation
- Intuitive tuning

**Disadvantages**:
- No uncertainty estimates
- Fixed gain (can't adapt to noise changes)
- Suboptimal fusion (not Bayesian)
- No way to handle GPS dropout gracefully

### Extended Kalman Filter

Probabilistic approach that models noise explicitly:

```
output = weighted_average(sensors)
weight_i = 1 / variance_i
```

**How it works**:
- Maintain full covariance matrix (uncertainty)
- Predict state using process model
- Update with measurements, weighted by noise covariance
- Adapt weights based on measured uncertainty

**Advantages**:
- Provides uncertainty estimates (critical for autonomous systems!)
- Optimal fusion for Gaussian noise
- Adapts to changing conditions
- Mathematically principled
- Handles sensor dropout gracefully

**Disadvantages**:
- More complex implementation
- Higher computational cost
- More parameters to tune (Q, R matrices)
- Sensitive to model mismatch

### Comparison Table

| Aspect | Complementary | EKF |
|--------|---------------|-----|
| **Computation** | O(n) | O(n³) due to matrix ops |
| **Uncertainty** | None | Full covariance |
| **Fusion Quality** | Good | Optimal (Gaussian) |
| **Tuning** | 1 parameter | ~10 parameters |
| **Dropout Handling** | Poor | Excellent |
| **Implementation** | ~50 lines | ~200 lines |

### When to Use Each

**Use Complementary Filter if**:
- Sensors are always available
- Uncertainty estimates not needed
- Computational power is extremely limited
- Tuning time is short

**Use EKF if**:
- GPS dropout expected
- Uncertainty estimates needed (safety-critical)
- Multiple sensor types with different rates
- Longer flight times (sensor fusion quality matters)

**For auto_orientation**: EKF is the right choice because:
- GPS dropout is common in urban environments
- We need uncertainty for autonomous decision-making
- We have sufficient computational power (Arduino Mega)
- Multi-sensor fusion is complex (IMU + GPS + magnetometer)

---

## Practical Tuning Guidelines

### Understanding Q Matrix (Process Noise)

Q represents "how much we trust our motion model"

```
High Q (large values):
  - Says "our model is terrible"
  - Filter trusts measurements more
  - Responds quickly to changes
  - Can be noisy/jumpy
  
Low Q (small values):
  - Says "our model is great"
  - Filter trusts predictions more
  - Smoother output
  - Can miss rapid changes
```

**Typical values** (for 100 Hz IMU, 1 Hz GPS):
```
Q_attitude = 1e-4 rad²/s²    (small, model is good)
Q_velocity = 1e-3 m²/s³      (small, kinematics exact)
Q_position = 1e-6 m²/s²      (tiny, position integrates velocity)
Q_bias = 1e-7 m²/s⁵          (tiny, biases don't change fast)
```

### Understanding R Matrix (Measurement Noise)

R represents "how much we trust our sensors"

```
High R (large values):
  - Says "measurement is noisy"
  - Filter ignores measurements
  - Smooth but inaccurate
  
Low R (small values):
  - Says "measurement is accurate"
  - Filter trusts measurements heavily
  - Accurate but can be jumpy
```

**Typical values**:
```
R_GPS_horizontal = 4.0 m²      (2m std dev × 2m std dev)
R_GPS_vertical = 9.0 m²        (3m std dev × 3m std dev)
```

Use HDOP/VDOP if available:
```
R_horizontal = (HDOP × 2m)²
R_vertical = (VDOP × 3m)²
```

### Initial Covariance (P0)

Represents initial uncertainty about state:

```
High P0:
  - Start with large uncertainty
  - Filter takes time to converge
  - More robust to bad initial guess
  
Low P0:
  - Start with high confidence
  - Filter converges quickly
  - Risky if initial state is wrong
```

**Typical values**:
```
P0_attitude = 0.1 rad²        (10° uncertainty)
P0_velocity = 1.0 m²/s²       (1 m/s uncertainty)
P0_position = 100.0 m²        (10 m uncertainty)
P0_bias = 1.0 m²/s⁴           (1 m/s² uncertainty in bias)
```

### Tuning Procedure

1. **Start with defaults**
   - Use typical values from tuning guide
   - Test on representative data

2. **Assess performance**
   - Plot filtered vs raw measurements
   - Plot uncertainty over time
   - Check for oscillation or lag

3. **Adjust Q if**
   - Filter lags behind true motion: Increase Q
   - Filter is jittery: Decrease Q

4. **Adjust R if**
   - Filter ignores GPS: Decrease R
   - Filter too jumpy: Increase R

5. **Iterate and validate**
   - Field test on actual hardware
   - Log all outputs (state, covariance, innovations)
   - Visualize results

### Red Flags in Tuning

**Covariance diverges** (goes to infinity):
- Usually: Q too high or R too high
- Fix: Decrease Q or R

**Covariance converges to zero**:
- Usually: Q too low or R too low
- Problem: Overconfident, filter will diverge on bad sensor
- Fix: Increase Q or R

**Filter oscillates/rings**:
- Usually: Q and R mismatch
- Try: Increase Q (trust model less)

**Filter lags behind true motion**:
- Usually: Q too low or R too high
- Try: Increase Q or decrease R

---

## Implementation Notes

### Quaternion Kinematics

The quaternion rate of change is:

```
dq/dt = 0.5 * q ⊗ ω
```

Where ⊗ is quaternion multiplication:
```
q ⊗ ω = [  w*w_x + x*w_w - y*w_z + z*w_y,
           w*w_y + y*w_w - x*w_z - z*w_x,
           w*w_z + z*w_w + x*w_y - y*w_x,
          -x*w_x - y*w_y - z*w_z + w*w_w ]
```

Discrete approximation:
```
q_{k+1} = q_k + dt * 0.5 * (q_k ⊗ ω)
```

Then normalize to unit magnitude.

### Rotation Matrix from Quaternion

The rotation matrix R(q) transforms from body to NED:

```
R = [1-2y²-2z²   2xy-2wz    2xz+2wy  ]
    [2xy+2wz    1-2x²-2z²   2yz-2wx  ]
    [2xz-2wy    2yz+2wx    1-2x²-2y² ]
```

Where q = [w, x, y, z].

This is used to rotate accelerometer measurements from body frame to NED frame.

### GPS to NED Conversion

GPS gives latitude, longitude, altitude. To use in the filter, convert to NED relative to a reference point:

```
Δlat_rad = (lat - lat_ref) * π/180
Δlon_rad = (lon - lon_ref) * π/180
Δalt = alt - alt_ref

North = Δlat_rad * 6371000  (Earth radius in meters)
East = Δlon_rad * cos(lat_ref) * 6371000
Down = -Δalt
```

Then NED position is [North, East, Down] relative to origin.

### Jacobian Computation

For each nonlinear function f(x), compute the Jacobian:

```
F[i,j] = ∂f_i/∂x_j
```

Analytically or numerically (finite differences):

```
F[i,j] ≈ (f_i(x + δe_j) - f_i(x)) / δ
```

Where δ = 1e-6 and e_j is unit vector in j direction.

---

## References

### Classical Papers
- Kalman, R.E. (1960). "A new approach to linear filtering and prediction problems"
- Welch, G. & Bishop, G. (2006). "An Introduction to the Kalman Filter"

### Books
- "Fundamentals of Kalman Filtering: A Practical Approach" (Zarchan & Musoff)
- "Factor Graphs for Robot Perception" (Dellaert & Kaess)

### Related Documentation
- QUATERNION_REFERENCE.md - Quaternion mathematics
- EKF_API_REFERENCE.md - Implementation details
- EKF_TUNING_GUIDE.md - Practical tuning procedures
- IMU_GPS_SENSOR_FUSION.md - System architecture

