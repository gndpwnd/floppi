# Arduino Mega Bootloader Recovery Guide

## Quick Summary

If your Arduino Mega uploads fail with "avrdude: stk500v2_ReceiveMessage(): timeout" on Linux but worked on Windows, this guide will help you recover and upload your firmware successfully.

---

## Part 1: Understanding the Problem

### What is a Bootloader?

The **bootloader** is a small program stored in the first 4KB of your Arduino Mega's flash memory (addresses 0xF000-0xFFFF). It runs when the board powers on or resets, and its job is to:

1. Wait for incoming firmware data from your computer via the serial port
2. Accept the new firmware using the STK500v2 protocol
3. Write the firmware to the remaining flash memory
4. Jump to the user program after upload completes or a timeout

Without a bootloader, you'd need an expensive ICSP programmer to upload any code.

### Why Upload Fails: The Handshake Timeout

The upload process requires precise timing:

```
Computer Side                          Arduino Side (Bootloader)
    |                                           |
    |-- Send STK500v2 SYNC (0x30) --------->  |
    |                                      [Wait 500ms for sync]
    |  <-------- Send SYNC response (0x14) --|
    |-- Send more commands                    |
    |-- [Upload firmware in blocks]           |
    |-- [Handshake timeout = 5-10 seconds]    |
```

**Failure scenario:** The computer sends the SYNC byte but the bootloader never responds. This causes avrdude to timeout and abort the upload.

### Windows vs Linux: Serial Port Behavior

The key difference lies in **DTR (Data Terminal Ready)** and **RTS (Request To Send)** signal handling:

#### Windows Behavior
- When you open a serial port, the driver automatically manages DTR/RTS
- DTR pulse triggers the Arduino's reset circuit consistently
- Reset timing is reliable: ~50ms before upload starts
- Bootloader wakes up and is ready to receive data

#### Linux Behavior
- DTR/RTS handling depends on the USB driver (usbserial, cp210x, etc.)
- Reset timing can be delayed, unpredictable, or inconsistent
- Bootloader may not have finished initializing when first SYNC byte arrives
- Result: Bootloader misses the SYNC byte, responds too late → timeout

### The Hardware Reset Circuit

Your Arduino Mega's bootloader is triggered by the **reset pin** being pulled low. The circuit works like this:

```
USB DTR pin ----[1μF capacitor]----+---- Arduino RESET pin
                                   |
                                [10kΩ resistor to +5V]
                                   |
                                  GND
```

When DTR goes LOW → capacitor charges through reset to GND → reset pulse
When DTR goes HIGH → capacitor discharges → reset releases

**Problem on Linux:** If DTR doesn't toggle reliably, the capacitor charge state becomes unpredictable, and reset timing fails.

---

## Part 2: Troubleshooting Checklist

### Step 1: Verify the Bootloader is Actually Present

Before trying recovery steps, check that the bootloader hasn't been erased:

```bash
# Attempt upload with maximum verbosity
pio run -e arduino_mega -t upload -v

# Look for these specific errors:
# - "avrdude: stk500v2_ReceiveMessage(): timeout"   → Bootloader not responding
# - "device signature = 0x000000"                    → Bootloader erased (flash is blank)
# - "device signature = 0x1e9801"                    → Correct! Mega2560 detected
```

If you see `signature = 0x000000`, your bootloader is erased. Skip to "Bootloader Re-flash via ICSP" section.

### Step 2: Hardware Diagnostics

```bash
# Check if the USB port is detected
lsusb | grep Arduino
# Expected output: "Arduino LLC Arduino Mega 2560" or similar

# Verify serial port is available
ls -la /dev/ttyACM* /dev/ttyUSB*
# Expected: /dev/ttyACM0 or /dev/ttyUSB0

# Monitor reset pulses (optional, needs oscilloscope/logic analyzer)
# Watch DTR line while uploading - should see ~100-200ms LOW pulse
```

### Step 3: Check Bootloader Timeout With Verbose Output

Create a test script to capture verbose output:

```bash
# From auto_orientation directory
pio run -e arduino_mega -t upload -v 2>&1 | tee upload_verbose.log

# Analyze the log for timing information
# Look for patterns like:
# - "Sending SYNC message" (sent by avrdude)
# - "avrdude: stk500v2_ReceiveMessage(): timeout" (no response from bootloader)
# - "got packet" (successful response from bootloader)
```

---

## Part 3: Recovery Procedures

### Option 1: Press Reset Button During Upload (Simplest)

**When to use:** Bootloader is present but timing is off

