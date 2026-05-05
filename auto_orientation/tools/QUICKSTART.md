# Real-Time Monitor - Quick Start Guide

## Installation

Ensure pyserial is installed:
```bash
pip install pyserial
```

## Basic Usage

### 1. Connect Your Device

Plug in your auto_orientation board via USB. It will appear as `/dev/ttyACM0` (Linux) or `/dev/tty.usbmodem*` (macOS) or `COM3+` (Windows).

### 2. Run the Monitor

```bash
python3 real_time_monitor.py /dev/ttyACM0
```

You should see the real-time monitor display with:
- **ORIENTATION**: Roll, pitch, yaw angles and quaternion
- **POSITION**: GPS latitude, longitude, altitude, fix quality
- **CALIBRATION**: Visual bars showing sensor calibration levels
- **STATISTICS**: Data rates, sample counts, errors, and uptime

### 3. Common Options

#### Log data to file
```bash
python3 real_time_monitor.py /dev/ttyACM0 --log data.jsonl
```

#### Custom baud rate
```bash
python3 real_time_monitor.py /dev/ttyACM0 --baud 9600
```

#### Disable colors (for terminals without color support)
```bash
python3 real_time_monitor.py /dev/ttyACM0 --no-color
```

#### Quiet mode (no display, logging only)
```bash
python3 real_time_monitor.py /dev/ttyACM0 --quiet --log data.jsonl
```

## Understanding the Display

### ORIENTATION Section
```
ORIENTATION:
  Roll:   -5.2°  |  Pitch:  12.1°  |  Yaw:  123.4°
  Quat:  w=0.707, x=0.0, y=0.0, z=0.707
  Cal:  ███ SYS  ██░ ACC  █░░ GYR  ░░░ MAG
```

- **Roll/Pitch/Yaw**: Rotation angles in degrees (derived from quaternion)
- **Quat**: Raw quaternion data (w, x, y, z components)
- **Cal**: Calibration status for each sensor (░=uncalibrated, █=calibrated)

### POSITION Section
```
POSITION:
  Fix: GPS      |  Sats:  8  |  HDOP:   0.9m  |  CEP: ±4.5m
  Lat:  30.27123°  |  Lon: -97.74156°  |  Alt: 158.2m
```

- **Fix**: Type of GPS fix (GPS, DGPS, RTK, etc.)
- **Sats**: Number of visible satellites
- **HDOP**: Horizontal dilution of precision
- **CEP**: Circular error probable (accuracy estimate)
- **Lat/Lon**: Geographic coordinates
- **Alt**: Altitude above sea level

### DATA RATE Section
```
DATA RATE: 10.2 Hz orientation, 1.0 Hz position
STATS: Samples: 1024 ori / 102 pos  |  Errors: 0 JSON / 0 parse  |  Uptime: 00h 05m 30s
```

- **DATA RATE**: Current data rates in Hz (averaged over session)
- **Samples**: Total samples received for each data type
- **Errors**: JSON parse errors and data validation errors
- **Uptime**: How long the monitor has been running

## Common Issues

### "Address already in use" or "Port busy"

Another process is using the serial port:
```bash
# Release the port
python3 serial_monitor.py /dev/ttyACM0 --release
```

Or kill the process manually:
```bash
sudo fuser -k /dev/ttyACM0
```

### Garbled or no data

The baud rate might be wrong. Try:
```bash
python3 real_time_monitor.py /dev/ttyACM0 --baud 9600
```

Or check your firmware configuration for the correct baud rate.

### No GPS fix

This is normal! GPS takes time to acquire a fix:
- **Cold start**: 30-60 seconds
- **Warm start**: 10-30 seconds
- **Hot start**: 1-5 seconds

The satellite count (Sats) will increase as more satellites are acquired.

## Testing Without Hardware

Use the demo simulator to test the monitor:

```bash
# Terminal 1: Start the simulator
python3 demo_monitor.py

# Terminal 2: Run the monitor (use the pseudo-terminal shown in Terminal 1)
python3 real_time_monitor.py /dev/pts/X
```

The simulator creates realistic sensor data:
- Orientation rotating 360° in 30-second cycles
- Calibration progressing from uncalibrated to fully calibrated
- GPS fix acquisition with increasing satellite count

## Logging and Analysis

### View logged data
```bash
# Pretty-print first 10 entries
head -10 data.jsonl | python3 -m json.tool

# Count total samples
wc -l data.jsonl

# Extract just the roll angles
jq '.orientation.roll' data.jsonl | head -20
```

### Filter valid orientation data
```bash
jq 'select(.orientation.valid == true)' data.jsonl > valid_ori.jsonl
```

### Filter valid GPS fixes
```bash
jq 'select(.position.valid == true and .position.num_satellites >= 5)' data.jsonl
```

## Key Commands

| Command | Purpose |
|---------|---------|
| `python3 real_time_monitor.py /dev/ttyACM0` | Basic monitoring |
| `python3 real_time_monitor.py /dev/ttyACM0 --log data.jsonl` | Monitor + logging |
| `python3 real_time_monitor.py /dev/ttyACM0 --no-color` | Disable colors |
| `python3 test_monitor.py` | Run unit tests |
| `python3 demo_monitor.py` | Start simulator |
| `Ctrl+C` | Exit monitor |

## Next Steps

1. **Monitor your sensor**: Run the tool and verify orientation and position data
2. **Log data**: Use `--log` to capture sessions for analysis
3. **Analyze**: Use `jq` or Python to analyze the logged data
4. **Verify accuracy**: Compare displayed angles with physical orientation
5. **Troubleshoot**: Check calibration bars and GPS fix quality

For detailed information, see `MONITOR_README.md`.
