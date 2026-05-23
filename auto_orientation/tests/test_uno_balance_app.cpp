/**
 * Native Unit Tests for UnoBalanceApp (minimal Uno balance program).
 *
 * Verifies the bare-minimum control pipeline:
 *   - read_imu() applies PITCH_OFFSET_DEG, rejects NaN / out-of-range readings
 *   - step() produces PWM in [PWM_MIN, PWM_MAX] for valid pitch
 *   - step() stops motors when tipped past TIP_CUTOFF_DEG
 *   - abort() latches motors off; subsequent step() returns 0
 *   - With reference constants (Kp=65, Ki=12, Kd=38), a synthetic pitch
 *     trajectory produces non-zero, sign-correct PWM commands
 *
 * Test runner style matches tests/test_balance_app.cpp and
 * tests/test_l298n_motor.cpp (printf + TEST_ASSERT, returns 0 on all-pass).
 *
 * Compile (from repo root):
 *
 *   g++ -std=c++11 -O2 -DUNIT_TEST \
 *       -o tests/test_uno_balance_app \
 *       tests/test_uno_balance_app.cpp \
 *       src/applications/balancing_robot_uno/uno_balance_app.cpp \
 *       src/control/pid_controller.cpp
 *   ./tests/test_uno_balance_app
 */

#include <cstdio>
#include <cstdint>
#include <cmath>

#include "../src/applications/balancing_robot_uno/uno_balance_app.h"
#include "../src/applications/balancing_robot_uno/balance_constants.h"
#include "../src/control/pid_controller.h"
#include "../src/sensors/sensor_base.h"
#include "../src/actuators/motor_driver.h"

// ============================================================================
// Test infrastructure
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                \
  do {                                                                        \
    tests_run++;                                                              \
    if (cond) {                                                               \
      tests_passed++;                                                         \
      printf("  PASS: %s\n", msg);                                            \
    } else {                                                                  \
      tests_failed++;                                                         \
      printf("  FAIL (%s:%d): %s\n", __FILE__, __LINE__, msg);                \
    }                                                                         \
  } while (0)

// ============================================================================
// Mocks
// ============================================================================

class MockIMU : public OrientationSensor {
 public:
  MockIMU() : initialized_(true), readable_(true) {
    od_.pitch_deg = 0.0f;
    od_.roll_deg  = 0.0f;
    od_.yaw_deg   = 0.0f;
    od_.w = 1.0f; od_.x = 0.0f; od_.y = 0.0f; od_.z = 0.0f;
  }

  void set_pitch(float p)   { od_.pitch_deg = p; }
  void set_readable(bool r) { readable_ = r; }

  bool begin() override            { initialized_ = true; return true; }
  void end() override              { initialized_ = false; }
  bool isInitialized() const override { return initialized_; }
  bool read() override             { return readable_; }
  bool hasNewData() const override { return true; }
  const char* name() const override { return "MockIMU"; }
  bool isHealthy() const override  { return true; }
  const char* getStatusString() const override { return "OK"; }

  const OrientationData& getOrientation() const override { return od_; }
  bool setCalibrationProfile(const uint8_t*, uint16_t) override { return true; }
  bool getCalibrationProfile(uint8_t*, uint16_t*) override      { return true; }

 private:
  OrientationData od_;
  bool initialized_;
  bool readable_;
};

class MockMotors : public DualMotorDriver {
 public:
  MockMotors() : last_left_(0), last_right_(0), stops_(0) {}

  bool begin() override { return true; }
  void set_speeds(int16_t l, int16_t r) override {
    last_left_  = l;
    last_right_ = r;
  }
  void stop() override {
    last_left_  = 0;
    last_right_ = 0;
    stops_++;
  }
  int16_t last_left() const override  { return last_left_; }
  int16_t last_right() const override { return last_right_; }
  uint32_t stops() const              { return stops_; }

 private:
  int16_t last_left_;
  int16_t last_right_;
  uint32_t stops_;
};

// ============================================================================
// Tests
// ============================================================================

