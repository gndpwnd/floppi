/**
 * Guided BNO055 calibration session — implementation.
 *
 * See calibration_session.h for the operator-facing workflow. This file is
 * gated by UNO_GUIDED_TUNING; the flight build (`arduino_uno_minimal`) ships
 * without the wizard to save flash, and instead relies on the pre-saved blob
 * loaded from EEPROM at boot (see uno_balance_app.cpp begin()).
 *
 * Why no state machine: the BNO055 already tracks per-axis cal accuracy
 * (sys/accel/gyro/mag, each 0..3) in hardware. Adafruit_BNO055::isFullyCalibrated
 * returns true iff all four hit 3. The wizard's job is therefore just to (a)
 * keep fusion running by polling read() so the chip accumulates samples, and
 * (b) surface the live digits so the operator knows what motion is needed
 * next. No firmware-side stage tracking is necessary.
 */

// Double-gate matches calibration_session.h — BNO055-only wizard inside the
// guided-tuning build. USE_BNO085 elides this TU entirely (the BNO085 needs
// its own SH-2 FRS-based calibration session, out of scope for this turn).
#if defined(UNO_GUIDED_TUNING) && defined(USE_BNO055)

#include "calibration_session.h"

#include <Arduino.h>
#include <stdint.h>

#include "uno_balance_app.h"
#include "tune_storage.h"
#include "../../sensors/bno055.h"

namespace {

// Status-line cadence — fast enough that the operator sees the digits ticking
// upward as they rotate through poses, slow enough that the serial output is
// readable. ~500 ms ≈ 2 Hz, comfortably under any human reaction time.
const uint32_t STATUS_PRINT_INTERVAL_MS = 500;

// IMU polling cadence — tight loop is fine, the BNO055 internal fusion runs
// at ~100 Hz and read() is just an I²C burst. delay() between reads avoids
// hammering the I²C bus and gives Serial.available() / .read() a chance.
const uint32_t IMU_POLL_INTERVAL_MS = 20;

// Drain any pending Serial bytes; return true if any was 'q' or 'a' (case-
// insensitive abort). Other bytes are silently consumed so they don't leak
// into the next interactive command after we exit.
bool drain_serial_for_abort_() {
  bool aborted = false;
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c == 'q' || c == 'Q' || c == 'a' || c == 'A') {
      aborted = true;
    }
  }
  return aborted;
}

// Print a one-line live status: "cal=S3A2G3M1". Pulled from the chip via the
// BNO055 base getStatusString() — which returns a fixed "Sssagm" string the
// driver formats without snprintf (flash budget).
void print_status_line_(BNO055& imu) {
  Serial.print(F("cal="));
  Serial.println(imu.getStatusString());
}

// Print the pose script up-front so the operator knows what motion to perform.
// Calibration order on the BNO055:
//   - Gyro:  rest the bot flat & still for ~3 s (auto-cal at rest).
//   - Accel: rotate to each of six static orientations (±X, ±Y, ±Z faces up),
//            holding each for a few seconds.
//   - Mag:   draw a figure-8 in the air a few times.
//   - Sys:   becomes 3 once the above are all 3 and fusion is converged.
void print_pose_script_() {
  Serial.println(F("==== BNO055 GUIDED CAL — disarmed; rotate by hand ===="));
  Serial.println(F("1. GYRO: hold still flat on a table ~3 s"));
  Serial.println(F("2. ACCEL: rotate to each face (top/bot/left/right/fwd/back), hold ~3 s each"));
  Serial.println(F("3. MAG: draw a figure-8 in the air several times"));
  Serial.println(F("Watch cal=SaGM — each digit 0..3, all four = 3 = done."));
  Serial.println(F("Press 'q' or 'a' to abort without saving."));
  Serial.println(F("------------------------------------------------------"));
}

}  // namespace

namespace calibration_session {

void run_guided_cal(UnoBalanceApp& app, BNO055& imu) {
  // Safety first — operator is about to pick the bot up and rotate it through
  // six orientations and a figure-8. Motors driving during that would be both
  // useless (chassis is no longer on the ground) and dangerous to fingers.
  app.abort();

  print_pose_script_();

  uint32_t last_status_ms = 0;
  uint32_t last_poll_ms   = 0;

  bool finished = false;   // hit full-cal
  bool aborted  = false;   // operator hit 'q' / 'a'

  while (!finished && !aborted) {
    const uint32_t now_ms = millis();

    // Poll the IMU on a fixed cadence to keep NDOF fusion fed. We don't use
    // imu.read()'s return value for control — failures here are transient
    // (I²C glitch) and self-clear on the next tick.
    if ((uint32_t)(now_ms - last_poll_ms) >= IMU_POLL_INTERVAL_MS) {
      last_poll_ms = now_ms;
      imu.read();
      // Check completion BEFORE the next sleep so we exit promptly on the
      // tick that hits 3/3/3/3 rather than waiting for the status print.
      if (imu.isFullyCalibrated()) {
        finished = true;
        break;
      }
    }

    // Periodic live-status print so the operator sees progress.
    if ((uint32_t)(now_ms - last_status_ms) >= STATUS_PRINT_INTERVAL_MS) {
      last_status_ms = now_ms;
      print_status_line_(imu);
    }

    // Operator-abort check every iteration (cheap, no blocking).
    if (drain_serial_for_abort_()) {
      aborted = true;
      break;
    }

    // Small delay so we're not spinning the CPU at 16 MHz. The actual cadence
    // gating is done by the millis() checks above — this just yields a few ms.
    delay(5);
  }

  if (aborted) {
    Serial.println(F("CAL ABORTED"));
    return;
  }

  // Pull the 22-byte blob from the chip. BNO055::getCalibrationProfile takes
  // (uint8_t*, uint16_t*) — we pass a stack buffer and an in/out length.
  uint8_t blob[22];
  uint16_t len = sizeof(blob);
  if (!imu.getCalibrationProfile(blob, &len) || len != 22) {
    // Belt-and-suspenders: isFullyCalibrated() said true but the chip refused
    // to hand over the blob. Don't write garbage to EEPROM.
    Serial.println(F("CAL FAIL: could not read 22-byte blob"));
    return;
  }

  if (!tune_storage::save_cal_blob(blob, 22)) {
    Serial.println(F("CAL FAIL: EEPROM save returned false"));
    return;
  }

  Serial.println(F("CAL OK: 22-byte blob saved to EEPROM"));

  // Print the photo-backup block (cal-only — PID is owned by the tuning
  // session and will be printed there on its own commit). Pass a dummy
  // TuneBlock since pid_valid=false ignores it. IMU tag is "BNO055" — the
  // operator will paste this into balance_constants.h as BNO055_CAL_BLOB[22].
  TuneBlock dummy_pid = {0.0f, 0.0f, 0.0f, 0.0f};
  tune_storage::print_photo_backup(/*pid_valid=*/false, dummy_pid,
                                   /*cal_valid=*/true,  blob, /*cal_len=*/22,
                                   /*imu_tag=*/"BNO055");
}

}  // namespace calibration_session

#endif  // UNO_GUIDED_TUNING && USE_BNO055
