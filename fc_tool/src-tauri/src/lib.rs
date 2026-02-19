use serde::Serialize;
use serialport::SerialPort;
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use tauri::{AppHandle, Emitter, State};

/// Format current time as ISO-ish timestamp (UTC seconds + millis, no chrono dep).
/// Output: "1740000000.123"
fn format_timestamp() -> String {
    let d = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    format!("{}.{:03}", d.as_secs(), d.subsec_millis())
}

// ============================================================================
// Startup Args (passed from CLI → managed state → frontend)
// ============================================================================

#[derive(Clone, Serialize)]
pub struct StartupArgs {
    pub port: Option<String>,
    pub baud: Option<u32>,
}

// ============================================================================
// Serial State
// ============================================================================

pub struct SerialState {
    port: Arc<Mutex<Option<Box<dyn SerialPort + Send>>>>,
    reader_running: Arc<AtomicBool>,
    connected_port: Arc<Mutex<Option<String>>>,
    log_writer: Arc<Mutex<Option<fs::File>>>,
    log_path: Arc<Mutex<Option<String>>>,
}

impl Default for SerialState {
    fn default() -> Self {
        Self {
            port: Arc::new(Mutex::new(None)),
            reader_running: Arc::new(AtomicBool::new(false)),
            connected_port: Arc::new(Mutex::new(None)),
            log_writer: Arc::new(Mutex::new(None)),
            log_path: Arc::new(Mutex::new(None)),
        }
    }
}

#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub port_type: String,
    pub board_name: Option<String>,
    pub manufacturer: Option<String>,
    pub serial_number: Option<String>,
    pub product: Option<String>,
    pub is_usb: bool,
}

/// Identify board by USB VID/PID
fn identify_board(vid: u16, pid: u16) -> Option<&'static str> {
    match (vid, pid) {
        // Teensy (PJRC)
        (0x16C0, 0x0483) => Some("Teensy"),
        (0x16C0, 0x0478) => Some("Teensy (bootloader)"),

        // Arduino Official
        (0x2341, 0x0043) => Some("Arduino Uno"),
        (0x2341, 0x0042) => Some("Arduino Mega 2560"),
        (0x2341, 0x8036) => Some("Arduino Leonardo"),
        (0x2341, 0x8037) => Some("Arduino Micro"),
        (0x2341, 0x003D) => Some("Arduino Due (Prog)"),
        (0x2341, 0x003E) => Some("Arduino Due (Native)"),
        (0x2341, 0x0069) => Some("Arduino Uno R4 Minima"),
        (0x2341, 0x1002) => Some("Arduino Uno R4 WiFi"),

        // ESP32 Native USB
        (0x303A, 0x0002) => Some("ESP32-S2"),
        (0x303A, 0x1001) => Some("ESP32-S3/C3"),

        // Common USB-Serial chips (used by ESP32 DevKits, clones)
        (0x10C4, 0xEA60) => Some("CP2102 (ESP32/generic)"),
        (0x1A86, 0x7523) => Some("CH340 (clone/ESP32)"),
        (0x0403, 0x6001) => Some("FTDI FT232R"),
        (0x067B, 0x2303) => Some("PL2303"),

        _ => None,
    }
}

/// Filter out ports that are never relevant for flight controllers.
/// - macOS: remove `/dev/tty.*` duplicates (keep `cu.*` only — tty.* blocks on open)
/// - All platforms: remove Bluetooth serial ports
fn filter_ports(ports: Vec<serialport::SerialPortInfo>) -> Vec<serialport::SerialPortInfo> {
    ports
        .into_iter()
        .filter(|p| {
            // macOS: filter tty.* duplicates (cu.* is the correct one to use)
            #[cfg(target_os = "macos")]
            if p.port_name.starts_with("/dev/tty.") {
                return false;
            }

            // All platforms: filter Bluetooth ports
            if matches!(p.port_type, serialport::SerialPortType::BluetoothPort) {
                return false;
            }

            // macOS: filter Bluetooth by name (some don't report as BluetoothPort)
            #[cfg(target_os = "macos")]
            if p.port_name.contains("Bluetooth") {
                return false;
            }

            true
        })
        .collect()
}

