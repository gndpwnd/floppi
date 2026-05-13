# WiFi + Telemetry Integration Design for auto_orientation

## Recommendation Summary

- **Mirror the flight_controller layout 1:1**: a `src/network/` subtree with `wifi_manager`, `web_server`, `api_server`, and `ota` modules, each guarded by `#if defined(HAS_WIFI_CAPABLE) && defined(USE_WIFI)` plus a per-feature flag. This keeps the Arduino Mega / Nano / Teensy builds completely WiFi-free and the ESP32/ESP32-S3 builds opt-in.
- **Use ESPAsyncWebServer + AsyncWebSocket on Core 1, EKF/IMU on Core 0**, with a single `portMUX_INITIALIZER_UNLOCKED` spinlock guarding a `latestEKFSnapshot_t` struct (the same pattern `flight_controller/src/web_server.cpp` already uses for `DisplayData_t`). No new IPC primitives needed.
- **Choose ArduinoOTA over HTTP-pull** for now (matches `flight_controller/src/ota.cpp`, works with `pio run -t upload --upload-port floppi-bot-XXXX.local`, and is fine for a research framework on a trusted LAN). Document the upgrade path to signed HTTP-pull for fielded deployments under the Security section.

---

## 1. What "WiFi telemetry" means for auto_orientation

WiFi is **not** a flight-critical path; it is a developer/UX surface bolted onto the existing `SensorOutputManager`. Applications:

| Application | Phase | Direction | Channel |
|---|---|---|---|
| Live 3D quaternion view in browser | 4+ | FC -> browser | WS `/ws` (`formatFusedStateJSON`) |
| Phone-held balance-point capture | 4 | browser -> FC | `POST /api/calibration/capture` |
| Auto-PID-tune progress curve | 5 | FC -> browser | WS `/ws` event `pid_tune.progress` |
| OTA firmware updates | any | host -> FC | ArduinoOTA UDP/3232 |
| Headless logging (SD-card replacement) | 4+ | FC -> server | `POST /api/log` to swarm_api/custom sink |
| Remote command (replay test vectors) | 6+ | host -> FC | `POST /api/commands` |

All additive to the existing serial JSON output — `pio device monitor` keeps working, WiFi just gives the same payload a second sink.

## 2. Compile-flag cascade

Mirror `flight_controller/include/config.h` lines 54-58. The auto-enable block belongs in `src/config/mode.h` (or a new `src/config/network.h`) so it is visible everywhere:

```c
// --- Platform capability detection ---
#if defined(USE_ESP32) || defined(USE_ESP32S3)
#  define HAS_WIFI_CAPABLE 1
#endif

// --- WiFi sub-feature cascade (auto-enabled when USE_WIFI is set) ---
#if defined(HAS_WIFI_CAPABLE) && defined(USE_WIFI)
#  define USE_WEB_SERVER       // ESPAsyncWebServer + AsyncWebSocket on Core 1
#  define USE_API_SERVER       // Outbound HTTP POST to a ground-station sink
#  define USE_OTA              // ArduinoOTA over UDP/3232
#endif

// --- Hard error on non-ESP32 platforms ---
#if defined(USE_WIFI) && !defined(HAS_WIFI_CAPABLE)
#  error "USE_WIFI requires USE_ESP32 or USE_ESP32S3"
#endif
```

What each flag enables:

- `USE_WIFI` — STA-mode connect/reconnect + mDNS hostname (`floppi-bot-XXXX.local`). Sole prerequisite.
- `USE_WEB_SERVER` — REST + WebSocket + static dashboard. Default ON.
- `USE_API_SERVER` — Outbound telemetry POST to a configured URL (headless logging / swarm). Default ON.
- `USE_OTA` — ArduinoOTA service. Default ON. Disable on fielded units that should not auto-flash.

Per-feature `//#define` overrides let users disable any one without losing WiFi (same pattern as FC).

## 3. Module structure

Proposed subtree under `auto_orientation/src/network/`:

