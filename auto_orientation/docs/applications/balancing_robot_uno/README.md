# Balancing Robot — Uno Minimal

**Status**: Phase 4U landed 2026-05-19 (scaffold) / 2026-05-20 (guided-tuning pivot — Workstream UT-D docs) / 2026-05-26 (Mega-vs-Uno capability-tier clarification; SETUP-vs-OPERATIONAL framing; P→D→I order; value-robustness principle; on-Uno IMU calibration + photo-backup printer shipped) / 2026-05-26 wave 6 (Uno IMU selection wired at build level with `#error` on USE_BNO085+Uno; cal-blob slot widened to 72 B for future BNO085).
**Build envs**: `arduino_uno_minimal` (lean flight build / OPERATIONAL MODE) + `arduino_uno_tuning` (IMU calibration + guided P→D→I tuning build / SETUP MODE).
**Sibling**: [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — the Mega-universal reference application.
**Index**: [INDEX.md](INDEX.md).

---

## 1. What this is

The Uno-minimal balance bot is the **small/cheap target** of the 2026-05-19 [platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal). It is a single-purpose program: read pitch from a calibrated IMU (**BNO055 default; USE_BNO085 future-supported on non-Uno targets per the memory-tier principle** — see callout below), run a fixed-gain PID, drive an L298N. That is the entire control pipeline:

```
pitch = BNO055.pitch() - PITCH_OFFSET_DEG
if |pitch| > TIP_CUTOFF_DEG  -> motors.stop()
else                          -> PWM = PID(pitch); motors.write(PWM)
```

No adaptive layer. No BOOTSTRAP, no RLS, no OnlineMountingEstimator, no collision detection, no position containment. Those live on the Mega-universal sibling. If you need any of them, you are not looking at the right application — see [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md).

### IMU selection on Uno — BNO055 default; USE_BNO085 rejected at build time

`main.cpp` (wave 6 / 2026-05-26) selects the IMU at compile time. The `arduino_uno_*` envs ship `-DUSE_BNO055` by default. If a developer overrides with `-DUSE_BNO085` on a Uno target, the build fails fast with:

```text
#error "USE_BNO085 not supported on AVR ATmega328P (Uno) — BNO085 library exceeds Uno's 32 KB flash. Use BNO055 on Uno, or BNO085 on Mega/Teensy/ESP32."
```

A second `#error` rejects builds that define both `USE_BNO055` and `USE_BNO085`; a third asserts the default (USE_BNO055) when neither is set. This is the [memory-tier principle](../../scope.md#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal) made concrete — the Adafruit BNO08x / SH-2 library footprint (~15–20 KB flash) plus the existing minimal app does not fit in the Uno's 32 KB. USE_BNO085 on Mega/Teensy/ESP32 is architecturally supported (no flash constraint there) but Mega-side wiring is a future workstream — see [../../todo.md](../../todo.md).

The split between Mega and Uno is **memory-driven capability tiering**, not IMU-driven: the Mega has the flash + RAM headroom to host the universal auto-tune stack ("plug-and-play"), so it does; the Uno does not, so it deliberately ships a **manual operator-guided** flow instead. The Uno's "more manual steps" are by design — that is the small/cheap tier of the platform bifurcation, not a deficiency.

Gains (`Kp`, `Ki`, `Kd`, `PITCH_OFFSET_DEG`, `PWM_MAX`) are tuned through an **on-device guided P→D→I tuning session** — a serial-driven interactive walkthrough hosted by the `arduino_uno_tuning` build env. The tuned gains are persisted to **EEPROM**; the flight build (`arduino_uno_minimal`) reads them at boot. `balance_constants.h` is a hand-editable **cold-start seed** used only when EEPROM holds no tune (and the canonical hardcode-from-photo target if EEPROM is ever wiped — see [§4.7](#47-value-robustness--every-persisted-value-is-photographable)). There is no on-MCU *adaptation* (no RLS, no BOOTSTRAP) — tuning is a deliberate operator-driven session, not continuous learning. See [§4](#4-the-guided-tuning-workflow) for the workflow and [`findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md) for the design.

### SETUP MODE vs OPERATIONAL MODE

The Uno program lives in **two distinct modes**, one per build env:

- **SETUP MODE** — `arduino_uno_tuning`. The operator-facing flow: IMU calibration, guided P→D→I tuning, save (which prints values for photo-backup).
- **OPERATIONAL MODE** — `arduino_uno_minimal`. Lean flight build. Reads stored values at boot, runs `pitch → PID → PWM`. None of the SETUP-MODE strings or state machines.

A typical lifecycle: flash SETUP MODE → run the guided session → save → flash OPERATIONAL MODE → operate. Re-tunes repeat the loop; no recompile of OPERATIONAL MODE is required.

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

**At first boot** the bot prints `==== OPERATIONAL MODE -- flying on EEPROM values ====` followed by the active `Kp/Ki/Kd/off`, then:

1. Initialises BNO055 (NDOF fusion mode, I²C 0x28) and L298N.
2. **Restores the BNO055 calibration blob from on-Uno EEPROM** if present (`BNO055 cal restored from EEPROM`). If the blob is missing the flight build prints a loud warning with the recovery path:
   ```
   WARN: no BNO055 cal in EEPROM
     -> flash arduino_uno_tuning and run 'c' to calibrate,
        OR hardcode BNO055_CAL_BLOB[22] in balance_constants.h.
   ```
3. Starts the MsTimer2 200 Hz PID tick (5 ms period).
4. Begins balancing immediately — no calibration UX, no button press.

The operator can do **everything on the Uno alone** — calibration, tuning, and flight. There is no Mega-side prerequisite. If `s` returns NaN pitch on day one, the BNO055 cal blob is missing or stale — flash `arduino_uno_tuning` and run `'c'` (see §4).

---

## 4. The guided tuning workflow

**This is the operational loop you care about.** The Uno balance build's SETUP MODE (env `arduino_uno_tuning`) is a *guided, interactive, on-device IMU-calibration + P→D→I tuning experience*: you flash the tuning firmware, open a serial terminal, and the firmware walks you through calibrating the IMU, then tuning each PID term one at a time while the bot balances live. The tuned result is saved to EEPROM (and printed for photo-backup); you then flash the OPERATIONAL MODE lean flight build (`arduino_uno_minimal`) to fly. The design is documented in [`findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md).

The end-to-end flow is: `open serial → 'c' IMU calibration → 't' guided P → D → I → 'w' save (which prints the photo-backup block) → flash OPERATIONAL MODE`.

### When to re-tune

Re-run a guided session when the bot stops balancing well — typically after any of:

- **Battery**: voltage shifts more than ~15 %, chemistry swap (NiMH → LiPo), or noticeable sag.
- **Wheels / tyres**: different diameter, different rubber, different grip surface.
- **Chassis mass / payload**: IMU remounted higher or lower, anything bolted to the deck.
- **Motors / gearbox**: noticeable wear or replacement.
- **Operating surface**: carpet → hardwood, etc.

A re-tune is a guided serial session — no recompile, no host tooling. The previous tune lives in EEPROM until you overwrite it.

### 4.1 — Flash the guided-tuning firmware (SETUP MODE)

From `auto_orientation/`:

```bash
pio run -e arduino_uno_tuning -t upload
```

`arduino_uno_tuning` is the `arduino_uno_minimal` program **plus** a `TuningSession` state machine and the serial command set below. (The flight build omits all of it to stay lean — see [§5](#5-what-lives-where).)

### 4.2 — Open serial and calibrate the IMU (`'c'`)

Open a serial terminal at **115200 baud**. The boot banner is:

```
==== SETUP MODE -- calibrate + guided PID tune ====
Commands: c=calibrate t=tune | + - * n b r w q | a g s p
When done: flash arduino_uno_minimal to fly.
```

Type `'c'` first. The firmware **disarms** the bot (motors off — you're about to pick it up), prints the pose script, and polls BNO055 cal status:

```
==== BNO055 GUIDED CAL — disarmed; rotate by hand ====
1. GYRO: hold still flat on a table ~3 s
2. ACCEL: rotate to each face (top/bot/left/right/fwd/back), hold ~3 s each
3. MAG: draw a figure-8 in the air several times
Watch cal=SaGM — each digit 0..3, all four = 3 = done.
Press 'q' or 'a' to abort without saving.
```

A live `cal=Sssagm` line ticks twice a second so you can see the digits climb as you rotate. When all four hit 3 the firmware reads the 22-byte cal blob from the chip, saves it to EEPROM (`CAL OK: 22-byte blob saved to EEPROM`), and prints the **photo-backup block** (see §4.7) with the `BNO055_CAL_BLOB[22] = { ... }` line. `'q'` or `'a'` aborts cleanly without writing.

**(Wave 6 / 2026-05-26)** The on-EEPROM cal-blob slot is now **variable-length** (up to 72 B reserved at base `0x220`, version byte `0x03`) so the same layout can serve BNO055 today (22 B) and a future BNO085 SH-2 FRS cal (up to ~72 B). A pre-existing **22-byte v2 blob** from a prior firmware version will **reject-on-load cleanly** — the operator re-runs `'c'` to re-cal and the new v3 blob is written. This is the safe failure mode; reading a 22-byte v2 payload as if it had a length header would corrupt the calibration.

Set the bot back down upright. The bot stays **live and balancing throughout** the subsequent tuning stages; the 25° tip-cutoff still catches falls and `a` is always available as an emergency stop.

### 4.3 — Walk P → D → I (`'t'`)

Type `'t'` to begin tuning → the firmware enters **STAGE_P**. Each stage tunes one term with the other two masked per the classic manual PID procedure. The order is **P → D → I**: derivative damping is established before integral wind-up is introduced. The actual stage prompts printed by the firmware are:

- **Stage P** — `[P] Ki=Kd=0. +/- Kp. n=next`. `Ki` and `Kd` are forced to 0 by the mask. Tap `+` to raise `Kp` until the bot is crisp but visibly oscillating, then back off ~20–30 % with `-`. Press `n` to lock `Kp` and advance.
- **Stage D** — `[D] Ki=0. +/- Kd. n=next b=back`. `Kp` locked, `Ki` still 0. Raise `Kd` to damp the overshoot ringing seen at the end of Stage P; back off if the motors get buzzy. `n` advances, `b` returns to Stage P.
- **Stage I** — `[I] +/- Ki. n=review b=back`. `Kp` and `Kd` locked, full PID now active (no mask). Watch for steady drift; raise `Ki` until the drift is removed, back off if a slow (<1 Hz) wobble appears. `n` advances to **REVIEW**.
- **REVIEW** — `[REVIEW] w=save q=quit b=back`. `w` saves; `q` discards; `b` re-enters Stage I.

### 4.4 — The serial command set

Available in the `arduino_uno_tuning` build (see design doc §2.2):

| Cmd | Meaning | Valid in |
|---|---|---|
| `c` | Run guided BNO055 calibration → save 22-byte blob + photo-backup | IDLE |
| `t` | Start tuning session → Stage P | IDLE |
| `+` | Increase the current stage's gain one step | Stage P/D/I |
| `-` | Decrease the current stage's gain one step | Stage P/D/I |
| `*` | Coarse-mode toggle (×5 ↔ ×1 step size) | Stage P/D/I |
| `n` | Lock the current term, advance to next stage | Stage P/D/I |
| `b` | Back one stage | Stage D/I, REVIEW |
| `r` | Reset the current term to its stage-entry value | Stage P/D/I |
| `w` | Save tuned gains to EEPROM + print photo-backup block | REVIEW |
| `q` | Quit without saving → IDLE | any tuning stage |
| `a` | Emergency stop (latched) | any |
| `s` | One-line status **+ photo-backup snapshot** of whatever lives in EEPROM | any |
| `p` | Periodic telemetry toggle | any |

The `'s'` command is an especially useful one-keystroke EEPROM snapshot: at any time during setup it reprints the photo-backup block for the *currently-persisted* PID + cal — convenient for re-photographing after a settled session, or for verifying what is actually in EEPROM versus what is in the working gains.

### 4.5 — Save and fly (cross over to OPERATIONAL MODE)

In **REVIEW**, press `w`. The firmware writes the tune to EEPROM, prints `SAVED. Reboot to fly.`, and then — as the **last thing on screen so the operator photographs it** — emits the photo-backup block:

```
==== PHOTO-BACKUP -- paste into balance_constants.h ====
// Uno guided session output. Photograph this block.
static const float BALANCE_KP       = 94.4873f;
static const float BALANCE_KI       = 33.6538f;
static const float BALANCE_KD       = 90.2880f;
static const float PITCH_OFFSET_DEG = -4.3327f;
// BNO055 22-byte cal blob (hex, MSB-first as stored in EEPROM 0x222..0x237):
static const uint8_t BNO055_CAL_BLOB[22] = {
  0x.., 0x.., ... 0x..
};
==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ====
```

Photograph the scrollback. If EEPROM is ever wiped (chip replacement, accidental erase, board swap), paste those lines into the **PHOTO-BACKUP HARDCODE SITE** in `src/applications/balancing_robot_uno/balance_constants.h` and reflash — see [§4.7](#47-value-robustness--every-persisted-value-is-photographable).

Then flash and boot the lean flight build:

```bash
pio run -e arduino_uno_minimal -t upload
```

`arduino_uno_minimal` reads the EEPROM tune at boot (falling back to the `balance_constants.h` seed if EEPROM is empty) and prints which source it used. It carries no tuning UX and no prompt strings.

### 4.6 — Bench validation

1. Place the bot upright on a flat indoor surface (hardwood or low-pile carpet).
2. Release. Hands off.
3. Time to first tip-over.

**Success criterion** (roadmap §Phase 4U.5): **≥ 30 s balance** on a flat indoor surface, no operator intervention.

If the bot tips immediately, see Troubleshooting §6.

### 4.7 — Value robustness — every persisted value is photographable

Across SETUP MODE, **every value that gets persisted to EEPROM is also printed to serial in a copy-paste-ready form** so the operator can photograph the serial console and hardcode the values back into source later if EEPROM is wiped or the chip is replaced. This is a **first-class principle** of the Uno design, not an afterthought, and it is implemented.

Every EEPROM-writing path calls the same `tune_storage::print_photo_backup()` printer, which emits a block bounded by `==== PHOTO-BACKUP -- paste into balance_constants.h ====` / `==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ====`. The block contains:

- The tuned **PID gains** as `static const float BALANCE_KP/KI/KD = ...f;` matching the seed schema.
- The **pitch offset** as `static const float PITCH_OFFSET_DEG = ...f;`.
- The **IMU calibration blob** as `static const uint8_t BNO055_CAL_BLOB[22] = { 0x.., ... };`.

The PID lines print when a PID tune is present; the cal-blob line prints when a cal blob is present — so a cal-only flow (just `'c'`) prints only the cal block, a save (`'w'`) at REVIEW prints both, and `'s'` prints whatever is currently in EEPROM.

`balance_constants.h` is the **canonical hardcode-from-photo target**. It has a `PHOTO-BACKUP HARDCODE SITE` comment block at the top calling this out explicitly, and declares the exact symbols (`BALANCE_KP/KI/KD`, `PITCH_OFFSET_DEG`, `BNO055_CAL_BLOB[22]`) that the printer emits — paste the photographed block in verbatim, reflash `arduino_uno_minimal`, and the bot is back to its last-known-good state. No host tooling, no laptop required beyond a text editor and `pio run`.

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
├── tune_storage.h        # EEPROM tune-block API — tune_storage::save_tuning/load_tuning/has_tuning + variable-length cal-blob API save_cal_blob/load_cal_blob/has_cal_blob (up to 72 B, version 0x03)
├── tune_storage.cpp      # EEPROM persistence (tune block 0x200, cal blob 0x220 variable-length, CRC-8-CCITT)
├── tuning_session.h      # TuningSession state machine — guided P/D/I walkthrough
├── tuning_session.cpp    # implementation; gated on UNO_GUIDED_TUNING (tuning build only)
├── calibration_session.h # Guided BNO055 cal wizard — invoked by 'c' in SETUP MODE
└── calibration_session.cpp # implementation; gated on UNO_GUIDED_TUNING (tuning build only)

tools/sim/
├── brute_tune.py                       # optional cold-start seed generator
├── balance_bot_sim.py                  # plant + IMU + PID simulator brute_tune.py drives
├── balance_constants_template.h.in     # template brute_tune.py fills in
└── README.md                           # seed-generator recipe summary
```

`tune_storage.{h,cpp}`, `tuning_session.{h,cpp}`, and `calibration_session.{h,cpp}` ship the guided-tuning + IMU-calibration flow (Workstreams UT-A / UT-C of the [guided-tuning design](../../findings/uno_guided_tuning_design_2026-05-20.md), plus the 2026-05-26 calibration module). Both the `arduino_uno_minimal` (flight) and `arduino_uno_tuning` (guided-tuning) envs compile `tune_storage.cpp`; only `arduino_uno_tuning` defines `UNO_GUIDED_TUNING`, so the flight build compiles out all `TuningSession`, `calibration_session`, and prompt-string code.

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
- If re-running doesn't fix it, the BNO055 calibration blob has drifted. Re-calibrate on the Uno itself: flash `arduino_uno_tuning` and run `'c'` (see §4.2). *(Updated 2026-05-26 — on-Uno calibration superseded the prior "Mega calibration path" guidance.)*

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

- **DO NOT add adaptive code** (RLS, BOOTSTRAP, OnlineMountingEstimator, collision detection, position containment, K cross-check, velocity outer loop) to the Uno program. That is the Mega-universal application's job, per [scope.md §Platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal). The Uno's design is to be small enough that the flash-erase-write cycle IS the tuning loop.

- **`balance_constants.h` is a hand-editable cold-start seed — that is intentional.** It is no longer auto-generated, and editing it (or regenerating it via `brute_tune.py`) is fine. The *live* tuned gains come from EEPROM, set via the guided session, and supersede the seed at boot. Do not re-add "DO NOT HAND-EDIT" language — see §7 note below and the [guided-tuning design](../../findings/uno_guided_tuning_design_2026-05-20.md).

- **DO NOT edit `tools/sim/balance_constants_template.h.in`** unless you are extending the seed generator itself (adding a new constant to the contract). The template is the schema the on-MCU consumer (`uno_balance_app.cpp`, `main.cpp`) is wired against — break the schema and the build breaks.

- **Serial gain-tuning commands ARE the intended design** (as of 2026-05-20). An earlier revision of this README said "DO NOT add serial commands for runtime gain editing" — that is **superseded** by the operator's clarification that the Uno balance build is primarily a guided on-device P→D→I tuning experience. The `t/+/-/*/n/b/r/w/q` command set lives in the `arduino_uno_tuning` build by design; see [`findings/uno_guided_tuning_design_2026-05-20.md`](../../findings/uno_guided_tuning_design_2026-05-20.md) §2.2. Note this is *guided manual tuning*, not on-MCU adaptation — RLS / BOOTSTRAP / OnlineMountingEstimator remain off-limits on the Uno (first bullet).

- **The `arduino_uno_tuning` build env IS the intended design** (as of 2026-05-20). An earlier revision said "DO NOT introduce a new build env" — that is **superseded**. The flash budget makes the split mandatory: `arduino_uno_tuning` hosts the guided-tuning firmware (state machine + ~3–5 KB of prompt strings); `arduino_uno_minimal` stays the lean flight build. The legacy `uno_balance` env (old universal-stack-on-Uno path) is still dead — do not extend *that* one.

---

## 8. References

- [scope.md §Platform bifurcation](../../scope.md#platform-bifurcation-2026-05-19-clarified-2026-05-26--mega-universal-vs-uno-minimal) — the pivot framing
- [roadmap.md §Phase 4U](../../roadmap.md#phase-4u--uno-minimal-hardcoded-balancer--python-brute-force-tuner) — phase plan and success criteria
- [INDEX.md](INDEX.md) — file index for this application
- [tools/sim/README.md](../../../tools/sim/README.md) — tuner recipe summary
- [../balancing_robot/HARDWARE_SETUP.md](../balancing_robot/HARDWARE_SETUP.md) — wiring, BOM, power topology (canonical)
- [../balancing_robot/INDEX.md](../balancing_robot/INDEX.md) — the Mega-universal sibling
- [findings/audit_uno_minimal_2026-05-19.md](../../findings/audit_uno_minimal_2026-05-19.md) — audit of this scaffold
- [findings/brute_tune_simplification_design_2026-05-19.md](../../findings/brute_tune_simplification_design_2026-05-19.md) — tuner design notes
- `archive/balancing_robot_reference/SelfBallancingRobot3.ino` — hand-tuned reference the algorithm shape echoes

---

*Last updated: 2026-05-26 wave 6 (Uno IMU selection wired at build level via `#ifdef USE_BNO085` / `#else USE_BNO055` + three `#error` guards, including a hard refusal of USE_BNO085 on AVR ATmega328P [memory-tier principle made concrete] — see §1 IMU selection callout; cal-blob EEPROM slot widened to variable-length up to 72 B with version 0x03 to accommodate future BNO085 SH-2 FRS, old v2 22-byte blobs reject-on-load cleanly — see §4.2; §5 file-tree note refreshed to reflect the new cal-blob API. Prior 2026-05-26: on-Uno guided BNO055 calibration via `'c'`, P→D→I stage order shipped, photo-backup printer + balance_constants.h hardcode site shipped, SETUP/OPERATIONAL boot banners shipped, on-boot cal restore in flight build. Earlier 2026-05-26: Mega-vs-Uno capability-tier clarification: IMU choice orthogonal to MCU choice; SETUP-vs-OPERATIONAL mode framing; intended flow order P→D→I; value-robustness §4.7 added. Previous: 2026-05-20 Workstream UT-D — guided-tuning pivot: §4 walkthrough, §7 constraints rewrite.)*
