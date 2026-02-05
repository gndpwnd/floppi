@echo off
REM fc_tool system dependencies for Windows
REM Run this ONCE in an elevated (Administrator) Command Prompt.
REM
REM Usage:
REM   Right-click Command Prompt > Run as Administrator
REM   install-system-deps.bat

setlocal enabledelayedexpansion

REM Check for admin privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo This script must be run as Administrator.
    echo Right-click Command Prompt ^> Run as Administrator, then re-run this script.
    exit /b 1
)

echo === Installing fc_tool system dependencies (Windows) ===

REM Check if winget is available (comes with Windows 10 1709+ and Windows 11)
where winget >nul 2>&1
if %errorlevel% neq 0 (
    echo winget not found. Please install App Installer from the Microsoft Store.
    echo https://apps.microsoft.com/detail/9NBLGGH4NNS1
    exit /b 1
)

REM [1/2] Visual Studio Build Tools (provides MSVC compiler, linker, Windows SDK)
echo.
echo [1/2] Checking Visual Studio Build Tools...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "delims=" %%i in ('"%VSWHERE%" -latest -property installationPath 2^>nul') do (
        echo   Visual Studio Build Tools already installed: %%i
        goto :webview2
    )
)
echo   Installing Visual Studio Build Tools...
echo   This will open the Visual Studio Installer.
echo   Select 'Desktop development with C++' workload and install.
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

:webview2
REM [2/2] WebView2 Runtime (usually pre-installed on Windows 10/11)
echo.
echo [2/2] Checking WebView2 Runtime...
reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" >nul 2>&1
if %errorlevel% equ 0 (
    echo   WebView2 Runtime already installed.
) else (
    echo   Installing WebView2 Runtime...
    winget install Microsoft.EdgeWebView2Runtime
)

echo.
echo === System dependencies installed ===
echo Now run setup-dev.bat (no admin required)

endlocal
