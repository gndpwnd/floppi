# WiFi Configuration Guide

> ESP32 builds only. Teensy builds do not use WiFi.

## Quick Start

Edit `include/wifi_credentials.h` with your network details.

### WPA2-Personal (Home/Lab)

Most common setup. Set SSID and password:

```cpp
#define WIFI_SSID       "MyNetwork"
#define WIFI_PASSWORD   "MyPassword123"
```

### Open Network (No Password)

Set SSID, leave password empty:

```cpp
#define WIFI_SSID       "OpenNetwork"
#define WIFI_PASSWORD   ""
```

### WPA2-Enterprise PEAP (Eduroam/University)

Most university WiFi uses PEAP authentication (username + password, no certificates):

```cpp
#define WIFI_SSID              "eduroam"
#define WIFI_PASSWORD          ""

#define WIFI_USE_ENTERPRISE
#define WIFI_EAP_AUTH_METHOD   WPA2_AUTH_PEAP
#define WIFI_EAP_IDENTITY      "user@university.edu"
#define WIFI_EAP_USERNAME      "user@university.edu"
#define WIFI_EAP_PASSWORD      "your_password"
```

### WPA2-Enterprise PEAP with CA Certificate

Same as above but verifies the server's certificate (more secure):

```cpp
#define WIFI_USE_ENTERPRISE
#define WIFI_EAP_AUTH_METHOD   WPA2_AUTH_PEAP
#define WIFI_EAP_IDENTITY      "user@university.edu"
#define WIFI_EAP_USERNAME      "user@university.edu"
#define WIFI_EAP_PASSWORD      "your_password"
#define WIFI_USE_CERTS
```

Then edit `include/wifi_certs.h` with your CA certificate PEM.

### WPA2-Enterprise TLS (Certificate-Only)

For networks requiring mutual certificate authentication (no password):

```cpp
#define WIFI_USE_ENTERPRISE
#define WIFI_EAP_AUTH_METHOD   WPA2_AUTH_TLS
#define WIFI_EAP_IDENTITY      "device@corp.com"
#define WIFI_EAP_USERNAME      ""
#define WIFI_EAP_PASSWORD      ""
#define WIFI_USE_CERTS
```

Then edit `include/wifi_certs.h` with CA cert, client cert, and client key.

## Files

| File | Purpose |
|------|---------|
| `include/wifi_credentials.h` | SSID, password, enterprise settings (user edits this) |
| `include/wifi_certs.h` | PEM certificates for enterprise auth (optional, user edits) |
| `include/wifi_config.h` | Module header (function declarations) |
| `src/wifi_manager.cpp` | WiFi connection logic (don't edit unless developing) |

## WiFi Types Supported

| Type | Auth | Certificates | Define |
|------|------|-------------|--------|
| WPA2-Personal | Password | No | _(default)_ |
| Open | None | No | _(empty password)_ |
| WPA2-Enterprise PEAP | Username + Password | Optional CA | `WIFI_USE_ENTERPRISE` |
| WPA2-Enterprise TLS | Certificates | CA + Client | `WIFI_USE_ENTERPRISE` + `WIFI_USE_CERTS` |

## Architecture

- WiFi runs entirely on **Core 1** (never blocks flight control on Core 0)
- **STA mode** only — drone connects to existing network infrastructure
- Auto-reconnect with 5-second retry interval
- 15-second connection timeout on startup (non-blocking after that)
- `handleWiFi()` called periodically from Core 1 loop

## Network Diagnostics

In calibration mode, press `n` to run network diagnostics:
- WiFi credentials configured?
- WiFi connected? Signal strength (RSSI)?
- MAC address, IP, gateway, DNS
- mDNS hostname resolution
- Web server port availability
- Free heap memory

## Compile-Time Guards

WiFi code is fully gated — zero overhead on non-WiFi builds:

```cpp
#if defined(USE_ESP32) && defined(USE_WIFI)
// WiFi code only compiled here
#endif
```

Enterprise support only adds code when `WIFI_USE_ENTERPRISE` is defined.
Certificate support only adds code when `WIFI_USE_CERTS` is defined.
