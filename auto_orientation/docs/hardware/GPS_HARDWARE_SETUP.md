# GPS Hardware Setup Guide

**Status**: Phase 2 Hardware Documentation  
**Last Updated**: 2026-05-07  
**Related Docs**: [GPS Driver API](../reference/GPS_DRIVER_API_REFERENCE.md), [Troubleshooting](GPS_TROUBLESHOOTING.md)

## Overview

This guide covers physical hardware setup for connecting a UART-based GPS module (NEO-M8/M9N/M10S) to an Arduino Mega running the auto_orientation firmware.

**What You'll Learn:**
- Which GPS modules are compatible
- Pinout and wiring diagram
- Power supply requirements
- Baud rate configuration
- Signal level shifting (if needed)
- Antenna positioning for best results

---

## Supported GPS Modules

### Recommended: u-blox NEO-M9N

**Specifications**:
- **Interface**: UART (TTL serial)
- **Baud Rates**: 4800, 9600 (default), 19200, 38400, 115200
- **Chipset**: u-blox M9 engine (dual-frequency GNSS)
- **Signals Supported**: GPS, GLONASS, Galileo, BeiDou
- **Time to First Fix (TTFF)**:
  - Cold start: 25-45 seconds
  - Warm start: 5-15 seconds
  - Hot start: 1-2 seconds
- **Accuracy**: ±2-5 meters (typical, HDOP < 2.0)
- **Update Rate**: 1-10 Hz (1 Hz default)
- **Power**: 3.3V or 5V, ~50-150 mA

**Where to Buy**:
- Adafruit (UART variant): "Adafruit Ultimate GPS Breakout - NEO-M9N"
- SparkFun: "GPS-19032" (u-blox ZED-F9P eval kit)
- u-blox Direct: Browse "Evaluation Kits" on u-blox.com

**Advantages**:
- Excellent accuracy with multi-constellation support
- Wide baud rate support (9600 and 115200 common)
- Small form factor (breakout board ~1" x 1")
- Well-documented, reliable
- Works with standard level shifters

### Alternative: Adafruit Ultimate GPS

**Specifications**:
- **Chipset**: u-blox LEA-6H or NEO-6M (earlier generation)
- **Update Rate**: 1 Hz
- **Accuracy**: ±3-10 meters
- **Power**: 5V (can operate at 3.3V with level shifter)

**Status**: Works fine, but older chipset (LEA-6H) - recommend M9N if available

### Alternative: SparkFun GPS Breakout

**Chipset**: u-blox ZED-F9P  
**Status**: Works, more advanced than needed for this project

### NOT Recommended: USB-Only GPS Modules

The following modules will **NOT work** with this project:
- Garmin eTrex (USB only)
- u-blox C94-M8P (USB only)
- Adafruit USB GPS modules

We need UART-based modules. If you have USB-only hardware, you'll need to:
1. Use a different microcontroller with USB support, OR
2. Add a USB-to-UART converter bridge (more complex)

---

## Arduino Mega UART Pin Assignments

The Arduino Mega has 4 hardware UARTs. GPS can use any of them:

### Arduino Mega UART Pinout

| UART | Serial Object | RX Pin | TX Pin | Notes |
|------|---------------|--------|--------|-------|
| 0 | Serial (USB) | 0 | 1 | Reserved for USB-to-Serial (debugging) |
| 1 | Serial1 | 19 | 18 | **Recommended for GPS** |
| 2 | Serial2 | 17 | 16 | Alternative (e.g., if Serial1 occupied) |
| 3 | Serial3 | 15 | 14 | Alternative (e.g., if Serial1/2 occupied) |

**Why Serial1 (UART 1) is Recommended:**
- UART0 is reserved for USB serial debugging
- Serial1 (pins 18/19) is most accessible on Mega pinout
- Leave Serial2/3 available for future sensors

---

## Wiring Diagram

### Simple Connection (5V GPS Module)

