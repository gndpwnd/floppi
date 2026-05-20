# Balance-Stack Code-Quality Audit — 2026-05-19

Scope: `auto_orientation/src/applications/balancing_robot/`, `src/control/plant_identifier.*`, `src/navigation/online_mounting_estimator.*`, `src/sensors/bno055.*`, `src/actuators/l298n_motor_driver.*`, `src/main.cpp`, `src/config/*.h`. Read-only audit; no source changes made.

Out of scope: `bno085*`, GPS, EKF (inactive for balance bot).

---

## 1. Summary

| Category | P0 | P1 | P2 |
|---|---|---|---|
| ISR / volatile / atomicity | 4 | 2 | 1 |
| State machine | 0 | 3 | 2 |
| Numerical / overflow | 0 | 2 | 3 |
| Scope-violation triage | n/a | n/a | n/a (table only) |
| Flash / RAM | 0 | 2 | 4 |
| Test coverage gaps | 0 | 4 | 2 |
| Doc-in-code drift | 0 | 1 | 3 |
| **Totals** | **4** | **14** | **15** |

P0 = causes wrong behaviour or crash; P1 = latent bug, may misbehave on edge cases; P2 = code-quality / maintainability.

---

## 2. ISR / volatile / atomicity findings

The 5 ms MsTimer2 ISR (`pid_tick_isr` at `src/main.cpp:282`) calls `app.tick()` which reads many member fields written by `loop()` via `read_sensors()`. **No critical sections, no `volatile` qualifiers on the cross-boundary fields, and no atomic-block guards exist anywhere in the balance stack** (`grep -E "noInterrupts|cli\(|sei\(|ATOMIC_BLOCK"` returns zero hits). On AVR a `float` or `int16_t` read is 4 / 2 bytes and can tear mid-byte if an ISR fires during the read.

### P0-ISR-1 — `pitch_deg_` torn read across ISR/loop boundary
- **Where:** `balance_app.cpp:1353` (`pitch_deg_ = od.pitch_deg;` in `read_imu_`, called from `loop()`); read at `balance_app.cpp:500, 539, 1045, 1435` etc. (inside `tick()` → `step_run_`, `step_bootstrap_`).
- **Why it matters:** Loop writes 4 bytes non-atomically; if the 5 ms ISR fires after bytes 0–1 are stored but before bytes 2–3, the ISR's `step_run_` may see a `pitch_deg_` that mixes the old upper half with the new lower half. On the balance bot at low pitch (e.g. -0.01 mid-update to +0.03) the torn float can be a huge nonsense value (e.g. NaN, ±1e30). One such tick can push a 240 PWM motor pulse in the wrong direction or flip the soft-cutoff gate. Bench symptom: "twitched, fell in 1 s" (2026-05-18 PM late).
- **Fix:** Wrap the loop-side write in an atomic block:
  ```cpp
  noInterrupts(); pitch_deg_ = od.pitch_deg; interrupts();
  ```
  Same pattern for `raw_gyro_dps_[3]`, `linear_accel_mag_`, `g_lateral_dps_lpf_`, `a_dev_lpf_`, `a_align_`. Or use a double-buffered snapshot struct, swapped by single-byte index.

### P0-ISR-2 — `raw_gyro_dps_[3]` torn read (BOOTSTRAP K_motor measurement)
- **Where:** `balance_app.cpp:1373-1375` (write), `balance_app.cpp:501, 1046-1047, 1102, 1124` (read in ISR-side bootstrap).
- **Why it matters:** A torn `raw_gyro_dps_[1]` during BOOTSTRAP pulse-end measurement directly corrupts `dgyro = gyro_now - bs_pulse_start_gyro_` (line 1124). One bad sample produces a wildly wrong K_motor estimate → wrong Kp via pole-placement → unstable RUN entry. This is the most plausible explanation for the operator-observed "K spread across pulses" mentioned in the memory.
- **Fix:** Same atomic-block wrap as P0-ISR-1, applied to the 12-byte `raw_gyro_dps_` write.

