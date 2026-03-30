# Project Scope: floppi

> Last updated: 2026-03-30
> Status: Active

## Mission Statement

floppi is an open-source high-performance drone research platform — onboard control systems, frame design, and swarm coordination for advanced drone research. The flight controller firmware handles stabilization, acrobatics, and sensor fusion using quaternion-based attitude representation. Frame design supports multiple VTOL configurations. The platform serves as a hardware demo for multi-agent systems and aims to help researchers with advanced drone development including acrobatic maneuvers, swarm behavior, and novel flight control strategies.

## Sub-Projects

| Sub-Project | Purpose | Status | Docs |
|-------------|---------|--------|------|
| `flight_controller/` | PlatformIO embedded firmware (Teensy + ESP32) | **Active** — hardware validation phase | [scope](../flight_controller/docs/scope.md), [roadmap](../flight_controller/docs/roadmap.md) |
| `fc_tool/` | Tauri 2 desktop app (Rust + vanilla JS) — serial monitor, plotter | **Active** — feature-rich, pytest test framework | [scope](../fc_tool/docs/scope.md), [roadmap](../fc_tool/docs/roadmap.md) |
| `swarm_api/` | Python FastAPI ground station for ESP32 drone control over WiFi | Paused — needs ESP32 hardware testing | [scope](../swarm_api/docs/scope.md), [roadmap](../swarm_api/docs/roadmap.md) |
| `drone_3d_model/` | 3D frame design, sensor mounts, component layouts for multi-rotor configs | **New** — bootstrapped | [scope](../drone_3d_model/docs/scope.md), [roadmap](../drone_3d_model/docs/roadmap.md) |

## Core Philosophy

- **Bare-bones flight stabilizer**, not an autopilot. Not trying to be Betaflight/ArduPilot.
- **Compile-time everything** — `#ifdef` feature gating, zero runtime overhead for unused features.
- **Two config files** — users edit only `config.h` and `wifi_credentials.h`.
- **Calibration workflow** — flash calibration build, run calibration, copy values to config.h, flash live build, fly.
- **External complexity** — in-flight mode switching, GPS, autonomous navigation, swarm coordination all belong on the flight computer.

## Target Platforms

### Firmware

- **Teensy 4.0/4.1** — Primary platform (ARM Cortex-M7 @ 600 MHz, hardware FPU)
- **ESP32/ESP32-S3** — WiFi-enabled platform (dual-core 240 MHz, WiFi STA mode)
- **Teensy 3.6** — Legacy support

### fc_tool

- **Linux** — Primary (validated, .deb/.rpm/.AppImage builds)
- **Windows** — Scripts written, untested
- **macOS** — Scripts written, untested

## In Scope

### Flight Controller Firmware

- PID control loops (rate + angle modes, compile-time selection)
- IMU integration (MPU6050 primary, Madgwick 6DOF filter)
- SBUS receiver support
- Motor mixing (quad X, extensible to other VTOL types)
- Auto-calibration (IMU, radio, orientation, 6-position accelerometer)
- Safety systems (arming, failsafe, throttle cut)
- ESP32: WiFi STA, web server, API client, OTA updates, OLED display
- Modular features via config.h flags (USE_OPTIMIZATION, USE_RACING, USE_WEB_SERVER, USE_API_SERVER, USE_OTA)
- D-term low-pass filter, derivative on measurement
- Biquad filters, notch filter, feed-forward, TPA, expo, air mode

### fc_tool Desktop App

- Serial port detection with USB VID/PID board identification
- Serial monitor terminal (bidirectional)
- Dynamic multi-graph serial plotter (`name@plotId:value` protocol)
- Dark theme with neon data palette
- Auto-reconnect on disconnect
- Measurement cursors (2 vertical + 2 horizontal per plot, draggable, delta readout)
- X/Y axis zoom and pan controls
- Signal statistics readout (min, max, avg per variable)
- Live dashboard mode (key=value grid updating in place)
- Headless mode, CLI arguments, raw data logging
- Per-plot mode selector (Continuous / Period / Single Period / Frozen)
- Period mode with threshold trigger (edge detection, frequency readout)
- Device session persistence (baud rate per USB serial number)
- pytest test framework (285 tests: unit, integration, e2e, performance)
- Future: additional period detection algorithms, anomaly detection

## Out of Scope

- GPS, barometer, magnetometer integration (flight computer territory)
- Autonomous navigation, waypoint following (flight computer territory)
- In-flight mode switching (flight computer sends commands)
- Dynamic gyro filtering (FFT, RPM filters — Betaflight-level complexity)
- SD card logging, runtime configuration files
- Custom PCB design (uses off-the-shelf boards)

## Companion Projects

- **engineering360** — Physical drone design, structural analysis, component selection
- **Flight computer** (future) — External computer (ESP32/RPi) for complex logic, connected via UART or WiFi API

## Technical Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Base firmware | dRehmFlight | Proven VTOL FC, MIT license |
| Build system | PlatformIO | Cross-platform, multi-board |
| IMU | MPU6050 via I2C | Cheap, accessible, well-supported |
| Receiver | SBUS | Clean digital protocol |
| Desktop app | Tauri 2 (Rust + vanilla JS) | Small binary, native perf, no Electron bloat |
| Charting | Chart.js 4.x | Rich plugin API, adequate perf for sensor data |
| WiFi | STA mode (join existing) | Enables swarm on same network |
| Feature gating | `#ifdef` preprocessor | Zero binary cost for unused features |
| Calibration storage | Hard-coded in config.h | No SD cards, simple, reliable |

---

*See sub-project scope documents for detailed boundaries and technical decisions.*
