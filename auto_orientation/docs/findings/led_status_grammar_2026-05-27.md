# Common LED Status Grammar — Uno + Mega `status_indicator`

**Date**: 2026-05-27
**Agent**: `ao-X1-led-grammar@floppi:1` (design doc, READ-ONLY over `src/`)
**Mode**: Single new findings file. No source edits, no builds, no commits.
**Roadmap item**: **X-1** (Common LED status grammar) from
`docs/findings/ao_roadmap_post_4m14_2026-05-20.md` — the cross-tier operator-UX
piece that prerequisites the future **U-2** (Uno LED feedback) and **M-5**
(Mega headless first-success) code drops.

---

## 0. TL;DR — what this doc is and is not

This is the **design contract** for a future `status_indicator.{h,cpp}` module
that both balance-bot tiers will share. **No code lands here.** The actual
~80-line implementation is intentionally deferred to the dispatched U-2 / M-5
work (both bench-gated), because the blink patterns are useless until a human
verifies them at arm's length on real hardware — picking patterns "in
simulation" then breaking them on the bench wastes a session. Writing the
grammar *now* prevents the alternative failure mode: U-2 and M-5 ship in
separate weeks, each invents its own ad-hoc blink language, and an operator
who learns the Uno bot has to relearn the Mega bot for no good reason.

**Headline decision** (Section 3): a **7-state grammar** keyed off **pulse
count** (1 / 2 / 3 / steady / fast / SOS / 3-on-1-off) at **two speeds**
(heartbeat 1 Hz, fault 5 Hz) — every pattern is distinguishable at arm's
length without an oscilloscope, and the same vocabulary maps cleanly onto the
Uno operator-visible states (`armed_`/`tipped_`/`cal_missing_block_arm_` plus
the calibration + tuning sessions) and onto the Mega `BalanceAppState` enum
(`IDLE`/`CAPTURE_MOUNTING`/`BOOTSTRAP`/`RUN`/`HELD`/`FALLEN`).

---

## 1. Why a shared grammar across tiers

The AO project deliberately ships **two balance-bot tiers** (`docs/scope.md`
memory-tier framing): the Uno tier is the smallest viable controller (no auto-
tune, no encoders, ~30 KB flash budget) and the Mega tier is the full
auto-discovery stack (BOOTSTRAP, PWM_DISCOVERY, mounting capture, held/fallen
state machine). They share IMU + motor driver, but their state machines are
*different shapes*: the Uno is essentially `armed × tipped` (two booleans),
the Mega is an 8-value enum. An operator who has flashed both naturally wants
to learn **one** physical-blink vocabulary, not two.

Three concrete benefits:

1. **First-success acceleration.** An end user with no serial monitor open
   (Section 7) can tell "the bot is alive and waiting" from "the bot needs cal"
   from "the bot is balancing OK" from across the room. This is the U-2 / M-5
   raison d'être.
2. **Cross-tier muscle memory.** Same blink → same meaning regardless of which
   bot is on the workbench. Reduces context-switch cost during the
   common "I'm tuning Mega, occasionally re-checking Uno" workflow.
3. **Documentation reuse.** One blink-vocabulary cheat-sheet (printed and
   taped to the workbench) covers both bots. Section 4 includes the
   cheat-sheet table.

This complements (does **not** replace) the existing serial status lines —
serial remains the diagnostic channel; LED is the operator-glance channel.

---

## 2. States to communicate — derived from actual code

I enumerated the operator-relevant states by reading the real state enums and
flags, not by guessing. Citations follow each row.

### 2.1 Uno tier (from `uno_balance_app.h` + `calibration_session.cpp` + `tuning_session.h`)

The Uno has no single enum — operator-visible state is the cartesian product
of a few booleans plus the active session:

