# GPS & Coordinate Frames: Quick Reference

*Use this guide for day-to-day development. See GPS_GEODETIC_COORDINATE_SYSTEMS.md for theory.*

---

## TL;DR: Essential Facts

| What | Value | Notes |
|------|-------|-------|
| GPS outputs | Lat/Lon (WGS84), altitude (HAE) | Always ellipsoidal height |
| Local navigation | NED frame | Industry standard for drones |
| Horizontal accuracy | ±3-5m | Typical GPS, degrades with urban canyon |
| Vertical accuracy | ±5-10m | Worse than horizontal (VDOP typically 2x HDOP) |
| Good HDOP | < 2.5 | Indicates 8+ satellites well-spaced |
| Magnetic declination | -25° to +25° | Varies by location, use NOAA calculator |
| Geoid correction | ±100m globally | Critical for MSL vs HAE conversion |

---

## GPS Data Outputs: What You Get

### From NEO-M9N Module (GPGGA Sentence)

```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47

Position:     48°07'02.28"N, 11°31'00.00"E
Altitude:     545.4m (above WGS84 ellipsoid)
Satellites:   8 (in view)
HDOP:         0.9 (excellent geometry)
Geoid sep:    46.9m (WGS84 is 46.9m above sea level here)
```

### What Each Value Means for Your Drone

```python
# Raw GPS data
lat = 48.1173°      # Latitude (positive = North)
lon = 11.5167°      # Longitude (positive = East)
alt_hae = 545.4m    # Height Above Ellipsoid (GPS native)

# For flight control, you need:
alt_msl = 545.4 - 46.9 = 498.5m    # Navigation altitude (regulatory)
alt_agl = 498.5 - terrain = ???      # Obstacle avoidance (critical)

# Accuracy indicators
hdop = 0.9          # Horizontal accuracy ±2.7m (3 × HDOP)
vdop = ???          # Usually higher than HDOP
satellites = 8      # More is better (8+ is good)
```

---

## Coordinate Frame Decision Tree

```
START: I need to convert GPS data
│
├─ "I want local navigation (relative position)" 
│  └─> Use NED frame
│      • X = North (forward)
│      • Y = East (right)
│      • Z = Down (gravity)
│      └─> Code: transformer.ecef_to_ned()
│
├─ "I want robot mapping (3D point cloud)" 
│  └─> Use ENU frame
│      • X = East
│      • Y = North
│      • Z = Up (opposite gravity)
│      └─> Code: transformer.ecef_to_enu()
│
├─ "I want body-aligned coordinates (pitch/roll/yaw)"
│  └─> First get NED, then rotate by attitude
│      └─> Code: ned_to_body(roll, pitch, yaw)
│
└─ "I'm working with satellites/ECEF"
   └─> Keep in Earth-Centered Earth-Fixed
       └─> Code: transformer.geodetic_to_ecef()
```

---

## Quick Conversion Recipes

### Recipe 1: GPS → Local NED Position

```python
from CoordinateTransformer import CoordinateTransformer

# Your reference point (e.g., launch site)
ref_lat, ref_lon, ref_alt = 47.123, 11.456, 500.0

# Current drone position from GPS
drone_lat, drone_lon, drone_alt = 47.124, 11.457, 520.0

# Transform
tf = CoordinateTransformer()
ref_ecef = tf.geodetic_to_ecef(ref_lat, ref_lon, ref_alt)
drone_ecef = tf.geodetic_to_ecef(drone_lat, drone_lon, drone_alt)

north, east, down = tf.ecef_to_ned(ref_lat, ref_lon, ref_alt, drone_ecef)

print(f"Drone is {north:.1f}m north, {east:.1f}m east, {down:.1f}m down")
# Expected: ~111m north, ~91m east, -20m down (20m above reference)
```

### Recipe 2: Magnetometer → True Heading

```python
import math
from NOAA_MagneticModel import get_declination

# Get location from GPS
lat, lon = 47.123, 11.456
declination = get_declination(lat, lon)  # Returns degrees (< 0 = west)

# Your magnetometer (after calibration)
mag_north = 0.8
mag_east = 0.2

# Calculate heading
mag_heading_rad = math.atan2(mag_east, mag_north)
mag_heading_deg = math.degrees(mag_heading_rad) % 360

# Apply declination
true_heading = (mag_heading_deg + declination) % 360

print(f"Magnetic: {mag_heading_deg:.1f}°, True: {true_heading:.1f}°")
# If declination is -12° (West), add 12° to convert magnetic → true
```

