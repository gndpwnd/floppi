# Auto Orientation Project - Folder Structure

This document explains the organization of the auto_orientation project (BNO085 IMU + GPS + EKF Fusion).

---

## Project Overview

**Auto Orientation** is a complete sensor fusion system for Arduino Mega that:
1. Reads absolute orientation from BNO085 IMU
2. Reads GPS position and validates with local coordinate frames
3. Fuses both sensors using Extended Kalman Filter (EKF) for smooth, accurate tracking

**Status**: Phase 3 Complete (all 143+ tests passing, 18.1% flash used)

---

## Directory Structure

```
auto_orientation/
├── platformio.ini              # Build system configuration
│                              # Defines 8 build environments
│                              # Specifies library dependencies
│
├── src/                        # Source code (organized in layers)
│   ├── main.cpp               # Main entry point, event loop
│   │
│   ├── config/                # Configuration layer
│   │   ├── pins.h            # Arduino pin assignments (I2C, SPI, UART)
│   │   ├── mode.h            # Build mode selection (CALIBRATION_MODE, etc)
│   │   ├── gps_config.h      # GPS tuning (baud rate, timeout)
│   │   └── ekf_config.h      # EKF parameters (Q, R, initial covariance)
│   │
│   ├── sensors/               # Hardware drivers
│   │   ├── sensor_base.h     # Abstract base classes (Sensor, OrientationSensor)
│   │   ├── bno085.h/cpp      # BNO085 IMU driver (quaternion output)
│   │   ├── gps.h/cpp         # GPS UART driver (NMEA parsing)
│   │   └── button_input.h    # Debounced button input
│   │
│   ├── math/                  # Pure math (no hardware dependencies)
│   │   ├── quaternion.h/cpp           # Quaternion algebra (multiply, rotate, normalize)
│   │   ├── quaternion_conversions.h   # Convert quaternion ↔ matrix ↔ Euler angles
│   │   ├── coordinates.h/cpp          # GPS ↔ ECEF ↔ NED conversions
│   │   └── magnetic_declination.h/cpp # Magnetic heading corrections
│   │
│   ├── navigation/            # State estimation and position tracking
│   │   ├── coordinate_frame.h/cpp     # Maintains local NED origin
│   │   ├── ekf.h/cpp                 # Extended Kalman Filter (16D state vector)
│   │   ├── state_dynamics.h/cpp       # IMU state transition, Jacobian F
│   │   ├── measurement_model.h/cpp    # GPS measurement function h, Jacobian H
│   │   └── covariance_manager.h/cpp   # Numerical stability (symmetry, eigenvalues)
│   │
│   ├── features/              # Optional features (conditional #ifdef)
│   │   ├── snapshot_recorder.h/cpp    # Record quaternion to SD card
│   │   └── snapshot_logger.h/cpp      # SD card logging
│   │
│   ├── output/                # Data formatting and serialization
│   │   └── sensor_output_manager.h/cpp # JSON formatting for all sensors
│   │
│   └── file_system/           # File I/O
│       ├── sd_card.h/cpp      # SD card wrapper
│       └── file_manager.h/cpp # File utilities
│
├── tests/                      # Test suite (143+ tests, all passing)
│   │
│   ├── Phase 1 Tests (Math Foundation):
│   ├── test_quaternion.cpp               # 26 tests
│   ├── test_coordinates.cpp              # 38 tests
│   ├── test_magnetic_declination.cpp     # 20 tests
│   ├── test_snapshot_recorder.cpp        # 16 tests
│   │
│   ├── Phase 2 Tests (GPS Integration):
│   ├── test_json_output.cpp              # 17 tests
│   ├── test_coordinate_frame.cpp         # 70 tests
│   ├── test_gps.cpp                      # 46 tests
│   ├── integration_test_gps_fusion.cpp   # 35+ tests
│   │
│   └── Phase 3 Tests (EKF Fusion):
│       ├── test_ekf.cpp                  # 26 tests
│       ├── test_state_dynamics.cpp       # 20 tests
│       ├── test_measurement_model.cpp    # 20 tests
│       ├── integration_test_ekf_full.cpp # 53 tests
│       └── scenario_test_ekf.cpp         # 15 real-world scenarios
│
├── docs/                      # Project documentation
│   ├── README.md              # Project overview and features
│   ├── QUICK_START.md         # 5-minute getting started guide
│   ├── BUILD_GUIDE.md         # How to compile and flash
│   ├── FOLDER_STRUCTURE.md    # This file (at root level too)
│   │
│   ├── PHASE_1_COMPLETION_SUMMARY.md    # Math library complete
│   ├── PHASE_2_COMPLETION_SUMMARY.md    # GPS integration complete
│   ├── PHASE_3_COMPLETION_SUMMARY.md    # EKF fusion complete
│   ├── PHASE_3_MASTER_IMPLEMENTATION_PLAN.md
│   │
│   ├── API References (detailed documentation):
│   ├── API_REFERENCE.md                    # Main API overview
│   ├── QUATERNION_API_REFERENCE.md         # Quaternion class API
│   ├── COORDINATE_CONVERSION_API.md        # Coordinate transform API
│   ├── EKF_API_REFERENCE.md                # EKF class API
│   ├── GPS_DRIVER_API_REFERENCE.md         # GPS driver API
│   ├── STATE_SPACE_MODEL.md                # Mathematical state space definition
│   │
│   ├── Theory and Tuning:
│   ├── EKF_THEORY.md                    # Extended Kalman Filter theory
│   ├── EKF_TUNING_GUIDE.md              # How to tune Q/R matrices
│   ├── GPS_COORDINATE_QUICK_REFERENCE.md
│   │
│   ├── Hardware and Setup:
│   ├── CALIBRATION_GUIDE.md             # User guide for BNO085 calibration
│   ├── CALIBRATION_IMPLEMENTATION_GUIDE.md
│   ├── GPS_HARDWARE_SETUP.md            # GPS module wiring
│   ├── GPS_TROUBLESHOOTING.md           # Common GPS issues
│   ├── COORDINATE_FRAME_API_REFERENCE.md
│   │
│   ├── archive/                        # Old documentation (for reference)
│   │   ├── QUATERNION_IMPLEMENTATION_SUMMARY.md
│   │   ├── GPS_DRIVER_IMPLEMENTATION_SUMMARY.md
│   │   ├── COORDINATE_FRAME_IMPLEMENTATION.md
│   │   ├── SESSION_SUMMARY_2026-05-06_FINAL.md
│   │   ├── SESSION_SUMMARY_2026-05-07_BNO_COMPLETE.md
│   │   ├── READY_TO_USE.md
│   │   ├── BNO085_EXTENSIONS_USAGE.md
│   │   └── IMPLEMENTATION_SUMMARY_*.md
│   │
│   └── findings/              # Research findings and notes
│       └── Various analysis documents
│
├── lib/                        # Third-party Arduino libraries
│   ├── Adafruit_BNO08x/       # BNO085 sensor library
│   ├── Adafruit_Sensor/       # Unified sensor interface
│   └── ...others...           # SD card, Wire, etc
│
├── tools/                      # Build scripts and utilities
│   └── build_tests.sh         # Compile and run all tests
│
├── .pio/                       # PlatformIO cache (auto-generated, not in git)
│   ├── build/                 # Compiled object files
│   ├── libdeps/               # Downloaded dependencies
│   └── ...

├── .gitignore                 # Git ignore rules (build artifacts, etc)
└── platformio.ini             # Build configuration (see details below)
```

