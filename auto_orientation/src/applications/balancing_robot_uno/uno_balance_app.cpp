/**
 * UnoBalanceApp implementation.
 *
 * See uno_balance_app.h for the design philosophy. Below is the bare-minimum
 * port of SelfBallancingRobot3.ino into framework-driver form.
 */

#include "uno_balance_app.h"
#include "balance_constants.h"

#include <math.h>      // isnan, fabs

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
      tipped_(false) {}

void UnoBalanceApp::begin() {
  pid_.set_tunings(BALANCE_KP, BALANCE_KI, BALANCE_KD);
  pid_.set_sample_time(PID_SAMPLE_MS);
  pid_.set_output_limits(static_cast<float>(PWM_MIN),
                         static_cast<float>(PWM_MAX));
  pid_.set_setpoint(0.0f);  // we balance around 0° corrected-pitch
  pid_.reset();

  last_pitch_deg_ = 0.0f;
  pitch_valid_    = false;
  last_pwm_       = 0;
  tipped_         = false;
  // armed_ retains its constructor value (true) — abort() is the only setter.
}

bool UnoBalanceApp::read_imu() {
  if (!imu_.read()) {
    pitch_valid_ = false;
    return false;
  }
  const OrientationData& d = imu_.getOrientation();
  float raw = d.pitch_deg;

  // Sanity gate — same as the reference .ino: reject NaN and any reading
  // whose magnitude exceeds PITCH_SANITY_DEG (BNO055 fused pitch can flip
  // near gimbal lock; we don't want to drive motors off garbage data).
  if (isnan(raw) || fabs(raw) >= PITCH_SANITY_DEG) {
    pitch_valid_ = false;
    return false;
  }

  last_pitch_deg_ = raw - PITCH_OFFSET_DEG;
  pitch_valid_    = true;
  return true;
}

int16_t UnoBalanceApp::step() {
  // Disarmed → motors off, no PID update.
  if (!armed_) {
    motors_.stop();
    last_pwm_ = 0;
    return 0;
  }

  // No fresh / valid pitch → motors off, but keep the loop alive so we
  // recover automatically once read_imu() succeeds again.
  if (!pitch_valid_) {
    motors_.stop();
    last_pwm_ = 0;
    return 0;
  }

  // Snapshot the volatile so the rest of this tick uses one consistent value.
  float pitch = last_pitch_deg_;

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
