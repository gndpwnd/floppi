# GPS Driver API Reference

**Status**: Phase 2 Documentation  
**Last Updated**: 2026-05-07  
**Related Docs**: [GPS Hardware Setup](../hardware/GPS_HARDWARE_SETUP.md), [Troubleshooting](../hardware/GPS_TROUBLESHOOTING.md)

## Overview

The GPS driver (`src/sensors/gps.h/cpp`) provides NMEA sentence parsing for u-blox NEO-M8/M9N and compatible UART-based GPS modules. It inherits from the `PositionSensor` base class and integrates cleanly into the auto_orientation sensor framework.

**Key Features:**
- UART-based serial communication (configurable baud rate)
- NMEA sentence parsing (GPGGA, GPRMC formats)
- Automatic data validation (satellite count, fix quality, HDOP)
- Stale data detection (>1 second timeout)
- Checksum verification (XOR-based)
- Status string for debugging

---

## Class Interface: GPS

### Header Location
```cpp
#include "sensors/gps.h"
```

### Inheritance Hierarchy
```
PositionSensor (base class)
  └─ GPS (concrete implementation)
```

### Constructor & Destructor

#### `GPS()`
Default constructor. Initializes internal state but does not open serial port.

**Parameters**: None

**Example:**
```cpp
GPS gps;
```

#### `~GPS()`
Virtual destructor. Calls `end()` to close serial port if active.

**Parameters**: None

---

### Initialization Methods

#### `bool begin()`
Initialize GPS with default baud rate (9600 baud).

**Parameters**: None

**Returns**: 
- `true` if serial port opened successfully
- `false` if serial port opening failed

**Details**:
- Uses Serial1 by default (configurable via `GPS_UART_PORT` build flag)
- Sets baud rate from `GPS_BAUD` build flag (default 9600)
- Enables internal state machine and timeout tracking
- Safe to call multiple times (closes previous connection)

**Example:**
```cpp
GPS gps;
if (gps.begin()) {
  Serial.println("GPS initialized at 9600 baud");
} else {
  Serial.println("ERROR: GPS initialization failed");
  while (1) delay(100);
}
```

#### `bool begin(uint32_t baud)`
Initialize GPS with specific baud rate (overrides build flag).

**Parameters**:
- `baud` (uint32_t): Baud rate in bits per second
  - Common values: 9600, 115200
  - Must match GPS module configuration

**Returns**:
- `true` if serial port opened and configured
- `false` if baud rate is invalid or port opening failed

**Details**:
- Overrides `GPS_BAUD` compile-time setting
- Useful for runtime baud rate selection
- Typical UART baud rates: 9600, 19200, 38400, 115200

**Example:**
```cpp
GPS gps;
// Try common baud rates
if (!gps.begin(9600)) {
  Serial.println("9600 baud failed, trying 115200...");
  if (gps.begin(115200)) {
    Serial.println("GPS running at 115200 baud");
  }
}
```

#### `void end()`
Close serial port and disable GPS reading.

**Parameters**: None

**Returns**: void

**Details**:
- Safe to call even if GPS is not initialized
- After calling `end()`, must call `begin()` again to re-initialize
- No data updates occur after `end()` is called

**Example:**
```cpp
gps.end();
// Now serial port is free for other use (e.g., debugging)
```

---

### Query Methods

#### `bool isInitialized() const`
Check if GPS serial port is open and ready.

**Parameters**: None

**Returns**:
- `true` if `begin()` succeeded and `end()` not called
- `false` if not initialized

**Example:**
```cpp
if (gps.isInitialized()) {
  gps.read();  // Safe to read
} else {
  Serial.println("GPS not initialized");
}
```

#### `bool read()`
Attempt to read and parse one or more NMEA sentences from serial buffer.

**Parameters**: None

**Returns**:
- `true` if at least one valid NMEA sentence was parsed
- `false` if no complete sentence available or all sentences were invalid

**Details**:
- Non-blocking: returns immediately
- Reads up to 128 bytes per call from serial buffer
- Maintains internal state machine for partial sentence buffering
- Supported sentence types:
  - `$GPGGA` or `$GNGGA` (GGA: position, altitude, fix quality, satellite count, HDOP)
  - `$GPRMC` or `$GNRMC` (RMC: velocity, course, date/time)
- Validates checksum before accepting sentence
- Updates `last_update_ms_` timestamp on successful parse