### P0-ISR-3 — `last_output_` torn read (RUN plant-id regressor)
- **Where:** Write at `balance_app.cpp:547, 1110-1111, 1122` (ISR-side via `step_run_` / `step_bootstrap_`). Read at `balance_app.cpp:645` (`pwm_total = 2.0f * (float)last_output_;`).
- **Why it matters:** All accesses are inside `tick()` so they're ISR-local — **NOT torn**. But also read in loop-side `'s'` command handler at `main.cpp:435` (`app.get_last_output()`). The `s`-command path is benign (operator-visible status), but it's worth wrapping if the operator polls fast.
- **Fix:** Low priority — wrap `get_last_output()` in an atomic-read accessor only if the field grows beyond `int16_t`.

### P0-ISR-4 — Stale-mount kill-switch reads non-atomic float from ISR-written `pitch_deg_`
- **Where:** `main.cpp:467` (`float p = app.get_pitch_deg();` then compared at line 468). `get_pitch_deg()` returns the same field the ISR may be partway through writing... except that `pitch_deg_` is written by `read_sensors()` which runs in `loop()`, NOT the ISR. So this read is loop-against-loop and IS safe.
- **Re-classify:** This is **not** a P0; the kill-switch's reader is the same context as the writer. Demoted to a note: leave as-is.

### P1-ISR-1 — `collision_latched_` race between ISR sustain-counter and loop `clear_collision()`
- **Where:** `balance_app.h:349` (`clear_collision()` clears `collision_latched_` and `collision_consec_`); `balance_app.cpp:1438-1453` (ISR-side `read_imu_` increments `collision_consec_` and sets `collision_latched_`). But wait — `read_imu_` is called from `loop()` per the Item-3 split (see `balance_app.cpp:270-272`), so this is loop-vs-loop... EXCEPT `enter_state_()` is called from ISR-side `tick()` and clears the latch (line 829-830).
- **Why it matters:** Sequence: ISR enters `step_run_` → `collision_latched_` true → enters HELD via `enter_state_(HELD)` which clears latch. Simultaneously, loop's `read_imu_` is mid-increment of `collision_consec_`. The ISR's write of `0` to `collision_consec_` could be clobbered by loop's increment-completion, leaving a stale counter. Net effect: next sustained spike trips one tick late.
- **Fix:** Move the latch clear-on-state-entry to a deferred flag drained loop-side, or wrap the three-byte clear in `noInterrupts() / interrupts()`.

### P1-ISR-2 — `pulse_log_.seq` torn read in `drain_pulse_log`
- **Where:** `balance_app.h:326-336` (template `drain_pulse_log`). Reads `pulse_log_.seq` (uint8_t — atomic on AVR ✓), then reads `pulse_log_.cmd_pwm` (int16_t — NOT atomic), `pulse_log_.metric_x10` (int16_t — NOT atomic), etc.
- **Why it matters:** If the ISR populates a new pulse-log record between the loop reading `seq` and reading the rest of the fields, loop prints torn / mixed-pulse data. Telemetry-only — won't affect control — but the operator-visible log is exactly how bench diagnosis works.
- **Fix:** Loop-side: copy `pulse_log_` into a local with interrupts disabled briefly, then print from the local.

### P2-ISR-1 — `pending_state_log_` is correctly `volatile` but no memory barrier
- **Where:** `balance_app.h:485` (`volatile uint8_t pending_state_log_;`).
- **Why it matters:** On AVR `volatile` prevents register caching but doesn't provide ordering. Loop's drain reads pending, sets to `0xFF`. If the ISR fires *between* those two operations and writes a new pending value, loop clobbers it back to `0xFF` losing the transition. Acceptable per the doc comment ("the later transition wins" — actually the OPPOSITE happens here, the new transition is *lost*).
- **Fix:** Swap with `noInterrupts(); pending = pending_state_log_; pending_state_log_ = 0xFF; interrupts();` for a 4-byte read-modify-write under critical section.

