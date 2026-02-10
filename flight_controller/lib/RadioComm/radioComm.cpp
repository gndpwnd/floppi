/*
 * Radio Communication Implementation
 * Complete single-file approach for all receiver protocols
 * Based on original dRehmFlight radioComm.ino with all features
 */

#include "radioComm.h"
#include "pin_definitions.h"
#include <algorithm>

//========================================================================================================================//
//                                            PROTOCOL-SPECIFIC INCLUDES                                                  //
//========================================================================================================================//

#ifdef USE_SBUS_RECEIVER
    #include "SBUS.h"
    SBUS sbus(SBUS_SERIAL_PORT);
    uint16_t sbusChannels[16];
    bool sbusFailSafe = false;
    bool sbusLostFrame = false;
#endif

#ifdef USE_DSM_RECEIVER
    #include "DSMRX.h"
    DSM1024 DSM;  // Match original naming
#endif

#ifdef USE_IBUS_RECEIVER
    // iBUS protocol constants
    static const uint8_t IBUS_FRAME_LENGTH = 32;
    static const uint8_t IBUS_HEADER_0 = 0x20;  // Length byte
    static const uint8_t IBUS_HEADER_1 = 0x40;  // Command: channel data
    static const uint8_t IBUS_MAX_CHANNELS = 14;
    static uint8_t ibusBuffer[32];
    static uint8_t ibusIndex = 0;
    static uint16_t ibusChannels[14];
#endif

#ifdef USE_SERIAL_COMMANDS
    // Serial command protocol constants
    // Frame: [0xAA] [0x55] [ch1_lo] [ch1_hi] ... [ch6_lo] [ch6_hi] [checksum]
    // 15 bytes total: 2 header + 12 channel data + 1 XOR checksum
    // Channel values: uint16_t little-endian, 1000-2000 microseconds
    static const uint8_t SCMD_FRAME_LENGTH = 15;
    static const uint8_t SCMD_HEADER_0 = 0xAA;
    static const uint8_t SCMD_HEADER_1 = 0x55;
    static uint8_t scmdBuffer[15];
    static uint8_t scmdIndex = 0;
#endif

#ifdef USE_I2C_COMMANDS
    // I2C command buffer — written by ISR (onI2CReceive), read by getCommands()
    // FC is I2C SLAVE at I2C_CMD_ADDRESS on Wire1 (separate from IMU on Wire).
    // Master (Raspberry Pi etc.) writes 12 bytes: 6x uint16_t LE, 1000-2000us.
    static volatile uint16_t i2cCmdChannels[6] = {1500, 1500, 1000, 1500, 1000, 1000};
    static volatile uint32_t i2cCmdTimestamp = 0;
    static volatile bool i2cCmdReady = false;

    // I2C receive callback — called from ISR context
    void onI2CReceive(int numBytes) {
        if (numBytes == 12) {
            uint8_t buf[12];
            for (int i = 0; i < 12; i++) {
                buf[i] = Wire1.read();
            }
            i2cCmdChannels[0] = buf[0]  | (buf[1]  << 8);
            i2cCmdChannels[1] = buf[2]  | (buf[3]  << 8);
            i2cCmdChannels[2] = buf[4]  | (buf[5]  << 8);
            i2cCmdChannels[3] = buf[6]  | (buf[7]  << 8);
            i2cCmdChannels[4] = buf[8]  | (buf[9]  << 8);
            i2cCmdChannels[5] = buf[10] | (buf[11] << 8);
            i2cCmdTimestamp = millis();
            i2cCmdReady = true;
        } else {
            // Flush unexpected data
            while (Wire1.available()) Wire1.read();
        }
    }
#endif

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
    // WiFi API command buffer — written by Core 1 (web server), read by Core 0 (flight control)
    // Protected by spinlock for thread-safe cross-core access.
    #include <freertos/portmacro.h>
    static portMUX_TYPE wifiCmdMux = portMUX_INITIALIZER_UNLOCKED;
    static volatile uint16_t wifiCmdChannels[6] = {1500, 1500, 1000, 1500, 1000, 1000};
    static volatile uint32_t wifiCmdTimestamp = 0;

    void setWifiCommandChannels(uint16_t ch1, uint16_t ch2, uint16_t ch3, uint16_t ch4, uint16_t ch5, uint16_t ch6) {
        portENTER_CRITICAL(&wifiCmdMux);
        wifiCmdChannels[0] = ch1;
        wifiCmdChannels[1] = ch2;
        wifiCmdChannels[2] = ch3;
        wifiCmdChannels[3] = ch4;
        wifiCmdChannels[4] = ch5;
        wifiCmdChannels[5] = ch6;
        wifiCmdTimestamp = millis();
        portEXIT_CRITICAL(&wifiCmdMux);
    }
