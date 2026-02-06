/*
 * Motors Module Implementation
 * Motor and servo control
 */

#include "motors.h"
#include "globals.h"
#include "config.h"
#include "pin_definitions.h"
#include "radioComm.h"

//========================================================================================================================//
//                                              MOTOR SETUP                                                                //
//========================================================================================================================//

void setupMotors() {
    // Setup motor pins
    pinMode(MOTOR_PIN_1, OUTPUT);
    pinMode(MOTOR_PIN_2, OUTPUT);
    pinMode(MOTOR_PIN_3, OUTPUT);
    pinMode(MOTOR_PIN_4, OUTPUT);
    pinMode(MOTOR_PIN_5, OUTPUT);
    pinMode(MOTOR_PIN_6, OUTPUT);

    // Setup servo pins
    servo1.attach(SERVO_PIN_1);
    servo2.attach(SERVO_PIN_2);
    servo3.attach(SERVO_PIN_3);
    servo4.attach(SERVO_PIN_4);
    servo5.attach(SERVO_PIN_5);
    servo6.attach(SERVO_PIN_6);
    servo7.attach(SERVO_PIN_7);
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
        #else
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
    #else
        analogWriteFrequency(MOTOR_PIN_1, 250);
        analogWriteResolution(12);

        analogWrite(MOTOR_PIN_1, (m1_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_2, (m2_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_3, (m3_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_4, (m4_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_5, (m5_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_6, (m6_command_PWM / 4000.0) * 4095);
    #endif

    // Servos
    servo1.write((s1_command_PWM - 1000) * 0.18);
    servo2.write((s2_command_PWM - 1000) * 0.18);
    servo3.write((s3_command_PWM - 1000) * 0.18);
    servo4.write((s4_command_PWM - 1000) * 0.18);
    servo5.write((s5_command_PWM - 1000) * 0.18);
    servo6.write((s6_command_PWM - 1000) * 0.18);
    servo7.write((s7_command_PWM - 1000) * 0.18);
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
