"""Core serial I/O helpers for the simulator package.

Provides open_serial_write() and write_line() used by all scenario modules.
"""

import os
import sys

try:
    import fcntl
    import termios

    HAS_TERMIOS = True
except ImportError:
    HAS_TERMIOS = False


def open_serial_write(port, baud=115200):
    """Open serial port for writing with raw termios settings."""
    if not HAS_TERMIOS:
        raise RuntimeError("termios not available (Windows?). Use --stdout instead.")

    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = 0  # iflag
        attrs[1] = 0  # oflag
        attrs[3] = 0  # lflag
        attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.HUPCL)
        attrs[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD

        baud_const = getattr(termios, f"B{baud}", None)
        if baud_const is None:
            baud_const = baud
        attrs[4] = baud_const
        attrs[5] = baud_const
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0

        termios.tcsetattr(fd, termios.TCSANOW, attrs)

        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
    except Exception:
        os.close(fd)
        raise

    return fd


def write_line(fd, line):
    """Write a line to fd (file descriptor or 'stdout')."""
    data = (line + "\n").encode()
    if fd == "stdout":
        sys.stdout.write(line + "\n")
        sys.stdout.flush()
    else:
        os.write(fd, data)
