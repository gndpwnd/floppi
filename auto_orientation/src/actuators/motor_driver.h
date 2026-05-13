/**
 * Dual-Channel Motor Driver — Abstract Interface
 *
 * Phase 4.7b of the auto_orientation framework. Hardware-agnostic actuator
 * abstraction for two-wheel/two-axis motor outputs (balancing robot, future
 * gimbal applications, differential-drive ground vehicles).
 *
 * The interface intentionally mirrors the *.ino reference behaviour from
 * `docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino`:
 *
 *   - Speed range: signed [-255, +255]. Positive = "forward" for that motor's
 *     wired direction (sign of the input swaps the H-bridge direction pins).
 *   - Stiction floor: implementations may clamp the absolute speed up to a
 *     minimum PWM value when the input is non-zero but below the motor's
 *     break-away torque threshold. This is configurable per-driver.
 *   - Stop vs brake: stop() lets the motors free-coast (both INx LOW, PWM=0).
 *     brake() actively shorts the motor leads (both INx HIGH, PWM=0) for
 *     drivers that support it. Default brake() implementation delegates to
 *     stop() — concrete drivers may override.
 *
 * Concrete implementations live alongside this header:
 *   - l298n_motor_driver.{h,cpp} — L298N H-bridge (Arduino Mega reference)
 *
 * See: docs/findings/MASTER_DESIGN.md §4.7
 */

#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdint.h>

// ============================================================================
// Dual Motor Driver Base Class
// ============================================================================

class DualMotorDriver {
 public:
  virtual ~DualMotorDriver() {}

  /**
   * Initialize the driver hardware (pin modes, enable pins, etc.).
   * @return true on success, false on hardware failure.
   */
  virtual bool begin() = 0;

  /**
   * Set both motor speeds in [-255, +255].
   * Positive = forward for that motor's wired direction.
   * Implementations clamp out-of-range values and apply any stiction floor.
   */
  virtual void set_speeds(int16_t left, int16_t right) = 0;

  /**
   * Convenience: set both motors to the same speed (most common case for
   * balance-bot pitch-axis control).
   */
  void set_speed(int16_t speed) { set_speeds(speed, speed); }

  /**
   * Stop motors — free-coast. Both direction pins LOW, PWM = 0.
   */
  virtual void stop() = 0;

  /**
   * Brake motors — active brake (both direction pins HIGH, PWM = 0) for
   * drivers that support it. Default implementation delegates to stop().
   */
  virtual void brake() { stop(); }

  /**
   * Inspection: last commanded speeds (after clamping and stiction).
   * Useful for telemetry, dashboards, and verifying control loop output.
   */
  virtual int16_t last_left() const = 0;
  virtual int16_t last_right() const = 0;
};

#endif  // MOTOR_DRIVER_H
