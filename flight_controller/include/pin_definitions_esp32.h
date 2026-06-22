/*
 * Pin Definitions for ESP32 and ESP32-S3
 * Physical pin assignments for all peripherals
 *
 * Note: ESP32 GPIO numbers are used directly (not physical pin numbers).
 * This file is included when USE_ESP32 is defined.
 *
 * All pins use #ifndef guards — override any pin in config.h before this
 * file is included. Example in config.h:
 *   #define MOTOR_PIN_1 32  // Override default pin 25
 *
 * ESP32-S3 overrides only apply when the user hasn't already defined
 * a pin in config.h (user config.h overrides always win).
 */

#ifndef PIN_DEFINITIONS_ESP32_H
#define PIN_DEFINITIONS_ESP32_H

//========================================================================================================================//
//                                              IMU PINS (I2C)                                                            //
//========================================================================================================================//

#ifndef IMU_SDA_PIN
    #ifdef USE_ESP32S3
        #define IMU_SDA_PIN 8   // I2C SDA (GPIO8) — S3 default
    #else
        #define IMU_SDA_PIN 21  // I2C SDA (GPIO21) — ESP32 default
    #endif
#endif
#ifndef IMU_SCL_PIN
    #ifdef USE_ESP32S3
        #define IMU_SCL_PIN 9   // I2C SCL (GPIO9) — S3 default
    #else
        #define IMU_SCL_PIN 22  // I2C SCL (GPIO22) — ESP32 default
    #endif
#endif

//========================================================================================================================//
//                                              MOTOR PINS (PWM via LEDC)                                                 //
//========================================================================================================================//
// ESP32 uses LEDC peripheral for PWM output.
// These pins support PWM output. Avoid strapping pins (0, 2, 12, 15).

#ifndef MOTOR_PIN_1
    #ifdef USE_ESP32S3
        #define MOTOR_PIN_1 35
    #else
        #define MOTOR_PIN_1 25  // Front Left in X config
    #endif
#endif
#ifndef MOTOR_PIN_2
    #ifdef USE_ESP32S3
        #define MOTOR_PIN_2 36
    #else
        #define MOTOR_PIN_2 26  // Front Right in X config
    #endif
#endif
#ifndef MOTOR_PIN_3
    #ifdef USE_ESP32S3
        #define MOTOR_PIN_3 37
    #else
        #define MOTOR_PIN_3 27  // Back Right in X config
    #endif
#endif
#ifndef MOTOR_PIN_4
    #ifdef USE_ESP32S3
        #define MOTOR_PIN_4 38
    #else
        #define MOTOR_PIN_4 14  // Back Left in X config
    #endif
#endif
#ifndef MOTOR_PIN_5
    #ifdef USE_ESP32S3
        #define MOTOR_PIN_5 39
    #else
        #define MOTOR_PIN_5 12  // Optional — Note: strapping pin on ESP32
    #endif
#endif
#ifndef MOTOR_PIN_6
    #ifdef USE_ESP32S3
        #define MOTOR_PIN_6 40
    #else
        #define MOTOR_PIN_6 13  // Optional
    #endif
#endif

//========================================================================================================================//
//                                              SERVO PINS (50Hz PWM via LEDC)                                            //
//========================================================================================================================//

#ifndef SERVO_PIN_1
    #ifdef USE_ESP32S3
        #define SERVO_PIN_1 41
    #else
        #define SERVO_PIN_1 32
    #endif
#endif
#ifndef SERVO_PIN_2
    #ifdef USE_ESP32S3
        #define SERVO_PIN_2 42
    #else
        #define SERVO_PIN_2 33
    #endif
#endif
#ifndef SERVO_PIN_3
    #ifdef USE_ESP32S3
        #define SERVO_PIN_3 1
    #else
        // 2026-05-20: moved off GPIO 4 (was 4). GPIO 4 is shared by
        // IBUS_RX/DSM_RX/SERIAL_CMD_RX — Conflict B. Reassigned to GPIO 13
        // (free on 4-motor airframes; was MOTOR_PIN_6) so any receiver
        // protocol can be enabled. See esp32_gpio_conflict_resolution_2026-05-20.md.
        #define SERVO_PIN_3 13
    #endif
#endif
#ifndef SERVO_PIN_4
    #ifdef USE_ESP32S3
        #define SERVO_PIN_4 2
    #else
        // 2026-05-20: moved off GPIO 16 (was 16). GPIO 16 = SBUS_RX_PIN —
        // Conflict A. Reassigned to GPIO 5 (free when servo ch6 unused;
        // 1-5 servo airframes never use ch6). See esp32_gpio_conflict_resolution_2026-05-20.md.
        #define SERVO_PIN_4 5
    #endif