---

## Build Environments (platformio.ini)

The project supports 8 different build configurations:

| Environment | Features | Use Case |
|-------------|----------|----------|
| `arduino_mega` | Base: BNO085 only | Development, baseline |
| `arduino_mega_calibration` | BNO085 + calibration mode (verbose) | Calibrating BNO085 |
| `arduino_mega_production` | BNO085 only, minimal output | Deployment |
| `arduino_mega_gps` | BNO085 + GPS at 9600 baud | GPS integration |
| `arduino_mega_gps_115200` | BNO085 + GPS at 115200 baud | M9N/M10S modules |
| `arduino_mega_snapshot` | BNO085 + GPS + calibration + snapshot recording | Full feature development |
| `arduino_mega_snapshot_only` | BNO085 + GPS + snapshot (no calibration) | Production with snapshots |
| `arduino_mega_full` | All: BNO085 + GPS + calibration + snapshots | Everything enabled |

**How to build**:
```bash
platformio run -e arduino_mega_gps -t upload
platformio test --environment=arduino_mega
```

---

## Source Code Layers (Dependency Flow)

```
main.cpp (event loop)
    ↓
sensors/ (hardware drivers)
    ↓
math/ (pure math, no state)
    ↓
navigation/ (state estimation)
    ↓
output/ (data serialization)
```

### Layer Details

