/*
 * Calibration Mode - Implementation
 * Serial command interface and calibration workflow for calibration builds
 *
 * Extracted from main.cpp to keep the main file focused on flight control.
 * All functions are compiled only when CALIBRATION_MODE is defined.
 */

#ifdef CALIBRATION_MODE

#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "pin_definitions.h"
#include "globals.h"
#include "calibration.h"
#include "calibration_mode.h"
#include "debug.h"
#include "radioComm.h"

#if defined(USE_ESP32) && defined(USE_WIFI)
#include <WiFi.h>
#include "wifi_config.h"
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#else
    #define WIFI_SSID     "unconfigured"
    #define WIFI_PASSWORD ""
#endif
#ifdef USE_WEB_SERVER
#include <ESPmDNS.h>
#endif
#endif

// Channel PWM values (defined in radioComm module)
extern unsigned long channel_1_pwm, channel_2_pwm, channel_3_pwm;
extern unsigned long channel_4_pwm, channel_5_pwm, channel_6_pwm;

// Forward declarations
#if defined(USE_ESP32) && defined(USE_WIFI)
static void runNetworkDiagnostics();
#endif

// Calibration status helpers — check compile-time markers from config.h
static void printCalibrationStatus() {
    Serial.println(F("\n=== CALIBRATION STATUS ==="));
    Serial.println(F("Stages marked [x] are calibrated (uncommented in config.h)."));
    Serial.println(F("Stages marked [ ] still need calibration.\n"));

    Serial.println(F("--- Stage 1: MCU + IMU ---"));
    #ifdef CALIBRATED_IMU
    Serial.println(F("  [x] IMU offsets (i/m)"));
    #else
    Serial.println(F("  [ ] IMU offsets (i/m) — place board flat, run 'i' or 'm'"));
    #endif

    #ifdef CALIBRATED_IMU_ORIENT
    Serial.println(F("  [x] IMU orientation (o)"));
    #else
    Serial.println(F("  [ ] IMU orientation (o) — only if non-standard mounting"));
    #endif

    #ifdef USE_MPU9250
    #ifdef CALIBRATED_MAG
    Serial.println(F("  [x] Magnetometer"));
    #else
    Serial.println(F("  [ ] Magnetometer — rotate in all orientations"));
    #endif
    #endif

    Serial.println(F("\n--- Stage 2: + Command Source ---"));
    #ifdef CALIBRATED_RADIO
    Serial.println(F("  [x] Radio channel mapping (r)"));
    #else
    Serial.println(F("  [ ] Radio channel mapping (r) — move sticks when prompted"));
    #endif

    #ifdef CALIBRATED_FAILSAFE
    Serial.println(F("  [x] Failsafe values (f)"));
    #else
    Serial.println(F("  [ ] Failsafe values (f) — power off transmitter when prompted"));
    #endif

    Serial.println(F("\n--- Stage 3: + ESCs/Motors (NO PROPS!) ---"));
    #ifdef CALIBRATED_ESC
    Serial.println(F("  [x] ESC endpoints (e)"));
    #else
    Serial.println(F("  [ ] ESC endpoints (e) — REMOVE PROPS, follow power cycle"));
    #endif

    Serial.println(F("\n--- Stage 4: Full Drone (tethered) ---"));
    #ifdef CALIBRATED_PID
    Serial.println(F("  [x] PID gains (g)"));
    #else
    Serial.println(F("  [ ] PID gains (g) — hover tethered, adjust via serial"));
    #endif

    #ifdef CALIBRATED_FILTERS
    Serial.println(F("  [x] Filters/limits (p)"));
    #else
    Serial.println(F("  [ ] Filters/limits (p) — tune if defaults don't work"));
    #endif

    Serial.println();
}