#endif
#ifndef SERVO_PIN_5
    #ifdef USE_ESP32S3
        #define SERVO_PIN_5 10
    #else
        // 2026-05-20: moved off GPIO 17 (was 17). GPIO 17 = SBUS_TX_PIN —
        // Conflict A. Reassigned to GPIO 18 (free when servo ch7 unused;
        // 1-5 servo airframes never use ch7). See esp32_gpio_conflict_resolution_2026-05-20.md.
        #define SERVO_PIN_5 18
    #endif
#endif
#ifndef SERVO_PIN_6
    #ifdef USE_ESP32S3
        #define SERVO_PIN_6 11
    #else
        // 2026-05-20: GPIO 5 was reassigned to SERVO_PIN_4 (Conflict A fix).
        // No free ESP32 GPIO remains for ch6 — mirror SERVO_PIN_4's pin.
        // Harmless: 1-5 servo airframes never command ch6, and this keeps
        // every servo pin off every receiver pin (no Conflict A/B revival).
        #define SERVO_PIN_6 SERVO_PIN_4
    #endif
#endif
#ifndef SERVO_PIN_7
    #ifdef USE_ESP32S3
        #define SERVO_PIN_7 12
    #else
        // 2026-05-20: GPIO 18 was reassigned to SERVO_PIN_5 (Conflict A fix).
        // No free ESP32 GPIO remains for ch7 — mirror SERVO_PIN_5's pin.
        // Harmless: 1-5 servo airframes never command ch7, and this keeps
        // every servo pin off every receiver pin (no Conflict A/B revival).
        #define SERVO_PIN_7 SERVO_PIN_5
    #endif
#endif

// Guards against silent double-claim of ESP32 LEDC GPIOs when an airframe
// actually enables servo ch6/ch7. The default mirror (SERVO_PIN_6=SERVO_PIN_4,
// SERVO_PIN_7=SERVO_PIN_5) is harmless only when SERVO_COUNT < 6/7 (ch6/ch7
// never commanded). If SERVO_COUNT >= 6 or >= 7 on standard ESP32, the user
// MUST override SERVO_PIN_6/SERVO_PIN_7 in config.h. ESP32-S3 has distinct
// pin assignments so the comparison is skipped there.
#if !defined(USE_ESP32S3) && SERVO_COUNT >= 6 && (SERVO_PIN_6 == SERVO_PIN_4)
    #error "SERVO_COUNT>=6 on ESP32 requires explicit SERVO_PIN_6 override in config.h — default mirror would silently double-claim LEDC GPIO"
#endif
#if !defined(USE_ESP32S3) && SERVO_COUNT >= 7 && (SERVO_PIN_7 == SERVO_PIN_5)
    #error "SERVO_COUNT>=7 on ESP32 requires explicit SERVO_PIN_7 override in config.h — default mirror would silently double-claim LEDC GPIO"
#endif

//========================================================================================================================//
//                                              RECEIVER PINS                                                             //
//========================================================================================================================//

// SBUS (Serial2 on ESP32)
// Note: SBUS requires inverted serial - ESP32 can do this in software
#ifndef SBUS_RX_PIN
    #ifdef USE_ESP32S3
        #define SBUS_RX_PIN  18  // RX on S3
    #else
        #define SBUS_RX_PIN  16  // RX2 pin
    #endif
#endif
#ifndef SBUS_TX_PIN
    #ifdef USE_ESP32S3
        #define SBUS_TX_PIN  17  // TX on S3
    #else
        #define SBUS_TX_PIN  17  // TX2 pin (not used)
    #endif
#endif

// iBUS (Serial1 on ESP32)
#ifndef IBUS_RX_PIN
    #ifdef USE_ESP32S3
        #define IBUS_RX_PIN  16  // RX1 on S3
    #else
        #define IBUS_RX_PIN  4   // RX1 pin
    #endif
#endif
#ifndef IBUS_TX_PIN
    #ifdef USE_ESP32S3
        #define IBUS_TX_PIN  15  // TX1 on S3
    #else
        #define IBUS_TX_PIN  2   // TX1 pin (not used)
    #endif
#endif

// DSM (Serial1)
#ifndef DSM_RX_PIN
    #define DSM_RX_PIN   4   // RX1 pin
#endif
#ifndef DSM_TX_PIN
    #define DSM_TX_PIN   2   // TX1 pin (not used)
#endif

// Serial Commands (external flight computer, Serial1)
#ifndef SERIAL_CMD_RX_PIN
    #ifdef USE_ESP32S3
        #define SERIAL_CMD_RX_PIN 16  // RX1 on S3
    #else
        #define SERIAL_CMD_RX_PIN 4   // RX1 pin
    #endif
