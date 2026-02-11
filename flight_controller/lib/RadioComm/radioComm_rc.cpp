/*
 * Radio Communication — RC Receiver Protocols
 * SBUS, iBUS, DSM, PPM, PWM implementations.
 * Each protocol provides a setup function and a read function.
 */

#include "radioComm.h"
#include "pin_definitions.h"

//========================================================================================================================//
//                                              SBUS RECEIVER                                                             //
//========================================================================================================================//

#ifdef USE_SBUS_RECEIVER

#include "SBUS.h"

SBUS sbus(SBUS_SERIAL_PORT);
static uint16_t sbusChannels[16];
static bool sbusFailSafe = false;
static bool sbusLostFrame = false;

void sbusSetup() {
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
}

bool readSBUS(CommandBuffer& buf) {
    if (sbus.read(&sbusChannels[0], &sbusFailSafe, &sbusLostFrame)) {
        if (sbusFailSafe || sbusLostFrame) {
            // Protocol-level fault — treat as no valid data
            buf.active = false;
            return false;
        }
        buf.channels[0] = map(sbusChannels[0], 172, 1811, 1000, 2000);  // Roll
        buf.channels[1] = map(sbusChannels[1], 172, 1811, 1000, 2000);  // Pitch
        buf.channels[2] = map(sbusChannels[2], 172, 1811, 1000, 2000);  // Throttle
        buf.channels[3] = map(sbusChannels[3], 172, 1811, 1000, 2000);  // Yaw
        buf.channels[4] = map(sbusChannels[4], 172, 1811, 1000, 2000);  // Aux1
        buf.channels[5] = map(sbusChannels[5], 172, 1811, 1000, 2000);  // Aux2
        buf.timestamp = millis();
        buf.active = true;
        return true;
    }
    // No new frame — check timeout
    if (buf.timestamp > 0 && millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS) {
        buf.active = false;
    }
    return false;
}

#endif // USE_SBUS_RECEIVER

//========================================================================================================================//
//                                              iBUS RECEIVER                                                             //
//========================================================================================================================//

#ifdef USE_IBUS_RECEIVER

static const uint8_t IBUS_FRAME_LENGTH = 32;
static const uint8_t IBUS_HEADER_0 = 0x20;  // Length byte
static const uint8_t IBUS_HEADER_1 = 0x40;  // Command: channel data
static const uint8_t IBUS_MAX_CHANNELS = 14;
static uint8_t ibusBuffer[32];
static uint8_t ibusIndex = 0;
static uint16_t ibusChannels[14];

void ibusSetup() {
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
}

bool readIBUS(CommandBuffer& buf) {
    bool newData = false;

    while (IBUS_SERIAL_PORT.available()) {
        uint8_t b = IBUS_SERIAL_PORT.read();

        if (ibusIndex == 0 && b != IBUS_HEADER_0) continue;
        if (ibusIndex == 1 && b != IBUS_HEADER_1) { ibusIndex = 0; continue; }

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
                for (uint8_t ch = 0; ch < IBUS_MAX_CHANNELS; ch++) {
                    ibusChannels[ch] = ibusBuffer[2 + ch * 2] | (ibusBuffer[3 + ch * 2] << 8);
                }
                buf.channels[0] = ibusChannels[0];  // Roll
                buf.channels[1] = ibusChannels[1];  // Pitch
                buf.channels[2] = ibusChannels[2];  // Throttle
                buf.channels[3] = ibusChannels[3];  // Yaw
                buf.channels[4] = ibusChannels[4];  // Aux1
                buf.channels[5] = ibusChannels[5];  // Aux2
                buf.timestamp = millis();
                buf.active = true;
                newData = true;
            }
        }
    }

    if (!newData && buf.timestamp > 0 && millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS) {
        buf.active = false;
    }
    return newData;
}

#endif // USE_IBUS_RECEIVER

//========================================================================================================================//
//                                              DSM RECEIVER                                                              //
//========================================================================================================================//

#ifdef USE_DSM_RECEIVER

#include "DSMRX.h"

DSM1024 DSM;

void dsmSetup() {
    DSM_SERIAL_PORT.begin(115000);
    Serial.println("DSM receiver initialized");
    Serial.println("  Port: Serial3 (RX pin 15)");
    Serial.println("  Baud: 115000");
    Serial.println("  Channels: 12");
}

bool readDSM(CommandBuffer& buf) {
    if (!DSM.timedOut()) {
        buf.channels[0] = DSM.getChannelValue(1);  // Roll
        buf.channels[1] = DSM.getChannelValue(2);  // Pitch
        buf.channels[2] = DSM.getChannelValue(3);  // Throttle
        buf.channels[3] = DSM.getChannelValue(4);  // Yaw
        buf.channels[4] = DSM.getChannelValue(5);  // Aux1
        buf.channels[5] = DSM.getChannelValue(6);  // Aux2
        buf.timestamp = millis();
        buf.active = true;
        return true;
    }
    buf.active = false;
    return false;
}

