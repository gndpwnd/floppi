/* Calibration Module - IMU Calibration (basic + 6-position) */

#include "calibration.h"
#include "config.h"

//========================================================================================================================//
//                                         IMU CALIBRATION FUNCTION                                                       //
//========================================================================================================================//

void calibrateIMU() {
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     IMU CALIBRATION PROGRAM                               ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.println(F("INSTRUCTIONS:"));
    Serial.println(F("  1. Place the aircraft on a FLAT, LEVEL surface"));
    Serial.println(F("  2. Do NOT touch or move the aircraft during calibration"));
    Serial.println(F("  3. Keep the area VIBRATION-FREE (no fans, motors, etc.)"));
    Serial.println(F("  4. This will take approximately 5 seconds\n"));

    if (!waitForConfirmation(5)) return;

    // Check if IMU is stable before starting
    Serial.println(F("Checking IMU stability..."));
    delay(1000);

    float gyroStability = 0;
    float accelStability = 0;
    for (int i = 0; i < 100; i++) {
        getIMUdata();
        gyroStability += abs(GyroX) + abs(GyroY) + abs(GyroZ);
        accelStability += abs(AccZ - 1.0);
        delay(10);
    }
    gyroStability /= 100.0;
    accelStability /= 100.0;

    if (gyroStability > 5.0) {
        Serial.println(F("⚠️  WARNING: Gyro readings unstable (vehicle moving or vibrating)"));
        Serial.print(F("   Stability metric: ")); Serial.println(gyroStability);
        Serial.println(F("   Place on stable surface and try again."));
        Serial.println(F("\nRetry? (y/n)"));
        if (waitForConfirmation(3)) {
            calibrateIMU(); // Recursive retry
        }
        return;
    }

    if (accelStability > 0.2) {
        Serial.println(F("⚠️  WARNING: Surface may not be level"));
        Serial.print(F("   Level metric: ")); Serial.println(accelStability);
        Serial.println(F("   Try to find a more level surface."));
        Serial.println(F("\nContinue anyway? (y/n)"));
        if (!waitForConfirmation(3)) return;
    }

    Serial.println(F("✅ IMU stable, starting calibration...\n"));
    Serial.println(F("Starting in 3 seconds..."));
    delay(1000);
    Serial.println(F("2..."));
    delay(1000);
    Serial.println(F("1..."));
    delay(1000);
    Serial.println(F("\n*** CALIBRATING - DO NOT MOVE! ***\n"));

    // Collect samples
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

        // Progress indicator
        if (i % 200 == 0) {
            Serial.print(F("."));
        }

        delay(2);
    }
    Serial.println();

    // Calculate errors
    float AccErrorX = AccX_sum / numSamples;
    float AccErrorY = AccY_sum / numSamples;
    float AccErrorZ = (AccZ_sum / numSamples) - 1.0; // Should be 1g when level

    float GyroErrorX = GyroX_sum / numSamples;
    float GyroErrorY = GyroY_sum / numSamples;
    float GyroErrorZ = GyroZ_sum / numSamples;

    Serial.println(F("\n*** CALIBRATION COMPLETE! ***\n"));

    // Validate results
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     CALIBRATION QUALITY CHECK                             ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    bool goodCal = true;

    // Check accelerometer
    if (abs(AccErrorX) > 0.2) {
        Serial.println(F("⚠️  AccX offset is large - surface may not be level"));
        goodCal = false;
    } else {
        Serial.println(F("✅ AccX offset good"));
    }

    if (abs(AccErrorY) > 0.2) {
        Serial.println(F("⚠️  AccY offset is large - surface may not be level"));
        goodCal = false;
    } else {
        Serial.println(F("✅ AccY offset good"));
    }

    if (abs(AccErrorZ) > 0.1) {
        Serial.println(F("⚠️  AccZ offset is large - check sensor orientation"));
        goodCal = false;
    } else {
        Serial.println(F("✅ AccZ offset good"));
    }

    // Check gyro
    if (abs(GyroErrorX) > 2.0) {
        Serial.println(F("⚠️  GyroX drift is high - vehicle may have moved"));
        goodCal = false;
    } else {
        Serial.println(F("✅ GyroX drift acceptable"));
    }

    if (abs(GyroErrorY) > 2.0) {
        Serial.println(F("⚠️  GyroY drift is high - vehicle may have moved"));
        goodCal = false;
    } else {
        Serial.println(F("✅ GyroY drift acceptable"));
    }

    if (abs(GyroErrorZ) > 2.0) {
        Serial.println(F("⚠️  GyroZ drift is high - vehicle may have moved"));
        goodCal = false;
    } else {
        Serial.println(F("✅ GyroZ drift acceptable"));
    }

    Serial.println();

    if (!goodCal) {
        Serial.println(F("⚠️  Calibration quality is marginal."));
        Serial.println(F("\nRetry calibration? (y/n)"));
        if (waitForConfirmation(3)) {
            calibrateIMU(); // Recursive retry
            return;
        }
    } else {
        Serial.println(F("✅ Calibration quality is EXCELLENT!\n"));
    }

    printIMUCalibrationResults(AccErrorX, AccErrorY, AccErrorZ, GyroErrorX, GyroErrorY, GyroErrorZ);
}

