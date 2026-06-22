#!/usr/bin/env python3
"""
validate_photo_backup.py — host-side validator for the Uno guided-tuning
photo-backup block before the operator pastes it into balance_constants.h.

The Uno's setup-mode photo-backup printer (see
auto_orientation/src/applications/balancing_robot_uno/tune_storage.cpp
:: print_photo_backup) emits a banner-delimited block to Serial that the
operator photographs as a paper backup of their tuned PID gains and IMU
calibration blob. Restoring from that photo means OCR'ing the image (or
hand-typing it) and pasting into balance_constants.h, then reflashing
arduino_uno_minimal.

OCR is lossy. A single 'O' read as '0' (or vice versa) in a hex byte will
silently corrupt the cold-start seed; the bot may then fail to balance with
no obvious error. This script catches OCR misreads BEFORE the paste:

  1. Strips OCR garbage (lines outside banners, smart-quote / em-dash
     substitutions, leading/trailing whitespace).
  2. Parses the four `static const float BALANCE_*`/`PITCH_OFFSET_DEG`
     lines and the `<TAG>_CAL_BLOB[N] = { 0xAB, 0xCD, ... };` block.
  3. Validates each value (float ranges, hex byte count, hex digit set).
  4. Sanity-flags common OCR confusions (O->0, l->1, S->5, B->8, etc.)
     in hex contexts and offers a corrected suggestion.
  5. On success, emits a reformatted block exactly matching the firmware's
     print_photo_backup() output — safe to paste into balance_constants.h.

CRC-8 limitation: print_photo_backup() does NOT print the CRC byte that
the firmware writes alongside the EEPROM record (see tune_storage.cpp ::
save_tuning / save_cal_blob). The CRC is a runtime EEPROM-integrity check
only; the operator-visible block has no CRC of its own. This validator
therefore cannot prove byte-perfect fidelity to what was on the chip —
only that the parsed values are syntactically sane and within plausible
ranges. The --strict flag is reserved for a future format revision that
includes a print-time CRC.

Usage:
    python3 tools/validate_photo_backup.py photo_ocr.txt
    cat photo_ocr.txt | python3 tools/validate_photo_backup.py -
    python3 tools/validate_photo_backup.py --help

Exit status:
    0  block is valid; reformatted output printed to stdout
    1  block has parse / validation errors; numbered issue list to stderr
    2  CLI / I/O error (bad file, etc.)

Python 3 standard library only; no third-party dependencies.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from typing import Optional


# ---------------------------------------------------------------------------
# Banner + line patterns — kept in sync with tune_storage.cpp print_photo_backup.
# ---------------------------------------------------------------------------

BANNER_START = "==== PHOTO-BACKUP -- paste into balance_constants.h ===="
BANNER_END = "==== END PHOTO-BACKUP -- PHOTOGRAPH THIS SCROLLBACK ===="

# print_photo_backup uses fixed-width column padding before `=`. After
# whitespace-normalization we just need the symbol name and the value.
#   static const float BALANCE_KP       = 94.4873f;
FLOAT_LINE_RE = re.compile(
    r"^\s*static\s+const\s+float\s+"
    r"(?P<name>BALANCE_KP|BALANCE_KI|BALANCE_KD|PITCH_OFFSET_DEG)"
    r"\s*=\s*(?P<value>[-+0-9.eE]+)\s*[fF]?\s*;\s*$"
)

# Cal blob header: `static const uint8_t <TAG>_CAL_BLOB[<len>] = {`
CAL_HEADER_RE = re.compile(
    r"^\s*static\s+const\s+uint8_t\s+"
    r"(?P<tag>[A-Za-z0-9_]+?)_CAL_BLOB"
    r"\s*\[\s*(?P<len>\d+)\s*\]"
    r"\s*=\s*\{\s*$"
)

CAL_CLOSE_RE = re.compile(r"^\s*\}\s*;\s*$")

# A single hex byte token, tolerating uppercase / lowercase, optional 0x.
# We deliberately match a broader alphabet here so we can detect OCR slips
# like "0xOA" instead of "0x0A" and report them with a suggestion.
HEX_TOKEN_RE = re.compile(r"0[xX][0-9A-Za-z]{1,4}")

# Permitted float-line value ranges (rough sanity bounds, not safety limits).
FLOAT_RANGES = {
    "BALANCE_KP":       (0.0, 1000.0),
    "BALANCE_KI":       (0.0, 1000.0),
    "BALANCE_KD":       (0.0, 1000.0),
    "PITCH_OFFSET_DEG": (-20.0, 20.0),
}

# OCR confusion table — keys are characters that frequently appear in OCR
# output of a hex byte but are NOT valid hex digits, mapped to the digit the
# operator likely meant. Used to suggest fixes for misread hex bytes.
OCR_HEX_FIXES = {
    "O": "0",  # capital O -> zero
    "o": "0",  # lowercase o -> zero (rare in uppercase hex but possible)
    "l": "1",  # lowercase L -> one
    "I": "1",  # capital I -> one
    "i": "1",  # lowercase i -> one
    "S": "5",  # capital S -> five
    "s": "5",  # lowercase s -> five
    "Z": "2",  # capital Z -> two
    "z": "2",
    "G": "6",  # capital G -> six (less common, but possible in low-res photos)
    "g": "9",  # lowercase g -> nine
    "T": "7",  # capital T -> seven
    "q": "9",  # lowercase q -> nine
    "B": "8",  # capital B -> eight (rare; B is itself a hex digit, see below)
}
# Note: 'B' is a valid hex digit, so we never auto-suggest B->8 — see
# suggest_hex_fix() which excludes already-valid hex characters.

# Smart-quote / dash substitutions some OCR engines insert. OCR engines very
# commonly fuse a typewritten `--` into an em-dash glyph, which would break the
# banner match (`==== PHOTO-BACKUP -- paste...`). Map em-dash and en-dash back
# to `--` so the banner and any in-block comments survive that pass. Plain
# unicode minus, by contrast, almost always represents a single `-` sign
# (e.g. negative PITCH_OFFSET_DEG), so it maps to `-`.
OCR_TEXT_FIXES = [
    ("“", '"'),   # left double curly
    ("”", '"'),   # right double curly
    ("‘", "'"),   # left single curly
    ("’", "'"),   # right single curly
    ("—", "--"),  # em-dash -> double-dash (banner / comment context)
    ("–", "--"),  # en-dash -> double-dash (same)
    ("−", "-"),   # unicode minus -> single dash (numeric sign context)
    (" ", " "),   # non-breaking space
]


# ---------------------------------------------------------------------------
# Issue reporting
# ---------------------------------------------------------------------------

@dataclass
class Issue:
    """One validation problem, optionally tied to a line in the input."""
    line_no: Optional[int]
    message: str
    suggestion: Optional[str] = None

    def render(self) -> str:
        loc = f"line {self.line_no}: " if self.line_no is not None else ""
        out = f"{loc}{self.message}"
        if self.suggestion:
            out += f"\n    suggestion: {self.suggestion}"
        return out


@dataclass
class ParseResult:
    floats: dict = field(default_factory=dict)         # name -> (value, line_no)
    cal_tag: Optional[str] = None
    cal_declared_len: Optional[int] = None
    cal_bytes: list = field(default_factory=list)      # list of (int, line_no, raw_token)
    cal_header_line: Optional[int] = None
    issues: list = field(default_factory=list)


# ---------------------------------------------------------------------------
# OCR normalization
# ---------------------------------------------------------------------------

def normalize_text(raw: str) -> str:
    """Strip smart-quote / dash substitutions an OCR pass may insert."""
    out = raw
    for src, dst in OCR_TEXT_FIXES:
        out = out.replace(src, dst)
    return out


def extract_block(lines: list) -> tuple:
    """
    Find the photo-backup block between the start/end banners.
    Returns (block_lines, start_line_no, end_line_no, issues).
    block_lines is the content BETWEEN the banners (banners excluded), each
    item is (original_line_no, content_string).
    """
    issues = []
    start_idx = None
    end_idx = None
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if start_idx is None and BANNER_START in stripped:
            start_idx = idx
        elif start_idx is not None and BANNER_END in stripped:
            end_idx = idx
            break

    if start_idx is None:
        issues.append(Issue(
            None,
            f"missing start banner: {BANNER_START!r}",
            "is the photo-backup block in the file? "
            "did OCR mangle the '====' delimiters?",
        ))
        return [], None, None, issues

    if end_idx is None:
        issues.append(Issue(
            start_idx + 1,
            f"missing end banner: {BANNER_END!r}",
            "photo-backup block is unterminated; "
            "is part of the scrollback missing from the photo?",
        ))
        return [], start_idx + 1, None, issues

    block = [
        (i + 1, lines[i].rstrip("\n"))
        for i in range(start_idx + 1, end_idx)
    ]
    return block, start_idx + 1, end_idx + 1, issues


def suggest_hex_fix(token: str) -> Optional[str]:
    """
    If `token` looks like an OCR-mangled hex byte ("0xOA", "0xlF"), return a
    suggested correction with confusable characters mapped to digits. Returns
    None if the token is already valid hex or has no fixable confusables.
    """
    if not token.lower().startswith("0x"):
        return None
    body = token[2:]
    if all(c in "0123456789abcdefABCDEF" for c in body):
        return None
    fixed_chars = []
    changed = False
    for ch in body:
        if ch in "0123456789abcdefABCDEF":
            fixed_chars.append(ch)
        elif ch in OCR_HEX_FIXES:
            fixed_chars.append(OCR_HEX_FIXES[ch])
            changed = True
        else:
            return None  # un-fixable character
    if not changed:
        return None
    return "0x" + "".join(fixed_chars).upper()


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

def parse_block(block: list) -> ParseResult:
    """Walk the block content and pull out floats + cal bytes."""
    result = ParseResult()

    in_cal = False
    cal_close_line = None

    for line_no, raw in block:
        line = raw.strip()
        if not line or line.startswith("//"):
            continue

        if not in_cal:
            m = FLOAT_LINE_RE.match(line)
            if m:
                name = m.group("name")
                value_str = m.group("value")
                try:
                    value = float(value_str)
                except ValueError:
                    result.issues.append(Issue(
                        line_no,
                        f"could not parse float for {name}: {value_str!r}",
                        "expected e.g. '94.4873f' — check for OCR digit slips",
                    ))
                    continue
                if name in result.floats:
                    result.issues.append(Issue(
                        line_no,
                        f"duplicate {name} declaration "
                        f"(first seen on line {result.floats[name][1]})",
                    ))
                result.floats[name] = (value, line_no)
                continue

            mh = CAL_HEADER_RE.match(line)
            if mh:
                result.cal_tag = mh.group("tag")
                try:
                    result.cal_declared_len = int(mh.group("len"))
                except ValueError:
                    result.issues.append(Issue(
                        line_no,
                        f"could not parse cal blob length: {mh.group('len')!r}",
                    ))
                result.cal_header_line = line_no
                in_cal = True
                continue

            # Unrecognized non-empty, non-comment line — flag softly.
            result.issues.append(Issue(
                line_no,
                f"unrecognized line inside photo-backup block: {line!r}",
                "expected a `static const float ...` or cal-blob line; "
                "is this OCR noise?",
            ))
        else:
            # Inside the cal-blob `{ ... };` body.
            if CAL_CLOSE_RE.match(line):
                in_cal = False
                cal_close_line = line_no
                continue

            for tok_match in HEX_TOKEN_RE.finditer(line):
                tok = tok_match.group(0)
                body = tok[2:]
                # Length sanity: hex bytes should be 1-2 digits.
                if len(body) > 2:
                    result.issues.append(Issue(
                        line_no,
                        f"hex token {tok!r} has more than 2 digits — "
                        "two adjacent bytes likely missed a separator",
                    ))
                    continue
                if all(c in "0123456789abcdefABCDEF" for c in body):
                    val = int(body, 16)
                    if not (0 <= val <= 0xFF):
                        result.issues.append(Issue(
                            line_no,
                            f"hex byte {tok!r} out of [0x00, 0xFF]",
                        ))
                    else:
                        result.cal_bytes.append((val, line_no, tok))
                else:
                    fix = suggest_hex_fix(tok)
                    if fix is not None:
                        result.issues.append(Issue(
                            line_no,
                            f"suspicious hex byte {tok!r} — "
                            "contains a character that is not a hex digit",
                            f"did you mean {fix} instead of {tok}?",
                        ))
                    else:
                        result.issues.append(Issue(
                            line_no,
                            f"invalid hex byte {tok!r}",
                            "hex digits must be 0-9 and A-F only",
                        ))

    if in_cal:
        result.issues.append(Issue(
            result.cal_header_line,
            "cal blob block was opened but never closed with '};' "
            f"(opened on line {result.cal_header_line})",
            "is the closing brace cut off from the photo?",
        ))

    # Cross-checks once the walk is done.
    if result.cal_declared_len is not None:
        actual = len(result.cal_bytes)
        if actual != result.cal_declared_len:
            result.issues.append(Issue(
                cal_close_line or result.cal_header_line,
                f"cal blob length mismatch: header declared "
                f"[{result.cal_declared_len}] but found {actual} hex bytes",
                "BNO055 cal blobs are 22 bytes; "
                "check for missing / duplicated bytes from OCR",
            ))

    return result


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

def validate(result: ParseResult) -> list:
    """Run the range / completeness checks that depend on parsed values."""
    issues = list(result.issues)

    has_any_pid = any(n in result.floats for n in FLOAT_RANGES)
    has_full_pid = all(n in result.floats for n in FLOAT_RANGES)
    if has_any_pid and not has_full_pid:
        missing = [n for n in FLOAT_RANGES if n not in result.floats]
        issues.append(Issue(
            None,
            "partial PID block — missing: " + ", ".join(missing),
            "photo-backup PID block prints all four float lines atomically; "
            "a partial block means OCR dropped one — re-shoot the photo",
        ))

    for name, (value, line_no) in result.floats.items():
        lo, hi = FLOAT_RANGES[name]
        if not (lo <= value <= hi):
            issues.append(Issue(
                line_no,
                f"{name} = {value!r} is outside the sanity range [{lo}, {hi}]",
                "if this is intentional, hand-edit balance_constants.h instead "
                "of relying on the validator",
            ))

    if result.cal_tag is not None:
        # Specifically warn if the operator's target is balance_constants.h
        # but the cal blob tag is not BNO055 (the current Uno IMU).
        if result.cal_tag != "BNO055" and result.cal_declared_len == 22:
            issues.append(Issue(
                result.cal_header_line,
                f"cal blob tag is {result.cal_tag!r}, not 'BNO055'",
                "balance_constants.h expects BNO055_CAL_BLOB; "
                "is this from a different IMU?",
            ))

    # Must have produced SOMETHING worth pasting.
    if not result.floats and not result.cal_bytes:
        issues.append(Issue(
            None,
            "no PID block and no cal blob block were found in the photo-backup",
            "did print_photo_backup() actually emit a block? "
            "the firmware only prints sections for which EEPROM holds valid data",
        ))

    return issues


# ---------------------------------------------------------------------------
# Reformat — mirror print_photo_backup() output exactly.
# ---------------------------------------------------------------------------

def reformat(result: ParseResult) -> str:
    """
    Reproduce print_photo_backup()'s output for the parsed values. The
    operator can paste this directly into balance_constants.h and the result
    is byte-equivalent to the firmware's own emission.
    """
    lines = []
    lines.append(BANNER_START)
    lines.append("// Uno guided session output. Photograph this block.")

    if all(n in result.floats for n in FLOAT_RANGES):
        # print_photo_backup pads the symbol-name field to 16 characters,
        # then prints a literal " = " — so BALANCE_KP gets 6 trailing spaces
        # and PITCH_OFFSET_DEG gets none. Reproduce that exactly.
        for name in ("BALANCE_KP", "BALANCE_KI", "BALANCE_KD",
                     "PITCH_OFFSET_DEG"):
            value, _ = result.floats[name]
            lines.append(
                f"static const float {name:<16} = {value:.4f}f;"
            )

    if result.cal_bytes and result.cal_tag is not None:
        tag = result.cal_tag
        n = len(result.cal_bytes)
        lines.append(
            f"// {tag} {n}-byte cal blob "
            "(hex, MSB-first as stored in EEPROM):"
        )
        lines.append(f"static const uint8_t {tag}_CAL_BLOB[{n}] = {{")
        body = ", ".join(f"0x{b:02X}" for (b, _, _) in result.cal_bytes)
        lines.append(f"  {body}")
        lines.append("};")

    lines.append(BANNER_END)
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def read_input(path: str) -> str:
    if path == "-":
        return sys.stdin.read()
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


def main(argv: Optional[list] = None) -> int:
    parser = argparse.ArgumentParser(
        prog="validate_photo_backup.py",
        description=(
            "Validate a photographed-and-OCR'd Uno photo-backup block before "
            "pasting into balance_constants.h. Catches OCR misreads that "
            "would otherwise produce a silently-corrupt cold-start seed."
        ),
        epilog=(
            "Exit code 0 means the block is valid and a reformatted, "
            "paste-ready version is on stdout. Non-zero means issues were "
            "found; see stderr for the numbered list."
        ),
    )
    parser.add_argument(
        "input",
        help="path to OCR'd text file, or '-' to read from stdin",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help=(
            "reserved for future format revisions that include a print-time "
            "CRC; currently a no-op because print_photo_backup() does not "
            "emit a CRC byte (CRC is firmware-internal, computed at EEPROM "
            "write time)"
        ),
    )
    args = parser.parse_args(argv)

    try:
        raw = read_input(args.input)
    except OSError as exc:
        print(f"error: could not read {args.input!r}: {exc}", file=sys.stderr)
        return 2

    text = normalize_text(raw)
    lines = text.splitlines()

    block, _start, _end, banner_issues = extract_block(lines)
    if banner_issues and not block:
        # No usable block — report and exit.
        print("INVALID — photo-backup block could not be located:",
              file=sys.stderr)
        for i, issue in enumerate(banner_issues, 1):
            print(f"  {i}. {issue.render()}", file=sys.stderr)
        return 1

    result = parse_block(block)
    # Surface banner-related issues alongside parse issues.
    issues = banner_issues + validate(result)

    if args.strict:
        # Surfacing this once at the top is friendlier than silently no-op'ing.
        print(
            "note: --strict requested, but print_photo_backup() does not emit "
            "a CRC byte; --strict is currently a no-op.",
            file=sys.stderr,
        )

    if issues:
        print(
            f"INVALID — {len(issues)} issue(s) found; do NOT paste into "
            "balance_constants.h:",
            file=sys.stderr,
        )
        for i, issue in enumerate(issues, 1):
            print(f"  {i}. {issue.render()}", file=sys.stderr)
        return 1

    print("VALID -- safe to paste into balance_constants.h")
    print()
    sys.stdout.write(reformat(result))
    return 0


if __name__ == "__main__":
    sys.exit(main())
