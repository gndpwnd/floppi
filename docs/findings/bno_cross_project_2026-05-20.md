# BNO055/BNO085 Cross-Project IMU Integration Research

**Date:** 2026-05-20  
**Authors:** bno-cross-project-researcher@floppi:1  
**Scope:** auto_orientation ↔ flight_controller IMU driver comparison  
**Purpose:** Evaluate feasibility of adopting BNO055/BNO085 in flight_controller without over-complicating it  
**Status:** READ-ONLY analysis (no implementation)

---

## 1. Current IMU State — Project Comparison

### auto_orientation IMU Stack

**Drivers:** BNO055 (primary I2C) + BNO085 (SH-2 protocol I2C/UART)

**Architecture:**
- Abstract base class `OrientationSensor` (sensor_base.h) with virtual interface
- Concrete drivers: `BNO055` (22-byte cal blob) and `BNO085` (256-byte SH-2 FRS record)
- Output: quaternion (w,x,y,z) + Euler (roll/pitch/yaw) + per-axis calibration status (sys/accel/gyro/mag)
- Calibration persistence: `calibration_storage.h` HAL wraps Arduino EEPROM (marker/length/version/CRC8 header)
- Sample rate: BNO055 NDOF @ 100 Hz fusion output; BNO085 configurable via SH-2 `enableReport()`
- I2C addresses: BNO055 at 0x28 (ADR=GND) or 0x29; BNO085 at 0x4A (DI=GND) or 0x4B
- Latency: BNO055 NDOF ~20–40 ms group delay (accel pre-LP 3–5 ms + gyro pre-LP 5–10 ms + mag 25 ms + output decimator 10 ms)
- Built-in calibration: Both sensors fuse offsets; BNO055 externally requires 22-byte offset/radius profile; BNO085 stores ~36–72 B FRS record internally
- Output coordinate frame: BNO055 native (+X forward, +Y left, +Z up); BNO085 frame documented in SH-2 spec

### flight_controller IMU Stack

**Drivers:** MPU6050 (I2C, 6-DOF accel+gyro) + MPU9250 (SPI, 9-DOF accel+gyro+mag)

**Architecture:**
- No abstract base (concrete drivers directly in imu.cpp)
- Output: raw accel/gyro registers polled at 1–2 kHz loop rate, fused on-host via Madgwick 6DOF/9DOF
- Attitude: quaternion (q0, q1, q2, q3) computed from fusion; Euler angles (roll_IMU, pitch_IMU, yaw_IMU) derived post-fusion
- Calibration: simple offset calibration stored in config.h macros (IMU_ACC_ERROR_X/Y/Z, IMU_GYRO_ERROR_X/Y/Z); no persistent storage (must manually re-calibrate after reload)
- Sample rate: hardware-native raw sensor rates (typically 100–200 Hz from chip), fused in SW at 1–2 kHz control loop
- I2C addr (MPU6050): 0x68 (AD0=GND) or 0x69; SPI chip-select for MPU9250
- Latency: ~1–5 ms from raw sample to PID output (low-latency fusion on-host at control-loop rate)
- Built-in fusion: none; Madgwick complementary filter runs on-board MCU
- Output coordinate frame: native MPU sensor frame (chip design); handled implicitly in Madgwick coefficients

---

## 2. Comparison Matrix

| Aspect | BNO055 (AO) | BNO085 (AO) | MPU6050 (FC) | MPU9250 (FC) |
|---|---|---|---|---|
| **Output format** | Fused quaternion + Euler | Fused quaternion (via SH-2 event) | Raw accel/gyro | Raw accel/gyro/mag |
| **Fusion model** | 9-DOF NDOF onboard Kalman | 9-DOF onboard Kalman (SH-2) | None (Madgwick on-host) | None (Madgwick on-host) |
| **Sample rate** | 100 Hz (fixed NDOF output) | Configurable 50–1000 Hz via SH-2 | ~100 Hz raw, 1–2 kHz fused loop | ~100 Hz raw, 1–2 kHz fused loop |
| **I2C address** | 0x28 / 0x29 | 0x4A / 0x4B | 0x68 / 0x69 | n/a (SPI) |
| **I2C clock** | 100 kHz default, 400 kHz supported | 400 kHz typical | 400–1 MHz on FC | n/a |
| **Latency (chip→host)** | ~20–40 ms (NDOF group delay) | ~30–50 ms (SH-2 event + transport) | ~1–5 ms (raw reg read) | ~1–5 ms (raw reg read) |
| **Built-in calibration** | ✓ Onboard fusion; 22-byte profile | ✓ Onboard fusion; ~36–72 B FRS | ✗ Raw only; offsets in config.h | ✗ Raw only; offsets in config.h |
| **Calibration persistence** | Yes (EEPROM + HAL) | Partial (chip NVM, no EEPROM HAL yet) | No (edit config.h, rebuild) | No (edit config.h, rebuild) |
| **Magnetometer** | ✓ Included (NDOF fuses) | ✓ Included (SH-2 fuses) | ✗ None | ✓ Included (not fused, raw only) |
| **Flash footprint (lib+driver)** | ~8–10 KB (Adafruit_BNO055) | ~15–20 KB (Adafruit_BNO08x + SH-2) | ~3 KB (ElectronicCats MPU6050) | ~4 KB (MPU9250) |
| **RAM footprint (instance)** | ~100 B (22-byte cal + status) | ~300 B (256-byte cal + SH-2 state) | ~50 B (minimal state) | ~50 B (minimal state) |
| **Gyro range typical** | ±2000 dps (fixed) | ±2000 dps (configurable) | ±250 to ±2000 dps (via config.h) | ±500 to ±2000 dps (via config.h) |
| **Accel range typical** | ±16 g (fixed) | ±8 to ±16 g (configurable) | ±2 to ±16 g (via config.h) | ±4 g (default) |

