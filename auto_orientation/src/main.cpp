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
#include <math.h>     // isnan / isinf — EEPROM float-load finiteness guards
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
#include "control/tuning_strategy.h"
#include "navigation/mounting_calibration.h"
#include "navigation/online_mounting_estimator.h"
#include "applications/balancing_robot/balance_app.h"
#include "applications/balancing_robot/safety.h"
#include "storage/persistent_storage.h"
#include "config/calibration_storage.h"
#ifdef USE_WHEEL_ENCODERS
#include "sensors/wheel_encoder.h"   // Phase 4M.10 — quadrature wheel encoders
#endif
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
static L298NMotorDriver     motors(motor_pins, /*stiction_min_pwm=*/80);

// PID gains are SET BY BOOTSTRAP (Phase 4.10c), not at construction. These
// constructor args only matter if BOOTSTRAP fails — in which case the bot stays
// in IDLE and never tries to balance, so any non-negative gains are safe.
// Output limits are real (±255 = hardware PWM range).
static PIDController        balance_pid(0.0f, 0.0f, 0.0f, -255.0f, 255.0f);

// AUTO_TUNE state retained in the enum for API stability, but is no longer
// wired to any operator gesture — long-press / 'b' / capture-success all
// chain to BOOTSTRAP (Phase 4.10c) instead. The injected tuning strategy is
// therefore a no-op that immediately reports failure; it satisfies the
// BalanceApp constructor without dragging in RelayFeedbackStrategy's 1.3 KB
// of relay-feedback machinery. Removed from the build_src_filter in
// platformio.ini (see -<control/tuners/relay_feedback.cpp>).
class NoOpStrategy : public ITuningStrategy {
public:
    void begin(float, float, float, uint32_t) override {}
    float step(float, const SafetyLimits&, uint32_t) override { return 0.0f; }
    bool is_done() const override { return true; }
    TuningResult get_result() const override {
        TuningResult r{}; r.failure_reason = "no_op_strategy"; return r;
    }
    const char* name() const override { return "noop"; }
};
static NoOpStrategy          noop_strategy;
static AutoPIDTuner          tuner(noop_strategy);

static MountingCalibration   mounting;
static OnlineMountingEstimator online_est;
static BalanceSafety         safety;
// Phase 4.10 — scalar RLS plant identifier + closed-form PD-from-K_motor.
// See src/control/plant_identifier.h. Owned by main, injected into BalanceApp.
static PlantIdentifier       plant_id;

static BalanceApp app(imu, motors, balance_pid, tuner,
                      mounting, online_est, safety, plant_id);

#ifdef USE_WHEEL_ENCODERS
// Phase 4M.10/4M.11 — quadrature wheel encoders. Pins per RWE §2:
// left  A/B = Mega 18/19 (INT5/INT4); right A/B = 2/3 (INT0/INT1).
// Pins 20/21 are I²C → BNO055 and MUST NOT be repurposed. begin() attaches
// the ISRs via the PJRC Encoder backend. Mega-only (Uno has 2 INT pins).
static WheelEncoder enc_left(/*pin_a=*/18, /*pin_b=*/19);
static WheelEncoder enc_right(/*pin_a=*/2,  /*pin_b=*/3);
#endif

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
static const uint8_t  EE_MOUNT_VER   = 0x02;   // 0x02: CRC-8-CCITT (was XOR-sum)

// Phase 2 — actuator characterisation record (stiction floor + future fields).
// Layout: [magic 0xAC][ver 0x02][stiction_pwm][reserved][crc] (8 bytes).
static const uint16_t EE_ACT_ADDR  = 0x210;
static const uint16_t EE_ACT_LEN   = 8;
static const uint8_t  EE_ACT_MAGIC = 0xAC;
static const uint8_t  EE_ACT_VER   = 0x02;   // 0x02: CRC-8-CCITT (was XOR-sum)

