/*
 * Web Status Server Implementation
 * ESPAsyncWebServer with JSON API, WebSocket streaming, and mDNS.
 *
 * Runs entirely on Core 1. Flight control on Core 0 never touches this.
 *
 * Endpoints:
 *   GET  /api/status    - JSON snapshot of current telemetry
 *   WS   /ws            - WebSocket for real-time telemetry streaming
 *   GET  /              - Simple text status page
 *
 * mDNS: http://floppi.local (or floppi-XXXX.local with MAC suffix)
 *
 * See: docs/findings/esp32-wifi-connectivity.md
 */

#include "config.h"

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)

#include "web_server.h"
#include "radioComm.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Optional barometer telemetry — read directly from the baro module's
// spinlock-guarded snapshot. Self-contained per contradiction C-3: this adds
// no field to DisplayData_t, so the schema struct is untouched.
#ifdef USE_BAROMETER
#include "barometer.h"
#endif

// Optional GPS passthrough telemetry — read directly from the GPS module's
// spinlock-guarded snapshot. Self-contained per contradiction C-3: this adds
// no field to DisplayData_t, so the schema struct is untouched.
#ifdef USE_GPS
#include "gps.h"
#endif

// Server and WebSocket instances
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Latest telemetry data (updated from Core 1 loop, read by async callbacks)
static portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
static DisplayData_t latestData = {};

// WebSocket broadcast rate limiting
#define WS_BROADCAST_INTERVAL_MS 100  // 10Hz
static unsigned long ws_broadcast_timer = 0;

// mDNS hostname (e.g. "floppi-A1B2"), computed once in setupWebServer()
static char mdns_hostname[20] = "";

//========================================================================================================================//
//                                              HELPER: SERIALIZE DATA                                                     //
//========================================================================================================================//

static void serializeDisplayData(JsonObject& root, const DisplayData_t* data) {
    // System state
    root["armed"] = data->armed;
    root["calibrating"] = data->calibration_in_progress;

    // Attitude
    root["roll"] = serialized(String(data->roll, 2));
    root["pitch"] = serialized(String(data->pitch, 2));
    root["yaw"] = serialized(String(data->yaw, 2));

    // IMU raw
    JsonObject imu = root["imu"].to<JsonObject>();
    imu["ax"] = serialized(String(data->accX, 4));
    imu["ay"] = serialized(String(data->accY, 4));
    imu["az"] = serialized(String(data->accZ, 4));
    imu["gx"] = serialized(String(data->gyroX, 2));
    imu["gy"] = serialized(String(data->gyroY, 2));
    imu["gz"] = serialized(String(data->gyroZ, 2));

    // Motors (scaled 0-1)
    JsonObject motors = root["motors"].to<JsonObject>();
    motors["m1"] = serialized(String(data->m1, 3));
    motors["m2"] = serialized(String(data->m2, 3));
    motors["m3"] = serialized(String(data->m3, 3));
    motors["m4"] = serialized(String(data->m4, 3));

    // Performance
    root["loop_us"] = data->loop_dt_us;

    // Network
    JsonObject net = root["net"].to<JsonObject>();
    net["mac"] = data->mac_address;
    net["hostname"] = mdns_hostname;
    net["ssid"] = data->ssid;
    net["ip"] = data->ip_address;
    net["rssi"] = data->wifi_rssi;
    net["connected"] = data->wifi_connected;

#ifdef USE_BAROMETER
    // Barometer telemetry (telemetry-only — Core-1 task snapshot).
    // Schema change: adds a "baro" object to /api/status and /ws. Per the
    // swarm-API contract (swarm_api_contract_2026-05-20.md §7), any schema
    // change should re-stamp that doc's "Verified" block — flagged for the
    // merged telemetry workstream, not silently relied upon here.
    JsonObject baro = root["baro"].to<JsonObject>();
    baro["ok"] = baroTelemetryOk();
    baro["pressure_pa"] = serialized(String(baroTelemetryPressurePa(), 1));
    baro["altitude_m"]  = serialized(String(baroTelemetryAltitudeM(), 2));
    baro["temp_c"]      = serialized(String(baroTelemetryTemperatureC(), 2));
#endif

#ifdef USE_GPS
    // GPS passthrough telemetry (raw NMEA — Core-1 task snapshot, Flavour A).
    // PASSTHROUGH ONLY: gps.nmea is the most-recent complete sentence verbatim;
    // the FC parses nothing. gps.ok is a liveness bit (a sentence arrived
    // within GPS_STALE_TIMEOUT_MS), NOT a fix-quality bit — fix status lives
    // inside the NMEA text for the consumer to parse. gps.age_ms is the age of
    // that sentence. Schema change: adds a "gps" object to /api/status and /ws.
    // Per the swarm-API contract (swarm_api_contract_2026-05-20.md §7), any
    // schema change should re-stamp that doc's "Verified" block — flagged for
    // the merged telemetry workstream, not silently relied upon here.
    {
        char gps_nmea[GPS_NMEA_MAX];
        uint32_t gps_age = gpsTelemetryNMEA(gps_nmea, sizeof(gps_nmea));
        JsonObject gps = root["gps"].to<JsonObject>();
        gps["nmea"]   = gps_nmea;
        gps["age_ms"] = gps_age;
        gps["ok"]     = gpsTelemetryOk();
    }
#endif

    // System info
    root["heap"] = ESP.getFreeHeap();
    root["uptime_ms"] = millis();
}

