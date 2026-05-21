/**
 * Minimal Self-Balancing Robot — Arduino Uno entry point.
 *
 * This file is the COMPLETE program (setup + loop). It is built ONLY by the
 * `arduino_uno_minimal` env, which excludes the universal balance app's
 * main.cpp via build_src_filter. There is NO #ifdef gate around this code —
 * if the file is in the build, it IS the program.
 *
 * Hardware (matches docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino):
 *   - BNO055 on I2C  (Uno: SDA=A4, SCL=A5). Address 0x28.
 *   - L298N motor driver: ENA=5, IN1=6, IN2=7, IN3=9, IN4=8, ENB=10.
 *
 * Serial commands (115200 baud):
 *   'a' — emergency stop (latched until 'g' or reflash)
 *   'g' — re-arm after abort (clears the disarm latch and PID integral)
 *   's' — print one-line status (one-shot)
 *   'p' — toggle 10 Hz periodic telemetry stream (off at boot)
 *
 * Anything beyond that is deliberately absent. The brute-force tuner workflow
 * does NOT need runtime gain editing — change balance_constants.h, recompile,
 * reflash. The whole point of this program is to be small enough that the
 * flash-erase-write cycle is the iteration loop.
 */

#include <Arduino.h>
#include <Wire.h>
#include <MsTimer2.h>

#include "../../sensors/bno055.h"
#include "../../actuators/l298n_motor_driver.h"
#include "../../control/pid_controller.h"
#include "uno_balance_app.h"
#include "balance_constants.h"

#ifdef UNO_GUIDED_TUNING
#include "tuning_session.h"
#endif

// ----------------------------------------------------------------------------
// Module instances (file-scope — no heap)
// ----------------------------------------------------------------------------

// BNO055: Adafruit breakouts ship with the external 32 kHz crystal populated.
// Set BNO055_NO_EXT_CRYSTAL in build_flags for generic modules (CJMCU-055, etc).
#ifdef BNO055_NO_EXT_CRYSTAL
static BNO055             imu(0x28, /*use_ext_crystal=*/false);
#else
static BNO055             imu(0x28, /*use_ext_crystal=*/true);
#endif

// L298N pin map matches the reference .ino exactly. If your bot drives
// backward when it should drive forward, swap (in1, in2) and (in3, in4) here
// rather than rewiring the motor leads.
static L298NPins          motor_pins = {
    /*ena*/ 5,  /*in1*/ 6, /*in2*/ 7,
    /*enb*/ 10, /*in3*/ 9, /*in4*/ 8
};
static L298NMotorDriver   motors(motor_pins, STICTION_PWM);

// PID initial gains are zero; UnoBalanceApp::begin() applies the constants.
// Output range matches PWM_MIN/PWM_MAX from balance_constants.h.
static PIDController      balance_pid(0.0f, 0.0f, 0.0f,
                                      static_cast<float>(PWM_MIN),
                                      static_cast<float>(PWM_MAX));

static UnoBalanceApp      app(imu, motors, balance_pid);

#ifdef UNO_GUIDED_TUNING
// Guided P->I->D tuning state machine over the serial CLI. Tuning build only.
static TuningSession      tuningSession;
#endif

// ----------------------------------------------------------------------------
// MsTimer2 ISR: 5 ms PID tick
// ----------------------------------------------------------------------------
// Doing the I2C IMU read in the ISR would block for ~3 ms at 100 kHz I²C —
// longer than the tick itself. Instead, loop() refreshes pitch as fast as it
// can and this ISR only consumes the cached value.
static void pid_isr() {
  app.step();
}

// ----------------------------------------------------------------------------
// Serial commands & periodic telemetry
// ----------------------------------------------------------------------------

// Periodic-telemetry toggle. False at boot — operator opts in via 'p'.
static bool     telem_periodic = false;
// Last time we emitted a periodic-telemetry line (ms). Reset whenever the
// stream is toggled on so the first line appears immediately.
static uint32_t telem_last_ms  = 0;
// Period between periodic-telemetry lines (~10 Hz).
static const uint16_t TELEM_PERIOD_MS = 100;

