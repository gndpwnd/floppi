# Multi-MCU Port Strategy for `auto_orientation/`

**Status:** Research / design proposal
**Date:** 2026-05-12
**Companion to:** `bno055_driver_and_multi_imu_strategy.md`
**Scope:** Add Arduino Nano, Teensy 4.0, Teensy 4.1, ESP32 (WROOM-32), and ESP32-S3 alongside the existing Arduino Mega target.

---

## Recommendation Summary

- **Mirror the `flight_controller/` two-file pattern:** a thin `pin_definitions.h` "dispatcher" that includes a per-platform `pin_definitions_<plat>.h`, plus `USE_ESP32` / `USE_TEENSY` umbrella flags driven from `platformio.ini`. This is the same split that has already shipped on the sister project for Teensy 3.6/4.0/4.1 and ESP32/ESP32-S3 — do not reinvent it.
- **Introduce a `persistent_storage` HAL today, before adding any new boards.** `calibration_storage.cpp` directly calls `EEPROM.read/write`; on ESP32 that compiles but silently fails to persist (missing `EEPROM.begin(size)` / `EEPROM.commit()`). On Teensy 4.x it works but is emulated in 4 KB of flash. The HAL is a 4-function header with three back-ends (AVR / Teensy / ESP32-NVS).
- **Tier the feature set by MCU class.** Nano = budget build, no SD, no EKF, BNO055-only; Mega = current baseline; Teensy 4.x = high-rate research (EKF at 100–400 Hz, optional Eigen); ESP32/S3 = same as Teensy plus dual-core split and future WiFi telemetry. Migration order: HAL split → Teensy 4.0 → ESP32 (no WiFi) → Nano (BNO055-only) → ESP32-S3 + WiFi.

---

## 1. MCU Comparison Matrix

| MCU                      | Clock        | RAM             | Flash             | FPU             | Persistent storage              | I2C buses | HW UARTs | WiFi | BT       | Active power (typ.) | Price (US, 2026) |
|--------------------------|--------------|-----------------|-------------------|-----------------|---------------------------------|-----------|----------|------|----------|---------------------|------------------|
| Arduino Nano (ATmega328P)| 16 MHz       | 2 KB SRAM       | 32 KB (30 KB usr.)| none (soft FP)  | 1 KB EEPROM (real)              | 1 (Wire)  | 1        | no   | no       | ~19 mA              | $5–10 clone      |
| Arduino Mega (ATmega2560)| 16 MHz       | 8 KB SRAM       | 256 KB (248 KB usr.)| none (soft FP) | 4 KB EEPROM (real)              | 1 (Wire)  | 4        | no   | no       | ~85 mA              | $15–40           |
| Teensy 4.0 (IMXRT1062)   | 600 MHz Cortex-M7 | 1 MB (512 KB DTCM + 512 KB OCRAM) | 1.94 MB (2 MB raw) | hard FPU (FPv5-D16, double-precision) | 4080 B EEPROM (flash-emulated) | 3 (Wire, Wire1, Wire2) | 7 | no | no | ~100 mA @ 600 MHz | $24 (PJRC) |
| Teensy 4.1 (IMXRT1062)   | 600 MHz Cortex-M7 | 1 MB (+ optional 8 MB PSRAM) | 8 MB | hard FPU (FPv5-D16) | 4284 B EEPROM (flash-emul.) + onboard SD slot | 3 | 8 | no (ext. via Ethernet PHY pads) | no | ~100 mA | $32 (PJRC) |
| ESP32-WROOM-32 (dev)     | 240 MHz dual Xtensa LX6 | 520 KB SRAM (320 KB usable) + 4 MB flash | 4 MB (default partition ~1.3 MB app) | hard FPU (single-precision per core) | NVS partition in flash (typ. 24 KB) | 2 (Wire, Wire1) | 3 | 2.4 GHz b/g/n | BT 4.2 / BLE | ~80 mA idle, 160–260 mA WiFi TX | $4–8 module |
| ESP32-S3                 | 240 MHz dual Xtensa LX7 | 512 KB SRAM + up to 8 MB PSRAM | 4–16 MB flash | hard FPU (single-precision) + vector ext. | NVS in flash | 2 | 3 | 2.4 GHz b/g/n | BLE 5.0 (no Classic) | ~80–240 mA | $5–10 module |

