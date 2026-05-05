# Adding New Sensors - Developer Guide

Complete step-by-step guide for integrating custom sensors into the auto-orientation system.

## Overview

The auto-orientation system uses a modular sensor architecture with clear base classes and interfaces. This guide walks you through adding a new sensor, whether orientation-based (IMU), position-based (GPS), or custom data types.

### Design Principles

1. **Non-blocking operations**: All `read()` calls must complete in < 5ms
2. **Clear error states**: Sensors must report health status via `isHealthy()` and `getStatusString()`
3. **Data isolation**: Each sensor owns its data structures; no global state
4. **Graceful degradation**: System works with partial sensors (IMU only, GPS only, etc.)

---

## Step 1: Choose Sensor Base Class

Decide which base class your sensor should inherit from:

### Option A: Orientation Sensor (IMU)

Use if your sensor outputs **absolute orientation** (quaternion, Euler angles, etc.).

**Base Class:** `OrientationSensor : public Sensor`

**Required Methods:**
- `bool begin()` - Initialize
- `bool read()` - Read latest data
- `bool hasNewData() const` - Check for new data
- `const OrientationData& getOrientation() const` - Get quaternion
- `bool setCalibrationProfile(const uint8_t* data, uint16_t len)` - Restore calibration
- `bool getCalibrationProfile(uint8_t* data, uint16_t* len)` - Save calibration

**Example Sensors:**
- BNO085 (Bosch 9-axis IMU) - **Already implemented**
- MPU6050 (InvenSense 6-axis IMU)
- LSM6DSOX (STMicroelectronics 6-axis IMU)

### Option B: Position Sensor (GNSS)

Use if your sensor outputs **geographic position** (latitude, longitude, altitude).

**Base Class:** `PositionSensor : public Sensor`

**Required Methods:**
- `bool begin()` - Initialize
- `bool read()` - Read latest data
- `bool hasNewData() const` - Check for new data
- `const PositionData& getPosition() const` - Get position

**Example Sensors:**
- NEO-M9N (Ublox multi-band GNSS) - **Already implemented**
- NEO-6M (Ublox basic GPS)
- SIM68R (u-blox SIM card with GPS)

### Option C: Custom Sensor Type

If your sensor doesn't fit these categories, extend the base `Sensor` class and create a new data structure.

---

## Step 2: Create Data Structure (if needed)

If your sensor outputs data not covered by `OrientationData` or `PositionData`, define a custom struct in `sensor_base.h`.

**Example: Altitude Sensor**

```cpp
struct AltitudeData {
  float altitude_m;           // Meters
  float vertical_speed_ms;    // Meters per second
  float temperature_c;        // Celsius (for calibration)
  uint8_t status;             // 0=invalid, 1=valid
  uint32_t timestamp_ms;
  
  AltitudeData() :
    altitude_m(0), vertical_speed_ms(0), temperature_c(0),
    status(0), timestamp_ms(0) {}
};
```

Add this to `src/sensors/sensor_base.h` (update `SENSOR_BASE_H` guard):

```cpp
// Near OrientationData and PositionData definitions
struct AltitudeData {
  // ... fields ...
};
```

---

## Step 3: Create Header File

Create `src/sensors/YOUR_SENSOR.h` with the class declaration.

**Template for OrientationSensor:**

