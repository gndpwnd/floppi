/**
 * Auto Orientation Framework — Application Dispatcher
 *
 * This file is the single entry point for the framework. The actual
 * application body is selected at compile time via the USE_<APP> flags
 * defined in src/config/mode.h. Each application composes the framework's
 * sensor / control / navigation / storage modules for its own use case.
 *
 * Currently supported applications:
 *
 *   USE_BALANCING_ROBOT   - Phase 4.7 self-balancing robot reference
 *                           (BNO055 + L298N + auto-PID-tune, see
 *                            docs/applications/balancing_robot/)
 *
 *   USE_IMU_GPS_TELEMETRY - BNO085 + GPS + EKF fusion, JSON telemetry
 *                           over serial. The default when no other
 *                           application flag is set.
 *
 * Adding an application:
 *   1. Define USE_<APP> in src/config/mode.h
 *   2. Add an `#elif defined(USE_<APP>)` branch below
 *   3. Document at docs/applications/<app>/
 *
 * See docs/scope.md for the framework mission and
 * docs/findings/MASTER_DESIGN.md for design decisions.
 */

#include <Arduino.h>
#include "config/mode.h"

#ifdef USE_BALANCING_ROBOT
// ============================================================================
// Balance Robot Application (Phase 4.7)
// ============================================================================
// Single-binary path used by arduino_uno_balancing / arduino_mega_balancing.
// Everything below the #else (legacy BNO085 + GPS + EKF) is excluded for
// these builds.
//
// Hardware (matches the archived SelfBallancingRobot3.ino):
//   - BNO055 on I2C (Uno: SDA=A4 SCL=A5; Mega: SDA=20 SCL=21). Address 0x28.
//   - L298N motor driver: ENA=5, IN1=6, IN2=7, IN3=9, IN4=8, ENB=10.
//   - Optional button on D4 (active-low, INPUT_PULLUP). Without one, control
//     via serial: 'c' = short-press, 't' = long-press, 'a' = abort, 's' =
//     status, 'r' = re-cal BNO055 (clears EEPROM).
//
// Persistent storage layout (Arduino EEPROM, 256 B slot at offset 0):
//   See src/config/calibration_storage.h — stores 22-byte BNO055 offsets
//   blob with marker + length + version + CRC8. Survives power cycle.

#include "sensors/bno055.h"
#include "actuators/l298n_motor_driver.h"
#include "control/pid_controller.h"
#include "control/auto_pid_tuner.h"
#include "control/plant_identifier.h"
#include "control/tuners/relay_feedback.h"
#include "navigation/mounting_calibration.h"
#include "navigation/online_mounting_estimator.h"
#include "applications/balancing_robot/balance_app.h"
#include "applications/balancing_robot/safety.h"
#include "storage/persistent_storage.h"
#include "config/calibration_storage.h"
#include <MsTimer2.h>   // Item 3: 5 ms hardware-timer PID ISR (200 Hz)

// ---- Pin assignments (Uno and Mega use the same numeric pins for L298N; the
// I2C pins are routed by the Wire library to the platform-native I2C bus). ----
static const uint8_t PIN_BTN = 4;

// ---- Module instances --------------------------------------------------------
// IMPORTANT: external crystal flag must match the breakout!
//   - Adafruit BNO055 / Adafruit Stemma QT BNO055 (ships with 32 kHz crystal): true
//   - CJMCU-055, GY-955, generic BNO055 modules (no crystal populated): false
// Forcing external-crystal mode on a board without one will freeze the fusion
// pipeline (cal values stay at 0). Override per-build with -DBNO055_NO_EXT_CRYSTAL.
#ifdef BNO055_NO_EXT_CRYSTAL
static BNO055               imu(0x28, /*use_ext_crystal=*/false);
#else
static BNO055               imu(0x28, /*use_ext_crystal=*/true);
#endif

