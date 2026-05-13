# Phase 2: GPS Integration & Coordinate Frame Management
**Date**: 2026-05-07  
**Phase 1 Status**: ✓ COMPLETE (113/113 tests passing)  
**Phase 2 Status**: Ready for execution  
**Target Duration**: 1 week continuous work  

---

## Scope: What Will Be Completed

### Primary Deliverables

1. **GPS Module Driver** (`src/sensors/gps.h/cpp`)
   - UART serial reading at configurable baud rate (9600/115200)
   - NMEA sentence parsing (GPGGA, GPRMC)
   - GPS data validation (satellite count, fix quality, HDOP)
   - Timeout handling for stale data
   - Inherits from PositionSensor base class

2. **Coordinate Frame Manager** (`src/navigation/coordinate_frame.h/cpp`)
   - Maintains local NED (North-East-Down) origin
   - GPS → ECEF → NED conversion pipeline
   - Dynamic origin initialization on first GPS fix
   - Helper functions for position queries
   - Thread-safe for concurrent sensor reads

3. **GPS Fusion Output** (extend `src/output/sensor_output_manager.h/cpp`)
   - Merged JSON with orientation + position + velocity
   - Timestamp synchronization
   - Validity flags for each data type
   - Human-readable formatting (degrees, meters, m/s)

4. **Real-time Position Display** (new serial output)
   - Live GPS lock status
   - Satellite count and HDOP metrics
   - Current NED position relative to origin
   - Velocity estimates

5. **Configuration Updates** (`src/config/gps_config.h`, `platformio.ini`)
   - Build flags: `GPS_ENABLE`, `GPS_UART_PORT`, `GPS_BAUD`
   - New build environments: `arduino_mega_gps`, `arduino_mega_full`
   - Pin definitions for UART serial communication

### Test Coverage

6. **Unit Tests** (`tests/test_gps.cpp`)
   - NMEA sentence parsing (various formats and edge cases)
   - GPS data validation (invalid satellites, bad fix quality)
   - Timeout detection (>1 second stale data)
   - Known coordinate conversion (examples from research)

7. **Integration Tests** (`tests/integration_test_gps_fusion.cpp`)
   - BNO085 orientation + GPS position in single JSON
   - Coordinate frame conversions (GPS → NED with known reference points)
   - Timestamp synchronization across sensors
   - Cold start behavior (no initial origin)

8. **Hardware Tests** (`tests/test_gps_hardware.cpp`)
   - Actual serial reading from GPS module
   - Live NMEA parsing from hardware
   - Real-time position tracking
   - Performance measurements

### Documentation

9. **Code Documentation**
   - Header comments in all files
   - Function documentation with examples
   - UART configuration guide
   - NMEA format reference

10. **User Guide**
    - How to connect GPS module to Arduino
    - Calibration procedure (cold/warm start)
    - Output format specification
    - Troubleshooting GPS lock issues

---

## Hardware Requirements

### GPS Module (UART-Based, NOT USB)
**Recommended:** u-blox NEO-M9N (same as USB modules but with UART variant)
- **Interface:** TTL-level UART (3.3V or 5V)
- **Baud Rate:** 9600 baud (default), 115200 supported
- **Output:** NMEA or UBX binary
- **Connection to Arduino Mega:**
  - TX (from GPS) → RX1 (pin 19) or RX2 (pin 17) or RX3 (pin 15)
  - RX (to GPS) → TX1 (pin 18) or TX2 (pin 16) or TX3 (pin 14)
  - GND → GND
  - +5V → +5V (or via level shifter if 3.3V module)

**Alternative:** Any GPS module with NMEA output (Adafruit Ultimate GPS, etc.)

---

## File Structure (Complete)

```
auto_orientation/
├── src/
│   ├── config/
│   │   ├── gps_config.h (NEW)
│   │   ├── pins.h (update with GPS UART pins)
│   │   └── mode.h (add GPS_ENABLE flag)
│   ├── sensors/
│   │   ├── gps.h (NEW)
│   │   ├── gps.cpp (NEW)
│   │   └── sensor_base.h (reviewed, has PositionSensor interface)
│   ├── navigation/
│   │   ├── coordinate_frame.h (NEW)
│   │   ├── coordinate_frame.cpp (NEW)
│   │   └── fusion_manager.h (NEW - coordinates + filtering)
│   ├── output/
│   │   ├── sensor_output_manager.h (update for GPS)
│   │   └── sensor_output_manager.cpp (update JSON format)
│   └── main.cpp (update to initialize GPS, coordinate frame)
├── tests/
│   ├── test_gps.cpp (NEW)
│   ├── test_coordinate_frame.cpp (NEW)
│   ├── integration_test_gps_fusion.cpp (NEW)
│   └── test_gps_hardware.cpp (NEW - optional, hardware-dependent)
├── docs/
│   ├── GPS_DRIVER_API_REFERENCE.md (NEW)
│   ├── COORDINATE_FRAME_API_REFERENCE.md (NEW)
│   ├── GPS_HARDWARE_SETUP.md (NEW)
│   ├── GPS_TROUBLESHOOTING.md (NEW)
│   └── PHASE_2_TEST_RESULTS.md (NEW)
└── platformio.ini (update with GPS build flags)
```