#endif

//========================================================================================================================//
//                                              CHANNEL VARIABLES                                                         //
//========================================================================================================================//

// Channel values in microseconds (1000-2000)
unsigned long channel_1_pwm = 1500;
unsigned long channel_2_pwm = 1500;
unsigned long channel_3_pwm = 1000;  // Throttle starts at minimum
unsigned long channel_4_pwm = 1500;
unsigned long channel_5_pwm = 1000;
unsigned long channel_6_pwm = 1000;

// For PPM/PWM - raw channel storage (used internally)
static unsigned long channel_1_raw = 1500;
static unsigned long channel_2_raw = 1500;
static unsigned long channel_3_raw = 1000;
static unsigned long channel_4_raw = 1500;
static unsigned long channel_5_raw = 1000;
static unsigned long channel_6_raw = 1000;

// Interrupt timing variables for PPM/PWM
static unsigned long rising_edge_start_1, rising_edge_start_2, rising_edge_start_3;
static unsigned long rising_edge_start_4, rising_edge_start_5, rising_edge_start_6;
static int ppm_counter = 0;
static unsigned long time_ms = 0;

// Last frame time for timeout detection
unsigned long lastFrameTime = 0;

//========================================================================================================================//
//                                        INTERRUPT SERVICE ROUTINES                                                      //
//========================================================================================================================//

// PPM interrupt handler
void getPPM() {
    #ifdef USE_PPM_RECEIVER
        unsigned long dt_ppm;
        int trig = digitalRead(PPM_PIN);
        
        if (trig == 1) { // Only care about rising edge
            dt_ppm = micros() - time_ms;
            time_ms = micros();
            
            if (dt_ppm > 5000) { // Waiting for long pulse to indicate new pulse train
                ppm_counter = 0;
            }
            
            if (ppm_counter == 1) { // First pulse
                channel_1_raw = dt_ppm;
            }
            if (ppm_counter == 2) { // Second pulse
                channel_2_raw = dt_ppm;
            }
            if (ppm_counter == 3) { // Third pulse
                channel_3_raw = dt_ppm;
            }
            if (ppm_counter == 4) { // Fourth pulse
                channel_4_raw = dt_ppm;
            }
            if (ppm_counter == 5) { // Fifth pulse
                channel_5_raw = dt_ppm;
            }
            if (ppm_counter == 6) { // Sixth pulse
                channel_6_raw = dt_ppm;
            }
            
            ppm_counter = ppm_counter + 1;
        }
    #endif
}

// PWM interrupt handlers
void getCh1() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH1_PIN);
        if (trigger == 1) {
            rising_edge_start_1 = micros();
        }
        else if (trigger == 0) {
            channel_1_raw = micros() - rising_edge_start_1;
        }
    #endif
}

void getCh2() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH2_PIN);
        if (trigger == 1) {
            rising_edge_start_2 = micros();
        }
        else if (trigger == 0) {
            channel_2_raw = micros() - rising_edge_start_2;
        }
    #endif
}

void getCh3() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH3_PIN);
        if (trigger == 1) {
            rising_edge_start_3 = micros();
        }
        else if (trigger == 0) {
            channel_3_raw = micros() - rising_edge_start_3;
        }
    #endif
}

void getCh4() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH4_PIN);
        if (trigger == 1) {
            rising_edge_start_4 = micros();
        }
        else if (trigger == 0) {
            channel_4_raw = micros() - rising_edge_start_4;
        }
    #endif
}

void getCh5() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH5_PIN);
        if (trigger == 1) {
            rising_edge_start_5 = micros();
        }
        else if (trigger == 0) {
            channel_5_raw = micros() - rising_edge_start_5;
        }
    #endif
}

