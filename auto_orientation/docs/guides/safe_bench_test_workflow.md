# Safe Bench Test Workflow — Balancing Robot

Operator procedure for running the self-balancing robot (Phase 4.10c BOOTSTRAP +
collision detector) without wrecking the bot, the bench, or the session.

The bot has **no encoders, no bumpers, and no physical kill switch**. The only
emergency stop is one serial character (`a`) that needs an open serial monitor.
Collisions are caught by the BNO055 detector landed alongside this doc; position
containment is designed but **not yet implemented** ([research_imu_only_position_containment.md](../findings/research_imu_only_position_containment.md)).
The bot WILL drift — plan the bench accordingly.

Last updated: 2026-05-19.

---

## Section 1 — Test area setup

Build the area before plugging in the bot. The other order is how the 2026-05-18
session lost two boot cycles.

- **Mat:** 1.0 m × 1.0 m rubber gym mat (≈ 10 mm closed-cell foam interlocking
  tiles). Carpet is acceptable; bare hardwood/tile is not — wheels skid and the
  BNO055 sees floor-noise that poisons the BOOTSTRAP baseline.
- **Barriers:** couch pillows or 4-inch foam blocks butted against all four mat
  edges. Height = 1.5× the bot's body (≈ 25 cm for the reference bot). Leave one
  corner open as the operator-access lane — reach in, not over.
- **Cable:** USB tether routes from the access corner. **30 cm of slack minimum**
  before the first tie-down. A tugged tether reads as a wall hit and fires the
  collision detector (true positive, wasted run). Tape the cable to the bench
  edge so a wandering bot pulls slack, not the connector.
- **Power:** USB tether only during bench tests. Battery removes the `a` kill
  switch. Save battery for outdoor work after > 30 s of stable RUN.
- **Start orientation:** manually level the bot within **±2°** of vertical before
  power-on. The auto-BOOTSTRAP gate is ±5°, but stale-mount + sloppy stand-up
  combine quickly. Point wheels toward the access corner so the first wander is
  toward you, not into a wall.

---

## Section 2 — Pre-flight checklist (cold start)

| # | Step | Expected |
|---|------|----------|
| 1 | `sudo systemctl stop ModemManager` | No output. Stops USB-CDC probe that corrupts first 5 s of serial. |
| 2 | `fuser -k /dev/ttyACM0` | Clears stale handles. |
| 3 | `pio device monitor -b 115200 -p /dev/ttyACM0` | Opens the kill-switch channel. **Do not skip.** |
| 4 | Plug in USB | Boot banner `B`, then `READY`. |
| 5 | If `BF` printed | IMU init failed. Power-cycle, re-seat I²C. Do not proceed. |
| 6 | Send `s` | `IDLE 0.00 <mount> 0 <stiction>`. Pitch ≈ 0 with bot vertical. |
| 7 | Watch for `stale_mount` | If printed, recapture with `c` before BOOTSTRAP fires. |
| 8 | Confirm collision detector idle | No spontaneous `[state] -> HELD`. Resting `linear_accel_mag` consistently > 4 m/s² means BNO055 mis-cal → re-run `r` wizard. |
| 9 | Verify `USE_BALANCE_AUTO_BOOTSTRAP` | Default ON in `platformio.ini`. ON fires BOOTSTRAP 2 s after `READY` when a mount is saved; OFF waits for `b`. Know which build you have before letting go of the bot. |

If any step fails, **stop and diagnose**. Every minute here saves a fall later.

---

## Section 3 — BOOTSTRAP procedure (with collision detector)

BOOTSTRAP fires four ±PWM pulses (≈ 1.5 s total) to measure `K_motor` before RUN.
The bot lurches; wander is expected.

### Operator stance
**Hands off, palms within 10 cm.** Catch the bot if it dives off the mat; do not
touch it during the 300 ms baseline or any pulse — contact poisons the gyro
baseline and threshold becomes unreachable.

### Reading the PulseLog
Each pulse prints one line:

```
bs#0 pwm= 180  g0= -0.1  m= 9.5   thr=1.1  ok=1
```

| Field | Meaning | Healthy |
|-------|---------|---------|
| `pwm` | Commanded PWM (signed) | ±180, ±240 |
| `g0` | Gyro Y at pulse start (dps) | **|g0| < 5** — larger = prior-pulse momentum contaminating the measurement |
| `m` | |Δω| metric (dps) | 5–100 |
| `thr` | Adaptive noise threshold | **thr < 3** — larger = baseline saw motion |
| `ok` | Pulse passed | want 1 |