### Recipe 3: GPS Altitude → MSL & AGL

```python
# GPS gives you ellipsoidal height
gps_alt_hae = 545.4  # meters

# Get geoid undulation for location (use NOAA)
lat, lon = 48.117, 11.517
geoid_undulation = -46.9  # meters (varies by location)

# Convert to MSL
alt_msl = gps_alt_hae - geoid_undulation
print(f"MSL altitude: {alt_msl:.1f}m")  # 545.4 - (-46.9) = 592.3m

# For AGL, you need terrain elevation (e.g., from DEM)
terrain_elev = 500.0  # meters
alt_agl = alt_msl - terrain_elev
print(f"AGL altitude: {alt_agl:.1f}m")  # 592.3 - 500 = 92.3m (above ground)
```

### Recipe 4: Accuracy Assessment

```python
# From GPS NMEA data
hdop = 0.9
vdop = 1.5
satellites = 11
fix_quality = 1  # 1=GPS, 4=RTK fixed

# Estimate accuracy in meters
horizontal_error = 3.0 * hdop  # ±2.7m for this example
vertical_error = 3.0 * vdop    # ±4.5m for this example

print(f"Horizontal: ±{horizontal_error:.1f}m (HDOP={hdop})")
print(f"Vertical: ±{vertical_error:.1f}m (VDOP={vdop})")
print(f"Satellites: {satellites} (8+ is good)")

# Decision logic for drone control
if hdop < 2.0 and satellites >= 8:
    print("✓ Safe for autonomous navigation")
elif hdop < 5.0 and satellites >= 6:
    print("⚠ Acceptable but monitor, avoid RTK applications")
else:
    print("✗ Too inaccurate, hover or land")
```

---

## Common Coordinate Frame Mistakes

### ❌ Mistake 1: Using HAE for Obstacle Avoidance

```python
# WRONG
if gps_altitude_hae > 100:
    avoid_obstacle()  # Incorrect! HAE varies with geoid

# RIGHT
geoid_undulation = get_geoid_for_location(lat, lon)
alt_msl = gps_altitude_hae - geoid_undulation
if alt_msl > 100:
    avoid_obstacle()  # Correct with respect to gravity
```

### ❌ Mistake 2: Mixing Reference Frames

```python
# WRONG
north = gps_lat * 111000  # DON'T DO THIS
east = gps_lon * 111000 * cos(lat)
# Latitude isn't in meters and changes with longitude

# RIGHT
transformer.ecef_to_ned(ref_lat, ref_lon, ref_alt, drone_ecef)
# Proper geodetic to local transformation
```

### ❌ Mistake 3: Ignoring Magnetic Declination

```python
# WRONG
true_heading = magnetometer_heading  # Magnetometer points to MAGNETIC north

# RIGHT
declination = get_declination(lat, lon)
true_heading = magnetometer_heading + declination
# Now points to TRUE north (fixes map alignment)
```

### ❌ Mistake 4: GPS Accuracy Too Optimistic

```python
# WRONG
if hdop < 10:
    print("Good enough")  # No! HDOP=10 means ±30m error

# RIGHT
if hdop < 2.0:
    print("Good")     # ±6m
elif hdop < 5.0:
    print("Fair")     # ±15m
else:
    print("Poor")     # Very inaccurate
```

---

## Debugging Checklist

### GPS Position Looks Wrong?

- [ ] Check `fix_quality` field (should be 1 or higher)
- [ ] Verify satellite count (need 4+ for 2D, 5+ for 3D)
- [ ] Check HDOP (< 5.0 is minimum acceptable)
- [ ] Is antenna outside building? (multipath causes large errors)
- [ ] Did you convert DMS correctly? (easy mistake: minutes ÷ 60, not 100)

### Altitude Conversion Seems Off?

- [ ] Confirm you're using geoid_undulation correctly: `alt_msl = alt_hae - N`
- [ ] Are you using the right geoid model? (EGM96, EGM2008, or EGM2020?)
- [ ] Check terrain elevation source is accurate for location
- [ ] Is altitude in meters? (some systems use feet)

### Heading Doesn't Match GPS Course?

- [ ] Did you calibrate the magnetometer? (offset/scale matter!)
- [ ] Are you using correct declination for location? (use NOAA tool)
- [ ] GPS course is over-ground, magnetometer is instantaneous heading
- [ ] Magnetometer needs ~100ms averaging to match GPS quality