#ifdef USE_WHEEL_ENCODERS
// Phase 4M.11 — wheel-encoder calibration record (RWE §5). Slot 0x220, 16 B:
//   [magic 0xAD][ver 0x01][cpm_left f32 LE][cpm_right f32 LE]
//   [radius_m f32 LE][reserved 1 B][crc8]
// cpm_* = encoder counts per metre of wheel-tread travel (measured by the
// `e` command's roll-a-known-distance procedure). radius_m = derived wheel
// radius. CRC-8-CCITT over bytes 0..14 (project standard, see
// calibration_storage.cpp::calculateCRC8). Mega-only. 0x220 sits between the
// actuator slot (0x210) and the PWM-discovery slot (0x230) — no overlap.
static const uint16_t EE_ENC_ADDR  = 0x220;
static const uint16_t EE_ENC_LEN   = 16;
static const uint8_t  EE_ENC_MAGIC = 0xAD;
static const uint8_t  EE_ENC_VER   = 0x01;

// Phase 4M.12 — PWM range auto-discovery record. Layout:
//   [magic 0xAD][ver 0x02][min_lo][min_hi][max_lo][max_hi][reserved][crc] (8 B).
// Mega-only. 0x220 is reserved by the sibling encoder-cal agent (CPR + radius);
// we land at 0x230 so the two slots don't collide. Calibration blob headroom:
// the cal blob runs 0x000..0x100 (256 B reserved), mount lives at 0x200,
// actuator at 0x210, encoder-cal at 0x220, PWM-discovery at 0x230 — leaves
// 4096 - 0x238 = 3528 bytes free on Mega EEPROM.
static const uint16_t EE_PWMDISC_ADDR  = 0x230;
static const uint16_t EE_PWMDISC_LEN   = 8;
static const uint8_t  EE_PWMDISC_MAGIC = 0xAD;
static const uint8_t  EE_PWMDISC_VER   = 0x02;   // 0x02: CRC-8-CCITT (was XOR-sum)
#endif

// EEPROM slot integrity uses CRC-8-CCITT (calculateCRC8 from
// calibration_storage.h) project-wide. The mount/actuator/PWM-discovery slots
// were upgraded from a weak XOR-sum to CRC-8-CCITT on 2026-05-20 (security
// finding NEW-P1-001): two compensating bit-flips could pass the XOR-sum. Each
// upgraded slot's version byte was bumped 0x01 -> 0x02 so pre-upgrade records
// are rejected by the load path and the operator is forced to re-calibrate.
static bool save_mount_offset_(float deg) {
    uint8_t buf[EE_MOUNT_LEN] = {0};
    buf[0] = EE_MOUNT_MAGIC;
    buf[1] = EE_MOUNT_VER;
    memcpy(buf + 2, &deg, 4);          // bytes 2..5 = float LE
    buf[7] = calculateCRC8(buf, 7);
    if (!ps::write(EE_MOUNT_ADDR, buf, EE_MOUNT_LEN)) return false;
    return ps::commit();
}

static bool load_mount_offset_(float& deg) {
    uint8_t buf[EE_MOUNT_LEN];
    if (!ps::read(EE_MOUNT_ADDR, buf, EE_MOUNT_LEN)) return false;
    if (buf[0] != EE_MOUNT_MAGIC || buf[1] != EE_MOUNT_VER) return false;
    if (calculateCRC8(buf, 7) != buf[7]) return false;
    float v;
    memcpy(&v, buf + 2, 4);
    // Audit P1-002: this was the only EEPROM-loaded float without a NaN/range
    // guard (encoder-radius and PWM-disc paths ARE guarded). A CRC-valid record
    // whose payload is NaN/Inf (partial-flash, coincident corruption) would set
    // reference_deg_ = NaN downstream, disabling the OnlineMountingEstimator
    // clamp (NaN bounds pass everything) and poisoning corrected_pitch_(). A
    // mounting offset outside ±90° is physically nonsensical, so reject it and
    // fall back to the safe default (caller treats false as "no saved mount").
    if (isnan(v) || isinf(v) || v < -90.0f || v > 90.0f) return false;
    deg = v;
    return true;
}

static bool save_actuator_(uint8_t stiction) {
    uint8_t buf[EE_ACT_LEN] = {0};
    buf[0] = EE_ACT_MAGIC; buf[1] = EE_ACT_VER; buf[2] = stiction;
    buf[7] = calculateCRC8(buf, 7);
    if (!ps::write(EE_ACT_ADDR, buf, EE_ACT_LEN)) return false;
    return ps::commit();
}

static bool load_actuator_(uint8_t& stiction) {
    uint8_t buf[EE_ACT_LEN];
    if (!ps::read(EE_ACT_ADDR, buf, EE_ACT_LEN)) return false;
    if (buf[0] != EE_ACT_MAGIC || buf[1] != EE_ACT_VER) return false;
    if (calculateCRC8(buf, 7) != buf[7]) return false;
    stiction = buf[2];
    return true;
}

