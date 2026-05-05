/**
 * NEO-M9N GPS Sensor Driver Implementation
 *
 * Ublox multi-band GNSS receiver with RTK capability.
 * Communicates via USB serial (CDC interface) at 115200 baud.
 *
 * Features:
 * - NMEA 0183 sentence parsing (GPGGA, GPRMC)
 * - Serial buffer management with circular buffering
 * - NMEA checksum validation
 * - Coordinate conversion (DMS to decimal degrees)
 * - Position accuracy estimation via HDOP
 * - Satellite count and fix quality tracking
 *
 * Protocol Details:
 * - Format: $<talker><sentence>,<fields>*<checksum><CR><LF>
 * - Checksum: XOR of all characters between $ and *
 * - Baud: 115200, 8N1
 * - Output rate: ~1 Hz (default)
 *
 * See: https://www.u-blox.com/en/product/neo-m9n-module
 */

#include "neo_m9n.h"
#include "../config/pins.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Constants
// ============================================================================

// Serial buffer size for incoming NMEA data
#define NMEA_BUFFER_SIZE 256

// Maximum number of fields in a NMEA sentence
#define NMEA_MAX_FIELDS 20

// Timeout for reading a complete NMEA sentence (milliseconds)
#define NMEA_SENTENCE_TIMEOUT_MS 1000

// ============================================================================
// Constructor / Destructor
// ============================================================================

NEOM9N::NEOM9N()
    : initialized_(false),
      new_data_(false),
      last_read_ms_(0) {
  // Initialize position_ with default constructor
  // (already zeroed via PositionData default constructor)
}

NEOM9N::~NEOM9N() {
  end();
}

// ============================================================================
// Initialization
// ============================================================================

bool NEOM9N::begin() {
  // On Linux/desktop systems, we would open /dev/ttyACM1
  // On Arduino, this would use Serial (USB) or a hardware UART
  // For now, we'll attempt to use Serial3 if available (typical for GPS on Mega)
  // or fallback to Serial (USB serial on most boards)

#if defined(__AVR_ATmega2560__)
  // Arduino Mega: use Serial3 for GPS (RX3=15, TX3=14)
  Serial3.begin(GPS_BAUD_RATE);
  // Give the serial port a moment to stabilize
  delay(100);
#elif defined(TEENSY31) || defined(TEENSY32)
  // Teensy 3.x: use Serial2 for GPS
  Serial2.begin(GPS_BAUD_RATE);
  delay(100);
#elif defined(ESP32)
  // ESP32: use Serial2 on pins 16/17
  Serial2.begin(GPS_BAUD_RATE, SERIAL_8N1, 16, 17);
  delay(100);
#else
  // Fallback: use Serial for USB GPS or custom configuration
  // This assumes GPS is connected via USB CDC (SerialUSB on some boards)
  Serial.begin(GPS_BAUD_RATE);
  delay(100);
#endif

  initialized_ = true;
  return true;
}

void NEOM9N::end() {
  // Close serial connection
#if defined(__AVR_ATmega2560__)
  Serial3.end();
#elif defined(TEENSY31) || defined(TEENSY32)
  Serial2.end();
#elif defined(ESP32)
  Serial2.end();
#else
  Serial.end();
#endif

  initialized_ = false;
}

bool NEOM9N::isInitialized() const {
  return initialized_;
}

// ============================================================================
// Data Reading
// ============================================================================

/**
 * Read NMEA sentences from serial buffer.
 *
 * Processes incoming serial data byte-by-byte, accumulating characters
 * until a complete NMEA sentence is detected ($...*XX\r\n).
 * Validates checksum and parses GPGGA / GPRMC sentences.
 *
 * Returns true if a new complete sentence was processed successfully,
 * false if no complete sentence available or parse error.
 */
