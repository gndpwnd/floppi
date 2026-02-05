# 🔌 HARDWARE SETUP GUIDE

Complete wiring guide for Teensy 4.0/4.1 + MPU6050 + FS-iA6B flight controller.

---

## 📋 Bill of Materials

### Required Components

| Item | Quantity | Notes |
|------|----------|-------|
| Teensy 4.0 or 4.1 | 1 | Microcontroller |
| MPU6050 (GY-521) | 1 | 6-axis IMU |
| FS-iA6B receiver | 1 | 6-channel SBUS receiver |
| FlySky transmitter | 1 | FS-i6, FS-i6X, or compatible |
| Jumper wires | 8 | Female-to-male dupont |
| USB cable | 1 | Micro USB or USB-C (for Teensy) |
| ESCs | 4-6 | Electronic Speed Controllers |
| Motors | 4-6 | Brushless motors |
| Battery | 1 | LiPo 3S-4S |
| 5V BEC | 1 | Power for Teensy + Receiver |

### Optional Components

| Item | Purpose |
|------|---------|
| Servos | For control surfaces (VTOL/planes) |
| Buzzer | Audio feedback |
| LED strip | Status indication |
| GPS module | Position hold (future) |

---

## 🔧 Part 1: MPU6050 IMU Wiring

### Physical Mounting

**Location:** Mount IMU at center of gravity, oriented with:
- **X-axis** → Aircraft forward (nose direction)
- **Y-axis** → Aircraft right (starboard wing)
- **Z-axis** → Aircraft up (vertical)

**Mounting tips:**
- Use foam tape for vibration dampening
- Keep IMU level with aircraft when mounted
- Secure firmly - no movement!
- Route wires away from motors/ESCs (EMI)

---

### Wiring Diagram

```
MPU6050 GY-521 Module
┌─────────────────────┐
│  [●] VCC  (3.3V)    │───────────► Teensy Pin 3.3V (NOT 5V!)
│  [●] GND            │───────────► Teensy GND
│  [●] SCL            │───────────► Teensy Pin 19 (SCL)
│  [●] SDA            │───────────► Teensy Pin 18 (SDA)
│  [ ] XDA  (unused)  │
│  [ ] XCL  (unused)  │
│  [ ] AD0  (leave floating or GND)
│  [ ] INT  (unused)  │
└─────────────────────┘

Note: Some GY-521 boards can tolerate 5V on VCC,
but 3.3V is safer and recommended.
```

### Pin Mapping Table

| MPU6050 Pin | Wire Color | Teensy 4.0/4.1 Pin | Function |
|-------------|------------|-------------------|----------|
| VCC | Red | 3.3V | Power (3.3V) |
| GND | Black | GND | Ground |
| SCL | Yellow/Blue | 19 | I2C Clock |
| SDA | Green | 18 | I2C Data |
| XDA | - | Not connected | Aux I2C (unused) |
| XCL | - | Not connected | Aux I2C (unused) |
| AD0 | - | GND or float | I2C Address select |
| INT | - | Not connected | Interrupt (unused) |

### I2C Address Configuration

**Default address:** 0x68 (AD0 pin floating or connected to GND)

**Alternative address:** 0x69 (AD0 pin connected to 3.3V)

> ⚠️ **Important:** If you have multiple I2C devices and need to change address, connect AD0 to 3.3V and update `config.h`:
> ```cpp
> #define MPU6050_ADDRESS 0x69
> ```

---

### Testing MPU6050 Connection

**After wiring, test I2C communication:**

1. Upload code with:
   ```cpp
   // In main.cpp loop():
   printGyroData();
   printAccelData();
   ```

2. Open Serial Monitor (115200 baud)

3. **Expected output:**
   ```
   GyroX:0.05 GyroY:-0.12 GyroZ:0.03
   AccX:0.01 AccY:0.00 AccZ:1.01
   ```

4. **If no output or errors:**
   - Check wiring (especially SCL/SDA)
   - Verify power (3.3V, not 5V)
   - Try scanner sketch to find I2C address
   - Check for loose connections

---

## 📡 Part 2: FS-iA6B Receiver Wiring

### Receiver Overview

**FS-iA6B Features:**
- 6 channels (expandable to 10 with iBUS)
- SBUS, iBUS, or PPM output
- 2.4GHz AFHDS 2A protocol
- Dual antenna for better range
- Telemetry support (with compatible TX)

**For this project, we use SBUS protocol.**

---

### Transmitter Configuration

**⚠️ DO THIS FIRST before wiring!**

1. Power ON FlySky transmitter
2. Press and hold **MENU** button
3. Navigate: **System → RX Setup → Serial Mode**
4. Select: **SBUS** (NOT PPM, iBUS, or PWM)
5. Press **OK** to save
6. **Power OFF transmitter**
7. **Power ON transmitter** (must power cycle!)