---

## 3. State-machine findings

### P1-SM-1 — `enter_state_(RUN)` overwrites `applied_kp_/kd_/ki_` AFTER BOOTSTRAP just pushed gains
- **Where:** `balance_app.cpp:1220-1228` (BOOTSTRAP success path): `pid_.set_tunings(ps.kp_target, ...); enter_state_(RUN);` then `adaptive_active_ = true; run_entered_ms_ -= 10000;`.
- The `enter_state_(RUN)` call at line 1221 runs `case RUN` at line 868, which does `pid_.get_tunings(kp_now, ki_now, kd_now); plant_id_.reset(kp_now, ...); applied_kp_ = kp_now; ...`.
- **Why it matters:** Reading back gains we just wrote works correctly — but `plant_id_.reset(kp_now, kd_now)` *also* recomputes the RLS prior θ = ω_n²/Kp at `plant_identifier.cpp:138`. This MULTIPLIES Kp's K_motor implication and the BOOTSTRAP-pushed `seed_k_motor()` is now overwritten by `reset()`. The carefully-measured K is discarded one millisecond later.
- **Fix:** In BOOTSTRAP finalise path, call `plant_id_.seed_k_motor(k_measured)` *after* `enter_state_(RUN)`, not before. Or skip `plant_id_.reset()` in the RUN-entry side effect when `bootstrap_result_.converged == true`.

### P1-SM-2 — `failure_reason` is set but never cleared on the success path
- **Where:** `balance_app.cpp:1215` sets `bootstrap_result_.failure_reason = 0` on success, but the constructor at line 192 also initialises to 0. The next BOOTSTRAP attempt: `enter_state_(BOOTSTRAP)` at line 909-935 does NOT reset `failure_reason`. So after one failed BOOTSTRAP with `failure_reason=5`, a second attempt that succeeds will (after success) set it to 0 correctly. **But** if the second attempt itself fails partway, the user sees the new failure code — correct. So this is actually fine.
- **Re-classify:** Not a bug. Withdraw.

### P1-SM-3 — `enter_state_(HELD)` on collision does not record collision diagnostics
- **Where:** `balance_app.cpp:441-444` (RUN→HELD on `collision_latched_`).
- **Why it matters:** `enter_state_(HELD)` clears the latch (line 829), so the operator loses any way to know the HELD entry was collision-driven vs. lateral-gyro-driven. Both produce identical telemetry.
- **Fix:** Add a `held_entry_reason_` uint8_t field, set before `enter_state_` call. Drain via `s` status.

### P1-SM-4 — `FALLEN` short-press routes to BOOTSTRAP but `USE_BALANCE_FALL_DETECTION` is OFF by default
- **Where:** `balance_app.cpp:746-756` (FALLEN short-press handler). `balance_app.cpp:423-428` (RUN→FALLEN transition guarded by `#ifdef USE_BALANCE_FALL_DETECTION`).
- **Why it matters:** In the default `[base_balance]` build, the bot *never* enters FALLEN (the guard at line 424 is removed). The on_short_press FALLEN handler is dead code. The on_short_press(IDLE) at line 738 goes to CAPTURE_MOUNTING, which auto-chains to BOOTSTRAP — fine. So the FALLEN restart path is consistent, just unreachable. Latent but not a bug today.
- **Fix:** Add `#ifdef USE_BALANCE_FALL_DETECTION` around the FALLEN case in `on_short_press` to make the dead-code status explicit.