```cpp
/**
 * YOUR_SENSOR - Description
 *
 * Hardware: Vendor, model, interface (UART, I2C, SPI)
 * Features: List key capabilities
 * Link: https://vendor.com/datasheet.pdf
 */

#ifndef YOUR_SENSOR_H
#define YOUR_SENSOR_H

#include "sensor_base.h"

/**
 * YOUR_SENSOR Class
 *
 * Brief description of what sensor does and how it works.
 */
class YourSensor : public OrientationSensor {
 public:
  // Lifecycle
  YourSensor();
  virtual ~YourSensor();

  // Sensor interface (required)
  bool begin() override;
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "YourSensor"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  // Orientation interface (required)
  const OrientationData& getOrientation() const override;
  bool setCalibrationProfile(const uint8_t* profile_data, 
                            uint16_t length) override;
  bool getCalibrationProfile(uint8_t* profile_data,
                            uint16_t* length) override;

 private:
  // Hardware interface
  // Example: HardwareSerial* uart_;
  // Example: I2CInterface* i2c_;

  // Data storage
  OrientationData orientation_;
  bool initialized_;
  bool new_data_;
  uint32_t last_read_ms_;

  // Calibration (if supported)
  uint8_t calibration_data_[256];
  uint16_t calibration_data_length_;

  // Helper methods
  // Example: bool parseData(uint8_t* buffer);
  // Example: bool sendCommand(const char* cmd);
};

#endif  // YOUR_SENSOR_H
```

**Template for PositionSensor:**

```cpp
#ifndef YOUR_GPS_H
#define YOUR_GPS_H

#include "sensor_base.h"

class YourGPS : public PositionSensor {
 public:
  YourGPS();
  virtual ~YourGPS();

  // Sensor interface
  bool begin() override;
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "YourGPS"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  // Position interface
  const PositionData& getPosition() const override;

 private:
  PositionData position_;
  bool initialized_;
  bool new_data_;
  uint32_t last_read_ms_;
};

#endif  // YOUR_GPS_H
```

---

## Step 4: Create Implementation File

Create `src/sensors/YOUR_SENSOR.cpp` with the implementation.

**Key Rules:**
1. All operations must be **non-blocking** (complete in < 5ms)
2. No delays or busy loops in `read()`
3. Use `millis()` for timeout detection instead of `delay()`
4. Handle initialization failures gracefully

### Constructor & Destructor

```cpp
#include "your_sensor.h"
#include "../config/pins.h"
#include <Arduino.h>
#include <string.h>

YourSensor::YourSensor()
    : initialized_(false),
      new_data_(false),
      last_read_ms_(0),
      calibration_data_length_(0) {
  memset(calibration_data_, 0, sizeof(calibration_data_));
}

YourSensor::~YourSensor() {
  end();
}
```

### begin() - Initialize Hardware

```cpp
bool YourSensor::begin() {
  // Step 1: Configure serial/I2C/SPI interface
  // Example for UART:
  Serial1.begin(115200);

  // Step 2: Initialize hardware (send init commands, wait for ready)
  // IMPORTANT: Use timeout loops, not delay()
  uint32_t start_ms = millis();
  while (!hardwareReady() && millis() - start_ms < 1000) {
    delay(10);  // Small delay to avoid busy loop
  }

  if (!hardwareReady()) {
    initialized_ = false;
    return false;  // Timeout
  }

  // Step 3: Configure sensor reports/data output
  if (!configureReports()) {
    initialized_ = false;
    return false;
  }

  initialized_ = true;
  last_read_ms_ = millis();
  return true;
}
```

### read() - Read Latest Data (Non-Blocking)

```cpp
bool YourSensor::read() {
  if (!initialized_) {
    return false;
  }

  new_data_ = false;

  // Non-blocking: Check available data without waiting
  if (!dataAvailable()) {
    return false;
  }

  // Parse incoming data
  uint8_t raw_data[64];
  uint16_t data_len = readRawData(raw_data, sizeof(raw_data));
  
  if (data_len == 0) {
    return false;
  }

  // Convert to OrientationData
  if (!parseOrientationData(raw_data, data_len, &orientation_)) {
    return false;
  }

  orientation_.timestamp_ms = millis();
  new_data_ = true;
  last_read_ms_ = millis();
  
  return true;
}
```

### hasNewData() - Check for New Data

```cpp
bool YourSensor::hasNewData() const {
  // Return flag and clear it
  bool result = new_data_;
  new_data_ = false;  // Note: This modifies state, so mutable flag recommended
  return result;
}
```

