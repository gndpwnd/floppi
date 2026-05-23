# WiFi Configuration Guide

> ESP32 / ESP32-S3 builds only. Teensy builds do not use WiFi.

## Overview

WiFi auth is selected at **compile time** with a single selector in `config.h`,
exactly like the IMU / RC-protocol / display selectors: you uncomment **one**
`WIFI_AUTH_MODE_*` define. Only the chosen mode's code is compiled — unused modes
cost zero flash/RAM. STA mode only (the drone joins an existing network).

Three files are involved:

| File | What goes here | Edited by |
|------|----------------|-----------|
| `include/config.h` | The auth **mode selector** + optional flags (certs, static IP, hostname) | user |
| `include/wifi_credentials.h` | **Secrets / addresses** (SSID, password, EAP identity/user/pass, static IP) | user |
| `include/wifi_certs.h` | **PEM blobs** (CA cert, client cert+key) — only when `USE_WIFI_CERTS` is on | user |
| `include/wifi_config.h` | Compile-time `#error` validation + module header | — |
| `include/wifi_connect.h` / `src/wifi_connect.cpp` | Liftable per-mode connect logic | — |
| `src/wifi_manager.cpp` | Connection lifecycle (setup / reconnect / telemetry) | — |

`config.h` stays **secret-free** — passwords/identities/IPs live only in
`wifi_credentials.h` (mirrors how `FLOPPI_CMD_TOKEN` and `OTA_PASSWORD` are handled).

> **Naming note:** the selector tokens are `WIFI_AUTH_MODE_*` (not bare
> `WIFI_AUTH_OPEN` / `WIFI_AUTH_ENTERPRISE`). The ESP-IDF defines an enum
> `wifi_auth_mode_t` whose members are *named* `WIFI_AUTH_OPEN`,
> `WIFI_AUTH_ENTERPRISE`, `WIFI_AUTH_WPA3_PSK`, … — defining those as
> preprocessor macros corrupts `<esp_wifi_types.h>`. The `WIFI_AUTH_MODE_` prefix
> is collision-free.

## Selecting a mode (`config.h`)

Inside the `#if defined(USE_ESP32) && defined(USE_WIFI)` block:

```c
#define WIFI_AUTH_MODE_PSK            // WPA/WPA2-Personal (default)
//#define WIFI_AUTH_MODE_OPEN         // no password
//#define WIFI_AUTH_MODE_WPA3_SAE     // WPA3-Personal
//#define WIFI_AUTH_MODE_ENTERPRISE   // WPA2/WPA3-Enterprise (EAP)
```

| Mode | Underlying call | Secrets needed |
|------|-----------------|----------------|
| `WIFI_AUTH_MODE_OPEN` | `WiFi.begin(ssid)` | SSID only |
| `WIFI_AUTH_MODE_PSK` (default) | `WiFi.begin(ssid, pass)` | SSID + password |
| `WIFI_AUTH_MODE_WPA3_SAE` | `setMinSecurity(WIFI_AUTH_WPA3_PSK)` + `WiFi.begin(ssid, pass)` | SSID + password |
| `WIFI_AUTH_MODE_ENTERPRISE` | enterprise `WiFi.begin(...)` overload | EAP block (see below) |

## Per-mode setup

### Open (no password)

```c
// config.h
#define WIFI_AUTH_MODE_OPEN
// wifi_credentials.h
#define WIFI_SSID "OpenNetwork"
```

### WPA2-Personal (home / lab) — DEFAULT

```c
// config.h
#define WIFI_AUTH_MODE_PSK
// wifi_credentials.h
#define WIFI_SSID     "MyNetwork"
#define WIFI_PASSWORD "MyPassword123"
```

This path is **byte-identical to the legacy behavior** — existing builds are
unaffected.

### WPA3-Personal (SAE)

```c
// config.h
#define WIFI_AUTH_MODE_WPA3_SAE
// wifi_credentials.h
#define WIFI_SSID     "MyWPA3Network"
#define WIFI_PASSWORD "MyPassword123"
```

`setMinSecurity(WIFI_AUTH_WPA3_PSK)` **forces** SAE and will refuse a WPA2-only
AP. For WPA2/WPA3-transition networks, use `WIFI_AUTH_MODE_PSK` instead — it
already negotiates the strongest mutually-supported security.

### WPA2/WPA3-Enterprise (EAP)

Pick exactly one EAP sub-method in `config.h`:

