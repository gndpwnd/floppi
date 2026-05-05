# Roadmap: Auto Orientation

**Current Phase**: Initialization & Foundation  
**Next Milestone**: v1.0 (BNO085 + GPS working end-to-end)

---

## Milestone: v1.0 – BNO085 + GPS Integration (Stable Release)

### Foundation & Tooling
- [ ] PlatformIO project structure initialized
  - Sensor abstraction layer / HAL (Hardware Abstraction Layer)
  - Local library dependencies (BNO08x, ArduinoJSON, GPS parser)
- [ ] Serial monitoring Python script (adapted from flight_controller/tools)
- [ ] Build & flash scripts for development workflow

### BNO085 IMU Sensor
- [ ] Initialize BNO085 in Absolute Orientation (not RVC) mode
- [ ] Stream quaternion data (scalar + 3-component) over serial
- [ ] Expose calibration status (system, accel, gyro, mag)
- [ ] Research & implement persistent calibration save/restore (See [findings](docs/findings/bno085-calibration-persistence.md))

### Ublox NEO-M9N GPS
- [ ] Initialize GPS module over USB serial
- [ ] Parse NMEA frames (or UBX binary if available)
- [ ] Extract lat/lon/altitude + accuracy (CEP)
- [ ] Research GPS accuracy improvement via multi-sample averaging (See [findings](docs/findings/gps-accuracy-improvement.md))

### Integration & Output
- [ ] Combined data structure (timestamp + quaternion + position + accuracy)
- [ ] Structured serial output (JSON or delimited text)
- [ ] Python serial monitor (real-time display, data logging to CSV)

### Testing & Validation
- [ ] Static calibration test (verify stable orientation on flat surface)
- [ ] Dynamic rotation test (rotate through known angles, track with quaternions)
- [ ] Persistence test (power cycle, verify calibration restored)
- [ ] GPS integration test (position streaming, accuracy estimation)
- [ ] Field test with actual hardware

### Documentation (v1.0)
- [ ] API reference for sensor classes
- [ ] Calibration guide (manual steps, data persistence)
- [ ] Hardware hookup guide (wiring, pin configuration)
- [ ] Developer guide for adding new sensors
- [ ] Troubleshooting guide (common issues, debug checklist)

---

## Milestone: v1.1 – Multi-Sensor Framework & MPU 6050

### MPU 6050 Support
- [ ] Research orientation inference without magnetometer (See [findings](docs/findings/mpu6050-yaw-estimation.md))
- [ ] Add MPU 6050 to sensor HAL
- [ ] Implement pitch/roll from accel; research yaw options
- [ ] Consider SD card logging for calibration data (MPU has no onboard flash)

### Extensibility
- [ ] Sensor abstraction complete (easy to add future sensors)
- [ ] Config file support (select active sensors, calibration parameters)
- [ ] Unit tests for sensor drivers

---

## Milestone: Future – Enhanced Capabilities

### Data Logging & Analysis
- [ ] SD card integration (high-frequency IMU/GPS logging)
- [ ] Analysis scripts (Python; detect calibration drift, analyze fusion behavior)

### Advanced Features
- [ ] Web dashboard (real-time visualization of quaternion/position)
- [ ] Sensor fusion tuning (if custom fusion needed beyond BNO firmware)
- [ ] Accuracy metrics (CEP tracking, orientation stability over time)

### Integration with Downstream Projects
- [ ] Flight controller auto-calibration bridge
- [ ] Skytracker camera orientation context
- [ ] Integration tests with flight controller

---

## Research Items (Ongoing)

These are documented in `docs/findings/` as discovered:

1. **BNO085 calibration persistence** – How to properly save/restore magnetometer calibration profile to flash memory
2. **GPS accuracy improvement** – Statistical multi-sample averaging for better CEP when stationary
3. **MPU 6050 yaw without magnetometer** – Approaches for yaw estimation (dead reckoning, integration of gyro, or external reference)
4. **Quaternion output validation** – Verify scalar vs. component ordering in BNO085 output

---

## Dependencies & Blockers

| Item | Status | Blocker? | Notes |
|------|--------|----------|-------|
| Adafruit BNO08x library (local) | Not started | No | Need to clone locally |
| Ublox NEO-M9N datasheets | Partial | No | MDC has docs; need USB protocol details |
| Arduino dev environment | Ready | No | PlatformIO setup |
| Hardware (BNO085 + GPS) | Available | No | User has both; wired per MDC's guide |
| GPS USB driver | Likely ready | Maybe | Typical on Linux/Mac; may need validation on target platform |

---

## Success Metrics (v1.0)

1. **Functionality**: BNO085 quaternion output + GPS position streamed reliably for 10+ min without dropouts
2. **Calibration**: Magnetometer calibration persists across power cycle
3. **Usability**: A developer can add a new sensor to HAL within 2 hours using only local code
4. **Documentation**: All four "Developer guide" sections complete (API, calibration, hardware, troubleshooting)
5. **Testing**: All success criteria from scope.md met

