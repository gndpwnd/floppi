/*
 * Calibration Mode - Header
 * Serial command interface and calibration workflow for calibration builds
 */

#ifndef CALIBRATION_MODE_H
#define CALIBRATION_MODE_H

#ifdef CALIBRATION_MODE

void checkCalibrationMode();
void checkSerialCommands();
void runCalibrationIfRequested();

#endif // CALIBRATION_MODE
#endif // CALIBRATION_MODE_H
