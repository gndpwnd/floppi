# Phase 4M.11 Landed — `e` Serial Command + EEPROM Wheel-Encoder Calibration

Status: LANDED 2026-05-20. Workstream D. Agent: ao-phase-4m11@floppi:2.
Implements RWE §5 (`research_wheel_encoders_mega_2026-05-19.md`).

## What `e` does

Operator sends `e` over serial while the bot is in IDLE (motors off). Firmware
runs a blocking calibration wizard:

1. Zeroes both wheel encoders, prints `enc_cal: roll bot 1.000 m, press btn`.
2. Operator picks the bot up, sets it on a marked start line, and rolls it by
   hand exactly 1.000 m along a tape measure. A live `L_ticks=N R_ticks=M`
   stream prints once per second so the operator can confirm both wheels count
   and count with the right sign.
3. Operator presses the button (D4). Firmware reads final ticks, computes
   counts-per-metre (CPM) per wheel, derives wheel radius, applies the values
   to the live encoder objects, and persists them to EEPROM slot 0x220.
4. Prints `enc_cal: L=<cpm> R=<cpm> r=<radius> saved`.

Escape hatch: sending `a` during the wizard aborts without writing EEPROM.
Zero-tick guard: if either wheel read 0 ticks, prints `no ticks - not saved`.

CPM→radius: one wheel revolution is `cpr` counts over `2π·r` metres, so
`r = cpr / (CPM_avg · 2π)`, where `CPM_avg` is the mean of the two wheels.
CPM uses `|ticks|` so a swapped-A/B wheel still calibrates (direction is a
wiring concern surfaced by the live stream, not corrected in software).

## Command flow

- `loop()` serial switch: new `else if (c == 'e')` branch → `run_encoder_cal_()`.
- Gated by `#ifdef USE_WHEEL_ENCODERS` — uno_balance never compiles it.
- Encoder objects `enc_left` (pins 18/19) and `enc_right` (pins 2/3) are static
  instances; `begin()` is called once in `setup()`.
- Boot-time read-back: `setup()` calls `load_encoder_cal_()`; on a valid record
  it applies the saved radius to both encoders and prints `ld enc L=.. R=.. r=..`.

## EEPROM slot 0x220 layout as implemented (16 bytes)

```
offset  size  field
0       1     magic 0xAD
1       1     version 0x01
2..5    4     cpm_left   (float32 LE)
6..9    4     cpm_right  (float32 LE)
10..13  4     radius_m   (float32 LE)
14      1     reserved (0x00)
15      1     crc8 (CRC-8-CCITT over bytes 0..14)
```

CRC uses the project-standard `calculateCRC8()` from `calibration_storage.cpp`
(CRC-8-CCITT, poly 0x07, init 0x00) — same algorithm now used project-wide.
Note: RWE §5 sketched `cpm` as u16; the implementation uses float32 per the
Workstream-D brief (more headroom, matches `radius_m` precision). Slot 0x220
sits between the actuator slot (0x210) and the PWM-discovery slot (0x230) — no
overlap. Magic 0xAD is shared with the 0x230 slot but the addresses are
distinct, so there is no aliasing.

## Lines added (main.cpp only — exclusive write zone)

- `#include "sensors/wheel_encoder.h"` (gated).
- 2 static `WheelEncoder` instances (left/right).
- 4 EEPROM constants `EE_ENC_*` (slot 0x220).
- `save_encoder_cal_()` / `load_encoder_cal_()` helpers.
- `run_encoder_cal_()` blocking wizard.
- `setup()` encoder `begin()` + boot read-back block.
- `loop()` serial switch `e` branch.
~150 lines net, all inside `#ifdef USE_WHEEL_ENCODERS` except the include guard.

## Build verification

- `pio run -e mega_balance` → SUCCESS. RAM 17.9% (1468/8192 B),
  Flash 14.4% (36678/253952 B).
- `pio run -e uno_balance` → SUCCESS. RAM 62.2% (1273/2048 B),
  Flash 93.7% (30222/32256 B) — **byte-identical to pre-change** (all encoder
  code gated out). No regression.

Flash/RAM delta on mega_balance vs. pre-4M.11: the new code (encoder objects,
wizard, EEPROM helpers) is the increment over the PWM-discovery baseline; mega
has ample headroom (Flash <15%, RAM <18%).

## What Phase 4M.2 will need

- Phase 4M.2 (Workstream F, K cross-check) needs encoder velocity during the
  BOOTSTRAP pulse. The calibrated `radius_m` from this phase makes
  `WheelEncoder::read_velocity_mps()` physically meaningful — 4M.2 can call it
  to derive `K_motor_encoder = Δv / ΔPWM` and cross-check the gyro-based K.
- The `enc_left` / `enc_right` static instances now exist in main.cpp; 4M.13's
  velocity outer loop will need them injected into `BalanceApp` (constructor or
  setter) — currently they are main-owned and not wired into the app.
- CPM is persisted but `read_velocity_*()` derives from `cpr_`, not CPM. If
  4M.2/4M.13 want CPM-based distance directly, a `set_counts_per_rev()` from
  the loaded CPM·(2π·r) is one option; left out here to avoid double-applying
  the calibration (radius already carries it).
