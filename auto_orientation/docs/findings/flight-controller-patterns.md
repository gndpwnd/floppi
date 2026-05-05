# Research: Flight Controller Code Patterns & Reusable Components

**Status**: Initial Research Complete  
**Priority**: High (informs auto_orientation architecture)  
**Last Updated**: 2026-05-05

---

## Overview

The flight_controller project provides useful patterns for sensor abstraction, calibration, and data processing. This document identifies what can be reused and what must be adapted for auto_orientation.

---

## Key Patterns Found

### 1. Sensor Abstraction via Preprocessor Directives

**Pattern**: Compile-time sensor selection using #defines

```cpp
#ifdef USE_MPU6050
  #include "MPU6050.h"
  MPU6050 mpu6050;
#elif defined(USE_MPU9250)
  #include "MPU9250.h"
  MPU9250 mpu9250(SPI, 36);
#endif
```

**Pros**:
- No runtime overhead
- Single binary for target board
- Easy to understand

**Cons**:
- Not flexible for runtime sensor selection
- Recompile needed to switch sensors
- Doesn't match auto_orientation's goal (multi-sensor support)

**Recommendation for auto_orientation**:
- Use virtual base classes (sensor_base.h approach) instead
- Allows runtime sensor selection
- Support multiple sensors simultaneously (e.g., BNO085 + MPU 6050 side-by-side for testing)

---

### 2. Calibration System

Flight controller has detailed calibration routines:

#### Available Calibrations
- **Basic IMU**: Gyro/accel offset on level surface (~30 samples)
- **6-Position Accelerometer**: Gravity measurement in 6 orientations → scale factors + offsets
- **Magnetometer**: Sphere calibration (rotate through all orientations)
- **IMU Orientation**: Auto-detect which physical axis maps to aircraft forward/right/up
- **Radio/ESC**: Separate calibration routines

#### Data Structures
```cpp
struct CalibrationResults {
  float accErrorX, accErrorY, accErrorZ;  // Offsets
  float gyroErrorX, gyroErrorY, gyroErrorZ;
  float accScaleX, accScaleY, accScaleZ;  // Scale factors (6-position)
  char forwardAxis, rightAxis, upAxis;    // Orientation mapping
  // ... more fields
};
```

**Reusable**:
- Calibration results structure (adapt for auto_orientation)
- 6-position calibration algorithm
- Orientation auto-detection concept

**Must Change**:
- Flight controller uses compile-time config storage
- Auto_orientation needs runtime + persistent storage (flash/SD)
- BNO085 has built-in fusion; different approach than flight controller's Madgwick filter

---

### 3. Data Flow Pattern

```
setupIMU()           // Initialize hardware
  ↓
getIMUdata()         // Read raw data
  ↓
Apply offsets/scale  // calibration.h values
  ↓
Filter (optional)    // filters.h
  ↓
Attitude estimation  // Madgwick filter
  ↓
Output               // Serial / WiFi / Web
```

**For auto_orientation**:
- Similar high-level flow, but:
  - BNO085 returns quaternions directly (no manual sensor fusion needed)
  - MPU 6050 still requires Madgwick or similar
  - GPS adds position data in parallel

---

### 4. Hardware Abstraction

Flight controller has pin definitions for multiple boards:

```cpp
#include "pin_definitions.h"  // Generic
#include "pin_definitions_esp32.h"  // ESP32-specific
```

**Pattern**: Board-specific includes, common interface

**For auto_orientation**:
- Adapted to src/config/pins.h (single board-agnostic file with conditionals)
- Supports Arduino Nano/Mega/Teensy/ESP32

---

## Calibration Insights for Auto Orientation

### BNO085 vs. Flight Controller's MPU6050

| Aspect | BNO085 | Flight Controller (MPU6050) |
|--------|--------|---------------------------|
| **Output** | Absolute quaternion (built-in fusion) | Raw accel/gyro (manual fusion) |
| **Calibration** | Factory-based; magnetometer needs tuning | Gyro/accel offsets + scales |
| **Persistence** | Onboard flash memory | Config.h (compile-time) |
| **Magnetometer** | Included | Separate IC (MPU9250 adds it) |
| **Complexity** | Lower (chip handles fusion) | Higher (firmware implements Madgwick) |

