# QA Verdict — Compile-Time WiFi Auth-Mode Selector

> **Reviewer**: `reviewer@flight_controller:wifi-qa` (Quality team), 2026-05-22
> **Scope**: skeptical correctness review of the uncommitted WiFi auth-mode feature.
> **Spec**: `docs/plans/wifi-network-modes-plan.md`
> **Method**: read all changed files, cross-checked against the pinned Arduino-ESP32
> core 3.x headers in `~/.platformio/packages/framework-arduinoespressif32/`, and ran
> read-only `pio run -e esp32` builds for every mode + every `#error` guard.
> **No source modified. No commits.**

## Verdict: GO (with one minor opt-in bug to fix — HOSTNAME ordering)

Default PSK build is byte-for-byte the legacy path and compiles clean. All five modes
compile, all four `#error` guards fire correctly, and the core-API calls match the pinned
core 3.x signatures. One real (minor, opt-in-only) ordering bug found in the `WIFI_HOSTNAME`
path — it does not affect any default build or any of PSK/OPEN/WPA3/Enterprise behavior, only
whether a custom hostname takes effect on first boot. Runtime / real-network validation
(actual association to OPEN/WPA3/eduroam APs) remains the operator's step.

---

## Per-area findings

### 1. Enterprise EAP `WiFi.begin(...)` overload — SOUND
Verified against `WiFi/src/WiFiSTA.h:47` (pinned core, framework-arduinoespressif32 3.x):
```cpp
wl_status_t begin(const char* wpa2_ssid, wpa2_auth_method_t method,
                  const char* wpa2_identity=NULL, const char* wpa2_username=NULL,
                  const char* wpa2_password=NULL, const char* ca_pem=NULL,
                  const char* client_crt=NULL, const char* client_key=NULL, ...);
```
The implementation (`src/wifi_connect.cpp:117-127`) passes
`(SSID, method, outer_identity, user, pass [, ca, crt, key])` — argument order and types
match exactly. `wpa2_auth_method_t { WPA2_AUTH_TLS=0, WPA2_AUTH_PEAP=1, WPA2_AUTH_TTLS=2 }`
(`WiFiSTA.h:34-37`) confirms `WPA2_AUTH_TTLS` exists, so the new TTLS path is valid.

- Outer/inner identity handling is correct: anon identity (if defined) → `wpa2_identity`
  (outer/privacy), `WIFI_EAP_USERNAME` → `wpa2_username` (inner). Matches the convenience
  overload's contract.
- EAP-TLS correctly leaves user/pass NULL (`wifi_connect.cpp:63-69`) and supplies cert/key.
- Cert args use `sizeof(WIFI_CLIENT_CERT) > 1 ? ... : NULL` (line 121-122) — matches the old
  empty-string→NULL idiom. CA always passed under `USE_WIFI_CERTS`.
- Uses only the Arduino convenience overload; **no raw `<esp_wpa2.h>`/`esp_eap_client_*`** —
  correct for portability, matches R1 mitigation in the plan.

### 2. WPA3-SAE — SOUND
`src/wifi_connect.cpp:111-113`:
```cpp
WiFi.setMinSecurity(WIFI_AUTH_WPA3_PSK);   // force SAE
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
```
- `setMinSecurity(wifi_auth_mode_t)` exists at `WiFiSTA.h:75`.
- `WIFI_AUTH_WPA3_PSK` is a valid enum member (`esp_wifi_types.h:60`).
- The PSK password is still passed to `begin()` — correct (WPA3-Personal = SAE over the
  passphrase). `setMinSecurity` is called before `begin()` — correct (the header comment at
  `WiFiSTA.h:74` requires "before WiFi.begin()").

