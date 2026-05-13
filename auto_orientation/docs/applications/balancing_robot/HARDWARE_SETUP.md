# Self-Balancing Robot — Hardware Setup

**Wiring, components, power, and chassis notes for the Phase 4 reference build.**

- **Document Status**: v1.0
- **Target Audience**: Builders wiring the bot for the first time
- **Tested On**: Arduino Mega 2560 + BNO055 (Adafruit) + L298N + 2× TT geared DC motors

---

## Table of Contents

1. [Bill of materials](#bill-of-materials)
2. [Pin map](#pin-map)
3. [ASCII wiring diagram](#ascii-wiring-diagram)
4. [Step-by-step wiring](#step-by-step-wiring)
5. [Battery and power](#battery-and-power)
6. [Chassis](#chassis)
7. [Safety notes](#safety-notes)
8. [Verification before first flash](#verification-before-first-flash)

---

## Bill of materials

| Item | Qty | Notes |
|------|-----|-------|
| Arduino Mega 2560 | 1 | Main controller. Any clone with the standard pinout works. |
| BNO055 or BNO085 breakout (Adafruit) | 1 | I2C mode. Pull ADR/SA0 low for address `0x28`. |
| L298N dual H-bridge module | 1 | Generic eBay clone is fine. Remove the 5 V regulator jumper if your supply is already 5 V. |
| TT geared DC motor (3–6 V) | 2 | Match the pair. ~200 RPM @ 6 V is typical. |
| Motor wheels | 2 | 65 mm rubber, push-fit onto the TT shaft. |
| 2-wheel chassis (≥10 cm tall) | 1 | See [Chassis](#chassis). |
| 18650 Li-Ion cell | 1 | 2500–3500 mAh. |
| TP4056 charge module (USB-C) | 1 | With protection. |
| MT3608 boost converter | 1 | Set output to 5.0 V before connecting. |
| Tactile push button | 1 | Through-hole, momentary. |
| Piezo buzzer (5 V passive) | 1 | Optional but recommended. |
| Jumper wires | ~20 | M-M and M-F. |
| Battery holder for 18650 | 1 | Optional but tidier than soldering tabs. |

The IMU choice (BNO055 vs BNO085) is yours. Both are supported via the framework's runtime polymorphism (see [findings/bno055_driver_and_multi_imu_strategy.md](../../findings/bno055_driver_and_multi_imu_strategy.md)). BNO055 is the Phase 4 default because the build env `arduino_mega_balancing` flips `-DUSE_BNO055`.

---

## Pin map

### Arduino Mega 2560 — full assignment

| Pin | Direction | Role | Connects to |
|-----|-----------|------|-------------|
| 20 (SDA) | bidir | I2C data | IMU SDA |
| 21 (SCL) | out | I2C clock | IMU SCL |
| 3.3V | out | IMU logic supply | IMU VCC |
| GND | — | Common ground | IMU GND, L298N GND, battery GND, button GND |
| 5 | out (PWM) | Motor A enable | L298N ENA |
| 6 | out | Motor A direction 1 | L298N IN1 |
| 7 | out | Motor A direction 2 | L298N IN2 |
| 9 | out | Motor B direction 1 | L298N IN3 |
| 8 | out | Motor B direction 2 | L298N IN4 |
| 10 | out (PWM) | Motor B enable | L298N ENB |
| 4 | in (pull-up) | Button input | Pushbutton → GND |
| 13 | out | Status LED | On-board Mega LED |
| 11 | out | Piezo buzzer (optional) | Buzzer + lead |
| VIN | in | Main 5 V power from boost | MT3608 OUT+ |

> The IMU is on **Wire** (Mega pins 20/21). The framework reserves **Wire1** (pins 22/23 on the SCL/SDA1 header) for an optional OLED display — see [findings/multi_mcu_port_strategy.md §8](../../findings/multi_mcu_port_strategy.md).

### IMU pin map (BNO055 or BNO085 in I2C mode)

| IMU pin | Setting | Wire to |
|---------|---------|---------|
| VCC | — | Mega 3.3V |
| GND | — | Mega GND |
| SDA | — | Mega pin 20 |
| SCL | — | Mega pin 21 |
| ADR / SA0 | LOW (tie to GND) | Mega GND → I2C address `0x28` |
| PS0 / PS1 (BNO055 only) | LOW / LOW | Mega GND → I2C mode |
| P0 / P1 (BNO085 only) | LOW / HIGH | I2C mode |
| INT, RST | (leave floating) | — |

The framework's I2C driver expects address `0x28`. If you must run at `0x29` (ADR HIGH), edit `src/sensors/bno055.cpp` (or `bno085.cpp`) accordingly.

### L298N pin map

| L298N pin | Wire to |
|-----------|---------|
| OUT1, OUT2 | Motor A leads |
| OUT3, OUT4 | Motor B leads |
| 12V (motor supply) | Battery + (after boost, 5 V) — see note below |
| 5V (logic supply) | Mega 5V (only if 5V regulator jumper is **off**) |
| GND | Common ground |
| ENA | Mega pin 5 (PWM) |
| IN1 | Mega pin 6 |
| IN2 | Mega pin 7 |
| IN3 | Mega pin 9 |
| IN4 | Mega pin 8 |
| ENB | Mega pin 10 (PWM) |

> **Jumper note**: Most L298N clones ship with the 5 V regulator jumper **on**. If your motor supply is already 5 V (from the MT3608 boost), leave the jumper **on** and tie the L298N's `5V` pin to the Mega's `5V` rail. If you upgrade to a higher motor voltage (e.g., 2S Li-Ion at 7.4 V), pull the jumper **off** and feed the Mega from its own 5 V supply.

---

## ASCII wiring diagram

```
        ┌─────────────────────────────────────────────┐
        │           Arduino Mega 2560                 │
        │                                             │
        │ 3.3V ──┐                                    │
        │ GND ───┼──┐                                 │
        │        │  │                                 │
        │ Pin 20 (SDA) ────┐                          │
        │ Pin 21 (SCL) ────┼──┐                       │
        │        │  │     │  │                       │
        │ Pin 4 ─┼──┼──┐  │  │                       │
        │ Pin 13 (built-in LED)                      │
        │ Pin 11 ┼──┼──┼──┼──┼──┐                    │
        │        │  │  │  │  │  │                    │
        │ Pin 5 (PWM) ──┐    │  │                    │
        │ Pin 6 ────────┼──┐ │  │                    │
        │ Pin 7 ────────┼──┼─┼┐ │                    │
        │ Pin 8 ────────┼──┼─┼┼─┼┐                   │
        │ Pin 9 ────────┼──┼─┼┼─┼┼┐                  │
        │ Pin 10 (PWM) ─┼──┼─┼┼─┼┼┼┐                 │
        │ VIN ◄── 5V from MT3608 boost                │
        └────────│──┘  │ │  │ │││││──────────────────┘
                 │     │ │  │ │││││
                 │     │ │  │ │││││
        ┌────────┴─────┼─┼──┼─┼┼┼┼┼─┐
        │  BNO055 / BNO085           │
        │   VCC ← 3.3V               │
        │   GND ← GND                │
        │   SDA ← Pin 20             │
        │   SCL ← Pin 21             │
        │   ADR ← GND (→ 0x28)       │
        └────────────────────────────┘

                       ┌────────────────────────────┐
                       │  L298N H-Bridge            │
                       │  ENA ← Pin 5  (PWM)        │
                       │  IN1 ← Pin 6               │
                       │  IN2 ← Pin 7               │
                       │  IN3 ← Pin 9               │
                       │  IN4 ← Pin 8               │
                       │  ENB ← Pin 10 (PWM)        │
                       │  12V ← Boost +5V           │
                       │  GND ← GND                 │
                       │  OUT1/2 → Motor A          │
                       │  OUT3/4 → Motor B          │
                       └────────────────────────────┘

   Button:                       Buzzer (optional):
   Pin 4 ───┐                    Pin 11 ──── (+) buzzer (-) ── GND
            ├── pushbutton
            └── GND

   Battery / power tree:
   18650 Li-Ion (3.0–4.2 V)
       │
       ├── TP4056 (USB-C charge) ──── USB-C charging port
       │
       └── MT3608 boost (set to 5.0 V) ───► Mega VIN + L298N 12V pin + 5V rail
```

---

## Step-by-step wiring

Work through this in order. Test power **before** plugging anything sensitive in.

### 1. Mount the chassis

- Stand the chassis upright, wheels-down.
- Attach motors to the chassis brackets.
- Push wheels onto the motor shafts.
- Place the Mega on the top deck, secured by standoffs or double-sided tape.

### 2. Wire the IMU

```
IMU VCC  → Mega 3.3V
IMU GND  → Mega GND
IMU SDA  → Mega pin 20
IMU SCL  → Mega pin 21
IMU ADR  → Mega GND  (selects address 0x28)
```

For BNO055 also tie `PS0` and `PS1` to GND (I2C mode).
For BNO085 also tie `P0` to GND and `P1` to 3.3V (I2C mode).

### 3. Wire the L298N to the Mega

```
L298N ENA → Mega pin 5    (PWM)
L298N IN1 → Mega pin 6
L298N IN2 → Mega pin 7
L298N IN3 → Mega pin 9
L298N IN4 → Mega pin 8
L298N ENB → Mega pin 10   (PWM)
L298N GND → Mega GND
L298N 5V  → Mega 5V       (only if L298N regulator jumper is on)
```

### 4. Wire the motors to the L298N

```
Motor A leads → L298N OUT1, OUT2
Motor B leads → L298N OUT3, OUT4
```

Polarity doesn't matter yet — the framework's mounting calibration step will discover which direction is "forward". If you find one motor fights the other after calibration, swap one motor's leads at the L298N.

### 5. Wire the button

```
One leg → Mega pin 4
Other leg → Mega GND
```

The Mega uses its internal pull-up; no external resistor needed.

### 6. Wire the buzzer (optional)

```
Buzzer (+) → Mega pin 11
Buzzer (–) → Mega GND
```

### 7. Wire the power tree

1. Solder battery leads to the 18650 holder.
2. Connect holder `+`/`–` to TP4056 `BAT+`/`BAT–`.
3. Connect TP4056 `OUT+`/`OUT–` to MT3608 `IN+`/`IN–`.
4. **Before connecting the boost output to anything,** power the bare cell, probe MT3608 `OUT+` with a multimeter, and turn the trim pot until it reads **5.00 V ± 0.05 V**.
5. Then connect MT3608 `OUT+` → Mega `VIN` **and** → L298N `12V` (yes, even though it's only 5 V; that pin is just the motor rail input).
6. Connect MT3608 `OUT–` → Mega `GND` → L298N `GND`.

### 8. Verify ground continuity

With a multimeter on continuity / beep mode, confirm:

- Mega GND ↔ IMU GND
- Mega GND ↔ L298N GND
- Mega GND ↔ button leg
- Mega GND ↔ buzzer (–)
- Mega GND ↔ battery (–) after the boost

All five should beep. If any does not, fix the missing wire before powering on.

---

## Battery and power

### Power budget

| Load | Idle | Peak |
|------|------|------|
| Mega 2560 | 50 mA | 80 mA |
| IMU (BNO055 or BNO085) | 12 mA | 12 mA |
| L298N quiescent | 30 mA | 60 mA |
| 2× TT motors @ 50% duty | 150 mA | 800 mA (stall) |
| LED + buzzer + button | <5 mA | 30 mA |
| **Total** | **~250 mA** | **~1.0 A** |

A 2500 mAh 18650 will run the bot for ~6–8 hours of idle / 2–3 hours of active balancing.

### Recommended power topology

```
USB-C in  →  TP4056  →  18650 cell  →  MT3608 boost (5.0 V)  →  Mega VIN + L298N 12V
```

- **TP4056** handles charging and provides cell protection (over-discharge, over-current).
- **MT3608** boosts the 3.0–4.2 V cell voltage up to a steady 5 V regardless of state of charge.
- **Set the MT3608 trim pot to 5.0 V before plugging anything in**. Miswired or unset, it can output 28 V and kill the Mega instantly.

### Why not 2S Li-Ion?

For the Mega + L298N + TT-motors combo, 1S + boost is enough and simpler. If you upgrade to brushed motors that want 7.4 V, switch to 2S and feed the Mega from its own 5 V buck regulator instead. See [findings/tetherless_operation_strategy.md §Power](../../findings/tetherless_operation_strategy.md) for the analogous flight-controller treatment.

### Low-voltage cutoff (software)

The framework reads battery voltage via a 2:1 resistor divider on an ADC pin (configurable in `src/config/pins.h`). Default thresholds:

| State | Voltage (under load) | Action |
|-------|----------------------|--------|
| Healthy | > 3.6 V/cell | Normal operation |
| Warning | 3.4–3.6 V/cell | Slow buzzer chirp + LED double-blink |
| Cutoff | 3.2–3.4 V/cell | Motors off |
| Shutdown | < 3.2 V/cell | Full power down |

If you skip the divider, the framework simply disables the low-voltage logic (the bot runs until the TP4056 protection kicks in around 2.5 V).

---

## Chassis

Anything that:

- Stands roughly **10 cm tall** (taller = lazier balance, easier to learn).
- Has **two wheels** on the same axle, ~10 cm apart.
- Has a **flat top** for the electronics deck (Mega, breadboard, L298N).
- Has a **flat bottom** between the wheels so the bot can rest stably during mounting capture.

The Phase 4 reference was built on a generic eBay "2WD smart car chassis" with one deck removed and the motors stood vertically. Acrylic, 3D-printed, or cardboard works equally well — the IMU does not care.

> **Important**: the IMU should be **rigidly attached** to the chassis, not hot-glued to a wobbly bracket. Vibration coupling between the IMU and the motors degrades the pitch estimate.

---

## Safety notes

⚠ **Do not reverse battery polarity.** The TP4056 has reverse-polarity protection, but the MT3608 does not. Reverse polarity will destroy the boost converter and may short-circuit the cell.

⚠ **Do not power the motors from USB alone.** USB current limits will brown out the Mega the first time the motors stall, restarting it mid-balance. Always run the motor rail from the battery + boost.

⚠ **Set the MT3608 output to 5.0 V before connecting it.** Out of the box it often outputs >20 V. Probe it bare-bones first.

⚠ **Disconnect the battery before rewiring.** Even at 5 V, a short across the L298N's H-bridge will weld jumper wires.

⚠ **Never let the bot balance over your computer or another device.** It is going to fall over during the first calibration attempts.

⚠ **Keep fingers clear of the wheels during auto-tune.** The relay-feedback tuner deliberately drives the motors to limit cycle — the wheels will spin in bursts.

---

## Verification before first flash

Run through this checklist before you upload firmware.

- [ ] Battery installed, MT3608 output measured at 5.0 V ± 0.05 V
- [ ] Mega 5V rail measured at ~5.0 V with battery powered
- [ ] IMU 3.3V rail measured at ~3.3 V
- [ ] I2C address `0x28` confirmed (run an `i2c_scanner` sketch — see [TROUBLESHOOTING.md](TROUBLESHOOTING.md))
- [ ] Button: pin 4 reads HIGH idle, LOW when pressed
- [ ] L298N: ENA/ENB pulled briefly HIGH in a smoke-test sketch spins each motor
- [ ] Both motors spin the same direction when driven the same way (if not, swap one motor's leads)
- [ ] All five ground continuity checks beep

Once everything passes, jump to [USER_GUIDE.md §First flash](USER_GUIDE.md#first-flash).

---

**Last Updated**: 2026-05-12
**Version**: 1.0
**Tested On**: Arduino Mega 2560, BNO055 (Adafruit), L298N module, TT geared DC motors, 18650 + TP4056 + MT3608
