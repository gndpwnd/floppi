/*
 * Barometer Module Implementation  (USE_BAROMETER — telemetry-only)
 *
 * Self-contained I2C barometer driver + dedicated Core-1 FreeRTOS task.
 *
 * SCOPE: telemetry-only. The barometer is polled on ESP32 Core 1 and its
 * pressure / temperature / relative-altitude readings are published into a
 * spinlock-guarded snapshot that the swarm-API serializer reads. It NEVER
 * feeds the Core-0 1 kHz flight loop. Altitude logic is flight-computer
 * territory (scope.md; barometer_integration_spec_2026-05-20.md §3 Option A).
 *
 * SENSOR: BMP280 is the implemented default — cheapest, most common, adequate
 * for a telemetry-grade vertical readout (baro spec §2). BMP388 and MS5611 are
 * configurable via BAROMETER_TYPE; if selected they currently fall back to a
 * graceful no-op driver (begin() returns false) rather than breaking the build
 * — a full driver for those parts is a follow-up workstream.
 *
 * I2C BUS: the primary `Wire` bus (GPIO 21/22), shared with the MPU6050 IMU.
 * The baro sits at 0x76/0x77, the IMU at 0x68 — no address clash, they coexist.
 * The ESP32 Arduino I2C HAL serialises every transaction on a per-bus mutex, so
 * the Core-1 baro read and the Core-0 IMU read cannot corrupt each other; the
 * baro just waits a few ms if it collides with an IMU tick. This deliberately
 * avoids contradiction C-1 (Wire1's GPIO 25/26 defaults collide with
 * MOTOR_PIN_1/2) — by staying on `Wire` the baro touches no motor pin at all.
 * See docs/findings/phase_w2_barometer_landed_2026-05-20.md.
 *
 * The whole file is gated on USE_BAROMETER — zero bytes when the flag is off.
 */

#include "config.h"

#ifdef USE_BAROMETER

#include "barometer.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

//=============================================================================
// BMP280 register map  (also valid for the temperature/pressure subset used)
//=============================================================================
#define BMP280_REG_CHIPID    0xD0
#define BMP280_REG_RESET     0xE0
#define BMP280_REG_STATUS    0xF3
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG    0xF5
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_CALIB     0x88   // 0x88..0xA1 — 24 bytes of trim data

#define BMP280_CHIPID_A      0x58   // BMP280
#define BMP280_CHIPID_B      0x56   // BMP280 (sample silicon)
#define BMP280_CHIPID_C      0x57   // BMP280 (sample silicon)
#define BME280_CHIPID        0x60   // BME280 — register-compatible for P+T

//=============================================================================
// BMP388 register map  (Bosch BMP3 datasheet — DATASHEET-REVIEW the values
// below against the BMP388/BMP390 datasheet rev 1.x if exact silicon differs)
//=============================================================================
#define BMP388_REG_CHIPID    0x00
#define BMP388_REG_ERR       0x02
#define BMP388_REG_STATUS    0x03
#define BMP388_REG_DATA      0x04   // 0x04..0x09 — press[3] then temp[3]
#define BMP388_REG_PWR_CTRL  0x1B
#define BMP388_REG_OSR       0x1C
#define BMP388_REG_ODR       0x1D
#define BMP388_REG_CONFIG    0x1F   // IIR filter
#define BMP388_REG_CALIB     0x31   // 0x31..0x45 — 21 bytes of NVM trim
#define BMP388_REG_CMD       0x7E

#define BMP388_CHIPID        0x50   // BMP388
#define BMP390_CHIPID        0x60   // BMP390 (register-compatible for P+T)
#define BMP388_CMD_SOFTRESET 0xB6

