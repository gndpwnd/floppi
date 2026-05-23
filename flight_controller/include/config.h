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
// Radio calibration: serial command 'r' in calibration builds
//

//=============================================================================
// FEATURE MODULES (compile-time)
//=============================================================================
// Enable/disable optional features. Each uses #ifdef — disabled features
// add zero binary cost. Use tools/complexity_calculator.py to check if your
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
    #define USE_OTA             // Over-the-air firmware updates (ArduinoOTA)

    //=========================================================================
    // WIFI AUTH MODE  (ESP32/S3 only — uncomment exactly ONE)
    //=========================================================================
    // Compile-time link-layer auth selection. Only the chosen mode's code is
    // compiled; unused modes cost zero flash/RAM. Validated by a #error block
    // in wifi_config.h. Secrets/addresses live in wifi_credentials.h (NOT here)
    // so config.h stays secret-free. STA mode only.
    //
    //   OPEN       — no password (WiFi.begin(ssid))
    //   PSK        — WPA/WPA2-Personal (DEFAULT, byte-identical to legacy path)
    //   WPA3_SAE   — WPA3-Personal (forces SAE; refuses WPA2-only APs)
    //   ENTERPRISE — WPA2/WPA3-Enterprise EAP (the heavy mode: ~20-40 KB extra)
    //
    // NOTE on naming: the selector tokens use the WIFI_AUTH_MODE_* prefix (NOT
    // bare WIFI_AUTH_OPEN/WIFI_AUTH_ENTERPRISE) because the ESP-IDF defines an
    // enum wifi_auth_mode_t whose members ARE named WIFI_AUTH_OPEN /
    // WIFI_AUTH_ENTERPRISE / WIFI_AUTH_WPA3_PSK ...; defining those as macros
    // corrupts <esp_wifi_types.h>. The WIFI_AUTH_MODE_* prefix is collision-free.
    //
    // (A build flag -DWIFI_AUTH_OVERRIDE lets CI/test builds pick a different
    //  mode via -D without editing this default. Normal users ignore it.)
    #ifndef WIFI_AUTH_OVERRIDE
    #define WIFI_AUTH_MODE_PSK            // WPA/WPA2-Personal (default)
    //#define WIFI_AUTH_MODE_OPEN         // no password
    //#define WIFI_AUTH_MODE_WPA3_SAE     // WPA3-Personal
    //#define WIFI_AUTH_MODE_ENTERPRISE   // WPA2/WPA3-Enterprise (EAP)

    // --- Enterprise EAP method (only used under WIFI_AUTH_MODE_ENTERPRISE; pick ONE) ---
    #define WIFI_EAP_METHOD_PEAP     // username/password, optional CA
    //#define WIFI_EAP_METHOD_TTLS   // username/password, optional CA
    //#define WIFI_EAP_METHOD_TLS    // client cert+key (requires USE_WIFI_CERTS)
    #endif // WIFI_AUTH_OVERRIDE

    // --- Certificates (optional; CA validates the RADIUS server, client
    //     cert+key provide EAP-TLS mutual auth). Blobs live in wifi_certs.h. ---
    //#define USE_WIFI_CERTS

    // --- Network addressing (optional; DHCP is the default) ---
    //#define USE_STATIC_IP          // static address (IP/gateway/subnet/DNS in wifi_credentials.h)
    //#define WIFI_HOSTNAME "floppi-fc"   // DHCP hostname; omit -> derived from MAC (floppi-XXXX)
#endif

