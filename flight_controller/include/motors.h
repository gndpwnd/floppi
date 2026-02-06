/*
 * Motors Module Header
 * Motor and servo control
 */

#ifndef MOTORS_H
#define MOTORS_H

#include <Arduino.h>

// Setup motor pins and servos
void setupMotors();

// Scale commands to PWM values
void scaleCommands();

// Apply throttle cut if needed
void throttleCut();

// Arm ESCs
void armMotors();

// Send commands to motors and servos
void commandMotors();

// Loop rate control
void loopRate(int freq);

#endif // MOTORS_H
