/*
 * Radio Communication Module
 * Complete interface for all receiver protocols
 * Based on original dRehmFlight - all features included
 */

#ifndef RADIO_COMM_H
#define RADIO_COMM_H

#include <Arduino.h>
#include "config.h"

// Initialize receiver hardware
void radioSetup();

// Read receiver channels (call this every loop)
void getCommands();

// Handle failsafe condition
void failSafe();

// Check if receiver is connected
bool isReceiverConnected();

// Get specific channel PWM value (1-6) - original dRehmFlight function
unsigned long getRadioPWM(int ch_num);

// DSM serial event handler (called automatically)
void serialEvent3();

// External channel variables (microseconds: 1000-2000)
extern unsigned long channel_1_pwm, channel_2_pwm, channel_3_pwm;
extern unsigned long channel_4_pwm, channel_5_pwm, channel_6_pwm;

#endif // RADIO_COMM_H