# BNO085 Pin Diagrams and Wiring Guide

---

## Adafruit BNO085 Breakout Board Layout

### Top View

```
                    [USB Type-C]
                         |
        ┌─────────────────────────────┐
        │   Adafruit BNO085 Logo      │
        │                             │
  GND ─ ●─────────────────────────●─ VIN (3-5V)
  NC ── ●─────────────────────────●─ 3Vo (3.3V out)
  INT ─ ●─────────────────────────●─ RST
  ADR ─ ●─────────────────────────●─ PS1
  PS0 ─ ●─────────────────────────●─ GND
  DI ── ●─────────────────────────●─ DO
        │     [solder jumpers      │
        │      on back]            │
  CS ── ●─────────────────────────●─ SCL/SDA (I2C)
                                     or RX (UART)
        └─────────────────────────────┘
```

### Bottom View (Solder Pads/Jumpers)

```
        ┌─────────────────────────────┐
        │  [Solder Jumper Area]       │
        │                             │
        │  P0 jumper (pull to GND)    │
        │  [    ]  ← default open     │
        │                             │
        │  P1 jumper (pull to VCC)    │
        │  [    ]  ← default open     │
        │                             │
        │  I2C Pull-up resistors      │
        │  [ X ]  ← usually populated │
        │                             │
        └─────────────────────────────┘
```

---

## I2C Mode Wiring (Default)

### Adafruit Breakout Configuration

```
BNO085 Breakout                Arduino Mega
─────────────────              ─────────────

VIN ───────────────────────→   5V
GND ───────────────────────→   GND
SCL ───────────────────────→   Pin 21 (SCL)
                               (via 4.7kΩ pull-up)
SDA ───────────────────────→   Pin 20 (SDA)
                               (via 4.7kΩ pull-up)
DI (optional)
  ├─→ GND  → I2C address 0x4A (default)
  └─→ 3Vo  → I2C address 0x4B
```

### Pull-up Resistor Configuration

I2C requires pull-up resistors on clock and data lines:

```
        5V
        │
        ┌───[4.7kΩ]───┬───[4.7kΩ]───┐
        │              │              │
        └─────SCL──────┤           SDA──────┘
                  Arduino Mega
                  ├─ SCL: Pin 21
                  └─ SDA: Pin 20

Alternate: 10kΩ resistors also acceptable
Note: Adafruit breakout usually has pull-ups built-in
```

### Block Diagram

```
┌──────────────────┐
│  Arduino Mega    │
│                  │
│  I2C Master      │
│  ┌────────────┐  │
│  │ Wire Lib   │  │
│  └────┬───┬──┘  │
│       │   │     │
│    SCL   SDA   │
│     ↓     ↓    │
└─────┼─────┼────┘
      │     │
   [4.7kΩ] [4.7kΩ]  Pull-ups to 5V
      │     │
      ├─────┼─────────┐
      │     │         │
      ◇     ◇    ┌────────────────┐
     SCL   SDA   │ BNO085         │
              │  │ I2C Slave      │
              └──┤ (0x4A/0x4B)    │
                 └────────────────┘
```

### I2C Communication Timing

```
SCL (Clock):  ┌─────┐   ┌─────┐   ┌─────┐
              │     └───┘     └───┘     └───
              
SDA (Data):   ┌─┐   ┌───┐ ┌──┐ ┌────┐
              │ └───┘   └─┘  └─┘    └──────
              
              START BIT ADDRESS   DATA    STOP
              
Frame Time @ 400 kHz: ~25-100 µs per byte
```

---

## UART-SHTP Mode Wiring (Current Project) ✅

### Adafruit Breakout Configuration

```
BNO085 Breakout                Arduino Mega
─────────────────              ─────────────

VIN ───────────────────────→   5V
GND ───────────────────────→   GND
SDA (=RX) ────────────────→    Pin 19 (RX1)
SCL (=TX) ────────────────→    Pin 18 (TX1)
P1 (JUMPER) ──────────────→    5V  ← CRITICAL!
P0 (JUMPER) ──────────────→    GND ← optional
```

