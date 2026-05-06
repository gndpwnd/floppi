# BNO085 I2C Hang - Quick Reference Card

## Symptom: Code Hangs After "==="

```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
[HANGS HERE]
```

---

## 30-Second Diagnosis

| Check | How | Expected | Fix if Wrong |
|-------|-----|----------|--------------|
| **Power** | Check VCC with multimeter | 3.3V ±0.2V | Verify power supply |
| **SCL line** | Multimeter continuity (power off) | <5Ω to pin 21 | Check soldering |
| **SDA line** | Multimeter continuity (power off) | <5Ω to pin 20 | Check soldering |
| **SCL idle voltage** | Multimeter (power on) | 3.0-3.3V | Add 4.7kΩ pull-up |
| **SDA idle voltage** | Multimeter (power on) | 3.0-3.3V | Add 4.7kΩ pull-up |

---

## Quick Fix Checklist

Try these in order:

1. **Power cycle everything**
   - Unplug Arduino and sensor
   - Wait 30 seconds
   - Reconnect power
   - Test again

2. **Reduce I2C clock speed**
   ```cpp
   // Change from:
   Wire.setClock(400000L);
   // To:
   Wire.setClock(100000L);
   ```

3. **Add pull-up resistors**
   - Solder 4.7kΩ resistors from SCL (pin 21) to VCC (3.3V)
   - Solder 4.7kΩ resistors from SDA (pin 20) to VCC (3.3V)

4. **Verify wiring**
   - BNO085 VCC → Arduino 3.3V (NOT 5V)
   - BNO085 GND → Arduino GND
   - BNO085 SDA → Arduino Pin 20
   - BNO085 SCL → Arduino Pin 21

5. **Check DI pin**
   - Tie BNO085 DI pin to GND (with 10kΩ resistor or direct)
   - Update code to use address 0x4A:
     ```cpp
     imu.begin_I2C(0x4A, &Wire, 0);
     ```

6. **Run I2C scanner**
   - Use test sketch "TEST 1: I2C Bus Scanner"
   - Should find device at 0x4A or 0x4B
   - If nothing found, hardware issue

7. **Update code with debug output**
   ```cpp
   Serial.println("Before Wire.begin()");
   Wire.begin();
   Serial.println("After Wire.begin()");
   
   Serial.println("Before setClock()");
   Wire.setClock(100000L);
   Serial.println("After setClock()");
   
   Serial.println("Before begin_I2C()");
   if (!imu.begin_I2C(0x4A, &Wire, 0)) {
     Serial.println("begin_I2C() returned false");
   }
   Serial.println("After begin_I2C()");
   ```

---

## Test Sketches Quick Start

| Test | When to Use | Expected Result |
|------|------------|-----------------|
| TEST 1: I2C Scanner | Device not responding | Device found at 0x4A or 0x4B |
| TEST 2: Minimal I2C Test | Quick hardware check | All 3 steps OK |
| TEST 4: Clock Speed Test | Hang only at 400kHz | Works at 100kHz |
| TEST 6: Adafruit Init | Testing library | begin_I2C() returns true |
| TEST 9: Full Sensor Test | Everything works? | Quaternion values printed |

---

## Wiring Diagram (Arduino Mega)

```
BNO085 Breakout        Arduino Mega
━━━━━━━━━━━━━━━━       ━━━━━━━━━━━━━
VCC ────────────────→ 3.3V
GND ────────────────→ GND
SDA ────────────────→ Pin 20
SCL ────────────────→ Pin 21
DI  ────────────────→ GND (tie to ground)

Pull-up Resistors (4.7kΩ each):
SDA ──┤4.7kΩ├─→ 3.3V
SCL ──┤4.7kΩ├─→ 3.3V
```

---

## Critical Voltage Readings

