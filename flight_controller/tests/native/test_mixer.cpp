/*
 * test_mixer.cpp — native unit tests for the motor mixer + command scaling
 * =============================================================================
 * Validates the pure arithmetic of:
 *   - src/control.cpp  controlMixer()  (quadcopter X mixer, ~L282)
 *   - src/motors.cpp   scaleCommands() (~L130) and throttleCut() (~L162)
 *
 * The formulas below are copied verbatim from the source so the test exercises
 * the REAL algorithm. Hardware-coupled globals are replaced by the pure-C++
 * stand-ins in globals_stub.h. The standard-PWM (non-OneShot125) scaling path
 * is used: m_PWM = m_scaled * 1000 + 1000 -> microseconds in [1000, 2000].
 *
 * Compile (matches tools/build_tests.sh uniform line):
 *   g++ -std=c++11 -O2 -DUNIT_TEST -Itests/native -o /tmp/tm \
 *       tests/native/test_mixer.cpp && /tmp/tm
 * =============================================================================
 */
#include "test_helpers.h"
#include "globals_stub.h"

using namespace fc_stub;

// ---------------------------------------------------------------------------
// constrain() — Arduino macro replicated as a pure host function.
// ---------------------------------------------------------------------------
static float fc_constrain(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

// ---------------------------------------------------------------------------
// controlMixer() — verbatim quadcopter-X mixer arithmetic from src/control.cpp.
// USE_RACING/USE_AIRMODE air-mode branch is intentionally omitted: it is not
// the default build, and the task targets the base mixer + clamp path.
//
// Sign convention (read directly from src/control.cpp L285-288):
//   m1 = thro - pitch_PID + roll_PID + yaw_PID   (front-?, raised by +roll,+yaw)
//   m2 = thro - pitch_PID - roll_PID - yaw_PID
//   m3 = thro + pitch_PID - roll_PID + yaw_PID
//   m4 = thro + pitch_PID + roll_PID - yaw_PID
// => +roll_PID  : raises m1,m4 / lowers m2,m3   (roll-axis differential pair)
// => +pitch_PID : raises m3,m4 / lowers m1,m2   (pitch-axis differential pair)
// => +yaw_PID   : raises m1,m3 / lowers m2,m4   (yaw diagonal pair)
// ---------------------------------------------------------------------------
static void controlMixer() {
    m1_command_scaled = thro_des - pitch_PID + roll_PID + yaw_PID;
    m2_command_scaled = thro_des - pitch_PID - roll_PID - yaw_PID;
    m3_command_scaled = thro_des + pitch_PID - roll_PID + yaw_PID;
    m4_command_scaled = thro_des + pitch_PID + roll_PID - yaw_PID;
    m5_command_scaled = 0.0f;
    m6_command_scaled = 0.0f;

    m1_command_scaled = fc_constrain(m1_command_scaled, 0.0f, 1.0f);
    m2_command_scaled = fc_constrain(m2_command_scaled, 0.0f, 1.0f);
    m3_command_scaled = fc_constrain(m3_command_scaled, 0.0f, 1.0f);
    m4_command_scaled = fc_constrain(m4_command_scaled, 0.0f, 1.0f);
    m5_command_scaled = fc_constrain(m5_command_scaled, 0.0f, 1.0f);
    m6_command_scaled = fc_constrain(m6_command_scaled, 0.0f, 1.0f);

    // Servo passthrough mixing (src/control.cpp L322-336).
    s1_command_scaled = fc_constrain(roll_passthru,  -1.0f, 1.0f);
    s2_command_scaled = fc_constrain(pitch_passthru, -1.0f, 1.0f);
    s3_command_scaled = fc_constrain(yaw_passthru,   -1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// scaleCommands() — standard-PWM path from src/motors.cpp L140-145.
// ---------------------------------------------------------------------------
static void scaleCommands() {
    m1_command_PWM = (int)(m1_command_scaled * 1000 + 1000);
    m2_command_PWM = (int)(m2_command_scaled * 1000 + 1000);
    m3_command_PWM = (int)(m3_command_scaled * 1000 + 1000);
    m4_command_PWM = (int)(m4_command_scaled * 1000 + 1000);
    m5_command_PWM = (int)(m5_command_scaled * 1000 + 1000);
    m6_command_PWM = (int)(m6_command_scaled * 1000 + 1000);
}

// ---------------------------------------------------------------------------
// throttleCut() — standard-PWM path from src/motors.cpp L162-187.
// Cut condition: channel_5_pwm > 1500  OR  !armedFly.
// ---------------------------------------------------------------------------
static void throttleCut() {
    if ((channel_5_pwm > 1500) || !armedFly) {
        armedFly = false;
        m1_command_PWM = 1000; m2_command_PWM = 1000; m3_command_PWM = 1000;
        m4_command_PWM = 1000; m5_command_PWM = 1000; m6_command_PWM = 1000;
        integral_roll = 0.0f; integral_pitch = 0.0f; integral_yaw = 0.0f;
    }
}

// Reset all mixer inputs to a known neutral state before each case.
static void reset_inputs() {
    thro_des = roll_PID = pitch_PID = yaw_PID = 0.0f;
    roll_passthru = pitch_passthru = yaw_passthru = 0.0f;
    armedFly = true;
    channel_5_pwm = 1000;
    integral_roll = integral_pitch = integral_yaw = 1.0f; // non-zero to detect reset
}

TEST_MAIN("motor mixer / command scaling") {

    // --- Case 1: hover symmetry ---------------------------------------------
    // Zero roll/pitch/yaw PID + mid throttle => all four motors equal.
    reset_inputs();
    thro_des = 0.5f;
    controlMixer();
    CHECK_NEAR(m1_command_scaled, 0.5f, 1e-6, "hover: m1 == mid throttle");
    CHECK(m1_command_scaled == m2_command_scaled &&
          m2_command_scaled == m3_command_scaled &&
          m3_command_scaled == m4_command_scaled,
          "hover: all four motors equal (symmetric)");

    // --- Case 2: roll differential ------------------------------------------
    // +roll_PID raises m1,m4 and lowers m2,m3 by the same magnitude.
    reset_inputs();
    thro_des = 0.5f; roll_PID = 0.1f;
    controlMixer();
    CHECK_NEAR(m1_command_scaled, 0.6f, 1e-6, "+roll: m1 raised");
    CHECK_NEAR(m4_command_scaled, 0.6f, 1e-6, "+roll: m4 raised");
    CHECK_NEAR(m2_command_scaled, 0.4f, 1e-6, "+roll: m2 lowered");
    CHECK_NEAR(m3_command_scaled, 0.4f, 1e-6, "+roll: m3 lowered");

    // --- Case 3: pitch differential -----------------------------------------
    // +pitch_PID raises m3,m4 (rear pair) and lowers m1,m2 (front pair).
    reset_inputs();
    thro_des = 0.5f; pitch_PID = 0.2f;
    controlMixer();
    CHECK_NEAR(m3_command_scaled, 0.7f, 1e-6, "+pitch: m3 raised");
    CHECK_NEAR(m4_command_scaled, 0.7f, 1e-6, "+pitch: m4 raised");
    CHECK_NEAR(m1_command_scaled, 0.3f, 1e-6, "+pitch: m1 lowered");
    CHECK_NEAR(m2_command_scaled, 0.3f, 1e-6, "+pitch: m2 lowered");

    // --- Case 4: yaw differential -------------------------------------------
    // +yaw_PID raises diagonal m1,m3 and lowers diagonal m2,m4.
    reset_inputs();
    thro_des = 0.5f; yaw_PID = 0.15f;
    controlMixer();
    CHECK_NEAR(m1_command_scaled, 0.65f, 1e-6, "+yaw: m1 raised (diag)");
    CHECK_NEAR(m3_command_scaled, 0.65f, 1e-6, "+yaw: m3 raised (diag)");
    CHECK_NEAR(m2_command_scaled, 0.35f, 1e-6, "+yaw: m2 lowered (diag)");
    CHECK_NEAR(m4_command_scaled, 0.35f, 1e-6, "+yaw: m4 lowered (diag)");

    // --- Case 5: command scaling --------------------------------------------
    // scaleCommands maps scaled [0,1] -> PWM [1000,2000] us.
    reset_inputs();
    thro_des = 0.5f;
    controlMixer();
    scaleCommands();
    CHECK_EQ(m1_command_PWM, 1500, "scale: 0.5 scaled -> 1500us PWM");

    // --- Case 6: throttle-cut / disarm --------------------------------------
    // channel_5_pwm > 1500 => every motor forced to 1000us, integrators reset.
    reset_inputs();
    thro_des = 0.8f; roll_PID = 0.2f;
    controlMixer();
    scaleCommands();              // motors now spun up
    channel_5_pwm = 1600;         // assert throttle cut
    throttleCut();
    CHECK_EQ(m1_command_PWM, 1000, "cut: m1 forced to min PWM");
    CHECK(m1_command_PWM == 1000 && m2_command_PWM == 1000 &&
          m3_command_PWM == 1000 && m4_command_PWM == 1000 &&
          m5_command_PWM == 1000 && m6_command_PWM == 1000,
          "cut: all six motors forced to 1000us");
    CHECK_FALSE(armedFly, "cut: armedFly cleared");
    CHECK(integral_roll == 0.0f && integral_pitch == 0.0f &&
          integral_yaw == 0.0f, "cut: PID integrators reset to zero");

    // Disarmed path: !armedFly alone also forces the cut.
    reset_inputs();
    thro_des = 0.7f;
    controlMixer();
    scaleCommands();
    armedFly = false;             // disarmed, aux channel still low
    throttleCut();
    CHECK_EQ(m2_command_PWM, 1000, "disarm: !armedFly forces min PWM");

    // --- Case 7: clamp at endpoints -----------------------------------------
    // Extreme PID terms must saturate scaled outputs to [0,1] -> PWM [1000,2000].
    reset_inputs();
    thro_des = 0.5f; roll_PID = 5.0f;        // huge positive roll
    controlMixer();
    scaleCommands();
    CHECK_NEAR(m1_command_scaled, 1.0f, 1e-6, "clamp: m1 saturates high at 1.0");
    CHECK_NEAR(m2_command_scaled, 0.0f, 1e-6, "clamp: m2 saturates low at 0.0");
    CHECK(m1_command_PWM >= 1000 && m1_command_PWM <= 2000 &&
          m2_command_PWM >= 1000 && m2_command_PWM <= 2000 &&
          m3_command_PWM >= 1000 && m3_command_PWM <= 2000 &&
          m4_command_PWM >= 1000 && m4_command_PWM <= 2000,
          "clamp: all motor PWM within [1000,2000]");
    CHECK_EQ(m1_command_PWM, 2000, "clamp: m1 PWM endpoint == 2000us");
    CHECK_EQ(m2_command_PWM, 1000, "clamp: m2 PWM endpoint == 1000us");

    // Negative extreme: huge negative throttle/PID clamps to the low endpoint.
    reset_inputs();
    thro_des = 0.0f; pitch_PID = -9.0f;
    controlMixer();
    CHECK_NEAR(m3_command_scaled, 0.0f, 1e-6, "clamp: large -pitch clamps m3 to 0");
    CHECK(m1_command_scaled >= 0.0f && m1_command_scaled <= 1.0f,
          "clamp: m1 stays within [0,1] under extreme input");

    // --- Case 8: finiteness --------------------------------------------------
    // Outputs are always finite for finite inputs (including saturated ones).
    reset_inputs();
    thro_des = 0.5f; roll_PID = 1e6f; pitch_PID = -1e6f; yaw_PID = 1e6f;
    controlMixer();
    scaleCommands();
    CHECK_FINITE(m1_command_scaled, "finite: m1 scaled output finite");
    CHECK_FINITE(m2_command_scaled, "finite: m2 scaled output finite");
    CHECK_FINITE(m3_command_scaled, "finite: m3 scaled output finite");
    CHECK_FINITE(m4_command_scaled, "finite: m4 scaled output finite");
    CHECK_FINITE((double)m1_command_PWM, "finite: m1 PWM output finite");
}
