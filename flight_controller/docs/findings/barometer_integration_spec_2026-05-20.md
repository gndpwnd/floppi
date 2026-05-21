# Barometer Integration Spec (`USE_BAROMETER`)

> Date: 2026-05-20
> Agent: fc-sensor-specs@flight_controller:2
> Status: Scaffolding spec — NO CODE. Unblocks a future implementation session.
> Scaffolding source: `future_session_scaffolding_2026-05-20.md` §3.2

**Cross-references (do not duplicate):**
- `future_session_scaffolding_2026-05-20.md` §3.2 — the contract this spec fulfills.
- `swarm_api_contract_2026-05-20.md` — the WiFi telemetry surface a baro field would extend.
- `bare-bones-fc-research.md` — the loop-overhead philosophy this spec respects.
- `esp32-dual-core-research.md` — Core 0 / Core 1 split rationale.
- `scope.md` "Out of Scope": baro is listed as "flight computer territory".

---

## 1. Framing and the scope tension

`scope.md` explicitly lists "GPS, barometer, magnetometer — flight computer
territory" under **Out of Scope**. The scaffolding §3.2 acknowledges this and
records the conflict: recon §9 calls the barometer the next-most-modular sensor
add, while scope.md keeps it off the FC.

This spec does **not** override scope.md. It scopes the *minimal* version that
the modular hardware architecture already allows. `scope.md` "Hardware
Architecture Vision" explicitly says:

> "Future sensors added modularly: Additional sensors (barometer, GPS, lidar)
> would each get their own `USE_*` flag and run on Core 1 (ESP32) or be handled
> by an external flight computer. The base flight loop on Core 0 is never
> affected."

So a **telemetry-only barometer on Core 1** is consistent with the architecture
vision even though raw "altitude integration" is flight-computer territory. A
barometer that feeds the flight loop is **not** — see §3.

The scaffolding verdict is **CONDITIONAL** and this spec restates it (§9):
telemetry-only is fine; vertical-rate feedback into the loop is autopilot creep.

---

## 2. Sensor candidates

All three are I2C, 3.3 V tolerant, available as cheap hobbyist breakouts, and
adequate for a relative-altitude / vertical-trend readout. None needs SPI.

| Sensor  | Typ. cost | I2C address(es)        | RMS noise (still)        | Max ODR  | Notes |
|---------|-----------|------------------------|--------------------------|----------|-------|
| BMP280  | ~$1-3     | `0x76` (SDO=GND) / `0x77` (SDO=VCC) | ~0.12 Pa ≈ ±1.0 m raw, ~0.2 m oversampled | ~157 Hz (low-power modes lower) | Cheapest, ubiquitous, also on many GY-modules. Temp + pressure. Adequate for telemetry. |
| BMP388  | ~$3-6     | `0x76` / `0x77`        | ~0.03 Pa ≈ ±0.25 m, ~0.08 m oversampled | ~200 Hz | Lower noise, built-in FIFO, better temp stability than BMP280. Bosch's drone-targeted part. |
| MS5611  | ~$4-8     | `0x76` / `0x77` (CSB pin selects) | ~0.012 mbar ≈ ±0.10 m (24-bit ADC) | ~100+ Hz (conversion-time limited) | Best resolution, classic autopilot baro (used on early Pixhawk). Conversion is a request/read state machine — needs ~10 ms between trigger and read. |

**Address-clash note:** all three default to `0x76`/`0x77`. The IMU (MPU6050)
sits at `0x68`/`0x69` and the OLED at `0x3C` — **no clash with the baro** on a
shared bus. A baro and a second I2C device that also wants `0x76`/`0x77` would
clash; none is currently planned.

**Recommendation:** **BMP280** as the default (`BAROMETER_BMP280`) — cheapest,
most available, and more than good enough for a telemetry-grade vertical
readout. **BMP388** as the documented upgrade for users who want lower noise.
**MS5611** supported as an option but flagged: its request/read conversion
cycle is more state-machine work in the driver and is overkill for telemetry.

---

## 3. Where it runs — the explicit scope decision

This is the load-bearing decision of the spec. Two options:

### Option A — Telemetry-only, Core 1 (RECOMMENDED)

The barometer is polled on **ESP32 Core 1**, alongside the existing display /
WiFi / web-server / API-client work. Pressure and a derived relative-altitude
value are placed into the `DisplayData_t` snapshot and exposed:
- on the OLED (a new optional screen / field),
- in the `/api/status` JSON and `/ws` telemetry stream,
- in the outbound `/api/telemetry` POST payload.

