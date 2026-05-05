# Project Scope: Auto Orientation

**Status**: Initialization Phase  
**Last Updated**: 2026-05-05

---

## Objectives

Build a **universal orientation calibration and detection toolkit** that:
- Automatically detects absolute orientation (pitch, roll, yaw) in Earth's NED frame (North-East-Down)
- Works with various sensor combinations (accelerometer, gyroscope, magnetometer)
- Persists calibration data locally for rapid re-initialization
- Outputs standard orientation representations (quaternions, Euler angles, rotation matrices)
- Serves as a foundation for downstream applications (flight controller auto-calibration, camera orientation, 3D reconstruction)

---

## First Stable Release: BNO085 + GPS Integration

### Definition
A working, testable system that:
- Reads BNO085 IMU (absolute orientation quaternions, calibration status)
- Reads Ublox NEO-M9N GPS (lat/lon/alt, positional accuracy)
- Persists BNO085 calibration to onboard flash memory
- Outputs combined orientation + position data over serial
- Runs on Arduino-based hardware with PlatformIO

### Success Criteria
- [ ] BNO085 initializes and outputs quaternions in Absolute Orientation mode (not RVC)
- [ ] Magnetometer calibration persists across power cycles
- [ ] GPS module communicates over USB serial at startup
- [ ] Combined data logged to serial in structured format
- [ ] Python monitoring script displays real-time orientation + position
- [ ] Can run sketch locally without cloud dependencies
- [ ] Documentation sufficient for developers to add new sensors

### Testing Targets
- Static calibration test: Place device on flat surface, verify orientation remains stable for 10+ samples
- Dynamic calibration test: Rotate device through known orientations, verify quaternions track rotation
- Persistence test: Power cycle device, verify calibration state restored without re-doing full calibration
- GPS test: Boot with GPS connected, verify position and accuracy estimates stream correctly

---

## Constraints & Assumptions

### Hardware Constraints
- Primary target: Arduino-compatible boards (tested on Nano, Mega)
- BNO085 communicates via UART (P1 pin set high for non-RVC UART mode)
- NEO-M9N GPS communicates via USB serial
- Calibration storage: BNO085 has onboard flash; other sensors may use SD card

### Library Constraints
- Use Adafruit BNO08x library (not RVC variant, not legacy BNO055)
- Use standard ArduinoJSON or similar for local config parsing
- **Local-first**: All libraries cloned locally; no cloud/IDE library manager during field development
- PlatformIO for build/upload (not Arduino IDE)

### Environmental Constraints
- Works best when magnetometer is calibrated for local magnetic declination
- Yaw (heading) accuracy degrades with poor magnetometer calibration (see findings)
- GPS accuracy: nominal ±1m; can improve to ~0.1m with multi-sample averaging on stationary fix

---

## What IS In Scope

**For v1.0 release**:
- ✅ BNO085 IMU integration (quaternion + calibration status output)
- ✅ Ublox NEO-M9N GPS integration (lat/lon/alt + accuracy)
- ✅ Persistent calibration (saved to BNO085 flash, restored on boot)
- ✅ Serial output format (structured, parseable)
- ✅ Python monitoring tool (display orientation + position in real-time)
- ✅ PlatformIO project structure (modular, extensible)
- ✅ Documentation (how to add sensors, calibration guide, API reference)

**For future releases**:
- 📋 MPU 6050 support (gyro + accel, no magnetometer; requires alternative calibration approach)
- 📋 SD card logging (for high-frequency data capture during development)
- 📋 IMU-only orientation (when GPS unavailable)
- 📋 GPS accuracy improvement (multi-sample static averaging)
- 📋 Web dashboard (real-time visualization of orientation + position)

---

## What is OUT of Scope

**Explicitly excluded from v1.0**:
- ❌ Flight controller integration (auto-calibration of FC per se; see flight_controller project instead)
- ❌ Camera orientation inference without IMU (3D reconstruction/skytracker integration is separate)
- ❌ Sensor fusion algorithms beyond BNO085's built-in fusion (BNO's firmware handles this)
- ❌ Real-time 3D visualization (web dashboard is future work)
- ❌ Cloud storage or external APIs
- ❌ Support for non-Arduino platforms (Raspberry Pi, etc. are future)

---

## Technical Decisions & Rationale

| Decision | Choice | Why |
|----------|--------|-----|
| **Build System** | PlatformIO | Modular, reproducible locally, IDE-agnostic |
| **First Sensor** | BNO085 | Built-in sensor fusion, persistent calibration, quaternion output |
| **GPS Module** | Ublox NEO-M9N | Pre-selected by collaborator, clean USB interface, good accuracy |
| **Orientation Representation** | Quaternions (primary) | Standard for rotation tracking; minimal gimbal lock; aligns with ROS/flight control conventions |
| **Calibration Storage** | Sensor-native when possible | BNO085 has built-in flash; reduces external dependencies |
| **Architecture** | Sensor abstraction layer | Support adding MPU 6050 / other sensors without rewriting core logic |
| **Deployment Model** | Standalone toolkit | Keeps auto_orientation independent; flight_controller/skytracker import as needed |
| **Development Constraints** | Local libraries only | Field deployment must not depend on cloud IDE or dynamic downloads |

---

## Known Research Gaps (See findings/)

- **Magnetometer calibration persistence**: How to properly read/write BNO085 calibration profile from/to memory (initial testing in progress)
- **GPS accuracy improvement**: Statistical approaches for multi-sample averaging to reduce CEP error
- **MPU 6050 yaw-only orientation**: Without magnetometer, MPU 6050 can only provide pitch/roll reliably. Approaches for yaw estimation need research.
- **Sensor fusion design**: Whether to implement custom sensor fusion or use sensor manufacturer firmware (currently using BNO085 firmware; MPU 6050 will require decision)

---

## Integration Points

### Upstream (Provides to)
- `flight_controller` project: auto-calibration routines for IMU/compass
- `skytracker_algorithm`: camera orientation context for 3D reconstruction
- `camera calibration kit`: could use auto_orientation for initial rig orientation

### Downstream (Depends on)
- Adafruit BNO08x library (local clone)
- Ublox NEO-M9N USB driver / serial protocol documentation
- PlatformIO build system

---

## Non-Goals

- **General-purpose IMU library**: This is task-specific, not a reusable component library
- **Real-time visualization**: Text serial output for v1.0; visualization is future
- **High-frequency logging**: Focus on calibration + position output; data logging is secondary
- **Extreme accuracy**: Focus on "good enough for field deployment"; sub-mm accuracy not required

