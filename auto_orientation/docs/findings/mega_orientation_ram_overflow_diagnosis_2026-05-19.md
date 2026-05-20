# mega_orientation RAM Overflow — Diagnosis & Reclaim Plan

**Date:** 2026-05-19
**Env:** `mega_orientation` (Arduino Mega 2560, ATmega2560)
**Builds:** SUCCESS, but **RAM 125.5 % (10283 / 8192 B)** — **2 091 B over**.
**Flash:** 44 772 / 253 952 B (17.6 %) — plenty of headroom.
**Status:** Will not boot in practice. AVR will crash during static-init / first `setup()` call once the stack/heap collide with `.bss`.

---

## 1. Problem statement

The `mega_orientation` env compiles the legacy BNO085 + GPS + 16-state EKF reference application (Phase 3). It links cleanly but its initialised + uninitialised data exceeds the Mega's 8 KB SRAM by **2 091 bytes**. The verification report (`verification_2026-05-19.md`) flagged this as pre-existing — the env was never bench-validated, the balance work happens on `arduino_uno_minimal` / `mega_balance`.

The runtime stack on AVR-Arduino lives at the top of SRAM and grows down; the heap grows up from the end of `.bss`. With `.bss` ending at byte 10 282, the stack starts at byte −2 091, i.e. the very first call frame writes into `.bss` and corrupts the EKF covariance matrix, the I²C buffers, or `Serial`'s ring buffers — depending on link order. Boot will hang or behave randomly.

---

## 2. Memory section breakdown

`avr-size -A -x .pio/build/mega_orientation/firmware.elf`:

| section | bytes  | what                                                                  |
|---------|-------:|-----------------------------------------------------------------------|
| .text   | 43 210 | code in flash — irrelevant to SRAM                                    |
| .data   |  1 562 | initialised globals → copied to SRAM at boot                          |
| .bss    |  8 721 | zero-initialised globals → permanently in SRAM                        |
| total RAM | **10 283** | **must fit in 8 192 — over by 2 091**                           |

`.data` is mostly vtables (BNO085, GPS, TwoWire, HardwareSerial, SdFile = 0x80 B total) plus a 64 B `error_buffer_` (Adafruit BusIO) and a handful of constexpr switch tables. Almost nothing reclaimable. **The fight is in `.bss`.**

---

## 3. Top RAM offenders (`.bss`, sorted desc.)

`avr-objdump -t firmware.elf | awk '$4==".bss"' | sort -k5 -r`:

| rank | bytes | symbol                                              | source                                                | category                |
|----:|------:|-----------------------------------------------------|-------------------------------------------------------|-------------------------|
|  1  | 1 051 | `ekf` (incl. nested 4 × `Matrix16x16` = 4 096 B less padding)\* | `src/main.cpp:540` → `src/navigation/ekf.h:222-226`   | our code — EKF state    |
|  2  | 1 936 | `instances` (one `shtp_t`)                          | `lib/Adafruit_BNO08x_Arduino/src/shtp.c:149`          | vendored lib            |
|  3  |   512 | `SdVolume::cacheBuffer_`                            | `lib/SD/src/utility/SdVolume.cpp:25`                  | vendored lib            |
|  4  |   482 | `_sh2`                                              | `lib/Adafruit_BNO08x_Arduino/src/sh2.c:323`           | vendored lib            |
|  5  |   370 | `imu` (BNO085 wrapper, incl. 256 B `calibration_data_`) | `src/main.cpp:535` → `src/sensors/bno085.h:123`   | our code                |
|  6  |   237 | `gps`                                               | `src/main.cpp:536` → `src/sensors/gps.h` (`status_string_[64]` + `sentence_buffer_[128]`) | our code                |
|  7  |   221 | `output_manager`                                    | `src/main.cpp:537` → `sensor_output_manager.h`        | our code                |
|  8  |   157 | `Serial` ring buffer (USB)                          | Arduino core                                          | core                    |
|  9  |   157 | `Serial1` ring buffer (GPS UART)                    | Arduino core                                          | core                    |
| 10  |    96 | `BNO085::getStatusString` static buffer             | `lib/Adafruit_BNO08x_Arduino`                         | vendored lib            |
|  -  |    96 | TWI rx/tx + master + Wire rx/tx (5 × 32 B)          | Arduino Wire / twi.c                                  | core                    |
|  -  |    73 | `SDLib::SD`                                         | SD library global instance                            | vendored lib            |