**Procedure:**
1. Start the upload: `pio run -e arduino_mega -t upload`
2. Watch the terminal for "Uploading..." message
3. **As soon as you see "Uploading..."**, press and **hold the reset button** on the Mega
4. Release the button when you see "Received signature" in the output
5. Upload should complete successfully

**Why this works:** Manual reset gives you precise control over timing. You synchronize the bootloader startup with avrdude's handshake attempt.

**Success indicators:**
```
Uploading .pio/build/arduino_mega/firmware.hex
Connecting to programmer...
Found programmer
     programmer hardware version: 4
     programmer firmware version: 6.2
     programmer software version: 1.4
Transmitting: 0x30 0x20 0x53 0x54 0x4b 0x35 0x30 0x30 0x32 0x1e     [Bootloader responding!]
Received signature: 0x1e 0x98 0x01
     processor signature: 0x1e 0x98 0x01
Uploading: ....................
```

### Option 2: Unplug and Re-plug USB (Power Cycle)

**When to use:** DTR signal isn't triggering properly, or port state is corrupted

**Procedure:**
1. Unplug the USB cable from the Mega
2. Wait 2 seconds
3. Run: `pio run -e arduino_mega -t upload`
4. Wait until you see "Uploading..." message (or "Connecting to programmer...")
5. **Immediately plug the USB cable back in** (timing: within 1-2 seconds)
6. Upload should proceed

**Why this works:** Unplug clears the USB state and device driver state. Replugging during upload window synchronizes bootloader startup with avrdude's request.

**Timing is critical:** If you plug in too late, avrdude times out. If too early, bootloader hasn't started waiting.

### Option 3: Try Different USB Port

**When to use:** One USB port behaves inconsistently, another might be more reliable

**Procedure:**
1. Identify which USB port on your computer has better power delivery/stability
2. Update platformio.ini:
   ```ini
   upload_port = /dev/ttyUSB1  # or /dev/ttyACM1
   monitor_port = /dev/ttyUSB1
   ```
3. Try upload: `pio run -e arduino_mega -t upload`

**Why this works:** Different USB hubs/ports have different driver stacks and reset behavior. Sometimes a different port has more stable DTR handling.

### Option 4: Adjust Upload Speed

**When to use:** 115200 baud is too fast for your USB driver's timing

**Procedure:**
1. Edit `auto_orientation/platformio.ini`:
   ```ini
   [env:arduino_mega]
   upload_speed = 57600  # or 9600 for maximum stability
   ```
2. Try upload: `pio run -e arduino_mega -t upload`

**Why this works:** Slower baud rate gives more time for the bootloader to respond. Less timing-sensitive.

**Speed recommendations:**
- 115200: Standard, fastest, requires stable DTR/RTS
- 57600: More tolerant of timing issues
- 9600: Most stable, but slower (takes ~1-2 minutes for large firmware)

### Option 5: Check avrdude Configuration

Some Linux setups need explicit reset signal configuration:

```bash
# Create a custom avrdude.conf override
cat > ~/.avrduderc << 'EOF'
# Linux-specific workaround for Arduino Mega bootloader
default_serial = "/dev/ttyACM0";
default_bitclock = 5.0;  # Safe default for Mega

programmer
  id = "arduino_linux";
  desc = "Arduino Mega on Linux (with DTR reset)";
  type = "stk500v2";
  connection_type = serial;
  reset = dtr;          # Explicitly use DTR for reset
  ...
EOF
```

---

## Part 4: If Bootloader is Corrupted (Advanced)

### Bootloader Re-flash via ICSP

**When to use:** Bootloader is erased (`signature = 0x000000`) and reset button doesn't work

**Requirements:**
- ICSP programmer (Arduino as ISP, Atmel ICE, or similar)
- Connection to ICSP header on Mega (6-pin programming port)

**Procedure:**
1. Download Mega2560 bootloader:
   ```bash
   # This is built-in to Arduino IDE
   # Usually at: ~/.arduino15/packages/arduino/hardware/avr/1.8.x/bootloaders/stk500v2/stk500v2_mega2560.hex
   ```

2. Connect ICSP programmer to Mega's ICSP header:
   ```
   ICSP Header pinout:
   1 - MISO (pin 50)
   2 - GND
   3 - SCK (pin 52)
   4 - MOSI (pin 51)
   5 - RESET (pin 53)
   6 - +5V
   ```

3. Use avrdude to re-flash:
   ```bash
   avrdude -c usbtiny -p m2560 -U lfuse:w:0xff:m -U hfuse:w:0xd0:m -U efuse:w:0xf5:m -U flash:w:stk500v2_mega2560.hex:i
   ```

