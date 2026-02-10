/*
 * Pin Definitions - Platform Dispatcher
 * Includes the correct pin definitions based on target platform
 */

#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

//========================================================================================================================//
//                                              PLATFORM DETECTION                                                        //
//========================================================================================================================//

#ifdef USE_ESP32
    // ESP32 or ESP32-S3 platform
    #include "pin_definitions_esp32.h"
#else
    // Default: Teensy platform (Teensy 4.0, 4.1, 3.6)
    // Pin definitions are below in this file
#endif

//========================================================================================================================//
//                                         TEENSY PIN DEFINITIONS                                                         //
//========================================================================================================================//
// Only compiled for Teensy builds (when USE_ESP32 is not defined)
//
// All pins use #ifndef guards — override any pin in config.h before this file
// is included. Example in config.h:
//   #define MOTOR_PIN_1 10  // Override default pin 0

#ifndef USE_ESP32

//========================================================================================================================//
//                                              IMU PINS (I2C)                                                            //
//========================================================================================================================//

#ifndef IMU_SDA_PIN
    #define IMU_SDA_PIN 18  // I2C SDA (Wire)
#endif
#ifndef IMU_SCL_PIN
    #define IMU_SCL_PIN 19  // I2C SCL (Wire)
#endif

//========================================================================================================================//
//                                              MOTOR PINS (OneShot125 or PWM)                                            //
//========================================================================================================================//

#ifndef MOTOR_PIN_1
    #define MOTOR_PIN_1 0   // Motor 1 (Front Left in X config)
#endif
#ifndef MOTOR_PIN_2
    #define MOTOR_PIN_2 1   // Motor 2 (Front Right in X config)
#endif
#ifndef MOTOR_PIN_3
    #define MOTOR_PIN_3 2   // Motor 3 (Back Right in X config)
#endif
#ifndef MOTOR_PIN_4
    #define MOTOR_PIN_4 3   // Motor 4 (Back Left in X config)
#endif
#ifndef MOTOR_PIN_5
    #define MOTOR_PIN_5 4   // Motor 5 (optional)
#endif
#ifndef MOTOR_PIN_6
    #define MOTOR_PIN_6 5   // Motor 6 (optional)
#endif

//========================================================================================================================//
//                                              SERVO PINS (50Hz PWM)                                                     //
//========================================================================================================================//

#ifndef SERVO_PIN_1
    #define SERVO_PIN_1 6   // Servo 1
#endif
#ifndef SERVO_PIN_2
    #define SERVO_PIN_2 7   // Servo 2
#endif
#ifndef SERVO_PIN_3
    #define SERVO_PIN_3 8   // Servo 3
#endif
#ifndef SERVO_PIN_4
    #define SERVO_PIN_4 9   // Servo 4
#endif
#ifndef SERVO_PIN_5
    #define SERVO_PIN_5 10  // Servo 5
#endif
#ifndef SERVO_PIN_6
    #define SERVO_PIN_6 11  // Servo 6
#endif
#ifndef SERVO_PIN_7
    #define SERVO_PIN_7 12  // Servo 7
#endif

//========================================================================================================================//
//                                              RECEIVER PINS                                                             //
//========================================================================================================================//

// SBUS (Serial5)
#ifndef SBUS_RX_PIN
    #define SBUS_RX_PIN  21  // RX5 pin
#endif
#ifndef SBUS_TX_PIN
    #define SBUS_TX_PIN  20  // TX5 pin (not used)
#endif

// iBUS (Serial3)
#ifndef IBUS_RX_PIN
    #define IBUS_RX_PIN  15  // RX3 pin
#endif
#ifndef IBUS_TX_PIN
    #define IBUS_TX_PIN  14  // TX3 pin (not used)
#endif

// DSM (Serial3)
#ifndef DSM_RX_PIN
    #define DSM_RX_PIN   15  // RX3 pin
#endif
#ifndef DSM_TX_PIN
    #define DSM_TX_PIN   14  // TX3 pin (not used)
#endif

// Serial Commands (external flight computer, Serial3)
#ifndef SERIAL_CMD_RX_PIN
    #define SERIAL_CMD_RX_PIN 15  // RX3 pin
#endif
#ifndef SERIAL_CMD_TX_PIN
    #define SERIAL_CMD_TX_PIN 14  // TX3 pin (future: acknowledgments)
#endif

// PPM
#ifndef PPM_PIN
    #define PPM_PIN      23  // Single PPM input
#endif

// PWM (individual channel inputs)
#ifndef PWM_CH1_PIN
    #define PWM_CH1_PIN  23  // Channel 1 (Roll)
#endif
#ifndef PWM_CH2_PIN
    #define PWM_CH2_PIN  22  // Channel 2 (Pitch)
#endif
#ifndef PWM_CH3_PIN
    #define PWM_CH3_PIN  21  // Channel 3 (Throttle)
#endif
#ifndef PWM_CH4_PIN
    #define PWM_CH4_PIN  20  // Channel 4 (Yaw)
