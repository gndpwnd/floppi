/**
 * UnoBalanceApp — minimal self-balancing controller for Arduino Uno.
 *
 * Mission: be as small and predictable as the reference sketch
 *   docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino
 * while consuming the framework's BNO055 and L298N drivers. No state
 * machine, no auto-tune, no learning, no mounting estimator. The control
 * pipeline is exactly:
 *
 *     pitch = BNO055.read() - PITCH_OFFSET_DEG
 *     if |pitch| > TIP_CUTOFF_DEG  -> stop motors
 *     else                          -> PWM = PID(pitch); drive both wheels
 *
 * Constants live in balance_constants.h, which the offline Python brute-force
 * tuner overwrites between flashes. Nothing in this class is parameterized
 * at runtime — change a value, recompile, reflash.
 *
 * Threading: step() is intended to be called from the MsTimer2 5 ms ISR.
 * read_imu() is called from loop() at a slower cadence (the BNO055 internal
 * fusion rate is ~100 Hz; reading it every PID tick would just block). The
 * latest pitch is double-buffered into a volatile so the ISR sees a
 * consistent value without disabling interrupts.
 *
 * Safety:
 *   - Emergency stop ('a' over serial) sets armed=false. Motors are forced
 *     off until the operator re-arms (re-flash or future serial command).
 *   - Tip cutoff: |pitch| > TIP_CUTOFF_DEG forces motors off but keeps the
 *     loop alive — when the bot is righted, balancing resumes automatically.
 *   - Sensor failure: if the BNO055 read returns NaN or out-of-range pitch,
 *     motors are stopped and the PID integral is reset.
 */

#ifndef UNO_BALANCE_APP_H
#define UNO_BALANCE_APP_H

#include <stdint.h>

#include "../../sensors/sensor_base.h"
#include "../../actuators/motor_driver.h"
#include "../../control/pid_controller.h"

class UnoBalanceApp {
 public:
  /**
   * Construct with references to already-instantiated framework modules.
   * The app owns no hardware — the embedder builds the BNO055 / L298N /
   * PID and hands them in.
   */
  UnoBalanceApp(OrientationSensor& imu,
                DualMotorDriver& motors,
                PIDController& pid);

  /**
   * Initialize the PID with the constants from balance_constants.h.
   * Call once from setup(), AFTER the IMU and motor driver have begun().
   */
  void begin();

  /**
   * Refresh the cached pitch from the IMU. Call from loop() — does an
   * I2C transaction so it must NOT run inside the 5 ms PID ISR.
   * Returns true if a fresh, valid pitch was captured.
   */
  bool read_imu();

  /**
   * One PID tick. Called from the 5 ms MsTimer2 ISR. Reads the cached
   * pitch, runs PID.compute(), writes the result to both motors.
   * Returns the PWM that was applied (signed).
   */
  int16_t step();

  /**
   * Stop motors and freeze the loop (emergency stop). Trigger via 'a' over
   * serial. Stays disarmed until the firmware is reflashed.
   */
  void abort();

  /**
   * Inspection accessors for serial telemetry. Safe to call from loop().
   */
  bool   is_armed()        const { return armed_; }
  bool   is_tipped()       const { return tipped_; }
  float  last_pitch_deg()  const { return last_pitch_deg_; }
  int16_t last_pwm()       const { return last_pwm_; }

 private:
  OrientationSensor& imu_;
  DualMotorDriver&   motors_;
  PIDController&     pid_;

  // Latest IMU pitch (corrected for mounting offset). Written by read_imu()
  // from loop(), consumed by step() in ISR context. Volatile + float read on
  // 8-bit AVR is technically multi-byte and could tear — but at 200 Hz with
  // a slow-changing physical quantity, a torn read is at worst one stale tick
  // and the next sample corrects it. Avoiding noInterrupts() in the loop()
  // path is worth that tradeoff for this minimal program.
  volatile float last_pitch_deg_;
  volatile bool  pitch_valid_;

  // Last commanded PWM (for telemetry).
  volatile int16_t last_pwm_;

  // Armed / tipped state (no sticky FALLEN — when un-tipped, balancing resumes).
  volatile bool armed_;
  volatile bool tipped_;
};

#endif  // UNO_BALANCE_APP_H
