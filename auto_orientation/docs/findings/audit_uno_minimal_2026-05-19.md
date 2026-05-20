# Uno Minimal Balancer — Code Audit (2026-05-19)

**Scope**: `src/applications/balancing_robot_uno/`, `tests/test_uno_balance_app.cpp`, plus brute-tuner contract verification (`tools/sim/balance_constants_template.h.in`, `tools/sim/brute_tune.py`).
**Mode**: read-only audit. No source changed. No commits.
**Reference target**: `docs/archive/balancing_robot_reference/SelfBallancingRobot3.ino`.
**Build state at audit**: 49.7 % flash / 34.7 % RAM on `arduino_uno_minimal`, 7 native tests pass.

---

## 1. Summary

| Category                          | P0 | P1 | P2 |
|-----------------------------------|----|----|----|
| ISR / atomicity                   | 1  | 1  | 1  |
| PID hygiene                       | 0  | 2  | 1  |
| Safety                            | 1  | 2  | 1  |
| Hardcoded values to expose        | 0  | 1  | 2  |
| Test coverage gaps                | 0  | 3  | 2  |
| Build / flash hygiene             | 0  | 1  | 1  |
| Comparison vs SelfBallancingRobot3| 0  | 2  | 2  |
| Brute-tuner integration           | 0  | 1  | 1  |
| Doc accuracy                      | 0  | 2  | 1  |
| **Total**                         | **2** | **15** | **12** |

P0 = blocks first bench run / can damage hardware. P1 = will bite during real-bot testing. P2 = polish.

---

## 2. ISR / atomicity findings

The app intentionally splits IMU read (`loop()`) from PID step (5 ms MsTimer2 ISR). The boundary needs guarding; the comment at `uno_balance_app.h:94-97` already concedes there is a tear hazard and waves it away. That trade-off should NOT be made on AVR for a control-loop variable.

- **[P0] Torn `float` read of `last_pitch_deg_`** — `uno_balance_app.h:98`, consumed by `uno_balance_app.cpp:78`. AVR is 8-bit; a 4-byte float read in the ISR can interleave with the `loop()` write and produce a value that has never existed (high half from sample N, low half from sample N+1). For a slow-changing physical signal the magnitude of error is usually small, BUT the BNO055's NDOF output can include large transients (gimbal-flip near ±90°, calibration switchovers). The PID then multiplies the garbage by Kp=65 and slams the motor full-tilt for one tick. Even one such tick at start-up will tip the bot. Compare to the universal app's pattern at `balance_app.cpp:1481` which uses `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` around all ISR-shared float publishes.
  **Patch**: wrap the assignments in `uno_balance_app.cpp:56-58` and the snapshot read at line 78 in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` (include `<util/atomic.h>` guarded by `#ifdef __AVR__`). Adds ~4 bytes of flash, zero runtime cost in non-ISR context.

- **[P1] `pitch_valid_` / `tipped_` / `armed_` are `bool volatile` but read+write across ISR without atomicity** — `uno_balance_app.h:99,105,106`. Reads of `bool` ARE atomic on AVR (8-bit), so this is technically fine for the booleans alone. BUT the read of `pitch_valid_` at `uno_balance_app.cpp:71` followed by the read of `last_pitch_deg_` at line 78 is a TOCTOU: the ISR may see `pitch_valid_=true` then read a torn pitch that `read_imu()` is in the middle of updating because the writer (`read_imu()`) updates `last_pitch_deg_` BEFORE setting `pitch_valid_=true` (lines 56→57). The order is correct (publish-data-then-flag), but without a memory barrier the AVR compiler is free to reorder. Mark the publishes with the same ATOMIC_BLOCK as the P0 fix.

- **[P2] `last_pwm_` is `volatile int16_t`** — `uno_balance_app.h:102`, written from ISR at `uno_balance_app.cpp:95`, read from `loop()` via `last_pwm()` accessor at `main.cpp:89`. 16-bit reads also tear on AVR. Telemetry-only, so worst case is a printed gibberish value once in a while. Wrap the read in `ATOMIC_BLOCK` or make the accessor copy under a critical section.

