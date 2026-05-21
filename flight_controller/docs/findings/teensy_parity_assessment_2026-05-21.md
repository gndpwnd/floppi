# Teensy Build — Feature-Parity Assessment vs ESP32

> Date: 2026-05-21
> Agent: fc-teensy-recon@flight_controller:1
> Status: RECON / scoping note — read-only. No code edited, no git commit.
> Scope: assess whether the Teensy flight-control build is meaningfully
> "behind" the ESP32 build, and whether parity work should be scheduled.

---

## 1. Summary

**Headline verdict: the Teensy build does NOT need parity work. The ESP32/Teensy
split is correct as-is and intentional.** Every feature the ESP32 has that the
Teensy lacks is a *connectivity / off-board-telemetry* feature that exists
**because the ESP32 has WiFi and a spare CPU core** — neither of which the Teensy
has. None of them is flight-control functionality. The shared flight-control
core (IMU, Madgwick, PID, mixer, motors, radio, calibration, OLED) is **already
at full parity** — it is literally the same source compiled for both targets.

Per `scope.md`, the Teensy is the *intentionally simplest* tier of a documented
3-tier progression (Teensy+RC → ESP32+RC → ESP32+WiFi-API). The Teensy being
"feature-behind" is the design, not a defect.

**Portable-TODO count: 0 items recommended for scheduling.** One item (barometer
on Teensy) is *technically* portable but is assessed **skip** — see §4.

---

## 2. Build environments (`platformio.ini`)

| Env family | Board(s) | Key macros |
|---|---|---|
| `teensy40` / `41` / `36` | Teensy 4.0/4.1/3.6 | `ARDUINO_TEENSY4x`, `USB_SERIAL` |
| `teensy4x_calibration` | + `Calibration` lib | adds `CALIBRATION_MODE`, `USE_OLED_DISPLAY` |
| `esp32` / `esp32s3` | esp32dev / S3-devkitc | `USE_ESP32`, `USE_OLED_DISPLAY`, `USE_WIFI`, FreeRTOS-HZ, AsyncTCP core-pin |
| `esp32_calibration` / `esp32s3_calibration` | + `Calibration` lib | adds `CALIBRATION_MODE` |

The defining differences are exactly three macros: **`USE_ESP32`** (platform),
**`USE_WIFI`** (only set on ESP32 envs), and the AsyncTCP/FreeRTOS tuning flags
(only meaningful on ESP32). `USE_OLED_DISPLAY` is **already on both** sides
(ESP32 base + every Teensy `_calibration` env). ESP32's `USE_WIFI` in turn
auto-enables `USE_WEB_SERVER`, `USE_API_SERVER`, `USE_OTA` in `config.h:54-57`.

---

## 3. Feature-parity table

| Feature | ESP32 | Teensy | Gating macro | Verdict |
|---|---|---|---|---|
| IMU read + Madgwick fusion | yes | yes | shared (`USE_MPU6050/9250`) | parity — no action |
| PID control (rate/angle) | yes | yes | shared (`USE_*_CONTROLLER`) | parity — no action |
| Motor/servo mixer + output | yes | yes | shared (PWMServo on Teensy, LEDC on ESP32) | parity — no action |
| RadioComm (SBUS/iBUS/DSM/PPM/PWM) | yes | yes | shared (`USE_*_RECEIVER`) | parity — no action |
| Serial + I2C command sources | yes | yes | shared (`USE_SERIAL/I2C_COMMANDS`) | parity — no action |
| Arming / failsafe / throttle-cut | yes | yes | shared | parity — no action |
| Full calibration suite (IMU/radio/orient/ESC/mag/PID/filter) | yes | yes | `CALIBRATION_MODE` (both) | parity — no action |
| OLED display | yes | yes | `USE_OLED_DISPLAY` (both) | parity — no action |
| Dual-core (Core 0 = FC, Core 1 = services) | yes | n/a | `USE_ESP32` | **intentional** — Teensy is single-core silicon |
| WiFi STA | yes | no | `USE_ESP32 && USE_WIFI` | **intentional** — Teensy has no radio |
| Web server (JSON API / WebSocket / mDNS) | yes | no | `USE_ESP32 && USE_WEB_SERVER` | **intentional** — needs WiFi |
| Swarm API client (telemetry POST) | yes | no | `USE_ESP32 && USE_API_SERVER` | **intentional** — needs WiFi |
| OTA firmware update | yes | no | `USE_ESP32 && USE_OTA` | **intentional** — needs WiFi; Teensy flashes over USB |
| Barometer (telemetry-only) | yes | no | `USE_BAROMETER` + `USE_ESP32` setup gate | **skip** (technically portable — see §4) |
| GPS passthrough (raw NMEA) | yes | no | `USE_GPS` + `USE_ESP32` setup gate | **intentional** — exists only to serve the WiFi telemetry surface |

Sources: `platformio.ini`; `src/main.cpp:35-59,249-266,358-418,425-486`;
`include/config.h:44-57,100-181`; `src/{web_server,wifi_manager,api_client,ota,
barometer,gps}.cpp` guard headers; `docs/scope.md` §"Hardware Architecture
Vision" / §"Progression Path"; `phase_w2_barometer_landed_2026-05-20.md`;
`phase_w5_gps_landed_2026-05-20.md`.

---

## 4. The one "technically portable" feature — barometer — and why it is *skip*

