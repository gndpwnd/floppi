# 16 — GravityProbe capability inventory

**Source:** `~/GravityProbe`, branch `data-analysis` (already checked out; never switched).
**Read:** 2026-08-19. **Every one of the 22 tracked files was opened**, in full, except the
99,748-byte binary `esp32_GPIO.jpg` (listed, not decoded) and lines 5-6 of
`teensy_esp8266/teensy_esp8266.ino`, which were read through a masking filter that printed only the
declaration tokens — see §9.
**Also read for comparison:** `~/floppi/temp_reorg/11_routing_v2_2026-08-18.md` (governing),
`~/floppi/temp_reorg/08_gravityprobe_recon.md` (prior recon of the same tree),
`~/floppi/flight_controller/{include/wifi_config.h, include/wifi_connect.h, src/wifi_connect.cpp,
include/config.h}`, `~/floppi/auto_orientation/tests/i2c_scanner.ino`,
`~/floppi/auto_orientation/src/file_system/sd_card.cpp`, `~/floppi/flight_controller/src/display.cpp`,
`~/lowprofiledronegurus/sensor_interactions/`.
**Access discipline:** read-only. No writes, no branch changes, no git mutations against any legacy repo.
The only file written by this pass is this one.

> **HONESTY CAVEAT — nothing here has been re-run.**
> GravityProbe's last commit is `1e0c086`, **2025-01-29**. Nobody has built, flashed, or executed any of
> this in the ~19 months since. Every statement below is either **DOCUMENTED** (the repo's own prose or a
> commit message asserts it) or **VERIFIED** (I confirmed it by reading the code in this tree). Nothing is
> **TESTED**. Where I say a sketch "cannot work as written," that is a code-reading claim about a specific
> line, not a bench result — it is falsifiable and I name the line so it can be checked.

---

## 1. Branch claim — VERIFIED, and it is worse than "main is thin"

```
git -C ~/GravityProbe ls-tree -r --name-only main   →  README.md          (1 file)
git -C ~/GravityProbe ls-tree -r --name-only HEAD   →  22 files           (data-analysis)
git -C ~/GravityProbe status --short                →  (empty; tree clean, matches HEAD)
```

`main` holds **exactly one file**, and `git show main:README.md` is **byte-identical** to the
`data-analysis` copy (both 100 bytes, blob `d7d9d500`). So `main` is not a reduced version of the project —
it is the initial commit and nothing else. Anyone who clones GravityProbe with default settings
(`origin/HEAD → origin/main`) gets a two-line README and concludes the repo is empty. **All 1,617 lines of
source live on a non-default branch.** That is the single most important operational fact about this repo.

Full history, all branches (6 commits, 3 days of work in Jan 2025 plus one follow-up):

| Commit | Date | Author | Subject |
|---|---|---|---|
| `1e0c086` | 2025-01-29 | gndpwnd | enterprise wpa2 peap on esp32 works |
| `692c6e3` | 2025-01-04 | gndpwnd | mpu6050 readable data |
| `ceb4acd` | 2025-01-04 | gndpwnd | teensy SD and OLED working |
| `16b8cf6` | 2025-01-03 | msc_intra | init esp32 data collection and matlab analysis |
| `0cbf092` | 2025-01-03 | msc_intra | init |
| `c8f4965` | 2025-01-03 | msc_intra | Initial commit |

Two contributor identities: `msc_intra` and `gndpwnd`. If anything is carried into
`sensor_interactions`, both belong in its `PROVENANCE.md` (routing doc Gate 5).

---

## 2. What the repo actually is

`README.md`, in full (both branches): *"# GravityProbe / Code for NASA RockSat-C experiment to verify
newton's theory of gravitational change"*. That is the entire statement of purpose in the repo.

**Shape:** a flat bring-up playground. 12 folders, 11 of which hold exactly one `.ino`; `matlab_scripts/`
holds 5 `.m` files. No `platformio.ini`, no build config, no tests, no CI, no findings docs, no
scope/roadmap, no recorded data files. 1,617 lines of source total:

| Folder / file | Lines | Bytes | What it is |
|---|---|---|---|
| `teensy_mpu6050_zero/teensy_mpu6050_zero.ino` | 289 | 8,404 | MPU6050 offset calibration (bracket search) |
| `matlab_scripts/grocketBLE.m` | 264 | 8,849 | Live serial telemetry plotter + 2 AHRS scratch cells |
| `teensy_sdcard/teensy_sdcard.ino` | 114 | 3,187 | SD CSV logger (Teensy pinout) |
| `teensy_mpu6050/teensy_mpu6050.ino` | 112 | 3,564 | MPU6050 raw read w/ baked offsets |
| `esp32_hw125_sd/esp32_hw125_sd.ino` | 109 | 3,130 | SD CSV logger (ESP32 pinout) |
| `esp32_ewpa2_iic_091/esp32_ewpa2_iic_091.ino` | 104 | 4,394 | WPA2-EAP client + OLED IP display |
| `i2c_scanner_whoami/i2c_scanner_whoami.ino` | 103 | 2,838 | I²C scanner + register probe |
| `matlab_scripts/grocket.m` | 91 | 2,869 | Offline gravity-extraction analysis |
| `esp32_enterprise_wpa3_eap/…​.ino` | 87 | 3,722 | WPA2-EAP client + HTTP GET |
| `teensy_esp8266_wifi_home/…​.ino` | 82 | 1,943 | ESP8266 raw `WiFiServer` hello-world |
| `esp32_ent_wpa2_peap_web80/…​.ino` | 76 | 2,317 | WPA2-EAP client + :80 web server |
| `matlab_scripts/data_to_pos.m` | 46 | 1,591 | complementaryFilter → quaternions → trajectory |
| `teensy_esp8266/teensy_esp8266.ino` | 43 | 1,012 | ESP8266 `ESP8266WebServer` hello-world **(credential)** |
| `teensy_i2c_oled/teensy_i2c_oled.ino` | 43 | 1,226 | SSD1306 128×32 loading-bar demo |
| `matlab_scripts/plot_pos.m` | 38 | 1,110 | Plot trajectory + q0 |
| `matlab_scripts/ex_data.m` | 16 | 490 | Synthetic CSV fixture generator |
| `teensy4.0.md` | — | 4,370 | Teensy wiring + the captured MPU6050 offsets |
| `esp32.md` | — | 2,009 | ESP32 wiring + library links |
| `matlab.md` | — | 194 | Two MathWorks doc links, nothing else |
| `README.md` | — | 100 | Two lines |
| `.gitignore` | — | 20 | `**wifi_credentials.h` |
| `esp32_GPIO.jpg` | — | 99,748 | ESP32 pinout reference image (not decoded) |

**No recorded data exists in this tree.** `find` over the whole worktree returns zero `.csv`, `.txt`,
`.bin`, or log files. The MATLAB scripts read `data.csv`, `gdata.csv`, `quaternions.csv`,
`rocket_position.csv`; **three of the four have in-repo producers** — `data.csv` from `ex_data.m:15`
(synthetic sin/cos), and `quaternions.csv` + `rocket_position.csv` from `data_to_pos.m:22` and
`data_to_pos.m:45` (the chain described in §4.8). **Only `gdata.csv` has no producer anywhere in the
tree** — and `gdata.csv` is precisely what `grocket.m:2`, the actual gravity analysis, reads. Whatever
flight or bench capture `grocket.m` was written against is **not here** — the analysis pipeline has no
input. (`08_gravityprobe_recon.md` §3.6 describes `gdata.csv` as "real flight data"; that is an inference,
not something the tree supports.)

