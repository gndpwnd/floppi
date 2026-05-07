#ifndef MEASUREMENT_MODEL_H
#define MEASUREMENT_MODEL_H

#include "../math/coordinates.h"
#include "state_dynamics.h"

/**
 * Measurement Model for Phase 3 Extended Kalman Filter
 *
 * Implements GPS measurement functions that convert between GPS (lat/lon/alt)
 * and NED coordinates, and compute the measurement Jacobian H for the filter.
 *
 * The measurement vector is z = [north, east, down] from position state.
 * The measurement function is: h(x) = [p_north, p_east, p_down] (extracts position from state)
 *
 * Measurement noise (R matrix):
 * - Diagonal elements: position variance from GPS HDOP
 * - Uses ekf_config.h parameters for scaling and bounds
 */

// ============================================================================
// GPS TO NED CONVERSION
// ============================================================================

/**
 * Convert GPS coordinates to NED relative to local frame origin
 *
 * Takes absolute GPS coordinates and converts them to a relative position
 * in the local North-East-Down frame centered at the origin.
 *
 * Algorithm:
 * 1. Validate GPS coordinates
 * 2. Convert GPS to ECEF using gps_to_ecef()
 * 3. Convert ECEF to NED relative to origin using ecef_to_ned()
 * 4. Return [north_m, east_m, down_m]
 *
 * @param lat_deg Latitude in degrees (-90 to +90)
 * @param lon_deg Longitude in degrees (-180 to +180)
 * @param alt_m Altitude above WGS84 ellipsoid in meters
 * @param origin_lat_deg Origin latitude in degrees
 * @param origin_lon_deg Origin longitude in degrees
 * @param origin_alt_m Origin altitude in meters
 * @param ned_out Output NED position [north_m, east_m, down_m]
 * @return true if conversion succeeded, false if coordinates are invalid
 *
 * @note Handles edge cases: poles, equator, antimeridian
 * @note Uses cached trigonometric values for origin (no recomputation)
 */
bool gps_to_ned_measurement(double lat_deg, double lon_deg, double alt_m,
                           double origin_lat_deg, double origin_lon_deg,
                           double origin_alt_m,
                           float ned_out[3]);

// ============================================================================
// NED TO GPS CONVERSION (INVERSE)
// ============================================================================

/**
 * Convert NED coordinates back to GPS (inverse operation)
 *
 * Converts from local North-East-Down frame back to absolute GPS coordinates.
 * Used for validation and round-trip accuracy testing.
 *
 * Algorithm:
 * 1. Convert NED to ECEF relative to origin using ned_to_ecef()
 * 2. Convert ECEF to GPS using ecef_to_gps()
 * 3. Return [lat_deg, lon_deg, alt_m]
 *
 * @param north_m North component in meters
 * @param east_m East component in meters
 * @param down_m Down component in meters
 * @param origin_lat_deg Origin latitude in degrees
 * @param origin_lon_deg Origin longitude in degrees
 * @param origin_alt_m Origin altitude in meters
 * @param gps_out Output GPS coordinates [lat, lon, alt]
 * @return true if conversion succeeded, false otherwise
 *
 * @note Should satisfy round-trip property:
 *       ned_to_gps(gps_to_ned(lat, lon, alt, orig), orig) ≈ (lat, lon, alt)
 */
bool ned_to_gps_measurement(float north_m, float east_m, float down_m,
                           double origin_lat_deg, double origin_lon_deg,
                           double origin_alt_m,
                           GPS_Data& gps_out);

// ============================================================================
// MEASUREMENT JACOBIAN
// ============================================================================

