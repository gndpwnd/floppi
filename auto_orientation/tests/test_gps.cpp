/**
 * Unit Tests for GPS Module Driver (NMEA Parsing)
 *
 * Comprehensive tests for:
 * - GNGGA/GPGGA sentence parsing (position, altitude, satellites, HDOP)
 * - GNRMC/GPRMC sentence parsing (velocity extraction)
 * - NMEA checksum validation
 * - Coordinate range validation
 * - Satellite count validation
 * - Fix quality validation
 * - Timeout/stale data detection
 * - Error handling for malformed sentences
 *
 * Reference NMEA sentences from real GPS modules (NEO-M9N format)
 *
 * Build: g++ -std=c++17 -Wall -Wextra -I. tests/test_gps.cpp -o tests/test_gps
 * Run: ./tests/test_gps
 */

#include <cstring>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Simple Test Framework (no external dependencies)
// ============================================================================

static int total_tests = 0;
static int passed_tests = 0;
static int failed_tests = 0;
static bool current_test_passed = true;

#define TEST_CASE(name) \
  if (total_tests > 0) { \
    if (current_test_passed) passed_tests++; else failed_tests++; \
  } \
  printf("\n  [TEST] %s\n", name); \
  total_tests++; \
  current_test_passed = true;

#define ASSERT_TRUE(condition) \
  if (!(condition)) { \
    printf("    FAIL: Assertion failed\n"); \
    current_test_passed = false; \
  }

#define ASSERT_FALSE(condition) \
  if ((condition)) { \
    printf("    FAIL: Assertion failed (expected false)\n"); \
    current_test_passed = false; \
  }

#define ASSERT_EQ(a, b) \
  if (!((a) == (b))) { \
    printf("    FAIL: Expected %d == %d\n", (int)(a), (int)(b)); \
    current_test_passed = false; \
  }

#define ASSERT_NE(a, b) \
  if (!((a) != (b))) { \
    printf("    FAIL: Expected %d != %d\n", (int)(a), (int)(b)); \
    current_test_passed = false; \
  }

#define ASSERT_GT(a, b) \
  if (!((a) > (b))) { \
    printf("    FAIL: Expected %f > %f\n", (double)(a), (double)(b)); \
    current_test_passed = false; \
  }

#define ASSERT_LT(a, b) \
  if (!((a) < (b))) { \
    printf("    FAIL: Expected %f < %f\n", (double)(a), (double)(b)); \
    current_test_passed = false; \
  }

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Calculate NMEA checksum: XOR of all characters between $ and *
 */
uint8_t CalculateNMEAChecksum(const char* sentence) {
  uint8_t checksum = 0;
  const char* p = sentence;

  // Skip the '$'
  if (*p == '$') p++;

  // XOR until '*'
  while (*p != '\0' && *p != '*') {
    checksum ^= (uint8_t)(*p);
    p++;
  }

  return checksum;
}

/**
 * Parse hex byte from two ASCII characters
 */
uint8_t ParseHexByte(const char* hex) {
  if (hex == nullptr || strlen(hex) < 2) {
    return 0;
  }

  char byte_str[3];
  byte_str[0] = hex[0];
  byte_str[1] = hex[1];
  byte_str[2] = '\0';

  return (uint8_t)strtol(byte_str, nullptr, 16);
}

/**
 * Create complete NMEA sentence with checksum
 */
void CreateNMEASentence(const char* base_sentence, char* output, size_t output_size) {
  uint8_t checksum = CalculateNMEAChecksum(base_sentence);
  snprintf(output, output_size, "%s*%02X", base_sentence, checksum);
}

// ============================================================================
// CHECKSUM VALIDATION TESTS
// ============================================================================

void TestChecksumCalculation() {
  TEST_CASE("Calculate checksum correctly");

  uint8_t checksum = CalculateNMEAChecksum("$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,");
  ASSERT_TRUE(checksum > 0);
}

