/*
 * dRehmFlight VTOL - Complete Flight Controller
 * PlatformIO Port with Comment/Uncomment Debug Sections
 * 
 * USAGE:
 * - This is the complete flight controller with ALL functionality
 * - Uncomment sections in loop() to enable debug output
 * - Debug prints DO NOT affect flight controller operation
 * - Use Serial Monitor to view text output
 * - Use Serial Plotter to visualize IMU/PID data
 * 
 * Original by Nicholas Rehm (dRehmFlight)
 * PlatformIO port
 */

#include <Arduino.h>
#include <Wire.h>
#include <PWMServo.h>

#include "config.h"
#include "pin_definitions.h"
#include "radioComm.h"

//========================================================================================================================//
//                                                    IMU SETUP                                                           //
//========================================================================================================================//

#ifdef USE_MPU6050
    #include "MPU6050.h"
    #include "I2Cdev.h"
    MPU6050 mpu6050;
#elif defined(USE_MPU9250)
    #include "MPU9250.h"
    MPU9250 mpu9250(SPI,36);
#endif

//========================================================================================================================//
//                                                 GLOBAL VARIABLES                                                       //
//========================================================================================================================//

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
float AccErrorX = 0.0, AccErrorY = 0.0, AccErrorZ = 0.0;
float GyroErrorX = 0.0, GyroErrorY = 0.0, GyroErrorZ = 0.0;

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

// Servo commands
PWMServo servo1, servo2, servo3, servo4, servo5, servo6, servo7;
int s1_command_PWM, s2_command_PWM, s3_command_PWM, s4_command_PWM, s5_command_PWM, s6_command_PWM, s7_command_PWM;
float s1_command_scaled, s2_command_scaled, s3_command_scaled, s4_command_scaled, s5_command_scaled, s6_command_scaled, s7_command_scaled;

// Arming state
bool armedFly = false;

//========================================================================================================================//
//                                              FUNCTION DECLARATIONS                                                     //
//========================================================================================================================//

void setupBlink(int num_blinks, int blink_delay);
void getIMUdata();
void calculate_IMU_error();
void calibrateAttitude();
void Madgwick(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz, float invSampleFreq);
void Madgwick6DOF(float gx, float gy, float gz, float ax, float ay, float az, float invSampleFreq);
void getDesState();
void controlRATE();
void controlANGLE();
void controlMixer();
void scaleCommands();
void throttleCut();
void armMotors();
void armedStatus();
void commandMotors();
void failSafe();
void loopRate(int freq);
float invSqrt(float x);

// Debug print functions
void printRadioData();
void printDesiredState();
void printGyroData();
void printAccelData();
void printMagData();
void printRollPitchYaw();
void printPIDoutput();
void printMotorCommands();
void printServoCommands();
void printLoopRate();

