# BNO055 Driver Design and Multi-IMU / Multi-MCU Strategy

**Status:** Research / design proposal
**Author:** Architecture review, 2026-05-12
**Targets:** `auto_orientation/src/sensors/` — new `bno055.{h,cpp}` driver

---

## Recommendation Summary

- **Add a `BNO055` class that derives from `OrientationSensor`** (same vtable as `BNO085`); use **runtime polymorphism via `OrientationSensor*`** on Teensy/ESP32, but allow a `-D USE_BNO055_ONLY` compile-time short-circuit for Nano (2 KB SRAM, vtable cost matters less than code-elimination wins).
- **Pull the EEPROM access out of `calibration_storage.{h,cpp}`** behind a thin `PersistentStorage` HAL — `<EEPROM.h>` compiles on AVR/Teensy, but on ESP32 it is an emulation wrapper over NVS with surprising lifetime semantics; we need a `Preferences`-backed implementation for ESP32-class boards.
- **Don't unify the calibration blob format.** BNO085 = ~36–72 byte SH-2 FRS record (we currently allocate 256 B), BNO055 = fixed **22-byte** offset/radius dump (Adafruit `getSensorOffsets(uint8_t*)`). Tag the EEPROM payload with a 1-byte sensor-ID header so a swap doesn't silently load a BNO085 blob into a BNO055.

---

## 1. BNO085 vs BNO055 — What the Adapter Has to Hide

| Concern               | BNO085 (current driver)                              | BNO055 (proposed driver)                             |
|-----------------------|------------------------------------------------------|------------------------------------------------------|
| Protocol              | SH-2 over I2C/UART/SPI (packet-based, async events) | Plain I2C register reads/writes (synchronous)        |
| Library               | `Adafruit_BNO08x` (depends on `sh2`, `Adafruit_BusIO`) | `Adafruit_BNO055` (depends on `Adafruit_Sensor`, `Wire`) |
| Fusion output         | Rotation vector quaternion + accuracy estimate       | Euler XYZ, quaternion (`getQuat()`), linear accel, gravity |
| Update model          | "Event arrived?" via `getSensorEvent()`              | Poll-anytime; `getEvent(&e, VECTOR_EULER)` blocks ~2 ms |
| Cal status            | 4 fused values in event `.status` (0–3)              | Four separate registers via `getCalibration(&sys,&g,&a,&m)` |
| Cal blob size         | ~36–72 B (SH-2 FRS DYNAMIC_CALIBRATION record)       | **Exactly 22 B** (`adafruit_bno055_offsets_t`)        |
| External crystal      | n/a                                                  | `setExtCrystalUse(true)` required for stable yaw     |
| I2C addr              | 0x4A (DI→GND) / 0x4B                                 | 0x28 (ADR→GND) / 0x29                                |

### `OrientationSensor` method → `Adafruit_BNO055` call

