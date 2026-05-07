# GPS and Geodetic Coordinate Systems for Aerial Platforms

## Table of Contents

1. [GPS Data Outputs](#gps-data-outputs)
2. [Geodetic Coordinate Systems](#geodetic-coordinate-systems)
3. [Coordinate Frame Transformations](#coordinate-frame-transformations)
4. [Magnetic vs True North](#magnetic-vs-true-north)
5. [Altitude Types](#altitude-types)
6. [Implementation Guide](#implementation-guide)
7. [Practical Examples](#practical-examples)

---

## GPS Data Outputs

### Standard GPS Module Outputs

Commercial GPS modules (like the NEO-M9N) provide the following raw data through NMEA 0183 sentences:

#### **Basic Position Data (GPGGA Sentence)**

| Parameter | Description | Units | Typical Accuracy |
|-----------|-------------|-------|------------------|
| Latitude | Position north/south of equator | DD.ddddd (degrees) | ±3-5 meters |
| Longitude | Position east/west of prime meridian | DD.ddddd (degrees) | ±3-5 meters |
| Altitude (HAE) | Height Above Ellipsoid (WGS84) | meters | ±5-10 meters |
| UTC Time | Timestamp of fix | hhmmss.ss | microseconds |
| Fix Quality | 0=Invalid, 1=GPS, 2=DGPS, 4=RTK | enum | varies by type |
| Satellites | Number of satellites used | count | typically 8-12 |
| HDOP | Horizontal Dilution of Precision | unitless | measure of geometry |
| Geoid Separation | Height difference WGS84→MSL | meters | ±2-3 meters |

#### **Navigation Data (GPRMC Sentence)**

| Parameter | Description | Units |
|-----------|-------------|-------|
| Latitude | Current position (redundant with GPGGA) | DD.ddddd |
| Longitude | Current position (redundant with GPGGA) | DD.ddddd |
| Speed Over Ground | Velocity magnitude | knots (0.514 m/s) |
| Course Over Ground | True heading | degrees (0-360°) |
| Status | A=Valid, V=Invalid | flag |

### Accuracy Metrics (DOP Values)

**Dilution of Precision (DOP)** quantifies how satellite geometry affects position accuracy:

| Metric | Measures | Formula | Interpretation |
|--------|----------|---------|-----------------|
| **HDOP** | Horizontal accuracy | Geometric factor × receiver noise | ≤ 2.5 is good |
| **VDOP** | Vertical accuracy | Geometric factor × receiver noise | ≤ 2.5 is good |
| **PDOP** | Position (3D) | √(HDOP² + VDOP²) | ≤ 5 is good |
| **GDOP** | Including time | √(PDOP² + TDOP²) | ≤ 8 is good |
| **TDOP** | Time accuracy | Receiver clock quality | rarely reported |

**Interpretation:**

```
HDOP/VDOP Values:
< 1.0   → Ideal (rarely achieved)
1.0-2.0 → Excellent
2.0-5.0 → Good
5.0-10.0→ Moderate (usable for drones)
> 10    → Poor (avoid for precision work)
```

Standard GPS achieves **±3m horizontal, ±5m vertical** 95% of the time under good conditions.

---

## Geodetic Coordinate Systems

### Overview

Modern navigation uses multiple coordinate reference systems:

1. **WGS84 (Geodetic)** - Latitude/Longitude/Altitude - GPS native output
2. **ECEF** - Earth-Centered Earth-Fixed - Cartesian coordinates
3. **Local Tangent Plane** - NED/ENU - Aircraft/drone navigation

### WGS84 Geodetic System

WGS84 (World Geodetic System 1984) defines Earth as an oblate ellipsoid:

**Key Parameters:**

- Semi-major axis (equatorial): $a = 6,378,137$ m
- Semi-minor axis (polar): $b = 6,356,752.3145$ m
- Flattening: $f = \frac{a-b}{a} = 1/298.257223563$
- Eccentricity squared: $e^2 = 2f - f^2 \approx 0.00669437999014$

**Coordinates:**
- $\phi$ = Latitude (-90° to +90°, positive north)
- $\lambda$ = Longitude (-180° to +180°, positive east)
- $h$ = Height above WGS84 ellipsoid (meters)

### ECEF (Earth-Centered, Earth-Fixed) System

Cartesian coordinates with origin at Earth's center:

- **X-axis**: Points to intersection of prime meridian and equator
- **Y-axis**: Points 90° east (perpendicular to X in equatorial plane)
- **Z-axis**: Points to North Pole

#### **Geodetic to ECEF Conversion**

Convert WGS84 (φ, λ, h) to ECEF (x, y, z):

**Step 1: Calculate radius of curvature in prime vertical**

$$N(\phi) = \frac{a}{\sqrt{1 - e^2 \sin^2(\phi)}}$$

**Step 2: Compute Cartesian coordinates**

$$x = (N + h) \cos(\phi) \cos(\lambda)$$

$$y = (N + h) \cos(\phi) \sin(\lambda)$$

$$z = (N(1 - e^2) + h) \sin(\phi)$$

**Pseudocode:**

```python
def geodetic_to_ecef(lat_deg, lon_deg, altitude_m):
    """Convert WGS84 to ECEF coordinates."""
    # WGS84 parameters
    a = 6378137.0  # Semi-major axis (meters)
    e2 = 0.00669437999014  # Eccentricity squared
    
    # Convert to radians
    lat_rad = math.radians(lat_deg)
    lon_rad = math.radians(lon_deg)
    
    # Radius of curvature in prime vertical
    N = a / math.sqrt(1 - e2 * math.sin(lat_rad)**2)
    
    # ECEF coordinates
    x = (N + altitude_m) * math.cos(lat_rad) * math.cos(lon_rad)
    y = (N + altitude_m) * math.cos(lat_rad) * math.sin(lon_rad)
    z = (N * (1 - e2) + altitude_m) * math.sin(lat_rad)
    
    return (x, y, z)
```

#### **ECEF to Geodetic Conversion**

Convert ECEF (x, y, z) back to WGS84 (φ, λ, h):

**Step 1: Calculate longitude**

$$\lambda = \text{atan2}(y, x)$$

**Step 2: Use iterative method for latitude and altitude**

```python
def ecef_to_geodetic(x, y, z):
    """Convert ECEF to WGS84 coordinates."""
    a = 6378137.0
    e2 = 0.00669437999014
    b = a * (1 - e2)**0.5
    
    # Longitude
    lon_rad = math.atan2(y, x)
    
    # Latitude (iterative method)
    p = math.sqrt(x**2 + y**2)
    lat_rad = math.atan2(z, p * (1 - e2))
    
    # Iterate 3-4 times for convergence
    for _ in range(4):
        N = a / math.sqrt(1 - e2 * math.sin(lat_rad)**2)
        h = p / math.cos(lat_rad) - N
        lat_rad = math.atan2(z, p * (1 - e2 * N / (N + h)))
    
    # Final values
    N = a / math.sqrt(1 - e2 * math.sin(lat_rad)**2)
    h = p / math.cos(lat_rad) - N
    
    lat_deg = math.degrees(lat_rad)
    lon_deg = math.degrees(lon_rad)
    
    return (lat_deg, lon_deg, h)
```

---

## Coordinate Frame Transformations

### Local Tangent Plane Frames

For aerial navigation, we use local coordinate frames centered at the vehicle. Three conventions exist:

#### **1. NED (North-East-Down)**

```
     ↑ North (X-axis)
     |
     o------→ East (Y-axis)
    /
   ↙ Down (Z-axis, into Earth)
```

- **X-axis (North)**: Points toward geographic north pole along surface normal
- **Y-axis (East)**: Points east, perpendicular to north in horizontal plane
- **Z-axis (Down)**: Points downward, away from zenith

**Standard in:** Aviation, robotics, military applications

#### **2. ENU (East-North-Up)**

```
     ↑ Up (Z-axis)
     |
     o------→ East (X-axis)
    /
   ↙ North (Y-axis)
```

- **X-axis (East)**: Points east
- **Y-axis (North)**: Points north
- **Z-axis (Up)**: Points upward (opposite gravity)

**Standard in:** Mapping, GIS, some robotics systems

#### **3. NEU (North-East-Up)**

Less common, combines North and East axes with Up (rarely used in practice).

### ECEF to NED Transformation

**Step 1: Establish reference point (origin)**

Choose a reference location as the origin for the local frame:
- $\phi_0, \lambda_0, h_0$ = Reference latitude, longitude, altitude
- $P_0 = (x_0, y_0, z_0)$ = Reference point in ECEF

**Step 2: Compute rotation matrix from ECEF to NED**

The rotation matrix transforms ECEF coordinates to NED. It's composed of two rotations:
- First rotation: around Z-axis by -longitude
- Second rotation: around Y-axis by -(π/2 - latitude)

$$R_{ECEF \to NED} = \begin{pmatrix}
-\sin(\phi) \cos(\lambda) & -\sin(\lambda) & -\cos(\phi) \cos(\lambda) \\
-\sin(\phi) \sin(\lambda) & \cos(\lambda) & -\cos(\phi) \sin(\lambda) \\
\cos(\phi) & 0 & -\sin(\phi)
\end{pmatrix}$$

**Step 3: Transform position**

For a point $P_{ECEF}$ (any point in ECEF coordinates):

1. Compute relative position vector:
   $$\Delta P = P_{ECEF} - P_0$$

2. Apply rotation:
   $$\begin{pmatrix} N \\ E \\ D \end{pmatrix} = R_{ECEF \to NED} \cdot \Delta P$$

**Pseudocode:**

```python
import math

def ecef_to_ned(lat0_deg, lon0_deg, alt0_m, point_ecef):
    """
    Transform ECEF point to NED relative to reference location.
    
    Args:
        lat0_deg, lon0_deg, alt0_m: Reference location in geodetic
        point_ecef: (x, y, z) in ECEF
    
    Returns:
        (north, east, down) in local NED frame
    """
    # Convert reference to ECEF
    ref_ecef = geodetic_to_ecef(lat0_deg, lon0_deg, alt0_m)
    
    # Convert degrees to radians
    lat_rad = math.radians(lat0_deg)
    lon_rad = math.radians(lon0_deg)
    
    # Relative position in ECEF
    dx = point_ecef[0] - ref_ecef[0]
    dy = point_ecef[1] - ref_ecef[1]
    dz = point_ecef[2] - ref_ecef[2]
    
    # Rotation matrix components
    sin_lat = math.sin(lat_rad)
    cos_lat = math.cos(lat_rad)
    sin_lon = math.sin(lon_rad)
    cos_lon = math.cos(lon_rad)
    
    # Apply rotation matrix (ECEF to NED)
    north = -sin_lat * cos_lon * dx - sin_lat * sin_lon * dy + cos_lat * dz
    east = -sin_lon * dx + cos_lon * dy
    down = -cos_lat * cos_lon * dx - cos_lat * sin_lon * dy - sin_lat * dz
    
    return (north, east, down)
```

### ECEF to ENU Transformation

ENU is related to NED by a 180° rotation about the north axis:

$$\begin{pmatrix} E \\ N \\ U \end{pmatrix} = \begin{pmatrix} 
0 & 1 & 0 \\
1 & 0 & 0 \\
0 & 0 & -1
\end{pmatrix} \begin{pmatrix} N \\ E \\ D \end{pmatrix}$$

Or directly from ECEF:

$$R_{ECEF \to ENU} = \begin{pmatrix}
-\sin(\lambda) & \cos(\lambda) & 0 \\
-\sin(\phi) \cos(\lambda) & -\sin(\phi) \sin(\lambda) & \cos(\phi) \\
\cos(\phi) \cos(\lambda) & \cos(\phi) \sin(\lambda) & \sin(\phi)
\end{pmatrix}$$

**Pseudocode:**

```python
def ecef_to_enu(lat0_deg, lon0_deg, alt0_m, point_ecef):
    """Transform ECEF to ENU (East-North-Up) frame."""
    # Get NED coordinates first
    north, east, down = ecef_to_ned(lat0_deg, lon0_deg, alt0_m, point_ecef)
    
    # Convert NED to ENU
    enu_east = east
    enu_north = north
    enu_up = -down
    
    return (enu_east, enu_north, enu_up)
```

### NED to Body Frame (Aircraft Body)

For aerial vehicles, we further transform from NED to the aircraft body frame using Euler angles:

$$\begin{pmatrix} x_{body} \\ y_{body} \\ z_{body} \end{pmatrix} = R_z(\psi) \cdot R_y(\theta) \cdot R_x(\phi) \cdot \begin{pmatrix} n \\ e \\ d \end{pmatrix}$$

Where:
- $\phi$ = Roll (rotation about X-axis, positive right wing down)
- $\theta$ = Pitch (rotation about Y-axis, positive nose up)
- $\psi$ = Yaw (rotation about Z-axis, positive clockwise viewed from above)

**Rotation matrices:**

$$R_x(\phi) = \begin{pmatrix}
1 & 0 & 0 \\
0 & \cos(\phi) & \sin(\phi) \\
0 & -\sin(\phi) & \cos(\phi)
\end{pmatrix}$$

$$R_y(\theta) = \begin{pmatrix}
\cos(\theta) & 0 & -\sin(\theta) \\
0 & 1 & 0 \\
\sin(\theta) & 0 & \cos(\theta)
\end{pmatrix}$$

$$R_z(\psi) = \begin{pmatrix}
\cos(\psi) & \sin(\psi) & 0 \\
-\sin(\psi) & \cos(\psi) & 0 \\
0 & 0 & 1
\end{pmatrix}$$

**Combined NED to Body transformation:**

```python
def ned_to_body(north, east, down, roll_rad, pitch_rad, yaw_rad):
    """Transform NED coordinates to aircraft body frame."""
    # Compute rotation matrices
    c_roll = math.cos(roll_rad)
    s_roll = math.sin(roll_rad)
    c_pitch = math.cos(pitch_rad)
    s_pitch = math.sin(pitch_rad)
    c_yaw = math.cos(yaw_rad)
    s_yaw = math.sin(yaw_rad)
    
    # Combined rotation matrix (ZYX Euler angles)
    # This represents rotation from NED to body frame
    x_body = (c_yaw * c_pitch) * north + (c_yaw * s_pitch * s_roll - s_yaw * c_roll) * east + (c_yaw * s_pitch * c_roll + s_yaw * s_roll) * down
    y_body = (s_yaw * c_pitch) * north + (s_yaw * s_pitch * s_roll + c_yaw * c_roll) * east + (s_yaw * s_pitch * c_roll - c_yaw * s_roll) * down
    z_body = (-s_pitch) * north + (c_pitch * s_roll) * east + (c_pitch * c_roll) * down
    
    return (x_body, y_body, z_body)
```

---

## Magnetic vs True North

### Concepts

**True North (Geographic North):**
- Direction toward the geographic North Pole (rotation axis of Earth)
- Fixed reference for navigation
- Used by GPS and surveying systems

**Magnetic North:**
- Direction toward the magnetic north pole (dipole axis)
- Varies with location and time
- Used by compass and magnetometer sensors

**Magnetic Declination (Variation):**
- Angle between true north and magnetic north
- Positive = magnetic north is east of true north (easterly declination)
- Negative = magnetic north is west of true north (westerly declination)
- Typically ±25° depending on location
- Changes ~0.1-0.25° per year due to Earth's core dynamics

### Declination by Location (2024)

| Location | Declination | Trend |
|----------|-------------|-------|
| San Francisco, CA | 11° 53' E | Increasing |
| New York, NY | 12° 15' W | Decreasing |
| London, UK | 0° 43' W | Changing to East |
| Tokyo, Japan | 8° 00' W | Increasing westward |
| Sydney, Australia | 12° 24' E | Decreasing |

**Current declination varies by location: -25° to +25°**

### Magnetometer to True Heading Conversion

**Given:**
- Magnetic heading $H_m$ from magnetometer (0-360°)
- Magnetic declination $D$ at location (negative west, positive east)

**Conversion formula:**

$$H_{true} = H_m + D$$

**Example:**
- Magnetometer reads 40° (magnetic north)
- Location has declination -14° (14° west)
- True heading = 40° + (-14°) = 26° (true north)

### Implementation with Calibrated Magnetometer

```python
def magnetometer_to_true_heading(mag_x, mag_y, declination_deg):
    """
    Convert magnetometer readings to true heading.
    
    Args:
        mag_x, mag_y: Calibrated magnetometer X, Y components (East, North)
        declination_deg: Local magnetic declination (negative = west)
    
    Returns:
        true_heading_deg: Heading relative to true north (0-360°)
    """
    # Calculate magnetic heading (from magnetometer X,Y)
    # Note: X = East, Y = North in sensor frame
    mag_heading_rad = math.atan2(mag_x, mag_y)
    mag_heading_deg = math.degrees(mag_heading_rad)
    
    # Normalize to 0-360°
    mag_heading_deg = mag_heading_deg % 360
    
    # Apply declination correction
    true_heading_deg = (mag_heading_deg + declination_deg) % 360
    
    return true_heading_deg

def get_local_declination(lat_deg, lon_deg, date=None):
    """
    Get magnetic declination for a location.
    
    Real implementation requires NOAA or WMM data.
    For accuracy, use:
    - NOAA Magnetic Declination Calculator
    - World Magnetic Model (WMM)
    - igrfmodel Python package
    
    This is a simplified approximation only.
    """
    # This is simplified; use NOAA data in production
    # https://www.ngdc.noaa.gov/geomag-web/
    # Return declination in degrees (negative = west)
    pass
```

### Using NOAA Declination Data

For production applications, use NOAA's World Magnetic Model (WMM):

1. **Online calculator:** https://www.ngdc.noaa.gov/geomag-web/
2. **Python package:** `igrfmodel` or `wmm2020`
3. **Data files:** WMM coefficients updated annually

---

## Altitude Types

### Three Altitude Reference Systems

#### **1. WGS84 Ellipsoidal Height (HAE)**

**Definition:** Height above the WGS84 reference ellipsoid

**Properties:**
- Directly measured by GPS satellites
- Doesn't follow gravity (mathematical ellipsoid)
- Varies smoothly across Earth's surface
- No local topography dependence

**Accuracy:** ±5-10 meters (typical GPS)

**Use cases:** Satellite positioning, antenna placement, mathematical reference

#### **2. MSL (Mean Sea Level) or Orthometric Height**

**Definition:** Height above the geoid (mean sea level adjusted for gravity)

**Properties:**
- Follows equipotential surface of gravity
- Related to actual water levels
- Used for aviation, mapping
- Requires geoid model to compute

**Relationship:** 

$$h_{MSL} = h_{HAE} - N$$

Where $N$ is the **geoid undulation** (height difference WGS84→geoid)

**Geoid undulation varies by location:**
- Southern India: $N \approx -100$ m (ellipsoid below geoid)
- New Guinea: $N \approx +80$ m (ellipsoid above geoid)
- US average: $N \approx -30$ m
- Much of Europe: $N \approx +40$ m

#### **3. AGL (Above Ground Level)**

**Definition:** Height above local terrain

**Properties:**
- Most important for obstacle avoidance
- Requires terrain elevation data
- Relative to Earth's surface features

**Relationship:**

$$h_{AGL} = h_{MSL} - h_{terrain}$$

Or with ellipsoidal:

$$h_{AGL} = h_{HAE} - N - h_{terrain}$$

### Conversion Strategy

**GPS provides:** $h_{HAE}$ (WGS84 ellipsoidal height)

**To get MSL:**
1. Obtain geoid undulation $N$ for your location
2. Compute: $h_{MSL} = h_{HAE} - N$
3. Use geoid model: EGM96, EGM2008, EGM2020 (US NOAA provides)

**To get AGL:**
1. Get terrain elevation $h_{terrain}$ from DEM (Digital Elevation Model)
2. Compute: $h_{AGL} = h_{MSL} - h_{terrain}$
   Or: $h_{AGL} = h_{HAE} - N - h_{terrain}$

### Practical Implementation

```python
def compute_altitudes(gps_latitude_deg, gps_longitude_deg, gps_altitude_hae_m):
    """
    Compute MSL and AGL altitudes from GPS ellipsoidal height.
    
    Args:
        gps_latitude_deg: Latitude from GPS
        gps_longitude_deg: Longitude from GPS
        gps_altitude_hae_m: GPS altitude (HAE/WGS84 ellipsoidal)
    
    Returns:
        dict with altitudes in meters
    """
    # Step 1: Get geoid undulation for location
    # Real implementation uses EGM model or online service
    N = get_geoid_undulation(gps_latitude_deg, gps_longitude_deg)
    
    # Step 2: Compute MSL
    altitude_msl = gps_altitude_hae_m - N
    
    # Step 3: Get terrain elevation (requires DEM data)
    # For demo, assume some known terrain elevation
    terrain_elevation = get_terrain_elevation(gps_latitude_deg, gps_longitude_deg)
    
    # Step 4: Compute AGL
    altitude_agl = altitude_msl - terrain_elevation
    
    return {
        'altitude_hae': gps_altitude_hae_m,  # GPS raw output
        'altitude_msl': altitude_msl,         # Navigation reference
        'altitude_agl': altitude_agl,         # Obstacle avoidance
        'geoid_undulation': N,
        'terrain_elevation': terrain_elevation
    }

# Geoid undulation lookup (simplified)
def get_geoid_undulation(lat_deg, lon_deg):
    """
    Get geoid undulation from EGM model.
    
    In production, use:
    - NOAA's Geoid Height Calculator
    - GeographicLib
    - rasterio + geoid GeoTIFF files
    """
    # Simplified approximation (NOT accurate for production)
    # In practice, use gridded geoid data (EGM96, EGM2008, EGM2020)
    # For demo purposes only:
    return -35.0  # Example: 35m below ellipsoid (US typical)

def get_terrain_elevation(lat_deg, lon_deg):
    """Get terrain elevation from DEM."""
    # Use SRTM, GEBCO, or similar DEM
    # Returns height above sea level in meters
    pass
```

### GPS Altitude Accuracy Implications

| Scenario | VDOP | Vertical Error | Suitable For |
|----------|------|---|---|
| Good sky view | 1.5 | ±7.5m | Drone altitude control |
| Partial sky (forest) | 4.0 | ±20m | General navigation |
| Poor sky (urban canyon) | 8.0 | ±40m | Only rough estimates |

**For aerial platforms:**
- Use **MSL** for aviation navigation and regulatory compliance
- Use **AGL** for obstacle avoidance and landing
- HAE is intermediate for calculations
- Accuracy degradation above tree line is critical

---

## Implementation Guide

### Complete Flight Controller Integration

#### **Module 1: GPS Data Acquisition**

```python
class GPSModule:
    """Read GPS NMEA data and extract coordinates."""
    
    def parse_gpgga(self, nmea_sentence):
        """Parse GPGGA sentence to get position + altitude."""
        fields = nmea_sentence.split(',')
        
        # Extract fields
        timestamp = fields[1]
        lat_ddmm = fields[2]
        lat_dir = fields[3]
        lon_dddmm = fields[4]
        lon_dir = fields[5]
        fix_quality = int(fields[6])
        satellites = int(fields[7])
        hdop = float(fields[8])
        altitude_hae = float(fields[9])  # Above ellipsoid
        geoid_sep = float(fields[11]) if len(fields) > 11 else 0
        
        # Convert DMS to decimal degrees
        lat = self.dms_to_decimal(lat_ddmm, lat_dir)
        lon = self.dms_to_decimal(lon_dddmm, lon_dir)
        
        return {
            'lat': lat,
            'lon': lon,
            'altitude_hae': altitude_hae,
            'geoid_separation': geoid_sep,
            'fix_quality': fix_quality,
            'satellites': satellites,
            'hdop': hdop,
            'timestamp': timestamp
        }
    
    @staticmethod
    def dms_to_decimal(coord_str, direction):
        """Convert DDMM.MMMM to decimal degrees."""
        # Extract degrees and minutes
        if '.' in coord_str:
            integer, decimal = coord_str.split('.')
        else:
            integer, decimal = coord_str, '0'
        
        # Degrees are first 2 or 3 digits
        if len(integer) % 2 == 1:  # Latitude (2 digits)
            degrees = int(integer[:-2])
            minutes = int(integer[-2:])
        else:  # Longitude (3 digits)
            degrees = int(integer[:-2])
            minutes = int(integer[-2:])
        
        # Compute decimal
        decimal_deg = minutes / 60.0 + float('0.' + decimal) / 60.0
        result = degrees + decimal_deg
        
        # Apply direction
        if direction in ('S', 'W'):
            result = -result
        
        return result
```

#### **Module 2: Coordinate Transformation**

```python
class CoordinateTransformer:
    """Transform between geodetic, ECEF, and local NED/ENU frames."""
    
    # WGS84 constants
    WGS84_A = 6378137.0  # Semi-major axis (m)
    WGS84_E2 = 0.00669437999014  # Eccentricity squared
    WGS84_B = 6356752.314245  # Semi-minor axis (m)
    
    def geodetic_to_ecef(self, lat_deg, lon_deg, alt_m):
        """WGS84 → ECEF conversion."""
        lat_rad = math.radians(lat_deg)
        lon_rad = math.radians(lon_deg)
        
        N = self.WGS84_A / math.sqrt(1 - self.WGS84_E2 * math.sin(lat_rad)**2)
        
        x = (N + alt_m) * math.cos(lat_rad) * math.cos(lon_rad)
        y = (N + alt_m) * math.cos(lat_rad) * math.sin(lon_rad)
        z = (N * (1 - self.WGS84_E2) + alt_m) * math.sin(lat_rad)
        
        return np.array([x, y, z])
    
    def ecef_to_geodetic(self, x, y, z):
        """ECEF → WGS84 conversion (iterative)."""
        p = math.sqrt(x**2 + y**2)
        lon_rad = math.atan2(y, x)
        lat_rad = math.atan2(z, p * (1 - self.WGS84_E2))
        
        # Iterate for convergence
        for _ in range(4):
            N = self.WGS84_A / math.sqrt(1 - self.WGS84_E2 * math.sin(lat_rad)**2)
            h = p / math.cos(lat_rad) - N
            lat_rad = math.atan2(z, p * (1 - self.WGS84_E2 * N / (N + h)))
        
        N = self.WGS84_A / math.sqrt(1 - self.WGS84_E2 * math.sin(lat_rad)**2)
        h = p / math.cos(lat_rad) - N
        
        return (math.degrees(lat_rad), math.degrees(lon_rad), h)
    
    def ecef_to_ned(self, lat0_deg, lon0_deg, alt0_m, point_ecef):
        """ECEF → NED (relative to reference point)."""
        ref_ecef = self.geodetic_to_ecef(lat0_deg, lon0_deg, alt0_m)
        delta = point_ecef - ref_ecef
        
        lat_rad = math.radians(lat0_deg)
        lon_rad = math.radians(lon0_deg)
        
        sin_lat = math.sin(lat_rad)
        cos_lat = math.cos(lat_rad)
        sin_lon = math.sin(lon_rad)
        cos_lon = math.cos(lon_rad)
        
        # Rotation matrix: ECEF → NED
        north = -sin_lat * cos_lon * delta[0] - sin_lat * sin_lon * delta[1] + cos_lat * delta[2]
        east = -sin_lon * delta[0] + cos_lon * delta[1]
        down = -cos_lat * cos_lon * delta[0] - cos_lat * sin_lon * delta[1] - sin_lat * delta[2]
        
        return np.array([north, east, down])
    
    def ecef_to_enu(self, lat0_deg, lon0_deg, alt0_m, point_ecef):
        """ECEF → ENU (relative to reference point)."""
        north, east, down = self.ecef_to_ned(lat0_deg, lon0_deg, alt0_m, point_ecef)
        return np.array([east, north, -down])
```

#### **Module 3: Magnetic Heading Correction**

```python
class MagneticHeading:
    """Convert magnetometer readings to true heading."""
    
    @staticmethod
    def mag_to_true_heading(mag_x, mag_y, declination_deg):
        """
        Convert calibrated magnetometer X,Y to true heading.
        
        Args:
            mag_x: East component (calibrated)
            mag_y: North component (calibrated)
            declination_deg: Local declination (negative=west)
        
        Returns:
            True heading in degrees (0-360), where 0° = true north
        """
        # Magnetic heading from atan2(East, North)
        mag_heading_rad = math.atan2(mag_x, mag_y)
        mag_heading_deg = math.degrees(mag_heading_rad)
        
        # Normalize to 0-360
        mag_heading_deg = mag_heading_deg % 360
        
        # Apply declination
        true_heading_deg = (mag_heading_deg + declination_deg) % 360
        
        return true_heading_deg
    
    @staticmethod
    def true_to_mag_heading(true_heading_deg, declination_deg):
        """Reverse: true heading to magnetic heading."""
        mag_heading_deg = (true_heading_deg - declination_deg) % 360
        return mag_heading_deg
```

#### **Module 4: Altitude Computation**

```python
class AltitudeManager:
    """Manage conversion between altitude reference systems."""
    
    # Simplified geoid undulation table (use real EGM model in production)
    GEOID_TABLE = {
        # (lat_band, lon_band): undulation_m
        # Real implementation uses full grid
    }
    
    @staticmethod
    def get_geoid_undulation(lat_deg, lon_deg):
        """
        Get geoid undulation for location.
        
        In production, use EGM96, EGM2008, or EGM2020 from NOAA.
        https://www.ncei.noaa.gov/products/vertical-datums/global-geoid
        """
        # Placeholder: real implementation uses gridded data
        # For now, return approximate US value
        return -35.0
    
    def compute_msl_altitude(self, gps_altitude_hae_m, lat_deg, lon_deg):
        """Convert GPS ellipsoidal height to MSL."""
        N = self.get_geoid_undulation(lat_deg, lon_deg)
        altitude_msl = gps_altitude_hae_m - N
        return altitude_msl
    
    def compute_agl_altitude(self, altitude_msl, terrain_elevation):
        """Compute AGL from MSL and terrain elevation."""
        altitude_agl = altitude_msl - terrain_elevation
        return altitude_agl
```

---

## Practical Examples

### Example 1: GPS to Local NED

**Scenario:** Drone at latitude 37.4°N, longitude -122.1°W, altitude 100m (HAE)

```python
# Initialize
gps_data = {
    'lat': 37.4227,
    'lon': -122.1428,
    'altitude_hae': 100.5
}

reference = {
    'lat': 37.4227,
    'lon': -122.1428,
    'alt': 100.0
}

# Step 1: Transform to ECEF
transformer = CoordinateTransformer()
gps_ecef = transformer.geodetic_to_ecef(gps_data['lat'], gps_data['lon'], gps_data['altitude_hae'])
ref_ecef = transformer.geodetic_to_ecef(reference['lat'], reference['lon'], reference['alt'])

# Result (Silicon Valley):
# GPS ECEF: (-2707149.3, -4271049.5, 3886151.0) meters
# Ref ECEF: (-2707149.3, -4271049.5, 3886151.0) meters

# Step 2: Convert to NED relative to reference
ned = transformer.ecef_to_ned(reference['lat'], reference['lon'], reference['alt'], gps_ecef)

# Result: NED = (0m, 0m, 0.5m) - 0.5m above reference
```

### Example 2: Magnetometer to True Heading

**Scenario:** Magnetometer reads 45° magnetic, location has 12°W declination

```python
mag_heading_deg = 45.0
declination_deg = -12.0  # West is negative

true_heading = MagneticHeading.mag_to_true_heading(
    mag_x=1.0,  # Example calibrated reading
    mag_y=1.0,  # Example calibrated reading
    declination_deg=declination_deg
)

# Calculation:
# mag_heading = atan2(1, 1) = 45° (NE direction)
# true_heading = 45° + (-12°) = 33° (still NE but corrected to true north)
```

### Example 3: Altitude Conversions

**Scenario:** GPS reports 500m HAE in San Francisco area

```python
altitude_hae = 500.0
lat = 37.7749
lon = -122.4194
geoid_undulation = -32.0  # San Francisco typical value

# Compute MSL
altitude_msl = altitude_hae - geoid_undulation  # 500 - (-32) = 532m

# Assume terrain at 10m MSL
terrain_elevation = 10.0
altitude_agl = altitude_msl - terrain_elevation  # 532 - 10 = 522m AGL

print(f"GPS (HAE): {altitude_hae}m")
print(f"Navigation (MSL): {altitude_msl}m")
print(f"Obstacle avoidance (AGL): {altitude_agl}m")
```

### Example 4: Complete Navigation Stack

```python
class DroneNavigationSystem:
    """Integrated navigation system for aerial platform."""
    
    def __init__(self, reference_lat, reference_lon, reference_alt):
        self.transformer = CoordinateTransformer()
        self.mag_heading = MagneticHeading()
        self.altitude_mgr = AltitudeManager()
        
        self.reference = {
            'lat': reference_lat,
            'lon': reference_lon,
            'alt': reference_alt
        }
    
    def update_position(self, gps_data, magnetometer_data, declination):
        """
        Update drone position and heading.
        
        Args:
            gps_data: {'lat', 'lon', 'altitude_hae', 'hdop', 'satellites'}
            magnetometer_data: {'x', 'y', 'z'} (calibrated)
            declination: local magnetic declination in degrees
        
        Returns:
            Navigation state dict
        """
        # 1. Convert GPS to ECEF
        gps_ecef = self.transformer.geodetic_to_ecef(
            gps_data['lat'], gps_data['lon'], gps_data['altitude_hae']
        )
        
        # 2. Get local NED coordinates
        ned_position = self.transformer.ecef_to_ned(
            self.reference['lat'], self.reference['lon'], self.reference['alt'],
            gps_ecef
        )
        
        # 3. Convert altitude to MSL and AGL
        altitude_msl = self.altitude_mgr.compute_msl_altitude(
            gps_data['altitude_hae'], gps_data['lat'], gps_data['lon']
        )
        
        # 4. Get true heading from magnetometer
        true_heading = self.mag_heading.mag_to_true_heading(
            magnetometer_data['y'], magnetometer_data['x'], declination
        )
        
        # Return comprehensive state
        return {
            'position_ned': ned_position,  # (north, east, down) in meters
            'altitude_hae': gps_data['altitude_hae'],
            'altitude_msl': altitude_msl,
            'heading_true': true_heading,
            'hdop': gps_data['hdop'],
            'satellites': gps_data['satellites'],
            'fix_quality': gps_data.get('fix_quality', 0),
        }
```

---

## References & Data Sources

### Standards Documents
- **WGS84 Specification** - IHO Special Publication S-32 (international maritime)
- **ICAO Annex 4** - Aeronautical Charts (aviation altitude standards)
- **FAA AC 90-100A** - U.S. Standard Atmosphere (altitude definitions)

### Coordinate Transformation Resources
- [Navipedia: ECEF to ENU Transformations](https://gssc.esa.int/navipedia/index.php/Transformations_between_ECEF_and_ENU_coordinates)
- [Fixposition: ECEF to ENU Conversion](https://docs.fixposition.com/fd/converting-from-ecef-to-enu-local-frame)
- [MATLAB Aerospace Toolbox Documentation](https://www.mathworks.com/help/aeroblks/directioncosinematrixeceftoned.html)

### GPS Accuracy & DOP
- [GIS Geography: GPS Accuracy & DOP](https://gisgeography.com/gps-accuracy-hdop-pdop-gdop-multipath/)
- [GNSS.ae: Understanding Accuracy Metrics](https://gnss.ae/understanding-gnss-accuracy-metrics-pdop-hdop-and-vdop/)
- [Wikipedia: Dilution of Precision](https://en.wikipedia.org/wiki/Dilution_of_precision_(navigation))

### Magnetic Declination
- [NOAA Magnetic Declination Calculator](https://www.ncei.noaa.gov/products/magnetic-declination)
- [Mapscaping: Interactive Declination Map](https://mapscaping.com/interactive-magnetic-declination-calculator/)
- [Wikipedia: Magnetic Declination](https://en.wikipedia.org/wiki/Magnetic_declination)

### Altitude Systems & Geoid
- [NOAA Geoid Height Calculator](https://www.unavco.org/software/geodetic-utilities/geoid-height-calculator/geoid-height-calculator.html)
- [ESRI: Mean Sea Level, GPS, and the Geoid](https://www.esri.com/about/newsroom/arcuser/mean-sea-level-gps-geoid)
- [NextNav: Height Above Ellipsoid (HAE)](https://nextnav.com/hae/)

### UAV & Aerospace Standards
- [DJI Mobile SDK: Flight Control Concepts](https://developer.dji.com/mobile-sdk/documentation/introduction/flightController_concepts.html)
- [Parrot Developer Docs: Glossary](https://developer.parrot.com/docs/airsdk/glossary.html)
- [BWSI UAV: Coordinate Frames](https://bwsi-uav.github.io/website/coordinate_transforms.html)

### Academic Papers
- Koks, Don. "[Using Rotations to Build Aerospace Coordinate Systems](https://apps.dtic.mil/sti/pdfs/ADA484864.pdf)" DTIC Technical Report, 2012.
- Dargham et al. "[Euler and Quaternion Parameterization in VTOL UAV](https://www.ijais.org/research/volume9/number8/dargham-2015-ijais-451447.pdf)" International Journal of Advanced Information Systems and Applications.
- MDPI Paper (2024): "[Target Localization of a Quadrotor UAV with Multi-Level Coordinate System Transformation](https://www.mdpi.com/2079-9292/14/22/4371)"

---

## Summary Table: When to Use Each Frame

| Frame | Use Case | Advantages | Disadvantages |
|-------|----------|-----------|-----------------|
| **WGS84 (Lat/Lon)** | GPS raw data, maps | Intuitive, universal reference | Non-Cartesian, discontinuous at poles |
| **ECEF** | Satellite calculations | Cartesian, universal | Cumbersome for local navigation |
| **NED** | Aircraft/drone control | Natural for gravity-aligned navigation, standard in aviation | Down direction can confuse operators |
| **ENU** | Robotics, mapping | Intuitive up direction | Less common in aviation |
| **Body Frame** | Sensor/actuator control | Aligned with vehicle | Requires orientation data |

**Recommended for aerial platforms:** **NED relative to local origin**, computed from GPS data in real-time.

---

**Document Version:** 1.0  
**Created:** 2026-05-07  
**Last Updated:** 2026-05-07  
**Applicable To:** UAVs, drones, aerial vehicles, autonomous platforms  
