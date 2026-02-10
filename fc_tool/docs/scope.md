# fc_tool - Scope

> Last updated: 2026-02-09
> Status: Active

---

## Overview

fc_tool is a cross-platform desktop application for interacting with floppi flight controller hardware. It provides serial monitoring, dynamic multi-graph data plotting, and (future) firmware management via PlatformIO integration. Built in Rust with Tauri 2 for native executables requiring no runtime dependencies.

## Objectives

- Provide a simple, offline-first GUI for serial communication with flight controller boards
- Visualize serial data in real time via dynamic multi-graph plotter (any `name@plotId:value` data, not limited to IMU)
- Integrate with PlatformIO to compile and flash firmware (future)
- Eliminate the need for users to run scripts or install runtimes

## Architecture: fc_tool vs PlatformIO Boundary

fc_tool has two layers with a clean dependency boundary:

```
┌─────────────────────────────────────────────────────────┐
│  fc_tool (standalone — no external dependencies)        │
│                                                         │
│  ┌─────────────────┐  ┌──────────────────────────────┐  │
│  │ Serial Monitor  │  │ Dynamic Serial Plotter       │  │
│  │ (serialport-rs) │  │ (Chart.js, multi-graph)      │  │
│  └─────────────────┘  └──────────────────────────────┘  │
├─────────────────────────────────────────────────────────┤
│  fc_tool + PlatformIO (optional — requires pio on PATH) │
│                                                         │
│  ┌─────────────────┐  ┌──────────────────────────────┐  │
│  │ Firmware Compile │  │ Firmware Flash/Upload        │  │
│  │ (pio run)        │  │ (pio run --target upload)    │  │
│  └─────────────────┘  └──────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### What fc_tool owns (standalone, no dependencies)

- **Serial port detection and connection** — via `serialport-rs` (Rust). No PlatformIO serial monitor dependency. fc_tool talks to the USB serial device directly.
- **Serial terminal** — send/receive raw serial data.
- **Dynamic serial plotter** — parses `name@plotId:value` (and other formats) from the serial stream, creates Chart.js graphs dynamically per plot ID. Dark theme, crosshair, neon data palette.
- **Auto-reconnect** — detects device disconnect, automatically retries connection.

These features work with just the compiled fc_tool binary. No Python, no PlatformIO, no Node.js at runtime.

### What fc_tool delegates to PlatformIO (optional)

- **Firmware compilation** — fc_tool calls `pio run` (PlatformIO CLI) pointed at the `flight_controller/` project.
- **Firmware flashing** — fc_tool calls `pio run --target upload`.
- **Board detection for upload** — PlatformIO handles which upload protocol to use (Teensy Loader, esptool, etc.).

fc_tool detects PlatformIO at startup by checking if `pio` or `platformio` is on `$PATH`. If not found, compile/flash features are disabled in the GUI. Everything else still works.

### Why this split

| Concern | fc_tool owns it? | Why |
|---------|-----------------|-----|
| Serial comms | Yes | serialport-rs is better than shelling out to pio device monitor. No Python dependency. Direct USB access. |
| Data parsing/plotting | Yes | fc_tool defines how to parse telemetry. Tightly coupled to the GUI. |
| Firmware compile | No — delegates to PlatformIO | PlatformIO already handles toolchains, board definitions, library management. Reimplementing this would be reinventing the wheel. |
| Firmware flash | No — delegates to PlatformIO | Upload protocols (Teensy Loader, esptool) are complex and platform-specific. PlatformIO handles this correctly. |

### PlatformIO installation

PlatformIO is Python-based and installs cross-platform via its official installer script (`get-platformio.py`). The `setup-dev` scripts on each platform install PlatformIO automatically alongside Rust and Node.js.

PlatformIO installs to `~/.platformio/` on all platforms (Linux, Windows, macOS). When fc_tool triggers a firmware build via `pio run`, PlatformIO automatically downloads the required toolchains, board definitions, and libraries on first use — no manual setup required.

fc_tool does **not** bundle Python or PlatformIO in its compiled binary. The binary is standalone for core features (serial, IMU). PlatformIO is an optional tool installed on the developer's machine.

## Cross-Platform Build Strategy

### The problem

Tauri apps use the OS-native webview (WebKit on Linux, WebView2 on Windows, WKWebView on macOS). You cannot cross-compile from Linux to Windows/macOS because the native webview SDK and system libraries are platform-specific.

### The solution: Local builds on each platform

Each platform has its own build scripts in `dev_setup/<platform>/`. To build for a platform, clone the repo on that platform and run the scripts.

| Platform | Output format | How |
|----------|---------------|-----|
| Linux | `.deb`, `.rpm`, `.AppImage` | Run `dev_setup/linux/build.sh` |
| Windows | `.msi`, `.exe` installer | Run `dev_setup/windows/build.bat` |
| macOS | `.dmg`, `.app` bundle | Run `dev_setup/macos/build.sh` |

### Target platforms (priority order)

1. **Linux** — primary development platform
2. **Windows** — end-user platform
3. **macOS** — end-user platform (lowest priority)

### Development workflow

Clone the repo on the target platform, run the setup scripts, and build locally:

```
dev_setup/
├── linux/
│   ├── install-system-deps.sh   # sudo apt-get (once)
│   ├── setup-dev.sh             # Rust, Node.js (once)
│   └── build.sh                 # compile or dev mode
├── windows/
│   ├── install-system-deps.bat  # MSVC, WebView2 (Admin, once)
│   ├── setup-dev.bat            # Rust, Node.js (once)
│   └── build.bat                # compile or dev mode
└── macos/
    ├── install-system-deps.sh   # Xcode CLT (once)
    ├── setup-dev.sh             # Rust, Node.js (once)
    └── build.sh                 # compile or dev mode
