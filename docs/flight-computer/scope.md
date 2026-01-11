# Flight Computer Integration - Project Scope

## Overview

This project aims to integrate a WiFi-enabled flight computer (ESP32 or Raspberry Pi) with the Teensy-based flight controller to enable advanced command and control capabilities. This integration is separate from and complementary to the basic radio control system.

## Primary Objective

Enable swarm/flock control of multiple micro drones operating simultaneously in a shared indoor environment over a common WiFi network.

## Core Components

### Hardware Integration

#### Flight Computer Options
- **ESP32**: Lightweight, low-power WiFi-enabled microcontroller
  - Ideal for weight-constrained micro drone applications
  - Sufficient processing for command routing and basic coordination
  - Lower cost per unit for multi-drone deployments

- **Raspberry Pi**: Full Linux computer with extensive processing capabilities
  - Suitable for advanced onboard computation
  - Enables sophisticated autonomous behaviors
  - Higher power consumption and weight considerations

#### Connection to Teensy Flight Controller
- Establish reliable serial communication between flight computer and Teensy
- Flight computer acts as high-level command interface
- Teensy maintains low-level flight control and sensor fusion
- Clear separation of concerns between systems

### Functional Capabilities

#### WiFi-Based Command and Control
- Remote command interface over WiFi network
- Real-time telemetry streaming from drone to ground station
- Parameter adjustment and configuration updates
- Mission planning and execution

#### Complex Command Sequences
- Pre-programmed flight patterns and maneuvers
- Conditional logic and decision trees
- Waypoint navigation and path planning
- Synchronized multi-drone choreography

#### Multi-Drone Coordination
- Network discovery and identification of available drones
- Centralized or distributed command architecture
- Inter-drone communication for swarm behaviors
- Collision avoidance and spatial awareness

## Key Requirements

### Communication
- Robust serial protocol between flight computer and Teensy
- Low-latency command transmission
- Reliable WiFi connectivity in indoor environments
- Graceful handling of communication failures

### Safety
- Failsafe behavior when WiFi connection is lost
- Emergency stop capability
- Respect for basic radio control override
- Independent flight controller operation if flight computer fails

### Scalability
- Support for multiple drones on same network
- Efficient bandwidth utilization
- Unique addressing and identification
- Minimal interference between units

### Performance
- Minimal latency added to control loop
- Efficient message parsing and routing
- Suitable for real-time control applications
- Low power consumption (especially for ESP32)

## Out of Scope

- Replacement of basic radio control system
- GPS navigation and outdoor flight
- Computer vision and object detection (initial phase)
- Long-range communication (focused on indoor WiFi range)
- Autonomous obstacle avoidance using sensors

## Success Criteria

1. Successfully establish bidirectional communication between flight computer and Teensy
2. Send flight commands over WiFi and observe drone response
3. Execute complex pre-programmed sequences via WiFi interface
4. Demonstrate coordinated flight of 2+ drones in shared space
5. Maintain stable operation with graceful failure modes

## Future Expansion Possibilities

- Camera integration for FPV or computer vision
- Advanced sensor fusion with flight computer IMU
- Machine learning models for autonomous behaviors
- Outdoor operation with GPS integration
- Mesh networking for extended range
