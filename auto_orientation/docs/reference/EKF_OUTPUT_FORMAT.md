# EKF Fused State JSON Output Format (Phase 3)

## Overview

The Extended Kalman Filter (EKF) produces a fused state estimate combining BNO085 IMU and GPS measurements. This document describes the JSON output format for the fused state, uncertainty estimates, GPS status, and EKF health metrics.

## JSON Schema Specification

### Complete Fused State Output

```json
{
  "timestamp": 12345,
  "fused": {
    "valid": true,
    "attitude": {
      "quaternion": {
        "w": 0.707,
        "x": 0.0,
        "y": 0.0,
        "z": 0.707
      },
      "euler": {
        "roll_deg": 90.0,
        "pitch_deg": 0.0,
        "yaw_deg": 0.0
      }
    },
    "velocity": {
      "north_mps": 1.0,
      "east_mps": 0.5,
      "down_mps": -0.1
    },
    "position": {
      "north_m": 145.2,
      "east_m": 87.3,
      "down_m": -23.5
    }
  },
  "uncertainty": {
    "attitude_rad": 0.01,
    "velocity_mps": 0.1,
    "position_m": 2.5,
    "accel_bias_m_s2": 0.05
  },
  "gps_status": {
    "locked": true,
    "satellites": 12,
    "hdop": 0.75,
    "dropout": false,
    "age_ms": 45,
    "dead_reckoning_valid": true,
    "dead_reckoning_age_ms": 450
  },
  "ekf_health": {
    "covariance_trace": 125.3,
    "innovation_magnitude": 0.5,
    "num_updates": 1234
  }
}
```

## Field Descriptions and Units

### Top Level

- **timestamp** (uint32): System time in milliseconds since boot (monotonic)

### Attitude Section

#### Quaternion
- **w**: Scalar component (real part), range [-1, 1]
- **x**: Vector component (imaginary i), range [-1, 1]
- **y**: Vector component (imaginary j), range [-1, 1]
- **z**: Vector component (imaginary k), range [-1, 1]

Quaternion convention: `q = [w, x, y, z]` where magnitude ≈ 1.0 (unit quaternion).
Represents rotation from NED (North-East-Down) frame to body frame.

#### Euler Angles
- **roll_deg**: Roll angle in degrees, range [-180, 180]
  - Rotation around X-axis (forward/backward tilt)
  - Positive: right wing down
- **pitch_deg**: Pitch angle in degrees, range [-90, 90]
  - Rotation around Y-axis (nose up/down)
  - Positive: nose up
- **yaw_deg**: Yaw angle in degrees, range [-180, 180]
  - Rotation around Z-axis (heading)
  - Positive: clockwise when viewed from above

### Velocity Section (NED Frame)

All in meters per second (m/s):

- **north_mps**: Velocity component toward magnetic north
  - Positive: moving north
  - Typical range: ±50 m/s
- **east_mps**: Velocity component toward east
  - Positive: moving east
  - Typical range: ±50 m/s
- **down_mps**: Velocity component toward down
  - Positive: moving downward (descending)
  - Typical range: ±20 m/s

### Position Section (NED Frame)

All in meters (m):

- **north_m**: Position relative to coordinate frame origin
  - Positive: north of origin
  - Typical range: ±10,000 m
- **east_m**: Position relative to coordinate frame origin
  - Positive: east of origin
  - Typical range: ±10,000 m
- **down_m**: Position relative to coordinate frame origin
  - Positive: below origin (downward/deeper)
  - Typical range: ±1,000 m

The coordinate frame origin is established at the first valid GPS fix.

### Uncertainty Section

Represents standard deviation (σ) extracted from the EKF covariance matrix diagonal:

- **attitude_rad**: Attitude uncertainty in radians
  - Typical range: 0.001 to 1.0 rad
  - Decreases as filter converges with IMU data
- **velocity_mps**: Velocity uncertainty in m/s
  - Typical range: 0.1 to 100.0 m/s
  - Higher during GPS dropout
- **position_m**: Position uncertainty in meters
  - Typical range: 1.0 to 10,000 m
  - Increases during GPS dropout (dead reckoning mode)
- **accel_bias_m_s2**: Accelerometer bias uncertainty in m/s²
  - Typical range: 0.01 to 1.0 m/s²

