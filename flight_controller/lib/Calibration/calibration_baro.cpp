/*
 * Barometer Calibration - Implementation  (W4 — Workstream W4)
 *
 * Serial command 'b': sample current barometric pressure and establish the
 * sea-level reference pressure used by the relative-altitude conversion.
 *
 * SCOPE: this is a calibration-build helper only. It runs a LOCAL Barometer
 * instance (it does NOT touch the Core-1 telemetry task / snapshot). The
 * altitude readout it produces is telemetry-grade and never feeds the Core-0
 * flight loop — altitude logic is flight-computer territory
 * (see docs/findings/barometer_integration_spec_2026-05-20.md §3).
 *
 * Double-gated: compiles only when CALIBRATION_MODE && USE_BAROMETER.
 */

#include "calibration_baro.h"

#if defined(CALIBRATION_MODE) && defined(USE_BAROMETER)

#include <Arduino.h>
#include "barometer.h"

// Number of pressure samples averaged to settle sensor + filter noise.
static const int BARO_CAL_SAMPLES = 50;

void calibrateBarometer() {
    Serial.println(F("\n+-----------------------------------------------------------+"));
    Serial.println(F("|     BAROMETER CALIBRATION (sea-level reference)           |"));
    Serial.println(F("+-----------------------------------------------------------+\n"));

    Serial.println(F("This samples the barometer's current pressure and sets the"));
    Serial.println(F("sea-level reference so THIS location reads altitude zero."));
    Serial.println(F("Keep the aircraft still at the height you want as zero.\n"));

    // Local instance — independent of the Core-1 telemetry task.
    Barometer baro;

    Serial.println(F("Initialising barometer..."));
    if (!baro.begin() || !baro.isPresent()) {
        Serial.println(F("\nFAILED: no barometer detected on the I2C (Wire) bus."));
        Serial.println(F("  - Check wiring: SDA/SCL shared with the IMU (GPIO 21/22)."));
        Serial.print(F("  - Check BARO_I2C_ADDRESS (config.h): 0x"));
        Serial.println(BARO_I2C_ADDRESS, HEX);
        Serial.println(F("  - BMP280 breakouts are 0x76 (SDO=GND) or 0x77 (SDO=VCC)."));
        Serial.println(F("\nBarometer calibration aborted.\n"));
        return;
    }
    Serial.println(F("Barometer detected. Sampling pressure..."));

    // Average a burst of pressure samples to settle noise.
    double sum_pa = 0.0;
    double sum_temp = 0.0;
    int ok_samples = 0;
    for (int i = 0; i < BARO_CAL_SAMPLES; i++) {
        if (baro.read()) {
            sum_pa   += baro.pressurePa();
            sum_temp += baro.temperatureC();
            ok_samples++;
        }
        delay(20);  // ~50 Hz sampling, well within sensor ODR
    }

    if (ok_samples < BARO_CAL_SAMPLES / 2) {
        Serial.print(F("\nFAILED: only "));
        Serial.print(ok_samples);
        Serial.print(F(" of "));
        Serial.print(BARO_CAL_SAMPLES);
        Serial.println(F(" reads succeeded — sensor unstable."));
        Serial.println(F("Check wiring and try again.\n"));
        return;
    }

    float avg_pa   = (float)(sum_pa / ok_samples);
    float avg_temp = (float)(sum_temp / ok_samples);

    // Establish the sea-level reference so the current spot reads zero.
    // The altitude conversion is alt = 44330 * (1 - (P / P0)^0.1903); for the
    // measured pressure to map to alt = 0, the reference P0 must equal the
    // measured pressure. This is the W2 setSeaLevelPressure() hook.
    baro.setSeaLevelPressure(avg_pa);

    // Verify: one more read should now report ~0 m altitude.
    float verify_alt = 0.0f;
    if (baro.read()) {
        verify_alt = baro.altitudeM();
    }

    Serial.println(F("\n--- Barometer Calibration Results ---"));
    Serial.print(F("  Samples averaged : "));
    Serial.print(ok_samples);
    Serial.print(F(" / "));
    Serial.println(BARO_CAL_SAMPLES);
    Serial.print(F("  Pressure         : "));
    Serial.print(avg_pa, 1);
    Serial.println(F(" Pa"));
    Serial.print(F("  Temperature      : "));
    Serial.print(avg_temp, 2);
    Serial.println(F(" C"));
    Serial.print(F("  Sea-level ref    : "));
    Serial.print(baro.seaLevelPressure(), 1);
    Serial.println(F(" Pa"));
    Serial.print(F("  Altitude (zeroed): "));
    Serial.print(verify_alt, 2);
    Serial.println(F(" m"));

    Serial.println(F("\n--- Copy into config.h ---"));
    Serial.print(F("#define BARO_SEA_LEVEL_PA "));
    Serial.print(avg_pa, 1);
    Serial.println(F("f   // calibrated with 'b' command"));
    Serial.println(F("#define CALIBRATED_BAROMETER   // Barometer sea-level reference set ('b')"));
    Serial.println(F("\nBarometer calibration complete.\n"));
}

#endif // CALIBRATION_MODE && USE_BAROMETER
