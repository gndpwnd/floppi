# ESP32 / Teensy Portability Matrix — Mega-tier auto-features

**Date**: 2026-05-27
**Agent**: `ao-X3-esp32-portability@floppi:1` (documentation, READ-ONLY over `src/`)
**Mode**: Single new findings file. No source edits, no builds, no commits.
**Roadmap item**: **X-3** ("ESP32/Teensy portability audit") from `docs/findings/ao_roadmap_post_4m14_2026-05-20.md`.

---

## 0. TL;DR — what this doc is and is not

This is the **honest planning audit** the X-3 item asks for. The `esp32_balance` and
`teensy_balance` PlatformIO envs are *scaffolded but commented out* (see
`platformio.ini:251-261`) precisely because the Mega-tier auto-stack reaches into a
handful of AVR-specific primitives that ESP32/Teensy either lack or implement with
different semantics. **A naive `pio run -e esp32_balance` would compile, link, fail
silently in three different ways, and produce a strictly *worse* balancing
experience** than the Mega — violating the operator's "easiest for end users"
mandate. This document enumerates every AVR-specific dependency, classifies it
EASY / MEDIUM / REDESIGN, and lays out the port phasing so when ESP32 work is
prioritised the team knows exactly which dependency is a copy-paste and which is
new design work.

**Headline finding** (Section 8.1): the seemingly-obvious dependencies (MsTimer2,
EEPROM, PROGMEM) are all **MEDIUM** at most — the genuinely *hard* dependency that
a casual reader would not predict is the **PJRC `Encoder` library**. PJRC `Encoder`
is the GPIO-ISR-based quadrature decoder on the Mega; ESP32 ships with a dedicated
hardware **PCNT (pulse-counter) peripheral** that is *better* than the AVR ISR
backend, but PJRC's library does not target it. Naively dropping in the Arduino
GPIO-ISR fallback on ESP32 produces a backend that **drops ticks at high motor
RPM** because GPIO-ISR latency on ESP32 (~3-5 µs per edge, occasionally tens of
µs when WiFi is active) collides with the wheel-encoder edge rate. The encoder
port is therefore a **REDESIGN**, not a swap — and the cleaner architectural move
is a `wheel_encoder` backend selector (`#if WHEEL_ENCODER_BACKEND_PCNT`) parallel
to the existing PJRC / MOCK / STUB split (`src/sensors/wheel_encoder.cpp:38-46`).

**Verdict at a glance**:

| # | Category | Verdict | Lift |
|---|---|---|---|
| 1 | EEPROM direct API | **EASY** (HAL already exists) | 1 day |
| 2 | `MsTimer2` 5 ms PID tick | **MEDIUM** | 2-3 days |
| 3 | `ATOMIC_BLOCK` / `util/atomic.h` | **MEDIUM** | 1-2 days |
| 4 | `F()` / `PROGMEM` / `pgm_read_byte` | **EASY** (auto-no-op on non-AVR; existing shim works) | 0 days |
| 5 | `avr/io.h`, `util/delay.h`, raw AVR headers | **EASY** (already guarded) | 0 days |
| 6 | Hardcoded pin map (Mega INT pins 2/3/18/19) | **MEDIUM** | 1 day |
| 7 | PJRC `Encoder` library (wheel_encoder ISRs) | **REDESIGN** | 1-2 weeks |
| 8 | `Adafruit_BNO055` library on Arduino-ESP32 core | **EASY** (already known to work) | 0 days |

Total honest estimate: **3-4 weeks of focused work**, dominated by the encoder
redesign and bench-validation cycles. The "scaffolded esp32_balance" comment in
`platformio.ini` understates this — the comment lists two items, the real list is
seven, and one is non-trivial.

---

## 1. Audit methodology

Searches were run read-only over `src/**` from project root:

| Dependency | Grep pattern | Hits | Files |
|---|---|---:|---|
| MsTimer2 | `\bMsTimer2\b` | 21 lines | 6 files (2 main.cpp, 2 balance_app, 2 docs/comments) |
| ATOMIC_BLOCK | `ATOMIC_BLOCK\|util/atomic` | 23 lines | 4 files (2 balance_app.cpp, 2 wheel_encoder header comments) |
| direct EEPROM API | `EEPROM\.\(get\|put\|read\|write\|begin\|commit\|length\)` | 22 lines | 5 files |
| PJRC Encoder | `Encoder\.h\|PJRC.*Encoder` | 20+ lines | 4 files (wheel_encoder.{cpp,h}, balance_app.cpp, main.cpp) |
| `F()` macro / PROGMEM | `F("\|PROGMEM\|pgm_read` | 190 lines | 9 files (main.cpp, balance_app.{cpp,h}, all 5 Uno program files, bno055.cpp) |
| AVR headers (`avr/*`, `util/*`) | `avr/io\.h\|util/atomic\.h\|util/delay\.h\|avr/pgmspace` | 3 lines | 2 files (`balance_app.cpp:45-46`, `uno_balance_app.cpp:28`) |

