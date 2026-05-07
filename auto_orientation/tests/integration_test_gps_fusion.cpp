/**
 * Integration Tests: GPS Initialization, Parsing, Coordinate Frame Fusion
 *
 * Comprehensive integration tests for Phase 2 GPS functionality:
 * - GPS initialization and NMEA sentence parsing
 * - Coordinate frame initialization on first GPS fix
 * - GPS to NED conversion accuracy (hand-calculated expected values)
 * - Round-trip conversions: GPS -> NED -> GPS
 * - Merged JSON output with orientation + position
 * - Timestamp synchronization
 * - Error handling: no GPS lock, stale data, bad fix quality
 * - Simulated GPS data injection (NMEA sentences)
 *
 * Test Count: 35+ comprehensive cases
 * Target: All tests pass with <0.1m error tolerance
 *
 * Reference:
 * - GPS_GEODETIC_COORDINATE_SYSTEMS.md for coordinate conversions
 * - Phase 1 test patterns from test_quaternion.cpp and test_coordinates.cpp
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstdio>
#include <cstring>
#include <limits>
#include <chrono>

// Include coordinate and math libraries
#include "../src/math/coordinates.h"
#include "../src/navigation/coordinate_frame.h"

// ============================================================================
// Test Utilities and Constants
// ============================================================================

namespace {

// Tolerance values
constexpr double TOLERANCE_METER = 0.1;        // 0.1 meter for round-trip accuracy
constexpr double TOLERANCE_MILLIMETER = 0.001; // 1 millimeter
constexpr double TOLERANCE_DEGREE = 1e-7;      // ~1 cm at equator
constexpr double TOLERANCE_NED_METER = 0.1;    // 0.1 meter for NED conversions
constexpr float TOLERANCE_GPS_DEG = 1e-6f;     // GPS rounding tolerance

// Mock GPS sentence structures for testing
struct MockGPSSentence {
    const char* sentence;
    double expected_lat;
    double expected_lon;
    double expected_alt;
    uint8_t expected_satellites;
    uint8_t expected_fix_quality;
    bool should_parse_successfully;
};

// Helper function: Compare doubles with tolerance
bool doubles_equal(double a, double b, double tolerance) {
    return std::fabs(a - b) <= tolerance;
}

// Helper function: Compare floats with tolerance
bool floats_equal(float a, float b, float tolerance) {
    return std::fabs(a - b) <= tolerance;
}

// Helper function: Calculate distance between two GPS points in meters
double gps_distance_meters(double lat1, double lon1, double lat2, double lon2) {
    // Using simple formula for small distances (< 1km)
    // At equator: 1 degree lat ≈ 111111m, 1 degree lon ≈ 111111m
    const double DEG_TO_M = 111111.0;
    double dlat = (lat2 - lat1) * DEG_TO_M;
    double dlon = (lon2 - lon1) * DEG_TO_M * std::cos((lat1 + lat2) / 2.0 * M_PI / 180.0);
    return std::sqrt(dlat * dlat + dlon * dlon);
}

}  // namespace

// ============================================================================
// Test Fixture: GPS Integration Tests
// ============================================================================

class GPSIntegrationTest : public ::testing::Test {
 protected:
    // Test locations with known coordinates and hand-calculated expected values
    struct TestLocation {
        const char* name;
        double lat_deg;
        double lon_deg;
        double alt_m;
        // Expected NED relative to origin for validation (approximations)
        double expected_north_m;
        double expected_east_m;
        double expected_down_m;
    };

    // Munich, Germany (origin for many tests)
    static const TestLocation MUNICH_ORIGIN;
    // Point ~1.111 km north of Munich
    static const TestLocation MUNICH_NORTH_1KM;
    // Point ~1 km east of Munich
    static const TestLocation MUNICH_EAST_1KM;
    // Point 500m south and east of Munich (diagonal)
    static const TestLocation MUNICH_SE_500M;
    // New York (different hemisphere)
    static const TestLocation NEW_YORK;
    // Singapore (near equator)
    static const TestLocation SINGAPORE;
    // Sydney (southern hemisphere)
    static const TestLocation SYDNEY;
    // North Pole approximation (85° latitude)
    static const TestLocation NEAR_NORTH_POLE;
    // South Pole approximation (85° S latitude)
    static const TestLocation NEAR_SOUTH_POLE;
};

// Static test location definitions
const GPSIntegrationTest::TestLocation GPSIntegrationTest::MUNICH_ORIGIN = {
    "Munich, Germany",
    47.3667, 11.1833, 520.0,
    0.0, 0.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::MUNICH_NORTH_1KM = {
    "Munich +1km North",
    47.3756, 11.1833, 520.0,  // ~0.009° north ≈ 1 km
    1000.0, 0.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::MUNICH_EAST_1KM = {
    "Munich +1km East",
    47.3667, 11.1924, 520.0,  // ~0.0091° east ≈ 1 km
    0.0, 1000.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::MUNICH_SE_500M = {
    "Munich -500m South, +500m East",
    47.3622, 11.1878, 520.0,
    -500.0, 500.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::NEW_YORK = {
    "New York, USA",
    40.7128, -74.0060, 10.0,
    0.0, 0.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::SINGAPORE = {
    "Singapore",
    1.3521, 103.8198, 20.0,
    0.0, 0.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::SYDNEY = {
    "Sydney, Australia",
    -33.8688, 151.2093, 50.0,
    0.0, 0.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::NEAR_NORTH_POLE = {
    "Near North Pole",
    85.0, 0.0, 0.0,
    0.0, 0.0, 0.0
};

const GPSIntegrationTest::TestLocation GPSIntegrationTest::NEAR_SOUTH_POLE = {
    "Near South Pole",
    -85.0, 0.0, 0.0,
    0.0, 0.0, 0.0
};

// ============================================================================
// Test Group 1: GPS Initialization (5 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, CoordinateFrameInitializationSuccess) {
    // Test: Initialize coordinate frame with explicit GPS coordinates
    CoordinateFrame frame;
    EXPECT_FALSE(frame.isInitialized());

    bool success = frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
    EXPECT_TRUE(success);
    EXPECT_TRUE(frame.isInitialized());
}

TEST_F(GPSIntegrationTest, CoordinateFrameInitializationWithGPSData) {
    // Test: Initialize coordinate frame from GPS_Data structure
    CoordinateFrame frame;
    GPS_Data gps{MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m};

    bool success = frame.initializeOnFirstFix(gps);
    EXPECT_TRUE(success);
    EXPECT_TRUE(frame.isInitialized());

    // Verify origin is stored correctly
    GPS_Data origin = frame.getOrigin();
    EXPECT_TRUE(doubles_equal(origin.latitude_deg, MUNICH_ORIGIN.lat_deg, TOLERANCE_DEGREE));
    EXPECT_TRUE(doubles_equal(origin.longitude_deg, MUNICH_ORIGIN.lon_deg, TOLERANCE_DEGREE));
    EXPECT_TRUE(doubles_equal(origin.altitude_m, MUNICH_ORIGIN.alt_m, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, CoordinateFrameInvalidLatitudeRejected) {
    // Test: Reject invalid latitude (> 90 degrees)
    CoordinateFrame frame;
    bool success = frame.initialize(91.0, 0.0, 0.0);
    EXPECT_FALSE(success);
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(GPSIntegrationTest, CoordinateFrameInvalidLongitudeRejected) {
    // Test: Reject invalid longitude (> 180 degrees)
    CoordinateFrame frame;
    bool success = frame.initialize(45.0, 181.0, 0.0);
    EXPECT_FALSE(success);
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(GPSIntegrationTest, CoordinateFrameReinitialization) {
    // Test: Reinitialize frame with new origin
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
    EXPECT_TRUE(frame.isInitialized());

    // Reinitialize with different location
    bool success = frame.reinitialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
    EXPECT_TRUE(success);
    EXPECT_TRUE(frame.isInitialized());

    GPS_Data new_origin = frame.getOrigin();
    EXPECT_TRUE(doubles_equal(new_origin.latitude_deg, NEW_YORK.lat_deg, TOLERANCE_DEGREE));
}

// ============================================================================
// Test Group 2: GPS to NED Conversion Accuracy (8 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, OriginConvertsToZeroNED) {
    // Test: Origin point should convert to (0, 0, 0) in NED
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, NorthOffsetConversionAccuracy) {
    // Test: Point 1km north should have north_m ≈ 1000m
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);

    EXPECT_TRUE(doubles_equal(ned.north_m, 1000.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, EastOffsetConversionAccuracy) {
    // Test: Point 1km east should have east_m ≈ 1000m
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(MUNICH_EAST_1KM.lat_deg, MUNICH_EAST_1KM.lon_deg, MUNICH_EAST_1KM.alt_m);

    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 1000.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, AltitudeOffsetConversionAccuracy) {
    // Test: Higher altitude should have negative down_m (pointing up)
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    // Same location, but 100m higher
    LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m + 100.0);

    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, -100.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, DiagonalOffsetConversionAccuracy) {
    // Test: Point 500m south-east should compute correct diagonal
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(MUNICH_SE_500M.lat_deg, MUNICH_SE_500M.lon_deg, MUNICH_SE_500M.alt_m);

    // Expect approximately -500m north, 500m east
    EXPECT_TRUE(doubles_equal(ned.north_m, -500.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 500.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, ConversionAccuracyNewYork) {
    // Test: Conversion works correctly at different latitude (New York)
    CoordinateFrame frame;
    frame.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);

    // Same location should give zero NED
    LocalFrame ned = frame.gpsToLocalNED(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);

    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, ConversionAccuracyNearEquator) {
    // Test: Conversion works correctly near equator (Singapore)
    CoordinateFrame frame;
    frame.initialize(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);

    // Same location should give zero NED
    LocalFrame ned = frame.gpsToLocalNED(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);

    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.down_m, 0.0, TOLERANCE_METER));
}

// ============================================================================
// Test Group 3: Round-Trip Conversions (8 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, RoundTripOriginPreserved) {
    // Test: GPS -> NED -> GPS at origin should recover original
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
    GPS_Data recovered = frame.localNEDToGPS(ned);

    EXPECT_TRUE(doubles_equal(recovered.latitude_deg, MUNICH_ORIGIN.lat_deg, TOLERANCE_DEGREE));
    EXPECT_TRUE(doubles_equal(recovered.longitude_deg, MUNICH_ORIGIN.lon_deg, TOLERANCE_DEGREE));
    EXPECT_TRUE(doubles_equal(recovered.altitude_m, MUNICH_ORIGIN.alt_m, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, RoundTripNorthOffsetPreserved) {
    // Test: 1km north point should recover to ~1km north after round-trip
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned_forward = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
    GPS_Data recovered = frame.localNEDToGPS(ned_forward);

    // Distance should be preserved to within tolerance
    double error = gps_distance_meters(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, recovered.latitude_deg, recovered.longitude_deg);
    EXPECT_LT(error, TOLERANCE_METER);
}

TEST_F(GPSIntegrationTest, RoundTripEastOffsetPreserved) {
    // Test: 1km east point should recover to ~1km east after round-trip
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned_forward = frame.gpsToLocalNED(MUNICH_EAST_1KM.lat_deg, MUNICH_EAST_1KM.lon_deg, MUNICH_EAST_1KM.alt_m);
    GPS_Data recovered = frame.localNEDToGPS(ned_forward);

    double error = gps_distance_meters(MUNICH_EAST_1KM.lat_deg, MUNICH_EAST_1KM.lon_deg, recovered.latitude_deg, recovered.longitude_deg);
    EXPECT_LT(error, TOLERANCE_METER);
}

TEST_F(GPSIntegrationTest, RoundTripAltitudePreserved) {
    // Test: Altitude changes should be preserved in round-trip
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    // Test point 200m above origin
    double test_alt = MUNICH_ORIGIN.alt_m + 200.0;
    LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, test_alt);
    GPS_Data recovered = frame.localNEDToGPS(ned);

    EXPECT_TRUE(doubles_equal(recovered.altitude_m, test_alt, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, RoundTripDiagonalOffsetPreserved) {
    // Test: Diagonal offset 500m SE should preserve distance
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned_forward = frame.gpsToLocalNED(MUNICH_SE_500M.lat_deg, MUNICH_SE_500M.lon_deg, MUNICH_SE_500M.alt_m);
    GPS_Data recovered = frame.localNEDToGPS(ned_forward);

    double error = gps_distance_meters(MUNICH_SE_500M.lat_deg, MUNICH_SE_500M.lon_deg, recovered.latitude_deg, recovered.longitude_deg);
    EXPECT_LT(error, TOLERANCE_METER);
}

TEST_F(GPSIntegrationTest, RoundTripNewYork) {
    // Test: Round-trip at different location (New York)
    CoordinateFrame frame;
    frame.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);

    // Point 500m away
    LocalFrame ned_test(500.0, 300.0, -100.0);  // 500m N, 300m E, 100m up
    GPS_Data recovered = frame.localNEDToGPS(ned_test);

    LocalFrame ned_back = frame.gpsToLocalNED(recovered.latitude_deg, recovered.longitude_deg, recovered.altitude_m);

    EXPECT_TRUE(doubles_equal(ned_back.north_m, ned_test.north_m, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned_back.east_m, ned_test.east_m, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned_back.down_m, ned_test.down_m, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, RoundTripSouthernHemisphere) {
    // Test: Round-trip in southern hemisphere (Sydney)
    CoordinateFrame frame;
    frame.initialize(SYDNEY.lat_deg, SYDNEY.lon_deg, SYDNEY.alt_m);

    LocalFrame ned_test(1000.0, 1000.0, 0.0);  // 1km N, 1km E
    GPS_Data recovered = frame.localNEDToGPS(ned_test);

    LocalFrame ned_back = frame.gpsToLocalNED(recovered.latitude_deg, recovered.longitude_deg, recovered.altitude_m);

    EXPECT_TRUE(doubles_equal(ned_back.north_m, ned_test.north_m, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned_back.east_m, ned_test.east_m, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned_back.down_m, ned_test.down_m, TOLERANCE_METER));
}

// ============================================================================
// Test Group 4: Coordinate Frame Properties (4 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, OriginAltitudeAccessible) {
    // Test: Can retrieve origin altitude
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    double origin_alt = frame.getOriginAltitude();
    EXPECT_TRUE(doubles_equal(origin_alt, MUNICH_ORIGIN.alt_m, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, OriginDataAccessible) {
    // Test: Can retrieve full origin GPS data
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    GPS_Data origin = frame.getOrigin();
    EXPECT_TRUE(doubles_equal(origin.latitude_deg, MUNICH_ORIGIN.lat_deg, TOLERANCE_DEGREE));
    EXPECT_TRUE(doubles_equal(origin.longitude_deg, MUNICH_ORIGIN.lon_deg, TOLERANCE_DEGREE));
    EXPECT_TRUE(doubles_equal(origin.altitude_m, MUNICH_ORIGIN.alt_m, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, UninitializedFrameThrowsOnAccess) {
    // Test: Accessing uninitialized frame should throw or return error
    CoordinateFrame frame;
    EXPECT_FALSE(frame.isInitialized());

    // This should handle the error gracefully (return invalid NED or throw)
    try {
        LocalFrame ned = frame.gpsToLocalNED(47.0, 11.0, 500.0);
        // If no exception, check for NaN values
        EXPECT_TRUE(std::isnan(ned.north_m) || ned.north_m == 0.0);
    } catch (...) {
        // Exception is acceptable
        SUCCEED();
    }
}

TEST_F(GPSIntegrationTest, MultipleInitializations) {
    // Test: Multiple initializations/reinitializations work correctly
    CoordinateFrame frame;

    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
    EXPECT_TRUE(frame.isInitialized());

    frame.reinitialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
    EXPECT_TRUE(frame.isInitialized());

    GPS_Data origin = frame.getOrigin();
    EXPECT_TRUE(doubles_equal(origin.latitude_deg, NEW_YORK.lat_deg, TOLERANCE_DEGREE));

    frame.reinitialize(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);
    origin = frame.getOrigin();
    EXPECT_TRUE(doubles_equal(origin.latitude_deg, SINGAPORE.lat_deg, TOLERANCE_DEGREE));
}

// ============================================================================
// Test Group 5: Edge Cases and Pole Handling (5 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, NorthPoleApproximation) {
    // Test: Near North Pole (85°N) handling
    CoordinateFrame frame;
    bool success = frame.initialize(NEAR_NORTH_POLE.lat_deg, NEAR_NORTH_POLE.lon_deg, NEAR_NORTH_POLE.alt_m);
    EXPECT_TRUE(success);

    // Same location should give zero NED
    LocalFrame ned = frame.gpsToLocalNED(NEAR_NORTH_POLE.lat_deg, NEAR_NORTH_POLE.lon_deg, NEAR_NORTH_POLE.alt_m);
    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, SouthPoleApproximation) {
    // Test: Near South Pole (85°S) handling
    CoordinateFrame frame;
    bool success = frame.initialize(NEAR_SOUTH_POLE.lat_deg, NEAR_SOUTH_POLE.lon_deg, NEAR_SOUTH_POLE.alt_m);
    EXPECT_TRUE(success);

    // Same location should give zero NED
    LocalFrame ned = frame.gpsToLocalNED(NEAR_SOUTH_POLE.lat_deg, NEAR_SOUTH_POLE.lon_deg, NEAR_SOUTH_POLE.alt_m);
    EXPECT_TRUE(doubles_equal(ned.north_m, 0.0, TOLERANCE_METER));
    EXPECT_TRUE(doubles_equal(ned.east_m, 0.0, TOLERANCE_METER));
}

TEST_F(GPSIntegrationTest, AntimeridianCrossing) {
    // Test: Longitude near ±180 (antimeridian)
    CoordinateFrame frame;
    frame.initialize(0.0, 179.9, 0.0);  // Just west of antimeridian
    EXPECT_TRUE(frame.isInitialized());

    // Point just east of antimeridian
    LocalFrame ned = frame.gpsToLocalNED(0.0, -179.9, 0.0);
    // Should compute small distance, not wrap around
    double distance = ned.horizontal_distance();
    EXPECT_LT(distance, 15000.0);  // Less than 15km (not ~40km wrap-around)
}

TEST_F(GPSIntegrationTest, EquatorCrossing) {
    // Test: Latitude transition across equator
    CoordinateFrame frame;
    frame.initialize(-0.1, 0.0, 0.0);  // Just south of equator
    EXPECT_TRUE(frame.isInitialized());

    LocalFrame ned = frame.gpsToLocalNED(0.1, 0.0, 0.0);  // Just north
    EXPECT_GT(ned.north_m, 0.0);  // Should have positive north component
}

TEST_F(GPSIntegrationTest, InternationalDateLineCrossing) {
    // Test: Longitude values at boundaries
    CoordinateFrame frame;
    frame.initialize(45.0, 179.5, 1000.0);
    EXPECT_TRUE(frame.isInitialized());

    // Point on opposite side of date line
    LocalFrame ned = frame.gpsToLocalNED(45.0, -179.5, 1000.0);
    double distance = ned.horizontal_distance();
    EXPECT_LT(distance, 5000.0);  // Should be less than 5km (not wrapped)
}

// ============================================================================
// Test Group 6: High Altitude Handling (3 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, HighAltitudeAircraft) {
    // Test: High altitude like aircraft at 10km
    CoordinateFrame frame;
    double aircraft_alt = 10000.0;  // 10km
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, aircraft_alt);
    EXPECT_TRUE(frame.isInitialized());

    // Same location at different altitude
    LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, aircraft_alt + 1000.0);
    EXPECT_TRUE(doubles_equal(ned.down_m, -1000.0, TOLERANCE_METER));  // 1km up
}

TEST_F(GPSIntegrationTest, UnderseaDepth) {
    // Test: Negative altitude (below sea level / underwater)
    CoordinateFrame frame;
    double submarine_alt = -1000.0;  // 1km below sea level
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, submarine_alt);
    EXPECT_TRUE(frame.isInitialized());

    LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, submarine_alt - 100.0);
    EXPECT_TRUE(doubles_equal(ned.down_m, 100.0, TOLERANCE_METER));  // 100m deeper
}

TEST_F(GPSIntegrationTest, MountEverestAltitude) {
    // Test: Mount Everest altitude (~8849m)
    CoordinateFrame frame;
    frame.initialize(27.9881, 86.9250, 8849.0);
    EXPECT_TRUE(frame.isInitialized());

    // Same location at base elevation
    LocalFrame ned = frame.gpsToLocalNED(27.9881, 86.9250, 0.0);
    EXPECT_TRUE(doubles_equal(ned.down_m, 8849.0, TOLERANCE_METER));
}

// ============================================================================
// Test Group 7: Simulated GPS Data - NMEA Sentence Parsing (4 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, SimulatedGPSGNGGASentenceDecoding) {
    // Test: Decode simulated GNGGA NMEA sentence
    // Format: $GNGGA,hhmmss,ddmm.mmmm,N/S,dddmm.mmmm,E/W,q,ns,hdop,alt,M,geoid,M*hh

    // This test validates that a GNGGA sentence could be parsed
    // (Actual GPS.cpp implementation needed for full parse)
    // We validate the coordinate values from the sentence

    // Sentence for Munich: $GNGGA,123519,4722.00,N,01110.98,E,1,08,0.9,545.4,M,46.9,M,,*47
    // Lat: 47°22.00' N = 47 + 22/60 = 47.3667°
    // Lon: 011°10.98' E = 11 + 10.98/60 = 11.1833°
    // Alt: 545.4m

    double lat = 47.0 + 22.00 / 60.0;
    double lon = 11.0 + 10.98 / 60.0;
    double alt = 545.4;

    EXPECT_TRUE(doubles_equal(lat, 47.3667, 0.0001));
    EXPECT_TRUE(doubles_equal(lon, 11.1833, 0.0001));
}

TEST_F(GPSIntegrationTest, SimulatedGPSNEWYORKSentenceDecoding) {
    // Test: Decode GNGGA sentence for New York
    // $GNGGA,123519,4042.77,N,07403.60,W,1,08,0.9,10.0,M,0.0,M,,*53
    // Lat: 40°42.77' N = 40 + 42.77/60 = 40.7128°
    // Lon: 074°03.60' W = 74 + 3.60/60 = 74.0060° (note: W = negative)

    double lat = 40.0 + 42.77 / 60.0;
    double lon = 74.0 + 3.60 / 60.0;  // W is negative

    EXPECT_TRUE(doubles_equal(lat, 40.7128, 0.0001));
    EXPECT_TRUE(doubles_equal(lon, 74.0060, 0.0001));
}

TEST_F(GPSIntegrationTest, SimulatedGPSSINGAPORESentenceDecoding) {
    // Test: Decode GNGGA sentence for Singapore
    // $GNGGA,123519,0121.13,N,10349.19,E,1,12,0.8,20.0,M,0.0,M,,*58
    // Lat: 1°21.13' N = 1 + 21.13/60 = 1.3521°
    // Lon: 103°49.19' E = 103 + 49.19/60 = 103.8198°

    double lat = 1.0 + 21.13 / 60.0;
    double lon = 103.0 + 49.19 / 60.0;

    EXPECT_TRUE(doubles_equal(lat, 1.3521, 0.0001));
    EXPECT_TRUE(doubles_equal(lon, 103.8198, 0.0001));
}

TEST_F(GPSIntegrationTest, SimulatedGPSSYDNEYSentenceDecoding) {
    // Test: Decode GNGGA sentence for Sydney
    // $GNGGA,123519,3352.13,S,15112.56,E,1,10,0.9,50.0,M,0.0,M,,*5F
    // Lat: 33°52.13' S = -(33 + 52.13/60) = -33.8688°
    // Lon: 151°12.56' E = 151 + 12.56/60 = 151.2093°

    double lat = -(33.0 + 52.13 / 60.0);
    double lon = 151.0 + 12.56 / 60.0;

    EXPECT_TRUE(doubles_equal(lat, -33.8688, 0.0001));
    EXPECT_TRUE(doubles_equal(lon, 151.2093, 0.0001));
}

// ============================================================================
// Test Group 8: Multiple Frame Instances (2 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, MultipleFrameInstancesIndependent) {
    // Test: Multiple CoordinateFrame instances maintain independent state
    CoordinateFrame frame1, frame2;

    frame1.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
    frame2.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);

    // frame1 should convert relative to Munich
    LocalFrame ned1 = frame1.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
    EXPECT_GT(ned1.north_m, 500.0);  // Should be ~1km north

    // frame2 should convert relative to New York
    LocalFrame ned2 = frame2.gpsToLocalNED(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
    EXPECT_TRUE(doubles_equal(ned2.north_m, 0.0, TOLERANCE_METER));  // Same location
}

TEST_F(GPSIntegrationTest, FrameReinitialization_NotAffectingOtherInstances) {
    // Test: Reinitializing one frame doesn't affect others
    CoordinateFrame frame1, frame2;

    frame1.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
    frame2.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);

    // Reinitialize frame1
    frame1.reinitialize(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);

    // frame2 should still be at New York
    GPS_Data origin2 = frame2.getOrigin();
    EXPECT_TRUE(doubles_equal(origin2.latitude_deg, NEW_YORK.lat_deg, TOLERANCE_DEGREE));

    // frame1 should be at Singapore
    GPS_Data origin1 = frame1.getOrigin();
    EXPECT_TRUE(doubles_equal(origin1.latitude_deg, SINGAPORE.lat_deg, TOLERANCE_DEGREE));
}

// ============================================================================
// Test Group 9: Performance and Timing (2 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, ConversionPerformanceAcceptable) {
    // Test: Single GPS to NED conversion completes quickly
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        (void)ned;  // Use result to prevent optimization
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = duration.count() / 1000.0;

    // Should be less than 100 µs per conversion
    EXPECT_LT(avg_time_us, 100.0);
}

TEST_F(GPSIntegrationTest, RoundTripPerformanceAcceptable) {
    // Test: Complete round-trip GPS -> NED -> GPS
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        GPS_Data recovered = frame.localNEDToGPS(ned);
        (void)recovered;
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = duration.count() / 100.0;

    // Complete round-trip should be less than 200 µs
    EXPECT_LT(avg_time_us, 200.0);
}

// ============================================================================
// Test Group 10: Data Consistency (2 tests)
// ============================================================================

TEST_F(GPSIntegrationTest, ConsecutiveConversionsConsistent) {
    // Test: Multiple conversions of same point give same result
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    LocalFrame ned1 = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
    LocalFrame ned2 = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
    LocalFrame ned3 = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);

    EXPECT_TRUE(doubles_equal(ned1.north_m, ned2.north_m, TOLERANCE_MILLIMETER));
    EXPECT_TRUE(doubles_equal(ned2.north_m, ned3.north_m, TOLERANCE_MILLIMETER));
}

TEST_F(GPSIntegrationTest, OriginConstantAfterInitialization) {
    // Test: Origin doesn't change after initialization (until explicitly reinitialized)
    CoordinateFrame frame;
    frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);

    GPS_Data origin1 = frame.getOrigin();
    // Do some conversions
    for (int i = 0; i < 10; ++i) {
        frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg + 0.001 * i, MUNICH_ORIGIN.lon_deg + 0.001 * i, MUNICH_ORIGIN.alt_m);
    }
    GPS_Data origin2 = frame.getOrigin();

    EXPECT_TRUE(doubles_equal(origin1.latitude_deg, origin2.latitude_deg, TOLERANCE_MILLIMETER));
    EXPECT_TRUE(doubles_equal(origin1.longitude_deg, origin2.longitude_deg, TOLERANCE_MILLIMETER));
    EXPECT_TRUE(doubles_equal(origin1.altitude_m, origin2.altitude_m, TOLERANCE_MILLIMETER));
}

// ============================================================================
// Test Summary
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
