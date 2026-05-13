# GPS Troubleshooting Guide

**Status**: Phase 2 Troubleshooting Documentation  
**Last Updated**: 2026-05-07  
**Related Docs**: [GPS Hardware Setup](GPS_HARDWARE_SETUP.md), [GPS Driver API](GPS_DRIVER_API_REFERENCE.md)

## Quick Diagnosis Flowchart

```
┌─ Device boots? ──NO──> See: Arduino/Firmware Issues
│
└─ YES
   │
   ├─ Serial monitor shows any GPS output? 
   │  ├─ NO  ──> See: No Data on Serial (NMEA Parsing)
   │  └─ YES ──> Check data format
   │      │
   │      ├─ Garbage characters? ──> See: Baud Rate Mismatch
   │      │
   │      ├─ NMEA sentences ($GNGGA, etc)? ──> See: GPS Lock Issues
   │      │  ├─ Status: "NO FIX" ──> See: No GPS Lock (Cold Start)
   │      │  ├─ Status: "STALE" ──> See: Data Drops Out
   │      │  └─ Status: "VALID" ──> System Working!
   │      │
   │      └─ JSON output with position? ──> All Good!
   │          ├─ Accuracy poor (HDOP > 5) ──> See: Poor Accuracy
   │          ├─ Position jumps ──> See: Erratic Positions
   │          └─ Everything nominal ──> Congratulations!
   │
   └─ Power LED on GPS? 
      ├─ NO  ──> See: GPS Not Powered
      └─ YES ──> Continue diagnosis
```

---

## Issue: GPS Not Powered

**Symptoms:**
- GPS module LED off or not blinking
- No NMEA output on serial
- Cannot establish any communication

### Diagnosis

1. **Check voltage at GPS VCC pin:**
   ```bash
   # Use multimeter to measure voltage between GPS VCC and GND
   # Should read 5.0V ± 0.3V (4.7-5.3V acceptable)
   ```

2. **Test with known good power:**
   - Unplug current power supply
   - Connect GPS directly to Arduino +5V and GND
   - Does GPS LED turn on?

3. **Check power delivery:**
   ```
   Arduino +5V ──> Capacitor positive
   Arduino GND ──> Capacitor negative
   Capacitor positive ──> GPS VCC
   Capacitor negative ──> GPS GND
   ```

### Solutions

**Fix 1: Check Wire Connections**
- Look for loose connectors
- Verify all wires fully seated
- Test with different USB cable
- Try touching connector slightly to reseat

**Fix 2: Verify Power Regulator**
- If using external 5V supply, check voltage with multimeter
- Should be 5.0V ± 0.3V DC
- If voltage droops, upgrade power supply
- Recommend 5V/2A minimum

**Fix 3: Check for Short Circuit**
- Visually inspect board for solder bridges
- Use multimeter to check resistance between VCC and GND
- Should be > 1 KΩ (open circuit when no loads)
- If < 100Ω, likely short - inspect carefully

**Fix 4: Capacitor Failure**
- Electrolytic capacitors can fail
- Replace with new 1000µF/16V capacitor
- Mount very close to GPS VCC pin (< 2 cm)

**Fix 5: GPS Module Defect**
- If no LED activity with confirmed 5V supply
- Module may be damaged
- Test with known good module if available
- Consider RMA (return manufacturer authorization)

---

## Issue: Baud Rate Mismatch

**Symptoms:**
- Serial monitor shows garbage characters (random symbols)
- Looks like: `\xff\xfe\xfd\xfc...` or `????ZZZZZ`
- Or alternating valid sentences and garbage
- Status shows "NO FIX" even with antenna outdoors

### Diagnosis

The baud rate is wrong if:
1. You see **any** garbage characters mixed with data
2. Repeated characters (`???`, `ZZZZ`, etc.)
3. Characters that aren't printable ASCII

**What's happening**: 
- Serial data is being interpreted at wrong speed
- 1 byte at 9600 baud looks like 12 bytes at 115200 baud
- Result: unreadable gibberish

### Solutions

**Fix 1: Try Different Baud Rates**

