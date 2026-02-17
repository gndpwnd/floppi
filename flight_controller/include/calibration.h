/*
 * Calibration Module - Header File
 * Auto-calibration for Radio and IMU with step-by-step guidance
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "config.h"

// Forward declarations of functions defined elsewhere (C++ linkage)
void getIMUdata();
void getCommands();

// External variables from radioComm.cpp
extern unsigned long channel_1_pwm, channel_2_pwm, channel_3_pwm;
extern unsigned long channel_4_pwm, channel_5_pwm, channel_6_pwm;

// External IMU data variables from main.cpp
extern float AccX, AccY, AccZ;
extern float GyroX, GyroY, GyroZ;

// ======================================================================
// CALIBRATION RESULTS ACCUMULATOR
// ======================================================================
// Stores results from each calibration so 'd' can dump everything at once.

struct CalibrationResults {
    // IMU offsets
    bool hasIMU;
    float accErrorX, accErrorY, accErrorZ;
    float gyroErrorX, gyroErrorY, gyroErrorZ;

    // IMU scale factors (from 6-position only)
    bool hasIMUScale;
    float accScaleX, accScaleY, accScaleZ;

    // IMU orientation
    bool hasOrientation;
    char forwardAxis, rightAxis, upAxis;
    bool forwardInverted, rightInverted, upInverted;

    // Radio channel mapping
    bool hasRadio;
    int throttleChannel, rollChannel, pitchChannel, yawChannel, aux1Channel, aux2Channel;

    // Failsafe values
    bool hasFailsafe;
    uint32_t failsafe[6];

    // PID gains (from runtime tuning)
    bool hasPID;

    // Filter params
    bool hasFilters;

    #ifdef USE_MPU9250
    // Magnetometer
    bool hasMag;
    float magOffsetX, magOffsetY, magOffsetZ;
    float magScaleX, magScaleY, magScaleZ;
    #endif
};

extern CalibrationResults calResults;

// ======================================================================
// HELPER FUNCTIONS
// ======================================================================

/**
 * Wait for user confirmation (y/n)
 * Blocks indefinitely until user types 'y' or 'n'. LED blinks while waiting.
 * @return true if confirmed, false if cancelled
 */
bool waitForConfirmation();

/**
 * Detect which channel has moved the most from neutral positions
 * @param neutral1..neutral6 Neutral values for channels 1-6
 * @param current1..current6 Current values for channels 1-6
 * @param exclude1..exclude3 Channel indices to exclude from detection
 * @return Channel index (0-5) with largest change, -1 if none
 */
int detectMovedChannel(uint16_t neutral1, uint16_t current1,
                      uint16_t neutral2, uint16_t current2,
                      uint16_t neutral3, uint16_t current3,
                      uint16_t neutral4, uint16_t current4,
                      uint16_t neutral5, uint16_t current5,
                      uint16_t neutral6, uint16_t current6,
                      int exclude1 = -1, int exclude2 = -1, int exclude3 = -1,
                      int exclude4 = -1, int exclude5 = -1);

/**
 * Get current PWM value for a specific channel
 * @param channel Channel index (0-5)
 * @return Current PWM value
 */
uint16_t getChannelValue(int channel);

// ======================================================================
// RADIO CALIBRATION
// ======================================================================

/**
 * Step-by-step radio calibration with channel auto-detection
 * Guides user through each control and detects which physical
 * control maps to which channel
 */
void calibrateRadio();

/**
 * Print radio calibration results with copy-paste ready code
 * @param throttleChannel Detected throttle channel (0-5)
 * @param rollChannel Detected roll channel (0-5)
 * @param pitchChannel Detected pitch channel (0-5)
 * @param yawChannel Detected yaw channel (0-5)
 * @param aux1Channel Detected aux1 channel (0-5)
 * @param aux2Channel Detected aux2 channel (0-5)
 */
void printRadioCalibrationResults(int throttleChannel, int rollChannel, int pitchChannel, 
                                  int yawChannel, int aux1Channel, int aux2Channel);

