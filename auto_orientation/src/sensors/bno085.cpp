/**
 * BNO085 Sensor Driver Implementation
 *
 * Wraps Adafruit BNO08x library to provide:
 * - I2C initialization (Arduino Mega: pins 20/SDA, 21/SCL)
 * - Absolute orientation (quaternion) reading
 * - Calibration status monitoring
 * - Sensor health checking
 *
 * Key Design Notes:
 * - Uses ROTATION VECTOR (SH2_ROTATION_VECTOR = 0x05)
 * - Reads quaternion data (w, x, y, z) with magnitude ≈ 1.0
 * - Tracks per-axis calibration status (accel, gyro, mag)
 * - Status string includes calibration levels for debugging
 * - I2C Address: 0x4A (default, DI pin to GND)
 *
 * Adafruit APIs Used:
 * - Adafruit_BNO08x::begin_I2C() - Initialize with Wire I2C
 * - Adafruit_BNO08x::enableReport() - Enable rotation vector report
 * - Adafruit_BNO08x::getSensorEvent() - Read sensor data
 * - Adafruit_BNO08x::wasReset() - Check for sensor reset
 */

#include "bno085.h"
#include "../config/pins.h"
#include <Arduino.h>
#include <string.h>

// Include the Adafruit library
#include "Adafruit_BNO08x.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

BNO085::BNO085()
    : imu_(nullptr),
      initialized_(false),
      new_data_(false),
      last_read_ms_(0),
      calibration_data_length_(0) {
  memset(calibration_data_, 0, sizeof(calibration_data_));
}

BNO085::~BNO085() {
  end();
}

// ============================================================================
// Initialization
// ============================================================================

bool BNO085::begin() {
  // Allocate IMU object
  imu_ = new Adafruit_BNO08x();
  if (!imu_) {
    initialized_ = false;
    return false;
  }

  // Initialize I2C communication (all boards use Wire library)
  Wire.begin();
  Wire.setClock(100000L);  // 100 kHz I2C clock (BNO085 has timing issues at 400 kHz)

  // Initialize BNO08x via I2C (address 0x4A - DI pin to GND)
  if (!imu_->begin_I2C(0x4A, &Wire, 0)) {
    delete imu_;
    imu_ = nullptr;
    initialized_ = false;
    return false;
  }

  // Enable the rotation vector (absolute orientation) report
  // SH2_ROTATION_VECTOR = 0x05
  // Period: 100ms (10 Hz, or 100000 microseconds)
  if (!imu_->enableReport(SH2_ROTATION_VECTOR, 100000)) {
    delete imu_;
    imu_ = nullptr;
    initialized_ = false;
    return false;
  }

  initialized_ = true;
  last_read_ms_ = millis();

  return true;
}

void BNO085::end() {
  if (imu_) {
    delete imu_;
    imu_ = nullptr;
  }
  initialized_ = false;
}

bool BNO085::isInitialized() const {
  return initialized_;
}

// ============================================================================
// Data Reading
// ============================================================================

bool BNO085::read() {
  if (!initialized_ || !imu_) {
    return false;
  }

  new_data_ = false;

  // Check for sensor reset
  if (imu_->wasReset()) {
    // Re-enable reports after reset
    imu_->enableReport(SH2_ROTATION_VECTOR, 100000);
  }

  // Try to read sensor value
  sh2_SensorValue_t sensor_value;
  if (!imu_->getSensorEvent(&sensor_value)) {
    // No new data available
    return false;
  }

  // Verify it's a rotation vector report
  if (sensor_value.sensorId != SH2_ROTATION_VECTOR) {
    // Not the orientation data we're looking for
    return false;
  }

  // Extract quaternion from rotation vector
  // The sh2_RotationVector_t uses: real (scalar), i, j, k (vector components)
  // Standard quaternion convention: w (scalar), x, y, z (vector)
  orientation_.w = sensor_value.un.rotationVector.real;
  orientation_.x = sensor_value.un.rotationVector.i;
  orientation_.y = sensor_value.un.rotationVector.j;
  orientation_.z = sensor_value.un.rotationVector.k;
  orientation_.timestamp_ms = millis();

  // Get calibration status from sensor_value.status
  // status values: 0=unreliable, 1=low, 2=medium, 3=high
  orientation_.cal_status = sensor_value.status;

  // For now, store the same calibration value for all axes
  // (the Adafruit library doesn't provide per-axis calibration in this mode)
  orientation_.cal_accel = sensor_value.status;
  orientation_.cal_gyro = sensor_value.status;
  orientation_.cal_mag = sensor_value.status;

  new_data_ = true;
  last_read_ms_ = millis();

  return true;
}

bool BNO085::hasNewData() const {
  return new_data_;
}

// ============================================================================
// Data Access
// ============================================================================

const OrientationData& BNO085::getOrientation() const {
  return orientation_;
}

// ============================================================================
// Health & Status
// ============================================================================

bool BNO085::isHealthy() const {
  if (!initialized_ || !imu_) {
    return false;
  }

  // Consider sensor healthy if:
  // 1. It's initialized
  // 2. We've read data recently (within 2 seconds)
  // 3. Calibration is at least at MEDIUM level (2) or better
  uint32_t now_ms = millis();
  bool recent_data = (now_ms - last_read_ms_) < 2000;
  bool calibrated = orientation_.cal_status >= 2;  // 2 = MEDIUM

  return recent_data && calibrated;
}

const char* BNO085::getStatusString() const {
  static char status_buffer[96];

  if (!initialized_) {
    snprintf(status_buffer, sizeof(status_buffer), "BNO085: NOT INITIALIZED");
    return status_buffer;
  }

  const char* cal_labels[] = {"Unreliable", "Low", "Medium", "High"};

  // Clamp calibration values to valid range (0-3)
  uint8_t sys_cal = (orientation_.cal_status <= 3) ? orientation_.cal_status : 3;

  snprintf(
      status_buffer,
      sizeof(status_buffer),
      "BNO085 OK | Q: %.3f,%.3f,%.3f,%.3f | Cal: %s",
      orientation_.w, orientation_.x, orientation_.y, orientation_.z,
      cal_labels[sys_cal]);

  return status_buffer;
}

// ============================================================================
// Calibration (Stub for Research Phase)
// ============================================================================

bool BNO085::setCalibrationProfile(const uint8_t* profile_data,
                                    uint16_t length) {
  if (length > sizeof(calibration_data_)) {
    return false;
  }

  memcpy(calibration_data_, profile_data, length);
  calibration_data_length_ = length;
  return true;
}

bool BNO085::getCalibrationProfile(uint8_t* profile_data,
                                    uint16_t* length) {
  if (!profile_data || !length) {
    return false;
  }

  if (*length < calibration_data_length_) {
    return false;
  }

  memcpy(profile_data, calibration_data_, calibration_data_length_);
  *length = calibration_data_length_;
  return true;
}
