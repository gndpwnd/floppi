/*
 * Control Module Implementation
 * PID controllers and mixer
 */

#include "control.h"
#include "globals.h"
#include "config.h"
#include "pin_definitions.h"
#include "radioComm.h"

//========================================================================================================================//
//                                              GET DESIRED STATE                                                          //
//========================================================================================================================//

void getDesState() {
    thro_des = (channel_3_pwm - 1000.0) / 1000.0;
    roll_des = (channel_1_pwm - 1500.0) / 500.0;
    pitch_des = (channel_2_pwm - 1500.0) / 500.0;
    yaw_des = (channel_4_pwm - 1500.0) / 500.0;

    thro_des = constrain(thro_des, 0.0, 1.0);
    roll_des = constrain(roll_des, -1.0, 1.0);
    pitch_des = constrain(pitch_des, -1.0, 1.0);
    yaw_des = constrain(yaw_des, -1.0, 1.0);

    #ifdef USE_RATE_CONTROLLER
        roll_des *= MAX_ROLL_RATE;
        pitch_des *= MAX_PITCH_RATE;
        yaw_des *= MAX_YAW_RATE;
    #else
        roll_des *= MAX_ROLL_ANGLE;
        pitch_des *= MAX_PITCH_ANGLE;
        yaw_des *= MAX_YAW_RATE;
    #endif

    roll_passthru = (channel_1_pwm - 1500.0) / 500.0;
    pitch_passthru = (channel_2_pwm - 1500.0) / 500.0;
    yaw_passthru = (channel_4_pwm - 1500.0) / 500.0;
}

//========================================================================================================================//
//                                              RATE CONTROLLER                                                            //
//========================================================================================================================//

void controlRATE() {
    // Roll
    error_roll = roll_des - GyroX;
    integral_roll += error_roll * dt;
    integral_roll = constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL);
    derivative_roll = (error_roll - error_roll_prev) / dt;
    roll_PID = KP_ROLL_RATE * error_roll + KI_ROLL_RATE * integral_roll + KD_ROLL_RATE * derivative_roll;
    error_roll_prev = error_roll;

    // Pitch
    error_pitch = pitch_des - GyroY;
    integral_pitch += error_pitch * dt;
    integral_pitch = constrain(integral_pitch, -I_LIMIT_PITCH, I_LIMIT_PITCH);
    derivative_pitch = (error_pitch - error_pitch_prev) / dt;
    pitch_PID = KP_PITCH_RATE * error_pitch + KI_PITCH_RATE * integral_pitch + KD_PITCH_RATE * derivative_pitch;
    error_pitch_prev = error_pitch;

    // Yaw
    error_yaw = yaw_des - GyroZ;
    integral_yaw += error_yaw * dt;
    integral_yaw = constrain(integral_yaw, -I_LIMIT_YAW, I_LIMIT_YAW);
    derivative_yaw = (error_yaw - error_yaw_prev) / dt;
    yaw_PID = KP_YAW_RATE * error_yaw + KI_YAW_RATE * integral_yaw + KD_YAW_RATE * derivative_yaw;
    error_yaw_prev = error_yaw;
}

//========================================================================================================================//
//                                              ANGLE CONTROLLER                                                           //
//========================================================================================================================//

