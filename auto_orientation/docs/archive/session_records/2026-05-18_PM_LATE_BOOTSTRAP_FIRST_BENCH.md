# 2026-05-18 PM late — BOOTSTRAP first bench validation

Continuation of [2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md](2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md). The prior session shipped Phase 4.10c BOOTSTRAP in code; this session put it on real hardware for the first time, found two pre-existing bugs that blocked it, fixed them, observed the bot run end-to-end with telemetry, but did NOT reach stable balance.

---

## Headline

**Motors move. BOOTSTRAP runs end-to-end. Bot does not yet balance.**

First flash with the bug fixes showed all 4 BOOTSTRAP pulses passing on a clean baseline (thr=1.1 dps, metrics 9.5–53.4 dps). The bot transitioned IDLE → BOOTSTRAP → RUN with measured gains. On the bench it "twitched but fell quickly (under a second)" per operator observation. A second power-cycle saw all 4 pulses fail because the bot wasn't at rest during the 300 ms baseline window (gyro range ≈ ±25 dps from operator-placement settling); threshold jumped to 50 dps and even real pulse responses couldn't clear it.

---

## What landed (code)

### 1. BOOTSTRAP noise-threshold bug fixed

**Root cause:** the threshold check compared apples-to-oranges:
- Baseline: peak per-tick |α| from 5 ms gyro differences → ~80–240 dps² at realistic BNO055 noise
- Pulse: |Δω/τ| averaged over 100 ms

The per-tick peak over-estimated noise by 5–20× because the chip only refreshes gyro at 100 Hz (10 ms intervals) while we sample at 200 Hz (5 ms ticks). A real motor response at ~80 dps² could never clear the resulting threshold.

**Fix** ([balance_app.cpp step_bootstrap_](../../src/applications/balancing_robot/balance_app.cpp)):
- Track peak-to-peak gyro_y range over the entire baseline window (same units, same time-domain as the pulse measurement)
- Threshold = 3 × baseline_range (dps), floored at 0.5 dps so a perfectly-quiet gyro doesn't accept noise
- Pulse passes if |Δω| > threshold (directly comparable; no τ division needed)

### 2. Pulse magnitudes raised

100/150 PWM was producing thin Δω even when motors moved. Raised to:
- `PULSE_PWMS[4] = {180, 180, 240, 240}` (was {100, 100, 150, 150})
- `PULSE_MS = 150` (was 100)
- `COOLDOWN_MS = 150` (was 100)

Total BOOTSTRAP duration ~1.5 s (was ~1.1 s).

### 3. Per-pulse Serial telemetry for BOOTSTRAP AND CHARACTERISE

Added a `PulseLog` struct + `drain_pulse_log()` template to `BalanceApp`. Both `step_bootstrap_` (in cooldown branch) and `step_char_act_` (at each pulse boundary + final pulse) populate the struct via the ISR-safe deferred-publish pattern; `loop()` in `main.cpp` drains it with a monotonic `seq` watcher and emits one line per pulse:

```
bs#0 pwm=180 g0=-0.1 m=9.5 thr=1.1 ok=1
ch#3 pwm=-120 g0=0.0 m=42.0 thr=15.0 ok=1
```

This is the visibility that closed the "motors not moving" debugging loop in seconds — first run showed all 4 BOOTSTRAP pulses passing.

### 4. Test fixture sync

`tests/test_balance_app_bootstrap.cpp` had hardcoded `PULSE_MS=100`, `COOLDOWN_MS=100`, `PWMS={100,100,150,150}` in the synthetic-plant loop. Updated to match the new firmware constants (150 / 150 / {180,180,240,240}). 27/27 BOOTSTRAP tests still pass.

### Build state

| Env | Before | After |
|---|---|---|
| `uno_balance` flash | 92.2% (2518 B free) | **94.9% (1652 B free)** |
| `uno_balance` RAM | 64.7% | 65.4% |

Cost: ~860 B flash for the PulseLog struct + drain template + main.cpp drain call + F-string telemetry literals. Still inside Uno budget but tightening.

---

## What we observed on the bench

### First boot, fresh after flash — BOOTSTRAP succeeded:

```
[state] -> BOOT
bs#0 pwm= 180  g0= -0.1  m= 9.5   thr=1.1  ok=1
bs#1 pwm=-180  g0=-25.5  m=27.7   thr=1.1  ok=1
bs#2 pwm= 240  g0=+20.3  m=53.4   thr=1.1  ok=1
bs#3 pwm=-240  g0=+ 1.9  m= 6.6   thr=1.1  ok=1
[state] -> RUN
```

K_motor per pulse:
- Pulse 0: K = 9.5 / (0.15 × 360) = **0.18**
- Pulse 1: K = 27.7 / (0.15 × 360) = **0.51**
- Pulse 2: K = 53.4 / (0.15 × 480) = **0.74**
- Pulse 3: K = 6.6 / (0.15 × 480) = **0.09**

Mean K = 0.38 → Kp = 64/0.38 ≈ 168, Kd ≈ 30, Ki ≈ 8 (pole-placement, ω_n=8 rad/s, ζ=0.7). Within plant_id class bounds.

**Operator report:** "Twitched but fell quickly (under a second)."

### Second boot (after reset) — BOOTSTRAP failed:

```
[state] -> BOOT
bs#0 pwm= 180  g0=-5.8   m=15.5   thr=50.0  ok=0
bs#1 pwm=-180  g0=-2.3   m= 3.8   thr=50.0  ok=0
bs#2 pwm= 240  g0=+0.9   m= 4.4   thr=50.0  ok=0
bs#3 pwm=-240  g0=+13.7  m=32.3   thr=50.0  ok=0
[state] -> IDLE
```

Threshold of 50 dps means baseline peak-to-peak ≈ 16.7 dps — the bot was clearly NOT at rest during the 300 ms baseline window. Likely the operator was still touching it, or the bot was settling from being placed.

---

## Open problems for next session

### Z. Bot wanders during testing and collides with stuff (operator-reported, this session)

The balance loop constrains pitch but not position — the bot drifts forward/backward during BOOTSTRAP, CHARACTERISE, and RUN. With no encoders, no optical flow, and no bumpers, the bot can hit walls / kill-switches / cables, poisoning every algorithm that assumes "no external forces during the test window":

- **BOOTSTRAP**: a mid-pulse collision spikes |Δω| into the threshold but the cause is the impact, not the plant. K_est goes bogus. Already suspected as one contributor to the 0.09–0.74 K spread seen this session.
- **CHARACTERISE**: at low PWMs the bot might be in contact with something at rest. When it "moves" we attribute it to motor force breaking stiction; in reality it broke a static-contact force. Stiction estimate is high.
- **RUN**: bot wanders, hits a barrier, can't recover, falls.

**Code-side mitigations using BNO055 alone** (each ~50–100 B flash, all small):

1. **Collision detection via BNO055 VECTOR_LINEARACCEL**. Magnitude spike > 15 m/s² (1.5 g body-frame) over a few ms = impact. Probe `Adafruit_BNO055::getVector(VECTOR_LINEARACCEL)` — already exposed by the library, returns m/s² of total accel with gravity removed.
2. **BOOTSTRAP/CHARACTERISE collision-abort**: spike during baseline or any pulse → abandon the sequence, IDLE with new `failure_reason=5 (collision)`. Operator repositions the bot in the safe area and retries.
3. **RUN collision → HELD**: spike during balance → motors off, wait for the existing quiet+level resume criteria. Replaces "PID over-corrects an impact disturbance".
4. **Optional: drop boot-time auto-BOOTSTRAP**. Currently `setup()` fires BOOTSTRAP automatically 2 s after power-on if a mount is saved. With wandering risk, the operator may want to position the bot in the test area first. Add a build flag (`USE_BALANCE_AUTO_BOOTSTRAP`) to make auto-fire opt-in.

**Research items (more than a one-session change)**:

- **Position-cascade balance control with IMU-only**: double-integrate BNO055 linear accel for short-term position estimate; use it as an outer loop to nudge pitch setpoint and keep the bot near the origin point. Drift is bounded over tens of seconds — adequate for indoor benches. Reference: search for "linear accel position estimate balance bot", "two-wheeled inverted pendulum drift containment IMU only".
- **Collision signature characterization**: empirical accel traces from a bench impact vs balance recovery. Distinguishability claim: collisions are sharp (50–150 ms peak), body-X dominant, with abrupt onset; recovery accel is more gradual, more aligned with gravity vector. Needs bench traces to confirm.
- **Safe-test-area workflow**: operator ops doc covering physical layout (mat, barriers, kill-switch placement) + power-on / reset procedure. Out of scope for firmware research per se, but reduces test-cycle waste.

