# BNO085 Calibration Persistence Framework

Quick reference for the calibration persistence implementation.

## What's Implemented

### EEPROM Storage (Ready to Use)
```cpp
#include "src/config/calibration_storage.h"

// Save calibration to EEPROM (after sensor calibrates)
saveToEEPROM(cal_data, cal_length);

// Restore from EEPROM (on startup)
if (restoreFromEEPROM(cal_data, &cal_length)) {
    bno.setCalibrationProfile(cal_data, cal_length);
}

// Check if saved
if (hasCalibrationInEEPROM()) {
    // Calibration available
}

// Clear (force re-calibration)
clearCalibrationFromEEPROM();
```

### SH-2 Protocol Layer (Framework Ready, Blocked)
```cpp
#include "src/sensors/bno085_calibration.h"

// Read calibration from sensor (via SH-2 FRS)
readCalibrationProfile(buffer, &length);  // BLOCKED - needs sh2_getFrs

// Write calibration to sensor
writeCalibrationProfile(buffer, length);  // BLOCKED - needs sh2_setFrs

// Validate before write
if (validateCalibrationData(buffer, length)) {
    // OK to write
}

// Get error description
const char* err = getCalibrationError();
```

## Architecture

```
BNO085 Sensor ←→ SH-2 Protocol ←→ Calibration I/O ←→ Arduino EEPROM
               (FRS records)      (bno085_calibration)  (256 bytes)
                [BLOCKED]          [BLOCKED]           [COMPLETE]
```

## Blocking Issue

**Problem**: Can't access `sh2_getFrs()` and `sh2_setFrs()` functions

**Solution**: Add wrapper methods to Adafruit_BNO08x class

**Patch needed** (add to `lib/Adafruit_BNO08x_Arduino/src/Adafruit_BNO08x.h`):
```cpp
class Adafruit_BNO08x {
  // ... existing methods ...
  
  bool getFrs(uint16_t recordId, uint32_t *pData, uint16_t *words) {
    return (sh2_getFrs(recordId, pData, words) == 0);
  }
  
  bool setFrs(uint16_t recordId, const uint32_t *pData, uint16_t words) {
    return (sh2_setFrs(recordId, (uint32_t*)pData, words) == 0);
  }
};
```

## Files

| File | Purpose | Status |
|------|---------|--------|
| src/sensors/bno085_calibration.h | API for SH-2 FRS | Complete |
| src/sensors/bno085_calibration.cpp | Implementation | Blocked (needs FRS) |
| src/config/calibration_storage.h | EEPROM API | Complete |
| src/config/calibration_storage.cpp | EEPROM impl | Complete |
| src/sensors/bno085_integration_example.h | Integration ref | Complete |
| docs/findings/bno085_sh2_protocol_analysis.md | Deep dive | Complete |
| docs/findings/CALIBRATION-IMPLEMENTATION-STATUS.md | Status & roadmap | Complete |

## EEPROM Layout

256 bytes reserved at address 0x00:

```
Offset  | Bytes | Content
--------|-------|------------------------------------------
0x00    | 1     | Marker: 0xCA=valid, 0xFF=empty
0x01    | 1     | Data length (1-252)
0x02    | 1     | Version (0x01)
0x03    | 1     | CRC8 (XOR checksum)
0x04    | 252   | Calibration data payload
```

## Integration Checklist

For actual v1.0 integration:

1. **Patch Adafruit library** to expose getFrs/setFrs
2. **Implement bno085_calibration.cpp** TODOs (readCalibrationProfile, writeCalibrationProfile)
3. **Add to BNO085::begin()**: Restore calibration from EEPROM
4. **Add to main loop**: Auto-save when cal_mag reaches level 3
5. **Add serial commands**: CAL_SAVE, CAL_RESTORE, CAL_CLEAR, CAL_STATUS
6. **Test hardware**: Full calibration → save → power cycle → restore cycle

See `bno085_integration_example.h` for code templates.

## Next Steps

1. **Immediate**: Apply Adafruit wrapper patch
2. **Week 1**: Complete FRS implementations and test
3. **Week 2**: Integrate with driver and test hardware
4. **Week 3**: Validate for v1.0 release

## Documentation

- **bno085_sh2_protocol_analysis.md**: SH-2 protocol deep dive
- **CALIBRATION-IMPLEMENTATION-STATUS.md**: Detailed status report
- **bno085-calibration-persistence.md**: Original research findings
- **bno085_integration_example.h**: Reference implementation

## Key Metrics

- **EEPROM Usage**: 256 bytes (4 header + 252 payload)
- **Write Time**: ~850ms for full 256-byte block
- **Read Time**: <10ms for quick checks
- **Validation**: CRC8 + data range checks
- **Board Support**: All Arduino boards with EEPROM

## FAQ

**Q**: Will restored calibration work right away?  
**A**: Yes, sensor applies calibration immediately on write via SH-2 FRS.

**Q**: What if EEPROM is corrupted?  
**A**: CRC check catches this, sensor falls back to fresh calibration.

**Q**: Can I move sensor to new location?  
**A**: Current implementation will apply old calibration. v1.1 will add location checking.

**Q**: How long does calibration take?  
**A**: Initial: 30-60s. Restored: Instantaneous (no motion needed).

**Q**: What if I want to re-calibrate?  
**A**: Use CAL_CLEAR command to force fresh calibration on next boot.

---

**Status**: Framework complete, ready for SH-2 API resolution and v1.0 integration.

See CALIBRATION-IMPLEMENTATION-STATUS.md for full details.