Better approach with mutable flag:

```cpp
// In class definition:
// mutable bool new_data_;

bool YourSensor::hasNewData() const {
  bool result = new_data_;
  new_data_ = false;
  return result;
}
```

### getOrientation() - Return Data

```cpp
const OrientationData& YourSensor::getOrientation() const {
  return orientation_;
}
```

### isHealthy() - Check Sensor Health

```cpp
bool YourSensor::isHealthy() const {
  if (!initialized_) {
    return false;
  }

  // Check for sensor timeout (5+ seconds without data)
  if (millis() - last_read_ms_ > 5000) {
    return false;
  }

  // Check calibration (example: need cal_status >= 1)
  if (orientation_.cal_status < 1) {
    return false;
  }

  return true;
}
```

### getStatusString() - Human-Readable Status

```cpp
const char* YourSensor::getStatusString() const {
  static char status[96];
  
  if (!initialized_) {
    snprintf(status, sizeof(status), "YourSensor: Not initialized");
    return status;
  }

  if (millis() - last_read_ms_ > 5000) {
    snprintf(status, sizeof(status), "YourSensor: No data (timeout)");
    return status;
  }

  // Format with calibration/status info
  snprintf(status, sizeof(status),
           "YourSensor: cal %d/3, mode %d, temp %d°C",
           orientation_.cal_status,
           getCurrentMode(),
           getCurrentTemperature());

  return status;
}
```

### Calibration Methods

```cpp
bool YourSensor::setCalibrationProfile(const uint8_t* profile_data,
                                      uint16_t length) {
  if (length != 256 || !profile_data) {
    return false;
  }

  // Send calibration data to sensor (if supported)
  if (!sendCalibrationToHardware(profile_data, length)) {
    return false;
  }

  memcpy(calibration_data_, profile_data, length);
  calibration_data_length_ = length;
  return true;
}

bool YourSensor::getCalibrationProfile(uint8_t* profile_data,
                                      uint16_t* length) {
  if (!profile_data || !length) {
    return false;
  }

  if (calibration_data_length_ == 0) {
    // Get current calibration from hardware
    if (!readCalibrationFromHardware(calibration_data_,
                                    sizeof(calibration_data_),
                                    &calibration_data_length_)) {
      return false;
    }
  }

  memcpy(profile_data, calibration_data_, calibration_data_length_);
  *length = calibration_data_length_;
  return true;
}
```

---

## Step 5: Update Sensor Base Class (if needed)

If you created a custom data type, add it to `src/sensors/sensor_base.h`:

```cpp
// In sensor_base.h, near PositionSensor class:

class YourDataSensor : public Sensor {
 public:
  virtual const YourData& getYourData() const = 0;
};
```

If you created a new base class, update the includes in `main.cpp`.

---

## Step 6: Integrate into main.cpp

Update `src/main.cpp` to instantiate and use your new sensor.

**Example: Adding MPU6050 alongside BNO085**

```cpp
#include "sensors/bno085.h"
#include "sensors/mpu6050.h"  // Your new sensor

// Sensor instances
BNO085 imu;
MPU6050 mpu;  // NEW

void setup() {
  Serial.begin(115200);

  // Initialize BNO085
  if (!imu.begin()) {
    Serial.println("ERROR: BNO085 initialization failed!");
    while (1) delay(1000);
  }
  Serial.println("BNO085 OK");

  // Initialize MPU6050 (NEW)
  if (!mpu.begin()) {
    Serial.println("ERROR: MPU6050 initialization failed!");
    // Decide: fail entirely or continue with just BNO085?
    while (1) delay(1000);
  }
  Serial.println("MPU6050 OK");
}

void loop() {
  // Read from primary sensor
  if (imu.read()) {
    if (imu.hasNewData()) {
      const OrientationData& orientation = imu.getOrientation();
      // ... use orientation ...
    }
  }

  // Read from secondary sensor (NEW)
  if (mpu.read()) {
    if (mpu.hasNewData()) {
      const OrientationData& mpu_orientation = mpu.getOrientation();
      // ... use mpu_orientation, compare, fuse, etc. ...
    }
  }

  // Output at configured frequency
  uint32_t now_ms = millis();
  if (now_ms - last_output_ms >= output_interval_ms) {
    last_output_ms = now_ms;

    // Output status from both sensors
    Serial.println(imu.getStatusString());
    Serial.println(mpu.getStatusString());  // NEW
  }
}
```

