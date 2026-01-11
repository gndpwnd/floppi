# Project Scope: floppi

## Mission Statement

floppi is an open-source flight controller firmware platform for drone systems. This project focuses exclusively on flight controller firmware development, sensor integration, real-time control systems, and flight computer integration. All physical design, mechanical optimization, aerodynamic calculations, and component selection are handled by the companion [engineering360](https://github.com/yourusername/engineering360) project.

## Core Purpose

floppi implements flight controller firmware for real-time flight stabilization, autonomous control, and sensor fusion. The project provides the software brain that stabilizes drones, processes sensor data, and executes flight commands. This is a firmware-only project - all hardware design, frame construction, and propulsion calculations exist in the engineering360 repository.

## Target Platforms

floppi firmware is designed to run on microcontroller-based flight controllers across various scale ranges. The firmware adapts to different physical platforms designed in engineering360.

### Supported Microcontrollers
- **Teensy 4.0/4.1** - Primary development platform (ARM Cortex-M7 @ 600 MHz)
- **ESP32** - WiFi-enabled flight controller (future support)
- **Other ARM platforms** - Via PlatformIO configuration

### Scale Range Support
The firmware is scale-agnostic and supports platforms from micro (<250g) to heavy-lift cargo drones through configuration and PID tuning. Physical platform specifications are defined in engineering360.

---

## Core Focus Areas

### 1. Flight Controller Firmware (PRIMARY FOCUS)

#### In Scope:
- **dRehmFlight-based firmware** development and enhancement
- **PID control loops** for stabilization and attitude control
- **Sensor integration:**
  - IMU (MPU6050, MPU9250, ICM-20948)
  - Barometer (BMP280, MS5611)
  - Magnetometer (HMC5883L, QMC5883L)
  - GPS modules (UBLOX, NMEA)
  - Optical flow sensors
  - Lidar/ultrasonic rangefinders
- **Radio receiver protocols:**
  - SBUS (primary)
  - iBUS
  - DSM/DSMX
  - PPM
- **Motor control:**
  - PWM ESC output
  - OneShot125/OneShot42 (future)
  - DShot (future)
- **Flight modes:**
  - Rate/Acro mode
  - Angle/Stabilize mode
  - Altitude hold
  - Position hold
  - Return-to-home (RTH)
- **Failsafe systems** and safety protocols
- **Sensor fusion algorithms** (Madgwick, Mahony, Kalman filters)
- **PlatformIO-based development** environment
- **Hardware abstraction** for multiple microcontroller platforms

#### Key Technologies:
- **Microcontrollers:** Teensy 4.0/4.1, ESP32
- **Development Environment:** PlatformIO (Arduino framework)
- **Communication:** I2C, SPI, UART, Serial
- **Control:** PWM output, interrupt-based timing

---

### 2. Flight Computer Integration (SECONDARY FOCUS)

#### In Scope:
- **Communication protocols** between flight controller and flight computer
- **Serial/UART messaging** for telemetry and commands
- **WiFi connectivity** via ESP32 flight computer
- **Telemetry streaming** to ground control stations
- **High-level autonomous control:**
  - Waypoint navigation
  - Mission planning execution
  - GPS-based navigation
- **Vision system integration:**
  - Camera interface (Raspberry Pi)
  - Object detection/tracking
  - Optical navigation
- **Multi-drone coordination** (swarm capabilities)
- **Ground control station (GCS) communication**

#### Supported Flight Computers:
- **ESP32:** WiFi telemetry, web interface, lightweight autonomy
- **Raspberry Pi Zero 2 W / Pi 4:** Advanced computing, vision, SLAM
- **Jetson Nano (future):** AI/ML-based control and perception

#### Communication Architecture:
- **Teensy (flight controller)** ↔ **Flight Computer** via UART/Serial
- **Flight Computer** ↔ **Ground Station** via WiFi/RF link
- Message format: Structured binary or text-based protocols
- Error handling and message validation

---

### 3. Firmware Testing and Validation

#### In Scope:
- **Bench testing frameworks** for hardware validation
- **IMU calibration procedures** and tools
- **ESC calibration** and motor response testing
- **Sensor validation** (accelerometer, gyro, magnetometer)
- **Control loop testing** (PID tuning, stability analysis)
- **Flight testing procedures** and safety checklists
- **Data logging** for post-flight analysis
- **Simulation integration** (future: software-in-the-loop, hardware-in-the-loop)

#### Testing Phases:
1. **Component validation:** Individual sensor and actuator tests
2. **Integration testing:** Combined system bench tests
3. **Tethered flight tests:** Constrained hover and control validation
4. **Free flight tests:** Progressive envelope expansion
5. **Autonomous flight tests:** Waypoint navigation and advanced features

---

### 4. Documentation and Developer Resources

#### In Scope:
- **Firmware setup guides** and installation instructions
- **Hardware wiring diagrams** for flight controllers
- **Calibration procedures** (IMU, ESC, radio)
- **Configuration guides** for different drone types
- **Troubleshooting documentation** and debugging tips
- **API/protocol documentation** for flight computer integration
- **Code documentation** (Doxygen-style comments)
- **Example configurations** for common builds

#### Documentation Structure:
- `/docs/flight-controller/` - Flight controller firmware documentation
- `/docs/flight-computer/` - Flight computer integration guides
- `/docs/*.md` - Hardware wiring, power systems, feature comparisons

---

## What's In Scope

### Firmware Development
- Flight controller firmware implementation
- Control algorithm development and optimization
- Sensor driver development
- Communication protocol implementation
- Safety and failsafe logic
- Real-time embedded systems programming

### Flight Computer Integration
- High-level autonomy software
- Telemetry and command interfaces
- Ground control station communication
- Vision system integration
- Multi-drone coordination

### Testing and Validation
- Bench testing procedures
- Flight testing protocols
- Data logging and analysis
- Calibration tools

### Developer Tools
- Build system configuration (PlatformIO)
- Debugging utilities
- Configuration tools
- Log analysis scripts

---

## What's Out of Scope

### Physical Design and Mechanical Engineering (ALL → engineering360)
- 3D modeling and CAD design
- Frame design and structural analysis
- Motor and propeller selection
- Battery selection and power calculations
- Aerodynamic optimization
- Weight and balance calculations
- Material selection and stress analysis
- VTOL performance modeling
- Lift-to-weight ratio optimization
- Component sourcing and bill of materials (BOM)
- Physical assembly instructions
- Mechanical testing procedures

### Manufacturing and Production
- Mass production facilities
- Commercial assembly services
- Supply chain management
- Large-scale manufacturing

### Commercial Applications
- Paid service delivery platforms
- Commercial operations management
- Enterprise fleet management
- Revenue-generating services

### Regulatory Compliance Services
- FAA certification consulting
- Legal advisory services
- Insurance and liability services
- Regulatory paperwork processing

### Consumer Products
- Plug-and-play commercial drones
- Warranty and customer support
- End-user mobile applications
- Marketing and sales

### Non-Aviation Systems
- Ground robots or wheeled vehicles
- Marine vessels (unless aerial-aquatic hybrid)
- Fixed-wing aircraft (unless hybrid VTOL)
- General-purpose robotics platforms

---

## Relationship to engineering360

**engineering360** handles all physical design and component selection. **floppi** implements the firmware to control those designs.

### Clear Division:

| Aspect | floppi (firmware) | engineering360 (hardware) |
|--------|-------------------|---------------------------|
| **Focus** | Flight control algorithms, sensor fusion, real-time systems | Frame design, propulsion selection, structural analysis |
| **Outputs** | Compiled firmware (.hex/.bin files), PID configurations | CAD models, component BOMs, performance calculations |
| **Inputs** | Physical parameters (mass, inertia, motor specs) | Flight test data, control requirements |
| **Tools** | PlatformIO, C/C++, Git | FreeCAD, Python calculators, spreadsheets |

### Workflow:
1. **engineering360** designs physical drone platform and selects components
2. **engineering360** provides specifications to floppi (mass, inertia, motor characteristics)
3. **floppi** configures firmware parameters and tunes PID controllers
4. **floppi** provides flight-ready firmware
5. Flight testing generates data
6. **engineering360** uses flight data to refine physical design
7. Iterate

**Repository Link:** [engineering360](https://github.com/yourusername/engineering360)

---

## Target Audience

### Primary Users

#### Firmware Developers
- Embedded systems programmers
- Flight control algorithm researchers
- Sensor fusion engineers
- Real-time systems developers

#### Drone Builders
- DIY drone builders needing custom firmware
- Researchers building experimental platforms
- Competition teams (DARPA Lift, university competitions)
- FPV pilots wanting advanced control features

#### Robotics Engineers
- Autonomous systems researchers
- Multi-agent coordination developers
- Vision-based navigation engineers
- Swarm robotics researchers

### Secondary Users

#### Educators
- University professors teaching embedded systems
- Robotics course instructors
- STEM educators running drone workshops

#### Hobbyists
- Electronics enthusiasts exploring drone firmware
- Makers with microcontroller experience
- Tinkerers wanting to understand flight control

---

## Technical Prerequisites

### Required Skills
- **C/C++ programming** (intermediate level)
- **Embedded systems fundamentals** (interrupts, timers, serial communication)
- **Basic electronics** (reading schematics, using multimeter)
- **Command-line proficiency** (Linux/Unix preferred)
- **Git version control** basics

### Recommended Skills
- **PID control theory** understanding
- **Sensor fusion concepts** (complementary filters, Kalman filters)
- **Real-time systems** experience
- **Python** for scripting and tools (flight computer integration)
- **Soldering** and hardware debugging

### Hardware Access
- **Microcontroller:** Teensy 4.0/4.1 or ESP32 dev board
- **IMU:** MPU6050 or MPU9250 breakout board
- **RC receiver:** SBUS-capable (e.g., FlySky FS-iA6B)
- **Basic tools:** Soldering iron, multimeter, wire strippers
- **Programmer:** USB cable (Teensy has built-in bootloader)

---

## Project Structure

### Core Repositories

#### `/flight_controller/`
Flight controller firmware (dRehmFlight-based)
- PlatformIO project structure
- Sensor drivers and calibration
- PID control loops
- Radio receiver interface
- Motor control and mixing
- Failsafe and safety features

#### `/flight_computer/`
Flight computer integration software
- ESP32 WiFi telemetry
- Raspberry Pi autonomous navigation
- Communication protocols
- Ground control station interface
- Swarm coordination (future)

#### `/docs/`
Documentation and guides
- **`/docs/flight-controller/`** - Firmware documentation
- **`/docs/flight-computer/`** - Flight computer guides
- **`/docs/scope.md`** - This document
- **`/docs/ROADMAP.md`** - Development roadmap
- Wiring diagrams and setup guides

#### `/tools/`
Development utilities
- Calibration tools
- Log analysis scripts
- Configuration generators
- Testing utilities

---

## Collaboration Model

### Open Source Philosophy
- **License:** MIT or GPL (compatible with dRehmFlight)
- **Contributions:** Pull requests welcome
- **Issue tracking:** GitHub Issues for bugs and features
- **Transparent development:** Design decisions documented publicly

### Knowledge Sharing
- Research findings shared openly
- Failed experiments documented
- Design rationale explained
- Community discussions encouraged

### Attribution
- Original dRehmFlight work credited
- Contributors recognized
- Academic citations provided

---

## Success Metrics

### Technical Goals
- **Stable flight** across all scale categories
- **Modular, maintainable** codebase
- **Comprehensive documentation** for developers
- **Active community** engagement and contributions

### Community Goals
- Enable **100+ successful firmware deployments**
- Support **academic research** publications
- Foster **collaborative development**
- Grow diverse **contributor base**

### Innovation Goals
- Advance **open-source flight control** capabilities
- Demonstrate **novel sensor fusion** approaches
- Enable **multi-drone coordination** research
- Contribute to **drone firmware ecosystem**

---

## Future Directions

### Near-Term (6-12 months)
- Complete **Phase 1-2 flight controller** development (see ROADMAP.md)
- Implement **ESP32 flight computer** integration
- Expand **sensor support** (GPS, barometer, magnetometer)
- Improve **documentation coverage**

### Mid-Term (1-2 years)
- **Raspberry Pi** flight computer integration
- **Autonomous waypoint navigation**
- **Basic swarm coordination** (2-3 drones)
- **Comprehensive testing framework**

### Long-Term (2+ years)
- **Advanced swarm capabilities** (10+ drones)
- **Vision-based navigation** and obstacle avoidance
- **Machine learning integration** for adaptive control
- **ROS2 integration** for research platforms

---

## Contact and Community

### Getting Started
1. Read `/docs/flight-controller/` documentation
2. Review hardware setup guides
3. Join GitHub Discussions
4. Start with micro/mini builds before large platforms

### Contributing
- Report bugs via GitHub Issues
- Submit pull requests with clear descriptions
- Document new features thoroughly
- Follow existing code style

### Support
- **GitHub Issues:** Technical questions and bug reports
- **Documentation:** Check `/docs/` first
- **Community-driven:** No paid support

---

## Relationship Summary

| **Aspect** | **floppi (firmware)** | **engineering360 (hardware)** |
|------------|----------------------|-------------------------------|
| **Focus** | Real-time control algorithms, sensor fusion | Physical design, component selection, performance modeling |
| **Outputs** | Compiled firmware, PID configs, telemetry protocols | CAD files, BOMs, assembly instructions, performance specs |
| **Primary Skills** | Embedded C/C++, control theory, real-time systems | Mechanical engineering, aerodynamics, structural analysis |
| **Tools** | PlatformIO, GCC, Git | FreeCAD, Python calculators, spreadsheets |
| **Documentation** | Firmware setup, PID tuning, sensor calibration | Component selection rationale, performance calculations |
| **Testing** | Bench tests, flight tests, sensor validation | Structural simulations, thrust calculations, weight estimates |

**Both projects are required for complete, flyable drone systems.**

---

**Version:** 2.0
**Last Updated:** 2026-01-11
**Maintainers:** floppi community

This scope document is a living document and will evolve as the project grows. Feedback and suggestions are welcome.
