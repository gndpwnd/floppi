# Auto Orientation: Universal Sensor Calibration Toolkit

**Purpose**: Automatically detect absolute orientation (pitch, roll, yaw) and position from multi-sensor combinations, with persistent calibration and field-ready deployment.

**Status**: Initialization Phase (v1.0 in active development)

> **Design direction**: see [MINIMIZE_ACCELERATIONS_PHILOSOPHY.md](MINIMIZE_ACCELERATIONS_PHILOSOPHY.md) for the project's current design direction (balancing-robot reference application).
>
> **2026-05-19 pivot**: balancing-robot reference is now **bifurcated** by MCU class. The universal/adaptive stack (BOOTSTRAP, RLS, mounting estimator, collision detector, planned encoders + position containment) moves to **Mega-class hardware only** — see [`MEGA_UNIVERSAL_PLAN.md`](MEGA_UNIVERSAL_PLAN.md). The Uno gets a **separate small hardcoded balancer** with offline Python brute-force tuning — see [`applications/balancing_robot_uno/README.md`](applications/balancing_robot_uno/README.md). Top-level index in [`INDEX.md`](INDEX.md) §Strategic notes.

---

## Quick Start

**Prerequisites**: PlatformIO CLI, Arduino-compatible board (Nano/Mega), BNO085 IMU, Ublox NEO-M9N GPS

```bash
# Clone libraries locally (in progress)
platformio run --target build

# Flash to board
platformio run --target upload

# Monitor output
python3 tools/serial_monitor.py
```

---

## What It Does

### For v1.0
- **BNO085 IMU**: Reads absolute orientation (quaternions) + calibration status, persists magnetometer calibration
- **Ublox NEO-M9N GPS**: Reads position (lat/lon/alt) + positional accuracy
- **Serial Output**: Streams combined orientation + position data
- **Python Tools**: Real-time serial monitoring + data logging

### Future Capabilities
- Multi-sensor support (MPU 6050, other IMUs/magnetometers)
- SD card data logging
- Web dashboard for visualization
- Integration with flight controller auto-calibration

---

## Project Structure

```
auto_orientation/
├── docs/
│   ├── README.md                    # You are here
│   ├── scope.md                     # Project boundaries & constraints
│   ├── roadmap.md                   # Feature checklist & milestones
│   ├── todo.md                      # Current session tasks
│   ├── features/                    # Feature specifications
│   ├── findings/                    # Research & discoveries
│   │   ├── bno085-calibration-persistence.md
│   │   ├── gps-accuracy-improvement.md
│   │   └── mpu6050-yaw-estimation.md
│   └── archive/                     # Session summaries, old code
│       ├── BN085_I2C_Adafruit.ino   # Initial Arduino sketch
│       └── compere_init.md          # Dr. Comper's requirements email
├── src/
│   ├── main.cpp                     # Arduino sketch entry point
│   ├── sensors/                     # Sensor abstraction layer
│   │   ├── bno085.h / .cpp
│   │   ├── neo_m9n.h / .cpp
│   │   └── sensor_base.h
│   ├── output/
│   │   ├── serial_output.h / .cpp
│   │   └── data_format.h
│   └── config/
│       ├── pins.h                   # Pin configuration
│       └── sensor_config.h
├── lib/                             # Local library clones
│   ├── BNO08x-Arduino-Library/
│   ├── ublox-gps-parsers/
│   └── ArduinoJSON/ (if needed)
├── tools/
│   ├── serial_monitor.py            # Real-time serial monitoring
│   ├── data_logger.py               # CSV logging (future)
│   └── calibration_utils.py         # Calibration analysis (future)
├── tests/
│   ├── test_quaternion_math.py      # Unit tests
│   └── test_serial_format.py
├── platformio.ini                   # PlatformIO configuration
├── .gitignore                       # Git ignore rules
└── README.md                        # This file
```

---

## Key Concepts

### Orientation Representation
**Quaternions** (primary output): 4-element rotation representation (scalar + 3 components)
- Less subject to gimbal lock than Euler angles
- Standard in robotics/flight control
- BNO085 outputs natively

### Absolute vs. Relative Orientation
- **Absolute**: Orientation relative to Earth's NED (North-East-Down) frame. Requires magnetometer + reference north.
- **Relative**: Orientation relative to some reference. Gyro-only approaches.

### Persistent Calibration
- BNO085 has onboard flash memory for magnetometer calibration
- On boot, restores calibration without user intervention
- Saves ~30 seconds of manual calibration per boot cycle

### GPS Accuracy
- Nominal accuracy: ±1 meter CEP
- Can improve to ~0.1m CEP by averaging multiple samples when stationary
- Accuracy metrics in output stream

---

## Usage

### Running the Hardware
1. Wire BNO085 per hookup guide (UART mode, P1 pin = 5V)
2. Connect NEO-M9N GPS via USB
3. Build & flash with PlatformIO
4. Monitor with `python3 tools/serial_monitor.py`

### Understanding Output
```
timestamp, quat_w, quat_x, quat_y, quat_z, lat, lon, alt, cep_m
1234567890, 0.707, 0.0, 0.0, 0.707, 37.4419, -122.1430, 150.5, 1.2
```

