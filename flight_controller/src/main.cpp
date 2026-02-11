/*
 * dRehmFlight VTOL - Flight Controller Main
 * PlatformIO Port with Modular Architecture
 *
 * Original by Nicholas Rehm (dRehmFlight)
 * PlatformIO port with modular refactoring
 *
 * Architecture:
 *   Teensy: Single-core. Flight control + display in main loop.
 *   ESP32:  Dual-core. Core 0 = flight control, Core 1 = display + WiFi.
 */

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config.h"
#include "pin_definitions.h"
#include "globals.h"
#include "radioComm.h"
#include "imu.h"
#include "control.h"
#include "motors.h"

#ifdef CALIBRATION_MODE
#include "calibration.h"
#include "debug.h"
#endif

#ifdef USE_OLED_DISPLAY
#include "display.h"
#endif

#if defined(USE_ESP32) && defined(USE_WIFI)
#include "wifi_config.h"
#endif
#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
#include "web_server.h"
#endif
#if defined(USE_ESP32) && defined(USE_API_SERVER)
#include "api_client.h"
#endif
#if defined(USE_ESP32) && defined(USE_OTA)
#include "ota.h"
#endif

// ESP32 FreeRTOS for dual-core support
#ifdef USE_ESP32
#include "display_data.h"
static TaskHandle_t flightControlTaskHandle = NULL;
static QueueHandle_t displayQueue = NULL;
#endif

//========================================================================================================================//
//                                                 GLOBAL VARIABLES                                                        //
//========================================================================================================================//
// These are the actual definitions (not extern) - they're declared extern in globals.h

// Timing
unsigned long current_time, prev_time;
unsigned long print_counter;
float dt;

// IMU raw data
int16_t AcX, AcY, AcZ, GyX, GyY, GyZ, MgX, MgY, MgZ;
float AccX, AccY, AccZ;
float AccX_prev, AccY_prev, AccZ_prev;
float GyroX, GyroY, GyroZ;
float GyroX_prev, GyroY_prev, GyroZ_prev;
float MagX, MagY, MagZ;
float MagX_prev, MagY_prev, MagZ_prev;

// IMU calibration
float AccErrorX = IMU_ACC_ERROR_X;
float AccErrorY = IMU_ACC_ERROR_Y;
float AccErrorZ = IMU_ACC_ERROR_Z;
float GyroErrorX = IMU_GYRO_ERROR_X;
float GyroErrorY = IMU_GYRO_ERROR_Y;
float GyroErrorZ = IMU_GYRO_ERROR_Z;

// Attitude (Madgwick quaternion)
float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
float roll_IMU, pitch_IMU, yaw_IMU;
float roll_IMU_prev, pitch_IMU_prev;

// Desired states from radio
float thro_des, roll_des, pitch_des, yaw_des;
float roll_passthru, pitch_passthru, yaw_passthru;

// PID variables
float error_roll, error_roll_prev, integral_roll, integral_roll_prev, derivative_roll, roll_PID = 0;
float error_pitch, error_pitch_prev, integral_pitch, integral_pitch_prev, derivative_pitch, pitch_PID = 0;
float error_yaw, error_yaw_prev, integral_yaw, integral_yaw_prev, derivative_yaw, yaw_PID = 0;

// Motor commands
int m1_command_PWM, m2_command_PWM, m3_command_PWM, m4_command_PWM, m5_command_PWM, m6_command_PWM;
float m1_command_scaled, m2_command_scaled, m3_command_scaled, m4_command_scaled, m5_command_scaled, m6_command_scaled;

// Servo objects (Teensy only - ESP32 uses LEDC directly)
#ifndef USE_ESP32
PWMServo servo1, servo2, servo3, servo4, servo5, servo6, servo7;
#endif

// Servo commands
int s1_command_PWM, s2_command_PWM, s3_command_PWM, s4_command_PWM, s5_command_PWM, s6_command_PWM, s7_command_PWM;
float s1_command_scaled, s2_command_scaled, s3_command_scaled, s4_command_scaled, s5_command_scaled, s6_command_scaled, s7_command_scaled;

// Arming state
bool armedFly = false;

// Calibration program state (only in calibration builds)
#ifdef CALIBRATION_MODE
CalibrationMode calibration_mode = CALIB_NONE;
bool calibration_in_progress = false;
unsigned long calibration_start_time = 0;
int telemetry_mode = 0;  // 0=off, 1=IMU, 2=full

