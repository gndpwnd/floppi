# Balance Robot — Quickstart

**Hardware on the bench**: Arduino Uno + BNO055 (I2C 0x28) + L298N motor driver + 2× DC motors + battery pack.

**Firmware status (2026-05-12)**: `arduino_uno_balancing` builds cleanly. Flash 82% / RAM 71% on Uno. Ready to flash.

---

## 1. Wiring (matches the archived `.ino`)

```
BNO055      Uno
 VCC   ->   3.3V (or 5V if your breakout has the regulator)
 GND   ->   GND
 SDA   ->   A4
 SCL   ->   A5
 ADR   ->   GND (forces address 0x28)

L298N       Uno
 ENA   ->   D5  (left motor PWM)
 IN1   ->   D6
 IN2   ->   D7
 IN3   ->   D9
 IN4   ->   D8
 ENB   ->   D10 (right motor PWM)
 +12V  ->   battery +
 GND   ->   battery − and Uno GND (common ground)
 +5V   ->   leave (or wire to Uno VIN if powering Uno from L298N's regulator)

Optional button:  one leg -> D4, other leg -> GND  (uses INPUT_PULLUP)
```

⚠ Confirm motor polarity by hand-spinning: with `IN1=HIGH, IN2=LOW` the left wheel should turn the bot **forward**. If it goes backwards, swap the motor leads.

---

## 2. Flash

```bash
cd /home/devel/floppi/auto_orientation
pio run -e arduino_uno_balancing -t upload
```

If the upload picks the wrong port, add `--upload-port /dev/ttyACM0` (or wherever your Uno enumerates).

After upload, open a serial monitor at 115200 baud:

```bash
pio device monitor -e arduino_uno_balancing -b 115200
```

---

## 3. First-time BNO055 calibration

On a fresh Uno (no EEPROM cal saved yet), boot output:

```
==================================================
 Auto Orientation Framework — Balance Robot Mode
==================================================
[boot] BNO055 init... OK
[cal] no saved BNO055 calibration

=== BNO055 Calibration Wizard ===
Rotate device through all orientations slowly.
Saves automatically when sys/accel/gyro/mag >= 2.
Short-press button to accept partial cal.
Send 'k' over serial to force-skip.

[cal] sys=0 accel=0 gyro=0 mag=0
[cal] sys=0 accel=1 gyro=3 mag=0
[cal] sys=0 accel=2 gyro=3 mag=0
...
```

To get the four accuracies up:

- **gyro → 3**: leave the device still for ~3 seconds
- **accel → 3**: tilt the device into 6 different orientations (each axis up and down), pausing 1–2 s in each
- **mag → 3**: trace a slow figure-8 in the air
- **sys → 3**: composite; gets to 3 only when the others are good

When all are ≥ 2, the firmware auto-saves and continues:

```
[cal] ALL >= 2 — saving
[cal] BNO055 calibration SAVED to EEPROM (22 bytes)

[boot] L298N init... OK

READY
Short-press button -> mounting capture
Long-press button   -> abort (or skip cap. into auto-tune)
Serial: 'c' short, 't' long, 'a' abort, 's' status, 'r' re-cal
```

**Subsequent boots**: the calibration is loaded automatically from EEPROM:

```
[cal] BNO055 calibration restored from EEPROM
```

You can force a re-calibration any time by sending `r` over serial.

---

## 4. Mounting capture

⚠ Critical: **untether the USB cable first** if your bot is small. The cable's torque shifts the balance point by 0.5–1° (see [findings/tetherless_operation_strategy.md](docs/findings/tetherless_operation_strategy.md)). Either: (a) power the Uno from the L298N's 5V regulator + run on serial after power cycle, or (b) use a BLE serial bridge (future Phase 4.8).

If you don't have an untethered setup yet, just be aware the bot will balance ~1° off-center.