A clean run: four `ok=1`, `thr` < 2, `|g0|` < 5 every pulse. Anything else suspect.

### `failure_reason=5 (collision)` — what to do
The detector aborts BOOTSTRAP/CHARACTERISE if it sees > 12 m/s² peak, > 8 m/s²
for 3 ticks, or > 6 m/s² with > 200 dps pitch rate. Bot exits to IDLE.

1. Look at the bot's position — what did it hit (barrier, cable, your hand, mat edge)?
2. Lift bot off mat, re-center facing the access corner.
3. If it hit the same wall twice, add 10 cm of barrier distance — BOOTSTRAP's
   lurch is biased that direction and keeps delivering it there.
4. Re-trigger with `b` (auto-fire is one-shot, does not re-arm).
5. After three collision aborts in a row the **test area is wrong, not the bot**.
   Widen the barriers before retrying.

### Diagnosing poisoned measurements
- `thr` > 10 on pulse 0 → operator touch during baseline. Hands off, retry.
- `|g0|` > 15 on pulse 1+ → 150 ms cooldown insufficient for this chassis. Settle
  longer between `b` commands. (Firmware note: raise `COOLDOWN_MS` to 400 ms.)
- `m` < 3 on a passing pulse → motor barely moved. Check stalled wheel, loose
  wire, low battery.
- Wildly different `m` across pulses (e.g. 9.5 / 27.7 / 53.4 / 6.6) → K_mean
  unreliable, RUN will twitch-fall. Settle and retry.

### When to physically reset
- > 5 failed BOOTSTRAPs in a session → power-cycle the bot. The BNO055 fusion
  engine occasionally wedges; a reset is faster than diagnosing it.
- BOOTSTRAP succeeds but RUN twitches in < 2 s repeatedly → re-capture mount
  (`c`). Mount drift is the likely cause.

---

## Section 4 — RUN-mode operation

PID runs forever (no fall detection by default, per operator preference).

### Where to stand
Operator-access corner, **hands within reach but not touching**. Catch the bot
before any hard collision. Eyes on bot, not on terminal.

### Recognizing imminent collision
- **Persistent tilt toward one wall** > 2 s → wandering, not balancing.
- **Body rocking 2–5 Hz** → PID oscillating. K_motor wrong (re-BOOTSTRAP) or
  pole target too aggressive (drop `ω_n` 8 → 5 rad/s — see [BOOTSTRAP_FIRST_BENCH](../archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md)).
- **Loud rapid wheel-direction switching** → motor saturation, past linear
  region. Catch it.

### HELD on collision
On confirmed collision the bot enters HELD: motors drop to zero, bot **falls**
(no soft landing). HELD is not sticky — re-enters RUN automatically once the
resume gate clears (motion-quiet AND level within ±8° for 200 ms).

Recovery:
1. Pick up bot before it finishes falling, if you can.
2. Re-center on mat, hold vertical within ±2°.
3. Hold steady 1 s — gate needs 200 ms quiescence + level; longer guarantees
   release fires the transition.
4. Release cleanly. No hand jerk — that fires the collision detector again.

### Emergency stop: `a` vs let it fall
- **Use `a`** when the bot is about to hit something you don't want hit (cable
  connector, laptop, coffee). Motors drop in < 50 ms; state → IDLE.
- **Let it fall** when it's past ~ 8° tilt and the fall is into mat or foam.
  Motors spin benignly on the side. Save `a` for unrecoverable situations.
- After `a`: bot is IDLE. Re-trigger with `b` (BOOTSTRAP) or `c` (recapture
  mount, then BOOTSTRAP).

---

## Section 5 — Reset & recovery

### After a fall — physical inspection
| Check | What to look for | Fix |
|-------|------------------|-----|
| Motor wires | Tugged-out connector | Re-seat |
| BNO055 / I²C wires | Loose, kinked, pulled | Re-seat, check continuity if repeated falls |
| Wheels | Cracked hub, slipped on shaft | Tighten set screw or replace |
| Chassis | Bent, twisted, PCB shifted | Straighten. If IMU plane changed, **re-capture mount** before next BOOTSTRAP |
| BNO055 cal | After ~ 5 hard falls `cal_status` may regress | `r` to re-run cal wizard |

### Re-trigger BOOTSTRAP vs trust prior K_motor
- **Re-trigger** after: battery swap, payload change, motor replacement, shifted
  chassis, > 30 min since last BOOTSTRAP.