**Consequences:**
- **Zero flight-loop impact.** Core 0 at 1 kHz never reads the baro, never waits
  on its I2C transaction, never integrates its output. This is exactly the
  guarantee `scope.md` "Hardware Architecture Vision" promises for future
  sensors.
- The barometer becomes data *for the external flight computer* — which is
  precisely where `scope.md` says altitude logic belongs. The FC is a relay, the
  flight computer decides what to do with altitude.
- I2C contention is contained to Core 1 (see §4).
- Teensy builds: telemetry-only baro would run in the 10 Hz display slice of the
  single-core `loop()`. Possible, but secondary — the WiFi telemetry surface
  that makes baro useful only exists on ESP32. **Recommend ESP32-first;** treat
  Teensy support as optional and lower priority.

### Option B — Vertical-rate feedback into the flight loop, Core 0 (NOT recommended)

The barometer (or a baro-derived vertical-rate estimate) feeds a Core-0 control
term — altitude hold or vertical-speed damping.

**Consequences:**
- **This is autopilot territory.** It adds runtime cost to the 1 kHz loop, adds
  an I2C transaction (or a cross-core hand-off) into the real-time path, and
  requires a baro/accel fusion filter to get usable vertical rate. That fusion,
  the altitude PID, and the loop coupling are exactly the "complex logic" that
  `scope.md` ("The FC stabilizes. Period.") assigns to the flight computer.
- It would violate the bare-bones philosophy in `bare-bones-fc-research.md`:
  "Every feature that adds runtime overhead to the flight loop must justify its
  existence."
- Verdict: **out of scope.** If altitude hold is ever wanted, the flight
  computer consumes the Option-A telemetry stream and sends throttle commands
  back through the existing `/api/commands` channel surface — no firmware change
  to the flight loop.

**Decision: implement Option A only.** `USE_BAROMETER` is, by definition in this
spec, a telemetry-only feature. There is no `USE_BAROMETER_ALTITUDE_HOLD`.

---

## 4. I2C bus allocation — avoiding IMU contention

The FC already uses two I2C surfaces (verified against `imu.cpp`,
`pin_definitions_esp32.h`, `pin_definitions.h`):

| Bus            | ESP32 default pins        | Used by | Owner core |
|----------------|---------------------------|---------|------------|
| `Wire`         | SDA `GPIO21`, SCL `GPIO22` (S3: `GPIO8`/`GPIO9`) | IMU (MPU6050 @ `0x68`); OLED shares it at `0x3C` per wiring notes | Core 0 reads IMU; OLED read on Core 1 |
| `Wire1`        | SDA `GPIO25`, SCL `GPIO26` (S3: `41`/`42`) | I2C command slave (`USE_I2C_COMMANDS`, addr `0x42`) | Core 1 |

`imu.cpp` calls `Wire.begin(IMU_SDA_PIN, IMU_SCL_PIN)` at 400 kHz and the
1 kHz flight loop calls `mpu6050.getMotion6()` on `Wire` **every tick on
Core 0**. Putting the barometer on `Wire` would mean Core 1 issuing baro
transactions onto the same bus the real-time loop depends on — a transaction
collision or a stretched clock could stall a flight-loop IMU read. **Do not put
the baro on `Wire`.**

**Recommended allocation:** put the barometer on **`Wire1`**.

- `Wire1` is already a Core-1-owned bus when `USE_I2C_COMMANDS` is set, and is
  otherwise free. The barometer at `0x76`/`0x77` does not clash with the I2C
  command slave at `0x42`, so a baro and `USE_I2C_COMMANDS` can coexist on
  `Wire1`.
- If `USE_I2C_COMMANDS` is **not** used, `Wire1` is dedicated to the baro —
  cleanest case.
- Config provides `BARO_SDA_PIN` / `BARO_SCL_PIN` overrides (see §5) defaulting
  to the `Wire1` pins, so a user with a different wiring can move them.
- A future second Core-1 I2C sensor would also live on `Wire1`; only an
  address clash (another `0x76`/`0x77` device) would force a third bus.

**Rule for the implementation session:** the barometer driver must be
initialized and polled **only from Core 1 code paths** (the `loop()` ESP32
branch in `main.cpp`), never from `flightControlTick()` / `flightControlTask()`.

---

## 5. Config flag layout

