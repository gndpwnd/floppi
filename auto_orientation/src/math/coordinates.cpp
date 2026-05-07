#include "coordinates.h"
#include <math.h>

// ============================================================================
// GPS <-> ECEF CONVERSIONS
// ============================================================================

ECEF gps_to_ecef(double lat_deg, double lon_deg, double alt_m) {
    // Validate input
    if (!is_valid_gps(lat_deg, lon_deg, alt_m)) {
        // Return ECEF with NaN to indicate invalid input
        return ECEF(NAN,
                    NAN,
                    NAN);
    }

    // Convert degrees to radians
    double lat_rad = lat_deg * M_PI / 180.0;
    double lon_rad = lon_deg * M_PI / 180.0;

    // Precompute trigonometric values
    double cos_lat = cos(lat_rad);
    double sin_lat = sin(lat_rad);
    double cos_lon = cos(lon_rad);
    double sin_lon = sin(lon_rad);

    // Calculate radius of curvature in prime vertical
    double N = WGS84::radius_of_curvature(lat_rad);

    // Compute ECEF coordinates
    double x = (N + alt_m) * cos_lat * cos_lon;
    double y = (N + alt_m) * cos_lat * sin_lon;
    double z = (N * (1.0 - WGS84::ECCENTRICITY_SQ) + alt_m) * sin_lat;

    return ECEF(x, y, z);
}

GPS_Data ecef_to_gps(double x, double y, double z) {
    // Validate input
    ECEF test(x, y, z);
    if (!is_valid_ecef(test)) {
        // Return GPS_Data with NaN to indicate invalid input
        return GPS_Data(NAN,
                        NAN,
                        NAN);
    }

    // Calculate longitude
    double lon_rad = atan2(y, x);

    // Calculate cylindrical distance from Z-axis
    double p = sqrt(x * x + y * y);

    // Initial latitude estimate using atan2
    double lat_rad = atan2(z, p * (1.0 - WGS84::ECCENTRICITY_SQ));

    // Iterate to improve latitude and altitude estimates (3-4 iterations for convergence)
    double N;
    double h;
    for (int i = 0; i < 4; ++i) {
        N = WGS84::radius_of_curvature(lat_rad);
        h = p / cos(lat_rad) - N;
        lat_rad = atan2(z, p * (1.0 - WGS84::ECCENTRICITY_SQ * N / (N + h)));
    }

    // Final computation of N and h
    N = WGS84::radius_of_curvature(lat_rad);
    h = p / cos(lat_rad) - N;

    // Handle special case: near poles where cos(lat) ≈ 0
    // Use alternative formula for altitude
    if (fabs(cos(lat_rad)) < 1e-6) {
        h = fabs(z) - WGS84::SEMI_MINOR_AXIS;
    }

    // Convert to degrees
    double lat_deg = lat_rad * 180.0 / M_PI;
    double lon_deg = lon_rad * 180.0 / M_PI;

    return GPS_Data(lat_deg, lon_deg, h);
}

// ============================================================================
// ECEF <-> NED CONVERSIONS
// ============================================================================

LocalFrame ecef_to_ned(const ECEF& point_ecef, const ECEF& origin_ecef, double origin_lat_rad) {
    // Compute relative position vector
    double dx = point_ecef.x - origin_ecef.x;
    double dy = point_ecef.y - origin_ecef.y;
    double dz = point_ecef.z - origin_ecef.z;

    // We need longitude too for the rotation matrix
    double lon_rad = atan2(origin_ecef.y, origin_ecef.x);

    // Precompute trigonometric values
    double sin_lat = sin(origin_lat_rad);
    double cos_lat = cos(origin_lat_rad);
    double sin_lon = sin(lon_rad);
    double cos_lon = cos(lon_rad);

    // Apply rotation matrix: R_ECEF→NED
    // [N]   [-sin(φ)cos(λ)  -sin(λ)  -cos(φ)cos(λ)]   [Δx]
    // [E] = [-sin(φ)sin(λ)   cos(λ)  -cos(φ)sin(λ)] × [Δy]
    // [D]   [ cos(φ)         0        -sin(φ)      ]   [Δz]

    double north = -sin_lat * cos_lon * dx - sin_lat * sin_lon * dy + cos_lat * dz;
    double east = -sin_lon * dx + cos_lon * dy;
    double down = -cos_lat * cos_lon * dx - cos_lat * sin_lon * dy - sin_lat * dz;

    return LocalFrame(north, east, down);
}

