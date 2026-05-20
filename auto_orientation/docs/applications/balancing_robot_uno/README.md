# Balancing Robot — Uno Minimal

**Status**: Phase 4U landed 2026-05-19 (scaffold) / 2026-05-20 (Workstream UNO-C docs).
**Build env**: `arduino_uno_minimal`.
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

Gains (`Kp`, `Ki`, `Kd`, `PITCH_OFFSET_DEG`, `PWM_MAX`) are **hardcoded constants** baked in at compile time from `balance_constants.h`. That file is generated offline by the Python brute-force tuner (`tools/sim/brute_tune.py`). When the bot stops balancing well — new battery, new wheels, new surface, new payload — you re-run the tuner, regenerate `balance_constants.h`, and reflash. There is no on-MCU adaptation, ever.

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

## 4. The re-tune workflow

**This is the operational loop you care about.** Re-tune whenever the bot stops balancing well.

### When to re-tune

Re-run the tuner and reflash when any of these change:

- **Battery**: voltage shifts more than ~15 %, chemistry swap (NiMH → LiPo), or noticeable sag.
- **Wheels / tyres**: different diameter, different rubber, different grip surface.
- **Chassis mass / payload**: IMU remounted higher or lower, anything bolted to the deck.
- **Motors / gearbox**: noticeable wear or replacement.
- **Operating surface**: carpet → hardwood, etc.

If you find yourself re-tuning more than once per session, you have outgrown the Uno path — switch to the Mega-universal sibling (which adapts on-MCU).

### Step 1 — Run the tuner

From `auto_orientation/`:

```bash
python3 tools/sim/brute_tune.py \
    --mode evolutionary \
    --budget 5000 \
    --plant uno_small \
    --output src/applications/balancing_robot_uno/balance_constants.h
```

Arguments:

- `--mode {grid, random, evolutionary}` — search strategy. `evolutionary` (hand-rolled GA, pop=30, ~50 generations) is the recommended default for production tunes. `random` is fine for quick exploration. `grid` is coarse-grid + refinement; use only when you want determinism over speed.
- `--budget N` — total simulated balance trials. **5000 is the production target** for `evolutionary`. Drop to `1000` for a smoke test (~30 s), bump to `10000` if the GA appears to plateau early. Each trial = 8 s of simulated balance × 2 init signs.
- `--plant {reference, uno_small, stress}` — plant preset baked into `tools/sim/brute_tune.py:PLANT_PRESETS`. **Use `uno_small` for the Uno bench bot** (smaller chassis, weaker motors, more friction, more sensor noise than the reference Mega bot). `reference` mirrors `SelfBallancingRobot3.ino`. `stress` is a harder plant used for robustness sweeps.
- `--output PATH` — where to write the generated header. The path above drops it directly into the source tree so the next `pio run` picks it up. Relative paths resolve against `auto_orientation/`.

Optional knobs (default-good — leave alone unless you know why):

- `--seed N` — RNG seed (default 42; the search is deterministic given seed).
- `--duration N` — trial duration in seconds (default 8).
- `--init-perturbation N` — initial tilt in degrees (default 8.0).
- `--disturbance N` — periodic impulse magnitude in deg/s² (default 500).
- `--no-write` — dry run; print best gains without touching the header.
- `--quiet` — suppress progress output.

Full flag list: `python3 tools/sim/brute_tune.py --help`. Recipe summary: [tools/sim/README.md](../../../tools/sim/README.md).

The tuner prints progress every 5 % of the budget and finishes with:

```
Winning gains:
  Kp = ...
  Ki = ...
  Kd = ...
  PITCH_OFFSET_DEG = ...
  PWM_MAX = ...
```

### Step 2 — Sanity-check the result

Open the generated `src/applications/balancing_robot_uno/balance_constants.h` and compare against the reference (hand-tuned working bot, `archive/balancing_robot_reference/SelfBallancingRobot3.ino`):

| Constant | Reference | Sanity range |
|---|---|---|
| `Kp` | 65 | 20 – 200 |
| `Ki` | 12 | 0 – 50 |
| `Kd` | 38 | 5 – 100 |
| `PITCH_OFFSET_DEG` | −8.6 | −15 – +15 |
| `PWM_MAX` | 255 | 150 – 255 |

If anything is wildly outside that range (e.g. Kp=480 or Kd=0.5), the tuner has converged on a degenerate corner. Re-run with a different `--seed` or fall back to `--mode random --budget 2000`.

Also check the header preamble — the tuner records mode, budget, seed, plant preset, achieved fitness, and tip time so the run is reproducible.

### Step 3 — Rebuild and flash

```bash
pio run -e arduino_uno_minimal -t upload
```

### Step 4 — Bench validation

1. Place the bot upright on a flat indoor surface (hardwood or low-pile carpet).
2. Release. Hands off.
3. Time to first tip-over.

**Success criterion** (roadmap §Phase 4U.5): **≥ 30 s balance** on a flat indoor surface, no operator intervention.

If the bot tips immediately, see Troubleshooting §6.

---

## 5. What lives where

```text
src/applications/balancing_robot_uno/
├── main.cpp              # Arduino setup() + loop() + MsTimer2 200 Hz ISR + serial CLI
├── uno_balance_app.h     # UnoBalanceApp class — IMU read, PID step, motor write
├── uno_balance_app.cpp   # implementation (~150 LOC)
└── balance_constants.h   # REGENERATED BY THE TUNER — do not hand-edit (see §7)

tools/sim/
├── brute_tune.py                       # the tuner
├── balance_bot_sim.py                  # plant + IMU + PID simulator the tuner drives
├── balance_constants_template.h.in     # template the tuner fills in
└── README.md                           # tuner recipe summary
```

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

- **DO NOT hand-tune `balance_constants.h`** for production. The Python tuner is the source of truth. The header is regenerated wholesale on every tuner run; any hand-edits will be overwritten. Emergency exception: tweaking `PITCH_OFFSET_DEG` by ≤ 2° to recover from a barely-tipping bot when the operator cannot run the tuner is acceptable, but flag the file as dirty in git so the next tuner run is intentional.

- **DO NOT edit `tools/sim/balance_constants_template.h.in`** unless you are extending the tuner itself (adding a new constant to the contract). The template is the schema the on-MCU consumer (`uno_balance_app.cpp`, `main.cpp`) is wired against — break the schema and the build breaks.

- **DO NOT add serial commands for runtime gain editing.** The whole point of the Uno path is that constants are compile-time. If runtime tunability matters, you want the Mega.

- **DO NOT introduce a new build env.** `arduino_uno_minimal` is the only Uno env. The legacy `uno_balance` env exists but is the old universal-stack-on-Uno path that motivated the 2026-05-19 pivot — do not extend it.

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

*Last updated: 2026-05-20 (Workstream UNO-C — re-tune workflow doc).*