/// Sort ports: known USB boards first, then unknown USB, then non-USB.
/// Alphabetical within each tier.
fn sort_ports(ports: &mut [SerialPortInfo]) {
    ports.sort_by(|a, b| {
        fn rank(p: &SerialPortInfo) -> u8 {
            if p.is_usb && p.board_name.is_some() {
                0
            } else if p.is_usb {
                1
            } else {
                2
            }
        }
        rank(a).cmp(&rank(b)).then(a.name.cmp(&b.name))
    });
}

#[derive(Clone, Serialize)]
pub struct SerialDataEvent {
    pub data: String,
}

#[derive(Clone, Serialize)]
pub struct SerialErrorEvent {
    pub error: String,
}

#[derive(Clone, Serialize)]
pub struct SerialDisconnectedEvent {
    pub port: String,
}

#[derive(Serialize)]
pub struct ConnectionStatus {
    pub connected: bool,
    pub port: Option<String>,
}

// ============================================================================
// Tauri Commands
// ============================================================================

#[tauri::command]
fn list_serial_ports() -> Result<Vec<SerialPortInfo>, String> {
    let raw_ports = serialport::available_ports().map_err(|e| e.to_string())?;
    let filtered = filter_ports(raw_ports);

    let mut result: Vec<SerialPortInfo> = filtered
        .into_iter()
        .map(|p| {
            let (port_type, board_name, manufacturer, serial_number, product, is_usb) =
                match &p.port_type {
                    serialport::SerialPortType::UsbPort(info) => {
                        let board = identify_board(info.vid, info.pid);
                        let type_str = if board.is_some() {
                            format!("{:04X}:{:04X}", info.vid, info.pid)
                        } else {
                            format!(
                                "{:04X}:{:04X}{}",
                                info.vid,
                                info.pid,
                                info.product
                                    .as_deref()
                                    .map(|s| format!(" ({})", s))
                                    .unwrap_or_default()
                            )
                        };
                        (
                            type_str,
                            board.map(|s| s.to_string()),
                            info.manufacturer.clone(),
                            info.serial_number.clone(),
                            info.product.clone(),
                            true,
                        )
                    }
                    serialport::SerialPortType::PciPort => {
                        ("PCI".to_string(), None, None, None, None, false)
                    }
                    serialport::SerialPortType::BluetoothPort => {
                        ("Bluetooth".to_string(), None, None, None, None, false)
                    }
                    serialport::SerialPortType::Unknown => {
                        ("Unknown".to_string(), None, None, None, None, false)
                    }
                };
            SerialPortInfo {
                name: p.port_name,
                port_type,
                board_name,
                manufacturer,
                serial_number,
                product,
                is_usb,
            }
        })
        .collect();

    sort_ports(&mut result);
    Ok(result)
}

