# Flight Controller Firmware - Roadmap

> Last updated: 2026-02-05

## Overview

This roadmap tracks project-level features and milestones for the flight controller firmware. For immediate tasks, see `todo.md`. For project boundaries, see `scope.md`.

**Note**: No time estimates. Focus on WHAT needs to be done, not WHEN.

**Design philosophy**: Usability and simplicity first. Careful iteration of features over time. Not trying to be all-in-one — a good performing open-source flight controller that people can build in their garage.

---

## Core Features

### Firmware Foundation (dRehmFlight Port)

- [x] Port dRehmFlight to PlatformIO project structure
  - Completed: Pre-2026
  - Notes: Clean build for Teensy 4.0/4.1/3.6

- [x] Multi-platform board support (Teensy 4.0, 4.1, 3.6)
  - Completed: Pre-2026
  - Notes: platformio.ini with separate environments

- [x] MPU6050 IMU integration via I2C
  - Completed: Pre-2026

- [x] SBUS receiver support (FlySky FS-iA6B)
  - Completed: Pre-2026

- [x] PID control loops (rate mode + angle mode)
  - Completed: Pre-2026

- [x] Madgwick 6DOF attitude filter
  - Completed: Pre-2026

- [x] Motor mixing for quadcopter X configuration
  - Completed: Pre-2026

- [x] Arming/disarming safety system (throttle low + CH5)
  - Completed: Pre-2026

- [x] Failsafe on signal loss
  - Completed: Pre-2026

- [x] Servo output (7 channels for VTOL/plane control surfaces)
  - Completed: Pre-2026

### Calibration System

- [x] Basic IMU auto-calibration via CH6 switch
  - Completed: Pre-2026
  - Notes: Gyro bias + accelerometer offset calculation, 2000 samples

- [x] Attitude filter warm-up calibration
  - Completed: Pre-2026

- [x] Radio channel auto-mapping and calibration
  - Completed: Pre-2026
  - Notes: Step-by-step guided routine in lib/Calibration/calibration.cpp. Auto-detects channel mapping, outputs copy-paste config.h values.
  - Related findings: [auto-calibration-research.md](findings/auto-calibration-research.md)

- [x] IMU orientation auto-detection
  - Completed: Pre-2026
  - Notes: 3-position test (level, nose-up, right-up) in lib/Calibration/calibration.cpp. Generates axis transformation code.
  - Related findings: [auto-calibration-research.md](findings/auto-calibration-research.md)
- [ ] Multi-position accelerometer calibration
  - Description: 6-position calibration for more accurate accel offset and scale factors
  - Dependencies: Basic IMU calibration working

- [ ] Calibration value export workflow
  - Description: Calibration mode outputs values in config.h `#define` format, ready to copy-paste
  - Dependencies: All calibration routines, output format fix
  - Notes: Key usability feature. Currently partially working but output format mismatches config.h syntax. Fix is tracked in Firmware State Machine section.

### Firmware State Machine

- [ ] Build target separation (calibration vs live)
  - Description: PlatformIO `_calibration` environments using `extends` to inherit board config and add `-D CALIBRATION_MODE` build flag. Default environments are live builds.
  - Dependencies: None
  - Notes: See [features/build-targets.md](features/build-targets.md) for full design. Usage: `pio run -e teensy40` (live) vs `pio run -e teensy40_calibration` (calibration).
  - Includes: `#ifdef CALIBRATION_MODE` guards in main.cpp, guarding `RUN_*` flags in config.h

- [ ] Fix calibration output format
  - Description: calibration.cpp currently prints `float AccErrorX = ...;` but config.h uses `#define IMU_ACC_ERROR_X ...f`. Fix all print functions to output correct format.
  - Dependencies: None

- [ ] Unify calibration code paths
  - Description: CH6-triggered calibration in main.cpp should call the better calibration.cpp routines (with quality checks, stability validation). Remove duplicate simple versions from main.cpp.
  - Dependencies: Build target separation

- [ ] Setup/calibration mode for PID tuning
  - Description: A mode where PID values can be adjusted via serial or fc_tool without re-flashing
  - Dependencies: Build target separation, serial command interface

- [ ] Live mode with hard-coded values
  - Description: Production flight firmware with all values baked in, no debug overhead. Calibration code compiled out via `#ifndef CALIBRATION_MODE`.
  - Dependencies: Build target separation

### Hardware Testing & Validation

- [ ] Bench test: IMU sensor data validation
  - Description: Verify accelerometer and gyroscope readings are correct and calibrated
  - Dependencies: Hardware available

- [ ] Bench test: SBUS receiver communication
  - Description: Verify all 6 channels respond correctly to transmitter inputs
  - Dependencies: Hardware available