### P1 Pin Connection (CRITICAL!)

```
WRONG - P1 not connected (floating):
P1 ──┐
     └─ (nothing) ✗ BNO085 won't initialize!

WRONG - P1 to 3.3V:
P1 ──┐
     └─ 3Vo ✗ May not work reliably (marginal voltage)

RIGHT - P1 to 5V:
      ┌─────→ 5V ✓ Correct! Sensor enters UART-SHTP mode
P1 ───┤
      │
      └─ (or GPIO pin set HIGH)

Alternative using GPIO:
P1 ───→ Arduino Pin (set digitalWrite(pin, HIGH))
        digitalWrite(P1_GPIO, HIGH);  // Enable UART mode
```

### UART Hardware Pins (Serial1)

```
Arduino Mega                   BNO085 Breakout
─────────────                  ───────────────

         RX1 (Pin 19) ←─────→ SDA/RX
         TX1 (Pin 18) ←─────→ SCL/TX

         GND ←─────→ GND
         5V ←─────→ VIN
         (any GPIO - Pin XX) ←─────→ P1 (set HIGH)
```

### Serial Configuration

```cpp
Serial1.begin(115200);
// Settings:
// - 8 data bits
// - 1 stop bit
// - No parity
// - No flow control
```

### Block Diagram

```
┌──────────────────┐
│  Arduino Mega    │
│                  │
│  UART Interface  │
│  ┌────────────┐  │
│  │ Serial1    │  │
│  │ 115200     │  │
│  │ 8N1        │  │
│  └────┬───┬──┘  │
│       │   │     │
│      TX  RX    │
│       ↓   ↓    │
└───────┼───┼────┘
        │   │
        ↓   ↓
    (Xtal)
       │ │
       ├─┤ UART Interface (async)
       │ │
    (Xtal)
        ↓ ↑
    ┌────────────────┐
    │ BNO085         │
    │ Sensor Hub 2   │
    │ SH-2 Protocol  │
    │ (SHTP frames)  │
    └────────────────┘
        │ │ │
        ├─┼─┤ 9-DOF sensors
        │ │ │
        ▼ ▼ ▼
      Acc Gyro Mag
```

### UART Communication Timing

```
TX Bit Stream:  S 0 1 1 0 1 0 1 S P
                │─────────────────│
                ← 10 bits @ 115200 bps = 86.8 µs

One frame (10-50 bytes):
[AA AA] [Header] [Length] [Data...] [CRC]
 2      1        2        0-256     4 bytes
 ↓
 Time: ~0.5-5 ms per complete SH-2 frame
```

### UART-SHTP Frame Structure

```
Byte 0-1:  AA AA      (SH-2 Preamble - sync marker)
Byte 2:    83         (Header: channel 3, continuation)
Byte 3-4:  00 0E      (Length: 14 bytes of data)
Byte 5:    05         (Report ID: rotation vector)
Bytes 6-9:   xxxxxxxx (Quaternion real/w component - float32)
Bytes 10-13: xxxxxxxx (Quaternion i/x component - float32)
Bytes 14-17: xxxxxxxx (Quaternion j/y component - float32)
Bytes 18-21: xxxxxxxx (Quaternion k/z component - float32)
Byte 22:   02         (Status/Accuracy: 0=unreliable, 3=high)
Bytes 23-26: xxxxxxxx (CRC-32 of data payload)
```

---

## UART-RVC Mode Wiring (Robot Mode)

### Configuration

```
BNO085 Breakout                Arduino Mega
─────────────────              ─────────────

VIN ───────────────────────→   5V
GND ───────────────────────→   GND
SDA (=TX only) ────────────→    Pin 18 (TX1)
P0 (JUMPER) ──────────────→    3.3V ← Sets RVC mode
P1 (JUMPER) ──────────────→    GND  (can be GND)
```

### TX-Only Configuration

