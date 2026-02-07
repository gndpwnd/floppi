/*
 * Display Module Header
 * OLED display abstraction layer with compile-time display selection.
 * Gated behind USE_OLED_DISPLAY build flag.
 *
 * See: docs/findings/display-module-architecture.md
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#ifdef USE_OLED_DISPLAY

#include "display_data.h"

// Initialize the OLED display (call in setup)
void setupDisplay();

// Populate display data struct from flight control globals
void populateDisplayData(DisplayData_t* data);

// Render the appropriate screen to the OLED
void renderDisplay(const DisplayData_t* data);

// Show a startup message on the display
void displayStartupMessage(const char* message);

#endif // USE_OLED_DISPLAY

#endif // DISPLAY_H
