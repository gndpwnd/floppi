/*
 * Display Module Implementation
 * OLED display with compile-time display selection using U8g2.
 * Uses software I2C on dedicated pins to avoid bus contention with IMU.
 *
 * Small displays (128x32, 3 lines): rotates between info screens every 2s.
 * Large displays (128x64, 6 lines): shows all info at once, no rotation.
 *
 * See: docs/findings/display-module-architecture.md
 * See: docs/findings/display-screen-capacity.md
 */

#ifdef USE_OLED_DISPLAY

#include "display.h"
#include "globals.h"
#include "pin_definitions.h"
#include <U8g2lib.h>

//========================================================================================================================//
//                                         COMPILE-TIME DISPLAY SELECTION                                                 //
//========================================================================================================================//

#if defined(DISPLAY_SSD1306_128X32)
    // 0.91" OLED (DSD TECH and similar)
    U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(
        U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 32
    #define DISPLAY_LINES  3
    #define LINE_HEIGHT    10

#elif defined(DISPLAY_SSD1306_128X64)
    // 0.96" OLED
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
        U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_LINES  6
    #define LINE_HEIGHT    10

#elif defined(DISPLAY_SH1106_128X64)
    // 1.3" OLED (HiLetGo and similar)
    U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(
        U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_LINES  6
    #define LINE_HEIGHT    10

#else
    // Default: 0.91" SSD1306 128x32 (most common small OLED)
    U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(
        U8G2_R0, OLED_SCL_PIN, OLED_SDA_PIN, U8X8_PIN_NONE);
    #define DISPLAY_WIDTH  128
    #define DISPLAY_HEIGHT 32
    #define DISPLAY_LINES  3
    #define LINE_HEIGHT    10
#endif

// Line Y positions (font baseline)
#define LINE_Y(n) ((n) * LINE_HEIGHT)

// Screen rotation interval for small displays (ms)
#define SCREEN_ROTATE_MS 2000

//========================================================================================================================//
//                                         SCREEN ROTATION STATE                                                          //
//========================================================================================================================//

#if DISPLAY_LINES < 6
static uint8_t screen_index = 0;
static unsigned long screen_timer = 0;
static uint8_t prev_state = 0xFF;  // Force reset on first render

static void advanceScreen(uint8_t state, uint8_t num_screens, unsigned long now) {
    if (state != prev_state) {
        prev_state = state;
        screen_index = 0;
        screen_timer = now;
    } else if (now - screen_timer >= SCREEN_ROTATE_MS) {
        screen_timer = now;
        screen_index = (screen_index + 1) % num_screens;
    }
}
#endif

//========================================================================================================================//
//                                              SCREEN RENDERING                                                          //
//========================================================================================================================//

static char buf[32];  // Reusable string buffer

static void drawCalibrating(const DisplayData_t* data) {
    u8g2.drawStr(0, LINE_Y(1), "CALIBRATING");

    const char* mode_str = "...";
    switch (data->calibration_mode) {
        case 1: mode_str = "IMU Offsets"; break;
        case 2: mode_str = "6-Position"; break;
        case 3: mode_str = "IMU + Orient"; break;
        case 4: mode_str = "Radio Map"; break;
    }
    u8g2.drawStr(0, LINE_Y(2), mode_str);

    if (DISPLAY_LINES >= 6) {
        u8g2.drawStr(0, LINE_Y(3), "Keep board still!");
    }
}

//------------------------------------------------------------------------
// 128x64 (6-line) screens — all info fits, no rotation
//------------------------------------------------------------------------

#if DISPLAY_LINES >= 6

static void drawIdle(const DisplayData_t* data) {
    u8g2.drawStr(0, LINE_Y(1), "FLOPPI READY");

    snprintf(buf, sizeof(buf), "Loop: %luus", (unsigned long)data->loop_dt_us);
    u8g2.drawStr(0, LINE_Y(2), buf);

    snprintf(buf, sizeof(buf), "R:%+6.1f P:%+6.1f", data->roll, data->pitch);
    u8g2.drawStr(0, LINE_Y(3), buf);
    snprintf(buf, sizeof(buf), "Y:%+6.1f", data->yaw);
    u8g2.drawStr(0, LINE_Y(4), buf);

    #if defined(USE_ESP32) && defined(USE_WIFI)
    if (data->wifi_connected) {
        snprintf(buf, sizeof(buf), "IP:%s", data->ip_address);
        u8g2.drawStr(0, LINE_Y(5), buf);
        snprintf(buf, sizeof(buf), "RSSI:%d %.10s", data->wifi_rssi, data->ssid);
        u8g2.drawStr(0, LINE_Y(6), buf);
    } else if (data->ssid[0] != '\0') {
        u8g2.drawStr(0, LINE_Y(5), "WiFi: connecting...");
    }
    #endif
}

static void drawArmed(const DisplayData_t* data) {
    u8g2.drawStr(0, LINE_Y(1), "** ARMED **");

    snprintf(buf, sizeof(buf), "R:%+5.1f P:%+5.1f", data->roll, data->pitch);
    u8g2.drawStr(0, LINE_Y(2), buf);

    int m1p = (int)(data->m1 * 100);
    int m2p = (int)(data->m2 * 100);
    int m3p = (int)(data->m3 * 100);
    int m4p = (int)(data->m4 * 100);
    snprintf(buf, sizeof(buf), "M:%d %d %d %d", m1p, m2p, m3p, m4p);
    u8g2.drawStr(0, LINE_Y(3), buf);

    snprintf(buf, sizeof(buf), "Y:%+6.1f", data->yaw);
    u8g2.drawStr(0, LINE_Y(4), buf);

    snprintf(buf, sizeof(buf), "Loop: %luus", (unsigned long)data->loop_dt_us);
    u8g2.drawStr(0, LINE_Y(5), buf);

    #if defined(USE_ESP32) && defined(USE_WIFI)
    if (data->wifi_connected) {
        snprintf(buf, sizeof(buf), "IP:%s", data->ip_address);
        u8g2.drawStr(0, LINE_Y(6), buf);
    }
    #endif
}

//------------------------------------------------------------------------
// 128x32 (3-line) screens — rotate every 2 seconds
//------------------------------------------------------------------------

#else // DISPLAY_LINES < 6

static void drawIdle(const DisplayData_t* data) {
    // Determine number of screens available
    uint8_t num_screens = 2;
    #if defined(USE_ESP32) && defined(USE_WIFI)
    if (data->wifi_connected || data->ssid[0] != '\0')
        num_screens = 3;
    #endif

    advanceScreen(0, num_screens, millis());

    switch (screen_index) {
        case 0:  // Status + attitude
            u8g2.drawStr(0, LINE_Y(1), "FLOPPI READY");
            snprintf(buf, sizeof(buf), "R:%+5.1f P:%+5.1f", data->roll, data->pitch);
            u8g2.drawStr(0, LINE_Y(2), buf);
            snprintf(buf, sizeof(buf), "Loop: %luus", (unsigned long)data->loop_dt_us);
            u8g2.drawStr(0, LINE_Y(3), buf);
            break;

        case 1:  // Yaw + more detail
            u8g2.drawStr(0, LINE_Y(1), "FLOPPI READY");
            snprintf(buf, sizeof(buf), "Y:%+6.1f", data->yaw);
            u8g2.drawStr(0, LINE_Y(2), buf);
            snprintf(buf, sizeof(buf), "R:%+5.1f P:%+5.1f", data->roll, data->pitch);
            u8g2.drawStr(0, LINE_Y(3), buf);
            break;

        #if defined(USE_ESP32) && defined(USE_WIFI)
        case 2:  // Network info
            if (data->wifi_connected) {
                snprintf(buf, sizeof(buf), "IP:%s", data->ip_address);
                u8g2.drawStr(0, LINE_Y(1), buf);
            } else {
                u8g2.drawStr(0, LINE_Y(1), "WiFi: connecting...");
            }
            if (data->ssid[0] != '\0') {
                snprintf(buf, sizeof(buf), "%.21s", data->ssid);
                u8g2.drawStr(0, LINE_Y(2), buf);
            }
            if (data->wifi_connected) {
                snprintf(buf, sizeof(buf), "RSSI: %d dBm", data->wifi_rssi);
                u8g2.drawStr(0, LINE_Y(3), buf);
            }
            break;
        #endif
    }
}

static void drawArmed(const DisplayData_t* data) {
    advanceScreen(1, 2, millis());

    switch (screen_index) {
        case 0:  // Primary flight data
            u8g2.drawStr(0, LINE_Y(1), "** ARMED **");
            snprintf(buf, sizeof(buf), "R:%+5.1f P:%+5.1f", data->roll, data->pitch);
            u8g2.drawStr(0, LINE_Y(2), buf);
            {
                int m1p = (int)(data->m1 * 100);
                int m2p = (int)(data->m2 * 100);
                int m3p = (int)(data->m3 * 100);
                int m4p = (int)(data->m4 * 100);
                snprintf(buf, sizeof(buf), "M:%d %d %d %d", m1p, m2p, m3p, m4p);
                u8g2.drawStr(0, LINE_Y(3), buf);
            }
            break;

        case 1:  // Secondary flight data
            u8g2.drawStr(0, LINE_Y(1), "** ARMED **");
            snprintf(buf, sizeof(buf), "Y:%+6.1f", data->yaw);
            u8g2.drawStr(0, LINE_Y(2), buf);
            snprintf(buf, sizeof(buf), "Loop: %luus", (unsigned long)data->loop_dt_us);
            u8g2.drawStr(0, LINE_Y(3), buf);
            break;
    }
}

#endif // DISPLAY_LINES

//========================================================================================================================//
//                                              PUBLIC FUNCTIONS                                                           //
//========================================================================================================================//

// Software I2C scan — check if a device ACKs at a given 7-bit address
// Uses the same pins as U8G2 software I2C
static bool swI2cProbe(uint8_t addr7bit) {
    // Bit-bang a START + address byte + read ACK on OLED pins
    uint8_t sda = OLED_SDA_PIN;
    uint8_t scl = OLED_SCL_PIN;

    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(10);

    // START: SDA goes low while SCL is high
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(5);
    pinMode(scl, OUTPUT);
    digitalWrite(scl, LOW);
    delayMicroseconds(5);

    // Send 7-bit address + write bit (0)
    uint8_t byte = (addr7bit << 1) | 0;  // write
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            pinMode(sda, INPUT_PULLUP);  // HIGH
        } else {
            pinMode(sda, OUTPUT);
            digitalWrite(sda, LOW);
        }
        delayMicroseconds(2);
        pinMode(scl, INPUT_PULLUP);  // clock high
        delayMicroseconds(5);
        pinMode(scl, OUTPUT);
        digitalWrite(scl, LOW);  // clock low
        delayMicroseconds(2);
    }

    // Read ACK: release SDA, clock one bit, check if SDA is LOW (ACK)
    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(2);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(5);
    bool ack = (digitalRead(sda) == LOW);
    pinMode(scl, OUTPUT);
    digitalWrite(scl, LOW);

    // STOP: SDA goes high while SCL is high
    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(2);
    pinMode(scl, INPUT_PULLUP);
    delayMicroseconds(5);
    pinMode(sda, INPUT_PULLUP);
    delayMicroseconds(5);

    return ack;
}

