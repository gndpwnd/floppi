# ESP32 WiFi Connectivity Research

> Research date: 2026-02-07

## Overview

This document covers ESP32 WiFi connectivity options for the flight controller firmware, with emphasis on university/enterprise environments (eduroam), captive portal configuration, web server telemetry, and drone-specific best practices.

**Context**: The ESP32 port of the flight controller firmware (see [esp32-fc-feasibility.md](esp32-fc-feasibility.md)) identifies built-in WiFi as the major advantage over Teensy. WiFi enables wireless telemetry, configuration, and OTA updates. This research informs the roadmap items: WiFi AP mode, WebSocket telemetry, and HTTP REST API.

---

## 1. WiFi Modes

The ESP32 supports three WiFi operating modes, each suited to different use cases.

### 1.1 Station Mode (STA) -- `WIFI_STA`

The ESP32 connects to an existing WiFi network as a client, just like a laptop or phone.

```cpp
#include <WiFi.h>

const char* ssid = "MyNetwork";
const char* password = "MyPassword";

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}
```

**When to use for the drone**: Connecting to a lab WiFi network for development/debugging. Streaming telemetry to fc_tool running on a laptop on the same network. Accessing the internet for NTP time sync if needed.

### 1.2 Access Point Mode (AP) -- `WIFI_AP`

The ESP32 creates its own WiFi network. Other devices connect to it directly. No internet access, no router needed.

```cpp
#include <WiFi.h>

const char* ap_ssid = "FLOPPI-DRONE";
const char* ap_password = "floppifc123";

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());  // Usually 192.168.4.1
}
```

**When to use for the drone**: Primary mode for field use. The drone creates its own network, and fc_tool or a phone connects directly. No dependency on external infrastructure. Predictable, always-available connectivity.

**This is the recommended default mode for the flight controller.**

### 1.3 AP+STA Mode (Dual) -- `WIFI_AP_STA`

The ESP32 runs both modes simultaneously: it connects to an existing network AND creates its own AP. This is the most versatile but uses more power.

```cpp
#include <WiFi.h>

const char* sta_ssid = "LabNetwork";
const char* sta_password = "labpass123";
const char* ap_ssid = "FLOPPI-DRONE";
const char* ap_password = "floppifc123";

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP_STA);

    // Start AP
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    // Connect to existing network
    WiFi.begin(sta_ssid, sta_password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
}
```

**When to use for the drone**: Lab/development scenarios where you want both direct device access (AP) and network access (STA). Useful for OTA updates while maintaining a direct control channel.

### 1.4 Mode Recommendation for Flight Controller

| Scenario | Mode | Rationale |
|----------|------|-----------|
| Field flying | AP | No infrastructure needed, always works |
| Lab development | AP+STA | Direct access + network for OTA/internet |
| University demo | AP+STA | Direct access + eduroam for internet |
| Bench calibration | AP | Simple, no network config needed |
| Production/default | AP | Most reliable, fewest failure modes |

---

## 2. WPA2-Personal (Home/Simple Networks)

### 2.1 Standard Connection

```cpp
WiFi.mode(WIFI_STA);
WiFi.begin("NetworkName", "Password");
```

### 2.2 Open Networks (No Password)

```cpp
WiFi.begin("OpenNetwork");  // No password argument
```

### 2.3 Static IP vs DHCP

By default, ESP32 uses DHCP to get an IP address. For a predictable address (useful for bookmarking the telemetry page), configure a static IP:

```cpp
IPAddress local_IP(192, 168, 1, 200);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

void setup() {
    WiFi.mode(WIFI_STA);

    // Configure static IP BEFORE WiFi.begin()
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Static IP config failed");
    }

    WiFi.begin(ssid, password);
    // ... wait for connection
}
```

**For the drone**: DHCP is fine for STA mode (mDNS handles discovery). In AP mode, the ESP32 IS the DHCP server -- clients always get an IP in the 192.168.4.x range and the drone is always at 192.168.4.1.

---

## 3. WPA2-Enterprise (University Networks / Eduroam)

This is the most complex WiFi scenario. University networks like eduroam use WPA2-Enterprise with 802.1X authentication, which requires identity, username, and password (and sometimes a CA certificate).

### 3.1 How WPA2-Enterprise Differs

