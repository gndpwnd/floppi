#!/usr/bin/env bash
# =============================================================================
# Swarm API - Common Utilities Module
# =============================================================================
# Color definitions, logging functions, and shared utilities.
# =============================================================================

# ---------------------------------------------------------------------------
# Color Definitions
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

# Status indicators
OK="${GREEN}[OK]${NC}"
FAIL="${RED}[FAIL]${NC}"
WARN="${YELLOW}[WARN]${NC}"
INFO="${BLUE}[INFO]${NC}"
SKIP="${DIM}[SKIP]${NC}"

# ---------------------------------------------------------------------------
# Logging Functions
# ---------------------------------------------------------------------------
info()    { printf "${INFO}  %s\n" "$*"; }
success() { printf "${OK}    %s\n" "$*"; }
warn()    { printf "${WARN}  %s\n" "$*"; }
error()   { printf "${FAIL}  %s\n" "$*" >&2; }
hint()    { printf "        ${DIM}Hint: %s${NC}\n" "$*"; }
header()  { echo ""; printf "${BOLD}${CYAN}%s${NC}\n" "$*"; printf "%s\n" "$(echo "$*" | sed 's/./-/g')"; }
debug()   { if [[ "${VERBOSE:-0}" == "1" ]]; then printf "${DIM}[DEBUG] %s${NC}\n" "$*" >&2; fi; }

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
get_script_dir()    { echo "${SCRIPT_DIR:-$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)}"; }
get_project_name()  { echo "${PROJECT_NAME:-swarm_api}"; }
get_default_port()  { echo "${DEFAULT_PORT:-8080}"; }

get_port() {
    local config="$SCRIPT_DIR/config.json"
    if [[ -f "$config" ]]; then
        python3 -c "import json; print(json.load(open('$config')).get('server',{}).get('port', $(get_default_port)))" 2>/dev/null || echo "$(get_default_port)"
    else
        echo "$(get_default_port)"
    fi
}

get_host() {
    local config="$SCRIPT_DIR/config.json"
    if [[ -f "$config" ]]; then
        python3 -c "import json; print(json.load(open('$config')).get('server',{}).get('host', '0.0.0.0'))" 2>/dev/null || echo "0.0.0.0"
    else
        echo "0.0.0.0"
    fi
}

# ---------------------------------------------------------------------------
# Error Handling
# ---------------------------------------------------------------------------
die() { error "$@"; exit 1; }
is_test_mode() { [[ "${TEST_MODE:-0}" == "1" ]]; }
is_verbose()   { [[ "${VERBOSE:-0}" == "1" ]]; }

# ---------------------------------------------------------------------------
# Path Utilities
# ---------------------------------------------------------------------------
ensure_dir() { [[ -d "$1" ]] || mkdir -p "$1" || die "Failed to create directory: $1"; }
file_exists() { [[ -f "$1" ]]; }
dir_exists()  { [[ -d "$1" ]]; }
command_exists() { command -v "$1" &>/dev/null; }

# ---------------------------------------------------------------------------
# PID File Management
# ---------------------------------------------------------------------------
PID_FILE="${SCRIPT_DIR}/.swarm_api.pid"
LOG_FILE="${SCRIPT_DIR}/swarm_api.log"

get_pid() {
    if [[ -f "$PID_FILE" ]]; then
        cat "$PID_FILE"
    fi
}

is_process_running() {
    local pid="$1"
    [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null
}

is_server_running() {
    local pid=$(get_pid)
    is_process_running "$pid"
}

# ---------------------------------------------------------------------------
# User Interaction
# ---------------------------------------------------------------------------
confirm() {
    local prompt="${1:-Are you sure?}"
    if is_test_mode; then return 0; fi
    read -p "$prompt (y/N) " -n 1 -r
    echo ""
    [[ $REPLY =~ ^[Yy]$ ]]
}

wait_for_keypress() {
    if ! is_test_mode; then
        read -p "${1:-Press Enter to continue...}"
    fi
}

# ---------------------------------------------------------------------------
# Python / Venv Utilities
# ---------------------------------------------------------------------------
VENV_DIR="${SCRIPT_DIR}/venv"

get_python() {
    if [[ -f "$VENV_DIR/bin/python" ]]; then
        echo "$VENV_DIR/bin/python"
    elif command_exists python3; then
        echo "python3"
    elif command_exists python; then
        echo "python"
    else
        echo ""
    fi
}

get_pip() {
    if [[ -f "$VENV_DIR/bin/pip" ]]; then
        echo "$VENV_DIR/bin/pip"
    elif command_exists pip3; then
        echo "pip3"
    elif command_exists pip; then
        echo "pip"
    else
        echo ""
    fi
}

is_venv_active() {
    [[ -f "$VENV_DIR/bin/python" ]]
}

# ---------------------------------------------------------------------------
# Banner
# ---------------------------------------------------------------------------
show_banner() {
    printf "${BOLD}${CYAN}"
    cat << 'BANNER'
  _____
 / ____|
| (_____      ____ _ _ __ _ __ ___
 \___ \ \ /\ / / _` | '__| '_ ` _ \
 ____) \ V  V / (_| | |  | | | | | |
|_____/ \_/\_/ \__,_|_|  |_| |_| |_|

BANNER
    printf "${NC}"
    printf "${DIM}Floppi Drone Swarm Controller${NC}\n"
    echo ""
}
