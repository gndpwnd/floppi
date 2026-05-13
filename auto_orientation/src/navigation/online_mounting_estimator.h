/**
 * Online Adaptive Mounting-Offset Estimator
 *
 * Phase 4.4 of the auto_orientation framework. Complements the one-shot
 * mounting capture (Phase 4.3) with a continuous, slowly-adaptive estimate
 * of the *dynamic-equivalent* mounting offset. Handles drift from
 * cable-tether torque, battery sag, payload changes, and aging surfaces.
 *
 * AVR Mega path (this file, decision D7 in MASTER_DESIGN.md):
 *   Slow LPF of the inner-loop PID integral term. Lightweight, ~20 bytes
 *   state, easy to reason about. Time constant on the order of minutes so
 *   that real CoM/torque drift is tracked but transients are rejected.
 *
 * Teensy/ESP32 path (TODO, decision D7):
 *   Extend the 2-state balance Kalman (pitch + gyro-bias) to a 3-state
 *   form by adding mounting_offset as a slow random-walk state with very
 *   small process noise. The covariance entry doubles as the confidence
 *   value. Not implemented in this file — wait until the 2-state Kalman
 *   lands in src/navigation/balance_kalman.{h,cpp} (Phase 4.7), then
 *   extend it directly.
 *
 * Safety (decision D8):
 *   - Hard refuse |estimate - reference| > 5°
 *   - Rate limit |d/dt estimate| to 0.5°/s
 *   - Freeze adaptation during tipover recovery, I-term windup, or
 *     high gyro rate (transient bumps).
 *
 * Persistence (research finding §8):
 *   EEPROM auto-save is *not* implemented in this estimator; the host
 *   application schedules it via the last_save_ms accessor on the status
 *   struct to keep storage decisions out of the estimator's responsibility.
 *
 * See: docs/findings/MASTER_DESIGN.md §4.4
 *      docs/findings/online_adaptive_balance_tracking.md
 */

#ifndef ONLINE_MOUNTING_ESTIMATOR_H
#define ONLINE_MOUNTING_ESTIMATOR_H

#include <stdint.h>

/**
 * Reason the estimator stopped adapting. Reported via MountingCalibrationStatus
 * for dashboard/telemetry. FREEZE_NONE means adaptation is running.
 */
enum AdaptationFreezeReason : uint8_t {
    FREEZE_NONE       = 0,
    FREEZE_TIPOVER    = 1,
    FREEZE_WINDUP     = 2,
    FREEZE_USER       = 3,
    FREEZE_HIGH_GYRO  = 4
};

/**
 * Status snapshot for the API/dashboard. Sized small for the AVR transport.
 *
 *   estimate_deg        — current adapted mounting offset (deg)
 *   drift_rate_dps      — instantaneous adaptation rate (deg/s); zero when frozen
 *   confidence_0_to_1   — heuristic: 1.0 at reference, decreases linearly to
 *                         0.0 at the hard bound
 *   last_save_ms        — caller-managed; estimator only stores what the host
 *                         hands back via mark_saved()
 *   adaptation_frozen   — true if any freeze gate is active
 *   freeze_reason       — most recent AdaptationFreezeReason
 */
struct MountingCalibrationStatus {
    float    estimate_deg;
    float    drift_rate_dps;
    float    confidence_0_to_1;
    uint32_t last_save_ms;
    bool     adaptation_frozen;
    uint8_t  freeze_reason;
};

class OnlineMountingEstimator {
public:
    OnlineMountingEstimator();

    /**
     * Initialize with the one-shot reference. Call after a fresh mounting
     * capture (Phase 4.3) or after restoring a saved value from EEPROM.
     *
     * Resets the estimate to the reference, clears the freeze reason, and
     * arms the dt-based update path with now_ms.
     */
    void initialize(float reference_deg, uint32_t now_ms);