// Runtime-tunable PID gains (initialized from config.h macros)
#ifdef USE_RATE_CONTROLLER
float tune_kp_roll = KP_ROLL_RATE;
float tune_ki_roll = KI_ROLL_RATE;
float tune_kd_roll = KD_ROLL_RATE;
float tune_kp_pitch = KP_PITCH_RATE;
float tune_ki_pitch = KI_PITCH_RATE;
float tune_kd_pitch = KD_PITCH_RATE;
#else
float tune_kp_roll = KP_ROLL_ANGLE;
float tune_ki_roll = KI_ROLL_ANGLE;
float tune_kd_roll = KD_ROLL_ANGLE;
float tune_kp_pitch = KP_PITCH_ANGLE;
float tune_ki_pitch = KI_PITCH_ANGLE;
float tune_kd_pitch = KD_PITCH_ANGLE;
#endif
float tune_kp_yaw = KP_YAW_RATE;
float tune_ki_yaw = KI_YAW_RATE;
float tune_kd_yaw = KD_YAW_RATE;

// Runtime-tunable filter coefficients and control limits
float tune_b_accel = B_ACCEL;
float tune_b_gyro = B_GYRO;
float tune_b_dterm = B_DTERM;
float tune_madgwick_beta = MADGWICK_BETA;
#ifdef USE_RATE_CONTROLLER
float tune_max_roll = MAX_ROLL_RATE;
float tune_max_pitch = MAX_PITCH_RATE;
#else
float tune_max_roll = MAX_ROLL_ANGLE;
float tune_max_pitch = MAX_PITCH_ANGLE;
#endif
float tune_max_yaw = MAX_YAW_RATE;
#endif

//========================================================================================================================//
//                                              FUNCTION DECLARATIONS                                                      //
//========================================================================================================================//

void setupBlink(int num_blinks, int blink_delay);
void flightControlTick();
void updateStatusLED();

#ifdef CALIBRATION_MODE
void checkCalibrationMode();
void checkSerialCommands();
void runCalibrationIfRequested();
#endif

//========================================================================================================================//
//                                           FLIGHT CONTROL TICK                                                           //
//========================================================================================================================//
// One iteration of the flight control loop.
// Called from loop() on Teensy, or from Core 0 task on ESP32.

void flightControlTick() {
    // Keep track of time
    prev_time = current_time;
    current_time = micros();
    dt = (current_time - prev_time) / 1000000.0;

    #ifdef CALIBRATION_MODE
    checkCalibrationMode();
    checkSerialCommands();
    #endif

    // Core flight controller (skip if calibration in progress)
    #ifdef CALIBRATION_MODE
    if (!calibration_in_progress) {
    #endif
        getCommands();
        failSafe();
        getIMUdata();

        // Attitude estimation with Madgwick filter
        #ifdef USE_MPU6050
            Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt);
        #elif defined(USE_MPU9250)
            Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
        #endif

        getDesState();
        armedStatus();

        if (armedFly) {
            #ifdef USE_RATE_CONTROLLER
                controlRATE();
            #elif defined(USE_ANGLE_CONTROLLER)
                controlANGLE();
            #endif
            controlMixer();
        } else {
            roll_PID = 0;
            pitch_PID = 0;
            yaw_PID = 0;
            m1_command_scaled = 0;
            m2_command_scaled = 0;
            m3_command_scaled = 0;
            m4_command_scaled = 0;
            m5_command_scaled = 0;
            m6_command_scaled = 0;
        }

        scaleCommands();
        throttleCut();
        commandMotors();

    #ifdef CALIBRATION_MODE
    }

    runCalibrationIfRequested();

    // fc_tool telemetry output (toggle with 't' command)
    if (telemetry_mode == 1) {
        printIMUTelemetry();
    } else if (telemetry_mode == 2) {
        printFullTelemetry();
    }
    #endif

    updateStatusLED();
    loopRate(LOOP_FREQUENCY_HZ);
}

//========================================================================================================================//
//                                           ESP32 DUAL-CORE SUPPORT                                                      //
//========================================================================================================================//

#ifdef USE_ESP32

// Core 0: Flight control task (high priority, real-time)
void flightControlTask(void* parameter) {
    for (;;) {
        flightControlTick();

        // Push telemetry data to Core 1 via queue (non-blocking)
        // Used by: display, web server, API client
        if (displayQueue != NULL) {
            DisplayData_t data;
            populateDisplayData(&data);
            xQueueOverwrite(displayQueue, &data);
        }
    }
}

#endif // USE_ESP32