// API_AUTH — shared-token authentication on the WiFi COMMAND surface.
// Security audit 2026-05-22 SEC-01/03: POST /api/commands and the WebSocket
// /ws can arm and throttle the aircraft, and there is NO sender authentication.
// When USE_API_AUTH is enabled, every command-bearing request/WS message must
// carry the shared token defined as FLOPPI_CMD_TOKEN in wifi_credentials.h;
// mismatched/absent tokens are rejected (HTTP 401 / ignored WS frame).
//
// DEFAULT OFF for backward-compatibility: the existing swarm_api contract does
// NOT send a token yet (cross-project handoff:
// docs/handoffs/api_auth_contract_2026-05-22.md). Turning this ON before the
// ground station is updated WILL break command delivery. Telemetry
// (GET /api/status, /ws broadcast, GET /) is unaffected — read paths stay open.
//
//#define USE_API_AUTH

// Loud reminder: WiFi command surface is open when API auth is off. The SEC-01
// #warning is emitted from a single translation unit (src/web_server.cpp) so it
// fires exactly ONCE per build instead of once per TU that includes config.h
// (~14× per ESP32 build). The condition is identical to the build-gate that
// compiles web_server.cpp, so the reminder appears under exactly the same
// circumstances. It is a #warning (not #error) so default builds compile.

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
//#define DISPLAY_SH1106_128X64      // 1.3" OLED (HiLetGo, larger, same resolution as 0.96")

//=============================================================================
// IMU SENSOR SELECTION
//=============================================================================
// Uncomment ONLY ONE IMU type
#define USE_MPU6050       // MPU6050 via I2C (Default - most common)
//#define USE_MPU9250     // MPU9250 via SPI (has magnetometer)

// ---- BNO055 / BNO085 scaffolding (Phase A — 2026-05-20) ----
//
// These flags add I2C-detection scaffolding for the BNO055 and BNO085 sensors,
// but do NOT activate any of their code paths. See
// /home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md §5 for the
// phased integration plan. Phase B/B.5/C are deferred to future sessions.
//
// Leave OFF by default. Enable to see "BNOxxx detected on I2C" boot messages.
//
//#define USE_BNO055                // BNO055 9DOF (chip-fused, 100 Hz)
//#define USE_BNO085                // BNO085 9DOF (SH-2 protocol, hardware-fused)

//=============================================================================
// BAROMETER (optional, telemetry-only — ESP32 Core 1)
//=============================================================================
// Telemetry-only barometric pressure / temperature / relative-altitude sensor.
// It is polled by a DEDICATED Core-1 FreeRTOS task and exposed via the swarm
// API. It NEVER feeds the Core-0 flight loop — altitude logic belongs on the
// external flight computer (see scope.md and
// docs/findings/barometer_integration_spec_2026-05-20.md). There is, by design,
// no altitude-hold / vertical-rate path: USE_BAROMETER is telemetry, period.
//
// Default OFF. When undefined, the barometer module + task + telemetry field
// compile to zero bytes (#ifdef-gated everywhere, like other optional features).
//
//#define USE_BAROMETER

#ifdef USE_BAROMETER
    // Select ONE sensor type. BMP280 is the implemented default — cheapest,
    // most common, adequate for a telemetry-grade vertical readout. BMP388 /
    // MS5611 now have full drivers; pick one here or override via a build flag.
    // Override (no edit needed): pass -DBAROMETER_BMP388 or -DBAROMETER_MS5611
    // on the command line / platformio.ini; the guard below then skips the
    // BMP280 default so exactly one selector is active.
    #if !defined(BAROMETER_BMP280) && !defined(BAROMETER_BMP388) && !defined(BAROMETER_MS5611)
    #define BAROMETER_BMP280       // default — cheapest, most common
    #endif
    //#define BAROMETER_BMP388     // lower noise, FIFO
    //#define BAROMETER_MS5611     // best resolution

    // I2C address — most BMP280 breakouts default to 0x76 (SDO=GND);
    // 0x77 if SDO is pulled high. The barometer shares the primary `Wire`
    // bus with the IMU (MPU6050 @ 0x68) — no address clash, they coexist.
    // Using `Wire` (GPIO 21/22) avoids contradiction C-1: it touches NO motor
    // pin (unlike Wire1's GPIO 25/26 = MOTOR_PIN_1/2). See the W2 landing report.
    #ifndef BARO_I2C_ADDRESS
        #define BARO_I2C_ADDRESS 0x76
    #endif

    // Sea-level reference pressure (Pa) for the relative-altitude calculation.
    // ISA standard placeholder; replace with the value printed by the 'b'
    // calibration routine for an accurate AGL readout.
    #ifndef BARO_SEA_LEVEL_PA
        #define BARO_SEA_LEVEL_PA 101325.0f   // ISA standard — placeholder
    #endif

    // Core-1 task poll rate (Hz). Telemetry-grade; 10-25 Hz is plenty.
    #ifndef BARO_SAMPLE_RATE_HZ
        #define BARO_SAMPLE_RATE_HZ 20
    #endif

    // Output PT1 low-pass coefficient (0.0-1.0), same convention as B_ACCEL /
    // B_GYRO. Lower = smoother (slower) altitude readout.
    #ifndef BARO_LPF
        #define BARO_LPF 0.20f
    #endif
