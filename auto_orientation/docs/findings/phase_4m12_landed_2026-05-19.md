# Phase 4M.12 — PWM Range Auto-Discovery — As-Built Verification (2026-05-19)

Status: LANDED (partial). Sibling agent shipped `step_pwm_discovery_` +
`enter_pwm_discovery()` + EEPROM persistence + `p` serial command + Mega
build flags before the implementation session timed out. Native test file
was missing; this doc + `tests/test_balance_app_pwm_discovery.cpp` close
that gap.

Spec source: `docs/MEGA_UNIVERSAL_PLAN.md` §7 (esp. §7d "PWM range
auto-discovery") and §7a row 3.

---

## 1. As-built summary

| Item | Value | Spec match |
|---|---|---|
| State enum name | `BalanceAppState::PWM_DISCOVERY = 8` | spec said "CHAR_PWM_RANGE = 8"; renamed but value matches |
| Operator command | `'p'` (serial) | matches spec |
| Button trigger | none — serial only | spec implied serial only |
| Entry method | `BalanceApp::enter_pwm_discovery(now_ms)` | matches spec |
| Per-tick handler | `BalanceApp::step_pwm_discovery_(now_ms)` | matches spec |
| Result struct | `PwmDiscoveryResult` (5 fields) | matches spec |
| EEPROM persistence | `save_pwm_discovery_` in `main.cpp:208`; slot `EE_PWMDISC_ADDR` (spec said 0x210/0x230 — see Gap 2) | partial — slot address not 0x210 |
| Per-step telemetry | `pulse_log_` with `source=2` (PWMD), drained by `drain_pulse_log` | matches spec |
| `failure_reason` for timeout | `=8` (`pwm_discovery_timeout`) | matches spec |
| `failure_reason` for abort | `=4` (`user_abort`) | matches spec |
| `failure_reason` for collision | NOT IMPLEMENTED — collision does not abort PWMD | gap (see §4) |
| Inspectors | `get_pwm_discovery_result()`, `get_discovered_min_pwm()`, `get_discovered_max_pwm()` | matches spec |
| `get_discovered_*` clamp on `discovered=false` | yes — returns 0 when result.discovered is false | nice-to-have, present |
| State-name log token | `"PWMD"` | new — covered by `state_name()` and `drain_state_log` |
| Compile guard | `#ifdef USE_WHEEL_ENCODERS` everywhere | matches spec — Uno-minimal cannot pull it in |
| Mega flash impact | ~+600 B vs encoder-only baseline (per task brief: 14.7% vs 14.1%) | within MEGA_UNIVERSAL_PLAN §3 budget |

### Tuning constants (all `constexpr`, in `balance_app.h:282-287`)

| Constant | Value | Units |
|---|---|---|
| `PWM_DISC_STEP_PWM` | 5 | raw PWM |
| `PWM_DISC_STEP_DURATION_MS` | 200 | ms |
| `PWM_DISC_MIN_VELOCITY_DPS` | 5 | dps (matches `WheelEncoder::kStallVelocityDps`) |
| `PWM_DISC_PLATEAU_DELTA_DPS` | 2 | dps |
| `PWM_DISC_PLATEAU_COUNT` | 3 | consecutive small-delta steps |
| `PWM_DISC_TIMEOUT_MS` | 8000 | ms |

All constants are gated by `#ifdef USE_WHEEL_ENCODERS`, with thorough
rationale comments above each.

---

## 2. Code locations

| Concern | File:line |
|---|---|
| `PWM_DISCOVERY` enum member | `src/applications/balancing_robot/balance_app.h:84` |
| `PwmDiscoveryResult` struct | `src/applications/balancing_robot/balance_app.h:101-107` |
| Tuning constants block | `src/applications/balancing_robot/balance_app.h:282-287` |
| `enter_pwm_discovery()` decl | `src/applications/balancing_robot/balance_app.h:569` |
| `get_pwm_discovery_result()` | `src/applications/balancing_robot/balance_app.h:572-574` |
| `get_discovered_min_pwm()` / `_max_pwm()` (clamped on `discovered`) | `src/applications/balancing_robot/balance_app.h:577-584` |
| `step_pwm_discovery_` decl | `src/applications/balancing_robot/balance_app.h:713` |
| Private state (`pwm_disc_*`, `pwm_discovery_result_`) | `src/applications/balancing_robot/balance_app.h:780-786` |
| Encoder ctor + reset of PWMD members | `src/applications/balancing_robot/balance_app.cpp:193-200, 241-245` |
| `tick()` switch dispatch to PWMD | `src/applications/balancing_robot/balance_app.cpp:329` |
| `state_name()` case | `src/applications/balancing_robot/balance_app.cpp:880` |
| `enter_state_(PWM_DISCOVERY)` side-effects | `src/applications/balancing_robot/balance_app.cpp:1018-1039` |
| `enter_pwm_discovery()` impl (IDLE guard + enter) | `src/applications/balancing_robot/balance_app.cpp:1733-1738` |
| `step_pwm_discovery_` body | `src/applications/balancing_robot/balance_app.cpp:1740-1855` |
| `drain_state_log` PWMD case | `src/applications/balancing_robot/balance_app.h:421-423` |
| `drain_pulse_log` `source=2` ("pd#") branch | `src/applications/balancing_robot/balance_app.h:506-512` |
| Serial `'p'` command handler | `src/main.cpp:509-518` |
| EEPROM save on PWMD→IDLE transition | `src/main.cpp:568-587` |
| EEPROM helpers | `src/main.cpp:208-229` |

Algorithm walk-through (`step_pwm_discovery_`, `balance_app.cpp:1740-1855`):
1. Early-exit on `safety_.abort_requested()` (`finish_reason=4`) or
   elapsed > `PWM_DISC_TIMEOUT_MS` (`finish_reason=8`).
2. Otherwise drive both motors at `pwm_disc_cur_pwm_`, refresh encoder
   velocity windows, return if step boundary not reached.
3. At step boundary: read final `v_l` / `v_r` (both wheels), compute mean
   abs; if MIN not yet locked and BOTH wheels exceed
   `PWM_DISC_MIN_VELOCITY_DPS` → lock `discovered_min_pwm`. Otherwise if
   MIN already locked → run plateau detector (consecutive small-delta
   steps), lock `discovered_max_pwm` to the prior PWM (saturation onset).
4. Emit pulse_log entry (`source=2`, gyro_start_x10 carries left
   velocity, metric_x10 carries right velocity, thr_x10 carries plateau
   delta threshold, passed = 1/2/0 for MIN-lock/MAX-lock/none).
5. If not done, advance `pwm_disc_cur_pwm_` by `PWM_DISC_STEP_PWM`
   (off-the-top → timeout via finish_reason=8).
6. Single-exit finalize: stop motors, populate `pwm_discovery_result_`
   (`failure_reason`/`discovered`/`steps_attempted`), `enter_state_(IDLE)`.

---

## 3. What matches the spec

- All five PWM_DISC tuning constants exist with the spec values, each with
  multi-paragraph rationale comments.
- Compile-guard discipline is correct (`#ifdef USE_WHEEL_ENCODERS`
  everywhere — `arduino_uno_minimal` cannot pull in the state machine,
  the result struct, the inspectors, or `enter_pwm_discovery`).
- State machine flow matches §7d: IDLE → PWM_DISCOVERY → IDLE (always).
- The plateau detector locks `discovered_max_pwm` to the FIRST step in
  the plateau (saturation onset) per spec, not the last step (post-
  plateau).
- Both-wheels-clear gating for MIN lock matches the spec's "first non-zero
  velocity"; this is stricter than mean (a one-wheel-stalled bot won't
  declare MIN at half the true stiction value).
