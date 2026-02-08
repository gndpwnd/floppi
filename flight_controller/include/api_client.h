/*
 * API Client Header
 * HTTP POST/GET to centralized servers for swarm coordination.
 * Runs on Core 1, gated behind USE_ESP32 + USE_API_SERVER.
 *
 * Enables remote control and telemetry posting to external flight computers.
 * Best for: live flight, swarm coordination.
 * Enable: #define USE_API_SERVER in config.h (auto-enabled with USE_WIFI)
 * Requires: API_SERVER_URL defined in wifi_credentials.h.
 */

#ifndef API_CLIENT_H
#define API_CLIENT_H

#if defined(USE_ESP32) && defined(USE_API_SERVER)

#include "display_data.h"

// Initialize API client (call once from setup)
void setupApiClient();

// POST telemetry / GET commands periodically (call from Core 1 loop)
void handleApiClient(const DisplayData_t* data);

#endif // USE_ESP32 && USE_API_SERVER

#endif // API_CLIENT_H
