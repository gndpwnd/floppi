# dRehmFlight Mathematics and Algorithms Reference

> Complete mathematical documentation for every computation in the dRehmFlight Teensy BETA 1.3 flight controller.
> Source file: `dRehmFlight-master/code/dRehmFlight_Teensy_BETA_1.3.ino`

---

## 1. IMU Sensor Data Processing

### 1.1 Raw Sensor Reading and Scale Factors

The IMU produces raw 16-bit signed integer ADC values. These must be converted to physical units before use.

#### Accelerometer

The raw 16-bit value is divided by a scale factor that depends on the configured full-scale range:

```
AccX = AcX / ACCEL_SCALE_FACTOR   [G's]
AccY = AcY / ACCEL_SCALE_FACTOR   [G's]
AccZ = AcZ / ACCEL_SCALE_FACTOR   [G's]
```

| Full-Scale Range | ACCEL_SCALE_FACTOR | Sensitivity        |
|------------------|--------------------|---------------------|
| +/-2G (default)  | 16384.0            | 16384 LSB/G         |
| +/-4G            | 8192.0             | 8192 LSB/G          |
| +/-8G            | 4096.0             | 4096 LSB/G          |
| +/-16G           | 2048.0             | 2048 LSB/G          |

The scale factor is derived from the 16-bit signed range (-32768 to +32767) divided by twice the full-scale range. For +/-2G: 65536 / 4 = 16384.

#### Gyroscope

```
GyroX = GyX / GYRO_SCALE_FACTOR   [deg/sec]
GyroY = GyY / GYRO_SCALE_FACTOR   [deg/sec]
GyroZ = GyZ / GYRO_SCALE_FACTOR   [deg/sec]
```

| Full-Scale Range       | GYRO_SCALE_FACTOR | Sensitivity          |
|------------------------|-------------------|----------------------|
| +/-250 DPS (default)   | 131.0             | 131 LSB/(deg/sec)    |
| +/-500 DPS             | 65.5              | 65.5 LSB/(deg/sec)   |
| +/-1000 DPS            | 32.8              | 32.8 LSB/(deg/sec)   |
| +/-2000 DPS            | 16.4              | 16.4 LSB/(deg/sec)   |

#### Magnetometer (MPU9250 only)

The magnetometer raw value is divided by 6.0 to convert to microtesla, then hard-iron and soft-iron corrections are applied:

```
MagX = MgX / 6.0                          [uT, uncorrected]
MagX = (MagX - MagErrorX) * MagScaleX     [uT, corrected]
```

- **MagErrorX/Y/Z**: Hard-iron bias offsets (constant magnetic field from nearby ferrous materials on the PCB). These shift the center of the measured magnetic sphere to the origin.
- **MagScaleX/Y/Z**: Soft-iron scale factors (distortion of the magnetic field into an ellipsoid). These reshape the ellipsoid back into a sphere. Default is 1.0 (no correction).

```mermaid
flowchart LR
    subgraph "Magnetometer Correction"
        RAW["Raw ADC<br/>(int16)"] --> DIV6["/ 6.0<br/>= uT"]
        DIV6 --> HARD["- MagError<br/>(hard iron)"]
        HARD --> SOFT["* MagScale<br/>(soft iron)"]
        SOFT --> OUT["Corrected uT"]
    end
```

### 1.2 Low-Pass Filtering (First-Order IIR)

After scaling, all sensor readings pass through a first-order IIR (Infinite Impulse Response) low-pass filter, also known as an Exponential Moving Average (EMA):

```
filtered[n] = (1 - B) * filtered[n-1] + B * raw[n]
```

This is equivalent to the transfer function of a single-pole low-pass filter in the Z-domain:

```
H(z) = B / (1 - (1-B) * z^(-1))
```

#### Filter Coefficients

| Signal          | Coefficient (B)  | Approximate Cutoff Frequency |
|-----------------|-------------------|------------------------------|
| Accelerometer   | B_accel = 0.14    | ~44.6 Hz                     |
| Gyroscope       | B_gyro = 0.1      | ~31.8 Hz                     |
| Magnetometer    | B_mag = 1.0       | No filtering (passthrough)   |
| Radio commands  | b = 0.7           | ~222.8 Hz                    |

#### Cutoff Frequency Derivation

For a first-order IIR filter running at sample rate `f_s`, the -3dB cutoff frequency is:

```
f_c = -f_s / (2*pi) * ln(1 - B)
```

For small B, this simplifies to the approximation:

```
f_c ~ B * f_s / (2*pi)
```

With `f_s = 2000 Hz` (the loop rate):
- Accelerometer: `f_c ~ 0.14 * 2000 / (2*pi) = 44.6 Hz`
- Gyroscope: `f_c ~ 0.1 * 2000 / (2*pi) = 31.8 Hz`

The exact formula gives:
- Accelerometer: `f_c = -2000/(2*pi) * ln(0.86) = 48.0 Hz`
- Gyroscope: `f_c = -2000/(2*pi) * ln(0.9) = 33.5 Hz`

#### Interpretation

- **B = 0**: Output never changes (infinite memory, zero bandwidth).
- **B = 1**: No filtering; output equals current input (full bandwidth passthrough).
- **Larger B**: More responsive but noisier. Smaller B: smoother but more lag.

The magnetometer uses `B_mag = 1.0` meaning no filtering is applied (the Madgwick filter itself handles magnetometer fusion). Radio commands use `b = 0.7`, which is relatively aggressive but appropriate since radio signals update at a much lower rate than the 2kHz loop.

```mermaid
flowchart TD
    subgraph "Low-Pass Filter Pipeline (per axis)"
        RAW["Raw Scaled Value"] --> LP["filtered = (1-B)*prev + B*current"]
        LP --> PREV["Store as prev"]
        PREV --> LP
        LP --> OUT["Filtered Output"]
    end
```

