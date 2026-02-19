# Serial Port Filtering & Detection Research

Research for fc_tool (Tauri/Rust app using `serialport-rs` crate).
Date: 2026-02-18

---

## 1. What Does `serialport-rs` Already Filter?

### Answer: It does significant filtering, but NOT all serial-like devices are excluded.

`serialport::available_ports()` does **not** return keyboards, mice, or other HID devices. It specifically enumerates devices registered as serial/TTY ports through OS-level serial subsystems. However, the exact behavior differs by platform and whether `libudev` is enabled.

### Return Types

The `SerialPortType` enum has four variants:

```rust
pub enum SerialPortType {
    UsbPort(UsbPortInfo),  // USB serial — includes VID, PID, serial number, manufacturer, product
    PciPort,               // Permanent motherboard/PCI serial ports
    BluetoothPort,         // Bluetooth serial (rfcomm)
    Unknown,               // Cannot determine type
}
```

### What `UsbPortInfo` Provides

```rust
pub struct UsbPortInfo {
    pub vid: u16,                        // Vendor ID
    pub pid: u16,                        // Product ID
    pub serial_number: Option<String>,
    pub manufacturer: Option<String>,
    pub product: Option<String>,
    // Only with feature = "usbportinfo-interface":
    pub interface: Option<u8>,           // USB interface number
}
```

### Linux Filtering (with libudev)

Source: `src/posix/enumerate.rs`, lines 499-540

The libudev path does the most intelligent filtering:

1. **Enumerates only `tty` subsystem** via `enumerator.match_subsystem("tty")`.
2. **Requires a parent device** — orphan TTY nodes (like bare `/dev/tty0`) are skipped. Exception: `rfcomm` devices (Bluetooth) have no parent but are kept.
3. **Filters out ghost serial8250 ports** — if the parent driver is `serial8250`, it attempts to actually **open the port** with `serialport::new(devnode, 9600).open()`. If the open fails, the port is skipped. This is how it distinguishes real hardware UART ports (ttyS0) from the 64 phantom ports the kernel always creates.
4. **Determines port type via udev properties**: checks `ID_BUS` for "usb" or "pci", then extracts VID/PID/serial from udev properties.

```rust
// Key filtering logic (simplified)
for d in devices {
    let parent = d.parent();
    if parent.is_some() || is_rfcomm(&d) {
        if let Some(driver) = parent.as_ref().and_then(|d| d.driver()) {
            if driver == "serial8250" && crate::new(devnode, 9600).open().is_err() {
                continue;  // Skip phantom serial8250 ports
            }
        }
        // ... determine port_type and add to list
    }
}
```

### Linux Filtering (without libudev — musl builds)

Falls back to scanning `/sys/class/tty/`:

1. Follows `device` symlinks and checks the `subsystem` directory.
2. Recognizes: `amba` (RPi SoC UARTs) as Unknown, `pci` as PciPort, `usb`/`usb-serial` as UsbPort, `pnp` as Unknown.
3. Skips entries without a `device` subdirectory (filters out virtual TTYs).
4. Does **not** do the serial8250 open-to-verify trick — relies on the subsystem check only.

### macOS Filtering

Uses IOKit framework:

1. Searches for `IOSerialBSDServiceValue` matching `kIOSerialBSDAllTypes`.
2. Returns **both** `IOCalloutDevice` and `IODialinDevice` entries — meaning you get **both** `/dev/cu.*` and `/dev/tty.*` for each device.
3. Walks the IORegistry parent chain to identify USB (`IOUSBHostInterface`) or Bluetooth (`IOBluetoothSerialClient`) devices.
4. Falls back to `PciPort` for anything that isn't USB or Bluetooth.

### Windows Filtering

Uses SetupDi API:

1. Queries the "Ports" and "Modem" device classes.
2. Filters out **disabled devices** (checks `CM_PROB_DISABLED`).
3. Explicitly filters out parallel ports (skips names starting with "LPT").
4. Parses hardware IDs (HWID) to extract VID/PID for USB devices.
5. Falls back to reading `HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM` registry key for any COM ports not found via SetupDi, adding them as `Unknown` type.

### Key Takeaway

