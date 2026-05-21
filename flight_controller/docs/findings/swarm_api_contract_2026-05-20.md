# Swarm API Contract Specification

> Date: 2026-05-20
> Agent: fc-swarm-api-spec@flight_controller:1
> Status: Spec lifted from existing implementation — NOT new protocol design
> Scaffolding source: `future_session_scaffolding_2026-05-20.md` §3.5

**Verified 2026-05-20 against:**
- `src/web_server.cpp@HEAD` (commit 9dd60ca)
- `src/api_client.cpp@HEAD`
- `lib/RadioComm/radioComm.cpp`, `radioComm_ext.cpp`, `radioComm.h`
- `docs/findings/command-arbitration-design.md`
- `swarm_api/src/drone.py@HEAD` (client side — readable and reviewed)

This document is the joint contract between the ESP32 `flight_controller` firmware
and the sibling Python `swarm_api` project. Both sides should reference it. When
either side changes, re-stamp the "Verified" block above with the new SHA.

---

## 1. Overview

The **Swarm API** is the WiFi/HTTP/WebSocket surface that an ESP32-built
`flight_controller` exposes for external coordination. It lets a central host
(running `swarm_api`) discover, monitor, and command one or more drones over a
local WiFi network.

The surface has three parts:

1. **Inbound HTTP REST** — a swarm coordinator polls telemetry and POSTs channel
   commands to each drone (`GET /api/status`, `POST /api/commands`, `GET /`).
2. **Inbound WebSocket** — `/ws` streams telemetry out at 10 Hz and accepts
   channel commands in, on a single persistent socket.
3. **Outbound HTTP POST** — the FC's own `api_client` POSTs telemetry to a
   central server at `API_SERVER_URL/api/telemetry` (drone-initiated push).

All of this runs on **ESP32 Core 1** only. Flight control runs on Core 0 at
1 kHz and never touches networking. Cross-core data passes through spinlock-
protected buffers (`DisplayData_t` snapshot, `wifiCmdChannels[]`).

The entire surface is compile-gated. REST/WS require `USE_ESP32 && USE_WEB_SERVER`;
the outbound client requires `USE_ESP32 && USE_API_SERVER` plus `API_SERVER_URL`
defined in `wifi_credentials.h`. On Teensy builds none of this exists.

---

## 2. Transport

### HTTP REST server
- **Port:** 80 (hardcoded — `AsyncWebServer server(80)` in `web_server.cpp:29`).
- **mDNS hostname:** `floppi-XXXX.local`, where `XXXX` is the **last two MAC
  octets** uppercase hex. Computed in `setupWebServer()`:
  `snprintf(mdns_hostname, ..., "floppi-%02X%02X", mac[4], mac[5])`.
  Example: MAC `...:A1:B2` → `floppi-A1B2.local`.
- mDNS service advertised: `_http._tcp` on port 80.
- Server library: ESPAsyncWebServer; JSON via ArduinoJson.
- **No authentication, no TLS.** Plain HTTP. Assumes a trusted LAN. (See §8.)

### WebSocket
- **Endpoint:** `ws://floppi-XXXX.local/ws` (or `ws://<ip>/ws`).
- Single endpoint, bidirectional: telemetry frames out, command frames in.
- Server-push telemetry rate: **10 Hz** (`WS_BROADCAST_INTERVAL_MS = 100`),
  only broadcast when `ws.count() > 0`.
- Dead clients reaped each Core-1 loop via `ws.cleanupClients()`.

### Outbound HTTP POST telemetry client
- Active only when `API_SERVER_URL` is defined in `wifi_credentials.h`.
- Target: `{API_SERVER_URL}/api/telemetry`.
- Method: HTTP POST, `Content-Type: application/json`, 2 s timeout.
- Interval: `API_POST_INTERVAL_MS`, default **500 ms (2 Hz)**.
- Blocking call, acceptable because it runs on Core 1.
- A second URL `{API_SERVER_URL}/api/commands` is built in `setupApiClient()`
  but **never used** — inbound command pull is marked "future" and not
  implemented (`api_client.cpp:107`). FLAG: do not rely on it.

---

## 3. Endpoint reference

All schemas below are read directly from `web_server.cpp`. Field names and types
are exact.

### 3.1 `GET /api/status`

Returns a JSON telemetry snapshot of the latest `DisplayData_t` (copied under a
spinlock). Handler: `web_server.cpp:146-158`. Serializer:
`serializeDisplayData()` (`web_server.cpp:47-88`).

**Request:** none (no query params, no body).

**Response:** `200`, `application/json`:

