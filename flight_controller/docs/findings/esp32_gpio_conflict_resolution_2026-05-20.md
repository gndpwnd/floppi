# ESP32 GPIO Conflict Resolution Spec — 2026-05-20

**Author:** `fc-gpio-conflict-spec@flight_controller:2`
**Scope:** Resolution spec for the 3 ESP32 GPIO pin conflicts surfaced by the
2026-05-20 wiring-guide audit (`docs/findings/wiring_guide_audit_2026-05-20.md`).
**Status:** SPEC ONLY — no source or config edits made. No git commits.

This document is **standard ESP32 only** (`USE_ESP32`, not `USE_ESP32S3`). All
ESP32-S3 pin maps were verified separately and have no equivalent conflicts on
the affected GPIOs (S3 SBUS=18, iBUS/DSM/cmd=16, servos 41/42/1/2/10/11/12).

---

## 1. The conflicts — precise statement

All pin numbers verified against `include/pin_definitions_esp32.h` at HEAD.

### Conflict A — GPIO 16 & GPIO 17 (SBUS vs Servo 4/5)

| GPIO | Claimant 1 | Claimant 2 |
|------|------------|------------|
| 16   | `SBUS_RX_PIN = 16` (line 151) | `SERVO_PIN_4 = 16` (line 116) |
| 17   | `SBUS_TX_PIN = 17` (line 158) | `SERVO_PIN_5 = 17` (line 123) |

`SBUS_RX_PIN` is read at `radioComm_rc.cpp:28`. `SERVO_PIN_4/5` are attached at
`motors.cpp:85-86` (`ledcAttachPin`). They collide on any build that compiles
`USE_SBUS_RECEIVER` **and** drives servo channels 4 or 5.
Note: `SBUS_TX_PIN` is documented "not used" — TX-side risk on GPIO 17 is low,
but `SBUS_RX_PIN` on GPIO 16 is a genuine input/output clash.

### Conflict B — GPIO 4 (iBUS / DSM / Serial-cmd vs Servo 3)

| GPIO | Claimant 1 | Claimant 2 | Claimant 3 | Claimant 4 |
|------|------------|------------|------------|------------|
| 4    | `IBUS_RX_PIN = 4` (line 167) | `DSM_RX_PIN = 4` (line 180) | `SERIAL_CMD_RX_PIN = 4` (line 191) | `SERVO_PIN_3 = 4` (line 109) |

`IBUS_RX_PIN` used at `radioComm_rc.cpp:80`; `SERIAL_CMD_RX_PIN` at
`radioComm_ext.cpp:26`; `SERVO_PIN_3` attached at `motors.cpp:84`.
The three *receiver* claimants on GPIO 4 do NOT conflict with each other —
they are mutually exclusive build flags (only one receiver protocol is
selected, and `IBUS`/`DSM`/`SERIAL_CMD` all share Serial1 by design).
The real clash is **any one of them vs `SERVO_PIN_3`**.

### Conflict C — the third `[VERIFY]` flag (Teensy 16/17 — OLED vs PWM)

The audit's 3rd flag (audit §"Top 3", item 3) is a **Teensy** overlap:
`OLED_SDA_PIN=16 / OLED_SCL_PIN=17` vs `PWM_CH6_PIN=16 / PWM_CH5_PIN=17`.
It is **out of scope** for this ESP32 spec and is **not an ESP32 GPIO
conflict** — included here only to account for all 3 audit flags. On ESP32 the
equivalent pins do not collide (ESP32 OLED=23/19, PWM_CH5/6=23/19 — see §2-C).
No ESP32 action required for Conflict C.

---

## 2. REAL vs THEORETICAL

A conflict is **REAL** only if a single build env enables both claimants
simultaneously. ESP32 build envs (`platformio.ini`): `esp32`, `esp32_calibration`
(both extend `esp32_base`). Receiver protocol is selected in `config.h`
(currently `USE_SBUS_RECEIVER` active; all others commented).

**Critical finding — servos are NOT flag-gated.** `setupMotors()` is called
unconditionally from `main.cpp:318`, and `setupServos()` (`motors.cpp:81-88`)
calls `ledcAttachPin` for **all 7** servo pins on **every** ESP32 build —
there is no `USE_SERVO`/`NUM_SERVOS` guard. So `SERVO_PIN_3/4/5` (GPIO 4/16/17)
are claimed by the LEDC peripheral on every ESP32 firmware image, whether or
not a physical servo is wired.

### Conflict A — GPIO 16/17 (SBUS vs Servo 4/5): **REAL**

