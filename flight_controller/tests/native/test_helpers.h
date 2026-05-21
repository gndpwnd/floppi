/**
 * test_helpers.h — native (host-side) printf-based unit-test harness
 * =============================================================================
 *
 * Self-contained assertion + reporting macros for flight_controller's native
 * test suite. Mirrors the proven auto_orientation printf-assert pattern.
 *
 * Dependencies: ONLY <cstdio>, <cmath>, <cstdlib>. No Arduino / Wire / WiFi /
 * gtest / Unity. Every native test (tests/native/test_*.cpp) #includes this
 * header and nothing hardware-coupled.
 *
 * Usage:
 * -----------------------------------------------------------------------------
 *   #include "test_helpers.h"
 *
 *   int main() {
 *       TEST_BEGIN("my feature");
 *
 *       CHECK(2 + 2 == 4, "arithmetic works");
 *       CHECK_EQ(answer(), 42, "answer is 42");
 *       CHECK_NEAR(std::sqrt(2.0), 1.41421356, 1e-6, "sqrt(2) approx");
 *
 *       TEST_END();   // prints summary, returns non-zero if any CHECK failed
 *   }
 *
 * Or, for a one-liner main, wrap the body in TEST_MAIN:
 *
 *   TEST_MAIN("my feature") {
 *       CHECK(true, "always passes");
 *   }
 *
 * Reporting:
 * -----------------------------------------------------------------------------
 *   - Each CHECK prints "[PASS] msg" or "[FAIL] msg (...detail...)".
 *   - TEST_END() prints a "total / passed / failed" summary line.
 *   - The process exit code is 0 when all checks passed, 1 otherwise — this is
 *     what tools/build_tests.sh keys off of to mark a file [PASS]/[FAIL].
 *
 * Convention: tests are STANDALONE. They copy or include only pure, portable
 * math/logic under test — never link src/ and never pull hardware headers.
 * =============================================================================
 */
#ifndef FC_NATIVE_TEST_HELPERS_H
#define FC_NATIVE_TEST_HELPERS_H

#include <cstdio>
#include <cmath>
#include <cstdlib>

// ----------------------------------------------------------------------------
// Pass / fail counters. Defined here (as static) so each standalone test
// binary gets its own copy — no linking, no ODR clashes between test files.
// ----------------------------------------------------------------------------
namespace fc_test {

static int g_total  = 0;   // checks attempted
static int g_passed = 0;   // checks that passed
static int g_failed = 0;   // checks that failed

// Float comparison utilities ------------------------------------------------

/** Absolute-tolerance float compare. */
inline bool near_abs(double a, double b, double eps) {
    return std::fabs(a - b) <= eps;
}

/** Relative-tolerance float compare (falls back to absolute near zero). */
inline bool near_rel(double a, double b, double rel_eps) {
    double diff  = std::fabs(a - b);
    double scale = std::fmax(std::fabs(a), std::fabs(b));
    if (scale < 1e-12) return diff <= rel_eps;
    return diff <= rel_eps * scale;
}

/** True only when x is a finite (non-NaN, non-Inf) value. */
inline bool is_finite(double x) {
    return std::isfinite(x);
}

// Reporting -----------------------------------------------------------------

inline void begin(const char* name) {
    std::printf("------------------------------------------------------------\n");
    std::printf(" native test: %s\n", name);
    std::printf("------------------------------------------------------------\n");
}

inline void record_pass(const char* msg) {
    ++g_total; ++g_passed;
    std::printf("  [PASS] %s\n", msg);
}

inline void record_fail(const char* msg, const char* file, int line,
                         const char* detail) {
    ++g_total; ++g_failed;
    if (detail && detail[0]) {
        std::printf("  [FAIL] %s  (%s:%d  %s)\n", msg, file, line, detail);
    } else {
        std::printf("  [FAIL] %s  (%s:%d)\n", msg, file, line);
    }
}

/** Print the summary line and return the process exit code (0 = all passed). */
inline int summary() {
    std::printf("------------------------------------------------------------\n");
    std::printf(" summary: total=%d  passed=%d  failed=%d\n",
                g_total, g_passed, g_failed);
    std::printf("------------------------------------------------------------\n");
    return (g_failed == 0) ? 0 : 1;
}

} // namespace fc_test

// ----------------------------------------------------------------------------
// Assertion macros.
//
// All evaluate their arguments exactly once. On failure they record and keep
// going (non-fatal) so a single run reports every failing check.
// ----------------------------------------------------------------------------

