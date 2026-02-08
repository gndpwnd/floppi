/*
 * Control Module Implementation
 * PID controllers and mixer
 */

#include "control.h"
#include "globals.h"
#include "config.h"
#include "pin_definitions.h"
#include "radioComm.h"

#ifdef USE_OPTIMIZATION
#include "filters.h"
#endif

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

    #ifdef USE_RACING
    // Expo curves: gentle near center, aggressive at extremes
    // Formula: output = (1 - expo) * input + expo * input^3
    if (EXPO_ROLL > 0.0f)
        roll_des = (1.0f - EXPO_ROLL) * roll_des + EXPO_ROLL * roll_des * roll_des * roll_des;
    if (EXPO_PITCH > 0.0f)
        pitch_des = (1.0f - EXPO_PITCH) * pitch_des + EXPO_PITCH * pitch_des * pitch_des * pitch_des;
    if (EXPO_YAW > 0.0f)
        yaw_des = (1.0f - EXPO_YAW) * yaw_des + EXPO_YAW * yaw_des * yaw_des * yaw_des;
    #endif

    #ifdef USE_RATE_CONTROLLER
        roll_des *= MAX_ROLL_RATE;
        pitch_des *= MAX_PITCH_RATE;
        yaw_des *= MAX_YAW_RATE;
    #else
        roll_des *= MAX_ROLL_ANGLE;
        pitch_des *= MAX_PITCH_ANGLE;
        yaw_des *= MAX_YAW_RATE;
    #endif

    #ifdef USE_RACING
    #if SETPOINT_SMOOTH_CUTOFF_HZ > 0
    // Setpoint smoothing: PT1 low-pass on stick input for smoother transitions
    static float sp_coeff = 0;
    static bool sp_init = false;
    if (!sp_init) {
        float tau = 1.0f / (2.0f * 3.14159265f * SETPOINT_SMOOTH_CUTOFF_HZ);
        float dt_nominal = 1.0f / LOOP_FREQUENCY_HZ;
        sp_coeff = dt_nominal / (tau + dt_nominal);
        sp_init = true;
    }
    static float roll_sp_prev = 0, pitch_sp_prev = 0, yaw_sp_prev = 0;
    roll_des = roll_sp_prev + sp_coeff * (roll_des - roll_sp_prev);
    pitch_des = pitch_sp_prev + sp_coeff * (pitch_des - pitch_sp_prev);
    yaw_des = yaw_sp_prev + sp_coeff * (yaw_des - yaw_sp_prev);
    roll_sp_prev = roll_des;
    pitch_sp_prev = pitch_des;
    yaw_sp_prev = yaw_des;
    #endif
    #endif

    roll_passthru = (channel_1_pwm - 1500.0) / 500.0;
    pitch_passthru = (channel_2_pwm - 1500.0) / 500.0;
    yaw_passthru = (channel_4_pwm - 1500.0) / 500.0;
}

//========================================================================================================================//
//                                              RATE CONTROLLER                                                            //
//========================================================================================================================//

