# Flight Controller Firmware - Roadmap

> Last updated: 2026-02-07

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

- [ ] D-term low-pass filter
  - Description: Add first-order low-pass filter on PID derivative term. Prevents motor oscillation from sensor noise being amplified into motor commands. This is the single highest-impact improvement for flight quality — every serious FC has this.
  - Notes: Simple alpha filter: `filtered_d = alpha * new_d + (1-alpha) * prev_d`. Add `D_TERM_LPF_ALPHA` to config.h. ~3 extra multiplications per axis per tick. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md).
  - Dependencies: None (pure code change)

- [ ] Rate mode derivative on measurement
  - Description: Change rate mode D-term from `(error - error_prev) / dt` to `-(measurement - measurement_prev) / dt`. Prevents "derivative kick" when setpoint changes rapidly (stick movement). Angle mode already does this correctly (uses `-GyroX` directly).
  - Notes: One-line change per axis in `controlRATE()`. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md).
  - Dependencies: None (pure code change)

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

> All features are config.h flags using `#ifdef`. When disabled, zero binary cost. Users enable only what their MCU can handle. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md) for detailed research.
>
> **Timing calculator**: `tools/timing_calculator.py` auto-detects enabled features from config.h. See [tools/timing/](../tools/timing/) for modular source. See "Timing Calculator Improvements" section below for planned enhancements.

### Feature Modules (config.h)

| Flag | Platform | Description | Core |
|------|----------|-------------|------|
| `USE_OLED_DISPLAY` | All | OLED display (status, network info) | Core 1 (ESP32) / main loop (Teensy) |
| `USE_WEB_SERVER` | ESP32 | Live value display in browser (JSON API, WebSocket, mDNS) | Core 1 |
| `USE_API_SERVER` | ESP32 | HTTP POST telemetry to centralized server for remote control | Core 1 |
| `USE_OPTIMIZATION` | All | Noise reduction: biquad filters, notch filter | Core 0 |
| `USE_RACING` | All | Betaflight-style: feed-forward, TPA, expo, air mode | Core 0 |

- **Web server** is best for calibration and bench testing (browse to floppi.local)
- **API server** is best for live flight and swarm coordination (POST to centralized server)
- Both are independently toggleable — not required to use together
- On dual-core ESP32, WiFi features run on Core 1 (zero flight loop impact)
- On single-core Teensy, WiFi features are compile-time excluded (no WiFi hardware)

### USE_OPTIMIZATION — Noise reduction for cheap hardware

Enable with `#define USE_OPTIMIZATION` in config.h. For builds using budget motors, unbalanced props, or flexible frames that produce more vibration. Adds better filtering to compensate for noisier hardware.

- [ ] Biquad gyro low-pass filter (configurable cutoff, steeper rolloff than PT1)
- [ ] D-term biquad filter (more aggressive derivative noise rejection)
- [ ] Configurable gyro notch filter (target specific motor noise frequencies)
- [ ] Accelerometer second-stage low-pass (attitude stability under vibration)

### USE_RACING — Performance features for aggressive flying

Enable with `#define USE_RACING` in config.h. Betaflight-inspired optimizations for sharper stick response and high-speed maneuvers. Not for beginners.

- [ ] Feed-forward term (faster stick response without increasing P gain)
- [ ] TPA — Throttle PID Attenuation (reduce gains at high throttle)
- [ ] Setpoint smoothing (smooth stick input transitions)
- [ ] Air mode (full PID at zero throttle for flips/rolls)
- [ ] Expo curves (non-linear stick response)

---

## Timing Calculator Improvements

> Current state: `tools/timing_calculator.py` with `tools/timing/` package. Simplified hardware input (clock/cores/FPU), auto-detects features from config.h, always outputs min/recommended clock speeds and pass/fail status.

### Implemented

- [x] **Simplified inputs** — Platform reduced to clock speed (MHz) + core count + FPU (yes/no). Two FPU profiles (hardware FPU vs software float) instead of per-platform cycle costs. CLI accepts `--clock`, `--cores`, `--fpu`/`--no-fpu` for arbitrary hardware. Platform presets remain as convenience shortcuts (`-p esp32`, `-p teensy40`).