> \* Note: `avr-nm` reports `ekf` as 1 051 B because the `ExtendedKalmanFilter` class compiled in this TU shows only a sub-slice; the full storage is 4 × 1 024 B matrices (P, F, Q, P_temp) + 16 × 4 B state + tracking bytes. The four matrices alone account for **4 096 B** of RAM and the type is `float[16][16]`. Confirmed in `src/navigation/covariance_manager.h:29`.

**Subtotal of the top 10: ~5 220 B** — and that is *before* `ekf`'s full footprint, which the symbol table flattens across the loop function.

Stack pressure (not in `.bss` but worth noting): `loop()` allocates `char buffer[512]` on the stack at `main.cpp:789` plus a `Matrix16x16 P_init` (1 024 B) at `main.cpp:641` during EKF init — i.e. ~1.5 KB of transient stack. Together with the .bss overflow, the firmware would not survive setup() even if .bss fit.

---

## 4. Recommended fixes

| # | Fix                                                                                                | Est. saving | Risk    | Effort | Notes |
|---|----------------------------------------------------------------------------------------------------|------------:|---------|--------|-------|
| F1 | **Drop `P_temp_` from `ExtendedKalmanFilter`** — temp can be a function-scope local (stack) or share `F_`. | 1 024 B    | Low     | 1 h    | `ekf.h:226`. Verify no method holds `P_temp_` across calls. |
| F2 | **Drop `F_` member** — same — recompute Jacobian into a stack matrix inside `predict()`.            | 1 024 B    | Low     | 1 h    | Costs ~1 KB stack peak but only during predict(); ISR-free path. |
| F3 | **Shrink SH2 transfer buffers (vendored)** — `SH2_HAL_MAX_TRANSFER_OUT 256→64`, `…_IN 384→128`, `…_PAYLOAD_IN 384→128`. BNO085 sensor reports we use (game-rotation-vector, calibration status) are ≤ 32 B each. | ~ 832 B   | Medium  | 1 h    | `lib/Adafruit_BNO08x_Arduino/src/sh2_hal.h:29-36`. Risk: large FRS reads (sensor metadata) would fail; only used during enableReport once at boot — manageable. Vendor fork required. |
| F4 | **Shrink BNO085 `calibration_data_[256]`** to actual size (≤ 22 B for FRS calibration record) | ~ 234 B   | Low     | 30 m   | `src/sensors/bno085.h:123`. The Adafruit lib only ever writes the FRS save-DCD response into it. |
| F5 | **Disable SD library** — `lib_deps` line `SD` + `cacheBuffer_` (512 B) + `SDLib::SD` (73 B). SD is only used by `features/snapshot_recorder.cpp`, which is gated by `ENABLE_SNAPSHOT_RECORDER` (= `SNAPSHOT_MODE`, currently undefined in this env). | 585 B     | Low     | 15 m   | Just remove `SD` from `lib_deps` and exclude `features/` + `file_system/` in `build_src_filter` (same pattern as `balance_src_filter`). |
| F6 | **Wrap remaining `Serial.print("…")` literals in `F()`** in `main.cpp` (35 + `CAL_PRINT…` macros). Production mode strips CAL prints but the boot banner, ERROR, "[EKF Diagnostics]" line all stay. | ~ 350 B   | None    | 30 m   | Also update `CAL_PRINTLN` macro in `src/config/mode.h:25` to `Serial.println(F(x))` — touches one line, helps every TU. |
| F7 | **Shrink GPS `sentence_buffer_[128]→[96]`** — longest NMEA sentence used (`$GNGGA`) ≤ 82 B. | 32 B      | Low     | 15 m   | `src/sensors/gps.h:90`. |
| F8 | **Shrink GPS `status_string_[64]→[32]`** | 32 B      | Low     | 15 m   | `src/sensors/gps.h:87`. |
| F9 | **Drop EKF entirely from this env** — gyro/accel passed to `ekf.predict()` are hardcoded `{0,0,9.81}` placeholders (`main.cpp:688-689`) and GPS→EKF update is a `// TODO` (`main.cpp:757`). The filter is not doing anything useful. | ~ 4 200 B | High (loses feature) | 1 h | Either guard EKF behind `USE_EKF` (default off) or split into a separate env. Restores the env to BNO085+GPS streaming only — which is what the JSON output already publishes. |

