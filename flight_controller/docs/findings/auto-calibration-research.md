# Flight Controller Research Report: Calibration, State Machines, Auto-Detection, PID Tuning, and Teensy Considerations

**Date:** 2026-02-05
**Project:** floppi (Teensy-based dRehmFlight flight controller)
**Purpose:** Comprehensive research to inform development of auto-calibration, state management, auto-detection, and PID auto-tuning features.

---

## Table of Contents

1. [IMU Auto-Calibration Approaches](#1-imu-auto-calibration-approaches)
2. [Flight Controller Firmware State Machines](#2-flight-controller-firmware-state-machines)
3. [Auto-Detection Features in Flight Controllers](#3-auto-detection-features-in-flight-controllers)
4. [PID Auto-Tuning for Quadcopters](#4-pid-auto-tuning-for-quadcopters)
5. [Teensy-Specific Considerations](#5-teensy-specific-considerations)
6. [Gap Analysis: Current floppi Implementation](#6-gap-analysis-current-floppi-implementation)
7. [Recommended Action Items](#7-recommended-action-items)

---

## 1. IMU Auto-Calibration Approaches

### 1.1 Gyroscope Bias Calibration at Startup

#### How Betaflight Does It

Betaflight performs automatic gyroscope calibration every time the flight controller boots. The implementation lives in `src/main/sensors/gyro.c` and follows this approach:

- **Sample count:** Betaflight collects approximately 1024 samples (configurable, but 1024 is the default `gyroCalibrationDuration`).
- **Stationary detection:** Before and during calibration, Betaflight monitors the variance of gyro readings. If any reading exceeds a "movement threshold" (typically 48 raw LSBs, roughly 0.37 deg/s at +/-2000 dps), the calibration counter resets and starts over. This is the key insight: **calibration automatically restarts if the board is moved during startup.**
- **Bias calculation:** The simple arithmetic mean of all accepted samples on each axis becomes the gyro bias offset.
- **Convergence check:** Calibration is considered valid only if all samples were collected without triggering the movement detector. The system does not proceed to arm-ready state until gyro calibration passes.
- **Time to complete:** At 3.2 kHz gyro sampling, 1024 samples takes roughly 320ms of stationary time. With the reset-on-movement behavior, actual startup time depends on when the user stops touching the board.
- **LED indicator:** The status LED blinks in a specific pattern during gyro calibration and changes to steady when calibration succeeds.

#### How ArduPilot Does It

ArduPilot (in `libraries/AP_InertialSensor/AP_InertialSensor.cpp`) uses a more sophisticated approach:

- **Sample count:** 2048 samples per axis (configurable via `INS_GYR_CAL` parameter).
- **Stationary detection:** Uses accelerometer readings simultaneously. If accel variance exceeds a threshold, gyro calibration is aborted. This dual-sensor check is more robust than gyro-only variance checking.
- **Convergence criteria:** After collecting all samples, ArduPilot computes both the mean (bias) and the standard deviation. If the standard deviation exceeds a limit (~0.1 rad/s or ~5.7 deg/s), the calibration is rejected and the user is notified.
- **Multi-IMU support:** ArduPilot calibrates all connected IMUs (up to 3) simultaneously, comparing results between them for sanity checking.
- **Warm-up delay:** ArduPilot imposes a minimum 2-second delay after power-on before beginning gyro calibration, allowing the sensor oscillator to stabilize.
- **Storage:** Gyro offsets are stored persistently in parameters (equivalent to EEPROM). On subsequent boots, if the user has `INS_GYR_CAL = 0`, stored offsets are used instead of recalibrating. The default is to always recalibrate at boot.

#### How INAV Does It

INAV (fork of Cleanflight/Betaflight) follows a nearly identical approach to Betaflight but with some additions:

- **Default sample count:** 512 samples.
- **Temperature recording:** INAV records the gyro temperature at the time of calibration (on sensors that have an on-die temperature sensor, like MPU6050/ICM-20689). This is stored alongside the bias offsets, and INAV warns if the current temperature is significantly different from the calibration temperature.
- **Sensor health checking:** INAV performs a "gyro health" check during calibration that rejects obviously faulty readings (stuck at zero, stuck at max, all axes identical).

#### How Cleanflight Does It

Cleanflight (the original ancestor) uses the simplest approach:

- **Sample count:** 1000 samples.
- **No movement detection:** Original Cleanflight does NOT detect movement during calibration -- it simply averages 1000 samples. This makes it more susceptible to errors if the board is moved during boot.
- **Instructions to user:** The documentation tells users to keep the board still during the first 5 seconds after power-on.

#### Key Takeaways for floppi

1. **Always calibrate gyro at startup** -- this is universal across all projects. Gyro bias drifts with temperature and between power cycles, so stored values alone are insufficient.
2. **Implement movement detection** using either gyro variance (Betaflight approach) or accel variance (ArduPilot approach). The accel-based approach is arguably more robust since accelerometer readings are less noisy than gyro.
3. **Reset-on-movement** is strongly preferred over blind averaging. The floppi `calculate_IMU_error()` function currently uses 2000 samples with no movement detection.
4. **Minimum warm-up delay** of 1-2 seconds after power-on allows the sensor oscillator to stabilize before calibration begins.
5. **1024-2048 samples** is the sweet spot -- enough for statistical significance, fast enough for reasonable startup time.

#### Relevance to floppi

The current floppi implementation (`calculate_IMU_error()` in main.cpp) collects 2000 samples with a 1ms delay between each (2 seconds total), with no movement detection. This is roughly comparable to Cleanflight's basic approach but lacks the robustness of Betaflight's movement-reset behavior. The sample count is appropriate, but the lack of stationary verification is a significant gap.

---

### 1.2 Accelerometer Calibration

#### Single-Position Calibration

All four projects support single-position (level) calibration:

- **Betaflight:** `accCalibrationCycles` collects 400 samples on a level surface. The mean of X and Y is stored as the offset. The Z-axis offset is computed as `mean(Z) - 1.0g`. This is fast (~200ms) and sufficient for most use cases. The calibration is triggered by a CLI command (`calibrate acc`) or configurator button.

- **ArduPilot:** The "simple accelerometer calibration" (`accelcal_simple`) works similarly -- place level, collect samples, compute offsets assuming gravity points down. This is the "one-click" calibration in Mission Planner.

- **INAV:** Same as Betaflight. Triggered via Configurator or `calibrate acc` CLI command.

- **Cleanflight:** Same as Betaflight.

**Limitations of single-position calibration:**
- Only computes bias offsets, not scale factors. If a sensor reads 0.97g instead of 1.0g on an axis, single-position calibration cannot correct this.
- Assumes the board is truly level. A 1-degree tilt during calibration creates a 0.017g offset error, which translates to approximately 1 degree of persistent attitude error.
- Does not account for cross-axis sensitivity.

#### Multi-Position Calibration (6-Point)

All projects support an optional 6-position calibration:

- **ArduPilot (full calibration):** The standard accelerometer calibration in ArduPilot is a 6-position procedure:
  1. Level (Z up)
  2. Level inverted (Z down)
  3. Nose up (X up, 90 degrees pitch)
  4. Nose down (X down)
  5. Left side up (Y up, 90 degrees roll)
  6. Right side up (Y down)

  For each position, ArduPilot collects samples and records the mean reading. It then solves a system of equations for 6 unknowns: 3 bias offsets and 3 scale factors. The solver uses an iterative Gauss-Newton method to find the parameters that make all 6 gravity readings consistent with a 1g sphere. This produces significantly more accurate calibration than single-position.

- **Betaflight (6-point calibration):** Available in the CLI as a separate calibration mode. Less commonly used than the simple level calibration because Betaflight primarily relies on gyro for attitude (the accel is mainly used as a long-term reference in the attitude estimator).

- **INAV:** Supports 6-position calibration and RECOMMENDS it because INAV uses the accelerometer more heavily (for position estimation, altitude hold, etc.).

#### Academic/Best Practice Approach

The gold standard for accelerometer calibration in MEMS IMUs is the "6-position tumble test" combined with least-squares ellipsoid fitting:

1. Collect data at 6 orientations (each principal axis pointing up and down).
2. The ideal sensor would produce readings that lie on a sphere of radius 1g centered at the origin.
3. Fit an ellipsoid to the 6 mean readings using least-squares.
4. The ellipsoid center gives the 3 bias offsets.
5. The ellipsoid semi-axes give the 3 scale factors.
6. The ellipsoid orientation gives the 3 cross-axis coupling terms (misalignment).

This 9-parameter model is the most complete calibration possible without specialized equipment. ArduPilot's implementation is a simplified version that solves for 6 of these 9 parameters.

#### Key Takeaways for floppi

1. **Single-position calibration is the minimum** and should be available as a quick option. The current floppi implementation does this correctly.
2. **6-position calibration provides dramatically better accuracy** and should be a future enhancement. It corrects scale factor errors that single-position cannot.
3. **The floppi `calibrateIMUWithOrientation()` function** already implements a 3-position test (level, nose-up, right-up) for orientation detection. Extending this to a full 6-position calibration would be straightforward.
4. **For a startup gyro calibration, always do it automatically.** For accelerometer calibration, it should be a user-triggered procedure (not automatic at every boot) because it requires specific physical orientations.

---

### 1.3 Calibration Data Storage

#### Betaflight

- **Storage mechanism:** Uses the STM32's flash memory via a parameter system called "pg" (parameter groups). Parameters are stored in a dedicated flash sector.
- **Data stored:** Gyro offsets (3 floats), accelerometer offsets (3 floats), accelerometer scale (3 floats), magnetometer offsets and scales (6 floats).
- **Validity checking:** Each parameter group has a CRC checksum. If the CRC fails on boot, defaults are loaded.
- **CLI access:** `get gyro_offset_x`, `set acc_calibration`, etc.
- **When updated:** Gyro offsets are NOT stored to flash (they are always recalculated at boot). Accelerometer and magnetometer calibration values ARE stored and persist across power cycles.

#### ArduPilot

- **Storage mechanism:** Uses a "parameter" system backed by EEPROM (or flash emulated as EEPROM on STM32). Parameters are identified by name strings (e.g., `INS_ACCOFFS_X`).
- **Data stored:** Gyro offsets (3 floats per IMU, up to 3 IMUs), accel offsets (3 floats per IMU), accel scales (3 floats per IMU), temperature calibration coefficients (if enabled).
- **Validity checking:** Parameter system uses a metadata table with type, range, and default information. Invalid values are rejected and defaults are used.
- **Backup/restore:** ArduPilot supports parameter file export/import, allowing calibration data to be backed up and restored.

#### INAV

- Same approach as Betaflight (flash-based parameter storage with CRC).

#### Cleanflight

- Same approach as Betaflight (EEPROM/flash parameter storage).

#### Teensy EEPROM Considerations

This is where floppi's situation differs significantly from STM32-based flight controllers:

- **Teensy 4.0/4.1:** Does NOT have true EEPROM. It uses **flash-based EEPROM emulation** (the `EEPROM.h` library). The emulated EEPROM is 1080 bytes on Teensy 4.0 and 4284 bytes on Teensy 4.1. Each write wears the flash slightly, but the wear-leveling implementation means typical use patterns are acceptable (PJRC estimates 100,000+ write cycles).
- **Teensy 3.6:** Has 4096 bytes of real EEPROM that does not wear out as quickly.
- **Write endurance concern:** For calibration data that is written infrequently (only when the user runs a calibration procedure), flash wear is a non-issue. Do NOT continuously write calibration data in the main loop.

#### Key Takeaways for floppi

1. **The current floppi approach (hard-coded `#define` values in `config.h`) is the dRehmFlight approach** and is the simplest but most user-unfriendly method. Every calibration requires editing source code and reflashing.
2. **Moving to EEPROM storage is strongly recommended.** The Teensy 4.x EEPROM emulation provides 1080-4284 bytes, which is more than sufficient for calibration data (approximately 48 bytes for a full IMU calibration).
3. **Implement a simple data format:**
   - Magic number / version byte (2 bytes)
   - Gyro bias X, Y, Z (3 x 4 bytes = 12 bytes)
   - Accel bias X, Y, Z (3 x 4 bytes = 12 bytes)
   - Accel scale X, Y, Z (3 x 4 bytes = 12 bytes)
   - CRC16 checksum (2 bytes)
   - Total: 40 bytes
4. **Keep the hard-coded defaults as fallback.** If EEPROM data is invalid (bad CRC, first boot), fall back to the `config.h` values.
5. **Separate gyro calibration (automatic, every boot) from accel calibration (user-triggered, stored persistently).**

---

### 1.4 Temperature Compensation for IMU Drift

#### The Problem

MEMS gyroscopes and accelerometers exhibit significant bias drift with temperature. A typical MPU6050 gyro bias temperature coefficient is approximately 0.02 deg/s per degree Celsius. Over a 30-degree C temperature range (cold outdoor startup to warm in-flight), this means up to 0.6 deg/s of uncorrected gyro bias shift. This drift causes the attitude estimator to gradually accumulate heading error (yaw drift) and, to a lesser extent, roll/pitch error.

#### How ArduPilot Handles Temperature Compensation

ArduPilot has the most sophisticated temperature compensation system of any open-source flight controller:

- **Temperature calibration process:** The user places the flight controller in a freezer (or outdoors in cold weather) and then brings it inside. During the warm-up period, ArduPilot continuously logs IMU readings alongside the on-die temperature sensor reading.
- **Polynomial fitting:** After collecting data across a wide temperature range (ideally 30+ degrees C), ArduPilot fits a 3rd-order polynomial to each axis's bias vs. temperature curve:
  ```
  bias(T) = a0 + a1*T + a2*T^2 + a3*T^3
  ```
  This produces 4 coefficients per axis, 12 coefficients total for gyro, and 12 for accelerometer.
- **Runtime correction:** At each loop iteration, ArduPilot reads the current temperature from the IMU's on-die temperature sensor and evaluates the polynomial to get the temperature-corrected bias offset. This is subtracted from the raw reading BEFORE the Madgwick/EKF filter processes it.
- **Parameters:** The polynomial coefficients are stored as parameters (`INS_TCAL1_GYR1_X`, etc.). There are 72 parameters total for a full 3-IMU temperature calibration.
- **Effectiveness:** ArduPilot developers report that temperature calibration reduces gyro drift by a factor of 10x or more, making it especially important for long-duration flights or flights in varying temperature conditions.

#### How Betaflight Handles It

Betaflight does NOT have temperature compensation. The Betaflight philosophy is:

1. Gyro is recalibrated at every boot, so the starting bias is correct for the current temperature.
2. Flights are typically short (3-5 minutes for FPV racing), so temperature does not change dramatically during a flight.
3. The gyro is used primarily for rate control, where bias drift appears as a slow yaw drift that the pilot can easily correct.
4. The Madgwick/Mahony filter continuously corrects for gyro drift using accelerometer references (for roll/pitch) and optionally magnetometer (for yaw).

This is an acceptable tradeoff for Betaflight's use case (short, aggressive FPV flights) but is NOT acceptable for longer flights or autonomous operation.

#### How INAV Handles It

INAV does not have explicit temperature calibration but addresses the issue through:

1. More aggressive complementary filter corrections from accelerometer and magnetometer.
2. A barometer-based altitude estimator that does not depend on accelerometer accuracy.
3. GPS-based heading corrections that override gyro-integrated yaw.

#### Academic Approaches

Academic papers on IMU temperature compensation generally recommend:

1. **Factory calibration curves:** If the manufacturer provides temperature calibration data (Invensense/TDK does for some industrial-grade IMUs), use it.
2. **In-situ calibration:** If factory data is unavailable (which is the case for MPU6050 and MPU9250), perform the ArduPilot-style temperature sweep calibration.
3. **Online estimation:** Use an Extended Kalman Filter (EKF) that explicitly models gyro bias as a state variable, allowing it to be estimated and corrected in real-time. This is what ArduPilot's EKF3 does -- it estimates a 3-axis gyro bias alongside the attitude, velocity, and position states.
4. **Oven-based calibration:** For highest accuracy, perform calibration at multiple controlled temperature points in a temperature chamber.

#### Key Takeaways for floppi

1. **For Phase 1 (initial flights), temperature compensation is NOT critical.** Gyro calibration at startup handles the boot-time temperature, and short flights (~5 minutes) will not see dramatic temperature changes.
2. **For Phase 2+ (longer flights, autonomous operation), temperature compensation becomes important.** The ArduPilot polynomial approach is the gold standard.
3. **The MPU6050 and MPU9250 both have on-die temperature sensors** (accessible via register reads), so the hardware support is already present.
4. **A pragmatic interim approach:** At startup, read the temperature alongside the gyro bias. Store both. During flight, if the temperature has changed by more than 10 degrees C from the calibration temperature, warn the user (or trigger a mid-flight recalibration if stationary).
5. **Long-term goal:** Implement ArduPilot-style polynomial temperature calibration as a calibration program option.

---

## 2. Flight Controller Firmware State Machines

### 2.1 Betaflight's Approach

Betaflight uses a layered state machine architecture:

#### System States (top level)

```
SYSTEM_STATE_INITIALISING    -> Hardware init, sensor detection
SYSTEM_STATE_CONFIG_MODE     -> Connected to Betaflight Configurator (USB)
SYSTEM_STATE_READY           -> Calibrated, waiting for arm
SYSTEM_STATE_ARMED           -> Motors active, flight control running
SYSTEM_STATE_FAILSAFE        -> Radio link lost
SYSTEM_STATE_CALIBRATING     -> Running sensor calibration
```

#### Configurator Mode vs Flight Mode

This is one of Betaflight's most elegant design decisions:

- **Detection:** When Betaflight detects a USB connection to the Configurator application (via MSP protocol handshake), it enters `CONFIG_MODE`.
- **In CONFIG_MODE:**
  - The main flight control loop continues to run (IMU reads, attitude estimation, etc.) but motor output is DISABLED.
  - The MSP (MultiWii Serial Protocol) handler processes commands from the Configurator.
  - The user can read/write parameters, trigger calibrations, view real-time sensor data, and configure flight modes.
  - Arming is blocked (even if the radio commands would normally arm the controller).
- **Exiting CONFIG_MODE:**
  - Disconnecting USB automatically exits config mode.
  - Betaflight can also be configured to NOT auto-enter config mode when USB is connected (for in-flight logging via USB).
- **Safety implication:** This design ensures that motors cannot accidentally spin while the user is handling the aircraft near a computer.

#### Arming State Machine

Betaflight has a comprehensive arming check system (`src/main/fc/runtime_config.c`):

```
ARMING_DISABLED_NO_GYRO                -> Gyro calibration not completed
ARMING_DISABLED_FAILSAFE               -> Failsafe active
ARMING_DISABLED_RX_FAILSAFE            -> No radio signal
ARMING_DISABLED_BAD_RX_RECOVERY        -> Radio just recovered from failsafe
ARMING_DISABLED_BOXFAILSAFE            -> Failsafe switch active
ARMING_DISABLED_THROTTLE               -> Throttle not at minimum
ARMING_DISABLED_ANGLE                  -> Vehicle not level (angle > arming_angle)
ARMING_DISABLED_BOOT_GRACE_TIME        -> Still within 5-second boot grace period
ARMING_DISABLED_NOPREARM               -> Prearm switch not set
ARMING_DISABLED_LOAD                   -> CPU load too high
ARMING_DISABLED_CALIBRATING            -> Calibration in progress
ARMING_DISABLED_CLI                    -> CLI is active
ARMING_DISABLED_CMS_MENU               -> OSD menu active
ARMING_DISABLED_BST                    -> Blackbox logging error
ARMING_DISABLED_MSP                    -> MSP configuration active
ARMING_DISABLED_PARALYZE               -> Paralyze mode active (anti-theft)
ARMING_DISABLED_GPS                    -> GPS not ready
ARMING_DISABLED_RESC                   -> Crash recovery active
ARMING_DISABLED_REBOOT_REQUIRED        -> Parameter change requires reboot
ARMING_DISABLED_DSHOT_BITBANG           -> DShot bitbang failed
ARMING_DISABLED_ACC_CALIBRATION        -> Accel calibration needed
ARMING_DISABLED_MOTOR_PROTOCOL         -> Motor protocol error
ARMING_DISABLED_ARM_SWITCH             -> Arm switch in wrong position at boot
```

Each of these flags is independently checked, and ALL must be clear before arming is allowed. The OSD displays which flags are blocking arming.

#### Key Design Patterns

1. **Bitfield flags** for arming blockers (fast to check, easy to display).
2. **State transitions are event-driven** (USB connect/disconnect, radio commands, calibration completion).
3. **The flight control loop always runs** regardless of state -- only motor output is gated.
4. **Timer-based grace periods** prevent accidental arming immediately after boot or failsafe recovery.

### 2.2 ArduPilot's Approach

ArduPilot uses a more complex hierarchical state machine:

#### Vehicle States

```
INITIALISING   -> Hardware detection, driver init, parameter load
STARTUP        -> Sensor calibration, GPS lock wait
GROUND         -> On ground, not armed
FLYING         -> Armed and in flight
LANDING        -> Executing landing maneuver
```

#### Flight Modes (Copter)

ArduPilot separates "vehicle state" from "flight mode." The flight mode determines the control algorithm:

```
STABILIZE      -> Angle-stabilized manual control
ALT_HOLD       -> Stabilize + altitude hold
LOITER         -> Position hold (GPS required)
RTL            -> Return to launch
AUTO           -> Waypoint navigation
ACRO           -> Rate-controlled manual (no stabilization)
LAND           -> Automatic landing
GUIDED         -> External control (GCS or companion computer)
AUTOTUNE       -> PID auto-tuning mode
```

#### Calibration Workflow

ArduPilot's calibration is NOT done in-flight. It uses a separate calibration workflow:

1. **Ground Station Software (Mission Planner / QGroundControl)** initiates calibration via MAVLink commands.
2. **Calibration process runs on the flight controller** but communicates status back to the ground station for user guidance.
3. **During calibration:**
   - Motor output is completely disabled.
   - The flight mode is irrelevant.
   - ArduPilot displays progress messages and instructions ("place vehicle level," "place vehicle nose up," etc.).
4. **After calibration:**
   - Parameters are automatically saved.
   - The vehicle returns to normal operation.
   - A reboot may be required for some calibrations.

#### Key Design Pattern: Scheduler

ArduPilot uses a cooperative task scheduler rather than a single monolithic loop:

```cpp
// In copter's scheduler table:
SCHED_TASK(update_flight_mode,   400, 100),  // 400 Hz
SCHED_TASK(read_radio,           100,  50),  // 100 Hz
SCHED_TASK(read_baro,             10,  30),  //  10 Hz
SCHED_TASK(update_GPS,            50,  50),  //  50 Hz
SCHED_TASK(update_logging,        10, 100),  //  10 Hz
SCHED_TASK(compass_accumulate,   100,  50),  // 100 Hz
```

Each task has a target rate and a maximum execution time budget. The scheduler runs tasks in priority order and tracks execution time to prevent overruns.

### 2.3 INAV's Approach

INAV's state machine is a hybrid of Betaflight and ArduPilot:

- Same arming flag system as Betaflight (bitfield-based).
- Flight modes similar to ArduPilot (ANGLE, HORIZON, NAV_POSHOLD, NAV_RTH, etc.).
- Calibration triggered via Configurator or CLI (same as Betaflight).
- Navigation state machine layered on top of the base flight controller.

### 2.4 Best Practices for Debug/Test vs Production Builds

#### Betaflight

- **Compile-time defines:** `DEBUG_MODE`, `USE_CLI`, `USE_TELEMETRY_DEBUG`.
- **Runtime debug modes:** A `debug_mode` parameter enables specific debug data output to the blackbox log (e.g., `debug_mode = GYRO_SCALED` logs raw vs. filtered gyro data).
- **Build targets:** Different build targets can include or exclude features (e.g., `BETAFLIGHT_RELEASE` strips debug CLI commands).

#### ArduPilot

- **HAL (Hardware Abstraction Layer):** ArduPilot can be compiled for real hardware (HAL_ChibiOS) or for SITL (Software In The Loop) simulation (HAL_SITL). The SITL build replaces hardware drivers with simulated sensor inputs, allowing full testing on a desktop PC.
- **Log levels:** ArduPilot uses `GCS_SEND_TEXT` with severity levels (EMERGENCY, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO, DEBUG) for runtime diagnostics.
- **DataFlash logging:** All internal state is logged to onboard flash at high rate. This provides post-flight debugging without needing runtime debug builds.

#### Best Practices for floppi

1. **Use `#ifdef DEBUG` blocks** for serial print statements that should not run in production. The current floppi approach of commenting/uncommenting print functions is functional but error-prone.
2. **Consider a build flag in `platformio.ini`:**
   ```ini
   [env:teensy40_debug]
   build_flags = -DDEBUG -DSERIAL_DEBUG

   [env:teensy40_release]
   build_flags = -DNDEBUG -O2
   ```
3. **Never gate safety-critical code behind debug flags.** The flight controller, failsafe, and motor output code should be identical between debug and release builds.
4. **The current `checkCalibrationMode()` approach in floppi is reasonable** -- using a radio switch to enter calibration mode. This is simpler than Betaflight's MSP-based approach and appropriate for the project's current stage.

### 2.5 Key Takeaways for floppi

1. **The current floppi state model (CALIB_NONE / CALIB_ACCEL_GYRO / CALIB_ATTITUDE / CALIB_RADIO + armedFly boolean) is a good start** but should be formalized into a proper state machine enum.
2. **Add arming checks** beyond just throttle position. At minimum: gyro calibration complete, vehicle approximately level, no failsafe condition.
3. **Consider separating "USB connected" from "radio-controlled"** mode. When USB is connected, disable motor output as a safety measure (Betaflight pattern).
4. **The flight control loop should always run** (sensor reads, attitude estimation) even during calibration. Only motor output should be disabled. The current floppi implementation skips the entire flight controller during calibration (`if (!calibration_in_progress)` gates everything), which means the Madgwick filter stops running and will need to reconverge after calibration.
5. **Long-term: implement a task scheduler** similar to ArduPilot's, with prioritized tasks and execution time budgets. For now, the single-loop approach is acceptable.

---

## 3. Auto-Detection Features in Flight Controllers

### 3.1 IMU Auto-Detection

#### How Betaflight Does It

Betaflight has comprehensive IMU auto-detection in `src/main/drivers/accgyro/`:

1. **I2C scanning:** At boot, Betaflight probes known I2C addresses for supported IMUs:
   - MPU6000/6050: 0x68, 0x69
   - MPU6500: 0x68, 0x69
   - MPU9250: 0x68, 0x69
   - ICM-20601/20602/20608: 0x68, 0x69
   - ICM-20689: 0x68, 0x69
   - ICM-42688-P: 0x68, 0x69
   - BMI160/BMI270: 0x68, 0x69
   - LSM6DS3: 0x6A, 0x6B

2. **WHO_AM_I register:** After finding a device at an address, Betaflight reads the WHO_AM_I register (typically register 0x75) and compares the response to known values:
   - MPU6050: returns 0x68
   - MPU6500: returns 0x70
   - MPU9250: returns 0x71
   - ICM-20689: returns 0x98
   - ICM-42688-P: returns 0x47
   - BMI270: returns 0x24

3. **SPI detection:** For SPI-connected sensors, Betaflight tries each SPI bus/CS pin combination defined in the board target file, reading WHO_AM_I over SPI.

4. **Fallback:** If no supported IMU is detected, Betaflight enters an infinite error loop (no flight without IMU).

#### How ArduPilot Does It

ArduPilot takes a "probe all, use what responds" approach:

1. **Bus probing:** ArduPilot probes all defined I2C and SPI buses for known sensors.
2. **Multi-IMU support:** If multiple IMUs are found, ArduPilot uses all of them simultaneously for redundancy. The EKF fuses data from multiple IMUs and can detect a faulty sensor.
3. **Sensor priority:** If multiple incompatible sensors are found on the same bus, ArduPilot uses a priority system to select the preferred one.
4. **Hot-plug support:** Some ArduPilot boards support detecting sensors that are connected after boot (e.g., external I2C compass modules).

#### How It Applies to floppi

The current floppi implementation uses compile-time `#define USE_MPU6050` / `#define USE_MPU9250` selection. Auto-detection would work as follows:

```
1. Try I2C address 0x68, read WHO_AM_I register
   - If 0x68 -> MPU6050 detected
   - If 0x70 -> MPU6500 detected
   - If 0x71 -> MPU9250 detected
2. If I2C fails, try SPI with MPU9250 protocol
3. Set up the appropriate driver based on detection
```

**Feasibility for floppi: HIGH.** This is straightforward to implement and eliminates a common source of user confusion (wrong IMU selected in config.h).

### 3.2 Radio Receiver Protocol Auto-Detection

#### How Betaflight Does It

Betaflight supports limited receiver auto-detection:

- **SBUS vs CRSF vs FPORT:** These all use the same UART but have different baud rates and frame formats. Betaflight can detect the protocol by:
  1. Configuring the UART at SBUS baud rate (100000, 8E2) and checking for valid frames.
  2. If no valid SBUS frames after a timeout, trying CRSF baud rate (420000, 8N1).
  3. If no valid CRSF, trying FPort, etc.
- **PPM vs PWM:** These use different pin configurations and are selected at compile time or via configuration.
- **In practice:** Most users configure the receiver type in the Configurator. Auto-detection is a fallback.

#### How ArduPilot Does It

ArduPilot does NOT auto-detect receiver protocol. The user must set the `SERIAL_PROTOCOL` parameter for the UART connected to the receiver. This is because:
1. Trying wrong baud rates on a UART can cause garbage data that looks like valid commands.
2. The consequence of misdetection (motors spinning unexpectedly) is dangerous.
3. The receiver type is a one-time setup that does not change.

#### Key Takeaways for floppi

1. **Receiver auto-detection is risky and generally not recommended.** Misdetection could cause unsafe behavior.
2. **The current compile-time selection in config.h is the safest approach.** It is also what dRehmFlight uses.
3. **The floppi radio calibration program already auto-detects WHICH channel maps to WHICH control** (throttle, roll, pitch, yaw). This is the more useful auto-detection -- the protocol is fixed, but the channel mapping varies between transmitters.
4. **Future enhancement:** A "receiver test" mode that tries each protocol and reports what it sees, without acting on it. This would help users identify their receiver protocol without risking unsafe behavior.

### 3.3 Motor/ESC Configuration Detection

#### What Can Be Auto-Detected

- **Motor direction:** Betaflight's "motor test" feature spins each motor individually at low speed. The user (or a current sensor) can verify correct direction.
- **ESC protocol support:** Some ESCs respond to a "get info" command over DShot, reporting supported protocols and firmware version.
- **Motor count:** Cannot be auto-detected; the mixer must be configured manually.

#### What Needs Manual Configuration

- **Motor count and layout** (quad X, quad +, hex, octocopter, etc.)
- **Motor spin direction** (though Betaflight can reverse individual motors via DShot commands)
- **ESC protocol** (PWM, OneShot125, OneShot42, Multishot, DShot150/300/600)
- **Motor idle value** (minimum throttle that keeps motors spinning)

#### Key Takeaways for floppi

1. **Motor/ESC configuration should remain manual.** There is no safe way to auto-detect motor count or layout.
2. **A motor test mode** (spin each motor individually for verification) would be a valuable safety feature.
3. **The current floppi config.h approach** (`#define USE_ONESHOT125` vs `#define USE_STANDARD_PWM`) is appropriate.

### 3.4 Board Orientation Detection

#### How ArduPilot Handles It

ArduPilot has a `BOARD_ORIENTATION` parameter that can be set to standard orientations (ROTATION_NONE, ROTATION_YAW_90, ROTATION_YAW_180, etc.). There are 42 predefined orientations covering all practical mounting angles.

ArduPilot does NOT auto-detect board orientation. However, it provides a "calibration" that determines orientation:
1. During accelerometer calibration, the user places the board in known orientations.
2. ArduPilot uses the gravity vector readings to determine which axis is up.
3. If the results are inconsistent with the configured BOARD_ORIENTATION, it warns the user.

#### How Betaflight Handles It

Betaflight has `board_align_roll`, `board_align_pitch`, `board_align_yaw` parameters that specify the mounting angle in decidegrees (tenths of a degree). This allows arbitrary mounting orientations, not just 90-degree increments.

Betaflight does not auto-detect orientation. The user measures or estimates the mounting angle and enters it manually.

#### Key Takeaway for floppi

The floppi `calibrateIMUWithOrientation()` function already implements an auto-detection approach that is MORE sophisticated than what Betaflight or ArduPilot offer. It uses three physical positions (level, nose-up, right-up) to determine which IMU axis corresponds to which aircraft axis, including inversions. This is excellent work and is a differentiating feature.

**Recommendation:** Keep and refine this feature. It is one of the most user-friendly calibration approaches in any open-source flight controller.

---

## 4. PID Auto-Tuning for Quadcopters

### 4.1 Betaflight's Approach

Betaflight does NOT have a true auto-tune feature in the traditional sense. Instead, it uses:

#### Default PID Values

Betaflight ships with "one-size-fits-most" default PID values that work reasonably well on a wide range of quadcopters. The defaults (as of Betaflight 4.5+) are approximately:

```
Roll:  P=45, I=80, D=40, F=120
Pitch: P=47, I=84, D=46, F=125
Yaw:   P=45, I=80, D=0,  F=120
```

These defaults are tuned for a typical 5-inch FPV quadcopter weighing 500-700g.

#### Slider-Based Tuning

Betaflight Configurator provides "sliders" that scale all PID values simultaneously based on:
1. **Master PID multiplier:** Scales all P, I, D values together (for overall responsiveness).
2. **PD ratio:** Adjusts the ratio of P to D (for damping characteristics).
3. **Stick response:** Adjusts the feedforward (F) term.
4. **Filter tuning:** Adjusts the gyro and D-term filter cutoff frequencies.

This approach lets users tune the "feel" of the quad without understanding individual PID terms.

#### RPM Filtering

Betaflight's most impactful innovation is RPM filtering (bidirectional DShot), which uses motor RPM telemetry to place notch filters exactly at motor vibration frequencies. This removes vibration noise so effectively that PID gains can be pushed much higher, making the default gains work on a wider range of aircraft.

### 4.2 ArduPilot's AUTOTUNE Mode

ArduPilot has a full auto-tuning system (`mode_autotune.cpp`) that is the gold standard for open-source PID auto-tuning:

#### How It Works

1. **Activation:** The pilot switches to AUTOTUNE flight mode while hovering.
2. **Twitch maneuvers:** The flight controller performs a series of rapid angular "twitches" -- commanding a sharp change in rate and measuring the vehicle's response.
3. **For each axis (roll, pitch, yaw separately):**
   a. Command a rate step (e.g., +100 deg/s roll rate).
   b. Measure the response: overshoot, settling time, steady-state error.
   c. Adjust P gain: Increase until overshoot reaches target (typically 10-15%).
   d. Adjust D gain: Increase until oscillation begins, then back off.
   e. Set I gain: Based on P (typically I = P * 0.5, or tuned to eliminate steady-state error within a target time).
4. **Iterative refinement:** The autotune runs multiple twitch cycles per axis, converging on optimal gains.
5. **Completion:** After all axes are tuned, the pilot can test the new gains by switching back to STABILIZE mode. If the gains are acceptable, landing saves them permanently. If not, the old gains are restored.

#### Tuning Algorithm Details

ArduPilot's autotune uses a modified Ziegler-Nichols approach:

- **Rate P tuning:** Performs step responses and measures the response ratio (output / input). Adjusts P until the desired response ratio is achieved.
- **Rate D tuning:** Increases D until the system exhibits a specific amount of oscillation after a step input (measured by counting zero crossings of the rate error).
- **Angle P tuning:** Adjusts the angle P (outer loop) to achieve a target angle response time.
- **Aggressiveness parameter:** `AUTOTUNE_AGGR` (default 0.1) controls how aggressive the tuning is. Lower values produce gentler, more conservative gains. Higher values produce snappier, more aggressive gains.

#### Limitations

- **Requires stable hover:** The vehicle must be able to hover (even poorly) before autotune can work.
- **Wind sensitivity:** Wind gusts during autotune can corrupt the measurements, leading to poor gains.
- **Yaw tuning is less reliable:** Yaw dynamics are affected by motor inertia and propeller torque effects that are difficult to model with step responses.
- **Does not tune the altitude controller or position controller.**

### 4.3 Academic Approaches to Online PID Tuning

#### Relay Feedback Method (Astrom-Hagglund)

The most commonly cited academic approach for auto-tuning PID controllers in multirotors:

1. Replace the PID controller with a relay (bang-bang controller).
2. The relay causes the system to oscillate at its natural (resonant) frequency.
3. Measure the oscillation amplitude (a) and period (T_u).
4. Apply Ziegler-Nichols rules:
   - K_u (ultimate gain) = 4d / (pi * a), where d is the relay amplitude
   - P = 0.6 * K_u
   - I = 2 * P / T_u
   - D = P * T_u / 8

This is essentially what ArduPilot's autotune does, but with step responses instead of relay oscillation.

#### Model Reference Adaptive Control (MRAC)

A more sophisticated academic approach:

1. Define a reference model that describes the desired closed-loop behavior (e.g., "I want the roll rate to respond like a first-order system with time constant tau=0.05s").
2. The adaptive controller adjusts PID gains in real-time to make the actual response match the reference model.
3. Uses Lyapunov stability theory to guarantee convergence.

This approach is used in some research papers but is computationally intensive and not yet practical for Teensy-class microcontrollers.

#### Extremum Seeking

An online optimization approach:

1. Add a small sinusoidal perturbation to one PID gain.
2. Measure the effect on a cost function (e.g., tracking error variance).
3. Use the correlation between the perturbation and the cost function to estimate the gradient.
4. Move the gain in the direction that reduces the cost.
5. Repeat for all gains.

This converges slowly but is very robust and can adapt to changing conditions (e.g., wind, battery depletion).

#### Neural Network / Machine Learning Approaches

Recent academic papers explore:

1. Reinforcement learning to optimize PID gains in simulation, then transfer to real hardware.
2. Gaussian Process optimization (Bayesian optimization) for PID tuning.
3. Deep neural networks to predict optimal gains from vehicle parameters (mass, inertia, motor thrust curves).

These are research-stage and not practical for embedded implementation yet.

### 4.4 Key Takeaways for floppi

1. **For Phase 1, manual PID tuning with good defaults is sufficient.** The current floppi defaults are conservative and should be flyable.
2. **ArduPilot's autotune approach is the most practical to implement** and is the recommended long-term goal. It requires:
   - A stable rate controller (already in floppi).
   - The ability to command rate steps from within the firmware.
   - Response measurement (peak overshoot, settling time).
   - Iterative gain adjustment with convergence checks.
3. **A simpler "vibration analyzer" would be immediately useful:** Log gyro data during a short hover, compute the power spectral density, and identify problematic vibration frequencies. This helps the user set low-pass filter cutoffs and diagnose mechanical issues.
4. **The Betaflight slider approach** could be implemented as a simpler alternative to full autotune: provide a few "master tuning knobs" that scale all gains together.
5. **Estimated implementation effort for ArduPilot-style autotune:** Significant (several weeks of development and extensive flight testing). This is a Phase 3+ feature.

---

## 5. Teensy-Specific Considerations

### 5.1 EEPROM Emulation on Teensy 4.x

#### How It Works

The Teensy 4.0/4.1 (IMXRT1062 processor) does not have dedicated EEPROM hardware. Instead, Paul Stoffregen's Teensyduino core implements EEPROM emulation using a dedicated region of the NOR flash:

- **Teensy 4.0:** 1080 bytes of emulated EEPROM.
- **Teensy 4.1:** 4284 bytes of emulated EEPROM.
- **Implementation:** The EEPROM library (`EEPROM.h` from Teensyduino) handles all wear-leveling internally. Writes are accumulated in RAM and flushed to flash using a log-structured approach.
- **Wear leveling:** The library spreads writes across a larger flash region (typically 60KB) to distribute wear. PJRC estimates 100,000+ write cycles per logical byte.
- **Write timing:** Writes to EEPROM can take up to 100ms when a flash erase cycle is needed (rare, typically after thousands of writes). Most writes are much faster (~1ms). **Do not write EEPROM in the main flight control loop.**
- **Read timing:** Reads are fast (microseconds), as the data is cached in RAM.

#### Best Practices for floppi

1. **Use `EEPROM.put()` and `EEPROM.get()`** for structured data (they handle multi-byte reads/writes automatically).
2. **Allocate a fixed memory map:**
   ```
   Address 0-1:    Magic number (0xF10B for "floppi")
   Address 2:      Data version byte
   Address 3-14:   Gyro calibration (3 x float = 12 bytes)
   Address 15-26:  Accel calibration offsets (3 x float = 12 bytes)
   Address 27-38:  Accel calibration scales (3 x float = 12 bytes)
   Address 39-40:  CRC16 checksum
   Address 41-52:  Radio channel mapping (6 x uint16_t = 12 bytes)
   Address 53-64:  PID gains backup (for future autotune)
   ```
3. **Only write EEPROM when calibration completes** -- never in the main loop.
4. **Validate on read:** Check magic number, version, and CRC before trusting stored data.
5. **Teensy 3.6 note:** If supporting Teensy 3.6, it has 4096 bytes of real EEPROM (FlexRAM-backed) with even better wear characteristics.

### 5.2 Real-Time Performance Characteristics

#### Teensy 4.0/4.1 Hardware

- **Processor:** ARM Cortex-M7 (NXP IMXRT1062) running at 600 MHz.
- **FPU:** Full hardware single-precision AND double-precision floating point (this is exceptional for a microcontroller).
- **RAM:** 1 MB (Teensy 4.0) or 1 MB (Teensy 4.1), with 512 KB of tightly-coupled memory (ITCM for code, DTCM for data) that runs at full CPU speed with zero wait states.
- **Cache:** 32KB I-cache, 32KB D-cache for accessing the remaining 512KB of RAM.

#### Performance Benchmarks (Relevant to Flight Control)

- **Single-precision multiply:** 1 cycle (1.67 ns at 600 MHz).
- **Single-precision divide:** 14 cycles (~23 ns).
- **sqrt():** ~14 cycles using hardware FPU.
- **sin()/cos():** ~50-100 cycles using hardware FPU (depending on implementation).
- **Madgwick filter update (6DOF):** approximately 10-15 microseconds (measured on dRehmFlight).
- **Full control loop iteration (sensor read + Madgwick + PID + motor output):** approximately 50-100 microseconds.

This means the Teensy 4.0 can easily sustain **2 kHz loop rates** (500 microseconds per iteration) with 80-90% of each cycle spent idle. Some users have achieved **8 kHz** loop rates on dRehmFlight, though the benefit above 2 kHz is marginal for PID control.

#### Real-Time Considerations

1. **IntervalTimer:** Teensy 4.x supports hardware timers that can trigger ISRs at precise intervals. For consistent loop timing, using an IntervalTimer is more reliable than the current `loopRate()` busy-wait approach.
2. **DMA for sensor reads:** The Teensy 4.x supports DMA (Direct Memory Access) for both I2C and SPI. DMA-based sensor reads can run in the background while the CPU processes the previous sample, reducing overall latency.
3. **Interrupt priorities:** Teensy 4.x supports nested interrupts with configurable priority levels. Radio receiver interrupts should run at high priority; serial communication at lower priority.
4. **USB serial impact:** The USB serial interface runs on its own interrupt handler and should not affect flight control timing. However, **excessive Serial.print() calls can cause the USB buffer to fill, potentially blocking the calling code.** Always use non-blocking serial output in the main loop.

### 5.3 I2C Best Practices for IMU Communication

#### I2C on Teensy 4.x

- **Hardware I2C peripheral:** The IMXRT1062 has 4 hardware I2C peripherals (LPI2C1-4).
- **Wire library:** Teensyduino's Wire library uses the hardware I2C with DMA support.
- **Clock speed:** The MPU6050 datasheet specifies a maximum I2C clock of 400 kHz. dRehmFlight overclocks to 1 MHz, which works on most MPU6050 boards but is out of spec and may fail on some units.

#### Recommendations

1. **Use 400 kHz I2C** for reliability. The current floppi code uses `Wire.setClock(400000)`, which is correct. The original dRehmFlight uses 1 MHz, which is risky.
2. **Consider I2C timeout handling.** If the MPU6050 does not respond (loose wire, damaged sensor), the I2C read can hang indefinitely. Add a timeout:
   ```cpp
   Wire.setWireTimeout(3000, true);  // 3ms timeout, reset on timeout
   ```
   Teensy 4.x Wire library supports this since Teensyduino 1.54+.
3. **Minimize I2C reads per loop.** Reading all 6 axes (accel + gyro) from MPU6050 takes approximately 300-400 microseconds at 400 kHz. Using burst reads (reading all registers in one transaction) is essential and is what `mpu6050.getMotion6()` already does.
4. **Pull-up resistors:** Ensure adequate I2C pull-ups (4.7K ohm typical). Most MPU6050 breakout boards include pull-ups, but if using bare sensors, external pull-ups are required.

### 5.4 SPI Best Practices for IMU Communication

#### SPI on Teensy 4.x

- **Hardware SPI:** The IMXRT1062 has 2 hardware SPI peripherals (LPSPI3, LPSPI4).
- **Clock speed:** MPU9250 supports up to 1 MHz SPI for register reads and 20 MHz for burst data reads. The Teensy 4.x can easily drive SPI at these speeds.
- **DMA support:** Teensyduino's SPI library supports DMA transfers, allowing background data reads.

#### Recommendations

1. **SPI is significantly faster than I2C** for IMU communication. A full 6-axis read over SPI takes approximately 10-20 microseconds (vs. 300-400 microseconds for I2C at 400 kHz). If both sensors are supported, document this performance difference for users.
2. **Use chip select (CS) properly.** The CS pin must be driven LOW before the SPI transaction and HIGH after. The Teensy 4.x SPI library handles this automatically when using `SPI.beginTransaction()` and `SPI.endTransaction()`.
3. **SPI mode:** MPU9250 uses SPI Mode 3 (CPOL=1, CPHA=1). Ensure this is configured correctly.

### 5.5 Other Teensy-Specific Notes

#### Floating Point Performance

The Teensy 4.x Cortex-M7 has a full hardware FPU that supports both single-precision (float) and double-precision (double) operations. However:

- **float operations are typically 1 cycle** (pipeline permitting).
- **double operations are 2-4x slower** than float.
- **Recommendation:** Use `float` everywhere in the flight control code. The current floppi code correctly uses `float` throughout. Avoid accidental `double` promotion (e.g., use `0.5f` instead of `0.5`, use `constrain()` instead of arithmetic that might promote to double).

#### Memory Layout

- **ITCM (Instruction Tightly-Coupled Memory):** 512KB, runs at full speed. Time-critical functions should be placed here using `FASTRUN` attribute.
- **DTCM (Data Tightly-Coupled Memory):** 512KB, runs at full speed. Time-critical data (IMU readings, PID variables) should be placed here (this is the default for global variables).
- **OCRAM:** 512KB, accessible via cache. Suitable for less time-critical data (logging buffers, configuration).

**Recommendation:** Mark the main loop and all functions called from it with `FASTRUN`:

```cpp
FASTRUN void loop() { ... }
FASTRUN void getIMUdata() { ... }
FASTRUN void Madgwick6DOF(...) { ... }
FASTRUN void controlANGLE() { ... }
```

This ensures these functions are placed in ITCM and execute at full speed without cache misses.

---

## 6. Gap Analysis: Current floppi Implementation

Based on the research above, here is an assessment of the current floppi codebase against best practices:

### What floppi Does Well

| Feature | Assessment | Notes |
|---------|-----------|-------|
| Gyro/accel calibration procedure | Good | 2000 samples, correct bias calculation |
| IMU orientation auto-detection | Excellent | 3-position test with axis mapping -- better than most FC projects |
| Radio channel auto-detection | Excellent | Step-by-step interactive calibration |
| Calibration safety checks | Good | Checks armed status, throttle position, 3-second hold |
| PID controller structure | Correct | Standard rate and angle mode implementations |
| Madgwick filter | Correct | Standard 6DOF implementation |
| Low-pass filtering | Good | Appropriate coefficients for 2 kHz loop |
| Config.h organization | Good | Clear, well-commented configuration file |

### Gaps to Address

| Gap | Severity | Description |
|-----|----------|-------------|
| No movement detection during gyro calibration | High | Betaflight resets calibration if board moves. floppi blindly averages. |
| No EEPROM storage for calibration | High | Users must edit config.h and reflash after every calibration. |
| No automatic gyro calibration at every boot | High | Current code uses hard-coded values unless CH6 is high at startup. |
| Calibration blocks entire flight loop | Medium | Madgwick filter stops during calibration, requiring re-convergence. |
| No arming checks beyond throttle | Medium | Missing: gyro cal complete, vehicle level, failsafe check. |
| No USB-connected safety mode | Medium | Motors can activate while connected to computer. |
| No temperature recording at calibration | Low | Would help diagnose drift issues. |
| No multi-position accel calibration | Low | Single-position is adequate for initial flights. |
| No auto-detection of IMU type | Low | Would eliminate a common config.h error. |
| Debug output uses commented-out code | Low | Should use #ifdef DEBUG instead. |

---

## 7. Recommended Action Items

### Immediate (Before First Flight)

1. **Add automatic gyro calibration at every startup** with movement detection:
   - Collect 1024 samples at startup (no user intervention).
   - Monitor accel variance during collection; reset counter if variance exceeds threshold.
   - Block arming until calibration completes successfully.
   - Display progress via LED (fast blink during calibration, steady on when complete).

2. **Keep attitude filter running during calibration** by restructuring the main loop to only gate motor output, not sensor/filter processing.

3. **Add basic arming checks:**
   - Gyro calibration complete.
   - Vehicle approximately level (roll and pitch within +/-10 degrees).
   - Throttle at minimum.
   - No failsafe condition.

### Short Term (Post First Flight)

4. **Implement EEPROM storage** for accelerometer calibration data:
   - Store accel offsets after calibration.
   - Load from EEPROM at boot, validate with CRC.
   - Fall back to config.h defaults if EEPROM data is invalid.
   - Gyro offsets are NOT stored (always recalibrated at boot).

5. **Add `#ifdef DEBUG` build configuration** in platformio.ini with debug/release environments.

6. **Add IMU auto-detection** (WHO_AM_I register probe) to eliminate compile-time sensor selection.

### Medium Term (Phase 2)

7. **Implement 6-position accelerometer calibration** with scale factor correction.
8. **Add temperature logging** at calibration time and runtime for future temperature compensation.
9. **Implement a motor test mode** (spin individual motors for verification).
10. **Add USB-connected detection** to disable motor output when connected to a computer.

### Long Term (Phase 3+)

11. **Implement ArduPilot-style PID autotune** using step response measurement.
12. **Implement temperature compensation** with polynomial fit.
13. **Add `FASTRUN` attributes** to time-critical functions for optimal Teensy 4.x performance.
14. **Consider I2C timeout handling** for improved robustness with unreliable sensor connections.

---

## Sources and References

### Open-Source Flight Controller Repositories

- **Betaflight:** https://github.com/betaflight/betaflight
  - Key files: `src/main/sensors/gyro.c`, `src/main/sensors/acceleration.c`, `src/main/fc/runtime_config.c`
- **ArduPilot:** https://github.com/ArduPilot/ardupilot
  - Key files: `libraries/AP_InertialSensor/AP_InertialSensor.cpp`, `ArduCopter/mode_autotune.cpp`, `libraries/AP_InertialSensor/AP_InertialSensor_tempcal.cpp`
- **INAV:** https://github.com/iNavFlight/inav
  - Key files: `src/main/sensors/gyro.c`, `src/main/sensors/acceleration.c`
- **Cleanflight:** https://github.com/cleanflight/cleanflight (archived)
- **dRehmFlight:** https://github.com/nickrehm/dRehmFlight

### Teensy/PJRC Resources

- **Teensy 4.0 Product Page:** https://www.pjrc.com/store/teensy40.html
- **Teensy EEPROM Documentation:** https://www.pjrc.com/teensy/td_libs_EEPROM.html
- **Teensyduino Source (EEPROM):** https://github.com/PaulStoffregen/cores/blob/master/teensy4/eeprom.c
- **IMXRT1062 Reference Manual:** NXP IMXRT1060 Reference Manual (1062 is the variant used in Teensy 4.x)

### ArduPilot Documentation

- **Accelerometer Calibration:** https://ardupilot.org/copter/docs/common-accelerometer-calibration.html
- **Autotune:** https://ardupilot.org/copter/docs/autotune.html
- **Temperature Calibration:** https://ardupilot.org/copter/docs/common-imu-temperature-calibration.html

### Betaflight Documentation

- **Betaflight Wiki:** https://betaflight.com/docs/wiki
- **Betaflight Configurator:** https://github.com/betaflight/betaflight-configurator

### Academic References

- Tedaldi, D., Pretto, A., and Menegatti, E. "A Robust and Easy to Implement Method for IMU Calibration without External Equipments." IEEE ICRA, 2014. (6-position calibration with ellipsoid fitting)
- Astrom, K.J. and Hagglund, T. "Automatic Tuning of PID Controllers." Instrument Society of America, 1988. (Relay feedback method)
- Madgwick, S.O.H. "An efficient orientation filter for inertial and inertial/magnetic sensor arrays." University of Bristol, 2010. (The Madgwick filter used in dRehmFlight)

### IMU Sensor Datasheets

- **MPU6050 Datasheet:** InvenSense PS-MPU-6000A-00 Rev 3.4 (Register map, temperature sensor, calibration specifications)
- **MPU9250 Datasheet:** InvenSense PS-MPU-9250A-01 Rev 1.1 (SPI interface, magnetometer specifications)
- **ICM-20689 Datasheet:** TDK InvenSense DS-000143 (Betaflight's primary sensor for newer FC boards)

---

**Report compiled:** 2026-02-05
**For project:** floppi flight controller (Teensy-based dRehmFlight)