void controlANGLE() {
    // Roll
    error_roll = roll_des - roll_IMU;
    integral_roll += error_roll * dt;
    integral_roll = constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL);
    derivative_roll = -GyroX;
    roll_PID = KP_ROLL_ANGLE * error_roll + KI_ROLL_ANGLE * integral_roll + KD_ROLL_ANGLE * derivative_roll;
    error_roll_prev = error_roll;

    // Pitch
    error_pitch = pitch_des - pitch_IMU;
    integral_pitch += error_pitch * dt;
    integral_pitch = constrain(integral_pitch, -I_LIMIT_PITCH, I_LIMIT_PITCH);
    derivative_pitch = -GyroY;
    pitch_PID = KP_PITCH_ANGLE * error_pitch + KI_PITCH_ANGLE * integral_pitch + KD_PITCH_ANGLE * derivative_pitch;
    error_pitch_prev = error_pitch;

    // Yaw (rate control)
    error_yaw = yaw_des - GyroZ;
    integral_yaw += error_yaw * dt;
    integral_yaw = constrain(integral_yaw, -I_LIMIT_YAW, I_LIMIT_YAW);
    derivative_yaw = (error_yaw - error_yaw_prev) / dt;
    yaw_PID = KP_YAW_RATE * error_yaw + KI_YAW_RATE * integral_yaw + KD_YAW_RATE * derivative_yaw;
    error_yaw_prev = error_yaw;
}

//========================================================================================================================//
//                                              CONTROL MIXER                                                              //
//========================================================================================================================//

void controlMixer() {
    // *** CUSTOMIZE FOR YOUR AIRCRAFT ***
    // Quadcopter X configuration (default)
    m1_command_scaled = thro_des - pitch_PID + roll_PID + yaw_PID;
    m2_command_scaled = thro_des - pitch_PID - roll_PID - yaw_PID;
    m3_command_scaled = thro_des + pitch_PID - roll_PID + yaw_PID;
    m4_command_scaled = thro_des + pitch_PID + roll_PID - yaw_PID;
    m5_command_scaled = 0.0;
    m6_command_scaled = 0.0;

    m1_command_scaled = constrain(m1_command_scaled, 0.0, 1.0);
    m2_command_scaled = constrain(m2_command_scaled, 0.0, 1.0);
    m3_command_scaled = constrain(m3_command_scaled, 0.0, 1.0);
    m4_command_scaled = constrain(m4_command_scaled, 0.0, 1.0);
    m5_command_scaled = constrain(m5_command_scaled, 0.0, 1.0);
    m6_command_scaled = constrain(m6_command_scaled, 0.0, 1.0);

    // Servo mixing (passthrough example)
    s1_command_scaled = roll_passthru;
    s2_command_scaled = pitch_passthru;
    s3_command_scaled = yaw_passthru;
    s4_command_scaled = 0.0;
    s5_command_scaled = 0.0;
    s6_command_scaled = 0.0;
    s7_command_scaled = 0.0;

    s1_command_scaled = constrain(s1_command_scaled, -1.0, 1.0);
    s2_command_scaled = constrain(s2_command_scaled, -1.0, 1.0);
    s3_command_scaled = constrain(s3_command_scaled, -1.0, 1.0);
    s4_command_scaled = constrain(s4_command_scaled, -1.0, 1.0);
    s5_command_scaled = constrain(s5_command_scaled, -1.0, 1.0);
    s6_command_scaled = constrain(s6_command_scaled, -1.0, 1.0);
    s7_command_scaled = constrain(s7_command_scaled, -1.0, 1.0);
}

//========================================================================================================================//
//                                              ARMING STATUS                                                              //
//========================================================================================================================//

void armedStatus() {
    static bool was_armed = false;

    // Check arming conditions
    bool throttle_low = (channel_3_pwm < 1050);
    bool throttle_cut_off = (channel_5_pwm < 1500);

    if (!armedFly && throttle_low && throttle_cut_off) {
        // Conditions met to arm
        armedFly = true;

        // Reset integrators on arming
        integral_roll = 0.0;
        integral_pitch = 0.0;
        integral_yaw = 0.0;

        Serial.println(F("*** ARMED ***"));
    }

    // Disarm if throttle cut is activated
    if (armedFly && !throttle_cut_off) {
        armedFly = false;
        Serial.println(F("*** DISARMED ***"));
    }

    // Status LED indication
    if (armedFly && !was_armed) {
        // Just armed - fast blink
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
            delay(50);
        }
    }

    was_armed = armedFly;
}
