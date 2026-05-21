# Balancing Robot — Uno Minimal

**Status**: Phase 4U landed 2026-05-19 (scaffold) / 2026-05-20 (guided-tuning pivot — Workstream UT-D docs).
**Build envs**: `arduino_uno_minimal` (lean flight build) + `arduino_uno_tuning` (guided P→I→D tuning build).
**Sibling**: [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — the Mega-universal reference application.
**Index**: [INDEX.md](INDEX.md).

---

## 1. What this is

The Uno-minimal balance bot is the **small/cheap target** of the 2026-05-19 [platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal). It is a single-purpose program: read pitch from a calibrated BNO055, run a fixed-gain PID, drive an L298N. That is the entire control pipeline:

```
pitch = BNO055.pitch() - PITCH_OFFSET_DEG
if |pitch| > TIP_CUTOFF_DEG  -> motors.stop()
else                          -> PWM = PID(pitch); motors.write(PWM)
```

No adaptive layer. No BOOTSTRAP, no RLS, no OnlineMountingEstimator, no collision detection, no position containment. Those live on the Mega-universal sibling. If you need any of them, you are not looking at the right application — see [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md).

Gains (`Kp`, `Ki`, `Kd`, `PITCH_OFFSET_DEG`, `PWM_MAX`) are tuned through an **on-device guided P→I→D tuning session** — a serial-driven interactive walkthrough hosted by the `arduino_uno_tuning` build env. The tuned gains are persisted to **EEPROM**; the flight build (`arduino_uno_minimal`) reads them at boot. `balance_constants.h` is a hand-editable **cold-start seed** used only when EEPROM holds no tune. There is no on-MCU *adaptation* (no RLS, no BOOTSTRAP) — tuning is a deliberate operator-driven session, not continuous learning. See [§4](#4-the-guided-tuning-workflow) for the workflow and [`findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md) for the design.

---

## 2. Hardware

Wiring, pinout, BOM, and power topology live in the **Mega-balancer hardware doc**:

- **Canonical**: [../balancing_robot/HARDWARE_SETUP.md](../balancing_robot/HARDWARE_SETUP.md)

**Uno-specific deltas vs that doc**:

- **MCU**: Arduino Uno (ATmega328P, 32 KB flash, 2 KB RAM) instead of Mega 2560.
- **I²C pins**: Uno uses `A4` (SDA) / `A5` (SCL). The Mega uses `20` / `21`. Same BNO055 module otherwise.
- **No wheel encoders**. The Uno minimal program has no encoder driver and no position loop.
- **No GPS**. Same reason — flash and pins.
- **No SD card / telemetry recorder**. The Uno has just enough flash for the BNO055 + L298N + PID + a tiny serial CLI.
- **Motor driver pins** (matches `archive/balancing_robot_reference/SelfBallancingRobot3.ino`): `ENA=5, IN1=6, IN2=7, IN3=9, IN4=8, ENB=10`. See `src/applications/balancing_robot_uno/main.cpp` for the authoritative list.

If your wiring matches the Mega doc and you swap the I²C pins to A4/A5, you are ready.

---

## 3. Building and flashing

From `auto_orientation/`:

```bash
pio run -e arduino_uno_minimal              # compile only
pio run -e arduino_uno_minimal -t upload    # compile + flash
```

**Expected flash usage**: under 50 % of the Uno's 32 KB (per `platformio.ini` lines 120-152, which set the explicit `<50% flash` target). The 2026-05-19 audit measured **49.7 % flash / 34.7 % RAM** on the scaffold (see [findings/audit_uno_minimal_2026-05-19.md](../../findings/audit_uno_minimal_2026-05-19.md)). If your build comes in much over 50 %, something universal-stack has leaked in — check `build_src_filter` in `platformio.ini`.

**Serial after flash** (115200 baud — see `main.cpp:13`):

```
'a' — emergency stop (latched)
'g' — re-arm after stop (clears integral)
's' — one-shot status line
'p' — toggle 10 Hz telemetry stream
```

**At first boot** the bot:

1. Initialises BNO055 (NDOF fusion mode, I²C 0x28) and L298N.
2. Starts the MsTimer2 200 Hz PID tick (5 ms period).
3. Begins balancing immediately — no calibration UX, no button press. The BNO055 is assumed pre-calibrated (the Mega calibration build or any other host path writes the 22-byte calibration blob to EEPROM ahead of time).

If `s` returns NaN pitch on day one, the BNO055 cal blob is missing — calibrate it via the Mega path first.

---

## 4. The guided tuning workflow

**This is the operational loop you care about.** The Uno balance build is primarily a *guided, interactive, on-device P→I→D tuning experience*: you flash the tuning firmware, open a serial terminal, and the firmware walks you through tuning each PID term one at a time while the bot balances live. The tuned result is saved to EEPROM; you then reboot into the lean flight build to fly. The design is documented in [`findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md).

### When to re-tune

Re-run a guided session when the bot stops balancing well — typically after any of:

- **Battery**: voltage shifts more than ~15 %, chemistry swap (NiMH → LiPo), or noticeable sag.
- **Wheels / tyres**: different diameter, different rubber, different grip surface.
- **Chassis mass / payload**: IMU remounted higher or lower, anything bolted to the deck.
- **Motors / gearbox**: noticeable wear or replacement.
- **Operating surface**: carpet → hardwood, etc.

A re-tune is a guided serial session — no recompile, no host tooling. The previous tune lives in EEPROM until you overwrite it.

### Step 1 — Flash the guided-tuning firmware

From `auto_orientation/`:

```bash
pio run -e arduino_uno_tuning -t upload
```

`arduino_uno_tuning` is the `arduino_uno_minimal` program **plus** a `TuningSession` state machine and the serial command set below. (The flight build omits all of it to stay lean — see [§5](#5-what-lives-where).)

### Step 2 — Open serial and start the session

Open a serial terminal at **115200 baud**. The bot boots balancing on its current EEPROM (or seed) gains in the `IDLE` stage. Type `t` to begin tuning → the firmware enters **STAGE_P**.

The bot stays **live and balancing throughout** every stage; the 25° tip-cutoff still catches falls and `a` is always available as an emergency stop.

### Step 3 — Walk P → I → D

Each stage tunes one term with the other two masked per the classic manual PID procedure:

- **Stage P** — `Ki` and `Kd` forced to 0. Tap `+` to raise `Kp` until the bot is crisp but visibly oscillating, then back off ~20–30 % with `-`. Press `n` to lock `Kp` and advance.
- **Stage I** — `Kp` locked, `Kd` still 0. Watch for steady drift; raise `Ki` until the drift is removed, back off if a slow (<1 Hz) wobble appears. `n` advances; `b` returns to Stage P.
- **Stage D** — `Kp` and `Ki` locked. Raise `Kd` to damp overshoot ringing; back off if the motors get buzzy. `n` advances to **REVIEW**.
- **REVIEW** — the firmware prints the final `Kp/Ki/Kd`. `w` saves; `q` discards; `b` re-enters Stage D.

### Step 4 — The serial command set

Available in the `arduino_uno_tuning` build (see design doc §2.2):

| Cmd | Meaning | Valid in |
|---|---|---|
| `t` | Start tuning session → Stage P | IDLE |
| `+` | Increase the current stage's gain one step | Stage P/I/D |
| `-` | Decrease the current stage's gain one step | Stage P/I/D |
| `*` | Coarse-mode toggle (×5 ↔ ×1 step size) | Stage P/I/D |
| `n` | Lock the current term, advance to next stage | Stage P/I/D |
| `b` | Back one stage | Stage I/D, REVIEW |
| `r` | Reset the current term to its stage-entry value | Stage P/I/D |
| `w` | Save tuned gains to EEPROM | REVIEW |
| `q` | Quit without saving → IDLE | any tuning stage |
| `a` | Emergency stop (latched) | any |
| `s` | One-line status (stage + working gains) | any |
| `p` | Periodic telemetry toggle | any |

### Step 5 — Save and fly

In **REVIEW**, press `w`. The firmware writes the tune to EEPROM and prints `SAVED. Reboot to fly.` Then flash and boot the lean flight build:

```bash
pio run -e arduino_uno_minimal -t upload
```

`arduino_uno_minimal` reads the EEPROM tune at boot (falling back to the `balance_constants.h` seed if EEPROM is empty) and prints which source it used. It carries no tuning UX and no prompt strings.

### Step 6 — Bench validation

1. Place the bot upright on a flat indoor surface (hardwood or low-pile carpet).
2. Release. Hands off.
3. Time to first tip-over.

**Success criterion** (roadmap §Phase 4U.5): **≥ 30 s balance** on a flat indoor surface, no operator intervention.

If the bot tips immediately, see Troubleshooting §6.

### Generating a fresh seed (optional)

For a **brand-new chassis** with no working gains at all, the cold-start seed in `balance_constants.h` may be too far off for the bot to balance well enough to start Stage P. The offline Python tuner can regenerate that seed:

```bash
python3 tools/sim/brute_tune.py \
    --mode evolutionary --budget 5000 --plant uno_small \
    --output src/applications/balancing_robot_uno/balance_constants.h
```

This is now an **optional one-shot seed generator**, not the operational tuning loop — you still finish with a guided on-device session. `balance_constants.h` may also simply be hand-edited. Full flag list: `python3 tools/sim/brute_tune.py --help`; recipe summary: [tools/sim/README.md](../../../tools/sim/README.md).

---

## 5. What lives where

```text
src/applications/balancing_robot_uno/
├── main.cpp              # Arduino setup() + loop() + MsTimer2 200 Hz ISR + serial CLI
├── uno_balance_app.h     # UnoBalanceApp class — IMU read, PID step, motor write
├── uno_balance_app.cpp   # implementation (~150 LOC)
├── balance_constants.h   # hand-editable COLD-START SEED (see §7)
├── tune_storage.h        # EEPROM tune-block API — saveTuning/loadTuning/hasTuning
├── tune_storage.cpp      # EEPROM persistence (region base 0x200, CRC8)
├── tuning_session.h      # TuningSession state machine — guided P/I/D walkthrough
└── tuning_session.cpp    # implementation; gated on UNO_GUIDED_TUNING (tuning build only)

tools/sim/
├── brute_tune.py                       # optional cold-start seed generator
├── balance_bot_sim.py                  # plant + IMU + PID simulator brute_tune.py drives
├── balance_constants_template.h.in     # template brute_tune.py fills in
└── README.md                           # seed-generator recipe summary
```

`tune_storage.{h,cpp}` and `tuning_session.{h,cpp}` land this session (Workstreams UT-A / UT-C of the [guided-tuning design](../../findings/uno_guided_tuning_design_2026-05-20.md)). Both the `arduino_uno_minimal` (flight) and `arduino_uno_tuning` (guided-tuning) envs compile `tune_storage.cpp`; only `arduino_uno_tuning` defines `UNO_GUIDED_TUNING`, so the flight build compiles out all `TuningSession` code and prompt strings.

Reused framework modules (no Uno-specific code added):

- `src/sensors/bno055.{h,cpp}` — pitch from calibrated BNO055
- `src/actuators/l298n_motor_driver.{h,cpp}` — dual-channel PWM motor driver
- `src/control/pid_controller.{h,cpp}` — the PID itself
- `src/math/quaternion_conversions.{h,cpp}` — orientation math (pulled in by BNO055)

---

## 6. Troubleshooting

**Bot oscillates back and forth (low-frequency hunting)**

- `Kp` likely too high or `Kd` too low. Re-run with a larger `--budget` (10000) or try `--plant stress` to reward damping more aggressively.
- Check chassis: anything loose on the deck adds an unmodelled spring/damper.

**Bot drifts in one direction while balancing**

- `PITCH_OFFSET_DEG` is off. Re-run the tuner — the GA exposes asymmetric controllers via the `init_signs=(+1, -1)` worst-case fitness (see `brute_tune.py:evaluate_candidate`).
- If re-running doesn't fix it, the BNO055 calibration blob has drifted. Re-calibrate via the Mega calibration path.

**Bot falls immediately on release**

- `STICTION_PWM` mismatch: the simulated plant assumes motors don't move below PWM=15 (see `brute_tune.py:SAFETY_STICTION_PWM`). If your real motors need higher than 15 to start turning, the `uno_small` preset's `stiction_floor=18` may still be too low. Measure your bot's actual stiction PWM and adjust `PlantParams.stiction_floor` in `brute_tune.py:PLANT_PRESETS`. <!-- TODO: a CLI flag for stiction would be nicer; not yet present in brute_tune.py. -->
- Wheels slipping on the surface (high-gloss tile, dust).
- BNO055 NaN at boot — try `s` over serial; if pitch is NaN, the cal blob is missing.

**Tuner takes forever**

- Drop `--budget` to 1000 for a smoke test. Should finish in ~30 s.
- `--mode random --budget 1000` is the fastest path to "does this even converge".

**Tuner finishes but `tip_time = 0.000s` / `tipped = yes` for the winner**

- The plant preset is unbalanceable at the configured `PWM_MAX` (i.e. the bot is physically heavier/longer than the plant model assumes). Try `--plant reference` first; if that still tips, the plant model needs adjustment in `brute_tune.py:PLANT_PRESETS` for your chassis.

**Constants generated but bot worse than before**

- Restore the previous `balance_constants.h` from git (`git checkout src/applications/balancing_robot_uno/balance_constants.h`), then re-tune with a different `--mode`, `--seed`, or larger `--budget`.

**Build fails with `BALANCE_KP undefined`**

- Header schema mismatch (this was a known bug in early Phase 4U — see [findings/audit_uno_minimal_2026-05-19.md](../../findings/audit_uno_minimal_2026-05-19.md) "Brute-tuner integration"). Confirm the generated header defines `BALANCE_KP`, `BALANCE_KI`, `BALANCE_KD`, `PWM_MIN`, `PWM_MAX`, `STICTION_PWM`, `TIP_CUTOFF_DEG`, `PITCH_SANITY_DEG` at file scope (not inside a namespace).

---

## 7. Constraints — what NOT to do

- **DO NOT add adaptive code** (RLS, BOOTSTRAP, OnlineMountingEstimator, collision detection, position containment, K cross-check, velocity outer loop) to the Uno program. That is the Mega-universal application's job, per [scope.md §Platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal). The Uno's design is to be small enough that the flash-erase-write cycle IS the tuning loop.

- **`balance_constants.h` is a hand-editable cold-start seed — that is intentional.** It is no longer auto-generated, and editing it (or regenerating it via `brute_tune.py`) is fine. The *live* tuned gains come from EEPROM, set via the guided session, and supersede the seed at boot. Do not re-add "DO NOT HAND-EDIT" language — see §7 note below and the [guided-tuning design](../../findings/uno_guided_tuning_design_2026-05-20.md).

- **DO NOT edit `tools/sim/balance_constants_template.h.in`** unless you are extending the seed generator itself (adding a new constant to the contract). The template is the schema the on-MCU consumer (`uno_balance_app.cpp`, `main.cpp`) is wired against — break the schema and the build breaks.

- **Serial gain-tuning commands ARE the intended design** (as of 2026-05-20). An earlier revision of this README said "DO NOT add serial commands for runtime gain editing" — that is **superseded** by the operator's clarification that the Uno balance build is primarily a guided on-device P→I→D tuning experience. The `t/+/-/*/n/b/r/w/q` command set lives in the `arduino_uno_tuning` build by design; see [`findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md) §2.2. Note this is *guided manual tuning*, not on-MCU adaptation — RLS / BOOTSTRAP / OnlineMountingEstimator remain off-limits on the Uno (first bullet).

- **The `arduino_uno_tuning` build env IS the intended design** (as of 2026-05-20). An earlier revision said "DO NOT introduce a new build env" — that is **superseded**. The flash budget makes the split mandatory: `arduino_uno_tuning` hosts the guided-tuning firmware (state machine + ~3–5 KB of prompt strings); `arduino_uno_minimal` stays the lean flight build. The legacy `uno_balance` env (old universal-stack-on-Uno path) is still dead — do not extend *that* one.

---

## 8. References

- [scope.md §Platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19--mega-universal-vs-uno-minimal) — the pivot framing
- [roadmap.md §Phase 4U](../../roadmap.md#phase-4u--uno-minimal-hardcoded-balancer--python-brute-force-tuner) — phase plan and success criteria
- [INDEX.md](INDEX.md) — file index for this application
- [tools/sim/README.md](../../../tools/sim/README.md) — tuner recipe summary
- [../balancing_robot/HARDWARE_SETUP.md](../balancing_robot/HARDWARE_SETUP.md) — wiring, BOM, power topology (canonical)
- [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — the Mega-universal sibling
- [findings/audit_uno_minimal_2026-05-19.md](../../findings/audit_uno_minimal_2026-05-19.md) — audit of this scaffold
- [findings/brute_tune_simplification_design_2026-05-19.md](../../findings/brute_tune_simplification_design_2026-05-19.md) — tuner design notes
- `archive/balancing_robot_reference/SelfBallancingRobot3.ino` — hand-tuned reference the algorithm shape echoes

---

*Last updated: 2026-05-20 (Workstream UT-D — guided-tuning pivot: §4 walkthrough, §7 constraints rewrite).*
