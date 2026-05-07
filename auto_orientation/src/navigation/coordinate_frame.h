#ifndef COORDINATE_FRAME_H
#define COORDINATE_FRAME_H

#include "../math/coordinates.h"
#include <cmath>
#include <mutex>

/**
 * CoordinateFrame: Local NED Origin Manager
 *
 * Maintains a local North-East-Down (NED) coordinate frame with origin from
 * the first GPS fix received. Provides conversions between WGS84 GPS coordinates
 * and local NED frame.
 *
 * Thread-safe for concurrent read operations. Initialization must happen before
 * read operations begin, or use initializeOnFirstFix() for automatic initialization.
 *
 * Performance: Pre-computes trigonometric values (sin/cos of origin latitude)
 * to avoid recalculation in conversion loops.
 *
 * Example:
 *   CoordinateFrame frame;
 *   frame.initializeOnFirstFix({47.3667, 11.1833, 520.0});  // Munich
 *   LocalFrame ned = frame.gpsToLocalNED(47.3677, 11.1842, 530.0);  // Point ~1km away
 *   GPS_Data gps = frame.localNEDToGPS(ned);  // Convert back (±0.1m accuracy)
 */
class CoordinateFrame {
 public:
    /**
     * Constructor: Creates uninitialized coordinate frame.
     * Must call initialize() or initializeOnFirstFix() before using conversions.
     */
    CoordinateFrame();

    /**
     * Initialize coordinate frame with explicit origin coordinates.
     *
     * Sets the NED frame origin to the specified latitude, longitude, and altitude.
     * Pre-computes trigonometric values for performance.
     *
     * @param lat_deg Origin latitude in degrees (-90 to +90, positive = North)
     * @param lon_deg Origin longitude in degrees (-180 to +180, positive = East)
     * @param alt_m Origin altitude above WGS84 ellipsoid in meters
     * @return true if initialization succeeded (valid coordinates), false otherwise
     *
     * Thread-safe: Acquires internal lock for write.
     */
    bool initialize(double lat_deg, double lon_deg, double alt_m);

    /**
     * Initialize coordinate frame on first GPS fix.
     *
     * Convenience method that extracts lat/lon/alt from GPS_Data structure.
     * Sets the NED frame origin to the first valid GPS position received.
     *
     * @param gps_data GPS position data (lat, lon, alt)
     * @return true if initialization succeeded, false if GPS data is invalid
     *
     * Thread-safe: Acquires internal lock for write.
     */
    bool initializeOnFirstFix(const GPS_Data& gps_data);

    /**
     * Check if coordinate frame has been initialized.
     *
     * @return true if initialize() or initializeOnFirstFix() succeeded, false otherwise
     *
     * Thread-safe: Acquires internal lock for read.
     */
    bool isInitialized() const;

    /**
     * Convert GPS coordinates to local NED frame.
     *
     * Transforms WGS84 GPS coordinates (lat/lon/alt) to a position in the
     * local North-East-Down frame relative to the stored origin.
     *
     * Conversions:
     * - North (X): Positive toward geographic North Pole
     * - East (Y): Positive toward East
     * - Down (Z): Positive downward toward Earth center
     *
     * Algorithm:
     * 1. Validate input GPS coordinates
     * 2. Convert GPS to ECEF (calls gps_to_ecef from Phase 1)
     * 3. Convert ECEF to NED relative to origin (calls ecef_to_ned from Phase 1)
     *
     * @param lat_deg Latitude in degrees
     * @param lon_deg Longitude in degrees
     * @param alt_m Altitude in meters
     * @return LocalFrame with (north_m, east_m, down_m) in meters, or (NaN, NaN, NaN) on error
     *
     * Thread-safe: Acquires internal lock for read.
     * @throws std::runtime_error if frame is not initialized
     */
    LocalFrame gpsToLocalNED(double lat_deg, double lon_deg, double alt_m) const;

    /**
     * Convert local NED coordinates to GPS.
     *
     * Inverse transformation: converts from local North-East-Down frame back to
     * WGS84 GPS coordinates.
     *
     * Algorithm:
     * 1. Convert NED to ECEF relative to origin (calls ned_to_ecef from Phase 1)
     * 2. Convert ECEF to GPS (calls ecef_to_gps from Phase 1)
     *
     * @param ned_point Local position in NED frame
     * @return GPS_Data with latitude, longitude, altitude, or NaN values on error
     *
     * Thread-safe: Acquires internal lock for read.
     * @throws std::runtime_error if frame is not initialized
     */
    GPS_Data localNEDToGPS(const LocalFrame& ned_point) const;

    /**
     * Get the origin coordinates (latitude, longitude, altitude).
     *
     * @return GPS_Data structure with the origin position
     * @throws std::runtime_error if frame is not initialized
     */
    GPS_Data getOrigin() const;

    /**
     * Get the origin altitude in meters above WGS84 ellipsoid.
     *
     * @return Altitude in meters
     * @throws std::runtime_error if frame is not initialized
     */
    double getOriginAltitude() const;

    /**
     * Reinitialize the coordinate frame with new origin.
     *
     * Allows changing the NED frame origin after initialization.
     * Useful for updating the reference frame during flight.
     *
     * @param lat_deg New origin latitude in degrees
     * @param lon_deg New origin longitude in degrees
     * @param alt_m New origin altitude in meters
     * @return true if reinitialization succeeded, false otherwise
     *
     * Thread-safe: Acquires internal lock for write.
     */
    bool reinitialize(double lat_deg, double lon_deg, double alt_m);

 private:
    // Origin coordinates
    double origin_lat_deg_;     ///< Origin latitude in degrees
    double origin_lon_deg_;     ///< Origin longitude in degrees
    double origin_alt_m_;       ///< Origin altitude in meters

    // Pre-computed trigonometric values for performance (origin latitude only)
    double origin_lat_rad_;     ///< Origin latitude in radians (cached)
    mutable double sin_lat_;    ///< sin(origin_lat_rad) - mutable for const methods
    mutable double cos_lat_;    ///< cos(origin_lat_rad) - mutable for const methods

    // ECEF coordinates of origin (cached)
    mutable ECEF origin_ecef_;  ///< ECEF coordinates of origin - mutable for const methods

    // Initialization flag
    bool is_initialized_;       ///< Flag indicating if frame has been initialized

    // Thread synchronization
    // Mutable to allow const methods to acquire lock for thread-safe reads
    mutable std::mutex mutex_;  ///< Mutex for thread-safe concurrent reads

    /**
     * Internal helper: Validates and caches trigonometric values.
     * Called by initialize() after setting origin coordinates.
     * Must be called while holding the mutex.
     */
    void cache_trig_values_();

    /**
     * Internal helper: Validates that frame is initialized.
     * Throws std::runtime_error if not initialized.
     * Assumes mutex is already held by caller.
     */
    void assert_initialized_() const;
};

#endif  // COORDINATE_FRAME_H