**Parsing State Machine**:
```
STATE_IDLE ─→ (see '$') ─→ STATE_READING ─→ (see '*') ─→ STATE_CHECKSUM
   ↑                                                              │
   └──────────────────── (complete + valid) ──────────────────────┘
```

**Example:**
```cpp
while (gps.read()) {
  Serial.println("Got GPS data");
  const auto& pos = gps.getPosition();
  Serial.printf("Lat: %.6f, Lon: %.6f\n", pos.latitude, pos.longitude);
}
```

#### `bool hasNewData() const`
Check if `read()` has successfully parsed data since last query.

**Parameters**: None

**Returns**:
- `true` if new data parsed since last `read()` call
- `false` if no new data or reading not attempted

**Details**:
- Useful for event-driven code (only process when new data arrives)
- Flag is set by `read()` and cleared on next `read()` call
- Even if `hasNewData()` returns false, `getPosition()` returns last valid data

**Example:**
```cpp
gps.read();
if (gps.hasNewData()) {
  Serial.println("New position available");
} else {
  Serial.println("No new data this cycle");
}
```

#### `const char* name() const`
Get sensor name for logging/debug output.

**Parameters**: None

**Returns**: 
- Pointer to constant string: `"GPS"`

**Example:**
```cpp
Serial.printf("Sensor: %s\n", gps.name());  // Output: "Sensor: GPS"
```

#### `bool isHealthy() const`
Check if GPS has valid position lock and recent data.

**Parameters**: None

**Returns**:
- `true` if position is valid AND data is not stale (< 1 second old)
- `false` if position invalid OR data stale

**Details**:
- Stale timeout: `GPS_STALE_TIMEOUT_MS` (default 1000ms)
- A healthy GPS means:
  1. Fix quality >= 1 (at least SPS fix)
  2. Satellite count >= 4 (minimum for 3D fix)
  3. Data updated within last second
- Useful for real-time applications that need current data

**Example:**
```cpp
if (gps.isHealthy()) {
  Serial.println("GPS is healthy, position is recent");
} else {
  Serial.println("GPS data stale or no fix");
}
```

#### `const char* getStatusString() const`
Get human-readable status message (for debugging/display).

**Parameters**: None

**Returns**: 
- Pointer to status string buffer (64 bytes max)
- Example: `"GPS: 12 sats, HDOP=0.75, VALID"`

**Details**:
- Includes satellite count, HDOP, and lock status
- Updated after each `read()` call
- Safe to call anytime

**Example:**
```cpp
gps.read();
Serial.println(gps.getStatusString());
// Output: "GPS: 8 sats, HDOP=1.20, VALID"
```

#### `const PositionData& getPosition() const`
Get current position data (from PositionSensor base class).

**Parameters**: None

**Returns**: 
- Constant reference to `PositionData` struct:
  ```cpp
  struct PositionData {
    double latitude;           // Degrees (-90 to +90)
    double longitude;          // Degrees (-180 to +180)
    float altitude_m;          // Meters above sea level
    bool position_valid;       // true if fix quality >= 1 and sats >= 4
    uint32_t timestamp_ms;     // System time when data acquired
  };
  ```

**Details**:
- Returns last successfully parsed position (even if stale)
- Only valid when `getPosition().position_valid == true`
- Timestamp indicates when GPS data was parsed

**Example:**
```cpp
const auto& pos = gps.getPosition();
if (pos.position_valid) {
  Serial.printf("Position: %.6f, %.6f @ %.1f m\n",
                pos.latitude, pos.longitude, pos.altitude_m);
}
```

---

### GPS-Specific Data Access

#### `float getVelocityMps() const`
Get current velocity in meters per second.

**Parameters**: None

**Returns**: 
- Velocity in m/s (calculated from RMC speed field)
- 0.0 if no RMC sentence parsed yet
- Accuracy depends on GPS module and atmospheric conditions

**Details**:
- Source: RMC sentence "speed over ground" field (in knots)
- Formula: `velocity_mps = knots * 0.51444`
- Updated when RMC sentence is successfully parsed
- Not affected by position validity (can have speed even without fix)

**Example:**
```cpp
float speed = gps.getVelocityMps();
Serial.printf("Speed: %.2f m/s (%.2f km/h)\n", speed, speed * 3.6);
```

#### `uint32_t getLastUpdateMs() const`
Get timestamp of last successfully parsed NMEA sentence (in milliseconds).

**Parameters**: None

**Returns**: 
- System uptime (from `millis()`) when last sentence was parsed
- 0 if no sentence has been parsed yet

