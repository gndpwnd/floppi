# Wheel Encoder Integration for the Mega Balance Bot
Status: RESEARCH — supports the [strategic pivot 2026-05-19](../../docs/scope.md) (Mega-only universal stack). Wheel encoders are the unlock for full automation: PWM range auto-discovery, stiction characterisation, true position outer loop, and independent K_motor verification — none of which the IMU-only path can do robustly. This document evaluates encoder hardware + library + driver + integration architecture for the Mega-balance build.
Last updated: 2026-05-19

---

## Executive summary

The bench bot's geared DC motors almost certainly already have **hall-effect quadrature encoders built into the rear shaft** — they're standard on every yellow-TT-style hobby motor sold after ~2022 and on every N20 metal-gear motor sold today. **Recommend hall-effect motor-integrated quadrature encoders** with hand-written 2-ISR-per-channel decoding (≈400 B flash, ≈40 B RAM total for two wheels). Wire to Mega interrupt pins **18, 19 (left A/B) and 2, 3 (right A/B)** — pins 20/21 are reserved for the BNO055 I²C bus and MUST NOT be repurposed. Add `src/sensors/wheel_encoder.{h,cpp}` modeled on the existing actuator/IMU classes, give it a serial command `e` for live-tick calibration, and persist counts-per-metre + wheel radius into a new EEPROM slot `0x220`. Velocity feedback enters the existing pitch PID as an outer-loop **velocity → pitch-setpoint nudge** cascade (drop-in replacement for the option-B pitch-double-integrator from `research_imu_only_position_containment.md`). PWM-range auto-discovery becomes a new phase `4M.12` that ramps PWM with bot on a stand and reads encoder velocity to find PWM_MIN (first non-zero motion) and PWM_MAX (velocity plateau). The brute-force tuner gets realistic PWM bounds for free.

---

## 1. Encoder hardware options

| Option | Mechanism | Typical resolution at wheel | Mega interrupt cost | Unit cost (2026) | Pros | Cons | Verdict |
|---|---|---|---:|---:|---|---|---|
| **A. Hall-effect quadrature (motor-integrated)** | Magnet on motor rear shaft, two hall sensors 90° apart, AB phase output | TT motor: 8 PPR × 120:1 gearbox ≈ **960 CPR** (4× counts); N20: 7-12 PPR × 100:1 ≈ **2,800-4,800 CPR** | 2 INT pins/wheel = 4 total | $0 if already on motor; +$3-4/wheel if buying complete motor+encoder | Robust to dust/oil/vibration; survives wheel slip; reads even when motor is back-driven | Resolution drops if motor is at low gear ratio | **RECOMMENDED** |
| B. Optical quadrature (slot disk + photo-interrupter) | Slotted disk on shaft, IR LED + phototransistor pair 90° apart | 20-30 slots × gear ratio | 2 INT pins/wheel = 4 total | $2-3/wheel for HC-020K-style breakouts | Cheap; mechanical clearance forgiving | Disk alignment fiddly; dust/dirt sensitive; needs separate disk mounting | Skip — fragile relative to A |
| C. Magnetic absolute (AS5048A, MT6701) | IC reads angular position of magnet on shaft | 14-bit absolute (16,384 counts) | 0 INT (SPI / I²C) | $8-15/wheel + magnet | True absolute position, zero drift, no quadrature counting | Overkill for balance bot; SPI bus contention or extra I²C address; expensive | Skip for v1; mention as upgrade path |
| D. seesaw I²C quadrature decoder | Adafruit_seesaw chip decodes A/B in firmware, host reads count over I²C | Same as A | 0 INT (I²C) | $7-10/encoder breakout | Offloads ISR work to dedicated MCU | Adds I²C bus traffic that competes with BNO055; cost; flash overhead for Adafruit_seesaw lib (~4 KB) | Skip — wrong trade-off for Mega |

**Why option A.** The bench bot is almost certainly *already wearing* hall-effect quad encoders on the motor shafts — they're built into every modern yellow-TT motor and every N20 with the `…-EN` or `…-Encoder` suffix. The 6-pin motor connector (M+, M-, VCC, GND, A, B) is the giveaway. Zero added BOM cost in that case; just route the A/B pins to Mega interrupt pins. Even if a fresh motor pair is needed, motor+encoder combos run $5-10/each in 2026, the same price as a bare motor + a separate optical encoder + a mounting bracket.

