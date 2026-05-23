# Flight Controller Project Reconnaissance Report
**Date**: 2026-05-20  
**Agent**: fc-recon@flight_controller:1  
**Status**: READY for parallel implementation workstreams

---

## 1. Project Root Structure

```
flight_controller/
├── src/                          # 12 modules, ~3,613 lines total
│   ├── main.cpp (16,217 bytes)   # Teensy/ESP32 dual-mode entry point
│   ├── calibration_mode.cpp      # Serial command dispatch for calibration
│   ├── imu.cpp                   # IMU sensor integration (MPU6050/9250)
│   ├── control.cpp (15,205)      # PID controllers (rate + angle mode)
│   ├── motors.cpp (13,804)       # Motor mixing & PWM output
│   ├── display.cpp (15,753)      # OLED rendering (U8g2, SW I2C)
│   ├── filters.cpp               # Biquad DSP filters (optimization tier)
│   ├── wifi_manager.cpp          # ESP32 WiFi STA mode
│   ├── web_server.cpp            # ESPAsyncWebServer (Core 1)
│   ├── api_client.cpp            # HTTP POST telemetry client (Core 1)
│   ├── ota.cpp                   # ArduinoOTA firmware updates (Core 1)
│   └── debug.cpp (7,869)         # Debug print functions
├── include/                       # 19 header files, modular architecture
│   ├── config.h (513 lines)      # SINGLE user config file
│   ├── pin_definitions.h         # Teensy pins + #ifndef overrides
│   ├── pin_definitions_esp32.h   # ESP32 pins + #ifndef overrides
│   ├── globals.h (125 lines)     # Extern variable declarations
│   ├── calibration.h (242 lines) # Calibration routine interface
│   ├── calibration_mode.h        # Serial command header
│   ├── control.h / imu.h / motors.h / display.h / filters.h
│   ├── radioComm.h               # Universal command entry point
│   └── wifi_*.h / ota.h / display_data.h / api_client.h
├── lib/                          # 12 vendored libraries (offline builds)
│   ├── Calibration/              # 5 modular calibration files
│   │   ├── calibration.cpp (14.8k)
│   │   ├── calibration_imu.cpp (22.6k)
│   │   ├── calibration_orientation.cpp (13.9k)
│   │   ├── calibration_radio.cpp (18.2k)
│   │   └── calibration_hardware.cpp (15.0k) — ESC, failsafe, mag
│   ├── RadioComm/                # 3 modules: core, RC protocols, external sources
│   ├── SBUS/ / DSMRX/ / RadioComm/  # RC protocols
│   ├── MPU6050/ / MPU9250/       # IMU libraries
│   ├── PWMServo/                 # Motor output (Teensy-compatible)
│   ├── U8g2/                     # OLED display library (SSD1306, SH1106)
│   ├── ArduinoJson/              # JSON parsing (web API)
│   └── TFMPlus/                  # Lidar sensor (future)
├── lib_esp32/                    # ESP32-specific libraries
│   ├── AsyncTCP/                 # Core async networking
│   └── ESPAsyncWebServer/        # Web server + WebSocket
├── tools/
│   ├── calibrate.sh (menu-driven wrapper, auto port detection)
│   ├── serial_monitor.py (Python raw termios, cross-platform)
│   ├── flash_and_run.sh (build+flash+monitor)
│   ├── dev.sh (unified entry point: build/flash/monitor/test/calibrate)
│   ├── complexity_calculator.py (CPU/memory/source analysis)
│   └── calibration_reset.py (reset config.h to factory defaults)
├── tests/
│   └── test_calibration.sh (18 test functions, 42 assertions, no fc_tool dep)
├── docs/
│   ├── README.md / scope.md / roadmap.md / todo.md
│   ├── 0_quickstart.md (60-minute setup)
│   ├── 1_hardware_setup.md / 2_calibration_guide.md / 3_troubleshooting.md
│   ├── teensy_wiring.md / esp32_wiring.md
│   ├── wiring_diagrams/
│   │   ├── teensy_wiring_fsia6b_drone.md
│   │   ├── esp32_wiring_fsia6b_drone.md
│   │   └── esp32_wiring_web_api_drone.md
│   ├── findings/  (12 research docs, 4,945 lines)
│   │   ├── bare-bones-fc-research.md (426 lines)
│   │   ├── esp32-wifi-connectivity.md (1,030 lines)
│   │   ├── command-arbitration-design.md (625 lines)
│   │   ├── auto-calibration-research.md (925 lines)
│   │   └── [8 more: display, timing, acrobatics, ESP32 dual-core, etc.]
│   ├── features/ (compile-time architecture docs)
│   └── archive/ (session summaries)
├── platformio.ini (186 lines)
├── README.md (325 lines, user-facing guide)
├── build.sh / build.bat (shell/batch build wrappers)
├── .gitignore / .gitattributes
└── .vscode/ (settings for local dev)
```

