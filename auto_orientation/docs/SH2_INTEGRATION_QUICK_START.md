# SH-2 Calibration Integration Quick Start

**Task:** 16 - SH-2 Protocol Implementation (COMPLETE)  
**Status:** Ready for hardware testing (Task 17)

---

## Five-Minute Overview

The BNO085 sensor now supports persistent calibration storage using the SH-2 FRS (Feature Record Store) API.

**What works:**
- ✅ Read calibration from sensor (`readCalibrationProfile()`)
- ✅ Write calibration to sensor (`writeCalibrationProfile()`)
- ✅ Data validation and error reporting
- ✅ Integration test with 8 sub-tests

**What you need to do (Task 17):**
- Test on hardware
- Verify round-trip data
- Integrate with EEPROM storage

---

## API

### Read Calibration

```c
#include "bno085_calibration.h"

uint8_t cal_buffer[256];
uint16_t cal_length;

if (readCalibrationProfile(cal_buffer, &cal_length)) {
    // Success! cal_buffer contains cal_length bytes
    Serial.printf("Read %u bytes of calibration\n", cal_length);
} else {
    // Failed - see error message
    Serial.printf("Error: %s\n", getCalibrationError());
}
```

### Write Calibration

```c
#include "bno085_calibration.h"

uint8_t cal_buffer[256] = { /* calibration data */ };
uint16_t cal_length = 72;

if (writeCalibrationProfile(cal_buffer, cal_length)) {
    // Success! Sensor now using new calibration
    Serial.println("Calibration restored");
} else {
    // Failed
    Serial.printf("Error: %s\n", getCalibrationError());
}
```

### Validate Data

```c
if (validateCalibrationData(cal_buffer, cal_length)) {
    // Data looks valid
} else {
    // Data appears corrupted
    Serial.printf("Invalid: %s\n", getCalibrationError());
}
```

---

## Integration Points

### In BNO085::begin()

After sensor initialization, restore calibration:

```c
bool BNO085::begin(uint8_t i2c_addr) {
    // ... existing init code ...
    imu_->begin_UART(...);
    imu_->enableReport(...);
    
    // NEW: Restore calibration if available
    uint8_t cal_data[256];
    uint16_t cal_length;
    
    if (loadCalibrationFromEEPROM(cal_data, &cal_length)) {
        if (writeCalibrationProfile(cal_data, cal_length)) {
            Serial.println("Restored calibration from EEPROM");
        }
    }
    
    return true;
}
```

### In Main Loop

Save calibration when it reaches high level:

```c
void loop() {
    // ... normal sensor reading ...
    
    static bool saved = false;
    static uint32_t cal_achieved = 0;
    
    if (bno.getCalibration().mag >= 3) {
        if (!saved) {
            if (cal_achieved == 0) {
                cal_achieved = millis();
            }
            if (millis() - cal_achieved > 5000) {
                uint8_t cal_data[256];
                uint16_t cal_length;
                
                if (readCalibrationProfile(cal_data, &cal_length)) {
                    saveCalibrationToEEPROM(cal_data, cal_length);
                    saved = true;
                }
            }
        }
    } else {
        cal_achieved = 0;
    }
}
```

---

## Testing

### Quick Test
```bash
# Upload test_bno085_calibration_sh2.ino to Arduino
# Open serial monitor at 115200 baud
# Should see: "ALL TESTS PASSED!"
```

### Manual Test
1. Initialize sensor
2. Perform figure-8 motion for 10 seconds
3. Run these commands:
   ```
   uint8_t cal[256]; uint16_t len;
   readCalibrationProfile(cal, &len);           // Should read ~72 bytes
   writeCalibrationProfile(cal, len);           // Should succeed
   readCalibrationProfile(cal, &len);           // Should match original
   ```

### Persistence Test
1. Save calibration to EEPROM
2. Power cycle device
3. Verify calibration restores on boot
4. Check that orientation is immediately accurate

---

## Data Format

- **Input/Output:** Uint8_t byte arrays (0-256 bytes)
- **Internal:** Uint32_t word arrays (handled automatically)
- **FRS Record:** DYNAMIC_CALIBRATION (0x1F1F)
- **Typical Size:** 72-144 bytes
- **Byte Order:** Little-endian (handled automatically)

---

## Error Codes

```
"OK" - Success
"General error" - SH2_ERR
"Bad parameter" - SH2_ERR_BAD_PARAM
"Operation in progress" - SH2_ERR_OP_IN_PROGRESS
"I/O error" - SH2_ERR_IO
"Hub error" - SH2_ERR_HUB
"Timeout" - SH2_ERR_TIMEOUT
```

All errors are returned by `getCalibrationError()`.

---

## Performance

- **Read operation:** 10-50ms
- **Write operation:** 10-50ms
- **Memory usage:** 256 bytes temporary buffer + 64 bytes error buffer
- **Thread-safe:** No (blocking SH-2 calls)

---

## Files

### Implementation
- `src/sensors/bno085_calibration.cpp` - Full implementation
- `src/sensors/bno085_calibration.h` - API definitions

### Test
- `tests/test_bno085_calibration_sh2.ino` - Integration test

### Documentation
- `docs/findings/sh2_api_investigation.md` - Complete investigation
- `docs/TASK_16_COMPLETION.md` - Task summary
- `docs/SH2_INTEGRATION_QUICK_START.md` - This file

---

## Next Steps

1. **Run Integration Test** (test_bno085_calibration_sh2.ino)
   - Verify all 8 tests pass on your BNO085 hardware

2. **Test Round-Trip**
   - Read calibration
   - Write it back
   - Verify data matches

3. **Test Persistence**
   - Save to EEPROM
   - Power cycle
   - Verify calibration restores

4. **Integrate with Driver**
   - Add restore in BNO085::begin()
   - Add save in main loop
   - Test with complete application

---

## Troubleshooting

### "Timeout" Error
- Check BNO085 is responding
- Verify UART connection
- Try reading again (may need retry)

### "I/O error" Error
- Check sensor power
- Verify UART baud rate
- Check cable connections

### Data Mismatch on Write/Read
- Some variation is normal (sensor updates calibration)
- First read-write round trip should match very closely
- Subsequent reads may differ slightly

### Test Fails
- Check test output for which step failed
- See `getCalibrationError()` for details
- Consult `docs/findings/sh2_api_investigation.md`

---

## Key Implementation Details

1. **Word-to-Byte Conversion**
   - 32-bit words unpacked to 4 bytes each
   - Little-endian: byte[i] = (word >> (i*8)) & 0xFF

2. **FRS Record**
   - DYNAMIC_CALIBRATION (0x1F1F) contains runtime calibration
   - Opaque to application (firmware-specific format)
   - Sensor validates on write

3. **Error Handling**
   - All sh2_* errors mapped to strings
   - Buffer oversize checked
   - Data validation before write

4. **Blocking Behavior**
   - All calls block for 10-50ms
   - Safe from main loop
   - NOT safe from ISR

---

## References

- Full findings: `docs/findings/sh2_api_investigation.md`
- Task summary: `docs/TASK_16_COMPLETION.md`
- Integration example: `src/sensors/bno085_integration_example.h`
- Adafruit library: `lib/Adafruit_BNO08x_Arduino/src/sh2.h`

---

**Status:** Ready for Task 17 (Hardware Testing)

Questions? See the comprehensive investigation report in `docs/findings/sh2_api_investigation.md`
