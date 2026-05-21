/*
 * Barometer Calibration - Header  (W4 — Workstream W4)
 *
 * Adds the 'b' serial calibration command: samples the barometer's current
 * pressure and establishes the sea-level reference pressure so the present
 * location reads as altitude zero. Telemetry-only — this never feeds the
 * Core-0 flight loop (see docs/findings/barometer_integration_spec_2026-05-20.md).
 *
 * The whole module is double-gated: it only compiles in a calibration build
 * (CALIBRATION_MODE) AND when the barometer feature is enabled (USE_BAROMETER).
 * Default flight builds compile zero bytes of this.
 */

#ifndef CALIBRATION_BARO_H
#define CALIBRATION_BARO_H

#include "config.h"

#if defined(CALIBRATION_MODE) && defined(USE_BAROMETER)

/**
 * Barometer calibration routine (serial command 'b').
 *
 * Brings up a local Barometer instance, averages a burst of pressure
 * samples, sets the sea-level reference pressure so the current spot reads
 * altitude zero, prints a copy-paste-ready BARO_SEA_LEVEL_PA #define plus the
 * CALIBRATED_BAROMETER marker, and verifies the resulting altitude estimate.
 */
void calibrateBarometer();

#endif // CALIBRATION_MODE && USE_BAROMETER

#endif // CALIBRATION_BARO_H
