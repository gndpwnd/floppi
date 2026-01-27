# TODO: UNTESTED — written on Linux, needs validation on a real Windows machine.
# fc_tool system dependencies for Windows
# Run this ONCE in an elevated (Administrator) PowerShell.
#
# Usage:
#   Right-click PowerShell > Run as Administrator
#   .\install-system-deps.ps1

$ErrorActionPreference = "Stop"

# Check for admin privileges
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "This script must be run as Administrator." -ForegroundColor Red
    Write-Host "Right-click PowerShell > Run as Administrator, then re-run this script."
    exit 1
}

Write-Host "=== Installing fc_tool system dependencies (Windows) ===" -ForegroundColor Cyan

# Check if winget is available (comes with Windows 10 1709+ and Windows 11)
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host "winget not found. Please install App Installer from the Microsoft Store." -ForegroundColor Red
    Write-Host "https://apps.microsoft.com/detail/9NBLGGH4NNS1"
    exit 1
}

# Visual Studio Build Tools (provides MSVC compiler, linker, Windows SDK)
Write-Host ""
Write-Host "[1/2] Checking Visual Studio Build Tools..." -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsInstalled = & $vsWhere -latest -property installationPath 2>$null
    if ($vsInstalled) {
        Write-Host "  Visual Studio Build Tools already installed: $vsInstalled"
    }
} else {
    Write-Host "  Installing Visual Studio Build Tools..."
    Write-Host "  This will open the Visual Studio Installer."
    Write-Host "  Select 'Desktop development with C++' workload and install."
    winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
}

# WebView2 Runtime (usually pre-installed on Windows 10/11, but just in case)
Write-Host ""
Write-Host "[2/2] Checking WebView2 Runtime..." -ForegroundColor Yellow
$webview2 = Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" -ErrorAction SilentlyContinue
if ($webview2) {
    Write-Host "  WebView2 Runtime already installed: $($webview2.pv)"
} else {
    Write-Host "  Installing WebView2 Runtime..."
    winget install Microsoft.EdgeWebView2Runtime
}

Write-Host ""
Write-Host "=== System dependencies installed ===" -ForegroundColor Green
Write-Host "Now run .\setup-dev.ps1 (no admin required)"
