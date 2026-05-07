#!/usr/bin/env python3
"""
Test Data Validation Script

Validates math results from snapshot files:
- Verify quaternion magnitudes ≈ 1.0
- Verify Euler angles in valid range
- Verify round-trip conversions
- Generate report of what works

Usage:
    python3 validate_snapshot_data.py [snapshot_file_or_directory]

Example:
    python3 validate_snapshot_data.py /path/to/snapshots/
    python3 validate_snapshot_data.py snapshot_001.json
"""

import json
import math
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional, Dict, Tuple

# ============================================================================
# Data Structures
# ============================================================================


@dataclass
class Quaternion:
    """Quaternion representation: w, x, y, z"""
    w: float
    x: float
    y: float
    z: float

    def magnitude(self) -> float:
        """Get quaternion magnitude (should be ≈ 1.0 for valid rotations)"""
        return math.sqrt(self.w**2 + self.x**2 + self.y**2 + self.z**2)

    def is_normalized(self, tolerance: float = 0.01) -> bool:
        """Check if quaternion is normalized (magnitude ≈ 1.0)"""
        mag = self.magnitude()
        return abs(mag - 1.0) < tolerance

    def normalize(self) -> "Quaternion":
        """Return normalized copy"""
        mag = self.magnitude()
        if mag < 1e-6:
            return Quaternion(1.0, 0.0, 0.0, 0.0)
        inv_mag = 1.0 / mag
        return Quaternion(
            self.w * inv_mag, self.x * inv_mag, self.y * inv_mag, self.z * inv_mag
        )

    def to_euler_angles(self) -> Tuple[float, float, float]:
        """Convert quaternion to Euler angles (radians)

        Returns:
            (roll, pitch, yaw) in radians
        """
        # Roll (x-axis rotation)
        sinr_cosp = 2.0 * (self.w * self.x + self.y * self.z)
        cosr_cosp = 1.0 - 2.0 * (self.x**2 + self.y**2)
        roll = math.atan2(sinr_cosp, cosr_cosp)

        # Pitch (y-axis rotation)
        sinp = 2.0 * (self.w * self.y - self.z * self.x)
        if abs(sinp) >= 1.0:
            pitch = math.copysign(math.pi / 2.0, sinp)
        else:
            pitch = math.asin(sinp)

        # Yaw (z-axis rotation)
        siny_cosp = 2.0 * (self.w * self.z + self.x * self.y)
        cosy_cosp = 1.0 - 2.0 * (self.y**2 + self.z**2)
        yaw = math.atan2(siny_cosp, cosy_cosp)

        return (roll, pitch, yaw)

    @staticmethod
    def from_euler_angles(roll: float, pitch: float, yaw: float) -> "Quaternion":
        """Convert Euler angles (radians) to quaternion"""
        roll_h = roll / 2.0
        pitch_h = pitch / 2.0
        yaw_h = yaw / 2.0

        cr = math.cos(roll_h)
        sr = math.sin(roll_h)
        cp = math.cos(pitch_h)
        sp = math.sin(pitch_h)
        cy = math.cos(yaw_h)
        sy = math.sin(yaw_h)

        w = cr * cp * cy + sr * sp * sy
        x = sr * cp * cy - cr * sp * sy
        y = cr * sp * cy + sr * cp * sy
        z = cr * cp * sy - sr * sp * cy

        return Quaternion(w, x, y, z)


@dataclass
class EulerAngles:
    """Euler angles in degrees"""
    roll: float
    pitch: float
    yaw: float

    def to_radians(self) -> Tuple[float, float, float]:
        """Convert to radians"""
        deg_to_rad = math.pi / 180.0
        return (
            self.roll * deg_to_rad,
            self.pitch * deg_to_rad,
            self.yaw * deg_to_rad,
        )

    def is_valid(self) -> bool:
        """Check if angles are in valid ranges"""
        return (
            -180.0 <= self.roll <= 180.0
            and -90.0 <= self.pitch <= 90.0
            and -180.0 <= self.yaw <= 180.0
        )

    def is_near_gimbal_lock(self, tolerance: float = 5.0) -> bool:
        """Check if pitch is near gimbal lock (±90°)"""
        return abs(self.pitch) > (90.0 - tolerance)


