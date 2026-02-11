/*
 * WiFi Manager Implementation
 * ESP32 WiFi STA mode - connects to existing WiFi network.
 *
 * Architecture: Runs entirely on Core 1.
 * Flight control on Core 0 never touches WiFi.
 *
 * Credentials: Edit include/wifi_credentials.h with your network details.
 *
 * Vision: Drones connect to existing infrastructure (lab, university, field router).
 *         Enables swarm coordination via centralized API servers on the same network.
 *
 * See: docs/findings/esp32-wifi-connectivity.md
 */

#if defined(USE_ESP32) && defined(USE_WIFI)

#include "wifi_config.h"
#include <WiFi.h>

// WiFi credentials — edit include/wifi_credentials.h
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#else
    #warning "wifi_credentials.h not found — create it from the template in include/"
    #define WIFI_SSID     "unconfigured"
    #define WIFI_PASSWORD ""
#endif

// Enterprise certificates (optional) — edit include/wifi_certs.h
#if defined(WIFI_USE_ENTERPRISE) && defined(WIFI_USE_CERTS)
    #if __has_include("wifi_certs.h")
        #include "wifi_certs.h"
    #else
        #warning "wifi_certs.h not found — create it from the template in include/"
    #endif
#endif

// Default auth method if not specified
#if defined(WIFI_USE_ENTERPRISE) && !defined(WIFI_EAP_AUTH_METHOD)
    #define WIFI_EAP_AUTH_METHOD WPA2_AUTH_PEAP
#endif

static bool wifi_initialized = false;
static bool wifi_connected = false;
static unsigned long last_reconnect_attempt = 0;

// Reconnection interval (ms)
#define WIFI_RECONNECT_INTERVAL 5000
// Connection timeout before giving up initial connect (ms)
#define WIFI_CONNECT_TIMEOUT 15000

void setupWiFi() {
    Serial.print(F("[WiFi] MAC: "));
    Serial.println(WiFi.macAddress());

    // Check if credentials are configured
    if (strcmp(WIFI_SSID, "unconfigured") == 0 || strcmp(WIFI_SSID, "YourNetworkName") == 0) {
        Serial.println(F("[WiFi] No credentials configured!"));
        Serial.println(F("[WiFi] Edit include/wifi_credentials.h with your network details"));
        wifi_initialized = true;
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.print(F("[WiFi] Connecting to: "));
    Serial.println(WIFI_SSID);

    #ifdef WIFI_USE_ENTERPRISE
        // WPA2-Enterprise (eduroam, university, corporate)
        Serial.print(F("[WiFi] Mode: Enterprise "));
        #if WIFI_EAP_AUTH_METHOD == WPA2_AUTH_TLS
            Serial.println(F("(TLS)"));
        #else
            Serial.println(F("(PEAP)"));
        #endif
        #ifdef WIFI_USE_CERTS
            WiFi.begin(WIFI_SSID, WIFI_EAP_AUTH_METHOD, WIFI_EAP_IDENTITY,
                       WIFI_EAP_USERNAME, WIFI_EAP_PASSWORD,
                       WIFI_CA_CERT,
                       strlen(WIFI_CLIENT_CERT) > 0 ? WIFI_CLIENT_CERT : NULL,
                       strlen(WIFI_CLIENT_KEY) > 0 ? WIFI_CLIENT_KEY : NULL);
        #else
            WiFi.begin(WIFI_SSID, WIFI_EAP_AUTH_METHOD, WIFI_EAP_IDENTITY,
                       WIFI_EAP_USERNAME, WIFI_EAP_PASSWORD);
        #endif
    #else
        // WPA2-Personal (standard home/lab networks)
        if (strlen(WIFI_PASSWORD) > 0) {
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        } else {
            Serial.println(F("[WiFi] Mode: Open network"));
            WiFi.begin(WIFI_SSID);
        }
    #endif

    // Wait for connection (with timeout so we don't block Core 1 forever)
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        Serial.print(F("[WiFi] Connected! IP: "));
        Serial.println(WiFi.localIP());
        Serial.print(F("[WiFi] RSSI: "));
        Serial.print(WiFi.RSSI());
        Serial.println(F(" dBm"));
    } else {
        Serial.println(F("[WiFi] Connection failed. Will retry in background."));
    }

    wifi_initialized = true;
}

void populateNetworkData(DisplayData_t* data) {
    // MAC is always available
    strncpy(data->mac_address, WiFi.macAddress().c_str(),
            sizeof(data->mac_address) - 1);
    data->mac_address[sizeof(data->mac_address) - 1] = '\0';

    if (!wifi_initialized) {
        data->wifi_connected = false;
        data->ssid[0] = '\0';
        data->ip_address[0] = '\0';
        data->wifi_rssi = 0;
        return;
    }

    data->wifi_connected = (WiFi.status() == WL_CONNECTED);

    // Copy configured SSID
    strncpy(data->ssid, WIFI_SSID, sizeof(data->ssid) - 1);
    data->ssid[sizeof(data->ssid) - 1] = '\0';

    if (data->wifi_connected) {
        IPAddress ip = WiFi.localIP();
        snprintf(data->ip_address, sizeof(data->ip_address),
                 "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        data->wifi_rssi = (int8_t)WiFi.RSSI();
    } else {
        strncpy(data->ip_address, "connecting...", sizeof(data->ip_address) - 1);
        data->ip_address[sizeof(data->ip_address) - 1] = '\0';
        data->wifi_rssi = 0;
    }
}

void handleWiFi() {
    if (!wifi_initialized || strcmp(WIFI_SSID, "unconfigured") == 0 || strcmp(WIFI_SSID, "YourNetworkName") == 0) {
        return;
    }

    bool currently_connected = (WiFi.status() == WL_CONNECTED);

    if (!currently_connected) {
        // Try reconnecting periodically
        unsigned long now = millis();
        if (now - last_reconnect_attempt > WIFI_RECONNECT_INTERVAL) {
            last_reconnect_attempt = now;
            WiFi.reconnect();
        }
    }

    if (currently_connected && !wifi_connected) {
        wifi_connected = true;
        Serial.print(F("[WiFi] Connected! IP: "));
        Serial.println(WiFi.localIP());
    } else if (!currently_connected && wifi_connected) {
        wifi_connected = false;
        Serial.println(F("[WiFi] Disconnected. Reconnecting..."));
    }
}

#endif // USE_ESP32 && USE_WIFI