---

## 3. Folder names vs. reality — the audit

The routing doc's suspicion is correct and I found **more mismatches than it flagged**. This is the
single highest-value output of this pass: an engineer navigating this tree by folder name will be wrong
about six of twelve folders.

| Folder | Name implies | Code actually does | Evidence | Verdict |
|---|---|---|---|---|
| `esp32_enterprise_wpa3_eap/` | WPA3 EAP | **WPA2** PEAP | line 31: `WiFi.begin(ssid, WPA2_AUTH_PEAP, …)`; the only other options in the file (lines 34, 37, commented) are `WPA2_AUTH_PEAP` and `WPA2_AUTH_TLS`. No WPA3 token appears anywhere in the repo. | **LIES** |
| `teensy_esp8266/` | Teensy code | 100% **ESP8266** | lines 1-2: `#include <ESP8266WiFi.h>`, `#include <ESP8266WebServer.h>`. Zero Teensy API calls. | **LIES** |
| `teensy_esp8266_wifi_home/` | Teensy code | 100% **ESP8266** | line 1: `#include <ESP8266WiFi.h>`; `WiFiServer server(80)` at line 13. | **LIES** |
| `teensy_mpu6050/` | Teensy-specific | **board-agnostic** Arduino | Only `I2Cdev.h` + `MPU6050.h` + `LED_BUILTIN`. Nothing Teensy-only; compiles anywhere the ElectronicCats lib does (its `library.properties` lists avr/samd/sam/esp8266/esp32/stm32/renesas). | **misleading** |
| `teensy_mpu6050_zero/` | Teensy-specific | **board-agnostic** Arduino | Same: `I2Cdev.h` + `MPU6050.h` only. | **misleading** |
| `teensy_i2c_oled/` | Teensy-specific | **board-agnostic** | line 4 uses the bare `SCL`/`SDA` core macros, not pin numbers. Portable to any core that defines them. | **misleading** |
| `matlab_scripts/grocketBLE.m` | Bluetooth LE | **serial port** | lines 6-12: `comPort = 'COM36'`, `serialport(comPort, baudRate)`, `configureTerminator(…,"LF")`. No MATLAB `ble`/`blelist` call anywhere in the file. (Line 15's error text mentions "paired", so a Bluetooth-SPP virtual COM port is plausible — but the code path is plain serial.) | **LIES** |
| `esp32_ewpa2_iic_091/` | ESP32 + WPA2-Ent + 0.91" I²C | accurate on all three | line 47 PEAP, lines 7-8 U8G2 includes, `displayIPAddress()` at line 25 | accurate (but see §5.2) |
| `esp32_ent_wpa2_peap_web80/` | ESP32 + WPA2-Ent PEAP + web :80 | accurate | line 25 PEAP, line 10 `WiFiServer server(80)` | accurate |
| `esp32_hw125_sd/` | ESP32 + HW125 SD | accurate | `SD.begin(8)`, SPI | accurate |
| `i2c_scanner_whoami/` | I²C scan + WHO_AM_I | accurate in form, wrong in fact | see §5.5 | accurate name, broken feature |
| `teensy_sdcard/` | Teensy SD | pinout is Teensy-ish (`CS=10`), API is generic | `Sd2Card`/`SdVolume`/`SdFile` | accurate-ish |

**Also mismatched: the docs themselves.** `esp32.md` describes the OLED constructor as *"using normal scl
(gpio 22) and sda (18) pins **for the teensy**"* (line 26) — inside the ESP32 document, seven lines
below its own pinout block that says `SDA - 42 - GPIO 21` (line 19). It is a copy-paste from `teensy4.0.md` that was never
corrected, and it contradicts the correct GPIO 21 given above it. Separately, both `esp32.md` I²C lines
carry two numbers each (`SCL - 39 - GPIO 22`, `SDA - 42 - GPIO 21`); the GPIO values are the ESP32
defaults and are right, but the bare `39`/`42` are unexplained and exceed the header pin count of the
"DOIT ESP32 DevKit V1" named on line 2 of the same file. Treat GPIO 21/22 as authoritative and the bare
numbers as noise.

---

## 4. Capability inventory — what was proven, what it took, where the code is

### 4.1 ESP32 WPA2-Enterprise (PEAP) association — the repo's strongest claim
- **Status:** DOCUMENTED working. Commit `1e0c086` (2025-01-29) is literally *"enterprise wpa2 peap on
  esp32 works"*, and it added `esp32_ent_wpa2_peap_web80/esp32_ent_wpa2_peap_web80.ino` (76 lines).
  **The subject-to-file binding is looser than that phrasing suggests:** `git show --stat 1e0c086` is
  **10 files changed** — it also added `esp32_hw125_sd/esp32_hw125_sd.ino` (109 lines), `matlab.md`,
  `matlab_scripts/{data_to_pos,ex_data,plot_pos}.m` and `esp32_GPIO.jpg`, and rewrote `esp32.md` and
  `teensy4.0.md`. So the subject is evidence about the commit, not a certificate for one file — which
  matters, because the DOCUMENTED-vs-VERIFIED scheme used throughout this document leans on
  commit-subject-to-file binding.
- **What it took:** one `WiFi.begin()` overload. `esp32_ent_wpa2_peap_web80.ino:25` —
  `WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD)`. No `esp_wpa2.h`, no
  certificates, no anonymous outer identity. That is the whole technique.
- **Supporting pattern:** a connect-watchdog repeated in all three ESP32 WiFi sketches — count polls of
  `WiFi.status()`, and at 60 call `ESP.restart()`. **The poll interval is not the same in all three, so
  the timeout is not either** (an earlier revision stated a single "500 ms / ≈30 s" figure for all
  three; that merged two different configurations):
  - `esp32_enterprise_wpa3_eap.ino:40-45` — `delay(500)`, 60 polls ≈ **30 s**.
  - `esp32_ewpa2_iic_091.ino:56-60` — `delay(500)`, 60 polls ≈ **30 s**.
  - `esp32_ent_wpa2_peap_web80.ino:29-33` — `delay(100)`, 60 polls ≈ **6 s**, even though its own
    line-30 comment reads *"Timeout after 30 seconds"*. **The comment and the code disagree**; the code
    is authoritative. This is the same stale-comment failure mode catalogued in §5.10.

  Crude, but the pattern itself is a real field lesson: on a campus EAP network, reboot beats retry.
- **Reachability proof chosen:** HTTP GET of `wifitest.adafruit.com/rele/rele1.txt` on port 80
  (`esp32_enterprise_wpa3_eap.ino:71-73`, `esp32_ewpa2_iic_091.ino:88-90`). Note the URL path is
  copy-pasted from a third-party tutorial and does not match the host's own documented test path — the
  commented-out lines 74-75 / 91-92 show the author knew about `/testwifi/index.html` and did not switch.
- **Against two institutional networks.** Two different campus SSIDs are hardcoded across the ESP32
  sketches (see §9 — I am not listing them). That is the substance of the finding: the same PEAP code was
  pointed at two different institutions' RADIUS setups.
- **Where superseded:** §7. `floppi/flight_controller` is far ahead.

### 4.2 ESP32 as an HTTP server while joined to an enterprise network
- **Status:** DOCUMENTED working (same commit). `esp32_ent_wpa2_peap_web80.ino:10,42,54-74` — a raw
  `WiFiServer` on :80, `readStringUntil('\r')`, `indexOf("GET / ")`, hand-written response headers.
- **Worth knowing, but do not overread it:** the code *attempts* a `:80` server after EAP association.
  **Whether the campus network actually permitted an inbound connection is unknown** — client isolation
  is common on campus WLANs. The only evidence is commit `1e0c086`'s subject, which speaks to
  *association*; §7 records the unique empirical result as association against two RADIUS servers, and
  no repo prose, log or packet capture asserts that an inbound connection was ever accepted. Nobody has
  run this since 2025-01-29. (An earlier revision said this "proves" inbound reachability. It does not,
  and that wording is withdrawn.) If it were ever confirmed it would be a network-policy finding as much
  as a firmware one; as it stands it is an untested hypothesis that this code makes cheap to test.

### 4.3 ESP8266 as a WiFi coprocessor for a Teensy 4.0
- **Status: UNDOCUMENTED.** Nothing in the tree records either sketch serving a page. Both were added by
  `ceb4acd`, whose subject is *"teensy SD and OLED working"* — no mention of ESP8266 or WiFi; and
  `teensy4.0.md`'s ESP8266 section (lines 113-120) is four library links with no result claim. The code
  is a hello-world web server *by construction*, but "it served" is my reading of the source, not a
  record. (An earlier revision graded this DOCUMENTED; corrected.) Two variants, both ESP8266-only:
  - `teensy_esp8266/teensy_esp8266.ino` — high-level `ESP8266WebServer` with a route table
    (`server.on("/", HTTP_GET, handleRoot)`, line 28).
  - `teensy_esp8266_wifi_home/teensy_esp8266_wifi_home.ino` — low-level `WiFiServer`, manual request
    parse and response (lines 47-81), with the `STASSID`/`STAPSK` `#ifndef` template idiom (lines 4-7)
    and placeholder values.
- **The rationale is inferred, not stated:** the Teensy 4.0 has no radio, so an ESP8266 on a UART is the
  obvious bridge. **No Teensy-side bridge sketch exists in this tree** — there is no UART protocol, no
  framing, no AT-command layer. The coprocessor architecture was never actually built; only the ESP8266
  half's hello-world was.
- **Nothing in `~/floppi` supersedes this** — `grep -rl ESP8266` across floppi returns vendored library
  files (U8g2 portability headers, `lib_esp32/ESPAsyncWebServer`, `lib/MPU6050/README.md`,
  `lib/ArduinoJson/README.md`, `lib/PWMServo/docs/issue_template.md`), the two dRehmFlight MotionApps
  headers (`dRehmFlight-master/code/src/MPU6050/MPU6050_6Axis_MotionApps20.h` and `…_V6_12.h`), and one
  **first-party prose file** — `flight_controller/docs/findings/esp32-wifi-connectivity.md` — which an
  earlier revision's "only vendored library files" enumeration missed. What holds, and is the
  load-bearing half, is that there is **no first-party ESP8266 firmware**. So it is unique content, but
  unique content of near-zero value: the active line is ESP32-native, which needs no coprocessor.

### 4.4 SD-card CSV logging — two MCU variants, one schema
- **Status:** DOCUMENTED working for the Teensy side (commit `ceb4acd`, *"teensy SD and OLED working"*).
  **See §5.1 — I do not believe the Teensy sketch as committed can open a file.**
- **The durable artifact is the schema and the two operational rules**, both stated in `teensy4.0.md`:
  1. **Open once in `setup()`, never close in `loop()`.** *"The file (dataFile) is opened once in the
     setup() function and remains open for the entire duration of the loop."*
  2. **`flush()` after every write.** *"After every write, the flush() function is called to ensure the
     data is actually written to the SD card. This reduces the chances of data being cached and not
     actually written to the SD card."* (`teensy4.0.md:83`, quoted in full — an earlier revision dropped
     the closing "to the SD card") — implemented at `teensy_sdcard.ino:91` and `esp32_hw125_sd.ino:86`.
  For a sounding-rocket payload that loses power at an unpredictable moment, that pair is exactly right,
  and it is the only genuine design reasoning written down anywhere in the repo.
