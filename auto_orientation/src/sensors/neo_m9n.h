/**
 * Ublox NEO-M9N GPS Receiver
 *
 * Multi-band GNSS receiver with RTK capability.
 * Communicates via USB serial (CDC interface).
 *
 * See: https://www.u-blox.com/en/product/neo-m9n-module
 */

#ifndef NEO_M9N_H
#define NEO_M9N_H

#include "sensor_base.h"

class NEOM9N : public PositionSensor {
 public:
  NEOM9N();
  virtual ~NEOM9N();

  // Sensor interface
  bool begin() override;
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "NEO-M9N GPS"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  // Position interface
  const PositionData& getPosition() const override;

 private:
  PositionData position_;
  bool initialized_;
  bool new_data_;
  uint32_t last_read_ms_;

  // NMEA parsing
  bool parseNMEA(const char* sentence);
  bool parseGPGGA(const char* fields[]);  // Global Positioning System Fix Data
  bool parseGPRMC(const char* fields[]);  // Recommended Minimum Navigation Information
};

#endif  // NEO_M9N_H
