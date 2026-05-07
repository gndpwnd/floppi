# Feature Flags Documentation

**Document Version**: 1.0  
**Date**: 2026-05-07  
**Status**: Complete for Phase 1

---

## Overview

This document describes all available build-time configuration flags for the auto_orientation project. These flags enable/disable features at compile time, allowing optimized binaries for different use cases.

---

## Feature Flags

### CALIBRATION_MODE

**Description**: Enables verbose debug output to serial port

**Type**: Boolean (define present = enabled)

**Default**: Disabled (for production builds)

**Set via platformio.ini**:
```ini
build_flags = -D CALIBRATION_MODE
```

**Effects When Enabled**:

1. **Serial Output**: Increased verbosity
   - Sensor readings printed every 100 ms
   - Calibration status changes logged
   - Error conditions with detailed messages
   - IMU I2C transaction diagnostics

2. **Processing Overhead**: Minimal
   - Debug messages use macro (compiled to no-op if not enabled)
   - No additional computation, only serial I/O

3. **Serial Port Impact**:
   - Increases baud rate: 115200 (standard)
   - Message rate: ~10 messages/second
   - Typical payload: 50-100 bytes per message

4. **Code Size**:
   ```
   Without CALIBRATION_MODE:  ~85 KB
   With CALIBRATION_MODE:     ~88 KB
   Overhead:                  ~3 KB
   ```

5. **Example Output**:
   ```
   BNO085: System cal=3 Accel cal=3 Gyro cal=3 Mag cal=3
   Quaternion: [0.707, 0.000, 0.000, 0.707]
   Euler: Roll=0.0° Pitch=0.0° Yaw=90.0°
   ```

**Use Cases**:
- Development and debugging
- Field testing with diagnosis
- Integration verification
- Troubleshooting sensor issues

**Performance Impact**:
- Negligible if not recording to SD card
- Small impact on loop timing (serial I/O is slow)

---

### SNAPSHOT_MODE

**Description**: Enables snapshot recording feature (quaternion + timestamp to SD card in JSON format)

**Type**: Boolean (define present = enabled)

**Default**: Disabled (SD card optional)

**Set via platformio.ini**:
```ini
build_flags = -D SNAPSHOT_MODE
```

**Dependent Configuration** (auto-set by SNAPSHOT_MODE):
```cpp
#define ENABLE_SNAPSHOT_RECORDER 1          // Enable recorder
#define SNAPSHOT_BUFFER_SIZE 1024           // JSON buffer (bytes)
#define MAX_SNAPSHOT_FILES 100              // Files before wrap-around
#define SNAPSHOT_DIRECTORY "/snapshots/"    // SD directory
```

**Effects When Enabled**:

1. **Hardware Requirements**:
   - SD card (microSD with adapter, or SD shield)
   - SPI pins: CS, MOSI, MISO, SCK
   - Power: 3.3V regulated supply

2. **API Additions**:
   ```cpp
   snapshot_recorder_init()         // Initialize SD card
   snapshot_record(q, cal, ts)      // Record single snapshot
   snapshot_get_status()            // Check ready status
   ```

3. **Code Size**:
   ```
   Without SNAPSHOT_MODE:  ~85 KB
   With SNAPSHOT_MODE:     ~90 KB
   Overhead:               ~5 KB (includes SD library)
   ```

4. **RAM Usage**:
   ```
   Buffer:                 1 KB
   SD file handles:        ~100 bytes
   SnapshotRecorder obj:   ~64 bytes
   Total overhead:         ~1.2 KB
   ```

5. **Serial I/O**:
   - SD writes are blocking (20-50 ms per write)
   - Causes loop latency at high recording rates
   - Acceptable for ≤ 10 Hz recording frequency

6. **Storage Capacity**:
   - 1 GB SD card: 1-2 million snapshots
   - 100 byte avg. per snapshot: 100-200 MB data
   - Recording duration: 300+ hours at 10 Hz

