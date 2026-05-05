# Field Deployment: Getting Into the Field

Practical guidance for deploying your Auto Orientation system in the field.

---

## Pre-Deployment Checklist

### System Verification (Indoors)

Before leaving the lab, verify everything works:

- [ ] **Hardware tests**
  - [ ] All sensors responsive (BNO085 and GPS)
  - [ ] Calibration at "High" status
  - [ ] Serial monitor shows clean data
  - [ ] No error messages

- [ ] **Orientation accuracy**
  - [ ] Yaw values stable (not drifting)
  - [ ] Roll/pitch respond correctly to tilting
  - [ ] Heading matches compass (±5°)

- [ ] **Software**
  - [ ] Firmware compiled and uploaded
  - [ ] Python monitor installed and working
  - [ ] Logging script ready if needed
  - [ ] Backup copy of code on USB

- [ ] **Power verification**
  - [ ] Device runs for expected duration
  - [ ] No brownouts or resets
  - [ ] Battery fully charged (if using battery)
  - [ ] Have spare battery/power supply

- [ ] **Data storage**
  - [ ] Know where logs will be saved
  - [ ] Have USB drive for data backup
  - [ ] Have enough disk space (rule of thumb: ~50KB per minute)

---

## Packing Checklist

### Hardware to Bring

- [ ] **Arduino Mega + Sensors**
  - [ ] Arduino Mega 2560 (in protective case)
  - [ ] BNO085 IMU sensor
  - [ ] Ublox NEO-M9N GPS receiver
  - [ ] GPS antenna (remove after packing, reattach at site)

- [ ] **Power**
  - [ ] USB power bank (5V, 2A+ recommended) or wall adapter
  - [ ] USB-B cable for Arduino power (or barrel jack supply)
  - [ ] USB-B cable for GPS (if using USB connection)
  - [ ] Spare batteries (2-3 extra)
  - [ ] Power extension cord or multi-outlet adapter

- [ ] **Cables & Connectors**
  - [ ] UART cables for BNO085 (if not soldered)
  - [ ] USB cables (extra, different types)
  - [ ] Cable ties or velcro straps
  - [ ] Barrel jack adapter (if applicable)
  - [ ] USB hub (if powering multiple devices)

- [ ] **Tools & Spare Parts**
  - [ ] Multimeter (for troubleshooting voltage)
  - [ ] Small screwdriver set
  - [ ] Soldering iron (if connections come loose)
  - [ ] Solder and flux
  - [ ] Spare resistors and capacitors
  - [ ] Crimpers and wire (for quick repairs)

### Software & Data Storage

- [ ] **Laptop/Computer**
  - [ ] Laptop with Python installed
  - [ ] USB-C or USB-A cable for Arduino connection
  - [ ] Clone of code repository (or USB backup)
  - [ ] Monitor script (real_time_monitor.py)

- [ ] **Data Storage**
  - [ ] USB drive (for backup data)
  - [ ] Cloud sync enabled (e.g., Google Drive, Dropbox)
  - [ ] Local disk space available (check `df -h`)

### Documentation to Bring

- [ ] **Guides**
  - [ ] This deployment guide (printed or on phone/laptop)
  - [ ] Hardware Setup guide (troubleshooting reference)
  - [ ] Calibration guide (in case recalibration needed)
  - [ ] Troubleshooting guide (for common issues)

- [ ] **Reference Data**
  - [ ] Pre-deployment calibration status
  - [ ] Known good baseline values
  - [ ] GPS coordinates of test site
  - [ ] Contact info for support

### Miscellaneous

- [ ] **Site Logistics**
  - [ ] Weather forecast (waterproofing if needed)
  - [ ] Site permissions/access info
  - [ ] Maps or directions to site
  - [ ] Contact info for on-site coordinator

- [ ] **Safety**
  - [ ] First aid kit (for yourself)
  - [ ] Sun protection (sunscreen, hat)
  - [ ] Bug spray or insect repellent
  - [ ] Water bottle
  - [ ] Headlamp or flashlight

---

## Location Selection

### Optimal GPS Conditions

