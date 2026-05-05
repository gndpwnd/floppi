# BNO085 Implementation - Next Steps

## Status
BNO085 sensor driver implementation is **COMPLETE** and ready for integration.

## Immediate Next Steps (Phase 1: Library Integration)

### 1. Clone Adafruit Library
When network connectivity is available, clone the actual Adafruit library:

```bash
cd lib
git clone https://github.com/adafruit/Adafruit_BNO08x_Arduino.git
```

This will replace the stub files with the complete library.

### 2. Remove Stubs
Delete the placeholder files (optional - library will take precedence):

```bash
rm lib/Adafruit_BNO08x_Arduino.h
rm lib/Adafruit_BNO08x_Arduino.cpp
```

### 3. Test Compilation
Compile with PlatformIO:

```bash
pio run --environment arduino_mega
```

### 4. Upload and Test
Upload to Arduino Mega and monitor serial output:

```bash
pio run --environment arduino_mega --target upload
pio device monitor --baud 115200
```

Expected output:
```
=== Auto Orientation System ===
Board: Initializing BNO085 sensor...
BNO085 initialized successfully!
Reading orientation data...

1234 | Q: 0.7071,0.0000,0.0000,0.7071 | Mag: 1.0000 | Status: BNO085: Q(0.7071,0.0000,0.0000,0.7071) Sys:High Gyro:High Accel:High Mag:High
```

## Testing Checklist

- [ ] Code compiles without errors
- [ ] Sensor initializes successfully
- [ ] Quaternion values output at 10 Hz
- [ ] Quaternion magnitude is approximately 1.0
- [ ] Calibration status shows improvement over time
- [ ] Rotating sensor updates quaternion values sensibly
- [ ] Reset/error recovery works
- [ ] Output frequency matches configuration (10 Hz default)

## Validation Criteria

### Quaternion Validation
- Magnitude `sqrt(w² + x² + y² + z²)` should be ≈ 1.0
- If magnitude is significantly different, check:
  - Sensor is properly calibrated
  - UART communication is working
  - Data isn't corrupted during transmission

### Calibration Status
- Should progress from 0 (uncalibrated) → 3 (fully calibrated)
- Initial startup: Allow 30 seconds for auto-calibration
- Each axis (accel, gyro, mag) may calibrate at different rates
- Overall system status (Sys) should reach "High" for normal operation

### Health Checks
- `isHealthy()` returns true when:
  - Recent data received (< 2 seconds old)
  - Calibration status >= 2 (Medium)
- May return false initially while sensor is calibrating

## Phase 2: Calibration Persistence (Research Phase)

After library integration and testing works, implement calibration persistence:

### Research Steps
1. Check if Adafruit_BNO08x has `getSensorOffsetProfiler()` method
2. If available, use it for save/restore calibration
3. If not, implement manual approach:
   - Read calibration registers directly
   - Save to Arduino EEPROM
   - Restore on startup

### Implementation Options
1. **Adafruit API** (preferred if available)
   - `getSensorOffsetProfiler(buffer, length)` - save calibration
   - `setSensorOffsetProfiler(buffer, length)` - restore calibration

2. **EEPROM Fallback** (if API not available)
   - Store 256 bytes calibration data in EEPROM
   - Load and apply on startup
   - See findings/bno085-calibration-persistence.md for register details

3. **SD Card Backup** (for systems with storage)
   - Secondary backup of calibration
   - Named by date/time for history

### Files to Create/Update
- `src/sensors/calibration_manager.h` - New class for persistence
- `src/sensors/bno085.cpp` - Call calibration manager on startup
- `findings/calibration-implementation.md` - Document chosen approach

## Phase 3: GPS Integration

After BNO085 is working:

1. Implement NEO-M9N driver (`src/sensors/neo_m9n.cpp`)
2. Extend `main.cpp` to read from both sensors
3. Time-synchronize orientation and position data
4. Implement combined output format

## Phase 4: Output Formatting

When both sensors are working:

1. Implement JSON output format
2. Implement CSV output format
3. Support multiple output frequencies
4. Document output schema

## Quick Reference

### Key Files
- **Driver**: `src/sensors/bno085.cpp` (385 lines)
- **Header**: `src/sensors/bno085.h` (with API docs)
- **Test**: `src/main.cpp` (demonstrates usage)
- **Config**: `src/config/pins.h` (board-specific pins)
- **Library**: `lib/Adafruit_BNO08x_Arduino/` (to be cloned)

### Important Constants (in pins.h)
- `SERIAL_OUTPUT_FREQUENCY_HZ = 10` (change for different output rate)
- `BNO085_RX_PIN / BNO085_TX_PIN` (board-specific)
- `BNO085_USE_SOFTWARE_SERIAL` (true for Nano/Uno, false for Mega)

### Important Report IDs
- `QUAT_REPORT_ID = 5` (Rotation Vector / Absolute Orientation)
- `QUAT_RVC_REPORT_ID = 14` (RVC variant - NOT USED)

### Calibration Levels
- 0 = Uncalibrated
- 1 = Low calibration
- 2 = Medium calibration
- 3 = High calibration (fully calibrated)

## Troubleshooting

### Sensor won't initialize
- Check UART pins in `config/pins.h`
- Verify baud rate (115200)
- Check physical wiring to BNO085
- Ensure BNO085 has power (3.3V or 5V depending on module)

### Quaternions look wrong
- Check magnitude is ~1.0
- Allow 30 seconds for calibration
- Rotate sensor to calibrate axes
- Check if sensor needs factory reset

### No output from sensor
- Verify serial connection is working
- Check `imu.hasNewData()` returns true
- Monitor serial for error messages
- Check reset pin configuration

## Documentation

All relevant documentation is in the `docs/` directory:
- `README.md` - General overview and wiring guide
- `findings/` - Research on BNO085 calibration and GPS
- `archive/` - Initial implementation attempts
- `scope.md` - Project goals and scope
- `roadmap.md` - Implementation phases

## Support

For issues or questions:
1. Check error messages in serial output
2. Review `docs/findings/` for known issues
3. Check `IMPLEMENTATION_LOG.md` for design decisions
4. Review Adafruit library documentation when available
