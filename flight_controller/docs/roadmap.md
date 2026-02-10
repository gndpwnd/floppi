# Flight Controller Firmware - Roadmap

> Last updated: 2026-02-09

## Overview

This roadmap tracks project-level features and milestones for the flight controller firmware. For immediate tasks, see `todo.md`. For project boundaries, see `scope.md`.

**Note**: No time estimates. Focus on WHAT needs to be done, not WHEN.

**Design philosophy**: Bare-bones flight stabilizer, not a full autopilot. Raw performance and simplicity over feature count. The FC does lots of math really fast (read sensors → filter → PID → output motors). Complex logic (missions, mode switching, GPS navigation) belongs on an external flight computer. Not trying to be Betaflight/ArduPilot — a simple, fast, open-source flight controller that people can build in their garage. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md) for detailed analysis.

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

> **Goal**: Every hardware-dependent `#define` in config.h has a corresponding auto-calibration routine. Users never guess values — they run guided routines, get validated results, and copy-paste into config.h. The firmware is bare-bones at runtime precisely because calibration is thorough and automated upfront.
>
> **Pattern**: Each routine is a serial command in calibration builds. Routines guide the user step-by-step, validate results with quality checks, offer retry on poor results, and output copy-paste-ready `#define` blocks.

#### Completed Calibration Routines

- [x] IMU offset calibration — serial command `i`
  - Completed: Pre-2026
  - Notes: Gyro bias + accelerometer offset, 2000 samples, stability/level checks, retry on poor results.
  - Outputs: `IMU_ACC_ERROR_X/Y/Z`, `IMU_GYRO_ERROR_X/Y/Z` (6 values)

- [x] 6-position accelerometer calibration — serial command `m`
  - Completed: 2026-02-06
  - Notes: Offset + scale factor from 6 orientations. More accurate than single-position.
  - Outputs: `IMU_ACC_ERROR_X/Y/Z`, `IMU_ACC_SCALE_X/Y/Z`, `IMU_GYRO_ERROR_X/Y/Z` (9 values)

- [x] IMU orientation auto-detection — serial command `o`
  - Completed: Pre-2026
  - Notes: 3-position test (level, nose-up, right-up). Generates axis transformation code.
  - Outputs: Axis transformation code for imu.cpp

- [x] Radio channel auto-mapping — serial command `r`
  - Completed: Pre-2026
  - Notes: Guided stick-move routine. Auto-detects channel mapping, measures ranges.
  - Outputs: `THROTTLE_CHANNEL`, `ROLL_CHANNEL`, `PITCH_CHANNEL`, `YAW_CHANNEL`, `AUX1_CHANNEL`, `AUX2_CHANNEL` (6 values)

- [x] Runtime PID tuning — serial command `g`
  - Completed: 2026-02-09
  - Notes: Runtime-tunable PID gains in calibration builds. Non-blocking line-buffered serial parser.
  - Outputs: User copies tuned values back to config.h PID sections (9 values)

- [x] Attitude filter warm-up calibration
  - Completed: Pre-2026

- [x] Calibration value export workflow
  - Completed: 2026-02-05
  - Notes: All routines output values in config.h `#define` format, ready to copy-paste.

- [x] CH6 switch calibration trigger
  - Completed: Pre-2026
  - Notes: CH6 MID = IMU cal, HIGH = IMU + orientation. Hold 3 seconds to trigger.

- [x] Failsafe auto-detection — serial command `f`
  - Completed: 2026-02-09
  - Notes: Reads normal receiver values (TX on), then failsafe values (TX off). Outputs `FAILSAFE_THROTTLE/ROLL/PITCH/YAW/AUX1/AUX2` defines.

- [x] ESC endpoint calibration — serial command `e`
  - Completed: 2026-02-09
  - Notes: Safety-gated (props-off warning). Sends max PWM → user connects battery → sends min PWM. Standard ESC calibration automated.

