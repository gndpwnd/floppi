#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'



pio_env1="t4"
pio_env2="t41"

clean () {

    printf "${YELLOW}[-] Cleaning UP...${NC}\n"
    files_2_remove=( 
        "./.pio"
        "./.vscode"
        "./.gitignore"
    )

    for file in "${files_2_remove[@]}"
    do
        rm -rf $file
    done
}

build () {
    clear
    printf "${GREEN}[+] Building...${NC}\n"
    pio run
}

upload () {
    clear
    printf "${BLUE}Select the board to upload to: ${NC}\n"
    read -p "Teensy 4.0 or 4.1? [1/2]: " pio_env
    if [ $pio_env -eq 1 ]; then
        clear
        printf "${GREEN}[+] Uploading to Teensy 4.0...${NC}\n"
        pio run -t upload -e $pio_env1
    elif [ $pio_env -eq 2 ]; then
        clear
        printf "${GREEN}[+] Uploading to Teensy 4.1...${NC}\n"
        pio run -t upload -e $pio_env2
    else
        printf "${RED}[-] Invalid option!${NC}\n"
        exit 1
    fi
}

commit () {
    clear
    clean
    git add -A
    printf "${BLUE}Enter commit message: ${NC}\n"
    read -p "> " commit_message
    printf "${GREEN}[+] ${NC}Committing...${NC}\n"
    git commit -m "$commit_message"
    printf "${GREEN}[+] ${NC}Pushing Commit...${NC}\n"
    git push
}

init () {
    clear
    printf "${GREEN}[+] Initializing...\n${BLUE}"
    read -p "Build (b), Upload (u), or Commit (c): " choice
    if [ "$choice" = "b" ]; then
        build
    elif [ "$choice" = "u" ]; then
        upload
    elif [ "$choice" = "c" ]; then
        commit
    else
        printf "${RED}[-] Invalid option!${NC}\n"
        exit 1
    fi
}

clean
init