ECEF ned_to_ecef(const LocalFrame& ned_point, const ECEF& origin_ecef, double origin_lat_rad) {
    // We need longitude for the rotation matrix
    double lon_rad = atan2(origin_ecef.y, origin_ecef.x);

    // Precompute trigonometric values
    double sin_lat = sin(origin_lat_rad);
    double cos_lat = cos(origin_lat_rad);
    double sin_lon = sin(lon_rad);
    double cos_lon = cos(lon_rad);

    // Apply inverse rotation matrix (transpose of R_ECEF→NED)
    // [Δx]   [-sin(φ)cos(λ)  -sin(φ)sin(λ)   cos(φ)    ]   [N]
    // [Δy] = [-sin(λ)         cos(λ)          0         ] × [E]
    // [Δz]   [-cos(φ)cos(λ)  -cos(φ)sin(λ)  -sin(φ)   ]   [D]

    double dx = -sin_lat * cos_lon * ned_point.north_m - sin_lon * ned_point.east_m
                - cos_lat * cos_lon * ned_point.down_m;

    double dy = -sin_lat * sin_lon * ned_point.north_m + cos_lon * ned_point.east_m
                - cos_lat * sin_lon * ned_point.down_m;

    double dz = cos_lat * ned_point.north_m - sin_lat * ned_point.down_m;

    // Add origin offset
    double x = origin_ecef.x + dx;
    double y = origin_ecef.y + dy;
    double z = origin_ecef.z + dz;

    return ECEF(x, y, z);
}

// ============================================================================
// ROUND-TRIP CONVENIENCE FUNCTIONS
// ============================================================================

LocalFrame gps_to_ned(double lat_deg, double lon_deg, double alt_m,
                      double ref_lat_deg, double ref_lon_deg, double ref_alt_m) {
    // Convert GPS to ECEF
    ECEF point_ecef = gps_to_ecef(lat_deg, lon_deg, alt_m);
    ECEF origin_ecef = gps_to_ecef(ref_lat_deg, ref_lon_deg, ref_alt_m);

    // Convert to NED
    double ref_lat_rad = ref_lat_deg * M_PI / 180.0;
    return ecef_to_ned(point_ecef, origin_ecef, ref_lat_rad);
}

GPS_Data ned_to_gps(const LocalFrame& ned_point,
                    double ref_lat_deg, double ref_lon_deg, double ref_alt_m) {
    // Convert reference to ECEF
    ECEF origin_ecef = gps_to_ecef(ref_lat_deg, ref_lon_deg, ref_alt_m);

    // Convert NED to ECEF
    double ref_lat_rad = ref_lat_deg * M_PI / 180.0;
    ECEF point_ecef = ned_to_ecef(ned_point, origin_ecef, ref_lat_rad);

    // Convert ECEF to GPS
    return ecef_to_gps(point_ecef.x, point_ecef.y, point_ecef.z);
}

// ============================================================================
// INPUT VALIDATION
// ============================================================================

bool is_valid_gps(double lat_deg, double lon_deg, double alt_m) {
    // Check for NaN or infinity
    if (isnan(lat_deg) || isnan(lon_deg) || isnan(alt_m)) {
        return false;
    }
    if (isinf(lat_deg) || isinf(lon_deg) || isinf(alt_m)) {
        return false;
    }

    // Check latitude range [-90, +90]
    if (lat_deg < -90.0 || lat_deg > 90.0) {
        return false;
    }

    // Check longitude range [-180, +180]
    if (lon_deg < -180.0 || lon_deg > 180.0) {
        return false;
    }

    // Check altitude range (allow for oceans and high altitudes)
    // Range: [-500m (Mariana Trench) to +50000m (ISS + margin)]
    if (alt_m < -500.0 || alt_m > 50000.0) {
        return false;
    }

    return true;
}

bool is_valid_ecef(const ECEF& ecef) {
    // Check for NaN or infinity
    if (isnan(ecef.x) || isnan(ecef.y) || isnan(ecef.z)) {
        return false;
    }
    if (isinf(ecef.x) || isinf(ecef.y) || isinf(ecef.z)) {
        return false;
    }

    // Check distance from Earth's center
    // Should be approximately between Earth's radius (6.371e6 m) plus/minus altitude range
    // Allow range: [6.3e6, 6.4e6] meters to accommodate altitudes up to 50km
    double distance = ecef.magnitude();
    if (distance < 6.3e6 || distance > 6.4e6) {
        return false;
    }

    return true;
}
