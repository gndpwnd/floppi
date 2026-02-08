/*
 * WiFi Credentials
 * Edit this file with your network details. ESP32 builds only.
 *
 * For WPA2-Personal (home/lab networks):
 *   Set WIFI_SSID and WIFI_PASSWORD
 *
 * For WPA2-Enterprise (eduroam/university):
 *   Uncomment WIFI_USE_ENTERPRISE and set the EAP fields
 *   See: docs/findings/esp32-wifi-connectivity.md
 */

#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

// === WPA2-Personal (most common) ===
#define WIFI_SSID       "YourNetworkName"
#define WIFI_PASSWORD   "YourPassword"

// === WPA2-Enterprise / eduroam (uncomment if needed) ===
// #define WIFI_USE_ENTERPRISE
// #define WIFI_EAP_IDENTITY  "user@university.edu"
// #define WIFI_EAP_USERNAME  "user@university.edu"
// #define WIFI_EAP_PASSWORD  "your_password"

// === API Server for swarm coordination (uncomment when ready) ===
// When defined, the ESP32 will POST telemetry to this server periodically.
// #define API_SERVER_URL       "http://192.168.1.100:8080"
// #define API_POST_INTERVAL_MS 500   // How often to POST (ms), default 500

#endif // WIFI_CREDENTIALS_H