### A. K_motor inconsistency across pulses (8× spread)

`g0` (gyro at pulse start) values: −0.1, −25.5, +20.3, +1.9. The bot is gaining significant pitch-axis momentum during pulse 0 (drove the wheels forward → reaction torque tipped chassis backward → −25 dps). Pulse 1 fires at −25 dps already in motion; its measured |Δω| is partly braking the prior momentum, not pure plant excitation.

150 ms cooldown is too short for the bot to settle. Options:
1. Longer cooldown (500 ms+) — adds latency but produces cleaner samples
2. Active brake during cooldown (brief reverse pulse to null momentum)
3. Skip pulses whose start gyro |g0| > 5 dps (sample-quality gate)
4. All three

Option 3 is the cheapest fix and the most physically defensible.

### B. Operator-motion-poisons-the-baseline

If anyone touches the bot during the 300 ms baseline window, peak-to-peak gyro shoots into double digits → threshold becomes unreachable → all pulses fail → IDLE with `no_response`.

Options:
1. Cap threshold (e.g. max 5 dps) — defensive ceiling
2. Quiet-baseline gate — measure baseline, if peak-to-peak > N dps restart the window (up to M times); else fail
3. Longer grace period before auto-bootstrap fires (currently 2 s)

Option 1 is the smallest change. Option 2 is the cleanest.

### C. Bot doesn't balance even when BOOTSTRAP succeeds

Twitch-and-fall after RUN entry with measured gains. Possible causes:
- K_est = 0.38 is the mean of a wildly inconsistent set (see A) — likely far from the true plant K
- Derived gains may be too aggressive (Kp = 168 PWM/deg is a strong gain; ±1.5° saturates the ±255 output)
- Bot may not be sitting at the true balance point — mount offset of 0.87° was last captured 2026-05-18 PM, may be stale after bench moves
- The ω_n = 8 rad/s pole target may be too aggressive for BNO055 NDOF group delay (~25 ms phase lag → ~11° phase margin loss at gain crossover)

Next session needs to: get a clean K measurement (fix A), recapture mount, observe what RUN actually does (post-RUN telemetry — currently no per-tick output during RUN), tune ω_n down if necessary.

---

## Files changed

```
src/applications/balancing_robot/balance_app.h    +28 lines  (PulseLog struct + drain template)
src/applications/balancing_robot/balance_app.cpp  ~60 line changes (threshold fix, telemetry emit, magnitudes)
src/main.cpp                                      +5 lines   (drain_pulse_log in loop())
tests/test_balance_app_bootstrap.cpp              ~6 line changes (PULSE_MS/PWM constants sync, tick budget)
```

Native test results: 27/27 BOOTSTRAP tests pass after the firmware constant changes were mirrored in the test's synthetic plant.

---

## Lessons

1. **Telemetry IS the debugging tool.** "Motors not moving" went from a black-box bench observation to a one-line proof (`bs#0 pwm=180 ... ok=1`) in one flash cycle. Every state machine that produces a binary outcome (pass/fail) should emit per-step diagnostics; the cost is a few hundred bytes of flash and the benefit is sessions that don't repeat.
2. **Unit math for threshold gates is not optional.** A threshold derived from one time scale and applied to a metric measured at another can fail silently — pass on synthetic tests (no noise), fail on hardware (real noise dominates).
3. **Bench reality compresses calendar time.** Two boot cycles surfaced two real bugs that 123 native tests didn't catch. Bench access remains the single highest-leverage activity for this project.

---

## Cross-links

- Prior session: [2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md](2026-05-18_PM_BOOTSTRAP_PHASE_4_10C_LANDED.md)
- Reference: [findings/bootstrap_protocol_unstable_plant.md](../../findings/bootstrap_protocol_unstable_plant.md)
- Scope audit: [scope.md](../../scope.md#current-scope-violations--audit-2026-05-18-updated-pm-evening-phase-410c-landed) — open violations 14/21 still apply
