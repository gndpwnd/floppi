/*
 * Adafruit BNO08x Library - Stub Header
 * 
 * This is a minimal forward declaration to allow compilation.
 * The actual library will be cloned when network access is available.
 * For now, we provide key class definitions and function stubs.
 */

#ifndef ADAFRUIT_BNO08X_ARDUINO_H
#define ADAFRUIT_BNO08X_ARDUINO_H

#include <Arduino.h>
#include <Wire.h>

// Calibration status levels
#define SENSOR_CALIBRATION_UNRELIABLE 0
#define SENSOR_CALIBRATION_LOW 1
#define SENSOR_CALIBRATION_MEDIUM 2
#define SENSOR_CALIBRATION_HIGH 3

// Quaternion data structure
typedef struct {
  float w, x, y, z;
  uint8_t accuracy;
} sh2_quat_t;

// Sensor reports (events)
struct sh2_SensorEvent {
  uint16_t eventID;
  uint32_t timestamp;
  
  union {
    sh2_quat_t quat;
    struct {
      float x, y, z;
      uint8_t accuracy;
    } gyro;
    struct {
      float x, y, z;
      uint8_t accuracy;
    } accel;
    struct {
      float x, y, z;
      uint8_t accuracy;
    } mag;
  } un;
};

// Adafruit BNO08x main class
class Adafruit_BNO08x {
 public:
  Adafruit_BNO08x();
  ~Adafruit_BNO08x();

  // Initialization methods
  bool begin_I2C(uint8_t i2c_addr = 0x4A, TwoWire *wire = &Wire);
  bool begin_UART(Stream *serial, int reset_pin = -1);
  
  // Enable/disable reports
  bool enableReport(uint16_t reportID, uint32_t period_us = 100000);
  void reset();
  
  // Data reading
  bool wasReset();
  bool getEvent(sh2_SensorEvent *event);
  
  // Calibration status
  void getCalibration(uint8_t *sys, uint8_t *gyro, uint8_t *accel, uint8_t *mag);
  uint8_t getCalibrationStatus();
  
  // Calibration save/restore (if available)
  bool getSensorOffsetProfiler(uint8_t *profile_data, size_t max_length, size_t *output_length);
  bool setSensorOffsetProfiler(const uint8_t *profile_data, size_t length);

 private:
  Stream *serial_port_;
  TwoWire *i2_wire_;
  uint8_t i2c_addr_;
  int reset_pin_;
  bool was_reset_;
};

// Report IDs
#define SENSORMODULE_COMMAND_REQUEST 0xF0
#define SENSORMODULE_COMMAND_RESPONSE 0xF1

// Common report IDs
#define QUAT_REPORT_ID 5        // Rotation Vector (Absolute Orientation)
#define QUAT_RVC_REPORT_ID 14   // Rotation Vector (RVC variant)
#define ACCEL_REPORT_ID 1
#define GYRO_REPORT_ID 2
#define MAG_REPORT_ID 3

#endif // ADAFRUIT_BNO08X_ARDUINO_H
