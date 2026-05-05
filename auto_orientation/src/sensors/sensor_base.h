/**
 * Sensor Base Class (Hardware Abstraction Layer)
 *
 * Defines interface for all sensor types.
 * Implementations derive from this to provide sensor-specific behavior.
 */

#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H

#include <stdint.h>

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Orientation data: quaternion representation (w, x, y, z)
 * where w is the scalar (real) component.
 */
struct OrientationData {
  float w, x, y, z;        // Quaternion components
  uint8_t cal_status;      // Calibration status (0-3, 3=fully calibrated)
  uint8_t cal_accel;       // Accelerometer calibration (0-3)
  uint8_t cal_gyro;        // Gyroscope calibration (0-3)
  uint8_t cal_mag;         // Magnetometer calibration (0-3)
  uint32_t timestamp_ms;   // Timestamp in milliseconds

  OrientationData() :
    w(0), x(0), y(0), z(0),
    cal_status(0), cal_accel(0), cal_gyro(0), cal_mag(0),
    timestamp_ms(0) {}
};

/**
 * Position data: latitude, longitude, altitude + accuracy
 */
struct PositionData {
  double latitude;         // Degrees
  double longitude;        // Degrees
  float altitude;          // Meters (above WGS84 ellipsoid)
  float accuracy_m;        // Estimated accuracy in meters (CEP)
  uint8_t num_satellites;  // Number of satellites used
  uint8_t fix_quality;     // 0=invalid, 1=GPS fix, 2=DGPS, 3=PPS, etc.
  uint32_t timestamp_ms;   // Timestamp in milliseconds

  PositionData() :
    latitude(0), longitude(0), altitude(0),
    accuracy_m(0), num_satellites(0), fix_quality(0),
    timestamp_ms(0) {}
};

/**
 * Combined sensor output: orientation + position + metadata
 */
struct SensorOutput {
  OrientationData orientation;
  PositionData position;
  bool orientation_valid;
  bool position_valid;
  uint32_t system_time_ms;

  SensorOutput() :
    orientation_valid(false), position_valid(false), system_time_ms(0) {}
};

// ============================================================================
// Sensor Base Class
// ============================================================================

class Sensor {
 public:
  virtual ~Sensor() {}

  // Initialization
  virtual bool begin() = 0;
  virtual void end() = 0;
  virtual bool isInitialized() const = 0;

  // Data reading
  virtual bool read() = 0;
  virtual bool hasNewData() const = 0;

  // Sensor type
  virtual const char* name() const = 0;

  // Status
  virtual bool isHealthy() const = 0;
  virtual const char* getStatusString() const = 0;
};

// ============================================================================
// Orientation Sensor Base Class
// ============================================================================

class OrientationSensor : public Sensor {
 public:
  virtual const OrientationData& getOrientation() const = 0;
  virtual bool setCalibrationProfile(const uint8_t* profile_data, uint16_t length) = 0;
  virtual bool getCalibrationProfile(uint8_t* profile_data, uint16_t* length) = 0;
};

// ============================================================================
// Position Sensor Base Class
// ============================================================================

class PositionSensor : public Sensor {
 public:
  virtual const PositionData& getPosition() const = 0;
};

#endif  // SENSOR_BASE_H
