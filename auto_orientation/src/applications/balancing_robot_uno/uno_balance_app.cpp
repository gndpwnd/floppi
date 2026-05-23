/**
 * UnoBalanceApp implementation.
 *
 * See uno_balance_app.h for the design philosophy. Below is the bare-minimum
 * port of SelfBallancingRobot3.ino into framework-driver form.
 */

#include "uno_balance_app.h"
#include "balance_constants.h"
#include "tune_storage.h"   // EEPROM boot-precedence — host-safe, platform-guarded

#include <math.h>      // isnan, fabs

#if defined(ARDUINO)
  #include <Arduino.h>   // Serial, F()
#endif

// ATOMIC_BLOCK guards multi-byte (float / int16_t) reads/writes shared between
// loop() and the MsTimer2 5 ms ISR. On 8-bit AVR a 4-byte float load can be
// interrupted mid-byte by the ISR, producing a value that has never existed
// (high half from sample N, low half from sample N+1). Multiplied by Kp=65
// inside the PID, even one torn read commands full PWM and tips the bot.
//
// Same pattern as applications/balancing_robot/balance_app.cpp lines 44-62.
// On non-AVR (NATIVE_TEST) the macro shims to a no-op — single-threaded native
// tests cannot race, and 32-bit hosts have atomic float loads anyway.
#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
  #include <util/atomic.h>
#else
  #ifndef ATOMIC_BLOCK
    #define ATOMIC_BLOCK(x)
    #define ATOMIC_RESTORESTATE
  #endif
#endif

UnoBalanceApp::UnoBalanceApp(OrientationSensor& imu,
                             DualMotorDriver& motors,
                             PIDController& pid)
    : imu_(imu),
      motors_(motors),
      pid_(pid),
      last_pitch_deg_(0.0f),
      pitch_valid_(false),
      last_pwm_(0),
      armed_(true),
      tipped_(false),
      cur_kp_(BALANCE_KP),
      cur_ki_(BALANCE_KI),
      cur_kd_(BALANCE_KD),
      read_fail_count_(0) {}

void UnoBalanceApp::begin() {
  // Boot precedence: a valid EEPROM tune block wins over the
  // balance_constants.h seed. tune_storage is platform-guarded (host-safe).
  TuneBlock blk;
  if (tune_storage::load_tuning(blk)) {
    cur_kp_ = blk.kp;
    cur_ki_ = blk.ki;
    cur_kd_ = blk.kd;
#if defined(ARDUINO)
    Serial.println(F("gains=EEPROM"));
#endif
  } else {
    cur_kp_ = BALANCE_KP;
    cur_ki_ = BALANCE_KI;
    cur_kd_ = BALANCE_KD;
#if defined(ARDUINO)
    Serial.println(F("gains=seed"));
#endif
  }
  pid_.set_tunings(cur_kp_, cur_ki_, cur_kd_);
  pid_.set_sample_time(PID_SAMPLE_MS);
  pid_.set_output_limits(static_cast<float>(PWM_MIN),
                         static_cast<float>(PWM_MAX));
  pid_.set_setpoint(0.0f);  // we balance around 0° corrected-pitch
  pid_.reset();

  last_pitch_deg_  = 0.0f;
  pitch_valid_     = false;
  last_pwm_        = 0;
  tipped_          = false;
  read_fail_count_ = 0;
  // armed_ retains its constructor value (true) — abort() is the only setter.
}