**GPS needs clear sky view. Choose locations with:**

- [ ] **Open sky exposure**
  - [ ] 5+ degrees above horizon to all directions
  - [ ] Avoid buildings, dense trees, bridges overhead
  - [ ] Rooftops better than street level
  - [ ] Open fields/parking lots best
  - [ ] Example: Wide-open field with no buildings ✓
  - [ ] Example: GPS antenna pointing at sky ✓

- [ ] **Minimal obstructions**
  - [ ] No dense foliage directly overhead
  - [ ] Not under power lines (RF interference)
  - [ ] Away from cell towers (RF interference)
  - [ ] Away from metal structures/fences

- [ ] **Favorable geography**
  - [ ] Higher elevation better (less obstruction)
  - [ ] Flat terrain easier to work on
  - [ ] Not in valley (hills block signals)
  - [ ] Away from water (reflections interfere)

- [ ] **Electromagnetic quiet**
  - [ ] Away from WiFi routers
  - [ ] Away from radar antennas
  - [ ] Away from microwave ovens (if indoors)
  - [ ] Away from active power transmission lines

**Visual checks:**

```
Good location:
        Sky (clear and open)
        ^
        |
    □   |   □     ← Antenna vertical
  □   ▲   □       ← Device at location
 □   ▲ ▲   □      ← Open horizon
─────────────────  ← Ground level

Bad location:
        Buildings/trees (obstructing)
        ▐ ▌ ▐ ▌
        ▐ ▌ ▐ ▌    ← Obstacles block sky
    ▌   ▌   ▌
  ▌ ▲ ▌     ▌     ← Limited sky view
 ▌   ▲ ▲     ▌
─────────────────
```

### Magnetic Conditions

**For accurate heading (yaw):**

- [ ] **Minimize local magnetic interference**
  - [ ] Move away from large metal objects
  - [ ] Avoid vehicles parked nearby
  - [ ] Don't place device on metal surface
  - [ ] Metal benches, railings can cause 10-30° error

- [ ] **Avoid anomalous locations**
  - [ ] Some areas have geological magnetic anomalies
  - [ ] Usually not a problem unless doing precision work
  - [ ] If heading seems off: compare to compass or map

- [ ] **Environmental factors**
  - [ ] Proximity to powerlines (some interference)
  - [ ] Avoid active construction sites
  - [ ] Stay clear of overhead power lines

---

## Setting Up at Site

### Initial Setup (5-10 minutes)

1. **Secure the hardware**:
   - [ ] Place Arduino on stable, non-metal surface
   - [ ] Mount antenna vertically (or nearly vertical)
   - [ ] Ensure antenna has clear sky view
   - [ ] Keep BNO085 away from metal objects

2. **Connect power**:
   - [ ] Connect 5V power supply
   - [ ] Verify LED lights up
   - [ ] Check for voltage with multimeter (4.8-5.2V)

3. **Start monitoring**:
   - [ ] Connect laptop to Arduino USB
   - [ ] Launch monitor: `python3 tools/real_time_monitor.py /dev/ttyACM0`
   - [ ] Verify data is flowing
   - [ ] Check calibration status (should show "High" from indoors)

4. **Initial verification**:
   - [ ] Orientation values appear reasonable
   - [ ] Data rate shows ~100 Hz
   - [ ] No error messages
   - [ ] GPS status (if outdoors with clear sky)

### Waiting for GPS Lock

**If using GPS:**

| Time | Status | What to Do |
|------|--------|-----------|
| 0-30 sec | "Acquiring" | Wait, sensor finding satellites |
| 30-60 sec | "Acquiring" | Still normal, especially first lock |
| 60-120 sec | "No Fix" ❌ | Check antenna position/sky view |
| >120 sec | "No Fix" ❌ | Move to different location |

**Speed up GPS acquisition:**

1. Move to location with absolutely clear sky
2. Keep antenna vertical and pointing up
3. Wait full 2 minutes before giving up
4. Try different spot if no lock after 5 minutes