```c
#define WIFI_AUTH_MODE_ENTERPRISE
#define WIFI_EAP_METHOD_PEAP     // username/password, optional CA
//#define WIFI_EAP_METHOD_TTLS   // username/password, optional CA
//#define WIFI_EAP_METHOD_TLS    // client cert+key (requires USE_WIFI_CERTS)
```

#### PEAP / TTLS (username + password)

The common eduroam / corporate case:

```c
// wifi_credentials.h
#define WIFI_SSID         "eduroam"
#define WIFI_EAP_IDENTITY "user@university.edu"   // required
#define WIFI_EAP_USERNAME "user@university.edu"   // required for PEAP/TTLS
#define WIFI_EAP_PASSWORD "your_password"         // required for PEAP/TTLS
```

**Anonymous (outer) identity** — optional, for privacy on the unencrypted
outer tunnel. When set, the anon string is the outer identity and
`WIFI_EAP_USERNAME` is the inner identity:

```c
#define WIFI_EAP_ANON_IDENTITY "anonymous@university.edu"
```

**Verify the RADIUS server** (recommended) by adding a CA cert:

```c
// config.h
#define USE_WIFI_CERTS
// then put the CA PEM in wifi_certs.h (WIFI_CA_CERT)
```

#### EAP-TLS (certificate-only, no password)

Mutual-auth networks. Requires `USE_WIFI_CERTS` (enforced by `#error`):

```c
// config.h
#define WIFI_AUTH_MODE_ENTERPRISE
#define WIFI_EAP_METHOD_TLS
#define USE_WIFI_CERTS
// wifi_credentials.h
#define WIFI_SSID         "secure-corp"
#define WIFI_EAP_IDENTITY "device01@corp.com"     // required (no username/password)
```

Put CA cert + client cert + client key PEM in `wifi_certs.h`.

## Certificates (`USE_WIFI_CERTS`)

Default **off**. When on, `wifi_certs.h` is compiled in and the PEM blobs are
passed to the enterprise `begin()` overload:

| Field | PEAP | TTLS | TLS |
|-------|:----:|:----:|:---:|
| `WIFI_CA_CERT` (validate server) | optional | optional | required |
| `WIFI_CLIENT_CERT` (mutual auth) | — | — | required |
| `WIFI_CLIENT_KEY` (mutual auth) | — | — | required |

