@echo off
REM Flight Controller Build Script
REM Usage: build.bat [build|upload|clean]

set ENV_T40=teensy40
set ENV_T41=teensy41
set ENV_ESP32=esp32

if "%1"=="clean" goto clean
if "%1"=="upload" goto upload
if "%1"=="build" goto build
if "%1"=="" goto menu
echo Unknown option: %1
goto menu

:menu
echo.
echo   Flight Controller Build Tool
echo   ----------------------------
echo   b - Build all environments
echo   u - Upload to device
echo   c - Clean build artifacts
echo.
set /p choice="Select [b/u/c]: "
if /i "%choice%"=="b" goto build
if /i "%choice%"=="u" goto upload
if /i "%choice%"=="c" goto clean
echo Invalid option.
goto menu

:build
echo Building...
pio run
goto end

:upload
echo.
echo   1 - Teensy 4.0
echo   2 - Teensy 4.1
echo   3 - ESP32
echo   4 - ESP32 (calibration)
echo.
set /p device="Upload to [1/2/3/4]: "
if "%device%"=="1" pio run -t upload -e %ENV_T40%
if "%device%"=="2" pio run -t upload -e %ENV_T41%
if "%device%"=="3" pio run -t upload -e %ENV_ESP32%
if "%device%"=="4" pio run -t upload -e esp32_calibration
goto end

:clean
echo Cleaning build artifacts...
if exist .pio rmdir /s /q .pio
echo Done.
goto end

:end
