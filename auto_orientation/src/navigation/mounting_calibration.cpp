/**
 * Phase 4.3 — One-shot mounting-angle capture (implementation).
 *
 * See `mounting_calibration.h` for the API contract and the EEPROM record
 * layout. This translation unit is intentionally lean — no Arduino-only
 * headers, only `<math.h>` and `<string.h>`. That keeps it host-testable
 * and lets it build on AVR / ARM / x86 without surgery.
 */

#include "mounting_calibration.h"

#include <math.h>
#include <string.h>

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

const float kDegToRad = 0.01745329251994329577f;   // π / 180

inline float vec_norm3(const float v[3]) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

inline void vec_normalize3(const float in[3], float out[3]) {
    const float n = vec_norm3(in);
    if (n < 1e-9f) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
        return;
    }
    const float inv = 1.0f / n;
    out[0] = in[0] * inv;
    out[1] = in[1] * inv;
    out[2] = in[2] * inv;
}

inline void quat_normalize(float q[4]) {
    const float n2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (n2 < 1e-18f) {
        q[0] = 1.0f;
        q[1] = 0.0f;
        q[2] = 0.0f;
        q[3] = 0.0f;
        return;
    }
    const float inv = 1.0f / sqrtf(n2);
    q[0] *= inv;
    q[1] *= inv;
    q[2] *= inv;
    q[3] *= inv;
}

}  // namespace


// ============================================================================
// CRC8 — simple XOR (matches calculateCRC8 in calibration_storage.cpp)
// ============================================================================

uint8_t auto_orient_crc8(const uint8_t* data, uint16_t len) {
    if (!data || len == 0) return 0;
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= data[i];
    }
    return crc;
}


// ============================================================================
// shortest_arc_quaternion — Melax (Game Programming Gems 1, 2000)
// ============================================================================

void MountingCalibration::shortest_arc_quaternion(const float observed_g[3],
                                                  const float target_g[3],
                                                  float q_out[4]) {
    // Default to identity in case of pathological inputs.
    q_out[0] = 1.0f; q_out[1] = 0.0f; q_out[2] = 0.0f; q_out[3] = 0.0f;

    float a[3], b[3];
    vec_normalize3(observed_g, a);
    vec_normalize3(target_g,   b);

    // Either input degenerate? Return identity.
    if ((a[0] == 0.0f && a[1] == 0.0f && a[2] == 0.0f) ||
        (b[0] == 0.0f && b[1] == 0.0f && b[2] == 0.0f)) {
        return;
    }

    const float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];   // cos(angle)

    // Case 1: already aligned (dot ≈ +1) → identity.
    if (d > 0.999999f) {
        return;
    }

    // Case 2: 180° flip (dot ≈ -1). Cross product is zero — pick a
    // deterministic perpendicular axis.
    if (d < -0.999999f) {
        // Choose the world-axis least parallel to `a`.
        float axis[3] = {1.0f, 0.0f, 0.0f};
        if (fabsf(a[0]) > 0.9f) {
            axis[0] = 0.0f; axis[1] = 1.0f; axis[2] = 0.0f;
        }
        // Make `axis` perpendicular to `a`: axis -= (axis·a) * a, then normalize.
        const float dot_aa = axis[0]*a[0] + axis[1]*a[1] + axis[2]*a[2];
        axis[0] -= dot_aa * a[0];
        axis[1] -= dot_aa * a[1];
        axis[2] -= dot_aa * a[2];
        float axn[3];
        vec_normalize3(axis, axn);
        q_out[0] = 0.0f;
        q_out[1] = axn[0];
        q_out[2] = axn[1];
        q_out[3] = axn[2];
        return;
    }

    // General case: q = { s*0.5, cross.x/s, cross.y/s, cross.z/s }, s = sqrt((1+d)*2)
    const float cx = a[1] * b[2] - a[2] * b[1];
    const float cy = a[2] * b[0] - a[0] * b[2];
    const float cz = a[0] * b[1] - a[1] * b[0];

    const float s     = sqrtf((1.0f + d) * 2.0f);
    const float inv_s = 1.0f / s;

    q_out[0] = s * 0.5f;
    q_out[1] = cx * inv_s;
    q_out[2] = cy * inv_s;
    q_out[3] = cz * inv_s;

    quat_normalize(q_out);
}