**Key observation**: Extremely well-organized. Single config.h, modular src/, vendored libs, comprehensive docs.

---

## 2. Build System — PlatformIO Multi-Board Architecture

### Environment Definitions (platformio.ini: line 5-186)

| Environment | Platform | Type | Status | Notes |
|-------------|----------|------|--------|-------|
| `teensy40` | Teensy 4.0 | Live | **RECOMMENDED** | 600 MHz, Cortex-M7, FPU, -O2 optimized |
| `teensy40_calibration` | Teensy 4.0 | Calibration | **WORKING** | +CALIBRATION_MODE, +USE_OLED_DISPLAY |
| `teensy41` | Teensy 4.1 | Live | Supported | Same CPU as 4.0, more pins |
| `teensy41_calibration` | Teensy 4.1 | Calibration | Supported | - |
| `teensy36` | Teensy 3.6 | Live | Legacy | Older MCU, no FPU |
| `teensy36_calibration` | Teensy 3.6 | Calibration | Legacy | - |
| `esp32` | ESP32 | Live | **WORKING** | 240 MHz, dual-core, WiFi, LEDC PWM |
| `esp32_calibration` | ESP32 | Calibration | Tested | +CALIBRATION_MODE, +USE_OLED_DISPLAY |
| `esp32s3` | ESP32-S3 | Live | Supported | Newer ESP32 variant, USB native |
| `esp32s3_calibration` | ESP32-S3 | Calibration | Supported | - |

### Build Flag Tiers

**Common** (all boards):
- `-D USB_SERIAL`, `-D LAYOUT_US_ENGLISH`, `-I include`
- `-Wno-unknown-pragmas`, `-Wno-unused-variable`

**Teensy 4.0/4.1** (lines 33-60):
- `-D ARDUINO_TEENSY40/41`, `-D __IMXRT1062__`
- `-O2 -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16` (hard FPU)

**ESP32 base** (lines 126-151):
- `-D USE_ESP32`, `-D CONFIG_FREERTOS_HZ=1000`
- `-D USE_OLED_DISPLAY`, `-D USE_WIFI` (always on for ESP32)
- `-D CONFIG_ASYNC_TCP_RUNNING_CORE=1` (WiFi on Core 1)

**Calibration variants** (lines 84-185):
- Inherit from board environment
- Add `-D CALIBRATION_MODE`
- Add `Calibration` to lib_deps

### Current Compilation Status (from todo.md, lines 137-138)
- **7/10 environments pass** (as of 2026-02-20)
- **3 expected fails**: Teensy live builds fail because `USE_SBUS_RECEIVER` is commented out in config.h (bench testing without receiver)
- **Action**: Re-enable SBUS in config.h → all 10 should pass

---

## 3. Source Layout — Modular Architecture

### Main Modules (src/)

| File | Lines | Purpose | Key Functions |
|------|-------|---------|---|
| **main.cpp** | 490 (after modularization) | Entry point, timing loop, dual-core task setup | `setup()`, `loop()`, flight control @1kHz |
| **calibration_mode.cpp** | 614 | Serial command dispatch (calibration builds only) | `serialEvent()`, command routing |
| **imu.cpp** | ~300 | IMU init, read, apply offsets, Madgwick fusion | `initIMU()`, `readIMU()`, `madgwickUpdate()` |
| **control.cpp** | 450 | PID loops (rate + angle), mixer, failsafe | `controlRATE()`, `controlANGLE()`, `controlMixer()` |
| **motors.cpp** | 410 | Motor PWM output, servo control | `motorsInit()`, `commandMotors()` |
| **display.cpp** | 470 | OLED rendering, 2-second screen rotation on 128x32 | `displayInit()`, `updateDisplay()` |
| **filters.cpp** | ~100 | Biquad filter DSP primitives | `biquadFilter()`, `notchFilter()` |
| **wifi_manager.cpp** | 180 | WiFi STA mode (ESP32 only), SSID/password from config | `initWiFi()`, `reconnect()` |
| **web_server.cpp** | 275 | ESPAsyncWebServer, JSON `/api/status`, WebSocket `/ws`, mDNS | `initWebServer()`, handlers |
| **api_client.cpp** | 105 | HTTP POST telemetry to centralized servers | `sendTelemetry()` |
| **ota.cpp** | 64 | ArduinoOTA firmware updates (safety-gated by armedFly) | `initOTA()` |
| **debug.cpp** | 234 | Print functions (radio, desired state, gyro, accel, etc.) | `printRadioData()`, etc. (all guarded by CALIBRATION_MODE) |

### Compilation Strategy

