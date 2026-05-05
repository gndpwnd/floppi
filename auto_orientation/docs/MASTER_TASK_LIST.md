# Auto Orientation v1.0: Master Task List

**Status**: Development Phase - 60% Complete  
**Target Release**: May 2026  
**Last Updated**: 2026-05-05

---

## Overview

21 critical tasks to complete v1.0 (BNO085 + independent GPS support).

**Key Principle**: Sensors are INDEPENDENT. Orientation (BNO085) and Position (GPS) run separately, merged only at output layer if needed.

---

## PHASE 1: Hardware Testing & Validation (4 tasks)

### Task 1: Test BNO085 Hardware & Upload Firmware ⚙️
**Status**: Ready (firmware compiled successfully)  
**Deliverable**: Live quaternion output from Arduino Mega on /dev/ttyACM0  
**Steps**:
- Upload compiled firmware to Mega
- Verify boot message ("=== Auto Orientation System ===")
- Collect 30 seconds of quaternion output (should be ~10 Hz)
- Verify quaternion magnitude ≈ 1.0 (not 0, not invalid)
- Document calibration progression (0→3)

**Success**: 10+ quaternion samples showing sensible values

---

### Task 2: Test GPS Hardware & NMEA Lock ⚙️
**Status**: GPS USB detected on /dev/ttyACM1, warming up  
**Deliverable**: Live GPGGA/GPRMC sentences from NEO-M9N  
**Steps**:
- Wait for GPS to acquire lock (may take 30-60 sec in open sky)
- Capture NMEA sentences using serial_monitor.py
- Verify fix quality (should show 1 = GPS fix or 2 = DGPS)
- Verify satellite count (expect 8+ in good conditions)
- Verify HDOP (should be < 2.0)
- Record actual lat/lon from clear sky location for accuracy validation

**Success**: 20+ NMEA sentences with valid fix and position

---

### Task 3: Create Hardware Validation Test Report 📋
**Status**: Blocked on Task 1 & 2  
**Deliverable**: docs/findings/hardware_validation_report.md  
**Content**:
- BNO085 test results (calibration progression, quaternion stability)
- GPS test results (fix time, accuracy, satellite count)
- NMEA parsing verification
- Wiring diagrams (Mermaid)
- Pin configuration confirmation

---

### Task 4: Create System Architecture Diagram 📊
**Status**: Design phase  
**Deliverable**: docs/SYSTEM_ARCHITECTURE.md (with Mermaid diagram)  
**Content**:
- Block diagram: BNO085 → UART → Mega, GPS → USB → Mega
- Data flow: sensor read → parse → output
- Serial output format (JSON + delimited options)
- Independent sensor streams concept

---

## PHASE 2: Output & Data Format (5 tasks)

### Task 5: Define & Implement JSON Output Format 📝
**Status**: Not started  
**Deliverable**: src/output/json_output.cpp  
**Format**:
```json
{
  "timestamp_ms": 123456,
  "orientation": {
    "valid": true,
    "quaternion": {"w": 0.707, "x": 0.0, "y": 0.0, "z": 0.707},
    "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 2}
  },
  "position": {
    "valid": false,
    "latitude": null,
    "longitude": null,
    "altitude_m": null,
    "accuracy_m": null,
    "satellites": 0,
    "fix_quality": 0
  }
}
```

---

### Task 6: Implement CSV Data Logger 📊
**Status**: Not started  
**Deliverable**: src/output/csv_logger.cpp  
**Format**:
```
timestamp_ms,quat_w,quat_x,quat_y,quat_z,cal_status,lat,lon,alt_m,accuracy_m,num_sats
123456,0.707,0.0,0.0,0.707,3,,,,,0
```

---

### Task 7: Create Combined Output Interface 🔗
**Status**: Not started  
**Deliverable**: src/output/sensor_output.cpp  
**Features**:
- Multiplexes orientation + position data
- Formats as JSON or CSV based on user selection
- Handles independent sensor timing (BNO085 @ 10 Hz, GPS @ 1 Hz)
- Buffering strategy to avoid dropping data

---

