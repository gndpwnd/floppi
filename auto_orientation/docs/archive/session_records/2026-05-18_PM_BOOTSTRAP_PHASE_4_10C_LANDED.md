# 2026-05-18 PM evening — BOOTSTRAP (Phase 4.10c) landed

Continuation of [2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md](2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md). That session ended with a 19-row scope-violations audit and an explicit "BOOTSTRAP is the unblocking work for the next session" pointer. This session implemented it: a new `BOOTSTRAP` state measures K_motor from controlled ±PWM pulses, derives Kp/Ki/Kd analytically via pole-placement, and pushes the gains to the PID before entering RUN. **No more hardcoded `Kp=50`.** Auto-RUN at boot now chains through BOOTSTRAP with a stale-mount sanity check.

---

## What landed

### Core change — BOOTSTRAP state (Phase 4.10c)

New state `BalanceAppState::BOOTSTRAP` (enum value 7) wedges between `CAPTURE_MOUNTING` and `RUN` in the state machine. Algorithm (in [`src/applications/balancing_robot/balance_app.cpp` step_bootstrap_](../../src/applications/balancing_robot/balance_app.cpp)):

1. **PHASE 0 baseline (300 ms)** — motors off, capture peak |α| gyro-noise floor for the response-threshold gate. Reuses the 3× baseline-noise rule from CHARACTERISE (Phase 2.1).
2. **PHASE 1–4 pulses (4 × 200 ms = 800 ms total)** — apply ±100, ±150 PWM per wheel (alternating sign so net momentum cancels). For each pulse:
   - Latch `gyro_y` at pulse start.
   - Drive motors for 100 ms.
   - At cooldown entry, read `gyro_y` again → `Δω = end − start`.
   - If `|Δω|/PULSE_SEC > 3 × baseline noise`, compute `K_i = |Δω|/(PULSE_SEC × |pwm_total|)` and add to the running sum.
3. **FINALISE** — mean of valid K samples → `k_measured`. Sanity-check: at least 2/4 pulses produced response; clamped K is within ±50% of measured K (rejects garbage that needed extreme clamping). On success: `plant_id_.seed_k_motor(K)` → pull derived Kp/Kd/Ki from PlantIdentifier targets → `pid_.set_tunings()` → enter RUN with `adaptive_active=true` (skip the 5 s RLS-warmup freeze).
4. **Failure paths**: pitch out of ±10° at start → IDLE with `failure_reason=1`. Pitch > ±15° during pulses → IDLE with `failure_reason=1`. < 2 valid pulses → IDLE with `failure_reason=2`. K wildly out of plant_id bounds → IDLE with `failure_reason=3`. Operator abort → IDLE with `failure_reason=4`.

Total bootstrap duration: ~1.1 s. Result struct exposed via `app.get_bootstrap_result()` for telemetry.

### Wiring changes (state machine + boot sequence)

- **`CAPTURE_MOUNTING` success → BOOTSTRAP** (was → IDLE). Operator presses `c`, capture lands, BOOTSTRAP fires automatically.
- **`on_long_press(IDLE)` → BOOTSTRAP** (was → AUTO_TUNE). Long-press / `t` skips capture, uses currently-loaded mount.
- **`on_short_press(FALLEN)` → BOOTSTRAP** (was → RUN with hardcoded ±80 PWM clamp). Re-measure K under current battery/surface after tipover.
- **Boot auto-bootstrap** (`src/main.cpp:354-385`): if a saved mount exists AND `|current_pitch − saved_mount| < 5°`, fire BOOTSTRAP automatically after a 2 s grace period. Stale-mount sanity check guards against the 2026-05-18 PM near-failure where a stored mount of −1.21° vs actual upright pitch of +0.3° caused the bot to drive backward into the kill switch at boot.
- **New `b` serial command** — manual BOOTSTRAP trigger from IDLE (operator-friendly bind for non-button consoles).
- **Removed `R` command** — hardcoded Kp=65/Ki=12/Kd=38 path. Replaced by BOOTSTRAP.