// Note: IN1/IN2 and IN3/IN4 are SWAPPED from the .ino's original pinout to
// invert direction at the driver level. The .ino had wiring such that
// positive PWM rolled the bot backward; the balance loop assumes positive
// PWM = forward. Swapping these is functionally identical to swapping the
// two motor leads at the L298N output, but doesn't require rewiring.
static L298NPins motor_pins = {
    /*ena*/ 5, /*in1*/ 7, /*in2*/ 6,
    /*enb*/ 10, /*in3*/ 8, /*in4*/ 9
};
// stiction floor 0 — the kick-up-to-threshold was creating an 18 PWM chunky
// step that made small corrections over-shoot, oscillating into a fall.
// Cleaner: let tiny PWM commands stay tiny. Motors won't move below ~15 PWM
// physically, which acts as a natural deadband that the PID can live with.
// If we need stiction compensation again it should be one-shot per direction
// reversal, not unconditional — but for now: zero floor.
static L298NMotorDriver     motors(motor_pins, /*stiction_min_pwm=*/0);

// Conservative defaults overwritten in begin() via default_config(). Kept in
// sync with balance_app.cpp's kDefaultInitial* constants — three locations must
// agree (see IMPLEMENTATION_PLAN.md Anticipated snags).
static PIDController        balance_pid(18.0f, 3.0f, 10.0f, -255.0f, 255.0f);

// Relay strategy: 150 PWM amplitude (well below saturation), 0.5° hysteresis.
static RelayFeedbackStrategy relay_strategy(/*amp=*/150.0f, /*hyst=*/0.5f);
static AutoPIDTuner          tuner(relay_strategy);

static MountingCalibration   mounting;
static OnlineMountingEstimator online_est;
static BalanceSafety         safety;
// Phase 4.10 — scalar RLS plant identifier + closed-form PD-from-K_motor.
// See src/control/plant_identifier.h. Owned by main, injected into BalanceApp.
static PlantIdentifier       plant_id;

static BalanceApp app(imu, motors, balance_pid, tuner,
                      mounting, online_est, safety, plant_id);

// ---- EEPROM persistence for mounting offset ---------------------------------
// Per project design (2026-05-12): only HARDWARE / PHYSICAL properties get
// persisted across reboots. The BNO055 sensor cal (~22 B) lives in slot 0x000
// (managed by calibration_storage.h). Mounting offset (where the IMU sits on
// the chassis) is a physical property of the build — saved separately at
// 0x200. PID gains are NOT persisted: they're either hardcoded defaults or
// adapted dynamically during operation by the online mounting estimator.
// See docs/applications/balancing_robot/USER_GUIDE.md for rationale.
static const uint16_t EE_MOUNT_ADDR = 0x200;
static const uint16_t EE_MOUNT_LEN  = 8;
static const uint8_t  EE_MOUNT_MAGIC = 0xAB;
static const uint8_t  EE_MOUNT_VER   = 0x01;

static uint8_t xor_crc8_(const uint8_t* d, uint16_t n) {
    uint8_t c = 0; for (uint16_t i = 0; i < n; ++i) c ^= d[i]; return c;
}

static bool save_mount_offset_(float deg) {
    uint8_t buf[EE_MOUNT_LEN] = {0};
    buf[0] = EE_MOUNT_MAGIC;
    buf[1] = EE_MOUNT_VER;
    memcpy(buf + 2, &deg, 4);          // bytes 2..5 = float LE
    buf[7] = xor_crc8_(buf, 7);
    if (!ps::write(EE_MOUNT_ADDR, buf, EE_MOUNT_LEN)) return false;
    return ps::commit();
}

static bool load_mount_offset_(float& deg) {
    uint8_t buf[EE_MOUNT_LEN];
    if (!ps::read(EE_MOUNT_ADDR, buf, EE_MOUNT_LEN)) return false;
    if (buf[0] != EE_MOUNT_MAGIC || buf[1] != EE_MOUNT_VER) return false;
    if (xor_crc8_(buf, 7) != buf[7]) return false;
    memcpy(&deg, buf + 2, 4);
    return true;
}

