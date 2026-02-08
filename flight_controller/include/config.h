/*
 * Flight Controller Configuration File
 * All user-configurable settings in one place
 */

#ifndef CONFIG_H
#define CONFIG_H

//=============================================================================
// CALIBRATION MODE
//=============================================================================
// Calibration is triggered via CH6 switch in calibration builds:
//   pio run -e teensy40_calibration --target upload
//
// CH6 positions (hold 3 seconds to trigger):
//   LOW  (<1200us):  Normal flight mode
//   MID  (1200-1800us): IMU offset calibration
//   HIGH (>1800us): IMU calibration + orientation detection
//
// Radio calibration: TBD (serial command trigger)
//

//=============================================================================
// FEATURE MODULES (compile-time)
//=============================================================================
// Enable/disable optional features. Each uses #ifdef — disabled features
// add zero binary cost. Use tools/timing_calculator.py to check if your
// MCU can handle the enabled features at your target loop rate.
//
// Platform notes:
//   - WiFi features require ESP32/S3 (ignored on Teensy)
//   - OLED display works on all platforms (uses SW I2C, separate from IMU bus)
//   - Optimization and Racing work on all platforms
//
// Current overhead estimates (see findings/bare-bones-fc-research.md):
//   Base FC loop:    ~150-200us on ESP32, ~50-80us on Teensy
//   Optimization:    +50-70 FP multiplications/tick (~5-10us on ESP32)
//   Racing:          +30-40 FP multiplications/tick (~3-5us on ESP32)
//   Web server:      Core 1 only (zero flight loop impact on ESP32)
//   API server:      Core 1 only (zero flight loop impact on ESP32)
//   OLED display:    Core 1 on ESP32 (zero FC impact), 10Hz in main loop on Teensy

// WiFi sub-features (ESP32/S3 only)
// Auto-enabled when USE_WIFI is set (platformio.ini for ESP32 builds).
// Comment out individually to disable a feature while keeping WiFi connectivity.
//
// WEB_SERVER: Live value display in browser. JSON API at /api/status,
//   WebSocket at /ws, mDNS at floppi-XXXX.local.
//   Best for: calibration, bench testing, diagnostics.
//
// API_SERVER: HTTP POST telemetry to centralized server for remote control.
//   Server URL set in wifi_credentials.h (API_SERVER_URL).
//   Best for: live flight, swarm coordination, external flight computer.
#if defined(USE_ESP32) && defined(USE_WIFI)
    #define USE_WEB_SERVER      // Web status server (JSON API, WebSocket, mDNS)
    #define USE_API_SERVER      // API client (POST to centralized servers)
#endif

// Optimization — Noise reduction for cheaper hardware.
// Biquad gyro/D-term filters, gyro notch filter, accel second-stage LP.
// Enable if using budget motors, unbalanced props, or flexible frames.
// Parameters below in "OPTIMIZATION PARAMETERS" section.
//#define USE_OPTIMIZATION

// Racing — Betaflight-inspired features for aggressive/FPV flying.
// Feed-forward, TPA, setpoint smoothing, air mode, expo curves.
// Not for beginners. Parameters below in "RACING PARAMETERS" section.
//#define USE_RACING

//=============================================================================
// OLED DISPLAY SELECTION
//=============================================================================
// Uncomment ONLY ONE display type (used when OLED display is enabled)
// Add new displays here as needed — only the define changes, drawing code is the same.
#define DISPLAY_SSD1306_128X32       // 0.91" OLED (DSD TECH, most common small OLED)
//#define DISPLAY_SSD1306_128X64     // 0.96" OLED (standard size)
//#define DISPLAY_SH1106_128X64      // 1.3" OLED (larger, same resolution as 0.96")

//=============================================================================
// IMU SENSOR SELECTION
//=============================================================================
// Uncomment ONLY ONE IMU type
#define USE_MPU6050       // MPU6050 via I2C (Default - most common)
//#define USE_MPU9250     // MPU9250 via SPI (has magnetometer)

//=============================================================================
// RECEIVER PROTOCOL SELECTION
//=============================================================================
// Uncomment ONLY ONE receiver protocol
//#define USE_PWM_RECEIVER   // Individual PWM channels
//#define USE_PPM_RECEIVER   // PPM (single wire, 8 channels)
#define USE_SBUS_RECEIVER    // SBUS (Futaba/FrSky standard)
//#define USE_DSM_RECEIVER   // DSM/DSM2/DSMX (Spektrum)