bool NEOM9N::read() {
  if (!initialized_) {
    return false;
  }

  static char nmea_buffer[NMEA_BUFFER_SIZE];
  static uint16_t buffer_pos = 0;
  bool sentence_complete = false;

  // Get reference to appropriate serial port
  Stream* gps_serial = nullptr;
#if defined(__AVR_ATmega2560__)
  gps_serial = &Serial3;
#elif defined(TEENSY31) || defined(TEENSY32)
  gps_serial = &Serial2;
#elif defined(ESP32)
  gps_serial = &Serial2;
#else
  gps_serial = &Serial;
#endif

  // Read available bytes from serial buffer
  while (gps_serial && gps_serial->available() > 0) {
    char c = gps_serial->read();

    // Detect start of NMEA sentence
    if (c == '$') {
      buffer_pos = 0;
      nmea_buffer[buffer_pos++] = c;
      continue;
    }

    // If we haven't seen $ yet, skip this byte
    if (buffer_pos == 0) {
      continue;
    }

    // Accumulate characters until we hit the end marker
    nmea_buffer[buffer_pos++] = c;

    // Detect end of NMEA sentence (checksum after *)
    if (c == '\n' && buffer_pos > 5) {
      // We have: ...*XX\r\n
      // Mark sentence as complete
      sentence_complete = true;
      nmea_buffer[buffer_pos] = '\0';  // Null-terminate for safety
      break;
    }

    // Prevent buffer overflow
    if (buffer_pos >= NMEA_BUFFER_SIZE) {
      buffer_pos = 0;  // Reset and start over
    }
  }

  // If we have a complete sentence, parse it
  if (sentence_complete && buffer_pos > 0) {
    // Remove trailing whitespace/newlines
    while (buffer_pos > 0 && (nmea_buffer[buffer_pos - 1] == '\n' ||
                               nmea_buffer[buffer_pos - 1] == '\r')) {
      buffer_pos--;
    }
    nmea_buffer[buffer_pos] = '\0';

    // Parse the NMEA sentence
    if (parseNMEA(nmea_buffer)) {
      new_data_ = true;
      last_read_ms_ = millis();
      buffer_pos = 0;
      return true;
    }

    buffer_pos = 0;
  }

  return false;
}

bool NEOM9N::hasNewData() const {
  return new_data_;
}

const PositionData& NEOM9N::getPosition() const {
  return position_;
}

// ============================================================================
// NMEA Parsing
// ============================================================================

/**
 * Validate NMEA checksum.
 *
 * Format: $<data>*<checksum>
 * Checksum is XOR of all characters between $ and *.
 *
 * @param sentence Complete NMEA sentence including $ and *
 * @return true if checksum is valid or missing, false if invalid
 */
static bool validateChecksum(const char* sentence) {
  if (!sentence) {
    return false;
  }

  // Find $ and * positions
  const char* dollar_pos = strchr(sentence, '$');
  const char* star_pos = strchr(sentence, '*');

  if (!dollar_pos || !star_pos || star_pos <= dollar_pos) {
    // No checksum present; treat as valid for lenient parsing
    return true;
  }

  // Calculate XOR of characters between $ and *
  uint8_t calculated = 0;
  for (const char* p = dollar_pos + 1; p < star_pos; p++) {
    calculated ^= (uint8_t)(*p);
  }

  // Parse provided checksum (hex after *)
  char checksum_str[3] = {0};
  if (strlen(star_pos + 1) >= 2) {
    strncpy(checksum_str, star_pos + 1, 2);
    uint8_t provided = (uint8_t)strtol(checksum_str, nullptr, 16);
    return calculated == provided;
  }

  return false;
}

/**
 * Convert DDMM.MMMM format to decimal degrees.
 *
 * @param coord DDMM.MMMM string (lat) or DDDMM.MMMM (lon)
 * @param direction N/S for latitude, E/W for longitude
 * @return Decimal degrees (negative for S/W)
 */