### 3. Static IP — SOUND
`src/wifi_connect.cpp:99-102`:
```cpp
WiFi.config(IPAddress(WIFI_STATIC_IP), IPAddress(WIFI_STATIC_GATEWAY),
            IPAddress(WIFI_STATIC_SUBNET), IPAddress(WIFI_STATIC_DNS));
```
`WiFiSTA.h:58`: `config(local_ip, gateway, subnet, dns1=0, dns2=0)` — order matches
(local, gateway, subnet, dns). `config()` is called inside `wifiConnectBegin()` **before**
the per-mode `WiFi.begin(...)` dispatch — correct (must precede begin). The comma-expanded
`192,168,1,50 → IPAddress(...)` idiom builds cleanly (verified by build).

### 4. `#ifdef` branch integrity — SOUND
- Exactly-one-mode is enforced by `WIFI_MODE_COUNT` + `#error` (wifi_config.h:56-73).
  Verified empirically: default PSK + a second `-D` mode → "Multiple WiFi auth modes" error.
- The mode dispatch in `wifiConnectBegin()` is a single `#if/#elif/#elif/#elif` chain
  (`wifi_connect.cpp:105-128`) — no fall-through, exactly one branch emits a `begin()`.
- The `wifi_connect` module cleanly hides every mode `#ifdef`; `wifi_manager.cpp:62` just
  calls `wifiConnectBegin()` and logs `wifiAuthModeName()`. Clean separation achieved.
- **No leftover inline `WIFI_USE_ENTERPRISE` logic.** Grep across `src/` + `include/` shows
  the only references to `WIFI_USE_ENTERPRISE`/`WIFI_EAP_AUTH_METHOD`/`WIFI_USE_CERTS` are in
  the alias/comment blocks (wifi_config.h:31-53, wifi_certs.h:4). No double-define risk.

### 5. PSK-path parity — SOUND
`src/wifi_connect.cpp:108-109` is `WiFi.begin(WIFI_SSID, WIFI_PASSWORD)` — identical to the
legacy personal call. The only behavioral diff is the new `[WiFi] Auth mode: PSK` log line
(wifi_manager.cpp:57-58). `setupWiFi()` timeout (15 s / 250 ms poll), `handleWiFi()`
reconnect loop (5 s `WiFi.reconnect()`), and `populateNetworkData()` are unchanged. The
"unconfigured"/"YourNetworkName" guard is unchanged. **Regression-free** for the committed
default. Confirmed by build: default `esp32` = 572273 B flash, 35644 B RAM.

### 6. Validation guards — SOUND
All four `#error` blocks were exercised by deliberately broken `-D` builds and all fired:
| Guard | Trigger tested | Result |
|-------|----------------|--------|
| (A) exactly-one mode | default PSK + `-DWIFI_AUTH_MODE_WPA3_SAE` | fires "Multiple WiFi auth modes" |
| (B) enterprise needs identity | `ENTERPRISE+PEAP`, no identity | fires "requires WIFI_EAP_IDENTITY" + "PEAP/TTLS require..." |
| (B) EAP-TLS needs certs | `ENTERPRISE+TLS`, no `USE_WIFI_CERTS` | fires "EAP-TLS requires USE_WIFI_CERTS" |
| (D) static IP needs all 4 | `-DUSE_STATIC_IP` alone | fires "USE_STATIC_IP requires ..." |

None too loose (each bad combo is rejected); none too strict (every valid combo built — see
build matrix). The IDF enum name-collision fix is **complete and correct**: the selector uses
the `WIFI_AUTH_MODE_*` prefix, never the bare `WIFI_AUTH_OPEN`/`WIFI_AUTH_ENTERPRISE`/
`WIFI_AUTH_WPA3_PSK` tokens, which I confirmed ARE real members of `wifi_auth_mode_t`
(`esp_wifi_types.h:53,58,60`). No remaining macro shadows an enum member — the only place the
bare `WIFI_AUTH_WPA3_PSK` token appears is as an *argument value* to `setMinSecurity()`
(wifi_connect.cpp:112), which is the intended enum use, not a `#define`.

