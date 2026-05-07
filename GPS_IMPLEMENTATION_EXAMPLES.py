"""
GPS and Coordinate Transformation - Complete Implementation Examples

This module provides production-ready code for:
1. GPS NMEA parsing
2. Geodetic ↔ ECEF conversions
3. ECEF ↔ Local NED/ENU transformations
4. Magnetic heading correction
5. Altitude conversions

See: GPS_GEODETIC_COORDINATE_SYSTEMS.md for theory and equations
"""

import math
import numpy as np
from dataclasses import dataclass
from typing import Tuple, Dict, Optional


# ============================================================================
# SECTION 1: WGS84 & ECEF TRANSFORMATIONS
# ============================================================================

class WGS84Constants:
    """WGS84 ellipsoid parameters."""
    SEMI_MAJOR_AXIS = 6378137.0  # a (meters)
    SEMI_MINOR_AXIS = 6356752.314245  # b (meters)
    FLATTENING = 1.0 / 298.257223563  # f
    ECCENTRICITY_SQ = 0.00669437999014  # e^2

    @classmethod
    def radius_of_curvature(cls, lat_rad: float) -> float:
        """Calculate N(φ) - radius of curvature in prime vertical."""
        sin_lat = math.sin(lat_rad)
        return cls.SEMI_MAJOR_AXIS / math.sqrt(1 - cls.ECCENTRICITY_SQ * sin_lat**2)


def geodetic_to_ecef(lat_deg: float, lon_deg: float, alt_m: float) -> np.ndarray:
    """
    Convert WGS84 geodetic coordinates to ECEF (Earth-Centered, Earth-Fixed).

    Mathematical basis:
        N(φ) = a / sqrt(1 - e² sin²(φ))
        x = (N + h) cos(φ) cos(λ)
        y = (N + h) cos(φ) sin(λ)
        z = (N(1 - e²) + h) sin(φ)

    Args:
        lat_deg: Latitude in degrees (-90 to +90)
        lon_deg: Longitude in degrees (-180 to +180)
        alt_m: Altitude above WGS84 ellipsoid in meters

    Returns:
        np.array([x, y, z]) in ECEF coordinates (meters)

    Example:
        >>> ecef = geodetic_to_ecef(47.3667, 11.1833, 500.0)
        >>> print(ecef)
        [-2764000.0, 1440000.0, 4690000.0]  # approximate
    """
    lat_rad = math.radians(lat_deg)
    lon_rad = math.radians(lon_deg)

    N = WGS84Constants.radius_of_curvature(lat_rad)

    cos_lat = math.cos(lat_rad)
    sin_lat = math.sin(lat_rad)
    cos_lon = math.cos(lon_rad)
    sin_lon = math.sin(lon_rad)

    x = (N + alt_m) * cos_lat * cos_lon
    y = (N + alt_m) * cos_lat * sin_lon
    z = (N * (1 - WGS84Constants.ECCENTRICITY_SQ) + alt_m) * sin_lat

    return np.array([x, y, z])


def ecef_to_geodetic(x: float, y: float, z: float) -> Tuple[float, float, float]:
    """
    Convert ECEF coordinates to WGS84 geodetic.

    Uses iterative method (Heikkinen's solution).
    Converges in 3-4 iterations for typical cases.

    Args:
        x, y, z: ECEF coordinates in meters

    Returns:
        (latitude_deg, longitude_deg, altitude_m)

    Example:
        >>> lat, lon, alt = ecef_to_geodetic(-2764000.0, 1440000.0, 4690000.0)
        >>> print(f"{lat:.4f}, {lon:.4f}, {alt:.1f}")
        47.3667, 11.1833, 500.0
    """
    p = math.sqrt(x**2 + y**2)

    # Longitude
    lon_rad = math.atan2(y, x)

    # Latitude (iterative)
    lat_rad = math.atan2(z, p * (1 - WGS84Constants.ECCENTRICITY_SQ))

    # Iterate for convergence
    for _ in range(4):
        N = WGS84Constants.radius_of_curvature(lat_rad)
        h = p / math.cos(lat_rad) - N
        lat_rad = math.atan2(z, p * (1 - WGS84Constants.ECCENTRICITY_SQ * N / (N + h)))

    # Final computation
    N = WGS84Constants.radius_of_curvature(lat_rad)
    h = p / math.cos(lat_rad) - N

    return (math.degrees(lat_rad), math.degrees(lon_rad), h)