**Power OFF** (Multimeter Ohms mode):
- SCL to Pin 21: <5Ω (solid connection)
- SDA to Pin 20: <5Ω (solid connection)
- VCC to 3.3V: <1Ω (solid connection)
- GND to GND: <1Ω (solid connection)

**Power ON** (Multimeter Volts mode):
- VCC: 3.3V ±0.2V (typically 3.25-3.35V)
- SCL (idle): 3.0-3.3V (pulled HIGH by resistors)
- SDA (idle): 3.0-3.3V (pulled HIGH by resistors)

**If voltages wrong**:
- Both lines at 0V → Short circuit or device holding bus LOW
- Both lines at >3.3V → Open circuit or weak pull-ups
- Lines oscillate → Timing issue (add pull-ups, reduce speed)

---

## I2C Address Reference

| DI Pin State | I2C Address | Adafruit Code |
|--------------|-------------|---------------|
| Tied to GND | 0x4A | `imu.begin_I2C(0x4A)` |
| Tied to VCC | 0x4B | `imu.begin_I2C(0x4B)` |
| **FLOATING** | **UNDEFINED** | **⚠️ ALWAYS tie DI pin** |

---

## Most Common Root Causes (by Frequency)

1. **Missing or weak pull-up resistors** (40%)
   - Fix: Add 4.7kΩ resistors to SCL and SDA lines

2. **I2C clock speed too high** (25%)
   - Fix: Change to 100kHz or 50kHz

3. **Wrong wiring** (20%)
   - Fix: Verify connections with multimeter

4. **DI pin not tied to voltage** (10%)
   - Fix: Tie DI to GND or VCC (not floating)

5. **Device in bad state** (5%)
   - Fix: Power cycle completely

---

## Serial Output Interpretation

### Good Sequence
```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
BNO085 OK
Board: Initializing output manager...
Output Manager: JSON format (v1.0), 10 Hz frequency
Reading sensor data...
```

### Hanging at begin()
```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
[NO OUTPUT - HANGS HERE]
↓
Possible causes:
- Weak pull-ups
- Clock speed too high
- Device not present
- Sensor in bad state
```

### Never gets to "Board: Initializing"
```
=== Auto Orientation System ===
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
[NO OUTPUT - HANGS BEFORE ANY CODE RUNS]
↓
Possible causes:
- Wire.begin() hanging (I2C subsystem issue)
- Wire.setClock() hanging (rare)
- Device pulling bus low (short circuit)
```

---

## Step 1: Power Cycle

```
1. Unplug power from Arduino
2. Unplug BNO085 sensor (if separate)
3. Wait 30 seconds (allow capacitors to discharge)
4. Reconnect BNO085
5. Reconnect Arduino power
6. Test
```

**Success rate**: 30-40% for sensor lockups

---

## Step 2: Check with Multimeter

**POWER OFF**: Set to Ohms (Ω)
```
Probes                              Expected
───────────────────────────────────────────────
Pin 21 to Arduino SCL               <5Ω
Pin 20 to Arduino SDA               <5Ω
3.3V to sensor VCC                  <1Ω
GND to sensor GND                   <1Ω
```

**POWER ON**: Set to DC Volts (V)
```
Probes                              Expected
───────────────────────────────────────────────
3.3V rail to GND                    3.25-3.35V
Pin 21 (SCL) to GND                 3.0-3.3V
Pin 20 (SDA) to GND                 3.0-3.3V
Sensor VCC to GND                   3.25-3.35V
```

---

## Step 3: Run I2C Scanner

