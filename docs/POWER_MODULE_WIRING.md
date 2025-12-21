# Power Module Wiring Guide

## Overview
This document explains how to use the GM v1.0 Power Module (Pixhawk-style) to power the Teensy flight controller and ESCs from an 11.1V 3S LiPo battery.

## Critical Safety Warning

**DO NOT combine all red and black wires together!**

The power module outputs TWO different voltages:
- **11.1V** for ESCs (high current)
- **5V** for Teensy (regulated, low current)

Connecting 11.1V directly to Teensy will destroy it immediately.

## Power Module Output Identification

### Method 1: Visual Inspection
Look for these on your power module:

1. **6-pin JST connector** (small plastic connector)
   - This is the 5V output for Teensy
   - Usually labeled or has thinner wires

2. **Multiple thick red/black wire pairs**
   - These are 11.1V passthrough for ESCs
   - Thicker gauge wire for high current

### Method 2: Multimeter Test
1. Connect LiPo battery to power module INPUT
2. Use multimeter to measure voltage on each red/black pair
3. **~5V output** → Connect to Teensy
4. **~11.1V output** → Connect to ESCs

## Wiring Diagram

```
3S LiPo Battery (11.1V)
    |
    └──> Power Module GM v1.0 INPUT
         |
         ├──> ESC OUTPUTS (11.1V - thick wires)
         |    ├──> ESC 1 ──> Motor 1
         |    ├──> ESC 2 ──> Motor 2
         |    ├──> ESC 3 ──> Motor 3
         |    └──> ESC 4 ──> Motor 4
         |
         └──> 5V OUTPUT (6-pin JST or thin wires)
              |
              ├── 5V (red) ──────> Teensy VIN
              └── GND (black) ───> Teensy GND
```

## Teensy Connections

### Power (from Power Module 5V output):
- **Red wire (5V)** → Teensy **VIN** pin
- **Black wire (GND)** → Teensy **GND** pin

### ESC Control Signals (from Teensy):
- Teensy PWM pins → ESC signal wires (white/yellow)
- Connect all ESC grounds to Teensy GND (common ground)

### Sensors:
- MPU-6050 → Teensy I2C (SCL/SDA) + 3.3V + GND
- Receiver → Teensy digital pins + GND

## Pre-Flight Checklist

Before connecting Teensy:

- [ ] Verify 5V output with multimeter
- [ ] Confirm all grounds are connected together
- [ ] Double-check NO 11.1V wires touch Teensy
- [ ] Secure all connections
- [ ] Test with multimeter before applying power

## Troubleshooting

**No power to Teensy:**
- Check LiPo battery is charged
- Verify power module 5V output with multimeter
- Check wire connections

**Teensy immediately shuts down:**
- May have connected wrong voltage
- Disconnect immediately and check voltages

**Motors don't spin:**
- Verify ESCs receiving 11.1V from power module
- Check ESC signal wires connected to correct Teensy PWM pins
- Ensure ESCs are calibrated

## Specifications

- **Input:** 11.1V 3S LiPo
- **ESC Output:** 11.1V (battery voltage passthrough)
- **Teensy Output:** 5V regulated @ 2-3A
- **ESC Rating:** 30A each
- **Power Module:** GM v1.0 (Pixhawk-compatible)

## Reference

Similar power modules:
- https://www.amazon.com/Pixhawk-BEC-Helicopter-Quadcopters-Accessories/dp/B0BCJM3R5P
