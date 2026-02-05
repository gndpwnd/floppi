@echo off
REM fc_tool development environment setup for Windows
REM Installs Rust, Node.js, PlatformIO, and npm dependencies. No admin required.
REM
REM Install locations (all default paths):
REM   Rust:        %USERPROFILE%\.cargo\, %USERPROFILE%\.rustup\
REM   Node.js:     System-wide via winget (default Program Files location)
REM   PlatformIO:  %USERPROFILE%\.platformio\
REM   npm:         fc_tool\node_modules\ (project-local)
REM
REM Prerequisites: Run install-system-deps.bat first (as Administrator).
REM
REM Usage:
REM   setup-dev.bat

setlocal enabledelayedexpansion

REM Determine project directory (two levels up from this script)
set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%\..\.."
set "PROJECT_DIR=%CD%"
popd

echo === fc_tool dev environment setup (Windows) ===
echo.

REM ── 1. Verify system dependencies ─────────────────────────────

echo [1/5] Checking system dependencies...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo   Visual Studio Build Tools not found.
    echo   Run install-system-deps.bat as Administrator first.
    exit /b 1
)
echo   MSVC Build Tools detected.

REM ── 2. Rust toolchain ─────────────────────────────────────────
REM Installs to: %USERPROFILE%\.cargo\ and %USERPROFILE%\.rustup\

echo [2/5] Checking Rust toolchain...

REM Add cargo to PATH if it exists
if exist "%USERPROFILE%\.cargo\bin" (
    set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
)

where rustc >nul 2>&1
if %errorlevel% equ 0 (
    for /f "delims=" %%v in ('rustc --version') do echo   Rust already installed: %%v
) else (
    echo   Installing Rust via rustup...
    echo   Install location: %USERPROFILE%\.cargo\ and %USERPROFILE%\.rustup\
    curl -sSf -o "%TEMP%\rustup-init.exe" https://win.rustup.rs/x86_64
    "%TEMP%\rustup-init.exe" -y
    set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
    del /q "%TEMP%\rustup-init.exe" 2>nul
)

for /f "delims=" %%v in ('rustc --version 2^>nul') do echo   rustc: %%v
for /f "delims=" %%v in ('cargo --version 2^>nul') do echo   cargo: %%v

REM ── 3. Node.js ────────────────────────────────────────────────
REM Installs to: default winget location (Program Files)

echo [3/5] Checking Node.js...

where node >nul 2>&1
if %errorlevel% equ 0 (
    for /f "delims=" %%v in ('node --version') do echo   Node.js already installed: %%v
) else (
    echo   Installing Node.js 20 via winget...
    winget install OpenJS.NodeJS.LTS --accept-package-agreements --accept-source-agreements
    REM Refresh PATH to pick up newly installed Node
    for /f "tokens=2*" %%a in ('reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul') do set "PATH=%%b;%PATH%"
)

for /f "delims=" %%v in ('node --version 2^>nul') do echo   node: %%v
for /f "delims=" %%v in ('npm --version 2^>nul') do echo   npm: %%v

REM ── 4. PlatformIO (optional, for firmware compile/flash) ──────
REM Installs to: %USERPROFILE%\.platformio\

echo [4/5] Checking PlatformIO...

set "PIO_PENV=%USERPROFILE%\.platformio\penv\Scripts"
if exist "%PIO_PENV%" (
    set "PATH=%PIO_PENV%;%PATH%"
)

where pio >nul 2>&1
if %errorlevel% equ 0 (
    for /f "delims=" %%v in ('pio --version') do echo   PlatformIO already installed: %%v
) else (
    echo   Installing PlatformIO...
    echo   Install location: %USERPROFILE%\.platformio\

    REM Check for Python 3
    set "PYTHON_CMD="
    where python >nul 2>&1
    if %errorlevel% equ 0 (
        set "PYTHON_CMD=python"
    ) else (
        where python3 >nul 2>&1
        if %errorlevel% equ 0 (
            set "PYTHON_CMD=python3"
        )
    )

    if defined PYTHON_CMD (
        curl -fsSL -o "%TEMP%\get-platformio.py" https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
        !PYTHON_CMD! "%TEMP%\get-platformio.py"
        del /q "%TEMP%\get-platformio.py" 2>nul
        if exist "%PIO_PENV%" set "PATH=%PIO_PENV%;%PATH%"
    ) else (
        echo   WARNING: Python not found. PlatformIO requires Python 3.
        echo   Install Python 3 from https://www.python.org/downloads/ or via: winget install Python.Python.3.12
        echo   Then re-run this script.
        echo   Skipping PlatformIO. fc_tool will still work for serial monitoring.
    )
)

where pio >nul 2>&1
if %errorlevel% equ 0 (
    for /f "delims=" %%v in ('pio --version') do echo   pio: %%v
    echo   PlatformIO will auto-download board toolchains on first firmware build.
) else (
    echo   PlatformIO not installed. Compile/flash features will be unavailable.
    echo   Serial monitoring and IMU visualization will still work.
)

REM ── 5. npm dependencies ───────────────────────────────────────

echo [5/5] Installing npm dependencies...
pushd "%PROJECT_DIR%"
call npm install
popd

REM ── Done ──────────────────────────────────────────────────────

echo.
echo === Setup complete ===
echo.
echo Install locations:
echo   Rust:        %USERPROFILE%\.cargo\ and %USERPROFILE%\.rustup\
echo   Node.js:     (winget default)
echo   PlatformIO:  %USERPROFILE%\.platformio\
echo   npm:         %PROJECT_DIR%\node_modules\
echo.
echo To build and run fc_tool:
echo   cd %PROJECT_DIR%
echo   dev_setup\windows\build.bat           -- release binary
echo   dev_setup\windows\build.bat dev       -- development mode
echo.
echo The release binary will be in src-tauri\target\release\bundle\

endlocal
