# Flight Controller Firmware - Roadmap

> Last updated: 2026-02-17

## Overview

This roadmap tracks project-level features and milestones for the flight controller firmware. For immediate tasks, see `todo.md`. For project boundaries, see `scope.md`.

**Note**: No time estimates. Focus on WHAT needs to be done, not WHEN.

**Current focus**: Feature development is paused (~90% complete). Priority is hardware validation, calibration, PID tuning, and first flight on real hardware. See `todo.md` for immediate tasks.

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

- [x] iBUS receiver support (FlySky FS-iA6B)
  - Completed: 2026-02-10
  - Notes: Inline parser (no external library). 115200 baud, 8N1, non-inverted. 32-byte frames, 14 channels, checksum validation. Voltage divider required (5V → 3.3V). Recommended protocol for FlySky hardware.

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

- [x] Calibration value dump — serial command `d`
  - Completed: 2026-02-10
  - Notes: Prints ALL calibration values from current session in one copy-paste-ready config.h block. CalibrationResults accumulator stores results as each calibration completes. Only prints sections that were calibrated. PID gains and filter params always included (from runtime values). Sequential workflow (`a`) suggests `d` at completion.
  - Outputs: Combined block with all calibrated sections (IMU, radio, failsafe, orientation, magnetometer, PID, filters)

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
  - Notes: IMU initialized OK on Teensy 4.0. AccZ reads -0.08g (MPU6050 mounted with X-axis pointing down). Run orientation detection (`o` command) to auto-detect and correct.

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

### Automated Test Infrastructure (High Priority)

> **Vision**: A modular test harness that can test any combination of board + firmware across all calibration commands. Uses Python (`serial_monitor.py`) and PlatformIO (`pio device monitor`) for serial communication — **no fc_tool dependency**. Runnable as CI-like pass/fail checks.
>
> **Serial tools**: `tools/serial_monitor.py` (Python/pyserial — works for ESP32 and Teensy), `pio device monitor` (PlatformIO built-in). No dependency on fc_tool for testing.

- [x] Calibration test suite for Teensy — `tests/test_calibration.sh`
  - Completed: 2026-02-12, rewritten 2026-02-13, expanded 2026-02-17
  - Notes: 18 test functions, **42 checks**, covering all calibration commands. Uses `serial_monitor.py` backend. Includes boot drain after `reboot_teensy()`, automatic CDC recovery (`teensy_reboot` on empty output), silence-based output drain, `check_absent()` negative helper. No fc_tool dependency. See [archive/bench-test-2026-02-17.md](archive/bench-test-2026-02-17.md).

- [x] Rewrite test harness to use Python serial (drop fc_tool dependency)
  - Completed: 2026-02-13
  - Description: Replaced fc_tool headless FIFO interaction in `test_calibration.sh` with `serial_monitor.py` (raw termios). Tests are self-contained within flight_controller/ with no cross-project dependency.
  - Pattern: `python3 tools/serial_monitor.py /dev/ttyACM0 --send h --wait 3 --output results/help.txt`

- [ ] Modular test runner architecture
  - Priority: High
  - Description: Refactor test suite into a modular framework. Separate test definitions from harness logic. Support test suites: `imu`, `radio`, `motors`, `telemetry`, `full`. Config-driven board/port/firmware selection.
  - Pattern: `./test_runner.sh --board teensy40 --suite imu` or `./test_runner.sh --board esp32 --suite full`
  - Key modules: `tests/lib/harness.sh` (port mgmt, serial_monitor.py interaction, board reset, assertions), `tests/suites/test_imu.sh`, `tests/suites/test_radio.sh`, `tests/suites/test_motors.sh`, etc.

- [ ] ESP32 test support
  - Priority: High
  - Description: Add ESP32 serial communication to the test harness. ESP32 uses standard USB-UART (CP2102/CH340) which works with pyserial natively. Board reset via RTS/DTR toggle (not teensy_reboot). Auto-detect board type from USB VID/PID.
  - Notes: `serial_monitor.py` works for ESP32. Test harness should abstract board-specific reset and connection logic.

