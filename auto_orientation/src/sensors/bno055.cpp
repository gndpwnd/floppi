/**
 * BNO055 Sensor Driver Implementation
 *
 * Wraps Adafruit_BNO055 to provide:
 * - I2C initialization (default: Wire on Mega pins 20/SDA, 21/SCL)
 * - Absolute orientation quaternion via getQuat()
 * - Euler angles derived from the quaternion (avoids the BNO055
 *   VECTOR_EULER 90°-pitch discontinuity — see Adafruit datasheet errata)
 * - Per-axis calibration accuracies (sys / accel / gyro / mag)
 * - 22-byte calibration offset/radius profile via getSensorOffsets()
 *
 * Key Design Notes:
 * - Operating mode: OPERATION_MODE_NDOF (9-DoF fusion w/ magnetometer).
 * - Default I2C address 0x28 (ADR=GND on Adafruit breakout).
 * - We deliberately do NOT remap quaternion components — body frame is the
 *   chip's native frame (+X forward, +Y left, +Z up). Unifying with BNO085
 *   is tracked as Phase 4.6.5.
 *
 * Adafruit APIs Used:
 * - Adafruit_BNO055(sensor_id, address, wire) - construction
 * - bool begin(opmode_t)                       - initialize + set mode
 * - void setExtCrystalUse(bool)                - external 32 kHz crystal
 * - imu::Quaternion getQuat()                  - fusion quaternion
 * - void getCalibration(&sys,&gyro,&accel,&mag)- four 0-3 accuracies
 * - bool isFullyCalibrated()                   - all accuracies == 3
 * - bool getSensorOffsets(uint8_t*)            - 22-byte profile read
 * - void setSensorOffsets(const uint8_t*)      - 22-byte profile write
 */

#include "bno055.h"

// USE_BNO055 must be defined by the build env to compile this driver
// (otherwise the Adafruit_BNO055 library isn't pulled in via lib_deps).
// Without USE_BNO055, this TU is empty — declarations from bno055.h are
// unaffected (it uses a forward declaration), so headers stay safely
// includable; only actual instantiation would link-fail (and no env that
// lacks USE_BNO055 instantiates BNO055).
#ifdef USE_BNO055

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// The Adafruit headers stay in the .cpp so callers including bno055.h don't
// transitively drag in Adafruit_Sensor.h / utility/imumaths.h.
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// File-scope Adafruit driver instance — replaces the heap allocation
// (`new Adafruit_BNO055(...)`) that previously dragged malloc/free into the
// Uno binary (~586 B). One BNO055 chip per application is the only supported
// topology; if a future dual-IMU use case needs distinct addresses, swap in
// placement-new over a member byte buffer.
static Adafruit_BNO055 g_adafruit_bno055(55, 0x28, &Wire);

// ============================================================================
// Constructor / Destructor
// ============================================================================

BNO055::BNO055(uint8_t i2c_address, bool use_ext_crystal)
    : bno_(nullptr),
      i2c_address_(i2c_address),
      use_ext_crystal_(use_ext_crystal),
      initialized_(false),
      new_data_(false),
      last_read_ms_(0),
      cal_blob_valid_(false) {
  memset(cal_blob_, 0, sizeof(cal_blob_));
}

BNO055::~BNO055() {
  end();
}

// ============================================================================
// Initialization
// ============================================================================

bool BNO055::begin() {
  // Point at the file-scope Adafruit driver (constructed at program startup,
  // no heap involved). i2c_address_ stays as a member for API parity, but only
  // 0x28 is wired in this static-init path; see file-scope comment above.
  bno_ = &g_adafruit_bno055;

  // Initialize I2C. BNO055 supports 400 kHz, but we follow the reference
  // sketch and let Wire run at its framework default (100 kHz). Faster clocks
  // can be enabled in a follow-up build flag if needed.
  Wire.begin();
  // I²C left at default 100 kHz. Tried 400 kHz Fast-mode (KI-8, would save
  // ~3 ms per read) but Adafruit BNO055 breakout boards typically ship with
  // 4.7 kΩ pull-ups which are too weak for reliable Fast-mode operation.
  // To re-enable, add stronger pull-ups (1-2 kΩ) on SDA/SCL and uncomment:
  //   Wire.setClock(400000UL);
  delay(100);

  // begin() blocks ~650 ms for the chip's internal boot sequence.
  if (!bno_->begin(OPERATION_MODE_NDOF)) {
    bno_ = nullptr;
    initialized_ = false;
    return false;
  }

  // Adafruit's library doc explicitly requires a short delay before issuing
  // additional commands after begin().
  delay(50);

  // External-crystal mode must be enabled AFTER begin() — see datasheet
  // 4.3.55. Adafruit breakouts ship with a 32 kHz crystal populated.
  if (use_ext_crystal_) {
    bno_->setExtCrystalUse(true);
    delay(10);
  }

  initialized_ = true;
  new_data_ = false;
  last_read_ms_ = millis();
  cal_blob_valid_ = false;
  return true;
}

void BNO055::end() {
  // No heap allocation to free — bno_ points at the file-scope instance.
  bno_ = nullptr;
  initialized_ = false;
  new_data_ = false;
}

bool BNO055::isInitialized() const {
  return initialized_;
}

// ============================================================================
// Data Reading
// ============================================================================