- [x] Magnetometer calibration — serial command (MPU9250 only)
  - Completed: 2026-02-09
  - Notes: 30s sphere calibration. Hard-iron offsets + soft-iron scale factors. Guarded by `#ifdef USE_MPU9250`.
  - Outputs: `MAG_ERROR_X/Y/Z`, `MAG_SCALE_X/Y/Z` (6 values)

- [x] Runtime filter/limits tuning — serial command `p`
  - Completed: 2026-02-09
  - Notes: Same pattern as PID `g` command. Covers `B_ACCEL`, `B_GYRO`, `B_DTERM`, `MADGWICK_BETA`, max rates/angles.

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

- [x] D-term low-pass filter
  - Completed: 2026-02-07
  - Description: First-order PT1 low-pass filter on PID derivative term. Prevents motor oscillation from sensor noise being amplified into motor commands. `B_DTERM` coefficient in config.h (0.15 default).
  - Notes: Applied in both controlRATE() and controlANGLE(). ~3 extra muls per axis per tick. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md).

- [x] Rate mode derivative on measurement
  - Completed: 2026-02-07
  - Description: All D-terms now use derivative-on-measurement: `-(GyroX - GyroX_prev) / dt` instead of `(error - error_prev) / dt`. Prevents "derivative kick" on stick input. Fixed in both rate mode (all 3 axes) and angle mode yaw.
  - Notes: Uses separate static variables for previous gyro values since GyroX_prev is overwritten by imu.cpp LP filter.

- [x] Setup/calibration mode for PID tuning
  - Completed: 2026-02-09
  - Description: Runtime-tunable PID gains in calibration builds. Non-blocking line-buffered serial parser. Commands: `g` (show), `g kp_roll 0.2` (set). Gains initialized from config.h macros.
  - Files: main.cpp (tunable variables, serial parser), control.cpp (uses tunable gains in CALIBRATION_MODE), globals.h (extern declarations)

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
- [x] Library vendoring — standalone builds
  - Completed: 2026-02-07
  - Notes: All external libs vendored into lib/ (U8g2, ArduinoJson) and lib_esp32/ (AsyncTCP, ESPAsyncWebServer). No internet downloads needed. See platformio.ini.
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
- [x] Timing calculator (modular, simplified)
  - Completed: 2026-02-07
  - Notes: `tools/timing_calculator.py` entry point with `tools/timing/` package. Simplified inputs: clock/cores/FPU (via `--clock`, `--cores`, `--fpu`/`--no-fpu`) or platform presets (`-p esp32`). Auto-detects features from config.h. Always outputs min/recommended clock speeds and pass/fail status.

---

## Modular Feature System

> **Design**: Every feature is a `#define` flag in config.h. When disabled, the code is **completely excluded** from the binary — zero flash, zero RAM, zero CPU overhead. Users enable exactly what their MCU and use case need.
>
> **Independence**: All feature flags are orthogonal. Enable any combination without conflicts. Tested in all permutations (base only, optimization only, racing only, both, with/without WiFi features).
>
> **Platform awareness**: WiFi features (web server, API client, OTA) are ESP32-only and auto-enabled with `USE_WIFI`. Flight loop features (optimization, racing) work on all platforms. On ESP32, WiFi features run on Core 1 — **zero flight loop impact**.
>
> **Timing calculator**: `tools/timing_calculator.py` auto-detects enabled features from config.h and validates your MCU can handle the compute load. See [tools/timing/](../tools/timing/).

### Feature Modules (config.h)

| Flag | Platform | Description | Core | Status |
| ---- | -------- | ----------- | ---- | ------ |
| `USE_OLED_DISPLAY` | All | OLED display (status, network info) | Core 1 (ESP32) / main (Teensy) | Done |
| `USE_WEB_SERVER` | ESP32 | JSON API, WebSocket, mDNS (floppi.local) | Core 1 | Done |
| `USE_API_SERVER` | ESP32 | HTTP POST telemetry to centralized server | Core 1 | Done |
| `USE_OTA` | ESP32 | Over-the-air firmware updates (ArduinoOTA) | Core 1 | Done |
| `USE_OPTIMIZATION` | All | Biquad filters, notch filter, accel 2nd LP | Core 0 | Done |
| `USE_RACING` | All | Feed-forward, TPA, expo, air mode, smoothing | Core 0 | Done |