### 7. Secrets / certs — SOUND
- `wifi_credentials.h` is placeholder-only (`"YourNetworkName"`/`"YourPassword"`,
  enterprise + static-IP blocks commented out). The `setupWiFi()` guard treats
  `"YourNetworkName"` as unconfigured. No real secrets.
- `wifi_certs.h` is placeholder PEM (`REPLACE_WITH_YOUR_CA_CERTIFICATE`, empty client
  cert/key). Compiled only under `WIFI_AUTH_MODE_ENTERPRISE && USE_WIFI_CERTS`
  (wifi_connect.cpp:31). `__has_include` guards intact in both wifi_connect.cpp:19 and
  wifi_config.h:21. The `static_assert(sizeof(WIFI_CA_CERT) > 1, ...)` (line 37) correctly
  rejects an empty CA blob (mirrors ota.cpp).

---

## Bugs / nits

### BUG-1 (minor, opt-in only) — `WIFI_HOSTNAME` is applied too late to take effect on first boot
**Files**: `src/wifi_manager.cpp:52` + `src/wifi_connect.cpp:94-96`

In core 3.x the STA hostname is pushed to the netif **inside `WiFi.mode(WIFI_MODE_STA)`**:
`WiFiGeneric.cpp:1265` calls `set_esp_interface_hostname(ESP_IF_WIFI_STA, get_esp_netif_hostname())`
at mode-set time, and `WiFi.setHostname()` only writes a static `default_hostname` buffer
(`WiFiGeneric.cpp:901-905` → `set_esp_netif_hostname`, line 292-296). The STA_START event
handler (`WiFiGeneric.cpp:1043-1048`) does **not** re-apply it.

Current call order in `setupWiFi()`:
1. `WiFi.mode(WIFI_STA)` — wifi_manager.cpp:52 → applies the *current* (default MAC-derived) hostname
2. `wifiConnectBegin()` → `WiFi.setHostname(WIFI_HOSTNAME)` — wifi_connect.cpp:95 → updates the buffer **after** it was already consumed

Net effect: a custom `WIFI_HOSTNAME` does **not** appear in DHCP/router tables on the first
(boot) association; the device shows up under its default `espXX-XXXXXX` name instead. It
would only take effect on a later mode toggle, which never happens here. Default-derived
hostname is unaffected (no bug when `WIFI_HOSTNAME` is undefined — which is the default).

**Fix** (operator/dev step, not mine to apply): set the hostname **before** `WiFi.mode()`.
Either move a `WiFi.setHostname(WIFI_HOSTNAME)` call ahead of wifi_manager.cpp:52, or split
`wifiConnectBegin()` so the hostname is applied pre-mode. Simplest: in `wifi_manager.cpp`,
right before `WiFi.mode(WIFI_STA)`, add (guarded):
```cpp
#if defined(WIFI_HOSTNAME)
    WiFi.setHostname(WIFI_HOSTNAME);
#endif
```
and drop the duplicate from `wifiConnectBegin()` (or leave it — harmless once the pre-mode
call exists). NOTE: this would re-introduce a tiny project-specific `#ifdef` into
wifi_manager, slightly denting the "module hides all ifdefs" goal; acceptable, or keep it in
wifi_connect by exposing a `wifiApplyHostname()` called before mode-set.

Severity: LOW. Opt-in feature, cosmetic (DHCP naming), zero impact on connectivity or any
default build. Worth fixing before anyone relies on `WIFI_HOSTNAME`.

### NIT-1 (robustness) — `wifi_connect.cpp` doesn't include `wifi_config.h`, so guard (C) doesn't protect its TU
`wifi_connect.cpp` includes only `config.h`, not `wifi_config.h`. So if a user sets
`USE_WIFI_CERTS` but `wifi_certs.h` is missing, the friendly `#error` (C) in wifi_config.h
does **not** fire in the wifi_connect.cpp translation unit; instead the `static_assert` on
line 37 hits an *undefined* `WIFI_CA_CERT` and emits a confusing compiler error there. The
overall build still fails with the clear message (wifi_manager.cpp DOES include
wifi_config.h, so guard (C) fires somewhere), but the wifi_connect.cpp TU error is noisy.
Severity: VERY LOW (cosmetic ordering of compiler diagnostics). Optional: `#include
"wifi_config.h"` at the top of wifi_connect.cpp, or wrap the `__has_include` so a missing
file degrades to NULL CA rather than an undefined symbol.