void getCh6() {
    #ifdef USE_PWM_RECEIVER
        int trigger = digitalRead(PWM_CH6_PIN);
        if (trigger == 1) {
            rising_edge_start_6 = micros();
        }
        else if (trigger == 0) {
            channel_6_raw = micros() - rising_edge_start_6;
        }
    #endif
}

//========================================================================================================================//
//                                              RADIO SETUP                                                               //
//========================================================================================================================//

void radioSetup() {
    #ifdef USE_SBUS_RECEIVER
        sbus.begin();
        Serial.println("SBUS receiver initialized");
        #ifdef USE_ESP32
            Serial.println("  Port: Serial2 (ESP32)");
            Serial.print("  RX Pin: "); Serial.println(SBUS_RX_PIN);
        #elif (SBUS_SERIAL_PORT == Serial5)
            Serial.println("  Port: Serial5 (RX pin 21)");
        #elif (SBUS_SERIAL_PORT == Serial3)
            Serial.println("  Port: Serial3 (RX pin 15)");
        #endif
        Serial.println("  Baud: 100000, 8E2 (inverted)");
        Serial.println("  Channels: 16");
        
    #elif defined(USE_IBUS_RECEIVER)
        #ifdef USE_ESP32
            IBUS_SERIAL_PORT.begin(115200, SERIAL_8N1, IBUS_RX_PIN, IBUS_TX_PIN);
            Serial.println("iBUS receiver initialized");
            Serial.print("  Port: Serial1 (ESP32), RX Pin: "); Serial.println(IBUS_RX_PIN);
        #else
            IBUS_SERIAL_PORT.begin(115200);
            Serial.println("iBUS receiver initialized");
            Serial.println("  Port: Serial3 (RX pin 15)");
        #endif
        Serial.println("  Baud: 115200, 8N1 (non-inverted)");
        Serial.println("  Channels: 14");

    #elif defined(USE_DSM_RECEIVER)
        DSM_SERIAL_PORT.begin(115000);  // Match original 115000 baud
        Serial.println("DSM receiver initialized");
        Serial.println("  Port: Serial3 (RX pin 15)");
        Serial.println("  Baud: 115000");
        Serial.println("  Channels: 12");

    #elif defined(USE_SERIAL_COMMANDS)
        #ifdef USE_ESP32
            SERIAL_CMD_PORT.begin(115200, SERIAL_8N1, SERIAL_CMD_RX_PIN, SERIAL_CMD_TX_PIN);
            Serial.println("Serial command input initialized");
            Serial.print("  Port: Serial1 (ESP32), RX Pin: "); Serial.println(SERIAL_CMD_RX_PIN);
        #else
            SERIAL_CMD_PORT.begin(115200);
            Serial.println("Serial command input initialized");
            Serial.println("  Port: Serial3 (RX pin 15)");
        #endif
        Serial.println("  Baud: 115200, 8N1");
        Serial.println("  Protocol: Binary (15-byte frames)");
        Serial.println("  Frame: [AA 55] [ch1..ch6 LE uint16] [XOR checksum]");

    #elif defined(USE_I2C_COMMANDS)
        #ifdef USE_ESP32
            Wire1.begin(I2C_CMD_ADDRESS, I2C_CMD_SDA_PIN, I2C_CMD_SCL_PIN, 400000);
        #else
            Wire1.begin(I2C_CMD_ADDRESS);
        #endif
        Wire1.onReceive(onI2CReceive);
        Serial.println("I2C command source initialized");
        Serial.printf("  Address: 0x%02X on Wire1\n", I2C_CMD_ADDRESS);
        Serial.println("  Protocol: 12 bytes (6x uint16 LE), 1000-2000us");
        Serial.println("  Failsafe timeout: 500ms");

    #elif defined(USE_PPM_RECEIVER)
        // PPM setup
        pinMode(PPM_PIN, INPUT_PULLUP);
        delay(20);
        attachInterrupt(digitalPinToInterrupt(PPM_PIN), getPPM, CHANGE);
        Serial.println("PPM receiver initialized");
        Serial.print("  Pin: "); Serial.println(PPM_PIN);
        
    #elif defined(USE_PWM_RECEIVER)
        // PWM setup - attach interrupts for each channel
        pinMode(PWM_CH1_PIN, INPUT_PULLUP);
        pinMode(PWM_CH2_PIN, INPUT_PULLUP);
        pinMode(PWM_CH3_PIN, INPUT_PULLUP);
        pinMode(PWM_CH4_PIN, INPUT_PULLUP);
        pinMode(PWM_CH5_PIN, INPUT_PULLUP);
        pinMode(PWM_CH6_PIN, INPUT_PULLUP);
        delay(20);
        attachInterrupt(digitalPinToInterrupt(PWM_CH1_PIN), getCh1, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH2_PIN), getCh2, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH3_PIN), getCh3, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH4_PIN), getCh4, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH5_PIN), getCh5, CHANGE);
        attachInterrupt(digitalPinToInterrupt(PWM_CH6_PIN), getCh6, CHANGE);
        delay(20);
        Serial.println("PWM receiver initialized");
        Serial.println("  Individual channel pins");
        
    #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
        // WiFi API command source — no hardware initialization needed
        // Commands arrive via POST /api/commands or WebSocket from external controller
        Serial.println("WiFi API command source (no physical receiver)");
        Serial.println("  Commands via: POST /api/commands, WebSocket /ws");
        Serial.println("  Failsafe timeout: 500ms");

    #else
        #error "No receiver or command source defined in config.h!"
    #endif

    delay(100);
    lastFrameTime = millis();
}