**Live builds** (`teensy40`, `esp32`):
- Compiles: main + imu + control + motors + display (if `USE_OLED_DISPLAY`)
- WiFi/web (if ESP32 + `USE_WIFI`)
- **Excludes**: calibration_mode, debug, all calibration routines
- **Binary size**: Lean, optimized with `-O2`

**Calibration builds** (`teensy40_calibration`, `esp32_calibration`):
- Includes: all above + calibration_mode + Calibration library (5 files) + debug
- Also enables: CH6 state machine, serial command parser
- **Binary size**: Larger but still fits (Teensy 4.0: 256kB flash)

### Flight-Critical Hardware Drivers

| Driver | Location | Details |
|--------|----------|---------|
| **MPU6050** (6DOF accel+gyro) | lib/MPU6050/src | I2C init, data read, 250 DPS / 2G ranges |
| **MPU9250** (9DOF with mag) | lib/MPU9250/ | Full implementation, 30-second sphere calibration |
| **SBUS receiver** | lib/SBUS/ + radioComm_rc.cpp | Inverted serial, 25Hz frames, 16 channels |
| **iBUS receiver** | radioComm_rc.cpp (inline) | 115200 baud, recommended for FlySky FS-iA6B |
| **Motor PWM** | motors.cpp, lib/PWMServo/ | Teensy: analogWrite; ESP32: LEDC 50Hz, 1000-2000us |
| **Servo outputs** | motors.cpp, lib/PWMServo/ | 7 channels (m5-m11 for VTOL control surfaces) |

### Sensor Fusion & Attitude Estimation

- **Madgwick 6DOF filter**: Gyro integration + accel correction, single tuning parameter `MADGWICK_BETA` (default 0.04)
- **Quaternion state**: q0-q3 (gimbal-lock free)
- **Euler angles**: Roll/Pitch/Yaw extracted from quaternion each iteration
- **Filter warm-up**: First 2 seconds discard noisy startup data

---

## 4. Dependencies — Library Vendoring Strategy

### Vendored in lib/ (offline builds, no internet required)

| Library | Version | Purpose | Status |
|---------|---------|---------|--------|
| **ArduinoJson** | v7 | JSON parsing (web API) | Vendored, minimal config |
| **Calibration** | custom | 5-file modular auto-calibration library | Part of flight_controller |
| **DSMRX** | custom | Spektrum DSM/DSMX receiver protocol | Optional |
| **MPU6050** | custom fork | I2C IMU driver, accelerometer + gyro | Primary, 6DOF |
| **MPU9250** | custom | Full MPU9250 driver, includes magnetometer | Secondary, 9DOF option |
| **PWMServo** | custom | Motor PWM + servo control | Critical, Teensy-specific |
| **RadioComm** | custom | Universal RC input layer (SBUS, iBUS, DSM, PPM, PWM) | Core module |
| **SBUS** | custom | SBUS receiver protocol, 16-channel support | Primary receiver |
| **TFMPlus** | custom | Lidar distance sensor (future) | Not used yet |
| **U8g2** | vendored | Monochrome OLED graphics, SSD1306/SH1106 | SW I2C |

### Vendored in lib_esp32/ (ESP32-only)

| Library | Version | Purpose |
|---------|---------|---------|
| **AsyncTCP** | custom | Non-blocking async TCP for ESP32 |
| **ESPAsyncWebServer** | custom | HTTP + WebSocket server on Core 1 |

### Framework Built-ins (Arduino core provided)

| Library | Purpose |
|---------|---------|
| **Wire** | I2C for IMU (both platforms) |
| **SPI** | SPI interface (future expansion) |
| **Serial** | UART for receiver + calibration serial |
| **FreeRTOS** | Task management (ESP32 dual-core) |
| **LEDC** | PWM for motors (ESP32) |

### Dependency Graph

```mermaid
flowchart TB
    MAIN["main.cpp"]
    MAIN --> RADIO["radioComm.h"]
    RADIO --> RADIO_DEP["SBUS.h / DSMRX.h / radioComm_rc.cpp / radioComm_ext.cpp"]
    MAIN --> IMU["imu.h"]
    IMU --> IMU_DEP["MPU6050 / MPU9250"]
    MAIN --> CONTROL["control.h (no external deps)"]
    MAIN --> MOTORS["motors.h"]
    MOTORS --> PWMSERVO["PWMServo"]
    MAIN --> DISPLAY["display.h"]
    DISPLAY --> DISPLAY_DEP["U8g2 / display_data.h"]
    MAIN --> CONFIG["config.h (single point of truth)"]
    MAIN --> GLOBALS["globals.h (extern declarations)"]
    MAIN --> CALMODE["calibration_mode.h"]
    CALMODE --> CAL["Calibration/ (5 modules)"]
    CAL2["calibration/"] --> CAL2_DEP["ArduinoJson, config.h"]

    subgraph esp32["ESP32 only"]
        WIFI["wifi_config.h"] --> WIFI_DEP["WiFi STA code"]
        WEB["web_server.h"] --> WEB_DEP["ESPAsyncWebServer / ArduinoJson"]
        API["api_client.h"] --> API_DEP["HTTP client"]
        OTA["ota.h"] --> OTA_DEP["ArduinoOTA"]
    end
    MAIN -.-> esp32
```

