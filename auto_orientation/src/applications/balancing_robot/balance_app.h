/**
 * BalanceApp — top-level state machine for the self-balancing robot
 * reference application (Phase 4.7a of the auto_orientation framework).
 *
 * Orchestrates four phases of operation:
 *
 *     IDLE -> CAPTURE_MOUNTING -> AUTO_TUNE -> RUN <----+
 *                                              ^  |    |
 *                                              |  v    |
 *                                              HELD ---+
 *                                              |
 *                                              v
 *                                            FALLEN (sticky)
 *
 * with edges back to IDLE on user abort, capture failure, tune failure,
 * watchdog stall, or explicit long-press. FALLEN is sticky — only an
 * operator-initiated command (short-press / `c` / `R`) restarts.
 *
 * Composition (constructor-injected — the app owns *behaviour*, not the
 * hardware):
 *   - OrientationSensor& — pitch source. Current API only exposes
 *     OrientationData.pitch_deg; raw accel/gyro access is a Phase 4.6.5
 *     deliverable. For mounting capture in this skeleton we therefore use
 *     `pitch_deg` stability as the stillness gate (see CAPTURE_MOUNTING).
 *   - DualMotorDriver& — output. Speed in [-255, +255].
 *   - PIDController& — the actual balance loop controller.
 *   - AutoPIDTuner& — pre-wired with a strategy (relay-feedback by default).
 *   - MountingCalibration& — one-shot capture utility (Phase 4.3).
 *   - OnlineMountingEstimator& — slow drift tracker (Phase 4.4).
 *   - BalanceSafety& — tilt/abort/watchdog (safety.h).
 *
 * Tick contract: caller invokes step(now_ms) at the configured PID sample
 * rate (default 5 ms = 200 Hz, matching the legacy .ino). The app reads the
 * IMU, runs the current state's handler, and (in RUN) returns the motor
 * output that was just applied. In any other state, step() returns 0.
 *
 * User input: external code (button handler, WiFi endpoint, ...) calls
 * on_short_press() / on_long_press(). These don't drive state directly —
 * they set flags / request transitions that the next step() picks up. This
 * keeps the state machine single-threaded by construction.
 *
 * Compile gate: the application's main entry point should be guarded by
 * `#ifdef USE_BALANCING_ROBOT` (in src/main.cpp). The class itself is
 * unconditionally compileable — it depends only on already-compiled
 * framework modules (PID, tuner, mounting, sensor base, motor base). This
 * is intentional: the native test build links balance_app.{h,cpp} directly
 * without USE_BALANCING_ROBOT defined.
 *
 * See: docs/findings/MASTER_DESIGN.md §4.7
 *      docs/findings/balance_point_and_mounting_research.md
 *      docs/findings/online_adaptive_balance_tracking.md
 *      docs/findings/disturbance_compensation_research.md
 *      docs/findings/tetherless_operation_strategy.md
 *      docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino
 */

#ifndef BALANCE_APP_H
#define BALANCE_APP_H

#include <stdint.h>

#include "../../sensors/sensor_base.h"
#include "../../control/pid_controller.h"
#include "../../control/auto_pid_tuner.h"
#include "../../control/plant_identifier.h"
#include "../../navigation/mounting_calibration.h"
#include "../../navigation/online_mounting_estimator.h"
#include "../../actuators/motor_driver.h"
#include "safety.h"

enum class BalanceAppState : uint8_t {
    IDLE              = 0,
    CAPTURE_MOUNTING  = 1,
    AUTO_TUNE         = 2,
    RUN               = 3,
    HELD              = 4,   // bot picked up — motors off, ready to resume
    FALLEN            = 5    // sticky tipover — operator must restart
};

/**
 * Application-level config. Owned by the caller; passed once into begin().
 * Sensible defaults are provided by BalanceApp::default_config().
 */
struct BalanceAppConfig {
    // Initial PID gains (overwritten after auto-tune)
    float    initial_kp;
    float    initial_ki;
    float    initial_kd;
    uint16_t pid_sample_ms;             // default 5 ms (200 Hz)

    // Safety
    float    tilt_limit_deg;            // default 10° — enter FALLEN beyond
    float    tilt_recovery_deg;         // unused (FALLEN is sticky); kept for API stability
    uint8_t  recovery_consecutive_samples;  // default 30 (~150 ms at 200 Hz)

