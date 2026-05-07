#ifndef MAGNETIC_DECLINATION_H
#define MAGNETIC_DECLINATION_H

#include <cmath>

/**
 * Magnetic Declination and Heading Correction
 *
 * Provides functions to convert between magnetic headings (from compass/magnetometer)
 * and true headings (geographic north).
 *
 * Magnetic declination varies by location and time, typically ranging from -25° to +25°.
 * Positive = East (magnetic north is east of true north)
 * Negative = West (magnetic north is west of true north)
 *
 * Based on World Magnetic Model (WMM) 2024 or simplified IGRF model.
 */

// ============================================================================
// MAGNETIC DECLINATION LOOKUP
// ============================================================================

/**
 * Get magnetic declination for a location and year
 *
 * Uses simplified 2024 WMM data points for common locations.
 * For production use, consider:
 * - NOAA Magnetic Declination Calculator API
 * - igrfmodel Python package
 * - Full WMM2024 gridded dataset
 *
 * The simplified implementation uses a basic lookup table with linear
 * interpolation between known points.
 *
 * @param lat_deg Latitude in degrees (-90 to +90)
 * @param lon_deg Longitude in degrees (-180 to +180)
 * @param year Calendar year (e.g., 2024, 2025)
 * @return Magnetic declination in degrees
 *         Positive = East (mag north is east of true north)
 *         Negative = West (mag north is west of true north)
 *         Range: approximately -25° to +25°
 *
 * @note For highest accuracy, use online NOAA calculator or full WMM model.
 *       This simplified version has typical accuracy ±2-5° depending on location.
 *
 * Example:
 *   double decl = get_magnetic_declination(47.3667, 11.1833, 2024);
 *   // Munich in 2024: approximately 2-3° East
 *
 *   decl = get_magnetic_declination(37.7749, -122.4194, 2024);
 *   // San Francisco in 2024: approximately 12-13° East
 */
double get_magnetic_declination(double lat_deg, double lon_deg, int year);

/**
 * Get magnetic declination for a location (using current year 2024)
 *
 * Convenience function that calls get_magnetic_declination with year=2024
 *
 * @param lat_deg Latitude in degrees
 * @param lon_deg Longitude in degrees
 * @return Magnetic declination in degrees for 2024
 */
inline double get_magnetic_declination(double lat_deg, double lon_deg) {
    return get_magnetic_declination(lat_deg, lon_deg, 2024);
}

// ============================================================================
// HEADING CONVERSIONS
// ============================================================================

/**
 * Convert magnetic heading to true heading
 *
 * Applies magnetic declination correction to convert a compass/magnetometer
 * heading to true heading relative to geographic north.
 *
 * Formula:
 *   H_true = H_magnetic + declination
 *
 * The result is normalized to [0, 360) degrees.
 *
 * @param mag_heading_deg Magnetic heading in degrees [0, 360)
 *                        (from compass or magnetometer)
 * @param declination_deg Magnetic declination in degrees
 *                        Positive = East (magnetic north east of true)
 *                        Negative = West (magnetic north west of true)
 * @return True heading in degrees [0, 360)
 *         0° = True North
 *         90° = East
 *         180° = South
 *         270° = West
 *
 * Example:
 *   // Magnetometer reads 45° (NE), location has 12° East declination
 *   double true_heading = apply_declination(45.0, 12.0);
 *   // Returns 57.0° (still NE, but corrected to true north)
 *
 *   // Magnetometer reads 45°, location has -12° West declination
 *   true_heading = apply_declination(45.0, -12.0);
 *   // Returns 33.0° (corrected westward)
 */
double apply_declination(double mag_heading_deg, double declination_deg);

/**
 * Convert true heading to magnetic heading (inverse operation)
 *
 * Removes magnetic declination to get the reading a magnetometer would show
 * for a given true heading.
 *
 * Formula:
 *   H_magnetic = H_true - declination
 *
 * The result is normalized to [0, 360) degrees.
 *
 * @param true_heading_deg True heading in degrees [0, 360)
 * @param declination_deg Magnetic declination in degrees
 * @return Magnetic heading in degrees [0, 360)
 *
 * Example:
 *   // Want true heading of 33° at location with -12° declination
 *   double mag_heading = remove_declination(33.0, -12.0);
 *   // Returns 45.0° (what magnetometer should read)
 */
double remove_declination(double true_heading_deg, double declination_deg);

/**
 * Normalize heading to [0, 360) degrees
 *
 * Handles negative angles and angles >= 360°
 *
 * @param heading_deg Heading in degrees (any value)
 * @return Normalized heading in [0, 360)
 *
 * Example:
 *   normalize_heading(-45.0) -> 315.0°
 *   normalize_heading(405.0) -> 45.0°
 *   normalize_heading(45.0) -> 45.0°
 */
inline double normalize_heading(double heading_deg) {
    double result = std::fmod(heading_deg, 360.0);
    if (result < 0.0) {
        result += 360.0;
    }
    return result;
}

/**
 * Convert heading in degrees to radians
 *
 * @param heading_deg Heading in degrees
 * @return Heading in radians
 */
inline double heading_to_radians(double heading_deg) {
    return heading_deg * M_PI / 180.0;
}

/**
 * Convert heading in radians to degrees
 *
 * @param heading_rad Heading in radians
 * @return Heading in degrees (normalized to [0, 360))
 */
inline double heading_from_radians(double heading_rad) {
    return normalize_heading(heading_rad * 180.0 / M_PI);
}

#endif  // MAGNETIC_DECLINATION_H
