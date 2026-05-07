#include "measurement_model.h"
#include "../config/ekf_config.h"
#include <math.h>
#include <cstring>

// ============================================================================
// GPS TO NED CONVERSION
// ============================================================================

bool gps_to_ned_measurement(double lat_deg, double lon_deg, double alt_m,
                           double origin_lat_deg, double origin_lon_deg,
                           double origin_alt_m,
                           float ned_out[3]) {
    // Validate input coordinates
    if (!is_valid_gps(lat_deg, lon_deg, alt_m)) {
        return false;
    }
    if (!is_valid_gps(origin_lat_deg, origin_lon_deg, origin_alt_m)) {
        return false;
    }

    // Convert GPS to ECEF
    ECEF point_ecef = gps_to_ecef(lat_deg, lon_deg, alt_m);
    if (!is_valid_ecef(point_ecef)) {
        return false;
    }

    // Convert origin to ECEF
    ECEF origin_ecef = gps_to_ecef(origin_lat_deg, origin_lon_deg, origin_alt_m);
    if (!is_valid_ecef(origin_ecef)) {
        return false;
    }

    // Origin latitude in radians
    double origin_lat_rad = origin_lat_deg * M_PI / 180.0;

    // Convert ECEF to NED
    LocalFrame ned = ecef_to_ned(point_ecef, origin_ecef, origin_lat_rad);

    // Check for NaN in result
    if (isnan(ned.north_m) || isnan(ned.east_m) || isnan(ned.down_m)) {
        return false;
    }

    // Copy to output
    ned_out[0] = (float)ned.north_m;
    ned_out[1] = (float)ned.east_m;
    ned_out[2] = (float)ned.down_m;

    return true;
}

// ============================================================================
// NED TO GPS CONVERSION
// ============================================================================

bool ned_to_gps_measurement(float north_m, float east_m, float down_m,
                           double origin_lat_deg, double origin_lon_deg,
                           double origin_alt_m,
                           GPS_Data& gps_out) {
    // Validate origin
    if (!is_valid_gps(origin_lat_deg, origin_lon_deg, origin_alt_m)) {
        return false;
    }

    // Create LocalFrame from NED
    LocalFrame ned_point(north_m, east_m, down_m);

    // Convert origin to ECEF
    ECEF origin_ecef = gps_to_ecef(origin_lat_deg, origin_lon_deg, origin_alt_m);
    if (!is_valid_ecef(origin_ecef)) {
        return false;
    }

    // Origin latitude in radians
    double origin_lat_rad = origin_lat_deg * M_PI / 180.0;

    // Convert NED to ECEF
    ECEF point_ecef = ned_to_ecef(ned_point, origin_ecef, origin_lat_rad);
    if (!is_valid_ecef(point_ecef)) {
        return false;
    }

    // Convert ECEF to GPS
    GPS_Data gps = ecef_to_gps(point_ecef.x, point_ecef.y, point_ecef.z);

    // Check for valid result
    if (!is_valid_gps(gps.latitude_deg, gps.longitude_deg, gps.altitude_m)) {
        return false;
    }

    gps_out = gps;
    return true;
}

// ============================================================================
// MEASUREMENT JACOBIAN: H MATRIX
// ============================================================================

void compute_H_matrix(float H[48], const float state[EKF_STATE_SIZE]) {
    // Initialize H to zero (sparse matrix)
    memset(H, 0, 48*sizeof(float));

    // H is the Jacobian of the measurement function h(x) = [p_north, p_east, p_down]
    // with respect to state x = [q0, q1, q2, q3, vn, ve, vd, pn, pe, pd, ab0, ab1, ab2, gb0, gb1, gb2]
    //
    // Since h(x) simply extracts three position components:
    // h(x) = [x[7], x[8], x[9]]
    //
    // The Jacobian is:
    // H[0, 7] = 1.0
    // H[1, 8] = 1.0
    // H[2, 9] = 1.0
    // All other elements are 0.0

    H[0*16 + 7] = 1.0f;  // dh[0]/dp_north
    H[1*16 + 8] = 1.0f;  // dh[1]/dp_east
    H[2*16 + 9] = 1.0f;  // dh[2]/dp_down

    // (state parameter is unused but kept for signature consistency with potential future extensions)
    (void)state;
}

// ============================================================================
// INNOVATION COMPUTATION
// ============================================================================

bool compute_innovation(double lat_gps, double lon_gps, double alt_gps,
                       double origin_lat_deg, double origin_lon_deg,
                       double origin_alt_m,
                       const float state[EKF_STATE_SIZE],
                       float innovation_out[3]) {
    // Extract predicted position from state
    float p_predicted[3];
    p_predicted[0] = state[7];  // p_north
    p_predicted[1] = state[8];  // p_east
    p_predicted[2] = state[9];  // p_down

    // Convert GPS measurement to NED
    float p_measured_ned[3];
    bool success = gps_to_ned_measurement(lat_gps, lon_gps, alt_gps,
                                         origin_lat_deg, origin_lon_deg,
                                         origin_alt_m,
                                         p_measured_ned);
    if (!success) {
        return false;
    }

    // Compute innovation: z_measured - h(x_predicted)
    innovation_out[0] = p_measured_ned[0] - p_predicted[0];  // Δnorth
    innovation_out[1] = p_measured_ned[1] - p_predicted[1];  // Δeast
    innovation_out[2] = p_measured_ned[2] - p_predicted[2];  // Δdown

    return true;
}

// ============================================================================
// MEASUREMENT NOISE COVARIANCE: R MATRIX
// ============================================================================

void compute_R_matrix(float R[9], float hdop, float vdop) {
    // Initialize R to zero
    memset(R, 0, 9*sizeof(float));

    // Clamp HDOP and VDOP to reasonable ranges
    if (hdop < 1.0f) hdop = 1.0f;
    if (vdop < 1.0f) vdop = 1.0f;

    // Compute position variance from HDOP
    // R_position = (HDOP_factor * HDOP)²
    float var_position = R_POSITION_HDOP_FACTOR * hdop;
    var_position = var_position * var_position;

    // Clamp to bounds
    if (var_position < R_POSITION_MIN) var_position = R_POSITION_MIN;
    if (var_position > R_POSITION_MAX) var_position = R_POSITION_MAX;

    // Compute altitude variance from VDOP
    // R_altitude = (VDOP_factor * VDOP)²
    float var_altitude = R_ALTITUDE_HDOP_FACTOR * vdop;
    var_altitude = var_altitude * var_altitude;

    // Clamp to bounds
    if (var_altitude < R_ALTITUDE_MIN) var_altitude = R_ALTITUDE_MIN;
    if (var_altitude > R_ALTITUDE_MAX) var_altitude = R_ALTITUDE_MAX;

    // Diagonal structure: north, east, down
    R[0*3 + 0] = var_position;      // North variance
    R[1*3 + 1] = var_position;      // East variance
    R[2*3 + 2] = var_altitude;      // Down (altitude) variance

    // Off-diagonal elements are zero (independent noise)
}
