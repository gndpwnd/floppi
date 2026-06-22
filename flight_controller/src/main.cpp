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
#include "calibration_mode.h"
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
#if defined(USE_ESP32) && defined(USE_BAROMETER)
#include "barometer.h"
#endif
#if defined(USE_ESP32) && defined(USE_GPS)
#include "gps.h"
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
// MagX_prev/MagY_prev/MagZ_prev removed — sole consumer (9DOF mag LPF in
// imu.cpp) was deleted; see globals.h note. Restore alongside any future
// mag-filter implementation.

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
int telemetry_mode = 0;  // 0=off, 1=IMU, 2=full, 3=quaternion

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
    } else if (telemetry_mode == 3) {
        printQuaternionTelemetry();
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
    Serial.println(F("  d - Dump ALL calibration values (copy-paste for config.h)"));
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

    // Optional telemetry-only barometer — dedicated Core-1 FreeRTOS task
    // (priority 1, 3072 B stack). Spawned after the WiFi stack so its tasks
    // exist first; the task does its own begin() so a slow/absent sensor
    // never delays setup(). It NEVER touches the Core-0 flight loop.
    // See docs/findings/phase_w2_barometer_landed_2026-05-20.md.
    #ifdef USE_BAROMETER
    startBarometerTask();
    Serial.println(F("[ESP32] Barometer telemetry task on Core 1"));
    #endif

    // Optional GPS passthrough — dedicated Core-1 FreeRTOS task (priority 1,
    // 3072 B stack). Reads raw NMEA on UART1 RX-only and publishes the latest
    // sentence to a spinlock-guarded snapshot for the swarm API. PASSTHROUGH
    // ONLY: the FC parses nothing and the Core-0 flight loop never reads GPS.
    // The task does its own non-blocking begin() so a slow/absent module never
    // delays setup(). See docs/findings/phase_w5_gps_landed_2026-05-20.md.
    #ifdef USE_GPS
    startGpsTask();
    Serial.println(F("[ESP32] GPS passthrough task on Core 1"));
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