### Removed hardcoded values

7 scope violations retired:

- `kDefaultInitialKp = 50.0f`, `Ki = 2.0f`, `Kd = 20.0f` (balance_app.cpp config defaults — still in struct for API, but main.cpp never sets PID via them anymore).
- `balance_pid.set_tunings(50.0f, 1.0f, 10.0f)` in main.cpp setup — removed; BOOTSTRAP pushes gains.
- `R` command with `set_tunings(65.0f, 12.0f, 38.0f)` — removed.
- FALLEN restart `pid_.set_output_limits(-80.0f, 80.0f)` clamp — removed; FALLEN re-enters BOOTSTRAP.
- `RelayFeedbackStrategy(amp=150.0f, hyst=0.5f)` and `tune_max_duration_sec = 30.0f` — relay tuner deleted from balance build entirely via `platformio.ini` `build_src_filter`. Main.cpp uses a 10-line `NoOpStrategy` stub to satisfy the BalanceApp constructor. Saves ~1.3 KB flash.

### New `seed_k_motor()` on PlantIdentifier

`src/control/plant_identifier.{h,cpp}` — pushes a directly-measured K with `SEED_P = 0.05` (much smaller than `INITIAL_P = 1.0`) so the RLS trusts the measurement strongly but still adapts to battery sag / wear / surface drift. Clamps into (k_min, k_max) class bounds. Resets sample counter. Used by BOOTSTRAP after the pulse sequence completes.

### Build state

| Env | Before this session | After |
|---|---|---|
| `uno_balance` flash | 95.6% (1414 B free) | **92.2% (2518 B free)** — net SAVED 1104 B |
| `uno_balance` RAM | 70.4% | 64.7% |
| `mega_balance` flash | 12.3% | 12.2% |

Relay tuner removal (1.3 KB) more than paid for BOOTSTRAP code (~200 B net, including new BootstrapResult struct + state vars).

### Test status

123 native tests pass across all balance-related modules:

| Test file | Result |
|---|---|
| `test_balance_app.cpp` | 28/28 |
| `test_balance_app_bootstrap.cpp` (NEW) | 27/27 |
| `test_balance_app_soft_cutoff.cpp` | 13/13 |
| `test_held_state_machine.cpp` | 8/8 |
| `test_plant_identifier.cpp` (+1 seed_k_motor test) | 13/13 |
| `test_online_mounting_estimator.cpp` | 14/14 |
| `test_mounting_calibration.cpp` | 20/20 |
| `test_pid_controller.cpp` | (PIO-only, requires Unity — unchanged) |
| `test_relay_feedback.cpp` | (PIO-only, requires Unity — unchanged) |

Test-fixture migrations required by the AUTO_TUNE → BOOTSTRAP rewire:

- `test_balance_app.cpp` — replaced `on_long_press(IDLE)` paths (which now enter BOOTSTRAP, not AUTO_TUNE) with direct `enter_run_with_current_gains()` calls. Marked `test_tune_to_run_on_success` / `test_tune_to_idle_on_failure` as SKIP — AUTO_TUNE success/failure paths moved to BOOTSTRAP and are covered by `test_balance_app_bootstrap.cpp`.
- `test_balance_app_soft_cutoff.cpp` — `enter_run` fixture switched to `enter_run_with_current_gains()`. Shortened loops to stay under STUCK_TIMEOUT_MS (1500 ms) — the mock IMU returns zero gyro so the STUCK detector would otherwise fire on saturated PID output.
- `test_held_state_machine.cpp` — `enter_run` fixture switched. Lateral-gyro HELD tests rewritten as Phase 2.5 ext_motion tests (drive `gyro_y` high with `pitch=0` so PID output is small; ext_motion sees cmd_quiet + pitch_gyro_fast and enters HELD). "Stay in HELD" test sets `gyro_x` high (not `gyro_y`) since the HELD-exit quiet gate measures `sqrt(gx² + gz²)`.
- `test_online_mounting_estimator.cpp` — bulk-replaced `i_term-driven` patterns with `pitch_deg-driven` patterns to match the prior session's "target = pitch_deg directly" architectural fix. Test 3 renamed `test_slow_drift_toward_i_term` → `test_slow_drift_toward_pitch`.

