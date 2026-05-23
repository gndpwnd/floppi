# Security & Safety Audit — auto_orientation framework (2026-05-22)

**Auditor:** security-reviewer (Claude Code Orchestra)
**Scope:** `auto_orientation/` — persistent storage / calibration, auto-tune / BOOTSTRAP
safety bounds, serial command surface, memory safety, NaN propagation into motor
commands, and the new `noise_floor_estimator.h`.
**Method:** static read of the real source (no exploits, no builds, no fixes).
**Predecessor:** `docs/findings/audit_security_2026-05-20.md` (28 findings). This pass
re-verifies the 2026-05-20 fixes that touched the files in scope and adds new findings.

This is a motor-driving balancing robot. SAFETY = security here. The focus is on whether
corrupt EEPROM, bad measurements, or NaN can drive the L298N motors with garbage, and
whether the failsafes actually catch the bad cases.

---

## Severity counts

| Severity | Count | Meaning |
|----------|-------|---------|
| P0 | 0 | Runaway motor / hardware damage reachable in normal operation |
| P1 | 3 | Failsafe gap / unbounded value into the motor path under a plausible fault |
| P2 | 6 | Robustness — degraded behaviour, missing guard, defense-in-depth |
| P3 | 4 | Hygiene / latent / documentation-grade |

No P0 was found. The layered design (PID NaN guard, soft-cutoff at 25°, ±20° kill-switch,
output clamp, CRC + version + buf_capacity on calibration restore) is genuinely solid.
The P1s are all **failsafe-completeness gaps**: the bad input is *usually* caught by some
downstream layer, but there is a reachable path where the catch does not fire.

---

## Verification of prior (2026-05-20) fixes — all CONFIRMED in scope

| Prior finding | Status in current tree |
|---------------|------------------------|
| P1-015 weak XOR CRC → CRC-8-CCITT | FIXED. `util::crc8_ccitt()` (`src/util/crc8.h:49`) is the single shared leaf; `calculateCRC8` delegates (`calibration_storage.cpp:39`). Mount/actuator/encoder/PWM-disc EEPROM slots in `main.cpp` all use it. |
| P2-008 / P2-018 length field uint8_t | FIXED. v2 header stores length as uint16_t LE (`calibration_storage.cpp:82-83,153`). |
| P2-016 version mismatch silently accepted | FIXED. `version != CAL_FORMAT_VERSION` now rejects (`calibration_storage.cpp:148`). |
| `restoreFromEEPROM` buffer overflow | FIXED + verified at call site. `buf_capacity` param added (`calibration_storage.cpp:163`); the only firmware caller passes `sizeof(cb)` correctly (`main.cpp:489`, `cb[32]`, expects `cl != 22`). No unsafe call site found. |

The `restoreFromEEPROM` guard chain is correct and complete: null check → marker → version
→ `stored_length==0 || > CAL_DATA_MAX_SIZE` → `stored_length > buf_capacity` → CRC. A garbage
EEPROM cannot overflow `cb[32]` and cannot pass CRC by accident at any realistic rate.

---

## P1 findings (failsafe gaps — fix before next bench session)

### P1-001 — NaN pitch bypasses the ±20° kill-switch AND the PID holds last (possibly saturated) output
**File:** `src/main.cpp:737-743` (kill-switch), `src/control/pid_controller.cpp:131-133`,
`src/applications/balancing_robot/balance_app.cpp:1783-1789` (`read_imu_`), `:1941` (`corrected_pitch_`).

**Impact:** The belt-and-suspenders kill-switch is `if (p > 20.0f || p < -20.0f)`. If the
BNO055 returns NaN pitch (transient I2C glitch, fusion error — the prior audit P2-012 notes
the BNO055 read is *not* NaN-checked), **both comparisons are false** (all NaN comparisons are
false), so the kill-switch never fires. Meanwhile the PID's own NaN guard returns
`last_output_` — which on the previous tick may have been a saturated ±255. Net effect: a NaN
pitch can leave the motors latched at the last commanded PWM with the highest-level failsafe
silently skipped. `read_imu_` (`balance_app.cpp:1788-1789`) copies `od.pitch_deg` straight into
`pitch_deg_` with no finiteness check; this was flagged P3-014 in the prior audit but its
interaction with the kill-switch makes it P1 here.

