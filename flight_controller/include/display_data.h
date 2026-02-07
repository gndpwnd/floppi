/*
 * Display Data Structure
 * Shared between flight control and display module.
 * No display library dependencies - safe to include from any module.
 */

#ifndef DISPLAY_DATA_H
#define DISPLAY_DATA_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // System state
    bool armed;
    bool calibration_in_progress;
    uint8_t calibration_mode;       // 0=none, 1=accel/gyro, 2=6pos, 3=attitude, 4=radio

    // Attitude (from Madgwick filter)
    float roll;
    float pitch;
    float yaw;

    // IMU data
    float accX, accY, accZ;
    float gyroX, gyroY, gyroZ;

    // Calibration results
    float accErrorX, accErrorY, accErrorZ;
    float gyroErrorX, gyroErrorY, gyroErrorZ;

    // Motor outputs (scaled 0-1)
    float m1, m2, m3, m4;

    // Loop timing
    uint32_t loop_dt_us;

    // Network info (ESP32 only, populated by Core 1)
    char mac_address[18];    // "AA:BB:CC:DD:EE:FF\0"
    char ssid[33];           // WiFi SSID
    char ip_address[16];     // "192.168.4.1\0"
    bool wifi_connected;
    int8_t wifi_rssi;

    // Timestamp
    uint32_t timestamp_us;
} DisplayData_t;

#endif // DISPLAY_DATA_H