**GPS Lock indicators:**
```
GPS Status:
  □ No satellite signals yet (Acquiring ░)
  █ Good fix available (Fixed █)
  
Fix Type:
  2D Fix: Latitude/longitude only (less accurate)
  3D Fix: Latitude/longitude + altitude (full accuracy)
  
Accuracy:
  CEP: 1-3 m = excellent
  CEP: 3-5 m = good
  CEP: >5 m = degraded, move to clearer location
```

### Testing the Site

Before collecting long-term data, verify the location:

1. **Orientation test**:
   - [ ] Rotate device 90° clockwise
   - [ ] Verify yaw changes by ~90° (e.g., 0° → 90°)
   - [ ] Rotate back and verify yaw returns
   - [ ] Indicates good calibration and no interference

2. **GPS test** (if deployed):
   - [ ] Wait for "Fixed" status
   - [ ] Record position (lat/lon/alt)
   - [ ] Walk 10 meters north
   - [ ] Verify latitude increased slightly
   - [ ] Return to original position
   - [ ] Verify position returns

3. **Data quality test**:
   - [ ] Monitor for 1-2 minutes
   - [ ] Check for jumps or dropouts
   - [ ] Verify error count stays at 0
   - [ ] Confirm data rate stable at ~100 Hz

4. **Environmental check**:
   - [ ] No sudden changes in yaw
   - [ ] GPS accuracy steady (CEP value)
   - [ ] Signal strength adequate (if displayed)
   - [ ] Device not overheating

---

## Power Management for Extended Deployment

### Battery Duration Calculation

**Current draw:**
- BNO085 IMU: 50 mA
- NEO-M9N GPS: 150 mA
- Arduino: 100 mA
- **Total: ~300 mA**

**Battery life:**
```
Battery capacity / Current draw = Hours

Example:
5000 mAh / 300 mA = 16.7 hours

Common batteries:
- USB power bank (5000 mAh): ~16 hours
- USB power bank (10000 mAh): ~33 hours
- Alkaline AA batteries (2000 mAh): ~6 hours
```

### Power Optimization

To extend battery life:

1. **Reduce sampling rate** (if possible via firmware):
   - Lower IMU update rate from 100 Hz to 50 Hz = 25% power reduction
   - Reduce GPS update rate from 1 Hz to 0.1 Hz = 20% power reduction

2. **Minimize USB overhead**:
   - Use direct UART for GPS if USB available
   - Use USB only for monitoring, not data logging

3. **Disable unnecessary sensors**:
   - If only orientation needed, disable GPS output
   - Saves ~150 mA (50% reduction)

4. **Use efficient power supply**:
   - Regulated DC supply better than USB (efficiency)
   - Removes USB overhead

### Monitoring Power

During deployment:

```bash
# Monitor continuously in background
python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet --log field_data.jsonl &

# Check periodically
tail -f field_data.jsonl

# Monitor shows uptime
Uptime: 00:23:45  # Device has been running 23 min 45 sec
```

### Emergency Power Management

If battery getting low:

1. **Stop non-essential tasks**:
   - Stop Python monitoring (saves USB power)
   - Close applications on laptop

2. **Reduce sensor load**:
   - Switch to orientation-only mode
   - Disable GPS if not needed

3. **Deploy backup**:
   - Connect to wall outlet if available
   - Use backup battery

4. **Graceful shutdown**:
   - Save all data to disk/USB
   - Power off cleanly
   - Avoid data corruption

---

## Field Troubleshooting

### GPS Still Won't Lock After 5 Minutes

**Symptoms**: Shows "Acquiring" or "No Fix" indefinitely

**Quick fixes**:

1. **Check antenna**:
   - Is it firmly screwed to NEO-M9N?
   - Is it vertical or nearly vertical?
   - Any metal or water nearby?

2. **Move to different location**:
   ```
   Bad location:
   - Under trees
   - Near buildings
   - In valley
   
   Good location:
   - Open field
   - Rooftop (if safe)
   - Parking lot
   ```

3. **Wait longer**:
   - Initial (cold start): 60-120 seconds
   - Typical (warm start): 10-30 seconds
   - Be patient first time at new location

