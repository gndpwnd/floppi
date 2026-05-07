/**
 * Unit Tests for CoordinateFrame (Standalone - No GTest dependency)
 *
 * Tests the coordinate frame manager that maintains a local NED origin from
 * the first GPS fix and provides conversions between GPS and NED coordinates.
 *
 * Compile with: g++ -std=c++17 -Wall -Wextra -I. \
 *   src/math/coordinates.cpp \
 *   src/navigation/coordinate_frame.cpp \
 *   tests/test_coordinate_frame_standalone.cpp \
 *   -o tests/test_coordinate_frame_standalone
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <sstream>
#include <functional>
#include "../src/math/coordinates.h"
#include "../src/navigation/coordinate_frame.h"

// ============================================================================
// TEST FRAMEWORK
// ============================================================================

class TestRunner {
 public:
    int test_count = 0;
    int pass_count = 0;
    int fail_count = 0;

    void assert_true(bool condition, const char* test_name, const char* message) {
        test_count++;
        if (condition) {
            pass_count++;
            std::cout << "  [PASS] " << test_name << " - " << message << std::endl;
        } else {
            fail_count++;
            std::cout << "  [FAIL] " << test_name << " - " << message << std::endl;
        }
    }

    void assert_near(double actual, double expected, double tolerance,
                     const char* test_name, const char* message) {
        test_count++;
        double error = std::abs(actual - expected);
        if (error <= tolerance) {
            pass_count++;
            std::cout << "  [PASS] " << test_name << " - " << message
                      << " (error: " << error << "m)" << std::endl;
        } else {
            fail_count++;
            std::cout << "  [FAIL] " << test_name << " - " << message
                      << " (error: " << error << "m, tolerance: " << tolerance << "m)"
                      << std::endl;
        }
    }

    void assert_throw(std::function<void()> func, const char* test_name,
                      const char* message) {
        test_count++;
        try {
            func();
            fail_count++;
            std::cout << "  [FAIL] " << test_name << " - " << message
                      << " (no exception thrown)" << std::endl;
        } catch (const std::runtime_error& e) {
            pass_count++;
            std::cout << "  [PASS] " << test_name << " - " << message
                      << " (exception: " << e.what() << ")" << std::endl;
        } catch (const std::exception& e) {
            fail_count++;
            std::cout << "  [FAIL] " << test_name << " - " << message
                      << " (unexpected exception: " << e.what() << ")" << std::endl;
        }
    }

    void summary() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed:      " << pass_count << std::endl;
        std::cout << "Failed:      " << fail_count << std::endl;
        std::cout << std::string(70, '=') << std::endl;
    }

    int get_fail_count() const { return fail_count; }
};

// ============================================================================
// TEST LOCATIONS
// ============================================================================

struct TestLocation {
    const char* name;
    double lat_deg;
    double lon_deg;
    double alt_m;
};

const TestLocation MUNICH = {"Munich, Germany", 48.1351, 11.5820, 530.0};
const TestLocation LOS_ANGELES = {"Los Angeles, USA", 34.0522, -118.2437, 100.0};
const TestLocation SINGAPORE = {"Singapore", 1.3521, 103.8198, 20.0};
const TestLocation EQUATOR_PRIME = {"Equator, Prime Meridian", 0.0, 0.0, 0.0};
const TestLocation SYDNEY = {"Sydney, Australia", -33.8688, 151.2093, 30.0};

// Tolerances
const double TOLERANCE_METER = 1.0;
const double TOLERANCE_CM = 0.01;
const double TOLERANCE_DEGREE = 1e-7;
const double TOLERANCE_ROUNDTRIP = 0.1;

// ============================================================================
// TESTS
// ============================================================================

void test_initialization(TestRunner& runner) {
    std::cout << "\n[INITIALIZATION TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        runner.assert_true(!frame.isInitialized(), "test_init_1",
                          "New frame should not be initialized");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
        runner.assert_true(success, "test_init_2", "Initialize with valid coordinates");
        runner.assert_true(frame.isInitialized(), "test_init_2b",
                          "Frame should be initialized after initialize()");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(91.0, 0.0, 0.0);
        runner.assert_true(!success, "test_init_3", "Invalid latitude (91°) should fail");
        runner.assert_true(!frame.isInitialized(), "test_init_3b",
                          "Frame should not be initialized after failed initialize()");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 181.0, 0.0);
        runner.assert_true(!success, "test_init_4", "Invalid longitude (181°) should fail");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 0.0, 50001.0);
        runner.assert_true(!success, "test_init_5",
                          "Invalid altitude (>50km) should fail");
    }

    {
        CoordinateFrame frame;
        GPS_Data gps(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
        bool success = frame.initializeOnFirstFix(gps);
        runner.assert_true(success, "test_init_6",
                          "initializeOnFirstFix with valid GPS data");
        runner.assert_true(frame.isInitialized(), "test_init_6b",
                          "Frame initialized via initializeOnFirstFix");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 0.0, 0.0);
        runner.assert_true(success, "test_init_7", "Initialize at sea level");
        runner.assert_near(frame.getOriginAltitude(), 0.0, TOLERANCE_CM, "test_init_7b",
                          "Origin altitude at sea level");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 0.0, 3000.0);
        runner.assert_true(success, "test_init_8", "Initialize at 3000m elevation");
        runner.assert_near(frame.getOriginAltitude(), 3000.0, TOLERANCE_CM,
                          "test_init_8b", "Origin altitude at 3000m");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 0.0, -400.0);
        runner.assert_true(success, "test_init_9",
                          "Initialize at -400m (deep ocean)");
        runner.assert_near(frame.getOriginAltitude(), -400.0, TOLERANCE_CM,
                          "test_init_9b", "Origin altitude at -400m");
    }
}

void test_gps_to_ned(TestRunner& runner) {
    std::cout << "\n[GPS TO NED CONVERSION TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        LocalFrame ned = frame.gpsToLocalNED(MUNICH.lat_deg, MUNICH.lon_deg,
                                            MUNICH.alt_m);
        runner.assert_near(ned.north_m, 0.0, TOLERANCE_METER, "test_gps2ned_1",
                          "NED at origin - north");
        runner.assert_near(ned.east_m, 0.0, TOLERANCE_METER, "test_gps2ned_1b",
                          "NED at origin - east");
        runner.assert_near(ned.down_m, 0.0, TOLERANCE_METER, "test_gps2ned_1c",
                          "NED at origin - down");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        LocalFrame ned = frame.gpsToLocalNED(MUNICH.lat_deg + 0.001, MUNICH.lon_deg,
                                            MUNICH.alt_m);
        runner.assert_true(ned.north_m > 100.0, "test_gps2ned_2",
                          "Offset north gives positive north component");
        runner.assert_near(ned.east_m, 0.0, TOLERANCE_METER, "test_gps2ned_2b",
                          "Offset north - minimal east");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        LocalFrame ned = frame.gpsToLocalNED(MUNICH.lat_deg, MUNICH.lon_deg + 0.001,
                                            MUNICH.alt_m);
        runner.assert_true(ned.east_m > 60.0, "test_gps2ned_3",
                          "Offset east gives positive east component");
        runner.assert_near(ned.north_m, 0.0, TOLERANCE_METER, "test_gps2ned_3b",
                          "Offset east - minimal north");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        LocalFrame ned = frame.gpsToLocalNED(MUNICH.lat_deg, MUNICH.lon_deg,
                                            MUNICH.alt_m + 100.0);
        runner.assert_true(ned.down_m < -90.0, "test_gps2ned_4",
                          "Altitude increase gives negative down");
        runner.assert_near(ned.north_m, 0.0, TOLERANCE_METER, "test_gps2ned_4b",
                          "Altitude increase - minimal north");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        LocalFrame ned = frame.gpsToLocalNED(91.0, 0.0, 0.0);
        runner.assert_true(std::isnan(ned.north_m), "test_gps2ned_5",
                          "Invalid coordinates return NaN");
    }

    {
        CoordinateFrame frame;
        runner.assert_throw(
            [&frame]() {
                frame.gpsToLocalNED(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
            },
            "test_gps2ned_6", "Conversion before initialization throws");
    }
}

void test_ned_to_gps(TestRunner& runner) {
    std::cout << "\n[NED TO GPS CONVERSION TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        GPS_Data gps = frame.localNEDToGPS(LocalFrame(0.0, 0.0, 0.0));
        runner.assert_near(gps.latitude_deg, MUNICH.lat_deg, TOLERANCE_DEGREE,
                          "test_ned2gps_1", "NED origin - latitude");
        runner.assert_near(gps.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE,
                          "test_ned2gps_1b", "NED origin - longitude");
        runner.assert_near(gps.altitude_m, MUNICH.alt_m, TOLERANCE_CM,
                          "test_ned2gps_1c", "NED origin - altitude");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        GPS_Data gps = frame.localNEDToGPS(LocalFrame(111.0, 0.0, 0.0));
        runner.assert_true(gps.latitude_deg > MUNICH.lat_deg, "test_ned2gps_2",
                          "NED north offset gives higher latitude");
        runner.assert_near(gps.longitude_deg, MUNICH.lon_deg, TOLERANCE_DEGREE,
                          "test_ned2gps_2b", "NED north - longitude unchanged");
    }

    {
        CoordinateFrame frame;
        runner.assert_throw(
            [&frame]() { frame.localNEDToGPS(LocalFrame(0.0, 0.0, 0.0)); },
            "test_ned2gps_3", "Conversion before initialization throws");
    }
}

void test_round_trip(TestRunner& runner) {
    std::cout << "\n[ROUND-TRIP TESTS (GPS -> NED -> GPS)]" << std::endl;

    // Test at Munich
    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        GPS_Data original(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                            original.altitude_m);
        GPS_Data result = frame.localNEDToGPS(ned);

        runner.assert_near(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_1", "Munich round-trip - latitude");
        runner.assert_near(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_1b", "Munich round-trip - longitude");
        runner.assert_near(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP,
                          "test_roundtrip_1c", "Munich round-trip - altitude");
    }

    // Test with small offset
    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        GPS_Data original(MUNICH.lat_deg + 0.0005, MUNICH.lon_deg + 0.0005,
                         MUNICH.alt_m + 50.0);
        LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                            original.altitude_m);
        GPS_Data result = frame.localNEDToGPS(ned);

        runner.assert_near(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_2", "Small offset round-trip - latitude");
        runner.assert_near(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_2b", "Small offset round-trip - longitude");
        runner.assert_near(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP,
                          "test_roundtrip_2c", "Small offset round-trip - altitude");
    }

    // Test with larger offset (~10 km)
    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        GPS_Data original(MUNICH.lat_deg + 0.1, MUNICH.lon_deg + 0.1,
                         MUNICH.alt_m + 100.0);
        LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                            original.altitude_m);
        GPS_Data result = frame.localNEDToGPS(ned);

        runner.assert_near(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_3", "Large offset round-trip - latitude");
        runner.assert_near(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_3b", "Large offset round-trip - longitude");
        runner.assert_near(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP,
                          "test_roundtrip_3c", "Large offset round-trip - altitude");
    }

    // Test at Los Angeles
    {
        CoordinateFrame frame;
        frame.initialize(LOS_ANGELES.lat_deg, LOS_ANGELES.lon_deg, LOS_ANGELES.alt_m);

        GPS_Data original(LOS_ANGELES.lat_deg, LOS_ANGELES.lon_deg, LOS_ANGELES.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                            original.altitude_m);
        GPS_Data result = frame.localNEDToGPS(ned);

        runner.assert_near(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_4", "LA round-trip - latitude");
        runner.assert_near(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_4b", "LA round-trip - longitude");
        runner.assert_near(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP,
                          "test_roundtrip_4c", "LA round-trip - altitude");
    }

    // Test at equator
    {
        CoordinateFrame frame;
        frame.initialize(EQUATOR_PRIME.lat_deg, EQUATOR_PRIME.lon_deg,
                        EQUATOR_PRIME.alt_m);

        GPS_Data original(EQUATOR_PRIME.lat_deg, EQUATOR_PRIME.lon_deg,
                         EQUATOR_PRIME.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                            original.altitude_m);
        GPS_Data result = frame.localNEDToGPS(ned);

        runner.assert_near(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_5", "Equator round-trip - latitude");
        runner.assert_near(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_5b", "Equator round-trip - longitude");
        runner.assert_near(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP,
                          "test_roundtrip_5c", "Equator round-trip - altitude");
    }

    // Test at Sydney (southern hemisphere)
    {
        CoordinateFrame frame;
        frame.initialize(SYDNEY.lat_deg, SYDNEY.lon_deg, SYDNEY.alt_m);

        GPS_Data original(SYDNEY.lat_deg, SYDNEY.lon_deg, SYDNEY.alt_m);
        LocalFrame ned = frame.gpsToLocalNED(original.latitude_deg, original.longitude_deg,
                                            original.altitude_m);
        GPS_Data result = frame.localNEDToGPS(ned);

        runner.assert_near(result.latitude_deg, original.latitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_6", "Sydney round-trip - latitude");
        runner.assert_near(result.longitude_deg, original.longitude_deg, TOLERANCE_DEGREE,
                          "test_roundtrip_6b", "Sydney round-trip - longitude");
        runner.assert_near(result.altitude_m, original.altitude_m, TOLERANCE_ROUNDTRIP,
                          "test_roundtrip_6c", "Sydney round-trip - altitude");
    }
}

void test_reinitialization(TestRunner& runner) {
    std::cout << "\n[REINITIALIZATION TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);
        runner.assert_true(frame.isInitialized(), "test_reinit_1",
                          "Frame initialized at Munich");

        bool success =
            frame.reinitialize(LOS_ANGELES.lat_deg, LOS_ANGELES.lon_deg,
                              LOS_ANGELES.alt_m);
        runner.assert_true(success, "test_reinit_1b",
                          "Reinitialization at LA succeeded");

        GPS_Data origin = frame.getOrigin();
        runner.assert_near(origin.latitude_deg, LOS_ANGELES.lat_deg, TOLERANCE_DEGREE,
                          "test_reinit_1c", "Origin changed to LA latitude");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        bool success = frame.reinitialize(91.0, 0.0, 0.0);
        runner.assert_true(!success, "test_reinit_2",
                          "Reinitialization with invalid coordinates fails");

        // After failed reinitialization, frame becomes uninitialized
        runner.assert_true(!frame.isInitialized(), "test_reinit_2b",
                          "Frame becomes uninitialized after failed reinitialization");
    }
}

void test_boundary_conditions(TestRunner& runner) {
    std::cout << "\n[BOUNDARY CONDITION TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 0.0, 0.0);
        runner.assert_true(success, "test_boundary_1",
                          "Initialize at equator and prime meridian");

        LocalFrame ned = frame.gpsToLocalNED(0.001, 0.001, 0.0);
        runner.assert_true(ned.north_m > 100.0, "test_boundary_1b",
                          "Conversion at equator - north component");
        runner.assert_true(ned.east_m > 100.0, "test_boundary_1c",
                          "Conversion at equator - east component");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(89.0, 0.0, 0.0);
        runner.assert_true(success, "test_boundary_2",
                          "Initialize near north pole");

        LocalFrame ned = frame.gpsToLocalNED(89.001, 0.0, 0.0);
        runner.assert_true(ned.north_m > 100.0, "test_boundary_2b",
                          "Conversion near north pole");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(-89.0, 0.0, 0.0);
        runner.assert_true(success, "test_boundary_3",
                          "Initialize near south pole");

        LocalFrame ned = frame.gpsToLocalNED(-89.001, 0.0, 0.0);
        runner.assert_true(ned.north_m < -100.0, "test_boundary_3b",
                          "Conversion near south pole");
    }

    {
        CoordinateFrame frame;
        bool success = frame.initialize(0.0, 0.0, 10000.0);
        runner.assert_true(success, "test_boundary_4",
                          "Initialize at 10km altitude");

        LocalFrame ned = frame.gpsToLocalNED(0.001, 0.0, 10000.0);
        runner.assert_true(ned.north_m > 100.0, "test_boundary_4b",
                          "Conversion at high altitude");
    }
}

void test_error_handling(TestRunner& runner) {
    std::cout << "\n[ERROR HANDLING TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        runner.assert_throw(
            [&frame]() { frame.getOrigin(); }, "test_error_1",
            "getOrigin() before initialization throws");
    }

    {
        CoordinateFrame frame;
        runner.assert_throw(
            [&frame]() { frame.getOriginAltitude(); }, "test_error_2",
            "getOriginAltitude() before initialization throws");
    }
}

void test_consistency(TestRunner& runner) {
    std::cout << "\n[CONSISTENCY TESTS]" << std::endl;

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        GPS_Data point(MUNICH.lat_deg + 0.001, MUNICH.lon_deg + 0.001,
                      MUNICH.alt_m);

        LocalFrame ned1 =
            frame.gpsToLocalNED(point.latitude_deg, point.longitude_deg,
                              point.altitude_m);
        LocalFrame ned2 =
            frame.gpsToLocalNED(point.latitude_deg, point.longitude_deg,
                              point.altitude_m);

        runner.assert_true(ned1.north_m == ned2.north_m, "test_consistency_1",
                          "Multiple conversions give identical results");
    }

    {
        CoordinateFrame frame;
        frame.initialize(MUNICH.lat_deg, MUNICH.lon_deg, MUNICH.alt_m);

        LocalFrame ned_original(111.0, 93.0, -50.0);

        GPS_Data gps = frame.localNEDToGPS(ned_original);
        LocalFrame ned_result = frame.gpsToLocalNED(gps.latitude_deg, gps.longitude_deg,
                                                   gps.altitude_m);

        runner.assert_near(ned_result.north_m, ned_original.north_m, TOLERANCE_METER,
                          "test_consistency_2", "Inverse operations - north");
        runner.assert_near(ned_result.east_m, ned_original.east_m, TOLERANCE_METER,
                          "test_consistency_2b", "Inverse operations - east");
        runner.assert_near(ned_result.down_m, ned_original.down_m, TOLERANCE_METER,
                          "test_consistency_2c", "Inverse operations - down");
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "COORDINATEFRAME UNIT TEST SUITE" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    TestRunner runner;

    test_initialization(runner);
    test_gps_to_ned(runner);
    test_ned_to_gps(runner);
    test_round_trip(runner);
    test_reinitialization(runner);
    test_boundary_conditions(runner);
    test_error_handling(runner);
    test_consistency(runner);

    runner.summary();

    return runner.get_fail_count() == 0 ? 0 : 1;
}