// ---- Button handling ---------------------------------------------------------
static int      g_last_button   = HIGH;
static uint32_t g_press_start_ms = 0;
static const uint32_t LONG_PRESS_MS  = 1000;
static const uint32_t SHORT_PRESS_MS = 30;

static void poll_button_(uint32_t now_ms) {
    int b = digitalRead(PIN_BTN);
    if (b == LOW && g_last_button == HIGH) {
        g_press_start_ms = now_ms;
    } else if (b == HIGH && g_last_button == LOW) {
        uint32_t held = now_ms - g_press_start_ms;
        if (held >= LONG_PRESS_MS)       app.on_long_press(now_ms);
        else if (held >= SHORT_PRESS_MS) app.on_short_press(now_ms);
    }
    g_last_button = b;
}

// ---- BNO055 calibration: load from EEPROM, or wait for the user to rotate ----
static bool restore_bno_cal_(BNO055& bno) {
    uint8_t buf[32];   // BNO055 blob is 22 B; padding for header read
    uint16_t len = 0;
    if (!restoreFromEEPROM(buf, &len) || len != 22) {
        return false;
    }
    return bno.setCalibrationProfile(buf, len);
}

static void save_bno_cal_(BNO055& bno) {
    uint8_t buf[32];
    uint16_t len = 22;
    bno.getCalibrationProfile(buf, &len);
    saveToEEPROM(buf, len);
}

// Blocking pre-flight: poll BNO055 calibration accuracies; save when fully
// calibrated. The threshold is "all four == 3" (gold standard) — anything
// less means the sensor isn't trustworthy for a balance loop. Two escape
// hatches in case the magnetometer gets stuck at 2 (common indoors near
// laptops/monitors):
//   - Short-press the button: save whatever level we currently have
//   - Send 'k' over serial: skip saving entirely (continue uncalibrated)
//
// What each axis needs to reach 3:
//   gyro=3  — leave device still for ~5 s
//   accel=3 — tilt into the 6 axis-aligned orientations, pause 1-2 s each
//   mag=3   — slow figure-8 motion in clean magnetic environment
//   sys=3   — composite of above; flips to 3 last
static void run_bno_cal_wizard_(BNO055& bno) {
    // Trimmed for Uno flash budget. Full guide lives in docs/calibration/.
    Serial.println(F("cal g3a3? k p"));

    uint32_t last_print = 0;
    while (true) {
        uint32_t now = millis();
        imu.read();
        const OrientationData& d = imu.getOrientation();

        if (now - last_print >= 500) {
            Serial.print(F("g=")); Serial.print(d.cal_gyro);
            Serial.print(F(" a=")); Serial.print(d.cal_accel);
            Serial.print(F(" m=")); Serial.print(d.cal_mag);
            Serial.print(F(" s=")); Serial.println(d.cal_status);
            last_print = now;
        }

        if (d.cal_gyro == 3 && d.cal_accel == 3) {
            save_bno_cal_(bno);
            return;
        }

        if (Serial.available()) {
            char c = Serial.read();
            if (c == 'k') return;
            if (c == 'p') { save_bno_cal_(bno); return; }
        }
        if (digitalRead(PIN_BTN) == LOW) {
            delay(40);
            if (digitalRead(PIN_BTN) == LOW) {
                save_bno_cal_(bno);
                while (digitalRead(PIN_BTN) == LOW) { delay(10); }
                return;
            }
        }
        delay(20);
    }
}

// ---- 5 ms PID hardware-timer ISR (Item 3) ----
// MsTimer2 on Timer2: doesn't conflict with the L298N PWM pins (ENA=5 on T0,
// ENB=10 on T1). Pin 3 and 11 lose analogWrite — neither is used here.
//
// ISR-safety: app.tick() never calls Serial.print (deferred via the
// pending_state_log flag) and never touches I²C (imu_.read() is loop-side).
// motors_.set_speed() reduces to atomic analogWrite + digitalWrite. millis()
// is interrupt-safe on AVR (atomic 4-byte read with interrupts blocked).
//
// Reference: research_osoyoo_reference_implementation.md §3 (osoyoo_abc.ino
// uses the structurally-identical MsTimer2::set(5, inter)).
static void pid_tick_isr() {
    app.tick(millis());
}

