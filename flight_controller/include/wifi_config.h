/*
 * WiFi Configuration Module Header
 * ESP32 WiFi AP mode and network info.
 * Gated behind USE_ESP32 + USE_WIFI build flags.
 *
 * See: docs/findings/esp32-wifi-connectivity.md
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#if defined(USE_ESP32) && defined(USE_WIFI)

#include "display_data.h"

// Initialize WiFi in AP mode (call from Core 1)
void setupWiFi();

// Populate network fields in display data (call from Core 1)
void populateNetworkData(DisplayData_t* data);

// Handle WiFi tasks (call periodically from Core 1 loop)
void handleWiFi();

#endif // USE_ESP32 && USE_WIFI

#endif // WIFI_CONFIG_H
