/*
 * Pin Definitions for ESP32 and ESP32-S3
 * Physical pin assignments for all peripherals
 *
 * Note: ESP32 GPIO numbers are used directly (not physical pin numbers).
 * This file is included when USE_ESP32 is defined.
 */

#ifndef PIN_DEFINITIONS_ESP32_H
#define PIN_DEFINITIONS_ESP32_H

//========================================================================================================================//
//                                              IMU PINS (I2C)                                                            //
//========================================================================================================================//

// ESP32 default I2C pins (can be remapped)
#define IMU_SDA_PIN 21  // I2C SDA (GPIO21)
#define IMU_SCL_PIN 22  // I2C SCL (GPIO22)

// ESP32-S3 can use different pins if needed
#ifdef USE_ESP32S3
    // S3 default I2C pins
    #undef IMU_SDA_PIN
    #undef IMU_SCL_PIN
    #define IMU_SDA_PIN 8   // I2C SDA (GPIO8)
    #define IMU_SCL_PIN 9   // I2C SCL (GPIO9)
#endif

//========================================================================================================================//
//                                              MOTOR PINS (PWM via LEDC)                                                 //
//========================================================================================================================//
// ESP32 uses LEDC peripheral for PWM output.
// These pins support PWM output. Avoid strapping pins (0, 2, 12, 15).

#define MOTOR_PIN_1 25  // Motor 1 (Front Left in X config)
#define MOTOR_PIN_2 26  // Motor 2 (Front Right in X config)
#define MOTOR_PIN_3 27  // Motor 3 (Back Right in X config)
#define MOTOR_PIN_4 14  // Motor 4 (Back Left in X config)
#define MOTOR_PIN_5 12  // Motor 5 (optional) - Note: strapping pin
#define MOTOR_PIN_6 13  // Motor 6 (optional)

#ifdef USE_ESP32S3
    // ESP32-S3 different pinout
    #undef MOTOR_PIN_1
    #undef MOTOR_PIN_2
    #undef MOTOR_PIN_3
    #undef MOTOR_PIN_4
    #undef MOTOR_PIN_5
    #undef MOTOR_PIN_6
    #define MOTOR_PIN_1 35  // Motor 1
    #define MOTOR_PIN_2 36  // Motor 2
    #define MOTOR_PIN_3 37  // Motor 3
    #define MOTOR_PIN_4 38  // Motor 4
    #define MOTOR_PIN_5 39  // Motor 5
    #define MOTOR_PIN_6 40  // Motor 6
#endif

//========================================================================================================================//
//                                              SERVO PINS (50Hz PWM via LEDC)                                            //
//========================================================================================================================//

#define SERVO_PIN_1 32  // Servo 1
#define SERVO_PIN_2 33  // Servo 2
#define SERVO_PIN_3 4   // Servo 3
#define SERVO_PIN_4 16  // Servo 4
#define SERVO_PIN_5 17  // Servo 5
#define SERVO_PIN_6 5   // Servo 6
#define SERVO_PIN_7 18  // Servo 7

#ifdef USE_ESP32S3
    #undef SERVO_PIN_1
    #undef SERVO_PIN_2
    #undef SERVO_PIN_3
    #undef SERVO_PIN_4
    #undef SERVO_PIN_5
    #undef SERVO_PIN_6
    #undef SERVO_PIN_7
    #define SERVO_PIN_1 41  // Servo 1
    #define SERVO_PIN_2 42  // Servo 2
    #define SERVO_PIN_3 1   // Servo 3
    #define SERVO_PIN_4 2   // Servo 4
    #define SERVO_PIN_5 10  // Servo 5
    #define SERVO_PIN_6 11  // Servo 6
    #define SERVO_PIN_7 12  // Servo 7
#endif

//========================================================================================================================//
//                                              RECEIVER PINS                                                             //
//========================================================================================================================//

// SBUS (Serial2 on ESP32)
// Note: SBUS requires inverted serial - ESP32 can do this in software
#define SBUS_RX_PIN  16  // RX2 pin
#define SBUS_TX_PIN  17  // TX2 pin (not used)

// iBUS (Serial1 on ESP32)
#define IBUS_RX_PIN  4   // RX1 pin
#define IBUS_TX_PIN  2   // TX1 pin (not used)

// DSM (Serial1)
#define DSM_RX_PIN   4   // RX1 pin
#define DSM_TX_PIN   2   // TX1 pin (not used)

// PPM
#define PPM_PIN      35  // PPM input (input-only GPIO)

// PWM (individual channel inputs)
#define PWM_CH1_PIN  35  // Channel 1 (Roll)
#define PWM_CH2_PIN  34  // Channel 2 (Pitch)
#define PWM_CH3_PIN  39  // Channel 3 (Throttle) - VN
#define PWM_CH4_PIN  36  // Channel 4 (Yaw) - VP
#define PWM_CH5_PIN  23  // Channel 5 (Aux1)
#define PWM_CH6_PIN  19  // Channel 6 (Aux2)

#ifdef USE_ESP32S3
    #undef SBUS_RX_PIN
    #undef SBUS_TX_PIN
    #undef IBUS_RX_PIN
    #undef IBUS_TX_PIN
    #undef PPM_PIN
    #define SBUS_RX_PIN  18  // RX on S3
    #define SBUS_TX_PIN  17  // TX on S3
    #define IBUS_RX_PIN  16  // RX1 on S3
    #define IBUS_TX_PIN  15  // TX1 on S3
    #define PPM_PIN      5   // PPM input
#endif

//========================================================================================================================//
//                                              STATUS LED                                                                //
//========================================================================================================================//

#define LED_PIN 2  // Built-in LED on most ESP32 dev boards (GPIO2)

#ifdef USE_ESP32S3
    #undef LED_PIN
    #define LED_PIN 48  // RGB LED on ESP32-S3 DevKitC-1 (or use GPIO48)
#endif

//========================================================================================================================//
//                                              OLED DISPLAY (Optional)                                                   //
//========================================================================================================================//
// Uses same I2C bus as IMU by default

#define OLED_SDA_PIN IMU_SDA_PIN
#define OLED_SCL_PIN IMU_SCL_PIN
#define OLED_I2C_ADDR 0x3C  // SSD1306 default address

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