Sources: ATmega328P/2560 datasheets (Atmel/Microchip), PJRC Teensy 4.0/4.1 product pages and the i.MX RT1062 reference manual, Espressif ESP32 datasheet rev 3.9 and ESP32-S3 datasheet rev 1.4. Power numbers are typical at the listed clock with one I2C peripheral active; treat as ballpark for spec, not bench-measured.

Key practical implications:

- **Nano:** 2 KB SRAM means the current 256 B BNO085 calibration buffer is ~13 % of all RAM. EKF state + covariance in plain `float` is ~700+ B and rules out the Nano unless EKF is `#ifdef`-ed out. SoftwareSerial is the only option for BNO085 UART.
- **Mega:** Already proven. The "fits comfortably" baseline.
- **Teensy 4.x:** ~50× more compute than Mega (FPU + cache + 600 MHz). Most current bottlenecks dissolve.
- **ESP32 family:** Comparable compute to Teensy for single-precision, dual-core, plus radios. Logic is 3.3 V only — **not 5 V-tolerant**, a regression from Mega.

---

## 2. Pin Assignment Strategy

Today `src/config/pins.h` already does platform `#ifdef`s but cohabitates all four platforms in one file (Nano, Mega, Teensy 3.x, ESP32). That works for two pins; it will not scale once SPI, multiple I2C buses, button GPIOs, and SD CS are added. The `flight_controller/` sister project demonstrates the cleaner pattern (`include/pin_definitions.h` is a dispatcher, ESP32 pins live in `pin_definitions_esp32.h`, Teensy pins are guarded by `#ifndef USE_ESP32` and each pin is `#ifndef`-guarded so `config.h` can override).

Recommended layout for `auto_orientation/`:

```
src/config/
  pins.h                      // dispatcher only
  pins_avr.h                  // Nano + Mega (variant macros inside)
  pins_teensy.h               // Teensy 3.x + 4.x (variant macros inside)
  pins_esp32.h                // ESP32 + ESP32-S3 (variant macros inside)
```

`pins.h` becomes:

```cpp
#if defined(USE_ESP32)        // set by platformio build flag
  #include "pins_esp32.h"
#elif defined(USE_TEENSY)
  #include "pins_teensy.h"
#elif defined(__AVR__)
  #include "pins_avr.h"
#else
  #error "Unknown platform — add a pins_*.h"
#endif

// Common pins (board-agnostic)
#ifndef LED_PIN
  #define LED_PIN 13
#endif
#define SERIAL_OUTPUT_BAUD 115200
```

Critical pins per MCU (defaults; all should be `#ifndef`-guarded so `config.h` can override):

| Function                  | Nano        | Mega        | Teensy 4.0 / 4.1     | ESP32-WROOM        | ESP32-S3             |
|---------------------------|-------------|-------------|----------------------|--------------------|----------------------|
| I2C SDA / SCL (`Wire`)    | A4 / A5     | 20 / 21     | 18 / 19              | 21 / 22 (remap OK) | 8 / 9 (remap OK)     |
| I2C bus 2 (`Wire1`)       | —           | —           | 17 / 16              | 25 / 26 (remap)    | 17 / 18 (remap)      |
| Default `Serial`          | USB/UART0 (D0/D1) | USB/UART0 | USB CDC native      | UART0 (TX0/RX0)    | USB CDC native       |
| `Serial1` (BNO085 UART)   | SoftwareSerial 10/11 | 19/18 (RX1/TX1) | 0 / 1            | 9 / 10 (remap)     | 18 / 17 (remap)      |
| BNO085 INT / nRST         | D2 / D3     | D2 / D3     | 2 / 3                | 4 / 5              | 4 / 5                |
| SPI bus (SD CS suggestion)| n/a (no SD) | 50/51/52/53 (CS=10) | 11/12/13/10 (CS) | 23/19/18, CS=5    | 35/37/36 (or pick), CS=10 |

Notes:

- BNO085 in I2C mode needs the `H_INT` (host-interrupt) line wired to a real GPIO that supports `attachInterrupt`. On Nano only D2/D3 are true interrupt pins — restricts wiring options. On all other targets any GPIO works.
- Teensy 4.x is **not 5 V-tolerant** despite being firmly 3.3 V-logic. The Mega tolerates 5 V signals directly — when porting wiring diagrams, add the level-shifter warning.
- ESP32 boot-strapping pins (GPIO0/2/12/15) must remain free at reset. Don't assign these to the IMU INT line.

