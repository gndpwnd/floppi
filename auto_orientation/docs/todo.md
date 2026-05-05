# Todo: Auto Orientation

**Current Phase**: Project Initialization  
**Last Updated**: 2026-05-05

---

## In Progress

- [x] Review and approve documentation (scope, roadmap, README)
- [x] Initialize PlatformIO project structure (Mega as default target)
- [x] Archive project initialization context (email, initial sketch)
- [x] Create findings skeleton for key research areas
- [ ] **Research Phase 1: BNO085 Calibration Persistence** (investigate Adafruit library API)
- [ ] **Research Phase 2: GPS Accuracy Improvement** (NMEA parsing + statistical approaches)

---

## Up Next (Foundation)

**Week 1: PlatformIO & Sensor Abstraction**
- [ ] Initialize `platformio.ini` with correct board/framework
- [ ] Set up .gitignore for PlatformIO and local libraries
- [ ] Create sensor abstraction layer (HAL) structure
  - [ ] Base sensor class interface
  - [ ] BNO085 sensor driver template
  - [ ] NEO-M9N sensor driver template
- [ ] Clone Adafruit BNO08x library locally
- [ ] Clone Ublox/GPS parsing libraries locally
- [ ] Verify build system (platformio run --target build succeeds)

**Week 1: Hardware Validation**
- [ ] Test BNO085 initialization (UART mode, P1=5V)
- [ ] Test NEO-M9N USB connection & serial output
- [ ] Validate UART pinout on target board

**Week 2: BNO085 Integration**
- [ ] Implement BNO085 sensor driver
  - [ ] Initialize in Absolute Orientation mode (not RVC)
  - [ ] Read and output quaternions
  - [ ] Read and output calibration status
- [ ] Research & document BNO085 calibration persistence (See findings)
- [ ] Implement calibration save/restore
- [ ] Test with static orientation validation

**Week 2: NEO-M9N Integration**
- [ ] Implement NEO-M9N sensor driver
  - [ ] USB serial parsing
  - [ ] NMEA frame extraction (lat/lon/alt/accuracy)
- [ ] Test GPS lock and position output

**Week 3: Combined Output & Monitoring**
- [ ] Design serial output format (JSON or delimited)
- [ ] Implement combined data structure (timestamp + orient + position)
- [ ] Adapt serial_monitor.py from flight_controller/tools
- [ ] Python data logging to CSV
- [ ] Real-time serial display

**Week 3: Testing & Validation**
- [ ] Static calibration test (flat surface, 10+ samples)
- [ ] Dynamic rotation test
- [ ] Persistence test (power cycle)
- [ ] GPS integration test
- [ ] Document test results

**Week 4: Documentation & Polish**
- [ ] Hardware hookup guide (photos + wiring diagram if possible)
- [ ] Calibration guide (step-by-step user instructions)
- [ ] API reference (sensor classes, method signatures)
- [ ] Troubleshooting guide
- [ ] Developer guide for adding sensors

---

## Backlog (Future Milestones)

### v1.1 – MPU 6050 Support
- [ ] Research yaw estimation without magnetometer
- [ ] Implement MPU 6050 sensor driver
- [ ] Investigate SD card logging for persistent storage
- [ ] Unit tests for new sensor

### Future – Enhanced Features
- [ ] Web dashboard for real-time visualization
- [ ] Advanced sensor fusion (if custom fusion needed)
- [ ] Integration with flight_controller auto-calibration
- [ ] Integration with skytracker for camera orientation

---

## Blocked / On Hold

- **GPS accuracy improvement**: Deferred to v1.1; start with basic position output
- **SD card logging**: Deferred to v1.1; focus on real-time serial output first
- **Flight controller integration**: Deferred until v1.0 is stable

---

## Recently Completed

- [x] Project scope definition (scope.md)
- [x] Roadmap creation (roadmap.md)
- [x] Documentation structure (README, findings skeleton)
- [x] Archive existing files (initial sketch, MDC email)

---

## Notes

**Assumptions**:
- User has BNO085 + NEO-M9N GPS + Arduino hardware ready
- PlatformIO CLI is installed and working
- Local library cloning approach is feasible (test with first library)

**Dependencies**:
- Waiting for user to confirm scope/roadmap before proceeding with code
- May need to research BNO085 calibration persistence (priority research item)

**Risks**:
- BNO085 calibration persistence may be underdocumented; may need to reverse-engineer from library code
- GPS USB driver compatibility varies by platform; validate early
- Library versions may have API differences; use specific commit hashes for local clones