//========================================================================================================================//
//                                                      SETUP                                                             //
//========================================================================================================================//

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println(F("\n\n========================================"));
    Serial.println(F("  dRehmFlight VTOL Flight Controller"));
    Serial.println(F("========================================"));
    
    // Initialize pins
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    
    // Initialize I2C/SPI for IMU
    #ifdef USE_MPU6050
        Wire.begin();
        Wire.setClock(400000);
    #elif defined(USE_MPU9250)
        SPI.begin();
    #endif
    
    // Initialize IMU
    Serial.println(F("\nInitializing IMU..."));
    #ifdef USE_MPU6050
        mpu6050.initialize();
        if (!mpu6050.testConnection()) {
            Serial.println(F("ERROR: MPU6050 connection failed!"));
            while (1) { setupBlink(3, 500); }
        }
        Serial.println(F("MPU6050 connected"));
        mpu6050.setFullScaleGyroRange(GYRO_SCALE);
        mpu6050.setFullScaleAccelRange(ACCEL_SCALE);
        
    #elif defined(USE_MPU9250)
        int status = mpu9250.begin();
        if (status < 0) {
            Serial.println(F("ERROR: MPU9250 initialization failed"));
            while (1) { setupBlink(3, 500); }
        }
        Serial.println(F("MPU9250 connected"));
        mpu9250.setGyroRange(GYRO_SCALE);
        mpu9250.setAccelRange(ACCEL_SCALE);
    #endif
    
    // Calibrate IMU
    Serial.println(F("\nCalibrating IMU..."));
    Serial.println(F("Keep board FLAT and STILL!"));
    delay(2000);
    calculate_IMU_error();
    
    // Calibrate attitude (warm up Madgwick filter)
    Serial.println(F("Calibrating attitude..."));
    calibrateAttitude();
    Serial.println(F("IMU calibration complete"));
    
    // Initialize receiver
    Serial.println(F("\nInitializing receiver..."));
    radioSetup();
    delay(500);
    
    // Setup motor pins
    pinMode(MOTOR_PIN_1, OUTPUT);
    pinMode(MOTOR_PIN_2, OUTPUT);
    pinMode(MOTOR_PIN_3, OUTPUT);
    pinMode(MOTOR_PIN_4, OUTPUT);
    pinMode(MOTOR_PIN_5, OUTPUT);
    pinMode(MOTOR_PIN_6, OUTPUT);
    
    // Setup servos
    servo1.attach(SERVO_PIN_1);
    servo2.attach(SERVO_PIN_2);
    servo3.attach(SERVO_PIN_3);
    servo4.attach(SERVO_PIN_4);
    servo5.attach(SERVO_PIN_5);
    servo6.attach(SERVO_PIN_6);
    servo7.attach(SERVO_PIN_7);
    
    // Arm motors (send minimum pulses)
    Serial.println(F("\nArming ESCs..."));
    armMotors();
    Serial.println(F("ESCs armed"));
    
    Serial.println(F("\n========================================"));
    Serial.println(F("  FLIGHT CONTROLLER READY!"));
    Serial.println(F("========================================\n"));
    
    // Final setup blink
    setupBlink(3, 160);
    
    delay(1000);
}

//========================================================================================================================//
//                                                   MAIN LOOP                                                            //
//========================================================================================================================//

void loop() {
    // Keep track of time
    prev_time = current_time;
    current_time = micros();
    dt = (current_time - prev_time) / 1000000.0;
    
    // ========== CORE FLIGHT CONTROLLER (ALWAYS RUNS) ==========
    
    // Get radio commands
    getCommands();
    
    // Check failsafe
    failSafe();
    
    // Get IMU data
    getIMUdata();
    
    // Attitude estimation with Madgwick filter
    #ifdef USE_MPU6050
        Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt);
    #elif defined(USE_MPU9250)
        Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
    #endif
    
    // Get desired state from radio
    getDesState();
    
    // Check arming status (throttle low + channel 5 low)
    armedStatus();
    
    // Only run PID controller if armed
    if (armedFly) {
        // Run flight controller
        #ifdef USE_RATE_CONTROLLER
            controlRATE();
        #elif defined(USE_ANGLE_CONTROLLER)
            controlANGLE();
        #endif
        
        // Mix control outputs to motors/servos
        controlMixer();
    } else {
        // Not armed - zero out PID outputs
        roll_PID = 0;
        pitch_PID = 0;
        yaw_PID = 0;
        
        // Set to minimum throttle in mixer
        m1_command_scaled = 0;
        m2_command_scaled = 0;
        m3_command_scaled = 0;
        m4_command_scaled = 0;
        m5_command_scaled = 0;
        m6_command_scaled = 0;
    }
    
    // Scale commands to PWM range
    scaleCommands();
    
    // Apply throttle cut if needed (safety override)
    throttleCut();
    
    // Send commands to motors and servos
    commandMotors();
    
    // ========== DEBUG OUTPUT (UNCOMMENT WHAT YOU WANT TO SEE) ==========
    // ⚠️ WARNING: Debug prints WILL reduce loop rate!
    //   - No debug:     ~2000Hz (500µs loop time)  
    //   - 1-2 prints:   ~1800Hz (555µs loop time)
    //   - 3+ prints:    ~1500Hz (666µs loop time)
    //   - All prints:   ~1000Hz (1000µs loop time)
    // 
    // For TUNING: Some debug is OK
    // For FLIGHT: Comment out ALL debug for max performance
    // 
    // These DO NOT affect flight controller operation, only loop speed:
    
    //printRadioData();        // Print receiver channels
    //printDesiredState();     // Print desired roll/pitch/yaw
    //printGyroData();         // Print gyro X/Y/Z (good for plotter)
    //printAccelData();        // Print accel X/Y/Z (good for plotter)
    //printMagData();          // Print mag X/Y/Z (MPU9250 only)
    //printRollPitchYaw();     // Print attitude angles (good for plotter)
    //printPIDoutput();        // Print PID controller outputs (good for tuning)
    //printMotorCommands();    // Print motor PWM commands
    //printServoCommands();    // Print servo PWM commands
    //printLoopRate();         // Print loop timing (target: 2kHz = 500µs)
    
    // ========== COMBINED DEBUG OUTPUTS ==========
    // Uncomment these for specific testing scenarios:
    
    // IMU Calibration Test (use with Serial Plotter):
    //printGyroData();
    //printAccelData();
    //printRollPitchYaw();
    
    // Receiver Test:
    //printRadioData();
    
    // PID Tuning Test (use with Serial Plotter):
    //printRollPitchYaw();
    //printPIDoutput();
    
    // Motor Output Test:
    //printMotorCommands();
    //printServoCommands();
    
    // ========== STATUS LED ==========
    // Blink LED at 1Hz to show system is running
    if (current_time - print_counter > 500000) {
        print_counter = current_time;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    
    // Maintain loop rate (2kHz = 500µs)
    loopRate(LOOP_FREQUENCY_HZ);
}

