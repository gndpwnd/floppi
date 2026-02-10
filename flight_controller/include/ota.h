/*
 * OTA (Over-The-Air) Firmware Update Header
 * ESP32 WiFi firmware updates without USB connection.
 * Gated behind USE_ESP32 + USE_OTA build flags.
 *
 * Safety: OTA only processes when disarmed (motors off).
 */

#ifndef OTA_H
#define OTA_H

#if defined(USE_ESP32) && defined(USE_OTA)

// Initialize OTA server (call after WiFi connected, Core 1)
void setupOTA();

// Handle OTA requests (call from Core 1 loop, skipped when armed)
void handleOTA();

#endif // USE_ESP32 && USE_OTA

#endif // OTA_H