- [x] **Pass/fail output** — Always outputs minimum clock required and recommended clock (+10% margin). Shows "NOT ENOUGH +X%" when platform clock is insufficient. Works with both presets and arbitrary hardware input.

- [x] **Auto-output of min/recommended clock speeds** — Every timing check prominently displays minimum and recommended clock speeds. No need to request these separately.

### Future Goals

- [ ] **Source code scanning** — Instead of manually maintaining a list of operations and their estimated cycle counts, scan the actual flight controller source files (src/, lib/) and trace the computation path through the code. Use computer science complexity analysis (count float operations, loops, function calls) to derive real numbers from real code. When new features are added to the firmware, the calculator automatically picks them up.

- [ ] **Build environment awareness** — Analyze different build environments (from platformio.ini) and config.h options to determine what code paths are active. Show the compute cost for each combination so users can compare: "base only" vs "base + optimization" vs "base + optimization + racing".

### Research Needed

- How to statically analyze C/C++ source for floating-point operation counts (AST parsing, regex heuristics, or compile-time instrumentation)
- Whether PlatformIO build system can provide intermediate files (preprocessed source, assembly) that make cycle counting easier
- Accuracy tradeoffs: static analysis vs actual profiling on hardware vs manual annotation

### Current Architecture

```text
tools/timing_calculator.py     # CLI entry point (--clock, --cores, --fpu/--no-fpu, -p preset)
tools/timing/
    platforms.py               # Platform dataclass (clock/cores/fpu) + FPU/soft-float cycle profiles
    operations.py              # Operation complexity definitions (TO BE REPLACED with source scanning)
    calc.py                    # Loop timing math, min clock calculation
    scanner.py                 # config.h feature detection
    report.py                  # Clean tabular output with pass/fail
```

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

- [ ] In-flight mode switching via radio channel
  - Description: **Likely out of scope** — flight computer territory. The flight computer commands the FC by sending pre-computed attitude targets. For manual flying, pick one mode at compile time.

- [ ] Wind compensation / disturbance rejection
  - Description: Mostly about PID tuning, not special algorithms. D-term filtering + good I-term tuning handles moderate wind. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md).
  - Dependencies: D-term low-pass filter

- [ ] ESC calibration serial command
  - Description: Add ESC endpoint calibration as a serial command in calibration build (e.g., `e`). Sends min/max PWM to all ESCs for range calibration. Uses the same single calibration build — no separate build needed.
  - Dependencies: Hardware testing

- [ ] WiFi AP mode fallback
  - Description: ESP32 creates its own WiFi access point for field use when no infrastructure WiFi is available. Each drone has its own AP (e.g., Floppi-AABBCC). Useful for single-drone field testing.
  - Notes: Archived code available in [docs/archive/wifi_ap_mode_implementation.md](../docs/archive/wifi_ap_mode_implementation.md)
  - Dependencies: WiFi STA mode (done)

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

- [ ] Live flight status display
  - Description: Show armed status, attitude, motor outputs. Screen rotation for multiple views.
  - Dependencies: Display module (done) + flight validation

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

- [ ] OTA firmware updates
  - Description: Update firmware over WiFi without USB connection
  - Dependencies: WiFi STA mode (done)

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

---

## Notes

- **Testing is hardware-based**: Tests are baked into the firmware as calibration modes and debug builds, not as separate test files. The firmware itself is the test harness.
- **Calibration workflow**: Flash calibration build → run calibration → copy values to config.h → flash live build → fly. This is by design — keeps live firmware lean.
- **fc_tool synergy**: The fc_tool desktop app will provide visual diagnostics during calibration, making the calibrate → hard-code → flash cycle more user-friendly.
- **VTOL generality**: Always design features to work across vehicle types, not just quadcopters. The mixer pattern from dRehmFlight supports this well.

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