#ifdef USE_WHEEL_ENCODERS
// Phase 4M.11 — persist wheel-encoder calibration (CPM L/R + wheel radius) to
// EEPROM slot 0x220. CRC-8-CCITT over the first 15 bytes (project standard).
static bool save_encoder_cal_(float cpm_left, float cpm_right, float radius_m) {
    uint8_t buf[EE_ENC_LEN] = {0};
    buf[0] = EE_ENC_MAGIC;
    buf[1] = EE_ENC_VER;
    memcpy(buf + 2,  &cpm_left,  4);   // bytes 2..5
    memcpy(buf + 6,  &cpm_right, 4);   // bytes 6..9
    memcpy(buf + 10, &radius_m,  4);   // bytes 10..13
    // byte 14 reserved (zero)
    buf[15] = calculateCRC8(buf, 15);
    if (!ps::write(EE_ENC_ADDR, buf, EE_ENC_LEN)) return false;
    return ps::commit();
}

static bool load_encoder_cal_(float& cpm_left, float& cpm_right,
                              float& radius_m) {
    uint8_t buf[EE_ENC_LEN];
    if (!ps::read(EE_ENC_ADDR, buf, EE_ENC_LEN)) return false;
    if (buf[0] != EE_ENC_MAGIC || buf[1] != EE_ENC_VER) return false;
    if (calculateCRC8(buf, 15) != buf[15]) return false;
    memcpy(&cpm_left,  buf + 2,  4);
    memcpy(&cpm_right, buf + 6,  4);
    memcpy(&radius_m,  buf + 10, 4);
    return true;
}

static bool save_pwm_discovery_(uint16_t min_pwm, uint16_t max_pwm) {
    uint8_t buf[EE_PWMDISC_LEN] = {0};
    buf[0] = EE_PWMDISC_MAGIC;
    buf[1] = EE_PWMDISC_VER;
    buf[2] = (uint8_t)(min_pwm & 0xFF);
    buf[3] = (uint8_t)((min_pwm >> 8) & 0xFF);
    buf[4] = (uint8_t)(max_pwm & 0xFF);
    buf[5] = (uint8_t)((max_pwm >> 8) & 0xFF);
    buf[7] = calculateCRC8(buf, 7);
    if (!ps::write(EE_PWMDISC_ADDR, buf, EE_PWMDISC_LEN)) return false;
    return ps::commit();
}

static bool load_pwm_discovery_(uint16_t& min_pwm, uint16_t& max_pwm) {
    uint8_t buf[EE_PWMDISC_LEN];
    if (!ps::read(EE_PWMDISC_ADDR, buf, EE_PWMDISC_LEN)) return false;
    if (buf[0] != EE_PWMDISC_MAGIC || buf[1] != EE_PWMDISC_VER) return false;
    if (calculateCRC8(buf, 7) != buf[7]) return false;
    min_pwm = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
    max_pwm = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    return true;
}
#endif

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

#ifdef USE_WHEEL_ENCODERS
// Phase 4M.11 — wheel-encoder calibration wizard (RWE §5).
//
// Operator workflow:
//   1. Send `e`. Firmware zeroes both encoders and prints a prompt.
//   2. Operator picks the bot up, sets it on a marked start line, and rolls
//      it manually exactly ENC_CAL_DISTANCE_M (1.000 m) along a tape measure.
//      Live `L_ticks R_ticks` stream once per second lets the operator verify
//      both wheels are counting and counting the right sign.
//   3. Operator presses the button. Firmware reads final ticks, computes
//      counts-per-metre (CPM) per wheel and a derived wheel radius, applies
//      the values to the live encoder objects, and saves them to EEPROM 0x220.
//
// Blocking by design — manual rolling is a one-time bench setup step, and the
// MsTimer2 PID ISR is not running yet at the only other call site (it is
// loop-side here, but the bot sits in IDLE during cal so motors are stopped).
//
// CPM→radius: one wheel revolution is cpr_ counts and covers 2π·r metres, so
//   r = cpr / (CPM · 2π).  Averages the two wheels' CPM for a single radius.
static const float ENC_CAL_DISTANCE_M = 1.000f;