/** CHECK(cond, msg) — assert that a boolean condition holds. */
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) { ::fc_test::record_pass(msg); }                             \
        else      { ::fc_test::record_fail((msg), __FILE__, __LINE__,           \
                                           "condition false: " #cond); }       \
    } while (0)

/** CHECK_TRUE / CHECK_FALSE — readable aliases for boolean checks. */
#define CHECK_TRUE(cond, msg)  CHECK((cond), (msg))
#define CHECK_FALSE(cond, msg) CHECK(!(cond), (msg))

/** CHECK_EQ(a, b, msg) — assert a == b (works for ints / enums / pointers). */
#define CHECK_EQ(a, b, msg)                                                    \
    do {                                                                       \
        if ((a) == (b)) { ::fc_test::record_pass(msg); }                       \
        else { ::fc_test::record_fail((msg), __FILE__, __LINE__,               \
                                      "expected " #a " == " #b); }             \
    } while (0)

/** CHECK_NE(a, b, msg) — assert a != b. */
#define CHECK_NE(a, b, msg)                                                    \
    do {                                                                       \
        if ((a) != (b)) { ::fc_test::record_pass(msg); }                       \
        else { ::fc_test::record_fail((msg), __FILE__, __LINE__,               \
                                      "expected " #a " != " #b); }             \
    } while (0)

/** CHECK_NEAR(a, b, eps, msg) — assert |a - b| <= eps (absolute tolerance). */
#define CHECK_NEAR(a, b, eps, msg)                                             \
    do {                                                                       \
        double _fa = (double)(a), _fb = (double)(b), _fe = (double)(eps);      \
        if (::fc_test::near_abs(_fa, _fb, _fe)) {                              \
            ::fc_test::record_pass(msg);                                       \
        } else {                                                               \
            char _buf[160];                                                    \
            std::snprintf(_buf, sizeof(_buf),                                  \
                "%.9g vs %.9g, |diff|=%.3g > eps=%.3g",                        \
                _fa, _fb, std::fabs(_fa - _fb), _fe);                          \
            ::fc_test::record_fail((msg), __FILE__, __LINE__, _buf);           \
        }                                                                      \
    } while (0)

/** CHECK_NEAR_REL(a, b, rel, msg) — assert relative-tolerance closeness. */
#define CHECK_NEAR_REL(a, b, rel, msg)                                         \
    do {                                                                       \
        double _fa = (double)(a), _fb = (double)(b), _fr = (double)(rel);      \
        if (::fc_test::near_rel(_fa, _fb, _fr)) {                              \
            ::fc_test::record_pass(msg);                                       \
        } else {                                                               \
            char _buf[160];                                                    \
            std::snprintf(_buf, sizeof(_buf),                                  \
                "%.9g vs %.9g, rel-eps=%.3g", _fa, _fb, _fr);                  \
            ::fc_test::record_fail((msg), __FILE__, __LINE__, _buf);           \
        }                                                                      \
    } while (0)

/** CHECK_FINITE(x, msg) — assert x is neither NaN nor Inf. */
#define CHECK_FINITE(x, msg)                                                   \
    do {                                                                       \
        if (::fc_test::is_finite((double)(x))) { ::fc_test::record_pass(msg); } \
        else { ::fc_test::record_fail((msg), __FILE__, __LINE__,               \
                                      "value is NaN or Inf"); }                \
    } while (0)

// ----------------------------------------------------------------------------
// Test scaffolding.
// ----------------------------------------------------------------------------

/** TEST_BEGIN(name) — print the test banner. Call once at the top of main(). */
#define TEST_BEGIN(name) ::fc_test::begin(name)

/** TEST_END() — print the summary and `return` the exit code from main(). */
#define TEST_END() return ::fc_test::summary()

/**
 * TEST_MAIN(name) — declares main() and the banner for you. Follow it with a
 * brace block; the summary + return are emitted automatically.
 *
 *   TEST_MAIN("widget") { CHECK(ok(), "widget ok"); }
 */
#define TEST_MAIN(name)                                                        \
    static void fc_test_body();                                                \
    int main() {                                                               \
        ::fc_test::begin(name);                                                \
        fc_test_body();                                                        \
        return ::fc_test::summary();                                           \
    }                                                                          \
    static void fc_test_body()

#endif // FC_NATIVE_TEST_HELPERS_H