// Sequential calibration workflow — guides user through all stages
static void calibrateSequential() {
    Serial.println(F("\n========================================"));
    Serial.println(F("  SEQUENTIAL CALIBRATION WORKFLOW"));
    Serial.println(F("========================================"));
    Serial.println(F("This guides you through all calibration stages."));
    Serial.println(F("Skip any stage by pressing 'n' when prompted."));
    Serial.println(F("After each stage, copy values to config.h and"));
    Serial.println(F("uncomment the matching CALIBRATED_* marker.\n"));

    printCalibrationStatus();

    // --- Stage 1: IMU ---
    Serial.println(F("========================================"));
    Serial.println(F("  STAGE 1: IMU Calibration"));
    Serial.println(F("========================================"));
    Serial.println(F("Hardware needed: MCU + IMU only"));

    #ifdef CALIBRATED_IMU
    Serial.println(F("\n[DONE] IMU offsets already calibrated. Skipping."));
    #else
    Serial.println(F("\nStep 1a: IMU Offset Calibration"));
    Serial.println(F("Run 6-position calibration for best accuracy? (y/n)"));
    if (waitForConfirmation()) {
        Serial.println(F("\n--- Running 6-Position IMU Calibration ---"));
        calibrateIMU6Position();
    } else {
        Serial.println(F("\nRun single-position instead? (y/n)"));
        if (waitForConfirmation()) {
            Serial.println(F("\n--- Running Single-Position IMU Calibration ---"));
            calibrateIMU();
        } else {
            Serial.println(F("Skipping IMU calibration."));
        }
    }
    #endif

    #ifndef CALIBRATED_IMU_ORIENT
    Serial.println(F("\nStep 1b: IMU Orientation Detection"));
    Serial.println(F("Only needed if IMU is NOT flat with chip up, X forward."));
    Serial.println(F("Run orientation detection? (y/n)"));
    if (waitForConfirmation()) {
        Serial.println(F("\n--- Running IMU + Orientation Detection ---"));
        calibrateIMUWithOrientation();
    } else {
        Serial.println(F("Skipping orientation detection (standard mounting assumed)."));
    }
    #else
    Serial.println(F("\n[DONE] IMU orientation already calibrated. Skipping."));
    #endif

    #ifdef USE_MPU9250
    #ifndef CALIBRATED_MAG
    Serial.println(F("\nStep 1c: Magnetometer Calibration (MPU9250)"));
    Serial.println(F("Run magnetometer calibration? (y/n)"));
    if (waitForConfirmation()) {
        Serial.println(F("\n--- Running Magnetometer Calibration ---"));
        calibrateMagnetometer();
    } else {
        Serial.println(F("Skipping magnetometer calibration."));
    }
    #else
    Serial.println(F("\n[DONE] Magnetometer already calibrated. Skipping."));
    #endif
    #endif

    // --- ESP32: Network Diagnostics ---
    #if defined(USE_ESP32) && defined(USE_WIFI)
    Serial.println(F("\n========================================"));
    Serial.println(F("  ESP32: Network Diagnostics"));
    Serial.println(F("========================================"));
    Serial.println(F("Run WiFi/network diagnostics? (y/n)"));
    if (waitForConfirmation()) {
        runNetworkDiagnostics();
    } else {
        Serial.println(F("Skipping network diagnostics."));
    }
    #endif

    // --- Stage 2: Command Source ---
    Serial.println(F("\n========================================"));
    Serial.println(F("  STAGE 2: Command Source"));
    Serial.println(F("========================================"));
    Serial.println(F("Hardware needed: MCU + IMU + receiver/transmitter"));
    Serial.println(F("(Skip if using serial/I2C/WiFi commands)"));

    #ifdef CALIBRATED_RADIO
    Serial.println(F("\n[DONE] Radio channels already calibrated. Skipping."));
    #else
    Serial.println(F("\nStep 2a: Radio Channel Mapping"));
    Serial.println(F("Is your receiver + transmitter connected and powered? (y/n)"));
    if (waitForConfirmation()) {
        Serial.println(F("\n--- Running Radio Calibration ---"));
        calibrateRadio();
    } else {
        Serial.println(F("Skipping radio calibration."));
    }
    #endif

    #ifdef CALIBRATED_FAILSAFE
    Serial.println(F("\n[DONE] Failsafe values already calibrated. Skipping."));
    #else
    Serial.println(F("\nStep 2b: Failsafe Auto-Detection"));
    Serial.println(F("Is your receiver connected? (y/n)"));
    if (waitForConfirmation()) {
        Serial.println(F("\n--- Running Failsafe Detection ---"));
        calibrateFailsafe();
    } else {
        Serial.println(F("Skipping failsafe detection."));
    }
    #endif

    // --- Stage 3: ESCs ---
    Serial.println(F("\n========================================"));
    Serial.println(F("  STAGE 3: ESC Calibration"));
    Serial.println(F("========================================"));
    Serial.println(F("Hardware needed: MCU + IMU + receiver + ESCs + motors"));
    Serial.println(F("*** REMOVE ALL PROPELLERS! ***"));

    #ifdef CALIBRATED_ESC
    Serial.println(F("\n[DONE] ESC endpoints already calibrated. Skipping."));
    #else
    Serial.println(F("\nStep 3a: ESC Endpoint Calibration"));
    Serial.println(F("Are ESCs connected and propellers REMOVED? (y/n)"));
    if (waitForConfirmation()) {
        Serial.println(F("\n--- Running ESC Calibration ---"));
        calibrateESC();
    } else {
        Serial.println(F("Skipping ESC calibration."));
    }
    #endif

    // --- Stage 4: PID/Filters ---
    Serial.println(F("\n========================================"));
    Serial.println(F("  STAGE 4: PID & Filter Tuning"));
    Serial.println(F("========================================"));
    Serial.println(F("Hardware needed: Full drone with props, TETHERED"));
    Serial.println(F("PID tuning requires flying — use 'g' and 'p' commands"));
    Serial.println(F("individually during tethered hover."));

    #ifdef CALIBRATED_PID
    Serial.println(F("[DONE] PID gains already tuned."));
    #else
    Serial.println(F("[ ] PID gains — use 'g' command during tethered flight"));
    #endif

    #ifdef CALIBRATED_FILTERS
    Serial.println(F("[DONE] Filters/limits already tuned."));
    #else
    Serial.println(F("[ ] Filters/limits — use 'p' command if defaults aren't good"));
    #endif

    // --- Summary ---
    Serial.println(F("\n========================================"));
    Serial.println(F("  CALIBRATION WORKFLOW COMPLETE"));
    Serial.println(F("========================================"));
    Serial.println(F("Next steps:"));
    Serial.println(F("  1. Type 'd' to dump ALL calibration values for config.h"));
    Serial.println(F("  2. Copy the output block into config.h"));
    Serial.println(F("  3. Uncomment CALIBRATED_* markers for completed stages"));
    Serial.println(F("  4. Flash calibration build to verify"));
    Serial.println(F("  5. When all stages done, flash LIVE build and fly!"));
    Serial.println(F("\nType 'd' to dump all calibration values for config.h"));
    Serial.println(F("Re-run any individual calibration with its command letter."));
    Serial.println(F("Run 'a' again to see status and continue where you left off.\n"));
}