**Fuse values explained:**
- `lfuse = 0xFF`: Enable external crystal (if present) or internal oscillator
- `hfuse = 0xD0`: Enable bootloader section (4KB), disable JTAG
- `efuse = 0xF5`: Enable brown-out detection at 2.7V

---

## Part 5: Understanding Verbose Output

### Successful Upload

```
avrdude: Version 6.3
         Copyright (c) 2000-2005 Brian Dean, http://www.bdm.cc
         Copyright (c) 2007-2014 Joerg Wunsch

         System wide configuration file is "/etc/avrdude.conf"
         User configuration file is "/home/user/.avrduderc"
         User configuration file does not exist

         Using Port                    : /dev/ttyACM0
         Using Programmer              : stk500v2
         Overriding Baud Rate          : 115200
         AVR Part                       : ATmega2560
         Chip Erase delay              : 9000 us
         PAGEL                         : PD7
         BS2                           : PA0
         RESET disposition             : dedicated
         Memory Detail                 :

[... lots of memory layout info ...]

         Programmer Type : STK500V2
         Description     : STK500 V2
         Programmer Model: AVRISP mkII
         Hardware Version: 4
         Firmware Version: 6.2
         Software Version: 1.4
         Vtarget         : 5.0 V
         SCK period      : 3.3 us

         avrdude: AVR device initialized and ready to accept instructions
         Reading | ################################################## | 100% 0.01s

         avrdude: Device signature = 0x1e 0x98 0x01 (probably m2560)
         avrdude: reading input file ".../firmware.hex"
         avrdude: input file .../firmware.hex auto detected as Intel HEX
         avrdude: writing flash (131956 bytes):

         Writing | ################################################## | 100% 18.46s

         avrdude: 131956 bytes of flash written
         avrdude: verifying flash memory against .../firmware.hex:
         Verifying | ################################################## | 100% 2.89s

         avrdude: 131956 bytes of flash verified

         avrdude done.  Thank you.
```

**Key success indicators:**
- `Device signature = 0x1e 0x98 0x01` ✓ (Mega2560 detected)
- `writing flash ... bytes` ✓ (Data transferred)
- `verified` ✓ (Data integrity confirmed)

### Failed Upload - Bootloader Not Responding

```
avrdude: Version 6.3
         [...]
         Using Port                    : /dev/ttyACM0
         Using Programmer              : stk500v2

avrdude: stk500v2_ReceiveMessage(): timeout
avrdude: stk500_initialize(): protocol error, expect=0x14, got=0xff
avrdude: initialization failed, rc=-1
         double check the connections and try again, or use -F to override
         this check.

avrdude done.  Thank you.
```

**What's happening:**
- `timeout`: Avrdude sent SYNC byte, bootloader didn't respond within 1 second
- `expect=0x14, got=0xff`: Expected sync response (0x14), got no data (0xff = idle state)
- **Root cause:** Bootloader not running, or reset didn't trigger properly

**Next step:** Try Option 1 (press reset button) or Option 2 (unplug/replug)

### Bootloader Corrupted - No Device Signature

```
avrdude: stk500v2_ReceiveMessage(): timeout
avrdude: stk500_initialize(): protocol error, expect=0x14, got=0xff
[... timeout errors ...]
avrdude: Yikes!  Invalid device signature.
         double check the connections and try again, or use -F to override
         this check.

avrdude: Device signature = 0x00 0x00 0x00 (probably m2560)
```

**What's happening:**
- Bootloader either missing or severely corrupted
- Flash memory is blank or uninitialized
- **Solution required:** ICSP re-flash (see Part 4)

---

## Part 6: Diagnosing DTR/RTS Issues on Linux

### Check DTR Signal with strace

This advanced technique monitors low-level serial port control:

```bash
# Start monitoring in one terminal
sudo strace -p $(pgrep avrdude) -e ioctl 2>&1 | grep -i "dtr\|rts" &

# In another terminal, start upload
pio run -e arduino_mega -t upload -v

# Expected output shows DTR being toggled (TIOCMSET ioctl calls)
```

### Check USB Driver Type

Different USB drivers handle reset differently:

```bash
# Find which driver is in use
lsusb -v | grep iProduct | head -1
# Look for: "Arduino Mega 2560"

# Check driver module
ls -la /sys/bus/usb/devices/*/driver | grep -E "cp210x|usbserial|cdc_acm"

# Each driver has different DTR behavior:
# - cp210x (Silicon Labs): Generally stable DTR
# - usbserial (generic): Often unstable DTR
# - cdc_acm (CDC ACM): Reliable DTR
```