With `USE_SBUS_RECEIVER` active (the current default), `radioComm_rc.cpp`
configures GPIO 16 as Serial2 RX, while `motors.cpp` has already done
`ledcAttachPin(16, ...)` and `ledcAttachPin(17, ...)`. Both the LEDC PWM
generator and the UART RX are bound to GPIO 16 in the same image. This is a
real software double-claim today, not just a wiring-table coincidence.
- Functional impact: LEDC output on 16/17 fights the SBUS UART; SBUS framing
  corruption and/or no servo motion on ch4/5. The drone will likely still arm
  (SBUS init runs after servo setup and may "win" the pin matrix), but
  behaviour is undefined and board-revision dependent.
- Mitigating note: if the airframe genuinely uses 0–3 servos, channels 4/5 are
  never *commanded* — but the pins are still *attached*. Still classified REAL
  because the GPIO is electrically driven by LEDC regardless of command value.

### Conflict B — GPIO 4 (iBUS/DSM/Serial-cmd vs Servo 3): **REAL (conditional)**

- vs **iBUS** (`USE_IBUS_RECEIVER`): REAL when that flag is set. Same mechanism
  as A — `ledcAttachPin(4,...)` + Serial1 RX on GPIO 4.
- vs **DSM** (`USE_DSM_RECEIVER`): REAL when that flag is set.
- vs **Serial commands** (`USE_SERIAL_COMMANDS`): REAL when that flag is set.
- With the **current** config (`USE_SBUS_RECEIVER`, no serial commands): GPIO 4
  is claimed only by `SERVO_PIN_3` → **no conflict in the shipping default
  build.** Conflict B is **THEORETICAL for the current config**, but becomes
  REAL the moment the operator switches to iBUS/DSM or enables serial commands.

### Conflict C — ESP32 16/17 OLED vs PWM: **THEORETICAL / N-A on ESP32**

ESP32 OLED pins are 23/19, not 16/17 (`pin_definitions_esp32.h:273,281`).
ESP32 `PWM_CH5/6` are 23/19 — those *would* collide with OLED *if*
`USE_PWM_RECEIVER` and OLED were both enabled, but that is a **different pin
pair (23/19)** and a different (PWM-receiver) conflict outside the 3 audit
flags. The audit's flag C is Teensy-specific. **No ESP32 GPIO 16/17 OLED
conflict exists.** No action.

### Summary

| Conflict | GPIO | REAL today? | REAL under some build? |
|----------|------|-------------|------------------------|
| A — SBUS vs Servo 4/5 | 16, 17 | **YES** (default `USE_SBUS_RECEIVER`) | YES |
| B — iBUS/DSM/cmd vs Servo 3 | 4 | NO (SBUS default) | **YES** (iBUS/DSM/serial-cmd builds) |
| C — OLED vs PWM (Teensy) | 16, 17 | NO (Teensy-only, not ESP32) | N/A on ESP32 |

**Net: 1 REAL-now (A), 1 latent-REAL (B), 1 not-applicable (C).**

---

## 3. Resolution options per REAL conflict

Safe free ESP32 GPIOs available for reassignment — avoiding strapping pins
(0, 2, 12, 15), flash pins (6–11), and input-only pins (34–39). See §5 table.

### Conflict A — GPIO 16/17 (SBUS vs Servo 4/5)

- **Option A — reassign.** Keep SBUS on 16/17 (the wiring docs and SBUS
  comments all assume this; SBUS is the active default). Move the *servos*:
  `SERVO_PIN_4 → 18` is wrong (18 = `SERVO_PIN_7`); use unused safe pins.
  Free candidates: GPIO 13, GPIO 5 (if motor 6 / servo 6 unused). Cleanest:
  reassign `SERVO_PIN_4` and `SERVO_PIN_5` only when 4+ servos are actually
  used. Most airframes (quad/hex, 0 servos) need no change.
- **Option B — `PIN_*` override block** in `config.h` (template in §4),
  applied per airframe that needs both SBUS and ch4/5 servos.
- **Option C — `#error` guard.** Add a compile-time check:
  `#if defined(USE_SBUS_RECEIVER) && (SERVO_PIN_4==SBUS_RX_PIN || ...)` →
  `#error`. Forces the operator to choose explicit overrides.

**Recommendation for A:** **Option C primary, Option B as the fix.** Because
servos are attached unconditionally (§2), a silent collision ships in the
default build *today*. A `#error` guard is the only option that prevents a
bad image from being flashed. Pair it with the §4 override template so the
operator has an immediate, copy-paste resolution. Reassigning defaults
(Option A) is discouraged: it would churn 5 wiring docs that currently agree.

