#!/usr/bin/env bash
# =============================================================================
# Swarm API - Unified Management Script
# =============================================================================
# A single command for everything: start, stop, diagnose, repair, monitor.
#
# Usage:
#   ./deploy.sh              - Interactive menu (default)
#   ./deploy.sh start        - Start server (background)
#   ./deploy.sh start --fg   - Start server (foreground)
#   ./deploy.sh stop         - Stop server
#   ./deploy.sh restart      - Restart server
#   ./deploy.sh status       - Check status and drone connectivity
#   ./deploy.sh diagnose     - Find and explain problems
#   ./deploy.sh repair       - Auto-fix common issues
#   ./deploy.sh logs [n]     - View last n log lines (default 100)
#   ./deploy.sh logs-f       - Follow logs in real-time
#   ./deploy.sh config       - Show configuration
#   ./deploy.sh drones       - Check drone connectivity
#   ./deploy.sh setup        - Set up venv and install dependencies
#   ./deploy.sh help         - Show help
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PROJECT_NAME="swarm_api"
DEFAULT_PORT=8080

# Global flags
export VERBOSE=0
export TEST_MODE=0
export DEBUG=0

# ---------------------------------------------------------------------------
# Parse Global Options
# ---------------------------------------------------------------------------
parse_global_options() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --verbose|-v) export VERBOSE=1; shift ;;
            --test)       export TEST_MODE=1; shift ;;
            --debug)      export DEBUG=1; export VERBOSE=1; shift ;;
            *)            return ;;
        esac
    done
}

ARGS=("$@")
parse_global_options "${ARGS[@]}"

# ---------------------------------------------------------------------------
# Load Modules
# ---------------------------------------------------------------------------
DEPLOY_DIR="$SCRIPT_DIR/deploy"

if [[ ! -d "$DEPLOY_DIR" ]]; then
    echo "ERROR: Deploy modules directory not found: $DEPLOY_DIR"
    exit 1
fi

source "$DEPLOY_DIR/common.sh"
source "$DEPLOY_DIR/service.sh"
source "$DEPLOY_DIR/health.sh"
source "$DEPLOY_DIR/diagnostics.sh"
source "$DEPLOY_DIR/menu.sh"

# ---------------------------------------------------------------------------
# Prerequisites
# ---------------------------------------------------------------------------
verify_prerequisites() {
    local missing=()
    command_exists curl    || missing+=("curl")
    command_exists python3 || missing+=("python3")

    if [[ ${#missing[@]} -gt 0 ]]; then
        error "Missing required commands: ${missing[*]}"
        for cmd in "${missing[@]}"; do
            echo "  - $cmd"
        done
        return 1
    fi
    debug "All prerequisites satisfied"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
main() {
    local cmd_args=()
    for arg in "$@"; do
        case "$arg" in
            --verbose|-v|--test|--debug) ;;
            *) cmd_args+=("$arg") ;;
        esac
    done

    local cmd="${cmd_args[0]:-menu}"
    local cmd_params=("${cmd_args[@]:1}")

    if [[ "$cmd" != "help" ]] && [[ "$cmd" != "-h" ]] && [[ "$cmd" != "--help" ]]; then
        verify_prerequisites || exit 1
    fi

    case "$cmd" in
        start)     cmd_start "${cmd_params[@]}" ;;
        stop)      cmd_stop ;;
        restart)   cmd_restart "${cmd_params[@]}" ;;
        status)    cmd_status ;;
        diagnose)  cmd_diagnose ;;
        repair)    cmd_repair ;;
        logs)      cmd_logs "${cmd_params[@]}" ;;
        logs-f)    cmd_logs_follow ;;
        config)    cmd_config ;;
        drones)    cmd_drones ;;
        setup)     ensure_environment ;;
        menu)      show_menu ;;
        help|-h|--help) cmd_help ;;
        version)   echo "Swarm API v0.1.0" ;;
        *)
            error "Unknown command: $cmd"
            echo ""
            cmd_help
            exit 1
            ;;
    esac
}

main "$@"
