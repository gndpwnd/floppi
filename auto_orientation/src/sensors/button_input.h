/**
 * Button Input Handler
 *
 * Detects button press events with debouncing.
 * Useful for triggering snapshot recording.
 *
 * Only compiled when SNAPSHOT_MODE is enabled.
 *
 * Hardware:
 * - Button connected to GPIO pin (default 2)
 * - Internal pull-up enabled (button pulls pin to GND when pressed)
 * - Rising edge detection (LOW -> HIGH transition)
 *
 * Debouncing:
 * - 20ms debounce delay to prevent false triggers from electrical noise
 * - Call is_pressed() at loop frequency to detect button presses
 *
 * Example Usage:
 *   ButtonInput button;
 *   button.initialize();
 *
 *   void loop() {
 *     if (button.is_pressed()) {
 *       // Button was just pressed
 *       Serial.println("Button pressed!");
 *     }
 *   }
 */

#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include "../config/mode.h"

#if ENABLE_SNAPSHOT_RECORDER

#include <stdint.h>

// ============================================================================
// Configuration
// ============================================================================

// GPIO pin for button input
// Arduino Mega: pin 2 (typically used for external interrupt)
#define BUTTON_INPUT_PIN 2

// Debounce delay in milliseconds
#define BUTTON_DEBOUNCE_MS 20

// ============================================================================
// Button Input Class
// ============================================================================

class ButtonInput {
 public:
  /**
   * Default constructor.
   */
  ButtonInput();

  /**
   * Initialize button input.
   * Must be called in setup() before checking button state.
   *
   * @return true if initialization successful
   *
   * Behavior:
   * - Configures GPIO pin as input with pull-up enabled
   * - Sets up internal state for debouncing
   */
  bool initialize();

  /**
   * Check if button was pressed.
   * Returns true only once per button press (rising edge).
   *
   * @return true if button press detected (edge triggered)
   *
   * Behavior:
   * - Samples button state with debouncing
   * - Returns true on LOW -> HIGH transition
   * - Automatically resets pressed state after returning true
   * - Safe to call every loop() iteration
   *
   * Debouncing:
   * - Waits BUTTON_DEBOUNCE_MS before confirming state change
   * - Prevents glitches from electrical noise
   */
  bool is_pressed();

  /**
   * Get human-readable status string.
   *
   * @return Status message (e.g., "Button ready")
   */
  const char* get_status_string() const { return "Button input ready"; }

 private:
  bool initialized_;
  bool last_state_;           // Last stable state (LOW or HIGH)
  bool pressed_;              // Flag indicating button press detected
  uint32_t last_change_time_; // Time of last state change

  /**
   * Helper: Read raw button state.
   * Does not perform debouncing.
   *
   * @return true if button is currently pressed (pin is LOW)
   */
  bool read_raw_button();
};

#else  // !ENABLE_SNAPSHOT_RECORDER

// Stub implementation when SNAPSHOT_MODE is disabled
class ButtonInput {
 public:
  ButtonInput() {}
  bool initialize() { return true; }
  bool is_pressed() { return false; }
  const char* get_status_string() const { return "Button input disabled"; }
};

#endif  // ENABLE_SNAPSHOT_RECORDER

#endif  // BUTTON_INPUT_H
