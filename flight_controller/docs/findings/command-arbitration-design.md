# Command Source Arbitration Design

> Date: 2026-02-10 (design), 2026-02-11 (implemented)
> Status: Implemented
> Files: radioComm.h, radioComm.cpp, radioComm_rc.cpp, radioComm_ext.cpp, config.h

## Summary

Command source arbitration allows multiple command sources to be active simultaneously with priority-based selection. An RC receiver (SBUS/iBUS/DSM/PPM/PWM) acts as the PRIMARY source for manual flying. Override sources (serial, I2C, WiFi) are SECONDARY -- they take priority when actively sending but fall back to the RC receiver on timeout. The design must preserve zero overhead when only one source is compiled (current behavior), and add minimal overhead when arbitration is active. All arbitration logic lives inside `getCommands()` -- the flight controller never knows the difference.

## Current Architecture

### Single-source compile-time selection

`getCommands()` uses a `#if / #elif / #endif` chain. Exactly one source compiles per build:

```
#ifdef USE_SBUS_RECEIVER
    // SBUS code
#elif defined(USE_IBUS_RECEIVER)
    // iBUS code
#elif defined(USE_SERIAL_COMMANDS)
    // serial code
#elif defined(USE_DSM_RECEIVER)
    // DSM code
#elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
    // PPM/PWM code
#elif defined(USE_ESP32) && defined(USE_WEB_SERVER)
    // WiFi-only code
#else
    #error "No receiver or command source defined!"
#endif
```

The same `#elif` chain pattern repeats in `radioSetup()`, `failSafe()`, and `isReceiverConnected()`.

### Key properties of current design

- Each source writes directly to `channel_X_pwm` globals (1000-2000us).
- Single `lastFrameTime` variable tracks the most recent valid frame.
- WiFi source already uses a separate buffer (`wifiCmdChannels[]`) with a spinlock -- this is the model for arbitration.
- All sources are mutually exclusive at compile time.

### What already exists for arbitration

- WiFi buffer pattern: separate `wifiCmdChannels[6]` + `wifiCmdTimestamp` + spinlock. This is exactly the pattern needed for every override source.
- 500ms timeout already used everywhere for failsafe.
- I2C commands (being implemented by another agent) will follow the same buffer pattern.

## Proposed Architecture

### Two source categories

| Category | Sources | Role | Config |
|----------|---------|------|--------|
| PRIMARY | SBUS, iBUS, DSM, PPM, PWM | Real-time RC receiver. Always one (or none). | `USE_*_RECEIVER` flags (mutually exclusive) |
| OVERRIDE | Serial, I2C, WiFi | Flight computer / autonomous control. Zero or more. | `USE_SERIAL_COMMANDS`, `USE_I2C_COMMANDS`, `USE_WEB_SERVER` |

### Source selection in getCommands()

```
1. Poll all compiled sources (each writes to its own buffer)
2. For each override source: is it active? (received data within timeout)
3. If any override is active, use highest-priority override
4. Else if primary RC source is available, use primary
5. Else failsafe values apply
```

Each source reads into its own local buffer (6 channels + timestamp). The arbitration logic at the end of `getCommands()` picks the winner and copies to `channel_X_pwm`.

### Override priority order

When multiple overrides are active simultaneously (unlikely but possible):

1. Serial commands (highest -- wired, lowest latency, most reliable)
2. I2C commands
3. WiFi commands (lowest -- wireless, higher latency)

This is a compile-time ordering. Not configurable by the user. If you have serial and WiFi both sending, serial wins. In practice, users won't have multiple overrides active.

## Preprocessor Strategy

### Problem

The current `#elif` chain means only one source compiles. For arbitration, the RC receiver AND override sources must compile together. But single-source builds must still work with zero overhead.

### Solution: Separate #ifdef blocks, not #elif

Change from:

```c
#ifdef USE_SBUS_RECEIVER
    // SBUS
#elif defined(USE_SERIAL_COMMANDS)
    // serial
#endif
```

To:

