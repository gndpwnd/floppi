# Calibration Guide — Hardware Requirements & Test Sequencing

> A step-by-step guide for setting up new hardware. Add components one at a time, test each, and build confidence before flying. Each step tells you what hardware you need, what calibration to run, and what values you get.

## Philosophy

- **Incremental setup**: Don't connect everything at once. Add one component, test it, move to the next.
- **No guessing**: Every hardware-dependent value in config.h has an auto-calibration routine. Run it, copy the output, flash.
- **Feature-aware**: Some calibrations only apply when certain feature flags are enabled. The guide tells you which.
- **Repeatable**: Use `tools/calibration_reset.py` to reset all calibration values back to defaults when starting fresh with new hardware.
- **Command source agnostic**: "Receiver" in this guide means whatever command source you configured in config.h — RC radio (SBUS/DSM/PPM/PWM), serial from a flight computer (`USE_SERIAL_COMMANDS`), I2C (`USE_I2C_COMMANDS`), or WiFi API (`USE_API_SERVER`). RadioComm handles all sources identically. Radio calibration (Stage 2) only applies to RC receivers — serial/I2C/WiFi sources skip it.

## Calibration Output

All calibration routines output detailed information to the **serial monitor** (USB serial). This is the primary calibration interface — connect via PlatformIO Serial Monitor, Arduino IDE, or any terminal at 115200 baud.

**Serial monitor** shows:

- Step-by-step instructions ("Place board level and hold still...")
- Real-time progress (sample counts, intermediate values)
- Quality validation results (pass/fail, retry prompts)
- Final `#define` values ready to copy-paste into config.h

**OLED display** (if connected) shows:

- Current calibration state ("Calibrating IMU...", "Radio cal...", "Done!")
- Key summary values (not the full `#define` output)
- Pass/fail status

You do NOT need an OLED to calibrate — serial is sufficient. The OLED is a convenience for users who don't have a laptop nearby during calibration.

## Quick Reference — Calibration Commands

| Command | Routine | Hardware Required | Feature Tier | Output Values |
|---------|---------|-------------------|--------------|---------------|
| `i` | IMU offset calibration | MCU + IMU | Base (always) | `IMU_ACC_ERROR_X/Y/Z`, `IMU_GYRO_ERROR_X/Y/Z` |
| `m` | 6-position IMU calibration | MCU + IMU | Base (always) | `IMU_ACC_ERROR_X/Y/Z`, `IMU_ACC_SCALE_X/Y/Z`, `IMU_GYRO_ERROR_X/Y/Z` |
| `o` | IMU orientation detection | MCU + IMU | Base (always) | Axis transformation code |
| `r` | Radio channel mapping | MCU + receiver + transmitter | Base (always) | `THROTTLE/ROLL/PITCH/YAW/AUX1/AUX2_CHANNEL` |
| `f` | Failsafe auto-detection | MCU + receiver + transmitter | Base (always) | `FAILSAFE_THROTTLE/ROLL/PITCH/YAW/AUX1/AUX2` |
| `g` | PID gain tuning | Full drone (motors + props) | Base (always) | `KP/KI/KD_ROLL/PITCH/YAW_RATE` or `_ANGLE` |
| `p` | Filter/limits tuning | Full drone (motors + props) | Base (always) | `B_ACCEL`, `B_GYRO`, `B_DTERM`, `MADGWICK_BETA`, max rates/angles |
| `e` | ESC endpoint calibration | MCU + ESCs (NO props!) | Base (always) | ESC min/max PWM range |
| (sphere) | Magnetometer calibration | MCU + MPU9250 | Base (MPU9250 only) | `MAG_ERROR_X/Y/Z`, `MAG_SCALE_X/Y/Z` |

---

## Setup Stages

### Stage 0: Fresh Start (Software Only)

**Hardware needed**: Just your computer.

**Steps**:
1. Clone the repo, install PlatformIO
2. Edit `config.h`:
   - Select your IMU: `USE_MPU6050` or `USE_MPU9250`
   - Select your receiver protocol: `USE_SBUS_RECEIVER`, `USE_DSM_RECEIVER`, etc.
   - Select your control mode: `USE_ANGLE_CONTROLLER` (recommended) or `USE_RATE_CONTROLLER`
   - Select your display: `DISPLAY_SSD1306_128X32`, etc. (if using OLED)
   - Optionally override pin definitions (see "Pin Configuration" section below)
