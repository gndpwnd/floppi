/*
 * wifi_connect — liftable ESP32 WiFi STA connect helper
 *
 * Hides ALL per-mode #ifdef logic behind two functions so the auth-mode
 * selection (OPEN / PSK / WPA3-SAE / Enterprise EAP) is invisible to callers.
 * Depends on nothing project-specific (no DisplayData_t) so it drops cleanly
 * into other Arduino-ESP32 (core 3.0.x) projects: copy this pair plus a
 * config.h selector + wifi_credentials.h + (optional) wifi_certs.h.
 *
 * The compile-time selector lives in config.h (WIFI_AUTH_*), secrets in
 * wifi_credentials.h, certs in wifi_certs.h. Validation (#error) is in
 * wifi_config.h, which includes this header.
 *
 * STA mode only. ESP32 / ESP32-S3 (identical WiFi/STA API on both).
 */

#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#if defined(USE_ESP32) && defined(USE_WIFI)

// Apply the optional WIFI_HOSTNAME (if configured). MUST be called BEFORE
// WiFi.mode(WIFI_STA): on Arduino-ESP32 core 3.x the STA hostname is pushed to
// the netif inside WiFi.mode(), so setHostname() after mode-set is ignored on
// the first association. No-op (and keeps the default MAC-derived hostname)
// when WIFI_HOSTNAME is undefined.
void wifiApplyHostname();

// Apply static IP (if configured) and start the connection for the
// compile-time-selected WIFI_AUTH_* mode. Non-blocking: returns immediately
// after WiFi.begin(...) — the caller polls WiFi.status() for the result.
// Caller must have already called wifiApplyHostname() then WiFi.mode(WIFI_STA).
void wifiConnectBegin();

// Human-readable name of the active auth mode, for logging.
// e.g. "OPEN" / "PSK" / "WPA3-SAE" / "EAP-PEAP" / "EAP-TTLS" / "EAP-TLS".
const char* wifiAuthModeName();

#endif // USE_ESP32 && USE_WIFI

#endif // WIFI_CONNECT_H
