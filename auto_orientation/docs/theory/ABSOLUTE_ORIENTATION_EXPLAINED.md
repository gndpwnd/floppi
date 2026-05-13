# Absolute Orientation Explained

**Technical deep-dive into how absolute orientation works, why quaternions matter, and how calibration relates to sensor fusion confidence.**

- **Document Status**: v1.0 - Comprehensive technical reference  
- **Target Audience**: Engineers, researchers, developers who want to understand the theory  
- **Prerequisites**: Basic physics/mathematics background recommended  
- **Tested On**: BNO085 IMU, Arduino Mega, real-world deployments

---

## Table of Contents

1. [What Is Absolute Orientation?](#what-is-absolute-orientation)
2. [Quaternions: The Better Way to Represent Rotation](#quaternions-the-better-way-to-represent-rotation)
3. [The BNO085: Three Sensors in One](#the-bno085-three-sensors-in-one)
4. [How Sensor Fusion Works](#how-sensor-fusion-works)
5. [Magnetometer and Calibration](#magnetometer-and-calibration)
6. [Soft-Iron vs Hard-Iron Effects](#soft-iron-vs-hard-iron-effects)
7. [Local vs Global Magnetic Field](#local-vs-global-magnetic-field)
8. [Calibration Levels and Sensor Fusion Confidence](#calibration-levels-and-sensor-fusion-confidence)
9. [When Yaw Is Unreliable](#when-yaw-is-unreliable)
10. [Practical Applications](#practical-applications)

---

## What Is Absolute Orientation?

### Definition

**Absolute orientation** is the real-time measurement of an object's **rotational position** in 3D space relative to a fixed reference frame (Earth's gravity and magnetic field).

### Key Concepts

**Fixed Reference Frames:**
1. **Gravity vector** (down): Defined by Earth's gravity
2. **Magnetic field vector** (north): Defined by Earth's magnetic field
3. **Cross product**: Perpendicular to both (east direction)

**Three-Axis Rotation Description:**
- **Roll (Φ)**: Rotation around the front-back axis (aircraft wing dip)
- **Pitch (θ)**: Rotation around the side-to-side axis (aircraft nose up/down)
- **Yaw (ψ)**: Rotation around the vertical axis (compass heading/direction)

### Example: Aircraft Orientation

```
Pitch (up-down tilt):
  Pitch = +30° means nose 30° above horizon
  Pitch = -15° means nose 15° below horizon

Roll (wing dip):
  Roll = +45° means right wing 45° lower than left
  Roll = -30° means left wing 30° lower than right

Yaw (heading):
  Yaw = 0° means facing magnetic north
  Yaw = 90° means facing magnetic east
  Yaw = 180° means facing magnetic south
  Yaw = 270° means facing magnetic west
```

### Why "Absolute"?

The orientation is **absolute** because it's defined relative to Earth's fixed reference frame (gravity + magnetic field), not relative to some arbitrary starting orientation.

**Contrast**:
- **Absolute orientation**: "Device is facing 45° east of north" (fixed reference)
- **Relative orientation**: "Device rotated 45° from where it started" (arbitrary reference)

The BNO085 provides absolute orientation, which is more useful for real-world applications.

---

## Quaternions: The Better Way to Represent Rotation

### Why Not Euler Angles?

Three-axis rotation could be represented with **Euler angles (roll, pitch, yaw)**:

```
Simple representation:
{
  "roll": 12.5°,
  "pitch": -4.3°,
  "yaw": 247.8°
}
```

**But Euler angles have problems:**

1. **Gimbal Lock**: At certain angles (e.g., pitch = 90°), the system loses one degree of freedom
   ```
   When pitch = 90°, roll and yaw become equivalent
   (This is the gimbal lock singularity)
   ```

2. **Discontinuity**: Yaw wraps around at 360° → 0°
   ```
   Yaw: ...359.5° → 359.8° → 0.1° → 0.5°...
   (Discontinuous at boundary)
   ```

3. **Interpolation**: Blending between two Euler angles doesn't produce smooth intermediate rotations

4. **Multiple representations**: Same rotation can be represented multiple ways
   ```
   (roll=0°, pitch=0°, yaw=45°)
   (roll=180°, pitch=180°, yaw=225°)
   Both represent the same physical orientation!
   ```

### What Are Quaternions?

A **quaternion** is a 4-component mathematical object representing rotation:

```
q = (w, x, y, z)

Where:
  w = scalar component (cos(θ/2))
  x, y, z = vector components (axis * sin(θ/2))
  
Constraint: w² + x² + y² + z² = 1 (unit quaternion)

Example: 90° rotation around Z-axis
q = (0.707, 0, 0, 0.707)
```

### Why Quaternions Are Better

| Property | Euler Angles | Quaternions |
|---|---|---|
| **Gimbal Lock** | Yes (problem) | No (always 3 DOF) |
| **Discontinuity** | Yes (at 360°) | No (continuous) |
| **Interpolation** | Difficult, artifacts | Smooth, natural (SLERP) |
| **Composition** | Slow (matrix multiply) | Fast (quaternion multiply) |
| **Uniqueness** | Multiple per rotation | Two per rotation (±q) |
| **Computational** | 9 floats (matrix) | 4 floats (compact) |

### Practical Advantage: Sensor Fusion

The BNO085 uses quaternion-based sensor fusion algorithms (specifically, an optimized Kalman filter) that:
1. Smoothly blend accelerometer, gyroscope, and magnetometer data
2. Never encounter gimbal lock even at extreme angles
3. Provide smooth output even during rapid rotations

**Result**: Highly stable, artifact-free orientation output.

### Conversion to Euler Angles

If you need Euler angles (roll, pitch, yaw) for your application:

```
Given quaternion q = (w, x, y, z):

roll = atan2(2(wx + yz), 1 - 2(x² + y²))
pitch = asin(2(wy - xz))
yaw = atan2(2(wz + xy), 1 - 2(y² + z²))

Most code libraries provide this conversion automatically.
```

---

## The BNO085: Three Sensors in One

The BNO085 is a 9-axis IMU integrating three sensor types:

### 1. Accelerometer (3-axis)

**Measures**: Linear acceleration and gravitational field

```
Acceleration = Linear acceleration + Gravity acceleration
```

**Use in orientation**:
- Detects which way is "down" (gravity vector)
- Provides pitch and roll angles
- Cannot measure yaw (gravity is always downward)

**Output Range**: ±16 g typically  
**Update Rate**: 10-100+ Hz (configurable)

**Example data**:
```
ax = 0.0 m/s²    (no acceleration in x)
ay = 0.0 m/s²    (no acceleration in y)
az = 9.81 m/s²   (gravity pointing down)

Interpretation: Device is upright, not accelerating
```

### 2. Gyroscope (3-axis)

**Measures**: Rotational velocity (rate of change of orientation)

```
Gyro measures: droll/dt, dpitch/dt, dyaw/dt
```

**Use in orientation**:
- Detects rotation on all three axes
- Provides short-term (seconds) rotation tracking
- Drifts over time without other sensors

**Output Range**: ±2000°/sec typically  
**Update Rate**: 10-100+ Hz (configurable)

**Example data**:
```
gx = 0 °/sec     (not rolling)
gy = 45 °/sec    (pitching at 45°/sec = 1/8 rotation per second)
gz = 0 °/sec     (not yawing)

Interpretation: Device is pitching forward at steady rate
```

### 3. Magnetometer (3-axis)

**Measures**: Magnetic field strength and direction

```
Magnetic field = Earth's field + Local distortion
```

**Use in orientation**:
- Detects which way is north (Earth's magnetic field)
- Provides yaw (compass heading) angle
- Requires calibration to remove local distortions
- Cannot measure pitch or roll directly

**Output Range**: ±4912 μT typical  
**Update Rate**: 10-100+ Hz (configurable)

**Example data**:
```
mx = 15.3 μT    (magnetic field pointing mostly north)
my = 2.1 μT     (slight component toward east)
mz = 50.4 μT    (upward vertical component)

Interpretation: Magnetic north is ~8° east of device's x-axis
```

---

## How Sensor Fusion Works

### The Challenge: Individual Sensors Are Flawed

| Sensor | Strength | Weakness |
|---|---|---|
| **Accelerometer** | Measures gravity (pitch/roll) | Sensitive to vibration/acceleration |
| **Gyroscope** | Measures rotation accurately (short-term) | Drifts over time (integration error) |
| **Magnetometer** | Measures north direction (yaw) | Distorted by local metal/interference |

### The Solution: Sensor Fusion Algorithm

The BNO085 uses an **optimized Kalman filter** that combines all three sensors:

```
Orientation Estimate = f(accel, gyro, mag, calibration)

Algorithm:
1. Predict next orientation from gyroscope data
   next_orientation ≈ current_orientation + gyro_rate * dt

2. Measure orientation from accelerometer (gravity direction)
   measured_roll_pitch = atan2(ay, az), atan2(-ax, sqrt(ay²+az²))

3. Measure yaw from magnetometer (north direction)
   measured_yaw = atan2(my, mx)

4. Fuse predictions and measurements with Kalman filter
   If measurements agree with prediction: high confidence, trust measurement
   If measurements disagree: low confidence, trust prediction

5. Output weighted blend of all information
   high_confidence_result = mostly_measurements + some_gyro
   low_confidence_result = mostly_gyro_prediction + some_measurements
```

### Why This Matters

```
Timeline of sensor strengths:

Instant 0 (device just powered on):
  Accel: Good (gravity detected)
  Gyro: Good (initial state known)
  Mag: May be poor (uncalibrated)
  → Orientation estimate: Moderate confidence

1 second later:
  Accel: Good (continuous)
  Gyro: Good (no drift yet)
  Mag: Still poor (uncalibrated)
  → Orientation estimate: Moderate confidence (yaw uncertain)

10 seconds later:
  Accel: Good (continuous)
  Gyro: Degrading (integration drift ~1° per 10 sec)
  Mag: Still poor (uncalibrated)
  → Orientation estimate: Moderate confidence

After calibration:
  Accel: Good
  Gyro: Good (short-term)
  Mag: Excellent (calibrated)
  → Orientation estimate: HIGH confidence
```

### The Role of Calibration

**Calibration improves the Kalman filter's confidence in magnetometer data:**

```
Without Calibration (mag uncalibrated):
  Kalman filter sees: "Mag says north is 45° off"
  vs Accel+Gyro says: "Your orientation should be here"
  Conclusion: Mag data is unreliable, weight it low
  Result: Yaw drifts over seconds/minutes

With Calibration (mag = 3):
  Kalman filter sees: "Mag says north direction accurately"
  vs Accel+Gyro says: "Your orientation is here"
  Conclusion: All data is consistent and reliable
  Result: Yaw stays steady indefinitely
```

---

## Magnetometer and Calibration

### Earth's Magnetic Field

**Magnitude**: ~25-65 μT (microtesla) depending on latitude

**Direction**: Varies by location
- **Declination**: Angle between true north and magnetic north (varies by latitude and longitude)
- **Inclination**: Angle of field below (or above) horizontal (varies by latitude)

**Example**:
```
San Francisco, CA:
  Declination: ~13° east (magnetic north is 13° east of true north)
  Inclination: ~61° (magnetic field points 61° downward from horizontal)

London, UK:
  Declination: ~2° west
  Inclination: ~66°

Sydney, Australia:
  Declination: ~12° east
  Inclination: ~61°
```

### The Magnetometer Distortion Problem

The magnetometer **measures total magnetic field**:

```
Measured Field = Earth's Field + Local Distortion
              = True North Vector + Metal/Electronics Interference
```

**Local distortions from**:
- Metal objects (nearby bracket, frame, fasteners)
- Electronic devices (computer, router, power supply)
- Permanent magnets (motor, speaker)
- Iron structures (building frame, vehicle body)

**Effect on yaw measurement**:

```
Without Local Distortion:
  Measured field points north
  Calculated yaw = 0° (correct!)

With Local Distortion (e.g., metal bracket to the right):
  Earth's field points north (0°)
  Local distortion points east (+90°)
  Total measured field points northeast (~45°)
  Calculated yaw = 45° (WRONG! Off by 45°)
```

### What Calibration Does

**Calibration measures the distortion and removes it:**

```
Raw Measurement = Earth's Field + Distortion

Calibration Step:
  1. Rotate sensor in all orientations
  2. Measure raw field from all angles
  3. Fit an ellipsoid to the measurements (accounts for both magnitude and direction)
  4. Calculate distortion vector: D = average of all "off-center" measurements
  5. Store D in sensor memory

Correction (applied automatically):
  Corrected Field = Raw Measurement - D
  Corrected Field ≈ Earth's Field (clean!)
  Calculated yaw is now accurate
```

**Calibration result**:
```
Before:  yaw = 45° (distorted by local metal)
After:   yaw = 2° (actual direction, ±2° error from measurement noise)
```

### Why mag = 3 Matters

The BNO085 tracks calibration quality:

```
mag = 0: No calibration data
  → Raw distortion unmeasured
  → Yaw can be off by ±30-90°

mag = 1: Partial calibration
  → Distortion partially measured (limited angle coverage)
  → Yaw off by ±15-30°

mag = 2: Good calibration
  → Distortion well-measured (most angles covered)
  → Yaw off by ±5-10°

mag = 3: Excellent calibration
  → Distortion well-measured from all angles
  → Yaw off by ±2-3° (near theoretical limit)
```

---

## Soft-Iron vs Hard-Iron Effects

The magnetometer distortion has two components:

### Hard-Iron Effects

**Source**: Permanent magnets and ferromagnetic materials (always magnetic)

**Cause**: Permanent magnets near sensor, or iron objects that become magnetized

**Effect**: Constant offset in measured field

```
Raw measurement always shifted by same vector, regardless of device orientation

Graphically (looking down from above):
True field points north (at origin)

With hard-iron distortion (metal bracket always to the right):
  Measurements cluster around a point displaced to the RIGHT
  Instead of centered at origin, centered at (+30μT, 0)
  
Result:
  All yaw measurements are offset by same amount
  Yaw is consistently off by ~20°, but at least stable
```

**Magnetometer calibration procedure**:
```
Rotate device in figure-8 to measure field from all angles
Map out the offset vector
Store offset vector in sensor memory
On every measurement, subtract offset automatically
Result: Hard-iron distortion removed
```

### Soft-Iron Effects

**Source**: Ferromagnetic materials that become magnetized only in presence of Earth's field

**Cause**: Iron/steel structures, ferrite cores, electrical components

**Effect**: Stretches/rotates the measured field (magnitude and direction vary with orientation)

```
Raw measurement is stretched and rotated, depends on device orientation

Graphically:
True field is circular (all angles equally likely)

With soft-iron distortion (asymmetric metal structure):
  Measurements form an ellipse (stretched in one direction)
  N-S measurements compressed, E-W measurements stretched
  Field appears elongated
  
Result:
  Yaw accuracy varies depending on which direction device points
  E-W heading more accurate than N-S heading (or vice versa)
  Some angles lose 5-10° accuracy
```

**Calibration procedure**:
```
Rotate device in figure-8 from all orientations
Map out the ellipse (not just offset)
Calculate stretch/rotation matrix
Store correction matrix in sensor memory
On every measurement, apply matrix correction
Result: Soft-iron distortion removed
```

### Combined Effect

In reality, **both hard-iron and soft-iron effects are present**:

```
Measured Field = Soft-Iron-Rotated(Earth's Field) + Hard-Iron-Offset
```

**BNO085 calibration removes both**:

1. **Figure-8 motion** from all orientations provides enough data points
2. **Kalman filter** fits an ellipsoid to all measurements
3. **Calibration algorithm** solves for both offset and stretch matrix
4. **Result**: High-quality calibration that works in all orientations

---

## Local vs Global Magnetic Field

### Global Magnetic Field

**Definition**: Earth's magnetic field that exists everywhere

**Characteristics**:
- Same direction and strength across a ~100km region
- Changes slowly over time (declination varies ~0.1° per year)
- Different from location to location

**Declination by Region**:
```
North America: 0-20° east
Europe: 5° west to 5° east
Asia: varies widely
Australia: 5-15° east
South America: 0-20° west

(Varies with exact latitude/longitude and changes over time)
```

**Practical implications**:
- Calibrate at location where you'll use the sensor for maximum accuracy
- Calibration persists if you move within ~100km of calibration location
- If moving 1000+ km, consider recalibration for best yaw accuracy

### Local Magnetic Anomalies

**Definition**: Temporary distortions caused by nearby metal/electronics

**Sources**:
- Metal objects (bracket, frame, fasteners)
- Permanent magnets (motors, speakers)
- Ferromagnetic materials (iron shielding, steel structures)
- Electronic devices (computers, routers, power supplies)
- Building materials (rebar in concrete, metal beams)

**Lifetime**:
- Permanent: Metal bracket mounted on sensor (always there)
- Temporary: Phone or laptop next to sensor (goes away when moved)

**Effect on calibration**:
```
Calibrate near laptop:
  Calibration includes distortion from laptop
  When away from laptop: yaw is slightly off
  When back near laptop: yaw is accurate again
  
Permanent mount near sensor:
  Calibration includes distortion from mount
  Yaw is always accurate (calibrated for this geometry)
  If you remove the mount: yaw becomes less accurate
```

**Practical approach**:
1. **Identify permanent mount geometry** (bracket, shielding, structure)
2. **Calibrate with final hardware assembly** (includes permanent components)
3. **Don't calibrate in different location** (temporary interference)
4. **If geometry changes**, recalibrate to account for new distortion

---

## Calibration Levels and Sensor Fusion Confidence

### What Calibration Levels Measure

The BNO085 reports four calibration metrics:

```
system:  Overall calibration quality (composite score)
accel:   Accelerometer quality
gyro:    Gyroscope quality
mag:     Magnetometer quality (this is what calibration improves)
```

**Only mag is directly affected by calibration procedure**  
(accel and gyro auto-calibrate during normal operation)

### Relationship Between Calibration Level and Fusion Confidence

The Kalman filter uses calibration status to determine **how much to trust each sensor**:

```
Sensor Fusion Weights (conceptual):

At mag = 0 (Uncalibrated):
  Accel weight:  40%  (trust it for pitch/roll)
  Gyro weight:   50%  (trust it for short-term rotation)
  Mag weight:    10%  (barely trust it for yaw)
  Result: Yaw drifts and is unreliable

At mag = 1 (Low):
  Accel weight:  35%  (trust it for pitch/roll)
  Gyro weight:   50%  (trust it for short-term rotation)
  Mag weight:    15%  (somewhat trust it for yaw)
  Result: Yaw drifts slowly over seconds

At mag = 2 (Medium):
  Accel weight:  30%  (trust it for pitch/roll)
  Gyro weight:   40%  (trust it for short-term rotation)
  Mag weight:    30%  (trust it for yaw)
  Result: Yaw stable for minutes, then slow drift

At mag = 3 (High):
  Accel weight:  25%  (trust it for pitch/roll)
  Gyro weight:   25%  (trust it for short-term rotation)
  Mag weight:    50%  (trust it heavily for yaw)
  Result: Yaw stable indefinitely
```

### Impact on Different Angles

**Pitch and Roll** (from accelerometer):
- Remain accurate regardless of mag calibration
- Measure gravity direction, not affected by magnetometer
- Stable even with mag = 0

**Yaw** (from magnetometer):
- Highly dependent on mag calibration
- mag = 0: Unreliable, drifts
- mag = 1: Biased, slow drift
- mag = 2: Good short-term accuracy (minutes)
- mag = 3: Excellent long-term accuracy (indefinite)

### Long-Term Drift Analysis

**Over extended operation**:

```
With mag = 0:
  Hour 0:  Yaw = 0° (starting position)
  Hour 1:  Yaw = 8° (drifted 8° due to gyro integration error)
  Hour 2:  Yaw = 18° (drifted 18° total)
  Hour 4:  Yaw = 35° (significant drift)
  Problem: Yaw becomes unusable

With mag = 2:
  Hour 0:  Yaw = 0° (starting position)
  Hour 1:  Yaw = 1° (minimal drift, magnetometer constrains it)
  Hour 2:  Yaw = 2° (still constrained by mag)
  Hour 4:  Yaw = 4° (slow drift)
  Hour 24: Yaw = 50° (long-term drift accumulates)
  
With mag = 3:
  Hour 0:  Yaw = 0° (starting position)
  Hour 1:  Yaw = 0.5° (minimal drift)
  Hour 2:  Yaw = 0.8° (still constrained)
  Hour 4:  Yaw = 1.2° (excellent constraint)
  Hour 24: Yaw = 3° (minimal long-term drift)
```

### Practical Implication

```
Mission duration:    Required mag level:
< 10 minutes         mag >= 1 (okay)
< 1 hour            mag >= 2 (good)
< 8 hours           mag = 3 (excellent)
> 24 hours          mag = 3 + periodic recalibration
```

---

## When Yaw Is Unreliable

### Scenario 1: Magnetometer Not Calibrated (mag = 0-1)

**Symptom**: Yaw drifts or jumps randomly

```
Monitor output shows:
Yaw: 45.2°
Yaw: 47.1°
Yaw: 42.8°  ← Jumped down
Yaw: 58.3°  ← Jumped up significantly
```

**Why**: Without calibration, local distortion is unmeasured. Kalman filter doesn't trust magnetometer, relies on gyroscope, which drifts.

**Solution**: Run calibration procedure to reach mag >= 2

---

### Scenario 2: Large Magnetic Interference

**Symptom**: Yaw is offset by large amount (e.g., always 30° off) but stable

```
Actual direction:  North (0°)
Monitor shows:     Yaw = 30°
Compass app shows: North (0°)
Difference:        30° offset

Also try:          East (90°)
Monitor shows:     Yaw = 120°
Compass app shows: East (90°)
Difference:        30° offset (consistent)
```

**Why**: Large permanent magnetic distortion (metal bracket, motor) nearby

**Solution**:
1. Calibrate with the metal object in place (future measurements will be accurate with it present)
2. Or move away from source of interference

---

### Scenario 3: Rapid Temperature Change

**Symptom**: Yaw jumps when temperature changes suddenly

```
Temperature: 20°C, Yaw: 45.2° (stable)
[Move into hot car interior]
Temperature: 35°C, Yaw: 45.2° (no change immediately)
[After 30 seconds of heating]
Temperature: 40°C, Yaw: 48.1° ← Jumped!
```

**Why**: Magnetometer sensitivity changes with temperature. BNO085 has temperature compensation, but large rapid changes can cause temporary offset.

**Solution**:
- Allow 30 seconds thermal stabilization after temperature change
- Or recalibrate if yaw drifts persistently after temperature change

---

### Scenario 4: Vibration or Mechanical Stress

**Symptom**: Yaw becomes noisy or drifts during vibration

```
Normal operation:     Yaw: 45.2° ± 0.1°  (stable)
During vibration:     Yaw: 45.2° ± 2.5°  (noisy)
After vibration:      Yaw: 45.2° ± 0.1°  (back to normal)

OR:

After mechanical stress, yaw might have offset:
Before:  Yaw = 45.0°
After:   Yaw = 45.5° (shifted by 0.5°)
```

**Why**: Vibration causes gyroscope drift and can temporarily affect accelerometer accuracy

**Solution**:
- Reduce vibration source if possible
- Allow sensor to settle after mechanical stress
- If persistent offset, recalibrate

---

### Scenario 5: Outdoor Use with Variable Magnetic Anomalies

**Symptom**: Yaw drifts as device moves (e.g., next to power lines, vehicle)

```
Position A (open field):    Yaw = 0°   (accurate)
Position B (near power line): Yaw = -8° (offset due to EM field)
Position C (next to car):     Yaw = +15° (offset due to car body metal)
Position D (back to field):   Yaw = 2° (back to normal)
```

**Why**: Calibration was done in one location. Moving to locations with different magnetic anomalies causes yaw bias.

**Solution**:
- Recalibrate at the location where you'll operate
- Or account for known magnetic anomalies (e.g., power line offset) in your application

---

## Practical Applications

### Application 1: Leveling a Camera Gimbal

**Requirements**:
- Roll accurate to ±2°
- Pitch accurate to ±2°
- Yaw not critical (no compass reference needed)

**Minimum calibration**: mag = 1 (acceptable)  
**Recommended**: mag = 2 (good)  
**Reason**: Roll and pitch don't need magnetometer calibration (from accelerometer). Yaw is not used.

---

### Application 2: Navigation/Heading Reference

**Requirements**:
- Yaw accurate to ±5°
- Must work for hours without recalibration
- Heading must remain stable even at rest

**Minimum calibration**: mag = 2 (marginal)  
**Recommended**: mag = 3 (excellent)  
**Reason**: Magnetometer quality directly impacts yaw accuracy. mag = 3 ensures stable long-term heading.

---

### Application 3: 3D Reconstruction / Pose Estimation

**Requirements**:
- Roll accurate to ±3°
- Pitch accurate to ±3°
- Yaw accurate to ±5° (for alignment with GPS/map)
- Must work for 8+ hours in field

**Minimum calibration**: mag = 2 (acceptable)  
**Recommended**: mag = 3 (excellent)  
**Reason**: All three angles matter. mag = 3 ensures gyro and magnetometer work together for stable long-term accuracy.

---

### Application 4: Autonomous Vehicle Heading Control

**Requirements**:
- Yaw accurate to ±2°
- Must respond to magnetic field in real-time
- Outdoor deployment in variable terrain (forests, cities, near roads)

**Minimum calibration**: mag = 3 (required)  
**Recommended**: Recalibrate at deployment location  
**Reason**: Autonomous vehicles need heading accuracy better than ±5°. Different environments have different magnetic anomalies. Recalibrating at deployment site ensures best accuracy for that specific location.

---

## Summary: When Absolute Orientation Is Reliable

### Reliable Conditions

✓ Accelerometer and gyroscope working (pitch/roll are always reliable if hardware is sound)  
✓ Magnetometer calibrated (mag >= 2 for short missions, mag = 3 for long missions)  
✓ Stable temperature environment (allow 30 sec warmup after temperature change)  
✓ No large vibration or mechanical stress  
✓ Away from large moving magnetic sources (vehicles, electromagnetic fields)  
✓ Operating within 100km of calibration location (for best yaw accuracy)

### Unreliable Conditions

✗ Magnetometer not calibrated (mag = 0-1)  
✗ Large permanent magnetic interference nearby (large metal structure)  
✗ In active electromagnetic field (high-power RF transmitter)  
✗ On or in metallic vehicle (without recalibration for that vehicle)  
✗ Immediately after rapid temperature change (needs 30 sec to settle)  
✗ During or immediately after mechanical shock or vibration  
✗ Operating 1000+ km away from calibration location with different magnetic declination

---

## Technical Reference: The Quaternion Update Equations

**For those interested in the math:**

The BNO085's Kalman filter updates quaternions using:

```
q(t+dt) ≈ q(t) + 0.5 * q(t) * ω(t) * dt

Where:
  q(t) = current quaternion estimate
  ω(t) = angular velocity vector from gyroscope
  dt = time step
  * = quaternion multiplication

After each update, quaternion is re-normalized to maintain unit magnitude:
  q_normalized = q / ||q||
```

This approach ensures:
- Smooth interpolation between measurement updates
- No gimbal lock at extreme angles
- Efficient computation (4 multiply-add operations)

---

## FAQ: Understanding Absolute Orientation

**Q: Why can't pitch and roll drift like yaw does?**  
A: Pitch and roll are measured directly from gravity (accelerometer), which is always present. Yaw is measured from magnetometer, which requires calibration. Gravity is a constant reference; magnetic field can be distorted.

**Q: If I calibrate indoors, will it work outdoors?**  
A: Yes, for short-term use. Earth's magnetic field is the same everywhere (locally). However, if you're more than 100km away and in a very different environment (e.g., mountains vs plains), recalibration may improve accuracy.

**Q: Why doesn't the BNO085 auto-calibrate magnetometer like it does for accel/gyro?**  
A: Magnetometer auto-calibration would require the device to be rotated in all directions continuously. That's not practical during normal use. User-performed calibration (figure-8 motion) is the efficient solution.

**Q: What's the theoretical limit of yaw accuracy?**  
A: About ±1-2° with perfect calibration, assuming no environmental magnetic noise. Real-world deployments see ±2-5° accuracy depending on location.

**Q: Can I calibrate once and use it everywhere on Earth?**  
A: For short missions (< 1 hour), yes—Earth's global field is consistent. For long missions or high-precision work, recalibrate at your deployment location for best accuracy.

---

**Last Updated**: 2026-05  
**Version**: 1.0  
**Target Audience**: Engineers, researchers, developers  
**Tested on**: BNO085 IMU sensor, Arduino Mega 2560