static void run_encoder_cal_(void) {
    enc_left.reset_ticks();
    enc_right.reset_ticks();
    Serial.println(F("enc_cal: roll bot 1.000 m, press btn"));

    uint32_t last_print = 0;
    while (true) {
        uint32_t now = millis();
        if (now - last_print >= 1000) {
            Serial.print(F("L_ticks="));
            Serial.print(enc_left.read_ticks());
            Serial.print(F(" R_ticks="));
            Serial.println(enc_right.read_ticks());
            last_print = now;
        }
        // Escape hatch: `a` aborts the cal without writing EEPROM.
        if (Serial.available()) {
            if (Serial.read() == 'a') {
                Serial.println(F("enc_cal: abort"));
                return;
            }
        }
        if (digitalRead(PIN_BTN) == LOW) {
            delay(40);
            if (digitalRead(PIN_BTN) == LOW) {
                while (digitalRead(PIN_BTN) == LOW) { delay(10); }
                break;
            }
        }
        delay(20);
    }

    const int32_t tl = enc_left.read_ticks();
    const int32_t tr = enc_right.read_ticks();
    // CPM uses |ticks| so a swapped-A/B wheel still calibrates; direction sign
    // is a wiring concern surfaced by the live stream above.
    const int32_t al = tl < 0 ? -tl : tl;
    const int32_t ar = tr < 0 ? -tr : tr;
    if (al == 0 || ar == 0) {
        Serial.println(F("enc_cal: no ticks - not saved"));
        return;
    }
    const float cpm_left  = (float)al / ENC_CAL_DISTANCE_M;
    const float cpm_right = (float)ar / ENC_CAL_DISTANCE_M;
    // Derived wheel radius from the averaged CPM (RWE §5).
    const float cpm_avg   = 0.5f * (cpm_left + cpm_right);
    const float radius_m  =
        (float)enc_left.counts_per_rev() / (cpm_avg * 2.0f * 3.14159265f);

    enc_left.set_counts_per_rev(enc_left.counts_per_rev());
    enc_left.set_wheel_radius_m(radius_m);
    enc_right.set_wheel_radius_m(radius_m);

    Serial.print(F("enc_cal: L="));
    Serial.print(cpm_left, 1);
    Serial.print(F(" R="));
    Serial.print(cpm_right, 1);
    Serial.print(F(" r="));
    Serial.print(radius_m, 4);
    if (save_encoder_cal_(cpm_left, cpm_right, radius_m)) {
        Serial.println(F(" saved"));
    } else {
        Serial.println(F(" SAVE FAIL"));
    }
}
#endif  // USE_WHEEL_ENCODERS

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
    Serial.println(F("\nB"));

    pinMode(PIN_BTN, INPUT_PULLUP);

    // Persistent storage HAL
    ps::begin(/*capacity_hint=*/512);

    // IMU
    if (!imu.begin()) {
        Serial.println(F("BF"));
        while (1) { delay(1000); }
    }

    // BNO055 calibration: restore from EEPROM, or run the wizard.
    {
        uint8_t cb[32]; uint16_t cl = 0;
        if (!restoreFromEEPROM(cb, sizeof(cb), &cl) || cl != 22 ||
            !imu.setCalibrationProfile(cb, cl)) {
            run_bno_cal_wizard_(imu);
        }
    }

    // Motors
    motors.begin();

    // Phase 2 — apply saved actuator characterisation if present. Replaces
    // hardcoded stiction floor with measured value. If no record exists, the
    // operator should run `k` to characterise this bot.
    {
        uint8_t st;
        if (load_actuator_(st) && st >= 20 && st <= 200) {
            motors.set_stiction_min_pwm(st);
        }
    }

