/*
 * Radio Communication Core
 * Command dispatch, arbitration, failsafe, and channel output.
 * Protocol-specific code is in radioComm_rc.cpp (RC) and radioComm_ext.cpp (external).
 */

#include "radioComm.h"
#include "pin_definitions.h"

//========================================================================================================================//
//                                              CHANNEL VARIABLES                                                         //
//========================================================================================================================//

unsigned long channel_1_pwm = 1500;
unsigned long channel_2_pwm = 1500;
unsigned long channel_3_pwm = 1000;  // Throttle starts at minimum
unsigned long channel_4_pwm = 1500;
unsigned long channel_5_pwm = 1000;
unsigned long channel_6_pwm = 1000;

unsigned long lastFrameTime = 0;

//========================================================================================================================//
//                                         COMMAND BUFFERS (one per enabled source)                                        //
//========================================================================================================================//

// Safe defaults: center sticks, min throttle, aux off
#define SAFE_INIT {{1500, 1500, 1000, 1500, 1000, 1000}, 0, false}

#if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
static CommandBuffer rcBuffer = SAFE_INIT;
#endif

#ifdef USE_SERIAL_COMMANDS
static CommandBuffer serialCmdBuffer = SAFE_INIT;
#endif

#ifdef USE_I2C_COMMANDS
static CommandBuffer i2cCmdBuffer = SAFE_INIT;
#endif

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
static CommandBuffer wifiCmdBuffer = SAFE_INIT;
#endif

#ifdef USE_COMMAND_ARBITRATION
CommandSource activeSource = CMD_SRC_NONE;
#endif

//========================================================================================================================//
//                                    HELPER: Copy buffer channels to output variables                                     //
//========================================================================================================================//

static void applyBuffer(const CommandBuffer& buf) {
    channel_1_pwm = buf.channels[0];
    channel_2_pwm = buf.channels[1];
    channel_3_pwm = buf.channels[2];
    channel_4_pwm = buf.channels[3];
    channel_5_pwm = buf.channels[4];
    channel_6_pwm = buf.channels[5];
    lastFrameTime = buf.timestamp;
}

//========================================================================================================================//
//                                              RADIO SETUP                                                               //
//========================================================================================================================//

void radioSetup() {
    #ifdef USE_SBUS_RECEIVER
        sbusSetup();
    #endif
    #ifdef USE_IBUS_RECEIVER
        ibusSetup();
    #endif
    #ifdef USE_DSM_RECEIVER
        dsmSetup();
    #endif
    #ifdef USE_PPM_RECEIVER
        ppmSetup();
    #endif
    #ifdef USE_PWM_RECEIVER
        pwmSetup();
    #endif
    #ifdef USE_SERIAL_COMMANDS
        serialCmdSetup();
    #endif
    #ifdef USE_I2C_COMMANDS
        i2cCmdSetup();
    #endif
    #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
        wifiCmdSetup();
    #endif

    delay(100);
    lastFrameTime = millis();
}

//========================================================================================================================//
//                                              GET COMMANDS                                                              //
//========================================================================================================================//

void getCommands() {

    // Read all enabled sources into their buffers
    #ifdef USE_SBUS_RECEIVER
        readSBUS(rcBuffer);
    #endif
    #ifdef USE_IBUS_RECEIVER
        readIBUS(rcBuffer);
    #endif
    #ifdef USE_DSM_RECEIVER
        readDSM(rcBuffer);
    #endif
    #ifdef USE_PPM_RECEIVER
        readPPM(rcBuffer);
    #endif
    #ifdef USE_PWM_RECEIVER
        readPWM(rcBuffer);
    #endif
    #ifdef USE_SERIAL_COMMANDS
        readSerialCmd(serialCmdBuffer);
    #endif
    #ifdef USE_I2C_COMMANDS
        readI2CCmd(i2cCmdBuffer);
    #endif
    #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
        readWifiCmd(wifiCmdBuffer);
    #endif

    //--- Select and apply active source ---

    #ifdef USE_COMMAND_ARBITRATION
        // Priority: Serial > I2C > WiFi (overrides), RC (primary fallback)
        CommandBuffer* selected = nullptr;
        CommandSource  source   = CMD_SRC_NONE;

        #ifdef USE_SERIAL_COMMANDS
        if (serialCmdBuffer.active) {
            selected = &serialCmdBuffer;
            source   = CMD_SRC_SERIAL;
        }
        #endif

        #ifdef USE_I2C_COMMANDS
        if (!selected && i2cCmdBuffer.active) {
            selected = &i2cCmdBuffer;
            source   = CMD_SRC_I2C;
        }
        #endif

        #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
        if (!selected && wifiCmdBuffer.active) {
            selected = &wifiCmdBuffer;
            source   = CMD_SRC_WIFI;
        }
        #endif

        // RC as primary fallback
        #if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        if (!selected && rcBuffer.active) {
            selected = &rcBuffer;
            source   = CMD_SRC_RC;
        }
        #endif

        if (selected) {
            applyBuffer(*selected);
        }
        activeSource = source;

    #else
        // Single source — apply the one enabled buffer
        #if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
            applyBuffer(rcBuffer);
        #elif defined(USE_SERIAL_COMMANDS)
            applyBuffer(serialCmdBuffer);
        #elif defined(USE_I2C_COMMANDS)
            applyBuffer(i2cCmdBuffer);
        #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
            applyBuffer(wifiCmdBuffer);
        #endif
    #endif

    // Constrain all channels to valid range
    channel_1_pwm = constrain(channel_1_pwm, 1000UL, 2000UL);
    channel_2_pwm = constrain(channel_2_pwm, 1000UL, 2000UL);
    channel_3_pwm = constrain(channel_3_pwm, 1000UL, 2000UL);
    channel_4_pwm = constrain(channel_4_pwm, 1000UL, 2000UL);
    channel_5_pwm = constrain(channel_5_pwm, 1000UL, 2000UL);
    channel_6_pwm = constrain(channel_6_pwm, 1000UL, 2000UL);
}