3. If resetting from a previous drone, run: `python3 tools/calibration_reset.py`
4. Build the calibration firmware to verify compilation:
   ```
   pio run -e teensy40_calibration    # or esp32_calibration, etc.
   ```

**Result**: Firmware compiles. All calibration values are at defaults (zeros/ones). Ready for hardware.

---

### Stage 1: MCU + IMU Only (No Receiver, No Motors)

**Hardware needed**: Microcontroller + IMU (MPU6050 or MPU9250), USB cable.

**Wiring**: Connect IMU to I2C pins (see wiring docs for your platform).

**What you can test**:
- IMU is responding over I2C
- Accelerometer reads ~1g on the correct axis
- Gyroscope reads ~0 when stationary

**Calibrations to run** (in order):

#### 1a. IMU Offset Calibration (`i`)
- **What it does**: Place on flat surface, hold still for ~5 seconds. Measures gyro drift and accelerometer offset.
- **Physical action**: Keep the board perfectly still on a flat surface.
- **Output**: Copy `IMU_ACC_ERROR_X/Y/Z` and `IMU_GYRO_ERROR_X/Y/Z` to config.h.

#### 1b. 6-Position IMU Calibration (`m`) — Optional but Recommended
- **What it does**: More accurate version of `i`. Measures offsets AND scale factors from 6 orientations.
- **Physical action**: Place board in 6 positions (level, upside-down, nose-up, nose-down, left-up, right-up). Hold still in each for ~5 seconds when prompted.
- **Output**: Copy `IMU_ACC_ERROR_X/Y/Z`, `IMU_ACC_SCALE_X/Y/Z`, and `IMU_GYRO_ERROR_X/Y/Z` to config.h.
- **Note**: Replaces values from `i` — run `m` instead of `i` if you want the most accurate calibration.

#### 1c. IMU Orientation Detection (`o`)
- **What it does**: Detects how the IMU is mounted (which way is up, which way is forward).
- **Physical action**: Place in 3 positions when prompted: level, nose-up 90deg, right-side-up 90deg.
- **Output**: Axis transformation code. Copy to `imu.cpp` if your IMU is mounted in a non-standard orientation.
- **Note**: Only needed if the IMU is NOT mounted flat with the chip facing up and the X-axis pointing forward. Skip if standard mounting.

#### 1d. Magnetometer Calibration (MPU9250 only)
- **What it does**: Sphere calibration for hard-iron and soft-iron compensation.
- **Physical action**: Slowly rotate the board in all orientations for 30 seconds (make a sphere in the air).
- **Output**: Copy `MAG_ERROR_X/Y/Z` and `MAG_SCALE_X/Y/Z` to config.h.
- **Note**: Only available with `USE_MPU9250`. Skip if using MPU6050.

**After Stage 1**: Flash the calibration build with your new values, verify IMU readings look correct (attitude should track orientation). You now have a calibrated IMU.

---

### Stage 2: MCU + IMU + Command Source (No Motors)

**Hardware needed**: Add your command source to Stage 1 setup. This depends on what you configured in config.h:

| Command Source           | Hardware Needed                    | Config Flag                     |
| ------------------------ | ---------------------------------- | ------------------------------- |
| RC radio (SBUS)          | SBUS receiver + transmitter        | `USE_SBUS_RECEIVER`             |
| RC radio (DSM)           | DSM/DSMX receiver + transmitter    | `USE_DSM_RECEIVER`              |
| RC radio (PPM)           | PPM receiver + transmitter         | `USE_PPM_RECEIVER`              |
| RC radio (PWM)           | PWM receiver + transmitter         | `USE_PWM_RECEIVER`              |
| Serial (flight computer) | UART connection to flight computer | `USE_SERIAL_COMMANDS` (planned) |
| I2C (flight computer)    | I2C connection to flight computer  | `USE_I2C_COMMANDS` (planned)    |
| WiFi API (ESP32)         | WiFi network + API server          | `USE_API_SERVER` (planned)      |

**Wiring**: Connect your command source to the appropriate pins (see wiring docs for your platform). All pins are configurable in config.h.

