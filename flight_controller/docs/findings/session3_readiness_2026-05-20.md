> **STATUS UPDATE (2026-05-21):** The S3 (barometer) and S5 (GPS) READY-TO-CODE
> sections below are **SUPERSEDED** — that code has since landed. See
> `phase_w2_barometer_landed_2026-05-20.md` and `phase_w5_gps_landed_2026-05-20.md`
> for the as-shipped W2/W5 implementations (`src/barometer.cpp`, `src/gps.cpp`,
> `include/barometer.h`, `include/gps.h`, plus the baro/gps blocks in
> `web_server.cpp`). The S1 (swarm-API) and S4 (ESP32 GPIO) portions remain
> valid and unchanged. Body retained below for historical context.

# Session 3 Readiness Gate — flight_controller (2026-05-20)

> Agent: `fc-session3-readiness@flight_controller:2`
> Status: READ-ONLY synthesis. No source/config/test edits. No git commits.
> Inputs: the 5 Session-2 specs that landed today + supporting Session-1/recon docs.
> Purpose: collapse 5 independent specs into ONE executable Session-3 plan,
> flag inter-spec contradictions, and name the work that is ready for coders.

**Synthesized from (all on disk, all complete):**
- `docs/findings/swarm_api_contract_2026-05-20.md` — Spec S1
- `docs/plans/motor-test-framework-plan.md` — Spec S2
- `docs/findings/barometer_integration_spec_2026-05-20.md` — Spec S3
- `docs/findings/esp32_gpio_conflict_resolution_2026-05-20.md` — Spec S4
- `docs/findings/gps_passthrough_spec_2026-05-20.md` — Spec S5

**Spot-checked source @HEAD (commit 9dd60ca):** `include/pin_definitions_esp32.h`,
`src/main.cpp`, `src/motors.cpp`, `src/imu.cpp`.

---

## 1. Executive summary

Five Session-2 specs landed clean. None contains code; all are scaffolding/contract
docs. Session 3 is **ready to dispatch coders, but NOT all five at once** — two specs
(S2, S4) have operator decisions that gate their *largest* workstreams, and three
have a shared cross-spec hazard (Core-1 over-subscription + the swarm-API schema
re-stamp) that must be sequenced, not parallelized blindly.

| Spec | Subject | Verdict | One-line reason |
|---|---|---|---|
| S1 swarm-API contract | WiFi protocol doc | **READY (doc-only)** | Pure documentation of shipping code; nothing to "code". Becomes a *dependency* for S3/S5. |
| S2 motor-test framework | Bench test harness | **DEFERRED (hardware) + partial-ready** | Firmware surface (WS-1) is codeable now; execution is hardware-gated; ESC-protocol decision (DShot?) gates WS-1. |
| S3 barometer (`USE_BAROMETER`) | Telemetry-only baro | **READY-TO-CODE** | Option A only, Core-1, clean file zones. No operator decision blocks WS-1. |
| S4 ESP32 GPIO conflicts | Pin double-claim fix | **READY-TO-CODE (guard) / blocked (defaults)** | `#error` guard is codeable now; the actual pin-reassignment needs operator answers (receiver protocol, servo count). |
| S5 GPS passthrough (`USE_GPS`) | Raw NMEA relay | **READY-TO-CODE** | Flavour A only, Core-1, 2 small workstreams. Inherits S4 Conflict B with documented mitigation. |

**Verdict on the session as a whole:** *Conditionally ready.* Coders can be
dispatched immediately on S4-guard, S3, and S5 (the build-capable, zero-hardware
work). S2-firmware and the S4 default-pin reassignment must wait on the operator
answers in §8. S1 needs no coding session at all — it is a reference artifact that
S3/S5 consume.

---

## 2. Per-spec recap

### S1 — Swarm API contract (`swarm_api_contract_2026-05-20.md`)

Specifies the joint wire contract between ESP32 firmware and the sibling `swarm_api`
Python project: inbound REST (`GET /api/status`, `POST /api/commands`, `GET /`),
inbound WebSocket `/ws` (10 Hz telemetry out, 6-channel commands in), outbound HTTP
POST telemetry to `{API_SERVER_URL}/api/telemetry`. Documents arbitration
(Serial>I2C>WiFi>RC), the 500 ms `OVERRIDE_TIMEOUT_MS` failsafe, and 10 enumerated
gaps. **OVER-COMPLICATES verdict:** NO — it documents shipping code, adds nothing.
**Build-flag layout:** none new — surface is gated by existing `USE_ESP32 &&
USE_WEB_SERVER` / `USE_API_SERVER`. **Open decisions:** (a) add `api_version` field
(§7 GAP, recommended but not in this spec); (b) auth/TLS — §8 item 1, an operator
decision (see §6 below). This spec produces no codeable workstream; its role in
Session 3 is as the contract S3-WS-2 and S5-WS-B must re-stamp when they touch the
schema.

