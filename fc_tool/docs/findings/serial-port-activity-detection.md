# Serial Port Activity Detection Without Exclusive Open

Research findings for enhancing fc_tool's port list to show which ports have active data,
helping users identify which port to connect to.

**Date**: 2026-02-18
**Context**: fc_tool (Tauri 2 + Rust + serialport-rs v4) already has USB hot-plug detection
(polling `serialport::available_ports()` every 2s) and VID/PID board identification.

---

## 1. Can You Detect Serial Port Activity Without Opening It?

### Linux

**Short answer: No reliable cross-platform way to detect *data activity* without opening.**

Several Linux interfaces were investigated:

**`/proc/tty/driver/serial`** exposes tx/rx byte counters for hardware UARTs (ttyS*):
```
0: uart:16550A port:000003F8 irq:4 tx:111780 rx:1321 RTS|DTR|DSR
```
However, this file only covers traditional 16550-style UARTs. USB CDC ACM devices
(`/dev/ttyACM0` -- used by Teensy, Arduino Leonardo, ESP32-S3 native USB) are handled by
the `cdc-acm` kernel driver which does **not** expose per-port statistics in `/proc/tty/driver/`.
So this approach fails for the exact devices we care about.

**`/sys/class/tty/ttyACM0/`** contains device metadata (driver symlinks, device path, etc.)
but no rx/tx byte counters or "data available" flags. The sysfs tty ABI exposes `rx_trig_bytes`
and `xmit_fifo_size` for hardware UARTs only.

**`FIONREAD` ioctl** can check bytes available in the OS receive buffer -- but requires an
open file descriptor. You must `open()` the device first.

**Opening without TIOCEXCL** (non-exclusive): On Linux, serial ports are **not** exclusive
by default. Two processes can `open()` the same `/dev/ttyACM0` simultaneously. However:
- Both processes share the same kernel tty buffer -- reads are **destructive**. If the peek
  process reads a byte, the real consumer never sees it.
- Opening a CDC ACM port can trigger DTR assertion, which reboots some boards (Arduino,
  but **not** Teensy 4.0 -- Teensy ignores DTR for reboot).
- Opening/closing rapidly degrades Teensy USB CDC (see `teensy-serial-troubleshooting.md`).

**Conclusion for Linux**: You cannot passively detect data activity on ttyACM devices
without opening the port, and opening the port has unacceptable side effects (data
consumption, potential board reset, CDC degradation).

### Windows

**Short answer: No.**

Windows COM ports require `CreateFile()` to access. There is no equivalent of `/proc` stats.

- `Win32_SerialPort` WMI class has a `StatusInfo` property, but it reflects device manager
  status (enabled/disabled), not data activity.
- `Win32_SerialPortConfiguration` has a `IsBusy` property, but this only shows if another
  process has the handle open -- not whether data is flowing.
- The `SetupDi*` APIs enumerate device properties (VID/PID, friendly name, driver) but
  provide no data-level introspection.
- Professional serial sniffers (Serial Port Monitor, etc.) use kernel filter drivers to
  intercept I/O -- far beyond what a user application should attempt.

### macOS

**Short answer: No.**

macOS serial ports (`/dev/cu.usbmodem*`) follow POSIX semantics. IOKit provides device
discovery and hotplug notifications (via `IOServiceAddMatchingNotification`), but no
data-level peek. Opening the device has the same issues as Linux (shared buffer, DTR side
effects).

### Summary Table

| Approach | Linux | Windows | macOS | Requires Open? | Side Effects |
|---|---|---|---|---|---|
| `/proc/tty/driver/` stats | ttyS only | N/A | N/A | No | None (but useless for USB) |
| `/sys/class/tty/` attributes | No activity data | N/A | N/A | No | None |
| `FIONREAD` ioctl | Yes (needs fd) | Yes (needs handle) | Yes (needs fd) | **Yes** | None if non-exclusive |
| Non-exclusive open + peek | Possible | Not reliable | Possible | **Yes** | **Destructive reads, DTR, CDC degradation** |
| WMI / IOKit metadata | N/A | Exists, no activity | Exists, no activity | No | None |

---

## 2. Non-Exclusive Port Opening in Rust

### serialport-rs (v4, currently used)

On POSIX (Linux/macOS), ports are opened **without** `TIOCEXCL` by default since v4.
Exclusive mode is opt-in:

```rust
use serialport::TTYPort;

// Open non-exclusive (default on POSIX since v4)
let port = serialport::new("/dev/ttyACM0", 115200)
    .timeout(std::time::Duration::from_millis(100))
    .open_native()  // Returns TTYPort on Linux, COMPort on Windows
    .expect("Failed to open");

// If you WANT exclusive:
// port.set_exclusive(true).expect("Failed to set exclusive");
```