#ifdef USE_DSM_RECEIVER
    #define DSM_NUM_CHANNELS 6  // Number of channels from DSM receiver
#endif

//=============================================================================
// IMU CONFIGURATION FOR ELECTRONICCATS MPU6050 LIBRARY
//=============================================================================
#ifdef USE_MPU6050
    // Gyro full scale range (degrees/second)
    //#define GYRO_250DPS
    //#define GYRO_500DPS
    #define GYRO_1000DPS  // Default - good balance
    //#define GYRO_2000DPS

    // Accelerometer full scale range (G's)
    #define ACCEL_2G      // Default - best resolution
    //#define ACCEL_4G
    //#define ACCEL_8G
    //#define ACCEL_16G
    
    // MPU6050 constants from ElectronicCats library
    // These match MPU6050.h in your lib/MPU6050/src/ folder
    #ifdef GYRO_250DPS
        #define GYRO_SCALE 0  // MPU6050_GYRO_FS_250
        #define GYRO_SCALE_FACTOR 131.0
    #elif defined(GYRO_500DPS)
        #define GYRO_SCALE 1  // MPU6050_GYRO_FS_500
        #define GYRO_SCALE_FACTOR 65.5
    #elif defined(GYRO_1000DPS)
        #define GYRO_SCALE 2  // MPU6050_GYRO_FS_1000
        #define GYRO_SCALE_FACTOR 32.8
    #elif defined(GYRO_2000DPS)
        #define GYRO_SCALE 3  // MPU6050_GYRO_FS_2000
        #define GYRO_SCALE_FACTOR 16.4
    #else
        #define GYRO_SCALE 2  // Default: 1000DPS
        #define GYRO_SCALE_FACTOR 32.8
    #endif

    #ifdef ACCEL_2G
        #define ACCEL_SCALE 0  // MPU6050_ACCEL_FS_2
        #define ACCEL_SCALE_FACTOR 16384.0
    #elif defined(ACCEL_4G)
        #define ACCEL_SCALE 1  // MPU6050_ACCEL_FS_4
        #define ACCEL_SCALE_FACTOR 8192.0
    #elif defined(ACCEL_8G)
        #define ACCEL_SCALE 2  // MPU6050_ACCEL_FS_8
        #define ACCEL_SCALE_FACTOR 4096.0
    #elif defined(ACCEL_16G)
        #define ACCEL_SCALE 3  // MPU6050_ACCEL_FS_16
        #define ACCEL_SCALE_FACTOR 2048.0
    #else
        #define ACCEL_SCALE 0  // Default: 2G
        #define ACCEL_SCALE_FACTOR 16384.0
    #endif
#endif

#ifdef USE_MPU9250
    // MPU9250 configuration would go here
    #define GYRO_SCALE 2
    #define GYRO_SCALE_FACTOR 32.8
    #define ACCEL_SCALE 0
    #define ACCEL_SCALE_FACTOR 16384.0
#endif

//=============================================================================
// IMU Calibration Values (Generated by calibration program)
//=============================================================================
// Run calibration program to get these values
// Use 'i' for single-position (offsets only)
// Use 'm' for 6-position (offsets + scale factors, more accurate)
//
// Default values (replace with your calibrated values):

// Accelerometer offsets (from single or 6-position calibration)
#define IMU_ACC_ERROR_X 0.0f
#define IMU_ACC_ERROR_Y 0.0f
#define IMU_ACC_ERROR_Z 0.0f

// Accelerometer scale factors (from 6-position calibration only)
// Set to 1.0 if using single-position calibration
#define IMU_ACC_SCALE_X 1.0f
#define IMU_ACC_SCALE_Y 1.0f
#define IMU_ACC_SCALE_Z 1.0f

// Gyroscope offsets
#define IMU_GYRO_ERROR_X 0.0f
#define IMU_GYRO_ERROR_Y 0.0f
#define IMU_GYRO_ERROR_Z 0.0f