---

## 3. Persistent Storage HAL

Current `calibration_storage.cpp` calls `EEPROM.read/write` directly. Behaviour by platform:

| Platform   | Library     | Semantics                                                                 |
|------------|-------------|---------------------------------------------------------------------------|
| AVR (Nano/Mega) | `<EEPROM.h>` (real EEPROM) | Byte-addressable, 3.3 ms/byte write, no `begin/commit` needed. |
| Teensy 4.x | `<EEPROM.h>` (PJRC, emulated in flash) | API-identical to AVR. Writes are buffered; `EEPROM.update()` is preferred. 4080–4284 B usable. |
| ESP32 / S3 | `<EEPROM.h>` (Arduino-ESP32 wrapper, **deprecated**) over NVS | Requires `EEPROM.begin(size)` *before any read/write* and `EEPROM.commit()` after writes. Without these, writes are dropped on reset. The preferred path is `Preferences` (the native NVS API). |

Without intervention, the existing `[env:esp32dev]` will appear to work — the build links, "calibration saved" log fires — but on reboot the marker reads 0xFF and the device re-calibrates. The BNO055 research doc already flagged this.

Proposed header sketch (do **not** write the file yet):

```cpp
// src/config/persistent_storage.h
#ifndef PERSISTENT_STORAGE_H
#define PERSISTENT_STORAGE_H

#include <stdint.h>
#include <stddef.h>

namespace ps {
  // Call once from setup(). reserve_bytes is a hint:
  //   AVR:    ignored (real EEPROM, size is fixed at link time)
  //   Teensy: ignored
  //   ESP32:  passed to EEPROM.begin(size) or Preferences.begin(...) internally
  bool begin(size_t reserve_bytes);

  uint8_t  read(size_t addr);
  void     write(size_t addr, uint8_t value);
  bool     commit();   // no-op on AVR/Teensy, real on ESP32
  bool     clear(size_t addr, size_t length);  // optional helper

  // Capability flags exposed for callers that want to skip block ops
  bool     isPersistent();   // false on platforms without storage (none currently)
  size_t   capacity();
}

#endif
```

Three back-ends, selected at compile time:

- `persistent_storage_avr.cpp`  — direct `EEPROM.read/write`, `commit()` returns true.
- `persistent_storage_teensy.cpp` — same, but use `EEPROM.update()` to spare flash wear.
- `persistent_storage_esp32.cpp` — wraps `Preferences` (native NVS) with a synthesized byte-address space, or falls back to the deprecated `EEPROM` lib if simpler. The wrapper variant is half the code; the `Preferences` variant survives core upgrades better.

`calibration_storage.cpp` then changes `EEPROM.read(x)` → `ps::read(x)` etc., and `setup()` gains a single `ps::begin(CAL_EEPROM_SIZE)` call. Net effect: existing AVR builds are bit-identical; ESP32 builds suddenly persist correctly.

---

## 4. Floating Point + DSP Performance

The EKF (`src/navigation/ekf.cpp`, ~750 lines, ~116 `float`/`double`/matrix references) is the dominant FP load. On the Mega, `float` is software-emulated by avr-gcc — every multiply is roughly 80–150 cycles, every divide ~250 cycles. A 9-state predict step with full Jacobian is several thousand FP ops, which is why we live at ~10 Hz on the Mega.

What lifts on each platform:

- **Teensy 4.0/4.1** — single FP multiply on the Cortex-M7 FPU is one cycle (VFP single-precision) and ~3–4 cycles for double. At 600 MHz, a 9-state EKF predict that takes ~70 ms on Mega should drop into the ~200–500 µs range, conservatively a **100× speedup**. EKF at 400 Hz on Teensy 4.x is realistic; EKF at 100 Hz leaves >90 % CPU headroom for everything else.
- **ESP32 / ESP32-S3** — single-precision FPU per core, ~2–3 cycles per multiply. Roughly half the FP throughput of a Teensy 4.x but still **20–40× faster than AVR**. EKF at 100 Hz on a single core is comfortable. Double-precision is software-emulated and noticeably slower — keep EKF in `float`, not `double`, on ESP32.
- **Nano** — same soft-float as Mega but with less RAM. Recommendation: **do not run the EKF on Nano.** Compile it out with `#ifdef USE_EKF` and tier it off in the Nano build env. Provide a simpler complementary-filter path for the Nano.

