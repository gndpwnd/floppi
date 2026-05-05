# BNO085 Hardware Testing - Quick Reference

## Compilation Status: ✓ SUCCESS

The BNO085 sensor driver has been successfully compiled for Arduino Mega hardware.

**Build Summary:**
- Compiler: avr-g++ (Arduino AVR framework)
- Target: Arduino Mega 2560
- Firmware Size: 25 KB (9.9% of available flash)
- Memory: 4,845 bytes RAM used (59.1% of available)
- Build Time: 0.96 seconds

## Quick Test Procedure

### Step 1: Upload Firmware
```bash
cd /home/devel/floppi/auto_orientation

# Build and upload to Mega
platformio run --target upload

# If permission denied on /dev/ttyACM0:
# - Either: Add user to dialout group: sudo usermod -a -G dialout devel
# - Or: Run with sudo (requires password)
```

**Expected Output:**
```
Processing arduino_mega...
Building in release mode
... [compilation messages] ...
Looking for upload port...
Auto-detected: /dev/ttyACM0
Uploading firmware...
======================== [SUCCESS] ========================
```

### Step 2: Monitor Serial Output
```bash
# In a terminal window, monitor the BNO085 output
python3 tools/serial_monitor.py /dev/ttyACM0 --baud 115200 --wait 15

# Or use simple serial monitor:
minicom -D /dev/ttyACM0 -b 115200

# Or use screen:
screen /dev/ttyACM0 115200
```

**Expected Startup Sequence:**
```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
BNO085 OK
Board: Initializing NEO-M9N GPS sensor...
ERROR: NEO-M9N initialization failed! (OK if no GPS connected)
...
Reading sensor data...

1000 | Q: 0.7071,0.0000,0.0000,0.7071 | Mag: 1.0000 | IMU: BNO085 OK | Q: ... | Cal: Low
1100 | Q: 0.7081,0.0045,-0.0023,0.7061 | Mag: 1.0001 | IMU: BNO085 OK | Q: ... | Cal: Medium
1200 | Q: 0.7089,0.0089,-0.0045,0.7053 | Mag: 0.9999 | IMU: BNO085 OK | Q: ... | Cal: High
```

### Step 3: Validation Checks

#### Check 1: Quaternion Magnitude
```
Expected: 1.0000 (±0.0001)
Formula: magnitude = sqrt(w² + x² + y² + z²)
```

#### Check 2: Calibration Status
```
0-2 seconds:   Status 0 (Unreliable) - Warming up
5-10 seconds:  Status 1 (Low)
30+ seconds:   Status 3 (High) - Converged
```

#### Check 3: Update Frequency
```
Expected: ~10 Hz (one line every 100ms)
Check: Count lines in 10 seconds (should be ~100 lines)
```

#### Check 4: Rotate Sensor
```
Slowly rotate BNO085 and verify:
- Quaternion components change smoothly
- No sudden jumps or spikes
- Magnitude stays ~1.0
- Calibration status stays high (3)
```

## Hardware Checklist

- [ ] Arduino Mega connected to USB (/dev/ttyACM0)
- [ ] BNO085 wired to Serial1 (pins 18/19)
  - [ ] RX pin 19 connected to BNO085 TX
  - [ ] TX pin 18 connected to BNO085 RX
  - [ ] GND connected
  - [ ] 5V power connected
- [ ] BNO085 has power LED on (steady or blinking)
- [ ] Firmware compiled successfully
- [ ] Serial monitor ready

## Troubleshooting

### Upload Failed: Permission Denied
```bash
# Add user to dialout group (requires sudo)
sudo usermod -a -G dialout devel
sudo usermod -a -G dialout $USER

# Then log out and log back in, or use:
newgrp dialout
```

### No Serial Output After Upload
1. Check USB cable is properly connected
2. Press RESET button on Mega
3. Verify /dev/ttyACM0 exists: `ls -la /dev/ttyACM*`
4. Try unplugging and replugging USB cable

### Quaternion Values are All Zero
1. Check BNO085 has 5V power
2. Check serial wiring on pins 18/19
3. Verify BNO085 boot sequence (LED should blink)

### Calibration Stuck at Low (1)
1. Normal during warm-up (takes 30+ seconds)
2. Try rotating sensor slowly
3. Keep sensor away from magnets
4. Ensure adequate space for gyroscope calibration

## Files Involved

### Firmware Source
- `src/main.cpp` - Main loop and output formatting
- `src/sensors/bno085.cpp` - BNO085 driver implementation
- `src/sensors/bno085.h` - BNO085 driver interface
- `src/config/pins.h` - Pin configuration

### Configuration
- `platformio.ini` - PlatformIO build configuration
- `lib/Adafruit_BNO08x_Arduino/` - Adafruit sensor library

### Documentation
- `docs/findings/bno085_hardware_test.md` - Detailed test results
- `QUICK_TEST.md` - This file

## Expected Serial Output Format

```
TIMESTAMP | Q: w,x,y,z | Mag: magnitude | IMU: status | GPS: status

Example breakdown:
1000              - Milliseconds since boot
Q: 0.7071,0.0000,0.0000,0.7071  - Quaternion (w, x, y, z)
Mag: 1.0000       - Quaternion magnitude (should be ~1.0)
Cal: Medium       - Calibration status (Unreliable/Low/Medium/High)
GPS: NOT INITIALIZED  - GPS status (will fail if not connected)
```

## Performance Metrics

| Metric | Value | Status |
|--------|-------|--------|
| Compilation | 0.96 s | ✓ Fast |
| Firmware Size | 25 KB | ✓ 9.9% of 248 KB |
| RAM Used | 4.8 KB | ✓ 59.1% of 8 KB |
| Flash Used | 25 KB | ✓ 9.9% of 248 KB |
| Update Rate | 10 Hz | ✓ Configured |
| Sensor Warmup | <2 seconds | ✓ Normal |
| Calibration Time | 30+ seconds | ✓ Normal |

## Next Steps After Testing

If all validation checks pass:

1. **Document Results**: Update this file with test data
2. **Integrate GPS**: Test with NEO-M9N if available
3. **Calibration Persistence**: Implement save/load of calibration
4. **Advanced Testing**: 
   - Long-term stability (hours)
   - Temperature effects
   - Magnetic interference
   - Integration with flight control

## Contact & Support

For issues or questions:
1. Check `docs/findings/bno085_hardware_test.md` for detailed information
2. Review BNO085 datasheet: https://www.adafruit.com/product/4754
3. Check Adafruit library docs: https://github.com/adafruit/Adafruit_BNO08x_Arduino

---
Last Updated: 2026-05-05
Status: Ready for Hardware Testing