---

## Implementation Detail: GPS Module Driver

### GPS.h Class Interface

```cpp
class GPS : public PositionSensor {
public:
  // Initialization
  bool begin(uint32_t baud = 9600);
  void end();
  bool isInitialized() const;

  // Reading
  bool read();
  bool hasNewData() const;

  // Status
  bool isHealthy() const;
  const char* getStatusString() const;

  // Sensor info
  const char* name() const { return "GPS"; }

  // Get position data (inherited)
  const PositionData& getPosition() const;

  // GPS-specific
  bool isLocked() const;
  uint8_t getNumSatellites() const;
  float getHDOP() const;
  float getVDOP() const;
  float getVelocity() const;  // m/s, calculated from RMC

private:
  // UART configuration
  HardwareSerial* serial;
  uint32_t baud_rate;

  // Parsing state machine
  bool parseSentence(const char* sentence);
  bool parseGPGGA(const char* tokens[]);  // Position, altitude
  bool parseGPRMC(const char* tokens[]);  // Velocity, course

  // Data storage
  PositionData position_data;
  float velocity_mps;
  uint32_t last_update_ms;
  bool has_lock;
};
```

### Parsing Strategy
- Line-by-line NMEA reading from serial buffer
- Tokenize by comma, handle checksums
- Two sentence types:
  - **GNGGA** (or GPGGA): lat, lon, alt, satellites, HDOP, fix_quality
  - **GNRMC** (or GPRMC): lat, lon, velocity, course, date/time
- Timeout: if no update for >1000ms, mark data stale

---

## Implementation Detail: Coordinate Frame Manager

### CoordinateFrame Class Interface

```cpp
class CoordinateFrame {
public:
  // Initialization
  bool initialize(double origin_lat, double origin_lon, float origin_alt_m);
  bool initializeOnFirstFix(const PositionData& gps);

  // Conversions
  bool gpsToLocalNED(double lat, double lon, float alt_m,
                      float* north_m, float* east_m, float* down_m) const;
  bool localNEDToGPS(float north_m, float east_m, float down_m,
                      double* lat, double* lon, float* alt_m) const;

  // State queries
  bool isInitialized() const;
  void getOrigin(double* lat, double* lon, float* alt_m) const;
  float getOriginAltitude() const;

private:
  // Cached values for performance
  double origin_lat, origin_lon;
  float origin_alt_m;
  float sin_lat, cos_lat;  // Pre-computed trig

  // Uses coordinates.h functions for actual math
  bool computeNED(double lat, double lon, float alt_m,
                   float* n, float* e, float* d) const;
};
```

### Workflow
1. On first valid GPS fix: Initialize with that lat/lon/alt as origin
2. All subsequent positions converted to relative NED
3. Magnetic declination applied for heading alignment
4. Re-initialization possible if manual origin override needed

---

## JSON Output Format (Extended from Phase 1)

**Phase 1 JSON:**
```json
{
  "timestamp": 12345,
  "orientation": {
    "w": 0.707,
    "x": 0, "y": 0, "z": 0.707,
    "euler": {"roll_deg": 0, "pitch_deg": 0, "yaw_deg": 90},
    "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 3}
  }
}
```

**Phase 2 Extended JSON:**
```json
{
  "timestamp": 12345,
  "orientation": {
    "w": 0.707, "x": 0, "y": 0, "z": 0.707,
    "euler": {"roll_deg": 0, "pitch_deg": 0, "yaw_deg": 90},
    "calibration": {"system": 3}
  },
  "position": {
    "gps": {
      "latitude": 61.391234,
      "longitude": -149.171234,
      "altitude_m": 205.3,
      "hdop": 0.75,
      "vdop": 1.30,
      "satellites": 12,
      "fix_quality": 2,
      "locked": true
    },
    "local_ned": {
      "north_m": 145.2,
      "east_m": 87.3,
      "down_m": -23.5
    },
    "velocity_mps": 0.55
  }
}
```

---

## Testing Strategy

### Unit Tests (test_gps.cpp)
- NMEA parsing with valid/invalid sentences
- Checksum validation
- Token extraction edge cases
- Timeout behavior (stale data detection)
- Data range validation (lat ±90°, lon ±180°, alt real-world)

### Integration Tests (integration_test_gps_fusion.cpp)
- Combined BNO085 orientation + GPS position JSON
- Coordinate frame initialization on first fix
- GPS → NED round-trip accuracy (sub-meter)
- Timestamp alignment
- Error handling (bad GPS data, missing satellites)

### Hardware Tests (test_gps_hardware.cpp - optional)
- Real serial connection to GPS module
- Live NMEA sentence parsing
- Satellite acquisition time
- Position update rate (1 Hz typical)
- HDOP accuracy check

