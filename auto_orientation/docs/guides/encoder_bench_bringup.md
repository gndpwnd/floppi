# Wheel Encoder Bench Bring-Up — Mega Balance Bot

Operator procedure for attaching wheel encoders to the Mega bot, wiring them
into the Mega without breaking the L298N or BNO055 wiring, calibrating
counts-per-revolution and wheel radius, and verifying stall detection before
letting the bot run on the outer-loop velocity cascade.

This guide is the encoder counterpart to
[safe_bench_test_workflow.md](safe_bench_test_workflow.md). Read that one first
if you have not yet bench-tested the bot at all — this doc assumes a working
BOOTSTRAP-and-balance loop and only adds the encoder layer on top.

Encoders are a **Mega-only** feature. The Uno does not have enough external
interrupt pins to support two quadrature wheels and is not in scope here.
Build the `mega_balance` env (auto-defines `USE_WHEEL_ENCODERS`); the driver
lives at `src/sensors/wheel_encoder.{h,cpp}`.

Last updated: 2026-05-19.

---

## Section 1 — Hardware shopping

### What to buy

**Recommended: hall-effect quadrature encoders integrated into the motor's
rear shaft** ([research §1](../findings/research_wheel_encoders_mega_2026-05-19.md#1-encoder-hardware-options)).
They survive dust, oil, vibration, and wheel slip; they read correctly when
the motor is back-driven (critical for balance); and they're already on most
modern hobby gear motors.

Two product families work without further thought:

| Motor + encoder combo | Resolution at wheel | Approx. cost (2026) | Source |
|---|---:|---:|---|
| **Yellow-TT motor with encoder** (8 PPR × 120:1 gearbox = **960 CPR**) | ~4.7 counts/mm at 65 mm wheel | $5-7/motor | [DFRobot FIT0458](https://wiki.dfrobot.com/fit0458/) |
| **N20 metal-gear with magnetic encoder** (7 PPR × 100:1 = **2 800 CPR**) | ~10 counts/mm at 43 mm wheel | $8-12/motor | [Adafruit 4639](https://www.adafruit.com/product/4639), [Waveshare DCGM-N20](https://www.waveshare.com/dcgm-n20-12v-en-200rpm.htm) |

If the bot is already assembled and the motors look like 6-pin yellow-TT
units, skip this section — you already have encoders. See "Do my motors
already have encoders?" below.

### Skip these

| Type | Why skip |
|---|---|
| Optical slot-disk (HC-020K) | Disk alignment is fiddly; dust kills it; you have to mount the disk |
| Magnetic absolute (AS5048A, MT6701) | Overkill for balance; extra SPI/I²C wiring; $8-15/wheel |
| Adafruit seesaw I²C decoder | Competes with BNO055 for I²C bandwidth; adds ~4 KB flash |

### Do my motors already have encoders?

Look at the motor connector:

- **6-pin connector** (M+, M-, VCC, GND, A, B) → you have hall encoders.
  Route the A and B wires to Mega INT pins per Section 2.
- **2-pin connector** (M+, M- only) → no encoders. Replace the motors with
  an encoder-equipped variant. Do NOT try to bolt on a separate optical
  encoder; the cost is the same as a replacement motor pair and the result
  is more fragile.

Glue-on magnet + hall-sensor add-on kits exist but are not recommended — the
mounting tolerance is tight and the readings are noisier than factory units.

---

## Section 2 — Wiring procedure

### Pin assignments

These are fixed by the driver and the research doc — do NOT change them
without updating `src/sensors/wheel_encoder.{h,cpp}` and the wiring doc.

| Signal | Mega pin | Mega INT vector |
|---|---:|---:|
| **L_ENC_A** | 18 | INT5 |
| **L_ENC_B** | 19 | INT4 |
| **R_ENC_A** | 2 | INT0 |
| **R_ENC_B** | 3 | INT1 |

### Pin conflict warnings

| Pin | Conflict | Action |
|---|---|---|
| 20, 21 | I²C SDA/SCL → BNO055 | **NEVER** repurpose. Pins 20/21 are unusable for encoders on Mega. |
| 18, 19 | Mega Serial1 TX/RX (latent) | OK for balance build (no GPS). If you later enable GPS or WiFi on Serial1, encoders MUST move pins. |
| 4 | PIN_BTN (operator button) | OK — keep as-is. |
| 5, 6, 7, 8, 9, 10 | L298N ENA/IN1/IN2/IN3/IN4/ENB | OK — encoder pins are physically far enough away on the Mega header that EMI from PWM (~490 Hz) does not bleed into the encoder lines as long as cables are <30 cm and twisted-pair. |

The reference build's motor pin map (verified in
[SelfBallancingRobot3.ino](../archive/balancing_robot_reference/SelfBallancingRobot3.ino))
is ENA=5, IN1=6, IN2=7, IN3=9, IN4=8, ENB=10. No overlap with 2/3/18/19.

### Wiring table

Connect each motor's 6-pin encoder cable as follows:

| Encoder wire | Goes to (left motor) | Goes to (right motor) |
|---|---|---|
| M+ | L298N OUT1 (motor +) | L298N OUT3 (motor +) |
| M- | L298N OUT2 (motor -) | L298N OUT4 (motor -) |
| VCC (encoder power) | Mega **5V** rail | Mega **5V** rail |
| GND (encoder ground) | Mega GND | Mega GND |
| A | **Mega pin 18** | **Mega pin 2** |
| B | **Mega pin 19** | **Mega pin 3** |

Encoder VCC and motor power are SEPARATE. Encoder VCC goes to logic 5V
(Mega's 5V rail or a regulated 5V from the battery). The motor terminals
(M+/M-) carry the full battery voltage through the L298N. **Do not** swap
these — connecting encoder VCC to 12V will destroy the hall sensors.

A printed image diagram lives (will live) at
`docs/wiring_diagrams/mega_balance_with_encoders.md` — for now this table is
authoritative.

### Power

- Encoder VCC draws ~10 mA per hall encoder. Mega's 5V rail handles two
  encoders comfortably (regulator is good for ~500 mA when USB-powered).
- On battery-only operation, route encoder VCC through the same 5V regulator
  that feeds the Mega; do not tap it from the motor supply.

### Pull-ups

The driver enables `INPUT_PULLUP` automatically (PJRC Encoder library does
this in its constructor). Most motor-integrated hall encoders use open-collector
outputs and require the pull-up — leave the default alone. The handful of
push-pull variants are still fine with internal pull-ups enabled.

If you see ticks even when the wheel is stationary (more than the occasional
single-tick bounce), suspect EMI from the L298N PWM lines, not pull-up
strength: shorten the encoder cable, twist A/B together, and route them
away from the motor leads.

---

## Section 3 — First-power verification (before letting the bot move)

Do this before any of the calibration steps. Goal: confirm the encoders
register ticks in the right direction.

1. **Bot lifted off the ground.** Cradle in two stacked books or a cardboard
   stand so wheels spin freely. Motors disabled (bot in IDLE).
2. **Open serial monitor** (`pio device monitor -b 115200 -p /dev/ttyACM0`).
   Pre-flight per [safe_bench_test_workflow.md §2](safe_bench_test_workflow.md#section-2--pre-flight-checklist-cold-start).
3. **Send the encoder-readout command.** *NOT YET IMPLEMENTED.* See
   [Section 11 — Open work](#section-11--open-work-whats-missing). Until that
   command lands, the only way to see ticks is via the per-tick telemetry the
   sibling agent is adding to `step_run_`. Bench test will require waiting
   for that command before this section can be exercised end-to-end.
4. **Hand-rotate the left wheel slowly forward** (the direction the bot would
   roll when leaning forward). Watch the left-tick value.
   - Expected: ticks **increase**.
   - If they decrease: swap `pin_a` and `pin_b` in the `WheelEncoder`
     constructor for the left wheel. Software inversion is intentionally
     not provided — see the driver header comment.
5. **Hand-rotate the right wheel slowly forward**. Watch right-tick value.
   - Expected: ticks **increase**. Same swap rule applies.
6. **Hand-rotate each wheel slowly backward.** Ticks should **decrease** by
   roughly the same amount.

Get both wheels reading the same sign convention before moving on. A wheel
that counts backward will fight the outer-loop cascade and the bot will
drift in the wrong direction at high gain.

---

## Section 4 — CPR calibration

Goal: measure counts-per-revolution for each wheel and save it.

1. **Mark one wheel.** Put a strip of tape on the tire and a matching tape
   mark on the chassis directly above it. The two marks should line up.
2. **Bot lifted, motors off**, serial monitor open.
3. **Zero the tick counts.** *Operator command not yet implemented — see
   Section 11.*
4. **Rotate the wheel by hand exactly one revolution**, ending with the two
   tape marks aligned again.
5. **Read the tick delta.** That is the CPR.
   - Expected for yellow-TT motor: **~960** (within ±10 from hand sloppiness).
   - Expected for N20 motor: **~2 800** (within ±30).
   - If you get something within ±5% of either default, you are fine —
     small variation comes from gear lash, not encoder error.
6. **Save CPR to EEPROM.** *Operator command not yet implemented — see
   Section 11.* For now, edit the driver's default constant or call
   `set_counts_per_rev()` in the application setup.
7. **Repeat for the other wheel.** Some motor pairs have ±5% CPR difference
   between left and right; calibrate them independently, do not assume they
   match.

If the measured CPR is wildly off (e.g. 60 instead of 960), the encoder is
counting only one channel — a wire is disconnected. Re-check Section 2's
wiring.

---

## Section 5 — Wheel radius calibration

Goal: convert ticks to distance.

1. **Measure wheel diameter with calipers** at the widest point of the tire,
   including the rubber tread. Do not measure the bare hub.
2. **Compute radius in metres**: `radius_m = (diameter_mm / 2) / 1000`.
   - 65 mm wheel → `radius_m = 0.0325`.
   - 43 mm wheel → `radius_m = 0.0215`.
3. **Save to EEPROM.** *Operator command not yet implemented — see Section
   11.* Until then, override via `set_wheel_radius_m()` in the application
   setup or edit the driver default.

Measure both wheels separately if they look unequal (worn tires, mismatched
spares). They almost always match within 0.1 mm out of the box.

---

## Section 6 — Distance verification

Goal: end-to-end check that CPR + wheel radius produce correct distance.

1. **Tape measure on a flat surface.** Mark 1.00 m exactly between two
   strips of tape on the floor.
2. **Bot powered, in IDLE, motors disabled.** Zero the tick counts.
3. **Push the bot by hand** from the start mark to the end mark, keeping
   wheels in a straight line.
4. **Read `distance_m()` from the encoder.** *Operator command not yet
   implemented — see Section 11.*
5. **Expected: 0.95 m to 1.05 m** (within ±5%).

| Reading | Diagnosis |
|---|---|
| Within ±5% | Done. Move on. |
| ±5% to ±10% | CPR or radius off by a touch. Re-measure radius with calipers; verify CPR with the Section 4 hand-rotation. |
| > ±10% | Encoder is counting wrong (likely one channel dead — bouncing on a single edge instead of full quadrature). Re-verify wiring; check tick count from Section 3 is consistent with hand-rotation in both directions. |
| Negative number | Sign convention reversed. Swap A/B at the driver level (Section 3 step 4). |

Repeat at 5 m or 10 m if you have the floor space — quantisation error
shrinks with distance, so a long-distance test is a tighter check on radius.
A 1 m test gives ~2% resolution at 4.7 counts/mm; a 10 m test gives 0.2%.

---

## Section 7 — Stall detection bench test

Goal: confirm the firmware enters HELD when wheels are blocked.

The encoder driver tracks "commanded PWM above threshold + zero velocity"
and flags `stalled() == true` ([wheel_encoder.cpp §stall detection](../../src/sensors/wheel_encoder.cpp)).
`balance_app` routes this to `HELD` with `failure_reason = 7 (motor_stall)`
per [MEGA_UNIVERSAL_PLAN §7b](../MEGA_UNIVERSAL_PLAN.md).

Procedure:

1. **Bot on the floor**, wheels free, BOOTSTRAP not yet fired, in IDLE.
2. **Hold both wheels firmly** against a desk or solid surface so they
   cannot turn. Pressure should be enough to oppose at least PWM 150
   (try to push a wheel by hand against the desk — it should not budge).
3. **Trigger a known-PWM test.** *Operator command not yet implemented —
   see Section 11.* The expected command is "drive both motors at PWM 150
   for 1 second" without the inner PID engaged.
4. **Expected: within ~200 ms the bot enters HELD** and prints
   `failure_reason=7 (motor_stall)`. Motors drop to zero.
5. **Release the wheels and lift the bot.** Hold vertical and steady.
6. **Expected: bot exits HELD** and re-enters RUN once the resume gate
   clears (motion-quiet AND level within ±8° for 200 ms — same gate as
   collision-HELD recovery, [safe_bench_test_workflow §4](safe_bench_test_workflow.md#section-4--run-mode-operation)).

If the stall does not fire within 1 second:
- Confirm the encoder is reading zero velocity (Section 3 manual check
  with motors stopped should show no ticks).
- Confirm the commanded PWM is being reported via
  `report_commanded_pwm()` — this is a driver-integration point in
  `balance_app` and may not be wired yet at the time of bring-up.

If the stall fires while the wheels are spinning freely, the velocity
threshold is too high or CPR is wrong (Section 4).

---

## Section 8 — Phase 4M.12 — PWM auto-discovery (queued)

This phase is **planned but not yet implemented** — when it lands, here is
what you will do.

Goal: measure the minimum PWM that produces any wheel motion (`PWM_MIN`,
the stiction floor) and the maximum useful PWM (`PWM_MAX`, the saturation
point where more PWM produces no more velocity). Without these the Python
brute-force tuner has to guess and the search space blows up.

Expected workflow:

1. **Bot lifted off the ground**, wheels free, operator-supervised.
   Cardboard cradle is fine.
2. **Send command `p`** in the serial monitor (letter chosen per
   [MEGA_UNIVERSAL_PLAN §7d](../MEGA_UNIVERSAL_PLAN.md); confirm against
   the firmware build you actually have).
3. **Watch the bot ramp PWM** from 0 upward in steps of 5 every 200 ms,
   per motor independently.
4. **First step with non-zero wheel velocity (>5 dps) → `PWM_MIN`** for
   that motor.
5. **Three consecutive steps with no velocity increase → `PWM_MAX`** for
   that motor (saturation reached).
6. **Bot prints summary**: `PWM_MIN_L`, `PWM_MIN_R`, `PWM_MAX_L`,
   `PWM_MAX_R`. Operator reviews the values look sensible (typically
   `PWM_MIN ≈ 30-80`, `PWM_MAX ≈ 240-255`).
7. **Values saved to EEPROM** slot `0x210` (actuator slot, extended).
8. **Brute-force tuner gets realistic bounds for free** — its search
   completes in minutes instead of hours.

Safety: bot must be lifted for the entire ramp. A bot on the ground will
launch off the bench when PWM crosses stiction. Operator-supervised, no
exceptions.

---

## Section 9 — Safety notes

Encoders do **not** make the bot safer to operate. The collision detector
remains the safety net during balance; this doc does not change Section
4 of [safe_bench_test_workflow.md](safe_bench_test_workflow.md#section-4--run-mode-operation).

What encoders DO buy you in safety terms:

- **Symmetric stall detection.** If an encoder wire comes loose mid-flight,
  ticks freeze, stall detection fires, bot enters HELD, motors stop. This
  is the "safe failure" direction.
- **Earlier stuck detection.** With encoders the stuck timer is ~100 ms
  instead of the 1 500 ms gyro-based fallback. Faster cut-off when a wheel
  catches on a wall.

What encoders DO NOT buy:

- **They cannot detect "the bot is lifted".** Ticks only measure wheel
  rotation. A bot held perfectly still in the air with motors off reads
  zero ticks, identical to a bot at rest on the floor. Use the BNO055
  motion-quiet detector for "is the bot stationary".
- **They cannot stop a controller bug from saturating motors.** The
  emergency stop is still `a` in the serial monitor.

Operating rule: encoders are an outer-loop *input*, not a kill switch.
Treat them like a sensor, not like a brake.

---

## Section 10 — Quick-reference card

Print. Tape to bench.

### Pin assignments

| Signal | Pin |
|---|---:|
| L_ENC_A | 18 |
| L_ENC_B | 19 |
| R_ENC_A | 2 |
| R_ENC_B | 3 |

**Reserved (do not touch): 20, 21 (I²C → BNO055).**

### Bring-up commands (target — most not yet implemented; see Section 11)

| Step | Command | Section |
|---|---|---|
| Direction check | (TBD — needs tick-readout command) | §3 |
| CPR calibration | (TBD — needs cal-save command) | §4 |
| Wheel radius save | (TBD — needs cal-save command) | §5 |
| Distance check | (TBD — needs distance-readout command) | §6 |
| Stall bench test | (TBD — needs raw-PWM command) | §7 |
| PWM auto-discovery | `p` (Phase 4M.12, not yet implemented) | §8 |

### Expected calibration values

| Wheel + motor | CPR (counts/rev) | Wheel radius |
|---|---:|---:|
| Yellow-TT (65 mm wheel) | 960 | 0.0325 m |
| N20 + 43 mm hub | 2 800 | 0.0215 m |

### Troubleshooting flowchart

```
Wheel turn forward → ticks NOT changing
  → check VCC, GND on the encoder (multimeter to 5V/0V)
  → check A wire continuity (Mega pin 18 or 2 to encoder A)
  → if still dead: encoder is broken; swap motor

Wheel turn forward → ticks DECREASE
  → swap A/B in WheelEncoder constructor for that wheel
  → re-flash, retry Section 3

Distance off by >10%
  → re-measure wheel diameter with calipers
  → re-run Section 4 CPR (slower hand-rotation, watch for one-channel-only)

Stall fires when wheels are free
  → CPR wrong, velocity reads non-zero noise (Section 4)
  → OR commanded-PWM reporting not wired up (firmware-side issue)

Random ticks when stationary
  → EMI from L298N — shorten encoder cable, twist A/B together
  → keep encoder cables physically separated from motor leads
```

### When to redo calibration

- After motor or wheel replacement → redo Sections 4 + 5.
- After re-mounting wheels (set-screw moved on shaft) → redo Section 6
  distance check; usually no recal needed.
- After EEPROM clear or firmware update that wipes the cal slot → redo
  Sections 4 + 5 + 8.

---

## Section 11 — Open work (what's missing)

The driver lands in this wave; the operator-facing CLI commands do **not**.
Track these as next-session work; the operator will need them to execute
Sections 3-7 end-to-end.

| # | Command (proposed) | What it does | Used by section |
|---|---|---|---|
| 1 | `e` | Stream live left/right ticks + velocity to serial; reset on second press | §3 (direction check), §4 (CPR cal) |
| 2 | `E` (or sub-mode of `e`) | Save current CPR + wheel radius to EEPROM slot `0x220` | §4, §5 |
| 3 | `d` | Print accumulated distance (m) and zero on second press | §6 |
| 4 | Raw-PWM test command | Drive both motors at a fixed PWM for a fixed duration (bench-only, requires bot on stand) | §7 (stall test), §8 prerequisite |
| 5 | `p` | Run PWM auto-discovery state (Phase 4M.12) | §8 |

Also queued from [MEGA_UNIVERSAL_PLAN §7](../MEGA_UNIVERSAL_PLAN.md):

- Encoder cascade integration into `step_run_` (Phase 4M.13) — the actual
  "stop wandering" payoff. Outer-loop velocity nudge replaces the option-B
  pitch-integrator.
- K_motor cross-verification during BOOTSTRAP (Phase 4M.14) — independent
  K estimate from encoder velocity vs. PWM, adds
  `failure_reason = 7 (K_disagreement)`.

When asking for the CLI work, reference this section by name — the parent
agent will know which commands to add.

---

## See also

- [research_wheel_encoders_mega_2026-05-19.md](../findings/research_wheel_encoders_mega_2026-05-19.md) —
  hardware survey, library comparison, driver design rationale, full bench
  test plan
- [MEGA_UNIVERSAL_PLAN.md §7](../MEGA_UNIVERSAL_PLAN.md) — encoder
  integration design + Phase 4M.10-14 ordering
- [safe_bench_test_workflow.md](safe_bench_test_workflow.md) — non-encoder
  bench-test procedure; required reading before this guide
- `src/sensors/wheel_encoder.{h,cpp}` — driver source + API
- [SelfBallancingRobot3.ino](../archive/balancing_robot_reference/SelfBallancingRobot3.ino) —
  motor pin reference (L298N ENA=5, IN1=6, IN2=7, IN3=9, IN4=8, ENB=10)
