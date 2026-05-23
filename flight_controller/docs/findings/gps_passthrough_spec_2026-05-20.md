# GPS Passthrough Integration Spec (`USE_GPS`)

> Date: 2026-05-20 (spec) · status updated 2026-05-21
> Agent: fc-gps-passthrough-spec@flight_controller:1
> Status: **LANDED** — Flavour-A raw-NMEA passthrough shipped in firmware
> (committed in `3f57a6c`). This spec is now a historical design record; the
> as-built detail lives in `phase_w5_gps_landed_2026-05-20.md` and the
> follow-up `phase_w5_gps_landed_2026-05-21.md` (missing-pin-default fix).
> Scaffolding source: `future_session_scaffolding_2026-05-20.md` §3.4
>
> **As-built notes (do not regress when editing):**
>
> - `GPS_PIN_RX` / `GPS_PIN_TX` defaults live in **`include/config.h`** (not
>   `pin_definitions_esp32.h` as §6 sketched): `GPS_PIN_RX` = GPIO 4 RX on the
>   standard ESP32, GPIO 16 RX on the ESP32-S3; `GPS_PIN_TX` = -1 (RX-only —
>   no TX GPIO claimed). Both are `#ifndef`-guarded so a build flag still wins.
>   A latent missing-pin-default build issue was fixed in `config.h` on
>   2026-05-21 (see `phase_w5_gps_landed_2026-05-21.md`).
> - GPS runs on **UART1 / `Serial1`** (`GPS_UART_NUM` default 1), so it never
>   collides with SBUS, which is on **UART2 / `Serial2`** in the default
>   receiver build. The GPS + SBUS combination coexists with no override.
> - A compile-time **`#error` guard in `gps.h`** prevents `USE_GPS` from sharing
>   UART1 with the serial receivers — `USE_IBUS_RECEIVER` / `USE_DSM_RECEIVER` /
>   `USE_SERIAL_COMMANDS` (and, for `GPS_UART_NUM 2`, `USE_SBUS_RECEIVER`).

**Cross-references (do not duplicate):**
- `future_session_scaffolding_2026-05-20.md` §3.4 — contract this spec fulfills.
- `barometer_integration_spec_2026-05-20.md` — sibling Core-1 sensor spec; same
  "telemetry-edge, never in the flight loop" pattern (its §3, §4 cover the
  Core-1 placement rationale not re-justified here).
- `swarm_api_contract_2026-05-20.md` — the WiFi telemetry surface a GPS field
  would extend (§5 sketches the extension; do not modify that doc).
- `esp32_gpio_conflict_resolution_2026-05-20.md` — ESP32 GPIO double-claims
  this spec must not aggravate.
- `scope.md` "Out of Scope": **"GPS, barometer, magnetometer — flight computer
  territory"** and **"Autonomous navigation or waypoint following — flight
  computer territory"**.

---

## 1. Framing and the hard scope boundary

`scope.md` is unambiguous: GPS is **out of scope**. So is "autonomous
navigation or waypoint following." The FC is a stabilizer — "The FC
stabilizes. Period." (`scope.md` Critical Notes).

This spec scopes **the only GPS feature consistent with that boundary**: a
**passthrough**. UART bytes from a GPS module arrive at ESP32 Core 1, get
buffered, and are exposed verbatim through the existing swarm API for an
**external flight computer** to consume. Nothing GPS-derived reaches the 1 kHz
Core-0 control loop. No parsing, no fusion, no setpoint generation, no
flight-loop coupling.

This is consistent with the architecture vision:

> "Future sensors added modularly: Additional sensors (barometer, GPS, lidar)
> would each get their own `USE_*` flag and run on Core 1 (ESP32) or be handled
> by an external flight computer. The base flight loop on Core 0 is never
> affected." — `scope.md` Hardware Architecture Vision