`serialport-rs` already does meaningful filtering, but the result set is "all serial ports the OS knows about" — not "only microcontroller/development boards." You will still see:
- Bluetooth serial ports (rfcomm)
- Built-in motherboard UARTs (ttyS0 on Linux, COM1 on Windows)
- USB modems that happen to expose serial ports
- macOS: both cu.* and tty.* variants of the same physical port

---

## 2. USB Device Class Filtering

### Relevant USB Class Codes

| Class | Subclass | Protocol | Description | Examples |
|-------|----------|----------|-------------|----------|
| 0x02 | 0x02 | 0x01 | CDC ACM (Abstract Control Model) | Teensy, ESP32-S3 native USB, Arduino Leonardo |
| 0xEF | 0x02 | 0x01 | Misc / Interface Association | Composite CDC devices (CDC + other) |
| 0xFF | - | - | Vendor-specific | FTDI FT232R, some CP210x modes |
| 0x02 | 0x0D | - | CDC NCM (Network Control Model) | USB networking (NOT serial — would not appear) |

### How to Distinguish Serial Devices from Other USB Devices

`serialport-rs` does **not** expose USB class codes — it only provides VID/PID. However, this is actually sufficient because:

1. **The OS already filters by serial class.** Only devices that register as TTY/COM ports appear in `available_ports()`. A USB keyboard (HID class 0x03) never creates a `/dev/ttyACM*` device, so it never appears.

2. **VID/PID is the practical approach.** This is what Arduino IDE, PlatformIO, and fc_tool already do. The existing `identify_board()` function in `lib.rs` is the right pattern.

### Common VID/PID Values for Development Boards

Already implemented in fc_tool's `identify_board()` — here's the extended reference:

```rust
// --- Dedicated USB-to-UART bridge chips ---
// These create /dev/ttyUSB* on Linux (not ttyACM)
(0x0403, 0x6001) => "FTDI FT232R",
(0x0403, 0x6010) => "FTDI FT2232",      // dual-port
(0x0403, 0x6011) => "FTDI FT4232",      // quad-port
(0x0403, 0x6014) => "FTDI FT232H",
(0x0403, 0x6015) => "FTDI FT-X series",
(0x10C4, 0xEA60) => "CP2102/CP2104",
(0x10C4, 0xEA70) => "CP2105",           // dual-port
(0x1A86, 0x7523) => "CH340/CH341",
(0x1A86, 0x55D4) => "CH9102",
(0x067B, 0x2303) => "PL2303",
(0x067B, 0x23A3) => "PL2303GC/GS",

// --- Native USB CDC (ACM) devices ---
// These create /dev/ttyACM* on Linux
(0x16C0, 0x0483) => "Teensy (USB Serial)",
(0x16C0, 0x0478) => "Teensy (HalfKay bootloader)",
(0x2341, _)      => "Arduino (official)",  // any Arduino product
(0x2A03, _)      => "Arduino (clone .org)",
(0x1B4F, _)      => "SparkFun",
(0x239A, _)      => "Adafruit",
(0x303A, 0x0002) => "ESP32-S2 native USB",
(0x303A, 0x1001) => "ESP32-S3/C3 native USB",
(0x1D50, 0x6018) => "Black Magic Probe",
(0x0483, 0x5740) => "STM32 Virtual COM Port",
```

### How Linux Chooses ttyACM vs ttyUSB

This distinction matters for understanding what you'll see:

| Driver | Device Name | Used By |
|--------|-------------|---------|
| `cdc_acm` | `/dev/ttyACM*` | Native USB CDC devices (Teensy, ESP32-S3, Arduino Leonardo, STM32) |
| `ftdi_sio` | `/dev/ttyUSB*` | FTDI chips (FT232R, etc.) |
| `cp210x` | `/dev/ttyUSB*` | Silicon Labs CP2102/2104 |
| `ch341` | `/dev/ttyUSB*` | WinChipHead CH340/341 |
| `pl2303` | `/dev/ttyUSB*` | Prolific PL2303 |

All of these are enumerated by `serialport-rs` correctly, with full VID/PID info when libudev is available.

---

## 3. Platform Differences

### Linux