```
                    ┌─────────────────────────┐
                    │  u-blox NEO-M9N Module  │
                    └─────────────────────────┘
                          │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
       TX                RX/TX              GND
        │                  │                  │
        ↓                  ↓                  ↓
    (GPS TX)          (UART connection)   (Ground)
        │                  │                  │
        │                  │         ┌────────┴────────┐
        │                  │         │                 │
        ↓                  ↓         ↓                 ↓
    Arduino Mega: Pin 19 ─ Pin 18 ─ GND ────── GND ─── +5V
    (RX1)         (TX1)              │                   │
                                     └─ Capacitor ──────┘
                                        (1000µF, 16V)
```

### Detailed Pin Connections

**GPS Module → Arduino Mega:**

| GPS Pin | Function | Arduino Mega Pin | Notes |
|---------|----------|------------------|-------|
| TX | Data Output | RX1 (pin 19) | Serial1 receive |
| RX | Data Input | TX1 (pin 18) | Serial1 transmit |
| GND | Ground | GND | Must connect |
| VCC | Power +5V | +5V | Via capacitor (see below) |
| (optional: PPS) | Pulse per second | Can ignore | Not used in this project |

### 3.3V GPS Module with Level Shifter

If using a 3.3V GPS module without integrated 5V tolerant inputs:

```
                   ┌──────────────────┐
                   │  3.3V GPS Module │
                   └──────────────────┘
                         │
            ┌────────────┴────────────┐
            │                         │
           TX (3.3V)                RX (3.3V)
            │                         │
            ↓                         ↓
      ┌─────────────────┐    ┌─────────────────┐
      │ Level Shifter   │    │ Level Shifter   │
      │ (BSS138 or      │    │ (BSS138 or      │
      │  TXB0106)       │    │  TXB0106)       │
      └─────────────────┘    └─────────────────┘
            │                         │
            │  (5V side)              │  (5V side)
            ↓                         ↓
       Arduino Mega             Arduino Mega
       Pin 19 (RX1)            Pin 18 (TX1)
```

**Popular Level Shifter Options:**
- **Adafruit BSS138 Level Shifter**: Simple, one level shifter handles multiple channels
- **SparkFun Logic Level Converter**: Bi-directional (ideal)
- **DIY with transistors**: Cheapest option if you're experienced

---

## Power Supply Setup

### Power Requirements

**GPS Module Power Draw:**
- Typical: 50-100 mA at 5V
- Peak (on startup): 150-200 mA
- Always-on current: ~50 mA

**Arduino Mega Power Draw:**
- With BNO085 I2C sensor: ~50 mA additional
- USB powered: Limited to ~500 mA total from USB

### Recommended Power Configuration

**Option A: Separate 5V Regulator (Recommended)**

```
  ┌─────────────┐
  │  5V Power   │ (USB power supply or battery)
  │  Supply     │ (2A recommended)
  └──────┬──────┘
         │
    ┌────┴─────┐
    │           │
    ↓           ↓
Arduino   GPS Module
(+5V)     (+5V)
    │           │
    └─── 1000µF capacitor
         (16V, close to GPS VCC pin)
```

**Why this works:**
- Dedicated 5V regulator can supply both Arduino and GPS
- Large capacitor smooths voltage spikes during GPS acquisition
- Prevents voltage droop on Arduino USB line

**Bill of Materials:**
- 5V/2A USB power supply
- 1000µF/16V electrolytic capacitor
- Wires (22-24 AWG)

**Capacitor Placement:**
- Mount capacitor **very close** to GPS module VCC pin (< 2 cm)
- This is critical! GPS draws bursts of current during satellite acquisition

### Option B: USB Powered (Simple, Limited)

**Pros:**
- No separate power supply needed
- Works for indoor testing

**Cons:**
- USB port limited to 500 mA
- May not have enough power if GPS cannot lock
- Not recommended for deployment

**Will work if:**
- You're running only GPS + Arduino (no BNO085)
- You have a high-power USB hub (2A output)
- Antenna has good sky view (quick satellite acquisition)