**What this spec explicitly DOES NOT do** (restated in §9):
- No waypoints, missions, or path following.
- No return-to-home (RTH) on link loss or low battery.
- No geo-fence enforcement.
- No coupling to the flight loop (no GPS-derived setpoints, no fusion).
- No NMEA parsing beyond an optional fix-quality / satellite-count extraction
  (deferred — see §5 Flavour B).

The scaffolding §3.4 verdict is **CONDITIONAL**: passthrough = OK, "active GPS
use" = OVER-COMPLICATES. This spec scopes the passthrough half only; the
active half stays deferred, and reopening it requires re-evaluating
`scope.md` first.

---

## 2. Sensor candidates

Typical UART GPS modules in the hobbyist range, all 3.3 V tolerant on the data
line (the ESP32 only reads NMEA — see §3 on RX-only wiring), all output
standards-compliant NMEA-0183 sentences (`$GPGGA`, `$GPRMC`, etc.) at boot
without configuration.

| Module       | Typ. cost | Fix rate | Constellations           | Sensitivity (tracking) | Notes |
|--------------|-----------|----------|--------------------------|------------------------|-------|
| u-blox NEO-6M  | ~$5     | 1 Hz default (up to 5 Hz w/ u-center) | GPS only                | ~-161 dBm | Legacy, ubiquitous, cheapest. Default 9600 baud. Acceptable for "where am I?" telemetry; sluggish for any high-rate logging. |
| u-blox NEO-M8N | ~$15    | 5–10 Hz                                | GPS + GLONASS (+ Galileo/BeiDou by config) | ~-167 dBm | Hobbyist default for drones. Default 38400 baud once configured (factory often 9600). Multi-GNSS lock noticeably faster than NEO-6M. |
| u-blox NEO-M9N | ~$25    | 10 Hz                                  | Multi-constellation (all 4)              | ~-167 dBm | Newer, lower power, better cold-start. Default 38400 baud. Overkill for telemetry-only; specify only if also using the module on a downstream flight computer. |

**Antenna:** all three need an active ceramic-patch or helical antenna for
usable cold-start; wiring details belong in a wiring doc, not here.

**Recommendation:** **NEO-M8N** as documented default — cheap, ubiquitous,
GPS+GLONASS gives much faster urban fixes than NEO-6M. NEO-6M documented as
budget option; NEO-M9N as upgrade. The driver is sensor-agnostic — it just
reads NMEA.

---

## 3. UART allocation — finding a free port

The ESP32 has three hardware UARTs:

| UART     | Typical role on this FC                                                                 | Source |
|----------|-----------------------------------------------------------------------------------------|--------|
| UART0    | USB-serial debug / `Serial` console — used by every build.                              | `main.cpp` `Serial.begin(SERIAL_BAUD)` |
| UART1 (`Serial1`) | iBUS / DSM / external serial-commands RX, depending on receiver flag.          | `radioComm_rc.cpp:80, 82` (`IBUS_SERIAL_PORT.begin(... IBUS_RX_PIN, IBUS_TX_PIN)`); `pin_definitions_esp32.h:165–199` (`IBUS_RX_PIN`, `DSM_RX_PIN`, `SERIAL_CMD_RX_PIN` all default to GPIO 4 — Serial1 RX). |
| UART2 (`Serial2`) | SBUS RX — used whenever `USE_SBUS_RECEIVER` is set (the shipping default).     | `radioComm_rc.cpp:27` ("Port: Serial2 (ESP32)"); `pin_definitions_esp32.h:151` (`SBUS_RX_PIN = 16`). |

SBUS builds occupy UART2; iBUS/DSM/serial-cmd builds occupy UART1. The
**WiFi-only build** (no compiled receiver — `config.h:118–120`) leaves both
UART1 and UART2 free, and is the most likely build for a swarm operator.

**Recommended allocation: GPS on UART1 (`Serial1`), RX-only.**

Rationale:
- UART1 is the "external flight-computer side" by current convention
  (iBUS / serial-cmd / DSM all use it); a GPS for the external flight
  computer fits the same role.