| Path Pattern | What It Is | Real? | Notes |
|---|---|---|---|
| `/dev/ttyACM*` | USB CDC ACM devices | Yes | Teensy, ESP32-S3, Arduino Leonardo |
| `/dev/ttyUSB*` | USB-serial bridge chips | Yes | FTDI, CP2102, CH340 |
| `/dev/ttyS0` | Motherboard UART (COM1) | Maybe | Real if hardware exists; serial8250 creates phantom ttyS0-ttyS63 |
| `/dev/ttyS1-S63` | Legacy serial ports | Usually no | Phantom ports from serial8250 driver. serialport-rs filters these via open-test. |
| `/dev/ttyAMA*` | ARM SoC UART (RPi) | Yes | Raspberry Pi hardware UART |
| `/dev/rfcomm*` | Bluetooth serial | Yes | Only present when paired |
| `/dev/tty0-63` | Virtual console TTYs | No | Never returned by serialport-rs (no parent device) |
| `/dev/pts/*` | Pseudo-terminals | No | Never returned by serialport-rs |

**Ghost port problem:** The `serial8250` kernel driver always registers ttyS0-ttyS3 (or more). Most modern PCs have zero physical RS-232 ports. `serialport-rs` with libudev handles this by attempting to open each serial8250 port — if the open fails, the port is phantom and gets skipped. Without libudev, the fallback `/sys/class/tty` scanner uses subsystem detection instead.

### Windows

| Pattern | What It Is | Real? | Notes |
|---|---|---|---|
| `COM1`, `COM2` | Motherboard/PCI serial | Maybe | Often phantom on modern PCs |
| `COM3`+ | Usually USB serial | Yes | Assigned dynamically when USB devices connect |
| High COM numbers | Often reassigned ports | Yes | Windows increments COM numbers; can accumulate stale assignments |

**Windows quirks:**
- COM port numbers persist in the registry even after a USB device is removed. Over time, COM numbers can climb to COM20+ as devices are plugged into different USB ports.
- The `HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM` registry key lists only currently-present ports.
- `serialport-rs` checks device problem status and skips disabled devices.
- Parallel ports (LPT) are explicitly filtered out since they share the "Ports" device class.

### macOS

| Pattern | What It Is | Notes |
|---|---|---|
| `/dev/cu.usbmodem*` | USB CDC ACM (call-out) | Native USB devices (Teensy, ESP32-S3). **Use this one.** |
| `/dev/tty.usbmodem*` | USB CDC ACM (call-in) | Same device, blocks on open until carrier detect. Avoid. |
| `/dev/cu.usbserial-*` | USB-serial bridge (call-out) | FTDI, CP2102, etc. **Use this one.** |
| `/dev/tty.usbserial-*` | USB-serial bridge (call-in) | Same device. Avoid. |
| `/dev/cu.Bluetooth-*` | Bluetooth serial | System Bluetooth ports |
| `/dev/tty.Bluetooth-*` | Bluetooth serial | Same, call-in variant |

**macOS quirk — duplicate ports:** `serialport-rs` returns **both** `cu.*` and `tty.*` for every device. For a serial monitor app, you should filter to show only `cu.*` ports, or at minimum prefer them. The `tty.*` variants will block on open until DCD (carrier detect) is asserted, which never happens with microcontrollers.

**Recommended filter for macOS:**
```rust
#[cfg(target_os = "macos")]
fn filter_macos_ports(ports: Vec<SerialPortInfo>) -> Vec<SerialPortInfo> {
    ports.into_iter()
        .filter(|p| {
            // Prefer cu.* (callout) over tty.* (dialin)
            // tty.* blocks on open waiting for carrier detect
            !p.port_name.starts_with("/dev/tty.")
        })
        .collect()
}
```

---

## 4. How Existing Tools Handle Port Filtering

### Arduino IDE 2 (serial-discovery)