| Operator-visible state | Source |
|---|---|
| `BOOTING` | `main.cpp` `setup()` window before `app.begin()` returns |
| `WAITING_FOR_CAL` | `cal_missing_block_arm_` latch in `uno_balance_app.h:179`; set when flight build boots without EEPROM cal blob; arm() refuses |
| `CAL_IN_PROGRESS` | `calibration_session.cpp:73-79` ("==== BNO055 GUIDED CAL ====") active |
| `TUNE_IN_PROGRESS` | `tuning_session.h:32-38` `TuneStage::STAGE_P` / `STAGE_D` / `STAGE_I` / `REVIEW` active (any non-IDLE) |
| `OPERATIONAL_OK` | `armed_ == true && tipped_ == false` (`uno_balance_app.h:127`, `:161-162`) |
| `OPERATIONAL_TIPPED` | `armed_ == true && tipped_ == true` (`uno_balance_app.h:127`) |
| `REFUSE_TO_ARM` | `cal_missing_blocks_arm() == true` after operator tried `g` (`uno_balance_app.h:121`) — distinct from `WAITING_FOR_CAL` only insofar as the operator has *attempted* to arm and been rejected; both can collapse onto the same pattern for simplicity, see §3 |
| `FAULT` | `read_fail_count_ > threshold` (`uno_balance_app.h:141`) — IMU dying mid-flight |

### 2.2 Mega tier (from `balance_app.h:89-108` + `balance_app.cpp:896-910`)

Mega's `BalanceAppState` enum is the source of truth. State-name strings come
from `BalanceApp::state_name()` (cited above):

| Enum value | `state_name()` | Operator meaning |
|---|---|---|
| `IDLE = 0` | `IDLE` | Booted, no session active, awaiting operator command |
| `CAPTURE_MOUNTING = 1` | `CAP` | Operator pressed `c`; 2-second window where bot must be held still while mounting offset is captured |
| `RUN = 3` | `RUN` | Balancing — the success state |
| `HELD = 4` | `HELD` | Operator picked the bot up; motors paused; will resume on release |
| `FALLEN = 5` | `FAL` | Sticky tip-over; operator intervention required |
| `CHAR_ACT = 6` | `?` | Phase 2 PWM sweep (rare, mostly research) |
| `BOOTSTRAP = 7` | `BOOT` | Phase 4.10c auto-derivation of Kp/Ki/Kd from K_motor pulses; takes ~10 s; operator should not interfere |
| `PWM_DISCOVERY = 8` | `PWMD` | Phase 4M.12 wheel-encoder PWM_MIN/MAX scan; operator lifts the bot |

Implicit operator state not in the enum:

- `BOOTING` — `main.cpp` `setup()` window before timer starts.
- `FAULT` — assertion failures, I²C errors, sensor disconnects (no dedicated
  enum slot; the bot just stops responding). The LED grammar gives this its
  own pattern so a silent freeze becomes a *visible* freeze.

### 2.3 Intersection — what the shared grammar needs to express

Boiling 2.1 and 2.2 down to the minimum mutually-distinguishable set:

| # | Shared name | Uno binds to | Mega binds to |
|---|---|---|---|
| 1 | `BOOTING` | setup window | setup window |
| 2 | `WAITING` (idle, awaiting input) | `WAITING_FOR_CAL` ∪ disarmed-but-cal-ok | `IDLE` |
| 3 | `OPERATOR_ACTION` (cal / mount capture) | `CAL_IN_PROGRESS` | `CAPTURE_MOUNTING` |
| 4 | `AUTO_ACTION` (auto-tune / bootstrap) | `TUNE_IN_PROGRESS` | `BOOTSTRAP` ∪ `PWM_DISCOVERY` ∪ `CHAR_ACT` |
| 5 | `OPERATIONAL_OK` | `armed && !tipped` | `RUN` |
| 6 | `SAFETY_HOLD` (intervention required, but recoverable) | `OPERATIONAL_TIPPED` | `HELD` ∪ `FALLEN` |
| 7 | `FAULT` (something is wrong, check serial) | `read_fail_count_` high | implicit (asserts, I²C errors) |
| 8 | `REFUSE_TO_ARM` (action required, NOT recoverable without operator) | `cal_missing_blocks_arm()` | (Mega has no equivalent — folds onto `WAITING`) |

**7 distinct patterns** cover the union — `REFUSE_TO_ARM` is Uno-only but
worth its own pattern because "won't arm" is a different operator
expectation from "ready, awaiting input".

---

## 3. LED blink grammar — the 7 patterns