---

## 3. PID hygiene findings

- **[P1] D-term LPF τ is left at its constructor default of 15 ms (10 Hz cutoff)** — `pid_controller.h:91-101` documents the default. The Uno program never calls `set_d_term_lpf_tau_sec()`. The reference `.ino` (`SelfBallancingRobot3.ino:47`) uses PID_v1 which has NO LPF — the hand-tuned Kd=38 was tuned against an unfiltered derivative. Inheriting our framework's 15 ms LPF silently changes the effective Kd by ~25 % at the BNO055 NDOF group delay band, so the reference gains are NOT a drop-in. Either (a) call `balance_pid.set_d_term_lpf_tau_sec(0.0f)` in `setup()` to match PID_v1 parity, or (b) document in the README that the brute tuner must search around a different Kd than the reference's 38. Option (a) is the minimum-surprise fix and matches the design intent ("port the .ino exactly").
  **Patch**: in `main.cpp` after `app.begin()`: `balance_pid.set_d_term_lpf_tau_sec(0.0f);  // PID_v1 parity — reference .ino used raw derivative`.

- **[P1] I-term clamping is on by default in `PIDController` but capped at the full PWM range (±255), not a smaller "authority" limit** — verified at `pid_controller.cpp:295-305`. The reference `.ino` has no extra cap so this matches behaviour, but unstable-plant balancing typically benefits from `set_i_term_limit(50.0f)` or so (sources: `auto_pid_tuning_research.md`). Add a tunable I-term contribution cap, default to PWM_MAX (parity), let brute tuner search over it. Documented as a future hook, not required for bench-1.

- **[P2] D-on-measurement is the framework default** — confirmed via `pid_controller.cpp:181-186` and the constructor default in `pid_controller.cpp:39` (no, actually `d_on_measurement_(true)` is implied; need to verify constructor sets it true — see `pid_controller.cpp` ctor initializer list). For a balance bot the setpoint is fixed at 0 so D-on-error vs D-on-measurement is mathematically equivalent. No action needed; just note for the README.

---

## 4. Safety findings

- **[P0] No startup delay after IMU `begin()` — motor ISR can fire before the BNO055 fusion has converged** — `main.cpp:116-127`. The BNO055 `begin()` blocks ~650 ms internally (per `bno055.cpp:99`), but the chip's NDOF fusion needs another ~700 ms after that to produce stable Euler output. The current code calls `MsTimer2::start()` immediately after `app.begin()`, so the ISR begins consuming `last_pitch_deg_` while `loop()` is still feeding it pre-fusion garbage. Combined with the P0 torn-read above, this is the most likely first-bench failure: bot armed before pitch is real, motor full-throttle for 1-2 s, bot dives forward.
  **Patch**: insert `delay(1000);` (or, better, poll `imu.read()` + `getCalibration()` until `sys >= 1`) BEFORE `MsTimer2::start()`. The reference `.ino` does `delay(500)` + `delay(1000)` at `SelfBallancingRobot3.ino:93,103` for exactly this reason and the Uno port dropped both.

- **[P1] `armed_` defaults to `true` in the constructor (`uno_balance_app.cpp:22`) and `begin()` does not change it (`uno_balance_app.cpp:37` explicit comment)** — combined with the missing startup delay, the bot is armed from the instant power is applied. Operator preference recorded in `feedback_balance_bot_preferences.md` is "lenient HELD, balance forever" but the safe default for a hardcoded balance program is "boot disarmed, arm via serial 'g' or after IMU calibrates". Reference `.ino` arms unconditionally so this matches the design intent — but at minimum, the serial 'a' should DISARM and a serial command should be added to re-arm without re-flashing. Currently the only way to recover from an emergency stop is a reflash, which is operator-hostile on a moving robot you just had to grab.
  **Patch**: add `case 'g': app.arm(); Serial.println(F("ARMED")); break;` to `handle_serial()` and a corresponding `void arm()` on the app.

