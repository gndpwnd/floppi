# Session — 2026-05-18 — Phase 2.x heuristics + flash triage

Continuation of [2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md](2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md). That session bottomed out at **4 B free on Uno** after landing the CHARACTERISE state, blocking every Phase 2.x follow-on. This session resolved the root cause (heavyweight C library code paths, not "the program is too big") and landed three measurement-driven heuristics on top.

---

## Strategic pivot: "stop trimming, start deleting"

Last session ended after 3 hours of byte-scale trims to fit one feature. The user asked the obvious question: *"why can't we fit everything on the Uno? what is so complicated?"*

The honest answer that came out of profiling:

**Comments are free.** The C++ preprocessor strips them; they don't touch flash. The Adafruit BNO055 driver has thousands of lines of comments and costs zero bytes.

**What actually consumes Uno's 32 KB** (validated via `avr-nm --size-sort` on the firmware ELF):

| Cost | Source | Notes |
| --- | --- | --- |
| ~7-8 KB | Adafruit BNO055 + Sensor + BusIO + I2C | Library code path; needed for the IMU |
| ~3-4 KB | Arduino core (Wire / Print / HardwareSerial) | Always linked |
| ~2 KB | Software floating-point (`__addsf3x`, `__mulsf3x`, `__divsf3x`) | AVR has no FPU; every `float * float` is a ~210 B function call |
| **~1.7 KB** | **`Print::printFloat` + libc `snprintf` + `vfprintf`** | **Pulled in by ONE call to `snprintf` in `BNO055::getStatusString`** |
| ~3 KB | Global constructors for static objects | One-shot at boot |
| ~5-6 KB | Balance app proper (state machine + PID + RLS + safety) | Our actual logic |
| ~600 B | `malloc`/`free` | Pulled in by `new Adafruit_BNO055(...)` in `BNO055::begin()` |

The platform tax is ~25 KB before our first line of business logic. The fix is **not** trimming our code — it's **deleting the heavyweight library paths**.

---

## What landed (in priority order)

### 1. `BNO055::getStatusString` — drop `snprintf` (saved 1574 B)

The status method was using `snprintf` to format a 4-byte cal status into a string like `"BNO055: sys=3 acc=2 gyr=3 mag=2"`. `snprintf` drags in libc `vfprintf` (948 B) + the snprintf wrapper (220 B) + Print::printFloat increments.

Rewrote to plain byte assignment producing `"S3223\0"` (sys / accel / gyro / mag digits). The format string is gone; callers that want human-readable output now print labels themselves. Net savings: **1574 B**.

See `src/sensors/bno055.cpp:233`.

### 2. `Adafruit_BNO055` — stack-allocate instead of `new` (saved 124 B flash + 140 B RAM)

`BNO055::begin()` was calling `new Adafruit_BNO055(55, i2c_address_, &Wire)`, which dragged `malloc` + `free` into the binary plus heap fragmentation risk. Moved to a file-scope static instance (`static Adafruit_BNO055 g_adafruit_bno055(55, 0x28, &Wire);` in bno055.cpp). The `i2c_address_` parameter remains in the API for parity but the static path always uses 0x28; a future dual-IMU build would swap in placement-new.

Net: 124 B flash, 140 B RAM (heap pre-allocation gone). See bno055.cpp:55.

### 3. `platformio.ini` — env cleanup (per user request)

Reduced the env count from 14 → 6, with explicit IMU selection per build flag:

```ini
[env:uno_balance]      ; Uno + BNO055 (current bench)
[env:mega_balance]     ; Mega + BNO055 (development headroom)
; [env:esp32_balance]  ; scaffolded - needs MsTimer2 port
; [env:teensy_balance] ; scaffolded - needs MsTimer2 port
[env:mega_orientation] ; Phase 3 BNO085 + GPS + EKF framework
[env:mega_orientation_calibration]
[env:native_test]
```

IMU override at command-line:
```bash
pio run -e mega_balance --project-option="build_flags=-D USE_BNO085 -U USE_BNO055"
```

ESP32 and Teensy envs are commented out — they need real ports (MsTimer2 is AVR-only; ESP32 has no `<avr/pgmspace.h>`; F() macro needs transitive Arduino.h). Documented as TODO in platformio.ini comments.

Also fixed `src/applications/balancing_robot/balance_app.cpp` to shim `PROGMEM` / `pgm_read_byte` on non-AVR platforms.

### 4. Phase 2.1 — measured response threshold for CHARACTERISE (saved 1 hardcoded constant)

The 2026-05-18 sweep reported stiction=30 PWM, which was a noise-floor artefact: the 10 deg/s response threshold was hardcoded and natural settling oscillation tripped it before motors pulsed.

New CHAR_ACT layout:
```
[0..200 ms]    BASELINE   motors off; accumulate |gyro_pitch_dps| × 10
[200..1400 ms] SWEEP      6 pulses x 200 ms, alternating direction
```

Threshold = `max(3 × baseline_acc, 800)` (= avg 2 deg/s minimum). Universal — adapts to any bot's idle noise floor. No more hardcoded 10 deg/s.

See `BalanceApp::step_char_act_` in balance_app.cpp:833. Cost: ~172 B.

### 5. Phase 2.5 — externally-driven-motion HELD trigger (saved 1 hardcoded constant)

Operator's framing (backlog #9): *"If the robot is moving without the motors moving intentionally, then it is falling."*

In the universal-balance vision (no sticky FALLEN), this signal is more useful as an EARLY HELD trigger: if the motor command is small but the pitch gyro shows fast rotation, an external force is moving the bot. Don't fight it — go to HELD until quiet.