### Task 8: Implement Real-Time Serial Monitor (Python) 🖥️
**Status**: Partially done (serial_monitor.py exists)  
**Deliverable**: tools/real_time_monitor.py (enhanced)  
**Features**:
- Parse JSON output
- Display: orientation (roll/pitch/yaw), position, calibration status
- Color-coded fix quality
- Real-time calibration progress bar

---

### Task 9: Create Data Logging & Analysis Tools 📈
**Status**: Not started  
**Deliverable**: tools/analyze_orientation_data.py  
**Features**:
- Load CSV from auto_orientation output
- Plot quaternion stability over time
- Detect calibration convergence
- Export to matplotlib/graphs

---

## PHASE 3: Documentation & Guides (6 tasks)

### Task 10: Write Hardware Hookup Guide 🔌
**Status**: Not started  
**Deliverable**: docs/guides/HARDWARE_SETUP.md  
**Content** (with Mermaid diagrams):
- BNO085 UART wiring (Mega Serial1: RX1=19, TX1=18)
- P1 pin configuration (must be HIGH/5V for UART mode)
- GPS USB connection
- Power requirements
- Troubleshooting (No data? Check P1 pin, UART baud rate, etc.)

---

### Task 11: Write Calibration Guide 🧭
**Status**: Not started  
**Deliverable**: docs/guides/CALIBRATION_GUIDE.md  
**Content**:
- Manual calibration procedure (figure-8 motion, etc.)
- Auto-saving to EEPROM on Mega
- Validating calibration status
- Recalibrating for new location
- Expected calibration time (30-120 seconds)

---

### Task 12: Write API Reference 📚
**Status**: Partially done (in source code)  
**Deliverable**: docs/reference/API_REFERENCE.md  
**Content**:
- BNO085 class API (begin, read, getOrientation, etc.)
- NEOM9N class API (begin, read, getPosition, etc.)
- Output formatter API (JSON, CSV)
- Main.cpp example usage

---

### Task 13: Write Developer Guide for Adding Sensors 🛠️
**Status**: Not started  
**Deliverable**: docs/guides/ADDING_NEW_SENSORS.md  
**Content**:
- Extend sensor_base.h interface
- Implement new sensor driver (template)
- Example: Adding MPU6050 (v1.1)
- Testing checklist for new sensors

---

### Task 14: Create Troubleshooting Guide ⚠️
**Status**: Not started  
**Deliverable**: docs/guides/TROUBLESHOOTING.md  
**Issues**:
- "BNO085 FAILED" → check UART pins, P1 voltage
- "No GPS fix" → check USB cable, antenna, clear sky
- "Serial garbage" → wrong baud rate
- "Calibration stuck at 0" → magnetometer interference

---

### Task 15: Create System Architecture Documentation 📋
**Status**: Partially done  
**Deliverable**: docs/ARCHITECTURE.md (with Mermaid)  
**Content**:
- Sensor independence principle
- Data flow diagram
- Code organization (src/, lib/, tools/)
- Integration points

---

## PHASE 4: Calibration Persistence (3 tasks)

### Task 16: Complete SH-2 Calibration Save Implementation 💾
**Status**: Framework done, API incomplete  
**Deliverable**: src/sensors/bno085_calibration.cpp  
**Blocker**: Need sh2_getFrs/sh2_setFrs from Adafruit library  
**Steps**:
- Investigate Adafruit sh2.h for FRS API
- Implement readCalibrationProfile()
- Implement writeCalibrationProfile()
- Integrate with EEPROM storage

---

### Task 17: Test Calibration Persistence (Power Cycles) 🔄
**Status**: Not started  
**Deliverable**: Hardware test + docs/findings/calibration_persistence_test.md  
**Test Procedure**:
- Calibrate BNO085 fully (cal_status = 3)
- Save to EEPROM
- Power cycle 5x
- Verify calibration restored (no re-calibration needed)

---

### Task 18: Validate Calibration for Multiple Locations 🌍
**Status**: Not started  
**Deliverable**: docs/findings/multi_location_calibration_test.md  
**Test Procedure**:
- Calibrate in Location A (e.g., Austin)
- Save & verify
- Move to Location B (e.g., different city, different magnetic declination)
- Attempt restore → should fail or show warning
- Verify user can force re-calibration