#endif

//=============================================================================
// GPS PASSTHROUGH (optional, telemetry-only — ESP32 Core 1, RX-only UART)
//=============================================================================
// PASSTHROUGH ONLY. Raw NMEA bytes are read on a DEDICATED Core-1 FreeRTOS task
// and the most-recent complete sentence is exposed verbatim via the swarm API
// as a `gps` field. The FC parses NOTHING — no fix decoding, no fusion, no
// waypoints, no return-to-home, no geo-fence, no navigation. The Core-0 1 kHz
// flight loop NEVER reads GPS data; GPS is bytes for an external flight
// computer (see scope.md "GPS … flight computer territory" and
// docs/findings/gps_passthrough_spec_2026-05-20.md — Flavour A).
//
// SECURITY NOTE: raw NMEA contains absolute latitude/longitude. The swarm API
// is unauthenticated — on an open network this `gps` field is a location leak.
// Run the FC on an isolated SSID only (session3_readiness_2026-05-20.md §6).
//
// ESP32 / ESP32-S3 only — passthrough exists to serve the WiFi telemetry
// surface, which Teensy lacks.
//
// Default OFF. When undefined, the GPS module + task + telemetry field compile
// to zero bytes (#ifdef-gated everywhere, like other optional features).
//
//#define USE_GPS