### 1.3 IMU Error Calibration

On startup, the `calculate_IMU_error()` function computes bias offsets by averaging 12,000 samples while the vehicle is stationary and level:

```
AccErrorX = (1/N) * SUM(AccX_i)   for i = 1..12000
AccErrorY = (1/N) * SUM(AccY_i)
AccErrorZ = (1/N) * SUM(AccZ_i) - 1.0    <-- subtract gravity!
GyroErrorX = (1/N) * SUM(GyroX_i)
GyroErrorY = (1/N) * SUM(GyroY_i)
GyroErrorZ = (1/N) * SUM(GyroZ_i)
```

Key points:
- The accelerometer Z-axis error has `1.0` subtracted because when the vehicle is level, Z should read exactly 1.0 G (gravity). The error is the deviation from this expected value.
- Gyroscope errors represent the zero-rate offset (bias drift). When stationary, all gyro readings should be zero.
- These errors are subtracted from every subsequent reading in `getIMUdata()`:
  ```
  AccX = AccX - AccErrorX
  GyroX = GyroX - GyroErrorX
  ```
- The calibration values are printed to the serial monitor and then hard-coded into the source. The calibration function is then commented out for production use.

```mermaid
flowchart TD
    subgraph "Complete IMU Data Pipeline"
        ADC["16-bit Raw ADC"] --> SCALE["Divide by<br/>Scale Factor"]
        SCALE --> ERRSUB["Subtract<br/>Calibration Error"]
        ERRSUB --> LPF["First-Order<br/>IIR Low-Pass"]
        LPF --> STORE["Store as<br/>_prev for next"]
        LPF --> OUTPUT["AccX, AccY, AccZ<br/>GyroX, GyroY, GyroZ<br/>MagX, MagY, MagZ"]
    end
```

---

## 2. Madgwick Attitude Estimation Filter

The Madgwick filter is a computationally efficient sensor fusion algorithm that combines gyroscope, accelerometer, and (optionally) magnetometer data to estimate the vehicle's orientation as a quaternion. It was published by Sebastian Madgwick in 2010.

### 2.1 Quaternion Representation

A unit quaternion encodes a 3D rotation without gimbal lock:

```
q = q0 + q1*i + q2*j + q3*k = [q0, q1, q2, q3]
```

where:
- `q0` is the scalar (real) part
- `q1, q2, q3` are the vector (imaginary) parts
- The unit constraint: `q0^2 + q1^2 + q2^2 + q3^2 = 1`

The quaternion is initialized to the identity rotation (level orientation):

```c
float q0 = 1.0f;
float q1 = 0.0f;
float q2 = 0.0f;
float q3 = 0.0f;
```

This represents zero rotation: the body frame is aligned with the world frame (NWU -- North-West-Up convention).

### 2.2 Axis Remapping at the Call Site

Before the Madgwick function is called, the code remaps sensor axes to match the NWU convention:

```c
Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
```

The negations and swaps align the IMU's physical axes with the expected NWU frame of the Madgwick algorithm.

### 2.3 The 6DOF Algorithm (Madgwick6DOF)

This variant is used with the MPU6050 (no magnetometer). It fuses gyroscope and accelerometer data only.

#### Step 1: Convert Gyroscope to Radians

```
gx_rad = gx * 0.0174533    (pi/180 = 0.0174533)
gy_rad = gy * 0.0174533
gz_rad = gz * 0.0174533
```

#### Step 2: Quaternion Derivative from Gyroscope

The kinematic equation for quaternion propagation using angular velocity is:

```
q_dot = (1/2) * q (x) omega
```

where `(x)` denotes quaternion multiplication and `omega = [0, gx, gy, gz]` is the angular velocity as a pure quaternion. Expanding:

```
qDot1 = 0.5 * (-q1*gx - q2*gy - q3*gz)
qDot2 = 0.5 * ( q0*gx + q2*gz - q3*gy)
qDot3 = 0.5 * ( q0*gy - q1*gz + q3*gx)
qDot4 = 0.5 * ( q0*gz + q1*gy - q2*gx)
```

This can be written in matrix form:

```
| qDot1 |         |  0  -gx  -gy  -gz | | q0 |
| qDot2 | = 0.5 * | gx   0    gz  -gy | | q1 |
| qDot3 |         | gy  -gz   0    gx | | q2 |
| qDot4 |         | gz   gy  -gx   0  | | q3 |
```

This gyroscope-only estimate would drift over time because gyroscopes have bias errors that accumulate during integration.

#### Step 3: Accelerometer Correction via Gradient Descent

The accelerometer measures the direction of gravity in the body frame. In the world frame, gravity points in the `[0, 0, 1]` direction (assuming NWU, where Z is up). The expected gravity direction in the body frame, given quaternion `q`, is:

```
g_expected = [2*(q1*q3 - q0*q2),
              2*(q0*q1 + q2*q3),
              q0^2 - q1^2 - q2^2 + q3^2]
```

The objective function `f` is the difference between the measured (normalized) accelerometer reading and the expected gravity direction:

```
f = g_expected - a_measured
```

The gradient of `||f||^2` with respect to `q` gives the direction to adjust `q` to minimize this error:

```
s0 = 4*q0*q2q2 + 2*q2*ax + 4*q0*q1q1 - 2*q1*ay
s1 = 4*q1*q3q3 - 2*q3*ax + 4*q0q0*q1 - 2*q0*ay - 4*q1 + 8*q1*q1q1 + 8*q1*q2q2 + 4*q1*az
s2 = 4*q0q0*q2 + 2*q0*ax + 4*q2*q3q3 - 2*q3*ay - 4*q2 + 8*q2*q1q1 + 8*q2*q2q2 + 4*q2*az
s3 = 4*q1q1*q3 - 2*q1*ax + 4*q2q2*q3 - 2*q2*ay
```

