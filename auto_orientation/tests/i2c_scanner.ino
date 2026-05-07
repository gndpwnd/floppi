/**
 * Simple I2C Scanner - Detects all devices on I2C bus
 *
 * Scans I2C addresses 0x00-0x7F and reports which ones respond.
 * BNO085 should be at 0x4A (DI pin to GND) or 0x4B (DI pin floating).
 */

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== I2C Address Scanner ===\n");

  // Initialize I2C at same settings as main code
  Wire.begin();
  Wire.setClock(100000L);  // 100 kHz

  Serial.println("Scanning I2C bus...\n");

  int count = 0;
  for (byte i = 0; i < 128; i++) {
    Wire.beginTransmission(i);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Found device at 0x");
      if (i < 16) Serial.print("0");
      Serial.println(i, HEX);
      count++;
    }
  }

  Serial.print("\nTotal devices found: ");
  Serial.println(count);

  if (count == 0) {
    Serial.println("\nWARNING: No I2C devices found!");
    Serial.println("Check:");
    Serial.println("  1. SDA wire (pin 20 on Mega) - connected?");
    Serial.println("  2. SCL wire (pin 21 on Mega) - connected?");
    Serial.println("  3. Pull-up resistors - present?");
    Serial.println("  4. BNO085 power (3.3V) - connected?");
    Serial.println("  5. BNO085 GND - connected?");
  }

  Serial.println("\nExpected: BNO085 at 0x4A or 0x4B");
  Serial.println("Done. Restart to scan again.\n");
}

void loop() {
  delay(1000);
}
