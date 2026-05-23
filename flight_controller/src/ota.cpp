/*
 * OTA (Over-The-Air) Firmware Update
 * Uses ArduinoOTA (built into ESP32 Arduino core).
 *
 * Architecture: Runs on Core 1 alongside WiFi/display.
 * Safety: Only processes OTA when disarmed (armedFly == false).
 *         This prevents firmware flash during flight.
 *
 * Usage: Once connected to WiFi, upload via PlatformIO:
 *   pio run -t upload --upload-port floppi-XXXX.local
 *
 * Or via Arduino IDE OTA target.
 */

#include "config.h"

#if defined(USE_ESP32) && defined(USE_OTA)

#include "ota.h"
#include "globals.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

// SEC-02: OTA must be password-protected. The credential lives in
// wifi_credentials.h (OTA_PASSWORD plaintext, or OTA_PASSWORD_HASH = MD5).
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif

// Enforce a non-empty OTA credential. Without this, an OTA-enabled image would
// accept firmware from any LAN peer (remote code execution). A hard compile
// error (not a silent default) forces the user to choose a password — this is
// the safer ergonomic per the audit, because a "safe default" password is no
// better than none once it is public in the repo.
//
// Presence is checked with the preprocessor (#error). Emptiness/length cannot
// be inspected by the preprocessor on a string literal, so a non-empty value is
// enforced with static_assert in the C++ body of setupOTA() below.
#if !defined(OTA_PASSWORD) && !defined(OTA_PASSWORD_HASH)
    #error "USE_OTA is enabled but no OTA credential is defined (SEC-02). Define OTA_PASSWORD (or OTA_PASSWORD_HASH) in wifi_credentials.h."
#endif

void setupOTA() {
    // SEC-02: reject an empty/placeholder-length credential at compile time.
    // sizeof("") == 1 (just the NUL); a real password is longer.
#if defined(OTA_PASSWORD_HASH)
    static_assert(sizeof(OTA_PASSWORD_HASH) == 33,
        "OTA_PASSWORD_HASH must be a 32-char lowercase MD5 hash (set it in wifi_credentials.h, or use OTA_PASSWORD instead).");
#else
    static_assert(sizeof(OTA_PASSWORD) > 1,
        "OTA_PASSWORD is empty. USE_OTA requires a non-empty password (SEC-02). Set OTA_PASSWORD (or OTA_PASSWORD_HASH) in wifi_credentials.h.");
#endif

    // Use same hostname pattern as web server (floppi-XXXX)
    String hostname = "floppi";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "-%02X%02X", mac[4], mac[5]);
    hostname += suffix;

    ArduinoOTA.setHostname(hostname.c_str());

    // SEC-02: require authentication for OTA. Prefer the MD5 hash form (keeps
    // the plaintext password out of the firmware's flash strings); fall back to
    // plaintext setPassword(). The compile-time guard above guarantees exactly
    // one of these is defined and non-empty.
#if defined(OTA_PASSWORD_HASH)
    ArduinoOTA.setPasswordHash(OTA_PASSWORD_HASH);
#else
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.printf("[OTA] Updating %s...\n", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        Serial.println(F("\n[OTA] Update complete. Rebooting..."));
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] %u%%\r", (progress * 100) / total);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println(F("Auth failed"));
        else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin failed"));
        else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect failed"));
        else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive failed"));
        else if (error == OTA_END_ERROR) Serial.println(F("End failed"));
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready at %s.local\n", hostname.c_str());
}

void handleOTA() {
    // Only process OTA when disarmed — never flash during flight
    if (!armedFly) {
        ArduinoOTA.handle();
    }
}

#endif // USE_ESP32 && USE_OTA