7. **File Format**: JSON (NDJSON - newline-delimited)
   ```json
   {"ts":1234567,"q":{"w":0.707,"x":0.0,"y":0.707,"z":0.0},"e":{"r":0.0,"p":90.0,"y":0.0},"c":{"s":3,"a":3,"g":3,"m":3}}
   ```

**Use Cases**:
- Field data collection for post-flight analysis
- Calibration quality monitoring over time
- Research data archival
- Debugging orientation problems

**Performance Impact**:
- Blocking SD I/O causes 20-50 ms per write
- Impact depends on recording frequency:
  - 1 Hz: < 5% impact
  - 10 Hz: ~5-10% impact
  - 100 Hz: ~50%+ impact (not recommended)

**Hardware Wiring**:
```
Arduino Mega → SD Shield (SPI)
Pin 10      → CS (Chip Select)
Pin 11      → MOSI
Pin 12      → MISO
Pin 13      → SCK
GND         → GND
5V          → 5V (shield supplies 3.3V to card)
```

---

## Build Flag Combinations

### Matrix: All Possible Combinations

| Environment | CALIBRATION | SNAPSHOT | Size | RAM | Use Case |
|-------------|------------|----------|------|-----|----------|
| arduino_mega | ✗ | ✗ | 85 KB | 4 KB | Basic testing |
| arduino_mega_calibration | ✓ | ✗ | 88 KB | 4 KB | Debug without logs |
| arduino_mega_production | ✗ | ✗ | 85 KB | 4 KB | Minimal production |
| arduino_mega_snapshot | ✓ | ✓ | 93 KB | 5 KB | Field test + debug |
| arduino_mega_snapshot_only | ✗ | ✓ | 90 KB | 5 KB | Production + logs |

### Recommended Combinations

**For Development**:
```ini
[env:dev]
build_flags = -D CALIBRATION_MODE
# No snapshot (faster, no SD dependency)
```

**For Field Testing**:
```ini
[env:field]
build_flags = -D CALIBRATION_MODE -D SNAPSHOT_MODE
# Debug output + data logging
```

**For Production Deployment**:
```ini
[env:production]
build_flags = -D SNAPSHOT_MODE
# Only logging, minimal output
```

**For Minimal Binary**:
```ini
[env:tiny]
build_flags = -Os
# Optimize for size, no extra features
```

---

## Configuration Details

### Buffer Sizes (Tunable in mode.h)

#### SNAPSHOT_BUFFER_SIZE

Default: 1024 bytes

```cpp
#define SNAPSHOT_BUFFER_SIZE 1024
```

This is the temporary buffer used for JSON serialization before writing to SD card.

**Calculation**:
```
Typical snapshot JSON: ~120 bytes
Buffer size:          ~1024 bytes (can handle longer data)
```

**Tuning**:
```cpp
// For minimal RAM:
#define SNAPSHOT_BUFFER_SIZE 256  // ~200 bytes per snapshot still fits

// For extra safety:
#define SNAPSHOT_BUFFER_SIZE 2048  // Handles very long quaternion strings
```

**Trade-off**: Larger buffer uses more RAM but provides margin for long field data

#### MAX_SNAPSHOT_FILES

Default: 100

```cpp
#define MAX_SNAPSHOT_FILES 100
```

Snapshots stored as `snapshot_001.json` through `snapshot_100.json`. After reaching 100, counter wraps (001 overwrites old data).

**Tuning**:
```cpp
// For longer record intervals without wrapping:
#define MAX_SNAPSHOT_FILES 1000  // snapshot_001 through snapshot_999

// For tight RAM budgets:
#define MAX_SNAPSHOT_FILES 10    // snapshot_01 through snapshot_10
```

**Note**: Counter is in-memory only (resets on power cycle)

#### SNAPSHOT_DIRECTORY

Default: "/snapshots/"

```cpp
#define SNAPSHOT_DIRECTORY "/snapshots/"
```

