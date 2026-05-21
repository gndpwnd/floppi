#ifndef MOUNTING_CALIBRATION_H
#define MOUNTING_CALIBRATION_H

#include <stdint.h>
#include <stddef.h>

/**
 * Phase 4.3 — One-shot mounting-angle capture.
 *
 * User holds the device at the desired "level" / balance point. The driver
 * gates on gyro stillness, low-pass filters the accelerometer for
 * ~200 ms to obtain a clean gravity estimate, then computes the
 * shortest-arc quaternion that rotates the observed gravity vector onto
 * the canonical body "+Z down" axis (`[0, 0, -1]`). That quaternion is the
 * mounting offset.
 *
 * Apply the INVERSE (conjugate) of `q_mount` to the live IMU attitude
 * quaternion to obtain an orientation referenced to the user-chosen level.
 *
 * Storage: 24-byte EEPROM record (magic 0xAB, version 0x01, q_mount[4],
 * stillness variance, capture age in minutes, CRC8). Layout below matches
 * `docs/findings/MASTER_DESIGN.md` §4.3 and
 * `docs/findings/balance_point_and_mounting_research.md` §2.
 *
 * Units note:
 *   - `accel_g[3]` is in *g* (1.0 ~= one Earth gravity). Either raw g or
 *     m/s^2 works as long as you are consistent — only the *direction* is
 *     used. We document g because BNO055/BNO085 expose accel in m/s^2
 *     and the caller is expected to scale (or just pass m/s^2 — direction
 *     is what matters).
 *   - `gyro_dps[3]` is in degrees per second.
 *
 * This module is intentionally standalone: it uses plain `float[4]`
 * quaternion arrays (w, x, y, z) and depends only on `<math.h>` and
 * `<string.h>` so it compiles cleanly on AVR (Arduino Mega 2560) without
 * pulling in the navigation/EKF tree.
 */

// ============================================================================
// EEPROM RECORD LAYOUT (24 bytes total)
// ============================================================================
//
//   offset  size  field
//     0      1    magic = 0xAB
//     1      1    version = 0x02
//     2-17  16    q_mount[4]            (float32 LE x 4: w, x, y, z)
//    18-21   4    qc_stillness_var      (float32 LE)
//     22     1    qc_capture_age_min    (uint8, capped at 255 minutes)
//     23     1    crc8                  (CRC-8-CCITT of bytes 0..22)
//
// Version history:
//   v0x01 — used a plain XOR-sum "CRC" that missed even-count column bitflips.
//   v0x02 — switched to the real shared util::crc8_ccitt(). The version byte
//           was deliberately bumped so any v1 record (which carries the weak
//           XOR checksum) is cleanly rejected on read instead of being
//           validated against the wrong algorithm.
//
// On AVR / ARM Cortex-M (Arduino, Teensy, ESP32), float is IEEE-754
// little-endian 32-bit. We `memcpy` to pack/unpack — no byte swapping
// needed for any of our target MCUs.

struct AutoOrientRecord {
    uint8_t magic;                  ///< 0xAB
    uint8_t version;                ///< 0x02
    float   q_mount[4];             ///< Mounting offset quaternion (w, x, y, z)
    float   qc_stillness_var;       ///< Variance of accel during capture (QC)
    uint8_t qc_capture_age_min;     ///< Minutes since capture, capped at 255
    uint8_t crc8;                   ///< CRC-8-CCITT of bytes 0..22
};

static const uint8_t  AUTO_ORIENT_RECORD_MAGIC   = 0xAB;
// Bumped 0x01 -> 0x02 in the 2026-05-21 EEPROM-security pass: the integrity
// byte changed from a weak XOR-sum to the real CRC-8-CCITT polynomial, so v1
// records must be rejected (not mis-validated). deserialize() enforces this.
static const uint8_t  AUTO_ORIENT_RECORD_VERSION = 0x02;
static const size_t   AUTO_ORIENT_RECORD_SIZE    = 24;

/**
 * CRC8 helper — CRC-8-CCITT (polynomial 0x07, init 0x00, no reflection,
 * no final XOR). Delegates to the shared `util::crc8_ccitt()` leaf in
 * `src/util/crc8.h`, the same canonical checksum used by
 * `calculateCRC8` in `src/config/calibration_storage.cpp`. The previous
 * implementation was a plain XOR-sum which silently missed any even-count
 * bitflip in a single column; that weakness is eliminated here.
 */
uint8_t auto_orient_crc8(const uint8_t* data, uint16_t len);


