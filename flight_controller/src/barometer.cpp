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
      _t_fine(0) {}

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

#else
    // BAROMETER_BMP388 / BAROMETER_MS5611 — full drivers are a follow-up
    // workstream. Fail cleanly so the build still succeeds and telemetry
    // simply reports the sensor as absent.
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
#else
    return false;
#endif
}

bool Barometer::read() {
    if (!_present) return false;
    if (!readChip()) return false;

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