```json
{
  "armed": false,
  "calibrating": false,
  "roll": -1.24,
  "pitch": 0.51,
  "yaw": 178.30,
  "imu": {
    "ax": 0.0123, "ay": -0.0045, "az": 0.9981,
    "gx": 0.12, "gy": -0.03, "gz": 0.00
  },
  "motors": { "m1": 0.000, "m2": 0.000, "m3": 0.000, "m4": 0.000 },
  "loop_us": 980,
  "net": {
    "mac": "AA:BB:CC:DD:A1:B2",
    "hostname": "floppi-A1B2",
    "ssid": "myWifi",
    "ip": "192.168.1.42",
    "rssi": -58,
    "connected": true
  },
  "heap": 210456,
  "uptime_ms": 84231
}
```

Field reference:

| Field | Type | Meaning |
|---|---|---|
| `armed` | bool | FC armed state (`DisplayData_t.armed`) |
| `calibrating` | bool | Calibration in progress |
| `roll`/`pitch`/`yaw` | number | Madgwick attitude, degrees, 2 dp |
| `imu.ax/ay/az` | number | Accel, g, 4 dp |
| `imu.gx/gy/gz` | number | Gyro, deg/s, 2 dp |
| `motors.m1..m4` | number | Motor output, scaled 0–1, 3 dp |
| `loop_us` | int | Core-0 loop duration, microseconds |
| `net.mac` | string | `"AA:BB:CC:DD:EE:FF"` |
| `net.hostname` | string | mDNS name without `.local` |
| `net.ssid` | string | Connected WiFi SSID |
| `net.ip` | string | Current STA IP |
| `net.rssi` | int | WiFi RSSI, dBm |
| `net.connected` | bool | WiFi link state |
| `heap` | int | `ESP.getFreeHeap()` bytes |
| `uptime_ms` | int | `millis()` since boot |

Note: numeric fields use ArduinoJson `serialized(String(...))` — emitted as raw
JSON numbers with fixed decimals, not strings.

### 3.2 `POST /api/commands`

Receives 6 RC channel values from an external controller. Handler:
`web_server.cpp:163-185`.

**Request:** `application/json` body:

```json
{ "ch1": 1500, "ch2": 1500, "ch3": 1000, "ch4": 1500, "ch5": 1000, "ch6": 1000 }
```

- Values are **microseconds, 1000–2000**, identical semantics to RC receiver
  PWM output. Channel mapping by FC convention: ch1 roll, ch2 pitch, ch3
  throttle, ch4 yaw, ch5 aux1 (arm), ch6 aux2.
- Any missing field defaults — ch1/ch2/ch4 → `1500`, ch3/ch5/ch6 → `1000`
  (via ArduinoJson `doc["chN"] | (uint16_t)default`). FLAG: a partial POST
  silently substitutes defaults; clients should always send all 6.
- The body is only parsed once fully received (`index + len == total`).

**Responses:**
- `200` `{"ok":true}` — accepted; channels handed to `setWifiCommandChannels()`.
- `400` `{"error":"invalid json"}` — body failed `deserializeJson()`.

There is no response echo of accepted values and no per-command ack ID.

### 3.3 `WS /ws`

Bidirectional WebSocket. Handler: `onWsEvent()` (`web_server.cpp:94-124`).

**Server → client (telemetry):** every 100 ms while ≥1 client connected, the
server sends a text frame with the **exact same JSON schema as `/api/status`**
(`serializeDisplayData()` is shared). Buffer cap 512 bytes.

**Client → server (commands):** send a text frame with the same body schema as
`POST /api/commands`:

```json
{ "ch1": 1500, "ch2": 1500, "ch3": 1000, "ch4": 1500, "ch5": 1000, "ch6": 1000 }
```

- Only `WS_TEXT`, single-frame, complete messages are parsed
  (`info->final && info->index==0 && info->len==len`). Fragmented or binary
  frames are dropped silently.
- Same default-substitution behavior as REST. Invalid JSON is silently ignored
  — **no error frame is sent back.** FLAG: WS command failures are invisible
  to the client.
- Routed to the same `setWifiCommandChannels()` as REST.

### 3.4 `GET /`

Human-readable `text/plain` status page (hostname, MAC, armed, attitude, loop
µs, IP, RSSI, heap). Handler: `web_server.cpp:188-212`. For browser/diagnostic
use; not part of the machine contract.

---

## 4. Command schema + arbitration

### What can be commanded over WiFi
Only the **6 RC channels** (1000–2000 µs). There is no other command surface —
no "arm" verb, no mode command, no parameter write. Arming is achieved
indirectly by commanding the throttle-low + CH5 channel pattern the FC's arming
logic expects, exactly as a physical RC transmitter would. FLAG: a swarm client
arms a drone by sending channel values, not by an explicit API call.