void setupDisplay() {
    Serial.print(F("OLED: pins SDA="));
    Serial.print(OLED_SDA_PIN);
    Serial.print(F(" SCL="));
    Serial.println(OLED_SCL_PIN);

    // Scan for OLED on common SSD1306 addresses
    Serial.print(F("OLED: scanning I2C... "));
    bool found_3c = swI2cProbe(0x3C);
    bool found_3d = swI2cProbe(0x3D);

    if (found_3c) {
        Serial.println(F("found at 0x3C"));
    } else if (found_3d) {
        Serial.println(F("found at 0x3D"));
        u8g2.setI2CAddress(0x3D * 2);  // U8G2 uses 8-bit address
    } else {
        Serial.println(F("NO DEVICE FOUND!"));
        Serial.println(F("  Check wiring: OLED SDA->Pin 16, SCL->Pin 17, VCC->3.3V"));
        Serial.println(F("  If still failing, try swapping SDA/SCL wires"));
        return;  // Don't try to init a display that isn't there
    }

    u8g2.begin();
    u8g2.setFont(u8g2_font_6x10_tf);
    displayStartupMessage("FLOPPI FC");
    delay(2000);  // Hold startup message so user can confirm display works
    Serial.println(F("OLED: display initialized OK"));
}

void displayStartupMessage(const char* message) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, LINE_Y(1), message);
    u8g2.drawStr(0, LINE_Y(2), "Starting...");
    u8g2.sendBuffer();
}