| Feature | WPA2-Personal | WPA2-Enterprise |
|---------|--------------|-----------------|
| Authentication | Pre-shared key (PSK) | 802.1X (RADIUS server) |
| Credentials | Single password | Identity + username + password |
| Certificate | None | Optional CA cert |
| Common protocol | WPA2-PSK | EAP-PEAP, EAP-TTLS |
| Example | Home WiFi | eduroam, corporate networks |

### 3.2 Arduino Core API (Current -- Arduino-ESP32 3.x)

The newer Arduino-ESP32 core (3.0+) supports WPA2-Enterprise directly through the `WiFi.begin()` overload:

```cpp
#include <WiFi.h>
// The newer core auto-selects the right header:
// esp_eap_client.h (new) or esp_wpa2.h (legacy)

#define EAP_IDENTITY "user@university.edu"   // Usually your email
#define EAP_USERNAME "user@university.edu"    // Same as identity for most eduroam
#define EAP_PASSWORD "your_password"

const char* ssid = "eduroam";

void setup() {
    Serial.begin(115200);
    delay(10);

    WiFi.disconnect(true);  // Clear previous credentials
    WiFi.mode(WIFI_STA);

    // New-style API: pass auth type and credentials directly
    WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);

    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        counter++;
        if (counter >= 60) {  // 30 second timeout
            Serial.println("\nConnection failed, restarting...");
            ESP.restart();
        }
    }

    Serial.println("\nConnected to eduroam!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
}
```

### 3.3 Legacy API (Arduino-ESP32 2.x and ESP-IDF Direct)

Older code uses the ESP-IDF functions directly. You may encounter this pattern in existing examples:

```cpp
#include <WiFi.h>
#include "esp_wpa2.h"  // Legacy header

#define EAP_IDENTITY "user@university.edu"
#define EAP_USERNAME "user@university.edu"
#define EAP_PASSWORD "your_password"

const char* ssid = "eduroam";

void setup() {
    Serial.begin(115200);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    // Legacy ESP-IDF API
    esp_wifi_sta_wpa2_ent_set_identity(
        (uint8_t*)EAP_IDENTITY, strlen(EAP_IDENTITY));
    esp_wifi_sta_wpa2_ent_set_username(
        (uint8_t*)EAP_USERNAME, strlen(EAP_USERNAME));
    esp_wifi_sta_wpa2_ent_set_password(
        (uint8_t*)EAP_PASSWORD, strlen(EAP_PASSWORD));

    // IMPORTANT: Must initialize config before enabling
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
    esp_wifi_sta_wpa2_ent_enable(&config);

    WiFi.begin(ssid);  // No password here -- credentials set above

    // ... wait for connection
}
```

### 3.4 Eduroam-Specific Configuration

Eduroam typically uses **EAP-PEAP with MSCHAPv2** inner authentication. Configuration varies by university:

| Parameter | Typical Value | Notes |
|-----------|--------------|-------|
| SSID | `eduroam` | Always lowercase |
| EAP Method | EAP-PEAP | Most common for eduroam |
| Phase 2 | MSCHAPv2 | Inner authentication |
| Identity | `user@university.edu` | Full email format |
| Username | `user@university.edu` | Often same as identity |
| Password | University password | Your login password |
| CA Certificate | Usually optional | Some universities require it |

**Getting your university's eduroam settings**: Check your university's IT help pages for "eduroam manual configuration" or "802.1X settings." The identity format varies -- some use `username@domain`, others use just `username`.

### 3.5 With CA Certificate (If Required)

Some universities require a CA certificate for verification. You can embed it as a string:

```cpp
// Root CA certificate (PEM format)
// Download from your university's IT page or https://www.geant.org/
const char* ca_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDwzCCAqugAwIBAgIBATANBgkqhkiG9w0BAQsFADCBgjELMAkGA1UEBhMCREUx\n" \
"... (certificate content) ...\n" \
"-----END CERTIFICATE-----\n";

void setup() {
    // ... WiFi mode setup ...

    // Set CA certificate before enabling WPA2 Enterprise
    esp_wifi_sta_wpa2_ent_set_ca_cert(
        (uint8_t*)ca_cert, strlen(ca_cert) + 1);

    // Set identity/username/password as before
    // ...

    WiFi.begin(ssid);
}
```