### Conflict B — GPIO 4 (iBUS/DSM/serial-cmd vs Servo 3)

- **Option A — reassign `SERVO_PIN_3`** to a free pin (e.g. GPIO 13) so GPIO 4
  is left for the serial receivers. GPIO 4 is a poor servo pin anyway
  (no strong reason to keep it).
- **Option B — `PIN_*` override** for `SERVO_PIN_3` in iBUS/DSM/serial builds.
- **Option C — `#error` guard** for `SERVO_PIN_3 == IBUS_RX_PIN` etc.

**Recommendation for B:** **Option C guard** (consistent with A — catches the
latent case the moment the operator switches receiver protocol), plus the §4
override template entry for `SERVO_PIN_3`. Since B is not REAL in the shipping
config, no default change is needed now; the guard makes the latent conflict
fail loudly instead of silently when iBUS/DSM/serial-commands is selected.

### Conflict C

No ESP32 action. (Teensy 16/17 OLED/PWM overlap is already documented in the
Teensy wiring guide per the audit; out of scope here.)

---

## 4. Proposed `config.h` PIN_* override template

Ready-to-paste **commented-out** block for the "PIN OVERRIDES" section of
`config.h` (currently lines ~474–516). **DO NOT apply — provided in spec only.**

```c
//=============================================================================
// ESP32 GPIO CONFLICT OVERRIDES (see findings/esp32_gpio_conflict_resolution_2026-05-20.md)
//=============================================================================
// The ESP32 servo pin defaults SHARE GPIOs with receiver pins:
//   SERVO_PIN_3 (GPIO 4)  collides with IBUS_RX / DSM_RX / SERIAL_CMD_RX
//   SERVO_PIN_4 (GPIO 16) collides with SBUS_RX
//   SERVO_PIN_5 (GPIO 17) collides with SBUS_TX
// setupServos() attaches ALL 7 servo pins on every ESP32 build, so the clash
// is real even if you do not wire those servos.
//
// If your airframe uses a serial receiver (SBUS/iBUS/DSM) AND servo channels
// 3/4/5, uncomment the matching lines to move the servos to free GPIOs.
// Safe free GPIOs: 13, 5, 18 (see §5 free-GPIO table — check against your
// motor count first). Do NOT use strapping pins 0/2/12/15, flash 6-11,
// or input-only 34-39.
//
// --- For SBUS builds that also use servo ch4/ch5 (Conflict A): ---
//#define SERVO_PIN_4 13   // moved off GPIO 16 (SBUS_RX)
//#define SERVO_PIN_5  5   // moved off GPIO 17 (SBUS_TX) — only if motor 6 unused
//
// --- For iBUS/DSM/serial-command builds that also use servo ch3 (Conflict B): ---
//#define SERVO_PIN_3 13   // moved off GPIO 4 (IBUS_RX/DSM_RX/SERIAL_CMD_RX)
//
// Alternatively, move the RECEIVER instead of the servo, e.g.:
//#define SBUS_RX_PIN 13   // keep servos on 16/17, move SBUS RX
//=============================================================================
```