void runCalibrationIfRequested() {
    if (calibration_mode != CALIB_NONE && !calibration_in_progress) {
        calibration_in_progress = true;
        calibration_start_time = millis();

        Serial.println(F("\n=== CALIBRATION MODE ACTIVATED ==="));

        switch (calibration_mode) {
            case CALIB_ACCEL_GYRO:
                Serial.println(F("Running IMU Calibration (single-position, offsets only)"));
                calibrateIMU();
                break;

            case CALIB_6POSITION:
                Serial.println(F("Running 6-Position IMU Calibration (offsets + scale)"));
                calibrateIMU6Position();
                break;

            case CALIB_ATTITUDE:
                Serial.println(F("Running IMU Calibration + Orientation Detection"));
                calibrateIMUWithOrientation();
                break;

            case CALIB_RADIO:
                Serial.println(F("Running Radio Calibration"));
                calibrateRadio();
                break;

            case CALIB_FAILSAFE:
                Serial.println(F("Running Failsafe Auto-Detection"));
                calibrateFailsafe();
                break;

            case CALIB_ESC:
                Serial.println(F("Running ESC Endpoint Calibration"));
                calibrateESC();
                break;

            #ifdef USE_MPU9250
            case CALIB_MAG:
                Serial.println(F("Running Magnetometer Calibration"));
                calibrateMagnetometer();
                break;
            #endif

            case CALIB_SEQUENTIAL:
                calibrateSequential();
                break;

            default:
                break;
        }

        calibration_in_progress = false;
        calibration_mode = CALIB_NONE;

        Serial.println(F("=== CALIBRATION COMPLETE ==="));
        Serial.println(F("Returning to normal mode...\n"));

        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }
}

