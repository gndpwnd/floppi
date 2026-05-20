# Phase 2 — CHARACTERISE_ACTUATOR Final Plan

**Status:** Ready to implement. Next session.

This is the deferred-from-2026-05-18-bench-session implementation plan, incorporating:

- The original agent design (8-pulse sweep, ~250 B impl).
- The flash-budget reality (overflowed by 104 B on first attempt; need ~150 B more savings before retry).
- The operator's 2026-05-18 reinforcement that **stiction is per-wheel, not combined** (rubber bands shift, motors wear unevenly).

## Why this is the only path forward

The 2026-05-18 bench session ended with a working bot that balances briefly, after iterating through three hardcoded-value tweaks (stiction floor 0 → 30 → 80; Kd 20 → 10; tilt 10° → 8°; HELD threshold 30 → 90). Every one of those numbers is a scope violation per [scope.md §Non-negotiable design constraints](../scope.md). They will be wrong for:

- The same bot tomorrow (battery sag, motor wear).
- The same bot after a rubber-band slips off one wheel.
- Any other bot on any other operator's bench.

The framework cannot ship with these as constants. CHARACTERISE replaces them with measured values.

## Hardware findings from the bench session that this plan encodes

- Bot's actual physical stiction is ~100 PWM (PWM 90 produced no wheel motion; PWM 200 spun cleanly).
- The hardcoded 0 floor wasted every PID command below 100 PWM (apparent two-state motor behavior).
- A floor of 80 made the bot balance for ~5 seconds (proof the algorithm works once stiction is right).
- Operator reports per-wheel stiction divergence is real (rubber bands).
- 38-second STUCK episode (motors at +255 with operator restraining bot) caused by no detection of "saturated output, no motion." STUCK detector added 2026-05-18 — but its threshold (180 PWM) is itself hardcoded; CHARACTERISE must derive it from the measured saturation point.

## State definition

```text
CHAR_ACT (enum value 6 in BalanceAppState)
  Entry:    from IDLE on serial `k` command OR boot auto-trigger if no EEPROM record.
  Exit:     to IDLE on completion OR operator abort.
  Motors:   actively driven during pulses (this is the only state besides RUN where motors move).
  Operator: bot should be on its wheels on a flat surface. Bot will move 5-15 cm during sweep.
```

## Two-phase sweep (per-wheel, not combined)

### Phase A — combined-wheel sweep (find rough stiction)

Pulse both wheels in the same direction. 8 pulses, 200 ms each, alternating sign:

```text
PWM_TABLE = {30, 50, 70, 90, 110, 130, 150, 200}
```

For each pulse: accumulate sum of `|raw_gyro_pitch_dps|` over the 200 ms (40 ticks at 5 ms). First PWM whose accumulator exceeds threshold = combined stiction floor.

Also track when the accumulator stops growing pulse-to-pulse → saturation PWM.

Time: 1.6 s.

### Phase B — differential sweep (per-wheel asymmetry)

Pulse wheels in *opposite* directions (left forward, right back). This produces yaw rotation, not pitch. Yaw response is far cleaner than pitch for per-wheel discrimination because gravity does not couple to yaw at small tilts.

4 pulses at the Phase-A-determined combined stiction PWM, alternating which wheel is forward:

```text
pulse_1: L=+stiction, R=-stiction  (positive yaw if symmetric)
pulse_2: L=-stiction, R=+stiction  (negative yaw if symmetric)
pulse_3: L=+stiction*1.3, R=-stiction*1.3  (overdrive)
pulse_4: L=-stiction*1.3, R=+stiction*1.3
```

Accumulate `|gyro_yaw_dps|` per pulse. If `mean(|yaw|_LR_forward) != mean(|yaw|_RL_forward)` significantly, one wheel has more authority than the other. Ratio = per-wheel asymmetry.

Time: 0.8 s.

**Total sweep: 2.4 s.**

## Persistent storage layout

Mirror the existing mount-offset save pattern. New EEPROM record at offset 0x210:

```text
EE_ACTUATOR_ADDR  = 0x210
EE_ACTUATOR_LEN   = 16   (8-byte aligned; extra room for future fields)
EE_ACTUATOR_MAGIC = 0xAC
EE_ACTUATOR_VER   = 0x01

byte 0: magic 0xAC
byte 1: version 0x01
byte 2: stiction_floor_combined  (uint8_t, PWM)
byte 3: saturation_combined      (uint8_t, PWM)
byte 4: asymmetry_left_pct       (int8_t, -100..+100, +5 means left wheel needs 5% more PWM)
byte 5: reserved
byte 6-13: reserved for K_motor float + noise-floor in future
byte 14: reserved
byte 15: xor_crc8 of bytes 0..14
```

## Interface changes

### `L298NMotorDriver`

```cpp
// Already landed 2026-05-18:
void set_stiction_min_pwm(uint8_t v);

// New for per-wheel asymmetry:
void set_per_wheel_gain(uint8_t left_pct, uint8_t right_pct);  // 100 = baseline
// In set_speeds(): apply per-wheel scaling before stiction floor.
```

### `BalanceApp`

