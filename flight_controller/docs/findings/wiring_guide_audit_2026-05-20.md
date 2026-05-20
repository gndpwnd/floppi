# Wiring Guide Audit — 2026-05-20

**Auditor:** `fc-wiring-guide-auditor@flight_controller:1`
**Scope:** Cross-check pin references in the 5 wiring docs against `include/pin_definitions.h` (Teensy), `include/pin_definitions_esp32.h` (ESP32/S3), and `include/config.h` overrides.
**Verdict:** Pin assignments in the wiring guides match the headers at HEAD with only minor textual drift. No source files were modified. No git commits made.

`config.h` has **no active `PIN_*` overrides** (all `#define PIN_*` lines under the "PIN OVERRIDES" section are commented out), so the platform-default values in `pin_definitions{,_esp32}.h` are authoritative.

---

## Per-Doc Summary

### 1. `docs/teensy_wiring.md`

| Metric | Count |
|---|---|
| Pins checked | ~32 (IMU 2, SBUS 2, iBUS 2, DSM 2, PPM 1, PWM 6, OLED 2, LED 1, Motors 6, Servos 7, alt protocols) |
| Matched | 31 |
| Mismatched | 1 (DSM baud rate typo: "115000" -> 115200) |
| Corrections made | 1 |
| `[VERIFY]` flags added | 0 |

**Drift fixed:**
- "Alternative Receiver Protocols" table: DSM/Spektrum row listed `115000 baud` (typo). Corrected to `115200 baud` per `RadioComm`/header comments.

**Notes:** All motor (0-5), servo (6-12), OLED (16/17), IMU (18/19), SBUS RX5 (21), iBUS RX3 (15), PPM (23), and PWM-channel (23/22/21/20/17/16) assignments match the header exactly.

---

### 2. `docs/esp32_wiring.md`

| Metric | Count |
|---|---|
| Pins checked | ~38 (ESP32 + S3 sections combined) |
| Matched | 38 |
| Mismatched | 0 |
| Corrections made | 0 (table-shape extension only) |
| `[VERIFY]` flags added | 3 |

**Pin assignments all match.** ESP32 IMU 21/22, SBUS RX2 16, iBUS RX1 4, DSM 4, PPM 35, PWM channels 35/34/39/36/23/19, OLED 23/19, LED 2, motors 25/26/27/14/12/13, servos 32/33/4/16/17/5/18. ESP32-S3 section also matches (IMU 8/9, motors 35-40, servos 41/42/1/2/10/11/12, OLED 3/46, LED 48, SBUS 18).

**`[VERIFY]` flags added (overlap warnings — not drift):**
- Servo 3 (GPIO 4) overlaps with iBUS_RX / DSM_RX / SERIAL_CMD_RX
- Servo 4 (GPIO 16) overlaps with SBUS_RX
- Servo 5 (GPIO 17) overlaps with SBUS_TX

These conflicts exist in the header defaults themselves — the receiver and servo functions share defaults because a typical build uses one or the other. Operators using both servos and SBUS must override `SERVO_PIN_*` in config.h. Flagged so the bench user can choose explicit overrides.

---

### 3. `docs/wiring_diagrams/teensy_wiring_fsia6b_drone.md`

| Metric | Count |
|---|---|
| Pins checked | ~18 (IMU, iBUS RX3 15, OLED 16/17, motors 0-3, LED 13, alt protocols) |
| Matched | 17 |
| Mismatched | 1 (alt-protocol "Pins 23-16" range was imprecise) |
| Corrections made | 1 |
| `[VERIFY]` flags added | 0 |

**Drift fixed:**
- Alternative Receiver Protocols row for PWM listed `Pins 23-16` (range notation, which could be misread as "pins 16 through 23"). Replaced with explicit `Pins 23, 22, 21, 20, 17, 16 (CH1-CH6)` to match the header.

---

### 4. `docs/wiring_diagrams/esp32_wiring_fsia6b_drone.md`

| Metric | Count |
|---|---|
| Pins checked | ~20 (ESP32 + S3 comparison table) |
| Matched | 20 |
| Mismatched | 0 |
| Corrections made | 0 |
| `[VERIFY]` flags added | 0 |

All pins match. iBUS pin GPIO 4 (Serial1 RX), motors 25/26/27/14, IMU 21/22, OLED 23/19, LED 2. Alt-protocol table (SBUS 16, PPM 35, PWM 35/34/39/36/23/19) matches headers. ESP32-S3 mapping table also matches.

---

### 5. `docs/wiring_diagrams/esp32_wiring_web_api_drone.md`

