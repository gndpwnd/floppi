# fc_tool - Scope

> Last updated: 2026-02-18
> Status: Active

---

## Overview

fc_tool is a cross-platform desktop application for interacting with floppi flight controller hardware. It provides serial monitoring and dynamic multi-graph data plotting. Built in Rust with Tauri 2 for native executables requiring no runtime dependencies.

## Objectives

- Provide a simple, offline-first GUI for serial communication with flight controller boards
- Visualize serial data in real time via dynamic multi-graph plotter (any `name@plotId:value` data, not limited to IMU)
- Provide measurement and signal analysis tools for debugging and tuning

## Architecture

fc_tool is a standalone desktop application with no external dependencies at runtime:

- **Serial port detection and connection** — via `serialport-rs` (Rust). Direct USB access.
- **Serial terminal** — send/receive raw serial data.
- **Dynamic serial plotter** — parses `name@plotId:value` (and other formats) from the serial stream, creates Chart.js graphs dynamically per plot ID. Dark theme, crosshair, neon data palette.
- **Auto-reconnect** — detects device disconnect, automatically retries connection.

No Python, no PlatformIO, no Node.js at runtime. Firmware compilation and flashing are handled via existing PlatformIO scripts outside of fc_tool.

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
| npm packages | `fc_tool/node_modules/` (project-local) | npm |
| Cargo crates | `fc_tool/src-tauri/target/` (project-local) | cargo |

### Environment variables

The setup scripts configure the necessary PATH entries so tools are available in the current shell. Each build script re-sources these at the top so there's no dependency on the user's shell profile being configured correctly.

| Variable | Set by | Value |
|----------|--------|-------|
| `PATH` (Rust) | `. $HOME/.cargo/env` | Adds `~/.cargo/bin/` |
| `NVM_DIR` | setup-dev.sh | `~/.nvm/` |
| `PATH` (Node) | `. $NVM_DIR/nvm.sh` | Adds nvm-managed node binary |

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

### Technical Requirements

- [ ] Rust backend with Tauri framework
- [ ] Web-based frontend (HTML/CSS/JS) for GUI
- [ ] Cross-platform: Windows, macOS, Linux
- [ ] Single executable distribution (no runtime dependencies for core features)
- [ ] Offline-first operation (no internet required)

### Resource Requirements

- [ ] Rust toolchain (stable)
- [ ] Node.js (for Tauri frontend build)

## Constraints

| Constraint | Reason | Flexible? |
|------------|--------|-----------|
| Must run offline | Developer-first tool, no cloud dependency | No |
| Rust + Tauri | Cross-platform compiled executables, decided by operator | No |
| No paid services | Open-source project | No |
| Standalone | No external dependencies at runtime (no Python, no PlatformIO) | No |

## Assumptions

- [VERIFIED] Primary use case is developer workflow: monitor serial data, visualize, analyze
- [VERIFIED] Windows and macOS are the primary end-user platforms; Linux is dev platform
- [VERIFIED] Frontend uses Chart.js 4.x for dynamic data visualization
- [VERIFIED] Serial plotter protocol: `name@plotId:value`, `name:value`, `name=value`, plain CSV
- [ASSUMED] Teensy boards use USB serial (CDC) for communication

## Boundaries

### In Scope

- Serial port connection and monitoring (standalone)
- Dynamic multi-graph serial plotter (any named data, not IMU-specific)
- Auto-reconnect on device disconnect
- Board detection (Teensy, Arduino, ESP32)
- Connection management (port selection, baud rate)
- Measurement cursors (2 vertical + 2 horizontal per plot, draggable, delta readout)
- X/Y axis zoom and pan controls
- Signal statistics readout (min, max, avg per variable)
- Live dashboard mode (key=value grid updating in place)
- Headless mode, CLI arguments, raw data logging with timestamps
- USB hot-plug detection
- Future: period mode, signal analysis

### Out of Scope (Exclusions)

- This project will NOT compile or flash firmware (use PlatformIO scripts directly)
- This project will NOT provide OTA/WiFi firmware updates (that belongs to flight computer phase)
- This project will NOT be a full ground control station (GCS) — no mission planning, no maps
- This project will NOT cross-compile from Linux to other platforms — build on each platform locally
- 3D attitude visualization is a future enhancement, not MVP

## Technical Decisions