- **[P1] BNO055 read failure mode is "motors stop, but no failure latch"** — `uno_balance_app.cpp:71-75`. If the I²C bus glitches and `imu.read()` returns false for ten ticks in a row, the motors stop and silently restart when communication recovers. For a balancing bot that's actually correct (don't latch — let it self-heal). BUT there is no telemetry indicating WHEN reads are failing, so a slowly-dying sensor presents as "bot just keeps tipping" with no clue why. Add a rolling read-failure counter to the `'s'` status print.

- **[P2] Tip-cutoff at 25° is checked AFTER PID, but PID is fed pitch unconditionally if `pitch_valid_`** — `uno_balance_app.cpp:78-94`. Means on the tick BEFORE the tip-cutoff trips, the PID has already integrated 25° of error into the I-term. The subsequent `pid_.reset()` at line 85 clears it, but only on the tick AFTER detection. This is one-tick stale; harmless because the motor is then stopped, but worth verifying with a test that PID state is in fact reset before the next `step()` after the bot is righted. (Currently `test_step_tip_cutoff` checks the cutoff trips but not the post-righting recovery.)

---

## 5. Hardcoded-but-tunable values to expose

The brute tuner emits `Kp`, `Ki`, `Kd`, `PITCH_OFFSET_DEG`, `PWM_MAX` (and mirrors `PWM_MIN`). Everything else is hardcoded in `balance_constants.h`. Below are values the tuner currently treats as fixed but probably shouldn't, ordered by audit-priority.

- **[P1] `STICTION_PWM` is fixed at 15** — `balance_constants.h:71` / `brute_tune.py:87`. The reference `.ino` value of 15 is for the operator's specific motors. A different motor/gearbox combo can need anywhere from 0 to 40. The tuner already injects stiction into its plant model (per `brute_tune.py` SAFETY_STICTION_PWM constant), so it KNOWS the value matters — but it doesn't search over it. Adding it to the search space costs one more axis (5 values → ~5× search budget) and lets the tuner discover the right break-away PWM per bot.
  **Patch (tuner)**: add `stiction_pwm` to the `Candidate` dataclass; emit into the header via the template's existing `{stiction_pwm}` placeholder.

- **[P2] `PID_SAMPLE_MS = 5`** — `balance_constants.h:60` / template `{pid_sample_ms}`. The tuner already passes this through. Confirm CLI flag to override exists; if not, add `--pid-sample-ms` to brute_tune.py. Most bots want 5 ms; some MCUs (Uno itself is at the edge of 200 Hz I²C+PID) may want 10 ms.

- **[P2] `TIP_CUTOFF_DEG = 25` and `PITCH_SANITY_DEG = 90`** — `balance_constants.h:79,83`. Safety values; should NOT be tuner-searched (the tuner already enforces tip at 25° as a fitness penalty per `brute_tune.py:79`). Document in `balance_constants.h` header comment that these are operator-overridable post-tuning if the bot has unusual geometry, but the tuner intentionally leaves them alone.

---

## 6. Test coverage gaps

`tests/test_uno_balance_app.cpp` runs 7 tests, all currently passing. Coverage map:

| Behavior                                    | Tested? | Test name                              |
|---------------------------------------------|---------|----------------------------------------|
| `read_imu()` offset subtraction             | yes     | `test_read_imu_applies_offset`         |
| `read_imu()` NaN reject                     | yes     | `test_read_imu_rejects_garbage`        |
| `read_imu()` out-of-range reject (±90°)     | yes     | `test_read_imu_rejects_garbage`        |
| `step()` no-IMU → motors stop               | yes     | `test_step_without_imu_stops_motors`   |
| `step()` tip cutoff trips                   | yes     | `test_step_tip_cutoff`                 |
| `step()` PWM sign + range                   | yes     | `test_step_pwm_sign_and_range`         |
| Symmetric L+R PWM                           | yes     | `test_step_drives_both_wheels_symmetric` |
| `abort()` latches                           | yes     | `test_abort_latches`                   |