### Verify DTR Timeout Settings

Linux has configurable DTR timeout. Check current values:

```bash
# Modern approach (systemd)
cat /sys/module/usbserial/parameters/dtr_rts_on_open
# Should be Y (yes) to enable DTR on open

# If not, you may need:
sudo modprobe -r usbserial
sudo modprobe usbserial dtr_rts_on_open=Y
```

---

## Part 7: Automated Recovery Script

An automated recovery script is provided in `tools/recover_bootloader.sh`. It tries each recovery strategy in sequence and reports which worked:

```bash
# From the floppi root directory
chmod +x tools/recover_bootloader.sh
./tools/recover_bootloader.sh

# Script will:
# 1. Check bootloader presence
# 2. Try manual reset timing
# 3. Try USB power cycle
# 4. Try alternate baud rates
# 5. Report success/failure with next steps
```

---

## Part 8: Prevention - Optimized platformio.ini

For maximum compatibility, add these settings to `auto_orientation/platformio.ini`:

```ini
[env:arduino_mega]
platform = atmelavr
board = megaatmega2560
framework = arduino
upload_speed = 115200
monitor_speed = 115200
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
lib_deps =
    Adafruit Unified Sensor
    Adafruit BusIO

# Linux-specific: Ensure DTR/RTS are handled properly
upload_flags = -v
upload_wait_for_upload_port = true
monitor_filters = esp32_exception_decoder, log2file

# Alternative if upload fails consistently
# upload_speed = 57600       # Slower, more stable
# upload_wait_for_upload_port = true
```

### Understanding These Settings

- `upload_wait_for_upload_port = true`: Wait for serial port to appear after DTR pulse
- `upload_flags = -v`: Enable verbose mode (shows reset behavior)
- `monitor_filters`: Optional, for better serial monitor output

---

## Quick Reference: Decision Tree

```
Upload fails with "timeout"?
├─ Yes, but worked on Windows?
│  └─ DTR/RTS timing issue (Options 1-2)
│     ├─ Try: Press reset button during upload
│     ├─ Try: Unplug/replug USB during upload
│     ├─ Try: Different USB port
│     └─ Try: Slower baud rate (57600)
│
├─ Device signature = 0x000000?
│  └─ Bootloader erased (ICSP re-flash required)
│
├─ Random timeouts (sometimes works)?
│  └─ Intermittent DTR issue
│     └─ Try: Reduce upload_speed, try different USB hub
│
└─ Never worked, new board?
   └─ Check bootloader jumper, verify USB driver installed
```

---

## Technical References

### STK500v2 Protocol Timing

The bootloader uses the STK500v2 (AVR ISP mk II protocol) with these critical timing requirements:

```
Event                          Timeout      Description
─────────────────────────────────────────────────────────
SYNC byte (0x30) sent         1 second     Bootloader must respond with 0x14
Response received              Immediate   Protocol handshake established
Data block transfer            Varies       Per-block confirmation needed
Between blocks                 100ms       Time to erase/write flash
Total upload                   ~20s        For 256KB firmware at 115200 baud
```

**Key insight:** The 1-second timeout for SYNC response is the critical failure point on Linux. If DTR reset happens too late, the bootloader misses the SYNC byte.

### Bootloader Code Flow

Arduino Mega bootloader (stk500v2) does this:

```
1. Power on → Jump to bootloader (address 0xF000)
2. Initialize UART at 115200 baud
3. Start 8-second timeout counter
4. Loop: Read serial input
   - If SYNC byte (0x30) received → Respond with 0x14
   - If other commands → Process according to STK500v2 spec
   - If timeout expired → Jump to user program
5. User program runs (or bootloader loops if no code)
```

**Critical period:** Between steps 2-3. If avrdude sends SYNC before UART initialization, bootloader misses it.

---

## Version History

- **2026-05-05**: Initial bootloader recovery guide created
  - Documented Windows/Linux DTR/RTS differences
  - Created 5-step recovery procedure
  - Added STK500v2 protocol explanation
  - Included verbose output examples

---

## See Also

- [Teensy Serial Troubleshooting Guide](/home/devel/floppi/docs/findings/teensy-serial-troubleshooting.md) - Different platform, similar issues
- [Arduino Mega Datasheet](http://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2549-8-bit-AVR-Microcontroller-ATmega640-ATmega1280-ATmega1281-ATmega2560-ATmega2561_datasheet.pdf) - ICSP pinout and fuse details
- [avrdude Documentation](https://www.nongnu.org/avrdude/user-manual/avrdude.html) - Serial programmer protocol
