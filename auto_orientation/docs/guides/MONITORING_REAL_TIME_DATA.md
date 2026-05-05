# Real-Time Monitoring: Understanding Your Data

Learn how to monitor, record, and analyze real-time sensor data from your Auto Orientation system.

---

## Quick Start: Launch the Monitor

```bash
cd /path/to/auto_orientation

# Start real-time display
python3 tools/real_time_monitor.py /dev/ttyACM0
```

You should see:
```
╔════════════════════════════════════════════════════════════╗
║         Auto Orientation Real-Time Monitor               ║
╠════════════════════════════════════════════════════════════╣

Orientation:
  Roll:    2.3°  (rotation around front-to-back axis)
  Pitch:  -5.4°  (rotation around left-to-right axis)
  Yaw:   247.8°  (rotation around vertical axis/heading)

Position:
  Latitude:  37.4419°
  Longitude: -122.1430°
  Altitude:  150.5 m
  Accuracy:  2.3 m (CEP)

Status:
  BNO085 Cal: High ███
  GPS:        Acquiring ░ Fix: No
  Uptime:     00:02:34

Data Rate: 102 Hz
Packets:   12,456  Errors: 0
```

---

## Understanding the Display

### Orientation Section

**Roll** (rotation around front-to-back axis):
- Range: -180° to +180°
- **0°**: Level side-to-side
- **90°**: Tilted 90° to the right
- **-90°**: Tilted 90° to the left
- **Example**: Leaning left/right

**Pitch** (rotation around left-to-right axis):
- Range: -90° to +90°
- **0°**: Level front-to-back
- **90°**: Tilted all the way back
- **-90°**: Tilted all the way forward
- **Example**: Tilting forward/backward

**Yaw** (rotation around vertical axis / heading):
- Range: 0° to 360° (or -180° to +180°)
- **0°/360°**: Pointing magnetic north
- **90°**: Pointing magnetic east
- **180°**: Pointing magnetic south
- **270°**: Pointing magnetic west
- **Example**: Direction you're facing

**Visual Reference:**
```
         North (0°)
           ↑
West ← ← ← → → → East
       ↙ ↖
    ↙ (315°) ↖
   ↙         ↖ (45°)
South (180°)

Common Yaw Values:
  - Facing north: 0-10°
  - Facing east: 80-100°
  - Facing south: 170-190°
  - Facing west: 260-280°
```

### Position Section

**Latitude** (North-South position):
- Positive values: North of equator
- Negative values: South of equator
- **37.4419°**: ~37 degrees north
- **Change of 0.01°** ≈ 1.1 km north/south

**Longitude** (East-West position):
- Positive values: East of Prime Meridian
- Negative values: West of Prime Meridian
- **-122.1430°**: ~122 degrees west
- **Change of 0.01°** ≈ 0.9 km east/west (at equator)

**Altitude** (height above sea level):
- In meters
- **150.5 m**: 150.5 meters above sea level
- **Note**: GPS altitude is less accurate than position, typically ±5-10m

**Accuracy (CEP)** (Circular Error Probable):
- **2.3 m**: 68% confidence that true position is within 2.3 m circle
- **>5 m**: GPS signal is degraded, move to clearer location
- **<1 m**: Excellent GPS accuracy, likely averaged or differential GPS

---

## How to Run the Monitor

### Basic Usage

```bash
# Monitor on default serial port and baud rate
python3 tools/real_time_monitor.py /dev/ttyACM0
```

### With Custom Baud Rate

```bash
# If default 115200 doesn't work
python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 9600
```

### With Data Logging

**Record data to a file for analysis:**

```bash
# Log to JSON lines format (one JSON object per line)
python3 tools/real_time_monitor.py /dev/ttyACM0 --log sensor_data.jsonl

# Monitor runs and saves all data simultaneously
# Ctrl+C to stop
```

