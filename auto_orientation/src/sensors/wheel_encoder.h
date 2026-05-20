/**
 * Wheel Encoder Driver (Phase 4M.10 — research_wheel_encoders_mega_2026-05-19)
 *
 * Quadrature wheel encoder for the Mega balance bot. Wraps the PJRC
 * `Encoder` library (PaulStoffregen/Encoder@^1.4.4) for ISR-based 4× counting
 * on Mega INT pins, and adds the velocity / distance / stall API that the
 * balance app needs for the outer-loop velocity cascade described in
 * `docs/findings/research_wheel_encoders_mega_2026-05-19.md` §6.
 *
 * Wiring (Mega):
 *
 *   Signal   Pin   INT vector   Notes
 *   -----    ---   ----------   -----
 *   L_ENC_A  18    INT5         pull-up enabled internally
 *   L_ENC_B  19    INT4         pull-up enabled internally
 *   R_ENC_A  2     INT0         pull-up enabled internally
 *   R_ENC_B  3     INT1         pull-up enabled internally
 *
 * Library:  PaulStoffregen/Encoder@^1.4.4 (lib_deps in mega_balance env)
 * Compiled only when `USE_WHEEL_ENCODERS` is defined (auto-defined by the
 * mega_balance env via -DUSE_WHEEL_ENCODERS). On other envs this TU stays
 * empty; the header is safely includable everywhere.
 *
 * Hardware bring-up notes (see also the research doc, §5 and §8):
 *
 *   1. Calibration procedure
 *      - counts_per_rev (CPR): rotate the wheel by hand exactly one
 *        full turn while watching read_ticks() output. The delta IS the CPR.
 *        Typical values: yellow-TT motor 8 PPR × 120:1 gearbox ≈ 960 CPR;
 *        N20 metal gear 7 PPR × 100:1 ≈ 2800 CPR.
 *      - wheel_radius_m: measure the wheel radius from hub centre to tread,
 *        in metres (e.g. 65 mm wheel → r = 0.0325 m).
 *      - Both values persist to EEPROM slot 0x220 (Phase 4M.11 — owned by a
 *        separate agent's serial-command implementation).
 *
 *   2. Direction check
 *      - Push the bot FORWARD by hand: read_ticks() should INCREASE on both
 *        wheels. If a wheel reads the wrong sign, swap its A/B pins in the
 *        constructor — software inversion is not provided (intentional:
 *        keeps ISR fast and the wiring docs unambiguous).
 *
 *   3. Integration with balance_app
 *      - Velocity feedback enters at `step_run_()` where pid_.set_setpoint()
 *        is called — replaces the option-B pitch-double-integrator from
 *        research_imu_only_position_containment.md §5. See the integration
 *        sketch in research_wheel_encoders_mega_2026-05-19.md §6 and
 *        MEGA_UNIVERSAL_PLAN.md.
 *      - Freeze position integration during BOOTSTRAP / HELD / FALLEN.
 *
 *   4. Pin conflict warning
 *      - Pins 18/19 are also Mega Serial1 TX/RX. The balance build does NOT
 *        use Serial1 (no GPS). If GPS is later enabled on Mega, L_ENC pins
 *        MUST move.
 *      - Pins 20/21 are I²C (BNO055) — DO NOT repurpose for encoders.
 */

#ifndef WHEEL_ENCODER_H
#define WHEEL_ENCODER_H

#include <stdint.h>

class WheelEncoder {
 public:
  /**
   * Default counts-per-revolution. Typical for a yellow-TT motor's
   * 8 PPR × 120:1 gearbox with 4× quadrature counting (8 × 120 = 960). Override
   * via set_counts_per_rev() once calibrated on the bench.
   */
  static const int kDefaultCPR = 960;

  /**
   * Default wheel radius in metres. Matches a 65 mm-diameter yellow-TT wheel.
   * Override via set_wheel_radius_m() to match the actual hardware.
   */
  static constexpr float kDefaultRadiusM = 0.0325f;

  /**
   * Default velocity window. Forward-difference of ticks over this window
   * yields a single deg/s sample. See research §4 for the rationale (100 ms
   * gives ~47 counts at walking speed → ~2% resolution noise).
   */
  static const uint16_t kDefaultVelocityWindowMs = 100;

  /**
   * Construct a WheelEncoder for a single wheel.
   *
   * @param pin_a  Channel-A digital pin (MUST be a Mega external-INT pin:
   *               2, 3, 18, or 19).
   * @param pin_b  Channel-B digital pin (MUST be a Mega external-INT pin).
   *
   * Constructor only stashes the pin numbers. begin() does the pinMode /
   * attachInterrupt work via the PJRC Encoder library.
   */
  WheelEncoder(uint8_t pin_a, uint8_t pin_b);

  ~WheelEncoder();

  // ------------------------------------------------------------------------
  // Lifecycle
  // ------------------------------------------------------------------------

  /**
   * Allocate the underlying PJRC Encoder instance and attach interrupts on
   * pin_a / pin_b. Returns true on success. Idempotent — calling begin()
   * twice is harmless (returns true on the second call without re-attaching).
   *
   * On NATIVE_TEST builds this is a no-op that always returns true; the
   * internal tick counter is driven by set_ticks_for_test() instead.
   */
  bool begin();

