# Choose Your Tier — Balancing Robot Onboarding Decision Tree

**One page. Brand-new operator, hardware in hand, needs to know which build env to flash first.**

The Mega + BNO055 path is the well-trodden default; everything else is flagged as such. Walk the four questions below in order — each leaf links to the matching FIRST_SUCCESS guide.

---

## 1. Which MCU is on your bench?

- **Arduino Mega 2560** (256 KB flash, 8 KB SRAM)
  → **Mega-universal tier**. Headroom for the full adaptive stack (BOOTSTRAP, RLS auto-tune, mounting estimator, wheel encoders, planned position containment). Single-flash workflow — gains adapt online.
  → Continue to question 2.

- **Arduino Uno R3 / Nano / any ATmega328P clone** (32 KB flash, 2 KB SRAM)
  → **Uno-minimal tier**. Stripped-down balancer with hardcoded PID; the flash-erase-write cycle is the iteration loop. Two builds, one EEPROM (tuning env writes; minimal env reads).
  → Continue to question 2.

---

## 2. Which IMU breakout do you have?

- **BNO055** — recommended for new builds. Supported on **both tiers**. NDOF fusion runs on the IMU; the firmware reads a quaternion at 100 Hz. This is the Phase 4 default — `mega_balance`, `arduino_uno_minimal`, and `arduino_uno_tuning` all set `-DUSE_BNO055`.

- **BNO085** — Mega only. The Adafruit_BNO08x + SH-2 library footprint (~15–20 KB flash) overflows the Uno's 32 KB and is rejected by an explicit `#error` in `src/applications/balancing_robot_uno/main.cpp`. On Mega override with `pio run -e mega_balance --project-option="build_flags=-D USE_BNO085 -U USE_BNO055"`. **Future workstream** — not the path for a first success.

If you have BNO085 + Uno, stop here and either swap to a BNO055 breakout or move to a Mega.

---

## 3. Which build env do you flash first?

| Tier | First flash | Guide |
|------|-------------|-------|
| Mega-universal | `pio run -e mega_balance -t upload` | [balancing_robot/FIRST_SUCCESS_MEGA.md](balancing_robot/FIRST_SUCCESS_MEGA.md) |
| Uno-minimal | `pio run -e arduino_uno_tuning -t upload` (SETUP MODE — calibrate + tune), then `pio run -e arduino_uno_minimal -t upload` (OPERATIONAL MODE — fly) | [balancing_robot_uno/FIRST_SUCCESS_UNO.md](balancing_robot_uno/FIRST_SUCCESS_UNO.md) |

**Do not skip the Uno SETUP MODE step.** The minimal flight build reads the calibration blob + tuned gains from EEPROM at boot; without those it refuses to arm.

---

## 4. Does your BNO055 breakout have an external crystal?

This question costs a full bench day if you get it wrong — the BNO055 silently freezes its fusion output and the bot appears to balance against a stuck quaternion (root cause documented in [../THEORETICALLY_SOUND_PROGRAM_PLAN.md](../THEORETICALLY_SOUND_PROGRAM_PLAN.md), §"Why this plan exists").

- **Adafruit BNO055 breakout** — has the 32 kHz crystal soldered on. **Leave defaults alone.**
  - `mega_balance` build_flags: do **not** add `-D BNO055_NO_EXT_CRYSTAL`.
  - `arduino_uno_minimal` / `arduino_uno_tuning` already set this correctly — no action.

- **Generic eBay / AliExpress BNO055 breakout** — no crystal. **Tell the driver.**
  - `mega_balance`: append `-D BNO055_NO_EXT_CRYSTAL` to `build_flags` in `platformio.ini`.
  - `arduino_uno_minimal` / `arduino_uno_tuning` already define this flag (the lean envs assume the generic breakout) — leave alone.

If you do not know which breakout you have: look for a small silver can next to the BNO055 chip. Can present → Adafruit/crystal. No can → generic/no-crystal.

---

## Cross-references

- Wiring + power tree + ASCII diagram: [balancing_robot/HARDWARE_SETUP.md](balancing_robot/HARDWARE_SETUP.md)
- Mega first-success walk: [balancing_robot/FIRST_SUCCESS_MEGA.md](balancing_robot/FIRST_SUCCESS_MEGA.md)
- Uno first-success walk: [balancing_robot_uno/FIRST_SUCCESS_UNO.md](balancing_robot_uno/FIRST_SUCCESS_UNO.md)
- Platform-bifurcation rationale: [INDEX.md](INDEX.md) and [../scope.md](../scope.md)

---

*Last updated: 2026-06-21.*