- **CSV schema (identical in both):** `t,ax,ay,az,gx,gy,gz,mx,my,mz` —
  `teensy_sdcard.ino:44`, `esp32_hw125_sd.ino:39`. Emitted at 1 Hz (`delay(1000)`).
- **Both log `random(-10,10)/10.0`, not sensors** (`teensy_sdcard.ino:57-65`,
  `esp32_hw125_sd.ino:52-60`). **The IMU was never wired into the logger.** GravityProbe never captured a
  single real sample through this path.
- **Hardware lesson worth carrying:** `esp32_hw125_sd.ino:5` — `const int chipSelect = 8; // GPIO 8 for CS
  pin, gpio2 might cause issues when flashing`. GPIO2 is a strapping pin on the ESP32; that comment is a
  real, transferable gotcha. (Note line 22's comment still says "with GPIO 2 as the CS pin" — stale, the
  code moved to 8.)
- **Superseded by** `~/floppi/auto_orientation/src/file_system/sd_card.cpp` (234 lines + a 186-line
  header, with `SD.begin(SD_CARD_CS_PIN)` done correctly at line 28).

### 4.5 SSD1306 128×32 OLED over software I²C
- **Status:** DOCUMENTED working (commit `ceb4acd`). `teensy_i2c_oled.ino` draws an animated 0→100%
  loading bar in `setup()` using `drawFrame`/`drawBox` and `map()` (lines 6-27); `loop()` is empty.
- **The load-bearing detail is one line** — the exact U8G2 constructor for the DSD Tech 0.91" module,
  recorded in both `esp32.md` and `teensy4.0.md` and used at `teensy_i2c_oled.ino:4`:
  `U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, SCL, SDA);`
  Picking the wrong U8G2 constructor for a cheap OLED is a classic multi-hour loss, so this is genuinely
  worth preserving as a note.
- **Superseded by** `~/floppi/flight_controller/src/display.cpp` (540 lines), which uses the **identical**
  `U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C` constructor (line 26) but parameterises the pins
  (`OLED_SCL_PIN`/`OLED_SDA_PIN`) and selects among **three distinct panels** at compile time —
  `SSD1306_128X32_UNIVISION` (line 26), `SSD1306_128X64_NONAME` (line 35) and `SH1106_128X64_NONAME`
  (line 44) — plus an `#else` default at line 53 that re-declares the 128×32. (An earlier revision said
  "four panel variants … two 128×64, SH1106", which double-counted the SH1106 as separate from the
  128×64 pair; there are four `#if` branches but three panels.)

### 4.6 I²C bus bring-up
- `i2c_scanner_whoami.ino` — the public-domain Arduino scanner (provenance honestly recorded in its own
  header comment, lines 1-27) plus a `getDeviceInfo()` addition (lines 77-103) that reads register `0x00`
  and probes `0x0F` as WHO_AM_I. Rescans every 5 s.
