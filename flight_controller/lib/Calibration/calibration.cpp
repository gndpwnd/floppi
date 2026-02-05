/*
 * Calibration Module - Implementation
 * Auto-calibration for Radio and IMU with step-by-step guidance
 */

#include "calibration.h"
#include "radioComm.h"

// Already declared in calibration.h, but let's ensure they're accessible
extern unsigned long channel_1_pwm, channel_2_pwm, channel_3_pwm;
extern unsigned long channel_4_pwm, channel_5_pwm, channel_6_pwm;

// These functions are in main.cpp, declared in calibration.h
extern void getIMUdata();
extern void getCommands();

//========================================================================================================================//
//                                              HELPER FUNCTIONS                                                          //
//========================================================================================================================//

bool waitForConfirmation(int timeoutSeconds) {
    // Wait for user to type 'y' or 'n' with timeout
    unsigned long startTime = millis();
    unsigned long timeout = timeoutSeconds * 1000;
    
    // Clear serial buffer
    while (Serial.available()) Serial.read();
    
    Serial.print(F("\nType 'y' to continue, 'n' to wait (timeout "));
    Serial.print(timeoutSeconds);
    Serial.println(F(" seconds): "));
    
    while (millis() - startTime < timeout) {
        if (Serial.available() > 0) {
            char response = Serial.read();
            while (Serial.available()) Serial.read(); // Clear rest of buffer
            
            if (response == 'y' || response == 'Y') {
                Serial.println(F("✓ Confirmed!"));
                return true;
            }
            else if (response == 'n' || response == 'N') {
                Serial.println(F("Waiting 3 seconds..."));
                delay(3000);
                return waitForConfirmation(timeoutSeconds); // Recursive call
            }
        }
        delay(10);
    }
    
    // Timeout - assume 'y'
    Serial.println(F("✓ Timeout - continuing..."));
    return true;
}