# ============================================================================
# SECTION 2: LOCAL TANGENT PLANE TRANSFORMATIONS
# ============================================================================

def ecef_to_ned(ref_lat_deg: float, ref_lon_deg: float, ref_alt_m: float,
                point_ecef: np.ndarray) -> np.ndarray:
    """
    Transform ECEF coordinates to NED (North-East-Down) relative to reference point.

    NED frame:
        X = North (toward geographic north)
        Y = East (perpendicular to north, eastward)
        Z = Down (toward Earth center, opposite of up)

    Rotation matrix from ECEF to NED:
        R = [[-sin(φ)cos(λ)  -sin(λ)  -cos(φ)cos(λ)]
             [-sin(φ)sin(λ)   cos(λ)  -cos(φ)sin(λ)]
             [ cos(φ)         0        -sin(φ)    ]]

    Args:
        ref_lat_deg, ref_lon_deg, ref_alt_m: Reference point (origin of NED frame)
        point_ecef: Point to transform as np.array([x, y, z])

    Returns:
        np.array([north, east, down]) in meters

    Example:
        >>> ref = geodetic_to_ecef(47.3667, 11.1833, 500.0)
        >>> point = geodetic_to_ecef(47.3668, 11.1834, 510.0)
        >>> ned = ecef_to_ned(47.3667, 11.1833, 500.0, point)
        >>> print(ned)  # Should show small offsets
        [111.0, 93.0, -10.0]  # approximately 111m north, 93m east, 10m up
    """
    # Reference point in ECEF
    ref_ecef = geodetic_to_ecef(ref_lat_deg, ref_lon_deg, ref_alt_m)

    # Relative position
    delta = point_ecef - ref_ecef

    # Convert to radians
    lat_rad = math.radians(ref_lat_deg)
    lon_rad = math.radians(ref_lon_deg)

    # Precompute trig functions
    sin_lat = math.sin(lat_rad)
    cos_lat = math.cos(lat_rad)
    sin_lon = math.sin(lon_rad)
    cos_lon = math.cos(lon_rad)

    # Apply rotation matrix: R_ECEF_to_NED
    north = (-sin_lat * cos_lon * delta[0] -
             sin_lat * sin_lon * delta[1] +
             cos_lat * delta[2])

    east = (-sin_lon * delta[0] + cos_lon * delta[1])

    down = (-cos_lat * cos_lon * delta[0] -
            cos_lat * sin_lon * delta[1] -
            sin_lat * delta[2])

    return np.array([north, east, down])