Each pattern must be **visually distinct at arm's length without timing
instruments**. The two design knobs are **pulse count** (1, 2, 3, or
continuous) and **pulse speed** (heartbeat = 1 Hz cycle, fast = 5 Hz). Counts
above 3 are unreliable for humans to read (you start losing count); speeds
finer than 2× ratios are unreliable to distinguish.

| # | State | Pattern | Cycle | Operator reading |
|---|---|---|---|---|
| 1 | `BOOTING` | LED solid OFF | — | "no power / very early boot" — matches the natural state before `setup()` enables the pin |
| 2 | `WAITING` | Slow heartbeat (200 ms ON, 800 ms OFF) | 1 s | "alive but not engaged — give me a command" |
| 3 | `OPERATOR_ACTION` | **Double pulse** every 2 s (100 ms ON, 100 ms OFF, 100 ms ON, 1.7 s OFF) | 2 s | "I'm capturing something from you — hold the bot still / rotate as instructed" |
| 4 | `AUTO_ACTION` | **Triple pulse** every 2 s (3 × 100 ms ON / 100 ms OFF, 1.4 s OFF) | 2 s | "I'm running an auto-procedure — don't touch the bot" |
| 5 | `OPERATIONAL_OK` | Steady ON | — | "everything good — balancing" |
| 6 | `SAFETY_HOLD` | 3-on / 1-off / 3-on / 1-off (3 × 150 ms ON, 150 ms OFF, repeat) | 1.2 s | "intervention needed but recoverable — pick me up or set me upright" |
| 7 | `FAULT` | Fast blink (100 ms ON, 100 ms OFF) | 200 ms | "something's wrong — check serial" |
| 8 | `REFUSE_TO_ARM` | **Long + short** every 3 s (700 ms ON, 200 ms OFF, 150 ms ON, 1.95 s OFF) | 3 s | SOS-ish — "operator must do something specific before I will engage" (Uno: run `c` to calibrate, or `F` to override) |

**Distinguishability check** (the test the bench validation must confirm):

- (1) vs all others: pattern 1 is OFF, every other pattern has *some* light.
- (5) vs all others: pattern 5 is the only "always ON".
- (2) vs (7): both are blinking — but 1 Hz vs 5 Hz is a clean **5× ratio**, easy.
- (3) vs (4): two pulses vs three pulses in a 2 s window — distinguishable
  for any count ≤ 3 (we deliberately stop at 3 — see §8 anti-patterns).
- (6) vs (4): both have multiple ON pulses per cycle, but (6) is *long* pulses
  (150 ms) with *equal* OFF gaps and *no long OFF tail*, while (4) is *short*
  pulses with a clear ~1.4 s OFF tail. They feel rhythmically different.
- (8) vs everything: the long-pulse + short-pulse pair is auditorily distinct
  from any other pattern.

**These specific timings are starting numbers, not contracts.** Bench
validation during U-2 / M-5 may tune the millisecond budgets (e.g. lengthen
the SOS short pulse if it visually merges with the long pulse at typical LED
brightness). The *vocabulary* — 7 distinct patterns each meaning what §2.3
says — is the contract.

---

## 4. Hardware — D13 first, OLED later

### 4.1 Common minimum: D13 LED

Every Arduino board (Uno, Mega, Teensy, the future ESP32 dev boards from
sibling doc `esp32_port_portability_2026-05-27.md`) ships with an LED wired to
pin **D13** ("BUILTIN_LED" / `LED_BUILTIN` in the Arduino core). No extra
wiring, no extra parts. **This is the only hardware the grammar requires.**

Some boards (e.g. certain ESP32 dev modules) are **active-LOW** on
`LED_BUILTIN`. The future code's `begin(pin, led_low_active)` parameter
(Section 5) inverts at the driver level so the grammar is identical at the
operator level.

### 4.2 Optional extension: OLED

A future SSD1306 0.96" OLED (I²C, shares the BNO055 bus) can show the
state name in plain words alongside the D13 blink. The mapping is just
`state_name(StatusIndicator::State)` → screen line 1. **This is an extension,
not a requirement** — the grammar is fully expressive on D13 alone, and
mandating OLED would push the Uno tier over its parts budget.

