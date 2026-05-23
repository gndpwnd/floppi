# Compile-Time WiFi Network/Auth-Mode Selector — Design Plan

> **Status**: DESIGN ONLY (architect, 2026-05-22). No source touched, no commits.
> **Scope**: ESP32 / ESP32-S3 firmware (`flight_controller/`). Teensy builds are unaffected.
> **Philosophy**: compile-time everything via `#ifdef`; users edit only `config.h`,
> `wifi_credentials.h`, `wifi_certs.h`; STA mode only; one auth mode per build so unused
> modes cost zero flash/RAM; modular enough to lift into future ESP32 projects.

---

## 1. Current State (what exists today)

WiFi already works on ESP32 and supports OPEN / WPA2-PSK / WPA2-Enterprise(PEAP, TLS). The
goal of this plan is **not** to add connectivity from scratch — it is to *formalize* the
ad-hoc flag scheme into a single clean selector, add WPA3 + EAP-TTLS, add `#error`
validation, add static-IP/hostname, and extract the per-mode logic into a liftable module.

### Where WiFi connects today
- **Entry point**: `src/wifi_manager.cpp` → `setupWiFi()` (declared in `include/wifi_config.h`).
  - Called once from `src/main.cpp:365` (Core 1 setup). Reconnect loop `handleWiFi()` at `main.cpp:470`.
  - Runs entirely on Core 1; Core 0 flight loop never touches WiFi (dual-core decision).
- **The connect logic** (`wifi_manager.cpp:65-97`): `WiFi.mode(WIFI_STA)` →
  `WiFi.setAutoReconnect(true)` → branch on `WIFI_USE_ENTERPRISE`:
  - Enterprise: `WiFi.begin(SSID, WIFI_EAP_AUTH_METHOD, identity, username, password [, ca, crt, key])`.
  - Personal: `WiFi.begin(SSID, PASSWORD)`, or `WiFi.begin(SSID)` when password is empty (open).
  - Then a blocking wait up to `WIFI_CONNECT_TIMEOUT` (15 s) with `delay(250)`.
- **Credentials** flow from `include/wifi_credentials.h` (tracked, placeholder values),
  pulled in via `#if __has_include("wifi_credentials.h")` with a `#warning` + dummy fallback.
- **Certs** flow from `include/wifi_certs.h` (tracked, placeholder PEM), included only when
  `WIFI_USE_ENTERPRISE && WIFI_USE_CERTS`. Provides `WIFI_CA_CERT`, `WIFI_CLIENT_CERT`,
  `WIFI_CLIENT_KEY` as `static const char[]`.

### Existing flag scheme (to be superseded, kept backward-compatible)
- `WIFI_USE_ENTERPRISE` (presence flag), `WIFI_EAP_AUTH_METHOD` (`WPA2_AUTH_PEAP` / `WPA2_AUTH_TLS`),
  `WIFI_EAP_IDENTITY` / `_USERNAME` / `_PASSWORD`, `WIFI_USE_CERTS`. **No single selector**, no
  WPA3, no anonymous identity, no TTLS, no `#error` validation, no static IP / hostname.

### Flag cascade (`include/config.h:54-58`)
```c
#if defined(USE_ESP32) && defined(USE_WIFI)   // USE_WIFI set in platformio.ini esp32_base
    #define USE_WEB_SERVER
    #define USE_API_SERVER
    #define USE_OTA
#endif
```
`USE_API_AUTH` (config.h:73, default OFF) is a separate **command-surface token** gate
(`FLOPPI_CMD_TOKEN`) — orthogonal to link-layer auth. This design does NOT touch it.
`OTA_PASSWORD` enforcement (`ota.cpp:39` `#error`) is the established validation pattern to mirror.

### Toolchain facts (verified against the pinned core)
- `platformio.ini` pins `platform = espressif32` → installed **6.12.0** →
  `framework-arduinoespressif32` **3.20017** (Arduino-ESP32 **core 3.0.x**, IDF 5.1).
