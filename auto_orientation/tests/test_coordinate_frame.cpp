/**
 * Unit Tests for CoordinateFrame
 *
 * Tests the coordinate frame manager that maintains a local NED origin from
 * the first GPS fix and provides conversions between GPS and NED coordinates.
 *
 * Tests include:
 * - Initialization with known coordinates
 * - GPS to NED conversions
 * - NED to GPS conversions (inverse)
 * - Round-trip accuracy (GPS -> NED -> GPS)
 * - Different origin altitudes
 * - Origin reinitialization
 * - Boundary conditions (equator, antimeridian, high altitudes)
 * - Error handling (uninitialized frame, invalid coordinates)
 * - Edge cases
 *
 * Reference Data:
 * - Munich: 48.1351°N, 11.5820°E, ~530m elevation
 * - Los Angeles: 34.0522°N, 118.2437°W, ~100m elevation
 * - Equator test: 0°N, 0°E
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <iostream>
#include "../src/math/coordinates.h"
#include "../src/navigation/coordinate_frame.h"

// ============================================================================
// TEST FIXTURES
// ============================================================================

class CoordinateFrameTest : public ::testing::Test {
 protected:
    // Tolerance values
    static constexpr double TOLERANCE_METER = 1.0;        // 1 meter for spatial accuracy
    static constexpr double TOLERANCE_CM = 0.01;          // 1 cm for round-trip
    static constexpr double TOLERANCE_DEGREE = 1e-7;      // ~1 cm at equator
    static constexpr double TOLERANCE_ROUNDTRIP = 0.1;    // <0.1m for round-trip accuracy

    // Test locations
    struct TestLocation {
        const char* name;
        double lat_deg;
        double lon_deg;
        double alt_m;
    };

    // Known test locations
    static const TestLocation MUNICH;
    static const TestLocation LOS_ANGELES;
    static const TestLocation SINGAPORE;
    static const TestLocation EQUATOR_PRIME;
    static const TestLocation SYDNEY;
    static const TestLocation NORTH_POLE_APPROX;
};

// Define test locations
const CoordinateFrameTest::TestLocation CoordinateFrameTest::MUNICH = {
    "Munich, Germany", 48.1351, 11.5820, 530.0
};

const CoordinateFrameTest::TestLocation CoordinateFrameTest::LOS_ANGELES = {
    "Los Angeles, USA", 34.0522, -118.2437, 100.0
};

const CoordinateFrameTest::TestLocation CoordinateFrameTest::SINGAPORE = {
    "Singapore", 1.3521, 103.8198, 20.0
};

const CoordinateFrameTest::TestLocation CoordinateFrameTest::EQUATOR_PRIME = {
    "Equator, Prime Meridian", 0.0, 0.0, 0.0
};

const CoordinateFrameTest::TestLocation CoordinateFrameTest::SYDNEY = {
    "Sydney, Australia", -33.8688, 151.2093, 30.0
};

const CoordinateFrameTest::TestLocation CoordinateFrameTest::NORTH_POLE_APPROX = {
    "North Pole (approx)", 89.9, 0.0, 100.0
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, DefaultConstructor_NotInitialized) {
    // Newly constructed frame should not be initialized
    CoordinateFrame frame;
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(CoordinateFrameTest, InitializeWithValidCoordinates_Munich) {
    // Initialize with known Munich coordinates
    CoordinateFrame frame;
    bool success = frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    EXPECT_TRUE(success);
    EXPECT_TRUE(frame.isInitialized());

    // Verify origin is stored correctly
    GPS_Data origin = frame.getOrigin();
    EXPECT_NEAR(origin.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(origin.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(origin.altitude_m, MUNICH.alt_m, TOLERANCE_CM);
}

TEST_F(CoordinateFrameTest, InitializeWithInvalidLatitude) {
    // Latitude > 90° should fail
    CoordinateFrame frame;
    bool success = frame.initialize(91.0, 0.0, 0.0);
    EXPECT_FALSE(success);
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(CoordinateFrameTest, InitializeWithInvalidLongitude) {
    // Longitude > 180° should fail
    CoordinateFrame frame;
    bool success = frame.initialize(0.0, 181.0, 0.0);
    EXPECT_FALSE(success);
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(CoordinateFrameTest, InitializeWithInvalidAltitude) {
    // Altitude > 50km should fail
    CoordinateFrame frame;
    bool success = frame.initialize(0.0, 0.0, 50001.0);
    EXPECT_FALSE(success);
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(CoordinateFrameTest, InitializeOnFirstFix_ValidGPSData) {
    // Initialize using GPS_Data structure
    CoordinateFrame frame;
    GPS_Data gps(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    bool success = frame.initializeOnFirstFix(gps);

    EXPECT_TRUE(success);
    EXPECT_TRUE(frame.isInitialized());

    GPS_Data origin = frame.getOrigin();
    EXPECT_NEAR(origin.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE);
}

TEST_F(CoordinateFrameTest, InitializeOnFirstFix_InvalidGPSData) {
    // Initialize with invalid GPS data
    CoordinateFrame frame;
    GPS_Data gps(91.0, 0.0, 0.0);  // Invalid latitude
    bool success = frame.initializeOnFirstFix(gps);

    EXPECT_FALSE(success);
    EXPECT_FALSE(frame.isInitialized());
}

TEST_F(CoordinateFrameTest, GetOriginAltitude) {
    // Test altitude getter
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    double alt = frame.getOriginAltitude();
    EXPECT_NEAR(alt, MUNICH.alt_m, TOLERANCE_CM);
}

// ============================================================================
// GPS TO NED CONVERSION TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, GPSToNED_AtOrigin) {
    // Point at origin should give (0, 0, 0)
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    EXPECT_NEAR(ned.north_m, 0.0, TOLERANCE_METER);
    EXPECT_NEAR(ned.east_m, 0.0, TOLERANCE_METER);
    EXPECT_NEAR(ned.down_m, 0.0, TOLERANCE_METER);
}

TEST_F(CoordinateFrameTest, GPSToNED_OffsetNorth) {
    // Point 0.001° north of origin
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(
        MUNICH.lat_deg + 0.001, MUNICH.lon_deg, MUNICH.alt_m);

    // Should have positive north component (~111m per degree)
    EXPECT_GT(ned.north_m, 100.0);
    EXPECT_NEAR(ned.east_m, 0.0, TOLERANCE_METER);
    EXPECT_NEAR(ned.down_m, 0.0, TOLERANCE_METER);
}

TEST_F(CoordinateFrameTest, GPSToNED_OffsetEast) {
    // Point 0.001° east of origin
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(
        MUNICH.lat_deg, MUNICH.lon_deg + 0.001, MUNICH.alt_m);

    // Should have positive east component
    // At Munich lat (~48°), 1° lon ≈ 73m east
    EXPECT_GT(ned.east_m, 60.0);
    EXPECT_NEAR(ned.north_m, 0.0, TOLERANCE_METER);
    EXPECT_NEAR(ned.down_m, 0.0, TOLERANCE_METER);
}

TEST_F(CoordinateFrameTest, GPSToNED_OffsetUp) {
    // Point 100m up from origin
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(
        MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m + 100.0);

    // Should have negative down component (up = -down)
    EXPECT_LT(ned.down_m, -90.0);
    EXPECT_NEAR(ned.north_m, 0.0, TOLERANCE_METER);
    EXPECT_NEAR(ned.east_m, 0.0, TOLERANCE_METER);
}

TEST_F(CoordinateFrameTest, GPSToNED_WithInvalidCoordinates) {
    // gpsToLocalNED with invalid input should return NaN
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(91.0, 0.0, 0.0);  // Invalid latitude

    EXPECT_TRUE(std::isnan(ned.north_m));
    EXPECT_TRUE(std::isnan(ned.east_m));
    EXPECT_TRUE(std::isnan(ned.down_m));
}

TEST_F(CoordinateFrameTest, GPSToNED_WithoutInitialization) {
    // gpsToLocalNED before initialization should throw
    CoordinateFrame frame;

    EXPECT_THROW(frame.gpsToLocalNED(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m),
                 std::runtime_error);
}

// ============================================================================
// NED TO GPS CONVERSION TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, NEDToGPS_AtOrigin) {
    // NED (0, 0, 0) should give origin GPS coordinates
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    GPS_Data gps = frame.localNEDToGPS(LocalFrame(0.0, 0.0, 0.0));

    EXPECT_NEAR(gps.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(gps.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(gps.altitude_m, MUNICH.alt_m, TOLERANCE_CM);
}

TEST_F(CoordinateFrameTest, NEDToGPS_SmallOffsetNorth) {
    // NED offset north should give higher latitude
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    GPS_Data gps = frame.localNEDToGPS(LocalFrame(111.0, 0.0, 0.0));

    // North ~111m should be ~0.001° latitude
    EXPECT_GT(gps.latitude_deg, MUNICH.lat_deg);
    EXPECT_NEAR(gps.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE);
}

TEST_F(CoordinateFrameTest, NEDToGPS_WithoutInitialization) {
    // localNEDToGPS before initialization should throw
    CoordinateFrame frame;

    EXPECT_THROW(frame.localNEDToGPS(LocalFrame(0.0, 0.0, 0.0)),
                 std::runtime_error);
}

// ============================================================================
// ROUND-TRIP ACCURACY TESTS (GPS -> NED -> GPS)
// ============================================================================

TEST_F(CoordinateFrameTest, RoundTrip_Munich_Origin) {
    // GPS -> NED -> GPS at origin should be accurate
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    GPS_Data original(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    // Convert to NED
    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);

    // Convert back to GPS
    GPS_Data result = frame.localNEDToGPS(ned);

    // Should recover original coordinates with <0.1m accuracy
    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

TEST_F(CoordinateFrameTest, RoundTrip_NearOrigin_SmallOffset) {
    // GPS -> NED -> GPS with small offset
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    GPS_Data original(MUNICH.lat_deg + 0.0005, MUNICH.lon_deg + 0.0005,
                     MUNICH.alt_m + 50.0);

    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);
    GPS_Data result = frame.localNEDToGPS(ned);

    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

TEST_F(CoordinateFrameTest, RoundTrip_LargerOffset) {
    // GPS -> NED -> GPS with larger offset (~10 km)
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    GPS_Data original(MUNICH.lat_deg + 0.1, MUNICH.lon_deg + 0.1,
                     MUNICH.alt_m + 100.0);

    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);
    GPS_Data result = frame.localNEDToGPS(ned);

    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

TEST_F(CoordinateFrameTest, RoundTrip_LosAngeles_Origin) {
    // Test round-trip at different location (Los Angeles)
    CoordinateFrame frame;
    frame.initialize(LOS_ANGELES.lat_deg, LOS_ANGELES.lon_deg, LOS_ANGELES.alt_m);

    GPS_Data original(LOS_ANGELES.lat_deg, LOS_ANGELES.lon_deg, LOS_ANGELES.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);
    GPS_Data result = frame.localNEDToGPS(ned);

    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

TEST_F(CoordinateFrameTest, RoundTrip_Equator) {
    // Test round-trip at equator
    CoordinateFrame frame;
    frame.initialize(EQUATOR_PRIME.lat_deg, EQUATOR_PRIME.lon_deg, EQUATOR_PRIME.alt_m);

    GPS_Data original(EQUATOR_PRIME.lat_deg, EQUATOR_PRIME.lon_deg, EQUATOR_PRIME.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);
    GPS_Data result = frame.localNEDToGPS(ned);

    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

TEST_F(CoordinateFrameTest, RoundTrip_SouthernHemisphere) {
    // Test round-trip at Sydney (southern hemisphere)
    CoordinateFrame frame;
    frame.initialize(SYDNEY.lat_deg, SYDNEY.lon_deg, SYDNEY.alt_m);

    GPS_Data original(SYDNEY.lat_deg, SYDNEY.lon_deg, SYDNEY.alt_m);

    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);
    GPS_Data result = frame.localNEDToGPS(ned);

    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

// ============================================================================
// ORIGIN ALTITUDE VARIATION TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, InitializeWithDifferentAltitudes_SeaLevel) {
    // Initialize at sea level
    CoordinateFrame frame;
    bool success = frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, 0.0);

    EXPECT_TRUE(success);
    EXPECT_NEAR(frame.getOriginAltitude(), 0.0, TOLERANCE_CM);
}

TEST_F(CoordinateFrameTest, InitializeWithDifferentAltitudes_HighElevation) {
    // Initialize at high elevation
    CoordinateFrame frame;
    bool success = frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, 3000.0);

    EXPECT_TRUE(success);
    EXPECT_NEAR(frame.getOriginAltitude(), 3000.0, TOLERANCE_CM);
}

TEST_F(CoordinateFrameTest, InitializeWithDifferentAltitudes_DeepOcean) {
    // Initialize at deep ocean (negative altitude, below ellipsoid)
    CoordinateFrame frame;
    bool success = frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, -400.0);

    EXPECT_TRUE(success);
    EXPECT_NEAR(frame.getOriginAltitude(), -400.0, TOLERANCE_CM);
}

TEST_F(CoordinateFrameTest, RoundTrip_HighAltitudeOrigin) {
    // Test round-trip with high-altitude origin
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, 2000.0);

    GPS_Data original(MUNICH.lat_deg + 0.001, MUNICH.lon_deg + 0.001, 2100.0);

    LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                          original.altitude_m);
    GPS_Data result = frame.localNEDToGPS(ned);

    EXPECT_NEAR(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP);
}

// ============================================================================
// REINITIALIZATION TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, Reinitialize_ChangeOrigin) {
    // Initialize at Munich, then reinitialize at Los Angeles
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    EXPECT_TRUE(frame.isInitialized());

    // Reinitialize
    bool success = frame.reinitialize(LOS_ANGELES.lat_deg, LOS_ANGELES.lon_deg,
                                      LOS_ANGELES.alt_m);
    EXPECT_TRUE(success);

    // Verify new origin
    GPS_Data origin = frame.getOrigin();
    EXPECT_NEAR(origin.latitude_deg, LOS_ANGELES.lat_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(origin.longitude_deg, LOS_ANGELES.lon_deg, TOLERANCE_DEGREE);
}

TEST_F(CoordinateFrameTest, Reinitialize_WithInvalidCoordinates) {
    // Initialize at Munich
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    // Try to reinitialize with invalid coordinates
    bool success = frame.reinitialize(91.0, 0.0, 0.0);
    EXPECT_FALSE(success);

    // Original frame should still be valid
    EXPECT_TRUE(frame.isInitialized());
    GPS_Data origin = frame.getOrigin();
    EXPECT_NEAR(origin.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE);
}

TEST_F(CoordinateFrameTest, Reinitialize_PreservesThreadSafety) {
    // Reinitialize multiple times (should maintain thread safety)
    CoordinateFrame frame;

    for (int i = 0; i < 3; ++i) {
        bool success = frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
        EXPECT_TRUE(success);
        EXPECT_TRUE(frame.isInitialized());
    }
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, BoundaryCondition_EquatorPrimeMeridian) {
    // Origin at equator and prime meridian
    CoordinateFrame frame;
    bool success = frame.initialize(0.0, 0.0, 0.0);

    EXPECT_TRUE(success);
    EXPECT_TRUE(frame.isInitialized());

    // Test conversions work correctly
    LocalFrame ned = frame.gpsToLocalNED(0.001, 0.001, 0.0);
    EXPECT_GT(ned.north_m, 100.0);
    EXPECT_GT(ned.east_m, 100.0);
}

TEST_F(CoordinateFrameTest, BoundaryCondition_PrimeMeridianWestOfAntimeridian) {
    // Longitude boundary at antimeridian
    CoordinateFrame frame;
    frame.initialize(0.0, 179.0, 0.0);

    // Point slightly east of antimeridian
    LocalFrame ned = frame.gpsToLocalNED(0.0, -179.0, 0.0);

    // Should still compute correctly (short distance across antimeridian)
    EXPECT_GT(ned.east_m, 100.0);
}

TEST_F(CoordinateFrameTest, BoundaryCondition_NearNorthPole) {
    // Origin near north pole
    CoordinateFrame frame;
    bool success = frame.initialize(89.0, 0.0, 0.0);

    EXPECT_TRUE(success);

    // Test conversion works
    LocalFrame ned = frame.gpsToLocalNED(89.001, 0.0, 0.0);
    EXPECT_GT(ned.north_m, 100.0);
}

TEST_F(CoordinateFrameTest, BoundaryCondition_NearSouthPole) {
    // Origin near south pole
    CoordinateFrame frame;
    bool success = frame.initialize(-89.0, 0.0, 0.0);

    EXPECT_TRUE(success);

    // Test conversion works
    LocalFrame ned = frame.gpsToLocalNED(-89.001, 0.0, 0.0);
    EXPECT_LT(ned.north_m, -100.0);  // South (negative north)
}

TEST_F(CoordinateFrameTest, BoundaryCondition_HighAltitude) {
    // Origin at high altitude
    CoordinateFrame frame;
    bool success = frame.initialize(0.0, 0.0, 10000.0);

    EXPECT_TRUE(success);

    // Test conversions still work
    LocalFrame ned = frame.gpsToLocalNED(0.001, 0.0, 10000.0);
    EXPECT_GT(ned.north_m, 100.0);
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, ErrorHandling_GetOriginBeforeInitialization) {
    // getOrigin() before initialization should throw
    CoordinateFrame frame;

    EXPECT_THROW(frame.getOrigin(), std::runtime_error);
}

TEST_F(CoordinateFrameTest, ErrorHandling_GetOriginAltitudeBeforeInitialization) {
    // getOriginAltitude() before initialization should throw
    CoordinateFrame frame;

    EXPECT_THROW(frame.getOriginAltitude(), std::runtime_error);
}

// ============================================================================
// CONSISTENCY TESTS
// ============================================================================

TEST_F(CoordinateFrameTest, Consistency_MultipleConversions) {
    // Multiple conversions from same origin should be consistent
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    GPS_Data point(MUNICH.lat_deg + 0.001, MUNICH.lon_deg + 0.001, MUNICH.alt_m);

    LocalFrame ned1 = frame.gpsToLocalNED(point.latitude_deg, point.longitude_deg,
                                          point.altitude_m);
    LocalFrame ned2 = frame.gpsToLocalNED(point.latitude_deg, point.longitude_deg,
                                          point.altitude_m);

    // Should get identical results
    EXPECT_EQ(ned1.north_m, ned2.north_m);
    EXPECT_EQ(ned1.east_m, ned2.east_m);
    EXPECT_EQ(ned1.down_m, ned2.down_m);
}

TEST_F(CoordinateFrameTest, Consistency_InverseOperations) {
    // ned_to_ecef and ecef_to_ned should be inverses
    CoordinateFrame frame;
    frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    // Create a point 1000m away
    LocalFrame ned_original(111.0, 93.0, -50.0);

    // Convert NED -> GPS -> NED
    GPS_Data gps = frame.localNEDToGPS(ned_original);
    LocalFrame ned_result = frame.gpsToLocalNED(gps.latitude_deg, gps.longitude_deg,
                                               gps.altitude_m);

    // Should recover original NED coordinates
    EXPECT_NEAR(ned_result.north_m, ned_original.north_m, TOLERANCE_METER);
    EXPECT_NEAR(ned_result.east_m, ned_original.east_m, TOLERANCE_METER);
    EXPECT_NEAR(ned_result.down_m, ned_original.down_m, TOLERANCE_METER);
}

#endif  // DEBUG_MODE