```cpp
// Trigger 1: external motion (cmd_mag < 20 PWM AND |gyro_pitch| > 30 dps for 100 ms)
// Trigger 2: legacy lateral-gyro (g_lat > 90 dps OR a_dev > 6 m/s² for 300 ms)
```

The cheap test (no quaternion math) catches the "motors quiet but bot moving" case in 100 ms. The legacy test catches sustained handling that the cheap test misses.

See balance_app.cpp:418. Net cost: ~80 B for both Phase 2.5 and 2.6 combined.

### 6. Phase 2.6 — gain scheduling near balance

Operator's framing (backlog #10): *"more deviation requires more motor speed, less deviation requires less motor speed."*

The previous behaviour: D-term-dominated PWM bursts on micro gyro noise near balance, producing the "motors too high speed" complaint. Fix: linearly scale the PID output by `|pitch_error| / SOFT_ZONE_DEG`, clamped to 1.0. Outside ±2°, full output. Inside, output ramps from 0 (at exact balance) to full (at 2°).

```cpp
constexpr float SOFT_ZONE_DEG = 2.0f;
if (abs_pitch < SOFT_ZONE_DEG) {
    out *= (abs_pitch / SOFT_ZONE_DEG);
}
```

The soft-zone width is currently a hardcoded 2°; the universal version would derive it from the OnlineMountingEstimator's stability metric. Tagged as Phase 2.6.1 follow-up.

See balance_app.cpp:454.

---

## Build state — end of session

| Env | Flash | RAM | Status |
| --- | --- | --- | --- |
| `uno_balance` | 95.9% (30920 / 32256) | 70.4% (1441 / 2048) | flashed + monitored, no anomalies |
| `mega_balance` | 12.5% (31856 / 253952) | 17.6% | builds clean — abundant headroom |
| `native_test` | n/a | n/a | unchanged (this session didn't touch it) |

**Flash freed total: 1698 B.** From 4 B free → 1336 B free after landing three Phase 2.x features. Still on the table for future sessions: removing the relay-feedback auto-tuner (1.3 KB, obsoleted by RLS per backlog #7) and replacing residual `Serial.print(float)` calls (432 B).

---

## 30 s monitor on bench

After flashing, ran a 30 s automated monitor (`python3` + pyserial) reading `s` status every 3 seconds and watching for `ERR` / `FAIL` / `BF` lines.

```
[  0.0] IDLE -1.50 0.87 0 30
[  3.0] IDLE -1.50 0.87 0 30
...
[ 27.1] IDLE -1.50 0.87 0 30
=== anomalies: 0 ===
```

Zero anomalies. State machine running, EEPROM-persisted stiction (30) loaded from last session, mount offset (0.87°) persisted across reboots.

**Caveat**: pitch reading is identical to two decimal places across 30 seconds. Either the bot is genuinely motionless and BNO055 fusion noise is below 0.01° (plausible for NDOF at rest), OR this is the same BNO055-freeze symptom we saw 2026-05-12. Operator should verify by physically tilting the bot — if pitch tracks, sensor is alive; if frozen, USB power-cycle.

---

## What we did NOT do (and why)

- **Phase 2.7 motor-null-space HELD detector**: full quaternion-projected null-space residual gating. Cost: ~150-200 B + ~40 B RAM. Defer to next session because (a) it needs `BNO055::getLinearAccel()` added first, (b) the cheap Phase 2.5 trigger already addresses the most common false-positive (lifted bot), (c) it's research-grade refinement rather than blocker. Tracked in operator_ideas_backlog.md #12.
- **Per-wheel CHARACTERISE sweep**: differential pulses to detect per-wheel stiction divergence. Cost: ~100 B + need motor.set_left()/set_right() separately. Defer because the combined-wheel sweep is functional enough and the per-wheel finding only matters when rubber-band wear becomes the dominant asymmetry source. Tracked in `phase2_characterise_final_plan.md` Phase B.
- **Remove relay tuner**: ~1.3 KB savings available but not blocking. Defer because nothing in the current path needs the savings. Tracked here as next-session optimisation if Phase 2.7 needs the room.

---

## Lessons baked in

1. **Profiling beats guessing.** I assumed for two sessions that the Adafruit library was the flash hog. `avr-nm --size-sort` revealed it was a SINGLE `snprintf` call in `getStatusString` pulling 1.3 KB of libc. Always profile before optimising.
2. **Library API hygiene matters on tiny MCUs.** `getStatusString` returning a printf-formatted blob is convenient on hosted platforms and catastrophic on AVR. The fix (raw byte assembly + caller-side formatting) is also more flexible — the same status data can render different ways for serial vs dashboard.
3. **The user's question is usually the right diagnostic prompt.** *"Why can't we fit everything on the Uno?"* — when I treated this as a genuine question instead of a frustration vent, the profiling investigation followed and the bottleneck collapsed.

---

## See also

- [2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md](2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md) — prior session that landed CHARACTERISE and bottomed out at 4 B free
- [findings/operator_ideas_backlog.md](../../findings/operator_ideas_backlog.md) — entries #9, #10, #12, #16 advanced/landed this session
- [scope.md](../../scope.md) — updated this session with env model + IMU selection + flash strategy
- [src/applications/balancing_robot/balance_app.cpp](../../../src/applications/balancing_robot/balance_app.cpp) — Phase 2.1 / 2.5 / 2.6 implementations
- [src/sensors/bno055.cpp](../../../src/sensors/bno055.cpp) — getStatusString rewrite + stack-allocation
