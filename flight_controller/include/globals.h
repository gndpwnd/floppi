/*
 * Global Variables Header
 * Shared state for flight controller modules
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

// PWMServo is Teensy-specific; ESP32 uses LEDC directly
#ifndef USE_ESP32
    #include <PWMServo.h>
#endif

//========================================================================================================================//
//                                                    TIMING                                                               //
//========================================================================================================================//

extern unsigned long current_time, prev_time;
extern unsigned long print_counter;
extern float dt;

//========================================================================================================================//
//                                                   IMU DATA                                                              //
//========================================================================================================================//

// Raw IMU data
extern int16_t AcX, AcY, AcZ, GyX, GyY, GyZ, MgX, MgY, MgZ;

// Processed IMU data (g's and deg/s)
extern float AccX, AccY, AccZ;
extern float AccX_prev, AccY_prev, AccZ_prev;
extern float GyroX, GyroY, GyroZ;
extern float GyroX_prev, GyroY_prev, GyroZ_prev;
extern float MagX, MagY, MagZ;
// MagX_prev/MagY_prev/MagZ_prev removed alongside the dead 9DOF LPF in
// imu.cpp (Madgwick(9DOF) currently falls through to 6DOF — no real consumer
// of mag-filtered data). The defining symbols in main.cpp persist but are now
// unused TU-local globals; can be removed when main.cpp is in scope.

// IMU calibration offsets
extern float AccErrorX, AccErrorY, AccErrorZ;
extern float GyroErrorX, GyroErrorY, GyroErrorZ;

//========================================================================================================================//
//                                                  ATTITUDE                                                               //
//========================================================================================================================//

// Madgwick quaternion
extern float q0, q1, q2, q3;

// Roll/pitch/yaw angles (degrees)
extern float roll_IMU, pitch_IMU, yaw_IMU;
extern float roll_IMU_prev, pitch_IMU_prev;

//========================================================================================================================//
//                                                CONTROL STATE                                                            //
//========================================================================================================================//

// Desired states from radio
extern float thro_des, roll_des, pitch_des, yaw_des;
extern float roll_passthru, pitch_passthru, yaw_passthru;

// PID state
extern float error_roll, error_roll_prev, integral_roll, integral_roll_prev, derivative_roll, roll_PID;
extern float error_pitch, error_pitch_prev, integral_pitch, integral_pitch_prev, derivative_pitch, pitch_PID;
extern float error_yaw, error_yaw_prev, integral_yaw, integral_yaw_prev, derivative_yaw, yaw_PID;

//========================================================================================================================//
//                                               MOTOR/SERVO STATE                                                         //
//========================================================================================================================//

// Motor commands
extern int m1_command_PWM, m2_command_PWM, m3_command_PWM, m4_command_PWM, m5_command_PWM, m6_command_PWM;
extern float m1_command_scaled, m2_command_scaled, m3_command_scaled, m4_command_scaled, m5_command_scaled, m6_command_scaled;

// Servo objects (Teensy only - ESP32 uses LEDC directly)
#ifndef USE_ESP32
extern PWMServo servo1, servo2, servo3, servo4, servo5, servo6, servo7;
#endif

// Servo commands
extern int s1_command_PWM, s2_command_PWM, s3_command_PWM, s4_command_PWM, s5_command_PWM, s6_command_PWM, s7_command_PWM;
extern float s1_command_scaled, s2_command_scaled, s3_command_scaled, s4_command_scaled, s5_command_scaled, s6_command_scaled, s7_command_scaled;

//========================================================================================================================//
//                                                 SYSTEM STATE                                                            //
//========================================================================================================================//

// Arming state
extern bool armedFly;

//========================================================================================================================//
//                                              CALIBRATION STATE                                                          //
//========================================================================================================================//

#ifdef CALIBRATION_MODE
enum CalibrationMode {
    CALIB_NONE,
    CALIB_ACCEL_GYRO,      // Single-position IMU calibration (offsets only)
    CALIB_6POSITION,       // 6-position accelerometer calibration (offsets + scale)
    CALIB_ATTITUDE,        // IMU + orientation detection
    CALIB_RADIO,           // Radio channel mapping
    CALIB_FAILSAFE,        // Failsafe auto-detection
    CALIB_ESC,             // ESC endpoint calibration
    CALIB_SEQUENTIAL       // Sequential guided workflow ('a' command)
};
extern CalibrationMode calibration_mode;
extern bool calibration_in_progress;
extern unsigned long calibration_start_time;

// Telemetry output mode for fc_tool integration
// 0 = off, 1 = IMU only, 2 = full telemetry
extern int telemetry_mode;

// Runtime-tunable PID gains (adjust via serial 'g' command, copy to config.h when done)
extern float tune_kp_roll, tune_ki_roll, tune_kd_roll;
extern float tune_kp_pitch, tune_ki_pitch, tune_kd_pitch;
extern float tune_kp_yaw, tune_ki_yaw, tune_kd_yaw;

// Runtime-tunable filter coefficients and limits (adjust via serial 'p' command)
extern float tune_b_accel, tune_b_gyro, tune_b_dterm, tune_madgwick_beta;
extern float tune_max_roll, tune_max_pitch, tune_max_yaw;
#endif

#endif // GLOBALS_H
