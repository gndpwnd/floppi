/* Calibration Module - IMU Orientation Detection */

#include "calibration.h"
#include "config.h"

//========================================================================================================================//
//                                    IMU ORIENTATION DETECTION FUNCTION                                                  //
//========================================================================================================================//

void calibrateIMUWithOrientation() {
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     IMU CALIBRATION & ORIENTATION DETECTION               ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.println(F("This program will:"));
    Serial.println(F("  ✓ Calibrate IMU offsets (gyro and accelerometer)"));
    Serial.println(F("  ✓ Auto-detect how your IMU is mounted"));
    Serial.println(F("  ✓ Generate axis transformation code automatically\n"));

    if (!waitForConfirmation()) return;

    // First run basic calibration
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 1: LEVEL CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Place aircraft on FLAT, LEVEL surface"));
    Serial.println(F("▶ Nose pointing FORWARD"));
    Serial.println(F("▶ Wings level (no roll)"));
    Serial.println(F("▶ Do NOT move during calibration (5 seconds)"));

    if (!waitForConfirmation()) return;

    // Run calibration
    Serial.println(F("Calibrating..."));

    int numSamples = 2000;
    float AccX_sum = 0, AccY_sum = 0, AccZ_sum = 0;
    float GyroX_sum = 0, GyroY_sum = 0, GyroZ_sum = 0;

    for (int i = 0; i < numSamples; i++) {
        getIMUdata();
        AccX_sum += AccX;
        AccY_sum += AccY;
        AccZ_sum += AccZ;
        GyroX_sum += GyroX;
        GyroY_sum += GyroY;
        GyroZ_sum += GyroZ;
        if (i % 200 == 0) Serial.print(F("."));
        delay(2);
    }
    Serial.println();

    float AccErrorX = AccX_sum / numSamples;
    float AccErrorY = AccY_sum / numSamples;
    float AccErrorZ = (AccZ_sum / numSamples) - 1.0;
    float GyroErrorX = GyroX_sum / numSamples;
    float GyroErrorY = GyroY_sum / numSamples;
    float GyroErrorZ = GyroZ_sum / numSamples;

    Serial.println(F("✅ Level calibration complete!\n"));

    // STEP 2: NOSE UP ORIENTATION TEST
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 2: NOSE UP ORIENTATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Tilt aircraft so NOSE points UP (tail on table)"));
    Serial.println(F("▶ Hold steady at approximately 90 degrees"));
    Serial.println(F("▶ Aircraft should be vertical with nose pointing at ceiling"));

    if (!waitForConfirmation()) return;

    delay(1000);
    float accX_noseUp = 0, accY_noseUp = 0, accZ_noseUp = 0;
    for (int i = 0; i < 100; i++) {
        getIMUdata();
        accX_noseUp += AccX;
        accY_noseUp += AccY;
        accZ_noseUp += AccZ;
        delay(10);
    }
    accX_noseUp /= 100.0;
    accY_noseUp /= 100.0;
    accZ_noseUp /= 100.0;

    Serial.print(F("Recorded: AccX=")); Serial.print(accX_noseUp, 3);
    Serial.print(F(" AccY=")); Serial.print(accY_noseUp, 3);
    Serial.print(F(" AccZ=")); Serial.println(accZ_noseUp, 3);
    Serial.println(F("✅ Nose up position recorded\n"));

    // STEP 3: RIGHT SIDE UP ORIENTATION TEST
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 3: RIGHT SIDE UP ORIENTATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Tilt aircraft so RIGHT SIDE points UP (left wing on table)"));
    Serial.println(F("▶ Nose should still point FORWARD (not left/right)"));
    Serial.println(F("▶ Aircraft rolled 90° to the right"));

    if (!waitForConfirmation()) return;

    delay(1000);
    float accX_rightUp = 0, accY_rightUp = 0, accZ_rightUp = 0;
    for (int i = 0; i < 100; i++) {
        getIMUdata();
        accX_rightUp += AccX;
        accY_rightUp += AccY;
        accZ_rightUp += AccZ;
        delay(10);
    }
    accX_rightUp /= 100.0;
    accY_rightUp /= 100.0;
    accZ_rightUp /= 100.0;

    Serial.print(F("Recorded: AccX=")); Serial.print(accX_rightUp, 3);
    Serial.print(F(" AccY=")); Serial.print(accY_rightUp, 3);
    Serial.print(F(" AccZ=")); Serial.println(accZ_rightUp, 3);
    Serial.println(F("✅ Right side up position recorded\n"));

    // STEP 4: TOP UP ORIENTATION TEST (verify)
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 4: TOP UP ORIENTATION (VERIFICATION)"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Return aircraft to NORMAL position (level on table)"));
    Serial.println(F("▶ Top/back pointing UP, bottom/belly on table"));
    Serial.println(F("▶ This verifies our measurements"));

    if (!waitForConfirmation()) return;

    delay(1000);
    float accX_topUp = 0, accY_topUp = 0, accZ_topUp = 0;
    for (int i = 0; i < 100; i++) {
        getIMUdata();
        accX_topUp += AccX;
        accY_topUp += AccY;
        accZ_topUp += AccZ;
        delay(10);
    }
    accX_topUp /= 100.0;
    accY_topUp /= 100.0;
    accZ_topUp /= 100.0;

    Serial.print(F("Recorded: AccX=")); Serial.print(accX_topUp, 3);
    Serial.print(F(" AccY=")); Serial.print(accY_topUp, 3);
    Serial.print(F(" AccZ=")); Serial.println(accZ_topUp, 3);
    Serial.println(F("✅ Top up position recorded\n"));

    // ANALYZE ORIENTATION
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("ANALYZING IMU ORIENTATION..."));
    Serial.println(F("═══════════════════════════════════════════════════════════\n"));

    // Determine which IMU axis corresponds to each aircraft axis
    char forwardAxis, rightAxis, upAxis;
    bool forwardInverted, rightInverted, upInverted;

    // Find FORWARD axis (largest when nose up)
    float absX_nose = abs(accX_noseUp);
    float absY_nose = abs(accY_noseUp);
    float absZ_nose = abs(accZ_noseUp);

    if (absX_nose > absY_nose && absX_nose > absZ_nose) {
        forwardAxis = 'X';
        forwardInverted = (accX_noseUp < 0);
    } else if (absY_nose > absX_nose && absY_nose > absZ_nose) {
        forwardAxis = 'Y';
        forwardInverted = (accY_noseUp < 0);
    } else {
        forwardAxis = 'Z';
        forwardInverted = (accZ_noseUp < 0);
    }

    // Find RIGHT axis (largest when right side up)
    float absX_right = abs(accX_rightUp);
    float absY_right = abs(accY_rightUp);
    float absZ_right = abs(accZ_rightUp);

    if (absX_right > absY_right && absX_right > absZ_right && forwardAxis != 'X') {
        rightAxis = 'X';
        rightInverted = (accX_rightUp < 0);
    } else if (absY_right > absX_right && absY_right > absZ_right && forwardAxis != 'Y') {
        rightAxis = 'Y';
        rightInverted = (accY_rightUp < 0);
    } else {
        rightAxis = 'Z';
        rightInverted = (accZ_rightUp < 0);
    }

    // Find UP axis (whichever is left)
    if (forwardAxis != 'X' && rightAxis != 'X') {
        upAxis = 'X';
        upInverted = (accX_topUp < 0);
    } else if (forwardAxis != 'Y' && rightAxis != 'Y') {
        upAxis = 'Y';
        upInverted = (accY_topUp < 0);
    } else {
        upAxis = 'Z';
        upInverted = (accZ_topUp < 0);
    }

    // Print results
    printIMUCalibrationResults(AccErrorX, AccErrorY, AccErrorZ, GyroErrorX, GyroErrorY, GyroErrorZ);
    printIMUOrientationResults(forwardAxis, forwardInverted, rightAxis, rightInverted, upAxis, upInverted);
}