#ifdef USE_WHEEL_ENCODERS
    // Phase 4M.10/4M.11 — bring up the quadrature encoders and restore the
    // wheel-encoder calibration (CPM L/R + radius) from EEPROM slot 0x220.
    // If no valid record exists the encoders keep their kDefault* values and
    // the operator should run the `e` command to calibrate this bot.
    enc_left.begin();
    enc_right.begin();
    {
        float cpm_l = 0.0f, cpm_r = 0.0f, radius = 0.0f;
        if (load_encoder_cal_(cpm_l, cpm_r, radius) &&
            cpm_l > 0.0f && cpm_r > 0.0f &&
            radius > 0.0f && radius < 1.0f) {
            enc_left.set_wheel_radius_m(radius);
            enc_right.set_wheel_radius_m(radius);
            Serial.print(F("ld enc L="));
            Serial.print(cpm_l, 1);
            Serial.print(F(" R="));
            Serial.print(cpm_r, 1);
            Serial.print(F(" r="));
            Serial.println(radius, 4);
        }
    }

    // Phase 4M.12 — apply saved PWM-discovery bounds if present. The
    // discovered MIN_PWM is the encoder-confirmed stiction floor and is
    // strictly more trustworthy than the CHARACTERISE (`k`) gyro-based
    // estimate (encoders measure actual wheel motion, not chassis rotation),
    // so it overrides the actuator-slot value if both exist.
    //
    // MAX_PWM isn't applied to any in-firmware bound yet — the Python brute-
    // force tuner is the consumer (it reads the EEPROM blob over serial /
    // dashboard and uses the bounds to seed its search space). Saved so the
    // tuner can consume on the next iteration without re-running discovery.
    {
        uint16_t min_pwm = 0, max_pwm = 0;
        if (load_pwm_discovery_(min_pwm, max_pwm) &&
            min_pwm >= PWM_DISC_STEP_PWM && min_pwm <= 200 &&
            max_pwm > min_pwm && max_pwm <= 255) {
            motors.set_stiction_min_pwm((uint8_t)min_pwm);
            Serial.print(F("ld pd min="));
            Serial.print(min_pwm);
            Serial.print(F(" max="));
            Serial.println(max_pwm);
        }
    }
#endif

    // App
    BalanceAppConfig cfg = BalanceApp::default_config();
    app.begin(cfg, millis());

    // PID gains: NOT SET HERE — BOOTSTRAP (Phase 4.10c) measures K_motor on
    // RUN entry and derives Kp = ω_n²/K, Kd = 2ζω_n/K, Ki = 0.05·Kp from the
    // measurement. If the bot powers up with a saved mount, BOOTSTRAP fires
    // automatically below; otherwise operator triggers it via short-press
    // (CAPTURE→BOOTSTRAP→RUN auto-chain) or 'b' / long-press (skip capture).
    //
    // Anti-windup I-term clamp + D-term LPF time-constant are structural PID
    // safety properties (not per-bot tunings) — they bound integrator divergence
    // and BNO055 fused-Euler quantization noise respectively, both physical
    // properties of the controller architecture rather than the chassis.
    balance_pid.set_i_term_limit(40.0f);
    balance_pid.set_d_term_lpf_tau_sec(0.003f);

    // OnlineMountingEstimator: 8 s time constant (was 20 s).
    // 2026-05-18 PM bench finding: at 20 s, the mount offset stayed glued at
    // 0.87° for an entire 25 s run while the bot drifted in one direction —
    // classic "true balance point isn't 0° and the estimator never caught up"
    // failure. 8 s is fast enough that any persistent I-term bias will trim
    // out within a few seconds of contiguous RUN. Drift cap unchanged.
    online_est.set_lpf_time_constant_sec(8.0f);
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

    // "Prop-and-go" auto-BOOTSTRAP (Phase 4.10c): if a mount offset is saved
    // AND the bot's current pitch is within ±5° of that saved mount, fire
    // BOOTSTRAP. Otherwise stay in IDLE — the operator either propped the bot
    // wrong (recaptureable via 'c') or the saved mount is stale (same fix).
    //
    // Stale-mount sanity check guards the 2026-05-18 PM near-failure: a stored
    // mount of −1.21° while the bot's actual upright pitch was +0.3° → PID
    // corrected_pitch ≈ +1.5° → drove backward → kill switch at −22°. With
    // the gate, that scenario lands in IDLE for the operator to recapture
    // instead of slamming motors at a tipped bot.
    //
    // USE_BALANCE_AUTO_BOOTSTRAP (default ON, see base_balance build flags):
    // operator preference is "prop-and-go" — boot to balancing without console
    // interaction. Undefine the flag to require manual 'b' / long-press, useful
    // when the operator needs to position the bot in a safe test area before
    // motors engage. Per docs/todo.md "TOP PRIORITY — collision detection".
