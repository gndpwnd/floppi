# SelfBallancingRobot3.ino — Dissection Notes

**Source**: `auto_orientation/SelfBallancingRobot3.ino` (user-supplied working sketch, archived 2026-05-12)
**Hardware**: Arduino Mega + Adafruit BNO055 (I2C 0x28) + L298N motor driver + 2× DC motors
**Status**: Working reference implementation. Manual PITCH_OFFSET, manually-tuned PID. To be superseded by the upcoming `src/applications/balancing_robot/` with auto-mounting-angle capture and auto-PID-tuning.

---

## TL;DR — What it does

A minimal pitch-stabilizing balance bot:

1. Reads Euler `pitch` (BNO055 calls it `.z`) at full loop speed.
2. Subtracts a hardcoded `PITCH_OFFSET = -8.6°` (the user's measured mechanical balance angle).
3. Runs a `PID_v1` controller (Kp=65, Ki=12, Kd=38, sample 5 ms, output ±255) on the corrected pitch.
4. Translates output → symmetric L298N motor commands (both wheels driven equally fast, same direction).
5. Has a minimum-speed deadband (15 PWM units) to overcome motor stiction.
6. Stops motors on NaN / unreasonable readings, with consecutive-error tracking.

---

## Code structure

```
setup()                  Init Serial, motor pins, BNO055 (external crystal), PID
loop()                   Read VECTOR_EULER, validate, PID compute, drive motors
controlMotors(speed)     Direction logic + analogWrite to ENA/ENB
stopMotors()             Zero everything
```

---

## Key constants & their meaning

| Symbol           | Value  | Meaning                                                                 |
|------------------|--------|-------------------------------------------------------------------------|
| `PITCH_OFFSET`   | -8.6°  | Mechanical balance angle — **the thing we want to auto-detect**         |
| `Kp`             | 65     | Proportional gain — manually tuned                                      |
| `Ki`             | 12     | Integral gain                                                           |
| `Kd`             | 38     | Derivative gain                                                         |
| sample time      | 5 ms   | PID step (200 Hz)                                                       |
| output limits    | ±255   | Match `analogWrite` 8-bit PWM                                           |
| Min PWM (stiction) | 15   | Below this, motor doesn't move; output snaps to ±15 when nonzero        |
| `abs(rawPitch) < 90` | gate | Sanity-reject readings near gimbal lock / sensor flip                |

The historical/abandoned values in the comments (`Kp: 55/45/150`, `Kd: 32/28/2.5`, `Ki: 10/8/100`) show several manual tuning iterations. **This is exactly the pain that auto-tune solves.**

---

## Pin assignments (Arduino Mega)

| Pin | Direction | Function           |
|-----|-----------|--------------------|
| A4  | I2C SDA   | BNO055 (note: Mega also has dedicated SDA on pin 20 — this sketch uses A4/A5, which on a Mega aliases to nothing — likely runs on Uno wiring assumption). For Mega, prefer 20/21. |
| A5  | I2C SCL   | BNO055              |
| 5   | PWM out   | ENA (left motor PWM) |
| 6   | digital   | IN1 (left motor dir A) |
| 7   | digital   | IN2 (left motor dir B) |
| 8   | digital   | IN4 (right motor dir B) |
| 9   | digital   | IN3 (right motor dir A) |
| 10  | PWM out   | ENB (right motor PWM) |

**⚠️ Pinout discrepancy:** The comment header says "BNO055 SDA -> A4 / SCL -> A5", which is correct for Uno but **on a Mega the I2C pins are 20 (SDA) and 21 (SCL)**. The Wire library will use the hardware I2C peripheral regardless, but the user should physically wire to 20/21 on Mega.

---

## BNO055 ↔ BNO085 mapping (for the upcoming abstraction)

| Concern              | BNO055 (Adafruit_BNO055)                                | BNO085 (our existing driver)                       |
|----------------------|---------------------------------------------------------|----------------------------------------------------|
| Library              | `Adafruit_BNO055`, `Adafruit_Sensor`, `utility/imumaths.h` | `Adafruit_BNO08x` (SH-2)                          |
| Address              | `0x28` (ADR low) or `0x29` (ADR high)                   | `0x4A` / `0x4B`                                    |
| Protocol             | Plain I2C register reads                                | SH-2 over I2C/UART/SPI (much more complex)         |
| External crystal     | `bno.setExtCrystalUse(true)`                            | n/a — BNO085 has internal source                   |
| Fusion mode          | Default after `bno.begin()` is NDOF (9-DOF + mag)       | Set via SH-2 report enable (rotation vector)       |
| Euler output         | `getEvent(&e, VECTOR_EULER)` → `e.orientation.{x,y,z}` where **z is pitch** (Adafruit convention) | Quaternion → Euler in our `quaternion_conversions.cpp` |
| Calibration profile  | 22 bytes (offsets + radii for accel/gyro/mag)           | ~256-byte SH-2 blob                                |
| Save calibration     | `bno.getSensorOffsets(offsets)` → struct                | Our `bno085_calibration.cpp` SH-2 commands         |
| Restore calibration  | `bno.setSensorOffsets(offsets)`                         | SH-2 calibration set commands                      |
| Calibration status   | `bno.getCalibration(&sys, &gyro, &accel, &mag)` (0–3)   | SH-2 report `accuracy` per sensor                  |
| Onboard fusion?      | Yes, runs internally                                    | Yes, runs internally                               |

**Note on Euler convention difference**: BNO055 Adafruit library returns `e.orientation.z` for **pitch** (it ships heading/roll/pitch as x/y/z). Our codebase uses `.pitch_deg`/`.roll_deg`/`.yaw_deg` in `OrientationData`. The BNO055 adapter must remap:

```
OrientationData.roll_deg  = event.orientation.y;
OrientationData.pitch_deg = event.orientation.z;
OrientationData.yaw_deg   = event.orientation.x;   // heading
```

(Verify on hardware — Adafruit docs and source code disagree subtly on axis labels depending on chip orientation.)

---

## Control-loop pattern worth keeping

Things this sketch does right that we should preserve in the production application:

1. **Sanity gate before motor command**: `!isnan(rawPitch) && abs(rawPitch) < 90`. If the IMU dropouts or returns weird data, motors stop. This is a safety property worth promoting to a base-class method on `OrientationData::isValid()`.
2. **Stiction deadband**: snapping `[−14, 14]` PWM range to `±15`. Real DC motors have a non-linearity below stiction torque. The application layer needs a per-platform configurable `MIN_MOTOR_PWM`.
3. **Single sample-time discipline**: PID set to 5 ms; loop has `delay(2)` and a 100 ms print throttle. We should drive the PID step from a `millis()` interval rather than a delay to avoid drift.
4. **Boot-time pause**: `delay(1000)` after BNO055 init. The Adafruit library warns the chip needs ~100 ms after `setExtCrystalUse(true)`. We should make this an explicit `wait_for_sensor_warmup()` rather than a magic delay.

---

## Things to leave behind / fix in the rewrite

1. `delay(2)` in the loop blocks the rest of the system. The auto_orientation main loop is non-blocking; the balancing app must follow that pattern (millis-based scheduler).
2. `while(1)` on `bno.begin()` failure deadlocks the MCU. Should fall through to a safe-stop state with a serial error message every N seconds.
3. The PITCH_OFFSET is a compile-time constant. It belongs in EEPROM (next to the BNO calibration profile) and should be set by an auto-capture routine.
4. PID tuning is by hand. Goal: relay-feedback auto-tune that runs once on demand, writes Kp/Ki/Kd to EEPROM.
5. No safe-fall detection: if the bot tips past, say, ±35°, it has lost balance — we should disarm the motors so they don't fight a tipped-over chassis and burn out.
6. Symmetric wheel drive only — yaw control / line-tracking will need differential wheel commands. Out of scope for v1 balance, but the abstraction should not preclude it.
7. Print formatting uses `Serial.print` with degree symbol and "°" — fine for serial but breaks if the output stream is parsed by tooling. The main project uses JSON output; the balancing app should support both human-readable and JSON modes via the existing `OutputFormat` enum.

---

## Mapping to the new architecture

Where the parts of this sketch will live after refactoring:

| Original concern                          | New location                                                    |
|-------------------------------------------|-----------------------------------------------------------------|
| BNO055 init + Euler read                  | `src/sensors/bno055.{h,cpp}` (new, implements `OrientationSensor`) |
| `PITCH_OFFSET = -8.6` (mounting angle)    | `src/navigation/mounting_calibration.{h,cpp}` (new) + EEPROM    |
| PID controller (`PID_v1`)                 | `src/control/pid_controller.{h,cpp}` (new, generic single-axis) |
| Auto-tune (replaces hand-tuned Kp/Ki/Kd)  | `src/control/auto_pid_tuner.{h,cpp}` (new, strategy-based)      |
| Motor driver (L298N)                      | `src/actuators/l298n_motor_driver.{h,cpp}` (new application-layer) |
| Sanity gate + safe-fall                   | `src/applications/balancing_robot/safety.{h,cpp}` (new)          |
| Top-level state machine (IDLE / CAPTURE / TUNE / RUN) | `src/applications/balancing_robot/balance_app.{h,cpp}` (new) |
| Compile gate                              | `USE_BALANCING_ROBOT` flag in `src/config/mode.h`                |
| Build env                                 | `arduino_mega_balancing` in `platformio.ini`                     |

This list is the implementation backbone for Phase 4 of the auto_orientation project.

---

## What we keep from this sketch as a regression test

A scenario-style integration test in `tests/scenario_test_balancing.cpp` should:

1. Replay a recorded pitch trajectory from a real bot.
2. Run the new PID + auto-mounting-angle code against it.
3. Assert that the motor commands closely match the original sketch's output **when given the same offset**.

This proves the new architecture is feature-equivalent before we change behavior with auto-tune.

---

## References

- Adafruit BNO055 library: `https://github.com/adafruit/Adafruit_BNO055`
- Adafruit Unified Sensor: `https://github.com/adafruit/Adafruit_Sensor`
- Brett Beauregard's PID library (`PID_v1`): `https://github.com/br3ttb/Arduino-PID-Library`
- BNO055 Datasheet: Bosch BST-BNO055-DS000 (sections 3.5 "Fusion modes" and 3.6 "Calibration")
- BNO055 vs BNO085 selection note: see `docs/findings/bno055_driver_and_multi_imu_strategy.md` (forthcoming, agent-written)

---

*Dissection by: auto_orientation framework planning session 2026-05-12.*
