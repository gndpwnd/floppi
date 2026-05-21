# Phase W5 — GPS Passthrough Landed (`USE_GPS`)

> Date: 2026-05-20
> Agent: fc-w5-gps@flight_controller:1
> Status: LANDED — telemetry-only raw-NMEA GPS passthrough, ESP32 Core-1 task.
> No git commit.
> Implements Session-3 Workstream W5 per `gps_passthrough_spec_2026-05-20.md`
> (Flavour A — raw NMEA passthrough) with the explicit Core-1 task + 3072 B
> stack from `fc_core1_budget_2026-05-20.md` §7. Mirrors the W2 barometer
> pattern (`phase_w2_barometer_landed_2026-05-20.md`).

---

## 1. Summary

`USE_GPS` adds an optional, **telemetry-only, passthrough-only** GPS feed to the
ESP32 flight controller. Raw NMEA bytes are read RX-only on UART1 by a
**dedicated Core-1 FreeRTOS task**, framed into complete `$…\r\n` sentences, and
the most-recent sentence is published verbatim into a spinlock-guarded snapshot
that the swarm-API serializer reads. The FC parses **nothing** beyond sentence
framing and a liveness bit. The GPS **never** touches the Core-0 1 kHz flight
loop. The flag defaults **OFF** and, when off, compiles to zero bytes.

---

## 2. Files created / edited

| File | Status | Lines |
|---|---|---|
| `include/gps.h` | NEW | full file — `Gps` class + UART `#error` guard + Core-1 task API + telemetry accessors |
| `src/gps.cpp` | NEW | full file — RX-only NMEA framer + `gpsTask` + snapshot + accessors |
| `include/config.h` | EDIT | new "GPS PASSTHROUGH" section after the BAROMETER section (~58 lines); `GPS_PIN_RX`/`GPS_PIN_TX` block in PIN OVERRIDES (~27 lines) |
| `src/main.cpp` | EDIT | `#include "gps.h"` guard (after the barometer include); `startGpsTask()` call in the ESP32 setup block (after the barometer task) |
| `src/web_server.cpp` | EDIT | `#include "gps.h"` guard; `"gps"` JSON block added to `serializeDisplayData()` after the `"baro"` block |
| `docs/findings/phase_w5_gps_landed_2026-05-20.md` | NEW | this report |

All edits are within the assigned W5 write zone. No other files were touched.

---

## 3. Driver design

`Gps` (in `gps.h`/`.cpp`) is a small, self-contained, RX-only UART NMEA framer:

- `begin()` — non-blocking. Opens the configured UART at `GPS_UART_BAUD` with
  `SERIAL_8N1`, `GPS_PIN_RX`, `GPS_PIN_TX` (-1, no TX GPIO claimed). Returns
  immediately — **no "wait for first sentence" loop** (gps spec §7 item 3:
  cold-start of 1–30 s is normal and must not block setup).
- `poll()` — drains up to `GPS_BUFFER_BYTES` bytes from the UART RX FIFO and
  runs a small state-machine framer: a `$` starts a sentence, `\n` terminates
  it, `\r` is ignored, bytes outside a frame are skipped (line-noise
  tolerance). On a complete framed sentence the latest-sentence buffer is
  updated and `true` is returned. Returns `true` only when a NEW sentence was
  framed this call.
- Accessors: `latestSentence()`, `lastSentenceMs()`, `droppedCount()`.

**Flavour A (raw passthrough)** — the FC decodes nothing. Any NMEA talker
(`$GP…`, `$GN…`, `$GL…`, `$GA…`) is supported automatically because the framer
is talker-agnostic. Parsing is the consumer's job (gps spec §5).

**Buffer overflow — drop-oldest.** If an in-progress accumulation exceeds
`GPS_NMEA_MAX` (83 — the longest valid NMEA frame plus NUL), the accumulator is
discarded and a `droppedCount()` counter increments (gps spec §7 item 5).
Telemetry clients always see a complete sentence, never a fragment.

No external library is vendored — the framer is ~120 lines, self-contained.

---

## 4. UART + RX pin chosen — UART1 (`Serial1`), GPIO 4, RX-only

**UART1 (`Serial1`)** is the GPS UART (`GPS_UART_NUM` default 1). Rationale
(gps spec §3): UART0 is the USB debug console on every build; UART2 is reserved
for SBUS in the shipping default receiver build; UART1 is the "external
flight-computer side" by convention and is free in a SBUS or WiFi-only build.
`GPS_UART_NUM 2` is also supported for builds that want GPS on UART2.

