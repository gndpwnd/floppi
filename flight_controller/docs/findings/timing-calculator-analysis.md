# Timing Calculator Analysis Results

> Generated: 2026-02-06 (historical — timing_calculator replaced by complexity_calculator 2026-02-10)
> Tool: `tools/complexity_calculator.py` (was `tools/timing_calculator.py`)

## Overview

This document summarizes the findings from the timing calculator tool, which uses computer science complexity analysis to estimate flight controller operation times across different platforms.

---

## Methodology

### Complexity Analysis

Each FC operation was analyzed for floating-point operation counts:

| Operation | Multiplies | Adds | Divs | Trig | Sqrt |
|-----------|------------|------|------|------|------|
| IMU Read | 6 | 6 | 0 | 0 | 0 |
| Calibration | 6 | 6 | 0 | 0 | 0 |
| LP Filter | 12 | 6 | 0 | 0 | 0 |
| Madgwick 6DOF | 80 | 60 | 0 | 3 | 2 |
| PID (3-axis) | 12 | 15 | 0 | 0 | 0 |
| Motor Mix | 16 | 12 | 0 | 0 | 0 |
| Command Scale | 6 | 6 | 0 | 0 | 0 |

### Platform Cycle Counts

Each platform has different cycle counts per operation:

| Platform | Clock | MUL | ADD | DIV | TRIG | SQRT |
|----------|-------|-----|-----|-----|------|------|
| Teensy 4.0 | 600MHz | 1 | 1 | 14 | 20 | 14 |
| ESP32 | 240MHz | 1 | 1 | 35 | 50 | 30 |
| STM32F405 | 168MHz | 1 | 1 | 14 | 25 | 14 |

---

## Key Findings

### 1. I/O Dominates Loop Time

The vast majority of loop time is spent on I/O, not computation:

| Platform | Compute Time | I/O Time | Total |
|----------|--------------|----------|-------|
| Teensy 4.0 | 0.58 µs | 110 µs | 110.58 µs |
| ESP32 (I2C) | 1.96 µs | 150 µs | 151.96 µs |
| ESP32 (SPI) | 1.96 µs | 30 µs | 31.96 µs |

**Insight**: Moving from I2C to SPI gives 5x improvement. CPU speed matters less than I/O choice.

### 2. Loop Rate Feasibility

All platforms can easily achieve 1kHz with I2C:

| Platform | @ 1kHz | @ 2kHz | @ 4kHz | @ 8kHz |
|----------|--------|--------|--------|--------|
| Teensy 4.0 | 11.1% | 22.1% | 44.2% | 88.5% |
| ESP32 (I2C) | 15.2% | 30.4% | 60.8% | NOT OK |
| ESP32 (SPI) | 3.2% | 6.4% | 12.8% | 25.6% |

**Conclusion**: ESP32 at 1kHz has ~85% headroom - very comfortable margin.

### 3. Minimum Clock Requirements

The actual CPU cycles are so low that theoretical minimum clocks are trivial:

| Target Rate | Min Clock (theoretical) |
|-------------|------------------------|
| 500 Hz | 0.3 MHz |
| 1000 Hz | 0.6 MHz |
| 2000 Hz | 1.1 MHz |

**Insight**: Any modern MCU (even 16MHz Arduino) could compute fast enough. The bottleneck is I/O.

### 4. WiFi API Latency

ESP32 WiFi round-trip analysis:

| Component | Typical |
|-----------|---------|
| WiFi round-trip | 10-15 ms |
| HTTP overhead | 2-3 ms |
| Processing | 1-2 ms |
| Response | 2-4 ms |
| **Total** | **15-25 ms** |

**Target: <50ms** - ACHIEVED with margin
**Target: <100ms** - ACHIEVED easily

### 5. Dual-Core Allocation

ESP32 at 1kHz FC loop:

| Core | Usage | Headroom |
|------|-------|----------|
| Core 0 (FC) | 15.2% | 84.8% |
| Core 1 (WiFi) | ~40% typical | ~60% |

**Conclusion**: Strong isolation between FC and WiFi. No interference expected.

---

## Recommendations Confirmed

1. **ESP32 is viable** for flight control at 1kHz (or even 2kHz)
2. **I2C is acceptable** but SPI would unlock higher rates if needed
3. **WiFi latency** is well under 50ms target
4. **Dual-core** provides excellent isolation

---

## Running the Calculator

```bash
cd flight_controller
python3 tools/complexity_calculator.py
```

The tool outputs:
- Platform specifications
- Operation complexity breakdown
- Per-platform timing analysis
- WiFi latency breakdown
- Dual-core allocation analysis
- Final recommendations
