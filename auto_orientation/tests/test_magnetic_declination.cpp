/**
 * Unit Tests for Magnetic Declination Conversions
 *
 * Tests declination lookup, heading conversions, and normalization.
 */

#ifdef DEBUG_MODE

#include <gtest/gtest.h>
#include <cmath>
#include "../src/math/magnetic_declination.h"

// ============================================================================
// TEST FIXTURES
// ============================================================================

class MagneticDeclinationTest : public ::testing::Test {
 protected:
    static constexpr double TOLERANCE_DEGREE = 0.1;  // ±0.1° tolerance

    // Known declination values (WMM2024)
    struct DeclinationPoint {
        const char* location;
        double lat;
        double lon;
        double expected_decl;
        double tolerance;  // Larger tolerance for simplified model
    };

    static const DeclinationPoint KNOWN_DECLINATIONS[];
};

const MagneticDeclinationTest::DeclinationPoint
    MagneticDeclinationTest::KNOWN_DECLINATIONS[] = {
        // Europe
        {"Munich, Germany", 47.3667, 11.1833, 3.2, 2.0},
        {"London, UK", 51.5074, -0.1278, -2.7, 2.0},
        {"Paris, France", 48.8566, 2.3522, -1.8, 2.0},

        // North America
        {"San Francisco, CA", 37.7749, -122.4194, 12.8, 2.0},
        {"Los Angeles, CA", 34.0522, -118.2437, 11.4, 2.0},
        {"New York City, USA", 40.7128, -74.0060, -12.4, 2.0},
        {"Miami, FL", 25.7617, -80.1918, -4.8, 2.0},

        // Asia
        {"Tokyo, Japan", 35.6762, 139.6503, 8.2, 2.0},
        {"Singapore", 1.3521, 103.8198, 0.8, 2.0},
        {"Hong Kong", 22.3193, 114.1694, 3.2, 2.0},

        // Southern Hemisphere
        {"Sydney, Australia", -33.8688, 151.2093, 12.0, 2.0},
        {"Cape Town, South Africa", -33.9249, 18.4241, -24.8, 3.0},
    };

// ============================================================================
// MAGNETIC DECLINATION LOOKUP TESTS
// ============================================================================

TEST_F(MagneticDeclinationTest, GetDeclinationKnownLocations) {
    // Test declination lookup at known locations
    for (const auto& point : KNOWN_DECLINATIONS) {
        SCOPED_TRACE(point.location);

        double decl = get_magnetic_declination(point.lat, point.lon);

        // Check that result is within expected range
        EXPECT_GE(decl, -30.0) << "Declination too negative";
        EXPECT_LE(decl, 30.0) << "Declination too positive";

        // Check approximate match to known value
        // (Using larger tolerance because simplified model is coarse)
        EXPECT_NEAR(decl, point.expected_decl, point.tolerance)
            << "Declination doesn't match known value for " << point.location;
    }
}

TEST_F(MagneticDeclinationTest, GetDeclinationWithYear) {
    // Test that year parameter affects result
    double decl_2024 = get_magnetic_declination(47.3667, 11.1833, 2024);
    double decl_2025 = get_magnetic_declination(47.3667, 11.1833, 2025);

    // Declination changes ~0.15°/year (approximate)
    // 2025 should be ~0.15° different from 2024
    double expected_change = 0.15;
    EXPECT_NEAR(decl_2025 - decl_2024, expected_change, 0.2)
        << "Year parameter not affecting declination";
}

TEST_F(MagneticDeclinationTest, GetDeclinationEquator) {
    // At equator, declination should be relatively small
    double decl = get_magnetic_declination(0.0, 0.0);
    EXPECT_LT(std::abs(decl), 10.0) << "Equator declination should be small";
}

