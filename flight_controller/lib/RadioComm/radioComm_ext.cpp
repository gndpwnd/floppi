/*
 * Radio Communication — External Command Sources
 * Serial (UART binary), I2C (Wire1 slave), WiFi API (ESP32 web server).
 * Each source provides a setup function and a read function.
 */

#include "radioComm.h"
#include "pin_definitions.h"

//========================================================================================================================//
//                                         SERIAL COMMANDS (external flight computer)                                      //
//========================================================================================================================//

#ifdef USE_SERIAL_COMMANDS

// Frame: [0xAA] [0x55] [ch1_lo] [ch1_hi] ... [ch6_lo] [ch6_hi] [checksum]
// 15 bytes total: 2 header + 12 channel data + 1 XOR checksum
static const uint8_t SCMD_FRAME_LENGTH = 15;
static const uint8_t SCMD_HEADER_0 = 0xAA;
static const uint8_t SCMD_HEADER_1 = 0x55;
static uint8_t scmdBuffer[15];
static uint8_t scmdIndex = 0;

void serialCmdSetup() {
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
}

bool readSerialCmd(CommandBuffer& buf) {
    bool newData = false;

    while (SERIAL_CMD_PORT.available()) {
        uint8_t b = SERIAL_CMD_PORT.read();

        if (scmdIndex == 0 && b != SCMD_HEADER_0) continue;
        if (scmdIndex == 1 && b != SCMD_HEADER_1) { scmdIndex = 0; continue; }

        scmdBuffer[scmdIndex++] = b;

        if (scmdIndex == SCMD_FRAME_LENGTH) {
            scmdIndex = 0;

            // Verify checksum: XOR of channel data bytes (indices 2..13)
            uint8_t checksum = 0;
            for (uint8_t i = 2; i < SCMD_FRAME_LENGTH - 1; i++) {
                checksum ^= scmdBuffer[i];
            }

            if (checksum == scmdBuffer[SCMD_FRAME_LENGTH - 1]) {
                buf.channels[0] = scmdBuffer[2]  | (scmdBuffer[3]  << 8);  // Roll
                buf.channels[1] = scmdBuffer[4]  | (scmdBuffer[5]  << 8);  // Pitch
                buf.channels[2] = scmdBuffer[6]  | (scmdBuffer[7]  << 8);  // Throttle
                buf.channels[3] = scmdBuffer[8]  | (scmdBuffer[9]  << 8);  // Yaw
                buf.channels[4] = scmdBuffer[10] | (scmdBuffer[11] << 8);  // Aux1
                buf.channels[5] = scmdBuffer[12] | (scmdBuffer[13] << 8);  // Aux2
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

#endif // USE_SERIAL_COMMANDS

//========================================================================================================================//
//                                         I2C COMMANDS (external flight computer)                                         //
//========================================================================================================================//

#ifdef USE_I2C_COMMANDS

// I2C command buffer — written by ISR (onI2CReceive), read by readI2CCmd()
// FC is I2C SLAVE at I2C_CMD_ADDRESS on Wire1 (separate from IMU on Wire).
// Master (Raspberry Pi etc.) writes 12 bytes: 6x uint16_t LE, 1000-2000us.
static volatile uint16_t i2cCmdChannels[6] = {1500, 1500, 1000, 1500, 1000, 1000};
static volatile uint32_t i2cCmdTimestamp = 0;
static volatile bool i2cCmdReady = false;

// I2C receive callback — called from ISR context
static void onI2CReceive(int numBytes) {
    if (numBytes == 12) {
        uint8_t raw[12];
        for (int i = 0; i < 12; i++) {
            raw[i] = Wire1.read();
        }
        i2cCmdChannels[0] = raw[0]  | (raw[1]  << 8);
        i2cCmdChannels[1] = raw[2]  | (raw[3]  << 8);
        i2cCmdChannels[2] = raw[4]  | (raw[5]  << 8);
        i2cCmdChannels[3] = raw[6]  | (raw[7]  << 8);
        i2cCmdChannels[4] = raw[8]  | (raw[9]  << 8);
        i2cCmdChannels[5] = raw[10] | (raw[11] << 8);
        i2cCmdTimestamp = millis();
        i2cCmdReady = true;
    } else {
        while (Wire1.available()) Wire1.read();
    }
}

void i2cCmdSetup() {
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
}

bool readI2CCmd(CommandBuffer& buf) {
    if (i2cCmdReady) {
        noInterrupts();
        buf.channels[0] = i2cCmdChannels[0];  // Roll
        buf.channels[1] = i2cCmdChannels[1];  // Pitch
        buf.channels[2] = i2cCmdChannels[2];  // Throttle
        buf.channels[3] = i2cCmdChannels[3];  // Yaw
        buf.channels[4] = i2cCmdChannels[4];  // Aux1
        buf.channels[5] = i2cCmdChannels[5];  // Aux2
        uint32_t ts = i2cCmdTimestamp;
        i2cCmdReady = false;
        interrupts();
        buf.timestamp = ts;
        buf.active = true;
        return true;
    }

    if (buf.timestamp > 0 && millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS) {
        buf.active = false;
    }
    return false;
}

#endif // USE_I2C_COMMANDS

//========================================================================================================================//
//                                         WIFI API COMMANDS (ESP32 only)                                                  //
//========================================================================================================================//

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)

#include <freertos/portmacro.h>

// WiFi API command buffer — written by Core 1 (web server), read by Core 0 (flight control)
// Protected by spinlock for thread-safe cross-core access.
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

void wifiCmdSetup() {
    Serial.println("WiFi API command source active");
    Serial.println("  Commands via: POST /api/commands, WebSocket /ws");
    Serial.println("  Failsafe timeout: 500ms");
}

bool readWifiCmd(CommandBuffer& buf) {
    portENTER_CRITICAL(&wifiCmdMux);
    uint16_t ch0 = wifiCmdChannels[0];
    uint16_t ch1 = wifiCmdChannels[1];
    uint16_t ch2 = wifiCmdChannels[2];
    uint16_t ch3 = wifiCmdChannels[3];
    uint16_t ch4 = wifiCmdChannels[4];
    uint16_t ch5 = wifiCmdChannels[5];
    uint32_t ts  = wifiCmdTimestamp;
    portEXIT_CRITICAL(&wifiCmdMux);

    buf.channels[0] = ch0;
    buf.channels[1] = ch1;
    buf.channels[2] = ch2;
    buf.channels[3] = ch3;
    buf.channels[4] = ch4;
    buf.channels[5] = ch5;

    if (ts > buf.timestamp) {
        buf.timestamp = ts;
        buf.active = true;
        return true;
    }

    if (buf.timestamp > 0 && millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS) {
        buf.active = false;
    }
    return false;
}

#endif // USE_ESP32 && USE_WEB_SERVER
