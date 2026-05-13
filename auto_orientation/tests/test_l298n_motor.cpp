/**
 * Native Unit Tests for L298NMotorDriver
 *
 * Phase 4.7b — verifies the L298N pin-control behaviour against the legacy
 * `controlMotors()` / `stopMotors()` logic from
 * docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino.
 *
 * Strategy: pin-mock. The driver's .cpp is compiled with -DUNIT_TEST, which
 * drops the <Arduino.h> include. This file supplies stub implementations of
 * pinMode / digitalWrite / analogWrite that record every call into an
 * inspectable buffer (MockPinState). Each test resets the buffer, drives
 * the L298N, and asserts on the captured pin levels and PWM duty cycles.
 *
 * Test style follows the existing project convention (see
 * tests/test_coordinates_standalone.cpp): printf-based with a TEST_ASSERT
 * macro and a tests_run / tests_passed / tests_failed counter. Returns 0
 * on all-pass, 1 on any failure — suitable for CI.
 *
 * Compile (from repo root):
 *
 *   g++ -std=c++11 -O2 -DUNIT_TEST -Isrc/actuators \
 *       -o tests/test_l298n_motor \
 *       tests/test_l298n_motor.cpp \
 *       src/actuators/l298n_motor_driver.cpp
 *   ./tests/test_l298n_motor
 */

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "../src/actuators/l298n_motor_driver.h"

// ============================================================================
// Arduino pin function stubs (capture pin operations into a state buffer)
// ============================================================================

#ifndef OUTPUT
#define OUTPUT  1
#endif
#ifndef HIGH
#define HIGH    1
#endif
#ifndef LOW
#define LOW     0
#endif

// Mega has pins 0..69 — 128 entries is plenty for any plausible mapping.
static const int MOCK_NUM_PINS = 128;

struct MockPinState {
  uint8_t mode[MOCK_NUM_PINS];      // last value passed to pinMode()
  uint8_t digital[MOCK_NUM_PINS];   // last value passed to digitalWrite()
  int     pwm[MOCK_NUM_PINS];       // last value passed to analogWrite()
  bool    mode_set[MOCK_NUM_PINS];  // pinMode() was called for this pin
  bool    digital_set[MOCK_NUM_PINS];
  bool    pwm_set[MOCK_NUM_PINS];

  void reset() {
    for (int i = 0; i < MOCK_NUM_PINS; ++i) {
      mode[i] = 0;
      digital[i] = 0;
      pwm[i] = 0;
      mode_set[i] = false;
      digital_set[i] = false;
      pwm_set[i] = false;
    }
  }
};

static MockPinState g_pins;

extern "C" void pinMode(uint8_t pin, uint8_t mode) {
  if (pin < MOCK_NUM_PINS) {
    g_pins.mode[pin] = mode;
    g_pins.mode_set[pin] = true;
  }
}

extern "C" void digitalWrite(uint8_t pin, uint8_t value) {
  if (pin < MOCK_NUM_PINS) {
    g_pins.digital[pin] = value;
    g_pins.digital_set[pin] = true;
  }
}

extern "C" void analogWrite(uint8_t pin, int value) {
  if (pin < MOCK_NUM_PINS) {
    g_pins.pwm[pin] = value;
    g_pins.pwm_set[pin] = true;
  }
}

// ============================================================================
// Test infrastructure (matches tests/test_coordinates_standalone.cpp style)
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message)                                       \
  do {                                                                        \
    tests_run++;                                                              \
    if (condition) {                                                          \
      tests_passed++;                                                         \
      printf("  PASS: %s\n", message);                                        \
    } else {                                                                  \
      tests_failed++;                                                         \
      printf("  FAIL (%s:%d): %s\n", __FILE__, __LINE__, message);            \
    }                                                                         \
  } while (0)

#define TEST_EQ_INT(actual, expected, message)                                \
  do {                                                                        \
    tests_run++;                                                              \
    long _a = static_cast<long>(actual);                                      \
    long _e = static_cast<long>(expected);                                    \
    if (_a == _e) {                                                           \
      tests_passed++;                                                         \
      printf("  PASS: %s\n", message);                                        \
    } else {                                                                  \
      tests_failed++;                                                         \
      printf("  FAIL (%s:%d): %s (expected %ld, got %ld)\n",                  \
             __FILE__, __LINE__, message, _e, _a);                            \
    }                                                                         \
  } while (0)

