/**
 * BalanceApp — implementation. See balance_app.h for the API contract and
 * the state-machine diagram.
 *
 * Implementation notes (skeleton-level):
 *
 *   1. Raw accel/gyro access. The OrientationSensor abstraction currently
 *      exposes only the fused OrientationData (quaternion + Euler), not the
 *      raw 3-axis accel and gyro vectors that MountingCalibration::feed_sample
 *      expects. Phase 4.6.5 will add a RawIMUAccess interface; until then,
 *      this app gates CAPTURE_MOUNTING on *pitch_deg stability* (a Welford
 *      running variance of pitch over the capture window). When the window
 *      closes with low enough variance, we synthesize the mounting offset as
 *      a small rotation about the +Y body axis equal to the mean pitch.
 *      That synthesis is mathematically equivalent to the "pitch-only" branch
 *      of the shortest_arc_quaternion result when accel is in the body-X-Z
 *      plane, which is the geometry of a typical balancing-robot mount.
 *
 *   2. EEPROM persistence. begin() does NOT load from EEPROM directly; that
 *      lives in the host application (main.cpp / lifecycle/) so we can keep
 *      this module hardware-agnostic and the native tests don't have to mock
 *      a persistence backend.
 *
 *   3. Watchdog. step_run_() feeds it. FALLEN also feeds it (we want the
 *      loop alive so we can detect recovery). IDLE and CAPTURE feed it too;
 *      AUTO_TUNE feeds it. The watchdog only catches a stuck step() call by
 *      the host, not anything internal to step().
 *
 *   4. Adaptive offset. RUN updates online_est_ with the current PID
 *      integral term. corrected_pitch_() then subtracts the estimate from
 *      the raw pitch before feeding it to the PID. This implements the
 *      "subtract the offset from the measurement" architecture (rather than
 *      "add to the setpoint"); both are mathematically equivalent for a
 *      zero-setpoint balance loop, and subtracting from measurement keeps
 *      the user-facing setpoint constant for downstream consumers.
 */

#include "balance_app.h"

#include <math.h>     // sqrtf (only used in capture variance — not a hot path)

// CAL_PRINTLN/CAL_PRINTF macros from config/mode.h are nice but require
// Serial.* which doesn't exist on the native test host. We provide a tiny
// local logger that compiles to nothing in non-Arduino builds.
#if defined(ARDUINO)
  #include <Arduino.h>
  #define BAL_LOGF(fmt, ...) Serial.print(F(fmt))   // basic, args dropped
#else
  #define BAL_LOGF(fmt, ...) ((void)0)
#endif

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

// Iterated gains 2026-05-12 evening. Kp=80 produced visible shake; Ki=8
// caused forward-drive windup. Settling on Kp=50 / Ki=2 / Kd=20 as a
// balance point — enough authority at small tilts (250 PWM/°), enough
// derivative damping to catch motion, small integral so the
// OnlineMountingEstimator can absorb the bias without windup fighting it.
// Phase 4.10 will learn all three per-bot.
static const float    kDefaultInitialKp           = 50.0f;
static const float    kDefaultInitialKi           = 2.0f;
static const float    kDefaultInitialKd           = 20.0f;
static const uint16_t kDefaultPidSampleMs         = 5;

static const float    kDefaultTiltLimitDeg        = 10.0f;   // FALLEN @ ±10° — fail fast
static const float    kDefaultTiltRecoveryDeg     = 4.0f;    // recover when |pitch|<4°
static const uint8_t  kDefaultRecoveryConsecutive = 30;

static const float    kDefaultTuneAmplitude       = 50.0f;   // gentle; was 80
static const float    kDefaultTuneHysteresis      = 0.5f;
static const float    kDefaultTuneMaxDurationSec  = 30.0f;

static const float    kDefaultOutputMin           = -255.0f; // full motor range
static const float    kDefaultOutputMax           = 255.0f;  // (slew limiter handles smoothness)

static const uint16_t kDefaultCaptureDurationMs   = 2000;
static const float    kDefaultCapturePitchVarDeg  = 0.5f;

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

