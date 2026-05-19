/**
 * Plant Identifier — scalar RLS estimator for the inverted-pendulum gain K_motor,
 * plus closed-form PD-from-K_motor gain mapping (Phase 4.10 / Item 5 of
 * docs/PHASE_4_STRUCTURAL_FIXES.md).
 *
 * The bench-class plant model (small two-wheel inverted pendulum, brushed DC,
 * L298N driver) collapses to one DoF:
 *
 *     α_pitch ≈ K_motor · pwm_total  +  g_eff · sin(pitch)
 *
 * Mass, height, CoM, wheel radius, motor Kv, gear ratio all collapse into the
 * scalar K_motor (deg/s² per PWM unit) and g_eff (deg/s², gravitational
 * restoring acceleration coefficient). g_eff is held to a class-typical
 * constant; K_motor is learned online via scalar RLS with forgetting factor.
 *
 * Algorithm (per docs/findings/research_universal_zero_knowledge_tuning.md §6.2
 * and docs/findings/dynamic_pwm_accel_learning.md §4a):
 *
 *   regressor  φ = pwm_total
 *   target     y = α_measured − g_eff · sin(pitch)
 *   gain       L = P · φ / (λ + φ · P · φ)
 *   update     θ ← θ + L · (y − φ · θ)         where θ ≡ K_motor
 *              P ← (P − L · φ · P) / λ
 *
 * Once θ is converged, gains follow from a critically-damped second-order
 * pole-placement (ω_n = 4/ts, ζ = 0.7):
 *
 *   Kp_target = ω_n² / K_motor
 *   Kd_target = 2 · ζ · ω_n / K_motor
 *   Ki_target = small fraction of Kp (keeps OnlineMountingEstimator alive)
 *
 * Robustness: σ-modification (Ioannou & Sun §8) — when |θ| nears the class
 * bound, a small leak pulls it back toward a prior to prevent bursting (which
 * is the failure mode of RLS run on an unstable plant when excitation dies;
 * Anderson, "Failures of adaptive control theory and their resolution", 2005).
 *
 * Caller responsibility (BalanceApp): rate-limit the target gains before
 * applying them to PIDController::set_tunings(). This module emits *targets*
 * only — never slams the controller.
 *
 * Freeze gates: caller passes `freeze=true` during HELD, large lateral gyro,
 * PID windup, or the bootstrap-quiet window. The estimator holds its state
 * and reports zero drift while frozen (matches OnlineMountingEstimator UX).
 *
 * AVR cost: ~24 B state + Status ≈ 60 B RAM. ~30 µs/tick. Well inside Uno
 * budget per dynamic_pwm_accel_learning.md §8.
 *
 * ISR safety: update() is called from inside the 5 ms hardware-timer ISR.
 * No Serial, no Wire, no allocation. Status is read from loop() — the single-
 * float reads/writes on AVR are not atomic but our reader is best-effort
 * status, so a torn read just produces a momentarily-inconsistent snapshot
 * that the next refresh corrects.
 *
 * See: docs/findings/dynamic_pwm_accel_learning.md   (design)
 *      docs/findings/research_universal_zero_knowledge_tuning.md (algorithm)
 *      docs/findings/bootstrap_protocol_unstable_plant.md (stage 3)
 *      docs/PHASE_4_STRUCTURAL_FIXES.md (item 5)
 */

#ifndef PLANT_IDENTIFIER_H
#define PLANT_IDENTIFIER_H

#include <stdint.h>

struct PlantIdentifierStatus {
    float    k_motor;          // learned plant gain (deg/s² per PWM unit)
    float    covariance;       // RLS P[n] — proxy for inverse confidence
    float    kp_target;        // analytical Kp from current K_motor
    float    kd_target;        // analytical Kd from current K_motor
    float    ki_target;        // small Kp-fraction; keeps OnlineMountingEstimator signal alive
    uint16_t samples;          // ticks since reset (saturates at 65535)
    bool     frozen;           // freeze gate active this tick
};