---

## Step 7: Testing & Validation

### Build Check

```bash
cd /home/devel/floppi/auto_orientation
pio run -e arduino_mega  # Adjust environment as needed
```

If build fails:
- Check header includes
- Verify virtual method signatures match base class
- Ensure no circular dependencies

### Hardware Test

```cpp
void setup() {
  Serial.begin(115200);
  
  YourSensor sensor;
  Serial.println("Initializing...");
  
  if (!sensor.begin()) {
    Serial.println("FAIL: begin()");
    while (1) delay(1000);
  }
  
  Serial.println("Sensor initialized");
  Serial.print("Name: ");
  Serial.println(sensor.name());
}

void loop() {
  // Test read loop
  if (sensor.read()) {
    if (sensor.hasNewData()) {
      const OrientationData& data = sensor.getOrientation();
      Serial.print("Q: ");
      Serial.print(data.w, 4);
      Serial.print(",");
      Serial.print(data.x, 4);
      Serial.print(",");
      Serial.print(data.y, 4);
      Serial.print(",");
      Serial.println(data.z, 4);
      
      // Verify magnitude
      float mag = sqrt(data.w*data.w + data.x*data.x + 
                      data.y*data.y + data.z*data.z);
      if (mag < 0.95 || mag > 1.05) {
        Serial.print("WARNING: Bad magnitude: ");
        Serial.println(mag, 3);
      }
    }
  }
  
  // Print status periodically
  static uint32_t last_status = 0;
  if (millis() - last_status > 2000) {
    last_status = millis();
    Serial.print("Status: ");
    Serial.println(sensor.getStatusString());
  }
  
  delay(50);
}
```

### Test Checklist

- [ ] Compiles without errors
- [ ] `begin()` succeeds with hardware connected
- [ ] `read()` returns `true` when data available
- [ ] `hasNewData()` returns `true` at least once
- [ ] Data values are in expected range
- [ ] Quaternion magnitude is 0.95-1.05 (if orientation sensor)
- [ ] `getStatusString()` updates with sensor state
- [ ] `isHealthy()` returns `true` when data valid
- [ ] `isHealthy()` returns `false` after hardware disconnect
- [ ] No blocking operations (call `read()` 100+ times/sec without hang)
- [ ] Memory usage stable (no leaks after 1000+ iterations)

---

## Example: Adding MPU6050

Complete template for adding an InvenSense MPU6050 6-axis IMU.

### File: `src/sensors/mpu6050.h`