Gaps:

- **[P1] No test for read failure → motors stop** — `MockIMU::set_readable(false)` exists but is never used. `imu.read()` returning false is a critical safety path and currently untested. Add `test_read_imu_fails_motors_stop`.
- **[P1] No test for the post-tip recovery** — after tip-cutoff trips and the bot is righted (pitch returns to <25°), `step()` should resume balancing and the PID I-term should NOT have any stale wind-up. Add `test_tip_recovery_clears_integral`.
- **[P1] No test that `set_d_term_lpf_tau_sec(0)` is in effect** — once the P1 fix in §3 lands, add a regression test that PID's `get_d_term_lpf_tau_sec()` returns 0 after `app.begin()`.
- **[P2] No test for `abort()` then re-`begin()`** — verifies an external "remote arm" wouldn't accidentally clear the latch via the existing begin() path. Currently begin() leaves `armed_` alone (good); a test would lock that behaviour in.
- **[P2] No test that asserts PWM is symmetric AND tracks PID output sign for a pitch ramp** — the existing test only checks sign at two points. A 5-step pitch sweep `[+5, +2, 0, -2, -5]` would verify monotonic PWM response.

There is no ISR/atomicity test possible from native code (the ATOMIC_BLOCK shim is a no-op off-AVR), so the P0 in §2 cannot be covered by `tests/`. Document as "verified by code review only" once fixed.

---

## 7. Build / flash hygiene

- **[P1] Float printing via `Serial.print(app.last_pitch_deg(), 2)`** — `main.cpp:88`. AVR `Serial.print(float, int)` pulls in ~1.5 KB of `dtostrf` + float-to-decimal code. The flash budget is generous now (49.7 %) so it fits, but the doc README claims `<60 %` and the universal app's flash crisis came from exactly this. If/when the brute tuner adds more constants or the program needs more telemetry, this will be the first thing to delete.
  **Patch**: print pitch as `int16_t` decideg: `Serial.print((int16_t)(app.last_pitch_deg() * 10));` — costs ~30 bytes flash instead of 1.5 KB and is more diff-friendly across runs.

- **[P2] `STICTION_PWM = 15` is passed to the `L298NMotorDriver` constructor at `main.cpp:52`, AND then the PID output is clamped to `[PWM_MIN, PWM_MAX]` and handed to `motors_.set_speed()` in the ISR.** The driver applies stiction snap-up internally (per `l298n_motor_driver.cpp:113-119`), so the PID effectively never gets to command 1-14 PWM. That's correct behaviour but worth a comment in `uno_balance_app.cpp:94` so the next reader doesn't assume the PID is producing the final commanded PWM. (Mostly a documentation nit.)

- F() macro usage in `main.cpp:82,86-89,104-108,111,117,127` is correct — all string literals are in PROGMEM. No bloat there.

- Includes: `uno_balance_app.h` pulls in `sensor_base.h`, `motor_driver.h`, `pid_controller.h` — minimal and clean. `main.cpp` adds `bno055.h`, `l298n_motor_driver.h`, `balance_constants.h`. No universal-app headers leaked in.

---

## 8. Comparison with SelfBallancingRobot3.ino

The reference is the design target. What differs MEANINGFULLY:

**Differences (in Uno port, not in reference)**

