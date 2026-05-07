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
      last_saved_cal_level_(0) {
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
  Serial.println("  DEBUG: Allocating BNO085 object...");
  imu_ = new Adafruit_BNO08x();
  if (!imu_) {
    Serial.println("  ERROR: Failed to allocate BNO085 object!");
    initialized_ = false;
    return false;
  }
  Serial.println("  DEBUG: Object allocated successfully");

  // Initialize I2C communication (all boards use Wire library)
  Serial.println("  DEBUG: Starting Wire (I2C)...");
  Wire.begin();
  Wire.setClock(100000L);  // 100 kHz I2C clock (BNO085 has timing issues at 400 kHz)
  Serial.println("  DEBUG: Wire initialized, clock set to 100 kHz");

  // Initialize BNO08x via I2C
  // Try both addresses: 0x4A (DI pin to GND) and 0x4B (DI pin to VCC/floating)
  // This handles cases where DI pin is not connected or misconfigured
  bool init_success = false;

  Serial.println("  DEBUG: Trying BNO085 at address 0x4A...");
  if (imu_->begin_I2C(0x4A, &Wire, 0)) {
    Serial.println("  DEBUG: SUCCESS at 0x4A!");
    init_success = true;
  } else {
    Serial.println("  DEBUG: Failed at 0x4A, trying 0x4B...");
    if (imu_->begin_I2C(0x4B, &Wire, 0)) {
      Serial.println("  DEBUG: SUCCESS at 0x4B!");
      init_success = true;
    } else {
      Serial.println("  ERROR: Failed at both 0x4A and 0x4B");
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
    if (restoreFromEEPROM(saved_cal, &saved_cal_length)) {
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

  // DEBUG: Log raw quaternion values
  if (orientation_.cal_status == 0) {
    Serial.print("  DEBUG: Raw quat when uncalibrated: w=");
    Serial.print(orientation_.w, 6);
    Serial.print(" x=");
    Serial.print(orientation_.x, 6);
    Serial.print(" y=");
    Serial.print(orientation_.y, 6);
    Serial.print(" z=");
    Serial.println(orientation_.z, 6);
  }

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
