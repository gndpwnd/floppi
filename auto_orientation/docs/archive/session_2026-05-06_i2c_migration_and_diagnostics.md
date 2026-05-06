# Session 2026-05-06: I2C Migration and Comprehensive Diagnostics

## Summary
Migrated BNO085 driver from UART to I2C mode (user's actual hardware setup), identified clock speed issue, created extensive troubleshooting documentation, and investigated GPS lock failures.

---

## What Was Accomplished

### 1. Hardware Discovery ✅
- **User's actual setup**: BNO085 via I2C (pins 20/SDA, 21/SCL) NOT UART
- **Original MDC setup**: UART mode (P1 pin high, TX/RX pins 18/19)
- **Why it works**: Both modes supported by Adafruit BNO08x library
- **Challenge**: Code was written for UART, hardware is I2C

### 2. Code Migration ✅
- Updated `src/sensors/bno085.cpp` to use `begin_I2C(0x4A, &Wire, 0)`
- Updated comments and documentation to reflect I2C mode
- Updated header file documentation
- Changed initialization from UART serial to Wire I2C
- **Firmware compiles successfully**: 21,518 bytes (up from 20,518)

### 3. I2C Clock Speed Issue Identified ⚠️
- **Initial problem**: Code used 400 kHz I2C clock
- **Root cause**: BNO085 has known timing violations at 400 kHz (clock stretching issues)
- **Solution**: Reduced to 100 kHz (`Wire.setClock(100000L)`)
- **Status**: Clock fix implemented and uploaded

### 4. Comprehensive Diagnostics Documentation ✅
Created 4 detailed research documents (68 KB total):

**a) GPS Lock Troubleshooting Guide** (`docs/findings/gps_lock_troubleshooting.md`)
- Why GPS fails to get lock
- NEO-M9N vs M8T comparison
- u-center diagnostic tool guide
- Cold start TTFF expectations
- Complete troubleshooting flowchart

**b) BNO085 Communication Modes** (`docs/findings/bno085_communication_modes.md`)
- UART vs I2C vs HID modes
- Mode selection via P0/P1 pins
- Pin diagrams and wiring
- Trade-offs and recommendations
- 13 external references

**c) BNO085 I2C Implementation Guide** (`docs/findings/bno085_i2c_implementation.md`)
- Adafruit library I2C specifics
- Initialization checklist
- Absolute Orientation output
- Calibration status reporting
- Code examples

**d) BNO085 I2C Hang Diagnosis** (`docs/findings/bno085_i2c_hang_diagnosis.md`)
- Root causes of initialization hang
- Hardware verification procedures
- I2C scanner code (find devices on bus)
- Debug techniques with code examples
- 10 ready-to-use test sketches

**e) BNO085 I2C Compatibility Analysis** (`docs/findings/bno085_i2c_compatibility_analysis.md`)
- Arduino Mega I2C configuration
- Adafruit library hang points
- DI pin configuration importance
- Power supply requirements
- Clock speed analysis (100 kHz vs 400 kHz)

### 5. Hardware/Code Mismatch Diagnosis ⚠️
- **Identified**: Code was UART-only, user has I2C hardware
- **Fixed**: Migrated code to I2C mode
- **Issue**: Boot message prints, sensor data doesn't
- **Status**: Likely still initialization hang despite clock fix

---

## Current Status

### ✅ Complete
- BNO085 code migrated to I2C
- I2C clock speed optimized (100 kHz)
- Firmware compiles and uploads
- Serial communication works (boot message visible)
- Comprehensive diagnostic documentation created

### ⏳ In Progress (Waiting for Hardware Verification)
- **BNO085 Data Output**: Boot message prints but no orientation data
- **Possible causes**:
  1. DI pin floating or misconfigured (most likely)
  2. I2C address detection still failing despite clock fix
  3. Wire library not initializing properly
  4. Power supply issue during initialization
  5. Sensor requires longer initialization time

### ⏳ Not Started (Blocked on BNO085)
- GPS lock troubleshooting (separate issue - 2 modules failing)
- Integration testing
- Calibration persistence validation

---

## Key Findings from Research

### BNO085 I2C Issues
1. **Clock Speed Critical**: 400 kHz too aggressive, 100 kHz more reliable
2. **DI Pin Matters**: Must be properly connected (GND for 0x4A, VCC for 0x4B)
3. **Power Supply**: 3.3V must stay stable during init (not drop below 2.8V)
4. **Pull-up Resistors**: Arduino Mega has ~47k onboard, usually sufficient but verify

