/**
 * GPS Configuration Header
 *
 * Controls GPS feature compilation via platformio.ini build_flags.
 * When GPS_ENABLE is not set, provides no-op stubs and disables all GPS code.
 *
 * Build Flag Examples:
 *   -D GPS_ENABLE                  (enable GPS support)
 *   -D GPS_UART_PORT=0             (use Serial1)
 *   -D GPS_BAUD=9600               (baud rate)
 *
 * See platformio.ini [env:arduino_mega_gps*] sections for examples.
 */

#ifndef GPS_CONFIG_H
#define GPS_CONFIG_H

/**
 * GPS_ENABLE: Master switch for GPS support
 *
 * Set via platformio.ini build_flags: -D GPS_ENABLE
 * When not defined, GPS code is compiled to no-ops and zero overhead.
 */
#ifdef GPS_ENABLE
  #define GPS_AVAILABLE 1
#else
  #define GPS_AVAILABLE 0
#endif

// ============================================================================
// GPS UART Configuration (only active when GPS_ENABLE is set)
// ============================================================================

#if GPS_AVAILABLE

  /**
   * GPS_UART_PORT: Which hardware UART to use for GPS communication
   *
   * Arduino Mega UARTs:
   *   0 = Serial1 (RX1=19, TX1=18) - pins 18/19
   *   1 = Serial2 (RX2=17, TX2=16) - pins 16/17
   *   2 = Serial3 (RX3=15, TX3=14) - pins 14/15
   *
   * Default: 0 (Serial1)
   * Configure in platformio.ini: -D GPS_UART_PORT=0
   */
  #ifndef GPS_UART_PORT
    #define GPS_UART_PORT 0
  #endif

  /**
   * GPS_BAUD: UART baud rate for GPS module
   *
   * Common baud rates:
   *   9600   - Standard for most GPS modules (NEO-M8, M9N default)
   *   115200 - High-speed variant, some M9N/M10S can run at this
   *
   * Default: 9600
   * Configure in platformio.ini: -D GPS_BAUD=115200
   */
  #ifndef GPS_BAUD
    #define GPS_BAUD 9600
  #endif

  /**
   * GPS_READ_TIMEOUT_MS: Maximum time to wait for a complete NMEA sentence
   *
   * If no complete sentence arrives within this time, reading returns false.
   * Typical NMEA sentence is ~80 bytes at 9600 baud = ~80ms.
   * Default: 100ms (safe margin)
   */
  #define GPS_READ_TIMEOUT_MS 100

  /**
   * GPS_STALE_TIMEOUT_MS: Time before GPS data is marked as stale
   *
   * If no new position update for > this time, position marked stale.
   * Typical GPS update rate is 1 Hz = 1000ms.
   * Default: 1000ms (one missed update)
   */
  #define GPS_STALE_TIMEOUT_MS 1000

  /**
   * GPS_SENTENCE_BUFFER_SIZE: Maximum NMEA sentence length
   *
   * Standard NMEA sentences are ~80-100 bytes.
   * Allows room for extended sentences with many fields.
   */
  #define GPS_SENTENCE_BUFFER_SIZE 128

#else  // GPS_AVAILABLE is 0

  // No-op stubs when GPS is disabled
  #define GPS_UART_PORT 0
  #define GPS_BAUD 9600
  #define GPS_READ_TIMEOUT_MS 0
  #define GPS_STALE_TIMEOUT_MS 0
  #define GPS_SENTENCE_BUFFER_SIZE 0

#endif  // GPS_AVAILABLE

// ============================================================================
// GPS Compile-time Constants (derived from above)
// ============================================================================

#if GPS_AVAILABLE
  /**
   * GPS_SERIAL: Which HardwareSerial object to use
   *
   * Macros for different boards:
   * - Arduino Mega: Serial1, Serial2, Serial3
   * - Arduino Nano: Only Serial1 (via software serial fallback)
   */
  #if GPS_UART_PORT == 0
    #define GPS_SERIAL Serial1
    #define GPS_RX_PIN 19
    #define GPS_TX_PIN 18
  #elif GPS_UART_PORT == 1
    #define GPS_SERIAL Serial2
    #define GPS_RX_PIN 17
    #define GPS_TX_PIN 16
  #elif GPS_UART_PORT == 2
    #define GPS_SERIAL Serial3
    #define GPS_RX_PIN 15
    #define GPS_TX_PIN 14
  #else
    #error "GPS_UART_PORT must be 0, 1, or 2"
  #endif

#endif  // GPS_AVAILABLE

#endif  // GPS_CONFIG_H
