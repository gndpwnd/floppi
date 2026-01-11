# Reference Platform Scope

## Overview

The reference platform is a generic optimized drone platform designed to serve as a proof-of-concept demonstrating that the VTOL calculator works in real-world applications. This project validates the optimization algorithms with actual hardware and provides a reference design for others to build upon.

## Primary Objectives

### 1. Generic Optimized Drone Platform
- Design a versatile quadcopter platform suitable for general-purpose use
- Implement optimizations derived from the VTOL calculator
- Create a platform that balances performance, efficiency, and practicality
- Ensure design is accessible and reproducible by others

### 2. Proof-of-Concept for VTOL Calculator
- Demonstrate that calculator predictions translate to real-world performance
- Validate the accuracy of thrust, power, and efficiency calculations
- Prove the optimization algorithms produce viable component selections
- Test theoretical models against measured flight data

### 3. Teensy Flight Controller Integration
- Utilize the custom Teensy-based flight controller developed for this project
- Validate flight control algorithms in real flight conditions
- Demonstrate the capability of the Teensy platform for drone applications
- Establish baseline performance for the flight controller design

### 4. Algorithm Validation with Real Hardware
- Compare predicted performance metrics to actual measurements
- Identify discrepancies between theoretical models and reality
- Validate optimization objectives (thrust-to-weight, efficiency, flight time)
- Quantify accuracy of component selection algorithms

### 5. Reference Design for Community
- Document complete build process and component selection rationale
- Provide reproducible design files and specifications
- Share lessons learned and best practices
- Enable others to build similar optimized platforms

## In Scope

- Complete quadcopter platform design and construction
- Component selection using VTOL calculator outputs
- Integration of Teensy flight controller
- Flight testing and performance measurement
- Data collection for calculator validation
- Documentation of design decisions and results
- Publishing reference design specifications

## Out of Scope

- Advanced autonomous features beyond basic stabilization
- Custom motor or ESC design (uses off-the-shelf components)
- Production-ready commercial product development
- Certification or regulatory compliance beyond hobby use
- Extensive payload-specific optimizations
- Long-range or specialized mission profiles

## Success Criteria

- Platform achieves stable flight with Teensy flight controller
- Measured performance metrics within acceptable margin of calculator predictions
- Complete documentation enables reproduction by others
- Identified improvements feed back into calculator development
- Design serves as validated reference for optimization methodology
