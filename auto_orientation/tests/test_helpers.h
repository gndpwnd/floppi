/**
 * Test Helper Utilities
 *
 * Provides common utilities for integration tests:
 * - Quaternion mathematics and validation
 * - Position validation and conversion
 * - Data generation for testing
 * - Mock sensor data
 *
 * NOTE: This header is only available when DEBUG_MODE is defined.
 * Include only in test code that is compiled with -D DEBUG_MODE.
 */

#ifdef DEBUG_MODE

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <cmath>
#include <cstdint>
#include "../src/sensors/sensor_base.h"

// ============================================================================
// Quaternion Helpers
// ============================================================================

/**
 * Calculate quaternion magnitude (should be ~1.0 for normalized)
 *
 * @param q Quaternion data
 * @return Magnitude = sqrt(w^2 + x^2 + y^2 + z^2)
 */
inline float QuaternionMagnitude(const OrientationData& q) {
  return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
}

/**
 * Check if quaternion is normalized (magnitude ~1.0)
 *
 * @param q Quaternion data
 * @param tolerance Acceptable deviation from 1.0 (default 0.01)
 * @return true if magnitude is within tolerance of 1.0
 */
inline bool IsQuaternionNormalized(const OrientationData& q, float tolerance = 0.01f) {
  float mag = QuaternionMagnitude(q);
  return (mag >= 1.0f - tolerance) && (mag <= 1.0f + tolerance);
}

/**
 * Create a normalized quaternion from axis-angle representation
 *
 * Useful for creating test data with known orientations.
 *
 * @param angle_rad Rotation angle in radians
 * @param axis_x X component of rotation axis (should be normalized)
 * @param axis_y Y component of rotation axis
 * @param axis_z Z component of rotation axis
 * @return Quaternion representing the rotation
 */
inline OrientationData QuaternionFromAxisAngle(float angle_rad, float axis_x, float axis_y, float axis_z) {
  OrientationData q;
  float half_angle = angle_rad / 2.0f;
  float sin_half = std::sin(half_angle);
  q.w = std::cos(half_angle);
  q.x = axis_x * sin_half;
  q.y = axis_y * sin_half;
  q.z = axis_z * sin_half;
  q.cal_status = 3;
  q.cal_accel = 3;
  q.cal_gyro = 3;
  q.cal_mag = 3;
  q.timestamp_ms = 0;
  return q;
}

/**
 * Create an identity quaternion (no rotation)
 *
 * @param cal_status Calibration status (default 3)
 * @return Identity quaternion [1, 0, 0, 0]
 */
inline OrientationData QuaternionIdentity(uint8_t cal_status = 3) {
  OrientationData q;
  q.w = 1.0f;
  q.x = 0.0f;
  q.y = 0.0f;
  q.z = 0.0f;
  q.cal_status = cal_status;
  q.cal_accel = cal_status;
  q.cal_gyro = cal_status;
  q.cal_mag = cal_status;
  q.timestamp_ms = 0;
  return q;
}

/**
 * Normalize a quaternion to unit magnitude
 *
 * @param q Input quaternion
 * @return Normalized quaternion
 */
inline OrientationData QuaternionNormalize(const OrientationData& q) {
  float mag = QuaternionMagnitude(q);
  OrientationData result = q;
  if (mag > 0.0001f) {
    result.w /= mag;
    result.x /= mag;
    result.y /= mag;
    result.z /= mag;
  }
  return result;
}

// ============================================================================
// Position Helpers
// ============================================================================

/**
 * Check if position is within valid geographic bounds
 *
 * @param p Position data
 * @return true if -90 <= lat <= 90 and -180 <= lon <= 180
 */
inline bool IsPositionValid(const PositionData& p) {
  return (p.latitude >= -90.0 && p.latitude <= 90.0 &&
          p.longitude >= -180.0 && p.longitude <= 180.0);
}

/**
 * Check if position indicates a valid GPS fix
 *
 * @param p Position data
 * @return true if fix_quality >= 1 and satellites > 0
 */
inline bool HasValidGPSFix(const PositionData& p) {
  return IsPositionValid(p) && p.fix_quality >= 1 && p.num_satellites > 0;
}

/**
 * Create a position at a known location
 *
 * @param latitude Latitude in degrees (-90 to 90)
 * @param longitude Longitude in degrees (-180 to 180)
 * @param altitude Altitude in meters
 * @param accuracy Estimated accuracy in meters
 * @param satellites Number of satellites
 * @param fix_quality Fix quality (1=GPS, 2=DGPS, etc.)
 * @return PositionData struct
 */
inline PositionData CreatePosition(double latitude, double longitude, float altitude = 0.0f,
                                   float accuracy = 2.5f, uint8_t satellites = 8,
                                   uint8_t fix_quality = 1) {
  PositionData p;
  p.latitude = latitude;
  p.longitude = longitude;
  p.altitude = altitude;
  p.accuracy_m = accuracy;
  p.num_satellites = satellites;
  p.fix_quality = fix_quality;
  p.timestamp_ms = 0;
  return p;
}

/**
 * Calculate distance between two positions in meters (approximate)
 *
 * Uses Haversine formula for accuracy. For short distances, difference
 * in latitude/longitude scaled by ~111km per degree is sufficient.
 *
 * @param p1 First position
 * @param p2 Second position
 * @return Approximate distance in meters
 */