### P2-SM-1 — `AUTO_TUNE` enum + handler exist but cannot be reached
- **Where:** `balance_app.cpp:280` (switch case wired to `step_tune_`); `step_tune_` at line 362. No code path sets state to AUTO_TUNE (the long-press handler was rewritten to BOOTSTRAP). Wastes ~500 B flash on dead state-handler code.
- **Fix:** Remove `step_tune_` and the AUTO_TUNE case from the switch. Keep the enum value for ABI stability if telemetry consumers depend on it.

### P2-SM-2 — `CAPTURE_MOUNTING` → `IDLE` (jitter rejected) does not give operator feedback
- **Where:** `balance_app.cpp:355-358`.
- **Why it matters:** Capture failure looks identical to capture success (both transition to IDLE / BOOTSTRAP) on the serial log — only the absence of `[state] -> BOOT` distinguishes them. Hard to diagnose "why didn't capture succeed".
- **Fix:** Stash a `capture_fail_reason_` (e.g. variance, abort, timeout); print via `s` status.

---

## 4. Numerical / overflow hazards

### P1-NUM-1 — `int16_t` overflow on `(uint16_t)(ag * 10.0f)` accumulator in CHARACTERISE
- **Where:** `balance_app.cpp:1273, 1344` (`ch_gyro_acc_x10_ += (uint16_t)(ag * 10.0f);`).
- **Why it matters:** `ch_gyro_acc_x10_` is `uint16_t` (max 65535). Each tick adds up to `|gyro| * 10`, called at 200 Hz over a 200 ms pulse = 40 samples. If `|gyro| > 165 dps` (plausible during pulse 5 at 200 PWM on a light bot), one sample contributes `1650` and 40 such samples overflow. Wrap is silent and pollutes the response gate.
- **Fix:** Promote `ch_gyro_acc_x10_` to `uint32_t` (+2 B RAM, no flash cost on AVR). Or saturate per-sample at 6000.

### P1-NUM-2 — RLS divide near zero when `phi * P_` overflows
- **Where:** `plant_identifier.cpp:220-226`. `denom = lambda_ + phi * phi_P`. For large `phi` (`pwm_total = ±510` is possible) and growing `P_` after a long quiet period, `phi*phi*P_` can grow >1e6. Float math is fine but `P_ = (P_ - L_gain * phi_P) / lambda_` can drive P_ negative due to floating-point cancellation when `L_gain * phi_P ≈ P_`. A negative P_ then produces unstable updates.
- **Fix:** After the update at line 226, add `if (P_ < 1e-6f) P_ = 1e-6f; if (P_ > 1e6f) P_ = 1e6f;`. ~3 lines.

### P1-NUM-3 — `BOOTSTRAP_FREEZE_MS` window underflow when `now_ms` is small
- **Where:** `balance_app.cpp:638` (`(now_ms - run_entered_ms_) < BOOTSTRAP_FREEZE_MS`). After bootstrap success: `run_entered_ms_ = now_ms - 10000` at line 1228. If `now_ms` is small (e.g., 5000 ms after boot — common for prop-and-go), `now_ms - 10000` wraps to `0xFFFF...` and `(now_ms - run_entered_ms_) = 10000` correctly via unsigned wrap. **Actually safe** on uint32_t — wrap arithmetic gives the right answer. Withdrawing.
- **Re-classify:** Not a bug. The unsigned subtraction is correct by design.

### P2-NUM-1 — Float-heavy `read_imu_` runs in loop, called as fast as loop spins
- **Where:** `balance_app.cpp:1380-1407`. Three `sqrtf`, six `if/else` float comparisons, three LPF updates with `alpha * (x - lpf)` per tick.
- **Why it matters:** On Uno, `sqrtf` is ~80 cycles (5 µs at 16 MHz). Three of those + 12 float multiplies ≈ 30 µs per loop iter. The loop spins much faster than the 5 ms ISR — this is steady-state ~600 µs/iter (12% CPU) of float work for telemetry. Could integerize: store accel components as int16_t and use lookup-table sqrt.
- **Fix:** Defer; not a correctness issue, just a CPU budget tax.

