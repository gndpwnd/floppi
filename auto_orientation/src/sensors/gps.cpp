/**
 * GPS Module Driver Implementation
 *
 * NMEA sentence parsing for GPS position and velocity data.
 * Supports GNGGA (position) and GNRMC/GPRMC (velocity) sentences.
 */

#include "gps.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef ARDUINO
  #include <Arduino.h>
  #define MILLIS() millis()
#else
  // For testing without Arduino hardware
  #include <chrono>
  static uint32_t test_millis_base = 0;
  uint32_t millis() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    return (uint32_t)(duration.count() - test_millis_base);
  }
  #define MILLIS() millis()

  // Host-side stub for Arduino's HardwareSerial. gps.h forward-declares this
  // class so the production driver can hold a HardwareSerial* without
  // including <Arduino.h> from the header. On native builds we never wire up
  // a real serial port (serial_ stays nullptr and GPS::read() short-circuits),
  // but the class must be a complete type for the compiler to emit method
  // calls in this TU. Methods are unreachable on NATIVE_TEST.
  class HardwareSerial {
   public:
    void begin(unsigned long) {}
    void end() {}
    int available() { return 0; }
    int read() { return -1; }
  };
#endif

// ============================================================================
// Constructor / Destructor
// ============================================================================

GPS::GPS()
    : serial_(nullptr),
      baud_rate_(9600),
      velocity_mps_(0.0f),
      last_update_ms_(0),
      has_lock_(false),
      initialized_(false),
      new_data_(false),
      sentence_pos_(0),
      parse_state_(STATE_IDLE) {
  position_data_ = PositionData();
  memset(status_string_, 0, sizeof(status_string_));
  memset(sentence_buffer_, 0, sizeof(sentence_buffer_));
  strncpy(status_string_, "Not initialized", sizeof(status_string_) - 1);
}

GPS::~GPS() {
  end();
}

// ============================================================================
// Sensor Interface Implementation
// ============================================================================

bool GPS::begin() {
#ifdef ARDUINO
  // Arduino Mega: use Serial1 (pins 18/19)
  if (serial_ == nullptr) {
    serial_ = &Serial1;
  }
  serial_->begin(baud_rate_);
#endif
  initialized_ = true;
  last_update_ms_ = getCurrentTimeMs();
  return true;
}

bool GPS::begin(uint32_t baud) {
  baud_rate_ = baud;
  return begin();
}

void GPS::end() {
#ifdef ARDUINO
  if (serial_ != nullptr && initialized_) {
    serial_->end();
  }
#endif
  initialized_ = false;
}

bool GPS::isInitialized() const {
  return initialized_;
}

bool GPS::read() {
  if (!initialized_ || serial_ == nullptr) {
    return false;
  }

  new_data_ = false;

  // Read available bytes from serial buffer
  while (serial_->available() > 0) {
    char c = (char)serial_->read();

    switch (parse_state_) {
      case STATE_IDLE:
        if (c == '$') {
          sentence_pos_ = 0;
          memset(sentence_buffer_, 0, sizeof(sentence_buffer_));
          parse_state_ = STATE_READING;
        }
        break;

      case STATE_READING:
        if (c == '*') {
          parse_state_ = STATE_CHECKSUM;
          sentence_buffer_[sentence_pos_] = '\0';
        } else if (sentence_pos_ < SENTENCE_BUFFER_SIZE - 1) {
          sentence_buffer_[sentence_pos_++] = c;
        } else {
          // Buffer overflow, reset
          parse_state_ = STATE_IDLE;
          sentence_pos_ = 0;
        }
        break;

      case STATE_CHECKSUM:
        // Read two hex characters for checksum
        if (sentence_pos_ < SENTENCE_BUFFER_SIZE - 1) {
          sentence_buffer_[sentence_pos_++] = c;

          // After reading 2 checksum characters and newline
          if (sentence_pos_ >= 2 && c == '\n') {
            // Extract the checksum characters
            char checksum_str[3];
            checksum_str[0] = sentence_buffer_[sentence_pos_ - 3];
            checksum_str[1] = sentence_buffer_[sentence_pos_ - 2];
            checksum_str[2] = '\0';

            // Append checksum back to sentence for validation
            sentence_buffer_[sentence_pos_ - 3] = '*';
            sentence_buffer_[sentence_pos_ - 2] = checksum_str[0];
            sentence_buffer_[sentence_pos_ - 1] = checksum_str[1];
            sentence_buffer_[sentence_pos_] = '\0';

            // Validate and parse
            if (validateChecksum(sentence_buffer_)) {
              if (parseSentence(sentence_buffer_)) {
                new_data_ = true;
                last_update_ms_ = getCurrentTimeMs();
              }
            }

            parse_state_ = STATE_IDLE;
            sentence_pos_ = 0;
          }
        }
        break;
    }
  }

  // Check for stale data
  if (has_lock_ && isStale()) {
    has_lock_ = false;
    position_data_.fix_quality = 0;
    strncpy(status_string_, "GPS data stale (>1000ms)", sizeof(status_string_) - 1);
  }

  return new_data_;
}

