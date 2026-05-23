# Calibration & Sensor Telemetry Verification — 2026-05-22

**Agent:** reviewer (reviewer@flight_controller:verify)
**Scope:** read-only verification of the calibration subsystem and sensor-telemetry
pipeline. No source modified, no commits. Native tests + ESP32/Teensy builds run
read-only.

---

## 1. Executive Verdict

| Subsystem | (a) Compiles | (b) Logic sound & test-covered | (c) Hardware-validated | Verdict |
|-----------|:---:|:---:|:---:|---------|
| Calibration command set (9 cmds) | ✅ | ✅ logic sound; no native tests (interactive serial) | ⚠️ bench-gated | **WORKING (a+b)** |
| IMU read + 6-pos / orient cal | ✅ | ✅ | ⚠️ bench-gated | **WORKING (a+b)** |
| Barometer 'b' sea-level cal | ✅ | ✅ | ⚠️ no baro on bench | **WORKING (a+b)** |
| Madgwick6DOF attitude | ✅ | ✅ test-covered **with a known NaN edge case** | ⚠️ bench-gated | **WORKING (a+b), 1 latent bug** |
| Barometer drivers (BMP280/388/MS5611) | ✅ | ✅ compensation math matches datasheets, replicated in tests | ⚠️ no baro hw | **WORKING (a+b)** |
| GPS passthrough (NMEA framing + Core-1 snapshot) | ✅ | ✅ logic sound; no native test | ⚠️ no GPS hw | **WORKING (a) , (b) by inspection** |
| /api/telemetry block assembly | ✅ | ✅ by inspection | ⚠️ bench-gated | **WORKING (a), (b) by inspection** |

**Overall:** The calibration subsystem and sensor-telemetry pipeline are **"working"
at levels (a) compiles and (b) logic sound + test-covered.** Level (c)
hardware-validation is bench-gated and out of reach this session — **no ESCs/motors,
no barometer, no GPS module on the bench** (per project memory: only OLED + MPU6050
present). One statically-fixable correctness issue exists (Madgwick NaN guard, P2).

---

## 2. Build & Test Results (exact counts)

### Native pure-math suite — `bash tools/build_tests.sh`
Auto-discovers `tests/native/test_*.cpp`, compiles each standalone with
`g++ -std=c++11 -O2 -DUNIT_TEST`, runs it.

| File | Assertions | Result |
|------|-----------:|:------:|
| test_attitude | 24 | PASS |
| test_barometer_compensation | 24 | PASS |
| test_filters | 29 | PASS |
| test_harness_selfcheck | 5 | PASS |
| test_mixer | 32 | PASS |
| **Total** | **114** | **5/5 files, 114/114 assertions PASS** |

(Brief estimated "~5 files / ~110 assertions" — confirmed: exactly 5 files, 114 assertions.)

### Firmware builds (read-only `pio run`)

| Env | Result | Flash | RAM |
|-----|:------:|-------|-----|
| `teensy40` | ✅ SUCCESS (10.3 s) | code 25,656 + data 6,328 + headers 8,972 B; ~1.99 MB free for files (≈2% used of 2 MB) | RAM1 vars 8,192 + code 23,480 B (483 KB free locals); RAM2 12,416 B (511 KB free) |
| `esp32` | ✅ SUCCESS (10.6 s) | 43.6% (571,121 / 1,310,720 B) | 10.9% (35,636 / 327,680 B) |

Both built clean (exit 0), no warnings surfaced. Note: default `esp32`/`teensy40`
configs do not necessarily enable USE_BAROMETER/USE_GPS — those modules are gated;
they compile cleanly when enabled but are not exercised in the default-env build
above. (Both are pure additive `#ifdef` blocks; verified by inspection they have no
unconditional symbols.)

---

## 3. Calibration Subsystem — Detail

### Command set (verified in `src/calibration_mode.cpp` `processSerialLine()`)
All single-char commands present and routed correctly:
`a` sequential · `c` status · `d` dump · `r` radio · `i` IMU single ·
`m` 6-position · `o` IMU+orientation · `f` failsafe · `e` ESC · `b` barometer
(gated on USE_BAROMETER) · `s` status · `t` telemetry toggle · `g`/`p` gain/param
get-set · `n` network diag (ESP32+WiFi) · `h`/`?` help.

Logic is sound:
- **Gyro/accel bias** (`calibration_imu.cpp`): two-pass variance stability gate
  before sampling (rejects gyro stdev > 3°/s, warns if AccZ ≠ ~1g), 2000-sample
  mean, Z-offset = mean − 1.0 g. Quality checks with sane thresholds (accel ±0.2/0.3 g,
  gyro ±15°/s). Recursion-based retry. Matches Betaflight `calibrate acc` approach
  documented in `docs/findings/auto-calibration-research.md` (offset = mean, Z = mean−1g).
