/**
 * Pin Configuration for Auto Orientation
 *
 * Customize for your specific board and wiring.
 * See docs/features/hardware-setup.md for wiring guide.
 */

#ifndef PINS_H
#define PINS_H

// ============================================================================
// UART/Serial Pins for BNO085
// ============================================================================
// Most Arduino boards use the main Serial port (Serial1 for hardware UART)
// Edit these if your board has different UART pins

#if defined(__AVR_ATmega328P__)
  // Arduino Nano / Uno
  // Using SoftwareSerial on pins 10 (RX) and 11 (TX)
  #define BNO085_RX_PIN 10
  #define BNO085_TX_PIN 11
  #define BNO085_USE_SOFTWARE_SERIAL true

#elif defined(__AVR_ATmega2560__)
  // Arduino Mega: use Serial1 (RX1=19, TX1=18)
  #define BNO085_RX_PIN 19
  #define BNO085_TX_PIN 18
  #define BNO085_USE_SOFTWARE_SERIAL false

#elif defined(TEENSY31) || defined(TEENSY32)
  // Teensy 3.x: use Serial1 (RX1=0, TX1=1)
  #define BNO085_RX_PIN 0
  #define BNO085_TX_PIN 1
  #define BNO085_USE_SOFTWARE_SERIAL false

#elif defined(ESP32)
  // ESP32: use Serial1 (RX1=9, TX1=10) or custom pins
  #define BNO085_RX_PIN 9
  #define BNO085_TX_PIN 10
  #define BNO085_USE_SOFTWARE_SERIAL false

#else
  // Default fallback
  #define BNO085_RX_PIN 10
  #define BNO085_TX_PIN 11
  #define BNO085_USE_SOFTWARE_SERIAL true
#endif

// ============================================================================
// UART/Serial Pins for GPS Module
// ============================================================================
// GPS modules communicate via UART (TTL-level serial)
// Configure which UART via platformio.ini: -D GPS_UART_PORT=0|1|2

#if defined(__AVR_ATmega2560__)
  // Arduino Mega: 4 Hardware UARTs available
  // Serial1 (UART0): RX1=19, TX1=18
  // Serial2 (UART1): RX2=17, TX2=16
  // Serial3 (UART2): RX3=15, TX3=14
  // (GPS configuration handled by gps_config.h based on GPS_UART_PORT)
  #define SERIAL1_RX_PIN 19
  #define SERIAL1_TX_PIN 18
  #define SERIAL2_RX_PIN 17
  #define SERIAL2_TX_PIN 16
  #define SERIAL3_RX_PIN 15
  #define SERIAL3_TX_PIN 14

#elif defined(__AVR_ATmega328P__)
  // Arduino Nano / Uno: Only Serial1 available
  // GPS would require SoftwareSerial on user-defined pins
  #define SERIAL1_RX_PIN 0  // RX
  #define SERIAL1_TX_PIN 1  // TX

#elif defined(TEENSY31) || defined(TEENSY32)
  // Teensy 3.x: Multiple UARTs available
  // Serial1 (UART0): RX=0, TX=1
  // Serial2 (UART1): RX=9, TX=10
  // Serial3 (UART2): RX=7, TX=8
  #define SERIAL1_RX_PIN 0
  #define SERIAL1_TX_PIN 1
  #define SERIAL2_RX_PIN 9
  #define SERIAL2_TX_PIN 10
  #define SERIAL3_RX_PIN 7
  #define SERIAL3_TX_PIN 8

#elif defined(ESP32)
  // ESP32: GPIO pins are flexible
  // Serial1 default: RX=9, TX=10
  // Serial2 default: RX=16, TX=17
  #define SERIAL1_RX_PIN 9
  #define SERIAL1_TX_PIN 10
  #define SERIAL2_RX_PIN 16
  #define SERIAL2_TX_PIN 17

#endif

// ============================================================================
// Status LED (optional)
// ============================================================================
// Blink on orientation valid, etc.
#define LED_PIN 13  // Most Arduino boards have LED on pin 13

// ============================================================================
// Output Configuration
// ============================================================================
#define SERIAL_OUTPUT_BAUD 115200
#define SERIAL_OUTPUT_FREQUENCY_HZ 10  // Output 10 times per second (100ms intervals)

#endif  // PINS_H
