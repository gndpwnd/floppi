# ESP32 WiFi Onboarding

First-time WiFi setup for ESP32 / ESP32-S3 builds. For configuration reference
(WPA2-Personal vs Enterprise PEAP/TLS, certificates, mDNS hostname rules,
diagnostics command `n`), see [features/wifi-configuration.md](features/wifi-configuration.md).

> Teensy builds do not use WiFi. Skip this doc on Teensy targets.

---

## First-time setup

WiFi credentials live in a single header that gets included at compile time:

| File | Purpose | Edit? |
|------|---------|-------|
| `include/wifi_credentials.h` | SSID, password, enterprise fields | YES |
| `include/wifi_certs.h` | PEM certificates (only if `WIFI_USE_CERTS`) | Sometimes |
| `include/wifi_config.h` | Function declarations | NO |
| `src/wifi_manager.cpp` | Connection logic | NO |

The repo ships `wifi_credentials.h` pre-populated with placeholder values
(`"YourNetworkName"` / `"YourPassword"`). The firmware detects these at boot
and refuses to connect (`wifi_manager.cpp:58`), so an unedited build is safe
but offline.

## Creating / editing `include/wifi_credentials.h`

Minimal WPA2-Personal template:

```cpp
#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
// Optional WPA2-Enterprise (uncomment block in features/wifi-configuration.md):
//#define WIFI_AUTH_MODE_ENTERPRISE
//#define WIFI_EAP_METHOD_PEAP
//#define WIFI_EAP_IDENTITY    "user@org"
//#define WIFI_EAP_USERNAME    "user@org"
//#define WIFI_EAP_PASSWORD    "your_password"
#endif
```

For open networks, leave `WIFI_PASSWORD` as `""`. For Enterprise PEAP/TLS and
certificate flows, see `features/wifi-configuration.md` — the credential header
already contains the relevant commented-out template blocks.

## Verifying `.gitignore`

The repo `.gitignore` files do NOT currently exclude
`include/wifi_credentials.h`, and the file IS tracked in version control
(`git ls-files` confirms it). Edits to your real SSID/password will show up in
`git status` and risk being committed.

**Recommended before adding real credentials:**

```bash
# 1. Add to flight_controller/.gitignore (or the top-level .gitignore):
#      flight_controller/include/wifi_credentials.h
#      flight_controller/include/wifi_certs.h
# 2. Stop tracking without deleting on disk:
git rm --cached flight_controller/include/wifi_credentials.h
git rm --cached flight_controller/include/wifi_certs.h  # if used
git commit -m "Stop tracking wifi credentials"
```

Optionally check in `wifi_credentials.h.example` with placeholders for new
clones. Until that change lands, confirm `git status` before any commit
involving `include/`.

## Fallback hostname

The web server (`src/web_server.cpp:131-138`) computes the mDNS name from the
ESP32 MAC address:

```
floppi-XXXX.local      (XXXX = last two MAC bytes, hex)
```

This is the only "hostname" the firmware advertises — there is no static
default. If you have multiple ESP32s on the same network, each will have a
unique `floppi-XXXX.local` derived from its MAC.

If WiFi never connects: mDNS never starts, but the firmware keeps running.
Core 0 flight control is unaffected (WiFi runs entirely on Core 1). The
calibration mode `n` command will report the un-resolved hostname and the
disconnected state.

## First-boot expectation

Serial log (`pio device monitor` or `dev.sh monitor`) on first successful boot:

```
[WiFi] MAC: AA:BB:CC:DD:EE:FF
[WiFi] Connecting to: YourNetworkName
.......
[WiFi] Connected! IP: 192.168.1.42
[WiFi] RSSI: -55 dBm
[Web] mDNS: http://floppi-EEFF.local
[OTA] Ready at floppi-EEFF.local
```

On failure (credentials still placeholder, wrong password, network out of range):

```
[WiFi] MAC: AA:BB:CC:DD:EE:FF
[WiFi] No credentials configured!
[WiFi] Edit include/wifi_credentials.h with your network details
```

or, when credentials are real but connection times out (15 s):

```
[WiFi] Connecting to: YourNetworkName
.................................................................
[WiFi] Connection failed. Will retry in background.
```