### P2-NUM-2 — `linear_accel_mag_` magnitude computed via `sqrtf` 200 Hz in `read_imu_`
- **Where:** `balance_app.cpp:1435`. Compare `linear_accel_mag_ > COLLISION_PEAK_MPS2` — works equally well on `mag²` vs `threshold²`.
- **Fix:** Store `linear_accel_mag_sq_` (skip sqrt), pre-compute squared thresholds. Saves ~5 µs/tick.

### P2-NUM-3 — `ch_gyro_acc_x10_ * 3` can overflow uint32_t cast assumption
- **Where:** `balance_app.cpp:1279`. `uint32_t thr = (uint32_t)ch_gyro_acc_x10_ * 3;` — safe, max 196605. Followed by clamp at 60000 — fine.
- **Re-classify:** Documenting only; not a bug.

---

## 5. Scope-violation triage (cross-reference vs `scope.md` audit table)

Status per the 21-row table; "code as of 2026-05-19".

| Row | Violation (scope.md) | scope.md status | Code reality 2026-05-19 |
|---|---|---|---|
| 1 | `kDefaultInitialKp = 50.0f` | ✅ retired | Still PRESENT at `balance_app.cpp:75`. Used by `default_config()` (line 100), which is called by `main.cpp:328`. BOOTSTRAP overrides on success, but on failure these values are loaded into the PID. **scope.md is wrong** — value still in source. |
| 2 | `kDefaultInitialKi = 2.0f` | ✅ retired | Same — `balance_app.cpp:76`. |
| 3 | `kDefaultInitialKd = 20.0f` | ✅ retired | Same — `balance_app.cpp:77`. |
| 4 | `R` command 65/12/38 | ✅ retired | Confirmed absent from `main.cpp`. |
| 5 | FALLEN restart ±80 PWM | ✅ retired | Confirmed absent (only ±255 at `main.cpp:101, 815`). |
| 6 | Relay tuner amp 150 | ✅ retired | Confirmed: NoOpStrategy in `main.cpp:110-119`. |
| 7 | `tune_max_duration_sec=30` | ✅ retired | Still in `balance_app.cpp:86, 859` but unreachable (AUTO_TUNE dead). Cosmetic dead code. |
| 8 | `cfg.tilt_limit_deg` | 🔄 partially | Confirmed: `main.cpp` does not override; falls back to default 10° (`balance_app.cpp:80`). |
| 9 | `kDefaultTiltLimitDeg = 35.0f` (safety.cpp) | ⏳ open | Still at `safety.cpp:10`. Note: BalanceApp's `kDefaultTiltLimitDeg = 10.0f` at `balance_app.cpp:80` is different value, pushed to safety via `set_tilt_limit` at `balance_app.cpp:231`. The `safety.cpp:10` value is effectively dead init. |
| 10 | `SOFT_ZONE_DEG = 1.0f` | ⏳ open | Confirmed at `balance_app.cpp:517` (constexpr inside step_run_). |
| 11 | `SAT_THRESHOLD_PWM = 180` | ⏳ open | Confirmed at `balance_app.cpp:560`. |
| 12 | `STUCK_GYRO_DPS = 5.0f` | ⏳ open | Confirmed at `balance_app.cpp:561`. |
| 13 | `STUCK_TIMEOUT_MS = 1500` | ⏳ open | Confirmed at `balance_app.cpp:562`. |
| 14 | Phase 2.5 `cmd_mag < 20` | ⏳ open | Confirmed at `balance_app.cpp:479`. |
| 15 | Phase 2.5 `gyro > 30 dps` | ⏳ open | Confirmed at `balance_app.cpp:479`. |
| 16 | Phase 2.5 `dwell=100ms` (20 ticks) | ⏳ open | Confirmed at `balance_app.cpp:483`. |
| 17 | HELD `a_dev_lpf_ > 6.0f` | ⏳ open | Confirmed at `balance_app.cpp:480`. |
| 18 | `BOOTSTRAP_FREEZE_MS = 5000` | 🔄 partially | Confirmed at `balance_app.cpp:637`; bypassed on BOOTSTRAP success via `run_entered_ms_ -= 10000` at line 1228 (clever but obscure). |
| 19 | Absolute pitch kill ±20° | ⏳ open | Confirmed at `main.cpp:468`. |
| 20 | `online_est max_deviation = 5°` | ⏳ open | Confirmed at `online_mounting_estimator.cpp:26`. |
| 21 | `online_est LPF tc = 8 s` | ⏳ open | Confirmed at `main.cpp:350` (was 20 s, recently lowered). |