class PlantIdentifier {
public:
    PlantIdentifier();

    // -------- Configuration (sensible defaults; setters optional) ----------

    // Forgetting factor λ ∈ (0, 1]. Default 0.998 (~25 s memory at 200 Hz);
    // see bootstrap_protocol_unstable_plant.md Stage 3.
    void set_forgetting_factor(float lambda);

    // Class-typical gravitational restoring coefficient g_eff (deg/s²). Default
    // 50 — appropriate for a ~20 cm bench-class bot. Hardcoded rather than
    // learned per dynamic_pwm_accel_learning.md §4a ("(m·g·d/I) is known a
    // priori from chassis geometry — hardcode it, don't learn it").
    void set_g_eff(float g_eff_deg_s2);

    // Target settling time ts (s). Default 0.5 → ω_n = 4/ts = 8 rad/s.
    void set_target_settling_time_sec(float ts);

    // Damping ratio ζ. Default 0.7 (slight overshoot allowed; faster than
    // critical without ringing).
    void set_damping_ratio(float zeta);

    // K_motor projection bounds (σ-modification). Defaults (0.02, 5.0) cover
    // the bench class per research_universal_zero_knowledge_tuning.md §6.2.
    void set_k_motor_bounds(float k_min, float k_max);

    // -------- Lifecycle ---------------------------------------------------

    // Reset state on RUN entry. `kp_initial` / `kd_initial` are used to back-
    // solve a sane prior θ_0 from the seed gains (so the very first target
    // emitted is close to the controller's current behaviour).
    void reset(float kp_initial, float kd_initial);

    // Seed θ from a directly-measured K_motor (e.g. BOOTSTRAP pulse response).
    // Sets covariance to SEED_P (small) so subsequent RLS updates trust the
    // measurement strongly but still adapt to battery / wear / surface drift.
    // The new θ is clamped to (k_min, k_max) before being applied. After this
    // call, recompute_targets_() emits Kp/Kd/Ki derived from the measured K.
    void seed_k_motor(float k_measured);

    // -------- Per-tick update --------------------------------------------

    /**
     * Feed one sample.
     *
     * @param pwm_total          Combined commanded PWM (pwm_left + pwm_right).
     *                           The balance bot drives both wheels identically,
     *                           so this is 2 × the per-wheel command. Either
     *                           sign convention works — only |φ| matters for
     *                           the regression magnitude.
     * @param gyro_rate_dps      Current pitch-axis gyro rate (deg/s).
     * @param gyro_rate_prev_dps Previous pitch-axis gyro rate (deg/s). The
     *                           caller stashes it across ticks; we compute
     *                           α = (rate − prev) / dt here so the gyro
     *                           differentiation is co-located with the
     *                           regressor that feeds it.
     * @param pitch_deg          Current pitch (already de-biased by caller).
     * @param dt_sec             Tick period in seconds (5 ms = 0.005 typical).
     * @param freeze             true to hold state this tick (HELD, large
     *                           lateral gyro, windup, bootstrap window).
     */
    void update(float pwm_total,
                float gyro_rate_dps,
                float gyro_rate_prev_dps,
                float pitch_deg,
                float dt_sec,
                bool  freeze);

    // -------- Inspection -------------------------------------------------

    float get_k_motor() const;
    float get_covariance() const;
    float get_kp_target() const;
    float get_kd_target() const;
    float get_ki_target() const;
    const PlantIdentifierStatus& get_status() const;

private:
    // RLS state
    float theta_;      // K_motor estimate
    float P_;          // covariance scalar

    // Parameters
    float lambda_;
    float g_eff_;
    float ts_target_;
    float zeta_;
    float k_min_;
    float k_max_;

    // Status snapshot (refreshed each update())
    PlantIdentifierStatus status_;

    // Refresh kp_/kd_/ki_ targets in status_ from theta_.
    void recompute_targets_();
};

#endif  // PLANT_IDENTIFIER_H