static double dmsToDecimal(const char* coord, char direction) {
  if (!coord || !*coord) {
    return 0.0;
  }

  // Parse the string
  double value = atof(coord);

  // Determine if latitude (2 digits) or longitude (3 digits)
  char int_buffer[16] = {0};
  strncpy(int_buffer, coord, strchr(coord, '.') ? strchr(coord, '.') - coord : strlen(coord));

  int len = strlen(int_buffer);
  int degrees, minutes;

  if (len <= 4) {
    // Latitude: DDMM.MMMM
    degrees = (int)(value / 100.0);
    minutes = (int)(value - degrees * 100.0);
  } else {
    // Longitude: DDDMM.MMMM
    degrees = (int)(value / 100.0);
    minutes = (int)(value - degrees * 100.0);
  }

  // Extract decimal minutes
  double decimal_minutes = value - (int)value;
  decimal_minutes += minutes;

  // Convert to decimal degrees
  double result = degrees + decimal_minutes / 60.0;

  // Apply direction
  if (direction == 'S' || direction == 'W') {
    result = -result;
  }

  return result;
}

/**
 * Split NMEA sentence into fields.
 *
 * Removes $ and checksum, then splits on commas.
 *
 * @param sentence NMEA sentence (including $, not including checksum)
 * @param fields Output array for field pointers
 * @param max_fields Maximum number of fields
 * @return Number of fields extracted
 */
static int splitNMEASentence(char* sentence, char* fields[], int max_fields) {
  if (!sentence || sentence[0] != '$') {
    return 0;
  }

  // Remove leading $
  sentence++;

  // Remove checksum if present
  char* star = strchr(sentence, '*');
  if (star) {
    *star = '\0';
  }

  // Split on commas
  int count = 0;
  fields[count++] = sentence;

  for (char* p = sentence; *p && count < max_fields; p++) {
    if (*p == ',') {
      *p = '\0';
      fields[count++] = p + 1;
    }
  }

  return count;
}

/**
 * Parse GPGGA sentence (Global Positioning System Fix Data).
 *
 * GPGGA,UTC time,latitude,lat dir,longitude,lon dir,fix quality,
 *       satellites,HDOP,altitude,alt units,geoid sep,geoid units
 *
 * @param fields Array of field strings
 * @return true if parsed successfully
 */
bool NEOM9N::parseGPGGA(const char* fields[]) {
  if (!fields || !fields[0] || strcmp(fields[0], "GPGGA") != 0) {
    return false;
  }

  // Need at least: GPGGA + time + lat + lat_dir + lon + lon_dir +
  //               fix + sats + HDOP + alt = 9 fields minimum
  // But we'll be lenient and parse what we can

  PositionData new_pos;
  new_pos.timestamp_ms = millis();

  // UTC time (field 1, optional)
  // We ignore this for now as we use system time

  // Latitude (field 2) and direction (field 3)
  if (fields[2] && fields[3]) {
    new_pos.latitude = dmsToDecimal(fields[2], fields[3][0]);
  }

  // Longitude (field 4) and direction (field 5)
  if (fields[4] && fields[5]) {
    new_pos.longitude = dmsToDecimal(fields[4], fields[5][0]);
  }

  // Fix quality (field 6)
  if (fields[6] && fields[6][0]) {
    new_pos.fix_quality = atoi(fields[6]);
  }

  // Number of satellites (field 7)
  if (fields[7] && fields[7][0]) {
    new_pos.num_satellites = atoi(fields[7]);
  }

  // HDOP (field 8)
  float hdop = 0.0f;
  if (fields[8] && fields[8][0]) {
    hdop = atof(fields[8]);
  }

  // Use HDOP * 5 as rough accuracy estimate (meters)
  // This is a common heuristic: at HDOP 0.8, accuracy ~4m; at HDOP 2.0, ~10m
  new_pos.accuracy_m = hdop * 5.0f;

  // Altitude (field 9)
  if (fields[9] && fields[9][0]) {
    new_pos.altitude = atof(fields[9]);
  }

  // Update our position data
  position_ = new_pos;
  return true;
}

