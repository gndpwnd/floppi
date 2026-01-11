# Flight Controller Project Scope

## Overview

This project implements a Teensy-based flight controller that serves as the foundation for all drone platforms in this repository. It is built upon the dRehmFlight flight controller software, with enhanced capabilities for modern sensor integration and ease of use.

## Base Platform

### dRehmFlight Foundation
- Established Arduino/Teensy flight controller software by Nicholas Rehm
- Proven architecture for VTOL and multirotor applications
- Madgwick filter for attitude estimation
- Support for multiple receiver protocols and IMU sensors
- Configurable for various aircraft types and configurations

### Teensy Microcontroller Platform
The project supports multiple Teensy boards:
- **Teensy 4.0** (Primary target) - ARM Cortex-M7 @ 600MHz
- **Teensy 4.1** - Extended capabilities with additional I/O
- **Teensy 3.6** - Legacy support for existing builds

These platforms provide:
- High-performance ARM processors with hardware floating-point
- Sufficient I/O for multiple PWM channels, serial interfaces, and sensors
- USB serial for configuration and debugging
- Real-time performance suitable for flight control loops

## Key Features

### Core Functionality
- **IMU Integration**: Support for MPU6050 (I2C) and MPU9250 (SPI) inertial measurement units
- **Radio Control Input**: Multiple receiver protocol support
  - PWM (individual channel wiring)
  - PPM (single wire, multiple channels)
  - SBUS (digital protocol via UART)
  - DSM/DSMX (Spektrum satellite receivers)
- **Motor Control**: PWM output for ESCs and servos
- **Sensor Fusion**: Madgwick or complementary filter for attitude estimation
- **PID Control**: Stabilization loops for roll, pitch, and yaw

### Primary Enhancement: Auto-Calibration

The key differentiator of this implementation is **automatic IMU calibration**:

#### Traditional Approach Limitations
- Manual calibration procedures requiring sensor to be stationary
- User must record and enter calibration offsets into configuration files
- Recalibration needed when sensors drift or environmental conditions change
- Error-prone process for inexperienced users

#### Auto-Calibration Goals
- **Startup Calibration**: Automatic gyroscope bias calculation at power-on
- **Accelerometer Calibration**: Simplified or automatic accelerometer offset and scale factor determination
- **Adaptive Calibration**: Optional runtime adjustment for sensor drift
- **User-Friendly**: Minimal user intervention required for accurate flight
- **Robust**: Handle various sensor units (MPU6050 variations, clones, genuine parts)

#### Calibration Algorithms
The auto-calibration system will implement:
1. **Gyroscope Zero-Rate Offset**: Statistical analysis of stationary readings
2. **Accelerometer Offset Calibration**: Multi-position or single-position gravity reference
3. **Scale Factor Calibration**: Normalization against known gravity vector
4. **Validation**: Sanity checks to ensure calibration quality
5. **Storage**: Persistent calibration data (EEPROM) with validity checking

## Repository Integration

### Foundation for All Drone Platforms
This flight controller serves as the core control system for:
- Fixed-wing aircraft
- Multirotors (quadcopters, hexacopters)
- VTOL (vertical takeoff and landing) aircraft
- Hybrid configurations

### Shared Infrastructure
- Common sensor interfaces
- Standardized configuration approach
- Reusable calibration routines
- Platform-specific tuning overlays

## Technical Stack

### Build System
- **PlatformIO**: Modern build system and library management
- Cross-platform development (Linux, Windows, macOS)
- Automated dependency resolution
- Multiple board target support

### Programming Language
- C++ (Arduino framework)
- Hardware abstraction for portability
- Performance-critical sections optimized for ARM Cortex-M7

### Libraries and Dependencies
- Wire (I2C communication)
- SPI (SPI communication)
- PWMServo (Teensy-specific PWM library)
- Receiver protocol libraries (SBUS, DSM)
- IMU driver libraries (MPU6050, MPU9250)

## Hardware Requirements

### Minimum Components
- Teensy 4.0, 4.1, or 3.6 microcontroller
- IMU sensor (MPU6050 or MPU9250)
- Radio receiver (any supported protocol)
- ESCs and motors appropriate for aircraft
- Power distribution system (BEC for 5V/3.3V logic)

### Recommended Setup
- Teensy 4.0 or 4.1 (better performance)
- MPU6050 IMU (widely available, well-supported)
- SBUS receiver (clean digital protocol, single wire)
- Appropriate power module with filtering

## Configuration Approach

### Hardware Configuration
- Receiver type selection (PWM/PPM/SBUS/DSM)
- IMU type selection (MPU6050/MPU9250)
- Sensor sensitivity ranges (gyro, accelerometer)
- Pin assignments for I/O

### Flight Configuration
- Aircraft type (multirotor, fixed-wing, VTOL)
- PID tuning parameters
- Control mixing matrices
- Failsafe behavior

### Calibration Data
- IMU offsets and scale factors (auto-generated)
- Radio channel mapping and endpoints
- ESC/servo endpoints and reversing

## Success Criteria

The flight controller implementation will be considered successful when:

1. **Reliable IMU Auto-Calibration**: System automatically calibrates sensors without user intervention, producing flight-ready results
2. **Multi-Sensor Support**: Works reliably with various MPU6050 and MPU9250 units (genuine and clone)
3. **Radio Integration**: Successfully receives and processes commands from all supported receiver types
4. **Stable Flight**: Achieves stable flight with properly tuned PID parameters
5. **Documentation**: Clear setup instructions and example configurations available
6. **Reusability**: Serves as foundation for multiple aircraft platforms in repository

## Out of Scope

The following items are explicitly excluded from this project scope:

- GPS navigation and waypoint following
- Optical flow or visual odometry
- Autonomous mission planning
- Ground control station software
- Custom PCB design (uses off-the-shelf Teensy boards)
- Wireless telemetry (may be added later)
- Advanced features like object avoidance or computer vision

## Future Considerations

Potential enhancements beyond initial scope:
- Magnetometer calibration and integration
- Barometer/altimeter integration for altitude hold
- GPS integration for position hold and return-to-home
- Telemetry output (MAVLink protocol)
- In-flight calibration refinement
- Black box logging to SD card (Teensy 4.1)