//=============================================================================
// Radio Channel Mapping (Auto-detected by calibration)
//=============================================================================
// Default channel mapping (Mode 2 - most common)
// Replace with values from radio calibration program
#define THROTTLE_CHANNEL 3  // Left stick up/down
#define ROLL_CHANNEL 1      // Right stick left/right
#define PITCH_CHANNEL 2     // Right stick up/down
#define YAW_CHANNEL 4       // Left stick left/right
#define AUX1_CHANNEL 5      // Switch (throttle cut/arm)
#define AUX2_CHANNEL 6      // Switch (aux function)

//=============================================================================
// Failsafe Values (Radio signal loss)
//=============================================================================
#define FAILSAFE_THROTTLE 1000  // Minimum throttle
#define FAILSAFE_ROLL 1500      // Center
#define FAILSAFE_PITCH 1500     // Center
#define FAILSAFE_YAW 1500       // Center
#define FAILSAFE_AUX1 2000      // Throttle cut ON (high = disarm)
#define FAILSAFE_AUX2 2000      // Default high

//=============================================================================
// Filter Coefficients (0.0 to 1.0)
//=============================================================================
// Lower = more filtering (smoother, slower)
// Higher = less filtering (faster, noisier)
#define MADGWICK_BETA 0.04  // Attitude filter (0.02-0.08 typical)
#define B_ACCEL 0.14        // Accelerometer LP filter
#define B_GYRO 0.10         // Gyroscope LP filter
#define B_MAG 1.0           // Magnetometer LP filter (MPU9250 only)

//=============================================================================
// Magnetometer Calibration (MPU9250 only)
//=============================================================================
#ifdef USE_MPU9250
    #define MAG_ERROR_X 0.0f
    #define MAG_ERROR_Y 0.0f
    #define MAG_ERROR_Z 0.0f
    #define MAG_SCALE_X 1.0f
    #define MAG_SCALE_Y 1.0f
    #define MAG_SCALE_Z 1.0f
#endif

//=============================================================================
// Control Mode Selection
//=============================================================================
// Uncomment ONLY ONE control mode. This is a compile-time selection — only
// the chosen controller is included in the binary (zero overhead).
//
// ANGLE MODE: Stick position = target tilt angle. Release stick → drone levels
//   itself. Best for: hovering, filming, beginners, autonomous flight.
//
// RATE MODE:  Stick position = rotation speed. Release stick → drone holds
//   current angle (no self-leveling). Best for: acrobatics, FPV racing.
//
// For in-flight switching, use a flight computer that sends pre-computed
// commands — the FC just executes. Keep it simple on the microcontroller.
//
//#define USE_RATE_CONTROLLER   // Rate mode (acro)
#define USE_ANGLE_CONTROLLER    // Angle mode (stabilize) — recommended default

//=============================================================================
// Maximum Control Limits
//=============================================================================
#ifdef USE_RATE_CONTROLLER
    // Rate mode: degrees per second
    #define MAX_ROLL_RATE 200.0f   // Maximum roll rate (deg/s)
    #define MAX_PITCH_RATE 200.0f  // Maximum pitch rate (deg/s)
    #define MAX_YAW_RATE 160.0f    // Maximum yaw rate (deg/s)
#else
    // Angle mode: degrees
    #define MAX_ROLL_ANGLE 30.0f   // Maximum roll angle (degrees)
    #define MAX_PITCH_ANGLE 30.0f  // Maximum pitch angle (degrees)
    #define MAX_YAW_RATE 160.0f    // Yaw still uses rate control
#endif

//=============================================================================
// PID Controller Gains - RATE MODE
//=============================================================================
// Start with these conservative values, tune incrementally
// See PID_TUNING_GUIDE.md for tuning instructions

// Roll Rate PID
#define KP_ROLL_RATE 0.15f
#define KI_ROLL_RATE 0.15f
#define KD_ROLL_RATE 0.0004f
#define I_LIMIT_ROLL 25.0f

// Pitch Rate PID
#define KP_PITCH_RATE 0.15f
#define KI_PITCH_RATE 0.15f
#define KD_PITCH_RATE 0.0004f
#define I_LIMIT_PITCH 25.0f

// Yaw Rate PID
#define KP_YAW_RATE 0.30f
#define KI_YAW_RATE 0.05f
#define KD_YAW_RATE 0.00015f
#define I_LIMIT_YAW 25.0f

