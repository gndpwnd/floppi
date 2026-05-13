/**
 * L298N Motor Driver Implementation
 *
 * Ports the controlMotors() / stopMotors() logic from
 * docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino into the
 * DualMotorDriver interface. The legacy behaviour is preserved verbatim:
 *
 *   - Clamp inputs to [-255, 255].
 *   - Stiction floor: |speed| < min_pwm && speed != 0 -> sign(speed) * min_pwm.
 *   - Direction:  speed > 0 -> (in_a HIGH, in_b LOW)
 *                 speed < 0 -> (in_a LOW,  in_b HIGH)
 *                 speed == 0 -> (in_a LOW, in_b LOW)   (free coast)
 *   - Magnitude:  analogWrite(ena, abs(speed)).
 *   - begin():    pinMode OUTPUT all six pins; write ENA/ENB HIGH (some L298N
 *                 modules require this to enable); call stop().
 *   - brake():    both INx HIGH, PWM = 0 — active brake. (Not used by the
 *                 legacy .ino but useful for safety-stop in the balancing app.)
 *
 * Native test support: when UNIT_TEST is defined, the Arduino pin-control
 * functions (pinMode, digitalWrite, analogWrite) and the OUTPUT/HIGH/LOW
 * macros are NOT pulled in via <Arduino.h>. The test binary supplies its
 * own stubs that capture pin operations into an inspectable buffer. See
 * tests/test_l298n_motor.cpp.
 */

#include "l298n_motor_driver.h"

#ifdef UNIT_TEST
// Test build: stubs are provided by the test binary (see test file).
// We just forward-declare them and the AVR-style constants here.
#ifndef OUTPUT
#define OUTPUT  1
#endif
#ifndef HIGH
#define HIGH    1
#endif
#ifndef LOW
#define LOW     0
#endif
extern "C" void pinMode(uint8_t pin, uint8_t mode);
extern "C" void digitalWrite(uint8_t pin, uint8_t value);
extern "C" void analogWrite(uint8_t pin, int value);
#else
#include <Arduino.h>
#endif

#include <stdlib.h>  // for abs()

// ============================================================================
// Helpers
// ============================================================================

namespace {
inline int16_t clamp_speed(int16_t speed) {
  if (speed > 255) return 255;
  if (speed < -255) return -255;
  return speed;
}
}  // namespace

// ============================================================================
// Construction
// ============================================================================

L298NMotorDriver::L298NMotorDriver(const L298NPins& pins, uint8_t stiction_min_pwm)
    : pins_(pins),
      stiction_min_pwm_(stiction_min_pwm),
      last_left_(0),
      last_right_(0),
      initialized_(false) {}

// ============================================================================
// Initialization
// ============================================================================

bool L298NMotorDriver::begin() {
  pinMode(pins_.ena, OUTPUT);
  pinMode(pins_.enb, OUTPUT);
  pinMode(pins_.in1, OUTPUT);
  pinMode(pins_.in2, OUTPUT);
  pinMode(pins_.in3, OUTPUT);
  pinMode(pins_.in4, OUTPUT);

  // Some L298N modules require ENA/ENB held HIGH to enable the H-bridge.
  // (PWM on the same pin still works — analogWrite() reuses the pin in the
  //  next call to set_speeds().) Matches SelfBallancingRobot3.ino lines 77-78.
  digitalWrite(pins_.ena, HIGH);
  digitalWrite(pins_.enb, HIGH);

  initialized_ = true;
  stop();
  return true;
}

// ============================================================================
// Speed Control
// ============================================================================

void L298NMotorDriver::set_speeds(int16_t left, int16_t right) {
  // Clamp first, then apply stiction floor. Matches the constrain()/min-PWM
  // sequence in the legacy controlMotors().
  left  = apply_stiction_(clamp_speed(left));
  right = apply_stiction_(clamp_speed(right));

  drive_channel_(left,  pins_.ena, pins_.in1, pins_.in2);
  drive_channel_(right, pins_.enb, pins_.in3, pins_.in4);

  last_left_  = left;
  last_right_ = right;
}

int16_t L298NMotorDriver::apply_stiction_(int16_t speed) const {
  if (speed == 0 || stiction_min_pwm_ == 0) {
    return speed;
  }
  const int16_t mag = (speed < 0) ? -speed : speed;
  if (mag < static_cast<int16_t>(stiction_min_pwm_)) {
    return (speed > 0) ? static_cast<int16_t>(stiction_min_pwm_)
                       : -static_cast<int16_t>(stiction_min_pwm_);
  }
  return speed;
}

void L298NMotorDriver::drive_channel_(int16_t speed, uint8_t en_pin,
                                       uint8_t in_a, uint8_t in_b) {
  if (speed > 0) {
    digitalWrite(in_a, HIGH);
    digitalWrite(in_b, LOW);
  } else if (speed < 0) {
    digitalWrite(in_a, LOW);
    digitalWrite(in_b, HIGH);
  } else {
    digitalWrite(in_a, LOW);
    digitalWrite(in_b, LOW);
  }
  const int16_t mag = (speed < 0) ? -speed : speed;
  analogWrite(en_pin, static_cast<int>(mag));
}

// ============================================================================
// Stop / Brake
// ============================================================================

void L298NMotorDriver::stop() {
  // Free-coast: PWM=0, both INx LOW for each channel.
  analogWrite(pins_.ena, 0);
  analogWrite(pins_.enb, 0);
  digitalWrite(pins_.in1, LOW);
  digitalWrite(pins_.in2, LOW);
  digitalWrite(pins_.in3, LOW);
  digitalWrite(pins_.in4, LOW);
  last_left_ = 0;
  last_right_ = 0;
}

void L298NMotorDriver::brake() {
  // Active brake: both INx HIGH (shorts the motor leads through the bridge),
  // PWM = 0. Effective only on L298N modules that decouple PWM from the
  // direction logic — most do.
  digitalWrite(pins_.in1, HIGH);
  digitalWrite(pins_.in2, HIGH);
  digitalWrite(pins_.in3, HIGH);
  digitalWrite(pins_.in4, HIGH);
  analogWrite(pins_.ena, 0);
  analogWrite(pins_.enb, 0);
  last_left_ = 0;
  last_right_ = 0;
}