- **The WHO_AM_I probe is wrong for this repo's own sensor** — see §5.5.
- `teensy4.0.md` opens with the correct workflow, and this is worth keeping as prose. It is **two
  separate lines** (`teensy4.0.md:3` and `:5`), quoted here unjoined and without the sentence-ending
  periods the source does not have — an earlier revision spliced them into one sentence:
  *"Use the i2c scanner to determine the addresses for the mpu and the oled display"* /
  *"Use the mpu zero script relative to mpu name to calibrate"*.

### 4.7 MPU6050 bring-up and offset calibration — the most reusable code here
- **Status — split, because the two sketches in this section have different evidence.** An earlier
  revision applied one "DOCUMENTED working (commit `692c6e3`)" header to both, which the history does
  not support (`git log --diff-filter=A`):
  - `teensy_mpu6050.ino` (raw read): **DOCUMENTED working.** Added by `ceb4acd`, then rewritten by
    `692c6e3` *"mpu6050 readable data"* — and `692c6e3` touched only this sketch and `teensy4.0.md`, so
    that subject does bind to it.
  - `teensy_mpu6050_zero.ino` (calibration): **no commit message documents it working.** It was added by
    `ceb4acd` (*"teensy SD and OLED working"*). The only evidence it ever ran is its captured output
    table in `teensy4.0.md` — real, but weaker than a "works" commit subject.
- **`teensy_mpu6050_zero.ino` (289 lines)** runs **two independent calibration methods back to back**:
  first the ElectronicCats library's own `CalibrateAccel(6)`/`CalibrateGyro(6)` plus four refinement
  rounds to 1000 readings (lines 112-131), then a hand-written bracket search — `PullBracketsOut()`
  expands the offset bracket by ±1000 until it straddles the target (lines 142-182), `PullBracketsIn()`
  binary-searches inward and switches from `NFast=1000` to `NSlow=10000` samples once all brackets are
  narrow (lines 184-227). `Target[iAz] = 16384` (line 65) — 1 g at the default ±2 g range.
- **The output is captured verbatim in `teensy4.0.md`**, including the converged bracket table and the six
  offsets. **These six numbers are per-device and are the only real measured data in the entire
  repository:** `accelX=-4347, accelY=-351, accelZ=2620, gyroX=43, gyroY=82, gyroZ=29`. They are
  reproduced in code at `teensy_mpu6050.ino:17-22`.
- `teensy_mpu6050.ino` reads raw 6-axis via `getMotion6()` and offers a compile-time readable-vs-binary
  output switch (`OUTPUT_READABLE_ACCELGYRO` / `OUTPUT_BINARY_ACCELGYRO`, lines 15, 90-107). The binary
  branch is dead as written (`OUTPUT_BINARY_ACCELGYRO` is never defined) but is a fine pattern.
- **See §5.3 — the raw-read sketch double-applies its offsets.**
- **Largely superseded in `floppi` — CORRECTED.** An earlier revision of this section claimed there is
  "no application-level MPU6050 calibration or raw-read sketch anywhere in floppi", and that §4.7 is the
  one firmware capability floppi lacks. **Both statements are false and are withdrawn.** The error came
  from the grep behind them being symbol-scoped to the ElectronicCats names
  (`CalibrateAccel|PullBracketsIn|IMU_Zero`), which by construction cannot find a hand-rolled
  implementation of the same capability. floppi has first-party code for both halves:
  - **Calibration —** `flight_controller/lib/Calibration/calibration_imu.cpp` (470 lines, first-party):
    `calibrateIMU()` at line 10 is a stability-gated averaging calibration computing `AccErrorX/Y/Z` and
    `GyroErrorX/Y/Z` (lines 106-110), with sanity thresholds at lines 126 and 152 and `#define
    IMU_ACC_ERROR_X …` emission at line 213; `calibrateIMU6Position()` at line 231 is a six-position
    routine. Persistence is first-party too: `flight_controller/src/calibration_mode.cpp:293-313`
    `persistIMUCalibration()`, whose comment reads *"Persist the freshly-computed MPU6050 offsets from
    calResults to EEPROM"* and which logs *"MPU6050 cal saved to EEPROM"*, plus
    `flight_controller/lib/CalibrationStorage/calibration_storage.h:17` — *"The FC only needs to persist
    6 floats of MPU6050 offsets (24 B)"*. **`calibration_mode.cpp` is MPU6050-oriented, not
    BNO085-oriented**; the BNO085/mounting files (`auto_orientation/src/sensors/bno085_calibration.cpp`,
    `auto_orientation/src/navigation/mounting_calibration.cpp`) are a separate body of work that an
    earlier revision wrongly lumped in with it.
  - **Raw read —** `flight_controller/src/imu.cpp` (382 lines, first-party): `getIMUdata()` calls
    `mpu6050.getMotion6(&AcX, &AcY, &AcZ, &GyX, &GyY, &GyZ)` at line 168, scales to g and °/s, subtracts
    the stored offsets at line 196 (`AccX = (AccX - AccErrorX) * IMU_ACC_SCALE_X;`), and restores the
    offsets from EEPROM at lines 141-151.

  floppi does still vendor the same ElectronicCats library
  (`flight_controller/lib/MPU6050/library.properties`: `Electronic Cats, version 1.4.4`), whose
  `MPU6050.cpp` contains `CalibrateAccel`/`CalibrateGyro` — but that is no longer the point.
- **What is genuinely unique here is much narrower.** `teensy_mpu6050_zero.ino` programs the MPU6050's
  **on-chip hardware offset registers** — `SetOffsets()` at lines 255-262 writes
  `setXAccelOffset`/`setYAccelOffset`/`setZAccelOffset`/`setX,Y,ZGyroOffset` — driven by the
  bracket/binary search. floppi instead computes **software** offsets and applies them in the read path
  at `imu.cpp:196`; `grep -rn 'setXAccelOffset\|setXGyroOffset'` over `flight_controller/src/` and
  `flight_controller/lib/Calibration/` returns **no hits**. That is a real difference in *technique*,
  not an absent capability.

### 4.8 MATLAB orientation/gravity analysis — unique content, and unfinished
- **Nothing in `~/floppi` competes:** `find ~/floppi -name '*.m'` returns **zero files**. This is the only
  MATLAB work in the whole estate.
- **`matlab.md` is 194 bytes** and contains only two MathWorks links (Sensor Fusion Toolbox; C++ Code
  Generation). The second link is the interesting one — it suggests an intent to codegen the MATLAB
  filter down to embedded C++. **No generated code and no codegen script exists.** Aspiration only.
- **The one coherent pipeline** is synthetic end-to-end and does run as a chain:
  `ex_data.m` (synthetic sin/cos, writes `data.csv`) → `data_to_pos.m` (`complementaryFilter`, writes
  `quaternions.csv` + `rocket_position.csv`) → `plot_pos.m` (3D plot + q0 plot).
- **`grocket.m`** is the payload analysis proper: per-row quaternion, `quat2rotm`, rotate body accel into
  the Earth frame, take the Earth-Z component as gravity, plot body-frame vs Earth-frame accel + a
  `cumsum` "path" + the gravity trace. **This is the actual gravitational-verification idea, in 91 lines.**
  It has no input file (`grocket.m:2` reads `gdata.csv`, which is not in the tree) and **one** listed
  defect, **§5.9**. (An earlier revision said "three separate defects (§5.6, §5.7)"; §5.6 and §5.7 are
  defects of `grocketBLE.m`, not of this file, and three `grocket.m` defects are enumerated nowhere in
  this document.)