- Core 3.x `WiFiSTA.h` confirms the APIs the design relies on:
  - `wpa2_auth_method_t { WPA2_AUTH_TLS=0, WPA2_AUTH_PEAP=1, WPA2_AUTH_TTLS=2 }` — **TTLS now present.**
  - `begin(ssid, method, identity, username, password, ca_pem, client_crt, client_key, ...)`
    — the convenience enterprise overload **still exists** (no `esp_wpa2.h`/`esp_eap_client.h`
    calls needed). The current code keeps compiling.
  - `setMinSecurity(wifi_auth_mode_t)` — used to force **WPA3-SAE** (`WIFI_AUTH_WPA3_PSK`).
  - `config(local, gateway, subnet, dns1, dns2)` — static IP. `setHostname(const char*)` — hostname.
- **No `<esp_wpa2.h>` include is needed on core 3.x.** (On core 2.x you needed
  `esp_wpa2.h` + `esp_wifi_sta_wpa2_ent_*`; the convenience overload bridges both — see Risks.)
- `esp32` and `esp32s3` share the identical WiFi/STA API surface. No per-variant code needed.

---

## 2. Proposed Selector

A **single selector** macro in `config.h`, exactly one mode active per build:

```c
//=== WIFI AUTH MODE (ESP32/S3 only — pick exactly ONE) ===
#define WIFI_AUTH_PSK              // <- default, identical behavior to today
//#define WIFI_AUTH_OPEN
//#define WIFI_AUTH_WPA3_SAE
//#define WIFI_AUTH_ENTERPRISE
```

Rationale for **presence flags rather than `#define WIFI_AUTH_MODE x`**: matches the
project's existing idiom (IMU select, display select, RC-protocol select are all
"uncomment ONE `#define`"), and lets each mode's option block be `#ifdef`-gated trivially.
A `#error` block (Section 6) enforces exactly-one.

### Modes
| Mode | Macro | Underlying call | Notes |
|------|-------|-----------------|-------|
| Open | `WIFI_AUTH_OPEN` | `WiFi.begin(ssid)` | no password compiled in |
| WPA/WPA2-Personal | `WIFI_AUTH_PSK` | `WiFi.begin(ssid, pass)` | **CURRENT DEFAULT — byte-identical path** |
| WPA3-Personal | `WIFI_AUTH_WPA3_SAE` | `WiFi.setMinSecurity(WIFI_AUTH_WPA3_PSK); WiFi.begin(ssid, pass)` | SAE; falls back gracefully on mixed APs if `setMinSecurity` relaxed |
| Enterprise EAP | `WIFI_AUTH_ENTERPRISE` | `WiFi.begin(ssid, method, ident, [anon], user, pass [, ca, crt, key])` | sub-select PEAP / TTLS / TLS |

### Enterprise sub-selection (only compiled under `WIFI_AUTH_ENTERPRISE`)
```c
#define WIFI_EAP_METHOD_PEAP       // <- pick ONE: PEAP | TTLS | TLS
//#define WIFI_EAP_METHOD_TTLS
//#define WIFI_EAP_METHOD_TLS
```
Maps to `WPA2_AUTH_PEAP` / `WPA2_AUTH_TTLS` / `WPA2_AUTH_TLS`. PEAP/TTLS use
username+password (+ optional CA cert to validate the RADIUS server). TLS uses a client
cert+key (no password) and requires `USE_WIFI_CERTS`.

---

## 3. Certificate Support

A single optional flag, **off by default**:
```c
//#define USE_WIFI_CERTS    // include CA / client cert+key from wifi_certs.h
```
- When set, `wifi_manager` includes `wifi_certs.h` and passes the PEM blobs to the
  enterprise `begin()` overload. `WIFI_CA_CERT` validates the server (PEAP/TTLS/TLS);
  `WIFI_CLIENT_CERT` + `WIFI_CLIENT_KEY` provide mutual auth (TLS only).