---

## Build Configuration Changes

### platformio.ini (NEW SECTIONS)

```ini
[env:arduino_mega_gps]
extends = env:arduino_mega
build_flags = -D GPS_ENABLE -D GPS_UART_PORT=1 -D GPS_BAUD=9600
description = Production mode with GPS support

[env:arduino_mega_full]
extends = env:arduino_mega
build_flags = -D CALIBRATION_MODE -D GPS_ENABLE -D SNAPSHOT_MODE -D GPS_UART_PORT=1 -D GPS_BAUD=9600
description = Full debug with all sensors

[env:arduino_mega_gps_baud_115200]
extends = env:arduino_mega_gps
build_flags = -D GPS_ENABLE -D GPS_UART_PORT=1 -D GPS_BAUD=115200
description = GPS at 115200 baud (for M9N variant)
```

### src/config/gps_config.h (NEW)

```cpp
#ifndef GPS_CONFIG_H
#define GPS_CONFIG_H

#ifdef GPS_ENABLE
  #define GPS_AVAILABLE 1
  #define GPS_UART_PORT 1       // 0=Serial1, 1=Serial2, 2=Serial3
  #define GPS_BAUD 9600         // Typical for NEO-M9N
  #define GPS_READ_TIMEOUT_MS 100
  #define GPS_STALE_TIMEOUT_MS 1000
#else
  #define GPS_AVAILABLE 0
#endif

#endif  // GPS_CONFIG_H
```

---

## Success Criteria

- ✓ Phase 2 complete: GPS module parsed, position data clean
- ✓ Coordinate frame initialized on first fix
- ✓ GPS → NED conversion accurate to ±2 meters
- ✓ JSON output includes position + orientation
- ✓ All 40+ tests passing
- ✓ Hardware test verifies live GPS data parsing
- ✓ Timestamp synchronization between sensors
- ✓ Graceful handling of GPS dropouts (data staleness detection)

---

## Estimated Effort Breakdown

- GPS module driver: 2-3 hours (parsing, validation, state machine)
- Coordinate frame manager: 1-2 hours (math wrapper around Phase 1 code)
- JSON integration: 1 hour (extend existing output manager)
- Unit tests: 2-3 hours (comprehensive NMEA parsing tests)
- Integration tests: 2 hours (combined orientation + position)
- Hardware tests: 1-2 hours (live GPS serial reading)
- Documentation: 2 hours (API reference, setup guide, troubleshooting)
- Build configuration: 0.5 hours (platform.ini, config.h)

**Total: 11.5-14 hours** (1-2 days with parallel agents)

---

## Parallel Agent Task Breakdown

### Agent 1: GPS Module Driver
- Create `src/sensors/gps.h/cpp`
- Implement UART reading and NMEA parsing
- Create `tests/test_gps.cpp` with 25+ unit tests
- Verify with hand-calculated test cases

### Agent 2: Coordinate Frame Manager
- Create `src/navigation/coordinate_frame.h/cpp`
- Use Phase 1 coordinate conversion functions
- Create `tests/test_coordinate_frame.cpp` with round-trip tests
- Test with known reference points (Munich, Los Angeles, etc.)

### Agent 3: JSON Integration & Output
- Extend `src/output/sensor_output_manager.h/cpp`
- Add GPS fields to JSON formatter
- Update `src/main.cpp` to initialize GPS + coordinate frame
- Create documentation for output format

### Agent 4: Build Config & API Docs
- Update `platformio.ini` with GPS build environments
- Create `src/config/gps_config.h`
- Write `docs/GPS_DRIVER_API_REFERENCE.md`
- Write `docs/GPS_HARDWARE_SETUP.md`

### Agent 5: Integration Testing & Verification
- Create `tests/integration_test_gps_fusion.cpp`
- Create `tests/test_gps_hardware.cpp` (with hardware verification)
- Run all tests, verify 100% pass rate
- Create `docs/PHASE_2_TEST_RESULTS.md`

---

## Git Strategy

- One commit per agent completion
- Final squash commit: "PHASE 2: COMPLETE - GPS integration & coordinate frames"
- Push to origin/main when complete

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| GPS module not available | Use simulated GPS data from test files |
| UART baud rate mismatch | Support multiple baud rates via build flags |
| NMEA format variation | Handle both GNXXX and GPXXX prefixes |
| Coordinate conversion errors | Validate with research code from Phase 1 |
| Timestamp sync issues | Use system millis() consistently |
| Memory constraints | Pre-allocate buffers, avoid dynamic strings |

---

## Next Phase (After Phase 2 Complete)

- Phase 3: Sensor Fusion with EKF (1.5 weeks)
- Phase 4: Camera Calibration (1 week)
- Phase 5: Applications (2-3 weeks)

---

**Status**: Ready for agent execution  
**Approval**: Awaiting go signal  
**Start Date**: 2026-05-07 (now)  
**Expected Completion**: 2026-05-08 (24 hours with parallel agents)