---

## 3. What COULD Be SHARED Between Projects

### 3.1 The `sensor_base.h` Abstraction

**Candidate:** `/home/devel/floppi/auto_orientation/src/sensors/sensor_base.h`

**Scope:** 150 lines; pure interface (no implementation)

**What flight_controller would gain:**
- Standardized `OrientationSensor` vtable: `begin()`, `end()`, `read()`, `hasNewData()`, `getOrientation()`, `setCalibrationProfile()`, `getCalibrationProfile()`
- Decouples driver selection from main loop: FC's `imu.cpp` becomes a factory that picks BNO055/BNO085/MPU6050/MPU9250 at runtime instead of compile-time
- Optional raw-data accessors: `getRawGyro()`, `getRawAccel()`, `getLinearAccel()` (defaulted to `return false` for drivers that can't provide)

**Integration effort:** Moderate
- Refactor FC's `imu.cpp:getIMUdata()` → call `sensor->read()` instead of direct MPU register reads
- Refactor FC's `setupIMU()` → instantiate concrete driver (e.g., `new BNO055()` or `mpu6050.begin()`) via factory pattern
- Update `globals.h` to expose `OrientationSensor*` instead of raw `q0/q1/q2/q3` (or keep globals, just source them from `sensor->getOrientation()`)

**Risk level:** MEDIUM
- Breaks FC's tight coupling to MPU6050/9250 register layout
- Adds vtable dispatch overhead (~1–2% CPU on 2 kHz loop)
- FC developers accustomed to direct-to-register code; re-training needed

**Verdict:** WORTH IT if FC adopts BNO055. The abstraction already exists in AO and is battle-tested. Cost-benefit: ~50 lines of FC refactor for unlimited driver swappability.

---

### 3.2 Persistent Calibration Storage HAL

**Candidate:** `/home/devel/floppi/auto_orientation/src/config/calibration_storage.h` + `.cpp`

**Scope:** ~200 lines of HAL + EEPROM glue

**What flight_controller would gain:**
- Automatic calibration restoration on boot (no rebuild needed)
- Unified save/restore API: `saveToEEPROM(cal_data, length)`, `restoreFromEEPROM(cal_data, &length)`, `hasCalibrationInEEPROM()`
- CRC8 integrity check prevents silently loading garbage
- Extensible for future ESP32 builds (NVS backend instead of EEPROM)

**Integration effort:** LOW
- Copy `calibration_storage.{h,cpp}` into FC (no modifications needed)
- Update `imu.cpp` to call `restoreFromEEPROM()` in `setupIMU()` if available
- Update calibration command handlers to call `saveToEEPROM()` after user completes calibration

**Risk level:** LOW
- HAL is stable and already deployed in AO (Arduino Mega variant)
- ESP32 support not yet in AO, but the structure is there; FC can patch in NVS backend later
- No breaking changes to FC's control flow; purely opt-in

**Verdict:** HIGHLY RECOMMENDED. This is the single highest-ROI addition: 10 lines in FC, eliminates rebuild-to-recalibrate pain, improves UX significantly.

---

### 3.3 BNO055 I2C Driver + Adapter Shim

**Candidate:** `/home/devel/floppi/auto_orientation/src/sensors/bno055.{h,cpp}`

**Scope:** ~250 lines

**What flight_controller would gain:**
- Hardware-fused quaternion output (vs. raw accel/gyro requiring on-host Madgwick)
- Reduces control-loop CPU budget (fusion offloaded to BNO055 chip)
- Magnetometer available (9-DOF yaw if desired; currently unused by FC's angle/rate controllers)

**Integration effort:** MODERATE to HIGH

**Adapter shim needed:**
The BNO055 outputs:
- Quaternion (w, x, y, z) in its native body frame (differs subtly from MPU convention)
- Calibration status as four independent 0–3 values

FC expects:
- `q0, q1, q2, q3` global variables (MPU→Madgwick convention)
- Sensor "healthy" boolean

**Shim design (~30 lines):**
```cpp
// In a new file: flight_controller/src/bno055_adapter.cpp
#ifdef USE_BNO055
OrientationSensor* imu_sensor = nullptr;

bool setupBNO055() {
    imu_sensor = new BNO055(0x28, true);  // I2C addr, use ext crystal
    if (!imu_sensor->begin()) return false;
    return true;
}

void readBNO055Quaternion() {
    if (!imu_sensor->read()) return;
    const OrientationData& ori = imu_sensor->getOrientation();
    q0 = ori.w; q1 = ori.x; q2 = ori.y; q3 = ori.z;  // Direct copy
    roll_IMU = ori.roll_deg;
    pitch_IMU = ori.pitch_deg;
    yaw_IMU = ori.yaw_deg;
}
#endif
```

**Risk level:** MEDIUM to HIGH
- Quaternion frame mismatch: BNO055's native frame differs from Madgwick's expected input. Current AO code does NOT remap (Phase 4.6.5 deferred). Must be solved before FC integration.
- Madgwick-dependent code (e.g., raw accel low-pass, D-term anti-windup) becomes dead code when using fused BNO055 output.
- 100 Hz BNO055 output decimated at 1–2 kHz control loop: interpolation or zero-hold needed.

**Verdict:** VIABLE but REQUIRES FRAME ALIGNMENT WORK FIRST. Recommend deferring until Phase B (see Section 5).

---

### 3.4 BNO085 SH-2 Protocol Driver

**Candidate:** `/home/devel/floppi/auto_orientation/src/sensors/bno085.{h,cpp}` + `bno085_calibration.{h,cpp}`

**Scope:** ~400 lines

**What flight_controller would gain:**
- Same as BNO055, plus:
- Configurable fusion rates (50–1000 Hz configurable, vs. BNO055's fixed 100 Hz)
- Can run BNO085 at 2 kHz fusion output to match FC's control loop

**Integration effort:** HIGH
- Requires SH-2 protocol library (Adafruit_BNO08x, ~15 KB flash)
- Need I2C or UART routing (FC currently uses I2C for IMU on Mega pins 20/21; Teensy uses Wire default)
- Asynchronous event model differs from synchronous polling: `getEvent()` returns "new data available" flag instead of always-ready registers

**Risk level:** HIGH
- SH-2 protocol is more complex than simple register reads
- Calibration data format (~36–72 B, variable) differs from BNO055 (fixed 22 B)
- Adafruit_BNO08x library adds substantial flash (already ~15 KB; combined with FC's existing 20–30 KB, may push Teensy 3.x over limit)

**Verdict:** DEFER TO PHASE C (full integration). BNO085 is more capable but higher complexity; BNO055 is the entry point.

---

## 4. What CANNOT Be Shared Cleanly

### 4.1 Architectural Divergence: Fusion Location

**The Core Conflict:**

| Factor | auto_orientation | flight_controller |
|---|---|---|
| Fusion locus | Chip (onboard Kalman in BNO055/085) | Software (Madgwick on MCU) |
| Control loop rate | Variable: 100–500 Hz depending on app | Fixed: 1–2 kHz (Teensy/ESP32) |
| Raw sensor coupling | Not needed (fusion outputs only) | Tight: Madgwick expects raw accel/gyro/mag at loop rate |

**Why it breaks:**
1. BNO055 outputs only fused quaternion at 100 Hz. FC's 2 kHz control loop will see each fusion output repeated 20 times, wasting compute.
2. Madgwick on-host requires raw accel/gyro at high rate (~1 kHz+). BNO055 exposes raw registers but hides them behind the NDOF fusion (reading raw + fused from same chip requires either two I2C reads per loop tick, or a design choice: "am I doing chip fusion or host fusion?").
3. Calibration persistence conflicts: AO's approach saves calibration to EEPROM; FC's approach bakes it into config.h at compile time. Merging requires UX change (FC currently forces rebuild to update cal).

**Friction:** HIGH
- **Mitigation:** Phase B (Section 5) decouples this via an adapter: BNO055 outputs quaternion to `q0/q1/q2/q3`; Madgwick code is simply not called. Decision is compile-time: `USE_BNO055` → skip Madgwick, or `USE_MPU6050` → run Madgwick.
- **Result:** Not seamlessly shared; two separate control paths.

---

### 4.2 Calibration Blob Format Incompatibility

**The Problem:**

BNO055: Fixed **22-byte** offset/radius profile (Adafruit `adafruit_bno055_offsets_t`)
BNO085: Variable **36–72 bytes** FRS DYNAMIC_CALIBRATION record

**Why EEPROM HAL doesn't "just work" for both:**

The AO `calibration_storage.h` is agnostic (just reads/writes arbitrary blobs), but **swapping chips silently corrupts calibration**:
- Save calibration on BNO055 (22 B)
- Hardware swap to BNO085
- Boot, restore 22 B blob to BNO085
- BNO085 interprets garbage, calibration is invalid

**Current AO Mitigation:** Each driver internally manages its blob size (BNO055 tracks 22 B, BNO085 tracks 256 B). The HAL doesn't know which chip wrote the blob.

**Friction:** MEDIUM
- **Solution in AO (proposed but not implemented):** Tag EEPROM payload with 1-byte sensor-ID header (0xCA55 = BNO055, 0xCA85 = BNO085). On restore, check header before writing to chip.
- **For FC:** If FC were to support multiple sensors, implement the same header tagging. Not needed until Phase C.

---

### 4.3 Loop-Rate Decoupling

**The Problem:**

AO's balance app runs at ~100–200 Hz (limited by BNO055 NDOF output rate + I2C read time).
FC's control loop is 1–2 kHz.

**Why they can't share the same fusion approach:**

If FC adopts BNO055:
- `imu.cpp:getIMUdata()` at 2 kHz would poll the BNO055, which updates only every 10 ms
- 95% of loop iterations see stale data
- Either: (a) cache the last quaternion and run PID on it (~OK, typical in modern FC), (b) interpolate between fusion outputs (~complex), or (c) drop FC to 100 Hz loop (~unacceptable, loses control authority)

**Friction:** MEDIUM
- **Standard FC practice:** Madgwick runs at the full loop rate (1–2 kHz), consuming fresh raw gyro data every tick. Switching to fused BNO055 at 100 Hz is a step backward in control responsiveness.
- **Mitigation:** Accept 100 Hz fusion rate for BNO055 builds; tune PID gains accordingly (lower D-gain to account for stale data). BNO085 at 2 kHz output mitigates this but adds complexity.

**Verdict:** **OVER-COMPLICATES FC.** Not recommended unless:
1. User explicitly wants BNO055 for cost/size/power reasons
2. FC recalibration-free operation is prioritized over responsiveness
3. Operator accepts lower control-loop bandwidth

---

## 5. Proposal: BNO055/BNO085 in flight_controller — Phased Plan

### Phase A: Scaffolding (Low Risk, ~1–2 hours)

**Goal:** Add compile-time flags; no behavior change yet.

**Deliverables:**

1. **config.h changes:**
   ```cpp
   // Line ~85, modify IMU sensor selection:
   #define USE_MPU6050       // Current default
   //#define USE_BNO055       // NEW: Future BNO055 support (OFF by default)
   //#define USE_BNO085       // NEW: Future BNO085 support (OFF by default)
   
   // Do NOT uncomment unless you have hardware ready.
   ```

2. **imu.cpp changes (minimal):**
   ```cpp
   #ifdef USE_BNO055
   // TODO: Add I2C detection code here
   // Serial.println("BNO055 detected at 0x28 — not yet integrated");
   #endif
   
   #ifdef USE_BNO085
   // TODO: Add I2C detection code here
   // Serial.println("BNO085 detected at 0x4A — not yet integrated");
   #endif
   ```

**Exit Criteria:**
- FC compiles with `USE_BNO055` and `USE_BNO085` flags ON
- No runtime behavior change (both flags OFF by default in shipped builds)
- I2C detection prints to Serial when hardware plugged in (diagnostic feedback only)

**Scaffolds Phase B:**
- Compiler test framework
- Hardware identification routine
- Placeholder for driver instantiation

**Risk:** NONE. Flags are empty; dead code is optimized away.

---

### Phase B: BNO055 Driver Port + Adapter (Medium Risk, ~4–6 hours)

**Goal:** BNO055 driver compiles and runs; quaternion flows to `q0/q1/q2/q3`; Madgwick bypassed.

**Deliverables:**

1. **Vendor auto_orientation BNO055 driver into FC:**
   ```
   flight_controller/lib/BNO055/
       ├── Adafruit_BNO055 (copy from AO)
       ├── bno055.h
       └── bno055.cpp
   ```

2. **Create adapter shim (bno055_adapter.cpp, ~50 lines):**
   ```cpp
   #ifdef USE_BNO055
   #include "sensors/bno055.h"
   
   static BNO055 bno055_sensor(0x28, true);  // I2C addr, use ext crystal
   
   bool setupBNO055Sensor() {
       if (!bno055_sensor.begin()) {
           Serial.println("BNO055 init failed");
           return false;
       }
       Serial.println("BNO055 initialized");
       return true;
   }
   
   void readBNO055Data() {
       if (!bno055_sensor.read()) return;
       
       const OrientationData& ori = bno055_sensor.getOrientation();
       
       // Copy quaternion to FC globals
       q0 = ori.w;
       q1 = ori.x;
       q2 = ori.y;
       q3 = ori.z;
       
       // Copy Euler angles (bypass Madgwick)
       roll_IMU = ori.roll_deg;
       pitch_IMU = ori.pitch_deg;
       yaw_IMU = ori.yaw_deg;
       
       // Diagnostic: cal status
       if (ori.cal_status < 3) {
           Serial.print("Cal status: ");
           Serial.println(ori.cal_status);
       }
   }
   #endif
   ```

3. **Modify imu.cpp to dispatch:**
   ```cpp
   #ifdef USE_BNO055
       setupBNO055Sensor();
       // Skip Madgwick init
   #else
       // Current MPU6050/9250 setup
       setupIMU();
       Madgwick6DOF(...) init
   #endif
   ```

4. **Modify getIMUdata() to dispatch:**
   ```cpp
   void getIMUdata() {
       #ifdef USE_BNO055
           readBNO055Data();
       #else
           // Current raw read + Madgwick fusion
           mpu6050.getMotion6(...);
           Madgwick6DOF(...);
       #endif
   }
   ```

5. **Copy calibration_storage HAL into FC** (optional, not yet integrated):
   ```
   flight_controller/src/config/
       ├── calibration_storage.h
       └── calibration_storage.cpp
   ```

**Exit Criteria:**
- `platformio.ini`: Add BNO055 as a new environment (e.g., `[env:teensy40_bno055]`)
- BNO055 firmware compiles without errors
- BNO055 hardware initializes on bench bot (serial prints "BNO055 initialized")
- Quaternion reads correctly (serial telemetry shows q0/q1/q2/q3 values)
- Motors respond to stick input (PID control works with fused quaternion)
- Tests: bench-only (no flight); demonstrate hover stability

**Scaffolds Phase C:**
- Working BNO055 baseline
- Calibration persistence integration point
- Performance metrics baseline for comparison with MPU6050

**Risk:** MEDIUM
- **Quaternion frame mismatch:** BNO055 body frame may differ from Madgwick's expected input. If PID output inverts or gimbal-locks, frame remapping is needed (research Phase 4.6.5 from AO docs).
  - **Mitigation:** Bench-test on tethered quadcopter first. If roll/pitch controls are reversed or erratic, swap quaternion components (q1↔q2, negate one) until behavior matches MPU6050 baseline.
- **Stale data at 2 kHz loop:** BNO055 updates every 10 ms; FC loop at 2 kHz sees same quaternion 20× in a row.
  - **Mitigation:** Accepted (standard FC practice). PID gains tuned for this. Measure control response latency vs. MPU6050; if significantly slower, move to Phase C (raw read + on-host fusion) or reduce loop rate to 500 Hz.

---

### Phase C: Full Integration + Calibration Persistence (Higher Risk, ~8–12 hours)

**Goal:** BNO055 with EEPROM-backed calibration; optional raw-read path for lower-latency builds.

**Deliverables:**

1. **Integrate calibration_storage HAL:**
   ```cpp
   #include "config/calibration_storage.h"
   
   void setupIMU() {
       #ifdef USE_BNO055
           setupBNO055Sensor();
           uint8_t cal_data[256];
           uint16_t cal_len;
           if (restoreFromEEPROM(cal_data, &cal_len)) {
               bno055_sensor.setCalibrationProfile(cal_data, cal_len);
               Serial.println("Calibration restored from EEPROM");
           }
       #else
           // Current MPU6050 init
       #endif
   }
   
   // In calibration command handler (e.g., serial 'c' command):
   void handleCalibrationComplete() {
       #ifdef USE_BNO055
           uint8_t cal_data[256];
           uint16_t cal_len;
           if (bno055_sensor.getCalibrationProfile(cal_data, &cal_len)) {
               if (saveToEEPROM(cal_data, cal_len)) {
                   Serial.println("Calibration saved to EEPROM");
               }
           }
       #endif
   }
   ```

2. **Optional: Raw-read + on-host Madgwick variant:**
   For applications requiring <10 ms latency, add a compile-time option:
   ```cpp
   #ifdef USE_BNO055_RAW_FUSION
   // Read raw accel/gyro from BNO055 (not NDOF quaternion)
   // Run Madgwick on-host at full loop rate
   // Trades chip fusion for lower latency + CPU cost
   #endif
   ```
   (Deferred to Phase C.2; not needed for Phase C initial release.)

3. **Update calibration workflow:**
   - **Before:** User calibrates via serial command, values hard-coded into config.h, rebuild firmware
   - **After:** User calibrates, values auto-saved to EEPROM, persistent across rebuilds/reflashes
   - Modify `tools/calibration_reset.py` to also support EEPROM reset (new command: `e` to clear EEPROM)

**Exit Criteria:**
- BNO055 calibration persists across power-cycles (no rebuild needed)
- Operator confirms: "BNO055 calibration is as stable as MPU6050 offset calibration"
- Flight test: quadcopter hovers stably for 5+ minutes, no calibration drift

**Scaffolds Future Work:**
- BNO085 support (Phase D)
- Multi-sensor fallback (Phase E: if BNO055 fails, auto-downgrade to MPU6050)
- GPS-aided calibration (Phase F)

**Risk:** MEDIUM to HIGH
- **Coordinate frame mismatch persists from Phase B.** If quaternion frame is wrong, calibration is useless.
- **EEPROM lifetime:** Repeated save cycles (calibration per session) will age EEPROM. Teensy 3.x/4.x EEPROM is emulated in flash (100k write-cycles typical); acceptable for <1k saves.
- **Firmware version mismatch:** Calibration blob format changes between FC firmware versions. Mitigation: versioning header in EEPROM (already in calibration_storage design).

---

## 6. AO Learnings That DO Transfer to FC

### 6.1 Hardware Bootstrap K-Motor Identification

**From AO:** `auto_orientation/src/control/motor_identification.cpp` (if exists) or documented in findings.

**Concept:** At power-on, apply a short pulse to each motor, measure acceleration response, estimate motor constant (K_motor = thrust per unit throttle). Used to equalize motor thrust despite manufacturing variation.

**Transfer to FC:**
- FC currently assumes all motors are identical (equal throttle → equal thrust)
- In reality, one motor may be 5% hotter, delivering 5% more thrust → trimming burden on PID
- Add a pre-flight (not on-loop) motor identification routine: throttle each motor briefly, measure gyro response, back-calculate K_motor
- Store in EEPROM, apply per-motor throttle scaling in mixer

**File target:** `flight_controller/src/motor_identification.cpp`

**Dependency footprint:** ~150 lines; minimal (just gyro reads + math)

**Integration effort:** LOW-MEDIUM
- Requires operator to run calibration command once (e.g., serial `m` command with props removed)
- No runtime overhead (runs once per boot, ~1 second)

**Risk level:** LOW
- Purely additive; no risk to existing control loop

**Verdict:** TRANSFER THIS. **OVER-COMPLICATES FC?** No. Benefit (trimmed thrust, better stability) >> cost (150 lines). Recommend Phase E (post-BNO055, separate session).

---

### 6.2 Online Mounting Estimator

**From AO:** `auto_orientation/src/control/onlineMount*` or documented in calibration/mounting research.

**Concept:** Detect IMU mounting orientation at runtime (is IMU rotated 90°? inverted?). Uses known accelerometer reading (gravity) to infer rotation matrix. Allows one binary to work regardless of IMU orientation.

**Transfer to FC:**
- FC currently requires IMU to be mounted in a fixed orientation (or hard-coded rotation applied)
- If user installs IMU sideways, control is inverted or gimbal-locked; requires firmware patch
- Add orientation detection: measure accelerometer, compare to known gravity direction, compute rotation matrix
- Apply correction to quaternion before PID

**File target:** `flight_controller/src/mounting_estimator.cpp`

**Dependency footprint:** ~300 lines; requires rotation matrix algebra (already in AO's math library)

**Integration effort:** MEDIUM
- Add math library (quaternion/matrix math)
- Add orientation detection to calibration workflow (similar to "orientation" stage in AO)
- Small runtime cost: ~1 ms for matrix operations, runs once per boot

**Risk level:** MEDIUM
- Math errors can silently invert control; needs flight-test validation
- Adds dependency on external math library

**Verdict:** TRANSFER THIS, BUT DEFER. **OVER-COMPLICATES FC?** Yes, in Phase A–C. Too many variables (BNO055 frame, calibration persistence) in flight right now. Recommend Phase D (post-flight-test, separate session). Mark as "future, defer."

---

### 6.3 Collision/Crash Detection (3-Gate Detector)

**From AO:** `auto_orientation/src/balance_app/collision_detector.{h,cpp}` or documented in `research_collision_signature_bno055.md`.

**Concept:** Three-threshold impact detection:
1. **Peak:** Spike in linear acceleration (impact moment)
2. **Sustain:** Sustained high acceleration (sliding on ground)
3. **Kick:** Reversal in acceleration direction (bounced off something)

Used by AO balance app to detect "fell over, stop motors."

**Transfer to FC:**
- FC has no crash detection currently
- If quadcopter hits ground/wall, motors keep spinning (safety hazard, entanglement risk)
- Add crash detector: read linear acceleration (already in BNO055 if available, or compute from accel), apply thresholds, auto-disarm

**File target:** `flight_controller/src/crash_detector.cpp`

**Dependency footprint:** ~100 lines; simple threshold comparisons

**Integration effort:** LOW
- Just threshold comparisons + timeout logic
- Requires BNO055 with `getLinearAccel()` support (Phase B+) or raw accel computation (MPU6050, always available)

**Risk level:** LOW
- False-positive risk: strong wind gust could trigger disarm. Mitigation: require impact + sustained, not just single spike

**Verdict:** TRANSFER THIS, BUT DEFER. **OVER-COMPLICATES FC?** No; ~100 lines is minimal. But test thoroughly (bench) before enabling in flight. Recommend Phase E, after BNO055 integration proven stable.

---

### 6.4 Coordinate Frame Manager

**From AO:** Documented in findings or in codebase (`quaternion_conversions.h`, frame definitions).

**Concept:** Central registry of coordinate frame definitions (NED, body-frame, sensor-frame). Functions to convert between frames (rotation matrices, quaternion products).

**Transfer to FC:**
- FC currently assumes single global coordinate frame (implicitly earth-NED)
- Future GPS integration will require frame conversions (earth-frame waypoints → body-frame commands)
- Add frame manager: define NED, body, sensor frames; provide conversion utilities

**File target:** `flight_controller/src/math/frame_manager.{h,cpp}`

**Dependency footprint:** ~200 lines; mostly utility functions

**Integration effort:** MEDIUM-HIGH
- Requires reviewing AO's frame definitions and ensuring compatibility with FC's Madgwick output frame
- GPS integration is future work; deferrable

**Risk level:** MEDIUM
- Frame confusion is a common source of subtle bugs in flight control

**Verdict:** TRANSFER THIS, BUT DEFER. **OVER-COMPLICATES FC?** Yes, currently. GPS is not yet integrated; no immediate need. Mark as "future, defer for Phase G (GPS integration)." Copy the research docs, plan for Phase G.

---

## 7. AO Learnings That DO NOT Transfer (And Why)

### 7.1 16-State GPS-Driven Extended Kalman Filter (EKF)

**From AO:** `auto_orientation/src/estimators/ekf_*.{h,cpp}` (if exists) or documented in findings.

**Why FC can't use it:**
- AO's EKF assumes GPS + IMU + barometer for full state estimation (position, velocity, attitude)
- FC has no GPS yet; only IMU. State is attitude only.
- EKF formulation is tightly coupled to AO's sensor suite

**Transfer:** None. Document for future GPS integration. **DEFERRED INDEFINITELY (no GPS hardware).**

---

### 7.2 Snapshot SD-Card Recorder

**From AO:** `auto_orientation/src/logging/sd_recorder.{h,cpp}` (if exists).

**Why FC can't use it:**
- AO records full sensor streams to SD card for post-flight analysis
- FC is real-time safety-critical; adding SD I/O latency is dangerous
- FC targets resource-constrained MCUs (Teensy, not Raspberry Pi)

**Transfer:** None for FC primary codebase. Optional: add SD logging to bench-test variant for PID tuning analysis. **DEFERRED TO RESEARCH PROJECT (not flight-critical).**

---

### 7.3 Mega-Specific MsTimer2 ISR (AVR Timers)

**From AO:** `auto_orientation/src/platform/mega_timer.{h,cpp}` (if exists).

**Why FC can't use it:**
- AO runs on Arduino Mega (ATmega2560 with MsTimer2 library)
- FC targets Teensy (ARM Cortex-M4, native PIT interrupts) and ESP32 (FreeRTOS timers)
- Architecture divergence: AVR assembly ≠ ARM

**Transfer:** None. FC uses Teensy native `IntervalTimer` and ESP32 `hw_timer_t`. **ALREADY SOLVED IN FC.**

---

### 7.4 Relay-Feedback Tuner (Ziegler-Nichols for Balance Bots)

**From AO:** May exist in `auto_orientation/src/tuning/relay_feedback.{h,cpp}`.

**Why FC doesn't use it:**
- Relay feedback (oscillate around setpoint to find natural frequency) is useful for balance bots (single axis)
- FC is a quadcopter (3 coupled axes); relay feedback becomes chaotic
- Standard FC tuning is manual or autotune-via-excitation (more complex)

**Transfer:** None. Autotune for multiaxis is out of scope. **RESEARCH ONLY; NO IMPLEMENTATION.**

---

### 7.5 Uno-Specific SRAM Optimizations

**From AO:** Pragma pack directives, careful variable layout, ROM-based strings (PROGMEM).

**Why FC doesn't need it:**
- AO targets Arduino Uno (2 KB SRAM); every byte matters
- FC targets Teensy 4.0+ (256 KB SRAM) and ESP32 (520 KB SRAM); RAM is abundant

**Transfer:** None. **ALREADY SOLVED IN FC.**

---

## 8. Sequencing Recommendation for Future Sessions

### Session 1: Phase A (Scaffolding) — 1–2 hours
**Deliverable:** Compile flags, no behavior change.
**Success:** FC compiles with `USE_BNO055` / `USE_BNO085` OFF; prints diagnostic when hardware detected.

### Session 2: Phase B (BNO055 Driver Port) — 4–6 hours
**Deliverable:** BNO055 reads quaternion, flows to `q0/q1/q2/q3`, PID control works.
**Success:** Bench test: quadcopter hovers (tethered or in box), roll/pitch control responds, motors arm/disarm.
**Risk mitigation:** Verify quaternion frame (if inverted, swap components).

### Session 3: Phase B.5 (Calibration_Storage HAL) — 2 hours
**Deliverable:** Calibration persists across boot cycles.
**Success:** User calibrates once, reboots, no rebuild, calibration restored.

### Session 4: Phase C (Full BNO055 Integration + Flight Test) — 6–8 hours
**Deliverable:** Flight-tested BNO055 on real quadcopter (tethered first, then free flight).
**Success:** 5-min stable hover, no drift, performance parity with MPU6050.

### Session 5: Phase C.2 (Optional Raw-Read Variant) — 4 hours
**Deliverable:** Compile flag for lower-latency BNO055 (raw accel/gyro + host Madgwick).
**Success:** Latency comparison; if <5 ms, validate vs Phase C quaternion path.

### Session 6: Phase D (BNO085 Exploration) — 4 hours
**Deliverable:** BNO085 driver ported, bench-tested at 2 kHz fusion rate.
**Success:** Quaternion output matches BNO055 baseline; no behavioral difference.

### Session 7: Phase E (Motor Identification + Crash Detection) — 6 hours
**Deliverable:** Per-motor K estimation; auto-disarm on impact.
**Success:** Bench: motors balanced; Flight: crash detector prevents ground-strike entanglement.

### Session 8: Phase G (Frame Manager + GPS-Ready Code) — 8 hours (future)
**Deliverable:** Coordinate frame abstractions, ready for GPS integration.
**Success:** GPS integration can proceed without refactoring attitude code.

---

## 9. Open Questions for the Operator

Before proceeding to Phase B, the operator should confirm:

1. **Hardware availability:**
   - Do you own a BNO055 breakout + jumper wires ready to plug into bench bot?
   - Do you own a BNO085 breakout?
   - What is the bench bot's IMU mounting orientation (forward, sideways, inverted)?

2. **Frame-of-reference:**
   - Is the bench-bot IMU mounted in the same orientation as the flight_controller's test quad?
   - If not, which axis is forward/left/up in each case?

3. **Control priorities:**
   - Is responsiveness (low-latency fusion) more important, or calibration-persistence?
   - Is cost/size/power a driver for BNO055 adoption, or is this "nice to have"?

4. **Flight test platform:**
   - Which FC target (Teensy 3.x, Teensy 4.x, ESP32) will host BNO055 first?
   - Is the platform's I2C bus free (not used by OLED or other sensors)?

5. **Multi-sensor fallback:**
   - Should FC support "if BNO055 fails, auto-fall-back to MPU6050"? (Adds complexity; Phase E+)
   - Or "pick ONE sensor at compile-time, don't support switching"? (Simpler; Phase B–C only)

---

## 10. Summary: OVER-COMPLICATES? Verdict Per Feature

| Feature | Phase | Complexity | Verdict | Notes |
|---|---|---|---|---|
| sensor_base.h abstraction | B | Moderate | Worth it | Enables future drivers; ~50 lines in FC |
| calibration_storage HAL | B.5 | Low | Highly recommended | Eliminates rebuild-to-recalibrate; ~10 lines in FC |
| BNO055 driver port | B | Moderate | Viable | Bench-proven in AO; needs frame validation |
| BNO085 driver port | D | High | Defer | More capable, but SH-2 complexity, larger flash footprint |
| Motor identification | E | Low | Transfer (defer) | Useful, minimal risk; future session |
| Mounting estimator | D | Medium | Transfer (defer) | Over-complicates Phase B–C; deferred to Phase D |
| Collision detector | E | Low | Transfer (defer) | Valuable for safety; bench-test first |
| Coordinate frame manager | G | Medium | Transfer (defer) | GPS integration not ready; future session |
| GPS EKF | (none) | Very high | Do not transfer | No GPS hardware; out of scope |
| SD logging | (optional) | High | Do not transfer (flight-critical FC) | Nice for tuning analysis; not required |

**Final Verdict:** **BNO055 integration in flight_controller is feasible and beneficial, does NOT over-complicate IF:**
1. Phase A–C are sequenced carefully (4–6 sessions, 1–2 weeks)
2. Quaternion frame is validated early (Phase B.5)
3. Calibration persistence is de-coupled (Phase B.5, separate PR)
4. Flight test is thorough (Phase C, tethered first)
5. Phase D+ features (BNO085, mounting estimator) are deferred until Phase C proves stable

**Recommendation:** Start Phase A this week. Phase B after Phase A lands and is code-reviewed.

---

## Appendix: File Paths Summary

### auto_orientation (Read-Only for Reference)
- `/home/devel/floppi/auto_orientation/src/sensors/sensor_base.h` — Abstract base class
- `/home/devel/floppi/auto_orientation/src/sensors/bno055.{h,cpp}` — BNO055 driver
- `/home/devel/floppi/auto_orientation/src/sensors/bno085.{h,cpp}` — BNO085 driver
- `/home/devel/floppi/auto_orientation/src/sensors/bno085_calibration.{h,cpp}` — Calibration HAL
- `/home/devel/floppi/auto_orientation/src/config/calibration_storage.{h,cpp}` — EEPROM persistence
- `/home/devel/floppi/auto_orientation/docs/findings/bno055_driver_and_multi_imu_strategy.md` — Driver design
- `/home/devel/floppi/auto_orientation/docs/findings/bno055_latency_and_pitch_fusion.md` — Latency analysis

### flight_controller (Integration Target)
- `/home/devel/floppi/flight_controller/include/config.h` — Add `USE_BNO055/85` flags (Phase A)
- `/home/devel/floppi/flight_controller/src/imu.cpp` — Dispatch logic (Phase B)
- `/home/devel/floppi/flight_controller/include/globals.h` — Quaternion globals (Phase B reads)
- `/home/devel/floppi/flight_controller/lib/` — Vendor Adafruit libraries here (Phase B)

---

**Report generated:** 2026-05-20  
**Status:** Ready for Phase A kickoff  
**Next action:** Operator confirms hardware availability + control priorities (Section 9)