```c
// ---- Primary source (exactly one) ----
#ifdef USE_SBUS_RECEIVER
    // read SBUS into primary buffer
#elif defined(USE_IBUS_RECEIVER)
    // read iBUS into primary buffer
#elif defined(USE_DSM_RECEIVER)
    // read DSM into primary buffer
#elif defined(USE_PPM_RECEIVER) || defined(USE_PWM_RECEIVER)
    // read PPM/PWM into primary buffer
#endif

// ---- Override sources (zero or more, each in its own #ifdef) ----
#ifdef USE_SERIAL_COMMANDS
    // read serial into serial override buffer
#endif

#ifdef USE_I2C_COMMANDS
    // read I2C into I2C override buffer
#endif

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
    // read WiFi into WiFi override buffer
#endif

// ---- Arbitration (only when multiple sources compiled) ----
#ifdef USE_COMMAND_ARBITRATION
    // pick active source, copy winner to channel_X_pwm
#else
    // single source: direct copy (current behavior, zero overhead)
#endif
```

### Backward compatibility

- If user defines ONE receiver flag and NO override sources: the `#ifdef USE_COMMAND_ARBITRATION` block is not compiled. Behavior identical to today. Zero overhead.
- If user defines a receiver flag AND one or more override sources: must also define `USE_COMMAND_ARBITRATION`. Both primary and override blocks compile. Arbitration logic runs.
- WiFi-only (no receiver, no overrides): current behavior. No arbitration needed.

### The WiFi edge case

WiFi is currently in the `#elif` chain as a fallback when no receiver is defined. With arbitration:

- WiFi-only (no receiver): WiFi is the sole source. No arbitration. Current behavior.
- RC receiver + WiFi: WiFi becomes an override source. Arbitration active.

The WiFi block already uses the correct buffer pattern, so this just means adding WiFi to the override source list when a primary receiver is also defined.

## API / Data Structures

### Per-source command buffer

```c
struct CommandBuffer {
    uint16_t channels[6];    // 1000-2000us per channel
    uint32_t timestamp;      // millis() of last valid frame
    bool     active;         // has received at least one valid frame
};
```

Each source gets one static instance:

```c
// Primary source (RC receiver)
static CommandBuffer primaryCmd = {{1500,1500,1000,1500,1000,1000}, 0, false};

// Override sources (each #ifdef-guarded)
#ifdef USE_SERIAL_COMMANDS
static CommandBuffer serialCmd = {{1500,1500,1000,1500,1000,1000}, 0, false};
#endif

#ifdef USE_I2C_COMMANDS
static CommandBuffer i2cCmd = {{1500,1500,1000,1500,1000,1000}, 0, false};
#endif

#if defined(USE_ESP32) && defined(USE_WEB_SERVER)
// WiFi already has wifiCmdChannels[] + wifiCmdTimestamp -- wrap into CommandBuffer
static CommandBuffer wifiCmd = {{1500,1500,1000,1500,1000,1000}, 0, false};
#endif
```

### Source enum (for logging)

```c
enum CommandSource : uint8_t {
    CMD_SRC_NONE = 0,
    CMD_SRC_RC,          // Any RC receiver (SBUS/iBUS/DSM/PPM/PWM)
    CMD_SRC_SERIAL,      // USE_SERIAL_COMMANDS
    CMD_SRC_I2C,         // USE_I2C_COMMANDS
    CMD_SRC_WIFI         // USE_WEB_SERVER (as command source)
};
```

### Active source tracking

```c
static CommandSource activeSource = CMD_SRC_NONE;
static CommandSource prevSource   = CMD_SRC_NONE;
```

When `activeSource != prevSource`, log the switch to Serial:

```
[ARBITRATION] Source changed: RC -> SERIAL
[ARBITRATION] Source changed: SERIAL -> RC (override timeout)
```

### Arbitration timeout

```c
#define OVERRIDE_TIMEOUT_MS 500  // Same as existing failsafe timeout
```

An override source is considered "active" when `millis() - source.timestamp < OVERRIDE_TIMEOUT_MS`.

## Priority Logic Pseudocode