//========================================================================================================================//
//                                                      SETUP                                                              //
//========================================================================================================================//

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println(F("\n\n========================================"));
    Serial.println(F("  dRehmFlight VTOL Flight Controller"));
    #ifdef USE_ESP32
    Serial.println(F("  ESP32 Dual-Core Architecture"));
    #else
    Serial.println(F("  Modular Architecture"));
    #endif
    Serial.println(F("========================================\n"));

    // LED setup
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // Setup OLED display (early, for startup feedback)
    #ifdef USE_OLED_DISPLAY
    setupDisplay();
    displayStartupMessage("IMU init...");
    #endif

    // Setup IMU
    Serial.print(F("IMU: "));
    setupIMU();

    #ifdef USE_OLED_DISPLAY
    displayStartupMessage("Radio init...");
    #endif

    // Setup radio
    Serial.print(F("Radio: "));
    radioSetup();

    // Get initial radio commands
    getCommands();

    #ifdef CALIBRATION_MODE
    if (channel_6_pwm > 1800) {
        Serial.println(F("\nManual calibration triggered on startup!"));
        Serial.println(F("Keep board FLAT and STILL!"));
        delay(2000);
        calibrateIMU();
    }
    #endif

    #ifdef USE_OLED_DISPLAY
    displayStartupMessage("Motors init...");
    #endif

    // Setup motors and servos
    setupMotors();
    armMotors();

    Serial.println(F("\n========================================"));
    Serial.println(F("  FLIGHT CONTROLLER READY!"));
    Serial.println(F("========================================\n"));

    #ifdef CALIBRATION_MODE
    Serial.println(F("=== CALIBRATION MODE ==="));
    Serial.println(F("Serial commands (type in monitor):"));
    Serial.println(F("  a - Sequential calibration (guided, start here!)"));
    Serial.println(F("  c - Calibration status (what's done/pending)"));
    Serial.println(F("  r - Radio calibration"));
    Serial.println(F("  i - IMU cal (single-pos)"));
    Serial.println(F("  m - IMU cal (6-pos, more accurate)"));
    Serial.println(F("  o - IMU + Orientation"));
    Serial.println(F("  f - Failsafe auto-detection"));
    Serial.println(F("  e - ESC endpoint calibration"));
    Serial.println(F("  s - Status"));
    Serial.println(F("  g - PID gains (show/set)"));
    Serial.println(F("  p - Filter & limits (show/set)"));
    Serial.println(F("  t - Toggle telemetry"));
    Serial.println(F("  h - Help"));
    Serial.println(F("\nCH6 switch (hold 3s):"));
    Serial.println(F("  Mid:  IMU cal"));
    Serial.println(F("  High: IMU + Orientation"));
    #endif

    setupBlink(3, 160);

    //------------------------------------------------------------------
    // ESP32: Start dual-core architecture
    //------------------------------------------------------------------
    #ifdef USE_ESP32
    // Create telemetry data queue (depth 1, always latest data)
    // Used by: display, web server, API client on Core 1
    displayQueue = xQueueCreate(1, sizeof(DisplayData_t));

    // Start WiFi stack + optional web server + API client on Core 1
    #ifdef USE_WIFI
    setupWiFi();
    #endif
    #ifdef USE_WEB_SERVER
    setupWebServer();
    #endif
    #ifdef USE_API_SERVER
    setupApiClient();
    #endif
    #ifdef USE_OTA
    setupOTA();
    #endif

    #ifdef USE_OLED_DISPLAY
    displayStartupMessage("Starting FC...");
    #endif

    // Spawn flight control on Core 0 (high priority)
    xTaskCreatePinnedToCore(
        flightControlTask,
        "FlightCtrl",
        4096,
        NULL,
        3,                        // Priority 3 (high)
        &flightControlTaskHandle,
        0                         // Core 0
    );

    Serial.println(F("[ESP32] Flight control on Core 0"));
    Serial.println(F("[ESP32] Display + WiFi on Core 1"));
    #else
    // Teensy: single-core, loop() handles everything
    delay(1000);
    #endif
}

//========================================================================================================================//
//                                                   MAIN LOOP                                                             //
//========================================================================================================================//

void loop() {
    #ifndef USE_ESP32
    //------------------------------------------------------------------
    // TEENSY: Single-core - flight control + display in main loop
    //------------------------------------------------------------------
    flightControlTick();

    // Rate-limited OLED display update (10Hz)
    #ifdef USE_OLED_DISPLAY
    static unsigned long display_timer = 0;
    if (current_time - display_timer > 100000) {
        display_timer = current_time;
        DisplayData_t displayData;
        populateDisplayData(&displayData);
        renderDisplay(&displayData);
    }
    #endif

    #else
    //------------------------------------------------------------------
    // ESP32: Core 1 - display + WiFi + web server + API client
    // (flight control is on Core 0)
    //------------------------------------------------------------------

    // Receive latest telemetry from Core 0
    static DisplayData_t displayData = {};
    xQueueReceive(displayQueue, &displayData, pdMS_TO_TICKS(5));

    // Add network info (Core 1 owns WiFi data)
    #ifdef USE_WIFI
    populateNetworkData(&displayData);
    #endif

    // OLED display rendering at 10Hz
    #ifdef USE_OLED_DISPLAY
    static unsigned long display_timer = 0;
    unsigned long now_display = millis();
    if (now_display - display_timer > 100) {
        display_timer = now_display;
        renderDisplay(&displayData);
    }
    #endif

    // WiFi stack + optional web server + API client handling
    #ifdef USE_WIFI
    handleWiFi();
    #endif
    #ifdef USE_WEB_SERVER
    handleWebServer(&displayData);
    #endif
    #ifdef USE_API_SERVER
    handleApiClient(&displayData);
    #endif
    #ifdef USE_OTA
    handleOTA();
    #endif

    // Yield to other tasks (don't spin Core 1 at 100%)
    vTaskDelay(pdMS_TO_TICKS(10));

    #endif // USE_ESP32
}

