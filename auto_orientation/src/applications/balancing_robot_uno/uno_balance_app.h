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
   * Initialize the PID gains. Boot precedence: a valid EEPROM tune block
   * (via tune_storage::load_tuning) wins; otherwise the balance_constants.h
   * seed (BALANCE_KP/KI/KD) is used. Prints which source won.
   * Call once from setup(), AFTER the IMU and motor driver have begun().
   */
  void begin();

  /**
   * Push live PID gains at runtime. Used by the guided-tuning session
   * (UT-C) to apply gains during a tuning stage. Also updates the cached
   * work-gains returned by get_work_gains().
   */
  void apply_gains(float kp, float ki, float kd);

  /**
   * Read back the currently-applied PID gains (for telemetry / status).
   */
  void get_work_gains(float& kp, float& ki, float& kd) const;

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
   * serial. Stays disarmed until re-armed via arm() (e.g. serial 'g') or a
   * reflash.
   */
  void abort();

  /**
   * Re-arm after abort() — clears the disarmed latch, resets the PID integral
   * so we don't slam the motors on the first post-arm tick, and lets step()
   * drive the wheels again on the next valid pitch sample. Operator preference
   * is "balance forever", so this is the supported way to recover from an 'a'
   * stop without reflashing.
   *
   * Refuse-to-arm: in the flight build (`#ifndef UNO_GUIDED_TUNING`), if
   * begin() detected no EEPROM cal AND no hand-pasted BNO055_CAL_BLOB seed,
   * arm() rejects the request (prints diagnostic, returns without engaging).
   * Use force_clear_cal_block() — wired to the 'F' serial command — to
   * override at the operator's risk.
   */
  void arm();

  /**
   * Force-clear the cal-missing arm latch (flight build only). Lets the
   * operator override the refuse-to-arm safety with a known-good IMU when
   * the EEPROM/seed cal is intentionally absent. No-op when the latch is
   * already clear.
   */
  void force_clear_cal_block() { cal_missing_block_arm_ = false; }

  /**
   * True iff begin() set the refuse-to-arm latch (flight build only — always
   * false in the tuning build, where the operator can run 'c' to calibrate).
   */
  bool cal_missing_blocks_arm() const { return cal_missing_block_arm_; }

  /**
   * Inspection accessors for serial telemetry. Safe to call from loop().
   */
  bool   is_armed()        const { return armed_; }
  bool   is_tipped()       const { return tipped_; }
  float  last_pitch_deg()  const { return last_pitch_deg_; }
  // last_pwm_ is written from the ISR; a 16-bit load tears on 8-bit AVR, so
  // copy it under a critical section (out-of-line in the .cpp where the
  // ATOMIC_BLOCK shim lives). Telemetry-only, but cheap to do right.
  int16_t last_pwm()       const;

  /**
   * Consecutive read_imu() failures (NaN / out-of-range / I2C error). Cleared
   * to 0 on the next good read. A slowly-dying BNO055 presents as the bot
   * tipping with no obvious cause; surfacing this count in the 's' status line
   * gives the operator a clue. Matches the reference .ino's consecutiveErrors.
   * Saturates at 255 — we only care whether it's 0 vs. "lots".
   */
  uint8_t read_fail_count() const { return read_fail_count_; }

 private:
  OrientationSensor& imu_;
  DualMotorDriver&   motors_;
  PIDController&     pid_;

  // Latest IMU pitch (corrected for mounting offset). Written by read_imu()
  // from loop(), consumed by step() in ISR context. A 4-byte float load on
  // 8-bit AVR is multi-byte and can tear if the ISR fires mid-load — and a
  // torn pitch multiplied by Kp slams the motors for one tick. read_imu()
  // (writer) and step() (reader) therefore guard the publish/snapshot with
  // ATOMIC_BLOCK(ATOMIC_RESTORESTATE); see uno_balance_app.cpp file-top notes.
  volatile float last_pitch_deg_;
  volatile bool  pitch_valid_;

  // Last commanded PWM (for telemetry).
  volatile int16_t last_pwm_;

  // Armed / tipped state (no sticky FALLEN — when un-tipped, balancing resumes).
  volatile bool armed_;
  volatile bool tipped_;

  // Currently-applied PID gains. Set by begin() (EEPROM or seed) and updated
  // by apply_gains(); read back via get_work_gains() for telemetry/status.
  float cur_kp_;
  float cur_ki_;
  float cur_kd_;

  // Consecutive read_imu() failures, saturating at 255. Written only from
  // loop() (read_imu), read from loop() telemetry — no ISR involvement, so a
  // plain uint8_t (atomic on AVR) suffices.
  uint8_t read_fail_count_;

  // Refuse-to-arm latch: set by begin() in the flight build when neither an
  // EEPROM cal blob NOR a hand-pasted BNO055_CAL_BLOB seed is available, so
  // arm() rejects until force_clear_cal_block() ('F' serial command) is
  // called. Always false in the tuning build (no flight latency that path).
  bool cal_missing_block_arm_;
};

#endif  // UNO_BALANCE_APP_H