//========================================================================================================================//
//                                              WEBSOCKET HANDLER                                                          //
//========================================================================================================================//

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected\n", client->id());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA: {
            // Parse incoming commands: {"ch1":1500,"ch2":1500,"ch3":1000,"ch4":1500,"ch5":1000,"ch6":1000}
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                JsonDocument doc;
                if (!deserializeJson(doc, data, len)) {
                    uint16_t ch1 = doc["ch1"] | (uint16_t)1500;
                    uint16_t ch2 = doc["ch2"] | (uint16_t)1500;
                    uint16_t ch3 = doc["ch3"] | (uint16_t)1000;
                    uint16_t ch4 = doc["ch4"] | (uint16_t)1500;
                    uint16_t ch5 = doc["ch5"] | (uint16_t)1000;
                    uint16_t ch6 = doc["ch6"] | (uint16_t)1000;
                    setWifiCommandChannels(ch1, ch2, ch3, ch4, ch5, ch6);
                }
            }
            break;
        }
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

//========================================================================================================================//
//                                              PUBLIC FUNCTIONS                                                            //
//========================================================================================================================//

void setupWebServer() {
    // mDNS: use MAC suffix for unique drone names in swarm
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(mdns_hostname, sizeof(mdns_hostname), "floppi-%02X%02X", mac[4], mac[5]);

    if (MDNS.begin(mdns_hostname)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[Web] mDNS: http://%s.local\n", mdns_hostname);
    }

    // WebSocket handler
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // GET /api/status - JSON telemetry snapshot
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncJsonResponse* response = new AsyncJsonResponse();
        JsonObject root = response->getRoot().to<JsonObject>();

        DisplayData_t snapshot;
        portENTER_CRITICAL(&dataMux);
        snapshot = latestData;
        portEXIT_CRITICAL(&dataMux);

        serializeDisplayData(root, &snapshot);
        response->setLength();
        request->send(response);
    });

    // POST /api/commands - receive channel values from external controller
    // JSON body: {"ch1":1500,"ch2":1500,"ch3":1000,"ch4":1500,"ch5":1000,"ch6":1000}
    // Values: 1000-2000 microseconds (same as RC receiver output)
    server.on("/api/commands", HTTP_POST,
        [](AsyncWebServerRequest* request) {},  // Headers callback (unused)
        NULL,  // Upload handler (unused)
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index + len != total) return;  // Wait for complete body

            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            uint16_t ch1 = doc["ch1"] | (uint16_t)1500;
            uint16_t ch2 = doc["ch2"] | (uint16_t)1500;
            uint16_t ch3 = doc["ch3"] | (uint16_t)1000;
            uint16_t ch4 = doc["ch4"] | (uint16_t)1500;
            uint16_t ch5 = doc["ch5"] | (uint16_t)1000;
            uint16_t ch6 = doc["ch6"] | (uint16_t)1000;

            setWifiCommandChannels(ch1, ch2, ch3, ch4, ch5, ch6);
            request->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // GET / - simple text status
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        DisplayData_t snapshot;
        portENTER_CRITICAL(&dataMux);
        snapshot = latestData;
        portEXIT_CRITICAL(&dataMux);

        char buf[320];
        snprintf(buf, sizeof(buf),
            "FLOPPI FC\n"
            "Hostname: %s\n"
            "MAC: %s\n"
            "Armed: %s\n"
            "Roll: %.1f  Pitch: %.1f  Yaw: %.1f\n"
            "Loop: %lu us\n"
            "IP: %s  RSSI: %d dBm\n"
            "Heap: %u bytes\n",
            mdns_hostname,
            snapshot.mac_address,
            snapshot.armed ? "YES" : "NO",
            snapshot.roll, snapshot.pitch, snapshot.yaw,
            (unsigned long)snapshot.loop_dt_us,
            snapshot.ip_address, snapshot.wifi_rssi,
            ESP.getFreeHeap());
        request->send(200, "text/plain", buf);
    });

    server.begin();
    Serial.println(F("[Web] Server started on port 80"));
}

void handleWebServer(const DisplayData_t* data) {
    // Update shared data for async callbacks
    portENTER_CRITICAL(&dataMux);
    latestData = *data;
    portEXIT_CRITICAL(&dataMux);

    // Cleanup dead WebSocket connections
    ws.cleanupClients();

    // Broadcast telemetry via WebSocket at rate limit
    unsigned long now = millis();
    if (ws.count() > 0 && (now - ws_broadcast_timer > WS_BROADCAST_INTERVAL_MS)) {
        ws_broadcast_timer = now;

        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        serializeDisplayData(root, data);

        char buf[512];
        size_t len = serializeJson(doc, buf, sizeof(buf));
        ws.textAll(buf, len);
    }
}

#endif // USE_ESP32 && USE_WEB_SERVER
