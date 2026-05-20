/**
 * BalanceApp — top-level state machine for the self-balancing robot
 * reference application (Phase 4.7a of the auto_orientation framework).
 *
 * Orchestrates four phases of operation:
 *
 *     IDLE -> CAPTURE_MOUNTING -> BOOTSTRAP -> RUN <----+
 *                                              ^  |    |
 *                                              |  v    |
 *                                              HELD ---+
 *                                              |
 *                                              v
 *                                            FALLEN (sticky)
 *
 * with edges back to IDLE on user abort, capture failure, bootstrap failure,
 * watchdog stall, or explicit long-press. FALLEN is sticky — only an
 * operator-initiated command (short-press / `c` / `R`) restarts.
 *
 * AUTO_TUNE was the legacy Phase 4.7a tuning path (relay-feedback). Phase
 * 4.10c retired it: BOOTSTRAP's ±PWM pulse identification + RLS plant-ID +
 * RUN-time adaptation replaces it. The enum value is kept for telemetry/ABI
 * compatibility but no code path reaches it.
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

#if !defined(F) && !defined(ARDUINO)
// Native test build: Arduino's F() macro from WString.h isn't available.
// In Arduino, F("literal") wraps the literal in a PROGMEM-aware container.
// On the host, plain string-literal passthrough is correct (PROGMEM is a no-op).
#define F(string_literal) (string_literal)
#endif

#include "../../sensors/sensor_base.h"
#include "../../control/pid_controller.h"
#include "../../control/auto_pid_tuner.h"
#include "../../control/plant_identifier.h"
#include "../../navigation/mounting_calibration.h"
#include "../../navigation/online_mounting_estimator.h"
#include "../../actuators/motor_driver.h"
#include "safety.h"

#ifdef USE_WHEEL_ENCODERS
#include "../../sensors/wheel_encoder.h"
#endif

enum class BalanceAppState : uint8_t {
    IDLE              = 0,
    CAPTURE_MOUNTING  = 1,
    AUTO_TUNE         = 2,
    RUN               = 3,
    HELD              = 4,   // bot picked up — motors off, ready to resume
    FALLEN            = 5,   // sticky tipover — operator must restart
    CHAR_ACT          = 6,   // Phase 2 — PWM sweep, measure stiction floor
    BOOTSTRAP         = 7,   // Phase 4.10c — measure K_motor, derive Kp/Ki/Kd
    PWM_DISCOVERY     = 8    // Phase 4M.12 — encoder-based PWM_MIN/MAX auto-discovery
                             // (Mega-only; only reachable when USE_WHEEL_ENCODERS).
                             // Operator lifts the bot off the ground; firmware
                             // ramps PWM 0→255 in steps and watches wheel
                             // encoders. First non-zero velocity = MIN_PWM;
                             // velocity plateau = MAX_PWM. Result saved to
                             // EEPROM 0x230 for the Python brute-force tuner.
};

#ifdef USE_WHEEL_ENCODERS
// Phase 4M.12 PWM_DISCOVERY result. Populated when PWM_DISCOVERY exits (back
// to IDLE in both success and failure paths). Zero-valued before the first
// run.
//
// `discovered_min_pwm` / `discovered_max_pwm` are in raw PWM units (0..255).
// `discovered=true` only when BOTH bounds were measured cleanly within the
// timeout. `failure_reason` codes: 0=ok, 4=user_abort, 8=pwm_discovery_timeout.
struct PwmDiscoveryResult {
    uint16_t discovered_min_pwm;
    uint16_t discovered_max_pwm;
    uint16_t steps_attempted;       // number of PWM steps executed
    uint8_t  failure_reason;
    bool     discovered;
};
#endif  // USE_WHEEL_ENCODERS

// Phase 4.10c BOOTSTRAP result. Populated when BOOTSTRAP exits (either to
// RUN with measured K_motor, or back to IDLE on failure).
struct BootstrapResult {
    float    k_motor;          // measured plant gain (deg/s² per PWM)
    float    derived_kp;       // ω_n²/K_motor (pushed to PID on success)
    float    derived_kd;       // 2ζω_n/K_motor
    float    derived_ki;       // 0.05 × Kp
    uint8_t  pulses_valid;     // count of pulses with detectable response
    uint8_t  pulses_total;     // total pulses applied (fixed by algorithm)
    uint8_t  failure_reason;   // 0=ok, 1=pitch_out_of_range, 2=no_response,
                               // 3=k_out_of_bounds, 4=user_abort,
                               // 5=collision (LIA spike during baseline/pulse),
                               // 6=baseline_noisy (operator handled bot during baseline)
    bool     converged;        // true iff K_motor was pushed to PlantIdentifier
};

// ---------------------------------------------------------------------------
// BOOTSTRAP K-quality constants (Fix C/D/E from audit_code_quality_balance_
// stack_2026-05-19.md + todo.md "THEN — fix BOOTSTRAP K-quality").
//
// Bench 2026-05-18 PM late produced a 0.09-0.74 K spread across 4 pulses
// (8× range, poisoned mean). Root cause: 150 ms cooldown was too short for
// the bot to settle, so pulse N+1 started with significant momentum from
// pulse N (g0 values: -0.1, -25.5, +20.3, +1.9 dps). Skipping pulses with
// large g0 + extending cooldown gives clean per-pulse K samples.
//
// BOOTSTRAP_G0_MAX_DPS (5.0 dps):
//   If gyro_y at the start of a pulse is above this magnitude, the bot is
//   still in motion from the prior pulse / cooldown. Including such a pulse
//   in the K estimate contaminates |Δω| with whatever the prior momentum
//   was decaying through (braking, not pure plant excitation). Skip and
//   move on — better to estimate K from fewer clean samples than from
//   noisy ones. Derivation: 3× the typical BNO055 NDOF gyro noise floor
//   (~1.5 dps 1σ at rest from bench observations), giving ~95% rejection
//   of true at-rest samples while catching ~5 dps and larger momentum.
//
// BOOTSTRAP_NOISE_FLOOR_MAX_DPS (5.0 dps):
//   Hard cap on the computed baseline noise threshold. The threshold
//   adapts to the actual baseline window (3 × peak-to-peak range), but
//   operator motion during the 300 ms baseline can balloon the measured
//   range to 50+ dps — at which point no real pulse can ever clear the
//   threshold and BOOTSTRAP fails with `no_response`. Capping at 5 dps
//   means: if baseline is noisier than this, the operator was clearly
//   not holding the bot still — fail explicitly with `baseline_noisy`
//   (failure_reason=6) so the operator knows to retry, rather than
//   silently masquerading as no_response. Matches BOOTSTRAP_G0_MAX_DPS
//   (same physical envelope — both express "bot is sufficiently at rest").
constexpr float BOOTSTRAP_G0_MAX_DPS          = 5.0f;
constexpr float BOOTSTRAP_NOISE_FLOOR_MAX_DPS = 5.0f;

// ---------------------------------------------------------------------------
// Collision detection — three-gate detector
// (research_collision_signature_bno055.md §3 / §5).
//
// Reads BNO055 VECTOR_LINEARACCEL (gravity-removed body accel) each tick, takes
// magnitude, and latches `collision_detected_` when ANY of the three OR-fire
// gates trips. NOTE: BNO055 NDOF LIA streams at 100 Hz internally; "ticks"
// below count BalanceApp::step() invocations (200 Hz default), so a
// COLLISION_SUSTAIN_TICKS of 3 corresponds to ~15 ms wall-time = roughly 1.5
// unique LIA samples — enough to confirm a sustained impact tail, short enough
// to fire before the bot falls.
//
//   PEAK    — |a| > COLLISION_PEAK_MPS2 single tick. Sharp impact.
//   SUSTAIN — |a| > COLLISION_SUSTAIN_MPS2 for >= COLLISION_SUSTAIN_TICKS.
//             Moderate impact tail.
//   KICK    — |a| > COLLISION_KICK_MPS2 AND |gyro_pitch| > COLLISION_KICK_GYRO_DPS.
//             Cross-arm: lower accel coincident with high rotation rate (the
//             "kick-over" signature where the bot is rotated sharply).
constexpr float   COLLISION_PEAK_MPS2       = 12.0f;
constexpr float   COLLISION_SUSTAIN_MPS2    = 8.0f;
constexpr uint8_t COLLISION_SUSTAIN_TICKS   = 3;
constexpr float   COLLISION_KICK_MPS2       = 6.0f;
constexpr float   COLLISION_KICK_GYRO_DPS   = 200.0f;

#ifdef USE_WHEEL_ENCODERS
// ---------------------------------------------------------------------------
// Wheel encoder integration (Phase 4M.11 — MEGA_UNIVERSAL_PLAN.md §7).
//
// Two quadrature encoders on Mega INT pins. Pin assignments match
// research_wheel_encoders_mega_2026-05-19.md §3:
//
//   Signal   Pin   INT vector   Notes
//   ------   ---   ----------   -----
//   L_ENC_A  18    INT5         Mega Serial1 TX — unused in balance build
//   L_ENC_B  19    INT4         Mega Serial1 RX — unused in balance build
//   R_ENC_A  2     INT0         general-purpose Mega INT pin
//   R_ENC_B  3     INT1         general-purpose Mega INT pin
//
// Pins 20/21 are reserved for I²C (BNO055); do NOT repurpose for encoders.
// If GPS is later added on Mega Serial1, the left encoder must move.
constexpr uint8_t ENC_L_A = 18;
constexpr uint8_t ENC_L_B = 19;
constexpr uint8_t ENC_R_A = 2;
constexpr uint8_t ENC_R_B = 3;

// Stall-detection thresholds passed to WheelEncoder::stalled() each tick.
//
// ENCODER_STALL_PWM_THRESHOLD (100):
//   The encoder only flags a stall when the *commanded* PWM exceeds this
//   value — small PWM (idle drift, micro-corrections, stiction-floor probes)
//   isn't expected to produce wheel motion, so it's not interesting for
//   stall detection. 100 sits comfortably above the observed BNO055
//   balance-bot stiction floor (~30-80 PWM bench 2026-05-18) and below
//   typical mid-recovery commands (~150 PWM). At/above this PWM the wheels
//   *should* be turning if the drivetrain is healthy.
//
// ENCODER_STALL_TIME_MS (300):
//   Sustained period the encoder must measure no motion before declaring
//   a stall. 300 ms = 60 ticks at 5 ms PID sample. Long enough that single-
//   pulse motor-driver glitches or one-tick gyro drops don't trip it; short
//   enough that the bot doesn't waste seconds pinning current into a wheel
//   that the operator is physically restraining. Replaces the gyro-based
//   STUCK detector's 1500 ms (which had to be long because the gyro
//   threshold of 5 dps was ambiguous between "wheel stalled" and "tipping
//   slowly"). Encoder velocity is unambiguous, so we can react faster.
constexpr uint16_t ENCODER_STALL_PWM_THRESHOLD = 100;
constexpr uint16_t ENCODER_STALL_TIME_MS       = 300;

// ---------------------------------------------------------------------------
// PWM range auto-discovery (Phase 4M.12 — MEGA_UNIVERSAL_PLAN.md §7d).
//
// Operator lifts the bot off the ground and triggers PWM_DISCOVERY. The
// firmware ramps commanded PWM from 0 upward in small steps, watching wheel
// encoders. First non-zero velocity = MIN_PWM (stiction floor); velocity
// plateaus = MAX_PWM (electrical/mechanical saturation onset). Both values
// are saved to EEPROM and consumed by the Python brute-force tuner so it
// doesn't have to GUESS PWM bounds.
//
// All constants are tunings rather than algorithmic parameters — they shape
// the trade-off between resolution and total wall-clock budget, not the
// algorithm itself.
//
// PWM_DISC_STEP_PWM (5):
//   Resolution of the discovered MIN/MAX (within ±5 PWM units). At 5 ms PID
//   sample and 200 ms step-duration, 5 PWM units / step gives 51 steps to
//   cover 0..255 — fits inside the 8 s timeout with ~2× headroom for a
//   typical stiction-then-plateau curve. Going smaller (1-2 PWM) would
//   exceed the timeout for noisy plants where the first non-zero velocity
//   isn't until ~PWM 80.
//
// PWM_DISC_STEP_DURATION_MS (200):
//   Hold each PWM step long enough for the motor + wheel inertia to reach
//   steady-state velocity. 200 ms is generous for the typical yellow-TT
//   motor's ~30 ms electromechanical time constant; the second half of the
//   window is used for the velocity sample, so the controller measures a
//   genuinely settled velocity not a transient.
//
// PWM_DISC_MIN_VELOCITY_DPS (5):
//   Threshold above which the wheel is considered "actually moving" rather
//   than encoder noise / single-tick quantization. Matches
//   WheelEncoder::kStallVelocityDps (also 5 dps) so the encoder API's
//   "moving / not moving" line is consistent across stall + discovery.
//
// PWM_DISC_PLATEAU_DELTA_DPS (2):
//   The step-over-step velocity change below which we declare velocity has
//   plateaued (i.e. PWM is no longer producing more wheel speed → motor or
//   driver is saturated). 2 dps is well above the per-window quantisation
//   floor at 100 ms windows (~1 dps) while still catching the small
//   electrical-saturation plateau on real motors.
//
// PWM_DISC_PLATEAU_COUNT (3):
//   Number of consecutive small-delta steps required to declare a plateau.
//   3 steps × 200 ms = 600 ms of "stuck velocity" before we lock MAX_PWM
//   to the FIRST of the plateau steps (the saturation onset). Three is the
//   minimum that distinguishes a real plateau from one-step encoder noise.
//
// PWM_DISC_TIMEOUT_MS (8000):
//   Total budget for the discovery procedure. (255 / 5) × 200 ms ≈ 10.2 s
//   if we have to ramp all the way to 255 without finding plateau, but
//   typical curves saturate near 200-220 PWM so 8 s gives the operator a
//   prompt failure on truly broken hardware (motor disconnected, encoder
//   dead) without giving up on slow plants.
constexpr uint16_t PWM_DISC_STEP_PWM           = 5;
constexpr uint16_t PWM_DISC_STEP_DURATION_MS   = 200;
constexpr int16_t  PWM_DISC_MIN_VELOCITY_DPS   = 5;
constexpr int16_t  PWM_DISC_PLATEAU_DELTA_DPS  = 2;
constexpr uint8_t  PWM_DISC_PLATEAU_COUNT      = 3;
constexpr uint16_t PWM_DISC_TIMEOUT_MS         = 8000;
#endif  // USE_WHEEL_ENCODERS

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

    // Convenience: built-in defaults for the balancing-robot reference build.
    // Post-BOOTSTRAP these initial PID gains are overwritten by the measured
    // K_motor result, so they only matter as a fallback if BOOTSTRAP never ran
    // (manual enter_run_with_current_gains path). Current values (see
    // balance_app.cpp): Kp=50 / Ki=2 / Kd=20, ±255 PWM, 5 ms sample,
    // 10° tilt limit / 4° recovery. Legacy 65/12/38 / 35°/15° values are
    // historical (legacy .ino) and no longer apply.
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
            case BalanceAppState::BOOTSTRAP:        out.println(F("BOOT")); break;
#ifdef USE_WHEEL_ENCODERS
            case BalanceAppState::PWM_DISCOVERY:    out.println(F("PWMD")); break;
#endif
            default:                                out.println(F("?")); break;
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

    // Phase 2 — start CHARACTERISE sweep. No-op unless state == IDLE.
    void enter_characterise_actuator(uint32_t now_ms);

    // Phase 2 — measured stiction floor (0 = no measurement / not finished).
    uint8_t get_ch_stiction_pwm() const { return ch_stiction_pwm_; }

    // Phase 4.10c — start BOOTSTRAP: apply symmetric ±PWM pulses, measure
    // gyro acceleration response, derive K_motor and seed the PlantIdentifier
    // + push Kp/Ki/Kd to the PID before entering RUN.
    //
    // Preconditions: state == IDLE AND |pitch_deg − online_est offset| <
    // BOOTSTRAP_MAX_INIT_PITCH (default 10°). If not, transitions back to
    // IDLE immediately with failure_reason = pitch_out_of_range.
    //
    // On success: PID gains updated from measured K_motor, state → RUN with
    // adaptive_active = true (no bootstrap-freeze window — pulses already
    // produced the K we need). On failure: state → IDLE, motors stopped.
    void enter_bootstrap(uint32_t now_ms);

    // Phase 4.10c — last BOOTSTRAP outcome. Zeros if BOOTSTRAP never ran.
    const BootstrapResult& get_bootstrap_result() const { return bootstrap_result_; }

    // Per-pulse diagnostic log, shared by BOOTSTRAP and CHARACTERISE so loop()
    // can stream each pulse's command + gyro response without violating
    // ISR-Serial.print rules. `seq` increments after every populated record;
    // drainer compares against its last-seen seq to spot new entries.
    struct PulseLog {
        uint8_t  seq;
        uint8_t  source;          // 0 = bootstrap, 1 = characterise
        uint8_t  pulse_idx;
        int16_t  cmd_pwm;         // signed per-wheel PWM applied this pulse
        int16_t  gyro_start_x10;  // gyro_y at pulse start (×10 dps)
        int16_t  metric_x10;      // bootstrap: |Δω| ×10; char: Σ|gyro|·dt ×10
        int16_t  thr_x10;         // threshold the metric was compared to
        uint8_t  passed;          // 0/1 — metric > thr
    };
    const PulseLog& get_pulse_log() const { return pulse_log_; }
    template <class TPrint>
    void drain_pulse_log(TPrint& out, uint8_t& last_seq) {
        if (pulse_log_.seq == last_seq) return;
        last_seq = pulse_log_.seq;
        // source: 0 = BOOTSTRAP pulse, 1 = CHARACTERISE pulse,
        //         2 = PWM_DISCOVERY step (Phase 4M.12, mega_balance only).
        // The PWM_DISCOVERY branch is gated by USE_WHEEL_ENCODERS so uno_balance
        // doesn't pay the extra string-literal + branch cost.
#ifdef USE_WHEEL_ENCODERS
        if (pulse_log_.source == 0)      out.print(F("bs#"));
        else if (pulse_log_.source == 1) out.print(F("ch#"));
        else                             out.print(F("pd#"));
#else
        out.print(pulse_log_.source == 0 ? F("bs#") : F("ch#"));
#endif
        out.print(pulse_log_.pulse_idx);
        out.print(F(" pwm=")); out.print(pulse_log_.cmd_pwm);
        // PWM_DISCOVERY repurposes the gyro_start_x10 slot to hold the LEFT
        // wheel velocity (x10 dps) and metric_x10 to hold the RIGHT wheel
        // velocity (x10 dps). thr_x10 carries the plateau-delta threshold so
        // operators see WHY a step did/didn't lock min/max. `passed` is 1 on
        // the step that locked MIN_PWM, 2 on the step that locked MAX_PWM,
        // and 0 otherwise.
        out.print(F(" g0=")); out.print(pulse_log_.gyro_start_x10 / 10.0f, 1);
        out.print(F(" m=")); out.print(pulse_log_.metric_x10 / 10.0f, 1);
        out.print(F(" thr=")); out.print(pulse_log_.thr_x10 / 10.0f, 1);
        out.print(F(" ok=")); out.println(pulse_log_.passed);
    }

    // ----- Inspection -----------------------------------------------------

    BalanceAppState get_state() const { return state_; }
    float           get_pitch_deg() const { return pitch_deg_; }
    float           get_mount_offset_deg() const;

    // HELD-entry reason codes (audit P1-SM-3). Diagnostic byte stamped by each
    // RUN→HELD path so the `s` status drain can tell operators WHY HELD fired
    // (collision impact vs. operator handling vs. encoder/gyro anomaly). The
    // value is cleared on RUN entry so a successful auto-resume starts a clean
    // slate; HELD entry itself does not clear it (we want to see the most
    // recent reason).
    enum HeldEntryReason : uint8_t {
        HELD_REASON_NONE              = 0,
        HELD_REASON_COLLISION         = 1,  // three-gate LIA detector latched
        HELD_REASON_GYRO_ANOMALY      = 2,  // encoder stall (motor pwm vs. velocity mismatch)
        HELD_REASON_OPERATOR_HANDLING = 3,  // lift / external-motion detector
    };
    uint8_t         get_held_entry_reason() const { return held_entry_reason_; }
    int16_t         get_last_output() const { return last_output_; }
    const MountingCalibrationStatus& get_mount_status() const { return mount_status_; }
    // get_tune_result() removed (audit P2-SM-1) — AUTO_TUNE state unreachable
    // and the tune_result_ member retired (~28 B RAM savings).
    const PlantIdentifierStatus& get_plant_status() const { return plant_id_.get_status(); }
    bool            is_adaptive_active() const { return adaptive_active_; }
    const char*     state_name() const;

    // ----- Collision detection (research_collision_signature_bno055.md) ----
    // Magnitude of the most recent VECTOR_LINEARACCEL read, in m/s². Updated
    // each tick by read_imu_(). Zero before the first IMU read or if the
    // driver does not implement getLinearAccel().
    float           get_linear_accel_mag() const { return linear_accel_mag_; }

    // True iff the three-gate detector latched a collision since the last
    // clear_collision() / state transition. Latching is the design — the bot
    // is small/fast and an impact can be a single 5 ms tick; we want callers
    // to see it on the next inspection even if the spike already passed.
    bool            collision_detected() const { return collision_latched_; }

    // Explicit reset. Called automatically on every state transition so the
    // RUN→HELD branch starts with a clean slate; tests use it to verify the
    // latch semantics.
    void            clear_collision();

#ifdef USE_WHEEL_ENCODERS
    // ----- PWM range auto-discovery (Phase 4M.12) --------------------------
    // Start PWM_DISCOVERY: operator must have lifted the bot off the ground
    // before calling. No-op if not currently in IDLE. Ramps commanded PWM
    // 0 → 255 in PWM_DISC_STEP_PWM increments, holding each for
    // PWM_DISC_STEP_DURATION_MS, and watches encoders for the first non-zero
    // velocity (MIN) and the velocity plateau (MAX). Result is exposed via
    // get_pwm_discovery_result() once state returns to IDLE.
    //
    // Mega-only — only compiled when USE_WHEEL_ENCODERS is defined. Host
    // application is expected to save the discovered values to EEPROM and
    // optionally seed them back into the L298NMotorDriver / PlantIdentifier
    // bounds at the next boot.
    void enter_pwm_discovery(uint32_t now_ms);

    // Last PWM_DISCOVERY outcome. Zeros if PWM_DISCOVERY never ran.
    const PwmDiscoveryResult& get_pwm_discovery_result() const {
        return pwm_discovery_result_;
    }

    // Convenience accessors (zero if discovery never ran / failed).
    uint16_t get_discovered_min_pwm() const {
        return pwm_discovery_result_.discovered ? pwm_discovery_result_.discovered_min_pwm
                                                : 0;
    }
    uint16_t get_discovered_max_pwm() const {
        return pwm_discovery_result_.discovered ? pwm_discovery_result_.discovered_max_pwm
                                                : 0;
    }

    // ----- Wheel encoder inspectors (Phase 4M.11) --------------------------
    // Read-through accessors so loop()/telemetry can see encoder state
    // without having to hold a reference to the encoders themselves. All
    // inspectors are safe to call from any context — the underlying
    // WheelEncoder reads atomically on AVR.
    int32_t encoder_left_ticks();
    int32_t encoder_right_ticks();

    // Mean of the two wheels' velocity in deg/s. Useful for the bot-frame
    // forward velocity (positive = forward). Note: this calls
    // read_velocity_dps on both encoders which advances their internal
    // windowed sample; tests should pass the same now_ms as the most recent
    // step() to keep window semantics intact.
    int32_t encoder_avg_velocity_dps(uint32_t now_ms);

    // True iff either wheel's stall detector latched on the most recent
    // step_run_ tick. Use BalanceApp::encoder_stalled_left()/right() if you
    // need to know which wheel — not exposed in the minimal v1 inspector
    // set per task spec ("encoder_stalled()" without per-wheel detail).
    bool encoder_stalled();
#endif

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
    uint8_t  held_entry_reason_;   // diagnostic — see HeldEntryReason above

    MountingCalibrationStatus mount_status_;
    // tune_result_ member removed (audit P2-SM-1) — AUTO_TUNE retired. ~28 B
    // RAM saved on Uno.

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

    // Per-state handlers. step_tune_ removed — AUTO_TUNE state retired
    // (audit P2-SM-1); enum value retained for ABI but is unreachable.
    void step_idle_(uint32_t now_ms);
    void step_capture_(uint32_t now_ms);
    void step_run_(uint32_t now_ms);
    void step_held_(uint32_t now_ms);
    void step_fallen_(uint32_t now_ms);
    void step_char_act_(uint32_t now_ms);
    void step_bootstrap_(uint32_t now_ms);
#ifdef USE_WHEEL_ENCODERS
    void step_pwm_discovery_(uint32_t now_ms);
#endif

    // Phase 2 CHAR_ACT state — pulse-sweep accumulator + result.
    // Phase 2.1: response threshold is now MEASURED (baseline noise × 3),
    // not the previous hardcoded 4000 (== avg 10 deg/s). Eliminates the
    // false-positive that produced stiction=30 on 2026-05-18.
    uint8_t  ch_last_idx_;       // most recent pulse index seen
    uint16_t ch_gyro_acc_x10_;   // sum of |gyro_pitch_dps| * 10 over current pulse
    uint16_t ch_response_thr_;   // measured threshold from baseline phase
    uint8_t  ch_stiction_pwm_;   // measured stiction floor (0 = not found)

    // Phase 4.10c BOOTSTRAP state — pulse loop accumulators + final result.
    // The step handler walks a small state machine indexed by bs_phase_idx_:
    //   0     : baseline noise window — accumulate |α| noise floor
    //   1..N  : alternating ±pulse + cooldown windows
    //   N+1   : finalize — median K_motor + push gains + transition
    // Phase index doubles as "are we currently driving the motors" via the
    // bs_pulse_active_ flag which step_bootstrap_ sets on pulse-window entry.
    uint8_t  bs_phase_idx_;         // current bootstrap phase (0..7)
    uint8_t  bs_pulse_count_;       // number of K samples successfully captured
    bool     bs_pulse_active_;      // are motors currently driven this tick
    float    bs_pulse_start_gyro_;  // gyro_y at pulse-start (for Δω measurement)
    float    bs_noise_alpha_max_;   // peak |α| seen during baseline window
    float    bs_prev_gyro_;         // for inter-tick gyro α differentiation
    float    bs_k_sum_;             // running sum of valid K samples
    BootstrapResult bootstrap_result_;

    // Per-pulse telemetry buffer (see PulseLog struct above). Written from the
    // ISR-side step_bootstrap_ / step_char_act_ at pulse-completion boundaries;
    // drained by loop() via drain_pulse_log().
    PulseLog pulse_log_;

    // Collision detector state (research_collision_signature_bno055.md).
    // Updated by read_imu_() every tick from VECTOR_LINEARACCEL magnitude.
    // collision_latched_ is sticky — once any gate fires it stays true until
    // clear_collision() or the next enter_state_() resets it. The SUSTAIN gate
    // counts consecutive ticks where |a| exceeds COLLISION_SUSTAIN_MPS2; reset
    // on any tick below floor.
    float    linear_accel_mag_;
    uint8_t  collision_sustain_counter_;
    bool     collision_latched_;

#ifdef USE_WHEEL_ENCODERS
    // Phase 4M.11 wheel-encoder integration. Two encoders, one per drive
    // wheel. Constructed with the pin pairs from balance_app.h's
    // ENC_{L,R}_{A,B} constants; begin() is called from BalanceApp::begin()
    // after the IMU init so the host application doesn't have to know about
    // them explicitly. step_run_ calls report_commanded_pwm() each tick and
    // queries stalled() to route into HELD on a stuck wheel. Construction
    // order in the .cpp initializer list must match member declaration order
    // (enc_left_ before enc_right_).
    WheelEncoder enc_left_;
    WheelEncoder enc_right_;

    // Phase 4M.12 PWM_DISCOVERY state. Driven by step_pwm_discovery_; reset on
    // every PWM_DISCOVERY entry via enter_state_'s side-effect block.
    //
    //   pwm_disc_step_index_   — count of completed PWM steps (also the
    //                            attempted-step counter exposed in the result)
    //   pwm_disc_cur_pwm_      — currently commanded PWM magnitude on both wheels
    //   pwm_disc_step_end_ms_  — wall-clock at which the current step expires
    //   pwm_disc_prev_vel_dps_ — last-step's measured |avg velocity|, used for
    //                            the plateau-delta comparison
    //   pwm_disc_plateau_run_  — consecutive small-delta count for plateau gate
    //   pwm_disc_min_locked_   — true once we've recorded the first non-zero
    //                            velocity step (discovered_min_pwm is final)
    uint8_t  pwm_disc_step_index_;
    uint16_t pwm_disc_cur_pwm_;
    uint32_t pwm_disc_step_end_ms_;
    int16_t  pwm_disc_prev_vel_dps_;
    uint8_t  pwm_disc_plateau_run_;
    bool     pwm_disc_min_locked_;
    PwmDiscoveryResult pwm_discovery_result_;
#endif

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