Each is treated as one category below.

---

## 2. Category 1 — Direct `<EEPROM.h>` API usage  →  **EASY**

### 2.1 Occurrences

Uno-side `tune_storage` is the only remaining direct-EEPROM caller in the
Mega-tier-relevant tree; the Mega-side path already routes through the HAL.

| File | Lines | Notes |
|---|---|---|
| `src/applications/balancing_robot_uno/tune_storage.cpp` | 94, 101, 108, 157, 161, 166, 167, 184, 189 | Wrapped in `#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)` (line 17) — already a Uno-only build path; not in scope for the Mega-tier port. |
| `src/storage/persistent_storage_avr.cpp` | 35, 51-52, 70-72, 92 | AVR backend of the HAL — exactly where direct EEPROM calls belong. |
| `src/storage/persistent_storage_teensy.cpp` | 35, 54 | Teensy backend, also a HAL leaf. |
| `src/config/calibration_storage.cpp` (Mega-side) | — | Already routes through `ps::` HAL (header comment line 5-10 explicitly notes this). |

### 2.2 Verdict — EASY

The `persistent_storage` HAL already exists with **three back-ends** —
`persistent_storage_avr.cpp`, `persistent_storage_esp32.cpp`,
`persistent_storage_teensy.cpp` — selected at compile time by
`#ifdef ARDUINO_ARCH_*`. The ESP32 backend uses `Preferences` (NVS) directly
(`persistent_storage_esp32.cpp:1-90`) and avoids the deprecated `<EEPROM.h>`
wrapper that was the source of Known-Issue KI-1 in `docs/scope.md`. The Mega-tier
calling code (`calibration_storage.cpp`, `main.cpp` mounting/actuator slots) is
already platform-agnostic.

### 2.3 ESP32 equivalent

Already implemented: `ps::begin(capacity_hint)` → `Preferences::begin("auto_orient", false)` → RAM-mirror →
`ps::commit()` flushes via `Preferences::putBytes`. Capacity is parameterised at
init.

### 2.4 Prior art (cross-project)

`flight_controller/lib/CalibrationStorage/calibration_storage.{h,cpp}` (vendored
from AO 2026-05-26, see `flight_controller/docs/findings/calibration_telemetry_verification_2026-05-22.md`)
proves the AO HAL pattern compiles and runs on ESP32 — same byte layout, same CRC.
The FC port is **the working precedent** for AO's storage port.

### 2.5 Caveats

- NVS commit cost is non-trivial (~10-20 ms per commit on ESP32). Calling
  `ps::commit()` per byte would degrade autocal UX. Batching is already the API
  contract (`persistent_storage.h:23-28`); the calling sites obey it.
- `Preferences::begin` requires a partition table that includes NVS; the
  default `esp32dev` board ships with one, but custom partition CSVs (e.g.
  larger OTA slots) must keep an NVS partition.

---

## 3. Category 2 — `MsTimer2` for the 5 ms PID ISR  →  **MEDIUM**

### 3.1 Occurrences

| File | Lines | Role |
|---|---|---|
| `src/main.cpp` | 66, 484, 559, 568, 746, 748, 749, 834 | Mega-tier: `MsTimer2::set(5, pid_tick_isr); MsTimer2::start();` at `setup()` (746-749). |
| `src/applications/balancing_robot_uno/main.cpp` | 27, 122, 301, 302 | Uno-tier: same pattern, `MsTimer2::set(PID_SAMPLE_MS, pid_isr);` |
| `src/applications/balancing_robot/balance_app.h` | 513 | API doc reference (no code dep). |
| `src/applications/balancing_robot/balance_app.cpp` | 1825 | Comment about ISR sharing — not a code dep. |
| `src/applications/balancing_robot_uno/uno_balance_app.{cpp,h}` | several | Comments only. |
| `src/applications/balancing_robot_uno/balance_constants.h` | 39 | Comment only. |

**Two real dependency sites**, both in `setup()`: Mega `src/main.cpp:746-749` and
Uno `src/applications/balancing_robot_uno/main.cpp:301-302`.

### 3.2 Verdict — MEDIUM

`MsTimer2` is an AVR-only library (uses Timer2 hardware on ATmega) — does not
compile on ESP32 or Teensy. Both platforms have *functional* equivalents but with
different APIs:

