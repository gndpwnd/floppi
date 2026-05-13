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

// ============================================================================
// Phase 4+ framework flags
// ============================================================================
//
// All optional. Each module is implemented in its own subtree and is only
// linked in when its flag is set.

// IMU drivers — at most one should be selected for a given build.
//   USE_BNO085  : Phase 1-3 driver (default if neither flag is defined)
//   USE_BNO055  : Phase 4.6 driver (alternative IMU)
//   USE_MPU6050 : Phase 5.5 raw-IMU + external magnetometer + Madgwick fusion

#if !defined(USE_BNO085) && !defined(USE_BNO055) && !defined(USE_MPU6050)
  #define USE_BNO085  // default IMU for legacy builds
#endif

// Auto-PID-tuner strategies — selected per build to save flash on AVR.
//   USE_TUNER_RELAY   : Åström-Hägglund relay feedback (default for pendulums)
//   USE_TUNER_TWIDDLE : Twiddle / coordinate descent (generic fallback)
//   USE_TUNER_RLS     : Recursive least-squares + analytical PID (drones, Phase 7)

// Application gates — exactly one application is active per binary.
// The framework is the unifying layer; applications compose its sensors,
// math, navigation, control, and storage modules to serve a specific use case.
//
//   USE_IMU_GPS_TELEMETRY : Phase 1-3 reference app — BNO085 + GPS + EKF fusion
//                          streaming JSON over serial. Default when no other
//                          application flag is set.
//   USE_BALANCING_ROBOT   : Phase 4.7 self-balancing robot (BNO055 + L298N
//                          + auto-PID-tune). See docs/applications/balancing_robot/
//   USE_MULTIROTOR_BRIDGE : Phase 7 I2C slave to flight_controller
//   USE_CAMERA_MOUNT      : Phase 7 gimbal
//   USE_PHOTOGRAMMETRY    : Phase 7 snapshot rig + GPS metadata
//   USE_EDU_KIT           : Phase 7 educational minimum-viable Nano+MPU6050 build

#if !defined(USE_BALANCING_ROBOT) && \
    !defined(USE_IMU_GPS_TELEMETRY) && \
    !defined(USE_MULTIROTOR_BRIDGE) && \
    !defined(USE_CAMERA_MOUNT) && \
    !defined(USE_PHOTOGRAMMETRY) && \
    !defined(USE_EDU_KIT)
  // Backwards compatibility: existing build envs (arduino_mega, esp32dev, ...)
  // without an explicit application flag get the IMU+GPS telemetry app.
  #define USE_IMU_GPS_TELEMETRY
#endif

// WiFi cascade — mirrors flight_controller/include/config.h pattern.
//   USE_WIFI            : enables WiFi STA + mDNS
//   USE_WEB_SERVER      : (auto-cascades from USE_WIFI) browser dashboard
//   USE_API_SERVER      : (auto-cascades from USE_WIFI) REST + WebSocket
//   USE_OTA             : (auto-cascades from USE_WIFI) ArduinoOTA + HTTP-pull

#if defined(ARDUINO_ARCH_ESP32)
  #define HAS_WIFI_CAPABLE 1
#endif

#if defined(HAS_WIFI_CAPABLE) && defined(USE_WIFI)
  #ifndef USE_WEB_SERVER
    #define USE_WEB_SERVER
  #endif
  #ifndef USE_API_SERVER
    #define USE_API_SERVER
  #endif
  #ifndef USE_OTA
    #define USE_OTA
  #endif
#endif

#endif // MODE_H
