# Phase 1: Master Implementation Plan

**Date**: 2026-05-07  
**Scope**: Complete Phase 1 from research to working code  
**Target Duration**: 1-2 weeks continuous work  
**Status**: Ready for immediate execution

---

## Scope: What Will Be Completed

### Primary Deliverables

1. **Quaternion Math Library** (`src/math/quaternion.h/cpp`)
   - Quaternion class with w, x, y, z components
   - Operations: multiply, normalize, conjugate, magnitude
   - Conversions: quaternion ↔ rotation matrix ↔ Euler angles
   - Validation and error checking

2. **Coordinate Frame Conversions** (`src/math/coordinates.h/cpp`)
   - WGS84 ↔ ECEF conversions
   - ECEF ↔ NED conversions
   - Magnetic declination application
   - Constants and helper functions

3. **Magnetic Declination Module** (`src/math/magnetic_declination.h/cpp`)
   - Get declination for lat/lon/year
   - Apply heading correction (magnetic → true north)

4. **BNO085 Extension** (extend `src/sensors/bno085.cpp`)
   - Extract and expose rotation matrix
   - Extract and expose Euler angles
   - Validation and error checks

5. **Snapshot/Logging Feature** (NEW - with build flag)
   - `src/features/snapshot_recorder.h/cpp` (only if SNAPSHOT_MODE enabled)
   - Record quaternion + timestamp + calibration level to SD card
   - Multiple file support with rotating filenames
   - JSON format for easy parsing
   - Button trigger via GPIO

6. **Configuration Updates** (`platformio.ini` + `src/config/mode.h`)
   - Add `SNAPSHOT_MODE` build flag
   - New build environment: `arduino_mega_snapshot`
   - Memory optimization through conditional compilation

### Test Coverage

7. **Unit Tests** (`tests/test_*.cpp`)
   - `test_quaternion.cpp` - all quaternion operations
   - `test_coordinates.cpp` - all coordinate conversions
   - `test_snapshot_recorder.cpp` - logging functionality
   - `test_bno085_extensions.cpp` - BNO085 integration

8. **Integration Tests**
   - Full math pipeline: BNO085 → matrix → local frame
   - Snapshot recording end-to-end
   - File system validation

### Documentation

9. **Code Documentation**
   - Header comments in all files
   - Function documentation with examples
   - Theory references to research docs

10. **User Guide**
    - How to use snapshot feature
    - Build flag options
    - Field testing procedures

---

## File Structure (Complete)

```
auto_orientation/
├── src/
│   ├── main.cpp (existing, minor updates)
│   ├── config/
│   │   ├── mode.h (add SNAPSHOT_MODE)
│   │   └── calibration_storage.h (existing)
│   ├── math/                          ← NEW
│   │   ├── quaternion.h
│   │   ├── quaternion.cpp
│   │   ├── coordinates.h
│   │   ├── coordinates.cpp
│   │   ├── magnetic_declination.h
│   │   └── magnetic_declination.cpp
│   ├── features/                      ← NEW
│   │   ├── snapshot_recorder.h
│   │   └── snapshot_recorder.cpp
│   ├── sensors/
│   │   ├── bno085.h/cpp (extend for matrix/Euler output)
│   │   └── button_input.h/cpp (NEW, if SNAPSHOT_MODE)
│   ├── output/
│   │   └── sensor_output_manager.cpp (extend for snapshots)
│   └── file_system/                   ← NEW
│       ├── sd_card.h/cpp
│       └── file_manager.h/cpp
├── tests/
│   ├── test_quaternion.cpp            ← NEW
│   ├── test_coordinates.cpp           ← NEW
│   ├── test_magnetic_declination.cpp  ← NEW
│   ├── test_snapshot_recorder.cpp     ← NEW
│   ├── test_bno085_extensions.cpp     ← NEW
│   ├── integration_test_math_pipeline.cpp ← NEW
│   └── integration_test_snapshot.cpp  ← NEW
├── docs/
│   ├── PHASE_1_MASTER_IMPLEMENTATION_PLAN.md (this file)
│   ├── MATH_AND_APPLICATIONS_MASTER_GUIDE.md (reference)
│   ├── IMPLEMENTATION_CHECKLIST.md (reference)
│   ├── QUATERNION_API_REFERENCE.md (NEW)
│   ├── COORDINATE_CONVERSION_API.md (NEW)
│   ├── SNAPSHOT_FEATURE_GUIDE.md (NEW)
│   └── PHASE_1_TEST_RESULTS.md (NEW, filled during execution)
└── platformio.ini (add snapshot build environment)
```