#endif
#ifndef SERIAL_CMD_TX_PIN
    #ifdef USE_ESP32S3
        #define SERIAL_CMD_TX_PIN 15  // TX1 on S3
    #else
        #define SERIAL_CMD_TX_PIN 2   // TX1 pin (future: acknowledgments)
    #endif
#endif

// I2C Commands (external flight computer, Wire1 — separate from IMU on Wire)
#ifndef I2C_CMD_SDA_PIN
    #ifdef USE_ESP32S3
        #define I2C_CMD_SDA_PIN 41  // Wire1 SDA on S3
    #else
        #define I2C_CMD_SDA_PIN 25  // Wire1 SDA
    #endif
#endif
#ifndef I2C_CMD_SCL_PIN
    #ifdef USE_ESP32S3
        #define I2C_CMD_SCL_PIN 42  // Wire1 SCL on S3
    #else
        #define I2C_CMD_SCL_PIN 26  // Wire1 SCL
    #endif
#endif
#ifndef I2C_CMD_ADDRESS
    #define I2C_CMD_ADDRESS 0x42  // I2C slave address
#endif

// PPM
#ifndef PPM_PIN
    #ifdef USE_ESP32S3
        #define PPM_PIN      5   // PPM input on S3
    #else
        #define PPM_PIN      35  // PPM input (input-only GPIO)
    #endif
#endif

// PWM (individual channel inputs)
#ifndef PWM_CH1_PIN
    #define PWM_CH1_PIN  35  // Channel 1 (Roll)
#endif
#ifndef PWM_CH2_PIN
    #define PWM_CH2_PIN  34  // Channel 2 (Pitch)
#endif
#ifndef PWM_CH3_PIN
    #define PWM_CH3_PIN  39  // Channel 3 (Throttle) - VN
#endif
#ifndef PWM_CH4_PIN
    #define PWM_CH4_PIN  36  // Channel 4 (Yaw) - VP
#endif
#ifndef PWM_CH5_PIN
    #define PWM_CH5_PIN  23  // Channel 5 (Aux1)
#endif
#ifndef PWM_CH6_PIN
    #define PWM_CH6_PIN  19  // Channel 6 (Aux2)
#endif

//========================================================================================================================//
//                                              STATUS LED                                                                //
//========================================================================================================================//

#ifndef LED_PIN
    #ifdef USE_ESP32S3
        #define LED_PIN 48  // RGB LED on ESP32-S3 DevKitC-1
    #else
        #define LED_PIN 2   // Built-in LED on most ESP32 dev boards
    #endif
#endif

//========================================================================================================================//
//                                              OLED DISPLAY (Software I2C)                                              //
//========================================================================================================================//
// Dedicated pins for OLED to avoid I2C bus contention with IMU.
// Default: GPIO 23/19 (free when not using PWM receiver, which is the common case).
// Change these if your receiver uses PWM mode.

#ifndef OLED_SDA_PIN
    #ifdef USE_ESP32S3
        #define OLED_SDA_PIN 3   // S3: Software I2C SDA
    #else
        #define OLED_SDA_PIN 23  // Software I2C SDA for OLED
    #endif
#endif
#ifndef OLED_SCL_PIN
    #ifdef USE_ESP32S3
        #define OLED_SCL_PIN 46  // S3: Software I2C SCL
    #else
        #define OLED_SCL_PIN 19  // Software I2C SCL for OLED
    #endif
#endif

//========================================================================================================================//
//                                              WIRING NOTES                                                              //
//========================================================================================================================//

