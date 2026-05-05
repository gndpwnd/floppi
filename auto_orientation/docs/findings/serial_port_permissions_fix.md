# Serial Port Permissions Fix (Known Issue)

## Problem
Attempting to upload to Arduino or read from serial devices results in:
```
Permission denied: /dev/ttyACM0
Permission denied: /dev/ttyACM1
```

## Root Cause
User account is not in the `dialout` group, which owns serial device files.

## Solution

### Step 1: Add user to dialout group
```bash
sudo usermod -aG dialout <username>
```

### Step 2: Activate the group membership (required for current shell session)
```bash
newgrp dialout
```

Or log out and log back in to apply permanently.

### Verification
```bash
id | grep dialout
```
Should show: `groups=... dialout ...`

### Why This Happens
- Linux requires group membership to access `/dev/ttyACM*` devices
- User is added to the group, but the current shell session doesn't reflect this until a new shell is started
- Each new SSH/terminal session automatically inherits the group membership after relogin

## Related Discovery (2026-05-05)
During testing, discovered that:
- GPS on `/dev/ttyACM0`: **Working perfectly** (live NMEA output)
- Arduino Mega on `/dev/ttyACM1`: **Not responding to bootloader** (separate issue - see bootloader_recovery_guide.md)

## For Future Agents
- Always verify dialout group membership with `id` before attempting serial operations
- If developing upload scripts/tools, document this permission requirement clearly
- Recommend running `newgrp dialout` in any CI/CD that automates uploads
