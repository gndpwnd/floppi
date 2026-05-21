/**
 * test_harness_selfcheck.cpp — canary smoke test for the native harness.
 * =============================================================================
 *
 * Build (also done automatically by tools/build_tests.sh glob discovery):
 *   g++ -std=c++11 -O2 -DUNIT_TEST -Itests/native \
 *       -o /tmp/test_harness_selfcheck tests/native/test_harness_selfcheck.cpp
 *
 * Purpose: prove the test_helpers.h harness compiles and runs end-to-end. It
 * exercises CHECK, CHECK_EQ, CHECK_NEAR and basic arithmetic. It MUST PASS —
 * if this canary ever goes red, the harness itself is broken, independent of
 * any feature test a follow-up agent may add.
 *
 * This file is STANDALONE: it includes only test_helpers.h, no src/, no
 * Arduino/Wire/WiFi headers.
 * =============================================================================
 */
#include "test_helpers.h"

TEST_MAIN("harness selfcheck") {
    // A plain passing boolean condition.
    CHECK(true, "CHECK accepts a true condition");

    // Basic integer arithmetic via CHECK_EQ.
    CHECK_EQ(2 + 2, 4, "CHECK_EQ: 2 + 2 == 4");

    // Inequality.
    CHECK_NE(1, 2, "CHECK_NE: 1 != 2");

    // Floating-point near-equality with an absolute tolerance.
    CHECK_NEAR(std::sqrt(2.0), 1.4142135623730951, 1e-9,
               "CHECK_NEAR: sqrt(2) within 1e-9");

    // Finite-value guard.
    CHECK_FINITE(3.14159, "CHECK_FINITE: pi is finite");
}
