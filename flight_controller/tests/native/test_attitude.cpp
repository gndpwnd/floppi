/*
 * test_attitude.cpp — native unit tests for flight_controller attitude math
 * =============================================================================
 *
 * Tests the attitude estimator from src/imu.cpp:
 *   - invSqrt()      — inverse square root
 *   - Madgwick6DOF() — gyro+accel gradient-descent quaternion filter
 *   - quaternion -> Euler conversion (roll/pitch/yaw)
 *
 * SELF-CONTAINED: the functions below are faithful copies of src/imu.cpp.
 * No <Arduino.h>, no Wire/MPU — pure float math, linked to nothing in src/.
 *
 * NOTE on invSqrt: the *current* src/imu.cpp implements invSqrt() as
 *   `return 1.0f / sqrtf(x);`
 * It is NOT the Quake fast-invsqrt. We replicate the source EXACTLY (as the
 * brief requires "copy the functions"), so it is accurate to float precision.
 * The accuracy test below still uses CHECK_NEAR_REL; with the exact impl the
 * relative error is ~1e-6, which trivially passes a 0.2% tolerance and would
 * also pass were the source swapped back to the Quake approximation.
 *
 * Constants pulled from include/config.h:
 *   MADGWICK_BETA 0.04   (config.h:386)   — filter gain (a.k.a. B_madgwick)
 *   LOOP_FREQUENCY_HZ    (config.h:535+)  — 1000 (ESP32) / 2000 (Teensy);
 *                                           invSampleFreq = 1/LOOP_FREQUENCY_HZ.
 * Quaternion state q0..q3 is global in src (globals.h:48); replicated as
 * file-scope globals here. roll_IMU/pitch_IMU/yaw_IMU likewise.
 * =============================================================================
 */
#include "test_helpers.h"
#include <cmath>

// ---------------------------------------------------------------------------
// Replicated constants (config.h)
// ---------------------------------------------------------------------------
#define MADGWICK_BETA 0.04f

// ---------------------------------------------------------------------------
// Replicated global state (globals.h)
// ---------------------------------------------------------------------------
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
static float roll_IMU = 0.0f, pitch_IMU = 0.0f, yaw_IMU = 0.0f;

// ---------------------------------------------------------------------------
// invSqrt — faithful copy of src/imu.cpp:306-308
// ---------------------------------------------------------------------------
static float invSqrt(float x) {
    return 1.0f / sqrtf(x);
}

// ---------------------------------------------------------------------------
// Madgwick6DOF — faithful copy of src/imu.cpp:228-295
// ---------------------------------------------------------------------------
static void Madgwick6DOF(float gx, float gy, float gz,
                         float ax, float ay, float az,
                         float invSampleFreq) {
    // 6DOF Madgwick filter (no magnetometer)
    gx *= 0.0174533f;
    gy *= 0.0174533f;
    gz *= 0.0174533f;

    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        _2q0 = 2.0f * q0;
        _2q1 = 2.0f * q1;
        _2q2 = 2.0f * q2;
        _2q3 = 2.0f * q3;
        _4q0 = 4.0f * q0;
        _4q1 = 4.0f * q1;
        _4q2 = 4.0f * q2;
        _8q1 = 8.0f * q1;
        _8q2 = 8.0f * q2;
        q0q0 = q0 * q0;
        q1q1 = q1 * q1;
        q2q2 = q2 * q2;
        q3q3 = q3 * q3;

        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

        // M-1 guard: skip the gradient-descent correction when the gradient
        // norm collapses (exact level gravity etc.), avoiding invSqrt(0)=inf.
        // Faithful copy of src/imu.cpp.
        float gradNormSq = s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
        if (gradNormSq > 1.0e-12f) {
            recipNorm = invSqrt(gradNormSq);
            s0 *= recipNorm;
            s1 *= recipNorm;
            s2 *= recipNorm;
            s3 *= recipNorm;

            qDot1 -= MADGWICK_BETA * s0;
            qDot2 -= MADGWICK_BETA * s1;
            qDot3 -= MADGWICK_BETA * s2;
            qDot4 -= MADGWICK_BETA * s3;
        }
    }

    q0 += qDot1 * invSampleFreq;
    q1 += qDot2 * invSampleFreq;
    q2 += qDot3 * invSampleFreq;
    q3 += qDot4 * invSampleFreq;

    // M-1 guard: reset to identity on a non-finite or zero-norm quaternion
    // instead of normalising through inf/NaN. Faithful copy of src/imu.cpp.
    float qNormSq = q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3;
    if (!(qNormSq > 1.0e-12f) || std::isnan(q0) || std::isnan(q1) ||
        std::isnan(q2) || std::isnan(q3)) {
        q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    } else {
        recipNorm = invSqrt(qNormSq);
        q0 *= recipNorm;
        q1 *= recipNorm;
        q2 *= recipNorm;
        q3 *= recipNorm;
    }

    roll_IMU  = atan2(q0 * q1 + q2 * q3, 0.5f - q1 * q1 - q2 * q2) * 57.2957795;
    pitch_IMU = asin(-2.0f * (q1 * q3 - q0 * q2)) * 57.2957795;
    yaw_IMU   = atan2(q1 * q2 + q0 * q3, 0.5f - q2 * q2 - q3 * q3) * 57.2957795;
}

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

