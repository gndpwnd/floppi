# FIRST SUCCESS — Uno Balance Robot (minimal tier)

**Goal**: brand-new operator, hardware in hand, reaches a first balanced release on the `arduino_uno_minimal` build after walking the guided SETUP-MODE flow.

The Uno tier is **two builds, one loop**: flash `arduino_uno_tuning` (SETUP MODE) to calibrate the IMU and walk P→D→I; flash `arduino_uno_minimal` (OPERATIONAL MODE) to fly. Tuned gains and the BNO055 cal blob persist in EEPROM across the reflash — that is the contract.

Pre-flight: serial monitor open at **115200 baud**. Battery delivering ≥ 4.7 V at the L298N logic pin under stationary load.

---

1. **Wire the bot.**
   - BNO055 on I²C: SDA=A4, SCL=A5, addr 0x28. L298N: ENA=5, IN1=6, IN2=7, ENB=10, IN3=9, IN4=8.
   - Same wiring as Mega minus encoders/button. Canonical reference: [`../balancing_robot/HARDWARE_SETUP.md`](../balancing_robot/HARDWARE_SETUP.md).
   - Expected: no shorts, no smoke, both motor leads firmly anchored.
   - Failure-mode: [`README.md` §2 "Hardware"](README.md#2-hardware) — Uno-specific deltas.

2. **Flash `arduino_uno_tuning` (SETUP MODE).**
   ```bash
   pio run -e arduino_uno_tuning -t upload
   ```
   - Expected serial: `==== SETUP MODE -- calibrate + guided PID tune ====` followed by `Commands: c=calibrate t=tune | + - * n b r w q | a g s p` and `When done: flash arduino_uno_minimal to fly.`
   - Failure-mode: [`README.md` §3 "Building and flashing"](README.md#3-building-and-flashing) — flash budget check (target < 50 %).

3. **Open serial and confirm `READY`.**
   - After the banner the firmware finishes BNO055 + L298N init and prints `READY`.
   - Expected serial: a final `READY` line (`main.cpp:283`) ~1.5 s after the banner — the boot path delays for BNO055 NDOF fusion convergence before arming the 200 Hz PID ISR.
   - Failure-mode: serial shows `ERR BNO055` or `ERR motors` → wiring / I²C bus fault, return to step 1.

4. **BNO055 calibration — `c`.**
   - Send `c`. The bot disarms; the firmware prints the pose script and starts polling cal status.
   - Expected serial: `==== BNO055 GUIDED CAL — disarmed; rotate by hand ====` followed by the 3-line script, then a live `cal=Sssagm` line ticking twice a second.
   - Failure-mode: `cal=` line stuck at `0000` → BNO055 not responding. Power-cycle and verify the I²C scan finds 0x28.

5. **Wait until calibration complete.**
   - Walk the script: hold still (gyro→3), rotate to each face (accel→3), figure-8 in air (mag→3).
   - Expected serial: `CAL OK: 22-byte blob saved to EEPROM` followed by the PHOTO-BACKUP block with `BNO055_CAL_BLOB[22] = { … };` and `READY for 't' to start tuning`.
   - Failure-mode: `CAL FAIL: could not read 22-byte blob` or `CAL FAIL: EEPROM save returned false` → re-run `c`; persistent failure means EEPROM is dying.

6. **Start P→D→I tuning — `t`.**
   - Place the bot upright on a flat indoor surface; the bot stays **live and balancing** through every stage (25° tip-cutoff still catches falls; `a` is the latched emergency stop).
   - Expected serial: `[P] Ki=Kd=0. +/- Kp. n=next` — STAGE_P prompt. From here `+/-` adjust Kp, `n` locks it and advances.
   - Failure-mode: bot tips immediately at STAGE_P entry → cold-start seed in `balance_constants.h` is too far off. See [`README.md` §4.7 "Generating a fresh seed"](README.md#47-value-robustness--every-persisted-value-is-photographable).

7. **Walk through stages.**
   - STAGE_P → raise Kp until visibly oscillating, back off ~20–30 %, press `n`.
   - Expected serial: `[D] Ki=0. +/- Kd. n=next b=back` — STAGE_D prompt. Damp the Stage-P ringing, `n` to advance.
   - Then `[I] +/- Ki. n=review b=back` — raise Ki only enough to kill steady drift; `n` advances to `[REVIEW] w=save q=quit b=back`.
   - Failure-mode: motors get buzzy at STAGE_D → Kd too high, back off with `-`. Slow wobble at STAGE_I → Ki too high.

8. **Save — `w` (and photograph the printed block).**
   - From REVIEW, send `w`. Firmware writes tune to EEPROM.
   - Expected serial: `SAVED. Reboot to fly.` immediately followed by the photo-backup block bounded by `==== PHOTO-BACKUP -- paste into balance_constants.h ====` and `==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ====` containing `BALANCE_KP/KI/KD`, `PITCH_OFFSET_DEG`, and `BNO055_CAL_BLOB[22]` C-array initialisers.
   - **Photograph the scrollback now.** This is the only recovery path if EEPROM is ever wiped — paste into `balance_constants.h` per [`README.md` §4.7](README.md#47-value-robustness--every-persisted-value-is-photographable).
   - Failure-mode: `SAVE FAILED` → EEPROM write fault. Re-try; persistent failure means the chip needs replacement.

9. **Flash `arduino_uno_minimal` (OPERATIONAL MODE).**
   ```bash
   pio run -e arduino_uno_minimal -t upload
   ```
   - Expected serial: `==== OPERATIONAL MODE -- flying on EEPROM values ====` followed by `Kp=<v> Ki=<v> Kd=<v> off=<v>` (the values just saved), then `READY`.
   - If the cal blob is missing from EEPROM: `WARN: no BNO055 cal in EEPROM` — you missed step 4/5. Reflash `arduino_uno_tuning` and re-run `c`.
   - Failure-mode: `Kp=…` line shows the `balance_constants.h` seed values rather than the tuned values → EEPROM tune slot CRC mismatch; re-run steps 2–8.

10. **Bench-release upright + watch for refuse-to-arm WARN.**
    - Send `g` to clear the boot disarm; bot is now live.
    - Expected serial: `ARMED`. The bot begins balancing immediately — no further prompts. Send `s` for a one-shot `armed=1 tipped=0 pitch_dd=<dd> pwm=<v> rdfail=0` status line; send `p` to toggle a 10 Hz periodic telemetry stream.
    - **If you see `WARN: no BNO055 cal in EEPROM` instead of clean arm, you missed step 4/5.** Reflash `arduino_uno_tuning`, run `c`, then return to step 9.
    - Success criterion (roadmap §Phase 4U.5): ≥ 30 s balance on a flat indoor surface, hands off. Failure-mode: [`README.md` §6 "Troubleshooting"](README.md#6-troubleshooting).

---

After first success: re-run the SETUP-MODE flow whenever the bot stops balancing well (battery chemistry change, new wheels, payload shift). The tune lives in EEPROM until overwritten. The Uno's design intent is that the **flash-erase-write cycle is the iteration loop** — there is no on-MCU adaptation, by design.
