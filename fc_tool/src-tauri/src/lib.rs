use serde::Serialize;
use serialport::SerialPort;
use std::io::{BufRead, BufReader};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use tauri::{AppHandle, Emitter, State};

// State for managing serial connection
pub struct SerialState {
    port: Arc<Mutex<Option<Box<dyn SerialPort + Send>>>>,
    reader_running: Arc<AtomicBool>,
    connected_port: Arc<Mutex<Option<String>>>,
}

impl Default for SerialState {
    fn default() -> Self {
        Self {
            port: Arc::new(Mutex::new(None)),
            reader_running: Arc::new(AtomicBool::new(false)),
            connected_port: Arc::new(Mutex::new(None)),
        }
    }
}

#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub port_type: String,
    pub board_name: Option<String>,
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

#[tauri::command]
fn list_serial_ports() -> Result<Vec<SerialPortInfo>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    Ok(ports
        .into_iter()
        .map(|p| {
            let (port_type, board_name) = match &p.port_type {
                serialport::SerialPortType::UsbPort(info) => {
                    let board = identify_board(info.vid, info.pid);
                    let type_str = if board.is_some() {
                        // Simplified display when board is identified
                        format!("{:04X}:{:04X}", info.vid, info.pid)
                    } else {
                        // Show product name for unknown devices
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
                    (type_str, board.map(|s| s.to_string()))
                }
                serialport::SerialPortType::BluetoothPort => ("Bluetooth".to_string(), None),
                serialport::SerialPortType::PciPort => ("PCI".to_string(), None),
                serialport::SerialPortType::Unknown => ("Unknown".to_string(), None),
            };
            SerialPortInfo {
                name: p.port_name,
                port_type,
                board_name,
            }
        })
        .collect())
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

    use std::io::Write;
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

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(SerialState::default())
        .invoke_handler(tauri::generate_handler![
            list_serial_ports,
            open_serial_port,
            close_serial_port,
            send_serial_data,
            get_connection_status
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