@dataclass
class SnapshotRecord:
    """A single snapshot from the data file"""
    timestamp_ms: int
    quaternion: Quaternion
    euler: Optional[EulerAngles] = None
    calibration: Optional[Dict] = None

    def validate(self) -> Tuple[bool, List[str]]:
        """Validate the record, return (is_valid, list_of_errors)"""
        errors = []

        # Check quaternion magnitude
        mag = self.quaternion.magnitude()
        if mag < 0.95 or mag > 1.05:
            errors.append(f"Quaternion magnitude out of range: {mag} (expected ≈1.0)")

        # Check Euler angles if present
        if self.euler:
            if not self.euler.is_valid():
                errors.append(
                    f"Euler angles out of range: "
                    f"roll={self.euler.roll}°, pitch={self.euler.pitch}°, yaw={self.euler.yaw}°"
                )

            # Check round-trip conversion (if not near gimbal lock)
            if not self.euler.is_near_gimbal_lock():
                roll_rad, pitch_rad, yaw_rad = self.euler.to_radians()
                q_from_euler = Quaternion.from_euler_angles(roll_rad, pitch_rad, yaw_rad)
                q_normalized = q_from_euler.normalize()
                q_normalized_original = self.quaternion.normalize()

                # Check if they represent the same rotation (via dot product)
                dot = (
                    q_normalized.w * q_normalized_original.w
                    + q_normalized.x * q_normalized_original.x
                    + q_normalized.y * q_normalized_original.y
                    + q_normalized.z * q_normalized_original.z
                )
                dot = abs(dot)

                if dot < 0.99:
                    errors.append(
                        f"Round-trip conversion error: "
                        f"quaternion → euler → quaternion dot product = {dot} "
                        f"(expected > 0.99)"
                    )

        return (len(errors) == 0, errors)


# ============================================================================
# File Loading and Parsing
# ============================================================================


def load_snapshot_file(filepath: Path) -> List[SnapshotRecord]:
    """Load and parse a snapshot JSON file"""
    records = []

    with open(filepath, "r") as f:
        content = f.read()

    # Try to parse as JSON Lines (one JSON object per line)
    lines = content.strip().split("\n")
    for i, line in enumerate(lines):
        if not line.strip():
            continue

        try:
            data = json.loads(line)

            # Parse quaternion
            q_data = data.get("quaternion", {})
            q = Quaternion(
                w=float(q_data.get("w", 1.0)),
                x=float(q_data.get("x", 0.0)),
                y=float(q_data.get("y", 0.0)),
                z=float(q_data.get("z", 0.0)),
            )

            # Parse Euler angles if present
            euler = None
            e_data = data.get("euler", {})
            if e_data:
                euler = EulerAngles(
                    roll=float(e_data.get("roll_deg", 0.0)),
                    pitch=float(e_data.get("pitch_deg", 0.0)),
                    yaw=float(e_data.get("yaw_deg", 0.0)),
                )

            record = SnapshotRecord(
                timestamp_ms=int(data.get("timestamp_ms", 0)),
                quaternion=q,
                euler=euler,
                calibration=data.get("calibration"),
            )
            records.append(record)

        except (json.JSONDecodeError, ValueError, KeyError) as e:
            print(f"  Warning: Could not parse line {i + 1}: {e}")

    return records


def find_snapshot_files(path: Path) -> List[Path]:
    """Find all snapshot JSON files in path (file or directory)"""
    if path.is_file():
        return [path]
    elif path.is_dir():
        return sorted(path.glob("*.json"))
    else:
        return []


# ============================================================================
# Validation and Reporting
# ============================================================================