void populateDisplayData(DisplayData_t* data) {
    data->armed = armedFly;
    data->roll = roll_IMU;
    data->pitch = pitch_IMU;
    data->yaw = yaw_IMU;
    data->accX = AccX;
    data->accY = AccY;
    data->accZ = AccZ;
    data->gyroX = GyroX;
    data->gyroY = GyroY;
    data->gyroZ = GyroZ;
    data->accErrorX = AccErrorX;
    data->accErrorY = AccErrorY;
    data->accErrorZ = AccErrorZ;
    data->gyroErrorX = GyroErrorX;
    data->gyroErrorY = GyroErrorY;
    data->gyroErrorZ = GyroErrorZ;
    data->m1 = m1_command_scaled;
    data->m2 = m2_command_scaled;
    data->m3 = m3_command_scaled;
    data->m4 = m4_command_scaled;
    data->loop_dt_us = (uint32_t)(dt * 1000000.0f);
    data->timestamp_us = current_time;

    #ifdef CALIBRATION_MODE
    data->calibration_in_progress = calibration_in_progress;
    data->calibration_mode = (uint8_t)calibration_mode;
    #else
    data->calibration_in_progress = false;
    data->calibration_mode = 0;
    #endif
}

void renderDisplay(const DisplayData_t* data) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    if (data->calibration_in_progress) {
        drawCalibrating(data);
    } else if (data->armed) {
        drawArmed(data);
    } else {
        drawIdle(data);
    }

    u8g2.sendBuffer();
}

#endif // USE_OLED_DISPLAY
