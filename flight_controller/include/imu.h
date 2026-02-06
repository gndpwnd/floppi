/*
 * IMU Module Header
 * IMU data acquisition and attitude estimation
 */

#ifndef IMU_H
#define IMU_H

#include <Arduino.h>
#include "config.h"

// IMU hardware setup (call from setup())
void setupIMU();

// Read and filter IMU data
void getIMUdata();

// Madgwick 6DOF attitude filter (no magnetometer)
void Madgwick6DOF(float gx, float gy, float gz, float ax, float ay, float az, float invSampleFreq);

// Madgwick 9DOF attitude filter (with magnetometer)
void Madgwick(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz, float invSampleFreq);

// Fast inverse square root
float invSqrt(float x);

#endif // IMU_H
