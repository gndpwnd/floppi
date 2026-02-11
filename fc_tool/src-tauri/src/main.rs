// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::env;

fn main() {
    let args: Vec<String> = env::args().collect();

    let port = find_arg(&args, "--port");
    let baud = find_arg(&args, "--baud").and_then(|s| s.parse::<u32>().ok());
    let headless = args.iter().any(|a| a == "--headless");

    if headless {
        fc_tool_lib::run_headless(port, baud);
    } else {
        fc_tool_lib::run_gui(port, baud);
    }
}

fn find_arg(args: &[String], flag: &str) -> Option<String> {
    args.iter()
        .position(|a| a == flag)
        .and_then(|i| args.get(i + 1))
        .cloned()
}