**Why P1 not P3:** the prior audit rated the missing pitch NaN-check P3 because "the PID
catches it." This audit finds the PID does *not* fully catch it — it freezes the last output
rather than failing safe, and the supposed catch-all kill-switch is itself NaN-blind. Two
defenses, both bypassed by the same single fault.

**Remediation (statically fixable):** (a) In `read_imu_`, reject NaN/Inf pitch before storing —
hold last-good and increment a fault counter (mirror the Uno app's `isnan(raw)` check,
prior-audit P3-014). (b) Make the kill-switch finiteness-aware:
`if (isnan(p) || p > 20.0f || p < -20.0f) { motors.stop(); safety.request_abort(); }`.
(c) Consider making the PID NaN path return `0.0f` (or decay last_output_) rather than holding a
possibly-saturated value, OR have the caller force `motors.stop()` whenever the IMU read fails.

---

### P1-002 — Mounting offset loaded from EEPROM with no NaN / range guard; poisons corrected_pitch and the OnlineMountingEstimator clamp
**File:** `src/main.cpp:208-215` (`load_mount_offset_`), `:586-587` (apply),
`src/navigation/online_mounting_estimator.cpp:52-66` (`reset_to_reference`/`initialize`),
`:189-196` (clamp with NaN reference).

**Impact:** `load_mount_offset_` validates magic + version + CRC, then `memcpy`s 4 raw bytes
into a float and returns it with **no `isnan`/range check** — unlike the encoder-cal path
(`main.cpp:518-519`, which gates `radius > 0 && radius < 1`) and the PWM-disc path (range-gated).
A CRC-valid record whose float payload is NaN/Inf (e.g. a record written before a partial-flash
event, or a corrupted-but-CRC-coincident blob) flows into
`online_est.reset_to_reference(off)`. That sets `reference_deg_ = NaN`. Subsequently the
estimator's hard clamp `clamp_(candidate, ref - max_dev, ref + max_dev)`
(`online_mounting_estimator.cpp:194`) has NaN bounds, so the clamp passes everything — the
mount estimate is no longer bounded. `corrected_pitch_() = pitch_deg_ - get_estimate_deg()`
(`balance_app.cpp:1942`) then becomes NaN, feeding the same PID-freeze path as P1-001.

**Mitigating factor:** the auto-bootstrap gate `abs_err < 5.0f` (`main.cpp:624`) is NaN-false,
so prop-and-go will *not* fire BOOTSTRAP with a NaN mount (it lands in IDLE — good). But a
manual `b`/`t`/long-press BOOTSTRAP or short-press CAPTURE→BOOTSTRAP bypasses that gate and
proceeds with a poisoned offset.

**Remediation (statically fixable):** in `load_mount_offset_`, after the `memcpy`, add
`if (isnan(deg) || deg < -90.0f || deg > 90.0f) return false;` (a mounting offset outside ±90°
is physically nonsensical). Defense-in-depth: have `OnlineMountingEstimator::initialize()`
reject a non-finite reference (clamp to 0).

---

### P1-003 — BOOTSTRAP-derived gains are pushed to the PID with no finiteness check; NaN gain defeats the output clamp
**File:** `src/applications/balancing_robot/balance_app.cpp:1502-1527` (FINALISE pushes
`ps.kp_target/kd_target/ki_target`), `src/control/plant_identifier.cpp:262-283`
(`recompute_targets_`), `src/control/pid_controller.cpp:77-85` (`set_tunings`),
`:196-197` (output clamp).