The gradient is then normalized:

```
norm = 1 / sqrt(s0^2 + s1^2 + s2^2 + s3^2)
s0 *= norm;  s1 *= norm;  s2 *= norm;  s3 *= norm;
```

#### Step 4: Apply Correction

The correction term is subtracted from the gyroscope-derived quaternion derivative, weighted by `B_madgwick`:

```
qDot1 -= B_madgwick * s0
qDot2 -= B_madgwick * s1
qDot3 -= B_madgwick * s2
qDot4 -= B_madgwick * s3
```

`B_madgwick = 0.04` controls the trade-off:
- **Higher B_madgwick**: Trusts accelerometer more; converges faster but noisier (susceptible to vibration).
- **Lower B_madgwick**: Trusts gyroscope more; smoother but slower to correct drift.

#### Step 5: Integrate

Forward Euler integration of the quaternion derivative:

```
q0 += qDot1 * dt
q1 += qDot2 * dt
q2 += qDot3 * dt
q3 += qDot4 * dt
```

where `dt` is `invSampleFreq`, the time elapsed since the last loop iteration in seconds.

#### Step 6: Normalize

Re-normalize the quaternion to maintain the unit constraint (numerical integration causes drift from unit length):

```
norm = 1 / sqrt(q0^2 + q1^2 + q2^2 + q3^2)
q0 *= norm;  q1 *= norm;  q2 *= norm;  q3 *= norm;
```

```mermaid
flowchart TD
    subgraph "Madgwick 6DOF Algorithm"
        GY["Gyro (deg/s)"] --> CONV["* 0.0174533<br/>= rad/s"]
        CONV --> QDOT["Quaternion Derivative<br/>q_dot = 0.5 * q x omega"]
        AC["Accel (G)"] --> NORM_A["Normalize<br/>a / |a|"]
        NORM_A --> GRAD["Gradient Descent<br/>Compute s0..s3"]
        GRAD --> NORM_S["Normalize<br/>Gradient"]
        NORM_S --> CORRECT["Apply Correction<br/>qDot -= B * s"]
        QDOT --> CORRECT
        CORRECT --> INTEGRATE["Euler Integration<br/>q += qDot * dt"]
        INTEGRATE --> NORM_Q["Normalize<br/>Quaternion"]
        NORM_Q --> EULER["Quaternion to<br/>Euler Angles"]
        EULER --> ANGLES["roll_IMU<br/>pitch_IMU<br/>yaw_IMU"]
    end
```

### 2.4 The 9DOF Algorithm (with Magnetometer)

Used with the MPU9250 when magnetometer data is available. The structure is identical to the 6DOF algorithm but with an extended gradient descent that also minimizes the error between the measured magnetic field and the expected Earth's magnetic field direction.

#### Reference Magnetic Field Computation

The Earth's magnetic field in the body frame is rotated to the world frame using the current quaternion estimate, then projected onto the horizontal plane:

```
hx = mx*q0q0 - 2*q0*my*q3 + 2*q0*mz*q2 + mx*q1q1 + 2*q1*my*q2 + 2*q1*mz*q3 - mx*q2q2 - mx*q3q3
hy = 2*q0*mx*q3 + my*q0q0 - 2*q0*mz*q1 + 2*q1*mx*q2 - my*q1q1 + my*q2q2 + 2*q2*mz*q3 - my*q3q3
```

The reference field is then defined as:

```
_2bx = sqrt(hx^2 + hy^2)     [horizontal component magnitude]
_2bz = (vertical component)   [computed similarly]
```

This means the algorithm does not need to know the local magnetic declination -- it dynamically computes the reference from the current measurement and only uses the relative direction.

#### Extended Gradient

The gradient `s0..s3` now includes terms from both the gravity objective function AND the magnetic field objective function. The expressions are significantly longer (each line in the source code combines both contributions):

```
s0 = (gravity terms) + (magnetic field terms)
s1 = (gravity terms) + (magnetic field terms)
s2 = (gravity terms) + (magnetic field terms)
s3 = (gravity terms) + (magnetic field terms)
```

The magnetic field terms involve `_2bx`, `_2bz`, `_4bx = 2*_2bx`, and `_4bz = 2*_2bz` along with the measured magnetometer vector `[mx, my, mz]`.

The rest of the algorithm (correction, integration, normalization, Euler extraction) is identical to the 6DOF version.

```mermaid
flowchart TD
    subgraph "Madgwick 9DOF Algorithm"
        GY["Gyro (deg/s)"] --> CONV["* 0.0174533<br/>= rad/s"]
        CONV --> QDOT["Quaternion Derivative<br/>q_dot = 0.5 * q x omega"]

        AC["Accel (G)"] --> NORMA["Normalize Accel"]
        MG["Mag (uT)"] --> NORMM["Normalize Mag"]

        NORMM --> REFB["Compute Reference<br/>Magnetic Field<br/>hx, hy, _2bx, _2bz"]
        REFB --> GRAD9["Extended Gradient Descent<br/>(gravity + magnetic)"]
        NORMA --> GRAD9

        GRAD9 --> NORMS["Normalize Gradient"]
        NORMS --> CORRECT["qDot -= B * s"]
        QDOT --> CORRECT
        CORRECT --> INT["q += qDot * dt"]
        INT --> NORMQ["Normalize q"]
        NORMQ --> EULER["Quaternion to Euler"]
        EULER --> OUT["roll_IMU, pitch_IMU, yaw_IMU"]
    end
```

### 2.5 Quaternion to Euler Angles

After the quaternion is updated, Euler angles are extracted using the NWU convention:

