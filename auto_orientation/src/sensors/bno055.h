/**
 * BNO055 Orientation Sensor
 *
 * Bosch BNO055 absolute orientation IMU with onboard sensor fusion,
 * magnetometer, and 22-byte calibration offset/radius profile.
 *
 * See: https://www.adafruit.com/product/2472
 * Library: https://github.com/adafruit/Adafruit_BNO055
 *
 * ============================================================================
 * ADAFRUIT API DOCUMENTATION
 * ============================================================================
 *
 * Key APIs from Adafruit_BNO055 library used in implementation:
 *
 * 1. Construction:
 *    - Adafruit_BNO055(int32_t sensor_id, uint8_t address, TwoWire *wire)
 *      sensor_id - User-supplied ID returned by Adafruit_Sensor::getSensor()
 *      address   - 0x28 (ADR pin to GND, default) or 0x29 (ADR to VCC)
 *      wire      - Wire instance (default &Wire on Mega pins 20/21)
 *
 * 2. Initialization:
 *    - bool begin(adafruit_bno055_opmode_t mode = OPERATION_MODE_NDOF)
 *      Initializes the sensor and sets a fusion operating mode.
 *      OPERATION_MODE_NDOF enables 9-DoF fusion with magnetometer.
 *      Internally blocks ~650 ms while the chip boots.
 *
 *    - void setExtCrystalUse(bool enable)
 *      Enables the external 32 kHz crystal on the breakout (recommended
 *      for stable yaw; must be called AFTER begin()).
 *
 * 3. Data Reading:
 *    - imu::Quaternion getQuat()
 *      Returns the fusion quaternion in BNO055's body frame.
 *      imu::Quaternion exposes w(), x(), y(), z() accessors.
 *
 *      NOTE: We intentionally do NOT use getEvent(&e, VECTOR_EULER) —
 *      the BNO055 Euler output has a 90° pitch discontinuity. Instead,
 *      we derive Euler angles from the quaternion via
 *      math/quaternion_conversions.h.
 *
 * 4. Calibration Status:
 *    - void getCalibration(uint8_t *sys, uint8_t *gyro, uint8_t *accel,
 *                          uint8_t *mag)
 *      Four independent 0-3 accuracies (0=uncalibrated, 3=fully calibrated).
 *
 *    - bool isFullyCalibrated()
 *      Convenience wrapper: all four accuracies == 3.
 *
 * 5. Calibration Persistence:
 *    - bool getSensorOffsets(uint8_t *calibData)         // 22 bytes
 *    - bool getSensorOffsets(adafruit_bno055_offsets_t &offsets_type)
 *    - void setSensorOffsets(const uint8_t *calibData)
 *    - void setSensorOffsets(const adafruit_bno055_offsets_t &offsets_type)
 *      The 22-byte blob is a packed adafruit_bno055_offsets_t (accel/mag/gyro
 *      offsets + accel/mag radius). getSensorOffsets() returns false unless
 *      the sensor is already fully calibrated.
 *
 * ============================================================================
 * FRAME-OF-REFERENCE NOTE
 * ============================================================================
 *
 * BNO055 default body frame: +X = forward, +Y = left, +Z = up (right-handed),
 * which differs subtly from BNO085. For this driver we copy w/x/y/z straight
 * from the chip with no remapping; a follow-up (Phase 4.6.5) will introduce a
 * common body-frame mapping shared with BNO085.
 */

#ifndef BNO055_H
#define BNO055_H

#include "sensor_base.h"
#include "../math/quaternion_conversions.h"

#include <stdint.h>

// Forward declaration - Adafruit library
// (the .cpp includes <Adafruit_BNO055.h> so transitive headers stay there)
class Adafruit_BNO055;

class BNO055 : public OrientationSensor {
 public:
  /**
   * Construct a BNO055 driver instance.
   *
   * @param i2c_address     I2C address: 0x28 (ADR=GND, default) or 0x29.
   * @param use_ext_crystal If true, enable the external 32 kHz crystal after
   *                        begin() — strongly recommended for stable yaw on
   *                        Adafruit breakouts.
   */
  BNO055(uint8_t i2c_address = 0x28, bool use_ext_crystal = true);
  virtual ~BNO055();

  // Sensor interface
  bool begin() override;
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "BNO055"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  // Orientation interface
  const OrientationData& getOrientation() const override;
  bool setCalibrationProfile(const uint8_t* profile_data,
                             uint16_t length) override;
  bool getCalibrationProfile(uint8_t* profile_data,
                             uint16_t* length) override;

  // BNO055-specific helpers
  /**
   * @return True when sys, accel, gyro, and mag accuracies are all == 3.
   */
  bool isFullyCalibrated() const;

  /**
   * Toggle the external-crystal preference. If called after begin(), the
   * change is forwarded to the chip immediately; otherwise it is applied at
   * the next begin().
   */
  void useExternalCrystal(bool enable);

  /**
   * Raw accelerometer reading in m/s² (for the calibration wizard and other
   * diagnostics). xyz buffer is 3 floats: [ax, ay, az]. Returns false and
   * leaves xyz untouched if the driver is not initialized.
   */
  bool getRawAccel(float xyz[3]) override;

  /**
   * Raw gyroscope reading in degrees/second. Used by the balance app's
   * motion detector to distinguish "being held / moved" from "balancing".
   * Returns false and leaves xyz untouched if the driver is not initialized.
   */
  bool getRawGyro(float xyz[3]) override;

 private:
  Adafruit_BNO055* bno_;
  uint8_t i2c_address_;
  bool use_ext_crystal_;
  bool initialized_;
  bool new_data_;
  OrientationData data_;
  uint32_t last_read_ms_;

  // 22-byte adafruit_bno055_offsets_t footprint. Kept around so a successful
  // getCalibrationProfile() can be re-served without re-querying the chip.
  uint8_t cal_blob_[22];
  bool    cal_blob_valid_;
};

#endif  // BNO055_H