#[tauri::command]
fn open_serial_port(
    port_name: String,
    baud_rate: u32,
    state: State<'_, SerialState>,
    app: AppHandle,
) -> Result<(), String> {
    // Close existing connection if any
    close_serial_port_internal(&state)?;

    // Open new port
    let port = serialport::new(&port_name, baud_rate)
        .timeout(Duration::from_millis(100))
        .open()
        .map_err(|e| format!("Failed to open {}: {}", port_name, e))?;

    // Clone port for reader thread
    let reader_port = port
        .try_clone()
        .map_err(|e| format!("Failed to clone port: {}", e))?;

    // Store port in state
    {
        let mut port_lock = state.port.lock().map_err(|e| e.to_string())?;
        *port_lock = Some(port);
    }
    {
        let mut connected = state.connected_port.lock().map_err(|e| e.to_string())?;
        *connected = Some(port_name.clone());
    }

    // Start reader thread
    state.reader_running.store(true, Ordering::SeqCst);
    let running = state.reader_running.clone();
    let port_arc = state.port.clone();
    let connected_arc = state.connected_port.clone();
    let log_writer_arc = state.log_writer.clone();
    let port_name_for_thread = port_name.clone();

    thread::spawn(move || {
        let mut reader = BufReader::new(reader_port);
        let mut line = String::new();

        while running.load(Ordering::SeqCst) {
            line.clear();
            match reader.read_line(&mut line) {
                Ok(0) => {
                    // EOF - port closed/disconnected
                    break;
                }
                Ok(_) => {
                    let _ = app.emit("serial-data", SerialDataEvent { data: line.clone() });
                    // Write to log file if active (with timestamp prefix)
                    if let Ok(mut lw) = log_writer_arc.lock() {
                        if let Some(ref mut f) = *lw {
                            let ts = format_timestamp();
                            let _ = write!(f, "[{}] {}", ts, line);
                        }
                    }
                }
                Err(e) => {
                    if e.kind() != std::io::ErrorKind::TimedOut {
                        let _ = app.emit(
                            "serial-error",
                            SerialErrorEvent {
                                error: e.to_string(),
                            },
                        );
                        // Non-timeout error = likely disconnect
                        break;
                    }
                }
            }
        }

        // If reader exited unexpectedly (not user-initiated close), clean up
        if running.load(Ordering::SeqCst) {
            running.store(false, Ordering::SeqCst);
            if let Ok(mut p) = port_arc.lock() {
                *p = None;
            }
            if let Ok(mut c) = connected_arc.lock() {
                *c = None;
            }
            let _ = app.emit(
                "serial-disconnected",
                SerialDisconnectedEvent {
                    port: port_name_for_thread,
                },
            );
        }
    });

    Ok(())
}

fn close_serial_port_internal(state: &State<'_, SerialState>) -> Result<(), String> {
    // Signal reader to stop
    state.reader_running.store(false, Ordering::SeqCst);

    // Close port
    {
        let mut port_lock = state.port.lock().map_err(|e| e.to_string())?;
        *port_lock = None;
    }
    {
        let mut connected = state.connected_port.lock().map_err(|e| e.to_string())?;
        *connected = None;
    }

    // Give reader thread time to exit
    thread::sleep(Duration::from_millis(150));

    Ok(())
}

#[tauri::command]
fn close_serial_port(state: State<'_, SerialState>) -> Result<(), String> {
    close_serial_port_internal(&state)
}

#[tauri::command]
fn send_serial_data(data: String, state: State<'_, SerialState>) -> Result<(), String> {
    let mut port_lock = state.port.lock().map_err(|e| e.to_string())?;
    let port = port_lock
        .as_mut()
        .ok_or_else(|| "No serial port connected".to_string())?;

    port.write_all(data.as_bytes())
        .map_err(|e| format!("Write failed: {}", e))?;
    port.flush().map_err(|e| format!("Flush failed: {}", e))?;

    Ok(())
}

#[tauri::command]
fn get_connection_status(state: State<'_, SerialState>) -> Result<ConnectionStatus, String> {
    let connected = state.connected_port.lock().map_err(|e| e.to_string())?;
    Ok(ConnectionStatus {
        connected: connected.is_some(),
        port: connected.clone(),
    })
}

/// Returns CLI startup args (port/baud) so frontend can auto-connect
#[tauri::command]
fn get_startup_args(state: State<'_, StartupArgs>) -> StartupArgs {
    state.inner().clone()
}

