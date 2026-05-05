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