4. **Check power**:
   - Verify 5V power to GPS module
   - Use multimeter if unsure
   - GPS is power-hungry (150 mA)

5. **Try factory reset**:
   - Power off for 10 seconds
   - Power back on (fresh acquisition)

### Yaw Values Erratic or Drifting

**Symptoms**: Yaw changes without rotating device

**Quick fixes**:

1. **Check for magnetic interference**:
   - Move away from metal objects
   - Move away from vehicles
   - Leave metal benches/tables

2. **Recalibrate**:
   - If indoors and suddenly degraded
   - Perform figure-8 motion for 60 seconds
   - Look for "High" status

3. **Verify antenna placement**:
   - GPS can contribute to magnetic anomalies
   - Try moving antenna farther from BNO085
   - Ensure antenna is vertical

4. **Check power stability**:
   - Unstable power can cause sensor errors
   - Use multimeter to check 5V rail
   - Try different power supply

### Serial Connection Drops

**Symptoms**: Monitor stops receiving data, then reconnects

**Quick fixes**:

1. **Check USB cable**:
   - Try different USB cable
   - Inspect for visible damage
   - Reseat connections firmly

2. **Power cycling**:
   - Power off device for 5 seconds
   - Power back on
   - Restart monitor: `python3 tools/real_time_monitor.py /dev/ttyACM0`

3. **Port configuration**:
   - Confirm port is correct
   - Try: `ls /dev/tty*` (Linux/Mac) or Device Manager (Windows)

4. **Reduce monitor update rate**:
   - Add `--baud 9600` if using long cables
   - Reduces noise/interference

### Data Logging Failures

**Symptoms**: Monitor runs but file not created or incomplete

**Quick fixes**:

1. **Check disk space**:
   ```bash
   df -h  # Linux/Mac
   ```
   Need at least 100 MB free

2. **Verify file path**:
   ```bash
   # Create specific directory
   mkdir -p ~/field_data
   python3 tools/real_time_monitor.py /dev/ttyACM0 --log ~/field_data/test.jsonl
   ```

3. **Check permissions**:
   ```bash
   # Ensure write permissions
   chmod u+w /path/to/logdir
   ```

4. **Backup periodically**:
   ```bash
   # Copy log file to USB drive
   cp field_data.jsonl /mnt/usb_drive/
   ```

---

## After Deployment: Data Recovery

### Retrieving Data

1. **Copy local log files**:
   ```bash
   # USB drive
   cp sensor_data.jsonl /mnt/usb_drive/
   
   # Cloud sync (Dropbox, Google Drive, etc.)
   cp sensor_data.jsonl ~/Dropbox/field_data/
   ```

2. **Sync to cloud** (if internet available):
   ```bash
   # Assuming Google Drive mounted
   cp field_data.jsonl /mnt/gdrive/auto_orientation/
   ```

3. **Backup multiple copies**:
   - USB drive (portable)
   - Cloud (redundant, accessible anywhere)
   - Local disk (original location)

### Data Validation

After retrieving data, verify integrity:

```python
import json

# Check file is valid JSONL
with open('field_data.jsonl', 'r') as f:
    count = 0
    errors = 0
    for line in f:
        try:
            json.loads(line)
            count += 1
        except:
            errors += 1

print(f"Records: {count}, Errors: {errors}")
if errors == 0:
    print("✓ File is valid")
else:
    print("✗ File has corrupted lines")
```

### Analysis

Now you can analyze the data:

```bash
# Convert to CSV
python3 << 'EOF'
import json
import csv

with open('field_data.jsonl', 'r') as f:
    data = [json.loads(line) for line in f]

with open('field_data.csv', 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=data[0].keys())
    writer.writeheader()
    writer.writerows(data)

print(f"Exported {len(data)} rows")
EOF

# Open in Excel/Sheets for analysis
```

---

## Field Deployment Checklist

### Before Departure (30 minutes before)

- [ ] All hardware packed and checked
- [ ] Batteries fully charged
- [ ] Code uploaded to Arduino
- [ ] Monitor scripts verified working
- [ ] Backup code on USB drive
- [ ] Documentation printed or on phone
- [ ] Route/directions confirmed
- [ ] Weather checked (waterproofing if needed)
- [ ] Permissions confirmed for site access

