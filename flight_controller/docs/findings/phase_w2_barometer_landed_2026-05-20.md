# Phase W2 — Barometer Integration Landed (`USE_BAROMETER`)

> Date: 2026-05-20
> Agent: fc-w2-barometer@flight_controller:1
> Status: LANDED — telemetry-only barometer, ESP32 Core-1 task. No git commit.
> Implements Session-3 Workstream W2 per `barometer_integration_spec_2026-05-20.md`
> AS AMENDED by `fc_core1_budget_2026-05-20.md` (the task-vs-loop-slice override).

---

## 1. Summary

`USE_BAROMETER` adds an optional, **telemetry-only** barometric sensor to the
ESP32 flight controller. Pressure, temperature and a derived relative altitude
are polled by a **dedicated Core-1 FreeRTOS task** and published into a
spinlock-guarded snapshot that the swarm-API serializer reads. The barometer
**never** touches the Core-0 1 kHz flight loop. The flag defaults **OFF** and,
when off, compiles to zero bytes.

---

## 2. Files created / edited

| File | Status | Lines |
|---|---|---|
| `include/barometer.h` | NEW | full file — `Barometer` class + Core-1 task API + telemetry accessors |
| `src/barometer.cpp` | NEW | full file — BMP280 driver + `baroTask` + snapshot |
| `include/config.h` | EDIT | new "BAROMETER" section inserted before "RECEIVER / COMMAND SOURCE SELECTION" (~52 lines) |
| `src/main.cpp` | EDIT | `#include "barometer.h"` (after the OTA include); `startBarometerTask()` call in the ESP32 setup block (after `setupOTA()`) |
| `src/web_server.cpp` | EDIT | `#include "barometer.h"` guard; `"baro"` JSON block added to `serializeDisplayData()` before the `heap`/`uptime_ms` lines |
| `docs/findings/phase_w2_barometer_landed_2026-05-20.md` | NEW | this report |

All edits are within the assigned W2 write zone. No other files were touched.

---

## 3. Driver design

`Barometer` (in `barometer.h`/`.cpp`) is a small, self-contained I2C driver:

- `begin()` — non-blocking. Reads the chip ID, soft-resets, loads the 24-byte
  factory trim block, configures oversampling (temp x2, pressure x16, IIR
  coeff 4, normal mode) and returns `true` only if a BMP280/BME280 ACKed.
- `read()` — burst-reads the 6 pressure+temperature bytes, applies the Bosch
  integer compensation formulas (64-bit pressure path), converts pressure to
  **relative altitude** via the international barometric formula referenced to
  a configurable **sea-level pressure**, and applies a `BARO_LPF` PT1 filter
  (same convention as `B_ACCEL`/`B_GYRO`).
- Accessors: `pressurePa()`, `temperatureC()`, `altitudeM()`, `isPresent()`.
- `setSeaLevelPressure()` / `seaLevelPressure()` — the calibration reference
  hook the future `'b'` routine (W4) writes through; defaults to
  `BARO_SEA_LEVEL_PA`.

No external library is vendored — the driver is ~200 lines and self-contained,
which keeps the change inside the W2 write zone (`lib_esp32/` is not in scope).

### Sensor chosen — BMP280 (why)

`BAROMETER_BMP280` is the implemented default, per the baro spec §2
recommendation: cheapest (~$1-3), most ubiquitous breakout, 3.3 V, I2C, and
more than adequate for a telemetry-grade vertical readout. `BAROMETER_BMP388`
and `BAROMETER_MS5611` are accepted by the config selector but currently
degrade gracefully — `begin()` returns `false` and telemetry reports
`baro.ok = false` — rather than breaking the build; full drivers for those
parts are a documented follow-up.

---

## 4. I2C bus decision — `Wire` (primary bus, GPIO 21/22)

**The barometer runs on the primary `Wire` bus, shared with the MPU6050 IMU.**
This is a deliberate, documented choice and the resolution of contradiction
**C-1**.

- The IMU sits at I2C `0x68`; the barometer at `0x76`/`0x77` — **no address
  clash**, the two devices coexist on one bus.
