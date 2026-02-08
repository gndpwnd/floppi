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
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Server and WebSocket instances
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Latest telemetry data (updated from Core 1 loop, read by async callbacks)
static portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
static DisplayData_t latestData = {};

// WebSocket broadcast rate limiting
#define WS_BROADCAST_INTERVAL_MS 100  // 10Hz
static unsigned long ws_broadcast_timer = 0;

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
    net["ssid"] = data->ssid;
    net["ip"] = data->ip_address;
    net["rssi"] = data->wifi_rssi;
    net["connected"] = data->wifi_connected;

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
        case WS_EVT_DATA:
            // Future: parse incoming commands from clients
            break;
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
    String hostname = "floppi";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[8];
    snprintf(suffix, sizeof(suffix), "-%02X%02X", mac[4], mac[5]);
    hostname += suffix;

    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[Web] mDNS: http://%s.local\n", hostname.c_str());
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

    // GET / - simple text status
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        DisplayData_t snapshot;
        portENTER_CRITICAL(&dataMux);
        snapshot = latestData;
        portEXIT_CRITICAL(&dataMux);

        char buf[256];
        snprintf(buf, sizeof(buf),
            "FLOPPI FC\n"
            "Armed: %s\n"
            "Roll: %.1f  Pitch: %.1f  Yaw: %.1f\n"
            "Loop: %lu us\n"
            "IP: %s  RSSI: %d dBm\n"
            "Heap: %u bytes\n",
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