    // Auto-tune
    float    tune_amplitude;            // relay amp in PWM units, default 150
    float    tune_hysteresis;           // default 0.5°
    float    tune_max_duration_sec;     // default 30

    // Output clamps (PWM range)
    float    output_min;                // default -255
    float    output_max;                // default +255

    // CAPTURE_MOUNTING timing. Skeleton uses pitch stability rather than raw
    // accel (see header notes). capture_duration_ms is how long we hold to
    // average; capture_pitch_var_deg is the max allowed pitch jitter inside
    // the window to still call the capture "good".
    uint16_t capture_duration_ms;       // default 2000
    float    capture_pitch_var_deg;     // default 0.5

    // Online adaptive
    bool     enable_online_adaptation;  // default true after tune
};

class BalanceApp {
public:
    BalanceApp(OrientationSensor& imu,
               DualMotorDriver& motors,
               PIDController& pid,
               AutoPIDTuner& tuner,
               MountingCalibration& mounting,
               OnlineMountingEstimator& online_est,
               BalanceSafety& safety,
               PlantIdentifier& plant_id);

    // Convenience: built-in defaults matching the legacy .ino (Kp=65 Ki=12
    // Kd=38, ±255 PWM, 5 ms sample, 35° tipover / 15° recovery).
    static BalanceAppConfig default_config();

    /**
     * Initialize the application:
     *  - Stores config; sets state to IDLE.
     *  - Pushes initial PID gains and output limits.
     *  - Pushes safety thresholds.
     *  - Initializes online_est_ with reference 0 (caller should override
     *    by feeding a deserialized AutoOrientRecord into mounting_/online_
     *    *before* calling begin() if a saved offset exists).
     *  - Motors stopped.
     *
     * Does NOT attempt to read from EEPROM directly — persistence is the
     * host application's responsibility (see main.cpp / lifecycle/).
     */
    void begin(const BalanceAppConfig& cfg, uint32_t now_ms);

    /**
     * Run one tick — back-compat wrapper that calls read_sensors() then tick().
     * Native tests and any caller that doesn't use the hardware-timer split
     * stay on this entry point unchanged.
     *
     * @return motor PWM that was just applied, or 0 if motors are stopped.
     */
    int16_t step(uint32_t now_ms);

    /**
     * Loop-side sensor refresh. ITEM 3 (Phase A): split out from step() so the
     * I²C IMU read (NOT ISR-safe — Wire uses interrupts internally) stays in
     * loop() while the PID tick runs in a hardware-timer ISR. The ISR consumes
     * whatever values this method most recently stashed.
     *
     * Call as fast as the loop can manage (~1 kHz typical) to keep IMU
     * staleness < 1 PID tick. Reads pitch / raw gyro / raw accel / motion
     * filters; does NOT touch motors, PID, or state transitions.
     *
     * Source: research_osoyoo_reference_implementation.md §3 #2 +
     *         latency_budget_2026-05-12.md.
     */
    void read_sensors(uint32_t now_ms);

    /**
     * ISR-side control tick. ITEM 3 (Phase A): runs the per-state handler
     * using the freshest snapshot from read_sensors(). MUST be called at
     * exactly the PID sample period (5 ms via MsTimer2 on AVR).
     *
     * ISR-safety contract:
     *   - No Serial.print: state-transition logging is deferred via the
     *     drain_state_log() flag (see below).
     *   - No I²C: imu_.read() lives in read_sensors(), not here.
     *   - motors_.set_speed() / analogWrite / digitalWrite are AVR-atomic.
     *   - Volatile-read of raw_gyro_dps_ is naturally safe because main-loop
     *     writes happen with this ISR paused.
     */
    void tick(uint32_t now_ms);

