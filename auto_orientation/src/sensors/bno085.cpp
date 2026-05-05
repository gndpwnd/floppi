/**
 * BNO085 Sensor Driver Implementation
 *
 * Wraps Adafruit BNO08x library to provide:
 * - UART initialization on configurable pins (from pins.h)
 * - Absolute orientation (quaternion) reading
 * - Calibration status monitoring
 * - Sensor health checking
 *
 * Key Design Notes:
 * - Uses ABSOLUTE ORIENTATION (Report ID 5), NOT RVC (Report ID 14)
 * - Reads quaternion data (w, x, y, z) with magnitude ≈ 1.0
 * - Tracks per-axis calibration status (accel, gyro, mag)
 * - Status string includes calibration levels for debugging
 *
 * Adafruit APIs Used:
 * - Adafruit_BNO08x::begin_UART() - Initialize with Stream object
 * - Adafruit_BNO08x::enableReport() - Enable absolute orientation report
 * - Adafruit_BNO08x::getEvent() - Read sensor data
 * - Adafruit_BNO08x::getCalibration() - Query calibration status
 * - Adafruit_BNO08x::wasReset() - Check for sensor reset
 *
 * TODO (Research Phase):
 * - Implement calibration persistence via:
 *   - Adafruit_BNO08x::getSensorOffsetProfiler() [if available]
 *   - Manual EEPROM storage of calibration state
 *   - SD card backup of calibration
 */

#include "bno085.h"
#include "../config/pins.h"
#include <Arduino.h>

// Include the Adafruit library
#include "Adafruit_BNO08x_Arduino.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

BNO085::BNO085()
    : imu_(nullptr),
      initialized_(false),
      new_data_(false),
      last_read_ms_(0),
      calibration_data_length_(0) {
  // Initialize orientation_ with default constructor
  // (already zeroed via OrientationData default constructor)
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

  // Determine which UART to use based on board type
  Stream *uart_stream = nullptr;

#if defined(__AVR_ATmega328P__)
  // Arduino Nano/Uno: Use SoftwareSerial
  #include <SoftwareSerial.h>
  static SoftwareSerial soft_serial(BNO085_RX_PIN, BNO085_TX_PIN);
  uart_stream = &soft_serial;

#elif defined(__AVR_ATmega2560__)
  // Arduino Mega: Use Serial1 (hardware UART)
  Serial1.begin(115200);
  uart_stream = &Serial1;

#elif defined(TEENSY31) || defined(TEENSY32)
  // Teensy 3.x: Use Serial1
  Serial1.begin(115200);
  uart_stream = &Serial1;

#elif defined(ESP32)
  // ESP32: Use Serial1 with custom pins
  Serial1.begin(115200, SERIAL_8N1, BNO085_RX_PIN, BNO085_TX_PIN);
  uart_stream = &Serial1;

#else
  // Default to Serial1 if available, fallback to Serial
  #ifdef Serial1
  Serial1.begin(115200);
  uart_stream = &Serial1;
  #else
  uart_stream = &Serial;
  #endif
#endif

  // Initialize BNO08x via UART
  // The reset_pin is optional; -1 means no hardware reset available
  if (!imu_->begin_UART(uart_stream, -1)) {
    delete imu_;
    imu_ = nullptr;
    initialized_ = false;
    return false;
  }

  // Enable the absolute orientation (quaternion) report
  // Report ID 5 = Rotation Vector (Absolute Orientation)
  // Period: 100ms (10 Hz, or 100000 microseconds)
  // This matches SERIAL_OUTPUT_FREQUENCY_HZ (10 Hz)
  if (!imu_->enableReport(QUAT_REPORT_ID, 100000)) {
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

  // Check for sensor reset (can happen during operation)
  if (imu_->wasReset()) {
    // Re-enable reports after reset
    imu_->enableReport(QUAT_REPORT_ID, 100000);
  }

  // Try to read sensor event
  sh2_SensorEvent event;
  if (!imu_->getEvent(&event)) {
    // No new data available
    return false;
  }

  // Verify it's a quaternion report
  if (event.eventID != QUAT_REPORT_ID) {
    // Not the orientation data we're looking for
    return false;
  }

  // Extract quaternion from event
  orientation_.w = event.un.quat.w;
  orientation_.x = event.un.quat.x;
  orientation_.y = event.un.quat.y;
  orientation_.z = event.un.quat.z;
  orientation_.timestamp_ms = millis();

  // Get calibration status (individual axis calibration)
  uint8_t sys_cal, gyro_cal, accel_cal, mag_cal;
  imu_->getCalibration(&sys_cal, &gyro_cal, &accel_cal, &mag_cal);

  orientation_.cal_status = sys_cal;      // Overall system calibration (0-3)
  orientation_.cal_accel = accel_cal;     // Accelerometer (0-3)
  orientation_.cal_gyro = gyro_cal;       // Gyroscope (0-3)
  orientation_.cal_mag = mag_cal;         // Magnetometer (0-3)

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
  // 3. Calibration is at least at MEDIUM level or better
  uint32_t now_ms = millis();
  bool recent_data = (now_ms - last_read_ms_) < 2000;
  bool calibrated = orientation_.cal_status >= SENSOR_CALIBRATION_MEDIUM;

  return recent_data && calibrated;
}

const char* BNO085::getStatusString() const {
  static char status_buffer[96];

  if (!initialized_) {
    snprintf(status_buffer, sizeof(status_buffer), "BNO085: NOT INITIALIZED");
    return status_buffer;
  }

  const char* cal_labels[] = {"Uncalibrated", "Low", "Medium", "High"};

  // Clamp calibration values to valid range (0-3)
  uint8_t sys_cal = (orientation_.cal_status <= 3) ? orientation_.cal_status : 3;
  uint8_t gyro_cal = (orientation_.cal_gyro <= 3) ? orientation_.cal_gyro : 3;
  uint8_t accel_cal = (orientation_.cal_accel <= 3) ? orientation_.cal_accel : 3;
  uint8_t mag_cal = (orientation_.cal_mag <= 3) ? orientation_.cal_mag : 3;

  snprintf(
      status_buffer,
      sizeof(status_buffer),
      "BNO085: Q(%.3f,%.3f,%.3f,%.3f) Sys:%s Gyro:%s Accel:%s Mag:%s",
      orientation_.w, orientation_.x, orientation_.y, orientation_.z,
      cal_labels[sys_cal],
      cal_labels[gyro_cal],
      cal_labels[accel_cal],
      cal_labels[mag_cal]);

  return status_buffer;
}

// ============================================================================
// Calibration (Stub for Research Phase)
// ============================================================================

bool BNO085::setCalibrationProfile(const uint8_t* profile_data,
                                    uint16_t length) {
  // TODO: Implement calibration persistence
  // This will use Adafruit_BNO08x::setSensorOffsetProfiler() if available,
  // or manual EEPROM/SD card storage.
  // For now, just store the data in memory.

  if (length > sizeof(calibration_data_)) {
    return false;
  }

  memcpy(calibration_data_, profile_data, length);
  calibration_data_length_ = length;
  return true;
}

bool BNO085::getCalibrationProfile(uint8_t* profile_data,
                                    uint16_t* length) {
  // TODO: Implement calibration retrieval
  // Research phase will determine if Adafruit_BNO08x exposes
  // calibration save/restore, or if we need manual implementation.

  if (!profile_data || !length) {
    return false;
  }

  if (*length < calibration_data_length_) {
    return false;  // Buffer too small
  }

  memcpy(profile_data, calibration_data_, calibration_data_length_);
  *length = calibration_data_length_;
  return true;
}
