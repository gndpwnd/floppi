# OTA Flash — Operator Runbook

> Over-the-air firmware updates for ESP32 builds. ESP32 only — Teensy has no OTA
> path and is always flashed over USB.

This is the operator-facing companion to the implementation in `src/ota.cpp` /
`include/ota.h`. For the wider WiFi/command security picture see
[`findings/swarm_api_contract_2026-05-20.md`](findings/swarm_api_contract_2026-05-20.md).

---

## When OTA is compiled in

OTA is gated behind two build flags: `USE_ESP32` **and** `USE_OTA`
(`include/ota.h:12`). On ESP32 you do not set `USE_OTA` by hand — it is
**auto-enabled** alongside the web server and API client whenever `USE_WIFI`
is set (`include/config.h:54-57`):

```c
#if defined(USE_ESP32) && defined(USE_WIFI)
    #define USE_WEB_SERVER
    #define USE_API_SERVER
    #define USE_OTA
#endif
```

Consequences:

- A **live** ESP32 build with WiFi enabled (`esp32`, `esp32s3`) has OTA available.
- A **`_calibration`** env that does not enable WiFi will **not** have OTA — the
  `#if defined(USE_OTA)` block compiles out and the device will not advertise an
  OTA endpoint. If `[OTA] Ready ...` never prints on serial, you flashed an env
  without OTA.
- To disable OTA on an otherwise-WiFi build, comment out `USE_WIFI` (this also
  drops the web server and API client — they share the flag cascade).

## Enabling / first flash

OTA cannot bootstrap itself: the **first** flash of a new board must be over USB
(`pio run -e esp32 -t upload`). After that firmware is running with `USE_OTA`,
subsequent flashes can go over the network.

## Hostname

The OTA hostname matches the web-server pattern: `floppi-XXXX`, where `XXXX` is
the last two bytes of the WiFi MAC in hex (`src/ota.cpp:25-31`). On boot the
firmware prints:

```
[OTA] Ready at floppi-XXXX.local
```

Note the literal `XXXX` is per-device — read the real suffix off the serial log
or from the web dashboard.

## Pushing an update

From `flight_controller/`:

```bash
pio run -e esp32 -t upload --upload-port floppi-XXXX.local
```

(`src/ota.cpp:10`.) Progress prints over serial as `[OTA] NN%`; on success the
device prints `[OTA] Update complete. Rebooting...` and restarts into the new
firmware.

## Safety: OTA is refused while armed

`handleOTA()` only services OTA when **disarmed** (`armedFly == false`,
`src/ota.cpp:61-65`). You can never flash a flying drone — by design.

**Failure mode — "OTA fails and the drone seems stuck armed":**

1. The push will simply never connect/begin because `ArduinoOTA.handle()` is not
   being called while `armedFly` is true.
2. Disarm first: throttle low **and** the arm channel (CH5) low, per your
   configured arming logic. Confirm disarm on serial / dashboard.
3. Retry the push. If it still fails, see the OTA flow in
   [`diagnose_decision_tree.md`](diagnose_decision_tree.md) (flow 5).

## Security warning

OTA has **no password** set (`src/ota.cpp` calls `setHostname` but not
`setPassword`). Combined with the WiFi command path, this means **anyone on the
same LAN can OTA-flash or command a drone** — the same caveat
`findings/swarm_api_contract_2026-05-20.md` raises for the command API.

Mitigations until authentication lands:

- Fly only on a **trusted, non-shared SSID** you control. Do not enable WiFi/OTA
  on guest or public networks.
- Keep drones on an isolated VLAN/subnet if your AP supports it.
- For a hard lockout, build a WiFi-disabled live env (comment out `USE_WIFI`),
  which removes the OTA endpoint entirely.
