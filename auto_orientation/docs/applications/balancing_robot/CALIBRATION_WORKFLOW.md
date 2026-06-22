# Self-Balancing Robot — Calibration Workflow (Mega tier)

Step-by-step procedure for the four operator-driven calibration commands in the `mega_balance` build:

| Command | Purpose | Required? | Persists to |
|---------|---------|-----------|-------------|
| BNO055 cal wizard | IMU sensor calibration (gyro + accel == 3) | YES — bot won't run BOOTSTRAP usefully without it | EEPROM 0x000 (cal blob, 22 B) |
| `c` (mount capture) | Captures chassis tilt at upright as the reference mounting offset | YES — recapture whenever the IMU's physical mounting on the chassis changes | EEPROM 0x200 (8 B, mount offset float) |
| `e` (encoder cal) | Counts-per-metre + wheel radius from a hand-rolled 1.000 m distance | YES on Mega — outer loop and K cross-check are degraded without it | EEPROM 0x220 (16 B) |
| `p` (PWM discovery) | Auto-discovers `MIN_PWM` (stiction floor) + `MAX_PWM` (saturation onset) from a wheels-free ramp | Recommended — the encoder-measured stiction overrides the CHARACTERISE estimate | EEPROM 0x230 (8 B) |
| `k` (characterise) | One-shot stiction-floor sweep using gyro response (no encoders needed) | Optional fallback if `p` is impractical | EEPROM 0x210 (8 B) |
| `b` (BOOTSTRAP) | Measure K_motor + derive PID gains (NOT a one-shot calibration — runs every session) | Triggered every prop-and-go boot OR manually via `b`/long-press | Not persisted — gains rebuild every session by design |

> Honest reminder: as of this writing the Mega bot has not yet successfully balanced on a bench. This workflow describes the **firmware procedure**; verification on hardware is pending. See the STATUS BANNER in [`USER_GUIDE.md`](USER_GUIDE.md).

---

## Table of contents