```cpp
/**
 * MPU6050 - 6-axis IMU (accel + gyro)
 *
 * Hardware: InvenSense MPU-6050
 * Interface: I2C (400 kHz)
 * Datasheet: https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf
 *
 * Note: MPU6050 outputs raw accel/gyro data.
 * To convert to quaternion (for OrientationData), you must:
 * 1. Implement sensor fusion algorithm (Madgwick, Kalman, etc.)
 * 2. Or add magnetometer (e.g., HMC5883L) for full 9-axis
 *
 * This example implements basic gyroscope integration for yaw estimation.
 * For production, use a full 9-axis IMU like BNO085.
 */

#ifndef MPU6050_H
#define MPU6050_H

#include "sensor_base.h"

// MPU6050 I2C address
#define MPU6050_ADDR 0x68

// Register addresses
#define MPU6050_REG_GYRO_XOUT_H 0x43
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

class MPU6050 : public OrientationSensor {
 public:
  MPU6050();
  virtual ~MPU6050();

  bool begin() override;
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "MPU6050"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  const OrientationData& getOrientation() const override;
  bool setCalibrationProfile(const uint8_t* profile_data, 
                            uint16_t length) override;
  bool getCalibrationProfile(uint8_t* profile_data,
                            uint16_t* length) override;

 private:
  OrientationData orientation_;
  bool initialized_;
  mutable bool new_data_;
  uint32_t last_read_ms_;
  uint32_t last_fusion_ms_;

  float gyro_offset_x_, gyro_offset_y_, gyro_offset_z_;
  float yaw_;  // Integrated from gyro Z

  // I2C helper methods
  bool i2cWrite(uint8_t reg, uint8_t value);
  bool i2cRead(uint8_t reg, uint8_t* buffer, uint16_t len);
  bool readRawData(float* accel, float* gyro);
  bool calibrateGyroscope();
  void integrateGyroscope(float* gyro, uint32_t dt_ms);
};

#endif  // MPU6050_H
```

### File: `src/sensors/mpu6050.cpp`

