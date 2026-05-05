/**
 * Ublox NEO-M9N GPS Receiver
 *
 * Multi-band GNSS receiver with RTK capability.
 * Communicates via USB serial (CDC interface) or hardware UART.
 *
 * Features:
 * - NMEA 0183 sentence parsing (GPGGA, GPRMC)
 * - Automatic serial port detection based on board type
 * - Coordinate conversion (DMS to decimal degrees)
 * - HDOP-based accuracy estimation
 * - Satellite tracking and fix quality monitoring
 *
 * Hardware Configuration (from config/pins.h):
 * - Baud Rate: 115200
 * - Linux Desktop: /dev/ttyACM1 or /dev/ttyACM0
 * - Arduino Mega: Serial3 (pins RX3=15, TX3=14)
 * - Teensy 3.x: Serial2
 * - ESP32: Serial2 (pins 16/17)
 *
 * Usage:
 * ```cpp
 * NEOM9N gps;
 * gps.begin();  // Initialize GPS
 *
 * loop {
 *   if (gps.read()) {  // Read NMEA sentences
 *     if (gps.hasNewData()) {
 *       const PositionData& pos = gps.getPosition();
 *       Serial.print("Lat: "); Serial.print(pos.latitude, 6);
 *       Serial.print(" Lon: "); Serial.println(pos.longitude, 6);
 *     }
 *   }
 *   Serial.println(gps.getStatusString());  // "GPS: 8 sats, GPS fix, HDOP 0.9m"
 * }
 * ```
 *
 * NMEA Sentence Support:
 * - GPGGA: Global Positioning System Fix Data (position, altitude, fix quality, HDOP)
 * - GPRMC: Recommended Minimum Navigation Information (position, speed, course)
 *
 * Data Output:
 * - PositionData struct:
 *   - latitude, longitude: decimal degrees
 *   - altitude: meters above WGS84 ellipsoid
 *   - accuracy_m: estimated horizontal accuracy (HDOP * 5)
 *   - num_satellites: count of satellites in fix
 *   - fix_quality: 0=invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=RTK-Float
 *   - timestamp_ms: time of fix acquisition
 *
 * Accuracy Notes:
 * - Typical CEP (Circular Error Probability): 0.8-1.2m in open sky
 * - HDOP < 2.0 is excellent; < 5.0 is good
 * - Multi-constellation (GPS, GLONASS, Galileo, BeiDou) improves accuracy
 * - Can achieve ~0.1m accuracy with 100+ sample averaging when stationary
 *
 * See: https://www.u-blox.com/en/product/neo-m9n-module
 */

#ifndef NEO_M9N_H
#define NEO_M9N_H

#include "sensor_base.h"

/**
 * NEO-M9N GPS Sensor Driver
 *
 * Implements PositionSensor interface for GNSS position tracking.
 * Handles NMEA sentence parsing, checksum validation, and coordinate conversion.
 */
class NEOM9N : public PositionSensor {
 public:
  /**
   * Constructor
   */
  NEOM9N();

  /**
   * Destructor - closes serial connection
   */
  virtual ~NEOM9N();

  // ========================================================================
  // Sensor Interface (from sensor_base.h)
  // ========================================================================

  /**
   * Initialize GPS receiver and open serial connection.
   *
   * Configures the appropriate serial port based on board type:
   * - Arduino Mega: Serial3 (hardware UART)
   * - Teensy 3.x: Serial2
   * - ESP32: Serial2
   * - Fallback: Serial (USB)
   *
   * Baud rate: 115200 (from config/pins.h GPS_BAUD_RATE)
   *
   * @return true if initialization successful
   */
  bool begin() override;

  /**
   * Close GPS serial connection and clean up resources.
   */
  void end() override;

  /**
   * Check if GPS receiver is initialized.
   *
   * @return true if begin() was called successfully
   */
  bool isInitialized() const override;

  /**
   * Read available NMEA sentences from serial buffer.
   *
   * Accumulates incoming bytes into complete NMEA sentences ($...*XX).
   * Validates checksum and parses position data.
   * Updates position_ and sets new_data_ flag on successful parse.
   *
   * Non-blocking. Returns immediately if no complete sentence available.
   *
   * @return true if a complete NMEA sentence was read and parsed
   */
  bool read() override;