void controlRATE() {
    // Previous gyro values for derivative-on-measurement
    // (GyroX_prev is already overwritten by imu.cpp LP filter, so we track separately)
    static float gyro_roll_prev = 0, gyro_pitch_prev = 0, gyro_yaw_prev = 0;

    #ifdef USE_OPTIMIZATION
    // Biquad D-term filter (replaces PT1 — steeper rolloff)
    static BiquadFilter dterm_lpf[3];
    static bool dterm_init = false;
    if (!dterm_init) {
        for (int i = 0; i < 3; i++)
            biquad_init_lpf(&dterm_lpf[i], DTERM_LPF_CUTOFF_HZ, LOOP_FREQUENCY_HZ);
        dterm_init = true;
    }
    #else
    // PT1 D-term filter (base tier)
    static float d_roll_prev = 0, d_pitch_prev = 0, d_yaw_prev = 0;
    #endif

    #ifdef USE_RACING
    static float roll_des_prev = 0, pitch_des_prev = 0, yaw_des_prev = 0;
    #endif

    // Roll
    error_roll = roll_des - GyroX;
    integral_roll += error_roll * dt;
    integral_roll = constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL);
    derivative_roll = -(GyroX - gyro_roll_prev) / dt;
    #ifdef USE_OPTIMIZATION
    derivative_roll = biquad_apply(&dterm_lpf[0], derivative_roll);
    #else
    derivative_roll = (1.0f - B_DTERM) * d_roll_prev + B_DTERM * derivative_roll;
    d_roll_prev = derivative_roll;
    #endif
    roll_PID = KP_ROLL_RATE * error_roll + KI_ROLL_RATE * integral_roll + KD_ROLL_RATE * derivative_roll;

    // Pitch
    error_pitch = pitch_des - GyroY;
    integral_pitch += error_pitch * dt;
    integral_pitch = constrain(integral_pitch, -I_LIMIT_PITCH, I_LIMIT_PITCH);
    derivative_pitch = -(GyroY - gyro_pitch_prev) / dt;
    #ifdef USE_OPTIMIZATION
    derivative_pitch = biquad_apply(&dterm_lpf[1], derivative_pitch);
    #else
    derivative_pitch = (1.0f - B_DTERM) * d_pitch_prev + B_DTERM * derivative_pitch;
    d_pitch_prev = derivative_pitch;
    #endif
    pitch_PID = KP_PITCH_RATE * error_pitch + KI_PITCH_RATE * integral_pitch + KD_PITCH_RATE * derivative_pitch;

    // Yaw
    error_yaw = yaw_des - GyroZ;
    integral_yaw += error_yaw * dt;
    integral_yaw = constrain(integral_yaw, -I_LIMIT_YAW, I_LIMIT_YAW);
    derivative_yaw = -(GyroZ - gyro_yaw_prev) / dt;
    #ifdef USE_OPTIMIZATION
    derivative_yaw = biquad_apply(&dterm_lpf[2], derivative_yaw);
    #else
    derivative_yaw = (1.0f - B_DTERM) * d_yaw_prev + B_DTERM * derivative_yaw;
    d_yaw_prev = derivative_yaw;
    #endif
    yaw_PID = KP_YAW_RATE * error_yaw + KI_YAW_RATE * integral_yaw + KD_YAW_RATE * derivative_yaw;

    #ifdef USE_RACING
    // Feed-forward: add setpoint derivative to PID output for faster stick response
    if (FF_ROLL > 0.0f)  roll_PID += FF_ROLL * (roll_des - roll_des_prev) / dt;
    if (FF_PITCH > 0.0f) pitch_PID += FF_PITCH * (pitch_des - pitch_des_prev) / dt;
    if (FF_YAW > 0.0f)   yaw_PID += FF_YAW * (yaw_des - yaw_des_prev) / dt;
    roll_des_prev = roll_des;
    pitch_des_prev = pitch_des;
    yaw_des_prev = yaw_des;

    // TPA: reduce PID output at high throttle to prevent oscillation
    if (thro_des > TPA_BREAKPOINT) {
        float tpa = 1.0f - TPA_RATE * (thro_des - TPA_BREAKPOINT) / (1.0f - TPA_BREAKPOINT);
        roll_PID *= tpa;
        pitch_PID *= tpa;
        yaw_PID *= tpa;
    }
    #endif

    // Save gyro values for next iteration
    gyro_roll_prev = GyroX;
    gyro_pitch_prev = GyroY;
    gyro_yaw_prev = GyroZ;
}

//========================================================================================================================//
//                                              ANGLE CONTROLLER                                                           //
//========================================================================================================================//

