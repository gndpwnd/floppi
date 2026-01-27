# Sensor Data Acquisition and Processing Pipeline

This document describes the complete sensor data pipeline in the dRehmFlight flight controller, from raw hardware readings through calibration and filtering to processed attitude estimates.

---

## 1. Hardware Overview

### IMU Options

**MPU6050 (6-DOF)**
- 3-axis accelerometer + 3-axis gyroscope
- Interface: I2C, overclocked to 1 MHz (spec is 400 kHz)
- No magnetometer; attitude estimation uses 6-DOF Madgwick variant

**MPU9250 (9-DOF)**
- 3-axis accelerometer + 3-axis gyroscope + 3-axis magnetometer
- Interface: SPI
- Magnetometer (AK8963) is internally connected via auxiliary I2C inside the MPU9250 package
- Enables full 9-DOF Madgwick fusion with heading reference

### Axis Convention

The system uses the **NWU (North-West-Up)** body frame convention:

- **X**: forward (nose direction)
- **Y**: left (port wing)
- **Z**: up

Note the axis remapping that occurs in the Madgwick filter call to align the physical sensor axes with the NWU convention:

```cpp
Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
```

The negations and axis swaps correct for differences between the IMU chip's native axis orientation and the desired NWU body frame. The gyroscope Y and Z axes are negated, the accelerometer X axis is negated, and the magnetometer X and Y axes are swapped (with X negated) to produce a consistent right-hand coordinate system.

---

## 2. IMU Initialization (`IMUinit`)

### MPU6050 Initialization Sequence

1. `Wire.begin()` -- start I2C bus
2. `Wire.setClock(1000000)` -- overclock I2C to 1 MHz for faster sensor reads (the MPU6050 spec rates at 400 kHz, but most boards tolerate 1 MHz)
3. Initialize the MPU6050 device
4. Test connection; if failed, enter an infinite loop (hard fault -- the vehicle cannot fly without an IMU)
5. Set gyroscope full-scale range (e.g., +/-250, +/-500, +/-1000, or +/-2000 deg/s)
6. Set accelerometer full-scale range (e.g., +/-2G, +/-4G, +/-8G, or +/-16G)

### MPU9250 Initialization Sequence

1. `SPI.begin()` -- start SPI bus
2. Set accelerometer full-scale range
3. Set gyroscope full-scale range
4. Set magnetometer calibration values (hard-iron and soft-iron)
5. Set sample rate divider to 0:
   - Gyroscope and accelerometer output at **1 kHz**
   - Magnetometer output at **100 Hz**

### Error Handling

Both initialization paths check the connection to the sensor. On failure, the controller enters an infinite `while(1)` loop, preventing any flight operations. This is intentional: there is no safe degraded mode without an IMU.

---

## 3. Sensor Scale Factors

The IMU outputs raw 16-bit signed integers. These must be divided by a scale factor determined by the configured full-scale range to produce engineering units.

### Gyroscope Scale Factors

| Full-Scale Range | Scale Factor (LSB per deg/s) | Output Unit |
|------------------|------------------------------|-------------|
| +/-250 deg/s     | 131.0                        | deg/s       |
| +/-500 deg/s     | 65.5                         | deg/s       |
| +/-1000 deg/s    | 32.8                         | deg/s       |
| +/-2000 deg/s    | 16.4                         | deg/s       |

### Accelerometer Scale Factors

| Full-Scale Range | Scale Factor (LSB per G) | Output Unit |
|------------------|--------------------------|-------------|
| +/-2 G           | 16384.0                  | G           |
| +/-4 G           | 8192.0                   | G           |
| +/-8 G           | 4096.0                   | G           |
| +/-16 G          | 2048.0                   | G           |

### Magnetometer Scale Factor

| Conversion            | Output Unit |
|-----------------------|-------------|
| raw / 6.0             | microtesla  |

---

## 4. `getIMUdata()` -- Complete Pipeline

This function is called once per control loop iteration (nominally at 2 kHz). It performs the full sequence from raw sensor read to filtered, calibrated engineering values.

### Step-by-Step