### Arbitration against RC (per `command-arbitration-design.md`)
WiFi is an **OVERRIDE source**, the lowest-priority of the override tier.
Implemented in `radioComm.cpp` `getCommands()`:

Priority order (highest first): **Serial → I2C → WiFi → RC (primary fallback)**.

- Every compiled source is polled into its own `CommandBuffer` each loop.
- An override source is selected only while it is `active`. Selection picks the
  first active source in priority order; if no override is active, the active
  RC receiver wins (`radioComm.cpp:132-169`).
- A `CommandBuffer` is marked **inactive** when no new data has arrived for
  `OVERRIDE_TIMEOUT_MS = 500` ms (`radioComm.h:41`, `radioComm_ext.cpp:208`).
- `setWifiCommandChannels()` stamps `wifiCmdTimestamp = millis()` on every
  accepted REST/WS command; `readWifiCmd()` promotes the buffer to `active`
  whenever that timestamp advances (`radioComm_ext.cpp:166-212`).

**Timeout-to-RC-fallback:** if a WiFi client stops sending for >500 ms, the WiFi
buffer goes inactive and `getCommands()` falls back to the next priority source
— an RC receiver if one is connected and active. To hold control, a swarm
client must send commands at **>2 Hz** (recommended ≥5–10 Hz; the WS path is
designed for this).

**Arbitration only exists when `USE_COMMAND_ARBITRATION` is defined.** In a
WiFi-only build (no RC receiver compiled), WiFi is the sole source — there is no
RC to fall back to (see Failsafe below).

### Failsafe on link loss
Implemented in `failSafe()` (`radioComm.cpp:213-253`):

- With arbitration: full failsafe triggers only when **all** compiled sources
  are inactive. If WiFi commands stop but RC is alive, RC takes over — no
  failsafe.
- WiFi-only build: when the WiFi buffer goes inactive (>500 ms silence),
  `failSafe()` applies `FAILSAFE_ROLL/PITCH/THROTTLE/YAW/AUX1/AUX2` (throttle to
  minimum, level attitude) and blinks the LED rapidly.
- A dropped WiFi association (`WiFi.status() != WL_CONNECTED`) stops new
  commands arriving → the 500 ms timeout fires → failsafe. There is no separate
  link-down event; timeout is the single failsafe trigger.

---

## 5. Telemetry schema (outbound POST)

`handleApiClient()` in `api_client.cpp:59-109` builds and POSTs this JSON to
`{API_SERVER_URL}/api/telemetry` every `API_POST_INTERVAL_MS` (default 500 ms):

```json
{
  "drone_id": "AA:BB:CC:DD:A1:B2",
  "armed": false,
  "roll": -1.24,
  "pitch": 0.51,
  "yaw": 178.30,
  "loop_us": 980,
  "motors": { "m1": 0.0, "m2": 0.0, "m3": 0.0, "m4": 0.0 },
  "rssi": -58,
  "heap": 210456,
  "uptime_ms": 84231
}
```

| Field | Type | Source |
|---|---|---|
| `drone_id` | string | `WiFi.macAddress()` — full MAC, set in `setupApiClient()` |
| `armed` | bool | `DisplayData_t.armed` |
| `roll`/`pitch`/`yaw` | number | Madgwick attitude, degrees |
| `loop_us` | int | Core-0 loop µs |
| `motors.m1..m4` | number | Motor outputs 0–1 |
| `rssi` | int | WiFi RSSI dBm |
| `heap` | int | Free heap bytes |
| `uptime_ms` | int | `millis()` |

Notes:
- This is **drone-initiated push** to a central server — distinct from the
  inbound `GET /api/status` poll. Schema is similar but **not identical**:
  outbound has `drone_id` and no `net{}`/`imu{}`/`calibrating` blocks;
  `/api/status` has no `drone_id`.
- POST failures are non-fatal: the FC logs at most once per 10 s and continues.
  No retry/queue — a missed sample is simply dropped.
- `drone_id` is the **full MAC**; the mDNS hostname uses only the **last two
  octets**. A coordinator correlating push telemetry with a polled drone must
  match on MAC, not hostname.

---

## 6. Safety guarantees

What an external swarm coordinator **CAN** do:
- Read full telemetry (poll or WS stream).
- Send 6-channel RC commands (REST or WS) — including throttle and the
  arm-channel pattern. In effect it can fly the drone.
- OTA-flash the drone (see below) — only while disarmed.

