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

```mermaid
flowchart TB
    S["SYMPTOM: dev.sh monitor / pio device monitor fails to open the port"]
    S --> RUN["RUN: dev.sh diagnose"]
    RUN --> Q1{"OUTPUT contains<br/>'ModemManager: ACTIVE'?"}
    Q1 -->|YES| A1["A: ModemManager corrupts/locks the port (see note 1)"]
    Q1 -->|NO| Q2{"OUTPUT contains<br/>'Serial ports: none found'?"}
    Q2 -->|YES| A2["B: no /dev/ttyACM*/ttyUSB* exists (see note 2)"]
    Q2 -->|NO| Q3{"OUTPUT shows port<br/>'(held by PIDs: N)'?"}
    Q3 -->|YES| A3["C: another process owns the line (see note 3)"]
    Q3 -->|NO| A4["D: port free but monitor fails — CDC degradation (see note 4)"]
```

1. **ModemManager ACTIVE** — opens every new CDC device for 30 s, corrupting the stream and locking the port. Remediation: `sudo systemctl stop ModemManager` (one-shot), `sudo systemctl mask ModemManager` (permanent disable), `sudo apt remove modemmanager` (nuke entirely). Then unplug+replug the board.
2. **No serial port** — no `/dev/ttyACM*` or `/dev/ttyUSB*` exists. Teensy: may be in HalfKay bootloader (no CDC) — press reset or run `tools/teensy_reboot`. USB cable: must be a data cable. ESP32: check USB-UART driver (CP2102/CH340) — `lsusb` should show the bridge. Receiver drawing too much current can brown out the MCU on USB power — disconnect receiver and retry.
3. **Held by PIDs** — another process owns the line. Identify and kill: `fuser -k /dev/ttyACM0`, `pkill -f serial_monitor` (typical culprit), `pkill -f "pio device"`.
4. **CDC degradation** — Teensy USB CDC degrades after ~15 rapid open/close cycles (`docs/todo.md` Notes). Only `teensy_reboot` or a physical unplug recovers — DTR/RTS does NOT reboot Teensy 4.0.

## 2. Gyro / attitude drift while running

```mermaid
flowchart TB
    S["SYMPTOM: roll/pitch drifts when stationary; gyro non-zero at rest; aircraft 'leans'"]
    S --> RUN["RUN: calibration mode (dev.sh calibrate), press 't', observe GyroX/Y/Z at rest"]
    RUN --> Q1{"GyroX/Y/Z biases<br/>> ~1 deg/s with board still?"}
    Q1 -->|YES| A1["A: gyro uncalibrated — re-run IMU calibration (see note 1)"]
    Q1 -->|NO| Q2{"AccZ at rest ≈ +1.0 g,<br/>AccX/Y ≈ 0?"}
    Q2 -->|NO| A2["B: IMU mis-mounted (see section 3)"]
    Q2 -->|YES| A3["C: increase Madgwick beta (see note 2)"]
```

1. **Gyro uncalibrated** — re-run IMU calibration (`i` → `y` → `y`). Board MUST be still and level; let MPU warm up 5 min first (temperature-dependent — see `3_troubleshooting.md` IMU section). Copy the printed `IMU_GYRO_ERROR_X/Y/Z` into config.h, rebuild, reflash.
2. **Attitude still drifts, biases small** — try increasing Madgwick beta (e.g. 0.04 → 0.06) so the filter trusts accel more — see `3_troubleshooting.md` "IMU values drift over time".

## 3. MPU mounting wrong / axes swapped or inverted

```mermaid
flowchart TB
    S["SYMPTOM: AccX or AccY ≈ 1.0 g at rest (instead of AccZ); roll/pitch swapped; tilt moves 'wrong' angle"]
    S --> RUN["RUN: calibration mode, press 't', observe Acc components at rest"]
    RUN --> Q1{"Which axis reads<br/>~1.0 g at rest?"}
    Q1 -->|"AccZ ≈ +1.0 g"| A1["Correct mounting — skip to gyro check"]
    Q1 -->|"AccX/Y ≈ ±1.0 g, or AccZ ≈ -1.0 g (upside down)"| A2["IMU rotated/inverted vs. firmware's expected frame"]
    A2 --> RUN2["RUN: orientation detection ('o' command, or dev.sh calibrate orientation) — 3 positions (level / nose-up / right-up), emits axis transform"]
    RUN2 --> FIX["Copy transform into config.h (or imu.cpp getIMUdata()), rebuild, reflash, re-verify AccZ ≈ 1.0 g, Gyro ≈ 0; re-run IMU calibration ('i') in NEW orientation"]
```