void checkCalibrationMode() {
    static CalibrationMode last_mode = CALIB_NONE;
    static unsigned long mode_start_time = 0;
    static bool triggered_this_hold = false;  // Debounce: must go LOW before re-trigger

    if (armedFly || channel_3_pwm > 1050) {
        calibration_mode = CALIB_NONE;
        return;
    }

    CalibrationMode new_mode = CALIB_NONE;

    if (channel_6_pwm < 1200) {
        new_mode = CALIB_NONE;
    } else if (channel_6_pwm >= 1200 && channel_6_pwm <= 1800) {
        new_mode = CALIB_ACCEL_GYRO;
    } else if (channel_6_pwm > 1800) {
        new_mode = CALIB_ATTITUDE;
    }

    if (new_mode != last_mode) {
        last_mode = new_mode;
        mode_start_time = millis();
        // Reset debounce when switch position changes
        if (new_mode == CALIB_NONE) {
            triggered_this_hold = false;
        }
        return;
    }

    if (new_mode != CALIB_NONE && !triggered_this_hold) {
        unsigned long hold_time = millis() - mode_start_time;

        if (hold_time >= 3000) {
            calibration_mode = new_mode;
            triggered_this_hold = true;  // Don't re-trigger until CH6 goes LOW

            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
        }
    }
}

// Helper: match a gain name and return pointer to it, or NULL
static float* findGain(const char* name) {
    if (strcmp(name, "kp_roll") == 0)  return &tune_kp_roll;
    if (strcmp(name, "ki_roll") == 0)  return &tune_ki_roll;
    if (strcmp(name, "kd_roll") == 0)  return &tune_kd_roll;
    if (strcmp(name, "kp_pitch") == 0) return &tune_kp_pitch;
    if (strcmp(name, "ki_pitch") == 0) return &tune_ki_pitch;
    if (strcmp(name, "kd_pitch") == 0) return &tune_kd_pitch;
    if (strcmp(name, "kp_yaw") == 0)   return &tune_kp_yaw;
    if (strcmp(name, "ki_yaw") == 0)   return &tune_ki_yaw;
    if (strcmp(name, "kd_yaw") == 0)   return &tune_kd_yaw;
    return NULL;
}

static void printGains() {
    Serial.println(F("\n=== PID GAINS ==="));
    #ifdef USE_RATE_CONTROLLER
    Serial.println(F("Mode: RATE"));
    #else
    Serial.println(F("Mode: ANGLE"));
    #endif
    Serial.print(F("  kp_roll="));  Serial.print(tune_kp_roll, 6);
    Serial.print(F("  ki_roll="));  Serial.print(tune_ki_roll, 6);
    Serial.print(F("  kd_roll="));  Serial.println(tune_kd_roll, 6);
    Serial.print(F("  kp_pitch=")); Serial.print(tune_kp_pitch, 6);
    Serial.print(F("  ki_pitch=")); Serial.print(tune_ki_pitch, 6);
    Serial.print(F("  kd_pitch=")); Serial.println(tune_kd_pitch, 6);
    Serial.print(F("  kp_yaw="));   Serial.print(tune_kp_yaw, 6);
    Serial.print(F("  ki_yaw="));   Serial.print(tune_ki_yaw, 6);
    Serial.print(F("  kd_yaw="));   Serial.println(tune_kd_yaw, 6);
    Serial.println(F("Set: g <name> <value>  (e.g. g kp_roll 0.2)"));
}