---

## PHASE 5: Testing & Validation (2 tasks)

### Task 19: Create Integration Test Suite 🧪
**Status**: Not started  
**Deliverable**: tests/integration_tests.cpp  
**Tests**:
- BNO085 initialization
- GPS NMEA parsing
- Combined data output
- EEPROM save/restore
- Error handling

---

### Task 20: Compile & Test v1.0 Final Build ✅
**Status**: Not started  
**Deliverable**: Successful "platformio run --target build" with no errors  
**Checklist**:
- [ ] All code compiles
- [ ] Firmware size < 90% of Mega flash
- [ ] RAM usage < 80% of Mega SRAM
- [ ] All integration tests pass
- [ ] Hardware tests pass (Tasks 1-2)
- [ ] Documentation complete (Tasks 10-15)

---

### Task 21: Create v1.0 Release Checklist ✅
**Status**: Not started  
**Deliverable**: docs/V1_0_RELEASE_CHECKLIST.md  
**Content**:
- Code complete & tested
- Documentation complete
- Hardware validated
- No critical bugs
- Git tagged as v1.0

---

## Summary Table

| # | Task | Status | Priority | Dependencies |
|---|------|--------|----------|--------------|
| 1 | BNO085 hardware test | 🔄 Ready | 🔴 Critical | None |
| 2 | GPS hardware test | 🔄 Ready | 🔴 Critical | None |
| 3 | Hardware validation report | ⏳ Blocked | 🔴 High | 1,2 |
| 4 | System architecture diagram | 📝 Design | 🟡 Medium | None |
| 5 | JSON output format | ⏳ Not started | 🔴 Critical | None |
| 6 | CSV logger | ⏳ Not started | 🔴 Critical | None |
| 7 | Combined output interface | ⏳ Not started | 🔴 Critical | 5,6 |
| 8 | Real-time monitor (Python) | 📝 Design | 🟡 Medium | 5,7 |
| 9 | Data analysis tools | ⏳ Not started | 🟢 Low | 6 |
| 10 | Hardware hookup guide | 📝 Design | 🔴 High | 1,2 |
| 11 | Calibration guide | 📝 Design | 🔴 High | 16 |
| 12 | API reference | 📝 Design | 🟡 Medium | None |
| 13 | Developer guide | 📝 Design | 🟡 Medium | None |
| 14 | Troubleshooting guide | 📝 Design | 🟡 Medium | 10 |
| 15 | Architecture documentation | 📝 Partial | 🟡 Medium | 4 |
| 16 | SH-2 calibration save | 🔄 Framework | 🔴 Critical | None |
| 17 | Calibration persistence test | ⏳ Blocked | 🔴 Critical | 16 |
| 18 | Multi-location validation | ⏳ Blocked | 🟡 High | 16 |
| 19 | Integration test suite | ⏳ Not started | 🟡 High | 1,2,5,6,16 |
| 20 | Final build compilation | ⏳ Blocked | 🔴 Critical | 1,2,5,6,16 |
| 21 | v1.0 release checklist | ⏳ Not started | 🔴 Critical | All |

---

## Parallel Work Strategy

**Can run in parallel** (independent):
- Task 1, 2 (hardware tests)
- Task 4, 5, 6 (output formats)
- Task 10, 11, 12, 13, 14, 15 (documentation)
- Task 16 (calibration research)

**Must wait for**:
- Task 3 waits for Tasks 1,2
- Task 7 waits for Tasks 5,6
- Task 8 waits for Task 7
- Task 17, 18 wait for Task 16
- Task 20 waits for all code tasks (1,5,6,16)

---

## Resource Allocation

**Recommended agent allocation** (10 agents):
1. BNO085 hardware test (Task 1)
2. GPS hardware test (Task 2)
3. JSON output + CSV logger (Tasks 5, 6)
4. Combined output interface (Task 7)
5. Python real-time monitor (Task 8)
6. Hardware hookup guide + diagrams (Task 10)
7. Calibration guide (Task 11)
8. API reference + Developer guide (Tasks 12, 13)
9. SH-2 calibration implementation (Task 16)
10. Architecture docs + diagrams (Tasks 4, 15)

---
