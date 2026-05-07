# Build Guide: Compiling auto_orientation

**Document Version**: 1.0  
**Date**: 2026-05-07  
**Status**: Complete for Phase 1

---

## Prerequisites

### Software Requirements

- **PlatformIO CLI**: Version 6.0 or later
  ```bash
  pip install platformio
  ```

- **Python**: Version 3.7 or later (for PlatformIO)

- **Git**: For cloning the repository

### Hardware Requirements

- **Arduino Mega 2560**: Primary target (16 MHz ATmega2560, 256 KB Flash, 8 KB RAM)
- **BNO085 IMU**: Over I2C
- **Optional: SD Card Shield**: For snapshot recording
- **Optional: USB-to-Serial**: For programming and monitoring

### System Requirements

- **Linux, macOS, or Windows** with a working terminal
- **USB drivers**: Arduino drivers (usually pre-installed)
- **Disk space**: ~100 MB for libraries and build artifacts

---

## Project Setup

### Clone Repository

```bash
cd ~/projects
git clone https://github.com/kaleldev/floppi.git
cd floppi/auto_orientation
```

### Initialize PlatformIO

```bash
platformio project init
```

This creates the `.platformio.ini` configuration file.

### Install Dependencies

Libraries are automatically downloaded during first build. Alternatively, pre-download:

```bash
platformio lib install
```

---

## Build Environments

The project supports multiple build configurations via platformio.ini:

### Available Environments

| Environment | Mode | Snapshot | Use Case |
|-------------|------|----------|----------|
| `arduino_mega` | Production | No | Basic testing |
| `arduino_mega_calibration` | Debug | No | Development |
| `arduino_mega_production` | Minimal | No | Deployment |
| `arduino_mega_snapshot` | Debug | Yes | Field testing with logs |
| `arduino_mega_snapshot_only` | Minimal | Yes | Production with logs |

### Environment Configuration

Each environment extends `arduino_mega` base with specific build flags:

```ini
[env:arduino_mega]
platform = atmelavr
board = megaatmega2560
framework = arduino
lib_deps = 
    Adafruit Unified Sensor
    Adafruit BusIO
    SD  # Added for snapshot modes

[env:arduino_mega_calibration]
extends = env:arduino_mega
build_flags = -D CALIBRATION_MODE
description = Calibration mode with verbose debug output

[env:arduino_mega_snapshot]
extends = env:arduino_mega
build_flags = -D CALIBRATION_MODE -D SNAPSHOT_MODE
description = Calibration mode with snapshot recording to SD card

[env:arduino_mega_snapshot_only]
extends = env:arduino_mega
build_flags = -D SNAPSHOT_MODE
description = Production mode with snapshot recording only
```

---

## Compilation Commands

### Basic Build

Compile default environment (`arduino_mega`):

```bash
platformio run
```

**Output**: Executable in `.pio/build/arduino_mega/firmware.elf`

### Build Specific Environment

```bash
platformio run -e arduino_mega_calibration
platformio run -e arduino_mega_snapshot
platformio run -e arduino_mega_snapshot_only
```

### Clean Build

Remove build artifacts and recompile:

```bash
platformio run -t clean
platformio run
```

### Build All Environments

Test that all configurations compile:

```bash
platformio run -e arduino_mega
platformio run -e arduino_mega_calibration
platformio run -e arduino_mega_snapshot
platformio run -e arduino_mega_snapshot_only
```

---

## Uploading to Hardware

### Prerequisites

1. Connect Arduino Mega to computer via USB
2. Verify port is detected:
   ```bash
   platformio device list
   ```

3. Note the port (e.g., `/dev/ttyUSB0` on Linux, `COM3` on Windows)

### Upload Firmware

**Default environment**:
```bash
platformio run -t upload
```

**Specific environment**:
```bash
platformio run -e arduino_mega_calibration -t upload
```

**With manual port specification**:
```bash
platformio run -e arduino_mega_snapshot --upload-port /dev/ttyUSB0 -t upload
```

### Post-Upload Verification

Monitor serial output to verify successful upload:

```bash
platformio device monitor -b 115200
```

Should see startup messages and sensor data (if BNO085 connected).

---

## Build Flag Configuration

### CALIBRATION_MODE

Enables verbose debug output to serial port.

