# fc_tool

Cross-platform desktop tool for floppi flight controller serial monitoring and real-time data visualization.

## Run It

```bash
cd fc_tool && npx tauri dev
```

Launches the GUI with hot-reload. Requires system deps installed first (see [Quick Start](#quick-start) below).

## Overview

fc_tool provides a native GUI for serial communication and real-time data visualization (dynamic multi-graph plotter). Built in Rust with Tauri, it ships as a single executable per platform with no runtime dependencies. Firmware compilation and flashing are handled externally via PlatformIO scripts.

## Quick Start

All setup scripts are in `dev_setup/<platform>/`. See [dev_setup/README.md](../dev_setup/README.md) for full details.

### Linux (Ubuntu/Debian)

```bash
cd fc_tool
sudo ./dev_setup/linux/install-system-deps.sh   # system libs (once)
./dev_setup/linux/setup-dev.sh                   # Rust, Node.js (once)
./dev_setup/linux/build.sh                       # release build
./dev_setup/linux/build.sh dev                   # dev mode
```

### Windows

```cmd
cd fc_tool
dev_setup\windows\install-system-deps.bat        REM MSVC, WebView2 (Admin, once)
dev_setup\windows\setup-dev.bat                  REM Rust, Node.js (once)
dev_setup\windows\build.bat                      REM release build
```

### macOS

```bash
cd fc_tool
./dev_setup/macos/install-system-deps.sh         # Xcode CLT (once)
./dev_setup/macos/setup-dev.sh                   # Rust, Node.js (once)
./dev_setup/macos/build.sh                       # release build
```

The release binary will be in `src-tauri/target/release/bundle/`.

## Build Dependencies

These are only needed for **building** fc_tool. End users just run the compiled binary.

### System Packages (Ubuntu/Debian)

```bash
sudo apt-get install -y \
    build-essential \
    pkg-config \
    libwebkit2gtk-4.1-dev \
    librsvg2-dev \
    libssl-dev \
    libgtk-3-dev \
    libayatana-appindicator3-dev \
    libudev-dev \
    curl wget file
```

| Package | Why |
|---------|-----|
| `build-essential` | C/C++ compiler, linker (gcc, make) |
| `pkg-config` | Locates system libraries for Rust build scripts |
| `libwebkit2gtk-4.1-dev` | Tauri's webview engine (renders the GUI) |
| `librsvg2-dev` | SVG rendering for app icons |
| `libssl-dev` | TLS support |
| `libgtk-3-dev` | GTK3 for native window chrome |
| `libayatana-appindicator3-dev` | System tray support |
| `libudev-dev` | USB device detection (required by serialport-rs) |

### Toolchains

| Tool | Version | Install |
|------|---------|---------|
| Rust | stable | `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \| sh` |
| Node.js | 20 LTS | Via [nvm](https://github.com/nvm-sh/nvm) or [nodesource](https://deb.nodesource.com) |
| PlatformIO CLI | latest | `pip install platformio` (optional, for firmware compile/flash) |

### Rust Crates (managed by Cargo)

| Crate | Purpose |
|-------|---------|
| `tauri` | Desktop app framework (webview + native APIs) |
| `serialport` | Cross-platform serial port access |
| `serde` / `serde_json` | Serialization between Rust backend and JS frontend |
| `tauri-plugin-opener` | System URL/file opener |

## CLI Reference

```bash
# GUI mode (default)
fc_tool                                          # launch GUI
fc_tool --port /dev/ttyACM0 --baud 115200        # GUI with auto-connect

# Headless mode (no GUI, serial → stdout)
fc_tool --headless --port /dev/ttyACM0           # raw serial to stdout (default 115200)
fc_tool --headless --port /dev/ttyACM0 --baud 9600
fc_tool --headless --port /dev/ttyACM0 --log data.txt   # tee to file + stdout

# Utility
fc_tool --kill-port /dev/ttyACM0                 # force-release a port held by stale processes
```

| Flag | Description |
|------|-------------|
| `--port <path>` | Serial port path (e.g., `/dev/ttyACM0`, `COM3`) |
| `--baud <rate>` | Baud rate (default: 115200) |
| `--headless` | No GUI — raw serial data to stdout, stdin forwarded to serial |
| `--log <file>` | Tee serial data to file (headless mode only). Creates parent dirs. |
| `--kill-port <path>` | Force-release a serial port held by another process (`fuser -k` on Linux) |

## Usage

1. Connect a Teensy board via USB
2. Launch fc_tool
3. Click "Scan Ports" to detect connected boards
4. Select a serial port and connect
5. Use the serial monitor to interact with the board
6. Click "Show Plotter" to visualize serial data in real time

### PlatformIO Serial Monitor (fallback)

During fc_tool development, you can use PlatformIO's built-in serial monitor as a
fallback for interacting with the flight controller:

```bash
# Basic serial monitor (115200 baud, auto-detects port)
pio device monitor

# Specify port and baud rate explicitly
pio device monitor --port /dev/ttyACM0 --baud 115200

# With filters (useful for calibration output)
pio device monitor --filter direct
```

This is useful when:

- fc_tool is being rebuilt or modified
- You need a quick serial connection without launching the full GUI
- Debugging serial protocol issues at a lower level

## Development

After installing system deps (see Quick Start), you can run fc_tool directly:

```bash
# Dev mode with hot-reload (frontend changes reload automatically)
cd fc_tool && npx tauri dev

# Release build (output in src-tauri/target/release/bundle/)
cd fc_tool && npx tauri build
```

Note: System dependencies must be installed first (see Quick Start above).

## Testing

All testing is done via bash scripts in `tests/`. See [scope.md Testing Policy](scope.md#testing-policy) for rationale.

### Prerequisites

```bash
# socat — creates virtual serial port pairs for testing without hardware
sudo apt-get install socat

# System deps — needed for cargo build (see Quick Start above)
sudo ./dev_setup/linux/install-system-deps.sh
```

### Running Tests

```bash
cd fc_tool

# Run all plotter tests (simulator scenarios, format checks, stress test)
./tests/test_plotter.sh

# Run all monitor tests (ANSI, dashboard format, headless echo)
./tests/test_monitor.sh

# Run both suites
./tests/test_plotter.sh && ./tests/test_monitor.sh
```

Tests that require socat or a compiled binary will skip gracefully if dependencies are missing.

### Data Simulator

The simulator generates fake serial data in all supported protocol formats:

```bash
# List available scenarios
python3 tests/simulate_serial.py --help

# Run a scenario to stdout (useful for eyeball checks)
python3 tests/simulate_serial.py --scenario imu
python3 tests/simulate_serial.py --scenario sine
python3 tests/simulate_serial.py --scenario ansi

# Write to a virtual serial port (socat)
socat -d -d pty,raw,echo=0 pty,raw,echo=0   # note the two PTY paths it prints
python3 tests/simulate_serial.py --scenario imu --port /dev/pts/X
```

Available scenarios: `imu`, `sine`, `mixed`, `ansi`, `stress`, `dashboard`, `protocol`, `noise`

### Verifying Rust Changes

After modifying Rust code, verify it compiles before committing:

```bash
cd fc_tool/src-tauri
cargo check     # fast syntax + type check (no binary produced)
cargo build     # full debug build
cargo build --release   # optimized release build
```

Note: `cargo check` / `cargo build` require system deps installed (see Prerequisites above).

## Project Structure

```
fc_tool/
├── docs/
│   ├── README.md       # This file
│   ├── scope.md        # Project scope and boundaries
│   ├── roadmap.md      # Feature roadmap
│   ├── todo.md         # Current tasks
│   ├── features/       # Feature specifications
│   ├── findings/       # Research and investigation docs
│   └── archive/        # Session summaries and historical records
├── src-tauri/          # Rust backend (Tauri)
│   ├── Cargo.toml      # Rust dependencies
│   └── src/
│       ├── lib.rs      # Tauri commands (serial port listing, etc.)
│       └── main.rs     # Entry point
├── src/                # Frontend (HTML/CSS/JS)
│   ├── index.html
│   ├── main.js
│   └── styles.css
├── dev_setup/
│   ├── README.md           # Per-platform setup instructions
│   ├── linux/              # install-system-deps.sh, setup-dev.sh, build.sh
│   ├── windows/            # install-system-deps.bat, setup-dev.bat, build.bat
│   └── macos/              # install-system-deps.sh, setup-dev.sh, build.sh
├── tests/
│   ├── simulate_serial.py  # Data simulator (generates fake serial data)
│   ├── test_plotter.sh     # Plotter test suite (socat virtual serial)
│   ├── test_monitor.sh     # Monitor test suite (ANSI, formats, headless)
│   └── results/            # Test output (gitignored)
├── package.json            # npm config (Tauri CLI)
└── .gitignore
```

## Cross-Platform Builds

Tauri uses the OS-native webview, so you cannot cross-compile from Linux to Windows/macOS. Builds for each platform run natively via GitHub Actions CI.

| Platform | Output | Built on |
|----------|--------|----------|
| Linux | `.deb`, `.rpm`, `.AppImage` | Linux runner (or local) |
| Windows | `.msi`, `.exe` | Windows CI runner |
| macOS | `.dmg`, `.app` | macOS CI runner |

For local development on Linux, use `./build.sh` or `./build.sh dev`.

## Architecture

fc_tool has two layers — see [scope.md](scope.md) for full details:

- **Rust backend:** Serial port detection, connection management, data streaming via `serialport-rs`. No Python, no PlatformIO needed at runtime.
- **JS frontend:** Serial monitor terminal, dynamic multi-graph plotter (Chart.js), dark theme with neon data palette.

## Documentation

- [scope.md](scope.md) - What this project is and isn't
- [roadmap.md](roadmap.md) - Feature progress
- [todo.md](todo.md) - Current tasks

## License

See floppi project license (TBD).

---

*For detailed project boundaries and technical decisions, see `docs/scope.md`*
