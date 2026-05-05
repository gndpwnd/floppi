# Auto Orientation System - API Reference

Complete API documentation for all sensor classes, data structures, and output formatters in the auto-orientation system.

## Table of Contents

1. [Data Structures](#data-structures)
2. [Sensor Base Classes](#sensor-base-classes)
3. [BNO085 IMU Sensor](#bno085-imu-sensor)
4. [NEO-M9N GPS Sensor](#neo-m9n-gps-sensor)
5. [Output Formatters](#output-formatters)

---

## Data Structures

### OrientationData

Represents absolute orientation using quaternion representation (w, x, y, z format).

**Header:** `src/sensors/sensor_base.h`

**Fields:**

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `w` | `float` | [-1.0, 1.0] | Quaternion scalar component (real part). Magnitude with x,y,z should normalize to ~1.0 |
| `x` | `float` | [-1.0, 1.0] | Quaternion x component |
| `y` | `float` | [-1.0, 1.0] | Quaternion y component |
| `z` | `float` | [-1.0, 1.0] | Quaternion z component |
| `cal_status` | `uint8_t` | 0-3 | Overall system calibration status: 0=uncalibrated, 1=low, 2=medium, 3=fully calibrated |
| `cal_accel` | `uint8_t` | 0-3 | Accelerometer calibration status |
| `cal_gyro` | `uint8_t` | 0-3 | Gyroscope calibration status |
| `cal_mag` | `uint8_t` | 0-3 | Magnetometer calibration status |
| `timestamp_ms` | `uint32_t` | 0-2^32-1 | Timestamp when data was captured (milliseconds) |

**Example Usage:**

```cpp
#include "sensors/sensor_base.h"

OrientationData orientation;
orientation.w = 0.707f;
orientation.x = 0.0f;
orientation.y = 0.0f;
orientation.z = 0.707f;
orientation.cal_status = 3;  // Fully calibrated
orientation.timestamp_ms = millis();

// Validate quaternion magnitude
float magnitude = sqrt(orientation.w * orientation.w + 
                     orientation.x * orientation.x + 
                     orientation.y * orientation.y + 
                     orientation.z * orientation.z);
if (magnitude > 0.95f && magnitude < 1.05f) {
  // Valid quaternion (normalized to ~1.0)
}
```

**Constructor:**

```cpp
OrientationData();  // Default constructor initializes all fields to 0
```

---

### PositionData

Represents geographic position, accuracy, and satellite information.

**Header:** `src/sensors/sensor_base.h`

**Fields:**

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `latitude` | `double` | [-90.0, 90.0] | Latitude in decimal degrees (negative = south) |
| `longitude` | `double` | [-180.0, 180.0] | Longitude in decimal degrees (negative = west) |
| `altitude` | `float` | Any (typically -500 to 9000) | Altitude above WGS84 ellipsoid in meters |
| `accuracy_m` | `float` | 0.0+ | Estimated horizontal accuracy (CEP) in meters |
| `num_satellites` | `uint8_t` | 0-99 | Number of satellites used in solution |
| `fix_quality` | `uint8_t` | 0-8 | Fix quality indicator: 0=invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK Fixed, 5=RTK Float, 6-8=reserved |
| `timestamp_ms` | `uint32_t` | 0-2^32-1 | Timestamp when fix was computed (milliseconds) |

**Fix Quality Values:**

| Value | Type | Description | Typical Accuracy |
|-------|------|-------------|------------------|
| 0 | Invalid | No fix | N/A |
| 1 | GPS | Standard GPS fix | 2-5m |
| 2 | DGPS | Differential GPS | 0.5-2m |
| 3 | PPS | Precise Point Positioning | 0.1-0.5m |
| 4 | RTK Fixed | RTK with fixed ambiguity | 0.01-0.1m |
| 5 | RTK Float | RTK with float ambiguity | 0.1-0.3m |

**Example Usage:**

```cpp
#include "sensors/sensor_base.h"

PositionData position;
position.latitude = 37.774;      // San Francisco
position.longitude = -122.419;
position.altitude = 52.0f;       // meters
position.accuracy_m = 2.5f;      // 2.5m CEP
position.num_satellites = 8;
position.fix_quality = 1;        // GPS fix
position.timestamp_ms = millis();

if (position.fix_quality >= 1 && position.num_satellites >= 4) {
  // Valid position fix with sufficient satellites
  Serial.print("Lat: "); Serial.println(position.latitude, 6);
  Serial.print("Lon: "); Serial.println(position.longitude, 6);
  Serial.print("Accuracy: "); Serial.print(position.accuracy_m, 1); Serial.println(" m");
}
```

**Constructor:**

```cpp
PositionData();  // Default constructor initializes all fields to 0
```

---

### SensorOutput

Combined sensor output with both orientation and position data plus metadata.

**Header:** `src/sensors/sensor_base.h`

**Fields:**

| Field | Type | Description |
|-------|------|-------------|
| `orientation` | `OrientationData` | Absolute orientation data (quaternion) |
| `position` | `PositionData` | Geographic position data |
| `orientation_valid` | `bool` | True if orientation data is valid |
| `position_valid` | `bool` | True if position data is valid |
| `system_time_ms` | `uint32_t` | System timestamp when output was generated |

---

## Sensor Base Classes

### Sensor (Base Class)

Abstract base class defining the interface for all sensor types.

**Header:** `src/sensors/sensor_base.h`

**Methods:**

#### `bool begin()`

Initialize sensor hardware and prepare for reading.

**Parameters:** None

**Returns:** `true` if initialization successful, `false` on error

**Notes:**
- Called once during system startup
- Should configure hardware (serial ports, pins, etc.)
- Non-blocking; should complete within 100ms
- If begin() fails, do not call other methods

**Example:**

```cpp
Sensor* sensor = new SomeSensor();
if (!sensor->begin()) {
  Serial.println("Sensor initialization failed!");
  return;
}
```

---

#### `void end()`

Shutdown sensor and release resources.

**Parameters:** None

**Returns:** None

**Notes:**
- Called during system shutdown or error recovery
- Should close serial connections, free memory, etc.
- Safe to call multiple times
- After end(), calling read() will return false

**Example:**

```cpp
sensor->end();
delete sensor;
```

---

#### `bool isInitialized() const`

Check if sensor has been successfully initialized.

**Parameters:** None

**Returns:** `true` if begin() completed successfully and end() has not been called

**Notes:**
- Non-blocking; safe to call in tight loops
- Returns false immediately after construction until begin() succeeds

**Example:**

```cpp
if (sensor->isInitialized()) {
  sensor->read();
}
```

---

#### `bool read()`

Attempt to read latest sensor data.

**Parameters:** None

**Returns:** `true` if new data was read, `false` if no new data available or read failed

**Notes:**
- **Non-blocking**: Should not delay or wait
- Must be called frequently (at least 10+ Hz) to avoid missing data
- Data availability determined by `hasNewData()`
- Safe to call even if no new data available

**Example:**

```cpp
void loop() {
  if (sensor->read()) {
    // New data available
    if (sensor->hasNewData()) {
      // Process data
    }
  }
}
```

---

#### `bool hasNewData() const`

Check if new sensor data is available since last check.

**Parameters:** None

**Returns:** `true` if new data has been read since last call to hasNewData()

**Notes:**
- Flag is typically cleared after first call (polling behavior)
- Safe to call without calling read() first
- Returns false on subsequent calls until read() succeeds again

**Example:**

```cpp
if (sensor->read()) {
  if (sensor->hasNewData()) {
    // Process new data
  }
}
```

---

#### `const char* name() const`

Get human-readable sensor name.

**Parameters:** None

**Returns:** Pointer to static string (e.g., "BNO085", "NEO-M9N GPS")

**Notes:**
- String pointer is valid for lifetime of sensor object
- Used for logging and status display
- String should be short (< 20 characters)

**Example:**

```cpp
Serial.print("Sensor: ");
Serial.println(sensor->name());  // Prints "Sensor: BNO085"
```

---

#### `bool isHealthy() const`

Check if sensor is operating normally.

**Parameters:** None

**Returns:** `true` if sensor is functional and has valid data

**Notes:**
- For orientation sensors: true if calibration is adequate
- For position sensors: true if fix quality is sufficient
- Non-blocking; instant status check

**Example:**

```cpp
if (!sensor->isHealthy()) {
  Serial.println("Sensor unhealthy!");
  Serial.println(sensor->getStatusString());
}
```

---

#### `const char* getStatusString() const`

Get human-readable status message.

**Parameters:** None

**Returns:** Pointer to static buffer with status string

**Notes:**
- String is short-lived; copy if needed beyond current scope
- Includes relevant diagnostic information:
  - BNO085: calibration levels, reset status
  - NEO-M9N: satellite count, fix type, accuracy
- Examples:
  - "BNO085: System cal 3/3, Accel 3/3, Gyro 3/3, Mag 2/3"
  - "GPS: 8 sats, GPS fix, HDOP 0.9m"

**Example:**

```cpp
Serial.println(sensor->getStatusString());
```

---

### OrientationSensor (Base Class)

Specialized sensor interface for absolute orientation devices.

**Header:** `src/sensors/sensor_base.h`

**Inherits from:** `Sensor`

**Methods:**

#### `const OrientationData& getOrientation() const`

Get latest orientation data.

**Parameters:** None

**Returns:** Reference to `OrientationData` struct

**Notes:**
- Returns last valid data even if no new data since last read()
- Reference is valid only until next call to read()
- Safe to copy the returned struct for later use

**Example:**

```cpp
const OrientationData& orientation = orientation_sensor->getOrientation();
Serial.print("Q: ");
Serial.print(orientation.w, 4);
Serial.print(",");
Serial.print(orientation.x, 4);
Serial.print(",");
Serial.print(orientation.y, 4);
Serial.print(",");
Serial.println(orientation.z, 4);
```

---

#### `bool setCalibrationProfile(const uint8_t* profile_data, uint16_t length)`

Restore calibration data from previously saved profile.

**Parameters:**
- `profile_data`: Pointer to calibration data buffer
- `length`: Size of buffer in bytes

**Returns:** `true` if calibration restored successfully, `false` on error

**Notes:**
- Used to restore previously saved calibration state
- Data should come from `getCalibrationProfile()`
- Some sensors may not support this; check implementation
- Safe to call multiple times

**Example:**

```cpp
uint8_t cal_data[256];
uint16_t cal_length;
// Load cal_data from EEPROM...
if (imu->setCalibrationProfile(cal_data, cal_length)) {
  Serial.println("Calibration restored");
}
```

---

#### `bool getCalibrationProfile(uint8_t* profile_data, uint16_t* length)`

Save current calibration state for later restoration.

**Parameters:**
- `profile_data`: Pointer to buffer for saving calibration data
- `length`: Pointer to variable receiving size of data written

**Returns:** `true` if calibration saved successfully, `false` on error

**Notes:**
- Saves internal calibration state to provided buffer
- Returned length indicates bytes written
- Buffer must be at least 256 bytes for BNO085
- Data can be saved to EEPROM for persistence across power cycles

**Example:**

```cpp
uint8_t cal_data[256];
uint16_t cal_length;
if (imu->getCalibrationProfile(cal_data, &cal_length)) {
  Serial.print("Calibration saved: ");
  Serial.print(cal_length);
  Serial.println(" bytes");
  // Save cal_data to EEPROM or SD card...
}
```

---

### PositionSensor (Base Class)

Specialized sensor interface for position/navigation devices.

**Header:** `src/sensors/sensor_base.h`

**Inherits from:** `Sensor`

**Methods:**

#### `const PositionData& getPosition() const`

Get latest position data.

**Parameters:** None

**Returns:** Reference to `PositionData` struct

**Notes:**
- Returns last valid fix even if no new data since last read()
- Reference is valid only until next call to read()
- Check `fix_quality` field to determine if data is valid
- Safe to copy the returned struct for later use

**Example:**

```cpp
const PositionData& position = gps->getPosition();
if (position.fix_quality >= 1) {
  Serial.print("Lat: ");
  Serial.print(position.latitude, 6);
  Serial.print(" Lon: ");
  Serial.println(position.longitude, 6);
  Serial.print("Accuracy: ");
  Serial.print(position.accuracy_m, 1);
  Serial.println(" m");
}
```

---

## BNO085 IMU Sensor

Complete documentation for the BNO085 absolute orientation IMU sensor.

**Header:** `src/sensors/bno085.h`

**Source:** `src/sensors/bno085.cpp`

**Hardware:**
- Adafruit BNO085 (or compatible Bosch BNO08x)
- Interface: UART (115200 baud)
- Pin configuration: See `config/pins.h`

**Features:**
- 9-axis absolute orientation (accel + gyro + mag)
- Onboard sensor fusion (calculates quaternion)
- 256-byte persistent calibration memory
- Automatic reset detection and recovery

### BNO085 Class

```cpp
class BNO085 : public OrientationSensor
```

**Constructor:**

```cpp
BNO085()
```

Initializes sensor object (does not communicate with hardware).

**Destructor:**

```cpp
virtual ~BNO085()
```

Calls `end()` to release resources.

---

### BNO085::begin()

Initialize BNO085 sensor via UART.

**Signature:**

```cpp
bool begin() override
```

**Parameters:** None (uses pins from config/pins.h)

**Returns:** `true` if sensor initialized successfully

**Error Conditions:**
- UART initialization failed
- Sensor not responding on UART
- enableReport() failed

**Hardware Setup Required:**
1. Connect BNO085 TX to configured RX pin
2. Connect BNO085 RX to configured TX pin
3. Connect BNO085 GND to Arduino GND
4. Connect BNO085 VCC to 3.3V (with voltage regulator)

**Typical Duration:** 50-100ms

**Example:**

```cpp
BNO085 imu;
if (!imu.begin()) {
  Serial.println("BNO085 initialization failed!");
  Serial.println("Check UART connections");
  while (1) delay(1000);
}
Serial.println("BNO085 ready");
```

**Notes:**
- Enables rotation vector report at 10 Hz (100ms period)
- Board-specific: Uses Serial1 on Mega, Teensy, ESP32; configurable on others
- After successful begin(), sensor starts outputting data immediately

---

### BNO085::end()

Shutdown BNO085 and release resources.

**Signature:**

```cpp
void end() override
```

**Parameters:** None

**Returns:** None

**Notes:**
- Closes UART connection
- Deletes internal Adafruit_BNO08x object
- Safe to call multiple times
- After end(), must call begin() before using sensor again

**Example:**

```cpp
imu.end();
```

---

### BNO085::isInitialized()

Check if BNO085 is initialized.

**Signature:**

```cpp
bool isInitialized() const override
```

**Parameters:** None

**Returns:** `true` if begin() completed successfully and end() not called

**Example:**

```cpp
if (imu.isInitialized()) {
  imu.read();
}
```

---

### BNO085::read()

Read latest quaternion data from BNO085.

**Signature:**

```cpp
bool read() override
```

**Parameters:** None

**Returns:** `true` if new quaternion data was read

**Behavior:**
1. Checks if sensor was reset (re-enables reports if so)
2. Queries Adafruit library for new sensor events
3. Reads rotation vector (quaternion) on success
4. Updates calibration status fields
5. Sets new_data_ flag

**Timing:** ~1-5ms (non-blocking)

**Frequency:** Should call at least 10+ Hz to maintain real-time operation

**Example:**

```cpp
void loop() {
  if (imu.read()) {
    if (imu.hasNewData()) {
      const OrientationData& q = imu.getOrientation();
      // Process quaternion
    }
  }
}
```

**Notes:**
- Non-blocking; returns immediately
- Reports available at ~10 Hz (100ms intervals)
- Handles sensor reset transparently

---

### BNO085::hasNewData()

Check if new quaternion data is available.

**Signature:**

```cpp
bool hasNewData() const override
```

**Parameters:** None

**Returns:** `true` if new data available since last call to hasNewData()

**Notes:**
- Flag is cleared after first call
- Use in polling loops to process data at variable rate
- Multiple calls without read() will return false

**Example:**

```cpp
if (imu.read()) {
  if (imu.hasNewData()) {
    // Process new quaternion
  }
}
```

---

### BNO085::getOrientation()

Get latest quaternion orientation data.

**Signature:**

```cpp
const OrientationData& getOrientation() const override
```

**Parameters:** None

**Returns:** Reference to internal `OrientationData` struct

**Fields Populated:**
- `w, x, y, z`: Quaternion components (magnitude ~1.0)
- `cal_status, cal_accel, cal_gyro, cal_mag`: Calibration levels (0-3)
- `timestamp_ms`: Time when quaternion was captured

**Data Validity:**
- Magnitude should be 0.95-1.05 (check with `sqrt(w² + x² + y² + z²)`)
- Calibration status indicates reliability:
  - 3: Fully calibrated, highest accuracy
  - 2: Medium calibration, good for most uses
  - 1: Low calibration, use with caution
  - 0: Uncalibrated, unreliable data

**Example:**

```cpp
const OrientationData& orientation = imu.getOrientation();

// Validate quaternion
float mag = sqrt(orientation.w * orientation.w +
                 orientation.x * orientation.x +
                 orientation.y * orientation.y +
                 orientation.z * orientation.z);

if (mag < 0.95 || mag > 1.05) {
  Serial.println("Invalid quaternion magnitude!");
  return;
}

// Use quaternion...
Serial.print("Quat: ");
Serial.print(orientation.w, 4);
Serial.print(",");
Serial.print(orientation.x, 4);
Serial.print(",");
Serial.print(orientation.y, 4);
Serial.print(",");
Serial.println(orientation.z, 4);

// Check calibration
if (orientation.cal_status == 3) {
  Serial.println("Fully calibrated - high accuracy");
}
```

---

### BNO085::isHealthy()

Check if BNO085 is operating normally.

**Signature:**

```cpp
bool isHealthy() const override
```

**Parameters:** None

**Returns:** `true` if sensor is responding and has adequate calibration

**Criteria:**
- Sensor is initialized
- Last read was within reasonable timeout (5 seconds)
- Calibration status > 0 (at least partially calibrated)

**Example:**

```cpp
if (!imu.isHealthy()) {
  Serial.println("BNO085 unhealthy!");
  Serial.println(imu.getStatusString());
  // Consider stopping application or using fallback
}
```

---

### BNO085::getStatusString()

Get human-readable status message.

**Signature:**

```cpp
const char* getStatusString() const override
```

**Parameters:** None

**Returns:** Pointer to status string (static buffer, ~80 characters)

**String Format:**

```
"BNO085: System cal X/3, Accel X/3, Gyro X/3, Mag X/3"
```

**Example Outputs:**
- `"BNO085: System cal 3/3, Accel 3/3, Gyro 3/3, Mag 3/3"` - Excellent
- `"BNO085: System cal 2/3, Accel 3/3, Gyro 3/3, Mag 1/3"` - Good (low mag cal)
- `"BNO085: Not initialized"` - Error state

**Example:**

```cpp
Serial.println(imu.getStatusString());
// Output: "BNO085: System cal 3/3, Accel 3/3, Gyro 3/3, Mag 2/3"
```

---

### BNO085::setCalibrationProfile()

Restore calibration data from saved profile.

**Signature:**

```cpp
bool setCalibrationProfile(const uint8_t* profile_data, 
                          uint16_t length) override
```

**Parameters:**
- `profile_data`: Pointer to 256-byte calibration buffer
- `length`: Size of buffer (should be 256 for BNO085)

**Returns:** `true` if restoration successful

**Usage Scenario:**
```cpp
// At startup: load saved calibration
uint8_t saved_cal[256];
// Load saved_cal from EEPROM...
imu.setCalibrationProfile(saved_cal, 256);

// Later: save new calibration
uint8_t current_cal[256];
uint16_t cal_len;
imu.getCalibrationProfile(current_cal, &cal_len);
// Save current_cal to EEPROM...
```

**Notes:**
- Calibration data is stored in BNO085 NVM
- Persists across power cycles even without explicit save
- Use this for faster convergence after cold start

---

### BNO085::getCalibrationProfile()

Save current calibration state.

**Signature:**

```cpp
bool getCalibrationProfile(uint8_t* profile_data,
                          uint16_t* length) override
```

**Parameters:**
- `profile_data`: Pointer to buffer for calibration data (min 256 bytes)
- `length`: Pointer to uint16_t receiving number of bytes written

**Returns:** `true` if save successful

**Output:**
- Buffer filled with calibration bytes
- `*length` = number of bytes written (typically 256)

**Example:**

```cpp
uint8_t cal_buffer[256];
uint16_t cal_length;

if (imu.getCalibrationProfile(cal_buffer, &cal_length)) {
  Serial.print("Saved ");
  Serial.print(cal_length);
  Serial.println(" bytes of calibration");
  
  // Save to EEPROM
  for (int i = 0; i < cal_length; i++) {
    EEPROM.write(EEPROM_CAL_OFFSET + i, cal_buffer[i]);
  }
  EEPROM.commit();
}
```

---

## NEO-M9N GPS Sensor

Complete documentation for the NEO-M9N multi-band GNSS receiver.

**Header:** `src/sensors/neo_m9n.h`

**Source:** `src/sensors/neo_m9n.cpp`

**Hardware:**
- Ublox NEO-M9N receiver
- Interface: USB serial (CDC) or UART (115200 baud)
- Constellations: GPS, GLONASS, Galileo, BeiDou
- Features: RTK capable, multi-band

### NEOM9N Class

```cpp
class NEOM9N : public PositionSensor
```

**Constructor:**

```cpp
NEOM9N()
```

Initializes GPS object (does not open serial connection).

**Destructor:**

```cpp
virtual ~NEOM9N()
```

Calls `end()` to close serial connection.

---

### NEOM9N::begin()

Initialize GPS receiver and open serial connection.

**Signature:**

```cpp
bool begin() override
```

**Parameters:** None (uses pins from config/pins.h)

**Returns:** `true` if serial port opened successfully

**Serial Port Selection (board-dependent):**

| Board | Serial Port | Pins |
|-------|-------------|------|
| Arduino Mega | Serial3 | RX3=15, TX3=14 |
| Teensy 3.x | Serial2 | - |
| ESP32 | Serial2 | 16/17 |
| Others | Serial (USB) | - |

**Baud Rate:** 115200 (from config/pins.h GPS_BAUD_RATE)

**Typical Duration:** 100-200ms

**Example:**

```cpp
NEOM9N gps;
if (!gps.begin()) {
  Serial.println("GPS initialization failed!");
  while (1) delay(1000);
}
Serial.println("GPS ready, waiting for fix...");
```

**Notes:**
- GPS acquisition time:
  - Cold start (no ephemeris): 30-60 seconds
  - Warm start (ephemeris cached): 5-10 seconds
  - Hot start (recent position): 1-5 seconds
- First fix may take 30+ seconds after power-on

---

### NEOM9N::end()

Close GPS serial connection.

**Signature:**

```cpp
void end() override
```

**Parameters:** None

**Returns:** None

**Example:**

```cpp
gps.end();
```

---

### NEOM9N::isInitialized()

Check if GPS serial connection is open.

**Signature:**

```cpp
bool isInitialized() const override
```

**Parameters:** None

**Returns:** `true` if begin() completed and end() not called

---

### NEOM9N::read()

Read and parse available NMEA sentences.

**Signature:**

```cpp
bool read() override
```

**Parameters:** None

**Returns:** `true` if a complete NMEA sentence was parsed successfully

**NMEA Sentences Supported:**

| Sentence | Type | Data |
|----------|------|------|
| GPGGA | Position | Lat/Lon, altitude, fix quality, satellites, HDOP |
| GPRMC | Position | Lat/Lon, speed, course |

**Parsing Details:**
1. Reads bytes from serial until complete sentence detected (`$...*XX`)
2. Validates checksum (XOR of all characters between $ and *)
3. Identifies sentence type (GPGGA or GPRMC)
4. Extracts fields and updates `position_` struct
5. Sets `new_data_` flag on success

**Timing:** ~1-10ms (depends on sentence size and checksum validation)

**Frequency:** ~1 Hz (one sentence per second typical)

**Example:**

```cpp
void loop() {
  // Non-blocking read
  if (gps.read()) {
    if (gps.hasNewData()) {
      const PositionData& pos = gps.getPosition();
      if (pos.fix_quality >= 1) {
        Serial.print("Lat: ");
        Serial.print(pos.latitude, 6);
        Serial.print(" Lon: ");
        Serial.println(pos.longitude, 6);
      }
    }
  }
}
```

**NMEA Sentence Format:**

```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
```

**Notes:**
- One complete sentence arrives approximately once per second
- Multiple sentences may arrive together; `read()` processes one per call
- HDOP (Horizontal Dilution of Precision) converted to accuracy_m (HDOP * 5)

---

### NEOM9N::hasNewData()

Check if new position data is available.

**Signature:**

```cpp
bool hasNewData() const override
```

**Parameters:** None

**Returns:** `true` if new position parsed since last call to hasNewData()

**Notes:**
- Flag is cleared after first call to hasNewData()
- Returns `false` on subsequent calls until read() successfully parses another sentence
- Typical call rate: ~1 per second (matches sentence rate)

**Example:**

```cpp
if (gps.read()) {
  if (gps.hasNewData()) {
    // Process new position fix
  }
}
```

---

### NEOM9N::getPosition()

Get latest position data.

**Signature:**

```cpp
const PositionData& getPosition() const override
```

**Parameters:** None

**Returns:** Reference to internal `PositionData` struct

**Fields Populated:**

| Field | Source | Notes |
|-------|--------|-------|
| `latitude` | GPGGA | Degrees, negative = south |
| `longitude` | GPGGA | Degrees, negative = west |
| `altitude` | GPGGA | Meters above WGS84 ellipsoid |
| `accuracy_m` | GPGGA | HDOP * 5 (estimated CEP) |
| `num_satellites` | GPGGA | Count of satellites in solution |
| `fix_quality` | GPGGA | 0=invalid, 1=GPS fix, 2=DGPS, etc. |
| `timestamp_ms` | read() | Time when sentence was parsed |

**Data Validity Check:**

```cpp
const PositionData& pos = gps.getPosition();

if (pos.fix_quality == 0) {
  // No fix
  Serial.println("Waiting for GPS fix...");
} else if (pos.num_satellites < 4) {
  // Fix but insufficient satellites
  Serial.println("Fix quality marginal (< 4 satellites)");
} else {
  // Valid fix
  Serial.print("Lat: ");
  Serial.print(pos.latitude, 6);
  Serial.print(" Lon: ");
  Serial.println(pos.longitude, 6);
}
```

**Accuracy Guidance:**

| Satellites | HDOP | Accuracy (CEP) | Use Case |
|-----------|------|---|---|
| < 4 | > 10 | > 50m | Not reliable |
| 4-6 | 5-10 | 25-50m | Outdoor, non-critical |
| 7-8 | 2-5 | 10-25m | Good outdoor |
| 9+ | < 2 | < 10m | Excellent |

**Example:**

```cpp
const PositionData& pos = gps.getPosition();

if (pos.fix_quality >= 1 && pos.num_satellites >= 7) {
  Serial.print("Position: ");
  Serial.print(pos.latitude, 6);
  Serial.print(", ");
  Serial.print(pos.longitude, 6);
  Serial.print(" (±");
  Serial.print(pos.accuracy_m, 1);
  Serial.println("m)");
}
```

---

### NEOM9N::isHealthy()

Check if GPS has valid position fix.

**Signature:**

```cpp
bool isHealthy() const override
```

**Parameters:** None

**Returns:** `true` if fix quality >= 1 (has valid position)

**Example:**

```cpp
if (gps.isHealthy()) {
  const PositionData& pos = gps.getPosition();
  Serial.println("GPS: Position valid");
} else {
  Serial.println("GPS: No fix");
}
```

---

### NEOM9N::getStatusString()

Get human-readable GPS status message.

**Signature:**

```cpp
const char* getStatusString() const override
```

**Parameters:** None

**Returns:** Pointer to status string (static buffer)

**String Format:**

```
"GPS: <satellites> sats, <fix_type>, HDOP <value>m"
```

**Example Outputs:**
- `"GPS: Not initialized"` - Serial not opened
- `"GPS: No fix"` - Searching for satellites
- `"GPS: 6 sats, GPS fix, HDOP 5.0m"` - Marginal fix
- `"GPS: 9 sats, GPS fix, HDOP 0.8m"` - Excellent fix
- `"GPS: 12 sats, DGPS fix, HDOP 0.5m"` - RTK capable

**Example:**

```cpp
Serial.println(gps.getStatusString());
// Output: "GPS: 8 sats, GPS fix, HDOP 1.2m"
```

---

## Output Formatters

Interfaces for formatting sensor data in different output formats.

**Header:** `src/output/data_formatter.h`

### DataFormatter (Base Class)

Abstract base class for all output formatters.

```cpp
class DataFormatter
```

**Methods:**

#### `uint16_t format(...)`

Format sensor data into string output.

**Signature:**

```cpp
virtual uint16_t format(const OrientationData& orientation,
                       const PositionData& position,
                       char* buffer,
                       uint16_t max_len) = 0
```

**Parameters:**
- `orientation`: Orientation data (may be invalid)
- `position`: Position data (may be invalid)
- `buffer`: Output buffer for formatted string
- `max_len`: Maximum buffer length (bytes)

**Returns:** Number of bytes written to buffer (excluding null terminator)

**Example:**

```cpp
JSONFormatter formatter;
char output[512];
uint16_t bytes_written = formatter.format(orientation, position, 
                                          output, sizeof(output));
Serial.write((uint8_t*)output, bytes_written);
Serial.write('\n');
```

---

#### `uint16_t formatHeader(...)`

Format header row (for CSV formatters).

**Signature:**

```cpp
virtual uint16_t formatHeader(char* buffer, uint16_t max_len)
```

**Parameters:**
- `buffer`: Output buffer
- `max_len`: Maximum buffer length

**Returns:** Number of bytes written (0 if no header needed)

**Notes:**
- Used by CSV formatters to output column names
- JSON formatters typically return 0 (no headers used)

---

#### `const char* getFormatName() const`

Get format name.

**Signature:**

```cpp
virtual const char* getFormatName() const = 0
```

**Returns:** Format name string (e.g., "JSON", "CSV")

---

### JSONFormatter

Formats orientation + position data as compact JSON.

**Header:** `src/output/json_formatter.h`

**Constructor:**

```cpp
JSONFormatter(uint8_t decimal_places = 3)
```

**Parameters:**
- `decimal_places`: Number of decimal places for float values (default: 3)

**Output Format:**

```json
{
  "timestamp_ms": 123456,
  "orientation": {
    "valid": true,
    "quaternion": {"w": 0.707, "x": 0.0, "y": 0.0, "z": 0.707},
    "calibration": {"system": 3, "accel": 3, "gyro": 3, "mag": 2}
  },
  "position": {
    "valid": true,
    "latitude": 37.774,
    "longitude": -122.419,
    "altitude_m": 52.0,
    "accuracy_m": 2.5,
    "num_satellites": 8,
    "fix_quality": 1
  }
}
```

**Example Usage:**

```cpp
#include "output/json_formatter.h"

JSONFormatter json_fmt(4);  // 4 decimal places

void outputData(const BNO085& imu, const NEOM9N& gps) {
  char buffer[512];
  uint16_t len = json_fmt.format(imu.getOrientation(),
                                 gps.getPosition(),
                                 buffer,
                                 sizeof(buffer));
  Serial.write((uint8_t*)buffer, len);
  Serial.write('\n');
}
```

**Notes:**
- Compact JSON (no unnecessary whitespace)
- Handles invalid sensors gracefully (null values, valid:false)
- Configurable decimal precision for float accuracy

---

## Error Handling

### Common Error Patterns

**Sensor initialization fails:**

```cpp
BNO085 imu;
if (!imu.begin()) {
  // Possible causes:
  // 1. UART pins not connected
  // 2. Sensor not powered (3.3V)
  // 3. Wrong baud rate in pins.h
  // 4. Sensor already in use by another component
}
```

**No new data available:**

```cpp
imu.read();  // Returns true/false
if (imu.hasNewData()) {
  // Process data
} else {
  // No new data available yet
  // This is normal - sensor outputs at fixed rate (~10 Hz)
}
```

**Invalid quaternion:**

```cpp
const OrientationData& q = imu.getOrientation();
float mag = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
if (mag < 0.95 || mag > 1.05) {
  // Quaternion not normalized - data may be corrupted
  // Try reading again or checking calibration
}
```

**GPS no fix:**

```cpp
const PositionData& pos = gps.getPosition();
if (pos.fix_quality == 0) {
  // Waiting for initial fix (cold start: 30-60 seconds)
  // Check:
  // 1. GPS antenna position (clear sky view)
  // 2. USB connection (for initial NMEA data)
  // 3. Sufficient satellites (check getStatusString())
}
```

---

## Board Support

### Arduino Mega 2560

- BNO085: Serial1 (115200 baud)
- NEO-M9N: Serial3 (115200 baud)
- Supported: Yes (fully tested)

### Teensy 3.x

- BNO085: Serial1
- NEO-M9N: Serial2
- Supported: Yes (partial)

### ESP32

- BNO085: Serial1 (pins 9/10)
- NEO-M9N: Serial2 (pins 16/17)
- Supported: Yes (configurable)

### Arduino Nano / Uno

- BNO085: Not supported (no dual UARTs)
- NEO-M9N: Not supported
- Supported: No

---

## Performance Notes

### Timing Characteristics

| Operation | Typical Time | Notes |
|-----------|--------------|-------|
| BNO085::begin() | 50-100ms | UART init + sensor handshake |
| BNO085::read() | 1-5ms | Non-blocking |
| BNO085::getStatusString() | < 1ms | Buffer writes |
| NEOM9N::begin() | 100-200ms | Serial init |
| NEOM9N::read() | 1-10ms | Depends on sentence |
| GPS acquisition (cold start) | 30-60s | First fix |
| GPS acquisition (warm start) | 5-10s | With cached ephemeris |

### Memory Usage

| Component | Typical Size |
|-----------|--------------|
| BNO085 object | ~50 bytes |
| NEOM9N object | ~300 bytes |
| Calibration buffer | 256 bytes |
| Status string buffer | ~80 bytes |

---

## Troubleshooting

### BNO085 Issues

**Symptom:** `begin()` fails

- Check UART connections
- Verify 3.3V power supply
- Confirm sensor is not already initialized elsewhere
- Check config/pins.h for correct board type

**Symptom:** Poor calibration (cal_status < 2)

- Move sensor through all orientations (figure-8 motion)
- Keep away from magnetic interference (motors, power supplies)
- Allow 30+ seconds of motion for full calibration
- Check magnetometer calibration especially

**Symptom:** Quaternion magnitude out of range

- Sensor data corruption (check UART connections)
- Arithmetic overflow in application code
- Try resetting sensor: call end() then begin()

### NEO-M9N Issues

**Symptom:** `begin()` returns true but no GPS data

- GPS not connected to configured serial port
- Check pins in config/pins.h
- Verify 3.3V power supply to GPS module
- antenna not connected or poorly positioned

**Symptom:** Long time to first fix

- Cold start (30-60s): Normal with no ephemeris
- Warm start (5-10s): Normal with cached data
- Check antenna placement (needs clear sky view)
- Check satellite count (getStatusString())

**Symptom:** Position jumping/drifting

- Low satellite count (need >= 4, preferably 8+)
- Poor HDOP (check accuracy_m field)
- NMEA sentences corrupted (check checksum)
- GPS still acquiring (first 1-2 minutes may be unstable)

---

## See Also

- [ADDING_NEW_SENSORS.md](../guides/ADDING_NEW_SENSORS.md) - Developer guide for custom sensor implementation
- [config/pins.h](../../src/config/pins.h) - Hardware pin configuration
- [BNO085 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bno085-ds000.pdf)
- [NEO-M9N Datasheet](https://www.u-blox.com/sites/default/files/products/documents/NEO-M9N_DataSheet_UBX-21045505.pdf)
- [Adafruit BNO08x Library](https://github.com/adafruit/Adafruit_BNO08x_Arduino)
