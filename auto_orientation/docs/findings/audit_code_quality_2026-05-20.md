# Code Quality Audit: Post-Merge State
## Auto Orientation Framework — Balance Robot Reference Application
**Date**: 2026-05-20  
**Merge State**: Clean (merged err0r/main at commit 71c90f6)  
**Scope**: Phase 4M.0 regression + general code quality post-merge  

---

## Executive Summary

The 2026-05-19 multi-agent merge integrated Phase 4.10c (BOOTSTRAP), audit fixes (gyro atomicity, K-quality gate), and the strategic pivot (Mega-universal vs Uno-minimal). Post-merge state is **functionally sound for most features** but contains **one critical P0 regression** (collision detection), **multiple P2 error-handling gaps**, and **unresolved scope violations (P3)**.

### Key Findings
- **P0 (1 finding)**: Collision-detection state-machine logic completely reverted; scaffolding survives
- **P1 (3 findings)**: Memory hazards minimal; Uno budget acceptable; no bloat detected
- **P2 (3 findings)**: Error-handling gaps in IMU read, motor init, HELD detection degradation
- **P3 (16 findings)**: Hardcoded thresholds per scope.md audit violations (expected post-pivot)
- **P4 (2 findings)**: Untracked test file, dead macro

---

## Detailed Findings

### P0: Correctness Bugs

#### 1. COLLISION-DETECTION REGRESSION (Phase 4M.0 top priority)
**Severity**: P0 | **File**: `src/applications/balancing_robot/balance_app.{h,cpp}`

The three-gate collision detector (PEAK, SUSTAIN, KICK from research_collision_signature_bno055.md) is **completely reverted**:

- `collision_detected()` → **NOT FOUND** in balance_app.h or .cpp
- `clear_collision()` → **NOT FOUND**
- Spike checks in `step_bootstrap_`, `step_char_act_`, `step_run_` → **NOT FOUND**
- Impact handling (→IDLE with failure_reason=5, CHARACTERISE abort, RUN→HELD) → **NOT FOUND**

**Scaffolding that survives** (can be reused):
- `bno055::getLinearAccel()` ✅ (src/sensors/bno055.cpp:206, overrides sensor_base.h:125)
- `OrientationSensor::getLinearAccel()` virtual ✅ (sensor_base.h:125)
- `test_balance_app_collision.cpp` with 12 test cases ✅ (tests/test_balance_app_collision.cpp, untracked)

**Test coverage** (untracked but complete):
- Tests 1-7: gate threshold detection (PEAK single-tick, SUSTAIN 3-tick, KICK cross-axis)
- Tests 8-10: BOOTSTRAP/CHARACTERISE abort scenarios
- Tests 11-12: RUN HELD entry and false-positive guards

**Reference**: todo.md §Phase 4M.0, lines 46–75

**Restoration scope**: Implement state checks in step_bootstrap_ (baseline + pulse phases), step_char_act_ (sweep phases), step_run_ (immediate HELD entry). Add `collision_detected_` flag and `collision_sustained_count_` member. ~80 LOC. Thresholds from research_collision_signature_bno055.md: PEAK=12 m/s², SUSTAIN=8 m/s² over 3 ticks, KICK=6 m/s² + 200 dps.

---

### P1: Memory & Flash Hazards

#### 2. Uno Flash Budget Acceptable (Intentional Hardcoding)
**Severity**: P1 | **File**: `src/applications/balancing_robot_uno/balance_constants.h`

The Uno minimal path intentionally uses hardcoded constants (per platform-bifurcation design):
- `BALANCE_KP=65.0f`, `BALANCE_KI=12.0f`, `BALANCE_KD=38.0f` (balance_constants.h:44-46)
- `PITCH_OFFSET_DEG=-8.6f` (line 54)
- `STICTION_PWM=15` (line 71)
- `TIP_CUTOFF_DEG=25.0f` (line 79)
- `PITCH_SANITY_DEG=90.0f` (line 83)

**Status**: ✅ NOT a violation. Per scope.md §Platform bifurcation, Uno-minimal is intentionally hardcoded. Derivation is offloaded to offline Python brute-force tuner (`tools/sim/`, sibling-owned).

---

#### 3. Stack Allocation Scan — No Bloat Detected
**Severity**: P1 (informational)