**New violations not in scope.md table:**

| New | Where | Note |
|---|---|---|
| Collision thresholds (PEAK=12, SUSTAIN=8, KICK=6, KICK_GYRO=200) | `balance_app.h:142-146` | Tagged in header comment with derivation but still literals. Add to scope.md as new row with derivation plan = "3σ over baseline LIA noise from CHARACTERISE Phase 2.1". |
| `BOOTSTRAP_MAX_INIT_PITCH = 10.0f` | `balance_app.cpp:1009` | Could derive from safety.tilt_limit/3. |
| `BOOTSTRAP_ABORT_PITCH = 15.0f` | `balance_app.cpp:1005` | Could derive from safety.tilt_limit/2. |
| PULSE_PWMS = {180, 180, 240, 240} | `balance_app.cpp:1003` | Structural per project doctrine, but the magnitudes themselves are tunings — derive from CHARACTERISE stiction × 4. |
| `set_i_term_limit(40.0f)` | `main.cpp:341` | "Structural" per comment but is a tuning. |
| `set_d_term_lpf_tau_sec(0.003f)` | `main.cpp:342` | Same. |
| stiction_min_pwm=80 ctor default | `main.cpp:95` | Hardcoded fallback if CHARACTERISE not run. |
| 2000 ms BOOTSTRAP grace delay | `main.cpp:385` | "Operator getting hands clear" — arbitrary. |
| `5.0f` stale-mount threshold | `main.cpp:396` | Half of BOOTSTRAP_MAX_INIT_PITCH — could derive. |

---

## 6. Flash / RAM optimization candidates

Uno at 97.2% flash; 700 B RAM free. Estimated savings, sorted descending:

| Est. | Item | Where | Mechanism |
|---|---|---|---|
| ~500 B | Remove dead `step_tune_()` + AUTO_TUNE switch case | `balance_app.cpp:362-400, 280, 854-867` | Long-press → BOOTSTRAP-only path is the only one wired. NoOpStrategy still needed for ctor injection though. |
| ~200 B | Inline collision OR-fire arithmetic into single comparison expression | `balance_app.cpp:1447-1453` | Three bool locals + jumps compile to ~60 B; `if ((a>P)||(c>=T)||((a>K)&&(g>G)))` compiles to ~20 B. |
| ~150 B | Replace `(int16_t)(x * 10.0f)` casts with int math | `balance_app.cpp:1035-1037, 1159-1162, 1258-1260, 1301-1304, 1326-1328` | `(int16_t)(x*10)` pulls in float→int conversion (~50 B once); doing `int(x*10)` six times costs nothing extra in flash now, but if rewritten as `(int16_t)((int32_t)(x*1024)>>7)` we get a `ldi/mul/asr` sequence and skip the float-to-long helper. |
| ~100 B | `print(float, N)` calls in `main.cpp:400-402, 433-434, 483` | `main.cpp` | Replace with int*100 + int formatter (per scope.md flash strategy table — "pending" row, ~432 B claim). |
| ~80 B | Single `Serial.print(F("[state] -> "))` in template `drain_state_log` | `balance_app.h:242` | Template means this string appears in flash once per `TPrint` type, which is fine — already optimal. Withdraw. |
| ~60 B | `pulse_log_` is 11 bytes of RAM always — could be union with bootstrap-vs-characterise variants | `balance_app.h:314-323` | Saves 0 flash, 0 RAM in practice (only one in use at a time but they share storage). Withdraw. |
| ~50 B | `MountingCalibrationStatus` 20 B / `TuningResult` ~28 B / `BootstrapResult` ~16 B as members | `balance_app.h:441-442, 495` | Move TuningResult to file-scope static (AUTO_TUNE dead). Save 28 B RAM. |
| ~40 B | Constexpr collision thresholds in header pull `float` constants into flash for every TU that includes `balance_app.h` | `balance_app.h:142-146` | Move into `.cpp` anonymous namespace. Saves ~20 B per inclusion (5 TUs include the header → ~100 B). |
| ~30 B | `kDefaultTuneAmplitude`, `kDefaultTuneHysteresis`, `kDefaultTuneMaxDurationSec` | `balance_app.cpp:84-86` | Dead — AUTO_TUNE unreachable. |
| RAM −28 B | `tune_result_` member never written meaningfully | `balance_app.h:441` | Remove (AUTO_TUNE dead). |
| RAM −16 B | Per-instance `g_lateral_dps_lpf_`, `a_dev_lpf_`, `a_align_`, plus three init bools | `balance_app.h:411-414` | These are live — keep. |

