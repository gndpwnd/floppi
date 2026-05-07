/**
 * Standalone Test for Coordinate Transformations
 *
 * This file can be compiled standalone to verify coordinate transformations
 * without requiring the full PlatformIO environment or Google Test framework.
 *
 * Compile with:
 *   g++ -std=c++11 -O2 -o test_coords \
 *       test_coordinates_standalone.cpp \
 *       ../src/math/coordinates.cpp \
 *       ../src/math/magnetic_declination.cpp \
 *       -lm
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include "../src/math/coordinates.h"
#include "../src/math/magnetic_declination.h"

// Tolerance constants
static constexpr double TOLERANCE_METER = 1.0;
static constexpr double TOLERANCE_DEGREE = 1e-7;
static constexpr double TOLERANCE_MILLIMETER = 0.001;

// Test counter
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

// Assertion macro
#define TEST_ASSERT(condition, message)                                     \
    do {                                                                     \
        tests_run++;                                                         \
        if (condition) {                                                     \
            tests_passed++;                                                  \
            std::cout << "✓ " << message << std::endl;                       \
        } else {                                                             \
            tests_failed++;                                                  \
            std::cout << "✗ FAILED: " << message << std::endl;              \
        }                                                                    \
    } while (0)

#define TEST_NEAR(actual, expected, tolerance, message)                     \
    TEST_ASSERT(std::abs((actual) - (expected)) <= (tolerance),             \
                message << " (expected " << std::fixed << std::setprecision(6) \
                        << (expected) << ", got " << (actual) << ")")

// ============================================================================
// TEST LOCATIONS
// ============================================================================

struct TestLocation {
    const char* name;
    double lat;
    double lon;
    double alt;
};

static const TestLocation TEST_LOCATIONS[] = {
    {"Munich, Germany", 47.3667, 11.1833, 520.0},
    {"Los Angeles, USA", 34.0522, -118.2437, 100.0},
    {"Singapore", 1.3521, 103.8198, 20.0},
    {"New York, USA", 40.7128, -74.0060, 10.0},
    {"Sydney, Australia", -33.8688, 151.2093, 30.0},
    {"Equator Prime Meridian", 0.0, 0.0, 0.0},
};

static constexpr int NUM_TEST_LOCATIONS = sizeof(TEST_LOCATIONS) / sizeof(TestLocation);

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

void test_gps_to_ecef() {
    std::cout << "\n=== Testing GPS to ECEF Conversion ===" << std::endl;

    for (int i = 0; i < NUM_TEST_LOCATIONS; ++i) {
        const TestLocation& loc = TEST_LOCATIONS[i];
        ECEF ecef = gps_to_ecef(loc.lat, loc.lon, loc.alt);

        // Check that result is valid (not NaN)
        TEST_ASSERT(!std::isnan(ecef.x) && !std::isnan(ecef.y) && !std::isnan(ecef.z),
                    std::string(loc.name) + ": ECEF not NaN");

        // Check distance from Earth center is reasonable
        double distance = ecef.magnitude();
        TEST_ASSERT(distance > 6.3e6 && distance < 6.4e6,
                    std::string(loc.name) + ": ECEF distance valid");
    }
}

void test_ecef_to_gps_roundtrip() {
    std::cout << "\n=== Testing ECEF to GPS Round-Trip ===" << std::endl;

    for (int i = 0; i < NUM_TEST_LOCATIONS; ++i) {
        const TestLocation& loc = TEST_LOCATIONS[i];

        // Forward: GPS -> ECEF
        ECEF ecef = gps_to_ecef(loc.lat, loc.lon, loc.alt);

        // Reverse: ECEF -> GPS
        GPS_Data gps_result = ecef_to_gps(ecef.x, ecef.y, ecef.z);

        // Check recovery
        TEST_NEAR(gps_result.latitude_deg, loc.lat, TOLERANCE_DEGREE,
                  std::string(loc.name) + ": Latitude recovered");
        TEST_NEAR(gps_result.longitude_deg, loc.lon, TOLERANCE_DEGREE,
                  std::string(loc.name) + ": Longitude recovered");
        TEST_NEAR(gps_result.altitude_m, loc.alt, TOLERANCE_MILLIMETER,
                  std::string(loc.name) + ": Altitude recovered");
    }
}

void test_ecef_to_ned() {
    std::cout << "\n=== Testing ECEF to NED Conversion ===" << std::endl;

    // Test with Munich as reference
    ECEF origin = gps_to_ecef(47.3667, 11.1833, 520.0);
    double origin_lat_rad = 47.3667 * M_PI / 180.0;

    // Same point -> NED should be zero
    LocalFrame ned_zero = ecef_to_ned(origin, origin, origin_lat_rad);
    TEST_NEAR(ned_zero.north_m, 0.0, 0.5, "NED same point: North zero");
    TEST_NEAR(ned_zero.east_m, 0.0, 0.5, "NED same point: East zero");
    TEST_NEAR(ned_zero.down_m, 0.0, 0.5, "NED same point: Down zero");

    // Point north of origin
    ECEF point_north = gps_to_ecef(47.3767, 11.1833, 520.0);  // +0.01° latitude
    LocalFrame ned_north = ecef_to_ned(point_north, origin, origin_lat_rad);
    TEST_ASSERT(ned_north.north_m > 900.0,
                "NED north offset: Positive north component");
    TEST_NEAR(ned_north.east_m, 0.0, 20.0, "NED north offset: East minimal");
}

void test_ned_to_ecef_inverse() {
    std::cout << "\n=== Testing NED to ECEF Inverse ===" << std::endl;

    ECEF origin = gps_to_ecef(47.3667, 11.1833, 520.0);
    ECEF point = gps_to_ecef(47.3677, 11.1842, 540.0);
    double origin_lat_rad = 47.3667 * M_PI / 180.0;

    // Forward: ECEF -> NED
    LocalFrame ned = ecef_to_ned(point, origin, origin_lat_rad);

    // Reverse: NED -> ECEF
    ECEF point_recovered = ned_to_ecef(ned, origin, origin_lat_rad);

    // Should recover original point
    TEST_NEAR(point_recovered.x, point.x, TOLERANCE_METER, "NED to ECEF: X recovered");
    TEST_NEAR(point_recovered.y, point.y, TOLERANCE_METER, "NED to ECEF: Y recovered");
    TEST_NEAR(point_recovered.z, point.z, TOLERANCE_METER, "NED to ECEF: Z recovered");
}

void test_gps_to_ned_convenience() {
    std::cout << "\n=== Testing GPS to NED Convenience Function ===" << std::endl;

    LocalFrame ned = gps_to_ned(47.3677, 11.1842, 540.0,
                                47.3667, 11.1833, 520.0);

    // Offset: 0.001° latitude (~111m), 0.0009° longitude (~67m), 20m altitude
    // Should show offset north and east
    TEST_ASSERT(ned.north_m > 100.0, "GPS to NED: North offset");
    TEST_ASSERT(ned.east_m > 50.0, "GPS to NED: East offset");
    TEST_ASSERT(ned.down_m < -15.0, "GPS to NED: Up component (negative down)");
}

void test_input_validation() {
    std::cout << "\n=== Testing Input Validation ===" << std::endl;

    // Valid inputs
    TEST_ASSERT(is_valid_gps(0.0, 0.0, 0.0), "Valid GPS: Equator/Prime");
    TEST_ASSERT(is_valid_gps(90.0, 0.0, 0.0), "Valid GPS: North Pole");
    TEST_ASSERT(is_valid_gps(-90.0, 0.0, 0.0), "Valid GPS: South Pole");

    // Invalid inputs
    TEST_ASSERT(!is_valid_gps(91.0, 0.0, 0.0), "Invalid GPS: Latitude > 90");
    TEST_ASSERT(!is_valid_gps(45.0, 181.0, 0.0), "Invalid GPS: Longitude > 180");
    TEST_ASSERT(!is_valid_gps(45.0, 45.0, 50001.0), "Invalid GPS: Altitude too high");
    TEST_ASSERT(!is_valid_gps(std::nan(""), 0.0, 0.0), "Invalid GPS: NaN latitude");
}

void test_magnetic_declination() {
    std::cout << "\n=== Testing Magnetic Declination ===" << std::endl;

    // Test known locations
    double decl_munich = get_magnetic_declination(47.3667, 11.1833);
    TEST_ASSERT(decl_munich > 0.0 && decl_munich < 10.0,
                "Munich declination: Positive (East)");

    double decl_sf = get_magnetic_declination(37.7749, -122.4194);
    TEST_ASSERT(decl_sf > 8.0 && decl_sf < 18.0,
                "San Francisco declination: East");

    double decl_nyc = get_magnetic_declination(40.7128, -74.0060);
    TEST_ASSERT(decl_nyc < 0.0,
                "New York declination: Negative (West)");
}

void test_heading_conversion() {
    std::cout << "\n=== Testing Heading Conversions ===" << std::endl;

    // Apply declination: 45° magnetic + 12° east = 57° true
    double true_heading = apply_declination(45.0, 12.0);
    TEST_NEAR(true_heading, 57.0, 1e-6, "Apply declination: East");

    // Remove declination: 57° true - 12° = 45° magnetic
    double mag_heading = remove_declination(57.0, 12.0);
    TEST_NEAR(mag_heading, 45.0, 1e-6, "Remove declination: East");

    // Wrap-around: 350° + 20° = 370° -> 10°
    double wrapped = apply_declination(350.0, 20.0);
    TEST_NEAR(wrapped, 10.0, 1e-6, "Declination: Wrap at 360");

    // Normalize heading
    TEST_NEAR(normalize_heading(370.0), 10.0, 1e-6, "Normalize: Positive wrap");
    TEST_NEAR(normalize_heading(-45.0), 315.0, 1e-6, "Normalize: Negative wrap");
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    std::cout << "==================================================================" << std::endl;
    std::cout << "Coordinate Transformation Standalone Tests" << std::endl;
    std::cout << "==================================================================" << std::endl;

    // Run all tests
    test_gps_to_ecef();
    test_ecef_to_gps_roundtrip();
    test_ecef_to_ned();
    test_ned_to_ecef_inverse();
    test_gps_to_ned_convenience();
    test_input_validation();
    test_magnetic_declination();
    test_heading_conversion();

    // Print summary
    std::cout << "\n==================================================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "Tests run:    " << tests_run << std::endl;
    std::cout << "Tests passed: " << tests_passed << std::endl;
    std::cout << "Tests failed: " << tests_failed << std::endl;

    if (tests_failed == 0) {
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed." << std::endl;
        return 1;
    }
}