The barometer is the *only* ESP32-only feature whose hardware (a BMP280 on I2C)
needs no WiFi and could physically attach to a Teensy. The feature flag
`USE_BAROMETER` is itself platform-neutral (`include/barometer.h:21`,
`config.h:112`). But it is assessed **skip**, not portable-TODO, for concrete
code and scope reasons:

1. **The driver is hard-wired to FreeRTOS.** `src/barometer.cpp:513-570` uses
   `portMUX_TYPE`, `portMUX_INITIALIZER_UNLOCKED`, `TaskHandle_t` and
   `xTaskCreatePinnedToCore` *unconditionally* — ESP32-only APIs. `main.cpp:382`
   only calls `startBarometerTask()` inside `#ifdef USE_ESP32`. A Teensy port is
   not a flag flip; it needs a second, single-core, `loop()`-sliced code path
   (the very design the W2 report §9 deviation 1 *rejected* for ESP32 because of
   HTTP head-of-line stalls — though that specific reason does not apply to a
   Teensy with no web server).
2. **There is no consumer for the data on Teensy.** On ESP32 the barometer
   exists to fill a `"baro"` field in the swarm-API JSON
   (`web_server.cpp serializeDisplayData()`). The Teensy has no web server and
   no API client — the only possible sink is the OLED or a serial print. That
   is a *new* telemetry surface that does not exist today, not a port.
3. **Scope explicitly frames it as a Core-1 sensor.** `scope.md` §"Hardware
   Architecture Vision" and `config.h:100` both describe the barometer as
   *"polled by a dedicated Core-1 FreeRTOS task"* — i.e. an ESP32-shaped
   feature. The scope.md footnote `[^baro-gps]` brought baro/GPS *into* scope
   specifically as **Core-1 modular sensors**. Putting the baro on a
   single-core Teensy contradicts the stated design intent.

Estimated cost if it were ever scheduled (informational only — **not
recommended**): ~0.5–1 day. Needs: a `#ifndef USE_ESP32` single-core poll path
in `barometer.cpp`, an OLED screen or serial-telemetry line as the data sink,
BMP280 hardware on the Teensy I2C bus. Verdict stands at **skip** unless a user
explicitly wants a Teensy altimeter readout — in which case re-scope first.

GPS passthrough is *not* even technically portable in spirit: its W5 landing
report §10 states outright "**No Teensy support** — ESP32/S3 only (passthrough
exists to serve the WiFi telemetry surface)." Without a web/API server the
relayed NMEA bytes have nowhere to go.

---

## 5. Why the WiFi-family features are intentional, not TODO

WiFi STA, web server, swarm-API client and OTA are **physically impossible** on
the Teensy 4.x/3.6 — those chips have no radio. They are not "not done yet";
they are the entire reason the ESP32 tier exists. `scope.md` §"Progression
Path" defines this explicitly:

- Tier 1 **Teensy + RC** — "Simplest wiring… Manual RC control only."
- Tier 2/3 **ESP32 + WiFi/API** — adds web dashboard, OTA, swarm coordination.

The OTA case is the clearest: the Teensy already has a *better* update path for
its tier (direct USB via `teensy-gui`, `platformio.ini:14`). OTA on Teensy
would be net-negative even if hardware allowed it.

Dual-core is likewise silicon: the Teensy is single-core by construction;
`main.cpp:9-11,107-109,415-418` document the single-core `loop()` path as the
deliberate Teensy architecture.

---

## 6. Recommendation

**Do not schedule Teensy parity work.** The evidence — `platformio.ini`,
`main.cpp`'s guard structure, `scope.md`'s explicit 3-tier progression, and the
W2/W5 landing reports — all agree that the split is by design:

- The **flight-control core is already at 100% parity** (shared source).
- Every ESP32-extra is a **connectivity feature gated out of Teensy because the
  Teensy lacks WiFi and a second core** — correct, not a gap.
- The session's ESP32 focus (WiFi, swarm-API, W2 baro, W5 GPS, GPIO conflicts)
  touched **only** ESP32-tier code; it did not and could not regress the Teensy
  flight build, which is reported compiling clean.

The healthiest framing: the Teensy build is *complete for its tier*, not
*behind*. Spending effort to bolt connectivity-shaped features onto it would
violate `scope.md`'s "Do not become Betaflight" / "bare bones" principles and
the explicit flight-computer boundary.

---

## 7. No action / out of scope

- **WiFi STA / web server / swarm-API / OTA on Teensy** — impossible (no radio)
  and unwanted (Teensy flashes over USB). Permanent ESP32-only. No action.
- **Dual-core split on Teensy** — single-core silicon. Permanent. No action.
- **GPS passthrough on Teensy** — explicitly ruled out by the W5 landing report;
  no telemetry sink without a web/API server. No action.
- **Barometer on Teensy** — technically portable but assessed **skip** (§4):
  needs a new single-core driver path and a new telemetry sink that do not
  exist; contradicts the documented "Core-1 modular sensor" framing. Revisit
  only on an explicit user request, and re-scope first.
- **Any flight-loop behaviour difference** — none exists; the core is shared
  source. No action.

---

*Recon complete. Verdict: the ESP32/Teensy split is correct; no Teensy parity
work recommended. 0 portable-TODO items scheduled (1 baro item assessed and
deliberately deferred as skip). One new doc written; no code edited; no git
commit.*
