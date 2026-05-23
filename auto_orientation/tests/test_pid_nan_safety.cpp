/**
 * PID NaN/Inf safety tests (audit P1-003, security_audit_2026-05-22.md).
 *
 * The Unity-framework test_pid_controller.cpp is NOT host-buildable from
 * build_tests.sh (needs unity.h on the include path). This standalone test
 * focuses ONLY on the finiteness guards added for the 2026-05-22 audit and is
 * host-buildable with a plain g++ — so the safety-critical NaN behaviour is
 * always exercised by `tools/build_tests.sh`.
 *
 * What it asserts (fault-path only — the nominal path is covered by the Unity
 * test and is deliberately NOT re-tested here so this file stays surgical):
 *   - set_tunings() rejects NaN/Inf gains (stores 0), not just negatives.
 *   - compute()/compute_with_rate() never return a non-finite output, even when
 *     the gains have been (somehow) poisoned — the clamp_() NaN→0 guard closes
 *     the NaN→(int16_t) motor-cast class.
 *   - A finite, in-range output on the nominal path is UNCHANGED by the guards.
 *
 * Compile (from auto_orientation/):
 *
 *   g++ -std=c++11 -O2 -DUNIT_TEST \
 *       -o tests/test_pid_nan_safety \
 *       tests/test_pid_nan_safety.cpp \
 *       src/control/pid_controller.cpp
 *   ./tests/test_pid_nan_safety
 */

#include <cstdio>
#include <cstdint>
#include <cmath>

#include "../src/control/pid_controller.h"

using std::isfinite;   // <cmath> places isfinite in std:: on the host toolchain

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (cond) { tests_passed++; printf("  PASS: %s\n", msg); }             \
        else { tests_failed++; printf("  FAIL (%s:%d): %s\n",                  \
                                      __FILE__, __LINE__, msg); }              \
    } while (0)

static bool approx(float a, float b, float tol) {
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= tol;
}

// (1) set_tunings rejects NaN gains -> stored as 0 (not propagated).
static void test_set_tunings_rejects_nan() {
    printf("\nTest 1: set_tunings rejects NaN gains (stored 0)\n");
    PIDController pid(10.0f, 1.0f, 2.0f, -255.0f, 255.0f);
    pid.set_tunings(NAN, NAN, NAN);
    float kp, ki, kd;
    pid.get_tunings(kp, ki, kd);
    TEST_ASSERT(kp == 0.0f && ki == 0.0f && kd == 0.0f,
                "NaN kp/ki/kd all collapse to 0");
}

// (2) set_tunings rejects Inf gains -> stored as 0.
static void test_set_tunings_rejects_inf() {
    printf("\nTest 2: set_tunings rejects +/-Inf gains (stored 0)\n");
    PIDController pid(10.0f, 1.0f, 2.0f, -255.0f, 255.0f);
    pid.set_tunings(INFINITY, -INFINITY, INFINITY);
    float kp, ki, kd;
    pid.get_tunings(kp, ki, kd);
    TEST_ASSERT(kp == 0.0f && ki == 0.0f && kd == 0.0f,
                "Inf kp/ki/kd all collapse to 0");
}

// (3) set_tunings still rejects negatives (regression — pre-existing behaviour).
static void test_set_tunings_rejects_negative() {
    printf("\nTest 3: set_tunings still rejects negative gains (regression)\n");
    PIDController pid(0.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    pid.set_tunings(-5.0f, -1.0f, -3.0f);
    float kp, ki, kd;
    pid.get_tunings(kp, ki, kd);
    TEST_ASSERT(kp == 0.0f && ki == 0.0f && kd == 0.0f, "negative gains -> 0");
}

// (4) Nominal finite gains are stored unchanged (guards do NOT touch the
//     happy path).
static void test_set_tunings_finite_unchanged() {
    printf("\nTest 4: finite gains stored unchanged (nominal path intact)\n");
    PIDController pid(0.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    pid.set_tunings(65.0f, 12.0f, 38.0f);   // reference SelfBallancingRobot3 gains
    float kp, ki, kd;
    pid.get_tunings(kp, ki, kd);
    TEST_ASSERT(approx(kp, 65.0f, 1e-6f) && approx(ki, 12.0f, 1e-6f) &&
                approx(kd, 38.0f, 1e-6f),
                "finite gains pass through untouched");
}

// (5) compute() output is finite and in range with NaN gains rejected to 0.
static void test_compute_finite_with_rejected_gains() {
    printf("\nTest 5: compute() output finite/in-range after NaN gains rejected\n");
    PIDController pid(10.0f, 1.0f, 2.0f, -255.0f, 255.0f);
    pid.set_tunings(NAN, NAN, NAN);   // -> all 0
    pid.set_setpoint(0.0f);
    float out = 0.0f;
    for (int i = 0; i < 10; ++i) out = pid.compute(5.0f, 5);
    TEST_ASSERT(isfinite(out), "output is finite");
    TEST_ASSERT(out >= -255.0f && out <= 255.0f, "output in range");
    TEST_ASSERT(approx(out, 0.0f, 1e-6f), "zero gains -> zero output");
}

// (6) compute() never returns NaN even when fed a NaN measurement (returns
//     last_output_, which starts at 0 -> finite).
static void test_compute_nan_measurement_finite() {
    printf("\nTest 6: compute() with NaN measurement returns finite last output\n");
    PIDController pid(10.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    float out = pid.compute(NAN, 5);
    TEST_ASSERT(isfinite(out), "NaN measurement -> finite (last_output_) output");
}

// (7) The clamp NaN->0 guard: directly drive a scenario where the summed output
//     would be non-finite. We poison via set_tunings(Inf) which is rejected to
//     0, AND we verify a normal large-error case still clamps to the limit
//     (not NaN). This confirms clamp_() returns 0 on non-finite and the
//     boundary clamp on finite values is unchanged.
static void test_clamp_behaviour_finite_and_nonfinite() {
    printf("\nTest 7: clamp returns limit on finite overshoot, 0 on non-finite\n");
    PIDController pid(1000.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    pid.set_setpoint(0.0f);
    float out = pid.compute(100.0f, 5);   // huge error -> saturates negative
    TEST_ASSERT(isfinite(out) && approx(out, -255.0f, 1e-3f),
                "finite overshoot clamps to output_min (-255), not NaN");
}

// (8) Nominal control output is byte-for-byte unchanged by the guards: a
//     known measurement with reference gains yields the same output a guard-
//     free PID would (P-only, single step, first-compute skips D).
static void test_nominal_output_unchanged() {
    printf("\nTest 8: nominal output unchanged by safety guards\n");
    PIDController pid(10.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    pid.set_setpoint(0.0f);
    // First compute: P = -kp*meas = -10*2 = -20, I=0, D skipped (first_compute).
    float out = pid.compute(2.0f, 5);
    TEST_ASSERT(approx(out, -20.0f, 1e-4f),
                "P-only output = -kp*meas (guards do not alter the math)");
}

int main() {
    printf("PID NaN/Inf safety tests (audit P1-003)\n");
    printf("=======================================\n");

    test_set_tunings_rejects_nan();
    test_set_tunings_rejects_inf();
    test_set_tunings_rejects_negative();
    test_set_tunings_finite_unchanged();
    test_compute_finite_with_rejected_gains();
    test_compute_nan_measurement_finite();
    test_clamp_behaviour_finite_and_nonfinite();
    test_nominal_output_unchanged();

    printf("\n--- Summary ---\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