---

## Implementation Order (Dependency Graph)

```
Start (Day 1)
├─ Branch 1: Math Library (in parallel)
│  ├─ Quaternion class [BLOCKING for: conversions, tests, BNO085]
│  ├─ Coordinates module [BLOCKING for: tests, integration]
│  ├─ Magnetic declination [BLOCKING for: field testing]
│  └─ Unit tests [validates all above]
│
├─ Branch 2: BNO085 Extension (after quaternion done)
│  ├─ Extract rotation matrix
│  ├─ Extract Euler angles
│  └─ Unit tests
│
└─ Branch 3: Snapshot Feature (in parallel, independent)
   ├─ SD card module [BLOCKING for: snapshot recorder]
   ├─ Snapshot recorder [uses SD card]
   ├─ Button input handling
   ├─ JSON serialization
   └─ Unit tests

Integration Tests (Day 10+)
└─ Full pipeline: BNO085 → quaternion → matrix → snapshot

Field Testing (Day 14+)
└─ Manual validation with actual hardware
```

---

## Build Configurations

### platformio.ini Environments

```ini
[env:arduino_mega_calibration]
# Existing - verbose debug output

[env:arduino_mega_production]
# Existing - minimal output

[env:arduino_mega_snapshot]
# NEW - calibration + snapshot recording
build_flags = -D CALIBRATION_MODE -D SNAPSHOT_MODE

[env:arduino_mega_snapshot_only]
# NEW - minimal output + snapshot recording
build_flags = -D SNAPSHOT_MODE
```

### Conditional Compilation

In `src/config/mode.h`:

```cpp
#ifdef SNAPSHOT_MODE
  #define ENABLE_SNAPSHOT_RECORDER 1
  #define SNAPSHOT_BUFFER_SIZE 1024  // bytes
  #define MAX_SNAPSHOT_FILES 100
#else
  #define ENABLE_SNAPSHOT_RECORDER 0
#endif
```

In `src/features/snapshot_recorder.cpp`:

```cpp
#if ENABLE_SNAPSHOT_RECORDER
// Full implementation
#else
// Stub implementation (no-op)
#endif
```

---

## Detailed Task Breakdown

### Task Group 1: Quaternion Math Library

**Task 1.1: Quaternion Class Definition** (2-3 hours)
- [ ] Create `src/math/quaternion.h`
- [ ] Define Quaternion struct: w, x, y, z (float32)
- [ ] Constructor from components
- [ ] Equality operator
- [ ] Serialization (to/from array)

**Task 1.2: Quaternion Operations** (3-4 hours)
- [ ] `normalize()` - make unit quaternion
- [ ] `magnitude()` - return length (should be ~1.0)
- [ ] `conjugate()` - return (w, -x, -y, -z)
- [ ] `multiply(q1, q2)` - quaternion multiplication (non-commutative!)
- [ ] `rotate_vector(v)` - apply rotation to 3D vector: q * v * conj(q)

**Task 1.3: Quaternion to Rotation Matrix** (2-3 hours)
- [ ] Formula from QUATERNION_REFERENCE.md, section "Rotation Matrix Conversion"
- [ ] Output: 3×3 matrix (row-major or column-major, choose consistently)
- [ ] Validation: check orthonormality (R × R^T = I)

