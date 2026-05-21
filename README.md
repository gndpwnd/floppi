# floppi

Open-source flight controller firmware and flight computer integration platform for drone systems.

**Focus:** Firmware development, sensor integration, and flight computer coordination for drones across the complete scale spectrum (micro to heavy-lift cargo).

> **Base system:** [dRehmFlight](https://github.com/nickrehm/dRehmFlight) - PlatformIO-based refactored implementation

---

## What is floppi?

floppi provides flight controller firmware and flight computer integration software for autonomous drone systems. This project focuses exclusively on the software and firmware side of drone development.

**For physical design, mechanical optimization, and performance calculators, see [engineering360](https://github.com/yourusername/engineering360).**

### Core Components

1. **Flight Controller Firmware** (`/flight_controller/`)
   - dRehmFlight-based firmware for Teensy 4.0/4.1 and ESP32/S3
   - PID control loops and stabilization
   - IMU sensor integration (MPU6050/MPU9250)
   - Multiple receiver protocols (SBUS, iBUS, DSM, PPM, PWM)
   - Flight modes: Rate, Angle (compile-time selected)
   - ESP32 WiFi telemetry and command API

2. **Swarm API** (`/swarm_api/`)
   - Python FastAPI ground-station control application
   - Browser dashboard for manual control and real-time telemetry
   - Controls ESP32-based drones over WiFi (HTTP + WebSocket)
   - Drone fleet management and mDNS discovery

3. **fc_tool** (`/fc_tool/`)
   - Cross-platform desktop tool (Rust + Tauri)
   - Serial monitoring and real-time data visualization
   - Dynamic multi-graph plotter for firmware diagnostics

4. **Auto Orientation** (`/auto_orientation/`)
   - Universal sensor calibration toolkit
   - Automatic detection of absolute orientation and position from multi-sensor combinations
   - Balancing-robot reference application (Arduino Uno/Mega)

5. **Documentation** (`/docs/`)
   - Setup guides and wiring diagrams
   - Calibration procedures
   - API documentation
   - Development roadmap

---

## Quick Start

### 1. Setup Development Environment

Install PlatformIO:

```bash
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
python3 get-platformio.py
```

Add to PATH temporarily:

```bash
export PATH="$PATH:$HOME/.platformio/penv/bin/"
```

Add to PATH permanently:

```bash
echo 'export PATH="$PATH:$HOME/.platformio/penv/bin/"' >> ~/.bashrc
source ~/.bashrc
```

### 2. Build Flight Controller Firmware

```bash
cd flight_controller
pio run
```

### 3. Upload to Teensy

```bash
pio run --target upload
```

---

## Documentation

- **[Project Scope](./docs/scope.md)** - What floppi does (and doesn't do)
- **[Development Roadmap](./docs/ROADMAP.md)** - Development phases and milestones
- **[Flight Controller Docs](./docs/flight-controller/)** - Firmware setup and configuration
- **[Flight Computer Docs](./docs/flight-computer/)** - Integration guides
- **[Migration Summary](./docs/MIGRATION_SUMMARY.md)** - Recent scope changes

---

## Project Status

**Current Phase:** Phase 1 - Flight Controller Core Development

- [x] Refactored dRehmFlight to PlatformIO
- [x] Modular receiver support (SBUS/iBUS/DSM)
- [x] IMU integration (MPU6050/MPU9250)
- [ ] Hardware bench testing
- [ ] First flight tests

See [ROADMAP.md](./docs/ROADMAP.md) for detailed status.

---

## Hardware Requirements

### Minimum (Flight Controller Only)

- **Microcontroller:** Teensy 4.0 or 4.1
- **IMU:** MPU6050 or MPU9250
- **Receiver:** SBUS-capable (e.g., FlySky FS-iA6B)
- **ESCs:** Standard PWM ESCs
- **Power:** 3S or 4S LiPo battery with BEC (5V switching regulator)

### Optional (Flight Computer)

- **ESP32** for WiFi telemetry
- **Raspberry Pi Zero 2 W / Pi 4** for advanced autonomy
- **GPS module** for waypoint navigation
- **Camera** for vision processing

See [docs/BEC_Wiring.md](./docs/BEC_Wiring.md) for power system setup.

---

## Related Projects

- **[engineering360](https://github.com/yourusername/engineering360)** - Physical design, calculators, optimization
- **[dRehmFlight](https://github.com/nickrehm/dRehmFlight)** - Original flight controller firmware

---

## Contributing

Contributions welcome! Focus areas:

- Firmware bug fixes and features
- Sensor driver development
- Flight computer integration
- Documentation improvements
- Testing and validation

---

## License

TBD - Likely MIT or GPL to maintain compatibility with dRehmFlight

---

## Acknowledgments

- **Nick Rehm** - Original dRehmFlight flight controller
- Community contributors

---

**Last Updated:** 2026-01-11