```cpp
#ifdef CALIBRATION_MODE
  #define IS_CALIBRATION_MODE 1
  CAL_PRINTLN("Debug message");  // Prints if CALIBRATION_MODE enabled
#else
  #define IS_CALIBRATION_MODE 0
  // CAL_PRINTLN macro is no-op
#endif
```

**Effects**:
- Prints sensor readings every 100 ms
- Prints calibration status changes
- Prints error conditions with details
- Increases binary size by ~3 KB

**Binary Size Impact**: +3 KB Flash

### SNAPSHOT_MODE

Enables snapshot recording feature (quaternion + timestamp to SD card in JSON format).

```cpp
#ifdef SNAPSHOT_MODE
  #define ENABLE_SNAPSHOT_RECORDER 1
  #define SNAPSHOT_BUFFER_SIZE 1024
  #define MAX_SNAPSHOT_FILES 100
#else
  #define ENABLE_SNAPSHOT_RECORDER 0
#endif
```

**Effects**:
- Enables SD card initialization
- Enables snapshot recording API
- Requires SD card hardware
- Blocking SD writes (~20-50 ms per write)

**Binary Size Impact**: +2 KB Flash (SD library larger if not already used)

### Combined Flags

| Build Env | CALIBRATION_MODE | SNAPSHOT_MODE | Use Case |
|-----------|------------------|---------------|----------|
| arduino_mega | No | No | Testing, minimal output |
| arduino_mega_calibration | Yes | No | Debug, no logging |
| arduino_mega_production | No | No | Deployment, minimal output |
| arduino_mega_snapshot | Yes | Yes | Field testing with debug + logs |
| arduino_mega_snapshot_only | No | Yes | Production deployment + logs |

### Custom Build Flags

Edit `platformio.ini` to add custom flags:

```ini
[env:custom_debug]
extends = env:arduino_mega
build_flags = 
    -D CALIBRATION_MODE
    -D SNAPSHOT_MODE
    -D DEBUG_SERIAL_SPEED=115200
    -Wall -Wextra -Werror
```

---

## Testing

### Run Unit Tests

PlatformIO can compile and run tests on the target:

```bash
platformio test --environment=arduino_mega
```

This compiles and runs all test files in `tests/` directory.

### Test Files

Located in `tests/`:
- `test_quaternion.cpp` - Quaternion math validation
- `test_coordinates.cpp` - GPS coordinate conversions
- `test_snapshot_recorder.cpp` - SD card and JSON format
- `integration_tests.cpp` - Full system validation

### Compilation Check (Without Upload)

Verify code compiles without uploading:

```bash
platformio run -t compiledb
```

This runs compiler checks but doesn't link or program hardware.

---

## Troubleshooting

### Issue: "Board Not Detected"

**Error**: `No port detected`

**Solution**:
1. Verify USB cable is connected
2. Check Arduino drivers installed:
   ```bash
   platformio device list
   ```
3. On Linux, add user to `dialout` group:
   ```bash
   sudo usermod -a -G dialout $USER
   logout  # Then log back in
   ```

### Issue: "Compilation Errors"

**Error**: `error: undefined reference to 'quaternion_multiply'`

**Solution**:
1. Verify all `.cpp` files are in `src/` directory
2. Check file isn't in `lib/` (which is for external libraries)
3. Rebuild from clean:
   ```bash
   platformio run -t clean
   platformio run
   ```

### Issue: "Out of Memory"

**Error**: `linking failed, section '.text' will not fit`

**Solution**:
1. Disable SNAPSHOT_MODE if not needed
2. Reduce debug output (disable CALIBRATION_MODE)
3. Use Arduino Mega (more memory than Nano)
4. Optimize compiler flags:
   ```ini
   build_flags = -Os  # Optimize for size
   ```

### Issue: "Upload Fails After Compilation"

**Error**: `Upload protocol error` or similar

**Solution**:
1. Try manually resetting Arduino before upload
2. Check baud rate:
   ```ini
   upload_speed = 115200
   ```
3. Verify correct COM port:
   ```bash
   platformio device list
   platformio run --upload-port /dev/ttyUSB0 -t upload
   ```

### Issue: "SD Card Not Recognized"

**Error**: `ERROR: SD card initialization failed`

**Solution**:
1. Verify SD card is inserted (microSD + adapter)
2. Check wiring: MOSI, MISO, SCK, CS pins correct
3. Verify power: 3.3V to SD shield (not 5V)
4. Try different SD card (Class 6 or higher)
5. Format SD card to FAT32

---

## Binary Size Analysis

### Compile with Size Report