New section in `include/config.h`, following the existing IMU-selection and
BNO-scaffolding conventions (single feature flag + a one-of-N type selector +
named constants, all `#ifdef`-gated for zero cost when disabled):

```c
//=============================================================================
// BAROMETER (optional, telemetry-only — ESP32 Core 1)
//=============================================================================
// Telemetry-only: barometer is read on Core 1 and exposed via OLED + WiFi API.
// It NEVER feeds the Core-0 flight loop. Altitude logic belongs on the flight
// computer (see scope.md). See docs/findings/barometer_integration_spec_2026-05-20.md
//
//#define USE_BAROMETER

#ifdef USE_BAROMETER
    // Select ONE sensor type:
    #define BAROMETER_BMP280       // default — cheapest, most common
    //#define BAROMETER_BMP388     // lower noise, FIFO
    //#define BAROMETER_MS5611     // best resolution, request/read conversion

    // I2C address — most breakouts default to 0x76; 0x77 if SDO/CSB pulled high
    #ifndef BARO_I2C_ADDRESS
        #define BARO_I2C_ADDRESS 0x76
    #endif

    // Sea-level reference pressure (Pa) for relative-altitude calculation.
    // Determined by the calibration routine below; copy the printed value here.
    #ifndef BARO_SEA_LEVEL_PA
        #define BARO_SEA_LEVEL_PA 101325.0f   // ISA standard — placeholder
    #endif

    // Poll rate on Core 1 (Hz). Telemetry-grade; 10-25 Hz is plenty.
    #ifndef BARO_SAMPLE_RATE_HZ
        #define BARO_SAMPLE_RATE_HZ 20
    #endif

    // Output low-pass filter coefficient (0.0-1.0), same PT1 convention as
    // B_ACCEL / B_GYRO in this file. Lower = smoother altitude readout.
    #ifndef BARO_LPF
        #define BARO_LPF 0.20f
    #endif
#endif
```

`BARO_SDA_PIN` / `BARO_SCL_PIN` are added to the **PIN OVERRIDES** block in
`config.h` and given `#ifndef`-guarded defaults in `pin_definitions_esp32.h`
that point at the `Wire1` pins (`GPIO25`/`GPIO26`; S3 `41`/`42`):

```c
// In pin_definitions_esp32.h, near the I2C-command pins:
#ifndef BARO_SDA_PIN
    #define BARO_SDA_PIN I2C_CMD_SDA_PIN   // shares Wire1, no address clash
#endif
#ifndef BARO_SCL_PIN
    #define BARO_SCL_PIN I2C_CMD_SCL_PIN
#endif
```

A new **calibration status marker** `CALIBRATED_BAROMETER` follows the existing
`CALIBRATED_*` pattern (gated under `#ifdef USE_BAROMETER`).

---

## 6. Calibration routine outline — sea-level pressure reference

The only thing a telemetry barometer needs calibrated is the **sea-level
pressure reference** used to convert measured pressure to a relative altitude.
This fits the project's "every hardware-dependent value has an auto-calibration
routine" philosophy (`scope.md` Auto-Calibration Philosophy).

New serial command in the calibration build — suggested letter **`b`** (verify
no clash with the existing set: `a c d r i m o f e g p t s n` — `b` is free).

Routine behavior:
1. Operator places the drone at its intended ground/reference height and runs `b`.
2. Routine reads the barometer for ~3-5 s, averages pressure, and checks
   stability (reject if the reading drifts beyond a noise threshold — same
   "validate own results" pattern as the IMU routines).
3. It then does one of two things, operator-selectable:
   - **Known-altitude mode:** operator enters the current field elevation; the
     routine back-solves the equivalent sea-level pressure from the barometric
     formula and the averaged reading.
   - **Zero-here mode:** the routine simply stores the averaged pressure as the
     reference, so reported altitude reads ~0 m at the current spot (relative
     AGL). This is the simpler default and adequate for telemetry.
4. It prints a copy-paste line: `#define BARO_SEA_LEVEL_PA <value>` for
   `config.h`, exactly like the other calibration routines emit `#define`s.
5. `'d'` (dump-all) and `'c'` (status) are extended to include the baro
   reference and `CALIBRATED_BAROMETER`.

No PID, no fusion, no per-axis scale factors — relative altitude from a single
reference pressure is the whole calibration.

---

## 7. Implementation workstreams

Three workstreams with clean file boundaries. Each is independently reviewable.

