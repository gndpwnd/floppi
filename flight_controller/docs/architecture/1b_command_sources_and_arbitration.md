# Architecture — Level 1b: Command Sources & Arbitration

> Up: [Level 0 — System Overview](0_system_overview.md) ·
> Index: [architecture/](INDEX.md) ·
> Detail: [Level 2 — Arbitration buffers & timeout](2a_arbitration_buffers.md)

Every command into the flight controller — hardware radio, serial, I2C, or
WiFi — flows through **RadioComm**, the single entry point. The flight loop
only ever reads `channel_1_pwm` … `channel_6_pwm` (1000–2000 µs); it neither
knows nor cares where those values came from.

## Sources into RadioComm

```mermaid
flowchart LR
    subgraph rcprot["RC protocols (one per build)"]
        SBUS["SBUS"]
        IBUS["iBUS"]
        DSM["DSM/DSMX"]
        PPM["PPM"]
        PWM["PWM"]
    end
    subgraph ext["External command sources"]
        SER["Serial (15-byte binary frames)"]
        I2C["I2C slave (Wire1 0x42, 12-byte)"]
        WIFI["WiFi API (POST /api/commands + WS)"]
    end

    SBUS --> RADIO
    IBUS --> RADIO
    DSM --> RADIO
    PPM --> RADIO
    PWM --> RADIO
    SER --> RADIO
    I2C --> RADIO
    WIFI --> RADIO

    RADIO["RadioComm getCommands()"] --> CH["channel_1_pwm … channel_6_pwm<br/>(1000-2000 µs)"]
    CH --> FC["Flight loop"]
```

- Exactly **one RC protocol** may be compiled per build (compile-time
  `#error` enforces this).
- RC protocol code lives in `radioComm_rc.cpp`; the external sources (serial,
  I2C, WiFi) live in `radioComm_ext.cpp`; the core dispatch + arbitration is
  in `radioComm.cpp`.

## Arbitration priority

When `USE_COMMAND_ARBITRATION` is set and more than one source is active,
`getCommands()` reads **all** enabled sources into their buffers each
iteration, then selects the highest-priority *active* source:

```mermaid
flowchart TB
    START["getCommands() — read all enabled source buffers"]
    START --> Q1{"Serial buffer active?"}
    Q1 -->|yes| USE_SER["Use Serial (CMD_SRC_SERIAL)"]
    Q1 -->|no| Q2{"I2C buffer active?"}
    Q2 -->|yes| USE_I2C["Use I2C (CMD_SRC_I2C)"]
    Q2 -->|no| Q3{"WiFi buffer active?"}
    Q3 -->|yes| USE_WIFI["Use WiFi (CMD_SRC_WIFI)"]
    Q3 -->|no| USE_RC["Fall back to RC receiver (primary)"]
    USE_SER --> APPLY["Copy selected buffer → channel_X_pwm"]
    USE_I2C --> APPLY
    USE_WIFI --> APPLY
    USE_RC --> APPLY
```

The mental model: **Serial > I2C > WiFi are "override" sources** (typically a
flight computer), and **RC is the primary real-time fallback**. An override
source only wins while it is actively sending; the moment it goes silent past
the timeout, RadioComm falls back down the chain (ultimately to RC). Failsafe
applies across all sources. See
[Level 2 — Arbitration buffers & timeout](2a_arbitration_buffers.md) for the
`CommandBuffer` struct and the 500 ms liveness timeout that drives the
`active` flag in each decision node above.

## Source anchors

- Arbitration: `lib/RadioComm/radioComm.cpp` `getCommands()` (priority
  Serial > I2C > WiFi > RC), `CommandSource activeSource`.
- Buffers: `CommandBuffer` instances `rcBuffer` / `serialCmdBuffer` /
  `i2cCmdBuffer` / `wifiCmdBuffer`, `SAFE_INIT` default.
- Source readers: `readSerialCmd()`, `readI2CCmd()`, `readWifiCmd()` in
  `lib/RadioComm/radioComm_ext.cpp`; RC parsers in `radioComm_rc.cpp`.
- Flags: `include/config.h` `USE_COMMAND_ARBITRATION`,
  `USE_*_RECEIVER`, `USE_SERIAL_COMMANDS`, `USE_I2C_COMMANDS`.