**Verify:** Display should show "SBUS" indicator (location varies by TX model).

---

### SBUS Wiring Diagram

```
FS-iA6B Receiver
┌─────────────────────────────────┐
│  ╔═══════════════════════════╗  │
│  ║  [ANT] [ANT]   [BIND]    ║  │ ← Top view
│  ╚═══════════════════════════╝  │
│                                 │
│  SBUS Port (3-pin connector):   │
│  ┌───┬───┬───┐                  │
│  │ S │ + │ G │  ← Pin labels    │
│  └─┬─┴─┬─┴─┬─┘                  │
│    │   │   └──[Black]──► Teensy GND
│    │   └──────[Red]────► Teensy VIN (5V via USB or BEC)
│    └──────────[White]──► Teensy Pin 21 (RX5)
│                                 │
│  PPM/PWM Ports: (NOT USED)      │
│  [1] [2] [3] [4] [5] [6]        │
└─────────────────────────────────┘
```

### Pin Mapping Table

| FS-iA6B SBUS Pin | Wire Color | Teensy 4.0/4.1 Pin | Function |
|------------------|------------|-------------------|----------|
| G (Ground) | Black | GND | Ground |
| + (Power) | Red | VIN | 5V power |
| S (Signal) | White/Black | 21 (RX5) | SBUS data |

---

### Power Considerations

**FS-iA6B power requirements:**
- Voltage: 3.5V - 9.0V (5V typical)
- Current: ~70mA (normal), ~100mA (peak during bind)

**Power sources:**

| Source | Use Case | Notes |
|--------|----------|-------|
| **USB (via VIN)** | Development/Testing | ✅ Works for receiver only |
| **5V BEC** | Flight | ✅ Required when motors connected |
| **Battery direct** | Not recommended | ⚠️ Voltage too high (7.4V-11.1V) |

**⚠️ Important:** 
- Teensy VIN pin can handle 5V input
- USB provides 5V on VIN when connected
- For flight, use dedicated 5V BEC from main battery
- Do NOT power receiver from Teensy 5V pin (current limited)

---

### SBUS Protocol Details

**Technical specs:**
- Baud rate: 100000 (100 kbaud)
- Format: 8E2 (8 data bits, even parity, 2 stop bits)
- Signal: Inverted UART
- Frame rate: ~7ms (140Hz)
- Channels: 16 (we use 6)

**✅ Good news:** Teensy hardware automatically handles signal inversion!

---

### Binding Procedure

**Step-by-step binding:**

1. **Transmitter:** Power OFF
2. **Receiver:** Locate bind button (small button on top, may be recessed)
3. **Receiver:** Press and HOLD bind button
4. **Receiver:** While holding, connect power (plug in USB to Teensy)
5. **Receiver:** LED flashes RAPIDLY (2-3x per second) - keep holding!
6. **Transmitter:** Power ON
7. **Wait:** 3-5 seconds for binding
8. **Receiver:** LED becomes SOLID - binding complete!
9. **Release bind button**

**✅ Verification:**
- LED solid when TX on
- LED slow flash when TX off
- Move TX sticks → LED dims/brightens slightly

**If bind fails:**
- Make sure TX is in SBUS mode
- Try holding bind button longer (5+ seconds)
- Make sure TX is OFF before starting
- Some receivers: tap bind button quickly 5x instead of hold

---

### Receiver Antenna Placement

**For best range:**
- Mount **both antennas** perpendicular to each other (90° angle)
- Keep antennas away from:
  - Carbon fiber (shields signal!)
  - Metal parts
  - Motors and ESCs
  - High-current wiring
- Ideal: One vertical, one horizontal
- Route through foam/plastic, NOT carbon

---

### Channel Mapping

**Default Mode 2 (most common):**

| Channel | Control | Typical PWM Range | Function |
|---------|---------|-------------------|----------|
| CH1 | Right stick L/R | 1000-2000μs | Roll |
| CH2 | Right stick U/D | 1000-2000μs | Pitch |
| CH3 | Left stick U/D | 1000-2000μs | Throttle |
| CH4 | Left stick L/R | 1000-2000μs | Yaw |
| CH5 | Switch (2-pos) | 1000/2000μs | Arm/Disarm |
| CH6 | Switch (3-pos) | 1000/1500/2000μs | Flight mode |

**Testing channel mapping:**

```cpp
// In main.cpp loop():
printRadioData();
```

Output:
```
CH1:1500 CH2:1500 CH3:1000 CH4:1500 CH5:1000 CH6:1000
```

Move sticks and switches - values should change 1000-2000.

---

## ⚡ Part 3: Motor/ESC Wiring

### ESC Signal Wiring