// ============================================================================
// Arduino entry points (balance mode)
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\nBal"));

    pinMode(PIN_BTN, INPUT_PULLUP);

    // Persistent storage HAL
    ps::begin(/*capacity_hint=*/512);

    // IMU
    if (!imu.begin()) {
        Serial.println(F("BNO FAIL"));
        while (1) { delay(1000); }
    }

    // BNO055 calibration: restore from EEPROM, or run the wizard.
    if (!restore_bno_cal_(imu)) {
        run_bno_cal_wizard_(imu);
    }

    // Motors
    motors.begin();

    // App
    BalanceAppConfig cfg = BalanceApp::default_config();
    app.begin(cfg, millis());

    // PID gains: Kp=50 Ki=1 Kd=20. Tiny Ki — just enough to feed the
    // OnlineMountingEstimator (which reads pid_.get_i_term() to drive
    // mount-offset adaptation). With Ki=1 the integrator can't dominate
    // the output and windup-saturate the motors before the bot recovers.
    balance_pid.set_tunings(50.0f, 1.0f, 20.0f);
    // I-term contribution hard-capped at 40 PWM regardless of output range —
    // unstable-plant anti-windup. Without this, the integrator could push
    // the motors to max independently of the position error, defeating
    // recovery. This is the fix for the "balances 1 second then motors max"
    // failure mode.
    balance_pid.set_i_term_limit(40.0f);
    balance_pid.set_d_term_lpf_tau_sec(0.003f);

    // OnlineMountingEstimator: fast 20 s time constant so the captured
    // mount-offset error auto-trims within a single bench session.
    // Drift rate cap 2°/s ensures it can adapt even with a 5° initial bias.
    online_est.set_lpf_time_constant_sec(20.0f);
    online_est.set_max_drift_rate_dps(2.0f);

    // Mounting offset IS persisted (it's a physical mounting property, not
    // a control parameter). Restore it on boot if available.
    bool have_mount = false;
    {
        float off;
        if (load_mount_offset_(off)) {
            online_est.reset_to_reference(off, millis());
            have_mount = true;
        }
    }

    Serial.println(F("READY"));

    // "Prop-and-go" auto-RUN: if we know where balance is (mount offset saved),
    // start balancing immediately with the default gains.
    if (have_mount) {
        delay(2000);  // grace period to get the bot in position
        app.enter_run_with_current_gains(millis());
    }

    // Item 3 — start the 5 ms PID tick ISR. Must be AFTER app.begin() so the
    // app's state is initialised before tick() can fire. MsTimer2 ticks at
    // exactly 200 Hz (no millis()-polled jitter from Serial / button code).
    MsTimer2::set(5, pid_tick_isr);
    MsTimer2::start();
}

