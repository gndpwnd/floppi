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
        #define SERVO_PIN_3 4
    #endif
#endif
#ifndef SERVO_PIN_4
    #ifdef USE_ESP32S3
        #define SERVO_PIN_4 2
    #else
        #define SERVO_PIN_4 16
    #endif
#endif
#ifndef SERVO_PIN_5
    #ifdef USE_ESP32S3
        #define SERVO_PIN_5 10
    #else
        #define SERVO_PIN_5 17
    #endif
#endif
#ifndef SERVO_PIN_6
    #ifdef USE_ESP32S3
        #define SERVO_PIN_6 11
    #else
        #define SERVO_PIN_6 5
    #endif
#endif
#ifndef SERVO_PIN_7
    #ifdef USE_ESP32S3
        #define SERVO_PIN_7 12
    #else
        #define SERVO_PIN_7 18
    #endif
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

#endif // PIN_DEFINITIONS_ESP32_H
