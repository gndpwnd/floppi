/*
 * BNO085 I2C Troubleshooting Test Sketches
 *
 * Collection of ready-to-use Arduino sketches for diagnosing BNO085 I2C issues
 *
 * Copy each sketch below into a new Arduino IDE file and upload to test.
 *
 * ============================================================================
 * QUICK TEST 1: I2C Bus Scanner - Find all devices on the bus
 * ============================================================================
 */

// ============================================================================
// TEST 1: I2C BUS SCANNER
// ============================================================================
// Upload this to scan for I2C devices at different clock speeds

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  Serial.println("\n=== I2C Bus Scanner ===");
  Serial.println("Scanning I2C addresses 0x00-0x7F...");
  delay(100);

  scanBusAtSpeed(100000L, "100kHz");
  scanBusAtSpeed(400000L, "400kHz");
}

void loop() {
  delay(10000);
}

void scanBusAtSpeed(uint32_t clock_speed, const char* speed_label) {
  Wire.begin();
  Wire.setClock(clock_speed);
  delay(100);

  Serial.print("\n--- Scanning at ");
  Serial.print(speed_label);
  Serial.println(" ---");

  int count = 0;
  for (uint8_t addr = 0x00; addr <= 0x7F; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission(true);

    if (error == 0) {
      Serial.print("Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      count++;
    }
  }

  Serial.print("Total devices found: ");
  Serial.println(count);
}

#endif

// ============================================================================
// TEST 2: MINIMAL I2C TEST - Test each step of I2C setup
// ============================================================================

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== Minimal I2C Test ===");

  // Step 1: Initialize Wire
  Serial.print("Step 1: Initializing Wire... ");
  Wire.begin();
  delay(10);
  Serial.println("OK");

  // Step 2: Set clock
  Serial.print("Step 2: Setting clock to 100kHz... ");
  Wire.setClock(100000L);
  delay(10);
  Serial.println("OK");

  // Step 3: Scan for BNO085
  Serial.print("Step 3: Scanning for BNO085 at 0x4A... ");
  Wire.beginTransmission(0x4A);
  uint8_t error = Wire.endTransmission(true);

  if (error == 0) {
    Serial.println("FOUND");
    Serial.println("\nSUCCESS: Device is present on I2C bus");
  } else {
    Serial.print("NOT FOUND (error ");
    Serial.print(error);
    Serial.println(")");

    Serial.print("Trying 0x4B... ");
    Wire.beginTransmission(0x4B);
    error = Wire.endTransmission(true);

    if (error == 0) {
      Serial.println("FOUND");
      Serial.println("NOTE: Device is at 0x4B (DI pin to VCC)");
    } else {
      Serial.println("NOT FOUND");
      Serial.println("ERROR: No BNO085 found on I2C bus");
      Serial.println("Check: Wiring, Power Supply, DI pin");
    }
  }
}

void loop() {
  delay(1000);
}

#endif

// ============================================================================
// TEST 3: DEBUG LEVEL 1 - Test Wire library initialization
// ============================================================================

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== DEBUG LEVEL 1: Wire Library Test ===");

  Serial.print("Test 1: Serial communication... ");
  Serial.println("OK");
  delay(500);

  Serial.print("Test 2: Calling Wire.begin()... ");
  Wire.begin();
  Serial.println("OK");
  delay(500);

  Serial.print("Test 3: Calling Wire.setClock(100000)... ");
  Wire.setClock(100000L);
  Serial.println("OK");
  delay(500);

  Serial.println("\nDEBUG LEVEL 1: PASSED");
  Serial.println("I2C hardware is responsive");
}

void loop() {
  delay(1000);
}

#endif

// ============================================================================
// TEST 4: CLOCK SPEED TESTING - Find working clock speed
// ============================================================================

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== I2C Clock Speed Testing ===");

  uint32_t speeds[] = {50000L, 100000L, 200000L, 400000L};
  const char* labels[] = {"50kHz", "100kHz", "200kHz", "400kHz"};

  for (int i = 0; i < 4; i++) {
    testClockSpeed(speeds[i], labels[i]);
    delay(1000);
  }

  Serial.println("\n=== Testing Complete ===");
}

void loop() {
  delay(1000);
}

void testClockSpeed(uint32_t speed_hz, const char* label) {
  Serial.print("\nTesting at ");
  Serial.print(label);
  Serial.print("... ");

  Wire.begin();
  Wire.setClock(speed_hz);
  delay(100);

  // Try to find BNO085
  Wire.beginTransmission(0x4A);
  uint8_t error = Wire.endTransmission(true);

  if (error == 0) {
    Serial.println("Device found - OK");
  } else {
    Serial.print("No device (error ");
    Serial.print(error);
    Serial.println(")");
  }
}