### Adding a New Sensor
See [Adding New Sensors](guides/ADDING_NEW_SENSORS.md)

---

## Documentation

### Phase 1: Complete (v1.0 - Quaternion Math & Data Logging)

**✓ COMPLETE - Phase 1 Status**: All core functionality implemented and tested (113 unit tests passing)

#### Getting Started
- **[Build Guide](build/BUILD_GUIDE.md)** — How to compile different configurations
- **[Feature Flags](build/FEATURE_FLAGS.md)** — Available build flags and their effects
- **[Quick Start](guides/QUICK_START.md)** — First-time setup and basic usage

#### API Reference (Phase 1)
- **[Quaternion API Reference](reference/QUATERNION_API_REFERENCE.md)** — Complete quaternion math library
  - Quaternion operations, conversions to/from Euler angles and rotation matrices
  - Vector rotation, frame transformations
  - Performance: all operations < 10 µs on Arduino Mega
  
- **[Coordinate Conversion API](reference/COORDINATE_CONVERSION_API.md)** — GPS and navigation transforms
  - GPS ↔ ECEF ↔ NED conversions
  - Accuracy: sub-meter round-trip error
  - Tested on 6+ reference locations (equator, poles, antimeridian)

#### Feature Guides
- **[Snapshot Feature Guide](build/SNAPSHOT_FEATURE_GUIDE.md)** — JSON data logging to SD card
  - How to enable SNAPSHOT_MODE
  - JSON format specification
  - Troubleshooting and performance characteristics
  - SD card file organization and reading

#### Testing & Quality Assurance
- **[Phase 1 Test Results](phases/PHASE_1_TEST_RESULTS.md)** — Complete test coverage and results
  - 113 unit tests (47 quaternion, 38 coordinate, 12 BNO085, 16 snapshot)
  - Performance benchmarks
  - Known limitations and workarounds
  - Memory and binary size analysis

### Architecture & Planning
- **[Layered Architecture](architecture/INDEX.md)** — Drill-down architecture diagrams: Level 0 system overview → Level 1 subsystems (Mega adaptive stack, Uno minimal path, sensor/odometry pipeline, storage HAL) → Level 2 components (BOOTSTRAP, position cascade). Reflects the Mega-universal vs Uno-minimal split.
- **[Architecture (sensor fusion)](getting_started/ARCHITECTURE.md)** — Original BNO085 + GPS system design with 12 Mermaid diagrams (comprehensive overview)
- **[Scope](scope.md)** — Project boundaries, constraints, first-release definition
- **[Roadmap](roadmap.md)** — Feature checklist and milestones
- **[Phase 1 Master Plan](phases/PHASE_1_MASTER_IMPLEMENTATION_PLAN.md)** — Detailed implementation roadmap
- **[Todo](todo.md)** — Current session tasks

### Research & Reference
- **[Math & Applications Guide](theory/MATH_AND_APPLICATIONS_MASTER_GUIDE.md)** — Quaternion and coordinate theory
- **[Findings](findings/)** — Research notes on calibration, GPS, sensor fusion
- **[Archive](archive/)** — Historical context, initial sketch, requirements email

---

## For Developers

### Getting Started (Recommended Order)
1. **[Build Guide](build/BUILD_GUIDE.md)** — Compile and upload firmware (5 min)
2. **[Feature Flags](build/FEATURE_FLAGS.md)** — Understand build options (10 min)
3. **[Quick Start](guides/QUICK_START.md)** — Run your first test (15 min)
4. **[Quaternion API Reference](reference/QUATERNION_API_REFERENCE.md)** — Learn math library (30 min)
5. **[Phase 1 Test Results](phases/PHASE_1_TEST_RESULTS.md)** — Understand capabilities & limits (20 min)

### For Field Deployment
1. **[Snapshot Feature Guide](build/SNAPSHOT_FEATURE_GUIDE.md)** — Set up data logging
2. **[Hardware Setup](guides/HARDWARE_SETUP.md)** — Physical wiring
3. **[Build Guide](build/BUILD_GUIDE.md)** — Use `arduino_mega_snapshot_only` environment
4. **[Feature Flags](build/FEATURE_FLAGS.md)** — Optimize binary for your board

### Architecture Overview
- Read [Architecture](getting_started/ARCHITECTURE.md) for comprehensive system overview with diagrams
- Check [Hardware Setup](guides/HARDWARE_SETUP.md) to understand wiring
- Review [Scope](scope.md) to understand project boundaries

### Want to Add a Sensor?
See [Adding Sensors](guides/ADDING_NEW_SENSORS.md)

### Debugging Issues?
1. Check [FAQs](getting_started/FAQS.md) and [Troubleshooting in QUICK_START](guides/QUICK_START.md#troubleshooting-quick-reference)
2. Run `python3 tools/serial_monitor.py --debug` for verbose output
3. Review findings in `docs/findings/` for known issues

---

## Related Projects

- **flight_controller** — Will import auto_orientation for auto-calibration
- **skytracker_algorithm** — May use camera orientation output for 3D reconstruction
- **drone_3d_model** — May reference orientation data for frame testing

---

## Contact & Questions

See [Scope](scope.md) for technical decisions and rationale.  
See [Roadmap](roadmap.md) for upcoming work and research items.