void printIMUOrientationResults(char forwardAxis, bool forwardInverted, char rightAxis, bool rightInverted, char upAxis, bool upInverted) {
    // Store results for dump command
    calResults.hasOrientation = true;
    calResults.forwardAxis = forwardAxis;
    calResults.forwardInverted = forwardInverted;
    calResults.rightAxis = rightAxis;
    calResults.rightInverted = rightInverted;
    calResults.upAxis = upAxis;
    calResults.upInverted = upInverted;

    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     DETECTED IMU ORIENTATION                              ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.print(F("Aircraft FORWARD → IMU "));
    Serial.print(forwardInverted ? "-" : "+");
    Serial.println(forwardAxis);

    Serial.print(F("Aircraft RIGHT   → IMU "));
    Serial.print(rightInverted ? "-" : "+");
    Serial.println(rightAxis);

    Serial.print(F("Aircraft UP      → IMU "));
    Serial.print(upInverted ? "-" : "+");
    Serial.println(upAxis);
    Serial.println();

    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     AXIS TRANSFORMATION CODE                              ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.println(F("📋 Open 'src/main.cpp', find function 'getIMUdata()'"));
    Serial.println(F("📋 ADD this code AFTER the existing calibration/filtering"));
    Serial.println(F("📋 Look for comment '// Apply axis transformation' and ADD:\n"));

    Serial.println(F("// ========== AXIS TRANSFORMATION (Auto-generated) =========="));
    Serial.println(F("// Transform IMU frame to aircraft frame"));
    Serial.println(F("float AccX_aircraft, AccY_aircraft, AccZ_aircraft;"));
    Serial.println(F("float GyroX_aircraft, GyroY_aircraft, GyroZ_aircraft;"));
    Serial.println();

    // Generate transformation for accelerometer
    Serial.print(F("AccX_aircraft = "));  // Aircraft forward (pitch axis)
    if (forwardInverted) Serial.print(F("-"));
    Serial.print(F("Acc")); Serial.print(forwardAxis); Serial.println(F(";"));

    Serial.print(F("AccY_aircraft = "));  // Aircraft right (roll axis)
    if (rightInverted) Serial.print(F("-"));
    Serial.print(F("Acc")); Serial.print(rightAxis); Serial.println(F(";"));

    Serial.print(F("AccZ_aircraft = "));  // Aircraft up (vertical axis)
    if (upInverted) Serial.print(F("-"));
    Serial.print(F("Acc")); Serial.print(upAxis); Serial.println(F(";"));
    Serial.println();

    // Generate transformation for gyroscope
    Serial.print(F("GyroX_aircraft = "));
    if (forwardInverted) Serial.print(F("-"));
    Serial.print(F("Gyro")); Serial.print(forwardAxis); Serial.println(F(";"));

    Serial.print(F("GyroY_aircraft = "));
    if (rightInverted) Serial.print(F("-"));
    Serial.print(F("Gyro")); Serial.print(rightAxis); Serial.println(F(";"));

    Serial.print(F("GyroZ_aircraft = "));
    if (upInverted) Serial.print(F("-"));
    Serial.print(F("Gyro")); Serial.print(upAxis); Serial.println(F(";"));
    Serial.println();

    Serial.println(F("// Replace original values with transformed values"));
    Serial.println(F("AccX = AccX_aircraft;"));
    Serial.println(F("AccY = AccY_aircraft;"));
    Serial.println(F("AccZ = AccZ_aircraft;"));
    Serial.println(F("GyroX = GyroX_aircraft;"));
    Serial.println(F("GyroY = GyroY_aircraft;"));
    Serial.println(F("GyroZ = GyroZ_aircraft;"));
    Serial.println(F("// ========== END AXIS TRANSFORMATION ==========\n"));

    Serial.println(F("\n📋 Flash the live build: pio run -e teensy40 --target upload\n"));

    Serial.println(F("═══════════════════════════════════════════════════════════\n"));
}
