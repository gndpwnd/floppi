#!/usr/bin/env python3
"""
auto_calibrate.py

Magnetometer ellipsoid-fit calibration tool. Reads a CSV of raw
magnetometer samples captured from the firmware while the operator
rotates the device through all orientations, computes hard-iron offset
and soft-iron correction matrix, and packs the result into one of the
firmware-readable EEPROM/NVS blob formats.

================================================================
STATUS: SKELETON --- Renaudin 2010 ellipsoid fit not implemented yet.
        Phase 5.5 follow-up.

The current implementation computes a NAIVE hard-iron estimate
(mean of all samples) and emits an identity soft-iron matrix. This
is INCORRECT for any non-trivial magnetic environment but lets the
end-to-end pipeline (firmware-stream -> CSV -> blob -> firmware-set)
run before the real fit lands. Do NOT trust the produced blob for
flight or for any heading-critical use.

The real implementation should use scipy.optimize.least_squares with
the Renaudin et al. 2010 formulation; see references in
docs/findings/mpu6050_external_mag_pipeline.md.
================================================================

Output formats and byte layouts (per docs/findings/):

    bno055        22 bytes  --- Adafruit_BNO055 `adafruit_bno055_offsets_t`
                              dump. NOTE: BNO055 uses int16 sensor-frame
                              offsets, not a hard/soft-iron decomposition;
                              the bytes written here are APPROXIMATE for the
                              skeleton (see stderr warning).
    bno085       256 bytes  --- SH-2 FRS dynamic-calibration record buffer.
                              Real layout is sensor-internal and ~36-72 B
                              of meaningful payload, zero-padded to 256.
    mpu_external 48 bytes   --- float[12] = mag_hard_iron[3] (offsets, uT)
                              + mag_soft_iron[9] (row-major 3x3).
                              Matches `mag_hard_iron` (offset 28) +
                              `mag_soft_iron` (offset 40) fields of the
                              MPU6050+mag EEPROM blob.

Input CSV columns (header required):

    mx, my, mz

with optional extra columns ignored. Units don't matter for the fit
itself (offsets come out in the input's units).

Dependencies: stdlib + optional numpy. Numpy is preferred for the
real fit; the skeleton falls back to pure Python when numpy is
unavailable.

Usage:
    auto_calibrate.py --samples mag.csv --output mag.bin --format mpu_external
    auto_calibrate.py --samples mag.csv --output bno.bin --format bno055
    auto_calibrate.py --samples mag.csv --output b85.bin --format bno085 \\
                      --min-samples 1500
"""

from __future__ import annotations

import argparse
import csv
import struct
import sys
from pathlib import Path

# ----------------------------------------------------------------------
# Optional numpy; pure-Python fallback below
# ----------------------------------------------------------------------
try:
    import numpy as np  # type: ignore
    _HAS_NUMPY = True
except ImportError:
    _HAS_NUMPY = False


# ----------------------------------------------------------------------
# Constants for blob sizes
# ----------------------------------------------------------------------
BLOB_SIZES = {
    "bno055": 22,
    "bno085": 256,
    "mpu_external": 48,
}


# ----------------------------------------------------------------------
# Sample loading
# ----------------------------------------------------------------------
def load_samples(csv_path: Path) -> list[tuple[float, float, float]]:
    samples: list[tuple[float, float, float]] = []
    with csv_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError(f"{csv_path}: no header row")
        missing = {"mx", "my", "mz"} - set(reader.fieldnames)
        if missing:
            raise ValueError(
                f"{csv_path}: CSV missing required column(s): "
                f"{sorted(missing)}"
            )
        for i, row in enumerate(reader, start=2):
            try:
                mx = float(row["mx"])
                my = float(row["my"])
                mz = float(row["mz"])
            except (KeyError, ValueError) as e:
                raise ValueError(f"{csv_path}:{i}: bad row: {e}") from e
            samples.append((mx, my, mz))
    return samples


# ----------------------------------------------------------------------
# Sample statistics
# ----------------------------------------------------------------------
def sample_stats(samples: list[tuple[float, float, float]]) -> dict:
    n = len(samples)
    if n == 0:
        return {"count": 0}
    xs = [s[0] for s in samples]
    ys = [s[1] for s in samples]
    zs = [s[2] for s in samples]
    return {
        "count": n,
        "min": (min(xs), min(ys), min(zs)),
        "max": (max(xs), max(ys), max(zs)),
        "mean": (sum(xs) / n, sum(ys) / n, sum(zs) / n),
    }