```
UART-RVC Mode (TX only):
┌──────────────────┐
│  Arduino Mega    │
│                  │
│  RX1 (Pin 19) ───┤ Not used (no input needed)
│  TX1 (Pin 18) ───┼──→ SDA pin on BNO085
│  GND ────────────┼──→ GND
│  5V ─────────────┼──→ VIN
│                  │
└──────────────────┘

RVC Output Format: "Yaw, Pitch, Roll, Accel_X, Accel_Y, Accel_Z"
Output frequency: Fixed 100 Hz
No configuration needed (pre-programmed)
```

### ⚠️ Not Suitable for Project

**Do NOT use for Auto-Orientation project**
- Limited pitch/roll range (robot-optimized)
- Separate library required
- No quaternion output
- Cannot configure update rate

---

## Mode Selection Summary Table

### Jumper Configuration for Mode Selection

```
┌─────────────────────────────────────────────┐
│ Mode Selection via P0/P1 Jumpers            │
├─────────────────────────────────────────────┤
│ P0 State    │ P1 State    │ Result Mode     │
├─────────────┼─────────────┼─────────────────┤
│ GND (open)  │ GND (open)  │ I2C (default)   │
│ GND (open)  │ 5V (close)  │ UART-SHTP ✓     │
│ 3.3V (close)│ any         │ UART-RVC        │
│ 5V (close)  │ 5V (close)  │ SPI             │
└─────────────┴─────────────┴─────────────────┘

Default: Both open = I2C mode
Current Project: P1 closed to 5V = UART-SHTP mode ✓
```

### Visual Jumper Positions

```
Breakout Board Solder Side:

I2C Mode (Default):
    ┌─ P0 ─┐      ┌─ P1 ─┐
    │ [  ] │      │ [  ] │
    └─────┘      └─────┘
    Both OPEN      Both OPEN
    
UART-SHTP Mode (Current) ✓:
    ┌─ P0 ─┐      ┌─ P1 ─┐
    │ [  ] │      │ [##] │ ← SOLDERED/CLOSED
    └─────┘      └─────┘
    OPEN          CLOSED to VCC
    
UART-RVC Mode:
    ┌─ P0 ─┐      ┌─ P1 ─┐
    │ [##] │      │ [  ] │ ← any state
    └─────┘      └─────┘
    CLOSED to VCC  OPEN/CLOSED
```

---

## Complete System Diagram: UART-SHTP (Current)

```
                    ┌─────────────────────────────┐
                    │     POWER SUPPLY            │
                    │  5V ← (USB or external)     │
                    └────────────┬────────────────┘
                                 │
                    ┌────────────┴─────────────┐
                    ▼                          ▼
              ┌──────────┐              ┌──────────┐
              │ Arduino  │              │ BNO085   │
              │  Mega    │              │ Sensor   │
              └──────────┘              └──────────┘
                  │                         │ │ │
         ┌────────┼─────────┐         ┌─────┴─┴─┴─┐
         │        │         │         │ Sensors:  │
      RX1/19   TX1/18    GND       VIN  GND  3.3V
         │        │         │       │     │     │
         │        │         │       │     │     │
         ↓        ↓         ↓       ↓     ↓     ↓
    [RX line]  [TX line] [GND]→[VIN] [GND] [3.3Vo]
         │        │         │       │     │     │
         └────────┼─────────┴───────┼─────┴─────┘
                  │ Serial 1        │
                  │ 115200 baud     │ P1 pin
                  │ 8N1             │ (tied to 5V)
                  │                 │
         ┌────────┴─────┐       ┌───┴────────┐
         │ SH-2 Protocol│       │P0/P1 Config│
         │ (SHTP)       │       │ P0: GND    │
         │              │       │ P1: 5V ✓   │
         │ Frames:      │       │ (UART-SHTP)│
         │ AA AA ...... │       └────────────┘
         │ CRC         │
         └──────────────┘
              ↓
         ┌─────────────────┐
         │ 9-DOF Fusion    │
         │ Accelerometer   │
         │ Gyroscope       │
         │ Magnetometer    │
         │ + DSP Engine    │
         │ = Quaternion    │
         │   Output        │
         └─────────────────┘
```

---

## Arduino Mega Pin Reference

### All UART Ports

