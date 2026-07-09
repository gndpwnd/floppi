# 08 — GravityProbe folder recon + contents-based routing recommendations

**Status:** 2026-07-09 recon pass. Read-only inspection of `~/GravityProbe/` (data-analysis branch), the confirmed third source repo for the reorg into the flat `lowprofiledronegurus` GitLab group.
**Scope:** Every top-level folder + every top-level doc + representative file reads for each subfolder. Every recommendation is grounded in an observed filename, README fragment, or file body.
**Not in scope:** copying, moving, git operations, GitLab CRUD, provenance tracking (user has dropped SOURCE_MAP per-mini).

Cross-reference:
- `00_reorg_master_plan.md` — locked repo list
- `01_target_repos_v2.md` — per-repo cards
- `03_folder_recon_findings.md` — floppi + SwarmLoc equivalent (this doc mirrors its shape)
- `04_mini_project_setup_guide.md` — naming conventions used below (`lowercase_snake_case`, `docs/{scope,roadmap,todo,features/,findings/,archive/}` bootstrap tree)

Target taxonomy (unchanged from 03):

- `auto_orientation_research` (IMU/mag/orientation math + fusion, domain-agnostic)
- `flight_controller` (FC firmware — not a target for GravityProbe content, but named for exclusion)
- `sensor_interactions` (**per user quote:** display/hardware POCs land here, incl. IMU read + SD logging + I2C bring-up)
- `networking_pocs` (**per user quote:** ESP32 commercial WiFi / networking POCs land here)
- `research` (literature + mini research projects + top-level README fragment routing)
- `communication_hardware`, `swarm_communication_protocol`, `swarm_api`, `position_denial_research`, `drone_frame_modeling`, `fc_tool`, `darpa_lift_2026`, `cybersecurity_demos` — none of these are primary targets for GravityProbe content, but listed for completeness.

---

## 1. Branch verification