- [ ] Motor/ESC test suite
  - Priority: Medium (blocked by hardware)
  - Description: Automated ESC calibration verification, motor spin-up test, PWM range validation. Safety: require explicit user confirmation before motor tests, assert props-off.

- [ ] Radio test suite
  - Priority: Medium (blocked by receiver hardware)
  - Description: Automated channel range verification, failsafe measurement, channel mapping validation. Requires radio transmitter.

- [ ] Auto-flash-and-test workflow
  - Priority: Medium
  - Description: Build firmware → flash to board → run test suite → report results. Single command: `./test_runner.sh --board teensy40 --suite imu --flash`. Eliminates manual build/flash/connect cycle.
  - Notes: PlatformIO handles build+flash (`pio run -e <env> -t upload`), test harness handles serial verification via `serial_monitor.py`.

- [ ] Test results archival
  - Priority: Low
  - Description: Machine-readable test output (JSON/TAP) alongside human-readable logs. Timestamped results in `tests/results/`. Git-ignored results, git-tracked test scripts.

### VTOL Configuration Support

- [x] Quadcopter X mixer
  - Completed: Pre-2026

- [ ] Configurable mixer for different VTOL types
  - Description: Easy-to-customize motor/servo mixing for hex, octo, fixed-wing, tiltrotor, etc.
  - Dependencies: Basic flight validated on quad
  - Notes: dRehmFlight already supports this pattern — formalize and document it

- [ ] Expand to 8 motor outputs for advanced VTOL configurations
  - Description: Scale from 6 to 8 motor slots (`m1`-`m8`) to support octocopters, tiltrotors, and hybrid VTOL vehicles that start in vertical hover and transition to glide/forward flight. Currently 6 motors + 7 servos. Adding 2 more motor outputs (pins + PWM + mixer slots) is straightforward.
  - Dependencies: Configurable mixer, hardware testing on quad first
  - Status: **Future research** — not implementing yet. Document mixer patterns for common configurations.
  - Notes: The complexity isn't in the motor count — it's in the mixer math. A quad X is 4 simple +/- combinations. A tiltrotor needs servo-driven motor tilt + transition logic (hover → cruise). That transition logic is flight computer territory. The FC just needs enough motor/servo outputs and a user-customizable mixer function. Keep the firmware's role simple: output N PWM signals based on PID + mixer math. The flight computer decides WHEN to transition and WHAT mixer weights to use.

- [ ] Example configurations for common builds
  - Description: Pre-made config files for popular drone types (quad X, quad +, hex, Y6, tricopter, octo X, tiltrotor)
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
  - Completed: 2026-02-06, expanded 2026-02-10
  - Notes: Split main.cpp into imu.cpp, control.cpp, motors.cpp, debug.cpp + globals.h. Further modularized 2026-02-10: calibration.cpp (1753 lines) split into 5 files (calibration.cpp core 287, calibration_imu.cpp 444, calibration_orientation.cpp 285, calibration_radio.cpp 375, calibration_hardware.cpp 347). Calibration mode code extracted from main.cpp (1091→490 lines) into calibration_mode.cpp (614 lines) + calibration_mode.h.
- [x] Serial command interface for calibration mode
  - Completed: 2026-02-06
  - Notes: Commands r/i/o/s/h for radio, IMU, orientation, status, help
- [x] fc_tool integration protocol
  - Completed: 2026-02-06
  - Notes: Added telemetry output functions (printIMUTelemetry, printFullTelemetry) compatible with fc_tool parser. Serial command 't' toggles telemetry modes.
  - Related: See [fc_tool/docs/features/serial-telemetry-protocol.md](/fc_tool/docs/features/serial-telemetry-protocol.md)
- [x] Complexity calculator (CPU timing, memory, source analysis)
  - Completed: 2026-02-10 (replaces timing_calculator)
  - Notes: `tools/complexity_calculator.py` entry point with `tools/complexity/` package. Dynamic source scanning (no manual operation lists). CPU timing from source-scanned FP ops, memory analysis from ELF builds, per-tier breakdown. Inputs: clock/cores/FPU (via `--clock`, `--cores`, `--fpu`/`--no-fpu`) or platform presets (`-p esp32`). Modes: `--full`, `--memory`, `--source`, `--all`, `--builds`.