**Output file format** (`sensor_data.jsonl`):
```json
{"timestamp": 1234567890.123, "roll": 2.3, "pitch": -5.4, "yaw": 247.8, "lat": 37.4419, "lon": -122.1430, "alt": 150.5, "cep": 2.3}
{"timestamp": 1234567890.124, "roll": 2.4, "pitch": -5.3, "yaw": 247.9, "lat": 37.4419, "lon": -122.1430, "alt": 150.5, "cep": 2.3}
```

### Without Color (Simple Terminals)

```bash
# For terminals without ANSI color support
python3 tools/real_time_monitor.py /dev/ttyACM0 --no-color
```

### Quiet Mode (Background Monitoring)

```bash
# Suppress display output, just log
python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl
```

### Verbose/Debug Mode

```bash
# Show all messages, JSON details, errors
python3 tools/real_time_monitor.py /dev/ttyACM0 --debug
```

### Combining Options

```bash
# Log data with custom baud rate
python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 9600 --log data.jsonl

# Debug output with no color
python3 tools/real_time_monitor.py /dev/ttyACM0 --debug --no-color

# Background monitoring with logging
python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl &
```

---

## Finding Your Serial Port

### On Linux/Mac

```bash
# List all serial ports
ls /dev/tty*

# Common ports:
#   /dev/ttyACM0   - Arduino (most common)
#   /dev/ttyUSB0   - USB serial adapters
#   /dev/ttyS0     - Built-in serial ports
```

### On Windows

1. Open **Device Manager**
2. Expand **Ports (COM & LPT)**
3. Look for entry like **Arduino Mega or USB Serial Port**
4. Note the COM port (e.g., COM3, COM5)

```bash
# Use in commands:
python3 tools/real_time_monitor.py COM3
```

### Auto-Detection

If you're not sure which port, try:

```bash
# The monitor will attempt auto-detection and show options
python3 tools/real_time_monitor.py
```

---

## Recording Data

### Method 1: Simple Logging (Recommended)

Simplest way to record everything:

```bash
# Record to JSONL file
python3 tools/real_time_monitor.py /dev/ttyACM0 --log my_test.jsonl

# Let it run for as long as you need
# Press Ctrl+C to stop

# File is saved with all data points
```

### Method 2: Timestamped Logging

Automatically create timestamped files:

```bash
# Log with timestamp in filename
python3 tools/real_time_monitor.py /dev/ttyACM0 --log "data_$(date +%Y%m%d_%H%M%S).jsonl"
```

### Method 3: Long-Duration Recording

For extended monitoring (e.g., 1 hour of data):

```bash
# Run in background with nohup (Linux/Mac)
nohup python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl &

# Or use screen/tmux for longer sessions
screen -S monitor
python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl
# Detach with Ctrl+A, then D

# View file periodically
tail -f data.jsonl
wc -l data.jsonl  # Count records
```

---

## Analyzing Recorded Data

### Reading the JSONL File

**Python example:**

```python
import json

# Read and parse JSONL file
data_points = []
with open('sensor_data.jsonl', 'r') as f:
    for line in f:
        data_points.append(json.loads(line))

print(f"Total records: {len(data_points)}")

# Access first point
first = data_points[0]
print(f"First yaw: {first['yaw']}°")
print(f"First position: ({first['lat']}, {first['lon']})")

# Calculate statistics
yaw_values = [d['yaw'] for d in data_points]
print(f"Yaw range: {min(yaw_values):.1f}° to {max(yaw_values):.1f}°")
print(f"Yaw average: {sum(yaw_values) / len(yaw_values):.1f}°")
```

### CSV Export

**Convert JSONL to CSV for Excel/spreadsheet:**

```bash
python3 << 'EOF'
import json
import csv

# Read JSONL
with open('sensor_data.jsonl', 'r') as f:
    data = [json.loads(line) for line in f]

# Write CSV
with open('sensor_data.csv', 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=data[0].keys())
    writer.writeheader()
    writer.writerows(data)

print(f"Exported {len(data)} rows to sensor_data.csv")
EOF
```