#ifdef USE_GPS
    // UART selection (1 or 2). UART1 is the default — UART0 is the USB debug
    // console and UART2 is reserved for SBUS in the default receiver build.
    // Do NOT enable USE_GPS together with USE_IBUS_RECEIVER / USE_DSM_RECEIVER /
    // USE_SERIAL_COMMANDS — they share UART1; a compile-time #error in gps.h
    // enforces this.
    #ifndef GPS_UART_NUM
        #define GPS_UART_NUM 1
    #endif

    // GPS module baud rate. NEO-6M factory default: 9600. NEO-M8N / NEO-M9N
    // factory default once configured: 38400. The driver is sensor-agnostic —
    // it just reads NMEA.
    #ifndef GPS_UART_BAUD
        #define GPS_UART_BAUD 9600
    #endif

    // Per-poll byte budget drained from the UART RX FIFO. One NMEA sentence is
    // ~82 bytes max; 256 covers a few queued sentences plus one in-progress
    // framing without letting a byte-storm monopolise the Core-1 task.
    #ifndef GPS_BUFFER_BYTES
        #define GPS_BUFFER_BYTES 256
    #endif

    // Liveness window. If no complete sentence arrives in this many ms, the
    // telemetry `gps.ok` field reads false (GPS missing / unpowered / no link).
    #ifndef GPS_STALE_TIMEOUT_MS
        #define GPS_STALE_TIMEOUT_MS 2000
    #endif

    // UART RX pin (RX-only — passthrough sends nothing to the module, so the TX
    // pin is the Arduino "unused" sentinel -1 and claims no GPIO). These mirror
    // the existing UART1 RX defaults in pin_definitions_esp32.h: GPIO 4 on the
    // ESP32, GPIO 16 on the ESP32-S3. They live here (in config.h, before the
    // pin-definition headers) rather than in pin_definitions_esp32.h because the
    // GPS driver was landed without its pin defaults; #ifndef-guarded so a build
    // flag or a future pin_definitions_esp32.h entry still wins.
    //
    // GPIO-CONFLICT NOTE (SBUS priority on GPIO 16): with GPS_UART_NUM 1 (the
    // default) the GPS is on UART1 RX and never touches GPIO 16 on the ESP32 —
    // SBUS keeps GPIO 16 (UART2) uncontested, so SBUS + GPS coexist with no
    // override. On the ESP32-S3, UART1 RX *is* GPIO 16, but the S3 SBUS RX
    // default is GPIO 18 (pin_definitions_esp32.h:167), so there is still no
    // collision. The only real contention is GPS vs the UART1 *serial*
    // receivers (iBUS / DSM / serial-commands), which the #error guard in gps.h
    // catches. See docs/findings/gps_passthrough_spec_2026-05-20.md §3.
    #ifndef GPS_PIN_RX
        #ifdef USE_ESP32S3
            #define GPS_PIN_RX 16   // S3 UART1 RX
        #else
            #define GPS_PIN_RX 4    // ESP32 UART1 RX
        #endif
    #endif
    #ifndef GPS_PIN_TX
        #define GPS_PIN_TX -1       // unused — passthrough is RX-only
    #endif
#endif

//=============================================================================
// RECEIVER / COMMAND SOURCE SELECTION
//=============================================================================
// Uncomment ONE RC receiver protocol for manual flying:
//#define USE_PWM_RECEIVER       // Individual PWM channels
//#define USE_PPM_RECEIVER       // PPM (single wire, 8 channels)
#define USE_SBUS_RECEIVER      // SBUS (Futaba/FrSky standard) — SBUS re-enabled 2026-05-20 per fc-recon@flight_controller:1
//#define USE_IBUS_RECEIVER      // iBUS (FlySky standard, 115200 baud)
//#define USE_DSM_RECEIVER       // DSM/DSM2/DSMX (Spektrum)
//
// Override command sources (can coexist with RC when arbitration is enabled):
//#define USE_SERIAL_COMMANDS    // External flight computer over UART (115200, binary)
//#define USE_I2C_COMMANDS       // External flight computer over I2C slave (Wire1)
//
// Command source arbitration — required when multiple sources are active.
// Priority: Serial > I2C > WiFi (overrides), RC (primary fallback).
// Without this flag, only one source compiles per build (current behavior).
//#define USE_COMMAND_ARBITRATION
//
// WiFi-only (ESP32): Comment out ALL receiver/command defines above.
// The web server becomes the sole command source via POST /api/commands
// and WebSocket /ws. Requires USE_WIFI + USE_WEB_SERVER. See swarm_api/.

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
// CALIBRATION STATUS MARKERS
//=============================================================================
// Uncomment each marker AFTER completing that calibration stage.
// The sequential calibration workflow ('a' command) uses these to detect
// what's been calibrated and where to resume.
//
// Reset all: python3 tools/calibration_reset.py (re-comments all markers)
// See: docs/features/calibration-guide.md for stage details.
//
//#define CALIBRATED_IMU          // Stage 1: IMU offsets calibrated ('i' or 'm')
//#define CALIBRATED_IMU_ORIENT   // Stage 1: IMU orientation detected ('o')
//#define CALIBRATED_RADIO        // Stage 2: Radio channels mapped ('r')
//#define CALIBRATED_FAILSAFE     // Stage 2: Failsafe values measured ('f')
//#define CALIBRATED_ESC          // Stage 3: ESC endpoints calibrated ('e')
//#define CALIBRATED_PID          // Stage 4: PID gains tuned ('g')
//#define CALIBRATED_FILTERS      // Stage 4: Filters/limits tuned ('p')
#ifdef USE_BAROMETER
//#define CALIBRATED_BAROMETER    // Barometer sea-level reference set ('b')
#endif
#ifdef USE_MPU9250
//#define CALIBRATED_MAG          // Stage 1: Magnetometer calibrated
#endif

