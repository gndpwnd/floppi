# Architecture — Level 2: Arbitration Buffers & Timeout

> Up: [Level 1b — Command Sources & Arbitration](1b_command_sources_and_arbitration.md) ·
> Index: [architecture/](INDEX.md)

This is the detail behind the `active` flag that the
[Level 1b arbitration decision](1b_command_sources_and_arbitration.md) branches
on. Each command source owns one `CommandBuffer`; a 500 ms liveness timeout is
what flips a source from active to inactive so RadioComm can fall back.

## The buffer

```mermaid
classDiagram
    class CommandBuffer {
        +uint16_t channels[6]  // 1000-2000 µs
        +uint32_t timestamp    // millis() of last valid update
        +bool active           // not timed out / faulted
    }
    class CommandSource {
        <<enumeration>>
        CMD_SRC_NONE
        CMD_SRC_RC
        CMD_SRC_SERIAL
        CMD_SRC_I2C
        CMD_SRC_WIFI
    }
    CommandBuffer "1" --> "1" CommandSource : selected as activeSource
```

There is one `CommandBuffer` per source — `rcBuffer`, `serialCmdBuffer`,
`i2cCmdBuffer`, `wifiCmdBuffer` — each initialized to `SAFE_INIT`
(centered sticks, throttle low: `{1500,1500,1000,1500,1000,1000}`).

## Per-source liveness state machine

Every override source (`readSerialCmd` / `readI2CCmd` / `readWifiCmd`) runs
the same logic: a fresh frame stamps `timestamp = millis()` and sets
`active = true`; if no new data arrives within `OVERRIDE_TIMEOUT_MS` (500 ms),
the source is marked `active = false`.

```mermaid
stateDiagram-v2
    [*] --> Inactive: SAFE_INIT (centered, throttle low)
    Inactive --> Active: new valid frame (timestamp = millis(), active = true)
    Active --> Active: another valid frame (timestamp refreshed)
    Active --> Inactive: millis() - timestamp > 500 ms (active = false)
```

## How a frame becomes motor output

```mermaid
flowchart TB
    READ["getCommands(): readSerialCmd / readI2CCmd / readWifiCmd / RC parse"]
    READ --> STAMP["Each source updates its CommandBuffer (channels, timestamp, active)"]
    STAMP --> SELECT{"Pick highest-priority active source<br/>Serial &gt; I2C &gt; WiFi &gt; RC"}
    SELECT --> SET["activeSource = chosen; copy buffer.channels → channel_X_pwm"]
    SET --> FAIL{"Any source active?"}
    FAIL -->|no| FAILSAFE["Failsafe values (throttle cut / safe defaults)"]
    FAIL -->|yes| FLIGHT["Flight loop reads channel_X_pwm"]
    FAILSAFE --> FLIGHT
```

The 500 ms timeout is the safety hinge: if the controlling flight computer
crashes or the WiFi link drops, its buffer goes inactive within half a second
and RadioComm hands control back down the priority chain — to RC if present,
or to failsafe if nothing is live.

## Source anchors

- Struct + enum + timeout: `lib/RadioComm/radioComm.h`
  (`struct CommandBuffer`, `enum CommandSource`, `#define OVERRIDE_TIMEOUT_MS 500`).
- Liveness updates: `lib/RadioComm/radioComm_ext.cpp` lines around the
  `buf.timestamp = millis(); buf.active = true;` /
  `millis() - buf.timestamp > OVERRIDE_TIMEOUT_MS` blocks.
- Selection: `lib/RadioComm/radioComm.cpp` `getCommands()`,
  `SAFE_INIT`, `activeSource`.