/*
 * ESP32 DevKit V1 PINOUT NOTES:
 * =============================
 *
 * POWER:
 * - VIN: 5V input (from USB or external)
 * - 3.3V: 3.3V regulated output (for sensors)
 * - GND: Ground
 *
 * AVOID THESE PINS:
 * - GPIO0: Strapping pin (affects boot mode)
 * - GPIO2: Strapping pin, but OK after boot (used for LED)
 * - GPIO12: Strapping pin (affects flash voltage)
 * - GPIO15: Strapping pin (debugging)
 * - GPIO6-11: Connected to internal flash (DO NOT USE)
 *
 * INPUT-ONLY PINS (cannot output):
 * - GPIO34, 35, 36 (VP), 39 (VN)
 *
 * MPU6050 WIRING:
 * ================
 * MPU6050 Pin → ESP32 Pin
 * ------------------------------------
 * VCC         → 3.3V (NOT 5V!)
 * GND         → GND
 * SDA         → GPIO21 (or GPIO8 on S3)
 * SCL         → GPIO22 (or GPIO9 on S3)
 *
 *
 * SBUS RECEIVER WIRING:
 * =====================
 * Receiver SBUS → ESP32 GPIO16 (RX2)
 * Receiver VCC  → 5V (VIN)
 * Receiver GND  → GND
 *
 * Note: ESP32 can invert the SBUS signal in software using
 * Serial2.begin(100000, SERIAL_8E2, SBUS_RX_PIN, SBUS_TX_PIN, true);
 * The 'true' parameter enables inversion.
 *
 *
 * MOTOR ESC WIRING:
 * =================
 * - ESC signal wires to motor pins (25, 26, 27, 14, etc.)
 * - ESC ground to ESP32 GND
 * - ESC power from battery
 * - Use 3.3V signal level (most ESCs accept this)
 *
 *
 * OLED DISPLAY WIRING (Optional):
 * ===============================
 * OLED Pin → ESP32 Pin
 * ------------------------------------
 * VCC      → 3.3V
 * GND      → GND
 * SDA      → GPIO21 (shared with IMU)
 * SCL      → GPIO22 (shared with IMU)
 *
 * I2C addresses: IMU=0x68, OLED=0x3C (no conflict)
 *
 *
 * POWER CONSIDERATIONS:
 * =====================
 * - ESP32 can be powered via USB (5V) or VIN pin (5-12V)
 * - Internal regulator provides 3.3V to ESP32 and sensors
 * - WiFi active: ~150mA typical, spikes to 350mA during TX
 * - Use a good quality 5V power source rated for at least 500mA
 * - For flight, use a dedicated 5V BEC
 */

//========================================================================================================================//
//                              ESP32 GPIO CONFLICT GUARDS (compile-time)                                                 //
//========================================================================================================================//
// Catches the known-bad ESP32 GPIO double-claims surfaced by the 2026-05-20
// wiring-guide audit. setupServos() in motors.cpp calls ledcAttachPin() for ALL
// servo pins on every ESP32 build (no USE_SERVO gate), so a servo pin that
// equals a receiver pin is an electrical double-claim even with no servo wired.
//
// These #error directives refuse to compile a config that genuinely enables
// both colliding claimants. They do NOT change any pin — resolve a fired guard
// via the PIN OVERRIDES section of config.h.
// See: docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md (Option C).
//
// Standard ESP32 only — ESP32-S3 pin maps have no equivalent conflicts.
#if defined(USE_ESP32) && !defined(USE_ESP32S3)

    // --- Conflict A: SBUS RX/TX (GPIO 16/17) vs SERVO_PIN_4 / SERVO_PIN_5 ---
    #if defined(USE_SBUS_RECEIVER) && \
        (SERVO_PIN_4 == SBUS_RX_PIN || SERVO_PIN_5 == SBUS_TX_PIN)
        #error "ESP32 GPIO conflict A: SERVO_PIN_4/SERVO_PIN_5 (GPIO 16/17) collide with SBUS_RX_PIN/SBUS_TX_PIN. setupServos() drives these pins via LEDC on every build, fighting the SBUS UART. Override SERVO_PIN_4/SERVO_PIN_5 (or the SBUS pins) in the PIN OVERRIDES section of config.h. See docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md."
    #endif

    // --- Conflict B: GPIO 4 multi-claim — SERVO_PIN_3 vs the selected
    //     serial receiver / serial-command RX (iBUS / DSM / serial-cmd) ---
    #if (defined(USE_IBUS_RECEIVER)   && SERVO_PIN_3 == IBUS_RX_PIN)       || \
        (defined(USE_DSM_RECEIVER)    && SERVO_PIN_3 == DSM_RX_PIN)        || \
        (defined(USE_SERIAL_COMMANDS) && SERVO_PIN_3 == SERIAL_CMD_RX_PIN)
        #error "ESP32 GPIO conflict B: SERVO_PIN_3 (GPIO 4) collides with the selected receiver RX (IBUS_RX_PIN / DSM_RX_PIN / SERIAL_CMD_RX_PIN). setupServos() drives GPIO 4 via LEDC on every build, fighting the Serial1 UART. Override SERVO_PIN_3 (or the receiver pin) in the PIN OVERRIDES section of config.h. See docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md."
    #endif

    // RESOLVED (C-1): no barometer GPIO guard is needed. W2 has landed and the
    // barometer shares the primary `Wire` bus (GPIO 21/22) with the IMU — it
    // does NOT use Wire1 / GPIO 25/26, so it touches no MOTOR_PIN. There are no
    // BARO_*_PIN symbols and nothing for a guard to catch. The earlier C-1
    // concern assumed a Wire1 baro on GPIO 25/26; that design was not adopted.
    // See docs/findings/phase_w2_barometer_landed_2026-05-20.md and config.h's
    // BAROMETER section. (Was: docs/findings/session3_readiness_2026-05-20.md C-1.)

#endif // USE_ESP32 && !USE_ESP32S3

#endif // PIN_DEFINITIONS_ESP32_H