// Helper: match a parameter name and return pointer to it, or NULL
static float* findParam(const char* name) {
    if (strcmp(name, "b_accel") == 0)  return &tune_b_accel;
    if (strcmp(name, "b_gyro") == 0)   return &tune_b_gyro;
    if (strcmp(name, "b_dterm") == 0)   return &tune_b_dterm;
    if (strcmp(name, "beta") == 0)      return &tune_madgwick_beta;
    if (strcmp(name, "max_roll") == 0)  return &tune_max_roll;
    if (strcmp(name, "max_pitch") == 0) return &tune_max_pitch;
    if (strcmp(name, "max_yaw") == 0)   return &tune_max_yaw;
    return NULL;
}

static void printParams() {
    Serial.println(F("\n=== FILTER & LIMITS ==="));
    Serial.print(F("  b_accel="));    Serial.println(tune_b_accel, 4);
    Serial.print(F("  b_gyro="));     Serial.println(tune_b_gyro, 4);
    Serial.print(F("  b_dterm="));    Serial.println(tune_b_dterm, 4);
    Serial.print(F("  beta="));       Serial.println(tune_madgwick_beta, 4);
    #ifdef USE_RATE_CONTROLLER
    Serial.println(F("Mode: RATE (deg/s)"));
    #else
    Serial.println(F("Mode: ANGLE (deg)"));
    #endif
    Serial.print(F("  max_roll="));   Serial.println(tune_max_roll, 1);
    Serial.print(F("  max_pitch="));  Serial.println(tune_max_pitch, 1);
    Serial.print(F("  max_yaw="));    Serial.println(tune_max_yaw, 1);
    Serial.println(F("Set: p <name> <value>  (e.g. p b_accel 0.10)"));
}

// ============================================================================
// ESP32 Network Diagnostics (command 'n')
// ============================================================================

