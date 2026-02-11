/* Calibration Module - Failsafe, ESC, and Magnetometer Calibration */

#include "calibration.h"
#include "config.h"
#include "radioComm.h"

// External motor command variables (from main.cpp / globals.h)
extern int m1_command_PWM, m2_command_PWM, m3_command_PWM;
extern int m4_command_PWM, m5_command_PWM, m6_command_PWM;

#ifdef USE_MPU9250
extern float MagX, MagY, MagZ;
#endif

//========================================================================================================================//
//                                         FAILSAFE AUTO-DETECTION                                                      //
//========================================================================================================================//

void calibrateFailsafe() {
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     FAILSAFE AUTO-DETECTION                               |"));
    Serial.println(F("+-----------------------------------------------------------+\n"));

    Serial.println(F("This program will detect your receiver's failsafe output."));
    Serial.println(F("When signal is lost, receivers output specific PWM values."));
    Serial.println(F("These values should be set as your failsafe config.\n"));

    Serial.println(F("STEP 1: First, let's read NORMAL values with transmitter ON."));
    Serial.println(F("  - Make sure your transmitter is ON and connected"));
    Serial.println(F("  - Center all sticks, switches in default position"));

    if (!waitForConfirmation(5)) return;

    // Read normal values (average over 50 samples)
    uint32_t normal[6] = {0};
    for (int i = 0; i < 50; i++) {
        getCommands();
        normal[0] += channel_1_pwm;
        normal[1] += channel_2_pwm;
        normal[2] += channel_3_pwm;
        normal[3] += channel_4_pwm;
        normal[4] += channel_5_pwm;
        normal[5] += channel_6_pwm;
        delay(20);
    }
    for (int i = 0; i < 6; i++) normal[i] /= 50;

    Serial.println(F("\nNormal values (TX on):"));
    for (int i = 0; i < 6; i++) {
        Serial.print(F("  CH")); Serial.print(i + 1);
        Serial.print(F(": ")); Serial.println(normal[i]);
    }

    // Check if receiver is actually connected
    bool receiverOk = true;
    for (int i = 0; i < 6; i++) {
        if (normal[i] < 800 || normal[i] > 2200) {
            receiverOk = false;
            break;
        }
    }
    if (!receiverOk) {
        Serial.println(F("\nWARNING: Some channels are out of normal range (800-2200us)."));
        Serial.println(F("Is your receiver connected and transmitter bound?"));
        Serial.println(F("Continue anyway?"));
        if (!waitForConfirmation(3)) return;
    }

    Serial.println(F("\nSTEP 2: Now TURN OFF your transmitter."));
    Serial.println(F("  - Power off the transmitter completely"));
    Serial.println(F("  - Wait for receiver to enter failsafe mode"));
    Serial.println(F("  - This usually takes 1-3 seconds"));
    Serial.println(F("\nAfter turning off transmitter, confirm below."));

    if (!waitForConfirmation(30)) return;

    Serial.println(F("\nWaiting 3 seconds for failsafe to activate..."));
    delay(3000);

    // Read failsafe values (average over 50 samples)
    uint32_t failsafe[6] = {0};
    for (int i = 0; i < 50; i++) {
        getCommands();
        failsafe[0] += channel_1_pwm;
        failsafe[1] += channel_2_pwm;
        failsafe[2] += channel_3_pwm;
        failsafe[3] += channel_4_pwm;
        failsafe[4] += channel_5_pwm;
        failsafe[5] += channel_6_pwm;
        delay(20);
    }
    for (int i = 0; i < 6; i++) failsafe[i] /= 50;

    Serial.println(F("\nFailsafe values (TX off):"));
    for (int i = 0; i < 6; i++) {
        Serial.print(F("  CH")); Serial.print(i + 1);
        Serial.print(F(": ")); Serial.print(failsafe[i]);
        int delta = (int)failsafe[i] - (int)normal[i];
        if (abs(delta) > 20) {
            Serial.print(F("  (changed by ")); Serial.print(delta); Serial.print(F("us)"));
        } else {
            Serial.print(F("  (unchanged)"));
        }
        Serial.println();
    }

    // Output copy-paste defines
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     COPY-PASTE READY CODE                                 |"));
    Serial.println(F("+-----------------------------------------------------------+\n"));

    Serial.println(F("Copy to config.h, replace the existing Failsafe Values section:\n"));

    Serial.println(F("//============================================================================="));
    Serial.println(F("// Failsafe Values (Auto-detected by calibration)"));
    Serial.println(F("//============================================================================="));
    Serial.print(F("#define FAILSAFE_THROTTLE ")); Serial.println(failsafe[2]);
    Serial.print(F("#define FAILSAFE_ROLL ")); Serial.println(failsafe[0]);
    Serial.print(F("#define FAILSAFE_PITCH ")); Serial.println(failsafe[1]);
    Serial.print(F("#define FAILSAFE_YAW ")); Serial.println(failsafe[3]);
    Serial.print(F("#define FAILSAFE_AUX1 ")); Serial.println(failsafe[4]);
    Serial.print(F("#define FAILSAFE_AUX2 ")); Serial.println(failsafe[5]);

    // Store results for dump command
    calResults.hasFailsafe = true;
    for (int i = 0; i < 6; i++) calResults.failsafe[i] = failsafe[i];

    Serial.println(F("\nNote: Channel-to-function mapping uses your THROTTLE_CHANNEL etc."));
    Serial.println(F("The values above use default channel order (1=roll, 2=pitch, 3=throttle, 4=yaw)."));
    Serial.println(F("If your channel mapping differs, adjust accordingly.\n"));

    Serial.println(F("IMPORTANT: Turn your transmitter back ON now!"));

    Serial.println(F("\n-----------------------------------------------------------\n"));
}