**Details**:
- Useful for computing data age
- Compare to current `millis()` to check staleness: `millis() - getLastUpdateMs()`
- Both GGA and RMC sentences update this timestamp

**Example:**
```cpp
uint32_t now = millis();
uint32_t age_ms = now - gps.getLastUpdateMs();
if (age_ms > 1000) {
  Serial.println("GPS data stale (>1 second)");
}
```

#### `bool hasLock() const`
Check if GPS currently has 3D position lock.

**Parameters**: None

**Returns**:
- `true` if fix quality >= 1 AND satellite count >= 4
- `false` otherwise

**Details**:
- "Lock" means satellite fix is available
- Does not guarantee accuracy (check HDOP for that)
- Returns false if no GGA sentence parsed yet

**Example:**
```cpp
if (gps.hasLock()) {
  Serial.printf("Locked with %d satellites\n", getNumSatellites());
} else {
  Serial.println("No GPS lock, waiting for satellites...");
}
```

#### `uint8_t getNumSatellites() const`
Get number of satellites currently being tracked.

**Parameters**: None

**Returns**: 
- Satellite count (0-20 typical)
- 0 if no GGA sentence parsed yet
- May include satellites with weak signals

**Details**:
- From GGA sentence field 7
- Minimum 4 satellites needed for 3D fix
- More satellites → more accurate position
- Can be 0 even when locked if module has poor signal

**Example:**
```cpp
if (gps.getNumSatellites() >= 4) {
  Serial.printf("Good fix: %d satellites\n", gps.getNumSatellites());
}
```

#### `float getHDOP() const`
Get Horizontal Dilution of Precision (position accuracy indicator).

**Parameters**: None

**Returns**: 
- HDOP value (typically 0.5-10.0)
- Lower is better (0.5-2.0 is excellent)
- `NAN` if no GGA sentence parsed yet

**Details**:
- From GGA sentence field 8
- HDOP < 1.0: Excellent accuracy (~1 meter)
- HDOP < 2.0: Good accuracy (~3-5 meters)
- HDOP < 5.0: Moderate accuracy (~10-20 meters)
- HDOP > 5.0: Poor accuracy (unreliable)
- Depends on satellite geometry and signal strength

**Example:**
```cpp
float hdop = gps.getHDOP();
if (hdop < 2.0) {
  Serial.println("Excellent position accuracy");
} else if (hdop < 5.0) {
  Serial.println("Moderate position accuracy");
} else {
  Serial.println("Poor position accuracy (HDOP > 5.0)");
}
```

#### `float getVDOP() const`
Get Vertical Dilution of Precision (altitude accuracy indicator).

**Parameters**: None

**Returns**: 
- VDOP value (typically 0.5-10.0)
- Similar scale to HDOP
- `NAN` if no GGA sentence parsed yet

**Details**:
- From GGA sentence field 9
- Vertical position accuracy is typically worse than horizontal (factor of 2-4)
- Same interpretation as HDOP

**Example:**
```cpp
float vdop = gps.getVDOP();
float hdop = gps.getHDOP();
Serial.printf("Position accuracy: HDOP=%.2f, VDOP=%.2f\n", hdop, vdop);
```

---

### Configuration Methods

#### `void setSerialPort(HardwareSerial* serial)`
Override default serial port (advanced use only).

**Parameters**:
- `serial`: Pointer to HardwareSerial object (Serial1, Serial2, Serial3, etc.)

**Returns**: void

**Details**:
- Useful if GPS needs to use non-default UART
- Must be called before `begin()`
- On Arduino Mega: Serial1, Serial2, Serial3 available
- Example: to use Serial2 instead of Serial1:
  ```cpp
  gps.setSerialPort(&Serial2);
  gps.begin();  // Now uses Serial2
  ```

**Warning**: Do not set to the same serial port used by other devices.

#### `void setBaudRate(uint32_t baud)`
Override default baud rate (advanced use only).

**Parameters**:
- `baud`: Baud rate in bits per second

**Returns**: void

**Details**:
- Must be called before `begin()`
- Typically use `begin(baud)` instead for most cases
- Useful if calling `end()` then `begin()` with different baud rate

**Example:**
```cpp
gps.setBaudRate(115200);
gps.begin();
```

---

## NMEA Sentence Format Reference

### GPGGA / GNGGA: Fix Data

**Format**: `$GPGGA,hhmmss.ss,ddmm.mmmm,a,dddmm.mmmm,a,x,xx,x.x,x.x,M,x.x,M,,*hh`

**Example**: `$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47`