### 3.6 Common WPA2-Enterprise Issues and Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Connection timeout | Wrong credentials | Verify identity/username format with IT dept |
| Immediate disconnect | Wrong EAP method | Try `WPA2_AUTH_TTLS` instead of `WPA2_AUTH_PEAP` |
| "WPA2 Enterprise not supported" | Wrong ESP32 variant | ESP32-C3/S2 may have limited support; ESP32 and ESP32-S3 work best |
| Works once, fails on reboot | Stale credentials cached | Call `WiFi.disconnect(true)` before setup |
| Certificate error | CA cert required | Get CA cert from university IT, or try without first |
| Identity format wrong | University-specific | Try `user`, `user@domain`, `DOMAIN\user` |

**Key debugging tip**: Use `WiFi.status()` return codes to diagnose:
- `WL_IDLE_STATUS` (0) -- WiFi is idle
- `WL_NO_SSID_AVAIL` (1) -- Network not found
- `WL_CONNECTED` (3) -- Success
- `WL_CONNECT_FAILED` (4) -- Authentication failed
- `WL_DISCONNECTED` (6) -- Disconnected

### 3.7 Eduroam Resources

- Official ESP32 example: [WiFiClientEnterprise.ino](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEnterprise/WiFiClientEnterprise.ino)
- Community examples for many universities: [martinius96/ESP32-eduroam](https://github.com/martinius96/ESP32-eduroam)
- Simplified Arduino example: [JeroenBeemster/ESP32-WPA2-enterprise](https://github.com/JeroenBeemster/ESP32-WPA2-enterprise)

---

## 4. Captive Portal / WiFi Manager

A captive portal lets end users configure WiFi credentials at runtime without recompiling. The ESP32 starts in AP mode, serves a web page where the user enters their WiFi SSID and password, then switches to STA mode.

### 4.1 WiFiManager Library (tzapu/WiFiManager)

The most popular option. Works with both ESP8266 and ESP32.

```cpp
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager

WiFiManager wifiManager;

void setup() {
    Serial.begin(115200);

    // Tries to connect with saved credentials.
    // If that fails, starts AP named "FLOPPI-SETUP" with captive portal.
    // Blocks until connected or timeout.
    bool connected = wifiManager.autoConnect("FLOPPI-SETUP", "setup1234");

    if (!connected) {
        Serial.println("Failed to connect, restarting...");
        ESP.restart();
    }

    Serial.println("Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}
```

**How it works**:
1. On first boot (or when saved credentials fail), the ESP32 creates an AP named "FLOPPI-SETUP"
2. User connects to that AP with a phone/laptop
3. A captive portal web page opens automatically showing available networks
4. User selects a network and enters the password
5. ESP32 saves credentials to NVS (Non-Volatile Storage) and connects
6. On subsequent boots, it connects automatically using saved credentials

### 4.2 Custom Captive Portal (Manual Implementation)

For more control (e.g., adding eduroam fields), build your own:

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
Preferences prefs;

// HTML form for WiFi configuration
const char* configPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>FLOPPI WiFi Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: sans-serif; max-width: 400px; margin: 40px auto; padding: 0 20px; }
  input, select { width: 100%; padding: 8px; margin: 8px 0; box-sizing: border-box; }
  button { background: #007bff; color: white; padding: 10px 20px; border: none; width: 100%; }
</style>
</head>
<body>
<h2>FLOPPI WiFi Setup</h2>
<form action="/save" method="POST">
  <label>WiFi Type:</label>
  <select name="type" id="wtype" onchange="toggleFields()">
    <option value="personal">WPA2-Personal</option>
    <option value="enterprise">WPA2-Enterprise (eduroam)</option>
  </select>
  <label>SSID:</label>
  <input name="ssid" placeholder="Network name">
  <label>Password:</label>
  <input name="pass" type="password" placeholder="Password">
  <div id="entFields" style="display:none">
    <label>Identity (email):</label>
    <input name="identity" placeholder="user@university.edu">
  </div>
  <button type="submit">Save & Connect</button>
</form>
<script>
function toggleFields() {
  var t = document.getElementById('wtype').value;
  document.getElementById('entFields').style.display =
    t === 'enterprise' ? 'block' : 'none';
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", configPage);
}

void handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String type = server.arg("type");
    String identity = server.arg("identity");

    // Save to NVS
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("type", type);
    prefs.putString("identity", identity);
    prefs.end();

    server.send(200, "text/html",
        "<h2>Saved!</h2><p>Restarting to connect...</p>");
    delay(2000);
    ESP.restart();
}

void startCaptivePortal() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FLOPPI-SETUP", "setup1234");

    // Redirect all DNS queries to our IP (captive portal magic)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);  // All URLs -> config page
    server.begin();
}
```

### 4.3 Storing Credentials in NVS (Preferences Library)

The ESP32's NVS (Non-Volatile Storage) persists across reboots and reflashes (unless you erase flash). The `Preferences` library provides a clean Arduino API:

```cpp
#include <Preferences.h>
Preferences prefs;