| Platform | Equivalent | Constructor cost |
|---|---|---|
| ESP32 | `esp_timer_create()` + `esp_timer_start_periodic(handle, 5000)` (µs) | ~10 LOC |
| Teensy 4.x | `IntervalTimer t; t.begin(pid_tick_isr, 5000)` | ~3 LOC |

The header comment in `platformio.ini:240-244` already names the porting plan but
**understates the design question**: ESP32 timers can run on either CPU and can
run from either the high-resolution timer service or a hardware timer. The
defaults differ in deterministic jitter (`esp_timer` callbacks queue through a
high-priority FreeRTOS task; `hw_timer` calls from IRAM context). For a control
loop the hw_timer path is the correct choice — and that *does* require attention
to ISR-safe code paths.

### 3.3 ESP32 equivalent (proposed)

```c
// src/platform/timer_periodic_esp32.cpp (new — sketch only, no code lands)
#include "esp_timer.h"
static esp_timer_handle_t pid_timer;
static void pid_tick_isr_wrapper(void* arg) { ((void(*)())arg)(); }

bool timer_periodic_start(uint32_t period_ms, void (*cb)()) {
  esp_timer_create_args_t args = {
    .callback = pid_tick_isr_wrapper,
    .arg = (void*)cb,
    .dispatch_method = ESP_TIMER_TASK,   // OR ESP_TIMER_ISR if available
    .name = "pid_tick",
  };
  if (esp_timer_create(&args, &pid_timer) != ESP_OK) return false;
  return esp_timer_start_periodic(pid_timer, period_ms * 1000) == ESP_OK;
}
```

…with a parallel `timer_periodic_avr.cpp` that delegates to `MsTimer2` and a
`timer_periodic.h` interface. Call sites become
`timer_periodic_start(5, pid_tick_isr)`.

### 3.4 Caveats — the non-obvious bit

- **WiFi interference**. If WiFi is active and the PID ISR runs on Core 0 (the
  protocol core), 5 ms jitter can spike to 1-2 ms. The Mega has none of this
  because it has no WiFi. ESP32 PID work should pin the timer task to Core 1.
- **Sample-period derivation**. `kDefaultPidSampleMs = 5` (`balance_app.cpp:87`)
  was chosen against AVR/MsTimer2 hardware. The `PlantIdentifier` and PID
  derivative term are *sample-period-aware* (period appears in the K_motor
  derivation: K = Δω/τ/pwm, where τ = sample period). Changing the timer to
  something with measurable jitter changes the effective τ; the BOOTSTRAP path
  may need to re-measure rather than trust the nominal period.

### 3.5 Lift estimate

2-3 days: introduce `timer_periodic.h`/`timer_periodic_*.cpp` HAL, port the
two `MsTimer2::set` call sites, bench-verify jitter at 200 Hz under WiFi load.

---

## 4. Category 3 — `ATOMIC_BLOCK` / `util/atomic.h`  →  **MEDIUM**

### 4.1 Occurrences

| File | Lines | Use |
|---|---|---|
| `src/applications/balancing_robot/balance_app.cpp` | 45-46 (include), 933, 1853, 1859, 1952 | Mega-tier: ISR-vs-loop float-sharing critical sections (AO-FIN-07 — these were *added* to fix torn-read bugs). |
| `src/applications/balancing_robot_uno/uno_balance_app.cpp` | 28 (include), 30-32 (no-op shim), 168, 181, 191, 204, 215, 240, 278, 300, 308, 336 | Uno-tier: same pattern, 11 critical sections. |
| `src/sensors/wheel_encoder.{cpp,h}` | comment-only | PJRC `Encoder::read()` does its own internal `ATOMIC_BLOCK` equivalent — we don't call it ourselves. |
| `src/applications/balancing_robot/balance_app.h` | 867 | API doc, no dep. |

**Real dependency sites**: ~15 critical sections across the two balance_apps,
each guarding a multi-byte float store that is shared between `loop()` and the
5 ms PID ISR.

### 4.2 The existing non-AVR shim

Both `balance_app.cpp` and `uno_balance_app.cpp` already define a no-op
`#define ATOMIC_BLOCK(x)` on non-AVR (`balance_app.cpp:54-62`,
`uno_balance_app.cpp:30-32`). This **compiles cleanly on ESP32** but is **wrong
for the wrong reason**: the comment says "On 32-bit platforms float reads are
naturally atomic" which is *true for the read*, but **does not address the
write-side**:

- On a single-core MCU, an aligned 32-bit float store *is* a single
  instruction → naturally atomic. Mega passes; native tests pass; Teensy single-
  core passes.