// ======================================================================
// IMU CALIBRATION
// ======================================================================

/**
 * Basic IMU offset calibration
 * Calibrates gyro and accelerometer offsets on level surface
 */
void calibrateIMU();

/**
 * Print IMU calibration results with copy-paste ready code
 * @param AccErrorX Accelerometer X error
 * @param AccErrorY Accelerometer Y error
 * @param AccErrorZ Accelerometer Z error
 * @param GyroErrorX Gyroscope X error
 * @param GyroErrorY Gyroscope Y error
 * @param GyroErrorZ Gyroscope Z error
 */
void printIMUCalibrationResults(float AccErrorX, float AccErrorY, float AccErrorZ,
                                float GyroErrorX, float GyroErrorY, float GyroErrorZ);

// ======================================================================
// 6-POSITION ACCELEROMETER CALIBRATION
// ======================================================================

/**
 * 6-position accelerometer calibration
 * Measures gravity in 6 orientations to calculate both offset
 * and scale factor for each axis. More accurate than single-position.
 */
void calibrateIMU6Position();

/**
 * Print 6-position calibration results with copy-paste ready code
 */
void print6PositionCalibrationResults(float AccOffsetX, float AccOffsetY, float AccOffsetZ,
                                       float AccScaleX, float AccScaleY, float AccScaleZ,
                                       float GyroErrorX, float GyroErrorY, float GyroErrorZ);

// ======================================================================
// IMU ORIENTATION DETECTION
// ======================================================================

/**
 * Advanced IMU calibration with orientation auto-detection
 * Guides user through physical orientations to auto-detect
 * how the IMU is mounted on the aircraft
 */
void calibrateIMUWithOrientation();

/**
 * Print IMU orientation results with axis transformation code
 * @param forwardAxis Which IMU axis corresponds to aircraft forward
 * @param forwardInverted Whether forward axis is inverted
 * @param rightAxis Which IMU axis corresponds to aircraft right
 * @param rightInverted Whether right axis is inverted
 * @param upAxis Which IMU axis corresponds to aircraft up
 * @param upInverted Whether up axis is inverted
 */
void printIMUOrientationResults(char forwardAxis, bool forwardInverted,
                                char rightAxis, bool rightInverted,
                                char upAxis, bool upInverted);

// ======================================================================
// FAILSAFE AUTO-DETECTION
// ======================================================================

/**
 * Auto-detect receiver failsafe PWM values.
 * Guides user to power off transmitter, then measures what the
 * receiver outputs when signal is lost. Outputs FAILSAFE_* defines.
 */
void calibrateFailsafe();

// ======================================================================
// ESC ENDPOINT CALIBRATION
// ======================================================================

/**
 * ESC endpoint calibration routine.
 * Sends min/max PWM to all motor outputs for ESC range calibration.
 * Standard procedure: max PWM → power ESCs → ESCs beep → min PWM → confirm.
 */
void calibrateESC();

// External motor functions (from motors.cpp / globals.h)
void commandMotors();

// ======================================================================
// DUMP ALL CALIBRATION VALUES
// ======================================================================

#ifdef CALIBRATION_MODE
/**
 * Print ALL calibration values collected during this session.
 * Outputs a single copy-paste-ready block for config.h.
 * Only prints sections that were actually calibrated.
 * PID gains and filter params are always included (from runtime values).
 * Triggered by serial command 'd'.
 */
void printAllCalibrationValues();
#endif

// ======================================================================
// MAGNETOMETER CALIBRATION (MPU9250 only)
// ======================================================================

#ifdef USE_MPU9250
/**
 * Magnetometer sphere calibration routine.
 * User rotates aircraft in all orientations while firmware records
 * min/max per axis. Calculates hard-iron offsets and soft-iron scales.
 * Outputs MAG_ERROR_X/Y/Z, MAG_SCALE_X/Y/Z defines.
 */
void calibrateMagnetometer();
#endif

#endif // CALIBRATION_H