```
Teensy 4.0/4.1 → ESC Signal Wires
─────────────────────────────────
Pin 0  →  ESC 1 (Front Left)
Pin 1  →  ESC 2 (Front Right)
Pin 2  →  ESC 3 (Back Right)
Pin 3  →  ESC 4 (Back Left)
Pin 4  →  ESC 5 (Optional)
Pin 5  →  ESC 6 (Optional)
```

### Motor Configuration (Quadcopter X)

```
       FRONT
         ↑
         
    1        2
    CCW      CW
      \    /
       \  /
       /  \
      /    \
    CW      CCW
    4        3
```

**Motor rotation:**
- Motor 1 (FL): Counter-clockwise (CCW)
- Motor 2 (FR): Clockwise (CW)
- Motor 3 (BR): Counter-clockwise (CCW)
- Motor 4 (BL): Clockwise (CW)

**Propeller direction:**
- CCW motors: CCW props (pusher)
- CW motors: CW props (puller)

**⚠️ CRITICAL:** Wrong motor direction = immediate flip on takeoff!

---

### ESC Power Distribution

```
Battery (7.4V - 14.8V)
    │
    ├─► Power Distribution Board (PDB)
    │       │
    │       ├─► ESC 1 → Motor 1
    │       ├─► ESC 2 → Motor 2
    │       ├─► ESC 3 → Motor 3
    │       └─► ESC 4 → Motor 4
    │
    └─► 5V BEC (Step-down regulator)
            │
            ├─► Teensy VIN (5V)
            └─► FS-iA6B +5V
```

**Common grounds:**
- Connect ALL ESC grounds together
- Connect ESC ground to Teensy GND
- Connect BEC ground to Teensy GND
- One common ground for entire system!

---

## 🔋 Part 4: Power System

### Power Architecture

```
┌────────────────────────────────────────┐
│  Main Battery (LiPo 3S-4S)             │
│  11.1V - 14.8V                         │
└───────────┬────────────────────────────┘
            │
            ├─► High Current Path
            │   └─► ESCs → Motors
            │
            └─► Low Current Path
                └─► 5V BEC (3A-5A)
                    │
                    ├─► Teensy VIN (5V, ~200mA)
                    │
                    ├─► FS-iA6B (5V, ~70mA)
                    │
                    └─► Spare capacity for future peripherals
```

### Current Budget

| Component | Typical Current | Peak Current |
|-----------|----------------|--------------|
| Teensy 4.0 | 100mA | 150mA |
| MPU6050 | 3.5mA | 5mA |
| FS-iA6B | 70mA | 100mA |
| **Total** | **~175mA** | **~260mA** |

**Recommendation:** 5V BEC rated for 3A or higher (plenty of headroom).

---

### BEC Selection

**Good options:**
- Castle Creations BEC (5V 5A) - Expensive, rock solid
- Pololu 5V Step-Down (3A) - Cheap, works well
- Hobbywing 5V BEC (3A) - Mid-range, reliable

**⚠️ Avoid:**
- Linear regulators (inefficient, get hot)
- BECs rated <1A (may brownout)
- BECs with no LC filter (RF noise)

---

### Battery Connection Safety

**⚠️ CRITICAL SAFETY:**

1. **ALWAYS** use XT60 or similar high-quality connectors
2. **NEVER** reverse polarity (+ and - swapped = 💥)
3. **ALWAYS** disconnect battery when:
   - Uploading code
   - Changing wiring
   - Not actively testing
4. **USE** a battery alarm (monitors cell voltage)
5. **CHECK** battery voltage before EVERY flight
6. **STORE** LiPo batteries at storage voltage (3.8V per cell)

---

## 🎛️ Part 5: Optional Components

### Servos (for VTOL/Planes)

```
Servo Signal Wires  →  Teensy Pins
─────────────────────────────────
Servo 1  →  Pin 6
Servo 2  →  Pin 7
Servo 3  →  Pin 8
Servo 4  →  Pin 9
Servo 5  →  Pin 10
Servo 6  →  Pin 11
Servo 7  →  Pin 12
```

**⚠️ Servo Power:**
- Do NOT power servos from Teensy!
- Use separate 5V BEC rated for servo current (often 3A+)
- Servos can draw 500mA+ each under load

---

### Buzzer (Audio Feedback)

```
Active Buzzer  →  Teensy Pin 14
GND            →  Teensy GND
VCC (5V)       →  Teensy 5V (via transistor/MOSFET)
```

**Use for:**
- Arming confirmation (beep pattern)
- Low battery alarm
- Failsafe warning

---

## 🧰 Part 6: Assembly Tips

### Wire Management

**Best practices:**
1. **Measure twice, cut once** - leave 2-3cm extra length
2. **Label wires** - use colored tape or markers
3. **Bundle together** - use zip ties or heat shrink
4. **Route away from props** - obvious but critical!
5. **Strain relief** - use hot glue at connection points
6. **Test BEFORE zip-tying** - easier to fix mistakes