Reference: `docs/todo.md` Notes — "MPU6050 mounting: AccX≈1.02 g, AccY≈0.05 g,
AccZ≈-0.10 g → X-axis points down" is the canonical example of a wrong-mount.

## 4. WiFi works but the web UI won't load

```mermaid
flowchart TB
    S["SYMPTOM: serial shows '[WiFi] Connected! IP: x.x.x.x' but http://floppi-XXXX.local won't load"]
    S --> RUN["RUN: calibration mode, press 'n' (network diagnostics)"]
    RUN --> Q1{"'[N] mDNS hostname'<br/>line printed?"}
    Q1 -->|NO| A1["A: USE_WEB_SERVER not enabled — rebuild with esp32/esp32s3 (not _calibration only), reflash"]
    Q1 -->|YES| Q2{"Browse raw IP<br/>http://192.168.1.x/ ?"}
    Q2 -->|LOADS| A2["B: mDNS broken on host/network — install Bonjour/Avahi, or keep using the IP"]
    Q2 -->|ALSO FAILS| Q3{"Same subnet /<br/>no AP isolation?"}
    Q3 -->|NO| A3["C: SSID has 'AP isolation'/'guest mode' — use a non-isolated SSID"]
    Q3 -->|YES| A4["D: curl -v http://&lt;ip&gt;/api/status (see note 1)"]
```

1. **`curl -v http://<ip>/api/status`** — "refused": port 80 not listening, check calibration `n` for "Web server port availability". "timed out": host firewall blocks 80, OR ESP32 lost WiFi between attempts (check serial).

Reference: `src/web_server.cpp:131-138` (mDNS), `features/wifi-configuration.md`
"Network Diagnostics".

## 5. OTA upload fails

```mermaid
flowchart TB
    S["SYMPTOM: ArduinoOTA / pio over_the_air target fails to push firmware"]
    S --> Q1{"Is the drone<br/>armed right now?"}
    Q1 -->|YES| A1["A: handleOTA() ignores OTA while armedFly == true (src/ota.cpp:62-65) — disarm (throttle low + CH5 low), retry"]
    Q1 -->|NO| Q2{"Serial shows '[OTA] Ready at<br/>floppi-XXXX.local'?"}
    Q2 -->|NO| A2["B: USE_OTA not compiled — rebuild with live env (not _calibration) so #if defined(USE_OTA) is true"]
    Q2 -->|YES| Q3{"Hostname / IP resolves<br/>from host? (see flow 4)"}
    Q3 -->|NO| A3["C: same fix as flow 4"]
    Q3 -->|YES| A4["D: OTA library / partition mismatch (see note 1)"]
```

1. **Partition mismatch** — confirm host PlatformIO and the board's installed firmware were built from the same ESP32 Arduino core version. A clean `pio run -t clean && pio run -t upload` over USB-serial re-syncs partitions.

## 6. `tools/dev.sh test` (test_calibration.sh) fails

```mermaid
flowchart TB
    S["SYMPTOM: dev.sh test reports one or more failed assertions"]
    S --> Q{"Which test failed?<br/>(output names function + assertion)"}
    Q -->|"Boot banner / READY"| A1["Firmware never reached main loop — check serial for IMU error/setup hang, often I2C wiring (see note 1)"]
    Q -->|"'t' telemetry tests"| A2["Line format changed, OR wrong env flashed — confirm _calibration env, not live"]
    Q -->|"'i'/'o'/'r'/'g'/'p' tests"| A3["A calibration command's output drifted — compare dev.sh calibrate vs expected strings in tests/suites/test_calibration.sh"]
    Q -->|"ANY test, serial-port errors"| A4["Run dev.sh diagnose, fall back to flow 1 (see note 2)"]
```

1. **Boot/READY failure** — see `3_troubleshooting.md` "IMU/Sensor Issues".
2. **Serial-port errors in tests** — the harness wraps `serial_monitor.py`; ModemManager or CDC degradation makes every test look broken.

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
