# Self-Balancing Robot — User Guide

**The Phase 4 reference application: a two-wheel inverted-pendulum robot that calibrates itself.**

- **Document Status**: v1.0 — first user-facing release
- **Target Audience**: Builders with an Arduino Mega, an IMU breakout, and an afternoon
- **Tested On**: Arduino Mega 2560, BNO055 (Adafruit) and BNO085 (Adafruit), L298N motor driver, 2× TT geared DC motors
- **Time Required**: 60–90 minutes (most of which is wiring; the bot calibrates itself in under a minute)

---

## Table of Contents

1. [What is this?](#what-is-this)
2. [Hardware requirements](#hardware-requirements)
3. [Pin map at a glance](#pin-map-at-a-glance)
4. [First flash](#first-flash)
5. [After flash: what to expect](#after-flash-what-to-expect)
6. [Calibration workflow at a high level](#calibration-workflow-at-a-high-level)
7. [Daily operation](#daily-operation)
8. [Going further](#going-further)

---

## What is this?

The balancing-robot reference application is a complete, end-to-end build that exercises every Phase 4 module in one demo:

- The **BNO085** (or **BNO055**) reads orientation at 200 Hz.
- A **2-state Kalman filter** fuses accelerometer + gyro into a clean pitch estimate.
- **One-shot mounting calibration** captures the IMU's orientation relative to the chassis the first time you press the button.
- An **online adaptive offset** tracks slow mechanical drift (cable drag, payload shift, battery sag) while the bot is running.
- A **relay-feedback auto-tuner** (Åström–Hägglund) figures out the PID gains on its own — you do not need to hand-tune.
- A **state machine** drives the entire workflow via a single button, an LED, and an optional piezo buzzer, so the bot is fully **USB-untethered**.

The result: you bolt everything together, flash once, press the button twice, and the bot stands up. No PID tuning notebooks. No serial cable hanging off the back fighting the balance loop.

> For the design rationale, read [findings/MASTER_DESIGN.md §4.7](../../findings/MASTER_DESIGN.md) and the linked research notes.

---

## Hardware requirements

### Bill of materials

| Item | Qty | Notes |
|------|-----|-------|
| Arduino Mega 2560 | 1 | Main controller. `arduino_mega_balancing` build env. |
| BNO055 **or** BNO085 (Adafruit breakout) | 1 | I2C mode. BNO055 is the recommended default for Phase 4; BNO085 also supported. |
| L298N dual H-bridge module | 1 | Drives the two DC motors. Most generic eBay clones are fine. |
| TT geared DC motor (3–6 V) | 2 | Yellow-gearbox style, matched pair. |
| Robot chassis (2-wheel) | 1 | Any chassis that stands ~10 cm tall with a flat top. See [HARDWARE_SETUP.md](HARDWARE_SETUP.md#chassis). |
| 18650 Li-Ion cell + TP4056 + MT3608 boost | 1 set | Battery and 5 V boost. Details in [HARDWARE_SETUP.md](HARDWARE_SETUP.md#battery-and-power). |
| Tactile push button | 1 | Momentary; one leg to pin 4, other leg to GND. |
| Piezo buzzer (optional) | 1 | 5 V passive buzzer on pin 11. Optional but strongly recommended for audible state feedback. |
| Status LED | 1 | Pin 13 already has the Mega's built-in LED. No external LED needed unless your enclosure hides the built-in. |
| Jumper wires | ~20 | Mix of M-M and M-F. |

### Software prerequisites

- PlatformIO Core (`pip install platformio`) — see [getting_started/GETTING_STARTED.md §Part 2](../../getting_started/GETTING_STARTED.md) for full setup.
- Python 3.7+ for the monitor scripts in `tools/`.

If you have already followed the framework's [Getting Started guide](../../getting_started/GETTING_STARTED.md), you have everything.

---

## Pin map at a glance

The full wiring, ASCII layout, and safety notes live in [HARDWARE_SETUP.md](HARDWARE_SETUP.md). Quick reference:

| Pin | Role | Connects to |
|-----|------|-------------|
| 20 (SDA) | I2C data | IMU SDA |
| 21 (SCL) | I2C clock | IMU SCL |
| 3.3V | IMU power | IMU VCC |
| GND | Common ground | IMU GND, L298N GND, battery GND |
| 5 | Motor A enable (PWM) | L298N ENA |
| 6 | Motor A IN1 | L298N IN1 |
| 7 | Motor A IN2 | L298N IN2 |
| 9 | Motor B IN3 | L298N IN3 |
| 8 | Motor B IN4 | L298N IN4 |
| 10 | Motor B enable (PWM) | L298N ENB |
| 4 | Button input | Pushbutton → GND (internal pull-up) |
| 13 | Status LED | Built-in Mega LED |
| 11 | Buzzer (optional) | Piezo + lead |

> The IMU sits on the Mega's hardware I2C bus (pins 20/21). If you are stacking an OLED display on `Wire1` instead of dedicated wires, see [findings/multi_mcu_port_strategy.md §8](../../findings/multi_mcu_port_strategy.md).

---

## First flash

Assuming you have wired the bot per [HARDWARE_SETUP.md](HARDWARE_SETUP.md) and connected the Mega over USB:

```bash
cd auto_orientation
pio run -e arduino_mega_balancing -t upload
```

This build pulls in:

- `USE_BALANCING_ROBOT` — compiles `src/applications/balancing_robot/`
- `USE_BNO055` (or `USE_BNO085`) — IMU driver
- `USE_TUNER_RELAY` — the relay-feedback auto-tuner
- `ENABLE_TETHERLESS_UX` — button / LED / buzzer state machine

If the upload succeeds you will see PlatformIO's `=========== [SUCCESS] ===========` line. If it fails, see the build/upload section of [getting_started/GETTING_STARTED.md §Part 2](../../getting_started/GETTING_STARTED.md) — the troubleshooting there applies identically to this build env.

---

## After flash: what to expect

### Serial output

Open the monitor (still on USB for the first boot):

```bash
python3 tools/simple_monitor.py /dev/ttyACM0
```

You should see, in order:

```
Initializing IMU (BNO055)...           [or BNO085]
IMU OK
Loading mounting calibration... NONE  (first boot — no saved data yet)
Loading PID gains... DEFAULTS         (first boot — no tuned gains yet)
State: IDLE
READY
```

After **READY** the bot is sitting in `IDLE` waiting for you to short-press the button.

If you see anything other than `READY` after ~5 seconds, jump to [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

### LED state codes (no serial monitor required)

| State | LED pattern | What it means |
|-------|-------------|---------------|
| `IDLE` | solid on | Booted, waiting for a short-press |
| `CAPTURE_MOUNTING` | slow pulse (1 Hz) | Capturing chassis tilt offset; hold the bot still |
| `AUTO_TUNE` | fast blink (5 Hz) | Running relay-feedback PID tune; leave the bot on its stand |
| `RUN` | solid on | Balancing |
| `SAFE_FALL` | rapid blink + buzzer | Fell over; lift it back up and press the button to re-arm |
| `FAULT` | SOS pattern | Sensor error, EEPROM CRC fail, or motor stall — see [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

The buzzer chirps once on every state transition so you can listen from across the room.

### Button-press flow

```
First time ever (no saved calibration):
  Power on → IDLE
  short-press → CAPTURE_MOUNTING → (auto) → AUTO_TUNE → (auto) → RUN

Every subsequent power-on:
  Power on → IDLE → RUN  (loads saved mounting + gains; one short-press to arm)

Force a recalibration:
  Long-press (≥1 s) from any state → IDLE, clears saved mounting offset
```

Long-press is your "I just changed something physical — recapture" gesture. Anytime you reroute a cable, change battery, swap the IMU mount, or move payload, long-press and run through the capture/tune flow again.

---

## Calibration workflow at a high level

The hands-off flow is:

1. **Power on the bot** on a flat surface.
2. Wait for the **READY** indicator (solid LED, single beep).
3. **Untether USB** before continuing — a USB cable's weight and stiffness changes the balance point. See [findings/tetherless_operation_strategy.md](../../findings/tetherless_operation_strategy.md).
4. **Short-press the button** → bot enters `CAPTURE_MOUNTING` (slow LED pulse).
5. Bot waits a couple of seconds for stillness, then captures the mounting offset.
6. On capture success it transitions to `AUTO_TUNE` (fast LED blink). **Place it on a stand** so it can wobble freely without falling.
7. Bot runs the relay-feedback tuner for ~30 seconds.
8. On tune success it transitions to `RUN` (solid LED) and balances.

Step-by-step instructions, photos of stand setups, and timing details are in [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md).

After the first calibration, all subsequent power-ons go straight to `RUN` (the mounting offset and PID gains are saved to EEPROM).

---

## Daily operation

Once the bot is calibrated:

- **Power on** → it loads the saved calibration in ~100 ms → arms to `RUN` after one short-press.
- **Pick it up while running** → tilt exceeds the safety limit → enters `SAFE_FALL`, motors cut, buzzer wails. Place it upright and short-press to re-arm.
- **It feels sluggish or twitchy after a few weeks** → the online adaptive offset has probably drifted toward its limit. Long-press to clear, then redo the capture (you do not normally need to redo the auto-tune).

For the underlying sensor-level calibration (magnetometer figure-8 motion for BNO085 yaw, etc.), see [calibration/CALIBRATION_GUIDE.md](../../calibration/CALIBRATION_GUIDE.md). For this balancing application, only the **mounting** calibration is application-specific — the IMU's internal sensor calibrations are handled by the framework.

---

## Going further

| You want to… | Read |
|--------------|------|
| Understand the underlying control loop | [findings/balance_point_and_mounting_research.md](../../findings/balance_point_and_mounting_research.md) |
| Understand how the auto-tuner works | [findings/auto_pid_tuning_research.md](../../findings/auto_pid_tuning_research.md) |
| Understand the online drift tracker | [findings/online_adaptive_balance_tracking.md](../../findings/online_adaptive_balance_tracking.md) |
| Understand the tetherless UX | [findings/tetherless_operation_strategy.md](../../findings/tetherless_operation_strategy.md) |
| Port to a different MCU | [findings/multi_mcu_port_strategy.md](../../findings/multi_mcu_port_strategy.md) |
| See what other applications are planned | [findings/application_catalog.md](../../findings/application_catalog.md) |
| Check the framework's overall plan | [findings/MASTER_DESIGN.md](../../findings/MASTER_DESIGN.md), [roadmap.md](../../roadmap.md), [scope.md](../../scope.md) |
| Hit a snag | [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

---

**Last Updated**: 2026-05-12
**Version**: 1.0
**Difficulty Level**: Intermediate (wiring) / Beginner (operation)
**Tested Hardware**: Arduino Mega 2560 + BNO055 + L298N + 2× TT motors