On Windows, COM ports are inherently exclusive via `CreateFile`. There is no shared mode
for COM ports. `set_exclusive()` is a no-op / Linux-only.

### serial2-rs (alternative crate)

The `serial2` crate provides similar functionality. It does not set exclusive mode by
default. However, switching crates would require significant refactoring for unclear benefit.

### The Fundamental Problem with Peeking

Even with non-exclusive open, **reading bytes is destructive** -- the kernel tty layer
removes read bytes from the buffer. There is no `MSG_PEEK` equivalent for tty devices
(unlike sockets). So:

1. You open the port non-exclusively to "peek"
2. You read 1 byte to confirm data is flowing
3. That byte is now **gone** -- the real consumer (when the user later connects) will
   never see it

For a flight controller sending telemetry at 60 Hz, losing a few bytes at startup is
arguably acceptable. But the DTR/CDC side effects make this approach fragile in practice.

### Theoretical Peek via Raw File Descriptor (Linux only)

```rust
use std::os::unix::io::AsRawFd;
use std::mem::MaybeUninit;

// Check bytes available without reading them
fn bytes_available(fd: i32) -> std::io::Result<i32> {
    let mut count: MaybeUninit<i32> = MaybeUninit::uninit();
    let ret = unsafe {
        libc::ioctl(fd, libc::FIONREAD, count.as_mut_ptr())
    };
    if ret == -1 {
        Err(std::io::Error::last_os_error())
    } else {
        Ok(unsafe { count.assume_init() })
    }
}
```

This checks the kernel receive buffer size without consuming data. **But**: for CDC ACM
devices, data only accumulates in the kernel buffer after the port is opened and DTR is
asserted. If nothing has opened the port yet, the device may not even be sending
(Teensy starts sending immediately regardless of DTR, but other boards vary).

---

## 3. Alternative Approaches That Do NOT Require Opening the Port

### 3a. Track Port Recency (NEW since app startup)

The simplest and most reliable approach. The port watcher already tracks additions:

```rust
// Already in lib.rs: start_port_watcher() emits PortsChangedEvent { added, removed }

// Enhanced version: include timestamps
#[derive(Clone, Serialize)]
pub struct PortsChangedEvent {
    pub added: Vec<PortChangeInfo>,
    pub removed: Vec<String>,
}

#[derive(Clone, Serialize)]
pub struct PortChangeInfo {
    pub name: String,
    pub timestamp_ms: u64,  // when the port appeared
}
```

Frontend highlights recently-added ports with a visual indicator (pulsing dot, "NEW" badge)
that fades after 30 seconds. This tells the user: "this port just appeared, it's probably
the board you just plugged in."

**Pros**: Zero side effects. Works on all platforms. Already partially implemented.
**Cons**: Does not distinguish "board sending data" from "board plugged in but idle."

### 3b. USB VID/PID Matching (Already Implemented)

The `identify_board()` function in `lib.rs` already maps known VID/PID pairs to board
names. Current coverage:

| VID | PID | Board |
|---|---|---|
| `16C0` | `0483` | Teensy |
| `16C0` | `0478` | Teensy (bootloader) |
| `2341` | various | Arduino (Uno, Mega, Leo, Micro, Due, R4) |
| `303A` | `0002` | ESP32-S2 |
| `303A` | `1001` | ESP32-S3/C3 (native USB JTAG/serial) |
| `10C4` | `EA60` | CP2102 (ESP32 DevKit) |
| `1A86` | `7523` | CH340 (ESP32 clone boards) |
| `0403` | `6001` | FTDI FT232R |
| `067B` | `2303` | PL2303 |

This is exactly what Arduino IDE and PlatformIO do (see Section 4). Ports with recognized
board names should be visually promoted above generic/unknown ports.

### 3c. Combined "Interest Score"

Assign a numeric score to each port and sort/highlight accordingly:

```rust
fn port_interest_score(port: &serialport::SerialPortInfo, is_new: bool) -> u32 {
    let mut score = 0;

    // Known board? High priority.
    if let serialport::SerialPortType::UsbPort(ref info) = port.port_type {
        if identify_board(info.vid, info.pid).is_some() {
            score += 100;
        }
        // USB port but unknown chip? Still more interesting than PCI/Bluetooth.
        score += 10;
    }

    // Recently plugged in? Boost.
    if is_new {
        score += 50;
    }

    score
}
```

Frontend: sort port list by score descending. Top port gets auto-selected. Ports with
score >= 100 get a board icon/badge.

