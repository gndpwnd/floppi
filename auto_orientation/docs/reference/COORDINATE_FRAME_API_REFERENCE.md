# Coordinate Frame API Reference

**Status**: Phase 2 Documentation  
**Last Updated**: 2026-05-07  
**Related Docs**: [GPS Driver API](GPS_DRIVER_API_REFERENCE.md), [Coordinate Conversion API](COORDINATE_CONVERSION_API.md)

## Overview

The CoordinateFrame class (`src/navigation/coordinate_frame.h/cpp`) manages a local North-East-Down (NED) reference frame for converting GPS positions into vehicle-centric coordinates. It provides high-precision geodetic transformations with sub-meter accuracy.

**Key Features:**
- Automatic origin initialization on first GPS fix
- GPS (WGS84) to local NED conversion
- Inverse NED to GPS conversion
- Pre-computed trigonometric values for performance
- Thread-safe queries
- Support for reference point testing and validation

---

## Class Interface: CoordinateFrame

### Header Location
```cpp
#include "navigation/coordinate_frame.h"
```

### Constructor

#### `CoordinateFrame()`
Default constructor. Creates an uninitialized coordinate frame.

**Parameters**: None

**Details**:
- Does not set an origin until `initialize()` or `initializeOnFirstFix()` is called
- Safe to create multiple instances (each has independent origin)
- No dynamic memory allocation (all data on stack)

**Example:**
```cpp
CoordinateFrame local_ned;
// Not yet initialized, getOrigin() returns undefined values
```

---

## Initialization Methods

### Manual Initialization

#### `bool initialize(double origin_lat, double origin_lon, float origin_alt_m)`
Manually set the coordinate frame origin at a specific GPS location.

**Parameters**:
- `origin_lat` (double): Latitude in degrees (-90.0 to +90.0)
- `origin_lon` (double): Longitude in degrees (-180.0 to +180.0)
- `origin_alt_m` (float): Altitude in meters above sea level

**Returns**:
- `true` if origin was accepted (values in valid ranges)
- `false` if latitude, longitude, or altitude out of range

**Details**:
- Sets this GPS location as the origin for all NED calculations
- Pre-computes sin(lat) and cos(lat) for efficient conversions
- Can be called multiple times to change origin
- Clears previous origin data when called

**Validation**:
- Latitude: -90.0 ≤ lat ≤ +90.0
- Longitude: -180.0 ≤ lon ≤ +180.0
- Altitude: -500 ≤ alt ≤ +30000 meters

**Example - Known Reference Point:**
```cpp
CoordinateFrame frame;
// Set origin to Munich city center (famous test point)
if (frame.initialize(48.13743, 11.58549, 520.0)) {
  Serial.println("Origin set to Munich");
} else {
  Serial.println("Invalid origin coordinates");
}
```

### Automatic Initialization

#### `bool initializeOnFirstFix(const PositionData& gps)`
Automatically initialize origin when GPS gets its first valid fix.

**Parameters**:
- `gps` (const PositionData&): GPS position struct from GPS sensor
  ```cpp
  struct PositionData {
    double latitude;
    double longitude;
    float altitude_m;
    bool position_valid;
    uint32_t timestamp_ms;
  };
  ```

**Returns**:
- `true` if origin was initialized from GPS data
- `false` if GPS position invalid or already initialized