What it **CANNOT** do:
- Override or disable failsafe. The 500 ms timeout is enforced firmware-side; a
  coordinator cannot extend or suppress it. Stop sending → drone fails safe.
- Beat RC priority. If a physical RC receiver is connected and arbitration is
  on, RC is the *fallback*, but more importantly the swarm client must keep
  sending faster than 2 Hz or RC reclaims control. The coordinator cannot lock
  out RC.
- Change PID/config/parameters — no such endpoint exists.
- Arm by an explicit call — only by commanding channel values, same as a TX.

OTA gating:
- `handleOTA()` (`ota.cpp`) calls `ArduinoOTA.handle()` **only when
  `!armedFly`**. Firmware cannot be flashed while the drone is armed. This is a
  hard, firmware-side guarantee independent of the swarm API.
- OTA uses the same `floppi-XXXX` hostname pattern; PlatformIO target
  `--upload-port floppi-XXXX.local`.

Cross-core safety:
- Inbound commands cross from Core 1 to Core 0 via the spinlock-protected
  `wifiCmdChannels[]` (`portMUX_TYPE wifiCmdMux`). Telemetry crosses Core 0 → 1
  via the `dataMux`-protected `latestData` snapshot. The 1 kHz flight loop is
  never blocked by network activity.
- All inbound channel values are `constrain()`-clamped to 1000–2000 µs in
  `getCommands()` regardless of source — a malformed POST cannot drive a channel
  out of range.

---

## 7. Versioning

**There is no protocol version field anywhere** — not in `/api/status`, not in
the command schema, not in the outbound telemetry payload, not in any header.
The swarm_api client (`drone.py`) likewise assumes a fixed schema.

GAP / recommendation (do **not** implement here):
- Add an `"api_version"` integer to `/api/status` and the outbound telemetry
  payload (e.g. start at `1`). Cheap: one line in `serializeDisplayData()` and
  one in `handleApiClient()`.
- The swarm_api client should read it and warn/refuse on mismatch.
- Until then, the "Verified … against …@SHA" stamp at the top of this document
  is the de-facto version marker. Any schema change must bump that stamp.

---

## 8. Gaps / open questions

Things a `swarm_api` client author will stumble on:

1. **No auth, no TLS.** Anyone on the LAN can fly or OTA-flash a drone. Acceptable
   only on an isolated/trusted network. If the swarm runs on shared WiFi this is
   a real risk. Decision needed: add a shared token header, or document the
   "isolated SSID only" requirement.
2. **No protocol version** (see §7).
3. **Inbound command-pull (`GET .../api/commands`) is unimplemented.** The URL is
   built in `setupApiClient()` but never called. A swarm design that expects the
   drone to *pull* commands will not work — only the *push*-telemetry and
   *inbound* REST/WS command paths exist.
4. **Silent default substitution.** A partial command JSON (missing `chN`) does
   not error — missing channels snap to defaults (throttle → 1000, others →
   1500/1000). A client that sends `{"ch3":1600}` alone will also command
   roll/pitch/yaw to center and aux to low. Always send all 6.
5. **WS command failures are invisible.** Invalid JSON or non-text frames on `/ws`
   are dropped with no error frame. Clients get no negative ack — only the
   telemetry stream (or its absence) signals state.
6. **Two non-identical telemetry schemas.** `/api/status` vs the outbound
   `/api/telemetry` payload differ (see §5). A coordinator handling both must not
   assume one parser fits both.
7. **Identity mismatch.** `drone_id` (outbound) = full MAC; mDNS hostname = last
   two MAC octets only. Correlate on MAC.
8. **Port 80 hardcoded; mDNS may fail silently.** If `MDNS.begin()` fails the
   server still starts on the IP — clients must have an IP fallback (the
   swarm_api `DroneClient` already does: `base_url` prefers `ip` over mDNS).
9. **Command rate is a hidden contract.** Nothing advertises the 500 ms timeout
   except a boot Serial log. A client polling at 1 Hz will intermittently lose
   control to failsafe/RC. Recommend documenting ">2 Hz required, 5–10 Hz
   advised" in the swarm_api client.
10. **No timestamp/sequence on commands.** No way to detect stale or
    out-of-order command frames; last-write-wins by arrival time only.

**Maintenance recommendation:** keep the "Verified <date> against
web_server.cpp@<SHA>" stamp at the top of this file. Any change to
`web_server.cpp`, `api_client.cpp`, or the `radioComm` WiFi path must re-verify
this document and re-stamp it. The `swarm_api` project should link to this file
as its single source of truth for the wire protocol.