**WiFi sub-features** (`USE_WEB_SERVER`, `USE_API_SERVER`, `USE_OTA`) are auto-defined when `USE_ESP32 + USE_WIFI` are set. Comment out individually in config.h to disable while keeping WiFi connectivity.

**Flight loop features** (`USE_OPTIMIZATION`, `USE_RACING`) are disabled by default. Uncomment in config.h to enable. Each has its own parameter section in config.h that's only compiled when the flag is active.

### USE_OPTIMIZATION — Noise reduction for cheap hardware

Enable with `#define USE_OPTIMIZATION` in config.h. For builds using budget motors, unbalanced props, or flexible frames that produce more vibration. Adds steeper filtering to compensate for noisier hardware.

Files: `filters.h`, `filters.cpp` (biquad DSP primitives), modifications to `imu.cpp` and `control.cpp`.

- [x] Biquad gyro low-pass filter — 2nd stage after base PT1, configurable cutoff (`GYRO_LPF_CUTOFF_HZ`)
- [x] D-term biquad filter — replaces base PT1, steeper -12dB/octave rolloff (`DTERM_LPF_CUTOFF_HZ`)
- [x] Configurable gyro notch filter — targets motor/prop resonance (`GYRO_NOTCH_CENTER_HZ`, `GYRO_NOTCH_WIDTH_HZ`). Set center to 0 to disable.
- [x] Accelerometer second-stage low-pass — extra PT1 for attitude stability under vibration (`B_ACCEL_STAGE2`)

### USE_RACING — Performance features for aggressive flying

Enable with `#define USE_RACING` in config.h. Betaflight-inspired optimizations for sharper stick response and high-speed maneuvers. Not for beginners.

Files: modifications to `control.cpp` (getDesState, controlRATE, controlANGLE, controlMixer).

- [x] Feed-forward term — adds setpoint derivative to PID output (`FF_ROLL`, `FF_PITCH`, `FF_YAW`). Set to 0 to disable per-axis.
- [x] TPA — Throttle PID Attenuation — reduces PID at high throttle (`TPA_BREAKPOINT`, `TPA_RATE`)
- [x] Setpoint smoothing — PT1 low-pass on stick input (`SETPOINT_SMOOTH_CUTOFF_HZ`). Set to 0 to disable.
- [x] Air mode — full PID at zero throttle for flips/rolls (`USE_AIRMODE` sub-flag)
- [x] Expo curves — cubic non-linear stick response (`EXPO_ROLL`, `EXPO_PITCH`, `EXPO_YAW`). Set to 0 for linear.

### USE_OTA — Over-the-air firmware updates

Auto-enabled with WiFi. Upload via `pio run -t upload --upload-port floppi-XXXX.local`. Safety: only processes when disarmed.

Files: `ota.h`, `ota.cpp`.

---

## Timing Calculator (Side Project — Low Priority)

> **Status**: Functional but manually maintained. Not a priority for flight_controller development. May become its own project in the future.
>
> **Current state**: `tools/timing_calculator.py` with `tools/timing/` package. Works today with manually maintained operation counts. Don't update it when adding firmware features — the goal is to eventually replace manual maintenance with automatic source scanning.
>
> **Vision**: The calculator should automatically scan all C/C++ source in `flight_controller/` and derive computation complexity from the code itself. No manual operation lists. When features are added to firmware, the calculator picks them up automatically. This is a non-trivial static analysis problem and may warrant its own project.
>
> **Alternative**: Could become a firmware build target (e.g., `pio run -e timing_analysis`) that instruments the actual compiled code rather than doing static analysis from Python.

---

## Hardware Usability (High Priority)

> Features that make the firmware easier to use with real hardware. Important for the "flash, calibrate, fly" experience. Configurable pins is top priority — users should configure everything in config.h.