// Emit one status line. Uses int16 decideg (pitch * 10) instead of float to
// avoid pulling dtostrf into flash — saves ~1.5 KB on AVR vs Serial.print(float).
static void print_status_line() {
  Serial.print(F("armed="));   Serial.print(app.is_armed());
  Serial.print(F(" tipped=")); Serial.print(app.is_tipped());
  // Pitch printed as decideg (e.g. -86 means -8.6°). Caller divides by 10.
  Serial.print(F(" pitch_dd=")); Serial.print((int16_t)(app.last_pitch_deg() * 10.0f));
  Serial.print(F(" pwm="));   Serial.println(app.last_pwm());
}

static void handle_serial() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    switch (c) {
      case 'a':
      case 'A':
        app.abort();
        Serial.println(F("ABORT"));
        break;
      case 'g':
      case 'G':
        app.arm();
        Serial.println(F("ARMED"));
        break;
      case 's':
      case 'S':
        print_status_line();
#ifdef UNO_GUIDED_TUNING
        // Tuning build: also emit the stage + working-gains line.
        tuningSession.print_status();
#endif
        break;
      case 'p':
      case 'P':
        telem_periodic = !telem_periodic;
        // Emit immediately so the operator sees the first sample on toggle.
        telem_last_ms = millis() - TELEM_PERIOD_MS;
        Serial.print(F("telem="));
        Serial.println(telem_periodic ? F("on") : F("off"));
        break;
      default:
#ifdef UNO_GUIDED_TUNING
        // Tuning build: route guided-tuning command chars (t + - * n b r w q).
        // 'a'/'g'/'s'/'p' are handled above; print_status above also covers 's'.
        tuningSession.handle_command((char)c);
#endif
        // ignore — minimal program, no other commands
        break;
    }
  }
}

// ----------------------------------------------------------------------------
// setup() / loop()
// ----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println(F("uno_balance_minimal"));
  Serial.print(F("Kp=")); Serial.print(BALANCE_KP);
  Serial.print(F(" Ki=")); Serial.print(BALANCE_KI);
  Serial.print(F(" Kd=")); Serial.print(BALANCE_KD);
  Serial.print(F(" off=")); Serial.println(PITCH_OFFSET_DEG);

  if (!motors.begin()) {
    Serial.println(F("ERR motors"));
    // Don't deadlock — fall through; abort() keeps things safe.
    app.abort();
  }

  if (!imu.begin()) {
    Serial.println(F("ERR BNO055"));
    app.abort();
  }

  app.begin();

#ifdef UNO_GUIDED_TUNING
  // Bind the guided-tuning session to the app (seeds working gains from
  // whatever app.begin() loaded — EEPROM tune block or balance_constants seed).
  tuningSession.begin(app);
#endif

  // PID_v1 parity: the reference SelfBallancingRobot3.ino used PID_v1, which
  // computes the derivative on the raw measurement with NO low-pass filter.
  // Our framework PIDController defaults to a non-zero D-term LPF tau, which
  // silently attenuates the effective Kd vs. the reference and breaks the
  // assumption that the brute tuner / reference gains "just work" out of the
  // box. Set tau = 0 so the derivative is raw, matching PID_v1.
  balance_pid.set_d_term_lpf_tau_sec(0.0f);

  // BNO055 NDOF fusion convergence — the chip needs ~700 ms after begin() to
  // produce stable Euler output. 1500 ms gives margin. MUST come BEFORE the
  // PID ISR is armed, otherwise the ISR consumes pre-fusion garbage pitch and
  // (with Kp=65) slams the motors to full PWM, tipping the bot on power-up.
  // Reference SelfBallancingRobot3.ino uses delay(500)+delay(1000) for the
  // same reason.
  delay(1500);

  // Arm the 5 ms PID tick.
  MsTimer2::set(PID_SAMPLE_MS, pid_isr);
  MsTimer2::start();

  Serial.println(F("READY"));
}

void loop() {
  // Refresh the IMU as fast as the BNO055 will allow. The fusion runs at
  // ~100 Hz internally, so polling much faster just returns the same sample.
  app.read_imu();

  // Handle 'a' / 'g' / 's' / 'p' commands.
  handle_serial();

  // Periodic telemetry (opt-in via 'p'). Uses millis() rollover-safe diff.
  if (telem_periodic) {
    const uint32_t now = millis();
    if ((uint32_t)(now - telem_last_ms) >= TELEM_PERIOD_MS) {
      telem_last_ms = now;
      print_status_line();
    }
  }

  // Tiny yield — keeps us from hammering I2C harder than the BNO055 updates.
  delay(2);
}
