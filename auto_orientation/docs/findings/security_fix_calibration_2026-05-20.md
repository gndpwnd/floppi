# Security Fix Report — Calibration Data Integrity (4 P1 findings)

Date: 2026-05-20
Agent: security-fixer-calibration@floppi:1
Audit reference: `auto_orientation/docs/findings/audit_security_2026-05-20.md`

## Scope

Four P1 findings from the security audit, all in the calibration persistence
path:

| Audit ID                | Issue                                              |
|-------------------------|----------------------------------------------------|
| P1-015                  | CRC8 weakness — XOR sum misses multi-bit errors    |
| P1-007 (audit's P2-007) | Integer overflow in BNO085 word→byte conversion    |
| P1-008 (audit's P2-008) | Length field truncation (uint8_t header)           |
| P1-016 (audit's P2-016) | Version mismatch silently accepted                 |

The audit's priority-summary table at line 310 lists the 4 P1s as "CRC8
weakness, length overflow, version mismatch, buffer overflow in cal blob"
and the priority-ordered remediation list at lines 319-321 explicitly cites
P1-007, P1-015, P2-008 (length) for sprint 1 — so the P1/P2 number prefixes
in the body text are inconsistent with the actual severity column. I treated
the four issues called out in the brief as authoritative.

## Backward-compatibility decision

**Version bump (v1 → v2). Existing v1 EEPROM blobs are rejected outright;
operator must re-calibrate.**

Rationale: fixes 1 and 3 both change the on-disk format (different CRC
algorithm, different header layout). Maintaining a dual-path verifier for
both layouts would mean carrying two CRC implementations and two header
parsers indefinitely, with no clean point to retire the v1 code. Since
re-calibration is a documented operator-facing step (and the device already
walks the operator through it on a missing-cal boot), the version bump is
the cleaner option. The audit brief explicitly defaults to "version bump
unless audit specifies otherwise"; the audit does not specify.

## Fix details

### Fix 1 — CRC8 weakness (P1-015)

**File:** `auto_orientation/src/config/calibration_storage.cpp` (also `.h`)

**Before** (lines 22-50, .cpp):
```cpp
uint8_t calculateCRC8(const uint8_t* data, uint16_t length) {
  if (!data || length == 0) return 0;
  uint8_t crc = 0;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];           // <-- single-byte XOR, not a real CRC
  }
  return crc;
}
```

**After:**
```cpp
uint8_t calculateCRC8(const uint8_t* data, uint16_t length) {
  if (!data || length == 0) return 0;
  uint8_t crc = 0x00;
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x07);
      else            crc = (uint8_t)(crc << 1);
    }
  }
  return crc;
}
```

Algorithm: CRC-8-CCITT (polynomial 0x07, init 0x00, no reflection, no final
XOR). Bitwise implementation — no 256-byte table, ~120 bytes flash on AVR.
Audit ref: P1-015, lines 135-140.

### Fix 2 — Integer overflow in word→byte conversion (P1-007)

**File:** `auto_orientation/src/sensors/bno085_calibration.cpp`

**Before** (line 67):
```cpp
uint16_t byte_count = num_words * 4;       // <-- promoted to int, then
                                           //     truncated; wraps mod 2^16
if (byte_count > BNO085_MAX_CAL_DATA) { ... }
```

**After:**
```cpp
uint32_t byte_count_wide = (uint32_t)num_words * 4u;
if (byte_count_wide > BNO085_MAX_CAL_DATA) { ... }
uint16_t byte_count = (uint16_t)byte_count_wide;
```

Also added an explicit `num_words > WORDS_BUFFER_CAPACITY` guard immediately
after `sh2_getFrs()` returns, closing the related stack-overflow path called
out as P1-018 in the audit (the pack loop runs `i < num_words` against a
fixed 64-word stack buffer). This second guard is technically beyond the
4-P1 scope but lives at the same line and a fix to one without the other
leaves a live overflow — included for safety. Audit refs: P1-007 (lines
68-73), P1-018 (lines 156-161).

### Fix 3 — Length field truncation (P1-008)

**File:** `auto_orientation/src/config/calibration_storage.{cpp,h}`

**Before** (header v1 layout, .cpp line 90):
```cpp
header[CAL_EEPROM_LENGTH_OFFSET] = (uint8_t)length;   // <-- silently
                                                      //     truncates >255
```

Header was 4 bytes: marker(1), length(1), version(1), crc(1).

**After** (v2 layout, 6 bytes): marker(1), version(1), length_lo(1),
length_hi(1), crc(1), reserved(1). Save path stores `length` as a true
little-endian uint16_t; restore path reconstructs and validates against
`CAL_DATA_MAX_SIZE` (now 506, since header grew by 2 bytes within the
512-byte slot). All existing callers already pass `uint16_t length` so no
caller-side changes are required. Audit ref: P1-008, lines 75-80.

### Fix 4 — Version mismatch silently accepted (P1-016)

**File:** `auto_orientation/src/config/calibration_storage.cpp`

**Before** (lines 153-156):
```cpp
if (version != CAL_FORMAT_VERSION) {
  // Format mismatch - could be from older firmware
  // For now, accept it (future: could handle multiple versions)
}
```

**After:**
```cpp
if (version != CAL_FORMAT_VERSION) {
  return false;        // explicit rejection; operator re-calibrates
}
```

Audit ref: P1-016 (audit's P2-016), lines 142-147.

`CAL_FORMAT_VERSION` bumped from `0x01` to `0x02` in the header to make the
backward-incompatibility explicit. Both fix 1 (CRC algorithm) and fix 3
(header layout) ride this single bump.

## Build verification

| Environment                    | Status   | Notes                                |
|--------------------------------|----------|--------------------------------------|
| `mega_orientation_calibration` | SUCCESS  | Primary target — both edited files compile and link. RAM 83.4%, Flash 15.6%. |
| `arduino_uno_minimal`          | SUCCESS  | RAM 34.3%, Flash 50.0%.              |
| `native_test`                  | FAILED   | Unrelated — `balance_app.cpp` -Wswitch error on missing `PWM_DISCOVERY` case. Out of write zone; surfaced per brief instructions. |

The `native_test` failure pre-exists in `src/applications/balancing_robot/balance_app.cpp`
at lines 319 and 937 (switch-on-enum without handling `PWM_DISCOVERY`).
That file is outside the write zone and the failure is not caused by these
edits.

## Open items / deferred

- **P1-018 (BNO085 buffer overflow on bad num_words)** — fixed as a side
  effect of the integer-overflow patch because the bounds check now sits
  immediately after `sh2_getFrs()`. Flagged here so the test-writer agent
  knows a regression test is justified, even though it wasn't on the
  original 4-P1 list.
- **CRC verification on read-back (P2-017)** — out of scope for this pass.
  The `saveToEEPROM()` path does not yet re-read and verify the blob after
  `ps::commit()`. Worth a follow-up for ESP32, where NVS can drop writes
  silently.
- **BNO085 calibration format validation (P1-031)** — heuristic-only check
  in `validateCalibrationData()` is unchanged in this pass; it's a separate
  body of work (FRS field-level validation).
- **Magnetometer per-field validation (P2-032)** — separate file, separate
  agent.

## Test recommendations (for sibling test-writer agents)

Native (host-side) tests, all of which can run without hardware by linking
against a mocked `ps::` HAL:

1. **CRC-8-CCITT vector test.** Verify `calculateCRC8()` against known
   reference vectors:
   - `""` → 0x00
   - `"A"` → 0xC0 <!-- 2026-05-20: corrected from 0x20 (incorrect) — verified against impl and hand-computation by test_calibration_storage.cpp -->
   - `"123456789"` → 0xF4 (canonical CRC-8-CCITT check value)
2. **Round-trip save/restore.** Save random 36/72/200/506-byte payloads,
   restore, assert equality. The 506-byte case exercises the formerly-
   truncating high-byte path.
3. **Length-truncation regression.** Save a 300-byte payload; assert restore
   returns the same 300 bytes (not 44).
4. **Single-bit flip detection.** Save a payload, corrupt one bit in the
   stored EEPROM (via the mock HAL), assert restore returns false. Repeat
   for every bit position over a small payload to confirm 100% single-bit
   detection.
5. **Two-bit flip detection.** XOR the same bit position into two different
   bytes (this is exactly the corruption pattern the old XOR-sum CRC
   missed). Restore must now return false. Pre-fix this would have returned
   true with corrupt data.
6. **Version rejection.** Write a header with `CAL_FORMAT_VERSION` set to
   `0x01` (legacy) and valid v1 CRC; assert `restoreFromEEPROM()` returns
   false. This is the explicit backward-incompatibility guarantee.
7. **Header marker rejection.** Marker = 0xFF, 0x00, 0xCB — all must return
   false.
8. **Oversize/zero length rejection.** length = 0, length = CAL_DATA_MAX_SIZE+1.
9. **BNO085 word→byte overflow guard.** Mock `sh2_getFrs()` to return
   `num_words = 16384` or higher; `readCalibrationProfile()` must return
   false rather than overflow the stack buffer. (Currently caught by the
   `num_words > WORDS_BUFFER_CAPACITY` guard before the multiplication
   matters, but the multiplication guard is the defense-in-depth layer.)
10. **`hasCalibrationInEEPROM()` unchanged contract.** Marker-only check
    still returns true on valid marker with otherwise-empty header, as
    documented — make sure the v2 bump didn't accidentally tighten this.