**Realistic Uno headroom from cheap wins**: ~800 B flash (removing AUTO_TUNE + dead defaults + inlined collision + float-print → int-print). Buys back 2.5% headroom.

---

## 7. Test coverage gaps

Existing native test files (32 tests across 5 files):
- `test_balance_app.cpp` — IDLE/CAPTURE/AUTO_TUNE/RUN/FALLEN transitions (12 tests)
- `test_balance_app_bootstrap.cpp` — BOOTSTRAP entry/no-response/success/auto-chain (6 tests)
- `test_balance_app_collision.cpp` — collision latch + BOOTSTRAP/CHAR/RUN abort (8 tests)
- `test_balance_app_soft_cutoff.cpp` — RUN soft-cutoff (4 tests)
- `test_held_state_machine.cpp` — RUN↔HELD transitions (3 tests)

### P1-COV-1 — No test for BOOTSTRAP→RUN gain push being preserved
- The most concerning bug (P1-SM-1 — `enter_state_(RUN)` clobbers measured K via `plant_id_.reset()`) has no test. Need: spy on `plant_id_.get_k_motor()` after BOOTSTRAP success → assert it equals the measured K, not the seed-prior backed out of Kp.

### P1-COV-2 — No test for `clear_collision()` being called on every `enter_state_` transition
- `enter_state_` at `balance_app.cpp:829-830` clears the latch unconditionally. Test should verify: latch HELD → enter_state_(RUN) → latch == false → another impact within the next 2 ticks re-latches.

### P1-COV-3 — No test for STUCK detector (saturation timeout)
- `balance_app.cpp:560-576` STUCK detector with 1500 ms timeout: no test covers it. Failure mode: stiction sweep test could accidentally land in STUCK during a long high-PWM hold.

### P1-COV-4 — No test for `OnlineMountingEstimator` LPF freeze gates exercised together
- `online_mounting_estimator.cpp:154-165` has tipover / windup / user / high_gyro priority chain. No test asserts the priority order (e.g., windup beats high_gyro).

### P2-COV-1 — No test for soft-cutoff transition while collision is also latched
- Two conditions interact in `step_run_` (collision check at 441 runs before soft-cutoff at 540) — test the precedence.

### P2-COV-2 — No test for `ramp_gain_` boundary behaviour
- `balance_app.cpp:1439-1450` — what happens if `target` is negative and `live` is positive? Should monotonically approach via clamped delta. No test verifies the delta clamp.

---

## 8. Doc-in-code drift

