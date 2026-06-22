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

// SEC-01/03: loud reminder that the WiFi command surface is UNAUTHENTICATED
// when USE_API_AUTH is off. Emitted here (a single translation unit gated by
// USE_ESP32 + USE_WEB_SERVER) rather than in config.h, so it fires exactly ONCE
// per build instead of once per TU that includes config.h. The semantic is
// preserved: this file only compiles when the command surface exists, so the
// warning appears under the same circumstances as before — just deduplicated.
#ifndef USE_API_AUTH
    #warning "USE_API_AUTH is OFF: POST /api/commands and WS /ws accept unauthenticated arm/throttle commands from any LAN peer (SEC-01/03). Enable USE_API_AUTH + set FLOPPI_CMD_TOKEN in wifi_credentials.h to gate the command surface."
#endif

// SEC-01/03: command-surface authentication. The shared token lives in
// wifi_credentials.h (FLOPPI_CMD_TOKEN) and is only enforced when USE_API_AUTH
// is defined in config.h (default OFF — backward-compatible with the existing
// swarm_api contract). Read paths (telemetry) are intentionally left open.
#ifdef USE_API_AUTH
    #if __has_include("wifi_credentials.h")
        #include "wifi_credentials.h"
    #endif
    #ifndef FLOPPI_CMD_TOKEN
        #error "USE_API_AUTH is enabled but FLOPPI_CMD_TOKEN is not defined in wifi_credentials.h (SEC-01/03)."
    #endif
    #include <string.h>

    // SEC-01/03: reject an auth-enabled build that still uses the public
    // placeholder token (or a too-short token) at COMPILE time. Mirrors the OTA
    // password guard in ota.cpp: a "safe default" credential that ships in the
    // repo is no better than none, so force the user to choose a real one rather
    // than silently building an "authenticated" image that anyone can command.
    //
    // The preprocessor cannot compare string literals, so this is enforced with
    // constexpr + static_assert in the C++ body. sizeof(literal) is length+1.
    namespace {
        // Minimum token length. Short tokens are trivially guessable/brute-forced
        // and defeat the purpose of the gate.
        constexpr size_t kMinTokenLen = 8;

        // Compile-time equality of two C string literals.
        constexpr bool ctStrEq(const char* a, const char* b) {
            return (*a == *b) && (*a == '\0' || ctStrEq(a + 1, b + 1));
        }

        static_assert(!ctStrEq(FLOPPI_CMD_TOKEN, "CHANGE-ME-floppi-token"),
            "FLOPPI_CMD_TOKEN is still the placeholder default. USE_API_AUTH provides "
            "no protection with the public placeholder token (SEC-01/03). Set a unique "
            "FLOPPI_CMD_TOKEN in wifi_credentials.h.");

        static_assert(sizeof(FLOPPI_CMD_TOKEN) - 1 >= kMinTokenLen,
            "FLOPPI_CMD_TOKEN is too short. USE_API_AUTH requires a token of at least "
            "8 characters (SEC-01/03). Set a longer FLOPPI_CMD_TOKEN in wifi_credentials.h.");
    } // namespace
#endif

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
        JsonObject gps = root["gps"].to<JsonObject>();
#if GPS_TELEMETRY_INCLUDE_POSITION
        // SEC-04: position included (default). The raw NMEA sentence carries
        // lat/lon/alt and fix/sat counts verbatim.
        char gps_nmea[GPS_NMEA_MAX];
        uint32_t gps_age = gpsTelemetryNMEA(gps_nmea, sizeof(gps_nmea));
        gps["nmea"]   = gps_nmea;
        gps["age_ms"] = gps_age;
#else
        // SEC-04: position withheld (GPS_TELEMETRY_INCLUDE_POSITION == 0). Omit
        // the raw NMEA sentence entirely — it is the only carrier of lat/lon on
        // a pure-passthrough FC. Still publish the non-identifying liveness
        // fields so consumers can tell the GPS is alive without learning where.
        // We still call gpsTelemetryNMEA() (into a discarded buffer) only to get
        // the age; cheaper would be a dedicated age accessor, but this keeps the
        // change additive and the field set consistent.
        char gps_nmea[GPS_NMEA_MAX];
        uint32_t gps_age = gpsTelemetryNMEA(gps_nmea, sizeof(gps_nmea));
        gps["age_ms"] = gps_age;
#endif
        gps["ok"]     = gpsTelemetryOk();
    }
#endif

    // System info
    root["heap"] = ESP.getFreeHeap();
    root["uptime_ms"] = millis();
}