//=============================================================================
// IMU Calibration Values (Generated by calibration program)
//=============================================================================
// STATUS: UNCALIBRATED — Run calibration to get your hardware's values.
//   Stage 1: 'i' for quick offsets, or 'm' for full 6-position calibration
//   See: docs/features/calibration-guide.md (Stage 1)
//   Reset all values: python3 tools/calibration_reset.py
//
// Replace these defaults with your calibrated values:

// Accelerometer offsets (from 'i' single-position or 'm' 6-position)
#define IMU_ACC_ERROR_X 0.0f    // ← calibrate with 'i' or 'm' command
#define IMU_ACC_ERROR_Y 0.0f    // ← calibrate with 'i' or 'm' command
#define IMU_ACC_ERROR_Z 0.0f    // ← calibrate with 'i' or 'm' command

// Accelerometer scale factors (from 'm' 6-position calibration only)
// Leave at 1.0 if using single-position 'i' calibration
#define IMU_ACC_SCALE_X 1.0f    // ← calibrate with 'm' command
#define IMU_ACC_SCALE_Y 1.0f    // ← calibrate with 'm' command
#define IMU_ACC_SCALE_Z 1.0f    // ← calibrate with 'm' command

// Gyroscope offsets
#define IMU_GYRO_ERROR_X 0.0f   // ← calibrate with 'i' or 'm' command
#define IMU_GYRO_ERROR_Y 0.0f   // ← calibrate with 'i' or 'm' command
#define IMU_GYRO_ERROR_Z 0.0f   // ← calibrate with 'i' or 'm' command

//=============================================================================
// Radio Channel Mapping (Auto-detected by calibration)
//=============================================================================
// STATUS: UNCALIBRATED — Run 'r' command to auto-detect your mapping.
//   Stage 2: Requires receiver + transmitter connected
//   See: docs/features/calibration-guide.md (Stage 2)
//
// Default: Mode 2 mapping (most common). Replace with calibrated values.
#define THROTTLE_CHANNEL 3  // ← calibrate with 'r' command
#define ROLL_CHANNEL 1      // ← calibrate with 'r' command
#define PITCH_CHANNEL 2     // ← calibrate with 'r' command
#define YAW_CHANNEL 4       // ← calibrate with 'r' command
#define AUX1_CHANNEL 5      // ← calibrate with 'r' command
#define AUX2_CHANNEL 6      // ← calibrate with 'r' command

//=============================================================================
// Failsafe Values (Radio signal loss)
//=============================================================================
// STATUS: UNCALIBRATED — Run 'f' command to auto-detect from your receiver.
//   Stage 2: Requires receiver + transmitter connected
//   See: docs/features/calibration-guide.md (Stage 2)
//
// These defaults are safe but may not match your receiver's actual failsafe.
#define FAILSAFE_THROTTLE 1000  // ← calibrate with 'f' command
#define FAILSAFE_ROLL 1500      // ← calibrate with 'f' command
#define FAILSAFE_PITCH 1500     // ← calibrate with 'f' command
#define FAILSAFE_YAW 1500       // ← calibrate with 'f' command
#define FAILSAFE_AUX1 2000      // ← calibrate with 'f' command
#define FAILSAFE_AUX2 2000      // ← calibrate with 'f' command

