# Serial Telemetry Protocol

> Last updated: 2026-02-06

This document describes the serial communication protocol between flight_controller firmware and fc_tool, including supported data formats and implementation details.

---

## Overview

fc_tool uses standard **RS-232 over USB** serial communication, following the same patterns as Arduino IDE's Serial Monitor. The protocol is:

- **Transport**: USB CDC (Communications Device Class) emulating a serial port
- **Format**: ASCII text with newline-delimited messages
- **Encoding**: UTF-8 (ASCII-compatible)
- **Flow control**: None (hardware flow control disabled)
- **Default baud rate**: 115200

---

## Connection Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Baud rate | 115200 | Default; 9600-921600 supported |
| Data bits | 8 | Standard |
| Parity | None | Standard |
| Stop bits | 1 | Standard |
| Flow control | None | No RTS/CTS or XON/XOFF |

These settings match Arduino's `Serial.begin(115200)` defaults.

---

## Message Format

Each telemetry message is a **single line of ASCII text** terminated by a newline character (`\n`). This matches Arduino's `Serial.println()` behavior.

### Supported Formats

fc_tool's parser automatically detects and handles multiple data formats:

#### 1. Key-Value Format (Recommended)

```
ax=1.23 ay=4.56 az=7.89 gx=0.12 gy=0.34 gz=0.56
```

**Firmware code (Arduino/Teensy):**
```cpp
Serial.print("ax="); Serial.print(accel_x, 2);
Serial.print(" ay="); Serial.print(accel_y, 2);
Serial.print(" az="); Serial.print(accel_z, 2);
Serial.print(" gx="); Serial.print(gyro_x, 2);
Serial.print(" gy="); Serial.print(gyro_y, 2);
Serial.print(" gz="); Serial.println(gyro_z, 2);
```

Or with sprintf:
```cpp
char buf[100];
sprintf(buf, "ax=%.2f ay=%.2f az=%.2f gx=%.2f gy=%.2f gz=%.2f",
        accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);
Serial.println(buf);
```

#### 2. Compact Format

```
A:1.23,4.56,7.89 G:0.12,0.34,0.56
```

**Firmware code:**
```cpp
Serial.print("A:");
Serial.print(accel_x, 2); Serial.print(",");
Serial.print(accel_y, 2); Serial.print(",");
Serial.print(accel_z, 2);
Serial.print(" G:");
Serial.print(gyro_x, 2); Serial.print(",");
Serial.print(gyro_y, 2); Serial.print(",");
Serial.println(gyro_z, 2);
```

#### 3. CSV Format

```
1.23,4.56,7.89,0.12,0.34,0.56
```

Order: `ax,ay,az,gx,gy,gz`

**Firmware code:**
```cpp
Serial.print(accel_x, 2); Serial.print(",");
Serial.print(accel_y, 2); Serial.print(",");
Serial.print(accel_z, 2); Serial.print(",");
Serial.print(gyro_x, 2); Serial.print(",");
Serial.print(gyro_y, 2); Serial.print(",");
Serial.println(gyro_z, 2);
```

#### 4. JSON Format

```json
{"accel":[1.23,4.56,7.89],"gyro":[0.12,0.34,0.56]}
```

Or with named fields:
```json
{"ax":1.23,"ay":4.56,"az":7.89,"gx":0.12,"gy":0.34,"gz":0.56}
```

**Firmware code (using ArduinoJson):**
```cpp
#include <ArduinoJson.h>

StaticJsonDocument<128> doc;
doc["ax"] = accel_x;
doc["ay"] = accel_y;
doc["az"] = accel_z;
doc["gx"] = gyro_x;
doc["gy"] = gyro_y;
doc["gz"] = gyro_z;
serializeJson(doc, Serial);
Serial.println();
```

#### 5. Multi-Graph Format (Extended)

fc_tool supports assigning variables to different plots using the `@plotId` suffix:

```
temperature@1:25.5 humidity@1:65.2 altitude@2:100.5
```

- `temperature` and `humidity` → Plot #1
- `altitude` → Plot #2

**Key rules:**

- No `@N` suffix → default plot (plot 0)
- Only referenced plot IDs create plots (e.g., `1, 3, 11` → 3 plots, not 11)
- Plots labeled with their ID: "#1", "#3", "#11"
- Backward compatible: plain `name:value` format still works

**Firmware code:**
```cpp
// Multi-graph output
char buf[100];
sprintf(buf, "temp@1:%.1f humidity@1:%.1f alt@2:%.1f",
        temperature, humidity, altitude);
Serial.println(buf);
```

Or using a helper function:
```cpp
void plotVar(const char* name, int plotId, float value) {
  Serial.print(name);
  Serial.print("@");
  Serial.print(plotId);
  Serial.print(":");
  Serial.print(value, 2);
  Serial.print(" ");
}

void loop() {
  plotVar("temp", 1, temperature);
  plotVar("humidity", 1, humidity);
  plotVar("altitude", 2, altitude);
  Serial.println();  // End the line
}
```

**Mixed usage (backward compatible):**

```
ax:1.23 ay:4.56 az:7.89 temp@1:25.5
```

- `ax`, `ay`, `az` → Default plot (no @)
- `temp` → Plot #1