// Reference pin assignment — matches SelfBallancingRobot3.ino lines 31-36.
static const L298NPins kRefPins = {
    /*.ena=*/5,  /*.in1=*/6, /*.in2=*/7,
    /*.enb=*/10, /*.in3=*/9, /*.in4=*/8,
};

// ============================================================================
// Tests
// ============================================================================

static void test_l298n_begin_sets_pin_modes() {
  printf("\nTest: begin() configures all six pins as OUTPUT\n");
  g_pins.reset();

  L298NMotorDriver drv(kRefPins);
  bool ok = drv.begin();

  TEST_ASSERT(ok, "begin() returns true");
  TEST_ASSERT(drv.isInitialized(), "isInitialized() true after begin()");
  TEST_EQ_INT(g_pins.mode[kRefPins.ena], OUTPUT, "ENA pinMode = OUTPUT");
  TEST_EQ_INT(g_pins.mode[kRefPins.enb], OUTPUT, "ENB pinMode = OUTPUT");
  TEST_EQ_INT(g_pins.mode[kRefPins.in1], OUTPUT, "IN1 pinMode = OUTPUT");
  TEST_EQ_INT(g_pins.mode[kRefPins.in2], OUTPUT, "IN2 pinMode = OUTPUT");
  TEST_EQ_INT(g_pins.mode[kRefPins.in3], OUTPUT, "IN3 pinMode = OUTPUT");
  TEST_EQ_INT(g_pins.mode[kRefPins.in4], OUTPUT, "IN4 pinMode = OUTPUT");

  // After begin() -> stop(), all INx LOW and PWM=0.
  TEST_EQ_INT(g_pins.digital[kRefPins.in1], LOW, "IN1 LOW after begin");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], LOW, "IN2 LOW after begin");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], LOW, "IN3 LOW after begin");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], LOW, "IN4 LOW after begin");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 0, "ENA PWM=0 after begin");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 0, "ENB PWM=0 after begin");
}

static void test_l298n_stiction_floor() {
  printf("\nTest: stiction floor snaps small non-zero speeds to ±15\n");
  L298NMotorDriver drv(kRefPins, /*stiction_min_pwm=*/15);
  drv.begin();

  // Below floor, positive: 7 -> 15
  g_pins.reset();
  drv.set_speeds(7, 7);
  TEST_EQ_INT(drv.last_left(), 15, "speed=7 -> last_left clamped to 15");
  TEST_EQ_INT(drv.last_right(), 15, "speed=7 -> last_right clamped to 15");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 15, "ENA PWM = 15");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 15, "ENB PWM = 15");

  // Below floor, negative: -3 -> -15
  g_pins.reset();
  drv.set_speeds(-3, -3);
  TEST_EQ_INT(drv.last_left(), -15, "speed=-3 -> last_left clamped to -15");
  TEST_EQ_INT(drv.last_right(), -15, "speed=-3 -> last_right clamped to -15");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 15, "ENA PWM = 15 (magnitude only)");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 15, "ENB PWM = 15 (magnitude only)");

  // Zero stays zero (floor does not promote idle to a creep)
  g_pins.reset();
  drv.set_speeds(0, 0);
  TEST_EQ_INT(drv.last_left(), 0, "speed=0 stays 0");
  TEST_EQ_INT(drv.last_right(), 0, "speed=0 stays 0");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 0, "ENA PWM = 0");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 0, "ENB PWM = 0");

  // Above floor passes through unchanged
  g_pins.reset();
  drv.set_speeds(100, 100);
  TEST_EQ_INT(drv.last_left(), 100, "speed=100 unchanged");
  TEST_EQ_INT(drv.last_right(), 100, "speed=100 unchanged");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 100, "ENA PWM = 100");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 100, "ENB PWM = 100");

  // Exactly at floor: 15 stays 15
  g_pins.reset();
  drv.set_speeds(15, -15);
  TEST_EQ_INT(drv.last_left(), 15, "speed=15 stays at floor");
  TEST_EQ_INT(drv.last_right(), -15, "speed=-15 stays at floor");
}

