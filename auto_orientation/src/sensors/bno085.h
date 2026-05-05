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
 *    - begin_UART(Stream *serial, int reset_pin)
 *      Initialize sensor on hardware/software serial stream
 *      Parameters:
 *        serial - Stream object (Serial1, SoftwareSerial, etc.)
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

 private:
  Adafruit_BNO08x* imu_;
  OrientationData orientation_;
  bool initialized_;
  bool new_data_;
  uint32_t last_read_ms_;

  // Calibration data
  uint8_t calibration_data_[256];
  uint16_t calibration_data_length_;
};

#endif  // BNO085_H