void printIMUCalibrationResults(float AccErrorX, float AccErrorY, float AccErrorZ, float GyroErrorX, float GyroErrorY, float GyroErrorZ) {
    // Store results for dump command
    calResults.hasIMU = true;
    calResults.accErrorX = AccErrorX;
    calResults.accErrorY = AccErrorY;
    calResults.accErrorZ = AccErrorZ;
    calResults.gyroErrorX = GyroErrorX;
    calResults.gyroErrorY = GyroErrorY;
    calResults.gyroErrorZ = GyroErrorZ;

    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     COPY-PASTE READY CODE                                 ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.println(F("📋 STEP 1: Open 'include/config.h'"));
    Serial.println(F("📋 STEP 2: Find the section: '// IMU Calibration Values'"));
    Serial.println(F("📋 STEP 3: REPLACE the existing values with these:\n"));

    Serial.println(F("//============================================================================="));
    Serial.println(F("// IMU Calibration Values (Generated by calibration program)"));
    Serial.println(F("//============================================================================="));
    Serial.print(F("#define IMU_ACC_ERROR_X ")); Serial.print(AccErrorX, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_ACC_ERROR_Y ")); Serial.print(AccErrorY, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_ACC_ERROR_Z ")); Serial.print(AccErrorZ, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_GYRO_ERROR_X ")); Serial.print(GyroErrorX, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_GYRO_ERROR_Y ")); Serial.print(GyroErrorY, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_GYRO_ERROR_Z ")); Serial.print(GyroErrorZ, 6); Serial.println(F("f"));
    Serial.println();

    Serial.println(F("\n📋 STEP 4: Flash the live build: pio run -e teensy40 --target upload"));
    Serial.println(F("📋 STEP 5: Your IMU is now calibrated!\n"));

    Serial.println(F("═══════════════════════════════════════════════════════════\n"));
}

//========================================================================================================================//
//                                   6-POSITION ACCELEROMETER CALIBRATION                                                 //
//========================================================================================================================//