# ----------------------------------------------------------------------
# Ellipsoid fit (SKELETON)
# ----------------------------------------------------------------------
def fit_ellipsoid(
    samples: list[tuple[float, float, float]],
) -> tuple[tuple[float, float, float], list[float]]:
    """
    Return (hard_iron_offset, soft_iron_matrix_row_major_9).

    SKELETON IMPLEMENTATION: returns mean-of-samples as hard-iron
    offset and 3x3 identity as soft-iron correction.

    TODO: replace with the real ellipsoid fit. Use
    scipy.optimize.least_squares with the Renaudin 2010 formulation
    (fit the implicit quadric a*x^2 + b*y^2 + c*z^2 + 2dxy + 2exz +
    2fyz + 2gx + 2hy + 2iz = 1, then decompose into center + axes via
    eigendecomposition of the shape matrix).
    """
    print(
        "TODO: ellipsoid fit not yet implemented; "
        "use scipy.optimize.least_squares with the Renaudin 2010 "
        "formulation. Returning naive mean-as-offset, identity-soft-iron.",
        file=sys.stderr,
    )

    if not samples:
        return (0.0, 0.0, 0.0), [
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0,
        ]

    if _HAS_NUMPY:
        arr = np.asarray(samples, dtype=np.float64)
        offset = tuple(float(v) for v in arr.mean(axis=0))
    else:
        n = len(samples)
        ox = sum(s[0] for s in samples) / n
        oy = sum(s[1] for s in samples) / n
        oz = sum(s[2] for s in samples) / n
        offset = (ox, oy, oz)

    soft_iron_identity = [
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
    ]
    return offset, soft_iron_identity


# ----------------------------------------------------------------------
# Blob packers
# ----------------------------------------------------------------------
def pack_mpu_external(
    offset: tuple[float, float, float],
    soft_iron: list[float],
) -> bytes:
    """
    48 bytes: float[12] little-endian = 3 offset floats + 9 soft-iron
    floats (row-major). Matches the `mag_hard_iron` (offset 28) +
    `mag_soft_iron` (offset 40) sub-fields of the MPU6050+mag EEPROM
    blob documented in docs/findings/mpu6050_external_mag_pipeline.md.
    """
    if len(soft_iron) != 9:
        raise ValueError("soft_iron must be a 9-element row-major list")
    payload = struct.pack("<3f", *offset) + struct.pack("<9f", *soft_iron)
    assert len(payload) == 48, len(payload)
    return payload


def pack_bno055(
    offset: tuple[float, float, float],
    soft_iron: list[float],  # noqa: ARG001 (intentionally unused; BNO055
                              # blob has no soft-iron field — its fusion
                              # firmware learns it internally)
) -> bytes:
    """
    22 bytes: APPROXIMATE Adafruit_BNO055 `adafruit_bno055_offsets_t`
    footprint --- the real struct is 11 int16 values (accel offset xyz,
    mag offset xyz, gyro offset xyz, accel radius, mag radius). We only
    have mag offsets to fill; the rest go to zero. The real driver also
    expects sensor-frame integer LSBs, not microtesla floats; a proper
    implementation needs the BNO055 mag LSB-per-uT scale (typically
    16 LSB/uT). Skeleton scales 1:1 and clamps to int16.
    """
    print(
        "WARNING: bno055 blob format is APPROXIMATE in the skeleton. "
        "Real implementation must scale mag offsets to BNO055 LSBs "
        "(~16 LSB/uT) and fill the accel/gyro offsets + radii fields. "
        "See Adafruit_BNO055 `getSensorOffsets(uint8_t*)`.",
        file=sys.stderr,
    )

    def _clamp_i16(v: float) -> int:
        iv = int(round(v))
        if iv > 32767:
            return 32767
        if iv < -32768:
            return -32768
        return iv

    accel_xyz = (0, 0, 0)
    mag_xyz = tuple(_clamp_i16(v) for v in offset)
    gyro_xyz = (0, 0, 0)
    accel_radius = 0
    mag_radius = 0

    # 11 int16 little-endian = 22 bytes
    payload = struct.pack(
        "<11h",
        accel_xyz[0], accel_xyz[1], accel_xyz[2],
        mag_xyz[0],   mag_xyz[1],   mag_xyz[2],
        gyro_xyz[0],  gyro_xyz[1],  gyro_xyz[2],
        accel_radius,
        mag_radius,
    )
    assert len(payload) == 22, len(payload)
    return payload