```
roll_IMU  =  atan2(q0*q1 + q2*q3, 0.5 - q1^2 - q2^2)  * 57.29577951
pitch_IMU = -asin(constrain(-2*(q1*q3 - q0*q2), -0.999999, 0.999999)) * 57.29577951
yaw_IMU   = -atan2(q1*q2 + q0*q3, 0.5 - q2^2 - q3^2)  * 57.29577951
```

Where `57.29577951 = 180/pi` converts radians to degrees.

**Important details:**

- The `constrain(-2*(q1*q3 - q0*q2), -0.999999, 0.999999)` prevents the `asin` argument from exceeding `[-1, +1]` due to floating-point errors, which would return `NaN`. This also provides **gimbal lock protection** near +/-90 degree pitch.
- The `0.5 - q1^2 - q2^2` terms come from the rotation matrix elements derived from the quaternion. Specifically, using the identity `q0^2 + q1^2 + q2^2 + q3^2 = 1`, we get `q0^2 - q1^2 - q2^2 + q3^2 = 1 - 2*q1^2 - 2*q2^2 = 2*(0.5 - q1^2 - q2^2)`. Since `atan2` is scale-invariant, the factor of 2 is dropped.
- The negative signs on `pitch_IMU` and `yaw_IMU` adjust the sign conventions to match the desired output frame.

These are the ZYX (yaw-pitch-roll) Tait-Bryan angles, representing sequential rotations about Z (yaw), Y (pitch), X (roll).

### 2.6 Fast Inverse Square Root

The `invSqrt()` function computes `1/sqrt(x)` and is called repeatedly for normalization. The source contains two commented-out versions of the famous "fast inverse square root" algorithm (originally from Quake III Arena):

```c
// Quake III version (commented out):
float halfx = 0.5f * x;
float y = x;
long i = *(long*)&y;
i = 0x5f3759df - (i>>1);    // "magic number" bit-level hack
y = *(float*)&i;
y = y * (1.5f - (halfx * y * y));  // Newton-Raphson iteration
y = y * (1.5f - (halfx * y * y));  // Second iteration for accuracy
return y;
```

This uses IEEE 754 floating-point bit manipulation to compute an initial approximation, then refines it with Newton-Raphson iterations. On the Teensy 4.0 (ARM Cortex-M7 with FPU), the hardware floating-point unit makes the standard library version fast enough:

```c
return 1.0/sqrtf(x);  // Used in practice
```

---

## 3. Desired State Computation

The `getDesState()` function converts raw radio PWM values (typically 1000-2000 us) into normalized control commands.

### 3.1 Throttle

```
thro_des = (channel_1_pwm - 1000) / 1000
```

Maps PWM 1000-2000 to the range [0.0, 1.0]. Constrained to [0, 1].

### 3.2 Roll, Pitch, Yaw

```
roll_des  = (channel_2_pwm - 1500) / 500    --> [-1, 1]
pitch_des = (channel_3_pwm - 1500) / 500    --> [-1, 1]
yaw_des   = (channel_4_pwm - 1500) / 500    --> [-1, 1]
```

Then scaled by the maximum allowed angle/rate:

```
roll_des  = constrain(roll_des, -1, 1) * maxRoll     [degrees or deg/sec]
pitch_des = constrain(pitch_des, -1, 1) * maxPitch    [degrees or deg/sec]
yaw_des   = constrain(yaw_des, -1, 1) * maxYaw        [deg/sec]
```

Default values: `maxRoll = 30 deg`, `maxPitch = 30 deg`, `maxYaw = 160 deg/sec`.

### 3.3 Passthrough Commands

For direct (unstabilized) actuator control:

```
roll_passthru  = roll_des_normalized / 2.0    --> [-0.5, 0.5]
pitch_passthru = pitch_des_normalized / 2.0   --> [-0.5, 0.5]
yaw_passthru   = yaw_des_normalized / 2.0     --> [-0.5, 0.5]
```

These are computed before the maxAngle scaling is applied (from the [-1,1] normalized value).

```mermaid
flowchart LR
    subgraph "Desired State Computation"
        PWM["Radio PWM<br/>1000-2000 us"] --> THR["Throttle<br/>(ch1 - 1000) / 1000<br/>[0, 1]"]
        PWM --> RPY["Roll/Pitch/Yaw<br/>(chN - 1500) / 500<br/>[-1, 1]"]
        RPY --> SCALE["* maxRoll/Pitch/Yaw"]
        RPY --> PASS["Passthrough<br/>/ 2.0<br/>[-0.5, 0.5]"]
        SCALE --> CONSTRAIN["constrain()"]
        CONSTRAIN --> OUT["roll_des<br/>pitch_des<br/>yaw_des"]
    end
```

---

## 4. PID Control Mathematics

dRehmFlight implements three PID controller variants. All share common features:
- Output is scaled by `0.01` to bring it into approximately [-1, 1] range
- Integrators are saturated at `+/- i_limit` (default 25.0)
- Integrators are reset to 0 when throttle PWM < 1060 (preventing wind-up on the ground)
- Yaw is always controlled in **rate mode** (even in `controlANGLE`)

### 4.1 controlANGLE() -- Single-Loop Angle Stabilization

This is the primary controller for beginners. It uses angle error for roll/pitch and rate error for yaw.

#### Roll and Pitch

```
error     = desired_angle - measured_angle     (e.g., roll_des - roll_IMU)
integral  = integral_prev + error * dt
integral  = constrain(integral, -i_limit, i_limit)
derivative = GyroX                              (measured angular rate, NOT derivative of error!)

output = 0.01 * (Kp * error + Ki * integral - Kd * derivative)
```

**Critical note on the D-term**: The derivative term uses the **raw gyroscope reading** (angular rate), not `d(error)/dt`. This is the "derivative on measurement" technique. The key insight:

```
d(error)/dt = d(desired - measured)/dt = d(desired)/dt - d(measured)/dt
```

