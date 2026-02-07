# ESP32 Flight Controller Feasibility

> Research date: 2026-02-06

## Overview

This document evaluates ESP32 as a flight controller platform, comparing it to Teensy 4.0 (current) and STM32 (industry standard).

---

## Executive Summary

| Factor | Verdict | Notes |
|--------|---------|-------|
| **Feasibility** | ✅ Yes | Multiple proven implementations exist |
| **Performance** | ⚠️ Adequate | Slower than Teensy, but sufficient for 1kHz |
| **Unique Value** | ✅ Strong | Built-in WiFi is major advantage |
| **Risk Level** | 🟡 Medium | Requires careful dual-core design |

**Bottom line**: ESP32 is viable and offers unique WiFi capabilities that justify the port.

---

## Existing ESP32 Flight Controller Projects

### 1. madflight

- **URL**: [madflight.com](https://madflight.com/)
- **Status**: Active development
- **Features**: Betaflight 4.2 compatibility, configurator support
- **Boards**: ESP32, ESP32-S3, RP2040, STM32
- **Cost**: DIY FC for ~$10

### 2. ESP-Drone (Espressif Official)

- **URL**: [github.com/espressif/esp-drone](https://github.com/espressif/esp-drone)
- **Status**: Official Espressif reference
- **Features**: Complete drone firmware, WiFi control
- **Target**: Mini drones, educational

### 3. esp-fc

- **URL**: [github.com/rtlopez/esp-fc](https://github.com/rtlopez/esp-fc)
- **Status**: Active hobbyist project
- **Features**: Betaflight-like, full FC functionality
- **Notes**: Good reference for ESP32-specific optimizations

### 4. dRehmFlight ESP32 Port

- **URL**: [github.com/goatboy29/dRehmFlight_ESP32](https://github.com/goatboy29/dRehmFlight_ESP32)
- **Status**: Community port
- **Notes**: Direct port of dRehmFlight to ESP32
- **Relevance**: Most relevant to our project

---

## Platform Comparison

### Hardware Specifications

| Feature | ESP32 | Teensy 4.0 | STM32F405 |
|---------|-------|------------|-----------|
| CPU | Xtensa LX6 | ARM Cortex-M7 | ARM Cortex-M4 |
| Cores | 2 | 1 | 1 |
| Clock | 240MHz | 600MHz | 168MHz |
| Flash | 4MB | 2MB | 1MB |
| RAM | 520KB | 1MB | 192KB |
| WiFi | ✅ Built-in | ❌ | ❌ |
| Bluetooth | ✅ Built-in | ❌ | ❌ |
| USB | ❌ (or S3) | ✅ | ✅ |
| Price | ~$5 | ~$25 | ~$10 |
| Availability | ✅ Excellent | ⚠️ Variable | ✅ Good |

### Performance Benchmarks (Estimated)

| Task | ESP32 | Teensy 4.0 | STM32F405 |
|------|-------|------------|-----------|
| Float multiply | 1.0x | 2.5x | 0.7x |
| Madgwick filter | 40µs | 15µs | 50µs |
| PID loop (3-axis) | 15µs | 5µs | 20µs |
| Max practical loop | 2kHz | 8kHz | 4kHz |

### Unique Advantages

| Platform | Unique Advantage |
|----------|-----------------|
| **ESP32** | Built-in WiFi + dual-core = FC + comms on one chip |
| **Teensy 4.0** | Raw speed, great for high loop rates |
| **STM32** | Industry standard, Betaflight compatibility |

---

## Why ESP32 Makes Sense for This Project

### User Requirements Alignment

| Requirement | ESP32 Capability | Fit |
|-------------|-----------------|-----|
| WiFi API access | Built-in WiFi | ✅ Perfect |
| <50ms command latency | Dual-core isolation | ✅ Achievable |
| OLED display | I2C available | ✅ Easy |
| Research drone | 1kHz sufficient | ✅ Adequate |
| Low cost | ~$5 per board | ✅ Excellent |
| Universal firmware | PlatformIO multi-env | ✅ Supported |

### What ESP32 Enables (That Teensy Can't)

1. **Wireless calibration**: Adjust PIDs via phone/laptop
2. **Real-time telemetry**: Stream attitude to monitoring app
3. **Fleet management**: Each drone has unique WiFi AP
4. **No USB cable needed**: Debug via WiFi serial bridge
5. **OTA updates**: Update firmware over WiFi

---

## Technical Challenges and Mitigations

### Challenge 1: Lower CPU Speed

**Problem**: ESP32 is ~2.5x slower than Teensy 4.0

**Mitigation**:
- Target 1kHz loop (not 2kHz) - sufficient for research
- Use fixed-point math where possible
- Optimize hot paths (Madgwick, PID)
- Dual-core: dedicate Core 0 entirely to FC

### Challenge 2: WiFi Interrupts

**Problem**: WiFi can cause timing jitter

**Mitigation**:
- Pin FC to Core 0, WiFi to Core 1
- Set FC task to highest priority
- Use hardware timer for precise loop timing
- Configure WiFi for low-latency mode

### Challenge 3: Memory Constraints

**Problem**: 520KB RAM (vs 1MB on Teensy)

**Mitigation**:
- Careful buffer sizing
- No dynamic allocation in flight loop
- Use `PROGMEM` for constant data
- Separate heap for each core

### Challenge 4: No USB on ESP32 (original)

**Problem**: No native USB for programming/debug

**Mitigation**:
- Use ESP32-S3 (has native USB)
- Or use external USB-UART adapter
- WiFi serial bridge once running

---

## Recommended ESP32 Variant

### ESP32-S3 (Best for FC)

| Feature | ESP32 | ESP32-S3 |
|---------|-------|----------|
| Cores | 2 | 2 |
| Clock | 240MHz | 240MHz |
| RAM | 520KB | 512KB + 8MB PSRAM option |
| USB | No | Yes (native) |
| WiFi | Yes | Yes (improved) |
| Price | ~$5 | ~$7 |

**Recommendation**: ESP32-S3 for new designs (USB + WiFi), original ESP32 for cost-sensitive.

---

## Implementation Strategy

### Phase 1: Port Validation (No WiFi)

```
Goal: Verify FC loop runs at 1kHz on ESP32
- Port dRehmFlight core to ESP32
- Single-core, no WiFi
- Validate IMU + motor control
- Measure timing and jitter
```

### Phase 2: Dual-Core Split

```
Goal: Isolate FC from system tasks
- Pin FC to Core 0
- Verify timing unaffected
- Add basic serial debug on Core 1
```

### Phase 3: WiFi Integration

```
Goal: Add wireless capabilities
- WiFi AP mode on Core 1
- HTTP REST API for commands
- WebSocket for telemetry
- Verify no FC impact
```

### Phase 4: Full Integration

```
Goal: Production-ready
- OLED display support
- OTA update capability
- Configuration storage in NVS
- Power management
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Performance insufficient | Low | High | Already proven in esp-fc, madflight |
| WiFi causes instability | Medium | Medium | Dual-core isolation |
| Development complexity | Medium | Low | Reference implementations exist |
| Hardware availability | Low | Low | ESP32 widely available |

---

## Decision Matrix

| Factor | Weight | ESP32 | Teensy 4.0 | Winner |
|--------|--------|-------|------------|--------|
| WiFi capability | 5 | 10 | 0 | ESP32 |
| Raw performance | 3 | 6 | 10 | Teensy |
| Cost | 4 | 10 | 4 | ESP32 |
| Availability | 3 | 10 | 6 | ESP32 |
| Community support | 2 | 8 | 7 | ESP32 |
| Ease of development | 2 | 7 | 9 | Teensy |
| **Weighted Total** | | **155** | **103** | **ESP32** |

---

## Conclusion

**ESP32 is recommended** for the following reasons:

1. **Built-in WiFi** aligns perfectly with API access requirement
2. **Dual-core** architecture enables clean separation of FC and comms
3. **Proven feasibility** through multiple existing projects
4. **Cost effective** at ~$5-7 per board
5. **Excellent availability** vs supply-constrained Teensy

The trade-off (lower raw speed) is acceptable given the research drone use case where 1kHz loop rate is sufficient.

---

## Sources

- [madflight](https://madflight.com/)
- [ESP-Drone - Espressif](https://github.com/espressif/esp-drone)
- [esp-fc - rtlopez](https://github.com/rtlopez/esp-fc)
- [dRehmFlight ESP32 - goatboy29](https://github.com/goatboy29/dRehmFlight_ESP32)
- [ESP32 Flight Controller - Instructables](https://www.instructables.com/ESP32-Based-Basic-Flight-Controller-for-a-Quadcopt/)