//========================================================================================================================//
//                                                  IMU FUNCTIONS                                                         //
//========================================================================================================================//

void getIMUdata() {
    #ifdef USE_MPU6050
        mpu6050.getMotion6(&AcX, &AcY, &AcZ, &GyX, &GyY, &GyZ);
        
        // Convert to g's and deg/s
        AccX = AcX / ACCEL_SCALE_FACTOR;
        AccY = AcY / ACCEL_SCALE_FACTOR;
        AccZ = AcZ / ACCEL_SCALE_FACTOR;
        
        GyroX = GyX / GYRO_SCALE_FACTOR;
        GyroY = GyY / GYRO_SCALE_FACTOR;
        GyroZ = GyZ / GYRO_SCALE_FACTOR;
        
    #elif defined(USE_MPU9250)
        mpu9250.readSensor();
        
        AccX = mpu9250.getAccelX_mss() / 9.81;
        AccY = mpu9250.getAccelY_mss() / 9.81;
        AccZ = mpu9250.getAccelZ_mss() / 9.81;
        
        GyroX = mpu9250.getGyroX_rads() * 57.2957795;
        GyroY = mpu9250.getGyroY_rads() * 57.2957795;
        GyroZ = mpu9250.getGyroZ_rads() * 57.2957795;
        
        MagX = mpu9250.getMagX_uT();
        MagY = mpu9250.getMagY_uT();
        MagZ = mpu9250.getMagZ_uT();
    #endif
    
    // Apply calibration
    AccX -= AccErrorX;
    AccY -= AccErrorY;
    AccZ -= AccErrorZ;
    
    GyroX -= GyroErrorX;
    GyroY -= GyroErrorY;
    GyroZ -= GyroErrorZ;
    
    // Low-pass filter
    AccX = (1.0 - B_ACCEL) * AccX_prev + B_ACCEL * AccX;
    AccY = (1.0 - B_ACCEL) * AccY_prev + B_ACCEL * AccY;
    AccZ = (1.0 - B_ACCEL) * AccZ_prev + B_ACCEL * AccZ;
    
    GyroX = (1.0 - B_GYRO) * GyroX_prev + B_GYRO * GyroX;
    GyroY = (1.0 - B_GYRO) * GyroY_prev + B_GYRO * GyroY;
    GyroZ = (1.0 - B_GYRO) * GyroZ_prev + B_GYRO * GyroZ;
    
    #ifdef USE_MPU9250
        MagX = (1.0 - B_MAG) * MagX_prev + B_MAG * MagX;
        MagY = (1.0 - B_MAG) * MagY_prev + B_MAG * MagY;
        MagZ = (1.0 - B_MAG) * MagZ_prev + B_MAG * MagZ;
    #endif
    
    // Save previous values
    AccX_prev = AccX;
    AccY_prev = AccY;
    AccZ_prev = AccZ;
    
    GyroX_prev = GyroX;
    GyroY_prev = GyroY;
    GyroZ_prev = GyroZ;
    
    #ifdef USE_MPU9250
        MagX_prev = MagX;
        MagY_prev = MagY;
        MagZ_prev = MagZ;
    #endif
}

