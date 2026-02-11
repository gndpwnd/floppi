/*
 * WiFi Credentials
 * Edit this file with your network details. ESP32 builds only.
 *
 * WiFi Types (uncomment ONE section):
 *
 *   1. WPA2-Personal (home/lab) — set SSID + PASSWORD  [default]
 *   2. Open network             — set SSID, leave PASSWORD empty
 *   3. WPA2-Enterprise PEAP     — uncomment WIFI_USE_ENTERPRISE + EAP fields
 *   4. WPA2-Enterprise TLS      — uncomment WIFI_USE_ENTERPRISE + WIFI_USE_CERTS
 *
 * See: docs/features/wifi-configuration.md
 */

#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

// === WPA2-Personal (most common — home/lab networks) ===
#define WIFI_SSID       "YourNetworkName"
#define WIFI_PASSWORD   "YourPassword"

// === WPA2-Enterprise (eduroam, university, corporate) ===
// Uncomment this block for enterprise WiFi. Set auth method + credentials.
// #define WIFI_USE_ENTERPRISE
// #define WIFI_EAP_AUTH_METHOD WPA2_AUTH_PEAP   // WPA2_AUTH_PEAP or WPA2_AUTH_TLS
// #define WIFI_EAP_IDENTITY   "user@university.edu"
// #define WIFI_EAP_USERNAME   "user@university.edu"  // Not needed for TLS
// #define WIFI_EAP_PASSWORD   "your_password"         // Not needed for TLS

// === Enterprise certificates (optional, for networks that require them) ===
// Uncomment to include CA and/or client certificates.
// Put your PEM strings in include/wifi_certs.h (see template).
// #define WIFI_USE_CERTS

// === API Server for swarm coordination (uncomment when ready) ===
// When defined, the ESP32 will POST telemetry to this server periodically.
// #define API_SERVER_URL       "http://192.168.1.100:8080"
// #define API_POST_INTERVAL_MS 500   // How often to POST (ms), default 500

#endif // WIFI_CREDENTIALS_H
