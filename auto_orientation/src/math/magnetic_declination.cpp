#include "magnetic_declination.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// WMM2024 SIMPLIFIED LOOKUP TABLE
// ============================================================================

/**
 * Simplified WMM2024 declination data points for common locations.
 * Format: (latitude, longitude, declination_2024)
 *
 * Real implementation should use full gridded WMM dataset or NOAA API.
 * This simplified table provides reasonable accuracy (±2-5°) for the listed points
 * and basic linear interpolation for nearby locations.
 *
 * Data sources:
 * - NOAA Magnetic Declination Calculator (https://www.ncei.noaa.gov/products/magnetic-declination)
 * - World Magnetic Model 2024 (https://www.ncei.noaa.gov/products/world-magnetic-model)
 */

struct DeclinationPoint {
    double lat;  // Latitude in degrees
    double lon;  // Longitude in degrees
    double decl; // Declination in degrees (positive = East)
};

// Simplified WMM2024 lookup table (2024 epoch)
static const DeclinationPoint DECLINATION_TABLE[] = {
    // Europe
    {50.0, 0.0, -2.5},         // Prime Meridian, UK coast
    {51.5074, -0.1278, -2.7},  // London, UK
    {47.3667, 11.1833, 3.2},   // Munich, Germany
    {48.8566, 2.3522, -1.8},   // Paris, France
    {52.5200, 13.4050, 4.5},   // Berlin, Germany
    {43.7102, 7.2620, 1.8},    // Nice, France
    {40.7128, -74.0060, -12.4},// New York City, USA

    // North America
    {37.7749, -122.4194, 12.8}, // San Francisco, CA
    {34.0522, -118.2437, 11.4}, // Los Angeles, CA
    {39.7392, -104.9903, -8.5}, // Denver, CO
    {41.8781, -87.6298, -4.1},  // Chicago, IL
    {25.7617, -80.1918, -4.8},  // Miami, FL
    {37.5407, -122.2710, 12.8}, // Palo Alto, CA

    // Asia
    {35.6762, 139.6503, 8.2},   // Tokyo, Japan
    {1.3521, 103.8198, 0.8},    // Singapore
    {22.3193, 114.1694, 3.2},   // Hong Kong

    // Southern Hemisphere
    {-33.8688, 151.2093, 12.0}, // Sydney, Australia
    {-23.5505, -46.6333, -22.8},// São Paulo, Brazil
    {-33.9249, 18.4241, -24.8}, // Cape Town, South Africa

    // Equatorial regions
    {0.0, 0.0, -2.5},           // Equator, Prime Meridian
    {0.0, 90.0, -0.5},          // Equator, 90°E
    {0.0, 180.0, -3.5},         // Equator, International Date Line
    {0.0, -90.0, 4.5},          // Equator, 90°W

    // Poles
    {89.0, 0.0, 89.0},          // Near North Pole
    {-89.0, 0.0, -89.0},        // Near South Pole
};

static constexpr int DECLINATION_TABLE_SIZE =
    sizeof(DECLINATION_TABLE) / sizeof(DeclinationPoint);

// ============================================================================
// HELPER: DISTANCE CALCULATION (GREAT CIRCLE APPROXIMATION)
// ============================================================================

/**
 * Calculate approximate great-circle distance between two points on Earth.
 * Uses simplified haversine formula.
 *
 * @param lat1_deg, lon1_deg: First point
 * @param lat2_deg, lon2_deg: Second point
 * @return Distance in degrees (approximate)
 */
static double angular_distance(double lat1_deg, double lon1_deg,
                                double lat2_deg, double lon2_deg) {
    double lat1_rad = lat1_deg * M_PI / 180.0;
    double lat2_rad = lat2_deg * M_PI / 180.0;
    double lon_delta_rad = (lon2_deg - lon1_deg) * M_PI / 180.0;

    // Simplified great-circle distance formula (accurate for small distances)
    double x = lon_delta_rad * std::cos((lat1_rad + lat2_rad) / 2.0);
    double y = lat2_rad - lat1_rad;
    return std::sqrt(x * x + y * y) * 180.0 / M_PI;  // Convert back to degrees
}

// ============================================================================
// MAGNETIC DECLINATION LOOKUP WITH INTERPOLATION
// ============================================================================

double get_magnetic_declination(double lat_deg, double lon_deg, int year) {
    // Constrain longitude to [-180, 180]
    if (lon_deg > 180.0) {
        lon_deg -= 360.0;
    } else if (lon_deg < -180.0) {
        lon_deg += 360.0;
    }

    // Find the closest points in the lookup table
    double min_distance = 1e9;
    int closest_idx = 0;
    double second_min_distance = 1e9;
    int second_closest_idx = -1;

    for (int i = 0; i < DECLINATION_TABLE_SIZE; ++i) {
        double dist = angular_distance(lat_deg, lon_deg,
                                       DECLINATION_TABLE[i].lat,
                                       DECLINATION_TABLE[i].lon);

        if (dist < min_distance) {
            // Shift current closest to second
            second_min_distance = min_distance;
            second_closest_idx = closest_idx;
            // Update closest
            min_distance = dist;
            closest_idx = i;
        } else if (dist < second_min_distance) {
            second_min_distance = dist;
            second_closest_idx = i;
        }
    }

    // If closest point is very close (< 1 degree), use it directly
    if (min_distance < 1.0) {
        double decl = DECLINATION_TABLE[closest_idx].decl;
        // Apply annual change (WMM updates ~0.1-0.25°/year depending on location)
        // Simplified: assume linear change, typical rate ~0.15°/year
        // Adjust from 2024 baseline to requested year
        int year_offset = year - 2024;
        decl += year_offset * 0.15;  // Rough approximation
        return decl;
    }

    // If we have a second closest point, interpolate
    if (second_closest_idx >= 0 && second_min_distance < 30.0) {
        // Linear interpolation by inverse distance
        double w1 = 1.0 / (min_distance + 0.1);  // Add epsilon to avoid division by zero
        double w2 = 1.0 / (second_min_distance + 0.1);
        double w_sum = w1 + w2;

        double decl1 = DECLINATION_TABLE[closest_idx].decl;
        double decl2 = DECLINATION_TABLE[second_closest_idx].decl;

        // Weighted average
        double decl = (w1 * decl1 + w2 * decl2) / w_sum;

        // Apply annual change
        int year_offset = year - 2024;
        decl += year_offset * 0.15;
        return decl;
    }

    // Fallback: use closest point (even if far)
    double decl = DECLINATION_TABLE[closest_idx].decl;
    int year_offset = year - 2024;
    decl += year_offset * 0.15;
    return decl;
}

// ============================================================================
// HEADING CONVERSIONS
// ============================================================================

double apply_declination(double mag_heading_deg, double declination_deg) {
    // Formula: true_heading = magnetic_heading + declination
    double true_heading = mag_heading_deg + declination_deg;

    // Normalize to [0, 360)
    return normalize_heading(true_heading);
}

double remove_declination(double true_heading_deg, double declination_deg) {
    // Formula: magnetic_heading = true_heading - declination
    double mag_heading = true_heading_deg - declination_deg;

    // Normalize to [0, 360)
    return normalize_heading(mag_heading);
}