Optional compile-time guard (for a future header patch — **not part of this
spec's deliverable**, shown for the implementer):

```c
#if defined(USE_ESP32) && !defined(USE_ESP32S3)
  #if defined(USE_SBUS_RECEIVER) && \
      (SERVO_PIN_4 == SBUS_RX_PIN || SERVO_PIN_5 == SBUS_TX_PIN)
    #error "GPIO conflict: SERVO_PIN_4/5 share SBUS pins 16/17. Override in config.h."
  #endif
  #if (defined(USE_IBUS_RECEIVER) && SERVO_PIN_3 == IBUS_RX_PIN) || \
      (defined(USE_DSM_RECEIVER)  && SERVO_PIN_3 == DSM_RX_PIN)  || \
      (defined(USE_SERIAL_COMMANDS) && SERVO_PIN_3 == SERIAL_CMD_RX_PIN)
    #error "GPIO conflict: SERVO_PIN_3 shares GPIO 4 with the selected receiver. Override in config.h."
  #endif
#endif
```

(Guard must be placed *after* `pin_definitions_esp32.h` is included so all
symbols are defined.)

---

## 5. Free-GPIO reference table (standard ESP32, DevKit V1)

Status reflects `pin_definitions_esp32.h` defaults at HEAD.

| GPIO | Default assignment | Safe to reuse? | Notes |
|------|--------------------|----------------|-------|
| 0  | — | NO | Strapping (boot mode) |
| 1  | — | NO | UART0 TX (USB serial) |
| 2  | LED_PIN | NO | Strapping; OK after boot, used for LED |
| 3  | — | NO | UART0 RX (USB serial) |
| 4  | SERVO_PIN_3, IBUS_RX, DSM_RX, SERIAL_CMD_RX | CONTESTED | Conflict B |
| 5  | SERVO_PIN_6, MOTOR_PIN_5? no — see 12 | PARTIAL | Free only if servo ch6 unused |
| 6–11 | — | NO | Internal SPI flash — never use |
| 12 | MOTOR_PIN_5 | NO | Strapping (flash voltage) |
| 13 | MOTOR_PIN_6 | **YES if motor 6 unused** | Best reassign target for quads/hex |
| 14 | MOTOR_PIN_4 | NO | In use (motor 4) |
| 15 | — | NO | Strapping |
| 16 | SBUS_RX, SERVO_PIN_4 | CONTESTED | Conflict A |
| 17 | SBUS_TX, SERVO_PIN_5 | CONTESTED | Conflict A |
| 18 | SERVO_PIN_7 | PARTIAL | Free only if servo ch7 unused |
| 19 | OLED_SCL, PWM_CH6 | NO | OLED in use (calibration builds) |
| 21 | IMU_SDA | NO | I2C IMU bus |
| 22 | IMU_SCL | NO | I2C IMU bus |
| 23 | OLED_SDA, PWM_CH5 | NO | OLED in use |
| 25 | MOTOR_PIN_1, I2C_CMD_SDA | NO | Motor 1 |
| 26 | MOTOR_PIN_2, I2C_CMD_SCL | NO | Motor 2 |
| 27 | MOTOR_PIN_3 | NO | Motor 3 |
| 32 | SERVO_PIN_1 | PARTIAL | Free if servo ch1 unused |
| 33 | SERVO_PIN_2 | PARTIAL | Free if servo ch2 unused |
| 34,35,36,39 | PWM_CH/PPM inputs | NO | **Input-only** — cannot drive servo/PWM |

**Best reassignment targets** for a typical quad (4 motors, no servos) wanting
servo ch3/4/5 freed off receiver pins: **GPIO 13** (if motor 6 unused), then
**GPIO 5** and **GPIO 18** (if servo ch6/ch7 unused). A pure 4-motor airframe
has 13, 5, 18 all genuinely free.

---

## 6. Open questions for the operator

1. **What receiver protocol is the real ESP32 drone using?** Config default is
   `USE_SBUS_RECEIVER` (re-enabled 2026-05-20). If the airframe actually uses a
   FlySky FS-iA6B (iBUS) — as the `esp32_wiring_fsia6b_drone.md` doc implies —
   then the active build flag is wrong *and* Conflict B becomes the live one,
   not Conflict A. **Confirm: SBUS, iBUS, or DSM?**
2. **How many servos does the airframe physically use?** If 0 (plain quad/hex),
   the conflicts are software-only double-claims — fixable purely by the
   `#error` guard + leaving pins, or by simply not wiring 4/5. If 3+ servos
   are wired (e.g. a fixed-wing / tilt-rotor / gimbal build), the §4 override
   block MUST be applied. **Confirm servo channel count.**
3. **Is `USE_SERIAL_COMMANDS` ever enabled** for the external-flight-computer
   path? If so, GPIO 4 is contested with `SERVO_PIN_3` regardless of receiver.
4. **Which motor channels are populated?** The recommended reassignment pins
   (13, 5, 18) are only free if motors 5/6 and servos 6/7 are unused. Confirm
   the motor count before choosing override targets.
5. **Should the SBUS or the servo move?** Wiring docs uniformly assume SBUS on
   16/17; moving SBUS churns 5 docs. Spec recommends moving the *servos*.
   Operator to confirm no external constraint pins SBUS elsewhere.

---

## Files read (read-only)

- `flight_controller/docs/findings/wiring_guide_audit_2026-05-20.md`
- `flight_controller/include/pin_definitions_esp32.h`
- `flight_controller/include/config.h`
- `flight_controller/src/motors.cpp`, `include/motors.h`, `src/main.cpp`
- `flight_controller/lib/RadioComm/radioComm.h`, `radioComm_rc.cpp`, `radioComm_ext.cpp`
- `flight_controller/platformio.ini`

No source or config files modified. No git commits.
```