BalanceAppConfig BalanceApp::default_config() {
    BalanceAppConfig c;
    c.initial_kp                    = kDefaultInitialKp;
    c.initial_ki                    = kDefaultInitialKi;
    c.initial_kd                    = kDefaultInitialKd;
    c.pid_sample_ms                 = kDefaultPidSampleMs;
    c.tilt_limit_deg                = kDefaultTiltLimitDeg;
    c.tilt_recovery_deg             = kDefaultTiltRecoveryDeg;
    c.recovery_consecutive_samples  = kDefaultRecoveryConsecutive;
    c.tune_amplitude                = kDefaultTuneAmplitude;
    c.tune_hysteresis               = kDefaultTuneHysteresis;
    c.tune_max_duration_sec         = kDefaultTuneMaxDurationSec;
    c.output_min                    = kDefaultOutputMin;
    c.output_max                    = kDefaultOutputMax;
    c.capture_duration_ms           = kDefaultCaptureDurationMs;
    c.capture_pitch_var_deg         = kDefaultCapturePitchVarDeg;
    c.enable_online_adaptation      = true;
    return c;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

BalanceApp::BalanceApp(OrientationSensor& imu,
                       DualMotorDriver& motors,
                       PIDController& pid,
                       AutoPIDTuner& tuner,
                       MountingCalibration& mounting,
                       OnlineMountingEstimator& online_est,
                       BalanceSafety& safety,
                       PlantIdentifier& plant_id)
    : imu_(imu),
      motors_(motors),
      pid_(pid),
      tuner_(tuner),
      mounting_(mounting),
      online_est_(online_est),
      safety_(safety),
      plant_id_(plant_id),
      cfg_(default_config()),
      state_(BalanceAppState::IDLE),
      state_entered_ms_(0),
      last_output_(0),
      pitch_deg_(0.0f),
      recovery_count_(0),
      sat_run_start_ms_(0),
      g_lateral_dps_lpf_(0.0f),
      a_dev_lpf_(0.0f),
      a_align_(1.0f),
      motion_filters_init_(false),
      hold_enter_count_(0),
      hold_exit_count_(0),
      hold_fall_count_(0),
      cap_sample_count_(0),
      cap_pitch_mean_(0.0f),
      cap_pitch_m2_(0.0f),
      cap_pitch_first_(0.0f),
      sat_consecutive_ticks_(0),
      gyro_pitch_prev_dps_(0.0f),
      applied_kp_(0.0f),
      applied_kd_(0.0f),
      applied_ki_(0.0f),
      run_entered_ms_(0),
      adaptive_active_(false),
      pending_state_log_(0xFF) {
    raw_gyro_dps_[0] = 0.0f;
    raw_gyro_dps_[1] = 0.0f;
    raw_gyro_dps_[2] = 0.0f;
    // Initialize result/status structs to safe defaults so accessors are
    // valid before any state runs.
    mount_status_.estimate_deg       = 0.0f;
    mount_status_.drift_rate_dps     = 0.0f;
    mount_status_.confidence_0_to_1  = 0.0f;
    mount_status_.last_save_ms       = 0;
    mount_status_.adaptation_frozen  = false;
    mount_status_.freeze_reason      = 0;

    tune_result_.kp                        = 0.0f;
    tune_result_.ki                        = 0.0f;
    tune_result_.kd                        = 0.0f;
    tune_result_.ultimate_gain             = 0.0f;
    tune_result_.ultimate_period_sec       = 0.0f;
    tune_result_.phase_margin_estimate_deg = 0.0f;
    tune_result_.converged                 = false;
    tune_result_.failure_reason            = "not_started";
}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------

void BalanceApp::begin(const BalanceAppConfig& cfg, uint32_t now_ms) {
    cfg_ = cfg;

    // PID setup
    pid_.set_tunings(cfg_.initial_kp, cfg_.initial_ki, cfg_.initial_kd);
    pid_.set_output_limits(cfg_.output_min, cfg_.output_max);
    pid_.set_sample_time(cfg_.pid_sample_ms);
    pid_.set_setpoint(0.0f);
    pid_.reset();

    // Safety thresholds
    safety_.set_tilt_limit(cfg_.tilt_limit_deg);
    safety_.set_tilt_recovery(cfg_.tilt_recovery_deg);
    safety_.clear_abort();
    safety_.feed_watchdog(now_ms);

    // Online estimator starts at 0 by default. If the host loaded a saved
    // mounting offset via mounting_.deserialize() before begin(), it should
    // also have called online_est_.initialize() with that offset. We do not
    // touch online_est_ here so we don't clobber that setup. For brand-new
    // boards with no saved offset, the estimator's constructor defaults
    // (reference 0, estimate 0) are correct.

    // Motors stopped
    motors_.stop();
    last_output_ = 0;

    // Enter IDLE
    enter_state_(BalanceAppState::IDLE, now_ms);

    mount_status_ = online_est_.get_status();
}

// ---------------------------------------------------------------------------
// step() / read_sensors() / tick() — Item 3 ISR-friendly split
// ---------------------------------------------------------------------------
//
// step() is the back-compat unified entry point: calls read_sensors() then
// tick(). All native tests and any caller that does not run tick() from an
// ISR continue to work unchanged.
//
// read_sensors() is the loop-side I/O — does the I²C IMU read (Wire is NOT
// ISR-safe; nesting the I²C ISR inside a control-tick ISR causes hangs).
//
// tick() is the ISR-safe control core. It assumes read_sensors() has already
// stashed fresh pitch / raw gyro / motion-filter values into member state.
// In Option A (research_osoyoo_reference_implementation.md §3 #2) the ISR
// runs at 200 Hz with at most one read_sensors() period of IMU staleness,
// comparable to the BNO055 NDOF group delay itself.

void BalanceApp::read_sensors(uint32_t now_ms) {
    read_imu_(now_ms);
}

void BalanceApp::tick(uint32_t now_ms) {
    safety_.feed_watchdog(now_ms);

    switch (state_) {
        case BalanceAppState::IDLE:             step_idle_(now_ms);      break;
        case BalanceAppState::CAPTURE_MOUNTING: step_capture_(now_ms);   break;
        case BalanceAppState::AUTO_TUNE:        step_tune_(now_ms);      break;
        case BalanceAppState::RUN:              step_run_(now_ms);       break;
        case BalanceAppState::HELD:             step_held_(now_ms);      break;
        case BalanceAppState::FALLEN:           step_fallen_(now_ms);    break;
    }
}

int16_t BalanceApp::step(uint32_t now_ms) {
    read_sensors(now_ms);
    tick(now_ms);
    return last_output_;
}

// ---------------------------------------------------------------------------
// Per-state handlers
// ---------------------------------------------------------------------------

void BalanceApp::step_idle_(uint32_t /*now_ms*/) {
    // Motors stopped. PID quiet. We do *not* tick online_est_ here — IDLE is
    // explicitly "no balance loop running", so there's no I-term signal worth
    // feeding into a slow LPF.
    motors_.stop();
    last_output_ = 0;
}

void BalanceApp::step_capture_(uint32_t now_ms) {
    // Skeleton implementation: gate on pitch_deg stillness (Welford running
    // variance). When the variance over `capture_duration_ms` is below the
    // configured threshold, we accept the capture using the mean pitch as
    // the offset. See implementation notes at top of file.
    motors_.stop();
    last_output_ = 0;

    // Update running stats (Welford).
    cap_sample_count_ += 1;
    if (cap_sample_count_ == 1) {
        cap_pitch_first_ = pitch_deg_;
        cap_pitch_mean_  = pitch_deg_;
        cap_pitch_m2_    = 0.0f;
    } else {
        const float delta = pitch_deg_ - cap_pitch_mean_;
        cap_pitch_mean_  += delta / (float)cap_sample_count_;
        const float delta2 = pitch_deg_ - cap_pitch_mean_;
        cap_pitch_m2_   += delta * delta2;
    }

    // User abort cancels the capture.
    if (safety_.abort_requested()) {
        safety_.clear_abort();
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // Window check.
    const uint32_t elapsed = now_ms - state_entered_ms_;
    if (elapsed >= cfg_.capture_duration_ms && cap_sample_count_ >= 2) {
        const float variance =
            cap_pitch_m2_ / (float)(cap_sample_count_ - 1);
        const float stddev = sqrtf(variance);

        if (stddev <= cfg_.capture_pitch_var_deg) {
            // Capture accepted. Push the mean pitch as the new reference
            // offset for the online estimator. Future Phase 4.6.5 will push
            // a full quaternion through MountingCalibration; for now this
            // pitch-only path is the structurally-correct hook.
            online_est_.reset_to_reference(cap_pitch_mean_, now_ms);
            mount_status_ = online_est_.get_status();
            // SAFETY: do NOT auto-transition to AUTO_TUNE here. Auto-tune
            // drives motors aggressively; the operator must explicitly opt
            // in by long-pressing the button (or 't' over serial) once the
            // bot is positioned on its tuning stand.
            enter_state_(BalanceAppState::IDLE, now_ms);
        } else {
            // Too jittery — back to IDLE. User will re-press to retry.
            enter_state_(BalanceAppState::IDLE, now_ms);
        }
    }
}

void BalanceApp::step_tune_(uint32_t now_ms) {
    // The tuner controls the plant during AUTO_TUNE. We forward the corrected
    // pitch as the measurement and apply the tuner's output to the motors.
    const float meas = corrected_pitch_();
    const float out  = tuner_.step(meas, now_ms);
    const int16_t pwm = (int16_t)out;
    motors_.set_speed(pwm);
    last_output_ = pwm;

    // Hard-stop on tipover during tune (the tuner has its own max_angle
    // tripwire via SafetyLimits, but defense in depth here too).
    if (safety_.is_tipover(pitch_deg_)) {
        tuner_.request_abort();
        // Force a step through the abort path so the tuner finalizes a
        // failure result on the next iteration; for this tick, disarm.
        motors_.stop();
        last_output_ = 0;
    }

    if (safety_.abort_requested()) {
        tuner_.request_abort();
        safety_.clear_abort();
    }

    if (tuner_.is_done()) {
        tune_result_ = tuner_.result();
        motors_.stop();
        last_output_ = 0;

        if (tuner_.succeeded()) {
            tuner_.apply_to(pid_);
            pid_.reset();
            enter_state_(BalanceAppState::RUN, now_ms);
        } else {
            tuner_.restore_original(pid_);
            enter_state_(BalanceAppState::IDLE, now_ms);
        }
    }
}

void BalanceApp::step_run_(uint32_t now_ms) {
    // "Minimize accelerations" philosophy (user-driven, 2026-05-12):
    //
    // The robot's job is exactly one thing — keep the gravity vector pointed
    // through the wheel axis. Anything else (fall detection, recovery state
    // machines, special tipped-over modes) is layered on top and tends to
    // misbehave more than it helps. With conservative gains (Kp=15 Ki=0 Kd=8)
    // and a tight PWM cap (±80) plus stiction dead-band (±25), small pitch
    // errors produce small motor activity; large errors produce capped
    // activity. That IS the "minimize accelerations" controller.
    //
    // So fall detection is disabled by default. The PID runs forever in RUN.
    // If the bot tips past the linear region (~5-8°) the PID saturates at
    // ±80 and the bot will simply lie on its side with the motors spinning
    // benignly (no traction → no damage, no runaway). HELD is still active
    // so the user can pick up the bot and have motors stop.
    //
    // Define USE_BALANCE_FALL_DETECTION at build time to re-enable the
    // tipover → FALLEN transition. Saturation timeout (below) remains
    // unconditional — it protects against hardware faults (stuck wheel,
    // wiring problem) where motors are pegged for >3 s, not against falls.
#ifdef USE_BALANCE_FALL_DETECTION
    if (safety_.is_tipover(pitch_deg_)) {
        enter_state_(BalanceAppState::FALLEN, now_ms);
        return;
    }
#endif
    if (safety_.abort_requested()) {
        safety_.clear_abort();
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // RUN → HELD entry. Lateral-axis gyro (sqrt(gx²+gz²)) OR accel-magnitude
    // deviation from gravity, sustained 150 ms (30 ticks @ 5 ms). Lateral
    // gyro avoids false trigger on pitch-axis recoveries; accel deviation
    // catches sudden lifts. Gated by USE_BALANCE_HELD_DETECTION — operator
    // can disable for the simplest possible "balance forever" mode.
#ifdef USE_BALANCE_HELD_DETECTION
    if (motion_filters_init_ &&
        (g_lateral_dps_lpf_ > 30.0f || a_dev_lpf_ > 3.0f)) {
        if (hold_enter_count_ < 65535) hold_enter_count_ += 1;
        if (hold_enter_count_ >= 30) {
            enter_state_(BalanceAppState::HELD, now_ms);
            return;
        }
    } else {
        hold_enter_count_ = 0;
    }
#endif

    // PID step (Item 1: research_osoyoo_reference_implementation.md §3 / §9).
    // Use the corrected (de-biased) pitch and the raw gyro pitch-axis rate as
    // the D-term source. Numerical differentiation of fused BNO055 NDOF pitch
    // adds ~25 ms of phase lag (NDOF group delay + LPF + diff); raw gyro is
    // the instantaneous physical measurement and gives clean damping with no
    // numerical-derivative noise.
    pid_.set_setpoint(0.0f);
    const float meas           = corrected_pitch_();
    const float gyro_pitch_dps = raw_gyro_dps_[1];  // Y-axis = pitch rate
    const float out = pid_.compute_with_rate(meas, gyro_pitch_dps,
                                              cfg_.pid_sample_ms);
    const int16_t pwm = (int16_t)out;

    // Item 4 — soft-cutoff when tipped past the linear region
    // (research_osoyoo_reference_implementation.md §3 #3 + §5f). When pitch
    // exceeds ±25° the controller is well outside the linear regime; further
    // PWM just spins wheels uselessly (or worse, hammers the motor driver on
    // the side of a tipped-over chassis). We let the PID *compute* anyway so
    // I-term and OnlineMountingEstimator state stay coherent, and so motors
    // re-engage instantly when the bot is righted (no sticky FALLEN state,
    // no state transition, no operator intervention required).
    //
    // Threshold (25°) sits between the 10° linear-region edge and the 35°
    // hard limit. Below 25° the linear-region assumption holds and the PID
    // can still recover; above 25° the wheels can't lift the chassis back
    // through that angle anyway, so PWM is wasted.
    //
    // This is the user's "no acceleration -> no motors" framing applied to
    // the fallen-flat case. Documented in MINIMIZE_ACCELERATIONS_PHILOSOPHY.md.
    const float abs_pitch = pitch_deg_ < 0.0f ? -pitch_deg_ : pitch_deg_;
    const bool soft_cutoff = (abs_pitch > 25.0f);

    if (soft_cutoff) {
        motors_.stop();
        last_output_ = 0;
    } else {
        motors_.set_speed(pwm);
        last_output_ = pwm;
    }

    // Saturation timeout: if the PID is pegged near the RUN cap for too long,
    // the controller is fighting an impossible state (wrong polarity, stuck
    // wheel, mount-offset way off). Warn — but do NOT transition to FALLEN;
    // Item 4's soft-cutoff makes the sticky-FALLEN escape obsolete and the
    // bot can recover autonomously the moment the operator rights it.
    static const int16_t  SAT_THRESHOLD_PWM = 180;
    static const uint32_t SAT_TIMEOUT_MS    = 3000;
    const int16_t mag = pwm < 0 ? -pwm : pwm;
    if (mag >= SAT_THRESHOLD_PWM) {
        if (sat_run_start_ms_ == 0) {
            sat_run_start_ms_ = now_ms;
        } else if (now_ms - sat_run_start_ms_ > SAT_TIMEOUT_MS) {
            // Item 3 — no Serial.print in ISR path. Reset the timer; if the
            // hardware fault persists the operator will notice from the
            // status command (s) and from the soft-cutoff zeroing motors
            // when |pitch| > 25° anyway.
            sat_run_start_ms_ = 0;
            // Stay in RUN — soft-cutoff (Item 4) already protects motors
            // when the bot is tipped; there's nothing useful FALLEN adds.
        }
    } else {
        sat_run_start_ms_ = 0;
    }

    // Item 2 — wire real windup + gyro signals into OnlineMountingEstimator
    // (bootstrap_protocol_unstable_plant.md §6, "highest-risk open bug").
    // Without these, the estimator's transient-freeze gate has been receiving
    // hardcoded {windup_active=false, gyro_pitch_dps=0} since Phase 4.4 ran.
    // Every recovery and pickup-replace contaminated the offset LPF.
    //
    //   - windup_active: PID output at >=95% of saturation for >=2 consecutive
    //     ticks. Two-tick hold avoids one-sample noise tripping the freeze;
    //     real saturation events last many ticks.
    //   - gyro_pitch_dps: raw_gyro_dps_[1] (Y-axis). Estimator freezes
    //     adaptation above HIGH_GYRO_THRESHOLD_DPS (60°/s).
    if (cfg_.enable_online_adaptation) {
        const float i_term = pid_.get_i_term();

        const float pid_output    = pid_.get_output();
        const float pid_output_abs = pid_output < 0.0f ? -pid_output : pid_output;
        const float sat_threshold = cfg_.output_max * 0.95f;
        if (pid_output_abs >= sat_threshold) {
            if (sat_consecutive_ticks_ < 255) sat_consecutive_ticks_++;
        } else {
            sat_consecutive_ticks_ = 0;
        }
        const bool windup_active = (sat_consecutive_ticks_ >= 2);

        online_est_.update(i_term,
                           pitch_deg_,
                           /*tipover_active=*/false,
                           windup_active,
                           /*gyro_pitch_dps=*/gyro_pitch_dps,
                           now_ms);
        mount_status_ = online_est_.get_status();

        // Phase 4.10 — universal zero-knowledge auto-tune (Item 5).
        // Body lives in run_plant_id_() so the ISR-side step_run_ stays
        // compact. See run_plant_id_() for algorithm details.
        run_plant_id_(gyro_pitch_dps, windup_active, now_ms);
    }
}

// Phase 4.10 — universal zero-knowledge auto-tune (Item 5 of
// docs/PHASE_4_STRUCTURAL_FIXES.md).
//
// The PlantIdentifier RLS-fits K_motor (deg/s² per PWM unit) from the
// pwm_total / gyro_pitch_acceleration / pitch triple. From K_motor we get
// analytical Kp/Kd targets via pole-placement (ω_n² / K, 2ζω_n/K). We then
// rate-limit-ramp the live PID gains toward those targets so a transient
// bad estimate can't destabilise the loop.
//
// Freeze gates (research_universal_zero_knowledge_tuning.md §6.2,
// bootstrap_protocol_unstable_plant.md §3.5):
//   - bootstrap window (first BOOTSTRAP_FREEZE_MS after RUN entry)
//   - large lateral gyro (handling — not pitch motion)
//   - PID saturation (regression invalid above linear region)
// Tipover doesn't appear here because soft-cutoff (Item 4) already zeros
// the motors, so the regressor naturally goes to ~0 and the MIN_PHI
// excitation gate in PlantIdentifier skips the update.
//
void BalanceApp::run_plant_id_(float gyro_pitch_dps, bool windup_active,
                               uint32_t now_ms) {
    constexpr uint32_t BOOTSTRAP_FREEZE_MS = 5000;
    const bool bootstrap_freeze = (now_ms - run_entered_ms_) < BOOTSTRAP_FREEZE_MS;
    adaptive_active_ = !bootstrap_freeze;
    const bool high_lateral = (g_lateral_dps_lpf_ > 30.0f);

    // pwm_total = pwm_left + pwm_right; both wheels get the same command in
    // the current driver. Use the actual applied output (post soft-cutoff)
    // so the regression reflects what the plant physically saw.
    const float pwm_total = 2.0f * (float)last_output_;
    const float dt_sec    = (float)cfg_.pid_sample_ms * 0.001f;

    plant_id_.update(pwm_total,
                     gyro_pitch_dps,
                     gyro_pitch_prev_dps_,
                     pitch_deg_,
                     dt_sec,
                     /*freeze=*/bootstrap_freeze || high_lateral ||
                                windup_active);

    if (adaptive_active_) {
        // Adapt Kp and Kd only. Ki stays at whatever the seed/operator set —
        // its job is to feed the OnlineMountingEstimator a small signal, and
        // adapting it would couple the two adaptive loops (Anderson 2005
        // "Failures of adaptive control theory" — bursting risk).
        const PlantIdentifierStatus& ps = plant_id_.get_status();
        ramp_gain_(applied_kp_, ps.kp_target, dt_sec);
        ramp_gain_(applied_kd_, ps.kd_target, dt_sec);
        ramp_gain_(applied_ki_, ps.ki_target, dt_sec);
        pid_.set_tunings(applied_kp_, applied_ki_, applied_kd_);
    }

    // Stash the rate for next tick's α = (rate − prev)/dt differentiation.
    gyro_pitch_prev_dps_ = gyro_pitch_dps;
}

void BalanceApp::step_held_(uint32_t now_ms) {
    // HELD: motion signals say the bot is being handled. Motors are off.
    // Resume when motion calms down — short dwell, lenient thresholds.
    // User feedback 2026-05-12 evening: the 800 ms / strict-alignment gate
    // was too sluggish; the bot felt "dead" after being repositioned.
    // Trade-off: faster resume means a brief false-resume during a
    // mid-handling pause is more likely. Acceptable — motors are gentle
    // (no slam) and the operator can re-trigger HELD by moving the bot.
    motors_.stop();
    last_output_ = 0;

    if (safety_.abort_requested()) {
        safety_.clear_abort();
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // HELD→RUN: motion quiet AND roughly level. Sustained 200 ms (40 ticks).
    // No a_align surface-contact check (held-still in mid-air should be
    // allowed to resume too — motors balance benignly while floating, and
    // setting down upright is the common case anyway).
    const float abs_pitch = pitch_deg_ < 0.0f ? -pitch_deg_ : pitch_deg_;
    const bool quiet = (g_lateral_dps_lpf_ < 12.0f) && (a_dev_lpf_ < 1.5f);
    const bool level = abs_pitch < 8.0f;

    if (quiet && level) {
        if (hold_exit_count_ < 65535) hold_exit_count_ += 1;
        if (hold_exit_count_ >= 40) {
            enter_state_(BalanceAppState::RUN, now_ms);
            return;
        }
    } else {
        hold_exit_count_ = 0;
    }
    // No HELD timeout — operator preference is "always be ready to balance".
    // If the bot sits in HELD for hours (e.g. someone left it on a shelf
    // at an angle), it stays in HELD: harmless, motors off.
}

void BalanceApp::step_fallen_(uint32_t now_ms) {
    // FALLEN is STICKY: motors stay off until the operator explicitly
    // restarts. Recovery paths:
    //   - short-press button → enter_run_with_current_gains
    //   - serial 'c'         → CAPTURE_MOUNTING (re-save mount, then RUN)
    //   - serial 'R'         → enter_run_with_current_gains (skip capture)
    //   - long-press         → abort flag → IDLE (no auto-restart)
    // The HELD state (entered from RUN when motion signals indicate the
    // operator is handling the bot) is the non-sticky counterpart that
    // auto-resumes when the bot is set back down upright.
    motors_.stop();
    last_output_ = 0;

    if (safety_.abort_requested()) {
        safety_.clear_abort();
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }
}

// ---------------------------------------------------------------------------
// User input
// ---------------------------------------------------------------------------

void BalanceApp::on_short_press(uint32_t now_ms) {
    switch (state_) {
        case BalanceAppState::IDLE:
            enter_state_(BalanceAppState::CAPTURE_MOUNTING, now_ms);
            break;
        case BalanceAppState::HELD:
            // Operator-forced resume: skip the 800 ms HELD→RUN dwell.
            // Useful when the operator knows the bot is correctly placed
            // and wants to bypass the motion-quiescence check.
            enter_state_(BalanceAppState::RUN, now_ms);
            break;
        case BalanceAppState::FALLEN:
            // Operator-initiated restart from sticky FALLEN. Bypasses the
            // enter_run_with_current_gains() IDLE-only guard because the
            // button is unambiguous operator intent.
            pid_.set_output_limits(-80.0f, 80.0f);
            pid_.set_setpoint(0.0f);
            pid_.reset();
            enter_state_(BalanceAppState::RUN, now_ms);
            break;
        default:
            // CAPTURE / TUNE / RUN ignore short press in the skeleton.
            break;
    }
}

void BalanceApp::on_long_press(uint32_t now_ms) {
    // Long press is the universal "get me out" signal.
    safety_.request_abort();

    // Special case: in IDLE, long-press also kicks off AUTO_TUNE skipping
    // the mounting capture. This is the "I trust the saved offset" gesture.
    if (state_ == BalanceAppState::IDLE) {
        // Clear the abort we just set — we're acting on the long-press here
        // as an explicit transition request, not as an abort.
        safety_.clear_abort();
        enter_state_(BalanceAppState::AUTO_TUNE, now_ms);
    }
}

// ---------------------------------------------------------------------------
// Inspection
// ---------------------------------------------------------------------------

float BalanceApp::get_mount_offset_deg() const {
    return online_est_.get_estimate_deg();
}

const char* BalanceApp::state_name() const {
    switch (state_) {
        case BalanceAppState::IDLE:             return "IDLE";
        case BalanceAppState::CAPTURE_MOUNTING: return "CAPTURE_MOUNTING";
        case BalanceAppState::AUTO_TUNE:        return "AUTO_TUNE";
        case BalanceAppState::RUN:              return "RUN";
        case BalanceAppState::HELD:             return "HELD";
        case BalanceAppState::FALLEN:           return "FALLEN";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void BalanceApp::enter_run_with_current_gains(uint32_t now_ms) {
    if (state_ != BalanceAppState::IDLE) {
        return;
    }
    // NO balance-specific PWM cap (2026-05-12 operator preference, post-bench
    // session). Motor authority is bounded only by the hardware ±255 range.
    // The user's framing: the bot should DYNAMICALLY know how much PWM
    // produces how much acceleration; an arbitrary balance-mode cap is
    // throwing away authority the controller may need to catch a fall.
    // System-ID future work documented at docs/findings/dynamic_pwm_accel_learning.md.
    pid_.set_output_limits(-255.0f, 255.0f);
    pid_.set_setpoint(0.0f);
    pid_.reset();
    enter_state_(BalanceAppState::RUN, now_ms);
}

void BalanceApp::enter_state_(BalanceAppState s, uint32_t now_ms) {
    state_ = s;
    state_entered_ms_ = now_ms;
    recovery_count_ = 0;
    reset_capture_accumulators_();

    // Item 3 — deferred state-transition log. enter_state_() may run from
    // either loop() (tests, IDLE→CAPTURE on short-press) or ISR (RUN→HELD,
    // RUN→FALLEN). Serial.print is NOT ISR-safe (its TX buffer can deadlock
    // with a main-loop write). Just stash the new state byte; loop() drains
    // via drain_state_log(). If a transition fires before the previous log
    // was drained, the later transition wins — acceptable trade-off versus
    // a TX-buffer race.
    pending_state_log_ = static_cast<uint8_t>(s);

    // State-entry side effects.
    switch (s) {
        case BalanceAppState::IDLE:
            motors_.stop();
            last_output_ = 0;
            break;
        case BalanceAppState::CAPTURE_MOUNTING:
            motors_.stop();
            last_output_ = 0;
            // mounting_ is left untouched in the skeleton — see notes at
            // top of file. Future Phase 4.6.5 will call mounting_.start_capture()
            // here once raw accel/gyro are exposed on the sensor base.
            break;
        case BalanceAppState::AUTO_TUNE: {
            motors_.stop();
            last_output_ = 0;
            SafetyLimits limits;
            safety_.populate_tuner_safety(limits);
            limits.max_duration_sec = cfg_.tune_max_duration_sec;
            tuner_.set_safety_limits(limits);
            tuner_.begin(pid_,
                         /*setpoint=*/0.0f,
                         cfg_.output_min,
                         cfg_.output_max,
                         now_ms);
            break;
        }
        case BalanceAppState::RUN: {
            pid_.reset();
            sat_run_start_ms_       = 0;   // fresh saturation-timeout window
            sat_consecutive_ticks_  = 0;   // Item 2 windup-active hold counter
            hold_enter_count_       = 0;

            // Phase 4.10 — seed PlantIdentifier prior from current PID gains
            // so its first target is consistent with the seed. The
            // bootstrap_freeze window (handled in step_run_) gives the loop
            // ~5 s to settle before the RLS begins adapting.
            float kp_now, ki_now, kd_now;
            pid_.get_tunings(kp_now, ki_now, kd_now);
            plant_id_.reset(kp_now, kd_now);
            applied_kp_         = kp_now;
            applied_ki_         = ki_now;
            applied_kd_         = kd_now;
            gyro_pitch_prev_dps_ = 0.0f;
            run_entered_ms_     = now_ms;
            adaptive_active_    = false;
            break;
        }
        case BalanceAppState::HELD:
            motors_.stop();
            last_output_ = 0;
            pid_.reset();
            hold_exit_count_ = 0;
            hold_fall_count_ = 0;
            break;
        case BalanceAppState::FALLEN:
            motors_.stop();
            last_output_ = 0;
            pid_.reset();
            break;
    }
}

void BalanceApp::read_imu_(uint32_t /*now_ms*/) {
    // Best-effort read; if it fails, we keep the previous pitch_deg_ value.
    // (A failed IMU read for too long will trip the watchdog on the host
    // side because the host loop also feeds it externally.)
    imu_.read();
    const OrientationData& od = imu_.getOrientation();
    pitch_deg_ = od.pitch_deg;

    // Motion signals for HELD/FALLEN distinction. Lateral gyro = sqrt(gx²+gz²)
    // (NOT |gyro| — pitch-axis motion is intrinsic to balancing and would
    // false-trigger HELD during a recovery). See balance_held_fallen_state_
    // machine.md §3, §5.
    //
    // Body-frame axis convention follows BNO055 mounting: pitch axis is Y,
    // so gx and gz are the "non-pitch" components. If the bot is mounted
    // with a different axis assignment, swap accordingly here.
    float g[3] = {0.0f, 0.0f, 0.0f};
    float a[3] = {0.0f, 0.0f, 0.0f};
    const bool have_g = imu_.getRawGyro(g);
    const bool have_a = imu_.getRawAccel(a);

    if (have_g) {
        // Stash the raw rates so step_run_ can feed the gyro pitch-axis rate
        // (index 1, BNO055 body Y = pitch axis) into PID compute_with_rate()
        // and OnlineMountingEstimator's transient-freeze gate. See balance_app.h
        // for axis convention notes.
        raw_gyro_dps_[0] = g[0];
        raw_gyro_dps_[1] = g[1];
        raw_gyro_dps_[2] = g[2];
    }

    if (have_g && have_a) {
        // Lateral gyro magnitude (deg/s).
        const float g_lat = sqrtf(g[0]*g[0] + g[2]*g[2]);

        // Accel magnitude deviation from gravity. |a|−g goes negative on
        // free-fall, positive when being shaken or carried with impulses.
        const float a_mag = sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
        const float a_dev = a_mag - 9.81f;
        const float a_dev_abs = a_dev < 0.0f ? -a_dev : a_dev;

        // Body-Z alignment with gravity: 1.0 ≈ upright sitting on a surface,
        // ~0.0 ≈ lying on side, negative ≈ upside-down. Hand-held bots
        // rarely keep Z aligned with gravity due to subtle hand motion.
        // Guard against divide-by-zero in zero-g (impossible on Earth).
        const float a_align_raw = (a_mag > 0.1f) ? (a[2] / a_mag) : 0.0f;

        // ~120 ms time constant LPF on all three (alpha = dt/(τ+dt),
        // with dt=5 ms -> alpha ≈ 0.04). Hard-coded for AVR cycle budget.
        const float alpha = 0.04f;
        if (!motion_filters_init_) {
            g_lateral_dps_lpf_   = g_lat;
            a_dev_lpf_           = a_dev_abs;
            a_align_             = a_align_raw;
            motion_filters_init_ = true;
        } else {
            g_lateral_dps_lpf_ += alpha * (g_lat       - g_lateral_dps_lpf_);
            a_dev_lpf_         += alpha * (a_dev_abs   - a_dev_lpf_);
            a_align_           += alpha * (a_align_raw - a_align_);
        }
    }
    // If raw access failed (e.g. driver not initialized) we leave the
    // motion filters at last known good. The HELD entry gate degrades to
    // pitch-only, which is acceptable — no false negatives, just no false-
    // positive suppression.
}

float BalanceApp::corrected_pitch_() const {
    return pitch_deg_ - online_est_.get_estimate_deg();
}

void BalanceApp::ramp_gain_(float& live, float target, float dt_sec) {
    // 5%/s of current value per second (proportional bound), with an absolute
    // floor of 0.001 PWM/s so a zero-valued gain can climb. Tiny: ~6 lines of
    // float-only arithmetic on AVR.
    float abs_live = live < 0.0f ? -live : live;
    float bound    = 0.05f * dt_sec * abs_live;
    if (bound < 0.001f) bound = 0.001f;
    float delta = target - live;
    if (delta >  bound) delta =  bound;
    if (delta < -bound) delta = -bound;
    live += delta;
}

void BalanceApp::reset_capture_accumulators_() {
    cap_sample_count_ = 0;
    cap_pitch_mean_   = 0.0f;
    cap_pitch_m2_     = 0.0f;
    cap_pitch_first_  = 0.0f;
}