Empty client cert/key are passed as `NULL`. The blobs are `static const char[]`
and live in flash (zero RAM cost). A `static_assert(sizeof(WIFI_CA_CERT) > 1)` in
`wifi_connect.cpp` fails the build if the CA blob is left as the empty template
(mirrors `ota.cpp`'s `OTA_PASSWORD` check).

> **Future TLS reuse:** the `USE_WIFI_CERTS` flag and `wifi_certs.h` layout are
> also the reserved foundation for HTTPS / OTA-over-TLS — the same `WIFI_CA_CERT`
> can later be handed to `WiFiClientSecure::setCACert()`. No HTTPS code is wired
> today (the API client stays plain `HTTPClient` to remain bare-bones).

## Network addressing (orthogonal options)

Both default off; both compile to ~0 bytes when unused. Independent of the auth
mode.

### Hostname

```c
// config.h
#define WIFI_HOSTNAME "floppi-fc"
```

One `setHostname()` call; shows up in router DHCP tables. Omit to let the device
derive a name from its MAC (the `floppi-XXXX` mDNS pattern).

> **Ordering caveat:** on Arduino-ESP32 core 3.x the STA hostname is pushed to
> the netif *inside* `WiFi.mode(WIFI_STA)`, so it must be set **before** the mode
> call or it won't appear in DHCP on the first association. The module handles
> this via `wifiApplyHostname()`, which `setupWiFi()` calls just before
> `WiFi.mode(WIFI_STA)` — do not move `WIFI_HOSTNAME` setup after the mode call.

### Static IP (instead of DHCP)

```c
// config.h
#define USE_STATIC_IP
// wifi_credentials.h — ALL FOUR required (comma form expands into IPAddress())
#define WIFI_STATIC_IP      192,168,1,50
#define WIFI_STATIC_GATEWAY 192,168,1,1
#define WIFI_STATIC_SUBNET  255,255,255,0
#define WIFI_STATIC_DNS     192,168,1,1
```

Useful for fixed swarm addressing without depending on mDNS.

## Memory note: Enterprise is the heavy mode (in theory)

Conceptually, Enterprise EAP pulls in the WPA2-Enterprise supplicant + TLS
handshake (mbedTLS) that PSK never touches — the plan budgeted ~20–40 KB extra.
**In practice, on this firmware the delta is ~0**: the WiFi/lwIP/supplicant/
mbedTLS stack is *already* fully linked for every ESP32 build because `USE_WIFI`
(plus the always-on web server / API client / OTA) references it. Measured
firmware section sizes are identical across OPEN / PSK / WPA3-SAE / Enterprise-
PEAP / Enterprise-TLS (the binaries differ byte-for-byte but round to the same
section totals). WPA3-SAE and Open are effectively free. If you ever strip the
web/API/OTA features down, expect Enterprise to become the materially heavier
mode again, and check the linker after enabling it (some builds run near the
flash limit).

| Mode (esp32) | Flash | RAM | vs PSK |
|--------------|------:|----:|:------:|
| PSK (default) | 572273 | 35644 | baseline |
| OPEN | 572273 | 35644 | 0 |
| WPA3-SAE | 572273 | 35644 | 0 |
| Enterprise PEAP | 572273 | 35644 | 0 |
| Enterprise TLS + certs | 572273 | 35644 | 0 |

(ESP32-S3 mirrors this: PSK and Enterprise both 568033 flash / 31644 RAM.)

## Compile-time validation (`#error`)

`wifi_config.h` enforces, at build time, in this order:

1. **Exactly one** `WIFI_AUTH_MODE_*` (zero → error, more than one → error).
2. Enterprise requires **exactly one** `WIFI_EAP_METHOD_*` and a `WIFI_EAP_IDENTITY`.
3. PEAP/TTLS require `WIFI_EAP_USERNAME` + `WIFI_EAP_PASSWORD`.
4. EAP-TLS requires `USE_WIFI_CERTS`.
5. `USE_WIFI_CERTS` requires `wifi_certs.h` to exist (and a non-empty `WIFI_CA_CERT`, via `static_assert`).
6. `USE_STATIC_IP` requires all four `WIFI_STATIC_*` values.

## Backward compatibility

Old `wifi_credentials.h` files keep building with **zero edits**:

- A file that only sets `WIFI_SSID` + `WIFI_PASSWORD` (no `WIFI_AUTH_MODE_*`)
  defaults to PSK.
- `WIFI_USE_ENTERPRISE` is aliased to `WIFI_AUTH_MODE_ENTERPRISE` (with a
  deprecation `#warning`).
- `WIFI_USE_CERTS` is aliased to `USE_WIFI_CERTS`.
- The legacy `WIFI_EAP_AUTH_METHOD` form maps to `WIFI_EAP_METHOD_PEAP` (with a
  deprecation `#warning`). Users needing TLS/TTLS should adopt the new
  `WIFI_EAP_METHOD_*` selector — the preprocessor cannot distinguish the runtime
  `WPA2_AUTH_*` enum values.

## Architecture

- WiFi runs entirely on **Core 1** (never blocks flight control on Core 0).
- **STA mode** only — the drone connects to existing infrastructure.
- Auto-reconnect with 5-second retry; 15-second initial connect timeout
  (non-blocking after that).
- `setupWiFi()` (in `wifi_manager.cpp`) prints the MAC + auth-mode name, then
  calls `wifiConnectBegin()` (in `wifi_connect.cpp`), which hides all per-mode
  `#ifdef` logic. `handleWiFi()` and `populateNetworkData()` are unchanged.
- `wifi_connect.h`/`.cpp` depend on nothing project-specific, so they can be
  lifted into other Arduino-ESP32 (core 3.0.x) projects.

## Network Diagnostics

In calibration mode, press `n` to run network diagnostics (credentials
configured, connected/RSSI, MAC/IP/gateway/DNS, mDNS, web-server port, free heap).

## Toolchain

Verified against the pinned `platform = espressif32` 6.12.0 →
`framework-arduinoespressif32` 3.20017 (Arduino-ESP32 **core 3.0.x**, IDF 5.1).
Uses only the convenience APIs (`WiFi.begin(...)` enterprise overload,
`setMinSecurity()`, `config()`, `setHostname()`) — no raw `<esp_wpa2.h>` /
`esp_eap_client_*`. If the platform pin is ever bumped, re-verify `WiFiSTA.h`.
