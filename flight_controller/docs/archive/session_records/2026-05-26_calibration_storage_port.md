# Session Record — 2026-05-26 — calibration_storage HAL port from auto_orientation

> Project: `flight_controller/` (Teensy + ESP32 firmware)
> Agent (this record): `wave6-docs@floppi:1` (Claude Code Orchestra, multi-agent wave 6)
> Status: **All changes uncommitted — working tree only, awaiting operator review.**
> Companion record (sibling project): [`auto_orientation/docs/archive/session_records/2026-05-26_uno_setup_mode.md` §Wave 6](../../../../auto_orientation/docs/archive/session_records/2026-05-26_uno_setup_mode.md#wave-6--uno-imu-selection-wiring-2026-05-26-late)
> Cross-project source of vendoring: [`/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md`](../../../../docs/findings/bno_cross_project_2026-05-20.md) (identified `calibration_storage` HAL port as the highest-ROI cross-project cheap win).

---

## 1. One-paragraph summary

This wave landed the **first cross-project HAL port from `auto_orientation/` into `flight_controller/`** — a small, focused vendoring of the calibration-storage module that lets the calibration-build's freshly-computed MPU6050 offsets survive a reflash without the operator hand-copying them into `config.h` first. The port is a deliberate **subset** of the AO original (the FC only needs 6 floats of MPU6050 offsets; the AO module carries history that supports 22 B → 506 B BNO blobs) and ships with all of the **2026-05-20 P1 security fixes** baked in: CRC-8-CCITT (not the original naive XOR), an `out_capacity` overflow guard on load, version-byte refusal of legacy v1 blobs, and a `marker == 0xCA` validity check. The backend handles the ESP32 NVS-wrapped-`EEPROM.h` footgun (`begin()` + `commit()` required, otherwise writes are silently dropped on reset — known as KI-1 in AO's HAL notes) so the same module behaves identically on AVR / Teensy / ESP32. **Scale factors** (3 macros set only by the 6-position `'m'` routine) still do **not** persist — that would require runtime-izing the macros + 3 new globals, flagged as a possible follow-up.

This is a **convenience** layer on top of the existing `flash calibration build → run cal → copy values to config.h → flash live build` workflow, not a replacement for it. Live builds still hard-code via `config.h`. The persistence path just removes the copy-paste-into-`config.h` step during the calibration-iteration loop.

---

## 2. What was vendored

**Source**: `auto_orientation/src/config/calibration_storage.{h,cpp}` (post-2026-05-20 P1 security pass, format version 0x02).
**Destination**: `flight_controller/lib/CalibrationStorage/calibration_storage.{h,cpp}`.

### Why `lib/CalibrationStorage/` and not `src/`

PlatformIO's library dependency finder (LDF) auto-discovers anything under `lib/<Name>/` as a private library — any `.cpp` in `src/` that does `#include "calibration_storage.h"` pulls the module into the link without touching `platformio.ini`. Keeping the EEPROM backend inside the lib makes the module self-contained, which matches the "vendor as a self-contained library" guidance and lets a future port to a different backend (Teensy emulated EEPROM, ESP32 Preferences/NVS direct, SD card) happen entirely inside the lib directory.

### Why a tighter API surface than AO

AO's `calibration_storage` carries history: it supports BNO calibration profile blobs up to 506 B. The FC's MPU6050 calibration is **6 floats = 24 B**, and that is all it will ever be on this codebase. The public surface here is the minimum needed:

```c
bool cs_begin(void);                                            // call once in setup()
bool cs_save(const uint8_t* blob, uint16_t len);                // header + CRC + payload
bool cs_load(uint8_t* out, size_t out_cap, uint16_t* out_len);  // capacity-guarded
bool cs_has_valid(void);                                        // cheap marker check
```

The on-disk layout (marker `0xCA`, version `0x02`, `uint16_t` length little-endian, CRC-8-CCITT, 1 reserved byte, payload) is **byte-identical** to AO's v2 layout, so a future cross-project read of the same EEPROM region is possible if needed.

### P1 security fixes carried over from AO