```cpp
void enter_characterise_actuator(uint32_t now_ms);  // No-op if not IDLE.
uint8_t get_ch_stiction_floor() const;
uint8_t get_ch_saturation() const;
int8_t  get_ch_asymmetry_pct() const;
bool    is_characterised() const;  // True after sweep succeeded.
```

### `main.cpp`

- Serial command `k` → triggers `enter_characterise_actuator()`.
- Boot logic: if no EEPROM actuator record, set a flag, skip auto-RUN, run CHARACTERISE first. Save result. Then auto-RUN.
- State-transition handler: on CHAR_ACT → IDLE, save results to EEPROM, push to motor driver.

## Flash budget

Previous attempt cost ~330 B (Phase A only, no differential). Per-wheel adds ~80 B. Total ~410 B needed.

Available pre-trim: 38 B (worst case, current state with safety fixes).

**Required additional savings: ~370 B.** Concrete candidates ranked by safety:

| Saving | Bytes | Risk |
| --- | --- | --- |
| Remove the `s` command's verbose F-strings, keep CSV format from earlier failed attempt but with single F-string concatenation | 80-120 | Low (just status formatting) |
| Remove `r` command (cal-wizard reset) — move to long-press button | 30-40 | Low (rarely used) |
| Remove periodic 60s mount-offset save during RUN — only save on transitions | 40-60 | Low (EEPROM cell life is fine) |
| Drop the BNO055 cal wizard's verbose per-axis prints, use one combined print | 30 | Low |
| Drop `state_name()` long-form strings, return state index as a single digit | 30-50 | Medium (changes serial UX) |
| Inline or remove `restoreFromEEPROM` wrapper indirection | 20-30 | Low |
| Move `R` command logic into a smaller helper | 10-20 | Low |

Conservative estimate: 240-330 B of trims feasible. Plus ~50 B from making CHAR_ACT impl tight (skip abort handling in-state, use uint8 timer instead of uint16, share gyro accumulator slot with the existing `cap_*` capture fields).

## Implementation order (one focused session)

1. **Flash trims first** (~30 min). Build after each. Target: 400+ B free.
2. **Add `set_stiction_min_pwm()` setter** (already landed) + `set_per_wheel_gain()` to L298NMotorDriver (~5 min).
3. **Add CHAR_ACT enum + plumbing** (state_name, tick switch, enter_state_) (~10 min).
4. **Add ch_* private fields + accessors to balance_app.h** (~5 min).
5. **Implement step_char_act_() with Phase A + Phase B logic** (~30 min).
6. **EEPROM save/load helpers + apply at boot** (~15 min).
7. **Serial `k` command + CHAR_ACT → IDLE transition handler in main.cpp** (~10 min).
8. **Bench validation** with operator at bot (~30 min):
   - Run `k`, watch sweep complete.
   - Verify saved floor + saturation + asymmetry in `s` output.
   - Re-test balance with characterised values.

Total: ~2 hours focused. No parameter iteration, no side-quests.

## What this does NOT include (deferred to later phases)

- **Phase 2.5**: cheap FALLEN heuristic (motion without intentional motor command). Wait until 2.7's full motor-null detector is feasible — then this becomes a fallback for non-IMU platforms.
- **Phase 2.6**: gain scheduling (gentle near balance, aggressive far). Needs Phase 2 K_motor first so the "near vs far" thresholds are derived, not constants.
- **Phase 2.7**: motor-null-space HELD/INFLUENCED/STUCK detector with BNO055 LIA. Research delivered ([research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md)). ~200 B + 40 B RAM. Only after Phase 2 lands.
- **Continuous re-characterisation**: if balance behaviour drifts past a tolerance, re-run CHARACTERISE without operator intervention. Defer until basic CHARACTERISE works.

## Success criteria

After Phase 2 lands, the bot must:

1. Boot with no EEPROM actuator record → automatically run CHARACTERISE → save results → auto-RUN.
2. Show measured stiction_floor, saturation, asymmetry in `s` output.
3. Balance at least as well as iteration 3 of the 2026-05-18 bench session (≥5 s contiguous RUN, proportional motor outputs, no STUCK episodes).
4. Pass the rubber-band test: remove a rubber band from one wheel, send `k`, bot re-characterises and adapts; balance behaviour preserved.
5. Battery-swap test: swap to a depleted battery, send `k`, characterisation re-finds new stiction (which will be higher), balance preserved.

## See also

- [scope.md §Non-negotiable design constraints](../scope.md) — the rule this plan implements.
- [findings/dynamic_pwm_accel_learning.md](dynamic_pwm_accel_learning.md) — the design ancestor (Phase 4.10 RLS K_motor learning).
- [findings/research_motor_null_space_handling_detection.md](research_motor_null_space_handling_detection.md) — Phase 2.7 follow-on.
- [findings/operator_ideas_backlog.md](operator_ideas_backlog.md) — rows 11, 13, 14 (this plan), rows 9, 10, 12 (deferred follow-ons).
- [archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md](../archive/session_records/2026-05-18_BENCH_MOTOR_STICTION_DIAGNOSIS.md) — the diagnostic chain that produced this plan.
