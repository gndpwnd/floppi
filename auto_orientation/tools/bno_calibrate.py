#!/usr/bin/env python3
"""
BNO085 Calibration & Monitoring Tool

Simple interface for:
1. Starting calibration procedure
2. Monitoring progress (level 0-3)
3. Recording final results
4. Viewing absolute orientation in real-time
"""

import serial
import json
import time
import sys
import argparse
from datetime import datetime

class BNOMonitor:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.start_time = None
        self.readings = []
        self.last_cal = -1

    def connect(self):
        """Open serial connection to Arduino"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(2)  # Wait for Arduino to stabilize

            # Clear any old data in buffer
            while self.ser.in_waiting:
                self.ser.readline()

            print(f"✓ Connected to {self.port}")
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.ser:
            self.ser.close()

    def calibrate(self, duration_seconds=600):
        """
        Run calibration procedure with real-time progress display.

        Args:
            duration_seconds: Maximum duration (default 10 minutes)
        """
        print("\n" + "="*70)
        print(" "*20 + "BNO085 CALIBRATION PROCEDURE")
        print("="*70)
        print("\nInstructions:")
        print("  1. PICK UP the Arduino board (hold by the edges)")
        print("  2. MOVE in LARGE figure-8 patterns continuously")
        print("  3. ROTATE through ALL 3 AXES (don't just rock it)")
        print("  4. KEEP MOVING for 5-10 minutes until level 3 reached")
        print("  5. Watch for 'EEPROM SAVED' message\n")
        print("Calibration Levels:")
        print("  ░░░ (0) = Uncalibrated")
        print("  █░░ (1) = Low")
        print("  ██░ (2) = Medium ← Auto-saves")
        print("  ███ (3) = High (YOUR GOAL)\n")
        print("="*70)
        print("\nPress ENTER to start moving, CTRL+C to stop...\n")

        try:
            input()  # Wait for user to press enter
        except KeyboardInterrupt:
            print("\nCanceled.")
            return

        self.start_time = time.time()
        self.last_cal = -1
        self.last_status_print = 0
        saved_to_eeprom = False
        final_level = -1

        print("🔄 Monitoring calibration...\n")

        while time.time() - self.start_time < duration_seconds:
            try:
                if not self.ser.is_open:
                    print("✗ Serial connection lost!")
                    break

                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='replace').strip()

                    # Check for EEPROM save message (always show this)
                    if '✓✓✓' in line or 'Calibration saved' in line:
                        print(f"\n{'='*70}")
                        print(f"🎉 {line}")
                        print(f"{'='*70}\n")
                        saved_to_eeprom = True
                        final_level = self.last_cal

                    # Parse JSON data
                    if line.startswith('{'):
                        try:
                            data = json.loads(line)
                            cal = data['orientation']['calibration']['system']
                            self.readings.append(data)

                            # ONLY show output when level CHANGES or on status interval
                            elapsed = time.time() - self.start_time

                            if cal != self.last_cal:
                                # Level changed - always show this
                                self.last_cal = cal
                                elapsed_s = int(elapsed)
                                bars = ["░░░", "█░░", "██░", "███"]
                                bar = bars[min(cal, 3)]
                                status = ["Uncalibrated", "Low", "Medium", "High"][cal] if cal < 4 else "?"
                                print(f"[{elapsed_s:3d}s] ✓ Level {cal}: {bar}  ({status})")
                                self.last_status_print = elapsed

                                # Show Euler angles when level changes
                                euler = data['orientation']['euler']
                                print(f"         Orientation: Roll={euler['roll_deg']:7.1f}° Pitch={euler['pitch_deg']:7.1f}° Yaw={euler['yaw_deg']:7.1f}°")

                                # Encouragement at level 3
                                if cal >= 3:
                                    print("         ✓✓ LEVEL 3! Keep moving - saving to EEPROM...")

                            # Periodically show status even without level change (every 30 sec)
                            elif elapsed - self.last_status_print > 30:
                                euler = data['orientation']['euler']
                                elapsed_s = int(elapsed)
                                print(f"[{elapsed_s:3d}s] Still at level {self.last_cal}... Keep moving!")
                                self.last_status_print = elapsed

                        except json.JSONDecodeError:
                            pass

            except KeyboardInterrupt:
                break

            time.sleep(0.01)

        elapsed = time.time() - self.start_time
        print(f"\n{'='*70}")
        print(f"Calibration session complete! (Duration: {elapsed:.0f} seconds)")
        print(f"Final level: {self.last_cal}")

        if saved_to_eeprom:
            print(f"\n✓✓✓ SUCCESS: Calibration saved to EEPROM (Level {final_level})")
            print("    System will remember this calibration after power cycle")
        else:
            if self.last_cal >= 2:
                print(f"\n⚠ Level {self.last_cal} reached but may not be saved yet")
                print("    Keep moving for a few more seconds to trigger auto-save")
            else:
                print("\n⚠ Calibration not complete - need to keep moving")
                print("    Target: Level 3 (███) for best results")
        print(f"{'='*70}\n")

    def monitor(self, duration_seconds=300):
        """
        Monitor orientation output in real-time

        Args:
            duration_seconds: Duration to monitor (default 5 minutes)
        """
        print("\n" + "="*70)
        print(" "*15 + "BNO085 ORIENTATION MONITORING")
        print("="*70)
        print("\nReal-time absolute orientation output (quaternion + Euler)")
        print("(Updated every 5 readings, ~0.5 seconds)\n")
        print("Press CTRL+C to stop\n")

        self.start_time = time.time()
        reading_count = 0

        try:
            while time.time() - self.start_time < duration_seconds:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='replace').strip()

                    if line.startswith('{'):
                        try:
                            data = json.loads(line)
                            reading_count += 1

                            # Extract data
                            timestamp = data['timestamp']
                            q = data['orientation']
                            euler = q['euler']
                            cal = q['calibration']

                            # Display every 5 readings (~0.5 seconds) to reduce spam
                            if reading_count % 5 == 0:
                                cal_bar = "░░░█░░██░███"[cal['system']*3:(cal['system']+1)*3]
                                print(
                                    f"[{timestamp:6d}ms] Q:({q['w']:7.4f},{q['x']:7.4f},{q['y']:7.4f},{q['z']:7.4f}) "
                                    f"Roll:{euler['roll_deg']:7.1f}° Pitch:{euler['pitch_deg']:7.1f}° Yaw:{euler['yaw_deg']:7.1f}° "
                                    f"Cal:{cal_bar}"
                                )
                        except json.JSONDecodeError:
                            pass
        except KeyboardInterrupt:
            elapsed = time.time() - self.start_time
            print(f"\n{'='*70}")
            print(f"Monitoring stopped ({elapsed:.0f}s, {reading_count} readings)")
            print(f"{'='*70}\n")

def main():
    parser = argparse.ArgumentParser(description='BNO085 Calibration & Monitoring Tool')
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyACM1)')
    parser.add_argument('--mode', choices=['calibrate', 'monitor'], default='calibrate',
                        help='Operation mode (default: calibrate)')
    parser.add_argument('--duration', type=int, default=600,
                        help='Duration in seconds (default: 600 for calibrate, 300 for monitor)')
    parser.add_argument('--baud', type=int, default=115200,
                        help='Baud rate (default: 115200)')

    args = parser.parse_args()

    monitor = BNOMonitor(args.port, args.baud)

    if not monitor.connect():
        sys.exit(1)

    try:
        if args.mode == 'calibrate':
            monitor.calibrate(args.duration)
        else:
            monitor.monitor(args.duration)
    finally:
        monitor.disconnect()

if __name__ == '__main__':
    main()
