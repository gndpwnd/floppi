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
   - dRehmFlight-based firmware for Teensy 4.0/4.1
   - PID control loops and stabilization
   - Sensor integration (IMU, GPS, barometer, magnetometer)
   - Multiple receiver protocols (SBUS, iBUS, DSM)
   - Flight modes: Rate, Angle, Altitude Hold, Position Hold

2. **Flight Computer Integration** (`/flight_computer/`)
   - ESP32 WiFi telemetry and control
   - Raspberry Pi autonomous navigation
   - Vision processing and object tracking
   - Swarm coordination (future)

3. **Documentation** (`/docs/`)
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