bool GPS::hasNewData() const {
  return new_data_;
}

bool GPS::isHealthy() const {
  return initialized_ && has_lock_ && !isStale();
}

const char* GPS::getStatusString() const {
  return status_string_;
}

const PositionData& GPS::getPosition() const {
  return position_data_;
}

// ============================================================================
// NMEA Parsing
// ============================================================================

bool GPS::parseSentence(const char* sentence) {
  if (sentence == nullptr || sentence[0] == '\0') {
    return false;
  }

  // Extract the sentence type (first 6 characters after $)
  // Format: $GNGGA,... or $GPGGA,... or $GNRMC,... or $GPRMC,...
  if (strlen(sentence) < 6) {
    return false;
  }

  // Check for GNGGA or GPGGA (position sentence)
  if ((strncmp(sentence, "GNGGA", 5) == 0 || strncmp(sentence, "GPGGA", 5) == 0) &&
      sentence[5] == ',') {
    return parseGPGGA(sentence);
  }

  // Check for GNRMC or GPRMC (RMC sentence with velocity)
  if ((strncmp(sentence, "GNRMC", 5) == 0 || strncmp(sentence, "GPRMC", 5) == 0) &&
      sentence[5] == ',') {
    return parseGPRMC(sentence);
  }

  return false;
}

bool GPS::parseGPGGA(const char* sentence) {
  // GPGGA format:
  // $GNGGA,hhmmss.ss,ddmm.mmmm,N/S,dddmm.mmmm,E/W,q,nn,hdop,alt,M,geoid,M,age,ref*hh
  //
  // Fields (1-indexed after sentence type):
  // 0: UTC time (hhmmss.ss)
  // 1: Latitude (ddmm.mmmm)
  // 2: N/S
  // 3: Longitude (dddmm.mmmm)
  // 4: E/W
  // 5: Fix quality (0=invalid, 1=GPS, 2=DGPS, etc.)
  // 6: Number of satellites
  // 7: HDOP (horizontal dilution of precision)
  // 8: Altitude above sea level (meters)
  // 9: "M" (meters)
  // 10: Geoid height
  // 11: "M"

  char field[32];
  uint8_t fix_quality = 0;
  uint8_t num_satellites = 0;
  double latitude = 0.0;
  double longitude = 0.0;
  float altitude = 0.0f;
  float hdop = 0.0f;
  bool ns_is_north = true;
  bool ew_is_east = true;

  // Skip past "GNGGA" or "GPGGA"
  const char* data = sentence + 6;

  // Field 0: UTC time (skip)
  if (!extractField(data, 0, field, sizeof(field))) return false;

  // Field 1: Latitude (ddmm.mmmm)
  if (!extractField(data, 1, field, sizeof(field))) return false;
  if (field[0] == '\0') return false;

  // Parse latitude: ddmm.mmmm format
  double lat_deg = strtod(field, nullptr);
  int lat_d = (int)(lat_deg / 100.0);
  double lat_m = lat_deg - (lat_d * 100.0);
  latitude = lat_d + lat_m / 60.0;

  // Field 2: N/S
  if (!extractField(data, 2, field, sizeof(field))) return false;
  ns_is_north = (field[0] == 'N');
  if (!ns_is_north) latitude = -latitude;

  // Field 3: Longitude (dddmm.mmmm)
  if (!extractField(data, 3, field, sizeof(field))) return false;
  if (field[0] == '\0') return false;

  // Parse longitude: dddmm.mmmm format
  double lon_deg = strtod(field, nullptr);
  int lon_d = (int)(lon_deg / 100.0);
  double lon_m = lon_deg - (lon_d * 100.0);
  longitude = lon_d + lon_m / 60.0;

  // Field 4: E/W
  if (!extractField(data, 4, field, sizeof(field))) return false;
  ew_is_east = (field[0] == 'E');
  if (!ew_is_east) longitude = -longitude;

  // Field 5: Fix quality
  if (!extractField(data, 5, field, sizeof(field))) return false;
  fix_quality = (uint8_t)strtol(field, nullptr, 10);

  if (fix_quality == 0) {
    // No fix - mark invalid
    strncpy(status_string_, "No GPS fix", sizeof(status_string_) - 1);
    position_data_.fix_quality = 0;
    has_lock_ = false;
    return false;
  }

  // Field 6: Number of satellites
  if (!extractField(data, 6, field, sizeof(field))) return false;
  num_satellites = (uint8_t)strtol(field, nullptr, 10);

  if (num_satellites < 4) {
    // Insufficient satellites
    snprintf(status_string_, sizeof(status_string_), "Only %u satellites", num_satellites);
    position_data_.fix_quality = 0;
    has_lock_ = false;
    return false;
  }

  // Field 7: HDOP
  if (!extractField(data, 7, field, sizeof(field))) return false;
  hdop = (float)strtod(field, nullptr);

  // Field 8: Altitude
  if (!extractField(data, 8, field, sizeof(field))) return false;
  altitude = (float)strtod(field, nullptr);

  // Validate coordinate ranges
  if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
    strncpy(status_string_, "Invalid coordinate range", sizeof(status_string_) - 1);
    return false;
  }

  // Update position data
  position_data_.latitude = latitude;
  position_data_.longitude = longitude;
  position_data_.altitude = altitude;
  position_data_.fix_quality = fix_quality;
  position_data_.num_satellites = num_satellites;
  position_data_.accuracy_m = hdop * 3.0f;  // Rough estimate: HDOP * 3 = horizontal error
  position_data_.timestamp_ms = getCurrentTimeMs();

  has_lock_ = true;
  snprintf(status_string_, sizeof(status_string_),
           "GPS lock: %u sats, HDOP=%.2f", num_satellites, hdop);

  return true;
}