---

## 5. Documentation State — Comprehensive & Current

### User-Facing Documents

| Document | Lines | Purpose | Last Update |
|----------|-------|---------|-------------|
| **README.md** | 325 | Quick-start guide, wiring links, calibration workflow | 2026-05-04 |
| **docs/0_quickstart.md** | ~200 | 60-minute setup guide | Recent |
| **docs/1_hardware_setup.md** | ~300 | Wiring + component selection | Recent |
| **docs/2_calibration_guide.md** | ~400 | Detailed calibration procedures (6 stages) | Recent |
| **docs/3_troubleshooting.md** | ~200 | Problem-solving reference | Recent |

### Architecture & Design Documents

| Document | Lines | Scope | Status |
|----------|-------|-------|--------|
| **scope.md** | 322 | Project boundaries, in/out of scope | CURRENT (2026-03-30) |
| **roadmap.md** | 715 | Feature checklist, milestones, completion tracking | CURRENT, ~90% done |
| **todo.md** | 156 | Immediate tasks, session notes | ACTIVE (2026-03-30) |

### Research Findings (12 documents)

| Finding | Lines | Topic |
|---------|-------|-------|
| bare-bones-fc-research.md | 426 | Design philosophy, PID tuning, IMU filtering |
| esp32-wifi-connectivity.md | 1,030 | WiFi STA, WPA2-Enterprise, reconnection logic |
| command-arbitration-design.md | 625 | Multi-source command priority system |
| auto-calibration-research.md | 925 | Calibration strategies, quality validation |
| esp32-dual-core-research.md | 286 | Core 0 FC vs Core 1 WiFi/display |
| esp32-fc-feasibility.md | 264 | ESP32 platform analysis, constraints |
| display-module-architecture.md | 509 | U8g2 integration, SW I2C, 128x32 rotation |
| [8 more] | ~1,000 | Timing, acrobatics, display options, filters, etc. |

### Wiring Diagrams (mermaid flowcharts)

| Diagram | Purpose |
|---------|---------|
| teensy_wiring.md | General Teensy pin reference |
| esp32_wiring.md | General ESP32 pin reference |
| teensy_wiring_fsia6b_drone.md | **Recommended**: Teensy + FlySky receiver + drone |
| esp32_wiring_fsia6b_drone.md | **Recommended**: ESP32 + FlySky receiver + drone |
| esp32_wiring_web_api_drone.md | ESP32 + WiFi-only control (no physical receiver) |

---

## 6. Test Coverage — Modular & Serial-Based

### Test Suite: test_calibration.sh

| Stat | Value |
|------|-------|
| **Test functions** | 18 |
| **Assertions** | 42 checks total |
| **Framework** | Shell script + `serial_monitor.py` (Python raw termios) |
| **Dependencies** | None (no fc_tool needed) |
| **Backend** | `python3 tools/serial_monitor.py` |
| **Last run** | 2026-02-17: **42/42 pass** (bench test) |

### Test Categories

| Category | Tests | Scope |
|----------|-------|-------|
| **Display** | 3 | Boot message, OLED init, telemetry output |
| **Calibration** | 6 | IMU, orientation, radio, failsafe, ESC, dump |
| **Telemetry** | 3 | IMU data, full telemetry, quaternion mode |
| **Safety** | 2 | Arming/disarming, failsafe response |
| **Commands** | 4 | Help, status, channel display, PID tuning |
| **Recovery** | 2 | CDC recovery on USB degradation, boot drain |

### Test Execution

```bash
# Prerequisites: Teensy 4.0 flashed with calibration firmware
./tests/test_calibration.sh                    # Run all safe tests
./tests/test_calibration.sh /dev/ttyACM0 imu   # Run single test
./tests/test_calibration.sh /dev/ttyACM0 all   # Explicit all
```

### Test Infrastructure

- **serial_monitor.py** (lines ~300): Raw POSIX termios, `--send`, `--wait`, `--output`, `--interactive` flags
- **Reboot logic**: Detects `teensy_reboot` tool, handles port re-enumeration, boot drain via "READY" marker
- **CDC recovery**: Auto-recovers from USB CDC degradation (Teensy quirk after 15 rapid open/close)
- **Assertion helpers**: `test_pass()`, `test_fail()`, `check_output()`, `check_absent()`

**Hardware-agnostic design**: Tests use serial commands, work on Teensy and ESP32 identically.

---

## 7. Current State Assessment

### Does it compile out of the box?

