/*
 * wifi_connect — liftable ESP32 WiFi STA connect helper (implementation)
 *
 * All per-mode #ifdef branching lives here. See wifi_connect.h.
 *
 * Uses the Arduino-ESP32 core 3.0.x convenience APIs only (no raw
 * <esp_wpa2.h> / esp_eap_client_*): the enterprise WiFi.begin(...) overload,
 * setMinSecurity() for WPA3-SAE, config() for static IP, setHostname().
 */

#include "config.h"

#if defined(USE_ESP32) && defined(USE_WIFI)

#include "wifi_config.h"   // auth-mode selector + validation #errors for this TU
#include <WiFi.h>

// Credentials (secrets) — provided by wifi_credentials.h. Also pulled in by
// wifi_config.h before validation; included here too for standalone reuse.
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#else
    #ifndef WIFI_SSID
        #define WIFI_SSID     "unconfigured"
    #endif
    #ifndef WIFI_PASSWORD
        #define WIFI_PASSWORD ""
    #endif
#endif

// Enterprise certificates (optional) — gated by USE_WIFI_CERTS.
#if defined(WIFI_AUTH_MODE_ENTERPRISE) && defined(USE_WIFI_CERTS)
    #if __has_include("wifi_certs.h")
        #include "wifi_certs.h"
    #endif
    // Fail loudly if the CA blob is still the empty template (can't be seen by
    // the preprocessor on a string literal — mirrors ota.cpp's OTA_PASSWORD).
    static_assert(sizeof(WIFI_CA_CERT) > 1,
        "USE_WIFI_CERTS is set but WIFI_CA_CERT is empty. Put a real CA PEM in wifi_certs.h.");
#endif

//-----------------------------------------------------------------------------
// Resolve the enterprise EAP method enum + the static_assert/arg helpers.
//-----------------------------------------------------------------------------
#ifdef WIFI_AUTH_MODE_ENTERPRISE
    #if defined(WIFI_EAP_METHOD_TLS)
        #define WIFI_CONNECT_EAP_METHOD WPA2_AUTH_TLS
    #elif defined(WIFI_EAP_METHOD_TTLS)
        #define WIFI_CONNECT_EAP_METHOD WPA2_AUTH_TTLS
    #else
        #define WIFI_CONNECT_EAP_METHOD WPA2_AUTH_PEAP
    #endif

    // Outer identity. With an anonymous identity, the outer (privacy) identity
    // is the anon string and the inner username is WIFI_EAP_USERNAME; without
    // it, the real identity is used for both outer and inner.
    #if defined(WIFI_EAP_ANON_IDENTITY)
        #define WIFI_CONNECT_OUTER_IDENTITY WIFI_EAP_ANON_IDENTITY
    #else
        #define WIFI_CONNECT_OUTER_IDENTITY WIFI_EAP_IDENTITY
    #endif

    // PEAP/TTLS use username+password; TLS leaves them NULL (cert is the auth).
    #if defined(WIFI_EAP_METHOD_TLS)
        #define WIFI_CONNECT_EAP_USER NULL
        #define WIFI_CONNECT_EAP_PASS NULL
    #else
        #define WIFI_CONNECT_EAP_USER WIFI_EAP_USERNAME
        #define WIFI_CONNECT_EAP_PASS WIFI_EAP_PASSWORD
    #endif
#endif // WIFI_AUTH_MODE_ENTERPRISE

const char* wifiAuthModeName() {
#if defined(WIFI_AUTH_MODE_OPEN)
    return "OPEN";
#elif defined(WIFI_AUTH_MODE_PSK)
    return "PSK";
#elif defined(WIFI_AUTH_MODE_WPA3_SAE)
    return "WPA3-SAE";
#elif defined(WIFI_AUTH_MODE_ENTERPRISE)
    #if defined(WIFI_EAP_METHOD_TLS)
        return "EAP-TLS";
    #elif defined(WIFI_EAP_METHOD_TTLS)
        return "EAP-TTLS";
    #else
        return "EAP-PEAP";
    #endif
#else
    return "UNKNOWN";
#endif
}

void wifiApplyHostname() {
    // Optional hostname (DHCP / router table). MUST run before WiFi.mode(STA):
    // core 3.x consumes the hostname inside WiFi.mode(), and setHostname() after
    // mode-set is ignored on the first association. Undefined → keep default
    // MAC-derived hostname (no call, byte-identical to legacy behavior).
#if defined(WIFI_HOSTNAME)
    WiFi.setHostname(WIFI_HOSTNAME);
#endif
}

void wifiConnectBegin() {
    // --- Optional static IP (else DHCP) ---
#if defined(USE_STATIC_IP)
    WiFi.config(IPAddress(WIFI_STATIC_IP), IPAddress(WIFI_STATIC_GATEWAY),
                IPAddress(WIFI_STATIC_SUBNET), IPAddress(WIFI_STATIC_DNS));
#endif

    // --- Mode dispatch (exactly one compiled) ---
#if defined(WIFI_AUTH_MODE_OPEN)
    WiFi.begin(WIFI_SSID);

#elif defined(WIFI_AUTH_MODE_PSK)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);          // byte-identical to legacy path

#elif defined(WIFI_AUTH_MODE_WPA3_SAE)
    WiFi.setMinSecurity(WIFI_AUTH_WPA3_PSK);       // force SAE
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

#elif defined(WIFI_AUTH_MODE_ENTERPRISE)
    #if defined(USE_WIFI_CERTS)
        WiFi.begin(WIFI_SSID, WIFI_CONNECT_EAP_METHOD,
                   WIFI_CONNECT_OUTER_IDENTITY,
                   WIFI_CONNECT_EAP_USER, WIFI_CONNECT_EAP_PASS,
                   WIFI_CA_CERT,
                   sizeof(WIFI_CLIENT_CERT) > 1 ? WIFI_CLIENT_CERT : NULL,
                   sizeof(WIFI_CLIENT_KEY)  > 1 ? WIFI_CLIENT_KEY  : NULL);
    #else
        WiFi.begin(WIFI_SSID, WIFI_CONNECT_EAP_METHOD,
                   WIFI_CONNECT_OUTER_IDENTITY,
                   WIFI_CONNECT_EAP_USER, WIFI_CONNECT_EAP_PASS);
    #endif
#endif
}

#endif // USE_ESP32 && USE_WIFI