//========================================================================================================================//
//                                              GET COMMANDS                                                              //
//========================================================================================================================//

void getCommands() {
    
    //========== SBUS RECEIVER ==========//
    #ifdef USE_SBUS_RECEIVER
        if (sbus.read(&sbusChannels[0], &sbusFailSafe, &sbusLostFrame)) {
            // SBUS channels are 0-2047, convert to 1000-2000 microseconds
            // Standard SBUS calibration: 172 = 1000us, 1811 = 2000us
            channel_1_pwm = map(sbusChannels[0], 172, 1811, 1000, 2000);  // Roll
            channel_2_pwm = map(sbusChannels[1], 172, 1811, 1000, 2000);  // Pitch
            channel_3_pwm = map(sbusChannels[2], 172, 1811, 1000, 2000);  // Throttle
            channel_4_pwm = map(sbusChannels[3], 172, 1811, 1000, 2000);  // Yaw
            channel_5_pwm = map(sbusChannels[4], 172, 1811, 1000, 2000);  // Aux1
            channel_6_pwm = map(sbusChannels[5], 172, 1811, 1000, 2000);  // Aux2
            
            lastFrameTime = millis();
        }
    
    //========== iBUS RECEIVER ==========//
    #elif defined(USE_IBUS_RECEIVER)
        // Read all available bytes and parse iBUS frames
        while (IBUS_SERIAL_PORT.available()) {
            uint8_t b = IBUS_SERIAL_PORT.read();

            if (ibusIndex == 0 && b != IBUS_HEADER_0) continue;  // Wait for length byte
            if (ibusIndex == 1 && b != IBUS_HEADER_1) { ibusIndex = 0; continue; }  // Validate command byte

            ibusBuffer[ibusIndex++] = b;

            if (ibusIndex == IBUS_FRAME_LENGTH) {
                ibusIndex = 0;

                // Verify checksum: 0xFFFF minus sum of bytes 0..29
                uint16_t checksum = 0xFFFF;
                for (uint8_t i = 0; i < IBUS_FRAME_LENGTH - 2; i++) {
                    checksum -= ibusBuffer[i];
                }
                uint16_t received = ibusBuffer[30] | (ibusBuffer[31] << 8);

                if (checksum == received) {
                    // Parse channels (little-endian uint16, already in microseconds)
                    for (uint8_t ch = 0; ch < IBUS_MAX_CHANNELS; ch++) {
                        ibusChannels[ch] = ibusBuffer[2 + ch * 2] | (ibusBuffer[3 + ch * 2] << 8);
                    }

                    channel_1_pwm = ibusChannels[0];  // Roll
                    channel_2_pwm = ibusChannels[1];  // Pitch
                    channel_3_pwm = ibusChannels[2];  // Throttle
                    channel_4_pwm = ibusChannels[3];  // Yaw
                    channel_5_pwm = ibusChannels[4];  // Aux1
                    channel_6_pwm = ibusChannels[5];  // Aux2

                    lastFrameTime = millis();
                }
            }
        }

    //========== SERIAL COMMANDS ==========//
    #elif defined(USE_SERIAL_COMMANDS)
        // Read all available bytes and parse serial command frames
        while (SERIAL_CMD_PORT.available()) {
            uint8_t b = SERIAL_CMD_PORT.read();

            if (scmdIndex == 0 && b != SCMD_HEADER_0) continue;  // Wait for 0xAA
            if (scmdIndex == 1 && b != SCMD_HEADER_1) { scmdIndex = 0; continue; }  // Validate 0x55

            scmdBuffer[scmdIndex++] = b;

            if (scmdIndex == SCMD_FRAME_LENGTH) {
                scmdIndex = 0;

                // Verify checksum: XOR of channel data bytes (indices 2..13)
                uint8_t checksum = 0;
                for (uint8_t i = 2; i < SCMD_FRAME_LENGTH - 1; i++) {
                    checksum ^= scmdBuffer[i];
                }

                if (checksum == scmdBuffer[SCMD_FRAME_LENGTH - 1]) {
                    // Parse 6 channels (little-endian uint16, already in microseconds)
                    channel_1_pwm = scmdBuffer[2]  | (scmdBuffer[3]  << 8);  // Roll
                    channel_2_pwm = scmdBuffer[4]  | (scmdBuffer[5]  << 8);  // Pitch
                    channel_3_pwm = scmdBuffer[6]  | (scmdBuffer[7]  << 8);  // Throttle
                    channel_4_pwm = scmdBuffer[8]  | (scmdBuffer[9]  << 8);  // Yaw
                    channel_5_pwm = scmdBuffer[10] | (scmdBuffer[11] << 8);  // Aux1
                    channel_6_pwm = scmdBuffer[12] | (scmdBuffer[13] << 8);  // Aux2

                    lastFrameTime = millis();
                }
            }
        }

    //========== I2C COMMANDS (external flight computer) ==========//
    #elif defined(USE_I2C_COMMANDS)
        if (i2cCmdReady) {
            noInterrupts();
            channel_1_pwm = i2cCmdChannels[0];  // Roll
            channel_2_pwm = i2cCmdChannels[1];  // Pitch
            channel_3_pwm = i2cCmdChannels[2];  // Throttle
            channel_4_pwm = i2cCmdChannels[3];  // Yaw
            channel_5_pwm = i2cCmdChannels[4];  // Aux1
            channel_6_pwm = i2cCmdChannels[5];  // Aux2
            uint32_t ts = i2cCmdTimestamp;
            i2cCmdReady = false;
            interrupts();
            lastFrameTime = ts;
        }

    //========== DSM RECEIVER ==========//
    #elif defined(USE_DSM_RECEIVER)
        if (!DSM.timedOut()) {
            // DSM already returns microseconds (1000-2000)
            channel_1_pwm = DSM.getChannelValue(1);  // Roll
            channel_2_pwm = DSM.getChannelValue(2);  // Pitch
            channel_3_pwm = DSM.getChannelValue(3);  // Throttle
            channel_4_pwm = DSM.getChannelValue(4);  // Yaw
            channel_5_pwm = DSM.getChannelValue(5);  // Aux1
            channel_6_pwm = DSM.getChannelValue(6);  // Aux2
            
            lastFrameTime = millis();
        }
    
    //========== PPM/PWM RECEIVER ==========//
    #elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        // For PPM/PWM, interrupts update channel_X_raw variables
        // Copy to channel_X_pwm variables
        channel_1_pwm = channel_1_raw;
        channel_2_pwm = channel_2_raw;
        channel_3_pwm = channel_3_raw;
        channel_4_pwm = channel_4_raw;
        channel_5_pwm = channel_5_raw;
        channel_6_pwm = channel_6_raw;

        // Update last frame time if channels are changing
        if (channel_1_raw != 1500 || channel_2_raw != 1500) {
            lastFrameTime = millis();
        }

    //========== WIFI API COMMANDS (ESP32 only, no physical receiver) ==========//
    #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
        // Read from shared buffer updated by web server on Core 1
        portENTER_CRITICAL(&wifiCmdMux);
        channel_1_pwm = wifiCmdChannels[0];  // Roll
        channel_2_pwm = wifiCmdChannels[1];  // Pitch
        channel_3_pwm = wifiCmdChannels[2];  // Throttle
        channel_4_pwm = wifiCmdChannels[3];  // Yaw
        channel_5_pwm = wifiCmdChannels[4];  // Aux1
        channel_6_pwm = wifiCmdChannels[5];  // Aux2
        uint32_t wifiTs = wifiCmdTimestamp;
        portEXIT_CRITICAL(&wifiCmdMux);

        if (wifiTs > 0) {
            lastFrameTime = wifiTs;
        }

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
    // Original dRehmFlight function to get channel value by number
    unsigned long returnPWM = 1500;  // Default center/safe value
    
    if (ch_num == 1) {
        returnPWM = channel_1_pwm;
    }
    else if (ch_num == 2) {
        returnPWM = channel_2_pwm;
    }
    else if (ch_num == 3) {
        returnPWM = channel_3_pwm;
    }
    else if (ch_num == 4) {
        returnPWM = channel_4_pwm;
    }
    else if (ch_num == 5) {
        returnPWM = channel_5_pwm;
    }
    else if (ch_num == 6) {
        returnPWM = channel_6_pwm;
    }
    
    return returnPWM;
}

