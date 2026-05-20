# Bench Session — 2026-05-12 — BNO055 Frozen-Pitch Diagnosis

**LIVE bench session.** Capturing findings in real-time as the diagnosis unfolds. This is the actual root-cause discovery that ended a day-long PID tuning frustration.

## The smoking gun

Captured via `python3 serial_monitor` at 2026-05-12 late evening, with the bot plugged in:

```
[  1.4s] pitch= +1.46°  raw=RUN B 1.46 2.63 58 K=1.280 P=1.00 Kp=50.0/50.0 Kd=20.0/8.8
[  1.7s] pitch= +1.46°  raw=RUN B 1.46 2.63 62 K=1.280 P=1.00 Kp=50.0/50.0 Kd=20.0/8.8
...
[ 30.0s] pitch= +1.46°  raw=RUN A 1.46 2.87 255 K=0.028 P=0.00 Kp=153.7/2316.5 Kd=61.5/405.4
```

**Pitch = exactly 1.46° for 30 consecutive seconds**, sampled every 0.5 s, while the operator was physically tilting the bot through ±10°. The reading never moved. This is pathognomonic of the BNO055 sensor not being read or returning cached values.

## What was happening downstream

The PID kept doing its job on the (frozen, wrong) pitch value:
- Output started at ~60 PWM (Kp=50 × 1.46 offset)
- RLS entered ADAPT mode at t=6.6s, immediately collapsed K_motor to its σ-projection floor (0.025), because regression on a constant input is degenerate.
- Target gains went absurd: Kp_target=2316, Kd_target=405. Closed-form math: `Kp = ω_n²/K = 64/0.025 = 2560` ≈ what we saw.
- Live Kp ramped up via the 5%/s limiter: 50→154 over 30s, still climbing.
- Output saturated at 255 PWM.
- mount-offset estimator slowly crept upward (2.63→2.87) trying to make the I-term go to zero — uselessly, because there was no real error signal.

**Every previous hand-tuning attempt in this session was operating on a frozen sensor input.** No amount of gain adjustment was going to make the bot balance because the controller never knew which way the bot was tilted.

## Why we missed it for so long

1. The `s` status command shows pitch, but in earlier sessions we read it once or twice and didn't compare across reads. A single read looks plausible (1.46° is a normal small-tilt value).
2. The bot WAS visibly moving its motors — which we interpreted as "the controller is alive." It was, but it was reacting to a fixed input bias, not real motion.
3. No earlier diagnostic explicitly asked "tilt the bot, watch pitch update." The BNO055 cal audit recommended exactly this protocol; it took until tonight to actually run it.
4. The mount-offset estimator's slow creep (2.38→2.87 over a minute) looked plausible — could be reasonable adaptation behavior. In reality it was trying to find an offset that would make a constant input look like zero error.

## What the value 1.46° likely represents

The chip booted, performed initial fusion for a brief moment, then froze. The 1.46° is wherever the chip's fusion got to before the lockup.

## Diagnosis: external-crystal flag mismatch

`main.cpp:73-77` defaults to `BNO055(0x28, /*use_ext_crystal=*/true)` unless `-DBNO055_NO_EXT_CRYSTAL` is defined. From the calibration audit doc §4b:

> Adafruit BNO055 / Adafruit Stemma QT BNO055 (ships with 32 kHz crystal): true
> CJMCU-055, GY-955, generic BNO055 modules (no crystal populated): false
> **Forcing external-crystal mode on a board without one will freeze the fusion pipeline (cal values stay at 0).**

The user's symptoms match exactly. Either the board doesn't have a crystal, OR the firmware sets the crystal-use bit too early before the chip is fully initialised, OR the chip's NDOF mode loop hangs after a brief init.

## Action — switch to no-crystal mode

Flash with `-DBNO055_NO_EXT_CRYSTAL` build flag. Test again. If pitch starts tracking physical orientation, we're done.

## Why this is the resolution of the whole session

If pitch was frozen the entire bench iteration, then **none of the PID tuning matters until this is fixed**. The Phase A + Phase B code is correct — it's just been operating on a stuck sensor input. The lessons captured in `LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md` are still valid, but the specific failure mode that prompted them was a sensor issue all along.

Once the BNO055 is producing real readings, the existing controller architecture (raw-gyro D-term, soft-cutoff, RLS auto-tune) should work. We won't know until we re-test.

## Next steps (this session, live)

1. Flash `-DBNO055_NO_EXT_CRYSTAL` build.
2. Re-run tilt test. Verify pitch tracks physical orientation.
3. If yes: prop bot upright, watch RLS converge, document outcome.
4. If still frozen: this isn't the crystal issue — investigate further (I²C wiring, BNO055 alive check via direct register read).

---

## RESOLUTION — crystal flag was the root cause ✓

After flashing with `-D BNO055_NO_EXT_CRYSTAL` and re-running the tilt test:

```
=== SENSOR TRACKING SWEEP TEST ===
[  3.5s] RUN    pitch=  +0.70°
[  4.9s] HELD   pitch=  -2.19°  ← bot grabbed, HELD triggered by gyro motion
[  8.9s] HELD   pitch=  +5.81°
[  9.9s] HELD   pitch=  -9.19°
[ 13.6s] HELD   pitch= +13.56°  ← real tilting
[ 16.7s] RUN    pitch=  +1.68°  ← bot set down, RUN resumed
[ 24.8s] RUN    pitch=  +1.06°  ← settled at rest

samples: 76
pitch range: -9.19° to +13.56° (Δ=22.75°)
VERDICT: sensor working ✓
```