**Impact:** `set_tunings` rejects **negative** gains (`< 0 → 0`) but does **not** reject
NaN/Inf — `(NaN < 0.0f)` is false, so `kp_ = NaN` is stored. The final output clamp
`clamp_(output, output_min_, output_max_)` (`pid_controller.cpp:281-285`) is `if (v < lo)…if
(v > hi)…return v;` — with `v = NaN`, both comparisons are false, so **NaN passes the clamp**
and reaches `motors_.set_speed(pwm)` via `(int16_t)out`. Casting a NaN float to int16_t is
**undefined behaviour** and on AVR/ARM typically yields 0 or an arbitrary value — i.e. an
unpredictable PWM, not a guaranteed-safe one.

Can a NaN gain actually be produced? `recompute_targets_` guards `theta_` away from zero
(`th < 1e-3f → 1e-3f`) and `seed_k_motor` clamps K into `(k_min, k_max)`, so the *normal*
path is finite. The residual risk paths: `ts_target_` is floored at 0.1 (safe), but
`recompute_targets_` is also reachable via `set_*` setters and `reset(kp_initial)` where
`theta_ = (wn*wn)/kp_initial` — `kp_initial` comes from `pid_.get_tunings()` and is only
clamped ≥0, so a previously-NaN-poisoned gain (e.g. via P1-001/P1-002 chains, or a future
caller) would propagate. The exposure is small but the consequence (NaN→int16_t→motor) is
severe and the guard is one line.

**Remediation (statically fixable):** in `PIDController::set_tunings`, reject non-finite:
`kp_ = (isnan(kp) || kp < 0.0f) ? 0.0f : kp;` (and ki, kd). Belt-and-suspenders: in the
output clamp, treat NaN as 0 (`if (isnan(v)) return 0.0f;` at the top of `clamp_`), which
closes this for *every* output path, not just gains. This single clamp fix also hardens
P1-001's `last_output_` concern.

---

## P2 findings (robustness / defense-in-depth)

### P2-001 — `int16_t` PWM cast can wrap before the driver clamp sees it
**File:** `balance_app.cpp:577` `const int16_t pwm = (int16_t)out;` then `:602`
`motors_.set_speed(pwm)`; clamp lives in `l298n_motor_driver.cpp:54-58` on an `int16_t`.

**Impact:** `out` is a float already clamped by the PID to `[output_min, output_max]`
(±255 in RUN), so in normal operation `(int16_t)out ∈ [-255,255]` and `clamp_speed` is a no-op.
But if the PID clamp is ever bypassed (NaN per P1-003, or `output_max` set huge), a float >32767
casts to a wrapped/UB int16_t *before* `clamp_speed` runs, so the driver clamp cannot save it.
The clamp is on the wrong side of the cast.

**Remediation:** clamp `out` as a float to `[-255,255]` before the int cast, OR widen the
driver clamp input to float. Low effort.

### P2-002 — `noise_floor_estimator.h` Welford variance: no NaN/Inf input rejection
**File:** `src/applications/balancing_robot/noise_floor_estimator.h:67-81` (`push`).

**Impact:** The divide-by-zero analysis is **clean** — `count_ - 1` is only used once
`count_ >= kWindowSamples` (=200), so the divisor is ≥199, and `sqrtf(variance)` is guarded
`variance > 0.0f`. No overflow/divide bug. However, if a NaN sample is pushed (gyro/accel
glitch), `mean_` and `m2_` become NaN and `settled()` latches a NaN `std_dev_`. The module is
documented as PURE OBSERVATION with nothing downstream consuming it *yet* — so this is P2, not
P1 — but the docstring's own roadmap (derive `STUCK_GYRO_DPS = 3·σ` etc.) means a NaN floor
would later feed thresholds. The feed gate in `read_imu_` (`balance_app.cpp:1882-1893`) also
does not pre-screen NaN.