1. [EEPROM persistence map](#1-eeprom-persistence-map)
2. [Pre-flight](#2-pre-flight)
3. [BNO055 calibration wizard](#3-bno055-calibration-wizard)
4. [Mount capture — `c`](#4-mount-capture--c)
5. [Encoder calibration — `e` (Mega only)](#5-encoder-calibration--e-mega-only)
6. [PWM discovery — `p` (Mega only)](#6-pwm-discovery--p-mega-only)
7. [CHARACTERISE — `k`](#7-characterise--k)
8. [BOOTSTRAP — `b` (and prop-and-go)](#8-bootstrap--b-and-prop-and-go)
9. [Photo-backup recovery — `B` (forward reference)](#9-photo-backup-recovery--b-forward-reference)
10. [Calibration order on a fresh board](#10-calibration-order-on-a-fresh-board)
11. [When to re-run each step](#11-when-to-re-run-each-step)

---

## 1. EEPROM persistence map

All slots are CRC-8-CCITT protected (project standard, `calibration_storage.cpp::calculateCRC8`). The mount/actuator/PWM-discovery slots' version bytes were bumped from 0x01 → 0x02 on 2026-05-20 when the previous weak XOR-sum was replaced with CRC-8-CCITT (security finding NEW-P1-001) — pre-upgrade records are refused on load and force a re-calibrate.

| Slot | Magic / Ver | Length | Contents | Written by | Read by |
|------|------------|--------|----------|------------|---------|
| `0x000` | (`calibration_storage.h`) | 22 B + framing | BNO055 sensor calibration blob | BNO055 cal wizard, `app.short-press` save during cal | `setup()` `restoreFromEEPROM` → `imu.setCalibrationProfile` |
| `0x200` | 0xAB / 0x02 | 8 B | Mounting offset (float deg, LE) | `loop()` on `CAPTURE_MOUNTING`→`IDLE` transition | `setup()` `load_mount_offset_` (NaN/±90° rejected) |
| `0x210` | 0xAC / 0x02 | 8 B | Stiction floor (`uint8_t`, 30–200 PWM) | `loop()` on `CHAR_ACT`→`IDLE` transition | `setup()` `load_actuator_` → `motors.set_stiction_min_pwm()` |
| `0x220` | 0xAD / 0x01 | 16 B | Encoder cal: CPM L (f32), CPM R (f32), radius_m (f32). Mega-only. | `run_encoder_cal_` after operator presses button | `setup()` `load_encoder_cal_` → applies radius to both encoders |
| `0x230` | 0xAD / 0x02 | 8 B | PWM discovery: `min_pwm` (u16 LE), `max_pwm` (u16 LE). Mega-only. | `loop()` on `PWM_DISCOVERY`→`IDLE` transition (success only) | `setup()` `load_pwm_discovery_` — **overrides** the stiction-floor value from 0x210 when present (encoder-measured MIN is more trustworthy than CHARACTERISE) |

The two `0xAD` slots use the same magic because both came from the same wheel-encoder workstream; they're disambiguated by address and version. Free EEPROM above 0x238 on Mega: ~3.5 KB.

---

## 2. Pre-flight

Before starting any calibration:

- [ ] `mega_balance` env flashed (`pio run -e mega_balance -t upload`).
- [ ] Serial monitor open at 115200 baud. The boot banner `B` appears within ~1 s; failure path prints `BF` and halts.
- [ ] Battery delivering ≥ 4.7 V at the L298N's logic pin under stationary load. Brown-out during EEPROM writes corrupts the slot (CRC then fails on next boot → re-calibration forced).
- [ ] For `c` / `e` / `k`: bot stationary on a stable, level surface.
- [ ] For `p`: bot **lifted off the ground** (wheels free to spin).

If the boot path prints `READY` and prop-and-go fires immediately, send `a` once to abort to IDLE before starting any calibration command.

---

## 3. BNO055 calibration wizard

Runs automatically at boot when no valid cal blob exists at EEPROM 0x000 (or when `r` is wired — there is no `r` command in the current Mega build; the cal blob is replaced only by the wizard's natural path or a deliberate EEPROM wipe).

The Mega code prints a compact progress line:

```
cal g3a3? k p
g=2 a=1 m=0 s=1
g=3 a=2 m=0 s=1
g=3 a=3 m=0 s=2
sv (cal blob saved, returns to main flow)
```

The fields are gyro / accel / mag / system calibration accuracy (0–3 each). The wizard saves automatically when **both** `gyro == 3` and `accel == 3` are observed — `mag` and `sys` are intentionally not required because the balance loop only needs pitch and the magnetometer is rarely usable indoors. Three operator escape hatches:

- `k` — skip saving entirely. The bot continues with uncalibrated IMU (gyro/accel will drift; balance will be poor).
- `p` — save whatever level we currently have. Use if `gyro` reaches 3 but `accel` is stuck at 2 on a level surface.
- Button (D4) short-press — save and continue.

Tips:
- gyro=3: leave the bot completely still for ~5 s.
- accel=3: tip the bot into each of the 6 axis-aligned orientations, pause 1–2 s each.
- mag=3 / sys=3: figure-8 the chassis in a magnetically-clean environment. Often unattainable indoors; not required.

---

## 4. Mount capture — `c`

Captures the chassis pitch at the operator-propped "perfect upright" so the RUN loop can subtract it from the live pitch reading. Without this, the bot tries to balance to whatever angle the IMU breakout happens to be glued at, not to true vertical.

Procedure:

1. Bot at rest in IDLE on a level surface, propped at the operator's intended balance pose.
2. Send `c` (or short-press the button).
3. State transitions to `CAPTURE_MOUNTING`. The firmware accumulates pitch samples with a Welford running variance over `capture_duration_ms = 2000` ms.
4. After 2 s:
   - If σ_pitch ≤ `capture_pitch_var_deg` = 0.5°: mean pitch is recorded as the mount reference and the state auto-chains into `BOOTSTRAP`. `loop()` then writes the new mount value to EEPROM 0x200 (`sv m=<value>` printed on the next `CAPTURE_MOUNTING`→`IDLE` transition — note: in the normal success path the next state is BOOTSTRAP, not IDLE, so the save fires on the BOOTSTRAP-aborted-back-to-IDLE or operator-aborted path).
   - If σ_pitch > 0.5°: bot was jittering; state returns to IDLE. Try again with the bot held more firmly.

You'll see in serial:

```
[state] -> CAP
[state] -> BOOT   (on success path — capture chained into BOOTSTRAP)
```

Cancel anytime with `a`.

---

## 5. Encoder calibration — `e` (Mega only)

Required for the Phase 4M.2 K cross-check and the Phase 4M.14 outer-loop gain derivation to use the derived (not fallback) path.

Procedure (blocking wizard, motors stay off):

1. Bot in IDLE. Send `e`.
2. Firmware zeroes both encoders and prints:
   ```
   enc_cal: roll bot 1.000 m, press btn
   ```
3. Once per second the firmware prints a live tick stream:
   ```
   L_ticks=… R_ticks=…
   ```
   Use this to verify both wheels are counting and the SIGNS make sense (rolling forward should increase both — if one decreases, the A/B leads on that encoder are swapped).
4. Pick the bot up, place it at a marked start line, and **manually roll it exactly 1.000 m forward** along a tape measure.
5. Press the D4 button. Firmware reads final ticks, computes:
   - `CPM_left = |L_ticks| / 1.000`
   - `CPM_right = |R_ticks| / 1.000`
   - `radius_m = counts_per_rev / (mean_CPM · 2π)`
6. Prints `enc_cal: L=… R=… r=… saved` (or `SAVE FAIL`).
7. Both `WheelEncoder` instances now hold the calibrated radius; the live RUN cascade and the next BOOTSTRAP's K cross-check both use it.

To abort without saving: send `a` during the wizard.

If `L_ticks` or `R_ticks` is exactly 0 at button-press, the wizard prints `enc_cal: no ticks - not saved` and returns to IDLE without writing — saves you from corrupting the radius with a dead encoder.

---

## 6. PWM discovery — `p` (Mega only)

Auto-discovers `MIN_PWM` (the smallest commanded PWM that produces non-zero wheel velocity — i.e. the real stiction floor) and `MAX_PWM` (the PWM at which velocity plateaus — saturation onset). Encoder-measured, so strictly more trustworthy than `k`'s gyro-based estimate; the discovered `MIN_PWM` overrides the `0x210` actuator slot at boot.

**The bot MUST be lifted off the ground before `p` is sent.** A bot on the floor when PWM crosses stiction launches off the bench.

Procedure:

1. Pick the bot up, hold or rest it so both wheels can spin freely.
2. Send `p`. State transitions to `PWM_DISCOVERY`.
3. The firmware ramps commanded PWM from 0 upward in `PWM_DISC_STEP_PWM = 5` increments, holding each step for `PWM_DISC_STEP_DURATION_MS = 200` ms (~10 s budget for the full ramp).
4. Per-step telemetry drains as `pd#<step> pwm=<cmd> g0=<L_vel> m=<R_vel> thr=<plateau_delta> ok=<flag>` (`ok=1` = locked MIN, `ok=2` = locked MAX).
5. State returns to IDLE on success, timeout (`PWM_DISC_TIMEOUT_MS = 8000`), collision, or operator abort.
6. On success only, `loop()` writes the bounds to EEPROM 0x230 and immediately applies `min_pwm` to the live driver:
   ```
   sv pd min=… max=…
   ```
   On failure: `pd fail r=<reason>`. Reasons: 4 user_abort, 8 timeout, 9 collision (LIA detector latched mid-ramp — set the bot down more gently next time).

The discovered MAX_PWM is not currently consumed by any in-firmware control limit — the Python brute-force tuner (`tools/sim/brute_tune.py`) reads it off the dashboard / serial stream to seed its PWM search space. Saved unconditionally so the tuner can use it on the next iteration.

---

## 7. CHARACTERISE — `k`

Gyro-based stiction-floor sweep. Older than `p` and lower fidelity — use `p` whenever encoders are available. Provided as a fallback for builds without encoders (Uno) or when the bot can't be safely held off the ground.

Procedure:

1. Bot in IDLE on a stable surface.
2. Send `k`. State transitions to `CHAR_ACT`.
3. The firmware applies a 6-step PWM sweep, measures peak gyro response per step, and records the first PWM that exceeds 3× the measured baseline noise as the stiction floor.
4. State returns to IDLE. On exit (`CHAR_ACT`→`IDLE`), `loop()` writes the result to EEPROM 0x210 if 30 ≤ stiction ≤ 200 PWM, and applies it to the live driver.

`s` will then show the new stiction value in the last field.

---

## 8. BOOTSTRAP — `b` (and prop-and-go)

NOT a one-time calibration. BOOTSTRAP runs every session — it measures K_motor and derives PID gains from current conditions (battery state, surface friction, payload). Gains are deliberately not persisted; the operator preference (`KNOWN_ISSUES.md` §"User preferences") is that calibration values that depend on physical/hardware properties persist, while gains rebuild each session.

Three ways BOOTSTRAP can fire:

| Trigger | When |
|---------|------|
| Prop-and-go (`USE_BALANCE_AUTO_BOOTSTRAP`, default ON in `mega_balance`) | At boot, if a mount offset is saved AND current pitch is within ±5° of it. After a 2 s grace period for operator to clear hands. |
| Capture-success auto-chain | When `c` / short-press completes successfully — `CAPTURE_MOUNTING` transitions directly to `BOOTSTRAP`. |
| Explicit operator | `b` from IDLE (any console); long-press from IDLE (button); short-press from FALLEN. |

Algorithm (~2.5 s wall-time):

```
0..299    ms   BASELINE      motors off, capture peak-to-peak gyro noise
300..449  ms   PULSE 0       +180 PWM/wheel
450..849  ms   COOLDOWN 0    motors off, measure |Δω| → K_0
850..999  ms   PULSE 1       -180 PWM/wheel
1000..1399 ms  COOLDOWN 1    → K_1
1400..1549 ms  PULSE 2       +240 PWM/wheel
1550..1949 ms  COOLDOWN 2    → K_2
1950..2099 ms  PULSE 3       -240 PWM/wheel
2100..2499 ms  COOLDOWN 3    → K_3
≥ 2500    ms   FINALISE      mean K, derive gains, enter RUN (or IDLE on fail)
```

Per-pulse telemetry drains as `bs#<idx> pwm=<signed> g0=<gyro_start> m=<|Δω|> thr=<noise·3> ok=<0|1>`. After all four pulses, on Mega a `bs#0xFD` sentinel line summarises the K cross-check: gyro K, encoder K, the 30 % threshold, pass/fail flag.

If the cross-check passes, the inner PID gains are derived (`Kp = ω_n²/K`, `Kd = 2ζω_n/K`, `Ki = 0.05·Kp`), the outer-loop gains are auto-derived (Phase 4M.14, encoder-equipped builds), and the bot transitions into RUN with the bootstrap-freeze window pre-bypassed (BOOTSTRAP already produced clean K — no need to wait 5 s of natural disturbance).

To disable prop-and-go and require explicit operator trigger, build with `-U USE_BALANCE_AUTO_BOOTSTRAP`.

For exhaustive failure-reason diagnostics see [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md).

---

## 9. Photo-backup recovery — `B` (forward reference)

A sibling agent (AO-FIN-05, this same documentation wave) is adding a `B` serial command that exports the live calibration EEPROM slots as a printable text block over serial — `c=…` for the BNO055 cal blob hex, `m=<value>`, `enc=<L,R,r>`, `pd=<min,max>`. The operator's recovery story is: photograph the printed block; if a board ever loses EEPROM (brown-out corruption, firmware re-flash that wipes EEPROM, board swap), the same `B` command in reverse imports the values back without re-running each calibration step.

This document will be updated in the same wave once `B` lands; the slot layout in [§1](#1-eeprom-persistence-map) above is the authoritative format the printed block mirrors.

---

## 10. Calibration order on a fresh board

The first-ever flash should walk through these in order. Each step's success is a prerequisite for the next.

1. Flash `mega_balance`. Open serial monitor (115200). See `B` then either `READY` (existing cal carried over) or the BNO055 cal wizard prompt.
2. **BNO055 cal** — drive the wizard to `g=3 a=3` then it auto-saves. Or send `p` to save the current level if accel is stuck. (Section [§3](#3-bno055-calibration-wizard).)
3. Send `a` if prop-and-go fired — get to IDLE.
4. **Encoder cal `e`** (Mega only). Roll the bot 1.000 m by hand, press button. (Section [§5](#5-encoder-calibration--e-mega-only).) Verify `r=…` is plausible for your wheel (yellow TT wheels: r ≈ 0.0325 m).
5. **PWM discovery `p`** (Mega only). Lift the bot, send `p`, watch the ramp, set it down on completion. (Section [§6](#6-pwm-discovery--p-mega-only).)
6. **Mount capture `c`**. Prop the bot upright on a stable surface, send `c`. Capture auto-chains into BOOTSTRAP; on the first attempt BOOTSTRAP will likely fail because the bot was let go before pulses started — that's fine, the mount itself was saved.
7. **BOOTSTRAP `b`**. With the bot upright and stable, send `b` and let the 2.5 s sequence run. Watch for `failure_reason = 0` and transition to RUN. If any failure_reason fires, consult [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md).
8. Power-cycle. On the next boot, prop-and-go should fire automatically: `READY` → 2 s grace → `[state] -> BOOT` → `[state] -> RUN`.

After this, [`workstream_g_bench_protocol_2026-05-21.md`](../../findings/workstream_g_bench_protocol_2026-05-21.md) is the procedure for **verifying** the auto-derived outer-loop gains do what they're supposed to. It's a verification protocol, not a tuning protocol — the operator confirms, captures telemetry via `g`, and records into the gain logbook; the operator does not hand-edit `K_POS`/`K_VEL`/`POS_LEAK`.

---

## 11. When to re-run each step

| Event | Re-run |
|-------|--------|
| Replaced or re-soldered the IMU breakout | BNO055 cal + mount capture (`c`) |
| IMU breakout shifted physically on the chassis | Mount capture (`c`) |
| Swapped wheels or replaced a gearbox | Encoder cal (`e`) + PWM discovery (`p`) |
| Battery from full to depleted (or different cell) | Optional: re-trigger BOOTSTRAP (`b`) — K_motor drops 10–25 % with battery sag and the continuous RLS already tracks it, but a fresh BOOTSTRAP gives a clean prior |
| Different drive surface (lab tile → carpet) | Re-trigger BOOTSTRAP (`b`) — K_motor varies with friction |
| Mounted a payload | Mount capture (`c`) — payload shifts CoM and therefore the upright pitch |
| Prop-and-go boot prints `stale_mount p=… m=…` | Mount capture (`c`) — saved mount is more than 5° off the current pitch |
| EEPROM CRC fail on any slot | Re-run the corresponding step. If the same slot keeps failing, suspect brown-out during writes (boost converter sagging under motor load — see `TROUBLESHOOTING.md`). |
| Re-flashed the firmware (any env change) | None automatically — EEPROM persists across flashes. But verify with `s` that the loaded mount / stiction values look sensible. |

---

*Last updated: 2026-06-21 — full rewrite. Documents `c` / `e` / `p` / `k` / `b` / prop-and-go matching the current firmware; adds EEPROM slot map (0x000 / 0x200 / 0x210 / 0x220 / 0x230) with CRC-8-CCITT + version-byte semantics; forward-references the `B` photo-backup command from AO-FIN-05. The legacy AUTO_TUNE/relay-feedback procedure has been removed.*