---

### Soldering Tips

**For header pins on modules:**
- Use 60/40 or 63/37 rosin-core solder
- 350°C iron temperature
- Tin both surfaces first
- Heat joint, not solder
- Inspect for cold joints (dull/grainy appearance)

**For high-current wiring:**
- Use 16-18 AWG silicone wire
- Pre-tin wire ends
- Heat shrink over all exposed connections
- Mechanical strain relief before soldering

---

### EMI/RFI Reduction

**Electromagnetic interference can corrupt sensor data.**

**Mitigation strategies:**
1. **Twist power pairs** - positive and negative wires together
2. **Shielded cables** for long sensor runs (optional but nice)
3. **Ferrite beads** on power wires near ESCs
4. **Separate signal/power grounds** at one central star point
5. **Route sensor wires perpendicular** to motor wires (not parallel)
6. **Add capacitors** (100nF ceramic + 10μF electrolytic) at power input

---

## 🧪 Part 7: System Testing

### Pre-Flight Electrical Tests

**With battery DISCONNECTED:**

1. **Continuity test:**
   - Check all GND connections are common
   - Check no shorts between VCC and GND
   - Check signal lines not shorted to power

2. **Voltage test:**
   - Connect USB to Teensy
   - Measure 3.3V on MPU6050 VCC pin
   - Measure 5V on FS-iA6B VCC pin
   - Should be stable (no fluctuation)

3. **Current test:**
   - Measure current draw with USB: ~175mA
   - Should NOT exceed 500mA (USB limit)

**With battery CONNECTED (props OFF!):**

1. **Voltage test:**
   - Measure battery voltage: 11.1V - 12.6V (3S)
   - Measure 5V BEC output: 4.9V - 5.1V
   - Measure Teensy VIN: 4.9V - 5.1V

2. **Arm/disarm test:**
   - Follow arming procedure
   - Watch for "ARMED" message
   - Slowly raise throttle (props OFF!)
   - Motors should attempt to spin
   - Disarm - motors stop immediately

---

## 📐 Final Wiring Diagram

```
                    COMPLETE SYSTEM WIRING
                                                                
         ┌─────────────────────────────────────────────┐
         │         LiPo Battery (3S-4S)                │
         │         11.1V - 14.8V                       │
         └────┬──────────────────────────┬─────────────┘
              │                          │
              │                          │ 5V BEC
              │                          │ (3A-5A)
              │                          │
         ┌────▼──────┐               ┌───▼────────────────┐
         │    PDB    │               │    Teensy 4.0     │
         │           │               │                    │
         │  ESC 1 ───────► Motor 1   │  Pin 0 ──► ESC 1   │
         │  ESC 2 ───────► Motor 2   │  Pin 1 ──► ESC 2   │
         │  ESC 3 ───────► Motor 3   │  Pin 2 ──► ESC 3   │
         │  ESC 4 ───────► Motor 4   │  Pin 3 ──► ESC 4   │
         │           │               │                    │
         └───────────┘               │  Pin 18 ─┐         │
                                     │  Pin 19 ─┤         │
                                     │  3.3V ───┤         │
                                     │  GND ────┤         │
                                     │          │         │
                                     │  Pin 21 ─┤         │
                                     │  VIN ────┤         │
                                     │  GND ────┤         │
                                     └──────────┼─────────┘
                                                │
                    ┌───────────────────────────┼──────────┐
                    │                           │          │
              ┌─────▼─────┐            ┌────────▼───────┐ │
              │  MPU6050  │            │   FS-iA6B      │ │
              │           │            │   Receiver     │ │
              │  SDA ─────┘            │                │ │
              │  SCL                   │  SBUS ─────────┘ │
              │  VCC                   │  +5V             │
              │  GND                   │  GND             │
              └───────────┘            └──────────────────┘

Legend:
─── Power connection
──► Signal connection
```

---

## ✅ Final Checklist

Before first flight, verify:

- [ ] All wiring matches diagrams above
- [ ] No loose connections or cold solder joints
- [ ] MPU6050 powered from 3.3V (not 5V)
- [ ] Receiver bound to transmitter
- [ ] Motor directions tested (props OFF)
- [ ] Propeller directions match motor rotation
- [ ] Arm/disarm function tested
- [ ] Battery voltage checked (>11.1V for 3S)
- [ ] All grounds connected together
- [ ] No shorts between power and ground
- [ ] Strain relief on all connections
- [ ] Wires secured away from props
- [ ] LED status indication working

**When all checked ✅ → Ready for calibration!**

📖 **Next:** [CALIBRATION_GUIDE.md](./CALIBRATION_GUIDE.md)