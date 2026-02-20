/*
 * IMU Module Implementation
 * IMU data acquisition and attitude estimation
 */

#include "imu.h"
#include "globals.h"
#include "config.h"
#include "pin_definitions.h"

#ifdef USE_OPTIMIZATION
#include "filters.h"
#endif

#include <Wire.h>

//========================================================================================================================//
//                                                  IMU HARDWARE                                                           //
//========================================================================================================================//

#ifdef USE_MPU6050
    #include "MPU6050.h"
    #include "I2Cdev.h"
    MPU6050 mpu6050;
#elif defined(USE_MPU9250)
    #include "MPU9250.h"
    MPU9250 mpu9250(SPI, 36);
#endif

//========================================================================================================================//
//                                                  IMU SETUP                                                              //
//========================================================================================================================//

void setupIMU() {
    #ifdef USE_MPU6050
        #ifdef USE_ESP32
            // ESP32: Specify I2C pins explicitly
            Wire.begin(IMU_SDA_PIN, IMU_SCL_PIN);
            Wire.setClock(400000);  // ESP32 typically uses 400kHz for I2C
        #else
            // Teensy: Use default I2C pins at 1MHz
            Wire.begin();
            Wire.setClock(1000000);
        #endif
        mpu6050.initialize();

        // Apply configured ranges from config.h.
        // mpu6050.initialize() hardcodes 250 DPS / 2G regardless of config,
        // so we must set the actual ranges explicitly after init.
        mpu6050.setFullScaleGyroRange(GYRO_SCALE);
        mpu6050.setFullScaleAccelRange(ACCEL_SCALE);

        if (mpu6050.testConnection()) {
            Serial.print(F("MPU6050 OK (gyro="));
            #if defined(GYRO_250DPS)
                Serial.print(F("250"));
            #elif defined(GYRO_500DPS)
                Serial.print(F("500"));
            #elif defined(GYRO_1000DPS)
                Serial.print(F("1000"));
            #elif defined(GYRO_2000DPS)
                Serial.print(F("2000"));
            #endif
            Serial.println(F("dps)"));
        } else {
            Serial.println(F("MPU6050 FAILED"));
        }
    #elif defined(USE_MPU9250)
        int status = mpu9250.begin();
        if (status < 0) {
            Serial.println(F("MPU9250 FAILED"));
        } else {
            Serial.println(F("MPU9250 OK"));
        }
        mpu9250.setAccelRange(MPU9250::ACCEL_RANGE_4G);
        mpu9250.setGyroRange(MPU9250::GYRO_RANGE_500DPS);
        mpu9250.setDlpfBandwidth(MPU9250::DLPF_BANDWIDTH_184HZ);
        mpu9250.setSrd(0);
    #endif
}

//========================================================================================================================//
//                                                 GET IMU DATA                                                            //
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

    // Apply calibration (offset then scale for 6-position calibration)
    AccX = (AccX - AccErrorX) * IMU_ACC_SCALE_X;
    AccY = (AccY - AccErrorY) * IMU_ACC_SCALE_Y;
    AccZ = (AccZ - AccErrorZ) * IMU_ACC_SCALE_Z;

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

    #ifdef USE_OPTIMIZATION
    // Biquad filters (2nd stage after PT1, steeper -12dB/octave rolloff)
    static BiquadFilter gyro_lpf[3];
    #if GYRO_NOTCH_CENTER_HZ > 0
    static BiquadFilter gyro_notch[3];
    #endif
    static bool opt_init = false;
    if (!opt_init) {
        for (int i = 0; i < 3; i++)
            biquad_init_lpf(&gyro_lpf[i], GYRO_LPF_CUTOFF_HZ, LOOP_FREQUENCY_HZ);
        #if GYRO_NOTCH_CENTER_HZ > 0
        for (int i = 0; i < 3; i++)
            biquad_init_notch(&gyro_notch[i], GYRO_NOTCH_CENTER_HZ, GYRO_NOTCH_WIDTH_HZ, LOOP_FREQUENCY_HZ);
        #endif
        opt_init = true;
    }

    // Gyro biquad low-pass
    GyroX = biquad_apply(&gyro_lpf[0], GyroX);
    GyroY = biquad_apply(&gyro_lpf[1], GyroY);
    GyroZ = biquad_apply(&gyro_lpf[2], GyroZ);

    // Gyro notch filter (targets motor/prop resonance)
    #if GYRO_NOTCH_CENTER_HZ > 0
    GyroX = biquad_apply(&gyro_notch[0], GyroX);
    GyroY = biquad_apply(&gyro_notch[1], GyroY);
    GyroZ = biquad_apply(&gyro_notch[2], GyroZ);
    #endif

    // Accelerometer 2nd stage low-pass (extra smoothing under vibration)
    static float AccX_s2 = 0, AccY_s2 = 0, AccZ_s2 = 0;
    AccX = (1.0f - B_ACCEL_STAGE2) * AccX_s2 + B_ACCEL_STAGE2 * AccX;
    AccY = (1.0f - B_ACCEL_STAGE2) * AccY_s2 + B_ACCEL_STAGE2 * AccY;
    AccZ = (1.0f - B_ACCEL_STAGE2) * AccZ_s2 + B_ACCEL_STAGE2 * AccZ;
    AccX_s2 = AccX;
    AccY_s2 = AccY;
    AccZ_s2 = AccZ;
    #endif // USE_OPTIMIZATION

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

//========================================================================================================================//
//                                              MADGWICK FILTER                                                            //
//========================================================================================================================//

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
    // 9DOF Madgwick filter (with magnetometer) - falls back to 6DOF for now
    Madgwick6DOF(gx, gy, gz, ax, ay, az, invSampleFreq);
}

//========================================================================================================================//
//                                                  UTILITIES                                                              //
//========================================================================================================================//

float invSqrt(float x) {
    return 1.0f / sqrtf(x);
}