// Reset the global quaternion state to identity (level, no rotation).
static void reset_quat() {
    q0 = 1.0f; q1 = 0.0f; q2 = 0.0f; q3 = 0.0f;
    roll_IMU = pitch_IMU = yaw_IMU = 0.0f;
}

// Loop rate: src uses LOOP_FREQUENCY_HZ (1000 ESP32 / 2000 Teensy).
// We test at 1000 Hz; dt = invSampleFreq = 1/1000 s.
static const float LOOP_HZ  = 1000.0f;
static const float DT       = 1.0f / LOOP_HZ;
static const float RAD2DEG  = 57.2957795f;
static const float DEG2RAD  = 0.0174533f;

// Produce the accelerometer vector (in g) seen by a static IMU at a given
// roll/pitch. The Madgwick6DOF Euler extraction in src/imu.cpp uses
//   roll  = atan2(q0*q1 + q2*q3, 0.5 - q1*q1 - q2*q2)
//   pitch = asin(-2*(q1*q3 - q0*q2))
// Empirically (and consistently with that convention) the body-frame gravity
// for positive roll phi and positive pitch theta is:
//   ax = -sin(theta)
//   ay =  sin(phi)*cos(theta)
//   az =  cos(phi)*cos(theta)
// Signs were cross-checked: feeding this vector with zero gyro converges the
// estimator to (+phi, +theta) in degrees.
static void tilt_accel(float roll_deg, float pitch_deg,
                       float& ax, float& ay, float& az) {
    float phi   = roll_deg  * DEG2RAD;
    float theta = pitch_deg * DEG2RAD;
    ax = -std::sin(theta);
    ay =  std::sin(phi) * std::cos(theta);
    az =  std::cos(phi) * std::cos(theta);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
TEST_MAIN("attitude estimation (Madgwick6DOF + invSqrt)") {

    // --- 1. invSqrt accuracy ------------------------------------------------
    // Compared against the exact 1/sqrt(x). Tolerance 0.2% covers both the
    // current exact impl and the classic Quake one-Newton-step approximation.
    {
        const double xs[] = {1.0, 4.0, 0.25, 100.0, 1e-3};
        for (int i = 0; i < 5; ++i) {
            double got = (double)invSqrt((float)xs[i]);
            double ref = 1.0 / std::sqrt(xs[i]);
            char msg[80];
            std::snprintf(msg, sizeof(msg),
                          "invSqrt(%g) approx 1/sqrt(%g)", xs[i], xs[i]);
            CHECK_NEAR_REL(got, ref, 2e-3, msg);
        }
    }

    // --- 2. Level convergence (slightly tilted) ----------------------------
    // Zero gyro, near-level accel. Confirms the estimator settles toward the
    // commanded ~2 deg tilt. (The exact-level degenerate case is now its own
    // test below — see test 2b — since the M-1 guard makes it safe.)
    {
        reset_quat();
        float ax, ay, az;
        tilt_accel(2.0f, 2.0f, ax, ay, az);   // ~2 deg off level
        for (int i = 0; i < 12000; ++i)
            Madgwick6DOF(0, 0, 0, ax, ay, az, DT);
        CHECK_NEAR(roll_IMU,  2.0, 1.0, "level: roll converges near 2 deg");
        CHECK_NEAR(pitch_IMU, 2.0, 1.0, "level: pitch converges near 2 deg");
    }

    // --- 2b. M-1 guard: exact level gravity must NOT produce NaN ------------
    // Feeding the *exactly* mathematically-pure (0,0,1)g into the identity
    // quaternion with zero gyro collapses the gradient vector s0..s3 to
    // all-zero. The UNGUARDED source computed invSqrt(0)=+inf -> 0*inf=NaN, and
    // the NaN poisoned the quaternion forever (unrecoverable attitude). The M-1
    // epsilon guard skips the correction in that case (and the final-norm
    // reset-to-identity is a backstop), so the estimator stays finite and at
    // identity (roll/pitch/yaw == 0). This test would FAIL (NaN) on the old src.
    {
        reset_quat();
        for (int i = 0; i < 5000; ++i)
            Madgwick6DOF(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, DT);  // exact +1g down
        CHECK_FINITE(q0, "M-1: q0 finite on exact level gravity (no NaN)");
        CHECK_FINITE(q1, "M-1: q1 finite on exact level gravity");
        CHECK_FINITE(q2, "M-1: q2 finite on exact level gravity");
        CHECK_FINITE(q3, "M-1: q3 finite on exact level gravity");
        CHECK_FINITE(roll_IMU,  "M-1: roll finite on exact level gravity");
        CHECK_FINITE(pitch_IMU, "M-1: pitch finite on exact level gravity");
        CHECK_FINITE(yaw_IMU,   "M-1: yaw finite on exact level gravity");
        float norm2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
        CHECK_NEAR(norm2, 1.0, 1e-4, "M-1: quaternion stays unit-norm at exact level");
        // Identity quaternion -> 0 deg attitude.
        CHECK_NEAR(roll_IMU,  0.0, 1e-3, "M-1: roll == 0 at exact level");
        CHECK_NEAR(pitch_IMU, 0.0, 1e-3, "M-1: pitch == 0 at exact level");
    }

    // --- 2c. M-1 guard: recovery from a NaN-injected quaternion ------------
    // Force the quaternion to NaN (simulating an upstream glitch) and confirm
    // the next update resets it to a finite, unit-norm identity rather than
    // staying dead. Exercises the isnan() reset-to-identity backstop directly.
    {
        reset_quat();
        q0 = std::nanf(""); q1 = std::nanf(""); q2 = 0.0f; q3 = 0.0f;
        // One update with a normal (slightly tilted) accel + zero gyro.
        float ax, ay, az;
        tilt_accel(3.0f, 0.0f, ax, ay, az);
        Madgwick6DOF(0.0f, 0.0f, 0.0f, ax, ay, az, DT);
        CHECK_FINITE(q0, "M-1: recovers finite q0 after NaN injection");
        float norm2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
        CHECK_NEAR(norm2, 1.0, 1e-4, "M-1: re-normalised to unit quaternion after NaN");
    }

    // --- 3. Tilt convergence: pure roll ------------------------------------
    // Static accel for a 20 deg roll. Estimator must recover ~20 deg roll,
    // ~0 deg pitch. Gradient-descent filter -> a few degrees tolerance.
    {
        reset_quat();
        float ax, ay, az;
        tilt_accel(20.0f, 0.0f, ax, ay, az);
        for (int i = 0; i < 20000; ++i)
            Madgwick6DOF(0, 0, 0, ax, ay, az, DT);
        CHECK_NEAR(roll_IMU,  20.0, 3.0, "tilt: roll converges to ~20 deg");
        CHECK_NEAR(pitch_IMU,  0.0, 3.0, "tilt: pitch stays ~0 deg");
    }

    // --- 4. Tilt convergence: pure pitch -----------------------------------
    {
        reset_quat();
        float ax, ay, az;
        tilt_accel(0.0f, -15.0f, ax, ay, az);
        for (int i = 0; i < 20000; ++i)
            Madgwick6DOF(0, 0, 0, ax, ay, az, DT);
        CHECK_NEAR(pitch_IMU, -15.0, 3.0, "tilt: pitch converges to ~-15 deg");
        CHECK_NEAR(roll_IMU,    0.0, 3.0, "tilt: roll stays ~0 deg");
    }

    // --- 5. Tilt convergence: combined roll + pitch ------------------------
    {
        reset_quat();
        float ax, ay, az;
        tilt_accel(12.0f, 25.0f, ax, ay, az);
        for (int i = 0; i < 25000; ++i)
            Madgwick6DOF(0, 0, 0, ax, ay, az, DT);
        CHECK_NEAR(roll_IMU,  12.0, 4.0, "combined tilt: roll ~12 deg");
        CHECK_NEAR(pitch_IMU, 25.0, 4.0, "combined tilt: pitch ~25 deg");
    }

    // --- 6. Gyro integration (short horizon) -------------------------------
    // Constant roll rate of 30 deg/s for 0.2 s with accel held ~level.
    // Over this short window gyro integration dominates: roll ~= rate*time.
    // Expected ~6 deg; the small accel pull-back keeps it slightly under.
    // Accel is a near-level vector (not exact (0,0,1) — see test 2 note).
    {
        reset_quat();
        const float rate_dps = 30.0f;     // about body x-axis
        const float duration = 0.2f;      // seconds
        int steps = (int)(duration * LOOP_HZ);
        float ax, ay, az;
        tilt_accel(0.5f, 0.5f, ax, ay, az);   // essentially level
        for (int i = 0; i < steps; ++i)
            Madgwick6DOF(rate_dps, 0, 0, ax, ay, az, DT);
        float expected = rate_dps * duration;   // 6.0 deg
        CHECK_NEAR(roll_IMU, expected, 1.5,
                   "gyro integration: roll ~= rate*time over 0.2 s");
        CHECK_NEAR(pitch_IMU, 0.0, 1.5,
                   "gyro integration: pitch unaffected by roll-axis rate");
    }

    // --- 7. Quaternion norm stays unit -------------------------------------
    // After a mixed run the quaternion must remain normalized.
    {
        reset_quat();
        float ax, ay, az;
        tilt_accel(10.0f, -8.0f, ax, ay, az);
        for (int i = 0; i < 5000; ++i)
            Madgwick6DOF(5.0f, -3.0f, 2.0f, ax, ay, az, DT);
        float norm2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
        CHECK_NEAR(norm2, 1.0, 1e-4, "quaternion remains unit-norm");
    }

    // --- 8. Finiteness over a long run -------------------------------------
    {
        reset_quat();
        float ax, ay, az;
        tilt_accel(7.0f, 13.0f, ax, ay, az);
        for (int i = 0; i < 50000; ++i)
            Madgwick6DOF(10.0f, -10.0f, 5.0f, ax, ay, az, DT);
        CHECK_FINITE(q0, "long run: q0 finite");
        CHECK_FINITE(roll_IMU,  "long run: roll_IMU finite");
        CHECK_FINITE(pitch_IMU, "long run: pitch_IMU finite");
        CHECK_FINITE(yaw_IMU,   "long run: yaw_IMU finite");
    }

    // --- 9. Degenerate input: zero accel vector ----------------------------
    // The src guard `if (!((ax==0)&&(ay==0)&&(az==0)))` skips the accel
    // correction (and the invSqrt of 0) — gyro-only integration, no NaN.
    {
        reset_quat();
        for (int i = 0; i < 2000; ++i)
            Madgwick6DOF(2.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, DT);
        CHECK_FINITE(q0, "zero-accel: q0 finite (no NaN from invSqrt(0))");
        CHECK_FINITE(roll_IMU,  "zero-accel: roll finite");
        CHECK_FINITE(pitch_IMU, "zero-accel: pitch finite");
        float norm2 = q0*q0 + q1*q1 + q2*q2 + q3*q3;
        CHECK_NEAR(norm2, 1.0, 1e-4, "zero-accel: quaternion still unit-norm");
    }
}
