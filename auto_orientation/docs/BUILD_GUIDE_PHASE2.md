# Build Guide: Phase 2 - GPS Support

**Status**: Phase 2 Build Documentation  
**Last Updated**: 2026-05-07  
**Target Board**: Arduino Mega (ATmega2560)  
**Related Docs**: [GPS Hardware Setup](GPS_HARDWARE_SETUP.md), [PHASE_2_MASTER_IMPLEMENTATION_PLAN.md](PHASE_2_MASTER_IMPLEMENTATION_PLAN.md)

## Table of Contents

1. [Quick Start](#quick-start)
2. [Build Environments](#build-environments)
3. [Detailed Build Instructions](#detailed-build-instructions)
4. [Configuration Options](#configuration-options)
5. [Compilation Verification](#compilation-verification)
6. [Troubleshooting Builds](#troubleshooting-builds)

---

## Quick Start

For the impatient (TL;DR):

```bash
# Build and upload GPS-only firmware (default 9600 baud)
cd /home/devel/floppi/auto_orientation
platformio run -e arduino_mega_gps -t upload

# Or high-speed variant (115200 baud for M9N/M10S)
platformio run -e arduino_mega_gps_115200 -t upload

# Or fully-featured (everything: calibration + GPS + snapshots)
platformio run -e arduino_mega_full -t upload

# Monitor output
platformio device monitor -b 115200
```

---

## Build Environments

Phase 2 adds three new build environments to `platformio.ini`:

### Environment: `arduino_mega_gps`
**Purpose**: Production mode with GPS support at standard baud rate

```ini
[env:arduino_mega_gps]
extends = env:arduino_mega
build_flags = -D GPS_ENABLE -D GPS_UART_PORT=0 -D GPS_BAUD=9600
description = Production mode with GPS support at 9600 baud
```

**Features**:
- GPS enabled
- Uses Serial1 (pins 18/19) for GPS UART
- Baud rate: 9600 (standard for most GPS modules)
- Minimal calibration/debug output

**Best for**:
- Standard NEO-M8/M9N GPS modules
- Field deployment
- Minimal serial output (fast, low memory)

**Build Command**:
```bash
platformio run -e arduino_mega_gps -t upload
```

### Environment: `arduino_mega_gps_115200`
**Purpose**: Production GPS at high baud rate

```ini
[env:arduino_mega_gps_115200]
extends = env:arduino_mega
build_flags = -D GPS_ENABLE -D GPS_UART_PORT=0 -D GPS_BAUD=115200
description = Production mode with GPS support at 115200 baud (for M9N/M10S variants)
```

**Features**:
- GPS enabled at 115200 baud
- Higher data rate = faster NMEA updates possible
- Reduced latency for real-time applications

**Best for**:
- NEO-M9N/M10S modules configured at 115200 baud
- Applications needing fast GPS update rate (10 Hz)
- Testing with higher-end GPS modules

**Build Command**:
```bash
platformio run -e arduino_mega_gps_115200 -t upload
```

**Important**: GPS module must be configured for 115200 baud using u-Center tool

### Environment: `arduino_mega_full`
**Purpose**: Complete debug/development build with all features

```ini
[env:arduino_mega_full]
extends = env:arduino_mega
build_flags = -D CALIBRATION_MODE -D GPS_ENABLE -D SNAPSHOT_MODE -D GPS_UART_PORT=0 -D GPS_BAUD=9600
description = Full debug mode with all features: calibration + GPS + snapshot recording
lib_deps =
    Adafruit Unified Sensor
    Adafruit BusIO
    SD
```

**Features**:
- Calibration mode (verbose output)
- GPS enabled
- Snapshot recording to SD card
- Maximum debug information

**Best for**:
- Development and testing
- Debugging issues
- Verifying calibration while using GPS
- Integration testing with all sensors

**Build Command**:
```bash
platformio run -e arduino_mega_full -t upload
```

### Existing Environments (Phase 1)

These remain available for reference:

| Environment | Features | Use Case |
|-------------|----------|----------|
| `arduino_mega` | Base environment only | Template for new environments |
| `arduino_mega_calibration` | Calibration mode | BNO085 calibration (no GPS) |
| `arduino_mega_production` | Production minimal output | Deployment without GPS |
| `arduino_mega_snapshot` | Calibration + snapshots | Sensor data recording (no GPS) |
| `arduino_mega_debug` | Debug mode | General debugging |

---

## Detailed Build Instructions

### Prerequisites

1. **PlatformIO Installed**
   ```bash
   # Install if not already present
   python -m pip install platformio
   ```

2. **Arduino Mega Connected**
   - Connect via USB cable
   - Should appear as `/dev/ttyACM0` (Linux) or `COM3` (Windows)

3. **GPS Module Connected** (optional for initial testing)
   - Can build and test without GPS hardware first
   - See [GPS Hardware Setup](GPS_HARDWARE_SETUP.md) for wiring

### Step 1: Navigate to Project

```bash
cd /home/devel/floppi/auto_orientation
```

### Step 2: Verify Configuration Files

Ensure these files exist and are properly configured:

```bash
# Check build configuration
ls -la platformio.ini
cat platformio.ini | grep -A 3 "env:arduino_mega_gps"

# Check GPS config header
ls -la src/config/gps_config.h
head -30 src/config/gps_config.h

# Check pin definitions
ls -la src/config/pins.h
grep -A 10 "GPS UART" src/config/pins.h
```

All three files should exist and be readable.

### Step 3: Clean Previous Build (Optional but Recommended)

```bash
# Remove old build artifacts
platformio run -e arduino_mega_gps --target clean

# Or remove entire .pio directory
rm -rf .pio
```

### Step 4: Build Firmware

```bash
# For standard 9600 baud:
platformio run -e arduino_mega_gps

# Output should end with:
# ============ [SUCCESS] Took X.XX seconds ============
```

**What the build does:**
1. Compiles all .cpp source files in `src/`
2. Links against configured libraries (Adafruit, etc.)
3. Applies build flags (`-D GPS_ENABLE`, etc.)
4. Generates `.elf` file and `.hex` hex image
5. Stores in `.pio/build/arduino_mega_gps/`

### Step 5: Upload to Board

```bash
# Upload immediately after building:
platformio run -e arduino_mega_gps -t upload

# Or combine build + upload:
platformio run -e arduino_mega_gps -t upload

# Arduino IDE will:
# 1. Hold RESET pin low to bootloader mode
# 2. Upload hex file at 115200 baud
# 3. Release RESET
# 4. Device boots with new firmware
```

**Expected output:**
```
Uploading .pio/build/arduino_mega_gps/firmware.hex
avrdude: AVR device initialized and ready to accept instructions
avrdude: Device signature = 0x1e9801 (probably m2560)
avrdude: reading input file ".pio/build/arduino_mega_gps/firmware.hex"
avrdude: writing flash (XXXXX bytes)
[===== ] 50%
[========== ] 100%
avrdude: done.  Thank you.

======================== [SUCCESS] ========================
```

### Step 6: Monitor Serial Output

```bash
# Open serial monitor at 115200 baud
platformio device monitor -b 115200

# You should see boot message:
# Auto Orientation System [CALIBRATION MODE]  (or PRODUCTION)
# Initializing sensors...
```

### Step 7: Verify GPS Data

Within 30-60 seconds (cold start), you should see GPS data:

```json
{
  "timestamp": 1234567890,
  "position": {
    "gps": {
      "satellites": 8,
      "hdop": 1.20,
      "latitude": 48.13745,
      "longitude": 11.58550,
      "altitude_m": 520.3,
      "locked": true
    }
  }
}
```

If no GPS data after 60 seconds:
1. Check antenna is outdoors with sky view
2. Verify wiring (see [GPS Hardware Setup](GPS_HARDWARE_SETUP.md))
3. Check baud rate matches GPS module
4. See [GPS Troubleshooting](GPS_TROUBLESHOOTING.md) for full diagnostics

---

## Configuration Options

### Build-Time Configuration

All GPS configuration happens at compile time via build flags:

### Flag: GPS_ENABLE

**Purpose**: Master switch for GPS support

**Default**: Disabled (GPS code not compiled)

**Usage**:
```bash
# Enable GPS (all environments do this):
platformio run -e arduino_mega_gps

# In platformio.ini:
# build_flags = -D GPS_ENABLE
```

**Effect**:
- When `GPS_ENABLE` defined: GPS code compiled
- When not defined: GPS code compiled to stubs (zero overhead)

### Flag: GPS_UART_PORT

**Purpose**: Select which hardware UART for GPS

**Values**:
- `0` = Serial1 (RX1=pin 19, TX1=pin 18) - **Recommended**
- `1` = Serial2 (RX2=pin 17, TX2=pin 16)
- `2` = Serial3 (RX3=pin 15, TX3=pin 14)

**Default**: 0 (Serial1)

**Usage**:
```bash
# Use Serial1 (default):
platformio run -e arduino_mega_gps

# Use Serial2 (custom platformio.ini):
# [env:arduino_mega_gps_serial2]
# build_flags = -D GPS_ENABLE -D GPS_UART_PORT=1 -D GPS_BAUD=9600
# platformio run -e arduino_mega_gps_serial2
```

**Selecting a UART**:
- **Serial1 (UART0)**: Already connected to USB (for debugging)
- **Serial1 (UART1)**: Recommended for GPS (pins 18/19)
- **Serial2 (UART2)**: Alternative (pins 16/17)
- **Serial3 (UART3)**: Alternative (pins 14/15)

### Flag: GPS_BAUD

**Purpose**: Configure GPS UART baud rate

**Values**:
- `9600` - Standard (default, most modules)
- `115200` - High-speed (NEO-M9N/M10S)
- Other: 4800, 19200, 38400 (less common)

**Default**: 9600

**Usage**:
```bash
# Standard 9600 baud:
platformio run -e arduino_mega_gps

# High-speed 115200 baud:
platformio run -e arduino_mega_gps_115200

# Custom (create in platformio.ini):
# [env:arduino_mega_gps_19200]
# extends = env:arduino_mega
# build_flags = -D GPS_ENABLE -D GPS_UART_PORT=0 -D GPS_BAUD=19200
```

**Important**: Firmware baud rate must match GPS module configuration
- Mismatch → garbage data on serial or no data at all

### Creating Custom Build Environments

To create your own combination:

```ini
# Add to platformio.ini:

[env:arduino_mega_gps_custom]
extends = env:arduino_mega
build_flags = 
    -D GPS_ENABLE 
    -D GPS_UART_PORT=1         # Serial2
    -D GPS_BAUD=115200
description = GPS at 115200 baud on Serial2

# Build with:
platformio run -e arduino_mega_gps_custom -t upload
```

---

## Compilation Verification

### Verify Build Success

```bash
# After running: platformio run -e arduino_mega_gps

# Check for success message
platformio run -e arduino_mega_gps 2>&1 | grep -i success

# Output should show:
# ============ [SUCCESS] Took X.XX seconds ============
```

### Check Compiler Flags

```bash
# See actual compiler invocation
platformio run -e arduino_mega_gps -v

# Look for GPS-related flags in output:
# -D GPS_ENABLE
# -D GPS_UART_PORT=0
# -D GPS_BAUD=9600
```

### Verify Firmware Size

```bash
# After successful build, check size:
ls -lh .pio/build/arduino_mega_gps/firmware.elf
ls -lh .pio/build/arduino_mega_gps/firmware.hex

# Typical sizes:
# With GPS: ~40-50 KB
# Without GPS: ~20-30 KB

# Arduino Mega flash: 256 KB total
# Used: ~16% with all features
```

### Compare Different Builds

```bash
# Compare GPS enabled vs disabled:
platformio run -e arduino_mega_gps -v   # With GPS
ls -lh .pio/build/arduino_mega_gps/firmware.elf

platformio run -e arduino_mega -v       # Without GPS
ls -lh .pio/build/arduino_mega/firmware.elf

# GPS code adds ~10-15 KB to firmware size
```

---

## Troubleshooting Builds

### Issue: "Unknown environment arduino_mega_gps"

**Symptom**:
```
ERROR: Unknown environment 'arduino_mega_gps'
```

**Solution**:
```bash
# Ensure platformio.ini is in current directory
ls -la platformio.ini

# Verify environment defined
grep "env:arduino_mega_gps" platformio.ini

# Make sure you're in the right directory
pwd  # Should show .../auto_orientation
```

### Issue: "GPS_ENABLE not defined"

**Symptom**: Firmware builds but GPS code won't compile

**Causes**:
- Build flag not set in platformio.ini
- Typo in build_flags line
- Wrong environment selected

**Solution**:
```bash
# Verify build_flags in platformio.ini
cat platformio.ini | grep -A 2 "env:arduino_mega_gps"

# Should show:
# [env:arduino_mega_gps]
# extends = env:arduino_mega
# build_flags = -D GPS_ENABLE -D GPS_UART_PORT=0 -D GPS_BAUD=9600

# Rebuild with verbose output
platformio run -e arduino_mega_gps -v 2>&1 | grep "GPS_ENABLE"
```

### Issue: "cannot find -lm"

**Symptom**: Linker error mentioning missing math library

**Cause**: Rare, but can happen with some PlatformIO versions

**Solution**:
```bash
# Update PlatformIO
python -m pip install --upgrade platformio

# Clean and rebuild
platformio run -e arduino_mega_gps --target clean
platformio run -e arduino_mega_gps
```

### Issue: Upload fails ("avrdude: stk500_recv()")

**Symptom**:
```
avrdude: stk500_recv(): programmer is not responding
```

**Causes**:
- Arduino not connected or wrong port
- Driver not installed
- Bootloader issue

**Solution**:
```bash
# Find Arduino port
ls /dev/ttyACM*  # Linux
ls /dev/cu.usbmodem*  # macOS
# Check Device Manager  # Windows

# Verify port in platformio.ini or upload_port
grep "upload_port" platformio.ini

# Rebuild and explicitly specify port
platformio run -e arduino_mega_gps -t upload --upload-port /dev/ttyACM0
```

### Issue: "Compilation error: no matching function"

**Symptom**: Compiler can't find GPS class methods

**Cause**: Missing include or GPS header not found

**Solution**:
```bash
# Verify src/sensors/gps.h exists
ls -la src/sensors/gps.h

# Verify includes in source code
grep "#include.*gps.h" src/*.cpp

# Rebuild clean
rm -rf .pio
platformio run -e arduino_mega_gps
```

### Issue: Out of Memory (upload succeeds but device resets)

**Symptom**: Device boots but crashes with watchdog reset

**Causes**:
- Firmware too large
- Not enough RAM for buffers
- Stack overflow

**Solution**:
```bash
# Check firmware size
ls -lh .pio/build/arduino_mega_gps/firmware.elf

# If > 200KB: too large
# Disable non-essential features in platformio.ini
# Remove: SNAPSHOT_MODE, CALIBRATION_MODE

# Or use different environment
platformio run -e arduino_mega_gps  # Minimal GPS
```

---

## Advanced: Custom Build Commands

### Build Without Upload

```bash
# Compile only, no upload
platformio run -e arduino_mega_gps

# Hex file location:
cat .pio/build/arduino_mega_gps/firmware.hex | head -1
```

### Build with Specific Library Versions

```bash
# In platformio.ini, specify exact library versions:
[env:arduino_mega_gps]
extends = env:arduino_mega
lib_deps =
    Adafruit Unified Sensor @ ^1.1.14
    Adafruit BusIO @ ^1.14.1
```

### Build with Extra Compiler Flags

```bash
# In platformio.ini:
[env:arduino_mega_gps]
build_flags = 
    -D GPS_ENABLE
    -D GPS_UART_PORT=0
    -D GPS_BAUD=9600
    -O2                # Optimization level 2
    -Wall              # Warn on all issues
```

### Monitor Build Progress

```bash
# Show verbose build output
platformio run -e arduino_mega_gps -v

# Filter for errors only
platformio run -e arduino_mega_gps 2>&1 | grep -i error

# Count warnings
platformio run -e arduino_mega_gps 2>&1 | grep -i warning | wc -l
```

---

## Testing After Build

### Verify Device Boots

```bash
platformio device monitor -b 115200

# Look for boot messages:
# Auto Orientation System
# Initializing sensors...
# ✓ BNO085 OK
```

### Test GPS Functionality

```bash
# Monitor for GPS data (wait up to 60 seconds)
timeout 120 platformio device monitor -b 115200 | grep "satellites"

# Should see satellite count increasing:
# "satellites": 0
# "satellites": 2
# "satellites": 6
# "satellites": 10
# "locked": true
```

### Test All Features (arduino_mega_full)

```bash
platformio run -e arduino_mega_full -t upload

# Monitor output
platformio device monitor -b 115200

# Should see:
# 1. BNO085 orientation data
# 2. GPS position data
# 3. Calibration information
# 4. Snapshot recording messages
```

---

## Build Performance Tips

### Speed Up Builds

```bash
# Parallel compilation (if supported)
platformio run -e arduino_mega_gps -j 4

# Use ccache (if available)
export CCACHE_BASEDIR=/path/to/project
platformio run -e arduino_mega_gps
```

### Monitor Memory Usage

```bash
# Show RAM/Flash usage
platformio run -e arduino_mega_gps --target size

# Output shows:
# Program:  45678 bytes (17.6% of 256 KB Flash)
# Data:      2048 bytes (25% of 8 KB RAM)
```

### Verify No Compiler Warnings

```bash
# Build with warnings-as-errors
platformio run -e arduino_mega_gps -v 2>&1 | grep -i warning

# No warnings = clean code
```

---

## Related Documentation

- [GPS Hardware Setup](GPS_HARDWARE_SETUP.md) - Physical wiring and setup
- [GPS Driver API Reference](GPS_DRIVER_API_REFERENCE.md) - Using GPS in code
- [GPS Troubleshooting](GPS_TROUBLESHOOTING.md) - Runtime issues
- [Coordinate Frame API Reference](COORDINATE_FRAME_API_REFERENCE.md) - GPS coordinate system
- [PHASE_2_MASTER_IMPLEMENTATION_PLAN.md](PHASE_2_MASTER_IMPLEMENTATION_PLAN.md) - Full implementation details

---

**Last Updated**: 2026-05-07  
**Version**: 1.0  
**Author**: Phase 2 Implementation  
**Status**: Complete and tested
