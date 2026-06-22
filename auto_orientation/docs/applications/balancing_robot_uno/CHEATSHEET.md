# Uno-minimal SETUP-mode operator cheatsheet

Bench card for first-time setup. **WHEELS OFF the ground** for the full SETUP session — you will pick the bot up and rotate it.

---

## 1. Hardware preflight

- BNO055 on Uno I²C (SDA=A4, SCL=A5, addr 0x28), powered.
- L298N pins ENA=5, IN1=6, IN2=7, ENB=10, IN3=9, IN4=8; **driver on its own pack** (≥ 4.7 V at logic pin under load).
- Uno on USB; motor leads anchored; no shorts. Full wiring: `../balancing_robot/HARDWARE_SETUP.md`.

## 2. Flash SETUP MODE + open serial (115200 baud)

```bash
pio run -e arduino_uno_tuning -t upload
pio device monitor -b 115200
```

Watch for: `==== SETUP MODE -- calibrate + guided PID tune ====` then a final `READY` ~1.5 s later.

## 3. Calibrate the IMU — `c`

```
+----------------------------------------------------------+
| Send 'c'. Bot disarms. Live `cal=Ssagm` (5 chars, e.g.   |
|   `cal=S3231`; goal `cal=S3333`) ticks 2 Hz.             |
|   1. GYRO   hold bot flat & still on table ~3 s          |
|   2. ACCEL  rotate to each face (top/bot/L/R/fwd/back),  |
|             hold ~3 s each                                |
|   3. MAG    draw a figure-8 in the air several times     |
| Each digit climbs 0..3; all four = 3 is done.            |
| Done -> "CAL OK: 22-byte blob saved to EEPROM" + photo   |
|         block + "READY for 't' to start tuning".         |
| Abort any time: 'q' or 'a' -> "CAL ABORTED".             |
+----------------------------------------------------------+
```

## 4. Tune P -> D -> I — `t`

```
+----------------------------------------------------------+
| Set bot upright. It stays LIVE through tuning (25° tip-  |
| cutoff catches falls; 'a' is always emergency stop).     |
| Send 't' -> STAGE_P: "[P] Ki=Kd=0. +/- Kp. n=next"       |
|  STAGE_P  +Kp until visibly oscillating, -20-30%, 'n'.   |
|  STAGE_D  damp the Stage-P ringing; buzzy = too high.    |
|  STAGE_I  raise Ki to kill steady drift; slow wobble     |
|           (<1 Hz) = too high. 'n' -> REVIEW.             |
|  REVIEW   'w'=save, 'q'=discard, 'b'=back.               |
| '*' toggles coarse x5 / fine. 'r' resets the current     |
| term to its stage-entry value. 'b' goes back one stage.  |
+----------------------------------------------------------+
```

## 5. Save & photo-backup

- On `w` you'll see `SAVED. Reboot to fly.` then a block bounded by `==== PHOTO-BACKUP -- paste into balance_constants.h ====` / `==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ====`.
- **Take a photo of the scrollback now.** Block contains `BALANCE_KP/KI/KD`, `PITCH_OFFSET_DEG`, and `BNO055_CAL_BLOB[22]`.
- If EEPROM ever wipes: paste those lines into the PHOTO-BACKUP HARDCODE SITE in `balance_constants.h` and reflash.

## 6. Fly — flash OPERATIONAL MODE

```bash
pio run -e arduino_uno_minimal -t upload
```

Watch for `==== OPERATIONAL MODE -- flying on EEPROM values ====` + your tuned `Kp/Ki/Kd/off`, then `READY`. Place bot upright, send `g` (`ARMED`), release. Success: ≥ 30 s balance, hands off.

## 7. Trouble

| Symptom                                  | Fix                                                                  |
|------------------------------------------|----------------------------------------------------------------------|
| `arm() rejected: cal missing.`           | Reflash `arduino_uno_tuning`, run `c`; OR send `F` to force-arm.      |
| Bot tips immediately / drives wrong way  | Swap `(IN1,IN2)` and `(IN3,IN4)` in `main.cpp` motor_pins; reflash.   |
| `cal=` stuck, never reaches 3/3/3/3      | Figure-8 in 3D; rotate fully through all six accel faces.             |
| Serial silent / garbage                  | Wrong baud — set monitor to **115200**.                               |
| Flash overflow on `pio run`              | Don't add features — Uno-minimal is flash-budgeted by design.         |

## 8. Command reference

| Cmd     | Action                                                                  |
|---------|-------------------------------------------------------------------------|
| `c`     | Guided BNO055 cal; save 22-byte blob + photo-backup (IDLE).              |
| `t`     | Start tuning -> STAGE_P (IDLE).                                          |
| `+` `-` | Raise / lower the current stage's gain one step.                         |
| `*`     | Toggle coarse (x5) / fine step size.                                     |
| `n` `b` | Lock current term, advance / back one stage.                             |
| `r`     | Reset current term to its stage-entry value.                             |
| `w` `q` | Save -> EEPROM + print photo-backup (REVIEW) / quit without saving.      |
| `s` `p` | One-line status + photo-backup snapshot / toggle 10 Hz telemetry.        |
| `a` `g` | Emergency stop (latched) / re-arm after stop (clears integral).          |
| `F`     | **Flight build only** — force-arm despite missing cal (drift expected).  |

---

When in doubt, run [FIRST_SUCCESS_UNO.md](FIRST_SUCCESS_UNO.md) for the full step-by-step. Background: [README.md](README.md).