Since desired angle changes slowly (or is constant between updates), `d(desired)/dt ~ 0`, so `d(error)/dt ~ -d(measured)/dt = -GyroX`. The code uses `- Kd * GyroX` which is equivalent to `+ Kd * (-GyroX)`. This avoids the "derivative kick" problem where sudden changes in setpoint cause large D-term spikes.

The sign is **negative** (`- Kd * derivative`) because `GyroX` is the rate in the same direction as increasing `roll_IMU`, so it naturally opposes the error without an extra negation.

#### Yaw

Yaw always operates in rate mode:

```
error     = yaw_des - GyroZ                    (desired rate - measured rate)
integral  = integral_prev + error * dt
integral  = constrain(integral, -i_limit, i_limit)
derivative = (error - error_prev) / dt          (derivative of error)

output = 0.01 * (Kp * error + Ki * integral + Kd * derivative)
```

Note: Yaw uses `+ Kd` (standard derivative of error), while roll/pitch use `- Kd` (derivative on measurement).

#### Default Gains

| Parameter | Roll | Pitch | Yaw |
|-----------|------|-------|-----|
| Kp        | 0.2  | 0.2   | 0.3 |
| Ki        | 0.3  | 0.3   | 0.05 |
| Kd        | 0.05 | 0.05  | 0.00015 |

```mermaid
flowchart TD
    subgraph "controlANGLE - Roll/Pitch"
        DES_A["roll_des<br/>(degrees)"] --> ERR_A["error =<br/>roll_des - roll_IMU"]
        IMU_A["roll_IMU<br/>(degrees)"] --> ERR_A
        ERR_A --> P_A["P = Kp * error"]
        ERR_A --> I_A["I += error * dt<br/>constrain to +/-25"]
        GYRO_A["GyroX<br/>(deg/sec)"] --> D_A["D = GyroX<br/>(direct measurement)"]
        P_A --> SUM_A["output = 0.01 *<br/>(P + I - D)"]
        I_A --> SUM_A
        D_A --> SUM_A
        SUM_A --> OUT_A["roll_PID<br/>~ [-1, 1]"]
    end

    subgraph "controlANGLE - Yaw"
        DES_Y["yaw_des<br/>(deg/sec)"] --> ERR_Y["error =<br/>yaw_des - GyroZ"]
        GYRO_Y["GyroZ<br/>(deg/sec)"] --> ERR_Y
        ERR_Y --> P_Y["P = Kp * error"]
        ERR_Y --> I_Y["I += error * dt<br/>constrain to +/-25"]
        ERR_Y --> D_Y["D = (error - error_prev) / dt"]
        P_Y --> SUM_Y["output = 0.01 *<br/>(P + I + D)"]
        I_Y --> SUM_Y
        D_Y --> SUM_Y
        SUM_Y --> OUT_Y["yaw_PID<br/>~ [-1, 1]"]
    end
```

### 4.2 controlANGLE2() -- Cascaded Angle + Rate Control

This is a two-loop (cascaded) controller that provides better performance but is harder to tune. The outer loop converts angle error into a desired rate, and the inner loop tracks that desired rate.

#### Outer Loop (Angle to Rate)

```
error     = desired_angle - measured_angle        (roll_des - roll_IMU)
integral  = integral_prev_ol + error * dt
integral  = constrain(integral, -i_limit, i_limit)
// Note: D-term is COMMENTED OUT in the code
rate_des  = Kp_angle * error + Ki_angle * integral

rate_des  = rate_des * Kl          (Kl = 30.0, loop gain)
rate_des  = constrain(rate_des, -240, 240)    [deg/sec]
rate_des  = (1 - B_loop) * rate_des_prev + B_loop * rate_des
```

The loop gain `Kl = 30.0` amplifies the PI output to produce desired angular rates. The constraint at +/-240 deg/sec prevents excessive rates. The LP filter with `B_loop_roll = 0.9` (or `B_loop_pitch = 0.9`) provides **artificial damping** -- it smooths the transition of the commanded rate, reducing overshoot without needing an explicit D-term in the outer loop.

#### Inner Loop (Rate PID)

```
error     = rate_des_from_outer - GyroX           (desired rate - measured rate)
integral  = integral_prev_il + error * dt
integral  = constrain(integral, -i_limit, i_limit)
derivative = (error - error_prev) / dt

output = 0.01 * (Kp_rate * error + Ki_rate * integral + Kd_rate * derivative)
```

This uses the **rate-mode gains** (`Kp_roll_rate`, `Ki_roll_rate`, `Kd_roll_rate`), not the angle-mode gains.

Yaw in `controlANGLE2()` is identical to yaw in `controlANGLE()`.

```mermaid
flowchart TD
    subgraph "controlANGLE2 - Cascaded (Roll shown)"
        DES["roll_des<br/>(degrees)"] --> ERR_OL["Outer Error =<br/>roll_des - roll_IMU"]
        IMU["roll_IMU"] --> ERR_OL

        ERR_OL --> P_OL["P = Kp_angle * error"]
        ERR_OL --> I_OL["I += error * dt"]
        P_OL --> PI_OL["PI_out = P + I"]
        I_OL --> PI_OL

        PI_OL --> GAIN["* Kl (30.0)"]
        GAIN --> CLAMP["constrain<br/>+/-240 deg/s"]
        CLAMP --> LPF["LP Filter<br/>B_loop = 0.9"]
        LPF --> RATE_DES["roll_des_ol<br/>(desired rate)"]

        RATE_DES --> ERR_IL["Inner Error =<br/>roll_des_ol - GyroX"]
        GYRO["GyroX"] --> ERR_IL

        ERR_IL --> P_IL["P = Kp_rate * error"]
        ERR_IL --> I_IL["I += error * dt"]
        ERR_IL --> D_IL["D = (error - prev) / dt"]

        P_IL --> PID_IL["output = 0.01 *<br/>(P + I + D)"]
        I_IL --> PID_IL
        D_IL --> PID_IL
        PID_IL --> OUT["roll_PID"]
    end
```