**The BNO055 board does NOT have an external 32 kHz crystal.** It's a generic CJMCU / GY-955 / no-crystal variant, not an Adafruit. The default `use_ext_crystal=true` was forcing the chip to wait for a clock that wasn't there → fusion CPU hung after a single internal-oscillator read produced the 1.46° value we saw frozen.

The fix is permanent for this user's board: keep `-D BNO055_NO_EXT_CRYSTAL` in the `arduino_uno_balancing` build flags. Other boards (Adafruit-branded) will need the flag removed.

Additional observations:
- **HELD detection works correctly**: bot transitioned RUN→HELD when handled, returned to RUN when set down.
- **Mount offset is now wrong**: was captured at 2.63° during the frozen-sensor era. Operator's "level" placement now reads ~1.06°, so PID sees a phantom -1.57° error and pushes motors that direction. **Mount offset must be re-captured with a working sensor before the bot can balance correctly.**

## Lessons (durable)

1. **Always include a "tilt test" as the very first sensor-health check** at the start of any bench session. Frozen pitch on a single read looks normal. Pitch unchanged across a 30 s tilt is pathognomonic.
2. **BNO055 board variants matter.** The crystal flag is the second-biggest gotcha after I²C wiring. Both bench-tested behaviour and the `LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md` doc should warn about this prominently.
3. **EEPROM-persisted mount offset must be invalidated** when the sensor pipeline changes substantively (firmware change to crystal flag, sensor swap, calibration redo). A persistent offset captured against a broken sensor is worse than no offset.
4. **The Phase A + Phase B firmware is structurally correct.** The whole day's struggle was a sensor-init bug, not a controller bug. Everything we built (raw-gyro D-term, real-signal estimator, MsTimer2 ISR, soft-cutoff, RLS auto-tune) is intact and waiting to be validated with a working sensor.

---

## SECOND DIAGNOSIS — Motors weren't powered

After fixing the crystal flag, mount-offset recapture worked (0.95°). RUN auto-entered. Watched a 90-second balance test expecting RLS convergence...

```text
[  1.2s] RUN A pitch=+0.94° mount=+0.95° out=  +0 K=1.280 Kp=50.0
[ 31.6s] RUN A pitch=+0.94° mount=+0.96° out= +24 K=0.025 Kp=212.2
[ 60.7s] RUN A pitch=+0.94° mount=+1.12° out=+148 K=0.024 Kp=852.1
[ 89.9s] RUN A pitch=+0.94° mount=+1.16° out=+255 K=0.022 Kp=2956.0
```

**Pitch EXACTLY 0.94° for the entire 90 seconds.** Output ramped from 0 PWM up to saturation at 255 PWM. RLS collapsed K_motor to its σ-projection floor (0.022) because no actual α response could be observed.

**User confirmed: motors weren't powered.** The L298N was getting PWM signals, but the battery for the motors was off. So the controller's commands had no effect on the bot's orientation.

This explains:
- Why the bot showed no balance dynamics (motors couldn't move it)
- Why RLS collapsed (no excitation → degenerate regression)
- Why Kp ramped to absurd values (closed-form `Kp = ω²/K → ∞` as K hit the floor)
- Why the bot didn't fall over despite commanded motion (no actual motion was happening)

## The two pre-existing bugs and the human-factor bug, all in one session

| Bug | Layer | Discovery | Resolution |
|---|---|---|---|
| BNO055 crystal flag | Sensor init | Tilt test showed pitch frozen | `-D BNO055_NO_EXT_CRYSTAL` build flag |
| EEPROM mount offset contaminated | Firmware state | Pitch=0.94° but mount=3.13° → big phantom error | Re-captured at 0.95° |
| Motors disconnected | Hardware setup | RLS collapsed in balance test | User powers motors next session |

The third one is operator-facing, not a firmware bug. But it cost us another 90 seconds of confused observation. The session-record protocol should mandate **a motor-test (`m`) as the second pre-bench check** after the tilt test.

## What this means for the algorithm

Phase A + Phase B firmware is still structurally correct — but **we have not yet validated it under realistic plant dynamics**. The bench session showed only:
- Sensor read pipeline works (post crystal fix)
- Mount-offset capture works
- HELD detection works (gyro-triggered)
- ABORT-to-IDLE works
- Motor test command runs

We did NOT confirm:
- PID can stabilise a real inverted pendulum
- RLS converges to a meaningful K_motor under disturbance excitation
- Closed-form gain mapping produces stable poles
- Rate-limited gain ramp is fast enough to track plant changes but slow enough not to destabilise

The user's next direction (2026-05-12 evening): **"make a theoretically sound program before we mess with motors."** Translated: prove the algorithm works in a simulator before pulling the bot back out for a fourth bench iteration.

→ See `THEORETICALLY_SOUND_PROGRAM_PLAN.md` for the simulation-first validation plan.

## See also

- [LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md](../LESSONS_LEARNED_BALANCE_BOT_2026-05-12.md) — the doc whose Gotcha #1 (don't trust gains until cal works) is the meta-lesson here
- [findings/research_bno055_calibration_audit.md](../../findings/research_bno055_calibration_audit.md) — predicted exactly this failure mode (§4b)
- [src/sensors/bno055.cpp:88](../../../src/sensors/bno055.cpp) — Wire.begin call
- [src/main.cpp:73-77](../../../src/main.cpp) — the crystal-flag default + `-DBNO055_NO_EXT_CRYSTAL` override