**Task 1.4: Rotation Matrix to Quaternion** (2-3 hours)
- [ ] Handle numerical stability (Shepperd's method or similar)
- [ ] Validate input is orthonormal before conversion
- [ ] Return normalized quaternion

**Task 1.5: Quaternion to Euler Angles** (2-3 hours)
- [ ] ZYX convention (yaw, pitch, roll)
- [ ] Return in both radians and degrees
- [ ] Note gimbal lock conditions (pitch ≈ ±90°)

**Task 1.6: Euler Angles to Quaternion** (2-3 hours)
- [ ] ZYX convention
- [ ] Accept both radians and degrees
- [ ] Formula: q = qz * qy * qx (order matters!)

**Task 1.7: Unit Tests for Quaternion** (3-4 hours)
- [ ] Test normalization
- [ ] Test multiplication: (q1*q2)*q3 = q1*(q2*q3)
- [ ] Test vector rotation commutativity with matrix multiply
- [ ] Test identity quaternion
- [ ] Test inverse
- [ ] Test round-trip conversions
- [ ] Test known angles (0°, 90°, 180°, etc.)
- [ ] Benchmark: measure computation time

**Subtotal**: 18-26 hours (≈ 2-3 days intensive)

---

### Task Group 2: Coordinate Frame Conversions

**Task 2.1: WGS84 Constants** (1 hour)
- [ ] Define WGS84 parameters (a, b, e², etc.)
- [ ] Earth radius calculations

**Task 2.2: GPS to ECEF** (2-3 hours)
- [ ] Formula from GPS_GEODETIC_COORDINATE_SYSTEMS.md
- [ ] Input: lat (°), lon (°), altitude (m)
- [ ] Output: (x, y, z) ECEF in meters
- [ ] Handle edge cases (poles, equator, antimeridian)

**Task 2.3: ECEF to GPS** (2-3 hours)
- [ ] Iterative solution (convert back from ECEF)
- [ ] Input: (x, y, z) in meters
- [ ] Output: lat (°), lon (°), altitude (m)

**Task 2.4: ECEF to NED** (2-3 hours)
- [ ] Compute rotation matrix from origin lat/lon
- [ ] Formula: involves sin(lat), cos(lat), sin(lon), cos(lon)
- [ ] Input: ECEF point, NED origin in ECEF, origin latitude
- [ ] Output: (north, east, down) in local tangent plane

**Task 2.5: NED to ECEF** (1-2 hours)
- [ ] Inverse of ECEF to NED
- [ ] Use transposed rotation matrix

**Task 2.6: Unit Tests for Coordinates** (3-4 hours)
- [ ] Round-trip GPS → ECEF → GPS
- [ ] Round-trip GPS → ECEF → NED → ECEF → GPS
- [ ] Known coordinates (Munich, LAX, Singapore, North Pole)
- [ ] Edge cases (on equator, on meridian)
- [ ] Precision: check error < 1 meter

**Subtotal**: 12-16 hours (≈ 1.5-2 days)

---

### Task Group 3: Magnetic Declination

**Task 3.1: Magnetic Declination Lookup** (2-3 hours)
- [ ] Use simplified IGRF model or 2024 lookup table
- [ ] Input: lat, lon, year
- [ ] Output: declination angle (degrees, can be negative)
- [ ] Pre-compute table for common locations

**Task 3.2: Apply Declination to Heading** (1-2 hours)
- [ ] Formula: true_heading = magnetic_heading + declination
- [ ] Handle wrap-around (0°-360°)
- [ ] Unit tests with known declinations

**Subtotal**: 3-5 hours (≈ 0.5 days)

---

### Task Group 4: BNO085 Extension

**Task 4.1: Extract Rotation Matrix** (2-3 hours)
- [ ] In `src/sensors/bno085.cpp`: add `get_rotation_matrix()` method
- [ ] Call quaternion_to_matrix internally
- [ ] Cache result if frequency demands it

**Task 4.2: Extract Euler Angles** (2-3 hours)
- [ ] In `src/sensors/bno085.cpp`: add `get_euler_angles()` method
- [ ] Call quaternion_to_euler internally
- [ ] Return as struct: {roll_deg, pitch_deg, yaw_deg}

**Task 4.3: Validation** (1-2 hours)
- [ ] Check quaternion magnitude (should be ≈ 1.0)
- [ ] Log warnings if magnitude deviates
- [ ] Unit tests

**Subtotal**: 5-8 hours (≈ 1 day)

---

### Task Group 5: Snapshot/Logging Feature

**Task 5.1: SD Card Module** (4-5 hours)
- [ ] Create `src/file_system/sd_card.h/cpp`
- [ ] Use Arduino SD library (or Adafruit SD library)
- [ ] Functions:
  - `initialize()` - init SD card
  - `is_ready()` - check if SD card present and ready
  - `create_file(filename)` - create new file
  - `append_line(filename, data)` - append JSON line
  - `close_file(filename)` - close file safely
- [ ] Error handling for missing SD card

**Task 5.2: Snapshot Recorder Module** (4-5 hours)
- [ ] Create `src/features/snapshot_recorder.h/cpp`
- [ ] Only compiled if SNAPSHOT_MODE enabled
- [ ] Functions:
  - `initialize()` - setup SD card and create snapshot directory
  - `record_snapshot(quaternion, calibration_level, timestamp)` - write to SD
  - `get_status()` - check if ready to record
- [ ] File naming: `snapshots/snap_001.json`, `snap_002.json`, etc.
- [ ] JSON format:
  ```json
  {
    "timestamp_ms": 1234567,
    "quaternion": {"w": 0.707, "x": 0, "y": 0, "z": 0.707},
    "euler": {"roll_deg": 0, "pitch_deg": 0, "yaw_deg": 90},
    "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 3}
  }
  ```

**Task 5.3: Button Input Handling** (2-3 hours)
- [ ] Create `src/sensors/button_input.h/cpp`
- [ ] Read GPIO pin for button press
- [ ] Debounce logic (20-50 ms)
- [ ] Detect rising edge (button press)
- [ ] Only if SNAPSHOT_MODE enabled

**Task 5.4: Integration in main.cpp** (2-3 hours)
- [ ] Initialize snapshot recorder in setup()
- [ ] In loop(): check button press → call snapshot recorder
- [ ] Conditional compilation: only if SNAPSHOT_MODE

**Task 5.5: Unit Tests** (2-3 hours)
- [ ] Mock SD card functions
- [ ] Test JSON serialization
- [ ] Test file rotation (after 100 files)
- [ ] Test with SNAPSHOT_MODE disabled

**Subtotal**: 14-19 hours (≈ 2 days)

---

### Task Group 6: Integration & Comprehensive Tests

**Task 6.1: Full Math Pipeline Test** (3-4 hours)
- [ ] Read BNO085 quaternion
- [ ] Compute rotation matrix
- [ ] Compute Euler angles
- [ ] Verify all outputs consistent
- [ ] Compare against reference values

**Task 6.2: Snapshot Recording Test** (2-3 hours)
- [ ] Press button, verify file created
- [ ] Verify JSON format correct
- [ ] Verify multiple snapshots create multiple files
- [ ] Verify data integrity

**Task 6.3: Benchmark & Profiling** (2-3 hours)
- [ ] Measure computation time for all math ops
- [ ] Measure SD card write latency
- [ ] Verify fits within budget (<5 ms per 100 Hz loop)

**Subtotal**: 7-10 hours (≈ 1 day)

---

### Task Group 7: Documentation

**Task 7.1: Code Comments** (2-3 hours)
- [ ] Add header comments to all files
- [ ] Add function documentation with examples
- [ ] Reference research documents

**Task 7.2: API Reference** (2-3 hours)
- [ ] Create `QUATERNION_API_REFERENCE.md`
- [ ] Create `COORDINATE_CONVERSION_API.md`
- [ ] Function signatures, examples, error codes

**Task 7.3: Snapshot Feature Guide** (2 hours)
- [ ] How to enable SNAPSHOT_MODE
- [ ] How to use snapshot feature
- [ ] How to read SD card files

**Task 7.4: Test Results Documentation** (1-2 hours)
- [ ] Create `PHASE_1_TEST_RESULTS.md`
- [ ] Record all test results
- [ ] Performance benchmarks
- [ ] Known limitations

**Subtotal**: 7-11 hours (≈ 1 day)

---

## Total Effort Estimate

| Task Group | Hours | Days | Notes |
|-----------|-------|------|-------|
| 1. Quaternion | 18-26 | 2-3 | Most complex, foundation for rest |
| 2. Coordinates | 12-16 | 1.5-2 | Math-intensive, many test cases |
| 3. Magnetic Declination | 3-5 | 0.5 | Straightforward |
| 4. BNO085 Extension | 5-8 | 1 | Depends on Task 1 |
| 5. Snapshot Feature | 14-19 | 2 | File I/O, build flags |
| 6. Integration Tests | 7-10 | 1 | Validation |
| 7. Documentation | 7-11 | 1 | Final polish |
| **TOTAL** | **66-95 hours** | **9-13 days** | **At 8 hours/day: 8-12 days** |

**With parallel agent work: 3-5 days (clock time)**

---

## Execution Strategy

### Day 1: Setup & Quaternion Core (8 hours)
- [ ] Create all file structure
- [ ] Implement quaternion class (Task 1.1-1.2)
- [ ] Start unit tests

### Day 2-3: Quaternion Conversions (16 hours)
- [ ] Implement conversions (Task 1.3-1.6)
- [ ] Complete unit tests (Task 1.7)
- [ ] Parallel: Coordinate module (Task 2.1-2.6)

### Day 4: BNO085 + Snapshot Setup (8 hours)
- [ ] Extend BNO085 (Task 4.1-4.3)
- [ ] Setup snapshot feature skeleton (Task 5.1-5.2)
- [ ] Parallel: Button input (Task 5.3)

### Day 5: Snapshot Implementation (8 hours)
- [ ] Complete snapshot recorder
- [ ] Integrate with main.cpp (Task 5.4)
- [ ] Unit tests (Task 5.5)

### Day 6: Integration & Testing (8 hours)
- [ ] Run full pipeline tests (Task 6.1-6.3)
- [ ] Fix any issues
- [ ] Benchmark

### Day 7: Documentation (8 hours)
- [ ] Write all code comments (Task 7.1)
- [ ] Create API references (Task 7.2-7.3)
- [ ] Document test results (Task 7.4)

---

## Success Criteria (Definition of Done)

✓ **Code Quality**
- All code compiles with `-Wall -Wextra -Werror`
- No dynamic memory allocation in real-time loops
- All public functions have unit tests
- Code follows Arduino/C++ standards

✓ **Quaternion Library**
- Round-trip conversion accurate to ±0.001° 
- Multiplication associative within floating-point precision
- Vector rotation matches matrix multiply
- 100+ test cases passing

✓ **Coordinate Conversions**
- GPS ↔ ECEF ↔ NED round-trip < 1 meter error
- Tests with 10+ known coordinates
- Handles edge cases (poles, antimeridian)

✓ **BNO085 Integration**
- Extract rotation matrix and Euler angles
- Results validate against hand calculations
- Integrated with existing calibration system

✓ **Snapshot Feature**
- Records quaternion + timestamp to SD card
- JSON format correct and parseable
- Button trigger works reliably
- Tested with SNAPSHOT_MODE on/off

✓ **Documentation**
- All files have header comments
- API references complete with examples
- Test results logged
- Theory references included

✓ **Performance**
- Math operations < 1 ms on Arduino Mega
- SD writes < 100 ms (non-blocking preferred)
- Fits in memory budget (RAM + Flash)

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Quaternion math complexity | Reference implementations provided (Python), unit tests first |
| Floating-point precision | Use double for intermediate calculations, validate results |
| SD card reliability | Implement soft error recovery, pre-allocate space |
| Memory constraints | Use `#define` constants, avoid dynamic allocation |
| Integration issues | Incremental integration, test after each feature |
| Build flag conflicts | Pre-compile test matrix (all combinations) |

---

## Quality Assurance Checklist

Before marking "DONE":

- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Code compiles without warnings
- [ ] No memory leaks (verify with valgrind on desktop if possible)
- [ ] Performance benchmarks recorded
- [ ] Documentation complete and reviewed
- [ ] Code committed with clear messages
- [ ] Snapshot feature works with both MODE on/off
- [ ] Field manual testing (if hardware available)

---

## Next Phase (After Phase 1 Complete)

- Phase 2: GPS integration (Week 2-3)
- Phase 3: Sensor fusion EKF (Week 3-4)
- Phase 4: Camera calibration (Week 4-5)
- Phase 5: Applications (Week 5-8)

But Phase 1 is self-contained and valuable standalone:
- Quaternion library usable by any IMU project
- Snapshot feature useful for data logging
- Tests and documentation valuable for learning

---

**Status**: Ready for immediate execution  
**Approval**: User approved 2026-05-07  
**Start Date**: 2026-05-07 (as soon as agents spawned)  
**Expected Completion**: 2026-05-14 (7 days with parallel work)