```
function getCommands():
    // Step 1: Poll all compiled sources into their buffers
    readPrimarySource(primaryCmd)      // RC receiver (one of SBUS/iBUS/DSM/PPM/PWM)
    readSerialCommands(serialCmd)      // if compiled
    readI2CCommands(i2cCmd)            // if compiled
    readWifiCommands(wifiCmd)          // if compiled

    // Step 2: Determine active source
    now = millis()
    selectedSource = CMD_SRC_NONE
    selectedBuffer = NULL

    // Check overrides in priority order (highest first)
    if USE_SERIAL_COMMANDS compiled:
        if serialCmd.active AND (now - serialCmd.timestamp < OVERRIDE_TIMEOUT_MS):
            selectedSource = CMD_SRC_SERIAL
            selectedBuffer = &serialCmd

    if selectedBuffer == NULL AND USE_I2C_COMMANDS compiled:
        if i2cCmd.active AND (now - i2cCmd.timestamp < OVERRIDE_TIMEOUT_MS):
            selectedSource = CMD_SRC_I2C
            selectedBuffer = &i2cCmd

    if selectedBuffer == NULL AND USE_WEB_SERVER compiled:
        if wifiCmd.active AND (now - wifiCmd.timestamp < OVERRIDE_TIMEOUT_MS):
            selectedSource = CMD_SRC_WIFI
            selectedBuffer = &wifiCmd

    // Fall back to primary RC source
    if selectedBuffer == NULL:
        if primaryCmd.active:
            selectedSource = CMD_SRC_RC
            selectedBuffer = &primaryCmd

    // Step 3: Copy winner to channel_X_pwm
    if selectedBuffer != NULL:
        channel_1_pwm = selectedBuffer->channels[0]
        channel_2_pwm = selectedBuffer->channels[1]
        ...
        lastFrameTime = selectedBuffer->timestamp

    // Step 4: Log source changes
    if selectedSource != prevSource:
        Serial.print("[ARBITRATION] Source changed: ")
        Serial.print(sourceName(prevSource))
        Serial.print(" -> ")
        Serial.println(sourceName(selectedSource))
        prevSource = selectedSource

    activeSource = selectedSource

    // Step 5: Constrain
    channel_X_pwm = constrain(channel_X_pwm, 1000, 2000)  // all 6 channels
```

## Failsafe Design

### Single-source failsafe (current behavior, unchanged)

One source active. Timeout = failsafe values. Simple.

### Multi-source failsafe with arbitration

Three scenarios:

| Primary RC | Override | Result |
|------------|----------|--------|
| Active | Active | Override wins. RC is ready as fallback. |
| Active | Timed out | RC resumes. Normal flight. Log the switch. |
| Timed out | Active | Override in control. RC failsafe flags are irrelevant while override is active. |
| Timed out | Timed out | **Full failsafe.** All sources lost. Apply failsafe values + LED blink. |

### Implementation in failSafe()

```c
void failSafe() {
    #ifdef USE_COMMAND_ARBITRATION
        // All sources have timed out = true failsafe
        bool allSourcesLost = true;

        // Check primary
        if (primaryCmd.active && (millis() - primaryCmd.timestamp < 500))
            allSourcesLost = false;

        // Check each override
        #ifdef USE_SERIAL_COMMANDS
        if (serialCmd.active && (millis() - serialCmd.timestamp < 500))
            allSourcesLost = false;
        #endif

        #ifdef USE_I2C_COMMANDS
        if (i2cCmd.active && (millis() - i2cCmd.timestamp < 500))
            allSourcesLost = false;
        #endif

        #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
        if (wifiCmd.active && (millis() - wifiCmd.timestamp < 500))
            allSourcesLost = false;
        #endif

        if (allSourcesLost) {
            // Apply failsafe values (same as today)
            channel_X_pwm = FAILSAFE_X;
            // Blink LED
        }

    #else
        // Single-source failsafe (current code, unchanged)
        ...
    #endif
}
```

### SBUS-specific failsafe

SBUS has hardware failsafe flags (`sbusFailSafe`, `sbusLostFrame`). In arbitration mode, these mean the primary RC source is lost, but an override source may still be active. The SBUS flags should mark `primaryCmd` as timed out, not trigger full failsafe immediately.

### isReceiverConnected() with arbitration

