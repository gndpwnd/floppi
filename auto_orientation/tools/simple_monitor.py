#!/usr/bin/env python3
"""Simple verbose serial monitor for BNO085 calibration feedback."""

import serial
import json
import sys
import time
from datetime import datetime

def close_port(ser):
    """Safely close serial port."""
    if ser and ser.is_open:
        ser.close()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 simple_monitor.py /dev/ttyACM1")
        sys.exit(1)

    port = sys.argv[1]
    ser = None

    try:
        try:
            ser = serial.Serial(port, 115200, timeout=1.0)
        except serial.SerialException as e:
            print(f"Error opening {port}: {e}")
            print(f"Check if another process is using the port.")
            print(f"Try: lsof {port}")
            sys.exit(1)

        print(f"\n{'='*60}")
        print(f"Connected to {port} at 115200 baud")
        print(f"Press CTRL+C to exit")
        print(f"{'='*60}\n")

        time.sleep(0.5)

        line_count = 0
        json_count = 0
        error_count = 0
        last_cal_values = None

        while True:
            try:
                if not ser.is_open:
                    print("ERROR: Serial port closed unexpectedly!")
                    break

                # Read with timeout
                if not ser.in_waiting:
                    time.sleep(0.01)
                    continue

                line = ser.readline().decode('utf-8', errors='replace').strip()

                if not line:
                    continue

                line_count += 1
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

                # If it's JSON, parse it
                if line.startswith('{'):
                    try:
                        data = json.loads(line)
                        json_count += 1

                        ori = data.get('orientation', {})
                        cal = ori.get('calibration', {})

                        sys_cal = cal.get('system', -1)
                        acc_cal = cal.get('accel', -1)
                        gyro_cal = cal.get('gyro', -1)
                        mag_cal = cal.get('mag', -1)

                        w = ori.get('w')
                        x = ori.get('x')
                        y = ori.get('y')
                        z = ori.get('z')

                        # Show calibration with visual bars
                        bars = {
                            0: "░░░",  # empty
                            1: "█░░",  # 1/3
                            2: "██░",  # 2/3
                            3: "███",  # full
                        }

                        sys_bar = bars.get(sys_cal, "???")
                        acc_bar = bars.get(acc_cal, "???")
                        gyro_bar = bars.get(gyro_cal, "???")
                        mag_bar = bars.get(mag_cal, "???")

                        print(f"[{timestamp}] JSON #{json_count} PARSED OK")
                        print(f"  Quaternion: w={w:8.6f} x={x:8.6f} y={y:8.6f} z={z:8.6f}")
                        print(f"  Calibration:")
                        print(f"    System: {sys_cal} {sys_bar}")
                        print(f"    Accel:  {acc_cal} {acc_bar}")
                        print(f"    Gyro:   {gyro_cal} {gyro_bar}")
                        print(f"    Mag:    {mag_cal} {mag_bar}")

                        # Check if calibrated
                        all_at_max = (sys_cal == 3 and acc_cal == 3 and gyro_cal == 3 and mag_cal == 3)
                        any_nonzero = (sys_cal > 0 or acc_cal > 0 or gyro_cal > 0 or mag_cal > 0)

                        if all_at_max:
                            print(f"  ✓✓✓ FULLY CALIBRATED! ✓✓✓")
                        elif any_nonzero:
                            print(f"  ⚠ Calibrating... (move board in figure-8)")
                        else:
                            print(f"  ✗ NOT CALIBRATED - move board now!")

                        print()
                        last_cal_values = (sys_cal, acc_cal, gyro_cal, mag_cal)

                    except json.JSONDecodeError as e:
                        error_count += 1
                        print(f"[{timestamp}] JSON ERROR: {e}")
                        print(f"  Line: {line[:100]}")
                        print()
                else:
                    # Regular text output (debug messages, etc.)
                    print(f"[{timestamp}] {line}")

            except serial.SerialException as e:
                print(f"Serial error: {e}")
                break

    except KeyboardInterrupt:
        print(f"\n\n{'='*60}")
        print(f"Monitor stopped by user (CTRL+C)")
        print(f"Statistics:")
        print(f"  Total lines: {line_count}")
        print(f"  Valid JSON: {json_count}")
        print(f"  JSON errors: {error_count}")
        if last_cal_values:
            print(f"  Last calibration: System={last_cal_values[0]}, Accel={last_cal_values[1]}, Gyro={last_cal_values[2]}, Mag={last_cal_values[3]}")
        print(f"{'='*60}\n")

    except Exception as e:
        print(f"Unexpected error: {e}")

    finally:
        # Always close the port
        close_port(ser)
        sys.exit(0)

if __name__ == '__main__':
    main()