```cpp
#include "mpu6050.h"
#include "../config/pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

// ============================================================================
// Constructor / Destructor
// ============================================================================

MPU6050::MPU6050()
    : initialized_(false),
      new_data_(false),
      last_read_ms_(0),
      last_fusion_ms_(0),
      gyro_offset_x_(0),
      gyro_offset_y_(0),
      gyro_offset_z_(0),
      yaw_(0) {}

MPU6050::~MPU6050() {
  end();
}

// ============================================================================
// Initialization
// ============================================================================

bool MPU6050::begin() {
  // Initialize I2C
  Wire.begin();
  Wire.setClock(400000);  // 400 kHz I2C speed

  // Verify MPU6050 is connected
  uint8_t who_am_i = 0;
  if (!i2cRead(0x75, &who_am_i, 1) || who_am_i != 0x68) {
    initialized_ = false;
    return false;
  }

  // Reset sensor
  i2cWrite(0x6B, 0x80);  // PWR_MGMT_1: Reset device
  delay(50);

  // Configure sensor
  i2cWrite(0x6B, 0x00);  // PWR_MGMT_1: Wake up, use internal oscillator
  i2cWrite(0x1A, 0x00);  // CONFIG: Gyro LPF ~260 Hz
  i2cWrite(0x1B, 0x00);  // GYRO_CONFIG: ±250 deg/s
  i2cWrite(0x1C, 0x00);  // ACCEL_CONFIG: ±2g

  // Calibrate gyroscope (accelerometer stationary)
  if (!calibrateGyroscope()) {
    initialized_ = false;
    return false;
  }

  initialized_ = true;
  last_read_ms_ = millis();
  last_fusion_ms_ = millis();

  return true;
}

void MPU6050::end() {
  Wire.end();
  initialized_ = false;
}

bool MPU6050::isInitialized() const {
  return initialized_;
}

// ============================================================================
// I2C Helpers
// ============================================================================

bool MPU6050::i2cWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool MPU6050::i2cRead(uint8_t reg, uint8_t* buffer, uint16_t len) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  uint16_t bytes_read = Wire.requestFrom((int)MPU6050_ADDR, (int)len);
  for (uint16_t i = 0; i < bytes_read; i++) {
    buffer[i] = Wire.read();
  }

  return bytes_read == len;
}

// ============================================================================
// Calibration
// ============================================================================

bool MPU6050::calibrateGyroscope() {
  Serial.println("Calibrating MPU6050... keep still");

  float sum_x = 0, sum_y = 0, sum_z = 0;
  const int samples = 200;

  for (int i = 0; i < samples; i++) {
    float accel[3], gyro[3];
    if (!readRawData(accel, gyro)) {
      return false;
    }
    sum_x += gyro[0];
    sum_y += gyro[1];
    sum_z += gyro[2];
    delay(10);
  }

  gyro_offset_x_ = sum_x / samples;
  gyro_offset_y_ = sum_y / samples;
  gyro_offset_z_ = sum_z / samples;

  Serial.println("Calibration complete");
  return true;
}

// ============================================================================
// Data Reading
// ============================================================================

bool MPU6050::readRawData(float* accel, float* gyro) {
  uint8_t buffer[14];
  if (!i2cRead(0x3B, buffer, 14)) {
    return false;
  }

  // Accel: 16-bit signed, ±2g = ±16384 LSB/g
  int16_t ax = (buffer[0] << 8) | buffer[1];
  int16_t ay = (buffer[2] << 8) | buffer[3];
  int16_t az = (buffer[4] << 8) | buffer[5];

  // Gyro: 16-bit signed, ±250 deg/s = ±131 LSB/deg/s
  int16_t gx = (buffer[8] << 8) | buffer[9];
  int16_t gy = (buffer[10] << 8) | buffer[11];
  int16_t gz = (buffer[12] << 8) | buffer[13];

  accel[0] = ax / 16384.0f;
  accel[1] = ay / 16384.0f;
  accel[2] = az / 16384.0f;

  gyro[0] = (gx / 131.0f) - gyro_offset_x_;
  gyro[1] = (gy / 131.0f) - gyro_offset_y_;
  gyro[2] = (gz / 131.0f) - gyro_offset_z_;

  return true;
}

void MPU6050::integrateGyroscope(float* gyro, uint32_t dt_ms) {
  // Simple integration: yaw += gyro_z * dt
  // Real sensor fusion (Madgwick, Kalman) would be better
  float dt_sec = dt_ms / 1000.0f;
  yaw_ += gyro[2] * dt_sec;

  // Wrap yaw to [-180, 180]
  while (yaw_ > 180) yaw_ -= 360;
  while (yaw_ < -180) yaw_ += 360;

  // Convert yaw to quaternion (simplified - rotation about Z only)
  // Full formula would use Madgwick algorithm
  float yaw_rad = yaw_ * M_PI / 180.0f;
  orientation_.w = cos(yaw_rad / 2.0f);
  orientation_.x = 0;
  orientation_.y = 0;
  orientation_.z = sin(yaw_rad / 2.0f);
}

bool MPU6050::read() {
  if (!initialized_) {
    return false;
  }

  new_data_ = false;

  float accel[3], gyro[3];
  if (!readRawData(accel, gyro)) {
    return false;
  }

  // Integrate gyroscope for orientation
  uint32_t now_ms = millis();
  uint32_t dt_ms = now_ms - last_fusion_ms_;
  if (dt_ms > 0) {
    integrateGyroscope(gyro, dt_ms);
  }
  last_fusion_ms_ = now_ms;

  orientation_.timestamp_ms = now_ms;
  new_data_ = true;
  last_read_ms_ = now_ms;

  return true;
}

bool MPU6050::hasNewData() const {
  bool result = new_data_;
  new_data_ = false;
  return result;
}

const OrientationData& MPU6050::getOrientation() const {
  return orientation_;
}

bool MPU6050::isHealthy() const {
  if (!initialized_) {
    return false;
  }

  // Check timeout
  if (millis() - last_read_ms_ > 5000) {
    return false;
  }

  return true;
}

const char* MPU6050::getStatusString() const {
  static char status[96];

  if (!initialized_) {
    snprintf(status, sizeof(status), "MPU6050: Not initialized");
    return status;
  }

  if (millis() - last_read_ms_ > 5000) {
    snprintf(status, sizeof(status), "MPU6050: No data (timeout)");
    return status;
  }

  snprintf(status, sizeof(status),
           "MPU6050: Yaw %.1f°, Mag %.3f",
           yaw_,
           sqrt(orientation_.w * orientation_.w +
                orientation_.x * orientation_.x +
                orientation_.y * orientation_.y +
                orientation_.z * orientation_.z));

  return status;
}

bool MPU6050::setCalibrationProfile(const uint8_t* profile_data,
                                   uint16_t length) {
  (void)profile_data;
  (void)length;
  // MPU6050 calibration handled in begin()
  return true;
}

bool MPU6050::getCalibrationProfile(uint8_t* profile_data,
                                   uint16_t* length) {
  (void)profile_data;
  (void)length;
  // MPU6050 has no persistent calibration storage
  return false;
}
```

