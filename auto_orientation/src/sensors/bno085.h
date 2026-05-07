/**
 * BNO085 Orientation Sensor
 *
 * Absolute orientation IMU with onboard sensor fusion, magnetometer,
 * and persistent calibration memory.
 *
 * See: https://www.adafruit.com/product/4754
 * Library: https://github.com/adafruit/Adafruit_BNO08x_Arduino
 *
 * ============================================================================
 * ADAFRUIT API DOCUMENTATION
 * ============================================================================
 *
 * Key APIs from Adafruit_BNO08x library used in implementation:
 *
 * 1. Initialization:
 *    - begin_I2C(uint8_t addr, TwoWire *wire, int reset_pin)
 *      Initialize sensor on I2C bus
 *      Parameters:
 *        addr - I2C address (0x4A if DI pin to GND, 0x4B if DI pin to VCC)
 *        wire - Wire object (default &Wire for Arduino Mega pins 20/21)
 *        reset_pin - GPIO pin for hardware reset (-1 if not available)
 *      Returns: true if initialization successful
 *
 * 2. Report Configuration:
 *    - enableReport(uint16_t reportID, uint32_t period_us)
 *      Enable a specific sensor report with update period
 *      Parameters:
 *        reportID - Sensor report type (5 = Rotation Vector / Absolute Orientation)
 *        period_us - Update period in microseconds (100000 = 100ms = 10 Hz)
 *
 * 3. Data Reading:
 *    - getEvent(sh2_SensorEvent *event)
 *      Read next available sensor event
 *      Returns: true if new data available, false if no new data
 *      Output: event structure containing quaternion (w,x,y,z)
 *
 * 4. Calibration Status:
 *    - getCalibration(uint8_t *sys, uint8_t *gyro, uint8_t *accel, uint8_t *mag)
 *      Query calibration status for each axis
 *      Output values: 0 = Uncalibrated, 1 = Low, 2 = Medium, 3 = High
 *
 * 5. Reset Detection:
 *    - wasReset() - Returns true if sensor was reset, clears flag
 *
 * ============================================================================
 * CALIBRATION PERSISTENCE (Research Phase)
 * ============================================================================
 *
 * TODO: Investigate if Adafruit_BNO08x provides:
 *   - getSensorOffsetProfiler() - Save calibration state to buffer
 *   - setSensorOffsetProfiler() - Restore calibration from buffer
 *
 * If not available in library, implement via:
 *   1. Direct register reads (BNO085 has 256 calibration bytes in NVM)
 *   2. EEPROM persistence on Arduino
 *   3. SD card backup on systems with storage
 *
 * See findings/ directory for calibration research results.
 */

#ifndef BNO085_H
#define BNO085_H

#include "sensor_base.h"
#include "../math/quaternion_conversions.h"

// Forward declaration - Adafruit library
class Adafruit_BNO08x;

class BNO085 : public OrientationSensor {
 public:
  BNO085();
  virtual ~BNO085();

  // Sensor interface
  bool begin() override;
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "BNO085"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  // Orientation interface
  const OrientationData& getOrientation() const override;
  bool setCalibrationProfile(const uint8_t* profile_data, uint16_t length) override;
  bool getCalibrationProfile(uint8_t* profile_data, uint16_t* length) override;

  /**
   * Get the rotation matrix corresponding to the current quaternion.
   * The matrix is computed from the quaternion using the formula from QUATERNION_REFERENCE.md.
   * @return 3x3 rotation matrix (row-major)
   */
  const RotationMatrix& getRotationMatrix();

  /**
   * Get Euler angles (roll, pitch, yaw) for the current quaternion.
   * Returned in both radians and degrees.
   * Uses ZYX convention (intrinsic rotations).
   * @return Structure containing roll, pitch, yaw in both radians and degrees
   */
  const EulerAngles& getEulerAngles();

  /**
   * Validate the quaternion magnitude.
   * For a valid rotation quaternion, magnitude should be ~1.0.
   * Logs warning if magnitude deviates by more than the tolerance.
   * @param tolerance Maximum acceptable deviation from 1.0 (default 0.001 = 0.1%)
   * @return True if quaternion magnitude is valid
   */
  bool validateQuaternionMagnitude(float tolerance = 0.001f);

 private:
  Adafruit_BNO08x* imu_;
  OrientationData orientation_;
  bool initialized_;
  bool new_data_;
  uint32_t last_read_ms_;

  // Calibration data
  uint8_t calibration_data_[256];
  uint16_t calibration_data_length_;

  // Calibration persistence
  uint8_t last_saved_cal_level_;  // Track last saved calibration to avoid re-saving

  // Cached conversions (computed lazily)
  RotationMatrix rotation_matrix_;
  EulerAngles euler_angles_;
  Quaternion last_quaternion_;      // Last quaternion used for caching
  bool cache_valid_;                // Whether cached values are up-to-date
};

#endif  // BNO085_H
