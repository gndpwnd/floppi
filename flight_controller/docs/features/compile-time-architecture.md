# Feature: Compile-Time Architecture

> Status: Implemented
> Created: 2026-02-07

How the firmware uses compile-time `#ifdef` directives and dual-core task pinning to produce lean, zero-overhead binaries from a single shared codebase.

---

## Compile-Time Feature Gating (`#ifdef` Pattern)

Every hardware and feature selection in this firmware is resolved at compile time using `#ifdef` / `#elif` preprocessor directives. This is not runtime branching -- the compiler physically excludes unselected code from the binary. The result is identical to deleting the code by hand, except the source remains maintainable.

On microcontrollers this matters: there is literally **zero cost** for features you don't use. No wasted flash, no wasted RAM, no branch instructions, no vtable lookups. The code does not exist in the binary.

### Feature flags in `config.h`

These are user-toggled `#define` lines. Uncomment one, comment the others.

**Flight mode** -- `USE_ANGLE_CONTROLLER` vs `USE_RATE_CONTROLLER`

Only one PID controller is compiled. In `main.cpp`:

```cpp
if (armedFly) {
    #ifdef USE_RATE_CONTROLLER
        controlRATE();
    #elif defined(USE_ANGLE_CONTROLLER)
        controlANGLE();
    #endif
    controlMixer();
}
```

The unselected function and its PID gains are never compiled. `control.cpp` uses the same guard to define the selected function body only.

**IMU driver** -- `USE_MPU6050` vs `USE_MPU9250`

Only one sensor driver is compiled. In `imu.cpp`, the `#ifdef` gates the include, object instantiation, setup sequence, and read function. The Madgwick filter call in `main.cpp` switches between 6DOF and 9DOF signatures:

```cpp
#ifdef USE_MPU6050
    Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt);
#elif defined(USE_MPU9250)
    Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
#endif
```

Magnetometer calibration constants (`MAG_ERROR_*`, `MAG_SCALE_*`) only exist when `USE_MPU9250` is defined.

**Radio protocol** -- `USE_SBUS_RECEIVER`, `USE_DSM_RECEIVER`, `USE_PPM_RECEIVER`, `USE_PWM_RECEIVER`

Only one protocol driver is compiled in `radioComm.cpp`. The SBUS path instantiates an `SBUS` object and includes `SBUS.h`. The DSM path instantiates a `DSM1024` object and includes `DSMRX.h`. The others use interrupt-based PWM/PPM capture. Unused protocol code and library objects are completely absent from the binary.

**Display hardware** -- `DISPLAY_SSD1306_128X32`, `DISPLAY_SSD1306_128X64`, `DISPLAY_SH1106_128X64`

Only one U8g2 constructor is compiled in `display.cpp`:

```cpp
#if defined(DISPLAY_SSD1306_128X32)
    U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(...);
    #define DISPLAY_HEIGHT 32
    #define DISPLAY_LINES  3
#elif defined(DISPLAY_SSD1306_128X64)
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(...);
    #define DISPLAY_HEIGHT 64
    #define DISPLAY_LINES  6
#elif defined(DISPLAY_SH1106_128X64)
    U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(...);
    // ...
#endif
```

The rendering code is shared -- it adapts layout via `DISPLAY_LINES`. Adding a new display means adding one `#elif` block in `config.h` and one constructor block in `display.cpp`.

### Feature flags in `platformio.ini`

These are set per build environment via `-D` build flags. They gate entire modules.

**`USE_OLED_DISPLAY`** -- When not defined, the entire display module (`display.cpp`, U8g2 library, `setupDisplay()`, `renderDisplay()`) is compiled out. Teensy live builds omit it. Calibration builds and all ESP32 builds include it.

**`USE_WIFI`** -- When not defined, `wifi_manager.cpp`, `web_server.cpp`, `api_client.cpp`, ESPAsyncWebServer, AsyncTCP, ArduinoJson, and mDNS are all compiled out. Only ESP32 environments define this flag. The guard is `#if defined(USE_ESP32) && defined(USE_WIFI)`.

**`CALIBRATION_MODE`** -- When not defined, all calibration routines, CH6 trigger logic, serial command handler, debug telemetry output, and the `lib/Calibration/` library are compiled out. The live flight loop becomes a tight sequence: read sensors, filter, PID, output motors. Nothing else.

**`USE_ESP32`** -- When not defined, all ESP32-specific code is compiled out: FreeRTOS task creation, `xQueueOverwrite`, LEDC PWM, dual-core logic. Teensy builds get a simple sequential `loop()`. ESP32 builds get the full dual-core architecture with task pinning and queue-based data transfer.

### Why this works on microcontrollers

A Teensy 4.0 has 2MB flash and 1MB RAM. An ESP32 has 4MB flash and 520KB RAM. Every unused feature that stays in the binary steals resources from the flight controller. Runtime `if/else` checks cost branch instructions in a 1kHz loop. With `#ifdef`, the compiler sees one straight path -- no branches, no dead code, no unused objects.