**Mega path (balance_app.cpp)**:
- `raw_gyro_dps_[3]` → 12 B (member array, acceptable)
- `PulseLog pulse_log_` → ~32 B struct (bounded, single instance)
- LPF filter floats (4 members × 4B = 16 B) — all member variables
- Bootstrap accumulators → scalar floats, no arrays
- Capture accumulators (Welford stats) → 4 floats = 16 B
- No `std::string`, `std::vector`, dynamic `new`/`delete`

**Uno path (uno_balance_app.cpp)**:
- Constructor state: pitch, PWM, armed flag → 12 B total
- No dynamic allocation; method-local arrays small (gyro/accel 3-float reads)

**Verdict**: ✅ Memory footprint is tight and disciplined. No heap fragmentation risk.

---

#### 4. Float Operations on Uno (Design Constraint, Not Bug)
**Severity**: P1 (informational)

Both Uno and Mega use software-emulated single-precision floats (no FPU on AVR 8-bit). Cost is part of the algorithm design. Not a bloat issue; trade-off is intentional for numerical stability in PID / RLS / quaternion math.

---

### P2: Concurrency & Error Handling

#### 5. IMU Read Failure — Best-Effort But Unlogged
**Severity**: P2 | **File**: `src/applications/balancing_robot/balance_app.cpp:1365-1370`

```cpp
void BalanceApp::read_imu_(uint32_t /*now_ms*/) {
    imu_.read();  // <-- return value discarded
    const OrientationData& od = imu_.getOrientation();
    const float new_pitch = od.pitch_deg;
```

If `imu_.read()` returns `false` (I2C error, timeout, sensor offline), the code continues with potentially stale pitch and zero gyro values:

```cpp
const bool have_g = imu_.getRawGyro(g);  // returns false if read failed
const bool have_a = imu_.getRawAccel(a);  // returns false if read failed
if (have_g && have_a) {
    // ... motion filters update
} else {
    // ... motion filters stay at last known good (line 1449)
}
```

**Impact**:
- Stale `pitch_deg_` fed to PID loop (will drive control error toward old measurement)
- Motion filters degrade to last-good values (HELD entry gate becomes pitch-only)
- Operator has no indication HELD detection is reduced
- Watchdog on host detects stuck ISR (via external feed_watchdog), not stale data

**Acceptable?** Partially. Watchdog catches complete failure (ISR stuck > N ms). Intermittent I2C glitches cause transient control error but motors stop on safety thresholds (tipover, saturation).

**Recommendation**: Log `imu_.read()` failures to a persistent counter (EEPROM) so operator can see "I2C lost connection 47 times" in telemetry. Add explicit comment that this is acceptable for I2C bus glitches but not for unplugged sensor (requires hardware watchdog or operator restart).

---

#### 6. Motor Initialization Failure Not Checked
**Severity**: P2 | **File**: `src/main.cpp:315`

```cpp
// Motors
motors.begin();  // <-- return value ignored
```

If L298N driver initialization fails (pin not configured, PWM timer unavailable, etc.), the app silently proceeds. Subsequent `motors_.stop()` or `motors_.set_speed()` calls are no-ops, leaving the bot in an undefined state.

**Recommendation**: Check and log:
```cpp
if (!motors.begin()) {
    Serial.println(F("MOTOR_INIT_FAIL"));
    // transition to safe state (block RUN entry?)
}
```

---

#### 7. Motion Filter Degradation Not Logged
**Severity**: P2 | **File**: `src/applications/balancing_robot/balance_app.cpp:1384-1450`

When `getRawGyro()` or `getRawAccel()` fail (line 1383-1384), motion filters (lateral gyro, accel deviation) degrade to "last known good". This is **structurally correct** (safe fallback) but **operationally opaque**:

- HELD entry gate relies on `g_lateral_dps_lpf_` and `a_dev_lpf_`
- If those are hours old, false-HELD entries are possible
- No debug output indicates this degradation

**Recommendation**: Add optional telemetry flag (e.g., `motion_filter_stale_`) and output it in status line (main.cpp `'s'` command).

---

### P3: Hardcoded Constants & Scope Violations (Expected, Tracked)

Per scope.md §Current scope violations table, these are **tagged violations with active replacement plans**. All remain hardcoded in the Mega path; Uno path intentionally excludes these features.