- SBUS lives on UART2 in the default build; reserving UART2 for the RC
  primary keeps receiver wiring stable.
- In a SBUS build, UART1 is unused — GPS slots in.
- In an iBUS / DSM / serial-cmd build, the operator picks GPS *or* the serial
  receiver — not both. A compile-time `#error` must fire if `USE_GPS` is
  combined with `USE_IBUS_RECEIVER` / `USE_DSM_RECEIVER` /
  `USE_SERIAL_COMMANDS`, mirroring the `#error`-guard pattern in
  `esp32_gpio_conflict_resolution_2026-05-20.md` §4.

**RX-only wiring.** Passthrough sends nothing to the GPS — no `$PUBX`, no rate
changes; factory NMEA output is fine. Only the **GPS TX → ESP32 RX** wire is
needed. `GPS_PIN_TX` is `-1` (Arduino "unused" sentinel) so
`Serial1.begin(baud, SERIAL_8N1, rx, tx)` does not claim a GPIO.

**Recommended GPIO: GPS_PIN_RX = GPIO 4** (the UART1 RX default —
`IBUS_RX_PIN`/`DSM_RX_PIN`/`SERIAL_CMD_RX_PIN` in
`pin_definitions_esp32.h:165–199`). GPIO 4 is also the `SERVO_PIN_3` default
(Conflict B in `esp32_gpio_conflict_resolution_2026-05-20.md` §1). The same
mitigation applies one-to-one:

- 0–2-servo airframes (most quads): GPIO 4 is genuinely free; the GPS-vs-
  receiver `#error` plus the GPIO-spec's existing guard suffice.
- 3+ servo airframes: apply the `SERVO_PIN_3 → GPIO 13` override from
  `esp32_gpio_conflict_resolution_2026-05-20.md` §4.

**Net: GPS on UART1 / GPIO 4 introduces no new conflict** — it inherits
Conflict B with the same already-documented mitigation. ESP32-S3 equivalent:
UART1 RX = GPIO 16 (`pin_definitions_esp32.h:165, 189`).

---

## 4. Where it runs — Core 1, period

The same Core-1 placement rationale developed in
`barometer_integration_spec_2026-05-20.md` §3 applies verbatim: the 1 kHz
Core-0 flight loop must never block on a peripheral that exists solely to
serve telemetry. Read that section if details on the dual-core split are
needed; it is not repeated here.

**Where the GPS driver lives:**
- Setup runs from the Core-1 setup branch of `main.cpp` (`src/main.cpp:352–391`,
  the `#ifdef USE_ESP32` block that already houses `setupWiFi()`,
  `setupWebServer()`, `setupApiClient()`, `setupOTA()`). A new `setupGPS()`
  is added there, gated by `#ifdef USE_GPS`.
- A dedicated **FreeRTOS task** is pinned to **Core 1**, low priority (priority
  1 — lower than the Core-0 `FlightCtrl` task at priority 3, and equal to or
  lower than the existing Core-1 work). The task body is a simple loop:

  1. Block on `Serial1.available()` with a short timeout (~10 ms) so the task
     yields when the GPS has nothing to say.
  2. Drain available bytes into a ring buffer of `GPS_BUFFER_BYTES` (default
     256 — enough for the longest single NMEA sentence, ~82 bytes, plus a
     few queued sentences).
  3. Whenever a complete sentence is detected (terminated by `\r\n`), copy
     the framed sentence into a "latest-sentence" snapshot guarded by a
     spinlock (`portMUX_TYPE`), exactly like the existing `dataMux` /
     `latestData` pattern in `web_server.cpp`. The web-server / api-client
     accessors read the snapshot under the same spinlock.

**Why a task rather than a slice of `loop()`:** UART RX is naturally
event-driven and the ESP-IDF UART driver already buffers bytes in an
interrupt-fed FIFO. Polling from the existing Core-1 `loop()` slice would
either over-poll (wasting cycles) or under-poll and drop bytes when WiFi is
busy. A dedicated low-priority task lets the scheduler give it CPU when
bytes arrive and lets it sleep otherwise. `vTaskDelay(pdMS_TO_TICKS(10))`
on idle keeps the task off the CPU.