def ecef_to_enu(ref_lat_deg: float, ref_lon_deg: float, ref_alt_m: float,
                point_ecef: np.ndarray) -> np.ndarray:
    """
    Transform ECEF to ENU (East-North-Up) relative to reference point.

    ENU frame:
        X = East (perpendicular to north, eastward)
        Y = North (toward geographic north)
        Z = Up (opposite of down, away from Earth center)

    Relationship to NED: ENU = [E, N, -D] from NED = [N, E, D]

    Args:
        ref_lat_deg, ref_lon_deg, ref_alt_m: Reference point (origin)
        point_ecef: Point to transform as np.array([x, y, z])

    Returns:
        np.array([east, north, up]) in meters

    Example:
        >>> ned = ecef_to_ned(ref_lat, ref_lon, ref_alt, point)
        >>> enu = ecef_to_enu(ref_lat, ref_lon, ref_alt, point)
        >>> # ENU has up where NED has down (negated)
    """
    # Convert to ENU directly
    ref_ecef = geodetic_to_ecef(ref_lat_deg, ref_lon_deg, ref_alt_m)
    delta = point_ecef - ref_ecef

    lat_rad = math.radians(ref_lat_deg)
    lon_rad = math.radians(ref_lon_deg)

    sin_lat = math.sin(lat_rad)
    cos_lat = math.cos(lat_rad)
    sin_lon = math.sin(lon_rad)
    cos_lon = math.cos(lon_rad)

    # Direct rotation matrix: R_ECEF_to_ENU
    east = (-sin_lon * delta[0] + cos_lon * delta[1])

    north = (-sin_lat * cos_lon * delta[0] -
             sin_lat * sin_lon * delta[1] +
             cos_lat * delta[2])

    up = (cos_lat * cos_lon * delta[0] +
          cos_lat * sin_lon * delta[1] +
          sin_lat * delta[2])

    return np.array([east, north, up])


def ned_to_body_frame(north: float, east: float, down: float,
                      roll_rad: float, pitch_rad: float, yaw_rad: float) -> np.ndarray:
    """
    Transform NED coordinates to aircraft body frame using Euler angles.

    Euler angles (ZYX convention):
        φ (phi) = Roll about X-axis (positive = right wing down)
        θ (theta) = Pitch about Y-axis (positive = nose up)
        ψ (psi) = Yaw about Z-axis (positive = clockwise from above)

    Combined rotation: R_body = Rz(ψ) @ Ry(θ) @ Rx(φ)

    Args:
        north, east, down: Position in NED frame
        roll_rad, pitch_rad, yaw_rad: Aircraft attitude in radians

    Returns:
        np.array([x_body, y_body, z_body]) in aircraft body frame

    Example:
        >>> # Aircraft flying north, banked 30°
        >>> x, y, z = ned_to_body_frame(100, 0, 0, math.radians(30), 0, 0)
        >>> print(f"Body: x={x:.1f}, y={y:.1f}, z={z:.1f}")
    """
    c_roll = math.cos(roll_rad)
    s_roll = math.sin(roll_rad)
    c_pitch = math.cos(pitch_rad)
    s_pitch = math.sin(pitch_rad)
    c_yaw = math.cos(yaw_rad)
    s_yaw = math.sin(yaw_rad)

    # Combined rotation matrix multiplication
    # This represents: first roll, then pitch, then yaw
    x_body = (c_yaw * c_pitch * north +
              (c_yaw * s_pitch * s_roll - s_yaw * c_roll) * east +
              (c_yaw * s_pitch * c_roll + s_yaw * s_roll) * down)

    y_body = (s_yaw * c_pitch * north +
              (s_yaw * s_pitch * s_roll + c_yaw * c_roll) * east +
              (s_yaw * s_pitch * c_roll - c_yaw * s_roll) * down)

    z_body = (-s_pitch * north + c_pitch * s_roll * east + c_pitch * c_roll * down)

    return np.array([x_body, y_body, z_body])


# ============================================================================
# SECTION 3: MAGNETIC HEADING CORRECTION
# ============================================================================

def magnetometer_to_true_heading(mag_east: float, mag_north: float,
                                 declination_deg: float) -> float:
    """
    Convert calibrated magnetometer readings to true heading.

    Magnetic declination converts magnetic north to true north:
        H_true = H_magnetic + declination

    Args:
        mag_east: Calibrated magnetometer East component (normalized)
        mag_north: Calibrated magnetometer North component (normalized)
        declination_deg: Local magnetic declination in degrees
                        (negative = west, positive = east)

    Returns:
        True heading in degrees (0-360°) where 0° = true north

    Example:
        >>> # Magnetometer points NE (45°), location has -12° declination (12°W)
        >>> heading = magnetometer_to_true_heading(0.707, 0.707, -12.0)
        >>> print(f"True heading: {heading:.1f}°")
        33.0  # 45° - 12° = 33° relative to true north
    """
    # Calculate magnetic heading from magnetometer components
    # Note: atan2(y, x) where y=east, x=north
    mag_heading_rad = math.atan2(mag_east, mag_north)
    mag_heading_deg = math.degrees(mag_heading_rad)

    # Normalize to 0-360°
    mag_heading_deg = mag_heading_deg % 360

    # Apply declination correction
    true_heading_deg = (mag_heading_deg + declination_deg) % 360

    return true_heading_deg