void controlANGLE() {
    // Previous gyro value for yaw derivative-on-measurement
    static float gyro_yaw_prev = 0;

    #ifdef USE_OPTIMIZATION
    // Biquad D-term filter (replaces PT1 — steeper rolloff)
    static BiquadFilter dterm_lpf[3];
    static bool dterm_init = false;
    if (!dterm_init) {
        for (int i = 0; i < 3; i++)
            biquad_init_lpf(&dterm_lpf[i], DTERM_LPF_CUTOFF_HZ, LOOP_FREQUENCY_HZ);
        dterm_init = true;
    }
    #else
    // PT1 D-term filter (base tier)
    static float d_roll_prev = 0, d_pitch_prev = 0, d_yaw_prev = 0;
    #endif

    #ifdef USE_RACING
    static float roll_des_prev = 0, pitch_des_prev = 0, yaw_des_prev = 0;
    #endif

    // Roll (derivative on measurement: uses -GyroX directly)
    error_roll = roll_des - roll_IMU;
    integral_roll += error_roll * dt;
    integral_roll = constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL);
    derivative_roll = -GyroX;
    #ifdef USE_OPTIMIZATION
    derivative_roll = biquad_apply(&dterm_lpf[0], derivative_roll);
    #else
    derivative_roll = (1.0f - B_DTERM) * d_roll_prev + B_DTERM * derivative_roll;
    d_roll_prev = derivative_roll;
    #endif
    roll_PID = KP_ROLL_ANGLE * error_roll + KI_ROLL_ANGLE * integral_roll + KD_ROLL_ANGLE * derivative_roll;

    // Pitch (derivative on measurement: uses -GyroY directly)
    error_pitch = pitch_des - pitch_IMU;
    integral_pitch += error_pitch * dt;
    integral_pitch = constrain(integral_pitch, -I_LIMIT_PITCH, I_LIMIT_PITCH);
    derivative_pitch = -GyroY;
    #ifdef USE_OPTIMIZATION
    derivative_pitch = biquad_apply(&dterm_lpf[1], derivative_pitch);
    #else
    derivative_pitch = (1.0f - B_DTERM) * d_pitch_prev + B_DTERM * derivative_pitch;
    d_pitch_prev = derivative_pitch;
    #endif
    pitch_PID = KP_PITCH_ANGLE * error_pitch + KI_PITCH_ANGLE * integral_pitch + KD_PITCH_ANGLE * derivative_pitch;

    // Yaw (rate control — derivative on measurement)
    error_yaw = yaw_des - GyroZ;
    integral_yaw += error_yaw * dt;
    integral_yaw = constrain(integral_yaw, -I_LIMIT_YAW, I_LIMIT_YAW);
    derivative_yaw = -(GyroZ - gyro_yaw_prev) / dt;
    #ifdef USE_OPTIMIZATION
    derivative_yaw = biquad_apply(&dterm_lpf[2], derivative_yaw);
    #else
    derivative_yaw = (1.0f - B_DTERM) * d_yaw_prev + B_DTERM * derivative_yaw;
    d_yaw_prev = derivative_yaw;
    #endif
    yaw_PID = KP_YAW_RATE * error_yaw + KI_YAW_RATE * integral_yaw + KD_YAW_RATE * derivative_yaw;

    #ifdef USE_RACING
    // Feed-forward: add setpoint derivative to PID output for faster stick response
    if (FF_ROLL > 0.0f)  roll_PID += FF_ROLL * (roll_des - roll_des_prev) / dt;
    if (FF_PITCH > 0.0f) pitch_PID += FF_PITCH * (pitch_des - pitch_des_prev) / dt;
    if (FF_YAW > 0.0f)   yaw_PID += FF_YAW * (yaw_des - yaw_des_prev) / dt;
    roll_des_prev = roll_des;
    pitch_des_prev = pitch_des;
    yaw_des_prev = yaw_des;

    // TPA: reduce PID output at high throttle to prevent oscillation
    if (thro_des > TPA_BREAKPOINT) {
        float tpa = 1.0f - TPA_RATE * (thro_des - TPA_BREAKPOINT) / (1.0f - TPA_BREAKPOINT);
        roll_PID *= tpa;
        pitch_PID *= tpa;
        yaw_PID *= tpa;
    }
    #endif

    gyro_yaw_prev = GyroZ;
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

    #if defined(USE_RACING) && defined(USE_AIRMODE)
    // Air mode: keep PID active at zero throttle for full attitude control
    // Shift motor outputs to preserve PID corrections within [0, 1] range
    float mix_min = min(min(m1_command_scaled, m2_command_scaled), min(m3_command_scaled, m4_command_scaled));
    float mix_max = max(max(m1_command_scaled, m2_command_scaled), max(m3_command_scaled, m4_command_scaled));
    if (mix_min < 0.0f) {
        float shift = -mix_min;
        m1_command_scaled += shift;
        m2_command_scaled += shift;
        m3_command_scaled += shift;
        m4_command_scaled += shift;
        mix_max += shift;
    }
    if (mix_max > 1.0f) {
        float shift = mix_max - 1.0f;
        m1_command_scaled -= shift;
        m2_command_scaled -= shift;
        m3_command_scaled -= shift;
        m4_command_scaled -= shift;
    }
    #endif

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