**Cross-core safety:** the GPS task lives entirely on Core 1. It never writes
to any Core-0-owned structure. The latest-sentence snapshot is consumed only
by the web server, the API client, and (optionally) the OLED — all Core-1
consumers. There is no spinlock contention with Core 0.

**Teensy:** no GPS passthrough on Teensy. The whole point of passthrough is
the WiFi surface; Teensy has no WiFi and the existing scope.md "flight
computer talks to FC over radio or WiFi API" assumption breaks for Teensy.
A Teensy operator who wants GPS feeds the GPS directly into their flight
computer, bypassing the FC. This spec is **ESP32 / ESP32-S3 only.**

---

## 5. Data flow — pick A or B; this spec picks A

Two flavours for how GPS data leaves the FC:

### Flavour A — raw NMEA passthrough (RECOMMENDED)

- The GPS task captures complete NMEA sentences (`$GPGGA,...*HH`,
  `$GPRMC,...*HH`, `$GNGGA,...`, etc.) verbatim.
- The most recent complete sentence is exposed to the swarm API as a single
  string field. Polling clients see whichever sentence was most recently
  framed; streaming WS clients see new sentences as they arrive.
- The FC parses **nothing**. The operator's flight computer (or
  `swarm_api`-side consumer) does all parsing — there are many mature NMEA
  parsers in Python, none in firmware that we'd want to vendor.

Pros: minimal firmware code, zero parser-bug surface, every NMEA sentence
type a future user might want is automatically supported (no `$GLGSV`
extraction routine to write), exactly matches the "bytes in, bytes out"
contract that makes "passthrough" honest.

Cons: clients must parse NMEA themselves. (Trivial; this is the same
contract every GPS module presents.)

### Flavour B — minimal parse (DEFERRED)

- The FC parses `$GPGGA` (fix quality, satellite count, time, lat, lon,
  altitude) into a typed struct and exposes the parsed fields.
- The swarm API then carries JSON-typed `gps.lat`, `gps.lon`, `gps.alt_m`,
  `gps.fix_quality`, `gps.sats` instead of (or in addition to) the raw
  sentence.

Pros: clients get usable numbers without an NMEA parser.

Cons: that is exactly the kind of firmware-side parsing scope.md's "FC
stabilises, period" tries to keep off the controller. The work belongs on
the flight computer.

**Decision: implement Flavour A only.** Flavour B is deferred to a future
session and would only be revisited if the operator concretely needs the FC
to know "we have a fix" for some FC-side decision (e.g. a fix-required LED
indicator on the OLED). Even then, a *minimum* parse — fix-quality bit and
satellite count — is the maximum acceptable; full position decoding stays on
the flight computer.

### Swarm API extension (do NOT modify `swarm_api_contract_2026-05-20.md`)

Sketch only — the implementer of the swarm-API integration owns the actual
schema edit and the "Verified" re-stamp described in that doc.

Proposed addition to `GET /api/status` and the WebSocket `/ws` telemetry
frame (both share `serializeDisplayData()` per
`swarm_api_contract_2026-05-20.md` §3.1):

```json
{
  "...existing fields...": "...",
  "gps": {
    "nmea": "$GNGGA,123519.00,4807.0380,N,01131.0008,E,1,08,0.9,545.4,M,46.9,M,,*47",
    "age_ms": 230,
    "ok": true
  }
}
```

- `gps.nmea` — most-recent complete sentence (string, up to ~82 chars). May be
  any NMEA talker / sentence (`$GP...`, `$GN...`, `$GL...`, `$GA...`); the
  client should not assume `$GPGGA`-only.
- `gps.age_ms` — milliseconds since the snapshot was last refreshed; tells the
  client whether bytes are still arriving.
