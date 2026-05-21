@echo off
setlocal enabledelayedexpansion

REM Flight Controller Build Script (Windows)
REM Dynamically reads environments from platformio.ini
REM
REM Usage:
REM   build.bat                     Interactive menu
REM   build.bat build [env]         Build (all or specific env)
REM   build.bat upload [env]        Upload to specific env
REM   build.bat clean [env]         Clean (all or specific env)
REM   build.bat monitor             Serial monitor
REM   build.bat list                List available environments
REM   build.bat matrix              Run build coverage matrix
REM   build.bat help                Show usage

set "PLATFORMIO_INI=platformio.ini"

REM Parse environments from platformio.ini
call :parse_environments

REM Handle command line arguments
if "%1"=="" goto menu
if /i "%1"=="build" goto cmd_build
if /i "%1"=="upload" goto cmd_upload
if /i "%1"=="clean" goto cmd_clean
if /i "%1"=="monitor" goto cmd_monitor
if /i "%1"=="serial" goto cmd_monitor
if /i "%1"=="matrix" goto cmd_matrix
if /i "%1"=="list" goto cmd_list
if /i "%1"=="envs" goto cmd_list
if /i "%1"=="help" goto cmd_help
if /i "%1"=="--help" goto cmd_help
if /i "%1"=="-h" goto cmd_help
echo Unknown command: %1
echo Use 'build.bat help' for usage information.
goto done

REM ======================================================================
REM INTERACTIVE MENU
REM ======================================================================

:menu
cls
echo.
echo   Flight Controller Build Tool
echo   ============================
echo.
echo   1) Select environment and build
echo   2) Select environment and upload
echo   3) Build all environments
echo   4) Clean all environments
echo   5) Serial monitor
echo   6) List environments
echo   7) Run build coverage matrix (ESP32/S3, USE_BAROMETER+USE_GPS)
echo   0) Exit
echo.
set /p "choice=Select option: "

if "%choice%"=="1" (
    call :select_env
    if defined SELECTED_ENV (
        echo.
        echo Building !SELECTED_ENV!...
        echo --------------------------------------------------------
        pio run -e !SELECTED_ENV!
        if !errorlevel! equ 0 (
            echo.
            echo [OK] Build succeeded.
        ) else (
            echo.
            echo [FAIL] Build failed.
        )
    )
    echo.
    pause
    goto menu
)
if "%choice%"=="2" (
    call :select_env
    if defined SELECTED_ENV (
        echo.
        echo Uploading to !SELECTED_ENV!...
        echo Ensure your board is connected!
        echo --------------------------------------------------------
        pio run -t upload -e !SELECTED_ENV!
        if !errorlevel! equ 0 (
            echo.
            echo [OK] Upload succeeded.
        ) else (
            echo.
            echo [FAIL] Upload failed.
        )
    )
    echo.
    pause
    goto menu
)
if "%choice%"=="3" (
    echo.
    echo Building all environments...
    echo --------------------------------------------------------
    pio run
    echo.
    pause
    goto menu
)
if "%choice%"=="4" (
    echo.
    echo Cleaning all environments...
    echo --------------------------------------------------------
    pio run -t clean
    echo.
    pause
    goto menu
)
if "%choice%"=="5" goto cmd_monitor_return
if "%choice%"=="6" (
    call :list_envs
    echo.
    pause
    goto menu
)
if "%choice%"=="7" (
    call :run_matrix
    echo.
    pause
    goto menu
)
if "%choice%"=="0" goto done
echo Invalid option.
timeout /t 1 >nul
goto menu

:cmd_monitor_return
pio device monitor
goto menu

REM ======================================================================
REM COMMAND LINE HANDLERS
REM ======================================================================

:cmd_build
if "%2"=="" (
    echo Building all environments...
    pio run
) else (
    call :validate_env %2
    if defined VALID_ENV (
        echo Building %2...
        pio run -e %2
    ) else (
        echo Unknown environment: %2
        call :list_envs
    )
)
goto done

:cmd_upload
if "%2"=="" (
    echo Upload requires an environment name.
    echo Usage: build.bat upload ENV_NAME
    echo.
    call :list_envs
) else (
    call :validate_env %2
    if defined VALID_ENV (
        echo Uploading to %2...
        echo Ensure your board is connected!
        pio run -t upload -e %2
    ) else (
        echo Unknown environment: %2
        call :list_envs
    )
)
goto done

:cmd_clean
if "%2"=="" (
    echo Cleaning all environments...
    pio run -t clean
) else (
    call :validate_env %2
    if defined VALID_ENV (
        echo Cleaning %2...
        pio run -t clean -e %2
    ) else (
        echo Unknown environment: %2
        call :list_envs
    )
)
goto done

