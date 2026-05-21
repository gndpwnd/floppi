/* ============================================================================
 * COLD-START SEED — hand-editable. NOT the live tuned gains.
 * ============================================================================
 *
 * As of 2026-05-20 (guided-tuning pivot) this file is a *cold-start seed*, not
 * an auto-generated artifact. It is safe to hand-edit.
 *
 * The LIVE tuned gains come from EEPROM, set via the on-device guided P->I->D
 * tuning session (the `arduino_uno_tuning` build env). The flight build
 * (`arduino_uno_minimal`) reads the EEPROM tune block at boot and only falls
 * back to the constants below when EEPROM holds no valid tune (fresh chip, or
 * a never-tuned chassis). See:
 *   docs/findings/uno_guided_tuning_design_2026-05-20.md
 *   docs/applications/balancing_robot_uno/README.md  (guided-tuning workflow)
 *
 * How to set / change these values:
 *   - Recommended: just run a guided tuning session — it writes EEPROM and
 *     leaves this seed untouched.
 *   - Hand-edit: edit the constants below directly to nudge the cold-start
 *     point (e.g. conservative defaults, or last-known-good gains).
 *   - Optional regenerate: tools/sim/brute_tune.py can produce a fresh seed
 *     for a brand-new chassis whose gains are too far off to balance well
 *     enough to start guided Stage P. brute_tune.py is now an OPTIONAL seed
 *     generator, not the operational tuning loop:
 *       python3 tools/sim/brute_tune.py --mode evolutionary --budget 5000 \
 *                                       --output <this-file>
 *
 * Reference for context (SelfBallancingRobot3.ino, manually-tuned working bot):
 *   Kp=65  Ki=12  Kd=38  PITCH_OFFSET=-8.6  PWM_MAX=255
 *
 * The values currently in this file were produced 2026-05-20 by an
 * evolutionary brute_tune.py run (budget=5000, seed=42, plant=uno_small,
 * fitness=3.272 / 8.000s balanced, tip=no) and are retained as the seed.
 *
 * Hardware assumptions baked into these values:
 *   - IMU: BNO055 in NDOF fusion mode at I2C 0x28, ~100 Hz internal rate
 *   - Motors: small DC + L298N, ~6-9 V supply, hobby gearbox
 *   - PWM range: ±255 (8-bit analogWrite)
 *   - PID sample period: 5 ms (200 Hz, via MsTimer2)
 *
 * Re-tune (a guided on-device session) when ANY of these change:
 *   - Battery voltage shifts more than ~15%
 *   - Wheels, gearbox, or motors swapped
 *   - Chassis mass redistributed (mounting the IMU higher/lower changes Kd)
 *   - Tether attached/detached (the cable acts as a spring + damper)
 */

#ifndef BALANCE_CONSTANTS_H
#define BALANCE_CONSTANTS_H

#include <stdint.h>

// ----------------------------------------------------------------------------
// PID gains  (SEED — starting value; live gains come from the guided tuning session / EEPROM)
// ----------------------------------------------------------------------------
// Reference .ino values (hand-tuned, Mega + BNO055 + L298N):
//   Kp = 65, Ki = 12, Kd = 38
static const float BALANCE_KP = 94.4873f;
static const float BALANCE_KI = 33.6538f;
static const float BALANCE_KD = 90.2880f;

// ----------------------------------------------------------------------------
// Mounting offset  (SEED — starting value; live gains come from the guided tuning session / EEPROM)
// ----------------------------------------------------------------------------
// Operator-measured mechanical balance angle, in degrees of pitch. The PID
// drives (raw_pitch - PITCH_OFFSET) toward zero. Reference .ino used -8.6°.
// Re-measure when the IMU is remounted or the chassis is rebuilt.
static const float PITCH_OFFSET_DEG = -4.3327f;

// ----------------------------------------------------------------------------
// Loop timing  (FIXED — not a tunable gain; never overridden by EEPROM)
// ----------------------------------------------------------------------------
// 5 ms = 200 Hz, matches the reference .ino's PID.SetSampleTime(5).
static const uint16_t PID_SAMPLE_MS = 5;

// ----------------------------------------------------------------------------
// Output limits & stiction
// ----------------------------------------------------------------------------
// PID output is clamped to [PWM_MIN, PWM_MAX]. PWM_MAX is a SEED value
// (a guided tuning session may persist its own to EEPROM); PWM_MIN is set
// to -PWM_MAX to keep the range symmetric (motor driver interprets the
// sign as direction).
static const int16_t PWM_MIN = -255;
static const int16_t PWM_MAX = 255;

// Below this PWM magnitude the motors hum but do not turn. Reference .ino
// snapped sub-threshold outputs up to ±15. Set to 0 to disable.
// FIXED — not a tunable gain; never overridden by EEPROM (matches the
// simulation stiction floor so deployed gains see the same dead-band).
static const uint8_t STICTION_PWM = 15;

// ----------------------------------------------------------------------------
// Safety  (FIXED — not tunable gains; never overridden by EEPROM; SAFETY limits)
// ----------------------------------------------------------------------------
// If |corrected_pitch| exceeds this, the bot is considered tipped over; motors
// are stopped and the loop refuses to drive them until pitch returns within
// the threshold (no sticky FALLEN state — simpler is better here).
static const float TIP_CUTOFF_DEG = 25.0000f;

// Sanity-reject any raw pitch reading whose magnitude exceeds this. Matches
// the reference .ino's `abs(rawPitch) < 90` guard against gimbal-lock / NaN.
static const float PITCH_SANITY_DEG = 90.0000f;

#endif  // BALANCE_CONSTANTS_H