void calculate_IMU_error() {
    int num_readings = 2000;
    float AccX_sum = 0, AccY_sum = 0, AccZ_sum = 0;
    float GyroX_sum = 0, GyroY_sum = 0, GyroZ_sum = 0;
    
    for (int i = 0; i < num_readings; i++) {
        #ifdef USE_MPU6050
            mpu6050.getMotion6(&AcX, &AcY, &AcZ, &GyX, &GyY, &GyZ);
            AccX = AcX / ACCEL_SCALE_FACTOR;
            AccY = AcY / ACCEL_SCALE_FACTOR;
            AccZ = AcZ / ACCEL_SCALE_FACTOR;
            GyroX = GyX / GYRO_SCALE_FACTOR;
            GyroY = GyY / GYRO_SCALE_FACTOR;
            GyroZ = GyZ / GYRO_SCALE_FACTOR;
            
        #elif defined(USE_MPU9250)
            mpu9250.readSensor();
            AccX = mpu9250.getAccelX_mss() / 9.81;
            AccY = mpu9250.getAccelY_mss() / 9.81;
            AccZ = mpu9250.getAccelZ_mss() / 9.81;
            GyroX = mpu9250.getGyroX_rads() * 57.2957795;
            GyroY = mpu9250.getGyroY_rads() * 57.2957795;
            GyroZ = mpu9250.getGyroZ_rads() * 57.2957795;
        #endif
        
        AccX_sum += AccX;
        AccY_sum += AccY;
        AccZ_sum += AccZ;
        GyroX_sum += GyroX;
        GyroY_sum += GyroY;
        GyroZ_sum += GyroZ;
        
        delay(1);
    }
    
    AccErrorX = AccX_sum / num_readings;
    AccErrorY = AccY_sum / num_readings;
    AccErrorZ = (AccZ_sum / num_readings) - 1.0;
    
    GyroErrorX = GyroX_sum / num_readings;
    GyroErrorY = GyroY_sum / num_readings;
    GyroErrorZ = GyroZ_sum / num_readings;
    
    Serial.print(F("AccError: X=")); Serial.print(AccErrorX, 4);
    Serial.print(F(" Y=")); Serial.print(AccErrorY, 4);
    Serial.print(F(" Z=")); Serial.println(AccErrorZ, 4);
    Serial.print(F("GyroError: X=")); Serial.print(GyroErrorX, 4);
    Serial.print(F(" Y=")); Serial.print(GyroErrorY, 4);
    Serial.print(F(" Z=")); Serial.println(GyroErrorZ, 4);
}

//========================================================================================================================//
//                                          CALIBRATE ATTITUDE FUNCTION                                                   //
//========================================================================================================================//

void calibrateAttitude() {
    //DESCRIPTION: Warm up the Madgwick filter before flight
    /*
     * This function runs on startup to allow the attitude estimation to converge.
     * The vehicle should be powered up on a level surface!
     */
    Serial.println(F("Warming up attitude filter..."));
    
    // Warm up IMU and madgwick filter
    for (int i = 0; i <= 10000; i++) {
        prev_time = current_time;      
        current_time = micros();      
        dt = (current_time - prev_time) / 1000000.0;
        
        getIMUdata();
        
        #ifdef USE_MPU6050
            Madgwick6DOF(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, dt);
        #elif defined(USE_MPU9250)
            Madgwick(GyroX, -GyroY, -GyroZ, -AccX, AccY, AccZ, MagY, -MagX, MagZ, dt);
        #endif
        
        loopRate(2000); // 2kHz
        
        // Progress indicator every 1000 iterations
        if (i % 1000 == 0) {
            Serial.print(".");
        }
    }
    Serial.println();
    Serial.println(F("Attitude filter ready"));
}