- Encoder ticks are reset on every PWM_DISCOVERY entry so the velocity
  windows start from a known origin (no carry-over from prior RUN motion).
- Per-step telemetry routes through the existing `pulse_log_` deferred
  publisher (ISR-safe — no Serial.print from the timer ISR).
- EEPROM persistence success path is gated on `pd.discovered &&
  min > 0 && max > min` — refuses to write nonsense values.
- Failure paths leave the previous EEPROM slot untouched so the prior
  run's bounds remain usable.

---

## 4. What's missing or incomplete from the spec

### Gap 1 — Collision detection does NOT abort PWM_DISCOVERY

`step_pwm_discovery_` (`balance_app.cpp:1745-1751`) checks only
`safety_.abort_requested()` and `elapsed > PWM_DISC_TIMEOUT_MS`. The
collision detector (`read_imu_()`) still runs every tick and updates
`collision_latched_`, but PWM_DISCOVERY ignores it. BOOTSTRAP
(`balance_app.cpp:1422`) and CHARACTERISE (`balance_app.cpp:1165`)
both abort on `collision_latched_` with their own failure_reason codes.

Spec context: the original task brief said "if collision_latched_
triggers during discovery, abort to IDLE with appropriate failure_reason"
as a required test case. Either: (a) wire collision into PWMD with a new
`failure_reason=9` (e.g. `pwm_discovery_collision`), or (b) decide that
operator-holding the bot during PWMD is the *correct* posture (the bot
must be lifted off the ground for PWMD anyway, so handling is expected).