### Plotting Data

**Python with matplotlib:**

```python
import json
import matplotlib.pyplot as plt

# Read data
with open('sensor_data.jsonl', 'r') as f:
    data = [json.loads(line) for line in f]

# Extract values
timestamps = [d['timestamp'] for d in data]
roll = [d['roll'] for d in data]
pitch = [d['pitch'] for d in data]
yaw = [d['yaw'] for d in data]

# Create plots
fig, axes = plt.subplots(3, 1, figsize=(12, 8))

axes[0].plot(timestamps, roll, label='Roll')
axes[0].set_ylabel('Roll (degrees)')
axes[0].legend()
axes[0].grid(True)

axes[1].plot(timestamps, pitch, label='Pitch', color='orange')
axes[1].set_ylabel('Pitch (degrees)')
axes[1].legend()
axes[1].grid(True)

axes[2].plot(timestamps, yaw, label='Yaw', color='green')
axes[2].set_ylabel('Yaw (degrees)')
axes[2].set_xlabel('Time (seconds)')
axes[2].legend()
axes[2].grid(True)

plt.tight_layout()
plt.savefig('orientation_plot.png', dpi=100)
print("Plot saved to orientation_plot.png")
```

---

## Interpreting What You See

### Good Sensor Behavior

**What to look for:**

1. **Stable values when still**:
   ```
   Device held level and still:
   Roll:  0.2°, Pitch: 0.1°, Yaw: 247.3°
   Roll:  0.1°, Pitch: 0.2°, Yaw: 247.2°  ← Small variations normal
   Roll:  0.3°, Pitch: -0.1°, Yaw: 247.4°
   ```

2. **Smooth changes when moving**:
   ```
   Slow rotation to the right:
   Yaw: 0° → 10° → 20° → 30°  ← Smooth progression
   ```

3. **Data rate around 100 Hz**:
   ```
   Data Rate: 100-110 Hz  ← Normal
   ```

4. **Zero errors**:
   ```
   Errors: 0  ← Good, no corrupted packets
   ```

### Common Issues to Watch

**Issue 1: Large oscillations**
```
Yaw: 245° → 265° → 225° → 250°  ← Jumping around
```
**Cause**: Magnetic interference or incomplete calibration
**Fix**: Recalibrate or move away from interference

**Issue 2: Data rate drops**
```
Data Rate: 50 Hz  ← Should be ~100 Hz
```
**Cause**: Serial communication issue or sensor overload
**Fix**: Check USB cable, reduce other serial traffic

**Issue 3: High error count**
```
Errors: 127  ← Bad JSON packets received
```
**Cause**: Serial corruption, bad cable, or baud rate mismatch
**Fix**: Try different baud rate, better cable, or power supply

**Issue 4: GPS stuck "Acquiring"**
```
GPS: Acquiring ░ Fix: No
```
**Cause**: No satellite signals, not outdoors, or antenna blocked
**Fix**: Move outdoors, clear sky view, check antenna connection

---

## Monitoring Specific Aspects

### Tracking Orientation Accuracy

1. **Place device in known orientation** (e.g., facing north)
2. **Record yaw for 30 seconds**
3. **Check stability**:
   ```
   Yaw values over 30 seconds:
   244.8°, 245.1°, 244.9°, 245.2°, 245.0°, ...
   
   Average: 245.0°  Std Dev: 0.15°  ← Very stable
   ```

### Tracking GPS Accuracy

1. **Leave device stationary outdoors for 5 minutes**
2. **Record position data**
3. **Analyze spread**:
   ```
   Latitude values:
   37.44190, 37.44191, 37.44189, 37.44190, ...
   
   Spread: 0.00002° ≈ 2.2 meters
   ```

### Detecting Interference

1. **Monitor yaw in suspect location**
2. **Look for drift or oscillation**
3. **Compare to known-good location**