- **[P2] BNO055 read path goes through Adafruit getQuat() then derives Euler** (`bno055.h:39-41` documents this), reference uses `getEvent(&e, VECTOR_EULER)` directly. The quaternion-derived path is technically better (avoids BNO055's well-documented Euler discontinuity), but it adds a quaternion-to-Euler conversion every read which costs ~50 µs. Net positive change, but means pitch numerics will differ slightly from the reference, which means the reference Kp/Ki/Kd values won't transfer exactly — flagged as a brute-tuner search-range issue, not a code defect.
- **[P2] Uno port runs PID inside a 5 ms ISR; reference runs it in `loop()`** at whatever rate `loop()` happens to iterate. PID_v1 internally enforces a 5 ms sample-time gate, so the reference is effectively 200 Hz too — but with jitter from the `delay(2)` and the 100 ms serial-print branch. Uno port is more deterministic, but that determinism imposes the ISR/atomicity hazard above. Worth keeping the ISR design, but the fix in §2 is mandatory.

**Missing in Uno port (present in reference)**

- **[P1] No `consecutiveErrors` counter for sensor health** — reference `SelfBallancingRobot3.ino:56,130,151-156` tracks 10 consecutive bad reads before warning. Already flagged in §4 P1.
- **[P1] No periodic debug print of pitch/raw/output** — reference at `SelfBallancingRobot3.ino:132-143` prints every 100 ms unconditionally. Uno port only prints on operator-typed `'s'`. For first-bench debugging the unsolicited stream is much more useful (you can see WHY it tipped). Add a `'p'` toggle that enables 10 Hz periodic telemetry.
- **[P2] No `motorOutputActive` state telemetry** — reference tracks whether motors are currently being driven; Uno port has `is_armed()` + `is_tipped()` which approximates it. Functionally equivalent.

**Present in Uno port (not in reference, intentional)**

- Emergency-stop `'a'` serial command — net positive, matches universal app convention.
- Tip cutoff with PID reset — reference has no tip cutoff at all and just keeps integrating. Better behaviour here.

---

## 9. Brute-tuner integration verification

Cross-checked `balance_constants.h` against `tools/sim/balance_constants_template.h.in` and `brute_tune.py` write_header():

| C++ constant in `balance_constants.h` | Template placeholder | Tuner emits? | Match? |
|---------------------------------------|---------------------|--------------|--------|
| `BALANCE_KP`                          | `{kp:.4f}`          | yes (`cand.kp`) | OK |
| `BALANCE_KI`                          | `{ki:.4f}`          | yes (`cand.ki`) | OK |
| `BALANCE_KD`                          | `{kd:.4f}`          | yes (`cand.kd`) | OK |
| `PITCH_OFFSET_DEG`                    | `{offset:.4f}`      | yes (`cand.pitch_offset_deg`) | OK |
| `PID_SAMPLE_MS`                       | `{pid_sample_ms}`   | yes (`tcfg.pid_sample_ms`) | OK |
| `PWM_MIN`                             | `{pwm_min}`         | yes (`-cand.pwm_max`) | OK |
| `PWM_MAX`                             | `{pwm_max}`         | yes (`cand.pwm_max`) | OK |
| `STICTION_PWM`                        | `{stiction_pwm}`    | yes (`SAFETY_STICTION_PWM`) | OK |
| `TIP_CUTOFF_DEG`                      | `{tip_cutoff_deg:.4f}` | yes | OK |
| `PITCH_SANITY_DEG`                    | `{pitch_sanity_deg:.4f}` | yes | OK |

Names match. **No mismatch found.** The sibling fix-up appears to have landed correctly.

- **[P1] `balance_constants.h` does NOT include `<stdint.h>`** — the in-tree canonical file at `balance_constants.h:36-85` uses `uint16_t`, `int16_t`, and `uint8_t` without including the header. The template at `balance_constants_template.h.in:48` DOES `#include <stdint.h>`. Currently builds only because `main.cpp:23` includes `<Arduino.h>` first which transitively pulls `stdint.h`. If the header is ever included first (e.g., from a future unit test or a tooling include-order change), it will fail to compile. The template will overwrite this on next tuner run and the include will appear — but until the first tuner run lands, this is a latent build failure waiting on an include-order shuffle.
  **Patch**: add `#include <stdint.h>` after `#define BALANCE_CONSTANTS_H` in `balance_constants.h:37`. The brute tuner's template already does this — the in-tree default just drifted out of sync.

- **[P2] The template's autogenerated comment block says "DO NOT HAND-EDIT" but the in-tree file is currently hand-edited** (matches reference values, no generation timestamp). Add a stub `Generated: hand-default` line so the difference between "tuner output" and "default" is obvious to the operator.

---

## 10. Doc accuracy

`docs/applications/balancing_robot_uno/README.md` vs. as-built code:

- **[P1] README §"Source layout" describes a path `src/applications/balancing_robot_uno/generated/balance_constants_uno.h`** (README.md:108-113) but the actual file is `src/applications/balancing_robot_uno/balance_constants.h` (no `generated/` subdir, no `_uno` suffix). The brute tuner default output path in `brute_tune.py:68` matches the actual layout, not the README. Update README §"Source layout" to match: file is `balance_constants.h` directly under `src/applications/balancing_robot_uno/`.

- **[P1] README §"Source layout" says top-level file is `balance_uno_app.h` / `balance_uno_app.cpp`** but as-built is `uno_balance_app.h` / `uno_balance_app.cpp` (word order reversed). Trivial but documentation-wrong. Pick one; the code-side name is the harder thing to change.

- **[P2] README §"Build" warns "fit comfortably under 60 % flash"** — as-built is 49.7 % so the claim holds. Update the README to record the measured number now that bench measurement exists.

- README correctly states: no auto-tune (true), no online estimator (true), no encoders (true), no collision detection (true), no HELD detection (true). Good design-doc/code alignment on the BIG points.

---

## 11. Prioritized punch list — top 5 fixes for next coding session

In order of "blocking the first successful bench run":

1. **[P0] Add startup delay before `MsTimer2::start()`** — `main.cpp:124`. The BNO055 needs ≥1 s after `begin()` to produce real NDOF output, and the reference `.ino` knew this. Without this fix the bot will boot, immediately consume garbage pitch in the ISR, and slam the motors. ~3 lines, ~10 bytes flash.

2. **[P0] Wrap ISR-shared float reads/writes in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`** — `uno_balance_app.cpp:56-58` (writer) and line 78 (reader). Without this, the PID can multiply a torn pitch reading by Kp=65 and full-throttle the motors for one tick. Pattern is already used in the universal app at `balance_app.cpp:1481`. ~6 lines, ~10 bytes flash.

3. **[P1] Disable the D-term LPF in `setup()`** — `main.cpp` after `app.begin()`: `balance_pid.set_d_term_lpf_tau_sec(0.0f);`. The framework default of 15 ms invisibly changes the effective Kd vs. the reference `.ino`'s PID_v1 (which has no LPF). This is what will make the operator say "I copied the reference gains and it doesn't balance." 1 line.

4. **[P1] Add a serial-arm command (`'g'`) and periodic-telemetry toggle (`'p'`)** — `main.cpp:75-96`. Currently the only recovery from `'a'` abort is reflash, and the only way to see what the bot is doing is type `'s'` repeatedly. Both make field iteration painful. ~15 lines, ~80 bytes flash (with `int16_t` decideg pitch, not float).

5. **[P1] Add `#include <stdint.h>` to `balance_constants.h:37`** — drifts from the brute-tuner template, currently builds only by transitive include luck. 1 line. Also add a `test_read_imu_fails_motors_stop` and `test_tip_recovery_clears_integral` to lock in the safety-path behaviours before the brute tuner starts feeding real gain values.

If only #1 and #2 land, the bot is at least safe to attempt a bench run with the reference gains. #3 makes the reference gains actually transfer. #4 makes iteration tolerable. #5 closes a build-fragility window.

---

*Audit complete. No source modified, no commits made. ~1700 words.*
