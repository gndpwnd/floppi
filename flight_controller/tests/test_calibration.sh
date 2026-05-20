#!/bin/bash
# Thin wrapper — delegates to tests/suites/test_calibration.sh.
#
# This entry point is preserved so existing operator workflows and any
# external scripts/CI that call `./tests/test_calibration.sh` continue to
# work after the test-infrastructure-v2 refactor (see
# docs/findings/test_infrastructure_v2_2026-05-20.md).
#
# All real logic lives in:
#   tests/lib/harness.sh           — shared helpers
#   tests/suites/test_calibration.sh — the 18 calibration tests
#
# Usage: same as the suite script.
#   ./tests/test_calibration.sh                    # all tests on /dev/ttyACM0
#   ./tests/test_calibration.sh /dev/ttyACM0 help  # single test
#   ./tests/test_calibration.sh /dev/ttyACM0 all   # all tests (explicit)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "$SCRIPT_DIR/suites/test_calibration.sh" "$@"
