# fc_tool

Cross-platform desktop tool for floppi flight controller firmware management and real-time diagnostics.

## Overview

fc_tool provides a native GUI for serial communication, real-time IMU sensor visualization, and PlatformIO-based firmware compilation and flashing. Built in Rust with Tauri, it ships as a single executable per platform with no runtime dependencies.

## Quick Start (Ubuntu/Debian)

Three scripts, run in order:

```bash
# 1. Install system libraries (requires sudo, run once)
sudo ./install-system-deps.sh

# 2. Install Rust, Node.js, npm deps (no sudo, run once)
./setup-dev.sh

# 3. Build the release binary (no sudo, run as needed)
./build.sh              # release binary
./build.sh dev          # development mode with live reload
```

The release binary will be in `src-tauri/target/release/bundle/`.

### Scripts Summary

| Script | Sudo? | Run when | What it does |
|--------|-------|----------|--------------|
| `install-system-deps.sh` | Yes | Once per machine | Installs apt packages (gcc, webkit2gtk, GTK, libudev, etc.) |
| `setup-dev.sh` | No | Once per user | Installs Rust (rustup), Node.js (nvm), npm dependencies |
| `build.sh` | No | Each build | Compiles Rust + frontend into a release binary or runs dev mode |

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
│   └── findings/       # Research and investigation docs
├── src-tauri/          # Rust backend (Tauri)
│   ├── Cargo.toml      # Rust dependencies
│   └── src/
│       ├── lib.rs      # Tauri commands (serial port listing, etc.)
│       └── main.rs     # Entry point
├── src/                # Frontend (HTML/CSS/JS)
│   ├── index.html
│   ├── main.js
│   └── styles.css
├── tests/              # Test files
├── setup-dev.sh        # Dev environment setup script (Ubuntu/Debian)
├── package.json        # npm config (Tauri CLI)
└── .gitignore
```

## Documentation

- [scope.md](scope.md) - What this project is and isn't
- [roadmap.md](roadmap.md) - Feature progress
- [todo.md](todo.md) - Current tasks

## License

See floppi project license (TBD).

---

*For detailed project boundaries and technical decisions, see `docs/scope.md`*