- **`grocketBLE.m`** documents a **17-field telemetry line** —
  `t,ax,ay,az,wx,wy,wz,mx,my,mz,qx,qy,qz,qw,earthX,earthY,earthZ` (lines 70, 99-117) — i.e. a firmware
  that already does on-board fusion and Earth-frame rotation. **No such firmware exists in this repo.**
  The most any in-repo sketch emits is 10 fields of `random()`. That 17-field format is a *specification
  of a device that was never built*, and it is arguably the most useful single thing in `matlab_scripts/`.
- **Three different fusion APIs across three scripts** — `complementaryFilter` (`data_to_pos.m:9`),
  `orientationSensor` (`grocket.m:5`), `ahrsfilter` (`grocketBLE.m:177, 227`). They are not variants of a
  chosen design; they are three attempts. At most one of them was ever run against real data, and
  `orientationSensor` does not appear in either doc link in `matlab.md` and is used nowhere else — treat
  its existence as unverified until someone opens MATLAB.

---

## 5. Defects found by reading — things that cannot have worked as committed

None of these are re-run results. Each names a line so it can be checked in five minutes.

**5.1 `teensy_sdcard.ino` never initialises the `SD` object it writes through.**
Line 27 calls `card.init(SPI_HALF_SPEED, chipSelect)` — that initialises the low-level `Sd2Card` object.
Line 39 then calls `SD.open(filename, FILE_WRITE)` on the global `SD` object, which was never given
`SD.begin()`. The `Sd2Card`/`SdVolume`/`SdFile` handles declared at lines 5-7 are never used again.
Expected behaviour: `dataFile` is falsy and every loop prints `"Failed to write to the file."`
**`teensy4.0.md` actively enshrines the mistake** — *"card.init() not the function SD.init()"* — so the
error is documented as if it were the fix. This sits directly under commit `ceb4acd` *"teensy SD and OLED
working"*, which is why "a green commit message is a claim, not proof" applies here literally. The ESP32
sibling gets it right (`esp32_hw125_sd.ino:23`, `SD.begin(chipSelect)`).

**5.2 `esp32_ewpa2_iic_091.ino` cannot compile — `u8g2` is undeclared.**
`displayIPAddress()` (lines 25-35) calls `u8g2.clearBuffer()`, `u8g2.setFont()`, `u8g2.drawStr()`,
`u8g2.sendBuffer()`. The file includes `<U8g2lib.h>` and `<U8x8lib.h>` (lines 7-8) but **declares no U8G2
object anywhere** — there is no `U8G2_SSD1306_… u8g2(…);` line. The working constructor exists in
`teensy_i2c_oled.ino:4` and in both markdown docs; it simply was not copied in. So the folder that the
routing conversation treats as "WiFi + OLED IP display" has never been built in this state.

**5.3 `teensy_mpu6050.ino` applies its calibration offsets twice.**
Lines 52-57 push the offsets into the MPU6050's own hardware offset registers
(`mpu.setXAccelOffset(accelXOffset)` etc.), so `getMotion6()` at line 79 already returns
offset-corrected values. Lines 82-87 then subtract the same six constants again in software. The Z axis is
the visible case: with the hardware offset applied, `az` should read ≈16384 at rest, and line 84 turns
that into ≈13764. Every sample this sketch prints is wrong by exactly one offset vector.

**5.4 `esp32_enterprise_wpa3_eap.ino`'s credential include is dead, and its credentials are placeholders.**
Line 2 does `#include "wifi_credentials.h"`, and then lines 4-6 `#define` `EAP_IDENTITY`,
`EAP_USERNAME`, `EAP_PASSWORD` to the literal strings `"username"`, `"username"`, `"password"` —
shadowing whatever the header provided. As committed, it authenticates as user "username". Meanwhile the
include still makes the sketch **fail to compile from a fresh clone**, because `wifi_credentials.h` is
gitignored and absent. Same fresh-clone compile failure applies to `esp32_ewpa2_iic_091.ino` (line 2),
which *does* use the header's macros properly (`WIFI_USER`, `WIFI_PASSWORD`, lines 4-6). Verified:
`git check-ignore -v wifi_credentials.h` → `.gitignore:1:**wifi_credentials.h`, and no such file exists
anywhere in the worktree.

**5.5 The scanner's WHO_AM_I probe reads the wrong register for this repo's own sensor.**
`i2c_scanner_whoami.ino:92` writes `0x0F` and calls it *"Common register for WHO_AM_I, used in some
sensors (e.g., accelerometers)"*. `0x0F` is the ST convention (LSM/LIS parts). **The MPU6050's WHO_AM_I is
`0x75`** — grounded in the library floppi vendors:
`flight_controller/lib/MPU6050/src/MPU6050.h:177` → `#define MPU6050_RA_WHO_AM_I 0x75`. So on the exact
sensor GravityProbe was built around, the "whoami" feature that gives the folder its name returns
garbage. The address-scan half is fine.

**5.6 `grocketBLE.m` reads two lines per iteration and drops every other sample.**
Lines 73-94 are a pasted duplicate of the read block, nested inside the outer `try`. Each pass through
`while true` therefore calls `readline(esp32)` at line 69 **and again** at line 76; the first line's
`values` is overwritten and discarded. The inner block's "process data" branch is a bare `...`
continuation stub (line 84) — it does nothing. At the stated 50 Hz that is a silent 50% sample loss.