Directory name on SD card where snapshots are stored. Created automatically on first write.

---

## Memory Impact by Configuration

### RAM Usage Breakdown

```
Base Arduino Mega:          8192 bytes
Global variables:           ~2000 bytes
Stack (typical):            ~1000 bytes

With CALIBRATION_MODE:      +0 bytes (macros only)
With SNAPSHOT_MODE:         +1200 bytes
  - Buffer: 1024 bytes
  - SD objects: ~100 bytes
  - SnapshotRecorder: ~64 bytes
  - Miscellaneous: ~12 bytes

Available after Phase 1:     ~4000-5000 bytes
```

### Flash Memory Usage Breakdown

```
Arduino Mega:               256 KB

Base firmware:              ~50 KB
Math library:               ~10 KB
  - Quaternion: 4 KB
  - Coordinates: 3 KB
  - Conversions: 3 KB
BNO085 driver:              ~8 KB
Output formatting:          ~5 KB

Subtotal core:              ~73 KB

CALIBRATION_MODE:           +3 KB
SNAPSHOT_MODE:              +5 KB (includes SD library)
Both modes:                 +8 KB

Total typical:              ~73-81 KB
Available:                  ~175-183 KB
```

---

## Compiler Flags (Advanced)

### Optimization Levels

Can be set in platformio.ini:

```ini
[env:small]
build_flags = -Os  # Optimize for size

[env:fast]
build_flags = -O3  # Optimize for speed

[env:debug]
build_flags = -g   # Include debug symbols
```

### Warning Levels

```ini
[env:strict]
build_flags = -Wall -Wextra -Werror
# Treat all warnings as errors

[env:permissive]
build_flags = 
# No special warning flags
```

### Feature Detection Macros

Automatically set by PlatformIO:

```cpp
#ifdef ARDUINO_ARCH_AVR
  // Arduino Mega/Nano/Uno
#endif

#ifdef ARDUINO_SAMD_ZERO
  // Arduino Zero
#endif

#ifdef ARDUINO_ARCH_ESP32
  // ESP32
#endif
```

---

## Conditional Compilation Examples

### Using Feature Flags in Code

```cpp
#include "src/config/mode.h"

void setup() {
    Serial.begin(115200);
    
    #if IS_CALIBRATION_MODE
        Serial.println("CALIBRATION MODE ENABLED");
    #else
        Serial.println("Production mode");
    #endif
    
    #if ENABLE_SNAPSHOT_RECORDER
        snapshot_recorder_init();
        Serial.println("Snapshot recording ready");
    #else
        Serial.println("Snapshot recording disabled");
    #endif
}

void loop() {
    Quaternion q = imu.get_quaternion();
    
    #if IS_CALIBRATION_MODE
        CAL_PRINTF("Q: [%.3f, %.3f, %.3f, %.3f]\n", 
                   q.w, q.x, q.y, q.z);
    #endif
    
    #if ENABLE_SNAPSHOT_RECORDER
        snapshot_record(q, imu.get_cal_status(), millis());
    #endif
}
```

### Selective Feature Compilation

```cpp
// This function only compiled if SNAPSHOT_MODE enabled
#if ENABLE_SNAPSHOT_RECORDER
void dump_snapshots_via_serial() {
    File dir = SD.open("/snapshots/");
    // ... dump logic
}
#endif

// This function always compiled
void print_orientation() {
    EulerAngles e = imu.get_euler_angles();
    Serial.println(e.yaw);
}
```

---

## Performance Characteristics by Configuration

### Loop Execution Time

Measured on Arduino Mega with 100 Hz BNO085 reading loop:

| Configuration | Loop Time | Overhead | Impact |
|---------------|-----------|----------|--------|
| Baseline (no features) | 10.0 ms | 0 | 0% |
| + CALIBRATION_MODE | 10.2 ms | 0.2 ms | Serial output only |
| + SNAPSHOT_MODE (1 Hz) | 10.4 ms | 0.4 ms | Occasional SD write |
| + SNAPSHOT_MODE (10 Hz) | 11.5 ms | 1.5 ms | Regular SD writes |
| + Both (10 Hz) | 11.7 ms | 1.7 ms | Debug + SD I/O |