:cmd_monitor
pio device monitor
goto done

:cmd_matrix
call :run_matrix
goto done

:cmd_list
call :list_envs
goto done

:cmd_help
echo.
echo   Flight Controller Build Tool
echo   ============================
echo.
echo   Usage:
echo     build.bat                     Interactive menu
echo     build.bat build [env]         Build (all or specific environment)
echo     build.bat upload env          Upload to environment
echo     build.bat clean [env]         Clean (all or specific environment)
echo     build.bat monitor             Start serial monitor
echo     build.bat list                List available environments
echo     build.bat matrix              Run build coverage matrix
echo.
echo   Examples:
echo     build.bat build teensy40      Build for Teensy 4.0
echo     build.bat upload esp32        Upload to ESP32
echo     build.bat clean               Clean all environments
echo.
goto done

REM ======================================================================
REM ENVIRONMENT PARSING
REM ======================================================================

:parse_environments
REM Parse [env:xxx] sections from platformio.ini into ENV_n variables
set "ENV_COUNT=0"
if not exist "%PLATFORMIO_INI%" (
    echo ERROR: platformio.ini not found in current directory.
    exit /b 1
)
for /f "usebackq tokens=*" %%a in ("%PLATFORMIO_INI%") do (
    set "line=%%a"
    REM Check for [env:xxx] pattern
    echo %%a | findstr /r /c:"^\[env:[a-zA-Z0-9_-]*\]$" >nul 2>&1
    if !errorlevel! equ 0 (
        REM Extract environment name between [env: and ]
        set "envname=%%a"
        set "envname=!envname:[env:=!"
        set "envname=!envname:]=!"
        set "ENV_!ENV_COUNT!=!envname!"
        set /a "ENV_COUNT+=1"
    )
)
goto :eof

:select_env
REM Display environments and let user select one
set "SELECTED_ENV="
echo.
call :list_envs
echo.
set /p "envsel=Select environment (0-%ENV_LAST%): "

REM Validate selection is a number in range
set "valid_num="
for /l %%i in (0,1,%ENV_LAST%) do (
    if "%envsel%"=="%%i" set "valid_num=1"
)
if not defined valid_num (
    echo Invalid selection.
    goto :eof
)
set "SELECTED_ENV=!ENV_%envsel%!"
goto :eof

:list_envs
REM Display all available environments
echo.
echo   Available Environments:
echo   -----------------------
set /a "ENV_LAST=%ENV_COUNT%-1"
for /l %%i in (0,1,%ENV_LAST%) do (
    echo     %%i) !ENV_%%i!
)
goto :eof

:validate_env
REM Check if argument matches a known environment
set "VALID_ENV="
set /a "ENV_LAST=%ENV_COUNT%-1"
for /l %%i in (0,1,%ENV_LAST%) do (
    if /i "%1"=="!ENV_%%i!" set "VALID_ENV=1"
)
goto :eof

:run_matrix
REM Build coverage matrix - exercises flag combinations the per-env builds
REM do not cover. Extra -D flags are injected via PLATFORMIO_BUILD_FLAGS,
REM which PlatformIO appends to the environment's build_flags.
echo.
echo   Running build coverage matrix...
echo   --------------------------------------------------------
set "MATRIX_FAILURES=0"

call :matrix_entry esp32 ""
call :matrix_entry esp32s3 ""
call :matrix_entry esp32 "-D USE_BAROMETER -D USE_GPS"
call :matrix_entry esp32s3 "-D USE_BAROMETER -D USE_GPS"

echo.
if "!MATRIX_FAILURES!"=="0" (
    echo [OK] Build matrix completed - all entries succeeded.
) else (
    echo [FAIL] Build matrix completed with !MATRIX_FAILURES! failure(s).
)
goto :eof

:matrix_entry
REM %1 = environment, %2 = extra build flags (quoted)
set "MATRIX_ENV=%1"
set "MATRIX_FLAGS=%~2"
echo.
echo Matrix build: %MATRIX_ENV%  (extra flags: %MATRIX_FLAGS%)
echo --------------------------------------------------------
set "PLATFORMIO_BUILD_FLAGS=%MATRIX_FLAGS%"
pio run -e %MATRIX_ENV%
if !errorlevel! equ 0 (
    echo [OK] Matrix build OK: %MATRIX_ENV%
) else (
    echo [FAIL] Matrix build FAILED: %MATRIX_ENV%
    set /a "MATRIX_FAILURES+=1"
)
set "PLATFORMIO_BUILD_FLAGS="
goto :eof

:done
endlocal