- `gps.ok` — boolean: bytes have arrived within the last
  `GPS_STALE_TIMEOUT_MS` window. `false` means the GPS module is missing,
  unpowered, or has no UART link. **This is the only piece of "parsing" the
  firmware does** — it is a liveness bit, not a fix-quality bit.

For the outbound `/api/telemetry` POST (`swarm_api_contract_2026-05-20.md`
§5), the same `gps` block is added. Both schemas are extended consistently;
the `swarm_api_contract` "Verified … @SHA" stamp must be re-issued and the §7
"no api_version" gap re-flagged in the same edit.

---

## 6. Config flag layout

New section in `include/config.h`, following the same conventions as the
barometer spec §5 — single feature flag + named constants, all `#ifdef`-gated
for zero cost when disabled. **Shown as a comment-only example, not a code
diff.**

```c
//=============================================================================
// GPS PASSTHROUGH (optional, telemetry-only — ESP32 Core 1, RX-only UART)
//=============================================================================
// PASSTHROUGH ONLY. Raw NMEA bytes are read on Core 1 and exposed via the
// swarm API as a `gps` field. The FC parses nothing. The flight loop never
// reads GPS data. See docs/findings/gps_passthrough_spec_2026-05-20.md.
//
//#define USE_GPS

#ifdef USE_GPS
    // UART selection (1 or 2). UART1 is the default — UART2 is reserved for
    // SBUS in the default receiver build. Do NOT enable USE_GPS together with
    // USE_IBUS_RECEIVER / USE_DSM_RECEIVER / USE_SERIAL_COMMANDS — they share
    // UART1 (a compile-time #error must enforce this).
    #ifndef GPS_UART_NUM
        #define GPS_UART_NUM 1
    #endif

    // GPS module baud rate. NEO-6M factory default: 9600. NEO-M8N / NEO-M9N
    // factory default once configured: 38400.
    #ifndef GPS_UART_BAUD
        #define GPS_UART_BAUD 9600
    #endif

    // Ring buffer for incoming NMEA bytes. One sentence is ~82 bytes max;
    // 256 holds ~3 sentences and one in-progress framing.
    #ifndef GPS_BUFFER_BYTES
        #define GPS_BUFFER_BYTES 256
    #endif

    // Liveness window. If no bytes arrive in this many ms, gps.ok = false.
    #ifndef GPS_STALE_TIMEOUT_MS
        #define GPS_STALE_TIMEOUT_MS 2000
    #endif
#endif
```

`GPS_PIN_RX` (and the unused `GPS_PIN_TX = -1`) are added to the PIN OVERRIDES
block in `config.h` and given `#ifndef`-guarded defaults in
`pin_definitions_esp32.h` next to the existing UART1 pin defaults:

```c
// In pin_definitions_esp32.h, near IBUS_RX_PIN / SERIAL_CMD_RX_PIN:
#ifndef GPS_PIN_RX
    #ifdef USE_ESP32S3
        #define GPS_PIN_RX 16   // S3 UART1 RX
    #else
        #define GPS_PIN_RX 4    // ESP32 UART1 RX (Conflict B — see GPIO spec)
    #endif
#endif
#ifndef GPS_PIN_TX
    #define GPS_PIN_TX -1       // unused; passthrough is RX-only
#endif
```

A new **calibration status marker** is *not* needed — the GPS module has no
calibration values to copy into `config.h`. The `b`/`i`/`m`/`r`/`f`/`e`/`g`
calibration-routine set is unchanged.

---

## 7. Failure modes

A passthrough is mostly bytes-shuffling, but a few specific failure modes
need explicit handling so a misbehaving GPS cannot disturb the Core-1
telemetry stack:

1. **No GPS module wired / unpowered.** No bytes arrive on UART1. The GPS
   task blocks on `Serial1.available()` and yields. The latest-sentence
   snapshot is never refreshed; `gps.age_ms` grows; `gps.ok` is `false` once
   `GPS_STALE_TIMEOUT_MS` has elapsed. Nothing in the FC crashes, nothing in
   the swarm API errors — the field is simply marked stale.