void dsmSerialEvent() {
    while (DSM_SERIAL_PORT.available()) {
        DSM.handleSerialEvent(DSM_SERIAL_PORT.read(), micros());
    }
}

#endif // USE_DSM_RECEIVER

//========================================================================================================================//
//                                              PPM RECEIVER                                                              //
//========================================================================================================================//

#ifdef USE_PPM_RECEIVER

static unsigned long channel_1_raw = 1500, channel_2_raw = 1500, channel_3_raw = 1000;
static unsigned long channel_4_raw = 1500, channel_5_raw = 1000, channel_6_raw = 1000;
static int ppm_counter = 0;
static unsigned long time_ms = 0;

static void getPPM() {
    unsigned long dt_ppm;
    int trig = digitalRead(PPM_PIN);

    if (trig == 1) {
        dt_ppm = micros() - time_ms;
        time_ms = micros();

        if (dt_ppm > 5000) {
            ppm_counter = 0;
        }

        if (ppm_counter == 1) channel_1_raw = dt_ppm;
        if (ppm_counter == 2) channel_2_raw = dt_ppm;
        if (ppm_counter == 3) channel_3_raw = dt_ppm;
        if (ppm_counter == 4) channel_4_raw = dt_ppm;
        if (ppm_counter == 5) channel_5_raw = dt_ppm;
        if (ppm_counter == 6) channel_6_raw = dt_ppm;

        ppm_counter++;
    }
}

void ppmSetup() {
    pinMode(PPM_PIN, INPUT_PULLUP);
    delay(20);
    attachInterrupt(digitalPinToInterrupt(PPM_PIN), getPPM, CHANGE);
    Serial.println("PPM receiver initialized");
    Serial.print("  Pin: "); Serial.println(PPM_PIN);
}

bool readPPM(CommandBuffer& buf) {
    buf.channels[0] = channel_1_raw;
    buf.channels[1] = channel_2_raw;
    buf.channels[2] = channel_3_raw;
    buf.channels[3] = channel_4_raw;
    buf.channels[4] = channel_5_raw;
    buf.channels[5] = channel_6_raw;

    // PPM has no frame-level detection; use channel value heuristic
    if (channel_1_raw != 1500 || channel_2_raw != 1500) {
        buf.timestamp = millis();
        buf.active = true;
        return true;
    }

    if (buf.timestamp > 0 && millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS) {
        buf.active = false;
    }
    return false;
}

#endif // USE_PPM_RECEIVER

//========================================================================================================================//
//                                              PWM RECEIVER                                                              //
//========================================================================================================================//

#ifdef USE_PWM_RECEIVER

static unsigned long channel_1_raw = 1500, channel_2_raw = 1500, channel_3_raw = 1000;
static unsigned long channel_4_raw = 1500, channel_5_raw = 1000, channel_6_raw = 1000;
static unsigned long rising_edge_start_1, rising_edge_start_2, rising_edge_start_3;
static unsigned long rising_edge_start_4, rising_edge_start_5, rising_edge_start_6;

static void getCh1() {
    int trigger = digitalRead(PWM_CH1_PIN);
    if (trigger == 1) rising_edge_start_1 = micros();
    else if (trigger == 0) channel_1_raw = micros() - rising_edge_start_1;
}

static void getCh2() {
    int trigger = digitalRead(PWM_CH2_PIN);
    if (trigger == 1) rising_edge_start_2 = micros();
    else if (trigger == 0) channel_2_raw = micros() - rising_edge_start_2;
}

static void getCh3() {
    int trigger = digitalRead(PWM_CH3_PIN);
    if (trigger == 1) rising_edge_start_3 = micros();
    else if (trigger == 0) channel_3_raw = micros() - rising_edge_start_3;
}

static void getCh4() {
    int trigger = digitalRead(PWM_CH4_PIN);
    if (trigger == 1) rising_edge_start_4 = micros();
    else if (trigger == 0) channel_4_raw = micros() - rising_edge_start_4;
}

static void getCh5() {
    int trigger = digitalRead(PWM_CH5_PIN);
    if (trigger == 1) rising_edge_start_5 = micros();
    else if (trigger == 0) channel_5_raw = micros() - rising_edge_start_5;
}

static void getCh6() {
    int trigger = digitalRead(PWM_CH6_PIN);
    if (trigger == 1) rising_edge_start_6 = micros();
    else if (trigger == 0) channel_6_raw = micros() - rising_edge_start_6;
}

void pwmSetup() {
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
}

bool readPWM(CommandBuffer& buf) {
    buf.channels[0] = channel_1_raw;
    buf.channels[1] = channel_2_raw;
    buf.channels[2] = channel_3_raw;
    buf.channels[3] = channel_4_raw;
    buf.channels[4] = channel_5_raw;
    buf.channels[5] = channel_6_raw;

    // PWM has no frame-level detection; use channel value heuristic
    if (channel_1_raw != 1500 || channel_2_raw != 1500) {
        buf.timestamp = millis();
        buf.active = true;
        return true;
    }

    if (buf.timestamp > 0 && millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS) {
        buf.active = false;
    }
    return false;
}

#endif // USE_PWM_RECEIVER