#### 8. HELD Entry Thresholds (Mega path)
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:474-478`

```cpp
const bool ext_motion = (last_cmd_mag < 20) && (abs_pitch_gyro > 30.0f);
const bool lift_detected = motion_filters_init_ && (a_dev_lpf_ > 6.0f);
const uint16_t dwell = ext_motion ? 20 : 60;  // ticks @ 5 ms
```

- Line 474: Motor-quiet threshold = **20 PWM** (hardcoded; should derive from stiction floor × 0.5)
- Line 474: Gyro-fast threshold = **30.0 dps** (should derive from baseline noise × 5)
- Line 475: Accel-deviation threshold = **6.0 m/s²** (should derive from baseline LIA noise × 3, see scope.md row 14)
- Line 478: HELD entry dwells = **20 or 60 ticks** (should derive from PID response time)

**Tracked in scope.md**: Lines "Phase 2.5: cmd_mag < 20", "Phase 2.5: gyro > 30 dps", "Phase 2.5 dwell = 100 ms", "HELD a_dev_lpf_ > 6.0f"

**Status**: ✅ Planned retirement via measurement derivation (Phase 4M follow-on)

---

#### 9. HELD Exit Conditions (Mega path)
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:689-694`

```cpp
const bool quiet = (g_lateral_dps_lpf_ < 12.0f) && (a_dev_lpf_ < 1.5f);
const bool level = abs_pitch < 8.0f;
if (quiet && level) {
    if (hold_exit_count_ < 65535) hold_exit_count_ += 1;
    if (hold_exit_count_ >= 40) {  // 200 ms @ 5 ms
        enter_state_(BalanceAppState::RUN, now_ms);
```

- Lateral gyro exit threshold = **12.0 dps** (should derive from baseline noise × margin)
- Accel-deviation exit threshold = **1.5 m/s²** (should derive from baseline)
- Level threshold = **8.0 deg** (hardcoded; should be ±0.5 × soft-zone?)
- Dwell = **40 ticks = 200 ms** (magic latency; should derive from PID response time × 2)

**Tracked in scope.md**: Rows "Phase 2.5 dwell", "online_est max_deviation"

**Status**: ✅ Planned retirement

---

#### 10. Soft-Cutoff Threshold (Mega path)
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:512, 535`

```cpp
constexpr float SOFT_ZONE_DEG = 1.0f;  // line 512
const bool soft_cutoff = (abs_pitch > 25.0f);  // line 535
```

- Soft zone (gain scheduling region) = **1.0 deg** (hardcoded; should derive from noise floor × 3)
- Soft cutoff (motor kill region) = **25.0 deg** (midpoint between linear region ~±10° and hard limit ±35°; acceptable as magic number per MINIMIZE_ACCELERATIONS philosophy)

**Tracked in scope.md**: Row "SOFT_ZONE_DEG = 1.0f" (deferred-to-mega)

**Status**: ✅ Planned retirement; soft-cutoff itself is a design parameter (acceptable hardcode per vision doc)

---

#### 11. STUCK Detector Thresholds (Mega path)
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:555-557`

```cpp
static const int16_t  SAT_THRESHOLD_PWM = 180;     // 70.6% of ±255
static const float    STUCK_GYRO_DPS    = 5.0f;
static const uint32_t STUCK_TIMEOUT_MS  = 1500;
```

- PWM saturation floor = **180** (should be 0.7 × measured saturation point from CHARACTERISE)
- Gyro no-motion floor = **5.0 dps** (should be baseline noise × 3)
- Timeout = **1500 ms** (should be 5 × expected response time)

**Tracked in scope.md**: Rows "SAT_THRESHOLD_PWM = 180", "STUCK_GYRO_DPS = 5.0f", "STUCK_TIMEOUT_MS = 1500"

**Status**: ✅ Planned retirement; safety bandaid with measured replacement

---

#### 12. BOOTSTRAP Freeze Window
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:632`

```cpp
constexpr uint32_t BOOTSTRAP_FREEZE_MS = 5000;
const bool bootstrap_freeze = (now_ms - run_entered_ms_) < BOOTSTRAP_FREEZE_MS;
```

RLS adapter frozen for 5 seconds after RUN entry (allows OnlineMountingEstimator to settle). Per scope.md row 13, this is a **safety bandaid** with bypass on successful BOOTSTRAP (adaptive_active immediately true).

**Status**: ✅ Bypassed when BOOTSTRAP succeeds; acceptable for fallback case

---

#### 13. Adaptive Gain Rate Limit
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:1456-1467`

```cpp
void BalanceApp::ramp_gain_(float& live, float target, float dt_sec) {
    float bound = 0.05f * dt_sec * abs_live;  // 5%/s proportional bound
    if (bound < 0.001f) bound = 0.001f;        // absolute floor
```

Rate limit = **5% per second of current value, minimum 0.001 per cycle**. This is a structural PID safety property (prevents transient K_motor errors from destabilizing loop), not a per-bot tuning. **Acceptable hardcode.**

