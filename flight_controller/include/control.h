/*
 * Control Module Header
 * PID controllers and mixer
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <Arduino.h>

// Get desired state from radio inputs
void getDesState();

// Rate mode PID controller
void controlRATE();

// Angle mode PID controller
void controlANGLE();

// Motor/servo mixer
void controlMixer();

// Arming status check
void armedStatus();

#endif // CONTROL_H