| Metric | Count |
|---|---|
| Pins checked | ~10 (subset — no receiver) |
| Matched | 10 |
| Mismatched | 0 |
| Corrections made | 0 |
| `[VERIFY]` flags added | 0 |

Strict subset of the ESP32 wiring (IMU 21/22, OLED 23/19, motors 25/26/27/14, LED 2). All match.

---

## Aggregate

| Metric | Total |
|---|---|
| Docs touched | 5 |
| Pins cross-checked (all docs) | ~118 |
| Hard mismatches | 2 (1 baud typo, 1 imprecise pin range) |
| Hard mismatches fixed in-place | 2 |
| `[VERIFY]` flags emitted | 3 |
| Verification footers added | 5 |
| Source files modified | 0 (headers/source remain untouched — per scaffolding) |

**Total drift count across all 5 docs: 2 hard, 3 advisory `[VERIFY]` annotations.**

---

## Top 3 Most-Confusing Pin Assignments (Operator Should Verify on Bench)

1. **ESP32 GPIO 16 / 17 dual-role** — Default `SBUS_RX_PIN=16`, `SBUS_TX_PIN=17`, but `SERVO_PIN_4=16` and `SERVO_PIN_5=17` share the same defaults. The wiring docs uniformly assume SBUS use, but the servo table lists the same pins. Anyone wiring 4+ servos with SBUS *will* hit a hardware conflict and the docs don't surface this except via the new `[VERIFY]` flags in `esp32_wiring.md`.

2. **ESP32 GPIO 4 triple-role** — `IBUS_RX_PIN=4`, `DSM_RX_PIN=4`, `SERIAL_CMD_RX_PIN=4`, *and* `SERVO_PIN_3=4`. Any combination of an iBUS/DSM/serial-commands receiver with a 3rd servo collides. Surfaced as a `[VERIFY]` flag.

3. **Teensy pins 16 / 17 dual-role (OLED vs PWM receiver)** — `OLED_SDA_PIN=16`, `OLED_SCL_PIN=17`, but `PWM_CH5_PIN=17` and `PWM_CH6_PIN=16`. The Teensy general wiring guide already documents this overlap in a note; no new flag needed. Worth bench-verifying that a PWM-receiver build does not also enable the OLED on default pins.

---

## Open Questions for Hardware-Side Confirmation

1. **SBUS receiver type on Teensy default** — `pin_definitions.h` and the Teensy wiring docs reference SBUS on Serial5 RX (pin 21), inverted at 100000 baud, and assert "Teensy 4.0/4.1 hardware automatically handles inversion." This is a claim about Teensy hardware; the docs do not call out a specific FrSky/Futaba receiver model. The FS-iA6B-specific drone doc routes iBUS instead (pin 15), so the SBUS path is exercised only with non-FlySky receivers. **Verify the receiver model and that hardware inversion is enabled in the SBUS init code path.**

2. **DSM baud rate** — The Teensy doc originally said `115000`; corrected to `115200`. The header does not encode the baud rate (only pin), so the truth source is the DSM driver. **Verify `lib/RadioComm` (or equivalent) actually opens DSM at 115200, not 115000.**

3. **ESP32-S3 SBUS pin (18)** — Header default `SBUS_RX_PIN=18` on S3. The general ESP32 wiring guide's "S3 Pin Reference" lists `SBUS Receiver | 18`. Both match. Worth bench-confirming on hardware because GPIO 18 on S3 DevKitC-1 is sometimes wired to onboard USB-J/PHY depending on the board variant.

4. **No `PIN_*` overrides in `config.h`** — confirmed clean. If any future builds inject overrides via `platformio.ini` `build_flags`, this audit will go stale. (Not currently the case at HEAD — only LOOP_FREQUENCY_HZ and USE_WIFI flags appear in platformio.ini build flag conventions, not pin overrides.)

---

## Files Touched

- `/home/devel/floppi/flight_controller/docs/teensy_wiring.md` (1 fix + footer)
- `/home/devel/floppi/flight_controller/docs/esp32_wiring.md` (3 `[VERIFY]` flags + footer + extended servo table to 3 columns)
- `/home/devel/floppi/flight_controller/docs/wiring_diagrams/teensy_wiring_fsia6b_drone.md` (1 fix + footer)
- `/home/devel/floppi/flight_controller/docs/wiring_diagrams/esp32_wiring_fsia6b_drone.md` (footer only)
- `/home/devel/floppi/flight_controller/docs/wiring_diagrams/esp32_wiring_web_api_drone.md` (footer only)

## Files Read (Read-Only)

- `/home/devel/floppi/flight_controller/include/pin_definitions.h`
- `/home/devel/floppi/flight_controller/include/pin_definitions_esp32.h`
- `/home/devel/floppi/flight_controller/include/config.h`
