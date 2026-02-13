/*
 * Radio Communication Module
 * Interface for all receiver protocols and command sources.
 * Supports single-source (default) and multi-source with arbitration (USE_COMMAND_ARBITRATION).
 * Based on original dRehmFlight — all features included.
 *
 * Files:
 *   radioComm.h        — Types, validation, public API
 *   radioComm.cpp      — Core: channels, dispatch, arbitration, failsafe
 *   radioComm_rc.cpp   — RC protocols: SBUS, iBUS, DSM, PPM, PWM
 *   radioComm_ext.cpp  — External sources: Serial, I2C, WiFi
 */

#ifndef RADIO_COMM_H
#define RADIO_COMM_H

#include <Arduino.h>
#include "config.h"

//========================================================================================================================//
//                                         COMMAND BUFFER & SOURCE TYPES                                                   //
//========================================================================================================================//

// Per-source command buffer — each source writes here, arbitration reads
struct CommandBuffer {
    uint16_t channels[6];   // Channel values in microseconds (1000-2000)
    uint32_t timestamp;     // millis() of last valid update
    bool     active;        // Source is providing data (not timed out / faulted)
};

// Command source identifier for arbitration
enum CommandSource : uint8_t {
    CMD_SRC_NONE   = 0,
    CMD_SRC_RC     = 1,  // Any RC receiver (SBUS, iBUS, DSM, PPM, PWM)
    CMD_SRC_SERIAL = 2,  // External flight computer over UART
    CMD_SRC_I2C    = 3,  // External flight computer over I2C
    CMD_SRC_WIFI   = 4   // WiFi API commands (ESP32)
};

// Override timeout — source considered inactive after this many ms with no data
#define OVERRIDE_TIMEOUT_MS 500

//========================================================================================================================//
//                                         COMPILE-TIME VALIDATION                                                         //
//========================================================================================================================//

// At most one RC receiver protocol
#if (defined(USE_SBUS_RECEIVER) + defined(USE_IBUS_RECEIVER) + defined(USE_DSM_RECEIVER) + defined(USE_PPM_RECEIVER) + defined(USE_PWM_RECEIVER)) > 1
    #error "Only one RC receiver protocol can be defined at a time!"
#endif

// Multiple command sources require arbitration
#ifndef USE_COMMAND_ARBITRATION
    #if (defined(USE_SBUS_RECEIVER) + defined(USE_IBUS_RECEIVER) + defined(USE_DSM_RECEIVER) + defined(USE_PPM_RECEIVER) + defined(USE_PWM_RECEIVER) + defined(USE_SERIAL_COMMANDS) + defined(USE_I2C_COMMANDS)) > 1
        #error "Multiple command sources defined — enable USE_COMMAND_ARBITRATION in config.h!"
    #endif
#endif

// At least one command source must be defined (except in calibration mode)
#if !(defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER) || defined(USE_SERIAL_COMMANDS) || defined(USE_I2C_COMMANDS)) && !(defined(USE_ESP32) && defined(USE_WEB_SERVER))
    #ifndef CALIBRATION_MODE
        #error "No receiver or command source defined in config.h!"
    #endif
#endif

//========================================================================================================================//
//                                         SERIAL PORT DEFINITIONS                                                         //
//========================================================================================================================//

#ifdef USE_SBUS_RECEIVER
    #ifdef USE_ESP32
        #define SBUS_SERIAL_PORT Serial2
    #else
        #define SBUS_SERIAL_PORT Serial5
    #endif
#endif

#ifdef USE_DSM_RECEIVER
    #ifdef USE_ESP32
        #define DSM_SERIAL_PORT Serial1
    #else
        #define DSM_SERIAL_PORT Serial3
    #endif
#endif

#ifdef USE_IBUS_RECEIVER
    #ifdef USE_ESP32
        #define IBUS_SERIAL_PORT Serial1
    #else
        #define IBUS_SERIAL_PORT Serial3
    #endif
#endif

#ifdef USE_SERIAL_COMMANDS
    #ifdef USE_ESP32
        #define SERIAL_CMD_PORT Serial1
    #else
        #define SERIAL_CMD_PORT Serial3
    #endif
#endif

#ifdef USE_I2C_COMMANDS
    #include <Wire.h>
#endif

//========================================================================================================================//
//                                              PUBLIC API                                                                 //
//========================================================================================================================//

void radioSetup();
void getCommands();
void failSafe();
bool isReceiverConnected();
unsigned long getRadioPWM(int ch_num);
void serialEvent3();

// External channel variables (microseconds: 1000-2000)
extern unsigned long channel_1_pwm, channel_2_pwm, channel_3_pwm;
extern unsigned long channel_4_pwm, channel_5_pwm, channel_6_pwm;

// Active command source (only with arbitration)
#ifdef USE_COMMAND_ARBITRATION
extern CommandSource activeSource;
#endif

//========================================================================================================================//
//                                   PER-SOURCE FUNCTIONS (radioComm_rc.cpp / radioComm_ext.cpp)                           //
//========================================================================================================================//

// --- RC receiver protocols (radioComm_rc.cpp) ---

#ifdef USE_SBUS_RECEIVER
void sbusSetup();
bool readSBUS(CommandBuffer& buf);
#endif

#ifdef USE_IBUS_RECEIVER
void ibusSetup();
bool readIBUS(CommandBuffer& buf);
#endif

#ifdef USE_DSM_RECEIVER
void dsmSetup();
bool readDSM(CommandBuffer& buf);
void dsmSerialEvent();
#endif

#ifdef USE_PPM_RECEIVER
void ppmSetup();
bool readPPM(CommandBuffer& buf);
#endif

#ifdef USE_PWM_RECEIVER
void pwmSetup();
bool readPWM(CommandBuffer& buf);
#endif

// --- External command sources (radioComm_ext.cpp) ---

#ifdef USE_SERIAL_COMMANDS
void serialCmdSetup();
bool readSerialCmd(CommandBuffer& buf);
#endif

#ifdef USE_I2C_COMMANDS
void i2cCmdSetup();
bool readI2CCmd(CommandBuffer& buf);
#endif

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
void wifiCmdSetup();
bool readWifiCmd(CommandBuffer& buf);
void setWifiCommandChannels(uint16_t ch1, uint16_t ch2, uint16_t ch3, uint16_t ch4, uint16_t ch5, uint16_t ch6);
#endif

#endif // RADIO_COMM_H