// Save credentials
void saveWiFiCredentials(const char* ssid, const char* pass) {
    prefs.begin("wifi", false);  // namespace "wifi", read-write
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
}

// Load credentials
bool loadWiFiCredentials(String &ssid, String &pass) {
    prefs.begin("wifi", true);  // namespace "wifi", read-only
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    prefs.end();
    return ssid.length() > 0;
}

// Clear credentials (factory reset)
void clearWiFiCredentials() {
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
}
```

**NVS capacity**: 20KB default partition. More than enough for WiFi credentials, calibration values, and configuration data. Ideal for storing the calibration values that currently get hard-coded into config.h.

### 4.4 Fallback AP Pattern

Recommended pattern for the flight controller:

```cpp
void setupWiFi() {
    String ssid, pass;

    if (loadWiFiCredentials(ssid, pass) && ssid.length() > 0) {
        // Try saved credentials
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 20) {
            delay(500);
            timeout++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print("Connected to ");
            Serial.println(ssid);
            return;
        }
    }

    // Fallback: start AP mode
    Serial.println("Starting AP mode (fallback)...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FLOPPI-DRONE", "floppifc123");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}
```

---

## 5. Web Server for Status/Telemetry

### 5.1 Library Comparison

| Feature | WebServer (built-in) | ESPAsyncWebServer |
|---------|---------------------|-------------------|
| Blocking | Yes (handles one request at a time) | No (async, concurrent) |
| WebSocket | Not built-in | Built-in |
| File serving | Manual | SPIFFS/LittleFS integration |
| Dependencies | None (built-in) | AsyncTCP library |
| Complexity | Simple | Moderate |
| Memory usage | Lower | Higher |
| Concurrent clients | 1 | Multiple |

**Recommendation for flight controller**: Use **ESPAsyncWebServer**. The non-blocking behavior is critical -- the flight controller cannot afford to block on HTTP requests. WebSocket support is needed for real-time telemetry streaming.

### 5.2 ESPAsyncWebServer Setup

PlatformIO dependencies:

```ini
; platformio.ini
lib_deps =
    ESP Async WebServer
    AsyncTCP
