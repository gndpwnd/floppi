/**
 * Wheel Encoder Driver Implementation
 *
 * Two compile modes:
 *
 *   1. Arduino build with USE_WHEEL_ENCODERS defined
 *      ------------------------------------------------
 *      Wraps `Encoder` from PaulStoffregen/Encoder@^1.4.4. The PJRC library
 *      installs CHANGE interrupts on the two pins and maintains a volatile
 *      int32_t count internally. Its read() accessor does the equivalent of
 *      ATOMIC_BLOCK on AVR (wraps the read in noInterrupts() / interrupts()).
 *
 *   2. NATIVE_TEST build
 *      ------------------------------------------------
 *      The PJRC Encoder library cannot run on a host — it requires Arduino's
 *      attachInterrupt and the AVR/Teensy ISR table. Instead we expose
 *      set_ticks_for_test(int32_t) and read_ticks() returns the mocked value.
 *      Velocity / distance / stall logic is fully exercised; only the
 *      hardware tick source is mocked. See tests/test_wheel_encoder.cpp.
 *
 * On Arduino envs that do NOT define USE_WHEEL_ENCODERS (e.g. uno_balance,
 * arduino_uno_minimal, mega_orientation), this TU is empty — keeping the
 * header safely includable everywhere without forcing the PJRC lib onto envs
 * that have no encoders attached.
 */

#include "wheel_encoder.h"

#include <math.h>

// ============================================================================
// Backend selection
// ============================================================================

#if defined(NATIVE_TEST)
  // Host-side test build — no Arduino, no PJRC Encoder.
  #define WHEEL_ENCODER_BACKEND_MOCK 1
#elif defined(USE_WHEEL_ENCODERS)
  // Real hardware path — pull in PJRC Encoder.
  #include <Arduino.h>
  #include <Encoder.h>
  #define WHEEL_ENCODER_BACKEND_PJRC 1
#else
  // Other Arduino envs — driver is unused, compile a no-op stub so static
  // libraries / tests don't fail to link. Members below are still defined.
  #define WHEEL_ENCODER_BACKEND_STUB 1
#endif

// ============================================================================
// Backend helpers
// ============================================================================

namespace {

#if defined(WHEEL_ENCODER_BACKEND_PJRC)
inline Encoder* as_pjrc(void* p) { return static_cast<Encoder*>(p); }
#endif

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegPerRad = 57.29577951308232f;
constexpr float kRadPerDeg = 0.017453292519943295f;

}  // namespace

// ============================================================================
// Construction / destruction
// ============================================================================

WheelEncoder::WheelEncoder(uint8_t pin_a, uint8_t pin_b)
    : pin_a_(pin_a),
      pin_b_(pin_b),
      cpr_(kDefaultCPR),
      wheel_radius_m_(kDefaultRadiusM),
      velocity_window_ms_(kDefaultVelocityWindowMs),
      last_velocity_ticks_(0),
      last_velocity_ms_(0),
      last_velocity_dps_(0),
      pwm_above_threshold_since_ms_(0),
      last_pwm_abs_(0),
      last_pwm_report_ms_(0),
      backend_(nullptr),
      initialized_(false)
#ifdef NATIVE_TEST
      ,
      mock_ticks_(0)
#endif
{
}

WheelEncoder::~WheelEncoder() {
#if defined(WHEEL_ENCODER_BACKEND_PJRC)
  if (backend_ != nullptr) {
    delete as_pjrc(backend_);
    backend_ = nullptr;
  }
#endif
}

// ============================================================================
// Lifecycle
// ============================================================================

bool WheelEncoder::begin() {
  if (initialized_) return true;

#if defined(WHEEL_ENCODER_BACKEND_PJRC)
  // PJRC Encoder constructor installs the interrupt handlers and configures
  // the pins as INPUT_PULLUP automatically. Allocate via `new` so the heavy
  // ~24 B/instance state lives off the stack.
  backend_ = new Encoder(pin_a_, pin_b_);
  if (backend_ == nullptr) return false;
#endif

  initialized_ = true;
  return true;
}

// ============================================================================
// Tick + velocity reads
// ============================================================================

