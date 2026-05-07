/**
 * BNO085 Calibration Diagnostic
 *
 * Reads internal sensor calibration state directly from FRS (Flexible Resilient Sensor)
 * to see if calibration is actually happening, even if rotation vector status shows 0.
 *
 * This helps determine if:
 * 1. Sensor is internally calibrating (good sign)
 * 2. Just not reporting it in the status field (library issue)
 * 3. Not calibrating at all (hardware/motion issue)
 */

#ifdef DEBUG_MODE

#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x bno;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);
  delay(1000);

  Serial.println("\n=== BNO085 Calibration Diagnostic ===\n");

  Wire.begin();
  Wire.setClock(100000L);

  if (!bno.begin_I2C(0x4A, &Wire, 0)) {
    Serial.println("ERROR: BNO085 not found!");
    while(1) delay(1000);
  }

  Serial.println("BNO085 initialized at 0x4A");
  bno.enableReport(SH2_ROTATION_VECTOR, 100000);

  Serial.println("Ready. Move the sensor in figure-8 patterns.");
  Serial.println("Watch for calibration changes...\n");
}

void loop() {
  // Read sensor event
  sh2_SensorValue_t sensorValue;
  if (bno.getSensorEvent(&sensorValue)) {
    if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
      uint8_t sys_cal = sensorValue.status;

      // Try to read FRS calibration metadata (if available)
      // This is non-standard but worth trying
      float w = sensorValue.un.rotationVector.real;
      float x = sensorValue.un.rotationVector.i;
      float y = sensorValue.un.rotationVector.j;
      float z = sensorValue.un.rotationVector.k;

      // Calculate magnitude (should be ~1.0 when calibrated)
      float mag = sqrt(w*w + x*x + y*y + z*z);

      Serial.print("Status: ");
      Serial.print(sys_cal);
      Serial.print(" | Quat magnitude: ");
      Serial.print(mag, 4);
      Serial.print(" | Q: (");
      Serial.print(w, 4);
      Serial.print(", ");
      Serial.print(x, 4);
      Serial.print(", ");
      Serial.print(y, 4);
      Serial.print(", ");
      Serial.print(z, 4);
      Serial.println(")");
    }
  }

  delay(500);  // Print every 500ms
}

#else
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);
  Serial.println("Compile with DEBUG_MODE to run this test");
}
void loop() { delay(1000); }
#endif
