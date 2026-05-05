# Archive: Project Context & Historical Documents

This folder contains original project context and early-stage code that informed the project setup.

## Contents

- **compere_init.md** — Dr. Comper's initial requirements email describing:
  - BNO085 sensor specifications and calibration needs
  - Ublox NEO-M9N GPS requirements
  - Expected output (quaternions, position, accuracy)
  - Magnetometer calibration persistence challenges
  
- **BN085_I2C_Adafruit.ino** — Initial Arduino sketch testing BNO085 connectivity
  - UART mode operation (P1 pin configuration)
  - Adafruit BNO08x library usage
  - Serves as reference for sensor initialization
  
- **project_init_prompt.md** — User's detailed project initialization request
  - Multi-project context (flight_controller, drone_3d_model, etc.)
  - Scope clarification for auto_orientation as primary focus
  - Setup preferences (PlatformIO, local libraries, Python monitoring)

## How These Informed Project Setup

The scope.md and roadmap.md were developed based on:

1. **Requirements** from compere_init.md:
   - v1.0 focus on BNO085 + NEO-M9N integration
   - Persistent calibration persistence as key feature
   - Quaternion + position output
   
2. **Architecture** informed by initial sketch:
   - UART mode operation confirmed
   - Adafruit library integration planned
   - Sensor abstraction layer designed to support future sensors
   
3. **Development approach** from user preferences:
   - PlatformIO as build system
   - Python serial monitoring tools
   - Local library cloning for field deployment
   - Modular project structure

## Reference for Future Work

- Review compere_init.md when:
  - Understanding GPS accuracy targets
  - Implementing magnetometer calibration persistence
  - Designing quaternion output format
  
- Review BN085_I2C_Adafruit.ino when:
  - Implementing BNO085 sensor driver
  - Debugging initialization issues
  - Setting up UART communication parameters

- Reference project_init_prompt.md for:
  - Integration points with other projects
  - Development workflow preferences
  - Hardware sensor list and combinations