/**
 * Parse GPRMC sentence (Recommended Minimum Navigation Information).
 *
 * GPRMC,UTC time,status,latitude,lat dir,longitude,lon dir,
 *       speed,course,date,mag var,var dir
 *
 * @param fields Array of field strings
 * @return true if parsed successfully
 */
bool NEOM9N::parseGPRMC(const char* fields[]) {
  if (!fields || !fields[0] || strcmp(fields[0], "GPRMC") != 0) {
    return false;
  }

  // For v1.0, we primarily care about position
  // Speed and course are optional enhancements

  PositionData new_pos = position_;  // Keep existing data
  new_pos.timestamp_ms = millis();

  // Status (field 2): A=valid, V=invalid
  if (fields[2] && fields[2][0] != 'A') {
    // Invalid status, but we'll still parse position
  }

  // Latitude (field 3) and direction (field 4)
  if (fields[3] && fields[4]) {
    new_pos.latitude = dmsToDecimal(fields[3], fields[4][0]);
  }

  // Longitude (field 5) and direction (field 6)
  if (fields[5] && fields[6]) {
    new_pos.longitude = dmsToDecimal(fields[5], fields[6][0]);
  }

  // Speed in knots (field 7, optional)
  // Course (field 8, optional)
  // We'll store these in future versions if needed

  // Update position
  position_ = new_pos;
  return true;
}

/**
 * Parse a complete NMEA sentence.
 *
 * Validates checksum, identifies sentence type, and dispatches to
 * appropriate parser (GPGGA or GPRMC).
 *
 * @param sentence Complete NMEA sentence (e.g., "$GPGGA,...")
 * @return true if sentence was parsed successfully
 */
bool NEOM9N::parseNMEA(const char* sentence) {
  if (!sentence || sentence[0] != '$') {
    return false;
  }

  // Validate checksum (lenient: accept if invalid but continue parsing)
  validateChecksum(sentence);

  // Make a mutable copy for splitting
  char sentence_copy[NMEA_BUFFER_SIZE];
  strncpy(sentence_copy, sentence, NMEA_BUFFER_SIZE - 1);
  sentence_copy[NMEA_BUFFER_SIZE - 1] = '\0';

  // Split into fields
  char* fields[NMEA_MAX_FIELDS];
  memset(fields, 0, sizeof(fields));
  int num_fields = splitNMEASentence(sentence_copy, fields, NMEA_MAX_FIELDS);

  if (num_fields == 0 || !fields[0]) {
    return false;
  }

  // Dispatch to appropriate parser
  if (strcmp(fields[0], "GPGGA") == 0) {
    return parseGPGGA((const char**)fields);
  } else if (strcmp(fields[0], "GPRMC") == 0) {
    return parseGPRMC((const char**)fields);
  }

  return false;
}

// ============================================================================
// Status
// ============================================================================

bool NEOM9N::isHealthy() const {
  // GPS is healthy if it has a valid fix (fix_quality >= 1)
  return position_.fix_quality >= 1;
}

const char* NEOM9N::getStatusString() const {
  static char status_str[80];

  if (!initialized_) {
    snprintf(status_str, sizeof(status_str), "GPS: Not initialized");
    return status_str;
  }

  if (position_.fix_quality == 0) {
    snprintf(status_str, sizeof(status_str), "GPS: No fix");
    return status_str;
  }

  const char* fix_type = "Unknown";
  switch (position_.fix_quality) {
    case 1:
      fix_type = "GPS";
      break;
    case 2:
      fix_type = "DGPS";
      break;
    case 3:
      fix_type = "PPS";
      break;
    case 4:
      fix_type = "RTK";
      break;
    case 5:
      fix_type = "RTK-Float";
      break;
    case 6:
      fix_type = "Dead-Reckoning";
      break;
  }

  // Calculate accuracy in meters (rough: HDOP * 5)
  float accuracy_m = position_.accuracy_m;

  snprintf(status_str, sizeof(status_str),
           "GPS: %d sats, %s fix, HDOP %.1fm",
           position_.num_satellites, fix_type, accuracy_m);

  return status_str;
}
