# Auto Orientation: Universal Sensor Calibration Toolkit

**Purpose**: Automatically detect absolute orientation (pitch, roll, yaw) and position from multi-sensor combinations, with persistent calibration and field-ready deployment.

**Status**: Initialization Phase (v1.0 in active development)

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
See [Developer Guide: Adding Sensors](docs/features/adding-sensors.md)

---

## Documentation

- **[Scope](docs/scope.md)** — Project boundaries, constraints, first-release definition
- **[Roadmap](docs/roadmap.md)** — Feature checklist and milestones
- **[Todo](docs/todo.md)** — Current session tasks
- **[Findings](docs/findings/)** — Research notes on calibration, GPS, sensor fusion
- **[Archive](docs/archive/)** — Historical context, initial sketch, requirements email

---

## For Developers

### First Time?
1. Read [Scope](docs/scope.md) to understand project boundaries
2. Check [Hardware Setup](docs/features/hardware-setup.md) to understand wiring
3. Review [API Reference](docs/features/api-reference.md) to understand sensor interfaces
4. Build and flash the existing sketch to validate your setup

### Want to Add a Sensor?
See [Adding Sensors](docs/features/adding-sensors.md)

### Debugging Issues?
1. Check [Troubleshooting](docs/features/troubleshooting.md)
2. Run `python3 tools/serial_monitor.py --debug` for verbose output
3. Review findings in `docs/findings/` for known issues

---

## Related Projects

- **flight_controller** — Will import auto_orientation for auto-calibration
- **skytracker_algorithm** — May use camera orientation output for 3D reconstruction
- **drone_3d_model** — May reference orientation data for frame testing

---

## Contact & Questions

See [Scope](docs/scope.md) for technical decisions and rationale.  
See [Roadmap](docs/roadmap.md) for upcoming work and research items.