**What you can test**:

- Command source is communicating with the MCU
- All channels respond to input (stick movements, serial commands, etc.)
- Channel values are in 1000-2000us range

**Calibrations to run** (in order):

#### 2a. Radio Channel Mapping (`r`) — RC receivers only

- **What it does**: Auto-detects which receiver channel maps to which control axis.
- **Physical action**: Move sticks one at a time when prompted (throttle up, roll right, pitch forward, yaw right, flip AUX switches).
- **Output**: Copy `THROTTLE_CHANNEL`, `ROLL_CHANNEL`, etc. to config.h.
- **Note**: Run this even if you think you know the mapping. It validates and avoids surprises.
- **Skip if**: Using serial, I2C, or WiFi commands — the flight computer already sends channels in the correct order.

#### 2b. Failsafe Auto-Detection (`f`) — RC receivers only

- **What it does**: Records what the receiver sends when the transmitter is ON vs OFF. Detects failsafe values.
- **Physical action**: Leave transmitter ON, then turn it OFF when prompted. Wait for measurement.
- **Output**: Copy `FAILSAFE_THROTTLE/ROLL/PITCH/YAW/AUX1/AUX2` to config.h.
- **Note**: Critical for safety. Failsafe values are what the FC falls back to when signal is lost.
- **Skip if**: Using serial, I2C, or WiFi commands — failsafe is handled by command source timeout (RadioComm arbitration).

**After Stage 2**: You have calibrated IMU + command source. The FC can read orientation and control inputs. Ready for motor testing.

---

### Stage 3: MCU + IMU + Command Source + ESCs (NO PROPS!)

**Hardware needed**: Add ESCs + motors to Stage 2 setup. **REMOVE ALL PROPELLERS.**

**Wiring**: Connect ESC signal wires to motor pins. Common ground required. Power ESCs from battery.

**Safety**: Triple-check that no propellers are attached. ESC calibration sends full-throttle signals.

**Calibrations to run**:

#### 3a. ESC Endpoint Calibration (`e`)
- **What it does**: Standard ESC range calibration (send max → power on → send min).
- **Physical action**: Follow prompts. Disconnect battery, send command, reconnect battery when told, wait for beeps.
- **Output**: ESCs learn your controller's PWM range. No config.h values — ESCs store internally.
- **Note**: Only needed once per ESC set. Some ESCs come pre-calibrated.

#### 3b. Verify Motor Spin Direction
- **What it does**: Manual check. Arm the FC (throttle low + AUX switch), gently increase throttle, verify each motor spins the correct direction for your frame type.
- **Note**: No auto-calibration for this. Swap any two motor wires to reverse direction, or use ESC firmware.

**After Stage 3**: ESCs are calibrated, motors spin correctly. Ready for PID tuning.

---

### Stage 4: Full Drone (With Props, Tethered)

**Hardware needed**: Complete drone with props attached. **Tether the drone** or use a test stand.

**Calibrations to run**:

#### 4a. PID Gain Tuning (`g`)
- **What it does**: Runtime PID adjustment while the drone is running.
- **Physical action**: Arm, hover (tethered), adjust gains via serial commands until stable.
- **Commands**: `g` (show current), `g kp_roll 0.2` (set value).
- **Output**: Copy final `KP/KI/KD_ROLL/PITCH/YAW` values to config.h.
- **Note**: Start with defaults. Increase P until oscillation, then back off 20%. Add D to dampen. Add I for steady-state.

#### 4b. Filter/Limits Tuning (`p`) — Optional
- **What it does**: Adjusts filter coefficients and max angle/rate limits at runtime.
- **Physical action**: Fly (tethered), adjust filters until vibration/noise is acceptable.
- **Commands**: `p` (show current), `p b_gyro 0.15` (set value).
- **Output**: Copy tuned values to config.h.
- **Note**: Only tune if the defaults aren't working well. Common reason: noisy motors/props need more filtering.

**After Stage 4**: Drone flies stably with tuned PID gains. Flash the live (non-calibration) build and fly freely.

---

## Feature Tier — Additional Calibrations

Some calibrations only apply when specific feature flags are enabled in config.h.

### Base Tier (Always Active)
All calibrations above apply to the base tier. No extra steps needed.