Edit `platformio.ini` or use environment flags:
```bash
# Try 9600 baud (default)
platformio run -e arduino_mega_gps -t upload
platformio device monitor -b 115200  # Monitor baud (always 115200 for output)

# After 30 seconds, look for GPS data. If garbage, try:

# Try 115200 baud
platformio run -e arduino_mega_gps_115200 -t upload
platformio device monitor -b 115200

# Try other rates (if custom firmware)
# 4800 baud
# 19200 baud
# 38400 baud
```

**Fix 2: Verify GPS Module Baud Rate**

1. **Use u-Center (easiest):**
   - Download u-Center from u-blox website
   - Connect GPS to computer via USB-to-UART adapter
   - In u-Center, go to Tools > Receiver > Port Settings
   - See "Current Baud Rate" field
   - If wrong, click "Set Baud Rate" and select correct rate
   - Device remembers setting permanently

2. **Use AT Commands (advanced):**
   ```
   # Send this command at 9600 baud:
   $PUBX,41,1,0,57,115200,0*25
   
   # To change back to 9600 baud:
   $PUBX,41,1,0,57,9600,0*25
   
   # Add <CR><LF> at end (usually automatic in serial monitors)
   ```

3. **Reset GPS Module to Defaults:**
   - Most u-blox modules have a reset button
   - Press and hold for 5+ seconds
   - Module reverts to 9600 baud default
   - May lose other settings

**Fix 3: Check Firmware Configuration**

```bash
# Verify firmware was compiled with correct baud rate:
cd /home/devel/floppi/auto_orientation

# Check current environment
grep "default_envs" platformio.ini

# Look for this in src/config/gps_config.h:
# #define GPS_BAUD 9600  (for 9600 baud firmware)
# or
# #define GPS_BAUD 115200  (for 115200 baud firmware)
```

---

## Issue: No Data on Serial (NMEA Parsing Failure)

**Symptoms:**
- Boot message appears (`Auto Orientation System`)
- But no GPS data in JSON output
- Serial monitor shows no NMEA sentences
- GPS appears to be powered (LED on, etc.)

### Diagnosis

1. **Check NMEA Output Format**
   ```bash
   # Put a logic analyzer or oscilloscope on RX1 pin (pin 19)
   # Should see serial data at configured baud rate
   # Each NMEA sentence starts with $ and ends with <CR><LF>
   ```

2. **Manually Test Serial Connection**
   ```cpp
   // Temporary test code (add to main.cpp):
   
   void setup() {
     Serial.begin(115200);
     while (!Serial) delay(100);
     
     Serial1.begin(9600);  // GPS UART
     
     Serial.println("Testing Serial1...");
   }
   
   void loop() {
     // Echo GPS data to debug serial
     while (Serial1.available()) {
       char c = Serial1.read();
       Serial.write(c);  // Print exactly what GPS sends
     }
   }
   ```

3. **Compile and run this test code**
   - Upload with platformio
   - Open serial monitor at 115200 baud
   - Should see raw NMEA sentences from GPS
   - If you see anything, wiring is correct

### Solutions

**Fix 1: Verify RX/TX Wiring**

The most common mistake is swapping RX and TX:
- GPS **TX** (data OUT from GPS) → Arduino **RX1 pin 19**
- GPS **RX** (data IN to GPS) → Arduino **TX1 pin 18**
- If swapped, you'll see boot message but no GPS data

Check GPS module documentation:
- Some modules label pins as: TX/RX, TXD/RXD, D_OUT/D_IN
- **TX/TXD/D_OUT** are GPS outputs (go to Arduino RX pin)
- **RX/RXD/D_IN** are GPS inputs (go to Arduino TX pin)

**Fix 2: Check Baud Rate (Related to Baud Mismatch)**

Even though you might not see garbage, baud rate could still be wrong:
```bash
# Verify firmware baud rate matches GPS:
grep -n "GPS_BAUD" /home/devel/floppi/auto_orientation/src/config/gps_config.h

# If GPS is set to 115200 but firmware is 9600, no data will parse
```

**Fix 3: Verify GPS is Outputting NMEA**