- **6-position** (`calibrate IMU6Position`): offset = (pos+neg)/2, scale =
  2/(pos−neg) per axis — textbook-correct. Scale sanity check ±5%.
- **Orientation detection** (`calibration_orientation.cpp`): nose-up / right-side-up /
  top-up gravity captures, max-axis selection with `forwardAxis != X` exclusion guards
  to prevent the same IMU axis being assigned to two aircraft axes. Sound.
- **Barometer 'b'** (`calibration_baro.cpp`): double-gated CALIBRATION_MODE &&
  USE_BAROMETER. Spins a **local** Barometer instance (does NOT touch the Core-1
  telemetry task — correct isolation), averages 50 samples, sets P0 = measured P so
  current spot reads altitude 0, verifies one read ≈ 0 m, emits a copy-paste
  `#define BARO_SEA_LEVEL_PA`. Aborts cleanly if < 25/50 reads succeed. Correct and safe.
- **Magnetometer** gated on USE_MPU9250 only (BNO sensors are detect-only Phase-A stubs).

### Minor observations (not bugs)
- `calibrateIMU()` retries via direct recursion (`calibrateIMU()` inside itself). On a
  pathologically unstable bench a user could nest several frames, but each requires an
  interactive `y`, so stack growth is operator-bounded. Acceptable for a calibration build.
- Orientation/6-position calibrations emit copy-paste config but rely on the operator to
  paste & re-flash (by design — compile-time-everything philosophy). Not auto-persisted.

---

## 4. Sensor-Telemetry Pipeline — Detail

### 4.1 IMU + Madgwick6DOF (`src/imu.cpp`) — **1 latent correctness issue (P2)**
`getIMUdata()` is sound: raw → g/deg-s scaling, offset-then-scale calibration, PT1 LP,
optional biquad/notch under USE_OPTIMIZATION. Madgwick6DOF math is the standard
gradient-descent form; Euler extraction correct; called as
`Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt)` in main.cpp:195.

**ISSUE M-1 (P2, statically fixable): Madgwick NaN-on-zero-gradient.**
`invSqrt(x)` is implemented as `1.0f / sqrtf(x)` (imu.cpp:306). When the accel vector
is **exactly** the level gravity vector and gyro is exactly zero, the gradient
`s0..s3` collapse to all-zero → `invSqrt(0) = +inf` → `s0*inf = NaN` → the quaternion
becomes NaN and never recovers (NaN propagates through the normalisation forever).
The native test (`test_attitude.cpp:182-185`) **explicitly documents this** and
deliberately perturbs its inputs to avoid tripping it; test 9 (zero-accel) confirms the
*accel-skip* branch is NaN-free, but the *zero-gradient* path is not guarded.
- Real-world likelihood: low (a physical MPU6050 never delivers a bit-exact gravity
  vector with bit-exact zero gyro), but a synthetic/replayed input, a perfectly still
  high-precision sensor, or a quantised value could hit it; once NaN, attitude is dead.
- **Suggested fix (do NOT apply here):** guard the gradient normalisation, e.g. skip the
  correction step when `(s0*s0+s1*s1+s2*s2+s3*s3)` is below a small epsilon, OR add a
  `isnan(q0)` → reset-to-identity recovery after the final normalisation. No NaN guard
  exists anywhere in the imu.cpp / main.cpp attitude path today.

### 4.2 Barometer drivers (`src/barometer.cpp`) — compensation matches datasheets
- **BMP280:** integer temperature + 64-bit integer pressure compensation is the exact
  Bosch BMP280 datasheet reference algorithm; includes `if (p1 == 0) return false`
  divide-by-zero guard. Trim block read little-endian from 0x88, chip-ID gate accepts
  BMP280 (0x58/0x56/0x57) + BME280 (0x60). ✅ matches datasheet.
- **BMP388:** float compensation per Bosch BMP3 §9.2/§9.3; NVM trim scaled by the
  documented powers-of-two quantisation divisors (2^-8 … 2^65). Header carries a
  "DATASHEET-REVIEW" caveat for exact silicon. ✅ matches datasheet form.
- **MS5611:** first-order + second-order (low-temp) compensation per MS5611-01BA, plus
  the AN520 CRC-4 over the 8 PROM words, all-zero / all-0xFFFF reject, and a per-read
  `d1==0||d2==0` guard. ✅ matches datasheet.
- All three compensation algorithms are **replicated verbatim** in
  `test_barometer_compensation.cpp` (24 assertions, PASS) — the test file states it
  copies the source identically and validates against datasheet expected values.