**config/**
- Static constants, pin assignments
- No state or logic
- Included by all other layers

**sensors/**
- Hardware drivers for BNO085, GPS, buttons
- Return data structures (OrientationData, PositionData)
- No state estimation logic

**math/**
- Pure mathematical operations
- Quaternion algebra, coordinate transforms
- NO hardware dependencies
- NO state tracking
- Fully tested with unit tests

**navigation/**
- State estimation (EKF)
- Uses math/ for transformations
- Maintains 16D state vector and covariance
- Main algorithmic complexity

**output/**
- JSON formatting
- Uses all other layers to produce human-readable output
- No algorithmic complexity

---

## Testing Strategy

**Tests are organized by phase**:

1. **Phase 1 (Math Foundation)**: 100 tests
   - Quaternion operations
   - Coordinate conversions
   - Magnetic declination
   - Snapshot recording

2. **Phase 2 (GPS Integration)**: 42+ tests
   - GPS NMEA parsing
   - Coordinate frame management
   - JSON output format
   - GPS + Orientation fusion

3. **Phase 3 (EKF Sensor Fusion)**: ~143 tests total
   - EKF core (predict/update)
   - State dynamics and Jacobians
   - Measurement model
   - Integration and scenarios

**All 143+ tests pass at 100%**

---

## Documentation Organization

**Top-level docs** (in `docs/`):
- `PHASE_X_COMPLETION_SUMMARY.md` - High-level phase summary
- `API_REFERENCE.md` - User-facing API guide
- `QUICK_START.md` - Getting started
- `BUILD_GUIDE.md` - Compilation instructions

**Component-specific docs**:
- `QUATERNION_API_REFERENCE.md` - Quaternion class
- `EKF_API_REFERENCE.md` - EKF class
- `GPS_DRIVER_API_REFERENCE.md` - GPS driver
- `EKF_THEORY.md` - Mathematical background
- `EKF_TUNING_GUIDE.md` - Parameter tuning

**Hardware setup**:
- `CALIBRATION_GUIDE.md` - BNO085 calibration procedure
- `GPS_HARDWARE_SETUP.md` - GPS module wiring
- `GPS_TROUBLESHOOTING.md` - Common issues

**Archived**:
- `archive/` - Old session summaries and superseded docs
- Keep for reference, not actively used

---

## Key Files by Purpose

**To understand the system**:
1. Read: `docs/QUICK_START.md`
2. Read: `docs/README.md`
3. Explore: `src/main.cpp` (event loop structure)
4. Study: `docs/API_REFERENCE.md`

**To compile and run**:
1. See: `platformio.ini` (build environments)
2. Run: `platformio run -e arduino_mega_gps -t upload`
3. Reference: `docs/BUILD_GUIDE.md`

**To understand the math**:
1. Read: `docs/EKF_THEORY.md`
2. Study: `src/math/quaternion.h` (quaternion operations)
3. Study: `src/math/coordinates.h` (coordinate transforms)
4. Reference: `docs/QUATERNION_API_REFERENCE.md`

**To tune the EKF**:
1. Read: `docs/EKF_TUNING_GUIDE.md`
2. Adjust: `src/config/ekf_config.h` (Q, R matrices)
3. Test: Run scenario tests in `tests/scenario_test_ekf.cpp`

**For troubleshooting**:
1. GPS issues: `docs/GPS_TROUBLESHOOTING.md`
2. Calibration: `docs/CALIBRATION_GUIDE.md`
3. EKF divergence: `docs/EKF_THEORY.md` section on stability

---

## Compilation & Firmware Size

**Current Status**:
- Flash used: 45,998 bytes (18.1% of 256 KB Arduino Mega)
- RAM used: ~2.4 KB (30% of 8 KB)
- Available for future features: 208 KB flash, 5.6 KB RAM

**Build time**: ~1-2 seconds (minimal rebuild with PlatformIO)

**Test execution**: All 143+ tests run in <30 seconds

---

## Adding New Features

**To add a new feature**:

1. **Create component files**:
   - `src/new_feature/new_feature.h` - Header
   - `src/new_feature/new_feature.cpp` - Implementation

2. **Add unit tests**:
   - `tests/test_new_feature.cpp` - Test cases

3. **Add documentation**:
   - `docs/NEW_FEATURE_API_REFERENCE.md` - User guide
   - Inline code comments

4. **Update this file**:
   - Add to folder structure
   - Explain dependencies

5. **Verify**:
   - All tests pass
   - Flash usage acceptable
   - No compiler warnings

---

## Maintenance

**Weekly**:
- Run all tests: `platformio test`
- Check compilation on all build environments
- Review any new warnings

**Monthly**:
- Archive old session summaries
- Update documentation if code changes
- Clean up any temporary files

**Before release**:
- All 143+ tests passing
- Flash usage < 50%
- RAM usage < 80%
- 0 compiler warnings
- Documentation current

---

## Related Documentation

- Root-level `FOLDER_STRUCTURE.md` - Complete repo organization
- `/research/` - Theory and research documents
- `/scripts/` - Utility scripts and examples
- Parent repo: `http://github.com/...` (if applicable)

---

*Last updated: 2026-05-07 (After Phase 3 cleanup)*
*Status: Phase 3 COMPLETE - Ready for Phase 4 (Camera Calibration)*
