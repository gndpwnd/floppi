# Phase W5 — GPS Passthrough Landed + Pin-Default Fix (`USE_GPS`)

> Date: 2026-05-21
> Agent: fc-w5-gps-landing@flight_controller:1
> Status: **LANDED** — telemetry-only raw-NMEA GPS passthrough, ESP32 Core-1
> task. Firmware committed in `3f57a6c`; a latent missing-pin-default build
> issue was subsequently fixed in `include/config.h`.
> Companion to `phase_w5_gps_landed_2026-05-20.md` (the original landing report,
> which predates the `api_client.cpp` / contract reconciliation and the
> pin-default fix). Spec: `gps_passthrough_spec_2026-05-20.md` (Flavour A).

---

## 1. Summary

`USE_GPS` is an optional, **telemetry-only, passthrough-only** GPS feed on the
ESP32 / ESP32-S3 flight controller. Raw NMEA bytes are read RX-only on UART1 by
a dedicated Core-1 FreeRTOS task, framed into complete `$…\r\n` sentences, and
the most-recent sentence is published verbatim into a spinlock-guarded snapshot
that the swarm-API serializers read. The FC parses **nothing** beyond sentence
framing and a liveness bit. GPS never touches the Core-0 1 kHz flight loop. The
flag defaults **OFF** and compiles to zero bytes when off.

This report records the state as actually committed (in `3f57a6c`) plus the
follow-up missing-pin-default fix, superseding the pre-reconciliation detail in
the 2026-05-20 report where they differ.

---

## 2. What landed

| Item | Where | Detail |
|---|---|---|
| `Gps` class + Core-1 task | `include/gps.h`, `src/gps.cpp` | RX-only UART NMEA framer (`begin()`/`poll()`), `gpsTask` pinned to Core 1, spinlock-guarded `s_gps_snapshot`. |
| Telemetry-only, passthrough (Flavour A) | `src/gps.cpp` | No fix decoding, no fusion, no waypoints, no RTH, no geo-fence, no nav. Only `$…\r\n` framing + a liveness bit. |
| Snapshot accessors | `src/gps.cpp` | `gpsTelemetryNMEA(out, cap)` returns the latest sentence + its age (ms); `gpsTelemetryOk()` returns the liveness bit (sentence within `GPS_STALE_TIMEOUT_MS`). |
| `/api/telemetry` GPS block (outbound POST) | `src/api_client.cpp` | `handleApiClient()` adds a `"gps": { nmea, age_ms, ok }` object under `#ifdef USE_GPS`, reading the GPS snapshot accessors directly (mirrors the W2 `baro` block). POST buffer raised to 512 B to cover the extra block. |
| `serializeDisplayData()` GPS block | `src/web_server.cpp` | Same `"gps"` object added under `#ifdef USE_GPS`, so it appears on `GET /api/status` and the `/ws` stream (shared serializer). Reads the snapshot accessors — adds no field to `DisplayData_t`. |
| Task spawn | `src/main.cpp` | `startGpsTask()` called from the `#ifdef USE_ESP32` setup block, gated by `#ifdef USE_GPS`, after the WiFi stack and barometer task. |

The 2026-05-21 reconciliation closed the W5/W6 gap: the original W5 report (§7,
§9) deliberately deferred the `api_client.cpp` outbound block and the
contract-doc re-stamp to a merged telemetry workstream. The outbound `gps`
block is now present in `api_client.cpp`, and `swarm_api_contract_2026-05-20.md`
documents the `gps` block on `/api/status`, `/ws`, and `/api/telemetry`.

---

## 3. The missing-pin-default fix

The driver was committed (in `3f57a6c`) **without resolvable `GPS_PIN_RX` /
`GPS_PIN_TX` defaults** in the build's include path. The spec §6 had sketched
those defaults in `pin_definitions_esp32.h`, but that file was outside the W5
write zone and was never edited, so a `-D USE_GPS` build had no value for
`GPS_PIN_RX` / `GPS_PIN_TX` — `Gps::begin()`'s
`GPS_SERIAL.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_PIN_RX, GPS_PIN_TX)` would not
compile.

**Fix (2026-05-21, `include/config.h`):** `#ifndef`-guarded defaults were added
in **two** places in `config.h`:

- the `USE_GPS` feature section (after the BAROMETER section), and
- the GPS PASSTHROUGH PINS block in the PIN OVERRIDES section.