void Madgwick6DOF(float gx, float gy, float gz, float ax, float ay, float az, float invSampleFreq) {
    // 6DOF Madgwick filter (no magnetometer)
    gx *= 0.0174533;
    gy *= 0.0174533;
    gz *= 0.0174533;
    
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;
    
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);
    
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        recipNorm = invSqrt(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;
        
        _2q0 = 2.0f * q0;
        _2q1 = 2.0f * q1;
        _2q2 = 2.0f * q2;
        _2q3 = 2.0f * q3;
        _4q0 = 4.0f * q0;
        _4q1 = 4.0f * q1;
        _4q2 = 4.0f * q2;
        _8q1 = 8.0f * q1;
        _8q2 = 8.0f * q2;
        q0q0 = q0 * q0;
        q1q1 = q1 * q1;
        q2q2 = q2 * q2;
        q3q3 = q3 * q3;
        
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
        
        recipNorm = invSqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;
        
        qDot1 -= MADGWICK_BETA * s0;
        qDot2 -= MADGWICK_BETA * s1;
        qDot3 -= MADGWICK_BETA * s2;
        qDot4 -= MADGWICK_BETA * s3;
    }
    
    q0 += qDot1 * invSampleFreq;
    q1 += qDot2 * invSampleFreq;
    q2 += qDot3 * invSampleFreq;
    q3 += qDot4 * invSampleFreq;
    
    recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 *= recipNorm;
    q1 *= recipNorm;
    q2 *= recipNorm;
    q3 *= recipNorm;
    
    roll_IMU = atan2(q0 * q1 + q2 * q3, 0.5f - q1 * q1 - q2 * q2) * 57.2957795;
    pitch_IMU = asin(-2.0f * (q1 * q3 - q0 * q2)) * 57.2957795;
    yaw_IMU = atan2(q1 * q2 + q0 * q3, 0.5f - q2 * q2 - q3 * q3) * 57.2957795;
}

void Madgwick(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz, float invSampleFreq) {
    // 9DOF Madgwick filter (with magnetometer) - implement if using MPU9250
    Madgwick6DOF(gx, gy, gz, ax, ay, az, invSampleFreq);  // Simplified for now
}

//========================================================================================================================//
//                                              CONTROL FUNCTIONS                                                         //
//========================================================================================================================//

void getDesState() {
    thro_des = (channel_3_pwm - 1000.0) / 1000.0;
    roll_des = (channel_1_pwm - 1500.0) / 500.0;
    pitch_des = (channel_2_pwm - 1500.0) / 500.0;
    yaw_des = (channel_4_pwm - 1500.0) / 500.0;
    
    thro_des = constrain(thro_des, 0.0, 1.0);
    roll_des = constrain(roll_des, -1.0, 1.0);
    pitch_des = constrain(pitch_des, -1.0, 1.0);
    yaw_des = constrain(yaw_des, -1.0, 1.0);
    
    #ifdef USE_RATE_CONTROLLER
        roll_des *= MAX_ROLL_RATE;
        pitch_des *= MAX_PITCH_RATE;
        yaw_des *= MAX_YAW_RATE;
    #else
        roll_des *= MAX_ROLL_ANGLE;
        pitch_des *= MAX_PITCH_ANGLE;
        yaw_des *= MAX_YAW_RATE;
    #endif
    
    roll_passthru = (channel_1_pwm - 1500.0) / 500.0;
    pitch_passthru = (channel_2_pwm - 1500.0) / 500.0;
    yaw_passthru = (channel_4_pwm - 1500.0) / 500.0;
}

