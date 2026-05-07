/**
 * Build Mode Configuration
 *
 * Controls whether firmware runs in CALIBRATION or PRODUCTION mode.
 * Set via platformio.ini build_flags or -D compile flag.
 *
 * Calibration mode: Extra serial output, detailed debug info, slower but informative
 * Production mode: Minimal output, fast, saves memory and serial bandwidth
 */

#ifndef MODE_H
#define MODE_H

// Build flag: -D CALIBRATION_MODE
#ifdef CALIBRATION_MODE
  #define IS_CALIBRATION_MODE 1
  #define IS_PRODUCTION_MODE 0
#else
  #define IS_CALIBRATION_MODE 0
  #define IS_PRODUCTION_MODE 1
#endif

// Conditional output macros
#if IS_CALIBRATION_MODE
  #define CAL_PRINTLN(x) Serial.println(x)
  #define CAL_PRINT(x) Serial.print(x)
  #define CAL_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define CAL_PRINTLN(x)
  #define CAL_PRINT(x)
  #define CAL_PRINTF(fmt, ...)
#endif

// Always output these (critical info)
#define ALWAYS_PRINTLN(x) Serial.println(x)
#define ALWAYS_PRINT(x) Serial.print(x)

// Build flag: -D SNAPSHOT_MODE
// Enables snapshot recording feature (quaternion + timestamp to SD card)
#ifdef SNAPSHOT_MODE
  #define ENABLE_SNAPSHOT_RECORDER 1
  #define SNAPSHOT_BUFFER_SIZE 1024  // bytes for JSON buffer
  #define MAX_SNAPSHOT_FILES 100     // maximum files before rotating
  #define SNAPSHOT_DIRECTORY "/snapshots/"
#else
  #define ENABLE_SNAPSHOT_RECORDER 0
#endif

#endif // MODE_H