void calibrateIMU6Position() {
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     6-POSITION ACCELEROMETER CALIBRATION                  ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.println(F("This calibration measures gravity in 6 orientations to calculate"));
    Serial.println(F("both OFFSET and SCALE FACTOR for each accelerometer axis."));
    Serial.println(F("\nThis is more accurate than single-position calibration but"));
    Serial.println(F("requires holding the aircraft in 6 different positions.\n"));

    Serial.println(F("You will need to hold the aircraft in these positions:"));
    Serial.println(F("  1. Level (top up)"));
    Serial.println(F("  2. Upside down (top down)"));
    Serial.println(F("  3. Nose up"));
    Serial.println(F("  4. Nose down"));
    Serial.println(F("  5. Right side up"));
    Serial.println(F("  6. Left side up\n"));

    if (!waitForConfirmation(5)) return;

    // Storage for measurements
    float measurements[6][3]; // [position][axis] - stores AccX, AccY, AccZ for each position
    const char* positionNames[] = {
        "LEVEL (top up, normal position)",
        "UPSIDE DOWN (top down)",
        "NOSE UP (tail on table)",
        "NOSE DOWN (nose on table)",
        "RIGHT SIDE UP (left wing on table)",
        "LEFT SIDE UP (right wing on table)"
    };

    // Collect data for each position
    for (int pos = 0; pos < 6; pos++) {
        Serial.println(F("\n═══════════════════════════════════════════════════════════"));
        Serial.print(F("POSITION ")); Serial.print(pos + 1); Serial.println(F(" of 6"));
        Serial.println(F("═══════════════════════════════════════════════════════════"));
        Serial.print(F("▶ Hold aircraft: ")); Serial.println(positionNames[pos]);
        Serial.println(F("▶ Keep STEADY for 2 seconds during measurement"));

        if (!waitForConfirmation(3)) return;

        Serial.println(F("Measuring..."));
        delay(500); // Settle time

        // Collect samples
        float sumX = 0, sumY = 0, sumZ = 0;
        int numSamples = 500;

        for (int i = 0; i < numSamples; i++) {
            getIMUdata();
            sumX += AccX;
            sumY += AccY;
            sumZ += AccZ;
            if (i % 50 == 0) Serial.print(F("."));
            delay(2);
        }
        Serial.println();

        measurements[pos][0] = sumX / numSamples;
        measurements[pos][1] = sumY / numSamples;
        measurements[pos][2] = sumZ / numSamples;

        Serial.print(F("✅ Recorded: X=")); Serial.print(measurements[pos][0], 4);
        Serial.print(F(" Y=")); Serial.print(measurements[pos][1], 4);
        Serial.print(F(" Z=")); Serial.println(measurements[pos][2], 4);
    }

    // Calculate offsets and scale factors
    // Position mapping:
    // 0: Level (Z up, Z should read +1g)
    // 1: Upside down (Z down, Z should read -1g)
    // 2: Nose up (X up, X should read +1g)
    // 3: Nose down (X down, X should read -1g)
    // 4: Right side up (Y up, Y should read +1g)
    // 5: Left side up (Y down, Y should read -1g)

    // For each axis: offset = (positive + negative) / 2
    //                scale = 2.0 / (positive - negative)

    // X axis: nose up (+1g) and nose down (-1g)
    float x_plus = measurements[2][0];  // Nose up
    float x_minus = measurements[3][0]; // Nose down
    float AccOffsetX = (x_plus + x_minus) / 2.0;
    float AccScaleX = 2.0 / (x_plus - x_minus);

    // Y axis: right side up (+1g) and left side up (-1g)
    float y_plus = measurements[4][1];  // Right side up
    float y_minus = measurements[5][1]; // Left side up
    float AccOffsetY = (y_plus + y_minus) / 2.0;
    float AccScaleY = 2.0 / (y_plus - y_minus);

    // Z axis: level (+1g) and upside down (-1g)
    float z_plus = measurements[0][2];  // Level
    float z_minus = measurements[1][2]; // Upside down
    float AccOffsetZ = (z_plus + z_minus) / 2.0;
    float AccScaleZ = 2.0 / (z_plus - z_minus);

    // Also calibrate gyro (take average from level position)
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("GYRO CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Return aircraft to LEVEL position"));
    Serial.println(F("▶ Keep STILL for gyro calibration"));

    if (!waitForConfirmation(3)) return;

    float GyroX_sum = 0, GyroY_sum = 0, GyroZ_sum = 0;
    int numGyroSamples = 1000;

    for (int i = 0; i < numGyroSamples; i++) {
        getIMUdata();
        GyroX_sum += GyroX;
        GyroY_sum += GyroY;
        GyroZ_sum += GyroZ;
        if (i % 100 == 0) Serial.print(F("."));
        delay(2);
    }
    Serial.println();

    float GyroErrorX = GyroX_sum / numGyroSamples;
    float GyroErrorY = GyroY_sum / numGyroSamples;
    float GyroErrorZ = GyroZ_sum / numGyroSamples;

    // Quality check
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     CALIBRATION QUALITY CHECK                             ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    bool goodCal = true;

    // Check scale factors (should be close to 1.0)
    if (abs(AccScaleX - 1.0) > 0.05) {
        Serial.print(F("⚠️  X scale factor off by "));
        Serial.print((AccScaleX - 1.0) * 100, 1);
        Serial.println(F("%"));
    } else {
        Serial.print(F("✅ X scale factor good ("));
        Serial.print(AccScaleX, 4);
        Serial.println(F(")"));
    }

    if (abs(AccScaleY - 1.0) > 0.05) {
        Serial.print(F("⚠️  Y scale factor off by "));
        Serial.print((AccScaleY - 1.0) * 100, 1);
        Serial.println(F("%"));
    } else {
        Serial.print(F("✅ Y scale factor good ("));
        Serial.print(AccScaleY, 4);
        Serial.println(F(")"));
    }

    if (abs(AccScaleZ - 1.0) > 0.05) {
        Serial.print(F("⚠️  Z scale factor off by "));
        Serial.print((AccScaleZ - 1.0) * 100, 1);
        Serial.println(F("%"));
    } else {
        Serial.print(F("✅ Z scale factor good ("));
        Serial.print(AccScaleZ, 4);
        Serial.println(F(")"));
    }

    // Check gyro
    if (abs(GyroErrorX) > 2.0 || abs(GyroErrorY) > 2.0 || abs(GyroErrorZ) > 2.0) {
        Serial.println(F("⚠️  Gyro drift higher than expected"));
        goodCal = false;
    } else {
        Serial.println(F("✅ Gyro drift acceptable"));
    }

    if (!goodCal) {
        Serial.println(F("\n⚠️  Some values are outside ideal range."));
        Serial.println(F("Retry calibration? (y/n)"));
        if (waitForConfirmation(3)) {
            calibrateIMU6Position();
            return;
        }
    }

    // Print results
    print6PositionCalibrationResults(AccOffsetX, AccOffsetY, AccOffsetZ,
                                      AccScaleX, AccScaleY, AccScaleZ,
                                      GyroErrorX, GyroErrorY, GyroErrorZ);
}

