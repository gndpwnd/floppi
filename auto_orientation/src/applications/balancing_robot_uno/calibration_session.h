/**
 * Guided BNO055 calibration session — Uno tuning build.
 *
 * Tiny by design: a single blocking function, no state machine. The BNO055
 * already exposes per-axis cal accuracies (sys/accel/gyro/mag, each 0..3) on
 * every read(), so the calibration "state" lives inside the chip — firmware
 * just has to keep fusion running, surface the live S*A*G*M* digits to the
 * operator, and watch for the four-3s completion signal.
 *
 * Workflow (operator-facing):
 *   1. Operator types 'c' on the serial console.
 *   2. main.cpp calls calibration_session::run_guided_cal(app, imu).
 *   3. This function disarms the app (safety — no PID driving motors while
 *      the bot is being rotated by hand), prints the pose script, then loops:
 *        - imu.read() every tick to keep NDOF fusion running
 *        - every ~500 ms, print "S*A*G*M*" so operator sees progress
 *        - drain Serial for 'q' / 'a' abort each tick
 *      Exits when isFullyCalibrated() returns true OR the operator aborts.
 *   4. On success: read the 22-byte blob, persist via tune_storage, and print
 *      the photo-backup block (cal-only — no PID, the tuning session owns that).
 *   5. On abort: print "CAL ABORTED" and return without touching EEPROM.
 *
 * Returns disarmed in either case. The operator re-arms with 'g' when the
 * bot is back on the ground ready to balance.
 *
 * Entire TU is gated by UNO_GUIDED_TUNING — the flight build never includes
 * the calibration wizard (flash budget; flight uses the pre-saved cal blob).
 */

#ifndef CALIBRATION_SESSION_H
#define CALIBRATION_SESSION_H

// The guided-cal wizard is BNO055-specific: it polls Adafruit_BNO055's
// per-axis cal accuracies and isFullyCalibrated() exit condition. The BNO085
// uses an entirely different calibration flow (SH-2 dynamic-cal NVM via FRS
// records) and warrants its own session — so this TU is doubly-gated on
// UNO_GUIDED_TUNING (build flavor) AND USE_BNO055 (IMU selection). Picking
// USE_BNO085 elides the wizard cleanly without forcing a god-class virtual
// onto the OrientationSensor base.
#if defined(UNO_GUIDED_TUNING) && defined(USE_BNO055)

// Forward-declare so callers don't need to drag in the full UnoBalanceApp and
// BNO055 headers just to know the entry-point signature.
class UnoBalanceApp;
class BNO055;

namespace calibration_session {

/**
 * @brief Block (with periodic Serial polling) until the BNO055 reports fully
 *        calibrated OR the operator aborts via 'q' / 'a'. On success, the
 *        22-byte blob is persisted to EEPROM via tune_storage::save_cal_blob
 *        and the photo-backup block is printed to Serial.
 *
 * @param[in,out] app  UnoBalanceApp to disarm at entry (motors off while the
 *                     operator hand-rotates the bot through the pose script).
 *                     Left DISARMED on return — operator re-arms with 'g'.
 * @param[in,out] imu  BNO055 driver. Must have already been begin()'d (the
 *                     caller — main.cpp 'c' handler — is responsible for that).
 */
void run_guided_cal(UnoBalanceApp& app, BNO055& imu);

}  // namespace calibration_session

#endif  // UNO_GUIDED_TUNING && USE_BNO055

#endif  // CALIBRATION_SESSION_H
