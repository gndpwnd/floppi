#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

BOLD='\033[1m'
UNDERLINE='\033[4m'

# Get project root directory
PROJECT_ROOT=$(pwd)
PLATFORMIO_INI="$PROJECT_ROOT/platformio.ini"

# ======================================================================
# HELPER FUNCTIONS
# ======================================================================

print_header() {
    clear
    echo -e "${BLUE}${BOLD}"
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║          dRehmFlight VTOL Flight Controller              ║"
    echo "║                 Build & Deployment System                ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_footer() {
    echo -e "${CYAN}${BOLD}"
    echo "╔══════════════════════════════════════════════════════════╗"
    echo "║                       Build Complete!                     ║"
    echo "╚══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_error() {
    echo -e "${RED}${BOLD}[-] $1${NC}"
}

print_success() {
    echo -e "${GREEN}${BOLD}[+] $1${NC}"
}

print_info() {
    echo -e "${CYAN}[*] $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}[!] $1${NC}"
}

# ======================================================================
# PLATFORMIO.INI PARSING
# ======================================================================

get_environments() {
    # Extract environment names from platformio.ini
    # Looks for lines like: [env:teensy40], [env:teensy41], etc.
    local envs=()
    
    if [[ ! -f "$PLATFORMIO_INI" ]]; then
        print_error "platformio.ini not found in $PROJECT_ROOT"
        exit 1
    fi
    
    while IFS= read -r line; do
        if [[ "$line" =~ ^\[env:([a-zA-Z0-9_-]+)\]$ ]]; then
            envs+=("${BASH_REMATCH[1]}")
        fi
    done < "$PLATFORMIO_INI"
    
    echo "${envs[@]}"
}

get_default_environment() {
    # Get default environment from [platformio] section
    if [[ -f "$PLATFORMIO_INI" ]]; then
        while IFS= read -r line; do
            if [[ "$line" =~ ^default_envs\s*=\s*([a-zA-Z0-9_-]+) ]]; then
                echo "${BASH_REMATCH[1]}"
                return
            fi
        done < "$PLATFORMIO_INI"
    fi
    echo ""
}