- [ ] Configurable pin definitions from config.h
  - Description: Move pin assignments from pin_definitions.h into config.h so users configure everything in one file. Currently pin_definitions.h has platform-specific defaults — keep those as fallbacks, but let config.h overrides take priority.
  - Rationale: Easier for users with different board layouts or custom wiring. Makes the firmware more portable across MCU variants.
  - Pattern: `#ifndef MOTOR_PIN_1` / `#define MOTOR_PIN_1 0` in pin_definitions.h, `#define MOTOR_PIN_1 25` in config.h to override.

- [ ] OLED-guided calibration workflow
  - Description: Show calibration instructions and progress on the OLED display during calibration mode. Currently, all calibration guidance is via serial only. Users without a connected laptop should still be able to calibrate using just the OLED + switches.
  - Design: Display prompts ("Calibrating IMU...", "Keep still!", "Done!"), progress bars during sample collection, and final pass/fail status. Works alongside serial output (display shows summary, serial shows details).
  - Dependencies: Display module (done), calibration routines (done)

- [ ] Sequential calibration workflow
  - Description: A guided "full calibration" mode that runs all calibration routines in sequence (e.g., serial command `a` for "all"). User goes through IMU → radio → failsafe → ESC in one session, with clear prompts between each step. Can also run individual tests independently as today.
  - Notes: Currently each calibration is triggered individually via serial commands or CH6 switch. Both modes (individual and sequential) should coexist.

- [ ] Wiring validation on startup
  - Description: Basic startup checks — detect if IMU is responding, if receiver is sending data, if OLED is connected. Report status on serial and display. Helps users catch wiring mistakes before attempting calibration.
  - Dependencies: Hardware testing

---

## RadioComm — Universal Command Layer (High Priority)

> **Vision**: RadioComm is the **single entry point** for all command/control input to the flight controller. Every source of commands — RC receiver, serial, I2C, WiFi API — flows through RadioComm. The flight controller only reads `channel_X_pwm` values and never knows or cares where they came from. This keeps the architecture clean and prevents command paths from getting messy.
>
> **Key insight**: The API web server is NOT a separate command path. It feeds INTO RadioComm, which feeds the flight controller. One entry point, one data format, one failsafe path.

**Current state**: RadioComm handles 4 RC protocols (SBUS, DSM, PPM, PWM), compile-time selected. One protocol per build. All produce `channel_1_pwm` through `channel_6_pwm` in 1000-2000us format.

**Planned command sources:**

| Source | Interface | Config Flag | Notes |
| ------ | --------- | ----------- | ----- |
| SBUS | Serial (inverted) | `USE_SBUS_RECEIVER` | Current default. Existing. |
| DSM/DSMX | Serial | `USE_DSM_RECEIVER` | Spektrum. Existing. |
| PPM | Single GPIO | `USE_PPM_RECEIVER` | Legacy. Existing. |
| PWM | 6 GPIOs | `USE_PWM_RECEIVER` | Legacy. Existing. |
| Serial commands | UART | `USE_SERIAL_COMMANDS` | Flight computer sends channel values over serial |
| I2C commands | I2C slave | `USE_I2C_COMMANDS` | Flight computer sends commands over I2C bus |
| WiFi API | HTTP/WebSocket | `USE_API_SERVER` | Web server routes commands through RadioComm |

- [ ] Serial command input (`USE_SERIAL_COMMANDS`)
  - Description: Accept channel values over serial UART from an external flight computer. Simple text or binary protocol. Flight computer sends throttle/roll/pitch/yaw values, RadioComm writes them to `channel_X_pwm`.
  - Pattern: Same `getCommands()` function, new code path guarded by `#ifdef USE_SERIAL_COMMANDS`.
  - Pin: Configurable in config.h (defaults in pin_definitions.h).

