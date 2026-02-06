# Flight Controller Firmware - Scope

> Last updated: 2026-02-05
> Status: Active

---

## Overview

Open-source VTOL flight controller firmware for Teensy microcontrollers, based on dRehmFlight. Designed for garage-buildable drones with an emphasis on usability, simplicity, and careful feature iteration. The firmware supports a two-mode workflow: **calibration mode** for determining hardware-specific values, and **live mode** for lean, hard-coded flight operation.

## Objectives

- Deliver a reliable, well-performing open-source flight controller accessible to hobbyists and makers
- Support VTOL vehicles broadly (multirotors first, then fixed-wing and hybrid configurations)
- Automate calibration and setup so users can flash, calibrate, and fly with minimal manual configuration
- Keep firmware lean: hard-coded values in live mode, no SD cards or extra memory requirements
- Iterate carefully on features over time rather than building an all-in-one solution

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
- [ ] PID auto-tuning or guided tuning workflow

### Technical Requirements

- [x] PlatformIO build system with multi-board support
- [x] Teensy 4.0/4.1 as primary platform (ARM Cortex-M7 @ 600MHz)
- [x] 2000Hz control loop rate
- [x] All calibration values hard-coded in live builds (no runtime configuration files)
- [x] Calibration mode: mutable offsets for testing and value determination
- [x] Clean separation between calibration/debug code and live flight code
- [x] Build targets for different firmware states (calibration vs live)

### Resource Requirements

- [x] Teensy 4.0 or 4.1 microcontroller
- [x] MPU6050 IMU (GY-521 breakout)
- [x] SBUS-compatible receiver
- [x] PlatformIO development environment
- [ ] Test drone frame with motors/ESCs for hardware validation

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| No SD cards or external storage in live firmware | Keep hardware simple, reduce failure points | No |
| Hard-coded calibration values in live mode | Simplicity, reliability, minimal memory | No |
| Teensy 4.0/4.1 primary target | Proven platform with dRehmFlight heritage | No |
| No paid services or cloud dependencies | Open-source, self-contained | No |
| PlatformIO build system | Cross-platform, library management | No |
| No separate tests/ directory | Tests are baked into firmware as build targets/calibration modes | No |

## Assumptions

- [VERIFIED] dRehmFlight provides a solid, proven foundation for VTOL flight control
- [VERIFIED] MPU6050 is the most accessible IMU for hobbyist builders
- [VERIFIED] SBUS is the preferred receiver protocol for clean digital communication
- [ASSUMED] Teensy 4.x EEPROM emulation (flash-based) is sufficient for calibration data storage during calibration mode
- [ASSUMED] Most users will have a 3-position switch on CH6 for mode selection
- [ASSUMED] Garage builders will iterate: calibrate → hard-code → flash → fly → repeat

## Boundaries

### In Scope

- Flight controller firmware for Teensy 4.0/4.1 (Teensy 3.6 legacy)
- PID control loops (rate, angle modes)
- IMU integration and sensor fusion (Madgwick filter)
- Auto-calibration features (IMU, radio, attitude)
- Multiple VTOL configurations (quad, hex, fixed-wing, tiltrotor, hybrid)
- Firmware state machine (calibration mode ↔ live mode)
- Build targets for different firmware states
- Serial debug output and diagnostics
- Integration with fc_tool for visual diagnostics during calibration
- Safety systems (arming, failsafe, throttle cut)

### Out of Scope (Exclusions)

- Physical drone design, frame construction, component selection (→ engineering360)
- SD card logging or runtime configuration files in live firmware
- Flight computer integration (ESP32/RPi) — separate project within floppi
- GPS, barometer, magnetometer — future scope, not current focus
- Ground control station software
- Professional/commercial-grade features
- Custom PCB design (uses off-the-shelf Teensy boards)
- Autonomous navigation or waypoint following
- Multi-drone coordination or swarm features

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Base firmware | dRehmFlight | Proven VTOL flight controller, well-documented, MIT license | Pre-2026 |
| Build system | PlatformIO | Cross-platform, multi-board, library management | Pre-2026 |
| Primary IMU | MPU6050 via I2C | Widely available, cheap, well-supported | Pre-2026 |
| Primary receiver | SBUS (FlySky FS-iA6B) | Clean digital protocol, single wire | Pre-2026 |
| Calibration storage | Hard-coded in config.h | No SD cards, no EEPROM in live builds, simple and reliable | 2026-02-05 |
| Firmware states | Calibration mode vs Live mode | Separate debug/test from production flight | 2026-02-05 |
| Testing approach | Built into firmware as build targets | Embedded firmware testing is hardware-based, not unit test files | 2026-02-05 |
| Build separation | PlatformIO `extends` + `-D CALIBRATION_MODE` | Each board gets a `_calibration` variant. Clean, DRY, no code duplication. | 2026-02-05 |

## Integration Points

- **fc_tool** (Tauri desktop app): Serial communication for real-time IMU visualization, calibration interface, firmware management
- **engineering360**: Receives physical platform specifications (mass, inertia, motor specs) for PID tuning
- **PlatformIO**: Build system, library management, firmware upload

## Open Questions

- [x] How to cleanly separate calibration builds from live builds in platformio.ini? → **Resolved**: Use PlatformIO `extends` directive. Each board gets a `_calibration` variant that inherits board config and adds `-D CALIBRATION_MODE`. See [features/build-targets.md](features/build-targets.md).
- [ ] Best approach for Teensy 4.x EEPROM emulation during calibration mode — see [findings/](findings/) when research completes
- [ ] Should IMU orientation auto-detection happen in calibration mode only, or also at startup in live mode?
- [ ] What PID auto-tuning approach is most practical for this project? (Betaflight-style relay test, ArduPilot AUTOTUNE, or simpler?) — see [findings/auto-calibration-research.md](findings/auto-calibration-research.md) for initial research
- [ ] How tightly should fc_tool integration be coupled to the calibration workflow?

## Critical Notes

- **Safety first**: Always remove props for ground testing. Test arming/disarming before every flight session
- **Calibration workflow**: Flash calibration build → run calibration → copy values → edit config.h → flash live build → fly
- **dRehmFlight heritage**: This project builds on Nick Rehm's work. Keep attribution and MIT license compatibility
- **VTOL generality**: Like dRehmFlight, the mixer is user-customizable for any VTOL configuration. Don't hard-code for quadcopter only

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-05 | Initial scope for flight_controller as standalone mini-project | LLM + User |
| 2026-02-05 | Resolved build separation approach (PlatformIO extends), added technical decision | LLM + User |

---

*This document evolves as the project develops. Major scope changes should be discussed before implementation.*
