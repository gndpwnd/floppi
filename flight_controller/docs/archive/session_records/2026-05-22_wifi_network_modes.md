# Session Record — 2026-05-22 — ESP32 Compile-Time WiFi Auth-Mode Selector

> Project: `flight_controller/` (ESP32 / ESP32-S3 firmware; Teensy unaffected)
> Agent (this record): `doc-writer@flight_controller:save-progress` (Claude Code Orchestra)
> Status: **All changes uncommitted — working tree only, awaiting operator review.**
> Companion to: [2026-05-22_security_correctness_docs.md](2026-05-22_security_correctness_docs.md)
> (this WiFi feature landed *after* that record was written). Every code/flag/path
> claim below was re-verified against the actual source on 2026-05-22.

---

## 1. One-paragraph summary

A compile-time **WiFi network/auth-mode selector** was designed, implemented, and QA'd for
the ESP32/S3 builds. WiFi already worked (OPEN / WPA2-PSK / WPA2-Enterprise-PEAP/TLS); this
wave **formalized** the ad-hoc flag scheme into a single clean selector, added **WPA3-SAE**
and **EAP-TTLS**, added `#error` compile-time validation, added **static IP + hostname**
options, and extracted the per-mode connect logic into a **liftable `wifi_connect` module**
that depends on nothing project-specific. Default behavior is byte-identical to the legacy
PSK path (zero regression). An independent QA review built every mode + every guard and
returned **GO**; the one bug it found (a `WIFI_HOSTNAME` ordering issue) was subsequently
fixed in code. **No git commits were made.**

---

## 2. What was done (verified against source)

### 2a. The selector (`include/config.h`, ESP32-gated)
A single "uncomment exactly ONE" selector mirroring the IMU / RC-protocol / display idiom,
inside the existing `#if defined(USE_ESP32) && defined(USE_WIFI)` block:

```c
#define WIFI_AUTH_MODE_PSK            // WPA/WPA2-Personal (default) — config.h:81
//#define WIFI_AUTH_MODE_OPEN         // no password
//#define WIFI_AUTH_MODE_WPA3_SAE     // WPA3-Personal
//#define WIFI_AUTH_MODE_ENTERPRISE   // WPA2/WPA3-Enterprise (EAP) — config.h:84
```

Enterprise sub-selects exactly one EAP method (compiled only under `WIFI_AUTH_MODE_ENTERPRISE`):
`WIFI_EAP_METHOD_PEAP` (default) / `WIFI_EAP_METHOD_TTLS` (config.h:88) / `WIFI_EAP_METHOD_TLS`
(config.h:89). Plus three orthogonal optional flags: `USE_WIFI_CERTS` (config.h:94),
`USE_STATIC_IP` (config.h:97), `WIFI_HOSTNAME` (config.h:98). Verified present in source.

| Mode | Underlying call | Secrets |
|------|-----------------|---------|
| `WIFI_AUTH_MODE_OPEN` | `WiFi.begin(ssid)` | SSID only |
| `WIFI_AUTH_MODE_PSK` (default) | `WiFi.begin(ssid, pass)` | SSID + password |
| `WIFI_AUTH_MODE_WPA3_SAE` | `setMinSecurity(WIFI_AUTH_WPA3_PSK)` + `WiFi.begin(ssid, pass)` | SSID + password |
| `WIFI_AUTH_MODE_ENTERPRISE` | enterprise `WiFi.begin(...)` overload (PEAP/TTLS/TLS) | EAP identity/user/pass (+certs for TLS) |

### 2b. Secrets, certs, validation
- `include/wifi_credentials.h` — restructured per mode: `WIFI_SSID`+`WIFI_PASSWORD`, plus
  commented enterprise block (`WIFI_EAP_IDENTITY` / `WIFI_EAP_ANON_IDENTITY` outer-identity /
  `WIFI_EAP_USERNAME` / `WIFI_EAP_PASSWORD`) and a commented static-IP block
  (`WIFI_STATIC_IP/GATEWAY/SUBNET/DNS` in the comma-expanded `IPAddress(...)` idiom).
  `config.h` stays **secret-free**. Placeholder-only (`"YourNetworkName"`/`"YourPassword"`).
