# BNO085 Calibration Persistence: Complete Research Index

**Research Status**: ✅ COMPLETE  
**Date**: 2026-05-05  
**Researcher**: Claude Code  

---

## Overview

This directory contains comprehensive research on BNO085 magnetometer calibration persistence for the auto_orientation toolkit. The BNO085 sensor can save and restore calibration profiles using the SH-2 protocol, allowing calibration to persist across power cycles.

**Key Finding**: The Adafruit BNO08x library **does NOT** expose calibration save/restore methods. This functionality must be implemented via direct SH-2 protocol access.

---

## Document Guide

### 1. **bno085-calibration-persistence.md** (Primary Research)
   - **Length**: ~15KB
   - **Audience**: Decision makers, architects
   - **Content**:
     - Problem statement and motivation
     - Research findings (Adafruit API, BNO085 architecture, implementation options)
     - Three implementation approaches (SH-2 protocol, Arduino EEPROM, SD card)
     - Calibration lifecycle and failure modes
     - Code integration points in existing codebase
     - Testing strategy
     - Recommendations (DO/DON'T/CONSIDER)
   - **Use this when**: You need to understand the problem, options, and trade-offs
   - **Key sections**:
     - Section 3: BNO085 Calibration Data Architecture
     - Section 4: Implementation Approaches (with pros/cons)
     - Section 7: Code Integration Points (points to existing BNO085.h/sensor_base.h)

### 2. **sh2-protocol-reference.md** (Technical Specification)
   - **Length**: ~12KB
   - **Audience**: Firmware engineers implementing the protocol
   - **Content**:
     - Complete SH-2 packet structure and format
     - CRC-16 calculation algorithm (CCITT, polynomial 0x1021)
     - Feature 0xFE (Calibration Profile) GET/SET commands
     - I2C vs UART communication differences
     - Complete pseudo-code examples for reading/writing calibration
     - Debugging checklist and references
   - **Use this when**: You're writing the actual SH-2 protocol layer
   - **Key sections**:
     - Packet Structure (byte-by-byte breakdown)
     - Calibration Data Access (Feature 0xFE details)
     - CRC-16 Calculation (with implementation)
     - Implementation Example (pseudo-code for GET_FEATURE and SET_FEATURE)

### 3. **calibration-implementation-guide.md** (Developer Roadmap)
   - **Length**: ~24KB
   - **Audience**: Developers implementing the feature
   - **Content**:
     - Recommended file structure and architecture
     - Four implementation phases with priority levels
     - Complete working code examples:
       - SH-2 protocol wrapper (Phase 1)
       - BNO085 calibration functions (Phase 2)
       - Persistent storage (EEPROM) layer (Phase 3)
       - HAL implementation updates (Phase 4)
     - Testing strategy (unit tests + integration tests)
     - Serial command interface for debugging
     - Troubleshooting checklist
     - Future enhancement suggestions
   - **Use this when**: You're ready to implement the feature
   - **Key sections**:
     - Phase 1: SH-2 Protocol Layer (src/sensors/sh2_protocol.h)
     - Phase 2: BNO085 Calibration (src/sensors/bno085_calibration.h)
     - Phase 3: Persistent Storage (src/config/calibration_storage.h)
     - Phase 4: HAL Integration (update src/sensors/bno085.cpp)

---

## Quick Start

### For Architects / Decision Makers
1. Read: **bno085-calibration-persistence.md** (Section 1-4)
2. Review: **Recommendations** section for trade-offs
3. Decision: Choose Option A (SH-2), B (EEPROM only), or C (SD card)

### For Firmware Engineers
1. Read: **sh2-protocol-reference.md** (complete)
2. Study: CRC-16 calculation and packet structure
3. Reference: Implementation examples for GET_FEATURE and SET_FEATURE
4. Debug: Use checklist at end

### For Implementation Developers
1. Read: **calibration-implementation-guide.md** (complete)
2. Follow: Phase 1-4 implementation checklist
3. Code: Copy provided C++ examples and adapt
4. Test: Use provided unit test and integration test templates
5. Integrate: Update src/sensors/bno085.cpp with HAL methods

---

## Key Findings Summary

### What We Learned

1. **Adafruit Library Limitation**
   - The Adafruit BNO08x library does NOT provide save/restore calibration methods
   - This is a documented limitation across multiple GitHub issues and forums
   - Solution: Implement direct SH-2 protocol access

2. **BNO085 Architecture**
   - Calibration stored in BNO085's internal flash memory
   - Includes: accelerometer, gyroscope, magnetometer calibration parameters
   - Accessible via SH-2 protocol Feature 0xFE (Calibration Profile)
   - ~70-80 bytes of raw calibration data

3. **Best Implementation Path**
   - Use SH-2 protocol GET_FEATURE_REQUEST (read) and SET_FEATURE_REQUEST (write)
   - Store in Arduino EEPROM (256 bytes available on Mega)
   - Restore on boot: read from EEPROM → write to BNO085
   - Validate: CRC check, size validation, sanity checks

4. **Failure Modes**
   - Invalid calibration data → BNO085 ignores, uses defaults
   - Cross-location restoration → Magnetic declination mismatch
   - Partial write → Sensor fusion errors
   - Mitigation: Metadata (timestamp, location), CRC validation, user warnings

### Recommended Implementation

**Option A (Chosen)**: SH-2 Protocol + Arduino EEPROM
- Read calibration from BNO085 via SH-2 GET_FEATURE
- Store in Arduino EEPROM with metadata (timestamp, location)
- On boot: restore from EEPROM via SH-2 SET_FEATURE
- Estimated effort: 200-300 lines of code
- Works with all Arduino platforms (Nano, Mega, Teensy, ESP32)

---

## Code Integration Points

Existing code in `/home/devel/floppi/auto_orientation/`:

### Already in Place
- `src/sensors/sensor_base.h`: Abstract interface
  - `virtual bool setCalibrationProfile(const uint8_t* data, uint16_t len)`
  - `virtual bool getCalibrationProfile(uint8_t* data, uint16_t* len)`
  
- `src/sensors/bno085.h`: Sensor class stub
  - Has calibration data buffer: `uint8_t calibration_data_[256];`
  - Declares methods but NOT implemented

- `src/sensors/sensor_base.h`: OrientationData structure
  - Already captures calibration status: `cal_status`, `cal_accel`, `cal_gyro`, `cal_mag`

### To Create
1. `src/sensors/sh2_protocol.h` - SH-2 protocol wrapper
2. `src/sensors/bno085_calibration.h` - BNO085 calibration functions
3. `src/config/calibration_storage.h` - EEPROM abstraction
4. Update `src/sensors/bno085.cpp` - Implement HAL methods

---

## Testing Plan

### Unit Tests (PlatformIO TEST_FRAMEWORK)
- CRC-16 calculation verification
- Calibration data validation
- EEPROM save/load cycle
- Packet structure parsing

### Integration Tests (Hardware)
- Boot → read calibration from BNO085
- Calibrate → save → restore → verify
- Cross-location test (check declination)
- Corrupted data handling

### Success Criteria (v1.0)
- [x] Understand Adafruit API limitations
- [ ] Implement SH-2 protocol wrapper
- [ ] Read calibration from BNO085
- [ ] Store to Arduino EEPROM
- [ ] Restore on boot
- [ ] Power cycle test (calibration persists)
- [ ] Documentation + code examples

---

## Decision Matrix

| Approach | Effort | Complexity | Compatibility | Persistence | Recommended |
|----------|--------|-----------|---------------|-------------|-------------|
| A: SH-2 + EEPROM | 200 lines | Medium | All platforms | ✅ This boot | ⭐ YES |
| B: EEPROM only | 100 lines | Low | All platforms | ❌ Not to BNO | — |
| C: SD card | 300 lines | High | ESP32, boards with SD | ✅ Full | v1.1 future |

**Recommendation**: Implement Option A for v1.0 (SH-2 + EEPROM). Add Option C (SD card multi-profile) in v1.1.

---

## Timeline Estimate

- **Phase 1 (SH-2 protocol)**: 2-3 hours
- **Phase 2 (BNO calibration)**: 1-2 hours
- **Phase 3 (EEPROM storage)**: 1 hour
- **Phase 4 (HAL integration)**: 1 hour
- **Testing & debugging**: 3-4 hours
- **Documentation & examples**: 1-2 hours

**Total**: ~9-13 hours of focused development

---

## Files to Modify/Create

### New Files to Create
```
src/sensors/sh2_protocol.h                  (100 lines)
src/sensors/bno085_calibration.h            (150 lines)
src/config/calibration_storage.h            (100 lines)
tests/test_calibration.cpp                  (50 lines, optional)
```

### Files to Modify
```
src/sensors/bno085.cpp                      (update implementation)
src/sensors/bno085.h                        (no changes needed)
src/sensors/sensor_base.h                   (no changes needed)
src/main.cpp                                (add begin() call for BNO)
```

---

## Known Limitations & Trade-offs

### Limitations
1. **Cross-location recalibration required**: Moving >100 miles requires re-calibration due to magnetic declination
2. **EEPROM capacity**: Arduino Mega has only 4KB EEPROM (room for ~1 profile)
3. **No built-in versioning**: Firmware updates may change calibration data format
4. **No location awareness**: BNO085 doesn't know its physical location

### Trade-offs
- **SH-2 protocol complexity** (necessary evil for direct hardware access)
- **CRC overhead** (small, but ensures data integrity)
- **EEPROM wear** (each save = 1 write cycle; acceptable for occasional writes)
- **No multi-profile support in v1.0** (defer to v1.1 with SD card)

---

## References & Further Reading

### Datasheets (If Available)
- Bosch BNO085 Datasheet: https://www.bosch-sensortec.com/
- SH-2 Protocol Specification: (Available from Bosch, request via email)

### Adafruit Resources
- Product Page: https://www.adafruit.com/product/4754
- Hookup Guide: https://learn.adafruit.com/bno085-absolute-orientation-sensor-with-calibration
- Library Source: https://github.com/adafruit/Adafruit_BNO08x_Arduino

### Project References
- Original request from MDC: `docs/archive/compere_init.md`
- Example sketch: `docs/archive/BN085_I2C_Adafruit.ino`
- Roadmap: `docs/roadmap.md` (v1.0 milestone)

---

## Next Steps

1. **Review & Approve** this research (decision on Option A, B, or C)
2. **Start Phase 1**: Create `src/sensors/sh2_protocol.h` with CRC and send/receive functions
3. **Create tests**: Unit tests for CRC and packet structure
4. **Progress tracking**: Update `docs/todo.md` with implementation tasks

---

## Contact & Questions

For questions about this research:
- Review the specific document (research, protocol, or implementation guide)
- Check the troubleshooting checklists
- Refer to code examples and pseudo-code
- Test with hardware if possible

Research completed: **2026-05-05**
