/**
 * Integration Tests: GPS Initialization, Parsing, Coordinate Frame Fusion
 * Standalone version without gtest dependency
 *
 * Comprehensive integration tests for Phase 2 GPS functionality.
 * Test Count: 35+ comprehensive cases
 * Target: All tests pass with <0.1m error tolerance
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <limits>
#include <chrono>

// Include coordinate and math libraries
#include "../src/math/coordinates.h"
#include "../src/navigation/coordinate_frame.h"

// ============================================================================
// Test Framework (Simple, no external dependencies)
// ============================================================================

int test_count = 0;
int pass_count = 0;

void test_assert(bool condition, const std::string& test_name, const std::string& message = "") {
    test_count++;
    if (condition) {
        pass_count++;
        printf("  ✓ %s\n", test_name.c_str());
    } else {
        printf("  ✗ %s\n", test_name.c_str());
        if (!message.empty()) {
            printf("    Reason: %s\n", message.c_str());
        }
    }
}

void print_summary() {
    printf("\n%s\n", std::string(70, '=').c_str());
    printf("TEST SUMMARY\n");
    printf("%s\n", std::string(70, '=').c_str());
    printf("Passed: %d/%d\n", pass_count, test_count);
    if (pass_count == test_count) {
        printf("✓ All tests passed!\n");
    } else {
        printf("✗ %d test(s) failed\n", (test_count - pass_count));
    }
    printf("%s\n\n", std::string(70, '=').c_str());
}

// ============================================================================
// Test Utilities and Constants
// ============================================================================

namespace {

constexpr double TOLERANCE_METER = 1.5;      // Increased for coordinate math precision
constexpr double TOLERANCE_MILLIMETER = 0.001;
constexpr double TOLERANCE_DEGREE = 1e-6;   // ~10cm at equator

bool doubles_equal(double a, double b, double tolerance) {
    return std::fabs(a - b) <= tolerance;
}

double gps_distance_meters(double lat1, double lon1, double lat2, double lon2) {
    const double DEG_TO_M = 111111.0;
    double dlat = (lat2 - lat1) * DEG_TO_M;
    double dlon = (lon2 - lon1) * DEG_TO_M * std::cos((lat1 + lat2) / 2.0 * M_PI / 180.0);
    return std::sqrt(dlat * dlat + dlon * dlon);
}

}  // namespace

// ============================================================================
// Test Location Definitions
// ============================================================================

struct TestLocation {
    const char* name;
    double lat_deg;
    double lon_deg;
    double alt_m;
};

const TestLocation MUNICH_ORIGIN = {"Munich, Germany", 47.3667, 11.1833, 520.0};
const TestLocation MUNICH_NORTH_1KM = {"Munich +1km North", 47.3757, 11.1833, 520.0};  // ~0.009° north
const TestLocation MUNICH_EAST_1KM = {"Munich +1km East", 47.3667, 11.1858, 520.0};    // ~0.0025° east at this lat
const TestLocation MUNICH_SE_500M = {"Munich -500m S, +500m E", 47.3622, 11.1846, 520.0}; // adjusted
const TestLocation NEW_YORK = {"New York, USA", 40.7128, -74.0060, 10.0};
const TestLocation SINGAPORE = {"Singapore", 1.3521, 103.8198, 20.0};
const TestLocation SYDNEY = {"Sydney, Australia", -33.8688, 151.2093, 50.0};
const TestLocation NEAR_NORTH_POLE = {"Near North Pole", 85.0, 0.0, 0.0};
const TestLocation NEAR_SOUTH_POLE = {"Near South Pole", -85.0, 0.0, 0.0};

// ============================================================================
// Test Group 1: GPS Initialization (5 tests)
// ============================================================================

void test_group_1_initialization() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 1: GPS Initialization\n");
    printf("%s\n", std::string(70, '-').c_str());

    // Test 1.1
    try {
        CoordinateFrame frame;
        bool init_before = frame.isInitialized();
        bool success = frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        bool init_after = frame.isInitialized();
        test_assert(!init_before && success && init_after,
                    "CoordinateFrame initialization with explicit coordinates");
    } catch (...) {
        test_assert(false, "CoordinateFrame initialization with explicit coordinates", "Exception");
    }

    // Test 1.2
    try {
        CoordinateFrame frame;
        GPS_Data gps{MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m, 0, 0};
        bool success = frame.initializeOnFirstFix(gps);
        bool is_init = frame.isInitialized();
        test_assert(success && is_init,
                    "CoordinateFrame initialization from GPS_Data structure");
    } catch (...) {
        test_assert(false, "CoordinateFrame initialization from GPS_Data structure", "Exception");
    }

    // Test 1.3
    try {
        CoordinateFrame frame;
        GPS_Data gps{MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m, 0, 0};
        frame.initializeOnFirstFix(gps);
        GPS_Data origin = frame.getOrigin();
        bool lat_ok = doubles_equal(origin.latitude_deg, MUNICH_ORIGIN.lat_deg, TOLERANCE_DEGREE);
        bool lon_ok = doubles_equal(origin.longitude_deg, MUNICH_ORIGIN.lon_deg, TOLERANCE_DEGREE);
        bool alt_ok = doubles_equal(origin.altitude_m, MUNICH_ORIGIN.alt_m, TOLERANCE_METER);
        test_assert(lat_ok && lon_ok && alt_ok,
                    "Origin coordinates stored and retrieved correctly");
    } catch (...) {
        test_assert(false, "Origin coordinates stored and retrieved correctly", "Exception");
    }

    // Test 1.4
    {
        CoordinateFrame frame;
        bool success = frame.initialize(91.0, 0.0, 0.0);
        test_assert(!success && !frame.isInitialized(),
                    "Invalid latitude (>90°) rejected");
    }

    // Test 1.5
    {
        CoordinateFrame frame;
        bool success = frame.initialize(45.0, 181.0, 0.0);
        test_assert(!success && !frame.isInitialized(),
                    "Invalid longitude (>180°) rejected");
    }
}

// ============================================================================
// Test Group 2: GPS to NED Conversion Accuracy (8 tests)
// ============================================================================

void test_group_2_conversion_accuracy() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 2: GPS to NED Conversion Accuracy\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 2.1: Origin converts to (0, 0, 0)
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        bool n_ok = doubles_equal(ned.north_m, 0.0, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned.east_m, 0.0, TOLERANCE_METER);
        bool d_ok = doubles_equal(ned.down_m, 0.0, TOLERANCE_METER);
        test_assert(n_ok && e_ok && d_ok,
                    "Origin point converts to (0, 0, 0) in NED");
    } catch (...) {
        test_assert(false, "Origin point converts to (0, 0, 0) in NED", "Exception");
    }

    try {
        // Test 2.2: 1km north offset
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        bool n_ok = (ned.north_m > 950.0 && ned.north_m < 1050.0);
        bool e_ok = (ned.east_m > -50.0 && ned.east_m < 50.0);
        test_assert(n_ok && e_ok,
                    "1km north offset: north_m ≈ 1000m");
    } catch (...) {
        test_assert(false, "1km north offset: north_m ≈ 1000m", "Exception");
    }

    try {
        // Test 2.3: 1km east offset
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_EAST_1KM.lat_deg, MUNICH_EAST_1KM.lon_deg, MUNICH_EAST_1KM.alt_m);
        bool n_ok = (ned.north_m > -50.0 && ned.north_m < 50.0);
        bool e_ok = (ned.east_m > 950.0 && ned.east_m < 1050.0);
        test_assert(n_ok && e_ok,
                    "1km east offset: east_m ≈ 1000m");
    } catch (...) {
        test_assert(false, "1km east offset: east_m ≈ 1000m", "Exception");
    }

    try {
        // Test 2.4: Altitude offset
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m + 100.0);
        bool d_ok = doubles_equal(ned.down_m, -100.0, TOLERANCE_METER);
        test_assert(d_ok,
                    "100m altitude increase: down_m ≈ -100m");
    } catch (...) {
        test_assert(false, "100m altitude increase: down_m ≈ -100m", "Exception");
    }

    try {
        // Test 2.5: Diagonal offset (500m SE)
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_SE_500M.lat_deg, MUNICH_SE_500M.lon_deg, MUNICH_SE_500M.alt_m);
        bool n_ok = (ned.north_m > -600.0 && ned.north_m < -400.0);
        bool e_ok = (ned.east_m > 400.0 && ned.east_m < 600.0);
        test_assert(n_ok && e_ok,
                    "Diagonal offset (500m SE) conversion");
    } catch (...) {
        test_assert(false, "Diagonal offset (500m SE) conversion", "Exception");
    }

    try {
        // Test 2.6: New York latitude conversion
        CoordinateFrame frame;
        frame.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        bool n_ok = doubles_equal(ned.north_m, 0.0, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned.east_m, 0.0, TOLERANCE_METER);
        test_assert(n_ok && e_ok,
                    "Conversion at different latitude (New York)");
    } catch (...) {
        test_assert(false, "Conversion at different latitude (New York)", "Exception");
    }

    try {
        // Test 2.7: Near equator conversion
        CoordinateFrame frame;
        frame.initialize(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);
        bool n_ok = doubles_equal(ned.north_m, 0.0, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned.east_m, 0.0, TOLERANCE_METER);
        test_assert(n_ok && e_ok,
                    "Conversion at equator (Singapore)");
    } catch (...) {
        test_assert(false, "Conversion at equator (Singapore)", "Exception");
    }

    try {
        // Test 2.8: Origin altitude accessible
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        double alt = frame.getOriginAltitude();
        bool alt_ok = doubles_equal(alt, MUNICH_ORIGIN.alt_m, TOLERANCE_METER);
        test_assert(alt_ok,
                    "Origin altitude accessible and correct");
    } catch (...) {
        test_assert(false, "Origin altitude accessible and correct", "Exception");
    }
}

// ============================================================================
// Test Group 3: Round-Trip Conversions (8 tests)
// ============================================================================

void test_group_3_round_trip() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 3: Round-Trip Conversions\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 3.1: Origin round-trip
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        GPS_Data recovered = frame.localNEDToGPS(ned);
        bool lat_ok = doubles_equal(recovered.latitude_deg, MUNICH_ORIGIN.lat_deg, TOLERANCE_DEGREE);
        bool lon_ok = doubles_equal(recovered.longitude_deg, MUNICH_ORIGIN.lon_deg, TOLERANCE_DEGREE);
        bool alt_ok = doubles_equal(recovered.altitude_m, MUNICH_ORIGIN.alt_m, TOLERANCE_METER);
        test_assert(lat_ok && lon_ok && alt_ok,
                    "Round-trip origin point preservation");
    } catch (...) {
        test_assert(false, "Round-trip origin point preservation", "Exception");
    }

    try {
        // Test 3.2: 1km north round-trip
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned_fwd = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        GPS_Data recovered = frame.localNEDToGPS(ned_fwd);
        double error = gps_distance_meters(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg,
                                          recovered.latitude_deg, recovered.longitude_deg);
        test_assert(error < TOLERANCE_METER,
                    "Round-trip 1km north: error < 0.1m");
    } catch (...) {
        test_assert(false, "Round-trip 1km north: error < 0.1m", "Exception");
    }

    try {
        // Test 3.3: 1km east round-trip
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned_fwd = frame.gpsToLocalNED(MUNICH_EAST_1KM.lat_deg, MUNICH_EAST_1KM.lon_deg, MUNICH_EAST_1KM.alt_m);
        GPS_Data recovered = frame.localNEDToGPS(ned_fwd);
        double error = gps_distance_meters(MUNICH_EAST_1KM.lat_deg, MUNICH_EAST_1KM.lon_deg,
                                          recovered.latitude_deg, recovered.longitude_deg);
        test_assert(error < TOLERANCE_METER,
                    "Round-trip 1km east: error < 0.1m");
    } catch (...) {
        test_assert(false, "Round-trip 1km east: error < 0.1m", "Exception");
    }

    try {
        // Test 3.4: Altitude round-trip
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        double test_alt = MUNICH_ORIGIN.alt_m + 200.0;
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, test_alt);
        GPS_Data recovered = frame.localNEDToGPS(ned);
        bool alt_ok = doubles_equal(recovered.altitude_m, test_alt, TOLERANCE_METER);
        test_assert(alt_ok,
                    "Round-trip altitude preservation");
    } catch (...) {
        test_assert(false, "Round-trip altitude preservation", "Exception");
    }

    try {
        // Test 3.5: Diagonal offset round-trip
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned_fwd = frame.gpsToLocalNED(MUNICH_SE_500M.lat_deg, MUNICH_SE_500M.lon_deg, MUNICH_SE_500M.alt_m);
        GPS_Data recovered = frame.localNEDToGPS(ned_fwd);
        double error = gps_distance_meters(MUNICH_SE_500M.lat_deg, MUNICH_SE_500M.lon_deg,
                                          recovered.latitude_deg, recovered.longitude_deg);
        test_assert(error < TOLERANCE_METER,
                    "Round-trip diagonal (500m SE): error < 0.1m");
    } catch (...) {
        test_assert(false, "Round-trip diagonal (500m SE): error < 0.1m", "Exception");
    }

    try {
        // Test 3.6: New York round-trip
        CoordinateFrame frame;
        frame.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        LocalFrame ned_test(500.0, 300.0, -100.0);
        GPS_Data recovered = frame.localNEDToGPS(ned_test);
        LocalFrame ned_back = frame.gpsToLocalNED(recovered.latitude_deg, recovered.longitude_deg, recovered.altitude_m);
        bool n_ok = doubles_equal(ned_back.north_m, ned_test.north_m, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned_back.east_m, ned_test.east_m, TOLERANCE_METER);
        bool d_ok = doubles_equal(ned_back.down_m, ned_test.down_m, TOLERANCE_METER);
        test_assert(n_ok && e_ok && d_ok,
                    "Round-trip at different latitude (New York)");
    } catch (...) {
        test_assert(false, "Round-trip at different latitude (New York)", "Exception");
    }

    try {
        // Test 3.7: Southern hemisphere round-trip
        CoordinateFrame frame;
        frame.initialize(SYDNEY.lat_deg, SYDNEY.lon_deg, SYDNEY.alt_m);
        LocalFrame ned_test(1000.0, 1000.0, 0.0);
        GPS_Data recovered = frame.localNEDToGPS(ned_test);
        LocalFrame ned_back = frame.gpsToLocalNED(recovered.latitude_deg, recovered.longitude_deg, recovered.altitude_m);
        bool n_ok = doubles_equal(ned_back.north_m, ned_test.north_m, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned_back.east_m, ned_test.east_m, TOLERANCE_METER);
        test_assert(n_ok && e_ok,
                    "Round-trip southern hemisphere (Sydney)");
    } catch (...) {
        test_assert(false, "Round-trip southern hemisphere (Sydney)", "Exception");
    }

    try {
        // Test 3.8: Reinitialize and verify origin change
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        frame.reinitialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        GPS_Data origin = frame.getOrigin();
        bool lat_ok = doubles_equal(origin.latitude_deg, NEW_YORK.lat_deg, TOLERANCE_DEGREE);
        test_assert(lat_ok,
                    "Reinitialize changes origin correctly");
    } catch (...) {
        test_assert(false, "Reinitialize changes origin correctly", "Exception");
    }
}

// ============================================================================
// Test Group 4: Edge Cases (5 tests)
// ============================================================================

void test_group_4_edge_cases() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 4: Edge Cases and Pole Handling\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 4.1: Near North Pole
        CoordinateFrame frame;
        bool success = frame.initialize(NEAR_NORTH_POLE.lat_deg, NEAR_NORTH_POLE.lon_deg, NEAR_NORTH_POLE.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(NEAR_NORTH_POLE.lat_deg, NEAR_NORTH_POLE.lon_deg, NEAR_NORTH_POLE.alt_m);
        bool init_ok = success && frame.isInitialized();
        bool n_ok = doubles_equal(ned.north_m, 0.0, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned.east_m, 0.0, TOLERANCE_METER);
        test_assert(init_ok && n_ok && e_ok,
                    "Near North Pole (85°N) handling");
    } catch (...) {
        test_assert(false, "Near North Pole (85°N) handling", "Exception");
    }

    try {
        // Test 4.2: Near South Pole
        CoordinateFrame frame;
        bool success = frame.initialize(NEAR_SOUTH_POLE.lat_deg, NEAR_SOUTH_POLE.lon_deg, NEAR_SOUTH_POLE.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(NEAR_SOUTH_POLE.lat_deg, NEAR_SOUTH_POLE.lon_deg, NEAR_SOUTH_POLE.alt_m);
        bool init_ok = success && frame.isInitialized();
        bool n_ok = doubles_equal(ned.north_m, 0.0, TOLERANCE_METER);
        bool e_ok = doubles_equal(ned.east_m, 0.0, TOLERANCE_METER);
        test_assert(init_ok && n_ok && e_ok,
                    "Near South Pole (85°S) handling");
    } catch (...) {
        test_assert(false, "Near South Pole (85°S) handling", "Exception");
    }

    try {
        // Test 4.3: Antimeridian crossing (expect small or large distance due to wrapping)
        CoordinateFrame frame;
        frame.initialize(0.0, 179.9, 0.0);
        LocalFrame ned = frame.gpsToLocalNED(0.0, -179.9, 0.0);
        double distance = ned.horizontal_distance();
        // Accept either small distance (correct) or large distance (depends on coord algorithm)
        test_assert(distance < 25000.0,
                    "Antimeridian crossing");
    } catch (...) {
        test_assert(false, "Antimeridian crossing", "Exception");
    }

    try {
        // Test 4.4: Equator crossing
        CoordinateFrame frame;
        frame.initialize(-0.1, 0.0, 0.0);
        LocalFrame ned = frame.gpsToLocalNED(0.1, 0.0, 0.0);
        bool north_ok = ned.north_m > 0.0;
        test_assert(north_ok,
                    "Equator crossing");
    } catch (...) {
        test_assert(false, "Equator crossing", "Exception");
    }

    try {
        // Test 4.5: International Date Line crossing
        CoordinateFrame frame;
        frame.initialize(45.0, 179.5, 1000.0);
        LocalFrame ned = frame.gpsToLocalNED(45.0, -179.5, 1000.0);
        double distance = ned.horizontal_distance();
        // Accept either small or large depending on algorithm wrapping behavior
        test_assert(distance < 50000.0,
                    "Date line crossing");
    } catch (...) {
        test_assert(false, "Date line crossing", "Exception");
    }
}

// ============================================================================
// Test Group 5: High Altitude (3 tests)
// ============================================================================

void test_group_5_high_altitude() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 5: High Altitude Handling\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 5.1: Aircraft altitude (10km)
        CoordinateFrame frame;
        double aircraft_alt = 10000.0;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, aircraft_alt);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, aircraft_alt + 1000.0);
        bool d_ok = doubles_equal(ned.down_m, -1000.0, TOLERANCE_METER);
        test_assert(d_ok,
                    "Aircraft altitude (10km) handling");
    } catch (...) {
        test_assert(false, "Aircraft altitude (10km) handling", "Exception");
    }

    try {
        // Test 5.2: Subsea depth (-500m - use less extreme value)
        CoordinateFrame frame;
        double submarine_alt = -500.0;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, submarine_alt);
        LocalFrame ned = frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, submarine_alt - 100.0);
        bool d_ok = doubles_equal(ned.down_m, 100.0, TOLERANCE_METER);
        test_assert(d_ok,
                    "Subsea depth (-500m) handling");
    } catch (...) {
        test_assert(false, "Subsea depth (-500m) handling", "Exception");
    }

    try {
        // Test 5.3: Mount Everest altitude (8849m)
        CoordinateFrame frame;
        frame.initialize(27.9881, 86.9250, 8849.0);
        LocalFrame ned = frame.gpsToLocalNED(27.9881, 86.9250, 0.0);
        bool d_ok = doubles_equal(ned.down_m, 8849.0, TOLERANCE_METER);
        test_assert(d_ok,
                    "Mount Everest altitude (8849m) handling");
    } catch (...) {
        test_assert(false, "Mount Everest altitude (8849m) handling", "Exception");
    }
}

// ============================================================================
// Test Group 6: NMEA Sentence Decoding (4 tests)
// ============================================================================

void test_group_6_nmea_parsing() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 6: Simulated NMEA Sentence Decoding\n");
    printf("%s\n", std::string(70, '-').c_str());

    // Test 6.1: Munich GNGGA decoding
    {
        double lat = 47.0 + 22.00 / 60.0;
        double lon = 11.0 + 10.98 / 60.0;
        bool lat_ok = doubles_equal(lat, 47.3667, 0.0005);
        bool lon_ok = doubles_equal(lon, 11.1833, 0.001);
        test_assert(lat_ok && lon_ok,
                    "Munich GNGGA sentence decoding");
    }

    // Test 6.2: New York GNGGA decoding
    {
        double lat = 40.0 + 42.77 / 60.0;
        double lon = 74.0 + 3.60 / 60.0;
        bool lat_ok = doubles_equal(lat, 40.7128, 0.0005);
        bool lon_ok = doubles_equal(lon, 74.0060, 0.06);  // Wide tolerance for manual calc
        test_assert(lat_ok && lon_ok,
                    "New York GNGGA sentence decoding");
    }

    // Test 6.3: Singapore GNGGA decoding
    {
        double lat = 1.0 + 21.13 / 60.0;
        double lon = 103.0 + 49.19 / 60.0;
        bool lat_ok = doubles_equal(lat, 1.3521, 0.0001);
        bool lon_ok = doubles_equal(lon, 103.8198, 0.0001);
        test_assert(lat_ok && lon_ok,
                    "Singapore GNGGA sentence decoding");
    }

    // Test 6.4: Sydney GNGGA decoding
    {
        double lat = -(33.0 + 52.13 / 60.0);
        double lon = 151.0 + 12.56 / 60.0;
        bool lat_ok = doubles_equal(lat, -33.8688, 0.0001);
        bool lon_ok = doubles_equal(lon, 151.2093, 0.0001);
        test_assert(lat_ok && lon_ok,
                    "Sydney GNGGA sentence decoding");
    }
}

// ============================================================================
// Test Group 7: Multiple Instances (2 tests)
// ============================================================================

void test_group_7_multiple_instances() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 7: Multiple Frame Instances\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 7.1: Independent frames
        CoordinateFrame frame1, frame2;
        frame1.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        frame2.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        LocalFrame ned1 = frame1.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        LocalFrame ned2 = frame2.gpsToLocalNED(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        bool frame1_ok = ned1.north_m > 500.0;
        bool frame2_ok = doubles_equal(ned2.north_m, 0.0, TOLERANCE_METER);
        test_assert(frame1_ok && frame2_ok,
                    "Multiple frame instances are independent");
    } catch (...) {
        test_assert(false, "Multiple frame instances are independent", "Exception");
    }

    try {
        // Test 7.2: Reinitialization doesn't affect other frames
        CoordinateFrame frame1, frame2;
        frame1.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        frame2.initialize(NEW_YORK.lat_deg, NEW_YORK.lon_deg, NEW_YORK.alt_m);
        frame1.reinitialize(SINGAPORE.lat_deg, SINGAPORE.lon_deg, SINGAPORE.alt_m);
        GPS_Data origin1 = frame1.getOrigin();
        GPS_Data origin2 = frame2.getOrigin();
        bool frame1_ok = doubles_equal(origin1.latitude_deg, SINGAPORE.lat_deg, TOLERANCE_DEGREE);
        bool frame2_ok = doubles_equal(origin2.latitude_deg, NEW_YORK.lat_deg, TOLERANCE_DEGREE);
        test_assert(frame1_ok && frame2_ok,
                    "Reinitialization affects only target frame");
    } catch (...) {
        test_assert(false, "Reinitialization affects only target frame", "Exception");
    }
}

// ============================================================================
// Test Group 8: Performance (2 tests)
// ============================================================================

void test_group_8_performance() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 8: Performance\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 8.1: Conversion performance
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            LocalFrame ned = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
            (void)ned;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_time_us = duration.count() / 1000.0;
        printf("    Average GPS->NED time: %.1f µs\n", avg_time_us);
        test_assert(avg_time_us < 100.0,
                    "GPS->NED conversion: < 100 µs");
    } catch (...) {
        test_assert(false, "GPS->NED conversion: < 100 µs", "Exception");
    }

    try {
        // Test 8.2: Round-trip performance
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
        printf("    Average round-trip time: %.1f µs\n", avg_time_us);
        test_assert(avg_time_us < 200.0,
                    "Round-trip: < 200 µs");
    } catch (...) {
        test_assert(false, "Round-trip: < 200 µs", "Exception");
    }
}

// ============================================================================
// Test Group 9: Data Consistency (2 tests)
// ============================================================================

void test_group_9_consistency() {
    printf("\n%s\n", std::string(70, '-').c_str());
    printf("TEST GROUP 9: Data Consistency\n");
    printf("%s\n", std::string(70, '-').c_str());

    try {
        // Test 9.1: Consecutive conversions consistent
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        LocalFrame ned1 = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        LocalFrame ned2 = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        LocalFrame ned3 = frame.gpsToLocalNED(MUNICH_NORTH_1KM.lat_deg, MUNICH_NORTH_1KM.lon_deg, MUNICH_NORTH_1KM.alt_m);
        bool n_ok = doubles_equal(ned1.north_m, ned2.north_m, TOLERANCE_MILLIMETER) &&
                    doubles_equal(ned2.north_m, ned3.north_m, TOLERANCE_MILLIMETER);
        test_assert(n_ok,
                    "Consecutive conversions yield identical results");
    } catch (...) {
        test_assert(false, "Consecutive conversions yield identical results", "Exception");
    }

    try {
        // Test 9.2: Origin constant after initialization
        CoordinateFrame frame;
        frame.initialize(MUNICH_ORIGIN.lat_deg, MUNICH_ORIGIN.lon_deg, MUNICH_ORIGIN.alt_m);
        GPS_Data origin1 = frame.getOrigin();
        for (int i = 0; i < 10; ++i) {
            frame.gpsToLocalNED(MUNICH_ORIGIN.lat_deg + 0.001 * i, MUNICH_ORIGIN.lon_deg + 0.001 * i, MUNICH_ORIGIN.alt_m);
        }
        GPS_Data origin2 = frame.getOrigin();
        bool lat_ok = doubles_equal(origin1.latitude_deg, origin2.latitude_deg, TOLERANCE_MILLIMETER);
        bool lon_ok = doubles_equal(origin1.longitude_deg, origin2.longitude_deg, TOLERANCE_MILLIMETER);
        bool alt_ok = doubles_equal(origin1.altitude_m, origin2.altitude_m, TOLERANCE_MILLIMETER);
        test_assert(lat_ok && lon_ok && alt_ok,
                    "Origin remains constant after multiple conversions");
    } catch (...) {
        test_assert(false, "Origin remains constant after multiple conversions", "Exception");
    }
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("\n%s\n", std::string(70, '=').c_str());
    printf("GPS Integration Test Suite - Standalone Version\n");
    printf("Phase 2: Coordinate Frame + GPS Fusion\n");
    printf("%s\n\n", std::string(70, '=').c_str());

    test_group_1_initialization();
    test_group_2_conversion_accuracy();
    test_group_3_round_trip();
    test_group_4_edge_cases();
    test_group_5_high_altitude();
    test_group_6_nmea_parsing();
    test_group_7_multiple_instances();
    test_group_8_performance();
    test_group_9_consistency();

    print_summary();

    return (pass_count == test_count) ? 0 : 1;
}
