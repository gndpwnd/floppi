#!/usr/bin/env python3
"""Serial data simulator for fc_tool testing.

Thin wrapper around the simulator package. Kept for backwards compatibility
with existing bash test scripts (test_plotter.sh, test_monitor.sh).

For new code, use:
    python3 -m simulator --stdout --scenario imu
    from simulator.imu import scenario_imu
"""

from simulator.__main__ import main

if __name__ == "__main__":
    main()