The AO `calibration_storage` was hardened on 2026-05-20 (see AO `findings/security_fix_calibration_2026-05-20.md`). The port carries all four:

1. **CRC-8-CCITT** (poly `0x07`, init `0x00`, no reflect, no final XOR) — replaces an earlier naive XOR-folding "CRC" that did not catch bit-flips at any position.
2. **`out_capacity` overflow guard on load** — `cs_load(uint8_t* out, size_t out_cap, uint16_t* out_len)` rejects a stored payload whose length exceeds the caller's buffer, preventing a stack-buffer overflow when an attacker (or a stale EEPROM blob from a different firmware) writes a larger blob than the caller expects.
3. **Version-byte refusal of legacy v1 blobs** — `version != 0x02` → reject. Old v1 blobs are not auto-migrated; the operator re-runs calibration, which writes a v2 blob.
4. **`marker == 0xCA` validity check** — empty EEPROM (0xFF marker) fails cleanly; any other marker also fails, so a random-byte EEPROM does not get treated as a "valid" blob with a CRC happening to match.

---

## 3. The EEPROM backend choice — ESP32 NVS-awareness baked in

The backend is the standard Arduino `<EEPROM.h>` interface, but the implementation knows that on ESP32, that interface is a **wrapper around NVS** that requires `EEPROM.begin(size)` before any read/write AND `EEPROM.commit()` after each batch — otherwise writes are silently dropped on reset. This is the cross-project **KI-1 footgun** that AO's HAL notes call out (and the reason `auto_orientation` warned future ports not to do the bare `<EEPROM.h>` thing).

Three behaviour splits live inside the backend shim:

| Operation | AVR / Teensy | ESP32 |
|---|---|---|
| `cs_begin_backend()` | no-op (returns true) | `EEPROM.begin(CAL_EEPROM_SIZE)`; returns false on NVS init failure |
| `cs_write_byte(off, val)` | `EEPROM.update(off, val)` (skips unchanged cells → endurance) | `EEPROM.write(off, val)` (NVS doesn't need the update-vs-write distinction) |
| `cs_commit_backend()` | no-op (writes already persistent) | `EEPROM.commit()` (flushes buffered writes to NVS) |

A defensive **auto-begin** path in `cs_save()` / `cs_load()` / `cs_has_valid()` retries `cs_begin_backend()` if the caller forgot `cs_begin()` — so the module works on AVR/Teensy even if `setup()` skipped the begin call, and gives a chance on ESP32 if NVS was unavailable at first boot but is available now.

`s_backend_ready` is a static flag inside the TU; the module is a pure singleton (no instance state, no allocations, no threading concerns).

### CRC-8-CCITT — vendored inline, not as a separate file

The CRC implementation lives as a 20-line `static` helper inside `calibration_storage.cpp` rather than a separate `crc8.h` / `crc8.cpp` — keeps the library self-contained with no extra files for the LDF to discover. Parameters match AO exactly so blobs round-trip across projects.

---

## 4. Restore + persist call sites

### Restore on boot — `src/imu.cpp setupIMU()`

The restore path lives inside the `USE_MPU6050` branch of `setupIMU()`, after the existing MPU6050 `initialize()` + `setFullScaleGyroRange/setFullScaleAccelRange` + I2C-detect-only BNO stubs (the BNO055/BNO085 Phase A scaffolding from 2026-05-20):

```c
if (cs_begin()) {
    uint8_t blob[FC_MPU6050_CAL_BLOB_BYTES];  // 24 B
    uint16_t blob_len = 0;
    if (cs_load(blob, sizeof(blob), &blob_len) &&
        blob_len == FC_MPU6050_CAL_BLOB_BYTES) {
        float vals[6];
        memcpy(vals, blob, sizeof(vals));
        AccErrorX  = vals[0];  AccErrorY  = vals[1];  AccErrorZ  = vals[2];
        GyroErrorX = vals[3];  GyroErrorY = vals[4];  GyroErrorZ = vals[5];
        Serial.println(F("MPU6050 cal restored from EEPROM"));
    } else {
        Serial.println(F("MPU6050 using config.h defaults (no EEPROM cal)"));
    }
} else {
    Serial.println(F("MPU6050 using config.h defaults (EEPROM unavailable)"));
}
```

The blob layout is **6 floats in the same order the runtime globals appear in `main.cpp`** — `AccErrorX/Y/Z` then `GyroErrorX/Y/Z`. The serialization uses `memcpy` rather than struct packing so the layout does not depend on compiler/board padding rules. A `static_assert(FC_MPU6050_CAL_BLOB_BYTES == 24, ...)` catches any future drift.

**Silent fallthrough behaviour is intentional**: if no valid blob is present, the runtime globals keep the `config.h`-macro defaults that `main.cpp` seeded them with — byte-identical to pre-vendoring behaviour. A missing or stale EEPROM blob does **not** break the boot.

### Persist after calibration — `src/calibration_mode.cpp persistIMUCalibration()`

A new `static` helper writes the 6 offsets to EEPROM after any of the four calibration paths that compute them:

```c
static void persistIMUCalibration() {
    if (!calResults.hasIMU) return;  // user cancelled / cal didn't complete
    float vals[6] = {
        calResults.accErrorX,  calResults.accErrorY,  calResults.accErrorZ,
        calResults.gyroErrorX, calResults.gyroErrorY, calResults.gyroErrorZ,
    };
    uint8_t blob[sizeof(vals)];
    memcpy(blob, vals, sizeof(vals));
    if (cs_save(blob, sizeof(blob))) {
        Serial.print(F("MPU6050 cal saved to EEPROM ("));
        Serial.print((unsigned)sizeof(blob));
        Serial.println(F(" bytes)"));
    } else {
        Serial.println(F("MPU6050 cal save to EEPROM FAILED"));
    }
}
```

Called from `runCalibrationIfRequested()` after **each** of the four cal paths that touch the IMU:

- `CALIB_ACCEL_GYRO` — `'i'` single-position offsets-only routine
- `CALIB_6POSITION` — `'m'` 6-position offsets + scale routine (offsets persist; scale does not — see §5)
- `CALIB_ATTITUDE` — `'o'` IMU calibration + orientation detection
- `CALIB_SEQUENTIAL` — full guided workflow (which may include IMU cal as one of its steps; `persistIMUCalibration()` is a no-op if `calResults.hasIMU` is still false because the operator skipped it)

The `calResults.hasIMU` short-circuit makes the call **safe to fire unconditionally** after every cal path; if the operator cancelled or that path doesn't touch the IMU, nothing is written.

---

## 5. Caveat — only OFFSETS persist, SCALE factors do not

The **6 offsets** (`AccErrorX/Y/Z`, `GyroErrorX/Y/Z`) persist. The **3 scale factors** (`IMU_ACC_SCALE_X/Y/Z`) — which are only computed by the 6-position `'m'` routine, not by the simpler `'i'` / `'o'` routines — do **not** persist. They are still compile-time `#define` macros consumed inline by `imu.cpp getIMUdata()`:

```c
AccX = (AccX - AccErrorX) * IMU_ACC_SCALE_X;
AccY = (AccY - AccErrorY) * IMU_ACC_SCALE_Y;
AccZ = (AccZ - AccErrorZ) * IMU_ACC_SCALE_Z;
```

To persist the scale factors as well would require:
1. **Runtime-izing the macros** — replacing the `#define IMU_ACC_SCALE_X 1.0f` with a `float IMU_ACC_SCALE_X = 1.0f;` global (or 3 of them).
2. **Adding 3 globals** to the persistence blob, widening it from 24 B to 36 B (still well under the 506 B max).
3. **Touching `imu.cpp`'s `getIMUdata()`** to consume the runtime variables instead of the macros — in T2's nominal write zone, so doable.
4. **Adding 3 fields to `calResults`** in `calibration_mode.cpp` — also in T2's zone.

Steps 1 and 2 cross the boundary into runtime-mutable global state that today is a hard-coded constant — small, but a meaningful behavioural shift. Flagged as a possible follow-up in [`docs/todo.md`](../../todo.md) (and in the AO companion record's wave-6 follow-up list). Today the 6-position routine still works and still outputs the scale factors via the `'d'` dump command, so the operator can hand-paste them into `config.h` once — the persistence loop only shortens iteration on the offsets.

---

## 6. Build deltas

| Env | Result | Δ vs pre-port baseline |
|---|---|---|
| `esp32` | **SUCCESS** | **+8744 B → 581017 / 35676** (EEPROM/NVS library pulled into the link) |
| `teensy40` | **SUCCESS** | clean (no measurable delta — Teensy `<EEPROM.h>` was already on the link path) |
| `esp32_calibration` | **SUCCESS** | clean (calibration build was already pulling in EEPROM via existing paths) |

The ESP32 +8744 B is the cost of pulling the NVS-backed EEPROM library into the link for the first time on the live `esp32` env. It's a one-shot cost — adding a second persisted blob (e.g. the scale-factor follow-up above) would only add the payload bytes plus another 30 B of header, not another library.

---

## 7. Cross-references

- **Source of vendoring** — [`auto_orientation/src/config/calibration_storage.{h,cpp}`](../../../../auto_orientation/src/config/calibration_storage.h) (the AO original)
- **AO P1 security pass** — referenced inline; see AO `findings/security_fix_calibration_2026-05-20.md`
- **Cross-project research that identified this as the cheap win** — [`/home/devel/floppi/docs/findings/bno_cross_project_2026-05-20.md`](../../../../docs/findings/bno_cross_project_2026-05-20.md)
- **AO companion session record (sibling wave)** — [`auto_orientation/docs/archive/session_records/2026-05-26_uno_setup_mode.md` §Wave 6](../../../../auto_orientation/docs/archive/session_records/2026-05-26_uno_setup_mode.md#wave-6--uno-imu-selection-wiring-2026-05-26-late)
- **FC scope.md Auto-Calibration Philosophy section** — [`docs/scope.md`](../../scope.md) (now mentions the persistence layer)
- **FC todo.md** — [`docs/todo.md`](../../todo.md) (calibration-storage-HAL marked landed wave 6; scale-factor persistence flagged as follow-up)

---

## 8. State of the working tree after wave 6

New (untracked):
- `flight_controller/lib/CalibrationStorage/calibration_storage.h`
- `flight_controller/lib/CalibrationStorage/calibration_storage.cpp`

Modified:
- `flight_controller/src/imu.cpp` (restore-on-boot path inside `setupIMU()`'s `USE_MPU6050` branch)
- `flight_controller/src/calibration_mode.cpp` (new `persistIMUCalibration()` static + 4 call sites)
- `flight_controller/docs/scope.md` (Auto-Calibration Philosophy paragraph + revision history row)
- `flight_controller/docs/archive/session_records/INDEX.md` (this record added)
- `flight_controller/docs/archive/session_records/2026-05-26_calibration_storage_port.md` (this file)
- `flight_controller/docs/todo.md` (calibration-storage-HAL marked done + scale-factor follow-up flagged)

**No commits this session** per the wave-6 orchestrator instruction.

---

## 9. What's next

- **Hardware-validate the restore-on-boot path** — flash `teensy40_calibration` or `esp32_calibration`, run `'i'`, confirm the `MPU6050 cal saved to EEPROM (24 bytes)` line, power-cycle, confirm the `MPU6050 cal restored from EEPROM` line on the next boot. Hardware-gated.
- **Hardware-validate the ESP32 NVS path specifically** — the ESP32 backend split is the trickiest part (NVS `begin`/`commit` requirements); the test that matters is that a save followed by a hard power-cycle (not just a reset) shows the cal restored on the next cold boot. Hardware-gated.
- **Decide on scale-factor persistence** — see §5 caveat. Small, but crosses the runtime-mutable-constants boundary. Operator decision; flagged in `docs/todo.md`.
- **Consider a BNO055/BNO085 driver port** — the cross-project research doc identified `calibration_storage` as the cheap win and the BNO driver port as the bigger-but-still-tractable next step. Not part of this wave; tracked in [`docs/todo.md`](../../todo.md) under "Future Sessions Backlog".