### Build & Test

```bash
# Add to platformio.ini
lib_deps = 
  adafruit/Adafruit BNO08x

# Build
pio run -e arduino_mega

# Upload and test
pio run -e arduino_mega -t upload
```

---

## Common Mistakes to Avoid

### ❌ Mistake 1: Blocking Operations in `read()`

```cpp
// WRONG - blocks for 100ms
bool YourSensor::read() {
  Serial.println("Reading...");
  delay(100);  // ← BLOCKING!
  return true;
}
```

**Fix:**

```cpp
// RIGHT - non-blocking
bool YourSensor::read() {
  if (!dataReady()) {  // Check without waiting
    return false;
  }
  parseData();  // Process immediately
  return true;
}
```

### ❌ Mistake 2: Not Clearing `new_data_` Flag

```cpp
// WRONG - flag stays true forever
bool YourSensor::hasNewData() const {
  return new_data_;  // ← Never cleared!
}
```

**Fix:**

```cpp
// RIGHT - flag cleared after read
bool YourSensor::hasNewData() const {
  bool result = new_data_;
  new_data_ = false;  // ← Requires mutable flag
  return result;
}
```

### ❌ Mistake 3: Returning Invalid Quaternion

```cpp
// WRONG - quaternion magnitude not 1.0
orientation_.w = raw_w / 1024.0f;
orientation_.x = raw_x / 1024.0f;
// ← Magnitude not normalized!
```

**Fix:**

```cpp
// RIGHT - normalize quaternion
float mag = sqrt(raw_w*raw_w + raw_x*raw_x + 
                 raw_y*raw_y + raw_z*raw_z);
orientation_.w = raw_w / mag;
orientation_.x = raw_x / mag;
orientation_.y = raw_y / mag;
orientation_.z = raw_z / mag;
```

### ❌ Mistake 4: Using `delay()` for Timeouts

```cpp
// WRONG - blocks entire system
if (!connected) {
  delay(1000);  // ← Freezes everything!
  return false;
}
```

**Fix:**

```cpp
// RIGHT - non-blocking timeout
uint32_t start_ms = millis();
while (!connected && millis() - start_ms < 1000) {
  delay(10);  // Small delay to avoid busy loop
}

if (!connected) {
  return false;  // Timeout without blocking
}
```

### ❌ Mistake 5: Forgetting to Implement Base Class Methods

```cpp
// WRONG - missing override
class YourSensor : public OrientationSensor {
  bool begin() override;
  // ← Missing getOrientation()!
};
```

**Fix:**

```cpp
// RIGHT - implement all required methods
class YourSensor : public OrientationSensor {
  bool begin() override;
  bool read() override;
  bool hasNewData() const override;
  bool isInitialized() const override;
  void end() override;
  const char* name() const override;
  bool isHealthy() const override;
  const char* getStatusString() const override;
  const OrientationData& getOrientation() const override;
  bool setCalibrationProfile(const uint8_t*, uint16_t) override;
  bool getCalibrationProfile(uint8_t*, uint16_t*) override;
};
```

### ❌ Mistake 6: Global State

```cpp
// WRONG - global data (multiple sensors conflict)
float g_quaternion[4];
HardwareSerial* g_uart;

class Sensor1 : public OrientationSensor {
  // Uses global g_quaternion!
};
```

**Fix:**

```cpp
// RIGHT - each sensor owns its data
class Sensor1 : public OrientationSensor {
 private:
  OrientationData orientation_;  // Instance member
};
```