### 3d. USB Descriptor Deep Inspection

`serialport::available_ports()` already returns `UsbPortInfo` with:
- `vid`, `pid` (Vendor/Product ID)
- `serial_number` (unique per board -- useful for multi-board setups)
- `manufacturer` (e.g., "Teensyduino", "Espressif", "Arduino")
- `product` (e.g., "USB Serial", "Teensy USB Serial")

This metadata is available **without opening the port** on all three platforms. It comes
from USB descriptor queries (udev on Linux, SetupDi on Windows, IOKit on macOS).

```rust
// Enhanced port info for frontend
#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub port_type: String,
    pub board_name: Option<String>,
    pub manufacturer: Option<String>,
    pub serial_number: Option<String>,
    pub is_new: bool,           // appeared since app startup
    pub interest_score: u32,    // higher = more likely the user's target
}
```

### 3e. Kernel-Level Port Event Monitoring (Linux: udev)

Instead of polling `available_ports()` every 2 seconds, use udev for instant notification:

```rust
// Using the udev crate (Rust bindings to libudev)
// Cargo.toml: udev = "0.9"

use udev;

fn watch_ports_udev(callback: impl Fn(String, bool) + Send + 'static) {
    let socket = udev::MonitorBuilder::new()
        .expect("failed to create udev monitor")
        .match_subsystem("tty")
        .expect("failed to match subsystem")
        .listen()
        .expect("failed to listen");

    for event in socket.iter() {
        let action = event.action().unwrap_or_default();
        let devnode = event.devnode().map(|p| p.to_string_lossy().to_string());

        if let Some(path) = devnode {
            match action.to_str().unwrap_or("") {
                "add" => callback(path, true),
                "remove" => callback(path, false),
                _ => {}
            }
        }
    }
}
```

**Pros**: Instant detection (~0ms vs 2s polling). Can extract full USB hierarchy info.
**Cons**: Linux-only. Adds a build dependency (`libudev-dev`). The current 2-second
polling via `serialport::available_ports()` is cross-platform and already working.

**Recommendation**: Keep polling. 2 seconds is fast enough for human interaction. udev
adds complexity for marginal gain.

---

## 4. What Existing Serial Monitor Apps Do

### Arduino IDE 2.x

- **Port detection**: `serialport` library + pluggable discovery protocol
- **Board identification**: VID/PID lookup against `boards.txt` definitions from installed
  cores. Shows board name in port menu (e.g., "COM3 (Arduino Uno)")
- **No activity detection**: Does not sniff ports for data. Pure VID/PID matching.
- **Auto-select**: When a board is plugged in, the new port is auto-selected if it matches
  the currently selected board type
- **Limitation**: Boards using generic USB-to-serial chips (CH340, FT232R) show as
  "Unknown" because the VID/PID identifies the chip, not the board

### PlatformIO

- **Upload port detection**: Matches board's declared `upload_port` or HWID (VID:PID) from
  `board.json` against available ports
- **Monitor port**: Uses same auto-detection; falls back to first available port
- **No activity detection**: Pure VID/PID + port name pattern matching
- **Known issue**: Auto-detection fails when multiple serial devices are present. Users
  frequently set `upload_port` and `monitor_port` explicitly in `platformio.ini`

### CoolTerm

- **Port selection**: Manual scan + dropdown. "Rescan Serial Ports" button.
- **Auto-select**: If only one port exists, selects it. If multiple, keeps previous
  selection or picks first.
- **No activity detection**: No VID/PID identification at all. Just lists port names.

### Common Pattern

**None of these applications detect data activity.** They all rely on:
1. Port enumeration (OS-level)
2. VID/PID identification (where available)
3. Recency / user's last selection
4. Manual selection as fallback

---

## 5. Practical Recommendation

### The Answer: VID/PID + Recency Is Sufficient

Given the constraints:
- Opening a port exclusively prevents the user from connecting
- Opening non-exclusively has side effects (DTR reset, CDC degradation, destructive reads)
- No OS provides passive data-activity detection for USB CDC ACM devices
- Every major serial tool uses VID/PID matching, not activity detection

**The recommended approach is VID/PID board identification + recency highlighting.** This
matches what Arduino IDE and PlatformIO do, because it is the only approach that works
reliably across all platforms without side effects.

### Implementation Plan

**Backend changes (lib.rs):**

