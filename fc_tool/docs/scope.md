# fc_tool - Scope

> Last updated: 2026-01-27
> Status: Draft

---

## Overview

fc_tool is a cross-platform desktop application for interacting with floppi flight controller hardware. It provides serial monitoring, real-time IMU data visualization, and firmware management via PlatformIO integration. Built in Rust with Tauri for native executables requiring no runtime dependencies.

## Objectives

- Provide a simple, offline-first GUI for serial communication with flight controller boards
- Visualize IMU/MPU sensor data in real time (accelerometer, gyroscope, magnetometer)
- Integrate with PlatformIO to compile and flash firmware when users change calibration parameters or configuration
- Eliminate the need for users to run scripts or install runtimes

## Requirements

### Functional Requirements

- [ ] Connect to serial ports (auto-detect and manual selection)
- [ ] Serial monitor/terminal with send and receive
- [ ] Parse and display IMU telemetry data in real-time plots
- [ ] Integrate with PlatformIO CLI for firmware compilation and flashing
- [ ] Board detection (Teensy 4.0/4.1 initially, ESP32 later)
- [ ] Calibration parameter display and visualization

### Technical Requirements

- [ ] Rust backend with Tauri framework
- [ ] Web-based frontend (HTML/CSS/JS) for GUI
- [ ] Cross-platform: Windows, macOS, Linux
- [ ] Single executable distribution (no runtime dependencies)
- [ ] Offline-first operation (no internet required for core features)

### Resource Requirements

- [ ] Rust toolchain (stable)
- [ ] Node.js (for Tauri frontend build)
- [ ] PlatformIO CLI (for compile/flash integration)

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| Must run offline | Developer-first tool, no cloud dependency | No |
| Rust + Tauri | Cross-platform compiled executables, decided by operator | No |
| No paid services | Open-source project | No |
| PlatformIO integration | Existing build system for flight_controller firmware | No |
| [ASSUMED] Teensy-first, ESP32 later | ESP32 firmware not yet compiled in flight_controller | Yes |

## Assumptions

- [VERIFIED] Primary use case is developer workflow: change params, compile, flash, monitor
- [VERIFIED] Firmware does not yet have a defined serial telemetry protocol — will be defined as firmware matures
- [ASSUMED] PlatformIO CLI is available on the user's system (fc_tool wraps it, doesn't bundle it)
- [ASSUMED] Teensy boards use USB serial (CDC) for communication
- [ASSUMED] Frontend will use charting libraries (e.g., Chart.js, Plotly, or similar) for IMU visualization

## Boundaries

### In Scope

- Serial port connection and monitoring
- Real-time IMU data plotting (accel, gyro, mag)
- PlatformIO compile and flash integration
- Teensy 4.0/4.1 board support
- Calibration parameter viewing
- Connection management (port selection, baud rate)
- Data logging and export

### Out of Scope (Exclusions)

- This project will NOT implement its own firmware compiler (delegates to PlatformIO)
- This project will NOT provide OTA/WiFi firmware updates (that belongs to flight computer phase)
- This project will NOT be a full ground control station (GCS) — no mission planning, no maps
- This project will NOT bundle PlatformIO — users install it separately
- 3D attitude visualization is a future enhancement, not MVP
- ESP32 support is deferred until firmware is compiled for it

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Language | Rust | Best serial library ecosystem, compiled executables | 2026-01-27 |
| GUI Framework | Tauri | Web frontend + Rust backend, small binaries, OS webview | 2026-01-27 |
| Serial Library | serialport-rs | Most mature cross-platform serial in Rust | 2026-01-27 |
| Build Integration | PlatformIO CLI | Existing build system for flight_controller | 2026-01-27 |

## Integration Points

- **flight_controller/** — PlatformIO project that fc_tool compiles and flashes
- **PlatformIO CLI** — called by fc_tool for build/upload operations
- **Serial telemetry protocol** — TBD, will be co-designed between firmware and fc_tool

## Open Questions

- [ ] What serial telemetry format will the firmware use? (needs firmware-side design)
- [ ] Should fc_tool edit platformio.ini or calibration header files directly for parameter changes?
- [ ] What baud rate will be standard for telemetry? (115200 default assumed)
- [ ] Should fc_tool support multiple simultaneous serial connections?

## Critical Notes

- The serial telemetry protocol is a shared contract between firmware and fc_tool — changes to one affect the other
- PlatformIO must be installed on the user's machine; fc_tool does not bundle it

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-01-27 | Initial draft | LLM + User |

---

*This document evolves as the project develops. Requirements, constraints, and boundaries can be added, modified, or removed as understanding improves. Major scope changes should be discussed with the user.*
