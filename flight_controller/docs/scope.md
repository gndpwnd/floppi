# Flight Controller Firmware - Scope

> Last updated: 2026-05-20
> Status: Active

---

## Overview

Open-source bare-bones VTOL flight controller firmware for Teensy and ESP32 microcontrollers, based on dRehmFlight. Designed for garage-buildable drones with an emphasis on raw performance, simplicity, and careful feature iteration. The firmware is a flight **stabilizer**, not a flight **autopilot** — it does lots of math really fast (read sensors, filter, PID, output motors) and leaves complex logic to an external flight computer.

The firmware supports a two-mode workflow: **calibration mode** for determining hardware-specific values, and **live mode** for lean, hard-coded flight operation.

## Objectives

- Deliver a reliable, well-performing open-source flight controller accessible to hobbyists and makers
- Support VTOL vehicles broadly (multirotors first, then fixed-wing and hybrid configurations)
- **Automate every calibration process**: Every value that needs tuning should have an auto-calibration routine. Users flash, run guided calibration, copy values to config.h, flash live build, and fly. No manual guesswork required.
- Keep firmware lean: hard-coded values in live mode, no SD cards or extra memory requirements
- Iterate carefully on features over time rather than building an all-in-one solution
- **Do not become Betaflight.** Raw performance and simplicity over feature count. If a feature adds runtime overhead to the flight loop and isn't necessary for stable flight, it belongs on the flight computer, not in the firmware.
- **Bare bones yet efficient**: Minimal code, maximum automation. The firmware does less at runtime precisely because calibration is thorough and automated upfront.

## Requirements

### Functional Requirements

- [x] Stable flight control with PID loops (rate and angle modes)
- [x] IMU sensor integration (MPU6050 primary, MPU9250 secondary)
- [x] SBUS receiver support (FlySky FS-iA6B primary)
- [x] Automatic IMU calibration via CH6 switch
- [x] Arming/disarming safety system
- [x] Failsafe on signal loss
- [x] Firmware state machine: setup/calibration mode vs live mode
- [x] Auto-calibration that outputs values for hard-coding into live firmware
- [ ] Support for multiple VTOL configurations (quad X, hex, fixed-wing, tiltrotor)
- [x] Radio channel auto-mapping and calibration
- [x] IMU orientation auto-detection
- [x] Runtime PID tuning via serial commands (calibration mode)
- [x] Failsafe auto-detection (measure receiver failsafe outputs)
- [x] ESC endpoint calibration routine
- [x] Magnetometer auto-calibration (MPU9250)
- [x] Runtime filter/limits tuning via serial commands

### Technical Requirements