### 4.3 controlRATE() -- Pure Rate Stabilization

This is the simplest controller, used for acrobatic flight. It stabilizes on a desired angular rate.

```
error     = desired_rate - measured_rate       (roll_des - GyroX)
integral  = integral_prev + error * dt
integral  = constrain(integral, -i_limit, i_limit)
derivative = (error - error_prev) / dt

output = 0.01 * (Kp_rate * error + Ki_rate * integral + Kd_rate * derivative)
```

All three axes (roll, pitch, yaw) use the same structure. `maxRoll` and `maxPitch` represent maximum rates in deg/sec (not angles) when in rate mode.

#### Default Rate Gains

| Parameter | Roll   | Pitch  | Yaw     |
|-----------|--------|--------|---------|
| Kp        | 0.15   | 0.15   | 0.3     |
| Ki        | 0.2    | 0.2    | 0.05    |
| Kd        | 0.0002 | 0.0002 | 0.00015 |

```mermaid
flowchart TD
    subgraph "controlRATE - All Axes"
        DES["roll_des<br/>(deg/sec)"] --> ERR["error =<br/>roll_des - GyroX"]
        GYRO["GyroX<br/>(deg/sec)"] --> ERR
        ERR --> P["P = Kp_rate * error"]
        ERR --> I["I += error * dt<br/>constrain to +/-25"]
        ERR --> D["D = (error - error_prev) / dt"]
        P --> SUM["output = 0.01 *<br/>(P + I + D)"]
        I --> SUM
        D --> SUM
        SUM --> OUT["roll_PID"]
    end
```

### 4.4 Integrator Anti-Windup

All PID controllers implement two anti-windup mechanisms:

1. **Saturation clamping**: `integral = constrain(integral, -i_limit, i_limit)` where `i_limit = 25.0`. This prevents the integral term from growing unboundedly when the error persists (e.g., the vehicle is held at an angle while on the ground).

2. **Low-throttle reset**: When `channel_1_pwm < 1060` (throttle essentially at zero), the integral is forced to zero. This prevents integral wind-up on the ground and ensures the integrator starts from zero at takeoff.

---

## 5. Command Scaling

### 5.1 Motor Commands (OneShot125)

PID outputs and throttle are mixed in `controlMixer()` (application-specific), producing `mX_command_scaled` values in approximately [0, 1]. These are then scaled to OneShot125 protocol timing:

```
m_command_PWM = m_command_scaled * 125 + 125
m_command_PWM = constrain(m_command_PWM, 125, 250)
```

| Scaled Value | PWM (us) | Meaning        |
|-------------|----------|----------------|
| 0.0         | 125      | Motor off      |
| 1.0         | 250      | Full throttle  |

The `commandMotors()` function implements OneShot125 by bit-banging GPIO: all motor pins go HIGH simultaneously, then each pin goes LOW after its specific pulse duration has elapsed.

### 5.2 Servo Commands

```
s_command_PWM = s_command_scaled * 180
s_command_PWM = constrain(s_command_PWM, 0, 180)
```

| Scaled Value | Servo Angle | Meaning          |
|-------------|-------------|------------------|
| 0.0         | 0 degrees   | Minimum position |
| 0.5         | 90 degrees  | Center position  |
| 1.0         | 180 degrees | Maximum position |

The PWMServo library converts these 0-180 degree values to the appropriate 1000-2000 us PWM signals.

```mermaid
flowchart LR
    subgraph "Command Scaling"
        PID["PID Outputs<br/>~[-1, 1]"] --> MIX["controlMixer()<br/>Quad example:<br/>m1 = thro - pitch + roll + yaw"]
        MIX --> MSCALE["Motor: *125 + 125<br/>constrain [125, 250]"]
        MIX --> SSCALE["Servo: *180<br/>constrain [0, 180]"]
        MSCALE --> ONESHOT["OneShot125<br/>GPIO bit-bang"]
        SSCALE --> PWM["PWMServo<br/>library"]
    end
```

### 5.3 Example Quad Mixer

The default mixer implements a standard quadcopter "X" configuration:

```
m1 (Front Left)  = thro_des - pitch_PID + roll_PID + yaw_PID
m2 (Front Right) = thro_des - pitch_PID - roll_PID - yaw_PID
m3 (Back Right)  = thro_des + pitch_PID - roll_PID + yaw_PID
m4 (Back Left)   = thro_des + pitch_PID + roll_PID - yaw_PID
```

The sign pattern encodes:
- **Pitch**: Front motors subtract pitch_PID (nose-down torque), rear motors add it
- **Roll**: Left motors add roll_PID, right motors subtract it
- **Yaw**: Diagonal pairs share yaw sign (CW props on one diagonal, CCW on the other)

---

## 6. Transition Fading Utilities

### 6.1 floatFaderLinear()

Linearly interpolates a parameter between min and max at a fixed rate:

```
diffParam = (param_max - param_min) / (fadeTime * loopFreq)

if state == 1:    param += diffParam    (fade toward max)
if state == 0:    param -= diffParam    (fade toward min)

param = constrain(param, param_min, param_max)
```

Each loop iteration moves the parameter by `diffParam`. After `fadeTime` seconds (at `loopFreq` Hz), the full range is traversed.

### 6.2 floatFaderLinear2()

Fades a parameter toward a target value, with independent rates for increasing and decreasing:

```
if param > param_des:    (need to decrease)
    diffParam = (param_upper - param_des) / (fadeTime_down * loopFreq)
    param -= diffParam

if param < param_des:    (need to increase)
    diffParam = (param_des - param_lower) / (fadeTime_up * loopFreq)
    param += diffParam

param = constrain(param, param_lower, param_upper)
```