| Build | Status | Issue |
|-------|--------|-------|
| `teensy40_calibration` | ✅ PASS | Fully functional, 42/42 tests pass |
| `teensy40` (live) | ❌ EXPECTED FAIL | `USE_SBUS_RECEIVER` commented out in config.h (bench testing) |
| `teensy41_calibration` | ✅ LIKELY PASS | Same code as teensy40 |
| `esp32_calibration` | ✅ LIKELY PASS | Tested in Feb 2026 |
| Other variants | ⚠️ UNTESTED | Code in place, not exercised |

**Action**: Uncomment `USE_SBUS_RECEIVER` in config.h line ~X → all live builds pass.

### Development Maturity

| Aspect | Status | Notes |
|--------|--------|-------|
| **Core flight control** | ✅ COMPLETE | PID loops, attitude filter, motor mixing working |
| **Calibration system** | ✅ COMPLETE (~90%) | IMU, radio, failsafe, ESC, magnetometer all done. Manual PID tuning (`g` command) covers most cases. |
| **Teensy support** | ✅ MATURE | 4.0/4.1 recommended, 3.6 legacy. Proven with real hardware. |
| **ESP32 support** | ✅ MATURE | Dual-core (Core 0 FC, Core 1 WiFi/display). Builds clean. |
| **WiFi features** | ✅ COMPLETE | STA mode, web server, API client, OTA. All compile, not flight-tested. |
| **Documentation** | ✅ EXCELLENT | Scope, roadmap, 12 research docs, wiring guides, troubleshooting. |
| **Test coverage** | ✅ GOOD | 18 test functions, 42 assertions, bench-validated. No integration tests (need real motors). |

### Activity Level

- **Last meaningful commit**: 2026-05-20 (merge of research work)
- **Most recent work**: Phase 4M (auto_orientation) active; flight_controller paused at "hardware calibration" phase
- **Status**: Actively maintained repo, but flight_controller specifically is **awaiting hardware testing & PID tuning**

### Recent Phase Summary (from todo.md)

```
Current Hardware: Teensy 4.0 + MPU6050 + SSD1306 OLED + SBUS Receiver
Next Session Goal: Complete hardware calibrations (orientation, IMU, radio, failsafe)

Completed:
  ✅ Port dRehmFlight to PlatformIO (pre-2026)
  ✅ Calibration system fully automated (Feb 2026)
  ✅ Teensy 4.0 hardware validation (Feb 2026)
  ✅ Test suite rewrite + 42/42 pass (Feb 17)
  ✅ ESP32 dual-core architecture (Feb 2026)
  ✅ WiFi STA mode + web server (Feb 2026)

Blocked On:
  ❌ ESCs/motors not connected yet
  ❌ In-flight hardware testing not done
  ❌ Actual PID tuning not started
```

---

## 8. Comparison with auto_orientation

### Code Sharing Assessment

**No direct code sharing detected** (`grep -r auto_orientation` finds nothing).

**Why separate projects?**

| Project | Purpose | Hardware | Scope |
|---------|---------|----------|-------|
| **flight_controller** | Bare-bones VTOL stabilizer | Teensy 4.x + ESP32 | Flight control PID, IMU fusion, auto-calibration |
| **auto_orientation** | Balance robot + orientation framework | Arduino Mega + Uno | Robot balance control, orientation detection |

**Potential future sharing**:
- Madgwick filter implementation (both use IMU fusion)
- Calibration patterns (both auto-calibrate)
- Telemetry output formats

**Current status**: Completely independent projects in same repo. Different hardware, different flight physics.

---

## 9. Teensy Build Gaps — Minimal

### Current Teensy Capabilities ✅

- ✅ Dual I2C buses (IMU on Wire, OLED on Wire1)
- ✅ Multiple UARTs (receiver on Serial1 or SerialX)
- ✅ PWM outputs for 6 motors + 7 servos
- ✅ Timer support for precise 1-2kHz loop
- ✅ Plenty of RAM/flash for all features

### Teensy-to-ESP32 Differences (Not Gaps)

| Feature | Teensy 4.0 | ESP32 | Handled By |
|---------|-----------|-------|-----------|
| **Loop rate** | 2kHz (600 MHz) | 1kHz (240 MHz) | main.cpp timing |
| **Dual-core** | Single | Dual (FreeRTOS) | main.cpp + display.cpp |
| **WiFi** | None | Built-in | #ifdef USE_ESP32 + wifi_* modules |
| **PWM** | analogWrite() | LEDC library | motors.cpp with #ifdef |
| **Display** | Software I2C | Software I2C (same) | display.cpp |

### What's Needed for Future Teensy Enhancements