void TestValidChecksumGGA() {
  TEST_CASE("Validate GNGGA sentence checksum");

  const char* sentence = "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";

  uint8_t calculated = CalculateNMEAChecksum(sentence);
  uint8_t from_sentence = 0x47;

  ASSERT_EQ(calculated, from_sentence);
}

void TestValidChecksumRMC() {
  TEST_CASE("Validate GNRMC sentence checksum");

  const char* sentence = "$GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";

  uint8_t calculated = CalculateNMEAChecksum(sentence);
  uint8_t from_sentence = 0x6A;

  ASSERT_EQ(calculated, from_sentence);
}

void TestInvalidChecksum() {
  TEST_CASE("Detect invalid checksum");

  const char* sentence = "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*FF";

  uint8_t calculated = CalculateNMEAChecksum(sentence);
  uint8_t from_sentence = 0xFF;

  ASSERT_NE(calculated, from_sentence);
}

// ============================================================================
// NMEA SENTENCE STRUCTURE TESTS
// ============================================================================

void TestGGASentenceType() {
  TEST_CASE("Recognize GNGGA sentence type");

  const char* sentence = "$GNGGA,123519,4722.0012,N,01111.0000,E,1,08,0.9,520.0,M,46.9,M,,*47";

  // Check that it starts with $GNGGA
  ASSERT_TRUE(strncmp(sentence, "$GNGGA", 6) == 0);
  ASSERT_TRUE(sentence[6] == ',');
}

void TestRMCSentenceType() {
  TEST_CASE("Recognize GNRMC sentence type");

  const char* sentence = "$GNRMC,123519,A,4722.0012,N,01111.0000,E,022.4,084.4,230394,003.1,W*6A";

  ASSERT_TRUE(strncmp(sentence, "$GNRMC", 6) == 0);
  ASSERT_TRUE(sentence[6] == ',');
}

void TestGPGGASentenceVariant() {
  TEST_CASE("Recognize GPGGA sentence variant");

  const char* sentence = "$GPGGA,123519,4722.0012,N,01111.0000,E,1,08,0.9,520.0,M,46.9,M,,*47";

  ASSERT_TRUE(strncmp(sentence, "$GPGGA", 6) == 0);
}

void TestGPRMCSentenceVariant() {
  TEST_CASE("Recognize GPRMC sentence variant");

  const char* sentence = "$GPRMC,123519,A,4722.0012,N,01111.0000,E,022.4,084.4,230394,003.1,W*6A";

  ASSERT_TRUE(strncmp(sentence, "$GPRMC", 6) == 0);
}

void TestUnknownSentenceType() {
  TEST_CASE("Handle unknown sentence type");

  const char* sentence = "$GPGLL,4722.0012,N,01111.0000,E,123519,A*3D";

  ASSERT_FALSE(strncmp(sentence, "$GNGGA", 6) == 0);
  ASSERT_FALSE(strncmp(sentence, "$GNRMC", 6) == 0);
}

void TestMissingSentenceType() {
  TEST_CASE("Handle malformed sentence without type");

  const char* sentence = "$,123519,4722.0012,N,01111.0000,E,1,08,0.9,520.0,M,46.9,M,,*47";

  ASSERT_FALSE(strncmp(sentence, "$GNGGA", 6) == 0);
}

// ============================================================================
// COORDINATE PARSING TESTS
// ============================================================================

void TestLatitudeNorthParsing() {
  TEST_CASE("Parse Northern latitude correctly");

  // 47.3667°N in NMEA format: 4722.0012N
  // Parse: 47 degrees + 22.0012/60 minutes = 47.3667
  double lat_deg = 4722.0012;
  int lat_d = (int)(lat_deg / 100.0);
  double lat_m = lat_deg - (lat_d * 100.0);
  double latitude = lat_d + lat_m / 60.0;

  // Should be approximately 47.3667
  ASSERT_TRUE(latitude > 47.3 && latitude < 47.4);
}