- [x] Flash-and-run script (`tools/flash_and_run.sh`)
  - Completed: 2026-02-11
  - Notes: Builds, flashes PIO environment, waits for serial port, launches serial monitor. Default: `teensy40_calibration`. Can use `pio device monitor` or `serial_monitor.py`.
- [x] Setup permissions script (`setup_permissions.sh` at repo root)
  - Completed: 2026-02-11
  - Notes: Idempotent sudo script — installs Teensy/ESP32 udev rules, adds user to dialout/plugdev groups, reloads udev. Skips steps already done.

---

## Modular Feature System

> **Design**: Every feature is a `#define` flag in config.h. When disabled, the code is **completely excluded** from the binary — zero flash, zero RAM, zero CPU overhead. Users enable exactly what their MCU and use case need.
>
> **Independence**: All feature flags are orthogonal. Enable any combination without conflicts. Tested in all permutations (base only, optimization only, racing only, both, with/without WiFi features).
>
> **Platform awareness**: WiFi features (web server, API client, OTA) are ESP32-only and auto-enabled with `USE_WIFI`. Flight loop features (optimization, racing) work on all platforms. On ESP32, WiFi features run on Core 1 — **zero flight loop impact**.
>
> **Complexity calculator**: `tools/complexity_calculator.py` auto-detects enabled features from config.h and dynamically scans source code for CPU/memory analysis. See [tools/complexity/](../tools/complexity/).

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

## Complexity Calculator

> **Status**: Functional with automatic source scanning. No manual updates needed when firmware code changes.
>
> **Current state**: `tools/complexity_calculator.py` with `tools/complexity/` package. Dynamically scans all C/C++ source files for floating-point operations, maps them to feature tiers via `#ifdef` tracking, and calculates CPU timing from per-architecture cycle costs. Also analyzes PlatformIO build artifacts (ELF) for actual flash/RAM usage.
>
> **Usage**: `python3 tools/complexity_calculator.py --full` (CPU + memory + source scan), `--all` (compare platforms), `--memory` (build artifacts), `--source` (FP operation scan), `--builds` (list available builds). Supports custom hardware: `--clock 133 --cores 2 --no-fpu`.

---

## Hardware Usability (High Priority)

> Features that make the firmware easier to use with real hardware. Important for the "flash, calibrate, fly" experience. Configurable pins is top priority — users should configure everything in config.h.

- [x] Configurable pin definitions from config.h
  - Completed: 2026-02-10
  - Description: All pin definitions in pin_definitions.h and pin_definitions_esp32.h now use `#ifndef` guards. Users override any pin in config.h's "PIN OVERRIDES" section. Platform defaults remain as fallbacks.
  - Pattern: `#ifndef MOTOR_PIN_1` in pin_definitions.h, `#define MOTOR_PIN_1 25` in config.h to override.

- [x] Calibration guide with hardware requirements and stage-based test sequencing
  - Completed: 2026-02-10
  - Description: Step-by-step guide for incremental hardware setup (Stage 0-4). Each stage lists what hardware is needed, what calibrations to run, and what values you get. Feature tier calibration requirements documented (base/optimization/racing).
  - File: [features/calibration-guide.md](features/calibration-guide.md)

- [x] Calibration reset tool
  - Completed: 2026-02-10
  - Description: Python script that resets all calibration `#define` values in config.h to factory defaults. Preserves feature flags and non-calibration settings. Dry-run mode, confirmation prompt, custom config path.
  - File: `tools/calibration_reset.py`
  - Usage: `python3 tools/calibration_reset.py` (or `--dry-run` to preview, `--yes` to skip prompt)

- [x] Config.h calibration status indicators
  - Completed: 2026-02-10
  - Description: Each calibration section in config.h now has STATUS comments (UNCALIBRATED or defaults-are-safe) with references to the calibration guide stage and serial command. Users can scan config.h and immediately see what needs calibration.

- ~~OLED-guided calibration workflow~~ — **Dropped**. Serial-only calibration is sufficient with a proper test bench. OLED shows flight status, not calibration.