/**
 * Compute the measurement Jacobian matrix H
 *
 * H is the 3×16 matrix of partial derivatives of the measurement function
 * with respect to the state:
 *
 * h(x) = [p_north, p_east, p_down] = [x[7], x[8], x[9]]
 *
 * H[i][j] = ∂h[i]/∂x[j]
 *
 * Structure:
 * - H[0, 7] = 1.0 (dh[0]/dp[0])
 * - H[1, 8] = 1.0 (dh[1]/dp[1])
 * - H[2, 9] = 1.0 (dh[2]/dp[2])
 * - All other elements: 0.0
 *
 * In other words, H extracts the position vector from the state.
 *
 * @param H Output 3×16 measurement Jacobian (row-major, 48 elements)
 * @param state Current state vector (16 elements)
 *
 * @note The H matrix is sparse and constant (doesn't depend on state)
 * @note Memory layout: H[i*16 + j] = H[i][j] for row-major order
 * @note Implementation: Initialize to zero, then set three elements to 1.0
 */
void compute_H_matrix(float H[48],  // 3×16 = 48 elements
                      const float state[EKF_STATE_SIZE]);

// ============================================================================
// INNOVATION COMPUTATION
// ============================================================================

/**
 * Compute the measurement innovation (residual)
 *
 * Innovation = z_measured - h(x_predicted)
 *
 * Where:
 * - z_measured = [lat_gps, lon_gps, alt_gps] (GPS measurement)
 * - h(x) = [p_north, p_east, p_down] (predicted position from state)
 *
 * Algorithm:
 * 1. Extract position from state: p_predicted = [x[7], x[8], x[9]]
 * 2. Convert GPS measurement to NED: p_measured_ned = gps_to_ned(z_measured)
 * 3. Compute innovation: innovation = p_measured_ned - p_predicted
 * 4. Handle discontinuities (wrap angles if needed, though NED is continuous)
 *
 * @param lat_gps Measured GPS latitude (degrees)
 * @param lon_gps Measured GPS longitude (degrees)
 * @param alt_gps Measured GPS altitude (meters)
 * @param origin_lat_deg Local frame origin latitude (degrees)
 * @param origin_lon_deg Local frame origin longitude (degrees)
 * @param origin_alt_m Local frame origin altitude (meters)
 * @param state Current state vector (16 elements)
 * @param innovation_out Output innovation vector [Δnorth, Δeast, Δdown]
 * @return true if innovation computed successfully, false on error
 *
 * @note Innovation represents how well predicted position matches measurement
 * @note Large innovation indicates outlier (bad GPS fix) or filter divergence
 * @note Used in measurement update: x_new = x + K * innovation
 */
bool compute_innovation(double lat_gps, double lon_gps, double alt_gps,
                       double origin_lat_deg, double origin_lon_deg,
                       double origin_alt_m,
                       const float state[EKF_STATE_SIZE],
                       float innovation_out[3]);

// ============================================================================
// MEASUREMENT NOISE COVARIANCE
// ============================================================================

/**
 * Compute the measurement noise covariance matrix R (3×3)
 *
 * R is the covariance of the measurement noise for GPS position.
 * Diagonal structure with values computed from HDOP (Horizontal Dilution of Precision).
 *
 * Algorithm:
 * 1. Position variance (horizontal): R[i,i] = (HDOP_factor * HDOP)²
 *    Clamped to [R_POSITION_MIN, R_POSITION_MAX]
 * 2. Altitude variance: R[2,2] = (HDOP_factor_altitude * HDOP)²
 *    Clamped to [R_ALTITUDE_MIN, R_ALTITUDE_MAX]
 * 3. Off-diagonal elements: 0.0 (independent noise)
 *
 * @param R Output 3×3 measurement noise covariance (row-major, 9 elements)
 * @param hdop Horizontal Dilution of Precision (unitless)
 * @param vdop Vertical Dilution of Precision (unitless)
 *
 * @note HDOP ≈ 1-2 for good signal, 5+ for poor signal, >10 for very poor
 * @note Uses ekf_config.h parameters for scaling and bounds
 */
void compute_R_matrix(float R[9],   // 3×3 = 9 elements
                      float hdop, float vdop);

#endif  // MEASUREMENT_MODEL_H