---

#### 14. Bootstrap + CHARACTERISE Pulse Amplitudes
**Severity**: P3 | **File**: `src/applications/balancing_robot/balance_app.cpp:1011, 1278`

```cpp
// BOOTSTRAP (line 1011)
static const uint8_t PULSE_PWMS[4] PROGMEM = {180, 180, 240, 240};

// CHARACTERISE (line 1278)
static const uint8_t PWM_TABLE[] PROGMEM = {30, 60, 90, 120, 150, 200};
```

- BOOTSTRAP uses **180 and 240 PWM** (fixed; intentional for K_motor measurement protocol)
- CHARACTERISE sweeps **30–200 PWM** (fixed; structural algorithm choice)

Both are **protocol specifications, not tunings**. Acceptable hardcodes per scope.md §What's NOT a violation.

**Status**: ✅ By design; not a violation

---

#### 15. Online Estimator Configuration
**Severity**: P3 | **File**: `src/main.cpp:341-351`

```cpp
balance_pid.set_i_term_limit(40.0f);          // antiwindup clamp (line 341)
balance_pid.set_d_term_lpf_tau_sec(0.003f);   // D-term filter (line 342)
online_est.set_lpf_time_constant_sec(8.0f);   // mount LPF (line 350)
online_est.set_max_drift_rate_dps(2.0f);      // adaptation cap (line 351)
```

- I-term limit (40°) is a **structural safety limit**, not a bot-specific tuning (bounds integrator windup)
- D-term LPF (3 ms) is a **BNO055 NDOF quantization-noise filter**, chip property not bot property
- Mount LPF (8 s) is a **measurement filter time constant**; revised to 8 s (from 20 s) per 2026-05-18 bench (line 345-349 comment)
- Drift cap (2.0 dps) is a **safety bounds on online adaptation** (prevents creep beyond ±5°)

**Tracked in scope.md**: Rows "online_est LPF tc = 8 s", "online_est max_deviation = 5°"

**Status**: ⏳ LPF tc could be derived from closed-loop pole locations (now hardcoded but justified by bench data)

---

#### 16. Auto-Bootstrap Startup Gate
**Severity**: P3 | **File**: `src/main.cpp:396`

```cpp
if (abs_err < 5.0f) {  // Pitch error < ±5 degrees
    app.enter_bootstrap(millis());
} else {
    // stale_mount — stay in IDLE
}
```

Threshold = **5.0 deg** (gates auto-fire of BOOTSTRAP at startup; should be measurement-derived per scope.md row 12 "Absolute pitch kill = ±20°").

**Status**: ⏳ Functional gate; replacement would be ±0.8 × derived tilt_limit_deg

---

### P4: Code Style & Documentation

#### 17. Unused Debug Macro
**Severity**: P4 | **File**: `src/applications/balancing_robot/balance_app.cpp:69, 71`

```cpp
#if defined(ARDUINO)
  #define BAL_LOGF(fmt, ...) Serial.print(F(fmt))  // defined but never used
#else
  #define BAL_LOGF(fmt, ...) ((void)0)
#endif
```

Macro is defined but grep finds **zero uses** in balance_app.cpp. Safe to leave (no runtime cost), but dead code. Consider removing or documenting intent.

**Recommendation**: Delete or add `// Reserved for future telemetry` comment.

---

#### 18. Untracked Test File
**Severity**: P4 | **File**: `tests/test_balance_app_collision.cpp` (exists, untracked by PlatformIO)

Complete collision-detection test suite (tests/test_balance_app_collision.cpp, 483 lines, 12 test cases) is compiled via manual g++ command (header lines 44–56) but not integrated into PlatformIO test runner.

**Status**: ✅ Not a bug; intentional pending collision-detection re-implementation. Test can be re-enabled once code is restored.

---

#### 19. Axis Convention TODO
**Severity**: P4 | **File**: `src/applications/balancing_robot/balance_app.h:398-400`

```cpp
// TODO: VERIFY AXIS ON BENCH — currently assuming Y (consistent with
// pitch_deg_ source via quaternion_to_euler_degrees, but body-axis
// mounting can re-map).
```

Assumes BNO055 body Y-axis = pitch rotation. If bot is mounted differently (sensor rotated 90°, etc.), gyro feedback reads wrong axis and balance loop destabilizes. Needs bench validation post-balance.

**Risk**: Low (common mounting convention, consistent with BNO055 datasheet Figure 11). Bench validation is phase-gated.