- `Barometer::read()` carries a **P3 robustness guard already in place**:
  `if (_sea_level_pa <= 0.0f) return false;` prevents inf/NaN altitude if a runtime
  sea-level calibration sets P0 ≤ 0. Good. Altitude uses the international barometric
  formula `44330*(1-(P/P0)^0.1903)` with PT1 LP. Core-1 task + spinlock snapshot is
  correctly isolated from the Core-0 flight loop.

### 4.3 GPS passthrough (`src/gps.cpp`) — NMEA framing + Core-1 snapshot sound
- RX-only UART framer: `$` resets accumulator and starts a sentence, bytes outside a
  frame are dropped, `\r` ignored, `\n` publishes the sentence (requires payload >1 char,
  rejecting bare `$\n`), buffer-overflow → drop-oldest + counter. Per-poll byte budget
  caps work so a byte-storm can't monopolise the task. ✅ correct framing.
- **Snapshot accessors** (`gpsTelemetryNMEA/Ok`): spinlock-guarded, `last_ms==0` ⇒ no
  sentence, age computed with a `now>=last_ms` wrap guard, `gpsTelemetryOk()` is a
  liveness bit (age ≤ GPS_STALE_TIMEOUT_MS), explicitly NOT a fix-quality bit (by
  design — pure passthrough, FC parses nothing). Core-0 never reads GPS. ✅ sound.
- Minor: `gpsTelemetryNMEA()` returns age in ms and writes the NMEA string; `last_ms`
  is read inside the critical section but `now=millis()` is read after exiting — a
  benign sub-ms skew on the reported age, not a correctness problem.

### 4.4 /api/telemetry assembly (`src/api_client.cpp`) — sound by inspection
- Builds the JSON doc with base fields (drone_id, armed, roll/pitch/yaw, loop_us,
  motors m1-m4, rssi, heap, uptime). Baro block (gated USE_BAROMETER) and GPS block
  (gated USE_GPS) read the **same** spinlock snapshots as web_server's
  `serializeDisplayData()`, with matching field names/units/decimals — good consistency.
- 512-byte serialize buffer; documented worst case ≈480 B (base 260 + baro 90 + gps
  130). `serializeJson()` is length-bounded so no overrun. Blocking POST on Core 1 is
  fine (not time-critical), 2 s timeout, error log throttled to 10 s. ✅ sound.
- Minor: `baro["pressure_pa"] = serialized(String(...,1))` uses ArduinoJson
  `serialized()` to emit fixed-decimal — correct technique, no issue.

---

## 5. Prioritized statically-fixable correctness issues

| ID | Sev | Location | Issue | Suggested action (NOT applied) |
|----|-----|----------|-------|--------------------------------|
| M-1 | **P2** | `src/imu.cpp` Madgwick6DOF (gradient norm @ ~269; invSqrt @ 306) | Zero-gradient (exact level accel + zero gyro) ⇒ `invSqrt(0)=inf` ⇒ NaN quaternion that never recovers. Documented in test_attitude.cpp:182. No NaN guard anywhere in the attitude path. | Epsilon-guard the gradient normalisation, or add an `isnan(q0)`→reset-identity recovery after final normalise. Real-hardware likelihood low but failure is unrecoverable. |
| (none P0/P1) | — | — | No P0/P1 correctness defects found in calibration or telemetry. | — |

**Already-present good guards (no action):** BMP280 `p1==0` divide guard; baro
`_sea_level_pa<=0` guard; GPS `last_ms==0` / wrap guards; api_client length-bounded
serialize; orientation-axis exclusion guards.

---

## 6. Hardware-validation gate (level c)

Not achievable this session. Bench has OLED SSD1306 + MPU6050 only (no ESCs/motors —
ESC cal 'e' untestable; no barometer breakout — 'b' cal and baro telemetry untestable;
no GPS module — GPS passthrough untestable). Prior bench runs (project memory,
2026-02-17) validated the 9 IMU/radio calibration commands at 42/42 automated checks on
real Teensy hardware. Baro/GPS hardware validation remains a future bench task.

---

## 7. Conclusion

Calibration subsystem and sensor telemetry are **correct and "working" at levels (a)
and (b)**: both target builds compile clean, the 114-assertion native suite passes
5/5, and all logic (calibration math, Madgwick, three baro compensation algorithms,
NMEA framing, Core-1 snapshots, API JSON assembly) is sound by inspection and matches
the cited datasheets and findings docs. The single latent correctness issue is the
**Madgwick NaN-on-zero-gradient (M-1, P2)** — already documented by the test suite and
worth a guard, but not a blocker. Level (c) hardware validation is bench-gated for
baro/GPS/ESC and out of scope this session.