//========================================================================================================================//
//                                         ESC ENDPOINT CALIBRATION                                                       //
//========================================================================================================================//

void calibrateESC() {
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     ESC ENDPOINT CALIBRATION                               |"));
    Serial.println(F("+-----------------------------------------------------------+\n"));

    Serial.println(F("WARNING: REMOVE ALL PROPELLERS BEFORE CONTINUING!"));
    Serial.println(F("Motors WILL spin during this procedure.\n"));

    Serial.println(F("This calibrates ESC min/max throttle endpoints."));
    Serial.println(F("Standard procedure:"));
    Serial.println(F("  1. FC sends MAX throttle to all motors"));
    Serial.println(F("  2. You connect ESC power (battery)"));
    Serial.println(F("  3. ESCs beep to confirm max"));
    Serial.println(F("  4. FC sends MIN throttle"));
    Serial.println(F("  5. ESCs beep to confirm range\n"));

    Serial.println(F("Are propellers removed? Type 'y' to confirm."));

    if (!waitForConfirmation(30)) return;

    // Step 1: Send max throttle
    Serial.println(F("\nSending MAX throttle (2000us) to all motors..."));

    m1_command_PWM = 2000;
    m2_command_PWM = 2000;
    m3_command_PWM = 2000;
    m4_command_PWM = 2000;
    m5_command_PWM = 2000;
    m6_command_PWM = 2000;
    commandMotors();

    Serial.println(F("\nNow connect battery power to ESCs."));
    Serial.println(F("You should hear ascending beeps from the ESCs."));
    Serial.println(F("Wait for the beeps, then confirm below."));

    if (!waitForConfirmation(60)) {
        // Safety: send min on timeout/cancel
        m1_command_PWM = 1000;
        m2_command_PWM = 1000;
        m3_command_PWM = 1000;
        m4_command_PWM = 1000;
        m5_command_PWM = 1000;
        m6_command_PWM = 1000;
        commandMotors();
        return;
    }

    // Step 2: Send min throttle
    Serial.println(F("\nSending MIN throttle (1000us) to all motors..."));

    m1_command_PWM = 1000;
    m2_command_PWM = 1000;
    m3_command_PWM = 1000;
    m4_command_PWM = 1000;
    m5_command_PWM = 1000;
    m6_command_PWM = 1000;
    commandMotors();

    Serial.println(F("You should hear descending beeps confirming the range."));
    delay(3000);

    Serial.println(F("\nESC calibration complete!"));
    Serial.println(F("ESCs now know your min (1000us) and max (2000us) range."));
    Serial.println(F("No config.h changes needed — ESC endpoints are stored in the ESCs.\n"));

    Serial.println(F("-----------------------------------------------------------\n"));
}

//========================================================================================================================//
//                                    MAGNETOMETER CALIBRATION (MPU9250 only)                                             //
//========================================================================================================================//

#ifdef USE_MPU9250

