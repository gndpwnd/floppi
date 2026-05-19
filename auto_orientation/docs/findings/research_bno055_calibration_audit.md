# BNO055 Calibration Audit

**Status**: code review + recommendations. Aims to answer "is BNO055 cal the reason the bot isn't balancing?"
**Last updated**: 2026-05-12

## 1. The question

The operator's framing, verbatim:

> "is BNO055 calibration off or something?"

The implication is that days of bench iteration on PID gains have not produced a stable balance, and the suspicion has shifted upstream: maybe the controller is fine and the *sensor* is lying. This audit walks the entire calibration code path end-to-end, names every place it can fail silently, and produces a bench-test checklist so the operator can determine on the next session whether the sensor is the culprit — without further code changes.

The audit's conclusion up front: **the code path is structurally sound but has several silent-failure modes that match the symptoms the operator has reported.** None of them are unambiguous from the code alone — they all require a few minutes of bench-side observation to confirm or rule out.

## 2. How BNO055 calibration actually works (mechanism)

The BNO055 runs sensor fusion on-chip in NDOF mode (`OPERATION_MODE_NDOF`, set in `src/sensors/bno055.cpp:97`). NDOF fuses gyro, accel, and magnetometer into a quaternion in real time. The chip is continuously running a Kalman-style estimator that needs per-sensor bias and scale corrections; those corrections live in a 22-byte block of registers (`adafruit_bno055_offsets_t`).

The chip exposes four 0-3 accuracy values via `getCalibration(sys, gyro, accel, mag)` — 0 means no model yet, 3 means saturated good. **Critically: these values fluctuate during operation.** When the chip detects motion that doesn't match its current model, a value can drop 3 → 2 → 1, then climb back. This is normal and was confirmed in the first bench session (`archive/session_records/2026-05-12_uno_balancing_hardware.md:59-61`, issue 3).

What matters is the 22-byte offsets blob captured at a moment of good cal — once saved to EEPROM and restored on boot, every session starts with proven coefficients in place.

## 3. The auto_orientation cal flow — step by step

### 3a. Boot — restore from EEPROM

`main.cpp:setup()` calls `ps::begin(512)` (line 266), then `imu.begin()` (269), then `restore_bno_cal_(imu)` (275). `restore_bno_cal_()` at `main.cpp:171-178`:

1. Calls `restoreFromEEPROM(buf, &len)` (`config/calibration_storage.cpp:116-174`), which reads a 4-byte header from EEPROM offset 0 (marker 0xCA, length, version 0x01, CRC8) and then 22 bytes of payload.
2. Validates the marker and recomputes CRC8 over the payload.
3. Forwards the 22 bytes to `BNO055::setCalibrationProfile()` (`sensors/bno055.cpp:300-317`), which calls `bno_->setSensorOffsets()`.

### 3b. If restore succeeds

The chip starts in NDOF mode with previously-captured offsets loaded. Accuracy values typically take 5-30 s to climb to 2-3, then fluctuate. The mounting offset is loaded from slot 0x200 (`main.cpp:307-314`); if present, the bot enters auto-RUN after a 2 s grace (`main.cpp:320-323`).

### 3c. If restore fails or no cal in EEPROM

`run_bno_cal_wizard_()` runs (`main.cpp:200-238`) — a blocking loop polling `imu.read()` and watching the cal accuracies. Exit condition:

```cpp
if (d.cal_gyro == 3 && d.cal_accel == 3) {
    save_bno_cal_(bno);
    return;
}
```

Escape hatches: `k` skips entirely, `p` force-saves whatever the levels are, button short-press also force-saves. The `gyro=3 AND accel=3` threshold is explicitly relaxed from the gold "all four = 3" because magnetometer can stay stuck at 2 indoors near laptops (session record line 57). For a pitch-only balance bot, gyro and accel are the only axes that matter.

### 3d. Runtime

Every `imu.read()` refreshes `cal_gyro / cal_accel / cal_mag / cal_status` in `OrientationData` (`sensors/bno055.cpp:166-171`). Values can decrease over time. **The balance app does not gate on them** — the control loop reads `pitch_deg` regardless. There is no "wait for cal=3 before balancing" gate.

### 3e. The save path

