# ESP32 + Web API + Quad Drone Wiring Guide

Wiring reference for a WiFi-controlled quadcopter drone using ESP32 **without a physical RC receiver**. Commands are sent over WiFi from an external controller application. This is the simplest wiring configuration.

> **No RC receiver needed.** The ESP32 receives flight commands via WiFi API.
> This configuration requires a WiFi network and a controller application running on a separate computer.

---

## Wiring Diagram

```mermaid
flowchart LR
    subgraph ESP["ESP32 DevKit"]
        G21[GPIO 21 SDA]
        G22[GPIO 22 SCL]
        G23[GPIO 23]
        G19[GPIO 19]
        G25[GPIO 25]
        G26[GPIO 26]
        G27[GPIO 27]
        G14[GPIO 14]
        EGND[GND]
        EV33[3.3V]
        EVIN[VIN 5V]
    end

    subgraph IMU["MPU6050 (GY-521)"]
        ISDA[SDA]
        ISCL[SCL]
        IVCC[VCC]
        IGND[GND]
    end

    subgraph OLED["OLED Display (optional)"]
        OSDA[SDA]
        OSCL[SCL]
        OVCC[VCC]
        OGND[GND]
    end

    subgraph ESC1["ESC 1 (QWinOut 30A)"]
        E1S[Signal]
        E1G[GND]
        E1V[5V BEC]
    end

    subgraph ESC234["ESC 2-4 (QWinOut 30A)"]
        E2S[Signal x3]
        E2G[GND x3]
    end

    G21 --- ISDA
    G22 --- ISCL
    EV33 --- IVCC
    EGND --- IGND

    G23 --- OSDA
    G19 --- OSCL
    EV33 --- OVCC
    EGND --- OGND

    G25 --- E1S
    EGND --- E1G
    E1V --- EVIN

    G26 --- E2S
    G27 --- E2S
    G14 --- E2S
    EGND --- E2G
```

---

## Pin Reference (ESP32)

### Core Connections

| Component | Component Pin | ESP32 GPIO | Notes |
|-----------|---------------|------------|-------|
| MPU6050 | SDA | 21 | I2C data |
| MPU6050 | SCL | 22 | I2C clock |
| MPU6050 | VCC | 3.3V | **3.3V only! NOT 5V!** |
| MPU6050 | GND | GND | Common ground |
| OLED (optional) | SDA | 23 | Software I2C |
| OLED (optional) | SCL | 19 | Software I2C |
| OLED (optional) | VCC | 3.3V | 3.3V power |
| OLED (optional) | GND | GND | Common ground |
| Status LED | - | 2 | Built-in |

No receiver pins needed — commands arrive over WiFi.

### ESC Connections (QWinOut 30A)

| ESC | Signal GPIO | Motor Position | Rotation | BEC VCC |
|-----|-------------|----------------|----------|---------|
| ESC 1 | 25 | Front Left | CCW | **Connected** to VIN |
| ESC 2 | 26 | Front Right | CW | **Disconnected** |
| ESC 3 | 27 | Back Right | CCW | **Disconnected** |
| ESC 4 | 14 | Back Left | CW | **Disconnected** |

---

## How WiFi Control Works

```mermaid
sequenceDiagram
    participant App as Controller App (PC/laptop)
    participant ESP as ESP32 Drone
    Note over App: Web dashboard
    Note over ESP: WiFi STA mode
    App->>ESP: Send commands via HTTP (POST throttle/roll/pitch/yaw)
    Note over ESP: Web server receives
    Note over ESP: RadioComm processes channel_X_pwm values
    Note over ESP: PID → Motors
```

**Architecture:**
1. ESP32 connects to your WiFi network (STA mode)
2. Web server runs on Core 1, flight control on Core 0
3. Controller app sends commands via HTTP POST or WebSocket
4. Commands route through RadioComm (same path as RC receiver)
5. Failsafe activates if no commands received within timeout

**This is NOT a direct RC link.** WiFi has higher latency (~20-50ms) and is dependent on network quality. Suitable for:
- Indoor testing and development
- Slow/hovering flights
- Swarm coordination (multiple drones on same network)
- Autonomous flight with flight computer

**Not suitable for:**
- Fast FPV flying
- Outdoor flights beyond WiFi range
- Situations where RC failsafe is critical

---

## Optional: GPS Passthrough (`USE_GPS`)

Telemetry-only, **passthrough-only**. The ESP32 reads raw NMEA on UART1 (RX-only)
and relays the most-recent sentence over the swarm API. The flight loop never
reads GPS data — it is bytes for an external flight computer. ESP32 / ESP32-S3
only. Enable with `#define USE_GPS` in `config.h` (default OFF). See
`docs/findings/phase_w5_gps_landed_2026-05-21.md`.