2. **GPS powered but no fix yet (cold start, sat count < 4).** The module
   still emits NMEA sentences — `$GPGGA` with a `fix=0` field, `$GPRMC` with
   status `V` (void) — at its configured rate. Bytes flow; `gps.ok` is
   `true`; `gps.nmea` contains a valid sentence; the *fix* status is encoded
   inside the NMEA text for the flight computer to parse. **The firmware
   does not distinguish "no fix" from "fix" — that is the consumer's job.**

3. **Power-up race.** GPS modules often take 1–30 s before emitting their
   first sentence (cold start). The GPS task is non-blocking — `setupGPS()`
   only calls `Serial1.begin(...)`, then returns immediately. The task
   starts polling and simply waits. `setupGPS()` must not contain any
   "wait for first sentence" loop; doing so would block Core-1 setup and
   could delay `setupWebServer()`. **Hard rule: `setupGPS()` is
   non-blocking; the absence of a fix at boot is normal.**

4. **UART framing error / line noise.** The NMEA framer accepts only
   `$`-prefixed sentences ending in `\r\n` and (optionally) a valid `*HH`
   checksum tail. Malformed bytes between sentences are skipped. The
   passthrough does not propagate corrupted sentences — only complete,
   framed ones. (Optional checksum validation is a one-liner; recommended.)

5. **Buffer overflow on slow downstream.** Telemetry is sampled by polling
   clients at 1–10 Hz; a 10 Hz NEO-M8N producing 5+ sentences per fix means
   the ring buffer can fill faster than it drains. **Policy: drop-oldest.**
   The ring buffer is single-writer (the GPS task) and single-reader (the
   snapshot updater); on overflow, the oldest framed sentence is overwritten
   and a counter increments. Telemetry clients always see the most recent
   complete sentence — they never see fragments.

6. **GPS module hot-unplug / brownout.** Same effective behaviour as #1: bytes
   stop arriving, `gps.ok` flips to `false` after `GPS_STALE_TIMEOUT_MS`.
   No reset, no recovery action, no failsafe. The GPS is not safety-critical
   — its absence cannot fail-safe a stabiliser.

**Most importantly: no GPS failure mode is allowed to affect Core 0.** The
spinlock-guarded snapshot pattern (see §4) ensures the worst case is a stale
or absent `gps` field in `/api/status`. There is no path by which a GPS
fault stalls the flight loop, arms a failsafe, or drops a motor PWM line.

---

## 8. Implementation workstreams

Two clean workstreams. Both are **S** — this is intentionally a small
feature.

### WS-A — GPS UART driver module (S, ~150–250 LOC)
- **New files:** `src/gps.cpp`, `include/gps.h`.
- Provides `setupGPS()`, the FreeRTOS Core-1 task body, the ring buffer, the
  NMEA framer (`$` … `\r\n`, optional `*HH` checksum), and the
  spinlock-guarded `getLatestNMEA(char* out, size_t cap, uint32_t& age_ms,
  bool& ok)` accessor.
- Owns `Serial1.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_PIN_RX, GPS_PIN_TX)`
  (or `Serial2` when `GPS_UART_NUM == 2`).
- Entirely Core-1-safe; no Core-0 entry points; no global flight state
  touched.
- The compile-time `#error` guard against UART1 collisions
  (`USE_GPS` + `USE_IBUS_RECEIVER`/`USE_DSM_RECEIVER`/`USE_SERIAL_COMMANDS`)
  lives here, in `gps.h`, after `pin_definitions_esp32.h` is included.

### WS-B — Swarm-API integration (S, ~50–100 LOC across existing files)
- **Touches:** `include/display_data.h` (add `gps_nmea[83]`, `gps_age_ms`,
  `gps_ok` to `DisplayData_t`); `src/main.cpp` ESP32 `loop()` (populate the
  three fields from `getLatestNMEA(...)` once per Core-1 loop iteration);
  `src/web_server.cpp` (`serializeDisplayData()` — add the `"gps"` block);
  `src/api_client.cpp` (add the same `"gps"` block to the outbound POST).