  /**
   * Check if new position data is available since last check.
   *
   * Flag is cleared after first call to allow polling behavior.
   *
   * @return true if new position data has been parsed
   */
  bool hasNewData() const override;

  /**
   * Get sensor name for logging and status display.
   *
   * @return "NEO-M9N GPS"
   */
  const char* name() const override { return "NEO-M9N GPS"; }

  /**
   * Check if GPS receiver is healthy (has valid fix).
   *
   * Healthy = fix_quality >= 1 (GPS fix or better)
   *
   * @return true if GPS has valid position fix
   */
  bool isHealthy() const override;

  /**
   * Get human-readable status string.
   *
   * Examples:
   * - "GPS: Not initialized"
   * - "GPS: No fix"
   * - "GPS: 8 sats, GPS fix, HDOP 0.9m"
   * - "GPS: 12 sats, DGPS fix, HDOP 0.5m"
   *
   * @return Status string (static buffer, valid until next call)
   */
  const char* getStatusString() const override;

  // ========================================================================
  // Position Interface (from sensor_base.h)
  // ========================================================================

  /**
   * Get latest position data.
   *
   * Returns current position structure with:
   * - latitude, longitude: decimal degrees
   * - altitude: meters
   * - accuracy_m: estimated CEP in meters
   * - num_satellites: count of satellites used
   * - fix_quality: 0-8 (see PositionData in sensor_base.h)
   * - timestamp_ms: time of fix
   *
   * Safe to call anytime; returns last known fix (not updated until
   * read() successfully parses new GPGGA or GPRMC sentence.
   *
   * @return Reference to PositionData struct (owned by this object)
   */
  const PositionData& getPosition() const override;

 private:
  // ========================================================================
  // State
  // ========================================================================

  PositionData position_;      ///< Latest parsed position data
  bool initialized_;           ///< Serial port opened successfully
  bool new_data_;              ///< New position parsed since last hasNewData() call
  uint32_t last_read_ms_;      ///< Timestamp of last read

  // ========================================================================
  // NMEA Parsing (Private Implementation)
  // ========================================================================

  /**
   * Parse a complete NMEA sentence.
   *
   * Validates checksum, identifies sentence type (GPGGA/GPRMC),
   * and dispatches to appropriate parser.
   *
   * @param sentence Complete NMEA sentence (e.g., "$GPGGA,...")
   * @return true if sentence was parsed successfully
   */
  bool parseNMEA(const char* sentence);

  /**
   * Parse GPGGA sentence (Global Positioning System Fix Data).
   *
   * Extracts: latitude, longitude, altitude, fix quality, satellite count, HDOP
   *
   * GPGGA Fields:
   * 0: GPGGA
   * 1: UTC time (hhmmss.ss)
   * 2: Latitude (ddmm.mmmm)
   * 3: Latitude direction (N/S)
   * 4: Longitude (dddmm.mmmm)
   * 5: Longitude direction (E/W)
   * 6: Fix quality (0-8)
   * 7: Satellite count
   * 8: HDOP (horizontal dilution of precision)
   * 9: Altitude above MSL (meters)
   * 10: Altitude units (M)
   * 11: Geoid separation (meters)
   * ...
   *
   * @param fields Array of field strings from parsed NMEA
   * @return true if all required fields present and valid
   */
  bool parseGPGGA(const char* fields[]);

  /**
   * Parse GPRMC sentence (Recommended Minimum Navigation Information).
   *
   * Extracts: latitude, longitude (also speed and course for future use)
   *
   * GPRMC Fields:
   * 0: GPRMC
   * 1: UTC time (hhmmss.ss)
   * 2: Status (A=valid, V=invalid)
   * 3: Latitude (ddmm.mmmm)
   * 4: Latitude direction (N/S)
   * 5: Longitude (dddmm.mmmm)
   * 6: Longitude direction (E/W)
   * 7: Speed over ground (knots)
   * 8: Course over ground (degrees true)
   * 9: Date (ddmmyy)
   * ...
   *
   * @param fields Array of field strings from parsed NMEA
   * @return true if all required fields present and valid
   */
  bool parseGPRMC(const char* fields[]);
};

#endif  // NEO_M9N_H