void controlRATE() {
    // Roll
    error_roll = roll_des - GyroX;
    integral_roll += error_roll * dt;
    integral_roll = constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL);
    derivative_roll = (error_roll - error_roll_prev) / dt;
    roll_PID = KP_ROLL_RATE * error_roll + KI_ROLL_RATE * integral_roll + KD_ROLL_RATE * derivative_roll;
    error_roll_prev = error_roll;
    
    // Pitch
    error_pitch = pitch_des - GyroY;
    integral_pitch += error_pitch * dt;
    integral_pitch = constrain(integral_pitch, -I_LIMIT_PITCH, I_LIMIT_PITCH);
    derivative_pitch = (error_pitch - error_pitch_prev) / dt;
    pitch_PID = KP_PITCH_RATE * error_pitch + KI_PITCH_RATE * integral_pitch + KD_PITCH_RATE * derivative_pitch;
    error_pitch_prev = error_pitch;
    
    // Yaw
    error_yaw = yaw_des - GyroZ;
    integral_yaw += error_yaw * dt;
    integral_yaw = constrain(integral_yaw, -I_LIMIT_YAW, I_LIMIT_YAW);
    derivative_yaw = (error_yaw - error_yaw_prev) / dt;
    yaw_PID = KP_YAW_RATE * error_yaw + KI_YAW_RATE * integral_yaw + KD_YAW_RATE * derivative_yaw;
    error_yaw_prev = error_yaw;
}

void controlANGLE() {
    // Roll
    error_roll = roll_des - roll_IMU;
    integral_roll += error_roll * dt;
    integral_roll = constrain(integral_roll, -I_LIMIT_ROLL, I_LIMIT_ROLL);
    derivative_roll = -GyroX;
    roll_PID = KP_ROLL_ANGLE * error_roll + KI_ROLL_ANGLE * integral_roll + KD_ROLL_ANGLE * derivative_roll;
    error_roll_prev = error_roll;
    
    // Pitch
    error_pitch = pitch_des - pitch_IMU;
    integral_pitch += error_pitch * dt;
    integral_pitch = constrain(integral_pitch, -I_LIMIT_PITCH, I_LIMIT_PITCH);
    derivative_pitch = -GyroY;
    pitch_PID = KP_PITCH_ANGLE * error_pitch + KI_PITCH_ANGLE * integral_pitch + KD_PITCH_ANGLE * derivative_pitch;
    error_pitch_prev = error_pitch;
    
    // Yaw (rate control)
    error_yaw = yaw_des - GyroZ;
    integral_yaw += error_yaw * dt;
    integral_yaw = constrain(integral_yaw, -I_LIMIT_YAW, I_LIMIT_YAW);
    derivative_yaw = (error_yaw - error_yaw_prev) / dt;
    yaw_PID = KP_YAW_RATE * error_yaw + KI_YAW_RATE * integral_yaw + KD_YAW_RATE * derivative_yaw;
    error_yaw_prev = error_yaw;
}

void controlMixer() {
    // *** CUSTOMIZE FOR YOUR AIRCRAFT ***
    // Quadcopter X configuration (default)
    m1_command_scaled = thro_des - pitch_PID + roll_PID + yaw_PID;
    m2_command_scaled = thro_des - pitch_PID - roll_PID - yaw_PID;
    m3_command_scaled = thro_des + pitch_PID - roll_PID + yaw_PID;
    m4_command_scaled = thro_des + pitch_PID + roll_PID - yaw_PID;
    m5_command_scaled = 0.0;
    m6_command_scaled = 0.0;
    
    m1_command_scaled = constrain(m1_command_scaled, 0.0, 1.0);
    m2_command_scaled = constrain(m2_command_scaled, 0.0, 1.0);
    m3_command_scaled = constrain(m3_command_scaled, 0.0, 1.0);
    m4_command_scaled = constrain(m4_command_scaled, 0.0, 1.0);
    m5_command_scaled = constrain(m5_command_scaled, 0.0, 1.0);
    m6_command_scaled = constrain(m6_command_scaled, 0.0, 1.0);
    
    // Servo mixing (passthrough example)
    s1_command_scaled = roll_passthru;
    s2_command_scaled = pitch_passthru;
    s3_command_scaled = yaw_passthru;
    s4_command_scaled = 0.0;
    s5_command_scaled = 0.0;
    s6_command_scaled = 0.0;
    s7_command_scaled = 0.0;
    
    s1_command_scaled = constrain(s1_command_scaled, -1.0, 1.0);
    s2_command_scaled = constrain(s2_command_scaled, -1.0, 1.0);
    s3_command_scaled = constrain(s3_command_scaled, -1.0, 1.0);
    s4_command_scaled = constrain(s4_command_scaled, -1.0, 1.0);
    s5_command_scaled = constrain(s5_command_scaled, -1.0, 1.0);
    s6_command_scaled = constrain(s6_command_scaled, -1.0, 1.0);
    s7_command_scaled = constrain(s7_command_scaled, -1.0, 1.0);
}

