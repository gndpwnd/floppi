/**
 * BNO085 — Full Rotation Vector over I2C using Adafruit BNO08x Library
 *
 * Wiring (Arduino Mega):
 *   BNO085 SDA  →  Mega pin 20 (SDA)
 *   BNO085 SCL  →  Mega pin 21 (SCL)
 *   BNO085 P0   →  GND   (both LOW = I2C mode, address 0x4A)
 *   BNO085 P1   →  GND
 *   BNO085 RST  →  Mega pin 4  (set BNO_RESET_PIN to -1 if not wired)
 *   BNO085 VCC  →  3.3V
 *   BNO085 GND  →  GND
 *
 *  Adafruit BNO085 breakout has built-in pull-ups — no external resistors needed.
 *  Disconnect TX1/RX1 wires if they were previously connected.
 *
 * Library: Adafruit BNO08x
 *   Install via: Sketch → Include Library → Manage Libraries → "Adafruit BNO08x"
 *
 * Calibration status (per sensor event):
 *   0 = Unreliable   1 = Low   2 = Medium   3 = High ✓
 * Wave in a figure-8 to improve magnetometer (yaw) calibration.
 */

#include <Adafruit_BNO08x.h>

#define BNO_RESET_PIN      4       // set to -1 if RST pin is not wired
#define REPORT_INTERVAL_US 50000   // 50 000 µs = 20 Hz

Adafruit_BNO08x   bno(BNO_RESET_PIN);
sh2_SensorValue_t sensorValue;

// ── Cal status as readable string ─────────────────────────────────────────
const char* calLabel(uint8_t status) {
  switch (status) {
    case 0:  return "UNRELIABLE";
    case 1:  return "LOW";
    case 2:  return "MEDIUM";
    case 3:  return "HIGH";
    default: return "?";
  }
}

// ── Enable reports ────────────────────────────────────────────────────────
void enableReports() {
  if (!bno.enableReport(SH2_ROTATION_VECTOR, REPORT_INTERVAL_US)) {
    Serial.println(F("[ERR] Could not enable Rotation Vector report"));
  } else {
    Serial.println(F("[OK]  Rotation Vector report enabled"));
  }
}

// ── Quaternion → Euler (ZYX aerospace convention) ─────────────────────────
void quatToEuler(float qw, float qx, float qy, float qz,
                 float &yaw, float &pitch, float &roll) {
  float sinr_cosp = 2.0f * (qw * qx + qy * qz);
  float cosr_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
  roll = atan2f(sinr_cosp, cosr_cosp);

  float sinp = 2.0f * (qw * qy - qz * qx);
  sinp = constrain(sinp, -1.0f, 1.0f);
  pitch = asinf(sinp);

  float siny_cosp = 2.0f * (qw * qz + qx * qy);
  float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
  yaw = atan2f(siny_cosp, cosy_cosp);

  yaw   *= 180.0f / PI;
  pitch *= 180.0f / PI;
  roll  *= 180.0f / PI;
}

// ── Setup ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

  delay(1000);  // give BNO085 time to fully boot after Mega reset

  Serial.println(F("\n=== BNO085 I2C Full Rotation Vector (Adafruit) ==="));
  Serial.println(F("Connecting on I2C (SDA=pin20, SCL=pin21)..."));

  if (!bno.begin_I2C()) {
    Serial.println(F("[ERR] BNO085 not found. Check:"));
    Serial.println(F("       - SDA → pin 20, SCL → pin 21"));
    Serial.println(F("       - P0 and P1 both tied to GND"));
    Serial.println(F("       - VCC = 3.3V, GND connected"));
    Serial.println(F("       - TX1/RX1 wires disconnected"));
    while (true) { delay(1000); }
  }

  Serial.println(F("[OK]  BNO085 connected over I2C\n"));
  enableReports();

  Serial.println(F("\ntimestamp_ms, yaw_deg, pitch_deg, roll_deg, "
                   "qw, qx, qy, qz, accuracy_deg, cal_status"));
}

// ── Loop ──────────────────────────────────────────────────────────────────
void loop() {
  if (bno.wasReset()) {
    Serial.println(F("[INFO] BNO085 reset — re-enabling reports"));
    enableReports();
  }

  if (!bno.getSensorEvent(&sensorValue)) return;
  if (sensorValue.sensorId != SH2_ROTATION_VECTOR) return;

  float qw = sensorValue.un.rotationVector.real;
  float qx = sensorValue.un.rotationVector.i;
  float qy = sensorValue.un.rotationVector.j;
  float qz = sensorValue.un.rotationVector.k;
  float accuracyDeg = sensorValue.un.rotationVector.accuracy * (180.0f / PI);
  uint8_t calStatus = sensorValue.status & 0x03;

  float yaw, pitch, roll;
  quatToEuler(qw, qx, qy, qz, yaw, pitch, roll);

  //Serial.print(millis());        Serial.print(F(", ")); // disabel for serial plotter
  Serial.print(yaw,   2);        Serial.print(F(", "));
  Serial.print(pitch, 2);        Serial.print(F(", "));
  Serial.print(roll,  2);        Serial.print(F(", "));
  Serial.print(qw, 6);           Serial.print(F(", "));
  Serial.print(qx, 6);           Serial.print(F(", "));
  Serial.print(qy, 6);           Serial.print(F(", "));
  Serial.print(qz, 6);           Serial.print(F(", "));
  Serial.print(accuracyDeg, 2);  Serial.print(F(", "));
  //Serial.println(calLabel(calStatus)); // calibration words
  Serial.println(calStatus); // calibration number: 0 1 2 3 (3 is best)
}