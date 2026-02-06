/*
 * Debug Module Implementation
 * Debug print functions (CALIBRATION_MODE only)
 */

#include "debug.h"

#ifdef CALIBRATION_MODE

#include "globals.h"
#include "radioComm.h"

void printRadioData() {
    if (current_time - print_counter > 100000) {
        print_counter = current_time;
        Serial.print(F("CH1:")); Serial.print(channel_1_pwm);
        Serial.print(F(" CH2:")); Serial.print(channel_2_pwm);
        Serial.print(F(" CH3:")); Serial.print(channel_3_pwm);
        Serial.print(F(" CH4:")); Serial.print(channel_4_pwm);
        Serial.print(F(" CH5:")); Serial.print(channel_5_pwm);
        Serial.print(F(" CH6:")); Serial.println(channel_6_pwm);
    }
}

void printDesiredState() {
    if (current_time - print_counter > 100000) {
        print_counter = current_time;
        Serial.print(F("thro_des:")); Serial.print(thro_des);
        Serial.print(F(" roll_des:")); Serial.print(roll_des);
        Serial.print(F(" pitch_des:")); Serial.print(pitch_des);
        Serial.print(F(" yaw_des:")); Serial.println(yaw_des);
    }
}

void printGyroData() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("GyroX:")); Serial.print(GyroX);
        Serial.print(F(" GyroY:")); Serial.print(GyroY);
        Serial.print(F(" GyroZ:")); Serial.println(GyroZ);
    }
}

void printAccelData() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("AccX:")); Serial.print(AccX);
        Serial.print(F(" AccY:")); Serial.print(AccY);
        Serial.print(F(" AccZ:")); Serial.println(AccZ);
    }
}

void printMagData() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("MagX:")); Serial.print(MagX);
        Serial.print(F(" MagY:")); Serial.print(MagY);
        Serial.print(F(" MagZ:")); Serial.println(MagZ);
    }
}

void printRollPitchYaw() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("roll:")); Serial.print(roll_IMU);
        Serial.print(F(" pitch:")); Serial.print(pitch_IMU);
        Serial.print(F(" yaw:")); Serial.println(yaw_IMU);
    }
}

void printPIDoutput() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("roll_PID:")); Serial.print(roll_PID);
        Serial.print(F(" pitch_PID:")); Serial.print(pitch_PID);
        Serial.print(F(" yaw_PID:")); Serial.println(yaw_PID);
    }
}

void printMotorCommands() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("m1:")); Serial.print(m1_command_PWM);
        Serial.print(F(" m2:")); Serial.print(m2_command_PWM);
        Serial.print(F(" m3:")); Serial.print(m3_command_PWM);
        Serial.print(F(" m4:")); Serial.print(m4_command_PWM);
        Serial.print(F(" m5:")); Serial.print(m5_command_PWM);
        Serial.print(F(" m6:")); Serial.println(m6_command_PWM);
    }
}

void printServoCommands() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("s1:")); Serial.print(s1_command_PWM);
        Serial.print(F(" s2:")); Serial.print(s2_command_PWM);
        Serial.print(F(" s3:")); Serial.print(s3_command_PWM);
        Serial.print(F(" s4:")); Serial.print(s4_command_PWM);
        Serial.print(F(" s5:")); Serial.print(s5_command_PWM);
        Serial.print(F(" s6:")); Serial.print(s6_command_PWM);
        Serial.print(F(" s7:")); Serial.println(s7_command_PWM);
    }
}

void printLoopRate() {
    if (current_time - print_counter > 10000) {
        print_counter = current_time;
        Serial.print(F("dt(us):")); Serial.println(dt * 1000000.0);
    }
}

#endif // CALIBRATION_MODE