- [x] PlatformIO build system with multi-board support
- [x] Teensy 4.0/4.1 as primary platform (ARM Cortex-M7 @ 600MHz)
- [x] ESP32/S3 as WiFi-enabled platform (dual-core, 240MHz)
- [x] 1-2kHz control loop rate (1kHz ESP32, 2kHz Teensy)
- [x] All calibration values hard-coded in live builds (no runtime configuration files)
- [x] Calibration mode: mutable offsets for testing and value determination
- [x] Clean separation between calibration/debug code and live flight code
- [x] Build targets for different firmware states (calibration vs live)
- [x] Compile-time feature gating (#ifdef) — zero runtime overhead for unused features
- [x] Dual-core architecture on ESP32 (Core 0 = FC, Core 1 = peripherals)

### Resource Requirements

- [x] Teensy 4.0 or 4.1 microcontroller — OR — ESP32/S3 dev board
- [x] MPU6050 IMU (GY-521 breakout)
- [x] SBUS-compatible receiver
- [x] PlatformIO development environment
- [ ] Test drone frame with motors/ESCs for hardware validation

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| No SD cards or external storage in live firmware | Keep hardware simple, reduce failure points | No |
| Hard-coded calibration values in live mode | Simplicity, reliability, minimal memory | No |
| Teensy 4.0/4.1 + ESP32/S3 targets | Proven platforms, ESP32 adds WiFi | No |
| No paid services or cloud dependencies | Open-source, self-contained | No |
| PlatformIO build system | Cross-platform, library management | No |
| Testing via firmware + serial scripts | Calibration routines self-validate. `tests/` has bash/Python test harness using `serial_monitor.py` | No |

## Assumptions

- [VERIFIED] dRehmFlight provides a solid, proven foundation for VTOL flight control
- [VERIFIED] MPU6050 is the most accessible IMU for hobbyist builders
- [VERIFIED] SBUS is the preferred receiver protocol for clean digital communication
- [ASSUMED] Teensy 4.x EEPROM emulation (flash-based) is sufficient for calibration data storage during calibration mode
- [ASSUMED] Most users will have a 3-position switch on CH6 for mode selection
- [ASSUMED] Garage builders will iterate: calibrate → hard-code → flash → fly → repeat

## Auto-Calibration Philosophy

The firmware aims to **automate every calibration process**. Every `#define` value in config.h that depends on specific hardware should have a corresponding auto-calibration routine in the calibration build. The user should never need to guess or manually measure a value.

**Calibration coverage target:**

| Category | Values | Auto-Calibration | Status |
|----------|--------|-------------------|--------|
| IMU offsets (accel + gyro) | 6 values | Single-position routine (`i`) | Done |
| IMU offsets + scale factors | 9 values | 6-position routine (`m`) | Done |
| IMU orientation/mounting | axis mapping | 3-position routine (`o`) | Done |
| Radio channel mapping | 6 channels | Guided stick-move routine (`r`) | Done |
| PID gains | 9 values | Runtime serial tuning (`g`) | Done |
| Failsafe values | 6 values | Auto-detect from receiver (`f`) | Done |
| ESC endpoints | min/max PWM | ESC calibration routine (`e`) | Done |
| Magnetometer (MPU9250) | 6 values | Sphere calibration routine | Done |
| Filter coefficients | ~6 values | Runtime serial tuning (`p`) | Done |
| Max rates/angles | 3 values | Runtime serial tuning (`p`) | Done |

**Workflow**: Flash calibration build → run auto-calibration routines → firmware outputs `#define` values → copy to config.h → flash live build → fly. Every step is guided with serial prompts and quality validation.

**Testing built-in**: Each calibration routine validates its own results (stability checks, range checks, quality scoring) and offers retry on poor results. The firmware is its own test harness.

## RadioComm — Universal Command Entry Point

RadioComm is the **single entry point** for all command/control input to the flight controller. Every source of commands — whether hardware radio, serial, I2C, or WiFi — flows through RadioComm before reaching flight control logic. This keeps the architecture clean: the flight controller only talks to RadioComm, never directly to individual command sources.

**Architecture:**

```text
SBUS receiver ─────┐
iBUS receiver ─────┤
DSM receiver  ─────┤
PPM receiver  ─────┤
PWM receiver  ─────┼──→ RadioComm ──→ channel_X_pwm ──→ Flight Controller
Serial commands ───┤     (unified)     (1000-2000us)
I2C commands   ────┤
WiFi API (ESP32) ──┘
```

**Current state**: RadioComm handles 5 RC protocols (SBUS, iBUS, DSM, PPM, PWM) + 3 command sources (serial, I2C, WiFi API), compile-time selected. One source active per build. Outputs `channel_1_pwm` through `channel_6_pwm` (1000-2000us range).

**Command sources**:

| Source | Interface | Config Flag | Priority | Notes |
|--------|-----------|-------------|----------|-------|
| SBUS | Serial (inverted) | `USE_SBUS_RECEIVER` | Primary | Current default |
| iBUS | Serial (115200) | `USE_IBUS_RECEIVER` | Primary | FlySky FS-iA6B recommended |
| DSM/DSMX | Serial | `USE_DSM_RECEIVER` | Primary | Spektrum |
| PPM | Single GPIO | `USE_PPM_RECEIVER` | Primary | Legacy |
| PWM | 6 GPIOs | `USE_PWM_RECEIVER` | Primary | Legacy |
| Serial commands | UART | `USE_SERIAL_COMMANDS` | Override | Done. Binary protocol, 15-byte frames, 115200 baud |
| I2C commands | I2C slave | `USE_I2C_COMMANDS` | Override | Done. FC as I2C slave on Wire1 (0x42), 12-byte frames |
| WiFi API | HTTP/WebSocket | `USE_WEB_SERVER` | Override | Done. POST /api/commands + WebSocket, spinlock cross-core |

**Priority / arbitration**: When multiple sources are active, RadioComm needs an arbitration strategy. RC receiver is the primary (real-time, hardware). Serial/I2C/WiFi are override sources (typically from a flight computer). If an override source is active and sending, it takes priority. If it goes silent, RC receiver resumes. Failsafe applies across all sources.

**Key principle**: The API web server is NOT a separate command path. It feeds INTO RadioComm. The flight controller only ever reads `channel_X_pwm` — it doesn't know or care where the values came from.

**Pin configuration**: All command source pins are configurable in config.h (override defaults from pin_definitions.h).

## Hardware Architecture Vision

The flight controller is a **modular base system** where each component is configurable and swappable:

```text
┌─────────────────────────────────────────────────────┐
│                  Base System (always)                │
│                                                     │
│  MCU (Teensy/ESP32)  +  IMU (MPU6050/MPU9250)      │
│       +  Receiver (iBUS/SBUS/DSM/PPM/WiFi API)     │
│       +  ESCs (PWM signal, up to 8 motors)           │
│                                                     │
├─────────────────────────────────────────────────────┤
│           Optional (config.h flags)                  │
│                                                     │
│  OLED Display (USE_OLED_DISPLAY → select model)     │
│  WiFi features (ESP32 only, auto with USE_WIFI)     │
│  Optimization filters (USE_OPTIMIZATION)            │
│  Racing features (USE_RACING)                       │
│  OTA updates (USE_OTA)                              │
└─────────────────────────────────────────────────────┘
```

**Key principles:**

- **Each component is configurable**: MCU choice, IMU model, receiver protocol, ESC brand — all selected in config.h. Swap hardware without changing code.
- **OLED is optional**: Enable with `USE_OLED_DISPLAY`, select display model in config.h. Not required for flight.
- **Magnetometer is out of scope**: Compass data is flight computer territory, not the base stabilizer. MPU9250 mag calibration exists for users who want it, but it's not part of the core loop.
- **ESP32 dual-role**: Core 0 = flight controller (real-time PID loop). Core 1 = lightweight flight computer services (WiFi, web server, telemetry, OTA). Core 1 can outsource heavy computation to an external host over WiFi.
- **Future sensors added modularly**: Additional sensors (barometer, GPS, lidar) would each get their own `USE_*` flag and run on Core 1 (ESP32) or be handled by an external flight computer. The base flight loop on Core 0 is never affected.
- **Motor/servo scaling (future)**: Currently 6 motor outputs + 7 servo channels. Expanding to 8 motors is straightforward (add `m7`/`m8` variables, pins, PWM channels). The real complexity is in the mixer math, not the motor count. Advanced VTOL (tiltrotors, hybrid hover-to-glide) needs servo-driven tilt + transition logic — the transition decisions belong on the flight computer, the FC just outputs PWM based on mixer weights. See roadmap for details.

### Progression Path

The project supports three hardware configurations, from simple to advanced:

1. **Teensy + FS-iA6B** (starting point)
   - Simplest wiring, proven dRehmFlight base
   - Manual RC control only
   - Best for: learning, PID tuning, initial flight testing

2. **ESP32 + FS-iA6B** (WiFi-enabled)
   - Same RC control + WiFi calibration/telemetry
   - Web dashboard at floppi.local, OTA updates
   - Best for: iterative development, remote diagnostics

3. **ESP32 + Web API** (no physical receiver)
   - Commands via WiFi from external controller app
   - ESP32 Core 1 as lightweight flight computer proxy
   - Best for: swarm coordination, autonomous flight, indoor testing

### External Controller App (Built — `swarm_api/`)

A standalone Python application outside `flight_controller/` for controlling ESP32 drones over WiFi:

- **Web dashboard** with fleet panel, throttle/roll/pitch/yaw sliders, real-time telemetry
- **Config file** (`config.json`) with ESP32 MAC addresses, mDNS hostnames, network settings
- **mDNS + IP fallback** discovery (floppi-XXXX.local)
- **Dual command path**: WebSocket `/ws` (primary, 10Hz) with HTTP POST `/api/commands` fallback
- **Command format**: `{"ch1":1500,"ch2":1500,"ch3":1000,"ch4":1500,"ch5":1000,"ch6":1000}`
- **Stack**: Python 3.10+, FastAPI, uvicorn, httpx, websockets, zeroconf
- **Run**: `cd swarm_api && pip install -r requirements.txt && python3 -m uvicorn src.main:app --host 0.0.0.0 --port 8080`

This is a separate project. The FC firmware just exposes the WiFi API endpoints.

## Boundaries

### In Scope

- Flight controller firmware for Teensy 4.0/4.1 (Teensy 3.6 legacy) and ESP32/S3
- PID control loops (rate, angle modes) — compile-time selection
- IMU integration and sensor fusion (Madgwick 6DOF filter)
- Auto-calibration features (IMU, radio, attitude)
- Multiple VTOL configurations (quad, hex, octo, fixed-wing, tiltrotor, hybrid) — up to 8 motors + 7 servos (future)
- Firmware state machine (calibration mode ↔ live mode)
- Build targets for different firmware states
- Serial debug output and diagnostics
- Integration with fc_tool for visual diagnostics during calibration
- Safety systems (arming, failsafe, throttle cut)
- ESP32: WiFi STA mode, web status server, API client for swarm coordination
- ESP32: OLED display on Core 1 (non-real-time)
- WiFi telemetry (JSON API, WebSocket streaming, mDNS)
- RadioComm as universal command entry point — all command sources (SBUS, DSM, PPM, PWM, serial, I2C, WiFi API) flow through RadioComm to the flight controller
- Modular feature system — all features selectable in config.h via #ifdef flags
- Feature tiers: USE_OPTIMIZATION (noise reduction), USE_RACING (performance)
- Standalone library vendoring — all dependencies in lib/ and lib_esp32/

### Out of Scope (Exclusions)

- Physical drone design, frame construction, component selection (→ engineering360)
- SD card logging or runtime configuration files in live firmware
- GPS, barometer, magnetometer — flight computer territory [^baro-gps]

[^baro-gps]: Telemetry-only barometer and passthrough-only GPS are now in scope as Core-1 modular sensors — they add data, not flight logic. See [findings/barometer_integration_spec_2026-05-20.md](findings/barometer_integration_spec_2026-05-20.md) (telemetry-only baro) and [findings/gps_passthrough_spec_2026-05-20.md](findings/gps_passthrough_spec_2026-05-20.md) (passthrough-only GPS — bytes relayed to the flight computer). GPS-guided/autonomous flight remains OUT of scope.
- Ground control station software
- Professional/commercial-grade features
- Custom PCB design (uses off-the-shelf boards)
- Autonomous navigation or waypoint following — flight computer territory
- Multi-drone coordination or swarm features — flight computer territory
- swarm-API authentication / TLS — intentionally NOT implemented in the current development phase [^api-auth]

[^api-auth]: The ESP32 swarm-API (HTTP + WebSocket command/telemetry, plus OTA) has no authentication and no transport encryption. This is a deliberate development-phase decision (2026-05-20): the flight controller is assumed to run on a closed, trusted lab LAN. Anyone with LAN access can send flight commands, trigger an OTA flash, or read telemetry — including GPS location once `USE_GPS` is enabled. **Do NOT connect the FC to a shared or untrusted network in this state.** A shared-secret header or TLS is deferred, not refused; see [findings/swarm_api_contract_2026-05-20.md](findings/swarm_api_contract_2026-05-20.md) for the options. Revisit before any non-lab deployment.
- In-flight mode switching — flight computer sends commands, FC executes
- Dynamic gyro filtering (FFT, RPM filters) — Betaflight-level complexity, not needed
- In-flight PID tuning — calibration mode + fc_tool covers this
- Blackbox logging — flight computer can log via WiFi

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Base firmware | dRehmFlight | Proven VTOL flight controller, well-documented, MIT license | Pre-2026 |
| Build system | PlatformIO | Cross-platform, multi-board, library management | Pre-2026 |
| Primary IMU | MPU6050 via I2C | Widely available, cheap, well-supported | Pre-2026 |
| Primary receiver | iBUS/SBUS (FlySky FS-iA6B) | iBUS recommended (non-inverted, 115200, direct microseconds). SBUS also supported. | 2026-02-10 |
| Calibration storage | Hard-coded in config.h | No SD cards, no EEPROM in live builds, simple and reliable | 2026-02-05 |
| Firmware states | Calibration mode vs Live mode | Separate debug/test from production flight | 2026-02-05 |
| Testing approach | Built into firmware + Python serial tools | Calibration routines validate own results. Test harness uses `serial_monitor.py` (raw termios) + `pio device monitor`. `calibrate.sh` is the primary user-facing tool. No fc_tool dependency. NEVER use raw bash for serial (cat/stty/echo). | 2026-02-17 |
| Build separation | PlatformIO `extends` + `-D CALIBRATION_MODE` | Each board gets a `_calibration` variant. Clean, DRY, no code duplication. | 2026-02-05 |
| Attitude filter | Madgwick 6DOF | Better noise rejection than complementary, simpler than EKF, single tuning parameter | Pre-2026 |
| WiFi architecture | STA mode (connect to existing) | Swarm coordination — drones on same network, API to centralized computers | 2026-02-07 |
| Dual-core split | Core 0 = FC, Core 1 = WiFi/display | Flight control isolated from non-real-time tasks, zero interference | 2026-02-07 |
| Flight mode selection | Compile-time only | No runtime overhead. Flight computer handles mode switching externally | 2026-02-07 |
| Feature gating | #ifdef preprocessor | Zero binary cost for unused features, see [features/compile-time-architecture.md](features/compile-time-architecture.md) | 2026-02-07 |
| Modular features | config.h flags | USE_WEB_SERVER, USE_API_SERVER, USE_OPTIMIZATION, USE_RACING — users enable what their MCU can handle | 2026-02-07 |
| Library vendoring | lib/ + lib_esp32/ | All deps vendored for offline/standalone builds. No external downloads needed. | 2026-02-07 |
| Web vs API server | Separate config flags | Web server = calibration/display, API server = remote control/swarm. Independently toggleable. | 2026-02-07 |
| RadioComm as universal entry point | All command sources → RadioComm → FC | Single abstraction layer for all input: RC protocols (SBUS/DSM/PPM/PWM), serial commands, I2C commands, WiFi API commands. One entry point, one data format (`channel_X_pwm`), one failsafe path. API/web server feeds into RadioComm, not directly to FC. | 2026-02-09 |
| Configurable pin definitions | config.h overrides pin_definitions.h | Users configure pin assignments in config.h alongside everything else. pin_definitions.h provides platform defaults with `#ifndef` guards. | 2026-02-09 |
| Modular hardware architecture | Swappable MCU + IMU + receiver + ESCs | Base system components are all configurable in config.h. Optional features (OLED, WiFi, filters) added via flags. Future sensors added modularly without affecting base loop. | 2026-02-10 |
| Progression path | Teensy+RC → ESP32+RC → ESP32+WiFi API | Three hardware tiers from simple to advanced. Each adds capability without requiring previous tier's hardware. | 2026-02-10 |

## Integration Points

- **fc_tool** (Tauri desktop app): Optional visualization tool for serial data plotting. Not required for development or testing — `serial_monitor.py` and `pio device monitor` cover all testing needs.
- **Flight computer** (external): Sends commands to FC via radio or WiFi API. Handles complex logic (missions, mode switching, aerobatics sequencing, swarm coordination). FC just executes.
- **engineering360**: Receives physical platform specifications (mass, inertia, motor specs) for PID tuning
- **PlatformIO**: Build system, library management, firmware upload

## Open Questions

- [x] How to cleanly separate calibration builds from live builds in platformio.ini? → **Resolved**: Use PlatformIO `extends` directive. Each board gets a `_calibration` variant that inherits board config and adds `-D CALIBRATION_MODE`. See [features/build-targets.md](features/build-targets.md).
- [ ] Best approach for Teensy 4.x EEPROM emulation during calibration mode — see [findings/](findings/) when research completes
- [ ] Should IMU orientation auto-detection happen in calibration mode only, or also at startup in live mode?
- [ ] What PID auto-tuning approach is most practical for this project? (Betaflight-style relay test, ArduPilot AUTOTUNE, or simpler?) — see [findings/auto-calibration-research.md](findings/auto-calibration-research.md) for initial research
- [ ] How tightly should fc_tool integration be coupled to the calibration workflow?

## ResearchHub Integration

This sub-project is the primary research workspace for all flight dynamics topics within floppi. ResearchHub is configured to auto-research: quaternions, PID/LQR/MPC control, IMU sensor fusion, acrobatics trajectory planning, rotational dynamics, coordinate transforms, and safety constraints. The existing 14 findings documents in `docs/findings/` will be ingested into a RAG knowledge base for retrieval-augmented research. PDFs are stored at `docs/findings/sources/pdfs/`. Different build configurations (`#ifdef`) handle different sensors and flight modes, making this the natural home for all flight dynamics R&D.

## Critical Notes

- **Safety first**: Always remove props for ground testing. Test arming/disarming before every flight session
- **Calibration workflow**: Flash calibration build → run calibration → copy values → edit config.h → flash live build → fly
- **dRehmFlight heritage**: This project builds on Nick Rehm's work. Keep attribution and MIT license compatibility
- **VTOL generality**: Like dRehmFlight, the mixer is user-customizable for any VTOL configuration. Don't hard-code for quadcopter only
- **Bare-bones philosophy**: The FC has ~90% of its target features. Resist adding more. Every feature that adds runtime overhead to the flight loop must justify its existence. See [findings/bare-bones-fc-research.md](findings/bare-bones-fc-research.md)
- **Flight computer boundary**: In-flight mode switching, autonomous features, GPS/baro/mag integration, mission planning, and complex aerobatics all belong on the flight computer. The FC stabilizes. Period.

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-05 | Initial scope for flight_controller as standalone mini-project | LLM + User |
| 2026-02-05 | Resolved build separation approach (PlatformIO extends), added technical decision | LLM + User |
| 2026-02-06 | Added research for ESP32 platform support as future feature (not current scope) | LLM + User |
| 2026-02-06 | Added timing calculator tool (tools/timing_calculator.py) for platform analysis | LLM + User |
| 2026-02-07 | ESP32 now in scope (dual-core, WiFi STA, web server, API client). Updated boundaries: flight computer handles complex logic. Added bare-bones philosophy. | LLM + User |
| 2026-02-07 | Added modular feature system (USE_WEB_SERVER, USE_API_SERVER, USE_OPTIMIZATION, USE_RACING). Library vendoring for standalone builds. Updated timing calculator with per-core and feature tier analysis. | LLM + User |
| 2026-02-09 | Added auto-calibration philosophy: every hardware-dependent value must have an auto-calibration routine. Documented calibration coverage table and planned routines (failsafe, ESC, magnetometer, filter/limits tuning). | LLM + User |
| 2026-02-09 | Calibration coverage table: all items marked Done. Added RadioComm universal command layer design (all command sources → RadioComm → FC). Added configurable pin definitions as technical decision. | LLM + User |
| 2026-02-10 | Added hardware architecture vision (modular base system, progression path, external controller app). Added iBUS receiver support. Updated RadioComm to include iBUS. Wiring diagrams reorganized into dedicated folder with specific build guides. | LLM + User |
| 2026-02-10 | All RadioComm command sources implemented (serial, I2C, WiFi API). I2C slave on Wire1. Arbitration design doc written. External controller app (swarm_api) marked as built. | LLM + User |
| 2026-05-20 | Bumped "Last updated" header to match content. Added carve-out footnote on the baro/GPS/magnetometer exclusion: telemetry-only barometer and passthrough-only GPS are now in scope as Core-1 modular sensors (see barometer/GPS specs); GPS-guided flight remains out of scope. | LLM + User |

---

*This document evolves as the project develops. Major scope changes should be discussed before implementation.*
