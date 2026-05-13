/**
 * L298N Dual H-Bridge Motor Driver
 *
 * Concrete DualMotorDriver implementation for the L298N module — the workhorse
 * H-bridge used in the balancing-robot reference application.
 *
 * Pinout (Arduino Mega reference, from SelfBallancingRobot3.ino):
 *   Motor A (Left):  ENA = 5, IN1 = 6, IN2 = 7
 *   Motor B (Right): ENB = 10, IN3 = 9, IN4 = 8
 *
 * Wiring contract:
 *   ENA/ENB  — PWM-capable digital pins (driver writes 0..255).
 *   INx pins — direction control. HIGH/LOW on (INa, INb) sets the H-bridge
 *              direction for that channel. (LOW, LOW) = free coast.
 *              (HIGH, HIGH) = active brake.
 *
 * Stiction-deadband behaviour (preserved from the legacy .ino):
 *   If the requested speed magnitude is non-zero but below the configured
 *   stiction floor (default 15), the magnitude is snapped up to the floor.
 *   This guarantees the motor has enough torque to actually move — below
 *   ~PWM 15 the wheels hum but do not turn. Sign (direction) is preserved.
 *
 * Reference: docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino
 *            (see controlMotors() and stopMotors() — this driver is a direct
 *            port of that pattern wrapped in the DualMotorDriver interface.)
 *
 * See: docs/findings/MASTER_DESIGN.md §4.7
 */

#ifndef L298N_MOTOR_DRIVER_H
#define L298N_MOTOR_DRIVER_H

#include <stdint.h>
#include "motor_driver.h"

// ============================================================================
// Pin Configuration
// ============================================================================

/**
 * L298N pin assignments. Two H-bridge channels, each with one PWM enable pin
 * and two direction pins.
 *
 * Arduino Mega reference values (from the .ino):
 *   ena = 5,  in1 = 6, in2 = 7
 *   enb = 10, in3 = 9, in4 = 8
 */
struct L298NPins {
  uint8_t ena;  // Motor A enable (PWM)
  uint8_t in1;  // Motor A direction A
  uint8_t in2;  // Motor A direction B
  uint8_t enb;  // Motor B enable (PWM)
  uint8_t in3;  // Motor B direction A
  uint8_t in4;  // Motor B direction B
};

// ============================================================================
// L298N Driver
// ============================================================================

class L298NMotorDriver : public DualMotorDriver {
 public:
  /**
   * Construct an L298N driver.
   *
   * @param pins             Pin assignment for both channels.
   * @param stiction_min_pwm Minimum PWM magnitude for non-zero speeds.
   *                         Default 15 matches SelfBallancingRobot3.ino.
   *                         Pass 0 to disable the stiction floor entirely.
   */
  L298NMotorDriver(const L298NPins& pins, uint8_t stiction_min_pwm = 15);

  // DualMotorDriver interface
  bool begin() override;
  void set_speeds(int16_t left, int16_t right) override;
  void stop() override;
  void brake() override;
  int16_t last_left() const override { return last_left_; }
  int16_t last_right() const override { return last_right_; }

  /**
   * Inspection helpers (mostly for tests and diagnostics).
   */
  const L298NPins& pins() const { return pins_; }
  uint8_t stiction_min_pwm() const { return stiction_min_pwm_; }
  bool isInitialized() const { return initialized_; }

 private:
  L298NPins pins_;
  uint8_t stiction_min_pwm_;
  int16_t last_left_;
  int16_t last_right_;
  bool initialized_;

  /**
   * Apply the stiction floor: if |speed| is non-zero but less than the
   * configured minimum, snap to ±min preserving sign. Speed 0 stays 0.
   */
  int16_t apply_stiction_(int16_t speed) const;

  /**
   * Drive a single H-bridge channel: set direction pins from the speed sign
   * and write |speed| to the enable PWM pin.
   */
  void drive_channel_(int16_t speed, uint8_t en_pin,
                      uint8_t in_a, uint8_t in_b);
};

#endif  // L298N_MOTOR_DRIVER_H
