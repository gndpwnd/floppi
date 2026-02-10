/*
 * Radio Communication Module
 * Complete interface for all receiver protocols
 * Based on original dRehmFlight - all features included
 */

#ifndef RADIO_COMM_H
#define RADIO_COMM_H

#include <Arduino.h>
#include "config.h"

// Serial port definitions for receivers
#ifdef USE_SBUS_RECEIVER
    #ifdef USE_ESP32
        // ESP32: SBUS uses Serial2 (configurable pins)
        #define SBUS_SERIAL_PORT Serial2
    #else
        // Teensy: SBUS uses Serial5 (pins 20 TX, 21 RX)
        #define SBUS_SERIAL_PORT Serial5
    #endif
#endif

#ifdef USE_DSM_RECEIVER
    #ifdef USE_ESP32
        // ESP32: DSM uses Serial1 (configurable pins)
        #define DSM_SERIAL_PORT Serial1
    #else
        // Teensy: DSM uses Serial3 (pins 14 TX, 15 RX)
        #define DSM_SERIAL_PORT Serial3
    #endif
#endif

#ifdef USE_IBUS_RECEIVER
    #ifdef USE_ESP32
        // ESP32: iBUS uses Serial1 (configurable pins)
        #define IBUS_SERIAL_PORT Serial1
    #else
        // Teensy: iBUS uses Serial3 (pins 14 TX, 15 RX)
        #define IBUS_SERIAL_PORT Serial3
    #endif
#endif

#ifdef USE_SERIAL_COMMANDS
    #ifdef USE_ESP32
        // ESP32: Serial commands use Serial1 (configurable pins)
        #define SERIAL_CMD_PORT Serial1
    #else
        // Teensy: Serial commands use Serial3 (pins 14 TX, 15 RX)
        #define SERIAL_CMD_PORT Serial3
    #endif
#endif

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

// WiFi API command routing — web server on Core 1 feeds commands to RadioComm on Core 0
// Called by web_server.cpp when POST /api/commands or WebSocket commands arrive.
// Values are 1000-2000 microseconds, same as all other command sources.
#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
void setWifiCommandChannels(uint16_t ch1, uint16_t ch2, uint16_t ch3, uint16_t ch4, uint16_t ch5, uint16_t ch6);
#endif

#endif // RADIO_COMM_H