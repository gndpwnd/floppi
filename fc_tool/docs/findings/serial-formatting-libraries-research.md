# Serial Output Formatting Libraries & Protocol Research

> Date: 2026-02-10
> Status: Research findings
> Relevance: fc_tool serial protocol design, flight_controller debug output

---

## 1. Arduino Libraries for Formatted Serial Output

### 1.1 ANSI Color / Terminal Formatting Libraries

**AnsiColor / ANSI Escape Libraries**

Several small Arduino libraries wrap ANSI escape sequences for colored Serial output:

| Library | GitHub | Notes |
|---------|--------|-------|
| `ANSIcolor` (RobTillaart) | `github.com/RobTillaart/ANSI` | Full ANSI escape code wrapper. Supports 16 colors, bold, underline, cursor positioning, screen clearing. ~2KB flash. Works with any `Stream` object. |
| `ArduinoANSI` | `github.com/khoih-prog/ArduinoANSI` | Lightweight ANSI color macros. Header-only. `ANSI_RED`, `ANSI_GREEN`, etc. |
| `TermColor` | Various forks | Simple `#define` macros for `\033[31m` style escape codes. Zero overhead when disabled. |
| `BasicTerm` | `github.com/nottwo/BasicTerm` | VT100/ANSI terminal library. Cursor control, colors, screen clearing. Designed for interactive serial terminals. |

**How ANSI escape codes work on serial:**

```cpp
// Raw escape codes (no library needed)
Serial.print("\033[31m");   // Set text color to red
Serial.print("ERROR: ");
Serial.print("\033[0m");    // Reset formatting
Serial.println("Gyro timeout");

// Using RobTillaart/ANSI library
#include <ANSI.h>
ANSI ansi(&Serial);
ansi.foreground(ansi.red);
ansi.print("ERROR: ");
ansi.normal();
ansi.println("Gyro timeout");
```

**Key consideration:** ANSI escape codes only work when the receiving terminal supports them. Arduino IDE Serial Monitor does NOT support ANSI codes (they render as garbage). PlatformIO's serial monitor renders them correctly. A custom desktop tool (like fc_tool) can choose to interpret them or strip them.

**Cost:** ANSI escape sequences add 4-8 bytes per color change (`\033[31m` = 5 bytes). For telemetry at 50Hz, this is non-trivial overhead. Best reserved for startup banners and error messages, not per-frame telemetry.

### 1.2 ArduinoLog

- **GitHub:** `github.com/thijse/Arduino-Log`
- **PlatformIO:** `thijse/ArduinoLog`
- **Purpose:** Structured logging with severity levels, printf-style formatting, output handlers

**Features:**
- Six log levels: FATAL, ERROR, WARNING, INFO, TRACE, VERBOSE
- Printf-style formatting: `Log.info("Temperature: %F degrees" CR, temp);`
- Custom output handler: redirect logs to any `Print` object (Serial, File, network)
- Compile-time level filtering: `LOG_LEVEL_INFO` compiles out all TRACE/VERBOSE calls
- Suffix handler: attach custom handler that runs after each log message (useful for adding timestamps, colors, etc.)
- Flash string support with `F()` macro
- **Size:** ~3-5KB flash, minimal RAM

**API example:**

```cpp
#include <ArduinoLog.h>

void setup() {
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_INFO, &Serial);
    Log.setPrefix(printTimestamp);   // Custom prefix handler
    Log.setSuffix(printNewline);     // Custom suffix handler

    Log.info("System initialized" CR);
    Log.warning("Battery voltage low: %F V" CR, voltage);
    Log.error("IMU communication failed, code: %d" CR, errCode);
}
```

**Custom format tokens:**
- `%s` - string (RAM)
- `%S` - string (Flash/PROGMEM)
- `%d` - integer
- `%F` - float
- `%T` - boolean ("true"/"false")
- `%t` - boolean ("t"/"f")
- `CR` - carriage return + newline

**Relevance to Floppi:** ArduinoLog is well-suited for startup messages, error reporting, and calibration output. Not ideal for high-frequency telemetry (the printf-style formatting has overhead per call). Could replace the current `Serial.print(F("========"))` banner style with structured levels. The compile-time level filtering is valuable -- disable verbose logging in production builds while keeping error logging active.

### 1.3 StreamUtils (bblanchon)

- **GitHub:** `github.com/bblanchon/ArduinoStreamUtils`
- **PlatformIO:** `bblanchon/StreamUtils`
- **Purpose:** Decorator pattern for Arduino Streams -- adds buffering, logging, counting, and other features

**Features:**
- `BufferingPrint` -- buffers output, flushes at capacity or on `flush()`. Reduces USB/UART transaction overhead.
- `WriteLoggingStream` -- mirrors all writes to a secondary stream (tee output).
- `ReadLoggingStream` -- logs all reads from a stream.
- `CountingStream` -- tracks bytes read/written.
- `StringStream` -- wraps a `String` as a `Stream` (useful for testing).
- `EepromStream` -- read/write EEPROM as a stream.
- Chainable: `BufferingPrint buffered(WriteLoggingStream(Serial, logFile), 64);`

**Buffered output example:**

```cpp
#include <StreamUtils.h>

WriteBufferingStream bufferedSerial(Serial, 64);  // 64-byte buffer

void loop() {
    // These don't hit USB/UART until buffer is full or flush() called
    bufferedSerial.print("ax=");
    bufferedSerial.print(AccX, 2);
    bufferedSerial.print(" ay=");
    bufferedSerial.print(AccY, 2);
    bufferedSerial.println();
    bufferedSerial.flush();  // Send the complete line atomically
}
```