- **On a dual-core ESP32**, the PID ISR (Core 0) and `loop()` (Core 1) can race
  on a non-atomic struct (e.g. two consecutive floats: one core writes both,
  the other core can observe one updated and one stale). The shim's no-op is
  *unsafe* on ESP32 the moment the PID timer is on a different core from `loop()`.

### 4.3 Verdict — MEDIUM

The fix is a one-line shim swap, but understanding *why* it must change is
load-bearing — this is the kind of silent breakage the "honest planning doc"
exists to catch. ESP32 needs FreeRTOS portMUX:

```c
// In a new src/platform/atomic_esp32.h (sketch):
#include "freertos/FreeRTOS.h"
static portMUX_TYPE g_atomic_mux = portMUX_INITIALIZER_UNLOCKED;
#define ATOMIC_BLOCK(x)        for (int __i=(portENTER_CRITICAL(&g_atomic_mux),0); !__i; __i=(portEXIT_CRITICAL(&g_atomic_mux),1))
#define ATOMIC_RESTORESTATE    /* unused */
```

…OR (more idiomatic for ESP32) wrap each ISR-shared float in `std::atomic<float>`
with `memory_order_relaxed`. The cleaner refactor is the latter, but it touches
~20 member declarations across two files — bigger diff, smaller risk.

### 4.4 Caveats

- The Uno-side `ATOMIC_BLOCK` count (11 sites) was driven by 8-bit AVR's 4-byte
  float-tear vulnerability. On 32-bit single-core (Teensy 4), some are
  unnecessary; on dual-core ESP32, all are necessary. **The 11 sites are
  not over-engineering — they will all need to stay (with a different macro
  expansion) on ESP32.**
- If the PID ISR is pinned to the same core as `loop()` (Section 3.4's caveat),
  the dual-core race vanishes and the existing no-op shim *would* be correct.
  But pinning costs WiFi throughput. Recommend portMUX — it's cheap (a
  spinlock around 20-50 ns each enter/exit) and explicit about intent.

### 4.5 Lift estimate

1-2 days: introduce a one-file `atomic_compat.h` HAL with three branches (AVR
real, ESP32 portMUX, native/Teensy no-op), update the two `#ifndef ATOMIC_BLOCK`
guards in `balance_app.cpp` / `uno_balance_app.cpp` to include it instead.

---

## 5. Category 4 — `F()` macro, `PROGMEM`, `pgm_read_byte`  →  **EASY**

### 5.1 Occurrences

190 lines across 9 files; densest in `balance_app.cpp`, `main.cpp`, and all five
Uno-program `.cpp` files (~93 occurrences on the Uno side). Each `F("string")`
saves SRAM by keeping the literal in flash; `PROGMEM` + `pgm_read_byte` enable
flash-resident lookup tables (used in `balance_app.cpp:41-46` for one table).

### 5.2 Verdict — EASY

The existing non-AVR shim is **already correct** for ESP32:

```c
// src/applications/balancing_robot/balance_app.cpp:47-53
#ifndef PROGMEM
  #define PROGMEM
#endif
#ifndef pgm_read_byte
  #define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#endif
```

…and the Arduino-ESP32 core defines `F(x)` as a passthrough so SRAM-vs-flash is
moot on a 320 KB-SRAM ESP32. **No port work required**.

### 5.3 Caveats

- One small footgun: the ESP32 Arduino core defines `F()` as `(x)` (no
  `FlashStringHelper` wrapper), so any code that passes the result of `F()` into
  a function expecting `const __FlashStringHelper*` (uncommon, but Adafruit
  occasionally does this) will fail to compile. Quick grep over the AO tree
  shows no such usage.
- The lookup-table use in `balance_app.cpp` is small (~few hundred bytes); it
  simply lands in DRAM on ESP32 with no measurable impact.

### 5.4 Lift estimate

0 days. The existing shim handles it.

---

## 6. Category 5 — Raw AVR headers (`avr/io.h`, `util/delay.h`, `avr/pgmspace.h`)  →  **EASY**

### 6.1 Occurrences

Only three lines total:

| File | Line | Header |
|---|---|---|
| `src/applications/balancing_robot/balance_app.cpp` | 45 | `<avr/pgmspace.h>` |
| `src/applications/balancing_robot/balance_app.cpp` | 46 | `<util/atomic.h>` |
| `src/applications/balancing_robot_uno/uno_balance_app.cpp` | 28 | `<util/atomic.h>` |

All three are inside `#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)` guards
(see `balance_app.cpp:44`). On ESP32/Teensy the headers are not included and the
fallback shims (Sections 4.3, 5.2) apply.

### 6.2 Verdict — EASY

Already guarded. No code work.

---