    /**
     * Drain any deferred state-transition log set by tick() (in ISR context).
     * Call from loop(). Emits one line to `out` per transition that fired
     * since the previous drain.
     *
     * This is the "ISR sets flag, loop prints it" pattern — Serial.print is
     * not ISR-safe because its TX buffer can deadlock with main-loop writes.
     */
    template <class TPrint>
    void drain_state_log(TPrint& out) {
        // The volatile flag is the new-state value; 0xFF means "nothing pending".
        // No critical section needed: this is the only writer of the flag from
        // loop context, and a torn read of a single byte is impossible on AVR
        // (lb/sb are atomic for uint8_t).
        uint8_t pending = pending_state_log_;
        if (pending == 0xFF) return;
        pending_state_log_ = 0xFF;
        out.print(F("[state] -> "));
        switch (static_cast<BalanceAppState>(pending)) {
            case BalanceAppState::IDLE:             out.println(F("IDLE")); break;
            case BalanceAppState::CAPTURE_MOUNTING: out.println(F("CAP")); break;
            case BalanceAppState::AUTO_TUNE:        out.println(F("TUNE")); break;
            case BalanceAppState::RUN:              out.println(F("RUN")); break;
            case BalanceAppState::HELD:             out.println(F("HELD")); break;
            case BalanceAppState::FALLEN:           out.println(F("FALLEN")); break;
        }
    }

    // ----- User actions (call from button handler / WiFi endpoint) ---------

    /**
     * Short press semantics by state:
     *   IDLE              -> kick off CAPTURE_MOUNTING
     *   CAPTURE_MOUNTING  -> no-op (let it finish or time-out)
     *   AUTO_TUNE         -> no-op
     *   RUN               -> no-op (we don't re-tune mid-flight without
     *                       explicit operator intent — see on_long_press)
     *   HELD              -> force-resume to RUN (skip dwell)
     *   FALLEN            -> restart: re-enter RUN with current gains
     */
    void on_short_press(uint32_t now_ms);

    /**
     * Long press semantics:
     *   - Anywhere: request abort via BalanceSafety (the state machine
     *     converts that into a transition to IDLE on the next step()).
     *   - Additionally, if we're already in IDLE: skip mounting capture
     *     and jump straight into AUTO_TUNE using whatever offset is
     *     currently loaded (caller is expected to have deserialized a
     *     saved AutoOrientRecord before this point).
     */
    void on_long_press(uint32_t now_ms);

    /**
     * Skip auto-tune and enter RUN immediately with whatever gains the PID
     * currently holds. Use this when you've manually set known-good gains
     * (e.g. via PIDController::set_tunings()) and just want to balance.
     * No-op if not currently in IDLE.
     */
    void enter_run_with_current_gains(uint32_t now_ms);

    // ----- Inspection -----------------------------------------------------

    BalanceAppState get_state() const { return state_; }
    float           get_pitch_deg() const { return pitch_deg_; }
    float           get_mount_offset_deg() const;
    int16_t         get_last_output() const { return last_output_; }
    const MountingCalibrationStatus& get_mount_status() const { return mount_status_; }
    const TuningResult& get_tune_result() const { return tune_result_; }
    const PlantIdentifierStatus& get_plant_status() const { return plant_id_.get_status(); }
    bool            is_adaptive_active() const { return adaptive_active_; }
    const char*     state_name() const;

    // Direct accessor used by callers that want to drive an external user
    // interface (LEDs, dashboard).
    BalanceSafety&  safety() { return safety_; }

private:
    OrientationSensor&        imu_;
    DualMotorDriver&          motors_;
    PIDController&            pid_;
    AutoPIDTuner&             tuner_;
    MountingCalibration&      mounting_;
    OnlineMountingEstimator&  online_est_;
    BalanceSafety&            safety_;
    PlantIdentifier&          plant_id_;

    BalanceAppConfig          cfg_;
    BalanceAppState           state_;
    uint32_t                  state_entered_ms_;
    int16_t                   last_output_;
    float                     pitch_deg_;
    uint8_t                   recovery_count_;
    uint32_t                  sat_run_start_ms_;   // saturation-timeout origin

    // Motion signals for HELD/FALLEN distinction (Tier 2.2). Lateral-axis
    // gyro is the trick from balance_held_fallen_state_machine.md §3 — it
    // suppresses false HELD entries during aggressive recoveries because
    // intrinsic balance motion is single-axis (pitch only). Handling
    // produces motion on roll + yaw that self-balancing physically cannot.
    float    g_lateral_dps_lpf_;   // sqrt(gx² + gz²), LP-filtered
    float    a_dev_lpf_;           // |accel| − 9.81, LP-filtered
    float    a_align_;             // accel_z / |accel|; 1.0 ≈ upright on surface
    bool     motion_filters_init_;

