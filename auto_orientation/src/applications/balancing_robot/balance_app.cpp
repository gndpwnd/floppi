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
// PROGMEM / pgm_read_byte: AVR keeps tables in flash. Other platforms (Teensy,
// ESP32, native test) put tables in regular memory; we shim the macros to
// plain pointer access so the same source compiles everywhere.
#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
  #include <avr/pgmspace.h>
  #include <util/atomic.h>
#else
  #ifndef PROGMEM
    #define PROGMEM
  #endif
  #ifndef pgm_read_byte
    #define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
  #endif
  // On non-AVR (native test, Teensy/ESP32) ATOMIC_BLOCK is a no-op shim. The
  // ISR/loop race only exists on the 8-bit AVR path where a 4-byte float load
  // can be interrupted mid-byte. On 32-bit platforms float reads are naturally
  // atomic; native tests run single-threaded so there is no race at all.
  #ifndef ATOMIC_BLOCK
    #define ATOMIC_BLOCK(x)
    #define ATOMIC_RESTORESTATE
  #endif
#endif

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
      pending_state_log_(0xFF),
      ch_last_idx_(0xFF),
      ch_gyro_acc_x10_(0),
      ch_response_thr_(0),
      ch_stiction_pwm_(0),
      bs_phase_idx_(0),
      bs_pulse_count_(0),
      bs_pulse_active_(false),
      bs_pulse_start_gyro_(0.0f),
      bs_noise_alpha_max_(0.0f),
      bs_prev_gyro_(0.0f),
      bs_k_sum_(0.0f),
      linear_accel_mag_(0.0f),
      collision_sustain_counter_(0),
      collision_latched_(false)
#ifdef USE_WHEEL_ENCODERS
      ,
      // Phase 4M.11 — pin pairs from balance_app.h (constexpr ENC_*_*). Order
      // matches member declaration (enc_left_ first). The WheelEncoder ctor
      // is cheap — just stashes the pin numbers; the heavy work (attaching
      // ISR vectors via PJRC Encoder) happens in begin().
      enc_left_(ENC_L_A, ENC_L_B),
      enc_right_(ENC_R_A, ENC_R_B),
      pwm_disc_step_index_(0),
      pwm_disc_cur_pwm_(0),
      pwm_disc_step_end_ms_(0),
      pwm_disc_prev_vel_dps_(0),
      pwm_disc_plateau_run_(0),
      pwm_disc_min_locked_(false)
