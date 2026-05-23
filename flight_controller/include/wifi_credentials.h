/*
 * WiFi Credentials
 * Edit this file with your network details. ESP32 builds only.
 *
 * The auth MODE is chosen in include/config.h (WIFI_AUTH_MODE_* — pick ONE).
 * This file holds only the SECRETS / ADDRESSES for the mode you selected:
 *
 *   WIFI_AUTH_MODE_OPEN        — set WIFI_SSID only (no password)
 *   WIFI_AUTH_MODE_PSK         — set WIFI_SSID + WIFI_PASSWORD            [default]
 *   WIFI_AUTH_MODE_WPA3_SAE    — set WIFI_SSID + WIFI_PASSWORD (WPA3)
 *   WIFI_AUTH_MODE_ENTERPRISE  — set the EAP block (PEAP/TTLS use user/pass,
 *                           TLS uses certs in wifi_certs.h + USE_WIFI_CERTS)
 *
 * See: docs/features/wifi-configuration.md
 */

#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

// SSID is required for EVERY mode.
#define WIFI_SSID       "YourNetworkName"

// === WIFI_AUTH_MODE_PSK / WIFI_AUTH_MODE_WPA3_SAE (home/lab/WPA3 networks) ===
#define WIFI_PASSWORD   "YourPassword"

// === WIFI_AUTH_MODE_OPEN ===  (no secret needed — WIFI_SSID above is enough)

// === WIFI_AUTH_MODE_ENTERPRISE (eduroam, university, corporate) ===
// PEAP/TTLS: identity + username + password (+ optional CA cert via USE_WIFI_CERTS).
// TLS:       identity + client cert/key from wifi_certs.h (USE_WIFI_CERTS required).
//#define WIFI_EAP_IDENTITY        "user@university.edu"      // outer/real identity (required)
//#define WIFI_EAP_ANON_IDENTITY   "anonymous@university.edu" // optional outer id; omit to reuse identity
//#define WIFI_EAP_USERNAME        "user@university.edu"      // inner username (PEAP/TTLS)
//#define WIFI_EAP_PASSWORD        "your_password"            // (PEAP/TTLS)

// === USE_STATIC_IP (all four required when enabled; comma form expands into IPAddress()) ===
//#define WIFI_STATIC_IP      192,168,1,50
//#define WIFI_STATIC_GATEWAY 192,168,1,1
//#define WIFI_STATIC_SUBNET  255,255,255,0
//#define WIFI_STATIC_DNS     192,168,1,1

// === API Server for swarm coordination (uncomment when ready) ===
// When defined, the ESP32 will POST telemetry to this server periodically.
// #define API_SERVER_URL       "http://192.168.1.100:8080"
// #define API_POST_INTERVAL_MS 500   // How often to POST (ms), default 500

// === Command-surface shared token (SEC-01/03) ===
// Used ONLY when USE_API_AUTH is enabled in config.h. Every command-bearing
// request must present this token (HTTP header "X-Floppi-Token", or WS/JSON
// field "token") or it is rejected. CHANGE THIS before enabling auth — the
// placeholder below is a public default and provides no protection.
//
// This is a CONTROL-PLANE gate, not a confidentiality control: traffic is
// plaintext (SEC-06), so a passive sniffer on the LAN can read the token. Run
// the FC on an isolated SSID. The matching ground-station change is documented
// in docs/handoffs/api_auth_contract_2026-05-22.md (swarm_api side).
#define FLOPPI_CMD_TOKEN "CHANGE-ME-floppi-token"

// === OTA password (SEC-02) ===
// Used ONLY when USE_OTA is enabled. ArduinoOTA with no password lets any LAN
// peer flash arbitrary firmware (remote code execution). A non-empty password
// is REQUIRED to build an OTA-enabled image — config.h / ota.cpp enforce this
// with a #error if this is left empty. CHANGE THIS from the placeholder.
//
// Plaintext form (simple). For a flash-resident hash instead, define
// OTA_PASSWORD_HASH (32-char lowercase MD5 of the password) and leave
// OTA_PASSWORD empty — ota.cpp prefers the hash when both are present.
#define OTA_PASSWORD "CHANGE-ME-floppi-ota"
// #define OTA_PASSWORD_HASH "00000000000000000000000000000000"

#endif // WIFI_CREDENTIALS_H