The TT motor's 8 PPR × 120:1 gearbox × 4× quadrature counts = **960 counts per wheel revolution**. With a 65 mm diameter wheel (typical yellow-TT pairing) the circumference is ~204 mm, giving **~4.7 counts/mm** — more than enough resolution for an outer-loop velocity / position controller running at 50-200 Hz. N20 motors usually have higher resolution (2,800-4,800 CPR at the wheel) which is comfortable headroom.

**Rationale for the single recommendation.** Hall-effect motor-integrated quadrature encoders, because (a) they are likely already on the bench bot, (b) they're robust to the dust + bumps + back-drive that balance bots routinely experience, and (c) hand-written ISR decoding fits in ~400 B flash on Mega without library bloat.

---

## 2. Mega pin allocation

Mega external interrupt pins: **2, 3, 18, 19, 20, 21**. Pins 20/21 are the I²C SDA/SCL lines used by the BNO055 — UNUSABLE for encoders. That leaves 4 viable pins: 2, 3, 18, 19. Exactly enough for two quadrature wheels (2 INT pins each).

### Current Mega pin usage (verified from `src/main.cpp:85-88` and `src/config/pins.h`)

| Pin | Current use | Source |
|----:|---|---|
| 4 | PIN_BTN (operator button, INPUT_PULLUP) | `src/main.cpp:66` |
| 5 | L298N ENA (Motor A PWM) | `src/main.cpp:86` |
| 6 | L298N IN2 (Motor A dir B, swapped) | `src/main.cpp:86` |
| 7 | L298N IN1 (Motor A dir A) | `src/main.cpp:86` |
| 8 | L298N IN4 (Motor B dir B) | `src/main.cpp:87` |
| 9 | L298N IN3 (Motor B dir A, swapped) | `src/main.cpp:87` |
| 10 | L298N ENB (Motor B PWM) | `src/main.cpp:87` |
| 14-19 | Hardware Serial1/2/3 (only used if GPS attached — not on balance build) | `src/config/pins.h:55-66` |
| 20 | I²C SDA → BNO055 | Wire library (AVR-fixed) |
| 21 | I²C SCL → BNO055 | Wire library (AVR-fixed) |

### Recommended encoder pin assignment

| Signal | Pin | Mega INT vector | Rationale |
|---|---:|---:|---|
| **L_ENC_A** | 18 | INT5 | Far from L298N pins (clean digital signal); high INT priority |
| **L_ENC_B** | 19 | INT4 | Adjacent to A on the Mega header; easy 4-wire ribbon |
| **R_ENC_A** | 2 | INT0 | High INT priority; opposite side of Mega from L_ENC for noise isolation |
| **R_ENC_B** | 3 | INT1 | Adjacent to A |

**Pin-conflict audit:** Pins 18/19 are also Mega Serial1 TX/RX. The balance build does NOT use Serial1 (GPS is excluded by `[balance_src_filter]` and `pins.h:55-66` only routes those pins when `__AVR_ATmega2560__` is paired with GPS code that the balance src filter excludes). However if the operator later enables WiFi/GPS, pins 18/19 must move. RECOMMEND adding a compile-time assert in `wheel_encoder.cpp` that errors out if both `USE_WHEEL_ENCODERS` and `USE_GPS` are defined on Mega.

**Hidden gotcha discovered.** Pins 20/21 cannot be used for encoders on Mega (I²C). This restriction does NOT exist on the Uno (where I²C is on A4/A5 instead) — but the Uno only has 2 external INT pins (2, 3) anyway, so it can support at most one wheel encoder. This is one more reason the encoder feature is **Mega-only** by design.

---

## 3. Library options