/// Start logging serial data to a file. Returns the log file path.
#[tauri::command]
fn start_log(file_path: Option<String>, state: State<'_, SerialState>) -> Result<String, String> {
    let path = file_path.unwrap_or_else(|| {
        let secs = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();
        format!("fc_tool_{}.log", secs)
    });

    // Create parent dirs if needed
    if let Some(parent) = std::path::Path::new(&path).parent() {
        let _ = fs::create_dir_all(parent);
    }

    let file = fs::File::create(&path)
        .map_err(|e| format!("Failed to create log file {}: {}", path, e))?;

    {
        let mut lw = state.log_writer.lock().map_err(|e| e.to_string())?;
        *lw = Some(file);
    }
    {
        let mut lp = state.log_path.lock().map_err(|e| e.to_string())?;
        *lp = Some(path.clone());
    }

    Ok(path)
}

/// Stop logging serial data. Returns the path of the closed log file.
#[tauri::command]
fn stop_log(state: State<'_, SerialState>) -> Result<Option<String>, String> {
    let path;
    {
        let mut lp = state.log_path.lock().map_err(|e| e.to_string())?;
        path = lp.take();
    }
    {
        let mut lw = state.log_writer.lock().map_err(|e| e.to_string())?;
        *lw = None;
    }
    Ok(path)
}

// ============================================================================
// USB Hot-Plug Detection (polling)
// ============================================================================

#[derive(Clone, Serialize)]
pub struct PortsChangedEvent {
    pub added: Vec<String>,
    pub removed: Vec<String>,
}

/// Start a background thread that polls for USB port changes every 2 seconds.
fn start_port_watcher(app: AppHandle) {
    thread::spawn(move || {
        let mut known_ports: Vec<String> = serialport::available_ports()
            .unwrap_or_default()
            .into_iter()
            .map(|p| p.port_name)
            .collect();
        known_ports.sort();

        loop {
            thread::sleep(Duration::from_secs(2));

            let mut current: Vec<String> = serialport::available_ports()
                .unwrap_or_default()
                .into_iter()
                .map(|p| p.port_name)
                .collect();
            current.sort();

            if current != known_ports {
                let added: Vec<String> = current
                    .iter()
                    .filter(|p| !known_ports.contains(p))
                    .cloned()
                    .collect();
                let removed: Vec<String> = known_ports
                    .iter()
                    .filter(|p| !current.contains(p))
                    .cloned()
                    .collect();

                if !added.is_empty() || !removed.is_empty() {
                    let _ = app.emit(
                        "ports-changed",
                        PortsChangedEvent { added, removed },
                    );
                }

                known_ports = current;
            }
        }
    });
}