### NIT-2 (doc) — hostname ordering caveat not documented
`docs/features/wifi-configuration.md:170-178` describes `WIFI_HOSTNAME` as "one
`setHostname()` call" but does not mention the pre-mode ordering requirement. If BUG-1 is
fixed in code this is moot; otherwise the doc overstates that it works.

---

## Build confirmation (read-only, this wave owns the FC .pio lock)

| Build | Flags | Result |
|-------|-------|--------|
| Default (PSK) | none | **SUCCESS** — Flash 43.7% (572273 B), RAM 10.9% (35644 B) |
| WPA3-SAE | `-DWIFI_AUTH_OVERRIDE -DWIFI_AUTH_MODE_WPA3_SAE` | SUCCESS — 572273 B (no delta) |
| Enterprise PEAP | `...ENTERPRISE -DWIFI_EAP_METHOD_PEAP` + identity/user/pass | SUCCESS — 572273 B (no delta) |
| Enterprise TLS + certs | `...ENTERPRISE -DWIFI_EAP_METHOD_TLS -DUSE_WIFI_CERTS` + identity | SUCCESS |
| OPEN | `-DWIFI_AUTH_OVERRIDE -DWIFI_AUTH_MODE_OPEN` | SUCCESS |
| Static IP + hostname (valid) | all 4 IPs + `-DWIFI_HOSTNAME` | SUCCESS |
| **#error: 2 modes** | default + `-DWIFI_AUTH_MODE_WPA3_SAE` | fails as designed |
| **#error: ent. no identity** | `...ENTERPRISE -DWIFI_EAP_METHOD_PEAP` | fails as designed |
| **#error: TLS no certs** | `...ENTERPRISE -DWIFI_EAP_METHOD_TLS` | fails as designed |
| **#error: static IP partial** | `-DUSE_STATIC_IP` alone | fails as designed |

`.pio` left at the clean default-PSK build after testing.

**Observed flash deltas contradict the plan's §8 estimate** (which predicted Enterprise
+20-40 KB): on this build *all* modes produce an identical 572273 B image, because the full
WPA2-Enterprise/EAP supplicant + mbedTLS is already linked by the WiFi lib + web/API/OTA
stack regardless of the selected mode. Net incremental cost of selecting Enterprise here is
~0. This is a finding (cheaper than feared), not a defect — but the §8 numbers should be
corrected in the feature doc when someone next touches it. (Note: with `-DWIFI_AUTH_OVERRIDE`
the default `WIFI_EAP_METHOD_PEAP` block is suppressed because it lives inside the
`#ifndef WIFI_AUTH_OVERRIDE` guard, so the override path must also supply the EAP method —
which it does in the test matrix. Real config.h edits don't have this concern.)

---

## Final go/no-go

**GO** for the default (PSK) build and for OPEN/WPA3/Enterprise compile-correctness — the core
API calls are right, branch integrity is clean, validation is tight, no secrets leaked, and
PSK is a verified zero-regression of the legacy path.

**Remaining for the operator/dev**:
1. Fix BUG-1 (HOSTNAME pre-mode ordering) before relying on `WIFI_HOSTNAME`. Low severity.
2. Optionally address NIT-1/NIT-2.
3. **Runtime / real-network validation** is unavoidable and outside this review: actually
   associate against a real OPEN AP, a genuine WPA3-SAE AP, and an eduroam/RADIUS network
   (PEAP and, if used, TLS with real certs). The build proves it *compiles* and the calls are
   *correct for the pinned core*; only a live AP proves association.