//========================================================================================================================//
//                                              CALIBRATION FUNCTIONS                                                      //
//========================================================================================================================//

#ifdef CALIBRATION_MODE

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
    if (waitForConfirmation(30)) {
        Serial.println(F("\n--- Running 6-Position IMU Calibration ---"));
        calibrateIMU6Position();
    } else {
        Serial.println(F("\nRun single-position instead? (y/n)"));
        if (waitForConfirmation(15)) {
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
    if (waitForConfirmation(15)) {
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
    if (waitForConfirmation(15)) {
        Serial.println(F("\n--- Running Magnetometer Calibration ---"));
        calibrateMagnetometer();
    } else {
        Serial.println(F("Skipping magnetometer calibration."));
    }
    #else
    Serial.println(F("\n[DONE] Magnetometer already calibrated. Skipping."));
    #endif
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
    if (waitForConfirmation(30)) {
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
    if (waitForConfirmation(15)) {
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
    if (waitForConfirmation(30)) {
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
    Serial.println(F("  1. Copy all output values to config.h"));
    Serial.println(F("  2. Uncomment CALIBRATED_* markers for completed stages"));
    Serial.println(F("  3. Flash calibration build to verify"));
    Serial.println(F("  4. When all stages done, flash LIVE build and fly!"));
    Serial.println(F("\nRe-run any individual calibration with its command letter."));
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
                telemetry_mode = (telemetry_mode + 1) % 3;
                if (telemetry_mode == 0) {
                    Serial.println(F("\n>>> Telemetry OFF"));
                } else if (telemetry_mode == 1) {
                    Serial.println(F("\n>>> Telemetry: IMU (50Hz)"));
                    Serial.println(F("    Format: ax=X ay=Y az=Z gx=X gy=Y gz=Z"));
                } else {
                    Serial.println(F("\n>>> Telemetry: FULL (20Hz)"));
                    Serial.println(F("    Format: ax ay az gx gy gz roll pitch yaw m1 m2 m3 m4"));
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
            case 'a': case 'A':
                Serial.println(F("\n>>> Sequential Calibration requested via serial"));
                calibration_mode = CALIB_SEQUENTIAL;
                return;
            case 'c': case 'C':
                printCalibrationStatus();
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
                Serial.println(F("  r - Radio calibration (channel mapping)"));
                Serial.println(F("  i - IMU calibration (single-position, offsets only)"));
                Serial.println(F("  m - IMU calibration (6-position, offsets + scale)"));
                Serial.println(F("  o - IMU + Orientation detection"));
                Serial.println(F("  f - Failsafe auto-detection (measures receiver failsafe)"));
                Serial.println(F("  e - ESC endpoint calibration (min/max PWM)"));
                Serial.println(F("  s - Status (show channel values)"));
                Serial.println(F("  t - Toggle telemetry (off/IMU/full) for fc_tool"));
                Serial.println(F("  g - Show/set PID gains (g <name> <value>)"));
                Serial.println(F("  p - Show/set filter & limits (p <name> <value>)"));
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

//========================================================================================================================//
//                                              HELPER FUNCTIONS                                                           //
//========================================================================================================================//

void updateStatusLED() {
    static unsigned long led_timer = 0;
    #ifdef CALIBRATION_MODE
    if (calibration_in_progress) {
        if (current_time - led_timer > 100000) {
            led_timer = current_time;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    } else
    #endif
    if (armedFly) {
        digitalWrite(LED_PIN, HIGH);
    } else {
        if (current_time - led_timer > 500000) {
            led_timer = current_time;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
}

void setupBlink(int num_blinks, int blink_delay) {
    for (int i = 0; i < num_blinks; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(blink_delay);
        digitalWrite(LED_PIN, LOW);
        delay(blink_delay);
    }
}
