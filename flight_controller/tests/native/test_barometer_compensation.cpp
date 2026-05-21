/**
 * test_barometer_compensation.cpp — native unit tests for the barometer
 * pressure/temperature compensation math.
 * =============================================================================
 *
 * Scope: the PURE fixed-point / float compensation arithmetic inside
 * src/barometer.cpp for the three supported sensors:
 *
 *   - BMP280  : Bosch classic integer (fixed-point) compensation
 *   - BMP388  : Bosch BMP3 datasheet §9.2/§9.3 float compensation
 *   - MS5611  : MS5611-01BA first-order + second-order temperature
 *               compensation, plus the AN520 CRC-4 over the PROM
 *   - altitude: the international barometric formula in Barometer::read()
 *
 * Per the native-test convention (tests/native/README.md) this file is
 * STANDALONE: the pure arithmetic from src/barometer.cpp is replicated below
 * as local functions — IDENTICAL to the source — so the test validates the
 * exact algorithm the firmware ships. NO <Arduino.h>, NO <Wire.h>, NO src/
 * linkage. If a replica diverges from the source, fix the replica (or report
 * a real source bug) — do not loosen the assertions.
 * =============================================================================
 */
#include "test_helpers.h"
#include <cstdint>

// ============================================================================
// BMP280 — replica of the integer compensation from barometer.cpp readChip()
// (lines ~367-393). Bundled into one struct so the calibration set is explicit.
// ============================================================================
struct Bmp280Calib {
    uint16_t dig_t1;
    int16_t  dig_t2, dig_t3;
    uint16_t dig_p1;
    int16_t  dig_p2, dig_p3, dig_p4, dig_p5, dig_p6, dig_p7, dig_p8, dig_p9;
};

// Returns t_fine; writes the 0.01-degC temperature into *temp_centi.
static int32_t bmp280_temp(const Bmp280Calib& c, int32_t adc_t,
                           int32_t* temp_centi) {
    int32_t var1, var2;
    var1 = ((((adc_t >> 3) - ((int32_t)c.dig_t1 << 1))) * ((int32_t)c.dig_t2)) >> 11;
    var2 = (((((adc_t >> 4) - ((int32_t)c.dig_t1)) *
             ((adc_t >> 4) - ((int32_t)c.dig_t1))) >> 12) *
            ((int32_t)c.dig_t3)) >> 14;
    int32_t t_fine = var1 + var2;
    *temp_centi = (t_fine * 5 + 128) >> 8;   // temperature in 0.01 deg C
    return t_fine;
}

