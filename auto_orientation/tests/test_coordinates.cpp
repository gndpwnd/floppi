/**
 * Comprehensive Unit Tests for Coordinate Transformations
 *
 * Tests GPS ↔ ECEF ↔ NED conversions, with validation against known locations,
 * round-trip accuracy, and edge cases.
 *
 * Reference: GPS_GEODETIC_COORDINATE_SYSTEMS.md
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "../src/math/coordinates.h"
#include "../src/math/magnetic_declination.h"

// ============================================================================
// TEST FIXTURES
// ============================================================================

class CoordinateTransformationTest : public ::testing::Test {
 protected:
    // Tolerance values for comparisons
    static constexpr double TOLERANCE_METER = 1.0;        // 1 meter
    static constexpr double TOLERANCE_MILLIMETER = 0.001; // 1 millimeter
    static constexpr double TOLERANCE_DEGREE = 1e-7;      // ~1 cm at equator
    static constexpr double TOLERANCE_NED_METER = 0.5;    // 0.5 meter for NED

    // Test locations with known coordinates
    struct TestLocation {
        const char* name;
        double lat_deg;
        double lon_deg;
        double alt_m;
        // Expected ECEF (approximate, for validation)
        double ecef_x_approx;
        double ecef_y_approx;
        double ecef_z_approx;
    };

    // Known test locations
    static const TestLocation MUNICH;
    static const TestLocation LOS_ANGELES;
    static const TestLocation SINGAPORE;
    static const TestLocation NEW_YORK;
    static const TestLocation LONDON;
    static const TestLocation SYDNEY;
    static const TestLocation SAN_FRANCISCO;
    static const TestLocation EQUATOR_PRIME;
    static const TestLocation SOUTH_POLE;
    static const TestLocation NORTH_POLE_APPROX;
};

// Define test locations with actual coordinates
// ECEF values are approximate (±1000m) for validation purposes
const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::MUNICH = {
    "Munich, Germany",
    47.3667, 11.1833, 520.0,
    -2764000.0, 1440000.0, 4690000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::LOS_ANGELES = {
    "Los Angeles, USA",
    34.0522, -118.2437, 100.0,
    -2428000.0, -4723000.0, 3580000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::SINGAPORE = {
    "Singapore",
    1.3521, 103.8198, 20.0,
    -2359000.0, 5677000.0, 140000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::NEW_YORK = {
    "New York City, USA",
    40.7128, -74.0060, 10.0,
    1335000.0, -4635000.0, 4155000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::LONDON = {
    "London, UK",
    51.5074, -0.1278, 10.0,
    3975000.0, -55000.0, 4969000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::SYDNEY = {
    "Sydney, Australia",
    -33.8688, 151.2093, 30.0,
    -4674000.0, 2544000.0, -3521000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation CoordinateTransformationTest::SAN_FRANCISCO = {
    "San Francisco, USA",
    37.7749, -122.4194, 10.0,
    -2707000.0, -4271000.0, 3886000.0  // Approximate
};

const CoordinateTransformationTest::TestLocation
    CoordinateTransformationTest::EQUATOR_PRIME = {
        "Equator, Prime Meridian",
        0.0, 0.0, 0.0,
        6378137.0, 0.0, 0.0  // On ellipsoid at equator
    };

const CoordinateTransformationTest::TestLocation
    CoordinateTransformationTest::SOUTH_POLE = {
        "South Pole",
        -90.0, 0.0, 100.0,
        0.0, 0.0, -6356752.314245 - 100.0  // Approximate (any longitude valid)
    };

const CoordinateTransformationTest::TestLocation
    CoordinateTransformationTest::NORTH_POLE_APPROX = {
        "North Pole (approx)",
        90.0, 0.0, 100.0,
        0.0, 0.0, 6356752.314245 + 100.0  // Approximate (any longitude valid)
    };

// ============================================================================
// GPS ↔ ECEF CONVERSION TESTS
// ============================================================================

TEST_F(CoordinateTransformationTest, GPSToECEFKnownLocation_Munich) {
    ECEF ecef = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    // Check that result is not NaN or Inf
    EXPECT_FALSE(std::isnan(ecef.x)) << "ECEF X is NaN";
    EXPECT_FALSE(std::isnan(ecef.y)) << "ECEF Y is NaN";
    EXPECT_FALSE(std::isnan(ecef.z)) << "ECEF Z is NaN";
    EXPECT_FALSE(std::isinf(ecef.x)) << "ECEF X is Inf";
    EXPECT_FALSE(std::isinf(ecef.y)) << "ECEF Y is Inf";
    EXPECT_FALSE(std::isinf(ecef.z)) << "ECEF Z is Inf";

    // Check distance from Earth center (should be ~6.37M meters)
    double distance = ecef.magnitude();
    EXPECT_GT(distance, 6.3e6) << "Distance too small";
    EXPECT_LT(distance, 6.4e6) << "Distance too large";

    // Check approximate agreement with known ECEF (within ~1km)
    EXPECT_NEAR(ecef.x, MUNICH.ecef_x_approx, 1e4) << "ECEF X differs significantly";
    EXPECT_NEAR(ecef.y, MUNICH.ecef_y_approx, 1e4) << "ECEF Y differs significantly";
    EXPECT_NEAR(ecef.z, MUNICH.ecef_z_approx, 1e4) << "ECEF Z differs significantly";
}

TEST_F(CoordinateTransformationTest, ECEFToGPSRoundTrip_Munich) {
    // Convert GPS to ECEF
    ECEF ecef = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    // Convert back to GPS
    GPS_Data gps_result = ecef_to_gps(ecef.x, ecef.y, ecef.z);

    // Check round-trip accuracy
    EXPECT_NEAR(gps_result.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE)
        << "Latitude round-trip error too large";
    EXPECT_NEAR(gps_result.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE)
        << "Longitude round-trip error too large";
    EXPECT_NEAR(gps_result.altitude_m, MUNICH.alt_m, TOLERANCE_MILLIMETER)
        << "Altitude round-trip error too large";
}

TEST_F(CoordinateTransformationTest, GPSECEFRoundTripMultipleLocations) {
    // Test round-trip for multiple known locations
    const TestLocation* locations[] = {
        &MUNICH,
        &LOS_ANGELES,
        &SINGAPORE,
        &NEW_YORK,
        &LONDON,
        &SYDNEY,
        &SAN_FRANCISCO,
    };

    for (const auto* loc : locations) {
        SCOPED_TRACE(loc->name);

        ECEF ecef = gps_to_ecef(loc->lat_deg, loc->lon_deg, loc->alt_m);
        GPS_Data gps_result = ecef_to_gps(ecef.x, ecef.y, ecef.z);

        EXPECT_NEAR(gps_result.latitude_deg, loc->lat_deg, TOLERANCE_DEGREE)
            << "Latitude round-trip failed";
        EXPECT_NEAR(gps_result.longitude_deg, loc->lon_deg, TOLERANCE_DEGREE)
            << "Longitude round-trip failed";
        EXPECT_NEAR(gps_result.altitude_m, loc->alt_m, TOLERANCE_MILLIMETER)
            << "Altitude round-trip failed";
    }
}

TEST_F(CoordinateTransformationTest, GPSToECEFEquator) {
    ECEF ecef = gps_to_ecef(0.0, 0.0, 0.0);

    // At equator with zero altitude and zero longitude, X should be ~a (6378137)
    EXPECT_NEAR(ecef.x, WGS84::SEMI_MAJOR_AXIS, TOLERANCE_METER);
    EXPECT_NEAR(ecef.y, 0.0, TOLERANCE_METER);
    EXPECT_NEAR(ecef.z, 0.0, TOLERANCE_METER);
}

TEST_F(CoordinateTransformationTest, GPSToECEFAltitudeEffect) {
    // Same location, different altitudes
    ECEF ecef_0m = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, 0.0);
    ECEF ecef_1000m = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, 1000.0);

    // 1000m altitude difference should translate to ~1000m change in ECEF
    double dist_diff = ecef_1000m.magnitude() - ecef_0m.magnitude();
    EXPECT_NEAR(dist_diff, 1000.0, TOLERANCE_METER)
        << "Altitude change not properly reflected in ECEF";
}

// ============================================================================
// ECEF <-> NED CONVERSION TESTS
// ============================================================================

TEST_F(CoordinateTransformationTest, ECEFToNEDSamePoint) {
    // When point equals origin, NED should be (0, 0, 0)
    ECEF ecef = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    double lat_rad = MUNICH.lat_deg * M_PI / 180.0;

    LocalFrame ned = ecef_to_ned(ecef, ecef, lat_rad);

    EXPECT_NEAR(ned.north_m, 0.0, TOLERANCE_NED_METER) << "North offset should be zero";
    EXPECT_NEAR(ned.east_m, 0.0, TOLERANCE_NED_METER) << "East offset should be zero";
    EXPECT_NEAR(ned.down_m, 0.0, TOLERANCE_NED_METER) << "Down offset should be zero";
}

TEST_F(CoordinateTransformationTest, NEDToECEFInverse) {
    // ned_to_ecef should be inverse of ecef_to_ned
    ECEF origin = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    ECEF point = gps_to_ecef(MUNICH.lat_deg + 0.001, MUNICH.lon_deg + 0.001,
                              MUNICH.alt_m + 50.0);
    double lat_rad = MUNICH.lat_deg * M_PI / 180.0;

    // Forward: ECEF -> NED
    LocalFrame ned = ecef_to_ned(point, origin, lat_rad);

    // Reverse: NED -> ECEF
    ECEF point_result = ned_to_ecef(ned, origin, lat_rad);

    // Should recover original point
    EXPECT_NEAR(point_result.x, point.x, TOLERANCE_METER) << "ECEF X not recovered";
    EXPECT_NEAR(point_result.y, point.y, TOLERANCE_METER) << "ECEF Y not recovered";
    EXPECT_NEAR(point_result.z, point.z, TOLERANCE_METER) << "ECEF Z not recovered";
}

TEST_F(CoordinateTransformationTest, NEDOffsetNorth) {
    // Offset directly north from reference
    ECEF origin = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    ECEF point_north = gps_to_ecef(MUNICH.lat_deg + 0.001, MUNICH.lon_deg, MUNICH.alt_m);
    double lat_rad = MUNICH.lat_deg * M_PI / 180.0;

    LocalFrame ned = ecef_to_ned(point_north, origin, lat_rad);

    // Should have primarily north component (~111m per degree)
    EXPECT_GT(ned.north_m, 100.0) << "North offset too small";
    EXPECT_NEAR(ned.east_m, 0.0, 10.0) << "East offset should be minimal";
    EXPECT_NEAR(ned.down_m, 0.0, 10.0) << "Down offset should be minimal";
}

TEST_F(CoordinateTransformationTest, NEDOffsetEast) {
    // Offset directly east from reference
    ECEF origin = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    ECEF point_east = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg + 0.001, MUNICH.alt_m);
    double lat_rad = MUNICH.lat_deg * M_PI / 180.0;

    LocalFrame ned = ecef_to_ned(point_east, origin, lat_rad);

    // Should have primarily east component
    // At Munich latitude (~47°), 1° longitude ≈ 73m east
    EXPECT_GT(ned.east_m, 60.0) << "East offset too small";
    EXPECT_NEAR(ned.north_m, 0.0, 10.0) << "North offset should be minimal";
    EXPECT_NEAR(ned.down_m, 0.0, 10.0) << "Down offset should be minimal";
}

TEST_F(CoordinateTransformationTest, NEDOffsetUp) {
    // Offset directly up from reference
    ECEF origin = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
    ECEF point_up = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m + 100.0);
    double lat_rad = MUNICH.lat_deg * M_PI / 180.0;

    LocalFrame ned = ecef_to_ned(point_up, origin, lat_rad);

    // Should have primarily down component (negative for "up")
    EXPECT_LT(ned.down_m, -90.0) << "Down offset (should be negative for up) too small";
    EXPECT_NEAR(ned.north_m, 0.0, 10.0) << "North offset should be minimal";
    EXPECT_NEAR(ned.east_m, 0.0, 10.0) << "East offset should be minimal";
}

// ============================================================================
// ROUND-TRIP GPS -> ECEF -> NED -> ECEF -> GPS TESTS
// ============================================================================

TEST_F(CoordinateTransformationTest, FullRoundTripGPStoNED) {
    // GPS -> ECEF -> NED -> ECEF -> GPS
    GPS_Data original = {MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m, 1.0f, 1.5f};

    // Forward: GPS to ECEF
    ECEF ecef = gps_to_ecef(original.latitude_deg, original.longitude_deg, original.altitude_m);

    // Forward: ECEF to NED (with reference = origin)
    LocalFrame ned = ecef_to_ned(ecef, ecef, original.latitude_deg * M_PI / 180.0);

    // Reverse: NED to ECEF
    ECEF ecef_recovered = ned_to_ecef(ned, ecef, original.latitude_deg * M_PI / 180.0);

    // Reverse: ECEF to GPS
    GPS_Data final_gps = ecef_to_gps(ecef_recovered.x, ecef_recovered.y, ecef_recovered.z);

    // Should recover original GPS data
    EXPECT_NEAR(final_gps.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(final_gps.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(final_gps.altitude_m, original.altitude_m, TOLERANCE_MILLIMETER);
}

TEST_F(CoordinateTransformationTest, DirectGPStoNED) {
    // Test convenience function gps_to_ned
    LocalFrame ned = gps_to_ned(MUNICH.lat_deg + 0.001, MUNICH.lon_deg + 0.001,
                                MUNICH.alt_m + 50.0, MUNICH.lat_deg, MUNICH.lon_deg,
                                MUNICH.alt_m);

    // Should show offset north, east, and up
    EXPECT_GT(ned.north_m, 80.0) << "North offset too small";
    EXPECT_GT(ned.east_m, 50.0) << "East offset too small";
    EXPECT_LT(ned.down_m, -40.0) << "Down offset (up) insufficient";
}

TEST_F(CoordinateTransformationTest, DirectNEDtoGPS) {
    // Test convenience function ned_to_gps
    LocalFrame ned{100.0, 80.0, -50.0};  // 100m north, 80m east, 50m up

    GPS_Data result = ned_to_gps(ned, MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

    // Result should be near Munich but offset
    EXPECT_GT(result.latitude_deg, MUNICH.lat_deg) << "Latitude should increase for north offset";
    EXPECT_GT(result.longitude_deg, MUNICH.lon_deg) << "Longitude should increase for east offset";
    EXPECT_GT(result.altitude_m, MUNICH.alt_m) << "Altitude should increase for up offset";
}

// ============================================================================
// INPUT VALIDATION TESTS
// ============================================================================

TEST_F(CoordinateTransformationTest, ValidGPSInputsAccepted) {
    EXPECT_TRUE(is_valid_gps(0.0, 0.0, 0.0)) << "Equator/Prime Meridian invalid";
    EXPECT_TRUE(is_valid_gps(90.0, 0.0, 0.0)) << "North Pole invalid";
    EXPECT_TRUE(is_valid_gps(-90.0, 0.0, 0.0)) << "South Pole invalid";
    EXPECT_TRUE(is_valid_gps(45.0, 180.0, 5000.0)) << "Valid high altitude invalid";
    EXPECT_TRUE(is_valid_gps(45.0, -180.0, 5000.0)) << "Valid negative longitude invalid";
    EXPECT_TRUE(is_valid_gps(45.0, 45.0, -300.0)) << "Valid negative altitude invalid";
}

TEST_F(CoordinateTransformationTest, InvalidGPSInputsRejected) {
    EXPECT_FALSE(is_valid_gps(91.0, 0.0, 0.0)) << "Latitude > 90° should be invalid";
    EXPECT_FALSE(is_valid_gps(-91.0, 0.0, 0.0)) << "Latitude < -90° should be invalid";
    EXPECT_FALSE(is_valid_gps(45.0, 181.0, 0.0)) << "Longitude > 180° should be invalid";
    EXPECT_FALSE(is_valid_gps(45.0, -181.0, 0.0)) << "Longitude < -180° should be invalid";
    EXPECT_FALSE(is_valid_gps(45.0, 45.0, -501.0)) << "Altitude < -500m should be invalid";
    EXPECT_FALSE(is_valid_gps(45.0, 45.0, 50001.0)) << "Altitude > 50km should be invalid";
    EXPECT_FALSE(is_valid_gps(std::nan(""), 0.0, 0.0)) << "NaN latitude should be invalid";
    EXPECT_FALSE(is_valid_gps(45.0, std::nan(""), 0.0)) << "NaN longitude should be invalid";
    EXPECT_FALSE(is_valid_gps(45.0, 45.0, std::nan(""))) << "NaN altitude should be invalid";
    EXPECT_FALSE(is_valid_gps(std::numeric_limits<double>::infinity(), 0.0, 0.0))
        << "Infinite latitude should be invalid";
}

TEST_F(CoordinateTransformationTest, InvalidECEFInputsRejected) {
    EXPECT_FALSE(is_valid_ecef(ECEF(std::nan(""), 0.0, 0.0))) << "NaN X should be invalid";
    EXPECT_FALSE(is_valid_ecef(ECEF(0.0, std::nan(""), 0.0))) << "NaN Y should be invalid";
    EXPECT_FALSE(is_valid_ecef(ECEF(0.0, 0.0, std::nan("")))) << "NaN Z should be invalid";
    EXPECT_FALSE(is_valid_ecef(ECEF(1e8, 0.0, 0.0))) << "Extreme X should be invalid";
    EXPECT_FALSE(is_valid_ecef(ECEF(1e5, 1e5, 1e5))) << "Too small magnitude should be invalid";
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(CoordinateTransformationTest, HighAltitudeConversion) {
    // Test with high altitude (ISS orbit ~400km)
    ECEF ecef = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, 400000.0);
    GPS_Data result = ecef_to_gps(ecef.x, ecef.y, ecef.z);

    EXPECT_NEAR(result.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, 400000.0, 1.0) << "High altitude not preserved";
}

TEST_F(CoordinateTransformationTest, NegativeAltitudeConversion) {
    // Test with negative altitude (underwater, deep trench)
    ECEF ecef = gps_to_ecef(MUNICH.lat_deg, MUNICH.lon_deg, -400.0);
    GPS_Data result = ecef_to_gps(ecef.x, ecef.y, ecef.z);

    EXPECT_NEAR(result.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE);
    EXPECT_NEAR(result.altitude_m, -400.0, 1.0) << "Negative altitude not preserved";
}

TEST_F(CoordinateTransformationTest, AntimeridianCrossing) {
    // Test longitude near ±180° (international date line)
    ECEF ecef1 = gps_to_ecef(45.0, 179.5, 1000.0);
    ECEF ecef2 = gps_to_ecef(45.0, -179.5, 1000.0);

    GPS_Data result1 = ecef_to_gps(ecef1.x, ecef1.y, ecef1.z);
    GPS_Data result2 = ecef_to_gps(ecef2.x, ecef2.y, ecef2.z);

    EXPECT_NEAR(result1.latitude_deg, 45.0, TOLERANCE_DEGREE);
    EXPECT_NEAR(result2.latitude_deg, 45.0, TOLERANCE_DEGREE);
    // Longitude should be near ±180° (can wrap around)
    EXPECT_TRUE((std::abs(result1.longitude_deg - 179.5) < 1.0) ||
                (std::abs(result1.longitude_deg + 179.5) < 1.0))
        << "Antimeridian longitude recovery failed";
}

#endif  // DEBUG_MODE