//=============================================================================
// Filter Coefficients (0.0 to 1.0)
//=============================================================================
// STATUS: Defaults are conservative and safe for most hardware.
//   Fine-tune with 'p' command during tethered flight (Stage 4).
//   See: docs/features/calibration-guide.md (Stage 4)
//
// Lower = more filtering (smoother, slower)
// Higher = less filtering (faster, noisier)
#define MADGWICK_BETA 0.04  // Attitude filter (0.02-0.08 typical)
#define B_ACCEL 0.14        // Accelerometer LP filter
#define B_GYRO 0.10         // Gyroscope LP filter
#define B_DTERM 0.15        // D-term LP filter (prevents motor oscillation from noise)
#define B_MAG 1.0           // Magnetometer LP filter (MPU9250 only)

//=============================================================================
// Magnetometer Calibration (MPU9250 only)
//=============================================================================
// STATUS: UNCALIBRATED — Run sphere calibration in calibration build.
//   Stage 1: Requires MCU + MPU9250 only
//   See: docs/features/calibration-guide.md (Stage 1d)
#ifdef USE_MPU9250
    #define MAG_ERROR_X 0.0f    // ← calibrate with sphere rotation
    #define MAG_ERROR_Y 0.0f    // ← calibrate with sphere rotation
    #define MAG_ERROR_Z 0.0f    // ← calibrate with sphere rotation
    #define MAG_SCALE_X 1.0f    // ← calibrate with sphere rotation
    #define MAG_SCALE_Y 1.0f    // ← calibrate with sphere rotation
    #define MAG_SCALE_Z 1.0f    // ← calibrate with sphere rotation
#endif

//=============================================================================
// Control Mode Selection
//=============================================================================
// Uncomment ONLY ONE control mode. This is a compile-time selection — only
// the chosen controller is included in the binary (zero overhead).
//
// ANGLE MODE: Stick position = target tilt angle. Release stick -> drone levels
//   itself. Best for: hovering, filming, beginners, autonomous flight.
//
// RATE MODE:  Stick position = rotation speed. Release stick -> drone holds
//   current angle (no self-leveling). Best for: acrobatics, FPV racing.
//
// For in-flight switching, use a flight computer that sends pre-computed
// commands — the FC just executes. Keep it simple on the microcontroller.
//
//#define USE_RATE_CONTROLLER   // Rate mode (acro)
#define USE_ANGLE_CONTROLLER    // Angle mode (stabilize) — recommended default

//=============================================================================
// ACROBATIC FLIGHT QUICK SETUP
//=============================================================================
// To enable acrobatic flight (flips, rolls, inverted hover, etc.):
//
// 1. Switch to rate mode:
//      Uncomment USE_RATE_CONTROLLER above, comment out USE_ANGLE_CONTROLLER
//
// 2. Enable racing features:
//      Uncomment USE_RACING (line 69) — adds air mode, feed-forward, expo
//
// 3. Enable air mode (CRITICAL for flips):
//      Uncomment USE_AIRMODE in RACING PARAMETERS section below
//      Keeps PID active at zero throttle during inverted portions of maneuvers
//
// 4. Set gyro range for aggressive maneuvers:
//      Change GYRO_1000DPS -> GYRO_2000DPS in IMU CONFIGURATION section
//      Prevents gyro saturation during fast rotations (>1000 deg/s)
//
// 5. Tune MAX_RATE values below for your flying style:
//      Conservative: 300 deg/s.  Standard acro: 500 deg/s.  Racing: 800 deg/s.
//
// 6. Optional: enable USE_OPTIMIZATION for noisy hardware (biquad filters)
//
// See: docs/findings/acrobatics-command-architecture.md for full details.
// The FC just tracks rate setpoints — a flight computer handles trajectory.