static void test_l298n_stiction_disabled() {
  printf("\nTest: stiction_min_pwm = 0 disables the floor\n");
  L298NMotorDriver drv(kRefPins, /*stiction_min_pwm=*/0);
  drv.begin();

  g_pins.reset();
  drv.set_speeds(3, -7);
  TEST_EQ_INT(drv.last_left(), 3, "speed=3 unchanged when floor disabled");
  TEST_EQ_INT(drv.last_right(), -7, "speed=-7 unchanged when floor disabled");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 3, "ENA PWM = 3");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 7, "ENB PWM = 7 (magnitude)");
}

static void test_l298n_direction_forward() {
  printf("\nTest: positive speed sets (in_a HIGH, in_b LOW)\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();
  g_pins.reset();

  drv.set_speeds(100, 100);

  // Motor A: IN1 HIGH, IN2 LOW
  TEST_EQ_INT(g_pins.digital[kRefPins.in1], HIGH, "IN1 HIGH for left forward");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], LOW, "IN2 LOW for left forward");
  // Motor B: IN3 HIGH, IN4 LOW
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], HIGH, "IN3 HIGH for right forward");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], LOW, "IN4 LOW for right forward");
}

static void test_l298n_direction_reverse() {
  printf("\nTest: negative speed sets (in_a LOW, in_b HIGH)\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();
  g_pins.reset();

  drv.set_speeds(-100, -100);

  TEST_EQ_INT(g_pins.digital[kRefPins.in1], LOW, "IN1 LOW for left reverse");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], HIGH, "IN2 HIGH for left reverse");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], LOW, "IN3 LOW for right reverse");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], HIGH, "IN4 HIGH for right reverse");
  // PWM magnitudes only
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 100, "ENA PWM=100 (|−100|)");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 100, "ENB PWM=100 (|−100|)");
}

static void test_l298n_stop() {
  printf("\nTest: stop() drives all INx LOW and PWM=0\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();
  drv.set_speeds(150, -150);  // get into a non-zero state first

  g_pins.reset();
  drv.stop();

  TEST_EQ_INT(g_pins.digital[kRefPins.in1], LOW, "IN1 LOW after stop");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], LOW, "IN2 LOW after stop");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], LOW, "IN3 LOW after stop");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], LOW, "IN4 LOW after stop");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 0, "ENA PWM=0 after stop");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 0, "ENB PWM=0 after stop");
  TEST_EQ_INT(drv.last_left(), 0, "last_left = 0 after stop");
  TEST_EQ_INT(drv.last_right(), 0, "last_right = 0 after stop");
}

static void test_l298n_brake() {
  printf("\nTest: brake() drives all INx HIGH and PWM=0\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();
  drv.set_speeds(80, 80);

  g_pins.reset();
  drv.brake();

  TEST_EQ_INT(g_pins.digital[kRefPins.in1], HIGH, "IN1 HIGH after brake");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], HIGH, "IN2 HIGH after brake");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], HIGH, "IN3 HIGH after brake");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], HIGH, "IN4 HIGH after brake");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 0, "ENA PWM=0 after brake");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 0, "ENB PWM=0 after brake");
  TEST_EQ_INT(drv.last_left(), 0, "last_left = 0 after brake");
  TEST_EQ_INT(drv.last_right(), 0, "last_right = 0 after brake");
}

static void test_l298n_clamp() {
  printf("\nTest: out-of-range speeds clamped to ±255\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();

  g_pins.reset();
  drv.set_speeds(999, -999);
  TEST_EQ_INT(drv.last_left(), 255, "999 clamped to 255");
  TEST_EQ_INT(drv.last_right(), -255, "-999 clamped to -255");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 255, "ENA PWM=255");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 255, "ENB PWM=255 (magnitude)");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], LOW, "Right reverse: IN3 LOW");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], HIGH, "Right reverse: IN4 HIGH");

  // INT16_MAX / INT16_MIN should also clamp safely
  g_pins.reset();
  drv.set_speeds(32767, -32768);
  TEST_EQ_INT(drv.last_left(), 255, "INT16_MAX clamped to 255");
  TEST_EQ_INT(drv.last_right(), -255, "INT16_MIN clamped to -255");
}