get_environment_info() {
    # Get board name and platform for an environment
    local env="$1"
    local board=""
    local platform=""
    local in_env_section=false
    
    while IFS= read -r line; do
        # Check if we're entering this environment section
        if [[ "$line" =~ ^\[env:$env\]$ ]]; then
            in_env_section=true
            continue
        fi
        
        # Check if we're leaving environment section
        if [[ "$in_env_section" == true && "$line" =~ ^\[ ]]; then
            break
        fi
        
        if [[ "$in_env_section" == true ]]; then
            if [[ "$line" =~ ^board\s*=\s*([a-zA-Z0-9_-]+) ]]; then
                board="${BASH_REMATCH[1]}"
            elif [[ "$line" =~ ^platform\s*=\s*([a-zA-Z0-9_-]+) ]]; then
                platform="${BASH_REMATCH[1]}"
            fi
        fi
    done < "$PLATFORMIO_INI"
    
    echo "$board:$platform"
}

print_environments() {
    local envs=($(get_environments))
    local default_env=$(get_default_environment)
    
    if [[ ${#envs[@]} -eq 0 ]]; then
        print_error "No environments found in platformio.ini"
        exit 1
    fi
    
    echo -e "${CYAN}${BOLD}Available Build Environments:${NC}"
    echo -e "${CYAN}══════════════════════════════════════════════════════════${NC}"
    
    for i in "${!envs[@]}"; do
        local env="${envs[$i]}"
        local info=$(get_environment_info "$env")
        local board=$(echo "$info" | cut -d':' -f1)
        local platform=$(echo "$info" | cut -d':' -f2)
        local prefix="  "
        
        if [[ "$env" == "$default_env" ]]; then
            prefix="${GREEN}★ ${NC}"
            env="${GREEN}${BOLD}$env${NC} (default)"
        else
            prefix="  "
            env="${CYAN}$env${NC}"
        fi
        
        echo -e "$prefix$i) $env"
        echo -e "   ${MAGENTA}Board:${NC} $board ${MAGENTA}Platform:${NC} $platform"
    done
    echo -e "${CYAN}══════════════════════════════════════════════════════════${NC}"
}

select_environment() {
    local envs=($(get_environments))
    
    if [[ ${#envs[@]} -eq 1 ]]; then
        echo "${envs[0]}"
        return
    fi
    
    while true; do
        print_environments
        echo -e "${BLUE}${BOLD}Select environment (0-$(( ${#envs[@]} - 1 ))) or 'q' to quit: ${NC}"
        read -p "> " choice
        
        if [[ "$choice" == "q" || "$choice" == "Q" ]]; then
            exit 0
        fi
        
        if [[ "$choice" =~ ^[0-9]+$ ]] && [[ "$choice" -ge 0 ]] && [[ "$choice" -lt ${#envs[@]} ]]; then
            echo "${envs[$choice]}"
            return
        else
            print_error "Invalid selection. Please try again."
            sleep 1
        fi
    done
}

# ======================================================================
# BUILD FUNCTIONS
# ======================================================================

clean() {
    local env="$1"
    print_header
    print_info "Cleaning build environment: $env"
    echo -e "${YELLOW}══════════════════════════════════════════════════════════${NC}"
    
    if [[ -n "$env" ]]; then
        pio run -t clean -e "$env"
    else
        pio run -t clean
    fi
    
    if [[ $? -eq 0 ]]; then
        print_success "Clean completed successfully"
    else
        print_error "Clean failed"
        return 1
    fi
}

build() {
    local env="$1"
    print_header
    print_info "Building for environment: $env"
    
    if [[ -n "$env" ]]; then
        local info=$(get_environment_info "$env")
        local board=$(echo "$info" | cut -d':' -f1)
        print_info "Board: $board"
    fi
    
    echo -e "${GREEN}══════════════════════════════════════════════════════════${NC}"
    
    if [[ -n "$env" ]]; then
        pio run -e "$env"
    else
        pio run
    fi
    
    local result=$?
    
    if [[ $result -eq 0 ]]; then
        print_success "Build completed successfully"
        return 0
    else
        print_error "Build failed with exit code: $result"
        return $result
    fi
}

upload() {
    local env="$1"
    print_header
    print_info "Uploading to environment: $env"
    
    if [[ -n "$env" ]]; then
        local info=$(get_environment_info "$env")
        local board=$(echo "$info" | cut -d':' -f1)
        print_info "Board: $board"
    fi
    
    echo -e "${BLUE}══════════════════════════════════════════════════════════${NC}"
    print_warning "Ensure your board is connected and in programming mode!"
    print_warning "For Teensy, press the Program button when prompted..."
    echo ""
    
    if [[ -n "$env" ]]; then
        pio run -t upload -e "$env"
    else
        pio run -t upload
    fi
    
    local result=$?
    
    if [[ $result -eq 0 ]]; then
        print_success "Upload completed successfully"
        return 0
    else
        print_error "Upload failed with exit code: $result"
        return $result
    fi
}

monitor() {
    print_header
    print_info "Starting serial monitor"
    print_warning "Press Ctrl+A then Ctrl+X to exit"
    echo -e "${MAGENTA}══════════════════════════════════════════════════════════${NC}"

    pio device monitor
}

build_matrix_entry() {
    # Build a single coverage-matrix entry: an env plus extra -D flags
    # injected via PLATFORMIO_BUILD_FLAGS (PlatformIO appends these to the
    # environment's build_flags).
    local env="$1"
    local extra_flags="$2"

    print_info "Matrix build: $env  (extra flags: ${extra_flags:-none})"
    echo -e "${GREEN}══════════════════════════════════════════════════════════${NC}"

    PLATFORMIO_BUILD_FLAGS="$extra_flags" pio run -e "$env"
    local result=$?

    if [[ $result -eq 0 ]]; then
        print_success "Matrix build OK: $env"
        return 0
    else
        print_error "Matrix build FAILED ($env) exit code: $result"
        return $result
    fi
}

build_matrix() {
    # Coverage matrix — exercises flag combinations that the per-env builds
    # do not cover on their own. Each entry is "env|extra build flags".
    print_header
    print_info "Running build coverage matrix"
    echo -e "${CYAN}══════════════════════════════════════════════════════════${NC}"

    local matrix=(
        "esp32|"
        "esp32s3|"
        "esp32|-D USE_BAROMETER -D USE_GPS"
        "esp32s3|-D USE_BAROMETER -D USE_GPS"
    )

    local failures=0
    for entry in "${matrix[@]}"; do
        local env="${entry%%|*}"
        local flags="${entry#*|}"
        build_matrix_entry "$env" "$flags" || failures=$((failures + 1))
        echo ""
    done

    if [[ $failures -eq 0 ]]; then
        print_success "Build matrix completed — all ${#matrix[@]} entries OK"
        return 0
    else
        print_error "Build matrix completed with $failures failure(s)"
        return 1
    fi
}

# ======================================================================
# MAIN MENU
# ======================================================================

main_menu() {
    local selected_env=""
    
    while true; do
        print_header
        echo -e "${BOLD}Build Options:${NC}"
        echo -e "${CYAN}══════════════════════════════════════════════════════════${NC}"
        echo "  1) Select environment and build"
        echo "  2) Select environment and upload"
        echo "  3) Select environment, clean and build"
        echo "  4) Select environment, clean, build and upload"
        echo "  5) Build all environments"
        echo "  6) Upload to all environments"
        echo "  7) Clean all environments"
        echo "  8) Serial monitor"
        echo "  9) Show environment details"
        echo " 10) Run build coverage matrix (ESP32/S3, USE_BAROMETER+USE_GPS)"
        echo "  0) Exit"
        echo -e "${CYAN}══════════════════════════════════════════════════════════${NC}"
        
        read -p "Select option: " choice
        
        case "$choice" in
            1)
                selected_env=$(select_environment)
                if [[ -n "$selected_env" ]]; then
                    build "$selected_env"
                    echo ""
                    read -p "Press Enter to continue..."
                fi
                ;;
            2)
                selected_env=$(select_environment)
                if [[ -n "$selected_env" ]]; then
                    upload "$selected_env"
                    echo ""
                    read -p "Press Enter to continue..."
                fi
                ;;
            3)
                selected_env=$(select_environment)
                if [[ -n "$selected_env" ]]; then
                    clean "$selected_env" && build "$selected_env"
                    echo ""
                    read -p "Press Enter to continue..."
                fi
                ;;
            4)
                selected_env=$(select_environment)
                if [[ -n "$selected_env" ]]; then
                    clean "$selected_env" && build "$selected_env" && upload "$selected_env"
                    echo ""
                    read -p "Press Enter to continue..."
                fi
                ;;
            5)
                build ""
                echo ""
                read -p "Press Enter to continue..."
                ;;
            6)
                upload ""
                echo ""
                read -p "Press Enter to continue..."
                ;;
            7)
                clean ""
                echo ""
                read -p "Press Enter to continue..."
                ;;
            8)
                monitor
                ;;
            9)
                print_header
                print_environments
                echo ""
                read -p "Press Enter to continue..."
                ;;
            10)
                build_matrix
                echo ""
                read -p "Press Enter to continue..."
                ;;
            0)
                print_header
                echo -e "${GREEN}Thank you for using dRehmFlight!${NC}"
                echo ""
                exit 0
                ;;
            *)
                print_error "Invalid option"
                sleep 1
                ;;
        esac
    done
}