#if defined(USE_ESP32) && defined(USE_WIFI)
static void runNetworkDiagnostics() {
    Serial.println(F("\n========================================"));
    Serial.println(F("  NETWORK DIAGNOSTICS (ESP32)"));
    Serial.println(F("========================================"));

    int pass = 0;
    int fail = 0;
    int skip = 0;

    // --- Test 1: WiFi credentials configured ---
    Serial.print(F("\n[1] WiFi credentials configured... "));
    const char* ssid = WIFI_SSID;
    if (strcmp(ssid, "unconfigured") == 0 || strcmp(ssid, "YourNetworkName") == 0) {
        Serial.println(F("FAIL"));
        Serial.println(F("    Edit include/wifi_credentials.h with your network details"));
        fail++;
        Serial.println(F("\n>>> Cannot continue without WiFi credentials."));
        Serial.printf("    Results: %d PASS, %d FAIL, %d SKIP\n\n", pass, fail, skip);
        return;
    }
    Serial.println(F("PASS"));
    Serial.printf("    SSID: %s\n", ssid);
    pass++;

    // --- Test 2: WiFi connection ---
    Serial.print(F("[2] WiFi connected... "));
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("PASS"));
        Serial.print(F("    IP: "));
        Serial.println(WiFi.localIP());
        pass++;
    } else {
        Serial.println(F("FAIL"));
        Serial.printf("    Status: %d (expected WL_CONNECTED=%d)\n", WiFi.status(), WL_CONNECTED);
        Serial.println(F("    Check: SSID/password correct? Router reachable? Antenna?"));
        fail++;
        Serial.println(F("\n>>> Cannot continue without WiFi connection."));
        Serial.printf("    Results: %d PASS, %d FAIL, %d SKIP\n\n", pass, fail, skip);
        return;
    }

    // --- Test 3: Signal strength (RSSI) ---
    Serial.print(F("[3] Signal strength (RSSI)... "));
    int8_t rssi = WiFi.RSSI();
    if (rssi > -50) {
        Serial.printf("EXCELLENT (%d dBm)\n", rssi);
    } else if (rssi > -70) {
        Serial.printf("GOOD (%d dBm)\n", rssi);
    } else if (rssi > -80) {
        Serial.printf("WEAK (%d dBm) — move closer to router\n", rssi);
    } else {
        Serial.printf("POOR (%d dBm) — signal too weak for reliable operation\n", rssi);
    }
    pass++; // RSSI is always informational

    // --- Test 4: MAC address ---
    Serial.print(F("[4] MAC address... "));
    Serial.println(WiFi.macAddress());
    pass++;

    // --- Test 5: Gateway reachable ---
    Serial.print(F("[5] Gateway... "));
    IPAddress gw = WiFi.gatewayIP();
    Serial.printf("%d.%d.%d.%d\n", gw[0], gw[1], gw[2], gw[3]);
    pass++;

    // --- Test 6: DNS ---
    Serial.print(F("[6] DNS server... "));
    IPAddress dns = WiFi.dnsIP();
    Serial.printf("%d.%d.%d.%d\n", dns[0], dns[1], dns[2], dns[3]);
    pass++;

    // --- Test 7: mDNS hostname ---
    #ifdef USE_WEB_SERVER
    Serial.print(F("[7] mDNS hostname... "));
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char expected_hostname[20];
    snprintf(expected_hostname, sizeof(expected_hostname), "floppi-%02X%02X", mac[4], mac[5]);
    Serial.printf("%s.local\n", expected_hostname);
    Serial.printf("    URL: http://%s.local\n", expected_hostname);
    pass++;

    // --- Test 8: Web server port 80 ---
    Serial.print(F("[8] Web server (port 80)... "));
    WiFiClient testClient;
    IPAddress localIP = WiFi.localIP();
    if (testClient.connect(localIP, 80)) {
        testClient.stop();
        Serial.println(F("PASS (listening)"));
        pass++;
    } else {
        Serial.println(F("FAIL (not responding)"));
        Serial.println(F("    Web server may not have started yet"));
        fail++;
    }
    #else
    Serial.println(F("[7] mDNS/Web server... SKIP (USE_WEB_SERVER not enabled)"));
    skip += 2;
    #endif

    // --- Test 9: Free heap ---
    Serial.print(F("[9] Free heap memory... "));
    uint32_t heap = ESP.getFreeHeap();
    Serial.printf("%u bytes", heap);
    if (heap < 30000) {
        Serial.println(F(" — WARNING: low memory"));
    } else {
        Serial.println(F(" — OK"));
    }
    pass++;

    // --- Test 10: Uptime ---
    Serial.print(F("[10] Uptime... "));
    unsigned long up = millis();
    Serial.printf("%lu.%lus\n", up / 1000, (up % 1000) / 100);
    pass++;

    // --- Summary ---
    Serial.println(F("\n----------------------------------------"));
    Serial.printf("Results: %d PASS, %d FAIL, %d SKIP\n", pass, fail, skip);

    if (fail == 0) {
        Serial.println(F("Network is ready for swarm_api / WiFi commands."));
        #ifdef USE_WEB_SERVER
        Serial.printf("Dashboard: http://%s.local\n", expected_hostname);
        Serial.printf("API: http://%s.local/api/status\n", expected_hostname);
        Serial.printf("WebSocket: ws://%s.local/ws\n", expected_hostname);
        #endif
    } else {
        Serial.println(F("Fix failures above before using WiFi features."));
    }
    Serial.println();
}
#endif // USE_ESP32 && USE_WIFI