**Remediation:** add `if (isnan(sample)) { abort_window(); return; }` at the top of `push()`.
Trivial, and future-proofs the consumers.

### P2-003 — `m2_` can accumulate to +Inf for a pathological non-quiet signal
**File:** `noise_floor_estimator.h:73` `m2_ += delta * delta2;`.

**Impact:** `m2_` is a float sum of squared deviations over up to 200 samples. The feed gate
only admits "quiet" samples (`g_lat < NOISE_FLOOR_QUIET_GYRO_DPS`), so realistic magnitudes
keep `m2_` tiny; overflow is not reachable with sane sensor data. Listed for completeness —
the same `isnan/isinf` guard from P2-002 covers it.

### P2-004 — `derive_position_gains_` uses `expf` and float pole-placement with no NaN result check
**File:** `balance_app.cpp:1636-1657`.

**Impact:** The derivation is well-guarded (`g_outer > 1e-9f`, `dt_sec > 0`, and every raw
value is range-clamped via `kpos_ok/kvel_ok/leak_ok` before use, falling back to constants
otherwise). A NaN `raw_k_pos` would fail the `>=MIN && <=MAX` range test (NaN-false) and
correctly fall back. So this is actually **safe by construction** — noted only to confirm the
clamp-then-reject pattern here is the *correct* model the rest of the codebase should follow
(contrast P1-003 which lacks it).

### P2-005 — `ramp_gain_` propagates a NaN target straight into `applied_kp_`
**File:** `balance_app.cpp:1945-1956`, called `:759-762`.

**Impact:** `ramp_gain_(applied_kp_, ps.kp_target, dt)` bounds the *step* but if `target` is
NaN, `delta = target - live = NaN`, the `delta > bound`/`delta < -bound` clamps are NaN-false,
and `live += NaN → NaN`. That NaN then goes to `pid_.set_tunings(applied_kp_,…)`. Same root as
P1-003 (PlantIdentifier targets are finite in the normal path), so same one-line fix in
`set_tunings` closes it. Kept separate because `ramp_gain_` is the *adaptive RUN-time* path
(continuous), distinct from the BOOTSTRAP one-shot push.

### P2-006 — Watchdog never disarms the motors on its own
**File:** `src/applications/balancing_robot/safety.cpp:52-59` (`watchdog_starved`),
`safety.h:55-62`. No caller invokes `watchdog_starved()`.

**Impact:** `BalanceSafety` implements a watchdog (fed every `tick()` at `balance_app.cpp:325`)
but **nothing reads `watchdog_starved()`** anywhere in `balance_app.cpp` or `main.cpp`. The
watchdog is fed but never checked, so a stalled `tick()` ISR (the very failure it exists to
catch) produces no motor cut. The actual protection against a stalled loop is the external
±20° kill-switch in `loop()` — which itself only runs if `loop()` is alive. If the MsTimer2
ISR wedges while `loop()` runs, motors hold their last command indefinitely. Prior audit
P2-021 is adjacent (no watchdog kick on failure paths); this is the complementary "watchdog
result is never consumed" gap.

**Remediation:** in `loop()` (or a hardware WDT), check `safety.watchdog_starved(now)` and
`motors.stop()` + abort if starved. Consider arming the AVR hardware watchdog
(`<avr/wdt.h>`) as the true last resort.

---

## P3 findings (hygiene / latent)