The OLED is *additive* — it does not change the LED grammar. An operator who
flashes a non-OLED bot still gets the full vocabulary on D13.

### 4.3 Anti-recommendation: RGB LED

See §8.

---

## 5. Code stub — header + skeleton

This is **not** code that lands now. It's the API contract U-2 and M-5 must
satisfy.

```cpp
// status_indicator.h (~30 LOC declaration)
#ifndef STATUS_INDICATOR_H
#define STATUS_INDICATOR_H
#include <stdint.h>

class StatusIndicator {
 public:
  enum class State : uint8_t {
    BOOTING          = 0,
    WAITING          = 1,
    OPERATOR_ACTION  = 2,
    AUTO_ACTION      = 3,
    OPERATIONAL_OK   = 4,
    SAFETY_HOLD      = 5,
    FAULT            = 6,
    REFUSE_TO_ARM    = 7,
  };

  void begin(uint8_t pin, bool led_low_active = false);
  void set_state(State s);                  // O(1), no I/O
  void update(uint32_t now_ms);             // call every loop() iteration
  State get_state() const { return state_; }

 private:
  uint8_t  pin_           = 13;
  bool     low_active_    = false;
  State    state_         = State::BOOTING;
  uint32_t state_entered_ = 0;
  void     write_(bool on);                 // honours low_active_
};
#endif
```

```cpp
// status_indicator.cpp — sketch of update() (~30-40 LOC)
void StatusIndicator::update(uint32_t now_ms) {
  uint32_t t = now_ms - state_entered_;     // ms since state entry
  bool     on = false;                      // computed per pattern
  switch (state_) {
    case State::BOOTING:         on = false; break;
    case State::WAITING:         on = (t % 1000) < 200;             break;  // heartbeat
    case State::OPERATIONAL_OK:  on = true;                          break;  // steady
    case State::FAULT:           on = (t % 200) < 100;               break;  // 5 Hz
    case State::OPERATOR_ACTION: {                                           // 2 pulses / 2 s
      uint32_t p = t % 2000;
      on = (p < 100) || (p >= 200 && p < 300);
      break;
    }
    case State::AUTO_ACTION: {                                               // 3 pulses / 2 s
      uint32_t p = t % 2000;
      on = (p < 100) || (p >= 200 && p < 300) || (p >= 400 && p < 500);
      break;
    }
    case State::SAFETY_HOLD: {                                               // 3-on / 1-off
      uint32_t p = t % 1200;
      on = (p < 150) || (p >= 300 && p < 450) || (p >= 600 && p < 750);
      break;
    }
    case State::REFUSE_TO_ARM: {                                             // long + short
      uint32_t p = t % 3000;
      on = (p < 700) || (p >= 900 && p < 1050);
      break;
    }
  }
  write_(on);
}
```

**Total estimated implementation**: ~80 LOC across `.h` + `.cpp`, of which
~40 is the `update()` switch. Fits the X-1 roadmap line "tiny code (~80 LOC
reusable)".

**Non-blocking**: `update()` is pure arithmetic + one `digitalWrite`; cheap
enough to call every `loop()` iteration. It does **not** consume Timer2 (which
`MsTimer2` owns for the PID ISR) — this is deliberate, because grabbing a
second hardware timer for a blink loop would block the Mega-tier PWM_DISCOVERY
path and is anyway unnecessary for sub-Hz patterns.

---

## 6. Integration touch points

Where the future code will call `set_state(...)` in each tier.

### 6.1 Uno (`src/applications/balancing_robot_uno/main.cpp`)

- `setup()` entry: `status.begin(LED_BUILTIN); status.set_state(BOOTING);`
- after `app.begin()`: branch on `app.cal_missing_blocks_arm()` → `REFUSE_TO_ARM`,
  else → `WAITING`.
- `loop()` serial dispatch:
  - `'c'` (start calibration via `CalibrationSession::start()`): `OPERATOR_ACTION`.
  - `'t'` (enter `TuneStage::STAGE_P` via `TuningSession`): `AUTO_ACTION`.
  - `'g'` (arm via `app.arm()`): if `app.is_armed()` → `OPERATIONAL_OK`, else
    `REFUSE_TO_ARM`.
  - `'a'` (abort): `WAITING`.