| Base method                          | BNO055 implementation                                                                 |
|--------------------------------------|---------------------------------------------------------------------------------------|
| `begin()`                            | `Wire.begin(); bno_.begin(OPERATION_MODE_NDOF); bno_.setExtCrystalUse(true);`         |
| `end()`                              | `delete bno_;` (Adafruit lib has no explicit shutdown)                                |
| `read()`                             | `bno_.getQuat()` (returns `imu::Quaternion`) **or** `bno_.getEvent(&e, VECTOR_EULER)` |
| `getOrientation()`                   | Convert quat → Euler with our existing `quaternion_conversions.h` (don't trust BNO055 Euler — it has the discontinuity-at-90° bug) |
| `getCalibrationProfile(buf, &len)`   | `if (!bno_.isFullyCalibrated()) return false; bno_.getSensorOffsets(offsets_); memcpy(buf, &offsets_, 22); *len = 22;` |
| `setCalibrationProfile(buf, len)`    | `if (len != 22) return false; memcpy(&offsets_, buf, 22); bno_.setSensorOffsets(offsets_);` |
| `cal_status/accel/gyro/mag`          | `bno_.getCalibration(&sys, &gyro, &accel, &mag);` (four out-params, four 0–3 values)  |

Note that the BNO055 driver actually populates all four `cal_*` fields independently — unlike the current BNO085 code, which collapses them to `sensor_value.status`. This is an improvement worth keeping.

### Proposed header sketch (inline — do not create the file yet)

```cpp
// src/sensors/bno055.h
#ifndef BNO055_H
#define BNO055_H

#include "sensor_base.h"
#include "../math/quaternion_conversions.h"

class Adafruit_BNO055;  // forward-decl, keeps Adafruit_Sensor.h out of header

class BNO055 : public OrientationSensor {
 public:
  BNO055(uint8_t i2c_addr = 0x28, int32_t sensor_id = 55);
  virtual ~BNO055();

  bool begin() override;
  void end() override;
  bool isInitialized() const override { return initialized_; }
  bool read() override;
  bool hasNewData() const override   { return new_data_; }
  const char* name() const override  { return "BNO055"; }
  bool isHealthy() const override;
  const char* getStatusString() const override;

  const OrientationData& getOrientation() const override { return orientation_; }
  bool setCalibrationProfile(const uint8_t* data, uint16_t length) override; // expects 22 B
  bool getCalibrationProfile(uint8_t* data, uint16_t* length) override;      // returns 22 B

 private:
  Adafruit_BNO055* bno_;
  uint8_t  i2c_addr_;
  int32_t  sensor_id_;
  bool     initialized_ = false;
  bool     new_data_    = false;
  uint32_t last_read_ms_ = 0;
  OrientationData orientation_;
  uint8_t  cal_blob_[22];        // adafruit_bno055_offsets_t footprint
};

#endif
```

Notes: 22 B static `cal_blob_` (vs. 256 B in `BNO085`) saves 234 B of SRAM on Nano — non-trivial.

---

## 2. Adapter / Strategy Choice — Compile-time vs. Runtime

| Option                                       | Flash cost                | SRAM cost                                | Pros                                              | Cons                                                     |
|----------------------------------------------|---------------------------|------------------------------------------|---------------------------------------------------|----------------------------------------------------------|
| (a) `#ifdef USE_BNO085 / USE_BNO055`         | Smallest (~0 overhead)    | One object only                          | Dead-code elimination; trivial on Nano            | Can't probe-and-pick; per-build binary; uglier `main.cpp` |
| (b) Runtime `OrientationSensor*` polymorphism | +~150 B per class for vtable + indirect-call thunks | vtable pointer per instance (2 B AVR, 4 B ARM, 8 B Xtensa) + heap fragmentation if `new` is used | One binary scans 0x28/0x29/0x4A/0x4B and picks; same `main.cpp` for both | vtable cost; AVR `new`/`delete` is fragile |

**Recommendation:** **(b) on Teensy/ESP32, (a) on Nano.** The base class already requires virtual dispatch (`OrientationSensor` has `=0` methods), so the vtable exists whether we use it or not — the only "extra" runtime cost is the heap-allocated derived object and a tagged-construction helper. On Nano we still keep one binary per IMU because (i) Flash is 32 KB and the Adafruit_BNO055 library alone is ~8–10 KB compiled, (ii) shipping both drivers blows that out, (iii) Nano deployments are single-purpose anyway. Concretely: add `-D USE_BNO055_ONLY` / `-D USE_BNO085_ONLY` flags that `#ifdef` out the other driver entirely.

---

## 3. Multi-MCU Portability

- **AVR Mega 2560:** dedicated I2C on pins 20 (SDA) / 21 (SCL). 8 KB SRAM, 4 KB EEPROM, 256 KB Flash. No issues — this is the baseline.
- **AVR Nano (ATmega328P):** I2C on A4 (SDA) / A5 (SCL). **Only 2 KB SRAM, 1 KB EEPROM.** Our current 256-byte BNO085 cal buffer is 12.5% of SRAM — a BNO055-only Nano build saves that, and EEPROM still has plenty of room (22 B cal + 4 B header well under 1 KB).
- **AVR Uno:** Same as Nano (same MCU). Mention only — no separate env needed.
- **Teensy 3.x / 4.x:** I2C via `Wire` (default), `Wire1`, `Wire2`. **3.3 V logic; 3.x is 5 V-tolerant on most pins, 4.x is NOT 5 V-tolerant** — BNO055 breakouts almost universally have onboard regulator+level shifter, but verify. Wire library is fast — push I2C to 400 kHz (BNO055 supports it; BNO085 is buggy above 100 kHz per current driver comment). Teensy 4.0 has 1080 KB Flash and 1 MB SRAM — both drivers fit trivially. EEPROM is **emulated in Flash** (4284 B on T4.x), `<EEPROM.h>` works unchanged.
- **ESP32:** dual I2C buses (`Wire`, `Wire1`), pins are remappable via `Wire.begin(sda_pin, scl_pin)`. 3.3 V only, NOT 5 V-tolerant. **`<EEPROM.h>` is deprecated** — the ESP32 Arduino core ships an `EEPROM` library that wraps NVS, but it requires `EEPROM.begin(size)` first and `EEPROM.commit()` after writes, neither of which our `calibration_storage.cpp` calls. The current code will compile (the wrapper is present) but **silently fail to persist** on ESP32. This is a real bug if anyone tries the existing `[env:esp32dev]`.

**Persistent-storage strategy:** introduce a tiny HAL in `src/config/`:

```cpp
// persistent_storage.h
bool ps_begin(size_t reserve_bytes);   // EEPROM.begin / NVS open
uint8_t ps_read(size_t addr);
void    ps_write(size_t addr, uint8_t v);
bool    ps_commit();                   // no-op on AVR, real on ESP32/Teensy
```

`calibration_storage.cpp` calls these instead of `EEPROM.read/write` directly. Three back-ends: `persistent_storage_avr.cpp` (direct EEPROM), `persistent_storage_teensy.cpp` (also EEPROM, but with `commit()` trivial), `persistent_storage_esp32.cpp` (uses `Preferences` for native NVS, or the wrapped `EEPROM` lib for minimum churn).

---

## 4. Proposed PlatformIO Environments

| Env name                       | Board / platform               | build_flags                                                  |
|--------------------------------|--------------------------------|--------------------------------------------------------------|
| `arduino_mega_bno055_balance`  | `megaatmega2560` / `atmelavr`  | `-D USE_BNO055_ONLY -D APP_SELF_BALANCING`                   |
| `arduino_nano_bno055_balance`  | `nanoatmega328`  / `atmelavr`  | `-D USE_BNO055_ONLY -D APP_SELF_BALANCING -D MINIMAL_LOGGING`|
| `teensy40_bno085`              | `teensy40`       / `teensy`    | `-D USE_BNO085_ONLY -D USE_FAST_I2C`                         |
| `esp32_bno085`                 | `esp32dev`       / `espressif32` | `-D USE_BNO085_ONLY -D USE_NVS_STORAGE`                    |
| `teensy40_multi`               | `teensy40`       / `teensy`    | `-D USE_BNO085 -D USE_BNO055 -D USE_RUNTIME_SENSOR_PROBE`    |

Keep the existing `arduino_mega` (BNO085-only) env unchanged for backward compat.

---

## 5. Library Dependencies (paste into platformio.ini)

For BNO055 environments:

```ini
lib_deps =
    adafruit/Adafruit BNO055 @ ^1.6.4
    adafruit/Adafruit Unified Sensor @ ^1.1.14
    adafruit/Adafruit BusIO @ ^1.16.1
```

(`Wire` is shipped with each framework — do not list it.) For the existing BNO085 envs keep `adafruit/Adafruit BNO08x @ ^1.2.5`. The `PID_v1` library is needed only for the self-balancing app, list it under those envs.

References: [Adafruit BNO055 library README](https://github.com/adafruit/Adafruit_BNO055), [BNO055 datasheet rev 1.4 §3.6.4 "calibration data"](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bno055-ds000.pdf), [Adafruit BNO08x library](https://github.com/adafruit/Adafruit_BNO08x).

---

## 6. Migration Order and Risks

Order of work:

1. **HAL split first** — extract `persistent_storage` HAL, prove the existing BNO085 build still passes on Mega. Zero new IMU code yet. Lowest blast radius.
2. **Driver skeleton** — add `bno055.h/.cpp` with begin/read/getOrientation working, calibration methods as stubs returning `false`. Build under a *new* env (`arduino_mega_bno055_balance`) so old envs are untouched.
3. **Sensor-tagged EEPROM payload** — extend the 4-byte header to 5 bytes by adding a `sensor_id` field (0x85 = BNO085, 0x55 = BNO055). Bump `CAL_FORMAT_VERSION` to `0x02`. Old payloads are invalidated on first read (intended).
4. **Wire calibration round-trip** — `getSensorOffsets`/`setSensorOffsets`, persist 22 B, restore on boot.
5. **Port the working `.ino` logic** into a `src/app/self_balancing.cpp` (PID + motor pins) gated by `-D APP_SELF_BALANCING`.
6. **Tests** — add a `tests/test_bno055_driver/` analogous to existing BNO085 tests; the existing `tests/` already runs under `pio test`.

Expected compile breakage:

- **`#include <Adafruit_Sensor.h>` and `<utility/imumaths.h>`** need to be added to the BNO055 driver's `.cpp` (not `.h`) to keep transitive includes out of `main.cpp`.
- **ESP32 build:** `<EEPROM.h>` exists but won't persist without `commit()` — until HAL lands, expect "calibration saved" log followed by next-boot "no calibration found".
- **Nano build:** the existing BNO085 driver allocates a 256 B class member + uses `new`. Don't try to instantiate both drivers on Nano — the linker may succeed but you'll OOM at runtime.
- **`bno055.begin()` returns `false`** even with correct wiring if `Wire.setClock(400000)` is called before `bno.begin()` on some clones — call `setClock` after.
- The `Adafruit_BNO055` library blocks ~650 ms inside `begin()` waiting for boot — don't put it inside an interrupt or short watchdog window.

Tests likely to flake:

- Anything that asserts `cal_blob_length == 256` — BNO055 always returns 22.
- I2C-address-probe tests assuming 0x4A/0x4B — add 0x28/0x29 to the probe list.
- Existing `bno085_test_sketches.ino` in `docs/findings/` won't link in a BNO055-only build; gate it behind `#ifdef USE_BNO085`.