### Option C: Battery Powered (Field Deployment)

```
  ┌────────────────┐
  │ LiPo Battery   │ (11.1V 3S or similar)
  │ (3S LiPo ~12V) │
  └───────┬────────┘
          │
          ↓
    ┌─────────────┐
    │ 5V Regulator│ (USB-C buck converter, 2A output)
    │ (LM7805 or  │ e.g., Adafruit USB-C Breakout
    │ buck conv.) │
    └─────┬───────┘
          │
    ┌─────┴─────────────────────┐
    │                           │
    ↓                           ↓
Arduino                   GPS Module
(+5V)                     (+5V)
    │                           │
    └── 1000µF capacitor ───────┴── (1000µF on GPS VCC)
         (16V)
```

---

## Baud Rate Configuration

### Build Flags

Configure baud rate when building the firmware:

#### Default: 9600 Baud
```bash
platformio run -e arduino_mega_gps -t upload
```
This uses 9600 baud (default for most GPS modules)

#### High Speed: 115200 Baud
```bash
platformio run -e arduino_mega_gps_115200 -t upload
```
Use this if your GPS module runs at 115200 baud

### Setting GPS Module Baud Rate

Most u-blox GPS modules default to 9600 baud. To change it:

**Option 1: u-Center Software (Recommended)**
1. Download u-Center from u-blox website
2. Connect GPS to computer with USB-to-UART adapter
3. In u-Center, go to Tools > Receiver > Port Settings
4. Set "Target Baud Rate" to desired rate (9600 or 115200)
5. Click "Set Baud Rate"
6. Device remembers setting even after power-off

**Option 2: AT Commands**
Send this NMEA sentence to the GPS (9600 baud):
```
$PUBX,41,1,0,57,115200,0*25
```
This sets UART1 to 115200 baud

**Option 3: Default (Leave at 9600)**
- Most modules ship at 9600 baud
- This is the safest option if unsure
- Works fine for all applications

### Verifying Baud Rate

Use Arduino Serial Monitor:
1. Upload firmware with matching baud rate
2. Open Serial Monitor at same baud rate
3. Should see NMEA sentences (e.g., `$GNGGA,...`)
4. If garbage text, baud rate mismatch - try different rate

---

## Antenna Setup

### Antenna Types

**Passive Antenna (Patch Antenna)**
- Small square patch (~25mm x 25mm)
- No power required
- Typical gain: 5 dBi
- Range: Direct line-of-sight ~10 meters outdoor
- Cost: $5-15
- **Recommended for indoor/prototyping**

**Active Antenna (with amplifier)**
- Looks similar but heavier (contains amplifier)
- Requires +3.3V or +5V power (via special connector)
- Typical gain: 25-28 dBi (much better!)
- Range: ~50 meters outdoor through vegetation
- Cost: $20-50
- **Recommended for deployment/difficult environments**

### Antenna Positioning

**Outdoors (Best Performance)**
- Place antenna on roof or high point
- Need clear sky view (no obstructions)
- Avoid metal structures/gutters above antenna
- Mount on ground plane (metal plate ≥ 10cm x 10cm)
- Typical TTFF: 25-45 seconds cold start

**Window Mount (Acceptable)**
- Inside near window with clear view
- Non-metal window films work fine
- Metal-coated windows block signal
- Typical TTFF: 2-5 minutes cold start
- Position: Highest point in room

**Indoors (Poor Performance)**
- Works only if near large window
- Will require 5-30 minutes for cold start
- HDOP will be 5-10 (poor accuracy)
- **Not recommended for production use**

### Testing Antenna Performance

Check antenna quality with HDOP and satellite count:

**Excellent** (active antenna, clear sky view):
- 12+ satellites
- HDOP < 1.0
- Accuracy ±1-2 meters

**Good** (passive antenna, clear sky view):
- 8-10 satellites
- HDOP < 2.0
- Accuracy ±2-5 meters

