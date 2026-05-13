# Latency Budget — Why the Balance Robot Feels Delayed

Status: diagnostic / recommendations. Latest bench feedback 2026-05-12.

## Symptom

User report, verbatim:

> "the robot is delayed in trying to balance — it lets itself fall over then
> tries to start the motors"

Phase-lag complaint, not a gain complaint. The controller is acting on a
stale picture of the world.

## End-to-end latency budget

One PID tick is **5 ms** (`kDefaultPidSampleMs = 5` in `balance_app.cpp:65`;
gated in `main.cpp:425` as `if (now - last_step >= 5)`).

| # | Stage | Typical (ms) | Worst (ms) | Source |
| - | ----- | ------------ | ---------- | ------ |
| 1 | BNO055 NDOF internal fusion group delay | 20 | 40 | BNO055 datasheet §3.6.2 + Bosch BSX whitepaper |
| 2 | I²C read: quat+gyro+accel+cal ≈ 24–32 B at 400 kHz | 1.0 | 1.5 | `bno055.cpp:93`; 32 B × 9 b ÷ 400 kHz ≈ 0.72 ms + START/ACK overhead |
| 3 | PID compute (no sqrtf, 1 LPF, 1 clamp) | 0.05 | 0.1 | `pid_controller.cpp:117` — flat arithmetic |
| 4 | `analogWrite()` → L298N PWM | <0.01 | <0.01 | Atomic OCRxx write; duty latches at next PWM period |
| 5 | Loop scheduler jitter (5 ms tick) | 2.5 | 5.0 | `main.cpp:425` — `if (now - last_step >= 5)` |
| 6 | D-term LPF group delay (first-order, τ=5 ms) | 5.0 | 5.0 | `pid_controller.cpp:36` — `d_term_lpf_tau_sec_ = 0.005f` |
|   | **Sum** | **~28.5** | **~51.6** | |

**Typical ≈ 29 ms. Worst ≈ 52 ms.** Row 1 (BNO055 NDOF group delay) is the
bottleneck — alone larger than the rest of the pipeline combined.

## Phase-lag interpretation

Natural period of an inverted pendulum: T ≈ 2π·√(L/g). With L ≈ 9 cm (CG
height of the bench bot), T ≈ 0.6 s = 600 ms. Phase lag φ = 360°·(Δt/T):

| Latency | Phase lag at T = 600 ms |
| ------- | ----------------------- |
| 29 ms (typical) | **17.4°** |
| 52 ms (worst) | **31.2°** |

Closed-loop systems go marginally stable around 60° of phase margin loss.
**At 17° of pipeline phase lag we burn a third of our margin before the
PID gains contribute anything.** Kd (a phase *lead*) is the only thing
earning it back, and Kd is capped at 10 because its noise pathway is
also delayed.

This matches the symptom: pitch builds, PID sees a stale value, motors
engage past the linear regime, stiction eats remaining authority, bot
tips.

## What just changed (in flight as of this writeup)

- **I²C clock 100 → 400 kHz** (`bno055.cpp:93`). Saves ~3 ms per read.
  KNOWN_ISSUES.md KI-8 and IMPLEMENTATION_PLAN.md Tier 3 step 5.
- **D-term LPF τ 15 → 5 ms** (`pid_controller.cpp:36`,
  `d_term_lpf_tau_sec_(0.005f)`). First-order group delay drops from
  ~15 ms to ~5 ms (≈ 6° less phase lag at T = 600 ms). Kd noise pathway
  gets noisier; acceptable because Kd = 10 is conservative.
- **Output cap 80 → 255** (`balance_app.cpp:75-76,590`, `main.cpp:96`).
  Authority fix, not latency. With ±80 the controller could not produce
  enough torque to catch >5° departures.
- **Ki 0 → 3** (`balance_app.cpp:63`, `main.cpp:96,322`). Chases slow
  drift between mounted IMU zero and true balance point so P-term is
  not always playing catch-up.

Combined: ~13 ms of pipeline latency removed (≈ 8° of phase margin)
plus real authority and drift-tracking gains. If the bot still feels
delayed, the BNO055 NDOF group delay is the only remaining big lever.

## Ranked next-up fixes

### Tier 0 — done this session (0 hours)
- I²C 400 kHz.
- D-term τ → 5 ms.
- Output cap ±255.
- Ki → 3.

### Tier 1 — BNO055 AMG mode + pitch-from-accel (1 hour) — RECOMMENDED

Switch BNO055 to `OPERATION_MODE_AMG` (raw accel + mag + gyro, no on-chip
fusion). Compute pitch as `atan2(ax, az) * 180/π` (~50 µs on Mega).

You lose the fused quaternion (yaw drifts; irrelevant for pitch-only
balance). You gain: the 20–40 ms NDOF group delay collapses to ~5 ms
(raw sample period). Net pipeline latency drops from ~29 ms to **~14 ms**
(≈ 8° phase lag — a 50% improvement). Kd can be raised back toward 20.

Cite: `IMPLEMENTATION_PLAN.md` Tier 3 step 1;
`findings/bno055_latency_and_pitch_fusion.md` §3.

### Tier 2 — Lauszus 2-state Kalman on raw gyro + accel-pitch (3–4 h)

After Tier 1, replace `atan2(ax,az)` with the 2-state Kalman (Lauszus
reference, ~60 LOC, state = [pitch, gyro_bias]). Same latency as Tier 1
but much less noise on pitch, so Kp and Kd can both be turned up.

Cite: `findings/MASTER_DESIGN.md` §4.7 D6.

### Tier 3 — Madgwick on raw IMU (6 h)

Overkill for pitch-only. Only implement if Tier 2 is insufficient
(it won't be). Cite: `findings/MASTER_DESIGN.md` §4.7 D6 footnote.

## Non-latency causes of "feels delayed"

- **Stiction dead-band** — `stiction_min_pwm = 18` (`main.cpp:91`). Below
  18 PWM wheels do not move. If PID requests 5–15 PWM near vertical,
  wheels are silent and the bot keeps tilting. Worth dropping to 12 or 8
  experimentally.
- **Insufficient Kd → no anticipation.** Kd = 10 is conservative because
  Kd × NDOF-noise creates jitter. Tier 1 lets Kd go to ~20.
- **Ki = 0 (now 3).** Without Ki, slow drift accumulates uncorrected.
- **Output cap ±80 → ±255.** Authority, not latency, but looks identical.
- **Mounting offset wrong/stale.** A 2° error becomes a 2° steady-state.
  Re-run `c` (capture) on a level surface.

## Open: pre-fall versus mid-fall behavior

One five-minute test disambiguates "sensor pipeline too slow" from
"stiction dead-band too high":

Hold the bot by hand at ~3° tilt. Watch `out=` from serial `s`.

- `out` = 30–80 PWM, **wheels silent** → not latency. Stiction (or PWM
  not reaching driver). Drop `stiction_min_pwm` to 8 or 12.
- `out` = 5–15 PWM, wheels silent → stiction is correctly the floor,
  controller is under-responding. Raise Kp to ~25, or do Tier 1.
- `out` correct magnitude, motors running, bot still falls → genuinely
  sensor latency / noise. Tier 1–2 plan applies.

Run this before spending four hours on the Kalman filter.
