/*
 * API Client Implementation
 * HTTP POST/GET to centralized servers for swarm coordination.
 *
 * Runs on Core 1. Only active when API_SERVER_URL is defined
 * in wifi_credentials.h.
 *
 * Architecture:
 *   - POST telemetry to {API_SERVER_URL}/api/telemetry at configurable rate
 *   - GET commands from {API_SERVER_URL}/api/commands (future)
 *   - Blocking HTTP calls are OK on Core 1 (not time-critical)
 *
 * See: docs/findings/esp32-wifi-connectivity.md
 */

#include "config.h"

#if defined(USE_ESP32) && defined(USE_API_SERVER)

#include "api_client.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Optional barometer telemetry — read directly from the baro module's
// spinlock-guarded snapshot. Mirrors the "baro" block emitted by
// serializeDisplayData() in web_server.cpp. Self-contained: adds no field
// to DisplayData_t. Compiles out entirely when USE_BAROMETER is undefined.
#ifdef USE_BAROMETER
#include "barometer.h"
#endif

// Optional GPS passthrough telemetry — read directly from the GPS module's
// spinlock-guarded snapshot. Mirrors the "gps" block emitted by
// serializeDisplayData() in web_server.cpp. Compiles out entirely when
// USE_GPS is undefined.
#ifdef USE_GPS
#include "gps.h"
#endif

// Only compile API client when server URL is configured
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif

#ifdef API_SERVER_URL

// Default POST interval: 500ms (2Hz) — configurable in wifi_credentials.h
#ifndef API_POST_INTERVAL_MS
    #define API_POST_INTERVAL_MS 500
#endif

static unsigned long api_post_timer = 0;
static bool api_enabled = false;
static char telemetry_url[128];
static char commands_url[128];

// Drone identifier (MAC-based)
static char drone_id[18];

void setupApiClient() {
    // Build endpoint URLs
    snprintf(telemetry_url, sizeof(telemetry_url), "%s/api/telemetry", API_SERVER_URL);
    snprintf(commands_url, sizeof(commands_url), "%s/api/commands", API_SERVER_URL);

    // Drone ID from MAC
    strncpy(drone_id, WiFi.macAddress().c_str(), sizeof(drone_id) - 1);
    drone_id[sizeof(drone_id) - 1] = '\0';

    api_enabled = true;
    Serial.printf("[API] Client enabled → %s\n", API_SERVER_URL);
    Serial.printf("[API] POST interval: %d ms\n", API_POST_INTERVAL_MS);
}

void handleApiClient(const DisplayData_t* data) {
    if (!api_enabled || WiFi.status() != WL_CONNECTED) {
        return;
    }

    unsigned long now = millis();
    if (now - api_post_timer < API_POST_INTERVAL_MS) {
        return;
    }
    api_post_timer = now;

    // Build telemetry JSON
    JsonDocument doc;
    doc["drone_id"] = drone_id;
    doc["armed"] = data->armed;
    doc["roll"] = data->roll;
    doc["pitch"] = data->pitch;
    doc["yaw"] = data->yaw;
    doc["loop_us"] = data->loop_dt_us;

    JsonObject motors = doc["motors"].to<JsonObject>();
    motors["m1"] = data->m1;
    motors["m2"] = data->m2;
    motors["m3"] = data->m3;
    motors["m4"] = data->m4;

    doc["rssi"] = data->wifi_rssi;
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime_ms"] = millis();

#ifdef USE_BAROMETER
    // Barometer telemetry — mirrors the "baro" block in serializeDisplayData()
    // (web_server.cpp). Same field names, types, units and fixed-decimal
    // emission. Absent entirely from the POST body when USE_BAROMETER is OFF.
    JsonObject baro = doc["baro"].to<JsonObject>();
    baro["ok"] = baroTelemetryOk();
    baro["pressure_pa"] = serialized(String(baroTelemetryPressurePa(), 1));
    baro["altitude_m"]  = serialized(String(baroTelemetryAltitudeM(), 2));
    baro["temp_c"]      = serialized(String(baroTelemetryTemperatureC(), 2));
#endif

#ifdef USE_GPS
    // GPS passthrough telemetry — mirrors the "gps" block in
    // serializeDisplayData() (web_server.cpp). gps.nmea is the most-recent
    // complete sentence verbatim; gps.ok is a liveness bit; gps.age_ms is the
    // age of that sentence. Absent entirely from the POST body when USE_GPS
    // is OFF.
    {
        char gps_nmea[GPS_NMEA_MAX];
        uint32_t gps_age = gpsTelemetryNMEA(gps_nmea, sizeof(gps_nmea));
        JsonObject gps = doc["gps"].to<JsonObject>();
        gps["nmea"]   = gps_nmea;
        gps["age_ms"] = gps_age;
        gps["ok"]     = gpsTelemetryOk();
    }
#endif

    // Buffer worst case: base payload ~260 B + baro block ~90 B + gps block
    // (82-char max NMEA sentence) ~130 B ≈ 480 B. 512 B covers it with margin.
    // serializeJson() returns the actual length and never overruns the buffer.
    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));

    // POST telemetry (blocking, OK on Core 1)
    HTTPClient http;
    http.begin(telemetry_url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(2000);  // 2s timeout — don't block forever

    int code = http.POST((uint8_t*)buf, len);
    if (code < 0) {
        // Connection failed — not fatal, just skip
        static unsigned long last_error_log = 0;
        if (now - last_error_log > 10000) {  // Log errors at most every 10s
            last_error_log = now;
            Serial.printf("[API] POST failed: %s\n", http.errorToString(code).c_str());
        }
    }
    // Future: parse response for commands
    http.end();
}

#else // API_SERVER_URL not defined

void setupApiClient() {
    Serial.println(F("[API] No API_SERVER_URL configured (edit wifi_credentials.h to enable)"));
}

void handleApiClient(const DisplayData_t* data) {
    (void)data;  // unused
}

#endif // API_SERVER_URL

#endif // USE_ESP32 && USE_API_SERVER