## 7. Category 6 — Hardcoded pin map (Mega INT pins 2/3/18/19)  →  **MEDIUM**

### 7.1 Occurrences

| File | Lines | Pin commitments |
|---|---|---|
| `src/main.cpp` | 70 (BTN=4), 89-93 (motor pins 5/6/7/8/9/10), 142 (`enc_left(18,19)`), 143 (`enc_right(2,3)`) | Mega-tier — INT pins 2/3/18/19 for quadrature encoders, Timer0/Timer2 PWM pins for L298N. |
| `src/applications/balancing_robot_uno/main.cpp` | 102-106 (motor pins) | Uno-tier — fewer commitments because no encoders. |
| `src/config/pins.h` | full file | Already has `#elif defined(ESP32)` and `#elif defined(TEENSY31)` branches for BNO085 / GPS UART pin selection — the pattern exists. |

### 7.2 Verdict — MEDIUM

Two distinct issues:

**Issue (a) — L298N PWM pins.** ESP32 PWM does not use Timer0/Timer2; it uses
LEDC channels (`ledcAttachPin(pin, channel)` + `ledcWrite(channel, duty)`). The
L298N driver (`src/actuators/l298n_motor_driver.cpp`, not opened in this audit
but referenced from `main.cpp:99`) almost certainly uses `analogWrite()`, which
the Arduino-ESP32 core *does* implement as a LEDC shim — so PWM probably "just
works" at default 5 kHz / 8-bit resolution. **But** the 5 kHz default produces
audible whine; ESP32 ports of motor drivers typically tune LEDC to 20 kHz / 10-bit
explicitly. This is a quality issue, not a correctness one — exactly the
"quietly degraded experience" the X-3 mandate warns about.

**Issue (b) — Quadrature encoder pins.** Mega uses external-INT pins
(`INT4=18, INT5=19, INT0=2, INT1=3`). ESP32 has 32 GPIOs that can attach to
interrupts (any pin), but the GPIO-ISR path is fundamentally not the right
backend — see Section 8. **The pin choice itself is irrelevant on ESP32; the
backend choice dominates.**

### 7.3 ESP32 equivalent (proposed)

Extend `src/config/pins.h` (which already has `#elif defined(ESP32)` precedent)
with a balance-bot section:

```c
#elif defined(ESP32)
  #define BALANCE_BTN_PIN       0    // BOOT button on most dev boards
  #define MOTOR_ENA_PIN        25
  #define MOTOR_IN1_PIN        26
  ...
  #define ENC_LEFT_A_PIN       34   // input-only is fine for PCNT
  #define ENC_LEFT_B_PIN       35
  #define ENC_RIGHT_A_PIN      32
  #define ENC_RIGHT_B_PIN      33
```

…and replace the literal `18` / `19` / `2` / `3` in `src/main.cpp:142-143` with
the macro names. This is a sub-day refactor on the AO side; the ESP32 board
pinout reservation is a wiring decision for whoever ports.

### 7.4 Lift estimate

1 day: pin macros, replace literals, document the recommended ESP32 dev-board
wiring in `docs/applications/balancing_robot/HARDWARE.md` (or equivalent).

---

## 8. Category 7 — PJRC `Encoder` library (wheel_encoder ISRs)  →  **REDESIGN**

### 8.1 Occurrences and architecture today

`src/sensors/wheel_encoder.cpp:38-46` already has a backend selector:

```c
#if defined(NATIVE_TEST)
  #define WHEEL_ENCODER_BACKEND_MOCK 1
#elif defined(USE_WHEEL_ENCODERS)
  #include <Arduino.h>
  #include <Encoder.h>
  #define WHEEL_ENCODER_BACKEND_PJRC 1
#else
  #define WHEEL_ENCODER_BACKEND_STUB 1
#endif
```

PJRC `Encoder` *does* compile on ESP32 — but its ESP32 backend uses
`attachInterrupt(pin, ISR, CHANGE)` (the Arduino GPIO-interrupt path). That works
at low rates and **drops ticks at high rates / under WiFi load** for two reasons:

1. **GPIO-ISR latency**: an ESP32 GPIO interrupt costs ~3-5 µs to enter the ISR
   in the best case, and can be delayed by tens of µs when the WiFi or Bluetooth
   stack holds priority. A 1000-tick/rev encoder on a wheel spinning at 5 rev/s
   produces edges every 200 µs — close to the latency floor.
2. **WiFi/dual-core preemption**: the GPIO ISR is by default not pinned to a
   core. Bursts of WiFi packet handling can starve the ISR for milliseconds.