**Implication**: auto_orientation's approach is simpler but requires understanding BNO085's built-in fusion.

### Calibration Persistence Challenge

Flight controller stores calibration in **config.h** (compile-time constant):

```cpp
#define ACC_OFFSET_X -0.1234f
#define GYRO_ERROR_X -0.5678f
// ...
```

BNO085 supports **runtime storage** to onboard flash, but:
- Adafruit library may not expose save/restore directly
- Need to investigate library API or use raw SPI/I2C commands
- See: [bno085-calibration-persistence.md](bno085-calibration-persistence.md)

---

## GPS Integration Gaps

**Flight controller has NO GPS integration**. This is entirely new for auto_orientation.

**Key decisions needed**:
- How to parse NMEA sentences from NEO-M9N
- Whether to implement static accuracy improvement (multi-sample averaging)
- How to synchronize timestamp between BNO085 (I2C UART) and GPS (USB serial)

---

## Reusable Code from flight_controller/tools/

### serial_monitor.py
✅ **Copied to auto_orientation/tools/**

Already adapted for:
- BNO085 + GPS output
- Pattern-based waiting (e.g., wait for "GPS_LOCK")
- CSV logging
- Serial communication with Teensy USB CDC support

### Other Scripts in flight_controller/tools/

Available for future use:
- **calibrate.sh** - Automated calibration runner (shell script)
- **dev.sh** - Development environment setup
- **calibration_reset.py** - Reset calibration values
- **complexity_calculator.py** - Code complexity analysis

**Recommendation**: Review and adapt these as needed during implementation.

---

## Architecture Decisions for auto_orientation

Based on flight_controller patterns:

| Decision | Flight Controller | auto_orientation | Rationale |
|----------|------------------|------------------|-----------|
| **Sensor Selection** | Compile-time #defines | Runtime virtual classes | Need multi-sensor simultaneous support |
| **Calibration Storage** | config.h (compile) | Flash/SD (runtime) | Need persistent storage across power cycles |
| **Attitude Filter** | Madgwick (9DOF) | BNO085 firmware (for v1.0) | BNO085 handles fusion; reduces code complexity |
| **Hardware Support** | Teensy 4.x primary | Arduino Mega primary, others later | Different target constraints |
| **Build System** | platformio.ini + build configs | Single platformio.ini, env selection | Simpler for embedded toolkit |

---

## Recommendations for Implementation

### Phase 1: BNO085 Integration
1. Adapt calibration results structure from flight_controller
2. Implement sensor_base.h interface (different from flight_controller's #define approach)
3. Research BNO085 calibration persistence (critical gap)
4. Test data flow: BNO init → quaternion output → CSV logging

### Phase 2: GPS Integration
1. Implement NMEA parser (not in flight_controller; entirely new)
2. Test NEO-M9N connection and data parsing
3. Investigate accuracy improvement techniques (see [gps-accuracy-improvement.md](gps-accuracy-improvement.md))

### Phase 3: Combined System
1. Synchronize timestamps between BNO085 and GPS
2. Implement combined data structure
3. Output as JSON/CSV

### Phase 4: Calibration Persistence
1. Understand BNO085 flash memory API
2. Implement save/restore for magnetometer calibration
3. Test power cycle behavior

---

## References

- Flight controller repo: /home/devel/floppi/flight_controller
- Relevant files:
  - include/imu.h, include/calibration.h
  - src/imu.cpp, src/calibration*.cpp
  - include/config.h (calibration defines)
  - tools/serial_monitor.py (already reused)

---

## Next Steps

1. ✅ Research complete (this document)
2. ⏳ Investigate BNO085 calibration persistence (Adafruit library API)
3. ⏳ Investigate GPS accuracy improvement techniques
4. ⏳ Begin BNO085 sensor driver implementation
5. ⏳ Begin NEO-M9N GPS driver implementation

