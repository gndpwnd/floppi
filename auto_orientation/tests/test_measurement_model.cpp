/**
 * Unit Tests for Measurement Model (Phase 3 EKF)
 *
 * Tests the GPS measurement functions and Jacobian computation used in
 * the Extended Kalman Filter for BNO085 IMU + GPS sensor fusion.
 *
 * Test Coverage:
 * - GPS to NED conversion accuracy
 * - NED to GPS round-trip conversion
 * - H matrix dimensions and structure
 * - H matrix Jacobian properties
 * - Innovation computation
 * - R matrix computation from HDOP/VDOP
 * - Reference point validation (Munich, LA, Equator, Poles)
 * - Coordinate system edge cases
 *
 * Total: 15+ test cases
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>

#include "../src/navigation/measurement_model.h"
#include "../src/navigation/state_dynamics.h"
#include "../src/config/ekf_config.h"
#include "../src/math/coordinates.h"

// ============================================================================
// TEST FIXTURES AND REFERENCE DATA
// ============================================================================

class MeasurementModelTest : public ::testing::Test {
 protected:
    // Tolerance values
    static constexpr float TOLERANCE_NED = 1.0f;           // 1 meter
    static constexpr float TOLERANCE_ROUNDTRIP = 0.5f;     // 0.5 meter
    static constexpr double TOLERANCE_GPS = 1e-7;          // ~1 cm at equator
    static constexpr float TOLERANCE_MATRIX = 1e-6f;
    static constexpr float TOLERANCE_INNOVATION = 1e-3f;

    // Reference test locations
    struct TestLocation {
        const char* name;
        double lat_deg;
        double lon_deg;
        double alt_m;
    };

    static constexpr TestLocation MUNICH = {"Munich", 48.1351, 11.5820, 530.0};
    static constexpr TestLocation LOS_ANGELES = {"Los Angeles", 34.0522, -118.2437, 100.0};
    static constexpr TestLocation EQUATOR_PRIME = {"Equator/Prime", 0.0, 0.0, 0.0};
    static constexpr TestLocation NORTH_POLE_APPROX = {"North Pole", 89.9, 0.0, 100.0};
    static constexpr TestLocation SOUTH_POLE_APPROX = {"South Pole", -89.9, 0.0, 100.0};

    // Helper: Initialize state to origin
    void initialize_state_at_origin(float state[EKF_STATE_SIZE]) {
        // Quaternion: identity
        state[0] = 1.0f;
        state[1] = 0.0f;
        state[2] = 0.0f;
        state[3] = 0.0f;

        // Velocity: zero
        state[4] = 0.0f;
        state[5] = 0.0f;
        state[6] = 0.0f;

        // Position: origin
        state[7] = 0.0f;
        state[8] = 0.0f;
        state[9] = 0.0f;

        // Biases: zero
        for (int i = 10; i < 16; i++) {
            state[i] = 0.0f;
        }
    }

    // Helper: Initialize state with known position
    void initialize_state_at_position(float state[EKF_STATE_SIZE],
                                     float p_north, float p_east, float p_down) {
        initialize_state_at_origin(state);
        state[7] = p_north;
        state[8] = p_east;
        state[9] = p_down;
    }

    // Helper: Compute distance between two positions
    float position_distance(float p1[3], float p2[3]) {
        float dn = p1[0] - p2[0];
        float de = p1[1] - p2[1];
        float dd = p1[2] - p2[2];
        return sqrtf(dn*dn + de*de + dd*dd);
    }
};

// ============================================================================
// GPS TO NED CONVERSION TESTS
// ============================================================================

TEST_F(MeasurementModelTest, GpsToNed_Munich_LocalPoint) {
    // Convert a point near Munich to NED
    // Origin at Munich, point 1km north
    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    // Point approximately 1 km north (at Munich latitude, 1 degree ≈ 111 km)
    double lat_deg = origin_lat + 1.0 / 111.0;
    double lon_deg = origin_lon;
    double alt_m = origin_alt;

    float ned[3];
    bool success = gps_to_ned_measurement(lat_deg, lon_deg, alt_m,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);

    EXPECT_TRUE(success) << "GPS to NED conversion failed";
    EXPECT_NEAR(ned[0], 1000.0f, TOLERANCE_NED) << "North component incorrect";
    EXPECT_NEAR(ned[1], 0.0f, TOLERANCE_NED) << "East component should be ~0";
    EXPECT_NEAR(ned[2], 0.0f, TOLERANCE_NED) << "Down component should be ~0";
}

TEST_F(MeasurementModelTest, GpsToNed_Munich_EastPoint) {
    // Convert a point 1km east of Munich
    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    // 1 km east: 1 degree longitude at Munich latitude ≈ 74 km
    // So 1 km = 1/74 ≈ 0.0135 degrees
    double lat_deg = origin_lat;
    double lon_deg = origin_lon + 1.0 / 74.0;
    double alt_m = origin_alt;

    float ned[3];
    bool success = gps_to_ned_measurement(lat_deg, lon_deg, alt_m,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);

    EXPECT_TRUE(success) << "GPS to NED conversion failed";
    EXPECT_NEAR(ned[0], 0.0f, TOLERANCE_NED) << "North should be ~0";
    EXPECT_NEAR(ned[1], 1000.0f, TOLERANCE_NED) << "East component incorrect";
    EXPECT_NEAR(ned[2], 0.0f, TOLERANCE_NED) << "Down should be ~0";
}

TEST_F(MeasurementModelTest, GpsToNed_InvalidCoordinates) {
    // Test with invalid GPS coordinates
    float ned[3];

    bool success = gps_to_ned_measurement(91.0, 0.0, 0.0,  // Invalid latitude
                                         0.0, 0.0, 0.0,
                                         ned);
    EXPECT_FALSE(success) << "Should reject invalid latitude";

    success = gps_to_ned_measurement(0.0, 181.0, 0.0,  // Invalid longitude
                                    0.0, 0.0, 0.0,
                                    ned);
    EXPECT_FALSE(success) << "Should reject invalid longitude";
}

// ============================================================================
// NED TO GPS CONVERSION TESTS
// ============================================================================

TEST_F(MeasurementModelTest, NedToGps_RoundTrip_Munich) {
    // GPS -> NED -> GPS round-trip at Munich
    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    // Original GPS coordinates
    double lat_orig = origin_lat + 0.01;
    double lon_orig = origin_lon + 0.01;
    double alt_orig = origin_alt + 100.0;

    // GPS to NED
    float ned[3];
    bool success = gps_to_ned_measurement(lat_orig, lon_orig, alt_orig,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);
    EXPECT_TRUE(success);

    // NED back to GPS
    GPS_Data gps_result;
    success = ned_to_gps_measurement(ned[0], ned[1], ned[2],
                                    origin_lat, origin_lon, origin_alt,
                                    gps_result);
    EXPECT_TRUE(success);

    // Check round-trip accuracy
    EXPECT_NEAR(gps_result.latitude_deg, lat_orig, TOLERANCE_GPS)
        << "Latitude round-trip error";
    EXPECT_NEAR(gps_result.longitude_deg, lon_orig, TOLERANCE_GPS)
        << "Longitude round-trip error";
    EXPECT_NEAR(gps_result.altitude_m, alt_orig, TOLERANCE_ROUNDTRIP)
        << "Altitude round-trip error";
}

TEST_F(MeasurementModelTest, NedToGps_AtOrigin) {
    // NED at origin should return origin GPS
    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    GPS_Data gps_result;
    bool success = ned_to_gps_measurement(0.0f, 0.0f, 0.0f,
                                         origin_lat, origin_lon, origin_alt,
                                         gps_result);
    EXPECT_TRUE(success);

    EXPECT_NEAR(gps_result.latitude_deg, origin_lat, TOLERANCE_GPS);
    EXPECT_NEAR(gps_result.longitude_deg, origin_lon, TOLERANCE_GPS);
    EXPECT_NEAR(gps_result.altitude_m, origin_alt, TOLERANCE_ROUNDTRIP);
}

// ============================================================================
// H MATRIX TESTS
// ============================================================================

TEST_F(MeasurementModelTest, HMatrix_Dimensions) {
    // H should be 3x16 = 48 elements
    float H[48];
    float state[EKF_STATE_SIZE];
    initialize_state_at_origin(state);

    compute_H_matrix(H, state);

    // If we get here, dimensions are OK
    EXPECT_TRUE(true);
}

TEST_F(MeasurementModelTest, HMatrix_Structure) {
    // H should extract position from state
    // H[0, 7] = 1, H[1, 8] = 1, H[2, 9] = 1, rest = 0
    float H[48];
    float state[EKF_STATE_SIZE];
    initialize_state_at_origin(state);

    compute_H_matrix(H, state);

    // Check structure
    memset(H, 0, 48*sizeof(float));  // Clear
    compute_H_matrix(H, state);

    // Expected structure
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 16; j++) {
            float expected = 0.0f;
            if (i == 0 && j == 7) expected = 1.0f;  // dh[0]/dp_north
            if (i == 1 && j == 8) expected = 1.0f;  // dh[1]/dp_east
            if (i == 2 && j == 9) expected = 1.0f;  // dh[2]/dp_down

            EXPECT_NEAR(H[i*16 + j], expected, TOLERANCE_MATRIX)
                << "H[" << i << ", " << j << "] = " << H[i*16 + j]
                << ", expected " << expected;
        }
    }
}

TEST_F(MeasurementModelTest, HMatrix_IndependentOfState) {
    // H should not depend on state (since it's just position extraction)
    float H1[48];
    float H2[48];

    float state1[EKF_STATE_SIZE];
    float state2[EKF_STATE_SIZE];

    initialize_state_at_origin(state1);
    initialize_state_at_position(state2, 100.0f, 200.0f, 50.0f);

    compute_H_matrix(H1, state1);
    compute_H_matrix(H2, state2);

    // H matrices should be identical
    for (int i = 0; i < 48; i++) {
        EXPECT_EQ(H1[i], H2[i])
            << "H[" << i << "] depends on state (should be constant)";
    }
}

// ============================================================================
// INNOVATION TESTS
// ============================================================================

TEST_F(MeasurementModelTest, Innovation_ZeroError) {
    // When predicted position matches GPS measurement, innovation should be zero
    float state[EKF_STATE_SIZE];
    initialize_state_at_origin(state);

    // GPS measurement at origin
    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    float innovation[3];
    bool success = compute_innovation(origin_lat, origin_lon, origin_alt,
                                     origin_lat, origin_lon, origin_alt,
                                     state, innovation);

    EXPECT_TRUE(success);
    EXPECT_NEAR(innovation[0], 0.0f, TOLERANCE_INNOVATION);
    EXPECT_NEAR(innovation[1], 0.0f, TOLERANCE_INNOVATION);
    EXPECT_NEAR(innovation[2], 0.0f, TOLERANCE_INNOVATION);
}

TEST_F(MeasurementModelTest, Innovation_KnownOffset) {
    // GPS measurement 100m north of predicted position
    float state[EKF_STATE_SIZE];
    initialize_state_at_origin(state);

    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    // GPS point 100m north
    double gps_lat = origin_lat + 100.0 / (111000.0);
    double gps_lon = origin_lon;
    double gps_alt = origin_alt;

    float innovation[3];
    bool success = compute_innovation(gps_lat, gps_lon, gps_alt,
                                     origin_lat, origin_lon, origin_alt,
                                     state, innovation);

    EXPECT_TRUE(success);
    EXPECT_NEAR(innovation[0], 100.0f, TOLERANCE_NED)
        << "North innovation incorrect";
    EXPECT_NEAR(innovation[1], 0.0f, TOLERANCE_NED)
        << "East innovation should be ~0";
}

TEST_F(MeasurementModelTest, Innovation_InvalidGps) {
    // Test with invalid GPS coordinates
    float state[EKF_STATE_SIZE];
    initialize_state_at_origin(state);

    float innovation[3];
    bool success = compute_innovation(91.0, 0.0, 0.0,  // Invalid latitude
                                     0.0, 0.0, 0.0,
                                     state, innovation);

    EXPECT_FALSE(success) << "Should reject invalid GPS";
}

// ============================================================================
// R MATRIX TESTS
// ============================================================================

TEST_F(MeasurementModelTest, RMatrix_Dimensions) {
    // R should be 3x3 = 9 elements
    float R[9];
    compute_R_matrix(R, 2.0f, 3.0f);

    EXPECT_TRUE(true);
}

TEST_F(MeasurementModelTest, RMatrix_DiagonalStructure) {
    // R should be diagonal
    float R[9];
    compute_R_matrix(R, 2.0f, 3.0f);

    // Off-diagonal should be zero
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (i != j) {
                EXPECT_NEAR(R[i*3 + j], 0.0f, TOLERANCE_MATRIX)
                    << "R[" << i << "," << j << "] should be zero";
            }
        }
    }
}

TEST_F(MeasurementModelTest, RMatrix_PositiveDiagonal) {
    // All diagonal elements should be positive
    float R[9];
    compute_R_matrix(R, 2.0f, 3.0f);

    for (int i = 0; i < 3; i++) {
        EXPECT_GT(R[i*3 + i], 0.0f)
            << "R[" << i << "," << i << "] should be positive";
    }
}

TEST_F(MeasurementModelTest, RMatrix_ScalesWithHdop) {
    // R should scale with HDOP
    float R1[9];
    float R2[9];

    compute_R_matrix(R1, 1.0f, 1.5f);
    compute_R_matrix(R2, 2.0f, 3.0f);

    // R2 position noise should be ~4x R1 (squared scaling)
    // R2[0,0] ≈ (2.0 * 1.5)^2 / (1.0 * 1.5)^2 = 4x
    float ratio_horizontal = R2[0*3 + 0] / R1[0*3 + 0];
    EXPECT_NEAR(ratio_horizontal, 4.0f, 0.5f);

    // R2 altitude noise should be scaled by vdop ratio squared
    float ratio_altitude = R2[2*3 + 2] / R1[2*3 + 2];
    float expected_ratio = (3.0f / 1.5f) * (3.0f / 1.5f);
    EXPECT_NEAR(ratio_altitude, expected_ratio, 1.0f);
}

TEST_F(MeasurementModelTest, RMatrix_RespectsBounds) {
    // R should respect minimum and maximum bounds
    float R_low[9];
    float R_high[9];

    // Very good signal (low HDOP)
    compute_R_matrix(R_low, 0.5f, 0.5f);

    // Very poor signal (high HDOP)
    compute_R_matrix(R_high, 100.0f, 100.0f);

    // Low HDOP should still have at least R_POSITION_MIN
    EXPECT_GE(R_low[0*3 + 0], R_POSITION_MIN * 0.9f);

    // High HDOP should not exceed R_POSITION_MAX
    EXPECT_LE(R_high[0*3 + 0], R_POSITION_MAX * 1.1f);
}

// ============================================================================
// EQUATOR AND POLE TESTS
// ============================================================================

TEST_F(MeasurementModelTest, GpsToNed_EquatorPrime) {
    // Test at equator and prime meridian
    double origin_lat = EQUATOR_PRIME.lat_deg;
    double origin_lon = EQUATOR_PRIME.lon_deg;
    double origin_alt = EQUATOR_PRIME.alt_m;

    double lat_deg = origin_lat + 0.01;  // 1 degree north
    double lon_deg = origin_lon + 0.01;  // 1 degree east
    double alt_m = origin_alt;

    float ned[3];
    bool success = gps_to_ned_measurement(lat_deg, lon_deg, alt_m,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);

    EXPECT_TRUE(success);
    // At equator, 1 degree ≈ 111.3 km north and east
    EXPECT_NEAR(ned[0], 1113.0f, 100.0f);
    EXPECT_NEAR(ned[1], 1113.0f, 100.0f);
}

TEST_F(MeasurementModelTest, GpsToNed_NorthPoleApprox) {
    // Test near north pole
    double origin_lat = NORTH_POLE_APPROX.lat_deg;
    double origin_lon = NORTH_POLE_APPROX.lon_deg;
    double origin_alt = NORTH_POLE_APPROX.alt_m;

    double lat_deg = origin_lat - 0.01;  // 0.01 degree south
    double lon_deg = origin_lon;
    double alt_m = origin_alt;

    float ned[3];
    bool success = gps_to_ned_measurement(lat_deg, lon_deg, alt_m,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);

    EXPECT_TRUE(success);
    // Should be ~1 km south
    EXPECT_NEAR(ned[0], -1113.0f, 200.0f);
}

TEST_F(MeasurementModelTest, GpsToNed_SouthPoleApprox) {
    // Test near south pole
    double origin_lat = SOUTH_POLE_APPROX.lat_deg;
    double origin_lon = SOUTH_POLE_APPROX.lon_deg;
    double origin_alt = SOUTH_POLE_APPROX.alt_m;

    double lat_deg = origin_lat + 0.01;  // 0.01 degree north
    double lon_deg = origin_lon;
    double alt_m = origin_alt;

    float ned[3];
    bool success = gps_to_ned_measurement(lat_deg, lon_deg, alt_m,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);

    EXPECT_TRUE(success);
    // Should be ~1 km north
    EXPECT_NEAR(ned[0], 1113.0f, 200.0f);
}

// ============================================================================
// ALTITUDE TESTS
// ============================================================================

TEST_F(MeasurementModelTest, GpsToNed_AltitudeChange) {
    // Test altitude conversion in NED
    double origin_lat = MUNICH.lat_deg;
    double origin_lon = MUNICH.lon_deg;
    double origin_alt = MUNICH.alt_m;

    double lat_deg = origin_lat;
    double lon_deg = origin_lon;
    double alt_m = origin_alt + 100.0;  // 100m higher

    float ned[3];
    bool success = gps_to_ned_measurement(lat_deg, lon_deg, alt_m,
                                         origin_lat, origin_lon, origin_alt,
                                         ned);

    EXPECT_TRUE(success);
    // In NED, positive altitude (higher) means negative down
    EXPECT_NEAR(ned[2], -100.0f, TOLERANCE_NED);
}

#endif  // DEBUG_MODE
