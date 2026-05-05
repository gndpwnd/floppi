#!/usr/bin/env python3
"""
NMEA Sentence Parser and Test Suite
Parses GPGGA and GPRMC sentences from NEO-M9N GPS receiver.

NMEA 0183 Format:
  $<talker><sentence>,<data fields>,<checksum>

Talker IDs:
  GP = GPS (Garmin, standard)
  GL = GLONASS
  GB = BeiDou

Common Sentences:
  GPGGA - Global Positioning System Fix Data (position, time, fix quality)
  GPRMC - Recommended Minimum Navigation Information (position, speed, course)

GPGGA Fields:
  0: Sentence ID (GPGGA)
  1: UTC time (hhmmss.ss)
  2: Latitude (ddmm.mmmm)
  3: Latitude direction (N/S)
  4: Longitude (dddmm.mmmm)
  5: Longitude direction (E/W)
  6: Fix quality (0=invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=RTK Float, etc.)
  7: Number of satellites in use
  8: Horizontal dilution of precision (HDOP)
  9: Altitude above mean sea level (meters)
  10: Units of altitude (M=meters)
  11: Geoid separation (meters)
  12: Units of geoid separation (M=meters)
  13: Time since last DGPS fix (blank if no DGPS)
  14: DGPS station ID (blank if no DGPS)

GPRMC Fields:
  0: Sentence ID (GPRMC)
  1: UTC time (hhmmss.ss)
  2: Status (A=active/valid, V=void/invalid)
  3: Latitude (ddmm.mmmm)
  4: Latitude direction (N/S)
  5: Longitude (dddmm.mmmm)
  6: Longitude direction (E/W)
  7: Speed over ground (knots)
  8: Course over ground (degrees true)
  9: Date (ddmmyy)
  10: Magnetic variation (degrees)
  11: Variation direction (E/W)
  12: Mode indicator (A=autonomous, D=DGPS, E=DR, N=no fix)
"""

import argparse
import csv
import re
import sys
from dataclasses import dataclass, field
from typing import Optional, Dict, List


@dataclass
class GPSFix:
    """Represents a single GPS position fix from GPGGA."""
    timestamp: str = ""
    latitude: float = 0.0
    longitude: float = 0.0
    altitude: float = 0.0
    fix_quality: int = 0
    fix_quality_name: str = ""
    satellites: int = 0
    hdop: float = 0.0
    geoid_separation: float = 0.0

    # Metadata
    sentence_type: str = "GPGGA"
    raw_sentence: str = ""
    checksum_valid: bool = False
    parse_error: str = ""


@dataclass
class NavigationData:
    """Represents navigation data from GPRMC."""
    timestamp: str = ""
    latitude: float = 0.0
    longitude: float = 0.0
    speed_knots: float = 0.0
    speed_kmh: float = 0.0
    course_degrees: float = 0.0
    status: str = ""  # A=valid, V=invalid

    # Metadata
    sentence_type: str = "GPRMC"
    raw_sentence: str = ""
    checksum_valid: bool = False
    parse_error: str = ""