- **How it avoids C-1:** the baro spec §4 routed the baro onto `Wire1`, whose
  ESP32 default pins are GPIO 25/26 — which `session3_readiness` finding C-1
  identified as **`MOTOR_PIN_1`/`MOTOR_PIN_2`**. Any `Wire1` baro therefore
  electrically collides with the motor LEDC outputs on a 4-motor airframe. By
  staying on `Wire` (GPIO 21/22), the barometer **touches no motor pin at
  all** — C-1 is avoided structurally, not merely worked around with a pin
  override. No `BARO_SDA/SCL_PIN` symbols are introduced, so the C-1 GPIO
  25/26 collision can never be re-introduced through this feature.
- **Core-0 contention is bounded and safe.** The baro spec §4 cautioned
  against `Wire` because the Core-0 1 kHz loop reads the IMU on it. The
  mitigation: the ESP32 Arduino I2C HAL serialises every bus transaction on a
  per-bus mutex, so a Core-1 baro read and a Core-0 IMU read cannot corrupt
  each other — the baro simply waits a few ms if it collides with an IMU tick.
  The baro is low-rate (20 Hz) and on a non-real-time task, so an occasional
  few-ms wait is invisible; the IMU read is never starved because the baro
  task is priority 1 (below Core-0's priority-3 `FlightCtrl`, and a different
  core entirely). This is a telemetry-only sensor — bounded jitter on its own
  cadence is acceptable; corrupting an IMU read is not, and the HAL mutex
  prevents it.

This is a **deviation from baro spec §4** (which mandated `Wire1`). It is taken
deliberately: the spec's `Wire1` choice was made before C-1 was discovered, and
C-1 makes `Wire1` the worse option. The task brief explicitly directed
preferring the primary `Wire` bus for exactly this reason.

---

## 5. Core-1 task parameters

Per `fc_core1_budget_2026-05-20.md` §7 (W2 recommendations):

```
xTaskCreatePinnedToCore(baroTask, "Baro", 3072, NULL, 1, &handle, 1)
```

| Parameter | Value | Rationale |
|---|---|---|
| Core | 1 | Never Core 0 — the flight loop is untouched. |
| Priority | 1 | Equal to the Arduino `loopTask`; cannot starve the web server. |
| Stack | 3072 B | Budget-recommended; shallow task body, conservative margin. |
| Period | `1000 / BARO_SAMPLE_RATE_HZ` ms (50 ms @ 20 Hz default) | `vTaskDelay`-paced — a clean cadence **immune to the blocking 2 Hz HTTP POST**. |

The task runs `begin()` itself (so a slow/absent sensor never delays
`setup()`), then loops `read()` → publish-to-snapshot → `vTaskDelay`. It is
spawned from the `#ifdef USE_ESP32` setup block in `main.cpp`, after the WiFi
stack is up, gated by `#ifdef USE_BAROMETER`.

### Cross-core / cross-task handoff

The task publishes each reading into a `portMUX_TYPE`-guarded snapshot
(`s_baro_snapshot`) — the same spinlock pattern as `web_server.cpp`'s
`dataMux`/`latestData`. The serializer reads it through
`baroTelemetryOk/PressurePa/TemperatureC/AltitudeM()` accessors, each of which
copies one value under the spinlock. The baro task does **not** write
`DisplayData_t` directly.

---

## 6. Config flags (`config.h`)

New "BAROMETER" section, all `#ifdef USE_BAROMETER`-gated:

| Flag | Default | Purpose |
|---|---|---|
| `USE_BAROMETER` | **undefined (OFF)** | master feature flag |
| `BAROMETER_BMP280` / `_BMP388` / `_MS5611` | `BMP280` | one-of-N sensor selector (`BAROMETER_TYPE`) |
| `BARO_I2C_ADDRESS` | `0x76` | sensor I2C address |
| `BARO_SEA_LEVEL_PA` | `101325.0f` | sea-level reference pressure (ISA placeholder) |
| `BARO_SAMPLE_RATE_HZ` | `20` | Core-1 task poll rate |
| `BARO_LPF` | `0.20f` | output PT1 filter coefficient |

No `BARO_SDA_PIN`/`BARO_SCL_PIN` symbols are added — the barometer uses the
existing `Wire` bus and its `IMU_SDA_PIN`/`IMU_SCL_PIN`, so there are no new
pin defaults that could collide. No `CALIBRATED_BAROMETER` marker yet — that
belongs to the W4 calibration workstream.

---

## 7. Serialization field added

`serializeDisplayData()` in `web_server.cpp` gains a `"baro"` object, gated by
`#ifdef USE_BAROMETER`:

```json
"baro": { "ok": true, "pressure_pa": 101180.4, "altitude_m": 12.07, "temp_c": 23.41 }
```

