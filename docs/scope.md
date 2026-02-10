# Project Scope: floppi

> Last updated: 2026-02-09
> Status: Active

## Mission Statement

floppi is an open-source bare-bones flight controller platform: firmware + desktop tool. The flight controller firmware does one thing well — stabilize a drone (read sensors, filter, PID, output motors). Complex logic (missions, GPS navigation, mode switching) belongs on an external flight computer. The desktop tool (fc_tool) provides serial monitoring, data visualization, and calibration support.

## Sub-Projects

| Sub-Project | Purpose | Docs |
|-------------|---------|------|
| `flight_controller/` | PlatformIO embedded firmware (Teensy + ESP32) | [scope](../flight_controller/docs/scope.md), [roadmap](../flight_controller/docs/roadmap.md) |
| `fc_tool/` | Tauri 2 desktop app (Rust + vanilla JS) — serial monitor, plotter, calibration | [scope](../fc_tool/docs/scope.md), [roadmap](../fc_tool/docs/roadmap.md) |

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
- Future: calibration interface, measurement cursors, signal analysis

## Out of Scope

- Physical drone design, frame construction, component selection (engineering360)
- GPS, barometer, magnetometer integration (flight computer territory)
- Autonomous navigation, waypoint following (flight computer territory)
- In-flight mode switching (flight computer sends commands)
- Multi-drone coordination (external systems)
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
