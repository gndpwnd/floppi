# Auto Orientation Framework — Folder Structure

This document explains the organization of the `auto_orientation/` project. It has evolved from a BNO085+GPS sensor toolkit (Phases 1–3) into a portable 3D-orientation **framework** with multi-IMU, multi-MCU, and optional-WiFi support. See [docs/scope.md](docs/scope.md) for the framework mission.

---

## Project Overview

**Auto Orientation** is a portable 3D-orientation framework for embedded systems. It provides:

1. A portable **sensor abstraction** that swaps freely between IMU chips (BNO085 today; BNO055, MPU6050 + external magnetometer planned).
2. **Multi-MCU portability** (Arduino Mega today; Nano, Teensy 4.0/4.1, ESP32, ESP32-S3 planned).
3. Sensor fusion (EKF + Madgwick) with **persistent calibration** across power cycles.
4. **Automatic calibration** (one-shot mounting capture + online adaptive drift tracking + magnetometer ellipsoid fit) — replaces hand-tuned offsets.
5. A generic **auto-PID-tuner** (relay feedback, twiddle, RLS) usable across pendulum/drone/generic control loops.
6. Optional **WiFi telemetry + browser dashboard + OTA** on ESP32-class builds (mirroring the sister `flight_controller/` project's conventions).
7. A growing catalog of **reference applications** under `src/applications/` (balancing robot, multirotor bridge, gimbal, photogrammetry rig, educational kit).

**Current state** (2026-05-26): Phase 3 complete (BNO085 + GPS + EKF, 143+ tests passing). Phase 4 has shipped — bifurcated 2026-05-19 into Phase 4U (Uno minimal flight build + on-device guided tuning) and Phase 4M.* (Mega universal/adaptive stack: BOOTSTRAP, RLS, mounting estimator, collision detector, wheel encoders, two-stage cascade through 4M.14). See [docs/roadmap.md](docs/roadmap.md) for the full phase plan and per-phase landing reports under [docs/findings/](docs/findings/INDEX.md).

---

## Directory Structure

```
auto_orientation/
├── platformio.ini              # Build system configuration
│                              # Defines 7 active build environments
│                              # (+2 scaffolded/commented: esp32_balance, teensy_balance)
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

## Planned `src/` additions (Phase 4+)

These modules are scoped in [docs/roadmap.md](docs/roadmap.md) and detailed in [docs/findings/](docs/findings/INDEX.md). They do not yet exist in source but the planning is complete:

```
src/
├── storage/                       # Phase 4.1 — persistent storage HAL
│   ├── persistent_storage.h       # Single API: begin/read/write/commit/clear/capacity
│   ├── persistent_storage_avr.cpp # AVR EEPROM backend
│   ├── persistent_storage_teensy.cpp  # Teensy emulated EEPROM backend
│   └── persistent_storage_esp32.cpp   # ESP32 Preferences/NVS backend
│                                  # ^ fixes Known Issue KI-1 (silent EEPROM fail on ESP32)
│
├── control/                       # Phase 4.5 — generic PID + auto-tuner
│   ├── pid_controller.h/cpp       # Generic single-axis PID
│   ├── auto_pid_tuner.h/cpp       # Strategy-driven auto-tune coordinator
│   ├── tuners/
│   │   ├── relay_feedback.cpp     # Åström-Hägglund, default for pendulums
│   │   ├── twiddle.cpp            # Coordinate descent, generic fallback
│   │   └── rls_systemid.cpp       # Recursive LS + analytical PID, for drones
│   └── tuning_strategy.h          # ITuningStrategy interface + SafetyLimits
│
├── navigation/                    # Phase 4.3-4.4 additions
│   ├── mounting_calibration.h/cpp # One-shot gravity-vector mounting capture
│   ├── balance_kalman.h/cpp       # 2-state Kalman (pitch + gyro-bias) for balance loop
│   └── online_mounting_estimator.h/cpp  # Adaptive drift tracker (Phase 4.4)
│
├── sensors/                       # Phase 4.6 + 5.5 additions
│   ├── bno055.h/cpp               # BNO055 driver (Phase 4.6)
│   ├── mpu6050.h/cpp              # Raw gyro+accel (Phase 5.5)
│   ├── external_magnetometer.h/cpp # Abstract magnetometer (Phase 5.5)
│   ├── hmc5883l.h/cpp             # Concrete magnetometer (Phase 5.5)
│   ├── qmc5883l.h/cpp             # Concrete magnetometer (Phase 5.5)
│   ├── lis3mdl.h/cpp              # Concrete magnetometer (Phase 5.5)
│   └── fused_imu.h/cpp            # Madgwick adapter; implements OrientationSensor
│
├── actuators/                     # Phase 4.7 — application motor drivers
│   ├── motor_driver.h             # Base interface
│   └── l298n_motor_driver.h/cpp   # L298N dual-channel PWM
│
├── network/                       # Phase 6 — ESP32 WiFi stack
│   ├── wifi_manager.h/cpp         # STA + mDNS hostname
│   ├── wifi_credentials.h         # Template; gitignored at runtime
│   ├── web_server.h/cpp           # Static asset server (LittleFS)
│   ├── api_server.h/cpp           # REST + WebSocket endpoints
│   └── ota.h/cpp                  # ArduinoOTA + HTTP-pull update
│
└── applications/                  # Reference application catalog
    ├── balancing_robot/           # Phase 4.7 — primary reference
    │   ├── balance_app.h/cpp      # IDLE → CAPTURE → TUNE → RUN → SAFE_FALL state machine
    │   └── safety.h/cpp           # Tilt limits, motor disarm, watchdog
    ├── multirotor_bridge/         # Phase 7 — I2C slave for flight_controller
    ├── camera_mount/              # Phase 7 — 2- or 3-axis gimbal
    ├── photogrammetry/            # Phase 7 — wires up snapshot recorder
    └── edu_kit/                   # Phase 7 — Nano + MPU6050 minimum-viable build

data/                              # Phase 6 — LittleFS browser dashboard assets
└── www/
    ├── index.html                 # Dashboard home
    ├── three.min.js               # Vendored Three.js (~150 KB)
    ├── app.js                     # WebSocket + UI logic
    └── style.css

tests/data/                        # Phase 4.7 — scenario test fixtures
└── balancing_reference_trajectory.csv  # Recorded pitch trace for regression
```

Each module is gated by a compile flag in `src/config/mode.h`:
`USE_BNO055`, `USE_MPU6050`, `USE_WIFI`, `USE_BALANCING_ROBOT`, `USE_TUNER_RELAY` / `USE_TUNER_TWIDDLE` / `USE_TUNER_RLS`, etc. A build only links the code its application needs.

---

## Documentation Layout (post-reorganization 2026-05-12)

```
docs/
├── INDEX.md                       # Root navigation index
├── README.md
├── scope.md                       # Framework mission, in/out of scope
├── roadmap.md                     # Phase 4–8 plan
├── todo.md                        # Active task list
│
├── getting_started/               # Onboarding (GETTING_STARTED, FAQS, ARCHITECTURE)
├── theory/                        # Math + concept background
├── build/                         # Build guides + feature flags
├── hardware/                      # Wiring + GPS hardware + troubleshooting
├── calibration/                   # End-user procedure + impl notes
├── phases/                        # PHASE_1/2/3 plans, results, summaries, release checklists
├── reference/                     # API references (quaternion, GPS, BNO085, EKF, SH-2)
├── implementation/                # Per-component impl walk-throughs
├── guides/                        # Task-oriented how-to guides
├── research/                      # Long-form research compilations (MPU6050)
├── findings/                      # Short focused research notes (drives design decisions)
│   ├── INDEX.md
│   ├── auto_pid_tuning_research.md
│   ├── balance_point_and_mounting_research.md
│   ├── bno055_driver_and_multi_imu_strategy.md
│   ├── multi_mcu_port_strategy.md
│   ├── wifi_telemetry_integration_design.md
│   ├── mpu6050_external_mag_pipeline.md
│   ├── application_catalog.md
│   ├── test_infrastructure_expansion.md
│   ├── browser_dashboard_architecture.md
│   ├── online_adaptive_balance_tracking.md
│   ├── disturbance_compensation_research.md
│   └── tetherless_operation_strategy.md
│
├── setup/                         # First-time setup + next-step planning
├── testing/                       # Test manifests + READMEs + integration test guide
├── todo/                          # In-progress checklists + per-session status
└── archive/                       # Old session records, superseded docs, reference sketches
    ├── balancing_robot_reference/ # Archived .ino + dissection notes
    └── session_records/           # Dated work logs (YYYY-MM-DD_topic.md)
```

Every doc folder has an `INDEX.md` for quick navigation. The 32 previously-loose files at `docs/` root have been moved into thematic subfolders.

---

## Build Environments (platformio.ini)

The project supports 7 active build configurations (per [`platformio.ini`](platformio.ini)):

| Environment | Features | Use Case |
|-------------|----------|----------|
| `mega_balance` | Universal/adaptive balance stack (BOOTSTRAP, RLS, mounting est, collision, encoders, cascade) | Primary build — new code lands here |
| `arduino_uno_minimal` | Lean Uno balance: hardcoded PID, no auto-tune | Uno flight build (after `arduino_uno_tuning`) |
| `arduino_uno_tuning` | `arduino_uno_minimal` + on-device guided BNO055 cal (`'c'`) + P→D→I tune (`'t'`) | Uno setup/tuning bench |
| `mega_orientation` | BNO085 + GPS telemetry (EKF stub: `USE_EKF=0`) | Orientation framework reference |
| `mega_orientation_calibration` | `mega_orientation` + verbose `CALIBRATION_MODE` | BNO085 calibration walk-through |
| `native_test` | Unity unit tests on host | `pio test -e native_test` |
| `uno_balance` | LEGACY/DEAD — old universal-stack-on-Uno (~93.6% flash) | Kept for history; do not extend |

Scaffolded but commented out: `esp32_balance` (future WiFi/dashboard port), `teensy_balance` (future FPU/high-rate port).

**How to build**:
```bash
pio run -e mega_balance -t upload         # Mega primary
pio run -e arduino_uno_tuning -t upload   # Uno setup/tune
pio run -e arduino_uno_minimal -t upload  # Uno flight
pio test -e native_test                   # Host tests
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

*Last updated: 2026-06-21 (nav-polish — env table reflects current `platformio.ini`; Phase 4U + 4M.* shipped per 2026-05-20 session records)*
*Status: Phase 4 SHIPPED — Phase 4U (Uno minimal + guided tuning) live; Phase 4M.* (Mega universal stack through 4M.14 two-stage cascade) live. Bench-validation, multi-MCU ports, WiFi/dashboard, applications catalog expansion remain.*