### WS-1 — Barometer driver module (M)
- **New files:** `src/barometer.cpp`, `include/barometer.h`.
- Provides `setupBarometer()`, `readBarometer()`, and accessors for pressure,
  temperature, and relative altitude. Owns `Wire1` init at `BARO_SDA_PIN` /
  `BARO_SCL_PIN`. Driver dispatch on `BAROMETER_BMP280` / `_BMP388` / `_MS5611`.
- Vendor the chosen Bosch / MS5611 library into `lib_esp32/` per the project's
  library-vendoring rule (`scope.md` technical decisions).
- Applies the `BARO_LPF` PT1 filter; converts pressure → relative altitude
  using `BARO_SEA_LEVEL_PA`.
- Entirely Core-1-safe; contains no Core-0 entry points.
- **Size: M** — driver + library vendoring + the MS5611 request/read state
  machine if that variant is enabled.

### WS-2 — Telemetry integration (S/M)
- **Touches:** `include/display_data.h` (add `baro_pressure`,
  `baro_altitude_m`, `baro_temp_c` to `DisplayData_t`); `src/main.cpp` ESP32
  `loop()` (add a `BARO_SAMPLE_RATE_HZ` poll slice on Core 1, mirroring the
  10 Hz display slice); `src/web_server.cpp` (`serializeDisplayData()` — add a
  `"baro"` block); `src/api_client.cpp` (add baro fields to the outbound POST);
  optionally `src/display.cpp` (a baro field/screen).
- Cross-reference `swarm_api_contract_2026-05-20.md`: adding a `"baro"` block to
  `/api/status` and `/api/telemetry` is a **schema change** — the "Verified …
  @SHA" stamp in that doc must be re-stamped, and the absent `api_version`
  field (its §7 GAP) makes a coordinator unable to detect the addition. Flag
  this; do not silently extend the schema.
- **Size: S/M** — additive, but spans several files and the WiFi contract.

### WS-3 — Barometer calibration routine + docs (S)
- **Touches:** the calibration command parser (the `checkSerialCommands()` path
  referenced from `main.cpp`), `lib/Calibration/` (new `b` routine alongside the
  existing ones), `config.h` (`CALIBRATED_BAROMETER` marker, `BARO_SEA_LEVEL_PA`
  default), and `docs/features/calibration-guide.md` + `2_calibration_guide.md`
  (document the `b` stage).
- Implements §6: read, validate, back-solve or zero, emit `#define`.
- **Size: S** — one self-validating routine, no new control math.

**Suggested order:** WS-1 → WS-2 → WS-3. WS-2 needs WS-1's accessors; WS-3 needs
WS-1 to read pressure. WS-2 and WS-3 are independent of each other once WS-1
lands and could be parallelized.

---

## 8. Out of scope for this feature (explicit non-goals)

- **No altitude-hold or vertical-speed PID.** No Core-0 control term. (§3 Option B.)
- **No baro/accel fusion filter** in firmware — that is a flight-computer job.
- **No SD-card logging of baro data** — prohibited by `scope.md`; baro telemetry
  reaches a host over WiFi, consistent with `future_session_scaffolding` §3.6.
- **No Teensy-priority support** — telemetry baro is only meaningfully useful
  with the WiFi surface; Teensy is optional/secondary.
- **No automatic in-flight sea-level-pressure tracking.** Single reference set
  at calibration; weather drift is the flight computer's concern.

---

## 9. OVER-COMPLICATES verdict

Restating `future_session_scaffolding_2026-05-20.md` §3.2: **CONDITIONAL.**

- **Telemetry-only barometer on Core 1 (Option A): NOT over-complicating.** It
  is exactly what `scope.md`'s "Hardware Architecture Vision" pre-authorizes for
  future sensors — own `USE_*` flag, runs on Core 1, base flight loop untouched.
  The FC stays a stabilizer; the baro is data for the external flight computer.
  **This is the default and only recommended implementation.**

- **Vertical-rate / altitude-hold feedback into the loop (Option B): OVER-
  COMPLICATES — YES, defer.** It puts a sensor and a fusion filter into the
  1 kHz real-time path and crosses into autopilot territory that `scope.md`
  ("The FC stabilizes. Period.") and the bare-bones philosophy explicitly
  exclude. If altitude control is ever wanted, the flight computer consumes the
  Option-A telemetry and commands throttle through the existing channel API —
  **no flight-loop change required.**

The implementation session should build `USE_BAROMETER` strictly as the
telemetry-only Option A and stop there.