//========================================================================================================================//
//                                          ARMED STATUS FUNCTION                                                         //
//========================================================================================================================//

void armedStatus() {
    //DESCRIPTION: Check arming conditions and update armedFly status
    /*
     * Vehicle can arm when:
     * 1. Throttle cut switch (CH5) is OFF (< 1500)
     * 2. Throttle stick is LOW (< 1050)
     * 
     * Once armed, vehicle stays armed until throttle cut switch is activated.
     */
    static bool was_armed = false;
    
    // Check arming conditions
    bool throttle_low = (channel_3_pwm < 1050);
    bool throttle_cut_off = (channel_5_pwm < 1500);
    
    if (!armedFly && throttle_low && throttle_cut_off) {
        // Conditions met to arm
        armedFly = true;
        
        // Reset integrators on arming
        integral_roll = 0.0;
        integral_pitch = 0.0;
        integral_yaw = 0.0;
        
        Serial.println(F("*** ARMED ***"));
    }
    
    // Disarm if throttle cut is activated
    if (armedFly && !throttle_cut_off) {
        armedFly = false;
        Serial.println(F("*** DISARMED ***"));
    }
    
    // Status LED indication
    if (armedFly && !was_armed) {
        // Just armed - fast blink
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
            delay(50);
        }
    }
    
    was_armed = armedFly;
}

//========================================================================================================================//
//                                          MOTOR/SERVO CONTROL FUNCTIONS                                                 //
//========================================================================================================================//

void scaleCommands() {
    // Motors: OneShot125 (125-250μs) or standard PWM (1000-2000μs)
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
    
    // Servos: 1000-2000μs (0-180 degrees)
    s1_command_PWM = (s1_command_scaled + 1.0) * 500 + 1000;
    s2_command_PWM = (s2_command_scaled + 1.0) * 500 + 1000;
    s3_command_PWM = (s3_command_scaled + 1.0) * 500 + 1000;
    s4_command_PWM = (s4_command_scaled + 1.0) * 500 + 1000;
    s5_command_PWM = (s5_command_scaled + 1.0) * 500 + 1000;
    s6_command_PWM = (s6_command_scaled + 1.0) * 500 + 1000;
    s7_command_PWM = (s7_command_scaled + 1.0) * 500 + 1000;
}