def pack_bno085(
    offset: tuple[float, float, float],
    soft_iron: list[float],
) -> bytes:
    """
    256 bytes: the BNO085 stores its dynamic-calibration data in an
    SH-2 FRS record (~36-72 B of opaque payload). We can't fabricate
    a valid FRS record from a host-side fit --- the BNO085's internal
    fusion learns its own corrections at runtime.

    This packer therefore writes the real soft-iron + offset values
    into the first 48 bytes as a host-readable header (same layout as
    `mpu_external`) and zero-pads the rest to 256. The firmware-side
    `bno085_calibration_persist` code that consumes this blob will
    need to know to skip the FRS-record path when these bytes are
    detected; that gating is part of the Phase 5.5 follow-up.
    """
    print(
        "WARNING: bno085 256-byte blob format is APPROXIMATE in the "
        "skeleton. Real implementation must write a valid SH-2 FRS "
        "DYNAMIC_CALIBRATION record; this skeleton writes the same "
        "48-byte payload as `mpu_external` and zero-pads to 256.",
        file=sys.stderr,
    )
    payload = pack_mpu_external(offset, soft_iron)
    return payload + b"\x00" * (256 - len(payload))


def pack_blob(
    fmt: str,
    offset: tuple[float, float, float],
    soft_iron: list[float],
) -> bytes:
    if fmt == "mpu_external":
        return pack_mpu_external(offset, soft_iron)
    if fmt == "bno055":
        return pack_bno055(offset, soft_iron)
    if fmt == "bno085":
        return pack_bno085(offset, soft_iron)
    raise ValueError(f"unknown format: {fmt}")


# ----------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------
def parse_args(argv: list) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=(
            "Magnetometer calibration blob packer. "
            "SKELETON --- Renaudin 2010 ellipsoid fit not implemented. "
            "Outputs naive mean-as-offset, identity soft-iron."
        ),
    )
    p.add_argument(
        "--samples",
        required=True,
        help="Path to CSV of magnetometer samples (columns mx,my,mz).",
    )
    p.add_argument(
        "--output",
        required=True,
        help="Path to write the binary calibration blob.",
    )
    p.add_argument(
        "--format",
        required=True,
        choices=sorted(BLOB_SIZES.keys()),
        help=(
            "Blob format: bno055 (22 B), bno085 (256 B), or "
            "mpu_external (48 B)."
        ),
    )
    p.add_argument(
        "--min-samples",
        type=int,
        default=1000,
        help="Minimum sample count required to proceed (default: 1000).",
    )
    return p.parse_args(argv)


def main(argv: list) -> int:
    args = parse_args(argv)

    samples_path = Path(args.samples)
    if not samples_path.is_file():
        print(
            f"ERROR: --samples file not found: {samples_path}",
            file=sys.stderr,
        )
        return 2

    try:
        samples = load_samples(samples_path)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    stats = sample_stats(samples)
    print(f"Loaded {stats['count']} samples from {samples_path}")
    if stats["count"] < args.min_samples:
        print(
            f"ERROR: {stats['count']} samples < --min-samples "
            f"({args.min_samples}). Rotate the device through more "
            f"orientations and re-capture.",
            file=sys.stderr,
        )
        return 2

    print(f"  min   (x,y,z): {stats['min']}")
    print(f"  max   (x,y,z): {stats['max']}")
    print(f"  mean  (x,y,z): {stats['mean']}")
    if not _HAS_NUMPY:
        print(
            "  (numpy not available; using pure-Python statistics)",
            file=sys.stderr,
        )

    offset, soft_iron = fit_ellipsoid(samples)
    blob = pack_blob(args.format, offset, soft_iron)

    expected = BLOB_SIZES[args.format]
    if len(blob) != expected:
        print(
            f"INTERNAL ERROR: packed blob is {len(blob)} bytes, "
            f"expected {expected} for format {args.format}",
            file=sys.stderr,
        )
        return 1

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as f:
        f.write(blob)

    print(
        f"Wrote {len(blob)} bytes to {out_path}. "
        f"Hard-iron offset: [{offset[0]:.4f}, {offset[1]:.4f}, "
        f"{offset[2]:.4f}]. Soft-iron: identity."
    )
    print(
        "REMINDER: this is a SKELETON output. The hard-iron value is "
        "a naive mean and the soft-iron is identity --- not a real "
        "ellipsoid fit. Do not deploy for heading-critical use.",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