    // ---------------------------------------------------------------------
    // Configuration (all optional — sane defaults in the constructor)
    // ---------------------------------------------------------------------

    /**
     * LPF time constant in seconds. Larger = slower adaptation. Default 300 s
     * (5 minutes), matching the research finding's recommendation that 1°
     * accumulation should take ~5 minutes of stable runtime.
     *
     * The per-step update is:  estimate += (1/tc) * dt_s * (target - estimate)
     * so tc has units of seconds and dt_s scales appropriately at any sample
     * rate.
     */
    void set_lpf_time_constant_sec(float tc);

    /**
     * Hard bound on |estimate - reference| in degrees. Default 5.0.
     * Beyond this the estimate is clamped and adaptation effectively stalls
     * at the rail — the host should warn the user (likely physical fault).
     */
    void set_max_deviation_deg(float deg);

    /**
     * Rate limit on |d/dt estimate| in deg/s. Default 0.5.
     * Bounds how fast a sensor glitch or compute fault can drag the offset.
     */
    void set_max_drift_rate_dps(float dps);

    /**
     * Mapping from PID integral term value to mounting-angle equivalent.
     * Default 0.01 (i.e., I-term of 100 corresponds to ~1° of offset). The
     * correct value is plant-specific; the balancing-robot application sets
     * this from a measured kI / kP ratio.
     */
    void set_gain_to_angle(float gain);

    // ---------------------------------------------------------------------
    // Live update
    // ---------------------------------------------------------------------

    /**
     * Feed one sample. Called at the PID update rate (~200 Hz for the
     * balancing-robot reference).
     *
     * @param i_term         Current PID integral term value
     * @param pitch_deg      Current pitch angle (deg). Reserved for future
     *                       cross-checks; not used by the LPF math directly.
     * @param tipover_active true while the safety module is recovering from
     *                       a fall — adaptation is frozen.
     * @param windup_active  true while the PID controller reports I-term
     *                       saturation — adaptation is frozen.
     * @param gyro_pitch_dps Pitch-axis gyro rate (deg/s). Adaptation freezes
     *                       above 60 °/s to reject transient bumps.
     * @param now_ms         Monotonic millisecond clock (e.g., millis()).
     *
     * @return current estimate (deg). When frozen, returns the held estimate
     *         unchanged.
     */
    float update(float i_term, float pitch_deg,
                 bool tipover_active, bool windup_active,
                 float gyro_pitch_dps, uint32_t now_ms);

    /**
     * Force-freeze adaptation (e.g., user-disabled via dashboard).
     */
    void set_user_freeze(bool freeze);

    // ---------------------------------------------------------------------
    // Inspection
    // ---------------------------------------------------------------------

    float get_estimate_deg() const;
    float get_drift_rate_dps() const;
    float get_reference_deg() const;
    MountingCalibrationStatus get_status() const;

    /**
     * Reset to a new reference (e.g., after a fresh one-shot recapture).
     */
    void reset_to_reference(float new_ref_deg, uint32_t now_ms);

    /**
     * Inform the estimator that the host just persisted the current estimate
     * to EEPROM. The estimator only stores the timestamp for the status
     * struct; it does not enforce a save cadence itself.
     */
    void mark_saved(uint32_t now_ms);

private:
    // Configuration
    float reference_deg_;
    float max_deviation_deg_;
    float max_drift_rate_dps_;
    float gain_to_angle_;
    float lpf_time_constant_sec_;   // tc; alpha = 1/tc is computed on-the-fly

    // Live state
    float estimate_deg_;
    float drift_rate_dps_;

    // Freeze tracking
    bool    user_frozen_;
    uint8_t last_freeze_reason_;

    // Timing / persistence
    uint32_t last_update_ms_;
    uint32_t last_save_ms_;

    bool initialized_;

    // Helpers
    static float clamp_(float v, float lo, float hi);
};

#endif // ONLINE_MOUNTING_ESTIMATOR_H