void throttleCut() {
    //DESCRIPTION: Cut throttle if channel 5 is high or not armed
    /*
     * Safety feature that sets all motor commands to minimum if:
     * - Channel 5 (throttle cut switch) is high, OR
     * - Vehicle is not armed
     */
    if ((channel_5_pwm > 1500) || !armedFly) {
        armedFly = false;
        
        #ifdef USE_ONESHOT125
            // OneShot125 minimum: 125µs
            m1_command_PWM = 125;
            m2_command_PWM = 125;
            m3_command_PWM = 125;
            m4_command_PWM = 125;
            m5_command_PWM = 125;
            m6_command_PWM = 125;
        #else
            // Standard PWM minimum: 1000µs
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

void armMotors() {
    //DESCRIPTION: Arm ESCs by sending minimum throttle commands
    Serial.println(F("Arming ESCs..."));
    
    for (int i = 0; i < 100; i++) {
        #ifdef USE_ONESHOT125
            // OneShot125: 125µs pulses
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
            // Standard PWM: 1000µs pulses
            analogWriteFrequency(MOTOR_PIN_1, 250);
            analogWriteResolution(12);
            
            // 1000µs / 4000µs period = 0.25 duty cycle
            int pwm_value = (1000.0 / 4000.0) * 4095;
            
            analogWrite(MOTOR_PIN_1, pwm_value);
            analogWrite(MOTOR_PIN_2, pwm_value);
            analogWrite(MOTOR_PIN_3, pwm_value);
            analogWrite(MOTOR_PIN_4, pwm_value);
            analogWrite(MOTOR_PIN_5, pwm_value);
            analogWrite(MOTOR_PIN_6, pwm_value);
            delay(20);
        #endif
        
        // Progress indicator
        if (i % 25 == 0) {
            Serial.print(".");
        }
    }
    Serial.println();
    Serial.println(F("ESCs armed"));
}

void commandMotors() {
    #ifdef USE_ONESHOT125
        // OneShot125 protocol: 125-250µs pulses via bit-banging
        int wentLow = 0;
        int pulseStart, timer;
        int flagM1 = 0, flagM2 = 0, flagM3 = 0;
        int flagM4 = 0, flagM5 = 0, flagM6 = 0;
        
        // Write all motor pins HIGH
        digitalWrite(MOTOR_PIN_1, HIGH);
        digitalWrite(MOTOR_PIN_2, HIGH);
        digitalWrite(MOTOR_PIN_3, HIGH);
        digitalWrite(MOTOR_PIN_4, HIGH);
        digitalWrite(MOTOR_PIN_5, HIGH);
        digitalWrite(MOTOR_PIN_6, HIGH);
        pulseStart = micros();

        // Write each pin LOW as its pulse length is reached
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
        // Standard PWM protocol: 1000-2000µs via analogWrite
        // Frequency: 250Hz (4000µs period)
        // Resolution: 12-bit (0-4095)
        analogWriteFrequency(MOTOR_PIN_1, 250);
        analogWriteResolution(12);
        
        analogWrite(MOTOR_PIN_1, (m1_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_2, (m2_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_3, (m3_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_4, (m4_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_5, (m5_command_PWM / 4000.0) * 4095);
        analogWrite(MOTOR_PIN_6, (m6_command_PWM / 4000.0) * 4095);
    #endif
    
    // Servos: Convert microseconds (1000-2000µs) to degrees (0-180°)
    servo1.write((s1_command_PWM - 1000) * 0.18);
    servo2.write((s2_command_PWM - 1000) * 0.18);
    servo3.write((s3_command_PWM - 1000) * 0.18);
    servo4.write((s4_command_PWM - 1000) * 0.18);
    servo5.write((s5_command_PWM - 1000) * 0.18);
    servo6.write((s6_command_PWM - 1000) * 0.18);
    servo7.write((s7_command_PWM - 1000) * 0.18);
}

void loopRate(int freq) {
    float invFreq = 1.0 / freq * 1000000.0;
    unsigned long checker = micros();
    
    while (invFreq > (checker - current_time)) {
        checker = micros();
    }
}

//========================================================================================================================//
//                                            DEBUG PRINT FUNCTIONS                                                       //
//========================================================================================================================//
// These functions print data to serial monitor/plotter
// They DO NOT affect flight controller operation
// Uncomment function calls in loop() to use

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
        Serial.print(F("dt(μs):")); Serial.println(dt * 1000000.0);
    }
}

//========================================================================================================================//
//                                              HELPER FUNCTIONS                                                          //
//========================================================================================================================//

void setupBlink(int num_blinks, int blink_delay) {
    for (int i = 0; i < num_blinks; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(blink_delay);
        digitalWrite(LED_PIN, LOW);
        delay(blink_delay);
    }
}

float invSqrt(float x) {
    return 1.0 / sqrtf(x);
}