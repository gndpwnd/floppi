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
#include "../config/calibration_storage.h"
#include "bno085_calibration.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>

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
      calibration_data_length_(0),
      last_saved_cal_level_(0),
      cache_valid_(false) {
  memset(calibration_data_, 0, sizeof(calibration_data_));
  // Initialize last_quaternion to identity
  last_quaternion_.w = 1.0f;
  last_quaternion_.x = 0.0f;
  last_quaternion_.y = 0.0f;
  last_quaternion_.z = 0.0f;
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
    Serial.println("  ERROR: Failed to allocate BNO085 object!");
    initialized_ = false;
    return false;
  }

  // Initialize I2C communication (all boards use Wire library)
  Wire.begin();
  delay(100);
  Wire.setClock(100000L);  // 100 kHz I2C clock (BNO085 has timing issues at 400 kHz)
  delay(100);

  // Give BNO085 time to be ready after I2C initialization
  delay(500);  // Wait 500ms for sensor to stabilize

  // Initialize BNO08x via I2C
  // Try both addresses: 0x4A (DI pin to GND) and 0x4B (DI pin to VCC/floating)
  // This handles cases where DI pin is not connected or misconfigured
  bool init_success = false;

  if (imu_->begin_I2C(0x4A, &Wire, 0)) {
    init_success = true;
  } else {
    if (imu_->begin_I2C(0x4B, &Wire, 0)) {
      init_success = true;
    }
  }

  if (!init_success) {
    delete imu_;
    imu_ = nullptr;
    initialized_ = false;
    return false;
  }

  // Try to load previously saved calibration from EEPROM
  if (hasCalibrationInEEPROM()) {
    uint8_t saved_cal[256];
    uint16_t saved_cal_length = 0;
    if (restoreFromEEPROM(saved_cal, sizeof(saved_cal), &saved_cal_length)) {
      Serial.println("  Found saved calibration in EEPROM, restoring...");
      if (writeCalibrationProfile(saved_cal, saved_cal_length)) {
        Serial.println("  ✓ Calibration restored successfully!");
        last_saved_cal_level_ = 3;  // Mark as already saved (full calibration)
      } else {
        Serial.print("  ⚠ Warning: Could not apply saved calibration - ");
        Serial.println(getCalibrationError());
      }
    }
  } else {
    Serial.println("  ℹ No saved calibration - you will need to calibrate with figure-8 motion");
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
// Helper Functions
// ============================================================================

static void calculateEulerAngles(float w, float x, float y, float z,
                                 float& roll_deg, float& pitch_deg, float& yaw_deg) {
  // Convert quaternion to Euler angles (roll, pitch, yaw in degrees)
  const float PI_LOCAL = 3.14159265359f;

  // Roll (X-axis rotation)
  float sinr_cosp = 2.0f * (w * x + y * z);
  float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
  roll_deg = atan2(sinr_cosp, cosr_cosp) * 180.0f / PI_LOCAL;

  // Pitch (Y-axis rotation)
  float sinp = 2.0f * (w * y - z * x);
  if (sinp > 1.0f) sinp = 1.0f;
  if (sinp < -1.0f) sinp = -1.0f;
  pitch_deg = asin(sinp) * 180.0f / PI_LOCAL;

  // Yaw (Z-axis rotation)
  float siny_cosp = 2.0f * (w * z + x * y);
  float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
  yaw_deg = atan2(siny_cosp, cosy_cosp) * 180.0f / PI_LOCAL;
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

  // Calculate Euler angles from quaternion (roll, pitch, yaw in degrees)
  calculateEulerAngles(orientation_.w, orientation_.x, orientation_.y, orientation_.z,
                       orientation_.roll_deg, orientation_.pitch_deg, orientation_.yaw_deg);


  // Get calibration status from sensor_value.status
  // status values: 0=unreliable, 1=low, 2=medium, 3=high
  orientation_.cal_status = sensor_value.status;
  orientation_.cal_accel = sensor_value.status;
  orientation_.cal_gyro = sensor_value.status;
  orientation_.cal_mag = sensor_value.status;

  // Auto-save calibration when it reaches level 2+ (medium or higher)
  // Done asynchronously to avoid blocking the main read loop
  static uint32_t last_save_attempt_ms = 0;
  if (sensor_value.status >= 2 && sensor_value.status > last_saved_cal_level_) {
    uint32_t now_ms = millis();
    // Throttle save attempts to every 5 seconds (avoid spam)
    if (now_ms - last_save_attempt_ms > 5000) {
      last_save_attempt_ms = now_ms;

      // NOTE: This blocks for ~1-2 seconds (sensor read + EEPROM write)
      // In production, this should be moved to a background task
      Serial.println("\n  [Saving calibration to EEPROM - please wait...]");

      uint8_t cal_data[256];
      uint16_t cal_length = 0;

      // Try to read current calibration from sensor
      if (readCalibrationProfile(cal_data, &cal_length)) {
        // Save to EEPROM
        if (saveToEEPROM(cal_data, cal_length)) {
          Serial.print("  ✓✓✓ Calibration level ");
          Serial.print(sensor_value.status);
          Serial.println(" saved to EEPROM!");
          Serial.println("  ✓✓✓ Will persist on next power cycle!\n");
          last_saved_cal_level_ = sensor_value.status;
        } else {
          Serial.println("  ✗ Failed to save to EEPROM\n");
        }
      } else {
        Serial.print("  ✗ Could not read calibration: ");
        Serial.println(getCalibrationError());
      }
    }
  }

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

  // Use dtostrf for floats (Arduino snprintf doesn't support %f)
  char w_buf[12], x_buf[12], y_buf[12], z_buf[12];
  char roll_buf[12], pitch_buf[12], yaw_buf[12];
  dtostrf(orientation_.w, 6, 3, w_buf);
  dtostrf(orientation_.x, 6, 3, x_buf);
  dtostrf(orientation_.y, 6, 3, y_buf);
  dtostrf(orientation_.z, 6, 3, z_buf);
  dtostrf(orientation_.roll_deg, 6, 1, roll_buf);
  dtostrf(orientation_.pitch_deg, 6, 1, pitch_buf);
  dtostrf(orientation_.yaw_deg, 6, 1, yaw_buf);

  snprintf(
      status_buffer,
      sizeof(status_buffer),
      "BNO085 OK | Q: %s,%s,%s,%s | Roll: %s° Pitch: %s° Yaw: %s° | Cal: %s",
      w_buf, x_buf, y_buf, z_buf,
      roll_buf, pitch_buf, yaw_buf,
      cal_labels[sys_cal]);

  return status_buffer;
}

// ============================================================================
// Rotation Matrix & Euler Angles (Extensions for Phase 1)
// ============================================================================

const RotationMatrix& BNO085::getRotationMatrix() {
  // Check if cache is valid (quaternion hasn't changed)
  Quaternion current_q(orientation_.w, orientation_.x, orientation_.y, orientation_.z);

  if (!cache_valid_ ||
      current_q.w != last_quaternion_.w ||
      current_q.x != last_quaternion_.x ||
      current_q.y != last_quaternion_.y ||
      current_q.z != last_quaternion_.z) {
    // Quaternion changed or cache never set - recompute
    last_quaternion_ = current_q;
    rotation_matrix_ = quaternion_to_rotation_matrix(current_q);
    cache_valid_ = true;
  }

  return rotation_matrix_;
}

const EulerAngles& BNO085::getEulerAngles() {
  // Check if cache is valid (quaternion hasn't changed)
  Quaternion current_q(orientation_.w, orientation_.x, orientation_.y, orientation_.z);

  if (!cache_valid_ ||
      current_q.w != last_quaternion_.w ||
      current_q.x != last_quaternion_.x ||
      current_q.y != last_quaternion_.y ||
      current_q.z != last_quaternion_.z) {
    // Quaternion changed or cache never set - recompute
    last_quaternion_ = current_q;
    euler_angles_ = quaternion_to_euler_degrees(current_q);
    cache_valid_ = true;
  }

  return euler_angles_;
}

bool BNO085::validateQuaternionMagnitude(float tolerance) {
  Quaternion q(orientation_.w, orientation_.x, orientation_.y, orientation_.z);
  float magnitude = q.magnitude();
  float deviation = magnitude - 1.0f;

  // Check if magnitude deviates from 1.0 beyond tolerance
  if (fabs(deviation) > tolerance) {
    Serial.print("  ⚠ WARNING: Quaternion magnitude out of range: ");
    Serial.print(magnitude);
    Serial.print(" (expected ~1.0, deviation: ");
    Serial.print(fabs(deviation) * 100.0f);
    Serial.println("%)");
    return false;
  }

  return true;
}

// ============================================================================
// Calibration (Stub for Research Phase)
// ============================================================================

bool BNO085::setCalibrationProfile(const uint8_t* profile_data,
                                    uint16_t length) {
  if (!profile_data) {
    return false;
  }

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