static void test_l298n_set_speeds_independent() {
  printf("\nTest: set_speeds drives left and right independently\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();

  // Left forward, right reverse — classic spin-in-place
  g_pins.reset();
  drv.set_speeds(100, -100);

  // Left = forward: IN1 HIGH, IN2 LOW
  TEST_EQ_INT(g_pins.digital[kRefPins.in1], HIGH, "Left forward: IN1 HIGH");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], LOW, "Left forward: IN2 LOW");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 100, "Left PWM = 100");
  // Right = reverse: IN3 LOW, IN4 HIGH
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], LOW, "Right reverse: IN3 LOW");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], HIGH, "Right reverse: IN4 HIGH");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 100, "Right PWM = 100 (magnitude)");

  // Swap directions
  g_pins.reset();
  drv.set_speeds(-50, 200);
  TEST_EQ_INT(g_pins.digital[kRefPins.in1], LOW, "Left reverse: IN1 LOW");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], HIGH, "Left reverse: IN2 HIGH");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 50, "Left PWM = 50");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], HIGH, "Right forward: IN3 HIGH");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], LOW, "Right forward: IN4 LOW");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 200, "Right PWM = 200");
}

static void test_l298n_last_inspection() {
  printf("\nTest: last_left()/last_right() reflect last commanded values\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();

  TEST_EQ_INT(drv.last_left(), 0, "last_left = 0 after begin");
  TEST_EQ_INT(drv.last_right(), 0, "last_right = 0 after begin");

  drv.set_speeds(123, -45);
  TEST_EQ_INT(drv.last_left(), 123, "last_left = 123");
  TEST_EQ_INT(drv.last_right(), -45, "last_right = -45");

  drv.set_speed(77);  // convenience overload
  TEST_EQ_INT(drv.last_left(), 77, "set_speed(77): last_left = 77");
  TEST_EQ_INT(drv.last_right(), 77, "set_speed(77): last_right = 77");

  drv.set_speeds(999, -999);  // post-clamp values
  TEST_EQ_INT(drv.last_left(), 255, "last_left reflects clamped 255");
  TEST_EQ_INT(drv.last_right(), -255, "last_right reflects clamped -255");
}

static void test_l298n_zero_pins_low() {
  printf("\nTest: speed 0 sets both INx LOW (free coast on that channel)\n");
  L298NMotorDriver drv(kRefPins);
  drv.begin();
  drv.set_speeds(100, 100);  // start non-zero

  g_pins.reset();
  drv.set_speeds(0, 0);

  TEST_EQ_INT(g_pins.digital[kRefPins.in1], LOW, "IN1 LOW for left=0");
  TEST_EQ_INT(g_pins.digital[kRefPins.in2], LOW, "IN2 LOW for left=0");
  TEST_EQ_INT(g_pins.digital[kRefPins.in3], LOW, "IN3 LOW for right=0");
  TEST_EQ_INT(g_pins.digital[kRefPins.in4], LOW, "IN4 LOW for right=0");
  TEST_EQ_INT(g_pins.pwm[kRefPins.ena], 0, "ENA PWM=0");
  TEST_EQ_INT(g_pins.pwm[kRefPins.enb], 0, "ENB PWM=0");
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("==============================================\n");
  printf("L298NMotorDriver Unit Tests (Phase 4.7b)\n");
  printf("==============================================\n");

  test_l298n_begin_sets_pin_modes();
  test_l298n_stiction_floor();
  test_l298n_stiction_disabled();
  test_l298n_direction_forward();
  test_l298n_direction_reverse();
  test_l298n_stop();
  test_l298n_brake();
  test_l298n_clamp();
  test_l298n_set_speeds_independent();
  test_l298n_last_inspection();
  test_l298n_zero_pins_low();

  printf("\n==============================================\n");
  printf("Results: %d run, %d passed, %d failed\n",
         tests_run, tests_passed, tests_failed);
  printf("==============================================\n");

  return (tests_failed == 0) ? 0 : 1;
}
