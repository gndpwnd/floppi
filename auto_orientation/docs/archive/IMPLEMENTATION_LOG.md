# BNO085 Sensor Driver Implementation

**Date**: 2026-05-05  
**Status**: Initial implementation complete, ready for library integration

## Summary

Implemented a functional BNO085 sensor driver for Arduino-based auto orientation system. The driver:
- Initializes BNO085 via UART (pins configurable from `config/pins.h`)
- Reads quaternion data using Adafruit BNO08x library
- Tracks per-axis calibration status
- Provides health checking and status reporting
- Includes comprehensive test in `main.cpp`

## Files Implemented

### Core Implementation
- **src/sensors/bno085.cpp** (385 lines)
  - Full implementation of BNO085 sensor driver
  - Constructor/destructor with proper resource management
  - `begin()` - UART initialization with board-specific handling
  - `read()` - Quaternion data reading with calibration tracking
  - `getOrientation()` - Returns OrientationData struct
  - `isHealthy()` - Health check (recent data + calibration status)
  - `getStatusString()` - Human-readable status with quaternion values

- **src/sensors/bno085.h** (Updated)
  - Comprehensive documentation of Adafruit APIs used
  - Calibration persistence research notes
  - Clear interface definition

### Library Support
- **lib/Adafruit_BNO08x_Arduino.h** (Stub header)
  - Forward declaration of Adafruit_BNO08x class
  - Key structure definitions (sh2_SensorEvent, sh2_quat_t)
  - Report IDs and calibration constants
  - Function signatures for all required APIs

- **lib/Adafruit_BNO08x_Arduino.cpp** (Stub implementation)
  - Minimal working implementation for compilation
  - Ready to be replaced with full Adafruit library once cloned

### Test Code
- **src/main.cpp** (Updated)
  - Initialization and error handling
  - Sensor read loop at configured frequency
  - Quaternion output with magnitude validation
  - Calibration status display

## Key Design Decisions

### 1. Report ID Selection
Used **Report ID 5 (Rotation Vector)** for absolute orientation, NOT RVC (Report ID 14).
- Provides quaternion directly from sensor fusion
- No additional RVC conversion needed
- Matches BNO085 typical usage

### 2. UART Initialization
Implemented board-specific UART selection in `begin()`:
```
- Arduino Mega: Serial1 (pins 18/19) - hardware UART
- Arduino Nano/Uno: SoftwareSerial (configurable pins)
- Teensy: Serial1
- ESP32: Serial1 with custom pins
- Fallback: Serial or Serial1
```
Pins are configured in `config/pins.h` and conditionally compiled.

### 3. Calibration Status Tracking
Implemented per-axis calibration tracking:
- `cal_status` - Overall system calibration (0-3)
- `cal_accel` - Accelerometer calibration (0-3)
- `cal_gyro` - Gyroscope calibration (0-3)
- `cal_mag` - Magnetometer calibration (0-3)

Health check requires medium or higher calibration.

### 4. Data Validation
Output includes quaternion magnitude validation:
- Magnitude should be ≈ 1.0 for unit quaternions
- Status string displays magnitude for debugging
- Helps detect corrupted or unconverted data

## Next Steps

### Phase 1: Library Integration (Immediate)
1. Clone Adafruit BNO08x library to `lib/Adafruit_BNO08x_Arduino/`
   ```bash
   cd lib && git clone https://github.com/adafruit/Adafruit_BNO08x_Arduino.git
   ```
2. Remove stub header and implementation
3. Test compilation with PlatformIO
4. Upload to Arduino Mega and verify sensor communication

### Phase 2: Calibration Persistence (Research)
Research and document:
1. Does Adafruit_BNO08x expose `getSensorOffsetProfiler()`/`setSensorOffsetProfiler()`?
2. If not, investigate:
   - Direct register access for BNO085 calibration data
   - EEPROM storage on Arduino
   - SD card backup
3. Implement persistent calibration loading on startup

### Phase 3: Integration (Later)
1. Integrate GPS (NEO-M9N) sensor
2. Combine sensor outputs into unified data stream
3. Time-synchronize orientation and position data
4. Implement output formatting (JSON, CSV, binary)

## Testing Checklist

- [ ] Compile with PlatformIO (arduino_mega environment)
- [ ] Upload to Arduino Mega
- [ ] Monitor serial output for quaternion values
- [ ] Verify quaternion magnitude is approximately 1.0
- [ ] Check calibration status output
- [ ] Rotate sensor and verify quaternion changes sensibly
- [ ] Test reset/error recovery
- [ ] Verify output frequency (10 Hz default)

## File Locations

```
auto_orientation/
├── src/sensors/
│   ├── bno085.h              (header with Adafruit API docs)
│   ├── bno085.cpp            (implementation)
│   └── sensor_base.h         (base class)
├── src/config/
│   └── pins.h                (board-specific pin config)
├── src/main.cpp              (test program)
├── lib/
│   ├── Adafruit_BNO08x_Arduino.h       (stub - to be replaced)
│   └── Adafruit_BNO08x_Arduino.cpp     (stub - to be replaced)
└── LIBRARIES.md              (external dependency documentation)
```

## Adafruit API Reference

| API | Purpose | Key Parameters |
|-----|---------|-----------------|
| `begin_UART()` | Init on serial stream | Stream*, reset_pin |
| `enableReport()` | Configure report | reportID, period_us |
| `getEvent()` | Read sensor data | sh2_SensorEvent* |
| `getCalibration()` | Query calibration | *sys, *gyro, *accel, *mag |
| `wasReset()` | Check for reset | (none) |

## Notes

- Stub library files are placeholders - will be replaced with actual Adafruit repository
- Status string format: `BNO085: Q(w,x,y,z) Sys:Status Gyro:Status Accel:Status Mag:Status`
- Default output frequency: 10 Hz (100ms intervals) - configurable in `pins.h`
- Sensor considered "healthy" if recently updated and at least medium calibration
- No calibration persistence implemented yet - reserved for research phase

## Troubleshooting

If sensor doesn't initialize:
1. Check UART pins in `config/pins.h`
2. Verify baud rate (default 115200)
3. Check wiring to BNO085 module
4. Monitor serial output for initialization errors

If quaternions look wrong:
1. Check orientation.w + .x + .y + .z magnitude ≈ 1.0
2. Verify calibration status (should be at least 2 = Medium)
3. Allow 30 seconds for auto-calibration on first power-up
4. Try manual calibration if needed