  // ------------------------------------------------------------------------
  // Tick + velocity reads
  // ------------------------------------------------------------------------

  /**
   * Raw quadrature tick count since boot (or since the last reset_ticks()
   * call). Signed: positive when wheel moves in the "forward" direction
   * defined by the A-leading-B convention of the underlying motor encoder.
   *
   * On AVR this is read atomically: the underlying PJRC Encoder library
   * wraps its int32_t accessor in noInterrupts()/interrupts() for torn-read
   * protection (equivalent to ATOMIC_BLOCK on AVR).
   */
  int32_t read_ticks();

  /**
   * Zero the tick counter and the velocity-window state. Safe to call at
   * any time; does not affect the underlying ISR table.
   */
  void reset_ticks();

  /**
   * Instantaneous wheel angular velocity in degrees per second, derived as
   * the forward-difference of ticks over the configured velocity window
   * (default 100 ms). When called more frequently than the window, returns
   * the last-computed sample without recomputing — this avoids amplifying
   * tick-quantisation noise.
   *
   * NATIVE_TEST builds: the timestamp is supplied via the now_ms parameter
   * so tests can synthesise arbitrary windows. Arduino builds also accept
   * a caller-supplied now_ms — pass millis() at the call site so the driver
   * does not pull in <Arduino.h> via this header.
   */
  int32_t read_velocity_dps(uint32_t now_ms);

  /**
   * Linear wheel-tread velocity in m/s. Uses wheel_radius_m_:
   *   v_mps = (dps * π / 180) * radius_m
   */
  float read_velocity_mps(uint32_t now_ms);

  /**
   * Total distance travelled by the wheel tread since the last reset_ticks().
   * Calculated from the current tick count:
   *   d_m = (ticks / cpr) * 2π * radius_m
   */
  float distance_m();

  // ------------------------------------------------------------------------
  // Stall detection
  // ------------------------------------------------------------------------

  /**
   * Report a commanded PWM value to the encoder for stall detection. Should
   * be called every PID tick with the absolute value of the latest motor
   * PWM command. The encoder snapshots the last time PWM exceeded the
   * stall-detection threshold; stalled() then compares against that.
   *
   * Decoupling this from the motor driver keeps the encoder library-free of
   * dependencies on the actuator layer.
   */
  void report_commanded_pwm(uint16_t pwm_abs, uint32_t now_ms);

  /**
   * True if the motor has been commanded above pwm_threshold for at least
   * time_ms continuous milliseconds but the wheel has not moved (|velocity|
   * below kStallVelocityDps). False otherwise.
   *
   * Default thresholds chosen to match the research doc §6 stall criterion.
   */
  bool stalled(uint16_t pwm_threshold = 100, uint16_t time_ms = 200);

  /** Velocity (in dps at the wheel) below which the wheel is "not moving". */
  static constexpr float kStallVelocityDps = 5.0f;

  // ------------------------------------------------------------------------
  // Calibration setters / getters
  // ------------------------------------------------------------------------

  void set_counts_per_rev(int cpr) { cpr_ = (cpr > 0) ? cpr : kDefaultCPR; }
  int  counts_per_rev() const      { return cpr_; }

  void  set_wheel_radius_m(float r) { wheel_radius_m_ = (r > 0.0f) ? r : kDefaultRadiusM; }
  float wheel_radius_m() const      { return wheel_radius_m_; }

  void     set_velocity_window_ms(uint16_t ms) { velocity_window_ms_ = (ms > 0) ? ms : kDefaultVelocityWindowMs; }
  uint16_t velocity_window_ms() const          { return velocity_window_ms_; }

  // ------------------------------------------------------------------------
  // Test-only hooks (enabled only on NATIVE_TEST builds)
  // ------------------------------------------------------------------------

#ifdef NATIVE_TEST
  /**
   * Inject a synthetic tick count. Used by the native test rig to drive the
   * driver without actually firing interrupts.
   */
  void set_ticks_for_test(int32_t t) { mock_ticks_ = t; }

  /**
   * Read the mock tick value directly (bypasses any millis() dependency in
   * read_ticks()). Useful in assertions.
   */
  int32_t mock_ticks_for_test() const { return mock_ticks_; }
#endif

 private:
  // Construction-time config
  uint8_t  pin_a_;
  uint8_t  pin_b_;

  // Calibration
  int      cpr_;
  float    wheel_radius_m_;
  uint16_t velocity_window_ms_;

  // Velocity-window state
  int32_t  last_velocity_ticks_;
  uint32_t last_velocity_ms_;
  int32_t  last_velocity_dps_;

  // Stall-detection state
  uint32_t pwm_above_threshold_since_ms_;  // 0 means "not above threshold"
  uint16_t last_pwm_abs_;
  uint32_t last_pwm_report_ms_;

  // Backend pointer (PJRC Encoder*) on Arduino builds; nullptr on NATIVE_TEST.
  void*    backend_;
  bool     initialized_;

#ifdef NATIVE_TEST
  int32_t  mock_ticks_;
#endif
};

#endif  // WHEEL_ENCODER_H