See [multi-graph-plotter-research.md](../findings/multi-graph-plotter-research.md) for design rationale.

---

## Data Units

| Measurement | Unit | Typical Range | Notes |
|-------------|------|---------------|-------|
| Accelerometer | g (gravity) | -16 to +16 | 1g ≈ 9.81 m/s² |
| Gyroscope | °/s (degrees/second) | -2000 to +2000 | Angular velocity |
| Magnetometer | µT (microtesla) | -4800 to +4800 | Future support |

---

## Timing

| Parameter | Recommended | Notes |
|-----------|-------------|-------|
| Update rate | 10-100 Hz | 50 Hz typical for IMU |
| Message interval | 10-100 ms | Avoid flooding serial buffer |
| Maximum rate | ~1000 msg/s | Limited by baud rate |

At 115200 baud with ~60 character messages, maximum throughput is approximately:
- 115200 / 10 bits per byte / 60 bytes ≈ 192 messages/second

---

## Implementation Details

### fc_tool Parser (JavaScript)

The parser in `main.js` uses regex patterns and JSON parsing:

```javascript
const IMU_PATTERNS = [
  // Key-value: ax=1.23 ay=4.56 az=7.89 gx=0.12 gy=0.34 gz=0.56
  /ax[=:]?\s*([-\d.]+).*?ay[=:]?\s*([-\d.]+).*?az[=:]?\s*([-\d.]+).*?gx[=:]?\s*([-\d.]+).*?gy[=:]?\s*([-\d.]+).*?gz[=:]?\s*([-\d.]+)/i,

  // Compact: A:1.23,4.56,7.89 G:0.12,0.34,0.56
  /A[=:]?\s*([-\d.]+)[,\s]+([-\d.]+)[,\s]+([-\d.]+).*?G[=:]?\s*([-\d.]+)[,\s]+([-\d.]+)[,\s]+([-\d.]+)/i,

  // CSV: 1.23,4.56,7.89,0.12,0.34,0.56
  /^([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)$/,
];
```

### Backend Serial (Rust)

The Rust backend uses `serialport-rs` with line-buffered reading:

```rust
let mut reader = BufReader::new(port);
let mut line = String::new();
reader.read_line(&mut line)?;
```

---

## Example Session

```
[Connected to /dev/ttyACM0 at 115200 baud]
ax=0.02 ay=-0.01 az=1.01 gx=0.5 gy=-0.3 gz=0.1
ax=0.03 ay=-0.02 az=1.00 gx=0.4 gy=-0.2 gz=0.2
ax=0.02 ay=-0.01 az=1.01 gx=0.6 gy=-0.4 gz=0.1
> status
[Device responds to commands...]
```

---

## Comparison with Arduino Serial Monitor

| Feature | Arduino IDE | fc_tool |
|---------|-------------|---------|
| Transport | USB Serial | USB Serial |
| Text format | ASCII + newline | ASCII + newline |
| Baud selection | Manual | Manual |
| Data parsing | None (raw display) | Auto IMU parsing |
| Plotting | Serial Plotter (separate) | Integrated charts |
| Multi-format | No | Yes (JSON, CSV, key-value) |

---

## References

### Arduino Serial API

- [Arduino Serial Reference](https://www.arduino.cc/reference/en/language/functions/communication/serial/) - Official Arduino documentation for Serial.print(), Serial.println(), Serial.begin()
- [Arduino Serial.print()](https://www.arduino.cc/reference/en/language/functions/communication/serial/print/) - Print function reference
- [Arduino Serial.println()](https://www.arduino.cc/reference/en/language/functions/communication/serial/println/) - Print with newline reference

### USB CDC (Communications Device Class)

- [USB CDC Specification](https://www.usb.org/document-library/class-definitions-communication-devices-12) - Official USB-IF specification for USB serial emulation
- [Teensy USB Serial](https://www.pjrc.com/teensy/td_serial.html) - PJRC documentation for Teensy USB Serial

### Serial Port Standards

- [RS-232 Wikipedia](https://en.wikipedia.org/wiki/RS-232) - Overview of serial communication standard
- [8-N-1 Format](https://en.wikipedia.org/wiki/8-N-1) - Standard 8 data bits, no parity, 1 stop bit

### Rust Serial Library

- [serialport-rs](https://docs.rs/serialport/latest/serialport/) - Cross-platform serial port library used by fc_tool backend
- [serialport-rs GitHub](https://github.com/serialport/serialport-rs) - Source repository

### ArduinoJson (for JSON format)

- [ArduinoJson](https://arduinojson.org/) - JSON library for Arduino/embedded platforms
- [ArduinoJson Tutorial](https://arduinojson.org/v6/doc/) - Documentation for JSON serialization

---

## Future Extensions

- **Magnetometer data**: `mx`, `my`, `mz` fields
- **Quaternion orientation**: `qw`, `qx`, `qy`, `qz` fields
- **Calibration parameters**: Separate message types with prefix tags
- **Binary protocol**: Higher throughput for high-rate telemetry (future)

---

*This document defines the serial protocol contract between firmware and fc_tool. Changes to the protocol should be coordinated between both projects.*