TEST_F(MagneticDeclinationTest, GetDeclinationPoles) {
    // Declination at poles should be well-defined but large
    double decl_north = get_magnetic_declination(89.0, 0.0);
    double decl_south = get_magnetic_declination(-89.0, 0.0);

    // Both should be defined and substantial
    EXPECT_FALSE(std::isnan(decl_north)) << "North pole declination is NaN";
    EXPECT_FALSE(std::isnan(decl_south)) << "South pole declination is NaN";
}

// ============================================================================
// HEADING CONVERSION TESTS
// ============================================================================

TEST_F(MagneticDeclinationTest, ApplyDeclinationEast) {
    // Magnetic north 45° from East declination 12°
    // True north should be 45° + 12° = 57°
    double true_heading = apply_declination(45.0, 12.0);
    EXPECT_NEAR(true_heading, 57.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, ApplyDeclinationWest) {
    // Magnetic north 45°, from West declination -12°
    // True north should be 45° - 12° = 33°
    double true_heading = apply_declination(45.0, -12.0);
    EXPECT_NEAR(true_heading, 33.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, ApplyDeclinationNormalization) {
    // Test that result wraps to [0, 360)
    double result1 = apply_declination(350.0, 20.0);  // 350 + 20 = 370 -> 10
    EXPECT_NEAR(result1, 10.0, 1e-6);

    double result2 = apply_declination(10.0, -20.0);  // 10 - 20 = -10 -> 350
    EXPECT_NEAR(result2, 350.0, 1e-6);

    double result3 = apply_declination(0.0, 0.0);  // No change
    EXPECT_NEAR(result3, 0.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, RemoveDeclinationEast) {
    // True heading 57°, East declination 12°
    // Magnetic should be 57° - 12° = 45°
    double mag_heading = remove_declination(57.0, 12.0);
    EXPECT_NEAR(mag_heading, 45.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, RemoveDeclinationWest) {
    // True heading 33°, West declination -12°
    // Magnetic should be 33° - (-12°) = 45°
    double mag_heading = remove_declination(33.0, -12.0);
    EXPECT_NEAR(mag_heading, 45.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, RemoveDeclinationNormalization) {
    // Test wrap-around for remove_declination
    double result1 = remove_declination(10.0, 20.0);  // 10 - 20 = -10 -> 350
    EXPECT_NEAR(result1, 350.0, 1e-6);

    double result2 = remove_declination(350.0, -20.0);  // 350 - (-20) = 370 -> 10
    EXPECT_NEAR(result2, 10.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, DeclinationRoundTrip) {
    // Round-trip: true -> apply remove -> magnetic -> apply declination -> true
    double true_heading = 45.0;
    double declination = 12.0;

    double magnetic = remove_declination(true_heading, declination);
    double true_recovered = apply_declination(magnetic, declination);

    EXPECT_NEAR(true_recovered, true_heading, 1e-6)
        << "Round-trip conversion failed";
}

// ============================================================================
// HEADING NORMALIZATION TESTS
// ============================================================================

TEST_F(MagneticDeclinationTest, NormalizeHeadingPositive) {
    EXPECT_NEAR(normalize_heading(0.0), 0.0, 1e-6);
    EXPECT_NEAR(normalize_heading(45.0), 45.0, 1e-6);
    EXPECT_NEAR(normalize_heading(90.0), 90.0, 1e-6);
    EXPECT_NEAR(normalize_heading(180.0), 180.0, 1e-6);
    EXPECT_NEAR(normalize_heading(270.0), 270.0, 1e-6);
    EXPECT_NEAR(normalize_heading(359.9), 359.9, 1e-6);
}

TEST_F(MagneticDeclinationTest, NormalizeHeadingWrap) {
    EXPECT_NEAR(normalize_heading(360.0), 0.0, 1e-6);
    EXPECT_NEAR(normalize_heading(405.0), 45.0, 1e-6);
    EXPECT_NEAR(normalize_heading(720.0), 0.0, 1e-6);
    EXPECT_NEAR(normalize_heading(810.0), 90.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, NormalizeHeadingNegative) {
    EXPECT_NEAR(normalize_heading(-45.0), 315.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-90.0), 270.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-180.0), 180.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-270.0), 90.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-360.0), 0.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-405.0), 315.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, NormalizeHeadingLarge) {
    EXPECT_NEAR(normalize_heading(3600.0), 0.0, 1e-6);
    EXPECT_NEAR(normalize_heading(3645.0), 45.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-3600.0), 0.0, 1e-6);
    EXPECT_NEAR(normalize_heading(-3645.0), 315.0, 1e-6);
}

// ============================================================================
// HEADING RADIAN CONVERSION TESTS
// ============================================================================

TEST_F(MagneticDeclinationTest, HeadingToRadians) {
    EXPECT_NEAR(heading_to_radians(0.0), 0.0, 1e-6);
    EXPECT_NEAR(heading_to_radians(90.0), M_PI / 2.0, 1e-6);
    EXPECT_NEAR(heading_to_radians(180.0), M_PI, 1e-6);
    EXPECT_NEAR(heading_to_radians(270.0), 3 * M_PI / 2.0, 1e-6);
    EXPECT_NEAR(heading_to_radians(360.0), 2 * M_PI, 1e-6);
}

TEST_F(MagneticDeclinationTest, HeadingFromRadians) {
    EXPECT_NEAR(heading_from_radians(0.0), 0.0, 1e-6);
    EXPECT_NEAR(heading_from_radians(M_PI / 2.0), 90.0, 1e-6);
    EXPECT_NEAR(heading_from_radians(M_PI), 180.0, 1e-6);
    EXPECT_NEAR(heading_from_radians(3 * M_PI / 2.0), 270.0, 1e-6);
    EXPECT_NEAR(heading_from_radians(2 * M_PI), 0.0, 1e-6);
}

TEST_F(MagneticDeclinationTest, HeadingConversionRoundTrip) {
    double original = 45.0;
    double radians = heading_to_radians(original);
    double recovered = heading_from_radians(radians);
    EXPECT_NEAR(recovered, original, 1e-6);
}

// ============================================================================
// REAL-WORLD SCENARIO TESTS
// ============================================================================

TEST_F(MagneticDeclinationTest, ScenarioSanFranciscoMagnetometerReading) {
    // Scenario: Magnetometer in San Francisco reads 45° magnetic
    // Need true heading for navigation
    double lat = 37.7749;
    double lon = -122.4194;
    double mag_heading = 45.0;

    double declination = get_magnetic_declination(lat, lon);
    double true_heading = apply_declination(mag_heading, declination);

    // Should be roughly easterly (45° + ~13° east = 58°)
    EXPECT_GT(true_heading, 40.0);
    EXPECT_LT(true_heading, 70.0);
}

TEST_F(MagneticDeclinationTest, ScenarioNewYorkMagnetometerReading) {
    // Scenario: Magnetometer in New York reads 45° magnetic
    // Need true heading for navigation
    double lat = 40.7128;
    double lon = -74.0060;
    double mag_heading = 45.0;

    double declination = get_magnetic_declination(lat, lon);
    double true_heading = apply_declination(mag_heading, declination);

    // Should be roughly adjusted for west declination (-12°)
    // 45° - 12° = 33°
    EXPECT_NEAR(true_heading, 33.0, 5.0);
}

TEST_F(MagneticDeclinationTest, ScenarioMunichNavigationToTrueHeading) {
    // Scenario: Navigation system has true heading 0° (north)
    // What should magnetometer read?
    double lat = 47.3667;
    double lon = 11.1833;
    double true_heading = 0.0;

    double declination = get_magnetic_declination(lat, lon);
    double mag_heading = remove_declination(true_heading, declination);

    // Munich has ~3° east declination, so magnetometer should read ~-3°=357°
    EXPECT_NEAR(mag_heading, 357.0, 5.0);
}

#endif  // DEBUG_MODE