// ============================================================================
// MountingCalibration
// ============================================================================

MountingCalibration::MountingCalibration()
    : state_(STATE_IDLE),
      last_error_(""),
      gyro_max_rad_per_sec_(0.5f),
      capture_duration_ms_(200),
      still_samples_required_(3),
      accel_lp_alpha_(0.1f),
      consecutive_still_count_(0),
      capture_start_ms_(0),
      capture_sample_count_(0),
      var_mean_(0.0f),
      var_m2_(0.0f),
      var_n_(0),
      stillness_var_(0.0f)
{
    accel_lp_[0] = 0.0f;
    accel_lp_[1] = 0.0f;
    accel_lp_[2] = 0.0f;

    // Result starts as identity so callers reading get_quaternion() before
    // completion get a sane "no rotation" answer.
    result_q_[0] = 1.0f;
    result_q_[1] = 0.0f;
    result_q_[2] = 0.0f;
    result_q_[3] = 0.0f;
}

void MountingCalibration::set_stillness_threshold(float gyro_max_rad_per_sec) {
    gyro_max_rad_per_sec_ = gyro_max_rad_per_sec;
}

void MountingCalibration::set_capture_duration_ms(uint16_t ms) {
    capture_duration_ms_ = ms;
}

void MountingCalibration::set_sample_count_required(uint8_t n) {
    still_samples_required_ = (n == 0) ? 1 : n;
}

void MountingCalibration::start_capture() {
    state_                   = STATE_WAITING_STILL;
    last_error_              = "";
    consecutive_still_count_ = 0;
    capture_start_ms_        = 0;
    capture_sample_count_    = 0;
    accel_lp_[0] = accel_lp_[1] = accel_lp_[2] = 0.0f;
    var_mean_ = 0.0f;
    var_m2_   = 0.0f;
    var_n_    = 0;
    stillness_var_ = 0.0f;
    result_q_[0] = 1.0f;
    result_q_[1] = 0.0f;
    result_q_[2] = 0.0f;
    result_q_[3] = 0.0f;
}

bool MountingCalibration::is_capturing() const {
    return state_ == STATE_WAITING_STILL || state_ == STATE_CAPTURING;
}

bool MountingCalibration::is_complete() const {
    return state_ == STATE_COMPLETE;
}

bool MountingCalibration::has_failed() const {
    return state_ == STATE_FAILED;
}

const char* MountingCalibration::last_error() const {
    return last_error_;
}

void MountingCalibration::feed_sample(const float accel_g[3],
                                      const float gyro_dps[3],
                                      uint32_t now_ms) {
    if (state_ == STATE_IDLE || state_ == STATE_COMPLETE || state_ == STATE_FAILED) {
        return;
    }

    // Stillness gate (gyro magnitude in rad/s).
    const float gx = gyro_dps[0] * kDegToRad;
    const float gy = gyro_dps[1] * kDegToRad;
    const float gz = gyro_dps[2] * kDegToRad;
    const float gyro_mag_rad = sqrtf(gx * gx + gy * gy + gz * gz);
    const bool  is_still     = (gyro_mag_rad < gyro_max_rad_per_sec_);

    if (is_still) {
        if (consecutive_still_count_ < 255) consecutive_still_count_++;
    } else {
        consecutive_still_count_ = 0;
    }

    if (state_ == STATE_WAITING_STILL) {
        if (consecutive_still_count_ >= still_samples_required_) {
            // Transition to CAPTURING, seed the EMA with the current sample.
            state_                = STATE_CAPTURING;
            capture_start_ms_     = now_ms;
            capture_sample_count_ = 0;
            accel_lp_[0] = accel_g[0];
            accel_lp_[1] = accel_g[1];
            accel_lp_[2] = accel_g[2];
            var_mean_ = 0.0f;
            var_m2_   = 0.0f;
            var_n_    = 0;
        }
        return;
    }

    // STATE_CAPTURING --------------------------------------------------------

    // Motion during capture is a fatal abort — the gravity estimate would be
    // contaminated and the user clearly disturbed the rig mid-gesture.
    if (!is_still) {
        state_      = STATE_FAILED;
        last_error_ = "motion detected during capture";
        return;
    }

    // Exponential moving average on accel.
    const float a = accel_lp_alpha_;
    const float one_minus_a = 1.0f - a;
    accel_lp_[0] = accel_lp_[0] * one_minus_a + accel_g[0] * a;
    accel_lp_[1] = accel_lp_[1] * one_minus_a + accel_g[1] * a;
    accel_lp_[2] = accel_lp_[2] * one_minus_a + accel_g[2] * a;
    capture_sample_count_++;

    // Welford variance on accel magnitude (QC metric).
    const float amag = sqrtf(accel_g[0] * accel_g[0] +
                             accel_g[1] * accel_g[1] +
                             accel_g[2] * accel_g[2]);
    var_n_++;
    const float delta  = amag - var_mean_;
    var_mean_         += delta / static_cast<float>(var_n_);
    const float delta2 = amag - var_mean_;
    var_m2_           += delta * delta2;

    // Capture window complete?
    if ((now_ms - capture_start_ms_) >= capture_duration_ms_) {
        // Compute mounting quaternion: rotate observed gravity onto body "down".
        const float target_down[3] = { 0.0f, 0.0f, -1.0f };
        shortest_arc_quaternion(accel_lp_, target_down, result_q_);

        stillness_var_ = (var_n_ > 1) ? (var_m2_ / static_cast<float>(var_n_ - 1)) : 0.0f;
        state_         = STATE_COMPLETE;
    }
}