- **Cross-reference `swarm_api_contract_2026-05-20.md`:** adding a `"gps"`
  block to `/api/status` and `/api/telemetry` is a **schema change** — the
  "Verified … @SHA" stamp at the top of that doc must be re-issued, and the
  doc's §7 "no api_version" gap must be re-flagged in the same edit. Do
  **not** silently extend the schema.
- No new dependencies. No library vendoring. ArduinoJson already handles
  short strings fine.

**Suggested order:** WS-A → WS-B. WS-B trivially depends on
`getLatestNMEA()` from WS-A. Both together are well under one implementation
session; this is by design.

---

## 9. What this spec explicitly does NOT do (re-stated for the implementer)

If a future PR proposes any of the following, it has exceeded the scope of
this spec and the operator must re-evaluate `scope.md` before proceeding:

- **No GPS-derived setpoints reach the flight loop.** Not as a position
  hold, not as a velocity damp, not as a yaw-from-track override. The
  flight loop is `imu → madgwick → pid → mixer → motors`, full stop.
- **No automatic return-to-home (RTH).** Not on link loss, not on low
  battery, not on geo-fence exit. RTH is autopilot territory and belongs
  on the flight computer (and the flight computer already has full
  channel-command authority over the existing `/api/commands` surface, so
  it can implement RTH externally with zero firmware change).
- **No geo-fence enforcement.** The FC never inspects coordinates against
  a fence polygon. If a fence is needed, the flight computer enforces it
  by withholding throttle / commanding land.
- **No NMEA parsing beyond optional fix-quality / sat-count extraction**
  (and even that is deferred to a future session per §5 Flavour B).
- **No GPS-derived altitude.** The barometer (`barometer_integration_spec_2026-05-20.md`)
  is the telemetry altitude surface; GPS altitude is noisier and is the
  flight computer's concern if it wants it.
- **No GPS-time synchronisation of the flight loop.** Loop timing is set
  by `LOOP_FREQUENCY_HZ`; PPS hardware lines from a GPS module are
  ignored.
- **No SD-card logging of NMEA.** Prohibited by `scope.md`; the flight
  computer logs the WiFi telemetry stream instead.
- **No Teensy support** (see §4).

Anything beyond raw passthrough is **deferred to a future session** and
re-evaluating `scope.md` is a prerequisite, not a formality.

---

## 10. OVER-COMPLICATES verdict

Restating `future_session_scaffolding_2026-05-20.md` §3.4: **CONDITIONAL.**
This spec resolves the conditional into two halves:

- **GPS passthrough on Core 1 (Flavour A, this spec): OVER-COMPLICATES — NO.**
  It is exactly what the architecture vision ("future sensors added
  modularly… run on Core 1… base flight loop never affected") pre-authorises.
  The FC stays a stabiliser; the GPS is bytes for the external flight
  computer. The flight loop is untouched. The only firmware-side
  "intelligence" is sentence framing and a liveness bit — not parsing, not
  fusion, not control. **Implement as specified.**

- **Active GPS use (waypoints, RTH, nav, geo-fence, position-hold): OVER-
  COMPLICATES — YES.** This crosses the explicit `scope.md` boundary
  ("Autonomous navigation or waypoint following — flight computer territory")
  and would import autopilot-class complexity (EKF, NMEA parser, mission
  state machine, failsafe-on-fix-loss policy) into a stabilizer firmware
  whose stated philosophy is to *not* be that. **DEFER. Any reopening
  requires re-evaluating `scope.md` first; this is a different project
  conceptually, not a feature addition.**

The implementation session should build `USE_GPS` strictly as the Flavour-A
passthrough described above and stop there. The two-workstream
S+S sizing in §8 is the budget.