Both blocks are identical and idempotent (the `#ifndef` guards mean the second
is a no-op if the first already defined the symbol):

```c
#ifndef GPS_PIN_RX
    #ifdef USE_ESP32S3
        #define GPS_PIN_RX 16   // ESP32-S3 UART1 RX
    #else
        #define GPS_PIN_RX 4    // ESP32 UART1 RX
    #endif
#endif
#ifndef GPS_PIN_TX
    #define GPS_PIN_TX -1       // unused — passthrough is RX-only
#endif
```

`config.h` is included before the pin-definition headers, so these defaults
resolve for every build while remaining overridable by a build flag or a
future `pin_definitions_esp32.h` entry. `pin_definitions_esp32.h` was **not**
edited, so the W1/W1b GPIO `#error` guards and the `SERVO_PIN_3 → GPIO 13`
reassignment are not regressed.

---

## 4. Pin / UART allocation (as built)

| Symbol | Default | Meaning |
|---|---|---|
| `GPS_UART_NUM` | `1` | UART1 / `Serial1`. UART0 = USB debug; UART2 = SBUS. |
| `GPS_PIN_RX` | `4` (ESP32) / `16` (ESP32-S3) | UART1 RX — the GPS module's TX wire lands here. |
| `GPS_PIN_TX` | `-1` | unused — passthrough is RX-only, claims no TX GPIO. |
| `GPS_UART_BAUD` | `9600` | NEO-6M factory default (M8N/M9N use 38400). |
| `GPS_BUFFER_BYTES` | `256` | per-`poll()` RX-FIFO drain budget. |
| `GPS_STALE_TIMEOUT_MS` | `2000` | liveness window for `gps.ok`. |

- **No SBUS collision.** With `GPS_UART_NUM 1` (default) GPS is on UART1 and
  never touches GPIO 16; SBUS keeps UART2 / GPIO 16 (ESP32) or GPIO 18
  (ESP32-S3) uncontested. GPS + SBUS coexist with no override.
- **GIO 4 is conflict-free** on a current build: the W1b GPIO-conflict fix
  already moved `SERVO_PIN_3` off GPIO 4 to GPIO 13.
- **`#error` guard in `gps.h`** rejects `USE_GPS` sharing UART1 with the serial
  receivers (`USE_IBUS_RECEIVER` / `USE_DSM_RECEIVER` / `USE_SERIAL_COMMANDS`),
  and `GPS_UART_NUM 2` + `USE_SBUS_RECEIVER`.

---

## 5. Build results

ESP32 env `esp32` (board `esp32dev`); ESP32-S3 env `esp32s3`.

| Build | Flash | RAM |
|---|---|---|
| `esp32`, `USE_GPS` **off** (default) | 43.6% | 10.9% |
| `esp32`, `USE_GPS` **on** | 43.7% | 11.0% |
| `esp32s3`, `USE_GPS` **on** | 17.0% | 9.7% |

**Feature cost (esp32):** ~**+1188 B flash / +272 B RAM** when `USE_GPS` is
enabled. The Core-1 task's 3072 B stack is allocated from the FreeRTOS heap at
`startGpsTask()` time and is not counted in the static RAM figure. With the
flag off, every line of GPS code is `#ifdef USE_GPS`-gated — zero overhead.

---

## 6. What this explicitly does NOT do

Per `gps_passthrough_spec_2026-05-20.md` §9 and `scope.md`, `USE_GPS` is
**passthrough only**: no GPS-derived setpoints reach the flight loop; no NMEA
parsing beyond `$…\r\n` framing + a liveness bit; no return-to-home, waypoints,
geo-fence, navigation, position-hold, GPS-time sync, or NMEA SD logging; no
flight-loop coupling (worst case is a stale `gps` telemetry field); no Teensy
support (ESP32 / ESP32-S3 only). Anything beyond raw passthrough requires
re-evaluating `scope.md` first.

> **SECURITY:** raw NMEA carries absolute latitude/longitude, and the swarm API
> has no auth or TLS. With `USE_GPS` enabled, anyone on the LAN can read the
> drone's position. Run on an isolated/trusted SSID only (tracked scope
> decision — `swarm_api_contract_2026-05-20.md` §8, `scope.md`).

---

*W5 GPS passthrough LANDED. Missing-pin-default fix applied in `include/config.h`
on 2026-05-21. Telemetry-only, passthrough-only, no flight-loop coupling. No git
commit in this doc update.*
