/*
 * Motors Module Implementation
 * Motor and servo control
 *
 * Platform support:
 * - Teensy: Uses analogWrite with PWMServo library
 * - ESP32: Uses LEDC peripheral for PWM output
 */

#include "motors.h"
#include "globals.h"
#include "config.h"
#include "pin_definitions.h"
#include "radioComm.h"

#ifdef USE_ESP32
    // ESP32 LEDC PWM configuration
    #define LEDC_TIMER_BITS  12
    #define LEDC_BASE_FREQ   250   // 250Hz for ESCs (4ms period)
    #define LEDC_SERVO_FREQ  50    // 50Hz for servos

    // LEDC channel assignments (ESP32 has 16 channels, 0-15)
    #define MOTOR_LEDC_CH_1  0
    #define MOTOR_LEDC_CH_2  1
    #define MOTOR_LEDC_CH_3  2
    #define MOTOR_LEDC_CH_4  3
    #define MOTOR_LEDC_CH_5  4
    #define MOTOR_LEDC_CH_6  5
    #define SERVO_LEDC_CH_1  6
    #define SERVO_LEDC_CH_2  7
    #define SERVO_LEDC_CH_3  8
    #define SERVO_LEDC_CH_4  9
    #define SERVO_LEDC_CH_5  10
    #define SERVO_LEDC_CH_6  11
    #define SERVO_LEDC_CH_7  12

    // Helper: Convert PWM microseconds to LEDC duty cycle
    // For 250Hz: period = 4000us, so duty = (pwm_us / 4000) * 4095
    inline uint32_t pwmToDuty(int pwm_us) {
        return (uint32_t)((pwm_us / 4000.0) * 4095);
    }

    // Helper: Convert servo PWM to LEDC duty cycle
    // For 50Hz: period = 20000us, so duty = (pwm_us / 20000) * 4095
    inline uint32_t servoPwmToDuty(int pwm_us) {
        return (uint32_t)((pwm_us / 20000.0) * 4095);
    }
#endif

//========================================================================================================================//
//                                              MOTOR SETUP                                                                //
//========================================================================================================================//