**5.7 `grocketBLE.m`'s two analysis cells operate on scalars.**
Lines 168-215 and 217-265 are near-duplicate `%%` cells (one calls `ahrs(...)` functionally, the other
`update(ahrs, ...)` — two different API forms of the same idea). Both set
`numSamples = length(ax)`, but `ax` at that point is the **scalar** left over from the last loop iteration
at line 113, not a column vector. `numSamples` is 1. The comment at line 168 (*"Assuming the following
data are available as column vectors"*) admits the precondition that the file itself does not satisfy.

**5.8 `data_to_pos.m` silently writes only one of four quaternion components.**
Line 21: `quaternionTable = table(time, parts(quaternions));`. MATLAB's `parts()` returns four outputs
(w,x,y,z); called in a single-output context it yields only the first. So `quaternions.csv` contains time
and `w` alone. **This is corroborated inside the repo**: `plot_pos.m:31` reads
`q0 = quaternionData.Var2; % Use Var2 for quaternion data` — the author hit the truncation and worked
around it by plotting the one surviving component rather than fixing the write.

**5.9 `grocket.m`'s "3D Path of Travel" is not a path.**
Lines 76-78 plot `cumsum` of Earth-frame **acceleration** on all three axes and label the axes
"X (integrated ax)". One cumulative sum of acceleration approximates velocity, not position, and there is
no `dt` scaling anywhere (the samples are summed as if `dt = 1`). Similarly `data_to_pos.m` rotates the
fixed unit vector `[1 0 0]` by each quaternion (line 38) and writes the result as
`X_Position/Y_Position/Z_Position` into `rocket_position.csv` (lines 42-45) — that is an orientation
direction trace on the unit sphere, not a position. `plot_pos.m:22` titles the same data *"3D Orientation
Trajectory"*, so the author knew; the filename and column names did not get fixed.

**5.10 Cosmetic but propagated: the unique-filename comment is wrong in both SD sketches.**
`teensy_sdcard.ino:105` and `esp32_hw125_sd.ino:100` both promise `data000.csv, data001.csv, …`; the code
is `String("data") + String(fileIndex, DEC) + ".csv"` with no zero padding, producing `data0.csv`,
`data1.csv`. Files will sort lexicographically wrong past 9. `08_gravityprobe_recon.md` §3.4 and §3.12
repeat the comment's claim as fact — a good example of why the folder-name/comment audit was worth doing.

---

## 6. The duplication claim vs `~/floppi` — CHECKED, and it mostly holds

The routing brief asked whether `teensy_mpu6050`, `teensy_mpu6050_zero`, and `i2c_scanner_whoami`
duplicate floppi. Result: **two real overlaps and one non-duplicate.** (An earlier revision of this
section reported the opposite — "one near-overlap, two non-duplicates" — on the strength of a
symbol-scoped grep. Corrected below and in §4.7; the two MPU6050 rows changed verdict.)

| GravityProbe | Nearest floppi artifact | Real relationship | Better copy |
|---|---|---|---|
| `i2c_scanner_whoami/i2c_scanner_whoami.ino` (103 ln) | `auto_orientation/tests/i2c_scanner.ino` (54 ln) | **Not a duplicate — independent implementations.** `diff -u` shows no shared body; they overlap only in the `Wire.beginTransmission`/`endTransmission` idiom that every scanner on earth uses. | **Split.** floppi's is better structurally: scans `0..127` (GravityProbe scans `1..126`), sets `Wire.setClock(100000L)` to match the app, prints a 5-item wiring checklist on zero devices, and states the expected BNO085 address. GravityProbe's is better in exactly one way: it has the register-probe feature — which is broken (§5.5). **Recommendation: keep floppi's, and port the `getDeviceInfo()` idea across with the register made a parameter (0x75 for MPU, 0x0F for ST, 0x00 generic).** |
| `teensy_mpu6050/teensy_mpu6050.ino` | `flight_controller/src/imu.cpp` (382 ln, first-party) — **not** merely the vendored `lib/MPU6050/` | **Duplicate in capability.** `imu.cpp:168` calls the same `getMotion6()`, scales to g and °/s, subtracts stored offsets at line 196 and restores them from EEPROM at lines 141-151. An earlier revision of this row said floppi "has no app-level MPU6050 raw-read sketch"; that was wrong (§4.7). | **floppi's**, comfortably — it is integrated, EEPROM-backed and filtered. GravityProbe's is a 112-line demo and it carries §5.3 (double-applied offsets). |
| `teensy_mpu6050_zero/teensy_mpu6050_zero.ino` | `flight_controller/lib/Calibration/calibration_imu.cpp` (470 ln, first-party) | **Overlapping, not unique.** The symbol grep `grep -rl 'CalibrateAccel\|PullBracketsIn\|IMU_Zero'` across floppi hits `flight_controller/lib/MPU6050/src/MPU6050.h`, `…/src/MPU6050.cpp`, `flight_controller/lib/MPU6050/keywords.txt`, `dRehmFlight-master/code/src/MPU6050/MPU6050.h` and `…/MPU6050.cpp` — **not** `lib/MPU9250/`, which an earlier revision listed in error (re-verified: `lib/MPU9250/{MPU9250.cpp,MPU9250.h}` contain zero matches for all three symbols). More importantly, that grep proves nothing about capability: floppi's calibration runner is hand-rolled and shares none of those symbols (§4.7). | **Split.** floppi's `calibration_imu.cpp` covers the same ground in software, including a six-position routine. GravityProbe's unique part is *technique*: it programs the MPU6050's **hardware** offset registers by bracket search, which floppi never does. Worth porting as a technique; **not** an absent capability. |

Two further non-duplications worth recording, because they define what `sensor_interactions` would be
inheriting uniquely:
- **MATLAB:** zero `.m` files in floppi. `matlab_scripts/` is unique.
- **ESP8266:** no first-party ESP8266 firmware in floppi (all `ESP8266` grep hits are vendored library
  portability code). Unique — and low value, per §4.3.
- **GY-271 / QMC5883L magnetometer:** both GravityProbe docs give wiring and the QMC-vs-HMC clone gotcha,
  and **no GravityProbe sketch reads it.** floppi has the reverse: several docs discuss it
  (`auto_orientation/docs/findings/mpu6050_external_mag_pipeline.md`,
  `auto_orientation/docs/research/MPU6050_RESEARCH_COMPILATION.md`) but `grep` over `.h/.cpp/.ino` returns
  **no QMC5883/HMC5883 code either**. Neither estate has ever read that magnetometer. The `mx,my,mz`
  columns in the CSV schema were always aspirational.

---

## 7. ESP32 enterprise auth: GravityProbe vs `floppi/flight_controller` — floppi wins outright

This is the comparison the routing brief asked for, and it is not close.

| Dimension | GravityProbe (3 sketches, ~267 ln total) | `floppi/flight_controller` |
|---|---|---|
| Auth modes | PEAP only (WPA2). The two **commented-out** alternatives are PEAP-with-certs (`esp32_enterprise_wpa3_eap.ino:34`) and TLS (`:37`) — never exercised; this matches the §3 row. **No `WPA2_AUTH_TTLS` call exists anywhere in GravityProbe** — the token "TTLS" appears only inside prose comments at `esp32_enterprise_wpa3_eap.ino:30` and `esp32_ewpa2_iic_091.ino:46` (*"a cert-file-free eduroam with PEAP (or TTLS)"*). An earlier revision of this row said "TLS/TTLS present as commented-out lines", contradicting §3. | **Four**, compile-time exclusive: `WIFI_AUTH_MODE_OPEN` / `PSK` / `WPA3_SAE` / `ENTERPRISE` (`include/config.h:81-84`), dispatched in `src/wifi_connect.cpp:111-134`. |
| EAP methods | PEAP hardcoded | PEAP / TTLS / TLS selector (`config.h:87-89` → `wifi_connect.cpp:46-52`) |
| WPA3 | **name only** — the folder says `wpa3_eap`, no WPA3 token exists in the repo | real: `WiFi.setMinSecurity(WIFI_AUTH_WPA3_PSK)` before begin (`wifi_connect.cpp:118`) |
| Certificates | three commented-out `const char*` declarations, one commented `WiFi.begin` overload | live cert path gated on `USE_WIFI_CERTS`, with `static_assert(sizeof(WIFI_CA_CERT) > 1, …)` so an empty template PEM fails the **build**, not the field (`wifi_connect.cpp:32-40, 122-128`) |
| Anonymous outer identity | absent | `WIFI_EAP_ANON_IDENTITY` → separate outer/inner identity, with the privacy rationale in the comment (`wifi_connect.cpp:54-61`) |
| Misconfiguration handling | none — wrong config compiles and fails silently at 3 a.m. on a launch pad | **eight `#error` guards** in `include/wifi_config.h` at lines 70, 72, 78, 81, 85, 88, 94, 100: no-mode-selected, multiple-modes-selected, exactly-one-EAP-method, identity required, PEAP/TTLS require user+pass, EAP-TLS requires certs, `USE_WIFI_CERTS` set but `wifi_certs.h` not found, static IP requires all four values. (An earlier revision counted six and omitted the line-94 cert-file guard.) |
| Legacy migration | n/a | back-compat aliases with `#warning` deprecations for `WIFI_USE_ENTERPRISE` and `WIFI_EAP_AUTH_METHOD`, plus a zero-edit PSK default for old credential files (`wifi_config.h:31-66`) |
| Secret handling | one file has a real credential in git (§9); two sketches include a gitignored header that isn't there | `include/wifi_credentials.h` is a template — every secret macro present, all values placeholders, EAP block commented out; `__has_include` fallback to `"unconfigured"` (`wifi_connect.cpp:20-29`) |
| Reusability | copy the `.ino` | explicitly designed to lift: *"Depends on nothing project-specific… copy this pair plus a config.h selector + wifi_credentials.h"* (`include/wifi_connect.h:6-8`) |
| Non-obvious lesson captured | reboot-on-connect-timeout watchdog — ≈30 s in two sketches, ≈6 s in the third (§4.1) | hostname must be set **before** `WiFi.mode(WIFI_STA)` because core 3.x consumes it inside `mode()` (`wifi_connect.h:22-26`, `wifi_connect.cpp:93-101`) — a genuinely subtle Arduino-ESP32 3.x behaviour |
| Documentation | 3 lines in `esp32.md` (lines 4-6: a heading, a blank line, and one line carrying the library link) | 35 KB `docs/findings/esp32-wifi-connectivity.md` (9 sections incl. a dedicated WPA2-Enterprise/eduroam section) + a 13 KB adversarial QA verdict `docs/findings/wifi_modes_qa_2026-05-22.md` that reached **GO with one named minor bug** |

**Verdict: floppi's selector supersedes GravityProbe's WiFi work completely.** There is exactly **one**
thing GravityProbe has that floppi's does not: the empirical record that this PEAP approach associated
against **two different institutions' RADIUS servers**, and the crude
`counter >= 60 → ESP.restart()` recovery pattern — whose real timeout is ≈30 s in two sketches and ≈6 s
in the third, not the uniform 30 s the code comments claim (§4.1). Both are one sentence of findings
text, not code.

**Do not migrate any GravityProbe WiFi sketch into `sensor_interactions`.** They would be strictly worse
than what `flight_controller` already ships, they carry hardcoded institutional SSIDs, and one carries a
live credential.

---

## 8. Recommendation for `sensor_interactions` (the R7 successor)

`~/lowprofiledronegurus/sensor_interactions` is currently **empty** — one commit (`0608749 Initial
commit`), one file, and that file is the stock GitLab "Getting started" README. It genuinely needs
inheritance, so the selection below matters.

Extraction from GravityProbe is **allowed** (routing doc R6, "taking content OUT"), **selective**, and
**attributed**. Recommended split:

### CARRY (with provenance + the named fix applied first)
| Item | Land as | Why | Fix required before use |
|---|---|---|---|
| `teensy_mpu6050_zero/teensy_mpu6050_zero.ino` | `imu/mpu6050/calibration/` | A **technique** floppi does not use: it programs the MPU6050's on-chip **hardware** offset registers by bracket search, where floppi computes software offsets applied at `flight_controller/src/imu.cpp:196` (§4.7, §6). Board-agnostic despite the name. **This is a narrower justification than an earlier revision gave** ("no equivalent anywhere in floppi" — withdrawn; `flight_controller/lib/Calibration/calibration_imu.cpp` covers the same ground in software), so carry it as a technique reference, not as a missing capability. | Rename off `teensy_`. Optionally drop the redundant second method (lib `CalibrateAccel` **or** the bracket search, not both). |
| `teensy_mpu6050/teensy_mpu6050.ino` | `imu/mpu6050/raw_read/` | Tight pair with the above — it is the consumer of the offsets, and a standalone demo is what makes the calibration technique runnable. Not unique: `flight_controller/src/imu.cpp` is the better *production* reference (§6), it is just not a standalone sketch. | **Must fix §5.3 (double-applied offsets) or it ships wrong data.** Rename off `teensy_`. |
| The six captured offsets + the bracket-convergence table from `teensy4.0.md` | `imu/mpu6050/docs/calibration_walkthrough.md`, **verbatim** | The only measured data in the repo; per-device but a real worked example of what convergence looks like. | none — quote as-is, marked as one specific device |
| The 17-field telemetry line format from `grocketBLE.m:70,113-117` | `docs/telemetry_line_format.md` | A spec for on-board-fusion firmware that was designed and never built (§4.8). Cheap to record, expensive to re-derive. | note clearly that no producer exists |
| SD-logging **rules** from `teensy4.0.md` (open-once, flush-every-write) + the GPIO2 strapping-pin gotcha from `esp32_hw125_sd.ino:5` | `docs/findings/sd_logging_notes.md` | Two transferable hardware lessons, ~5 lines. | none |
| The U8G2 constructor line for the DSD Tech 0.91" SSD1306 | one line in the same findings doc | saves hours of constructor roulette | none |

### CITE ONLY (reference the path; do not copy the code)
| Item | Why not carried |
|---|---|
| `matlab_scripts/` (all 5) | Unique in the estate (§6) and conceptually the most interesting content here — but it has **no input data**, three competing fusion APIs, and four separate defects (§5.6-§5.9). Copying it into the successor imports four bugs and a dead pipeline. **Better: a `docs/findings/matlab_gravity_pipeline.md` describing the intended chain and the Earth-frame gravity-extraction idea, citing `~/GravityProbe/matlab_scripts/grocket.m` for anyone who wants the original.** Revisit if the operator ever produces real capture data. |
| `i2c_scanner_whoami/` | floppi's `auto_orientation/tests/i2c_scanner.ino` is better (§6). Port the *idea* of a parameterised register probe; do not copy this file. |
| `teensy_i2c_oled/` | Superseded by `flight_controller/src/display.cpp` (§4.5). The constructor line is the only durable part and it is carried above. |
| `esp32_hw125_sd/`, `teensy_sdcard/` | Superseded by `auto_orientation/src/file_system/sd_card.cpp`; the Teensy one is broken (§5.1) and neither ever logged a real sample. The two rules and the GPIO2 note are carried above. |
| `esp32.md`, `teensy4.0.md` wiring blocks | Useful as reference, but `esp32.md` contains a verified copy-paste error (§3) and the bare 39/42 pin numbers are unexplained. Extract the specific facts, not the file. |

### DROP (do not carry, do not cite as guidance)
- All three ESP32 WiFi sketches — superseded outright by floppi's selector (§7), and they carry hardcoded
  institutional SSIDs.
- Both ESP8266 sketches — no first-party ESP8266 anywhere in the active estate, the coprocessor
  architecture they imply was never built (no Teensy-side bridge exists), and the active line is ESP32-native
  and needs no coprocessor. One of the two carries a live credential (§9).
- `README.md`, `matlab.md`, `.gitignore`, `esp32_GPIO.jpg` — a two-line README, two links available from
  MathWorks, a 20-byte ignore rule, and a pinout image reproducible from Espressif in seconds.

**Net:** two sketches and about a page of findings text. Everything else is a citation. That is what the
routing doc's *"not everything from them is useful"* looks like when applied file by file.

---

## 9. Credential and attribution hygiene

**The known finding is confirmed, and it has already propagated.**

`~/GravityProbe/teensy_esp8266/teensy_esp8266.ino` lines 5-6 hold a real SSID and password. I read the
file through a mask that printed only structure: both lines are `const char*` string-literal
initialisers feeding `WiFi.begin(ssid, password)` at line 16. **The values are not
reproduced anywhere in this document.** (An earlier revision also recorded each line's byte length. That
is removed deliberately: the declaration prefixes are fixed and quoted above, so a line length discloses
the exact SSID and password *lengths*. Length is credential metadata, not structure.) Committed in `ceb4acd`, 2025-01-04, and reachable from
`origin/data-analysis`.

The repo's entire `.gitignore` is one line, `**wifi_credentials.h`. Verified with `git check-ignore -v`:
that pattern **does** cover `wifi_credentials.h` at any depth (git treats non-special consecutive
asterisks as a plain `*`, and a pattern with no `/` matches at any level), and it **does not** cover
`teensy_esp8266.ino`. The ignore rule was never the wrong pattern — the credential was simply put in a
different file.

> **CORRECTION — it did *not* propagate.** An earlier revision of this section claimed that
> `~/floppi/temp_reorg/08_gravityprobe_recon.md` reproduces the SSID **and the password in cleartext** in
> three places. **That claim is false and is withdrawn.** Re-verified by exact substring search of both
> literals (extracted from the source line into a shell variable, never printed) against every file under
> `~/floppi`: **zero matches**, case-sensitive and case-insensitive. The recon document quotes the
> declaration with placeholder tokens — `<REDACTED-SSID>` and `<REDACTED-PASSWORD>` at its lines **203,
> 316 and 399** (§3.7, §4.4, §6) — i.e. it flagged the credential exactly as this document does. **The
> live credential is confined to `~/GravityProbe`**: one file, one commit (`ceb4acd`), reachable from
> `origin/data-analysis`. Handling it there is operator-only work (git mutations are human-only); I am
> flagging, not acting. Per routing doc §4 the WiFi-security topic is **parked**, so this is recorded as
> information, not raised as a gate.

**Hardcoded SSIDs — measured, not assumed.** Five sketches assign an SSID. Counting distinct quoted
literals across all `.ino` files gives **exactly 3**: **two** institutional networks (one appearing in
both `esp32_enterprise_wpa3_eap.ino:7` and `esp32_ent_wpa2_peap_web80.ino:8`, a second in
`esp32_ewpa2_iic_091.ino:10`) plus the private network in `teensy_esp8266.ino:5` (§9, the credential
file). The fifth, `teensy_esp8266_wifi_home.ino:9`, resolves an `STASSID` macro whose value is the
placeholder `"your-ssid"` — clean. The institutional SSIDs are not secrets, but together they are an
attribution trail naming the author's institutions. I have deliberately not listed them; they are one
`grep -rn 'ssid *=' ~/GravityProbe --include=*.ino` away. **If any WiFi sketch is ever republished, strip
them** — which §8 makes moot by dropping all five WiFi sketches.

> **Correction to the briefing.** The reorg brief lists a **third** institutional SSID
> (`<institutional SSID C>` — the guest network named in the brief; not reproduced here, for the same
> attribution reason as the other two) among the SSIDs appearing "across the repo". **It does not appear
> in GravityProbe at all** — grepping its distinctive token over every `.ino` and `.md` in the tree
> returns no match. Only two institutional SSIDs are present here. If that third one is real it lives in
> one of the other legacy repos, not this one.

**Attribution:** two identities in this repo's history, `msc_intra` (3 commits, the initial scaffold and
the ESP32/MATLAB seed) and `gndpwnd` (3 commits, all the "works" commits). Both belong in any
`PROVENANCE.md` for content landed in `sensor_interactions`.