bool BNO055::read() {
  if (!initialized_ || !bno_) {
    new_data_ = false;
    return false;
  }

  new_data_ = false;

  // Pull the fusion quaternion from the chip. imu::Quaternion's accessors are
  // w()/x()/y()/z() (double-typed in Adafruit's imumaths; cast to float for
  // OrientationData).
  imu::Quaternion q = bno_->getQuat();
  data_.w = static_cast<float>(q.w());
  data_.x = static_cast<float>(q.x());
  data_.y = static_cast<float>(q.y());
  data_.z = static_cast<float>(q.z());

  // Convert to Euler via the project's quaternion math (avoids the BNO055
  // VECTOR_EULER 90° pitch discontinuity bug).
  Quaternion proj_q(data_.w, data_.x, data_.y, data_.z);
  EulerAngles euler_deg = quaternion_to_euler_degrees(proj_q);
  data_.roll_deg  = euler_deg.roll;
  data_.pitch_deg = euler_deg.pitch;
  data_.yaw_deg   = euler_deg.yaw;

  // Per-axis calibration accuracies (0-3). Note the Adafruit argument order
  // is (sys, gyro, accel, mag) — DIFFERENT from how we store them.
  uint8_t sys_cal = 0, gyro_cal = 0, accel_cal = 0, mag_cal = 0;
  bno_->getCalibration(&sys_cal, &gyro_cal, &accel_cal, &mag_cal);
  data_.cal_status = sys_cal;
  data_.cal_accel  = accel_cal;
  data_.cal_gyro   = gyro_cal;
  data_.cal_mag    = mag_cal;

  data_.timestamp_ms = millis();
  last_read_ms_      = data_.timestamp_ms;
  new_data_          = true;

  return true;
}

bool BNO055::hasNewData() const {
  return new_data_;
}

bool BNO055::getRawAccel(float xyz[3]) {
  if (!initialized_ || !bno_) {
    return false;
  }
  imu::Vector<3> v = bno_->getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
  xyz[0] = static_cast<float>(v.x());
  xyz[1] = static_cast<float>(v.y());
  xyz[2] = static_cast<float>(v.z());
  return true;
}

bool BNO055::getRawGyro(float xyz[3]) {
  if (!initialized_ || !bno_) {
    return false;
  }
  imu::Vector<3> v = bno_->getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  xyz[0] = static_cast<float>(v.x());
  xyz[1] = static_cast<float>(v.y());
  xyz[2] = static_cast<float>(v.z());
  return true;
}

// ============================================================================
// Data Access
// ============================================================================

const OrientationData& BNO055::getOrientation() const {
  return data_;
}

// ============================================================================
// Health & Status
// ============================================================================

bool BNO055::isHealthy() const {
  if (!initialized_ || !bno_) {
    return false;
  }

  // Match the BNO085 driver's heuristic:
  //   1. Initialized.
  //   2. Read within the last 2 s.
  //   3. System calibration at least MEDIUM (2).
  uint32_t now_ms = millis();
  bool recent_data = (now_ms - last_read_ms_) < 2000;
  bool calibrated  = data_.cal_status >= 2;
  return recent_data && calibrated;
}

const char* BNO055::getStatusString() const {
  // Avoid snprintf/vfprintf — that pulls ~1.3 KB of libc onto AVR. Encode the
  // four cal accuracies into a fixed 6-byte string "Saagm\0" where each digit
  // is 0..3 (sys/accel/gyro/mag). Callers wanting human-readable output should
  // print labels themselves: e.g. Serial.print(F("s=")); Serial.print(buf[1]).
  static char status_buffer[6];
  if (!initialized_) {
    status_buffer[0] = 'X';
    status_buffer[1] = 0;
    return status_buffer;
  }
  status_buffer[0] = 'S';
  status_buffer[1] = static_cast<char>('0' + (data_.cal_status & 0x03));
  status_buffer[2] = static_cast<char>('0' + (data_.cal_accel  & 0x03));
  status_buffer[3] = static_cast<char>('0' + (data_.cal_gyro   & 0x03));
  status_buffer[4] = static_cast<char>('0' + (data_.cal_mag    & 0x03));
  status_buffer[5] = 0;
  return status_buffer;
}

// ============================================================================
// BNO055-specific Helpers
// ============================================================================

bool BNO055::isFullyCalibrated() const {
  return data_.cal_status == 3 &&
         data_.cal_accel  == 3 &&
         data_.cal_gyro   == 3 &&
         data_.cal_mag    == 3;
}

void BNO055::useExternalCrystal(bool enable) {
  use_ext_crystal_ = enable;
  if (initialized_ && bno_) {
    bno_->setExtCrystalUse(enable);
  }
}

// ============================================================================
// Calibration Profile
// ============================================================================

bool BNO055::getCalibrationProfile(uint8_t* profile_data, uint16_t* length) {
  if (!profile_data || !length) {
    return false;
  }
  if (*length < 22) {
    *length = 22;  // Hint the required size to the caller.
    return false;
  }
  if (!initialized_ || !bno_) {
    return false;
  }

  // Adafruit_BNO055::getSensorOffsets(uint8_t*) returns false unless the
  // sensor reports fully calibrated. We forward that contract upstream.
  uint8_t buf[22];
  if (!bno_->getSensorOffsets(buf)) {
    return false;
  }

  memcpy(cal_blob_, buf, sizeof(cal_blob_));
  cal_blob_valid_ = true;

  memcpy(profile_data, cal_blob_, sizeof(cal_blob_));
  *length = sizeof(cal_blob_);
  return true;
}

bool BNO055::setCalibrationProfile(const uint8_t* profile_data,
                                   uint16_t length) {
  if (!profile_data || length != 22) {
    return false;
  }
  if (!initialized_ || !bno_) {
    return false;
  }

  memcpy(cal_blob_, profile_data, sizeof(cal_blob_));
  cal_blob_valid_ = true;

  // setSensorOffsets(const uint8_t*) is void in Adafruit_BNO055 — there is no
  // way to detect an I2C failure from this call. We assume success because
  // length validation has already passed.
  bno_->setSensorOffsets(cal_blob_);
  return true;
}

#endif  // USE_BNO055
