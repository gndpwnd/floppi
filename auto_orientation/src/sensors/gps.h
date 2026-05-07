/**
 * GPS Module Driver (NMEA Parsing)
 *
 * Supports NMEA sentence parsing for GPS position/velocity data.
 * Inherits from PositionSensor base class.
 *
 * Supported Sentences:
 * - GNGGA / GPGGA: Position, altitude, fix quality, satellite count, HDOP
 * - GNRMC / GPRMC: Velocity data, date/time
 *
 * Hardware:
 * - Arduino Mega: Uses Serial1 (UART on pins 18/19)
 * - Configurable baud rate (default 9600)
 *
 * NMEA Format Example:
 *   $GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
 *   $GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
 *
 * Checksum: XOR of all characters between $ and *, result in hex (last 2 digits)
 *
 * Error Handling:
 * - Invalid fix quality (0) or < 4 satellites → position_valid = false
 * - No data for > 1000ms → marked as stale
 * - Bad checksums → sentence rejected
 * - HDOP > 5.0 → position marked as uncertain (but still valid)
 *
 * Data Members:
 * - serial: HardwareSerial pointer (Serial1 on Mega)
 * - baud_rate: UART baud rate (typically 9600)
 * - position_data: Current PositionData struct
 * - velocity_mps: Current velocity in m/s (from GPRMC speed knots)
 * - last_update_ms: Timestamp of last successful read
 * - has_lock: True if fix_quality >= 1 and satellites >= 4
 */

#ifndef GPS_H
#define GPS_H

#include "sensor_base.h"
#include <stdint.h>

// Forward declaration for Arduino HardwareSerial
class HardwareSerial;

class GPS : public PositionSensor {
 public:
  GPS();
  virtual ~GPS();

  // Sensor interface
  bool begin() override;
  bool begin(uint32_t baud);  // Initialize with specific baud rate
  void end() override;
  bool isInitialized() const override;
  bool read() override;
  bool hasNewData() const override;
  const char* name() const override { return "GPS"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  // Position interface
  const PositionData& getPosition() const override;

  // GPS-specific data access
  float getVelocityMps() const { return velocity_mps_; }
  uint32_t getLastUpdateMs() const { return last_update_ms_; }
  bool hasLock() const { return has_lock_; }

  // Configuration
  void setSerialPort(HardwareSerial* serial);
  void setBaudRate(uint32_t baud) { baud_rate_ = baud; }

 private:
  // Hardware
  HardwareSerial* serial_;
  uint32_t baud_rate_;

  // Data
  PositionData position_data_;
  float velocity_mps_;
  uint32_t last_update_ms_;
  bool has_lock_;
  bool initialized_;
  bool new_data_;

  // Status string buffer
  char status_string_[64];

  // NMEA parsing
  static const uint16_t SENTENCE_BUFFER_SIZE = 128;
  char sentence_buffer_[SENTENCE_BUFFER_SIZE];
  uint16_t sentence_pos_;

  // Parser state
  enum ParseState {
    STATE_IDLE,        // Waiting for '$'
    STATE_READING,     // Reading sentence until '*'
    STATE_CHECKSUM     // Reading checksum
  };
  ParseState parse_state_;

  // Helper methods
  bool parseSentence(const char* sentence);
  bool parseGPGGA(const char* sentence);
  bool parseGPRMC(const char* sentence);
  bool validateChecksum(const char* sentence) const;
  uint8_t parseHexByte(const char* hex) const;
  bool extractField(const char* sentence, uint8_t field_num, char* output,
                    uint8_t output_len) const;
  uint32_t getCurrentTimeMs() const;
  bool isStale() const;
};

#endif  // GPS_H