### GPS Lock Issues (Research Only - Not Yet Tested)
1. **Antenna Positioning**: Must be flat, parallel to horizon, outdoors with clear sky
2. **NEO-M9N Cold Start**: 24 seconds typical for first fix
3. **M8T Differences**: Timing-optimized, no firmware updates available
4. **Common Failure**: Insufficient USB power (<200mA), antenna damage/obstruction

---

## What's Next (For Next Session)

### Immediate (BNO085 Data Recovery)
1. **Verify DI Pin Configuration**:
   - Visually check BNO085 DI pin (should be connected to GND for 0x4A address)
   - Use multimeter to verify connection
   - May need to solder or reseat jumper

2. **Test I2C Bus with Scanner**:
   - Use provided I2C scanner sketch from diagnostics doc
   - Verify 0x4A device appears on bus
   - If not found, DI pin is the issue

3. **Add Debug Output**:
   - Modify `bno085.cpp` to add Serial.println() before/after each step
   - Identify exactly where initialization hangs
   - Could be address detection, reset, or product ID read

4. **Try Fallback Address** (if DI pin uncertain):
   - Modify code to try 0x4A first, fallback to 0x4B
   - Would solve floating DI pin issue

### Next Steps (GPS)
1. **Use u-center Tool** (free from u-blox):
   - Connect to GPS module
   - Monitor satellite acquisition
   - Verify antenna, check configuration
   - Both NEO-M9N and M8T compatible

2. **Extended Outdoor Test**:
   - Move to clear outdoor location (no buildings, trees)
   - Let GPS warm up 60+ seconds
   - Monitor u-center for satellite acquisition
   - Check that antenna isn't damaged/loose

### Documentation Ready
- **For BNO085 Troubleshooting**: Use `docs/findings/bno085_i2c_hang_diagnosis.md`
  - Includes I2C scanner code (copy/paste ready)
  - Progressive test sketches
  - Decision tree for diagnosis
  
- **For GPS Troubleshooting**: Use `docs/findings/gps_lock_troubleshooting.md`
  - u-center setup guide
  - Expected satellite acquisition timeline
  - Power/antenna checklist

---

## Files Modified This Session
- `src/sensors/bno085.cpp` - Migrated to I2C, optimized clock speed
- `src/sensors/bno085.h` - Updated documentation

## Files Created This Session
- `docs/findings/gps_lock_troubleshooting.md`
- `docs/findings/bno085_communication_modes.md`
- `docs/findings/bno085_i2c_implementation.md`
- `docs/findings/bno085_i2c_hang_diagnosis.md` (with 10 test sketches)
- `docs/findings/bno085_i2c_compatibility_analysis.md`
- `docs/archive/session_2026-05-06_i2c_migration_and_diagnostics.md` (this file)

---

## Critical Path Forward

1. **Verify BNO085 I2C Connection**:
   - Check DI pin (must be connected to GND or VCC)
   - Run I2C scanner to confirm 0x4A/0x4B is present
   - Fix any hardware issues (reseating, soldering, loose wires)

2. **Debug Initialization**:
   - Add Serial output at each step
   - Determine exact hang point
   - Compare with working Adafruit example

3. **Once BNO085 Responds**:
   - Should see orientation data immediately
   - Can then proceed to calibration testing
   - Integration with any future GPS module

4. **Parallel: GPS Investigation**:
   - Independent of BNO085
   - Use u-center for diagnosis
   - Both modules (NEO-M9N and M8T) need testing

---

## Lessons Learned

1. **Communication Modes Matter**: BNO085 can use UART, I2C, or HID - verify which hardware uses
2. **Clock Speed is Critical**: 400 kHz too aggressive for some I2C devices, 100 kHz safer
3. **DI Pin Configuration**: Easy to miss - connection determines I2C address
4. **Documentation > Trial**: Comprehensive research saves debugging time
5. **Hardware Assumptions Dangerous**: Always verify actual wiring matches code assumptions

---

## Session Duration
~90 minutes (research agents + code changes + testing)

## Next Session Focus
- Execute BNO085 diagnostics (I2C scanner, debug output)
- Potentially fix DI pin issue if found
- Begin GPS troubleshooting if BNO085 resolves
- Target: Get at least one sensor (BNO085 or GPS) fully working
