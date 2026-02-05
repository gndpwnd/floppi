# fc_tool

Cross-platform desktop tool for floppi flight controller firmware management and real-time diagnostics.

## Overview

fc_tool provides a native GUI for serial communication, real-time IMU sensor visualization, and PlatformIO-based firmware compilation and flashing. Built in Rust with Tauri, it ships as a single executable per platform with no runtime dependencies.

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

```powershell
cd fc_tool
.\dev_setup\windows\install-system-deps.ps1      # MSVC, WebView2 (Admin, once)
.\dev_setup\windows\setup-dev.ps1                # Rust, Node.js (once)
.\dev_setup\windows\build.ps1                    # release build
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
| `tokio` | Async runtime for serial I/O |
| `serde` / `serde_json` | Serialization between Rust backend and JS frontend |
| `tauri-plugin-opener` | System URL/file opener |

## Usage

1. Connect a Teensy board via USB
2. Launch fc_tool
3. Click "Scan Ports" to detect connected boards
4. Select a serial port and connect
5. Use the serial monitor to interact with the board
6. View real-time IMU data in the plots tab

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
│   ├── windows/            # install-system-deps.ps1, setup-dev.ps1, build.ps1
│   └── macos/              # install-system-deps.sh, setup-dev.sh, build.sh
├── tests/              # Test files
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

- **Standalone (no dependencies):** Serial monitor, IMU plots, calibration display. Uses `serialport-rs` directly. No Python, no PlatformIO needed.
- **Optional PlatformIO integration:** Compile and flash firmware. Enabled when `pio` is on `$PATH`, grayed out otherwise.

## Documentation

- [scope.md](scope.md) - What this project is and isn't
- [roadmap.md](roadmap.md) - Feature progress
- [todo.md](todo.md) - Current tasks

## License

See floppi project license (TBD).

---

*For detailed project boundaries and technical decisions, see `docs/scope.md`*