- `loop()` per-iteration mirror of `app.is_tipped()`: if true and state is
  `OPERATIONAL_OK`, set `SAFETY_HOLD`. When `is_tipped()` clears, revert.
- IMU watchdog: when `app.read_fail_count() > 10`, set `FAULT`.
- Every `loop()`: `status.update(millis());`

### 6.2 Mega (`src/main.cpp`)

- `setup()` entry: `status.begin(LED_BUILTIN); status.set_state(BOOTING);`
- after `app.begin(...)`: `WAITING` (Mega has no refuse-to-arm latch).
- After each `app.drain_state_log(...)` call, map the freshly-logged
  `BalanceAppState` to a `StatusIndicator::State`:

| `BalanceAppState` | `StatusIndicator::State` |
|---|---|
| `IDLE` | `WAITING` |
| `CAPTURE_MOUNTING` | `OPERATOR_ACTION` |
| `BOOTSTRAP` | `AUTO_ACTION` |
| `PWM_DISCOVERY` | `AUTO_ACTION` |
| `CHAR_ACT` | `AUTO_ACTION` |
| `RUN` | `OPERATIONAL_OK` |
| `HELD` | `SAFETY_HOLD` |
| `FALLEN` | `SAFETY_HOLD` |

- Sensor-failure paths (`BalanceSafety` asserts, I²C disconnects): `FAULT`.
- Every `loop()`: `status.update(millis());`

The mapping above is what the M-5 work will hard-code; it's small enough
(~10 lines) to live inline in `main.cpp` and does **not** require modifying
`BalanceApp` itself.

---

## 7. Why not just use the serial monitor?

The serial monitor is the **diagnostic** channel — it shows numbers (pitch,
PWM, gains), state-transition logs, and assertion messages. It is essential
for tuning, but it has three properties that disqualify it as the *operator-
glance* channel:

1. **It requires a tethered laptop.** An end user who has flashed the bot to
   show off at a friend's house has no monitor open. The LED is sufficient
   for first-success operation without USB.
2. **It is text — invisible across the room.** The "is it balancing yet?"
   question is answered in 250 ms by glancing at a steady-on LED; reading a
   serial line takes 1-2 s of attention and a screen in your hand.
3. **It is silent on hangs.** When the firmware hangs in an I²C loop, serial
   stops emitting. The LED grammar deliberately includes `FAULT` as a
   *fast-blink* pattern that the driver actively runs — even a hung main
   loop can be made to emit `FAULT` from a watchdog ISR (`update()` from the
   timer ISR is an option, though not the default proposed here). The LED is
   the **liveness signal** the serial channel cannot be.

**Rule of thumb**: if you can answer the question "is everything OK?" from
across the room, you don't need a serial monitor open. The 7-state grammar is
sized to that question.

---

## 8. Anti-patterns — what we explicitly rejected

### 8.1 RGB LED (e.g. WS2812 / common-cathode)

- **Extra hardware on Uno** (the smallest tier has the least board real estate
  and the tightest parts budget).
- **Colour-blind operators get a worse experience.** Red-green colour
  blindness is ~8% of males; "red = fault, green = balancing" silently fails
  for this group. The pulse-pattern grammar is colour-agnostic.
- **Slower to read at distance.** A red LED looks orange-ish at low brightness,
  and "is that orange or yellow" is a real perceptual ambiguity. Pulse
  patterns survive distance and brightness changes.

### 8.2 Multi-byte numeric codes ("blink 7 times = error 7")

- **Counts above 3 are unreliable.** Human pattern matching for blink counts
  is good up to 3 and degrades sharply above. The grammar caps at **3 pulses
  per cycle** for this reason.
- **Operators have to time-multiplex two attentions** (count blinks, then
  remember the long codebook table). Pulse-shape patterns are recognisable in
  one cycle.

### 8.3 Morse-code state codes

- Same problem as numeric codes, plus the time-budget overhead of a long
  message (S = `...`, O = `---`, several seconds per code) when the
  underlying state has already changed.

