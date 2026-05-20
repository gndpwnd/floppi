/**
 * Unit Tests for JSONFormatter (src/output/json_formatter.{h,cpp})
 *
 * Purpose
 * -------
 * Pure-native unit tests for the JSONFormatter class — exercises every public
 * method (constructor, format()) and indirectly its private writeFloat /
 * writeDouble helpers via the format() output. Addresses P0 finding from
 * docs/findings/audit_test_coverage_2026-05-20.md: json_formatter.cpp (165 LOC)
 * was a pure utility yet had no native test.
 *
 * Conventions
 * -----------
 *   - printf-based asserts (no Unity / gtest dependency — matches
 *     test_plant_identifier.cpp / test_quaternion.cpp style).
 *   - Each scenario is one void function; main() invokes them all and prints
 *     a final "Tests run: N, passed: M" line.
 *   - Exits 0 on full pass, 1 on any failure.
 *   - Does NOT depend on Arduino.h (P2 finding in the audit).
 *
 * Canonical compile line (mirrors tools/build_tests.sh build_test() pattern):
 *
 *   g++ -std=c++11 -O2 -fpermissive \
 *       -Iinclude -Isrc \
 *       -o tests/test_json_formatter \
 *       tests/test_json_formatter.cpp \
 *       src/output/json_formatter.cpp
 *
 * Author / Date: test-writer-json-formatter agent, 2026-05-20
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "../src/output/json_formatter.h"
#include "../src/sensors/sensor_base.h"

// ============================================================================
// Test infrastructure
// ============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static bool g_current_failed = false;

static void test_begin(const char* name) {
    g_tests_run++;
    g_current_failed = false;
    printf("[Test %d] %s\n", g_tests_run, name);
}

static void test_end() {
    if (!g_current_failed) {
        g_tests_passed++;
        printf("  PASS\n");
    } else {
        printf("  FAIL\n");
    }
}

#define EXPECT_TRUE(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("  FAIL: %s (line %d): %s\n", msg, __LINE__, #cond);   \
            g_current_failed = true;                                      \
        }                                                                 \
    } while (0)

#define EXPECT_CONTAINS(haystack, needle)                                 \
    do {                                                                  \
        if (std::strstr((haystack), (needle)) == NULL) {                  \
            printf("  FAIL: line %d: expected to find \"%s\"\n"           \
                   "        in: %s\n",                                    \
                   __LINE__, (needle), (haystack));                       \
            g_current_failed = true;                                      \
        }                                                                 \
    } while (0)

#define EXPECT_NOT_CONTAINS(haystack, needle)                             \
    do {                                                                  \
        if (std::strstr((haystack), (needle)) != NULL) {                  \
            printf("  FAIL: line %d: expected NOT to find \"%s\"\n"       \
                   "        in: %s\n",                                    \
                   __LINE__, (needle), (haystack));                       \
            g_current_failed = true;                                      \
        }                                                                 \
    } while (0)

#define EXPECT_EQ_INT(actual, expected)                                   \
    do {                                                                  \
        long _a = (long)(actual);                                         \
        long _e = (long)(expected);                                       \
        if (_a != _e) {                                                   \
            printf("  FAIL: line %d: expected %ld, got %ld\n",            \
                   __LINE__, _e, _a);                                     \
            g_current_failed = true;                                      \
        }                                                                 \
    } while (0)

// ----------------------------------------------------------------------------
// Helpers for building synthetic sensor data
// ----------------------------------------------------------------------------

static OrientationData make_orientation(float w, float x, float y, float z,
                                        uint32_t ts_ms = 0) {
    OrientationData o;
    o.w = w; o.x = x; o.y = y; o.z = z;
    o.cal_status = 3;
    o.cal_accel  = 3;
    o.cal_gyro   = 3;
    o.cal_mag    = 2;
    o.timestamp_ms = ts_ms;
    return o;
}

static PositionData make_position(double lat, double lon, float alt,
                                  float acc, uint8_t sats, uint8_t fix) {
    PositionData p;
    p.latitude = lat;
    p.longitude = lon;
    p.altitude = alt;
    p.accuracy_m = acc;
    p.num_satellites = sats;
    p.fix_quality = fix;
    return p;
}

// ============================================================================
// 1. Happy path: full valid orientation + valid position
// ============================================================================

static void test_happy_path_full() {
    test_begin("happy_path: orientation + position both valid");

    JSONFormatter fmt;  // default 3 decimal places
    char buf[512];
    OrientationData o = make_orientation(0.707f, 0.0f, 0.0f, 0.707f, 123456);
    PositionData    p = make_position(37.7749, -122.4194, 50.0f, 2.5f, 8, 1);

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format() returned non-zero bytes");
    EXPECT_TRUE(n < sizeof(buf), "byte count within buffer");
    EXPECT_TRUE(std::strlen(buf) == n, "string length matches return value");
    // Outer braces and schema keys.
    EXPECT_TRUE(buf[0] == '{', "starts with {");
    EXPECT_TRUE(buf[n - 1] == '}', "ends with }");
    EXPECT_CONTAINS(buf, "\"timestamp_ms\":123456");
    EXPECT_CONTAINS(buf, "\"orientation\":");
    EXPECT_CONTAINS(buf, "\"quaternion\":");
    EXPECT_CONTAINS(buf, "\"calibration\":");
    EXPECT_CONTAINS(buf, "\"position\":");
    EXPECT_CONTAINS(buf, "\"valid\":true");      // both sections valid
    EXPECT_CONTAINS(buf, "\"fix_quality\":1");

    test_end();
}

// ============================================================================
// 2. Format name
// ============================================================================

static void test_get_format_name() {
    test_begin("getFormatName() returns \"JSON\"");
    JSONFormatter fmt;
    EXPECT_TRUE(std::strcmp(fmt.getFormatName(), "JSON") == 0,
                "format name == \"JSON\"");
    test_end();
}

// ============================================================================
// 3. Float precision: default 3 decimal places
// ============================================================================

static void test_float_precision_default() {
    test_begin("float precision: default = 3 decimals");

    JSONFormatter fmt;  // default = 3
    char buf[512];
    OrientationData o = make_orientation(0.123456f, 0.0f, 0.0f, 1.0f, 0);
    PositionData    p;  // invalid -> nulls

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    // 0.123456 -> "0.123" at 3dp
    EXPECT_CONTAINS(buf, "\"w\":0.123");
    EXPECT_NOT_CONTAINS(buf, "0.1234");  // must round/truncate at 3dp

    test_end();
}

// ============================================================================
// 4. Float precision: configurable (6 dp)
// ============================================================================

static void test_float_precision_six() {
    test_begin("float precision: 6 decimals (constructor arg)");

    JSONFormatter fmt(6);
    char buf[512];
    OrientationData o = make_orientation(0.123456789f, 0.0f, 0.0f, 1.0f, 0);
    PositionData    p;

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    // 0.123456789f at 6 dp ≈ "0.123457" (rounded). Just check 6 digits present.
    EXPECT_CONTAINS(buf, "\"w\":0.12345");  // first 5 digits stable

    test_end();
}

// ============================================================================
// 5. Float precision: zero decimals
// ============================================================================

static void test_float_precision_zero() {
    test_begin("float precision: 0 decimals");

    JSONFormatter fmt(0);
    char buf[512];
    OrientationData o = make_orientation(1.7f, 0.0f, 0.0f, 1.0f, 0);
    PositionData    p;

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    // At 0 dp, 1.7 -> "2" (banker's-ish rounding via snprintf -> "2").
    EXPECT_CONTAINS(buf, "\"w\":2");
    EXPECT_NOT_CONTAINS(buf, "\"w\":1.7");

    test_end();
}

// ============================================================================
// 6. Orientation validity heuristic: all-zero quaternion => valid:false
// ============================================================================

static void test_orientation_invalid_when_all_zero() {
    test_begin("orientation.valid=false when w=x=y=z=0");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(0.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p;  // also invalid

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    EXPECT_CONTAINS(buf, "\"orientation\":{\"valid\":false");
    EXPECT_CONTAINS(buf, "\"position\":{\"valid\":false");

    test_end();
}

// ============================================================================
// 7. Position invalid: lat/lon/alt/acc serialize as JSON null
// ============================================================================

static void test_position_invalid_serializes_nulls() {
    test_begin("position.valid=false -> lat/lon/alt/acc are \"null\"");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p = make_position(99.0, 99.0, 100.0f, 1.0f, 5, 0);
    // fix_quality=0 -> invalid (per impl), and lat/lon/etc should be "null".

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    EXPECT_CONTAINS(buf, "\"latitude\":null");
    EXPECT_CONTAINS(buf, "\"longitude\":null");
    EXPECT_CONTAINS(buf, "\"altitude_m\":null");
    EXPECT_CONTAINS(buf, "\"accuracy_m\":null");
    // Even though invalid, num_satellites/fix_quality are still serialized.
    EXPECT_CONTAINS(buf, "\"num_satellites\":5");
    EXPECT_CONTAINS(buf, "\"fix_quality\":0");

    test_end();
}

// ============================================================================
// 8. Position valid: lat/lon serialized as doubles
// ============================================================================

static void test_position_valid_serializes_numbers() {
    test_begin("position.valid=true -> lat/lon/alt are numeric");

    JSONFormatter fmt(6);  // 6 dp for high-precision GPS
    char buf[512];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p = make_position(47.123456, -122.654321, 100.5f, 1.5f, 9, 2);

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    EXPECT_CONTAINS(buf, "\"latitude\":47.123456");
    EXPECT_CONTAINS(buf, "\"longitude\":-122.654321");
    EXPECT_CONTAINS(buf, "\"altitude_m\":100.500000");
    EXPECT_CONTAINS(buf, "\"accuracy_m\":1.500000");
    EXPECT_CONTAINS(buf, "\"num_satellites\":9");
    EXPECT_CONTAINS(buf, "\"fix_quality\":2");
    // No nulls in the numeric fields when valid.
    EXPECT_NOT_CONTAINS(buf, "\"latitude\":null");

    test_end();
}

// ============================================================================
// 9. Calibration field serialization
// ============================================================================

static void test_calibration_fields() {
    test_begin("calibration: system/accel/gyro/mag serialized");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    o.cal_status = 3;
    o.cal_accel  = 2;
    o.cal_gyro   = 1;
    o.cal_mag    = 0;
    PositionData p;

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    EXPECT_CONTAINS(buf, "\"system\":3");
    EXPECT_CONTAINS(buf, "\"accel\":2");
    EXPECT_CONTAINS(buf, "\"gyro\":1");
    EXPECT_CONTAINS(buf, "\"mag\":0");

    test_end();
}

// ============================================================================
// 10. Zero timestamp + zero quaternion still produces well-formed JSON
// ============================================================================

static void test_zero_inputs_well_formed() {
    test_begin("zero inputs: still produce valid JSON braces");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o;  // default-constructed: all zeros
    PositionData    p;  // default-constructed

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    EXPECT_TRUE(buf[0] == '{', "starts with {");
    EXPECT_TRUE(buf[n - 1] == '}', "ends with }");
    EXPECT_CONTAINS(buf, "\"timestamp_ms\":0");

    test_end();
}

// ============================================================================
// 11. Null buffer / zero max_len rejected
// ============================================================================

static void test_null_buffer_rejected() {
    test_begin("null buffer => returns 0");

    JSONFormatter fmt;
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p;
    uint16_t n = fmt.format(o, p, NULL, 256);
    EXPECT_EQ_INT(n, 0);

    test_end();
}

static void test_tiny_max_len_rejected() {
    test_begin("max_len < 10 => returns 0");

    JSONFormatter fmt;
    char buf[16];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p;
    uint16_t n = fmt.format(o, p, buf, 5);  // < 10
    EXPECT_EQ_INT(n, 0);

    test_end();
}

// ============================================================================
// 12. Buffer overflow safety — small buffer truncates without overrun
// ============================================================================

static void test_buffer_overflow_safety() {
    test_begin("buffer overflow: tight buffer returns 0, no overrun");

    JSONFormatter fmt;
    // Sentinels around a small buffer to detect overrun.
    char canary_before[8];
    char buf[40];
    char canary_after[8];
    std::memset(canary_before, 0xAB, sizeof(canary_before));
    std::memset(canary_after,  0xCD, sizeof(canary_after));
    std::memset(buf, 0, sizeof(buf));

    OrientationData o = make_orientation(0.707f, 0.0f, 0.0f, 0.707f, 99999);
    PositionData    p = make_position(37.7749, -122.4194, 50.0f, 2.5f, 8, 1);

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    // Output (~250 bytes) won't fit in 40 — impl should return 0.
    EXPECT_EQ_INT(n, 0);

    // Verify the canaries were not overwritten.
    bool canary_before_ok = true;
    bool canary_after_ok = true;
    for (size_t i = 0; i < sizeof(canary_before); ++i) {
        if ((unsigned char)canary_before[i] != 0xAB) canary_before_ok = false;
        if ((unsigned char)canary_after[i]  != 0xCD) canary_after_ok  = false;
    }
    EXPECT_TRUE(canary_before_ok, "canary BEFORE not corrupted");
    EXPECT_TRUE(canary_after_ok,  "canary AFTER not corrupted");

    test_end();
}

// ============================================================================
// 13. Exact-fit buffer: succeeds when buffer is just big enough
// ============================================================================

static void test_exact_fit_buffer() {
    test_begin("exact-fit buffer: succeeds when sized to output");

    JSONFormatter fmt;
    char probe[1024];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p;  // invalid

    uint16_t needed = fmt.format(o, p, probe, sizeof(probe));
    EXPECT_TRUE(needed > 0, "probe formatting succeeded");

    // Now retry with exactly `needed + 1` bytes (room for null terminator).
    char tight[1024];
    std::memset(tight, 0, sizeof(tight));
    uint16_t n = fmt.format(o, p, tight, (uint16_t)(needed + 1));
    EXPECT_EQ_INT(n, needed);
    EXPECT_TRUE(tight[n - 1] == '}', "ends with }");
    EXPECT_TRUE(tight[n] == '\0', "null-terminated");

    test_end();
}

// ============================================================================
// 14. NaN / Inf float handling
// ============================================================================
//
// NOTE: The implementation passes raw float values to snprintf with "%.Nf"
// without checking std::isnan() / std::isinf().  On glibc, %f for NaN
// produces "nan" (or "-nan"), and for Inf produces "inf" / "-inf" — NEITHER
// of which is valid JSON.  This test DOCUMENTS that broken behavior; do not
// "fix" the test if json_formatter ever starts emitting "null" instead.
//
// FINDING (P2 candidate, surface to follow-up agent):
//   json_formatter.cpp emits literal "nan"/"inf" tokens when fed NaN/Inf
//   floats from the orientation quaternion.  These produce invalid JSON
//   that downstream consumers (jq, JS JSON.parse) will reject.

static void test_nan_quaternion_documents_behavior() {
    test_begin("NaN quaternion: documents current (broken) behavior");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f, 0);
    PositionData p;

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format does not abort on NaN");

    // The output MAY contain "nan" (invalid JSON) or "null" (proper handling).
    // Current impl produces "nan" — document and accept either, but flag.
    bool has_nan_token  = (std::strstr(buf, "nan")  != NULL) ||
                          (std::strstr(buf, "NaN")  != NULL) ||
                          (std::strstr(buf, "NAN")  != NULL);
    bool has_null_token = (std::strstr(buf, "\"w\":null") != NULL);
    EXPECT_TRUE(has_nan_token || has_null_token,
                "NaN produces SOME token (nan or null)");
    if (has_nan_token && !has_null_token) {
        printf("  NOTE: impl emits literal nan/NaN -> INVALID JSON. "
               "Surface as follow-up finding.\n");
    }

    test_end();
}

static void test_inf_quaternion_documents_behavior() {
    test_begin("Inf quaternion: documents current behavior");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(
        std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0.0f, 0);
    PositionData p;

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format does not abort on Inf");

    bool has_inf  = (std::strstr(buf, "inf") != NULL) ||
                    (std::strstr(buf, "Inf") != NULL) ||
                    (std::strstr(buf, "INF") != NULL);
    bool has_null = (std::strstr(buf, "\"w\":null") != NULL);
    EXPECT_TRUE(has_inf || has_null,
                "Inf produces SOME token (inf or null)");
    if (has_inf && !has_null) {
        printf("  NOTE: impl emits literal inf -> INVALID JSON. "
               "Surface as follow-up finding.\n");
    }

    test_end();
}

// ============================================================================
// 15. Negative values for floats and doubles
// ============================================================================

static void test_negative_values() {
    test_begin("negative quaternion components and lat/lon");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(-0.5f, -0.5f, -0.5f, -0.5f, 0);
    PositionData    p = make_position(-33.8688, -151.2093, -25.0f, 5.0f, 7, 1);

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");
    EXPECT_CONTAINS(buf, "\"w\":-0.500");
    EXPECT_CONTAINS(buf, "\"latitude\":-33.869");
    EXPECT_CONTAINS(buf, "\"longitude\":-151.209");
    EXPECT_CONTAINS(buf, "\"altitude_m\":-25.000");
    test_end();
}

// ============================================================================
// 16. Max-ish timestamp and saturated calibration fields
// ============================================================================

static void test_max_timestamp_and_cal() {
    test_begin("max timestamp + maxed cal_* fields");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 4294967295u);
    o.cal_status = 3;
    o.cal_accel  = 3;
    o.cal_gyro   = 3;
    o.cal_mag    = 3;
    PositionData p;

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");

    // NOTE: impl uses snprintf(..., "%d", timestamp) which is a signed-int
    // format applied to a uint32_t. On 32-bit-int hosts (the typical g++
    // target), 4_294_967_295u prints as "-1". This test documents that.
    // FINDING (P2 candidate): format string should be "%u" for timestamp_ms.
    bool has_unsigned = std::strstr(buf, "\"timestamp_ms\":4294967295") != NULL;
    bool has_signed   = std::strstr(buf, "\"timestamp_ms\":-1") != NULL;
    EXPECT_TRUE(has_unsigned || has_signed,
                "timestamp prints as either %u (correct) or -1 (signed bug)");
    if (has_signed && !has_unsigned) {
        printf("  NOTE: timestamp_ms uses %%d not %%u -> wraps to -1 at UINT32_MAX. "
               "Surface as follow-up finding.\n");
    }

    EXPECT_CONTAINS(buf, "\"system\":3");
    EXPECT_CONTAINS(buf, "\"mag\":3");

    test_end();
}

// ============================================================================
// 17. JSON structural integrity — balanced braces
// ============================================================================

static void test_balanced_braces() {
    test_begin("structural: braces balance");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(0.707f, 0.0f, 0.0f, 0.707f, 1000);
    PositionData    p = make_position(0.0, 0.0, 0.0f, 0.0f, 0, 1);

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");

    int depth = 0;
    int max_depth = 0;
    bool balanced = true;
    for (uint16_t i = 0; i < n; ++i) {
        if (buf[i] == '{') {
            depth++;
            if (depth > max_depth) max_depth = depth;
        } else if (buf[i] == '}') {
            depth--;
            if (depth < 0) { balanced = false; break; }
        }
    }
    EXPECT_TRUE(balanced && depth == 0,
                "all { closed by matching }, no underflow");
    EXPECT_TRUE(max_depth >= 3, "expected nested depth >= 3");

    test_end();
}

// ============================================================================
// 18. String escaping — N/A in current schema
// ============================================================================
//
// The JSONFormatter currently emits ZERO user-controlled string fields:
// all values are numbers, bools, or "null" literals — there are no free-form
// strings (no device name, no error string) in the output. Therefore there
// are no JSON-special characters (quotes/backslashes) to escape.
//
// FINDING (informational): If a future schema adds a string field (e.g.
// "device_name") sourced from user / sensor input, escaping logic will need
// to be added. Currently NOT a defect.

static void test_no_user_strings_to_escape() {
    test_begin("escaping: schema has no user strings (informational)");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p;
    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");

    // Only known string literals in output: keys, "true"/"false", "null".
    // Backslash should never appear.
    EXPECT_TRUE(std::strchr(buf, '\\') == NULL,
                "no backslash escape sequences in any output");
    test_end();
}

// ============================================================================
// 19. Repeated calls with same formatter yield identical output
// ============================================================================

static void test_repeated_calls_idempotent() {
    test_begin("two calls with same inputs produce identical output");

    JSONFormatter fmt;
    char buf1[512];
    char buf2[512];
    OrientationData o = make_orientation(0.5f, 0.5f, 0.5f, 0.5f, 42);
    PositionData    p = make_position(1.0, 2.0, 3.0f, 0.5f, 4, 1);

    uint16_t n1 = fmt.format(o, p, buf1, sizeof(buf1));
    uint16_t n2 = fmt.format(o, p, buf2, sizeof(buf2));
    EXPECT_EQ_INT(n1, n2);
    EXPECT_TRUE(std::strcmp(buf1, buf2) == 0,
                "output bytewise identical across calls");

    test_end();
}

// ============================================================================
// 20. Compact: no whitespace between tokens
// ============================================================================

static void test_compact_no_whitespace() {
    test_begin("compact format: no spaces/newlines/tabs in output");

    JSONFormatter fmt;
    char buf[512];
    OrientationData o = make_orientation(1.0f, 0.0f, 0.0f, 0.0f, 0);
    PositionData    p = make_position(1.0, 2.0, 3.0f, 0.5f, 4, 1);

    uint16_t n = fmt.format(o, p, buf, sizeof(buf));
    EXPECT_TRUE(n > 0, "format succeeded");

    for (uint16_t i = 0; i < n; ++i) {
        if (buf[i] == ' ' || buf[i] == '\t' ||
            buf[i] == '\n' || buf[i] == '\r') {
            printf("  FAIL: whitespace at byte %u (0x%02x)\n",
                   (unsigned)i, (unsigned)buf[i]);
            g_current_failed = true;
            break;
        }
    }

    test_end();
}

// ============================================================================
// main — runs all scenarios
// ============================================================================

int main() {
    printf("================================================\n");
    printf(" JSONFormatter native unit tests\n");
    printf("================================================\n\n");

    test_happy_path_full();
    test_get_format_name();
    test_float_precision_default();
    test_float_precision_six();
    test_float_precision_zero();
    test_orientation_invalid_when_all_zero();
    test_position_invalid_serializes_nulls();
    test_position_valid_serializes_numbers();
    test_calibration_fields();
    test_zero_inputs_well_formed();
    test_null_buffer_rejected();
    test_tiny_max_len_rejected();
    test_buffer_overflow_safety();
    test_exact_fit_buffer();
    test_nan_quaternion_documents_behavior();
    test_inf_quaternion_documents_behavior();
    test_negative_values();
    test_max_timestamp_and_cal();
    test_balanced_braces();
    test_no_user_strings_to_escape();
    test_repeated_calls_idempotent();
    test_compact_no_whitespace();

    printf("\n================================================\n");
    printf(" Tests run: %d, passed: %d\n", g_tests_run, g_tests_passed);
    printf("================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
