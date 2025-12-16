/*
 * dRehmFlight VTOL Configuration
 * ALL flight parameters in one place
 */

#ifndef CONFIG_H
#define CONFIG_H

//========================================================================================================================//
//                                              IMU SELECTION                                                             //
//========================================================================================================================//

// Choose ONE IMU (comment out the one you're NOT using)
#define USE_MPU6050      // ← Uncomment for MPU6050 (I2C)
//#define USE_MPU9250    // ← Uncomment for MPU9250 (SPI)

//========================================================================================================================//
//                                          IMU SCALE SETTINGS (CRITICAL!)                                                //
//========================================================================================================================//

#ifdef USE_MPU6050
    // MPU6050 Gyroscope full scale range
    // Options: MPU6050_GYRO_FS_250, MPU6050_GYRO_FS_500, MPU6050_GYRO_FS_1000, MPU6050_GYRO_FS_2000
    #define GYRO_SCALE MPU6050_GYRO_FS_500
    
    // MPU6050 Accelerometer full scale range  
    // Options: MPU6050_ACCEL_FS_2, MPU6050_ACCEL_FS_4, MPU6050_ACCEL_FS_8, MPU6050_ACCEL_FS_16
    #define ACCEL_SCALE MPU6050_ACCEL_FS_2
    
    // Scale factors for converting raw values
    #define GYRO_SCALE_FACTOR  65.5    // For ±500°/s: 65536 / (2 * 500) = 65.5
    #define ACCEL_SCALE_FACTOR 16384.0 // For ±2g: 65536 / (2 * 2) = 16384
    
#elif defined(USE_MPU9250)
    // MPU9250 ranges
    #define GYRO_SCALE MPU9250::GYRO_RANGE_500DPS
    #define ACCEL_SCALE MPU9250::ACCEL_RANGE_2G
    
    #define GYRO_SCALE_FACTOR  65.5
    #define ACCEL_SCALE_FACTOR 16384.0
#endif

//========================================================================================================================//
//                                          RECEIVER SELECTION                                                            //
//========================================================================================================================//

// Choose ONE receiver protocol (comment out all others)
#define USE_SBUS_RECEIVER    // ← Most common (FlySky FS-iA6B, FrSky, etc.)
//#define USE_DSM_RECEIVER   // ← Spektrum DSM2/DSMX
//#define USE_PPM_RECEIVER   // ← Single wire PPM
//#define USE_PWM_RECEIVER   // ← Individual PWM channels

//========================================================================================================================//
//                                          RECEIVER SETTINGS                                                             //
//========================================================================================================================//

// SBUS Settings
#ifdef USE_SBUS_RECEIVER
    #define SBUS_SERIAL_PORT Serial5  // Teensy 4.0: RX5 = Pin 21
#endif

// DSM Settings  
#ifdef USE_DSM_RECEIVER
    #define DSM_SERIAL_PORT Serial3   // Teensy 4.0: RX3 = Pin 15
#endif

// PPM Settings
#ifdef USE_PPM_RECEIVER
    #define PPM_PIN 14  // Any interrupt-capable pin
#endif

// PWM Settings (individual channels)
#ifdef USE_PWM_RECEIVER
    #define PWM_CH1_PIN 14
    #define PWM_CH2_PIN 15
    #define PWM_CH3_PIN 16
    #define PWM_CH4_PIN 17
    #define PWM_CH5_PIN 18
    #define PWM_CH6_PIN 19
#endif

// Failsafe values (microseconds)
#define FAILSAFE_ROLL     1500  // Center
#define FAILSAFE_PITCH    1500  // Center
#define FAILSAFE_THROTTLE 1000  // Minimum
#define FAILSAFE_YAW      1500  // Center
#define FAILSAFE_AUX1     2000  // High = throttle cut
#define FAILSAFE_AUX2     1500  // Center

//========================================================================================================================//
//                                          CONTROLLER SELECTION                                                          //
//========================================================================================================================//

// Choose ONE controller mode
#define USE_RATE_CONTROLLER    // ← Acro mode (rate control)
//#define USE_ANGLE_CONTROLLER // ← Stabilize mode (self-leveling)

//========================================================================================================================//
//                                          LOOP FREQUENCY                                                                //
//========================================================================================================================//

#define LOOP_FREQUENCY_HZ 2000  // 2kHz = 500μs loop time

//========================================================================================================================//
//                                          FILTER COEFFICIENTS                                                           //
//========================================================================================================================//

// Low-pass filter coefficients (0.0 to 1.0)
// Lower = more filtering = smoother but slower response
// Higher = less filtering = faster but noisier
#define B_ACCEL 0.14  // Accelerometer filter
#define B_GYRO  0.10  // Gyroscope filter
#define B_MAG   0.10  // Magnetometer filter (MPU9250 only)

//========================================================================================================================//
//                                          MADGWICK FILTER                                                               //
//========================================================================================================================//

// Madgwick filter beta (0.0 to 1.0)
// Higher = trusts accelerometer more (fights gyro drift, but more noise)
// Lower = trusts gyroscope more (smooth, but drifts over time)
#define MADGWICK_BETA 0.04

//========================================================================================================================//
//                                          PID GAINS - RATE CONTROLLER                                                   //
//========================================================================================================================//

// Roll Rate PID
#define KP_ROLL_RATE  0.15
#define KI_ROLL_RATE  0.15
#define KD_ROLL_RATE  0.0004
#define I_LIMIT_ROLL  25.0

// Pitch Rate PID
#define KP_PITCH_RATE  0.15
#define KI_PITCH_RATE  0.15
#define KD_PITCH_RATE  0.0004
#define I_LIMIT_PITCH  25.0

// Yaw Rate PID
#define KP_YAW_RATE  0.30
#define KI_YAW_RATE  0.05
#define KD_YAW_RATE  0.00015
#define I_LIMIT_YAW   25.0

//========================================================================================================================//
//                                          PID GAINS - ANGLE CONTROLLER                                                  //
//========================================================================================================================//

// Roll Angle PID
#define KP_ROLL_ANGLE  0.20
#define KI_ROLL_ANGLE  0.00
#define KD_ROLL_ANGLE  0.05

// Pitch Angle PID  
#define KP_PITCH_ANGLE  0.20
#define KI_PITCH_ANGLE  0.00
#define KD_PITCH_ANGLE  0.05

//========================================================================================================================//
//                                          MAXIMUM RATES AND ANGLES                                                      //
//========================================================================================================================//

// Rate mode limits (degrees/second)
#define MAX_ROLL_RATE   360.0
#define MAX_PITCH_RATE  360.0
#define MAX_YAW_RATE    200.0

// Angle mode limits (degrees)
#define MAX_ROLL_ANGLE  45.0
#define MAX_PITCH_ANGLE 45.0

//========================================================================================================================//
//                                          MOTOR/ESC SETTINGS                                                            //
//========================================================================================================================//

// Choose motor protocol
//#define USE_ONESHOT125   // High-speed ESCs
#define USE_STANDARD_PWM   // Standard 1000-2000μs PWM (most common)

//========================================================================================================================//
//                                          LED PIN                                                                       //
//========================================================================================================================//

#define LED_PIN 13  // Teensy onboard LED

#endif // CONFIG_H