### NED Position Drifts Over Time?

- [ ] GPS drift is normal (HDOP variation)
- [ ] Large jumps indicate poor fix or multipath
- [ ] Add Kalman filter to smooth GPS position
- [ ] If reference point moved, recalculate all NED vectors

---

## Data Sheet Reference

### NEO-M9N GPS Module

```
Output:          NMEA 0183 (GPGGA, GPRMC, etc)
Update rate:     1-10 Hz (typically 1 Hz)
Position accuracy: ±2.5m (95%)
Altitude accuracy: ±2.5m (95%) 
Horizontal DOP:  Typical 1.2-3.0
Vertical DOP:    Typical 2.0-5.0
Time to first fix: Cold start 35s, warm start 1s
```

### HDOP/VDOP Interpretation

```
Value   Horizontal    Vertical    Assessment
<1.0    ±3m           ±5m         Excellent (rare)
1.5     ±4.5m         ±7.5m       Very Good
2.0     ±6m           ±10m        Good (8+ satellites)
3.0     ±9m           ±15m        Fair (urban)
5.0     ±15m          ±25m        Poor (multipath)
>10     ±30m+         ±50m+       Unacceptable
```

### Geoid Undulation by Region

```
Region              N (meters)    Note
New Guinea          +80           Ellipsoid far above geoid
Northern Europe     +40-60        Typical
North America       -20 to -40    Varies by region
Southern India      -100          Ellipsoid far below geoid
Pacific Ocean       ~0            Average
```

---

## Tools & Resources

### Online Tools
- **NOAA Declination Calculator:** https://www.ncei.noaa.gov/geomag-web/
- **Geoid Height Calculator:** https://www.unavco.org/software/geodetic-utilities/geoid-height-calculator/
- **ECEF Converter:** https://convertecef.com/

### Python Libraries
```bash
# Coordinate transformations
pip install pyproj           # Professional geodetic library
pip install geographiclib    # For precise calculations

# Magnetic field
pip install igrfmodel        # World Magnetic Model 2020
pip install wmm2020          # Alternative WMM implementation

# DEM / Terrain
pip install rasterio         # Read GeoTIFFs (for geoid, DEM)
pip install elevation        # Download SRTM data
```

### Quick Python Snippets

```python
# Proper WGS84 conversion using pyproj
import pyproj

wgs84 = pyproj.Proj(proj='latlong', ellps='WGS84')
ecef = pyproj.Proj(proj='geocentric', ellps='WGS84')

transformer = pyproj.Transformer.from_proj(wgs84, ecef)
x, y, z = transformer.transform(lon, lat, alt)

# Get geoid height using geographiclib
from geographiclib.geodesic import Geodesic
geoid = Geodesic.WGS84
h = geoid.Planimeter(lat1=0, lon1=0, lat2=lat, lon2=lon)  # Simplified

# Get magnetic declination using igrfmodel
from igrfmodel import igrf
dec = igrf.declination(latitude=lat, longitude=lon, height=alt)
```

---

## Standards Reference

### Altitude Terms (FAA/ICAO)

| Term | Definition | Use |
|------|-----------|-----|
| **HAE** | Height Above WGS84 Ellipsoid | GPS native, intermediate |
| **MSL** | Mean Sea Level (geoid) | Aviation altimeter, charts |
| **AGL** | Above Ground Level | Terrain clearance, landing |
| **QNH** | Pressure altitude setting | Aviation only |
| **FL** | Flight Level (QNH = 1013.25 hPa) | Aviation cruise altitudes |

### Frame Orientation Conventions

**NED (Aviation Standard)**
- X = North (forward)
- Y = East (right)
- Z = Down (gravity)
- Roll positive = right wing down
- Pitch positive = nose up
- Yaw positive = clockwise (viewed from above)

**ENU (Robotics Common)**
- X = East (right)
- Y = North (forward)
- Z = Up (opposite gravity)

**Body Frame (Aircraft)**
- X = Forward (nose direction)
- Y = Right wing
- Z = Down (belly)

---

## Version & Updates

**Quick Reference Version:** 1.0  
**Last Updated:** 2026-05-07  
**Companion Document:** GPS_GEODETIC_COORDINATE_SYSTEMS.md  

*Keep this tab open while developing! Print the tables for field testing.*