void calibrateMagnetometer() {
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     MAGNETOMETER CALIBRATION (Sphere)                     |"));
    Serial.println(F("+-----------------------------------------------------------+\n"));

    Serial.println(F("This calibrates the magnetometer for hard-iron and soft-iron"));
    Serial.println(F("distortion. You will slowly rotate the aircraft in ALL"));
    Serial.println(F("orientations while the firmware records min/max per axis.\n"));

    Serial.println(F("INSTRUCTIONS:"));
    Serial.println(F("  1. Hold aircraft away from metal objects and electronics"));
    Serial.println(F("  2. When prompted, slowly rotate in all directions:"));
    Serial.println(F("     - Roll left/right (full 360)"));
    Serial.println(F("     - Pitch forward/back (full 360)"));
    Serial.println(F("     - Yaw left/right (full 360)"));
    Serial.println(F("     - Combine motions — cover as many orientations as possible"));
    Serial.println(F("  3. Collection runs for 30 seconds\n"));

    if (!waitForConfirmation(5)) return;

    Serial.println(F("\nStart rotating NOW! 30 seconds..."));

    float minX = 99999.0f, maxX = -99999.0f;
    float minY = 99999.0f, maxY = -99999.0f;
    float minZ = 99999.0f, maxZ = -99999.0f;

    unsigned long startTime = millis();
    unsigned long duration = 30000; // 30 seconds
    int samples = 0;

    while (millis() - startTime < duration) {
        getIMUdata(); // This reads MagX, MagY, MagZ for MPU9250

        if (MagX < minX) minX = MagX;
        if (MagX > maxX) maxX = MagX;
        if (MagY < minY) minY = MagY;
        if (MagY > maxY) maxY = MagY;
        if (MagZ < minZ) minZ = MagZ;
        if (MagZ > maxZ) maxZ = MagZ;

        samples++;

        // Progress every 5 seconds
        unsigned long elapsed = (millis() - startTime) / 1000;
        if (samples % 500 == 0) {
            Serial.print(elapsed); Serial.print(F("s... "));
            Serial.print(F("X[")); Serial.print(minX, 1); Serial.print(F(",")); Serial.print(maxX, 1);
            Serial.print(F("] Y[")); Serial.print(minY, 1); Serial.print(F(",")); Serial.print(maxY, 1);
            Serial.print(F("] Z[")); Serial.print(minZ, 1); Serial.print(F(",")); Serial.print(maxZ, 1);
            Serial.println(F("]"));
        }

        delay(10);
    }

    Serial.print(F("\nCollected ")); Serial.print(samples); Serial.println(F(" samples."));

    // Calculate hard-iron offsets (center of sphere)
    float offsetX = (maxX + minX) / 2.0f;
    float offsetY = (maxY + minY) / 2.0f;
    float offsetZ = (maxZ + minZ) / 2.0f;

    // Calculate soft-iron scale factors (normalize to sphere)
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float rangeZ = maxZ - minZ;
    float avgRange = (rangeX + rangeY + rangeZ) / 3.0f;

    float scaleX = (avgRange > 0.001f) ? avgRange / rangeX : 1.0f;
    float scaleY = (avgRange > 0.001f) ? avgRange / rangeY : 1.0f;
    float scaleZ = (avgRange > 0.001f) ? avgRange / rangeZ : 1.0f;

    // Quality check
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     CALIBRATION QUALITY CHECK                             |"));
    Serial.println(F("+-----------------------------------------------------------+"));

    bool goodCal = true;

    // Check range (should be significant)
    if (rangeX < 10.0f || rangeY < 10.0f || rangeZ < 10.0f) {
        Serial.println(F("WARNING: Range too small — did you rotate fully?"));
        goodCal = false;
    }

    // Check scale factors (should be close to 1.0)
    if (abs(scaleX - 1.0f) > 0.2f || abs(scaleY - 1.0f) > 0.2f || abs(scaleZ - 1.0f) > 0.2f) {
        Serial.println(F("WARNING: Scale factors off — possible nearby magnetic interference"));
        goodCal = false;
    }

    if (goodCal) {
        Serial.println(F("Calibration quality looks GOOD!"));
    } else {
        Serial.println(F("\nRetry calibration? (y/n)"));
        if (waitForConfirmation(3)) {
            calibrateMagnetometer();
            return;
        }
    }

    // Output results
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     COPY-PASTE READY CODE                                 |"));
    Serial.println(F("+-----------------------------------------------------------+\n"));

    Serial.println(F("Copy to config.h, replace the Magnetometer Calibration section:\n"));

    Serial.println(F("//============================================================================="));
    Serial.println(F("// Magnetometer Calibration (Auto-detected by sphere calibration)"));
    Serial.println(F("//============================================================================="));
    Serial.println(F("#ifdef USE_MPU9250"));
    Serial.print(F("    #define MAG_ERROR_X ")); Serial.print(offsetX, 6); Serial.println(F("f"));
    Serial.print(F("    #define MAG_ERROR_Y ")); Serial.print(offsetY, 6); Serial.println(F("f"));
    Serial.print(F("    #define MAG_ERROR_Z ")); Serial.print(offsetZ, 6); Serial.println(F("f"));
    Serial.print(F("    #define MAG_SCALE_X ")); Serial.print(scaleX, 6); Serial.println(F("f"));
    Serial.print(F("    #define MAG_SCALE_Y ")); Serial.print(scaleY, 6); Serial.println(F("f"));
    Serial.print(F("    #define MAG_SCALE_Z ")); Serial.print(scaleZ, 6); Serial.println(F("f"));
    Serial.println(F("#endif"));

    // Store results for dump command
    calResults.hasMag = true;
    calResults.magOffsetX = offsetX;
    calResults.magOffsetY = offsetY;
    calResults.magOffsetZ = offsetZ;
    calResults.magScaleX = scaleX;
    calResults.magScaleY = scaleY;
    calResults.magScaleZ = scaleZ;

    Serial.println(F("\n-----------------------------------------------------------\n"));
}

#endif // USE_MPU9250