The Mega ATmega2560 has no WiFi, runs the encoder ISRs directly from the
external-INT hardware in ~0.5 µs, and never starves them. **The Mega's encoder
backend is — surprisingly — strictly better than the naive ESP32 GPIO-ISR
equivalent.** This is the headline finding of the audit and the kind of regression
a casual port would silently produce.

### 8.2 Verdict — REDESIGN

The right ESP32 backend is the **PCNT (pulse counter) peripheral**: dedicated
hardware up/down counters with quadrature mode built in, no CPU cycles required,
WiFi-immune by design. The Arduino-ESP32 core wraps PCNT in
`<driver/pulse_cnt.h>` (ESP-IDF v5) or `<driver/pcnt.h>` (ESP-IDF v4); both
expose a counter that can be configured for quadrature decoding from two GPIOs.

The redesign adds a fourth branch to the existing `wheel_encoder.cpp` backend
selector:

```c
#elif defined(USE_WHEEL_ENCODERS) && defined(ARDUINO_ARCH_ESP32)
  #include <driver/pulse_cnt.h>
  #define WHEEL_ENCODER_BACKEND_PCNT 1
#elif defined(USE_WHEEL_ENCODERS)
  ...PJRC branch as today...
#endif
```

…and a parallel implementation of the four backend helpers
(`pjrc_alloc`, `pjrc_read`, `pjrc_reset`, `pjrc_dispose`) for PCNT. The public
`WheelEncoder` class API does not change — `read_ticks()`, `reset_ticks()`,
`velocity_dps()` all keep their signatures. The ATOMIC_BLOCK question goes away
because PCNT counters are atomically read in hardware.

### 8.3 Design proposal

| Step | Work |
|---|---|
| 1 | Add `WHEEL_ENCODER_BACKEND_PCNT` branch to `wheel_encoder.cpp:38-46`. |
| 2 | Implement `pcnt_unit_handle_t` allocation in `WheelEncoder::begin()`. |
| 3 | `read_ticks()` → `pcnt_unit_get_count()`. |
| 4 | Decide glitch-filter ns (PCNT supports `pcnt_chan_set_level_action` to debounce). |
| 5 | Add a unit test on the `MOCK` backend that the velocity-window math still matches at simulated ESP32 tick rates (~5 kHz). |
| 6 | Bench-validate: spin wheels manually with WiFi off and on; confirm tick count is monotone and matches optical reference. |

### 8.4 Caveats

- **ESP32 PCNT unit count**: ESP32 (classic) has 8 PCNT units. ESP32-S3 has 4.
  Two wheels = two units = fine on both.
- **Quadrature mode subtleties**: PCNT's quadrature decode requires
  `PCNT_CHANNEL_LEVEL_ACTION_KEEP` on the level pin and edge actions on the
  count pin. Getting this configuration wrong silently inverts direction or
  halves the resolution. The redesign should include a self-test on `begin()`
  that pulses the channels in a known pattern and verifies the count moves the
  expected direction.
- **Teensy 4.x has no PCNT** — but it has the `<QuadEncoder.h>` library that
  uses the four hardware quadrature decoders on the IMXRT chip, which is the
  equivalent. So Teensy gets a fifth backend, not a fallback to PJRC GPIO-ISR.

### 8.5 Lift estimate

**1-2 weeks**, including the bench validation cycle. This is the dominant
single line item in the port budget.

---

## 9. Category 8 — `Adafruit_BNO055` library on Arduino-ESP32  →  **EASY**

### 9.1 Occurrences

`src/sensors/bno055.cpp` — wraps `<Adafruit_BNO055.h>`. Already known to work on
ESP32 from the `flight_controller/` project (which runs IMU calibration on
ESP32 today — see `flight_controller/src/imu.cpp:23`).

### 9.2 Verdict — EASY

The `Adafruit_BNO055` and `Adafruit_BNO08x` libraries both target ESP32 via the
Arduino-ESP32 core. I²C pins move to ESP32 defaults (`SDA=21, SCL=22` on most
dev boards); the BNO055 driver itself is unaware. The pin selection lands in
`pins.h` (Section 7).

### 9.3 Caveats

- The Wave-6 Uno BNO085 build error (`#error` in
  `src/applications/balancing_robot_uno/main.cpp` per `docs/scope.md` line 98)
  is Uno-specific; on ESP32 the BNO085 driver footprint is irrelevant.

---

## 10. Cross-references

- **`docs/scope.md` (memory-tier framing)** — ESP32 belongs in its own tier:
  generous flash + RAM like Mega, **plus WiFi** (which Mega lacks) and **dual
  cores** (which Mega lacks). The scope doc currently lists `esp32_balance` as
  *scaffolded* (line 76) and `mega_balance` as the home of the universal stack.
  This audit recommends the ESP32 port **inherit the Mega-tier auto-features**
  rather than carve out a new tier — but with a clear understanding that the
  port is a **superset** (Mega-tier + WiFi telemetry) once these 8 categories
  land, not a sidegrade.