---

## Design Patterns

### Pattern 1: Hardware with Serial Interface

```cpp
class SerialSensor : public OrientationSensor {
 private:
  HardwareSerial* uart_;
  
  bool sendCommand(const char* cmd, uint32_t timeout_ms) {
    uart_->print(cmd);
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
      if (uart_->available()) {
        return true;
      }
      delay(1);
    }
    return false;  // Timeout
  }
};
```

### Pattern 2: I2C Register-Based Sensor

```cpp
class I2CSensor : public OrientationSensor {
 private:
  bool i2cWrite(uint8_t addr, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }

  bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buffer, int len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission();
    
    Wire.requestFrom(addr, len);
    for (int i = 0; i < len && Wire.available(); i++) {
      buffer[i] = Wire.read();
    }
    return Wire.available() == 0;
  }
};
```

### Pattern 3: Sensor with Background Task

```cpp
class BackgroundSensor : public OrientationSensor {
  // Use interrupt or FreeRTOS task for data acquisition
  // read() just copies latest data from buffer
  
 private:
  volatile OrientationData buffer_;
  volatile uint32_t buffer_updated_ms_;
  
  // Interrupt handler (called by hardware)
  void onDataReady() {
    // Quick copy from hardware buffer
    readHardwareBuffer(&buffer_);
    buffer_updated_ms_ = millis();
  }
};
```

---

## Extension: Output Formatters

If your sensor outputs custom data, extend the formatter system.

**Custom Data Formatter:**

```cpp
#include "output/data_formatter.h"

class YourDataFormatter : public DataFormatter {
 public:
  uint16_t format(const OrientationData& orientation,
                  const PositionData& position,
                  char* buffer,
                  uint16_t max_len) override {
    // Custom format...
    snprintf(buffer, max_len,
             "%d,%d,%d",
             (int)orientation.w,
             (int)orientation.x,
             (int)orientation.y);
    
    return strlen(buffer);
  }

  const char* getFormatName() const override {
    return "YourFormat";
  }
};
```

---

## Troubleshooting

### Sensor doesn't initialize

**Checklist:**
- [ ] Hardware power connected (3.3V or 5V as required)
- [ ] Communication lines connected (UART/I2C/SPI)
- [ ] Correct pins in `config/pins.h`
- [ ] Baud rate matches hardware (usually 115200)
- [ ] No other device using same serial port
- [ ] Sensor not in bootloader mode

### Data is garbage

- Check data format matches hardware specification
- Verify byte order (big-endian vs little-endian)
- Check scaling factors (raw units to SI units)
- Verify checksum/CRC if used
- Test with manufacturer's example code first

### Sensor times out

- Check serial buffer size (may be too small)
- Verify communication speed matches hardware
- Check for missing CR/LF or terminators
- Add debug output to see what's arriving
- Test with USB serial monitor (TeraTerm, etc.)

### Quaternion magnitude invalid

- Verify normalization after calculation
- Check for integer overflow (use float, not int)
- Ensure scaling factors correct
- Test with static orientation (known quaternion)

---

## See Also

- [API_REFERENCE.md](../reference/API_REFERENCE.md) - Complete API documentation
- [config/pins.h](../../src/config/pins.h) - Pin configuration
- [src/sensors/bno085.h](../../src/sensors/bno085.h) - Reference implementation
- [src/sensors/neo_m9n.h](../../src/sensors/neo_m9n.h) - GPS reference implementation

---

## Questions?

If you have issues adding a new sensor, check:

1. Compilation errors → verify virtual method signatures
2. Runtime errors → check `begin()` success
3. No data → check `read()` and `hasNewData()` logic
4. Bad data → check scaling and data format
5. Timeout errors → check serial port and baud rate

Test incrementally:
- ✓ Compiles
- ✓ Initializes  
- ✓ Returns data
- ✓ Data is reasonable
- ✓ Status string is helpful
- ✓ Integrates with main system