**Fields**:
1. `hhmmss.ss` - UTC time (hours:minutes:seconds.centiseconds)
2. `ddmm.mmmm,a` - Latitude (degrees:minutes, N/S)
3. `dddmm.mmmm,a` - Longitude (degrees:minutes, E/W)
4. `x` - Fix quality: 0=none, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=Float RTK
5. `xx` - Satellite count (0-20)
6. `x.x` - Horizontal DOP (HDOP)
7. `x.x` - Altitude above sea level (meters)
8. `M` - Altitude unit (always M = meters)
9. `x.x` - Geoid height (meters)
10. `M` - Geoid unit
11. (empty) - Age of differential data (seconds)
12. `xxxx` - Differential station ID
13. `*hh` - Checksum (XOR of all chars between $ and *)

**Parser**: `parseGPGGA()`

---

### GPRMC / GNRMC: Recommended Minimum Navigation

**Format**: `$GPRMC,hhmmss.ss,a,ddmm.mmmm,a,dddmm.mmmm,a,x.x,x.x,ddmmyy,x.x,a*hh`

**Example**: `$GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A`

**Fields**:
1. `hhmmss.ss` - UTC time
2. `A/V` - Status: A=active/valid, V=void/invalid
3. `ddmm.mmmm,a` - Latitude
4. `dddmm.mmmm,a` - Longitude
5. `x.x` - Speed over ground (knots) → converted to m/s
6. `x.x` - Course true (degrees 0-360)
7. `ddmmyy` - UTC date
8. `x.x` - Magnetic variation (degrees)
9. `a` - Magnetic variation direction (E/W)
10. `*hh` - Checksum

**Parser**: `parseGPRMC()`

---

## Checksum Calculation

**Algorithm**: XOR (exclusive-or) of all characters between `$` and `*`

**Example**: Calculate checksum for `$GNGGA,123519,4807.038,N...`
```
XOR('G','N','G','G','A',',','1',...) = 0x47
Format as hex: *47
```

**Verification**:
```cpp
uint8_t expected = message[message_length - 2:];  // Last 2 chars
uint8_t calculated = validateChecksum(message);
if (expected != calculated) {
  Serial.println("Checksum error - sentence rejected");
}
```

---

## Data Validation Rules

The GPS driver enforces these validation rules before accepting position data:

### Position Validity
- **Accepted**: Fix quality >= 1 AND satellite count >= 4
- **Rejected**: Fix quality = 0 OR satellite count < 4

### HDOP Thresholds
- **Excellent**: HDOP < 1.0 (position accurate to ~1 meter)
- **Good**: HDOP < 2.0 (position accurate to ~3-5 meters)
- **Acceptable**: HDOP < 5.0 (position accurate to ~10-20 meters)
- **Poor**: HDOP >= 5.0 (unreliable, position marked uncertain)

### Latitude/Longitude Ranges
- **Latitude**: -90.0 to +90.0 degrees
- **Longitude**: -180.0 to +180.0 degrees
- Values outside these ranges are rejected