**RX pin: `GPS_PIN_RX` = GPIO 4** (standard ESP32) / GPIO 16 (ESP32-S3) — the
existing UART1-RX default. **Conflict-free justification:** GPIO 4 was
historically also `SERVO_PIN_3`, but the 2026-05-20 W1b GPIO-conflict fix
already reassigned `SERVO_PIN_3` to GPIO 13 (`pin_definitions_esp32.h:113`), so
on a current build GPIO 4 is no longer double-claimed by a servo. The remaining
hazard — UART1 shared with iBUS/DSM/serial-command receivers — is caught at
compile time by an `#error` guard in `gps.h` (see §6). The W1 `#error` guards
and W1b servo reassignment in `pin_definitions_esp32.h` are not regressed:
`pin_definitions_esp32.h` was not edited.

**RX-only wiring.** Passthrough sends nothing to the GPS, so `GPS_PIN_TX` is -1
(Arduino "unused" sentinel) and `Serial1.begin()` claims no TX GPIO.

---

## 5. Core-1 task parameters

Per `fc_core1_budget_2026-05-20.md` §7 (W5 recommendations):

```
xTaskCreatePinnedToCore(gpsTask, "Gps", 3072, NULL, 1, &handle, 1)
```

| Parameter | Value | Rationale |
|---|---|---|
| Core | 1 | Never Core 0 — the flight loop is untouched. |
| Priority | 1 | Equal to the Arduino `loopTask`; cannot starve the web server. |
| Stack | 3072 B | Budget-recommended; shallow byte-shuffler, conservative margin. |
| Loop | `poll()` then `vTaskDelay(pdMS_TO_TICKS(10))` | A separate schedulable entity — immune to the blocking 2 Hz HTTP POST; the ESP-IDF UART driver buffers RX bytes from an ISR so no byte loss even during a 2 s POST stall. |

The task runs `begin()` itself (so a slow/absent module never delays
`setup()`), then loops `poll()` → publish-to-snapshot → `vTaskDelay`. It is
spawned from the `#ifdef USE_ESP32` setup block in `main.cpp`, after the WiFi
stack and the barometer task, gated by `#ifdef USE_GPS`.

### Cross-core / cross-task handoff

The task publishes each framed sentence into a `portMUX_TYPE`-guarded snapshot
(`s_gps_snapshot`) — the same spinlock pattern as W2's `s_baro_snapshot` and
`web_server.cpp`'s `dataMux`/`latestData`. The serializer reads it through
`gpsTelemetryNMEA()` / `gpsTelemetryOk()`, which copy under the spinlock. The
GPS task does **not** write `DisplayData_t` directly. The snapshot is consumed
only by Core-1 readers; there is no spinlock contention with Core 0.

---

## 6. Config flags + compile-time guard (`config.h` / `gps.h`)

New "GPS PASSTHROUGH" section in `config.h`, all `#ifdef USE_GPS`-gated:

| Flag | Default | Purpose |
|---|---|---|
| `USE_GPS` | **undefined (OFF)** | master feature flag |
| `GPS_UART_NUM` | `1` | UART selection (1 = `Serial1`, 2 = `Serial2`) |
| `GPS_UART_BAUD` | `9600` | GPS module baud (NEO-6M default; M8N/M9N use 38400) |
| `GPS_BUFFER_BYTES` | `256` | per-poll byte budget drained from the RX FIFO |
| `GPS_STALE_TIMEOUT_MS` | `2000` | liveness window for `gps.ok` |
| `GPS_PIN_RX` | `4` (ESP32) / `16` (S3) | UART1 RX GPIO — in the PIN OVERRIDES block |
| `GPS_PIN_TX` | `-1` | unused — passthrough is RX-only |

`GPS_PIN_RX`/`GPS_PIN_TX` are placed in `config.h`'s PIN OVERRIDES block (not
`pin_definitions_esp32.h`, which is outside the W5 write zone) — see §9
deviation 1. They are `#ifndef`-guarded, and `config.h` is included before
`pin_definitions.h`, so the values are still overridable.

**UART-collision `#error` guard** lives in `gps.h` (after `config.h` is
included, so `GPS_UART_NUM` and the `USE_*RECEIVER` flags are defined):
`USE_GPS` + `USE_IBUS_RECEIVER`/`USE_DSM_RECEIVER`/`USE_SERIAL_COMMANDS` with
`GPS_UART_NUM 1` fails to compile (all share UART1); `GPS_UART_NUM 2` +
`USE_SBUS_RECEIVER` also fails (both share UART2). This mirrors the W1 GPIO
`#error`-guard pattern and was placed in `gps.h` per gps spec §8 WS-A.

No calibration marker is added — a GPS module has no calibration values
(gps spec §6).

---

## 7. Serialization field added

`serializeDisplayData()` in `web_server.cpp` gains a `"gps"` object, gated by
`#ifdef USE_GPS`, placed right after the `"baro"` block:

```json
"gps": { "nmea": "$GNGGA,123519.00,4807.0380,N,...*47", "age_ms": 230, "ok": true }
```

