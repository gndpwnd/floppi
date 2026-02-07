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

// ============================================================================
// FC_TOOL TELEMETRY OUTPUT
// ============================================================================
// These functions output data in formats compatible with fc_tool's parser.
// See: fc_tool/docs/features/serial-telemetry-protocol.md

void printIMUTelemetry() {
    // Output at ~50Hz (20ms interval) for fc_tool visualization
    if (current_time - print_counter > 20000) {
        print_counter = current_time;
        // Key-value format: ax=1.23 ay=4.56 az=7.89 gx=0.12 gy=0.34 gz=0.56
        Serial.print(F("ax=")); Serial.print(AccX, 2);
        Serial.print(F(" ay=")); Serial.print(AccY, 2);
        Serial.print(F(" az=")); Serial.print(AccZ, 2);
        Serial.print(F(" gx=")); Serial.print(GyroX, 2);
        Serial.print(F(" gy=")); Serial.print(GyroY, 2);
        Serial.print(F(" gz=")); Serial.println(GyroZ, 2);
    }
}

void printAttitudeTelemetry() {
    // Output at ~50Hz for fc_tool visualization
    if (current_time - print_counter > 20000) {
        print_counter = current_time;
        // Key-value format for attitude
        Serial.print(F("roll=")); Serial.print(roll_IMU, 2);
        Serial.print(F(" pitch=")); Serial.print(pitch_IMU, 2);
        Serial.print(F(" yaw=")); Serial.println(yaw_IMU, 2);
    }
}

void printFullTelemetry() {
    // Complete telemetry packet at ~20Hz (50ms interval)
    if (current_time - print_counter > 50000) {
        print_counter = current_time;
        // IMU raw
        Serial.print(F("ax=")); Serial.print(AccX, 2);
        Serial.print(F(" ay=")); Serial.print(AccY, 2);
        Serial.print(F(" az=")); Serial.print(AccZ, 2);
        Serial.print(F(" gx=")); Serial.print(GyroX, 2);
        Serial.print(F(" gy=")); Serial.print(GyroY, 2);
        Serial.print(F(" gz=")); Serial.print(GyroZ, 2);
        // Attitude
        Serial.print(F(" roll=")); Serial.print(roll_IMU, 2);
        Serial.print(F(" pitch=")); Serial.print(pitch_IMU, 2);
        Serial.print(F(" yaw=")); Serial.print(yaw_IMU, 2);
        // Motors (PWM 1000-2000)
        Serial.print(F(" m1=")); Serial.print(m1_command_PWM);
        Serial.print(F(" m2=")); Serial.print(m2_command_PWM);
        Serial.print(F(" m3=")); Serial.print(m3_command_PWM);
        Serial.print(F(" m4=")); Serial.println(m4_command_PWM);
    }
}

#endif // CALIBRATION_MODE