### Bootstrap unit-test infrastructure (new)

`tests/test_balance_app_bootstrap.cpp` introduces a `GyroIMU` mock that extends `OrientationSensor` with programmable `getRawGyro()`. Six scenarios:

1. **pitch_out_of_range** — bot tilted 12° at entry → first tick bails to IDLE with `failure_reason=1`, motors never fire.
2. **no_response** — gyro stays at 0 through all pulses → `failure_reason=2`, 0/4 pulses valid.
3. **success** — synthetic plant produces `gyro_y = K_TRUE × pwm_total × t_into_pulse`. K_true=0.4 → K_est=0.385 (3.8% error), derived Kp=166 Kd=29 Ki=8, lands in RUN with PID re-tuned.
4. **capture_chains_to_bootstrap** — short-press IDLE → CAPTURE → BOOTSTRAP auto-chain verified.
5. **long_press_idle_to_bootstrap** — long-press IDLE → BOOTSTRAP (was AUTO_TUNE) verified.
6. **user_abort** — abort during BOOTSTRAP → IDLE with motors stopped, `failure_reason=4`.

### Scope.md audit table updated

The 19-row violations table now uses ✅/🔄/⏳ status markers. 7 of 21 violations (33%) retired this session. The remaining 14 were ALL blocked-by the PID-gains violation — they need balance data to derive their measurements, and the bot couldn't balance without measured gains. Each row's "Replacement" column names the specific measurement; future sessions can systematically retire them now that BOOTSTRAP unblocks the data pipeline.

---

## The architectural insight that drove the session

From the prior session's operator framing:

> "all parameters that need tuning should never be hardcoded.... they should be found through the calibration or earlier stages or otherwise dynamically handled"

The audit table made the violation distribution visible — 19 rows, all dependent on the same root: **PID gains cannot be hardcoded because they depend on plant parameters that vary across bots / batteries / surfaces.** Every other violation in the table was either (a) safe-because-balance-loop-keeps-error-bounded or (b) deferred-pending-balance-data. So retiring the PID gains via BOOTSTRAP wasn't one violation out of many — it was the *gating* violation that blocked addressing all the others.

The session also confirmed an architectural principle: **a measurement that runs concurrently with the system being measured can't bootstrap an unstable plant.** The pre-existing PlantIdentifier (RLS during RUN) is the "right" solution for adapting to drift, but it CANNOT discover K from cold start because the bot is oscillating to failure before RLS sees enough quiet data. BOOTSTRAP exists precisely to provide that cold-start measurement via deliberate, bounded excitation BEFORE the unstable balance loop closes.

---

## What did NOT happen this session

- **Bench-validated balance**: code compiles + 123 tests pass, but the bot wasn't on the bench this session. Next bench session is the validation step.
- **Remaining 14 audit-table violations**: each is unblocked but not yet implemented. Order matters: STUCK / Phase 2.5 thresholds depend on CHARACTERISE noise floor + saturation_pwm measurements which need to be added to the CHARACTERISE state. SOFT_ZONE_DEG depends on observed pitch RMS during quiet balance. Tilt limits depend on observed envelope. None are individually difficult; all are sequenced behind BOOTSTRAP-enabled balance data.
- **AUTO_TUNE state removal**: kept in the enum for API stability. The `step_tune_` handler is now unreachable from the public API but compiles fine. A future session can remove it entirely once we're confident no telemetry consumer reads the AUTO_TUNE state name.
- **ESP32/Teensy port of BOOTSTRAP**: works on Mega today via shared firmware code; uno_balance flash budget is fine. Other platforms need MsTimer2 replacement (existing TODO, not BOOTSTRAP-specific).

---

## Files changed