// Returns compensated pressure in pascals (Q24.8 -> float), or -1.0 on the
// p1==0 divide-by-zero guard path (the source returns false there).
static double bmp280_pressure(const Bmp280Calib& c, int32_t t_fine,
                              int32_t adc_p) {
    int64_t p1, p2, p;
    p1 = ((int64_t)t_fine) - 128000;
    p2 = p1 * p1 * (int64_t)c.dig_p6;
    p2 = p2 + ((p1 * (int64_t)c.dig_p5) << 17);
    p2 = p2 + (((int64_t)c.dig_p4) << 35);
    p1 = ((p1 * p1 * (int64_t)c.dig_p3) >> 8) +
         ((p1 * (int64_t)c.dig_p2) << 12);
    p1 = (((((int64_t)1) << 47) + p1)) * ((int64_t)c.dig_p1) >> 33;
    if (p1 == 0) return -1.0;                 // avoid divide-by-zero
    p = 1048576 - adc_p;
    p = (((p << 31) - p2) * 3125) / p1;
    p1 = (((int64_t)c.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    p2 = (((int64_t)c.dig_p8) * p) >> 19;
    p = ((p + p1 + p2) >> 8) + (((int64_t)c.dig_p7) << 4);
    return (double)p / 256.0;                 // Q24.8 -> pascals
}

// ============================================================================
// BMP388 — replica of the float compensation from barometer.cpp.
// The struct holds the SCALED quantisation coefficients (post §9.1 scaling);
// bmp388_scale() reproduces that NVM->float scaling from begin().
// ============================================================================
struct Bmp388Calib {
    float t1, t2, t3;
    float p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11;
    float t_lin;   // shared linearised temperature term (like _t_fine)
};

// Replica of the §9.1 NVM->float scaling in begin().
static Bmp388Calib bmp388_scale(uint16_t nvm_t1, uint16_t nvm_t2, int8_t nvm_t3,
                                int16_t nvm_p1, int16_t nvm_p2, int8_t nvm_p3,
                                int8_t nvm_p4, uint16_t nvm_p5, uint16_t nvm_p6,
                                int8_t nvm_p7, int8_t nvm_p8, int16_t nvm_p9,
                                int8_t nvm_p10, int8_t nvm_p11) {
    Bmp388Calib c;
    c.t1  = (float)nvm_t1  / 0.00390625f;          // 2^-8
    c.t2  = (float)nvm_t2  / 1073741824.0f;        // 2^30
    c.t3  = (float)nvm_t3  / 281474976710656.0f;   // 2^48
    c.p1  = ((float)nvm_p1 - 16384.0f) / 1048576.0f;          // 2^14 / 2^20
    c.p2  = ((float)nvm_p2 - 16384.0f) / 536870912.0f;        // 2^14 / 2^29
    c.p3  = (float)nvm_p3  / 4294967296.0f;        // 2^32
    c.p4  = (float)nvm_p4  / 137438953472.0f;      // 2^37
    c.p5  = (float)nvm_p5  / 0.125f;               // 2^-3
    c.p6  = (float)nvm_p6  / 64.0f;                // 2^6
    c.p7  = (float)nvm_p7  / 256.0f;               // 2^8
    c.p8  = (float)nvm_p8  / 32768.0f;             // 2^15
    c.p9  = (float)nvm_p9  / 281474976710656.0f;   // 2^48
    c.p10 = (float)nvm_p10 / 281474976710656.0f;   // 2^48
    c.p11 = (float)nvm_p11 / 36893488147419103232.0f; // 2^65
    c.t_lin = 0.0f;
    return c;
}

// Replica of the §9.2 temperature compensation. Writes t_lin into c.
static float bmp388_temp(Bmp388Calib& c, uint32_t adc_t) {
    float pd1 = (float)adc_t - c.t1;
    float pd2 = pd1 * c.t2;
    c.t_lin = pd2 + (pd1 * pd1) * c.t3;
    return c.t_lin;
}

// Replica of the §9.3 pressure compensation (uses c.t_lin from bmp388_temp).
static float bmp388_pressure(const Bmp388Calib& c, uint32_t adc_p) {
    float t = c.t_lin;
    float po1, po2, po3;
    po1 = c.p6 * t;
    po2 = c.p7 * (t * t);
    po3 = c.p8 * (t * t * t);
    float out1 = c.p5 + po1 + po2 + po3;

    po1 = c.p2 * t;
    po2 = c.p3 * (t * t);
    po3 = c.p4 * (t * t * t);
    float out2 = (float)adc_p * (c.p1 + po1 + po2 + po3);

    po1 = (float)adc_p * (float)adc_p;
    po2 = c.p9 + c.p10 * t;
    po3 = po1 * po2;
    float out3 = po3 + (po1 * (float)adc_p) * c.p11;

    return out1 + out2 + out3;   // pascals
}

// ============================================================================
// MS5611 — replica of the first/second-order compensation from barometer.cpp.
// ============================================================================
struct Ms5611Calib {
    uint16_t c1, c2, c3, c4, c5, c6;
};

// Writes 0.01-degC temperature into *temp_centi, returns pressure in Pa.
static int64_t ms5611_compensate(const Ms5611Calib& cal, uint32_t d1,
                                 uint32_t d2, int32_t* temp_centi) {
    int32_t dT   = (int32_t)d2 - ((int32_t)cal.c5 << 8);
    int32_t temp = 2000 + (int32_t)(((int64_t)dT * cal.c6) >> 23);

    int64_t off  = ((int64_t)cal.c2 << 16) + (((int64_t)cal.c4 * dT) >> 7);
    int64_t sens = ((int64_t)cal.c1 << 15) + (((int64_t)cal.c3 * dT) >> 8);

    // Second-order temperature compensation (improves low-temp accuracy).
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
    p >>= 15;
    *temp_centi = temp;
    return p;
}

// Replica of ms5611Crc4() — AN520 CRC-4 over the 8 PROM words.
static uint8_t ms5611_crc4(uint16_t* prom) {
    uint16_t rem = 0;
    uint16_t saved = prom[7];
    prom[7] &= 0xFF00;
    for (int i = 0; i < 16; i++) {
        if (i & 1) rem ^= (uint16_t)(prom[i >> 1] & 0x00FF);
        else       rem ^= (uint16_t)(prom[i >> 1] >> 8);
        for (int bit = 8; bit > 0; bit--) {
            if (rem & 0x8000) rem = (rem << 1) ^ 0x3000;
            else              rem = (rem << 1);
        }
    }
    prom[7] = saved;
    return (uint8_t)((rem >> 12) & 0x0F);
}

// ============================================================================
// Altitude — replica of the conversion in Barometer::read().
// Returns the special value -999999.0 to model the _sea_level_pa<=0 guard
// path (the source returns false there and never computes raw_alt).
// ============================================================================
static double baro_altitude(float pressure_pa, float sea_level_pa) {
    if (sea_level_pa <= 0.0f) return -999999.0;   // guard: read() returns false
    return 44330.0f * (1.0f - powf(pressure_pa / sea_level_pa, 0.1903f));
}

// ============================================================================
TEST_MAIN("barometer compensation math (BMP280 / BMP388 / MS5611)") {

    // ------------------------------------------------------------------------
    // BMP280 — Bosch datasheet worked example.
    // Calibration + raw adc from the BMP280 datasheet reference: the canonical
    // values produce ~25.08 degC and ~100653 Pa.
    // ------------------------------------------------------------------------
    Bmp280Calib b280 = {
        27504,          // dig_t1
        26435, -1000,   // dig_t2, dig_t3
        36477,          // dig_p1
        -10685, 3024, 2855, 140, -7, 15500, -14600, 6000  // dig_p2..p9
    };
    int32_t b280_adc_t = 519888;
    int32_t b280_adc_p = 415148;

    int32_t b280_t_centi = 0;
    int32_t b280_t_fine  = bmp280_temp(b280, b280_adc_t, &b280_t_centi);
    double  b280_temp_c  = b280_t_centi / 100.0;
    double  b280_p_pa    = bmp280_pressure(b280, b280_t_fine, b280_adc_p);

    CHECK_NEAR(b280_temp_c, 25.08, 0.01,
               "BMP280 datasheet example temperature ~= 25.08 degC");
    // Datasheet 64-bit reference result: t_fine = 128422.
    CHECK_EQ(b280_t_fine, 128422,
             "BMP280 datasheet example t_fine == 128422");
    CHECK_NEAR(b280_p_pa, 100653.0, 2.0,
               "BMP280 datasheet example pressure ~= 100653 Pa");
    CHECK_FINITE(b280_p_pa, "BMP280 pressure is finite");

    // Monotonicity: a lower pressure adc code yields a higher compensated Pa
    // (the (1048576 - adc_p) term is inversely related to adc_p).
    double b280_p_lo_adc = bmp280_pressure(b280, b280_t_fine, b280_adc_p - 5000);
    CHECK(b280_p_lo_adc > b280_p_pa,
          "BMP280 lower adc_p -> higher compensated pressure");

    // ------------------------------------------------------------------------
    // BMP388 — representative calibration + raw set. The Bosch float
    // compensation path has no public golden vector, so this is a constructed
    // (but physically reasoned) case: the NVM trim is chosen so the §9.1
    // scaling yields a plausible ~17 degC / ~104 kPa reading where the
    // adc_p-dependent term (out2) carries a meaningful, monotonic share.
    // Validation here = scaling fidelity + finiteness + plausibility +
    // monotonicity, NOT an exact datasheet number.
    // ------------------------------------------------------------------------
    Bmp388Calib b388 = bmp388_scale(
        /*t1*/ 27000, /*t2*/ 18500, /*t3*/ -10,
        /*p1*/ 24400, /*p2*/ -3100, /*p3*/  20, /*p4*/ -5,
        /*p5*/  6250, /*p6*/ 30800, /*p7*/ -12, /*p8*/  9,
        /*p9*/    12, /*p10*/  1,   /*p11*/ -6);

    // Raw 24-bit codes — picked so the linearised temperature lands in range.
    uint32_t b388_adc_t = 7900000u;
    uint32_t b388_adc_p = 6500000u;

    float b388_temp_c = bmp388_temp(b388, b388_adc_t);
    float b388_p_pa   = bmp388_pressure(b388, b388_adc_p);

    CHECK_FINITE(b388_temp_c, "BMP388 temperature is finite");
    CHECK_FINITE(b388_p_pa,   "BMP388 pressure is finite");
    CHECK(b388_temp_c > -40.0f && b388_temp_c < 85.0f,
          "BMP388 temperature physically plausible (-40..85 degC)");
    CHECK(b388_p_pa > 30000.0f && b388_p_pa < 110000.0f,
          "BMP388 pressure physically plausible (30000..110000 Pa)");

    // Monotonicity: higher raw pressure code -> higher compensated Pa
    // (out2's dominant term scales linearly with adc_p, p1 coeff > 0).
    float b388_p_hi = bmp388_pressure(b388, b388_adc_p + 200000u);
    CHECK(b388_p_hi > b388_p_pa,
          "BMP388 higher raw pressure -> higher compensated Pa");

    // Self-consistency: the scaling divisors are powers of two — t1 recovers
    // exactly (27000 / 2^-8 == 27000 * 256).
    CHECK_NEAR(b388.t1, 27000.0f * 256.0f, 1e-3,
               "BMP388 §9.1 t1 scaling == nvm_t1 * 256");

    // ------------------------------------------------------------------------
    // MS5611 — datasheet worked example (MS5611-01BA datasheet).
    // PROM C1..C6 and D1/D2 from the datasheet's "PRESSURE AND TEMPERATURE
    // CALCULATION" example -> TEMP = 2007 (20.07 degC), P = 100009 (1000.09 mbar).
    // ------------------------------------------------------------------------
    Ms5611Calib ms = {
        40127,   // C1 — pressure sensitivity
        36924,   // C2 — pressure offset
        23317,   // C3 — temp coeff of pressure sensitivity
        23282,   // C4 — temp coeff of pressure offset
        33464,   // C5 — reference temperature
        28312    // C6 — temp coeff of the temperature
    };
    uint32_t ms_d1 = 9085466;   // digital pressure value
    uint32_t ms_d2 = 8569150;   // digital temperature value

    int32_t ms_temp_centi = 0;
    int64_t ms_p = ms5611_compensate(ms, ms_d1, ms_d2, &ms_temp_centi);

    CHECK_EQ(ms_temp_centi, 2007,
             "MS5611 datasheet example TEMP == 2007 (20.07 degC)");
    CHECK_EQ((long)ms_p, 100009L,
             "MS5611 datasheet example pressure == 100009 Pa");

    // Second-order low-temperature branch: feed a cold D2 so TEMP < 2000 and
    // confirm the T2/OFF2/SENS2 correction activates (cold temp differs from
    // what the first-order path alone would give).
    uint32_t ms_d2_cold = 8100000u;   // smaller D2 -> negative dT -> cold
    int32_t ms_cold_centi = 0;
    int64_t ms_cold_p = ms5611_compensate(ms, ms_d1, ms_d2_cold, &ms_cold_centi);
    // First-order-only temperature for the same dT (no second-order T2 subtract).
    int32_t cold_dT = (int32_t)ms_d2_cold - ((int32_t)ms.c5 << 8);
    int32_t cold_first_order =
        2000 + (int32_t)(((int64_t)cold_dT * ms.c6) >> 23);
    CHECK(cold_first_order < 2000,
          "MS5611 cold D2 drives first-order TEMP below 2000 (branch entry)");
    CHECK(ms_cold_centi != cold_first_order,
          "MS5611 second-order branch alters cold TEMP (T2 correction active)");
    CHECK_FINITE((double)ms_cold_p, "MS5611 cold-branch pressure is finite");

    // ------------------------------------------------------------------------
    // MS5611 CRC-4 — known-good PROM, then a flipped bit.
    // PROM word [7] low nibble holds the factory CRC. We compute the CRC for a
    // sample PROM, store it, and assert the routine reproduces it; flipping a
    // bit elsewhere must break the match.
    // ------------------------------------------------------------------------
    uint16_t prom[8] = {
        0x1234, 40127, 36924, 23317, 23282, 33464, 28312, 0x0000
    };
    // Establish the "factory" CRC nibble for this PROM and write it into [7].
    uint8_t good_crc = ms5611_crc4(prom);
    prom[7] = (uint16_t)(prom[7] & 0xFF00) | good_crc;
    uint8_t crc_read = (uint8_t)(prom[7] & 0x000F);
    CHECK_EQ(ms5611_crc4(prom), crc_read,
             "MS5611 CRC-4 matches the stored nibble for a good PROM");

    // Flip one bit in C3 -> the CRC must no longer match the stored nibble.
    uint16_t prom_bad[8];
    for (int i = 0; i < 8; i++) prom_bad[i] = prom[i];
    prom_bad[3] ^= 0x0040;   // flip a bit in C3
    uint8_t bad_crc_read = (uint8_t)(prom_bad[7] & 0x000F);
    CHECK_NE(ms5611_crc4(prom_bad), bad_crc_read,
             "MS5611 CRC-4 detects a single-bit PROM corruption");

    // ------------------------------------------------------------------------
    // Altitude — barometric formula 44330 * (1 - (p/p0)^0.1903).
    // ------------------------------------------------------------------------
    const float sea_level = 101325.0f;

    double alt_at_sea = baro_altitude(sea_level, sea_level);
    CHECK_NEAR(alt_at_sea, 0.0, 1e-3,
               "altitude at sea-level pressure ~= 0 m");

    double alt_low_p = baro_altitude(95000.0f, sea_level);
    CHECK(alt_low_p > 0.0,
          "altitude: lower pressure than sea level -> positive altitude");
    CHECK_FINITE(alt_low_p, "altitude at reduced pressure is finite");

    // Monotonicity: still-lower pressure -> still-higher altitude.
    double alt_lower_p = baro_altitude(90000.0f, sea_level);
    CHECK(alt_lower_p > alt_low_p,
          "altitude: monotonic — lower pressure -> higher altitude");

    // Guard path: _sea_level_pa <= 0 must NOT produce NaN/Inf — the source
    // returns false before the division. Our replica models that with the
    // sentinel; assert it is finite and is the sentinel (no division ran).
    double alt_guard = baro_altitude(95000.0f, 0.0f);
    CHECK_FINITE(alt_guard, "altitude guard path (sea_level<=0) yields no NaN/Inf");
    CHECK_EQ(alt_guard, -999999.0,
             "altitude guard path: sea_level<=0 short-circuits before division");
}