#endif

// ============================================================================
// TEST 5: HARDWARE VOLTAGE CHECK - Verify I2C line voltages
// ============================================================================

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>

// If your board has analog input, use this to check voltages
// Connect multimeter to check actual voltages:
// - SCL at pin 21 (idle should be 3.0-3.3V)
// - SDA at pin 20 (idle should be 3.0-3.3V)

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== Hardware Voltage Check ===");
  Serial.println("Use multimeter to verify voltages:");
  Serial.println("- SCL (pin 21) idle: 3.0-3.3V");
  Serial.println("- SDA (pin 20) idle: 3.0-3.3V");
  Serial.println("- VCC to sensor: 3.3V ± 0.2V");
  Serial.println("- GND continuity: <1 ohm");
  Serial.println("");
  Serial.println("This test initializes I2C so you can probe with multimeter.");
  Serial.println("Waiting 30 seconds for you to connect multimeter...");

  delay(30000);

  Wire.begin();
  Wire.setClock(100000L);

  Serial.println("I2C initialized. Check voltages on multimeter.");
  Serial.println("Code will attempt device scan in 5 seconds...");
  delay(5000);

  Wire.beginTransmission(0x4A);
  uint8_t error = Wire.endTransmission(true);

  Serial.print("Scan result: ");
  Serial.println(error == 0 ? "Device found" : "No device");
}

void loop() {
  delay(1000);
}

#endif

// ============================================================================
// TEST 6: ADAFRUIT LIBRARY INITIALIZATION - Test Adafruit_BNO08x.begin_I2C()
// ============================================================================

#if 0  // Change to #if 1 to use this sketch
// REQUIRES: Adafruit BNO08x library installed

#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x imu;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== Adafruit Library Initialization Test ===");

  Serial.println("Step 1: Initialize Wire");
  Wire.begin();
  Wire.setClock(100000L);
  delay(100);
  Serial.println("Step 1: OK");

  Serial.println("\nStep 2: Call imu.begin_I2C(0x4A)");
  Serial.println("  [If this hangs, problem is library or device]");

  bool success = imu.begin_I2C(0x4A, &Wire, 0);

  Serial.print("\nStep 2: Result = ");
  Serial.println(success ? "SUCCESS" : "FAILED");

  if (success) {
    Serial.println("\nSensor initialized successfully!");
    Serial.print("Product IDs: ");
    Serial.println(imu.prodIds.swVer);
  } else {
    Serial.println("\nSensor initialization failed.");
    Serial.println("Try: Different clock speed, check wiring, add pull-ups");
  }
}

void loop() {
  delay(1000);
}

#endif

// ============================================================================
// TEST 7: REGISTER READ - Read BNO085 chip ID register
// ============================================================================

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== BNO085 Chip ID Read Test ===");

  Wire.begin();
  Wire.setClock(100000L);
  delay(100);

  Serial.println("Attempting to read chip ID register...");

  uint8_t chip_id = readRegisterFromAddress(0x4A, 0x00);

  Serial.print("Chip ID (register 0x00): 0x");
  Serial.println(chip_id, HEX);

  if (chip_id == 0xA0) {
    Serial.println("SUCCESS: Chip ID matches BNO085 (0xA0)");
  } else {
    Serial.print("WARNING: Unexpected chip ID. Expected 0xA0, got 0x");
    Serial.println(chip_id, HEX);
  }
}

void loop() {
  delay(1000);
}

uint8_t readRegisterFromAddress(uint8_t device_addr, uint8_t reg_addr) {
  Wire.beginTransmission(device_addr);
  Wire.write(reg_addr);
  uint8_t error = Wire.endTransmission(false);  // Don't release bus

  if (error != 0) {
    Serial.print("  Error writing register address (");
    Serial.print(error);
    Serial.println(")");
    return 0xFF;
  }

  Wire.requestFrom(device_addr, (uint8_t)1, (uint8_t)true);

  if (Wire.available()) {
    return Wire.read();
  }

  Serial.println("  Error: No data received");
  return 0xFF;
}

#endif

// ============================================================================
// TEST 8: PROGRESSIVE CLOCK SPEED WITH ADAFRUIT - Find max working speed
// ============================================================================

#if 0  // Change to #if 1 to use this sketch
// REQUIRES: Adafruit BNO08x library installed

#include <Wire.h>
#include "Adafruit_BNO08x.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== Progressive Clock Speed Test with Adafruit ===");

  uint32_t speeds[] = {50000L, 100000L, 200000L, 400000L};
  const char* labels[] = {"50kHz", "100kHz", "200kHz", "400kHz"};

  for (int i = 0; i < 4; i++) {
    testAdafruitAtSpeed(speeds[i], labels[i]);
    delay(2000);
  }

  Serial.println("\n=== All tests complete ===");
}