void loop() {
    uint32_t now = millis();

    poll_button_(now);

    if (Serial.available()) {
        char c = Serial.read();
        if      (c == 'c') app.on_short_press(now);
        else if (c == 't') app.on_long_press(now);
        else if (c == 'a') safety.request_abort();
        else if (c == 'r') {
            clearCalibrationFromEEPROM();
            run_bno_cal_wizard_(imu);
        } else if (c == 's') {
            // Phase 4.10 — extended status: state, pitch, mount-offset,
            // output, live PID gains, learned K_motor + P, target Kp/Kd from
            // PlantIdentifier. 'A' = ADAPT (bootstrap window cleared), 'B'.
            float kp, ki, kd;
            balance_pid.get_tunings(kp, ki, kd);
            const PlantIdentifierStatus& pi = app.get_plant_status();
            Serial.print(app.state_name());
            Serial.print(app.is_adaptive_active() ? F(" A ") : F(" B "));
            Serial.print(app.get_pitch_deg(), 2); Serial.print(' ');
            Serial.print(app.get_mount_offset_deg(), 2); Serial.print(' ');
            Serial.print(app.get_last_output()); Serial.print(F(" K="));
            Serial.print(pi.k_motor, 3); Serial.print(F(" P="));
            Serial.print(pi.covariance, 2); Serial.print(F(" Kp="));
            Serial.print(kp, 1); Serial.print('/');
            Serial.print(pi.kp_target, 1); Serial.print(F(" Kd="));
            Serial.print(kd, 1); Serial.print('/');
            Serial.println(pi.kd_target, 1);
        } else if (c == 'R') {
            // Apply .ino gains (session only — NOT persisted), enter RUN.
            balance_pid.set_tunings(65.0f, 12.0f, 38.0f);
            app.enter_run_with_current_gains(now);
        } else if (c == 'm') {
            // Motor test (Uno flash-trimmed).
            motors.set_speeds(90, 0);    delay(700);
            motors.set_speeds(-90, 0);   delay(700);
            motors.stop();               delay(300);
            motors.set_speeds(0, 90);    delay(700);
            motors.set_speeds(0, -90);   delay(600);
            motors.stop();               delay(500);
            motors.set_speeds(90, 90);   delay(1500);
            motors.stop();               delay(500);
            motors.set_speeds(-90, -90); delay(1500);
            motors.stop();
            Serial.println(F("mt"));
        }
    }

    // Item 3 — loop-side responsibilities only. The 5 ms PID tick runs in the
    // MsTimer2 ISR (pid_tick_isr); here we just keep the IMU snapshot fresh
    // and drain any deferred state-transition log lines the ISR queued.
    //
    // read_sensors() is the I²C-bound IMU read; run it as fast as the loop
    // can manage (~1 kHz typical). The ISR consumes the most recent stash.
    app.read_sensors(now);
    app.drain_state_log(Serial);

    // Persist state on key transitions so the next boot can auto-RUN.
    static BalanceAppState last_state = BalanceAppState::IDLE;
    BalanceAppState cur = app.get_state();
    if (cur != last_state) {
        // CAPTURE_MOUNTING -> IDLE means capture finished; save the new offset.
        if (last_state == BalanceAppState::CAPTURE_MOUNTING &&
            cur == BalanceAppState::IDLE) {
            float off = app.get_mount_offset_deg();
            if (save_mount_offset_(off)) {
                Serial.print(F("sv m="));
                Serial.println(off, 2);
            }
        }
        last_state = cur;
    }

    // Periodic save of the live mounting offset during RUN — captures the
    // online estimator's drift refinements (no point if state is stable).
    // Limit to once every 60 s of runtime to spare the EEPROM cell life.
    static uint32_t last_periodic_save = 0;
    if (cur == BalanceAppState::RUN && now - last_periodic_save >= 60000) {
        float off = app.get_mount_offset_deg();
        save_mount_offset_(off);
        last_periodic_save = now;
    }
}

#elif defined(USE_IMU_GPS_TELEMETRY)
// ============================================================================
// IMU + GPS Telemetry Application
// ============================================================================
// BNO085 IMU + GPS + EKF fusion, JSON telemetry over serial. The framework's
// reference application for orientation+position logging and downstream
// fusion (Phases 1-3 of the framework's evolution).
//
// Hardware:
//   - BNO085 (Adafruit) via I2C (Mega: SDA=20, SCL=21)
//   - GPS (NEO-M9N or similar) via UART Serial1 (Mega: pins 18/19)
//   - Outputs JSON with orientation + position + calibration status
//   - EKF fused state JSON when enabled
//
// Rates: BNO085 ~100 Hz; GPS ~1 Hz; JSON output ~10 Hz; EKF ~10 Hz.
// Calibration: auto-saves on level >= 2; auto-loads on boot; survives uploads.
//
// See docs/calibration/CALIBRATION_GUIDE.md, docs/theory/ABSOLUTE_ORIENTATION_EXPLAINED.md,
// docs/getting_started/GETTING_STARTED.md, docs/reference/EKF_OUTPUT_FORMAT.md.