It appears on `GET /api/status` and the `/ws` stream (both share the
serializer). **Per contradiction C-3, the edit is self-contained:** it reads
the barometer module's snapshot accessors directly and adds **no field to
`DisplayData_t`** — the shared telemetry struct (`display_data.h`) is not in
the W2 write zone and is untouched. The outbound `/api/telemetry` POST
(`api_client.cpp`) is **not** modified — `api_client.cpp` is outside the W2
write zone; extending that payload is left to the merged baro/gps telemetry
workstream.

**Schema-change flag:** adding `"baro"` to `/api/status` + `/ws` is a swarm-API
schema change. Per `swarm_api_contract_2026-05-20.md` §7, the contract doc's
"Verified … @SHA" stamp should be re-stamped. A code comment in
`serializeDisplayData()` flags this for the merged telemetry workstream rather
than relying on it silently.

---

## 8. Build results

ESP32 env discovered from `platformio.ini`: **`esp32`** (board `esp32dev`).
Both builds: `timeout 360 pio run -e esp32 --jobs 1`.

| Build | Result | Flash | RAM |
|---|---|---|---|
| `USE_BAROMETER` **undefined** (default) | SUCCESS | 571025 B (43.6%) | 35636 B (10.9%) |
| `USE_BAROMETER` **defined** (`-D USE_BAROMETER`) | SUCCESS | 576137 B (44.0%) | 35708 B (10.9%) |
| Delta (feature cost) | — | +5112 B | +72 B |

**Zero-overhead-when-off confirmed:** the default build's flash/RAM figures are
the pre-W2 baseline — every line of barometer code is `#ifdef USE_BAROMETER`-
gated, so with the flag off nothing new is compiled. The +5.1 KB flash / +72 B
RAM is the entire feature cost and is only paid when the flag is enabled (the
3072 B task stack is allocated from the FreeRTOS heap at `startBarometerTask()`
time, not counted in the static RAM figure above).

---

## 9. Deviations from the barometer spec

1. **Core-1 dedicated task instead of a `loop()`-slice poll.** Baro spec §7
   WS-2 proposed a `BARO_SAMPLE_RATE_HZ` poll slice inside the ESP32 `loop()`.
   **Overridden** per `fc_core1_budget_2026-05-20.md` §4/§7 (resolving
   contradiction C-2): the `loop()` slice inherits the blocking 2 Hz HTTP
   POST's head-of-line stall, so `BARO_SAMPLE_RATE_HZ` would be an average not
   a cadence. A dedicated FreeRTOS task gives a POST-immune cadence and matches
   the GPS spec's structurally identical choice. This is a **deliberate,
   sanctioned deviation** — the task brief mandates it.

2. **I2C bus: `Wire` instead of `Wire1`.** Baro spec §4 mandated `Wire1`.
   Overridden because `session3_readiness` finding **C-1** showed `Wire1`'s
   GPIO 25/26 defaults are `MOTOR_PIN_1`/`MOTOR_PIN_2`. Using `Wire`
   (GPIO 21/22, shared with the IMU at a non-clashing address) avoids C-1
   entirely. Core-0 contention is bounded by the ESP32 I2C HAL's per-bus mutex.
   See §4. The OLED-bus row of baro spec §4 was already known wrong for ESP32
   (C-4) and was not relied upon.

3. **No `api_client.cpp` / `display_data.h` edit.** Baro spec §7 WS-2 listed
   both. Both are outside the W2 write zone (C-3: serialization edits must be
   self-contained). The `"baro"` block is added only to the shared
   `serializeDisplayData()` and reads the baro module's own snapshot — the
   outbound POST and the `DisplayData_t` struct are deliberately left for the
   merged telemetry workstream.

4. **BMP388/MS5611 drivers not implemented.** Only the BMP280 default driver
   is implemented; the other two selectors compile and degrade gracefully
   (`begin()` → false). Full drivers are a follow-up — out of W2 scope.

5. **No `'b'` calibration routine.** Baro spec §7 WS-3 (the `'b'` serial
   command + `CALIBRATED_BAROMETER` marker) is a separate workstream (W4) and
   is not part of W2. The driver exposes `setSeaLevelPressure()` as the hook
   W4 will use.

---

*W2 complete. Telemetry-only, no flight-loop coupling. `USE_BAROMETER` default
OFF, zero overhead when off. No GPIO 25/26 used. Both builds pass. No git
commit.*
