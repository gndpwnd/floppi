# Self-Balancing Robot — Troubleshooting

**Symptom → cause → fix for the Phase 4 reference application.**

- **Document Status**: v1.0
- **Target Audience**: Builders who have wired and flashed the bot but are hitting an issue
- **Related**: [HARDWARE_SETUP.md](HARDWARE_SETUP.md), [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md), [USER_GUIDE.md](USER_GUIDE.md)

For framework-wide issues (build failures, serial port problems, sensor-level calibration), see [getting_started/GETTING_STARTED.md §Troubleshooting](../../getting_started/GETTING_STARTED.md) and [calibration/CALIBRATION_GUIDE.md §Troubleshooting](../../calibration/CALIBRATION_GUIDE.md).

---

## Table of Contents

1. [Boot and serial output issues](#boot-and-serial-output-issues)
2. [Calibration / capture issues](#calibration--capture-issues)
3. [Auto-tune issues](#auto-tune-issues)
4. [Balance / RUN issues](#balance--run-issues)
5. [Power and hardware issues](#power-and-hardware-issues)
6. [Online adaptive offset issues](#online-adaptive-offset-issues)
7. [Display and feedback issues](#display-and-feedback-issues)

---

## Boot and serial output issues

### Serial output stuck at "Initializing BNO055"

**Symptoms**:

```
Initializing IMU (BNO055)...
[no further output, even after 10+ seconds]
```

**Most likely cause**: I2C wiring or address mismatch.

**Fixes**:

1. **Run an I2C scanner sketch** to confirm the IMU is on the bus.

   Use this minimal scanner (paste into a fresh `pio init` project, or use the Arduino IDE's built-in `Wire/i2c_scanner` example):

   ```cpp
   #include <Wire.h>
   void setup() {
     Serial.begin(115200);
     Wire.begin();
     for (uint8_t addr = 1; addr < 127; addr++) {
       Wire.beginTransmission(addr);
       if (Wire.endTransmission() == 0) {
         Serial.print("Found device at 0x");
         Serial.println(addr, HEX);
       }
     }
   }
   void loop() {}
   ```

   You should see `Found device at 0x28`.

2. If you see **`0x29`** instead: your ADR/SA0 pin is floating or pulled HIGH. Tie it firmly to GND.

3. If you see **no devices**: re-check SDA/SCL wiring. Pin 20 = SDA, pin 21 = SCL on the Mega. They are easy to swap.

4. If the scanner itself hangs: the IMU is shorting the bus. Disconnect SDA and SCL, confirm the scanner runs (it should print nothing but not hang). Re-add wires one at a time.

5. **3.3V vs 5V**: the BNO055/BNO085 logic must be powered from **3.3V**, not 5V. The Adafruit breakouts have on-board level shifters that tolerate 5V on VIN, but the framework's reference wiring is 3.3V to keep the I2C lines clean.

---

### Serial output garbage / unreadable characters

**Symptoms**: Random characters streaming, no recognisable text.

**Cause**: Baud rate mismatch.

**Fix**: The framework uses **115200 baud** for the Mega. Confirm your monitor command:

```bash
python3 tools/simple_monitor.py /dev/ttyACM0
# or
pio device monitor -b 115200
```

If you opened the Arduino IDE serial monitor at 9600, change it to 115200.

---

### "Loading mounting calibration... CRC FAIL" on every boot

**Symptoms**:

```
Loading mounting calibration... CRC FAIL
Refusing to load corrupt blob. State: IDLE
Please recapture.
```

**Cause**: Either (a) the EEPROM was never written (first boot — expected) or (b) the saved blob was corrupted by a brown-out during write.

**Fix**:

- **First boot**: this message is normal. Run through the calibration in [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md).
- **Persistent across reboots**: your battery is browning out during EEPROM writes. Confirm the MT3608 output is steady 5.0 V under motor load. If it dips below ~4.5 V during writes, replace the boost module or step up to a larger cell.

---

## Calibration / capture issues

### Capture state never completes

**Symptoms**: LED slow-pulses indefinitely. No triple-chirp confirmation.

**Cause**: The bot is not still enough for the stillness gate to trigger. The gate requires all three gyro axes under 0.5 °/s for 500 ms continuously.

**Fixes**:

1. **Use a solid surface**. A wobbly desk transmits enough vibration to keep the gate open. A book on a table, or the floor, works better.

2. **Do not touch the bot during capture.** Even resting a finger on it transmits enough motion. Set it down and step back.

3. **Confirm the IMU is rigidly mounted**. If the breakout board is wobbling on its header pins, you will never pass the stillness gate. Reflow the headers or tape the board down.

4. **Check serial output for `gyro_var = X.XX deg^2/s^2`** during capture. If `gyro_var` is consistently above 0.25 (= 0.5 °/s²), there is a physical vibration source — find and stop it.

---

### Capture completes but the bot leans heavily in one direction during RUN

**Symptoms**: Bot balances but is constantly fighting a lean, motors whining in one direction.

**Cause**: The mounting offset was captured while something was biasing the chassis — USB cable, a hand resting on it, the bot on a non-flat surface.

**Fix**: Recapture per [CALIBRATION_WORKFLOW.md §Recalibration](CALIBRATION_WORKFLOW.md#recalibration-when-and-how). Long-press, untether USB, lay it flat, short-press, hold still.

> The online adaptive offset will eventually pull this out (over ~5 minutes of stable runtime), but it is faster and cleaner to just recapture.

---

## Auto-tune issues

### Auto-tune fails with "no_oscillation"

**Symptoms**:

```
State: AUTO_TUNE
[~10 s passes]
Tune aborted: no_oscillation
Returning to IDLE.
```

**Cause**: The relay-feedback amplitude is too small to actually excite the plant. The motors twitch but never produce a measurable pitch oscillation.

**Fixes**:

1. **Confirm the bot is on a stand**, not the floor. On the floor the wheels grip and the chassis cannot tilt — there is nothing to oscillate. On a stand the wheels spin freely and a small motor pulse produces a visible pitch change.

2. **Check that both motors actually move** during tune. Listen for the tick-tick-tick. If only one motor is moving, the other is miswired (see L298N wiring in [HARDWARE_SETUP.md](HARDWARE_SETUP.md)).

3. **Increase relay amplitude in code** if your motors are weaker than the reference TT motors. Edit `src/applications/balancing_robot/balance_app.cpp`, find the `RELAY_AMPLITUDE_PWM` constant, and double it. Recompile and reflash.

4. **Confirm motor voltage**. If the MT3608 is sagging under load to ~3.5 V, the motors will barely move. Check with a multimeter on the L298N's 12V pin while the tuner is running.

---

### Auto-tune converges but bot oscillates wildly in RUN

**Symptoms**: Bot starts balancing then immediately develops a large, growing oscillation — falls within a few seconds.

**Cause**: The tune was performed with the wheels on the ground (not on a stand). The plant dynamics with the wheels constrained are completely different from the plant with the wheels free; the gains learned in the constrained case are far too aggressive for the free case.

**Fix**:

1. Long-press twice to clear both mounting and PID blobs (per [CALIBRATION_WORKFLOW.md §How to recalibrate](CALIBRATION_WORKFLOW.md#how-to-recalibrate)).
2. Redo capture on a flat surface.
3. **Place the bot on a stand** so wheels are free to spin.
4. Let auto-tune complete.
5. Lift onto the ground and short-press to arm.

This is the single most common new-builder mistake.

---

### Auto-tune fails with "tilt_limit_exceeded"

**Symptoms**:

```
Tune aborted: tilt_limit_exceeded
Pitch went to -12.4° (limit ±10°)
```

**Cause**: The bot fell off the stand mid-tune, or the relay amplitude was too aggressive and pushed it past the safety limit.

**Fixes**:

1. Use a wider or more stable stand.
2. Make sure nothing is touching the bot during tune.
3. If it consistently overshoots even on a stable stand, lower `RELAY_AMPLITUDE_PWM` in code.

---

## Balance / RUN issues

### Bot balances briefly then falls

**Symptoms**: Stands for 5–20 s, then tips over and triggers `SAFE_FALL`.

**Causes and fixes** (in order of likelihood):

1. **Floor friction differs from stand friction**. The tune was probably done on a slippery stand; the floor is grippier and the gains are too soft. Redo auto-tune on a stand with similar grip to the floor (a paper-on-stand setup works well).

2. **Centre of mass too low**. The bot needs to be top-heavy enough that the pitch dynamics are slow. If the battery is at the bottom and nothing is at the top, balancing is mathematically harder. Move the battery up.

3. **Motor backlash**. TT gearboxes have slop. If the motors are flopping back and forth across the dead zone, the controller cannot stabilise. Replace the gearboxes or accept the limit.

4. **Mounting offset drifted**. Check serial: if `mount offset` is at the ±5° hard limit, recapture (long-press → short-press, flat surface).

---

### Bot stands but vibrates constantly

**Symptoms**: Bot is balanced but motors are buzzing/chattering at high frequency.

**Cause**: Kd is too high (oversensitive to gyro noise) or the IMU is loosely mounted.

**Fixes**:

1. **Tighten the IMU mount**. Mechanical wobble adds to gyro noise.
2. **Re-tune** — the relay-feedback tuner is robust against this, but if you manually edited gains they may be wrong.
3. **Check power rail noise**. A noisy 5V supply causes ADC noise that the gyro estimator amplifies. Add a 100 µF capacitor across the Mega's 5V/GND pins.

---

## Power and hardware issues

### Motors spin but in the wrong direction

**Symptoms**: Bot tries to balance but corrects the wrong way and falls immediately, every time.

**Cause**: Motor leads swapped on one or both motors.

**Fix**: Swap the two leads going to **one** motor at the L298N OUT1/OUT2 (or OUT3/OUT4) screw terminals. Confirm by hand-tipping the bot forward — the wheels should briefly spin to push it back to upright. If they spin **away** from upright, swap the leads on that motor.

---

### Bot resets / reboots when motors engage

**Symptoms**: Mega LED blinks off, serial output restarts, every time the motors start drawing serious current.

**Cause**: Brown-out — motor current is dropping the 5V rail below the Mega's minimum (~4.5 V).

**Fixes**:

1. **Check MT3608 output under load** with a multimeter. If it sags below 4.6 V when motors run, the boost module is undersized. Common reasons:
   - 18650 cell is depleted (< 3.4 V) and the boost cannot keep up.
   - You forgot to set the trim pot to 5 V before connecting, and the input is now too low for the converter's duty cycle.
   - Boost module is a cheap clone with poor output capacitance — add a 470 µF electrolytic across OUT+ / OUT–.

2. **Add bulk capacitance** on the L298N's 12V pin. A 1000 µF / 16 V cap soaks up motor spikes.

3. **Upgrade the cell**. A 2500 mAh 18650 may struggle with the 1 A peak; a 3500 mAh cell has lower internal resistance and holds voltage better.

---

### Battery dies in under 1 hour

**Symptoms**: Bot worked fine for 30–40 minutes then quit, even though cell is supposedly 2500 mAh.

**Cause**: Either (a) cell is older than its label suggests, or (b) the bot is drawing more than ~700 mA average (much more than the ~250 mA budget in [HARDWARE_SETUP.md](HARDWARE_SETUP.md#power-budget)).

**Fixes**:

1. **Test the cell capacity** with a hobby charger (the kind with a discharge / capacity test). If the cell measures < 2000 mAh, replace it.
2. **Profile current draw** with a USB power meter inline on the boost output. If average is way over budget, look for a stalled or grinding motor.

---

## Online adaptive offset issues

### Online adaptive offset drifts to limit

**Symptoms**:

```
Mount offset: -4.95°  (warning: near ±5° hard limit)
Online adaptive: FROZEN — refuse to apply further change
```

The bot still balances (because the framework refuses to apply more than ±5° of online adjustment from the reference mounting), but you keep seeing this warning.

**Cause**: A **physical** change has shifted the actual balance point beyond what the online tracker can compensate for:

- New cable routing dragging on the bot.
- New payload mounted off-centre.
- Battery shifted in its holder.
- IMU breakout came loose.

**Fix**: Recapture mounting. Long-press to clear, untether, short-press, lay flat.

Don't fight the limit — the limit exists deliberately. See [findings/online_adaptive_balance_tracking.md §Safety bounds](../../findings/online_adaptive_balance_tracking.md) for the rationale.

---

### Online adaptive offset is jumpy / unstable

**Symptoms**: `mount offset` value bounces by 0.5+ degrees per second in the serial output.

**Cause**: The adaptation rate limiter is misconfigured, or there is a transient disturbance that the tracker is wrongly interpreting as a mounting shift.

**Fixes**:

1. **Confirm rate limit**. The framework's default is 0.5 °/s rate limit. Check serial logs for "rate-limit hit" warnings.
2. **Avoid steady disturbances during the first 60 s of RUN**. The tracker freezes when the integral term saturates, but small persistent pushes still confuse it.
3. **Recapture** to reset the reference and start fresh.

---

## Display and feedback issues

### OLED display missing — should I add one?

**Symptoms**: USER_GUIDE mentions OLED but you don't have one.

**Status**: **Optional component**. The framework's `USE_OLED` flag is **off** by default in `arduino_mega_balancing`. The bot runs identically without an OLED via serial output (USB-tethered) or the LED + buzzer (untethered).

**If you want to add one**: SSD1306 128×32 on `Wire1` (Mega pins 22 SDA1, 23 SCL1) is the supported configuration. See [findings/tetherless_operation_strategy.md §Display upgrades](../../findings/tetherless_operation_strategy.md). Enable in code with `#define USE_OLED` in `src/config/mode.h` and recompile.

---

### Buzzer is silent

**Symptoms**: LED states change correctly but no audio feedback.

**Causes and fixes**:

1. **Wrong buzzer type**. The framework drives the buzzer pin with a PWM tone, expecting a **passive** piezo. If you have an **active** buzzer (one with internal driver), it will only beep when the pin is held HIGH continuously, not when toggled. Replace with a passive piezo.
2. **Polarity swapped**. Passive piezos do not care, but active ones do. Swap leads.
3. **Pin 11 wiring**. Confirm the (+) lead goes to pin 11 and (–) to GND.
4. **`USE_BUZZER` flag disabled**. Check `src/config/mode.h`.

---

### Button does nothing

**Symptoms**: Short-press from `IDLE` is ignored; LED stays solid.

**Causes and fixes**:

1. **Button wired backwards or not to GND**. The framework uses `INPUT_PULLUP` on pin 4 and expects the button to connect pin 4 to GND when pressed. Confirm with a multimeter: pin 4 to GND should read continuity only while button is pressed.
2. **Button bouncing**. The framework debounces at 20 ms — if your button is mechanically very bouncy, increase `BUTTON_DEBOUNCE_MS` in `src/sensors/button_input.cpp`.
3. **`ENABLE_TETHERLESS_UX` disabled** in `src/config/mode.h`. The `arduino_mega_balancing` build env enables it by default; if you're on a custom env it may not.

---

## Last resort: clean reflash

If multiple things are misbehaving and you suspect EEPROM corruption or a flaky upload:

```bash
cd auto_orientation
pio run -e arduino_mega_balancing -t clean
pio run -e arduino_mega_balancing -t upload
```

Then on first boot, hold the button down **while powering on**. This triggers a one-time "factory reset" path that clears all calibration blobs from EEPROM and drops you back to `IDLE` with a fresh slate.

---

## Still stuck

1. Re-read [USER_GUIDE.md](USER_GUIDE.md) and [HARDWARE_SETUP.md](HARDWARE_SETUP.md) — about 80% of new-builder issues are wiring slips.
2. Check the framework-wide troubleshooting at [getting_started/GETTING_STARTED.md §Troubleshooting](../../getting_started/GETTING_STARTED.md).
3. Read the relevant research note in [findings/](../../findings/INDEX.md) — they document the design intent and common pitfalls.
4. Open an issue with: serial log (full output from boot through failure), photo of wiring, MT3608 output voltage under load, and which step in [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md) you got to.

---

**Last Updated**: 2026-05-12
**Version**: 1.0
**Related**: [USER_GUIDE.md](USER_GUIDE.md), [CALIBRATION_WORKFLOW.md](CALIBRATION_WORKFLOW.md), [HARDWARE_SETUP.md](HARDWARE_SETUP.md)