---

## 10. What is *not* here (so nobody hunts for it)

- **No captured data at all.** Zero `.csv`/log files in the worktree. The gravitational experiment this
  repo is named for has no dataset in it.
- **No integrated payload.** The IMU sketch prints to Serial; the SD sketch logs `random()`. **They were
  never joined.** There is no sketch anywhere that reads a real sensor and writes it to a card.
- **No magnetometer code**, despite `mx,my,mz` in the CSV schema and wiring for the GY-271/QMC5883L in both
  docs (§6).
- **No Teensy-side code for the ESP8266 bridge** — no UART protocol, no framing, no AT layer (§4.3).
- **No barometer, no GPS, no telemetry radio, no recovery/deployment logic** — nothing a real sounding-rocket
  payload needs beyond the IMU.
- **No firmware producing the 17-field line `grocketBLE.m` parses** (§4.8).
- **No build system.** No `platformio.ini`, no `.vscode`, no CI. Arduino IDE, folder-per-sketch.
- **No MATLAB codegen output**, despite `matlab.md` linking the codegen docs.

Read together: GravityProbe is a set of **subsystem bring-ups that were never integrated**, plus an
**analysis pipeline with no data**, and the two halves were designed against **different data formats**
(10 fields vs 17). The project stopped at the point where integration would have started.

