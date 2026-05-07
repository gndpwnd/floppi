#!/usr/bin/env python3
"""
Simple serial monitor - reads and displays serial output
No complexity, just basic line-by-line streaming with optional filtering
"""

import serial
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description='Serial monitor for Arduino')
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyACM1)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--filter', help='Only show lines containing this text (optional)')

    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
        print(f"\n✓ Connected to {args.port} at {args.baud} baud")
        print("Press CTRL+C to exit\n")

        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    # Filter if requested
                    if args.filter:
                        if args.filter.lower() in line.lower():
                            print(line)
                    else:
                        print(line)

    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\nDisconnected")
        sys.exit(0)

if __name__ == '__main__':
    main()