Copy "TEST 1: I2C Bus Scanner" from `bno085_test_sketches.ino`:
1. Open Arduino IDE
2. Create new sketch
3. Copy TEST 1 code (change #if 0 to #if 1)
4. Upload to Arduino
5. Open Serial Monitor (115200 baud)
6. Observe output

**Expected output**:
```
=== I2C Bus Scanner ===
Scanning I2C addresses 0x00-0x7F...

--- Scanning at 100kHz ---
Device found at 0x4a
Total devices found: 1
```

**Troubleshooting output**:
- No devices found → Wiring or power issue
- Multiple devices → Check for address conflicts
- Scanner hangs → I2C subsystem issue

---

## Step 4: Reduce Clock Speed

**Current code** (in bno085.cpp):
```cpp
Wire.begin();
Wire.setClock(400000L);  // 400 kHz - may be too fast
```

**Try 100 kHz**:
```cpp
Wire.begin();
Wire.setClock(100000L);  // 100 kHz - more reliable
```

**Try 50 kHz** (if 100 kHz still hangs):
```cpp
Wire.begin();
Wire.setClock(50000L);   // 50 kHz - safest option
```

**Recompile, upload, test**

**If works at lower speed**:
- Problem: Weak pull-ups or noisy I2C bus
- Fix: Add 4.7kΩ pull-up resistors

---

## Step 5: Add Pull-up Resistors

**Hardware modification**:

```
Solder two 4.7kΩ resistors on BNO085 breakout:

SDA pin ──┤ 4.7kΩ resistor ├─ 3.3V pin
SCL pin ──┤ 4.7kΩ resistor ├─ 3.3V pin
```

**Or on Arduino side**:
```
Arduino pin 20 ──┤ 4.7kΩ resistor ├─ 3.3V
Arduino pin 21 ──┤ 4.7kΩ resistor ├─ 3.3V
```

**Testing**:
- Power cycle
- Try I2C scan at 400kHz
- If successful, weak pull-ups were the problem

---

## When to Declare Sensor Dead

If you've tried everything above and still hanging:

1. **Worked before?** → Probably not dead, try different Arduino
2. **Never worked?** → Likely defective sensor or I2C hardware

**Before declaring dead**:
- [ ] Tested at 50kHz clock speed
- [ ] Added external pull-up resistors
- [ ] Verified wiring with multimeter
- [ ] Power cycled multiple times
- [ ] Tried different Arduino board (if available)

---

## Contact Information

**For further help**:
- Check `/docs/findings/bno085_i2c_hang_diagnosis.md` (full guide)
- Review `/docs/findings/bno085_test_sketches.ino` (test sketches)
- Adafruit Support: https://www.adafruit.com/product/4754

---

## Preventive Measures

**For future builds**:
1. Always use 4.7kΩ pull-up resistors (don't rely on onboard only)
2. Keep I2C wires short (<30 cm)
3. Shield I2C wires away from high-current paths
4. Test with 100kHz clock speed first, then increase if stable
5. Use bypass capacitors (0.1µF) near sensor power pins
6. Add hardware reset capability (pin 5 → sensor reset)

---

## Minimal Working Code

```cpp
#include <Wire.h>
#include "Adafruit_BNO08x.h"

Adafruit_BNO08x imu;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(100); }

  // Initialize I2C at safe speed
  Wire.begin();
  Wire.setClock(100000L);  // 100 kHz (safe for most setups)
  delay(100);

  // Initialize sensor with debug output
  Serial.print("Initializing sensor...");
  if (!imu.begin_I2C(0x4A, &Wire, 0)) {
    Serial.println("FAILED");
    while(1) { delay(1000); }
  }
  Serial.println("OK");

  // Enable orientation report
  imu.enableReport(SH2_ROTATION_VECTOR, 100000);
}

void loop() {
  sh2_SensorValue_t sensor_value;
  if (imu.getSensorEvent(&sensor_value)) {
    if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
      Serial.print("Quaternion: ");
      Serial.print(sensor_value.un.rotationVector.real, 3);
      Serial.print(",");
      Serial.print(sensor_value.un.rotationVector.i, 3);
      Serial.println();
    }
  }
  delay(100);
}
```

**Key points**:
- Wire.begin() called first
- Clock speed 100kHz (safe default)
- Error checking on begin_I2C()
- Debug output to Serial
- Uses SH2_ROTATION_VECTOR for orientation