void print6PositionCalibrationResults(float AccOffsetX, float AccOffsetY, float AccOffsetZ,
                                       float AccScaleX, float AccScaleY, float AccScaleZ,
                                       float GyroErrorX, float GyroErrorY, float GyroErrorZ) {
    // Store results for dump command
    calResults.hasIMU = true;
    calResults.accErrorX = AccOffsetX;
    calResults.accErrorY = AccOffsetY;
    calResults.accErrorZ = AccOffsetZ;
    calResults.gyroErrorX = GyroErrorX;
    calResults.gyroErrorY = GyroErrorY;
    calResults.gyroErrorZ = GyroErrorZ;
    calResults.hasIMUScale = true;
    calResults.accScaleX = AccScaleX;
    calResults.accScaleY = AccScaleY;
    calResults.accScaleZ = AccScaleZ;

    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     6-POSITION CALIBRATION RESULTS                        ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));

    Serial.println(F("📋 STEP 1: Open 'include/config.h'"));
    Serial.println(F("📋 STEP 2: Find the section: '// IMU Calibration Values'"));
    Serial.println(F("📋 STEP 3: REPLACE with these values:\n"));

    Serial.println(F("//============================================================================="));
    Serial.println(F("// IMU Calibration Values (Generated by 6-position calibration)"));
    Serial.println(F("//============================================================================="));
    Serial.println(F("// Accelerometer offsets"));
    Serial.print(F("#define IMU_ACC_ERROR_X ")); Serial.print(AccOffsetX, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_ACC_ERROR_Y ")); Serial.print(AccOffsetY, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_ACC_ERROR_Z ")); Serial.print(AccOffsetZ, 6); Serial.println(F("f"));
    Serial.println(F("// Accelerometer scale factors"));
    Serial.print(F("#define IMU_ACC_SCALE_X ")); Serial.print(AccScaleX, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_ACC_SCALE_Y ")); Serial.print(AccScaleY, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_ACC_SCALE_Z ")); Serial.print(AccScaleZ, 6); Serial.println(F("f"));
    Serial.println(F("// Gyroscope offsets"));
    Serial.print(F("#define IMU_GYRO_ERROR_X ")); Serial.print(GyroErrorX, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_GYRO_ERROR_Y ")); Serial.print(GyroErrorY, 6); Serial.println(F("f"));
    Serial.print(F("#define IMU_GYRO_ERROR_Z ")); Serial.print(GyroErrorZ, 6); Serial.println(F("f"));
    Serial.println();

    Serial.println(F("\n📋 STEP 4: Open 'src/imu.cpp'"));
    Serial.println(F("📋 STEP 5: Find getIMUdata() and update accel correction to:\n"));

    Serial.println(F("// Apply 6-position calibration (offset then scale)"));
    Serial.println(F("AccX = (AccX - IMU_ACC_ERROR_X) * IMU_ACC_SCALE_X;"));
    Serial.println(F("AccY = (AccY - IMU_ACC_ERROR_Y) * IMU_ACC_SCALE_Y;"));
    Serial.println(F("AccZ = (AccZ - IMU_ACC_ERROR_Z) * IMU_ACC_SCALE_Z;"));
    Serial.println();

    Serial.println(F("\n📋 STEP 6: Flash the live build: pio run -e teensy40 --target upload\n"));

    Serial.println(F("═══════════════════════════════════════════════════════════\n"));
}
