@echo off
REM fc_tool build script for Windows
REM
REM Prerequisites:
REM   1. Run install-system-deps.bat as Administrator (once)
REM   2. Run setup-dev.bat (once)
REM
REM Usage:
REM   build.bat          -- build release binary
REM   build.bat dev      -- run in development mode (live reload)

setlocal enabledelayedexpansion

REM Determine project directory (two levels up from this script)
set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%\..\.."
set "PROJECT_DIR=%CD%"
popd

REM Source Rust if needed
where cargo >nul 2>&1
if %errorlevel% neq 0 (
    if exist "%USERPROFILE%\.cargo\bin" (
        set "PATH=%USERPROFILE%\.cargo\bin;%PATH%"
    ) else (
        echo Error: cargo not found. Run setup-dev.bat first.
        exit /b 1
    )
)

REM Source PlatformIO if available (optional)
if exist "%USERPROFILE%\.platformio\penv\Scripts" (
    set "PATH=%USERPROFILE%\.platformio\penv\Scripts;%PATH%"
)

REM Verify toolchains
for %%c in (cargo node npm) do (
    where %%c >nul 2>&1
    if !errorlevel! neq 0 (
        echo Error: %%c not found. Run setup-dev.bat first.
        exit /b 1
    )
)

REM Parse mode argument
set "MODE=%~1"
if "%MODE%"=="" set "MODE=build"

pushd "%PROJECT_DIR%"

if /i "%MODE%"=="dev" (
    echo === Starting fc_tool in development mode ===
    call npm run tauri dev
) else if /i "%MODE%"=="build" (
    echo === Building fc_tool release binary ===
    call npm run tauri build

    echo.
    echo === Build artifacts ===
    set "BUNDLE_DIR=%PROJECT_DIR%\src-tauri\target\release\bundle"
    if exist "!BUNDLE_DIR!" (
        for /r "!BUNDLE_DIR!" %%f in (*.msi *.exe) do (
            echo   %%~nxf
        )
    )
) else (
    echo Usage: build.bat [dev^|build]
    echo   dev    Run in development mode with live reload
    echo   build  Compile release binary (default)
    popd
    exit /b 1
)

popd
endlocal