1. **Read raw data**: Read six 16-bit signed integers (three gyro axes, three accel axes) from the IMU. For the MPU9250, also read three 16-bit magnetometer values.

2. **Scale to engineering units**: Divide raw values by the appropriate scale factor:
   - `AccX = raw_acc_x / ACCEL_SCALE_FACTOR` (result in G)
   - `GyroX = raw_gyro_x / GYRO_SCALE_FACTOR` (result in deg/s)
   - `MagX = raw_mag_x / 6.0` (result in microtesla)

3. **Subtract calibration errors**: Remove bias offsets determined during calibration:
   - `AccX = AccX - AccErrorX`
   - `GyroX = GyroX - GyroErrorX`
   - `MagX = (MagX - MagErrorX) * MagScaleX` (hard-iron then soft-iron)

4. **Apply first-order low-pass filter**: Smooth the signal to reject high-frequency noise:
   - `AccX = (1.0 - B_accel) * AccX_prev + B_accel * AccX`
   - `GyroX = (1.0 - B_gyro) * GyroX_prev + B_gyro * GyroX`
   - `MagX = (1.0 - B_mag) * MagX_prev + B_mag * MagX` (B_mag = 1.0, so no filtering)

5. **Store previous values**: Save the filtered output for use as `_prev` on the next iteration.

---

## 5. Low-Pass Filter Deep Dive

### Transfer Function

The filter is a first-order IIR (infinite impulse response) low-pass filter, also known as an exponential moving average:

```
y[n] = (1 - B) * y[n-1] + B * x[n]
```

This is algebraically equivalent to:

```
y[n] = y[n-1] + B * (x[n] - y[n-1])
```

The second form is sometimes called the "leaky integrator" form and makes the behavior intuitive: the output moves a fraction `B` of the way from its previous value toward the new input each sample.

### Filter Coefficient `B`

- `B = 0`: output is stuck at its initial value (infinite smoothing, no new data gets through)
- `B = 1`: output equals the input exactly (no filtering, full passthrough)
- `0 < B < 1`: low-pass filtering; smaller values of `B` produce heavier smoothing

### Relationship to Cutoff Frequency

The -3 dB cutoff frequency of this filter is:

```
fc = -fs / (2 * pi) * ln(1 - B)
```

For small values of `B`, this simplifies to the approximation:

```
fc ~= B * fs / (2 * pi)
```

where `fs` is the sampling frequency.

### Configured Values at 2 kHz Loop Rate

| Signal       | B coefficient | Approx. cutoff frequency | Notes                            |
|--------------|---------------|--------------------------|----------------------------------|
| Accelerometer| 0.14          | ~45 Hz                   | Moderate filtering; removes vibration while preserving tilt info |
| Gyroscope    | 0.1           | ~32 Hz                   | Heavier filtering; gyro noise is more problematic for integration |
| Magnetometer | 1.0           | N/A (passthrough)        | No filtering; mag already at 100 Hz sample rate |
| Radio (ch1-4)| 0.7           | ~223 Hz                  | Light filtering; preserves pilot responsiveness |

### Design Tradeoff

Lower `B` values produce cleaner signals but introduce more phase lag. In a flight controller, excessive lag on the gyroscope signal degrades stability margins and can cause oscillations. The chosen values represent a balance between noise rejection and control responsiveness.

---

## 6. IMU Calibration (`calculate_IMU_error`)

This function determines the bias (zero-offset) error for each accelerometer and gyroscope axis.

### Procedure

1. The vehicle must be placed on a **level surface** and remain **completely stationary**.
2. The function reads **12,000 samples** from the IMU.
3. For each axis, it computes the **mean** of all samples.
4. **Gyroscope biases**: The mean of each gyro axis at rest should be zero. Any nonzero mean is the bias error.
   - `GyroErrorX = mean(GyroX_samples)`
   - `GyroErrorY = mean(GyroY_samples)`
   - `GyroErrorZ = mean(GyroZ_samples)`
5. **Accelerometer biases**: At rest and level, AccX and AccY should read 0 G, and AccZ should read 1.0 G (due to gravity).
   - `AccErrorX = mean(AccX_samples)`
   - `AccErrorY = mean(AccY_samples)`
   - `AccErrorZ = mean(AccZ_samples) - 1.0`

