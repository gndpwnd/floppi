#!/usr/bin/env bash
# =============================================================================
# Swarm API - Diagnostics Module
# =============================================================================
# System diagnostics, problem detection, and auto-repair.
# =============================================================================

# ---------------------------------------------------------------------------
# Diagnostics
# ---------------------------------------------------------------------------

cmd_diagnose() {
    header "Swarm API Diagnostics"

    local issues=0
    local warnings=0

    # Check 1: Python
    echo ""
    printf "Checking Python... "
    if command_exists python3; then
        local py_version=$(python3 --version 2>&1)
        local py_minor=$(python3 -c "import sys; print(sys.version_info.minor)" 2>/dev/null)
        if [[ "$py_minor" -ge 10 ]]; then
            success "$py_version"
        else
            warn "$py_version (3.10+ recommended)"
            warnings=$((warnings + 1))
        fi
    else
        error "Python3 not found"
        hint "Install Python 3.10+: https://python.org"
        issues=$((issues + 1))
    fi

    # Check 2: Virtual environment
    printf "Checking virtual environment... "
    if is_venv_active; then
        success "Found at $VENV_DIR"
    else
        warn "No venv (will be created on start)"
        warnings=$((warnings + 1))
    fi

    # Check 3: Dependencies
    printf "Checking dependencies... "
    local python=$(get_python)
    if [[ -n "$python" ]]; then
        local missing=$("$python" -c "
import importlib, sys
deps = ['fastapi', 'uvicorn', 'httpx', 'websockets', 'pydantic', 'zeroconf']
missing = []
for d in deps:
    try:
        importlib.import_module(d)
    except ImportError:
        missing.append(d)
if missing:
    print(','.join(missing))
" 2>/dev/null)
        if [[ -z "$missing" ]]; then
            success "All installed"
        else
            warn "Missing: $missing"
            hint "Run: ./deploy.sh repair"
            warnings=$((warnings + 1))
        fi
    else
        printf "${SKIP} Can't check (no Python)\n"
    fi

    # Check 4: Config file
    printf "Checking config.json... "
    if [[ -f "$SCRIPT_DIR/config.json" ]]; then
        local valid=$(python3 -c "
import json, sys
try:
    with open('$SCRIPT_DIR/config.json') as f:
        cfg = json.load(f)
    drones = cfg.get('drones', [])
    print(f'valid:{len(drones)}')
except Exception as e:
    print(f'error:{e}')
" 2>/dev/null)
        if [[ "$valid" == valid:* ]]; then
            local count="${valid#valid:}"
            success "Valid ($count drone(s) registered)"
        else
            error "Invalid JSON: ${valid#error:}"
            issues=$((issues + 1))
        fi
    else
        warn "Not found (will use defaults)"
        warnings=$((warnings + 1))
    fi

    # Check 5: Source files
    printf "Checking source files... "
    local missing_src=()
    [[ -f "$SCRIPT_DIR/src/main.py" ]] || missing_src+=("src/main.py")
    [[ -f "$SCRIPT_DIR/src/config.py" ]] || missing_src+=("src/config.py")
    [[ -f "$SCRIPT_DIR/src/drone.py" ]] || missing_src+=("src/drone.py")
    [[ -f "$SCRIPT_DIR/src/manager.py" ]] || missing_src+=("src/manager.py")
    [[ -f "$SCRIPT_DIR/src/static/index.html" ]] || missing_src+=("src/static/index.html")

    if [[ ${#missing_src[@]} -eq 0 ]]; then
        success "All present"
    else
        error "Missing: ${missing_src[*]}"
        issues=$((issues + 1))
    fi

    # Check 6: Port
    local port=$(get_port)
    printf "Checking port %s... " "$port"
    if is_server_running; then
        success "In use by Swarm API (OK)"
    elif lsof -i ":$port" -sTCP:LISTEN &>/dev/null 2>&1 || ss -tlnp "sport = :$port" 2>/dev/null | grep -q LISTEN; then
        warn "In use by another application"
        hint "Change port in config.json or stop the other process"
        warnings=$((warnings + 1))
    else
        success "Available"
    fi

    # Check 7: Disk space
    printf "Checking disk space... "
    local available_gb=$(df -BG "$SCRIPT_DIR" | tail -1 | awk '{print $4}' | tr -d 'G')
    if [[ $available_gb -gt 2 ]]; then
        success "${available_gb}GB available"
    elif [[ $available_gb -gt 1 ]]; then
        warn "${available_gb}GB available (low)"
        warnings=$((warnings + 1))
    else
        error "${available_gb}GB available (critically low)"
        issues=$((issues + 1))
    fi

    # Check 8: mDNS support
    printf "Checking mDNS support... "
    if command_exists avahi-browse; then
        success "avahi available"
    elif [[ "$(uname -s)" == "Darwin" ]]; then
        success "Bonjour available (macOS)"
    else
        warn "avahi not detected (mDNS discovery may fail)"
        hint "Install: sudo apt install avahi-daemon avahi-utils"
        warnings=$((warnings + 1))
    fi

    # Check 9: Server health (if running)
    if is_server_running; then
        echo ""
        printf "Checking API health... "
        if check_api_health &>/dev/null; then
            success "Responding"
        else
            warn "Server running but not responding"
            hint "Check logs: ./deploy.sh logs"
            warnings=$((warnings + 1))
        fi

        printf "Checking dashboard... "
        if check_dashboard; then
            success "Accessible"
        else
            warn "Not accessible"
            warnings=$((warnings + 1))
        fi

        echo ""
        info "Drone connectivity:"
        check_all_drones
    fi

    # Check 10: Network
    show_network_info

    # Summary
    echo ""
    echo "============================================"
    if [[ $issues -eq 0 && $warnings -eq 0 ]]; then
        printf "${GREEN}${BOLD}All checks passed!${NC}\n"
    elif [[ $issues -eq 0 ]]; then
        printf "${YELLOW}${BOLD}%d warning(s), no critical issues${NC}\n" "$warnings"
    else
        printf "${RED}${BOLD}%d issue(s), %d warning(s)${NC}\n" "$issues" "$warnings"
        echo ""
        echo "Try: ./deploy.sh repair"
    fi
    echo ""

    return $issues
}

# ---------------------------------------------------------------------------
# Auto-Repair
# ---------------------------------------------------------------------------

cmd_repair() {
    header "Swarm API Auto-Repair"

    local fixed=0
    local failed=0

    # Fix 1: Create venv if missing
    printf "Checking virtual environment... "
    if ! is_venv_active; then
        if command_exists python3; then
            python3 -m venv "$VENV_DIR" 2>/dev/null
            if [[ $? -eq 0 ]]; then
                success "Created venv"
                fixed=$((fixed + 1))
            else
                error "Failed to create venv"
                failed=$((failed + 1))
            fi
        else
            error "Python3 not found"
            failed=$((failed + 1))
        fi
    else
        printf "${SKIP} Already exists\n"
    fi

    # Fix 2: Install dependencies
    printf "Checking dependencies... "
    local pip=$(get_pip)
    if [[ -n "$pip" ]]; then
        "$pip" install -q -r "$SCRIPT_DIR/requirements.txt" 2>/dev/null
        if [[ $? -eq 0 ]]; then
            success "Dependencies installed"
            fixed=$((fixed + 1))
        else
            error "Failed to install dependencies"
            failed=$((failed + 1))
        fi
    else
        error "pip not found"
        failed=$((failed + 1))
    fi

    # Fix 3: Create config.json if missing
    printf "Checking config.json... "
    if [[ ! -f "$SCRIPT_DIR/config.json" ]]; then
        cat > "$SCRIPT_DIR/config.json" << 'EOF'
{
  "drones": [],
  "network": {
    "interface": "auto",
    "command_rate_hz": 10,
    "telemetry_poll_interval_ms": 500,
    "connection_timeout_ms": 2000
  },
  "server": {
    "host": "0.0.0.0",
    "port": 8080
  }
}
EOF
        success "Created default config.json"
        fixed=$((fixed + 1))
    else
        printf "${SKIP} Already exists\n"
    fi

    # Fix 4: Create missing directories
    printf "Checking directories... "
    local created=0
    for dir in src/static src/api tests; do
        if [[ ! -d "$SCRIPT_DIR/$dir" ]]; then
            mkdir -p "$SCRIPT_DIR/$dir"
            created=$((created + 1))
        fi
    done
    if [[ $created -gt 0 ]]; then
        success "Created $created directories"
        fixed=$((fixed + 1))
    else
        printf "${SKIP} All exist\n"
    fi

    # Fix 5: Clean stale PID file
    printf "Checking for stale PID file... "
    local pid=$(get_pid)
    if [[ -n "$pid" ]] && ! is_process_running "$pid"; then
        rm -f "$PID_FILE"
        success "Removed stale PID file"
        fixed=$((fixed + 1))
    else
        printf "${SKIP} None found\n"
    fi

    # Summary
    echo ""
    echo "============================================"
    if [[ $failed -eq 0 ]]; then
        if [[ $fixed -gt 0 ]]; then
            printf "${GREEN}${BOLD}Repair complete! Fixed %d item(s).${NC}\n" "$fixed"
        else
            printf "${GREEN}${BOLD}No repairs needed.${NC}\n"
        fi
        echo ""
        echo "Run './deploy.sh start' to start the server"
    else
        printf "${RED}${BOLD}Some repairs failed (%d). Run ./deploy.sh diagnose for details.${NC}\n" "$failed"
    fi
    echo ""
}

# ---------------------------------------------------------------------------
# Config Commands
# ---------------------------------------------------------------------------

cmd_config() {
    header "Swarm API Configuration"

    local config="$SCRIPT_DIR/config.json"
    if [[ ! -f "$config" ]]; then
        warn "No config.json found"
        hint "Run ./deploy.sh repair to create one"
        return 1
    fi

    python3 -c "
import json
with open('$config') as f:
    cfg = json.load(f)

server = cfg.get('server', {})
net = cfg.get('network', {})
drones = cfg.get('drones', [])

print(f'  Server:')
print(f'    Host: {server.get(\"host\", \"0.0.0.0\")}')
print(f'    Port: {server.get(\"port\", 8080)}')
print()
print(f'  Network:')
print(f'    Interface:    {net.get(\"interface\", \"auto\")}')
print(f'    Command rate: {net.get(\"command_rate_hz\", 10)} Hz')
print(f'    Poll interval: {net.get(\"telemetry_poll_interval_ms\", 500)} ms')
print(f'    Timeout:      {net.get(\"connection_timeout_ms\", 2000)} ms')
print()
print(f'  Drones ({len(drones)} registered):')
for d in drones:
    status = ''
    ip = d.get('last_ip', 'no IP')
    mdns = d.get('mdns_hostname', '')
    name = d.get('name', 'unnamed')
    mac = d.get('mac', '?')
    print(f'    {name} | {mac} | {ip} | {mdns}.local' if mdns else f'    {name} | {mac} | {ip}')
" 2>/dev/null
    echo ""
}

# Check drone connectivity (standalone command)
cmd_drones() {
    header "Drone Connectivity"

    if ! is_server_running; then
        info "Server not running — checking drones directly from config"
    fi

    check_all_drones
    echo ""
}