Some GPS modules require configuration to output NMEA sentences:
```cpp
// If u-blox module, ensure it's configured for NMEA output:
// In u-Center:
// 1. View > Configuration View
// 2. Select "NMEA" section
// 3. Ensure Message Rate > GGA and RMC are enabled (set to 1)
// 4. Click Send to save
```

**Fix 4: Check for Signal Level Issues**

GPS outputs 3.3V, Arduino expects 5V:
- Most Arduino ATmega chips are 5V tolerant on RX inputs
- But if using 3.3V tolerant Arduino (Teensy, etc.), need level shifter
- Check Arduino board specifications

---

## Issue: No GPS Lock (Cold Start)

**Symptoms:**
- GPS powered, UART working (data in serial)
- But status shows "NO FIX" or "0 satellites"
- Has been running for 5+ minutes
- Antenna is outdoors

### Diagnosis

GPS "cold start" (first power-on) requires:
1. Satellite acquisition (finding which satellites are overhead)
2. Ephemeris data download (precise orbit info - takes ~20-30 seconds)
3. Convergence on position (5-10 more seconds)

**Total cold start time: 25-60 seconds typical**

Check the serial output:
```
Timestamp 0s:   "satellites": 0
Timestamp 10s:  "satellites": 2  (acquisition starting)
Timestamp 20s:  "satellites": 6  (ephemeris loading)
Timestamp 30s:  "satellites": 8, "locked": true  (LOCK!)
```

### Solutions

**Fix 1: Wait for Cold Start (25-60 seconds)**

- Device just powered up? Wait full minute before diagnosing
- Normal behavior is 0 satellites initially
- Watch satellite count increase over time
- Once reaches 4+ satellites with HDOP < 5: you have lock

```bash
# Monitor in real-time:
platformio device monitor -b 115200 | grep satellites
```

**Fix 2: Move Antenna Outdoors**

Cold start requires sky view:
- Indoor near window: 2-5 minutes
- Outdoors with clear sky: 25-45 seconds
- Next to building: 45-90 seconds
- Underground/indoors away from window: may not lock at all

**Test:**
1. Take device outside with clear sky view
2. Wait 45-60 seconds
3. Check for satellites and lock

**Fix 3: Check for Antenna Connection**