The subtraction of 1.0 from the Z-axis mean accounts for the gravity vector. Without this correction, the calibration would try to zero out the gravity reading, producing incorrect tilt estimates.

### Usage

This is a **one-time calibration procedure**. The computed values are printed to the serial monitor. The user copies these values into the source code as constants. They are then subtracted from every subsequent sensor reading in `getIMUdata()`.

---

## 7. Magnetometer Calibration

Magnetometer calibration corrects for two classes of distortion caused by nearby ferromagnetic materials and current-carrying wires on the vehicle.

### Hard-Iron Correction

Hard-iron effects produce a constant bias offset on each axis. These shift the center of the measured magnetic sphere away from the origin.

```
MagX_corrected = MagX_raw - MagErrorX
MagY_corrected = MagY_raw - MagErrorY
MagZ_corrected = MagZ_raw - MagErrorZ
```

### Soft-Iron Correction

Soft-iron effects distort the magnetic sphere into an ellipsoid. Scale factors on each axis re-normalize the ellipsoid back to a sphere.

```
MagX_final = MagX_corrected * MagScaleX
MagY_final = MagY_corrected * MagScaleY
MagZ_final = MagZ_corrected * MagScaleZ
```

### `calibrateMagnetometer()` Function

1. The user rotates the IMU slowly in all directions (figure-eight pattern or full sphere coverage).
2. The library collects min/max values on each axis and computes the bias (center) and scale (axis ratios) automatically.
3. The resulting `MagError` and `MagScale` values are set during `IMUinit()`.

### Location Dependence

The Earth's magnetic field varies by geographic location (declination, inclination, and intensity all differ). Soft-iron distortions from the vehicle airframe are fixed, but the interaction with the local field direction means calibration may need to be repeated when flying at a significantly different location.

---

## 8. Attitude Estimation Warm-Up (`calibrateAttitude`)

Before entering the main control loop, the Madgwick filter must converge from its arbitrary initial state to a valid attitude estimate.

### Procedure

1. Run **10,000 iterations** of the following at the nominal 2 kHz loop rate:
   - Call `getIMUdata()` to obtain fresh, calibrated, filtered sensor data
   - Call `Madgwick()` with the current sensor values and timestep `dt`
2. After 10,000 iterations (approximately 5 seconds), the filter's internal quaternion has converged to represent the true orientation.

### Assumptions

- The vehicle is **stationary** and approximately **level** during this warm-up period.
- The gyroscope biases have already been calibrated (either hardcoded or via `calculate_IMU_error()`).
- After warm-up, the Madgwick filter's roll and pitch estimates will be near zero (level), and yaw will reflect the magnetic heading (9-DOF) or will be arbitrary (6-DOF).

---

## 9. Radio Receiver Pipeline

dRehmFlight supports four radio receiver protocols. All produce normalized channel values in the range of approximately 1000 to 2000 microseconds.

### PWM (Pulse Width Modulation)

- Each channel uses a **dedicated pin** with a **hardware interrupt** attached.
- On rising edge: record `micros()` timestamp.
- On falling edge: compute pulse width as `micros() - rising_timestamp`.
- Result: pulse width in microseconds (typically 1000-2000 us).
- Most common protocol; one wire per channel.

### PPM (Pulse Position Modulation)

- **Single pin** carries all channels as a pulse train.
- A gap greater than **5000 us** between pulses indicates the start of a new frame.
- Each subsequent pulse-to-pulse interval encodes one channel value.
- Channels are decoded sequentially from the frame sync gap.

### SBUS (Serial Bus)

- Uses **Serial5** (inverted UART at 100 kbaud, 8E2 framing).
- Raw SBUS values are 11-bit integers (0-2047).
- Conversion to standard microsecond range:
  ```
  channel_us = raw_value * 0.615 + 895.0
  ```
  This maps the SBUS range into approximately 895-2154 us, centered near 1500 us.

### DSM (Digital Spectrum Modulation / Spektrum Satellite)