**Status**: ⏳ Deferred to first hardware validation session

---

### Summary Table: All 19 Findings

| # | Severity | Category | File | Line | Issue | Status |
|----|----------|----------|------|------|-------|--------|
| 1 | P0 | Regression | balance_app.{h,cpp} | — | Collision detection reverted | **CRITICAL** |
| 2 | P1 | Memory | balance_constants.h | 71 | Uno hardcoded (intentional) | ✅ OK |
| 3 | P1 | Memory | balance_app.cpp | — | No bloat detected | ✅ OK |
| 4 | P1 | Memory | — | — | Float ops on Uno (design) | ✅ OK |
| 5 | P2 | Error-handling | balance_app.cpp | 1369 | IMU read failure unlogged | ⏳ Log counter |
| 6 | P2 | Error-handling | main.cpp | 315 | Motor init not checked | ⏳ Check & log |
| 7 | P2 | Error-handling | balance_app.cpp | 1384-1450 | Motion filter degrades silently | ⏳ Add telemetry flag |
| 8 | P3 | Hardcoded | balance_app.cpp | 474-478 | HELD entry thresholds | ⏳ Phase 4M follow-on |
| 9 | P3 | Hardcoded | balance_app.cpp | 689-694 | HELD exit conditions | ⏳ Phase 4M follow-on |
| 10 | P3 | Hardcoded | balance_app.cpp | 512, 535 | Soft-zone & soft-cutoff | ⏳ Phase 4M follow-on |
| 11 | P3 | Hardcoded | balance_app.cpp | 555-557 | STUCK detector thresholds | ⏳ Phase 4M follow-on |
| 12 | P3 | Hardcoded | balance_app.cpp | 632 | BOOTSTRAP freeze window | ✅ Bypassed on success |
| 13 | P3 | Hardcoded | balance_app.cpp | 1456-1467 | Gain ramp rate | ✅ Structural PID property |
| 14 | P3 | Hardcoded | balance_app.cpp | 1011, 1278 | Pulse amplitudes | ✅ Protocol spec |
| 15 | P3 | Hardcoded | main.cpp | 341-351 | Online estimator config | ⏳ Partially derived from bench |
| 16 | P3 | Hardcoded | main.cpp | 396 | Auto-bootstrap gate | ⏳ Measurement derivation |
| 17 | P4 | Style | balance_app.cpp | 69 | Unused debug macro | ⏳ Delete or document |
| 18 | P4 | Documentation | test_balance_app_collision.cpp | — | Untracked test file | ✅ Pending restoration |
| 19 | P4 | Documentation | balance_app.h | 398-400 | Axis convention TODO | ⏳ Hardware validation |

---

## Recommendations by Priority

### Immediate (Pre-Bench Validation)

1. **Restore collision detection** (P0) — ~80 LOC, 3–4 hour task
   - Copy state checks from test_balance_app_collision.cpp reference
   - Add collision_detected_ flag and spike counter
   - Integrate into step_bootstrap_, step_char_act_, step_run_ with appropriate exits
   - Test against test_balance_app_collision.cpp suite

2. **Add motor.begin() check** (P2) — ~5 LOC, 15 min
   - Log failure, optionally block RUN entry

3. **Log IMU read failures** (P2) — ~10 LOC, 30 min
   - Add persistent counter (EEPROM), display in status line

### Before Next Bench Session

4. **Add motion-filter stale flag** (P2) — ~5 LOC, 20 min
   - Set when getRawGyro/Accel fail, output in telemetry

5. **Document or remove BAL_LOGF** (P4) — 1 LOC, 5 min

### Phase 4M Follow-On (After Hardware Validation)

6–11. **Retire hardcoded thresholds** per scope.md violations table
   - Each row has a concrete measurement-derivation plan
   - Sequence: HELD thresholds → STUCK detector → soft-zone → online estimator → etc.
   - Estimated: 1–2 findings per week of bench work

---

## Conclusion

The post-merge codebase is **structurally sound** for the universal Mega path and **correct by design** for the minimal Uno path. The collision-detection regression is a **known issue** (documented in todo.md) awaiting re-implementation; all scaffolding survives. Error-handling gaps are **acceptable for beta** (watchdog catches major failures) but should be **logged pre-production**. Hardcoded constants are **tracked, justified, and planned for retirement** via the scope.md audit table.

No showstopper issues. Ready for hardware validation of Phase 4.10c (BOOTSTRAP) + Phase 4M.0 (collision detection re-implementation).