//========================================================================================================================//
//                                         GET RADIO PWM (Original function)                                              //
//========================================================================================================================//

unsigned long getRadioPWM(int ch_num) {
    switch (ch_num) {
        case 1: return channel_1_pwm;
        case 2: return channel_2_pwm;
        case 3: return channel_3_pwm;
        case 4: return channel_4_pwm;
        case 5: return channel_5_pwm;
        case 6: return channel_6_pwm;
        default: return 1500;
    }
}

//========================================================================================================================//
//                                              FAILSAFE                                                                  //
//========================================================================================================================//

void failSafe() {
    bool signalLost = false;

    #ifdef USE_COMMAND_ARBITRATION
        // All sources must be inactive for full failsafe
        bool anyActive = false;
        #if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        if (rcBuffer.active) anyActive = true;
        #endif
        #ifdef USE_SERIAL_COMMANDS
        if (serialCmdBuffer.active) anyActive = true;
        #endif
        #ifdef USE_I2C_COMMANDS
        if (i2cCmdBuffer.active) anyActive = true;
        #endif
        #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
        if (wifiCmdBuffer.active) anyActive = true;
        #endif
        signalLost = !anyActive;
    #else
        // Single source
        #if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
            signalLost = !rcBuffer.active;
        #elif defined(USE_SERIAL_COMMANDS)
            signalLost = !serialCmdBuffer.active;
        #elif defined(USE_I2C_COMMANDS)
            signalLost = !i2cCmdBuffer.active;
        #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
            signalLost = !wifiCmdBuffer.active;
        #endif
    #endif

    if (signalLost) {
        channel_1_pwm = FAILSAFE_ROLL;
        channel_2_pwm = FAILSAFE_PITCH;
        channel_3_pwm = FAILSAFE_THROTTLE;
        channel_4_pwm = FAILSAFE_YAW;
        channel_5_pwm = FAILSAFE_AUX1;
        channel_6_pwm = FAILSAFE_AUX2;

        // Blink LED rapidly to indicate failsafe
        if (millis() % 200 < 100) {
            digitalWrite(LED_PIN, HIGH);
        } else {
            digitalWrite(LED_PIN, LOW);
        }
    }
}

//========================================================================================================================//
//                                        CHECK RECEIVER CONNECTION                                                       //
//========================================================================================================================//

bool isReceiverConnected() {
    #ifdef USE_COMMAND_ARBITRATION
        #if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        if (rcBuffer.active) return true;
        #endif
        #ifdef USE_SERIAL_COMMANDS
        if (serialCmdBuffer.active) return true;
        #endif
        #ifdef USE_I2C_COMMANDS
        if (i2cCmdBuffer.active) return true;
        #endif
        #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
        if (wifiCmdBuffer.active) return true;
        #endif
        return false;
    #else
        #if defined(USE_SBUS_RECEIVER) || defined(USE_IBUS_RECEIVER) || defined(USE_DSM_RECEIVER) || defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
            return rcBuffer.active;
        #elif defined(USE_SERIAL_COMMANDS)
            return serialCmdBuffer.active;
        #elif defined(USE_I2C_COMMANDS)
            return i2cCmdBuffer.active;
        #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
            return wifiCmdBuffer.active;
        #else
            return false;
        #endif
    #endif
}

//========================================================================================================================//
//                                         DSM SERIAL EVENT HANDLER                                                       //
//========================================================================================================================//

void serialEvent3() {
    #ifdef USE_DSM_RECEIVER
        dsmSerialEvent();
    #endif
}