```mermaid
flowchart LR
    subgraph GPS["GPS module (NEO-6M / M8N / M9N)"]
        GTX[TX]
        GRX[RX unused]
        GVCC[VCC]
        GGND[GND]
    end
    subgraph ESP["ESP32"]
        G4[GPIO 4 UART1 RX]
        EV33b[3.3V / 5V*]
        EGNDb[GND]
    end
    GTX --> G4
    EV33b --- GVCC
    EGNDb --- GGND
```

> *Only the **GPS TX → ESP32 RX** wire is connected. Passthrough sends nothing to
> the module (`GPS_PIN_TX = -1`), so the module's RX pin is left unconnected.

### GPS Pin Reference

| GPS pin | ESP32 GPIO | Notes |
| ------- | ---------- | ----- |
| TX | 4 (UART1 RX) | ESP32-S3: GPIO 16. RX-only — the only data wire. |
| RX | — | Unused (`GPS_PIN_TX = -1`); leave unconnected. |
| VCC | 3.3V or 5V | Per the module's regulator — check your breakout. |
| GND | GND | Common ground. |

- **No SBUS collision.** GPS is on UART1; SBUS is on UART2 (GPIO 16 on ESP32,
  GPIO 18 on ESP32-S3). They coexist with no override. The compile-time `#error`
  guard in `gps.h` only blocks GPS sharing UART1 with iBUS / DSM / serial-command
  receivers.
- **Active antenna.** All NEO-6M / M8N / M9N modules need an active ceramic-patch
  or helical antenna for usable cold-start. Place it with a clear sky view, away
  from the ESP32 and ESCs (RF noise lengthens time-to-fix).

> **SECURITY — position leak.** Raw NMEA contains the drone's absolute
> latitude/longitude. The swarm API has **no authentication and no TLS**, so with
> `USE_GPS` enabled anyone on the LAN (and any configured central server) can
> read the drone's exact position from the `gps.nmea` field on `/api/status`,
> `/ws`, and the outbound `/api/telemetry` POST. Run the drone on an
> isolated/trusted SSID only. This is a tracked development-phase scope decision
> — see `docs/findings/swarm_api_contract_2026-05-20.md` §8 and `scope.md`.

---

## Motor Layout (Quad X)

```text
        FRONT
    M1 (CCW)    M2 (CW)
    GPIO 25     GPIO 26
        \      /
         \    /
          \  /
          /  \
         /    \
        /      \
    M4 (CW)     M3 (CCW)
    GPIO 14     GPIO 27
        BACK
```

---

## Power Architecture

Same as the FS-iA6B ESP32 guide, minus the receiver:

```text
LiPo Battery (3S or 4S)
    |
    +-- ESC 1 (power + signal + GND) --> Motor 1
    |       |
    |       +-- BEC 5V out --> ESP32 VIN
    |
    +-- ESC 2 (power + signal + GND, NO BEC VCC) --> Motor 2
    +-- ESC 3 (power + signal + GND, NO BEC VCC) --> Motor 3
    +-- ESC 4 (power + signal + GND, NO BEC VCC) --> Motor 4
```

---

## Quick Wiring Checklist

- [ ] MPU6050 SDA -> GPIO 21
- [ ] MPU6050 SCL -> GPIO 22
- [ ] MPU6050 VCC -> 3.3V (**not 5V!**)
- [ ] MPU6050 GND -> GND
- [ ] ESC 1 signal -> GPIO 25, GND -> GND, BEC 5V -> VIN
- [ ] ESC 2 signal -> GPIO 26, GND -> GND, **red wire removed**
- [ ] ESC 3 signal -> GPIO 27, GND -> GND, **red wire removed**
- [ ] ESC 4 signal -> GPIO 14, GND -> GND, **red wire removed**
- [ ] OLED SDA -> GPIO 23 (optional)
- [ ] OLED SCL -> GPIO 19 (optional)
- [ ] WiFi credentials configured in wifi_credentials.h
- [ ] All grounds connected (common ground!)
- [ ] Props removed for initial testing!

**Config.h settings:**
```cpp
// No receiver protocol needed — commands via WiFi API
// Comment out all receiver defines:
//#define USE_SBUS_RECEIVER
//#define USE_IBUS_RECEIVER
//#define USE_PPM_RECEIVER
//#define USE_PWM_RECEIVER
```

> WiFi API command routing through RadioComm is **fully implemented** (2026-02-10). POST `/api/commands` and WebSocket `/ws` both feed into RadioComm via spinlock-protected cross-core buffer. This configuration works as the sole command source — no RC receiver needed. See `swarm_api/` for the ground station that sends commands.

---
*Verified against include/pin_definitions_esp32.h on 2026-05-20 by fc-wiring-guide-auditor@flight_controller:1. Pin assignments match HEAD; any [VERIFY] flags inline indicate open questions for hardware-side confirmation.*