Note that the rate is computed based on the distance from the boundary to the target, not the current distance to the target. This means the approach speed is constant (linear), not proportional to remaining distance.

---

## 7. Comprehensive Math Flow Diagrams

### 7.1 Complete Pipeline: Sensors to Actuators

```mermaid
flowchart TD
    subgraph SENSORS ["Sensor Hardware"]
        MPU["MPU6050 / MPU9250<br/>I2C / SPI"]
        RX["Radio Receiver<br/>PWM/PPM/SBUS/DSM"]
    end

    subgraph IMU_PROC ["IMU Processing (getIMUdata)"]
        SCALE_A["Accel: raw / 16384.0 = G"]
        SCALE_G["Gyro: raw / 131.0 = deg/s"]
        SCALE_M["Mag: raw / 6.0 = uT"]
        ERRSUB_A["Subtract AccError"]
        ERRSUB_G["Subtract GyroError"]
        MAG_CAL["(Mag - MagError) * MagScale"]
        LPF_A["LP Filter B=0.14"]
        LPF_G["LP Filter B=0.1"]
        LPF_M["LP Filter B=1.0"]
    end

    subgraph ATTITUDE ["Attitude Estimation (Madgwick)"]
        DEG2RAD["Gyro * pi/180"]
        QDOT["q_dot = 0.5 * q x omega"]
        GRADIENT["Gradient Descent<br/>Correction"]
        INTEGRATE["q += qDot * dt"]
        NORM_Q["Normalize q"]
        Q2E["Quaternion to Euler<br/>roll, pitch, yaw"]
    end

    subgraph RADIO ["Radio Processing"]
        GET_CMD["getCommands()"]
        LPF_R["LP Filter b=0.7"]
        FAILSAFE["failSafe() check"]
        DES_STATE["getDesState()<br/>Normalize to<br/>angles/rates"]
    end

    subgraph CONTROL ["PID Controller"]
        CTRL["controlANGLE() or<br/>controlANGLE2() or<br/>controlRATE()"]
        ROLL_PID["roll_PID"]
        PITCH_PID["pitch_PID"]
        YAW_PID["yaw_PID"]
    end

    subgraph OUTPUT ["Actuator Output"]
        MIXER["controlMixer()<br/>Vehicle-specific mixing"]
        SCALE_CMD["scaleCommands()<br/>Motors: *125+125<br/>Servos: *180"]
        THRO_CUT["throttleCut()<br/>Safety override"]
        MOTORS["commandMotors()<br/>OneShot125"]
        SERVOS["servo.write()<br/>PWM"]
    end

    MPU --> SCALE_A & SCALE_G & SCALE_M
    SCALE_A --> ERRSUB_A --> LPF_A
    SCALE_G --> ERRSUB_G --> LPF_G
    SCALE_M --> MAG_CAL --> LPF_M

    LPF_A --> GRADIENT
    LPF_G --> DEG2RAD --> QDOT
    LPF_M --> GRADIENT
    QDOT --> INTEGRATE
    GRADIENT --> INTEGRATE
    INTEGRATE --> NORM_Q --> Q2E

    RX --> GET_CMD --> LPF_R --> FAILSAFE --> DES_STATE

    Q2E --> CTRL
    LPF_G --> CTRL
    DES_STATE --> CTRL

    CTRL --> ROLL_PID & PITCH_PID & YAW_PID
    DES_STATE -->|thro_des| MIXER
    ROLL_PID & PITCH_PID & YAW_PID --> MIXER

    MIXER --> SCALE_CMD --> THRO_CUT
    THRO_CUT --> MOTORS & SERVOS
```

### 7.2 Madgwick Filter Detailed Data Flow

```mermaid
flowchart TD
    subgraph INPUT ["Inputs"]
        GX["gx (deg/s)"]
        GY["gy (deg/s)"]
        GZ["gz (deg/s)"]
        AX["ax (G)"]
        AY["ay (G)"]
        AZ["az (G)"]
        DT["dt (seconds)"]
        Q_PREV["q0,q1,q2,q3<br/>(previous)"]
    end

    subgraph GYRO_PATH ["Gyroscope Path"]
        RAD["Convert to rad/s<br/>* 0.0174533"]
        QKIN["Quaternion Kinematics<br/>qDot1 = 0.5*(-q1*gx - q2*gy - q3*gz)<br/>qDot2 = 0.5*(q0*gx + q2*gz - q3*gy)<br/>qDot3 = 0.5*(q0*gy - q1*gz + q3*gx)<br/>qDot4 = 0.5*(q0*gz + q1*gy - q2*gx)"]
    end

    subgraph ACCEL_PATH ["Accelerometer Correction Path"]
        ANORM["Normalize: a / |a|"]
        AUX["Compute auxiliary<br/>_2q0, _2q1, _4q0, etc."]
        OBJFN["Evaluate gradient<br/>of gravity objective"]
        SNORM["Normalize gradient<br/>s / |s|"]
    end

    subgraph FUSION ["Sensor Fusion"]
        CORRECT["qDot -= B_madgwick * s<br/>(B=0.04)"]
        EULER_INT["Forward Euler:<br/>q += qDot * dt"]
        QNORM["Normalize quaternion<br/>q / |q|"]
    end

    subgraph OUTPUT ["Outputs"]
        ROLL["roll_IMU = atan2(...) * 57.3"]
        PITCH["pitch_IMU = -asin(constrain(...)) * 57.3"]
        YAW["yaw_IMU = -atan2(...) * 57.3"]
    end

    GX & GY & GZ --> RAD --> QKIN
    AX & AY & AZ --> ANORM --> AUX --> OBJFN --> SNORM
    Q_PREV --> QKIN
    Q_PREV --> AUX

    QKIN --> CORRECT
    SNORM --> CORRECT
    CORRECT --> EULER_INT
    DT --> EULER_INT
    EULER_INT --> QNORM
    QNORM --> ROLL & PITCH & YAW
    QNORM -->|"stored for<br/>next iteration"| Q_PREV
```

