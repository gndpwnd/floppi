# fc_tool - Scope

> Last updated: 2026-02-05
> Status: Active

---

## Overview

fc_tool is a cross-platform desktop application for interacting with floppi flight controller hardware. It provides serial monitoring, real-time IMU data visualization, and firmware management via PlatformIO integration. Built in Rust with Tauri for native executables requiring no runtime dependencies.

## Objectives

- Provide a simple, offline-first GUI for serial communication with flight controller boards
- Visualize IMU/MPU sensor data in real time (accelerometer, gyroscope, magnetometer)
- Integrate with PlatformIO to compile and flash firmware when users change calibration parameters or configuration
- Eliminate the need for users to run scripts or install runtimes

## Architecture: fc_tool vs PlatformIO Boundary

fc_tool has two layers with a clean dependency boundary:

```
┌─────────────────────────────────────────────────────────┐
│  fc_tool (standalone — no external dependencies)        │
│                                                         │
│  ┌─────────────────┐  ┌──────────────────────────────┐  │
│  │ Serial Monitor  │  │ IMU/Sensor Visualization     │  │
│  │ (serialport-rs) │  │ (charts, calibration view)   │  │
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
- **IMU data visualization** — parses telemetry from the serial stream and renders charts.
- **Calibration display** — shows calibration values from live telemetry.

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
| IMU parsing | Yes | fc_tool defines how to parse telemetry. Tightly coupled to the GUI. |
| Firmware compile | No — delegates to PlatformIO | PlatformIO already handles toolchains, board definitions, library management. Reimplementing this would be reinventing the wheel. |
| Firmware flash | No — delegates to PlatformIO | Upload protocols (Teensy Loader, esptool) are complex and platform-specific. PlatformIO handles this correctly. |

### PlatformIO installation

PlatformIO is Python-based and installs cross-platform via its official installer script (`get-platformio.py`). The `setup-dev` scripts on each platform install PlatformIO automatically alongside Rust and Node.js.

PlatformIO installs to `~/.platformio/` on all platforms (Linux, Windows, macOS). When fc_tool triggers a firmware build via `pio run`, PlatformIO automatically downloads the required toolchains, board definitions, and libraries on first use — no manual setup required.

fc_tool does **not** bundle Python or PlatformIO in its compiled binary. The binary is standalone for core features (serial, IMU). PlatformIO is an optional tool installed on the developer's machine.

## Cross-Platform Build Strategy

### The problem

Tauri apps use the OS-native webview (WebKit on Linux, WebView2 on Windows, WKWebView on macOS). You cannot cross-compile from Linux to Windows/macOS because the native webview SDK and system libraries are platform-specific.

### The solution: CI/CD builds on each platform

```
┌─────────────────────────────────────────────────────────┐
│  GitHub Actions CI                                      │
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │ Linux    │  │ Windows  │  │ macOS    │              │
│  │ runner   │  │ runner   │  │ runner   │              │
│  │          │  │          │  │          │              │
│  │ .deb     │  │ .msi     │  │ .dmg     │              │
│  │ .AppImage│  │ .exe     │  │ .app     │              │
│  │ .rpm     │  │          │  │          │              │
│  └──────────┘  └──────────┘  └──────────┘              │
│                      │                                  │
│               GitHub Releases                           │
│            (pre-built binaries)                          │
└─────────────────────────────────────────────────────────┘
```

| Platform | Output format | How |
|----------|---------------|-----|
| Linux | `.deb`, `.rpm`, `.AppImage` | Build natively or on Linux CI runner |
| Windows | `.msi`, `.exe` installer | Build on Windows CI runner (GitHub Actions has these) |
| macOS | `.dmg`, `.app` bundle | Build on macOS CI runner (GitHub Actions has these) |

### Target platforms (priority order)

1. **Windows** — primary end-user platform
2. **macOS** — primary end-user platform
3. **Linux** — development platform, also supported for end users

### Development workflow vs release workflow

There are two separate workflows, each with a clear purpose:

**Local development (fast iteration, bug fixing):**

Clone the repo on the target platform, run the setup scripts, and build locally. Each platform has its own scripts in `dev_setup/<platform>/`. This is for:
- Day-to-day development on Linux
- Fixing platform-specific bugs on Windows or macOS
- Testing GUI behavior on each OS
- Quick compile-run-test cycles

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

**Automated releases (publishing binaries for users):**

Manually triggered GitHub Actions workflow (`.github/workflows/fc_tool-release.yml`). This is for:
- Publishing tagged releases with pre-built binaries for all platforms
- Letting users download from GitHub Releases without building anything
- Triggered manually from the GitHub Actions UI (never on push)

```
You click "Run workflow" > enter version tag (e.g. v0.1.0) >
CI builds on Linux + Windows + macOS runners >
Binaries uploaded to GitHub Releases page >
Users download the installer for their platform
```

### Release process

1. Develop and test on Linux (primary dev machine)
2. When ready to release: optionally test on Windows/macOS locally by cloning and running `dev_setup/<platform>/` scripts
3. Go to GitHub > Actions > "fc_tool Release Build" > Run workflow
4. Enter version tag (e.g. `v0.1.0`), choose whether to mark as pre-release
5. CI builds all three platforms, creates a GitHub Release with binaries
6. Users download `.msi` (Windows), `.dmg` (macOS), `.deb`/`.AppImage` (Linux)

**The workflow is manual-only.** It never triggers on push or merge. You decide when a release happens.

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

- [ ] Connect to serial ports (auto-detect and manual selection)
- [ ] Serial monitor/terminal with send and receive
- [ ] Parse and display IMU telemetry data in real-time plots
- [ ] Integrate with PlatformIO CLI for firmware compilation and flashing
- [ ] Board detection (Teensy 4.0/4.1 initially, ESP32 later)
- [ ] Calibration parameter display and visualization
- [ ] Graceful degradation when PlatformIO is not installed

### Technical Requirements

- [ ] Rust backend with Tauri framework
- [ ] Web-based frontend (HTML/CSS/JS) for GUI
- [ ] Cross-platform: Windows, macOS, Linux
- [ ] Single executable distribution (no runtime dependencies for core features)
- [ ] Offline-first operation (no internet required)
- [ ] GitHub Actions CI for multi-platform builds

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
| CI for cross-platform builds | Cannot cross-compile Tauri from Linux | No |
| [ASSUMED] Teensy-first, ESP32 later | ESP32 firmware not yet compiled in flight_controller | Yes |

## Assumptions

- [VERIFIED] Primary use case is developer workflow: change params, compile, flash, monitor
- [VERIFIED] Firmware does not yet have a defined serial telemetry protocol — will be defined as firmware matures
- [VERIFIED] PlatformIO is optional; core features work without it
- [VERIFIED] Windows and macOS are the primary end-user platforms; Linux is dev platform
- [ASSUMED] PlatformIO CLI is pip-installable on all target platforms
- [ASSUMED] Teensy boards use USB serial (CDC) for communication
- [ASSUMED] Frontend will use charting libraries (e.g., Chart.js, Plotly, or similar) for IMU visualization

## Boundaries

### In Scope

- Serial port connection and monitoring (standalone, no PlatformIO)
- Real-time IMU data plotting (accel, gyro, mag)
- PlatformIO compile and flash integration (optional)
- Teensy 4.0/4.1 board support
- Calibration parameter viewing
- Connection management (port selection, baud rate)
- Data logging and export
- GitHub Actions CI for Windows/macOS/Linux builds

### Out of Scope (Exclusions)

- This project will NOT implement its own firmware compiler (delegates to PlatformIO)
- This project will NOT provide OTA/WiFi firmware updates (that belongs to flight computer phase)
- This project will NOT be a full ground control station (GCS) — no mission planning, no maps
- This project will NOT bundle PlatformIO or Python — users install separately
- This project will NOT cross-compile from Linux to other platforms — CI handles this
- 3D attitude visualization is a future enhancement, not MVP
- ESP32 support is deferred until firmware is compiled for it

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Language | Rust | Best serial library ecosystem, compiled executables | 2026-01-27 |
| GUI Framework | Tauri | Web frontend + Rust backend, small binaries, OS webview | 2026-01-27 |
| Serial Library | serialport-rs | Most mature cross-platform serial in Rust. Standalone, no Python. | 2026-01-27 |
| Build Integration | PlatformIO CLI (optional) | Existing build system for flight_controller. Not bundled. | 2026-01-27 |
| Cross-platform builds | GitHub Actions CI | Cannot cross-compile Tauri; CI builds natively on each OS | 2026-01-27 |
| Serial monitoring | Standalone (not PlatformIO) | Avoids Python dependency for core features | 2026-01-27 |

## Integration Points

- **flight_controller/** — PlatformIO project that fc_tool compiles and flashes
  - Firmware has two modes: **calibration mode** (mutable offsets, debug output) and **live mode** (hard-coded values, lean)
  - fc_tool is particularly valuable during calibration mode: visualizing IMU data, displaying calibration values, and helping users export values for hard-coding into config.h
  - See [flight_controller/docs/scope.md](/flight_controller/docs/scope.md) for firmware architecture
  - See [flight_controller/docs/findings/auto-calibration-research.md](/flight_controller/docs/findings/auto-calibration-research.md) for calibration approach research
- **PlatformIO CLI** — called by fc_tool for build/upload operations (optional)
- **Serial telemetry protocol** — TBD, will be co-designed between firmware and fc_tool. The firmware currently uses debug `Serial.print()` output; a structured protocol is needed for reliable parsing
- **GitHub Actions** — CI/CD for multi-platform binary releases

## Open Questions

- [ ] What serial telemetry format will the firmware use? (needs firmware-side design)
- [ ] Should fc_tool edit platformio.ini or calibration header files directly for parameter changes?
- [ ] What baud rate will be standard for telemetry? (115200 default assumed)
- [ ] Should fc_tool support multiple simultaneous serial connections?
- [ ] GitHub Actions workflow: trigger on tags only, or on every push to main?

## Critical Notes

- The serial telemetry protocol is a shared contract between firmware and fc_tool — changes to one affect the other
- PlatformIO is optional. fc_tool must be fully usable for monitoring without it.
- Cross-platform builds depend on GitHub Actions CI. Local development builds are Linux-only.

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-05 | Updated integration points with flight_controller calibration workflow context, promoted to Active status | LLM + User |
| 2026-01-27 | Added architecture boundary (standalone vs PlatformIO), cross-platform build strategy, CI/CD plan | LLM + User |
| 2026-01-27 | Initial draft | LLM + User |

---

*This document evolves as the project develops. Requirements, constraints, and boundaries can be added, modified, or removed as understanding improves. Major scope changes should be discussed with the user.*