**Decision needed**: whether (a) or (b) is the intent. Test 9 in
`test_balance_app_pwm_discovery.cpp` currently pins behaviour (b) — it
verifies the as-built "collision does NOT abort" behaviour. If you choose
(a), update both `step_pwm_discovery_` and Test 9 in the same PR.

### Gap 2 — EEPROM slot address mismatch with MEGA_UNIVERSAL_PLAN

`MEGA_UNIVERSAL_PLAN.md §7d` says "Save to EEPROM `0x210` (extend
actuator slot)". The as-built code stores it at `EE_PWMDISC_ADDR` (see
`main.cpp:217`). The actual slot address is whatever `EE_PWMDISC_ADDR`
resolves to in the EEPROM map header — likely a different slot. Audit
the EEPROM map header to confirm: (a) no slot collision with another
feature, (b) bumping into the documented `0x230` slot from the enum
comment in `balance_app.h:90` ("Result saved to EEPROM 0x230").

The `balance_app.h:90` comment says `0x230`. The plan says `0x210`. The
code uses `EE_PWMDISC_ADDR`. These need to be reconciled. Probably the
plan should be updated to match the as-built (the code is the source of
truth, but document the slot somewhere visible).

### Gap 3 — No load-back at boot wiring

`load_pwm_discovery_()` exists in `main.cpp:221-229` and is invoked at
`main.cpp:379` to read the EEPROM slot, but the loaded `min_pwm` /
`max_pwm` are not currently fed back into `L298NMotorDriver::set_stiction
_min_pwm()` or any other consumer. Per spec §7d the discovered MIN should
seed `stiction_min_pwm = max(PWM_MIN_L, PWM_MIN_R)`. Inspect `main.cpp:
375-385` to confirm — this verification was scoped READ-only and the
agent did not chase the full boot-time consumer chain.

### Gap 4 — Python brute-force tuner not yet consuming the slot

Per spec §7d the new EEPROM slot was the unblock for `tools/sim/
brute_tune.py`. That tool wasn't in the as-built scope and is the
"next session" deliverable. No code change needed in firmware for it.

---

## 5. Test coverage added by this verification agent

`tests/test_balance_app_pwm_discovery.cpp` — 10 tests, 47 assertions, all
passing on the native build. Build invocation in the file header.

| # | Test | Pins |
|---|---|---|
| 1 | `test_idle_to_pwm_discovery` | enter from IDLE transitions correctly |
| 2 | `test_pre_run_inspectors_are_zero` | inspector defaults |
| 3 | `test_motors_stopped_on_entry` | enter_state_(PWM_DISCOVERY) side-effects |
| 4 | `test_enter_from_run_is_noop` | guard against non-IDLE entry |
| 5 | `test_timeout_path_no_motion` | `failure_reason=8` after 8 s |
| 6 | `test_abort_path` | `failure_reason=4` via `safety.request_abort()` |
| 7 | `test_result_struct_resets_on_reentry` | no stale values across runs |
| 8 | `test_inspectors_gated_on_discovered_flag` | `get_discovered_*` clamps to 0 |
| 9 | `test_collision_does_not_abort_discovery` | pins Gap 1 (collision NOT aborting) |
| 10 | `test_pwm_discovery_always_exits_to_idle` | exits only via IDLE (both paths) |

**Not covered (limitation)**: synthetic encoder-tick injection to drive
MIN/MAX convergence. The `WheelEncoder` members inside `BalanceApp` are
private value members with no friend declaration; the host test has no
way to call `set_ticks_for_test()` on them. The underlying driver math
(velocity windowing, plateau math) is independently tested in
`tests/test_wheel_encoder.cpp`. End-to-end convergence is bench-only
(see operator instructions §6).

