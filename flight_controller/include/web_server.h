/*
 * Web Status Server Header
 * ESPAsyncWebServer with JSON API, WebSocket, and mDNS.
 * Runs on Core 1, gated behind USE_ESP32 + USE_WEB_SERVER.
 *
 * Live display of flight controller values in a browser.
 * Best for: calibration, bench testing, diagnostics.
 * Enable: #define USE_WEB_SERVER in config.h (auto-enabled with USE_WIFI)
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)

#include "display_data.h"

// Start web server, WebSocket, and mDNS (call once from setup)
void setupWebServer();

// Update server with latest data + broadcast WebSocket (call from Core 1 loop)
void handleWebServer(const DisplayData_t* data);

#endif // USE_ESP32 && USE_WEB_SERVER

#endif // WEB_SERVER_H
