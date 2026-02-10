#!/usr/bin/env bash
# =============================================================================
# Swarm API - Health Check Module
# =============================================================================
# API health checks and drone connectivity monitoring.
# =============================================================================

# ---------------------------------------------------------------------------
# API Health Checks
# ---------------------------------------------------------------------------

# Check /health endpoint
check_api_health() {
    local port=$(get_port)
    curl -sf --max-time 3 "http://localhost:${port}/health" 2>/dev/null
}

# Check dashboard is accessible
check_dashboard() {
    local port=$(get_port)
    curl -sf --max-time 3 "http://localhost:${port}/" -o /dev/null 2>/dev/null
}

# Wait for API to be ready
wait_for_api() {
    local max_wait="${1:-30}"
    local waited=0

    while [[ $waited -lt $max_wait ]]; do
        if check_api_health &>/dev/null; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
        printf "."
    done
    echo ""
    return 1
}

# ---------------------------------------------------------------------------
# Drone Connectivity
# ---------------------------------------------------------------------------

# Check connectivity to a single drone
# Args: $1 = IP or hostname
check_drone_connectivity() {
    local addr="$1"
    local url="http://${addr}/api/status"

    debug "Checking drone at $url"
    curl -sf --max-time 2 "$url" 2>/dev/null
}

# Check all registered drones from config.json
check_all_drones() {
    local config="$SCRIPT_DIR/config.json"
    if [[ ! -f "$config" ]]; then
        warn "No config.json found"
        return 1
    fi

    local drones=$(python3 -c "
import json
with open('$config') as f:
    cfg = json.load(f)
for d in cfg.get('drones', []):
    name = d.get('name', 'unknown')
    ip = d.get('last_ip', '')
    mdns = d.get('mdns_hostname', '')
    print(f'{name}|{ip}|{mdns}')
" 2>/dev/null)

    if [[ -z "$drones" ]]; then
        info "No drones registered in config.json"
        return 0
    fi

    echo ""
    while IFS='|' read -r name ip mdns; do
        printf "  %-20s " "$name:"

        local reachable=false

        # Try mDNS first
        if [[ -n "$mdns" ]]; then
            local mdns_addr="${mdns}.local"
            if check_drone_connectivity "$mdns_addr" &>/dev/null; then
                printf "${GREEN}online${NC} via %s\n" "$mdns_addr"
                reachable=true
            fi
        fi

        # Try IP if mDNS failed
        if [[ "$reachable" == "false" ]] && [[ -n "$ip" ]] && [[ "$ip" != "null" ]]; then
            if check_drone_connectivity "$ip" &>/dev/null; then
                printf "${GREEN}online${NC} via %s\n" "$ip"
                reachable=true
            fi
        fi

        if [[ "$reachable" == "false" ]]; then
            printf "${RED}offline${NC}"
            [[ -n "$mdns" ]] && printf " (tried %s.local" "$mdns"
            [[ -n "$ip" ]] && [[ "$ip" != "null" ]] && printf ", %s" "$ip"
            [[ -n "$mdns" ]] || [[ -n "$ip" && "$ip" != "null" ]] && printf ")"
            echo ""
        fi
    done <<< "$drones"
}

# Get drone telemetry summary
get_drone_telemetry() {
    local addr="$1"
    local data=$(check_drone_connectivity "$addr")

    if [[ -z "$data" ]]; then
        return 1
    fi

    python3 -c "
import json, sys
d = json.loads('''$data''')
armed = d.get('armed', False)
roll = d.get('roll', '?')
pitch = d.get('pitch', '?')
yaw = d.get('yaw', '?')
loop = d.get('loop_us', '?')
net = d.get('net', {})
rssi = net.get('rssi', '?')
hostname = net.get('hostname', '?')
print(f'  Hostname: {hostname}')
print(f'  Armed:    {\"YES\" if armed else \"NO\"}')
print(f'  Attitude: R={roll} P={pitch} Y={yaw}')
print(f'  Loop:     {loop} us')
print(f'  RSSI:     {rssi} dBm')
" 2>/dev/null
}

# ---------------------------------------------------------------------------
# Network Diagnostics
# ---------------------------------------------------------------------------

# Check if mDNS resolution is working
check_mdns_available() {
    # Try resolving any .local address
    if command_exists avahi-resolve 2>/dev/null; then
        debug "avahi-resolve available"
        return 0
    elif getent hosts "localhost.local" &>/dev/null 2>&1; then
        return 0
    else
        # Try a basic .local lookup
        python3 -c "
import socket
try:
    socket.getaddrinfo('localhost.local', 80, socket.AF_INET, socket.SOCK_STREAM)
except socket.gaierror:
    pass  # Expected to fail, just checking the system supports the query
" 2>/dev/null
        return 0  # If python3 runs without crash, mDNS stack is present
    fi
}

# Get local network interfaces and IPs
show_network_info() {
    echo ""
    info "Network interfaces:"
    if command_exists ip; then
        ip -4 addr show | grep -E "inet " | awk '{print "  " $NF ": " $2}' | grep -v "127.0.0.1"
    elif command_exists ifconfig; then
        ifconfig | grep -E "inet " | awk '{print "  " $2}' | grep -v "127.0.0.1"
    else
        python3 -c "
import socket
hostname = socket.gethostname()
try:
    ip = socket.gethostbyname(hostname)
    print(f'  {hostname}: {ip}')
except:
    print('  Could not determine IP')
" 2>/dev/null
    fi
}