### USE_OPTIMIZATION Tier
Enable: `#define USE_OPTIMIZATION` in config.h.

**Additional parameters to tune** (via `p` command or manually in config.h):

| Parameter | Default | Description | When to Change |
|-----------|---------|-------------|----------------|
| `GYRO_LPF_CUTOFF_HZ` | 100 | Biquad gyro low-pass cutoff | Lower if gyro is noisy (budget motors) |
| `DTERM_LPF_CUTOFF_HZ` | 80 | Biquad D-term low-pass cutoff | Lower if motors oscillate |
| `GYRO_NOTCH_CENTER_HZ` | 0 (disabled) | Notch filter center frequency | Set to motor/prop resonance frequency |
| `GYRO_NOTCH_WIDTH_HZ` | 30 | Notch filter bandwidth | Wider catches more noise, narrower is more precise |
| `B_ACCEL_STAGE2` | 0.05 | Extra accel smoothing | Lower for heavy vibration |

**How to find notch frequency**: Use fc_tool FFT analysis or Betaflight blackbox viewer to identify the peak noise frequency from motors/props. Set `GYRO_NOTCH_CENTER_HZ` to that frequency.

### USE_RACING Tier
Enable: `#define USE_RACING` in config.h.

**Additional parameters to tune** (via manual flight testing):

| Parameter | Default | Description | When to Change |
|-----------|---------|-------------|----------------|
| `FF_ROLL/PITCH/YAW` | 0.0 | Feed-forward gains | Increase for snappier stick response |
| `TPA_BREAKPOINT` | 0.65 | Throttle PID attenuation start | Adjust if oscillation at high throttle |
| `TPA_RATE` | 0.5 | TPA strength | Increase if oscillation at high throttle |
| `SETPOINT_SMOOTH_CUTOFF_HZ` | 0 (disabled) | Stick input smoothing | Enable for smoother camera footage |
| `EXPO_ROLL/PITCH/YAW` | 0.0 | Stick expo curves | 0.3-0.5 for gentle center, aggressive extremes |
| `USE_AIRMODE` | disabled | Full PID at zero throttle | Enable for flips, rolls, inverted flight |

**Note**: Racing parameters are tuned by feel during aggressive flight. Start with defaults, enable one at a time.

---

## Pin Configuration

All pin assignments can be overridden in config.h. The default values come from pin_definitions.h (Teensy) or pin_definitions_esp32.h (ESP32/S3). To override any pin, add a `#define` in config.h BEFORE the pin_definitions.h include:

```c
// Example: Override motor pins for custom wiring
#define MOTOR_PIN_1 10
#define MOTOR_PIN_2 11
#define MOTOR_PIN_3 12
#define MOTOR_PIN_4 13
```

See config.h "PIN OVERRIDES" section for all overridable pins.

---

## Starting Fresh — Calibration Reset

When moving to new hardware (different drone, different IMU, etc.), reset all calibration values:

```bash
python3 tools/calibration_reset.py
```

This resets all calibration `#define` values in config.h back to factory defaults (zeros for offsets, 1.0 for scale factors, standard channel mapping, default failsafe values). Feature flags and non-calibration settings are preserved.

**When to reset**:
- Moving firmware to a different drone
- Replacing IMU sensor
- Replacing receiver
- Starting from scratch after a bad calibration
- Sharing your config.h with someone else (strip your hardware-specific values)

---

## Calibration Workflow Summary

```text
Stage 0: Software setup, config.h basics, verify compilation
  │
Stage 1: MCU + IMU → calibrate IMU (i/m/o) + magnetometer (MPU9250)
  │                    Hardware: just MCU board + IMU breakout
  │
Stage 2: + Command source → calibrate radio (r) + failsafe (f) [RC only]
  │                    Hardware: add receiver/serial/I2C/WiFi (per config.h)
  │
Stage 3: + ESCs/Motors → calibrate ESCs (e), verify spin direction
  │                    Hardware: add ESCs + motors (NO PROPS!)
  │
Stage 4: + Props (tethered) → tune PID (g) + filters (p)
  │                    Hardware: full drone, tethered
  │
Done: Flash live build → fly freely
```

Each stage builds on the previous. You can always go back and re-run any calibration.