```
src/network/
  wifi_manager.{h,cpp}    // STA connect, reconnect, mDNS
  web_server.{h,cpp}      // ESPAsyncWebServer routes + WS
  api_server.{h,cpp}      // outbound HTTP client (mirrors api_client.cpp)
  ota.{h,cpp}             // ArduinoOTA wrapper
  wifi_credentials.h      // tracked w/ placeholders (FC convention)
  wifi_certs.h            // template; gitignored real PEM blobs
  web_assets/             // index.html, app.js, style.css as PROGMEM blobs
```

`wifi_credentials.h` follows `flight_controller/include/wifi_credentials.h` exactly — tracked, `"YourNetworkName"` / `"YourPassword"` placeholders, optional `#define WIFI_USE_ENTERPRISE` block, `__has_include` fallback in `wifi_manager.cpp`. mDNS hostname: `floppi-bot-XXYY` (MAC bytes 4-5) to avoid colliding with FC drones (`floppi-XXYY`) on the same LAN.

Every `.cpp` opens with `#include "config.h"` then `#if defined(HAS_WIFI_CAPABLE) && defined(USE_xxx)` — identical to FC. On AVR/Teensy builds these compile to empty translation units.

## 4. API endpoint surface

Wire-compatible with `swarm_api` (see `swarm_api/src/drone.py:78-138`: polls `/api/status`, posts `/api/commands`) plus auto_orientation-specific additions:

| Method | Path | Use case | Payload |
|---|---|---|---|
| GET  | `/api/status`              | Full EKF snapshot (orientation + position + biases + GPS status) | `formatFusedStateJSON` output |
| GET  | `/api/calibration`         | Current cal struct + completion flags | JSON from `calibration_storage` |
| POST | `/api/calibration/start`   | Begin interactive cal session | `{"mode":"balance_point"}` |
| POST | `/api/calibration/capture` | Capture sample at current pose | `{}` (uses live sensor) |
| POST | `/api/calibration/save`    | Persist to NVS/EEPROM | `{}` |
| POST | `/api/calibration/cancel`  | Abort cal session | `{}` |
| POST | `/api/pid_tune/start`      | Launch auto-tune (Phase 5) | `{"axis":"pitch","duration_s":30}` |
| GET  | `/api/pid_tune/status`     | Live progress + current Kp/Ki/Kd | tuner state JSON |
| POST | `/api/pid_tune/cancel`     | Abort tune | `{}` |
| GET  | `/api/system`              | Build info, free heap, uptime, RSSI | system JSON |
| POST | `/api/commands`            | Command override (`swarm_api` compat) | `{"ch1":1500,...}` |
| WS   | `/ws`                      | Telemetry stream + push events | binary or text frames |

WS events: tagged JSON `{"t":"fused", ...}`, `{"t":"cal.progress", ...}`, `{"t":"pid.curve", ...}`. Browser subscribes once, demuxes by `t`.

## 5. Dual-core distribution on ESP32

The multi-MCU strategy doc (`docs/findings/multi_mcu_port_strategy.md`) does not exist yet; the split below follows FC's working layout.

```
Core 0 (pinned FreeRTOS task, priority 3, stack 8192):
  IMU read -> EKF predict/update -> SensorOutputManager.updateFusedState()
  Loop: 100-400 Hz (BNO085 SH-2 max ~400 Hz). NEVER touches WiFi APIs.

Core 1 (default Arduino loopTask, priority 1, stack 8192):
  wifiLoop()      -- reconnect tick every 5 s
  webServerLoop() -- ws.cleanupClients(), broadcast at 10 Hz
  apiLoop()       -- outbound POST every 500 ms
  ArduinoOTA.handle() -- only when DISARMED / no cal session
  Build flags: -D CONFIG_ASYNC_TCP_RUNNING_CORE=1 -D CONFIG_ASYNC_TCP_STACK_SIZE=4096
  (copy from flight_controller/platformio.ini lines 145-146)
```