static void processSerialLine(char* line) {
    // Trim leading whitespace
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    // Single-character commands (backward compatible)
    if (line[1] == '\0' || line[1] == '\r') {
        char cmd = line[0];
        switch (cmd) {
            case 'r': case 'R':
                Serial.println(F("\n>>> Radio Calibration requested via serial"));
                calibration_mode = CALIB_RADIO;
                return;
            case 'i': case 'I':
                Serial.println(F("\n>>> IMU Calibration requested via serial"));
                calibration_mode = CALIB_ACCEL_GYRO;
                return;
            case 'o': case 'O':
                Serial.println(F("\n>>> IMU + Orientation Calibration requested via serial"));
                calibration_mode = CALIB_ATTITUDE;
                return;
            case 'm': case 'M':
                Serial.println(F("\n>>> 6-Position IMU Calibration requested via serial"));
                calibration_mode = CALIB_6POSITION;
                return;
            case 's': case 'S':
                Serial.println(F("\n=== STATUS ==="));
                Serial.print(F("CH1: ")); Serial.print(channel_1_pwm);
                Serial.print(F("  CH2: ")); Serial.print(channel_2_pwm);
                Serial.print(F("  CH3: ")); Serial.print(channel_3_pwm);
                Serial.print(F("  CH4: ")); Serial.print(channel_4_pwm);
                Serial.print(F("  CH5: ")); Serial.print(channel_5_pwm);
                Serial.print(F("  CH6: ")); Serial.println(channel_6_pwm);
                Serial.print(F("Armed: ")); Serial.println(armedFly ? "YES" : "NO");
                return;
            case 't': case 'T':
                telemetry_mode = (telemetry_mode + 1) % 4;
                if (telemetry_mode == 0) {
                    Serial.println(F("\n>>> Telemetry OFF"));
                } else if (telemetry_mode == 1) {
                    Serial.println(F("\n>>> Telemetry: IMU (50Hz)"));
                    Serial.println(F("    Accel->Plot0 Gyro->Plot1 (fc_tool multi-graph)"));
                } else if (telemetry_mode == 2) {
                    Serial.println(F("\n>>> Telemetry: FULL (20Hz)"));
                    Serial.println(F("    Accel->Plot0 Gyro->Plot1 Attitude->Plot2 Motors->Plot3"));
                } else {
                    Serial.println(F("\n>>> Telemetry: QUATERNION (50Hz)"));
                    Serial.println(F("    Quat->Plot4 Gyro->Plot1 (gimbal-lock-free for flight computer)"));
                    printGyroSaturationCheck();
                }
                return;
            case 'f': case 'F':
                Serial.println(F("\n>>> Failsafe Auto-Detection requested via serial"));
                calibration_mode = CALIB_FAILSAFE;
                return;
            case 'e': case 'E':
                Serial.println(F("\n>>> ESC Calibration requested via serial"));
                calibration_mode = CALIB_ESC;
                return;
            case 'n': case 'N':
                #if defined(USE_ESP32) && defined(USE_WIFI)
                runNetworkDiagnostics();
                #else
                Serial.println(F("\n>>> Network diagnostics only available on ESP32 with WiFi"));
                #endif
                return;
            case 'a': case 'A':
                Serial.println(F("\n>>> Sequential Calibration requested via serial"));
                calibration_mode = CALIB_SEQUENTIAL;
                return;
            case 'c': case 'C':
                printCalibrationStatus();
                return;
            case 'd': case 'D':
                Serial.println(F("\n>>> Dumping all calibration values"));
                printAllCalibrationValues();
                return;
            case 'g': case 'G':
                printGains();
                return;
            case 'p': case 'P':
                printParams();
                return;
            case 'h': case 'H': case '?':
                Serial.println(F("\n=== CALIBRATION COMMANDS ==="));
                Serial.println(F("  a - Sequential calibration (guided workflow)"));
                Serial.println(F("  c - Calibration status (what's done/pending)"));
                Serial.println(F("  d - Dump ALL calibration values (config.h ready block)"));
                Serial.println(F("  r - Radio calibration (channel mapping)"));
                Serial.println(F("  i - IMU calibration (single-position, offsets only)"));
                Serial.println(F("  m - IMU calibration (6-position, offsets + scale)"));
                Serial.println(F("  o - IMU + Orientation detection"));
                Serial.println(F("  f - Failsafe auto-detection (measures receiver failsafe)"));
                Serial.println(F("  e - ESC endpoint calibration (min/max PWM)"));
                Serial.println(F("  s - Status (show channel values)"));
                Serial.println(F("  t - Toggle telemetry (off/IMU/full/quat) for fc_tool"));
                Serial.println(F("  g - Show/set PID gains (g <name> <value>)"));
                Serial.println(F("  p - Show/set filter & limits (p <name> <value>)"));
                Serial.println(F("  n - Network diagnostics (ESP32 only — WiFi, mDNS, web server)"));
                Serial.println(F("  h - Help (this menu)"));
                Serial.println(F("\nOr use CH6 switch:"));
                Serial.println(F("  Mid position (3s hold): IMU calibration"));
                Serial.println(F("  High position (3s hold): IMU + Orientation"));
                return;
            default:
                return;
        }
    }

    // Multi-word commands: "g <name> <value>"
    if (line[0] == 'g' && line[1] == ' ') {
        char* name = line + 2;
        while (*name == ' ') name++;
        char* space = strchr(name, ' ');
        if (space == NULL) {
            // Just "g <name>" — print that one gain
            float* gain = findGain(name);
            if (gain) {
                Serial.print(F("  "));
                Serial.print(name);
                Serial.print(F(" = "));
                Serial.println(*gain, 6);
            } else {
                Serial.print(F("Unknown gain: "));
                Serial.println(name);
            }
            return;
        }
        *space = '\0';
        char* val_str = space + 1;
        while (*val_str == ' ') val_str++;

        float* gain = findGain(name);
        if (!gain) {
            Serial.print(F("Unknown gain: "));
            Serial.println(name);
            return;
        }
        float val = atof(val_str);
        *gain = val;
        Serial.print(F("  "));
        Serial.print(name);
        Serial.print(F(" = "));
        Serial.println(val, 6);
        return;
    }

    // Multi-word commands: "p <name> <value>"
    if (line[0] == 'p' && line[1] == ' ') {
        char* name = line + 2;
        while (*name == ' ') name++;
        char* space = strchr(name, ' ');
        if (space == NULL) {
            float* param = findParam(name);
            if (param) {
                Serial.print(F("  "));
                Serial.print(name);
                Serial.print(F(" = "));
                Serial.println(*param, 4);
            } else {
                Serial.print(F("Unknown param: "));
                Serial.println(name);
            }
            return;
        }
        *space = '\0';
        char* val_str = space + 1;
        while (*val_str == ' ') val_str++;

        float* param = findParam(name);
        if (!param) {
            Serial.print(F("Unknown param: "));
            Serial.println(name);
            return;
        }
        float val = atof(val_str);
        *param = val;
        Serial.print(F("  "));
        Serial.print(name);
        Serial.print(F(" = "));
        Serial.println(val, 4);
        return;
    }
}

void checkSerialCommands() {
    if (calibration_in_progress) return;

    // Non-blocking line buffer
    static char buf[64];
    static uint8_t pos = 0;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                buf[pos] = '\0';
                processSerialLine(buf);
                pos = 0;
            }
        } else if (pos < sizeof(buf) - 1) {
            buf[pos++] = c;
        }
    }
}

#endif // CALIBRATION_MODE
