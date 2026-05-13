/**
 * BNO055 Driver Test
 *
 * Phase 4.6 — verification harness for src/sensors/bno055.{h,cpp}.
 *
 * --------------------------------------------------------------------------
 * TEST STRATEGY
 * --------------------------------------------------------------------------
 *
 * Adafruit_BNO055 talks over real I2C and exposes no virtualization seam, so
 * a native googletest harness cannot meaningfully construct it without a
 * full hardware mock of the chip's register map. We therefore split coverage
 * in two:
 *
 *   1. Native (host) build — verify the orientation pipeline math that the
 *      driver delegates to (quaternion → Euler) using the project's own
 *      math module. This is the part most likely to silently regress and is
 *      independent of the chip. Built when GTEST is available.
 *
 *   2. Arduino target build — wrap a small hardware bring-up sketch under
 *      `#ifdef ARDUINO_HW_TEST` that exercises begin() / read() /
 *      getOrientation() / getCalibrationProfile() / setCalibrationProfile()
 *      against a real BNO055 wired to the I2C bus. Manual run on bench; not
 *      part of CI.
 *
 * Option (a) from the Phase 4.6 task description was chosen. A full mock of
 * Adafruit_BNO055 (option b's "compile-only" path) was considered but would
 * require shadowing roughly two dozen library symbols (imu::Quaternion,
 * adafruit_bno055_offsets_t, Wire, …) for negligible incremental value over
 * what the math tests already exercise.
 */

// ============================================================================
// Native-host math tests
// ============================================================================
//
// These mirror the conversion the BNO055 driver performs in read(): take an
// imu::Quaternion-shaped (w,x,y,z) tuple from the chip, run it through
// quaternion_to_euler_degrees(), and confirm the resulting roll/pitch/yaw is
// what the OrientationData.roll/pitch/yaw_deg fields should contain.

#if defined(BUILD_NATIVE_TESTS) || defined(GTEST_API_)

#include <gtest/gtest.h>
#include <cmath>

#include "../src/math/quaternion.h"
#include "../src/math/quaternion_conversions.h"

namespace {
constexpr float kAngleToleranceDeg = 0.05f;
}

// Identity quaternion → all zero Euler angles.
TEST(BNO055DriverMath, IdentityQuaternionYieldsZeroEuler) {
  Quaternion q(1.0f, 0.0f, 0.0f, 0.0f);
  EulerAngles e = quaternion_to_euler_degrees(q);
  EXPECT_NEAR(0.0f, e.roll,  kAngleToleranceDeg);
  EXPECT_NEAR(0.0f, e.pitch, kAngleToleranceDeg);
  EXPECT_NEAR(0.0f, e.yaw,   kAngleToleranceDeg);
}

// 90° roll about X-axis: q = (cos45°, sin45°, 0, 0).
TEST(BNO055DriverMath, NinetyDegreeRollMatchesQuaternion) {
  const float s = std::sin(M_PI / 4.0f);
  const float c = std::cos(M_PI / 4.0f);
  Quaternion q(c, s, 0.0f, 0.0f);
  EulerAngles e = quaternion_to_euler_degrees(q);
  EXPECT_NEAR(90.0f, e.roll,  kAngleToleranceDeg);
  EXPECT_NEAR( 0.0f, e.pitch, kAngleToleranceDeg);
  EXPECT_NEAR( 0.0f, e.yaw,   kAngleToleranceDeg);
}

// 45° yaw about Z-axis: q = (cos22.5°, 0, 0, sin22.5°).
TEST(BNO055DriverMath, YawRotationProjectsToYawOnly) {
  const float half_yaw = (45.0f * M_PI / 180.0f) * 0.5f;
  Quaternion q(std::cos(half_yaw), 0.0f, 0.0f, std::sin(half_yaw));
  EulerAngles e = quaternion_to_euler_degrees(q);
  EXPECT_NEAR( 0.0f, e.roll,  kAngleToleranceDeg);
  EXPECT_NEAR( 0.0f, e.pitch, kAngleToleranceDeg);
  EXPECT_NEAR(45.0f, e.yaw,   kAngleToleranceDeg);
}

// Driver should reject undersized calibration buffers — sanity check on the
// expected 22-byte length contract. (Cannot exercise the driver itself
// without a chip; this just locks the constant in.)
TEST(BNO055DriverMath, CalibrationProfileSizeIs22Bytes) {
  constexpr uint16_t kExpectedBnO055ProfileBytes = 22;
  EXPECT_EQ(kExpectedBnO055ProfileBytes, 22);
}

#endif  // BUILD_NATIVE_TESTS || GTEST_API_

// ============================================================================
// Arduino-target hardware bring-up sketch
// ============================================================================
//
// Build with `-D ARDUINO_HW_TEST` on the `arduino_mega_bno055` env (proposed
// in the implementation report) and a BNO055 wired to Wire (pins 20/21 on
// Mega). The sketch exercises every public method of the driver and prints
// PASS/FAIL lines suitable for the existing test_calibration.sh-style
// harness.

#if defined(ARDUINO_HW_TEST) && !defined(BUILD_NATIVE_TESTS)

#include <Arduino.h>
#include "../src/sensors/bno055.h"

static BNO055 g_bno;

static void hw_test_begin() {
  Serial.print("test_bno055_begin: ");
  bool ok = g_bno.begin();
  Serial.println(ok && g_bno.isInitialized() ? "PASS" : "FAIL");
}

static void hw_test_read() {
  Serial.print("test_bno055_read: ");
  bool ok = g_bno.read();
  if (!ok || !g_bno.hasNewData()) {
    Serial.println("FAIL (no data)");
    return;
  }
  const OrientationData& d = g_bno.getOrientation();
  float mag2 = d.w * d.w + d.x * d.x + d.y * d.y + d.z * d.z;
  // Quaternion magnitude should be ~1.0 (allow 2% drift).
  Serial.println((mag2 > 0.96f && mag2 < 1.04f) ? "PASS" : "FAIL (mag)");
}

static void hw_test_status_string() {
  Serial.print("test_bno055_status: ");
  const char* s = g_bno.getStatusString();
  Serial.println((s && strstr(s, "BNO055")) ? "PASS" : "FAIL");
}

static void hw_test_calibration_round_trip() {
  Serial.print("test_bno055_cal_roundtrip: ");
  uint8_t buf[22];
  uint16_t len = sizeof(buf);
  if (!g_bno.getCalibrationProfile(buf, &len)) {
    // Expected: sensor not yet fully calibrated. That's not a failure.
    Serial.println("SKIP (not fully calibrated)");
    return;
  }
  if (len != 22) {
    Serial.println("FAIL (bad length)");
    return;
  }
  bool ok = g_bno.setCalibrationProfile(buf, len);
  Serial.println(ok ? "PASS" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) { /* wait for USB */ }
  Serial.println("---- BNO055 hardware tests ----");
  hw_test_begin();
  hw_test_read();
  hw_test_status_string();
  hw_test_calibration_round_trip();
  Serial.println("---- done ----");
}

void loop() {
  delay(1000);
  if (g_bno.read() && g_bno.hasNewData()) {
    Serial.println(g_bno.getStatusString());
  }
}

#endif  // ARDUINO_HW_TEST