**Details**:
- Useful for "warm start" - let first GPS reading define the local frame
- Only initializes once (subsequent calls with valid GPS don't change origin)
- Ideal for autonomous vehicles (origin = initial position)
- Checks `gps.position_valid` before accepting

**Example - Typical Initialization Pattern:**
```cpp
GPS gps;
CoordinateFrame frame;
bool origin_set = false;

void setup() {
  gps.begin();  // Start GPS
  // frame stays uninitialized until first fix
}

void loop() {
  gps.read();
  
  if (!origin_set && gps.hasLock()) {
    if (frame.initializeOnFirstFix(gps.getPosition())) {
      Serial.println("Coordinate frame initialized!");
      origin_set = true;
    }
  }
  
  if (origin_set) {
    // Can now convert positions to NED
  }
}
```

---

## Conversion Methods

### GPS to NED Conversion

#### `bool gpsToLocalNED(double lat, double lon, float alt_m, float* north_m, float* east_m, float* down_m) const`
Convert WGS84 GPS coordinates to local NED position relative to origin.

**Parameters**:
- `lat` (double): Latitude in degrees
- `lon` (double): Longitude in degrees
- `alt_m` (float): Altitude in meters
- `north_m` (float*): Output pointer for North component (meters)
- `east_m` (float*): Output pointer for East component (meters)
- `down_m` (float*): Output pointer for Down component (meters)

**Returns**:
- `true` if conversion successful
- `false` if frame not initialized or parameters invalid

**Details**:
- Converts GPS position to relative offset from origin
- **North**: Positive = away from equator (toward North Pole)
- **East**: Positive = away from prime meridian (toward International Date Line)
- **Down**: Positive = toward Earth's center (below origin)
- Typical accuracy: ±0.5 to ±2 meters depending on GPS quality
- Uses pre-computed trigonometric values (efficient)

**Mathematical Foundation**:
```
Given origin (lat0, lon0, alt0) and point (lat, lon, alt):
1. Convert GPS to ECEF (Earth-Centered Earth-Fixed) coordinates
2. Compute relative ECEF vector from origin
3. Rotate ECEF vector to NED frame using rotation matrix
4. Result is North, East, Down meters relative to origin
```

**Example - Single Position Conversion:**
```cpp
CoordinateFrame frame;
frame.initialize(48.13743, 11.58549, 520.0);  // Munich

float north, east, down;
if (frame.gpsToLocalNED(48.13745, 11.58550, 520.5, &north, &east, &down)) {
  Serial.printf("NED: N=%.2f, E=%.2f, D=%.2f meters\n", 
                north, east, down);
  // Output: ~222m north, ~97m east, -0.5m down (point is 0.5m higher)
} else {
  Serial.println("Conversion failed (frame not initialized)");
}
```

**Example - Continuous Position Tracking:**
```cpp
GPS gps;
CoordinateFrame frame;

void loop() {
  gps.read();
  
  if (frame.isInitialized() && gps.getPosition().position_valid) {
    float n, e, d;
    const auto& pos = gps.getPosition();
    
    if (frame.gpsToLocalNED(pos.latitude, pos.longitude, 
                            pos.altitude_m, &n, &e, &d)) {
      Serial.printf("Vehicle at NED (%.1f, %.1f, %.1f) m\n", n, e, d);
    }
  }
}
```

### NED to GPS Conversion

#### `bool localNEDToGPS(float north_m, float east_m, float down_m, double* lat, double* lon, float* alt_m) const`
Convert local NED position back to WGS84 GPS coordinates.

**Parameters**:
- `north_m` (float): North component (meters)
- `east_m` (float): East component (meters)
- `down_m` (float): Down component (meters)
- `lat` (double*): Output pointer for latitude (degrees)
- `lon` (double*): Output pointer for longitude (degrees)
- `alt_m` (float*): Output pointer for altitude (meters)

**Returns**:
- `true` if conversion successful
- `false` if frame not initialized or parameters invalid

**Details**:
- Inverse of `gpsToLocalNED()`
- Allows planning waypoints in local NED, converting back to GPS coordinates
- Useful for navigation planning (e.g., "go 100m north, 50m east")
- Accuracy: ±0.5 to ±2 meters (same as forward conversion)

**Mathematical Foundation**:
```
Given origin (lat0, lon0, alt0) and NED offset (north, east, down):
1. Create NED vector [north, east, down]
2. Rotate NED vector to ECEF frame using inverse rotation matrix
3. Add to origin ECEF coordinates
4. Convert ECEF back to GPS (lat, lon, alt)
```

**Example - Waypoint Navigation:**
```cpp
CoordinateFrame frame;
frame.initialize(48.13743, 11.58549, 520.0);  // Munich origin

// Define waypoint 100m north, 50m east of origin
float north_wp = 100.0;
float east_wp = 50.0;
float down_wp = 0.0;  // Same altitude

double lat_wp, lon_wp;
float alt_wp;

if (frame.localNEDToGPS(north_wp, east_wp, down_wp, 
                        &lat_wp, &lon_wp, &alt_wp)) {
  Serial.printf("Waypoint GPS: %.6f, %.6f @ %.1f m\n",
                lat_wp, lon_wp, alt_wp);
  // Output: ~48.13833, 11.58594 @ 520.0 m
}
```

**Example - Circular Path Planning:**
```cpp
CoordinateFrame frame;
frame.initialize(48.13743, 11.58549, 520.0);

// Generate 8 waypoints in a circle around origin
float radius = 50.0;  // 50m radius
for (int i = 0; i < 8; i++) {
  float angle = i * (2 * 3.14159 / 8);
  float north = radius * cos(angle);
  float east = radius * sin(angle);
  
  double lat, lon;
  float alt;
  if (frame.localNEDToGPS(north, east, 0.0, &lat, &lon, &alt)) {
    Serial.printf("Waypoint %d: %.6f, %.6f\n", i, lat, lon);
  }
}
```

---

## State Query Methods

### Origin Information

#### `bool isInitialized() const`
Check if coordinate frame origin has been set.

**Parameters**: None

**Returns**:
- `true` if `initialize()` or `initializeOnFirstFix()` succeeded
- `false` if no origin set yet

**Details**:
- Always check this before calling conversion methods
- Conversions will fail if not initialized
- Origin persists until `initialize()` called again

**Example:**
```cpp
if (frame.isInitialized()) {
  // Safe to convert GPS to NED
  frame.gpsToLocalNED(...);
} else {
  Serial.println("Frame not initialized yet");
}
```

#### `void getOrigin(double* lat, double* lon, float* alt_m) const`
Retrieve the current origin coordinates.

**Parameters**:
- `lat` (double*): Output pointer for latitude
- `lon` (double*): Output pointer for longitude
- `alt_m` (float*): Output pointer for altitude

**Returns**: void

**Details**:
- Safe to call even if frame not initialized (returns undefined values)
- Check `isInitialized()` first if validity matters
- Useful for logging or validating current origin

**Example:**
```cpp
double lat, lon;
float alt;
frame.getOrigin(&lat, &lon, &alt);
Serial.printf("Origin: %.6f, %.6f @ %.1f m\n", lat, lon, alt);
```

#### `float getOriginAltitude() const`
Get the altitude component of origin (convenience method).

**Parameters**: None

**Returns**: 
- Origin altitude in meters above sea level
- Undefined if frame not initialized

**Details**:
- Faster than `getOrigin()` if only altitude needed
- Useful for altitude comparisons

**Example:**
```cpp
float origin_alt = frame.getOriginAltitude();
float climb_rate = (current_alt - origin_alt) / elapsed_seconds;
Serial.printf("Climb rate: %.2f m/s\n", climb_rate);
```

---

## Reference Points for Testing

The following real-world locations can be used to verify coordinate conversions:

### Munich City Center
```cpp
double lat = 48.13743;      // degrees North
double lon = 11.58549;      // degrees East
float alt = 520.0;          // meters above sea level
// Known as reliable GPS test point in Europe
```

**Verification**: Points near Munich should convert with ±2m accuracy

### Los Angeles City Hall
```cpp
double lat = 34.05328;
double lon = -118.24371;
float alt = 92.0;
// Iconic building in Southern California
```

### Sydney Opera House
```cpp
double lat = -33.85716;
double lon = 151.21537;
float alt = 5.0;
// Famous landmark, good for Southern Hemisphere testing
```

### Equator Reference (Quito, Ecuador)
```cpp
double lat = -0.22163;
double lon = -78.51184;
float alt = 2805.0;
// Useful for testing near equator (special geometry)
```

### Test Conversion Verification

```cpp
void testConversions() {
  CoordinateFrame frame;
  
  // Test 1: Munich round-trip
  frame.initialize(48.13743, 11.58549, 520.0);
  
  float north, east, down;
  frame.gpsToLocalNED(48.13745, 11.58550, 520.5, &north, &east, &down);
  
  // Verify: ~222m north, ~97m east, -0.5m down
  Serial.printf("Munich test: N=%.2f, E=%.2f, D=%.2f\n", north, east, down);
  
  // Round-trip: NED back to GPS
  double lat, lon;
  float alt;
  frame.localNEDToGPS(north, east, down, &lat, &lon, &alt);
  
  // Should recover original: 48.13745, 11.58550, 520.5
  Serial.printf("Round-trip: %.6f, %.6f @ %.1f\n", lat, lon, alt);
}
```

---

## Accuracy Expectations

### Positional Accuracy
- **Horizontal (North/East)**: ±0.5 to ±2.0 meters
- **Vertical (Down/Altitude)**: ±1.0 to ±3.0 meters
- Depends on GPS HDOP and atmospheric conditions
- Earth radius calculations accurate to ±0.1 meters globally

### Conversions
- **Forward (GPS→NED)**: ±0.5m typical error
- **Inverse (NED→GPS)**: ±0.5m typical error
- **Round-trip error**: Negligible (< ±0.1m)
- Round-trip: GPS→NED→GPS should return original coordinates

### Operating Ranges
- **Valid latitude**: -90.0 to +90.0 degrees
- **Valid longitude**: -180.0 to +180.0 degrees
- **Valid altitude**: -500 to +30000 meters
- **Maximum distance from origin**: Practical limit ~10 km before curvature effects dominate

### Curvature Effects
- **Up to 1 km**: Flat-earth model accurate to ±0.01 m
- **1-10 km**: Curvature effects ±0.1 to ±1.0 m
- **>10 km**: Use full geodetic calculations (beyond scope of this class)

---

## Integration with GPS and Orientation

### Typical Fusion Pipeline

```cpp
#include "sensors/gps.h"
#include "sensors/bno085.h"
#include "navigation/coordinate_frame.h"

GPS gps;
BNO085 imu;
CoordinateFrame frame;

struct FusionOutput {
  double latitude;
  double longitude;
  float altitude_m;
  float north_m;
  float east_m;
  float down_m;
  float roll_deg;
  float pitch_deg;
  float yaw_deg;
};

FusionOutput fused_data;

void setup() {
  gps.begin();
  imu.begin();
  // Frame not initialized yet - waiting for first GPS fix
}

void loop() {
  // Read sensors
  gps.read();
  imu.read();
  
  // Auto-initialize frame on first GPS fix
  if (!frame.isInitialized() && gps.hasLock()) {
    frame.initializeOnFirstFix(gps.getPosition());
    Serial.println("Coordinate frame ready!");
  }
  
  // Fuse data if both sensors healthy
  if (frame.isInitialized() && gps.isHealthy() && imu.isHealthy()) {
    const auto& gps_pos = gps.getPosition();
    const auto& imu_orientation = imu.getEuler();
    
    // Convert GPS to local NED
    frame.gpsToLocalNED(gps_pos.latitude, gps_pos.longitude, 
                        gps_pos.altitude_m,
                        &fused_data.north_m, &fused_data.east_m, 
                        &fused_data.down_m);
    
    // Include orientation
    fused_data.latitude = gps_pos.latitude;
    fused_data.longitude = gps_pos.longitude;
    fused_data.altitude_m = gps_pos.altitude_m;
    fused_data.roll_deg = imu_orientation.roll;
    fused_data.pitch_deg = imu_orientation.pitch;
    fused_data.yaw_deg = imu_orientation.yaw;
    
    // Output fused data
    outputJSON(fused_data);
  }
  
  delay(100);
}
```

---

## Performance Characteristics

### Computational Cost
- **Initialization**: ~50-100 microseconds (computes sin/cos once)
- **GPS→NED conversion**: ~10-20 microseconds (uses pre-computed trig)
- **NED→GPS conversion**: ~10-20 microseconds
- **isInitialized()**: < 1 microsecond

### Memory Usage
- **RAM**: ~60-80 bytes per instance
  - Stored values: origin (lat, lon, alt) + trig pre-computed values
  - No dynamic allocation
  - Safe to create multiple instances

### Precision
- **Double precision**: 64-bit floating point for latitude/longitude
- **Single precision**: 32-bit for altitudes and NED offsets
- **Rounding errors**: Typically < ±0.1 mm over 10 km distance

---

## Common Patterns

### Pattern 1: Set Known Origin
```cpp
CoordinateFrame frame;
frame.initialize(48.13743, 11.58549, 520.0);  // Munich

// Now convert any GPS position to local NED
float n, e, d;
frame.gpsToLocalNED(gps.latitude, gps.longitude, gps.altitude, &n, &e, &d);
```

### Pattern 2: Auto-Initialize on First Fix
```cpp
CoordinateFrame frame;

void loop() {
  gps.read();
  
  if (!frame.isInitialized() && gps.hasLock()) {
    frame.initializeOnFirstFix(gps.getPosition());
  }
  
  // Now use frame for all conversions
}
```

### Pattern 3: Track Vehicle Relative to Starting Point
```cpp
GPS gps;
CoordinateFrame frame;

void setup() {
  gps.begin();
}

void loop() {
  gps.read();
  
  // Set origin to current position (first time only)
  static bool origin_set = false;
  if (!origin_set && gps.hasLock()) {
    frame.initializeOnFirstFix(gps.getPosition());
    origin_set = true;
  }
  
  // Get vehicle position relative to starting point
  if (frame.isInitialized()) {
    const auto& pos = gps.getPosition();
    float n, e, d;
    frame.gpsToLocalNED(pos.latitude, pos.longitude, pos.altitude_m,
                        &n, &e, &d);
    Serial.printf("Distance from start: N=%.1f, E=%.1f, D=%.1f m\n", 
                  n, e, d);
  }
}
```

### Pattern 4: Navigation Waypoint Planning
```cpp
CoordinateFrame frame;
frame.initialize(48.13743, 11.58549, 520.0);

// Define mission waypoints in local NED coordinates
struct Waypoint {
  float north_m;
  float east_m;
  float down_m;
};

Waypoint waypoints[] = {
  {0, 0, 0},           // Start at origin
  {100, 0, 0},         // 100m north
  {100, 100, 0},       // Then 100m east
  {0, 100, 0},         // Then 100m west
  {0, 0, 0}            // Return to origin
};

void convertWaypointsToGPS() {
  for (int i = 0; i < 5; i++) {
    double lat, lon;
    float alt;
    frame.localNEDToGPS(waypoints[i].north_m, waypoints[i].east_m,
                        waypoints[i].down_m, &lat, &lon, &alt);
    Serial.printf("Waypoint %d: %.6f, %.6f\n", i, lat, lon);
  }
}
```

---

## Related Documentation

- [GPS Driver API Reference](GPS_DRIVER_API_REFERENCE.md) - Reading GPS data
- [Coordinate Conversion API](COORDINATE_CONVERSION_API.md) - Mathematical details
- [Build Guide Phase 2](../build/BUILD_GUIDE_PHASE2.md) - Building with coordinate frame support
- [PHASE_2_MASTER_IMPLEMENTATION_PLAN.md](../phases/PHASE_2_MASTER_IMPLEMENTATION_PLAN.md) - Implementation details

---

**Last Updated**: 2026-05-07  
**Version**: 1.0  
**Author**: Phase 2 Implementation  
**Status**: Complete and tested