- Uses **Serial3** at **115000 baud**.
- Proprietary Spektrum satellite receiver protocol.
- Library handles frame parsing and channel extraction.

### Radio Low-Pass Filtering

Channels 1 through 4 (roll, pitch, throttle, yaw) are passed through the same first-order IIR low-pass filter used for IMU data, with coefficient `b = 0.7`:

```
channel1_filtered = (1.0 - 0.7) * channel1_prev + 0.7 * channel1_raw
```

At a 2 kHz loop rate, this gives a cutoff frequency of approximately 223 Hz. The filtering is light, preserving pilot stick responsiveness while removing any jitter or noise from the radio link.

---

## 10. Pipeline Diagrams

### Full Sensor Data Flow Pipeline

```mermaid
flowchart TD
    subgraph Hardware
        MPU6050["MPU6050 (I2C @ 1 MHz)"]
        MPU9250["MPU9250 (SPI)"]
    end

    subgraph RawRead["1. Raw Read"]
        RAW_ACC["Raw Accel\n16-bit signed int"]
        RAW_GYRO["Raw Gyro\n16-bit signed int"]
        RAW_MAG["Raw Mag\n16-bit signed int\n(MPU9250 only)"]
    end

    subgraph Scale["2. Scale to Engineering Units"]
        SCALE_ACC["AccX,Y,Z = raw / ACCEL_SCALE\n(output: G)"]
        SCALE_GYRO["GyroX,Y,Z = raw / GYRO_SCALE\n(output: deg/s)"]
        SCALE_MAG["MagX,Y,Z = raw / 6.0\n(output: uT)"]
    end

    subgraph Calibrate["3. Subtract Calibration Errors"]
        CAL_ACC["AccX -= AccErrorX\nAccY -= AccErrorY\nAccZ -= AccErrorZ"]
        CAL_GYRO["GyroX -= GyroErrorX\nGyroY -= GyroErrorY\nGyroZ -= GyroErrorZ"]
        CAL_MAG["MagX = (MagX - MagErrorX) * MagScaleX\nMagY = (MagY - MagErrorY) * MagScaleY\nMagZ = (MagZ - MagErrorZ) * MagScaleZ"]
    end

    subgraph Filter["4. Low-Pass Filter"]
        LPF_ACC["B_accel = 0.14\nfc ~ 45 Hz"]
        LPF_GYRO["B_gyro = 0.1\nfc ~ 32 Hz"]
        LPF_MAG["B_mag = 1.0\n(passthrough)"]
    end

    subgraph StorePrev["5. Store Previous Values"]
        PREV["AccX_prev, AccY_prev, AccZ_prev\nGyroX_prev, GyroY_prev, GyroZ_prev\nMagX_prev, MagY_prev, MagZ_prev"]
    end

    subgraph AxisRemap["6. Axis Remapping"]
        REMAP["Madgwick(\nGyroX, -GyroY, -GyroZ,\n-AccX, AccY, AccZ,\nMagY, -MagX, MagZ,\ndt)"]
    end

    subgraph Attitude["7. Attitude Output"]
        OUTPUT["roll_IMU (deg)\npitch_IMU (deg)\nyaw_IMU (deg)"]
    end

    MPU6050 --> RAW_ACC
    MPU6050 --> RAW_GYRO
    MPU9250 --> RAW_ACC
    MPU9250 --> RAW_GYRO
    MPU9250 --> RAW_MAG

    RAW_ACC --> SCALE_ACC
    RAW_GYRO --> SCALE_GYRO
    RAW_MAG --> SCALE_MAG

    SCALE_ACC --> CAL_ACC
    SCALE_GYRO --> CAL_GYRO
    SCALE_MAG --> CAL_MAG

    CAL_ACC --> LPF_ACC
    CAL_GYRO --> LPF_GYRO
    CAL_MAG --> LPF_MAG

    LPF_ACC --> PREV
    LPF_GYRO --> PREV
    LPF_MAG --> PREV

    LPF_ACC --> REMAP
    LPF_GYRO --> REMAP
    LPF_MAG --> REMAP

    REMAP --> OUTPUT
```

### Low-Pass Filter Signal Flow