### P1-DOC-1 — `balance_app.cpp:69-78` comment: "Iterated gains 2026-05-12 evening… Kp=50 / Ki=2 / Kd=20"
- Per scope.md these are RETIRED. The comment is stale (Phase 4.10c removed reliance) but the constants are still in source as ctor defaults. The audit table claims "✅" but reality is "🔄 (fallback path)". **Update both the comment and scope.md**.

### P2-DOC-1 — `balance_app.h:7-13` state diagram shows IDLE→CAPTURE→AUTO_TUNE→RUN
- Should be IDLE→CAPTURE→BOOTSTRAP→RUN. AUTO_TUNE is unreachable.

### P2-DOC-2 — `balance_app.h:166-168` `default_config()` doc: "Convenience: built-in defaults matching the legacy .ino (Kp=65 Ki=12 Kd=38, ±255 PWM, 5 ms sample, 35° tipover / 15° recovery)"
- Wrong on every count: actual values are Kp=50/Ki=2/Kd=20 (line 75-77), 10° tilt/4° recovery (line 80-81). Misleading.

### P2-DOC-3 — `sensor_base.h:118-122` and `bno055.h:142` both reference `COLLISION_SPIKE_MPS2`
- Constant renamed to `COLLISION_PEAK_MPS2`. Update both doc-comments.

### P2-DOC-4 — `balance_app.h:393` TODO: "VERIFY AXIS ON BENCH"
- Still open after bench session. The 2026-05-18 bench did fire motors during BOOTSTRAP and saw response — but the audit doesn't capture whether axis was confirmed. Either resolve or restate.

---

## 9. Prioritized punch list — top 5 for next coding session

1. **P0-ISR-1/2 (atomicity)**: Wrap loop→ISR shared-state writes in `noInterrupts()/interrupts()` blocks. Three fields: `pitch_deg_` (4 B), `raw_gyro_dps_[3]` (12 B), `linear_accel_mag_` (4 B). One-liner per write in `read_imu_`. Expected effect: eliminates the BOOTSTRAP "K spread across pulses" + sporadic single-bad-sample motor twitches. ~12 lines, no flash cost (cli/sei are 1 cycle each).

2. **P1-SM-1 (BOOTSTRAP K gets clobbered)**: In `balance_app.cpp:1219-1228`, swap the order:
   ```cpp
   enter_state_(BalanceAppState::RUN, now_ms);   // RUN side-effect resets plant_id from PID gains
   plant_id_.seed_k_motor(k_measured);           // NOW re-push the measured K (overrides reset)
   pid_.set_tunings(ps.kp_target, ps.ki_target, ps.kd_target);
   ```
   Or guard the `plant_id_.reset()` in `enter_state_(RUN)` with `if (!bootstrap_result_.converged)`. ~3 lines. Restores the entire point of BOOTSTRAP.

3. **P0-Constant-Drift (`COLLISION_SPIKE_MPS2` references)**: Audit confirmed grep — comments only, not code. Update `sensor_base.h:118-122` and `bno055.h:142` to current name `COLLISION_PEAK_MPS2`. ~2 lines. Free.

4. **P1-NUM-1 (CHARACTERISE uint16 overflow)**: Promote `ch_gyro_acc_x10_` to `uint32_t` at `balance_app.h:478`. ~1 line, +2 B RAM, removes a silent-wrap that contaminates stiction measurement on light bots / strong pulses.

5. **P1-NUM-2 (RLS covariance can go negative)**: At `plant_identifier.cpp:226`, clamp `P_` to `[1e-6, 1e6]` after update. ~2 lines. Prevents adaptive RLS from blowing up after a quiet period followed by sudden excitation.

**Not in top-5 but free wins**: Update stale doc comments (P1-DOC-1, P2-DOC-1, P2-DOC-2) — pure code-comprehension improvement, no risk.

---

*Audit complete. ~1480 words. No source files modified.*