| Decision | Choice | Rationale | Date |
|----------|--------|-----------|------|
| Language | Rust | Best serial library ecosystem, compiled executables | 2026-01-27 |
| GUI Framework | Tauri 2 | Web frontend + Rust backend, small binaries, OS webview | 2026-01-27 |
| Serial Library | serialport-rs | Most mature cross-platform serial in Rust. Standalone, no Python. | 2026-01-27 |
| Charting | Chart.js 4.x | Rich plugin API, adequate perf for sensor data, UMD global | 2026-02-06 |
| Frontend | Vanilla JS (ES modules) | No bundler needed, Tauri serves src/ directly | 2026-02-06 |
| Cross-platform builds | Local builds per platform | Cannot cross-compile Tauri; build natively on each OS | 2026-01-27 |

## Integration Points

- **flight_controller/** — fc_tool monitors and visualizes serial data from this firmware
  - See [flight_controller/docs/scope.md](/flight_controller/docs/scope.md) for firmware architecture
- **Serial data protocol** — fc_tool's plotter accepts `name@plotId:value`, `name:value`, `name=value`, and plain CSV. Firmware can use any of these formats via `Serial.print()`

## Open Questions

- [ ] Should fc_tool support multiple simultaneous serial connections?

## Testing Policy

- **All testing via test scripts or pytest** in `tests/`, never manual terminal commands
- **Two test systems**: pytest (240 tests, modular, preferred) and bash scripts (29 tests, legacy)
- Tests use `simulate_serial.py` to generate fake data and `socat` for virtual serial ports
- pytest fixtures manage socat automatically — no manual port setup needed
- LLM/agents must never run ad-hoc hardware commands — they will fail and get stuck on serial timing, port locking, and USB enumeration issues
- Test scripts are the source of truth: if it doesn't have a test, it's not verified
- Test output goes to `tests/results/` (gitignored)
- Tests vs diagnostics: **tests** simulate fake data for development validation; **diagnostics** are built-in features for users to troubleshoot the live application

### Quick reference

```bash
# Prerequisites (once)
sudo apt-get install socat              # virtual serial ports
pip install pytest                       # pytest framework
sudo ./dev_setup/linux/install-system-deps.sh   # Tauri build deps

# pytest (preferred — modular, 240 tests)
cd tests && pytest                       # all tests
pytest -m unit                           # fast unit tests (<0.2s)
pytest -m integration                    # simulator tests (~15s)
pytest -m e2e                            # needs fc_tool binary + socat (~43s)
pytest test_parse_line.py::TestNamedPlotFormat::test_single_named_plot  # single test

# bash scripts (legacy — 29 tests)
./tests/test_plotter.sh                 # plotter test suite
./tests/test_monitor.sh                 # monitor test suite

# Verify Rust compiles
cd src-tauri && cargo check
```

See [README.md Testing section](README.md#testing) for full details.

## Critical Notes

- The serial plotter protocol (`name@plotId:value`) is flexible enough that firmware doesn't need a strict contract — any key-value or CSV output works
- Cross-platform builds require building locally on each platform
- Max 10 simultaneous plots (configurable in PlotterManager, default cap prevents runaway DOM creation)

---

## Revision History

| Date | Changes | By |
|------|---------|-----|
| 2026-02-18 | Updated Testing Policy with pytest framework (183 tests), updated In Scope with completed features (cursors, dashboard, headless, zoom/pan) | LLM + User |
| 2026-02-17 | Added Testing Policy section (bash scripts only, no manual commands), max plots cap, tests vs diagnostics distinction | LLM + User |
| 2026-02-10 | Removed PlatformIO integration and Calibration UI from scope (handled externally via scripts) | LLM + User |
| 2026-02-09 | Updated for dynamic plotter (replaced IMU-specific references), auto-reconnect, Chart.js decisions, verified assumptions | LLM + User |
| 2026-02-05 | Updated integration points with flight_controller calibration workflow context, promoted to Active status | LLM + User |
| 2026-01-27 | Added architecture boundary (standalone vs PlatformIO), cross-platform build strategy, CI/CD plan | LLM + User |
| 2026-01-27 | Initial draft | LLM + User |

---

*This document evolves as the project develops. Requirements, constraints, and boundaries can be added, modified, or removed as understanding improves. Major scope changes should be discussed with the user.*