```mermaid
flowchart LR
    subgraph Input
        XN["x[n]\n(current sample)"]
    end

    subgraph Filter["First-Order IIR Low-Pass Filter"]
        DIFF["Subtract"]
        MULT_B["Multiply by B"]
        ADD["Add"]
        DELAY["z^-1\n(unit delay)"]
    end

    subgraph Output
        YN["y[n]\n(filtered output)"]
    end

    XN --> DIFF
    DELAY -->|"y[n-1]"| DIFF
    DIFF -->|"x[n] - y[n-1]"| MULT_B
    MULT_B -->|"B * (x[n] - y[n-1])"| ADD
    DELAY -->|"y[n-1]"| ADD
    ADD --> YN
    YN --> DELAY
```

### Calibration Data Flow

```mermaid
flowchart TD
    subgraph OneTime["One-Time Calibration (on ground)"]
        LEVEL["Place vehicle level\nand stationary"]
        CALC["calculate_IMU_error()"]
        SAMPLES["Read 12,000 samples"]
        MEAN["Compute mean of each axis"]

        GYRO_ERR["GyroErrorX = mean(GyroX)\nGyroErrorY = mean(GyroY)\nGyroErrorZ = mean(GyroZ)"]
        ACC_ERR["AccErrorX = mean(AccX)\nAccErrorY = mean(AccY)\nAccErrorZ = mean(AccZ) - 1.0"]

        SERIAL["Print to Serial Monitor"]
        PASTE["User pastes values\ninto source code"]
    end

    subgraph MagCal["Magnetometer Calibration"]
        ROTATE["Rotate IMU in\nall directions"]
        MAGCALFN["calibrateMagnetometer()"]
        HARDIRON["MagErrorX, Y, Z\n(hard-iron bias)"]
        SOFTIRON["MagScaleX, Y, Z\n(soft-iron scale)"]
    end

    subgraph Runtime["Runtime Application (every loop)"]
        RAW["Raw sensor reading"]
        SUB_GYRO["GyroX -= GyroErrorX"]
        SUB_ACC["AccX -= AccErrorX"]
        SUB_MAG_HI["MagX -= MagErrorX"]
        MULT_MAG_SI["MagX *= MagScaleX"]
        CLEAN["Calibrated output"]
    end

    LEVEL --> CALC
    CALC --> SAMPLES
    SAMPLES --> MEAN
    MEAN --> GYRO_ERR
    MEAN --> ACC_ERR
    GYRO_ERR --> SERIAL
    ACC_ERR --> SERIAL
    SERIAL --> PASTE

    ROTATE --> MAGCALFN
    MAGCALFN --> HARDIRON
    MAGCALFN --> SOFTIRON

    PASTE --> SUB_GYRO
    PASTE --> SUB_ACC
    HARDIRON --> SUB_MAG_HI
    SOFTIRON --> MULT_MAG_SI

    RAW --> SUB_GYRO
    RAW --> SUB_ACC
    RAW --> SUB_MAG_HI
    SUB_MAG_HI --> MULT_MAG_SI

    SUB_GYRO --> CLEAN
    SUB_ACC --> CLEAN
    MULT_MAG_SI --> CLEAN
```

### Radio Receiver Pipeline