```rust
use std::time::Instant;
use std::collections::HashMap;

// Add to port watcher state
struct PortWatcherState {
    known_ports: Vec<String>,
    first_seen: HashMap<String, Instant>,  // port name -> when first discovered
}

#[derive(Serialize)]
pub struct SerialPortInfo {
    pub name: String,
    pub port_type: String,
    pub board_name: Option<String>,
    pub manufacturer: Option<String>,
    pub product: Option<String>,
    pub serial_number: Option<String>,
    pub is_new: bool,           // appeared after app startup
    pub seconds_since_seen: f64, // how long ago the port first appeared
}

#[tauri::command]
fn list_serial_ports(/* add state for first_seen times */) -> Result<Vec<SerialPortInfo>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    // ... existing mapping code, plus:
    // - Extract manufacturer/product/serial_number from UsbPortInfo
    // - Look up first_seen timestamp
    // - Calculate is_new (appeared after startup, within last 30s)
    // - Sort by interest score (known board + new > known board > unknown USB > PCI > BT)
    Ok(/* ... */)
}
```

**Frontend changes (connection.js):**

```javascript
// In scanPorts(), when rendering port options:
ports.forEach((p) => {
    const option = document.createElement("option");
    option.value = p.name;

    let label = p.name;
    if (p.board_name) {
        label += ` — ${p.board_name}`;
    }
    if (p.manufacturer) {
        label += ` (${p.manufacturer})`;
    }
    if (p.is_new) {
        label += " [NEW]";
        option.style.fontWeight = "bold";
    }
    option.textContent = label;
    portSelect.appendChild(option);
});

// Auto-select: prefer the newest known-board port
const best = ports.find(p => p.board_name && p.is_new)
           || ports.find(p => p.board_name)
           || ports[0];
if (best) {
    portSelect.value = best.name;
}
```

### What This Gets You

1. **Plug in Teensy** -> port appears -> labeled "Teensy" -> auto-selected -> [NEW] badge
2. **Plug in ESP32-S3** -> port appears -> labeled "ESP32-S3/C3" -> auto-selected -> [NEW]
3. **Plug in unknown board** -> port appears -> labeled by chip (e.g., "CH340") -> [NEW]
4. **System serial port** (ttyS0, COM1) -> always present -> no badge -> sorted to bottom
5. **Multiple boards** -> all labeled -> user picks from clearly-identified list

### What This Does NOT Get You (and why that is fine)

- Cannot tell if a board is actively transmitting data. But neither can Arduino IDE,
  PlatformIO, or CoolTerm. Users expect to click "Connect" and then see data.
- Cannot distinguish between a Teensy running flight_controller firmware vs. a Teensy
  running something else. The VID/PID is the same either way. Only connecting and
  reading the output can tell you what firmware is running.

---

## Sources

- [serialport-rs GitHub](https://github.com/serialport/serialport-rs) -- Rust serial port library
- [serial2-rs GitHub](https://github.com/de-vri-es/serial2-rs) -- Alternative Rust serial crate
- [serialport-rs TTYPort docs](https://docs.rs/serialport/latest/serialport/struct.TTYPort.html) -- set_exclusive API
- [Linux sysfs-tty ABI](https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-tty) -- sysfs tty attributes
- [Linux USB CDC ACM](https://michael.stapelberg.ch/posts/2021-04-27-linux-usb-virtual-serial-cdc-acm/) -- CDC ACM internals
- [Arduino pluggable discovery](https://arduino.github.io/arduino-cli/0.21/pluggable-discovery-specification/) -- How Arduino detects boards
- [PlatformIO upload_port docs](https://docs.platformio.org/en/latest/projectconf/sections/env/options/upload/upload_port.html) -- Auto-detection algorithm
- [PlatformIO VID:PID monitoring issue](https://github.com/platformio/platformio-core/issues/3349) -- Board detection discussion
- [Arduino VID/PID definitions](https://github.com/per1234/zzInoVIDPID) -- USB ID database for Arduino
- [Teensy VID/PID (16C0:0483)](https://devicehunt.com/view/type/usb/vendor/16C0/device/0483) -- PJRC Teensy USB IDs
- [Espressif USB PIDs](https://github.com/espressif/usb-pids) -- ESP32 USB ID allocations
- [udev-rs (Smithay)](https://github.com/Smithay/udev-rs) -- Rust udev bindings
- [Linux serial eavesdropping (interceptty)](https://hackaday.com/2022/09/07/linux-fu-eavesdropping-on-serial/) -- Sniffing serial ports
- [CoolTerm help](https://freeware.the-meiers.org/CoolTermHelp/) -- CoolTerm port detection
- [Win32_SerialPort WMI class](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-serialport) -- Windows serial enumeration
- [macOS IOKit serial port observation](https://khorbushko.github.io/article/2021/05/05/observe-serial-ports-on-macOS.html) -- IOKit device monitoring