1. Hold the bot at its **natural balance point** (gently let it find the angle where it doesn't want to fall either way).
2. Press the button (short, < 1 s) — **or** send `c` over serial.
3. Bot enters `CAPTURE_MOUNTING`. Hold steady for ~2 s.
4. Bot transitions to `AUTO_TUNE`.

If the bot was wobbling during capture, it'll abort and return to `IDLE`. Try again.

---

## 5. Auto-tune

⚠ Place the bot on a **stand** that lets it wobble but not fall. A book on each side works.

1. After mounting capture succeeds, the bot enters `AUTO_TUNE` automatically.
2. The relay-feedback tuner drives the motors with ±150 PWM, recording the oscillation period.
3. Takes ~30 s. Bot will wobble noticeably.
4. On success, gains are applied to the PID and bot transitions to `RUN`.
5. On failure (timeout, tipover beyond ±10°), original gains are restored and bot returns to `IDLE`.

Send `s` over serial to check state and current pitch:

```
[status] state=AUTO_TUNE pitch=2.34 mount_off=-8.42 out=-150
```

---

## 6. Run / balance

After auto-tune, bot enters `RUN` and starts balancing. Send `s` to monitor:

```
[status] state=RUN pitch=0.12 mount_off=-8.42 out=-23
```

- Tipover beyond **35°** → `SAFE_FALL` (motors stop).
- Bot back below **15°** for 30 consecutive samples → recovers automatically to `RUN`.
- Long-press button (or send `t`) → abort to `IDLE` for tweaking.

The online drift estimator silently tracks slow shifts in the balance point (battery sag, cable pull) and adjusts the offset within ±5° of the captured reference. Watch `mount_off` over time to see it adapt.

---

## 7. Troubleshooting

See [docs/applications/balancing_robot/TROUBLESHOOTING.md](docs/applications/balancing_robot/TROUBLESHOOTING.md) for the long list. Quick triage:

| Symptom | First thing to check |
|---------|----------------------|
| Boot stuck at `BNO055 init... ` | Wiring: SDA=A4, SCL=A5; address 0x28 (ADR to GND); 3.3V supply present |
| `cal` numbers never go up | Move device more aggressively; magnetometer needs figure-8 motion |
| Capture state never completes | Bot is moving — place on a solid surface, redo |
| Auto-tune fails immediately | Bot tipped over — needs to be on a stand that limits travel to ±10° |
| Bot balances but wobbles wildly | Auto-tune ran with bot on the floor not a stand → gains too aggressive; `r` to re-cal, then `c`+`t` to redo capture+tune |
| `mount_off` drifts toward ±5° fast | Physical change (cable, payload). Long-press → IDLE, then short-press to re-capture |

---

## 8. What's next

- Hardware: add a button on D4 + an LED on D13 for tetherless operation (see [docs/applications/balancing_robot/HARDWARE_SETUP.md](docs/applications/balancing_robot/HARDWARE_SETUP.md))
- Tuning: experiment with `tune_amplitude` in `BalanceApp::default_config()` (in `src/applications/balancing_robot/balance_app.cpp`)
- Move to Mega: `pio run -e arduino_mega_balancing -t upload` — more RAM headroom, easier to add features
- Move to ESP32: Phase 6 dashboard (planned) — browser UI for cal + tune + telemetry over WiFi

---

## 9. References

- User guide (long form): [docs/applications/balancing_robot/USER_GUIDE.md](docs/applications/balancing_robot/USER_GUIDE.md)
- Calibration workflow detail: [docs/applications/balancing_robot/CALIBRATION_WORKFLOW.md](docs/applications/balancing_robot/CALIBRATION_WORKFLOW.md)
- Hardware setup: [docs/applications/balancing_robot/HARDWARE_SETUP.md](docs/applications/balancing_robot/HARDWARE_SETUP.md)
- Framework design: [docs/findings/MASTER_DESIGN.md](docs/findings/MASTER_DESIGN.md)
- Why the framework: [docs/scope.md](docs/scope.md)

---

*Last updated: 2026-05-12. Quickstart created when the user pivoted from Mega+BNO085 to Uno+BNO055 hardware. Verified: `arduino_uno_balancing` builds at 82%/71% flash/RAM. Ready to flash.*