void setupMotors() {
#ifdef USE_ESP32
    // ESP32: Setup LEDC channels for motors (250Hz)
    ledcSetup(MOTOR_LEDC_CH_1, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
    ledcSetup(MOTOR_LEDC_CH_2, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
    ledcSetup(MOTOR_LEDC_CH_3, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
    ledcSetup(MOTOR_LEDC_CH_4, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
    ledcSetup(MOTOR_LEDC_CH_5, LEDC_BASE_FREQ, LEDC_TIMER_BITS);
    ledcSetup(MOTOR_LEDC_CH_6, LEDC_BASE_FREQ, LEDC_TIMER_BITS);

    // Attach LEDC channels to motor pins
    ledcAttachPin(MOTOR_PIN_1, MOTOR_LEDC_CH_1);
    ledcAttachPin(MOTOR_PIN_2, MOTOR_LEDC_CH_2);
    ledcAttachPin(MOTOR_PIN_3, MOTOR_LEDC_CH_3);
    ledcAttachPin(MOTOR_PIN_4, MOTOR_LEDC_CH_4);
    ledcAttachPin(MOTOR_PIN_5, MOTOR_LEDC_CH_5);
    ledcAttachPin(MOTOR_PIN_6, MOTOR_LEDC_CH_6);

    // Setup LEDC channels for servos (50Hz)
    ledcSetup(SERVO_LEDC_CH_1, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);
    ledcSetup(SERVO_LEDC_CH_2, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);
    ledcSetup(SERVO_LEDC_CH_3, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);
    ledcSetup(SERVO_LEDC_CH_4, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);
    ledcSetup(SERVO_LEDC_CH_5, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);
    ledcSetup(SERVO_LEDC_CH_6, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);
    ledcSetup(SERVO_LEDC_CH_7, LEDC_SERVO_FREQ, LEDC_TIMER_BITS);

    // Attach LEDC channels to servo pins
    ledcAttachPin(SERVO_PIN_1, SERVO_LEDC_CH_1);
    ledcAttachPin(SERVO_PIN_2, SERVO_LEDC_CH_2);
    ledcAttachPin(SERVO_PIN_3, SERVO_LEDC_CH_3);
    ledcAttachPin(SERVO_PIN_4, SERVO_LEDC_CH_4);
    ledcAttachPin(SERVO_PIN_5, SERVO_LEDC_CH_5);
    ledcAttachPin(SERVO_PIN_6, SERVO_LEDC_CH_6);
    ledcAttachPin(SERVO_PIN_7, SERVO_LEDC_CH_7);

#else
    // Teensy: Setup motor pins as outputs
    pinMode(MOTOR_PIN_1, OUTPUT);
    pinMode(MOTOR_PIN_2, OUTPUT);
    pinMode(MOTOR_PIN_3, OUTPUT);
    pinMode(MOTOR_PIN_4, OUTPUT);
    pinMode(MOTOR_PIN_5, OUTPUT);
    pinMode(MOTOR_PIN_6, OUTPUT);

    // Teensy: Setup servo pins using PWMServo library
    servo1.attach(SERVO_PIN_1);
    servo2.attach(SERVO_PIN_2);
    servo3.attach(SERVO_PIN_3);
    servo4.attach(SERVO_PIN_4);
    servo5.attach(SERVO_PIN_5);
    servo6.attach(SERVO_PIN_6);
    servo7.attach(SERVO_PIN_7);
#endif
}

//========================================================================================================================//
//                                              SCALE COMMANDS                                                             //
//========================================================================================================================//

void scaleCommands() {
    // Motors: OneShot125 (125-250us) or standard PWM (1000-2000us)
    #ifdef USE_ONESHOT125
        m1_command_PWM = m1_command_scaled * 125 + 125;
        m2_command_PWM = m2_command_scaled * 125 + 125;
        m3_command_PWM = m3_command_scaled * 125 + 125;
        m4_command_PWM = m4_command_scaled * 125 + 125;
        m5_command_PWM = m5_command_scaled * 125 + 125;
        m6_command_PWM = m6_command_scaled * 125 + 125;
    #else
        m1_command_PWM = m1_command_scaled * 1000 + 1000;
        m2_command_PWM = m2_command_scaled * 1000 + 1000;
        m3_command_PWM = m3_command_scaled * 1000 + 1000;
        m4_command_PWM = m4_command_scaled * 1000 + 1000;
        m5_command_PWM = m5_command_scaled * 1000 + 1000;
        m6_command_PWM = m6_command_scaled * 1000 + 1000;
    #endif

    // Servos: 1000-2000us (0-180 degrees)
    s1_command_PWM = (s1_command_scaled + 1.0) * 500 + 1000;
    s2_command_PWM = (s2_command_scaled + 1.0) * 500 + 1000;
    s3_command_PWM = (s3_command_scaled + 1.0) * 500 + 1000;
    s4_command_PWM = (s4_command_scaled + 1.0) * 500 + 1000;
    s5_command_PWM = (s5_command_scaled + 1.0) * 500 + 1000;
    s6_command_PWM = (s6_command_scaled + 1.0) * 500 + 1000;
    s7_command_PWM = (s7_command_scaled + 1.0) * 500 + 1000;
}

//========================================================================================================================//
//                                              THROTTLE CUT                                                               //
//========================================================================================================================//

void throttleCut() {
    if ((channel_5_pwm > 1500) || !armedFly) {
        armedFly = false;

        #ifdef USE_ONESHOT125
            m1_command_PWM = 125;
            m2_command_PWM = 125;
            m3_command_PWM = 125;
            m4_command_PWM = 125;
            m5_command_PWM = 125;
            m6_command_PWM = 125;
        #else
            m1_command_PWM = 1000;
            m2_command_PWM = 1000;
            m3_command_PWM = 1000;
            m4_command_PWM = 1000;
            m5_command_PWM = 1000;
            m6_command_PWM = 1000;
        #endif

        // Reset PID integrators when throttle cut
        integral_roll = 0.0;
        integral_pitch = 0.0;
        integral_yaw = 0.0;
    }
}

//========================================================================================================================//
//                                              ARM MOTORS                                                                 //
//========================================================================================================================//

void armMotors() {
    Serial.println(F("Arming ESCs..."));

    for (int i = 0; i < 100; i++) {
        #ifdef USE_ONESHOT125
            // OneShot125: bit-bang pulse timing
            digitalWrite(MOTOR_PIN_1, HIGH);
            digitalWrite(MOTOR_PIN_2, HIGH);
            digitalWrite(MOTOR_PIN_3, HIGH);
            digitalWrite(MOTOR_PIN_4, HIGH);
            digitalWrite(MOTOR_PIN_5, HIGH);
            digitalWrite(MOTOR_PIN_6, HIGH);
            delayMicroseconds(125);
            digitalWrite(MOTOR_PIN_1, LOW);
            digitalWrite(MOTOR_PIN_2, LOW);
            digitalWrite(MOTOR_PIN_3, LOW);
            digitalWrite(MOTOR_PIN_4, LOW);
            digitalWrite(MOTOR_PIN_5, LOW);
            digitalWrite(MOTOR_PIN_6, LOW);
            delay(2);

        #elif defined(USE_ESP32)
            // ESP32: Use LEDC to send minimum throttle
            uint32_t min_duty = pwmToDuty(1000);
            ledcWrite(MOTOR_LEDC_CH_1, min_duty);
            ledcWrite(MOTOR_LEDC_CH_2, min_duty);
            ledcWrite(MOTOR_LEDC_CH_3, min_duty);
            ledcWrite(MOTOR_LEDC_CH_4, min_duty);
            ledcWrite(MOTOR_LEDC_CH_5, min_duty);
            ledcWrite(MOTOR_LEDC_CH_6, min_duty);
            delay(20);

        #else
            // Teensy: Use analogWrite
            analogWriteFrequency(MOTOR_PIN_1, 250);
            analogWriteResolution(12);
            int pwm_value = (1000.0 / 4000.0) * 4095;
            analogWrite(MOTOR_PIN_1, pwm_value);
            analogWrite(MOTOR_PIN_2, pwm_value);
            analogWrite(MOTOR_PIN_3, pwm_value);
            analogWrite(MOTOR_PIN_4, pwm_value);
            analogWrite(MOTOR_PIN_5, pwm_value);
            analogWrite(MOTOR_PIN_6, pwm_value);
            delay(20);
        #endif

        if (i % 25 == 0) {
            Serial.print(".");
        }
    }
    Serial.println();
    Serial.println(F("ESCs armed"));
}

//========================================================================================================================//
//                                              COMMAND MOTORS                                                             //
//========================================================================================================================//

void commandMotors() {
    #ifdef USE_ONESHOT125
        // OneShot125: bit-bang pulse timing
        int wentLow = 0;
        int pulseStart, timer;
        int flagM1 = 0, flagM2 = 0, flagM3 = 0;
        int flagM4 = 0, flagM5 = 0, flagM6 = 0;

        digitalWrite(MOTOR_PIN_1, HIGH);
        digitalWrite(MOTOR_PIN_2, HIGH);
        digitalWrite(MOTOR_PIN_3, HIGH);
        digitalWrite(MOTOR_PIN_4, HIGH);
        digitalWrite(MOTOR_PIN_5, HIGH);
        digitalWrite(MOTOR_PIN_6, HIGH);
        pulseStart = micros();

        while (wentLow < 6) {
            timer = micros();
            if ((m1_command_PWM <= timer - pulseStart) && (flagM1 == 0)) {
                digitalWrite(MOTOR_PIN_1, LOW);
                wentLow++;
                flagM1 = 1;
            }
            if ((m2_command_PWM <= timer - pulseStart) && (flagM2 == 0)) {
                digitalWrite(MOTOR_PIN_2, LOW);
                wentLow++;
                flagM2 = 1;
            }
            if ((m3_command_PWM <= timer - pulseStart) && (flagM3 == 0)) {
                digitalWrite(MOTOR_PIN_3, LOW);
                wentLow++;
                flagM3 = 1;
            }
            if ((m4_command_PWM <= timer - pulseStart) && (flagM4 == 0)) {
                digitalWrite(MOTOR_PIN_4, LOW);
                wentLow++;
                flagM4 = 1;
            }
            if ((m5_command_PWM <= timer - pulseStart) && (flagM5 == 0)) {
                digitalWrite(MOTOR_PIN_5, LOW);
                wentLow++;
                flagM5 = 1;
            }
            if ((m6_command_PWM <= timer - pulseStart) && (flagM6 == 0)) {
                digitalWrite(MOTOR_PIN_6, LOW);
                wentLow++;
                flagM6 = 1;
            }
        }

    #elif defined(USE_ESP32)
        // ESP32: Use LEDC for PWM output
        ledcWrite(MOTOR_LEDC_CH_1, pwmToDuty(m1_command_PWM));
        ledcWrite(MOTOR_LEDC_CH_2, pwmToDuty(m2_command_PWM));
        ledcWrite(MOTOR_LEDC_CH_3, pwmToDuty(m3_command_PWM));
        ledcWrite(MOTOR_LEDC_CH_4, pwmToDuty(m4_command_PWM));
        ledcWrite(MOTOR_LEDC_CH_5, pwmToDuty(m5_command_PWM));
        ledcWrite(MOTOR_LEDC_CH_6, pwmToDuty(m6_command_PWM));

        // ESP32: Servos also use LEDC
        ledcWrite(SERVO_LEDC_CH_1, servoPwmToDuty(s1_command_PWM));
        ledcWrite(SERVO_LEDC_CH_2, servoPwmToDuty(s2_command_PWM));
        ledcWrite(SERVO_LEDC_CH_3, servoPwmToDuty(s3_command_PWM));
        ledcWrite(SERVO_LEDC_CH_4, servoPwmToDuty(s4_command_PWM));
        ledcWrite(SERVO_LEDC_CH_5, servoPwmToDuty(s5_command_PWM));
        ledcWrite(SERVO_LEDC_CH_6, servoPwmToDuty(s6_command_PWM));
        ledcWrite(SERVO_LEDC_CH_7, servoPwmToDuty(s7_command_PWM));
        return;  // Skip Teensy servo code below

    #else
        // Teensy: Use analogWrite
        analogWriteFrequency(MOTOR_PIN_1, 250);
        analogWriteResolution(12);

        analogWrite(MOTOR_PIN_1, (m1_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_2, (m2_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_3, (m3_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_4, (m4_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_5, (m5_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_6, (m6_command_PWM / 4000.0) * 4095);
    #endif

#ifndef USE_ESP32
    // Teensy: Servos use PWMServo library
    servo1.write((s1_command_PWM - 1000) * 0.18);
    servo2.write((s2_command_PWM - 1000) * 0.18);
    servo3.write((s3_command_PWM - 1000) * 0.18);
    servo4.write((s4_command_PWM - 1000) * 0.18);
    servo5.write((s5_command_PWM - 1000) * 0.18);
    servo6.write((s6_command_PWM - 1000) * 0.18);
    servo7.write((s7_command_PWM - 1000) * 0.18);
#endif
}

//========================================================================================================================//
//                                              LOOP RATE                                                                  //
//========================================================================================================================//

void loopRate(int freq) {
    float invFreq = 1.0 / freq * 1000000.0;
    unsigned long checker = micros();

    while (invFreq > (checker - current_time)) {
        checker = micros();
    }
}