bool UnoBalanceApp::read_imu() {
  if (!imu_.read()) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      pitch_valid_ = false;
    }
    if (read_fail_count_ < 255) read_fail_count_++;
    return false;
  }
  const OrientationData& d = imu_.getOrientation();
  float raw = d.pitch_deg;

  // Sanity gate — same as the reference .ino: reject NaN and any reading
  // whose magnitude exceeds PITCH_SANITY_DEG (BNO055 fused pitch can flip
  // near gimbal lock; we don't want to drive motors off garbage data).
  if (isnan(raw) || fabs(raw) >= PITCH_SANITY_DEG) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      pitch_valid_ = false;
    }
    if (read_fail_count_ < 255) read_fail_count_++;
    return false;
  }

  // Compute the corrected pitch into a local first, then publish both the
  // float AND the valid-flag in one atomic block so the ISR cannot observe
  // pitch_valid_=true with a torn last_pitch_deg_.
  const float corrected = raw - PITCH_OFFSET_DEG;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    last_pitch_deg_ = corrected;
    pitch_valid_    = true;
  }
  read_fail_count_ = 0;  // good read clears the rolling failure count
  return true;
}

int16_t UnoBalanceApp::step() {
  // Disarmed → motors off, no PID update.
  if (!armed_) {
    motors_.stop();
    last_pwm_ = 0;
    return 0;
  }

  // Snapshot the ISR-shared state under one critical section. Reading
  // last_pitch_deg_ outside ATOMIC_BLOCK would race with read_imu()'s 4-byte
  // store (see file-top comment). ATOMIC_BLOCK on the ISR side is technically
  // a no-op (interrupts already disabled inside the MsTimer2 ISR) but the
  // RESTORESTATE shim is cheap and makes the snapshot pattern symmetric with
  // the writer — and it lets step() be safely called from non-ISR contexts
  // (e.g. native tests, future bench harnesses).
  bool  pitch_ok;
  float pitch;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    pitch_ok = pitch_valid_;
    pitch    = last_pitch_deg_;
  }

  // No fresh / valid pitch → motors off, but keep the loop alive so we
  // recover automatically once read_imu() succeeds again.
  if (!pitch_ok) {
    motors_.stop();
    last_pwm_ = 0;
    return 0;
  }

  // Tip cutoff: bot is too far over, don't fight it. Reset PID so the
  // integrator doesn't wind up while we wait to be set back upright.
  if (fabs(pitch) > TIP_CUTOFF_DEG) {
    tipped_ = true;
    motors_.stop();
    pid_.reset();
    last_pwm_ = 0;
    return 0;
  }
  tipped_ = false;

  // Run PID and drive both wheels symmetrically (no yaw control in v1).
  float out = pid_.compute(pitch, PID_SAMPLE_MS);
  int16_t pwm = static_cast<int16_t>(out);
  motors_.set_speed(pwm);
  last_pwm_ = pwm;
  return pwm;
}

void UnoBalanceApp::abort() {
  armed_ = false;
  motors_.stop();
  pid_.reset();
  last_pwm_ = 0;
}

void UnoBalanceApp::apply_gains(float kp, float ki, float kd) {
  // Defensive clamp: negative PID gains invert the control law and would
  // drive the bot away from upright. The guided-tuning state machine already
  // clamps its work gains >= 0, but apply_gains() is public API — guard here
  // too so a future caller or test harness cannot inject negative gains.
  if (kp < 0.0f) kp = 0.0f;
  if (ki < 0.0f) ki = 0.0f;
  if (kd < 0.0f) kd = 0.0f;
  cur_kp_ = kp;
  cur_ki_ = ki;
  cur_kd_ = kd;
  pid_.set_tunings(kp, ki, kd);
}

int16_t UnoBalanceApp::last_pwm() const {
  // Atomic copy of the ISR-written 16-bit value (see header note).
  int16_t pwm;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    pwm = last_pwm_;
  }
  return pwm;
}

void UnoBalanceApp::get_work_gains(float& kp, float& ki, float& kd) const {
  kp = cur_kp_;
  ki = cur_ki_;
  kd = cur_kd_;
}

void UnoBalanceApp::arm() {
  // Clear any stale tip state and wind-up before the next step() runs — we do
  // NOT want to commit a bunch of pre-arm integrated error to the motors on
  // the first tick post-arm.
  pid_.reset();
  tipped_   = false;
  last_pwm_ = 0;
  armed_    = true;
}