- [ ] I2C command input (`USE_I2C_COMMANDS`)
  - Description: Accept channel values over I2C with the FC as I2C slave. Flight computer (I2C master) writes channel data. Useful for companion computers (Raspberry Pi, etc.) physically connected to the FC.
  - Pattern: I2C slave ISR writes to a buffer, `getCommands()` copies buffer to `channel_X_pwm`.
  - Pin: Configurable in config.h (defaults in pin_definitions.h). Must use different I2C bus than IMU.

- [ ] WiFi API command routing
  - Description: Route commands received by the ESP32 web server (via HTTP POST or WebSocket) through RadioComm instead of directly to flight control. Web server writes to a shared command buffer, `getCommands()` picks it up.
  - Dependencies: `USE_API_SERVER` (ESP32 only). Web server already exists on Core 1.
  - Pattern: Shared volatile struct or queue between Core 1 (web server) and Core 0 (RadioComm/FC).

- [ ] Command source arbitration
  - Description: When multiple command sources are active, RadioComm needs priority logic. RC receiver = primary (real-time hardware). Serial/I2C/WiFi = override sources (flight computer). If override source is active and sending, it takes priority. If it goes silent (timeout), RC receiver resumes. Failsafe applies across ALL sources.
  - Notes: Simple timeout-based arbitration. No complex state machine needed.

---

## Nice to Have (Lower Priority)

- [ ] PID auto-tuning mode
  - Description: Fully automated PID tuning via relay/step response test (like Betaflight/ArduPilot AUTOTUNE). Currently the `g` command allows manual runtime tuning — auto-tuning would replace human judgment with algorithmic optimization.
  - Related findings: [auto-calibration-research.md](findings/auto-calibration-research.md)
  - Notes: High complexity, requires flight-ready hardware. Manual `g` tuning covers 90% of use cases.

- [ ] Temperature compensation for IMU drift
  - Description: Adjust calibration values based on temperature sensor readings

- [ ] MPU9250 full 9DOF Madgwick filter
  - Description: Currently falls back to 6DOF — implement full 9DOF with magnetometer

- [ ] Motor direction auto-detection
  - Description: Detect motor spin direction and warn if incorrect

- [ ] OneShot125/DShot ESC protocol
  - Description: Higher performance ESC communication protocols

- [ ] Voltage monitoring and low-battery warning
  - Description: ADC reading of battery voltage, warning via LED/buzzer

- [ ] In-flight mode switching via radio channel
  - Description: **Likely out of scope** — flight computer territory. The flight computer commands the FC by sending pre-computed attitude targets. For manual flying, pick one mode at compile time.

- [ ] Wind compensation / disturbance rejection
  - Description: Mostly about PID tuning, not special algorithms. D-term filtering + good I-term tuning handles moderate wind. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md).
  - Dependencies: D-term low-pass filter

---

## Future Platform Features

> These features represent a significant evolution of the project. They are documented here for planning purposes but are lower priority than core functionality validation.

### ESP32 Platform Support

- [x] ESP32 port of dRehmFlight core
  - Completed: 2026-02-06
  - Notes: Basic port compiles for ESP32 and ESP32-S3. Includes LEDC PWM, I2C, serial port adaptations.
  - Environments: esp32, esp32_calibration, esp32s3, esp32s3_calibration
  - Related findings: [esp32-fc-feasibility.md](findings/esp32-fc-feasibility.md)

- [x] Dual-core architecture
  - Completed: 2026-02-07
  - Notes: FC on Core 0 (FreeRTOS task, priority 3), Display+WiFi on Core 1 (Arduino loop). xQueueOverwrite for data transfer.
  - Related findings: [esp32-dual-core-research.md](findings/esp32-dual-core-research.md)

### Universal Firmware Goal

- [x] PlatformIO multi-board structure for ESP32
  - Completed: 2026-02-06
  - Notes: Added esp32, esp32_calibration, esp32s3, esp32s3_calibration environments. Same codebase, platform-specific code guarded with USE_ESP32.

- [ ] Shared abstraction layer
  - Description: Hardware abstraction for IMU, motors, radio across Teensy and ESP32
  - Dependencies: ESP32 port
  - Notes: Currently pin_definitions.h partially does this

