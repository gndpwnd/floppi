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

void setupOTA() {
    // Use same hostname pattern as web server (floppi-XXXX)
    String hostname = "floppi";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "-%02X%02X", mac[4], mac[5]);
    hostname += suffix;

    ArduinoOTA.setHostname(hostname.c_str());

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
