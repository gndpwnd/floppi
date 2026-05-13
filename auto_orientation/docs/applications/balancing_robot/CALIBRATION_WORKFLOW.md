# Self-Balancing Robot — Calibration Workflow

**The hands-off button/LED/buzzer flow that takes the bot from "just flashed" to "balancing on its own".**

- **Document Status**: v1.0
- **Target Audience**: Builders who have completed [HARDWARE_SETUP.md](HARDWARE_SETUP.md) and flashed `arduino_mega_balancing`
- **Time Required**: ~2 minutes of active interaction (plus ~30 s for the auto-tuner to think)

---

## Table of Contents

1. [What gets calibrated, and why](#what-gets-calibrated-and-why)
2. [Before you start](#before-you-start)
3. [The full flow, step by step](#the-full-flow-step-by-step)
4. [State table reference](#state-table-reference)
5. [Subsequent power cycles](#subsequent-power-cycles)
6. [Recalibration: when and how](#recalibration-when-and-how)
7. [Verifying success](#verifying-success)

---

## What gets calibrated, and why

This application performs **two** calibrations beyond the IMU's internal sensor calibration:

| Calibration | What it measures | When it happens |
|-------------|------------------|-----------------|
| **One-shot mounting offset** | The chassis tilt at perfect balance (i.e., where on the pitch axis the bot's centre of mass is directly over the wheel axis) | Once, on the bench, when you press the button |
| **Auto-PID tune** | Kp, Ki, Kd for the pitch loop | Once, on a stand, immediately after mounting |
| **Online adaptive offset** | Slow drift in the mounting offset due to cable shift, payload, battery sag | Continuously, while the bot is in `RUN` |

The first two are one-shot bench calibrations triggered by a button press. The third runs invisibly in the background once the bot is balancing. Design details are in [findings/balance_point_and_mounting_research.md](../../findings/balance_point_and_mounting_research.md), [findings/auto_pid_tuning_research.md](../../findings/auto_pid_tuning_research.md), and [findings/online_adaptive_balance_tracking.md](../../findings/online_adaptive_balance_tracking.md).

If you also want full magnetometer calibration for the BNO085 (for absolute yaw heading), do the figure-8 procedure separately per [calibration/CALIBRATION_GUIDE.md](../../calibration/CALIBRATION_GUIDE.md). It is **not required** for balancing — the balance loop only uses pitch.

---

## Before you start

- [ ] Bot is wired per [HARDWARE_SETUP.md](HARDWARE_SETUP.md).
- [ ] Firmware flashed: `pio run -e arduino_mega_balancing -t upload` returned `[SUCCESS]`.
- [ ] Battery installed, MT3608 set to 5.0 V, ground continuity verified.
- [ ] You have a **stand** ready — a small box, a stack of books, anything that lets the bot wobble freely on its wheels without falling over. The wheels should not touch the ground.
- [ ] You have a **flat surface** for the mounting capture step (a table works fine).
- [ ] USB cable is **not** plugged in. (See step 3 below — this matters.)

---

## The full flow, step by step

### Step 1 — Power on the bot

Press the power switch (or insert the battery if you do not have a switch).

**Expected indication**: LED comes on solid within ~1 s. After ~2 s the buzzer chirps once and the LED stays solid — this is the **READY** signal. State is now `IDLE`.

If you do not see this, see [TROUBLESHOOTING.md §Serial output stuck at 'Initializing'](TROUBLESHOOTING.md#serial-output-stuck-at-initializing-bno055).

> **First-time only**: it is fine to leave USB plugged in for **step 1 only**, so you can watch the serial output to confirm boot succeeded. After confirming, unplug USB before continuing.

### Step 2 — Lay the bot flat on a stable surface

Place the bot on its side or flat on the table — anything except upright. The mounting capture needs the IMU to be **completely still** while it averages samples.

A typical setup: lay the bot face-down (wheels up) on a hardback book.

### Step 3 — Untether USB (CRITICAL)

If USB was plugged in for boot confirmation, **unplug it now**.

> **Why this matters**: USB cables are stiff and apply a small but measurable torque to the chassis. The mounting capture algorithm assumes the bot is in its natural balance configuration. If the cable is pulling on it, the captured offset is biased by the cable's weight, and the bot will lean toward the cable's anchor point in `RUN`. Full design discussion in [findings/tetherless_operation_strategy.md](../../findings/tetherless_operation_strategy.md).
>
> The framework prints a warning to serial if USB voltage is detected when capture is triggered, but it will not stop you.

### Step 4 — Short-press the button

A **short press** (< 500 ms hold) advances the state machine from `IDLE` → `CAPTURE_MOUNTING`.

**Expected indication**: LED starts a slow pulse (1 Hz, ~50% duty). The buzzer chirps once on entry.

### Step 5 — Hold the bot still for 2–3 seconds

The framework's stillness gate watches the gyro. Once all three axes read **< 0.5 °/s** for **500 ms continuously**, it begins averaging the accelerometer samples. The averaging takes another ~200 ms.

**Expected indication**: LED stops pulsing and stays solid for ~100 ms (the "captured!" indication), then the buzzer plays a triple-chirp.

If you bump the bot mid-capture, the stillness gate resets and waits again. The framework will retry indefinitely — just hold it still.

### Step 6 — Auto-transition to AUTO_TUNE

Immediately after a successful mounting capture, the state machine transitions to `AUTO_TUNE`.

**Expected indication**: LED switches to a fast blink (5 Hz). The buzzer plays an ascending two-note chirp.

### Step 7 — Place the bot on a stand

You have a few seconds before the auto-tuner starts driving the motors. Move the bot to your stand so that:

- The wheels can spin freely.
- The bot can tilt forward and back by ~10° without anything catching.
- The bot will not fall onto anything fragile if it does fall (it should not, but be safe).

A good stand is a small upturned box with the bot sitting on the box's edge, wheels overhanging.

### Step 8 — Let the auto-tuner run (~30 seconds)

The relay-feedback tuner deliberately drives the motors in alternating directions to induce a small limit cycle around the balance point. The bot will rock back and forth visibly. This is normal.

**Expected indication during tune**:

- LED fast-blinks the whole time.
- The buzzer chirps every 5 seconds as a heartbeat (so you know it is still alive).
- The motors are audible: a regular tick-tick-tick as they reverse.

The tuner monitors the amplitude and period of the oscillation, applies the Åström–Hägglund formulas, and converges on Kp, Ki, Kd. Typical convergence is **20–40 seconds**.

> **Safety tripwire**: If the bot's pitch exceeds **±10°** during tuning, the tuner aborts and transitions to `SAFE_FALL`. Place it back on the stand and short-press to retry. See [TROUBLESHOOTING.md §Auto-tune fails](TROUBLESHOOTING.md#auto-tune-fails-with-no_oscillation).

### Step 9 — Auto-transition to RUN

When the tuner converges, it saves the gains to EEPROM and transitions to `RUN`.

**Expected indication**:

- LED solid on.
- Buzzer plays a three-note success motif (low-mid-high).
- The motors start active control.

### Step 10 — Balance

Lift the bot off the stand and set it on the ground. It should hold itself upright. Give it a gentle push — it should recover.

**You are done.** The mounting offset and PID gains are saved to EEPROM. Subsequent power cycles skip steps 4–9 entirely.

---

## State table reference

| State | LED pattern | Buzzer | What to do |
|-------|-------------|--------|------------|
| `IDLE` | solid | silent (chirp on entry) | Short-press to begin |
| `CAPTURE_MOUNTING` | slow pulse (1 Hz) | beep on entry | Hold the bot still on a flat surface |
| `AUTO_TUNE` | fast blink (5 Hz) | beep every 5 s | Leave the bot on its stand |
| `RUN` | solid | silent (success chord on entry) | Normal balancing operation |
| `SAFE_FALL` | rapid blink + buzzer | continuous wail | Catch the bot, place it upright, short-press to re-arm |
| `FAULT` | SOS (... --- ...) | SOS in audio | See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

This table is the same one printed in [USER_GUIDE.md](USER_GUIDE.md) — it is reproduced here so you can keep this page open during calibration.

---

## Subsequent power cycles

After the first successful calibration:

```
Power on
   ↓
IDLE  (LED solid, single chirp)
   ↓  short-press
RUN   (LED solid, success chord)
```

The framework loads the saved mounting quaternion and PID gains from EEPROM during boot (~100 ms). Both are sensor-tagged blobs — if you ever swap a BNO055 for a BNO085, the framework refuses to load the BNO055 mounting blob and re-prompts for capture. See [findings/MASTER_DESIGN.md §D2](../../findings/MASTER_DESIGN.md) and `src/config/calibration_storage.h`.

---

## Recalibration: when and how

### When to recalibrate

| Trigger | What to redo |
|---------|--------------|
| Cable rerouted, payload moved, battery swapped | Mounting only |
| IMU swapped (BNO055 ↔ BNO085) | Mounting + auto-tune (gains are sensor-coupled) |
| Motor or wheel replaced | Auto-tune |
| Online adaptive offset has drifted to its ±5° limit (warning beep + log) | Mounting only |
| Bot feels sluggish or twitchy compared to day one | Both — start with mounting |

### How to recalibrate

**Mounting only:**

1. Power on, wait for `READY`.
2. **Long-press the button** (≥ 1 s) from `IDLE` or `RUN`. This clears the saved mounting offset and drops you back to `IDLE`.
3. Short-press to begin capture (steps 4–6 above).
4. The framework detects existing PID gains in EEPROM and **skips** the auto-tune, going straight to `RUN`.

**Full re-tune:**

1. Long-press the button **twice** from `IDLE` (within 1 s of each other) — this clears both mounting and PID blobs.
2. Short-press to begin capture.
3. Run through all of steps 4–9 above.

The double-long-press is intentionally awkward — you do not want to accidentally retune a working bot.

---

## Verifying success

After the first successful flow:

### Visual check

- Bot stands on its own on a flat floor for ≥ 30 s without intervention.
- A gentle push (a finger tap on the top) recovers within ~1 s without falling.
- The wheels make a quiet, continuous correction tick — not a constant whining (too aggressive) or silence (motors disengaged).

### Serial check (USB temporarily reconnected)

```bash
python3 tools/simple_monitor.py /dev/ttyACM0
```

Expected output during `RUN`:

```
State: RUN
Pitch: -0.42°    (close to zero, drifting < 1°)
Kp: 8.20   Ki: 0.31   Kd: 0.92   (your values will differ)
Mount offset: -0.05°  drift_rate: +0.001°/s   confidence: 0.94
Online adaptive: enabled, |estimate - reference| = 0.05°
```

Key things to look at:

- **Pitch** should oscillate around zero. Sustained non-zero pitch means the captured mounting offset was biased — recapture.
- **Mount offset** is the online adaptive estimate. It should be small (under 1°) for at least the first 30 minutes of use, then drift slowly thereafter.
- **|estimate - reference|** must stay under **5°** at all times. If it hits 5°, the framework refuses to apply further adaptation and beeps a warning — that means something physical has changed and you need to recapture.
- **Kp/Ki/Kd** values are sane and stable: typical Phase 4 reference values are Kp ~5–15, Ki ~0.1–1.0, Kd ~0.5–2.0. Wildly different numbers (Kp = 200, Kd = 0) mean the tuner failed and you should redo it on a proper stand.

### Power-cycle check

1. Power off the bot.
2. Wait 10 seconds.
3. Power on. **Do not** trigger calibration — just short-press to arm.
4. Bot should balance again without you doing anything else.

Confirms that mounting + PID gains persisted across the cycle.

---

**Last Updated**: 2026-05-12
**Version**: 1.0
**Related**: [USER_GUIDE.md](USER_GUIDE.md), [TROUBLESHOOTING.md](TROUBLESHOOTING.md), [HARDWARE_SETUP.md](HARDWARE_SETUP.md)