//=============================================================================
// PID Controller Gains - ANGLE MODE
//=============================================================================
// Roll Angle PID
#define KP_ROLL_ANGLE 0.20f
#define KI_ROLL_ANGLE 0.00f
#define KD_ROLL_ANGLE 0.05f

// Pitch Angle PID
#define KP_PITCH_ANGLE 0.20f
#define KI_PITCH_ANGLE 0.00f
#define KD_PITCH_ANGLE 0.05f

// Note: Yaw uses rate control even in angle mode

//=============================================================================
// Motor Protocol Selection
//=============================================================================
//#define USE_ONESHOT125    // OneShot125 ESC protocol (125-250μs)
#define USE_STANDARD_PWM    // Standard PWM (1000-2000μs)

//=============================================================================
// Loop Rate
//=============================================================================
// Can be overridden by platformio.ini build flags (-D LOOP_FREQUENCY_HZ=1000)
#ifndef LOOP_FREQUENCY_HZ
    #ifdef USE_ESP32
        #define LOOP_FREQUENCY_HZ 1000  // ESP32: 1kHz (sufficient, more headroom)
    #else
        #define LOOP_FREQUENCY_HZ 2000  // Teensy: 2kHz (faster processor)
    #endif
#endif

//=============================================================================
// OPTIMIZATION PARAMETERS (only when USE_OPTIMIZATION is enabled)
//=============================================================================
#ifdef USE_OPTIMIZATION
    // Gyro biquad low-pass filter cutoff (Hz)
    // Steeper rolloff than base PT1 filter (-12dB/octave vs -6dB/octave).
    // Lower = more filtering, more delay. 80-150Hz typical.
    #define GYRO_LPF_CUTOFF_HZ 100

    // D-term biquad low-pass filter cutoff (Hz)
    // Usually lower than gyro cutoff. 60-120Hz typical.
    #define DTERM_LPF_CUTOFF_HZ 80

    // Gyro notch filter — narrow-band rejection at a specific frequency.
    // Targets motor/prop resonance noise. Set center to 0 to disable.
    // Find your noise frequency with fc_tool FFT or Betaflight blackbox viewer.
    #define GYRO_NOTCH_CENTER_HZ 0      // 0 = disabled
    #define GYRO_NOTCH_WIDTH_HZ 30      // Bandwidth around center

    // Accelerometer second-stage low-pass filter coefficient (0.0-1.0)
    // Extra smoothing on accel for attitude estimation under vibration.
    #define B_ACCEL_STAGE2 0.05
#endif

//=============================================================================
// RACING PARAMETERS (only when USE_RACING is enabled)
//=============================================================================
#ifdef USE_RACING
    // Feed-forward gain — adds setpoint derivative to PID output.
    // Improves stick response without increasing P gain.
    // 0.0 = disabled, 0.5-2.0 typical for aggressive flying.
    #define FF_ROLL 0.0f
    #define FF_PITCH 0.0f
    #define FF_YAW 0.0f

    // TPA (Throttle PID Attenuation)
    // Reduces PID gains at high throttle to prevent oscillation.
    // Breakpoint: throttle level where attenuation starts (0.0-1.0, 0.65 typical).
    // Rate: how much to reduce (0.0 = none, 1.0 = full attenuation at max throttle).
    #define TPA_BREAKPOINT 0.65f
    #define TPA_RATE 0.5f

    // Setpoint smoothing — low-pass on stick input for smoother transitions.
    // 0 = disabled. 20-80Hz typical.
    #define SETPOINT_SMOOTH_CUTOFF_HZ 0

    // Air mode — keeps PID active at zero throttle for full attitude control
    // during flips, rolls, and inverted flight. Uncomment to enable.
    //#define USE_AIRMODE

    // Expo curves — non-linear stick response.
    // 0.0 = linear, 0.5 = moderate curve, 0.8 = aggressive.
    // Gentle near center, aggressive at extremes.
    #define EXPO_ROLL 0.0f
    #define EXPO_PITCH 0.0f
    #define EXPO_YAW 0.0f
#endif

//=============================================================================
// Debug Options
//=============================================================================
// Serial baud rate
#define SERIAL_BAUD 115200

// Debug print rate limiting (microseconds between prints)
#define DEBUG_PRINT_INTERVAL 10000  // 10ms = 100Hz print rate

#endif // CONFIG_H