---

## 5. Recommended fix order

**Phase A — cheap, no behavioural change (target: get under 8 192 with margin):**

1. **F5** — drop SD lib (-585 B). Five-minute platformio.ini change.
2. **F1** — remove `P_temp_` (-1 024 B).
3. **F4** — shrink BNO085 calibration buffer (-234 B).
4. **F6** — `F()`-wrap remaining string literals (-350 B).

Cumulative: **≈ 2 193 B reclaimed**. That brings RAM to roughly **8 090 / 8 192 (98.8 %)** — just under, but with only ~100 B headroom. Stack would still crash.

**Phase B — needed for actual stability (target: ≤ 6 KB so stack has room):**

5. **F2** — remove `F_` member (-1 024 B). RAM → ~7 066 B (86 %). Now there is real stack room.
6. **F7 + F8** — GPS buffer shrinks (-64 B).

**Phase C — only if you want to enable SD/snapshot, or if you want to keep EKF but with smaller transfer windows:**

7. **F3** — vendor-fork the BNO08x SHTP buffers (-832 B). Requires copying the lib into `lib/Adafruit_BNO08x_Arduino_min/` and editing.
8. **F9** — the nuclear option if EKF isn't going to be productionised soon.

**Total reclaim potential, Phases A+B:** **≈ 2 257 B** (gets the build to ~78 % SRAM, leaves 1.8 KB for stack — comfortable on Mega).
**Total reclaim potential, Phases A+B+C:** **≈ 4 113 B** (~50 % SRAM).

---

## 6. Will the application work once fixed?

**Mechanically: yes, with caveats.** The link is already clean and the algorithms compile fine. Once `.bss` fits and stack has headroom (Phase A+B), the firmware will boot and the JSON output pipeline (BNO085 → `SensorOutputManager` → Serial) will work end-to-end. That's actually most of what users would interact with.

**Functionally for EKF: no, this env's EKF is a stub today.** Two pre-existing gaps in `main.cpp`:

- Lines 688-689: gyro/accel inputs to `ekf.predict()` are **hardcoded placeholders** (`{0,0,0}` and `{0,0,9.81}`). The BNO085 driver exposes only the fused quaternion via `getOrientation()`; raw gyro/accel events from the SH2 protocol are never plumbed through to main. The EKF "runs" but does nothing meaningful.
- Line 757: `// TODO: Feed GPS measurement to EKF update step` — there is no `ekf.update(...)` call anywhere. GPS only flips the dropout flag.

So if the goal is just **"link the env, ship JSON telemetry to host"**, Phase A+B is enough and the app will be useful. If the goal is **"actually fuse GPS+IMU on the Mega"**, F9 (drop the dead EKF until Phase 3-finish is done) is the honest answer — the 4 KB of RAM are paying for a placeholder. Recommendation: do Phase A+B now, file an issue tracking the EKF wiring gap, and revisit when there's a real fusion need (probably on Teensy/ESP32 — the 16-state EKF with float64 matrix ops is the wrong shape for an 8 KB / 16 MHz part anyway).

---

**Done:** read-only diagnosis; no source modified; no commits.