- **`platformio.ini:240-261`** — the scaffolded `esp32_balance` / `teensy_balance`
  envs are commented out with a 2-bullet porting note that names only MsTimer2
  and `pgm_read_byte`. **This audit expands that to 7 real items.** A future
  patch to uncomment those envs should be paired with phases A-E below; the
  comment block should also be updated to reference this doc.
- **`flight_controller/lib/CalibrationStorage/` (2026-05-26)** — direct
  precedent for the AO storage HAL port: same byte layout, same CRC, already
  exercised on ESP32 hardware. Section 2.4 details this.
- **`flight_controller/src/filters.cpp` (Madgwick, biquad)** — verified to work
  on ESP32 via `tests/native/`. The AO `OrientationSensor` interface is
  decoupled from any AVR-specific code; the AO Madgwick path will port the same
  way. **AO can reuse the FC project's ESP32 validation experience verbatim.**
- **`flight_controller/docs/findings/esp32-fc-feasibility.md`** — separate FC
  ESP32 feasibility analysis. Worth re-reading before kicking off the AO port
  for general ESP32-vs-AVR gotchas (memory map, interrupt priorities, etc.).
- **The X-3 roadmap item** (`docs/findings/ao_roadmap_post_4m14_2026-05-20.md`,
  search for "esp32") — this doc is the X-3 deliverable.

---

## 11. The user mandate — and why this doc is part of it

The operator-facing principle is **"easiest for end users"**. Translated into
port constraints:

1. **The ESP32 build must auto-discover its hardware just like the Mega.** A
   user who reads "ESP32 build supports auto-PID" must get the same BOOTSTRAP +
   K-cross-check + analytical gain auto-derivation that the Mega offers — not a
   version with disabled auto-tune.
2. **A silently-worse experience is worse than no port.** Specifically: a build
   that *appears* to enable wheel encoders but drops ticks under WiFi load
   (Section 8.1) would degrade the position-loop accuracy without surfacing any
   error — the user would observe drift and blame the math.
3. **Documentation drift is a port failure mode.** If `platformio.ini` ships
   uncommented ESP32 envs while `docs/scope.md` still says "Mega-only", a user
   will run them and hit any of the seven gotchas without warning. **The phase
   plan below sequences code+docs together for exactly this reason.**

Translated into the verdict scheme: **REDESIGN items must land before any env is
uncommented**. EASY items can land alongside or after. MEDIUM items need a
working bench validation before the env is advertised in `docs/scope.md`.

---

## 12. Roadmap — five-phase ESP32 port

Sequencing rationale: build the no-risk shim infrastructure first so subsequent
phases have somewhere to land; defer the encoder redesign to last so the rest
of the work can be incremental and reversible.

### Phase A — Shim infrastructure (1-2 days)

- Introduce `src/platform/timer_periodic.h` (interface) + `timer_periodic_avr.cpp`
  (`MsTimer2` delegate) + a stub `timer_periodic_esp32.cpp` that just compiles.
- Introduce `src/platform/atomic_compat.h` with three branches (AVR
  `<util/atomic.h>` real / ESP32 portMUX / native+Teensy no-op).
- Replace the inline `#ifndef ATOMIC_BLOCK` guards in `balance_app.cpp:54-62`
  and `uno_balance_app.cpp:30-32` with `#include "platform/atomic_compat.h"`.
- **No behaviour change on Mega.** Mega tests / Uno tests must still pass
  identically. This phase is pure refactor.
- **Exit criterion**: `mega_balance` + `arduino_uno_minimal` +
  `arduino_uno_tuning` + `native_test` all still build and pass tests.

### Phase B — Storage HAL re-validation on ESP32 (1-2 days)

- The HAL already exists. This phase is *verification*, not implementation:
  spin up a throwaway sketch on ESP32 that calls `ps::begin / write / commit /
  read` and prove the round-trip survives a reset. Reuse the
  `flight_controller/lib/CalibrationStorage/` byte layout as the test fixture.
- Add a unit test under `tests/` (host build) that exercises the
  `persistent_storage_esp32.cpp` RAM-mirror logic with a stub `Preferences`.
- **Exit criterion**: a `pio test`-runnable proof the ESP32 backend's
  byte-addressing and commit semantics match the AVR backend.

### Phase C — Timing port (2-3 days)

