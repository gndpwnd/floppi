# Board VID/PID Reference

> Last updated: 2026-02-06

This document contains USB Vendor ID (VID) and Product ID (PID) values for common microcontroller boards supported by fc_tool. These values are used for automatic board detection.

---

## Teensy Boards (PJRC)

**Vendor ID: 0x16C0** (Van Ooijen Technische Informatica)

| Board | PID (Serial Mode) | PID (Bootloader) | Notes |
|-------|-------------------|------------------|-------|
| Teensy 4.1 | 0x0483 | 0x0478 | Primary target for flight_controller |
| Teensy 4.0 | 0x0483 | 0x0478 | Same PIDs as 4.1 |
| Teensy 3.6 | 0x0483 | 0x0478 | Legacy support |
| Teensy 3.5 | 0x0483 | 0x0478 | Legacy support |
| Teensy 3.2/3.1 | 0x0483 | 0x0478 | Legacy support |
| Teensy LC | 0x0483 | 0x0478 | Legacy support |

**Detection Notes:**
- When running user code with USB Serial enabled: PID 0x0483
- When in bootloader mode (programming): PID 0x0478
- Raw HID mode: PID 0x0486
- All Teensy boards share the same VID

---

## Arduino Boards

### Official Arduino (arduino.cc)

**Vendor ID: 0x2341** (Arduino SA)

| Board | PID | Notes |
|-------|-----|-------|
| Arduino Uno R3 | 0x0043 | ATmega16U2 USB chip |
| Arduino Uno R4 Minima | 0x0069 | Native USB |
| Arduino Uno R4 WiFi | 0x1002 | Native USB |
| Arduino Mega 2560 | 0x0042 | ATmega16U2 USB chip |
| Arduino Leonardo | 0x8036 | Native USB (ATmega32U4) |
| Arduino Micro | 0x8037 | Native USB (ATmega32U4) |
| Arduino Due (Programming) | 0x003D | SAM3X8E native USB |
| Arduino Due (Native) | 0x003E | SAM3X8E native USB |
| Arduino Nano Every | 0x0058 | ATmega4809 |

### Arduino.org (Legacy)

**Vendor ID: 0x2A03** (Arduino.org - now merged back)

Some older boards may still have this VID.

---

## ESP32 Boards

ESP32 boards typically use external USB-to-Serial chips. Common configurations:

### Silicon Labs CP2102/CP2104

**Vendor ID: 0x10C4** (Silicon Labs)

| Chip | PID | Common Boards |
|------|-----|---------------|
| CP2102 | 0xEA60 | Many ESP32 DevKits |
| CP2104 | 0xEA60 | Some ESP32 modules |

### WCH CH340/CH341

**Vendor ID: 0x1A86** (Nanjing Qinheng Microelectronics)

| Chip | PID | Common Boards |
|------|-----|---------------|
| CH340 | 0x7523 | Budget ESP32 DevKits, Arduino clones |
| CH341 | 0x5523 | Some Arduino clones |

### FTDI FT232

**Vendor ID: 0x0403** (Future Technology Devices International)

| Chip | PID | Notes |
|------|-----|-------|
| FT232R | 0x6001 | Common USB-Serial chip |
| FT232H | 0x6014 | High-speed variant |
| FT2232 | 0x6010 | Dual channel |

### Espressif Native USB

**Vendor ID: 0x303A** (Espressif)

| Board/Chip | PID | Notes |
|------------|-----|-------|
| ESP32-S2 | 0x0002 | Native USB-CDC |
| ESP32-S3 | 0x1001 | Native USB-CDC |
| ESP32-C3 | 0x1001 | USB-Serial/JTAG |

---

## Clone Boards and Common USB-Serial Chips

Many clone/budget boards use these chips:

| Chip | VID | PID | Notes |
|------|-----|-----|-------|
| CH340G | 0x1A86 | 0x7523 | Very common in clones |
| CP2102 | 0x10C4 | 0xEA60 | ESP32 DevKits |
| PL2303 | 0x067B | 0x2303 | Prolific (older) |
| FT232R | 0x0403 | 0x6001 | FTDI |

---

## fc_tool Detection Strategy

### Priority Order

1. **Teensy (0x16C0:0x0483)** - Primary flight controller target
2. **Arduino Official (0x2341:*)** - Common development boards
3. **ESP32 Native (0x303A:*)** - Future ESP32 support
4. **CP2102 (0x10C4:0xEA60)** - Common ESP32 DevKits
5. **CH340 (0x1A86:0x7523)** - Clone boards, budget ESP32

### Display Names

When a known VID/PID is detected, show a friendly name:

```
/dev/ttyACM0 - Teensy 4.x
/dev/ttyUSB0 - Arduino Uno
/dev/ttyUSB1 - ESP32 (CP2102)
/dev/ttyUSB2 - CH340 Serial
```

### Unknown Devices

For unknown VID/PIDs, show:
```
/dev/ttyUSB3 - USB Serial (VID:PID)
```

---

## Linux Device Paths

| Device Type | Path Pattern | Notes |
|-------------|--------------|-------|
| ACM devices | /dev/ttyACM* | Native USB CDC (Teensy, Arduino Leonardo) |
| USB-Serial | /dev/ttyUSB* | External USB-to-Serial chips |
| Symlinks | /dev/serial/by-id/* | Persistent naming by device |

---

## References

- [USB ID Database](http://www.linux-usb.org/usb-ids.html)
- [PJRC Teensy USB IDs](https://www.pjrc.com/teensy/usb_debug_only.html)
- [Arduino USB IDs](https://github.com/arduino/ArduinoCore-avr/blob/master/boards.txt)
- [Espressif USB IDs](https://github.com/espressif/esp-idf)

---

*Update this document as new boards are tested and verified.*