### OLED Display Integration

- [x] Display module abstraction layer
  - Completed: 2026-02-07
  - Description: Compile-time display selection via build flags. U8g2 library with SW I2C to avoid IMU bus contention. Producer-consumer pattern: flight code writes DisplayData_t struct, display module renders.
  - Library: U8g2 (supports SSD1306 128x32, SSD1306 128x64, SH1106 128x64)
  - Files: display.h, display_data.h, display.cpp
  - Related findings: [display-module-architecture.md](findings/display-module-architecture.md), [oled-display-options.md](findings/oled-display-options.md)

- [x] ESP32 dual-core architecture
  - Completed: 2026-02-07
  - Description: Core 0 runs flight control (FreeRTOS task, priority 3). Core 1 runs display + WiFi (Arduino loop). Queue-based data transfer via xQueueOverwrite.
  - Related findings: [esp32-dual-core-research.md](findings/esp32-dual-core-research.md)

- [x] ESP32 network info display
  - Completed: 2026-02-07
  - Description: Show MAC address, SSID, IP address, RSSI on OLED. Core 1 populates network fields in DisplayData_t.
  - Dependencies: Display module + WiFi STA mode

- [x] Display screen rotation for small displays
  - Completed: 2026-02-09
  - Description: 128x32 displays rotate between info screens every 2 seconds. 128x64 displays show all info at once. State changes reset rotation. Compile-time selection via DISPLAY_LINES macro.
  - Related findings: [display-screen-capacity.md](findings/display-screen-capacity.md)

- [ ] Live flight status display
  - Description: Validate display during actual flight. Currently shows armed/idle/calibrating states with motor outputs, attitude, WiFi info.
  - Dependencies: Hardware testing

### ESP32 WiFi Integration

> **Architecture**: Drones connect to existing WiFi infrastructure (STA mode). This enables swarm coordination — multiple drones on the same network making API requests to centralized computers. No web servers on individual drones (at least not as primary architecture).

- [x] WiFi STA mode (connect to existing WiFi)
  - Completed: 2026-02-07
  - Description: ESP32 connects to existing WiFi network. Supports WPA2-Personal and WPA2-Enterprise (eduroam/university via PEAP). Credentials in gitignored wifi_credentials.h. Non-blocking connection with background reconnection.
  - Related findings: [esp32-wifi-connectivity.md](findings/esp32-wifi-connectivity.md)
  - Reference code: ~/GravityProbe/esp32_enterprise_wpa3_eap/, esp32_ewpa2_iic_091/

- [x] API client for swarm coordination
  - Completed: 2026-02-07
  - Description: HTTP POST telemetry to centralized servers, configurable URL via wifi_credentials.h, non-blocking on Core 1.
  - Notes: api_client.h/cpp

- [x] Web status server
  - Completed: 2026-02-07
  - Description: ESPAsyncWebServer on Core 1. JSON endpoint at /api/status, WebSocket at /ws for streaming telemetry, mDNS at floppi-XXXX.local. ArduinoJson v7.
  - Notes: web_server.h/cpp

- [x] OTA firmware updates
  - Completed: 2026-02-09
  - Description: ArduinoOTA on Core 1. Safety: only processes when disarmed. Upload via `pio run -t upload --upload-port floppi-XXXX.local`. Auto-enabled with USE_WIFI, individually disableable via USE_OTA.
  - Files: ota.h, ota.cpp

### Notes on Future Features