- Implement `timer_periodic_esp32.cpp` against `esp_timer` (Section 3.3).
- Replace the two `MsTimer2::set` call sites in `src/main.cpp:748-749` and
  `src/applications/balancing_robot_uno/main.cpp:301-302` with
  `timer_periodic_start(5, pid_tick_isr)`. **Uno path is unchanged** because
  the AVR backend still delegates to `MsTimer2`.
- Pin selection for the ESP32 timer's CPU core (Section 3.4): pin to Core 1 to
  avoid WiFi jitter.
- **Exit criterion**: ESP32 build runs the PID ISR at 200 Hz ±0.5 ms jitter
  under WiFi-active load. Bench-measured, not assumed.

### Phase D — Pin map + L298N PWM quality (1 day)

- Extend `src/config/pins.h` with the balance-bot ESP32 section (Section 7.3).
- Replace pin literals in `src/main.cpp:142-143` and `:89-93` with the macros.
- Audit `src/actuators/l298n_motor_driver.cpp` (not opened in this audit) for
  `analogWrite()` calls; on ESP32 these should explicitly set up LEDC at
  20 kHz / 10-bit to silence motor whine.
- **Exit criterion**: motors drive smoothly without audible whine; PWM 0-255
  range maps to 0-100% duty linearly.

### Phase E — Wheel encoder redesign (1-2 weeks)

- Implement `WHEEL_ENCODER_BACKEND_PCNT` branch (Section 8.3).
- Bench-validate: spin wheels manually at known RPM, compare PCNT count to
  optical reference, repeat under WiFi-active load.
- Update `wheel_encoder.h` doxygen to document the ESP32-PCNT and
  Teensy-QuadEncoder backends alongside the existing PJRC notes.
- **Exit criterion**: a 100 % WiFi-load bench run shows zero dropped ticks
  over 60 seconds at 5 rev/s. (Mega baseline: zero dropped ticks. ESP32 must
  match, not be "close to.")

### Phase F — Documentation + env uncomment (0.5 day)

- Uncomment `[env:esp32_balance]` in `platformio.ini`.
- Update the comment block at `platformio.ini:240-244` to point to this doc
  and to the (then-current) phase-completion findings file.
- Add an entry to `docs/scope.md` line ~76 (Build environments table)
  upgrading `esp32_balance` from "scaffolded" to "active".
- Add an `esp32_balance_port_landed_2026-MM-DD.md` findings doc that records
  the bench-validation numbers from Phases C and E so future contributors can
  see what passed.
- **Exit criterion**: a fresh-checkout user runs
  `pio run -e esp32_balance -t upload` and gets a working balance bot whose
  auto-features behave identically to `mega_balance`.

---

## 13. Honest cost summary

| Phase | Lift | Risk |
|---|---|---|
| A — Shim infrastructure | 1-2 days | Low — pure refactor, gated by existing tests. |
| B — Storage re-validation | 1-2 days | Low — HAL exists; this is just exercising it. |
| C — Timing port | 2-3 days | Medium — WiFi jitter is the only unknown; mitigation is well-understood. |
| D — Pin map + PWM quality | 1 day | Low — pure config, plus a known LEDC idiom. |
| E — Encoder redesign | 1-2 weeks | **High** — new peripheral, new self-tests, bench validation cycle. |
| F — Docs + env uncomment | 0.5 day | Low — paperwork. |
| **Total** | **3-4 weeks** | Dominated by Phase E. |

**Recommendation**: do not interleave the ESP32 port with active Mega-tier
feature development. The port is small enough to do end-to-end in a single
focused sprint, and large enough that splitting it across sessions will leave
the codebase in a half-shimmed state where contributors can't tell which file
expects which platform. **Schedule the port; don't drip-feed it.**

---

## 14. Out-of-scope (deliberately not addressed here)

- **WiFi telemetry / dashboard.** Once the ESP32 port lands, WiFi auto-discovery
  + browser dashboard becomes worthwhile — but that is X-4 / X-5 territory,
  not X-3. Mention only: the dashboard pattern is already vendored in the
  sister `flight_controller/` project; AO would reuse the same flag cascade
  (`USE_WIFI → USE_WEB_SERVER + USE_API_SERVER`).
- **OTA updates.** Same — depends on Phase F landing first.
- **Teensy port.** Teensy 4.x avoids most of the ESP32 gotchas
  (single-core-ish from the ISR's perspective, real PJRC EEPROM emulation, no
  WiFi). A Teensy port off the same shim infrastructure (Phase A) is probably
  half the work of the ESP32 port. Not scoped here; deserves its own findings
  doc when prioritised.
- **Source code changes.** This is a planning doc, per X-3's mandate. Code
  changes happen in subsequent sessions, gated on this audit being reviewed.

---

*End of audit. Single deliverable, no source touched.*