This is the standard pattern in embedded firmware. It is how you get maximum performance without maintaining separate codebases.

---

## Dual-Core vs Single-Core Architecture

The same source files build for both ESP32 (dual-core) and Teensy (single-core). The `#ifdef USE_ESP32` guard splits the architecture.

### ESP32: Dual-Core with FreeRTOS

**Core 0 -- Flight control (real-time, priority 3)**

```cpp
xTaskCreatePinnedToCore(
    flightControlTask, "FlightCtrl",
    4096, NULL,
    3,                  // Priority 3 (high)
    &flightControlTaskHandle,
    0                   // Core 0
);
```

Core 0 runs `flightControlTick()` in an infinite loop at 1kHz:
- IMU read (I2C)
- Madgwick attitude filter
- PID controller (angle or rate)
- Motor output (LEDC PWM)
- `loopRate()` timing enforcement

After each tick, Core 0 pushes a `DisplayData_t` snapshot to Core 1 via `xQueueOverwrite()`. This is non-blocking: it overwrites the single-slot queue with the latest data. Core 0 never waits for Core 1.

**Core 1 -- Everything else (Arduino `loop()`, not time-critical)**

Core 1 handles all non-real-time tasks with a 10ms yield (`vTaskDelay`) between iterations:
- OLED display rendering at 10Hz
- WiFi connection management (STA mode, auto-reconnect)
- ESPAsyncWebServer (HTTP + WebSocket, async callbacks on Core 1 via `CONFIG_ASYNC_TCP_RUNNING_CORE=1`)
- API client (periodic HTTP POST to centralized servers for swarm coordination)

These tasks can take 5-50ms each and it does not matter. Flight control on Core 0 is completely isolated.

**Data transfer between cores**

```cpp
// Core 0 pushes (non-blocking, depth-1 queue, always latest data):
DisplayData_t data;
populateDisplayData(&data);
xQueueOverwrite(displayQueue, &data);

// Core 1 receives (short timeout):
DisplayData_t displayData;
xQueueReceive(displayQueue, &displayData, pdMS_TO_TICKS(5));
```

The `DisplayData_t` struct contains attitude, IMU readings, motor outputs, calibration state, and loop timing. Core 1 adds network info (IP, SSID, RSSI, MAC) after receiving, since WiFi state lives on Core 1.

No mutex is needed: `xQueueOverwrite` on a depth-1 queue is an atomic overwrite. There is exactly one producer (Core 0) and one consumer (Core 1).

### Teensy: Single-Core Sequential

Everything runs in `loop()`, one call at a time:

```cpp
void loop() {
    flightControlTick();    // IMU + filter + PID + motors at 2kHz

    // Rate-limited display (10Hz)
    #ifdef USE_OLED_DISPLAY
    if (current_time - display_timer > 100000) {
        DisplayData_t displayData;
        populateDisplayData(&displayData);
        renderDisplay(&displayData);
    }
    #endif
}
```

Flight control runs first every iteration. Display update fires once every 100ms (~2-4ms render time via software I2C). This is acceptable overhead: 2ms out of every 100ms is 2%.

Teensy has no WiFi hardware, so there is no web server, API client, or WiFi manager. Those modules are gated behind `#if defined(USE_ESP32) && defined(USE_WIFI)` and do not exist in the Teensy binary.

### Summary

| Aspect | ESP32 | Teensy |
|--------|-------|--------|
| Cores | 2 (Core 0 = FC, Core 1 = peripherals) | 1 (sequential) |
| Flight loop | FreeRTOS task, priority 3, pinned to Core 0 | `loop()` direct call |
| Loop rate | 1kHz | 2kHz |
| Data sharing | `xQueueOverwrite` (non-blocking, depth-1) | Direct struct populate |
| Display | Core 1, 10Hz, zero impact on FC | In loop, 10Hz, ~2ms cost |
| WiFi | Core 1 (STA mode, web server, API client) | Not available |
| AsyncTCP | Pinned to Core 1 (`CONFIG_ASYNC_TCP_RUNNING_CORE=1`) | N/A |
| Guard | `#ifdef USE_ESP32` | `#ifndef USE_ESP32` |

The same `flightControlTick()` function runs on both platforms. The same `display.cpp` renders on both. The architecture split is purely in how `setup()` and `loop()` are structured, controlled by a single `#ifdef USE_ESP32` block.

---

## Related

- [build-targets.md](build-targets.md) -- Calibration vs live build separation
- [../findings/display-module-architecture.md](../findings/display-module-architecture.md) -- Display module design
- [../findings/esp32-dual-core-research.md](../findings/esp32-dual-core-research.md) -- ESP32 dual-core research
- [../findings/esp32-wifi-connectivity.md](../findings/esp32-wifi-connectivity.md) -- WiFi STA mode design