**Relevance to Floppi:** `BufferingPrint` is directly useful. Currently, `debug.cpp` makes ~12 separate `Serial.print()` calls per telemetry line. Each call may trigger a USB transaction. Buffering the entire line and flushing once reduces USB overhead significantly. On ESP32 dual-core, buffering on Core 0 and flushing atomically prevents interleaving with Core 1 debug output.

### 1.4 Other Notable Libraries

| Library | Purpose | Notes |
|---------|---------|-------|
| `PrintEx` (`github.com/Chris--A/PrintEx`) | Extended printf for Arduino `Print` class. Adds `printf()`, `printx()` (hex), streaming operator `<<`. | Useful if you want `Serial << "ax=" << AccX << endl;` syntax. ~2KB flash. |
| `Streaming` (`github.com/janelia-arduino/Streaming`) | Operator `<<` overloads for `Print`. | Header-only. `Serial << "temp=" << temp << endl;` compiles to chained `print()` calls. Zero overhead. |
| `SerialDebug` (`github.com/JoaoLopesF/SerialDebug`) | Interactive debug console with levels, function profiling, watch variables. | Heavy (~20KB flash). Includes runtime debug commands. Overkill for bare-bones FC. |
| `CmdMessenger` (`github.com/thijse/Arduino-CmdMessenger`) | Bidirectional command protocol over serial. Field separators, command IDs, callbacks. | Text-based structured protocol. Pairs with desktop app (C#, Python). |
| `PacketSerial` (`github.com/bakercp/PacketSerial`) | COBS/SLIP framed binary packets over serial. | Robust framing, no escape character conflicts. Good for binary protocols. |
| `SerialTransfer` (`github.com/PowerBroker2/SerialTransfer`) | Struct-based binary transfer with CRC. | `txObj(myStruct)` / `rxObj(myStruct)`. Automatic packetization. |

---

## 2. Structured Serial Protocols Used by Tools

### 2.1 Arduino Serial Plotter Protocol

The Arduino IDE's Serial Plotter uses a simple text-based protocol:

**Format:**
```
[label1:]value1[separator][label2:]value2[separator]...\n
```

**Rules:**
- Each line terminated by `\n` (newline)
- Values separated by space (` `), tab (`\t`), or comma (`,`)
- Optional labels use `Label:Value` format (colon separator, no space between label and value)
- Labels must be consistent across lines (same order, same count)
- Values must be numeric (integer or float)
- X-axis is sample number (auto-incrementing), Y-axis is the value
- Rolling window of ~500 data points
- All values plotted on a single shared Y-axis (auto-scaling)
- No multi-graph support -- all variables on one plot

**Unlabeled example (3 variables):**
```
25.5 30.2 18.7\n
26.0 31.1 19.2\n
```

**Labeled example:**
```
Temperature:25.5 Humidity:30.2 Pressure:1013.25\n
```

**Important details:**
- The plotter auto-detects the number of variables from the first line
- Changing the number of variables mid-stream resets the plot
- Non-numeric lines (debug text) are ignored by the plotter
- The plotter and serial monitor share the serial port -- you cannot use both simultaneously in Arduino IDE 1.x (fixed in IDE 2.x where plotter is integrated)

**Arduino IDE 2.x improvements:**
- Plotter is built into the same window as the monitor
- Supports labeled data with color-coded legend
- Per-variable visibility toggling
- Interpolation between data points

### 2.2 PlatformIO Serial Monitor Filters

PlatformIO's `pio device monitor` supports pluggable filters that transform serial data in real-time:

**Built-in filters:**
- `colorize` -- adds ANSI colors to recognized patterns (timestamps, log levels)
- `time` -- prepends local timestamp to each line
- `log2file` -- mirrors output to a log file
- `hexlify` -- displays hex representation of bytes
- `printable` -- replaces non-printable characters
- `send_on_enter` -- buffers typed input until Enter
- `direct` -- raw passthrough (default)
- `default` -- combination of `direct` + `send_on_enter`
- `debug` -- GDB integration for debug output
- `esp32_exception_decoder` -- decodes ESP32 crash stack traces

**Usage:**
```ini
; platformio.ini
[env:esp32]
monitor_filters = time, colorize, log2file
monitor_speed = 115200
```

**Custom filters:** PlatformIO allows writing custom Python filter plugins. Each filter implements a `rx()` method that processes incoming bytes and a `tx()` method for outgoing:

```python
# custom_filter.py
from platformio.device.monitor.filters.base import DeviceMonitorFilterBase

class MyFilter(DeviceMonitorFilterBase):
    NAME = "my_filter"

    def rx(self, text):
        # Process incoming serial data
        # Parse structured data, add formatting, etc.
        return formatted_text

    def tx(self, text):
        # Process outgoing data (user input)
        return text
```

**Relevance to Floppi:** A custom PlatformIO filter could parse the fc_tool telemetry format (`ax=1.23 ay=4.56 ...`) and render it with colors, aligned columns, or sparkline graphs in the terminal -- without modifying firmware at all. This is a pure host-side enhancement. However, it only works in PlatformIO's terminal, not in fc_tool.

### 2.3 Firmata Protocol

- **Specification:** `github.com/firmata/protocol`
- **Arduino Library:** `github.com/firmata/arduino` (StandardFirmata)
- **Purpose:** Generic protocol for communicating with microcontrollers from host software

**Architecture:**
- Based on MIDI message format (7-bit data bytes, 8th bit indicates message type)
- Binary protocol over serial (not text-based)
- Host software sends commands, MCU responds with data
- MCU runs StandardFirmata firmware (generic -- not application-specific)
- Host libraries: JavaScript (firmata.js/johnny-five), Python (pyFirmata), C# (Firmata.NET), Processing

**Message format:**
```
[command byte] [data byte 0] [data byte 1] ...
```

- Command byte: bit 7 = 1 (0x80-0xFF range)
- Data bytes: bit 7 = 0 (0x00-0x7F range, 7 bits of data)
- Multi-byte values split into 7-bit chunks (LSB first)

**Key message types:**

| Command | Byte | Description |
|---------|------|-------------|
| DIGITAL_MESSAGE | 0x90-0x9F | Digital pin state (port-based) |
| ANALOG_MESSAGE | 0xE0-0xEF | Analog pin value (10-bit, 2 bytes) |
| REPORT_ANALOG | 0xC0-0xCF | Enable/disable analog reporting |
| REPORT_DIGITAL | 0xD0-0xDF | Enable/disable digital reporting |
| SET_PIN_MODE | 0xF4 | Configure pin mode |
| SYSEX_START | 0xF0 | Start of variable-length message |
| SYSEX_END | 0xF7 | End of variable-length message |
| SYSTEM_RESET | 0xFF | Reset to default state |

**SysEx messages (extensible):**
```
0xF0  [command]  [data0]  [data1]  ...  0xF7
```

Used for: I2C read/write, string messages, capability queries, pin state queries, servo config, encoder data, custom user messages.

**Strengths:**
- Well-proven, widely supported
- Self-describing (capability query tells host what MCU supports)
- Extensible via SysEx
- Works with any USB serial connection

**Weaknesses:**
- 7-bit encoding wastes ~14% bandwidth (every byte loses 1 bit)
- Designed for general-purpose I/O control, not telemetry streaming
- MCU runs generic firmware -- not suitable when MCU has its own application logic
- StandardFirmata is ~30KB flash on AVR
- Not suitable for real-time flight control (too much latency, no priority system)

**Relevance to Floppi:** Firmata's architecture (generic MCU firmware + smart host) is the opposite of Floppi's architecture (smart MCU firmware + lightweight host). However, the SysEx message concept (variable-length, typed, extensible) is a good pattern for a custom protocol. The 7-bit encoding is not worth the complexity for Floppi.

### 2.4 MAVLink Protocol

- **Specification:** `mavlink.io/en/`
- **Purpose:** Lightweight messaging protocol for drone/vehicle communication
- **Used by:** ArduPilot, PX4, QGroundControl, Mission Planner

**Packet format (MAVLink v2):**

```
Byte  Field           Size    Description
0     Magic (STX)     1       0xFD (v2) or 0xFE (v1)
1     Payload length  1       0-255 bytes
2     Incompat flags  1       Flags (signing, etc.)
3     Compat flags    1       Backward-compatible flags
4     Sequence        1       Packet sequence counter (0-255)
5     System ID       1       Sender system ID (1-255)
6     Component ID    1       Sender component ID (1-255)
7-9   Message ID      3       24-bit message type ID (v2) / 1 byte in v1
10+   Payload         N       Message-specific data
N+10  Checksum        2       CRC-16/MCRF4XX over bytes 1..N+9
N+12  Signature       13      Optional authentication (if flag set)
```

**Key design decisions:**
- **Fixed overhead:** 12 bytes per packet (v2) + 2 CRC = 14 bytes minimum
- **Message ID space:** 16.7 million message types (24-bit), but common messages use IDs 0-300
- **CRC includes schema seed:** The CRC calculation includes a "CRC_EXTRA" byte derived from the message schema. This means if sender and receiver have different message definitions, the CRC fails -- automatic version mismatch detection.
- **Little-endian** byte order for all multi-byte fields
- **No framing:** Relies on magic byte + CRC for synchronization. Lost bytes cause one dropped packet.
- **Telemetry messages are periodic:** HEARTBEAT (1Hz), ATTITUDE (configurable, typically 10-50Hz), RAW_IMU (50Hz), etc.

**Common telemetry messages:**

| Message | ID | Payload | Rate |
|---------|----|---------|------|
| HEARTBEAT | 0 | 9 bytes (mode, status, type) | 1 Hz |
| ATTITUDE | 30 | 28 bytes (roll, pitch, yaw + rates) | 10-50 Hz |
| RAW_IMU | 27 | 26 bytes (accel, gyro, mag raw) | 50 Hz |
| SCALED_IMU | 26 | 22 bytes (accel, gyro, mag scaled) | 50 Hz |
| RC_CHANNELS | 65 | 42 bytes (18 channels + RSSI) | 5 Hz |
| BATTERY_STATUS | 147 | 36 bytes | 1 Hz |

**Overhead analysis for telemetry at 50Hz:**
- Packet: 14 bytes overhead + ~28 bytes payload = 42 bytes
- At 50Hz: 42 * 50 = 2100 bytes/sec = ~17 kbits/sec
- At 115200 baud: uses ~18% of bandwidth. Comfortable.

**Code generation:** MAVLink uses XML message definitions that generate C/C++ headers. The generated code handles serialization, deserialization, CRC, and type safety. No runtime library needed -- all generated inline functions.

**Relevance to Floppi:** MAVLink is the gold standard for drone telemetry protocols. However, it is heavyweight for a bare-bones FC:
- The generated headers are ~500KB of C code (hundreds of message types)
- The protocol assumes an autopilot architecture (mission waypoints, parameter system, command protocol)
- Minimum viable MAVLink requires HEARTBEAT + SYS_STATUS + a handful of messages, but the header overhead is still significant
- The CRC_EXTRA versioning concept is excellent and worth borrowing
- The fixed packet structure with sequence numbering is a good pattern

### 2.5 MSP (MultiWii Serial Protocol)

- **Used by:** Betaflight, iNav, MultiWii, Cleanflight, KISS
- **Purpose:** Bidirectional telemetry and configuration protocol for flight controllers

**MSP v1 packet format:**

```
Byte  Field         Size  Description
0-2   Header        3     "$M" + direction ('<' = request, '>' = response, '!' = error)
3     Payload size  1     0-255 bytes
4     Command ID    1     Message type (0-255)
5+    Payload       N     Command-specific data
N+5   Checksum      1     XOR of size + command + all payload bytes
```

**Example: MSP_ATTITUDE (command 108)**
```
Request:  $M< 0x00 0x6C 0x6C          (empty payload, cmd=108)
Response: $M> 0x06 0x6C roll_L roll_H pitch_L pitch_H yaw_L yaw_H checksum
```

- Roll/pitch: int16, 1/10 degree (e.g., 150 = 15.0 degrees)
- Yaw: int16, degrees (0-359)

**MSP v2 packet format (iNav extension):**

```
Byte  Field         Size  Description
0     Header        1     '$'
1     Type          1     'X' (MSP v2)
2     Direction     1     '<' / '>' / '!'
3     Flags         1     Reserved (0)
4-5   Command ID    2     16-bit command space (0-65535)
6-7   Payload size  2     16-bit payload length
8+    Payload       N     Command-specific data
N+8   Checksum      1     CRC8-DVB-S2 over bytes 3..N+7
```

**Key MSP design decisions:**
- **Request/response model:** Host requests data, FC responds. FC does not spontaneously send telemetry (except in some implementations with "push" mode).
- **Compact:** 6 bytes overhead (v1) or 9 bytes overhead (v2). Much lighter than MAVLink.
- **Simple checksum:** XOR (v1) or CRC8 (v2). Fast to compute.
- **No sequence numbering:** Simpler but no packet loss detection.
- **Binary packed data:** Values are raw little-endian bytes. No encoding overhead.
- **Typed commands:** Each command ID has a fixed payload format. No self-describing messages.

**Common MSP commands:**

| Command | ID | Direction | Payload |
|---------|-----|-----------|---------|
| MSP_STATUS | 101 | Response | Cycle time, I2C errors, sensors, flags |
| MSP_RAW_IMU | 102 | Response | 9x int16 (accel, gyro, mag) |
| MSP_ATTITUDE | 108 | Response | 3x int16 (roll, pitch, yaw) |
| MSP_RC | 105 | Response | Nx uint16 (channel values) |
| MSP_MOTOR | 104 | Response | 8x uint16 (motor values) |
| MSP_SET_RAW_RC | 200 | Request | Nx uint16 (override RC channels) |
| MSP_SET_PID | 202 | Request | PID gains |

**Betaflight's MSP implementation details:**
- Runs on the FC's main serial port at 115200 baud
- Configurator (Betaflight Configurator, Electron app) sends requests at 10Hz
- FC processes MSP on the main loop -- each request/response takes ~50-200us
- MSP commands are handled by a large switch statement dispatching to handler functions
- Telemetry streams: some implementations support MSP "displayport" for OSD and "push" telemetry for continuous streaming without polling

**Relevance to Floppi:**
MSP is the closest existing protocol to what Floppi needs. It is:
- Lightweight (6-9 bytes overhead)
- Binary (efficient bandwidth usage)
- Well-proven in FC firmware
- Paired with a desktop configurator (Betaflight Configurator)
- Simple enough to implement on resource-constrained MCUs

However, MSP has limitations:
- Request/response model adds latency (host must poll)
- Command ID space is cluttered with legacy MultiWii messages
- No built-in versioning beyond v1/v2 header
- Payload formats are implicitly defined (no schema)

---

## 3. Arduino Library + Desktop App Pairings

### 3.1 Processing + Arduino

The canonical Arduino-to-desktop pairing. Processing is a Java-based creative coding environment with serial support.

**Pattern:**
- Arduino sends structured text data (CSV, labeled values, or custom delimiters)
- Processing reads serial, parses text, renders visualization
- No formal protocol -- each project invents its own format
- Bidirectional: Processing can send commands back

**Common approach:**
```cpp
// Arduino side
Serial.print(sensorA);
Serial.print(",");
Serial.print(sensorB);
Serial.print(",");
Serial.println(sensorC);
```

```java
// Processing side
String[] values = trim(serialLine).split(",");
float a = float(values[0]);
float b = float(values[1]);
float c = float(values[2]);
```

**Lesson:** Text CSV is the lowest-friction protocol for prototyping. But it falls apart with mixed data types, variable-length messages, and error recovery.

### 3.2 Telemetrix

- **GitHub:** `github.com/MrYsLab/telemetrix`
- **Pattern:** Python host library + Arduino firmware. Similar to Firmata but with Python-native API.
- Uses binary protocol with command/response framing.
- Supports: digital I/O, analog, I2C, SPI, DHT sensors, servos, stepper motors.
- Protocol: `[packet_length] [command_id] [pin] [data...]`
- Firmware is generic (like Firmata). Not application-specific.

### 3.3 SerialPlot (hyOzd)

- **GitHub:** `github.com/hyOzd/serialplot`
- **Qt-based desktop app** for real-time serial data visualization
- Supports multiple data formats: ASCII (CSV), binary (raw bytes), custom frame format

**Binary frame format support:**
```
[sync_byte_1] [sync_byte_2] [data_bytes...] [optional_checksum]
```

- Configurable sync bytes, data types (int8, int16, int32, float), byte order
- Supports fixed and variable frame sizes
- Multi-channel: each channel maps to a separate plot
- No Arduino library needed -- any firmware that outputs the configured format works

**Relevance:** SerialPlot demonstrates that the desktop app can be protocol-agnostic. Instead of requiring a specific Arduino library, the app is configurable to parse whatever format the firmware sends. fc_tool already follows this pattern with multi-format parsing.

### 3.4 Serial Studio

- **GitHub:** `github.com/Serial-Studio/Serial-Studio`
- **Qt-based** cross-platform serial visualization tool
- **JSON-driven configuration:** A JSON file describes the data frame format, and Serial Studio auto-generates dashboards

**Data format options:**
1. **Quick plot** -- CSV values (like Arduino Serial Plotter)
2. **JSON map** -- Firmware sends JSON frames
3. **Project file** -- External JSON config describes the binary/text frame format

**JSON frame example from firmware:**
```json
{"t":"Telemetry","g":[{"t":"IMU","d":[{"t":"AccX","v":1.23},{"t":"AccY","v":4.56}]}]}
```

**Lesson:** JSON from firmware is feature-rich but expensive. The `t`/`v`/`g`/`d` keys add ~60% overhead. Serial Studio also supports binary frames with external config -- this separates the protocol description from the data stream, which is cleaner.

### 3.5 Betaflight Configurator + Betaflight Firmware

The most relevant pairing for Floppi:

- **Desktop:** Betaflight Configurator (Electron/JavaScript app)
- **Firmware:** Betaflight FC firmware (C, runs on STM32)
- **Protocol:** MSP (see section 2.5)
- **Connection:** USB serial (typically 115200 baud)

**Architecture:**
- Configurator sends MSP requests at 10Hz for telemetry data
- FC responds with binary MSP packets
- Configuration changes (PID tuning, rates) sent as MSP SET commands
- FC stores config in EEPROM, responds with ACK
- Configurator has tabs: Setup, PID Tuning, Receiver, Motors, OSD, etc.
- Real-time data displayed on gauges, graphs, 3D model

**What makes it work well:**
- MSP is simple enough that FC overhead is minimal (~100us per request)
- Binary format keeps bandwidth low
- Request/response model means FC only sends data when asked
- Well-defined command set (each tab knows which MSP commands to request)
- Configurator handles all rendering complexity

**What is painful:**
- Adding new telemetry fields requires updating BOTH firmware AND configurator
- MSP command IDs are a flat namespace -- no grouping or versioning
- No streaming mode in standard MSP (some forks add it)

### 3.6 CmdMessenger

- **GitHub:** `github.com/thijse/Arduino-CmdMessenger`
- **Companion libraries:** C# (`CmdMessenger.CSharp`), Python, Processing

**Protocol:**
```
command_id,field1,field2,...;
```

- Text-based with configurable field separator (default `,`) and command separator (default `;`)
- Command IDs are integers
- Callback-based dispatch on both sides
- Supports: acknowledgment, timeout, retry
- Binary mode available for efficiency

**Example:**
```cpp
// Arduino
enum { kAcknowledge, kError, kSensorData };
CmdMessenger cmdMessenger = CmdMessenger(Serial);

void onSensorRequest() {
    cmdMessenger.sendCmdStart(kSensorData);
    cmdMessenger.sendCmdArg(temperature);
    cmdMessenger.sendCmdArg(humidity);
    cmdMessenger.sendCmdEnd();
}

void setup() {
    cmdMessenger.attach(kRequestSensor, onSensorRequest);
}
```

**Relevance:** CmdMessenger demonstrates a clean bidirectional command/response pattern with minimal protocol overhead. The command ID + callback model is similar to MSP but text-based. The field separator approach is more flexible than fixed binary layouts.

---

## 4. Lightweight Serial Protocol Design Patterns

### 4.1 Binary vs Text-Based Protocols

| Aspect | Text (ASCII) | Binary |
|--------|-------------|--------|
| **Bandwidth** | ~2-3x more bytes for same data | Compact (raw bytes) |
| **Human readable** | Yes -- debug with any terminal | No -- need hex viewer or parser |
| **Parsing on MCU** | `sprintf`/`sscanf` (slow, ~50-200us) | `memcpy`/struct cast (fast, ~1-5us) |
| **Parsing on host** | String split + parseFloat (easy) | Byte unpacking (slightly harder) |
| **Framing** | Newline-delimited (simple) | Sync bytes + length (more robust) |
| **Error recovery** | Skip to next newline | Skip to next sync pattern |
| **Mixed data** | All converted to text | Native types (int, float, etc.) |
| **Protocol evolution** | Add new fields at end of line | Must handle variable payload lengths |
| **Tools compatibility** | Works with any serial monitor | Requires custom parser |

**Recommendation for Floppi:**

Keep **text-based for debug/calibration output** (the current `key=value` format). It works with Arduino IDE, PlatformIO, and fc_tool. The overhead is acceptable at 20-50Hz.

Consider **binary for high-rate telemetry** if fc_tool ever needs >100Hz data or many channels. A hybrid approach is ideal:
- Normal operation: text telemetry at 50Hz (compatible with everything)
- High-rate mode: binary telemetry at 500Hz+ (requires fc_tool)
- Switchable via serial command or config flag

### 4.2 Protocol Framing Patterns

**Pattern 1: Newline-delimited text (current Floppi approach)**
```
key1=value1 key2=value2 key3=value3\n
```
- Pros: Simplest, human-readable, works everywhere
- Cons: No integrity check, can't contain newlines in data

**Pattern 2: COBS (Consistent Overhead Byte Stuffing)**
```
[COBS-encoded payload] 0x00
```
- Zero byte (0x00) is guaranteed frame delimiter
- Any payload content that would be 0x00 is transparently encoded
- Overhead: exactly 1 byte per 254 bytes of payload (~0.4%)
- Library: `PacketSerial` (Arduino), COBS implementations exist for every language
- Pros: Robust framing, minimal overhead, works with binary data
- Cons: Requires COBS encode/decode (but very fast -- ~1us per packet)

**Pattern 3: Header + Length + Checksum (MSP/MAVLink style)**
```
[sync] [length] [type] [payload...] [checksum]
```
- Pros: Self-describing, error detection, typed messages
- Cons: More overhead (4-14 bytes), sync recovery needed on corruption

**Pattern 4: SLIP (Serial Line Internet Protocol)**
```
[payload with escaped 0xC0/0xDB] 0xC0
```
- END byte (0xC0) delimits frames
- ESC byte (0xDB) escapes payload bytes that match END or ESC
- Overhead: variable (depends on payload content, typically ~1%)
- Library: `PacketSerial` supports SLIP mode
- Used by: OSC (Open Sound Control) over serial

**Recommendation:** For a binary protocol upgrade path, COBS framing via `PacketSerial` is the best fit. It is the simplest robust framing method with near-zero overhead. The library is tiny (~1KB flash) and well-tested.

### 4.3 Protocol Versioning and Backward Compatibility

**Pattern 1: Version byte in header**
```
[sync] [version] [type] [length] [payload] [checksum]
```
- Receiver checks version and uses appropriate parser
- Simple but requires maintaining multiple parser versions on host

**Pattern 2: CRC-extra (MAVLink approach)**
- Include a hash of the message schema in the checksum calculation
- If sender and receiver have different schemas, checksum fails automatically
- No explicit version field needed -- incompatible messages are silently dropped
- Elegant but makes debugging harder (silent rejection)

**Pattern 3: Type-length-value (TLV)**
```
[type:1] [length:1] [value:N] [type:1] [length:1] [value:N] ...
```
- Self-describing: receiver skips unknown types
- Forward-compatible: new fields are ignored by old receivers
- Overhead: 2 bytes per field (type + length)
- Used by: USB descriptors, network protocols, protobuf wire format

**Pattern 4: Fixed fields + extension area**
```
[fixed_header] [known_payload] [extension_length] [extension_data]
```
- Base fields never change
- New fields added in extension area
- Old receivers ignore extension data (they know the base payload size)
- MSP v2 uses this approach (flags byte indicates extensions)

**Recommendation for Floppi:**
- For text protocol: backward compatibility is trivial -- add new `key=value` pairs, old parsers ignore unknown keys
- For binary protocol (future): TLV is the safest pattern. fc_tool can add new field types over time without breaking old firmware. The 2-byte-per-field overhead is acceptable for telemetry packets.

### 4.4 Bandwidth Analysis for Floppi

**Current text telemetry (printFullTelemetry):**
```
ax=0.02 ay=-0.01 az=1.01 gx=0.5 gy=-0.3 gz=0.1 roll=1.23 pitch=-0.45 yaw=180.0 m1=1200 m2=1300 m3=1250 m4=1275\n
```
- Length: ~95 bytes per line
- At 20Hz: 95 * 20 = 1900 bytes/sec = ~15.2 kbits/sec
- At 115200 baud: uses ~13% of bandwidth. Plenty of headroom.

**Equivalent binary (packed struct):**
```c
struct TelemetryPacket {
    uint8_t  sync[2];     // 0xAA 0x55
    uint8_t  type;        // message type ID
    float    accel[3];    // 12 bytes
    float    gyro[3];     // 12 bytes
    float    attitude[3]; // 12 bytes
    uint16_t motors[4];   // 8 bytes
    uint8_t  checksum;    // 1 byte
};  // Total: 48 bytes
```
- At 20Hz: 48 * 20 = 960 bytes/sec = ~7.7 kbits/sec
- At 50Hz: 48 * 50 = 2400 bytes/sec = ~19.2 kbits/sec
- **2x more efficient** than text at the same rate
- Or: same bandwidth supports **2.5x higher update rate**

**Conclusion:** Text protocol at 20-50Hz is fine for current needs. Binary buys you headroom for higher rates or more data fields in the future, but is not urgently needed.

---

## 5. ESP32 / Teensy Specific Considerations

### 5.1 USB CDC vs UART Differences

**Teensy (USB CDC):**
- Teensy 4.0/4.1 use USB 2.0 Full Speed (12 Mbit/s)
- `Serial` is USB CDC -- NOT a UART. Baud rate parameter in `Serial.begin()` is **ignored** (USB negotiates its own speed)
- USB transfers happen in 64-byte packets (USB endpoint size)
- Multiple small `Serial.print()` calls may be coalesced into one USB packet (good)
- Or they may cause multiple USB transactions if the USB frame timing aligns badly (bad)
- `Serial.flush()` forces pending data to be sent as a USB packet
- **Implication for protocol design:** Buffer entire telemetry lines and flush once. Individual `Serial.print()` calls do NOT guarantee atomic delivery. The host may receive partial lines if the USB frame boundary falls mid-message.

**ESP32 (UART + USB):**
- ESP32 classic: `Serial` is UART0 (through USB-UART bridge chip like CP2102/CH340). True UART at configured baud rate. Each byte transmitted individually. Baud rate matters.
- ESP32-S3: Has native USB CDC (like Teensy). `Serial` can be USB CDC or UART depending on configuration. `USB_CDC_ON_BOOT=1` in platformio.ini makes `Serial` use USB CDC.
- **UART behavior:** Each `Serial.print()` call immediately starts transmitting bytes. At 115200 baud, each byte takes ~87us. A 95-byte telemetry line takes ~8.3ms to transmit. This is significant on the flight loop (2000Hz = 500us per iteration).

**Protocol implications:**
- On Teensy (USB CDC): `Serial.print()` is fast (writes to USB buffer, ~1us per call). The actual USB transfer happens asynchronously. Line buffering is nice-to-have but not critical for timing.
- On ESP32 UART: `Serial.print()` may block if the UART TX buffer is full (default 256 bytes for ESP32). For a 95-byte line at 20Hz, the buffer handles it. At 100Hz, the UART becomes the bottleneck. Options:
  - Increase TX buffer: `Serial.setTxBufferSize(512);`
  - Use DMA (ESP-IDF UART DMA mode, not exposed by Arduino API)
  - Offload to Core 1 (see below)
  - Reduce message size (binary protocol)
- On ESP32-S3 USB CDC: Same behavior as Teensy. Fast writes to USB buffer, async transfer.

### 5.2 ESP32 Dual-Core Considerations

**Current architecture (from codebase context):**
- Core 0: Flight control loop (FreeRTOS task, priority 3)
- Core 1: WiFi, web server, API server, display

**Serial output from Core 0 (current `debug.cpp`):**
- `Serial.print()` calls execute on Core 0 during the flight loop
- Each call acquires the UART mutex (ESP32 Arduino Serial is thread-safe via mutex)
- The mutex acquisition + UART write adds ~5-20us per `Serial.print()` call
- 12 calls per telemetry line = ~60-240us of flight loop time spent on serial output
- At 2000Hz loop rate (500us budget), this is 12-48% of the loop -- **significant**

**Improvement options:**

**Option A: Buffer on Core 0, send from Core 1**
```cpp
// Core 0: Write to buffer (no UART, no mutex)
char telemetryBuf[128];
int len = snprintf(telemetryBuf, sizeof(telemetryBuf),
    "ax=%.2f ay=%.2f ...", AccX, AccY, ...);
xQueueSend(telemetryQueue, telemetryBuf, 0);  // Non-blocking

// Core 1: Read from queue, send to Serial
void telemetryTask(void* param) {
    char buf[128];
    while (true) {
        if (xQueueReceive(telemetryQueue, buf, portMAX_DELAY)) {
            Serial.println(buf);
        }
    }
}
```
- Core 0 cost: one `snprintf` + one queue send = ~10-30us
- Core 1 handles all Serial I/O -- no mutex contention on flight loop
- **Best option for ESP32 dual-core**

**Option B: DMA-based UART (ESP-IDF level)**
- Use `uart_write_bytes()` with DMA ring buffer
- Write is non-blocking (copies to DMA buffer, hardware handles the rest)
- Requires ESP-IDF API, not standard Arduino Serial
- More complex but zero CPU overhead after the buffer copy

**Option C: Reduce print frequency (current approach)**
- `printFullTelemetry()` runs at 20Hz (50ms interval guard)
- This is already implemented and works, but the 12 `Serial.print()` calls still block Core 0 when they fire
- Combining into a single `snprintf` + `Serial.println(buf)` reduces from 12 mutex acquisitions to 1

**Recommendation:** Option C (single snprintf) is the immediate win with minimal code change. Option A (queue to Core 1) is the correct long-term solution and aligns with the existing dual-core architecture.

### 5.3 Memory and CPU Overhead of Protocol Libraries

| Library | Flash (bytes) | RAM (bytes) | CPU per message | Notes |
|---------|--------------|-------------|-----------------|-------|
| Raw `Serial.print()` | ~0 (built-in) | ~0 | ~5-20us per call | Current approach |
| `snprintf` buffer | ~2KB (libc) | 128B (buffer) | ~10-30us per line | Single buffer approach |
| ArduinoLog | ~3-5KB | ~100B | ~20-50us per line | Printf-style, level filtering |
| StreamUtils (buffering) | ~1-2KB | 64-256B (buffer) | ~5us per flush | Reduces USB transactions |
| PacketSerial (COBS) | ~1KB | ~256B (buffer) | ~5us encode | Binary framing |
| SerialTransfer | ~3KB | ~256B | ~10us per struct | CRC + framing |
| CmdMessenger | ~5-8KB | ~500B | ~30us per command | Text protocol + callbacks |
| MAVLink (generated) | ~50-100KB | ~2KB | ~20us per packet | Only if using full msg set |
| MAVLink (minimal) | ~10-15KB | ~500B | ~10us per packet | HEARTBEAT + few messages |
| MSP (minimal) | ~3-5KB | ~256B | ~5-10us per packet | Lightweight, proven |
| Firmata | ~30KB | ~2KB | ~50us per message | Generic firmware, heavy |

**For Teensy 4.0 (1MB flash, 512KB RAM, 600MHz):**
- Any of these libraries are trivially affordable
- CPU overhead is negligible at 600MHz
- `snprintf` of a 95-byte telemetry line: ~2us

**For ESP32 (4MB flash, 320KB RAM, 240MHz):**
- Flash: all libraries fit easily
- RAM: budget is tighter. WiFi stack uses ~50KB. Remaining ~270KB.
- CPU: 240MHz is fast enough, but flight loop is the priority
- The queue-to-Core-1 approach makes CPU overhead of formatting irrelevant to flight performance

**For Teensy 3.6 (1MB flash, 256KB RAM, 180MHz):**
- More constrained. Avoid heavy libraries (Firmata, full MAVLink)
- ArduinoLog or snprintf buffering is the sweet spot

### 5.4 Teensy-Specific USB CDC Atomicity

Teensy's USB CDC has a subtle property: `Serial.print()` writes to a 4KB transmit buffer. The USB stack sends this buffer in 64-byte USB packets asynchronously. If the transmit buffer has data when a USB SOF (Start of Frame) arrives (every 1ms at Full Speed), the pending data is sent.

This means:
- If you call `Serial.print()` 12 times within ~100us, all data likely ends up in the same USB frame -- effectively atomic from the host's perspective.
- If your telemetry calls span a USB SOF boundary (crosses a 1ms boundary), the host receives a partial line, then the rest.
- `Serial.send_now()` (Teensy-specific) forces an immediate USB packet send.

**Best practice for Teensy:**
```cpp
// Build complete line in buffer
char buf[128];
int len = snprintf(buf, sizeof(buf), "ax=%.2f ay=%.2f ...", AccX, AccY);
// Send as single write (one memcpy to USB buffer)
Serial.write(buf, len);
Serial.write('\n');
Serial.send_now();  // Force immediate USB transmission
```

This guarantees the host receives the complete line in a single USB transaction.

---

## 6. Summary and Recommendations for Floppi

### What to keep (current approach works well)
1. **Text `key=value` format** for telemetry -- compatible with everything, human-readable, already implemented
2. **Newline-delimited framing** -- simplest, works with all tools
3. **50Hz telemetry rate** -- well within bandwidth budget
4. **CALIBRATION_MODE guard** -- debug output only when needed

### Quick wins (minimal effort, clear benefit)
1. **Replace 12x `Serial.print()` with single `snprintf` + `Serial.println(buf)`** in `debug.cpp`
   - Reduces mutex contention on ESP32
   - Ensures atomic line delivery on Teensy USB CDC
   - ~30 minutes to implement
2. **Use `F()` macro consistently** (already done in most places)
3. **Consider ArduinoLog for startup/error messages** -- structured levels, compile-time filtering

### Medium-term improvements
1. **Queue telemetry to Core 1 on ESP32** -- remove serial I/O from flight loop entirely
2. **StreamUtils `BufferingPrint`** -- buffer output for atomic USB transactions without manual `snprintf`
3. **PlatformIO custom filter** -- add colors/formatting to terminal output without firmware changes

### Future protocol evolution path (if needed)
1. **Binary protocol with COBS framing** via `PacketSerial` for high-rate telemetry
2. **MSP-inspired command set** for bidirectional communication (fc_tool sending commands to FC)
3. **TLV extension fields** for forward compatibility
4. Keep text protocol as a fallback/debug mode (always available, selectable via config flag or runtime command)

### Libraries to evaluate for inclusion

| Priority | Library | Purpose | Size |
|----------|---------|---------|------|
| High | (none -- snprintf is sufficient) | Telemetry formatting | 0 |
| Medium | `ArduinoLog` | Structured debug logging | ~4KB |
| Medium | `StreamUtils` (BufferingPrint only) | Atomic serial output | ~1KB |
| Low | `PacketSerial` | Binary protocol framing (future) | ~1KB |
| Low | `SerialTransfer` | Struct-based binary transfer (future) | ~3KB |

---

## References

### Libraries
- ArduinoLog: `github.com/thijse/Arduino-Log`
- StreamUtils: `github.com/bblanchon/ArduinoStreamUtils`
- ANSI (RobTillaart): `github.com/RobTillaart/ANSI`
- PacketSerial: `github.com/bakercp/PacketSerial`
- SerialTransfer: `github.com/PowerBroker2/SerialTransfer`
- CmdMessenger: `github.com/thijse/Arduino-CmdMessenger`
- PrintEx: `github.com/Chris--A/PrintEx`
- Streaming: `github.com/janelia-arduino/Streaming`
- SerialDebug: `github.com/JoaoLopesF/SerialDebug`
- BasicTerm: `github.com/nottwo/BasicTerm`

### Protocols
- Firmata: `github.com/firmata/protocol`
- MAVLink: `mavlink.io/en/`
- MSP v1: `github.com/multiwii/multiwii-firmware` (original)
- MSP v2: `github.com/iNavFlight/inav/wiki/MSP-V2`
- COBS: Wikipedia "Consistent Overhead Byte Stuffing"
- SLIP: RFC 1055

### Tools
- Arduino Serial Plotter: `docs.arduino.cc/software/ide-v2/tutorials/ide-v2-serial-plotter`
- PlatformIO Monitor Filters: `docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html`
- Teleplot: `github.com/nesnes/teleplot`
- SerialPlot: `github.com/hyOzd/serialplot`
- Serial Studio: `github.com/Serial-Studio/Serial-Studio`
- Betaflight Configurator: `github.com/betaflight/betaflight-configurator`

### Desktop Pairings
- Processing + Arduino: `processing.org/reference/libraries/serial/`
- Telemetrix: `github.com/MrYsLab/telemetrix`
- pyFirmata: `github.com/tino/pyFirmata`

---

*This research informs protocol design decisions for fc_tool and flight_controller serial communication.*