inline float PositionDistance(const PositionData& p1, const PositionData& p2) {
  // Simple approximation: 1 degree ≈ 111 km
  float lat_diff = (p1.latitude - p2.latitude) * 111000.0f;  // meters
  float lon_diff = (p1.longitude - p2.longitude) * 111000.0f; // meters

  // Account for longitude compression at higher latitudes
  float lat_avg = (p1.latitude + p2.latitude) / 2.0f;
  lon_diff *= std::cos(lat_avg * 3.14159f / 180.0f);

  return std::sqrt(lat_diff * lat_diff + lon_diff * lon_diff);
}

// ============================================================================
// Test Data Generators
// ============================================================================

/**
 * Known test locations (real GPS coordinates)
 */
namespace TestLocations {
  inline PositionData SanFrancisco() { return CreatePosition(37.7749, -122.4194, 10.5f); }
  inline PositionData Sydney() { return CreatePosition(-33.8688, 151.2093, 5.0f); }
  inline PositionData London() { return CreatePosition(51.5074, -0.1278, 15.0f); }
  inline PositionData Tokyo() { return CreatePosition(35.6762, 139.6503, 20.0f); }
  inline PositionData NorthPole() { return CreatePosition(90.0, 0.0, 1000.0f); }
  inline PositionData SouthPole() { return CreatePosition(-90.0, 0.0, -413.0f); }
  inline PositionData PrimeMeridian() { return CreatePosition(0.0, 0.0, 0.0f); }
  inline PositionData DateLine() { return CreatePosition(0.0, 180.0, 0.0f); }
  inline PositionData Equator() { return CreatePosition(0.0, -122.0, 0.0f); }
} // namespace TestLocations

/**
 * Test orientations (rotation angles)
 */
namespace TestOrientations {
  inline OrientationData NoRotation() { return QuaternionIdentity(3); }

  inline OrientationData Roll90() {
    // 90 degree roll around X axis
    return QuaternionFromAxisAngle(3.14159f / 2.0f, 1.0f, 0.0f, 0.0f);
  }

  inline OrientationData Pitch90() {
    // 90 degree pitch around Y axis
    return QuaternionFromAxisAngle(3.14159f / 2.0f, 0.0f, 1.0f, 0.0f);
  }

  inline OrientationData Yaw90() {
    // 90 degree yaw around Z axis
    return QuaternionFromAxisAngle(3.14159f / 2.0f, 0.0f, 0.0f, 1.0f);
  }

  inline OrientationData Arbitrary() {
    // 45 degree rotation around [1, 1, 1] axis
    return QuaternionFromAxisAngle(0.7854f, 0.577f, 0.577f, 0.577f);
  }

  inline OrientationData Uncalibrated() {
    OrientationData q = QuaternionIdentity(0); // Cal status = 0
    q.cal_accel = 0;
    q.cal_gyro = 0;
    q.cal_mag = 0;
    return q;
  }

  inline OrientationData PartiallyCalibrated() {
    OrientationData q = QuaternionIdentity(2); // Cal status = 2
    q.cal_accel = 2;
    q.cal_gyro = 1;
    q.cal_mag = 3;
    return q;
  }
} // namespace TestOrientations

// ============================================================================
// Assertion Helpers
// ============================================================================

/**
 * Check if quaternion components are finite (not NaN or Inf)
 *
 * @param q Quaternion to check
 * @return true if all components are finite
 */
inline bool IsQuaternionFinite(const OrientationData& q) {
  return std::isfinite(q.w) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z);
}

/**
 * Check if position components are finite
 *
 * @param p Position to check
 * @return true if all components are finite
 */
inline bool IsPositionFinite(const PositionData& p) {
  return std::isfinite(p.latitude) && std::isfinite(p.longitude) &&
         std::isfinite(p.altitude) && std::isfinite(p.accuracy_m);
}

/**
 * Compare two quaternions with tolerance
 *
 * Useful for testing quaternion operations.
 *
 * @param q1 First quaternion
 * @param q2 Second quaternion
 * @param tolerance Component tolerance
 * @return true if all components within tolerance
 */
inline bool QuaternionNear(const OrientationData& q1, const OrientationData& q2, float tolerance = 0.01f) {
  return std::abs(q1.w - q2.w) < tolerance && std::abs(q1.x - q2.x) < tolerance &&
         std::abs(q1.y - q2.y) < tolerance && std::abs(q1.z - q2.z) < tolerance;
}

/**
 * Compare two positions with tolerance
 *
 * @param p1 First position
 * @param p2 Second position
 * @param lat_tol Latitude tolerance (degrees)
 * @param lon_tol Longitude tolerance (degrees)
 * @param alt_tol Altitude tolerance (meters)
 * @return true if positions are within tolerance
 */
inline bool PositionNear(const PositionData& p1, const PositionData& p2, double lat_tol = 0.0001,
                         double lon_tol = 0.0001, float alt_tol = 1.0f) {
  return std::abs(p1.latitude - p2.latitude) < lat_tol &&
         std::abs(p1.longitude - p2.longitude) < lon_tol &&
         std::abs(p1.altitude - p2.altitude) < alt_tol;
}

#endif // TEST_HELPERS_H

#endif // DEBUG_MODE