### GPS Status Section

- **locked**: Boolean, true if GPS has valid fix
- **satellites**: Number of satellites used (0-32)
- **hdop**: Horizontal Dilution of Precision, positive
  - Typical range: 0.5 to 10.0
  - Lower is better (< 2.0 is excellent)
  - Used to scale position measurement noise
- **dropout**: Boolean, true if GPS signal recently lost
  - Filter falls back to dead reckoning mode
  - Innovation checks become stricter
- **age_ms**: Milliseconds since last successful GPS update
  - 0 if just updated
  - Large values during dropout
- **dead_reckoning_valid**: Boolean, true if DR estimates trustworthy
  - Becomes false after > 30 seconds without GPS
- **dead_reckoning_age_ms**: Milliseconds in dead reckoning mode
  - 0 if GPS locked
  - Increases during dropout

### EKF Health Section

Used for diagnostics and filter monitoring:

- **covariance_trace**: Sum of diagonal elements of 16×16 covariance matrix
  - Represents total system uncertainty
  - Typical range: 0.1 to 1,000
  - Should decrease as filter updates and converge
- **innovation_magnitude**: Magnitude of measurement residual from last update
  - Innovation = measurement - predicted_measurement
  - Typical range: 0.01 to 10.0 m
  - Large values indicate innovation outlier
- **num_updates**: Total number of successful measurement updates (cumulative)
  - Starts at 0
  - Increments each time GPS update succeeds

## Coordinate Frames

### NED (North-East-Down)
- **Origin**: Latitude/longitude/altitude of first valid GPS fix
- **Axes**:
  - X (North): Toward magnetic north in local tangent plane
  - Y (East): Toward east in local tangent plane
  - Z (Down): Toward center of Earth (downward/deeper)
- **Position unit**: Meters relative to origin
- **Velocity unit**: m/s

### Body Frame
- **Origin**: Aircraft/vehicle center of mass
- **Axes**:
  - X (Forward): Forward direction of vehicle
  - Y (Right): Right wing direction
  - Z (Down): Downward from vehicle perspective
- **Quaternion**: Represents rotation from NED to body frame

## Example Outputs

### Good GPS Lock, Filter Converged

```json
{
  "timestamp": 45000,
  "fused": {
    "valid": true,
    "attitude": {
      "quaternion": {"w": 0.999, "x": 0.01, "y": 0.01, "z": 0.01},
      "euler": {"roll_deg": 1.15, "pitch_deg": 0.57, "yaw_deg": 0.57}
    },
    "velocity": {"north_mps": 5.2, "east_mps": -0.3, "down_mps": 0.1},
    "position": {"north_m": 500.2, "east_m": -150.3, "down_m": -50.0}
  },
  "uncertainty": {
    "attitude_rad": 0.002,
    "velocity_mps": 0.05,
    "position_m": 1.5,
    "accel_bias_m_s2": 0.01
  },
  "gps_status": {
    "locked": true,
    "satellites": 15,
    "hdop": 0.65,
    "dropout": false,
    "age_ms": 50,
    "dead_reckoning_valid": true,
    "dead_reckoning_age_ms": 0
  },
  "ekf_health": {
    "covariance_trace": 5.2,
    "innovation_magnitude": 0.3,
    "num_updates": 450
  }
}
```

### GPS Dropout, Dead Reckoning Mode

```json
{
  "timestamp": 65000,
  "fused": {
    "valid": true,
    "attitude": {
      "quaternion": {"w": 0.995, "x": 0.02, "y": 0.02, "z": 0.05},
      "euler": {"roll_deg": 2.3, "pitch_deg": 1.1, "yaw_deg": 2.9}
    },
    "velocity": {"north_mps": 10.5, "east_mps": -2.1, "down_mps": -0.5},
    "position": {"north_m": 750.5, "east_m": -280.1, "down_m": -45.0}
  },
  "uncertainty": {
    "attitude_rad": 0.015,
    "velocity_mps": 0.5,
    "position_m": 25.0,
    "accel_bias_m_s2": 0.1
  },
  "gps_status": {
    "locked": false,
    "satellites": 0,
    "hdop": 999.0,
    "dropout": true,
    "age_ms": 8500,
    "dead_reckoning_valid": true,
    "dead_reckoning_age_ms": 8500
  },
  "ekf_health": {
    "covariance_trace": 650.2,
    "innovation_magnitude": 0.0,
    "num_updates": 450
  }
}
```