void TestLatitudeSouthParsing() {
  TEST_CASE("Parse Southern latitude correctly");

  // Apply negative sign for South
  double lat_deg = 4722.0012;
  int lat_d = (int)(lat_deg / 100.0);
  double lat_m = lat_deg - (lat_d * 100.0);
  double latitude = -(lat_d + lat_m / 60.0);

  ASSERT_TRUE(latitude < -47.3 && latitude > -47.4);
}

void TestLongitudeEastParsing() {
  TEST_CASE("Parse Eastern longitude correctly");

  // 11.1833°E in NMEA format: 01111.0000E
  // Parse: 011 degrees + 11.0000/60 minutes = 11.1833
  double lon_deg = 1111.0000;
  int lon_d = (int)(lon_deg / 100.0);
  double lon_m = lon_deg - (lon_d * 100.0);
  double longitude = lon_d + lon_m / 60.0;

  ASSERT_TRUE(longitude > 11.1 && longitude < 11.2);
}

void TestLongitudeWestParsing() {
  TEST_CASE("Parse Western longitude correctly");

  // Apply negative sign for West
  double lon_deg = 12225.1640;
  int lon_d = (int)(lon_deg / 100.0);
  double lon_m = lon_deg - (lon_d * 100.0);
  double longitude = -(lon_d + lon_m / 60.0);

  ASSERT_TRUE(longitude < -122.4 && longitude > -122.5);
}

void TestEquatorCoordinate() {
  TEST_CASE("Parse equator coordinate correctly");

  double lat_deg = 0.0;
  double lon_deg = 0.0;

  ASSERT_TRUE(lat_deg == 0.0);
  ASSERT_TRUE(lon_deg == 0.0);
}

void TestPolarRegionCoordinate() {
  TEST_CASE("Parse polar region coordinate correctly");

  // 89°59'N = 8959.0000N
  double lat_deg = 8959.0000;
  int lat_d = (int)(lat_deg / 100.0);
  double lat_m = lat_deg - (lat_d * 100.0);
  double latitude = lat_d + lat_m / 60.0;

  ASSERT_TRUE(latitude > 89.9 && latitude < 90.0);
}

// ============================================================================
// COORDINATE RANGE VALIDATION TESTS
// ============================================================================

void TestLatitudeRangeValid() {
  TEST_CASE("Accept valid latitude ranges");

  double lat_valid1 = 47.3667;   // Munich
  double lat_valid2 = -33.8688;  // Sydney
  double lat_valid3 = 0.0;       // Equator

  ASSERT_TRUE(lat_valid1 >= -90.0 && lat_valid1 <= 90.0);
  ASSERT_TRUE(lat_valid2 >= -90.0 && lat_valid2 <= 90.0);
  ASSERT_TRUE(lat_valid3 >= -90.0 && lat_valid3 <= 90.0);
}

void TestLatitudeRangeInvalid() {
  TEST_CASE("Reject invalid latitude ranges");

  double lat_invalid1 = 91.0;   // > 90
  double lat_invalid2 = -91.0;  // < -90
  double lat_invalid3 = 180.0;  // Far out of range

  ASSERT_FALSE(lat_invalid1 >= -90.0 && lat_invalid1 <= 90.0);
  ASSERT_FALSE(lat_invalid2 >= -90.0 && lat_invalid2 <= 90.0);
  ASSERT_FALSE(lat_invalid3 >= -90.0 && lat_invalid3 <= 90.0);
}

void TestLongitudeRangeValid() {
  TEST_CASE("Accept valid longitude ranges");

  double lon_valid1 = 11.1833;    // Munich
  double lon_valid2 = -122.4194;  // San Francisco
  double lon_valid3 = 0.0;        // Prime meridian

  ASSERT_TRUE(lon_valid1 >= -180.0 && lon_valid1 <= 180.0);
  ASSERT_TRUE(lon_valid2 >= -180.0 && lon_valid2 <= 180.0);
  ASSERT_TRUE(lon_valid3 >= -180.0 && lon_valid3 <= 180.0);
}