//========================================================================================================================//
//                                              COMMAND-SURFACE AUTH (SEC-01/03)                                           //
//========================================================================================================================//
// All auth helpers compile to nothing unless USE_API_AUTH is defined. When the
// flag is off, the command surface behaves exactly as before (open) — zero
// added overhead, no behavior change. Auth runs entirely on Core 1 inside the
// async callbacks; Core 0's flight loop never executes any of this.

#ifdef USE_API_AUTH

// Fixed-time token compare. The expected token length is a compile-time
// constant (FLOPPI_CMD_TOKEN is a literal), so we always walk exactly that many
// bytes regardless of the candidate: no early return on first mismatch and no
// length-based early exit. A length mismatch is folded into the same
// accumulator as a byte mismatch. The accumulator is volatile so the compiler
// cannot reintroduce a short-circuit. Cost is a handful of bytes on Core 1.
static bool tokenMatches(const char* candidate) {
    if (candidate == nullptr) return false;
    const char* expected = FLOPPI_CMD_TOKEN;
    // sizeof includes the NUL; el is the token's character count.
    constexpr size_t el = sizeof(FLOPPI_CMD_TOKEN) - 1;
    volatile uint8_t diff = 0;
    size_t ci = 0;            // index into candidate, advanced without branching
    uint8_t past_end = 0;     // becomes nonzero once candidate's NUL is seen
    for (size_t i = 0; i < el; i++) {
        // Read the candidate byte at ci; substitute 0 once we are past its NUL.
        uint8_t cb = (uint8_t)candidate[ci];
        // past_end stays "sticky" once cb is the terminating NUL.
        past_end |= (uint8_t)(cb == 0);
        uint8_t use = past_end ? (uint8_t)0 : cb;
        diff |= (uint8_t)((uint8_t)expected[i] ^ use);
        // Advance ci only while still within the candidate string. This keeps
        // the read in-bounds without leaking a comparison via control flow that
        // depends on the secret (it depends on the candidate, not the token).
        ci += (past_end ? 0u : 1u);
    }
    // Length-mismatch term: the candidate must end exactly at el. If it has more
    // bytes, candidate[el] is non-NUL; if fewer, past_end was set early. Fold a
    // single trailing-byte check in so a longer candidate fails.
    diff |= (uint8_t)(past_end ? 0 : (uint8_t)(candidate[el] != 0));
    return diff == 0;
}

// HTTP request: accept the token from the "X-Floppi-Token" header.
static bool httpAuthorized(AsyncWebServerRequest* request) {
    if (request->hasHeader("X-Floppi-Token")) {
        return tokenMatches(request->getHeader("X-Floppi-Token")->value().c_str());
    }
    return false;
}

// WebSocket / JSON body: accept the token from the "token" field of the
// command document (the WS frame has no headers).
static bool jsonAuthorized(JsonDocument& doc) {
    const char* tok = doc["token"] | (const char*)nullptr;
    return tokenMatches(tok);
}

#endif // USE_API_AUTH

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
#ifdef USE_API_AUTH
                    // SEC-01/03: reject unauthenticated command frames. The WS
                    // protocol has no headers, so the token rides in the JSON
                    // "token" field. Silently drop unauthorized frames (no
                    // info-leaking error reply on the command channel).
                    if (!jsonAuthorized(doc)) {
                        break;
                    }
#endif
                    uint16_t ch1 = doc["ch1"] | (uint16_t)1500;
                    uint16_t ch2 = doc["ch2"] | (uint16_t)1500;
                    uint16_t ch3 = doc["ch3"] | (uint16_t)1000;
                    uint16_t ch4 = doc["ch4"] | (uint16_t)1500;
                    uint16_t ch5 = doc["ch5"] | (uint16_t)1000;
                    uint16_t ch6 = doc["ch6"] | (uint16_t)1000;
                    // Defense-in-depth: clamp each channel to the valid RC pulse
                    // range [1000, 2000] us before forwarding. getDesState()
                    // also constrain()s downstream, but clamping here keeps the
                    // command surface robust against out-of-range input.
                    ch1 = constrain(ch1, 1000, 2000);
                    ch2 = constrain(ch2, 1000, 2000);
                    ch3 = constrain(ch3, 1000, 2000);
                    ch4 = constrain(ch4, 1000, 2000);
                    ch5 = constrain(ch5, 1000, 2000);
                    ch6 = constrain(ch6, 1000, 2000);
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

#ifdef USE_API_AUTH
            // SEC-01/03: reject unauthenticated command POSTs before parsing.
            // Token is presented via the "X-Floppi-Token" header (preferred);
            // a "token" JSON field is also accepted as a fallback below.
            bool authed = httpAuthorized(request);
#endif

            JsonDocument doc;
            if (deserializeJson(doc, (const char*)data, len)) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

