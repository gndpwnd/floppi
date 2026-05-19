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
 *   'a' — emergency stop (latched; reflash to recover)
 *   's' — print one-line status
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
// Serial commands
// ----------------------------------------------------------------------------
static void handle_serial() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    switch (c) {
      case 'a':
      case 'A':
        app.abort();
        Serial.println(F("ABORT"));
        break;
      case 's':
      case 'S':
        Serial.print(F("armed="));   Serial.print(app.is_armed());
        Serial.print(F(" tipped=")); Serial.print(app.is_tipped());
        Serial.print(F(" pitch=")); Serial.print(app.last_pitch_deg(), 2);
        Serial.print(F(" pwm="));   Serial.println(app.last_pwm());
        break;
      default:
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

  // Arm the 5 ms PID tick.
  MsTimer2::set(PID_SAMPLE_MS, pid_isr);
  MsTimer2::start();

  Serial.println(F("READY"));
}

void loop() {
  // Refresh the IMU as fast as the BNO055 will allow. The fusion runs at
  // ~100 Hz internally, so polling much faster just returns the same sample.
  app.read_imu();

  // Handle 'a' / 's' commands.
  handle_serial();

  // Tiny yield — keeps us from hammering I2C harder than the BNO055 updates.
  delay(2);
}