```

### 5.3 Simple Status Page

```cpp
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Status page HTML (embedded in firmware)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>FLOPPI Flight Controller</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: monospace; background: #1a1a2e; color: #e0e0e0;
         max-width: 600px; margin: 0 auto; padding: 20px; }
  h1 { color: #0f9; }
  .card { background: #16213e; border-radius: 8px; padding: 16px;
          margin: 12px 0; }
  .label { color: #888; }
  .value { color: #0f9; font-size: 1.2em; }
  .warn { color: #f90; }
  table { width: 100%; border-collapse: collapse; }
  td { padding: 4px 8px; }
</style>
</head>
<body>
<h1>FLOPPI FC</h1>

<div class="card">
  <h3>Network</h3>
  <table>
    <tr><td class="label">MAC</td><td class="value" id="mac">--</td></tr>
    <tr><td class="label">IP</td><td class="value" id="ip">--</td></tr>
    <tr><td class="label">SSID</td><td class="value" id="ssid">--</td></tr>
    <tr><td class="label">RSSI</td><td class="value" id="rssi">--</td></tr>
  </table>
</div>

<div class="card">
  <h3>IMU</h3>
  <table>
    <tr><td class="label">Roll</td><td class="value" id="roll">--</td></tr>
    <tr><td class="label">Pitch</td><td class="value" id="pitch">--</td></tr>
    <tr><td class="label">Yaw</td><td class="value" id="yaw">--</td></tr>
  </table>
</div>

<div class="card">
  <h3>Calibration</h3>
  <table>
    <tr><td class="label">Gyro X bias</td><td class="value" id="gx">--</td></tr>
    <tr><td class="label">Gyro Y bias</td><td class="value" id="gy">--</td></tr>
    <tr><td class="label">Gyro Z bias</td><td class="value" id="gz">--</td></tr>
    <tr><td class="label">Accel X offset</td><td class="value" id="ax">--</td></tr>
    <tr><td class="label">Accel Y offset</td><td class="value" id="ay">--</td></tr>
    <tr><td class="label">Accel Z offset</td><td class="value" id="az">--</td></tr>
  </table>
</div>

<div class="card">
  <h3>Status</h3>
  <table>
    <tr><td class="label">Armed</td><td class="value" id="armed">--</td></tr>
    <tr><td class="label">Loop rate</td><td class="value" id="looprate">--</td></tr>
    <tr><td class="label">Uptime</td><td class="value" id="uptime">--</td></tr>
  </table>
</div>

<script>
var gateway = 'ws://' + window.location.hostname + '/ws';
var ws;

function initWS() {
    ws = new WebSocket(gateway);
    ws.onopen = function() { console.log('WS connected'); };
    ws.onclose = function() { setTimeout(initWS, 2000); };
    ws.onmessage = function(evt) {
        var d = JSON.parse(evt.data);
        for (var key in d) {
            var el = document.getElementById(key);
            if (el) el.textContent = d[key];
        }
    };
}
initWS();
</script>
</body>
</html>
)rawliteral";

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WS client #%u connected\n", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WS client #%u disconnected\n", client->id());
    }
}

void setupWebServer() {
    // Serve the status page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });

    // WebSocket handler
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
    Serial.println("HTTP server started");
}
```

### 5.4 mDNS for Easy Discovery

Instead of memorizing IP addresses, mDNS lets users access the drone at `http://drone.local`:

```cpp
#include <ESPmDNS.h>

void setupMDNS() {
    if (MDNS.begin("drone")) {  // hostname = "drone"
        Serial.println("mDNS: http://drone.local");
        MDNS.addService("http", "tcp", 80);       // Advertise HTTP
        MDNS.addService("floppi", "tcp", 80);     // Custom service for fc_tool discovery
    } else {
        Serial.println("mDNS failed to start");
    }
}
```

**Notes on mDNS**:
- Works on most platforms: macOS (native), Windows 10+ (native), Linux (avahi), iOS, Android (varies)
- If duplicate hostnames exist on the network, the library auto-appends a suffix (e.g., `drone-2.local`)
- The custom `_floppi._tcp` service allows fc_tool to auto-discover drones on the network using mDNS service browsing

### 5.5 WebSocket Telemetry Streaming

Send real-time telemetry data to all connected WebSocket clients:

```cpp
// Call this from the WiFi core task (Core 1), NOT from the flight loop (Core 0)
void broadcastTelemetry() {
    if (ws.count() == 0) return;  // No clients connected, skip

    // Build JSON payload
    // NOTE: Use ArduinoJson for production; manual string for simplicity here
    String json = "{";
    json += "\"roll\":" + String(roll_IMU, 2) + ",";
    json += "\"pitch\":" + String(pitch_IMU, 2) + ",";
    json += "\"yaw\":" + String(yaw_IMU, 2) + ",";
    json += "\"armed\":" + String(armed ? "true" : "false") + ",";
    json += "\"looprate\":" + String(loopRate) + ",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"ssid\":\"" + WiFi.SSID() + "\"";
    json += "}";

    ws.textAll(json);
    ws.cleanupClients();  // Free disconnected client resources
}

// In the WiFi task loop (runs on Core 1):
void wifiTaskLoop() {
    static unsigned long lastBroadcast = 0;
    unsigned long now = millis();

    if (now - lastBroadcast >= 100) {  // 10 Hz telemetry
        broadcastTelemetry();
        lastBroadcast = now;
    }
}
```

**Important**: Telemetry broadcasts MUST run on Core 1 (the WiFi/comms core), not Core 0 (the flight control loop). See [esp32-dual-core-research.md](esp32-dual-core-research.md) for the task architecture.

---

## 6. Getting Network Info

Useful functions for diagnostics, status pages, and logging:

```cpp
// MAC address -- available even before WiFi.begin()
Serial.print("MAC: ");
Serial.println(WiFi.macAddress());  // e.g., "24:6F:28:AA:BB:CC"

// Local IP -- only valid after connection
Serial.print("IP: ");
Serial.println(WiFi.localIP());  // e.g., "192.168.1.105"

// Connected SSID -- only in STA mode
Serial.print("SSID: ");
Serial.println(WiFi.SSID());  // e.g., "eduroam"

// Signal strength (RSSI) -- only in STA mode
Serial.print("RSSI: ");
Serial.print(WiFi.RSSI());  // e.g., -67
Serial.println(" dBm");

// AP mode info
Serial.print("AP IP: ");
Serial.println(WiFi.softAPIP());  // Usually 192.168.4.1

Serial.print("AP clients: ");
Serial.println(WiFi.softAPgetStationNum());

// WiFi channel
Serial.print("Channel: ");
Serial.println(WiFi.channel());

// Hostname (for mDNS)
Serial.print("Hostname: ");
Serial.println(WiFi.getHostname());

// BSSID (access point MAC)
Serial.print("BSSID: ");
Serial.println(WiFi.BSSIDstr());
```

### RSSI Signal Quality Reference

| RSSI (dBm) | Quality | Suitability |
|-------------|---------|-------------|
| -30 to -50 | Excellent | Full telemetry, video possible |
| -50 to -60 | Good | Reliable telemetry |
| -60 to -70 | Fair | Telemetry OK, some latency |
| -70 to -80 | Weak | Reduced throughput, possible drops |
| Below -80 | Poor | Connection unreliable |

---

## 7. Best Practices for Drone Applications

### 7.1 WiFi Reconnection Strategies

**Event-based reconnection** (recommended):

```cpp
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("WiFi lost. Reconnecting...");
    WiFi.begin(ssid, password);
}

void setup() {
    WiFi.onEvent(WiFiStationDisconnected,
                 ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}
```

**Timer-based reconnection** (backup approach):

```cpp
void checkWiFi() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 30000) return;  // Check every 30s
    lastCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi down, reconnecting...");
        WiFi.disconnect();
        WiFi.reconnect();
    }
}
```

**WiFiMulti for multiple networks**:

```cpp
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

void setup() {
    wifiMulti.addAP("LabNetwork", "labpass");
    wifiMulti.addAP("eduroam", "");  // Handled separately
    wifiMulti.addAP("HomeNetwork", "homepass");

    // Connects to strongest available network
    if (wifiMulti.run() == WL_CONNECTED) {
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
    }
}
```

### 7.2 Connection Timeout Handling

Never let WiFi block the flight controller. WiFi operations should be non-blocking or run on a separate core:

```cpp
// WRONG -- blocks flight control loop:
while (WiFi.status() != WL_CONNECTED) {
    delay(500);  // Flight controller is frozen!
}

// RIGHT -- non-blocking, runs on Core 1:
void wifiTask(void* param) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("FLOPPI-DRONE", "floppifc123");

    // STA connection is non-blocking on its own task
    WiFi.begin(ssid, password);

    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            // Reconnect logic here -- does not affect flight loop
        }
        broadcastTelemetry();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    xTaskCreatePinnedToCore(wifiTask, "wifi", 8192, NULL, 1, NULL, 1);
    // Core 0: flight control loop runs in loop() or its own task
}
```

### 7.3 Power Consumption Considerations

| Mode | Current Draw | Notes |
|------|-------------|-------|
| WiFi Active TX | 160-260 mA | Transmitting data |
| WiFi Active RX | 95-100 mA | Receiving data |
| WiFi Modem Sleep | 20-25 mA @ 80MHz | Connected but idle |
| WiFi Off | 20 mA | CPU only, no radio |
| Light Sleep | 0.8 mA | CPU paused, WiFi maintains connection |
| Deep Sleep | 10 uA | Everything off, wake on timer/GPIO |

**For the flight controller**:
- During flight, power consumption from WiFi is negligible compared to motors (10-30A per motor)
- Modem sleep is useful for bench/calibration scenarios on battery
- Consider disabling WiFi entirely in a "flight-only" mode if not needed
- In AP mode, power draw depends on number of connected clients and data throughput

```cpp
// Reduce WiFi TX power to save power and reduce interference
WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Default is ~20dBm

// Modem sleep (enabled by default in STA mode)
// The WiFi radio sleeps between DTIM beacons
// Explicit control:
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Minimum modem sleep
esp_wifi_set_ps(WIFI_PS_MAX_MODEM);  // Maximum modem sleep (more savings)
esp_wifi_set_ps(WIFI_PS_NONE);       // No power save (lowest latency)
```

### 7.4 Range and Antenna Considerations

**Stock ESP32 range**:
- Indoor: 30-50 meters
- Outdoor (line of sight): 80-100 meters
- This is adequate for bench/lab telemetry and short-range drone control

**Improving range**:

1. **External antenna**: ESP32 modules with a U.FL/IPEX connector (e.g., ESP32-WROVER) can use external antennas. A 5dBi omnidirectional antenna can reach 200-500m outdoors. Requires moving a 0-ohm resistor from the PCB antenna pad to the IPEX pad.

2. **TX power**: Maximum is +20dBm (100mW). Default is usually fine.
   ```cpp
   WiFi.setTxPower(WIFI_POWER_20dBm);  // Maximum range
   ```

3. **Channel selection**: In AP mode, choose a less congested channel:
   ```cpp
   WiFi.softAP(ssid, password, 6);  // Channel 6
   ```

4. **ESP-NOW for long range**: For telemetry-only (no web server), ESP-NOW protocol can reach 1km+ with external antennas. This is what DroneBridge for ESP32 uses.

5. **Antenna placement on the drone**: Keep the antenna away from motors, ESCs, and power wires. Mount on top of the frame, oriented vertically. Carbon fiber frames block WiFi signal -- consider antenna placement carefully.

**For our use case**: Stock ESP32 range is sufficient for bench testing and calibration. For field telemetry during flight, an external antenna on a U.FL-equipped module is recommended. Long-range control should use the RC radio (SBUS), not WiFi.

---

## 8. Recommended Architecture for FLOPPI Flight Controller

Based on this research, here is the recommended WiFi architecture:

### Core 0 (Flight Control -- Priority)
- IMU reading and filtering
- PID control loop
- Motor output
- Radio (SBUS) input
- **NO WiFi operations**

### Core 1 (Communications -- Best Effort)
- WiFi management (AP mode default, optional STA)
- AsyncWebServer (status page)
- WebSocket telemetry (10-50 Hz)
- mDNS responder
- Captive portal (if in setup mode)
- OTA update handler (future)

### WiFi Modes by Firmware State

| Firmware State | WiFi Mode | Behavior |
|---------------|-----------|----------|
| Live (default) | AP | Drone creates "FLOPPI-DRONE" network. Status page at 192.168.4.1 or drone.local |
| Calibration | AP | Same AP, but status page shows calibration UI |
| Lab/Dev | AP+STA | AP for direct access + STA for network |
| First boot (no config) | AP (captive portal) | Setup wizard for WiFi configuration |

### Implementation Priority

1. **Phase 1**: AP mode + simple status page (current roadmap item)
2. **Phase 2**: WebSocket telemetry for fc_tool integration
3. **Phase 3**: mDNS discovery + captive portal for WiFi config
4. **Phase 4**: WPA2-Enterprise support for university environments
5. **Phase 5**: OTA updates

---

## 9. PlatformIO Library Dependencies

For the ESP32 WiFi features described in this document:

```ini
; platformio.ini (ESP32 environments only)
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    ESP Async WebServer      ; Async HTTP + WebSocket server
    AsyncTCP                 ; Required by ESPAsyncWebServer
    ; WiFiManager            ; Optional: only if using captive portal library
    ; ArduinoJson            ; Optional: cleaner JSON serialization
; ESPmDNS is built-in, no need to add
; Preferences is built-in, no need to add
```

---

## Sources

- [ESP32 WiFi Library Functions (Arduino IDE)](https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/) -- RandomNerdTutorials
- [WiFi API -- Arduino-ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html) -- Espressif official
- [WiFiClientEnterprise.ino (Official Example)](https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientEnterprise/WiFiClientEnterprise.ino) -- Espressif GitHub
- [ESP32-eduroam Examples](https://github.com/martinius96/ESP32-eduroam) -- Community examples for university networks
- [ESP32 WPA2-Enterprise Simplified](https://github.com/JeroenBeemster/ESP32-WPA2-enterprise) -- JeroenBeemster
- [WPA2 Enterprise Issues](https://github.com/espressif/arduino-esp32/issues/4229) -- Arduino-ESP32 GitHub
- [Connect ESP32 to WPA/WPA2 Enterprise](https://www.elektormagazine.com/labs/how-connect-esp32-to-wpawpa2-enterprise-network) -- Elektor Magazine
- [WiFiManager Library](https://github.com/tzapu/WiFiManager) -- tzapu
- [ESP32 WiFi Provisioning with Captive Portal](https://www.haraldkreuzer.net/en/news/esp32-wifi-provisioning-soft-ap-and-captive-portal) -- Harald Kreuzer
- [Integrating WiFi Setup with Captive Portal](https://luegm.dev/posts/captiveportal/) -- luegm.dev
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) -- me-no-dev
- [WebServer vs ESPAsyncWebServer](https://forum.arduino.cc/t/webserver-vs-espasyncwebserver/928293) -- Arduino Forum
- [ESP32 WebSocket Server: Sensor Readings](https://randomnerdtutorials.com/esp32-websocket-server-sensor/) -- RandomNerdTutorials
- [ESP32 WebSocket Real-Time Communication](https://www.videosdk.live/developer-hub/websocket/esp32-websocket) -- VideoSDK
- [ESP32 mDNS Setup](https://randomnerdtutorials.com/esp32-mdns-arduino/) -- RandomNerdTutorials
- [mDNS on ESP32: Local Network Discovery](https://medium.com/engineering-iot/understanding-mdns-on-esp32-local-network-device-discovery-made-easy-9aab590f0eea) -- Engineering IoT
- [ESP32 Static IP Address](https://randomnerdtutorials.com/esp32-static-fixed-ip-address-arduino-ide/) -- RandomNerdTutorials
- [ESP32 Save Data with Preferences Library](https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/) -- RandomNerdTutorials
- [ESP32 NVS Non-Volatile Storage](https://dronebotworkshop.com/esp32-non-volatile-storage/) -- DroneBot Workshop
- [ESP32 Reconnect to WiFi After Lost Connection](https://randomnerdtutorials.com/solved-reconnect-esp32-to-wifi/) -- RandomNerdTutorials
- [ESP32 WiFiMulti: Connect to Strongest Network](https://randomnerdtutorials.com/esp32-wifimulti/) -- RandomNerdTutorials
- [DroneBridge for ESP32](https://dronebridge.gitbook.io/docs/dronebridge-for-esp32/configuration) -- DroneBridge Docs
- [WiFi Drone ESP32 Project](https://github.com/MichalSchwarz/wifi-drone-esp32) -- MichalSchwarz
- [ESP-IDF WiFi Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html) -- Espressif
- [ESP32 Sleep Modes and Power Consumption](https://lastminuteengineers.com/esp32-sleep-modes-power-consumption/) -- Last Minute Engineers
- [ESP32 Low Power WiFi Mode](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/low-power-mode-wifi.html) -- Espressif
- [ESP32 Power Saving: WiFi and CPU](https://mischianti.org/esp32-practical-power-saving-manage-wifi-and-cpu-1/) -- Mischianti
- [ESP32 WiFi Range: 10km with External Antenna](https://hackaday.com/2017/04/11/esp32-wifi-hits-10km-with-a-little-help/) -- Hackaday
- [ESP32 Range Extender / Antenna Modification](https://peterneufeld.wordpress.com/2021/10/14/esp32-range-extender-antenna-modification/) -- Peter Neufeld
- [ESP32-WROVER-B with IPEX Antenna](https://www.dfrobot.com/product-1945.html) -- DFRobot
- [WiFi Security -- ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi-security.html) -- Espressif
