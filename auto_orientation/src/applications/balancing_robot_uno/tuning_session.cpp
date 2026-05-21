/**
 * TuningSession implementation — see tuning_session.h.
 *
 * Entirely gated by UNO_GUIDED_TUNING: this translation unit emits nothing
 * for the flight build even though platformio compiles it.
 */

#ifdef UNO_GUIDED_TUNING

#include "tuning_session.h"

#include <Arduino.h>
#include <stdlib.h>  // dtostrf

#include "balance_constants.h"
#include "tune_storage.h"

const float TuningSession::KP_STEP = 2.0f;
const float TuningSession::KI_STEP = 1.0f;
const float TuningSession::KD_STEP = 2.0f;

// Format a float into a small stack buffer for terse serial output.
static const char* fmt_(float v, char* buf) {
  dtostrf(v, 0, 1, buf);
  return buf;
}

TuningSession::TuningSession()
    : app_(nullptr),
      stage_(TuneStage::IDLE),
      work_kp_(0.0f), work_ki_(0.0f), work_kd_(0.0f),
      entry_kp_(0.0f), entry_ki_(0.0f), entry_kd_(0.0f),
      coarse_(false) {}

void TuningSession::begin(UnoBalanceApp& app) {
  app_ = &app;
  // Seed the working gains from whatever the app booted with (EEPROM or seed).
  app_->get_work_gains(work_kp_, work_ki_, work_kd_);
  stage_ = TuneStage::IDLE;
  coarse_ = false;
  Serial.println(F("TUNE: 't' to start"));
}

void TuningSession::apply_masked_() {
  if (!app_) return;
  // Mask not-yet-tuned terms per stage.
  float kp = work_kp_;
  float ki = work_ki_;
  float kd = work_kd_;
  switch (stage_) {
    case TuneStage::STAGE_P: ki = 0.0f; kd = 0.0f; break;
    case TuneStage::STAGE_I: kd = 0.0f;            break;
    default:                                       break;  // D / REVIEW / IDLE: full
  }
  app_->apply_gains(kp, ki, kd);
}

void TuningSession::print_prompt_() {
  switch (stage_) {
    case TuneStage::STAGE_P:
      Serial.println(F("[P] Ki=Kd=0. +/- Kp. n=next"));
      break;
    case TuneStage::STAGE_I:
      Serial.println(F("[I] Kd=0. +/- Ki. n=next b=back"));
      break;
    case TuneStage::STAGE_D:
      Serial.println(F("[D] +/- Kd. n=review b=back"));
      break;
    case TuneStage::REVIEW:
      Serial.println(F("[REVIEW] w=save q=quit b=back"));
      break;
    default:
      Serial.println(F("[IDLE] t=start"));
      break;
  }
}

void TuningSession::enter_stage_(TuneStage s) {
  stage_ = s;
  coarse_ = false;
  // Snapshot entry values so 'r' can revert this stage's term.
  entry_kp_ = work_kp_;
  entry_ki_ = work_ki_;
  entry_kd_ = work_kd_;
  apply_masked_();
  print_prompt_();
  print_status();
}

void TuningSession::handle_command(char c) {
  switch (c) {
    case 't':  // start session
      if (stage_ == TuneStage::IDLE) {
        enter_stage_(TuneStage::STAGE_P);
      }
      break;

    case '+':
    case '-': {
      float sign = (c == '+') ? 1.0f : -1.0f;
      float mult = coarse_ ? (float)COARSE_MULT : 1.0f;
      switch (stage_) {
        case TuneStage::STAGE_P:
          work_kp_ += sign * KP_STEP * mult;
          if (work_kp_ < 0.0f) work_kp_ = 0.0f;
          break;
        case TuneStage::STAGE_I:
          work_ki_ += sign * KI_STEP * mult;
          if (work_ki_ < 0.0f) work_ki_ = 0.0f;
          break;
        case TuneStage::STAGE_D:
          work_kd_ += sign * KD_STEP * mult;
          if (work_kd_ < 0.0f) work_kd_ = 0.0f;
          break;
        default:
          return;  // not in an adjust stage
      }
      apply_masked_();
      print_status();
      break;
    }

    case '*':  // coarse toggle
      if (stage_ == TuneStage::STAGE_P || stage_ == TuneStage::STAGE_I ||
          stage_ == TuneStage::STAGE_D) {
        coarse_ = !coarse_;
        Serial.print(F("step="));
        Serial.println(coarse_ ? F("coarse") : F("fine"));
      }
      break;

    case 'n':  // lock current term, advance
      switch (stage_) {
        case TuneStage::STAGE_P: enter_stage_(TuneStage::STAGE_I); break;
        case TuneStage::STAGE_I: enter_stage_(TuneStage::STAGE_D); break;
        case TuneStage::STAGE_D: enter_stage_(TuneStage::REVIEW);  break;
        default: break;
      }
      break;

    case 'b':  // back one stage
      switch (stage_) {
        case TuneStage::STAGE_I: enter_stage_(TuneStage::STAGE_P); break;
        case TuneStage::STAGE_D: enter_stage_(TuneStage::STAGE_I); break;
        case TuneStage::REVIEW:  enter_stage_(TuneStage::STAGE_D); break;
        default: break;
      }
      break;

    case 'r':  // reset current term to stage-entry value
      switch (stage_) {
        case TuneStage::STAGE_P: work_kp_ = entry_kp_; break;
        case TuneStage::STAGE_I: work_ki_ = entry_ki_; break;
        case TuneStage::STAGE_D: work_kd_ = entry_kd_; break;
        default: return;
      }
      apply_masked_();
      print_status();
      break;

    case 'w':  // save (REVIEW only)
      if (stage_ == TuneStage::REVIEW) {
        TuneBlock blk;
        blk.kp = work_kp_;
        blk.ki = work_ki_;
        blk.kd = work_kd_;
        // pitch_off not guided in phase 1 — mirror the seed.
        blk.pitch_off = PITCH_OFFSET_DEG;
        if (tune_storage::save_tuning(blk)) {
          Serial.println(F("SAVED. Reboot to fly."));
        } else {
          Serial.println(F("SAVE FAILED"));
        }
      }
      break;

    case 'q':  // quit without saving
      if (stage_ != TuneStage::IDLE) {
        stage_ = TuneStage::IDLE;
        coarse_ = false;
        // Restore the gains the app booted with.
        app_->get_work_gains(work_kp_, work_ki_, work_kd_);
        Serial.println(F("QUIT (not saved)"));
        print_prompt_();
      }
      break;

    case 's':  // status
      print_status();
      break;

    default:
      break;  // not a tuning command — main.cpp handles the rest
  }
}

void TuningSession::print_status() {
  char b[12];
  Serial.print(F("stage="));
  switch (stage_) {
    case TuneStage::STAGE_P: Serial.print(F("P"));      break;
    case TuneStage::STAGE_I: Serial.print(F("I"));      break;
    case TuneStage::STAGE_D: Serial.print(F("D"));      break;
    case TuneStage::REVIEW:  Serial.print(F("REVIEW")); break;
    default:                 Serial.print(F("IDLE"));   break;
  }
  Serial.print(F(" Kp="));  Serial.print(fmt_(work_kp_, b));
  Serial.print(F(" Ki="));  Serial.print(fmt_(work_ki_, b));
  Serial.print(F(" Kd="));  Serial.println(fmt_(work_kd_, b));
}

#endif  // UNO_GUIDED_TUNING