**Path:** `/home/devel/GravityProbe/`
**Current branch:** `data-analysis` (matches user's stated third-source expectation)
**HEAD:** `1e0c086 enterprise wpa2 peap on esp32 works`
**Working tree:** clean (`nothing to commit, working tree clean`)
**Tracking:** `Your branch is up to date with 'origin/data-analysis'.`
**All branches present:**
- `* data-analysis` (checked out)
- `main`
- `remotes/origin/HEAD -> origin/main`
- `remotes/origin/data-analysis`
- `remotes/origin/main`

**Verdict:** ON EXPECTED BRANCH. Proceeding.

**READ-ONLY discipline observed:** No `git checkout`, no `git switch`, no `git stash`, no writes of any kind performed against `~/GravityProbe/` during recon.

---

## 2. Top-level layout

`ls -la ~/GravityProbe/` (excluding `.git` and dotfiles):

**Folders (12):**
- `esp32_enterprise_wpa3_eap/` — 1 `.ino`
- `esp32_ent_wpa2_peap_web80/` — 1 `.ino`
- `esp32_ewpa2_iic_091/` — 1 `.ino`
- `esp32_hw125_sd/` — 1 `.ino`
- `i2c_scanner_whoami/` — 1 `.ino`
- `matlab_scripts/` — 5 `.m` files
- `teensy_esp8266/` — 1 `.ino`
- `teensy_esp8266_wifi_home/` — 1 `.ino`
- `teensy_i2c_oled/` — 1 `.ino`
- `teensy_mpu6050/` — 1 `.ino`
- `teensy_mpu6050_zero/` — 1 `.ino`
- `teensy_sdcard/` — 1 `.ino`

**Top-level docs / assets (5):**
- `README.md` — 100 bytes. Full body: `"# GravityProbe\nCode for NASA RockSat-C experiment to verify newton's theory of gravitational change"`
- `esp32.md` — ~2 KB. Board select + WPA2-EAP notes + OLED (U8G2) wiring + SD (HW125) wiring + MPU6050 wiring + GY-271 QMC5883L magnetometer notes (I2C-address gotcha for QMC vs HMC clones).
- `matlab.md` — ~200 bytes. Two links: MATLAB Sensor Fusion Toolbox docs + C++ Code Generation docs.
- `teensy4.0.md` — ~4 KB. I2C scanner usage + MPU6050 offsets/calibration walkthrough (with real captured offsets: `accelXOffset=-4347`, `accelYOffset=-351`, `accelZOffset=2620`, `gyroXOffset=43`, `gyroYOffset=82`, `gyroZOffset=29`) + GY-271 QMC5883L + HW125 SD + OLED + ESP8266 wifi module notes.
- `esp32_GPIO.jpg` — ~100 KB reference image (ESP32 pinout).

**Meta:** `.gitignore` (20 bytes), `.git/`.

**Shape observation:** flat single-level structure. No nested projects. Twelve small POC sketches (11 of the 12 folders contain exactly one `.ino`; `matlab_scripts/` is the outlier with five `.m` files). This is a bring-up / POC playground, not a multi-project monorepo. Each folder is essentially one Arduino/PlatformIO sketch's worth of code.

**Repo purpose (from README + folder pattern):** NASA RockSat-C rocket payload prototypes for verifying gravitational-change theory — logging IMU + magnetometer + acceleration data during a sounding-rocket flight, plus post-flight MATLAB analysis to derive orientation quaternions and Earth-frame accelerations. Split across two MCU platforms (ESP32 + Teensy 4.0) with parallel bring-up sketches per subsystem.

---

## 3. Per-folder deep recon

### 3.1 `esp32_enterprise_wpa3_eap/`

**Path:** `/home/devel/GravityProbe/esp32_enterprise_wpa3_eap/esp32_enterprise_wpa3_eap.ino` (3.7 KB)

**Contents:** Arduino-IDE-style `.ino` derived from `WiFiClientEnterprise` example. `#include "wifi_credentials.h"` (external secret header, not present in tree — see §4.3). SSID `"ERAUStudents"`, connects via `WPA2_AUTH_PEAP` with `EAP_IDENTITY` / `EAP_USERNAME` / `EAP_PASSWORD` macros. On success fetches `wifitest.adafruit.com/rele/rele1.txt` over HTTP :80. 30-second timeout → `ESP.restart()`. Loop retries on disconnect.

**Primary purpose:** ESP32 WPA2 Enterprise (PEAP) client bring-up against a university-network (Embry-Riddle) with a simple HTTP GET as the "connection works" proof.

**FOLDER NAME vs. CODE MISMATCH:** folder says `wpa3_eap` but the code uses `WPA2_AUTH_PEAP`. This is WPA2, not WPA3. See §4.1.

**Recommended target:** `networking_pocs/esp32_wpa2_eap_client/`
**Alternative:** none. Clean WiFi POC — user quote routes this here directly.
**Confidence:** HIGH.
**Stay in GravityProbe:** none. Migrate. `wifi_credentials.h` will need to be re-recreated as a template header in the new mini-project's `.gitignore`d local file.

---

### 3.2 `esp32_ent_wpa2_peap_web80/`

**Path:** `/home/devel/GravityProbe/esp32_ent_wpa2_peap_web80/esp32_ent_wpa2_peap_web80.ino` (2.3 KB)

**Contents:** ESP32 WPA2-PEAP client (same `ERAUStudents` SSID pattern) that ALSO hosts a `WiFiServer server(80)` after connect and serves `"<html><body><h1>Hello World!</h1>...</body></html>"` on `GET / `. Credentials are inline `#define` placeholders (`"user"`/`"pass"`), no external header include.

**Primary purpose:** Combined WPA2-Enterprise client + tiny web server POC. Adds the "device is reachable and serves HTTP" half on top of §3.1's "device connects" half. This is the working sketch that HEAD's commit message (`enterprise wpa2 peap on esp32 works`) refers to.

**Recommended target:** `networking_pocs/esp32_wpa2_peap_web_server/`
**Alternative:** merge into the same mini as §3.1 (`esp32_wpa2_eap_client/` with a `variants/web_server/` subfolder). Recommend keeping split — they're independently interesting.
**Confidence:** HIGH.
**Stay in GravityProbe:** none.

---

### 3.3 `esp32_ewpa2_iic_091/`

**Path:** `/home/devel/GravityProbe/esp32_ewpa2_iic_091/esp32_ewpa2_iic_091.ino` (4.4 KB)

**Contents:** ESP32 WPA2-PEAP client (SSID `"UAA WiFi -MatSu"`, different institution than §3.1 — University of Alaska Anchorage MatSu) that ALSO drives a 0.91" SSD1306 I2C OLED to display the acquired IP address via U8G2 (`u8g2_font_logisoso28_tr`). Includes `wifi_credentials.h` (see §4.3). Same "GET wifitest.adafruit.com/rele/rele1.txt" health check as §3.1.

**Primary purpose:** WiFi client + on-device IP display — hybrid networking + display POC. Cross-cutting.

**Recommended target:** `networking_pocs/esp32_wpa2_client_oled_ip_display/`
**Rationale:** primary novelty vs. §3.1/§3.2 is the on-device display of the network state — still fundamentally a networking POC in shape.
**Alternative:** split into (a) `networking_pocs/esp32_wpa2_client_multi_ssid/` (drops OLED code) and (b) `sensor_interactions/esp32_ssd1306_oled/` (extract just the display driver setup). Not recommended — the sketch is small enough that splitting destroys context.
**Confidence:** MED (routing is HIGH; the mini-project name choice is MED — see §7).
**Stay in GravityProbe:** none.

---

### 3.4 `esp32_hw125_sd/`

**Path:** `/home/devel/GravityProbe/esp32_hw125_sd/esp32_hw125_sd.ino` (3.1 KB)

**Contents:** ESP32 + HW125 SPI SD card module. Auto-generates unique filenames (`data000.csv`, `data001.csv`, …), writes CSV header `t,ax,ay,az,gx,gy,gz,mx,my,mz`, then logs at 1 Hz. Sensor values are SIMULATED (`random(-10, 10) / 10.0`) — placeholder for future integration with real IMU + magnetometer. `chipSelect = 8`, comment warns "gpio2 might cause issues when flashing." `flush()` after every write. File stays open — no close in loop.

**Primary purpose:** SD-card data-logging harness — the "storage half" of the rocket payload. Ready to accept real sensor plumbing.

**Recommended target:** `sensor_interactions/esp32_hw125_sd_logger/`
**Rationale:** SD card is a hardware peripheral (sensor-adjacent I/O). User quote explicitly routes display/hardware POCs to `sensor_interactions/`; SD interface fits.
**Alternative:** `sensor_interactions/sd_logging_framework/esp32_hw125/` if a shared "SD logging" umbrella emerges combining §3.4 + §3.12.
**Confidence:** HIGH (target repo). MED (naming — see §7).
**Stay in GravityProbe:** none.

---

### 3.5 `i2c_scanner_whoami/`

**Path:** `/home/devel/GravityProbe/i2c_scanner_whoami/i2c_scanner_whoami.ino` (2.8 KB)

**Contents:** Classic Arduino i2c_scanner (public-domain provenance noted in-file: "Version 1... found in many places... Arduino.cc forum... original author is not known"), extended with a `getDeviceInfo(address)` helper that reads register `0x00` and probes the WHO_AM_I register at `0x0F`. Iterates `address = 1..126`, reports found devices with byte-of-info.

**Primary purpose:** Generic I2C bring-up utility — figure out addresses of new sensors during hardware bring-up. Not tied to any specific board.

**Recommended target:** `sensor_interactions/i2c_scanner_whoami/`
**Rationale:** Diagnostic tool used across ALL i2c sensor bring-up work. Belongs in the sensor_interactions umbrella as a shared utility.
**Alternative:** place under a `sensor_interactions/_utilities/` bucket if a general utility zone is established.
**Confidence:** HIGH.
**Stay in GravityProbe:** none. But note this utility likely duplicates something floppi already has (see §4.5). Recon of floppi in `03_folder_recon_findings.md` should be cross-checked before migrating.

---

### 3.6 `matlab_scripts/`

**Path:** `/home/devel/GravityProbe/matlab_scripts/` (5 `.m` files)

**File-by-file:**

- `ex_data.m` (490 B) — Generates synthetic 0..10s sensor data (sin/cos accel/gyro/mag) and writes `data.csv` with columns `time,ax,ay,az,gx,gy,gz,mx,my,mz`. Test-fixture generator.
- `data_to_pos.m` (1.6 KB) — Reads `data.csv`, runs MATLAB `complementaryFilter` to derive quaternions, converts to Euler `ZYX`, rotates a unit vector by each quaternion to build a 3D trajectory, writes `quaternions.csv` and `rocket_position.csv`. **This is orientation/quaternion sensor fusion.**
- `plot_pos.m` (1.1 KB) — Reads `rocket_position.csv` + `quaternions.csv` and produces 3D trajectory plot + q0 time-series plot. Pure visualization.
- `grocket.m` (2.9 KB) — Reads `gdata.csv` (real flight data), runs `orientationSensor` object to derive quaternion per row, applies `quat2rotm` to rotate the accel vector into the Earth frame, extracts the Earth-Z component as gravity, plots rocket-frame vs. Earth-frame accelerations + 3D `cumsum` integrated path + gravity component. **This is the payload experiment's post-flight analysis.**
- `grocketBLE.m` (8.8 KB) — Live serial reader (COM36 @ 115200) that parses 17-column CSV telemetry (`t, ax, ay, az, wx, wy, wz, mx, my, mz, qx, qy, qz, qw, earthX, earthY, earthZ`), maintains rolling 100-sample buffers, and updates four real-time tiled plots (accel / gyro / mag / earth-accel). Contains AHRS filter (`ahrsfilter`) processing at the bottom too. **Live-visualization tool.** BLE in the name is misleading — it's serial-over-USB, not Bluetooth.

**Primary purpose:** Post-experiment (and live) MATLAB analysis: turn raw IMU+mag telemetry into orientation quaternions, then into Earth-frame acceleration + trajectory + gravity extraction. This is the actual gravitational-verification pipeline.

**Recommended target:** `auto_orientation_research/matlab_sensor_fusion/`
**Rationale:** User quote: "Quaternion / math code → auto_orientation_research/<subname>/". Literal match. `orientationSensor`, `ahrsfilter`, `complementaryFilter`, quaternion→Euler, `quat2rotm`, Earth-frame rotation — all core AHRS material.
**Sub-structure suggestion inside the mini:**
- `matlab_scripts/data_to_pos.m` + `plot_pos.m` + `ex_data.m` — offline pipeline (complementary filter)
- `matlab_scripts/grocket.m` — offline pipeline (orientationSensor + gravity extraction)
- `matlab_scripts/grocketBLE.m` — live pipeline (serial + ahrsfilter, real-time plots)
- Add a `README.md` explaining the two pipelines and the file dependencies (`ex_data.m → data.csv → data_to_pos.m → {quaternions.csv, rocket_position.csv} → plot_pos.m`).
**Alternative:** `auto_orientation_research/rocksat_c_analysis/` if the payload context is worth preserving in the mini-project's identity.
**Confidence:** HIGH.
**Stay in GravityProbe:** none. The `.csv` fixture files that these scripts read (`data.csv`, `gdata.csv`, `quaternions.csv`, `rocket_position.csv`) are NOT present in the tree — they're generated at runtime or came from a specific flight. If any historical `gdata.csv` exists elsewhere (e.g. captured flight data), that would migrate as `auto_orientation_research/matlab_sensor_fusion/data/` — but I did not find any.

**BLE naming caveat:** the file name `grocketBLE.m` is misleading. Body uses `serialport(comPort='COM36', baudRate=115200)` — plain USB serial, not Bluetooth. Rename to `grocket_live_serial.m` during migration.

---

### 3.7 `teensy_esp8266/`

**Path:** `/home/devel/GravityProbe/teensy_esp8266/teensy_esp8266.ino` (1.0 KB)

**Contents:** ESP8266 (not the Teensy itself despite folder name) `ESP8266WebServer` on port 80. Hard-coded SSID `"Brown_Bear"` + password `"5n!cKer*"` (home wifi — see §4.4). Serves `"Hello, World!"` at `/`.

**Primary purpose:** ESP8266 home-WiFi web-server hello-world bring-up. Simpler than §3.2. Note: this is the ESP8266 side of the Teensy-plus-ESP8266 combo where the ESP8266 acts as a WiFi coprocessor for the Teensy 4.0 (which lacks built-in WiFi).

**FOLDER NAME MISLEADING:** folder says `teensy_esp8266` but this sketch runs entirely on the ESP8266. There is no Teensy code in it. See §4.2.

**Recommended target:** `networking_pocs/esp8266_home_wifi_hello_world/`
**Alternative:** merge with §3.8 into a single `esp8266_home_wifi_server/` mini-project (they're nearly identical).
**Confidence:** HIGH.
**Stay in GravityProbe:** none. **CREDENTIAL LEAK:** hard-coded SSID + password in git history. Migration should replace with `#include "wifi_credentials.h"` pattern (§3.1) — see §6.

---

### 3.8 `teensy_esp8266_wifi_home/`

**Path:** `/home/devel/GravityProbe/teensy_esp8266_wifi_home/teensy_esp8266_wifi_home.ino` (1.9 KB)

**Contents:** ESP8266 `WiFiServer server(80)` (raw, not `ESP8266WebServer` — a lower-level pattern than §3.7). Uses `STASSID`/`STAPSK` `#define` template pattern (placeholders `"your-ssid"`/`"your-password"`). Reads request until `\r`, prints client IP, responds with an `<html>Hello, World!</html>` page and closes.

**Primary purpose:** Same functional shape as §3.7 but using the raw WiFiServer API instead of ESP8266WebServer. Illustrative alternative.

**FOLDER NAME MISLEADING:** same "Teensy" misnomer as §3.7. ESP8266-only.

**Recommended target:** `networking_pocs/esp8266_home_wifi_hello_world/` — MERGE with §3.7 as two variants (`webserver_api/` vs `raw_wifiserver/`).
**Alternative:** keep as separate mini-project `esp8266_wifi_raw_server/`. Not recommended — thin content.
**Confidence:** HIGH (routing). MED (merge-vs-split).
**Stay in GravityProbe:** none.

---

### 3.9 `teensy_i2c_oled/`

**Path:** `/home/devel/GravityProbe/teensy_i2c_oled/teensy_i2c_oled.ino` (1.2 KB)

**Contents:** Teensy 4.0 + SSD1306 128×32 OLED via U8G2 (`U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C` constructor, SW I2C on pins SCL/SDA — `teensy4.0.md` says pins 19/18). Draws a "Loading:" label + percentage + loading bar (empty frame + filled box) that fills from 0 to 100% over 5 seconds.

**Primary purpose:** SSD1306 OLED bring-up + drawing primitives POC on Teensy 4.0.

**Recommended target:** `sensor_interactions/teensy_ssd1306_oled_loading_bar/`
**Rationale:** display hardware bring-up — direct match to user's "display/hardware POCs → sensor_interactions" quote.
**Alternative:** `sensor_interactions/ssd1306_oled/teensy_variant/` if a cross-MCU OLED umbrella emerges (also housing the OLED half of §3.3).
**Confidence:** HIGH.
**Stay in GravityProbe:** none.

---

### 3.10 `teensy_mpu6050/`

**Path:** `/home/devel/GravityProbe/teensy_mpu6050/teensy_mpu6050.ino` (3.6 KB)

**Contents:** Teensy 4.0 + MPU6050 raw accel/gyro read via `ElectronicCats/mpu6050` + `jrowberg/i2cdevlib` libraries. Applies hard-coded per-device offsets (`accelXOffset=-4347`, etc. — same values as `teensy4.0.md`), initializes MPU, tests connection, then reads and prints `ax ay az gx gy gz` at loop speed with `OUTPUT_READABLE_ACCELGYRO` `#define` toggling readable vs binary output.

**Primary purpose:** MPU6050 IMU bring-up + raw read POC on Teensy 4.0, with calibration offsets pre-applied.

**Recommended target:** `sensor_interactions/teensy_mpu6050_raw_read/`
**Rationale:** IMU sensor bring-up = hardware POC. User quote routes hardware POCs to `sensor_interactions/`.
**Alternative:** could plausibly route to `auto_orientation_research/imu_raw_capture/teensy_mpu6050/` since the whole GravityProbe project's purpose IS orientation. However, the sketch itself does no math — it's a driver bring-up. Keep in `sensor_interactions/`; the orientation math lives in §3.6.
**Confidence:** HIGH (target repo). See §4.5 — likely duplicate of floppi's MPU6050 FC code, needs cross-check.
**Stay in GravityProbe:** none.

---

### 3.11 `teensy_mpu6050_zero/`

**Path:** `/home/devel/GravityProbe/teensy_mpu6050_zero/teensy_mpu6050_zero.ino` (8.4 KB)

**Contents:** IMU Zero — a PID-style calibration routine that finds accel + gyro offsets by binary-search-bracketing. `PullBracketsOut()` expands until brackets straddle target, `PullBracketsIn()` binary-searches inward. `NFast=1000`, `NSlow=10000` samples per averaging pass. `Target[iAz]=16384` (1 g at MPU6050 default ±2 g range). Prints the six offsets in the format shown in `teensy4.0.md` (from the same PID loop). This is what produced the hard-coded offsets in §3.10.

**Primary purpose:** MPU6050 calibration routine (the "get the numbers you'll bake into the raw-read sketch" utility). Board-agnostic if MPU6050 lib is present.

**Recommended target:** `sensor_interactions/teensy_mpu6050_calibration/`
**Alternative 1:** merge with §3.10 into `sensor_interactions/teensy_mpu6050/` with subfolders `raw_read/` and `calibration/`. **RECOMMENDED** — these are a tight pair.
**Alternative 2:** route to `auto_orientation_research/imu_calibration/mpu6050/` — the offsets ARE orientation-critical. Weaker: calibration is a hardware/driver concern, not a math concern.
**Confidence:** HIGH (target repo). Recommend the merge.
**Stay in GravityProbe:** none.

---

### 3.12 `teensy_sdcard/`

**Path:** `/home/devel/GravityProbe/teensy_sdcard/teensy_sdcard.ino` (3.2 KB)

**Contents:** Teensy 4.0 + SD card via `Sd2Card`/`SdVolume`/`SdFile` + `SPI` libraries. Same unique-filename pattern as §3.4 (`data000.csv`, ...). Same CSV header (`t,ax,ay,az,gx,gy,gz,mx,my,mz`). Same SIMULATED sensor data (`random(-10,10)/10.0`). `chipSelect = 10`. `card.init(SPI_HALF_SPEED, chipSelect)` — note this uses the low-level `card.init` rather than `SD.begin` (per `teensy4.0.md`: "card.init() not the function SD.init()").

**Primary purpose:** Teensy-side counterpart of §3.4 — SD card data-logging harness on the other MCU platform. Ready for real sensor plumbing.

**Recommended target:** `sensor_interactions/teensy_sdcard_logger/`
**Alternative 1:** merge with §3.4 under `sensor_interactions/sd_logging/` with `esp32_hw125/` and `teensy/` variants — RECOMMENDED, given their structural parallel.
**Alternative 2:** route with the IMU code (§3.10/§3.11) into `sensor_interactions/rocksat_payload_teensy/` if reconstructing the payload's full logging stack is desired.
**Confidence:** HIGH (target repo). MED (merge-vs-split with §3.4).
**Stay in GravityProbe:** none.

---

## 4. Cross-cutting or ambiguous folders (and other flags)

### 4.1 Folder-name lies about WiFi generation (§3.1)

`esp32_enterprise_wpa3_eap/` claims WPA3 but the code uses `WPA2_AUTH_PEAP`. The Arduino-ESP32 core does not yet expose WPA3-Enterprise EAP through the `WiFi.begin(ssid, WPA2_AUTH_PEAP, ...)` API — this is a WPA2 project. When migrating to `networking_pocs/`, rename to `esp32_wpa2_eap_client/` and add a note in the mini's scope doc that the WPA3 name was aspirational.

### 4.2 Folder-name lies about MCU (§3.7, §3.8)

`teensy_esp8266/` and `teensy_esp8266_wifi_home/` both contain 100 % ESP8266 code. The Teensy is presumably wired up (UART bridge?) but no Teensy sketch appears in-tree. When migrating, drop the `teensy_` prefix — the artifacts are ESP8266-only. If the Teensy-side bridge sketch is discovered later (perhaps in floppi or another branch), it can be added as a sibling.

### 4.3 External credential header `wifi_credentials.h`

Referenced by §3.1 and §3.3 as `#include "wifi_credentials.h"`. Not present in `~/GravityProbe/` (and shouldn't be — it holds secrets). The `.gitignore` (not shown) presumably lists it. Migration should carry over the pattern:
- add `wifi_credentials.h.template` to the new mini-project with `#define WIFI_USER "..."` / `#define WIFI_PASSWORD "..."` stubs
- add `wifi_credentials.h` to the new repo's `.gitignore`
- document in the mini's `docs/README.md`

### 4.4 Hardcoded credentials in-tree (§3.7)

`teensy_esp8266.ino` contains `const char* ssid = "Brown_Bear"; const char* password = "5n!cKer*";` — a live home WiFi credential is in git history. When migrating, replace inline with the `wifi_credentials.h` pattern. This is a low-severity leak (private WLAN, no PII), but it's worth noting and NOT reproducing in the new repo. See §6.

### 4.5 Likely duplicate content vs floppi (§3.10, §3.11, §3.5)

`03_folder_recon_findings.md` documents floppi's flight-controller MPU6050 work; the MPU6050 zeroing / raw-read POCs here almost certainly predate or overlap with that work. Same story for the I2C scanner (§3.5). Before migrating:
- Compare `teensy_mpu6050.ino` here vs any MPU6050 code in `~/floppi/flight_controller/` — pick the more-evolved version, note the other as a POC ancestor in `docs/archive/`.
- Compare `i2c_scanner_whoami.ino` here vs any scanner in `~/floppi/` — pick one canonical copy; user quote elsewhere confirms no per-mini SOURCE_MAP, so pick and move on.

### 4.6 Sketches that could plausibly go to 2+ repos

| Sketch | Primary route | Plausible alternate | Chosen because |
|---|---|---|---|
| `matlab_scripts/grocketBLE.m` | `auto_orientation_research/matlab_sensor_fusion/` | `research/mini_projects/rocksat_c_live_telemetry/` | User quote: quaternion/math → auto_orientation_research |
| `esp32_ewpa2_iic_091/` (§3.3) | `networking_pocs/…_oled_ip_display/` | split (`networking_pocs/` + `sensor_interactions/`) | User quote: ESP32 WiFi POC → networking_pocs (primary novelty is WiFi + IP display) |
| `teensy_mpu6050/` (§3.10) | `sensor_interactions/…_mpu6050_raw_read/` | `auto_orientation_research/imu_raw_capture/` | Bring-up = sensor_interactions; math = auto_orientation |
| `teensy_mpu6050_zero/` (§3.11) | `sensor_interactions/…_calibration/` | `auto_orientation_research/imu_calibration/` | Calibration is a hardware step, not a math step |

### 4.7 Research-y vs POC — nothing purely research-y here

Unlike floppi/SwarmLoc, GravityProbe contains **no theory PDFs, no findings docs, no scope/roadmap files, no archive/**. Every artifact is either a bring-up sketch or a MATLAB analysis script. There is no `research/` payload to extract. The top-level README's single sentence about "verify newton's theory of gravitational change" is the only theoretical framing, and it fits better as a paragraph in the `auto_orientation_research/matlab_sensor_fusion/README.md` context section than as its own doc.

### 4.8 Top-level docs routing

- `README.md` (100 B) → don't migrate as-is (too thin). Fold its one sentence into `auto_orientation_research/matlab_sensor_fusion/docs/scope.md` under "Historical context: NASA RockSat-C payload".
- `esp32.md` (2 KB, ESP32 wiring + peripherals) → split by peripheral:
  - WPA2-EAP notes → `networking_pocs/esp32_wpa2_eap_client/docs/references.md`
  - OLED U8G2 wiring → `networking_pocs/esp32_wpa2_client_oled_ip_display/docs/wiring.md`
  - SD HW125 wiring → `sensor_interactions/esp32_hw125_sd_logger/docs/wiring.md`
  - MPU6050 wiring → (Teensy variant lives in sensor_interactions; note the ESP32 MPU wiring in `sensor_interactions/imu/mpu6050/docs/esp32_wiring.md` if an umbrella emerges — otherwise stash in `docs/archive/`)
  - GY-271 / QMC5883L notes → `sensor_interactions/magnetometer_gy271_qmc5883l/docs/references.md` (NEW mini-project if the code exists elsewhere; otherwise stash the notes only under `sensor_interactions/_references/`)
- `matlab.md` (200 B, two links) → `auto_orientation_research/matlab_sensor_fusion/docs/references.md`
- `teensy4.0.md` (4 KB, wiring + calibration walkthrough + real captured offsets) → primarily `sensor_interactions/teensy_mpu6050/docs/calibration_walkthrough.md`. The captured offsets are load-bearing — preserve verbatim (candidate for `07_verbatim_content_registry.md`).
- `esp32_GPIO.jpg` → `sensor_interactions/_references/esp32_gpio_pinout.jpg` (shared reference asset, cross-cutting).

### 4.9 GY-271 QMC5883L magnetometer — code not present

Both `esp32.md` and `teensy4.0.md` reference the QMC5883L magnetometer with wiring diagrams and a QMC-vs-HMC clone gotcha. **No `.ino` file for the QMC5883L exists in this tree.** The magnetometer is presumably read via a lib (referenced) or the code is on another branch / lost. Note this gap — when reconstructing the RockSat-C data pipeline, someone will look for the mag-read sketch and not find it. Flag in `research/mini_projects/rocksat_c_payload_reconstruction/todo.md` (if such a mini is ever created), or in `sensor_interactions/imu/mpu6050/docs/archive/known_gap.md`.

### 4.10 `.gitignore` and build artifacts

`.gitignore` is 20 bytes — presumably `wifi_credentials.h` + Arduino build outputs. Standard. No `platformio.ini` present anywhere (this repo predates the team's PlatformIO adoption — it's Arduino-IDE-style). When migrating, wrapping each `.ino` in a proper `platformio.ini` per mini-project is a nice-to-have but not required by user policy.

---

## 5. Summary routing table

Every top-level entry → one recommended target repo + confidence:

| Source path | Target repo | Target sub-path (proposed) | Confidence |
|---|---|---|---|
| `esp32_enterprise_wpa3_eap/` | networking_pocs | `esp32_wpa2_eap_client/` | HIGH |
| `esp32_ent_wpa2_peap_web80/` | networking_pocs | `esp32_wpa2_peap_web_server/` | HIGH |
| `esp32_ewpa2_iic_091/` | networking_pocs | `esp32_wpa2_client_oled_ip_display/` | HIGH (route) / MED (name) |
| `esp32_hw125_sd/` | sensor_interactions | `esp32_hw125_sd_logger/` | HIGH |
| `i2c_scanner_whoami/` | sensor_interactions | `i2c_scanner_whoami/` | HIGH |
| `matlab_scripts/` | auto_orientation_research | `matlab_sensor_fusion/` | HIGH |
| `teensy_esp8266/` | networking_pocs | `esp8266_home_wifi_hello_world/webserver_api/` | HIGH |
| `teensy_esp8266_wifi_home/` | networking_pocs | `esp8266_home_wifi_hello_world/raw_wifiserver/` | HIGH (merge w/ prev.) |
| `teensy_i2c_oled/` | sensor_interactions | `teensy_ssd1306_oled_loading_bar/` | HIGH |
| `teensy_mpu6050/` | sensor_interactions | `teensy_mpu6050/raw_read/` | HIGH |
| `teensy_mpu6050_zero/` | sensor_interactions | `teensy_mpu6050/calibration/` | HIGH (merge w/ prev.) |
| `teensy_sdcard/` | sensor_interactions | `teensy_sdcard_logger/` | HIGH |
| `README.md` (top) | auto_orientation_research | absorbed into `matlab_sensor_fusion/docs/scope.md` | MED |
| `esp32.md` (top) | SPLIT | see §4.8 (5 destinations) | MED |
| `matlab.md` (top) | auto_orientation_research | `matlab_sensor_fusion/docs/references.md` | HIGH |
| `teensy4.0.md` (top) | sensor_interactions | `teensy_mpu6050/docs/calibration_walkthrough.md` (primary) + split | HIGH |
| `esp32_GPIO.jpg` (top) | sensor_interactions | `_references/esp32_gpio_pinout.jpg` | HIGH |

**Routing distribution:**
- `sensor_interactions`: 6 folders + 2 top-level docs + 1 image
- `networking_pocs`: 4-5 folders + partial top-level doc (§4.8)
- `auto_orientation_research`: 1 folder + 2 top-level docs (incl. absorbed README)
- Nothing goes to `flight_controller`, `communication_hardware`, `swarm_*`, `position_denial_research`, `drone_frame_modeling`, `fc_tool`, `darpa_lift_2026`, `research`, or `cybersecurity_demos`.

**Confidence tallies:**
- HIGH: 13 items (11 folders + 2 top-level docs/images)
- MED: 4 items (`esp32_ewpa2_iic_091/` name, `README.md`, `esp32.md` split, top-level naming choices)
- LOW: 0 items

---

## 6. Surprises / replan triggers

1. **CREDENTIAL LEAK (§4.4).** `teensy_esp8266/teensy_esp8266.ino` embeds `"Brown_Bear"` + `"5n!cKer*"` in git history. Not a reorg-blocking issue, but the migration checklist agent (C) should include a "sanitize credentials" step for this specific file. Recommend the credential be rotated by the user separately; migration should not carry the raw values into the new repo. **This is the only material surprise.**

2. **Folder-name mismatches (§4.1, §4.2).** Three of twelve folder names lie about what's inside: `_wpa3_eap` is actually WPA2, and both `teensy_esp8266*` folders contain no Teensy code. Not a replan trigger — rename during migration.

3. **No `.csv` fixture data (§3.6).** MATLAB scripts reference `data.csv`, `gdata.csv`, `quaternions.csv`, `rocket_position.csv`. Only `ex_data.m` generates one of them; the actual RockSat-C flight capture (`gdata.csv`) is not in-tree on `data-analysis`. If preserving real flight data matters, ask the user before migrating (or check `main` branch — but that's out of scope for read-only recon on `data-analysis`).

4. **Missing magnetometer sketch (§4.9).** Both docs describe GY-271 QMC5883L wiring; no code exists here. Flag as a documented gap in the target mini-project.

5. **`grocketBLE.m` name misleading (§3.6).** Uses USB serial, not BLE. Rename during migration.

6. **Merge decisions requested (§3.4+§3.12, §3.7+§3.8, §3.10+§3.11).** Three pairs of tightly-related sketches are candidates for merger into single mini-projects. Master plan / mini-project scaffold agent (B?) should confirm the merge policy — this recon recommends merging all three pairs, but yields to the master plan.

**Not surprises (things that looked concerning but are fine):**
- MATLAB toolbox dependency (`orientationSensor`, `ahrsfilter`) — user has MATLAB with Sensor Fusion Toolbox per `matlab.md`. Preserve as-is.
- Two different university networks (ERAU + UAA) — expected given the user's academic trajectory. No cleanup needed.
- Arduino-IDE `.ino` layout (no PlatformIO) — pre-existing style. User has not asked for PlatformIO conversion during reorg.

---

## 7. Naming suggestions (mini-project folder names)

Per `04_mini_project_setup_guide.md` conventions: `lowercase_snake_case`, descriptive, `sensor_interactions/<mini>/` and `networking_pocs/<mini>/` and `auto_orientation_research/<mini>/`.

### 7.1 `networking_pocs/` minis

| Proposed name | Contents | Notes |
|---|---|---|
| `esp32_wpa2_eap_client/` | §3.1 (bare client, HTTP GET to adafruit) | Rename from misleading `wpa3` folder name |
| `esp32_wpa2_peap_web_server/` | §3.2 (client + local :80 server) | Matches HEAD commit message |
| `esp32_wpa2_client_oled_ip_display/` | §3.3 (client + SSD1306 IP display) | Long but descriptive. Alt: `esp32_wpa2_client_with_display/` |
| `esp8266_home_wifi_hello_world/` | §3.7 + §3.8 merged | Two variants inside: `webserver_api/`, `raw_wifiserver/` |

### 7.2 `sensor_interactions/` minis

| Proposed name | Contents | Notes |
|---|---|---|
| `esp32_hw125_sd_logger/` | §3.4 (SD-card logging harness, simulated sensor data) | Alt umbrella: `sd_logging/esp32_hw125/` if merged with Teensy variant |
| `i2c_scanner_whoami/` | §3.5 (generic I2C bring-up utility) | Cross-check for floppi duplicate first (§4.5) |
| `teensy_ssd1306_oled_loading_bar/` | §3.9 (OLED bring-up) | Alt umbrella: `ssd1306_oled/teensy_variant/` |
| `teensy_mpu6050/` (umbrella) | §3.10 + §3.11 merged | With `raw_read/` and `calibration/` variants inside. Cross-check for floppi duplicate first (§4.5). |
| `teensy_sdcard_logger/` | §3.12 (SD-card logging harness, Teensy side) | Alt umbrella: `sd_logging/teensy/` merged with §3.4 |

### 7.3 `auto_orientation_research/` minis

| Proposed name | Contents | Notes |
|---|---|---|
| `matlab_sensor_fusion/` | §3.6 (all 5 `.m` scripts + top-level `matlab.md`) | Alt: `rocksat_c_analysis/` if payload identity matters more than technique identity |

### 7.4 Umbrella / shared-asset paths (proposed)

| Proposed path | Contents | Rationale |
|---|---|---|
| `sensor_interactions/_references/esp32_gpio_pinout.jpg` | §4.8 image | Shared reference — leading underscore marks non-mini-project asset dirs |
| `sensor_interactions/_utilities/` (optional) | Home for `i2c_scanner_whoami/` if utility bucket adopted | Skip if `i2c_scanner_whoami/` is a top-level mini instead |

### 7.5 Naming conventions applied

- All names are `lowercase_snake_case` (matches §04 guide).
- Descriptive over cryptic: `esp32_wpa2_client_oled_ip_display/` beats `esp32_ewpa2_iic_091/` (source was cryptic and abbreviated).
- Board name → API pattern → application when useful: `esp8266_home_wifi_hello_world/webserver_api/`.
- Umbrella pattern (`teensy_mpu6050/{raw_read,calibration}/`) suggested where two sketches form a natural pair (bring-up + calibration for the same chip on the same board).
- No `_v2`, `_new`, `_old`, `_backup` suffixes proposed — none needed for this content.

---

## 8. What stays in `~/GravityProbe/`

**Nothing.** Every folder and every top-level file has a migration destination. Once migration completes:
- `~/GravityProbe/` stays on disk indefinitely (per user's confirmed policy).
- No content lives there uniquely after migration — everything is duplicated into the target repos.
- The `data-analysis` branch preservation is a user's local concern, not a reorg concern.

---

## End-of-doc discipline notes

- **Files written this session:** `/home/devel/floppi/temp_reorg/08_gravityprobe_recon.md` (this file, and only this file).
- **Files edited in `~/GravityProbe/`:** none.
- **Git operations on `~/GravityProbe/`:** none (branch/status/log READ ONLY).
- **Docker / builds / migration scripts:** none produced.
- **Cross-file edits into other `temp_reorg/*.md`:** none — this doc references them by name but never modifies them.