### GPS Signal Lost (Dead Reckoning Expired)

```json
{
  "timestamp": 95000,
  "fused": {
    "valid": true,
    "attitude": {
      "quaternion": {"w": 0.98, "x": 0.05, "y": 0.03, "z": 0.1},
      "euler": {"roll_deg": 5.7, "pitch_deg": 3.4, "yaw_deg": 5.8}
    },
    "velocity": {"north_mps": 12.0, "east_mps": -3.5, "down_mps": -1.0},
    "position": {"north_m": 1200.0, "east_m": -500.0, "down_m": -40.0}
  },
  "uncertainty": {
    "attitude_rad": 0.1,
    "velocity_mps": 5.0,
    "position_m": 500.0,
    "accel_bias_m_s2": 0.5
  },
  "gps_status": {
    "locked": false,
    "satellites": 0,
    "hdop": 999.0,
    "dropout": true,
    "age_ms": 38500,
    "dead_reckoning_valid": false,
    "dead_reckoning_age_ms": 38500
  },
  "ekf_health": {
    "covariance_trace": 250000.0,
    "innovation_magnitude": 0.0,
    "num_updates": 450
  }
}
```

## Parsing and Usage Examples

### Python Example

```python
import json

json_data = '{"timestamp": 45000, "fused": {"valid": true, ...}}'
data = json.loads(json_data)

# Access fused state
timestamp = data['timestamp']
attitude = data['fused']['attitude']
euler = attitude['euler']
yaw = euler['yaw_deg']

# Check filter health
uncertainty = data['uncertainty']
pos_uncertainty_m = uncertainty['position_m']

# Monitor GPS status
gps = data['gps_status']
if gps['dropout']:
    print(f"GPS dropout for {gps['dead_reckoning_age_ms']} ms")
    if not gps['dead_reckoning_valid']:
        print("WARNING: Dead reckoning estimates unreliable")

# EKF diagnostics
health = data['ekf_health']
print(f"Covariance trace: {health['covariance_trace']}")
```

### C/C++ Example (Arduino)

```cpp
// Parse JSON (use ArduinoJson library)
StaticJsonDocument<512> doc;
deserializeJson(doc, json_string);

// Extract fused state
float yaw_deg = doc["fused"]["attitude"]["euler"]["yaw_deg"];
float north_m = doc["fused"]["position"]["north_m"];

// Check GPS status
bool gps_locked = doc["gps_status"]["locked"];
uint16_t gps_age_ms = doc["gps_status"]["age_ms"];

// Monitor uncertainty
float pos_uncertainty = doc["uncertainty"]["position_m"];
```

### Typescript Example

```typescript
interface FusedState {
  timestamp: number;
  fused: {
    valid: boolean;
    attitude: {
      quaternion: { w: number; x: number; y: number; z: number };
      euler: { roll_deg: number; pitch_deg: number; yaw_deg: number };
    };
    velocity: { north_mps: number; east_mps: number; down_mps: number };
    position: { north_m: number; east_m: number; down_m: number };
  };
  uncertainty: {
    attitude_rad: number;
    velocity_mps: number;
    position_m: number;
    accel_bias_m_s2: number;
  };
  gps_status: {
    locked: boolean;
    satellites: number;
    hdop: number;
    dropout: boolean;
    age_ms: number;
    dead_reckoning_valid: boolean;
    dead_reckoning_age_ms: number;
  };
  ekf_health: {
    covariance_trace: number;
    innovation_magnitude: number;
    num_updates: number;
  };
}

function parseEKFOutput(json: string): FusedState {
  return JSON.parse(json);
}
```

## Uncertainty Interpretation Guide

### Attitude Uncertainty (attitude_rad)

- **0.001 to 0.01**: Excellent heading accuracy (±0.06° to ±0.6°)
- **0.01 to 0.05**: Good accuracy (±0.6° to ±2.9°)
- **0.05 to 0.1**: Moderate accuracy (±2.9° to ±5.7°)
- **0.1 to 1.0**: Poor accuracy, IMU unreliable (±5.7° to ±57°)