# ======================================================================
# QUICK COMMAND LINE ARGUMENTS
# ======================================================================

# Check for command line arguments
if [[ $# -gt 0 ]]; then
    case "$1" in
        "clean")
            if [[ $# -eq 2 ]]; then
                clean "$2"
            else
                clean ""
            fi
            ;;
        "build")
            if [[ $# -eq 2 ]]; then
                build "$2"
            else
                build ""
            fi
            ;;
        "upload")
            if [[ $# -eq 2 ]]; then
                upload "$2"
            else
                upload ""
            fi
            ;;
        "monitor"|"serial")
            monitor
            ;;
        "matrix")
            build_matrix
            ;;
        "list"|"envs")
            print_header
            print_environments
            ;;
        "help"|"--help"|"-h")
            print_header
            echo -e "${BOLD}Usage:${NC}"
            echo "  ./build.sh                    - Interactive menu"
            echo "  ./build.sh clean [env]        - Clean build"
            echo "  ./build.sh build [env]        - Build for environment"
            echo "  ./build.sh upload [env]       - Upload to environment"
            echo "  ./build.sh monitor            - Start serial monitor"
            echo "  ./build.sh list               - List environments"
            echo "  ./build.sh matrix             - Run build coverage matrix"
            echo ""
            echo -e "${BOLD}Examples:${NC}"
            echo "  ./build.sh build teensy40     - Build for Teensy 4.0"
            echo "  ./build.sh upload teensy41    - Upload to Teensy 4.1"
            echo "  ./build.sh clean              - Clean all environments"
            echo ""
            ;;
        *)
            print_error "Unknown command: $1"
            echo "Use './build.sh help' for usage information"
            exit 1
            ;;
    esac
else
    # No arguments, start interactive menu
    main_menu
fi