class NMEAParser:
    """Parse NMEA sentences from NEO-M9N GPS."""

    FIX_QUALITY_NAMES = {
        0: "Invalid",
        1: "GPS Fix",
        2: "DGPS Fix",
        3: "PPS Fix",
        4: "Real Time Kinematic (RTK)",
        5: "RTK Float",
        6: "Estimated/Dead Reckoning",
        7: "Manual Input Mode",
        8: "Simulation Mode"
    }

    def __init__(self):
        self.fixes: List[GPSFix] = []
        self.nav_data: List[NavigationData] = []
        self.sentence_stats: Dict[str, int] = {}
        self.checksum_failures = 0
        self.parse_errors = 0

    @staticmethod
    def validate_checksum(sentence: str) -> bool:
        """Validate NMEA checksum format: $<data>*<checksum>"""
        if '$' not in sentence or '*' not in sentence:
            return False

        try:
            data_part = sentence[sentence.index('$')+1:sentence.index('*')]
            checksum_part = sentence[sentence.index('*')+1:].strip()

            # Calculate XOR of all characters
            calculated = 0
            for char in data_part:
                calculated ^= ord(char)

            expected = int(checksum_part, 16)
            return calculated == expected
        except (ValueError, IndexError):
            return False

    @staticmethod
    def dms_to_decimal(coord: str, direction: str) -> float:
        """Convert DMS (degrees, minutes) to decimal degrees.

        Format: ddmm.mmmm (or dddmm.mmmm for longitude)
        Returns negative for S/W directions.
        """
        if not coord or not direction:
            return 0.0

        try:
            # Split integer and decimal parts
            if '.' in coord:
                integer_part, decimal_part = coord.split('.')
            else:
                integer_part, decimal_part = coord, "0"

            # Extract degrees and minutes
            if len(integer_part) % 2 == 1:  # Odd length = latitude (dd)
                degrees = int(integer_part[:-2])
                minutes = int(integer_part[-2:])
            else:  # Even length = longitude (ddd)
                degrees = int(integer_part[:-2])
                minutes = int(integer_part[-2:])

            # Build decimal
            decimal = float(minutes + float("0." + decimal_part)) / 60.0
            result = degrees + decimal

            # Apply direction
            if direction in ('S', 'W'):
                result = -result

            return result
        except (ValueError, IndexError) as e:
            return 0.0

    def parse_gpgga(self, fields: List[str]) -> GPSFix:
        """Parse GPGGA sentence (Global Positioning System Fix Data)."""
        fix = GPSFix(sentence_type="GPGGA")
        fix.raw_sentence = "$" + ",".join(fields)

        try:
            if len(fields) < 13:
                fix.parse_error = f"Insufficient fields: {len(fields)} < 13"
                return fix

            # Parse UTC time
            fix.timestamp = fields[1] if len(fields[1]) >= 6 else ""

            # Parse latitude and longitude
            lat_str = fields[2]
            lat_dir = fields[3]
            lon_str = fields[4]
            lon_dir = fields[5]

            fix.latitude = self.dms_to_decimal(lat_str, lat_dir)
            fix.longitude = self.dms_to_decimal(lon_str, lon_dir)

            # Parse fix quality
            if fields[6]:
                fix.fix_quality = int(fields[6])
                fix.fix_quality_name = self.FIX_QUALITY_NAMES.get(
                    fix.fix_quality, "Unknown"
                )

            # Parse satellite count
            if fields[7]:
                fix.satellites = int(fields[7])

            # Parse HDOP
            if fields[8]:
                fix.hdop = float(fields[8])

            # Parse altitude
            if fields[9]:
                fix.altitude = float(fields[9])

            # Parse geoid separation
            if len(fields) > 11 and fields[11]:
                fix.geoid_separation = float(fields[11])

        except (ValueError, IndexError) as e:
            fix.parse_error = str(e)

        return fix

    def parse_gprmc(self, fields: List[str]) -> NavigationData:
        """Parse GPRMC sentence (Recommended Minimum Navigation Information)."""
        nav = NavigationData(sentence_type="GPRMC")
        nav.raw_sentence = "$" + ",".join(fields)

        try:
            if len(fields) < 10:
                nav.parse_error = f"Insufficient fields: {len(fields)} < 10"
                return nav

            # Parse UTC time
            nav.timestamp = fields[1] if len(fields[1]) >= 6 else ""

            # Parse status
            nav.status = fields[2]

            # Parse latitude and longitude
            lat_str = fields[3]
            lat_dir = fields[4]
            lon_str = fields[5]
            lon_dir = fields[6]

            nav.latitude = self.dms_to_decimal(lat_str, lat_dir)
            nav.longitude = self.dms_to_decimal(lon_str, lon_dir)

            # Parse speed
            if fields[7]:
                nav.speed_knots = float(fields[7])
                nav.speed_kmh = nav.speed_knots * 1.852

            # Parse course
            if fields[8]:
                nav.course_degrees = float(fields[8])

        except (ValueError, IndexError) as e:
            nav.parse_error = str(e)

        return nav

    def parse_sentence(self, sentence: str) -> Optional[dict]:
        """Parse a single NMEA sentence."""
        sentence = sentence.strip()

        if not sentence.startswith('$'):
            return None

        # Validate and remove checksum
        checksum_valid = self.validate_checksum(sentence)
        if '*' in sentence:
            sentence = sentence[:sentence.index('*')]

        # Extract fields
        fields = sentence[1:].split(',')
        sentence_type = fields[0]

        self.sentence_stats[sentence_type] = self.sentence_stats.get(sentence_type, 0) + 1

        result = {
            'type': sentence_type,
            'checksum_valid': checksum_valid,
            'data': None
        }

        if sentence_type == 'GPGGA':
            data = self.parse_gpgga(fields)
            data.checksum_valid = checksum_valid
            result['data'] = data
            self.fixes.append(data)
        elif sentence_type == 'GPRMC':
            data = self.parse_gprmc(fields)
            data.checksum_valid = checksum_valid
            result['data'] = data
            self.nav_data.append(data)

        return result

    def parse_file(self, filename: str) -> None:
        """Parse all sentences in a file."""
        try:
            with open(filename, 'r') as f:
                for line_num, line in enumerate(f, 1):
                    self.parse_sentence(line)
        except IOError as e:
            print(f"Error reading file: {e}", file=sys.stderr)

    def generate_csv_report(self, output_file: str) -> None:
        """Generate CSV report of parsed fixes and navigation data."""
        try:
            with open(output_file, 'w', newline='') as f:
                writer = csv.writer(f)

                # Write headers
                writer.writerow([
                    'Type', 'Timestamp', 'Latitude', 'Longitude', 'Altitude',
                    'Satellites', 'HDOP', 'Fix Quality', 'Speed (knots)',
                    'Speed (km/h)', 'Course (deg)', 'Status', 'Checksum Valid',
                    'Parse Error'
                ])

                # Write GPS fix data
                for fix in self.fixes:
                    writer.writerow([
                        fix.sentence_type,
                        fix.timestamp,
                        f"{fix.latitude:.6f}",
                        f"{fix.longitude:.6f}",
                        f"{fix.altitude:.1f}" if fix.altitude else "",
                        fix.satellites if fix.satellites > 0 else "",
                        f"{fix.hdop:.1f}" if fix.hdop > 0 else "",
                        f"{fix.fix_quality} ({fix.fix_quality_name})",
                        "",
                        "",
                        "",
                        "",
                        "Yes" if fix.checksum_valid else "No",
                        fix.parse_error
                    ])

                # Write navigation data
                for nav in self.nav_data:
                    writer.writerow([
                        nav.sentence_type,
                        nav.timestamp,
                        f"{nav.latitude:.6f}",
                        f"{nav.longitude:.6f}",
                        "",
                        "",
                        "",
                        "",
                        f"{nav.speed_knots:.1f}" if nav.speed_knots > 0 else "",
                        f"{nav.speed_kmh:.1f}" if nav.speed_kmh > 0 else "",
                        f"{nav.course_degrees:.1f}" if nav.course_degrees > 0 else "",
                        nav.status,
                        "Yes" if nav.checksum_valid else "No",
                        nav.parse_error
                    ])

            print(f"CSV report saved to {output_file}", file=sys.stderr)
        except IOError as e:
            print(f"Error writing CSV: {e}", file=sys.stderr)

    def print_summary(self) -> None:
        """Print summary statistics."""
        total_sentences = sum(self.sentence_stats.values())

        print("\n=== NMEA Parsing Summary ===")
        print(f"Total sentences: {total_sentences}")
        print(f"Sentence types: {dict(self.sentence_stats)}")
        print(f"GPGGA fixes: {len(self.fixes)}")
        print(f"GPRMC nav data: {len(self.nav_data)}")

        # Summary of fixes
        if self.fixes:
            print(f"\nGPGGA Statistics:")
            valid_fixes = [f for f in self.fixes if not f.parse_error]
            print(f"  Valid fixes: {len(valid_fixes)}/{len(self.fixes)}")

            if valid_fixes:
                lats = [f.latitude for f in valid_fixes if f.latitude != 0]
                lons = [f.longitude for f in valid_fixes if f.longitude != 0]
                alts = [f.altitude for f in valid_fixes if f.altitude != 0]
                sats = [f.satellites for f in valid_fixes if f.satellites > 0]

                if lats:
                    print(f"  Latitude range: {min(lats):.6f} to {max(lats):.6f}")
                if lons:
                    print(f"  Longitude range: {min(lons):.6f} to {max(lons):.6f}")
                if alts:
                    print(f"  Altitude range: {min(alts):.1f}m to {max(alts):.1f}m")
                if sats:
                    print(f"  Satellite count: {min(sats)} to {max(sats)}")

        if self.nav_data:
            print(f"\nGPRMC Statistics:")
            valid_nav = [n for n in self.nav_data if not n.parse_error]
            print(f"  Valid records: {len(valid_nav)}/{len(self.nav_data)}")

            if valid_nav:
                speeds = [n.speed_knots for n in valid_nav if n.speed_knots > 0]
                courses = [n.course_degrees for n in valid_nav if n.course_degrees > 0]

                if speeds:
                    print(f"  Speed range: {min(speeds):.1f} to {max(speeds):.1f} knots")
                if courses:
                    print(f"  Course range: {min(courses):.1f} to {max(courses):.1f} degrees")