- `include/wifi_certs.h` — comment tweak only; PEM blobs unchanged, compiled only under
  `WIFI_AUTH_MODE_ENTERPRISE && USE_WIFI_CERTS`; `static_assert(sizeof(WIFI_CA_CERT) > 1)`
  rejects an empty CA blob (mirrors `ota.cpp`).
- `include/wifi_config.h` — the four `#error` guards (single include point so they fire once):
  (A) exactly-one auth mode; (B) Enterprise needs one EAP method + identity, PEAP/TTLS need
  user+pass, EAP-TLS needs `USE_WIFI_CERTS`; (C) `USE_WIFI_CERTS` needs `wifi_certs.h`;
  (D) `USE_STATIC_IP` needs all four IP values. Plus backward-compat aliases mapping legacy
  `WIFI_USE_ENTERPRISE` / `WIFI_USE_CERTS` / `WIFI_EAP_AUTH_METHOD` → new names, and a PSK
  default for old credentials files that only set SSID+PASSWORD.

### 2c. The liftable `wifi_connect` module (NEW)
- `include/wifi_connect.h` + `src/wifi_connect.cpp` — all per-mode `#ifdef` branching is
  hidden here behind a clean two-function API: `wifiConnectBegin()` (applies hostname +
  static IP + the single `#if/#elif` mode dispatch → `WiFi.begin(...)`) and
  `wifiAuthModeName()` (`"PSK"`/`"OPEN"`/`"WPA3-SAE"`/`"EAP-PEAP"`/… for logging). Depends on
  nothing project-specific (no `DisplayData_t`) so it lifts cleanly into future ESP32 projects.
- `src/wifi_manager.cpp` — the inline enterprise/personal branch was replaced with a single
  `wifiConnectBegin()` call (line 68) + a `wifiAuthModeName()` log line (line 64). MAC print,
  SSID-configured guard, 15 s/250 ms timeout wait, `handleWiFi()` reconnect, and
  `populateNetworkData()` are all unchanged.

---

## 3. Key decisions

- **Presence-flag selector, not `#define WIFI_AUTH_MODE x`** — matches the project's existing
  "uncomment ONE `#define`" idiom (IMU/display/RC-protocol selectors) and lets each mode's
  option block be `#ifdef`-gated trivially.
- **`WIFI_AUTH_MODE_*` prefix is mandatory (IDF enum name-collision gotcha).** The ESP-IDF
  defines `enum wifi_auth_mode_t` whose members are *named* `WIFI_AUTH_OPEN`,
  `WIFI_AUTH_ENTERPRISE`, `WIFI_AUTH_WPA3_PSK`, … (`esp_wifi_types.h`). Defining those bare
  tokens as preprocessor macros would corrupt `<esp_wifi_types.h>`. The `WIFI_AUTH_MODE_`
  prefix is collision-free; the only place a bare enum token appears is as the *argument
  value* to `setMinSecurity(WIFI_AUTH_WPA3_PSK)` — the intended enum use, not a `#define`.
- **Arduino convenience overload only** — no raw `<esp_wpa2.h>` / `esp_eap_client_*`. Correct
  and portable for the pinned Arduino-ESP32 core 3.20017 (IDF 5.1); `WPA2_AUTH_TTLS` exists in
  that core so the new TTLS path is valid. (If the platform pin bumps, re-verify `WiFiSTA.h`.)
- **STA mode only, one auth mode per build** — unused modes cost zero flash/RAM.
- **Multi-SSID fallback and HTTPS/TLS-for-API are DEFERRED** (bare-bones discipline). The cert
  flag + file layout are *reserved* for future HTTPS (`WiFiClientSecure::setCACert()`), but no
  HTTPS code was added.
- **Orthogonal to `USE_API_AUTH`.** Link-layer auth (this feature) and the command-surface
  token (`FLOPPI_CMD_TOKEN`, SEC-01/03 from the companion record) are independent; nothing
  here touches that path or the swarm_api contract.

