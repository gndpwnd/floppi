#!/usr/bin/env bash
#
# build_matrix.sh - PlatformIO multi-environment build matrix
#
# Builds every [env:*] defined in platformio.ini and prints a one-line
# summary per env showing STATUS / FLASH / RAM. Returns 0 if all envs
# built, 1 if any failed.
#
# Usage:
#   tools/build_matrix.sh                # build all envs
#   tools/build_matrix.sh --quick        # skip envs matching QUICK_SKIP
#   tools/build_matrix.sh --env <name>   # build a single env (for CI)
#   tools/build_matrix.sh --help
#
# Notes:
#   - Runs `pio` from PATH. No path tricks, no PIO config patching.
#   - Captures FLASH/RAM from PlatformIO's stdout (it prints
#     "RAM:   [...]   2432 bytes from 8192 bytes" style lines).
#   - For envs that don't surface size lines (e.g. some ESP targets when
#     a build fails early), the fields are shown as "-".
#

set -u

# -------- config --------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PIO_INI="$REPO_ROOT/platformio.ini"

# Substrings that --quick skips (heavier / less-frequent builds).
QUICK_SKIP_REGEX='(full|snapshot)'

# -------- arg parsing --------
MODE="all"
ONLY_ENV=""
QUICK=0

print_help() {
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --help|-h)
            print_help
            exit 0
            ;;
        --quick)
            QUICK=1
            shift
            ;;
        --env)
            if [ $# -lt 2 ]; then
                echo "ERROR: --env requires an argument" >&2
                exit 2
            fi
            MODE="single"
            ONLY_ENV="$2"
            shift 2
            ;;
        *)
            echo "ERROR: unknown argument: $1" >&2
            echo "Try --help" >&2
            exit 2
            ;;
    esac
done

# -------- preflight --------
if [ ! -f "$PIO_INI" ]; then
    echo "ERROR: platformio.ini not found at $PIO_INI" >&2
    exit 2
fi

if ! command -v pio >/dev/null 2>&1; then
    echo "ERROR: 'pio' not found in PATH. Install PlatformIO Core or activate the venv." >&2
    exit 2
fi

# -------- enumerate envs --------
ALL_ENVS="$(grep -E '^\[env:' "$PIO_INI" | sed 's/\[env:\(.*\)\]/\1/')"

if [ -z "$ALL_ENVS" ]; then
    echo "ERROR: no [env:*] sections found in $PIO_INI" >&2
    exit 2
fi

# Apply --env filter
if [ "$MODE" = "single" ]; then
    if ! echo "$ALL_ENVS" | grep -qx "$ONLY_ENV"; then
        echo "ERROR: env '$ONLY_ENV' not present in $PIO_INI" >&2
        echo "Available envs:" >&2
        echo "$ALL_ENVS" | sed 's/^/  /' >&2
        exit 2
    fi
    SELECTED_ENVS="$ONLY_ENV"
elif [ "$QUICK" -eq 1 ]; then
    SELECTED_ENVS="$(echo "$ALL_ENVS" | grep -Ev "$QUICK_SKIP_REGEX" || true)"
    if [ -z "$SELECTED_ENVS" ]; then
        echo "ERROR: --quick filtered out every env (regex: $QUICK_SKIP_REGEX)" >&2
        exit 2
    fi
else
    SELECTED_ENVS="$ALL_ENVS"
fi

# -------- helpers --------

# extract_size FIELD LOGFILE
# FIELD is "RAM" or "Flash". Parses the PlatformIO size summary line:
#   RAM:   [          ]   2.4% (used 2432 bytes from 8192 bytes)
# Returns "<used>/<total_pretty>" (e.g. "2432 / 8k") or "-" if not found.
extract_size() {
    local field="$1"
    local logfile="$2"
    local line used total

    line="$(grep -E "^${field}:" "$logfile" | tail -n 1 || true)"
    if [ -z "$line" ]; then
        echo "-"
        return
    fi

    # "used N bytes from M bytes"
    used="$(echo "$line" | sed -n 's/.*used \([0-9]\+\) bytes from \([0-9]\+\) bytes.*/\1/p')"
    total="$(echo "$line" | sed -n 's/.*used \([0-9]\+\) bytes from \([0-9]\+\) bytes.*/\2/p')"

    if [ -z "$used" ] || [ -z "$total" ]; then
        echo "-"
        return
    fi

    # Pretty-print total: bytes -> "Nk" if >=1024
    local total_pretty
    if [ "$total" -ge 1024 ]; then
        total_pretty="$((total / 1024))k"
    else
        total_pretty="${total}"
    fi
    echo "${used} / ${total_pretty}"
}

# build_one ENV LOGFILE
# Runs `pio run -e ENV` with stdout+stderr captured to LOGFILE.
# Returns the pio exit code.
build_one() {
    local env_name="$1"
    local logfile="$2"
    ( cd "$REPO_ROOT" && pio run -e "$env_name" ) >"$logfile" 2>&1
    return $?
}

# -------- run matrix --------
TMPDIR="$(mktemp -d -t build_matrix.XXXXXX)"
trap 'rm -rf "$TMPDIR"' EXIT

# Pre-compute column widths
MAX_ENV_LEN=3
for e in $SELECTED_ENVS; do
    if [ ${#e} -gt "$MAX_ENV_LEN" ]; then
        MAX_ENV_LEN=${#e}
    fi
done
ENV_COL=$((MAX_ENV_LEN + 2))

# Print header
printf "%-${ENV_COL}s %-8s %-15s %-15s\n" "ENV" "STATUS" "FLASH" "RAM"
printf "%-${ENV_COL}s %-8s %-15s %-15s\n" \
    "$(printf '%*s' "$MAX_ENV_LEN" '' | tr ' ' '-')" \
    "------" "-----" "---"

ANY_FAIL=0
for env_name in $SELECTED_ENVS; do
    LOG="$TMPDIR/${env_name}.log"
    if build_one "$env_name" "$LOG"; then
        status="OK"
        flash="$(extract_size Flash "$LOG")"
        ram="$(extract_size RAM "$LOG")"
    else
        status="FAIL"
        flash="-"
        ram="-"
        ANY_FAIL=1
    fi
    printf "%-${ENV_COL}s %-8s %-15s %-15s\n" "$env_name" "$status" "$flash" "$ram"
done

if [ "$ANY_FAIL" -ne 0 ]; then
    echo ""
    echo "One or more envs FAILED. Re-run with --env <name> to inspect a specific failure."
    exit 1
fi

exit 0