Arduino uses a [pluggable discovery system](https://github.com/arduino/serial-discovery). Key design:

1. **Lists all serial ports** — does not hide system ports.
2. **VID/PID matching** — the IDE matches VID/PID against a database in `boards.txt` to show board names. This is how "Arduino Uno" appears next to COM3.
3. **Event-based updates** — `START_SYNC` mode sends `add`/`remove` events as ports appear and disappear. Functionally identical to fc_tool's `start_port_watcher()`.
4. **No filtering** — all ports shown; known boards are labeled. User picks from the full list.

### PlatformIO

1. **Uses pyserial's `serial.tools.list_ports`** — Python equivalent of `serialport-rs`.
2. **Auto-detection via VID/PID** — board definitions include `upload_port_vid` and `upload_port_pid`. PlatformIO matches these to auto-select the upload port.
3. **`pio device list`** shows all ports with hardware info.
4. **No system port filtering** — all ports shown. Users configure `monitor_port` in `platformio.ini` if auto-detection fails.

### PuTTY

1. **Lists all COM ports** from Windows registry (`SERIALCOMM`).
2. **No filtering at all** — shows COM1 through whatever exists.
3. **No VID/PID info** — just port names. User must know which COM port to use.

### CoolTerm

1. **Lists all available ports** — no filtering.
2. **"Re-Scan" button** for hot-plug detection (manual, not automatic).
3. **Shows driver info** — clicking an info button shows driver name and rated speed.
4. **No board identification** — port names only.

### Common Pattern

All major tools follow the same approach: **list everything, label known devices.** None hide system ports. The differentiator is how much metadata (VID/PID, board name, manufacturer) is shown alongside each port.

---

## 5. Port Activity Detection

### Can you detect data flow without opening the port?

**No.** There is no cross-platform way to detect serial data activity without opening the port.

- **Linux:** The kernel buffers incoming data only after a process has the port open. There is no `/proc` or `/sys` entry showing "data waiting on this port."
- **Windows:** Same — data is only delivered to an open handle.
- **macOS:** Same — IOKit does not expose a "data pending" indicator.

### What about checking `bytes_to_read()` without fully opening?

You must call `serialport::new(...).open()` first. The `bytes_to_read()` method requires an open `SerialPort` object. There is no shortcut.

### Exclusive Access Implications

| Platform | Behavior | Notes |
|---|---|---|
| **Linux** | **Non-exclusive by default** | Multiple processes can open the same `/dev/ttyACM0` simultaneously. Both see the data stream. Use `flock()` or `TIOCEXCL` ioctl for exclusive access. `serialport-rs` has an optional `flock` feature. |
| **Windows** | **Exclusive by default** | Opening a COM port gives exclusive access. A second `CreateFile` on the same port fails with `ACCESS_DENIED`. This is OS-enforced. |
| **macOS** | **Non-exclusive by default** | Similar to Linux. Multiple opens succeed. Use `TIOCEXCL` ioctl for exclusive mode. |

### Practical Implications for fc_tool

1. **On Linux:** If another process (ModemManager, PlatformIO, another serial monitor) has the port open, you can still open it — but you'll get garbled data as both processes consume bytes. This is why `fuser -k` and stopping ModemManager are important.

2. **On Windows:** If another process has the port, your open will fail immediately with a clear error. This is actually more user-friendly.

3. **Checking port availability:** The only reliable way is to try opening the port. If it succeeds, the port is available. There is no "is this port in use?" API.

```rust
/// Check if a port can be opened (without keeping it open)
fn is_port_available(port_name: &str) -> bool {
    match serialport::new(port_name, 9600)
        .timeout(std::time::Duration::from_millis(50))
        .open()
    {
        Ok(_port) => true, // port is dropped here, closing it
        Err(_) => false,
    }
}
```

**Warning:** This function has side effects. Opening a serial port on Linux momentarily asserts DTR, which reboots Arduino boards (but not Teensy 4.x). Do NOT call this in a polling loop for port scanning.

---

## 6. Best Practices for a Serial Monitor App

### Recommended Port List Strategy

Based on how all major tools work, plus platform-specific knowledge:

#### Tier 1: Filter obvious noise

```rust
fn filter_ports(ports: Vec<serialport::SerialPortInfo>) -> Vec<serialport::SerialPortInfo> {
    ports.into_iter().filter(|p| {
        let name = &p.port_name;

        // macOS: skip tty.* variants (use cu.* only)
        #[cfg(target_os = "macos")]
        if name.starts_with("/dev/tty.") {
            return false;
        }

        // macOS: skip Bluetooth ports (cu.Bluetooth-*)
        #[cfg(target_os = "macos")]
        if name.contains("Bluetooth") {
            return false;
        }

        // Linux: skip Bluetooth rfcomm (unless you want it)
        #[cfg(target_os = "linux")]
        if matches!(p.port_type, serialport::SerialPortType::BluetoothPort) {
            return false;
        }

        true
    }).collect()
}
```

#### Tier 2: Sort by relevance

```rust
fn sort_ports(ports: &mut Vec<serialport::SerialPortInfo>) {
    ports.sort_by(|a, b| {
        // USB ports first, then PCI, then Unknown, then Bluetooth
        let type_order = |p: &serialport::SerialPortInfo| -> u8 {
            match &p.port_type {
                serialport::SerialPortType::UsbPort(_) => 0,
                serialport::SerialPortType::PciPort => 1,
                serialport::SerialPortType::Unknown => 2,
                serialport::SerialPortType::BluetoothPort => 3,
            }
        };
        type_order(a).cmp(&type_order(b))
            .then(a.port_name.cmp(&b.port_name))
    });
}
```

#### Tier 3: Rich display labels

```rust
fn port_display_label(port: &serialport::SerialPortInfo) -> String {
    match &port.port_type {
        serialport::SerialPortType::UsbPort(info) => {
            let board = identify_board(info.vid, info.pid);
            if let Some(board_name) = board {
                // Known board: "/dev/ttyACM0 - Teensy"
                format!("{} - {}", port.port_name, board_name)
            } else if let Some(product) = &info.product {
                // Unknown board but has product string: "/dev/ttyUSB0 - USB2.0-Ser!"
                format!("{} - {}", port.port_name, product)
            } else {
                // Unknown USB with VID/PID: "/dev/ttyUSB0 [1a86:7523]"
                format!("{} [{:04x}:{:04x}]", port.port_name, info.vid, info.pid)
            }
        }
        serialport::SerialPortType::PciPort => {
            format!("{} (system)", port.port_name)
        }
        serialport::SerialPortType::Unknown => {
            format!("{}", port.port_name)
        }
        serialport::SerialPortType::BluetoothPort => {
            format!("{} (Bluetooth)", port.port_name)
        }
    }
}
```

### Complete Recommended Implementation

```rust
use serialport::{SerialPortInfo, SerialPortType, UsbPortInfo};

#[derive(Clone, serde::Serialize)]
pub struct PortEntry {
    pub path: String,          // Raw path for opening
    pub label: String,         // Human-readable label
    pub port_type: String,     // "usb", "pci", "bluetooth", "unknown"
    pub board_name: Option<String>,
    pub vid_pid: Option<String>,
    pub manufacturer: Option<String>,
    pub serial_number: Option<String>,
    pub is_likely_target: bool, // Highlight probable dev boards
}

pub fn list_filtered_ports() -> Vec<PortEntry> {
    let ports = serialport::available_ports().unwrap_or_default();

    let mut entries: Vec<PortEntry> = ports.into_iter()
        .filter(|p| {
            // macOS: skip tty.* (keep cu.* only)
            #[cfg(target_os = "macos")]
            if p.port_name.starts_with("/dev/tty.") {
                return false;
            }
            // Skip Bluetooth everywhere
            if matches!(p.port_type, SerialPortType::BluetoothPort) {
                return false;
            }
            // macOS: also skip by name pattern
            #[cfg(target_os = "macos")]
            if p.port_name.contains("Bluetooth") {
                return false;
            }
            true
        })
        .map(|p| {
            let (port_type_str, board_name, vid_pid, manufacturer, serial_number, is_target) =
                match &p.port_type {
                    SerialPortType::UsbPort(info) => {
                        let board = identify_board(info.vid, info.pid);
                        (
                            "usb".to_string(),
                            board.map(|s| s.to_string()),
                            Some(format!("{:04x}:{:04x}", info.vid, info.pid)),
                            info.manufacturer.clone(),
                            info.serial_number.clone(),
                            true, // All USB serial ports are likely targets
                        )
                    }
                    SerialPortType::PciPort => {
                        ("pci".to_string(), None, None, None, None, false)
                    }
                    SerialPortType::BluetoothPort => {
                        ("bluetooth".to_string(), None, None, None, None, false)
                    }
                    SerialPortType::Unknown => {
                        ("unknown".to_string(), None, None, None, None, false)
                    }
                };

            let label = if let Some(ref board) = board_name {
                format!("{} - {}", p.port_name, board)
            } else if let Some(ref mfg) = manufacturer {
                format!("{} - {}", p.port_name, mfg)
            } else if let Some(ref vp) = vid_pid {
                format!("{} [{}]", p.port_name, vp)
            } else {
                p.port_name.clone()
            };

            PortEntry {
                path: p.port_name,
                label,
                port_type: port_type_str,
                board_name,
                vid_pid,
                manufacturer,
                serial_number,
                is_likely_target: is_target,
            }
        })
        .collect();

    // Sort: USB ports first, then by name
    entries.sort_by(|a, b| {
        b.is_likely_target.cmp(&a.is_likely_target)
            .then(a.path.cmp(&b.path))
    });

    entries
}
```

### Summary of Recommendations

| Decision | Recommendation | Rationale |
|---|---|---|
| Hide system ports? | No, show all but sort USB first | All major tools show everything. Hiding ports causes confusion when someone needs one. |
| Show VID/PID? | Yes, for USB ports | Essential for debugging "wrong port" issues. |
| Show board name? | Yes, via VID/PID lookup table | Major UX win. Already done in fc_tool. |
| Filter macOS tty.* ? | Yes, show only cu.* | tty.* blocks on open. No serial monitor tool should use tty.* for microcontrollers. |
| Filter Bluetooth? | Yes, hide by default | Flight controller context — Bluetooth serial is never relevant. |
| Auto-select port? | Only if exactly one USB port exists | Multi-board setups are common. Wrong auto-select wastes more time than manual pick. |
| Hot-plug detection? | Yes, polling every 2 seconds | Already implemented in fc_tool. This is the industry standard approach. |
| Group by type? | Optional: "Development Boards" / "Other Ports" | Nice UX but not essential. Sort order achieves similar effect. |

### Feature Flag: `usbportinfo-interface`

Consider enabling the `usbportinfo-interface` feature in Cargo.toml:

```toml
serialport = { version = "4", features = ["usbportinfo-interface"] }
```

This adds the `interface` field to `UsbPortInfo`, which helps identify which interface of a composite USB device is the serial port. Important for devices like Black Magic Probe (two serial ports on one USB device) or ESP32-S3 (CDC + JTAG).

---

## Appendix A: serialport-rs Crate Feature Flags

| Feature | Default | Effect |
|---|---|---|
| `libudev` | **Yes** (on Linux) | Uses libudev for device enumeration. Provides richer USB info. Falls back to `/sys/class/tty/` scanning without it. |
| `usbportinfo-interface` | No | Adds `interface: Option<u8>` to `UsbPortInfo`. |
| `serde` | No | Derive Serialize/Deserialize on port info types. |

For fc_tool, the current `serialport = "4"` uses defaults (libudev on Linux). The `serde` feature is not needed since fc_tool defines its own serializable structs.

## Appendix B: Quick Reference — What `available_ports()` Returns by Platform

### Linux (with Teensy + ESP32 plugged in, no physical RS-232)

```
/dev/ttyACM0  UsbPort { vid: 0x16c0, pid: 0x0483, product: "USB Serial", manufacturer: "Teensyduino" }
/dev/ttyUSB0  UsbPort { vid: 0x10c4, pid: 0xea60, product: "CP2102 USB to UART Bridge Controller" }
```

Ghost ttyS0-ttyS3 already filtered by the serial8250 open-test.

### macOS (with Teensy plugged in)

```
/dev/cu.usbmodem12345    UsbPort { vid: 0x16c0, pid: 0x0483, ... }
/dev/tty.usbmodem12345   UsbPort { vid: 0x16c0, pid: 0x0483, ... }   <-- FILTER THIS OUT
/dev/cu.Bluetooth-Incoming-Port  BluetoothPort                        <-- FILTER THIS OUT
/dev/tty.Bluetooth-Incoming-Port BluetoothPort                        <-- FILTER THIS OUT
```

### Windows (with Teensy plugged in)

```
COM3   UsbPort { vid: 0x16c0, pid: 0x0483, manufacturer: "PJRC", product: "USB Serial (COM3)" }
COM1   Unknown   <-- system port (may or may not be real hardware)
```