//=============================================================================
// Maximum Control Limits
//=============================================================================
#ifdef USE_RATE_CONTROLLER
    // Rate mode: degrees per second
    // For acrobatics (flips, rolls): 400-800 deg/s typical.
    // For gentle rate mode flying: 200-300 deg/s.
    // Must stay within gyro full-scale range (see GYRO_xxxDPS above).
    #define MAX_ROLL_RATE 500.0f   // Maximum roll rate (deg/s)
    #define MAX_PITCH_RATE 500.0f  // Maximum pitch rate (deg/s)
    #define MAX_YAW_RATE 400.0f    // Maximum yaw rate (deg/s)
#else
    // Angle mode: degrees
    #define MAX_ROLL_ANGLE 30.0f   // Maximum roll angle (degrees)
    #define MAX_PITCH_ANGLE 30.0f  // Maximum pitch angle (degrees)
    #define MAX_YAW_RATE 160.0f    // Yaw still uses rate control
#endif

//=============================================================================
// PID Controller Gains - RATE MODE
//=============================================================================
// STATUS: Defaults are conservative starting values.
//   Fine-tune with 'g' command during tethered flight (Stage 4).
//   See: docs/features/calibration-guide.md (Stage 4)

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
// PID Controller Gains - ANGLE MODE (tune with 'g' command in Stage 4)
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
// Servo Channel Count
//=============================================================================
// How many servo channels setupMotors() LEDC-attaches on ESP32 (channels
// 1..SERVO_COUNT). Range 0-7. Default 7 keeps existing builds byte-identical.
//
// Lowering this frees the higher servo GPIOs: an airframe with SERVO_COUNT<=5
// never drives SERVO_PIN_6/7, so the W1b ch6/7 pin-mirror (SERVO_PIN_6/7 =
// SERVO_PIN_4/5 in pin_definitions_esp32.h) is moot — no LEDC double-claim.
// The mirror only matters when SERVO_COUNT>=6, which then needs explicit
// distinct SERVO_PIN_6/7 overrides in the PIN OVERRIDES section.
#ifndef SERVO_COUNT
    #define SERVO_COUNT 7
#endif

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
// PIN OVERRIDES (Optional)
//=============================================================================
// Override default pin assignments here. Defaults come from pin_definitions.h
// (Teensy) or pin_definitions_esp32.h (ESP32/S3). Any pin defined here takes
// priority over platform defaults.
//
// Uncomment and change any pin you need to override:
//
// Motor pins:
//#define MOTOR_PIN_1 0
//#define MOTOR_PIN_2 1
//#define MOTOR_PIN_3 2
//#define MOTOR_PIN_4 3
//#define MOTOR_PIN_5 4
//#define MOTOR_PIN_6 5
//
// Servo pins:
//#define SERVO_PIN_1 6
//#define SERVO_PIN_2 7
//#define SERVO_PIN_3 8
//#define SERVO_PIN_4 9
//
// ESP32 GPIO conflict resolution (2026-05-20, Option A — see
// docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md):
//   SERVO_PIN_3/4/5 ESP32 defaults are now GPIO 13/5/18 — moved OFF the
//   receiver pins (was 4/16/17) so ANY RC protocol (SBUS, iBUS, DSM) and a
//   1-5 servo count are supported without a GPIO double-claim. The W1 #error
//   guards in pin_definitions_esp32.h stay as a regression net.
//   GPIO 13 is MOTOR_PIN_6's default — the new SERVO_PIN_3 is only safe on a
//   4-motor (or fewer) airframe. If you populate motor 6, override SERVO_PIN_3
//   (and/or MOTOR_PIN_6) below to a different free GPIO before building.
//   SERVO_PIN_6/7 mirror SERVO_PIN_4/5 (no free GPIO remains); they are
//   unused on a 1-5 servo airframe. If you need 6-7 servos, assign distinct
//   free pins here.
//#define SERVO_PIN_3 13   // ESP32 default (off GPIO 4 — IBUS/DSM/SERIAL_CMD_RX)
//#define SERVO_PIN_4 5    // ESP32 default (off GPIO 16 — SBUS_RX)
//#define SERVO_PIN_5 18   // ESP32 default (off GPIO 17 — SBUS_TX)
//
// IMU I2C pins:
//#define IMU_SDA_PIN 18
//#define IMU_SCL_PIN 19
//
// OLED display I2C pins (software I2C, separate from IMU):
//#define OLED_SDA_PIN 16
//#define OLED_SCL_PIN 17
//
// Receiver pins (depends on protocol selected above):
//#define SBUS_RX_PIN 21
//#define DSM_RX_PIN 15
//#define PPM_PIN 23
//
// I2C command pins (USE_I2C_COMMANDS — Wire1, separate from IMU):
//#define I2C_CMD_SDA_PIN 17
//#define I2C_CMD_SCL_PIN 16
//#define I2C_CMD_ADDRESS 0x42
//
// Status LED:
//#define LED_PIN 13

