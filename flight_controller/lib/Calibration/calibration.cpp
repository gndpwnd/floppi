/*
 * Calibration Module - Core (helpers, shared state, dump command)
 * Auto-calibration for Radio and IMU with step-by-step guidance
 */

#include "calibration.h"
#include "config.h"
#include "radioComm.h"
#include "pin_definitions.h"

// External variables (from main.cpp / globals.h)
extern unsigned long channel_1_pwm, channel_2_pwm, channel_3_pwm;
extern unsigned long channel_4_pwm, channel_5_pwm, channel_6_pwm;

// External functions (from main.cpp / motors.cpp)
extern void getIMUdata();
extern void getCommands();

//========================================================================================================================//
//                                       CALIBRATION RESULTS ACCUMULATOR                                                   //
//========================================================================================================================//

CalibrationResults calResults = {};

//========================================================================================================================//
//                                              HELPER FUNCTIONS                                                          //
//========================================================================================================================//

bool waitForConfirmation(int /* unused */) {
    // Block until user types 'y' (continue) or 'n' (cancel).
    // No timeout — waits indefinitely. LED blinks to show firmware is alive.

    while (Serial.available()) Serial.read();  // Clear serial buffer

    Serial.println(F("\n>> Type 'y' to continue, 'n' to cancel: "));

    unsigned long lastBlink = millis();
    bool ledState = false;

    while (true) {
        if (Serial.available() > 0) {
            char response = Serial.read();
            while (Serial.available()) Serial.read();  // Clear rest of buffer

            if (response == 'y' || response == 'Y') {
                Serial.println(F("✓ Confirmed!"));
                digitalWrite(LED_PIN, LOW);
                return true;
            }
            else if (response == 'n' || response == 'N') {
                Serial.println(F("✗ Cancelled."));
                digitalWrite(LED_PIN, LOW);
                return false;
            }
        }

        // Blink LED every 500ms to show we're waiting (not frozen)
        if (millis() - lastBlink > 500) {
            lastBlink = millis();
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        }

        delay(10);
    }
}

int detectMovedChannel(uint16_t neutral1, uint16_t current1,
                      uint16_t neutral2, uint16_t current2,
                      uint16_t neutral3, uint16_t current3,
                      uint16_t neutral4, uint16_t current4,
                      uint16_t neutral5, uint16_t current5,
                      uint16_t neutral6, uint16_t current6,
                      int exclude1, int exclude2, int exclude3,
                      int exclude4, int exclude5) {
    // Find channel with largest change from neutral
    int maxDelta = 0;
    int maxChannel = -1;

    int deltas[6];
    deltas[0] = abs((int)current1 - (int)neutral1);
    deltas[1] = abs((int)current2 - (int)neutral2);
    deltas[2] = abs((int)current3 - (int)neutral3);
    deltas[3] = abs((int)current4 - (int)neutral4);
    deltas[4] = abs((int)current5 - (int)neutral5);
    deltas[5] = abs((int)current6 - (int)neutral6);

    for (int i = 0; i < 6; i++) {
        if (i == exclude1 || i == exclude2 || i == exclude3 ||
            i == exclude4 || i == exclude5) continue;

        if (deltas[i] > maxDelta && deltas[i] > 100) {  // Threshold: 100us change
            maxDelta = deltas[i];
            maxChannel = i;
        }
    }

    return maxChannel;
}

uint16_t getChannelValue(int channel) {
    switch(channel) {
        case 0: return channel_1_pwm;
        case 1: return channel_2_pwm;
        case 2: return channel_3_pwm;
        case 3: return channel_4_pwm;
        case 4: return channel_5_pwm;
        case 5: return channel_6_pwm;
        default: return 1500;
    }
}

//========================================================================================================================//
//                                    DUMP ALL CALIBRATION VALUES ('d' command)                                             //
//========================================================================================================================//

#ifdef CALIBRATION_MODE

// External runtime-tunable variables (defined in main.cpp, declared in globals.h)
extern float tune_kp_roll, tune_ki_roll, tune_kd_roll;
extern float tune_kp_pitch, tune_ki_pitch, tune_kd_pitch;
extern float tune_kp_yaw, tune_ki_yaw, tune_kd_yaw;
extern float tune_b_accel, tune_b_gyro, tune_b_dterm, tune_madgwick_beta;
extern float tune_max_roll, tune_max_pitch, tune_max_yaw;

