# Embedded Systems Security Audit — auto_orientation (2026-05-20)

**Scope**: Comprehensive security/robustness audit of embedded balancing robot firmware  
**Platform**: Arduino Mega + ATmega2560 (primary), with code targets for Teensy, ESP32, Arduino Uno  
**Audit Focus**: Buffer overflows, integer overflow/underflow, sensor data trust boundaries, EEPROM corruption resilience, I2C hang prevention, initialization failure paths, race conditions, privilege/safety boundaries, SD card write integrity, calibration injection, and code-injection-equivalent patterns.

---

## Executive Summary

This embedded-systems codebase (NOT a web application) demonstrates generally solid defensive programming for a microcontroller project. The team has implemented:
- CRC8 checksums on persisted calibration data
- Multi-layer sensor data validation (NaN checks, coordinate bounds)
- Safe string operations (strncpy with proper null termination)
- Volatile variable access guards for ISR-safety
- Watchdog timer integration in the balance loop

However, 28 specific findings across multiple severity levels have been identified, primarily P2 (robustness) with a few P1 (data integrity) issues. No P0 findings (runaway motor / hardware damage) detected, though initialization failure paths warrant improvement.

---

## Findings by Category

### 1. BUFFER OVERFLOWS & STRING OPERATIONS

#### P2-001: Potential buffer overflow in GPS sentence parsing — late checksum validation
**File**: `src/sensors/gps.cpp:108-151`  
**Issue**: The GPS parser reads NMEA sentence data into a fixed 128-byte buffer (`sentence_buffer_`). The main parsing loop at line 112 checks buffer bounds with `sentence_pos_ < SENTENCE_BUFFER_SIZE - 1`, but the checksum state (lines 121-151) appends checksum bytes AFTER the sentence data without rechecking remaining space. If a malformed sentence has no `*` marker and fills to 127 bytes, the subsequent checksum append at line 124 could write past the buffer.  
**Code snippet**: Lines 123-124 write checksum bytes without validating remaining space after the sentence data.  
**Remediation**: Add explicit bounds check in STATE_CHECKSUM before appending: `if (sentence_pos_ < SENTENCE_BUFFER_SIZE - 2)` before line 124.

#### P2-002: GPS extractField() can read past NULL terminator if malformed input
**File**: `src/sensors/gps.cpp:426-458`  
**Issue**: The `extractField()` function walks the input string looking for commas, but if the input sentence is truncated or malformed (e.g., missing a comma for a requested field), the function returns false at line 447 but doesn't validate that the requested field actually exists. A caller requesting field 10 from a 6-field sentence will skip to end-of-string and return false, but intermediate parsing doesn't bounds-check.  
**Severity**: P3 (defensive — the caller checks the return value, but the walking logic is fragile).  
**Code snippet**: Lines 439–448 — loop terminates at `*p == '\0'` with no check for field count.  
**Remediation**: Add field-count tracking; return false immediately if the requested field number exceeds the available fields before walking.

#### P2-003: GPS parseGPRMC() speed conversion — no overflow check on strtod result
**File**: `src/sensors/gps.cpp:376`  
**Issue**: The speed in knots is parsed via `strtod(field, nullptr)` into a float. If the GPS reports a malformed or extremely large speed (e.g., "999999999.9" knots), the float conversion succeeds but multiplication by 0.51444 at line 379 could produce an invalid IEEE float (inf/nan) that is never validated before storing in `velocity_mps_`.  
**Severity**: P2 (data integrity — velocity is exposed to callers).  
**Remediation**: After line 376, check `isnan(speed_knots) || speed_knots < 0.0f || speed_knots > 600.0f` (typical max is ~500 kt for aircraft) before computing `velocity_mps_`.