- **Swarm vision**: Multiple drones on the same WiFi network, API client pattern to centralized computers
- **Multi-drone coordination is OUT OF SCOPE** for flight_controller firmware — handled by external systems
- **WiFi API enables** external coordination systems (fc_tool, swarm managers)
- **Target latency**: <100ms for commands, ideally <50ms
- **Design philosophy**: Keep FC firmware simple, let external tools handle complexity
- **Feature ceiling**: The FC has ~90% of target features. Resist adding more. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)
- **Compile-time architecture**: See [features/compile-time-architecture.md](features/compile-time-architecture.md) for details on the #ifdef feature gating and dual-core vs single-core architecture.

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
- [x] Display module architecture research — 2026-02-07
- [x] ESP32 WiFi connectivity research — 2026-02-07
- [x] Display module abstraction layer (U8g2 + SW I2C) — 2026-02-07
- [x] ESP32 dual-core architecture (Core 0 FC, Core 1 display+WiFi) — 2026-02-07
- [x] WiFi STA mode (connect to existing WiFi, WPA2-Personal + Enterprise) — 2026-02-07
- [x] ESP32 network info display (MAC, SSID, IP, RSSI on OLED) — 2026-02-07
- [x] Web status server (ESPAsyncWebServer, JSON API, WebSocket, mDNS) — 2026-02-07
- [x] API client for swarm coordination (HTTP POST to centralized servers) — 2026-02-07
- [x] 6-position accelerometer calibration — 2026-02-06
- [x] Bare-bones FC features research — 2026-02-07
- [x] Compile-time architecture documentation — 2026-02-07
- [x] Library vendoring (standalone offline builds) — 2026-02-07
- [x] Modular feature system (USE_WEB_SERVER, USE_API_SERVER, USE_OPTIMIZATION, USE_RACING) — 2026-02-07
- [x] Timing calculator update (feature tiers, per-core analysis) — 2026-02-07
- [x] Timing calculator modularization (scanner, min clock, clean output) — 2026-02-07
- [x] Timing calculator simplified inputs (clock/cores/FPU, auto min/recommended output) — 2026-02-07
- [x] D-term low-pass filter (PT1 filter, B_DTERM in config.h) — 2026-02-07
- [x] Rate mode derivative on measurement (all axes, both controllers) — 2026-02-07
- [x] USE_OPTIMIZATION implemented (biquad gyro LP, biquad D-term, gyro notch, accel 2nd LP) — 2026-02-09
- [x] USE_RACING implemented (feed-forward, TPA, expo curves, air mode, setpoint smoothing) — 2026-02-09
- [x] Biquad filter module (filters.h/cpp — reusable DSP primitives) — 2026-02-09
- [x] Build scripts dynamic environment parsing (build.bat rewrite, build.sh already dynamic) — 2026-02-09
- [x] OTA firmware updates (ArduinoOTA, Core 1, safety-gated by armedFly) — 2026-02-09
- [x] Serial PID tuning in calibration mode (runtime-tunable gains, line-buffered serial parser) — 2026-02-09
- [x] Display screen rotation for small OLEDs (128x32 cycles screens every 2s, 128x64 shows all) — 2026-02-09
- [x] Failsafe auto-detection calibration (`f` command) — 2026-02-09
- [x] ESC endpoint calibration (`e` command) — 2026-02-09
- [x] Magnetometer sphere calibration (MPU9250, `m` command) — 2026-02-09
- [x] Runtime filter/limits tuning (`p` command) — 2026-02-09
- [x] Wiring diagrams updated with OLED display connections — 2026-02-09

---

## Notes

- **Testing is hardware-based**: Tests are baked into the firmware as calibration modes and debug builds, not as separate test files. The firmware itself is the test harness. Each calibration routine validates its own results with quality checks and retry logic.
- **Calibration workflow**: Flash calibration build → run auto-calibration routines → copy `#define` values to config.h → flash live build → fly. This is by design — thorough automated calibration upfront means lean runtime.
- **Calibration automation goal**: Every hardware-dependent value in config.h should have a guided auto-calibration routine. No manual guesswork. The serial command interface in calibration builds is the primary calibration tool.
- **fc_tool synergy**: The fc_tool desktop app will provide visual diagnostics during calibration, making the calibrate → hard-code → flash cycle more user-friendly.
- **VTOL generality**: Always design features to work across vehicle types, not just quadcopters. The mixer pattern from dRehmFlight supports this well.
- **Bare bones + automation**: The firmware is intentionally minimal at runtime. It can afford to be simple because calibration is thorough. Automation has a large impact with minimal code.

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
