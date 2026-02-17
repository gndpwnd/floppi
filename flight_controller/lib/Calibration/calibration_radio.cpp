/* Calibration Module - Radio Channel Calibration */

#include "calibration.h"
#include "config.h"
#include "radioComm.h"

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

    if (!waitForConfirmation()) return;

    // Storage for channel data
    struct ChannelData {
        uint16_t min, max, center, neutral;
    };
    ChannelData channels[6];

    // Initialize - read neutral positions
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 0: NEUTRAL POSITION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Center ALL sticks"));
    Serial.println(F("▶ Set ALL switches to their middle or OFF position"));

    if (!waitForConfirmation()) return;

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

    if (!waitForConfirmation()) return;

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
    Serial.println(F("\n▶ Now move throttle to MAXIMUM (full up)"));
    Serial.println(F("▶ Hold for 2 seconds..."));

    if (!waitForConfirmation()) return;

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
    if (!waitForConfirmation()) return;

    // STEP 2: ROLL STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 2: ROLL STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the ROLL stick (typically RIGHT stick LEFT/RIGHT)"));
    Serial.println(F("▶ Move to full LEFT"));

    if (!waitForConfirmation()) return;

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
    if (!waitForConfirmation()) return;

    delay(500);
    getCommands();
    uint16_t roll_right = getChannelValue(rollChannel);

    channels[rollChannel].min = min(roll_left, roll_right);
    channels[rollChannel].max = max(roll_left, roll_right);
    channels[rollChannel].center = channels[rollChannel].neutral;
    Serial.print(F("✓ Roll range: "));
    Serial.print(channels[rollChannel].min);
    Serial.print(F(" to "));
    Serial.println(channels[rollChannel].max);

    Serial.println(F("\n▶ Return roll to center"));
    if (!waitForConfirmation()) return;

    // STEP 3: PITCH STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 3: PITCH STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the PITCH stick (typically RIGHT stick UP/DOWN)"));
    Serial.println(F("▶ Move to full FORWARD (down)"));

    if (!waitForConfirmation()) return;

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
    if (!waitForConfirmation()) return;

    delay(500);
    getCommands();
    uint16_t pitch_back = getChannelValue(pitchChannel);

    channels[pitchChannel].min = min(pitch_fwd, pitch_back);
    channels[pitchChannel].max = max(pitch_fwd, pitch_back);
    channels[pitchChannel].center = channels[pitchChannel].neutral;
    Serial.print(F("✓ Pitch range: "));
    Serial.print(channels[pitchChannel].min);
    Serial.print(F(" to "));
    Serial.println(channels[pitchChannel].max);

    Serial.println(F("\n▶ Return pitch to center"));
    if (!waitForConfirmation()) return;

    // STEP 4: YAW STICK
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 4: YAW STICK CALIBRATION"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the YAW stick (typically LEFT stick LEFT/RIGHT)"));
    Serial.println(F("▶ Move to full LEFT"));

    if (!waitForConfirmation()) return;

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
    if (!waitForConfirmation()) return;

    delay(500);
    getCommands();
    uint16_t yaw_right = getChannelValue(yawChannel);

    channels[yawChannel].min = min(yaw_left, yaw_right);
    channels[yawChannel].max = max(yaw_left, yaw_right);
    channels[yawChannel].center = channels[yawChannel].neutral;
    Serial.print(F("✓ Yaw range: "));
    Serial.print(channels[yawChannel].min);
    Serial.print(F(" to "));
    Serial.println(channels[yawChannel].max);

    Serial.println(F("\n▶ Return yaw to center"));
    if (!waitForConfirmation()) return;

    // STEP 5: AUX1 (Typically throttle cut / arm switch)
    Serial.println(F("\n═══════════════════════════════════════════════════════════"));
    Serial.println(F("STEP 5: AUX1 SWITCH CALIBRATION (Throttle Cut)"));
    Serial.println(F("═══════════════════════════════════════════════════════════"));
    Serial.println(F("▶ Move ONLY the AUX1 switch (typically a 2-position switch)"));
    Serial.println(F("▶ Move to LOW position"));

    if (!waitForConfirmation()) return;

    delay(500);
    getCommands();
    int aux1Channel = detectMovedChannel(
        channels[0].neutral, channel_1_pwm,
        channels[1].neutral, channel_2_pwm,
        channels[2].neutral, channel_3_pwm,
        channels[3].neutral, channel_4_pwm,
        channels[4].neutral, channel_5_pwm,
        channels[5].neutral, channel_6_pwm,
        throttleChannel, rollChannel, pitchChannel, yawChannel
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
        if (!waitForConfirmation()) return;

        delay(500);
        getCommands();
        uint16_t aux1_high = getChannelValue(aux1Channel);

        channels[aux1Channel].min = min(aux1_low, aux1_high);
        channels[aux1Channel].max = max(aux1_low, aux1_high);
        channels[aux1Channel].center = (aux1_low + aux1_high) / 2;

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

        if (waitForConfirmation()) {
            delay(500);
            getCommands();
            aux2Channel = detectMovedChannel(
                channels[0].neutral, channel_1_pwm,
                channels[1].neutral, channel_2_pwm,
                channels[2].neutral, channel_3_pwm,
                channels[3].neutral, channel_4_pwm,
                channels[4].neutral, channel_5_pwm,
                channels[5].neutral, channel_6_pwm,
                throttleChannel, rollChannel, pitchChannel, yawChannel, aux1Channel
            );

            if (aux2Channel != -1) {
                Serial.print(F("✓ AUX2 detected on CH")); Serial.println(aux2Channel + 1);
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
    // Store results for dump command
    calResults.hasRadio = true;
    calResults.throttleChannel = throttleChannel;
    calResults.rollChannel = rollChannel;
    calResults.pitchChannel = pitchChannel;
    calResults.yawChannel = yawChannel;
    calResults.aux1Channel = aux1Channel;
    calResults.aux2Channel = aux2Channel;

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
