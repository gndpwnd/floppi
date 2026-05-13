/**
 * Phase 4.3 unit tests — MountingCalibration.
 *
 * Test pattern matches `tests/test_quaternion.cpp` (assert helpers +
 * top-level main()). Build & run as a host program; no Arduino required.
 *
 * Build (example):
 *   g++ -O0 -g -Wall -I src \
 *       tests/test_mounting_calibration.cpp \
 *       src/navigation/mounting_calibration.cpp \
 *       -o tests/test_mounting_calibration_runner
 *   ./tests/test_mounting_calibration_runner
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "../src/navigation/mounting_calibration.h"

// ============================================================================
// TEST UTILITIES
// ============================================================================

static const float EPSILON      = 1e-4f;
static const float EPSILON_LOOSE = 1e-3f;

static int g_tests_run    = 0;
static int g_tests_passed = 0;

static void assert_true(bool cond, const char* msg) {
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        std::exit(1);
    }
}

static void assert_near(float actual, float expected, float tol, const char* msg) {
    if (std::fabs(actual - expected) > tol) {
        std::printf("FAIL: %s (expected %.6f, got %.6f, diff %.6f)\n",
                    msg, expected, actual, std::fabs(actual - expected));
        std::exit(1);
    }
}

static void assert_vec3_near(const float actual[3], const float expected[3],
                             float tol, const char* msg) {
    if (std::fabs(actual[0] - expected[0]) > tol ||
        std::fabs(actual[1] - expected[1]) > tol ||
        std::fabs(actual[2] - expected[2]) > tol) {
        std::printf("FAIL: %s (expected [%.4f, %.4f, %.4f], got [%.4f, %.4f, %.4f])\n",
                    msg, expected[0], expected[1], expected[2],
                    actual[0], actual[1], actual[2]);
        std::exit(1);
    }
}

// Rotate a 3-vector by a quaternion using v' = q * v * q^*.
// Returns the rotated vector in `out`.
static void rotate_vec_by_quat(const float q[4], const float v[3], float out[3]) {
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    // Pre-compute matrix terms.
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;

    out[0] = (1.0f - 2.0f * (yy + zz)) * v[0]
           + (2.0f * (xy - wz))        * v[1]
           + (2.0f * (xz + wy))        * v[2];
    out[1] = (2.0f * (xy + wz))        * v[0]
           + (1.0f - 2.0f * (xx + zz)) * v[1]
           + (2.0f * (yz - wx))        * v[2];
    out[2] = (2.0f * (xz - wy))        * v[0]
           + (2.0f * (yz + wx))        * v[1]
           + (1.0f - 2.0f * (xx + yy)) * v[2];
}

static float quat_magnitude(const float q[4]) {
    return std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
}

#define TEST(name) do { \
    g_tests_run++; \
    std::printf("\nTest %d: %s\n", g_tests_run, #name); \
    name(); \
    g_tests_passed++; \
    std::printf("  PASS: %s\n", #name); \
} while (0)


// ============================================================================
// SHORTEST-ARC QUATERNION TESTS
// ============================================================================

static void test_shortest_arc_identity() {
    // observed == target == [0, 0, -1]  →  identity quaternion.
    const float observed[3] = { 0.0f, 0.0f, -1.0f };
    const float target[3]   = { 0.0f, 0.0f, -1.0f };
    float q[4];
    MountingCalibration::shortest_arc_quaternion(observed, target, q);

    assert_near(q[0], 1.0f, EPSILON, "identity: w == 1");
    assert_near(q[1], 0.0f, EPSILON, "identity: x == 0");
    assert_near(q[2], 0.0f, EPSILON, "identity: y == 0");
    assert_near(q[3], 0.0f, EPSILON, "identity: z == 0");
    assert_near(quat_magnitude(q), 1.0f, EPSILON, "identity: |q| == 1");
}

static void test_shortest_arc_known_45_deg_pitch() {
    // observed = (sin45, 0, -cos45) → device pitched +45° about Y so X-axis
    // dips down. Target is body-down [0, 0, -1]. Applying q to observed must
    // produce target.
    const float s = std::sin(static_cast<float>(M_PI / 4.0));
    const float c = std::cos(static_cast<float>(M_PI / 4.0));
    const float observed[3] = { s, 0.0f, -c };
    const float target[3]   = { 0.0f, 0.0f, -1.0f };

    float q[4];
    MountingCalibration::shortest_arc_quaternion(observed, target, q);

    assert_near(quat_magnitude(q), 1.0f, EPSILON, "45° pitch: |q| == 1");

    float rotated[3];
    rotate_vec_by_quat(q, observed, rotated);
    assert_vec3_near(rotated, target, EPSILON, "45° pitch: rotate(observed) ≈ target");
}

static void test_shortest_arc_known_30_deg_roll() {
    // observed = (0, sin30, -cos30): device rolled +30° about X.
    const float s = std::sin(static_cast<float>(M_PI / 6.0));
    const float c = std::cos(static_cast<float>(M_PI / 6.0));
    const float observed[3] = { 0.0f, s, -c };
    const float target[3]   = { 0.0f, 0.0f, -1.0f };

    float q[4];
    MountingCalibration::shortest_arc_quaternion(observed, target, q);

    float rotated[3];
    rotate_vec_by_quat(q, observed, rotated);
    assert_vec3_near(rotated, target, EPSILON, "30° roll: rotate(observed) ≈ target");
}

static void test_shortest_arc_180_flip() {
    // observed = [0, 0, +1] (device upside-down). Target = [0, 0, -1].
    // The rotation must be 180° about *some* axis in the XY plane; we don't
    // care which — only that rotating observed by q yields target.
    const float observed[3] = { 0.0f, 0.0f, 1.0f };
    const float target[3]   = { 0.0f, 0.0f, -1.0f };

    float q[4];
    MountingCalibration::shortest_arc_quaternion(observed, target, q);

    assert_near(quat_magnitude(q), 1.0f, EPSILON, "180° flip: |q| == 1");
    // For a pure 180° rotation, w must be ~0.
    assert_near(q[0], 0.0f, EPSILON, "180° flip: w ≈ 0");

    float rotated[3];
    rotate_vec_by_quat(q, observed, rotated);
    assert_vec3_near(rotated, target, EPSILON, "180° flip: rotate(observed) ≈ target");
}

static void test_shortest_arc_unnormalized_input() {
    // observed has length 9.81 (m/s^2-scale) — function must normalize
    // internally and still produce the same result.
    const float observed[3] = { 0.0f, 5.0f, -8.5f };
    const float target[3]   = { 0.0f, 0.0f, -1.0f };

    float q[4];
    MountingCalibration::shortest_arc_quaternion(observed, target, q);

    assert_near(quat_magnitude(q), 1.0f, EPSILON, "unnormalized: |q| == 1");

    // Rotating the normalized observed vector should land on target.
    const float n = std::sqrt(observed[0]*observed[0] +
                              observed[1]*observed[1] +
                              observed[2]*observed[2]);
    const float observed_n[3] = { observed[0]/n, observed[1]/n, observed[2]/n };
    float rotated[3];
    rotate_vec_by_quat(q, observed_n, rotated);
    assert_vec3_near(rotated, target, EPSILON, "unnormalized: rotate(observed_n) ≈ target");
}

static void test_shortest_arc_zero_input() {
    // Zero-length input must not produce NaN — function should return identity.
    const float observed[3] = { 0.0f, 0.0f, 0.0f };
    const float target[3]   = { 0.0f, 0.0f, -1.0f };

    float q[4];
    MountingCalibration::shortest_arc_quaternion(observed, target, q);

    assert_near(q[0], 1.0f, EPSILON, "zero input: identity w");
    assert_near(q[1], 0.0f, EPSILON, "zero input: identity x");
    assert_near(q[2], 0.0f, EPSILON, "zero input: identity y");
    assert_near(q[3], 0.0f, EPSILON, "zero input: identity z");
}


// ============================================================================
// STATE-MACHINE TESTS
// ============================================================================

static void test_capture_idle_ignores_samples() {
    MountingCalibration cal;
    const float a_still[3] = { 0.0f, 0.0f, -1.0f };
    const float g_still[3] = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < 100; ++i) {
        cal.feed_sample(a_still, g_still, static_cast<uint32_t>(i) * 10u);
    }

    assert_true(!cal.is_capturing(), "IDLE: not capturing");
    assert_true(!cal.is_complete(),  "IDLE: not complete");
    assert_true(!cal.has_failed(),   "IDLE: not failed");
}

static void test_capture_state_machine_happy_path() {
    MountingCalibration cal;
    cal.set_capture_duration_ms(200);
    cal.set_sample_count_required(3);

    cal.start_capture();
    assert_true(cal.is_capturing(), "after start_capture: capturing");

    // Device tilted exactly 45° about Y → observed = (sin45, 0, -cos45).
    const float s = std::sin(static_cast<float>(M_PI / 4.0));
    const float c = std::cos(static_cast<float>(M_PI / 4.0));
    const float a_tilted[3] = { s, 0.0f, -c };
    const float g_still[3]  = { 0.0f, 0.0f, 0.0f };

    // Feed 50 still samples at ~10 ms intervals (≈500 ms total). After the
    // first 3 we transition to CAPTURING; after another ~200 ms we COMPLETE.
    uint32_t t = 1000;
    for (int i = 0; i < 50; ++i) {
        cal.feed_sample(a_tilted, g_still, t);
        t += 10;
        if (cal.is_complete()) break;
    }

    assert_true(cal.is_complete(),   "happy path: complete after 200ms");
    assert_true(!cal.has_failed(),   "happy path: not failed");
    assert_true(!cal.is_capturing(), "happy path: no longer capturing");

    // Verify quaternion rotates observed gravity onto target [0, 0, -1].
    const float* q = cal.get_quaternion();
    assert_near(quat_magnitude(q), 1.0f, EPSILON, "happy path: |q| == 1");

    float rotated[3];
    const float target[3] = { 0.0f, 0.0f, -1.0f };
    rotate_vec_by_quat(q, a_tilted, rotated);
    // Slightly loosened tol: EMA introduces tiny bias over the first few samples.
    assert_vec3_near(rotated, target, EPSILON_LOOSE,
                     "happy path: rotate(observed) ≈ [0,0,-1]");
}

static void test_capture_waits_for_stillness() {
    MountingCalibration cal;
    cal.set_capture_duration_ms(200);
    cal.set_sample_count_required(3);
    cal.set_stillness_threshold(0.5f);  // rad/s

    cal.start_capture();

    const float a_still[3] = { 0.0f, 0.0f, -1.0f };
    const float g_moving[3] = { 60.0f, 60.0f, 60.0f };  // ~1.8 rad/s — moving

    // 20 ms of motion → should still be WAITING_STILL.
    uint32_t t = 1000;
    for (int i = 0; i < 10; ++i) {
        cal.feed_sample(a_still, g_moving, t);
        t += 10;
    }
    assert_true(cal.is_capturing(), "wait still: still in WAITING_STILL");
    assert_true(!cal.is_complete(), "wait still: not complete yet");
}

static void test_capture_aborts_on_motion() {
    MountingCalibration cal;
    cal.set_capture_duration_ms(200);
    cal.set_sample_count_required(3);

    cal.start_capture();

    const float a_still[3] = { 0.0f, 0.0f, -1.0f };
    const float g_still[3] = { 0.0f, 0.0f, 0.0f };

    // 3 still samples → transition to CAPTURING.
    uint32_t t = 1000;
    cal.feed_sample(a_still, g_still, t); t += 10;
    cal.feed_sample(a_still, g_still, t); t += 10;
    cal.feed_sample(a_still, g_still, t); t += 10;
    // One more still sample to make sure we're in CAPTURING (the transition
    // happens once the 3rd still sample arrives — at this point the next
    // sample will be processed as a CAPTURING sample).
    cal.feed_sample(a_still, g_still, t); t += 10;

    // Now inject a sample with large gyro → should fail.
    const float g_moving[3] = { 200.0f, 200.0f, 200.0f };  // ~6 rad/s
    cal.feed_sample(a_still, g_moving, t);

    assert_true(cal.has_failed(),    "abort: state == FAILED");
    assert_true(!cal.is_complete(),  "abort: not complete");
    assert_true(!cal.is_capturing(), "abort: not capturing");
    assert_true(std::strlen(cal.last_error()) > 0, "abort: error message set");
}

static void test_capture_reset() {
    MountingCalibration cal;
    cal.start_capture();
    assert_true(cal.is_capturing(), "before reset: capturing");
    cal.reset();
    assert_true(!cal.is_capturing(), "after reset: not capturing");
    assert_true(!cal.is_complete(),  "after reset: not complete");
    assert_true(!cal.has_failed(),   "after reset: not failed");

    // Quaternion should be back to identity.
    const float* q = cal.get_quaternion();
    assert_near(q[0], 1.0f, EPSILON, "reset: q.w == 1");
    assert_near(q[1], 0.0f, EPSILON, "reset: q.x == 0");
    assert_near(q[2], 0.0f, EPSILON, "reset: q.y == 0");
    assert_near(q[3], 0.0f, EPSILON, "reset: q.z == 0");
}


// ============================================================================
// SERIALIZATION TESTS
// ============================================================================

static void test_serialize_size() {
    assert_true(AUTO_ORIENT_RECORD_SIZE == 24, "serialize: record size is 24 bytes");
}

static void test_serialize_requires_complete() {
    MountingCalibration cal;
    uint8_t buf[24] = {0};
    bool ok = cal.serialize(buf, sizeof(buf));
    assert_true(!ok, "serialize: refuses to serialize when not COMPLETE");
}

static void test_serialize_round_trip() {
    // Drive a capture to completion, serialize, deserialize into a fresh
    // instance, and check the quaternion round-trips byte-for-byte.
    MountingCalibration cal;
    cal.set_capture_duration_ms(200);
    cal.set_sample_count_required(3);
    cal.start_capture();

    const float s = std::sin(static_cast<float>(M_PI / 6.0));   // 30° pitch
    const float c = std::cos(static_cast<float>(M_PI / 6.0));
    const float a_tilted[3] = { s, 0.0f, -c };
    const float g_still[3]  = { 0.0f, 0.0f, 0.0f };

    uint32_t t = 1000;
    for (int i = 0; i < 50 && !cal.is_complete(); ++i) {
        cal.feed_sample(a_tilted, g_still, t);
        t += 10;
    }
    assert_true(cal.is_complete(), "round trip: capture completed");

    uint8_t buf[24] = {0};
    bool ok = cal.serialize(buf, sizeof(buf));
    assert_true(ok, "round trip: serialize succeeded");

    // Verify on-the-wire header.
    assert_true(buf[0] == AUTO_ORIENT_RECORD_MAGIC,   "round trip: magic byte");
    assert_true(buf[1] == AUTO_ORIENT_RECORD_VERSION, "round trip: version byte");
    assert_true(buf[23] == auto_orient_crc8(buf, 23), "round trip: CRC self-consistent");

    const float orig_q[4]   = { cal.get_quaternion()[0], cal.get_quaternion()[1],
                                cal.get_quaternion()[2], cal.get_quaternion()[3] };
    const float orig_var    = cal.get_stillness_variance();

    MountingCalibration cal2;
    ok = cal2.deserialize(buf, sizeof(buf));
    assert_true(ok, "round trip: deserialize succeeded");
    assert_true(cal2.is_complete(), "round trip: deserialized state is COMPLETE");

    const float* q2 = cal2.get_quaternion();
    // Float bit-equality is feasible since we memcpy with no transformation.
    assert_true(std::memcmp(q2, orig_q, sizeof(orig_q)) == 0,
                "round trip: quaternion bytes match exactly");
    assert_true(std::memcmp(&orig_var, &(const float&)cal2.get_stillness_variance(),
                            sizeof(float)) == 0 ||
                std::fabs(cal2.get_stillness_variance() - orig_var) < 1e-9f,
                "round trip: stillness variance matches");
}

static void test_deserialize_bad_magic() {
    uint8_t buf[24] = {0};
    buf[0] = 0x00;   // Wrong magic.
    buf[1] = AUTO_ORIENT_RECORD_VERSION;
    buf[23] = auto_orient_crc8(buf, 23);

    MountingCalibration cal;
    bool ok = cal.deserialize(buf, sizeof(buf));
    assert_true(!ok, "bad magic: deserialize rejected");
}

static void test_deserialize_bad_version() {
    uint8_t buf[24] = {0};
    buf[0] = AUTO_ORIENT_RECORD_MAGIC;
    buf[1] = 0x99;   // Wrong version.
    buf[23] = auto_orient_crc8(buf, 23);

    MountingCalibration cal;
    bool ok = cal.deserialize(buf, sizeof(buf));
    assert_true(!ok, "bad version: deserialize rejected");
}

static void test_deserialize_bad_crc() {
    // Build a real record then corrupt the CRC.
    MountingCalibration cal;
    cal.set_capture_duration_ms(200);
    cal.set_sample_count_required(3);
    cal.start_capture();
    const float a[3] = { 0.0f, 0.0f, -1.0f };
    const float g[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t t = 1000;
    for (int i = 0; i < 50 && !cal.is_complete(); ++i) {
        cal.feed_sample(a, g, t);
        t += 10;
    }
    uint8_t buf[24] = {0};
    cal.serialize(buf, sizeof(buf));

    // Corrupt the CRC.
    buf[23] ^= 0xFF;

    MountingCalibration cal2;
    bool ok = cal2.deserialize(buf, sizeof(buf));
    assert_true(!ok, "bad CRC: deserialize rejected");
}

static void test_deserialize_short_buffer() {
    uint8_t buf[10] = {0};
    MountingCalibration cal;
    bool ok = cal.deserialize(buf, sizeof(buf));
    assert_true(!ok, "short buffer: deserialize rejected");
}

static void test_crc8_xor() {
    // Confirm the CRC matches the documented XOR semantics.
    const uint8_t data[5] = { 0xAB, 0x01, 0x10, 0x20, 0x30 };
    const uint8_t expected = 0xAB ^ 0x01 ^ 0x10 ^ 0x20 ^ 0x30;
    assert_true(auto_orient_crc8(data, 5) == expected, "crc8: matches XOR");
    assert_true(auto_orient_crc8(nullptr, 0) == 0, "crc8: null returns 0");
    assert_true(auto_orient_crc8(data, 0) == 0,    "crc8: zero len returns 0");
}


// ============================================================================
// CONFIG TESTS
// ============================================================================

static void test_stillness_threshold_strict() {
    // With a very tight threshold (0.0087 rad/s = 0.5°/s), small gyro should
    // *not* count as still.
    MountingCalibration cal;
    cal.set_stillness_threshold(0.0087f);
    cal.set_sample_count_required(3);
    cal.set_capture_duration_ms(200);
    cal.start_capture();

    const float a[3] = { 0.0f, 0.0f, -1.0f };
    const float g[3] = { 5.0f, 0.0f, 0.0f };   // 5°/s — above strict threshold

    uint32_t t = 1000;
    for (int i = 0; i < 50; ++i) {
        cal.feed_sample(a, g, t);
        t += 10;
    }
    assert_true(!cal.is_complete(), "strict threshold: never completes under motion");
    assert_true(cal.is_capturing(), "strict threshold: still in WAITING_STILL");
}


// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    std::printf("========================================\n");
    std::printf("MOUNTING CALIBRATION TEST SUITE\n");
    std::printf("========================================\n");

    // Shortest-arc quaternion math
    TEST(test_shortest_arc_identity);
    TEST(test_shortest_arc_known_45_deg_pitch);
    TEST(test_shortest_arc_known_30_deg_roll);
    TEST(test_shortest_arc_180_flip);
    TEST(test_shortest_arc_unnormalized_input);
    TEST(test_shortest_arc_zero_input);

    // State machine
    TEST(test_capture_idle_ignores_samples);
    TEST(test_capture_state_machine_happy_path);
    TEST(test_capture_waits_for_stillness);
    TEST(test_capture_aborts_on_motion);
    TEST(test_capture_reset);

    // Serialization / persistence
    TEST(test_serialize_size);
    TEST(test_serialize_requires_complete);
    TEST(test_serialize_round_trip);
    TEST(test_deserialize_bad_magic);
    TEST(test_deserialize_bad_version);
    TEST(test_deserialize_bad_crc);
    TEST(test_deserialize_short_buffer);
    TEST(test_crc8_xor);

    // Config
    TEST(test_stillness_threshold_strict);

    std::printf("\n========================================\n");
    std::printf("ALL %d TESTS PASSED\n", g_tests_passed);
    std::printf("========================================\n");
    return 0;
}