### S2 — Motor / ESC test framework (`motor-test-framework-plan.md`)

Specifies a safety-gated bench test harness: a 6-state firmware state machine
(`IDLE→ATTEST_PROPS→ATTEST_BATTERY→ARMED_FOR_TEST→RUNNING_PHASE`, plus `FAULT`),
six test phases (PWM endpoints, single-motor sweep, mixer differential, failsafe
latency, arming gate, throttle-cut), a calibration-build-only command surface
(`T`, `PROPS-OFF`/`BATTERY-*` tokens, `p1..p6`, kill byte `x`), and a host suite
`tests/suites/test_motors.sh`. **OVER-COMPLICATES verdict:** NO (the spec's own §
verdict) — it finishes the existing calibration story; gated to `*_calibration`
builds, zero live-build cost. **Build-flag layout:** new "MOTOR TEST" `#define`
section in `config.h`; logic gated by existing `#ifdef CALIBRATION_MODE`. **Open
decisions:** §8 — ESC protocol (PWM/OneShot125/**DShot?** — top open question,
gates WS-1), test rig availability, motor count, RPM instrumentation, failsafe
latency bound, VTOL-servo test scope. Flash est: ~4–6 KB, calibration build only.

### S3 — Barometer integration (`barometer_integration_spec_2026-05-20.md`)

Specifies `USE_BAROMETER` as a **telemetry-only** Core-1 feature: poll a BMP280
(default) / BMP388 / MS5611 on `Wire1` at 10–25 Hz, derive relative altitude, expose
via OLED + `/api/status` + `/ws` + outbound POST. **Option B (vertical-rate feedback
into the loop) is explicitly rejected as autopilot creep.** **OVER-COMPLICATES
verdict:** CONDITIONAL — Option A NO, Option B YES (deferred). **Build-flag layout:**
new `USE_BAROMETER` section in `config.h` (one-of-N type selector
`BAROMETER_BMP280/_BMP388/_MS5611`, `BARO_I2C_ADDRESS`, `BARO_SEA_LEVEL_PA`,
`BARO_SAMPLE_RATE_HZ`, `BARO_LPF`), plus `BARO_SDA_PIN`/`BARO_SCL_PIN` in PIN
OVERRIDES defaulting to the `Wire1` pins, plus `CALIBRATED_BAROMETER` marker. **Open
decisions:** sensor choice is a documented default (BMP280), not a blocker; no hard
operator gate. 3 workstreams (driver / telemetry / calibration-`b`).

### S4 — ESP32 GPIO conflict resolution (`esp32_gpio_conflict_resolution_2026-05-20.md`)

Specifies fixes for 3 wiring-audit pin flags. **Conflict A** (GPIO 16/17:
`SBUS_RX/TX` vs `SERVO_PIN_4/5`) is **REAL today** in the shipping `USE_SBUS_RECEIVER`
default — confirmed against source: `setupServos()` `ledcAttachPin`-es all 7 servo
pins unconditionally (`motors.cpp:81-88`, called from `main.cpp:317`), so GPIO 16/17
are LEDC-claimed on every ESP32 image. **Conflict B** (GPIO 4: iBUS/DSM/serial-cmd
RX vs `SERVO_PIN_3`) is latent-REAL — fires the moment the operator switches off
SBUS. **Conflict C** is Teensy-only, N/A on ESP32. **OVER-COMPLICATES verdict:** not
stated as such; the recommended fix (a `#error` guard + a commented PIN-override
template) is minimal and correct. **Build-flag layout:** no new flags — a compile
guard keyed on existing `USE_SBUS_RECEIVER`/`USE_IBUS_RECEIVER`/`USE_DSM_RECEIVER`/
`USE_SERIAL_COMMANDS`. **Open decisions:** §6 — real receiver protocol, physical
servo count, whether `USE_SERIAL_COMMANDS` is used, motor channel population,
move-SBUS-vs-move-servo. The guard is codeable now; default reassignment is gated.

### S5 — GPS passthrough (`gps_passthrough_spec_2026-05-20.md`)

Specifies `USE_GPS` as a **raw-NMEA passthrough** (Flavour A): a Core-1 FreeRTOS
task reads NMEA on `Serial1` (UART1, RX-only, GPIO 4), frames `$…\r\n` sentences
into a spinlock-guarded latest-sentence snapshot, exposes a `gps{nmea,age_ms,ok}`
block via the swarm API. **No parsing, no fusion, no flight-loop coupling, no RTH/
waypoints/geo-fence.** **OVER-COMPLICATES verdict:** Flavour A NO; active GPS use
YES (deferred). **Build-flag layout:** new `USE_GPS` section in `config.h`
(`GPS_UART_NUM`, `GPS_UART_BAUD`, `GPS_BUFFER_BYTES`, `GPS_STALE_TIMEOUT_MS`),
`GPS_PIN_RX`/`GPS_PIN_TX(-1)` in PIN OVERRIDES; **no calibration marker**.
A `#error` guard forbids `USE_GPS` + iBUS/DSM/serial-cmd (UART1 sharing). **Open
decisions:** none hard — sensor default (NEO-M8N) documented; inherits S4 Conflict B
with the same mitigation. 2 small workstreams (driver / swarm-API integration).

---

## 3. Cross-spec contradiction check

This is the load-bearing section: items no single spec could see because each was
written in isolation.

### 3.1 — GPIO/pin conflicts the S4 GPIO-resolution spec did NOT cover

S4 audited only the 3 wiring-audit flags. It did **not** account for the *new* pins
S3 and S5 propose, because those specs landed in parallel:

| New pin (proposed) | Spec | Default GPIO | Collision S4 missed |
|---|---|---|---|
| `BARO_SDA_PIN` | S3 | GPIO 25 (`I2C_CMD_SDA_PIN`) | **GPIO 25 = `MOTOR_PIN_1`** (`pin_definitions_esp32.h:207` vs `:48`). See 3.5. |
| `BARO_SCL_PIN` | S3 | GPIO 26 (`I2C_CMD_SCL_PIN`) | **GPIO 26 = `MOTOR_PIN_2`** (`:214` vs `:55`). See 3.5. |
| `GPS_PIN_RX` | S5 | GPIO 4 | Conflict B — S5 *acknowledges* this; not missed, but it means S4's guard must also cover `USE_GPS`. |

**Finding C-1 (NEW, not in any spec): `Wire1` already collides with motors 1 & 2.**
`I2C_CMD_SDA_PIN`/`SCL_PIN` default to GPIO 25/26 (`pin_definitions_esp32.h:207,214`)
— the *same* GPIOs as `MOTOR_PIN_1`/`MOTOR_PIN_2` (`:48,:55`). S3 routes the
barometer onto `Wire1` and inherits this pre-existing double-claim **without flagging
it**. On any ESP32 build that drives 4 motors, `Wire1` (and therefore `USE_I2C_COMMANDS`
*and* a `Wire1` barometer) electrically conflicts with the motor LEDC outputs on
25/26. This is a 4th ESP32 GPIO conflict that the S4 spec — scoped to "the 3 audit
flags" — never examined. **Session 3 must resolve this before S3-WS-1 picks
`Wire1`:** either move the baro to dedicated free pins (GPIO 13/5/18 per S4 §5, if
motors 5/6 / servos 6/7 are unused) or document that `USE_BAROMETER` is incompatible
with motors on 25/26. **Recommend: S3-WS-1 must NOT default `BARO_*` to 25/26;
the S4 free-GPIO table should be the source of the defaults.**

### 3.2 — Build-flag naming consistency

Checked across the 5 specs. Naming is consistent: all use the `USE_*` feature-flag
+ named-`#define` convention. One soft inconsistency: S4's optional compile-guard
and S5's UART `#error` guard are *separate* guards in *separate* headers
(`config.h`-region vs `gps.h`). They should be co-located or at least cross-referenced
so a future reader finds all pin/UART guards in one place. Not a contradiction —
a maintainability flag for the implementer.

### 3.3 — Core-0 / Core-1 over-subscription — **the biggest synthesis hazard**

Verified against `src/main.cpp` ESP32 `loop()` (lines ~415–457): Core 1 *today*
runs, sequentially per loop, `populateNetworkData` + `renderDisplay` (10 Hz) +
`handleWiFi` + `handleWebServer` (incl. 10 Hz WS broadcast) + `handleApiClient`
(2 Hz POST, **blocking, 2 s timeout**) + `handleOTA`, then `vTaskDelay(10ms)`.

S3 adds a `BARO_SAMPLE_RATE_HZ` (≤25 Hz) baro poll slice to that same `loop()`.
S5 adds a **dedicated FreeRTOS task pinned to Core 1** (priority 1) for GPS.

**No spec analyzed the combined Core-1 load.** With `USE_BAROMETER` +
`USE_GPS` + WiFi + web server + API client all enabled, Core 1 carries:
WiFi stack, 10 Hz WS broadcast, a *blocking* 2 Hz HTTP POST, baro I2C transactions,
and a continuously-scheduled GPS UART task. The blocking `handleApiClient()` POST
(S1 §2: "Blocking call, acceptable because it runs on Core 1") is the worst actor —
while it blocks up to 2 s, the GPS task can still run (separate task), but the baro
*poll slice lives inside the same `loop()`* and will be starved during a slow POST,
causing baro sample jitter. **This does not threaten Core 0** (flight loop is
isolated, all specs correctly assert this), but it does mean baro telemetry cadence
is not the clean `BARO_SAMPLE_RATE_HZ` the spec implies. **Session 3 mitigation:**
either (a) move the baro poll into its own low-priority Core-1 task (mirroring S5's
GPS-task rationale — S5 even argues *for* a task over a `loop()` slice), or
(b) accept and document baro jitter. Recommend (a) — and S5's own §4 reasoning is
the precedent. **This is a real cross-spec gap: S3 chose a `loop()` slice, S5 chose
a task, for structurally identical Core-1 sensors. They should agree.**

### 3.4 — Swarm-API JSON field-name collisions

Checked S3's proposed `"baro"` block and S5's proposed `"gps"` block against S1's
documented `/api/status` + `/api/telemetry` schemas. **No field-name collision** —
`baro` and `gps` are new top-level objects; `baro.{pressure,altitude_m,temp_c}` and
`gps.{nmea,age_ms,ok}` do not shadow existing keys. **But:** both S3-WS-2 and
S5-WS-B independently say they will re-stamp the S1 "Verified … @SHA" block and
re-flag the missing `api_version`. If both land in the same session, they will
**race on the same lines of `swarm_api_contract_2026-05-20.md` and on
`serializeDisplayData()` in `web_server.cpp`**. These are not disjoint file zones.
**Session 3 must serialize the two schema edits** (one workstream owns the
`serializeDisplayData()` / `api_client.cpp` / contract-doc edit, or they run
sequentially) — see §7 dependency column.

### 3.5 — I2C bus contention (IMU vs barometer)

S3 correctly keeps the baro **off `Wire`** (the Core-0 IMU bus, GPIO 21/22 — verified
`imu.cpp:38`, 400 kHz, read every 1 kHz tick) and onto `Wire1`. Good. **But two
sub-findings:**

- **C-1 restated (see 3.1):** `Wire1` pins (GPIO 25/26) collide with `MOTOR_PIN_1/2`.
  The baro avoids the *IMU* bus only to land on the *motor* pins. Net I2C-vs-IMU
  contention: resolved. Net I2C-vs-motor contention: **introduced, unflagged.**
- **OLED bus claim is mis-stated in S3.** S3 §4 says the OLED "shares [`Wire`] at
  `0x3C`". Source disagrees: `pin_definitions_esp32.h:269-282` puts the ESP32 OLED
  on **dedicated software-I2C pins GPIO 23/19**, explicitly "to avoid I2C bus
  contention with IMU". The S3 claim that OLED sits on `Wire` is **incorrect for
  ESP32** (it may reflect the Teensy/shared-bus wiring note). Harmless to the baro
  conclusion, but the implementer should not trust S3 §4's OLED row.

### 3.6 — Summary of cross-spec findings

| # | Finding | Severity | Owner to fix in Session 3 |
|---|---|---|---|
| C-1 | `Wire1` (baro) defaults to GPIO 25/26 = `MOTOR_PIN_1/2` | **HIGH** | S3-WS-1 — change `BARO_*` defaults to S4 free-GPIO table |
| C-2 | Core-1 over-subscription: baro `loop()`-slice vs GPS task inconsistency | MEDIUM | S3-WS-2 — make baro a Core-1 task, mirror S5 |
| C-3 | S3-WS-2 and S5-WS-B both edit `serializeDisplayData()` + the S1 contract doc | MEDIUM | Sequence the two; not disjoint zones |
| C-4 | S3 §4 wrongly says OLED is on `Wire`; source = GPIO 23/19 | LOW | Doc inaccuracy; implementer ignore |
| C-5 | S4 guard + S5 guard live in separate headers | LOW | Co-locate or cross-ref |

---

## 4. Combined GPIO / pin allocation master table (standard ESP32)

Every default-assigned ESP32 GPIO across IMU, motors, servos, receivers, serial CMD,
plus the S3/S5 proposed pins. Verified against `pin_definitions_esp32.h@HEAD`.

| GPIO | Assigned to (default) | Build flag / gate | Overlap risk |
|---|---|---|---|
| 0 | — | strapping | reserved |
| 1 | UART0 TX (USB) | every build | reserved |
| 2 | `LED_PIN`; `IBUS_TX`/`DSM_TX`/`SERIAL_CMD_TX` | always; iBUS/DSM/cmd | strapping; TX "not used" |
| 3 | UART0 RX (USB) | every build | reserved |
| 4 | `SERVO_PIN_3`; `IBUS_RX`/`DSM_RX`/`SERIAL_CMD_RX`; **`GPS_PIN_RX`** | servos always; receiver flag; `USE_GPS` | **Conflict B (S4) + GPS (S5)** — REAL if iBUS/DSM/cmd/GPS + servo3 |
| 5 | `SERVO_PIN_6` | servos always | free if servo ch6 unused — S4 reassign target |
| 6–11 | — | — | flash — never use |
| 12 | `MOTOR_PIN_5` | motors always | strapping |
| 13 | `MOTOR_PIN_6` | motors always | free if motor 6 unused — **best S4 reassign target** |
| 14 | `MOTOR_PIN_4` | motors always | in use |
| 15 | — | strapping | reserved |
| 16 | `SBUS_RX`; `SERVO_PIN_4` | `USE_SBUS_RECEIVER`; servos always | **Conflict A (S4) — REAL in shipping default** |
| 17 | `SBUS_TX`; `SERVO_PIN_5` | SBUS; servos always | **Conflict A (S4)** — TX "not used", lower risk |
| 18 | `SERVO_PIN_7` | servos always | free if servo ch7 unused — S4 reassign target |
| 19 | `OLED_SCL`; `PWM_CH6` | OLED; `USE_PWM_RECEIVER` | OLED-vs-PWM (not in audit's 3 flags) |
| 21 | `IMU_SDA` (`Wire`) | every build | Core-0 I2C — do NOT share |
| 22 | `IMU_SCL` (`Wire`) | every build | Core-0 I2C — do NOT share |
| 23 | `OLED_SDA`; `PWM_CH5` | OLED; `USE_PWM_RECEIVER` | OLED-vs-PWM |
| 25 | `MOTOR_PIN_1`; `I2C_CMD_SDA` (`Wire1`); **`BARO_SDA` (proposed)** | motors always; `USE_I2C_COMMANDS`; `USE_BAROMETER` | **C-1 — HIGH: motor1 vs Wire1 vs proposed baro** |
| 26 | `MOTOR_PIN_2`; `I2C_CMD_SCL` (`Wire1`); **`BARO_SCL` (proposed)** | motors always; `USE_I2C_COMMANDS`; `USE_BAROMETER` | **C-1 — HIGH: motor2 vs Wire1 vs proposed baro** |
| 27 | `MOTOR_PIN_3` | motors always | in use |
| 32 | `SERVO_PIN_1` | servos always | free if servo ch1 unused |
| 33 | `SERVO_PIN_2` | servos always | free if servo ch2 unused |
| 34/35/36/39 | `PWM_CH*` / `PPM` inputs | `USE_PWM`/`USE_PPM` | input-only — cannot drive output |

**Key takeaway:** the only genuinely-free standard-ESP32 GPIOs on a 4-motor /
0-servo airframe are **13, 5, 18** (S4 §5). Both the S4 servo reassignment *and*
the S3 barometer-bus reassignment must draw from that same tiny pool — they
**compete for it**. A 4-motor airframe that wants SBUS + servo ch4/5 + a `Wire1`
barometer can run out of safe pins. Session 3 must allocate 13/5/18 deliberately,
not let S3 and S4 each grab them independently.

---

## 5. Build-matrix risk

No spec analyzed multi-flag combinations together. The dangerous combos:

| Combo | Risk | Spec coverage |
|---|---|---|
| `USE_SBUS_RECEIVER` + servos ch4/5 | Conflict A — ships broken today | S4 only (single-spec) |
| `USE_GPS` + `USE_IBUS/DSM/SERIAL_COMMANDS` | UART1 double-claim | S5 `#error` guard |
| `USE_GPS` + servo ch3 | Conflict B (GPIO 4) | S5 defers to S4 mitigation |
| `USE_BAROMETER` + `USE_I2C_COMMANDS` | OK on `Wire1` (addr 0x76 vs 0x42 — no clash) **but** both land on GPIO 25/26 = motors | **C-1 — no spec** |
| `USE_BAROMETER` + 4 motors | baro bus pins = `MOTOR_PIN_1/2` | **C-1 — no spec** |
| `USE_BAROMETER` + `USE_GPS` + WiFi + web + API | Core-1 over-subscription, baro jitter | **C-2 — no spec** |
| `esp32` live + `USE_BAROMETER` + `USE_GPS` | flash/RAM growth | partial (per-spec only) |

**Flash / RAM estimate (synthesizable from per-spec figures):**
- S2 motor-test: ~4–6 KB flash, **calibration build only**, <64 B RAM. Zero live cost.
- S3 barometer: spec gives no number; estimate driver + vendored Bosch lib + telemetry
  ≈ 6–12 KB flash, plus 3 floats in `DisplayData_t`.
- S5 GPS: ~150–250 LOC driver + ~50–100 LOC integration ≈ 3–6 KB flash, plus a
  `gps_nmea[83]` buffer + ring buffer (`GPS_BUFFER_BYTES` 256) + a FreeRTOS task
  stack (~2–4 KB RAM for the task).
- **Combined worst case (esp32 live + baro + GPS):** order ~10–20 KB flash and
  ~3–5 KB RAM on top of the WiFi build. ESP32 has ample flash; the GPS task stack
  is the only RAM item worth watching. **No build-matrix red flag — but no spec
  verified a `USE_BAROMETER`+`USE_GPS` build actually compiles.** Session 3's first
  CI action should be to add those env/flag combos to the build matrix.

---

## 6. Security carry-over

S1 §8 item 1 is explicit: **the swarm API has no authentication and no TLS** —
anyone on the LAN can fly or OTA-flash a drone (OTA only while disarmed, but flight
commands always). S1 defers the decision: "add a shared token header, or document
the isolated-SSID-only requirement."

**Do S3/S5 amplify the exposure? — Yes, S5 does.**
- S3 (baro): adds altitude telemetry. Low marginal exposure — relative altitude of
  an unknown craft leaks little.
- **S5 (GPS): adds raw NMEA, which contains absolute latitude/longitude.** The
  unauthenticated `/api/status` + `/ws` + outbound POST now broadcast the drone's
  *physical position* to anyone on the LAN. This is a materially larger exposure
  than the pre-GPS state: a passive listener learns where the drone (and operator)
  physically is. S5's threat model ("trusted LAN") is inherited from S1 but the
  *consequence* of a breach is now location disclosure, not just craft telemetry.

**Recommendation: the operator auth decision (S1 §8.1) SHOULD land before S5 (GPS)
is coded.** It need not block S3 or the S4 guard. Minimum acceptable: a documented,
enforced "isolated SSID only" requirement plus a note in the GPS spec that NMEA on
an open network is a location leak. Preferred: a shared-token header on
`/api/status` / `/ws` / `/api/commands` before GPS telemetry ships. **This is a
gating item — see §8.**

---

## 7. Session 3 execution plan — dispatch-ready workstream table

Workstreams drawn verbatim from each spec's own §. "Ready?" = can a coder be
dispatched today with no operator input.

| WS | Spec | Size | Files (exclusive) | Depends on | Build-capable? | Ready-to-dispatch? |
|---|---|---|---|---|---|---|
| **W1 — GPIO `#error` guard** | S4 | S | `config.h` guard region (after `pin_definitions_esp32.h`) + commented PIN-override template | none | YES (compile-only) | **YES — top of queue** |
| **W2 — Barometer driver** | S3 WS-1 | M | NEW `src/barometer.cpp`, `include/barometer.h`; `lib_esp32/` (vendored Bosch lib); `config.h` baro section; `pin_definitions_esp32.h` `BARO_*` defaults | C-1 fix (pins) | YES | **YES** *(with C-1 pin fix applied — see note)* |
| **W3 — Barometer telemetry** | S3 WS-2 | S/M | `include/display_data.h`, `src/main.cpp` Core-1, `src/web_server.cpp`, `src/api_client.cpp`, optional `src/display.cpp` | W2; **shares files with W6** | YES | YES — but serialize vs W6 |
| **W4 — Barometer calibration `b`** | S3 WS-3 | S | calibration parser path, `lib/Calibration/` (new `b`), `config.h`, calibration-guide docs | W2 | YES | YES |
| **W5 — GPS driver** | S5 WS-A | S | NEW `src/gps.cpp`, `include/gps.h`; `config.h` GPS section; `pin_definitions_esp32.h` `GPS_PIN_*` | W1 guard (UART1) | YES | **YES** |
| **W6 — GPS swarm-API integration** | S5 WS-B | S | `include/display_data.h`, `src/main.cpp` Core-1, `src/web_server.cpp`, `src/api_client.cpp`, `swarm_api_contract` re-stamp | W5; **shares files with W3** | YES | YES — but serialize vs W3 + see §6 |
| **W7 — Motor-test firmware** | S2 WS-1 | M | `src/calibration_mode.cpp`, NEW `lib/Calibration/calibration_motor_test.*`, `config.h` MOTOR TEST section | **ESC-protocol decision (§8)** | YES (calib build) | **NO — blocked on operator** |
| **W8 — Motor-test host suite** | S2 WS-2 | S/M | NEW `tests/suites/test_motors.sh` | W7 command table frozen | n/a (shell) | NO — after W7 |
| **W9 — Motor-test runbook** | S2 WS-3 | S | NEW `docs/motor-test-runbook.md`, `docs/roadmap.md`, `docs/3_troubleshooting.md` | W7 §3/§4 stable | n/a (docs) | NO — after W7 |
| **W10 — S4 default pin reassignment** | S4 | S | `config.h` PIN OVERRIDES (uncomment per airframe) | **operator answers §8** | YES | **NO — blocked on operator** |

**Note on W2:** dispatchable today *only if the coder is instructed to set
`BARO_SDA/SCL_PIN` defaults from the S4 free-GPIO table (e.g. 13/5/18), NOT the
`Wire1` GPIO 25/26 the spec proposes* — see C-1. Without that instruction W2 ships
a motor-pin conflict.

**Disjoint-zone parallelism:** W1, W2, W5 touch fully disjoint files and can run in
parallel immediately. W3 and W6 BOTH edit `display_data.h` / `main.cpp` Core-1 /
`web_server.cpp` / `api_client.cpp` — they are **NOT disjoint** and must be
sequenced (or merged into one telemetry-integration workstream covering both baro
and gps). W4 is disjoint from everything except its W2 dependency.

---

## 8. Gating items

| # | Gating item | Blocks | Type |
|---|---|---|---|
| G1 | **ESC protocol — PWM / OneShot125 / DShot?** No DShot path exists in `motors.cpp`. | W7, W8, W9 (all of S2) | Operator decision |
| G2 | **Bench test rig + ESCs + motors availability** | S2 *execution* (not coding) | Hardware |
| G3 | **Real receiver protocol — SBUS, iBUS, or DSM?** Config default is SBUS; wiring docs imply FS-iA6B/iBUS. | W10; determines whether Conflict A or B is the live one | Operator decision |
| G4 | **Physical servo channel count** (0 vs 3+) | W10 — decides if pin overrides are mandatory | Operator decision |
| G5 | **Is `USE_SERIAL_COMMANDS` used** for the flight-computer path? | W10 (GPIO 4 contention) | Operator decision |
| G6 | **Swarm-API auth/TLS decision** (S1 §8.1) | SHOULD precede W6 — GPS adds a location leak (§6) | Operator decision |
| G7 | **C-1 resolution** — must `BARO_*` move off GPIO 25/26? | W2 default-pin choice | Engineering decision (recommend yes, in-session) |
| G8 | **Motor count** (4/6) | W2/W4 pin pool; W7 phase 2/3 mixer map | Operator decision |
| G9 | **`api_version` field** — add now or defer? (S1 §7 GAP) | W3/W6 schema edit scope | Operator decision (low cost) |

**Count: 9 gating items.** 5 are operator decisions that block S2 entirely (G1)
or the S4 default reassignment (G3–G5, G8); G6 is a should-precede-W6 security
decision; G7 is an in-session engineering call; G9 is a cheap optional add.
**Zero gating items block W1, W2 (with G7 applied), W4, W5** — those are the
unblocked, ready-to-code Session-3 surface.

---

## 9. Motor-test-framework readiness

`docs/plans/motor-test-framework-plan.md` is a **complete, well-structured spec** —
safety state machine, 6 phases, command surface, host-integration stub, flash
estimate, 3 workstreams, 6 open questions. It does **not** need promotion to a
separate spec doc; it *is* the spec. **But it is NOT implementation-ready as a
dispatch target**, for two reasons:

1. **Open question G1 (ESC protocol) gates WS-1.** The spec itself flags this as the
   "top open question": if the operator's ESCs are DShot, the endpoint values and
   arming behavior change and there is no DShot path in `motors.cpp`. A coder
   dispatched on W7 today could implement the state machine and parser but would
   guess the endpoint semantics. **W7 must wait for G1.**
2. **Execution is hardware-gated (§6 of the spec, G2 here).** The spec is explicit:
   nothing runs until ESCs + motors + a secured rig + bench power exist.
   The spec's own recommendation — "implement firmware + host stub in one session,
   validate later" — is sound, but only *after* G1 is answered.

**Verdict:** spec quality = ready; dispatch readiness = **blocked on G1**, then
codeable as W7→W8→W9 (W9 may parallelize with W8). No spec-promotion needed.

---

## 10. Recommendation — what the next FC coding session looks like

**Session 3 should be a two-track session.**

**Track A — dispatch immediately, 3 parallel coders (disjoint file zones):**
1. **W1 — GPIO `#error` guard** (S4). Smallest, highest-safety-leverage: it makes
   the Conflict-A bad image (`SBUS` + servo 4/5) fail loudly at compile time
   instead of shipping silently. **This is the recommended top-of-queue workstream.**
2. **W2 — Barometer driver** (S3 WS-1) — *with the explicit C-1 instruction* to
   default `BARO_*` pins to free GPIOs (13/5/18), not `Wire1`'s 25/26.
3. **W5 — GPS driver** (S5 WS-A).

These three touch fully disjoint files (`config.h` guard region; `src/barometer.*`;
`src/gps.*`) and carry no operator-decision dependency.

**Track B — sequential, after Track A drivers land:**
4. **W4** (baro calibration `b`) — independent, can start once W2 exposes accessors.
5. **A single merged telemetry workstream** covering W3 + W6 — because they both
   edit `display_data.h`, `main.cpp` Core-1, `web_server.cpp`, `api_client.cpp` and
   both re-stamp the S1 contract doc (C-3). **Do not run W3 and W6 as parallel
   coders.** One coder, one schema edit, one re-stamp. While doing so: apply C-2
   (make the baro a Core-1 task, not a `loop()` slice, consistent with the GPS
   task) and decide G9 (`api_version`).

**Hold for the operator (do not dispatch):**
- **W7/W8/W9 (motor-test)** — blocked on G1 (ESC protocol). Put the §8 questions
  to the operator *before* Session 3 so S2 can join Session 4.
- **W10 (S4 default pin reassignment)** — blocked on G3/G4/G5/G8.
- **W6 (GPS telemetry)** specifically — should follow the G6 auth decision (§6);
  if the operator cannot decide in time, ship W6 with an explicit "isolated SSID
  only — NMEA is a location leak" warning in the GPS section of `config.h`.

**Net:** Session 3 realistically delivers W1 + W2 + W5 + W4 + the merged baro/gps
telemetry workstream — i.e. the entire barometer feature and the GPS driver,
plus the GPIO safety guard. That is a coherent, build-capable, zero-hardware
session. Motor-test slips to Session 4 pending operator answers. The single most
important pre-session action is getting the §8 G1/G3/G4/G6 answers from the
operator so Session 4 is not itself blocked.

---

*Synthesis complete. Read-only. No source, config, or test files modified. No git
commits. This is the only file written by this agent.*
