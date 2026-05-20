# Diagnose Decision Tree

Symptom-first troubleshooting flow for the flight controller. Pair this with
`dev.sh diagnose [PORT]` (host-side readiness check) and the `n` calibration
command (on-board network diagnostics, ESP32 only).

This is a **decision-tree presentation**. For flat-list troubleshooting
(upload errors, motor wiring, flight tuning) see
[3_troubleshooting.md](3_troubleshooting.md). Each block follows
`SYMPTOM → RUN → BRANCH`: pick the symptom you see, run the first check,
follow YES/NO down.

---

## 1. Serial port not detected / `dev.sh monitor` can't open the port

```
SYMPTOM: dev.sh monitor / pio device monitor fails to open the port
  |
  v
RUN: dev.sh diagnose
  |
  v
OUTPUT contains "ModemManager: ACTIVE"?
  +-- YES: ModemManager opens every new CDC device for 30 s, corrupting the
  |         stream and locking the port. Remediation:
  |           sudo systemctl stop ModemManager       (one-shot)
  |           sudo systemctl mask ModemManager       (permanent disable)
  |           sudo apt remove modemmanager           (nuke entirely)
  |         Then unplug+replug the board.
  |
  +-- NO: continue.
        |
        v
      OUTPUT contains "Serial ports: none found"?
        +-- YES: no /dev/ttyACM* or /dev/ttyUSB* exists.
        |         - Teensy: may be in HalfKay bootloader (no CDC). Press
        |           reset or run `tools/teensy_reboot`.
        |         - USB cable: must be a data cable.
        |         - ESP32: check USB-UART driver (CP2102/CH340) — lsusb
        |           should show the bridge.
        |         - Receiver drawing too much current can brown out the MCU
        |           on USB power — disconnect receiver and retry.
        |
        +-- NO: port exists. Continue.
              |
              v
            OUTPUT shows port "(held by PIDs: N)"?
              +-- YES: another process owns the line. Identify and kill:
              |         fuser -k /dev/ttyACM0
              |         pkill -f serial_monitor       (typical culprit)
              |         pkill -f "pio device"
              |
              +-- NO: port is free but monitor still fails. Suspect CDC
                    degradation: Teensy USB CDC degrades after ~15 rapid
                    open/close cycles (`docs/todo.md` Notes). Only
                    `teensy_reboot` or a physical unplug recovers — DTR/RTS
                    does NOT reboot Teensy 4.0.
```

## 2. Gyro / attitude drift while running

```
SYMPTOM: roll/pitch slowly drifts when board is stationary; gyro values
         non-zero at rest; aircraft "leans" in one direction
  |
  v
RUN: enter calibration mode (`dev.sh calibrate`), press `t` for telemetry,
     observe GyroX/Y/Z at rest
  |
  v
GyroX/Y/Z biases > ~1 deg/s with board still?
  +-- YES: gyro uncalibrated. Re-run IMU calibration (`i` -> `y` -> `y`).
  |         Board MUST be still and level; let MPU warm up 5 min first
  |         (temperature-dependent — see `3_troubleshooting.md` IMU section).
  |         Copy the printed IMU_GYRO_ERROR_X/Y/Z into config.h, rebuild,
  |         reflash.
  |
  +-- NO: biases small but attitude still drifts.
        |
        v
      AccZ at rest ≈ +1.0 g, AccX/Y ≈ 0?
        +-- NO: IMU mis-mounted (see "MPU mounting" below).
        +-- YES: try increasing Madgwick beta (e.g. 0.04 -> 0.06) so the
                  filter trusts accel more — see 3_troubleshooting.md
                  "IMU values drift over time".
```

## 3. MPU mounting wrong / axes swapped or inverted

```
SYMPTOM: AccX or AccY ≈ 1.0 g at rest (instead of AccZ); roll and pitch are
         swapped; tilting the board moves the "wrong" angle
  |
  v
RUN: in calibration mode, press `t`, observe Acc components at rest
  |
  v
Which axis reads ~1.0 g at rest?
  +-- AccZ ≈ +1.0 g:    correct mounting. Skip to gyro check.
  +-- AccX or AccY ≈ ±1.0 g, or AccZ ≈ -1.0 g (upside down):
        IMU is rotated/inverted vs. the firmware's expected frame.
        |
        v
      RUN: orientation detection (`o` command in calibration mode, or
           `dev.sh calibrate orientation`). Walks through three positions
           (level / nose-up / right-up) and emits an axis transform.
        |
        v
      Copy the generated #defines / transform into config.h (or imu.cpp's
      getIMUdata() per 3_troubleshooting.md), rebuild, reflash, re-verify:
      AccZ ≈ 1.0 g, GyroX/Y/Z ≈ 0. Then re-run IMU calibration (`i`) to
      capture biases in the NEW mounting orientation.
```