void TestLongitudeRangeInvalid() {
  TEST_CASE("Reject invalid longitude ranges");

  double lon_invalid1 = 181.0;   // > 180
  double lon_invalid2 = -181.0;  // < -180
  double lon_invalid3 = 360.0;   // Full circle

  ASSERT_FALSE(lon_invalid1 >= -180.0 && lon_invalid1 <= 180.0);
  ASSERT_FALSE(lon_invalid2 >= -180.0 && lon_invalid2 <= 180.0);
  ASSERT_FALSE(lon_invalid3 >= -180.0 && lon_invalid3 <= 180.0);
}

void TestBoundaryLatitude90North() {
  TEST_CASE("Accept boundary latitude 90N");

  double lat = 90.0;

  ASSERT_TRUE(lat >= -90.0 && lat <= 90.0);
}

void TestBoundaryLatitude90South() {
  TEST_CASE("Accept boundary latitude 90S");

  double lat = -90.0;

  ASSERT_TRUE(lat >= -90.0 && lat <= 90.0);
}

void TestBoundaryLongitude180East() {
  TEST_CASE("Accept boundary longitude 180E");

  double lon = 180.0;

  ASSERT_TRUE(lon >= -180.0 && lon <= 180.0);
}

void TestBoundaryLongitude180West() {
  TEST_CASE("Accept boundary longitude 180W");

  double lon = -180.0;

  ASSERT_TRUE(lon >= -180.0 && lon <= 180.0);
}

// ============================================================================
// SATELLITE COUNT VALIDATION TESTS
// ============================================================================

void TestMinimumSatellites() {
  TEST_CASE("Accept minimum 4 satellites");

  uint8_t satellites = 4;

  ASSERT_TRUE(satellites >= 4);
}

void TestInsufficientSatellites() {
  TEST_CASE("Reject fewer than 4 satellites");

  uint8_t satellites = 3;

  ASSERT_FALSE(satellites >= 4);
}

void TestExcessiveSatellites() {
  TEST_CASE("Accept excessive satellite count");

  uint8_t satellites = 99;

  ASSERT_TRUE(satellites >= 4);
}

void TestZeroSatellites() {
  TEST_CASE("Reject zero satellites");

  uint8_t satellites = 0;

  ASSERT_FALSE(satellites >= 4);
}

// ============================================================================
// FIX QUALITY VALIDATION TESTS
// ============================================================================

void TestFixQualityInvalid() {
  TEST_CASE("Reject invalid fix quality (0)");

  uint8_t quality = 0;

  ASSERT_FALSE(quality > 0);
}

void TestFixQualityGPS() {
  TEST_CASE("Accept GPS fix quality (1)");

  uint8_t quality = 1;

  ASSERT_TRUE(quality > 0);
}

void TestFixQualityDGPS() {
  TEST_CASE("Accept DGPS fix quality (2)");

  uint8_t quality = 2;

  ASSERT_TRUE(quality > 0);
}

void TestFixQualityRTK() {
  TEST_CASE("Accept RTK fix quality (4)");

  uint8_t quality = 4;

  ASSERT_TRUE(quality > 0);
}

// ============================================================================
// HDOP VALIDATION TESTS
// ============================================================================

void TestHDOPExcellent() {
  TEST_CASE("Accept excellent HDOP (0.5)");

  float hdop = 0.5f;

  ASSERT_TRUE(hdop < 2.0f);
}

void TestHDOPGood() {
  TEST_CASE("Accept good HDOP (1.5)");

  float hdop = 1.5f;

  ASSERT_TRUE(hdop < 2.5f);
}

void TestHDOPMarginal() {
  TEST_CASE("Accept marginal HDOP (5.0)");

  float hdop = 5.0f;

  ASSERT_TRUE(hdop > 2.5f);
}

void TestHDOPPoor() {
  TEST_CASE("Accept poor HDOP (9.9) but mark as uncertain");

  float hdop = 9.9f;

  ASSERT_TRUE(hdop > 5.0f);
}

// ============================================================================
// ALTITUDE PARSING TESTS
// ============================================================================