**Adequate** (passive antenna, partial view):
- 5-7 satellites
- HDOP < 5.0
- Accuracy ±10-20 meters

**Poor** (indoor or obstructed):
- < 4 satellites (no fix possible)
- HDOP > 5.0
- No reliable position

---

## Wiring Checklist

Before powering up, verify:

- [ ] GPS TX (output) → Arduino RX1 (pin 19)
- [ ] GPS RX (input) → Arduino TX1 (pin 18)
- [ ] GPS GND → Arduino GND
- [ ] GPS +5V → +5V (with capacitor)
- [ ] 1000µF capacitor between GPS VCC and GND (close to GPS)
- [ ] No loose wires touching other components
- [ ] All connectors firmly seated
- [ ] Antenna connected and positioned properly

---

## Testing the Connection

### Step 1: Verify Firmware Builds

```bash
cd /home/devel/floppi/auto_orientation
platformio run -e arduino_mega_gps
```

Should compile without errors.

### Step 2: Upload Firmware

```bash
platformio run -e arduino_mega_gps -t upload
```

Should complete successfully.

### Step 3: Open Serial Monitor

```bash
platformio device monitor -b 115200
```

You should see boot message:
```
Auto Orientation System [CALIBRATION MODE]
Initializing sensors...
Board: Initializing BNO085 IMU sensor...
✓ BNO085 OK
```

### Step 4: Verify GPS Output

Within 30-60 seconds, you should see GPS data:
```
{
  "timestamp": 12345,
  "position": {
    "gps": {
      "satellites": 8,
      "hdop": 1.20,
      "latitude": 48.13745,
      "longitude": 11.58550,
      "altitude_m": 520.3,
      "locked": true
    }
  }
}
```

If no GPS output after 60 seconds:
- Check antenna position (move to window or outdoors)
- Verify wiring (especially RX/TX pins)
- Check baud rate (should be 9600 by default)
- See Troubleshooting guide below

---

## Troubleshooting Hardware Issues

### No Data on Serial Port

**Symptoms**:
- Serial monitor shows boot message but no GPS sentences
- Status shows "NO FIX" indefinitely

**Causes & Fixes**:

1. **Baud Rate Mismatch**
   - Verify firmware uses correct baud rate: `platformio run -e arduino_mega_gps` for 9600
   - Check serial monitor is set to same baud rate
   - GPS may be configured at different baud (check with u-Center)

2. **RX/TX Swapped**
   - GPS TX should go to Arduino RX1 (pin 19)
   - GPS RX should go to Arduino TX1 (pin 18)
   - If swapped, no data will appear
   - Look at PCB labels carefully (tiny print!)

3. **Power Not Connected**
   - GPS needs +5V power (or +3.3V with level shifter)
   - Check capacitor is connected
   - Verify no power drops (use multimeter: should read 5.0V at GPS)

4. **GPS Module Not Powered On**
   - Some breakout boards have enable/reset jumper
   - Check it's in "ON" position
   - Try cycling power (disconnect, wait 5s, reconnect)

### GPS Gets Lock, Then Loses It

**Symptoms**:
- Satellites show 8-10 for a few minutes
- Then drops to 0-2 satellites
- HDOP climbs from 1.0 to 10.0+

**Causes & Fixes**:

1. **Antenna Deteriorating Connection**
   - Check antenna connector (should be snug)
   - Try slightly twisting antenna connector
   - Test with different antenna if available

2. **Power Instability**
   - Capacitor not working (check voltage with multimeter)
   - Supply voltage dropping during high current draw
   - GPS needs stable 5.0V ±5% (4.75-5.25V)

3. **Electromagnetic Interference**
   - USB cables radiating RF noise
   - Computer monitor or WiFi router nearby
   - Power supply with noise
   - Fix: Move GPS antenna away from cables/RF sources

4. **Temperature Effects**
   - GPS may overheat and reduce sensitivity
   - Ensure good airflow around GPS module
   - Check if issue appears only after 10-15 minutes