#ifdef USE_BALANCE_AUTO_BOOTSTRAP
    if (have_mount) {
        // Grace period — operator getting hands clear after power-on.
        delay(2000);
        // Sample a fresh pitch reading post-grace-period. read_sensors() reads
        // the IMU and updates app.get_pitch_deg().
        app.read_sensors(millis());
        const float pitch_now  = app.get_pitch_deg();
        const float mount_now  = app.get_mount_offset_deg();
        const float pitch_err  = pitch_now - mount_now;
        const float abs_err    = pitch_err < 0.0f ? -pitch_err : pitch_err;
        // Threshold matches BOOTSTRAP's own pre-condition (10°) minus headroom.
        // Lenient enough to tolerate normal mount drift, strict enough to
        // catch a fully-stale EEPROM mount (1.5°+ off).
        if (abs_err < 5.0f) {
            app.enter_bootstrap(millis());
        } else {
            Serial.print(F("stale_mount p="));
            Serial.print(pitch_now, 2);
            Serial.print(F(" m="));
            Serial.println(mount_now, 2);
            // Stay in IDLE — operator presses 'c' to recapture, then chain
            // continues automatically into BOOTSTRAP → RUN.
        }
    }
#else
    (void)have_mount;   // USE_BALANCE_AUTO_BOOTSTRAP disabled — manual trigger only
#endif

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
        else if (c == 's') {
            // Status: <state> <pitch> <mount> <output> <stiction>
            // RLS internals (K, P, Kp, Kd) removed 2026-05-18 to fit CHARACTERISE
            // implementation in flash budget. Add `g` (gains) command later if needed.
            Serial.print(app.state_name());          Serial.print(' ');
            Serial.print(app.get_pitch_deg(), 2);    Serial.print(' ');
            Serial.print(app.get_mount_offset_deg(), 2); Serial.print(' ');
            Serial.print(app.get_last_output());     Serial.print(' ');
            Serial.println(motors.stiction_min_pwm());
        } else if (c == 'g') {
            // Workstream G (G2) — flat telemetry line, drain-on-demand. Emitted
            // ONLY when the operator sends 'g' (never periodic), so it cannot
            // perturb the 5 ms PID tick. Fixed field order so a host parser can
            // rely on it — DO NOT reorder / insert fields:
            //   G,<millis>,<pitch_deg>,<pitch_sp_deg>,<wheel_vel_mps>,
            //     <position_m>,<nudge_deg>,<k_pos>,<k_vel>,<pos_leak>
            // On the Uno (no wheel encoders) the four outer-loop fields and the
            // wheel velocity are 0 — the arity is kept constant so one parser
            // serves both builds.
            Serial.print(F("G,"));
            Serial.print(millis());                       Serial.print(',');
            Serial.print(app.get_pitch_deg(), 3);         Serial.print(',');
            Serial.print(app.get_pitch_setpoint_deg(), 3); Serial.print(',');
#ifdef USE_WHEEL_ENCODERS
            Serial.print(app.get_wheel_velocity_mps(now), 4); Serial.print(',');
            Serial.print(app.get_position_m(), 4);        Serial.print(',');
            Serial.print(app.get_pos_nudge_deg(), 3);     Serial.print(',');
            Serial.print(app.get_k_pos(), 3);             Serial.print(',');
            Serial.print(app.get_k_vel(), 3);             Serial.print(',');
            Serial.println(app.get_pos_leak(), 4);
#else
            // No encoders → outer loop is absent; emit zeros to hold the arity.
            Serial.println(F("0.0000,0.0000,0.000,0.000,0.000,0.0000"));
#endif
        } else if (c == 'b') {
            // Manual BOOTSTRAP trigger — same as long-press, but operator can
            // bind it to a non-button console. Skips CAPTURE_MOUNTING, uses
            // whatever mount offset is currently loaded.
            app.enter_bootstrap(now);
        } else if (c == 'k') {
            app.enter_characterise_actuator(now);
        }
#ifdef USE_WHEEL_ENCODERS
        else if (c == 'e') {
            // Phase 4M.11 — wheel-encoder calibration. Blocking wizard: zeroes
            // both encoders, operator rolls the bot 1.000 m by hand, presses
            // the button; firmware computes counts-per-metre + wheel radius
            // and persists them to EEPROM slot 0x220. `a` during the wizard
            // aborts without writing. Safe to run only in IDLE (motors off).
            run_encoder_cal_();
        }
        else if (c == 'p') {
            // Phase 4M.12 — PWM range auto-discovery. Operator must lift the
            // bot off the ground before issuing; firmware ramps PWM 0→255 and
            // watches encoders for MIN_PWM (stiction) + MAX_PWM (saturation).
            // Result is saved to EEPROM on completion (see PWM_DISCOVERY→IDLE
            // handler below); next boot can consume the bounds.
            app.enter_pwm_discovery(now);
        }
