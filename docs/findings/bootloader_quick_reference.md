# Arduino Mega Bootloader Recovery - Quick Reference

## Problem: Upload Fails with "avrdude: stk500v2_ReceiveMessage(): timeout"

### This worked on Windows 2 days ago - What changed on Linux?

**Root cause:** DTR/RTS reset timing is different on Linux. The bootloader doesn't start listening in time for the STK500v2 handshake.

---

## 3-Second Solutions (Try These First)

### Option A: Press RESET Button (Easiest)
```bash
1. Start upload:        pio run -e arduino_mega -t upload
2. When you see text:   "Uploading..." or "Connecting..."
3. Press RESET button:  (Red button on Arduino Mega)
4. Hold for 1 second
5. Release
6. Upload completes automatically
```

**Success indicator:** You'll see "Found programmer" and progress bar.

---

### Option B: Unplug/Replug USB (Second Easiest)
```bash
1. Unplug USB from Mega
2. Wait 2 seconds
3. Run:                 pio run -e arduino_mega -t upload
4. Watch for:          "Connecting to programmer..."
5. Plug USB in:        (Within 1-2 seconds of step 4)
6. Upload completes
```

**Critical timing:** Plug in DURING upload attempt, not before or after.

---

### Option C: Slower Baud Rate (Most Reliable)
```bash
Edit: auto_orientation/platformio.ini

Change this line:
  upload_speed = 115200

To:
  upload_speed = 57600

Then: pio run -e arduino_mega -t upload
```

**Trade-off:** Upload takes longer (~1 minute) but much more stable.

---

## Troubleshooting Decision Tree

```
Can you press the RESET button in sync with the upload?
  └─ YES → Try Option A (press reset)
  └─ NO → Try Option B (unplug/replug)

Both options didn't work?
  └─ Try Option C (slower baud rate)

Still failing after all 3?
  └─ See "Detailed Diagnosis" section below
```

---

## Detailed Diagnosis

### Step 1: Check What's Actually Failing

```bash
cd auto_orientation

# Run with verbose output
pio run -e arduino_mega -t upload -v 2>&1 | head -30

# Look for one of these lines:
```

| Output | Meaning | Solution |
|--------|---------|----------|
| `Device signature = 0x1e 0x98 0x01` | ✓ Bootloader found | Should upload work → retry with reset button |
| `avrdude: stk500v2_ReceiveMessage(): timeout` | Bootloader not responding | Try reset button or unplug/replug |
| `device signature = 0x000000` | Bootloader erased | Need ICSP re-flash (advanced) |

---

### Step 2: Try Different USB Port

```bash
# Check available ports
ls -la /dev/tty*

# Edit platformio.ini:
upload_port = /dev/ttyUSB0     # or /dev/ttyUSB1, /dev/ttyACM1, etc.
monitor_port = /dev/ttyUSB0

# Retry upload
pio run -e arduino_mega -t upload
```

---

### Step 3: Check USB Cable and Driver

```bash
# Verify Arduino is detected
lsusb | grep -i arduino

# Expected: "Arduino LLC Arduino Mega 2560"

# Check if port is readable
ls -la /dev/ttyACM0

# If permission denied, try:
sudo chmod 666 /dev/ttyACM0

# Or add yourself to dialout group:
sudo usermod -aG dialout $USER
# (Requires logout/login to take effect)
```

---

## Automated Recovery

```bash
# Run automatic recovery script
cd /home/devel/floppi
./tools/recover_bootloader.sh

# This will:
# 1. Verify bootloader is present
# 2. Try reset button timing
# 3. Try USB power cycle
# 4. Try alternate baud rates
# 5. Report which method worked
```

---

## Success - What to Expect

```
avrdude: AVR device initialized and ready to accept instructions
Reading | ################################################## | 100% 0.01s

avrdude: Device signature = 0x1e 0x98 0x01 (probably m2560)
[... more output ...]
avrdude: writing flash (131956 bytes):

Writing | ################################################## | 100% 18.46s

avrdude: 131956 bytes of flash written
avrdude: verifying flash memory against .../firmware.hex:
Verifying | ################################################## | 100% 2.89s

avrdude: 131956 bytes of flash verified

avrdude done.  Thank you.
```

**Key indicators:**
- `Device signature = 0x1e 0x98 0x01` ✓
- `writing flash` with progress bar ✓
- `verified` at the end ✓
- No "timeout" errors ✓

---

## When to Give Up and Do ICSP Re-flash

You need an ICSP re-flash if:
- All recovery options failed
- You see `device signature = 0x000000` (blank device)
- Bootloader is definitely erased

**Requirements:**
- ICSP programmer (Arduino as ISP, Atmel ICE, etc.)
- Soldering skills (or use ICSP header if exposed)
- 30-60 minutes of time

See full guide: `docs/findings/bootloader_recovery_guide.md` → Part 4

---

## Common Mistakes to Avoid

### ❌ Mistake: "I'll just use the -F flag to force the upload"
Don't use `avrdude -F` or `-t upload -F` - this uploads garbage to the wrong memory location.

### ❌ Mistake: "I'll wait for the full timeout before trying recovery"
The upload timeout is 10+ seconds. Try recovery steps (reset button) DURING the upload attempt, not after it fails.

### ❌ Mistake: "I'm holding the button the whole time"
Bootloader needs a **pulse** (1 second), not held down. Holding it prevents the bootloader from exiting.

### ❌ Mistake: "I unplugged it, now what?"
If you unplug for recovery, you MUST plug back in DURING the upload window (1-2 seconds after upload starts), not after avrdude times out.

---

## Getting Help

If none of this works:

1. **Check the full guide:**
   ```
   /home/devel/floppi/docs/findings/bootloader_recovery_guide.md
   ```

2. **Generate debug log:**
   ```bash
   pio run -e arduino_mega -t upload -v 2>&1 | tee debug_upload.log
   # Share debug_upload.log with Arduino support
   ```

3. **Check project documentation:**
   ```
   /home/devel/floppi/docs/README.md
   /home/devel/floppi/auto_orientation/README.md
   ```

---

## Version Info

- **Created:** 2026-05-05
- **Tested on:** Ubuntu Linux with Arduino Mega 2560
- **Related issues:** Windows/Linux DTR/RTS timing differences
- **See also:** Full guide at bootloader_recovery_guide.md