### Position Jumps Around Randomly

**Symptoms**:
- Got lock (8+ satellites, HDOP < 2.0)
- But position jumps 10-50 meters between readings
- Happens even with good satellite count

**Likely Cause**: **Multipath error** (signals bouncing off nearby objects)

**Fixes**:
1. Move antenna away from metal objects (gutters, railings, car)
2. Use ground plane (metal plate under antenna)
3. Raise antenna higher
4. Use active antenna with better gain (25+ dBi)
5. Move to more open area if testing indoors

### HDOP Very High (> 5.0)

**Symptoms**:
- Got lock (satellites showing)
- But HDOP > 5.0 (poor geometry)
- Position accuracy marked "uncertain"

**Causes & Fixes**:

1. **Too Few Satellites**
   - Wait for more satellites to acquire (5 minutes typical)
   - Check antenna position (move to window/outdoors)

2. **Poor Satellite Geometry**
   - All satellites in one direction (bad geometry)
   - Can happen in urban canyon
   - Usually resolves in 5-10 minutes as satellites move

3. **Antenna Attenuation**
   - Metal structures blocking satellites
   - Indoors without clear window
   - Indoor foil-lined window film
   - Fix: Move antenna outdoors or to better window

---

## Verification Examples

### Expected Output Format (Production Mode)

```json
{
  "timestamp": 1234567890,
  "orientation": {
    "w": 0.707,
    "x": 0.0,
    "y": 0.0,
    "z": 0.707,
    "euler": {
      "roll_deg": 0.0,
      "pitch_deg": 0.0,
      "yaw_deg": 90.0
    }
  },
  "position": {
    "gps": {
      "latitude": 48.13745,
      "longitude": 11.58550,
      "altitude_m": 520.3,
      "hdop": 1.20,
      "vdop": 2.10,
      "satellites": 10,
      "fix_quality": 1,
      "locked": true
    },
    "local_ned": {
      "north_m": 22.5,
      "east_m": 15.3,
      "down_m": -0.5
    },
    "velocity_mps": 0.45
  }
}
```

### Performance Verification

Test your setup with this checklist:

| Metric | Excellent | Good | Acceptable | Poor |
|--------|-----------|------|-----------|------|
| Time to First Fix | < 30s | 30-60s | 1-3 min | > 5 min |
| Satellite Count | 12+ | 8-11 | 5-7 | < 4 |
| HDOP | < 1.0 | 1.0-2.0 | 2.0-5.0 | > 5.0 |
| Accuracy | ±1m | ±2-3m | ±5-10m | ±20m+ |
| Position Stability | < 0.5m drift | < 1m drift | < 5m drift | > 5m jumps |

---

## Advanced: Dual UART Configuration

If you want to use two GPS modules (advanced):

```cpp
// In src/config/gps_config.h, define GPS_UART_PORT during build:
// -D GPS_UART_PORT=0  → Uses Serial1 (pins 18/19)
// -D GPS_UART_PORT=1  → Uses Serial2 (pins 16/17)

GPS gps1;
GPS gps2;

void setup() {
  gps1.setSerialPort(&Serial1);
  gps1.begin(9600);
  
  gps2.setSerialPort(&Serial2);
  gps2.begin(115200);  // Different baud rate
}
```

---

## Related Documentation

- [GPS Driver API Reference](../reference/GPS_DRIVER_API_REFERENCE.md) - Using the GPS driver
- [GPS Troubleshooting](GPS_TROUBLESHOOTING.md) - Common software issues
- [Build Guide Phase 2](../build/BUILD_GUIDE_PHASE2.md) - Build instructions with GPS
- [PHASE_2_MASTER_IMPLEMENTATION_PLAN.md](../phases/PHASE_2_MASTER_IMPLEMENTATION_PLAN.md) - Full implementation guide

---

**Last Updated**: 2026-05-07  
**Version**: 1.0  
**Author**: Phase 2 Implementation  
**Status**: Complete and tested