#ifdef USE_API_AUTH
            if (!authed && !jsonAuthorized(doc)) {
                request->send(401, "application/json", "{\"error\":\"unauthorized\"}");
                return;
            }
#endif

            uint16_t ch1 = doc["ch1"] | (uint16_t)1500;
            uint16_t ch2 = doc["ch2"] | (uint16_t)1500;
            uint16_t ch3 = doc["ch3"] | (uint16_t)1000;
            uint16_t ch4 = doc["ch4"] | (uint16_t)1500;
            uint16_t ch5 = doc["ch5"] | (uint16_t)1000;
            uint16_t ch6 = doc["ch6"] | (uint16_t)1000;

            // Defense-in-depth: clamp each channel to the valid RC pulse range
            // [1000, 2000] us before forwarding. getDesState() also constrain()s
            // downstream, but clamping here keeps the command surface robust
            // against out-of-range input.
            ch1 = constrain(ch1, 1000, 2000);
            ch2 = constrain(ch2, 1000, 2000);
            ch3 = constrain(ch3, 1000, 2000);
            ch4 = constrain(ch4, 1000, 2000);
            ch5 = constrain(ch5, 1000, 2000);
            ch6 = constrain(ch6, 1000, 2000);

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

        // Roll/pitch/yaw are physically bounded attitude angles. Pre-format
        // via clamped integer math so GCC can statically bound the snprintf
        // output (otherwise %.1f triggers -Wformat-truncation against the
        // 320-byte buffer for theoretical FLT_MAX inputs). Hand-formatted to
        // also avoid the secondary "%ld may be truncated" warning gcc would
        // raise on snprintf-based int formatting.
        auto fmtFloat = [](char* out, size_t out_sz, float v) {
            if (out_sz == 0) return;
            if (!(v > -1000.0f)) v = -999.9f;  // also catches NaN
            if (!(v <  1000.0f)) v =  999.9f;
            bool neg = (v < 0.0f);
            int32_t scaled = (int32_t)((neg ? -v : v) * 10.0f);  // 0..9999
            int32_t whole = scaled / 10;                          // 0..999
            int32_t frac  = scaled % 10;                          // 0..9
            char tmp[8];
            int n = 0;
            if (neg) tmp[n++] = '-';
            if (whole >= 100) { tmp[n++] = '0' + (char)((whole / 100) % 10); }
            if (whole >= 10)  { tmp[n++] = '0' + (char)((whole / 10)  % 10); }
            tmp[n++] = '0' + (char)(whole % 10);
            tmp[n++] = '.';
            tmp[n++] = '0' + (char)frac;
            tmp[n] = '\0';
            size_t to_copy = ((size_t)n < out_sz - 1) ? (size_t)n : out_sz - 1;
            for (size_t i = 0; i < to_copy; ++i) out[i] = tmp[i];
            out[to_copy] = '\0';
        };
        char rollStr[8], pitchStr[8], yawStr[8];
        fmtFloat(rollStr,  sizeof(rollStr),  snapshot.roll);
        fmtFloat(pitchStr, sizeof(pitchStr), snapshot.pitch);
        fmtFloat(yawStr,   sizeof(yawStr),   snapshot.yaw);

        char buf[320];
        snprintf(buf, sizeof(buf),
            "FLOPPI FC\n"
            "Hostname: %s\n"
            "MAC: %s\n"
            "Armed: %s\n"
            "Roll: %s  Pitch: %s  Yaw: %s\n"
            "Loop: %lu us\n"
            "IP: %s  RSSI: %d dBm\n"
            "Heap: %u bytes\n",
            mdns_hostname,
            snapshot.mac_address,
            snapshot.armed ? "YES" : "NO",
            rollStr, pitchStr, yawStr,
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

        // SEC-05: the old fixed char buf[512] could silently truncate the frame
        // to invalid JSON once USE_GPS + USE_BAROMETER are both on (baro block
        // + up-to-82-char NMEA push the worst case past 512 B). Serialize into a
        // dynamically-grown String — the same approach /api/status already uses
        // via AsyncJsonResponse — so telemetry never truncates. Core 1 only;
        // the flight loop is unaffected.
        String out;
        size_t len = serializeJson(doc, out);
        if (len == 0) {
            // Serialization produced nothing (allocation failure) — skip this
            // frame rather than emit a broken/empty one. Logged sparsely.
            static unsigned long last_ws_err = 0;
            if (now - last_ws_err > 10000) {
                last_ws_err = now;
                Serial.println(F("[WS] telemetry serialize failed (low heap?) — frame skipped"));
            }
        } else {
            ws.textAll(out.c_str(), len);
        }
    }
}

#endif // USE_ESP32 && USE_WEB_SERVER