```mermaid
flowchart TD
    subgraph Transmitter
        TX["Pilot Transmitter\n(sticks + switches)"]
    end

    subgraph Receivers["Receiver Hardware"]
        PWM_RX["PWM Receiver\n(1 wire per channel)"]
        PPM_RX["PPM Receiver\n(single wire)"]
        SBUS_RX["SBUS Receiver\n(Serial5, inverted UART)"]
        DSM_RX["DSM Receiver\n(Serial3 @ 115000 baud)"]
    end

    subgraph PWM_Decode["PWM Decoding"]
        PWM_ISR["Hardware interrupt\nper channel pin"]
        PWM_RISE["Rising edge:\nrecord micros()"]
        PWM_FALL["Falling edge:\npulse_width = micros() - rise_time"]
        PWM_OUT["Channel value (us)"]
    end

    subgraph PPM_Decode["PPM Decoding"]
        PPM_ISR["Single pin interrupt"]
        PPM_GAP["Gap > 5000 us?\nYes: reset channel index\nNo: store pulse width"]
        PPM_OUT["Channel values (us)"]
    end

    subgraph SBUS_Decode["SBUS Decoding"]
        SBUS_READ["Read Serial5 frame\n(25 bytes, 8E2)"]
        SBUS_PARSE["Parse 11-bit\nchannel values"]
        SBUS_CONV["channel_us =\nraw * 0.615 + 895.0"]
        SBUS_OUT["Channel values (us)"]
    end

    subgraph DSM_Decode["DSM Decoding"]
        DSM_READ["Read Serial3 frame"]
        DSM_PARSE["Library parses\nchannel data"]
        DSM_OUT["Channel values (us)"]
    end

    subgraph PostProcess["Post-Processing"]
        LPF["Low-pass filter\nb = 0.7, fc ~ 223 Hz\n(channels 1-4 only)"]
        FAILSAFE["Failsafe check"]
        COMMANDS["roll_des, pitch_des,\nthrottle_des, yaw_des,\naux channels"]
    end

    TX --> PWM_RX
    TX --> PPM_RX
    TX --> SBUS_RX
    TX --> DSM_RX

    PWM_RX --> PWM_ISR
    PWM_ISR --> PWM_RISE
    PWM_RISE --> PWM_FALL
    PWM_FALL --> PWM_OUT

    PPM_RX --> PPM_ISR
    PPM_ISR --> PPM_GAP
    PPM_GAP --> PPM_OUT

    SBUS_RX --> SBUS_READ
    SBUS_READ --> SBUS_PARSE
    SBUS_PARSE --> SBUS_CONV
    SBUS_CONV --> SBUS_OUT

    DSM_RX --> DSM_READ
    DSM_READ --> DSM_PARSE
    DSM_PARSE --> DSM_OUT

    PWM_OUT --> LPF
    PPM_OUT --> LPF
    SBUS_OUT --> LPF
    DSM_OUT --> LPF

    LPF --> FAILSAFE
    FAILSAFE --> COMMANDS
```

### Axis Convention and Madgwick Remapping

```mermaid
flowchart TD
    subgraph PhysicalAxes["IMU Chip Physical Axes"]
        CHIP_X["Chip X"]
        CHIP_Y["Chip Y"]
        CHIP_Z["Chip Z"]
    end

    subgraph Remapping["Axis Remapping in Madgwick Call"]
        direction LR
        GYRO_REMAP["Gyro: (X, -Y, -Z)"]
        ACC_REMAP["Accel: (-X, Y, Z)"]
        MAG_REMAP["Mag: (Y, -X, Z)"]
    end

    subgraph BodyFrame["NWU Body Frame"]
        BODY_X["X: Forward (nose)"]
        BODY_Y["Y: Left (port wing)"]
        BODY_Z["Z: Up"]
    end

    subgraph AttitudeOutput["Madgwick Output"]
        ROLL["roll_IMU (deg)\nrotation about X"]
        PITCH["pitch_IMU (deg)\nrotation about Y"]
        YAW["yaw_IMU (deg)\nrotation about Z"]
    end

    CHIP_X --> GYRO_REMAP
    CHIP_Y --> GYRO_REMAP
    CHIP_Z --> GYRO_REMAP
    CHIP_X --> ACC_REMAP
    CHIP_Y --> ACC_REMAP
    CHIP_Z --> ACC_REMAP
    CHIP_X --> MAG_REMAP
    CHIP_Y --> MAG_REMAP
    CHIP_Z --> MAG_REMAP

    GYRO_REMAP --> BODY_X
    GYRO_REMAP --> BODY_Y
    GYRO_REMAP --> BODY_Z
    ACC_REMAP --> BODY_X
    ACC_REMAP --> BODY_Y
    ACC_REMAP --> BODY_Z
    MAG_REMAP --> BODY_X
    MAG_REMAP --> BODY_Y
    MAG_REMAP --> BODY_Z

    BODY_X --> ROLL
    BODY_Y --> PITCH
    BODY_Z --> YAW
```