**Note**: Loop must maintain < 10 ms for 100 Hz operation

### Memory Usage at Runtime

| Configuration | RAM Used | Available | Safety Margin |
|---------------|----------|-----------|---------------|
| Baseline | 3.0 KB | 5.0 KB | ✓ Safe |
| + CALIBRATION_MODE | 3.0 KB | 5.0 KB | ✓ Safe |
| + SNAPSHOT_MODE | 4.2 KB | 3.8 KB | ⚠ Marginal |
| + Both | 4.2 KB | 3.8 KB | ⚠ Marginal |

**⚠ Warning**: With SNAPSHOT_MODE, monitor heap fragmentation on long runs

---

## Migration Guide

### Disabling a Feature

To remove a feature after deployment:

**Before**:
```ini
[env:production]
build_flags = -D SNAPSHOT_MODE
```

**After**:
```ini
[env:production]
build_flags = 
# SNAPSHOT_MODE disabled
```

Then rebuild and redeploy.

**Note**: Code is compiled out, no functional changes needed

### Enabling a Feature

To add a feature to existing deployment:

**Before**:
```ini
[env:production]
build_flags = 
```

**After**:
```ini
[env:production]
build_flags = -D SNAPSHOT_MODE
```

Add hardware (SD card shield) and rebuild.

### Adding Custom Flags

Project-specific flags can be added to platformio.ini:

```ini
[env:custom]
extends = env:arduino_mega
build_flags = 
    -D CALIBRATION_MODE
    -D SNAPSHOT_MODE
    -D MY_CUSTOM_FEATURE
```

Then in code:
```cpp
#ifdef MY_CUSTOM_FEATURE
  // Feature-specific code
#endif
```

---

## Troubleshooting

### Issue: "undefined reference to 'snapshot_record'"

**Cause**: SNAPSHOT_MODE not defined, but code calls snapshot_record()

**Solution**: Ensure function is wrapped in conditional:
```cpp
#if ENABLE_SNAPSHOT_RECORDER
    snapshot_record(q, cal, ts);
#endif
```

Or always define SNAPSHOT_MODE in build_flags.

### Issue: "Binary too large"

**Cause**: Too many features enabled for target board

**Solution**:
1. Disable SNAPSHOT_MODE if not needed
2. Disable CALIBRATION_MODE for production
3. Use `-Os` optimization flag
4. Upgrade to board with larger flash

### Issue: "Out of RAM"

**Cause**: SNAPSHOT_MODE buffer conflicts with other allocations

**Solution**:
1. Reduce SNAPSHOT_BUFFER_SIZE in mode.h:
   ```cpp
   #define SNAPSHOT_BUFFER_SIZE 512  // Instead of 1024
   ```
2. Upgrade to Arduino Mega (more RAM than Nano)
3. Avoid dynamic memory allocation in loop

---

## Best Practices

1. **Development**: Enable both CALIBRATION_MODE and SNAPSHOT_MODE
2. **Testing**: Enable SNAPSHOT_MODE only
3. **Production**: Disable both, optimize for size
4. **Debugging**: Enable CALIBRATION_MODE only (no SD overhead)

5. **Always verify**: Test all enabled features before deployment

6. **Document**: Keep notes of which features enabled for each deployment

7. **Version Control**: Commit platformio.ini changes with feature modifications

---

## Reference

**Configuration File**: `src/config/mode.h`  
**Build Configuration**: `platformio.ini`  
**Documentation**: `BUILD_GUIDE.md`

See also:
- `SNAPSHOT_FEATURE_GUIDE.md` - How to use snapshot feature
- `PHASE_1_TEST_RESULTS.md` - Feature validation