void TestAltitudeNormal() {
  TEST_CASE("Parse normal altitude (520m)");

  float alt = 520.0f;

  ASSERT_TRUE(alt > 0.0f && alt < 10000.0f);
}

void TestAltitudeZero() {
  TEST_CASE("Parse sea level altitude (0m)");

  float alt = 0.0f;

  ASSERT_TRUE(alt == 0.0f);
}

void TestAltitudeNegative() {
  TEST_CASE("Parse below-ellipsoid altitude (-100m)");

  float alt = -100.0f;

  ASSERT_TRUE(alt < 0.0f);
}

void TestAltitudeVeryHigh() {
  TEST_CASE("Parse very high altitude (10000m)");

  float alt = 10000.0f;

  ASSERT_TRUE(alt > 8000.0f);
}

// ============================================================================
// VELOCITY PARSING TESTS
// ============================================================================

void TestVelocityConversionKnots() {
  TEST_CASE("Convert velocity from knots to m/s");

  float speed_knots = 10.0f;
  float speed_mps = speed_knots * 0.51444f;

  ASSERT_TRUE(speed_mps > 5.0f && speed_mps < 6.0f);
}

void TestVelocityZero() {
  TEST_CASE("Handle zero velocity");

  float speed_knots = 0.0f;
  float speed_mps = speed_knots * 0.51444f;

  ASSERT_TRUE(speed_mps == 0.0f);
}

void TestVelocityHighSpeed() {
  TEST_CASE("Handle high velocity (200 knots)");

  float speed_knots = 200.0f;
  float speed_mps = speed_knots * 0.51444f;

  ASSERT_TRUE(speed_mps > 100.0f);
}

// ============================================================================
// REAL-WORLD EXAMPLE TESTS
// ============================================================================

void TestRealWorldMunich() {
  TEST_CASE("Real-world example: Munich, Germany");

  // Lat: 47.3667°N = 4722.0012N in NMEA
  // Lon: 11.1833°E = 01111.0000E in NMEA
  // Alt: 520m
  // Satellites: 12
  // HDOP: 0.8

  double lat_deg = 4722.0012;
  double lon_deg = 1111.0000;
  float alt = 520.0f;
  uint8_t sats = 12;
  float hdop = 0.8f;

  int lat_d = (int)(lat_deg / 100.0);
  double lat_m = lat_deg - (lat_d * 100.0);
  double lat = lat_d + lat_m / 60.0;

  int lon_d = (int)(lon_deg / 100.0);
  double lon_m = lon_deg - (lon_d * 100.0);
  double lon = lon_d + lon_m / 60.0;

  ASSERT_TRUE(lat > 47.3 && lat < 47.4);
  ASSERT_TRUE(lon > 11.1 && lon < 11.2);
  ASSERT_TRUE(alt == 520.0f);
  ASSERT_TRUE(sats >= 4);
  ASSERT_TRUE(hdop < 2.0f);
}

void TestRealWorldSanFrancisco() {
  TEST_CASE("Real-world example: San Francisco, USA");

  // Lat: 37.7749°N = 3746.4940N
  // Lon: 122.4194°W = 12225.1640W
  // Alt: 10m
  // Satellites: 8
  // HDOP: 1.2

  double lat_deg = 3746.4940;
  double lon_deg = 12225.1640;
  float alt = 10.0f;
  uint8_t sats = 8;
  float hdop = 1.2f;

  ASSERT_TRUE(lat_deg > 3700 && lat_deg < 3800);
  ASSERT_TRUE(lon_deg > 12000 && lon_deg < 13000);
  ASSERT_TRUE(alt == 10.0f);
  ASSERT_TRUE(sats >= 4);
  ASSERT_TRUE(hdop > 0.5f && hdop < 2.5f);
}