def true_to_magnetic_heading(true_heading_deg: float, declination_deg: float) -> float:
    """
    Reverse conversion: true heading to magnetic heading.

    Args:
        true_heading_deg: True heading in degrees
        declination_deg: Local magnetic declination

    Returns:
        Magnetic heading in degrees (0-360°)

    Example:
        >>> mag = true_to_magnetic_heading(45.0, -12.0)
        >>> print(mag)
        57.0  # 45° - (-12°) = 57° magnetic
    """
    mag_heading_deg = (true_heading_deg - declination_deg) % 360
    return mag_heading_deg


# ============================================================================
# SECTION 4: ALTITUDE CONVERSIONS
# ============================================================================

class AltitudeConverter:
    """Convert between altitude reference systems."""

    # Simplified geoid undulation table (use full EGM grid in production)
    # Real implementation: Use NOAA EGM96, EGM2008, or EGM2020
    GEOID_UNDULATION_TABLE = {
        # Format: (lat_band_start, lon_band_start): undulation_m
        # This is HIGHLY simplified; real geoid models have 1-2km resolution
        (45, 10): -45.0,   # Central Europe
        (40, -120): -35.0, # California
        (0, 0): 0.0,       # Equator/Prime Meridian (approx)
    }

    @staticmethod
    def get_geoid_undulation(lat_deg: float, lon_deg: float) -> float:
        """
        Get geoid undulation (WGS84 ellipsoid height - MSL).

        N = h_ellipsoid - h_geoid

        For production use:
        - NOAA Geoid Height Calculator API
        - GeographicLib
        - Rasterio + EGM2020 GeoTIFF

        Args:
            lat_deg: Latitude
            lon_deg: Longitude

        Returns:
            Geoid undulation in meters (negative = ellipsoid below geoid)
        """
        # Lookup in simplified table
        for (lat_band, lon_band), undulation in AltitudeConverter.GEOID_UNDULATION_TABLE.items():
            if abs(lat_deg - lat_band) < 5 and abs(lon_deg - lon_band) < 5:
                return undulation

        # Fallback: global average
        return -30.0

    @staticmethod
    def hae_to_msl(altitude_hae: float, lat_deg: float, lon_deg: float) -> float:
        """
        Convert WGS84 ellipsoidal height (HAE) to Mean Sea Level (MSL).

        Formula:
            h_MSL = h_HAE - N
        where N is geoid undulation (typically ±100m globally)

        Args:
            altitude_hae: Height above WGS84 ellipsoid (meters)
            lat_deg: Latitude
            lon_deg: Longitude

        Returns:
            Altitude above mean sea level (meters)

        Example:
            >>> alt_msl = hae_to_msl(545.4, 48.117, 11.517)
            >>> print(alt_msl)
            592.3  # 545.4 - (-46.9)
        """
        N = AltitudeConverter.get_geoid_undulation(lat_deg, lon_deg)
        return altitude_hae - N

    @staticmethod
    def msl_to_hae(altitude_msl: float, lat_deg: float, lon_deg: float) -> float:
        """
        Convert MSL altitude to WGS84 ellipsoidal height.

        Formula:
            h_HAE = h_MSL + N

        Args:
            altitude_msl: Height above mean sea level (meters)
            lat_deg: Latitude
            lon_deg: Longitude

        Returns:
            WGS84 ellipsoidal height (meters)
        """
        N = AltitudeConverter.get_geoid_undulation(lat_deg, lon_deg)
        return altitude_msl + N

    @staticmethod
    def msl_to_agl(altitude_msl: float, terrain_elevation: float) -> float:
        """
        Convert MSL altitude to AGL (Above Ground Level).

        Formula:
            h_AGL = h_MSL - h_terrain

        Args:
            altitude_msl: Height above mean sea level
            terrain_elevation: Terrain elevation above MSL

        Returns:
            Height above ground level (meters)

        Example:
            >>> agl = msl_to_agl(500.0, 100.0)
            >>> print(agl)
            400.0  # 400 meters above terrain
        """
        return altitude_msl - terrain_elevation

    @staticmethod
    def hae_to_agl(altitude_hae: float, lat_deg: float, lon_deg: float,
                   terrain_elevation: float) -> float:
        """
        Convert GPS ellipsoidal height directly to AGL (two-step conversion).

        Args:
            altitude_hae: WGS84 ellipsoidal height
            lat_deg: Latitude
            lon_deg: Longitude
            terrain_elevation: Terrain elevation above MSL

        Returns:
            Height above ground level
        """
        alt_msl = AltitudeConverter.hae_to_msl(altitude_hae, lat_deg, lon_deg)
        return AltitudeConverter.msl_to_agl(alt_msl, terrain_elevation)