```
┌─────────────────────────────────────────────────────┐
│ Arduino Mega 2560 UART Pins                         │
├──────────────┬─────────┬─────────┬─────────────────┤
│ Serial Port  │ RX Pin  │ TX Pin  │ Default Use     │
├──────────────┼─────────┼─────────┼─────────────────┤
│ Serial0      │ 0       │ 1       │ USB/Bootloader  │
│ Serial1  ✓   │ 19      │ 18      │ BNO085 (Current)│
│ Serial2      │ 17      │ 16      │ Available       │
│ Serial3      │ 15      │ 14      │ Available       │
└──────────────┴─────────┴─────────┴─────────────────┘

Best choice for BNO085: Serial1 (pins 18/19)
- Dedicated hardware UART (not USB)
- Pins are clearly labeled
- No bootloader interference
```

### I2C/Two-Wire Interface

```
┌─────────────────────────────────┐
│ Arduino Mega I2C Pins           │
├─────────────┬───────────────────┤
│ Line        │ Pin               │
├─────────────┼───────────────────┤
│ SDA         │ Pin 20            │
│ SCL         │ Pin 21            │
│ Pull-ups    │ 4.7 kΩ (external) │
└─────────────┴───────────────────┘

Hardware I2C (Wire library) on Mega:
- Supports 100 kHz and 400 kHz
- Built-in pull-ups not sufficient (add external 4.7kΩ)
- Standard two-wire interface
```

---

## Voltage and Logic Levels

### Power Requirements

```
┌──────────────────────────────────┐
│ BNO085 Voltage Requirements      │
├──────────────────────────────────┤
│ VIN Input:  3.0 - 5.5 VDC        │
│ Logic High: 2.4 V (typical)      │
│ Logic Low:  0.4 V (typical)      │
│ Onboard Regulator: 3.3V output   │
│ Current Draw: ~3 mA (active)     │
│                ~2 mA (sleep)     │
└──────────────────────────────────┘

Recommended: Use 5V from Arduino
- Provides margin above minimum
- Onboard regulator handles conversion
- All GPIO signals at 3.3V or 5V tolerant
```

### Logic Level Tolerance

```
Arduino Output (5V tolerant):
  0V (LOW)   ←  Can accept down to 0V
  5V (HIGH)  ←  Can accept up to 5V

BNO085 Logic (3.3V native):
  0V (LOW)   ←  Accepts 0V as LOW
  3.3V (HIGH)←  Accepts 2.5-5V as HIGH

Conclusion: 5V signals to 3.3V input = SAFE
(Resistor divider optional but not needed)
```

---

## Troubleshooting Visual Guide

### Signal Integrity Check with Logic Analyzer

**I2C Mode:**
```
SCL: ┌─────┐   ┌─────┐   ┌─────┐    (clock line)
     │     └───┘     └───┘     └────
     
SDA: ├┐ ┌───┐ ┌──┐ ┌────┐         (data line)
     │└─┘   └─┘  └─┘    └────────
     
     START    ADDRESS    DATA    STOP
     ↑        ↑          ↑       ↑
     Good     Good       Good    Good
```

**UART-SHTP Mode:**
```
TX:  ├─┐ ┌┐┌──┐└┐┌──┐└┐ ┌┐┌──┐└┐┌──┐└┐ ┌┐
     │ └─┘│└┘  │ │  │ │ │└┘  │ │  │ │ │
     │   └────┘  └──┘ │ └────┘  └──┘ │ │
     S   0 1 0 0 1 1 0 0 P        (each byte)
     
     If you see: [AA AA] 83 ... → Correct SH-2 frame ✓
     If garbled: Check baud rate, P1 pin voltage
```

---

## Next Steps

1. **Check current configuration**: Examine P0/P1 solder pads
2. **Verify P1 voltage**: Should read 5V with multimeter (if UART mode)
3. **Test with logic analyzer**: Confirm correct protocol on bus
4. **Upload and monitor**: Check for initialization success

---

**Document Date**: 2026-05-06  
**Version**: 1.0  
**Related Docs**: `bno085_communication_modes.md`, `bno085_communication_modes_quick_reference.md`