// ============================================================================
// GUI Mode (Tauri window)
// ============================================================================

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run_gui(port: Option<String>, baud: Option<u32>) {
    let startup_args = StartupArgs { port, baud };

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(SerialState::default())
        .manage(startup_args)
        .invoke_handler(tauri::generate_handler![
            list_serial_ports,
            open_serial_port,
            close_serial_port,
            send_serial_data,
            get_connection_status,
            get_startup_args,
            start_log,
            stop_log
        ])
        .setup(|app| {
            start_port_watcher(app.handle().clone());
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

// ============================================================================
// Force-Release Serial Port
// ============================================================================

/// Force-release a serial port held by stale processes.
/// Uses `fuser -k` on Linux. On other platforms, prints an error.
pub fn kill_port(port_path: &str) {
    eprintln!("fc_tool: force-releasing {}", port_path);

    // Check if port exists
    if !std::path::Path::new(port_path).exists() {
        eprintln!("Port {} does not exist", port_path);
        std::process::exit(1);
    }

    // Check who holds the port
    match Command::new("fuser").arg(port_path).output() {
        Ok(output) => {
            let holders = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if holders.is_empty() {
                eprintln!("Port {} is not held by any process", port_path);
                return;
            }
            eprintln!("Port {} held by PIDs: {}", port_path, holders);

            // Kill holders
            match Command::new("fuser").arg("-k").arg(port_path).output() {
                Ok(_) => {
                    // Wait for processes to die
                    thread::sleep(Duration::from_secs(1));
                    eprintln!("Released {}", port_path);
                }
                Err(e) => {
                    eprintln!("Failed to release port: {}", e);
                    std::process::exit(1);
                }
            }
        }
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                eprintln!("'fuser' not found. Install psmisc: sudo apt-get install psmisc");
            } else {
                eprintln!("Failed to check port holders: {}", e);
            }
            std::process::exit(1);
        }
    }
}

// ============================================================================
// Headless Mode (no GUI, serial → stdout)
// ============================================================================

pub fn run_headless(port: Option<String>, baud: Option<u32>, log_file: Option<String>) {
    let port_name = match port {
        Some(p) => p,
        None => {
            eprintln!("Error: --headless requires --port <port>");
            eprintln!("Usage: fc_tool --headless --port /dev/ttyACM0 --baud 115200");
            // List available ports as a hint
            if let Ok(ports) = serialport::available_ports() {
                if !ports.is_empty() {
                    eprintln!("\nAvailable ports:");
                    for p in &ports {
                        let board = match &p.port_type {
                            serialport::SerialPortType::UsbPort(info) => {
                                identify_board(info.vid, info.pid)
                                    .map(|s| s.to_string())
                                    .unwrap_or_default()
                            }
                            _ => String::new(),
                        };
                        if board.is_empty() {
                            eprintln!("  {}", p.port_name);
                        } else {
                            eprintln!("  {} ({})", p.port_name, board);
                        }
                    }
                }
            }
            std::process::exit(1);
        }
    };
    let baud_rate = baud.unwrap_or(115200);

    // Set up log file if requested
    let mut log_writer: Option<fs::File> = None;
    if let Some(ref log_path) = log_file {
        // Create parent dirs if needed
        if let Some(parent) = std::path::Path::new(log_path).parent() {
            let _ = fs::create_dir_all(parent);
        }
        match fs::File::create(log_path) {
            Ok(f) => {
                eprintln!("Logging to: {}", log_path);
                log_writer = Some(f);
            }
            Err(e) => {
                eprintln!("Warning: Failed to open log file {}: {}", log_path, e);
            }
        }
    }

    eprintln!("fc_tool headless: {} @ {} baud", port_name, baud_rate);
    eprintln!("Press Ctrl+C to exit\n");

    let port = match serialport::new(&port_name, baud_rate)
        .timeout(Duration::from_millis(500))
        .open()
    {
        Ok(p) => p,
        Err(e) => {
            eprintln!("Error: Failed to open {}: {}", port_name, e);
            std::process::exit(1);
        }
    };

    // Clone port for stdin forwarding thread
    let mut writer = port.try_clone().expect("Failed to clone serial port");

    // Stdin → serial forwarding thread (enables sending commands in headless mode)
    thread::spawn(move || {
        let stdin = std::io::stdin();
        let mut input = String::new();
        loop {
            input.clear();
            match stdin.read_line(&mut input) {
                Ok(0) => break, // EOF
                Ok(_) => {
                    let _ = writer.write_all(input.as_bytes());
                    let _ = writer.flush();
                }
                Err(_) => break,
            }
        }
    });

    // Serial → stdout reader (main thread)
    let mut reader = BufReader::new(port);
    let mut line = String::new();
    let stdout = std::io::stdout();
    let mut out = stdout.lock();

    loop {
        line.clear();
        match reader.read_line(&mut line) {
            Ok(0) => {
                eprintln!("\nDevice disconnected");
                break;
            }
            Ok(_) => {
                // Raw serial data to stdout (verbose — includes all telemetry/plotter data)
                let _ = out.write_all(line.as_bytes());
                // Also write to log file if active (with timestamp)
                if let Some(ref mut lw) = log_writer {
                    let ts = format_timestamp();
                    let _ = write!(lw, "[{}] {}", ts, line);
                }
            }
            Err(e) => {
                if e.kind() == std::io::ErrorKind::TimedOut {
                    continue;
                }
                eprintln!("\nSerial error: {}", e);
                break;
            }
        }
    }
}