Reference: `docs/todo.md` Notes — "MPU6050 mounting: AccX≈1.02 g, AccY≈0.05 g,
AccZ≈-0.10 g → X-axis points down" is the canonical example of a wrong-mount.

## 4. WiFi works but the web UI won't load

```
SYMPTOM: ESP32 serial log shows "[WiFi] Connected! IP: x.x.x.x", but
         http://floppi-XXXX.local fails to load in a browser
  |
  v
RUN: in calibration mode, press `n` (network diagnostics)
  |
  v
"[N] mDNS hostname" line printed?
  +-- NO: USE_WEB_SERVER not enabled in this build. Rebuild with `esp32` or
  |         `esp32s3` (not `_calibration` only) and reflash.
  |
  +-- YES: hostname computed. Continue.
        |
        v
      Browse to the raw IP instead of `*.local`: http://192.168.1.x/
        +-- LOADS: mDNS broken on host/network. Install Bonjour/Avahi on the
        |         host, or just keep using the IP.
        |
        +-- ALSO FAILS: server not reachable.
              |
              v
            Same subnet / no AP isolation?
              +-- NO: SSID has "AP isolation" / "guest mode". Use a
              |         non-isolated SSID.
              |
              +-- YES: try `curl -v http://<ip>/api/status` from the host.
                       - "refused": port 80 not listening — check calibration
                         `n` for "Web server port availability".
                       - "timed out": host firewall blocks 80, OR ESP32 lost
                         WiFi between attempts (check serial).
```

Reference: `src/web_server.cpp:131-138` (mDNS), `features/wifi-configuration.md`
"Network Diagnostics".

## 5. OTA upload fails

```
SYMPTOM: ArduinoOTA / pio over_the_air target fails to push firmware
  |
  v
Is the drone armed right now?
  +-- YES: `handleOTA()` ignores OTA traffic while `armedFly == true`
  |         (`src/ota.cpp:62-65`). Disarm (throttle low + CH5 low) and retry.
  |
  +-- NO: continue.
        |
        v
      Serial log shows "[OTA] Ready at floppi-XXXX.local" on the board?
        +-- NO: USE_OTA not compiled in this build. Rebuild with the live
        |         env (not `_calibration`) so `#if defined(USE_OTA)` is true.
        |
        +-- YES: continue.
              |
              v
            Hostname / IP resolves from the host? (see flow #4 if not)
              +-- NO: same fix as flow #4.
              |
              +-- YES: OTA library / partition mismatch. Confirm host
                       PlatformIO and the board's installed firmware were
                       built from the same ESP32 Arduino core version. A
                       clean `pio run -t clean && pio run -t upload` over
                       USB-serial re-syncs partitions.
```

## 6. `tools/dev.sh test` (test_calibration.sh) fails

```
SYMPTOM: dev.sh test reports one or more failed assertions
  |
  v
Which test failed? (output names the function and assertion)
  +-- Boot banner / READY test: firmware never reached main loop. Check
  |         serial with `dev.sh monitor` for IMU error or setup hang. Often
  |         I2C wiring — see 3_troubleshooting.md "IMU/Sensor Issues".
  |
  +-- `t` telemetry tests: line format changed, OR wrong env flashed.
  |         Confirm you flashed the `_calibration` env, not a live one.
  |
  +-- `i` / `o` / `r` / `g` / `p` tests: a calibration command's output
  |         drifted from what the suite expects. Compare `dev.sh calibrate`
  |         (press the letter) against expected strings in
  |         tests/suites/test_calibration.sh.
  |
  +-- ANY test fails with serial-port errors: run `dev.sh diagnose` and fall
        back to flow #1. The harness wraps `serial_monitor.py`; ModemManager
        or CDC degradation makes every test look broken.
```

Reference: `tests/lib/harness.sh` (shared) and `tests/suites/test_calibration.sh`
(18 functions / 42 assertions).

---

## When all else fails

1. `dev.sh diagnose` — host environment.
2. `n` (calibration mode, ESP32) — on-board network state.
3. `c` (calibration mode) — which calibrations are stored vs. fresh.
4. `d` — dumps all config #defines as the firmware sees them.

If none isolate the problem, fall through to
[3_troubleshooting.md](3_troubleshooting.md) for upload errors, receiver
binding, ESC behavior, flight tuning, and power problems — not duplicated here.
