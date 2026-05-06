# BNO085 Communication Modes: Quick Reference Card

**For quick lookup: See full analysis in `bno085_communication_modes.md`**

---

## Mode Selection Matrix

### Current Project Setup ✅

| Aspect | Value |
|--------|-------|
| **Mode** | UART-SHTP (Standard SH-2) |
| **Arduino** | Mega 2560 |
| **Hardware Pins** | RX1=Pin 19, TX1=Pin 18 |
| **Serial Port** | Serial1 |
| **Baud Rate** | 115200 bps |
| **P0/P1 State** | P1=HIGH/5V, P0=LOW/GND |
| **Output Format** | Quaternion (w, x, y, z) |
| **Update Rate** | 10 Hz (configurable) |
| **Library** | Adafruit_BNO08x |
| **Init Function** | `begin_UART(&Serial1)` |

---

## Three Communication Options

### Option 1: I2C (Default Mode)

| Feature | Value |
|---------|-------|
| **Selection** | P0=LOW, P1=LOW (default) |
| **Pins (Mega)** | SCL=21, SDA=20 |
| **Address** | 0x4A or 0x4B (DI pin controls) |
| **Clock** | 100-400 kHz |
| **Wires** | 2 (SCL, SDA) + power |
| **Init** | `begin_I2C(0x4A)` |
| **Data Throughput** | ~600 B/s |
| **Complexity** | Low |
| **Recommended For** | Simplicity, standard Arduino projects |

### Option 2: UART-SHTP (Current Project) ✅

| Feature | Value |
|---------|-------|
| **Selection** | P0=LOW, P1=HIGH (5V) |
| **Pins (Mega)** | TX1=18, RX1=19 |
| **Baud Rate** | 115200 bps (fixed) |
| **Wires** | 2 (TX, RX) + power |
| **Init** | `begin_UART(&Serial1)` |
| **Data Throughput** | ~1300 B/s |
| **Complexity** | Medium (SHTP protocol) |
| **Recommended For** | Robust deployment, full sensor hub access |
| **⚠️ Critical** | **P1 MUST be 5V** |

### Option 3: UART-RVC (Robot Mode)

| Feature | Value |
|---------|-------|
| **Selection** | P0=HIGH (3.3V), P1=any |
| **Pins (Mega)** | TX1=18 only (RX not used) |
| **Baud Rate** | 115200 bps (fixed) |
| **Wires** | 1 (TX) + power |
| **Init** | Separate library: `Adafruit_BNO08x_RVC` |
| **Output** | Euler angles (yaw, pitch, roll) |
| **Pitch/Roll Range** | Limited (robot-optimized) |
| **Complexity** | Low (no SH-2 protocol) |
| **⚠️ Not Suitable** | **Drones/full pitch-roll range** |

---

## Absolute Orientation: The Key Difference

### Data Output Comparison

```
UART-SHTP / I2C Mode:
  Quaternion: w=0.7071, x=0.0000, y=0.0000, z=0.7071
  Magnitude: |q| ≈ 1.0 (normalized)
  Range: Full 360° (all orientations possible)

UART-RVC Mode:
  Euler: yaw=45°, pitch=2°, roll=-1°
  Format: Human-readable angles
  Range: pitch/roll limited to small angles (robot optimization)
```

### Critical for Project

**Project Requirement:** Deployable orientation sensors on drones/aerial vehicles  
**Implication:** Need full pitch/roll range  
**Decision:** **Must use UART-SHTP or I2C mode (NOT RVC)**

---

## Troubleshooting Flowchart

### "BNO085 UART initialization failed"

```
1. Check P1 voltage
   ├─ P1 not 5V? → MOST COMMON ISSUE
   │  └─ Solder 5V wire to P1 pin (or use GPIO set HIGH)
   │
2. Check pins 18/19 connected?
   ├─ Wrong pins? → Use logic analyzer to verify
   │  └─ Swap TX/RX connections if backwards
   │
3. Check Serial1 initialized?
   ├─ Did Serial1.begin(115200) execute?
   │  └─ Add debugging: Serial.println() before/after
   │
4. Check sensor powered?
   ├─ VIN = 3-5V? (should see LED if present)
   │  └─ Verify 3.3V on 3Vo pin
```

### "Quaternion magnitude not ≈ 1.0"

```
├─ Check data types in sensor_value struct
│  └─ Should be float32 (4 bytes each)
│
├─ Verify sensor initialized with enableReport()
│  └─ Must call after begin_UART()
│
└─ Check calibration status progressing
   └─ Status should go 0→1→2→3 over 30 seconds
```

---

## Pin Quick Reference

### Arduino Mega I2C
```
Pin 20 (SDA) ←→ BNO085 SDA
Pin 21 (SCL) ←→ BNO085 SCL
+ 4.7kΩ pull-ups on both lines (usually on breakout)
```