//=============================================================================
// MS5611 command set  (MS5611-01BA datasheet — DATASHEET-REVIEW values below)
//=============================================================================
#define MS5611_CMD_RESET     0x1E
#define MS5611_CMD_CONV_D1   0x40   // pressure conversion, OSR 256 base
#define MS5611_CMD_CONV_D2   0x50   // temperature conversion, OSR 256 base
#define MS5611_CMD_ADC_READ  0x00
#define MS5611_CMD_PROM_READ 0xA0   // 0xA0..0xAE — 8 PROM words
// OSR field is added to the base CONV command: +0/2/4/6/8 -> OSR 256..4096.
#define MS5611_OSR           0x08   // OSR 4096 — best resolution, ~9.04 ms
#define MS5611_CONV_DELAY_MS 10     // >= 9.04 ms max conversion time @ OSR4096

//=============================================================================
// Globals — the I2C address resolves from BARO_I2C_ADDRESS in config.h
//=============================================================================
static const uint8_t kBaroAddr = BARO_I2C_ADDRESS;

//-----------------------------------------------------------------------------
// Low-level I2C helpers (operate on the primary `Wire` bus)
//-----------------------------------------------------------------------------
static bool baroWrite8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(kBaroAddr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool baroReadBytes(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(kBaroAddr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;   // repeated start
    uint8_t got = Wire.requestFrom((int)kBaroAddr, (int)len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

#if defined(BAROMETER_MS5611)
//-----------------------------------------------------------------------------
// MS5611 helpers — the part has no register map; it is driven by raw single-
// byte commands (datasheet §Commands). RESET / CONVERT are write-only; PROM and
// ADC are read after addressing the command byte.
//-----------------------------------------------------------------------------
static bool ms5611Cmd(uint8_t cmd) {
    Wire.beginTransmission(kBaroAddr);
    Wire.write(cmd);
    return Wire.endTransmission() == 0;
}

// Read a 16-bit PROM word at the given PROM-read command address.
static bool ms5611ReadProm(uint8_t cmd, uint16_t* out) {
    Wire.beginTransmission(kBaroAddr);
    Wire.write(cmd);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)kBaroAddr, 2) != 2) return false;
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    *out = ((uint16_t)hi << 8) | lo;
    return true;
}

// Read the 24-bit ADC result of the last triggered conversion.
static bool ms5611ReadAdc(uint32_t* out) {
    Wire.beginTransmission(kBaroAddr);
    Wire.write((uint8_t)MS5611_CMD_ADC_READ);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)kBaroAddr, 3) != 3) return false;
    uint32_t b2 = Wire.read();
    uint32_t b1 = Wire.read();
    uint32_t b0 = Wire.read();
    *out = (b2 << 16) | (b1 << 8) | b0;
    return true;
}

// CRC-4 over the 8 PROM words (datasheet AN520 reference implementation).
static uint8_t ms5611Crc4(uint16_t* prom) {
    uint16_t rem = 0;
    uint16_t saved = prom[7];          // CRC byte sits in the low nibble of [7]
    prom[7] &= 0xFF00;                 // CRC nibble must be cleared for the calc
    for (int i = 0; i < 16; i++) {
        if (i & 1) rem ^= (uint16_t)(prom[i >> 1] & 0x00FF);
        else       rem ^= (uint16_t)(prom[i >> 1] >> 8);
        for (int bit = 8; bit > 0; bit--) {
            if (rem & 0x8000) rem = (rem << 1) ^ 0x3000;
            else              rem = (rem << 1);
        }
    }
    prom[7] = saved;                   // restore
    return (uint8_t)((rem >> 12) & 0x0F);
}
#endif // BAROMETER_MS5611

//=============================================================================
// Barometer class
//=============================================================================
Barometer::Barometer()
    : _present(false),
      _filter_primed(false),
      _pressure_pa(0.0f),
      _temp_c(0.0f),
      _altitude_m(0.0f),
      _sea_level_pa(BARO_SEA_LEVEL_PA),
      _dig_t1(0), _dig_t2(0), _dig_t3(0),
      _dig_p1(0), _dig_p2(0), _dig_p3(0), _dig_p4(0), _dig_p5(0),
      _dig_p6(0), _dig_p7(0), _dig_p8(0), _dig_p9(0),
      _t_fine(0)