bool GPS::parseGPRMC(const char* sentence) {
  // GPRMC format:
  // $GNRMC,hhmmss.ss,status,ddmm.mmmm,N/S,dddmm.mmmm,E/W,speed,course,ddmmyy,mag_var,mag_var_dir,mode*hh
  //
  // Fields (1-indexed after sentence type):
  // 0: UTC time
  // 1: Status (A=active/valid, V=void/invalid)
  // 2: Latitude
  // 3: N/S
  // 4: Longitude
  // 5: E/W
  // 6: Speed over ground (knots)
  // 7: Course over ground (degrees)
  // 8: Date (ddmmyy)
  // 9: Magnetic variation
  // 10: E/W of magnetic variation

  char field[32];
  char status;
  float speed_knots = 0.0f;

  // Skip past "GNRMC" or "GPRMC"
  const char* data = sentence + 6;

  // Field 0: UTC time (skip)
  if (!extractField(data, 0, field, sizeof(field))) return false;

  // Field 1: Status
  if (!extractField(data, 1, field, sizeof(field))) return false;
  if (field[0] == '\0') return false;
  status = field[0];

  if (status != 'A') {
    // Data not valid
    has_lock_ = false;
    return false;
  }

  // Fields 2-5: Position (parse but we mainly use GGA for position)
  if (!extractField(data, 2, field, sizeof(field))) return false;  // lat
  if (!extractField(data, 3, field, sizeof(field))) return false;  // N/S
  if (!extractField(data, 4, field, sizeof(field))) return false;  // lon
  if (!extractField(data, 5, field, sizeof(field))) return false;  // E/W

  // Field 6: Speed over ground (knots)
  if (!extractField(data, 6, field, sizeof(field))) return false;
  speed_knots = (float)strtod(field, nullptr);

  // Convert knots to m/s (1 knot = 0.51444 m/s)
  velocity_mps_ = speed_knots * 0.51444f;

  // Field 7: Course (skip for now)
  // if (!extractField(data, 7, field, sizeof(field))) return false;

  return true;
}

// ============================================================================
// Helper Methods
// ============================================================================

bool GPS::validateChecksum(const char* sentence) const {
  // Find the asterisk
  const char* asterisk = strchr(sentence, '*');
  if (asterisk == nullptr || strlen(asterisk) < 3) {
    return false;
  }

  // Calculate XOR checksum from $ to *
  uint8_t calculated_checksum = 0;
  const char* p = sentence;

  while (p != asterisk) {
    calculated_checksum ^= (uint8_t)(*p);
    p++;
  }

  // Parse the checksum from the sentence
  uint8_t sentence_checksum = parseHexByte(asterisk + 1);

  return (calculated_checksum == sentence_checksum);
}

uint8_t GPS::parseHexByte(const char* hex) const {
  if (hex == nullptr || strlen(hex) < 2) {
    return 0;
  }

  char byte_str[3];
  byte_str[0] = hex[0];
  byte_str[1] = hex[1];
  byte_str[2] = '\0';

  return (uint8_t)strtol(byte_str, nullptr, 16);
}

bool GPS::extractField(const char* sentence, uint8_t field_num, char* output,
                       uint8_t output_len) const {
  if (sentence == nullptr || output == nullptr || output_len == 0) {
    return false;
  }

  memset(output, 0, output_len);

  const char* p = sentence;
  uint8_t current_field = 0;
  uint16_t output_pos = 0;

  // Find the start of the requested field
  while (*p != '\0' && current_field < field_num) {
    if (*p == ',') {
      current_field++;
    }
    p++;
  }

  if (current_field != field_num) {
    return false;  // Field not found
  }

  // Extract the field content until ',' or '*' or end of string
  while (*p != '\0' && *p != ',' && *p != '*' && output_pos < output_len - 1) {
    output[output_pos++] = *p;
    p++;
  }

  output[output_pos] = '\0';
  return true;
}

uint32_t GPS::getCurrentTimeMs() const {
#ifdef ARDUINO
  return millis();
#else
  return MILLIS();
#endif
}

bool GPS::isStale() const {
  uint32_t now = getCurrentTimeMs();
  uint32_t age_ms = now - last_update_ms_;
  return age_ms > 1000;  // Stale if no update for 1 second
}

void GPS::setSerialPort(HardwareSerial* serial) {
  serial_ = serial;
}
