# Flight Controller Timing Requirements

> Research date: 2026-02-06

## Overview

Flight controller timing determines how responsive the aircraft is to disturbances and control inputs. This document covers loop rate requirements and latency targets for different applications.

---

## Loop Rate Standards

### Industry Evolution

| Era | Standard Loop Rate | Notes |
|-----|-------------------|-------|
| 2015 | 285Hz (3500µs) | Early Cleanflight/Betaflight |
| 2017 | 500Hz (2000µs) | Standard for most FCs |
| 2019 | 1kHz (1000µs) | Betaflight default |
| 2020+ | 2-8kHz | High-performance racing |

### Recommendations by Application

| Application | Minimum | Recommended | Maximum Benefit |
|-------------|---------|-------------|-----------------|
| Research/Education | 250Hz | 500Hz | 500Hz |
| Stable video platform | 500Hz | 1kHz | 1kHz |
| Acrobatic flight | 1kHz | 2kHz | 4kHz |
| Racing (FPV) | 2kHz | 4kHz | 8kHz |

**For university research drones**: 500Hz-1kHz is more than sufficient.

---

## Timing Budget Breakdown

### Typical 1kHz (1000µs) Loop

| Component | Time Budget | Notes |
|-----------|-------------|-------|
| IMU Read | 50-100µs | I2C ~100µs, SPI ~20µs |
| Sensor Fusion | 20-50µs | Madgwick/Mahony filter |
| PID Calculation | 10-20µs | 3-axis PID |
| Motor Mixing | 5-10µs | Mixer calculations |
| PWM/DShot Output | 10-50µs | Protocol dependent |
| **Total Processing** | ~100-230µs | Leaves headroom |
| **Idle/Buffer** | ~770-900µs | For jitter absorption |

### Why Not Always Maximum Speed?

From [Oscar Liang's research](https://oscarliang.com/best-looptime-flight-controller/):

> "As long as control loop frequency is kept at a reasonable level (around 1kHz for mini-quads), more flight performance improvement will come from lowering gyro signal delay, filters and PID tuning than from increasing this frequency."

Key insight: **Diminishing returns above 1kHz for most applications.**

---

## Latency Sources

### End-to-End Control Latency

```
Radio TX → Receiver → FC Processing → Motor Response → Propeller Response

Timeline:
├─ Radio latency:        10-25ms (2.4GHz radio system)
├─ Receiver processing:   1-5ms (SBUS/CRSF)
├─ FC loop:              0.5-2ms (at 500Hz-2kHz)
├─ Motor response:       5-15ms (ESC + motor inertia)
└─ Propeller response:   10-20ms (aerodynamic lag)
───────────────────────────────────
Total:                   27-67ms typical
```

### What FC Can Control

The flight controller only directly affects:
- **FC loop latency**: 0.5-2ms (determined by loop rate)
- **Filtering delay**: 1-10ms (gyro/D-term filters)

Other latencies are fixed by hardware choices.

---

## WiFi API Latency Target

### User's Target: <100ms (ideally <50ms)

This is for **command response**, not FC loop rate.

| Latency Component | Achievable | Notes |
|-------------------|-----------|-------|
| WiFi round-trip | 5-20ms | AP mode, local network |
| HTTP request parsing | 1-5ms | Simple REST API |
| Command processing | <1ms | Parse + validate |
| FC acknowledgment | 1-2ms | Next loop iteration |
| Response generation | 1-2ms | JSON response |
| **Total** | **10-30ms** | Well under 50ms target |

### WebSocket vs HTTP

| Protocol | Latency | Use Case |
|----------|---------|----------|
| HTTP REST | 20-50ms | Commands, config changes |
| WebSocket | 5-15ms | Real-time telemetry, low-latency commands |

**Recommendation**: Use WebSocket for real-time telemetry streaming (attitude, motor values at 10-50Hz), HTTP for infrequent commands.

---

## Jitter and Determinism

### Acceptable Jitter

| Application | Max Jitter | Notes |
|-------------|-----------|-------|
| Research drone | ±20% | e.g., 1ms ± 200µs |
| Video platform | ±10% | Smoother video |
| Racing | ±5% | Consistent feel |

### Sources of Jitter on ESP32

1. **WiFi interrupts**: Can cause 50-200µs pauses
2. **Flash access**: ~20µs when reading from flash
3. **Garbage collection**: N/A (C++, not managed)
4. **Task switching**: ~10µs with FreeRTOS

**Mitigation**: Pin FC task to dedicated core, higher priority.

---

## Gyro Sampling vs PID Rate

### Key Principle

> Gyro sampling rate must be ≥ PID loop rate

| Setup | Gyro Sample | PID Rate | Notes |
|-------|-------------|----------|-------|
| Basic | 1kHz | 500Hz | Simple, reliable |
| Matched | 1kHz | 1kHz | Common default |
| Oversampled | 8kHz | 2kHz | Better filtering |

### dRehmFlight Current Setup

```cpp
#define LOOP_FREQUENCY_HZ 2000  // 500µs loop (2kHz)
```

This is already high-performance for research applications.

---

## Recommendations for This Project

### Current State (Teensy 4.0)

- **Loop rate**: 2kHz (500µs) ✓ More than sufficient
- **IMU**: MPU6050 via I2C at 1MHz ✓
- **Processing**: Teensy 4.0 @ 600MHz ✓ Abundant headroom

### Future ESP32 Target

| Parameter | Target | Rationale |
|-----------|--------|-----------|
| FC Loop Rate | 1kHz (1ms) | Sufficient for research |
| Gyro Sample | 1kHz | Match loop rate |
| WiFi API Latency | <50ms | User requirement |
| Telemetry Rate | 50Hz | Via WebSocket |
| OLED Update | 10Hz | Minimal overhead |

### Why 1kHz (not 2kHz) for ESP32?

- ESP32 is slower than Teensy 4.0 (240MHz vs 600MHz)
- Dual-core split means FC has dedicated core
- 1kHz is proven sufficient for research drones
- Leaves headroom for other tasks on Core 0

---

## Timing Verification Code

```cpp
// Add to loop() for timing analysis
static unsigned long loopCount = 0;
static unsigned long lastReport = 0;
static unsigned long maxLoopTime = 0;
static unsigned long minLoopTime = ULONG_MAX;

unsigned long loopStart = micros();

// ... flight control code ...

unsigned long loopTime = micros() - loopStart;
maxLoopTime = max(maxLoopTime, loopTime);
minLoopTime = min(minLoopTime, loopTime);
loopCount++;

if (millis() - lastReport > 5000) {
    Serial.printf("Loops: %lu, Min: %luus, Max: %luus, Avg: %luus\n",
        loopCount, minLoopTime, maxLoopTime,
        5000000 / loopCount);  // Average in µs

    loopCount = 0;
    maxLoopTime = 0;
    minLoopTime = ULONG_MAX;
    lastReport = millis();
}
```

---

## Sources

- [Looptime and Flight Controller - Oscar Liang](https://oscarliang.com/best-looptime-flight-controller/)
- [PID Looptime - QuadMeUp](https://quadmeup.com/pid-looptime-why-it-is-not-only-about-frequency/)
- [Betaflight PID Tuning Guide](https://www.betaflight.com/docs/wiki/guides/current/PID-Tuning-Guide)
- [PX4 PID Tuning Guide](https://docs.px4.io/main/en/config_mc/pid_tuning_guide_multicopter.html)
