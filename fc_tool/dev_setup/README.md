# fc_tool Development Setup

Platform-specific scripts for setting up the fc_tool build environment.

## Quick Start

Pick your platform and run the scripts in order:

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
REM Run as Administrator:
dev_setup\windows\install-system-deps.bat        REM MSVC, WebView2 (once)
REM Run as normal user:
dev_setup\windows\setup-dev.bat                  REM Rust, Node.js (once)
dev_setup\windows\build.bat                      REM release build
dev_setup\windows\build.bat dev                  REM dev mode
```

### macOS

```bash
cd fc_tool
./dev_setup/macos/install-system-deps.sh         # Xcode CLT (once)
./dev_setup/macos/setup-dev.sh                   # Rust, Node.js (once)
./dev_setup/macos/build.sh                       # release build
./dev_setup/macos/build.sh dev                   # dev mode
```

## Script Separation

Each platform has three scripts with the same names and roles:

| Script | Privilege | Purpose |
|--------|-----------|---------|
| `install-system-deps.*` | Admin/sudo | OS-level packages (compilers, webview, libraries) |
| `setup-dev.*` | Normal user | Rust, Node.js, npm dependencies |
| `build.*` | Normal user | Compile release binary or run dev mode |

## Platform Notes

### Linux
- Requires `apt` packages: webkit2gtk, GTK3, libudev, etc.
- Produces: `.deb`, `.rpm`, `.AppImage`

### Windows
- Requires Visual Studio Build Tools (MSVC) and WebView2 Runtime
- WebView2 is pre-installed on Windows 10/11
- Produces: `.msi`, `.exe` (NSIS installer)

### macOS
- Requires Xcode Command Line Tools only
- WKWebView is built into macOS — no extra webview package needed
- Produces: `.dmg`, `.app` bundle
- No `sudo` needed for any script

## Automated Releases

For automated multi-platform builds, see `.github/workflows/fc_tool-release.yml`.
This is a manually-triggered GitHub Actions workflow that builds on all three platforms and publishes to GitHub Releases.
