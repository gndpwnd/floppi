/*
 * dRehmFlight VTOL - Flight Controller Main
 * PlatformIO Port with Modular Architecture
 *
 * Original by Nicholas Rehm (dRehmFlight)
 * PlatformIO port with modular refactoring
 */

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "pin_definitions.h"
#include "globals.h"
#include "radioComm.h"
#include "imu.h"
#include "control.h"
#include "motors.h"

#ifdef CALIBRATION_MODE
#include "calibration.h"
#include "debug.h"
#endif

//========================================================================================================================//
//                                                 GLOBAL VARIABLES                                                        //
//========================================================================================================================//
// These are the actual definitions (not extern) - they're declared extern in globals.h

// Timing
unsigned long current_time, prev_time;
unsigned long print_counter;
float dt;

// IMU raw data
int16_t AcX, AcY, AcZ, GyX, GyY, GyZ, MgX, MgY, MgZ;
float AccX, AccY, AccZ;
float AccX_prev, AccY_prev, AccZ_prev;
float GyroX, GyroY, GyroZ;
float GyroX_prev, GyroY_prev, GyroZ_prev;
float MagX, MagY, MagZ;
float MagX_prev, MagY_prev, MagZ_prev;

// IMU calibration
float AccErrorX = IMU_ACC_ERROR_X;
float AccErrorY = IMU_ACC_ERROR_Y;
float AccErrorZ = IMU_ACC_ERROR_Z;
float GyroErrorX = IMU_GYRO_ERROR_X;
float GyroErrorY = IMU_GYRO_ERROR_Y;
float GyroErrorZ = IMU_GYRO_ERROR_Z;

// Attitude (Madgwick quaternion)
float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
float roll_IMU, pitch_IMU, yaw_IMU;
float roll_IMU_prev, pitch_IMU_prev;

// Desired states from radio
float thro_des, roll_des, pitch_des, yaw_des;
float roll_passthru, pitch_passthru, yaw_passthru;

// PID variables
float error_roll, error_roll_prev, integral_roll, integral_roll_prev, derivative_roll, roll_PID = 0;
float error_pitch, error_pitch_prev, integral_pitch, integral_pitch_prev, derivative_pitch, pitch_PID = 0;
float error_yaw, error_yaw_prev, integral_yaw, integral_yaw_prev, derivative_yaw, yaw_PID = 0;

// Motor commands
int m1_command_PWM, m2_command_PWM, m3_command_PWM, m4_command_PWM, m5_command_PWM, m6_command_PWM;
float m1_command_scaled, m2_command_scaled, m3_command_scaled, m4_command_scaled, m5_command_scaled, m6_command_scaled;

// Servo objects (Teensy only - ESP32 uses LEDC directly)
#ifndef USE_ESP32
PWMServo servo1, servo2, servo3, servo4, servo5, servo6, servo7;
#endif

// Servo commands
int s1_command_PWM, s2_command_PWM, s3_command_PWM, s4_command_PWM, s5_command_PWM, s6_command_PWM, s7_command_PWM;
float s1_command_scaled, s2_command_scaled, s3_command_scaled, s4_command_scaled, s5_command_scaled, s6_command_scaled, s7_command_scaled;

// Arming state
bool armedFly = false;

// Calibration program state (only in calibration builds)
#ifdef CALIBRATION_MODE
CalibrationMode calibration_mode = CALIB_NONE;
bool calibration_in_progress = false;
unsigned long calibration_start_time = 0;
int telemetry_mode = 0;  // 0=off, 1=IMU, 2=full
#endif

//========================================================================================================================//
//                                              FUNCTION DECLARATIONS                                                      //
//========================================================================================================================//

void setupBlink(int num_blinks, int blink_delay);

#ifdef CALIBRATION_MODE
void checkCalibrationMode();
void checkSerialCommands();
#endif

//========================================================================================================================//
//                                                      SETUP                                                              //
//========================================================================================================================//

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n\n========================================"));
    Serial.println(F("  dRehmFlight VTOL Flight Controller"));
    Serial.println(F("  Modular Architecture"));
    Serial.println(F("========================================\n"));

    // LED setup
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // Setup IMU
    Serial.print(F("IMU: "));
    setupIMU();

    // Setup radio
    Serial.print(F("Radio: "));
    radioSetup();

    // Get initial radio commands
    getCommands();

    #ifdef CALIBRATION_MODE
    // Check if CH6 is high on startup (trigger manual calibration)
    if (channel_6_pwm > 1800) {
        Serial.println(F("\nManual calibration triggered on startup!"));
        Serial.println(F("Keep board FLAT and STILL!"));
        delay(2000);
        calibrateIMU();
    }
    #endif

    // Setup motors and servos
    setupMotors();

    // Arm ESCs
    armMotors();

    Serial.println(F("\n========================================"));
    Serial.println(F("  FLIGHT CONTROLLER READY!"));
    Serial.println(F("========================================\n"));

    #ifdef CALIBRATION_MODE
    Serial.println(F("=== CALIBRATION MODE ==="));
    Serial.println(F("Serial commands (type in monitor):"));
    Serial.println(F("  r - Radio calibration"));
    Serial.println(F("  i - IMU cal (single-pos)"));
    Serial.println(F("  m - IMU cal (6-pos, more accurate)"));
    Serial.println(F("  o - IMU + Orientation"));
    Serial.println(F("  s - Status"));
    Serial.println(F("  h - Help"));
    Serial.println(F("\nCH6 switch (hold 3s):"));
    Serial.println(F("  Mid:  IMU cal"));
    Serial.println(F("  High: IMU + Orientation"));
    #endif

    // Final setup blink
    setupBlink(3, 160);

    delay(1000);
}