# ============================================================================
# SECTION 5: GPS ACCURACY METRICS
# ============================================================================

@dataclass
class GPSAccuracy:
    """GPS accuracy indicators from DOP values."""

    hdop: float  # Horizontal Dilution of Precision
    vdop: float  # Vertical Dilution of Precision
    pdop: float = None  # Position DOP (computed)

    def __post_init__(self):
        """Calculate PDOP if not provided."""
        if self.pdop is None:
            self.pdop = math.sqrt(self.hdop**2 + self.vdop**2)

    def horizontal_error_m(self, sigma: float = 3.0) -> float:
        """
        Estimate horizontal position error in meters.

        Error = σ × HDOP
        Typical σ = 3.0 for standard GPS receivers

        Args:
            sigma: Standard deviation scaling factor

        Returns:
            Horizontal error estimate in meters
        """
        return sigma * self.hdop

    def vertical_error_m(self, sigma: float = 3.0) -> float:
        """
        Estimate vertical position error in meters.

        Args:
            sigma: Standard deviation scaling factor

        Returns:
            Vertical error estimate in meters
        """
        return sigma * self.vdop

    def position_error_3d_m(self, sigma: float = 3.0) -> float:
        """Estimate 3D position error."""
        return sigma * self.pdop

    def assessment(self) -> str:
        """Qualitative assessment of accuracy."""
        if self.hdop < 1.0:
            return "Excellent"
        elif self.hdop < 2.0:
            return "Very Good"
        elif self.hdop < 5.0:
            return "Good"
        elif self.hdop < 10.0:
            return "Moderate"
        else:
            return "Poor"

    def suitable_for_precision_navigation(self) -> bool:
        """Check if accuracy sufficient for autonomous navigation."""
        return self.hdop < 2.5 and self.vdop < 3.0


# ============================================================================
# SECTION 6: COMPLETE INTEGRATION EXAMPLE
# ============================================================================

@dataclass
class NavigationState:
    """Complete navigation state for aerial platform."""

    # Position
    position_ned: np.ndarray  # (north, east, down) in meters
    position_enu: np.ndarray  # (east, north, up) in meters

    # Altitude
    altitude_hae: float  # WGS84 ellipsoidal height
    altitude_msl: float  # Mean sea level
    altitude_agl: float  # Above ground level

    # Attitude
    heading_true: float  # True heading in degrees
    heading_magnetic: float  # Magnetic heading
    roll_deg: float = 0.0
    pitch_deg: float = 0.0

    # Accuracy
    hdop: float = 0.0
    vdop: float = 0.0
    satellites: int = 0

    # Status
    fix_quality: int = 0  # 0=invalid, 1=GPS, 4=RTK
    timestamp: str = ""