Returns `true` if ANY compiled source is active (not just the RC receiver). This is used by the arming system -- if at least one source is sending valid data, the FC can arm.

```c
bool isReceiverConnected() {
    #ifdef USE_COMMAND_ARBITRATION
        return (activeSource != CMD_SRC_NONE);
    #else
        // current single-source logic
        ...
    #endif
}
```

## Config Flag Design

### Explicit USE_COMMAND_ARBITRATION flag

```c
// In config.h, RECEIVER / COMMAND SOURCE SELECTION section:
#define USE_SBUS_RECEIVER        // Primary RC source
//#define USE_SERIAL_COMMANDS    // Override source: flight computer UART

// Enable arbitration when using a primary RC source + override sources.
// Without this, only one source compiles (zero overhead).
//#define USE_COMMAND_ARBITRATION
```

### Why explicit, not implicit?

Considered making arbitration automatic when multiple sources are defined. Rejected because:

1. **Accidental activation.** A user might uncomment USE_SERIAL_COMMANDS while having USE_SBUS_RECEIVER active, not realizing they need arbitration logic. Silent behavior change is dangerous on a flight controller.
2. **Compile-time validation.** With an explicit flag, we can `#error` on invalid configurations:
   ```c
   #if defined(USE_COMMAND_ARBITRATION) && !defined(USE_SBUS_RECEIVER) && !defined(USE_IBUS_RECEIVER) && ...
       #error "USE_COMMAND_ARBITRATION requires a primary RC source"
   #endif
   ```
3. **Zero overhead guarantee.** The flag makes it obvious that arbitration code is compiled in. No hidden cost.

### Compile-time validation rules

```c
// Error: arbitration needs at least one primary + one override
#if defined(USE_COMMAND_ARBITRATION)
    // Must have a primary RC source
    #if !defined(USE_SBUS_RECEIVER) && !defined(USE_IBUS_RECEIVER) && \
        !defined(USE_DSM_RECEIVER) && !defined(USE_PPM_RECEIVER) && \
        !defined(USE_PWM_RECEIVER)
        #error "USE_COMMAND_ARBITRATION requires a primary RC receiver"
    #endif
    // Must have at least one override source
    #if !defined(USE_SERIAL_COMMANDS) && !defined(USE_I2C_COMMANDS) && \
        !(defined(USE_ESP32) && defined(USE_WEB_SERVER))
        #error "USE_COMMAND_ARBITRATION requires at least one override source"
    #endif
#endif

// Error: multiple primary receivers (regardless of arbitration)
// Count defined primary sources -- only one allowed
#if (defined(USE_SBUS_RECEIVER) + defined(USE_IBUS_RECEIVER) + \
     defined(USE_DSM_RECEIVER) + defined(USE_PPM_RECEIVER) + \
     defined(USE_PWM_RECEIVER)) > 1
    #error "Only one primary RC receiver can be defined"
#endif
```

### WiFi-only builds (no change needed)

When no receiver or override is defined and `USE_WEB_SERVER` is active, WiFi is the sole source. No arbitration needed. Current behavior preserved.

## Implementation Plan

### Step 1: Introduce CommandBuffer struct and source enum (radioComm.h)

- Add `CommandBuffer` struct definition.
- Add `CommandSource` enum.
- Add `extern CommandSource activeSource` declaration (for display/telemetry modules).
- Add `OVERRIDE_TIMEOUT_MS` define.
- Keep all existing declarations untouched.

### Step 2: Refactor source reads into buffer-writing functions (radioComm.cpp)

For each source, extract the "read and parse" logic into a function that writes to a `CommandBuffer`:

- `readSBUS(CommandBuffer& buf)` -- extracted from getCommands() SBUS block
- `readIBUS(CommandBuffer& buf)` -- extracted from getCommands() iBUS block
- `readDSM(CommandBuffer& buf)` -- extracted from getCommands() DSM block
- `readPPMPWM(CommandBuffer& buf)` -- extracted from getCommands() PPM/PWM block
- `readSerialCmd(CommandBuffer& buf)` -- extracted from getCommands() serial block
- `readI2CCmd(CommandBuffer& buf)` -- new (being implemented separately)
- `readWifiCmd(CommandBuffer& buf)` -- adapted from current WiFi block