// ============================================================================
// MountingCalibration — one-shot capture state machine
// ============================================================================

class MountingCalibration {
public:
    MountingCalibration();

    // ---- Configuration -----------------------------------------------------

    /// Maximum gyro magnitude (rad/s) under which the device is considered
    /// "still". Default: 0.5 rad/s (~28.6°/s). Tighten to e.g. 0.0087 rad/s
    /// (0.5°/s) for a strict bench-cal gesture.
    void set_stillness_threshold(float gyro_max_rad_per_sec);

    /// Total duration of accel averaging once stillness has been confirmed.
    /// Default: 200 ms.
    void set_capture_duration_ms(uint16_t ms);

    /// Number of consecutive "still" samples required to *enter* capture.
    /// Default: 3.
    void set_sample_count_required(uint8_t n);

    // ---- Main interface ----------------------------------------------------

    void  start_capture();
    bool  is_capturing()  const;
    bool  is_complete()   const;
    bool  has_failed()    const;
    const char* last_error() const;

    /**
     * Feed one IMU sample into the calibrator. Called from the main loop.
     *
     * @param accel_g   3-axis acceleration. Units must contain gravity (i.e.
     *                  *not* gravity-removed linear-acceleration). g or m/s^2
     *                  both work — direction is what matters.
     * @param gyro_dps  3-axis angular rate in degrees-per-second.
     * @param now_ms    Monotonic time in milliseconds (e.g. `millis()`).
     */
    void feed_sample(const float accel_g[3], const float gyro_dps[3], uint32_t now_ms);

    // ---- Results -----------------------------------------------------------

    /// 4-element float array {w, x, y, z}. Identity until `is_complete()`.
    const float* get_quaternion() const;

    /// Variance of accel magnitude over the capture window. Lower = cleaner.
    float get_stillness_variance() const;

    /// Reset to IDLE — discards any in-progress capture and result.
    void reset();

    // ---- Persistence helpers (independent of EEPROM backend) ---------------

    /// Serialize the current result into a 24-byte buffer. Returns false
    /// if `buf_size < 24`, if the capture is not complete, or `buf` is null.
    /// The record's `qc_capture_age_min` field is written as 0; callers
    /// that track age externally should overwrite byte 22 (and recompute
    /// CRC) before persisting if they want a non-zero age.
    bool serialize(uint8_t* buf, uint16_t buf_size) const;

    /// Deserialize from a 24-byte buffer, validating magic, version, and
    /// CRC8. On success, the result quaternion and stillness variance are
    /// restored and the state becomes `COMPLETE`.
    bool deserialize(const uint8_t* buf, uint16_t buf_size);

    // ---- Static math helper (exposed for tests) ----------------------------

    /**
     * Compute the shortest-arc quaternion that rotates `observed_g` onto
     * `target_g`. Both inputs are 3-vectors and need not be normalized
     * (this function normalizes internally).
     *
     * @param observed_g  Source direction (length 3).
     * @param target_g    Destination direction (length 3).
     * @param q_out       Output unit quaternion {w, x, y, z}.
     *
     * Edge cases:
     *   - Already-aligned (dot ≈ +1):  returns identity {1, 0, 0, 0}.
     *   - 180° anti-aligned (dot ≈ -1): returns a 180° rotation about
     *     any axis perpendicular to `observed_g` (deterministic choice).
     *   - Zero-length input: returns identity.
     */
    static void shortest_arc_quaternion(const float observed_g[3],
                                        const float target_g[3],
                                        float q_out[4]);

private:
    enum State : uint8_t {
        STATE_IDLE          = 0,
        STATE_WAITING_STILL = 1,
        STATE_CAPTURING     = 2,
        STATE_COMPLETE      = 3,
        STATE_FAILED        = 4
    };

    State       state_;
    const char* last_error_;

    // Configuration
    float    gyro_max_rad_per_sec_;
    uint16_t capture_duration_ms_;
    uint8_t  still_samples_required_;
    float    accel_lp_alpha_;          ///< EMA coefficient (~0.1 @ 50 Hz / 200 ms)

    // Runtime state
    uint8_t  consecutive_still_count_;
    uint32_t capture_start_ms_;
    float    accel_lp_[3];             ///< low-pass-filtered accel
    uint16_t capture_sample_count_;

    // Variance accumulator (Welford online algorithm on accel magnitude)
    float    var_mean_;
    float    var_m2_;
    uint32_t var_n_;

    // Results
    float    result_q_[4];
    float    stillness_var_;
};

#endif  // MOUNTING_CALIBRATION_H