- [x] Sequential calibration workflow
  - Completed: 2026-02-10
  - Description: Guided "full calibration" mode via serial command `a`. Walks through IMU → radio → failsafe → ESC stages, skips already-completed stages (via CALIBRATED_* markers in config.h). Command `c` shows status checklist. Individual calibrations still work independently.
  - Notes: CALIBRATED_* #define markers in config.h track completion. Calibration reset tool re-comments markers.

- [ ] Wiring validation on startup
  - Description: Basic startup checks — detect if IMU is responding, if receiver is sending data, if OLED is connected. Report status on serial and display. Helps users catch wiring mistakes before attempting calibration.
  - Dependencies: Hardware testing

### Calibration Wrapper Script (High Priority)

> **Plan**: [docs/plans/calibrate-sh-plan.md](plans/calibrate-sh-plan.md)

- [x] `tools/calibrate.sh` — Menu-driven calibration wrapper for Linux/Mac
  - Completed: 2026-02-17
  - Description: Interactive bash wrapper around `serial_monitor.py`. 17 menu options (6 display, 6 calibration, 4 tuning, 1 guided workflow). Handles prerequisites (ModemManager, port detection, Python check). CLI mode for scriptable access (`./calibrate.sh PORT imu`).
  - Backend: `serial_monitor.py` with `--send CMD --wait N --interactive`.
  - Pattern: Follows `build.sh` conventions (colors, menu loop, CLI args).

- [ ] `tools/calibrate.bat` — Windows equivalent
  - Priority: Low (blocked by serial_monitor.py cross-platform support)
  - Description: Same menu structure as calibrate.sh but in batch syntax. Depends on making serial_monitor.py work on Windows (currently uses Linux-only termios/fcntl). Follows `build.bat` patterns.

- [x] Update calibration guide to reference calibrate.sh
  - Dependencies: calibrate.sh implementation

---

## RadioComm — Universal Command Layer (High Priority)

> **Vision**: RadioComm is the **single entry point** for all command/control input to the flight controller. Every source of commands — RC receiver, serial, I2C, WiFi API — flows through RadioComm. The flight controller only reads `channel_X_pwm` values and never knows or cares where they came from. This keeps the architecture clean and prevents command paths from getting messy.
>
> **Key insight**: The API web server is NOT a separate command path. It feeds INTO RadioComm, which feeds the flight controller. One entry point, one data format, one failsafe path.

**Current state**: RadioComm handles 5 RC protocols (SBUS, iBUS, DSM, PPM, PWM) + 3 command sources (serial, I2C, WiFi API). Single-source per build (default) or multi-source with `USE_COMMAND_ARBITRATION`. Modularized: radioComm.cpp (core/arbitration), radioComm_rc.cpp (RC protocols), radioComm_ext.cpp (external sources). All produce `channel_1_pwm` through `channel_6_pwm` in 1000-2000us format.

**Command sources:**

| Source | Interface | Config Flag | Notes |
| ------ | --------- | ----------- | ----- |
| SBUS | Serial (inverted) | `USE_SBUS_RECEIVER` | Current default. Done. |
| iBUS | Serial (115200) | `USE_IBUS_RECEIVER` | FlySky recommended. Done. |
| DSM/DSMX | Serial | `USE_DSM_RECEIVER` | Spektrum. Done. |
| PPM | Single GPIO | `USE_PPM_RECEIVER` | Legacy. Done. |
| PWM | 6 GPIOs | `USE_PWM_RECEIVER` | Legacy. Done. |
| Serial commands | UART | `USE_SERIAL_COMMANDS` | Done. Binary protocol, 15-byte frames. |
| I2C commands | I2C slave | `USE_I2C_COMMANDS` | Done. FC as slave on Wire1, 12-byte frames. |
| WiFi API | HTTP/WebSocket | `USE_WEB_SERVER` | Done. POST /api/commands + WebSocket. |

- [x] Serial command input (`USE_SERIAL_COMMANDS`)
  - Completed: 2026-02-10
  - Description: Accept channel values over serial UART from an external flight computer. Binary protocol: 15-byte frames `[0xAA 0x55] [6×uint16 LE channels] [XOR checksum]`. 115200 baud, 8N1. Timeout-based failsafe (500ms).
  - Pattern: Same `getCommands()` function, inline parser matching iBUS pattern.
  - Pin: Configurable in config.h (defaults in pin_definitions.h). Serial3 (Teensy), Serial1 (ESP32).

