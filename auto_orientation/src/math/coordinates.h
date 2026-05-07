#ifndef COORDINATES_H
#define COORDINATES_H

#include <cmath>
#include <cstdint>

/**
 * GPS and Geodetic Coordinate System Conversions
 *
 * Provides conversions between:
 * - WGS84 (Lat/Lon/Alt): GPS native format
 * - ECEF (Earth-Centered Earth-Fixed): Cartesian Earth coordinates
 * - NED (North-East-Down): Local tangent plane navigation frame
 *
 * All distances in meters, angles in degrees (except where marked as radians).
 *
 * Based on GPS_GEODETIC_COORDINATE_SYSTEMS.md theoretical framework.
 */

// ============================================================================
// WGS84 CONSTANTS
// ============================================================================

namespace WGS84 {
    /**
     * WGS84 Ellipsoid Parameters
     * Reference: IHO Special Publication S-32 (World Geodetic System 1984)
     */

    // Semi-major axis (equatorial radius)
    static constexpr double SEMI_MAJOR_AXIS = 6378137.0;

    // Semi-minor axis (polar radius)
    static constexpr double SEMI_MINOR_AXIS = 6356752.314245;

    // Flattening: f = (a - b) / a
    static constexpr double FLATTENING = 1.0 / 298.257223563;

    // Eccentricity squared: e² = 2f - f²
    // Derived from flattening, used in coordinate conversions
    static constexpr double ECCENTRICITY_SQ = 0.00669437999014;

    /**
     * Calculate radius of curvature in prime vertical N(φ)
     *
     * N(φ) = a / sqrt(1 - e² sin²(φ))
     *
     * This is the distance from the ellipsoid surface to the polar axis,
     * measured perpendicular to the ellipsoid meridian.
     *
     * @param lat_rad Latitude in radians
     * @return Radius of curvature in meters
     */
    inline double radius_of_curvature(double lat_rad) {
        double sin_lat = std::sin(lat_rad);
        return SEMI_MAJOR_AXIS / std::sqrt(1.0 - ECCENTRICITY_SQ * sin_lat * sin_lat);
    }
}  // namespace WGS84

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * GPS Data: Latitude, Longitude, Altitude, and Accuracy Metrics
 *
 * Represents position as obtained from GPS module (e.g., NEO-M9N).
 * Altitude is Height Above WGS84 Ellipsoid (HAE), not MSL.
 */
struct GPS_Data {
    double latitude_deg;   ///< Latitude in degrees (-90 to +90, positive = North)
    double longitude_deg;  ///< Longitude in degrees (-180 to +180, positive = East)
    double altitude_m;     ///< Height above WGS84 ellipsoid (HAE) in meters
    float hdop;            ///< Horizontal Dilution of Precision (unitless)
    float vdop;            ///< Vertical Dilution of Precision (unitless)

    GPS_Data()
        : latitude_deg(0.0), longitude_deg(0.0), altitude_m(0.0), hdop(0.0), vdop(0.0) {}

    GPS_Data(double lat, double lon, double alt, float h = 0.0f, float v = 0.0f)
        : latitude_deg(lat), longitude_deg(lon), altitude_m(alt), hdop(h), vdop(v) {}
};

/**
 * ECEF Coordinates: Earth-Centered, Earth-Fixed Cartesian
 *
 * Origin at Earth's center. X-axis points to prime meridian at equator,
 * Y-axis points 90° east, Z-axis points to North Pole.
 * All coordinates in meters.
 */
struct ECEF {
    double x;  ///< X-axis coordinate (meters)
    double y;  ///< Y-axis coordinate (meters)
    double z;  ///< Z-axis coordinate (meters)

    ECEF() : x(0.0), y(0.0), z(0.0) {}

    ECEF(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    /**
     * Get magnitude (distance from Earth's center)
     * @return Distance in meters
     */
    double magnitude() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    /**
     * Get squared magnitude (faster for comparisons)
     * @return Distance squared in meters²
     */
    double magnitude_squared() const {
        return x * x + y * y + z * z;
    }
};

/**
 * Local Tangent Plane: North-East-Down (NED)
 *
 * NED frame relative to a reference location:
 * - North (X): Positive toward geographic North Pole
 * - East (Y): Positive toward East (perpendicular to North)
 * - Down (Z): Positive toward Earth center (opposite of "Up")
 *
 * This is the standard for aviation and drone navigation.
 * All coordinates in meters relative to an origin point.
 */
struct LocalFrame {
    double north_m;  ///< North component (meters, positive = North)
    double east_m;   ///< East component (meters, positive = East)
    double down_m;   ///< Down component (meters, positive = Down)