`save_bno_cal_()` (`main.cpp:180-185`) calls `bno.getCalibrationProfile()`, which returns the 22-byte blob **only if the chip reports fully calibrated** (`sensors/bno055.cpp:285-290` — Adafruit's contract). It then calls `saveToEEPROM(buf, 22)`: 4-byte header + 22-byte payload to slot 0x000, with CRC8 over payload, then `ps::commit()` (no-op on AVR, NVS commit on ESP32).

## 4. The actual failure modes (ranked by likelihood)

### 4a. I²C wiring or pull-up issues → silent read failures

Adafruit BNO055 boards ship with 4.7 kΩ pull-ups. 400 kHz Fast-mode was tried and reverted (`sensors/bno055.cpp:89-93`); at the default 100 kHz those pull-ups are fine. But **loose dupont jumpers, long ribbon cables, and stale solder joints on the breakout pads** are an everyday source of intermittent BNO055 silence.

- **Symptom**: cal stuck at 0; pitch frozen at exactly 0.00 across reads; or pitch jumping tens of degrees with no motion.
- **Why hard to spot**: Adafruit's `setSensorOffsets()` returns `void` — `bno055.cpp:313-316` calls this out. A silent NACK during boot looks identical to a successful restore.
- **Test**: send `s`, note pitch. Wiggle the board by hand. Pitch should track within ~1° of physical orientation. If frozen, I²C path is broken.

### 4b. External-crystal flag mismatched to the breakout

`main.cpp:69-78` makes this user-selectable via `-DBNO055_NO_EXT_CRYSTAL`. Default is `use_ext_crystal=true`, correct for Adafruit (BNO055 1xxx, Stemma QT) but **wrong for CJMCU-055, GY-955, and most generic modules** which lack the 32 kHz crystal. Forcing crystal mode on a no-crystal board freezes the fusion pipeline.

- **Symptom**: pitch stuck at some non-zero value; cal values stuck at 0 even after long motion.
- **Test**: rebuild with `-DBNO055_NO_EXT_CRYSTAL`; see if `s` reports pitch values that change with motion.
- **Visual check**: Adafruit logo + USB-C → has crystal. Bare CJMCU/GY board → no crystal.

### 4c. Mount offset captured at a moment of bad cal

If the operator presses `c` while cal accuracy is in the middle of a drop, the captured offset bakes in whatever transient pitch bias was reported. `OnlineMountingEstimator` should absorb this over the 20 s LPF tau (`main.cpp:302`), but the bot is unstable during that window.

- **Symptom**: bot drifts forward or backward by a small consistent amount; output stays one-sided.
- **Test**: power on, hold still 60 s upright, watch cal stabilize at 3/3, *then* press `c`. Compare against the previous mount offset.

### 4d. Mediocre offsets blob from a hasty wizard

The wizard exits as soon as `gyro == 3 && accel == 3` is observed for a single iteration of the 50 ms polling loop. If both values briefly hit 3 during a motion transient before the chip's confidence is stable, the captured 22-byte blob is biased. The restore-then-fluctuate behaviour looks identical to the operator's "back to zero" issue, except the blob itself is the problem — not runtime fluctuation.

- **Symptom**: cal=3 for the first few minutes post-boot, then drops to 1-2 and doesn't climb back.
- **Test**: send `r` to clear, run the wizard slowly — gyro still 10 s, accel six axis-aligned poses with 3 s pauses, mag figure-8. Watch for both gyro=3 AND accel=3 across at least three consecutive 500 ms wizard prints before save.

### 4e. EEPROM corruption from non-atomic writes

`ps::write()` on AVR (`storage/persistent_storage_avr.cpp:60-74`) loops over `EEPROM.update()` byte-by-byte. ~85 ms total for header + payload. A mid-write cable yank or USB CDC reset corrupts the blob; CRC8 catches it on the next boot. **But on ESP32 the marker can be committed before payload**, so `hasCalibrationInEEPROM()` may report true even with garbage payload. Called out as "wizard pre-load: silent EEPROM corruption from a stale port holder" in the first bench session.

- **Symptom**: boot prints `READY` (implying restore succeeded) but cal stays at 0 forever.
- **Test**: `r` to clear, re-run wizard cleanly with no Python serial monitor in the background.

### 4f. The chip is fine, but the captured mount offset is off by several degrees

This is the most subtle case and likely the best match for current session symptoms. The chip works, pitch updates, cal is 2-3, but the captured mount offset is biased by 3-5°. `OnlineMountingEstimator` needs 60+ s of stable RUN at 20 s LPF tau (`main.cpp:302-303`) to absorb a 5° bias — and the bot will fall multiple times during that window. Additionally, the lessons-learned doc (`LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md:48-51`, point 4) flagged `balance_app.cpp:418-423` passing **placeholder zeros** for the estimator's freeze gates. Phase 4 partly addressed this; worth re-verifying.

- **Symptom**: bot balances briefly (1-3 s) then drifts consistently in one direction.
- **Test**: run for 2+ minutes upright (catch falls and re-prop manually); watch `s` for `mount=` trending toward zero net bias.

## 5. What would tell us cal is or isn't the problem — diagnostic protocol

Run this on the next bench session. Five tests, ~5 minutes total.

1. **Power-up boot test**: power-cycle. Note serial output. Did `READY` appear? Did the wizard run, or was the restore silent (means it succeeded)?
2. **Pitch sanity**: send `s`. Note pitch, cal_gyro, cal_accel, cal_mag, cal_status. Pitch should be within 1-2° of the bot's visual upright angle.
3. **Pitch motion**: gently tip the bot forward 10°, then backward 10°. The pitch value in `s` should swing through ±10° smoothly. If it freezes or jumps, sensor read is broken (failure mode 4a or 4b).
4. **Cal-rise test**: hold the bot still for 5 s. Then wave it through pitch axis for 5 s. Then send `s` a few times. cal_gyro should be at or near 3; cal_accel should at least be 1-2 after the motion. If cal_gyro never reaches 3 even after 30 s of variety, the blob restore failed (4e) or the crystal flag is wrong (4b).
5. **Comparison check**: hold the bot at a visually-known angle (e.g., resting on a flat surface, ~0° pitch). The reported pitch value should match within 2°. Significant mismatch → mount offset is wrong (4c, 4f).

**If all five pass → cal isn't the problem. The controller is.** Re-focus on Phase 4.10 (RLS plant identifier) convergence.

**If any fails → specific diagnosis per the failure mode above.**

## 6. Recommendations

Priority order. None mandatory before next session; the diagnostic in §5 is.

1. **Add a "wait for cal" boot gate.** Refuse to enter RUN until `cal_gyro >= 2 && cal_accel >= 2` sustained for 3-5 s. Mirrors Betaflight's gyro-still-on-boot pattern (`flight_controller/docs/findings/auto-calibration-research.md` §1.1). Removes the entire class of "balanced with bad cal" failures.
2. **Surface cal health at boot.** Print one cal-status line after restore: `CAL OK` or `CAL WARNING: gyro=2 accel=3 mag=0 sys=1`. Pass/fail at a glance.
3. **Auto-recapture mount offset when cal first reaches 3 after restore.** Currently one-shot via `c`; should also auto-retake when cal climbs to gold.
4. **Betaflight-style stillness-with-restart on the boot gate.** If motion is detected during cal-rise, reset the wait timer. Bots routinely boot leaning while the operator seats the battery.
5. **Persist cal_age in the EEPROM header.** Refuse or warn on stale blobs (>30 days?). Temperature and ambient magnetic field drift over weeks.
6. **Operator-overridable wizard threshold.** Currently `gyro == 3 && accel == 3` hardcoded at `main.cpp:218`. Allow `p2`/`p3` to force-save at chosen level.
7. **Sentinel-frozen-pitch check.** If `pitch_deg` is bit-identical across 100 ms while raw gyro is non-zero, flag it: fusion hung or I²C dead. Current `BNO055::isHealthy()` (`sensors/bno055.cpp:218-231`) checks last-read recency and sys cal, not value freshness.
8. **Crystal-flag in user docs.** `main.cpp:69-78` is good but only developers see it. Quickstart should ask: "look at your breakout — 4-pin crystal next to the chip? If not, build with `-DBNO055_NO_EXT_CRYSTAL`."

## 7. Cross-references

- `auto_orientation/docs/findings/bno055_driver_and_multi_imu_strategy.md` — sibling driver-strategy research
- `auto_orientation/docs/findings/bno055_latency_and_pitch_fusion.md` — NDOF group delay + raw-gyro D-term path
- `flight_controller/docs/findings/auto-calibration-research.md` — Betaflight/ArduPilot/INAV calibration patterns (the prior art)
- `auto_orientation/docs/archive/LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md` — durable bench notes; points 4 (placeholder zeros) and 6 (`s` first when debugging) directly relevant
- `auto_orientation/docs/archive/session_records/2026-05-12_uno_balancing_hardware.md` §49-61 — original "cal back to zero" + "stuck at 0 for 2 min" troubleshooting
- `auto_orientation/docs/archive/bno085-calibration-persistence.md` — sibling sensor's persistence path (similar 22-byte concept, different chip)

## 8. Open questions for the operator

Worth answering before declaring cal "fine" or "broken":

- **Does this BNO055 board have an external crystal?** Visual silkscreen check. Critical for 4b.
- **Has the wizard ever completed with all four accuracies at 3 in one session, or did it exit early with only gyro=3 and accel=3?** The relaxed two-of-four threshold (`main.cpp:218`) means a never-truly-gold blob produces mediocre restore every boot. Failure mode 4d.
- **Is the I²C cable physically reliable?** Tug-test both connector ends. Loose pins are the #1 silent culprit and invisible to firmware. Failure mode 4a.
- **Other I²C devices on the bus?** OLED, sonar, etc. added since the first session can cause address conflicts or clock stretching.
- **Did `s` ever show pitch at exactly `0.00` while the bot was held tilted by hand?** If yes, 4a is confirmed.

---

**Summary in one sentence**: the calibration code is functional and the EEPROM persistence layer is sound, but there are five plausible silent-failure paths (I²C wiring, crystal-flag mismatch, hasty wizard exit, stale mount offset, and the chip's normal runtime cal-fluctuation being mistaken for failure) — the 5-minute diagnostic protocol in §5 will tell us which (if any) applies.