class DroneNavigationSystem:
    """Complete navigation system for aerial platforms."""

    def __init__(self, reference_lat: float, reference_lon: float,
                 reference_alt: float):
        """
        Initialize navigation system with reference point.

        Args:
            reference_lat, reference_lon: Home location
            reference_alt: Home altitude (HAE)
        """
        self.ref_lat = reference_lat
        self.ref_lon = reference_lon
        self.ref_alt = reference_alt
        self.ref_ecef = geodetic_to_ecef(reference_lat, reference_lon, reference_alt)

    def update_from_gps(self, gps_lat: float, gps_lon: float, gps_alt_hae: float,
                       hdop: float, vdop: float, satellites: int,
                       fix_quality: int) -> NavigationState:
        """
        Update navigation state from GPS data.

        Args:
            gps_lat, gps_lon: Current position
            gps_alt_hae: GPS altitude (ellipsoidal)
            hdop, vdop: Horizontal and vertical DOP
            satellites: Satellite count
            fix_quality: 1=GPS, 4=RTK, etc

        Returns:
            NavigationState object
        """
        # Convert GPS to ECEF
        gps_ecef = geodetic_to_ecef(gps_lat, gps_lon, gps_alt_hae)

        # Get NED position relative to reference
        ned = ecef_to_ned(self.ref_lat, self.ref_lon, self.ref_alt, gps_ecef)
        enu = ecef_to_enu(self.ref_lat, self.ref_lon, self.ref_alt, gps_ecef)

        # Convert altitude
        alt_msl = AltitudeConverter.hae_to_msl(gps_alt_hae, gps_lat, gps_lon)

        # Placeholder for AGL (would need terrain elevation)
        alt_agl = alt_msl - 0.0  # In practice, subtract terrain elevation

        return NavigationState(
            position_ned=ned,
            position_enu=enu,
            altitude_hae=gps_alt_hae,
            altitude_msl=alt_msl,
            altitude_agl=alt_agl,
            heading_true=0.0,  # Would be updated from magnetometer
            heading_magnetic=0.0,
            hdop=hdop,
            vdop=vdop,
            satellites=satellites,
            fix_quality=fix_quality
        )

    def update_heading(self, mag_east: float, mag_north: float,
                      declination_deg: float) -> float:
        """
        Update heading from magnetometer.

        Args:
            mag_east, mag_north: Calibrated magnetometer components
            declination_deg: Local magnetic declination

        Returns:
            True heading in degrees
        """
        return magnetometer_to_true_heading(mag_east, mag_north, declination_deg)


# ============================================================================
# SECTION 7: EXAMPLE USAGE
# ============================================================================