### Velocity Uncertainty (velocity_mps)

- **0.05 to 0.2**: Excellent (±0.05 to ±0.2 m/s)
  - GPS updates frequent, high-quality
- **0.2 to 1.0**: Good (±0.2 to ±1.0 m/s)
  - Normal GPS operation, starting dead reckoning
- **1.0 to 10.0**: Moderate (±1.0 to ±10 m/s)
  - Extended GPS dropout (< 30 seconds)
- **> 10.0**: Poor (> ±10 m/s)
  - Very long GPS dropout, dead reckoning unreliable

### Position Uncertainty (position_m)

- **0.5 to 5.0**: Excellent (< 1 standard deviation)
  - GPS locked with good HDOP
- **5.0 to 20.0**: Good
  - Normal GPS operation
- **20.0 to 100.0**: Moderate
  - Beginning GPS dropout, initial DR phase
- **100.0 to 1,000.0**: Poor
  - Extended GPS dropout (10-30 seconds)
- **> 1,000.0**: Very poor
  - Very long dropout or filter divergence

### Accel Bias Uncertainty (accel_bias_m_s2)

- **0.01 to 0.05**: Excellent (bias well-estimated)
- **0.05 to 0.2**: Good
- **0.2 to 1.0**: Moderate (bias may be changing)
- **> 1.0**: Poor (bias diverging, filter needs update)

## Comparison JSON (Raw GPS vs Fused)

For validation and tuning, an optional comparison output is available:

```json
{
  "timestamp": 45000,
  "comparison": {
    "valid": true,
    "raw_gps": {
      "latitude": 52.520008,
      "longitude": 13.404954,
      "accuracy_m": 3.5,
      "num_satellites": 12
    },
    "fused_position": {
      "north_m": 145.2,
      "east_m": 87.3,
      "down_m": -23.5
    },
    "difference_m": 2.1,
    "rms_error_m": 1.8,
    "gps_dropout": false
  }
}
```

This shows:
- **difference_m**: Euclidean distance between raw GPS and fused position
- **rms_error_m**: RMS error from EKF uncertainty estimate
- Useful for evaluating filter tuning and model correctness

## Configuration Flags

See `src/config/ekf_config.h`:

```cpp
// Enable detailed JSON output with full fused state
static constexpr bool ENABLE_DETAILED_JSON = true;

// Enable comparison JSON (raw vs fused position)
static constexpr bool ENABLE_COMPARISON_JSON = true;

// Enable full uncertainty/covariance output
static constexpr bool ENABLE_UNCERTAINTY_JSON = true;
```

## Output Frequency

- Default: 10 Hz (100 ms intervals)
- Configurable via `output_manager.setFrequencyHz(hz)`
- EKF internal update rate: 100 Hz (IMU) + variable (GPS 1 Hz)
- JSON serialization: < 5 ms per output

## Performance Considerations

- **No dynamic allocation**: All buffers are fixed-size (1024 bytes typical)
- **Efficient formatting**: Uses dtostrf for float to string conversion
- **Minimal overhead**: JSON generation adds < 5ms per output
- **Compatible with Arduino Mega**: Suitable for embedded systems

## Troubleshooting

### Large Covariance Trace
- Filter uncertainty too high
- Possible causes: No GPS fix yet, GPS dropout, too much process noise
- Solution: Ensure GPS has solid lock, check Q matrix tuning

### Large Innovation Magnitude
- Measurement doesn't match prediction
- Possible causes: Bad GPS fix, filter divergence
- Solution: Check GPS accuracy (HDOP), verify sensor calibration

### Attitude Uncertainty Not Decreasing
- IMU not providing useful information
- Possible causes: Poor calibration, stationary system
- Solution: Ensure BNO085 is calibrated, move vehicle

### Position Increasing During GPS Dropout
- Expected behavior, uncertainty grows during dead reckoning
- Solution: Ensure GPS lock restored within 30 seconds

## Related Documentation

- See `docs/CALIBRATION_GUIDE.md` for BNO085 calibration
- See `src/config/ekf_config.h` for tuning parameters
- See `src/navigation/ekf.h` for EKF API reference
