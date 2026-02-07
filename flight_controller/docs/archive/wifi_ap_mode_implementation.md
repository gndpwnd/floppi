# WiFi AP Mode Implementation (Archived)

> Archived: 2026-02-07
> Reason: Architecture changed to STA mode (connect to existing WiFi) for swarm coordination.
> AP mode may be revisited as a future field-use fallback (see roadmap, last item).

## Context

The initial WiFi implementation used AP mode, where each ESP32 created its own WiFi access point. This was replaced with STA mode because:

1. **Swarm coordination**: Multiple drones need to be on the same network to communicate with centralized servers
2. **Simpler infrastructure**: Drones connect to existing WiFi (lab, university, field router) rather than each creating an access point
3. **API client pattern**: Drones make POST/GET requests to centralized computers, not host individual web servers

## Archived Code

### wifi_manager.cpp (AP mode version)

```cpp
/*
 * WiFi Manager Implementation - AP MODE (ARCHIVED)
 * ESP32 WiFi AP mode and network info.
 */

#if defined(USE_ESP32) && defined(USE_WIFI)

#include "wifi_config.h"
#include <WiFi.h>

static char ap_ssid[33];
static bool wifi_initialized = false;

#ifndef WIFI_AP_PASSWORD
    #define WIFI_AP_PASSWORD ""
#endif

void setupWiFi() {
    // Generate unique SSID from MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(ap_ssid, sizeof(ap_ssid), "Floppi-%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    WiFi.mode(WIFI_AP);

    if (strlen(WIFI_AP_PASSWORD) > 0) {
        WiFi.softAP(ap_ssid, WIFI_AP_PASSWORD);
    } else {
        WiFi.softAP(ap_ssid);
    }

    Serial.print(F("[WiFi] AP started: "));
    Serial.println(ap_ssid);
    Serial.print(F("[WiFi] IP: "));
    Serial.println(WiFi.softAPIP());
    Serial.print(F("[WiFi] MAC: "));
    Serial.println(WiFi.macAddress());

    wifi_initialized = true;
}

void populateNetworkData(DisplayData_t* data) {
    if (!wifi_initialized) {
        data->wifi_connected = false;
        data->ssid[0] = '\0';
        data->ip_address[0] = '\0';
        data->mac_address[0] = '\0';
        data->wifi_rssi = 0;
        return;
    }

    data->wifi_connected = true;
    strncpy(data->ssid, ap_ssid, sizeof(data->ssid) - 1);
    data->ssid[sizeof(data->ssid) - 1] = '\0';

    IPAddress ip = WiFi.softAPIP();
    snprintf(data->ip_address, sizeof(data->ip_address),
             "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    strncpy(data->mac_address, WiFi.macAddress().c_str(),
            sizeof(data->mac_address) - 1);
    data->mac_address[sizeof(data->mac_address) - 1] = '\0';
    data->wifi_rssi = 0;
}

void handleWiFi() {
    // AP mode runs autonomously after setup.
}

#endif
```

### platformio.ini flags used

```ini
-D USE_WIFI
```

## Restoration

To restore AP mode in the future, copy the code above into `src/wifi_manager.cpp` and modify `setupWiFi()` to use `WIFI_AP` mode instead of `WIFI_STA`. The display screens and `populateNetworkData()` would need to use `WiFi.softAPIP()` instead of `WiFi.localIP()`.