int detectMovedChannel(uint16_t neutral1, uint16_t current1,
                      uint16_t neutral2, uint16_t current2,
                      uint16_t neutral3, uint16_t current3,
                      uint16_t neutral4, uint16_t current4,
                      uint16_t neutral5, uint16_t current5,
                      uint16_t neutral6, uint16_t current6,
                      int exclude1, int exclude2, int exclude3) {
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
        if (i == exclude1 || i == exclude2 || i == exclude3) continue;
        
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
//                                         RADIO CALIBRATION FUNCTION                                                     //
//========================================================================================================================//

void calibrateRadio() {
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     RADIO CALIBRATION & CHANNEL MAPPING                   ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));
    
    Serial.println(F("This program will:"));
    Serial.println(F("  ✓ Detect which physical control maps to which channel"));
    Serial.println(F("  ✓ Measure min/max/center for each control"));
    Serial.println(F("  ✓ Detect your switch types (2-pos vs 3-pos)\n"));
    
    Serial.println(F("⚠️  IMPORTANT RULES:"));
    Serial.println(F("  • Move ONLY the control requested"));
    Serial.println(F("  • Keep ALL other controls centered/neutral"));
    Serial.println(F("  • Follow timing instructions carefully\n"));
    
    if (!waitForConfirmation(5)) return;
    
    // Storage for channel data
    struct ChannelData {
        uint16_t min, max, center, neutral;
        String physicalControl;
    };
    ChannelData channels[6];
    
    // Initialize - read neutral positions
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 0: NEUTRAL POSITION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Center ALL sticks"));
    Serial.println(F("▶ Set ALL switches to their middle or OFF position"));
    
    if (!waitForConfirmation(3)) return;
    
    Serial.println(F("Recording neutral positions..."));
    delay(500);
    getCommands();
    channels[0].neutral = channel_1_pwm;
    channels[1].neutral = channel_2_pwm;
    channels[2].neutral = channel_3_pwm;
    channels[3].neutral = channel_4_pwm;
    channels[4].neutral = channel_5_pwm;
    channels[5].neutral = channel_6_pwm;
    
    Serial.print(F("✓ Neutral: CH1=")); Serial.print(channels[0].neutral);
    Serial.print(F(" CH2=")); Serial.print(channels[1].neutral);
    Serial.print(F(" CH3=")); Serial.print(channels[2].neutral);
    Serial.print(F(" CH4=")); Serial.print(channels[3].neutral);
    Serial.print(F(" CH5=")); Serial.print(channels[4].neutral);
    Serial.print(F(" CH6=")); Serial.println(channels[5].neutral);
    
    // STEP 1: THROTTLE STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 1: THROTTLE STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the THROTTLE stick (typically LEFT stick UP/DOWN)"));
    Serial.println(F("▶ Move to MINIMUM (full down)"));
    Serial.println(F("▶ Hold for 2 seconds..."));
    
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    
    int throttleChannel = detectMovedChannel(
        channels[0].neutral, channel_1_pwm,
        channels[1].neutral, channel_2_pwm,
        channels[2].neutral, channel_3_pwm,
        channels[3].neutral, channel_4_pwm,
        channels[4].neutral, channel_5_pwm,
        channels[5].neutral, channel_6_pwm
    );
    
    if (throttleChannel == -1) {
        Serial.println(F("❌ ERROR: No channel detected movement!"));
        Serial.println(F("   Did you move the throttle stick?"));
        return;
    }
    
    Serial.print(F("✓ Throttle detected on CH")); Serial.println(throttleChannel + 1);
    uint16_t throttle_min = getChannelValue(throttleChannel);
    channels[throttleChannel].min = throttle_min;
    channels[throttleChannel].physicalControl = "Throttle";
    
    Serial.println(F("\n▶ Now move throttle to MAXIMUM (full up)"));
    Serial.println(F("▶ Hold for 2 seconds..."));
    
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    uint16_t throttle_max = getChannelValue(throttleChannel);
    channels[throttleChannel].max = throttle_max;
    channels[throttleChannel].center = (throttle_min + throttle_max) / 2;
    
    Serial.print(F("✓ Throttle range: ")); 
    Serial.print(throttle_min); 
    Serial.print(F(" to ")); 
    Serial.println(throttle_max);
    
    Serial.println(F("\n▶ Return throttle to center/neutral"));
    if (!waitForConfirmation(3)) return;
    
    // STEP 2: ROLL STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 2: ROLL STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the ROLL stick (typically RIGHT stick LEFT/RIGHT)"));
    Serial.println(F("▶ Move to full LEFT"));
    
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    int rollChannel = detectMovedChannel(
        channels[0].neutral, channel_1_pwm,
        channels[1].neutral, channel_2_pwm,
        channels[2].neutral, channel_3_pwm,
        channels[3].neutral, channel_4_pwm,
        channels[4].neutral, channel_5_pwm,
        channels[5].neutral, channel_6_pwm,
        throttleChannel
    );
    
    if (rollChannel == -1 || rollChannel == throttleChannel) {
        Serial.println(F("❌ ERROR: Could not detect roll channel"));
        return;
    }
    
    Serial.print(F("✓ Roll detected on CH")); Serial.println(rollChannel + 1);
    uint16_t roll_left = getChannelValue(rollChannel);
    
    Serial.println(F("\n▶ Now move roll to full RIGHT"));
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    uint16_t roll_right = getChannelValue(rollChannel);
    
    channels[rollChannel].min = min(roll_left, roll_right);
    channels[rollChannel].max = max(roll_left, roll_right);
    channels[rollChannel].center = channels[rollChannel].neutral;
    channels[rollChannel].physicalControl = "Roll";
    
    Serial.print(F("✓ Roll range: ")); 
    Serial.print(channels[rollChannel].min); 
    Serial.print(F(" to ")); 
    Serial.println(channels[rollChannel].max);
    
    Serial.println(F("\n▶ Return roll to center"));
    if (!waitForConfirmation(3)) return;
    
    // STEP 3: PITCH STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 3: PITCH STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the PITCH stick (typically RIGHT stick UP/DOWN)"));
    Serial.println(F("▶ Move to full FORWARD (down)"));
    
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    int pitchChannel = detectMovedChannel(
        channels[0].neutral, channel_1_pwm,
        channels[1].neutral, channel_2_pwm,
        channels[2].neutral, channel_3_pwm,
        channels[3].neutral, channel_4_pwm,
        channels[4].neutral, channel_5_pwm,
        channels[5].neutral, channel_6_pwm,
        throttleChannel, rollChannel
    );
    
    if (pitchChannel == -1) {
        Serial.println(F("❌ ERROR: Could not detect pitch channel"));
        return;
    }
    
    Serial.print(F("✓ Pitch detected on CH")); Serial.println(pitchChannel + 1);
    uint16_t pitch_fwd = getChannelValue(pitchChannel);
    
    Serial.println(F("\n▶ Now move pitch to full BACK (up)"));
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    uint16_t pitch_back = getChannelValue(pitchChannel);
    
    channels[pitchChannel].min = min(pitch_fwd, pitch_back);
    channels[pitchChannel].max = max(pitch_fwd, pitch_back);
    channels[pitchChannel].center = channels[pitchChannel].neutral;
    channels[pitchChannel].physicalControl = "Pitch";
    
    Serial.print(F("✓ Pitch range: ")); 
    Serial.print(channels[pitchChannel].min); 
    Serial.print(F(" to ")); 
    Serial.println(channels[pitchChannel].max);
    
    Serial.println(F("\n▶ Return pitch to center"));
    if (!waitForConfirmation(3)) return;
    
    // STEP 4: YAW STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 4: YAW STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the YAW stick (typically LEFT stick LEFT/RIGHT)"));
    Serial.println(F("▶ Move to full LEFT"));
    
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    int yawChannel = detectMovedChannel(
        channels[0].neutral, channel_1_pwm,
        channels[1].neutral, channel_2_pwm,
        channels[2].neutral, channel_3_pwm,
        channels[3].neutral, channel_4_pwm,
        channels[4].neutral, channel_5_pwm,
        channels[5].neutral, channel_6_pwm,
        throttleChannel, rollChannel, pitchChannel
    );
    
    if (yawChannel == -1) {
        Serial.println(F("❌ ERROR: Could not detect yaw channel"));
        return;
    }
    
    Serial.print(F("✓ Yaw detected on CH")); Serial.println(yawChannel + 1);
    uint16_t yaw_left = getChannelValue(yawChannel);
    
    Serial.println(F("\n▶ Now move yaw to full RIGHT"));
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    uint16_t yaw_right = getChannelValue(yawChannel);
    
    channels[yawChannel].min = min(yaw_left, yaw_right);
    channels[yawChannel].max = max(yaw_left, yaw_right);
    channels[yawChannel].center = channels[yawChannel].neutral;
    channels[yawChannel].physicalControl = "Yaw";
    
    Serial.print(F("✓ Yaw range: ")); 
    Serial.print(channels[yawChannel].min); 
    Serial.print(F(" to ")); 
    Serial.println(channels[yawChannel].max);
    
    Serial.println(F("\n▶ Return yaw to center"));
    if (!waitForConfirmation(3)) return;
    
    // STEP 5: AUX1 (Typically throttle cut / arm switch)
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 5: AUX1 SWITCH CALIBRATION (Throttle Cut)"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the AUX1 switch (typically a 2-position switch)"));
    Serial.println(F("▶ Move to LOW position"));
    
    if (!waitForConfirmation(3)) return;
    
    delay(500);
    getCommands();
    int aux1Channel = detectMovedChannel(
        channels[0].neutral, channel_1_pwm,
        channels[1].neutral, channel_2_pwm,
        channels[2].neutral, channel_3_pwm,
        channels[3].neutral, channel_4_pwm,
        channels[4].neutral, channel_5_pwm,
        channels[5].neutral, channel_6_pwm,
        throttleChannel, rollChannel, pitchChannel
    );
    
    // If no aux1 detected, skip
    int aux2Channel = -1;
    if (aux1Channel == -1) {
        Serial.println(F("⚠️  No AUX1 detected (optional)"));
        aux1Channel = 4; // Default to CH5
    } else {
        Serial.print(F("✓ AUX1 detected on CH")); Serial.println(aux1Channel + 1);
        uint16_t aux1_low = getChannelValue(aux1Channel);
        
        Serial.println(F("\n▶ Now move AUX1 to HIGH position"));
        if (!waitForConfirmation(3)) return;
        
        delay(500);
        getCommands();
        uint16_t aux1_high = getChannelValue(aux1Channel);
        
        channels[aux1Channel].min = min(aux1_low, aux1_high);
        channels[aux1Channel].max = max(aux1_low, aux1_high);
        channels[aux1Channel].center = (aux1_low + aux1_high) / 2;
        channels[aux1Channel].physicalControl = "Aux1";
        
        Serial.print(F("✓ AUX1 range: ")); 
        Serial.print(channels[aux1Channel].min); 
        Serial.print(F(" to ")); 
        Serial.println(channels[aux1Channel].max);
        
        // STEP 6: AUX2 (Optional)
        Serial.println(F("\n═══════════════════════════════════════════════════════════"));
        Serial.println(F("STEP 6: AUX2 SWITCH CALIBRATION (Optional)"));
        Serial.println(F("═══════════════════════════════════════════════════════════"));
        Serial.println(F("▶ If you have another switch, move it now"));
        Serial.println(F("▶ Otherwise, type 'n' to skip"));
        
        if (waitForConfirmation(3)) {
            delay(500);
            getCommands();
            aux2Channel = detectMovedChannel(
                channels[0].neutral, channel_1_pwm,
                channels[1].neutral, channel_2_pwm,
                channels[2].neutral, channel_3_pwm,
                channels[3].neutral, channel_4_pwm,
                channels[4].neutral, channel_5_pwm,
                channels[5].neutral, channel_6_pwm,
                throttleChannel, rollChannel, pitchChannel
            );
            
            if (aux2Channel != -1 && aux2Channel != aux1Channel) {
                Serial.print(F("✓ AUX2 detected on CH")); Serial.println(aux2Channel + 1);
                channels[aux2Channel].physicalControl = "Aux2";
            } else {
                Serial.println(F("⚠️  No AUX2 detected"));
                aux2Channel = 5; // Default to CH6
            }
        } else {
            aux2Channel = 5; // Default to CH6
        }
    }
    
    // Print results
    printRadioCalibrationResults(throttleChannel, rollChannel, pitchChannel, yawChannel, aux1Channel, aux2Channel);
}