#endif
    }

    // Item 3 — loop-side responsibilities only. The 5 ms PID tick runs in the
    // MsTimer2 ISR (pid_tick_isr); here we just keep the IMU snapshot fresh
    // and drain any deferred state-transition log lines the ISR queued.
    //
    // read_sensors() is the I²C-bound IMU read; run it as fast as the loop
    // can manage (~1 kHz typical). The ISR consumes the most recent stash.
    app.read_sensors(now);
    app.drain_state_log(Serial);
    // BOOTSTRAP / CHARACTERISE per-pulse telemetry — single line per pulse
    // showing cmd PWM, baseline-relative |Δω| (or |gyro| sum for char), the
    // threshold the metric was compared to, and whether it passed.
    static uint8_t last_pulse_seq = 0;
    app.drain_pulse_log(Serial, last_pulse_seq);

    // 2026-05-18 safety belt-and-suspenders: absolute-pitch kill-switch.
    // If pitch exceeds ±20° regardless of state, force-stop motors via
    // safety abort. Catches scenarios where the in-state FALLEN trigger
    // doesn't fire (BNO055 saturation, ISR stall) and prevents motors
    // running at 100% with the bot on its side.
    {
        float p = app.get_pitch_deg();
        // Audit P1-001: a NaN pitch makes BOTH (p > 20) and (p < -20) false
        // (all NaN comparisons are false), so the kill-switch was NaN-blind —
        // a non-finite IMU reading could leave the motors latched at the last
        // commanded PWM. Treat any non-finite pitch as a fault: cut motors and
        // request abort (-> IDLE), exactly as for an over-tilt.
        if (isnan(p) || isinf(p) || p > 20.0f || p < -20.0f) {
            motors.stop();
            safety.request_abort();
        }
    }

    // Audit P2-006: BalanceSafety maintains a watchdog that the 200 Hz tick()
    // feeds every healthy tick, but nothing ever read watchdog_starved() — the
    // watchdog was fed and never checked, so a wedged tick ISR (the exact
    // failure it exists to catch) produced no motor cut. Consume it here: if
    // the watchdog has gone hungry (default 200 ms with no feed), the control
    // loop is no longer running normally, so cut motors and request abort.
    if (safety.watchdog_starved(now)) {
        motors.stop();
        safety.request_abort();
    }

    // Persist state on key transitions so the next boot can auto-RUN.
    static BalanceAppState last_state = BalanceAppState::IDLE;
    BalanceAppState cur = app.get_state();
    if (cur != last_state) {
        if (last_state == BalanceAppState::CAPTURE_MOUNTING &&
            cur == BalanceAppState::IDLE) {
            float off = app.get_mount_offset_deg();
            if (save_mount_offset_(off)) {
                Serial.print(F("sv m="));
                Serial.println(off, 2);
            }
        } else if (last_state == BalanceAppState::CHAR_ACT &&
                   cur == BalanceAppState::IDLE) {
            uint8_t st = app.get_ch_stiction_pwm();
            if (st >= 30 && st <= 200) {
                save_actuator_(st);
                motors.set_stiction_min_pwm(st);
            }
            // Stiction reported via `s` status; no separate print needed.
        }
#ifdef USE_WHEEL_ENCODERS
        else if (last_state == BalanceAppState::PWM_DISCOVERY &&
                 cur == BalanceAppState::IDLE) {
            // Phase 4M.12 — persist discovered bounds (success path only).
            // Failure paths (timeout, abort) leave the stored slot untouched
            // so the previous-run value (if any) remains usable.
            const PwmDiscoveryResult& pd = app.get_pwm_discovery_result();
            if (pd.discovered &&
                pd.discovered_min_pwm > 0 &&
                pd.discovered_max_pwm > pd.discovered_min_pwm) {
                save_pwm_discovery_(pd.discovered_min_pwm,
                                    pd.discovered_max_pwm);
                // Gap-3 — wire the just-discovered MIN_PWM into the LIVE motor
                // driver's stiction floor so a completed `p` run takes effect
                // this session (previously it only applied on the next boot
                // via load_pwm_discovery_). The encoder-confirmed MIN is the
                // most trustworthy stiction estimate (it measures actual wheel
                // motion, not chassis rotation), so it overrides the
                // CHARACTERISE/EEPROM value — mirroring the boot-time apply.
                // Guarded by the SAME sane-PWM bounds as load_pwm_discovery_'s
                // boot path (min in [PWM_DISC_STEP_PWM,200], max>min, max<=255):
                // if the result is outside that envelope we leave the existing
                // default exactly as before — no regression to no-data behaviour.
                if (pd.discovered_min_pwm >= PWM_DISC_STEP_PWM &&
                    pd.discovered_min_pwm <= 200 &&
                    pd.discovered_max_pwm <= 255) {
                    motors.set_stiction_min_pwm(
                        (uint8_t)pd.discovered_min_pwm);
                }
                Serial.print(F("sv pd min=")); Serial.print(pd.discovered_min_pwm);
                Serial.print(F(" max="));      Serial.println(pd.discovered_max_pwm);
            } else {
                Serial.print(F("pd fail r="));
                Serial.println(pd.failure_reason);
            }
        }
#endif
        last_state = cur;
    }

    // STUCK detector hint — operator can see "STK" in s output (stiction
    // reading also helps diagnose). Removed dedicated print to save flash.
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