- `gps.nmea` — most-recent complete NMEA sentence verbatim (string, ≤82 chars).
- `gps.age_ms` — milliseconds since that sentence was framed.
- `gps.ok` — liveness bit: a sentence arrived within `GPS_STALE_TIMEOUT_MS`.
  `false` means GPS missing / unpowered / no link. **This is the only
  "parsing" the firmware does — a liveness bit, not a fix-quality bit.**

It appears on `GET /api/status` and the `/ws` stream (both share the
serializer). **Per contradiction C-3, the edit is self-contained:** it reads
the GPS module's snapshot accessors directly and adds **no field to
`DisplayData_t`**. The outbound `/api/telemetry` POST (`api_client.cpp`) and the
swarm-API contract doc are **not** modified — both are outside the W5 write
zone (C-3); extending them is left to the merged telemetry workstream. A code
comment in `serializeDisplayData()` flags the schema change for that workstream.

---

## 8. Build results

ESP32 env discovered from `platformio.ini`: **`esp32`** (board `esp32dev`).
Both builds: `timeout 360 pio run -e esp32 --jobs 1`.

| Build | Result | Flash | RAM |
|---|---|---|---|
| `USE_GPS` **undefined** (default) | SUCCESS | 571025 B (43.6%) | 35636 B (10.9%) |
| `USE_GPS` **defined** (`-D USE_GPS`) | SUCCESS | 572205 B (43.7%) | 35908 B (11.0%) |
| Delta (feature cost) | — | +1180 B | +272 B |

**Zero-overhead-when-off confirmed:** the default build's 571025 B / 35636 B
**exactly matches the post-W2 `USE_BAROMETER`-off baseline** stated in the task
brief. Every line of GPS code is `#ifdef USE_GPS`-gated, so with the flag off
nothing new is compiled. The +1180 B flash / +272 B RAM is the entire feature
cost and is only paid when the flag is enabled (the 3072 B task stack is
allocated from the FreeRTOS heap at `startGpsTask()` time, not counted in the
static RAM figure above).

---

## 9. Deviations from the GPS spec

1. **`GPS_PIN_RX`/`GPS_PIN_TX` defaults live in `config.h`, not
   `pin_definitions_esp32.h`.** The spec §6 sketched them in
   `pin_definitions_esp32.h`. That file is **outside the W5 write zone**, so
   the `#ifndef`-guarded defaults were placed in `config.h`'s PIN OVERRIDES
   section instead. `config.h` is included before `pin_definitions.h`, so the
   defaults still resolve and remain overridable. Functionally identical;
   purely a write-zone-driven placement change.

2. **No `api_client.cpp` / `display_data.h` / contract-doc edit.** The spec §8
   WS-B listed all three. All are outside the W5 write zone (C-3: the
   serialization edit must be self-contained). The `"gps"` block is added only
   to the shared `serializeDisplayData()` and reads the GPS module's own
   snapshot — the outbound POST, the `DisplayData_t` struct, and the swarm-API
   contract doc's "Verified" re-stamp are deliberately left for the merged
   baro/gps telemetry workstream (W6). Mirrors W2 deviation 3 exactly.

3. **Per-poll byte budget instead of a standalone ring-buffer struct.** The
   spec §4 described a `GPS_BUFFER_BYTES` ring buffer. The implementation uses
   `GPS_BUFFER_BYTES` as the per-`poll()` drain budget plus a single
   in-progress sentence accumulator — functionally the ring buffer's purpose
   (bound the work per wake, never block, drop-oldest on overflow) without a
   second copy. The ESP-IDF UART driver already provides the interrupt-fed RX
   FIFO the spec's ring buffer would duplicate. Drop-oldest is preserved
   (`droppedCount()`); telemetry clients still only ever see complete
   sentences. Minor, in-spirit simplification.

---

## 10. What this explicitly does NOT do

Per gps spec §9 and `scope.md`, `USE_GPS` is **passthrough only**:

- **No GPS-derived setpoints reach the flight loop.** The Core-0 loop is
  `imu → madgwick → pid → mixer → motors`, untouched.
- **No NMEA parsing** beyond `$…\r\n` framing and a liveness bit. No lat/lon/
  alt/sats/fix decoding in firmware.
- **No return-to-home, no waypoints, no geo-fence, no navigation, no
  position-hold, no GPS-time sync, no NMEA SD logging.**
- **No flight-loop coupling** — no GPS failure mode can stall the flight loop,
  arm a failsafe, or drop a motor PWM line. The worst case is a stale `gps`
  telemetry field.
- **No Teensy support** — ESP32 / ESP32-S3 only (passthrough exists to serve
  the WiFi telemetry surface).

Anything beyond raw passthrough is deferred and requires re-evaluating
`scope.md` first.

---

*W5 complete. Telemetry-only, passthrough-only, no flight-loop coupling.
`USE_GPS` default OFF, zero overhead when off (default build = post-W2
baseline). UART1 RX-only on GPIO 4, conflict-free. Both builds pass. No git
commit.*