### Memory finding (cheaper than estimated)
The plan (§8) predicted Enterprise would cost +20–40 KB flash. QA's empirical build matrix
found **all modes produce an identical 572273 B image** — the full WPA2-Enterprise/EAP
supplicant + mbedTLS is **already linked** by the WiFi lib + web/API/OTA stack regardless of
the selected mode. **Net incremental cost of selecting Enterprise here is ~0 ("Enterprise is
effectively free — mbedTLS already linked").** Default PSK build = 572273 B / 35644 B RAM,
byte-identical to the legacy path (the PSK-parity guarantee). The plan's §8 estimate should be
corrected in the feature doc when someone next touches it.

---

## 4. QA verdict — GO

Independent skeptical review (`reviewer@flight_controller:wifi-qa`,
[../../findings/wifi_modes_qa_2026-05-22.md](../../findings/wifi_modes_qa_2026-05-22.md)).
Read every changed file, cross-checked against the pinned core 3.x `WiFiSTA.h` /
`esp_wifi_types.h`, ran read-only `pio run -e esp32` for every mode + every `#error` guard:

- Enterprise overload arg order/types, WPA3-SAE `setMinSecurity` ordering, static-IP
  `WiFi.config()` ordering, `#ifdef` branch integrity, PSK byte-identical parity, all four
  `#error` guards (none too loose, none too strict), secrets/certs placeholders — **all SOUND**.
- Build matrix: default PSK / WPA3 / Enterprise-PEAP / Enterprise-TLS+certs / OPEN /
  static-IP+hostname all **SUCCESS**; the four bad-combo guards all **fail as designed**.
- **Verdict: GO** for default build + OPEN/WPA3/Enterprise compile-correctness.

### Bug found & FIXED — `WIFI_HOSTNAME` ordering (BUG-1, low severity, opt-in only)
QA found that core 3.x pushes the STA hostname to the netif **inside `WiFi.mode(WIFI_MODE_STA)`**,
so calling `WiFi.setHostname()` afterward (as the original `wifiConnectBegin()` did) meant a
custom `WIFI_HOSTNAME` wouldn't appear in DHCP/router tables on the first association. **Fixed**
in `src/wifi_manager.cpp:52-58`: the hostname is now applied *before* `WiFi.mode(WIFI_STA)`
(guarded, no-op when `WIFI_HOSTNAME` is unset). Verified in source. Zero impact on any default
build (default derives hostname from MAC).

QA nits not blocking: NIT-1 (`wifi_connect.cpp` doesn't include `wifi_config.h`, so a missing
`wifi_certs.h` yields a noisier compiler diagnostic in that TU — overall build still fails with
the clear message); NIT-2 (the hostname ordering caveat in the feature doc — moot now BUG-1 is
fixed).

---

## 5. Blockers / next steps

- **Runtime / real-network validation is the operator's step** (cannot be done from a build):
  actually associate against a real OPEN AP, a genuine WPA3-SAE AP, and an eduroam/RADIUS
  network (PEAP and, if used, EAP-TLS with real certs). The build proves it *compiles* and the
  calls are *correct for the pinned core*; only a live AP proves association. **Hardware-gated.**
- **Correct the plan's §8 flash-delta estimate** in `docs/features/wifi-configuration.md`
  (Enterprise is ~0 incremental, not +20–40 KB) — minor doc follow-up.
- Optionally address QA NIT-1 (`#include "wifi_config.h"` at the top of `wifi_connect.cpp`).
- **No commits made** — all changes (config.h, wifi_credentials.h, wifi_certs.h, wifi_config.h,
  wifi_connect.h/.cpp, wifi_manager.cpp) are in the working tree, awaiting operator review.

---

## 6. References

- Design plan: [../../plans/wifi-network-modes-plan.md](../../plans/wifi-network-modes-plan.md)
- Feature doc: [../../features/wifi-configuration.md](../../features/wifi-configuration.md)
- QA verdict: [../../findings/wifi_modes_qa_2026-05-22.md](../../findings/wifi_modes_qa_2026-05-22.md)
- Companion (security/correctness/docs): [2026-05-22_security_correctness_docs.md](2026-05-22_security_correctness_docs.md)

---

*Durable handoff record. Code/flag/path claims verified against the actual source on
2026-05-22. Sources cited above.*