- **Trust prior** for: < 5 falls, no inspection findings, bot was running cleanly
  before the fall. Re-running is free (1.5 s) — when in doubt, re-run.

### Re-level vs let the estimator track
- **Re-level (recapture with `c`)** if the bot was moved between setups, chassis
  was disassembled, or HELD→RUN keeps failing because the bot is consistently
  > 5° off when set down.
- **Let `OnlineMountingEstimator` track** slow drift (battery sag, payload shift,
  wear). 8 s LPF — invisible in real time but absorbs bias within 30 s of RUN.

---

## Section 6 — Known limitations + queued improvements

| # | Limitation | Status | Workaround |
|---|------------|--------|------------|
| L1 | No position containment | Research done, Phase 4.11a queued | Foam barriers; short sessions; eyes on bot |
| L2 | Collision detector tuned conservatively (12/8/6 m/s²) — may miss soft impacts ([research_collision_signature_bno055.md](../findings/research_collision_signature_bno055.md) §2) | Welford-learned threshold queued (Phase 2.7c) | Use rigid foam barriers, not pillows |
| L3 | No physical kill switch | `a` requires serial connection | Always run with monitor open; never battery-only on bench |
| L4 | BOOTSTRAP K spread (8× observed) | Sample-quality gate queued | Read PulseLog; retry if `|g0|` > 15 dps |
| L5 | Threshold uncapped → operator motion blocks BOOTSTRAP | Cap at 5 dps queued | Hands off during baseline |
| L6 | No per-tick RUN telemetry | Queued | Poll `s` at 1 Hz; lean on visual observation |
| L7 | `ω_n = 8 rad/s` pole target may be too aggressive | Drop to 5 rad/s if RUN keeps twitching | Not currently operator-tunable |

Open blockers tracked in [docs/todo.md](../todo.md) "Next session" section.

---

## Section 7 — Quick-reference card

Print. Tape to bench.

### Serial command cheat-sheet

| Key | Action | When |
|-----|--------|------|
| `s` | Status (state, pitch, mount, output, stiction) | Anytime — read-only |
| `c` | Capture mount → BOOTSTRAP → RUN | Bot vertical, want to re-save mount |
| `t` | Abort, then BOOTSTRAP from current mount | Bot vertical, mount is current |
| `b` | BOOTSTRAP only (skip capture) | Mount current, just want fresh K_motor |
| `k` | CHARACTERISE actuator (stiction floor) | Once per bot at assembly |
| `a` | **EMERGENCY STOP** → IDLE | About to hit something; motors stop < 50 ms |
| `r` | (in cal wizard) re-run BNO055 cal | After falls regress `cal_status` |
| `p` | (in cal wizard) save partial cal | Mag stuck at 2 — accept and continue |

### Boot states (normal flow)

```
B → READY → IDLE → BOOT (BOOTSTRAP) → RUN ⇄ HELD
                                       ↓
                                     IDLE  (on `a` or collision-during-BOOTSTRAP)
```

### Emergency procedure
1. **`a`** in serial monitor → motors stop, bot falls.
2. **Catch the bot** if possible.
3. **Set bot aside vertical** on the mat. Do not RUN immediately.
4. **`s`** → confirm state is `IDLE`.
5. **Inspect** (Section 5).
6. **Re-prop and `b`** when ready. Or `c` if mount drift suspected.

### PulseLog quick-read

```
bs#0 pwm= 180  g0= -0.1  m= 9.5   thr=1.1  ok=1     ← GOOD
bs#1 pwm=-180  g0=-25    m=27.7   thr=1.1  ok=1     ← g0 too big, K contaminated
bs#2 pwm= 240  g0= +0.9  m= 4.4   thr=50.0 ok=0     ← thr too big, baseline poisoned
```

Want: `|g0| < 5`, `thr < 3`, `ok=1` every line.

### If in doubt
- Stop (`a`).
- Read the last 10 lines of serial.
- Fix the obvious thing (re-level, re-center, barriers).
- One more try.
- If two attempts fail the same way, **stop the session** and diagnose in
  writing. Bench time is expensive; debugging blind is more expensive.

---

## See also

- [research_collision_signature_bno055.md](../findings/research_collision_signature_bno055.md) — threshold derivation
- [research_imu_only_position_containment.md](../findings/research_imu_only_position_containment.md) — queued position containment
- [BOOTSTRAP_FIRST_BENCH session record](../archive/session_records/2026-05-18_PM_LATE_BOOTSTRAP_FIRST_BENCH.md) — pain points this doc addresses
- [todo.md](../todo.md) — open blockers