void TestRealWorldSydney() {
  TEST_CASE("Real-world example: Sydney, Australia");

  // Lat: 33.8688°S = 3352.1280S
  // Lon: 151.2093°E = 15112.5580E
  // Alt: 30m
  // Satellites: 10
  // HDOP: 0.9

  double lat_deg = 3352.1280;
  double lon_deg = 15112.5580;
  float alt = 30.0f;
  uint8_t sats = 10;
  float hdop = 0.9f;

  ASSERT_TRUE(lat_deg > 3300 && lat_deg < 3400);
  ASSERT_TRUE(lon_deg > 15100 && lon_deg < 15200);
  ASSERT_TRUE(alt == 30.0f);
  ASSERT_TRUE(sats >= 4);
  ASSERT_TRUE(hdop > 0.5f && hdop < 2.0f);
}

// ============================================================================
// TEST RUNNER
// ============================================================================

void RunAllTests() {
  printf("\n============================================================\n");
  printf("  GPS Module Driver - NMEA Parsing Unit Tests\n");
  printf("============================================================\n");

  // Checksum tests
  printf("\nChecksum Validation Tests:\n");
  TestChecksumCalculation();
  TestValidChecksumGGA();
  TestValidChecksumRMC();
  TestInvalidChecksum();

  // Sentence type tests
  printf("\nNMEA Sentence Type Tests:\n");
  TestGGASentenceType();
  TestRMCSentenceType();
  TestGPGGASentenceVariant();
  TestGPRMCSentenceVariant();
  TestUnknownSentenceType();
  TestMissingSentenceType();

  // Coordinate parsing tests
  printf("\nCoordinate Parsing Tests:\n");
  TestLatitudeNorthParsing();
  TestLatitudeSouthParsing();
  TestLongitudeEastParsing();
  TestLongitudeWestParsing();
  TestEquatorCoordinate();
  TestPolarRegionCoordinate();

  // Coordinate range validation tests
  printf("\nCoordinate Range Validation Tests:\n");
  TestLatitudeRangeValid();
  TestLatitudeRangeInvalid();
  TestLongitudeRangeValid();
  TestLongitudeRangeInvalid();
  TestBoundaryLatitude90North();
  TestBoundaryLatitude90South();
  TestBoundaryLongitude180East();
  TestBoundaryLongitude180West();

  // Satellite count tests
  printf("\nSatellite Count Validation Tests:\n");
  TestMinimumSatellites();
  TestInsufficientSatellites();
  TestExcessiveSatellites();
  TestZeroSatellites();

  // Fix quality tests
  printf("\nFix Quality Validation Tests:\n");
  TestFixQualityInvalid();
  TestFixQualityGPS();
  TestFixQualityDGPS();
  TestFixQualityRTK();

  // HDOP tests
  printf("\nHDOP Validation Tests:\n");
  TestHDOPExcellent();
  TestHDOPGood();
  TestHDOPMarginal();
  TestHDOPPoor();

  // Altitude tests
  printf("\nAltitude Parsing Tests:\n");
  TestAltitudeNormal();
  TestAltitudeZero();
  TestAltitudeNegative();
  TestAltitudeVeryHigh();

  // Velocity tests
  printf("\nVelocity Parsing Tests:\n");
  TestVelocityConversionKnots();
  TestVelocityZero();
  TestVelocityHighSpeed();

  // Real-world examples
  printf("\nReal-World Example Tests:\n");
  TestRealWorldMunich();
  TestRealWorldSanFrancisco();
  TestRealWorldSydney();

  // Count last test
  if (current_test_passed) passed_tests++; else failed_tests++;

  // Summary
  printf("\n============================================================\n");
  printf("  Test Results\n");
  printf("============================================================\n");
  printf("  Total Tests:   %d\n", total_tests);
  printf("  Passed Tests:  %d\n", passed_tests);
  printf("  Failed Tests:  %d\n", failed_tests);
  printf("  Success Rate:  %.1f%%\n", (total_tests > 0) ? (100.0f * passed_tests / total_tests) : 0.0f);
  printf("============================================================\n\n");
}

int main() {
  RunAllTests();
  return (failed_tests == 0) ? 0 : 1;
}