### Arduino Mega UART-SHTP (Current)
```
Pin 18 (TX1) ←→ BNO085 RX
Pin 19 (RX1) ←→ BNO085 TX
+ P1 pin MUST be tied to 5V (not 3.3V!)
```

### Adafruit Breakout Board Jumpers

| Jumper | Function | Current | For I2C | For UART |
|--------|----------|---------|---------|----------|
| **P0** | Mode select (PS0) | Open/GND | GND | GND |
| **P1** | Mode select (PS1) | Check solder | GND | 5V |
| **Pull-ups** | I2C terminators | Usually on | On | Off (not needed) |

---

## Code Template: Mode Selection

```cpp
// Compile-time selection (add to platformio.ini):
// [build_flags]
// -DUSE_I2C         ← Uncomment for I2C
// -DUSE_UART_SHTP   ← Uncomment for UART

#if defined(USE_I2C)
    #include <Wire.h>
    bool init_sensor() {
        return bno.begin_I2C(0x4A, &Wire);  // Default address
    }

#elif defined(USE_UART_SHTP)
    #include <HardwareSerial.h>
    bool init_sensor() {
        Serial1.begin(115200);
        delay(100);
        return bno.begin_UART(&Serial1);
    }

#else
    #error "Must define USE_I2C or USE_UART_SHTP"
#endif

// Enable absolute orientation in both modes:
bno.enableReport(SH2_ROTATION_VECTOR, 100000);  // 10 Hz update

// Read in both modes (identical):
sh2_SensorValue_t sensor_value;
if (bno.getSensorEvent(&sensor_value)) {
    if (sensor_value.sensorId == SH2_ROTATION_VECTOR) {
        float w = sensor_value.un.rotationVector.real;
        float x = sensor_value.un.rotationVector.i;
        float y = sensor_value.un.rotationVector.j;
        float z = sensor_value.un.rotationVector.k;
        uint8_t cal = sensor_value.status;
    }
}
```

---

## Calibration Status Interpretation

```
Status Value | Level      | Meaning
-------------|------------|----------------------------------------
      0      | Unreliable | Sensor warming up or uncalibrated
      1      | Low        | Initial calibration (move sensor)
      2      | Medium     | Good orientation (typical operation)
      3      | High       | Excellent (convergence complete)
```

**Time Progression:**
- 0-2 sec: Status 0 (warming up)
- 2-10 sec: Status 1 (low calibration)
- 10-30 sec: Status 2 (medium, usable)
- 30+ sec: Status 3 (high, optimal)

---

## Performance Metrics

| Metric | I2C (400 kHz) | UART (115200 bps) |
|--------|---------------|-------------------|
| **Frame Time** | ~10-100 µs | ~1-5 ms |
| **Throughput** | ~600 B/s | ~1300 B/s |
| **Latency** | ~3-10 ms | ~5-15 ms |
| **Power (active)** | ~3 mA (sensor) | ~3 mA (sensor) |
| **Wire Count** | 2 + power | 2 + power |
| **Complexity** | Low | Medium |
| **Robustness** | Good | Better (CRC) |

---

## Decision Matrix for Your Use Case

### Question: What should I use?

**For Auto-Orientation Project** (Drones/Aerial vehicles)
→ **UART-SHTP (Current choice)** ✅

**If you have I2C hardware only**
→ Switch to I2C mode, update code with `begin_I2C()`

**If adding a second sensor**
→ Use I2C with different addresses (0x4A and 0x4B) or different I2C buses

**If simplicity is priority**
→ Use I2C mode (fewer jumpers, default mode)

**If robustness is priority**
→ Use UART-SHTP (error checking with CRC)

---

## Common Mistakes Checklist

- [ ] ✅ P1 pin is physically connected to 5V (not 3.3V)
- [ ] ✅ RX/TX pins correct (18/19 on Mega for Serial1)
- [ ] ✅ Baud rate 115200 bps on both sides
- [ ] ✅ enableReport() called after initialization
- [ ] ✅ Power supply stable (3-5V acceptable)
- [ ] ✅ GND connected between Arduino and sensor
- [ ] ✅ Using correct library (Adafruit_BNO08x, not RVC variant)
- [ ] ✅ getSensorEvent() checks return value
- [ ] ✅ sensorId matches expected type (0x05 for rotation vector)
- [ ] ✅ Quaternion magnitude approximately 1.0

---

## Next Steps

1. **Verify actual hardware mode**: Check P0/P1 jumpers, P1 voltage
2. **Run test**: Compile and upload with `platformio run --target upload`
3. **Monitor**: `python3 tools/serial_monitor.py /dev/ttyACM0 --baud 115200`
4. **Validate**: Look for quaternion data with magnitude ≈ 1.0 and status progression 0→3

See full documentation: `bno085_communication_modes.md`