//========================================================================================================================//
//                                              FAILSAFE                                                                  //
//========================================================================================================================//

void failSafe() {
    // Check for signal loss based on receiver type
    bool signalLost = false;
    
    #ifdef USE_SBUS_RECEIVER
        // SBUS has built-in failsafe flags
        signalLost = sbusFailSafe || sbusLostFrame;
        
        // Also check for timeout (no updates in 500ms)
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }
        
    #elif defined(USE_IBUS_RECEIVER)
        // iBUS: timeout-based signal loss detection
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }

    #elif defined(USE_SERIAL_COMMANDS)
        // Serial commands: timeout-based signal loss detection
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }

    #elif defined(USE_I2C_COMMANDS)
        // I2C commands: timeout-based signal loss detection
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }

    #elif defined(USE_DSM_RECEIVER)
        // DSM has built-in timeout detection
        signalLost = DSM.timedOut();

    #elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        // Check for timeout (no updates in 500ms)
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }

    #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
        // WiFi API commands: timeout-based signal loss detection
        if (millis() - lastFrameTime > 500) {
            signalLost = true;
        }
    #endif

    // If signal is lost, use failsafe values
    if (signalLost) {
        channel_1_pwm = FAILSAFE_ROLL;      // Center stick
        channel_2_pwm = FAILSAFE_PITCH;     // Center stick
        channel_3_pwm = FAILSAFE_THROTTLE;  // Minimum throttle
        channel_4_pwm = FAILSAFE_YAW;       // Center stick
        channel_5_pwm = FAILSAFE_AUX1;      // Throttle cut (high)
        channel_6_pwm = FAILSAFE_AUX2;      // Default value
        
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
    #ifdef USE_SBUS_RECEIVER
        return !sbusFailSafe && !sbusLostFrame && (millis() - lastFrameTime < 500);
        
    #elif defined(USE_IBUS_RECEIVER)
        return (millis() - lastFrameTime < 500);

    #elif defined(USE_SERIAL_COMMANDS)
        return (millis() - lastFrameTime < 500);

    #elif defined(USE_I2C_COMMANDS)
        return (millis() - lastFrameTime < 500);

    #elif defined(USE_DSM_RECEIVER)
        return !DSM.timedOut();

    #elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
        return (millis() - lastFrameTime < 500);

    #elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
        return (millis() - lastFrameTime < 500);

    #else
        return false;
    #endif
}

//========================================================================================================================//
//                                         DSM SERIAL EVENT HANDLER                                                       //
//========================================================================================================================//

// For DSM receivers - this is called automatically when serial data arrives
void serialEvent3() {
    #ifdef USE_DSM_RECEIVER
        while (DSM_SERIAL_PORT.available()) {
            DSM.handleSerialEvent(DSM_SERIAL_PORT.read(), micros());
        }
    #endif
}