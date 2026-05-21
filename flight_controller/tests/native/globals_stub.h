/*
 * globals_stub.h — minimal pure-C++ stand-in for flight_controller globals
 * =============================================================================
 * Host-side test stub. Declares/defines ONLY the global variables the motor
 * mixer / command-scaling math touches, as plain `float`/`int`/`bool` — no
 * Arduino types, no <Arduino.h>, no PWMServo. This lets the pure mixer
 * arithmetic from src/control.cpp (controlMixer) and src/motors.cpp
 * (scaleCommands / throttleCut) run unmodified on the host.
 *
 * Mirrors the relevant subset of include/globals.h. Variables are defined
 * `static` so each standalone test binary owns its own copy (no linking).
 *
 * Used by: tests/native/test_mixer.cpp
 * =============================================================================
 */
#ifndef FC_NATIVE_GLOBALS_STUB_H
#define FC_NATIVE_GLOBALS_STUB_H

namespace fc_stub {

// --- Control state: desired throttle + passthru (radio-derived) -------------
// thro_des is the [0,1] throttle the mixer adds to every motor.
static float thro_des       = 0.0f;
static float roll_passthru  = 0.0f;
static float pitch_passthru = 0.0f;
static float yaw_passthru   = 0.0f;

// --- PID outputs the mixer differentially applies to the motors -------------
static float roll_PID  = 0.0f;
static float pitch_PID = 0.0f;
static float yaw_PID   = 0.0f;

// --- Motor command outputs --------------------------------------------------
// *_scaled : mixer output, constrained to [0,1].
// *_PWM    : scaleCommands() result, microseconds (standard PWM: [1000,2000]).
static float m1_command_scaled = 0.0f, m2_command_scaled = 0.0f;
static float m3_command_scaled = 0.0f, m4_command_scaled = 0.0f;
static float m5_command_scaled = 0.0f, m6_command_scaled = 0.0f;

static int m1_command_PWM = 0, m2_command_PWM = 0, m3_command_PWM = 0;
static int m4_command_PWM = 0, m5_command_PWM = 0, m6_command_PWM = 0;

// --- Servo command outputs (mixer passthrough path) -------------------------
static float s1_command_scaled = 0.0f, s2_command_scaled = 0.0f;
static float s3_command_scaled = 0.0f, s4_command_scaled = 0.0f;
static float s5_command_scaled = 0.0f, s6_command_scaled = 0.0f;
static float s7_command_scaled = 0.0f;

// --- System state -----------------------------------------------------------
static bool armedFly = false;

// channel_5_pwm: aux channel used by throttleCut() (>1500 => cut).
static int channel_5_pwm = 1000;

// --- PID integrators reset by throttleCut() ---------------------------------
static float integral_roll  = 0.0f;
static float integral_pitch = 0.0f;
static float integral_yaw   = 0.0f;

} // namespace fc_stub

#endif // FC_NATIVE_GLOBALS_STUB_H