int32_t WheelEncoder::read_ticks() {
#if defined(WHEEL_ENCODER_BACKEND_PJRC)
  if (backend_ == nullptr) return 0;
  // PJRC Encoder::read() is itself atomic on AVR (wraps noInterrupts/interrupts).
  return static_cast<int32_t>(as_pjrc(backend_)->read());
#elif defined(NATIVE_TEST)
  return mock_ticks_;
#else
  return 0;
#endif
}

void WheelEncoder::reset_ticks() {
#if defined(WHEEL_ENCODER_BACKEND_PJRC)
  if (backend_ != nullptr) {
    as_pjrc(backend_)->write(0);
  }
#elif defined(NATIVE_TEST)
  mock_ticks_ = 0;
#endif
  last_velocity_ticks_ = 0;
  last_velocity_ms_ = 0;
  last_velocity_dps_ = 0;
  pwm_above_threshold_since_ms_ = 0;
}

int32_t WheelEncoder::read_velocity_dps(uint32_t now_ms) {
  const int32_t now_ticks = read_ticks();

  // Initialise the window on the very first call (last_velocity_ms_ == 0).
  if (last_velocity_ms_ == 0) {
    last_velocity_ticks_ = now_ticks;
    last_velocity_ms_ = (now_ms == 0) ? 1 : now_ms;  // avoid re-init loop at t=0
    last_velocity_dps_ = 0;
    return 0;
  }

  const uint32_t dt_ms = now_ms - last_velocity_ms_;
  if (dt_ms < velocity_window_ms_) {
    // Hold previous sample to avoid amplifying tick-quantisation noise.
    return last_velocity_dps_;
  }

  const int32_t d_ticks = now_ticks - last_velocity_ticks_;
  last_velocity_ticks_ = now_ticks;
  last_velocity_ms_ = now_ms;

  if (cpr_ <= 0 || dt_ms == 0) {
    last_velocity_dps_ = 0;
    return 0;
  }

  // ticks/sec → deg/sec at the wheel
  const float ticks_per_sec = (static_cast<float>(d_ticks) * 1000.0f) /
                              static_cast<float>(dt_ms);
  const float dps = ticks_per_sec * (360.0f / static_cast<float>(cpr_));
  last_velocity_dps_ = static_cast<int32_t>(dps >= 0 ? dps + 0.5f : dps - 0.5f);
  return last_velocity_dps_;
}

float WheelEncoder::read_velocity_mps(uint32_t now_ms) {
  const int32_t dps = read_velocity_dps(now_ms);
  return (static_cast<float>(dps) * kRadPerDeg) * wheel_radius_m_;
}

float WheelEncoder::distance_m() {
  const int32_t ticks = read_ticks();
  if (cpr_ <= 0) return 0.0f;
  // d = (ticks / cpr) * 2π * radius
  return (static_cast<float>(ticks) / static_cast<float>(cpr_)) *
         (2.0f * kPi * wheel_radius_m_);
}

// ============================================================================
// Stall detection
// ============================================================================

void WheelEncoder::report_commanded_pwm(uint16_t pwm_abs, uint32_t now_ms) {
  last_pwm_abs_ = pwm_abs;
  last_pwm_report_ms_ = now_ms;
}

bool WheelEncoder::stalled(uint16_t pwm_threshold, uint16_t time_ms) {
  const uint32_t now_ms = last_pwm_report_ms_;

  // No PWM report yet — can't decide.
  if (now_ms == 0) return false;

  if (last_pwm_abs_ > pwm_threshold) {
    if (pwm_above_threshold_since_ms_ == 0) {
      pwm_above_threshold_since_ms_ = (now_ms == 0) ? 1 : now_ms;
      return false;  // not stalled until the timer elapses
    }
    const uint32_t elapsed = now_ms - pwm_above_threshold_since_ms_;
    if (elapsed < time_ms) return false;

    // Sustained PWM above threshold for >= time_ms — check velocity.
    // Use the cached velocity (no need to recompute; caller is expected to
    // poll read_velocity_dps regularly).
    const int32_t v = last_velocity_dps_;
    const float v_abs = v < 0 ? -static_cast<float>(v)
                              :  static_cast<float>(v);
    return v_abs < kStallVelocityDps;
  }

  // PWM dropped below threshold — reset the timer.
  pwm_above_threshold_since_ms_ = 0;
  return false;
}