#endif
#ifndef PWM_CH5_PIN
    #define PWM_CH5_PIN  17  // Channel 5 (Aux1)
#endif
#ifndef PWM_CH6_PIN
    #define PWM_CH6_PIN  16  // Channel 6 (Aux2)
#endif

//========================================================================================================================//
//                                              OLED DISPLAY (Software I2C)                                              //
//========================================================================================================================//
// Dedicated pins for OLED to avoid I2C bus contention with IMU.
// Default: pins 16/17 (free when not using PWM receiver).
// Change these if your receiver uses PWM mode.

#ifndef OLED_SDA_PIN
    #define OLED_SDA_PIN 16  // Software I2C SDA for OLED
#endif
#ifndef OLED_SCL_PIN
    #define OLED_SCL_PIN 17  // Software I2C SCL for OLED
#endif

//========================================================================================================================//
//                                              STATUS LED                                                                //
//========================================================================================================================//

#ifndef LED_PIN
    #define LED_PIN 13  // Built-in LED on Teensy 4.0/4.1
#endif

//========================================================================================================================//
//                                              WIRING NOTES                                                              //
//========================================================================================================================//

/*
 * FLYSKY FS-iA6B WIRING (SBUS MODE):
 * ===================================
 * FS-iA6B Pin  → Teensy 4.0 Pin
 * ------------------------------------
 * VCC (Red)    → 5V (VIN or USB power)
 * GND (Black)  → GND
 * SBUS (Black) → Pin 21 (RX5)
 * 
 * IMPORTANT NOTES:
 * - Use the SBUS port on the receiver (NOT PPM/PWM)
 * - SBUS is an inverted serial signal at 100000 baud
 * - Teensy 4.0/4.1 hardware automatically handles inversion
 * - The receiver needs 3.5-9V power (5V is perfect)
 * - Bind the receiver to your transmitter before connecting
 * - Set transmitter to output SBUS format
 * 
 * 
 * FLYSKY FS-iA6B WIRING (iBUS MODE):
 * ===================================
 * FS-iA6B Pin  → Teensy 4.0 Pin
 * ------------------------------------
 * VCC (Red)    → 5V (VIN or USB power)
 * GND (Black)  → GND
 * iBUS Signal  → Pin 15 (RX3) via VOLTAGE DIVIDER
 *
 * VOLTAGE DIVIDER (REQUIRED - Teensy 4.0 is NOT 5V tolerant!):
 *   iBUS signal → [1k ohm] → Pin 15 (RX3)
 *                            |
 *                       [2k ohm]
 *                            |
 *                           GND
 *   Output: 5V * 2k/(1k+2k) = 3.33V (safe for Teensy)
 *
 * IMPORTANT NOTES:
 * - Use the iBUS port on the receiver (B/VCC header signal pin)
 * - iBUS is standard serial at 115200 baud, 8N1, non-inverted
 * - 14 channels available, values in 1000-2000 microseconds
 * - See docs/wiring_diagrams/ for complete build guides
 * 
 * 
 * MPU6050 WIRING:
 * ================
 * MPU6050 Pin → Teensy 4.0 Pin
 * ------------------------------------
 * VCC         → 3.3V (NOT 5V!)
 * GND         → GND
 * SDA         → Pin 18 (SDA/A4)
 * SCL         → Pin 19 (SCL/A5)
 * 
 * IMPORTANT NOTES:
 * - MPU6050 is 3.3V only! Do not connect to 5V!
 * - Use I2C address 0x68 (default)
 * - If using AD0 pin, connect to GND for 0x68 or 3.3V for 0x69
 * - No external pullup resistors needed (Teensy has internal pullups)
 * 
 * 
 * ESC WIRING (Motors):
 * ====================
 * - Connect ESC signal wires to motor pins 0-5
 * - ESC ground to Teensy GND
 * - ESC power comes from battery (NOT from Teensy!)
 * - Use a separate BEC or voltage regulator to power Teensy
 * - Common ground between Teensy and ESCs is required
 * 
 * 
 * SERVO WIRING:
 * =============
 * - Connect servo signal wires to servo pins 6-12
 * - Servo ground to Teensy GND
 * - Servo power (5V) from external BEC (servos draw too much current for Teensy)
 * - Do NOT power servos directly from Teensy 5V pin!
 * 
 * 
 * POWER CONSIDERATIONS:
 * =====================
 * - Teensy 4.0/4.1 can be powered via USB (5V) or VIN pin (5-5.5V)
 * - USB power is sufficient for development/testing
 * - For flight, use a 5V BEC from your main battery
 * - Total current draw:
 *   - Teensy 4.0: ~100mA
 *   - MPU6050: ~3.5mA
 *   - FS-iA6B: ~70mA
 *   - Total: <200mA (easily handled by most 5V BECs)
 * - Motors and servos should have separate power!
 */

#endif // !USE_ESP32

#endif // PIN_DEFINITIONS_H