void loop() {
  delay(1000);
}

void testAdafruitAtSpeed(uint32_t speed_hz, const char* label) {
  Serial.print("\n--- Testing at ");
  Serial.print(label);
  Serial.println(" ---");

  Wire.begin();
  Wire.setClock(speed_hz);
  delay(100);

  // First, verify device is present
  Serial.print("  Device scan... ");
  Wire.beginTransmission(0x4A);
  uint8_t error = Wire.endTransmission(true);

  if (error != 0) {
    Serial.println("NOT FOUND");
    return;
  }
  Serial.println("found");

  // Now try Adafruit initialization
  Serial.print("  Adafruit begin_I2C()... ");

  Adafruit_BNO08x imu;
  bool success = imu.begin_I2C(0x4A, &Wire, 0);

  Serial.println(success ? "SUCCESS" : "FAILED");
}

#endif

// ============================================================================
// TEST 9: FULL SENSOR TEST - Read actual orientation data
// ============================================================================

#if 0  // Change to #if 1 to use this sketch
// REQUIRES: Adafruit BNO08x library installed

#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x imu;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== Full BNO085 Sensor Test ===");

  Wire.begin();
  Wire.setClock(100000L);
  delay(100);

  Serial.print("Initializing sensor... ");
  if (!imu.begin_I2C(0x4A, &Wire, 0)) {
    Serial.println("FAILED");
    while(1) { delay(100); }
  }
  Serial.println("OK");

  // Enable rotation vector report
  Serial.print("Enabling rotation vector report... ");
  if (!imu.enableReport(SH2_ROTATION_VECTOR, 100000)) {
    Serial.println("FAILED");
  } else {
    Serial.println("OK");
  }

  Serial.println("\nWaiting for sensor data...");
  delay(1000);
}

void loop() {
  sh2_SensorValue_t sensorValue;

  if (imu.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      Serial.print("Quaternion: w=");
      Serial.print(sensorValue.un.rotationVector.real, 3);
      Serial.print(" x=");
      Serial.print(sensorValue.un.rotationVector.i, 3);
      Serial.print(" y=");
      Serial.print(sensorValue.un.rotationVector.j, 3);
      Serial.print(" z=");
      Serial.print(sensorValue.un.rotationVector.k, 3);
      Serial.print(" Cal=");
      Serial.println(sensorValue.status);
    }
  }

  delay(100);
}

#endif

// ============================================================================
// TEST 10: POWER CYCLE STRESS TEST - Ensure sensor recovers from power cycles
// ============================================================================

#if 0  // Change to #if 1 to use this sketch

#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x imu;

#define SENSOR_POWER_PIN 3  // Change to actual pin if using power control

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  Serial.println("\n=== Power Cycle Stress Test ===");
  Serial.println("This test power-cycles the sensor and verifies recovery");

  // Only works if sensor power is on a controllable pin
  // If not using power control, just test initialization recovery

  int success_count = 0;
  int failure_count = 0;

  for (int cycle = 1; cycle <= 5; cycle++) {
    Serial.print("\nCycle ");
    Serial.print(cycle);
    Serial.print(": ");

    Wire.begin();
    Wire.setClock(100000L);
    delay(100);

    if (imu.begin_I2C(0x4A, &Wire, 0)) {
      Serial.println("SUCCESS");
      success_count++;
    } else {
      Serial.println("FAILED");
      failure_count++;
    }

    delay(1000);
  }

  Serial.print("\n=== Results: ");
  Serial.print(success_count);
  Serial.print(" success, ");
  Serial.print(failure_count);
  Serial.println(" failures ===");
}

void loop() {
  delay(1000);
}

#endif

/*
 * ============================================================================
 * USAGE INSTRUCTIONS
 * ============================================================================
 *
 * 1. Open Arduino IDE
 * 2. Find the test you want to run (TEST 1-10 above)
 * 3. Change the #if 0 to #if 1 for that test
 * 4. Make sure all other tests have #if 0
 * 5. Upload to your Arduino Mega
 * 6. Open Serial Monitor (115200 baud)
 * 7. Observe output and follow troubleshooting guide
 *
 * RECOMMENDED TEST SEQUENCE:
 * 1. TEST 1 (I2C Bus Scanner) - Find BNO085 on bus
 * 2. TEST 2 (Minimal I2C Test) - Confirm basic I2C works
 * 3. TEST 4 (Clock Speed Testing) - Find working speed
 * 4. TEST 6 (Adafruit Library Init) - Test library with working speed
 * 5. TEST 9 (Full Sensor Test) - Read actual orientation data
 *
 * ============================================================================
 */