| Library | LOC pulled in | Est. flash (Mega) | Est. RAM | API quality | Verdict |
|---|---:|---:|---:|---|---|
| **PJRC Encoder** ([PaulStoffregen/Encoder](https://github.com/PaulStoffregen/Encoder)) | ~600 LOC, header-heavy | ~1.6-2.0 KB for two encoders (4 INT pins) | ~40 B static + 24 B/instance | Excellent — battle-tested, 4× counting | Heaviest; pulls in global ISR table |
| **Hand-written 2-ISR-per-channel** | ~80 LOC in `wheel_encoder.cpp` | ~400 B for two encoders | ~32 B/instance (volatile int32_t tick + last AB state) | We own it, we debug it | **RECOMMENDED** |
| Adafruit_seesaw | ~4 KB for the I²C glue + chip lookup table | ~4-5 KB | ~80 B | Off-MCU decode | Skip — adds I²C contention, costs more flash than two hand-written ISRs |

**Why hand-written wins on Mega.** PJRC's library is the gold standard on Teensy where it uses dedicated hardware quadrature decoders. On AVR Mega it falls back to interrupt-on-change handlers with a state-machine table — the same logic we'd write by hand, just with extra indirection and a `noInterrupts()`/`interrupts()` pair around each read that's slightly heavier than `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. The 4× counting state machine is 16 states (4 prev × 4 curr) and can be inlined into the ISR as a small switch — well under 100 instructions per edge.

**Flash budget on Mega.** Even if we used PJRC, Mega has 256 KB and the balance build sits at ~25-30 KB. Either choice is feasible. But the hand-written version gives us full control over the `ATOMIC_BLOCK` pattern that the rest of the codebase already uses for the gyro torn-read fix (audit_code_quality_balance_stack §1).

---

## 4. Driver design — `src/sensors/wheel_encoder.{h,cpp}`

### Header API

```cpp
// src/sensors/wheel_encoder.h
#ifndef WHEEL_ENCODER_H
#define WHEEL_ENCODER_H

#include <stdint.h>

class WheelEncoder {
public:
    // pin_a, pin_b MUST both be on Mega INT pins (2, 3, 18, 19).
    // Constructor only stashes pins; begin() does the pin-mode + INT-attach.
    WheelEncoder(uint8_t pin_a, uint8_t pin_b, uint8_t isr_slot);

    bool   begin();                              // pinMode INPUT_PULLUP + attachInterrupt
    int32_t read_ticks() const;                  // ATOMIC_BLOCK read of volatile count
    void   reset_ticks();                        // ATOMIC_BLOCK zero of count

    // Velocity: forward-difference of ticks over a fixed window (default 100 ms),
    // converted to deg/s at the wheel using cpr_.
    // Maintains an internal previous-tick + previous-time snapshot.
    float  read_velocity_dps(uint32_t now_ms);

    // Linear velocity at the contact patch (uses wheel_radius_m_).
    float  read_velocity_mps(uint32_t now_ms);

    // Calibration setters (operator runs `e` command, sets these, saves to EEPROM).
    void   set_counts_per_rev(int cpr) { cpr_ = cpr; }
    void   set_wheel_radius_m(float r) { wheel_radius_m_ = r; }
    int    counts_per_rev() const   { return cpr_; }
    float  wheel_radius_m() const   { return wheel_radius_m_; }

    // ISR entry point (called from the static dispatcher table).
    void   handle_isr_a();
    void   handle_isr_b();

private:
    uint8_t pin_a_, pin_b_, isr_slot_;
    int     cpr_;
    float   wheel_radius_m_;

    volatile int32_t tick_count_;     // updated in ISR
    volatile uint8_t prev_ab_state_;  // last AB pair, low 2 bits

    // Velocity window state (loop-side only)
    int32_t  last_velocity_ticks_;
    uint32_t last_velocity_ms_;
    float    last_velocity_dps_;
};

#endif
```

### ISR pattern (ATOMIC_BLOCK + state-machine quadrature decode)

```cpp
// src/sensors/wheel_encoder.cpp
#include <Arduino.h>
#include <util/atomic.h>

// Static dispatcher table (Arduino attachInterrupt doesn't support member fns).
// Max 4 encoder slots — matches Mega's 4 usable INT pins.
static WheelEncoder* g_encoder_slots[4] = {nullptr, nullptr, nullptr, nullptr};

template <uint8_t SLOT, uint8_t CHAN>
static void enc_isr() {
    WheelEncoder* e = g_encoder_slots[SLOT];
    if (e == nullptr) return;
    if (CHAN == 0) e->handle_isr_a();
    else           e->handle_isr_b();
}

bool WheelEncoder::begin() {
    if (isr_slot_ >= 4) return false;
    g_encoder_slots[isr_slot_] = this;
    pinMode(pin_a_, INPUT_PULLUP);
    pinMode(pin_b_, INPUT_PULLUP);
    // Initial AB read so first edge doesn't compute a phantom delta.
    prev_ab_state_ = (digitalRead(pin_a_) << 1) | digitalRead(pin_b_);
    // Two INTs per encoder; CHANGE so we count quadrature edges (4×).
    // Concrete attachInterrupt calls picked per isr_slot_ at compile time:
    switch (isr_slot_) {
      case 0: attachInterrupt(digitalPinToInterrupt(pin_a_), enc_isr<0,0>, CHANGE);
              attachInterrupt(digitalPinToInterrupt(pin_b_), enc_isr<0,1>, CHANGE); break;
      case 1: attachInterrupt(digitalPinToInterrupt(pin_a_), enc_isr<1,0>, CHANGE);
              attachInterrupt(digitalPinToInterrupt(pin_b_), enc_isr<1,1>, CHANGE); break;
      // ...
    }
    return true;
}

void WheelEncoder::handle_isr_a() {
    uint8_t a = digitalRead(pin_a_);
    uint8_t b = digitalRead(pin_b_);
    uint8_t curr = (a << 1) | b;
    // Lookup table indexed by (prev << 2) | curr → delta {-1, 0, +1}
    // (Invalid transitions return 0 — likely noise / bounce.)
    static const int8_t QTAB[16] = {
       0, -1, +1,  0,
      +1,  0,  0, -1,
      -1,  0,  0, +1,
       0, +1, -1,  0
    };
    tick_count_ += QTAB[(prev_ab_state_ << 2) | curr];
    prev_ab_state_ = curr;
}
// handle_isr_b is identical — both channels see all transitions.

int32_t WheelEncoder::read_ticks() const {
    int32_t snap;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        snap = tick_count_;
    }
    return snap;
}
```

### Velocity computation — forward difference, *not* EMA

Recommendation: **forward difference over a fixed 100 ms window** with no smoothing. Reasoning:

- The outer-loop position cascade is bandwidth-limited to <1 Hz anyway (pitch-setpoint nudge can only move at SLEW_DEG_S ≤ 2°/s, see `research_imu_only_position_containment.md` §5). Smoothing the velocity feedback inside that ≤1 Hz bandwidth adds latency for zero noise benefit.
- A 100 ms window at 4.7 counts/mm gives ~47 counts at walking speed (10 cm/s) — resolution noise of 1/47 ≈ 2%, well below the gain-rounding error of K_VEL.
- EMA would couple velocity step changes to the I-term path of the inner PID via the cascade, which is one of the reasons the bot oscillates today.

If oscillation in the outer loop is observed at bench-test time, **then** add a single first-order low-pass with τ = 50-100 ms — not before.

```cpp
float WheelEncoder::read_velocity_dps(uint32_t now_ms) {
    const int32_t now_ticks = read_ticks();
    const uint32_t dt_ms = now_ms - last_velocity_ms_;
    if (dt_ms < 100) return last_velocity_dps_;   // hold the previous value
    const int32_t d_ticks = now_ticks - last_velocity_ticks_;
    last_velocity_ticks_ = now_ticks;
    last_velocity_ms_    = now_ms;
    // ticks/sec → deg/sec at the wheel
    last_velocity_dps_ = (d_ticks * 1000.0f / dt_ms) * (360.0f / cpr_);
    return last_velocity_dps_;
}
```

---

## 5. Calibration workflow

### Counts-per-metre measurement procedure

1. Boot the bot in IDLE. Operator sends serial command **`e`** — bot prints `enc_cal: L=0 R=0 — push bot exactly 1.000 m and press button`.
2. Operator picks up the bot, sets it on a marked starting line, rolls it manually along a 1-metre tape, lifts it at the end line.
3. Press the button. Bot prints `enc_cal: L=4711 R=4698 — saved` and writes CPR-per-metre + the two wheel-circumference values to EEPROM slot `0x220`.
4. Optionally repeat 2-3× and average; operator-driven, not automated, because manual rolling is a one-time setup step.

Avoid command letters `a` (emergency stop), `b` (BOOTSTRAP), `c` (calibration), `R` (reset / restart). **`e` for "encoder"** is free.

### EEPROM layout (`0x220`, 16 bytes)

```
0x220: [magic 0xAD][ver 0x01][cpm_left u16 LE][cpm_right u16 LE]
       [radius_m float LE][reserved 4 B][crc8]
```

Same pattern as `EE_MOUNT_ADDR=0x200` and `EE_ACT_ADDR=0x210` in `src/main.cpp:141-152`. CRC: XOR of bytes 0-14.

### Live tick streaming for debugging

When in encoder-cal mode, the bot streams `L_ticks=N R_ticks=M dt=Tms vel_L=X.X vel_R=Y.Y dps` once per second to Serial so the operator can verify direction and counting before committing the save.

---

## 6. Integration with balance_app

### Where velocity feedback enters

The cascade replaces the *option-B pitch-double-integrator* from `research_imu_only_position_containment.md` §5:

```
encoder.read_velocity_mps() → outer-loop velocity controller
                            → pitch-setpoint nudge (±MAX_NUDGE_DEG)
                            → pid_.set_setpoint(nudge)
                            → existing inner pitch PID → motor PWM
```

Concretely in `step_run_()` at the line `pid_.set_setpoint(0.0f)`:

```cpp
// Average left+right wheel velocity (lateral disambiguation deferred to 4M.13)
const float v_mps = 0.5f * (enc_L_.read_velocity_mps(now_ms)
                          + enc_R_.read_velocity_mps(now_ms));

// Outer-loop integrator: position drift = ∫v dt
position_m_ += v_mps * (cfg_.pid_sample_ms * 0.001f);
position_m_ *= POS_LEAK;   // 20 s washout, same time-scale as mount estimator

// Setpoint nudge: lean back when drifted forward, lean forward when drifted back
float nudge = -K_POS_M * position_m_ - K_VEL_MPS * v_mps;
if (nudge >  MAX_NUDGE_DEG) nudge =  MAX_NUDGE_DEG;
if (nudge < -MAX_NUDGE_DEG) nudge = -MAX_NUDGE_DEG;
// (slew limit identical to the IMU-only patch)
pid_.set_setpoint(nudge);
```

The integration site is `src/applications/balancing_robot/balance_app.cpp:494` (where `pid_.set_setpoint(0.0f)` lives — verified via grep). Freeze position integration during BOOTSTRAP / HELD / FALLEN exactly the same way `PlantIdentifier` is frozen (`balance_app.cpp:546-547`).

### PWM range auto-discovery — new phase 4M.12

A new state `CHAR_PWM_RANGE` (next available enum slot after `BOOTSTRAP`). Triggered by serial command **`p`** (free letter — for "PWM"):

1. Operator picks up the bot and holds it with wheels free.
2. Bot ramps PWM from 0 in steps of 5 every 200 ms.
3. At each step, read encoder velocity. **First step with `|vel| > 5 dps` → save as `PWM_MIN`**.
4. Continue ramping. **When velocity stops increasing for 3 consecutive steps → save as `PWM_MAX`**.
5. Repeat for the other motor.
6. Save `PWM_MIN_L, PWM_MIN_R, PWM_MAX_L, PWM_MAX_R` to EEPROM (extend the actuator slot `0x210`).
7. Print summary table for operator review.

Result feeds two places:
- The L298N driver's `stiction_min_pwm` becomes `max(PWM_MIN_L, PWM_MIN_R)` — measured, not the current hardcoded 80.
- The Python brute-force tuner (sibling agent) gets realistic `PWM_MIN` and `PWM_MAX` bounds and stops searching in the dead zone.

This is the **unblock** for the brute-force tuner to converge in a reasonable wall-clock time.

### K_motor verification

After BOOTSTRAP measures K_motor from gyro response, a second independent estimate is now possible: K_motor_encoder = Δ(wheel velocity)/Δ(PWM) measured during the same pulse window. If the two estimates disagree by more than ~30%, BOOTSTRAP fails with a new `failure_reason = 7` (K_disagreement) and the operator is told to check for wheel slip / mechanical binding.

---

## 7. Wiring diagram update

New doc path: **`docs/wiring_diagrams/mega_balance_with_encoders.md`**.

Should contain (the sibling doc agent owns the actual writing):

1. **Photograph or ASCII diagram** of the Mega board with annotated pins:
   - Power rail to L298N (existing)
   - L298N pins 5/6/7/8/9/10 (existing)
   - I²C 20/21 → BNO055 (existing)
   - Button pin 4 (existing)
   - **NEW**: encoder 6-pin connector breakout for each motor — VCC (5V), GND, M+ (to L298N OUTx), M- (to L298N OUTy), A → Mega 18/2, B → Mega 19/3.
2. **Voltage-level warning**: motor-integrated hall encoders on yellow-TT motors typically run at the motor's logic voltage (3.3-5V depending on board). On a 12V battery setup, the encoder VCC pin draws power from the motor's *encoder* power input — DO NOT connect encoder pins directly to the motor's 12V terminals.
3. **Pull-up clarification**: hall encoders are open-collector on some boards (need INPUT_PULLUP), push-pull on others (INPUT is fine). Default to INPUT_PULLUP in `WheelEncoder::begin()`; document the exception case.
4. **Cable length warning**: encoder lines should be <30 cm. Twisted-pair or shielded if longer; otherwise EMI from the L298N PWM lines (~490 Hz at 80% duty) injects false counts.
5. **Cross-references** to the existing `teensy_fsia6b_drone` style wiring docs.

---

## 8. Test bench (no movement required)

Three bench-test scenarios, run in order of complexity:

1. **Manual finger-rotate.** Bot powered, motors disabled (held in IDLE). Operator unplugs the motor wheels and **rotates each wheel by hand** while watching the live tick stream from the `e` command. Verify:
   - One full revolution forward = +CPR ticks (within 2-3 ticks of nominal — sloppy hand rotation introduces noise)
   - One full revolution backward = -CPR ticks
   - Both wheels match each other's CPR within ~5%
   - **No direction inversion bug** (a swapped A/B pair will produce the right magnitude but wrong sign)
2. **Bot on a stand.** Bot's wheels lifted off the ground (use a cardboard cradle, two stacked books, etc.). Send the `p` command for PWM-range auto-discovery — bot ramps both motors and the operator confirms `PWM_MIN` and `PWM_MAX` values look sensible (typically `PWM_MIN ≈ 30-50`, `PWM_MAX = 255`).
3. **Knob-mount disk (optional).** For motors not yet attached to the chassis: clamp the motor in a vise, attach a paper printed encoder disk to the shaft, hand-rotate against a marked reference. This validates the encoder before mechanical integration, useful if a motor's encoder is flaky out of the box.

Bench test #1 is the must-do; #2 is the unblock for the brute-force tuner; #3 only for debugging a suspect encoder.

---

## Recommended build order

1. **Phase 4M.10 — Wheel encoder driver.** Implement `src/sensors/wheel_encoder.{h,cpp}` (this doc's §3-4). Native test: feed synthetic AB transitions into `handle_isr_a/b` and verify tick deltas + sign correctness. Bench test: scenario §8.1 (hand-rotate).
2. **Phase 4M.11 — Serial `e` command + EEPROM calibration.** Wire `e` into the existing serial command loop in `src/main.cpp`. New EEPROM slot `0x220`. Bench test: roll bot exactly 1 m, verify CPM saves and persists across reboot.
3. **Phase 4M.12 — PWM range auto-discovery (`CHAR_PWM_RANGE` state + `p` command).** Bot-on-stand ramping procedure (this doc §6). Result feeds L298N `stiction_min_pwm` + Python tuner bounds. Bench test: scenario §8.2.
4. **Phase 4M.13 — Velocity outer loop in `step_run_`.** Replace the option-B pitch-double-integrator with encoder-based cascade (this doc §6). K_POS, K_VEL hardcoded initially; bench-tune. Operator's "balance forever" preference holds.
5. **Phase 4M.14 — K_motor cross-verification + auto-derive outer-loop gains.** BOOTSTRAP gains a second K estimate from the encoder; PlantIdentifier exposes a method to derive `K_POS, K_VEL` from the verified K_motor via pole-placement (same trick as inner Kp/Kd). Closes the universal-tune loop with no manual gains.

This is the unblocking sequence. Phases 1-3 are pure plumbing (no controller behaviour change). Phase 4 swaps the position estimator; phase 5 closes the auto-tune loop.

---

## See also

- `research_imu_only_position_containment.md` — the IMU-only alternative; option B (pitch double-integration) becomes the *fallback* on platforms without encoders (Uno, sensor-only builds).
- `research_motor_null_space_handling_detection.md` §3 — `body_heading_unit` from Phase 2.7 enables left-vs-right velocity disambiguation (Phase 4M.16 — lateral cascade).
- `dynamic_pwm_accel_learning.md` — current actuator-characterisation theory; phase 4M.12 (PWM-range auto-discovery) is the encoder-enabled upgrade.
- `bootstrap_protocol_unstable_plant.md` §6 — BOOTSTRAP's K-quality gate; phase 4M.14 adds the encoder cross-check.
- `audit_code_quality_balance_stack_2026-05-19.md` §1 — gyro torn-read ATOMIC_BLOCK pattern; the wheel encoder uses the same atomicity protection.
- `src/applications/balancing_robot/balance_app.cpp:494` — `step_run_` cascade integration point.
- `src/main.cpp:85-88` — current L298N pin assignment (motor pin conflicts audit).
- `src/config/pins.h:55-66` — Mega Serial1 pin allocation (conflict with proposed L_ENC_A/B if GPS is enabled).

---

## References

- [PaulStoffregen/Encoder — GitHub](https://github.com/PaulStoffregen/Encoder) — PJRC Encoder library, the reference quadrature decoder for Arduino/Teensy. Source for the 4× counting state machine reproduced in §4.
- [PJRC Encoder library docs](https://www.pjrc.com/teensy/td_libs_Encoder.html) — covers Mega/Uno compatibility, optimized vs polling modes.
- [DFRobot TT Geared Motor with Encoder (FIT0458)](https://wiki.dfrobot.com/fit0458/) — typical hall-effect motor-integrated quadrature encoder; 8 PPR × 120:1 = 960 CPR at the wheel.
- [Adafruit N20 DC Motor with Magnetic Encoder, 6V 1:100](https://www.adafruit.com/product/4639) — N20 reference; 7 PPR × 100:1 = 2,800 CPR at the wheel.
- [Waveshare N20 DC Gear Motor with Magnetic Hall Encoder](https://www.waveshare.com/dcgm-n20-12v-en-200rpm.htm) — alternative N20 supply, similar specs.
- [AS5048A Magnetic Rotary Encoder (Tindie breakout)](https://www.tindie.com/products/smallrobots/as5048a-encoder-board-for-robots-motor-control/) — 14-bit absolute encoder, mentioned as upgrade path.
- [noranraskin/MT6701 Arduino library](https://github.com/noranraskin/MT6701) — MT6701 magnetic absolute encoder driver, similar option to AS5048A.
- [Zbotic — Motor Encoder Interfacing: Quadrature Encoder with Arduino](https://zbotic.in/motor-encoder-interfacing-quadrature-encoder-with-arduino/) — quadrature decoding tutorial confirming the AB-90°-phase signal model used in §1.
- [Arduino Forum — Hall Effects Sensor with DC Motor used as Encoder](https://forum.arduino.cc/t/hall-effects-sensor-with-dc-motor-used-as-encoder/532648) — community report of TT-motor encoder integration patterns, pull-up requirements.
- [Bemonoc DC Encoder Gearmotor 24V 35RPM (Amazon)](https://www.amazon.com/Bemonoc-Encoder-Gearmotor-Two-Channel-Effect/dp/B0D62XLW9Z) — typical 24V hobby motor with integrated 2-channel hall encoder, $15 range.
