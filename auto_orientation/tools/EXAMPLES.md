# Real-Time Monitor - Usage Examples

## Example 1: Basic Monitoring

Start monitoring sensor output on the default baud rate:

```bash
python3 real_time_monitor.py /dev/ttyACM0
```

Press `Ctrl+C` to exit. You'll see final statistics on stderr.

## Example 2: Continuous Logging Session

Capture 5 minutes of sensor data to a file:

```bash
python3 real_time_monitor.py /dev/ttyACM0 --log session_20250505.jsonl &
sleep 300  # 5 minutes
kill %1
```

Then analyze:
```bash
wc -l session_20250505.jsonl
tail -5 session_20250505.jsonl | python3 -m json.tool
```

## Example 3: High-Rate Data Collection

If your device outputs at higher baud rates, specify it:

```bash
python3 real_time_monitor.py /dev/ttyACM0 --baud 921600
```

## Example 4: Background Monitoring with Logging

Run monitoring in the background (no display updates), suitable for long-term data collection:

```bash
python3 real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl > /dev/null 2>&1 &
echo $!  # Save PID for later
# ... do other things ...
kill $!
```

## Example 5: Multiple Devices

Monitor multiple devices in parallel:

```bash
# Terminal 1
python3 real_time_monitor.py /dev/ttyACM0 --log device1.jsonl

# Terminal 2
python3 real_time_monitor.py /dev/ttyACM1 --log device2.jsonl
```

## Example 6: Testing Without Hardware

Use the simulator to test the monitor:

```bash
# Terminal 1: Start simulator
python3 demo_monitor.py
# Output shows: Pseudo-terminal created: /dev/pts/123

# Terminal 2: Run monitor (replace 123 with actual number)
python3 real_time_monitor.py /dev/pts/123
```

## Example 7: Monochrome Output (SSH/Remote)

For SSH or terminals without color support:

```bash
python3 real_time_monitor.py /dev/ttyACM0 --no-color
```

## Example 8: Extract Euler Angles Only

Log and then extract just the orientation data:

```bash
# Record session
python3 real_time_monitor.py /dev/ttyACM0 --log raw.jsonl

# Extract Euler angles to CSV
python3 << 'EOF'
import json
import math

# Convert quaternion to Euler angles
def quat_to_euler(w, x, y, z):
    sinr_cosp = 2 * (w * x + y * z)
    cosr_cosp = 1 - 2 * (x * x + y * y)
    roll = math.degrees(math.atan2(sinr_cosp, cosr_cosp))
    
    sinp = 2 * (w * y - z * x)
    sinp = max(-1.0, min(1.0, sinp))
    pitch = math.degrees(math.asin(sinp))
    
    siny_cosp = 2 * (w * z + x * y)
    cosy_cosp = 1 - 2 * (y * y + z * z)
    yaw = math.degrees(math.atan2(siny_cosp, cosy_cosp))
    
    return roll, pitch, yaw

# Extract data
print("timestamp_ms,roll_deg,pitch_deg,yaw_deg,sat_count,hdop_m")
with open('raw.jsonl') as f:
    for line in f:
        data = json.loads(line)
        if data['orientation']['valid']:
            q = data['orientation']['quaternion']
            r, p, y = quat_to_euler(q['w'], q['x'], q['y'], q['z'])
            ts = data['timestamp_ms']
            sats = data['position'].get('num_satellites', 0)
            hdop = data['position'].get('hdop', 0)
            print(f"{ts},{r:.2f},{p:.2f},{y:.2f},{sats},{hdop:.2f}")
EOF
```

## Example 9: Analyze GPS Accuracy

Compute statistics on GPS accuracy:

```bash
python3 << 'EOF'
import json
import statistics

accuracies = []
with open('raw.jsonl') as f:
    for line in f:
        data = json.loads(line)
        if data['position']['valid'] and data['position']['accuracy_m']:
            accuracies.append(data['position']['accuracy_m'])

if accuracies:
    print(f"GPS Accuracy Statistics:")
    print(f"  Samples: {len(accuracies)}")
    print(f"  Min: {min(accuracies):.2f}m")
    print(f"  Max: {max(accuracies):.2f}m")
    print(f"  Mean: {statistics.mean(accuracies):.2f}m")
    print(f"  Median: {statistics.median(accuracies):.2f}m")
    print(f"  StdDev: {statistics.stdev(accuracies):.2f}m")
EOF
```

## Example 10: Verify Calibration Progress

Track how calibration improves over time:

```bash
python3 << 'EOF'
import json

print("Time(s),Sys,Acc,Gyr,Mag")
with open('raw.jsonl') as f:
    for i, line in enumerate(f):
        if i % 10 != 0:  # Sample every 10th entry
            continue
        data = json.loads(line)
        cal = data['orientation']['calibration']
        time_s = data['timestamp_ms'] / 1000.0
        print(f"{time_s:.1f},{cal['system']},{cal['accel']},{cal['gyro']},{cal['mag']}")
EOF
```

## Example 11: Real-Time Rotation Measurement

Monitor rotation rate in real-time by comparing consecutive samples:

```bash
# Use monitor with logging and post-process
python3 real_time_monitor.py /dev/ttyACM0 --log rotation_test.jsonl &
MONITOR_PID=$!

# Wait 30 seconds
sleep 30
kill $MONITOR_PID

# Analyze rotation speed
python3 << 'EOF'
import json
import math

prev_yaw = None
max_rate = 0
with open('rotation_test.jsonl') as f:
    for line in f:
        data = json.loads(line)
        if not data['orientation']['valid']:
            continue
        q = data['orientation']['quaternion']
        # Simple yaw extraction (not full Euler, just demo)
        yaw = math.degrees(math.atan2(2*q['z']*q['w'] + 2*q['x']*q['y'],
                                      1 - 2*q['y']**2 - 2*q['z']**2))
        if prev_yaw is not None:
            rate = abs(yaw - prev_yaw)
            max_rate = max(max_rate, rate)
        prev_yaw = yaw

print(f"Maximum rotation rate: {max_rate:.2f}°/sample @ 10Hz = {max_rate*10:.1f}°/s")
EOF
```

## Example 12: Synchronized GPS and IMU Data

Extract synchronized orientation and position samples:

```bash
python3 << 'EOF'
import json

print("timestamp_ms,roll,pitch,yaw,lat,lon,alt,num_sats")
with open('raw.jsonl') as f:
    for line in f:
        data = json.loads(line)
        if data['orientation']['valid'] and data['position']['valid']:
            q = data['orientation']['quaternion']
            w, x, y, z = q['w'], q['x'], q['y'], q['z']
            
            # Euler angles
            roll = math.degrees(math.atan2(2*(w*x+y*z), 1-2*(x*x+y*y)))
            pitch = math.degrees(math.asin(2*(w*y-z*x)))
            yaw = math.degrees(math.atan2(2*(w*z+x*y), 1-2*(y*y+z*z)))
            
            p = data['position']
            print(f"{data['timestamp_ms']},{roll:.2f},{pitch:.2f},{yaw:.2f},"
                  f"{p['latitude']:.5f},{p['longitude']:.5f},{p['altitude_m']:.1f},"
                  f"{p['num_satellites']}")
EOF
```

## Example 13: Docker Container Usage

Use the monitor in a Docker container:

```dockerfile
FROM python:3.11-slim
RUN apt-get update && apt-get install -y python3-serial
WORKDIR /app
COPY real_time_monitor.py .
ENTRYPOINT ["python3", "real_time_monitor.py"]
```

Build and run:
```bash
docker build -t orientation-monitor .
docker run --device=/dev/ttyACM0 orientation-monitor /dev/ttyACM0
```

## Example 14: Integration with Data Pipeline

Save processed data to database:

```bash
# Run monitor with logging
python3 real_time_monitor.py /dev/ttyACM0 --log /tmp/live.jsonl &

# Process new lines continuously
tail -f /tmp/live.jsonl | python3 << 'EOF'
import sys, json, sqlite3

conn = sqlite3.connect('sensor_data.db')
c = conn.cursor()
c.execute('''CREATE TABLE IF NOT EXISTS samples
    (ts INTEGER, roll REAL, pitch REAL, yaw REAL, lat REAL, lon REAL)''')

for line in sys.stdin:
    data = json.loads(line)
    if data['orientation']['valid'] and data['position']['valid']:
        q = data['orientation']['quaternion']
        # ... compute Euler angles ...
        c.execute('INSERT INTO samples VALUES (?,?,?,?,?,?)',
                 (ts, roll, pitch, yaw, lat, lon))
        conn.commit()
EOF
```

## Example 15: Streaming to Network

Send live sensor data to a remote server:

```bash
# Forward via netcat
python3 real_time_monitor.py /dev/ttyACM0 --quiet --log /dev/stdout | nc 192.168.1.100 5000
```

Or with a Python receiver:
```python
import socket, json
sock = socket.socket()
sock.bind(('0.0.0.0', 5000))
sock.listen(1)
conn, addr = sock.accept()
for line in conn.makefile('r'):
    data = json.loads(line)
    print(f"Received: {data['orientation']['roll']}")
```

## Troubleshooting Examples

### Device Connection Issue

```bash
# Check if device exists
ls -la /dev/ttyACM0

# Check if it's busy
fuser /dev/ttyACM0

# Force release
sudo fuser -k /dev/ttyACM0

# Try monitor
python3 real_time_monitor.py /dev/ttyACM0
```

### Data Quality Issue

```bash
# Log raw data and inspect
python3 real_time_monitor.py /dev/ttyACM0 --log debug.jsonl

# Check error counts
head -100 debug.jsonl | wc -l

# Find malformed entries
jq -r 'select(.orientation.valid == false)' debug.jsonl | head -5
```

### Performance Monitoring

```bash
# Check CPU usage
time python3 real_time_monitor.py /dev/ttyACM0 --quiet --log /dev/null &
sleep 10
ps aux | grep real_time_monitor
```

All examples use standard Python and Unix tools. Adapt file paths and device names to your system.