#if defined(BAROMETER_BMP388)
    , _b3_t1(0), _b3_t2(0), _b3_t3(0),
      _b3_p1(0), _b3_p2(0), _b3_p3(0), _b3_p4(0), _b3_p5(0), _b3_p6(0),
      _b3_p7(0), _b3_p8(0), _b3_p9(0), _b3_p10(0), _b3_p11(0),
      _b3_t_lin(0.0f)
#endif
#if defined(BAROMETER_MS5611)
    , _ms_c1(0), _ms_c2(0), _ms_c3(0), _ms_c4(0), _ms_c5(0), _ms_c6(0)
#endif
{}

bool Barometer::begin() {
#if defined(BAROMETER_BMP280)
    // Read chip ID — confirms a BMP280/BME280 is on the bus.
    uint8_t id = 0;
    if (!baroReadBytes(BMP280_REG_CHIPID, &id, 1)) {
        _present = false;
        return false;
    }
    if (id != BMP280_CHIPID_A && id != BMP280_CHIPID_B &&
        id != BMP280_CHIPID_C && id != BME280_CHIPID) {
        _present = false;
        return false;
    }

    // Soft reset, then wait for the sensor to settle.
    baroWrite8(BMP280_REG_RESET, 0xB6);
    delay(5);

    // Read the 24-byte factory trim block (little-endian).
    uint8_t c[24];
    if (!baroReadBytes(BMP280_REG_CALIB, c, 24)) {
        _present = false;
        return false;
    }
    _dig_t1 = (uint16_t)(c[1]  << 8 | c[0]);
    _dig_t2 = (int16_t) (c[3]  << 8 | c[2]);
    _dig_t3 = (int16_t) (c[5]  << 8 | c[4]);
    _dig_p1 = (uint16_t)(c[7]  << 8 | c[6]);
    _dig_p2 = (int16_t) (c[9]  << 8 | c[8]);
    _dig_p3 = (int16_t) (c[11] << 8 | c[10]);
    _dig_p4 = (int16_t) (c[13] << 8 | c[12]);
    _dig_p5 = (int16_t) (c[15] << 8 | c[14]);
    _dig_p6 = (int16_t) (c[17] << 8 | c[16]);
    _dig_p7 = (int16_t) (c[19] << 8 | c[18]);
    _dig_p8 = (int16_t) (c[21] << 8 | c[20]);
    _dig_p9 = (int16_t) (c[23] << 8 | c[22]);

    // CONFIG: standby 0.5 ms, IIR filter coeff 4 (telemetry-grade smoothing).
    baroWrite8(BMP280_REG_CONFIG, (0x00 << 5) | (0x04 << 2));
    // CTRL_MEAS: temp oversample x2, pressure oversample x16, normal mode.
    baroWrite8(BMP280_REG_CTRL_MEAS, (0x02 << 5) | (0x05 << 2) | 0x03);
    delay(10);

    _present = true;
    return true;

#elif defined(BAROMETER_BMP388)
    // --- BMP388 driver (Bosch BMP3 datasheet, float compensation) ---
    uint8_t id = 0;
    if (!baroReadBytes(BMP388_REG_CHIPID, &id, 1)) {
        _present = false;
        return false;
    }
    if (id != BMP388_CHIPID && id != BMP390_CHIPID) {
        _present = false;
        return false;
    }

    // Soft reset via the CMD register, then wait for the part to settle.
    baroWrite8(BMP388_REG_CMD, BMP388_CMD_SOFTRESET);
    delay(10);

    // Read the 21-byte NVM trim block (datasheet §3.11, little-endian).
    uint8_t c[21];
    if (!baroReadBytes(BMP388_REG_CALIB, c, 21)) {
        _present = false;
        return false;
    }
    uint16_t nvm_t1  = (uint16_t)(c[1]  << 8 | c[0]);
    uint16_t nvm_t2  = (uint16_t)(c[3]  << 8 | c[2]);
    int8_t   nvm_t3  = (int8_t)  c[4];
    int16_t  nvm_p1  = (int16_t) (c[6]  << 8 | c[5]);
    int16_t  nvm_p2  = (int16_t) (c[8]  << 8 | c[7]);
    int8_t   nvm_p3  = (int8_t)  c[9];
    int8_t   nvm_p4  = (int8_t)  c[10];
    uint16_t nvm_p5  = (uint16_t)(c[12] << 8 | c[11]);
    uint16_t nvm_p6  = (uint16_t)(c[14] << 8 | c[13]);
    int8_t   nvm_p7  = (int8_t)  c[15];
    int8_t   nvm_p8  = (int8_t)  c[16];
    int16_t  nvm_p9  = (int16_t) (c[18] << 8 | c[17]);
    int8_t   nvm_p10 = (int8_t)  c[19];
    int8_t   nvm_p11 = (int8_t)  c[20];

    // Scale the raw NVM values into float quantisation coefficients
    // (datasheet §9.1 — divisors are powers of two: 2^-8 = 0.00390625, etc.).
    _b3_t1  = (float)nvm_t1  / 0.00390625f;          // 2^-8
    _b3_t2  = (float)nvm_t2  / 1073741824.0f;        // 2^30
    _b3_t3  = (float)nvm_t3  / 281474976710656.0f;   // 2^48
    _b3_p1  = ((float)nvm_p1 - 16384.0f) / 1048576.0f;          // 2^14 / 2^20
    _b3_p2  = ((float)nvm_p2 - 16384.0f) / 536870912.0f;        // 2^14 / 2^29
    _b3_p3  = (float)nvm_p3  / 4294967296.0f;        // 2^32
    _b3_p4  = (float)nvm_p4  / 137438953472.0f;      // 2^37
    _b3_p5  = (float)nvm_p5  / 0.125f;               // 2^-3
    _b3_p6  = (float)nvm_p6  / 64.0f;                // 2^6
    _b3_p7  = (float)nvm_p7  / 256.0f;               // 2^8
    _b3_p8  = (float)nvm_p8  / 32768.0f;             // 2^15
    _b3_p9  = (float)nvm_p9  / 281474976710656.0f;   // 2^48
    _b3_p10 = (float)nvm_p10 / 281474976710656.0f;   // 2^48
    _b3_p11 = (float)nvm_p11 / 36893488147419103232.0f; // 2^65

    // OSR: pressure x8 (0x03), temperature x1 (0x00) — telemetry-grade.
    baroWrite8(BMP388_REG_OSR, (0x00 << 3) | 0x03);
    // ODR: 0x02 -> 50 Hz subdivision, comfortably above the 20 Hz poll.
    baroWrite8(BMP388_REG_ODR, 0x02);
    // CONFIG: IIR filter coefficient 3 (bits 3:1) — light telemetry smoothing.
    baroWrite8(BMP388_REG_CONFIG, (0x02 << 1));
    // PWR_CTRL: enable pressure (bit0) + temperature (bit1), normal mode (0x30).
    baroWrite8(BMP388_REG_PWR_CTRL, 0x33);
    delay(10);

    _present = true;
    return true;

#elif defined(BAROMETER_MS5611)
    // --- MS5611 driver (MS5611-01BA datasheet) ---
    // Reset loads the factory PROM coefficients into the device registers.
    if (!ms5611Cmd(MS5611_CMD_RESET)) {
        _present = false;
        return false;
    }
    delay(5);   // datasheet: reload sequence completes in <= 2.8 ms

    // Read all 8 PROM words: [0] factory/setup, [1..6] = C1..C6, [7] = CRC.
    uint16_t prom[8];
    for (uint8_t i = 0; i < 8; i++) {
        if (!ms5611ReadProm(MS5611_CMD_PROM_READ + (i << 1), &prom[i])) {
            _present = false;
            return false;
        }
    }

    // A fully-zero or all-0xFFFF PROM means no device / bad read.
    bool all_zero = true, all_ff = true;
    for (uint8_t i = 0; i < 8; i++) {
        if (prom[i] != 0x0000) all_zero = false;
        if (prom[i] != 0xFFFF) all_ff = false;
    }
    if (all_zero || all_ff) {
        _present = false;
        return false;
    }

    // CRC-4 check over the PROM (datasheet AN520). Reject a corrupt PROM.
    uint8_t crc_read = (uint8_t)(prom[7] & 0x000F);
    if (ms5611Crc4(prom) != crc_read) {
        _present = false;
        return false;
    }

    _ms_c1 = prom[1];   // pressure sensitivity        | SENS_T1
    _ms_c2 = prom[2];   // pressure offset             | OFF_T1
    _ms_c3 = prom[3];   // temp coeff of pressure sens | TCS
    _ms_c4 = prom[4];   // temp coeff of pressure off  | TCO
    _ms_c5 = prom[5];   // reference temperature       | T_REF
    _ms_c6 = prom[6];   // temp coeff of the temperature

    _present = true;
    return true;

#else
    // No sensor selected — fail cleanly so the build still succeeds and
    // telemetry simply reports the sensor as absent.
    _present = false;
    return false;
#endif
}

