/*
 * Self Balancing Robot - Simplified (Based on Working Pitch Reader)
 * 
 * Wiring:
 * BNO055 VCC -> 3.3V or 5V
 * BNO055 GND -> GND
 * BNO055 SDA -> A4 (or SDA pin on Arduino)
 * BNO055 SCL -> A5 (or SCL pin on Arduino)
 * BNO055 ADR -> GND (for address 0x28)
 * 
 * L298N Motor Driver:
 * ENA -> 5
 * IN1 -> 6
 * IN2 -> 7
 * IN3 -> 9
 * IN4 -> 8
 * ENB -> 10
 */

#include <Wire.h>
#include <PID_v1.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// ===== MANUAL OFFSET VALUE =====
float PITCH_OFFSET = -8.6;  // Your measured offset from pitch reader
// =================================

// Motor pins
const int ENA = 5;
const int IN1 = 6;
const int IN2 = 7;
const int IN3 = 9;
const int IN4 = 8;
const int ENB = 10;

// PID variables
double setpoint = 0;
double input = 0;
double output = 0;
double Kp = 65;//55;//45;//150           // Proportional gain
double Kd = 38;//32;//28;//2.5          // Derivative gain  
double Ki =12;//10;// 8; // 100         // Integral gain

// Create PID object
PID pid(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

// Create BNO055 object (SAME as working pitch reader)
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

// Variables
float rawPitch = 0;
float correctedPitch = 0;
unsigned long lastPrint = 0;
int consecutiveErrors = 0;
bool motorOutputActive = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Self Balancing Robot - Simplified");
  Serial.println("==================================");
  Serial.print("Using manual offset: ");
  Serial.print(PITCH_OFFSET);
  Serial.println(" degrees");
  Serial.println();
  
  // Initialize motor pins
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Enable motors
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
  stopMotors();
  
  // Initialize BNO055 - EXACTLY like the working pitch reader
  if (!bno.begin()) {
    Serial.println("ERROR: BNO055 not detected!");
    Serial.println("Check wiring and I2C address");
    while (1);
  }
  
  Serial.println("BNO055 connected successfully!");
  
  // Use external crystal for better accuracy (same as pitch reader)
  bno.setExtCrystalUse(true);
  
  delay(500);
  
  // Setup PID controller
  pid.SetMode(AUTOMATIC);
  pid.SetSampleTime(5);
  pid.SetOutputLimits(-255, 255);
  
  Serial.println("Ready to balance!");
  Serial.println("---------------------------------------------------------\n");
  
  delay(1000);
}

void loop() {
  // Read pitch - EXACTLY like the working pitch reader
  sensors_event_t orientationData;
  bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
  
  // Read pitch (Z-axis is pitch for BNO055)
  rawPitch = orientationData.orientation.z;
  
  // Check if reading is valid
  if (!isnan(rawPitch) && abs(rawPitch) < 90) {
    // Apply manual offset (same as pitch reader)
    correctedPitch = rawPitch - PITCH_OFFSET;
    
    // Update PID input
    input = correctedPitch;
    
    // Calculate motor output
    pid.Compute();
    
    // Apply motor control
    controlMotors((int)output);
    motorOutputActive = true;
    
    // Reset error counter
    consecutiveErrors = 0;
    
    // Debug output (same format as pitch reader + output)
    if (millis() - lastPrint > 100) {
      Serial.print("Pitch: ");
      Serial.print(correctedPitch, 2);
      Serial.print("°");
      Serial.print("  |  Raw: ");
      Serial.print(rawPitch, 2);
      Serial.print("°");
      Serial.print("  |  Output: ");
      Serial.println(output);
      lastPrint = millis();
    }
  } else {
    // Invalid reading - stop motors
    if (motorOutputActive) {
      stopMotors();
      motorOutputActive = false;
    }
    
    consecutiveErrors++;
    
    if (consecutiveErrors > 10 && (millis() - lastPrint > 500)) {
      Serial.println("WARNING: Invalid sensor reading!");
      lastPrint = millis();
    }
    
    delay(10);
  }
  
  // Small delay to prevent overwhelming the system
  delay(2);
}

// Motor control function
void controlMotors(int speed) {
  int leftSpeed = constrain(speed, -255, 255);
  int rightSpeed = constrain(speed, -255, 255);
  
  // Minimum speed threshold - ensures motors have enough torque
  if (abs(leftSpeed) < 15 && leftSpeed != 0) {
    leftSpeed = (leftSpeed > 0) ? 15 : -15;
  }
  if (abs(rightSpeed) < 15 && rightSpeed != 0) {
    rightSpeed = (rightSpeed > 0) ? 15 : -15;
  }
  
  // Motor A (Left) - Pins 6 and 7
  if (leftSpeed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else if (leftSpeed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }
  analogWrite(ENA, abs(leftSpeed));
  
  // Motor B (Right) - Pins 8 and 9
  if (rightSpeed > 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (rightSpeed < 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }
  analogWrite(ENB, abs(rightSpeed));
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}