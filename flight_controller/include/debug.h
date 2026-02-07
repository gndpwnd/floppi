/*
 * Debug Module Header
 * Debug print functions (CALIBRATION_MODE only)
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "config.h"

#ifdef CALIBRATION_MODE

// Print radio channel values
void printRadioData();

// Print desired state values
void printDesiredState();

// Print gyroscope data
void printGyroData();

// Print accelerometer data
void printAccelData();

// Print magnetometer data
void printMagData();

// Print roll/pitch/yaw angles
void printRollPitchYaw();

// Print PID outputs
void printPIDoutput();

// Print motor commands
void printMotorCommands();

// Print servo commands
void printServoCommands();

// Print loop rate
void printLoopRate();

// ============================================================================
// FC_TOOL TELEMETRY OUTPUT
// ============================================================================
// These functions output data in formats compatible with fc_tool's parser.
// See: fc_tool/docs/features/serial-telemetry-protocol.md

// Print IMU data in fc_tool format: ax=X ay=Y az=Z gx=X gy=Y gz=Z
void printIMUTelemetry();

// Print attitude in fc_tool format: roll=X pitch=Y yaw=Z
void printAttitudeTelemetry();

// Print full telemetry packet (IMU + attitude + motors)
void printFullTelemetry();

#endif // CALIBRATION_MODE

#endif // DEBUG_H
