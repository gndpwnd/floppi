# Flight Controller Firmware - Roadmap

> Last updated: 2026-02-06

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
- [x] Multi-position accelerometer calibration
  - Completed: 2026-02-06
  - Notes: 6-position calibration for offset + scale factor. Serial command 'm'. Outputs 9 defines to config.h.

- [x] Calibration value export workflow
  - Completed: 2026-02-05
  - Notes: Calibration mode outputs values in config.h `#define` format, ready to copy-paste. Output format fixed to match config.h syntax.

### Firmware State Machine

- [x] Build target separation (calibration vs live)
  - Completed: 2026-02-05
  - Notes: PlatformIO `_calibration` environments using `extends` + `-D CALIBRATION_MODE`. Guards in main.cpp, config.h. See [features/build-targets.md](features/build-targets.md).
  - Usage: `pio run -e teensy40` (live) vs `pio run -e teensy40_calibration` (calibration)

- [x] Fix calibration output format
  - Completed: 2026-02-05
  - Notes: calibration.cpp now outputs `#define IMU_ACC_ERROR_X 0.123456f` format matching config.h

- [x] Unify calibration code paths
  - Completed: 2026-02-05
  - Notes: CH6 state machine now calls calibration.cpp routines directly (calibrateIMU, calibrateIMUWithOrientation, calibrateRadio). Old simple functions retained as guarded dead code pending removal.

- [x] Live mode with hard-coded values
  - Completed: 2026-02-05
  - Notes: Default `pio run -e teensy40` compiles without CALIBRATION_MODE — all calibration code, debug prints, and state machine compiled out

- [ ] Setup/calibration mode for PID tuning
  - Description: A mode where PID values can be adjusted via serial or fc_tool without re-flashing
  - Dependencies: Serial command interface

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
- [x] Modular source code architecture
  - Completed: 2026-02-06
  - Notes: Split main.cpp into imu.cpp, control.cpp, motors.cpp, debug.cpp + globals.h
- [x] Serial command interface for calibration mode
  - Completed: 2026-02-06
  - Notes: Commands r/i/o/s/h for radio, IMU, orientation, status, help
- [x] fc_tool integration protocol
  - Completed: 2026-02-06
  - Notes: Added telemetry output functions (printIMUTelemetry, printFullTelemetry) compatible with fc_tool parser. Serial command 't' toggles telemetry modes.
  - Related: See [fc_tool/docs/features/serial-telemetry-protocol.md](/fc_tool/docs/features/serial-telemetry-protocol.md)

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

## Future Platform Features

> These features represent a significant evolution of the project. They are documented here for planning purposes but are lower priority than core functionality validation.

### ESP32 Platform Support

- [x] ESP32 port of dRehmFlight core
  - Completed: 2026-02-06
  - Notes: Basic port compiles for ESP32 and ESP32-S3. Includes LEDC PWM, I2C, serial port adaptations.
  - Environments: esp32, esp32_calibration, esp32s3, esp32s3_calibration
  - Related findings: [esp32-fc-feasibility.md](findings/esp32-fc-feasibility.md)

- [ ] Dual-core architecture
  - Description: Pin FC to Core 0, WiFi/comms to Core 1 for deterministic timing
  - Dependencies: ESP32 port
  - Related findings: [esp32-dual-core-research.md](findings/esp32-dual-core-research.md)

- [ ] WiFi AP mode for configuration
  - Description: ESP32 creates WiFi access point for wireless configuration/telemetry
  - Dependencies: Dual-core architecture

- [ ] HTTP REST API for commands
  - Description: RESTful API for sending commands and reading state (<50ms latency target)
  - Dependencies: WiFi AP mode
  - Related findings: [fc-timing-requirements.md](findings/fc-timing-requirements.md)

- [ ] WebSocket for real-time telemetry
  - Description: Stream attitude, motor values at 10-50Hz for monitoring apps
  - Dependencies: WiFi AP mode

- [ ] OTA firmware updates
  - Description: Update firmware over WiFi without USB connection
  - Dependencies: WiFi integration stable

### Universal Firmware Goal

- [x] PlatformIO multi-board structure for ESP32
  - Completed: 2026-02-06
  - Notes: Added esp32, esp32_calibration, esp32s3, esp32s3_calibration environments. Same codebase, platform-specific code guarded with USE_ESP32.

- [ ] Shared abstraction layer
  - Description: Hardware abstraction for IMU, motors, radio across Teensy and ESP32
  - Dependencies: ESP32 port
  - Notes: Currently pin_definitions.h partially does this

### OLED Display Integration

- [ ] OLED display support (SSD1306)
  - Description: Small I2C OLED for showing calibration status, WiFi info, flight status
  - Library: U8g2
  - Related findings: [oled-display-options.md](findings/oled-display-options.md)

- [ ] Calibration mode display
  - Description: Show current calibration step, progress bar, values
  - Dependencies: OLED display support

- [ ] WiFi info display (ESP32)
  - Description: Show SSID, IP address, MAC on OLED
  - Dependencies: OLED + WiFi integration

- [ ] Live status display
  - Description: Show armed status, battery voltage (if sensor), connection status
  - Dependencies: OLED display support

### Notes on Future Features

- **Multi-drone coordination is OUT OF SCOPE** for flight_controller firmware
- **WiFi API enables** external coordination systems (fc_tool, swarm managers)
- **Target latency**: <100ms for commands, ideally <50ms
- **Design philosophy**: Keep FC firmware simple, let external tools handle complexity

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
- [x] Build target separation (calibration vs live builds) — 2026-02-05
- [x] Calibration output format fix (config.h `#define` format) — 2026-02-05
- [x] Calibration paths unified (CH6 → calibration.cpp) — 2026-02-05
- [x] Live mode (no calibration overhead in default build) — 2026-02-05
- [x] Serial command interface for calibration — 2026-02-06
- [x] Modular source code architecture — 2026-02-06
- [x] 6-position accelerometer calibration — 2026-02-06

---

## Notes

- **Testing is hardware-based**: Tests are baked into the firmware as calibration modes and debug builds, not as separate test files. The firmware itself is the test harness.
- **Calibration workflow**: Flash calibration build → run calibration → copy values to config.h → flash live build → fly. This is by design — keeps live firmware lean.
- **fc_tool synergy**: The fc_tool desktop app will provide visual diagnostics during calibration, making the calibrate → hard-code → flash cycle more user-friendly.
- **VTOL generality**: Always design features to work across vehicle types, not just quadcopters. The mixer pattern from dRehmFlight supports this well.

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