- [x] I2C command input (`USE_I2C_COMMANDS`)
  - Completed: 2026-02-10
  - Description: Accept channel values over I2C with the FC as I2C slave (address 0x42) on Wire1. Flight computer (I2C master) writes 12 bytes: 6× uint16_t LE, 1000-2000us. ISR-based receive, noInterrupts/interrupts for getCommands() read.
  - Pin: Configurable in config.h (defaults: Teensy Wire1 SDA=17/SCL=16, ESP32 SDA=25/SCL=26, ESP32-S3 SDA=41/SCL=42). Must use different I2C bus than IMU.

- [x] WiFi API command routing
  - Completed: 2026-02-10
  - Description: Route commands received by the ESP32 web server (via HTTP POST or WebSocket) through RadioComm. Web server on Core 1 writes to shared command buffer (spinlock-protected), RadioComm on Core 0 reads it in `getCommands()`.
  - Endpoints: POST `/api/commands` (JSON), WebSocket `/ws` (JSON). Format: `{"ch1":1500,"ch2":1500,"ch3":1000,"ch4":1500,"ch5":1000,"ch6":1000}`.
  - Works as sole command source when no RC receiver is defined (ESP32 + Web API progression path). Failsafe: 500ms timeout.

- [x] Command source arbitration
  - Completed: 2026-02-11
  - Description: Priority-based selection when multiple command sources are active. RC = primary, serial/I2C/WiFi = overrides. `USE_COMMAND_ARBITRATION` flag in config.h. CommandBuffer struct per source, 500ms timeout. RadioComm modularized into 3 files (core, rc protocols, external sources).
  - Design doc: [findings/command-arbitration-design.md](findings/command-arbitration-design.md)

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

### External Controller App (Built — `swarm_api/`)

> A standalone Python application outside `flight_controller/` for controlling ESP32 drones over WiFi. This is NOT part of the flight controller firmware — it's a separate project that talks to the FC via its WiFi API.

- [x] Web dashboard for basic drone controls
  - Completed: 2026-02-10
  - Description: Browser-based UI with fleet panel, throttle/roll/pitch/yaw sliders, real-time telemetry. Sends commands via WebSocket (primary, 10Hz) with HTTP POST `/api/commands` fallback.
  - Stack: Python 3.10+, FastAPI, uvicorn, httpx, websockets, zeroconf

- [x] Config file with drone registry
  - Completed: 2026-02-10
  - Description: `config.json` with ESP32 MAC addresses, mDNS hostnames, network settings. mDNS discovery (floppi-XXXX.local) + IP fallback.

- [ ] Computation offloading
  - Description: ESP32 Core 1 can request computation from the host (e.g., path planning, sensor fusion). Host returns results over WiFi. Keeps ESP32 firmware lean.
  - Notes: Future enhancement. Current swarm_api sends commands only.

**Run**: `cd swarm_api && pip install -r requirements.txt && python3 -m uvicorn src.main:app --host 0.0.0.0 --port 8080`

