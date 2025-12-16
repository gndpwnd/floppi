#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

pio_env1="teensy40"
pio_env2="teensy41"

clean() {
    printf "${YELLOW}[-] Cleaning...${NC}\n"
    pio run -t clean
}

build() {
    clear
    printf "${GREEN}[+] Building for all environments...${NC}\n"
    pio run
}

build_env() {
    clear
    printf "${BLUE}Select the board to build for: ${NC}\n"
    read -p "Teensy 4.0 or 4.1? [1/2]: " pio_env
    if [ "$pio_env" -eq 1 ]; then
        clear
        printf "${GREEN}[+] Building for Teensy 4.0...${NC}\n"
        pio run -e $pio_env1
    elif [ "$pio_env" -eq 2 ]; then
        clear
        printf "${GREEN}[+] Building for Teensy 4.1...${NC}\n"
        pio run -e $pio_env2
    else
        printf "${RED}[-] Invalid option!${NC}\n"
        exit 1
    fi
}

upload() {
    clear
    printf "${BLUE}Select the board to upload to: ${NC}\n"
    read -p "Teensy 4.0 or 4.1? [1/2]: " pio_env
    if [ "$pio_env" -eq 1 ]; then
        clear
        printf "${GREEN}[+] Uploading to Teensy 4.0...${NC}\n"
        pio run -t upload -e $pio_env1
    elif [ "$pio_env" -eq 2 ]; then
        clear
        printf "${GREEN}[+] Uploading to Teensy 4.1...${NC}\n"
        pio run -t upload -e $pio_env2
    else
        printf "${RED}[-] Invalid option!${NC}\n"
        exit 1
    fi
}

monitor() {
    clear
    printf "${GREEN}[+] Starting serial monitor...${NC}\n"
    pio device monitor
}

init() {
    clear
    printf "${GREEN}[+] dRehmFlight Build System\n${BLUE}"
    echo "1) Build all environments"
    echo "2) Build specific environment"
    echo "3) Upload"
    echo "4) Clean"
    echo "5) Monitor"
    echo "6) Clean, Build, and Upload"
    read -p "Select option: " choice
    
    case "$choice" in
        1) build ;;
        2) build_env ;;
        3) upload ;;
        4) clean ;;
        5) monitor ;;
        6) clean && build && upload ;;
        *) printf "${RED}[-] Invalid option!${NC}\n"; exit 1 ;;
    esac
}

init