---

## 11. What I did not read / could not verify

- **`esp32_GPIO.jpg`** — listed in `git ls-tree` (99,748 bytes) and assumed from its filename to be an
  ESP32 pinout image. **Not decoded, not viewed.** Its content is unverified.
- **`.git` internals beyond `log`/`ls-tree`/`show`/`check-ignore`/`status`.** I did not diff historical
  revisions of any file, so "committed in X" reflects the commit that touched the path, not a per-line
  blame.
- **`wifi_credentials.h`** — absent from the worktree by design. I do not know which macros it defined
  beyond the two used at `esp32_ewpa2_iic_091.ino:4-6` (`WIFI_USER`, `WIFI_PASSWORD`).
- **Nothing was compiled or run.** No Arduino build, no MATLAB. Every §5 defect is a code-reading claim.
  In particular I did **not** confirm that MATLAB's `orientationSensor` object exists in any toolbox
  release — I only observed that `grocket.m` is the sole user of it and that `matlab.md` links neither.
  Similarly, the exact runtime behaviour of the `...` stub at `grocketBLE.m:84` was reasoned about, not
  executed; the double-`readline` at lines 69/76 is the solid half of §5.6.
- **floppi comparison depth is bounded — and it was too shallow in one place.** The §4.7/§6 duplication
  verdict was originally reached by symbol-scoped grep alone, without opening floppi's own IMU code; the
  corrected version cites `flight_controller/src/imu.cpp`, `lib/Calibration/calibration_imu.cpp`,
  `src/calibration_mode.cpp` and `lib/CalibrationStorage/calibration_storage.h`, which I have since read
  at the cited lines (not in full). Otherwise I read `wifi_config.h`, `wifi_connect.h`,
  `wifi_connect.cpp`,
  the WiFi block of `config.h`, `auto_orientation/tests/i2c_scanner.ino`, and the section headings (not
  bodies) of `flight_controller/docs/findings/esp32-wifi-connectivity.md` (35 KB) and
  `wifi_modes_qa_2026-05-22.md` (13 KB). I did **not** read `wifi_manager.cpp`, `display.cpp` beyond its
  constructor block, or `sd_card.cpp` beyond its `SD.begin` line — sizes and API calls are grepped facts,
  not full reviews. I did not read `flight_controller/include/wifi_credentials.h`'s values (only its macro
  names, via a redacting filter, to confirm it is a template).
- **`08_gravityprobe_recon.md`** — read §1 through §7.1 (the first ~420 of its lines). I did not read its
  tail sections, so if it already records any §5 defect later in the file, I have duplicated rather than
  contradicted it.
- **Remote state unverified.** I did not contact GitHub. Whether `origin/data-analysis` still carries the
  credential *right now* is inferred from the local ref, not confirmed against the remote.
