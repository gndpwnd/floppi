# Floppi Repository Folder Structure

## Root Level Organization

```
/home/devel/floppi/
├── README.md                    # Main repository readme
├── .gitignore                   # Git ignore rules
│
├── auto_orientation/            # PRIMARY PROJECT: BNO085 + GPS + EKF
│   ├── src/                    # Source code (math, sensors, navigation, output)
│   ├── tests/                  # Unit and integration tests
│   ├── docs/                   # Project documentation
│   ├── tools/                  # Build scripts and utilities
│   ├── lib/                    # Third-party libraries
│   └── platformio.ini          # PlatformIO build configuration
│
├── flight_controller/           # Flight controller firmware (separate project)
├── drone_3d_model/             # 3D CAD models and related docs
├── fc_tool/                    # Flight controller tools application
├── swarm_api/                  # Swarm robotics API
│
├── research/                   # Research documents and references
│   ├── GPS_*.md               # GPS and geodetic coordinate theory
│   ├── CAMERA_EXTRINSIC_CALIBRATION.md
│   └── ARDUINO_DIAGNOSTICS.md
│
├── scripts/                    # Utility scripts (Python, Shell)
│   ├── GPS_IMPLEMENTATION_EXAMPLES.py
│   └── build_*.sh
│
├── archived_docs/              # Old documentation and session summaries
│   └── TASK_COMPLETION_SUMMARY.md
│
├── docs/                       # General project documentation (not auto_orientation)
│   ├── archive/               # Archived docs
│   ├── findings/              # Research findings
│   ├── guides/                # Setup and usage guides
│   └── todo/                  # Task tracking
│
├── literature/                # Academic papers and references
├── darpa_lift_2026/          # DARPA competition-related files
└── ...other_projects...       # Various other projects
```

---

## auto_orientation/ Structure (PRIMARY PROJECT)