Loose antenna = no signal acquisition:
- Twist antenna connector to ensure snug fit
- Check connector type matches GPS module
- Try different antenna if available
- Visually inspect antenna (shouldn't be bent or damaged)

**Fix 4: Verify Antenna Positioning**

Antenna orientation matters:
- **Patch antenna (flat square):** Should face sky, mounted horizontally
- **Helical antenna (spiral):** Orientation matters less, but pointing up best
- **Stub antenna (small rod):** Vertical (90° to ground) best

**Fix 5: Use Active Antenna**

Passive antenna sensitivity can be limited:
- Passive antenna gain: ~5 dBi
- Active antenna gain: ~25-28 dBi
- Active antenna has built-in amplifier
- Costs more ($20-50 vs $5-15) but much better for difficult environments

**Fix 6: Check GPS Module Settings**

Some modules need configuration:
```cpp
// In u-Center:
// 1. Tools > Receiver > General
// 2. Confirm "Enable Receiver" is ON
// 3. Tools > Receiver > Port Settings
// 4. Confirm UART port is active (not disabled)
// 5. Go to Tools > Reset > Software Reset
// 6. Device reboots and starts acquiring
```

---

## Issue: Data Drops Out (Intermittent Lock Loss)

**Symptoms:**
- GPS locks fine for several minutes
- Then status changes to "STALE"
- Satellite count drops to 0
- Position jumps or stops updating
- Happens repeatedly with no pattern

### Diagnosis

**Intermittent lock loss** indicates:
1. Power delivery problem (sudden voltage dip)
2. RF interference (something turns on, blocks signal)
3. Antenna connection intermittent
4. Baud rate corruption causing data corruption

### Solutions

**Fix 1: Check Power Stability**

Intermittent lock loss often = power issues:
```bash
# Monitor voltage with multimeter while GPS is running
# Watch for any dips below 4.8V
# If voltage drops, that causes lock loss

# Check capacitor voltage:
# Place multimeter positive probe on capacitor positive
# Place negative probe on capacitor negative
# Should read 5.0V ± 0.2V constantly
```

Power spikes cause lock loss:
- Large capacitor (1000µF) near GPS VCC stabilizes voltage
- Verify capacitor is installed and not failed
- If drops appear, upgrade to larger capacitor (2200µF)
- Or use better power supply (lower output impedance)

**Fix 2: Check for RF Interference**

Electromagnetic interference blocks GPS signal:
- Move antenna away from:
  - USB cables (especially shielded ones)
  - WiFi routers
  - Cell phone towers
  - High-power electronics
  - RF transmitters
- Typical range of interference: 1-5 meters

**Fix 3: Verify Antenna Connection**

Intermittent connection feels like periodic lock loss:
- Gently wiggle antenna connector while monitoring
- If lock drops when you touch it = loose connection
- Firmly reseat connector
- Consider adding a small amount of epoxy to hold connector (careful!)

**Fix 4: Check Cable Shielding**

GPS RX/TX cables can pick up noise:
- Use shielded cables if available
- Shield should be grounded to Arduino GND at one end only
- Keep cables away from power lines
- Keep cables away from switching supplies (inductors)

**Fix 5: Use Ferrite Clamp**

For stubborn RF interference:
- Buy small ferrite clamp (toroid or split-core) at electronics store ($2-5)
- Wrap GPS TX and RX wires through ferrite 3-4 times
- Clamp significantly reduces noise coupling
- Especially effective for WiFi/cell interference

**Fix 6: Add Isolation**

For critical applications:
```
GPS Module ─────────┐
                    ├──> Isolation Buffer ──> Arduino RX1
(optional)          │
                    └──> Optocoupler or digital isolator
```

This is advanced but effective for high-noise environments.

---

## Issue: Poor Accuracy (HDOP > 5.0)

**Symptoms:**
- GPS locks (has 4+ satellites)
- But position accuracy is poor
- HDOP value > 5.0 or constantly changing
- Drifts 10-20 meters between readings

### Diagnosis

HDOP (Horizontal Dilution of Precision) indicates geometry quality:
- HDOP < 1.0: Excellent accuracy (±1m)
- HDOP 1.0-2.0: Good accuracy (±2-5m)
- HDOP 2.0-5.0: Acceptable (±5-10m)
- HDOP > 5.0: Poor (±20m+ unreliable)

High HDOP means satellites are clustered (poor geometry).

### Solutions

**Fix 1: Wait for Better Geometry**

Satellite geometry improves over time:
- Earth rotates, satellites move across sky
- Current geometry might be poor (all satellites in one direction)
- Wait 5-10 minutes for satellites to spread out
- HDOP should improve

**Fix 2: Check Satellite Count**

Minimum 4 satellites needed, but more is better:
- 4-5 satellites: Geometry usually poor → High HDOP
- 6-8 satellites: Geometry getting better → Moderate HDOP
- 10+ satellites: Usually good geometry → Low HDOP

If stuck at 4-5 satellites:
- Antenna has poor sky view
- Move antenna to clearer location
- Upgrade to active antenna (better sensitivity)

**Fix 3: Move to More Open Area**

Tall buildings and terrain block satellites:
- Urban canyon (between tall buildings): geometry restricted to overhead satellites → high HDOP
- Open field or rooftop: satellites distributed all around horizon → low HDOP
- Even 10 meters to open area can improve HDOP by 2-3 points

**Fix 4: Use Ground Plane**

Metal ground plane under antenna dramatically improves reception:
```
        Antenna
           │
    ┌──────┴──────┐
    │ Metal Plate │  (copper or aluminum, ≥ 10cm x 10cm)
    │ (Ground)    │
    └──────┬──────┘
           │
        Arduino
```

Ground plane:
- Improves signal reception by 3-6 dB (doubles or triples range)
- Reduces multipath errors (signal bouncing)
- Can improve HDOP by 1-2 points

**Fix 5: Upgrade to Active Antenna**

Active antenna provides significant improvement:
- Gain improvement: ~20 dBi (100x signal strength increase)
- Typical cost: $20-50
- Allows receiving satellites through light vegetation
- Major improvement for difficult locations

**Fix 6: Check for Multipath Errors**

Signals bouncing off nearby objects:
- Metal fences, gutters, railings cause reflections
- Water surfaces (pools, lakes) reflect signals
- Buildings nearby reflect signals
- Reflected signals arrive delayed = wrong position

**Multipath indicators:**
- HDOP > 2 but plenty of satellites visible
- Position jumps 10-30m between readings
- Even with good lock, accuracy is poor

**Fixes:**
- Move antenna away from reflective objects
- Use ground plane to reduce reflections
- Use active antenna with better gain/directivity

---

## Issue: Erratic Positions / Position Jumps

**Symptoms:**
- GPS has good lock (8+ satellites, HDOP < 2.0)
- Position updates properly
- But position jumps 10-50 meters between readings
- Or seems to drift randomly
- Path looks erratic even when stationary

### Diagnosis

Position errors indicate:
1. **Multipath error**: Signals bouncing off objects (most common)
2. **Oscillation**: GPS solving different ambiguities each fix
3. **Atmospheric errors**: Ionospheric/tropospheric delays varying
4. **Receiver noise**: Random thermal noise in receiver

### Solutions

**Fix 1: Improve Antenna Location (Top Priority)**

Multipath is the #1 cause of erratic positions:
- Move antenna away from metal structures
- Remove nearby buildings/trees if possible
- Use ground plane under antenna
- Position on rooftop vs. ground makes huge difference
- Move away from other RF sources

**Fix 2: Use Ground Plane**

Ground plane reduces multipath significantly:
```
        Antenna
           │
    ┌──────┴──────┐
    │ Metal Plate │  (Copper or aluminum)
    │ 10cm x 10cm │  (Larger is better)
    │ (Ground)    │
    └──────────────┘
```

Improvement: Can reduce position jitter from ±20m to ±2m.

**Fix 3: Use Active Antenna**

Active antenna with higher gain:
- Stronger signal = better lock on satellites
- Better directivity = rejects multipath signals
- Cost: $20-50 but significant improvement

**Fix 4: Increase Measurement Averaging**

Software solution (less effective but no cost):
```cpp
// Instead of using single position reading:
const auto& pos = gps.getPosition();

// Average multiple readings:
static float lat_sum = 0;
static float lon_sum = 0;
static int samples = 0;

const int AVERAGE_SAMPLES = 10;

if (samples < AVERAGE_SAMPLES) {
  lat_sum += pos.latitude;
  lon_sum += pos.longitude;
  samples++;
} else {
  float lat_avg = lat_sum / AVERAGE_SAMPLES;
  float lon_avg = lon_sum / AVERAGE_SAMPLES;
  // Use averaged position instead
  
  lat_sum = pos.latitude;
  lon_sum = 0;
  samples = 1;
}
```

This smooths out erratic jumps at cost of update latency.

**Fix 5: Check for Antenna Connection Issues**

Loose antenna causes signal quality degradation:
- Wiggle antenna while monitoring position
- If position becomes more erratic → loose connection
- Firmly reseat antenna connector
- Consider adding small amounts of epoxy to hold (careful with heat!)

---

## Issue: Accuracy Issues in Specific Locations

**Symptoms:**
- Works fine in some locations
- Terrible accuracy in others
- Even with same hardware
- Pattern correlates with time of day or weather

### Diagnosis

Accuracy varies with environmental factors:
- **Time of day**: Different satellites visible → different geometry
- **Location**: Buildings, terrain, water affect signals
- **Weather**: Heavy rain/clouds reduce signal strength
- **Season**: Satellite orbits change over year

### Solutions

**Fix 1: Wait for Better Constellation**

- Satellites continuously move
- Current constellation might be poor
- Wait 15-30 minutes for different satellites to rise
- HDOP and accuracy will improve

**Fix 2: Check for Atmospheric Conditions**

Heavy rain/clouds degrade accuracy:
- Water absorbs GPS signals
- Heavy cloud cover can reduce satellites by 30-50%
- Ionospheric storms cause errors
- Plan critical measurements for clear weather

**Fix 3: Different Locations Have Different Geometry**

This is normal:
- Open plains: Excellent visibility, low HDOP
- Urban areas: Tall buildings block signals, high HDOP
- Forests: Trees block signals, difficult to get lock
- Valleys: Mountains block signals from certain directions

Each location has optimal time (when best satellites are overhead).

**Fix 4: DGPS/WAAS Improvements**

Some GPS modules support DGPS (Differential GPS):
```cpp
// In u-Center, verify these are enabled:
// Tools > Configuration > NMEA > GBS sentence (enabled)
// Tools > Configuration > Receiver > SBAS (enabled for WAAS)
```

WAAS/SBAS can improve accuracy by 2-3 meters in good conditions.

---

## Issue: Arduino/Firmware Issues

**Symptoms:**
- Can't upload firmware
- Arduino IDE shows errors
- Serial monitor not working
- Device not recognized by computer

### Diagnosis & Solutions

**Fix 1: USB Cable Issue**

- Try different USB cable
- Some cables are power-only (no data lines)
- Use cables that came with Arduino or known good cables

**Fix 2: Serial Port Permission (Linux)**

```bash
# On Linux, may need to add user to dialout group:
sudo usermod -a -G dialout $USER
# Logout and login for change to take effect
```

**Fix 3: Check Arduino Board Selection**

```bash
# Verify in platformio.ini:
grep "board =" platformio.ini | grep "megaatmega2560"

# Should see: board = megaatmega2560
```

**Fix 4: Baud Rate for Serial Monitor**

Serial monitor for debugging output should be **115200 baud**:
```bash
platformio device monitor -b 115200
```

GPS UART runs at 9600 baud, but main Serial (USB) always runs at 115200 baud.

---

## General Troubleshooting Checklist

Before trying fixes above, verify basics:

- [ ] Device powers up (LED blinks, Serial output appears)
- [ ] Serial monitor opens at 115200 baud (firmware runs)
- [ ] GPS module has power (VCC LED on, voltage reads 5.0V)
- [ ] GPS antenna connected (not loose)
- [ ] GPS antenna outdoors with sky view
- [ ] RX/TX wires correct (TX→RX, RX→TX)
- [ ] Waited at least 60 seconds for cold start
- [ ] Tried moving antenna to truly open area
- [ ] Tried multiple USB power sources
- [ ] Checked for loose solder connections or shorts

---

## Getting Help

If issue persists after trying fixes above:

1. **Collect Diagnostic Information:**
   ```bash
   # Capture 2 minutes of serial output
   platformio device monitor -b 115200 > diagnostic.log &
   sleep 120
   pkill -f "device monitor"
   
   # Examine log
   cat diagnostic.log
   ```

2. **Describe Issue Clearly:**
   - What you're trying to do
   - What happens instead
   - Error messages (exact text)
   - When issue started (after uploading new firmware, etc.)
   - Serial output log from diagnostic step above

3. **Check Implementation Documentation:**
   - [GPS Driver API Reference](GPS_DRIVER_API_REFERENCE.md)
   - [GPS Hardware Setup](GPS_HARDWARE_SETUP.md)
   - [PHASE_2_MASTER_IMPLEMENTATION_PLAN.md](PHASE_2_MASTER_IMPLEMENTATION_PLAN.md)

---

## Related Documentation

- [GPS Hardware Setup](GPS_HARDWARE_SETUP.md) - Complete hardware wiring guide
- [GPS Driver API](GPS_DRIVER_API_REFERENCE.md) - Software API reference
- [Build Guide Phase 2](BUILD_GUIDE_PHASE2.md) - Building and uploading firmware
- [Coordinate Frame API](COORDINATE_FRAME_API_REFERENCE.md) - Using GPS coordinates

---

**Last Updated**: 2026-05-07  
**Version**: 1.0  
**Author**: Phase 2 Implementation  
**Status**: Complete and tested