- [ ] Bench test: Motor output and ESC response
  - Description: Verify motor commands translate to correct ESC behavior (no props)
  - Dependencies: Hardware available

- [ ] Bench test: Arming/disarming and failsafe
  - Description: Verify safety systems work correctly under all conditions
  - Dependencies: Hardware available

- [ ] Tethered hover test
  - Description: First flight with drone secured/tethered for safety
  - Dependencies: All bench tests pass

- [ ] PID tuning on hardware
  - Description: Tune PID gains for stable flight on actual drone
  - Dependencies: Tethered hover test
  - Notes: Start with conservative values from config.h, iterate

- [ ] Free flight testing
  - Description: Progressive envelope expansion in open area
  - Dependencies: PID tuning baseline

### VTOL Configuration Support

- [x] Quadcopter X mixer
  - Completed: Pre-2026

- [ ] Configurable mixer for different VTOL types
  - Description: Easy-to-customize motor/servo mixing for hex, octo, fixed-wing, tiltrotor, etc.
  - Dependencies: Basic flight validated on quad
  - Notes: dRehmFlight already supports this pattern — formalize and document it

- [ ] Example configurations for common builds
  - Description: Pre-made config files for popular drone types (quad X, quad +, hex, Y6, tricopter)
  - Dependencies: Configurable mixer

---

## Infrastructure / Setup

- [x] PlatformIO project structure with multi-board support
- [x] config.h for all user-configurable settings
- [x] pin_definitions.h for hardware abstraction
- [x] Library organization (SBUS, MPU6050, RadioComm, Calibration, etc.)
- [ ] Serial command interface for calibration mode
  - Description: Accept commands over serial to trigger calibration routines, adjust values
- [ ] fc_tool integration protocol
  - Description: Define serial protocol for fc_tool to read telemetry and send commands
  - Related: See fc_tool docs at /fc_tool/docs/

---

## Nice to Have (Lower Priority)

- [ ] PID auto-tuning mode
  - Description: Automated PID tuning via relay/step response test (like Betaflight/ArduPilot AUTOTUNE)
  - Related findings: [auto-calibration-research.md](findings/auto-calibration-research.md) 
- [ ] Temperature compensation for IMU drift
  - Description: Adjust calibration values based on temperature sensor readings

- [ ] MPU9250 full 9DOF Madgwick filter
  - Description: Currently falls back to 6DOF — implement full 9DOF with magnetometer

- [ ] ESC calibration routine
  - Description: Automated ESC endpoint calibration via firmware

- [ ] Motor direction auto-detection
  - Description: Detect motor spin direction and warn if incorrect

- [ ] OneShot125/DShot ESC protocol
  - Description: Higher performance ESC communication protocols

- [ ] Voltage monitoring and low-battery warning
  - Description: ADC reading of battery voltage, warning via LED/buzzer

- [ ] Rate limiting and expo curves for control inputs
  - Description: Smoother control response for different skill levels

---

## Completed

### Firmware Foundation
- [x] dRehmFlight port to PlatformIO — Pre-2026
- [x] Multi-board support (Teensy 4.0/4.1/3.6) — Pre-2026
- [x] MPU6050 IMU integration — Pre-2026
- [x] SBUS receiver support — Pre-2026
- [x] PID control (rate + angle modes) — Pre-2026
- [x] Madgwick 6DOF attitude filter — Pre-2026
- [x] Motor mixing (quad X) — Pre-2026
- [x] Servo output (7 channels) — Pre-2026
- [x] Arming/disarming safety — Pre-2026
- [x] Failsafe system — Pre-2026
- [x] Basic IMU auto-calibration — Pre-2026
- [x] Attitude filter calibration — Pre-2026
- [x] Radio auto-mapping calibration routine — Pre-2026
- [x] IMU orientation auto-detection routine — Pre-2026
- [x] Project documentation bootstrapped — 2026-02-05
- [x] Auto-calibration research documented — 2026-02-05
- [x] Code review and build target separation plan — 2026-02-05

---

## Notes

- **Testing is hardware-based**: Tests are baked into the firmware as calibration modes and debug builds, not as separate test files. The firmware itself is the test harness.
- **Calibration workflow**: Flash calibration build → run calibration → copy values to config.h → flash live build → fly. This is by design — keeps live firmware lean.
- **fc_tool synergy**: The fc_tool desktop app will provide visual diagnostics during calibration, making the calibrate → hard-code → flash cycle more user-friendly.
- **VTOL generality**: Always design features to work across vehicle types, not just quadcopters. The mixer pattern from dRehmFlight supports this well.

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