```

## Installation Conventions

All setup scripts follow these principles to keep things predictable and debuggable.

### Install locations

Everything installs to default, well-known locations in the user's home directory. No custom paths, no hidden magic.

| Tool | Install location | Managed by |
|------|-----------------|------------|
| Rust / Cargo | `~/.cargo/` and `~/.rustup/` | rustup (default) |
| Node.js | `~/.nvm/` (Linux/macOS) or system-wide via winget (Windows) | nvm / winget |
| PlatformIO | `~/.platformio/` | get-platformio.py (official installer) |
| npm packages | `fc_tool/node_modules/` (project-local) | npm |
| Cargo crates | `fc_tool/src-tauri/target/` (project-local) | cargo |

### Environment variables

The setup scripts configure the necessary PATH entries so tools are available in the current shell. Each build script re-sources these at the top so there's no dependency on the user's shell profile being configured correctly.

| Variable | Set by | Value |
|----------|--------|-------|
| `PATH` (Rust) | `. $HOME/.cargo/env` | Adds `~/.cargo/bin/` |
| `NVM_DIR` | setup-dev.sh | `~/.nvm/` |
| `PATH` (Node) | `. $NVM_DIR/nvm.sh` | Adds nvm-managed node binary |
| `PATH` (PlatformIO) | setup-dev scripts | Adds `~/.platformio/penv/bin/` (Linux/macOS) or `%USERPROFILE%\.platformio\penv\Scripts\` (Windows) |

### PlatformIO auto-download behavior

When fc_tool calls `pio run` to compile firmware, PlatformIO automatically:
1. Downloads the correct compiler toolchain for the target board (e.g. ARM GCC for Teensy)
2. Downloads board definitions and framework packages
3. Resolves and installs library dependencies from `platformio.ini`

No manual toolchain setup is needed. The first build takes longer because of these downloads; subsequent builds use the cached toolchains in `~/.platformio/`.

### Script validation status

| Platform | install-system-deps | setup-dev | build | Notes |
|----------|-------------------|-----------|-------|-------|
| Linux (Ubuntu/Debian) | Validated | Validated | Validated | Primary dev platform, fully tested |
| Windows | Not yet validated | Not yet validated | Not yet validated | Scripts written, need testing on real Windows machine |
| macOS | Not yet validated | Not yet validated | Not yet validated | Scripts written, need testing on real macOS machine |

Scripts for unvalidated platforms include `# TODO: UNTESTED` comments at the top marking them as draft. They follow the same patterns as the Linux scripts and should work, but need manual verification.

## Requirements

### Functional Requirements

- [x] Connect to serial ports (auto-detect and manual selection)
- [x] Serial monitor/terminal with send and receive
- [x] Dynamic multi-graph serial plotter (`name@plotId:value` protocol)
- [x] Board detection (Teensy, Arduino, ESP32 by VID/PID)
- [x] Auto-reconnect on device disconnect
- [ ] Integrate with PlatformIO CLI for firmware compilation and flashing
- [ ] Calibration parameter display and visualization
- [ ] Graceful degradation when PlatformIO is not installed

### Technical Requirements

- [ ] Rust backend with Tauri framework
- [ ] Web-based frontend (HTML/CSS/JS) for GUI
- [ ] Cross-platform: Windows, macOS, Linux
- [ ] Single executable distribution (no runtime dependencies for core features)
- [ ] Offline-first operation (no internet required)

### Resource Requirements

