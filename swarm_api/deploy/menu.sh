#!/usr/bin/env bash
# =============================================================================
# Swarm API - Menu & Help Module
# =============================================================================
# Interactive menu and help system.
# =============================================================================

# ---------------------------------------------------------------------------
# Interactive Menu
# ---------------------------------------------------------------------------

show_menu() {
    clear
    show_banner

    local port=$(get_port)

    # Show current status
    if is_server_running; then
        local pid=$(get_pid)
        printf "  Status: ${GREEN}${BOLD}RUNNING${NC} (PID %s) at http://localhost:%s\n" "$pid" "$port"
    else
        printf "  Status: ${RED}${BOLD}STOPPED${NC}\n"
    fi

    echo ""
    echo "  What would you like to do?"
    echo ""
    echo "  ${BOLD}Server${NC}"
    echo "   1) Start server"
    echo "   2) Stop server"
    echo "   3) Restart server"
    echo "   4) Check status"
    echo ""
    echo "  ${BOLD}Monitoring${NC}"
    echo "   5) View logs"
    echo "   6) Follow logs (live)"
    echo "   7) Check drone connectivity"
    echo ""
    echo "  ${BOLD}Troubleshooting${NC}"
    echo "   8) Diagnose problems"
    echo "   9) Auto-repair"
    echo ""
    echo "  ${BOLD}Configuration${NC}"
    echo "  10) Show config"
    echo "  11) Start in foreground"
    echo ""
    echo "   h) Help"
    echo "   0) Exit"
    echo ""

    read -p "  Enter choice: " choice

    case "$choice" in
        1)  cmd_start ;;
        2)  cmd_stop ;;
        3)  cmd_restart ;;
        4)  cmd_status ;;
        5)  cmd_logs ;;
        6)  cmd_logs_follow ;;
        7)  cmd_drones ;;
        8)  cmd_diagnose ;;
        9)  cmd_repair ;;
        10) cmd_config ;;
        11) cmd_start --fg ;;
        h|H) cmd_help ;;
        0)  exit 0 ;;
        *)  warn "Invalid choice" ;;
    esac

    echo ""
    wait_for_keypress
    show_menu
}

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------

cmd_help() {
    cat << EOF

${BOLD}${CYAN}Swarm API - Floppi Drone Swarm Controller${NC}
============================================

Ground station for controlling ESP32 drones over WiFi.

${BOLD}Usage:${NC}
  ./deploy.sh              Interactive menu (default)
  ./deploy.sh <command>    Run a specific command

${BOLD}Commands:${NC}
  ${GREEN}start${NC}       Start the server (background)
              Options: --fg (foreground mode)
  ${GREEN}stop${NC}        Stop the server
  ${GREEN}restart${NC}     Restart the server
  ${GREEN}status${NC}      Show server and drone status
  ${GREEN}logs${NC}        View recent logs
              Usage: logs [lines]  (default: 100)
  ${GREEN}logs-f${NC}      Follow logs in real-time
  ${GREEN}diagnose${NC}    Check system for problems
  ${GREEN}repair${NC}      Auto-fix common issues (venv, deps, config)
  ${GREEN}config${NC}      Show current configuration
  ${GREEN}drones${NC}      Check connectivity to all registered drones
  ${GREEN}setup${NC}       Set up venv and install dependencies
  ${GREEN}help${NC}        Show this help

${BOLD}Global Options:${NC}
  --verbose, -v    Show detailed output
  --test           Run in test mode (non-interactive)

${BOLD}Examples:${NC}
  ./deploy.sh                    # Interactive menu
  ./deploy.sh start              # Start in background
  ./deploy.sh start --fg         # Start in foreground (see output)
  ./deploy.sh status             # Check what's running
  ./deploy.sh diagnose           # Find and explain problems
  ./deploy.sh drones             # Check drone connectivity
  ./deploy.sh logs 200           # View last 200 log lines
  ./deploy.sh logs-f             # Follow logs live

${BOLD}First-time Setup:${NC}
  1. Edit config.json with your drone MAC addresses
  2. Run: ./deploy.sh start
  3. Open: http://localhost:$(get_port)

${BOLD}Config:${NC}
  config.json  — Drone registry, network settings, server port
  See: docs/scope.md for ESP32 API contract details

EOF
}