    LocalFrame() : north_m(0.0), east_m(0.0), down_m(0.0) {}

    LocalFrame(double n, double e, double d) : north_m(n), east_m(e), down_m(d) {}

    /**
     * Get magnitude (distance from origin)
     * @return Distance in meters
     */
    double magnitude() const {
        return std::sqrt(north_m * north_m + east_m * east_m + down_m * down_m);
    }

    /**
     * Get horizontal distance (North-East plane only)
     * @return Distance in meters, ignoring vertical component
     */
    double horizontal_distance() const {
        return std::sqrt(north_m * north_m + east_m * east_m);
    }
};

// ============================================================================
// CONVERSION FUNCTIONS: GPS <-> ECEF
// ============================================================================

/**
 * Convert WGS84 geodetic coordinates to ECEF
 *
 * Converts from latitude/longitude/altitude (as reported by GPS) to
 * Earth-Centered Earth-Fixed Cartesian coordinates.
 *
 * Formula:
 *   N(φ) = a / sqrt(1 - e² sin²(φ))
 *   x = (N + h) cos(φ) cos(λ)
 *   y = (N + h) cos(φ) sin(λ)
 *   z = (N(1 - e²) + h) sin(φ)
 *
 * @param lat_deg Latitude in degrees (-90 to +90)
 * @param lon_deg Longitude in degrees (-180 to +180)
 * @param alt_m Altitude above WGS84 ellipsoid in meters
 * @return ECEF coordinates (x, y, z) in meters
 *
 * @note Input validation: Latitude must be in [-90, 90], longitude in [-180, 180].
 *       If invalid, returns an ECEF with NaN values.
 *
 * Example:
 *   ECEF ecef = gps_to_ecef(47.3667, 11.1833, 500.0);
 *   // Returns ECEF with x, y, z in meters for Munich coordinates
 */
ECEF gps_to_ecef(double lat_deg, double lon_deg, double alt_m);

/**
 * Convert ECEF coordinates to WGS84 geodetic (iterative solution)
 *
 * Converts from Cartesian Earth coordinates back to latitude/longitude/altitude.
 * Uses Heikkinen's iterative method, converging in 3-4 iterations.
 * Final accuracy: better than 1 millimeter for typical Earth locations.
 *
 * Method:
 * 1. Compute longitude from atan2(y, x)
 * 2. Iterate on latitude using: lat = atan2(z, p(1 - e²N/(N+h)))
 * 3. Compute altitude from: h = p / cos(lat) - N
 *
 * @param x X-axis ECEF coordinate (meters)
 * @param y Y-axis ECEF coordinate (meters)
 * @param z Z-axis ECEF coordinate (meters)
 * @return GPS_Data with latitude (degrees), longitude (degrees), altitude (meters)
 *
 * @note Handles special cases: poles (lat ±90°), equator (lat=0°), antimeridian (lon±180°)
 *
 * Example:
 *   GPS_Data gps = ecef_to_gps(-2764000.0, 1440000.0, 4690000.0);
 *   // Returns GPS_Data with lat≈47.3667°, lon≈11.1833°, alt≈500m
 */
GPS_Data ecef_to_gps(double x, double y, double z);

// ============================================================================
// CONVERSION FUNCTIONS: ECEF <-> NED
// ============================================================================

/**
 * Convert ECEF point to NED relative to a reference location
 *
 * Transforms an ECEF coordinate to North-East-Down frame centered at
 * a reference location. Uses rotation matrix derived from reference latitude/longitude.
 *
 * Process:
 * 1. Compute reference point in ECEF: origin_ecef = gps_to_ecef(ref_lat, ref_lon, ref_alt)
 * 2. Compute relative position: delta = point_ecef - origin_ecef
 * 3. Apply rotation matrix R_ECEF→NED:
 *      [N]   [-sin(φ)cos(λ)  -sin(λ)  -cos(φ)cos(λ)]   [Δx]
 *      [E] = [-sin(φ)sin(λ)   cos(λ)  -cos(φ)sin(λ)] × [Δy]
 *      [D]   [ cos(φ)         0        -sin(φ)      ]   [Δz]
 *
 * @param point_ecef The point to transform (ECEF coordinates)
 * @param origin_ecef The reference origin point (ECEF coordinates)
 * @param origin_lat_rad Reference latitude in radians
 * @return LocalFrame with (north_m, east_m, down_m) in meters
 *
 * @note The reference altitude affects origin_ecef calculation but not the rotation.
 *       Altimeter readings are relative to the ellipsoid in NED frame.
 *
 * Example:
 *   // Convert a point 111m north, 93m east, and 10m up from reference
 *   ECEF origin = gps_to_ecef(47.3667, 11.1833, 500.0);
 *   ECEF point = gps_to_ecef(47.3677, 11.1842, 510.0);  // Approximate offset
 *   LocalFrame ned = ecef_to_ned(point, origin, 47.3667 * M_PI / 180.0);
 *   // Returns approximately: north=111m, east=93m, down=-10m (up)
 */
LocalFrame ecef_to_ned(const ECEF& point_ecef, const ECEF& origin_ecef, double origin_lat_rad);

/**
 * Convert NED coordinates to ECEF
 *
 * Inverse transformation: converts from local North-East-Down frame back to
 * Earth-Centered Earth-Fixed Cartesian coordinates.
 *
 * Process:
 * 1. Apply inverse rotation matrix (transpose of R_ECEF→NED):
 *      [Δx]   [-sin(φ)cos(λ)  -sin(φ)sin(λ)   cos(φ)    ]   [N]
 *      [Δy] = [-sin(λ)         cos(λ)          0         ] × [E]
 *      [Δz]   [-cos(φ)cos(λ)  -cos(φ)sin(λ)  -sin(φ)   ]   [D]
 * 2. Add origin: point_ecef = delta + origin_ecef
 *
 * @param ned_point Local position (North-East-Down)
 * @param origin_ecef Reference origin in ECEF coordinates
 * @param origin_lat_rad Reference latitude in radians
 * @return ECEF coordinates of the point
 *
 * @note Inverse of ecef_to_ned(). Should satisfy:
 *       ecef_to_ned(ned_to_ecef(ned, origin, lat), origin, lat) ≈ ned
 *
 * Example:
 *   LocalFrame ned(111.0, 93.0, -10.0);  // 111m N, 93m E, 10m up
 *   ECEF origin = gps_to_ecef(47.3667, 11.1833, 500.0);
 *   ECEF point = ned_to_ecef(ned, origin, 47.3667 * M_PI / 180.0);
 *   // Returns ECEF coordinates of a point approximately at (47.3677, 11.1842, 510)
 */
ECEF ned_to_ecef(const LocalFrame& ned_point, const ECEF& origin_ecef, double origin_lat_rad);

// ============================================================================
// ROUND-TRIP CONVENIENCE FUNCTIONS
// ============================================================================

/**
 * Convert GPS coordinates to NED directly (combining gps_to_ecef + ecef_to_ned)
 *
 * @param lat_deg Latitude in degrees
 * @param lon_deg Longitude in degrees
 * @param alt_m Altitude in meters
 * @param ref_lat_deg Reference latitude in degrees
 * @param ref_lon_deg Reference longitude in degrees
 * @param ref_alt_m Reference altitude in meters
 * @return LocalFrame with NED coordinates relative to reference
 *
 * @note More convenient than two-step conversion, minimal performance overhead
 */
LocalFrame gps_to_ned(double lat_deg, double lon_deg, double alt_m,
                      double ref_lat_deg, double ref_lon_deg, double ref_alt_m);

/**
 * Convert NED coordinates to GPS directly (combining ned_to_ecef + ecef_to_gps)
 *
 * @param ned_point Local position in NED frame
 * @param ref_lat_deg Reference latitude in degrees
 * @param ref_lon_deg Reference longitude in degrees
 * @param ref_alt_m Reference altitude in meters
 * @return GPS_Data with latitude, longitude, altitude
 */
GPS_Data ned_to_gps(const LocalFrame& ned_point,
                    double ref_lat_deg, double ref_lon_deg, double ref_alt_m);

// ============================================================================
// INPUT VALIDATION
// ============================================================================

/**
 * Validate GPS coordinates are within acceptable ranges
 *
 * Checks:
 * - Latitude: [-90, +90]
 * - Longitude: [-180, +180]
 * - Altitude: [-500, +50000] meters (allows for deep oceans and high altitudes)
 *
 * @param lat_deg Latitude in degrees
 * @param lon_deg Longitude in degrees
 * @param alt_m Altitude in meters
 * @return True if all coordinates are valid, false otherwise
 */
bool is_valid_gps(double lat_deg, double lon_deg, double alt_m);

/**
 * Validate ECEF coordinates (basic sanity check)
 *
 * Checks:
 * - Not NaN or infinite
 * - Distance from Earth center is reasonable: [6.3e6, 6.4e6] meters
 *
 * @param ecef ECEF point to validate
 * @return True if valid, false otherwise
 */
bool is_valid_ecef(const ECEF& ecef);

#endif  // COORDINATES_H