**Progression path**: Teensy+FS-iA6B (manual RC) → ESP32+FS-iA6B (RC + WiFi telemetry) → ESP32+Web API (WiFi-only control)

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
- [x] Complexity calculator (replaces timing_calculator, dynamic source scanning, memory analysis) — 2026-02-10
- [x] Sequential calibration workflow (`a` command, CALIBRATED_* markers) — 2026-02-10
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
- [x] Configurable pin definitions (`#ifndef` guards, config.h PIN OVERRIDES section) — 2026-02-10
- [x] Calibration guide with hardware requirements and test sequencing — 2026-02-10
- [x] Calibration reset tool (`tools/calibration_reset.py`) — 2026-02-10
- [x] Config.h calibration status indicators (STATUS: UNCALIBRATED markers) — 2026-02-10
- [x] OLED I2C address auto-detection (software I2C scan for 0x3C/0x3D) — 2026-02-11
- [x] Blocking calibration input (waitForConfirmation blocks indefinitely, LED blink, no timeouts) — 2026-02-11
- [x] CH6 calibration trigger debounce (must go LOW before re-trigger) — 2026-02-11
- [x] fc_tool autoscroll fix (pauses during text selection) — 2026-02-11
- [x] Calibration value dump (`d` command — all values in one config.h block) — 2026-02-10
- [x] Calibration library modularization (calibration.cpp 1753→5 files, main.cpp 1091→490 lines) — 2026-02-10
- [x] iBUS receiver support (FlySky FS-iA6B, inline parser, checksum validation) — 2026-02-10
- [x] Wiring diagrams reorganized into `docs/wiring_diagrams/` with build-specific guides — 2026-02-10
- [x] Hardware architecture vision documented (modular base system, progression path) — 2026-02-10
- [x] Serial command input (`USE_SERIAL_COMMANDS`) — binary protocol from external flight computer — 2026-02-10
- [x] WiFi API command routing — POST /api/commands + WebSocket, spinlock cross-core buffer — 2026-02-10
- [x] I2C command input (`USE_I2C_COMMANDS`) — FC as I2C slave on Wire1, 12-byte frames — 2026-02-10
- [x] Command source arbitration design doc — [findings/command-arbitration-design.md](findings/command-arbitration-design.md) — 2026-02-10
- [x] Command source arbitration implementation (`USE_COMMAND_ARBITRATION`, CommandBuffer, priority logic) — 2026-02-11
- [x] RadioComm library modularization (663 lines → 3 files: core, rc protocols, external sources) — 2026-02-11
- [x] OLED startup build info (platform + receiver + mode tags, no delay) — 2026-02-11
- [x] LED_BUILTIN → LED_PIN fix in calibration.cpp (ESP32 build fix) — 2026-02-11
- [x] IMU calibration bug fixes from bench testing (variance-based stability check, relaxed thresholds) — 2026-02-12
- [x] Serial monitor rewritten with raw termios (dropped pyserial, POSIX termios matching serialport-rs) — 2026-02-13
- [x] Calibration test suite rewritten to use serial_monitor.py (11 tests, no fc_tool dependency) — 2026-02-13
- [x] `tools/calibrate.sh` menu-driven calibration wrapper (17 options, CLI mode, auto port detection) — 2026-02-17
- [x] Test suite expanded to 18 tests (42 checks), tightened assertions, failure diagnostics — 2026-02-17
- [x] `waitForConfirmation()` dead parameter removed (~45 call sites across 7 files) — 2026-02-17
- [x] serial_monitor.py reliability fixes (kernel flush, silence-based drain, `--wait-for`, `--quiet`, exit code 2) — 2026-02-17
- [x] Test suite boot drain (wait for "READY" after `reboot_teensy`) — 2026-02-17
- [x] Test suite CDC recovery (auto `teensy_reboot` on empty output from USB CDC degradation) — 2026-02-17
- [x] Bench test: OLED SSD1306 128x32, IMU MPU6050, telemetry, all serial commands — 42/42 pass — 2026-02-17

---

## Notes

- **Testing is hardware-based**: Tests are baked into the firmware as calibration modes and debug builds, not as separate test files. The firmware itself is the test harness. Each calibration routine validates its own results with quality checks and retry logic.
- **Serial tools**: `tools/calibrate.sh` (primary, menu-driven) wraps `tools/serial_monitor.py` (Python, raw termios). `pio device monitor` (PlatformIO) as fallback. No fc_tool dependency. **NEVER use raw bash for serial** (cat, stty, echo > /dev) — always use calibrate.sh, serial_monitor.py, or PlatformIO.
- **Calibration workflow**: Flash calibration build → run auto-calibration routines → copy `#define` values to config.h → flash live build → fly. This is by design — thorough automated calibration upfront means lean runtime.
- **Calibration automation goal**: Every hardware-dependent value in config.h should have a guided auto-calibration routine. No manual guesswork. The serial command interface in calibration builds is the primary calibration tool.
- **VTOL generality**: Always design features to work across vehicle types, not just quadcopters. The mixer pattern from dRehmFlight supports this well.
- **Bare bones + automation**: The firmware is intentionally minimal at runtime. It can afford to be simple because calibration is thorough. Automation has a large impact with minimal code.

---

*Update as features complete. Check boxes when done. Add new features as they're identified.*