### At Site (Upon Arrival)

- [ ] Scout location for optimal GPS/magnetic conditions
- [ ] Set up hardware on non-metal surface
- [ ] Attach antenna and orient vertically
- [ ] Connect power and verify LED
- [ ] Launch monitor and verify data flowing
- [ ] Test GPS lock (if applicable)
- [ ] Run 5-minute system verification
- [ ] Start data logging if needed

### During Operation

- [ ] Monitor uptime (every 30 minutes)
- [ ] Check for data anomalies
- [ ] Verify error count stays low
- [ ] Backup data mid-session if >1 hour
- [ ] Monitor battery level
- [ ] Check for physical damage/wind/weather issues

### End of Session

- [ ] Power off device cleanly
- [ ] Disconnect antenna (to prevent damage)
- [ ] Disconnect all cables
- [ ] Back up all data files
- [ ] Copy logs to USB and cloud
- [ ] Pack equipment carefully
- [ ] Document any issues encountered

---

## Safety Considerations

- **Outdoor work**: Bring water, sun protection, weather-appropriate clothing
- **Site access**: Confirm permissions before setting up
- **GPS antenna**: Keep away from eyes when pointing upward
- **Power**: Use surge protection if plugging into wall outlets
- **Electronics**: Protect from rain/moisture with waterproof case if needed
- **Cables**: Keep clear of trip hazards, use cable ties
- **EMI**: Avoid standing near active RF sources (radar, cell towers)

---

## Common Scenarios

### Scenario 1: Single 30-Minute Test

```bash
# Quick verification before going home

1. Set up at site (5 min)
2. Launch monitor (1 min)
3. Wait for GPS lock (1-2 min)
4. Run test rotations (2 min)
5. Verify all systems (1 min)
6. Pack up (5 min)

Total: ~15-20 minutes
No logging needed unless collecting data for later
```

### Scenario 2: Multi-Hour Field Trial

```bash
# Extended data collection during field campaign

1. Set up hardware (10 min)
2. Verify GPS and calibration (5 min)
3. Start logging: python3 tools/real_time_monitor.py /dev/ttyACM0 --log trial_001.jsonl
4. Monitor periodically (check every 30 min)
5. Backup mid-session (after 2 hours)
6. Run 30-minute test interval (repeat 3-4x)
7. Stop logging cleanly
8. Pack and backup data

Total: 3-4 hours with ~500 MB data
```

### Scenario 3: Multi-Day Campaign

```bash
# Extended deployment with daily monitoring

Each day:
1. Morning: Verify hardware, start logging
2. Midday: Quick health check
3. Evening: Power down, backup data
4. Back up to cloud immediately after

Schedule:
- Day 1: Cold start (slow GPS), full calibration check
- Day 2+: Faster GPS lock (warm start)

Storage: ~1 GB per day of continuous logging
```

---

## Quick Reference

### Essential Commands

```bash
# Launch monitor
python3 tools/real_time_monitor.py /dev/ttyACM0

# Monitor with logging
python3 tools/real_time_monitor.py /dev/ttyACM0 --log field_data.jsonl

# Find serial port (if unsure)
ls /dev/tty*  # Linux/Mac

# Backup data
cp field_data.jsonl /mnt/backup/
```

### Expected Performance

```
Orientation:
  Accuracy:     ±2-5° under normal conditions
  Stability:    <1° when stationary (with calibration)
  Update rate:  ~100 Hz
  Calibration:  Persistent (survives power cycles)

Position (GPS):
  Accuracy:     1-5 meters (CEP)
  Acquisition:  60-120 sec (cold start), 10-30 sec (warm start)
  Update rate:  1-10 Hz (depends on configuration)
  Requires:     Clear sky view

Power:
  Total draw:   ~300 mA
  Battery life: ~16 hours on 5000 mAh USB bank
```

---

**Last Updated**: 2025-05  
**Difficulty**: Intermediate  
**Time Required**: 30+ minutes (first deployment)