#endif
{
    pulse_log_.seq            = 0;
    pulse_log_.source         = 0;
    pulse_log_.pulse_idx      = 0;
    pulse_log_.cmd_pwm        = 0;
    pulse_log_.gyro_start_x10 = 0;
    pulse_log_.metric_x10     = 0;
    pulse_log_.thr_x10        = 0;
    pulse_log_.passed         = 0;
    bootstrap_result_.k_motor         = 0.0f;
    bootstrap_result_.derived_kp      = 0.0f;
    bootstrap_result_.derived_kd      = 0.0f;
    bootstrap_result_.derived_ki      = 0.0f;
    bootstrap_result_.pulses_valid    = 0;
    bootstrap_result_.pulses_total    = 0;
    bootstrap_result_.failure_reason  = 0;
    bootstrap_result_.converged       = false;
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

#ifdef USE_WHEEL_ENCODERS
    pwm_discovery_result_.discovered_min_pwm = 0;
    pwm_discovery_result_.discovered_max_pwm = 0;
    pwm_discovery_result_.steps_attempted    = 0;
    pwm_discovery_result_.failure_reason     = 0;
    pwm_discovery_result_.discovered         = false;
#endif
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

#ifdef USE_WHEEL_ENCODERS
    // Phase 4M.11 — attach encoder ISRs. begin() is idempotent so re-calling
    // BalanceApp::begin() (e.g. after a config reload) doesn't double-attach.
    // Failure to allocate is non-fatal: read_ticks() / stalled() will both
    // return 0/false, degrading to "no encoder data" without crashing.
    enc_left_.begin();
    enc_right_.begin();
#endif

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
        case BalanceAppState::CHAR_ACT:         step_char_act_(now_ms);  break;
        case BalanceAppState::BOOTSTRAP:        step_bootstrap_(now_ms); break;
#ifdef USE_WHEEL_ENCODERS
        case BalanceAppState::PWM_DISCOVERY:    step_pwm_discovery_(now_ms); break;
#endif
        // PWM_DISCOVERY is enum-defined unconditionally but only reachable when
        // USE_WHEEL_ENCODERS is set; on Uno the enter_pwm_discovery() entry
        // point isn't compiled, so the switch fallthrough (no default needed
        // — -Wswitch is off in this TU's build flags) is safe.
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
            // Auto-chain to BOOTSTRAP. With mount captured, the bot is upright
            // and propped; BOOTSTRAP runs ±PWM pulses to measure K_motor and
            // derives Kp/Kd/Ki before entering RUN. If pitch drifted between
            // capture-end and bootstrap-start, BOOTSTRAP's own pre-condition
            // check (|pitch| < 10°) catches it and bails to IDLE.
            enter_state_(BalanceAppState::BOOTSTRAP, now_ms);
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

    // Collision detected during RUN — drop to HELD (lenient, not sticky
    // FALLEN). Operator preference 2026-05-18: an external impact is the
    // same class of event as a pickup. HELD will auto-resume once motion
    // is quiet + level, so the bot recovers without operator intervention.
    if (collision_latched_) {
        enter_state_(BalanceAppState::HELD, now_ms);
        return;
    }

    // RUN → HELD entry. Lateral-axis gyro (sqrt(gx²+gz²)) OR accel-magnitude
    // deviation from gravity, sustained 150 ms (30 ticks @ 5 ms). Lateral
    // gyro avoids false trigger on pitch-axis recoveries; accel deviation
    // catches sudden lifts. Gated by USE_BALANCE_HELD_DETECTION — operator
    // can disable for the simplest possible "balance forever" mode.
#ifdef USE_BALANCE_HELD_DETECTION
    // Phase 2.5 (universal balance vision, backlog #9) + lateral-gyro fallback.
    //
    // Trigger 1 — externally-driven motion: motor command is small but pitch
    // gyro shows fast rotation. The motion is NOT caused by motor torque, so
    // an external force is moving the bot (operator lifted, shoved, or the
    // bot is falling). This is the operator's "motion without motor command =
    // falling" heuristic, re-framed as HELD trigger so the bot doesn't fight
    // the external force. Cheap (no quaternion math) and complementary to
    // the lateral-gyro / accel-dev test below.
    //
    // Trigger 2 — lenient lateral-gyro (legacy, 2026-05-18 thresholds): catches
    // sustained handling/lifting that the cheap test misses. Tripled from the
    // original to avoid firing on recovery transients. Phase 2.7 will replace
    // this with full motor-null-space projection.
    // Bench finding 2026-05-18 PM (Phase D 25 s observation): the lateral-gyro
    // threshold at 90 dps fires on every wheel-engaged recovery transient and
    // is the root cause of the RUN↔HELD oscillation. Removed it. The remaining
    // signals are physically grounded:
    //   - Phase 2.5 ext_motion (cmd quiet + pitch gyro fast) — bot is moving
    //     but PID isn't pushing → external force (handle, fall, restraint)
    //   - accel-deviation > 6 m/s² — gravity vector dropped → bot lifted
    // The legacy lateral-gyro path will return when Phase 2.7 ships the proper
    // motor-null-space projection (which can't be confused by yaw/roll induced
    // during a pitch recovery).
    const int16_t last_cmd_mag = last_output_ < 0 ? -last_output_ : last_output_;
    const float pitch_gyro_now = raw_gyro_dps_[1];
    const float abs_pitch_gyro = pitch_gyro_now < 0.0f ? -pitch_gyro_now : pitch_gyro_now;
    const bool ext_motion = (last_cmd_mag < 20) && (abs_pitch_gyro > 30.0f);
    const bool lift_detected = motion_filters_init_ && (a_dev_lpf_ > 6.0f);
    if (ext_motion || lift_detected) {
        if (hold_enter_count_ < 65535) hold_enter_count_ += 1;
        const uint16_t dwell = ext_motion ? 20 : 60;
        if (hold_enter_count_ >= dwell) {
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
    float out = pid_.compute_with_rate(meas, gyro_pitch_dps,
                                              cfg_.pid_sample_ms);

    // Phase 2.6 — gain scheduling (backlog #10). Scale output linearly inside
    // a soft zone around balance. Eliminates the operator's "motors too high
    // speed near balance" complaint by reducing D-term-dominated PWM bursts
    // on micro gyro noise. Outside the soft zone, output is unscaled — full
    // recovery authority.
    //
    // 2026-05-18 PM bench finding: 2° was too wide. At 1° pitch, output was
    // only 50% of PID command, not enough to arrest a real fall. Tightened
    // to 1° so recovery authority returns to 100% at half the tilt. The bot
    // gets full PID command for any real disturbance and gentle output only
    // for genuine balance-point trim.
    const float abs_pitch_for_sched = meas < 0.0f ? -meas : meas;
    constexpr float SOFT_ZONE_DEG = 1.0f;
    if (abs_pitch_for_sched < SOFT_ZONE_DEG) {
        out *= (abs_pitch_for_sched / SOFT_ZONE_DEG);
    }
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

#ifdef USE_WHEEL_ENCODERS
    // Phase 4M.11 — encoder stall detection.
    //
    // Report the applied PWM magnitude to BOTH encoders so each can timestamp
    // when its commanded effort crossed ENCODER_STALL_PWM_THRESHOLD. Both
    // wheels currently receive the same command (motors_.set_speed(pwm)
    // mirrors L = R), so |last_output_| is the right magnitude for both.
    // Also poll read_velocity_dps so each encoder's internal velocity window
    // advances — stalled() reads that cached velocity rather than re-deriving.
    //
    // failure reason 7 (motor_stall) is the new BootstrapResult.failure_reason
    // sentinel for encoder-detected wheel stall during RUN. Defined here as
    // documentation; RUN doesn't write BootstrapResult, but transitioning to
    // HELD via the same reason code keeps the operator-visible failure surface
    // unified across BOOTSTRAP and RUN telemetry.
    const uint16_t cmd_mag = (last_output_ < 0) ? (uint16_t)(-last_output_)
                                                : (uint16_t) last_output_;
    enc_left_.report_commanded_pwm(cmd_mag, now_ms);
    enc_right_.report_commanded_pwm(cmd_mag, now_ms);
    (void)enc_left_.read_velocity_dps(now_ms);
    (void)enc_right_.read_velocity_dps(now_ms);

    if (enc_left_.stalled(ENCODER_STALL_PWM_THRESHOLD,
                          ENCODER_STALL_TIME_MS) ||
        enc_right_.stalled(ENCODER_STALL_PWM_THRESHOLD,
                           ENCODER_STALL_TIME_MS)) {
        // Same lenient outcome as collision detection: drop to HELD, not
        // FALLEN. A stalled wheel is most often the operator restraining the
        // bot or a temporary obstruction; HELD's quiescence-and-level gate
        // resumes balance once the obstruction clears.
        enter_state_(BalanceAppState::HELD, now_ms);
        return;
    }
#endif

    // STUCK detector (2026-05-18 operator-proposed): PID saturated AND no
    // angular motion = something restraining the wheels (operator holding,
    // chassis trapped, motor stalled). Today's bench data caught a 38 s
    // episode of out=+255 / pitch frozen at -2.41° while operator was
    // physically restraining the bot. Without this detector, motors stall
    // against external force and damage themselves.
    //
    // Condition: |output| >= 180 (saturated) AND |gyro_pitch| < 5 deg/s
    // (no rotation actually happening) for >= 1500 ms.
    // Action: stop motors, request abort -> IDLE on next tick.
    static const int16_t  SAT_THRESHOLD_PWM = 180;
    static const float    STUCK_GYRO_DPS    = 5.0f;
    static const uint32_t STUCK_TIMEOUT_MS  = 1500;
    const int16_t mag = pwm < 0 ? -pwm : pwm;
    const float abs_gy = gyro_pitch_dps < 0 ? -gyro_pitch_dps : gyro_pitch_dps;
    if (mag >= SAT_THRESHOLD_PWM && abs_gy < STUCK_GYRO_DPS) {
        if (sat_run_start_ms_ == 0) {
            sat_run_start_ms_ = now_ms;
        } else if (now_ms - sat_run_start_ms_ > STUCK_TIMEOUT_MS) {
            sat_run_start_ms_ = 0;
            motors_.stop();
            last_output_ = 0;
            safety_.request_abort();   // -> IDLE on next tick
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
            // Operator-initiated restart from sticky FALLEN. Route through
            // BOOTSTRAP rather than direct-RUN so the bot re-measures K_motor
            // (battery may have sagged, surface may have changed during the
            // tipover) and re-derives gains for current conditions. Skips the
            // IDLE-only guard via direct state transition because the button
            // press is unambiguous operator intent.
            pid_.set_setpoint(0.0f);
            pid_.reset();
            enter_state_(BalanceAppState::BOOTSTRAP, now_ms);
            break;
        default:
            // CAPTURE / TUNE / RUN ignore short press in the skeleton.
            break;
    }
}

void BalanceApp::on_long_press(uint32_t now_ms) {
    // Long press is the universal "get me out" signal.
    safety_.request_abort();

    // Special case: in IDLE, long-press kicks off BOOTSTRAP skipping the
    // mounting capture (use the currently-loaded offset). Phase 4.10c — see
    // step_bootstrap_(). The relay-feedback AUTO_TUNE path is no longer wired
    // to long-press; RLS plant-ID via BOOTSTRAP + continuous RUN-time
    // adaptation replaces it.
    if (state_ == BalanceAppState::IDLE) {
        // Clear the abort we just set — we're acting on the long-press here
        // as an explicit transition request, not as an abort.
        safety_.clear_abort();
        enter_state_(BalanceAppState::BOOTSTRAP, now_ms);
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
        case BalanceAppState::CAPTURE_MOUNTING: return "CAP";
        case BalanceAppState::AUTO_TUNE:        return "TUNE";
        case BalanceAppState::RUN:              return "RUN";
        case BalanceAppState::HELD:             return "HELD";
        case BalanceAppState::FALLEN:           return "FAL";
        case BalanceAppState::BOOTSTRAP:        return "BOOT";
#ifdef USE_WHEEL_ENCODERS
        case BalanceAppState::PWM_DISCOVERY:    return "PWMD";
#endif
        default:                                return "?";   // CHAR_ACT, etc
    }
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

void BalanceApp::clear_collision() {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        collision_latched_         = false;
        collision_sustain_counter_ = 0;
    }
}

void BalanceApp::enter_state_(BalanceAppState s, uint32_t now_ms) {
    state_ = s;
    state_entered_ms_ = now_ms;
    recovery_count_ = 0;
    reset_capture_accumulators_();

    // Every state transition clears the collision latch + sustain counter.
    // Rationale: BOOTSTRAP / CHAR_ACT abort to IDLE on collision; RUN drops
    // to HELD; entering RUN from HELD should start with a clean detector.
    // If the next state is still in collision territory, the next read_imu_
    // tick will re-latch immediately.
    collision_latched_         = false;
    collision_sustain_counter_ = 0;

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
            //
            // Fix B (audit P1-SM-1): skip the back-derived reset() when the
            // just-finished BOOTSTRAP measured K directly. plant_id_.reset()
            // back-solves K from Kp (K = ω_n²/Kp) AND inflates the RLS
            // covariance to INITIAL_P (1.0), which would let early noise
            // drown the freshly-measured K. seed_k_motor() (called from
            // step_bootstrap_'s FINALISE) leaves covariance at SEED_P (0.05)
            // so the measurement is trusted; clobbering it here negates the
            // entire point of BOOTSTRAP. Only reset() when entering RUN from
            // a non-converged path (manual enter_run_with_current_gains,
            // CAPTURE→RUN without BOOTSTRAP, etc).
            float kp_now, ki_now, kd_now;
            pid_.get_tunings(kp_now, ki_now, kd_now);
            if (!bootstrap_result_.converged) {
                plant_id_.reset(kp_now, kd_now);
            }
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
        case BalanceAppState::CHAR_ACT:
            motors_.stop();
            last_output_ = 0;
            ch_last_idx_ = 0xFF;       // sentinel: first tick triggers pulse-0 init
            ch_gyro_acc_x10_ = 0;
            ch_response_thr_ = 0;      // computed in baseline phase
            ch_stiction_pwm_ = 0;
            break;
#ifdef USE_WHEEL_ENCODERS
        case BalanceAppState::PWM_DISCOVERY:
            // Phase 4M.12 — reset all sweep accumulators so a re-entry starts
            // clean. Motors held stopped on entry; the first step_pwm_discovery_
            // tick increments to PWM_DISC_STEP_PWM and starts the ramp. Encoders
            // are zeroed so the first velocity sample windows from a known
            // origin (no carry-over from prior IDLE / RUN motion).
            motors_.stop();
            last_output_ = 0;
            pwm_disc_step_index_   = 0;
            pwm_disc_cur_pwm_      = 0;
            pwm_disc_step_end_ms_  = now_ms + PWM_DISC_STEP_DURATION_MS;
            pwm_disc_prev_vel_dps_ = 0;
            pwm_disc_plateau_run_  = 0;
            pwm_disc_min_locked_   = false;
            enc_left_.reset_ticks();
            enc_right_.reset_ticks();
            pwm_discovery_result_.discovered_min_pwm = 0;
            pwm_discovery_result_.discovered_max_pwm = 0;
            pwm_discovery_result_.steps_attempted    = 0;
            pwm_discovery_result_.failure_reason     = 0;
            pwm_discovery_result_.discovered         = false;
            break;
#endif
        case BalanceAppState::BOOTSTRAP:
            motors_.stop();
            last_output_ = 0;
            bs_phase_idx_         = 0;
            bs_pulse_count_       = 0;
            bs_pulse_active_      = false;
            bs_pulse_start_gyro_  = 0.0f;
            // bs_prev_gyro_ + bs_noise_alpha_max_ are re-purposed during the
            // baseline window as running gyro_min / gyro_max. Seed both from
            // the current gyro so the running comparison is well-defined on
            // the very first baseline tick (before that tick has had a chance
            // to compare against itself). Replaces the previous per-tick |α|
            // accumulator, which compared 5 ms tick-diffs against the pulse's
            // 100 ms-averaged signal — apples-to-oranges, ~5–20× over-
            // threshold at realistic BNO055 noise levels.
            bs_prev_gyro_         = raw_gyro_dps_[1];
            bs_noise_alpha_max_   = raw_gyro_dps_[1];
            bs_k_sum_             = 0.0f;
            bootstrap_result_.pulses_valid   = 0;
            bootstrap_result_.pulses_total   = 0;
            bootstrap_result_.k_motor        = 0.0f;
            bootstrap_result_.derived_kp     = 0.0f;
            bootstrap_result_.derived_kd     = 0.0f;
            bootstrap_result_.derived_ki     = 0.0f;
            bootstrap_result_.failure_reason = 0;
            bootstrap_result_.converged      = false;
            break;
    }
}

// ---------------------------------------------------------------------------
// Phase 2 — actuator characterisation. Pulses motors at successively higher
// PWM values, accumulates |gyro_pitch| over each pulse, records the first PWM
// that produces measurable angular response as the stiction floor. Universal
// across bots, surfaces, batteries — replaces hardcoded stiction_min_pwm.
// ---------------------------------------------------------------------------
void BalanceApp::enter_characterise_actuator(uint32_t now_ms) {
    if (state_ == BalanceAppState::IDLE) {
        enter_state_(BalanceAppState::CHAR_ACT, now_ms);
    }
}

// ---------------------------------------------------------------------------
// Phase 4.10c BOOTSTRAP — measure K_motor from controlled ±PWM pulses, derive
// Kp/Kd/Ki via pole-placement (Kp = ω_n²/K, Kd = 2ζω_n/K), push to PID, enter
// RUN already tuned to this specific chassis. Universal across bots, surfaces,
// batteries, payloads — retires the hardcoded Kp/Ki/Kd seed.
//
// Algorithm: short symmetric pulses (±100, ±150 PWM), measure peak gyro α via
// finite-difference on raw_gyro_dps_[1], K_i = |Δω/dt| / |pwm_total_i|.
// Mean over valid pulses → K_motor. Reference design:
// docs/findings/bootstrap_protocol_unstable_plant.md.
//
// Pre-condition: |corrected_pitch| < BOOTSTRAP_MAX_INIT_PITCH (10°). Operator
// is presumed to have propped the bot upright after CAPTURE_MOUNTING.
//
// Pulse parameters are STRUCTURAL design choices (analogous to "6 pulses in
// CHARACTERISE"), not per-bot tunings: 4 pulses × 100ms each is long enough to
// excite measurable acceleration above noise floor, short enough that the bot
// doesn't drift past the linear regime during identification. Magnitudes
// (100/150 PWM/wheel) sit firmly above any reasonable stiction floor and well
// below saturation (255).
// ---------------------------------------------------------------------------
void BalanceApp::enter_bootstrap(uint32_t now_ms) {
    if (state_ != BalanceAppState::IDLE) {
        return;
    }
    enter_state_(BalanceAppState::BOOTSTRAP, now_ms);
}

void BalanceApp::step_bootstrap_(uint32_t now_ms) {
    // Phase layout (all relative to state_entered_ms_):
    //   0..299     ms   PHASE 0 baseline — motors off, peak |α| noise capture
    //   300..449   ms   PULSE 0  +180 PWM/wheel
    //   450..849   ms   COOL  0  motors off (400 ms), capture Δω → K_0
    //   850..999   ms   PULSE 1  -180 PWM/wheel
    //   1000..1399 ms   COOL  1  → K_1
    //   1400..1549 ms   PULSE 2  +240 PWM/wheel
    //   1550..1949 ms   COOL  2  → K_2
    //   1950..2099 ms   PULSE 3  -240 PWM/wheel
    //   2100..2499 ms   COOL  3  → K_3
    //   ≥ 2500     ms   FINALISE  → push gains, enter RUN (or IDLE on fail)
    //
    // Fix D (audit "THEN" todo.md): cooldown extended 150 → 400 ms so the bot
    // has time to settle between pulses. Bench 2026-05-18 PM late showed
    // pulse N+1 starting at -25 dps while pulse N produced +20 dps —
    // 150 ms was too short for the chassis to dump prior-pulse momentum,
    // contaminating |Δω|. Total BOOTSTRAP duration ~2.5 s (was 1.5 s).
    static const uint16_t BASELINE_MS = 300;
    static const uint16_t PULSE_MS    = 150;
    static const uint16_t COOLDOWN_MS = 400;
    static const uint8_t  N_PULSES    = 4;
    // Per-wheel pulse magnitudes. Alternating signs are applied below.
    // Bench finding 2026-05-18 evening: 100/150 PWM did not always overcome
    // motor stiction + battery sag, producing zero |Δω| and a `no_response`
    // bail-out. Raised to 180/240 so the smaller pulse is well above any
    // reasonable stiction floor and the larger pulse approaches saturation
    // (255). Pulse duration also raised 100→150 ms to give slow motors more
    // time to reach steady acceleration before the cooldown latch.
    static const uint8_t  PULSE_PWMS[4] PROGMEM = {180, 180, 240, 240};
    // Safety: abort if pitch leaves ±15° during identification — bot is
    // tipping over and pulses would only make it worse.
    static const float    BOOTSTRAP_ABORT_PITCH = 15.0f;
    // Pre-condition: pitch must start within ±10° of the bot's calibrated
    // upright. Anything beyond means the bot wasn't propped upright before
    // bootstrap began — fail fast rather than firing motors at a tipped bot.
    static const float    BOOTSTRAP_MAX_INIT_PITCH = 10.0f;

    // Operator abort always honoured.
    if (safety_.abort_requested()) {
        safety_.clear_abort();
        motors_.stop();
        last_output_ = 0;
        bootstrap_result_.failure_reason = 4;
        bootstrap_result_.converged      = false;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // Collision detected during BOOTSTRAP — abort to IDLE with reason=5.
    // Stamps a diagnostic PulseLog entry (pulse_idx=0xFE sentinel = collision)
    // so loop() can surface the LIA magnitude that triggered the bail. Any
    // K_motor estimate from a pulse contaminated by an external impact would
    // be garbage — better to fail explicitly and let the operator retry.
    if (collision_latched_) {
        motors_.stop();
        last_output_ = 0;
        bootstrap_result_.failure_reason = 5;
        bootstrap_result_.converged      = false;
        pulse_log_.source         = 0;
        pulse_log_.pulse_idx      = 0xFE;   // sentinel: collision abort
        pulse_log_.cmd_pwm        = 0;
        pulse_log_.gyro_start_x10 = 0;
        pulse_log_.metric_x10     = (int16_t)(linear_accel_mag_ * 10.0f);
        pulse_log_.thr_x10        = (int16_t)(COLLISION_PEAK_MPS2 * 10.0f);
        pulse_log_.passed         = 0;
        pulse_log_.seq++;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    const uint16_t elapsed = (uint16_t)(now_ms - state_entered_ms_);
    const float    pitch_corr = corrected_pitch_();
    const float    abs_pitch  = pitch_corr < 0.0f ? -pitch_corr : pitch_corr;
    const float    gyro_now   = raw_gyro_dps_[1];
    const float    dt_sec     = (float)cfg_.pid_sample_ms * 0.001f;

    // Entry guard: during the baseline window (motors off, no impulse), pitch
    // must stay within ±BOOTSTRAP_MAX_INIT_PITCH. If the operator propped the
    // bot at a steep angle, the upcoming pulse phase would worsen it — fail
    // fast instead. This gate runs every baseline tick (not just the first)
    // so even a slow tip during the 300 ms baseline triggers a bail.
    if (elapsed < BASELINE_MS && abs_pitch > BOOTSTRAP_MAX_INIT_PITCH) {
        motors_.stop();
        last_output_ = 0;
        bootstrap_result_.failure_reason = 1;
        bootstrap_result_.converged      = false;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // Runtime safety: pitch left the safe envelope during a pulse — bail.
    if (abs_pitch > BOOTSTRAP_ABORT_PITCH) {
        motors_.stop();
        last_output_ = 0;
        bootstrap_result_.failure_reason = 1;
        bootstrap_result_.converged      = false;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // ---- PHASE 0: baseline noise floor ----
    // Track peak-to-peak gyro_y over the entire baseline window. This is in the
    // SAME units (dps) as the |Δω| measured during a pulse, so the downstream
    // "did this pulse beat the noise?" gate is apples-to-apples. The previous
    // implementation tracked per-tick |α| (5 ms tick-diff / dt), which over-
    // estimated noise by the ratio of the chip's true sample interval (10 ms
    // at 100 Hz NDOF) to our 5 ms tick — a real motor response at ~80 dps²
    // never cleared the resulting threshold.
    (void)dt_sec;
    if (elapsed < BASELINE_MS) {
        motors_.stop();
        last_output_ = 0;
        bs_pulse_active_ = false;
        if (gyro_now < bs_prev_gyro_)        bs_prev_gyro_       = gyro_now;
        if (gyro_now > bs_noise_alpha_max_)  bs_noise_alpha_max_ = gyro_now;
        return;
    }

    // Fix E (audit "THEN" todo.md): noisy-baseline rejection. If the operator
    // was still touching / placing the bot during the 300 ms baseline window,
    // peak-to-peak gyro shoots into double-digit dps. Without a cap, the
    // downstream 3× threshold balloons to >15 dps and no real pulse can ever
    // clear it -> all pulses fail -> IDLE with no_response (which is
    // misleading; the motors were fine). Cap the implied threshold at 5 dps,
    // bail explicitly with failure_reason=6 (baseline_noisy) so the operator
    // knows to retry with the bot at rest. Bench 2026-05-18 PM late second
    // boot showed thr=50 dps, all pulses failed.
    //
    // Check exactly once on the first tick past BASELINE_MS (before any pulse
    // has run, bs_phase_idx_ still 0 and no captured count yet).
    if (bs_phase_idx_ == 0 && bs_pulse_count_ == 0 && !bs_pulse_active_) {
        const float baseline_range = bs_noise_alpha_max_ - bs_prev_gyro_;
        // Baseline range was measured as peak-to-peak; the implied threshold
        // is 3× this. If baseline_range alone exceeds NOISE_FLOOR_MAX, the
        // bot wasn't at rest at all and we should not proceed.
        if (baseline_range > BOOTSTRAP_NOISE_FLOOR_MAX_DPS) {
            motors_.stop();
            last_output_ = 0;
            bootstrap_result_.failure_reason = 6;   // baseline_noisy
            bootstrap_result_.converged      = false;
            // Stash a diagnostic pulse_log entry so loop()'s drain shows the
            // operator the actual baseline_range we measured.
            pulse_log_.source         = 0;
            pulse_log_.pulse_idx      = 0xFF;       // sentinel: not a real pulse
            pulse_log_.cmd_pwm        = 0;
            pulse_log_.gyro_start_x10 = 0;
            pulse_log_.metric_x10     = (int16_t)(baseline_range * 10.0f);
            pulse_log_.thr_x10        = (int16_t)(BOOTSTRAP_NOISE_FLOOR_MAX_DPS * 10.0f);
            pulse_log_.passed         = 0;
            pulse_log_.seq++;
            enter_state_(BalanceAppState::IDLE, now_ms);
            return;
        }
    }

    // ---- PHASE 1..N: pulses + cooldowns ----
    if (bs_phase_idx_ < N_PULSES) {
        const uint16_t cycle_start_ms = BASELINE_MS +
                                        (uint16_t)bs_phase_idx_ * (PULSE_MS + COOLDOWN_MS);
        const uint16_t cycle_elapsed  = elapsed - cycle_start_ms;

        if (cycle_elapsed < PULSE_MS) {
            // Pulse-window: drive motors at signed PWM, latch start-of-pulse gyro.
            if (!bs_pulse_active_) {
                bs_pulse_active_     = true;
                bs_pulse_start_gyro_ = gyro_now;
            }
            const uint8_t pwm_mag = pgm_read_byte(&PULSE_PWMS[bs_phase_idx_]);
            // Sign alternates: even idx = +, odd idx = -. Net momentum cancels
            // across the full sequence so the bot doesn't drift in one direction.
            const int16_t signed_pwm = (bs_phase_idx_ & 1)
                                       ? -(int16_t)pwm_mag
                                       :  (int16_t)pwm_mag;
            motors_.set_speeds(signed_pwm, signed_pwm);
            last_output_ = signed_pwm;
            return;
        }

        if (cycle_elapsed < (PULSE_MS + COOLDOWN_MS)) {
            // Cooldown window — motors off. On entry, capture Δω from the just-
            // completed pulse and compute K_i. We do this exactly once per
            // cycle by transitioning bs_pulse_active_ false on the first
            // cooldown tick.
            if (bs_pulse_active_) {
                motors_.stop();
                last_output_ = 0;
                bs_pulse_active_ = false;

                const float dgyro = gyro_now - bs_pulse_start_gyro_;
                const float abs_dgyro = dgyro < 0.0f ? -dgyro : dgyro;
                // Compare the pulse's gyro excursion directly to the baseline
                // peak-to-peak gyro range — both quantities are in dps and span
                // a comparable time window (pulse 150 ms, baseline 300 ms, so
                // the noise envelope baseline_range slightly over-counts by
                // √2; we accept that conservative margin). Floor the threshold
                // at 0.5 dps so a synthetic / perfectly-still gyro doesn't
                // accept zero-mean noise as signal.
                const float baseline_range = bs_noise_alpha_max_ - bs_prev_gyro_;
                float thr_dps = 3.0f * baseline_range;
                if (thr_dps < 0.5f) thr_dps = 0.5f;
                // Fix C (audit "THEN" todo.md): sample-quality gate. If the
                // bot was still in motion at pulse start (|g0| > 5 dps), the
                // measured |Δω| is partly braking the prior-pulse momentum,
                // not pure plant excitation. Skip this pulse rather than
                // contaminate the K mean. Bench 2026-05-18 PM late showed
                // g0 values of -0.1, -25.5, +20.3, +1.9 across 4 pulses ->
                // K spread 0.09-0.74 (8× range) -> poisoned mean.
                const float abs_g0 = bs_pulse_start_gyro_ < 0.0f
                                     ? -bs_pulse_start_gyro_
                                     :  bs_pulse_start_gyro_;
                const bool  g0_clean = abs_g0 <= BOOTSTRAP_G0_MAX_DPS;
                const bool  passed   = g0_clean && (abs_dgyro > thr_dps);

                const uint8_t pwm_mag = pgm_read_byte(&PULSE_PWMS[bs_phase_idx_]);
                if (passed) {
                    // K = α / pwm_total = (|Δω|/τ) / pwm_total. Both wheels are
                    // driven identically → pwm_total = 2 × per-wheel.
                    const float pulse_sec = (float)PULSE_MS * 0.001f;
                    const float pwm_total = 2.0f * (float)pwm_mag;
                    const float k_i       = abs_dgyro / (pulse_sec * pwm_total);
                    bs_k_sum_     += k_i;
                    bs_pulse_count_++;
                }

                // Per-pulse telemetry record — drained by loop() (see
                // BalanceApp::drain_pulse_log). Lets the bench operator see
                // motor command + gyro response per pulse, which is the only
                // way to tell "motors didn't move" from "gyro didn't measure".
                const int16_t signed_pwm = (bs_phase_idx_ & 1)
                                           ? -(int16_t)pwm_mag
                                           :  (int16_t)pwm_mag;
                pulse_log_.source         = 0;
                pulse_log_.pulse_idx      = bs_phase_idx_;
                pulse_log_.cmd_pwm        = signed_pwm;
                pulse_log_.gyro_start_x10 = (int16_t)(bs_pulse_start_gyro_ * 10.0f);
                pulse_log_.metric_x10     = (int16_t)(abs_dgyro * 10.0f);
                pulse_log_.thr_x10        = (int16_t)(thr_dps * 10.0f);
                pulse_log_.passed         = passed ? 1 : 0;
                pulse_log_.seq++;
            }
            return;
        }

        // Cycle complete — advance phase index for next tick.
        bs_phase_idx_++;
        return;
    }

    // ---- FINALISE ----
    motors_.stop();
    last_output_ = 0;
    bootstrap_result_.pulses_total = N_PULSES;
    bootstrap_result_.pulses_valid = bs_pulse_count_;

    // Need at least half the pulses to have produced a measurable response —
    // otherwise either the motors are disconnected, the gyro axis is wrong,
    // or stiction is unusually high.
    if (bs_pulse_count_ < (N_PULSES / 2)) {
        bootstrap_result_.failure_reason = 2;   // no_response
        bootstrap_result_.converged      = false;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    const float k_measured = bs_k_sum_ / (float)bs_pulse_count_;
    bootstrap_result_.k_motor = k_measured;

    // Hand the measured K to the PlantIdentifier. It clamps to its (k_min,
    // k_max) class bounds and recomputes the pole-placement target gains.
    plant_id_.seed_k_motor(k_measured);
    const PlantIdentifierStatus& ps = plant_id_.get_status();

    // Sanity: if seed_k_motor had to clamp aggressively, the measurement was
    // likely garbage (motors didn't move, axis swapped). Bail before pushing
    // wild gains to the PID.
    const float k_clamped_diff = k_measured > ps.k_motor
                                 ? k_measured - ps.k_motor
                                 : ps.k_motor - k_measured;
    const float k_rel_diff = k_measured > 1e-6f ? k_clamped_diff / k_measured
                                                 : 1.0f;
    if (k_rel_diff > 0.5f) {
        bootstrap_result_.failure_reason = 3;   // k_out_of_bounds
        bootstrap_result_.converged      = false;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    bootstrap_result_.derived_kp     = ps.kp_target;
    bootstrap_result_.derived_kd     = ps.kd_target;
    bootstrap_result_.derived_ki     = ps.ki_target;
    bootstrap_result_.failure_reason = 0;
    bootstrap_result_.converged      = true;

    // Push gains so step_run_'s enter_state_ side-effect sees them as the
    // "current" PID state when it back-seeds plant_id_.
    pid_.set_tunings(ps.kp_target, ps.ki_target, ps.kd_target);
    enter_state_(BalanceAppState::RUN, now_ms);
    // adaptive_active_ defaults to false on RUN entry; bypass the bootstrap-
    // freeze window because pulse identification already gave us a clean K.
    // Same value adaptive_active_ would reach after BOOTSTRAP_FREEZE_MS in
    // step_run_'s run_plant_id_ — we just save those 5 seconds since we no
    // longer need that warm-up to estimate K from natural disturbances.
    adaptive_active_   = true;
    run_entered_ms_    = now_ms - 10000;   // back-date so freeze window expires
}

void BalanceApp::step_char_act_(uint32_t now_ms) {
    // Collision detected during CHARACTERISE — abort to IDLE. Same rationale
    // as BOOTSTRAP: a stiction-sweep contaminated by an external impact would
    // report the wrong floor (the impact's |gyro| swamps the pulse signal).
    if (collision_latched_) {
        motors_.stop();
        last_output_ = 0;
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    // Phase 2.1 layout:
    //   [0..200 ms]      BASELINE — motors off, measure |gyro_pitch| noise floor
    //   [200..1400 ms]   SWEEP    — 6 pulses x 200 ms, alternating direction
    //
    // Response threshold = max(3 × baseline_acc, RESP_FLOOR). The hardcoded
    // 10 deg/s threshold from the first 2026-05-18 implementation was the
    // root cause of the suspicious stiction=30 reading: settling oscillation
    // at bot-placement exceeded the threshold before motors even pulsed.
    // The measured-floor approach is universal — works on any bot, surface,
    // mounting orientation, and during natural gyro noise.
    static const uint8_t  PWM_TABLE[]  PROGMEM = {30, 60, 90, 120, 150, 200};
    static const uint8_t  N_PULSES     = 6;
    static const uint16_t PULSE_MS     = 200;
    static const uint16_t BASELINE_MS  = 200;
    static const uint16_t RESP_FLOOR   = 800;   // ~2 deg/s avg over 200 ms

    const uint16_t elapsed = (uint16_t)(now_ms - state_entered_ms_);

    // ---- Phase 2.1 baseline phase ----
    if (elapsed < BASELINE_MS) {
        motors_.stop();
        const float gy = raw_gyro_dps_[1];
        const float ag = gy < 0.0f ? -gy : gy;
        ch_gyro_acc_x10_ += (uint16_t)(ag * 10.0f);
        return;
    }

    // Transition from baseline → sweep: lock in threshold, reset accumulator.
    if (ch_last_idx_ == 0xFF) {
        uint32_t thr = (uint32_t)ch_gyro_acc_x10_ * 3;
        if (thr < RESP_FLOOR) thr = RESP_FLOOR;
        if (thr > 60000) thr = 60000;
        ch_response_thr_ = (uint16_t)thr;
        ch_last_idx_     = 0;
        ch_gyro_acc_x10_ = 0;
    }

    // ---- Sweep phase ----
    const uint16_t sweep_ms = elapsed - BASELINE_MS;
    const uint8_t  idx      = sweep_ms / PULSE_MS;

    if (idx >= N_PULSES) {
        motors_.stop();
        const bool last_passed = ch_gyro_acc_x10_ > ch_response_thr_;
        // Emit final pulse's telemetry before transitioning out.
        const uint8_t last_pwm_mag = pgm_read_byte(&PWM_TABLE[N_PULSES - 1]);
        pulse_log_.source         = 1;
        pulse_log_.pulse_idx      = N_PULSES - 1;
        pulse_log_.cmd_pwm        = ((N_PULSES - 1) & 1)
                                    ? -(int16_t)last_pwm_mag
                                    :  (int16_t)last_pwm_mag;
        pulse_log_.gyro_start_x10 = 0;
        pulse_log_.metric_x10     = (int16_t)ch_gyro_acc_x10_;
        pulse_log_.thr_x10        = (int16_t)ch_response_thr_;
        pulse_log_.passed         = last_passed ? 1 : 0;
        pulse_log_.seq++;
        if (ch_stiction_pwm_ == 0 && last_passed) {
            ch_stiction_pwm_ = last_pwm_mag;
        }
        enter_state_(BalanceAppState::IDLE, now_ms);
        return;
    }

    if (idx != ch_last_idx_) {
        // Emit telemetry for the just-completed pulse before switching to the
        // next PWM. ch_last_idx_ == 0xFF means we're transitioning out of the
        // baseline window — nothing to log yet.
        if (ch_last_idx_ != 0xFF) {
            const uint8_t prev_pwm = pgm_read_byte(&PWM_TABLE[ch_last_idx_]);
            const bool prev_passed = ch_gyro_acc_x10_ > ch_response_thr_;
            pulse_log_.source         = 1;
            pulse_log_.pulse_idx      = ch_last_idx_;
            pulse_log_.cmd_pwm        = (ch_last_idx_ & 1)
                                        ? -(int16_t)prev_pwm
                                        :  (int16_t)prev_pwm;
            pulse_log_.gyro_start_x10 = 0;
            pulse_log_.metric_x10     = (int16_t)ch_gyro_acc_x10_;
            pulse_log_.thr_x10        = (int16_t)ch_response_thr_;
            pulse_log_.passed         = prev_passed ? 1 : 0;
            pulse_log_.seq++;
            if (ch_stiction_pwm_ == 0 && prev_passed) {
                ch_stiction_pwm_ = prev_pwm;
            }
        }
        ch_last_idx_ = idx;
        ch_gyro_acc_x10_ = 0;
    }

    const uint8_t pwm = pgm_read_byte(&PWM_TABLE[idx]);
    const int16_t signed_pwm = (idx & 1) ? -(int16_t)pwm : (int16_t)pwm;
    motors_.set_speeds(signed_pwm, signed_pwm);

    const float gy = raw_gyro_dps_[1];
    const float ag = gy < 0.0f ? -gy : gy;
    ch_gyro_acc_x10_ += (uint16_t)(ag * 10.0f);
}

void BalanceApp::read_imu_(uint32_t /*now_ms*/) {
    // Best-effort read; if it fails, we keep the previous pitch_deg_ value.
    // (A failed IMU read for too long will trip the watchdog on the host
    // side because the host loop also feeds it externally.)
    imu_.read();
    const OrientationData& od = imu_.getOrientation();
    const float new_pitch = od.pitch_deg;

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

    // Compute LPF / motion-filter values into locals first, then publish under
    // a single critical section. Fix A (audit P0-ISR-1 / P0-ISR-2): the 200 Hz
    // MsTimer2 ISR (tick()) reads these member floats; without protection the
    // 4-byte loads can tear mid-byte on AVR, producing the BOOTSTRAP "K spread
    // across pulses" symptom (bench 2026-05-18 PM late). Compute first, atomic-
    // publish second — the critical section is the minimum possible to hide
    // the cross-boundary stores.
    float g_lat_new        = 0.0f;
    float a_dev_abs_new    = 0.0f;
    float a_align_raw_new  = 0.0f;
    bool  publish_motion   = false;

    if (have_g && have_a) {
        // Lateral gyro magnitude (deg/s).
        g_lat_new = sqrtf(g[0]*g[0] + g[2]*g[2]);

        // Accel magnitude deviation from gravity. |a|−g goes negative on
        // free-fall, positive when being shaken or carried with impulses.
        const float a_mag = sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
        const float a_dev = a_mag - 9.81f;
        a_dev_abs_new = a_dev < 0.0f ? -a_dev : a_dev;

        // Body-Z alignment with gravity: 1.0 ≈ upright sitting on a surface,
        // ~0.0 ≈ lying on side, negative ≈ upside-down. Hand-held bots
        // rarely keep Z aligned with gravity due to subtle hand motion.
        // Guard against divide-by-zero in zero-g (impossible on Earth).
        a_align_raw_new = (a_mag > 0.1f) ? (a[2] / a_mag) : 0.0f;
        publish_motion = true;
    }

    // Publish all ISR-shared floats inside one critical section. ATOMIC_BLOCK
    // saves/restores SREG and disables interrupts for the duration; on Uno
    // this is 2 instructions in / 2 out (push/cli, pop/sei), so the per-tick
    // overhead is well under 1 µs. The ~120 ms LPF update is included so the
    // ISR never sees a torn LPF state either.
    const float alpha = 0.04f;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        pitch_deg_ = new_pitch;
        if (have_g) {
            // Stash the raw rates so step_run_ can feed the gyro pitch-axis
            // rate (index 1, BNO055 body Y = pitch axis) into PID
            // compute_with_rate() and OnlineMountingEstimator's transient-
            // freeze gate. See balance_app.h for axis convention notes.
            raw_gyro_dps_[0] = g[0];
            raw_gyro_dps_[1] = g[1];
            raw_gyro_dps_[2] = g[2];
        }
        if (publish_motion) {
            if (!motion_filters_init_) {
                g_lateral_dps_lpf_   = g_lat_new;
                a_dev_lpf_           = a_dev_abs_new;
                a_align_             = a_align_raw_new;
                motion_filters_init_ = true;
            } else {
                g_lateral_dps_lpf_ += alpha * (g_lat_new       - g_lateral_dps_lpf_);
                a_dev_lpf_         += alpha * (a_dev_abs_new   - a_dev_lpf_);
                a_align_           += alpha * (a_align_raw_new - a_align_);
            }
        }
    }
    // If raw access failed (e.g. driver not initialized) we leave the
    // motion filters at last known good. The HELD entry gate degrades to
    // pitch-only, which is acceptable — no false negatives, just no false-
    // positive suppression.

    // ------------------------------------------------------------------
    // Collision detection — three-gate detector
    // (research_collision_signature_bno055.md §3 / §5).
    //
    // VECTOR_LINEARACCEL is gravity-removed body accel from the BNO055 NDOF
    // fusion. At rest / balancing the magnitude is well under 1 m/s²; an
    // impact spikes far above. Drivers that don't implement getLinearAccel()
    // return false and leave the buffer untouched — we treat that as "0 m/s²"
    // (no spike) so the detector is a benign no-op on those platforms.
    //
    // Gates are OR-combined: any one trip latches collision_detected_.
    // collision_latched_ stays true until clear_collision() or a state
    // transition resets it (see enter_state_()).
    float la[3] = {0.0f, 0.0f, 0.0f};
    const bool have_la = imu_.getLinearAccel(la);
    float la_mag_new = 0.0f;
    if (have_la) {
        la_mag_new = sqrtf(la[0]*la[0] + la[1]*la[1] + la[2]*la[2]);
    }
    // gyro_y for the KICK gate. Pull from the freshly-published raw_gyro_dps_
    // (same axis convention used by the PID D-term and bootstrap pulses).
    const float gyro_y_for_kick = have_g ? g[1] : 0.0f;
    const float abs_gyro_y      = gyro_y_for_kick < 0.0f ? -gyro_y_for_kick : gyro_y_for_kick;

    // SUSTAIN counter — runs whether or not the latch is already set so the
    // post-latch state is well-defined for any subsequent inspection. Counter
    // resets on any tick below the SUSTAIN floor.
    if (la_mag_new > COLLISION_SUSTAIN_MPS2) {
        if (collision_sustain_counter_ < 255) collision_sustain_counter_++;
    } else {
        collision_sustain_counter_ = 0;
    }

    const bool peak_fires    = (la_mag_new > COLLISION_PEAK_MPS2);
    const bool sustain_fires = (collision_sustain_counter_ >= COLLISION_SUSTAIN_TICKS);
    const bool kick_fires    = (la_mag_new > COLLISION_KICK_MPS2) &&
                               (abs_gyro_y > COLLISION_KICK_GYRO_DPS);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        linear_accel_mag_ = la_mag_new;
        if (peak_fires || sustain_fires || kick_fires) {
            collision_latched_ = true;
        }
    }
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

#ifdef USE_WHEEL_ENCODERS
// ---------------------------------------------------------------------------
// Phase 4M.11 — wheel encoder inspectors. Thin pass-throughs to the
// underlying WheelEncoder; declared in balance_app.h.
// ---------------------------------------------------------------------------
int32_t BalanceApp::encoder_left_ticks() {
    return enc_left_.read_ticks();
}

int32_t BalanceApp::encoder_right_ticks() {
    return enc_right_.read_ticks();
}

int32_t BalanceApp::encoder_avg_velocity_dps(uint32_t now_ms) {
    const int32_t vl = enc_left_.read_velocity_dps(now_ms);
    const int32_t vr = enc_right_.read_velocity_dps(now_ms);
    return (vl + vr) / 2;
}

bool BalanceApp::encoder_stalled() {
    return enc_left_.stalled(ENCODER_STALL_PWM_THRESHOLD,
                             ENCODER_STALL_TIME_MS) ||
           enc_right_.stalled(ENCODER_STALL_PWM_THRESHOLD,
                              ENCODER_STALL_TIME_MS);
}

// ---------------------------------------------------------------------------
// Phase 4M.12 — PWM range auto-discovery (MEGA_UNIVERSAL_PLAN.md §7d).
//
// Operator lifts the bot off the ground (wheels must spin freely), then issues
// the `p` command. We ramp commanded PWM 0 → 255 in PWM_DISC_STEP_PWM-sized
// steps every PWM_DISC_STEP_DURATION_MS:
//   - First step where both wheels show |velocity| > PWM_DISC_MIN_VELOCITY_DPS
//     locks discovered_min_pwm_ (stiction floor).
//   - After MIN is locked, PWM_DISC_PLATEAU_COUNT consecutive steps with
//     |Δvelocity| < PWM_DISC_PLATEAU_DELTA_DPS lock discovered_max_pwm_ as
//     the FIRST step in the plateau (saturation onset).
//   - If both bounds are locked, exit to IDLE with success.
//   - If we exceed PWM_DISC_TIMEOUT_MS, exit to IDLE with failure_reason=8.
//
// Per-step Serial telemetry is published via the existing PulseLog deferred-
// publish path (source=2 = PWM_DISCOVERY). Encoders are reset on entry so the
// velocity windows have a clean origin.
//
// Why this is safe to run from the ISR-side tick():
//   - motors_.set_speeds() = analogWrite + digitalWrite (AVR atomic)
//   - enc_*.read_velocity_dps() is a windowed forward-difference (no I²C, no
//     Serial). The PJRC Encoder library protects its int32_t accessor with
//     noInterrupts()/interrupts(); reads from a higher-prio ISR are safe.
//   - All telemetry goes through the deferred PulseLog (loop drains it).
// ---------------------------------------------------------------------------
void BalanceApp::enter_pwm_discovery(uint32_t now_ms) {
    if (state_ != BalanceAppState::IDLE) {
        return;
    }
    enter_state_(BalanceAppState::PWM_DISCOVERY, now_ms);
}

void BalanceApp::step_pwm_discovery_(uint32_t now_ms) {
    // Single-exit pattern: gather a uint8_t "finish_reason" code and let the
    // tail emit telemetry + transition once. 0 = keep ramping; 1 = success
    // (MAX locked); 4 = user_abort; 8 = timeout. Keeps the function small
    // (one motors_.stop + one enter_state_).
    uint8_t finish_reason = 0;

    if (safety_.abort_requested()) {
        safety_.clear_abort();
        finish_reason = 4;
    } else if ((uint32_t)(now_ms - state_entered_ms_) > PWM_DISC_TIMEOUT_MS) {
        finish_reason = 8;   // pwm_discovery_timeout
    } else {
        // Drive both wheels at the current step PWM. Direction stays positive
        // for the entire sweep — operator has lifted the bot, so the encoder-
        // forward convention is what matters (not chassis pose).
        motors_.set_speeds((int16_t)pwm_disc_cur_pwm_, (int16_t)pwm_disc_cur_pwm_);
        last_output_ = (int16_t)pwm_disc_cur_pwm_;
        // Advance the encoder velocity-window so the cache stays fresh during
        // the hold period.
        (void)enc_left_.read_velocity_dps(now_ms);
        (void)enc_right_.read_velocity_dps(now_ms);
        // Step boundary not yet reached — keep running.
        if ((int32_t)(now_ms - pwm_disc_step_end_ms_) < 0) return;
    }

    // -----------------------------------------------------------------------
    // STEP-END or FINISH: read final velocity sample, classify, then either
    // emit telemetry + advance the ramp, OR commit the result and exit.
    // -----------------------------------------------------------------------
    const int32_t v_l = enc_left_.read_velocity_dps(now_ms);
    const int32_t v_r = enc_right_.read_velocity_dps(now_ms);
    const int32_t v_l_abs = v_l < 0 ? -v_l : v_l;
    const int32_t v_r_abs = v_r < 0 ? -v_r : v_r;
    // Mean of the two wheel magnitudes — more robust to one-wheel encoder
    // noise than min/max, and matches the brute-force tuner's expectation of
    // a per-bot scalar bound.
    const int32_t v_mean_abs = (v_l_abs + v_r_abs) / 2;

    uint8_t passed_flag = 0;
    if (finish_reason == 0) {
        pwm_disc_step_index_++;
        // ---- MIN_PWM lock: first step where BOTH wheels clear threshold.
        // Both-wheels is stricter than mean — a one-wheel-stuck bot would
        // otherwise declare MIN at half the true stiction value.
        if (!pwm_disc_min_locked_ &&
            v_l_abs > PWM_DISC_MIN_VELOCITY_DPS &&
            v_r_abs > PWM_DISC_MIN_VELOCITY_DPS) {
            pwm_discovery_result_.discovered_min_pwm = pwm_disc_cur_pwm_;
            pwm_disc_min_locked_ = true;
            passed_flag = 1;
            pwm_disc_prev_vel_dps_ = (int16_t)v_mean_abs;
            pwm_disc_plateau_run_  = 0;
        } else if (pwm_disc_min_locked_) {
            // ---- MAX_PWM lock: PWM_DISC_PLATEAU_COUNT consecutive small-
            // delta steps. MAX is the FIRST step of the plateau (the
            // saturation onset, not the end).
            const int32_t dv = (int32_t)v_mean_abs - (int32_t)pwm_disc_prev_vel_dps_;
            const int32_t dv_abs = dv < 0 ? -dv : dv;
            if (dv_abs < PWM_DISC_PLATEAU_DELTA_DPS) {
                if (pwm_disc_plateau_run_ == 0) {
                    // Lock to the prior PWM (the last that actually grew
                    // velocity). pwm_disc_cur_pwm_ is the first that didn't.
                    pwm_discovery_result_.discovered_max_pwm =
                        pwm_disc_cur_pwm_ >= PWM_DISC_STEP_PWM
                        ? (uint16_t)(pwm_disc_cur_pwm_ - PWM_DISC_STEP_PWM)
                        : pwm_disc_cur_pwm_;
                }
                if (pwm_disc_plateau_run_ < 255) pwm_disc_plateau_run_++;
                if (pwm_disc_plateau_run_ >= PWM_DISC_PLATEAU_COUNT) {
                    passed_flag   = 2;
                    finish_reason = 1;   // success — fall through to finalize
                }
            } else {
                // Velocity still climbing — reset plateau accounting.
                pwm_disc_plateau_run_ = 0;
                pwm_discovery_result_.discovered_max_pwm = 0;
            }
            pwm_disc_prev_vel_dps_ = (int16_t)v_mean_abs;
        }
    }

    // Always emit telemetry for this step.
    pulse_log_.source         = 2;
    pulse_log_.pulse_idx      = pwm_disc_step_index_;
    pulse_log_.cmd_pwm        = (int16_t)pwm_disc_cur_pwm_;
    pulse_log_.gyro_start_x10 = (int16_t)(v_l * 10);
    pulse_log_.metric_x10     = (int16_t)(v_r * 10);
    pulse_log_.thr_x10        = (int16_t)(PWM_DISC_PLATEAU_DELTA_DPS * 10);
    pulse_log_.passed         = passed_flag;
    pulse_log_.seq++;

    if (finish_reason == 0) {
        // Continue ramping. Off-the-top => treat as timeout failure.
        if (pwm_disc_cur_pwm_ + PWM_DISC_STEP_PWM > 255) {
            finish_reason = 8;
        } else {
            pwm_disc_cur_pwm_     += PWM_DISC_STEP_PWM;
            pwm_disc_step_end_ms_  = now_ms + PWM_DISC_STEP_DURATION_MS;
            return;
        }
    }

    // Single-exit finalize.
    motors_.stop();
    last_output_ = 0;
    pwm_discovery_result_.steps_attempted = pwm_disc_step_index_;
    if (finish_reason == 1) {
        pwm_discovery_result_.failure_reason = 0;
        pwm_discovery_result_.discovered     = true;
    } else {
        pwm_discovery_result_.failure_reason = finish_reason;   // 4 or 8
        pwm_discovery_result_.discovered     = false;
    }
    enter_state_(BalanceAppState::IDLE, now_ms);
}
#endif  // USE_WHEEL_ENCODERS