- Empty client cert/key → pass `nullptr` (matches today's `strlen(...) > 0 ? ... : NULL`).
- **This is also the foundation for future TLS/HTTPS** (the same `WIFI_CA_CERT` blob can later
  be handed to `WiFiClientSecure::setCACert()` for HTTPS API POST / OTA-over-TLS). The plan
  reserves the flag name and file layout for that, but does **not** wire HTTPS now (deferred —
  `api_client.cpp` is plain `HTTPClient` today; keep it that way to stay bare-bones).
- Rename note: keep `WIFI_USE_CERTS` working as an **alias** for `USE_WIFI_CERTS` so existing
  user files don't break (Section 6 backward-compat).

---

## 4. Orthogonal Options (recommend INCLUDE the lean ones, DEFER the bloaty ones)

| Option | Recommendation | Why |
|--------|----------------|-----|
| **Hostname** (`WIFI_HOSTNAME`) | **INCLUDE** | tiny (one `setHostname()` call + a string already implied by mDNS `floppi-XXXX`); nice for routers/DHCP tables. Default to derive from MAC if undefined. |
| **Static IP** (`USE_STATIC_IP` + IP/gateway/subnet/DNS) | **INCLUDE** | one `WiFi.config()` call, ~0 flash when off; genuinely useful for fixed swarm addressing (avoids mDNS dependency). All four `IPAddress` values in `wifi_credentials.h`. |
| **Multi-SSID fallback list** | **DEFER / keep simple** | Adds a loop, an array of credential structs, and per-entry RAM. Conflicts with "one auth mode per build" (each SSID could need a different mode). Bare-bones violation. If ever needed, scope it to a *same-mode* SSID array behind its own flag in a later phase. **Recommend: not now.** |

---

## 5. Exact Schema

### 5a. `config.h` — selector + option blocks (new section, ESP32-gated)
```c
#if defined(USE_ESP32) && defined(USE_WIFI)
//=============================================================================
// WIFI AUTH MODE  (pick exactly ONE)
//=============================================================================
#define WIFI_AUTH_PSK            // WPA/WPA2-Personal (default)
//#define WIFI_AUTH_OPEN         // no password
//#define WIFI_AUTH_WPA3_SAE     // WPA3-Personal
//#define WIFI_AUTH_ENTERPRISE   // WPA2/WPA3-Enterprise (EAP)

// --- Enterprise EAP method (only used under WIFI_AUTH_ENTERPRISE; pick ONE) ---
#define WIFI_EAP_METHOD_PEAP     // username/password, optional CA
//#define WIFI_EAP_METHOD_TTLS   // username/password, optional CA
//#define WIFI_EAP_METHOD_TLS    // client cert+key (requires USE_WIFI_CERTS)

// --- Certificates (optional; CA for server validation, client cert+key for EAP-TLS) ---
//#define USE_WIFI_CERTS

// --- Network addressing (optional) ---
//#define USE_STATIC_IP          // static address instead of DHCP
//#define WIFI_HOSTNAME "floppi-fc"   // omit -> derived from MAC
#endif
```
Secrets/addresses are NOT in config.h — they go in `wifi_credentials.h` so config.h stays
secret-free (mirrors how `FLOPPI_CMD_TOKEN`/`OTA_PASSWORD` already live in credentials).

### 5b. `wifi_credentials.h` — secrets per mode (documented placeholders)
```c
#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

// SSID is required for every mode.
#define WIFI_SSID       "YourNetworkName"

// --- WIFI_AUTH_PSK / WIFI_AUTH_WPA3_SAE ---
#define WIFI_PASSWORD   "YourPassword"

// --- WIFI_AUTH_OPEN ---  (no secret needed)

// --- WIFI_AUTH_ENTERPRISE (PEAP/TTLS) ---
//#define WIFI_EAP_IDENTITY        "user@university.edu"  // outer/real identity
//#define WIFI_EAP_ANON_IDENTITY   "anonymous@university.edu" // optional outer id; omit to reuse identity
//#define WIFI_EAP_USERNAME        "user@university.edu"  // inner username
//#define WIFI_EAP_PASSWORD        "your_password"
// --- WIFI_AUTH_ENTERPRISE (TLS) uses identity + client cert/key from wifi_certs.h ---
//#define WIFI_EAP_IDENTITY        "device01@university.edu"

// --- USE_STATIC_IP (all four required when enabled) ---
//#define WIFI_STATIC_IP      192,168,1,50
//#define WIFI_STATIC_GATEWAY 192,168,1,1
//#define WIFI_STATIC_SUBNET  255,255,255,0
//#define WIFI_STATIC_DNS     192,168,1,1

// (existing FLOPPI_CMD_TOKEN, OTA_PASSWORD blocks remain unchanged)
#endif
```
Note: `WIFI_STATIC_IP 192,168,1,50` expands inside `IPAddress(WIFI_STATIC_IP)` — the comma
form is the established Arduino idiom and keeps it as a single user-edited line.

### 5c. `wifi_certs.h` — cert blobs (unchanged shape; gated by USE_WIFI_CERTS)
Already correct as-is: `WIFI_CA_CERT`, `WIFI_CLIENT_CERT`, `WIFI_CLIENT_KEY` as
`static const char[]` PEM. Keep header guard; keep empty-string default for client cert/key.
Add one comment line clarifying which fields each EAP method needs (PEAP/TTLS: CA only; TLS: all three).

---

## 6. Compile-Time Validation (`#error`)

Place in `wifi_config.h` (single include point) so it fires once. Conditions:

```c
// (A) exactly one auth mode
#define WIFI_MODE_COUNT (defined(WIFI_AUTH_OPEN)+defined(WIFI_AUTH_PSK)+ \
                         defined(WIFI_AUTH_WPA3_SAE)+defined(WIFI_AUTH_ENTERPRISE))
#if WIFI_MODE_COUNT == 0
  #error "No WiFi auth mode selected. Define exactly one WIFI_AUTH_* in config.h."
#elif WIFI_MODE_COUNT > 1
  #error "Multiple WiFi auth modes selected. Define exactly ONE WIFI_AUTH_* in config.h."
#endif

// (B) enterprise needs exactly one EAP method + an identity
#ifdef WIFI_AUTH_ENTERPRISE
  #if (defined(WIFI_EAP_METHOD_PEAP)+defined(WIFI_EAP_METHOD_TTLS)+defined(WIFI_EAP_METHOD_TLS)) != 1
    #error "WIFI_AUTH_ENTERPRISE requires exactly one WIFI_EAP_METHOD_* (PEAP/TTLS/TLS)."
  #endif
  #if !defined(WIFI_EAP_IDENTITY)
    #error "WIFI_AUTH_ENTERPRISE requires WIFI_EAP_IDENTITY in wifi_credentials.h."
  #endif
  // PEAP/TTLS require username+password
  #if (defined(WIFI_EAP_METHOD_PEAP)||defined(WIFI_EAP_METHOD_TTLS)) && \
      !(defined(WIFI_EAP_USERNAME) && defined(WIFI_EAP_PASSWORD))
    #error "PEAP/TTLS require WIFI_EAP_USERNAME and WIFI_EAP_PASSWORD."
  #endif
  // EAP-TLS requires certs
  #if defined(WIFI_EAP_METHOD_TLS) && !defined(USE_WIFI_CERTS)
    #error "EAP-TLS requires USE_WIFI_CERTS (client cert+key in wifi_certs.h)."
  #endif
#endif

// (C) certs flag requires the file/blobs (soft: rely on __has_include + a static_assert
//     on sizeof(WIFI_CA_CERT) > 1 in the .cpp, mirroring ota.cpp's OTA_PASSWORD assert)
#if defined(USE_WIFI_CERTS) && !__has_include("wifi_certs.h")
  #error "USE_WIFI_CERTS set but wifi_certs.h not found (create it from include/ template)."
#endif

// (D) static IP needs all four values
#if defined(USE_STATIC_IP) && !(defined(WIFI_STATIC_IP) && defined(WIFI_STATIC_GATEWAY) && \
    defined(WIFI_STATIC_SUBNET) && defined(WIFI_STATIC_DNS))
  #error "USE_STATIC_IP requires WIFI_STATIC_IP/GATEWAY/SUBNET/DNS in wifi_credentials.h."
#endif

// (E) backward-compat aliases (do BEFORE the checks above)
#ifdef WIFI_USE_ENTERPRISE
  #define WIFI_AUTH_ENTERPRISE
  #warning "WIFI_USE_ENTERPRISE is deprecated; use WIFI_AUTH_ENTERPRISE. Aliased for now."
#endif
#ifdef WIFI_USE_CERTS
  #define USE_WIFI_CERTS
#endif
// If no WIFI_AUTH_* and no enterprise alias, default to PSK for old wifi_credentials.h files:
#if WIFI_MODE_COUNT == 0 && !defined(WIFI_AUTH_ENTERPRISE)
  #define WIFI_AUTH_PSK   // legacy files that only set SSID+PASSWORD keep working
#endif
```
The legacy `WIFI_EAP_AUTH_METHOD WPA2_AUTH_PEAP/TLS` form should also be honored as an alias
(map to `WIFI_EAP_METHOD_PEAP`/`_TLS`) so no existing user file breaks.

---

## 7. Modular Connection Module

Extract the per-mode `#ifdef` logic out of `setupWiFi()` into a small, liftable unit.

**Recommendation: keep `wifi_manager.cpp/.h` as the public face** (it already owns
`setupWiFi`/`handleWiFi`/`populateNetworkData` and is wired into `main.cpp`), and add a
**private internal helper `wifiBeginForMode()`** that contains all the `#ifdef` branching.
Optionally split that helper into a self-contained `wifi_connect.h/.cpp` pair that depends on
nothing project-specific (no `DisplayData_t`) so it lifts cleanly into other ESP32 projects.

Proposed clean API (in `wifi_connect.h`, or as a static in wifi_manager):
```c
// Reads the compile-time WIFI_AUTH_* selection + credentials/certs and starts the
// connection. All mode #ifdefs are hidden here. Returns immediately (non-blocking begin).
void wifiConnectBegin();          // applies hostname + static IP + WiFi.begin(...)
const char* wifiAuthModeName();   // "OPEN"/"PSK"/"WPA3-SAE"/"EAP-PEAP"/... for logging
```
`setupWiFi()` then becomes: print MAC → check SSID configured → `WiFi.mode(STA)` →
`wifiConnectBegin()` → existing timeout wait + status print. `handleWiFi()` is unchanged.
This keeps the dual-core / Core-1 contract and the existing logging untouched.

The single mode dispatch inside `wifiConnectBegin()`:
```c
#if defined(WIFI_HOSTNAME)
  WiFi.setHostname(WIFI_HOSTNAME);
#endif
#if defined(USE_STATIC_IP)
  WiFi.config(IPAddress(WIFI_STATIC_IP), IPAddress(WIFI_STATIC_GATEWAY),
              IPAddress(WIFI_STATIC_SUBNET), IPAddress(WIFI_STATIC_DNS));
#endif
#if defined(WIFI_AUTH_OPEN)
  WiFi.begin(WIFI_SSID);
#elif defined(WIFI_AUTH_PSK)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);          // byte-identical to today
#elif defined(WIFI_AUTH_WPA3_SAE)
  WiFi.setMinSecurity(WIFI_AUTH_WPA3_PSK);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#elif defined(WIFI_AUTH_ENTERPRISE)
  // method = PEAP/TTLS/TLS; anon id optional; cert args only under USE_WIFI_CERTS
  WiFi.begin(WIFI_SSID, EAP_METHOD, WIFI_EAP_IDENTITY, EAP_USER, EAP_PASS,
             CA_ARG, CRT_ARG, KEY_ARG);          // unused args -> nullptr
#endif
```
where `EAP_METHOD`/`EAP_USER`/`EAP_PASS`/`CA_ARG`/`CRT_ARG`/`KEY_ARG` are resolved by
sub-`#ifdef`s (TLS leaves user/pass NULL; non-certs leaves CA/CRT/KEY NULL). Anonymous
identity: core 3.x's convenience overload takes a single identity arg, so if
`WIFI_EAP_ANON_IDENTITY` is defined pass it as the `wpa2_identity` (outer) and
`WIFI_EAP_USERNAME` as `wpa2_username` (inner) — which is exactly the existing arg order.

---

## 8. Per-Mode Memory Budget (estimates, ESP32 core 3.x)

The WiFi/lwIP/IDF stack (`libnet80211`, `libwpa_supplicant`, `libcoexist`, lwIP) is **already
linked** for every ESP32 build because `USE_WIFI` is always on for those envs and the web
server uses it. So the *incremental* deltas of the auth modes are small — the heavy code is
present regardless.

| Mode | Flash delta vs PSK baseline | RAM delta | Notes |
|------|------------------------------|-----------|-------|
| `WIFI_AUTH_OPEN` | ≈ −0 to −0.1 KB | ~0 | drops the password string only |
| `WIFI_AUTH_PSK` (baseline) | **0 (unchanged)** | 0 | identical call path to today; this is the regression guarantee |
| `WIFI_AUTH_WPA3_SAE` | ~ +1–3 KB | negligible | pulls SAE crypto from `libwpa_supplicant` (the lib is already linked; this references more of it). May be near-zero if WPA2-PSK already references SAE symbols. |
| `WIFI_AUTH_ENTERPRISE` (PEAP/TTLS, no certs) | ~ +20–40 KB | + a few KB heap during handshake | pulls in the WPA2-Enterprise / EAP supplicant (`esp_eap_client`) + TLS handshake code (mbedTLS) that PSK never touches. This is the one materially heavy mode. |
| `WIFI_AUTH_ENTERPRISE` + `USE_WIFI_CERTS` | + size of PEM blobs (CA ~1.2–2 KB, client cert ~1–2 KB, key ~1.7 KB) on top of enterprise | +0 (blobs are `const`, live in flash) | EAP-TLS adds mutual-auth path; certs are flash-resident `const char[]`. |

Concrete expectation: the **default PSK build is unchanged** in size. Only operators who
select Enterprise pay the ~20–40 KB EAP/mbedTLS cost — which is the entire point of the
compile-time selector. WPA3 and Open are effectively free. (Exact deltas should be confirmed
empirically by building `esp32` before/after with `pio run -e esp32 -v` and comparing the
firmware `.bin` size / linker map; the coding agent should record the real numbers.)

ESP32 vs ESP32-S3: identical API and identical deltas — both link the same IDF WiFi libs.

---

## 9. Implementation Checklist (ordered, for the coding agent)

> Design-only here. Files to touch, in dependency order. Each step independently compiles.

1. **`include/config.h`** — add the WiFi-auth-mode section from §5a inside the existing
   `#if defined(USE_ESP32) && defined(USE_WIFI)` block (place it near lines 54-58, after the
   sub-feature cascade). Default `WIFI_AUTH_PSK` uncommented. Keep all comments terse.
2. **`include/wifi_credentials.h`** — restructure secrets per §5b: keep `WIFI_SSID` +
   `WIFI_PASSWORD`, add commented enterprise (incl. `WIFI_EAP_ANON_IDENTITY`) + static-IP
   blocks. Leave `FLOPPI_CMD_TOKEN` / `OTA_PASSWORD` untouched. Update the header comment's
   "WiFi Types" list to mention WPA3 + TTLS + the new selector.
3. **`include/wifi_certs.h`** — only a comment tweak (which fields each EAP method needs).
   No structural change.
4. **`include/wifi_config.h`** — add the §6 validation block (aliases first, then `#error`s)
   inside the existing `#if defined(USE_ESP32) && defined(USE_WIFI)` guard. Optionally declare
   `wifiConnectBegin()` / `wifiAuthModeName()` if going the separate-module route.
5. **(optional) `include/wifi_connect.h` + `src/wifi_connect.cpp`** — the liftable module
   from §7. If keeping it inside wifi_manager instead, skip this and put `wifiBeginForMode()`
   as a `static` in `wifi_manager.cpp`. **Recommend the separate module** for the "lift into
   future projects" goal, but it is acceptable to keep it in-file for bare-bones.
6. **`src/wifi_manager.cpp`** — replace the inline enterprise/personal branch (lines 71-97)
   with a single `wifiConnectBegin()` call; add the §6 alias-aware logging via
   `wifiAuthModeName()`; add the `static_assert(sizeof(WIFI_CA_CERT) > 1, ...)` under
   `USE_WIFI_CERTS` (mirrors `ota.cpp`). Keep MAC print, SSID-configured check, timeout wait,
   and `handleWiFi()`/`populateNetworkData()` exactly as-is.
7. **`docs/features/wifi-configuration.md`** — document the new selector, all four modes,
   TTLS, anonymous identity, static IP, hostname, and the backward-compat note.
8. **Build verification**: `pio run -e esp32` and `pio run -e esp32s3` for each mode
   (PSK, OPEN, WPA3, ENTERPRISE-PEAP, ENTERPRISE-TLS+CERTS) — confirm clean compile + the
   `#error`s fire on bad combos. Record real flash deltas (§8) in the doc.

---

## 10. Risks & Notes

- **R1 — Core-version API drift (the main risk).** The convenience
  `WiFi.begin(ssid, method, identity, username, password, ca, crt, key)` overload exists on
  the pinned core 3.20017 (verified). On older 2.x cores you needed `<esp_wpa2.h>` +
  `esp_wifi_sta_wpa2_ent_enable()`. The design relies on the convenience overload, so it is
  correct **for the pinned core**. If the platform pin ever bumps, re-verify `WiFiSTA.h`.
  Recommend NOT calling `esp_eap_client_*` directly — stay on the Arduino overload for portability.
- **R2 — WPA3 on mixed networks.** `setMinSecurity(WIFI_AUTH_WPA3_PSK)` forces SAE and will
  *refuse* a WPA2-only AP. Document that `WIFI_AUTH_WPA3_SAE` is for genuinely WPA3 networks;
  for WPA2/WPA3-transition APs, plain `WIFI_AUTH_PSK` already negotiates the best available.
- **R3 — Cert size vs flash.** Enterprise (esp. with certs) is the only mode that meaningfully
  grows the image. Some ESP32 builds are already near flash limits with web+API+OTA; the coding
  agent must check the linker after enabling Enterprise and may need to drop a sub-feature.
- **R4 — Backward compatibility is mandatory** (operator preference: "don't affect existing
  code, just add"). The alias block (§6E) and the PSK default ensure a user with an old
  `wifi_credentials.h` (only SSID+PASSWORD) builds identically with zero edits.
- **R5 — Scope discipline.** Multi-SSID fallback and HTTPS/TLS-for-API are explicitly DEFERRED
  to keep this bare-bones. The cert flag + file layout are reserved for future HTTPS, but no
  HTTPS code is added now.
- **R6 — Orthogonal to `USE_API_AUTH`.** Link-layer auth (this design) and command-surface
  token auth (`FLOPPI_CMD_TOKEN`, SEC-01/03) are independent; nothing here touches that path
  or the swarm_api contract.
```
