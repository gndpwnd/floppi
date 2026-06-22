/**
 * TuningSession — interactive guided PID tuning state machine (Uno).
 *
 * Built ONLY by the `arduino_uno_tuning` env: every declaration here is gated
 * by UNO_GUIDED_TUNING so the flight build (`arduino_uno_minimal`) compiles
 * none of it — no state machine, no prompt strings, no flash cost.
 *
 * The session walks the operator through classic manual PID tuning, one term
 * at a time, with the bot LIVE and balancing throughout:
 *
 *     IDLE -> STAGE_P -> STAGE_D -> STAGE_I -> REVIEW -> (save to EEPROM)
 *
 * Each stage masks the not-yet-tuned terms to zero (STAGE_P forces Ki=Kd=0,
 * STAGE_D forces Ki=0) and pushes the masked gains live via
 * UnoBalanceApp::apply_gains(). On REVIEW 'w' the working gains are persisted
 * through tune_storage::save_tuning().
 *
 * The session never blocks: handle_command() processes one serial char and
 * returns fast, so the 5 ms PID ISR keeps the bot balancing.
 */

#ifndef TUNING_SESSION_H
#define TUNING_SESSION_H

#ifdef UNO_GUIDED_TUNING

#include <stdint.h>

#include "uno_balance_app.h"

// Stage of the guided tuning walk-through.
enum class TuneStage : uint8_t {
  IDLE,      // bot balances on EEPROM/seed gains; no tuning active
  STAGE_P,   // Ki=Kd=0 forced; operator adjusts Kp
  STAGE_D,   // Kp locked; Ki=0 forced; operator adjusts Kd
  STAGE_I,   // Kp,Kd locked; operator adjusts Ki (full PID active)
  REVIEW     // all three locked; confirm + save
};

class TuningSession {
 public:
  // UI increment per '+'/'-' tap (fine mode). Coarse mode multiplies by 5.
  static const float    KP_STEP;
  static const float    KI_STEP;
  static const float    KD_STEP;
  static const uint8_t  COARSE_MULT = 5;

  TuningSession();

  /** Bind to the balance app. Call once from setup() after app.begin(). */
  void begin(UnoBalanceApp& app);

  /** Process one serial command char (see design §2.2). Returns fast. */
  void handle_command(char c);

  /** Print the one-line stage + working-gains status. */
  void print_status();

  /**
   * Transition IDLE -> STAGE_P without requiring the operator to type 't'.
   * Used by the '!' first-time-setup macro in main.cpp to chain BNO055
   * calibration directly into guided PID tuning entry. The macro removes
   * the keystroke friction only — the PID stages themselves still require
   * operator judgment via +/-/n.
   *
   * @return true on success (was IDLE, now in STAGE_P); false if the
   *         session was already in a non-IDLE stage (caller should not
   *         clobber an active tune).
   */
  bool auto_enter_after_cal();

 private:
  // Apply the current stage's gain mask and push live to the balance app.
  void apply_masked_();
  // Print the terse prompt for the current stage.
  void print_prompt_();
  // Enter a stage: snapshot the entry value for 'r', print the prompt.
  void enter_stage_(TuneStage s);

  UnoBalanceApp* app_;
  TuneStage      stage_;

  // Working gains (operator-adjusted, pre-mask).
  float work_kp_;
  float work_ki_;
  float work_kd_;

  // Stage-entry snapshots — 'r' resets the current term to these.
  float entry_kp_;
  float entry_ki_;
  float entry_kd_;

  // Coarse-mode flag (×5 step). Toggled by '*'.
  bool  coarse_;
};

#endif  // UNO_GUIDED_TUNING
#endif  // TUNING_SESSION_H