Should the EKF use **Eigen** on faster MCUs? Tentatively no, with caveats:

- Eigen is a powerful library but bloats flash by 30–80 KB and inflates compile times severely. On a 9-state EKF the assembly differences vs. hand-rolled arrays are marginal once the FPU is doing the work. Plain arrays remain a fine choice.
- **Where Eigen would help:** if we expand to a 15+ state EKF (bias terms, scale factors), or if we want to swap in `Eigen::Map` over DMA buffers without copying. Defer until that need is real.
- A middle path is `BasicLinearAlgebra` (Tom Stewart's header-only lib) — much lighter than Eigen, drop-in for small matrices, works on AVR. Worth a look if hand-rolled loops become unwieldy.

Concrete recommendation: **keep plain arrays for now**; add a `-D USE_EIGEN` flag later that gates an alternative implementation if/when state count grows. The right time to switch is when an existing function definitively needs it, not pre-emptively.

---

## 5. Dual-Core ESP32 Strategy

ESP32 and ESP32-S3 both have two cores (240 MHz Xtensa each on ESP32, same on S3). The `flight_controller/` precedent pins core 0 to flight control (priority 3 task) and leaves core 1 (the "Arduino loop core" by default) for WiFi, OLED, web server, and OTA, with a 1-deep `xQueueOverwrite` for state hand-off. Mirror that here:

| Core      | Tasks                                                                          | Priority | Period          |
|-----------|--------------------------------------------------------------------------------|----------|-----------------|
| Core 0    | `imuTask` — read BNO085 over I2C/UART, push to shared state                    | 4        | 100–400 Hz      |
| Core 0    | `ekfTask` — EKF predict + measurement update                                    | 3        | 100 Hz          |
| Core 1    | `gpsTask` — UART parse (NMEA / UBX), push fix updates                          | 2        | 5–10 Hz         |
| Core 1    | `outputTask` — serial telemetry formatting, log lines, plotter protocol         | 1        | 10–20 Hz        |
| Core 1    | `wifiTask` (future, `USE_WIFI` only) — telemetry stream / web UI                | 1        | event-driven    |
| Core 1    | Arduino `loop()` — button polling, status LED                                   | 1 (idle) | as-available    |

Synchronisation primitives:

- **IMU → EKF:** `xQueueOverwrite` depth-1, latest sample wins. The EKF doesn't need every sample, just the most recent + dt.
- **EKF → output:** same pattern — a `StateSnapshot_t` queue, depth 1. The serial task drops late samples without blocking the EKF.
- **GPS → EKF:** depth-4 queue (GPS fixes are slower and we want to buffer one or two while the EKF is busy).
- **Critical sections:** use `portMUX_TYPE` for the few shared globals that can't be queue-passed (e.g., calibration flags written from a serial command). Avoid `Wire.lock()`-style schemes — keep each I2C bus on a single core to dodge the issue.

I2C ownership: BNO085 on `Wire`, owned by core 0 (the only core that touches it). If a second I2C device is added (display, BNO055 backup), put it on `Wire1` and let core 1 own it. This prevents cross-core arbitration overhead.

ESP32-S3 differences: same model, plus the vector extension can be used for matrix ops if and when we move to Eigen. PSRAM (8 MB on S3 modules) is irrelevant for the current code but enables a much larger SD log buffer in the future.

---

## 6. Build Environment Proposal

Add the following envs to `platformio.ini` (today there are 8 Mega variants + stub Teensy 3.1 + stub `esp32dev`):

| Env name                       | Board / platform                | build_flags                                                                                   | Notes |
|--------------------------------|---------------------------------|-----------------------------------------------------------------------------------------------|-------|
| `arduino_nano`                 | `nanoatmega328` / `atmelavr`    | `-D USE_BNO055_ONLY -D MINIMAL_LOGGING -D DISABLE_EKF -D DISABLE_SD`                          | Budget build. SoftwareSerial for BNO085, no SD slot, EKF compiled out. |
| `arduino_nano_calibration`     | `nanoatmega328` / `atmelavr`    | `-D USE_BNO055_ONLY -D CALIBRATION_MODE -D DISABLE_EKF -D DISABLE_SD`                         | Calibration mode for Nano deploys. |
| `teensy40`                     | `teensy40` / `teensy`           | `-D USE_TEENSY -D EKF_RATE_HZ=100 -D USE_FAST_I2C`                                            | Research/high-rate. FPU-on by default in PlatformIO Teensy toolchain. |
| `teensy40_calibration`         | `teensy40` / `teensy`           | `-D USE_TEENSY -D CALIBRATION_MODE -D EKF_RATE_HZ=100`                                        | |
| `teensy41`                     | `teensy41` / `teensy`           | `-D USE_TEENSY -D USE_TEENSY41 -D EKF_RATE_HZ=200 -D USE_FAST_I2C`                            | Extra UARTs, onboard SD. |
| `teensy41_calibration`         | `teensy41` / `teensy`           | `-D USE_TEENSY -D USE_TEENSY41 -D CALIBRATION_MODE`                                           | |
| `esp32_dev`                    | `esp32dev` / `espressif32`      | `-D USE_ESP32 -D EKF_RATE_HZ=100 -D USE_NVS_STORAGE`                                          | ESP32-WROOM-32, **no** WiFi. Same compute model as Teensy. |
| `esp32_dev_wifi`               | `esp32dev` / `espressif32`      | `-D USE_ESP32 -D USE_WIFI -D EKF_RATE_HZ=100 -D USE_NVS_STORAGE`                              | Adds telemetry over WiFi (future, mirrors flight_controller pattern). |
| `esp32s3`                      | `esp32-s3-devkitc-1` / `espressif32` | `-D USE_ESP32 -D USE_ESP32S3 -D USE_WIFI -D BOARD_HAS_PSRAM -D EKF_RATE_HZ=200 -D USE_NVS_STORAGE` | PSRAM + WiFi by default. |
| `esp32s3_calibration`          | `esp32-s3-devkitc-1` / `espressif32` | `-D USE_ESP32 -D USE_ESP32S3 -D CALIBRATION_MODE -D BOARD_HAS_PSRAM`                       | |

What gets disabled on Nano:

- **SD card** (`-D DISABLE_SD`): no SPI pins free once SoftwareSerial owns 10/11, and 32 KB flash leaves no room for the `SD` library.
- **EKF** (`-D DISABLE_EKF`): too RAM-hungry. Fall back to a complementary filter (or just trust the BNO055's onboard fusion).
- **Snapshot mode** (already SD-dependent — auto-disabled).
- **GPS at 115200**: SoftwareSerial cannot keep up reliably above 38400 on a 16 MHz AVR; force 9600.
- **Verbose logging** (`-D MINIMAL_LOGGING`): trim per-loop debug strings to save flash.

The existing `arduino_mega*` envs remain unchanged. Add `-D USE_AVR_MEGA` to them if any of the new HAL files need to disambiguate Mega from Nano (they will — different EEPROM sizes, different UART counts).

---

## 7. Migration Order

1. **HAL split first.** Extract `persistent_storage_*.cpp` and wire it into `calibration_storage.cpp`. Build remains a single env (`arduino_mega`) and must produce a bit-identical binary. Lowest risk; highest leverage (fixes the latent ESP32 bug). 1–2 hours.
2. **Pin dispatcher refactor.** Split `pins.h` into `pins.h` + `pins_avr.h` + `pins_teensy.h` + `pins_esp32.h`. Keep all current pin numbers. Mega build still passes. ~1 hour.
3. **Teensy 4.0 first new target.** Add `[env:teensy40]`, fix anything that won't compile (Serial type differences, `printf` format width warnings, possibly `EEPROM.update()`). This is the easiest port — same Arduino API as Mega, faster CPU, no exotic platform features.  Run the existing self-test on bench hardware. ~half-day.
4. **ESP32 (no WiFi) second.** Add `[env:esp32_dev]`, confirm `ps::begin()` is called early in setup, verify calibration round-trips across a reset. This is where the latent EEPROM bug surfaces; the HAL pre-empts it. ~half-day.
5. **Nano third.** Add `[env:arduino_nano]` with BNO055-only build (BNO085 won't fit), `DISABLE_EKF`, `DISABLE_SD`. Validate that the simpler complementary-filter path produces usable orientation. ~half-day plus testing time.
6. **ESP32-S3 + WiFi.** Once `[env:esp32_dev]` is solid, copy it to `[env:esp32s3]` with PSRAM enabled; then layer in `USE_WIFI` and the dual-core task split. Borrow `setupWiFi()`/`xTaskCreatePinnedToCore` patterns directly from `flight_controller/src/main.cpp`. ~1–2 days.
7. **Teensy 4.1.** Trivial after T4.0 — same toolchain, same headers, just more pins and onboard SD. ~1–2 hours.

The rationale for ordering: each step gives a new capability (fast FP, then persistent storage that actually persists, then a cheap deployable variant, then radios). The two big risks (ESP32 storage and Nano memory pressure) are surfaced early.

---

## 8. Risks & Open Questions

- **Adafruit_BNO08x on ESP32 with shared I2C:** the library is documented to work on ESP32 but performance under load (display + IMU on same bus) hasn't been measured here. If we add an OLED on `Wire`, expect SH-2 packet drops. Mitigation: dedicate `Wire` to BNO085, put display on `Wire1`.
- **Teensy 4.x EEPROM wear leveling:** PJRC's emulated EEPROM has wear-levelling, but writing the full calibration block on every boot would still eventually exhaust the underlying flash sector. Use `EEPROM.update()` (compare-before-write) in the Teensy HAL back-end, not `EEPROM.write()`.
- **ESP32 NVS partition size:** default ESP32 partition tables allocate only 24 KB to NVS. If the calibration blob grows past that, we need a custom partition CSV. Current usage (256 B header + payload) is well within limits.
- **Nano flash budget unverified:** `Adafruit_BNO055 + Adafruit_Sensor + Adafruit_BusIO + Wire + SoftwareSerial + ps::* + the app` probably fits in 32 KB, but with `<5 KB to spare. A test build is needed before committing to the Nano env. If it overflows, drop SoftwareSerial and require I2C-mode BNO055 (which is the default anyway).
- **SoftwareSerial latency on Nano:** at 9600 baud SoftwareSerial is fine. At 115200 (BNO085 UART-RVC default) it drops bytes. The Nano build must force the BNO085/BNO055 into a slower mode or use I2C only.
- **ESP32 brown-out during WiFi TX:** WiFi TX can pull 240+ mA peaks. If powering from a weak BEC the IMU and EKF can desync mid-transmit. Document the BEC requirement; this is the same caution as `flight_controller/`.
- **Eigen vs. hand-rolled timing:** not benchmarked. Treat the "no Eigen for now" recommendation as a default, not a final decision.
- **Teensy 4.1 SD interaction with EKF rate:** SD card SPI transfers can block for tens of ms. If snapshot logging runs at full EKF rate, we'll lose samples. Snapshot writes must be off the EKF core (single-core Teensy: use a DMA-backed SD library, or buffer + flush at a lower rate).
- **Dual-core arbitration of `Serial`:** `Serial.print` is *not* re-entrant on ESP32 by default. If both cores log, output corrupts. Mirror `flight_controller/` by making core 1 the sole printer and core 0 push log messages through a queue.
- **ESP32-S3 USB-CDC vs. UART0 for `Serial`:** S3 native USB is a different driver from WROOM's UART0. `monitor_speed` is meaningless on native USB. Document this for anyone bringing up a new board.
- **`USE_TEENSY` umbrella vs. `ARDUINO_TEENSY40`:** the flight_controller defines both. Adopt the same convention here so existing snippets can be lifted unchanged.

References: PJRC Teensy 4.0/4.1 product pages and the i.MX RT1062 reference manual (NXP); Espressif ESP32 datasheet rev 3.9 and ESP32-S3 datasheet rev 1.4; Microchip ATmega328P (DS40002061B) and ATmega2560 (DS40002211A) datasheets; Arduino-ESP32 core docs on `Preferences` and the `EEPROM` compatibility wrapper; `flight_controller/include/pin_definitions.h`, `flight_controller/include/config.h`, and `flight_controller/src/main.cpp` for the in-tree dual-core / pin-dispatcher precedent.