bool Barometer::readChip() {
#if defined(BAROMETER_BMP280)
    // Burst-read pressure (3 bytes) + temperature (3 bytes) from 0xF7.
    uint8_t d[6];
    if (!baroReadBytes(BMP280_REG_PRESS_MSB, d, 6)) return false;

    int32_t adc_p = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    int32_t adc_t = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);

    // --- Temperature compensation (Bosch BMP280 datasheet, integer form) ---
    int32_t var1, var2;
    var1 = ((((adc_t >> 3) - ((int32_t)_dig_t1 << 1))) * ((int32_t)_dig_t2)) >> 11;
    var2 = (((((adc_t >> 4) - ((int32_t)_dig_t1)) *
             ((adc_t >> 4) - ((int32_t)_dig_t1))) >> 12) *
            ((int32_t)_dig_t3)) >> 14;
    _t_fine = var1 + var2;
    int32_t t = (_t_fine * 5 + 128) >> 8;     // temperature in 0.01 deg C
    _temp_c = t / 100.0f;

    // --- Pressure compensation (64-bit integer form) ---
    int64_t p1, p2, p;
    p1 = ((int64_t)_t_fine) - 128000;
    p2 = p1 * p1 * (int64_t)_dig_p6;
    p2 = p2 + ((p1 * (int64_t)_dig_p5) << 17);
    p2 = p2 + (((int64_t)_dig_p4) << 35);
    p1 = ((p1 * p1 * (int64_t)_dig_p3) >> 8) +
         ((p1 * (int64_t)_dig_p2) << 12);
    p1 = (((((int64_t)1) << 47) + p1)) * ((int64_t)_dig_p1) >> 33;
    if (p1 == 0) return false;                 // avoid divide-by-zero
    p = 1048576 - adc_p;
    p = (((p << 31) - p2) * 3125) / p1;
    p1 = (((int64_t)_dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    p2 = (((int64_t)_dig_p8) * p) >> 19;
    p = ((p + p1 + p2) >> 8) + (((int64_t)_dig_p7) << 4);

    _pressure_pa = (float)p / 256.0f;          // Q24.8 -> pascals
    return true;

#elif defined(BAROMETER_BMP388)
    // Burst-read pressure (3 bytes) + temperature (3 bytes) from 0x04.
    uint8_t d[6];
    if (!baroReadBytes(BMP388_REG_DATA, d, 6)) return false;

    // Both fields are 24-bit, little-endian (LSB first).
    uint32_t adc_p = ((uint32_t)d[2] << 16) | ((uint32_t)d[1] << 8) | d[0];
    uint32_t adc_t = ((uint32_t)d[5] << 16) | ((uint32_t)d[4] << 8) | d[3];

    // --- Temperature compensation (Bosch BMP3 datasheet §9.2, float form) ---
    float pd1 = (float)adc_t - _b3_t1;
    float pd2 = pd1 * _b3_t2;
    _b3_t_lin = pd2 + (pd1 * pd1) * _b3_t3;
    _temp_c = _b3_t_lin;

    // --- Pressure compensation (Bosch BMP3 datasheet §9.3, float form) ---
    float t   = _b3_t_lin;
    float po1, po2, po3;
    po1 = _b3_p6 * t;
    po2 = _b3_p7 * (t * t);
    po3 = _b3_p8 * (t * t * t);
    float out1 = _b3_p5 + po1 + po2 + po3;

    po1 = _b3_p2 * t;
    po2 = _b3_p3 * (t * t);
    po3 = _b3_p4 * (t * t * t);
    float out2 = (float)adc_p * (_b3_p1 + po1 + po2 + po3);

    po1 = (float)adc_p * (float)adc_p;
    po2 = _b3_p9 + _b3_p10 * t;
    po3 = po1 * po2;
    float out3 = po3 + (po1 * (float)adc_p) * _b3_p11;

    _pressure_pa = out1 + out2 + out3;          // pascals
    return true;

#elif defined(BAROMETER_MS5611)
    // MS5611 is a request/read state machine: trigger D1 (pressure), wait the
    // full conversion time, read the ADC; repeat for D2 (temperature).
    if (!ms5611Cmd(MS5611_CMD_CONV_D1 + MS5611_OSR)) return false;
    delay(MS5611_CONV_DELAY_MS);
    uint32_t d1 = 0;
    if (!ms5611ReadAdc(&d1)) return false;

    if (!ms5611Cmd(MS5611_CMD_CONV_D2 + MS5611_OSR)) return false;
    delay(MS5611_CONV_DELAY_MS);
    uint32_t d2 = 0;
    if (!ms5611ReadAdc(&d2)) return false;

    if (d1 == 0 || d2 == 0) return false;       // conversion not ready / bad read

    // --- First-order compensation (MS5611-01BA datasheet §PRESSURE/TEMP) ---
    int32_t dT   = (int32_t)d2 - ((int32_t)_ms_c5 << 8);
    int32_t temp = 2000 + (int32_t)(((int64_t)dT * _ms_c6) >> 23);

    int64_t off  = ((int64_t)_ms_c2 << 16) + (((int64_t)_ms_c4 * dT) >> 7);
    int64_t sens = ((int64_t)_ms_c1 << 15) + (((int64_t)_ms_c3 * dT) >> 8);

    // Second-order temperature compensation (datasheet, improves low-temp).
    if (temp < 2000) {
        int64_t t2    = ((int64_t)dT * dT) >> 31;
        int64_t toff  = (5 * (int64_t)(temp - 2000) * (temp - 2000)) >> 1;
        int64_t tsens = (5 * (int64_t)(temp - 2000) * (temp - 2000)) >> 2;
        if (temp < -1500) {
            toff  += 7 * (int64_t)(temp + 1500) * (temp + 1500);
            tsens += (11 * (int64_t)(temp + 1500) * (temp + 1500)) >> 1;
        }
        temp -= (int32_t)t2;
        off  -= toff;
        sens -= tsens;
    }

    int64_t p = (((int64_t)d1 * sens) >> 21) - off;
    p >>= 15;                                   // pressure in 0.01 mbar (Pa)

    _temp_c      = temp / 100.0f;               // 0.01 degC -> degC
    _pressure_pa = (float)p;                    // already in pascals
    return true;

#else
    return false;
#endif
}

bool Barometer::read() {
    if (!_present) return false;
    if (!readChip()) return false;

    // P3 robustness guard (security audit): a runtime sea-level calibration
    // that sets _sea_level_pa to <= 0 would make the division below produce
    // inf/NaN and poison the filtered altitude. Reject it instead — the last
    // good _altitude_m is retained and the read reports failure.
    if (_sea_level_pa <= 0.0f) return false;

    // Convert pressure to relative altitude via the international barometric
    // formula, referenced to the calibrated sea-level pressure.
    float raw_alt = 44330.0f *
        (1.0f - powf(_pressure_pa / _sea_level_pa, 0.1903f));

    // PT1 low-pass filter (same convention as B_ACCEL / B_GYRO).
    if (!_filter_primed) {
        _altitude_m = raw_alt;
        _filter_primed = true;
    } else {
        _altitude_m = (1.0f - BARO_LPF) * _altitude_m + BARO_LPF * raw_alt;
    }
    return true;
}

//=============================================================================
// Core-1 task + spinlock-guarded telemetry snapshot
//=============================================================================
// The task owns the Barometer instance. It publishes each reading into the
// snapshot below; the swarm-API serializer reads the snapshot under the same
// spinlock. The flight loop (Core 0) never touches any of this.

static Barometer        s_baro;
static TaskHandle_t     s_baro_task_handle = NULL;
static portMUX_TYPE     s_baro_mux = portMUX_INITIALIZER_UNLOCKED;

static struct {
    bool  ok;
    float pressure_pa;
    float temp_c;
    float altitude_m;
} s_baro_snapshot = { false, 0.0f, 0.0f, 0.0f };

static void baroTask(void* parameter) {
    (void)parameter;

    // begin() runs inside the task so a slow/absent sensor never delays
    // setup() / setupWebServer() on the main path.
    bool ok = s_baro.begin();
    if (ok) {
        Serial.printf("[Baro] %s detected at 0x%02X on Wire — telemetry-only\n",
#if defined(BAROMETER_BMP280)
                      "BMP280",
#elif defined(BAROMETER_BMP388)
                      "BMP388",
#elif defined(BAROMETER_MS5611)
                      "MS5611",
#else
                      "barometer",
#endif
                      (unsigned)BARO_I2C_ADDRESS);
    } else {
        Serial.println(F("[Baro] sensor not found — telemetry will report not-ok"));
    }

    const TickType_t period = pdMS_TO_TICKS(1000 / BARO_SAMPLE_RATE_HZ);
    for (;;) {
        if (ok && s_baro.read()) {
            portENTER_CRITICAL(&s_baro_mux);
            s_baro_snapshot.ok          = true;
            s_baro_snapshot.pressure_pa = s_baro.pressurePa();
            s_baro_snapshot.temp_c      = s_baro.temperatureC();
            s_baro_snapshot.altitude_m  = s_baro.altitudeM();
            portEXIT_CRITICAL(&s_baro_mux);
        }
        vTaskDelay(period);
    }
}

void startBarometerTask() {
    // Core 1, priority 1, 3072 B stack — per the Core-1 budget analysis
    // (fc_core1_budget_2026-05-20.md §7). A dedicated task gives the baro a
    // POST-immune cadence the loop()-slice form the spec §7 proposed cannot.
    xTaskCreatePinnedToCore(
        baroTask,
        "Baro",
        3072,
        NULL,
        1,                      // priority 1 — equal to the Arduino loopTask
        &s_baro_task_handle,
        1                       // Core 1
    );
}

bool baroTelemetryOk() {
    portENTER_CRITICAL(&s_baro_mux);
    bool v = s_baro_snapshot.ok;
    portEXIT_CRITICAL(&s_baro_mux);
    return v;
}

float baroTelemetryPressurePa() {
    portENTER_CRITICAL(&s_baro_mux);
    float v = s_baro_snapshot.pressure_pa;
    portEXIT_CRITICAL(&s_baro_mux);
    return v;
}

float baroTelemetryTemperatureC() {
    portENTER_CRITICAL(&s_baro_mux);
    float v = s_baro_snapshot.temp_c;
    portEXIT_CRITICAL(&s_baro_mux);
    return v;
}

float baroTelemetryAltitudeM() {
    portENTER_CRITICAL(&s_baro_mux);
    float v = s_baro_snapshot.altitude_m;
    portEXIT_CRITICAL(&s_baro_mux);
    return v;
}

#endif // USE_BAROMETER