### Altitude Sanity Check
- **Accepted**: -500 to +30000 meters
- **Rejected**: Outside this range (Earth's surface to jet altitude)

### Data Staleness
- **Fresh**: Last update < 1000ms ago
- **Stale**: Last update >= 1000ms ago (sensor marked unhealthy)

---

## Error Codes & Status

### Fix Quality Values
```
0: No fix (data invalid)
1: GPS fix (SPS - Standard Position Service)
2: DGPS fix (Differential GPS)
3: PPS fix (Precise Positioning Service - military)
4: Real Time Kinematic (RTK) - fixed integer
5: Float RTK (intermediate between PPS and RTK)
```

### Status Indicators
- `position_valid`: true if fix quality >= 1 and satellites >= 4
- `new_data`: true if `read()` successfully parsed data this cycle
- `has_lock_`: internal flag for 3D fix capability

### getStatusString() Examples
```
"GPS: 0 sats, HDOP=NAN, NO FIX"           // No lock
"GPS: 4 sats, HDOP=8.50, ACQUIRING"       // Getting fix
"GPS: 8 sats, HDOP=1.20, VALID"           // Good fix
"GPS: 12 sats, HDOP=0.75, VALID"          // Excellent fix
"GPS: 8 sats, HDOP=2.30, STALE"           // Lost recent data
```

---

## Troubleshooting Common Issues

### "GPS not initialized" (begin() returns false)
- **Causes**: 
  - Serial port already in use (check for other devices on Serial1)
  - Invalid baud rate
  - Hardware UART not available on this board
- **Fix**: 
  - Try different UART: `setSerialPort(&Serial2)`
  - Verify baud rate matches GPS module
  - Check platform support in `src/config/pins.h`

### No NMEA sentences parsed (read() always returns false)
- **Causes**:
  - GPS module not sending data (no UART communication)
  - Baud rate mismatch
  - Wiring issue (TX/RX swapped)
- **Fix**:
  - Verify GPS module is powered (check LED)
  - Test with known good UART connection
  - Check TX/RX pins in `src/config/pins.h`
  - Use serial monitor at correct baud rate to verify GPS output

### Position invalid, no satellites (getNumSatellites() = 0)
- **Causes**:
  - GPS module searching for satellites (cold start)
  - Antenna not outdoors or obstructed
  - Defective antenna or module
- **Fix**:
  - Wait 30-60 seconds for cold start
  - Move antenna outdoors with clear sky view
  - Check antenna connection (should be secure)
  - Verify GPS module is getting power

### High HDOP (> 5.0) - position accuracy poor
- **Causes**:
  - Too few satellites (< 6)
  - Poor satellite geometry (all satellites in one direction)
  - Tall buildings or terrain blocking signals
- **Fix**:
  - Wait for more satellites to acquire
  - Move antenna away from obstructions
  - Use a better antenna (gain > 27 dBi recommended)

### Position jumps erratically / drifts
- **Causes**:
  - Low satellite count (< 6)
  - Multipath error (signals bouncing off buildings)
  - Electromagnetic interference
- **Fix**:
  - Ensure HDOP < 2.0
  - Keep antenna away from RF sources (WiFi, cell towers)
  - Use a ground plane under antenna
  - Allow more satellites to acquire (> 10 sats ideal)

---

## Performance Characteristics

### Typical Performance (Good conditions)
- **Time to first fix**: 30-60 seconds (cold start)
- **Update rate**: 1 Hz (1 second between position updates)
- **Accuracy**: ±2-5 meters (HDOP < 2.0)
- **Satellite tracking**: 8-12 satellites typical
- **Data rate**: ~1.1 kbps at 9600 baud, ~13.8 kbps at 115200 baud

### Memory Usage
- **RAM**: ~200 bytes (sentence buffer + state)
- **Flash**: ~4-5 KB (driver code)

### Power Requirements
- **Power consumption**: 50-150 mA at 3.3V or 5V
- **Startup current**: May spike to 200 mA during acquisition
- Recommend separate 5V regulator with 1000µF capacitor

---

## Integration Example

Complete example showing GPS initialization and reading:

```cpp
#include "sensors/gps.h"
#include "config/gps_config.h"

GPS gps;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);

  Serial.println("Initializing GPS...");
  if (!gps.begin()) {
    Serial.println("ERROR: GPS initialization failed");
    while (1) delay(100);
  }
  Serial.println("GPS ready!");
}

void loop() {
  // Read available NMEA sentences
  if (gps.read()) {
    Serial.println(gps.getStatusString());
    
    const auto& pos = gps.getPosition();
    if (pos.position_valid) {
      Serial.printf("Position: %.6f, %.6f\n", 
                    pos.latitude, pos.longitude);
      Serial.printf("Altitude: %.1f m\n", pos.altitude_m);
      Serial.printf("Velocity: %.2f m/s\n", gps.getVelocityMps());
      Serial.printf("HDOP: %.2f\n", gps.getHDOP());
    }
  } else if (!gps.isHealthy()) {
    Serial.println("Waiting for GPS lock...");
  }
  
  delay(100);
}
```

---

## Related Documentation

- [GPS Hardware Setup Guide](../hardware/GPS_HARDWARE_SETUP.md) - How to connect GPS module
- [GPS Troubleshooting Guide](../hardware/GPS_TROUBLESHOOTING.md) - Common issues and solutions
- [Coordinate Frame API Reference](COORDINATE_FRAME_API_REFERENCE.md) - Using GPS position with NED frame
- [Build Guide Phase 2](../build/BUILD_GUIDE_PHASE2.md) - Building with GPS support
- [PHASE_2_MASTER_IMPLEMENTATION_PLAN.md](../phases/PHASE_2_MASTER_IMPLEMENTATION_PLAN.md) - Full implementation details

---

**Last Updated**: 2026-05-07  
**Version**: 1.0  
**Author**: Phase 2 Implementation  
**Status**: Complete and tested