### 7.3 PID Controller Comparison

```mermaid
flowchart TD
    subgraph ANGLE1 ["controlANGLE()"]
        direction TB
        A1_IN["angle error =<br/>desired_angle - IMU_angle"]
        A1_P["P = Kp * error"]
        A1_I["I = integral(error * dt)"]
        A1_D["D = GyroX/Y<br/>(measurement, NOT error)"]
        A1_OUT["0.01 * (P + I - D)"]
        A1_IN --> A1_P & A1_I
        A1_D --> A1_OUT
        A1_P & A1_I --> A1_OUT
    end

    subgraph ANGLE2 ["controlANGLE2()"]
        direction TB
        A2_OL["OUTER: PI on angle error<br/>rate_cmd = Kl*(Kp*e + Ki*I)"]
        A2_LP["LP filter + constrain<br/>+/-240 deg/s"]
        A2_IL["INNER: PID on rate error<br/>(rate_cmd - Gyro)"]
        A2_OUT["0.01 * (P + I + D)"]
        A2_OL --> A2_LP --> A2_IL --> A2_OUT
    end

    subgraph RATE ["controlRATE()"]
        direction TB
        R_IN["rate error =<br/>desired_rate - Gyro"]
        R_P["P = Kp * error"]
        R_I["I = integral(error * dt)"]
        R_D["D = d(error)/dt"]
        R_OUT["0.01 * (P + I + D)"]
        R_IN --> R_P & R_I & R_D
        R_P & R_I & R_D --> R_OUT
    end
```

### 7.4 Command Scaling Pipeline

```mermaid
flowchart LR
    subgraph MIXER ["Mixer (Quad Example)"]
        T["thro_des<br/>[0, 1]"]
        R["roll_PID<br/>~[-1, 1]"]
        P["pitch_PID<br/>~[-1, 1]"]
        Y["yaw_PID<br/>~[-1, 1]"]
        M1["m1 = T - P + R + Y"]
        M2["m2 = T - P - R - Y"]
        M3["m3 = T + P - R + Y"]
        M4["m4 = T + P + R - Y"]
        T & R & P & Y --> M1 & M2 & M3 & M4
    end

    subgraph MOTOR_SCALE ["Motor Scaling"]
        MS["* 125 + 125"]
        MC["constrain<br/>[125, 250] us"]
        MS --> MC
    end

    subgraph SERVO_SCALE ["Servo Scaling"]
        SS["* 180"]
        SC["constrain<br/>[0, 180] deg"]
        SS --> SC
    end

    M1 & M2 & M3 & M4 --> MS
    MC --> OS125["OneShot125<br/>GPIO Pulse"]
    SC --> SLIB["PWMServo<br/>Library"]
```

---

## Appendix A: Constants Reference

| Constant | Value | Meaning |
|----------|-------|---------|
| `0.0174533` | pi / 180 | Degrees to radians |
| `57.29577951` | 180 / pi | Radians to degrees |
| `0x5f3759df` | Quake magic number | Fast invSqrt initial guess (unused) |
| `16384.0` | 2^14 | Accel scale for +/-2G |
| `131.0` | 32768 / 250 | Gyro scale for +/-250 DPS |
| `6.0` | MPU9250 mag constant | Magnetometer raw-to-uT divisor |
| `0.04` | B_madgwick | Madgwick filter gain |
| `0.14` | B_accel | Accelerometer LP coefficient |
| `0.1` | B_gyro | Gyroscope LP coefficient |
| `1.0` | B_mag | Magnetometer LP coefficient (no filtering) |
| `0.7` | b (radio) | Radio command LP coefficient |
| `25.0` | i_limit | Integrator saturation limit |
| `30.0` | Kl | Cascaded controller loop gain |
| `240.0` | Rate limit | Cascaded controller rate constraint (deg/sec) |
| `0.9` | B_loop_roll/pitch | Cascaded controller damping filter |
| `2000` | Loop rate | Target control loop frequency (Hz) |
| `12000` | Calibration samples | Number of IMU samples for error calibration |

## Appendix B: Variable Flow Summary

| Stage | Input | Output | Function |
|-------|-------|--------|----------|
| IMU Read | I2C/SPI registers | AcX..MgZ (int16) | `getIMUdata()` |
| Scale | Raw int16 | AccX..MagZ (float, physical units) | `getIMUdata()` |
| Calibrate | Scaled values | Error-corrected values | `getIMUdata()` |
| LP Filter | Corrected values | Smoothed AccX..MagZ | `getIMUdata()` |
| Attitude | AccX,GyroX,MagX,dt | roll/pitch/yaw_IMU (degrees) | `Madgwick()` / `Madgwick6DOF()` |
| Radio | PWM/SBUS/DSM | channel_X_pwm (1000-2000) | `getCommands()` |
| Radio Filter | Raw channel PWM | Filtered channel PWM | `getCommands()` |
| Desired State | channel_X_pwm | thro/roll/pitch/yaw_des | `getDesState()` |
| PID | desired + IMU + Gyro | roll/pitch/yaw_PID (~[-1,1]) | `controlANGLE/2/RATE()` |
| Mix | PID + throttle | mX/sX_command_scaled | `controlMixer()` |
| Scale | Scaled commands | mX_PWM (125-250), sX_PWM (0-180) | `scaleCommands()` |
| Safety | PWM commands | Overridden if disarmed | `throttleCut()` |
| Output | Final PWM | GPIO pulses / Servo signals | `commandMotors()` / `servo.write()` |