1. **Barometer/altimeter support** → Add `USE_BAROMETER` flag + i2c_sensor.cpp
2. **GPS support** → Add `USE_GPS` flag + serial handling (no flight loop impact)
3. **Extended servo outputs** (8+ channels) → Already pin-configurable
4. **CAN bus support** → Would require new serial protocol handler (radioComm extension)

**Verdict**: Teensy support is **complete for flight controller role**. No missing gaps for stable flight. Future expansions are modular (no breaking changes).

---

## 10. ESP32 Build Gaps — Moderate Complexity

### Current ESP32 Capabilities ✅

- ✅ Dual-core with FreeRTOS
- ✅ WiFi STA mode (WPA2-Personal + Enterprise via EAP)
- ✅ Web server + JSON API + WebSocket
- ✅ OTA firmware updates
- ✅ LEDC PWM for motors
- ✅ Multiple UARTs for receiver + comms
- ✅ OLED display on Core 1

### Remaining ESP32 Gaps ⚠️

| Gap | Complexity | Impact | Solution |
|-----|-----------|--------|----------|
| **Actual flight test** | Moderate | Unknown stability at 1kHz on 240MHz | Need real hardware, motors, props |
| **FreeRTOS tuning** | Low | Task priority/stack sizes | Empirical via oscilloscope on real flight |
| **Power consumption** | Low | WiFi drain on battery | Measure on running drone, optimize if needed |
| **Receiver failsafe on WiFi overload** | Low | WiFi spikes might cause jitter | Already prioritized (Core 0 isolated) |
| **OTA safety in flight** | Low | Currently disabled when armed (safe) | Consider adding OTA during hover test |

### What's Needed for Full ESP32 Readiness

1. **Flight test on ESP32** → Mount on drone, execute PID tuning
2. **Battery voltage monitoring** → Add ADC reading, low-battery warning
3. **SD card logging** (future) → Would need SD library, SPI wiring
4. **GNSS (GPS) integration** (future) → Serial UART, radioComm extension
5. **Fine-tune FreeRTOS timing** → May need to increase Core 0 priority or tweak task stacks

**Verdict**: ESP32 is **feature-complete** but **flight-test pending**. Code architecture is solid; real-world validation needed.

---

## 11. Open Questions for the Orchestrator

### Critical (Block Parallel Work?)

1. **SBUS receiver re-enable**: Should we uncomment `USE_SBUS_RECEIVER` in config.h? (Changes 10/10 builds from FAIL/PASS/UNTESTED to 10/10 PASS)
   - **Recommendation**: YES, do this immediately (trivial, unblocks all Teensy live builds)

2. **Current hardware config**: What is mounted on the bench Teensy right now?
   - From todo.md (line 11): Teensy 4.0 + MPU6050 + SSD1306 OLED + **SBUS Receiver** (no motors/ESCs)
   - **Next step**: Calibrate orientation + IMU + radio without motors

3. **ESP32 board variant**: Do we have an ESP32-S3 board, or just standard ESP32 DevKit?
   - **Code ready for both**, just need to know which to prioritize

### Useful (Affects Workstream Partition)

4. **WiFi network available**: Is eduroam or enterprise WiFi needed, or just basic WPA2?
   - Code handles both, but affects test scope

5. **Timeline for motors/props**: When will ESCs be connected to start motor tests?
   - Blocks Phase 5 (ESC calibration) and later

6. **Flight test scope**: Tethered only, or open-field testing planned?
   - Affects PID tuning aggressiveness + safety procedures

### Nice-to-Have (Inform Design)

7. **Commercial autopilot comparison**: Is there a target performance benchmark (e.g., "match Betaflight stability")?
   - Affects tuning targets, feature priority

8. **Swarm coordination**: Is `swarm_api/` the primary control interface, or is manual RC the baseline?
   - Affects WiFi API test coverage

---

## 12. Recommended Workstream Partition

### Overview

**Project size**: **LARGE (≈15k LOC total, well-structured)**  
**Readiness**: **READY — Clean architecture, clear module boundaries, comprehensive docs**

Four parallel workstreams, **exclusive write zones**:

---

### 🔴 Workstream 1: Teensy Hardware Validation & Calibration