```
auto_orientation/
├── platformio.ini              # PlatformIO configuration
│                              # - Multiple build environments (production, debug, gps, full)
│                              # - Library dependencies (Adafruit BNO085, SD card, etc)
│
├── src/                        # Source code (organized by layer)
│   ├── main.cpp               # Main entry point and event loop
│   │
│   ├── config/                # Configuration and constants
│   │   ├── pins.h            # Arduino pin definitions
│   │   ├── mode.h            # Build mode macros (CALIBRATION_MODE, etc)
│   │   ├── gps_config.h      # GPS configuration parameters
│   │   └── ekf_config.h      # EKF tuning parameters (Q, R matrices)
│   │
│   ├── sensors/               # Hardware sensor drivers
│   │   ├── sensor_base.h     # Base classes for all sensors
│   │   ├── bno085.h/cpp      # BNO085 IMU absolute orientation
│   │   ├── gps.h/cpp         # GPS module NMEA parser
│   │   └── button_input.h    # Debounced button input (snapshot trigger)
│   │
│   ├── math/                  # Mathematical operations (Phase 1 foundation)
│   │   ├── quaternion.h/cpp           # Quaternion class and operations
│   │   ├── quaternion_conversions.h   # Quaternion ↔ matrix ↔ Euler
│   │   ├── coordinates.h/cpp          # GPS ↔ ECEF ↔ NED conversions
│   │   ├── magnetic_declination.h/cpp # Magnetic heading corrections
│   │   └── matrix_utils.h            # General matrix operations
│   │
│   ├── navigation/            # Navigation and state estimation (Phase 2-3)
│   │   ├── coordinate_frame.h/cpp     # Local NED origin manager
│   │   ├── ekf.h/cpp                 # Extended Kalman Filter
│   │   ├── state_dynamics.h/cpp       # EKF state transition and Jacobians
│   │   ├── measurement_model.h/cpp    # GPS measurement model
│   │   └── covariance_manager.h/cpp   # Numerical stability utilities
│   │
│   ├── features/              # Optional features (conditional compilation)
│   │   ├── snapshot_recorder.h/cpp    # Record orientation snapshots to SD card
│   │   └── snapshot_logger.h/cpp      # Logging infrastructure
│   │
│   ├── output/                # Data output formatting
│   │   └── sensor_output_manager.h/cpp # JSON formatting for all sensors
│   │
│   └── file_system/           # File I/O operations
│       ├── sd_card.h/cpp      # SD card read/write wrapper
│       └── file_manager.h/cpp # File management utilities
│
├── tests/                      # Comprehensive test suite
│   ├── test_quaternion.cpp               # Phase 1: Quaternion math
│   ├── test_coordinates.cpp              # Phase 1: Coordinate conversions
│   ├── test_magnetic_declination.cpp     # Phase 1: Magnetic declination
│   ├── test_snapshot_recorder.cpp        # Phase 1: Snapshot feature
│   ├── test_json_output.cpp              # Phase 2: JSON formatting
│   ├── test_coordinate_frame.cpp         # Phase 2: Coordinate frame
│   ├── test_gps.cpp                      # Phase 2: GPS driver
│   ├── integration_test_gps_fusion.cpp   # Phase 2: Integration
│   ├── test_ekf.cpp                      # Phase 3: EKF core
│   ├── test_state_dynamics.cpp           # Phase 3: State transitions
│   ├── test_measurement_model.cpp        # Phase 3: GPS measurement
│   ├── integration_test_ekf_full.cpp     # Phase 3: EKF integration
│   └── scenario_test_ekf.cpp             # Phase 3: Real-world scenarios
│
├── docs/                      # Project documentation
│   ├── README.md              # Auto Orientation project overview
│   ├── QUICK_START.md         # Getting started guide
│   ├── BUILD_GUIDE.md         # Compilation instructions
│   │
│   ├── PHASE_1_COMPLETION_SUMMARY.md
│   ├── PHASE_2_COMPLETION_SUMMARY.md
│   ├── PHASE_3_COMPLETION_SUMMARY.md
│   ├── PHASE_3_MASTER_IMPLEMENTATION_PLAN.md
│   │
│   ├── API_REFERENCE.md
│   ├── QUATERNION_API_REFERENCE.md
│   ├── EKF_API_REFERENCE.md
│   ├── EKF_THEORY.md
│   ├── EKF_TUNING_GUIDE.md
│   ├── GPS_DRIVER_API_REFERENCE.md
│   ├── GPS_HARDWARE_SETUP.md
│   ├── GPS_TROUBLESHOOTING.md
│   ├── COORDINATE_FRAME_API_REFERENCE.md
│   │
│   ├── archive/               # Old documentation
│   │   ├── QUATERNION_IMPLEMENTATION_SUMMARY.md
│   │   ├── GPS_DRIVER_IMPLEMENTATION_SUMMARY.md
│   │   ├── SESSION_SUMMARY_*.md
│   │   └── IMPLEMENTATION_SUMMARY_*.md
│   │
│   └── findings/              # Research findings
│
├── lib/                        # Third-party libraries
├── tools/                      # Build scripts
│   └── build_tests.sh
│
└── .pio/                       # PlatformIO build artifacts
```

---

## Quick File Navigation

**Getting Started**:
- `auto_orientation/README.md` - Project overview
- `auto_orientation/docs/QUICK_START.md` - Quick start guide
- `FOLDER_STRUCTURE.md` - This file

**Building & Flashing**:
- `auto_orientation/platformio.ini` - Build configuration
- `auto_orientation/docs/BUILD_GUIDE.md` - Build instructions

**Documentation by Phase**:
- Phase 1: Math & Quaternions
- Phase 2: GPS Integration
- Phase 3: EKF Sensor Fusion

**Research Documents**:
- `/research/GPS_*.md` - GPS theory
- `/research/CAMERA_EXTRINSIC_CALIBRATION.md` - Camera math

**Scripts & Tools**:
- `/scripts/GPS_IMPLEMENTATION_EXAMPLES.py` - Python examples
- `/scripts/build_*.sh` - Build automation

---

*Last updated: 2026-05-07 (After Phase 3 cleanup)*
