# floppi Development Roadmap

## Project Vision

Develop open-source flight controller firmware for drone platforms of all sizes. floppi is a firmware-only project focused on real-time flight control, sensor fusion, and autonomous navigation software.

**All physical design, component selection, and performance calculations are in [engineering360](https://github.com/yourusername/engineering360).**

---

## Project Overview

### 1. Teensy Flight Controller (dRehmFlight-based)

**Status:** In Progress
**Repository:** `/flight_controller/`
**Base System:** [dRehmFlight](https://github.com/nickrehm/dRehmFlight)

A refactored and modularized version of dRehmFlight optimized for PlatformIO development with support for multiple receiver protocols and IMU configurations.

### 2. Flight Computer Integration

**Status:** Planned
**Target Hardware:** ESP32 / Raspberry Pi

High-level control system providing WiFi connectivity, autonomous navigation, and swarm coordination capabilities.

### 3. Supporting Documentation

**Status:** Ongoing
**Repository:** `/docs/`

Comprehensive firmware setup guides, wiring diagrams, calibration procedures, and API documentation.

---

## Development Phases

## Phase 1: Foundation - Flight Controller Core

**Dependencies:** None
**Goal:** Establish reliable, flight-tested control system

### Milestones

#### 1.1 Hardware Configuration & Testing

- [x] Refactor dRehmFlight to PlatformIO project structure
- [x] Implement modular receiver support (SBUS/iBUS/DSM)
- [x] Configure pin definitions for Teensy 4.0/4.1
- [x] Integrate IMU support (MPU6050/MPU9250)
- [ ] Physical hardware bench testing with receiver
- [ ] IMU calibration and sensor fusion validation
- [ ] Validate SBUS communication with FS-iA6B receiver

#### 1.2 Power System Integration

- [x] Document BEC wiring requirements (linear regulator limitations identified)
- [x] Identify need for switching BEC for efficiency
- [ ] Test power distribution with 3S/4S LiPo configurations
- [ ] Validate voltage regulation under load
- [ ] Implement voltage monitoring and low-battery warnings

#### 1.3 Motor Control & ESC Calibration

- [ ] Configure PWM output for ESC control
- [ ] Implement ESC calibration routine
- [ ] Test throttle response and motor synchronization
- [ ] Validate failsafe and throttle cut functionality
- [ ] Bench test with motors (no propellers)

#### 1.4 Control Loop Tuning

- [ ] Implement rate controller (acro mode)
- [ ] Initial PID gain tuning via simulation or bench testing
- [ ] Test control mixer for quadcopter X configuration
- [ ] Validate Madgwick filter for attitude estimation
- [ ] Document baseline PID parameters

#### 1.5 First Flight Testing

- [ ] Tethered hover tests
- [ ] Manual control validation (rate mode)
- [ ] Flight dynamics characterization
- [ ] PID tuning refinement based on flight data
- [ ] Stability and handling assessment

**Completion Criteria:**

- Stable hover capability in rate (acro) mode
- Reliable receiver communication with failsafe
- Consistent motor response and control authority
- Documented baseline configuration and PID gains

---

## Phase 2: Advanced Flight Control

**Dependencies:** Phase 1 complete
**Goal:** Enhanced stability and autonomous capability

### Milestones

#### 2.1 Angle Mode Implementation

- [ ] Implement angle (stabilize) controller
- [ ] Tune angle mode PID loops
- [ ] Test self-leveling behavior
- [ ] Validate angle limits and control authority
- [ ] Flight testing and refinement

#### 2.2 Altitude Hold & Position Control

- [ ] Integrate barometer for altitude sensing (BMP280/MS5611)
- [ ] Implement altitude hold controller
- [ ] Add optical flow or GPS for position estimation
- [ ] Develop position hold capability
- [ ] Test hover stability in varying conditions

#### 2.3 Advanced Sensor Integration

- [ ] Integrate additional IMU for redundancy
- [ ] Add magnetometer for heading reference (HMC5883L/QMC5883L)
- [ ] Implement sensor fusion improvements
- [ ] Add lidar/ultrasonic for ground proximity
- [ ] Validate multi-sensor data fusion

#### 2.4 Flight Modes & Safety Features

- [ ] Implement multiple flight mode switching
- [ ] Add return-to-home (RTH) functionality
- [ ] Enhance failsafe behaviors (land, RTH, hover)
- [ ] Implement geofencing
- [ ] Add pre-flight checklist and arming safety

**Completion Criteria:**

- Multiple stable flight modes (rate, angle, altitude hold)
- Robust failsafe and safety features
- Multi-sensor fusion for improved state estimation
- Foundation for autonomous flight operations

---

## Phase 3: Flight Computer Integration - ESP32

**Dependencies:** Phase 1-2 complete
**Goal:** WiFi connectivity and telemetry

### Milestones

#### 3.1 Communication Architecture

- [ ] Define Teensy ↔ Flight Computer protocol
- [ ] Implement serial/UART communication interface
- [ ] Design message format and command structure
- [ ] Add error handling and message validation
- [ ] Test communication reliability and latency

#### 3.2 ESP32 Hardware Integration

- [ ] Select ESP32 module and development board
- [ ] Implement WiFi access point or station mode
- [ ] Create web interface for configuration and monitoring
- [ ] Add telemetry streaming over WiFi
- [ ] Implement remote control via web interface or UDP

#### 3.3 Ground Control Station (GCS) Communication

- [ ] Define telemetry message format (MAVLink-compatible or custom)
- [ ] Implement real-time telemetry streaming
- [ ] Add bidirectional command/control interface
- [ ] Implement data logging to SD card or flash
- [ ] Firmware update over WiFi (OTA)

#### 3.4 Basic Autonomous Features

- [ ] GPS integration (UBLOX or NMEA)
- [ ] Simple waypoint navigation
- [ ] Automatic takeoff and landing
- [ ] Return-to-home with GPS
- [ ] Mission abort and failsafe coordination

**Completion Criteria:**

- Reliable Teensy ↔ ESP32 communication
- WiFi telemetry with <100ms latency
- Web-based monitoring and control interface
- GPS-based waypoint navigation
- Documented communication protocol

---

## Phase 4: Flight Computer Integration - Raspberry Pi

**Dependencies:** Phase 3 complete
**Goal:** Advanced computing for vision and autonomy

### Milestones

#### 4.1 Raspberry Pi Hardware Setup

- [ ] Evaluate RPi Zero 2 W vs RPi 4 for weight/performance
- [ ] Implement UART/Serial communication with Teensy
- [ ] Power supply integration (5V rail from BEC)
- [ ] Add WiFi or RF telemetry link
- [ ] SD card management and logging

#### 4.2 Vision System Integration

- [ ] Camera module integration (Pi Camera or USB webcam)
- [ ] Implement video streaming over WiFi
- [ ] Object detection using OpenCV or TensorFlow Lite
- [ ] Visual tracking and following
- [ ] Optical flow for position estimation

#### 4.3 Advanced Autonomous Navigation

- [ ] GPS waypoint mission planning
- [ ] Path planning and obstacle avoidance
- [ ] SLAM for indoor/GPS-denied environments (future)
- [ ] Autonomous landing on visual markers
- [ ] Dynamic re-planning and replanning

#### 4.4 Ground Control Station Development

- [ ] Desktop GCS application (Python/Qt or web-based)
- [ ] Real-time telemetry visualization
- [ ] Mission planning and waypoint editing
- [ ] Log file analysis and replay
- [ ] Parameter tuning interface

**Completion Criteria:**

- Raspberry Pi running autonomous navigation stack
- Vision-based object detection and tracking
- Advanced waypoint navigation with obstacle avoidance
- Functional ground control station for mission management
- Comprehensive documentation and setup guides

---

## Phase 5: Swarm Coordination (Future)

**Dependencies:** Phase 4 complete
**Goal:** Multi-drone coordination capabilities

### Milestones

#### 5.1 Swarm Communication Protocol

- [ ] Define peer-to-peer or centralized coordination architecture
- [ ] Implement RF mesh or WiFi-based inter-drone communication
- [ ] Add time synchronization between drones
- [ ] Implement leader-follower or distributed consensus
- [ ] Test communication range and reliability

#### 5.2 Formation Flying

- [ ] Implement relative positioning between drones
- [ ] Maintain formation during flight
- [ ] Dynamic formation reconfiguration
- [ ] Handle drone failures or dropouts
- [ ] Validate with 2-3 drone tests

#### 5.3 Collision Avoidance

- [ ] Implement collision detection algorithms
- [ ] Add avoidance maneuvers
- [ ] Test with simulated and real scenarios
- [ ] Validate safety margins and reaction time
- [ ] Demonstrate safe operation with 3+ drones

#### 5.4 Cooperative Task Execution

- [ ] Task allocation and distribution
- [ ] Coordinated search and coverage
- [ ] Cooperative payload transport (stretch goal)
- [ ] Autonomous mission coordination
- [ ] Demonstrate multi-drone missions

**Completion Criteria:**

- Demonstrated swarm flight with 3+ drones
- Robust collision avoidance
- Coordinated task execution
- Comprehensive swarm API and documentation

---

## Phase 6: Advanced Features & Optimization

**Dependencies:** Phase 1-5 as needed
**Goal:** Refine firmware and expand capabilities

### Milestones

#### 6.1 Firmware Optimization

- [ ] Performance profiling and optimization
- [ ] Reduce latency in control loops
- [ ] Optimize sensor reading and filtering
- [ ] Memory usage optimization
- [ ] Power consumption reduction

#### 6.2 Alternative Hardware Platforms

- [ ] Port firmware to ESP32 (standalone flight controller)
- [ ] Support additional IMUs (ICM-20948, BMI088)
- [ ] Add support for different microcontrollers
- [ ] Test on alternative receiver protocols
- [ ] Document platform-specific configurations

#### 6.3 Advanced Control Features

- [ ] DShot ESC protocol implementation
- [ ] OneShot125/OneShot42 support
- [ ] Adaptive PID tuning (auto-tune)
- [ ] Acrobatic flight modes (flip, roll)
- [ ] Rate limiting and expo curves

#### 6.4 Testing & Validation Framework

- [ ] Automated unit testing for firmware modules
- [ ] Hardware-in-the-loop (HIL) simulation
- [ ] Software-in-the-loop (SIL) simulation
- [ ] Continuous integration (CI) pipeline
- [ ] Flight test data analysis tools

#### 6.5 Community & Ecosystem

- [ ] Comprehensive wiki or documentation site
- [ ] Video tutorials for setup and tuning
- [ ] GitHub discussions or forum
- [ ] Accept community contributions and pull requests
- [ ] Publish research papers or blog posts

**Completion Criteria:**

- Optimized firmware with reduced latency
- Support for multiple hardware platforms
- Advanced control features demonstrated
- Active community engagement and contributions
- Recognition as viable open-source flight controller

---

## Cross-Cutting Concerns

### Documentation & Knowledge Management

**Ongoing throughout all phases**

- Maintain `/docs/flight-controller/` and `/docs/flight-computer/` documentation
- Document design decisions and lessons learned
- Create tutorials and getting-started guides
- Keep wiring diagrams and feature comparisons updated
- API documentation for flight computer integration

### Testing & Validation

**Continuous process**

- Implement automated testing for firmware code
- Develop simulation environment for pre-flight testing
- Create test procedures and checklists
- Log all flight data for post-flight analysis
- Validate all features before release

### Safety & Compliance

**Critical at all stages**

- Follow RC aircraft safety guidelines
- Implement redundant failsafes
- Test all safety features before flight
- Comply with local drone regulations (FAA Part 107 for US)
- Document safety procedures and risk mitigations

### Version Control & Releases

**Ongoing**

- Tag stable releases at major milestones
- Maintain CHANGELOG for version history
- Use semantic versioning (vX.Y.Z)
- Create release notes with upgrade instructions
- Binary releases for common configurations

---

## Technology Stack

### Flight Controller (Teensy)

- **Hardware:** Teensy 4.0/4.1 (ARM Cortex-M7 @ 600 MHz)
- **Framework:** Arduino (PlatformIO)
- **IMU:** MPU6050, MPU9250, ICM-20948
- **Receiver:** SBUS (FS-iA6B), iBUS, DSM supported
- **ESC Protocol:** PWM (OneShot/DShot planned)

### Flight Computer

- **ESP32:** WiFi/Bluetooth connectivity, web interface, lightweight autonomy
- **Raspberry Pi:** Advanced computing, vision, GPS, SLAM

### Development Tools

- **IDE:** PlatformIO (VS Code)
- **Version Control:** Git
- **Testing:** Unit tests, HIL/SIL simulation
- **Documentation:** Markdown, Doxygen

---

## Success Metrics

### Phase 1-2 (Flight Controller)

- Stable hover for 5+ minutes
- Successful outdoor flight in light wind (<10 mph)
- No crashes due to control system failures
- PID tuning documented and reproducible

### Phase 3 (ESP32 Integration)

- Telemetry latency <100ms over WiFi
- Reliable communication over 100m range
- Web interface accessible and functional
- GPS waypoint navigation demonstrated

### Phase 4 (Raspberry Pi)

- Vision-based object tracking at 10+ fps
- Autonomous waypoint navigation with <2m accuracy
- Obstacle avoidance demonstrated in test scenarios
- GCS successfully used for mission planning

### Phase 5 (Swarm)

- Formation flight with 3+ drones
- No collisions during swarm tests
- Coordinated task execution demonstrated
- Robust to single drone failure

### Phase 6 (Advanced Features)

- Optimized firmware with measurable performance gains
- Support for 3+ hardware platforms
- Active community with regular contributions
- 100+ successful firmware deployments

---

## Risk Management

### Technical Risks

- **IMU drift/noise:** Mitigate with sensor fusion, magnetometer, external reference (GPS/optical flow)
- **Motor/ESC synchronization issues:** Use quality components, proper calibration, consider DShot protocol
- **Communication latency/dropouts:** Implement robust failsafe, redundant communication channels
- **Flight computer compute limitations:** Optimize algorithms, consider more powerful hardware

### Development Risks

- **Scope creep:** Prioritize core functionality, defer nice-to-have features
- **Component availability:** Identify alternative parts, design for common components
- **Testing limitations:** Start with tethered tests, use simulation, incremental flight testing
- **Documentation debt:** Document as you go, use templates, automate where possible

### Safety Risks

- **Flight testing accidents:** Follow safety protocols, use test stands, fly in open areas, wear eye protection
- **Battery fires:** Use LiPo-safe charging/storage, monitor cell voltages, dispose of damaged batteries properly
- **Regulatory compliance:** Research local laws, obtain certifications if required, stay under weight limits

---

## Resources & References

### Core Documentation

- `/docs/flight-controller/` - Firmware setup and configuration
- `/docs/flight-computer/` - Flight computer integration guides
- `/docs/BEC_WIRING.md` - Power system wiring guide
- `/docs/FEATURE_COMPARISON.md` - Flight controller feature analysis

### Key Repositories

- [dRehmFlight (Original)](https://github.com/nickrehm/dRehmFlight) - Base flight controller
- [floppi Repository](https://github.com/yourusername/floppi) - This project (firmware)
- [engineering360 Repository](https://github.com/yourusername/engineering360) - Physical design companion project

### External Resources

- **PlatformIO Documentation:** [platformio.org](https://platformio.org)
- **Teensy Documentation:** [pjrc.com/teensy](https://pjrc.com/teensy)
- **MAVLink Protocol:** [mavlink.io](https://mavlink.io)
- **ArduPilot (reference):** [ardupilot.org](https://ardupilot.org)

### Community & Support

- GitHub Issues - Bug reports and feature requests
- GitHub Discussions - Questions and design discussions
- Discord/Slack (TBD) - Real-time community chat

---

## Relationship to engineering360

**floppi = firmware only. engineering360 = hardware only.**

### What lives in engineering360:
- Frame design and CAD modeling
- Motor, propeller, and ESC selection
- Battery sizing and power system design
- Propulsion system optimization
- VTOL performance calculators
- Structural analysis and material selection
- Weight optimization and balance calculations
- Component sourcing (BOM)
- Assembly instructions

### What lives in floppi:
- Flight controller firmware (C/C++)
- PID control algorithms
- Sensor fusion and filtering
- Flight modes and autonomy
- Communication protocols
- Flight computer integration software

### Workflow:
1. **engineering360** designs physical platform and selects all components
2. **engineering360** provides specs: mass, inertia, motor KV, propeller size, etc.
3. **floppi** configures firmware for those specifications
4. **floppi** tunes PID controllers for the physical characteristics
5. Flight testing provides real-world performance data
6. **engineering360** iterates on physical design based on flight data
7. Loop back to step 2

---

## Contributing

This is an open-source project, and contributions are welcome at all phases. Areas where community input is especially valuable:

- **Testing:** Flight testing on different hardware configurations
- **Documentation:** Tutorials, troubleshooting guides, translations
- **Code:** Bug fixes, new receiver protocols, alternative IMU support
- **Firmware Features:** New flight modes, sensor drivers, control algorithms
- **Flight Computer:** Autonomous navigation, vision processing, GCS development

See `CONTRIBUTING.md` (TBD) for guidelines on submitting pull requests and reporting issues.

---

## License

Project licensing TBD. Likely MIT or GPL to maintain open-source compatibility with dRehmFlight base code.

---

## Changelog

**2026-01-11:** Roadmap refactored to focus on firmware and flight computer integration

- Removed physical design, VTOL calculator, and literature/RAG phases (moved to engineering360)
- Clarified firmware-focused development phases
- Added clearer separation between flight controller and flight computer work
- Emphasized relationship to engineering360 for physical design

**2026-01-11:** Initial roadmap created

- Defined 6-phase development plan
- Documented current progress on flight controller
- Outlined dependencies and success metrics

---

**Last Updated:** 2026-01-11
**Roadmap Version:** 2.0
**Project Status:** Phase 1 in progress