// 1. read_imu() applies the mounting offset.
static void test_read_imu_applies_offset() {
  printf("\n[test_read_imu_applies_offset]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(0, 0, 0, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  imu.set_pitch(0.0f);  // raw pitch from the sensor
  TEST_ASSERT(app.read_imu(), "read_imu returns true on valid sample");
  // With raw=0 and PITCH_OFFSET_DEG=-8.6 (default), corrected = 0 - (-8.6) = 8.6.
  TEST_ASSERT(std::fabs(app.last_pitch_deg() - (0.0f - PITCH_OFFSET_DEG)) < 0.01f,
              "offset subtracted from raw pitch");
}

// 2. read_imu() rejects out-of-range / NaN readings.
static void test_read_imu_rejects_garbage() {
  printf("\n[test_read_imu_rejects_garbage]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(0, 0, 0, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  imu.set_pitch(NAN);
  TEST_ASSERT(!app.read_imu(), "NaN rejected");

  imu.set_pitch(95.0f);  // > PITCH_SANITY_DEG (90)
  TEST_ASSERT(!app.read_imu(), "out-of-range rejected");
}

// 3. step() stops motors when no valid IMU sample has been captured.
static void test_step_without_imu_stops_motors() {
  printf("\n[test_step_without_imu_stops_motors]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  // No read_imu() call → pitch_valid_ stays false → step() must stop motors.
  int16_t pwm = app.step();
  TEST_ASSERT(pwm == 0, "step() returns 0 when no IMU data");
  TEST_ASSERT(motors.last_left() == 0 && motors.last_right() == 0,
              "motors at 0 when no IMU data");
}

// 4. step() applies tip cutoff.
static void test_step_tip_cutoff() {
  printf("\n[test_step_tip_cutoff]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  // Raw pitch big enough that corrected pitch > TIP_CUTOFF_DEG.
  // corrected = raw - PITCH_OFFSET_DEG = raw - (-8.6) = raw + 8.6
  // pick raw = 30 so corrected = 38.6 > 25
  imu.set_pitch(30.0f);
  TEST_ASSERT(app.read_imu(), "raw 30° accepted by sanity gate");
  int16_t pwm = app.step();
  TEST_ASSERT(pwm == 0, "step() returns 0 when tipped");
  TEST_ASSERT(app.is_tipped(), "is_tipped() flag set");
}

// 5. step() produces sign-correct, in-range PWM under reference gains.
static void test_step_pwm_sign_and_range() {
  printf("\n[test_step_pwm_sign_and_range]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  // Synthetic trajectory: bot leaning +5° corrected (raw = +5 + (-8.6) = -3.6).
  // PID setpoint is 0; error = 0 - 5 = -5; with Kp=65, P term = -325 → clamped
  // to -255 (PWM_MIN). Sign is negative (drive wheels backward to catch fall).
  imu.set_pitch(-3.6f);  // → corrected ≈ +5
  app.read_imu();
  int16_t pwm = app.step();
  TEST_ASSERT(pwm >= PWM_MIN && pwm <= PWM_MAX, "PWM within [-255, +255]");
  TEST_ASSERT(pwm < 0, "positive lean produces negative PWM (corrective)");

  // Other side: lean -5° corrected → positive PWM.
  imu.set_pitch(-13.6f);  // → corrected ≈ -5
  app.read_imu();
  pwm = app.step();
  TEST_ASSERT(pwm >= PWM_MIN && pwm <= PWM_MAX, "PWM within [-255, +255] (neg lean)");
  TEST_ASSERT(pwm > 0, "negative lean produces positive PWM (corrective)");
}

// 6. Both wheels driven equally.
static void test_step_drives_both_wheels_symmetric() {
  printf("\n[test_step_drives_both_wheels_symmetric]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  imu.set_pitch(-5.6f);  // small lean +3°
  app.read_imu();
  app.step();
  TEST_ASSERT(motors.last_left() == motors.last_right(),
              "both wheels receive identical PWM");
}

// 7. read_imu() failure → motors stop on the next step().
//    Regression for the "I2C glitch → silent quit" path. P1 from
//    docs/findings/audit_uno_minimal_2026-05-19.md §4.
static void test_read_imu_fails_motors_stop() {
  printf("\n[test_read_imu_fails_motors_stop]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  // First, prime with a valid sample so pitch_valid_ becomes true.
  imu.set_pitch(-3.6f);  // corrected ≈ +5°
  TEST_ASSERT(app.read_imu(), "valid pre-fail read succeeds");
  int16_t pwm = app.step();
  TEST_ASSERT(pwm != 0, "PID drives motors with valid pitch");

  // Now simulate I²C failure: imu.read() returns false. read_imu() must
  // invalidate the cached pitch — even though a previous sample was good.
  imu.set_readable(false);
  TEST_ASSERT(!app.read_imu(), "read_imu returns false on imu.read() failure");

  // Next step() must stop motors and report 0 PWM. The bot must NOT keep
  // driving on the stale-but-valid prior sample.
  pwm = app.step();
  TEST_ASSERT(pwm == 0, "step() returns 0 after read failure");
  TEST_ASSERT(motors.last_left() == 0 && motors.last_right() == 0,
              "motors halted after read failure");
}

// 7b. read_fail_count() tracks consecutive failures and clears on a good read.
//     P1 from audit §4: surface sensor-health telemetry so a dying BNO055 link
//     doesn't present as "bot just tips for no reason".
static void test_read_fail_count_tracks_and_clears() {
  printf("\n[test_read_fail_count_tracks_and_clears]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  TEST_ASSERT(app.read_fail_count() == 0, "counter starts at 0");

  // Three consecutive I2C failures.
  imu.set_readable(false);
  app.read_imu();
  app.read_imu();
  app.read_imu();
  TEST_ASSERT(app.read_fail_count() == 3, "counter increments per failed read");

  // An out-of-range (non-I2C) failure also counts.
  imu.set_readable(true);
  imu.set_pitch(120.0f);  // > PITCH_SANITY_DEG
  app.read_imu();
  TEST_ASSERT(app.read_fail_count() == 4, "out-of-range read also counts as failure");

  // A good read clears the counter.
  imu.set_pitch(-3.6f);  // valid corrected ≈ +5°
  TEST_ASSERT(app.read_imu(), "good read succeeds");
  TEST_ASSERT(app.read_fail_count() == 0, "good read clears the failure counter");
}

// 8. After a tip cutoff trips and the bot is righted, step() should resume
//    balancing with NO stale integrator wind-up. The PID is reset on the tick
//    that detects the tip, so the next post-righting step() should produce
//    PWM equal to a fresh-bot PID response at that pitch.
//    P1 from audit §4 / §6.
static void test_tip_recovery_clears_integral() {
  printf("\n[test_tip_recovery_clears_integral]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  // Reference baseline: drive the PID at a steady small pitch, just to record
  // the "fresh PID" PWM for comparison.
  PIDController ref_pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  ref_pid.set_tunings(BALANCE_KP, BALANCE_KI, BALANCE_KD);
  ref_pid.set_sample_time(PID_SAMPLE_MS);
  ref_pid.set_output_limits((float)PWM_MIN, (float)PWM_MAX);
  ref_pid.set_setpoint(0.0f);
  ref_pid.set_d_term_lpf_tau_sec(0.0f);
  ref_pid.reset();
  // Single fresh step at corrected pitch = +3°
  const float fresh_pwm = ref_pid.compute(3.0f, PID_SAMPLE_MS);

  // 1) Drive several steps at a large in-bounds pitch to wind up the I-term.
  //    corrected = raw - PITCH_OFFSET_DEG, so raw = corrected + PITCH_OFFSET_DEG.
  imu.set_pitch(15.0f + PITCH_OFFSET_DEG);  // corrected ≈ +15°
  for (int i = 0; i < 30; ++i) {
    app.read_imu();
    app.step();
  }

  // 2) Trip the cutoff. PID must be reset by step() on the trip tick.
  imu.set_pitch(40.0f);  // corrected ≈ +48.6 > TIP_CUTOFF_DEG
  TEST_ASSERT(app.read_imu(), "tip-magnitude raw read accepted");
  app.step();
  TEST_ASSERT(app.is_tipped(), "tip flag set");

  // 3) Right the bot to a small lean and step once. PID output should match
  //    the fresh-PID baseline within a small tolerance — i.e. the wind-up
  //    accumulated in (1) was discarded by the reset in (2).
  //    Note: production main.cpp calls set_d_term_lpf_tau_sec(0.0f) after
  //    app.begin(). We mirror that here so the comparison against fresh_pwm
  //    (which uses tau=0) is apples-to-apples.
  pid.set_d_term_lpf_tau_sec(0.0f);
  imu.set_pitch(3.0f + PITCH_OFFSET_DEG);  // corrected = +3°
  TEST_ASSERT(app.read_imu(), "post-righting read accepted");
  int16_t pwm = app.step();
  TEST_ASSERT(!app.is_tipped(), "tipped flag clears after righting");
  // After integral reset, first post-righting PWM should be within ~10 counts
  // of the fresh-PID response (D-term acts on derivative of measurement,
  // which is identical between fresh and reset PID at first sample).
  const float diff = std::fabs((float)pwm - fresh_pwm);
  TEST_ASSERT(diff < 10.0f, "post-righting PWM ~ fresh-PID PWM (integral cleared)");
}

// 9. Re-arming after abort() restores balancing.
//    P1 from audit §4: serial 'g' must let the operator recover from 'a' without
//    reflashing.
static void test_arm_recovers_after_abort() {
  printf("\n[test_arm_recovers_after_abort]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  imu.set_pitch(-3.6f);  // corrected ≈ +5°
  app.read_imu();

  // Abort: latch disarm.
  app.abort();
  TEST_ASSERT(!app.is_armed(), "armed cleared by abort");
  TEST_ASSERT(app.step() == 0, "step returns 0 while disarmed");

  // Re-arm via the new arm() entry point.
  app.arm();
  TEST_ASSERT(app.is_armed(), "armed re-asserted by arm()");
  TEST_ASSERT(!app.is_tipped(), "arm() clears tipped flag");

  // After re-arm with valid pitch, step() must drive motors again.
  app.read_imu();
  int16_t pwm = app.step();
  TEST_ASSERT(pwm != 0, "step() drives motors after re-arm");
  TEST_ASSERT(motors.last_left() == motors.last_right(),
              "motors still symmetric after re-arm");
}

// 10. abort() latches motors off.
static void test_abort_latches() {
  printf("\n[test_abort_latches]\n");
  MockIMU imu;
  MockMotors motors;
  PIDController pid(BALANCE_KP, BALANCE_KI, BALANCE_KD, PWM_MIN, PWM_MAX);
  UnoBalanceApp app(imu, motors, pid);
  app.begin();

  imu.set_pitch(-3.6f);
  app.read_imu();
  app.abort();
  TEST_ASSERT(!app.is_armed(), "armed flag cleared by abort()");

  // Even with fresh valid pitch, step() refuses to drive.
  app.read_imu();
  int16_t pwm = app.step();
  TEST_ASSERT(pwm == 0, "step() returns 0 after abort()");
  TEST_ASSERT(motors.last_left() == 0 && motors.last_right() == 0,
              "motors stay at 0 after abort()");
}

// ============================================================================
// main
// ============================================================================

int main() {
  printf("=== UnoBalanceApp native tests ===\n");
  printf("Defaults: Kp=%.1f Ki=%.1f Kd=%.1f offset=%.2f tip=%.1f stiction=%u\n",
         BALANCE_KP, BALANCE_KI, BALANCE_KD,
         PITCH_OFFSET_DEG, TIP_CUTOFF_DEG, (unsigned)STICTION_PWM);

  test_read_imu_applies_offset();
  test_read_imu_rejects_garbage();
  test_step_without_imu_stops_motors();
  test_step_tip_cutoff();
  test_step_pwm_sign_and_range();
  test_step_drives_both_wheels_symmetric();
  test_read_imu_fails_motors_stop();
  test_read_fail_count_tracks_and_clears();
  test_tip_recovery_clears_integral();
  test_arm_recovers_after_abort();
  test_abort_latches();

  printf("\n=== Results: %d/%d passed, %d failed ===\n",
         tests_passed, tests_run, tests_failed);
  return tests_failed == 0 ? 0 : 1;
}