### P3-001 — `hasCalibrationInEEPROM()` checks only the marker byte, not CRC
**File:** `calibration_storage.cpp:189-201`. Documented as intentional ("matches legacy
single-byte semantics"). A 0xCA marker with a corrupt payload reports "have calibration";
the actual restore still CRC-checks, so no unsafe data reaches the sensor. Low risk; noted
because a caller could branch on `hasCalibration` and skip the wizard expecting valid data.

### P3-002 — `clearCalibrationFromEEPROM` leaves payload intact (marker-only erase)
**File:** `calibration_storage.cpp:207-220`. By design (fast/reversible). Not a leak concern
for this device class, but worth noting the "cleared" data is still physically present.

### P3-003 — Uno hardcoded gains (`BALANCE_KP/KI/KD`) bypass all the adaptive safety machinery
**File:** `src/applications/balancing_robot_uno/main.cpp:157-159`. The Uno minimal build uses
compile-time gains and does not have BOOTSTRAP/PlantIdentifier. This is the documented design
(Uno = small hardcoded program). Its `read_imu` *does* check `isnan(raw)` (prior audit P3-014),
so it is actually safer than the Mega path re: P1-001. Noted for cross-stack consistency: the
Mega should adopt the Uno's pitch-NaN guard.

### P3-004 — Serial command surface is single-char dispatch — no injection/overflow risk
**File:** `main.cpp:651-716`, `balancing_robot_uno/main.cpp:108-148`. Both parsers read one
char and `switch` with a safe default (ignore). No buffers, no length fields, no `atoi` on
attacker-influenced strings, no array indexing by input. **Clean** — recorded as a positive
finding. The only state-changing commands (`b`, `c`, `t`, `p`, `k`, `e`) gate on `state_ ==
IDLE` inside the app, so a stray byte cannot start motors mid-RUN.

---

## Statically-fixable vs bench-gated

**Statically fixable now (no robot needed):**
- P1-001 — pitch NaN guard in `read_imu_` + NaN-aware kill-switch (`main.cpp`/`balance_app.cpp`)
- P1-002 — `isnan`/±90° range guard in `load_mount_offset_` (`main.cpp`)
- P1-003 — `isnan` reject in `PIDController::set_tunings` + NaN→0 in `clamp_` (`pid_controller.cpp`)
- P2-001 — clamp `out` as float before `(int16_t)` cast (`balance_app.cpp`)
- P2-002 / P2-003 — `isnan` guard in `NoiseFloorEstimator::push` (`noise_floor_estimator.h`)
- P2-005 — covered by the P1-003 `set_tunings` fix
- P2-006 — wire `watchdog_starved()` into `loop()` + optional AVR hardware WDT
- P3-001 / P3-003 — small consistency edits

**Bench-gated (need the physical bot to validate behaviour, not just compile):**
- Confirming P1-001's kill-switch fix actually cuts motors on a real BNO055 NaN event.
- Validating that a NaN→0 PID clamp does not introduce a control glitch on a transient.
- Confirming the watchdog-starved cut (P2-006) timing does not false-trip at 200 Hz.

**Note:** all P1/P2 remediations are one-to-a-few lines and behaviour-preserving on the
happy path — they only change what happens on already-anomalous (NaN/Inf/stall) inputs.

---

## Top 3 issues (start here)

1. **P1-001** — NaN pitch bypasses BOTH the ±20° kill-switch and the PID's protective intent
   (PID holds last, possibly-saturated, output). Two failsafes, one fault defeats both.
2. **P1-003** — NaN/Inf PID gains pass `set_tunings` (only `<0` is rejected) and then pass the
   output clamp (NaN comparisons are false), so a NaN reaches `(int16_t)out` → motor (UB cast).
   A single `isnan` check in the clamp closes the entire class.
3. **P1-002** — Mounting offset is the only EEPROM-loaded float with no NaN/range guard
   (encoder radius and PWM-disc bounds *are* guarded); a CRC-valid garbage offset poisons
   `corrected_pitch_` and disables the OnlineMountingEstimator clamp.

The unifying theme across all three P1s: **NaN comparisons are always false, so every
`if (x > limit)` clamp/gate in the motor path is silently no-op on NaN.** The codebase already
knows this (the PID and quaternion paths check `isnan`), but the knowledge is applied
unevenly. A project-wide "reject non-finite at every sensor boundary and before every motor
write" pass — the prior audit's own closing recommendation (line 298) — would retire all three.