void printAllCalibrationValues() {
    // Snapshot PID and filter values from runtime tuning variables
    calResults.hasPID = true;
    calResults.hasFilters = true;

    bool anySections = calResults.hasIMU || calResults.hasIMUScale || calResults.hasOrientation ||
                       calResults.hasRadio || calResults.hasFailsafe || calResults.hasPID ||
                       calResults.hasFilters;
    #ifdef USE_MPU9250
    anySections = anySections || calResults.hasMag;
    #endif

    if (!anySections) {
        Serial.println(F("\nNo calibration data collected this session."));
        Serial.println(F("Run calibrations first (try 'a' for guided workflow).\n"));
        return;
    }

    Serial.println(F("\n// ===== CALIBRATION VALUES -- copy into config.h ====="));
    Serial.println();

    // --- IMU Section ---
    if (calResults.hasIMU) {
        Serial.println(F("//============================================================================="));
        if (calResults.hasIMUScale) {
            Serial.println(F("// IMU Calibration Values (Generated by 6-position calibration)"));
        } else {
            Serial.println(F("// IMU Calibration Values (Generated by calibration program)"));
        }
        Serial.println(F("//============================================================================="));
        Serial.println(F("// Accelerometer offsets"));
        Serial.print(F("#define IMU_ACC_ERROR_X ")); Serial.print(calResults.accErrorX, 6); Serial.println(F("f"));
        Serial.print(F("#define IMU_ACC_ERROR_Y ")); Serial.print(calResults.accErrorY, 6); Serial.println(F("f"));
        Serial.print(F("#define IMU_ACC_ERROR_Z ")); Serial.print(calResults.accErrorZ, 6); Serial.println(F("f"));
        if (calResults.hasIMUScale) {
            Serial.println(F("// Accelerometer scale factors"));
            Serial.print(F("#define IMU_ACC_SCALE_X ")); Serial.print(calResults.accScaleX, 6); Serial.println(F("f"));
            Serial.print(F("#define IMU_ACC_SCALE_Y ")); Serial.print(calResults.accScaleY, 6); Serial.println(F("f"));
            Serial.print(F("#define IMU_ACC_SCALE_Z ")); Serial.print(calResults.accScaleZ, 6); Serial.println(F("f"));
        }
        Serial.println(F("// Gyroscope offsets"));
        Serial.print(F("#define IMU_GYRO_ERROR_X ")); Serial.print(calResults.gyroErrorX, 6); Serial.println(F("f"));
        Serial.print(F("#define IMU_GYRO_ERROR_Y ")); Serial.print(calResults.gyroErrorY, 6); Serial.println(F("f"));
        Serial.print(F("#define IMU_GYRO_ERROR_Z ")); Serial.print(calResults.gyroErrorZ, 6); Serial.println(F("f"));
        Serial.println();
    }

    // --- Orientation Section ---
    if (calResults.hasOrientation) {
        Serial.println(F("//============================================================================="));
        Serial.println(F("// IMU Orientation (Auto-detected by calibration)"));
        Serial.println(F("//============================================================================="));
        Serial.print(F("// Aircraft FORWARD -> IMU "));
        Serial.print(calResults.forwardInverted ? "-" : "+");
        Serial.println(calResults.forwardAxis);
        Serial.print(F("// Aircraft RIGHT   -> IMU "));
        Serial.print(calResults.rightInverted ? "-" : "+");
        Serial.println(calResults.rightAxis);
        Serial.print(F("// Aircraft UP      -> IMU "));
        Serial.print(calResults.upInverted ? "-" : "+");
        Serial.println(calResults.upAxis);
        Serial.println(F("// (See orientation calibration output for axis transformation code)"));
        Serial.println();
    }

    // --- Radio Section ---
    if (calResults.hasRadio) {
        Serial.println(F("//============================================================================="));
        Serial.println(F("// Radio Channel Mapping (Auto-detected by calibration)"));
        Serial.println(F("//============================================================================="));
        Serial.print(F("#define THROTTLE_CHANNEL ")); Serial.println(calResults.throttleChannel + 1);
        Serial.print(F("#define ROLL_CHANNEL ")); Serial.println(calResults.rollChannel + 1);
        Serial.print(F("#define PITCH_CHANNEL ")); Serial.println(calResults.pitchChannel + 1);
        Serial.print(F("#define YAW_CHANNEL ")); Serial.println(calResults.yawChannel + 1);
        Serial.print(F("#define AUX1_CHANNEL ")); Serial.println(calResults.aux1Channel + 1);
        Serial.print(F("#define AUX2_CHANNEL ")); Serial.println(calResults.aux2Channel + 1);
        Serial.println();
    }

    // --- Failsafe Section ---
    if (calResults.hasFailsafe) {
        Serial.println(F("//============================================================================="));
        Serial.println(F("// Failsafe Values (Auto-detected by calibration)"));
        Serial.println(F("//============================================================================="));
        Serial.print(F("#define FAILSAFE_THROTTLE ")); Serial.println(calResults.failsafe[2]);
        Serial.print(F("#define FAILSAFE_ROLL ")); Serial.println(calResults.failsafe[0]);
        Serial.print(F("#define FAILSAFE_PITCH ")); Serial.println(calResults.failsafe[1]);
        Serial.print(F("#define FAILSAFE_YAW ")); Serial.println(calResults.failsafe[3]);
        Serial.print(F("#define FAILSAFE_AUX1 ")); Serial.println(calResults.failsafe[4]);
        Serial.print(F("#define FAILSAFE_AUX2 ")); Serial.println(calResults.failsafe[5]);
        Serial.println();
    }

    #ifdef USE_MPU9250
    // --- Magnetometer Section ---
    if (calResults.hasMag) {
        Serial.println(F("//============================================================================="));
        Serial.println(F("// Magnetometer Calibration (Auto-detected by sphere calibration)"));
        Serial.println(F("//============================================================================="));
        Serial.println(F("#ifdef USE_MPU9250"));
        Serial.print(F("    #define MAG_ERROR_X ")); Serial.print(calResults.magOffsetX, 6); Serial.println(F("f"));
        Serial.print(F("    #define MAG_ERROR_Y ")); Serial.print(calResults.magOffsetY, 6); Serial.println(F("f"));
        Serial.print(F("    #define MAG_ERROR_Z ")); Serial.print(calResults.magOffsetZ, 6); Serial.println(F("f"));
        Serial.print(F("    #define MAG_SCALE_X ")); Serial.print(calResults.magScaleX, 6); Serial.println(F("f"));
        Serial.print(F("    #define MAG_SCALE_Y ")); Serial.print(calResults.magScaleY, 6); Serial.println(F("f"));
        Serial.print(F("    #define MAG_SCALE_Z ")); Serial.print(calResults.magScaleZ, 6); Serial.println(F("f"));
        Serial.println(F("#endif"));
        Serial.println();
    }
    #endif

    // --- PID Gains Section ---
    if (calResults.hasPID) {
        Serial.println(F("//============================================================================="));
        Serial.println(F("// PID Gains (current runtime values)"));
        Serial.println(F("//============================================================================="));
        #ifdef USE_RATE_CONTROLLER
        Serial.println(F("// Rate mode gains"));
        Serial.print(F("#define KP_ROLL_RATE ")); Serial.println(tune_kp_roll, 6);
        Serial.print(F("#define KI_ROLL_RATE ")); Serial.println(tune_ki_roll, 6);
        Serial.print(F("#define KD_ROLL_RATE ")); Serial.println(tune_kd_roll, 6);
        Serial.print(F("#define KP_PITCH_RATE ")); Serial.println(tune_kp_pitch, 6);
        Serial.print(F("#define KI_PITCH_RATE ")); Serial.println(tune_ki_pitch, 6);
        Serial.print(F("#define KD_PITCH_RATE ")); Serial.println(tune_kd_pitch, 6);
        #else
        Serial.println(F("// Angle mode gains"));
        Serial.print(F("#define KP_ROLL_ANGLE ")); Serial.println(tune_kp_roll, 6);
        Serial.print(F("#define KI_ROLL_ANGLE ")); Serial.println(tune_ki_roll, 6);
        Serial.print(F("#define KD_ROLL_ANGLE ")); Serial.println(tune_kd_roll, 6);
        Serial.print(F("#define KP_PITCH_ANGLE ")); Serial.println(tune_kp_pitch, 6);
        Serial.print(F("#define KI_PITCH_ANGLE ")); Serial.println(tune_ki_pitch, 6);
        Serial.print(F("#define KD_PITCH_ANGLE ")); Serial.println(tune_kd_pitch, 6);
        #endif
        Serial.print(F("#define KP_YAW_RATE ")); Serial.println(tune_kp_yaw, 6);
        Serial.print(F("#define KI_YAW_RATE ")); Serial.println(tune_ki_yaw, 6);
        Serial.print(F("#define KD_YAW_RATE ")); Serial.println(tune_kd_yaw, 6);
        Serial.println();
    }

    // --- Filter & Limits Section ---
    if (calResults.hasFilters) {
        Serial.println(F("//============================================================================="));
        Serial.println(F("// Filters & Limits (current runtime values)"));
        Serial.println(F("//============================================================================="));
        Serial.print(F("#define B_ACCEL ")); Serial.println(tune_b_accel, 4);
        Serial.print(F("#define B_GYRO ")); Serial.println(tune_b_gyro, 4);
        Serial.print(F("#define B_DTERM ")); Serial.println(tune_b_dterm, 4);
        Serial.print(F("#define MADGWICK_BETA ")); Serial.println(tune_madgwick_beta, 4);
        #ifdef USE_RATE_CONTROLLER
        Serial.print(F("#define MAX_ROLL_RATE ")); Serial.println(tune_max_roll, 1);
        Serial.print(F("#define MAX_PITCH_RATE ")); Serial.println(tune_max_pitch, 1);
        #else
        Serial.print(F("#define MAX_ROLL_ANGLE ")); Serial.println(tune_max_roll, 1);
        Serial.print(F("#define MAX_PITCH_ANGLE ")); Serial.println(tune_max_pitch, 1);
        #endif
        Serial.print(F("#define MAX_YAW_RATE ")); Serial.println(tune_max_yaw, 1);
        Serial.println();
    }

    Serial.println(F("// ===== END CALIBRATION VALUES =====\n"));
}

#endif // CALIBRATION_MODE