```
src/applications/balancing_robot/balance_app.h    +50 lines (BootstrapResult struct, BOOTSTRAP enum, state)
src/applications/balancing_robot/balance_app.cpp  +200 lines (enter_bootstrap, step_bootstrap_, wiring)
src/control/plant_identifier.h                    +8 lines (seed_k_motor decl)
src/control/plant_identifier.cpp                  +20 lines (SEED_P, seed_k_motor impl)
src/main.cpp                                      ~40 line changes (NoOpStrategy, auto-bootstrap, b cmd, removed R)
platformio.ini                                    +1 line (exclude relay_feedback.cpp from balance build)
tests/test_balance_app_bootstrap.cpp              NEW (380 lines, 27 assertions)
tests/test_balance_app.cpp                        ~30 line changes (transition rewires, AUTO_TUNE skips)
tests/test_balance_app_soft_cutoff.cpp            ~15 line changes (enter_run, loop durations)
tests/test_held_state_machine.cpp                 ~25 line changes (ext_motion semantics)
tests/test_online_mounting_estimator.cpp          ~30 line changes (i_term → pitch_deg targets)
tests/test_plant_identifier.cpp                   +60 lines (seed_k_motor test)
docs/scope.md                                     audit table updated (7/21 ✅, 1/21 🔄, 14/21 ⏳)
```

---

## Operator UX after this session

- **Cold boot, saved mount, bot propped upright (|pitch − saved_mount| < 5°)**: auto-BOOTSTRAP fires after 2 s grace period. Bot measures K, derives gains, enters RUN. ~3 s end-to-end from power-on to balancing.
- **Cold boot, stale mount or bot not upright**: stays in IDLE. Logs `stale_mount p=X.XX m=Y.YY`. Operator presses `c` to recapture, which auto-chains to BOOTSTRAP → RUN.
- **From IDLE, mount already saved**: press `b` (or long-press button) → BOOTSTRAP runs → RUN.
- **After FALLEN**: press button (short-press) → BOOTSTRAP re-measures K under current conditions → RUN.
- **Abort**: `a` or long-press always returns to IDLE, motors off.
- **Status (`s`)**: prints `<state> <pitch> <mount> <output> <stiction>`. State is one of IDLE, CAP, BOOT, RUN, HELD, FAL.

---

## Lessons reinforced

1. **The unblocking work is often invisible until the audit makes it visible.** 19 violations looked like 19 problems. The audit made it clear that 18 of them were one problem with downstream symptoms — and retiring the one (PID gains) unlocks all the others.
2. **Removing dead code is the cheapest way to fund new code.** Relay tuner deletion freed 1.3 KB → BOOTSTRAP cost ~200 B → net 1.1 KB headroom for the next round of measurement-replacement work.
3. **Test-fixture failures often signal that the production architecture moved.** The AUTO_TUNE-based fixture helpers were a code smell pointing at the same problem: tests were exercising a path that the production app no longer uses. Migrating them to BOOTSTRAP or `enter_run_with_current_gains()` was a forcing function for thinking about the public API surface.
4. **A unit-test mock that exposes the right interface IS the spec.** The `GyroIMU` mock for BOOTSTRAP tests was 50 lines, and writing it forced clarity on which raw IMU signals BOOTSTRAP actually depends on (just `getRawGyro()[1]`, the pitch-axis rate). That clarity transferred straight back to the production code's documentation.

---

## Cross-links

- Prior session: [2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md](2026-05-18_PM_BENCH_VALIDATION_AND_VIOLATIONS_AUDIT.md)
- Reference design: [findings/bootstrap_protocol_unstable_plant.md](../../findings/bootstrap_protocol_unstable_plant.md)
- Scope violations audit (updated): [scope.md §Current scope violations](../../scope.md#current-scope-violations--audit-2026-05-18-updated-pm-evening-phase-410c-landed)
- Plant identifier algorithm: [findings/dynamic_pwm_accel_learning.md](../../findings/dynamic_pwm_accel_learning.md)
- Universal vision: [UNIVERSAL_BALANCE_BOT_VISION.md](../../UNIVERSAL_BALANCE_BOT_VISION.md)