- [ ] Rust toolchain (stable)
- [ ] Node.js (for Tauri frontend build)
- [ ] PlatformIO CLI (optional — for compile/flash integration only)

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| Must run offline | Developer-first tool, no cloud dependency | No |
| Rust + Tauri | Cross-platform compiled executables, decided by operator | No |
| No paid services | Open-source project | No |
| Serial monitoring is standalone | Must not depend on PlatformIO for serial | No |
| PlatformIO for compile/flash | Existing build system for flight_controller firmware | No |
| [ASSUMED] Teensy-first, ESP32 later | ESP32 firmware not yet compiled in flight_controller | Yes |

## Assumptions

- [VERIFIED] Primary use case is developer workflow: change params, compile, flash, monitor
- [VERIFIED] PlatformIO is optional; core features work without it
- [VERIFIED] Windows and macOS are the primary end-user platforms; Linux is dev platform
- [VERIFIED] Frontend uses Chart.js 4.x for dynamic data visualization
- [VERIFIED] Serial plotter protocol: `name@plotId:value`, `name:value`, `name=value`, plain CSV
- [ASSUMED] PlatformIO CLI is pip-installable on all target platforms
- [ASSUMED] Teensy boards use USB serial (CDC) for communication

## Boundaries

### In Scope

- Serial port connection and monitoring (standalone, no PlatformIO)
- Dynamic multi-graph serial plotter (any named data, not IMU-specific)
- Auto-reconnect on device disconnect
- Board detection (Teensy, Arduino, ESP32)
- PlatformIO compile and flash integration (optional, future)
- Calibration parameter viewing (future)
- Connection management (port selection, baud rate)

### Out of Scope (Exclusions)

- This project will NOT implement its own firmware compiler (delegates to PlatformIO)
- This project will NOT provide OTA/WiFi firmware updates (that belongs to flight computer phase)
- This project will NOT be a full ground control station (GCS) — no mission planning, no maps
- This project will NOT bundle PlatformIO or Python — users install separately
- This project will NOT cross-compile from Linux to other platforms — build on each platform locally
- 3D attitude visualization is a future enhancement, not MVP
- ESP32 support: board detection works, firmware compilation is separate concern

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Language | Rust | Best serial library ecosystem, compiled executables | 2026-01-27 |
| GUI Framework | Tauri 2 | Web frontend + Rust backend, small binaries, OS webview | 2026-01-27 |
| Serial Library | serialport-rs | Most mature cross-platform serial in Rust. Standalone, no Python. | 2026-01-27 |
| Charting | Chart.js 4.x | Rich plugin API, adequate perf for sensor data, UMD global | 2026-02-06 |
| Frontend | Vanilla JS (ES modules) | No bundler needed, Tauri serves src/ directly | 2026-02-06 |
| Build Integration | PlatformIO CLI (optional) | Existing build system for flight_controller. Not bundled. | 2026-01-27 |
| Cross-platform builds | Local builds per platform | Cannot cross-compile Tauri; build natively on each OS | 2026-01-27 |
| Serial monitoring | Standalone (not PlatformIO) | Avoids Python dependency for core features | 2026-01-27 |

## Integration Points

- **flight_controller/** — PlatformIO project that fc_tool compiles and flashes
  - Firmware has two modes: **calibration mode** (mutable offsets, debug output) and **live mode** (hard-coded values, lean)
  - fc_tool is particularly valuable during calibration mode: visualizing IMU data, displaying calibration values, and helping users export values for hard-coding into config.h
  - See [flight_controller/docs/scope.md](/flight_controller/docs/scope.md) for firmware architecture
  - See [flight_controller/docs/findings/auto-calibration-research.md](/flight_controller/docs/findings/auto-calibration-research.md) for calibration approach research
- **PlatformIO CLI** — called by fc_tool for build/upload operations (optional)
- **Serial data protocol** — fc_tool's plotter accepts `name@plotId:value`, `name:value`, `name=value`, and plain CSV. Firmware can use any of these formats via `Serial.print()`

## Open Questions

- [ ] Should fc_tool edit platformio.ini or calibration header files directly for parameter changes?
- [ ] Should fc_tool support multiple simultaneous serial connections?

## Critical Notes

- The serial plotter protocol (`name@plotId:value`) is flexible enough that firmware doesn't need a strict contract — any key-value or CSV output works
- PlatformIO is optional. fc_tool must be fully usable for monitoring without it.
- Cross-platform builds require building locally on each platform.

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-09 | Updated for dynamic plotter (replaced IMU-specific references), auto-reconnect, Chart.js decisions, verified assumptions | LLM + User |
| 2026-02-05 | Updated integration points with flight_controller calibration workflow context, promoted to Active status | LLM + User |
| 2026-01-27 | Added architecture boundary (standalone vs PlatformIO), cross-platform build strategy, CI/CD plan | LLM + User |
| 2026-01-27 | Initial draft | LLM + User |

---

*This document evolves as the project develops. Requirements, constraints, and boundaries can be added, modified, or removed as understanding improves. Major scope changes should be discussed with the user.*
