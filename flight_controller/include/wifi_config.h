/*
 * WiFi Configuration Module Header
 * ESP32 WiFi STA mode, web server, and API client.
 * Gated behind USE_ESP32 + USE_WIFI build flags.
 *
 * See: docs/findings/esp32-wifi-connectivity.md
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#if defined(USE_ESP32) && defined(USE_WIFI)

#include "display_data.h"

//=============================================================================
// WiFi auth-mode validation (single include point — fires once)
//=============================================================================
// Pull in credentials here so the validation below (and wifi_connect) always
// see the per-mode secret macros regardless of which TU includes us first.
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif

// (E) Backward-compat aliases — applied FIRST so the checks below see the new
//     names. Old user files keep building unchanged.
//
// Selector tokens use the WIFI_AUTH_MODE_* prefix (not bare WIFI_AUTH_OPEN /
// WIFI_AUTH_ENTERPRISE) to avoid colliding with the ESP-IDF wifi_auth_mode_t
// enum members of those exact names — see the note in config.h.
#ifdef WIFI_USE_ENTERPRISE
    #ifndef WIFI_AUTH_MODE_ENTERPRISE
        #define WIFI_AUTH_MODE_ENTERPRISE
    #endif
    #warning "WIFI_USE_ENTERPRISE is deprecated; use WIFI_AUTH_MODE_ENTERPRISE in config.h. Aliased for now."
#endif
#ifdef WIFI_USE_CERTS
    #ifndef USE_WIFI_CERTS
        #define USE_WIFI_CERTS
    #endif
#endif
// Legacy EAP method form (WIFI_EAP_AUTH_METHOD WPA2_AUTH_PEAP/TTLS/TLS) → new
// WIFI_EAP_METHOD_* selector, so existing wifi_credentials.h files don't break.
// The WPA2_AUTH_* tokens are runtime enum values (not preprocessor macros), so
// they can't be distinguished by the preprocessor; legacy enterprise files
// almost universally used PEAP, so map the legacy presence to PEAP. Users who
// need TLS/TTLS should adopt the new WIFI_EAP_METHOD_* selector in config.h.
#if defined(WIFI_AUTH_MODE_ENTERPRISE) && defined(WIFI_EAP_AUTH_METHOD) && \
    !defined(WIFI_EAP_METHOD_PEAP) && !defined(WIFI_EAP_METHOD_TTLS) && \
    !defined(WIFI_EAP_METHOD_TLS)
    #define WIFI_EAP_METHOD_PEAP
    #warning "WIFI_EAP_AUTH_METHOD is deprecated; use WIFI_EAP_METHOD_PEAP/TTLS/TLS in config.h. Defaulting to PEAP."
#endif

// Count selected auth modes.
#define WIFI_MODE_COUNT (defined(WIFI_AUTH_MODE_OPEN)+defined(WIFI_AUTH_MODE_PSK)+ \
                         defined(WIFI_AUTH_MODE_WPA3_SAE)+defined(WIFI_AUTH_MODE_ENTERPRISE))

// Legacy files that only set SSID+PASSWORD (no WIFI_AUTH_MODE_* and no
// enterprise alias) default to PSK so they keep working with zero edits.
#if WIFI_MODE_COUNT == 0 && !defined(WIFI_AUTH_MODE_ENTERPRISE)
    #define WIFI_AUTH_MODE_PSK
    #undef WIFI_MODE_COUNT
    #define WIFI_MODE_COUNT (defined(WIFI_AUTH_MODE_OPEN)+defined(WIFI_AUTH_MODE_PSK)+ \
                             defined(WIFI_AUTH_MODE_WPA3_SAE)+defined(WIFI_AUTH_MODE_ENTERPRISE))
#endif

// (A) exactly one auth mode
#if WIFI_MODE_COUNT == 0
    #error "No WiFi auth mode selected. Define exactly one WIFI_AUTH_MODE_* (OPEN/PSK/WPA3_SAE/ENTERPRISE) in config.h."
#elif WIFI_MODE_COUNT > 1
    #error "Multiple WiFi auth modes selected. Define exactly ONE WIFI_AUTH_MODE_* in config.h."
#endif

// (B) enterprise needs exactly one EAP method + an identity
#ifdef WIFI_AUTH_MODE_ENTERPRISE
    #if (defined(WIFI_EAP_METHOD_PEAP)+defined(WIFI_EAP_METHOD_TTLS)+defined(WIFI_EAP_METHOD_TLS)) != 1
        #error "WIFI_AUTH_MODE_ENTERPRISE requires exactly one WIFI_EAP_METHOD_* (PEAP/TTLS/TLS) in config.h."
    #endif
    #if !defined(WIFI_EAP_IDENTITY)
        #error "WIFI_AUTH_MODE_ENTERPRISE requires WIFI_EAP_IDENTITY in wifi_credentials.h."
    #endif
    #if (defined(WIFI_EAP_METHOD_PEAP)||defined(WIFI_EAP_METHOD_TTLS)) && \
        !(defined(WIFI_EAP_USERNAME) && defined(WIFI_EAP_PASSWORD))
        #error "PEAP/TTLS require WIFI_EAP_USERNAME and WIFI_EAP_PASSWORD in wifi_credentials.h."
    #endif
    #if defined(WIFI_EAP_METHOD_TLS) && !defined(USE_WIFI_CERTS)
        #error "EAP-TLS requires USE_WIFI_CERTS (client cert+key in wifi_certs.h)."
    #endif
#endif

// (C) certs flag requires the cert file
#if defined(USE_WIFI_CERTS) && !__has_include("wifi_certs.h")
    #error "USE_WIFI_CERTS set but wifi_certs.h not found (create it from include/ template)."
#endif

// (D) static IP needs all four values
#if defined(USE_STATIC_IP) && !(defined(WIFI_STATIC_IP) && defined(WIFI_STATIC_GATEWAY) && \
    defined(WIFI_STATIC_SUBNET) && defined(WIFI_STATIC_DNS))
    #error "USE_STATIC_IP requires WIFI_STATIC_IP/GATEWAY/SUBNET/DNS in wifi_credentials.h."
#endif

#include "wifi_connect.h"

// Initialize WiFi STA mode (call from Core 1 setup)
void setupWiFi();

// Populate network fields in display data (call from Core 1)
void populateNetworkData(DisplayData_t* data);

// Handle WiFi reconnection (call periodically from Core 1 loop)
void handleWiFi();

#endif // USE_ESP32 && USE_WIFI

#endif // WIFI_CONFIG_H