class ValidationReport:
    """Collects and reports validation results"""

    def __init__(self):
        self.total_records = 0
        self.valid_records = 0
        self.invalid_records = 0
        self.errors: Dict[str, int] = {}
        self.mag_stats = {"min": 1.0, "max": 1.0, "avg": 1.0}
        self.magnitudes: List[float] = []

    def add_record(self, record: SnapshotRecord, is_valid: bool, errors: List[str]):
        """Add validation result for a record"""
        self.total_records += 1
        if is_valid:
            self.valid_records += 1
        else:
            self.invalid_records += 1
            for error in errors:
                self.errors[error] = self.errors.get(error, 0) + 1

        # Track magnitude statistics
        mag = record.quaternion.magnitude()
        self.magnitudes.append(mag)

    def finalize(self):
        """Compute final statistics"""
        if self.magnitudes:
            self.mag_stats["min"] = min(self.magnitudes)
            self.mag_stats["max"] = max(self.magnitudes)
            self.mag_stats["avg"] = sum(self.magnitudes) / len(self.magnitudes)

    def print_report(self):
        """Print validation report to stdout"""
        print("\n" + "=" * 70)
        print("SNAPSHOT DATA VALIDATION REPORT")
        print("=" * 70)

        print(f"\nRecords Analyzed: {self.total_records}")
        print(f"  Valid:   {self.valid_records} ({100*self.valid_records//self.total_records if self.total_records else 0}%)")
        print(f"  Invalid: {self.invalid_records} ({100*self.invalid_records//self.total_records if self.total_records else 0}%)")

        if self.total_records == 0:
            print("\nNo records found.")
            return

        print(f"\nQuaternion Magnitude Statistics:")
        print(f"  Min:     {self.mag_stats['min']:.6f}")
        print(f"  Max:     {self.mag_stats['max']:.6f}")
        print(f"  Average: {self.mag_stats['avg']:.6f}")
        print(f"  Deviation from 1.0: ±{max(abs(self.mag_stats['min']-1.0), abs(self.mag_stats['max']-1.0))*100:.4f}%")

        if self.errors:
            print(f"\nErrors Found ({len(self.errors)} unique types):")
            for error, count in sorted(self.errors.items(), key=lambda x: -x[1]):
                print(f"  [{count:3d}] {error}")
        else:
            print(f"\n✓ No validation errors found!")

        print("\n" + "=" * 70)

        # Recommendations
        print("\nRECOMMENDATIONS:")
        if self.valid_records == self.total_records:
            print("  ✓ All records are valid")
            print("  ✓ Quaternion magnitudes are within tolerance")
            print("  ✓ Euler angles are in valid ranges")
            print("  ✓ Round-trip conversions are accurate")
        else:
            if self.mag_stats["max"] - self.mag_stats["min"] > 0.1:
                print("  ⚠ Large variations in quaternion magnitude suggest:")
                print("    - Sensor calibration issues")
                print("    - Communication errors")
                print("    - Data corruption")

            if self.invalid_records > self.total_records * 0.1:
                print("  ⚠ More than 10% invalid records detected")
                print("    Recommended actions:")
                print("    1. Check sensor initialization")
                print("    2. Verify I2C communication")
                print("    3. Calibrate sensor with figure-8 motion")

        print("=" * 70 + "\n")


# ============================================================================
# Main Entry Point
# ============================================================================


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    input_path = Path(sys.argv[1])

    if not input_path.exists():
        print(f"Error: Path does not exist: {input_path}")
        sys.exit(1)

    # Find all snapshot files
    snapshot_files = find_snapshot_files(input_path)
    if not snapshot_files:
        print(f"Error: No snapshot files found in {input_path}")
        sys.exit(1)

    report = ValidationReport()

    # Process each snapshot file
    for filepath in snapshot_files:
        print(f"Processing: {filepath}")

        try:
            records = load_snapshot_file(filepath)
            print(f"  Loaded {len(records)} records")

            for record in records:
                is_valid, errors = record.validate()
                report.add_record(record, is_valid, errors)

                if not is_valid and len(errors) <= 2:
                    # Print brief error info for debugging
                    for error in errors:
                        print(f"    ⚠ {error}")

        except Exception as e:
            print(f"  Error processing file: {e}")

    report.finalize()
    report.print_report()

    # Exit with appropriate code
    sys.exit(0 if report.invalid_records == 0 else 1)


if __name__ == "__main__":
    main()