//========================================================================================================================//
//                                                   MAIN LOOP                                                             //
//========================================================================================================================//

void loop() {
    // Keep track of time
    prev_time = current_time;
    current_time = micros();
    dt = (current_time - prev_time) / 1000000.0;

    #ifdef CALIBRATION_MODE
    // Check for calibration mode
    checkCalibrationMode();
    checkSerialCommands();
    #endif

    // Core flight controller (skip if calibration in progress)
    #ifdef CALIBRATION_MODE
    if (!calibration_in_progress) {
    #endif
        // Get radio commands
        getCommands();

        // Check failsafe
        failSafe();

        // Get IMU data
        getIMUdata();

        // Attitude estimation with Madgwick filter
        #ifdef USE_MPU6050
            Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt);
        #elif defined(USE_MPU9250)
            Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
        #endif

        // Get desired state from radio
        getDesState();

        // Check arming status
        armedStatus();

        // Run flight controller if armed
        if (armedFly) {
            #ifdef USE_RATE_CONTROLLER
                controlRATE();
            #elif defined(USE_ANGLE_CONTROLLER)
                controlANGLE();
            #endif

            // Mix control outputs
            controlMixer();
        } else {
            // Not armed - zero PID outputs
            roll_PID = 0;
            pitch_PID = 0;
            yaw_PID = 0;

            m1_command_scaled = 0;
            m2_command_scaled = 0;
            m3_command_scaled = 0;
            m4_command_scaled = 0;
            m5_command_scaled = 0;
            m6_command_scaled = 0;
        }

        // Scale commands to PWM
        scaleCommands();

        // Apply throttle cut
        throttleCut();

        // Send commands to motors and servos
        commandMotors();

    #ifdef CALIBRATION_MODE
    }

    // Run calibration if mode is selected
    if (calibration_mode != CALIB_NONE && !calibration_in_progress) {
        calibration_in_progress = true;
        calibration_start_time = millis();

        Serial.println(F("\n=== CALIBRATION MODE ACTIVATED ==="));

        switch (calibration_mode) {
            case CALIB_ACCEL_GYRO:
                Serial.println(F("Running IMU Calibration (single-position, offsets only)"));
                calibrateIMU();
                break;

            case CALIB_6POSITION:
                Serial.println(F("Running 6-Position IMU Calibration (offsets + scale)"));
                calibrateIMU6Position();
                break;

            case CALIB_ATTITUDE:
                Serial.println(F("Running IMU Calibration + Orientation Detection"));
                calibrateIMUWithOrientation();
                break;

            case CALIB_RADIO:
                Serial.println(F("Running Radio Calibration"));
                calibrateRadio();
                break;

            default:
                break;
        }

        calibration_in_progress = false;
        calibration_mode = CALIB_NONE;

        Serial.println(F("=== CALIBRATION COMPLETE ==="));
        Serial.println(F("Returning to normal mode...\n"));

        // Blink LED to indicate calibration complete
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }

    // Debug output (uncomment as needed)
    //printRadioData();
    //printDesiredState();
    //printGyroData();
    //printAccelData();
    //printRollPitchYaw();
    //printPIDoutput();
    //printMotorCommands();
    //printLoopRate();

    // fc_tool telemetry output (toggle with 't' command)
    if (telemetry_mode == 1) {
        printIMUTelemetry();      // IMU data at 50Hz
    } else if (telemetry_mode == 2) {
        printFullTelemetry();     // Full telemetry at 20Hz
    }
    #endif

    // Status LED
    static unsigned long led_timer = 0;
    #ifdef CALIBRATION_MODE
    if (calibration_in_progress) {
        // Fast blink during calibration
        if (current_time - led_timer > 100000) {
            led_timer = current_time;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    } else
    #endif
    if (armedFly) {
        // Solid when armed
        digitalWrite(LED_PIN, HIGH);
    } else {
        // Normal 1Hz blink
        if (current_time - led_timer > 500000) {
            led_timer = current_time;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }

    // Maintain loop rate
    loopRate(LOOP_FREQUENCY_HZ);
}

//========================================================================================================================//
//                                              CALIBRATION FUNCTIONS                                                      //
//========================================================================================================================//

#ifdef CALIBRATION_MODE
void checkCalibrationMode() {
    static CalibrationMode last_mode = CALIB_NONE;
    static unsigned long mode_start_time = 0;

    // Safety: Don't allow calibration if armed or throttle high
    if (armedFly || channel_3_pwm > 1050) {
        calibration_mode = CALIB_NONE;
        return;
    }

    // Determine mode based on CH6 position
    CalibrationMode new_mode = CALIB_NONE;

    if (channel_6_pwm < 1200) {
        new_mode = CALIB_NONE;
    } else if (channel_6_pwm >= 1200 && channel_6_pwm <= 1800) {
        new_mode = CALIB_ACCEL_GYRO;
    } else if (channel_6_pwm > 1800) {
        new_mode = CALIB_ATTITUDE;
    }

    // Check if mode has changed
    if (new_mode != last_mode) {
        last_mode = new_mode;
        mode_start_time = millis();
        return;
    }

    // Check if we've held this position long enough
    if (new_mode != CALIB_NONE) {
        unsigned long hold_time = millis() - mode_start_time;

        if (hold_time >= 3000) {
            calibration_mode = new_mode;

            // Visual feedback
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
        }
    }
}

void checkSerialCommands() {
    // Don't process commands during active calibration
    if (calibration_in_progress) {
        return;
    }

    if (Serial.available() > 0) {
        char cmd = Serial.read();

        // Clear remaining characters
        while (Serial.available()) Serial.read();

        switch (cmd) {
            case 'r':
            case 'R':
                Serial.println(F("\n>>> Radio Calibration requested via serial"));
                calibration_mode = CALIB_RADIO;
                break;

            case 'i':
            case 'I':
                Serial.println(F("\n>>> IMU Calibration requested via serial"));
                calibration_mode = CALIB_ACCEL_GYRO;
                break;

            case 'o':
            case 'O':
                Serial.println(F("\n>>> IMU + Orientation Calibration requested via serial"));
                calibration_mode = CALIB_ATTITUDE;
                break;

            case 'm':
            case 'M':
                Serial.println(F("\n>>> 6-Position IMU Calibration requested via serial"));
                calibration_mode = CALIB_6POSITION;
                break;

            case 's':
            case 'S':
                Serial.println(F("\n=== STATUS ==="));
                Serial.print(F("CH1: ")); Serial.print(channel_1_pwm);
                Serial.print(F("  CH2: ")); Serial.print(channel_2_pwm);
                Serial.print(F("  CH3: ")); Serial.print(channel_3_pwm);
                Serial.print(F("  CH4: ")); Serial.print(channel_4_pwm);
                Serial.print(F("  CH5: ")); Serial.print(channel_5_pwm);
                Serial.print(F("  CH6: ")); Serial.println(channel_6_pwm);
                Serial.print(F("Armed: ")); Serial.println(armedFly ? "YES" : "NO");
                break;

            case 't':
            case 'T':
                // Cycle through telemetry modes: off -> IMU -> full -> off
                telemetry_mode = (telemetry_mode + 1) % 3;
                if (telemetry_mode == 0) {
                    Serial.println(F("\n>>> Telemetry OFF"));
                } else if (telemetry_mode == 1) {
                    Serial.println(F("\n>>> Telemetry: IMU (50Hz)"));
                    Serial.println(F("    Format: ax=X ay=Y az=Z gx=X gy=Y gz=Z"));
                } else {
                    Serial.println(F("\n>>> Telemetry: FULL (20Hz)"));
                    Serial.println(F("    Format: ax ay az gx gy gz roll pitch yaw m1 m2 m3 m4"));
                }
                break;

            case 'h':
            case 'H':
            case '?':
                Serial.println(F("\n=== CALIBRATION COMMANDS ==="));
                Serial.println(F("  r - Radio calibration (channel mapping)"));
                Serial.println(F("  i - IMU calibration (single-position, offsets only)"));
                Serial.println(F("  m - IMU calibration (6-position, offsets + scale)"));
                Serial.println(F("  o - IMU + Orientation detection"));
                Serial.println(F("  s - Status (show channel values)"));
                Serial.println(F("  t - Toggle telemetry (off/IMU/full) for fc_tool"));
                Serial.println(F("  h - Help (this menu)"));
                Serial.println(F("\nOr use CH6 switch:"));
                Serial.println(F("  Mid position (3s hold): IMU calibration"));
                Serial.println(F("  High position (3s hold): IMU + Orientation"));
                break;

            default:
                break;
        }
    }
}
#endif // CALIBRATION_MODE

//========================================================================================================================//
//                                              HELPER FUNCTIONS                                                           //
//========================================================================================================================//

void setupBlink(int num_blinks, int blink_delay) {
    for (int i = 0; i < num_blinks; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(blink_delay);
        digitalWrite(LED_PIN, LOW);
        delay(blink_delay);
    }
}
