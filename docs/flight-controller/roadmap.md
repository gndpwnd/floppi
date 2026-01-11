# Flight Controller Development Roadmap

This roadmap outlines the logical progression of work to develop the Teensy-based flight controller with auto-calibration capabilities. Tasks are organized by phase and dependency relationships.

## Phase 1: Foundation and Understanding

### 1.1 dRehmFlight Codebase Analysis
- Review dRehmFlight architecture and control flow
- Understand IMU integration patterns (MPU6050 and MPU9250)
- Map data flow from sensor read to motor output
- Document PID control implementation
- Identify sensor calibration touch points in existing code
- Understand radio input processing for all supported protocols
- Review Madgwick filter implementation and sensor fusion

### 1.2 PlatformIO Project Structure
- Verify PlatformIO configuration for all Teensy targets
- Validate library dependencies and include paths
- Ensure build system compiles successfully for Teensy 4.0/4.1/3.6
- Test upload and serial monitor functionality
- Document build and flash procedures

### 1.3 Baseline Testing
- Flash unmodified dRehmFlight to Teensy hardware
- Verify IMU communication (I2C for MPU6050, SPI for MPU9250)
- Test radio receiver input (confirm channel reception)
- Validate motor output generation
- Establish baseline performance metrics
- Document any issues with stock implementation

## Phase 2: IMU Characterization and Calibration Research

### 2.1 IMU Sensor Characterization
- Study MPU6050 and MPU9250 datasheets
- Understand sensor noise characteristics and bias stability
- Document register maps and configuration options
- Research sensor warm-up time and drift patterns
- Identify differences between genuine and clone sensors

### 2.2 Calibration Algorithm Research
- Survey existing IMU calibration approaches in literature
- Review calibration methods from other flight controller projects
- Evaluate statistical methods for bias estimation
- Research accelerometer calibration techniques (6-position, single-position)
- Document pros and cons of different approaches

### 2.3 Calibration Requirements Definition
- Define accuracy requirements for flight stability
- Establish calibration success criteria
- Determine calibration time constraints (startup time)
- Specify user interaction model (fully automatic vs. semi-automatic)
- Document storage requirements for calibration data

## Phase 3: Gyroscope Auto-Calibration

### 3.1 Gyroscope Bias Estimation
- Implement stationary detection algorithm
- Collect gyroscope samples during startup
- Calculate mean and variance for each axis
- Implement outlier rejection for robust bias estimation
- Apply bias correction to raw sensor readings

### 3.2 Gyroscope Calibration Validation
- Test calibration quality metrics (variance, stability)
- Implement sanity checks (reasonable bias values)
- Add calibration status indicators (LED, serial output)
- Test with multiple MPU6050 units (genuine and clones)
- Verify calibration consistency across power cycles

### 3.3 Gyroscope Calibration Storage
- Implement EEPROM storage for calibration data
- Add calibration validity markers and checksums
- Implement fallback to new calibration if stored data invalid
- Add option to force recalibration via user command
- Document calibration data format

## Phase 4: Accelerometer Auto-Calibration

### 4.1 Simple Accelerometer Calibration
- Implement single-position (level) calibration
- Estimate Z-axis scale factor from gravity
- Calculate X and Y axis offsets assuming level placement
- Apply offset and scale corrections to accelerometer data
- Test calibration quality with static measurements

### 4.2 Multi-Position Calibration (Optional Enhancement)
- Design user-guided multi-position calibration sequence
- Implement position detection (which axis pointing up/down)
- Collect data from 6 cardinal orientations
- Solve for 3-axis offsets and scale factors
- Compare accuracy improvement vs. single-position method

### 4.3 Accelerometer Calibration Integration
- Store accelerometer calibration to EEPROM
- Validate calibration data quality
- Integrate with sensor fusion (Madgwick filter input)
- Test attitude estimation accuracy with auto-calibrated data
- Document calibration procedure and limitations

## Phase 5: Calibration System Integration

### 5.1 Unified Calibration Framework
- Create calibration manager module
- Coordinate gyroscope and accelerometer calibration sequence
- Implement state machine for calibration progress
- Add user feedback (serial messages, LED patterns)
- Handle calibration failures gracefully

### 5.2 Configuration Interface
- Implement serial command interface for calibration control
- Add commands to view current calibration data
- Allow manual calibration initiation
- Provide calibration quality diagnostics
- Support calibration data export/import for backup

### 5.3 Startup Sequence Optimization
- Minimize calibration time while maintaining accuracy
- Implement fast-path for valid stored calibration
- Add option for quick startup with stored data
- Balance calibration thoroughness vs. startup time
- Document recommended startup procedures