These are `static` functions, not exposed in the header. Each writes to the buffer's `channels[]` and updates `timestamp` and `active` on successful parse.

When `USE_COMMAND_ARBITRATION` is NOT defined, these functions still exist but are called directly and write to `channel_X_pwm` (inlined by compiler, zero overhead vs current code).

### Step 3: Restructure getCommands() (radioComm.cpp)

Replace the `#elif` chain with:

```c
void getCommands() {
    #ifdef USE_COMMAND_ARBITRATION
        // Poll all compiled sources
        #ifdef USE_SBUS_RECEIVER
            readSBUS(primaryCmd);
        #elif defined(USE_IBUS_RECEIVER)
            readIBUS(primaryCmd);
        // ... other primary sources
        #endif

        #ifdef USE_SERIAL_COMMANDS
            readSerialCmd(serialCmd);
        #endif
        #ifdef USE_I2C_COMMANDS
            readI2CCmd(i2cCmd);
        #endif
        #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
            readWifiCmd(wifiCmd);
        #endif

        // Arbitration logic (from pseudocode above)
        arbitrate();

    #else
        // Single-source path (current behavior, unchanged)
        #ifdef USE_SBUS_RECEIVER
            // existing SBUS code, writes directly to channel_X_pwm
        #elif ...
        #endif
    #endif

    // Constrain all channels (always runs)
    ...
}
```

**Alternative (preferred for less code duplication):** Refactor single-source path to also use the read functions, just without the arbitration step. The compiler will optimize away the intermediate buffer when there is only one source. This means we can avoid duplicating the read logic inside `#else`.

```c
void getCommands() {
    // Poll primary source
    #ifdef USE_SBUS_RECEIVER
        readSBUS(primaryCmd);
    #elif defined(USE_IBUS_RECEIVER)
        readIBUS(primaryCmd);
    #elif ...
    #endif

    #ifdef USE_COMMAND_ARBITRATION
        // Poll override sources
        #ifdef USE_SERIAL_COMMANDS
            readSerialCmd(serialCmd);
        #endif
        #ifdef USE_I2C_COMMANDS
            readI2CCmd(i2cCmd);
        #endif
        #if defined(USE_ESP32) && defined(USE_WEB_SERVER)
            readWifiCmd(wifiCmd);
        #endif

        // Pick winner
        arbitrate();
    #else
        // Single source: copy primary directly
        channel_1_pwm = primaryCmd.channels[0];
        ...
        lastFrameTime = primaryCmd.timestamp;
    #endif

    constrain(...);
}
```

### Step 4: Refactor failSafe() and isReceiverConnected() (radioComm.cpp)

Add `#ifdef USE_COMMAND_ARBITRATION` blocks with multi-source logic. Keep existing `#else` paths unchanged.

### Step 5: Refactor radioSetup() (radioComm.cpp)

Change from `#elif` to separate `#ifdef` blocks so multiple sources can initialize:

```c
void radioSetup() {
    // Primary source (exactly one)
    #ifdef USE_SBUS_RECEIVER
        sbus.begin();
        ...
    #elif defined(USE_IBUS_RECEIVER)
        ...
    #endif

    // Override sources (zero or more)
    #ifdef USE_SERIAL_COMMANDS
        #ifdef USE_COMMAND_ARBITRATION
            SERIAL_CMD_PORT.begin(115200, ...);
            Serial.println("Serial command override initialized");
        #endif
    #endif

    #ifdef USE_I2C_COMMANDS
        #ifdef USE_COMMAND_ARBITRATION
            // I2C slave init
        #endif
    #endif

    // WiFi source init stays as-is (Core 1 handles it)

    delay(100);
    lastFrameTime = millis();
}
```

### Step 6: Add config.h flag and validation (config.h)

Add `USE_COMMAND_ARBITRATION` to the RECEIVER / COMMAND SOURCE SELECTION section with documentation. Add compile-time validation `#error` checks in radioComm.h or a dedicated validation block.

### Step 7: Update config.h comments