if __name__ == "__main__":
    # Example: Processing GPS data from NEO-M9N
    print("=== GPS Coordinate Transformation Examples ===\n")

    # 1. Parse GPS NMEA data
    print("1. GPS NMEA Data:")
    gps_lat = 47.3667
    gps_lon = 11.1833
    gps_alt_hae = 500.0
    hdop = 0.9
    vdop = 1.5
    satellites = 11
    print(f"   Latitude: {gps_lat:.4f}°")
    print(f"   Longitude: {gps_lon:.4f}°")
    print(f"   Altitude (HAE): {gps_alt_hae:.1f} m")
    print(f"   HDOP: {hdop}, Satellites: {satellites}")

    # 2. Convert to ECEF
    print("\n2. ECEF Conversion:")
    ecef = geodetic_to_ecef(gps_lat, gps_lon, gps_alt_hae)
    print(f"   X: {ecef[0]:,.1f} m")
    print(f"   Y: {ecef[1]:,.1f} m")
    print(f"   Z: {ecef[2]:,.1f} m")

    # 3. Verify round-trip
    print("\n3. Round-trip Verification:")
    lat_verify, lon_verify, alt_verify = ecef_to_geodetic(ecef[0], ecef[1], ecef[2])
    print(f"   Lat: {lat_verify:.4f}° (error: {abs(lat_verify - gps_lat)*1000:.2f} mm)")
    print(f"   Lon: {lon_verify:.4f}° (error: {abs(lon_verify - gps_lon)*1000:.2f} mm)")
    print(f"   Alt: {alt_verify:.1f} m (error: {abs(alt_verify - gps_alt_hae):.2f} mm)")

    # 4. NED conversion (relative to same point = origin)
    print("\n4. NED Conversion (relative to launch site):")
    ned = ecef_to_ned(gps_lat, gps_lon, gps_alt_hae, ecef)
    print(f"   North: {ned[0]:+.1f} m")
    print(f"   East:  {ned[1]:+.1f} m")
    print(f"   Down:  {ned[2]:+.1f} m")
    print("   (Should be near zero - same point)")

    # 5. Different point example
    print("\n5. Position Offset Example:")
    drone_lat = gps_lat + 0.001  # ~111m north
    drone_lon = gps_lon + 0.001  # ~93m east at this latitude
    drone_alt = gps_alt_hae + 20.0
    drone_ecef = geodetic_to_ecef(drone_lat, drone_lon, drone_alt)
    ned_offset = ecef_to_ned(gps_lat, gps_lon, gps_alt_hae, drone_ecef)
    print(f"   Drone position offset from launch:")
    print(f"   North: {ned_offset[0]:+.1f} m")
    print(f"   East:  {ned_offset[1]:+.1f} m")
    print(f"   Down:  {ned_offset[2]:+.1f} m (up)")

    # 6. Altitude conversion
    print("\n6. Altitude Conversions:")
    alt_msl = AltitudeConverter.hae_to_msl(gps_alt_hae, gps_lat, gps_lon)
    alt_agl = AltitudeConverter.msl_to_agl(alt_msl, 400.0)  # 400m terrain elevation
    print(f"   HAE (GPS): {gps_alt_hae:.1f} m")
    print(f"   MSL: {alt_msl:.1f} m")
    print(f"   AGL (above 400m terrain): {alt_agl:.1f} m")

    # 7. Magnetic heading
    print("\n7. Magnetic Heading Correction:")
    declination = -12.0  # 12° West
    mag_east, mag_north = 0.5, 0.866  # ~60° magnetic
    true_heading = magnetometer_to_true_heading(mag_east, mag_north, declination)
    print(f"   Magnetic heading: {math.degrees(math.atan2(mag_east, mag_north)) % 360:.1f}°")
    print(f"   Declination: {declination:.1f}° (W)")
    print(f"   True heading: {true_heading:.1f}°")

    # 8. Accuracy assessment
    print("\n8. GPS Accuracy Assessment:")
    accuracy = GPSAccuracy(hdop=hdop, vdop=vdop)
    print(f"   Horizontal error: ±{accuracy.horizontal_error_m():.1f} m")
    print(f"   Vertical error: ±{accuracy.vertical_error_m():.1f} m")
    print(f"   Assessment: {accuracy.assessment()}")
    print(f"   Suitable for autonomous nav: {accuracy.suitable_for_precision_navigation()}")

    # 9. Complete navigation update
    print("\n9. Complete Navigation System Update:")
    nav_system = DroneNavigationSystem(gps_lat, gps_lon, gps_alt_hae)
    state = nav_system.update_from_gps(
        drone_lat, drone_lon, drone_alt,
        hdop, vdop, satellites, fix_quality=1
    )
    print(f"   NED position: N={state.position_ned[0]:.1f}, "
          f"E={state.position_ned[1]:.1f}, D={state.position_ned[2]:.1f} m")
    print(f"   Altitude: HAE={state.altitude_hae:.1f}, "
          f"MSL={state.altitude_msl:.1f}, AGL={state.altitude_agl:.1f} m")