**Example:**
```
Office location:     Yaw = 245° ± 15°  (erratic)
Outdoor location:    Yaw = 245° ± 2°   (stable)
                     → Office has interference
```

---

## Data Rate and Performance

**Expected data rates:**

| Condition | Typical Rate | Notes |
|-----------|-------------|-------|
| Both sensors active | 100 Hz | BNO085 outputs ~100 Hz |
| BNO085 only | 100-110 Hz | Limited by IMU update rate |
| GPS data | 1-10 Hz | GPS updates slowly |
| Combined | 100 Hz | Orientation updates fast, position slow |

**Monitor shows**:
```
Data Rate: 102 Hz      ← Actual data points received per second
Packets: 12,456        ← Total valid packets since start
Errors: 0              ← Bad/corrupted packets
Uptime: 00:02:34       ← How long monitor has been running
```

---

## Troubleshooting Monitor Issues

### "Connection refused" or "Port not found"

```bash
# Find correct port
ls /dev/tty*

# Try different port
python3 tools/real_time_monitor.py /dev/ttyUSB0

# Or manually scan
python3 tools/test_monitor.py
```

### "Data Rate: 0 Hz" or "No packets received"

**Problem**: Not receiving any data

**Fixes**:
1. Verify Arduino is powered and flashed
2. Check USB cable is connected
3. Try different baud rate:
   ```bash
   python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 9600
   ```
4. Monitor serial output directly:
   ```bash
   minicom -D /dev/ttyACM0 -b 115200
   ```

### "High error rate" or "Errors: 1000+"

**Problem**: Receiving corrupted data

**Fixes**:
1. Try lower baud rate
2. Check USB cable quality
3. Verify power supply stability
4. Add ferrite clamp to USB cable
5. Move away from RF interference

### Monitor crashes or freezes

**Fixes**:
```bash
# Restart from scratch
Ctrl+C  # Stop current monitor

# Clean restart
python3 tools/real_time_monitor.py /dev/ttyACM0 --no-color
```

---

## Advanced Monitoring

### Monitor with Custom Processing

Create a Python script to process data in real-time:

```python
import json
import serial

port = '/dev/ttyACM0'
baud = 115200

with serial.Serial(port, baud) as ser:
    while True:
        line = ser.readline().decode('utf-8').strip()
        if line:
            try:
                data = json.loads(line)
                
                # Your custom processing
                if data['cep'] > 5:
                    print(f"WARNING: Poor GPS {data['cep']}m")
                
                if abs(data['yaw'] - 180) < 5:
                    print(f"Facing south: {data['yaw']}°")
                    
            except json.JSONDecodeError:
                pass
```

### Monitor Across Network

Stream data to another computer:

```bash
# On remote computer
nc -l 5000 | python3 tools/real_time_monitor.py

# On Arduino computer
python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet | nc remote.ip 5000
```

---

## Quick Reference: Common Commands

```bash
# Basic monitoring
python3 tools/real_time_monitor.py /dev/ttyACM0

# With logging
python3 tools/real_time_monitor.py /dev/ttyACM0 --log data.jsonl

# Debug mode
python3 tools/real_time_monitor.py /dev/ttyACM0 --debug

# Custom baud rate
python3 tools/real_time_monitor.py /dev/ttyACM0 --baud 9600

# Background logging
python3 tools/real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl &

# Stop background process
pkill -f real_time_monitor.py
```

---

## Next Steps

1. **Record baseline data**:
   - Monitor for 5 minutes in known-good location
   - Log to file for comparison later

2. **Test accuracy**:
   - Compare logged orientation to physical measurements
   - Verify GPS positions on a map

3. **Deploy to field**:
   - Use monitoring to validate field location
   - Record continuous data for analysis

4. **Analyze performance**:
   - Extract and plot data
   - Identify patterns or issues

---

**Last Updated**: 2025-05  
**Difficulty**: Intermediate  
**Time Required**: 5-30 minutes (depending on analysis)