Document that override sources now have dual behavior:
- Without `USE_COMMAND_ARBITRATION`: the override source is the ONLY source (current behavior).
- With `USE_COMMAND_ARBITRATION`: the override source supplements the primary RC receiver.

### Step 8: Serial port conflicts (important)

On Teensy, iBUS, DSM, serial commands, and I2C commands all default to Serial3 / Wire1 pins 14-17. This means:
- iBUS + serial commands = physical pin conflict (both use Serial3).
- SBUS (Serial5) + serial commands (Serial3) = no conflict. This is the expected use case.
- Any RC receiver + I2C commands = no conflict (I2C on Wire1, RC on serial or GPIO).

Add compile-time checks:

```c
#if defined(USE_COMMAND_ARBITRATION)
    // Warn about serial port conflicts on Teensy
    #if defined(USE_IBUS_RECEIVER) && defined(USE_SERIAL_COMMANDS) && !defined(USE_ESP32)
        #error "iBUS and serial commands share Serial3 on Teensy -- cannot use together"
    #endif
    #if defined(USE_DSM_RECEIVER) && defined(USE_SERIAL_COMMANDS) && !defined(USE_ESP32)
        #error "DSM and serial commands share Serial3 on Teensy -- cannot use together"
    #endif
#endif
```

On ESP32, pins are configurable so conflicts are the user's responsibility.

## Risks / Trade-offs

### Performance

- **Without arbitration:** Zero overhead. Single-source path is identical to today.
- **With arbitration:** One extra comparison per override source per loop (~3 comparisons = ~0.1us). Two buffer copies instead of direct writes (~0.3us). Total: <0.5us per loop iteration. Negligible vs 500-1000us loop budget.
- **RAM:** One `CommandBuffer` per compiled source = 16 bytes each. Max 4 sources = 64 bytes. Plus enum and tracking variables = ~70 bytes total.

### Complexity

- **Moderate.** The arbitration logic itself is simple (timestamp comparison + copy). The preprocessor restructuring is the hard part -- replacing `#elif` chains with independent `#ifdef` blocks while preserving single-source backward compatibility.
- **Risk:** The `#elif` chain appears in 4 functions (radioSetup, getCommands, failSafe, isReceiverConnected). All must be restructured consistently.

### Safety

- **Override timeout is critical.** If the override source crashes or disconnects, the FC must fall back to RC within 500ms. This is the same timeout already used for failsafe, so it is a proven value.
- **Double failure (RC lost + override lost) = full failsafe.** This is the correct behavior -- throttle to minimum, disarm. No ambiguity.
- **Source switch during flight.** When an override takes over, the transition is instantaneous (one loop iteration). Channel values may jump if the override source is commanding different attitudes. This is acceptable -- the flight computer is expected to send smooth transitions. A rate limiter on source switches could be added later if needed, but starts as out of scope.

### Testing

- **Cannot unit test on host** (embedded code with hardware dependencies). Test by compiling all valid flag combinations and checking binary sizes.
- **Recommended compile-time test matrix:**
  - SBUS only (no arbitration) -- current behavior
  - SBUS + serial commands + arbitration
  - SBUS + WiFi + arbitration (ESP32)
  - SBUS + serial + I2C + WiFi + arbitration (ESP32, all sources)
  - WiFi only (no arbitration, ESP32)
  - iBUS + serial commands + arbitration -- should `#error` on Teensy
- **Runtime testing:** requires at least two command sources connected. Verify: override takes over, override timeout returns to RC, double failure triggers failsafe, source change log messages appear.

### What this design intentionally does NOT do

- **No mixing of sources.** One source wins entirely. No blending channels from different sources (e.g., throttle from RC + attitude from WiFi). This would be a flight computer responsibility.
- **No runtime priority reconfiguration.** Priority order is compile-time fixed. Changing priority requires reflashing.
- **No heartbeat/handshake.** Override sources just send channel data. There is no "I am taking over" message. Activity = having sent data recently. Silence = releasing control.
- **No channel-level arbitration.** The winning source controls all 6 channels. Partial override (e.g., WiFi controls only throttle) is out of scope.

---

*This document describes the design only. Implementation should follow the step-by-step plan above after the I2C command source implementation is merged.*
