"""Python ports of fc_tool's JS protocol parsers.

Exact regex ports from the JS source for unit testing without a browser.
These parsers replicate the behavior of the JS code, not approximate it.

Sources:
  - parseLine()         → plotter.js:138  (NAMED_RE + CSV fallback)
  - parse_ansi()        → ansi.js:15      (ANSI_REGEX + SGR code map)
  - parse_dashboard()   → dashboard.js:8  (DASHBOARD_KV_RE)
  - format_val()        → cursors.js:74   (formatVal)
  - RunningStats        → plotter.js       (stat tracking accumulator)
  - cursor_delta()      → cursors.js       (cursor delta calculation)
"""

import math
import re
from dataclasses import dataclass, field
from typing import Optional


# ============================================================================
# Data types
# ============================================================================

@dataclass
class DataPoint:
    """Single parsed data point from a serial line."""
    name: str
    plot_id: int
    value: float


@dataclass
class AnsiSpan:
    """A text span with ANSI style attributes."""
    text: str
    bold: bool = False
    dim: bool = False
    underline: bool = False
    color: Optional[str] = None  # CSS class name, e.g. 'ansi-red'


# ============================================================================
# Protocol parser — ports plotter.js parseLine()
# ============================================================================

# Exact port of plotter.js:138
# JS: /([\w.]+)(?:@(\d+))?[=:]([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/g
NAMED_RE = re.compile(r'([\w.]+)(?:@(\d+))?[=:]([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)')

# Fallback: split on whitespace/comma/tab, parse as numbers
CSV_SPLIT_RE = re.compile(r'[\s,\t]+')


def parse_line(line: str) -> Optional[list[DataPoint]]:
    """Parse a serial line into data points.

    Supports: name@plotId:value, name:value, name=value, plain CSV/space-separated.
    Returns None if no data could be parsed.

    Exact port of parseLine() from plotter.js.
    """
    results = []
    for match in NAMED_RE.finditer(line):
        name = match.group(1)
        plot_id_str = match.group(2)
        value_str = match.group(3)
        plot_id = int(plot_id_str) if plot_id_str is not None else 0
        value = float(value_str)
        results.append(DataPoint(name=name, plot_id=plot_id, value=value))

    if results:
        return results

    # Fallback: plain numbers (CSV, space, or tab separated)
    parts = CSV_SPLIT_RE.split(line.strip())
    nums = []
    for p in parts:
        try:
            nums.append(float(p))
        except ValueError:
            continue

    if nums:
        return [DataPoint(name=f'ch{i}', plot_id=0, value=v) for i, v in enumerate(nums)]

    return None


# ============================================================================
# ANSI parser — ports ansi.js parseAnsi()
# ============================================================================

# Exact port of ansi.js:15
# JS: /\x1b\[([0-9;]*)m/g
ANSI_REGEX = re.compile(r'\x1b\[([0-9;]*)m')

# Exact port of ansi.js:8-13
ANSI_COLORS = {
    30: 'ansi-black',   31: 'ansi-red',     32: 'ansi-green',   33: 'ansi-yellow',
    34: 'ansi-blue',    35: 'ansi-magenta',  36: 'ansi-cyan',    37: 'ansi-white',
    90: 'ansi-bright-black', 91: 'ansi-bright-red', 92: 'ansi-bright-green', 93: 'ansi-bright-yellow',
    94: 'ansi-bright-blue', 95: 'ansi-bright-magenta', 96: 'ansi-bright-cyan', 97: 'ansi-bright-white',
}


def parse_ansi(text: str) -> list[AnsiSpan]:
    """Parse ANSI escape sequences and return styled text spans.

    Exact port of parseAnsi() from ansi.js. Returns AnsiSpan objects
    instead of DOM elements.
    """
    spans = []
    last_index = 0
    bold = False
    dim = False
    underline = False
    color = None

    for match in ANSI_REGEX.finditer(text):
        # Emit text before this escape sequence
        if match.start() > last_index:
            chunk = text[last_index:match.start()]
            if chunk:  # Skip empty strings
                spans.append(AnsiSpan(
                    text=chunk,
                    bold=bold,
                    dim=dim,
                    underline=underline,
                    color=color,
                ))
        last_index = match.end()

        # Parse SGR codes
        codes_str = match.group(1)
        codes = [int(c) for c in codes_str.split(';') if c] if codes_str else [0]
        for code in codes:
            if code == 0:
                bold = False
                dim = False
                underline = False
                color = None
            elif code == 1:
                bold = True
            elif code == 2:
                dim = True
            elif code == 4:
                underline = True
            elif code == 22:
                bold = False
                dim = False
            elif code == 24:
                underline = False
            elif code == 39:
                color = None
            elif code in ANSI_COLORS:
                color = ANSI_COLORS[code]

    # Emit remaining text after last escape sequence
    if last_index < len(text):
        chunk = text[last_index:]
        if chunk:
            spans.append(AnsiSpan(
                text=chunk,
                bold=bold,
                dim=dim,
                underline=underline,
                color=color,
            ))

    return spans


# ============================================================================
# Dashboard parser — ports dashboard.js DASHBOARD_KV_RE
# ============================================================================

# Exact port of dashboard.js:8
# JS: /([\w.]+)=(\S+)/g
DASHBOARD_KV_RE = re.compile(r'([\w.]+)=(\S+)')


def parse_dashboard_line(line: str) -> list[tuple[str, str]]:
    """Parse key=value pairs from a dashboard line.

    Exact port of the DASHBOARD_KV_RE matching in dashboard.js processLine().
    Returns list of (key, value) tuples.
    """
    return [(m.group(1), m.group(2)) for m in DASHBOARD_KV_RE.finditer(line)]


# ============================================================================
# Cursor helpers — ports cursors.js formatVal()
# ============================================================================

def format_val(v) -> str:
    """Format a numeric value for cursor readout display.

    Exact port of formatVal() from cursors.js:74-77.
    """
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return '--'
    if isinstance(v, int) or (isinstance(v, float) and math.isfinite(v) and v == int(v)):
        return str(int(v))
    return f'{v:.3f}'


# ============================================================================
# Statistics accumulator — ports plotter.js stat tracking
# ============================================================================

@dataclass
class RunningStats:
    """Incremental statistics accumulator matching plotter.js stat tracking."""
    count: int = 0
    min_val: float = float('inf')
    max_val: float = float('-inf')
    sum_val: float = 0.0

    def update(self, value: float) -> None:
        """Add a value to the running statistics."""
        self.count += 1
        if value < self.min_val:
            self.min_val = value
        if value > self.max_val:
            self.max_val = value
        self.sum_val += value

    @property
    def mean(self) -> float:
        """Calculate mean. Returns 0.0 if no values added."""
        if self.count == 0:
            return 0.0
        return self.sum_val / self.count

    def reset(self) -> None:
        """Reset all statistics."""
        self.count = 0
        self.min_val = float('inf')
        self.max_val = float('-inf')
        self.sum_val = 0.0


# ============================================================================
# Cursor delta — ports cursors.js delta calculation
# ============================================================================

def cursor_delta(v1: float, v2: float) -> float:
    """Calculate delta between two cursor positions.

    Returns the signed difference (v2 - v1), matching cursors.js behavior.
    """
    return v2 - v1