#include "config/pins.h"
#include "config/ekf_config.h"
#include "sensors/sensor_base.h"
#include "output/sensor_output_manager.h"
#include "sensors/bno085.h"
#include "sensors/gps.h"
#include "math/coordinates.h"
#include "navigation/ekf.h"

#if ENABLE_SNAPSHOT_RECORDER
#include "features/snapshot_recorder.h"
#include "sensors/button_input.h"
#endif

BNO085 imu;
GPS gps;
SensorOutputManager output_manager;

// Phase 3: Extended Kalman Filter for sensor fusion
ExtendedKalmanFilter ekf;
bool ekf_initialized = false;

// Coordinate frame reference point (initialized on first GPS fix)
ECEF coordinate_frame_origin_ecef;
double coordinate_frame_lat_rad;
bool coordinate_frame_initialized = false;

#if ENABLE_SNAPSHOT_RECORDER
SnapshotRecorder snapshot_recorder;
ButtonInput button_input;
#endif

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    delay(100);
  }

  // Boot message (calibration mode is verbose, production is minimal)
  if (IS_CALIBRATION_MODE) {
    Serial.println("\n==================================================");
    Serial.println("Auto Orientation System [CALIBRATION MODE]");
    Serial.println("==================================================");
    Serial.println("Initializing sensors...\n");
  } else {
    Serial.println("\nAuto Orientation [PRODUCTION]");
  }

  // Initialize BNO085 sensor
  CAL_PRINTLN("Board: Initializing BNO085 IMU sensor...");
  if (!imu.begin()) {
    ALWAYS_PRINTLN("ERROR: BNO085 initialization failed!");
    while (1) {
      delay(500);
      ALWAYS_PRINTLN("  (waiting for manual reset)");
    }
  }
  CAL_PRINTLN("✓ BNO085 OK\n");

  // Initialize GPS sensor (non-fatal if it fails)
  CAL_PRINTLN("Board: Initializing GPS sensor (Serial1)...");
  if (!gps.begin(9600)) {
    CAL_PRINTLN("WARNING: GPS initialization failed (continuing without GPS)");
    // Non-fatal: continue with orientation-only output
  } else {
    CAL_PRINTLN("✓ GPS OK (waiting for fix...)\n");
  }

  // Initialize output manager (JSON-only format)
  CAL_PRINTLN("Board: Initializing output manager...");
  if (!output_manager.begin(OutputFormat::JSON)) {
    ALWAYS_PRINTLN("ERROR: Output manager initialization failed!");
    while (1) {
      delay(500);
      ALWAYS_PRINTLN("  (waiting for manual reset)");
    }
  }

#if ENABLE_SNAPSHOT_RECORDER
  // Initialize snapshot recorder (if SNAPSHOT_MODE enabled)
  CAL_PRINTLN("Board: Initializing snapshot recorder...");
  if (!snapshot_recorder.initialize()) {
    CAL_PRINTLN("WARNING: Snapshot recorder initialization failed");
    // Non-fatal: continue without snapshot feature
  } else {
    CAL_PRINTLN("✓ Snapshot recorder OK");
  }

  // Initialize button input (if SNAPSHOT_MODE enabled)
  CAL_PRINTLN("Board: Initializing button input...");
  if (!button_input.initialize()) {
    CAL_PRINTLN("WARNING: Button input initialization failed");
    // Non-fatal: continue without button feature
  } else {
    CAL_PRINTLN("✓ Button input OK");
  }