**Scope**: Get Teensy 4.0 fully calibrated, live build running, ready for motor testing  
**Duration**: 3-4 hours  
**Write zone (exclusive)**:
- `/flight_controller/include/config.h` (all calibration #defines)
- `/flight_controller/docs/todo.md` (update session notes)

**Tasks**:

1. **Pre-flight** (15 min)
   - Uncomment `USE_SBUS_RECEIVER` in config.h → build `teensy40` (live)
   - Verify `pio run -e teensy40` compiles

2. **Hardware boot** (10 min)
   - Flash `teensy40_calibration`
   - Run test suite: `./tests/test_calibration.sh` — confirm 42/42 pass
   - Verify SBUS receiver channels respond (`s` command)

3. **Orientation detection** (20 min)
   - Run `./tools/calibrate.sh imu` (or manual `o` command)
   - 3-position calibration (level, nose-up, right-up)
   - Copy axis transformation to imu.cpp

4. **IMU calibration** (20 min)
   - `calibrate.sh imu` or manual `i` command
   - Level board, 2000-sample stability check
   - Copy gyro/accel offsets to config.h

5. **Radio calibration** (20 min)
   - `calibrate.sh radio` or manual `r` command
   - Move sticks as guided, auto-detect channels
   - Copy THROTTLE/ROLL/PITCH/YAW channels to config.h

6. **Failsafe detection** (10 min)
   - `calibrate.sh failsafe` or manual `f` command
   - TX on → TX off, measure failsafe values
   - Copy to config.h

7. **Calibration export** (5 min)
   - `calibrate.sh dump` or manual `d` command
   - Verify all values in one block, copy to config.h

8. **Live build test** (10 min)
   - Flash `teensy40` (live build)
   - Verify OLED boot message, serial telemetry
   - Confirm CH1-6 respond to transmitter

**Output artifacts**:
- Updated `/flight_controller/include/config.h` with real calibration values
- Test results log (pass/fail each phase)
- Updated `todo.md` with next steps

**Deliverable**: Production-ready Teensy 4.0 config, ready for ESC/motor testing.

---

### 🟢 Workstream 2: ESP32 Dual-Core Flight Testing & Tuning

**Scope**: Flight test ESP32 implementation, measure loop timing, start PID tuning  
**Duration**: 4-6 hours  
**Write zone (exclusive)**:
- `/flight_controller/include/config.h` (ESP32-specific defines + PID gains)
- `/flight_controller/src/main.cpp` (FreeRTOS task tuning if needed)
- `/flight_controller/docs/findings/esp32-flight-test-results.md` (NEW)

**Tasks**:

1. **Build & boot** (15 min)
   - Flash `esp32_calibration`
   - Verify WiFi connects to eduroam/local network
   - Verify web server at floppi-XXXX.local
   - Run test suite with ESP32 serial adapter

2. **Calibration transfer** (30 min)
   - **Option A**: Use Teensy calibration values from Workstream 1 (if same IMU orientation)
   - **Option B**: Run orientation + IMU calibration on ESP32 (if different mounting)
   - Copy final values to config.h

3. **Dual-core timing** (20 min)
   - Use `tools/complexity_calculator.py --clock 240 --cores 2 --full`
   - Measure actual Core 0 flight loop + Core 1 WiFi/display overhead
   - Compare against budget targets (Core 0 <1000us @ 1kHz)

4. **WiFi API test** (20 min)
   - Flash `esp32` (live build with WiFi)
   - Send commands via POST `/api/commands` + WebSocket
   - Measure command latency (target <100ms)

5. **Mock motors** (20 min, no ESCs yet)
   - Verify motor output PWM values are sane (1000-2000us range)
   - Verify mixer response to stick input
   - Note: No actual spin-up without ESCs

6. **PID baseline setup** (20 min)
   - Apply conservative defaults to config.h
   - Document PID gains for reference
   - Prepare for next phase (motor testing)

7. **Telemetry validation** (15 min)
   - Test all telemetry modes (IMU, full, quaternion)
   - Verify data over WiFi API + serial
   - Check for FreeRTOS timing glitches

**Output artifacts**:
- ESP32 calibration values in config.h
- `esp32-flight-test-results.md` (timing, WiFi latency, Core overhead analysis)
- Updated `todo.md` with next steps

**Deliverable**: ESP32 flight-ready, WiFi API validated, ready for motor/swarm testing.

---

### 🔵 Workstream 3: Test Suite Enhancement & CI/CD Prep

**Scope**: Modularize test harness, add ESP32 support, prepare for automated CI  
**Duration**: 2-3 hours  
**Write zone (exclusive)**:
- `/flight_controller/tests/` (all test files)
- `/flight_controller/tools/` (test helper scripts)
- `/flight_controller/docs/findings/test-infrastructure-v2.md` (NEW)

**Tasks**:

1. **Refactor test harness** (60 min)
   - Separate test definitions from harness logic
   - Extract shared functions to `tests/lib/harness.sh` (port mgmt, serial_monitor.py wrapper)
   - Create modular suite structure: `tests/suites/test_{imu,radio,motors,telemetry,full}.sh`

2. **Add ESP32 serial support** (40 min)
   - Extend harness to detect board type (Teensy vs ESP32)
   - Teensy: use `teensy_reboot` for reset
   - ESP32: use RTS/DTR toggle for reset
   - Update `serial_monitor.py` if needed for chip auto-detection

3. **Expand test coverage** (30 min)
   - Radio test suite: channel range, failsafe response
   - Motor test suite: PWM range validation (when ESCs connected)
   - Telemetry test: all 3 modes (IMU, full, quaternion)

4. **CI/CD skeleton** (20 min)
   - Create `.github/workflows/build_test.yml` (if using GitHub)
   - Runs `build-all` on every push
   - Runs test suite on Teensy (if hardware available in CI)

5. **Documentation** (20 min)
   - Update roadmap.md (test infrastructure completed)
   - Document test suite usage for next developers
   - Add troubleshooting guide for test failures

**Output artifacts**:
- Modularized `tests/` directory structure
- Enhanced `serial_monitor.py` (if needed)
- `.github/workflows/build_test.yml` (CI skeleton)
- `test-infrastructure-v2.md` (architecture docs)

**Deliverable**: Scalable test infrastructure, CI-ready, supports both Teensy and ESP32.

---

### 🟣 Workstream 4: Feature Expansion & Documentation

**Scope**: Implement modular feature additions, complete missing docs, plan next phase  
**Duration**: 3-4 hours  
**Write zone (exclusive)**:
- `/flight_controller/include/config.h` (new feature flags + docs)
- `/flight_controller/docs/` (all new docs)
- `/flight_controller/docs/features/` (feature specifications)

**Tasks**:

1. **Motor test suite framework** (45 min)
   - Design modular motor test pattern (PWM range, failsafe cut, mixing)
   - Create placeholder test functions in `tests/suites/test_motors.sh`
   - Document safety gates (props-off warning, explicit user confirmation)
   - **Blocked by**: No ESCs/motors connected yet — skeleton only

2. **PID tuning guide** (30 min)
   - Write interactive guide in `docs/pid-tuning-guide.md`
   - Explain `g` command workflow in calibration mode
   - Step-by-step: conservative → aggressive
   - Include typical values for quad vs other VTOL types

3. **Future sensor integration plan** (30 min)
   - Barometer (altitude + descent rate) → `USE_BAROMETER` flag spec
   - Magnetometer (compass heading) → `USE_COMPASS` flag spec
   - GPS (position) → `USE_GPS` flag spec
   - Document modular I2C sensor architecture

4. **Swarm coordination API docs** (20 min)
   - Document `/api/commands`, `/api/status` endpoints
   - Example curl/Python code for swarm_api integration
   - Failsafe guarantees under WiFi loss

5. **Phase 5 planning** (20 min)
   - Define motor/ESC testing workflow
   - Create checklist in roadmap.md
   - Document PID tuning milestones

6. **ResearchHub ingestion** (15 min)
   - Move all 12 findings documents into ResearchHub (if configured)
   - Configure RAG knowledge base for auto-research
   - Verify PDF sources are indexed

**Output artifacts**:
- New feature specs (barometer, magnetometer, GPS)
- `docs/pid-tuning-guide.md`
- Motor test framework skeleton
- Updated `roadmap.md` (Phase 5 plan)
- ResearchHub configuration docs

**Deliverable**: Clear path forward for feature expansion, docs complete, roadmap updated.

---

## Workstream Synchronization

### Critical Dependencies

```
Workstream 1 (Teensy calibration)
    ↓
Workstream 4 (PID tuning guide) ← uses calibration results
    ↓
Workstream 2 (ESP32 testing) ← can use Teensy cal values

Workstream 3 (Test suite) — parallel, independent
```

### Critical Path

1. **Workstream 1 FIRST** → produces real calibration values
2. **Workstream 2** → validates ESP32 with Teensy-derived calibration
3. **Workstream 4** → documents PID tuning based on Workstreams 1+2
4. **Workstream 3** → modularizes tests for future use

---

## Summary

| Aspect | Status | Notes |
|--------|--------|-------|
| **Architecture** | ✅ EXCELLENT | Modular, well-documented, proven design |
| **Code Quality** | ✅ GOOD | Clean separation, comprehensive headers, #ifdef gating |
| **Documentation** | ✅ EXCELLENT | Scope, roadmap, 12 research docs, wiring guides |
| **Test Coverage** | ✅ GOOD | 42/42 pass on Teensy; ESP32 untested but code ready |
| **Compilation** | ⚠️ PARTIAL | 7/10 pass; 3 expected fails (SBUS commented); action: re-enable |
| **Hardware Status** | ⏳ READY FOR TEST | Teensy 4.0 on bench, needs calibration + motor mounting |
| **Flight-Ready** | ❌ NOT YET | Calibration pending, PID tuning pending, motor test pending |

**Next steps**:
1. Execute Workstream 1 (calibration)
2. Execute Workstreams 2, 3, 4 in parallel
3. Start motor/ESC testing (Phase 5 in roadmap)
4. Begin PID tuning and first flight

---

**Report generated**: 2026-05-20  
**Agent**: fc-recon@flight_controller:1  
**Status**: Ready for parallel agent dispatch