### 8.4 Static patterns reading the live `BalanceAppState` enum directly

- Tempting because it removes the mapping table in §6.2 — but it would couple
  `StatusIndicator` to the Mega-side enum and prevent reuse on Uno (where the
  enum doesn't exist). The 7-state shared enum is the abstraction the cross-
  tier reuse depends on.

---

## 9. Roadmap — when this gets implemented

This design doc is a **prerequisite for U-2 and M-5**, not a deliverable on
its own. Implementation phasing:

| Phase | Work | When |
|---|---|---|
| **X-1 (this doc)** | Fix the grammar, settle the 7 patterns, agree the API | now |
| **U-2** (Uno LED feedback) | Implement `status_indicator.{h,cpp}`, wire into Uno `main.cpp` per §6.1, bench-validate the 7 patterns at arm's length | next dispatched Uno session |
| **M-5** (Mega headless first-success) | Reuse the same `status_indicator.{h,cpp}` (the LOC budget here is zero — same file linked from both envs), wire into Mega `main.cpp` per §6.2, bench-validate | next dispatched Mega session |
| **Future** (optional) | OLED extension per §4.2 — separate findings doc, gated on operator demand | unscheduled |

**Bench-gate criterion** for U-2 and M-5 closure: an operator who has never
seen the bot before is shown the cheat-sheet (table in §3) for 30 seconds,
then the bot is run through all 7 states (in any order), and the operator
correctly names each state from across the workbench. If the operator
mis-reads any pattern, that pattern's timing is re-tuned (Section 3's "starting
numbers, not contracts" caveat) and the test is rerun. This is exactly the
kind of UX test that **cannot be done from a desk** — hence the deferral.

---

## 10. Cross-references

- **`docs/findings/esp32_port_portability_2026-05-27.md`** — sibling X-3 doc
  from the same forward-roadmap session. Both docs are honest design audits
  that fix decisions before code lands. The ESP32 port (X-3 §4) involves
  pinning the PID ISR to Core 1; the LED grammar likewise must be polled from
  `loop()` (not from the PID ISR) so the two design constraints compose
  cleanly. **An ESP32 balance bot built post-X-3 will inherit the same 7-state
  grammar verbatim** — `status_indicator` is platform-agnostic
  (pure arithmetic on `millis()`).
- **`docs/findings/ao_roadmap_post_4m14_2026-05-20.md`** — X-1 is item 1 of
  the cross-tier UX cluster; U-2 / M-5 are the implementation tickets gated
  on this doc.
- **`src/applications/balancing_robot/balance_app.h:89-108`** — Mega
  `BalanceAppState` enum (source of truth for §2.2).
- **`src/applications/balancing_robot/balance_app.cpp:896-910`** —
  `state_name()` strings (kept consistent with the LED mapping in §6.2).
- **`src/applications/balancing_robot_uno/uno_balance_app.h:127`, `:161-162`,
  `:179`** — `armed_`, `tipped_`, `cal_missing_block_arm_` flags (source of
  truth for §2.1).
- **`src/applications/balancing_robot_uno/calibration_session.cpp:73-79`** —
  cal-session entry print, marks `OPERATOR_ACTION` start.
- **`src/applications/balancing_robot_uno/tuning_session.h:32-38`** —
  `TuneStage` enum, marks `AUTO_ACTION` window (any non-IDLE stage).
- **`docs/scope.md`** — memory-tier framing this grammar harmonises across.

---

## 11. Honest scope statement

This is a **design doc only**. No source has been read for editing, no code
will land in this session, no tests will be added. The eventual `status_indicator`
module will be implemented and **bench-validated** in U-2 / M-5, where a real
human reading a real LED at a real distance is the only way to confirm the
grammar is operator-readable. Pattern timings (§3) are starting numbers; the
**vocabulary** (7 states, what each means) is the contract.

If the U-2 / M-5 bench reveals that two patterns are confusable, the fix is to
re-tune the offending pattern's timing — **not** to add a ninth pattern.
Vocabulary growth past 7 violates the "≤3 pulses per cycle, ≤2 speeds" rule
that makes the grammar human-readable in the first place.

---

*End of design doc. Single deliverable, no source touched.*