// USE_EKF gating — the 16-state EKF costs ~4 KB SRAM (4 x 16x16 float matrices).
// On the Mega's 8 KB SRAM this blows the budget; mega_orientation passes
// -D USE_EKF=0 to skip the filter (it is currently a stub with placeholder
// inputs anyway). Default ON for any other host that opts into this app.
// See docs/findings/mega_orientation_ram_overflow_diagnosis_2026-05-19.md.
#ifndef USE_EKF
#define USE_EKF 1
#endif

#if USE_EKF
#include "navigation/ekf.h"
#endif

#if ENABLE_SNAPSHOT_RECORDER
#include "features/snapshot_recorder.h"
#include "sensors/button_input.h"
#endif

BNO085 imu;
GPS gps;
SensorOutputManager output_manager;

#if USE_EKF
// Phase 3: Extended Kalman Filter for sensor fusion
ExtendedKalmanFilter ekf;
bool ekf_initialized = false;
#endif

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
#if USE_EKF
  // ========================================================================
  // Initialize EKF on first IMU data (Phase 3)
  // ========================================================================
  if (!ekf_initialized && imu.read() && imu.hasNewData()) {
    // Create initial covariance matrix with large uncertainties.
    // Note: P_init lives on the stack and is ~1 KB — fine on Teensy/ESP32,
    // tight on Mega but transient (this block runs once at boot).
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
#endif  // USE_EKF

  // ========================================================================
  // Read BNO085 Orientation (high frequency: ~100 Hz)
  // ========================================================================
  if (imu.read() && imu.hasNewData()) {
    const OrientationData& orientation = imu.getOrientation();
    output_manager.updateOrientation(orientation);

#if USE_EKF
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
#endif  // USE_EKF

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

#if USE_EKF
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
#else
    // EKF disabled — still log the GPS fix in calibration mode so the human
    // operator gets the same visibility as the EKF-enabled build.
    if (position.fix_quality >= 1 && IS_CALIBRATION_MODE) {
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
#endif  // USE_EKF
  }
#if USE_EKF
  else if (ekf_initialized && gps.isInitialized()) {
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
#else
  else if (gps.isInitialized()) {
    // EKF disabled — still surface "waiting for fix" status in cal mode.
    static uint32_t last_gps_check_ms = 0;
    uint32_t now_ms = millis();
    if (now_ms - last_gps_check_ms >= 1000) {
      last_gps_check_ms = now_ms;
      if (IS_CALIBRATION_MODE && !gps.hasLock()) {
        CAL_PRINTLN("GPS: Waiting for fix...");
      }
    }
  }
#endif  // USE_EKF

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

#if USE_EKF
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
#endif  // USE_EKF
}

#else
  #error "No application selected. Set USE_BALANCING_ROBOT or USE_IMU_GPS_TELEMETRY (or another USE_<APP> flag) in mode.h or platformio.ini build_flags."
#endif  // application dispatcher