//=============================================================================
// GPS PASSTHROUGH PINS (USE_GPS — ESP32 UART1, RX-only)
//=============================================================================
// GPS_PIN_RX is the UART1 RX GPIO the GPS module's TX wire connects to.
// Defaults below: GPIO 4 on standard ESP32 (the UART1-RX default), GPIO 16 on
// ESP32-S3. GPIO 4 was historically SERVO_PIN_3 too, but the 2026-05-20
// GPIO-conflict fix moved SERVO_PIN_3 to GPIO 13 — so GPIO 4 is conflict-free
// for GPS on a current build. The receiver-vs-GPS UART1 contention is caught
// by the #error guard in gps.h. Passthrough is RX-only: GPS_PIN_TX is -1
// (Arduino "unused" sentinel) so no TX GPIO is claimed.
// See docs/findings/gps_passthrough_spec_2026-05-20.md §3.
#ifdef USE_GPS
    #ifndef GPS_PIN_RX
        #ifdef USE_ESP32S3
            #define GPS_PIN_RX 16   // ESP32-S3 UART1 RX
        #else
            #define GPS_PIN_RX 4    // ESP32 UART1 RX
        #endif
    #endif
    #ifndef GPS_PIN_TX
        #define GPS_PIN_TX -1       // unused — passthrough is RX-only
    #endif

    // GPS_TELEMETRY_INCLUDE_POSITION — privacy gate on the GPS telemetry block.
    // Security audit 2026-05-22 SEC-04: the swarm API client (api_client.cpp)
    // and the web server (web_server.cpp) relay the raw NMEA sentence verbatim
    // over the (by default unauthenticated) network. GGA/RMC sentences carry
    // latitude/longitude/altitude, so any LAN peer can track the precise
    // aircraft position. Because the FC is a pure passthrough (it parses
    // nothing), the position data lives ENTIRELY inside the NMEA string — there
    // is no separate lat/lon field to drop, and fix/sat counts are embedded in
    // that same string too. The only way to omit position without
    // re-implementing an NMEA parser on the FC is to omit the raw `nmea` field
    // altogether, keeping the non-identifying liveness fields (`ok`, `age_ms`).
    //
    // DEFAULT ON (1): preserves the existing swarm-API contract (full
    // passthrough) and the documented behavior. The privacy trade-off — a
    // default build broadcasts position to anyone on the flight LAN. Set to 0
    // for an OPSEC build that withholds the raw NMEA sentence; consumers then
    // see only gps.ok / gps.age_ms (a GPS-is-alive heartbeat) and no
    // coordinates. Pair with USE_API_AUTH (SEC-01) to also gate the read path.
    #ifndef GPS_TELEMETRY_INCLUDE_POSITION
        #define GPS_TELEMETRY_INCLUDE_POSITION 1
    #endif
#endif

//=============================================================================
// Debug Options
//=============================================================================
// Serial baud rate
#define SERIAL_BAUD 115200

// Debug print rate limiting (microseconds between prints)
#define DEBUG_PRINT_INTERVAL 10000  // 10ms = 100Hz print rate

#endif // CONFIG_H