**IPC**: a `portMUX_TYPE` spinlock + `latestEKFSnapshot_t` struct, written by Core 0 in `updateFusedState()`, read by Core 1 in the WS broadcast. Same pattern as `web_server.cpp:33` (`portMUX_INITIALIZER_UNLOCKED` + `dataMux`). No FreeRTOS queue needed — the EKF is state, not a stream, and stale-by-one-tick is fine at 10 Hz.

For inbound commands (cal triggers, PID-tune start), a small `xQueueCreate(8, sizeof(NetCommand_t))` is cleaner — events, not state. Core 1 enqueues, Core 0 drains at top-of-loop.

## 6. Browser dashboard scope (Phase 6+)

Minimal SPA served from PROGMEM via `web_assets/`:

- `index.html` — three.js quaternion cube + live readout (RPY, ENU position, GPS-fix badge), subscribes to `/ws`.
- `cal.html` — wizard with "Capture" button, posts to `/api/calibration/capture`.
- `tune.html` — Chart.js graph of Kp/Ki/Kd vs time during auto-tune.

Assets become PROGMEM byte arrays via a `tools/pack_web_assets.py` step (gzip first, serve with `Content-Encoding: gzip`). `web_server.cpp` registers them with `request->send_P(200, "text/html", index_html_gz, sizeof(index_html_gz))`. **Do not author the HTML in this design phase** — just reserve the directory.

## 7. OTA strategy

**Recommend ArduinoOTA.** The FC already uses it (`flight_controller/src/ota.cpp`), `pio run -t upload --upload-port floppi-bot-A1B2.local` Just Works once mDNS is up, and the `onStart`/`onEnd`/`onError` callbacks integrate cleanly with a calibration-state guard ("refuse OTA while a cal session is open" — analog of FC's `armedFly` guard). Built into the ESP32 Arduino core (`#include <ArduinoOTA.h>`) — no library dep.

Switch to signed HTTP-pull (`esp_https_ota.h`) only when fielded bots live on networks the developer cannot reach, or when firmware signing becomes a requirement. Note as future option, do not block on it.

## 8. Security considerations

No authentication is required for a local-network research framework. To harden later:

- HTTPS via ESPAsyncWebServer + mbedTLS (~30 KB heap cost — skip for v1).
- Token auth: `Authorization: Bearer ...` check in route handlers, token in `wifi_credentials.h`.
- WS origin check on upgrade.
- `ArduinoOTA.setPassword("...")`.
- Disable `USE_API_SERVER` on locked-down units to prevent exfiltration to a misconfigured URL.
- Never expose to the public Internet (no port-forwarding, no `0.0.0.0` bind on a routable interface).

## 9. Out of scope

- **Multi-bot fleet coordination** — `swarm_api`'s domain. The WiFi layer must be wire-compatible with swarm_api (`/api/status`, `/api/commands` shape) but must not contain fleet logic.
- **Cloud / MQTT / AWS IoT** — swarm_api is the bridge if a cloud sink is ever needed.
- **Public Internet exposure** — forbidden. STA mode on trusted LAN only.
- **WiFi provisioning UI (SmartConfig / captive portal)** — credentials live in `wifi_credentials.h` at compile time, identical to FC.

---

**References**:

- `flight_controller/include/config.h` (43-58) — cascade; `wifi_credentials.h` — template
- `flight_controller/src/{wifi_manager,web_server,ota,api_client}.cpp` — module patterns
- `flight_controller/platformio.ini` (126-186) — ESP32/S3 envs, `CONFIG_ASYNC_TCP_RUNNING_CORE`
- `swarm_api/src/drone.py` — confirms `/api/status` + `/api/commands` + `/ws` contract
- ESP32 Arduino core APIs: `WiFi.h` (`WiFi.mode(WIFI_STA)`, `setAutoReconnect`), `ESPmDNS.h` (`MDNS.begin`), `ESPAsyncWebServer.h` (`AsyncWebServer`, `AsyncWebSocket`, `send_P`), `ArduinoOTA.h` (`setHostname`, `setPassword`, `onStart`, `handle`)
