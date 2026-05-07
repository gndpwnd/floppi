/**
 * Button Input Implementation
 *
 * Handles GPIO button detection with debouncing.
 */

#include "button_input.h"

#if ENABLE_SNAPSHOT_RECORDER

#include <Arduino.h>

// ============================================================================
// Constructor
// ============================================================================

ButtonInput::ButtonInput()
    : initialized_(false),
      last_state_(HIGH),  // Default to unpressed (pull-up active)
      pressed_(false),
      last_change_time_(0) {
}

// ============================================================================
// Initialization
// ============================================================================

bool ButtonInput::initialize() {
  // Configure pin as input with pull-up
  pinMode(BUTTON_INPUT_PIN, INPUT_PULLUP);

  initialized_ = true;
  last_state_ = read_raw_button();
  last_change_time_ = millis();

  Serial.println("✓ Button input initialized");
  return true;
}

// ============================================================================
// Button Detection
// ============================================================================

bool ButtonInput::is_pressed() {
  if (!initialized_) {
    return false;
  }

  bool current_state = read_raw_button();
  uint32_t now = millis();

  // Check if state has changed
  if (current_state != last_state_) {
    // Start debounce timer
    if (last_change_time_ == 0) {
      last_change_time_ = now;
    }

    // Check if debounce delay has elapsed
    if (now - last_change_time_ >= BUTTON_DEBOUNCE_MS) {
      last_state_ = current_state;

      // Detect rising edge (button press: HIGH -> LOW, pull-up active)
      // Button is pressed when pin reads LOW
      if (current_state == LOW) {
        pressed_ = true;
      }

      last_change_time_ = 0;  // Reset debounce timer
    }
  } else {
    // State is stable, reset debounce timer
    last_change_time_ = 0;
  }

  // Return pressed state (one-time only per press)
  if (pressed_) {
    pressed_ = false;
    return true;
  }

  return false;
}

// ============================================================================
// Private Helper Functions
// ============================================================================

bool ButtonInput::read_raw_button() {
  // Button pulls pin LOW when pressed (pull-up enabled)
  // Return true if button is pressed (LOW), false if released (HIGH)
  return digitalRead(BUTTON_INPUT_PIN) == LOW;
}

#endif  // ENABLE_SNAPSHOT_RECORDER