const float* MountingCalibration::get_quaternion() const {
    return result_q_;
}

float MountingCalibration::get_stillness_variance() const {
    return stillness_var_;
}

void MountingCalibration::reset() {
    state_                   = STATE_IDLE;
    last_error_              = "";
    consecutive_still_count_ = 0;
    capture_start_ms_        = 0;
    capture_sample_count_    = 0;
    accel_lp_[0] = accel_lp_[1] = accel_lp_[2] = 0.0f;
    var_mean_ = 0.0f;
    var_m2_   = 0.0f;
    var_n_    = 0;
    stillness_var_ = 0.0f;
    result_q_[0] = 1.0f;
    result_q_[1] = 0.0f;
    result_q_[2] = 0.0f;
    result_q_[3] = 0.0f;
}


// ============================================================================
// Serialization — 24-byte record (see header for layout)
// ============================================================================

bool MountingCalibration::serialize(uint8_t* buf, uint16_t buf_size) const {
    if (!buf || buf_size < AUTO_ORIENT_RECORD_SIZE) return false;
    if (state_ != STATE_COMPLETE)                   return false;

    buf[0] = AUTO_ORIENT_RECORD_MAGIC;
    buf[1] = AUTO_ORIENT_RECORD_VERSION;

    // q_mount[4] at offset 2..17  (4 x float32 LE, native endianness on AVR/ARM/x86)
    memcpy(buf + 2,  &result_q_[0], 4);
    memcpy(buf + 6,  &result_q_[1], 4);
    memcpy(buf + 10, &result_q_[2], 4);
    memcpy(buf + 14, &result_q_[3], 4);

    // qc_stillness_var at offset 18..21
    memcpy(buf + 18, &stillness_var_, 4);

    // qc_capture_age_min at offset 22 — caller may overwrite before persisting.
    buf[22] = 0;

    // CRC8 over bytes 0..22
    buf[23] = auto_orient_crc8(buf, 23);
    return true;
}

bool MountingCalibration::deserialize(const uint8_t* buf, uint16_t buf_size) {
    if (!buf || buf_size < AUTO_ORIENT_RECORD_SIZE) return false;
    if (buf[0] != AUTO_ORIENT_RECORD_MAGIC)         return false;
    if (buf[1] != AUTO_ORIENT_RECORD_VERSION)       return false;

    const uint8_t expected_crc = auto_orient_crc8(buf, 23);
    if (buf[23] != expected_crc)                    return false;

    float q[4];
    float var;
    memcpy(&q[0], buf + 2,  4);
    memcpy(&q[1], buf + 6,  4);
    memcpy(&q[2], buf + 10, 4);
    memcpy(&q[3], buf + 14, 4);
    memcpy(&var,  buf + 18, 4);

    result_q_[0]   = q[0];
    result_q_[1]   = q[1];
    result_q_[2]   = q[2];
    result_q_[3]   = q[3];
    stillness_var_ = var;
    state_         = STATE_COMPLETE;
    last_error_    = "";
    return true;
}