The 15-second timeout in `setupWiFi()` is non-blocking after that point —
`handleWiFi()` retries every 5 s on Core 1 (`WIFI_RECONNECT_INTERVAL`).

## Common onboarding errors

| Symptom | Likely cause | Fix |
|---|---|---|
| `No credentials configured!` at boot | `WIFI_SSID` still `"YourNetworkName"` | Edit `include/wifi_credentials.h` and reflash |
| Connect attempt hangs, eventually times out | Wrong password, or 5 GHz-only SSID | ESP32 is 2.4 GHz only — confirm SSID is on 2.4 GHz, check password char-by-char |
| Connects to home WiFi but fails on university | Network is WPA2-Enterprise (eduroam etc.) | Uncomment `WIFI_AUTH_MODE_ENTERPRISE` block, fill EAP fields, see `features/wifi-configuration.md` |
| Captive-portal network (hotel/coffee shop) | ESP32 cannot complete a browser-based captive portal | Not supported — use a personal hotspot or a router without captive portal |
| Connects but `floppi-XXXX.local` doesn't resolve | mDNS blocked on network, or Windows host without Bonjour | Use the raw IP printed in the serial log; install mDNS responder on host if needed |
| Enterprise auth fails immediately | Wrong `WIFI_EAP_AUTH_METHOD`, or CA cert required by RADIUS server | Try `WPA2_AUTH_PEAP` first; if still fails, set `WIFI_USE_CERTS` and populate `wifi_certs.h` |

## References

- [features/wifi-configuration.md](features/wifi-configuration.md) — complete
  configuration reference (all auth modes, certificates, architecture, network
  diagnostics command `n`)
- `src/wifi_manager.cpp` — connection state machine
- `src/web_server.cpp` — mDNS hostname derivation
- `src/ota.cpp` — OTA hostname (same `floppi-XXXX` pattern, gated by `armedFly`)
- [diagnose_decision_tree.md](diagnose_decision_tree.md) — symptom-driven
  troubleshooting once WiFi is up but something else is wrong

## WiFi Credentials Setup

The repo ships `include/wifi_credentials.h.example` as the authoritative
template for new clones. The real `include/wifi_credentials.h` is also tracked
(so the build is self-contained), which means edits with your real SSID and
password would otherwise surface in `git status` and risk being committed. Use
this 3-step workflow on every fresh checkout to keep secrets local:

```bash
# 1. Seed the real header from the template (run from flight_controller/):
cp include/wifi_credentials.h.example include/wifi_credentials.h

# 2. Edit include/wifi_credentials.h and fill in:
#      - WIFI_SSID / WIFI_PASSWORD (or the EAP block for enterprise)
#      - FLOPPI_CMD_TOKEN if you will enable USE_API_AUTH
#      - OTA_PASSWORD     if you will enable USE_OTA

# 3. Tell your local git to ignore future edits to that file:
git update-index --skip-worktree include/wifi_credentials.h
```

`--skip-worktree` is per-clone and per-developer — it leaves the file tracked
upstream but stops your working copy from staging changes to it. To undo
later (e.g. to rebase a legitimate template change), run
`git update-index --no-skip-worktree include/wifi_credentials.h`.

### Before enabling `USE_API_AUTH` or `USE_OTA`

The following placeholders **hard-fail the build** (via `#error` in
`config.h` / `ota.cpp`) when the matching feature flag is enabled. If you flip
on `USE_API_AUTH` or `USE_OTA` and see a compile error, this is why:

| Placeholder | Required when | Replace with |
|---|---|---|
| `FLOPPI_CMD_TOKEN "CHANGE-ME-floppi-token"` | `USE_API_AUTH` enabled in `config.h` | A long random string shared with the ground station |
| `OTA_PASSWORD   "CHANGE-ME-floppi-ota"`     | `USE_OTA` enabled in `config.h`      | A non-empty password (or set `OTA_PASSWORD_HASH` instead) |

### Why this matters

The flight controller's WiFi password, API token, and OTA password together
gate physical control of the aircraft and the ability to flash arbitrary
firmware over the LAN. A single accidental `git add -A` followed by a push to
a public remote leaks all three at once — and unlike a server credential,
rotating a flashed device requires physical access to reflash. The template +
`--skip-worktree` pattern keeps the real secrets on the build host only, while
still letting the example header evolve in version control for new clones.