## Phase 6: Multi-Sensor Testing and Validation

### 6.1 MPU6050 Testing
- Test with multiple MPU6050 units from different suppliers
- Verify calibration works with genuine Invensense sensors
- Test with common clone sensors (GY-521 modules, etc.)
- Document sensor-specific quirks or issues
- Validate flight performance with auto-calibrated MPU6050

### 6.2 MPU9250 Testing
- Port calibration algorithms to MPU9250 SPI interface
- Test with MPU9250 hardware
- Compare calibration quality between MPU6050 and MPU9250
- Validate sensor fusion with MPU9250 data
- Document any MPU9250-specific considerations

### 6.3 Sensor Robustness Testing
- Test calibration with intentional sensor mounting misalignment
- Evaluate performance with sensors in various orientations
- Test temperature effects on calibration stability
- Assess long-term drift and need for recalibration
- Document sensor placement best practices

## Phase 7: Radio Receiver Integration

### 7.1 SBUS Receiver Integration
- Verify SBUS library integration in PlatformIO build
- Test SBUS signal reception on appropriate Teensy UART
- Validate channel mapping and failsafe detection
- Test with common SBUS receivers (FrSky, Futaba)
- Document SBUS wiring and configuration

### 7.2 Multi-Protocol Support
- Validate PWM receiver input
- Test PPM receiver support
- Verify DSM/DSMX satellite receiver functionality
- Document receiver selection and pin assignments
- Create receiver protocol selection guide

### 7.3 Radio Calibration and Mapping
- Implement radio endpoint calibration
- Add channel mapping configuration
- Support channel reversing where needed
- Implement failsafe behavior
- Document radio setup procedures

## Phase 8: Flight Testing and Tuning

### 8.1 Bench Testing
- Verify all control loops functional with auto-calibration
- Test motor output response to control inputs
- Validate attitude estimation accuracy
- Confirm failsafe behavior
- Perform safety checks before flight testing

### 8.2 Initial Flight Testing
- Configure test aircraft (quadcopter recommended)
- Perform tethered hover tests
- Validate stability and control response
- Collect flight logs for analysis
- Adjust PID parameters as needed

### 8.3 Flight Validation and Tuning
- Test in various flight conditions
- Validate auto-calibration across multiple flights
- Fine-tune control parameters for different aircraft types
- Document tuning procedures and recommended starting values
- Create platform-specific configuration examples

## Phase 9: Documentation and Examples

### 9.1 User Documentation
- Write comprehensive setup guide
- Document hardware connections and wiring
- Create step-by-step calibration instructions
- Provide troubleshooting guide
- Document configuration file format and options

### 9.2 Example Configurations
- Create example configuration for quadcopter
- Provide fixed-wing example configuration
- Document VTOL configuration approach
- Include receiver setup examples for each protocol
- Provide PID tuning starting points for common platforms

### 9.3 Developer Documentation
- Document code architecture and module organization
- Explain calibration algorithm implementation details
- Provide sensor integration guide for future sensors
- Document extension points for new features
- Create contribution guidelines for repository

### 9.4 Video and Visual Documentation
- Create wiring diagram illustrations
- Produce setup video tutorial (optional)
- Document LED status indicators
- Provide visual calibration procedure guide
- Create flight test demonstration videos

## Phase 10: Repository Integration and Deployment

### 10.1 Repository Organization
- Organize flight controller code in appropriate directory structure
- Integrate with existing repository documentation
- Link to related platform-specific configurations
- Ensure consistency with other projects in repository
- Update top-level README with flight controller information

### 10.2 Release Preparation
- Perform final code review and cleanup
- Validate all documentation accuracy
- Test complete setup procedure from scratch
- Create release notes documenting features and known issues
- Tag stable release version

### 10.3 Community and Maintenance
- Publish to repository for community access
- Monitor for issues and user feedback
- Plan maintenance schedule for updates
- Consider community contributions and pull requests
- Document future enhancement priorities

## Dependencies and Critical Path

### Critical Path Items
1. dRehmFlight understanding is prerequisite for all modifications
2. Gyroscope calibration must work before accelerometer calibration
3. Calibration storage needed before multi-flight testing
4. Radio integration required before flight testing
5. Documentation depends on stable implementation

### Parallel Work Opportunities
- IMU characterization can proceed alongside codebase analysis
- MPU6050 and MPU9250 testing can be done in parallel
- Documentation can be written incrementally during development
- Different receiver protocols can be tested independently

### Risk Mitigation
- Maintain working baseline throughout development
- Test incrementally to isolate issues
- Keep calibration algorithms modular for easy debugging
- Document assumptions and limitations clearly
- Plan for fallback to manual calibration if auto-calibration fails