def main():
    parser = argparse.ArgumentParser(
        description="Parse NMEA sentences from NEO-M9N GPS"
    )
    parser.add_argument("input_file", help="Input file with NMEA sentences")
    parser.add_argument(
        "--output", "-o",
        help="Output CSV file for parsed data"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Print detailed output"
    )

    args = parser.parse_args()

    nmea = NMEAParser()
    nmea.parse_file(args.input_file)

    nmea.print_summary()

    if args.verbose:
        print("\n=== Parsed GPGGA Fixes ===")
        for i, fix in enumerate(nmea.fixes, 1):
            if fix.parse_error:
                print(f"{i}. ERROR: {fix.parse_error}")
            else:
                print(f"{i}. Time={fix.timestamp} Lat={fix.latitude:.6f} "
                      f"Lon={fix.longitude:.6f} Alt={fix.altitude:.1f}m "
                      f"Sats={fix.satellites} HDOP={fix.hdop:.1f} "
                      f"Fix={fix.fix_quality_name}")

        print("\n=== Parsed GPRMC Data ===")
        for i, nav in enumerate(nmea.nav_data, 1):
            if nav.parse_error:
                print(f"{i}. ERROR: {nav.parse_error}")
            else:
                print(f"{i}. Time={nav.timestamp} Lat={nav.latitude:.6f} "
                      f"Lon={nav.longitude:.6f} Speed={nav.speed_knots:.1f}kt "
                      f"({nav.speed_kmh:.1f}km/h) Course={nav.course_degrees:.1f}° "
                      f"Status={nav.status}")

    if args.output:
        nmea.generate_csv_report(args.output)


if __name__ == "__main__":
    main()