    // Raw gyro pitch-axis rate, populated by read_imu_ each tick. Used as the
    // PID D-term source (Item 1: research_osoyoo_reference_implementation.md
    // §3 / §9) and as the OnlineMountingEstimator transient-freeze input
    // (Item 2: bootstrap_protocol_unstable_plant.md §6). BNO055 mounting
    // convention: pitch is body Y-axis, so index [1] is the relevant rate.
    // TODO: VERIFY AXIS ON BENCH — currently assuming Y (consistent with
    // pitch_deg_ source via quaternion_to_euler_degrees, but body-axis
    // mounting can re-map). See balance_app.cpp read_imu_ §3.
    float    raw_gyro_dps_[3];

    // HELD bookkeeping
    uint16_t hold_enter_count_;    // RUN→HELD dwell counter (samples)
    uint16_t hold_exit_count_;     // HELD→RUN dwell counter (samples)
    uint16_t hold_fall_count_;     // HELD→FALLEN dwell counter (samples)

    MountingCalibrationStatus mount_status_;
    TuningResult              tune_result_;

    // CAPTURE_MOUNTING uses a running mean/variance of pitch_deg (Welford)
    // to gate "stillness". See header notes — this is a temporary stand-in
    // for raw-accel access (Phase 4.6.5 will replace it).
    uint16_t cap_sample_count_;
    float    cap_pitch_mean_;
    float    cap_pitch_m2_;
    float    cap_pitch_first_;

    // PID saturation tracker for windup_active signal (Item 2). Counts
    // consecutive ticks where |out| ≥ 0.95 * output_max. >= 2 ticks holds
    // the windup signal high so we don't trip on single-sample noise.
    uint8_t  sat_consecutive_ticks_;

    // Phase 4.10 — adaptive PID gain plumbing (Item 5).
    //
    // The PlantIdentifier emits *target* gains every tick; we ramp the live
    // PID gains toward those targets at a bounded rate (5%/s of current value
    // per second). A transient bad K_motor estimate can't slam the loop.
    //
    // Bootstrap window: when entering RUN, we freeze the identifier for the
    // first BOOTSTRAP_FREEZE_MS milliseconds so the OnlineMountingEstimator
    // and PID itself can settle before plant-ID begins. Per
    // bootstrap_protocol_unstable_plant.md Stage 3 (RLS starts after the
    // mounting estimator has settled the I-term signal).
    //
    // gyro_pitch_prev_dps_ is stashed across ticks so the identifier can
    // compute α = dω/dt by single subtraction. Updated at the END of step_run_
    // so the next tick sees the previous-tick value.
    float    gyro_pitch_prev_dps_;
    float    applied_kp_;          // current live Kp (rate-limited)
    float    applied_kd_;          // current live Kd
    float    applied_ki_;          // current live Ki
    uint32_t run_entered_ms_;      // for bootstrap-window gating
    bool     adaptive_active_;     // true once bootstrap window has elapsed

    // Deferred state-transition log (Item 3). enter_state_() sets this to
    // the new BalanceAppState value (cast to uint8_t); drain_state_log()
    // prints and clears (0xFF = empty). Volatile because writer/reader cross
    // the ISR / loop boundary; single-byte loads/stores are atomic on AVR so
    // no critical section is needed.
    volatile uint8_t pending_state_log_;

    // Internal state transitions
    void enter_state_(BalanceAppState s, uint32_t now_ms);

    // Per-state handlers
    void step_idle_(uint32_t now_ms);
    void step_capture_(uint32_t now_ms);
    void step_tune_(uint32_t now_ms);
    void step_run_(uint32_t now_ms);
    void step_held_(uint32_t now_ms);
    void step_fallen_(uint32_t now_ms);

    // Helpers
    void  read_imu_(uint32_t now_ms);
    float corrected_pitch_() const;
    void  reset_capture_accumulators_();
    // Phase 4.10 — adaptive PID gain plumbing. Pulled out of step_run_ and
    // marked noinline so the 5 ms PID ISR keeps a manageable flash footprint
    // (per ~500 B saving on Uno when the RLS update isn't inlined). Single-
    // step rate-limit ramp for adaptive PID gains. 5%/s of current value per
    // second, never less than 0.001 absolute (so a gain can climb off zero).
    static void ramp_gain_(float& live, float target, float dt_sec);
    void run_plant_id_(float gyro_pitch_dps, bool windup_active, uint32_t now_ms);
};

#endif  // BALANCE_APP_H