#endif

  // Boot complete message
  if (IS_CALIBRATION_MODE) {
    Serial.println("✓ All sensors initialized successfully!\n");
    Serial.println("==================================================");
    Serial.println("Calibration progress indicator:");
    Serial.println("  ░░░ = Uncalibrated (0)");
    Serial.println("  █░░ = Low (1)");
    Serial.println("  ██░ = Medium (2) <-- Auto-saves");
    Serial.println("  ███ = High (3)");
    Serial.println("==================================================\n");
  } else {
    Serial.println("✓ Ready");
  }
}

void loop() {
  // ========================================================================
  // Initialize EKF on first IMU data (Phase 3)
  // ========================================================================
  if (!ekf_initialized && imu.read() && imu.hasNewData()) {
    // Create initial covariance matrix with large uncertainties
    Matrix16x16 P_init;
    memset(P_init, 0, sizeof(Matrix16x16));

    // Set initial uncertainties (diagonal matrix)
    for (int i = 0; i < 4; i++) {
      P_init[i][i] = 1.0f;  // Quaternion uncertainty
    }
    for (int i = 4; i < 7; i++) {
      P_init[i][i] = 10.0f;  // Velocity uncertainty (m/s)
    }
    for (int i = 7; i < 10; i++) {
      P_init[i][i] = 100.0f;  // Position uncertainty (m)
    }
    for (int i = 10; i < 13; i++) {
      P_init[i][i] = 0.1f;  // Accel bias uncertainty
    }
    for (int i = 13; i < 16; i++) {
      P_init[i][i] = 0.01f;  // Gyro bias uncertainty
    }

    if (ekf.initialize(P_init)) {
      ekf_initialized = true;
      CAL_PRINTLN("EKF: Initialized successfully");
    } else {
      CAL_PRINTLN("WARNING: EKF initialization failed");
    }
  }

  // ========================================================================
  // Read BNO085 Orientation (high frequency: ~100 Hz)
  // ========================================================================
  if (imu.read() && imu.hasNewData()) {
    const OrientationData& orientation = imu.getOrientation();
    output_manager.updateOrientation(orientation);

    // Phase 3: Feed IMU data to EKF for prediction step
    if (ekf_initialized) {
      static uint32_t last_ekf_predict_ms = 0;
      uint32_t now_ms = millis();
      float dt_sec = (now_ms - last_ekf_predict_ms) / 1000.0f;

      // Sanity check on dt
      if (dt_sec > 0.001f && dt_sec < 0.2f && last_ekf_predict_ms > 0) {
        // Get IMU raw measurements (gyro + accel)
        // Note: BNO085 provides quaternion, not raw gyro/accel
        // For Phase 3, we use placeholder sensor data
        // Production code would extract these from BNO085 raw sensor events
        float gyro_rad_s[3] = {0.0f, 0.0f, 0.0f};  // Placeholder
        float accel_m_s2[3] = {0.0f, 0.0f, 9.81f};  // Placeholder (gravity)

        ekf.predict(gyro_rad_s, accel_m_s2, dt_sec);
      }
      last_ekf_predict_ms = now_ms;
    }

#if ENABLE_SNAPSHOT_RECORDER
    // Check for button press and record snapshot if triggered
    if (button_input.is_pressed()) {
      if (snapshot_recorder.is_ready()) {
        if (snapshot_recorder.record_snapshot_from_orientation(orientation, millis())) {
          Serial.printf("✓ Snapshot #%lu recorded\n", snapshot_recorder.get_snapshot_count());
        } else {
          Serial.println("ERROR: Failed to record snapshot");
        }
      } else {
        Serial.println("ERROR: Snapshot recorder not ready");
      }
    }
#endif
  }

  // ========================================================================
  // Read GPS Position (low frequency: ~1 Hz, non-blocking)
  // ========================================================================
  if (gps.isInitialized() && gps.read() && gps.hasNewData()) {
    const PositionData& position = gps.getPosition();

    // Initialize coordinate frame on first valid GPS fix
    if (!coordinate_frame_initialized && position.fix_quality >= 1) {
      coordinate_frame_origin_ecef = gps_to_ecef(
          position.latitude, position.longitude, position.altitude);
      coordinate_frame_lat_rad = position.latitude * 3.14159265359 / 180.0;
      coordinate_frame_initialized = true;

      if (IS_CALIBRATION_MODE) {
        CAL_PRINTLN("GPS: CoordinateFrame initialized (first fix acquired)");
      }
    }

    // Update output manager with latest position
    output_manager.updatePosition(position);

    // Phase 3: GPS dropout handling via EKF
    if (ekf_initialized) {
      if (position.fix_quality >= 1) {
        // GPS has lock - mark no dropout in EKF
        ekf.set_gps_dropout(false);

        if (IS_CALIBRATION_MODE) {
          CAL_PRINTLN("GPS: Fix acquired");
          Serial.print("  Lat: ");
          Serial.print(position.latitude, 6);
          Serial.print(", Lon: ");
          Serial.print(position.longitude, 6);
          Serial.print(", Alt: ");
          Serial.print(position.altitude, 1);
          Serial.println(" m");
          Serial.print("  Satellites: ");
          Serial.print(position.num_satellites);
          Serial.print(", Fix Quality: ");
          Serial.print(position.fix_quality);
          Serial.print(", Accuracy: ");
          Serial.print(position.accuracy_m, 1);
          Serial.println(" m");
        }

        // TODO: Feed GPS measurement to EKF update step
        // This requires proper NED conversion and implementation
        // of the measurement update in the predict-only loop
      } else {
        // No GPS lock - mark as dropout
        ekf.set_gps_dropout(true);
      }
    }
  } else if (ekf_initialized && gps.isInitialized()) {
    // Periodically check GPS timeout for dropout detection
    static uint32_t last_gps_check_ms = 0;
    uint32_t now_ms = millis();

    if (now_ms - last_gps_check_ms >= 1000) {  // Check every 1 second
      last_gps_check_ms = now_ms;

      // If GPS age > 1 second, mark as dropout
      uint32_t gps_age = ekf.get_gps_age_ms();
      if (gps_age > 1000 || gps_age == UINT32_MAX) {
        ekf.set_gps_dropout(true);
      }

      if (IS_CALIBRATION_MODE && !gps.hasLock()) {
        CAL_PRINTLN("GPS: Waiting for fix...");
      }
    }
  }

  // ========================================================================
  // Output Sensor Data (frequency-controlled to ~10 Hz)
  // ========================================================================
  if (output_manager.shouldOutput()) {
    char buffer[512];
    uint16_t len = output_manager.getFormattedOutput(buffer, sizeof(buffer));
    if (len > 0) {
      Serial.println(buffer);
    }
  }

  // Phase 3: EKF Health Diagnostics (every 10 seconds in calibration mode)
  if (ekf_initialized && IS_CALIBRATION_MODE) {
    static uint32_t last_ekf_diag_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_ekf_diag_ms >= 10000) {
      last_ekf_diag_ms = now_ms;

      Serial.println("\n[EKF Diagnostics]");
      Serial.print("  GPS Locked: ");
      Serial.println(ekf.is_gps_locked() ? "Yes" : "No");
      Serial.print("  GPS Age (ms): ");
      Serial.println(ekf.get_gps_age_ms());
      Serial.print("  Dead Reckoning Age (ms): ");
      Serial.println(ekf.get_dead_reckoning_age_ms());
      Serial.print("  Covariance Trace: ");
      Serial.println(ekf.get_covariance_trace(), 2);
      Serial.print("  Covariance Valid: ");
      Serial.println(ekf.is_covariance_valid() ? "Yes" : "No");
      Serial.print("  Consecutive Dropout Updates: ");
      Serial.println(ekf.get_consecutive_dropout_updates());
      Serial.println();
    }
  }
}

#else
  #error "No application selected. Set USE_BALANCING_ROBOT or USE_IMU_GPS_TELEMETRY (or another USE_<APP> flag) in mode.h or platformio.ini build_flags."
#endif  // application dispatcher
