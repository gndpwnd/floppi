/*
 * Barometer Module Header  (USE_BAROMETER — telemetry-only)
 *
 * Optional barometric pressure / temperature / relative-altitude sensor.
 * TELEMETRY-ONLY: read on a dedicated ESP32 Core-1 FreeRTOS task and exposed
 * via the swarm-API surface. It NEVER feeds the Core-0 flight loop — altitude
 * logic is flight-computer territory (see scope.md and
 * docs/findings/barometer_integration_spec_2026-05-20.md).
 *
 * The entire module is gated on USE_BAROMETER. When the flag is undefined this
 * header expands to nothing and zero bytes are compiled.
 *
 * Enable: #define USE_BAROMETER in config.h (default OFF).
 */

#ifndef BAROMETER_H
#define BAROMETER_H

#include "config.h"

#ifdef USE_BAROMETER

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Driver
//=============================================================================
// A small self-contained I2C barometer driver. BAROMETER_TYPE selects the
// chip; BMP280 is the implemented default (cheapest, most common — see the
// baro spec §2). BMP388/MS5611 share the same public API; if selected without
// a full driver they degrade gracefully (begin() returns false, telemetry
// reports not-ok) rather than breaking the build.

class Barometer {
public:
    Barometer();

    // Initialise the sensor on the configured I2C bus + address.
    // Non-blocking. Returns true if the chip ID was read back correctly.
    bool begin();

    // Take one pressure + temperature sample and update the derived
    // relative-altitude estimate. Applies the BARO_LPF PT1 filter.
    // Returns true on a successful I2C read. Safe to call only from Core 1.
    bool read();

    // Latest filtered readings (valid after a successful read()).
    float pressurePa() const   { return _pressure_pa; }   // pressure, pascals
    float temperatureC() const { return _temp_c; }        // temperature, deg C
    float altitudeM() const    { return _altitude_m; }    // relative altitude, m

    bool isPresent() const { return _present; }

    // Sea-level reference pressure (Pa) used for the altitude conversion.
    // Set by the 'b' calibration routine; defaults to BARO_SEA_LEVEL_PA.
    void  setSeaLevelPressure(float pa) { _sea_level_pa = pa; }
    float seaLevelPressure() const      { return _sea_level_pa; }

private:
    bool  readChip();   // chip-specific raw read; fills _pressure_pa/_temp_c

    bool  _present;
    bool  _filter_primed;
    float _pressure_pa;
    float _temp_c;
    float _altitude_m;
    float _sea_level_pa;

    // BMP280 factory trim values, read once in begin() (BMP280 path only).
    uint16_t _dig_t1;
    int16_t  _dig_t2, _dig_t3;
    uint16_t _dig_p1;
    int16_t  _dig_p2, _dig_p3, _dig_p4, _dig_p5, _dig_p6, _dig_p7, _dig_p8, _dig_p9;
    int32_t  _t_fine;

#if defined(BAROMETER_BMP388)
    // BMP388 factory trim, NVM-read then scaled to float quantisation
    // coefficients per the Bosch BMP3 datasheet (§9.1, float compensation).
    float _b3_t1, _b3_t2, _b3_t3;
    float _b3_p1, _b3_p2, _b3_p3, _b3_p4, _b3_p5, _b3_p6;
    float _b3_p7, _b3_p8, _b3_p9, _b3_p10, _b3_p11;
    float _b3_t_lin;   // linearised temperature, shared term (like _t_fine)
#endif

#if defined(BAROMETER_MS5611)
    // MS5611 factory PROM calibration coefficients C1..C6 (datasheet §PROM).
    uint16_t _ms_c1, _ms_c2, _ms_c3, _ms_c4, _ms_c5, _ms_c6;
#endif
};

//=============================================================================
// Core-1 task API  (used by main.cpp)
//=============================================================================
// startBarometerTask() spawns the dedicated Core-1 FreeRTOS task. It performs
// begin() inside the task and then polls at BARO_SAMPLE_RATE_HZ, publishing
// each reading into a spinlock-guarded snapshot.
void startBarometerTask();

// Telemetry snapshot accessors — safe to call from any Core-1 context
// (e.g. the swarm-API serializer). They copy the latest reading under a
// spinlock. baroTelemetryOk() is false until the first successful sample.
bool  baroTelemetryOk();
float baroTelemetryPressurePa();
float baroTelemetryTemperatureC();
float baroTelemetryAltitudeM();

#endif // USE_BAROMETER

#endif // BAROMETER_H