void printRadioCalibrationResults(int throttleChannel, int rollChannel, int pitchChannel, int yawChannel, int aux1Channel, int aux2Channel) {
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     DETECTED CHANNEL MAPPING                              ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));
    
    Serial.print(F("Throttle → CH")); Serial.println(throttleChannel + 1);
    Serial.print(F("Roll     → CH")); Serial.println(rollChannel + 1);
    Serial.print(F("Pitch    → CH")); Serial.println(pitchChannel + 1);
    Serial.print(F("Yaw      → CH")); Serial.println(yawChannel + 1);
    Serial.print(F("Aux1     → CH")); Serial.println(aux1Channel + 1);
    Serial.print(F("Aux2     → CH")); Serial.println(aux2Channel + 1);
    
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║     COPY-PASTE READY CODE                                 ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝\n"));
    
    Serial.println(F("📋 STEP 1: Open 'include/config.h'"));
    Serial.println(F("📋 STEP 2: Find '// Radio Channel Mapping'"));
    Serial.println(F("📋 STEP 3: REPLACE with these values:\n"));
    
    Serial.println(F("//============================================================================="));
    Serial.println(F("// Radio Channel Mapping (Auto-detected by calibration)"));
    Serial.println(F("//============================================================================="));
    Serial.print(F("#define THROTTLE_CHANNEL ")); Serial.println(throttleChannel + 1);
    Serial.print(F("#define ROLL_CHANNEL ")); Serial.println(rollChannel + 1);
    Serial.print(F("#define PITCH_CHANNEL ")); Serial.println(pitchChannel + 1);
    Serial.print(F("#define YAW_CHANNEL ")); Serial.println(yawChannel + 1);
    Serial.print(F("#define AUX1_CHANNEL ")); Serial.println(aux1Channel + 1);
    Serial.print(F("#define AUX2_CHANNEL ")); Serial.println(aux2Channel + 1);
    Serial.println();
    
    Serial.println(F("\n📋 STEP 4: Flash the live build: pio run -e teensy40 --target upload\n"));

    Serial.println(F("═══════════════════════════════════════════════════════════\n"));
}

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
    
    if (!waitForConfirmation(5)) return;
    
    // First run basic calibration
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 1: LEVEL CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Place aircraft on FLAT, LEVEL surface"));
    Serial.println(F("▶ Nose pointing FORWARD"));
    Serial.println(F("▶ Wings level (no roll)"));
    Serial.println(F("▶ Do NOT move during calibration (5 seconds)"));
    
    if (!waitForConfirmation(3)) return;
    
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
    
    if (!waitForConfirmation(3)) return;
    
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
    
    if (!waitForConfirmation(3)) return;
    
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
    
    if (!waitForConfirmation(3)) return;
    
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