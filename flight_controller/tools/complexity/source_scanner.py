"""
Scan C/C++ source files to count floating-point operations per feature tier.

Regex-based heuristic approach: counts arithmetic operators and math function
calls within #ifdef-guarded blocks to map operations to feature tiers.

This replaces the manually maintained operations.py — no need to update
when code changes, just re-run the scanner.
"""

import os
import re
import glob

# Math functions that map to specific cycle cost categories
TRIG_FUNCS = {"sinf", "cosf", "tanf", "asinf", "acosf", "atanf", "atan2f",
              "sin", "cos", "tan", "asin", "acos", "atan", "atan2"}
SQRT_FUNCS = {"sqrtf", "sqrt"}
MATH_FUNCS = TRIG_FUNCS | SQRT_FUNCS

# Feature flag to tier mapping
FLAG_TO_TIER = {
    "USE_OPTIMIZATION": "optimization",
    "USE_RACING": "racing",
    "CALIBRATION_MODE": "calibration",
}

# Files to scan (relative to src/)
SOURCE_PATTERNS = ["src/*.cpp", "lib/Calibration/*.cpp"]

# Regex for preprocessor directives
RE_IFDEF = re.compile(r'^\s*#if(?:def)?\s+(.+)')
RE_ELIF = re.compile(r'^\s*#elif\s+(.+)')
RE_ELSE = re.compile(r'^\s*#else\b')
RE_ENDIF = re.compile(r'^\s*#endif\b')
RE_COMMENT_LINE = re.compile(r'^\s*//')
RE_COMMENT_BLOCK_START = re.compile(r'/\*')
RE_COMMENT_BLOCK_END = re.compile(r'\*/')

# Regex for FP operations (heuristic — counts operators on lines with float context)
RE_FUNC_CALL = re.compile(r'\b(' + '|'.join(MATH_FUNCS) + r')\s*\(')


def _count_ops_in_line(line):
    """Count FP-like operations in a single line of code (heuristic)."""
    # Strip comments
    line = re.sub(r'//.*$', '', line)
    line = re.sub(r'/\*.*?\*/', '', line)

    ops = {"mul": 0, "add": 0, "div": 0, "trig": 0, "sqrt": 0}

    # Count math function calls
    for m in RE_FUNC_CALL.finditer(line):
        fname = m.group(1)
        if fname in TRIG_FUNCS:
            ops["trig"] += 1
        elif fname in SQRT_FUNCS:
            ops["sqrt"] += 1

    # Count arithmetic operators (rough: doesn't distinguish int from float)
    # But in flight controller code, most arithmetic IS float
    # Skip pointer dereferences (*ptr), address-of (&var), and comment markers
    code = line

    # Remove string literals
    code = re.sub(r'"[^"]*"', '', code)
    code = re.sub(r"'[^']*'", '', code)

    # Count operators
    # Multiply: * but not -> or ** or /* or */
    ops["mul"] += len(re.findall(r'(?<![/\*])\*(?![/\*])', code))
    # Subtract pointer dereferences (unary *): rough — if * follows ( or , or = or return
    ops["mul"] -= len(re.findall(r'[=(,\s]\s*\*\s*\w', code))
    ops["mul"] = max(0, ops["mul"])

    # Add/sub: + and - (excluding ++ and --)
    ops["add"] += len(re.findall(r'(?<!\+)\+(?!\+)', code))
    ops["add"] += len(re.findall(r'(?<!-)-(?![->=])', code))

    # Divide: / but not // or /* or */
    ops["div"] += len(re.findall(r'(?<![/\*])/(?![/\*])', code))

    return ops


def _detect_tier(ifdef_stack):
    """Determine feature tier from the current #ifdef nesting stack."""
    for condition in reversed(ifdef_stack):
        for flag, tier in FLAG_TO_TIER.items():
            if flag in condition:
                return tier
    return "base"


def scan_source_file(filepath):
    """
    Scan a single C/C++ source file for FP operations.

    Returns dict: {tier: {mul, add, div, trig, sqrt}} where tier
    is "base", "optimization", "racing", or "calibration".
    """
    tiers = {}

    ifdef_stack = []
    in_block_comment = False

    with open(filepath, 'r') as f:
        for line in f:
            # Track block comments
            if in_block_comment:
                if RE_COMMENT_BLOCK_END.search(line):
                    in_block_comment = False
                continue
            if RE_COMMENT_BLOCK_START.search(line) and not RE_COMMENT_BLOCK_END.search(line):
                in_block_comment = True
                continue

            # Skip line comments
            if RE_COMMENT_LINE.match(line):
                continue

            # Track preprocessor conditionals
            m = RE_IFDEF.match(line)
            if m:
                ifdef_stack.append(m.group(1).strip())
                continue
            if RE_ELIF.match(line):
                if ifdef_stack:
                    ifdef_stack[-1] = RE_ELIF.match(line).group(1).strip()
                continue
            if RE_ELSE.match(line):
                if ifdef_stack:
                    ifdef_stack[-1] = "ELSE_" + ifdef_stack[-1]
                continue
            if RE_ENDIF.match(line):
                if ifdef_stack:
                    ifdef_stack.pop()
                continue

            # Count operations on this line
            ops = _count_ops_in_line(line)
            if any(v > 0 for v in ops.values()):
                tier = _detect_tier(ifdef_stack)
                if tier not in tiers:
                    tiers[tier] = {"mul": 0, "add": 0, "div": 0, "trig": 0, "sqrt": 0}
                for k, v in ops.items():
                    tiers[tier][k] += v

    return tiers


def scan_all_sources(project_root):
    """
    Scan all flight controller source files for FP operations.

    Returns:
        dict: {filename: {tier: {mul, add, div, trig, sqrt}}}
        dict: {tier: {mul, add, div, trig, sqrt}} (aggregated totals)
    """
    per_file = {}
    totals = {}

    for pattern in SOURCE_PATTERNS:
        full_pattern = os.path.join(project_root, pattern)
        for filepath in sorted(glob.glob(full_pattern)):
            relpath = os.path.relpath(filepath, project_root)
            file_tiers = scan_source_file(filepath)
            if file_tiers:
                per_file[relpath] = file_tiers
                for tier, ops in file_tiers.items():
                    if tier not in totals:
                        totals[tier] = {"mul": 0, "add": 0, "div": 0, "trig": 0, "sqrt": 0}
                    for k, v in ops.items():
                        totals[tier][k] += v

    return per_file, totals