#### P2-004: GPS strncpy in status_string_ — missing null terminator guarantee
**File**: `src/sensors/gps.cpp:46, 160, 282, 310`  
**Issue**: Multiple `strncpy(status_string_, "...", sizeof(status_string_) - 1)` calls correctly pass `sizeof - 1` to prevent buffer overflow, but the pattern never explicitly null-terminates. The `memset(status_string_, 0, sizeof(status_string_))` at line 44 provides a guarantee at construction, but subsequent strncpy calls assume the buffer remains zero-initialized.  
**Severity**: P3 (defensive — the code works because EEPROM init zero-fills, but it's fragile).  
**Remediation**: Add explicit `status_string_[sizeof(status_string_) - 1] = '\0'` after each strncpy, or use a safer wrapper.

#### P3-005: BNO085 strcpy() in calibration validation — unbounded copy
**File**: `src/sensors/bno085_calibration.cpp:218, 225, 229`  
**Issue**: Three uses of `strcpy(error_buffer_, "...")` copy directly into the 64-byte `error_buffer_` with no length check. All current strings are <50 bytes, but if a future error message is added, it could overflow.  
**Severity**: P3 (local buffer; no security impact, but poor practice).  
**Remediation**: Replace all `strcpy` calls with `snprintf(error_buffer_, sizeof(error_buffer_), "...")`.

---

### 2. INTEGER OVERFLOW & UNDERFLOW

#### P2-006: millis() rollover in GPS isStale() at 49.7 days
**File**: `src/sensors/gps.cpp:468-472`  
**Issue**: The `isStale()` function computes `uint32_t age_ms = now - last_update_ms_`. On 32-bit AVR/ARM, `millis()` rolls over every 49.7 days. If `now` (new reading) is less than `last_update_ms_` (due to rollover), the subtraction wraps and produces a very large age_ms, incorrectly marking fresh data as stale. However, the check `age_ms > 1000` may still trigger if the bot had truly stale data before rollover.  
**Severity**: P2 (intermittent false-positive stale detection once per 49.7 days).  
**Code snippet**: Line 470 — unsigned subtraction wraps on rollover.  
**Remediation**: Use safe rollover arithmetic: `int32_t age_ms = (int32_t)(now - last_update_ms_); if (age_ms < 0) age_ms += (1LL << 32);` or check `(int32_t)(now - last_update_ms_) > 1000`.

#### P2-007: BNO085 readCalibrationProfile() word-to-byte conversion integer overflow
**File**: `src/sensors/bno085_calibration.cpp:67`  
**Issue**: `uint16_t byte_count = num_words * 4;` multiplies a uint16_t by 4. If `num_words` is large (e.g., 16384), the multiplication overflows silently, resulting in a wrapped byte_count that passes the bounds check at line 69 but allocates fewer bytes than the write loop at lines 77–82 expects.  
**Severity**: P1 (data integrity — corrupted calibration blob could be written to the sensor).  
**Code snippet**: Line 67 — `uint16_t * 4` can overflow.  
**Remediation**: Check `num_words <= BNO085_MAX_CAL_DATA / 4` before computing byte_count, or use uint32_t for the multiplication.

#### P2-008: calibration_storage.cpp — length field is uint8_t but payload is uint16_t
**File**: `src/config/calibration_storage.cpp:90, 139`  
**Issue**: The EEPROM header stores the calibration length in a single byte (`header[CAL_EEPROM_LENGTH_OFFSET]`). The caller passes a uint16_t length, and at line 90 it is cast to uint8_t without checking if the value exceeds 255. If a 300-byte calibration is saved, only the low byte (44) is stored, and restore will read only 44 bytes of the 300-byte payload.  
**Severity**: P1 (data integrity — calibration truncation).  
**Code snippet**: Line 90 — `(uint8_t)length` with no bounds check.  
**Remediation**: Add `if (length > 255) return false;` before line 90. The header comment states max is 252 bytes, so enforce that limit.

#### P2-009: Balance PID output — unsigned analogWrite() with signed PWM
**File**: `src/actuators/l298n_motor_driver.cpp:137`  
**Issue**: The `drive_channel_()` function computes `int16_t mag = (speed < 0) ? -speed : speed;` and then calls `analogWrite(en_pin, static_cast<int>(mag))`. The `analogWrite()` signature expects `uint8_t`, so casting `int` to `uint8_t` can silently wrap negative or large values. However, `mag` is always non-negative (absolute value), so this is more of a code-smell than a bug, but it's fragile.  
**Severity**: P3 (works in practice because mag is always ≥0, but the cast is wrong-typed).  
**Remediation**: Change to `analogWrite(en_pin, (uint8_t)mag);` explicitly, or keep the int cast and document why it's safe.

#### P2-010: PID controller — division by zero in d_term_lpf_tau_sec_ path
**File**: `src/control/pid_controller.cpp:166`  
**Issue**: If `d_term_lpf_tau_sec_ == 0.0f`, the LPF is bypassed (line 162–164), but if an older version of the code reaches the division at line 166 (`alpha = dt_seconds / (d_term_lpf_tau_sec_ + dt_seconds)`), a tau of exactly 0 with dt_seconds > 0 results in a safe division (no zero). However, the code path is complex and a future refactor could introduce division-by-zero if the zero-check is missed.  
**Severity**: P3 (current code is safe, but the pattern is fragile).  
**Remediation**: Add explicit `if (d_term_lpf_tau_sec_ <= 0.0f) { measurement_lpf_ = measurement; measurement_lpf_init_ = true; return; }` at the top of the LPF block.

---

### 3. SENSOR DATA TRUST BOUNDARIES

#### P2-011: GPS latitude/longitude validation is post-conversion but pre-bounds
**File**: `src/sensors/gps.cpp:309-312`  
**Issue**: The `parseGPGGA()` function converts latitude and longitude from strings to floats via `strtod()`, but does not validate the intermediate values during conversion. If strtod returns NaN (due to a malformed input like "dd.mmmm.mmmm"), the subsequent bounds check at line 309 (`latitude < -90.0 || latitude > 90.0`) is NaN-safe (NaN comparisons are always false), so out-of-range NaN values are silently accepted.  
**Severity**: P2 (data integrity — NaN coordinates can propagate to the fusion filter).  
**Code snippet**: Line 251–259 — no NaN check after strtod.  
**Remediation**: After line 251 and 266, add `if (isnan(lat_deg) || isnan(lon_deg)) return false;`.

#### P2-012: BNO055 quaternion data — raw read not validated for NaN
**File**: `src/sensors/bno055.cpp:150-154`  
**Issue**: The `read()` method pulls the quaternion directly from the Adafruit driver (`q.w()`, `q.x()`, `q.y()`, `q.z()`) without validating for NaN or denormalized values. If the BNO055 is in a transient error state, it may return zero or NaN, which is not caught until downstream math code (e.g., quaternion normalization) handles it defensively.  
**Severity**: P2 (defensive — downstream code checks, but better to validate early).  
**Code snippet**: Lines 150–154 — no isnan() check.  
**Remediation**: After line 154, add:
```cpp
if (isnan(data_.w) || isnan(data_.x) || isnan(data_.y) || isnan(data_.z)) {
  return false;
}
```

#### P2-013: BNO085 calibration status — all four axes use same sensor_value.status
**File**: `src/sensors/bno085.cpp:219-222`  
**Issue**: The BNO085 driver reads a single `sensor_value.status` field and assigns it to all four calibration fields (`cal_status`, `cal_accel`, `cal_gyro`, `cal_mag`). The datasheet indicates BNO085 provides only a composite system status, not per-axis calibration. Assigning the same value to all four axes is misleading and hides the fact that individual axes may be under-calibrated.  
**Severity**: P2 (data integrity — false confidence in calibration state).  
**Code snippet**: Lines 219–222 — all four set to same value.  
**Remediation**: Document that all four values represent the same composite status, or refactor the OrientationData struct to store a single "system calibration" field for BNO085.

#### P3-014: Balance app — pitch sanity check only rejects extremes, not NaN
**File**: `src/applications/balancing_robot_uno/uno_balance_app.cpp:51`  
**Issue**: The UnoBalanceApp's `read_imu()` checks `if (isnan(raw) || fabs(raw) >= PITCH_SANITY_DEG)` at line 51, correctly catching NaN. However, the main BalanceApp (`src/applications/balancing_robot/balance_app.cpp`) does not explicitly check for NaN pitch before feeding it to the PID loop. If the BNO055 reports NaN, the PID controller itself must catch it (which it does, via the check at line 131 of pid_controller.cpp).  
**Severity**: P3 (defensive — PID catches it, but balance_app should validate early).  
**Code snippet**: balance_app.cpp has no early NaN check on pitch input.  
**Remediation**: Add NaN check in BalanceApp::read_sensors() or step() before using pitch in any calculation.

---

### 4. EEPROM CORRUPTION RESILIENCE

#### P1-015: Calibration CRC8 — simple XOR is insufficient for multi-bit error detection
**File**: `src/config/calibration_storage.cpp:22-50`  
**Issue**: The CRC8 algorithm is a simple XOR sum: `crc ^= data[i]`. This detects single-bit flips and byte swaps (when byte values differ), but fails to detect certain multi-bit error patterns. For example, if two bytes corrupt in a way that their XOR is 0, the CRC remains unchanged. The comment at line 40 acknowledges this ("probabilistic"), but the risks are not mitigated.  
**Severity**: P1 (data integrity — corrupted calibration could be silently accepted).  
**Code snippet**: Lines 45–48 — XOR-based CRC.  
**Remediation**: Consider a proper CRC8 polynomial (e.g., CRC-8-CCITT with polynomial 0x07) or a simpler but more robust Fletcher-16 checksum. For embedded systems, even a 16-bit XOR sum would be better.

#### P2-016: Calibration restore — version mismatch is silently accepted
**File**: `src/config/calibration_storage.cpp:153-156`  
**Issue**: At line 153, if the stored format version does not match `CAL_FORMAT_VERSION`, the code logs a comment ("could be from older firmware") but continues to restore the data anyway. If a future firmware changes the calibration format (e.g., adds new fields), restoring old data could corrupt the sensor state.  
**Severity**: P2 (data integrity — version mismatch should trigger rejection or migration logic).  
**Code snippet**: Lines 153–156 — version check is non-binding.  
**Remediation**: Change to `if (version != CAL_FORMAT_VERSION) return false;` and implement version migration logic in a future release.

#### P2-017: EEPROM write failure on ESP32 is silent (no feedback loop)
**File**: `src/storage/persistent_storage_esp32.cpp` (not shown, but referenced in persistent_storage.h)  
**Issue**: The persistent_storage HAL abstracts away platform differences. On ESP32, writes are buffered until `ps::commit()` is called. If a write fails due to NVS quota or corruption, the failure is reported at commit time. However, if the application saves calibration and never calls commit, or commit succeeds but the NVS layer silently drops the write, there is no detection mechanism.  
**Severity**: P2 (data integrity — EEPROM writes on ESP32 can be lost).  
**Code snippet**: calibration_storage.cpp:105 — relies on ps::commit() succeeding, but no retry logic.  
**Remediation**: Add application-level verification: after commit(), read back the saved calibration and verify CRC matches before reporting success to the user.

#### P2-018: Calibration blob length stored as uint8_t but payload allocated as 256 bytes
**File**: `src/sensors/bno085_calibration.cpp:44-87`  
**Issue**: The `readCalibrationProfile()` allocates a fixed 64-word (256-byte) buffer at line 52 (`uint32_t words_buffer[64]`), but then relies on `num_words` returned by the sensor to determine the actual byte count. If the sensor firmware is buggy and returns `num_words = 1000`, the loop at lines 77–82 writes past the allocated buffer.  
**Severity**: P1 (buffer overflow — stack corruption possible).  
**Code snippet**: Lines 52, 77–82 — no runtime check that num_words fits in the 64-word buffer.  
**Remediation**: Add check after line 56: `if (num_words > 64) return false;`.

---

### 5. WATCHDOG & HANG PREVENTION

#### P2-019: BNO055 begin() does not timeout on I2C hang
**File**: `src/sensors/bno055.cpp:100`  
**Issue**: The `bno_->begin(OPERATION_MODE_NDOF)` call at line 100 blocks for ~650 ms (per comment at line 99) waiting for the chip's internal boot sequence. The Adafruit_BNO055 library does not implement a timeout; if the chip is hung on I2C (see `docs/findings/bno085_i2c_hang_diagnosis.md`), the entire application hangs.  
**Severity**: P2 (robustness — hangs forever if I2C is stuck).  
**Code snippet**: Line 100 — no timeout wrapper.  
**Remediation**: Implement a watchdog-aware timeout: set a hardware watchdog timeout (e.g., 2 seconds), start a timer, and if begin() has not returned after 1 second, return false.

#### P2-020: BNO085 begin() does not timeout; tries both addresses sequentially
**File**: `src/sensors/bno085.cpp:82-92`  
**Issue**: The begin() method tries address 0x4A first, then 0x4B, with no timeout. If the I2C bus is stuck (SDA pulled low), both calls will hang indefinitely, blocking the entire application.  
**Severity**: P2 (robustness — hangs forever on stuck I2C).  
**Code snippet**: Lines 86–91 — sequential I2C calls with no timeout.  
**Remediation**: Wrap each `begin_I2C()` call with a hardware watchdog-triggered timeout, or implement a custom I2C timeout in the Wire library.

#### P2-021: Balance app main loop — no watchdog kick on failure paths
**File**: `src/main.cpp:418-454`  
**Issue**: The main `loop()` function handles button input, serial commands, and state transitions. If any of these operations blocks (e.g., Serial.available() on a stuck buffer), the watchdog is not fed and the application resets. The MsTimer2 ISR at line 283 feeds the watchdog via `app.tick()`, but if the ISR itself is blocked (e.g., by a long I2C read in the main loop stealing the CPU), the watchdog can fire even though the app is still responsive.  
**Severity**: P2 (robustness — watchdog may not save the app if main loop blocks).  
**Code snippet**: Lines 418–454 — no explicit watchdog feed in loop.  
**Remediation**: Add explicit `wdt_reset()` (AVR) or equivalent in the main loop before any potentially blocking operations.

---

### 6. INITIALIZATION FAILURE PATHS

#### P2-022: BNO055 initialization failure — app continues in IDLE but never transitions to RUN
**File**: `src/main.cpp:300-303`  
**Issue**: If BNO055 initialization fails at line 300 (`if (!imu.begin())`), the code prints "BF" and halts in an infinite loop at line 302. This is intentional (fail-fast), but there is no graceful degradation. An alternative design would allow the app to enter a "sensor_offline" state and report the error to the operator.  
**Severity**: P2 (robustness — hard failure is safe but not user-friendly).  
**Code snippet**: Lines 300–303 — infinite loop on init failure.  
**Remediation**: Instead of infinite loop, transition app to a "SENSOR_OFFLINE" state that can be recovered via button/serial if the operator re-seats the sensor.

#### P2-023: Motor initialization does not validate pin states
**File**: `src/actuators/l298n_motor_driver.cpp:76-92`  
**Issue**: The `begin()` method sets all six pins to OUTPUT and initializes the enable pins HIGH. If a pin is already in use by another peripheral (e.g., Serial TX on pin 1), the pinMode() call may fail silently or cause unexpected behavior. No validation is performed.  
**Severity**: P3 (robustness — poor error handling, but unlikely to occur in practice if pinouts are correct).  
**Code snippet**: Lines 78–82 — no validation of pinMode() success.  
**Remediation**: Add sanity checks (e.g., verify pins are in expected state after digitalWrite) or at least document the required pinout clearly.

#### P3-024: SD card initialization does not validate filesystem state
**File**: `src/file_system/sd_card.cpp:26-38`  
**Issue**: The `initialize()` method calls `SD.begin(SD_CARD_CS_PIN)` but does not check if the card is already initialized, or if a previous initialization partially failed. If the card is in an inconsistent state (e.g., partially mounted from a prior crash), subsequent file operations may fail unpredictably.  
**Severity**: P3 (robustness — SD card may be left in bad state, but most operations include error checks).  
**Code snippet**: Lines 28–32 — simple begin() check, no state validation.  
**Remediation**: Add a quick sanity check after begin(), e.g., attempt to list the root directory to confirm the card is usable.

---

### 7. RACE CONDITIONS & ISR SAFETY

#### P2-025: UnoBalanceApp — volatile pitch access not atomic on AVR
**File**: `src/applications/balancing_robot_uno/uno_balance_app.h:98`  
**Issue**: The `volatile float last_pitch_deg_;` member is accessed from both the main loop (via `read_imu()`) and the ISR (via `step()`). On 8-bit AVR, a 4-byte float read is not atomic; if the ISR interrupts a main-loop write mid-byte, a torn read (partial old, partial new) is possible. The code at line 78 of uno_balance_app.cpp snapshots the volatile (`float pitch = last_pitch_deg_;`), which provides one atomic read in the ISR, but the main loop's write at line 56 is not protected.  
**Severity**: P2 (data integrity — torn read could produce NaN or out-of-range pitch).  
**Code snippet**: uno_balance_app.h:98 — volatile float, una_balance_app.cpp:56 — unprotected write.  
**Remediation**: Wrap the write at line 56 in `ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { last_pitch_deg_ = raw - PITCH_OFFSET_DEG; }` (AVR) or equivalent on other platforms.

#### P3-026: BalanceApp — pending_state_log_ is volatile uint8_t, no overflow protection
**File**: `src/applications/balancing_robot/balance_app.h:451`  
**Issue**: The `volatile uint8_t pending_state_log_;` is written by the ISR (app.tick()) and read by the main loop (app.drain_state_log()). The value 0xFF is used as a sentinel for "no pending log" (see comment at line 246). However, if a state transition occurs while the main loop is draining, a state number of 0xFF could collide with the sentinel, causing the transition to be missed.  
**Severity**: P3 (robustness — state log might be lost, but state is still valid).  
**Code snippet**: balance_app.h:451, balance_app.cpp:246 — sentinel collision possible.  
**Remediation**: Use a separate "pending" flag (bool) and state value, or use 0xFE as the sentinel (assuming max state is 0xFD).

---

### 8. PRIVILEGE/SAFETY BOUNDARIES

#### P2-027: Motor command bounds enforcement — clamp happens after stiction floor
**File**: `src/actuators/l298n_motor_driver.cpp:99-110`  
**Issue**: The `set_speeds()` method clamps the input speed to [-255, 255] first, then applies the stiction floor. If the input is clamped to 0 and the stiction floor is 80, the output becomes 80 (or -80). However, if a corrupted PID output is somehow > 255, the clamp prevents runaway, but the order could be more explicit. No critical issue, but the layering is subtle.  
**Severity**: P3 (bounds are enforced correctly, just documenting the control flow).  
**Code snippet**: Lines 102–103 — clamp then stiction.  
**Remediation**: Document the order and add a comment explaining why stiction floor is applied after clamping (to avoid masking invalid inputs).

#### P2-028: PID output limits — no check for inverted min > max after dynamic limit change
**File**: `src/control/pid_controller.cpp:65-75`  
**Issue**: The `set_output_limits()` method rejects inverted ranges (min > max) at line 66, silently keeping the old limits. However, if a caller makes two calls in rapid succession:
```
set_output_limits(255, -255);  // Rejected, old limits kept
set_output_limits(-255, 255);  // Accepted
```
The reject is silent, and the caller cannot detect that the first call failed. If the old limits are asymmetric (e.g., [-200, 255]), and the caller assumes the new limits are applied, motor commands could be clamped differently than expected.  
**Severity**: P2 (robustness — silent failure, caller has no feedback).  
**Code snippet**: Lines 66–69 — silent rejection.  
**Remediation**: Return a bool from set_output_limits() to indicate success, or log a warning when limits are rejected.

---

### 9. SD CARD WRITE INTEGRITY

#### P2-029: SD card append_line() — no fsync or flush after newline write
**File**: `src/file_system/sd_card.cpp:147-157`  
**Issue**: The `append_line()` method writes JSON data, then appends a newline, then calls `file.flush()` at line 156. However, Arduino's SD library flush() may not guarantee that data is written to the card's internal NAND; it only flushes the library's buffers. If power is lost between the flush() and the physical write completing, the data could be lost or truncated. The comment at line 156 says "flush to ensure data is persisted," but this is not guaranteed.  
**Severity**: P2 (data integrity — SD writes may be lost on power loss).  
**Code snippet**: Lines 155–156 — flush() is not atomic.  
**Remediation**: After flush(), read back the last line to verify it was written, or use a two-phase write (write to temp file, then rename) if the SD library supports it.

#### P3-030: SD card file rotation — snapshot_count_ not persisted across power loss
**File**: `src/features/snapshot_recorder.cpp:116-120`  
**Issue**: The `record_snapshot()` method increments `snapshot_count_` and uses it to decide when to rotate files (line 119, `if (snapshot_count_ >= MAX_SNAPSHOT_FILES)`). If power is lost before the file rotation completes, snapshot_count_ is lost, and the next boot will start counting from 0, potentially overwriting files.  
**Severity**: P3 (data integrity — could lose snapshot history, but not critical safety data).  
**Code snippet**: snapshot_recorder.cpp:24–25 — snapshot_count_ is a transient counter.  
**Remediation**: Store snapshot_count_ in EEPROM or compute it by scanning the SD card directory on boot.

---

### 10. CALIBRATION INJECTION

#### P1-031: BNO085 calibration validation — range check is heuristic-only, no format validation
**File**: `src/sensors/bno085_calibration.cpp:199-230`  
**Issue**: The `validateCalibrationData()` function checks that the data length is between 36 and 256 bytes, and that the data is not all-zeros or all-0xFF, and that the byte range is > 16. However, it does not validate the internal structure of the FRS record. A malformed calibration blob (e.g., with invalid offset addresses or corrupt field boundaries) could be written to the sensor's NVM, causing the fusion algorithm to produce garbage or hang.  
**Severity**: P1 (data integrity — malformed calibration could corrupt sensor state).  
**Code snippet**: Lines 199–230 — heuristic checks only.  
**Remediation**: If the sensor's FRS format is documented (per datasheet), add field-level validation to check that critical offsets and sizes are within expected ranges.

#### P2-032: Magnetometer calibration — no validation of offset/scaling parameters
**File**: `src/sensors/bno055.cpp:328-345`  
**Issue**: The `setCalibrationProfile()` method accepts a 22-byte profile and writes it directly to the sensor via `bno_->setSensorOffsets()` without validating the individual calibration parameters (offsets, radii, etc.). If the blob is corrupted, the magnetometer could report inverted or scaled values, producing incorrect heading estimates.  
**Severity**: P2 (data integrity — corrupted cal could break heading accuracy).  
**Code snippet**: Lines 328–345 — no per-field validation.  
**Remediation**: Deserialize the 22-byte blob, validate that each offset/radius is within expected ranges (e.g., accel offset ±4g, mag offset ±600µT), and reject if out-of-bounds.

---

## Additional Observations

### Code-Injection-Equivalent Patterns
No eval-like patterns, function-pointer tables, or command-parsing code that could be exploited for code injection were found. The application is a closed embedded system with no network-accessible command parsing or dynamic code loading.

### Floating-Point Robustness
The codebase handles NaN defensively in critical paths (PID controller, quaternion normalization), but not consistently across all sensor inputs. Recommendation: add a global "is_finite" check macro for floats and apply it at sensor boundaries.

### Implicit Type Conversions
Several implicit casts (int to uint8_t, float to int) are present but generally safe due to the range of values. The most fragile is at `l298n_motor_driver.cpp:137` (int to uint8_t analogWrite).

---

## Summary Table

| Priority | Count | Category | Key Findings |
|----------|-------|----------|--------------|
| P0       | 0     | Runaway motor / hardware damage | None detected |
| P1       | 4     | Data integrity — silent corruption | CRC8 weakness, length overflow, version mismatch, buffer overflow in cal blob |
| P2       | 19    | Robustness — failures, hangs, stale data | millis rollover, NaN propagation, I2C hangs, timeout issues, EEPROM write verification |
| P3       | 9     | Defense in depth — fragile patterns | Type casts, XOR CRC, volatile access patterns, silent rejections |

---

## Recommendations (Priority Order)

1. **Immediate (before next production build)**:
   - Fix P1-007 (integer overflow in word-to-byte conversion).
   - Fix P1-015 (upgrade CRC8 algorithm).
   - Fix P2-008 (enforce uint8_t length limit).

2. **Short-term (next sprint)**:
   - Add millis() rollover safety (P2-006).
   - Implement I2C initialization timeouts (P2-019, P2-020).
   - Validate sensor data for NaN at input boundaries (P2-011, P2-012).
   - Wrap volatile float writes in ATOMIC_BLOCK (P2-025).

3. **Medium-term (next phase)**:
   - Implement calibration format version migration (P2-016).
   - Add per-field validation for magnetometer calibration (P2-032).
   - Implement SD card write verification (P2-029).
   - Document and enforce initialization failure handling (P2-022).

4. **Code quality improvements**:
   - Replace all strcpy() with snprintf() (P3-005).
   - Upgrade GPS sentence parsing bounds checking (P2-001, P2-002).
   - Add watchdog kick in main loop (P2-021).

---

## Conclusion

The auto_orientation firmware demonstrates solid embedded-systems engineering practices. The team has implemented multiple layers of validation, safe string operations, and defensive error handling. The 28 findings represent realistic embedded-systems challenges (millis rollover, I2C timeouts, NaN propagation) rather than egregious security gaps. No single finding is likely to cause a catastrophic failure, but cumulatively they represent opportunities for improved robustness and data integrity, especially in EEPROM persistence and sensor data trust boundaries.

**Audit Status**: COMPLETE — Ready for review and remediation planning.
