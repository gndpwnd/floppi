#!/usr/bin/env bash
# =============================================================================
# Swarm API - Service Management Module
# =============================================================================
# Start, stop, restart, and manage the uvicorn server process.
# =============================================================================

# ---------------------------------------------------------------------------
# Virtual Environment
# ---------------------------------------------------------------------------

# Create virtual environment if it doesn't exist
setup_venv() {
    if [[ -d "$VENV_DIR" ]]; then
        debug "Venv already exists at $VENV_DIR"
        return 0
    fi

    local python=$(get_python)
    if [[ -z "$python" ]]; then
        die "Python not found. Install Python 3.10+ first."
    fi

    info "Creating virtual environment..."
    "$python" -m venv "$VENV_DIR" || die "Failed to create venv"
    success "Virtual environment created"
}

# Install dependencies
install_deps() {
    local pip=$(get_pip)
    if [[ -z "$pip" ]]; then
        die "pip not found"
    fi

    local req="$SCRIPT_DIR/requirements.txt"
    if [[ ! -f "$req" ]]; then
        die "requirements.txt not found"
    fi

    info "Installing dependencies..."
    "$pip" install -q -r "$req" 2>&1 | tail -1
    success "Dependencies installed"
}

# Ensure venv and deps are ready
ensure_environment() {
    setup_venv
    install_deps
}

# ---------------------------------------------------------------------------
# Server Start / Stop
# ---------------------------------------------------------------------------

# Start the uvicorn server
cmd_start() {
    header "Starting Swarm API"

    local port=$(get_port)
    local host=$(get_host)

    # Check if already running
    if is_server_running; then
        local pid=$(get_pid)
        success "Swarm API is already running (PID $pid)"
        printf "  Dashboard: ${BOLD}http://localhost:%s${NC}\n" "$port"
        echo ""
        return 0
    fi

    # Ensure environment is ready
    ensure_environment

    # Check port availability
    if lsof -i ":$port" -sTCP:LISTEN &>/dev/null 2>&1 || ss -tlnp "sport = :$port" 2>/dev/null | grep -q LISTEN; then
        error "Port $port is already in use"
        hint "Change port in config.json or stop the other process"
        return 1
    fi

    # Check config
    if [[ ! -f "$SCRIPT_DIR/config.json" ]]; then
        warn "No config.json found, using defaults"
    fi

    local python=$(get_python)

    # Foreground or background
    local foreground=0
    for arg in "$@"; do
        case "$arg" in
            --fg|--foreground) foreground=1 ;;
        esac
    done

    if [[ "$foreground" -eq 1 ]]; then
        info "Starting in foreground (Ctrl+C to stop)..."
        echo ""
        cd "$SCRIPT_DIR" && exec "$python" -m uvicorn src.main:app --host "$host" --port "$port"
    else
        info "Starting server on $host:$port..."
        cd "$SCRIPT_DIR" && "$python" -m uvicorn src.main:app \
            --host "$host" --port "$port" \
            >> "$LOG_FILE" 2>&1 &
        local pid=$!
        echo "$pid" > "$PID_FILE"

        # Wait briefly and verify it started
        sleep 2
        if is_process_running "$pid"; then
            success "Swarm API started (PID $pid)"
            echo ""
            printf "  Dashboard: ${BOLD}http://localhost:%s${NC}\n" "$port"
            printf "  API docs:  ${BOLD}http://localhost:%s/docs${NC}\n" "$port"
            printf "  Logs:      ${DIM}%s${NC}\n" "$LOG_FILE"
            echo ""
        else
            error "Server failed to start"
            hint "Check logs: ./deploy.sh logs"
            rm -f "$PID_FILE"
            return 1
        fi
    fi
}

# Stop the server
cmd_stop() {
    header "Stopping Swarm API"

    local pid=$(get_pid)
    if [[ -z "$pid" ]] || ! is_process_running "$pid"; then
        info "Swarm API is not running"
        rm -f "$PID_FILE"
        return 0
    fi

    info "Stopping server (PID $pid)..."
    kill "$pid" 2>/dev/null

    # Wait for graceful shutdown
    local waited=0
    while is_process_running "$pid" && [[ $waited -lt 10 ]]; do
        sleep 1
        ((waited++))
    done

    if is_process_running "$pid"; then
        warn "Graceful shutdown timed out, forcing..."
        kill -9 "$pid" 2>/dev/null
    fi

    rm -f "$PID_FILE"
    success "Swarm API stopped"
    echo ""
}

# Restart the server
cmd_restart() {
    header "Restarting Swarm API"
    cmd_stop
    sleep 1
    cmd_start "$@"
}

# Show server status
cmd_status() {
    header "Swarm API Status"

    local port=$(get_port)
    local pid=$(get_pid)

    if is_server_running; then
        printf "  Server:  ${GREEN}${BOLD}RUNNING${NC} (PID %s)\n" "$pid"
        printf "  URL:     http://localhost:%s\n" "$port"

        # Check API health
        if curl -sf --max-time 3 "http://localhost:${port}/health" &>/dev/null; then
            printf "  Health:  ${GREEN}${BOLD}HEALTHY${NC}\n"

            # Get drone count
            local drones=$(curl -sf --max-time 3 "http://localhost:${port}/api/drones" 2>/dev/null)
            if [[ -n "$drones" ]]; then
                local count=$(echo "$drones" | python3 -c "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo "?")
                local online=$(echo "$drones" | python3 -c "import sys,json; print(sum(1 for d in json.load(sys.stdin) if d.get('online')))" 2>/dev/null || echo "?")
                printf "  Drones:  %s registered, %s online\n" "$count" "$online"
            fi
        else
            printf "  Health:  ${YELLOW}${BOLD}NOT RESPONDING${NC}\n"
            hint "Server may still be starting up"
        fi
    else
        printf "  Server:  ${RED}${BOLD}STOPPED${NC}\n"
        echo ""
        echo "  Run './deploy.sh start' to start"
    fi

    # Show venv status
    echo ""
    if is_venv_active; then
        local py_version=$("$VENV_DIR/bin/python" --version 2>&1)
        printf "  Python:  %s (venv)\n" "$py_version"
    elif command_exists python3; then
        printf "  Python:  %s (system)\n" "$(python3 --version 2>&1)"
    else
        printf "  Python:  ${RED}NOT FOUND${NC}\n"
    fi

    echo ""
}

# View logs
cmd_logs() {
    local lines="${1:-100}"

    if [[ ! -f "$LOG_FILE" ]]; then
        info "No log file found"
        hint "Start the server first: ./deploy.sh start"
        return 0
    fi

    header "Swarm API Logs (last $lines lines)"
    tail -n "$lines" "$LOG_FILE"
    echo ""
}

# Follow logs (live)
cmd_logs_follow() {
    if [[ ! -f "$LOG_FILE" ]]; then
        info "No log file found"
        return 0
    fi

    header "Swarm API Logs (live — Ctrl+C to stop)"
    tail -f "$LOG_FILE"
}
