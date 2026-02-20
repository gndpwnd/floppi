/*
 * Debug Module Implementation
 * Debug print functions (CALIBRATION_MODE only)
 */

#include "debug.h"
#include "config.h"

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
// Multi-graph format: name@plotId:value
// Plot 0 = Accelerometer, Plot 1 = Gyroscope, Plot 2 = Attitude, Plot 3 = Motors
// See: fc_tool/docs/features/serial-telemetry-protocol.md

void printIMUTelemetry() {
    // Output at ~50Hz (20ms interval) for fc_tool visualization
    if (current_time - print_counter > 20000) {
        print_counter = current_time;
        // Accel → plot 0, Gyro → plot 1
        Serial.print(F("ax@0:")); Serial.print(AccX, 2);
        Serial.print(F(" ay@0:")); Serial.print(AccY, 2);
        Serial.print(F(" az@0:")); Serial.print(AccZ, 2);
        Serial.print(F(" gx@1:")); Serial.print(GyroX, 2);
        Serial.print(F(" gy@1:")); Serial.print(GyroY, 2);
        Serial.print(F(" gz@1:")); Serial.println(GyroZ, 2);
    }
}

void printAttitudeTelemetry() {
    // Output at ~50Hz for fc_tool visualization
    if (current_time - print_counter > 20000) {
        print_counter = current_time;
        // Attitude → plot 2
        Serial.print(F("roll@2:")); Serial.print(roll_IMU, 2);
        Serial.print(F(" pitch@2:")); Serial.print(pitch_IMU, 2);
        Serial.print(F(" yaw@2:")); Serial.println(yaw_IMU, 2);
    }
}

void printFullTelemetry() {
    // Complete telemetry packet at ~20Hz (50ms interval)
    if (current_time - print_counter > 50000) {
        print_counter = current_time;
        // Accel → plot 0
        Serial.print(F("ax@0:")); Serial.print(AccX, 2);
        Serial.print(F(" ay@0:")); Serial.print(AccY, 2);
        Serial.print(F(" az@0:")); Serial.print(AccZ, 2);
        // Gyro → plot 1
        Serial.print(F(" gx@1:")); Serial.print(GyroX, 2);
        Serial.print(F(" gy@1:")); Serial.print(GyroY, 2);
        Serial.print(F(" gz@1:")); Serial.print(GyroZ, 2);
        // Attitude → plot 2
        Serial.print(F(" roll@2:")); Serial.print(roll_IMU, 2);
        Serial.print(F(" pitch@2:")); Serial.print(pitch_IMU, 2);
        Serial.print(F(" yaw@2:")); Serial.print(yaw_IMU, 2);
        // Motors → plot 3
        Serial.print(F(" m1@3:")); Serial.print(m1_command_PWM);
        Serial.print(F(" m2@3:")); Serial.print(m2_command_PWM);
        Serial.print(F(" m3@3:")); Serial.print(m3_command_PWM);
        Serial.print(F(" m4@3:")); Serial.println(m4_command_PWM);
    }
}

void printQuaternionTelemetry() {
    // Quaternion attitude at ~50Hz — avoids gimbal lock for flight computers
    // Plot 4 = quaternion, Plot 1 = gyro rates (useful during acro)
    if (current_time - print_counter > 20000) {
        print_counter = current_time;
        Serial.print(F("q0@4:")); Serial.print(q0, 4);
        Serial.print(F(" q1@4:")); Serial.print(q1, 4);
        Serial.print(F(" q2@4:")); Serial.print(q2, 4);
        Serial.print(F(" q3@4:")); Serial.print(q3, 4);
        Serial.print(F(" gx@1:")); Serial.print(GyroX, 2);
        Serial.print(F(" gy@1:")); Serial.print(GyroY, 2);
        Serial.print(F(" gz@1:")); Serial.println(GyroZ, 2);
    }
}

void printGyroSaturationCheck() {
    // One-shot gyro range utilization check — call during diagnostics.
    // Warns if gyro readings are near the full-scale limit.
    #ifdef USE_MPU6050
    float gyro_max = 0;
    #if defined(GYRO_250DPS)
        gyro_max = 250.0f;
    #elif defined(GYRO_500DPS)
        gyro_max = 500.0f;
    #elif defined(GYRO_1000DPS)
        gyro_max = 1000.0f;
    #elif defined(GYRO_2000DPS)
        gyro_max = 2000.0f;
    #endif
    float peak = max(max(abs(GyroX), abs(GyroY)), abs(GyroZ));
    float util = (peak / gyro_max) * 100.0f;
    Serial.print(F("Gyro range: +/-")); Serial.print((int)gyro_max);
    Serial.print(F("dps  Peak: ")); Serial.print(peak, 1);
    Serial.print(F("dps (")); Serial.print(util, 0); Serial.println(F("%)"));
    if (util > 90.0f) {
        Serial.println(F("WARNING: Gyro near saturation! Consider GYRO_2000DPS in config.h"));
    }
    #endif
}

#endif // CALIBRATION_MODE