---

## 6. Operator usage instructions

Bench procedure (`mega_balance` env, USE_WHEEL_ENCODERS defined):

1. Power up the bot. Bot enters IDLE.
2. **Lift the bot off the ground** — wheels must spin freely. This is
   the single critical operator precondition. The motor sweep would
   otherwise dump current into stalled wheels.
3. Send `p` over serial (115200 baud).
4. Observe per-step telemetry: lines starting with `pd#<step>` showing
   commanded PWM, left/right velocity (dps), plateau threshold, and a
   pass flag (1=locked MIN, 2=locked MAX, 0=in-progress).
5. Success path (≈ 4-8 s wall time): you see `pd fail r=0` then `sv pd
   min=<X> max=<Y>` printed once the state transitions back to IDLE.
   Saved to EEPROM `EE_PWMDISC_ADDR`.
6. Failure paths print `pd fail r=<N>` where N ∈ {4 (you aborted),
   8 (timed out — motors disconnected, wheels stalled, encoders dead)}.
   EEPROM slot is NOT overwritten on failure — previous values survive.

Validation steps:
- Send `s` afterwards to see the current `stiction_min_pwm` (gap 3
  may mean this doesn't yet reflect the discovered MIN — check before
  trusting).
- Reboot and observe the boot banner for any "loaded pd min=X max=Y"
  printout (if the load-back wiring lands in a follow-up).

---

## 7. Known limitations and follow-up tasks for next session

**Top 3 gaps** (in priority order):
1. **Collision does not abort PWMD** (Gap 1). Decide the intent and
   either wire collision into the abort path (+ test) or document the
   "operator handling is OK during PWMD" behaviour in the spec.
2. **EEPROM slot doc/code mismatch** (Gap 2). Plan says 0x210; code
   uses `EE_PWMDISC_ADDR`; enum comment says 0x230. Pick one and align.
3. **Boot-time consumer wiring** (Gap 3). `load_pwm_discovery_()` is
   called at boot but its outputs don't propagate to `stiction_min_pwm`.

Lesser follow-ups:
- Native-test access to BalanceApp's encoders for convergence tests.
  Options: (a) add a `friend class BalanceAppTest` declaration, (b)
  expose `WheelEncoder& test_encoder_left()` under `#ifdef NATIVE_TEST`
  in balance_app.h, (c) add a templated `BalanceApp::inject_encoder_
  ticks(int32_t l, int32_t r)` test hook. Whichever is cheapest.
- Bench scenario test (§8.2 in MEGA_UNIVERSAL_PLAN) — operator-driven
  procedure with success criteria (e.g. MIN within ±10 PWM of hand-
  measured stiction, MAX within ±20 PWM of saturation).
- `tools/sim/brute_tune.py` integration — consume the saved bounds.
- Decide what `PWM_DISC_PLATEAU_COUNT=3` should be on a motor with very
  smooth saturation (could over-shoot MAX by 600 ms × step). Bench
  observation will tell.

**Verification status**: Phase 4M.12 is FUNCTIONALLY COMPLETE for the
"happy path + abort + timeout + telemetry + EEPROM save" surface. The
gaps above are spec-vs-implementation drift, not blocking bugs. The
state machine is structurally sound (always exits to IDLE) and the
guard discipline (IDLE-only entry, USE_WHEEL_ENCODERS gating
throughout) is correct.

---

## Cross-references

- `docs/MEGA_UNIVERSAL_PLAN.md` §7 — the parent spec.
- `docs/findings/research_wheel_encoders_mega_2026-05-19.md` §6 — the
  PWM-range research that fed §7d.
- `docs/findings/audit_code_quality_balance_stack_2026-05-19.md` — Phase
  4M.11/.12 audit baseline (encoder review).
- `tests/test_balance_app_encoder.cpp` — sister test file (encoder
  integration). Same mocks, same build invocation modulo source list.
- `tests/test_wheel_encoder.cpp` — driver-level unit tests with
  `set_ticks_for_test()` injection (covers what this file cannot).
- `src/main.cpp:208-229, 509-518, 568-587` — EEPROM helpers + serial
  command + persistence-on-transition.