```bash
platformio run --verbose
```

Look for "Program Size" output:

```
Program Size: 98765 bytes
```

### Detailed Size Breakdown

Create and run custom script:

```bash
# List sections and sizes
avr-objdump -h .pio/build/arduino_mega/firmware.elf

# Show symbol sizes
avr-nm --print-size --size-sort .pio/build/arduino_mega/firmware.elf | tail -20
```

### Typical Binary Sizes

| Configuration | Flash Used | RAM Used | Free Flash |
|---------------|-----------|----------|-----------|
| arduino_mega | ~85 KB | 4 KB | ~171 KB |
| + CALIBRATION_MODE | ~88 KB | 4 KB | ~168 KB |
| + SNAPSHOT_MODE | ~90 KB | 5 KB | ~166 KB |
| + Both modes | ~93 KB | 5 KB | ~163 KB |

**Note**: All sizes well under 256 KB limit with 160+ KB remaining

---

## Optimization Tips

### For Smaller Binary

```ini
[env:optimized]
extends = env:arduino_mega
build_flags = -Os  # Optimize for size
```

This can reduce binary size by 10-20% at cost of speed.

### For Faster Execution

```ini
[env:fast]
extends = env:arduino_mega
build_flags = -O3  # Optimize for speed
```

This increases binary size but reduces computation time.

### Disable Unused Features

If not using snapshot recording:

```ini
[env:minimal]
extends = env:arduino_mega
lib_deps = 
    Adafruit Unified Sensor
    Adafruit BusIO
    # Remove: SD
```

This removes SD library, saving ~5 KB Flash.

---

## Development Workflow

### Edit → Build → Upload Cycle

1. **Edit source** in `src/` directory
2. **Build**:
   ```bash
   platformio run -e arduino_mega_calibration
   ```
3. **Upload**:
   ```bash
   platformio run -e arduino_mega_calibration -t upload
   ```
4. **Test** with serial monitor:
   ```bash
   platformio device monitor
   ```

### Using VS Code (Recommended)

1. Install PlatformIO extension
2. Open project folder
3. Use VS Code interface:
   - Click "Build" in PlatformIO toolbar
   - Click "Upload" to program Arduino
   - Click "Serial Monitor" to view output

### Using Git for Version Control

```bash
# Check status
git status

# Commit changes
git add src/
git commit -m "Fix quaternion normalization"

# Push to repository
git push origin main
```

---

## Multi-Board Support

### Porting to Other Boards

Supported boards defined in `platformio.ini`:
- Arduino Nano
- Arduino Mega 2560 (primary)
- Teensy 3.1
- ESP32

### Add New Board Environment

```ini
[env:arduino_nano]
platform = atmelavr
board = nanoatmega328
framework = arduino
lib_deps = 
    Adafruit Unified Sensor
    Adafruit BusIO
build_flags = -D CALIBRATION_MODE
```

Then compile with:
```bash
platformio run -e arduino_nano
```

**Note**: Some features may not work on boards with limited RAM (Nano has only 2 KB)

---

## Continuous Integration / CD

### GitHub Actions Example

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build all environments
        run: |
          pip install platformio
          platformio run -e arduino_mega
          platformio run -e arduino_mega_calibration
          platformio run -e arduino_mega_snapshot
          platformio run -e arduino_mega_snapshot_only
```

This automatically compiles all configurations on every push.

---

## Reference Commands Cheat Sheet

```bash
# Build & upload
platformio run -e arduino_mega_snapshot -t upload

# Monitor serial (Ctrl+C to exit)
platformio device monitor -b 115200

# Clean build
platformio run -t clean && platformio run

# Run tests
platformio test --environment=arduino_mega

# Show all environments
platformio run --list-envs

# Check device connection
platformio device list

# Full verbose output
platformio run --verbose

# Analyze code (static analysis)
platformio check
```

---

## Next Steps

1. **Build default configuration**: `platformio run`
2. **Verify hardware connectivity**: Serial monitor should show startup messages
3. **Enable snapshot mode**: Edit `platformio.ini` to use `arduino_mega_snapshot`
4. **Deploy to field**: Use `arduino_mega_snapshot_only` for production

---

## Additional Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [Arduino Mega Datasheet](https://store.arduino.cc/products/arduino-mega-2560-rev3)
- [Adafruit BNO085 Library](https://github.com/adafruit/Adafruit_BNO08x)
- See also: `QUICK_START.md` for getting started
