# 15 — SwarmLoc capability inventory (non-theory parts)

**What this answers, per source folder:** what did we *prove* we could do, what did it take, what
were the gotchas, and which specific files are worth carrying forward.

---

## Provenance

| | |
|---|---|
| **Read on** | 2026-08-19 |
| **Source** | `~/SwarmLoc` @ `main`, HEAD `6a6ee41` ("feat(esp32_field_node): WPA2-PSK path + iPhone hotspot connection verified", 2026-05-08). Working tree clean of any change by me — **read-only, per R6.** |
| **Governing doc** | `/home/devel/floppi/temp_reorg/11_routing_v2_2026-08-18.md` |
| **Scope covered** | `DWS1000_UWB/` (incl. its `docs/`), `esp32_field_node/`, `lora_feather_esp32/`, `GPS_module/`, `README.md`, `todo.md` |
| **Scope NOT covered** | `~/SwarmLoc/docs/` (4 files, 67,543 B) and `~/SwarmLoc/findings/` (3 files, 95,508 B) — the theory extract owns those. Listed in §8 so nothing is silently dropped. |

**Files I opened end-to-end:** `README.md`, `.gitignore`, `todo.md` (head 120 + tail 60 of 534 lines),
`DWS1000_UWB/{README.md, platformio.ini, include/config.h, include/display.h, src/anchor_main.cpp,
src/tag_main.cpp, src/calibration_main.cpp (head 80), monitor_ranging.sh, docs/scope.md, docs/todo.md,
docs/known_issues.md, docs/findings/{CRITICAL_HARDWARE_DISCOVERY.md, LDO_TUNING_FIX_SUCCESS.md,
SPI_EDGE_FIX_SESSION_2026-02-12.md, RX_DIAGNOSTIC_SESSION_2026-02-27.md,
ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md}, tests/README.md (head 60),
tests/test_outputs/ranging_verification_2026-01-11.txt, tests_new/** (all 12 files),
scripts/calibration/{README.md head 60, sample_results.json, test_results.json}, tools/dev.sh (head 40)}`;
`esp32_field_node/{README.md, platformio.ini, .gitignore, include/wifi_credentials.h,
include/wifi_credentials.local.h.example, docs/{scope.md, todo.md, roadmap.md head 60},
docs/findings/{existing-demo-inventory.md, m3-open-connection-success.md, m4-imu-web-success.md,
msc-guest-network-characterization.md (head 70), mpu6050-wiring.md (head 40)}}`;
`lora_feather_esp32/**` (every file); `GPS_module/**` (both sketches, one head-60 + full grep).

**Honesty caveat — read this before trusting any number below.**
SwarmLoc is an **archived project that nobody has re-run.** Last commit 2026-05-08; last DWS1000 work
2026-02-28. I did not build, flash, or execute anything. Every performance figure here is **the repo's
own claim**, transcribed with its source path — it is *documented*, not *reproduced*. Where I confirmed
something by reading code or measuring the tree myself I mark it **[VERIFIED]**; where I am relaying
the repo's assertion I mark it **[DOCUMENTED]**. Several repo claims contradict each other (§2.5); I
report the contradiction rather than picking a winner.

**Redaction note.** `11_routing_v2` §4 classes hardcoded institutional SSIDs as *"an attribution leak if
republished"* and directs that a capability-inventory doc must not carry the credential lines. Accordingly
the literals are replaced here by stable placeholders: **`<institutional SSID A>`** = the open,
captive-portal guest network; **`<institutional SSID B>`** = the WPA2-Enterprise campus network;
**`<device MAC>`** = the ESP32 STA MAC; **`<campus IP>`** = the campus DHCP lease. The technical point each
one illustrated is preserved; only the identifying string is removed. Source locations are given by path
and line so a rebuilder can find them without this document reproducing them.

---

## 1. Measured inventory — what is actually on disk

[VERIFIED] — `du -sb`, `find`, `git ls-files`, run 2026-08-19.

| Path | Bytes | Tracked files | Note |
|---|---:|---:|---|
| `DWS1000_UWB/` | 53,507,196 | 843 | The 53 MB. See the size breakdown below. |
| `esp32_field_node/` | 609,434 | 52 | 429,125 B of that is the vendored `lib/MPU6050` |
| `lora_feather_esp32/` | 47,005 | 7 | 5 sketches + 2 markdown |
| `GPS_module/` | 21,287 | 2 | two `.ino`, nothing else |
| `docs/` + `findings/` | 163,051 | 7 | **theory extract's scope**, not mine |
| `todo.md` | 24,919 | 1 | not a todo list — §6 |
| `README.md` | 376 | 1 | 2 sentences + a PlatformIO install curl |
| **repo `.git/`** | 16 MB | — | 26 commits, 914 tracked files |

### 1.1 Where the 53 MB actually is — **the routing doc is wrong about this**

`11_routing_v2` §"Why 10 and 11 die" states: *"The 53 MB is dominated by ~60 single-iteration debug
test variants."* [VERIFIED] **It is not.** Measured — the `Bytes` column is **apparent bytes** (`du -sb`) and every
percentage is that byte count over `DWS1000_UWB/`'s 53,507,196 B. Where the `du -sh`/`du -sk` *block* figure
differs materially it is given alongside, because §7.2 below quotes the block figures and the two standards
are easy to conflate (see §8):

| Component | Bytes | % of `DWS1000_UWB/` |
|---|---:|---:|
| `lib/` (all vendored) | 50,587,097 | **94.5%** |
| ├─ `lib/U8g2/` | 46,719,461 | **87.3%** (block usage: 45,916K = 84.7%) |
| │  └─ `lib/U8g2/src/clib/u8g2_fonts.c` — **one file** | 39,671,571 | **74.1%** |
| ├─ `lib/DW1000/` (thotro) | 3,496,215 | 6.5% (block: 4,440K = 8.2%) — mostly generated Doxygen HTML (`extras/doc/html/`, 133 files) + PCB gerbers |
| └─ `lib/DW1000-ng/` (F-Army) | 367,325 | 0.7% (block: 444K = 0.8%) |
| `tests/` (whole dir) | 1,052,648 | 2.0% |
| └─ the 60 root `*.cpp` debug variants | **398,954** | **0.75%** |
| `docs/` | 1,383,377 | 2.6% |
| `scripts/` | 267,549 | 0.5% (155 KB is one PNG) |
| `tests_new/` | 83,233 | 0.16% |
| `src/` + `include/` | 38,349 | 0.07% |

**Correction to carry forward:** the debug variants are 0.75% of the tree. The bloat is **one 39.7 MB
U8g2 font table**, committed to git. `11_routing_v2` carries **no** figure for the U8g2 pack to correct —
[VERIFIED] `grep -in -E 'u8g2|9 ?MB'` over `11_routing_v2_2026-08-18.md` returns no match, and its Gate 5
is "N-10: does git history travel?", unrelated. The ~45 MB on-disk figure is therefore new information, not
a correction (git compresses it well: the whole `.git` is 16 MB). Nothing about the
size argument changes the R1 ruling, but a future engineer sizing a migration needs the right number:
**deleting all 60 debug variants would save 0.4 MB; dropping `lib/U8g2` saves 45 MB.**

### 1.2 Authorship (relevant to Gate 5 / `PROVENANCE.md`)

[VERIFIED] `git shortlog -sne HEAD` — 26 commits, **4 identities**:

```
15  kaleldev <kaleldev@gmail.com>
 7  msc_intra <109829955+mscrobotics@users.noreply.github.com>
 3  gndpwnd <88689368+gndpwnd@users.noreply.github.com>
 1  gndpwnd <gnelsondev@gmail.com>
```

[VERIFIED, `git log --format='%ad|%an|%s' --date=short`] The `msc_intra` commits are the early LoRa/GPS
era and all seven fall between **2025-05-23 and 2025-05-30** — none in 2026. `gndpwnd` spans 2025-05-24 to
2025-06-13 and includes `24cd010 "reorg and init DWS1000"`, so **`gndpwnd`, not `kaleldev`, created the
DWS1000 tree.** `kaleldev` owns every commit from **2026-01-08 onward** (all 15), which is all subsequent
DWS1000 and all `esp32_field_node` work.

---

## 2. `DWS1000_UWB/` — UWB two-way ranging on Arduino Uno

### 2.1 What was PROVEN

[DOCUMENTED — `DWS1000_UWB/README.md` "Current Status" + `docs/scope.md` requirements checklist,
both last written 2026-02-28]

Asymmetric two-way ranging (TWR) between **two** Arduino Uno + Qorvo PCL298336 v1.3 DWS1000 shields,
producing calibrated absolute distance.

| Claim | Value | Source |
|---|---|---|
| Antenna delay calibrated | 16436 → **16405** (−31 ticks/device) | `include/config.h:74-75`; `docs/findings/ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` |
| Residual mean error @ 0.610 m | **+4.6 cm** (target was ±10–20 cm) | same session doc, "Round 2: Verification" |
| Precision | **±4.4 cm** StdDev over 564 samples / 60 s | same |
| Range rate | **9.4 Hz** (565 ranges / 60 s) | same |
| RX power at that range | −59 to −67 dBm typical | same |
| RX frame success | ACM0 **100%** (44/44), ACM1 **78%** (35/45) | `docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md` §11 |
| Radio config | Ch5 (6.5 GHz), 850 kbps, 16 MHz PRF, preamble 256, code 3 | `src/anchor_main.cpp:56-66` [VERIFIED in code] |

**The rate figures disagree across the repo — do not quote a single number.** [VERIFIED by reading all
three]:
- `README.md` and the calibration session say **9.4 Hz** (post-calibration, 60 s capture)
- `docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md` §"TWR Ranging — WORKING" says **2487 ranges in
  60 s = ~41 Hz, 97% success, mean 0.626 m ±8 cm** — but at the *uncalibrated* 16436 delay
- `docs/todo.md` "Recently Completed" says *"Live ranging verified with new config.h firmware (**37 Hz**)
  — 2026-02-28"*

The most defensible statement: **~9–41 Hz depending on firmware revision and timeout behaviour; ±4.4 cm
precision and +4.6 cm accuracy are from the single best-documented 60 s calibration run.** Nobody has
re-measured since 2026-02-28.

### 2.2 What it took — the gotcha ladder

This is the durable content. Seven distinct blockers — the first six each produced 0% RX or 0% SPI on
their own. In the order they were found:

**(1) Wrong chip assumed.** [DOCUMENTED — `docs/findings/CRITICAL_HARDWARE_DISCOVERY.md`, 2026-01-08]
The PCL298336 v1.3 shield was assumed to be a DWM3000EVB. Reading register 0x00 returned
`0xDECA0130` (DW1000/DWM1000), **not** `0xDECA0302` (DW3110/DWM3000). Every DWM3000-targeted artifact
in the tree predates this discovery — including all of `tests_new/` (§2.4).

**(2) IRQ not routed to an interrupt-capable pin.** [DOCUMENTED — `README.md` "Required Wiring", `docs/scope.md`]
The shield brings the DW1000 interrupt out on **D8**; the ATmega328P only has hardware interrupts on
D2/D3. **A jumper wire D8 → D2 is required on each shield.** Confirmed pin map (`README.md`,
`include/config.h:36-41`): RST=D7, IRQ=D2 (wired from D8), CS=D10, SPI on D11/D12/D13, optional OLED
on A4/A5.

**(3) `SPI_EDGE_BIT` incompatible with AVR SPI.** [VERIFIED in library source]
`lib/DW1000/src/DW1000.cpp` set bit 10 of `SYS_CFG` in `select()`, changing DW1000 MISO timing;
on AVR **every register read then returned `0xFF`**. The fix is present on disk at
`lib/DW1000/src/DW1000.cpp:140-146`:

```c
	// NOTE: SPI_EDGE_BIT (bit 10 of SYS_CFG) changes DW1000 MISO timing.
	// On Arduino Uno (AVR), this causes ALL reads to return 0xFF.
	// Disabled for AVR compatibility. Enable only for ESP8266/ESP32 if needed.
#if !defined(__AVR__)
	setBit(_syscfg, LEN_SYS_CFG, SPI_EDGE_BIT, true);
#endif
```

Reported effect [DOCUMENTED — `docs/findings/SPI_EDGE_FIX_SESSION_2026-02-12.md`]: 16% → 90-100% SPI
success in IDLE. Same session also dropped fast SPI from 16 MHz to 2 MHz for AVR
(`lib/DW1000/src/DW1000.cpp:106`), and reordered `handleInterrupt()` to check `isReceiveDone()`
*before* `isReceiveFailed()` (glitched error bits were discarding valid frames).

**(4) SPI is unreliable *during RX* — hardware EMI, not software.**
[DOCUMENTED — same session doc + `docs/known_issues.md`] IDLE 100%, TX ~100%, **RX 75-90%**. Attributed
to EMI from the DW1000 front-end on the shared Uno bus. Mitigations used: poll the IRQ *pin* (digital
read, no SPI) rather than SPI status registers during RX; double-read-with-retry; watchdog restart.

**(5) The real root cause — J1 jumper missing, DWM1000 power floating.**
[DOCUMENTED — `docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md` §10-11] J1 is a 3-pin header:
pin 1 = 3V3 from the on-board Torex DC-DC, pin 2 = 3V3 to the DWM1000, pin 3 = 3V3 from the Arduino LDO.
With **no** jumper the DWM1000 rail is floating and takes parasitic power through the SPI ESD diodes.
Symptoms: SAR-ADC-derived VDD **3.67 V** against a 3.6 V absolute max, `CLKPLL_LL` set on 402/403 RX
cycles, 0 good frames. Installing the jumper on **pins 1-2** took `CLKPLL_LL` from 75-99% to **0%** and
RX from 0% to 78-100%.

That session's negative results are as valuable as the fix — it explicitly ruled out, by test:
channel (Ch2 vs Ch5), device identity (TX/RX swap), TX power, PLLLDT init, PRF (16 vs 64 MHz), data
rate (110k / 850k / 6.8M), frame-check on/off, and a **full 32-value XTAL trim sweep** (zero good frames
at every trim). A future engineer should not repeat any of those.

**(6) Library choice is forced.** [DOCUMENTED — same doc §12] With J1 installed: thotro RX is
*completely* dead (0 events in 90 s); thotro-TX → ng-RX gives 0 good frames (incompatible frame
format); **DW1000-ng TX+RX gives 78-100%.** `docs/scope.md` records this as a binding technical
decision.

**(7) Host-side: USB hub contention breaks uploads.** [DOCUMENTED — `docs/known_issues.md`]
Two Unos on the same USB bus → `stk500_getsync()` failures on the second. Documented software fix is a
sysfs re-enumeration (`echo 0 > /sys/bus/usb/devices/$DEV/authorized`, sleep, `echo 1`); hardware fix is
separate buses. Also: STM32CubeIDE holds `/dev/ttyACM*` open even with no STM32 attached.

### 2.3 The canonical code path [VERIFIED by reading `platformio.ini` + `src/`]

`platformio.ini` defines four environments; three are live and share `[env_ng_common]`
(atmelavr / uno / arduino, 115200, `lib_extra_dirs = lib`, `-I lib/DW1000-ng/src -I include`):

| env | `build_src_filter` | Role |
|---|---|---|
| `uno_anchor` | `+<anchor_main.cpp>` | responder, flash to ACM0 |
| `uno_tag` (default) | `+<tag_main.cpp>` | initiator, flash to ACM1 |
| `uno_calibration` | `+<calibration_main.cpp>` | `-D CALIBRATION_MODE -D USE_OLED_DISPLAY` |
| `uno` | (legacy) | thotro lib, `-I lib/DW1000/src`, deprecated |

The protocol implemented in `src/anchor_main.cpp` + `src/tag_main.cpp` [VERIFIED]: 4-message asymmetric
TWR — `POLL(0) → POLL_ACK(1) → RANGE(2) → RANGE_REPORT(3)`, `RANGE_FAILED(255)` on protocol mismatch;
16-byte payload; three 5-byte DW1000 timestamps packed at offsets 1/6/11 of the RANGE frame; anchor
computes via `DW1000NgRanging::computeRangeAsymmetric()` + `correctRange()`; tag uses
`TransmitMode::DELAYED` with a 3000 µs `replyDelayTimeUS`; 500 ms inactivity reset on both sides.
Anchor addr 1, tag addr 2, network 10.

`src/calibration_main.cpp` is the auto-iterating calibration tag: 200 TWR samples → mean/stddev/min/max →
`new_delay = current + (measured − actual) / (2 × DISTANCE_OF_RADIO)`, `DISTANCE_OF_RADIO = 0.004692 m/tick`,
repeat until error < 5 cm. **Sign convention (easy to get backwards): measuring too short → *decrease*
delay** — that is what `docs/findings/ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md:42-44` records as
empirically established, and it is what the formula above implies. **Warning: `include/config.h:72` states
the exact inverse — see §2.5(8). The header comment is the odd one out and it travels with item 1.**
The 200 floats are 800 B = 54.6% of the Uno's 2 KB SRAM — the doc warns
>250 samples risks stack overflow.

### 2.4 `tests/` vs `tests_new/` — **the known-context assumption is inverted**

The task brief (and the routing doc's framing) expected `tests_new/` to be *"a cleaner reorganisation."*
[VERIFIED] **It is not. It is the abandoned first attempt, built on the wrong chip.**

| | `tests/` | `tests_new/` |
|---|---|---|
| Files | 74 in root + 10 subdirs | 12 (6 mini PlatformIO projects) |
| `.cpp` recursive / in root | 64 / **60** | 6 |
| Total lines (root `.cpp`) | 14,018 | 736 (whole tree) |
| Bytes | 1,052,648 | 83,233 |
| First commit touching it | `2b2ee40` **2026-01-08** | `925acb8` **2026-01-11** |
| Last commit touching it | `dae195e` **2026-02-27** | `925acb8` **2026-01-11** — never again |
| Target chip | DW1000 | **DWM3000 / DW3110** |
| Pin `RST` | 7 | **9** |

**The decisive evidence** [VERIFIED, `tests_new/test_01_chip_id/src/main.cpp` — three non-contiguous
lines: `:4`, `:7` and `:35`]:

```c
 * This test verifies that we can communicate with the DWM3000 chip via SPI.
 * Expected Device ID: 0xDECA0302 (DW3110 chip in DWM3000 module)
...
#define EXPECTED_DEV_ID  0xDECA0302
```

That is the ID `CRITICAL_HARDWARE_DISCOVERY.md` proved wrong on **2026-01-08**, three days before
`tests_new/` was committed. `tests_new/test_06_ranging/{anchor,tag}` also use the *thotro*
`DW1000Ranging.h` API with `PIN_RST = 9` — the deprecated library at the wrong pin.

**Ruling: `tests/` is canonical.** `tests_new/` is a dead end, kept only as evidence of the wrong-chip
detour. Do not migrate it.

**On the 60 variants.** The brief's "~60 single-iteration debug variants" is exactly right:
[VERIFIED] 60 `.cpp` in `tests/` root, of which **27 are `test_rx_*`** and **10 are `test_tx_*`**. The
`v3 → v8{,b,c,d,e,f} → v9{,b,c,d,e} → v10 → v11` chain in `test_rx_*` maps one-to-one onto the test
matrix table in `RX_DIAGNOSTIC_SESSION_2026-02-27.md` — each file is one row of a documented
elimination. 40 of the 60 contain the LDO/OTP write. **The narrative in that one findings doc carries
100% of the value of the 60 files; the files themselves are 0.75% of the tree and worth 0.4 MB.**
Only 5 of the 60 are non-throwaway: `test_twr_anchor.cpp`, `test_twr_tag.cpp`,
`test_calibration_tag.cpp` (the direct ancestors of `src/*_main.cpp` — a 48-line diff, refactored to
use `config.h`/`display.h`), plus `test_ldo_tuning_fix.cpp` and `test_rx_v11_xtalt_sweep.cpp` as the
two most instructive diagnostics.

### 2.5 Gotchas a future engineer will hit — internal contradictions in the repo's own docs

These are live traps. All [VERIFIED] by reading both sides.

1. **J1 jumper: the repo tells you both things.** `README.md`, `docs/scope.md`, `docs/todo.md` and
   `RX_DIAGNOSTIC_SESSION_2026-02-27.md` all say **install J1 on pins 1-2, it is REQUIRED**.
   `docs/known_issues.md` "Hardware Notes" says **"J1 jumper: Leave OPEN (no jumper). DC-DC powers
   DWM1000 directly."** [VERIFIED, `git log -1 -- <file>` on each] The *same-commit* fact holds only for
   `known_issues.md` and `RX_DIAGNOSTIC_SESSION_2026-02-27.md`, which share `dae195e` (2026-02-27);
   `README.md`, `docs/scope.md` and `docs/todo.md` were last written a day later in `261f4cf`
   (2026-02-28). So the REQUIRED side is also the *newer* side, and `known_issues.md` simply wasn't
   updated. **The jumper-installed reading is correct**; it is both the newer text and the one
   backed by the before/after measurement.
2. **The celebrated LDO/OTP fix is not in the production path.** `LDO_TUNING_FIX_SUCCESS.md` calls the
   OTP→AON_CTRL `OTP_LDO` write "the root cause" fix (0% → 98%). [VERIFIED] it *is* applied in the
   **deprecated** thotro library (`lib/DW1000/src/DW1000.cpp:195-205` and `:1108-1116`), and it is
   **still an unimplemented TODO in the library actually used** — `lib/DW1000-ng/src/DW1000Ng.cpp:1071-1072`
   reads `if(ldoTune[0] != 0) { // TODO tuning available, copy over to RAM: use OTP_LDO bit`.
   `LDO_TUNE_DEV0/DEV1` in `include/config.h:78-79` are **dead defines** — grep finds no reference to
   them outside `config.h`. Whether this matters is unresolved: the J1 hardware fix may have made the
   software LDO workaround moot. Carry the fact, not a conclusion.
3. **`README.md` is half-stale.** It advertises build envs `uno_ng` / `uno`; `platformio.ini` actually
   defines `uno_anchor` / `uno_tag` / `uno_calibration` / `uno`. Its Quick Start flashes
   `tests/test_twr_*.cpp` via `scripts/upload_and_capture.sh`, which is the *pre-refactor* workflow;
   the current one is `tools/dev.sh` + the PIO envs.
4. **`include/config.h` disagrees with the firmware on data rate.** `RADIO_DATA_RATE 0 // 0=110kbps`
   (`config.h:57`) vs `DataRate::RATE_850KBPS` hardcoded in all three `src/*_main.cpp` structs. The
   `RADIO_*` defines in `config.h` are **decorative** — the firmware does not read them.
5. **`scripts/calibration/` is a stale parallel toolchain.** Its `README.md` instructs you to flash
   `tests/calibration/calibration_test.ino` with an `IS_TAG` toggle over `/dev/ttyUSB0`. The live path
   is `src/calibration_main.cpp` + `uno_calibration` over `/dev/ttyACM*`. Worse, its
   `sample_results.json` and `test_results.json` are **byte-identical synthetic sample data** (30
   samples, ±0.145 cm stddev) — that is *not* a real measurement and must not be quoted; the real
   figure is ±4.4 cm.
6. **`compile_commands.json`** (27 KB, tracked) is a stale build artifact full of absolute
   `/home/devel/.platformio/...` paths.
7. **`tests/` has two near-duplicate master runners**, `run_all_tests.sh` (10,519 B) and
   `RUN_ALL_TESTS.sh` (10,218 B) — a case-collision hazard on any case-insensitive filesystem.
   [VERIFIED] `11_routing_v2` does **not** discuss case collisions anywhere
   (`grep -in 'case.collision'` → no match), and its Gate 5 is about git-history transfer. This is a
   hazard found here, not a gate being satisfied.
8. **The antenna-delay sign convention is stated *backwards* inside `config.h` — the file that is item 1
   on the migrate-out list.** `include/config.h:72` reads *"Sign: If measuring TOO LONG → decrease delay.
   TOO SHORT → increase delay."* That is the exact inverse of
   `docs/findings/ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md:42-44` (*"Measuring TOO SHORT →
   DECREASE antenna delay / Measuring TOO LONG → INCREASE antenna delay"*), **and** of `config.h`'s own
   formula three lines above it (`:69` — `new_delay = old_delay + (measured_error / 2) /
   DISTANCE_OF_RADIO`, which *raises* the delay when the measurement is long). Two sources against one,
   and the outlier is a comment no code reads — but a rebuilder who trusts the header will calibrate in
   the wrong direction, and `config.h` ships in item 1. **Fix `config.h:72` on the way out.**

---

## 3. `esp32_field_node/` — commercial-WiFi field node

**[VERIFIED] the routing-doc claim about cross-project references is correct**, and it is far more
extensive than "references floppi and GravityProbe": [VERIFIED, `grep -rn 'floppi\|GravityProbe'
~/SwarmLoc/esp32_field_node`, 2026-08-19] **53 reference lines across 11 files** — `docs/findings/` ×5,
`docs/archive/` ×3, plus `docs/roadmap.md`, `docs/scope.md` and `docs/todo.md`.
The relevant quotes, verbatim:

> **`esp32_field_node/docs/findings/existing-demo-inventory.md:75-84`**
> `**1. `~/floppi/flight_controller/`** — production-grade WiFi manager` … `Files of interest:`
> `- `include/wifi_credentials.h` — supports both PSK and Enterprise` /
> `- `include/wifi_certs.h` — optional CA cert + client cert/key for TLS` /
> `- `src/wifi_manager.cpp` — `setupWiFi()` + `handleWiFi()` with auto-reconnect, timeouts, dual-core safe`
> **`:83`** `Quality bar: this is our reference.`

> **`:86-91`** `**2. `~/GravityProbe/esp32_ewpa2_iic_091/`** — closest behavioral match` …
> `Auth: **PEAP + MSCHAPv2**, no client certs` … `Smallest path to a working <institutional SSID A> connection.`

> **`docs/findings/peap-mschapv2-reference.md:157-161`** — a side-by-side reconnect-strategy table:
> `| GravityProbe (both) | `ESP.restart()` after 30 s | hard reboot loop | brutal but works on flaky setups |`
> `| floppi flight_controller | log + continue without WiFi | `WiFi.reconnect()` every 5 s in loop | production pattern |`
> `**Recommended for esp32_field_node**: floppi pattern`

`peap-mschapv2-reference.md:211-214` and `demo-projects-tips-and-tricks.md:212-218` list absolute paths
into both `~/floppi/flight_controller/` and `~/GravityProbe/`. **This makes `esp32_field_node/docs/findings/`
a partial, already-written distillation of GravityProbe's WiFi capability** — the GravityProbe capability
inventory (`11_routing_v2` §3) should cite it rather than re-derive it.

### 3.1 What was PROVEN

[DOCUMENTED — `docs/roadmap.md`, `docs/findings/m3-*.md`, `m4-*.md`, all 2026-05-08. Every milestone
M0–M4 was completed **in a single autonomous session**, per `docs/todo.md` Notes.]

| Milestone | Status | Evidence |
|---|---|---|
| M0 boot + I2C scan | live-verified | OLED found at 0x3C |
| M1 WiFi scan | live-verified | 9 networks; **the guest SSID `<institutional SSID A>` reports `WIFI_AUTH_OPEN`**, overturning the project's founding assumption that it was WPA2-Enterprise (`findings/msc-guest-network-characterization.md`) |
| M2 SSD1306 128×64 via U8g2 | live-verified | 3-row layout; thin-stroke font was illegible, `u8g2_font_helvB12_tf` fixed it |
| M3 open + captive portal | **live-verified** | verbatim log in `m3-open-connection-success.md:21` (redacted here): `ip=<campus IP> mac=<device MAC> rssi=-79dBm portal=captive`, HTTP 302 from `connectivitycheck.gstatic.com/generate_204` |
| M3 WPA2-Enterprise PEAP/MSCHAPv2 | **BUILD-ONLY — never live-tested** | `m3-open-connection-success.md` "What's NOT yet verified": no `<institutional SSID B>` credentials in session |
| M4 MPU6050 + web + dual-core | **live-verified** | `m4-imu-web-success.md`: I2C 0x3C + 0x68, ±2 g / ±250 °/s, gravity +1.01 g on Z, gyro <4 °/s stationary, IMU task on Core 0 / loop on Core 1, HTTP :80 |
| WPA2-PSK / iPhone hotspot | **live-verified** | commit `6a6ee41` body: IP `172.20.10.13/28`, portal probe HTTP **204** (real internet), RSSI −34…−48 dBm |

Build budget [DOCUMENTED, `m4-imu-web-success.md`]: **flash 78.3%** (1,026,909 / 1,310,720 B),
**RAM 15.0%** (49,044 / 327,680 B). Hardware: ESP32-D0WDQ6 rev 1, 2 cores @ 240 MHz, 4 MB flash,
STA MAC `<device MAC>`.

### 3.2 What it took / gotchas

- **The founding assumption was wrong, and one scan proved it.** The project spent M0 gathering PEAP
  reference material for `<institutional SSID A>`; the first live scan returned `OPEN` on both BSSIDs. Lesson,
  in the repo's own words: *"the username/password the user remembers entering is consumed by a web
  form on the portal page, not by the WPA2-Enterprise EAP exchange."*
- **Captive-portal client isolation blocked the deliverable.** M4's web monitor was unreachable from a
  laptop on the same SSID. The fix that shipped (`6a6ee41`) was to add a WPA2-PSK path and use an
  iPhone hotspot instead. Recorded workaround chain, not a theory.
- **`connectAuto()` priority is PSK > Enterprise > Open**, selected by which credential macro is
  non-placeholder [VERIFIED, `src/main.cpp:250-253` — the priority comment — with the branch order at
  `:255-270`, plus the commit `6a6ee41` body]. **Live trap:** the header comment at
  `include/wifi_credentials.h:14-16` still states the *pre-PSK* two-way rule — *"At boot, the firmware
  picks based on credential state: if WIFI_ENT_USERNAME is not \"FILL_ME_IN\" → try enterprise / else →
  try open"* — and never mentions PSK. The header and the code disagree, and `wifi_credentials.h` is
  item 6 on the migrate-out list, so the stale comment travels unless it is fixed.
- **Dual-core needs an I2C mutex.** OLED + MPU6050 share the bus across two cores; an RAII `I2CLock`
  guard (`src/main.cpp:45-47`) is taken at each I2C call site **in `src/main.cpp`** — [VERIFIED] 14 sites,
  `:122` through `:347`. This is **caller-side discipline, not enforced coverage**: `lib/imu/imu.cpp` and
  `lib/display/display.h` include `<Wire.h>` and do no locking of their own, and `lib/imu/imu.h:26` says so
  outright — *"multi-core system must serialize Wire access externally."* A new caller that forgets the
  guard walks straight past it. Reported result [DOCUMENTED, `m4-imu-web-success.md`]: 30 s clean run, no
  `[i2c-error]`, no display corruption.
- **`<avr/pgmspace.h>` in the vendored i2cdevlib MPU6050 compiles on ESP32** because Arduino-ESP32
  shims it — so floppi's AVR-era MPU6050 library was reusable *unmodified*.

### 3.3 Credential hygiene — SwarmLoc's is the good pattern [VERIFIED]

`11_routing_v2` §4 says *"SwarmLoc's credentials are clean (`FILL_ME_IN` placeholders only)."*
Confirmed: a grep for password/psk/ssid across all non-`lib/` `.h/.cpp/.ino` finds **no secret**. The
three-file pattern is:

- `include/wifi_credentials.h` — **committed**, all secrets `"FILL_ME_IN"`, and at the top:
  `#if defined(__has_include)` / `#if __has_include("wifi_credentials.local.h")` → include it. Every
  default is `#ifndef`-guarded so the local file wins.
- `include/wifi_credentials.local.h.example` — committed template, every line commented out.
- `.gitignore` — ignores `include/wifi_credentials.local.h`, `include/credentials.h`,
  `include/secrets.h`, with a comment explaining *why* `wifi_credentials.h` itself is committed.

This is the working implementation of what **Gate 7** (not Gate 6 — Gate 6 is `.git` health) says
`lowprofiledronegurus` is baking into the `flight_controller` scaffold: `**/wifi_credentials.h` +
`!**/wifi_credentials.h.example`. Note Gate 7 is recorded **PARKED** by the operator and gates nothing.

**Residual leak — and the remediation is bigger than one line.** Two institutional SSIDs are hardcoded as
committed defaults: `<institutional SSID A>` (open, captive portal) and `<institutional SSID B>`
(WPA2-Enterprise). That is the attribution leak flagged in `11_routing_v2` §4. [VERIFIED by
`grep -n -i -E 'MSC|UAA|SSID'` on both files, 2026-08-19] they occur at **six line locations across two
files**, not the two usually cited:

| File | Lines | What is there |
|---|---|---|
| `include/wifi_credentials.h` | **11**, **12** | header-comment "e.g." examples naming both networks |
| `include/wifi_credentials.h` | **42** | `#define WIFI_OPEN_SSID` default |
| `include/wifi_credentials.h` | **47** | `#define WIFI_ENT_SSID` default |
| `include/wifi_credentials.local.h.example` | **20**, **23** | commented `WIFI_OPEN_SSID` / `WIFI_ENT_SSID` template lines |

Stripping only `:42,47` leaves four live copies behind, and the `.example` file travels in the same bundle
(§7.1 item 6). Set the two `#define` defaults to `"FILL_ME_IN"`, and **delete the SSID strings from the
header comment and from the `.example` template** as well. Separately, the file name
`docs/findings/msc-guest-network-characterization.md` embeds the guest network's name — it is retained in
this document only as a path anchor; rename it on migration.

---

## 4. `lora_feather_esp32/` — LoRa link + the ToF sizing argument

47,005 B, 7 tracked files. Hardware: Adafruit Huzzah32 (ESP32 Feather) + RFM95W 900 MHz LoRa
FeatherWing; pins CS=33, RST=27, IRQ=12 [VERIFIED, sketch header].

### 4.1 `notes.md` — the split point is **line 55**

[VERIFIED — read all 142 lines.] The brief said it "mixes hardware setup with ToF/positioning theory
and has an obvious split point." It does, and the seam is clean:

| Lines | Content | Verdict |
|---|---|---|
| **1–51** | Huzzah32 + RFM95W product link; Arduino IDE board-manager URLs; **Windows CP2104/CP2102N Silicon Labs VCP driver install, step-by-step via Device Manager** | Vendor toolchain instructions, Windows-only, already bit-rotting. Low value. |
| 52–54 | blank | the seam |
| **55–142** | ToF theory: `d = c·t`, `Δd = c·Δt`, `Δt = Δd/c`; worked examples for 10 cm (333 ps → ~3 GHz clock) and 100 ft (101.6 ns → ~10 MHz); a 4-row accuracy→clock-precision table; the rule of thumb `f = 1/Δt`; and clock-sync notes (one-way ToF needs synchronised clocks; **TWR needs only relative time**; UWB modules like DWM1000/DWM3000 do this internally at 10-15 cm) | **This is the founding argument of the whole project** — the derivation that says LoRa cannot do sub-metre ToF and UWB can. |

Lines 55–142 are ~88 lines of portable theory in one contiguous block. `git log` corroborates the
narrative: `1e4bddf "Init ToF accuracy to precision relationship"` → later `9255e99 "switching to
DWS1000 for ToF operations"`.

`adafruit_lora.md` (898 B) is a strict subset of `notes.md` lines 1–25 plus three library links
(sandeepmistry/arduino-LoRa, adafruit/RadioHead, PaulStoffregen/RadioHead) and a pinout image URL.

### 4.2 What was CLAIMED — and where the evidence stops

**This is the one section of this document with no supporting artifact, and it should not be read as a
result.** [DOCUMENTED — commit `43dc859` (msc_intra, 2025-05-30) "solved LoRa timing, state management,
sequential protocols and asymmetric protocols."] That commit *subject line* is the entire evidentiary
basis. [VERIFIED] `lora_feather_esp32/` holds exactly **7 files, of which 2 are markdown** (`notes.md`,
`adafruit_lora.md`): **no results doc, no findings doc, no captured log, and not one recorded RTT, RSSI,
SNR or frequency-error value anywhere in the folder.** What the source demonstrably contains is the
*implementation* — a deterministic two-node LoRa ping-pong with per-packet link-metric logging written
into it. That the link was ever actually closed rests on the committer's word alone.
`lora_gps_node/` is the master (PING, 1000 ms response delay, 20 s resend timeout);
`lora_gps-d_node/` is the slave (PONG, 2500 ms delay) — [VERIFIED] a 95-line diff, same file otherwise.
Radio config on both: 915 MHz, TX 17 dBm, SF8, BW 125 kHz, CR 4/8, sync word 0x12, CRC on. The sketches
log **round-trip time, processing time, sequence-mismatch detection, RSSI, SNR, and
`LoRa.packetFrequencyError()`** — i.e. they are link-characterization instruments, not just a demo.
Deliberate ToF-oriented touches: `LoRa.idle()` + 50 ms `preTransmitDelay` for a clean radio state,
150 ms `radioSettleTime`, non-blocking `endPacket(false)`.

**Gotcha — the names lie.** [VERIFIED by grep for `gps|nmea|tinygps|latitude`: **zero matches**]
`lora_gps_node` and `lora_gps-d_node` contain **no GPS code whatsoever.** They are pure LoRa ping-pong.
Anyone migrating on filename alone will carry the wrong thing.

**Second gotcha, documented in the git log itself** (`12c551e`): *"use softwareserial for uno,
hardwareserial for feather esp32, definitely use 115200 for the LoRa rylr890, but still buggy"* — note
this refers to a **RYLR890 AT-command module**, a different radio from the RFM95W the two node sketches
drive. Two distinct LoRa hardware paths live in this one folder.

`ard_BaudR_scan/` (AVR, SoftwareSerial on D2/D3, sweeps 9600–115200, sends `AT+VER?`, stops on a
response containing `VER` or `+`) and `adaf_esp32_BaudR_scan/` (ESP32 `HardwareSerial(1)` on GPIO16/17,
sweeps 4800–115200, sends `AT`) are the tooling for that. Small, generic, and genuinely reusable for
any unknown-baud UART module.

`ESP32_I2C_Scanner/` is the verbatim Random Nerd Tutorials I2C scanner (956 B) — superseded by the
device-labelling scanner in `esp32_field_node/src/main.cpp`.

---

## 5. `GPS_module/` — GPS bring-up

21,287 B, **2 files, nothing else** — no build system, no docs, no README. Board: Adafruit Feather
ESP32 + Adafruit GPS FeatherWing on `Serial1`, `Adafruit_GPS` library.

- **`GPS_OLED_091/GPS_OLED_091.ino`** (4,631 B) — the superset. GPS fix → DMS on a DSD Tech 0.91"
  SSD1306 128×32 over software I2C (`U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C(U8G2_R0, SCL, SDA)`,
  SDA=GPIO23, SCL=GPIO22). Init: `PMTK_SET_NMEA_OUTPUT_RMCGGA`, `PMTK_SET_NMEA_UPDATE_1HZ`,
  `PGCMD_ANTENNA`, 9600 baud. Carries a **startup loading bar** and a "Searching for GPS signal…"
  no-fix state.
- **`basic_GPS_LAT_LONG_LCD_adafruit_featherwing/…ino`** (4,368 B) — the same GPS parse + `convertToDMS`
  printed to serial only. [VERIFIED: 259-line diff, dominated by the OLED block; the GPS logic is
  common.] Despite "LCD" in the folder name there is **no LCD library included** — grep for `#include`
  returns only `Adafruit_GPS.h`.

**The reusable nugget** [VERIFIED, both files]: the `Adafruit_GPS` NMEA `DDMM.MMMM` → decimal-degrees
conversion, which is easy to get subtly wrong:

```c
float lat_decimal = (int)(GPS.latitude / 100) + ((GPS.latitude - (int)(GPS.latitude / 100) * 100) / 60);
```

plus `convertToDMS()` (decimal → deg/min/sec). Both files use `float`, which is ~1 m of precision at
these magnitudes — fine for a display, **not** fine for anything positional. Flag on migration.

`GPS_OLED_091.ino` is also the file `esp32_field_node/docs/findings/existing-demo-inventory.md` names as
its **top pick** for the OLED pattern. [VERIFIED, `grep -n`] The two facts live in two separate places, not
one: the top-pick naming is at **`:15-23`** (`### Top picks` → `**1. \`GPS_module/GPS_OLED_091/GPS_OLED_091.ino\`**
— closest match for ESP32`), and the warning that both demos target 128×32 while the newer hardware is
128×64 (different U8g2 constructor) is a separate `### Display-size warning` block at **`:56-64`**.

---

## 6. `SwarmLoc/README.md` and `todo.md`

**`README.md`** (376 B) — two sentences (*"A positioning system using a drone swarm for agents venturing
in a GPS-denied area."*) plus a `curl`-and-run PlatformIO installer. No project map, no status. The
per-project READMEs carry everything.

**`todo.md`** — **[VERIFIED] the brief is correct: it is not a todo list.** 534 lines, 24,919 B,
**zero `- [ ]` checkboxes**, no markdown headings. It opens:

> `This session is being continued from a previous conversation that ran out of context. The
> conversation is summarized below:` / `Analysis:` / `Let me chronologically analyze the conversation…`

It is a **Claude Code compaction dump** from 2026-01-12 (`git log`: three commits, all 2026-01-12,
last `869f6c2` "trying another DW1000 library"), followed by raw serial paste:

```
[TX #15] Sending PING... SENT!
[RX] Waiting for PONG...
[1]+  Exit 124                stty -F /dev/ttyACM0 115200 raw -echo && …
```

and it ends mid-thought on an `Update Todos` block. Two things make it worth *reading* once before
discarding: it is a first-person record of the pre-J1 dead ends (garbled RX — `"PING"` arriving as
`"Uá'}8zA="` with good CRC; IRQ count stuck at 0 despite the D8→D2 jumper), and it references
**`/home/devel/Desktop/SwarmLoc/`** throughout — a second checkout that **no longer exists.**
[VERIFIED 2026-08-19] `ls -d /home/devel/Desktop/SwarmLoc` → *No such file or directory* (the `Desktop`
directory itself is present; the `SwarmLoc` tree under it is not).
`existing-demo-inventory.md:47` flags the same path as *"mirror of this repo; ignore."* Both references are
stale pointers to a tree that is gone, so **`~/SwarmLoc` is the only checkout and this is not an extraction
blocker.**

Historical value only. Superseded in every particular by `docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md`.

---

## 7. SHORTLIST — what migrates out, where, and why

Rules applied: selective + attributed, never `cp -r` (R6); destination chosen from
`sensor_interactions` / `position_denial_research` / `research` / `flight_controller`.
Every item carries a `PROVENANCE` line naming `~/SwarmLoc`, the file path, the commit, and the
contributors (§1.2).

### 7.1 Migrate OUT

| # | Item (path under `~/SwarmLoc/`) | Size | → Destination | Why there |
|---|---|---:|---|---|
| 1 | `DWS1000_UWB/src/{anchor_main,tag_main,calibration_main}.cpp` + `include/{config,display}.h` + `platformio.ini` | 6 files, 31,218 B | **`position_denial_research/reference/uwb_twr_arduino/`** | The only *working, calibrated* cooperative TWR implementation in either legacy repo, and cooperative self-localization is precisely this repo's side of the hiverf line. **Flag for the operator:** R1 re-scopes this repo to theory-only. Carrying six archived files is not "growing a simulation platform", but it does test the boundary. **Fallback if the operator says no: `research/findings/` beside item 2.** |
| 2 | **This document** (`15_swarmloc_capability_inventory.md`) | — | **`research/findings/swarmloc_capability_inventory.md`** | Exactly the doc `11_routing_v2` §3 row 3 asks for. Note it also covers the `DWS1000_UWB` non-theory scope that §3 row 2 routes to `position_denial_research/findings/uwb_capability_inventory.md` — **recommend publishing once in `research/findings/` and having `position_denial_research` link to it**, rather than splitting one narrative across two repos. |
| 3 | `DWS1000_UWB/docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md` | 7,935 B | **`research/findings/`** (cite from item 1) | The single highest-value file in the whole tree. Its test matrix + "What Was Ruled Out" table is what makes the 60 debug variants disposable. Carrying it means nobody re-runs an XTAL trim sweep. |
| 4 | `DWS1000_UWB/docs/findings/ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` | 5,382 B | **`research/findings/`** | The only end-to-end calibration record with real numbers and the empirical **sign convention**. Method transfers to any DW1000/DW3000 work. |
| 5 | `DWS1000_UWB/docs/findings/{SPI_EDGE_FIX_SESSION_2026-02-12.md, LDO_TUNING_FIX_SUCCESS.md, CRITICAL_HARDWARE_DISCOVERY.md}` + the `lib/DW1000/src/DW1000.cpp:140-146` hunk as a `.patch` | 12,576 B (2,792 + 3,642 + 6,142) | **`research/findings/uwb_library_patches/`** | Three named root causes + the actual diff. **Carry the patch, not the 4.4 MB library.** Must ship with the §2.5(2) caveat that the LDO fix is absent from DW1000-ng. |
| 6 | `esp32_field_node/include/wifi_credentials.h` + `wifi_credentials.local.h.example` + the `.gitignore` stanza | 3,640 B for the two headers | **`flight_controller/`** | **Gate 7** (parked, gates nothing) says the `flight_controller` scaffold is already baking in `**/wifi_credentials.h` + `!**/wifi_credentials.h.example`. This is the working three-file `__has_include` override pattern, verified free of any secret. **Redact the two institutional SSIDs at all six locations before the copy — `wifi_credentials.h:11,12,42,47` and `wifi_credentials.local.h.example:20,23`** — setting `:42`/`:47` to `"FILL_ME_IN"` and deleting the strings from the header comment and the template (§3.3). Also fix the stale pre-PSK priority comment at `wifi_credentials.h:14-16` (§3.2). |
| 7 | `esp32_field_node/docs/findings/peap-mschapv2-reference.md` + `demo-projects-tips-and-tricks.md` | 14,758 B (7,396 + 7,362) | **`flight_controller/findings/wifi/`** | Both are *about* `flight_controller/src/wifi_manager.cpp` — they benchmark it against GravityProbe and the canonical PIO example and name it the production pattern. They belong with the code they critique. **Also: the GravityProbe capability inventory should cite these instead of re-deriving them.** |
| 8 | `esp32_field_node/docs/findings/mpu6050-wiring.md` | ~4 KB | **`sensor_interactions/imu/mpu6050/`** | Textbook R7 content: a complete pin-by-pin bring-up table (required vs optional vs unused pins, AD0 → 0x68/0x69, shared-bus coexistence with the OLED) plus the expected verification output. |
| 9 | `esp32_field_node/{lib/imu/,lib/display/,lib/web/}` (6 files) + `docs/findings/m4-imu-web-success.md` **+ the concurrency regions of `src/main.cpp`: `:45-47` (`struct I2CLock`), `:459` (`xTaskCreatePinnedToCore(imuTask, …)`) and the 14 `I2CLock` call sites `:122`–`:347`** | 18,249 B (six `lib/` files 12,264 B + the findings doc 5,985 B) + the `main.cpp` excerpts | **`sensor_interactions/esp32_sensor_node/`** | **373 lines** across the six `lib/` files (`imu` 81, `display` 129, `web` 163 — the last including the 140-line HTTP+JSON telemetry monitor), *not* 269. Those six files are thin HAL wrappers; the Core-0 task pinning and the RAII I2C guard that are the actual reason to carry this item live in `src/main.cpp`, which is why the `main.cpp` regions are named explicitly above — taking `lib/` alone delivers the wrappers and none of the concurrency pattern (§3.2). Documented live-verified 2026-05-08; not re-run. |
| 10 | `GPS_module/GPS_OLED_091/GPS_OLED_091.ino` | 4,631 B | **`sensor_interactions/gps/bringup/`** | GPS bring-up is named in R7's remit. Superset of the sibling sketch; carries the NMEA `DDMM.MMMM`→decimal conversion, the `PMTK_*` init sequence, and the no-fix UI state. **Migrate with the `float`-precision caveat (§5).** |
| 11 | `lora_feather_esp32/notes.md` **lines 55–142 only** | ~3 KB | **`position_denial_research/theory/`** | The ToF→required-clock-precision derivation and the one-way-vs-TWR clock-sync conclusion. It is the *reason* this programme is UWB and not LoRa, and it is pure theory — squarely this repo's charter. Split at line 55; leave 1–51 behind. |
| 12 | `lora_feather_esp32/{lora_gps_node,lora_gps-d_node}/*.ino` | 13,257 B | **`research/findings/lora_link_characterization/`** | A deterministic two-node ping-pong **implementation** with RTT / RSSI / SNR / frequency-error logging already written in — a ready-made link-budget instrument. **Carry it as code to re-validate, not as a result to cite:** no log, results doc or measurement survives anywhere in the source folder (§4.2). **Rename on arrival** — they contain no GPS. **Note:** the natural home is really the planned `swarm_communication_protocol` spec repo, which is outside the four destinations I was given — re-route there if it exists at move time. |
| 13 | `lora_feather_esp32/{ard_BaudR_scan,adaf_esp32_BaudR_scan}/*.ino` | 2.4 KB | **`sensor_interactions/tools/uart_baud_scan/`** | Generic unknown-baud UART discovery (AVR SoftwareSerial + ESP32 HardwareSerial variants). Cheap, and it recurs on every new serial peripheral. |
| 14 | `DWS1000_UWB/{tools/dev.sh, tools/serial_monitor.py, scripts/upload_and_capture.sh, scripts/upload_dual_and_capture.sh, scripts/COMMANDS_REFERENCE.md}` | 27,546 B | **`flight_controller/tools/`** | Board-agnostic two-device PlatformIO flash + timed serial capture, and a non-blocking serial-monitoring reference. `flight_controller` already has a `tools/` per `11_routing_v2` §2 and is the heaviest PlatformIO/serial consumer. Fallback: `research/tools/`. |
| 15 | `DWS1000_UWB/docs/known_issues.md` — **the USB/upload section only** | ~2 KB | **`flight_controller/findings/`** | The sysfs USB re-enumeration recipe and the STM32CubeIDE port-squatting note are host-environment problems that hit every AVR/ESP32 project on this machine. **Do not carry the "Hardware Notes" section — it contains the wrong J1 instruction (§2.5.1).** |

### 7.2 Stays behind — explicitly, with the reason

| Item | Size | Why it stays |
|---|---:|---|
| `DWS1000_UWB/lib/U8g2/` | 45 MB block / 46,719,461 B apparent | Unmodified upstream; **85% of the project by block usage, 87% by apparent bytes — and 74% of the whole tree is one font file.** Declare `olikraus/U8g2` in `lib_deps` instead — which is exactly what `esp32_field_node/platformio.ini` already does. |
| `DWS1000_UWB/lib/DW1000/` (thotro) | ~4.4 MB | Deprecated *and* proven broken for RX. Mostly generated Doxygen HTML + PCB gerbers. Only the `:140-146` patch hunk travels (item 5). |
| `DWS1000_UWB/lib/DW1000-ng/` | 444 KB | Upstream F-Army. Carry a pinned reference + the two documented deltas, not the tree. |
| The 60 `tests/*.cpp` debug variants | 399 KB | Superseded by items 3 + 5. 40 of 60 differ only in the LDO block. Value is in the narrative, not the files. |
| `DWS1000_UWB/tests_new/` | 83 KB | **Wrong chip.** Targets `0xDECA0302` (DWM3000); hardware is `0xDECA0130`. Dead end, never revisited after 2026-01-11 (§2.4). |
| `DWS1000_UWB/docs/archive/` | 210 KB, 22 files | Session diaries (`SESSION_COMPLETE_2026-01-11_EVENING.md` and friends). Pure process residue. |
| `DWS1000_UWB/scripts/calibration/` | 238,222 B (the 267,549 B figure in §1.1 is all of `scripts/`) | Stale parallel toolchain against a superseded firmware/port scheme, and its two result JSONs are **identical synthetic sample data** (§2.5.5). The 155 KB `test_plot.png` plots that fake data. |
| `DWS1000_UWB/{archive/initiator,archive/responder}/*.ino` | 13,772 B | The pre-project sketches whose bugs `CRITICAL_HARDWARE_DISCOVERY.md` enumerates. Historical only. |
| `DWS1000_UWB/compile_commands.json` | 27 KB | Stale build artifact, absolute `/home/devel/` paths. |
| `DWS1000_UWB/tests/{run_all_tests.sh, RUN_ALL_TESTS.sh}` | 21 KB | Near-duplicate, case-colliding pair (§2.5.7). |
| `lora_feather_esp32/notes.md` lines 1–51 + `adafruit_lora.md` | ~4 KB | Windows Silicon Labs VCP driver clickthrough + Arduino IDE board-manager URLs. Vendor instructions that rot; `adafruit_lora.md` is a subset of `notes.md`. |
| `lora_feather_esp32/ESP32_I2C_Scanner/` | 956 B | Verbatim Random Nerd Tutorials snippet; `esp32_field_node/src/main.cpp` has a better one that labels known addresses. |
| `GPS_module/basic_GPS_LAT_LONG_LCD_adafruit_featherwing/` | 4.4 KB | GPS logic is common with item 10; the only delta is serial-vs-OLED output. (Folder name says LCD; no LCD library is included.) |
| `~/SwarmLoc/todo.md` | 24.9 KB | LLM compaction dump, superseded in every particular (§6). |
| `~/SwarmLoc/README.md` | 376 B | Two sentences and a `curl`. |
| `DWS1000_UWB/docs/findings/` — the other ~45 files | ~1.0 MB | Not read individually (§8). Many are large LLM research dumps (`CALIBRATION_WEB_RESEARCH.md` 76 KB, `ESP32_MIGRATION_RESEARCH.md` 68 KB, `UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md` 53 KB). **`…SATURATION_MITIGATION.md` should be triaged against `hiverf`, not this inventory** — saturation is explicitly hiverf's side of the boundary. |

---

## 8. What I did NOT read / could not verify

- **`~/SwarmLoc/docs/` (4 files) and `~/SwarmLoc/findings/` (3 files)** — headers only. Out of my
  scope by assignment; the theory extract owns them. Measured at **163,051 B apparent** (`du -sb`). The
  routing doc's **176 KB** is the *same* trees measured as block usage (`du -ch ~/SwarmLoc/docs
  ~/SwarmLoc/findings` → `176K`), so **the two figures agree and the routing doc is not wrong here.** Both
  standards appear in this document — §1 and §1.1 quote apparent bytes, §7.2 quotes `du -sh` block
  figures — so compare like with like before calling either number an error.
- **~45 of the 53 files in `DWS1000_UWB/docs/findings/`** — I read 5 in full and listed the rest with
  sizes. I have **not** characterised the contents of `TWR_ACCURACY_OPTIMIZATION.md` (57 KB),
  `MULTILATERATION_IMPLEMENTATION.md` (58 KB), `DUAL_ROLE_ARCHITECTURE.md` (49 KB),
  `CALIBRATION_GUIDE.md` (46 KB), `DW1000_RANGING_BEST_PRACTICES.md` (45 KB),
  `UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md` (53 KB), `TEST_RESULTS.md` (41 KB), or the
  `ARDUINO_UPLOAD_TROUBLESHOOTING.md` / `ACM1_SPECIFIC_TROUBLESHOOTING.md` pair (86 KB combined).
  Several look like theory that overlaps the theory extract's remit — **the two extracts should
  reconcile `DWS1000_UWB/docs/findings/` ownership before either publishes.**
- **58 of the 60 `tests/*.cpp`** — I read filenames, sizes, and grepped for the LDO pattern. I opened
  only the naming/diff evidence needed to rule on canonicity. I did not verify that any given variant
  compiles.
- **`esp32_field_node/src/main.cpp` (512 lines) and `lib/wifi_field/`** — read in regions, not
  end-to-end: the WiFi-mode enum and credential plumbing, `connectAuto()` (`:250-270`), `struct I2CLock`
  (`:45-47`), its 14 call sites (`:122`–`:347`) and the task creation at `:459`. Everything else in that
  file is uncharacterised.
- **`/home/devel/Desktop/SwarmLoc/`** — **does not exist.** [VERIFIED 2026-08-19] `ls -d
  /home/devel/Desktop/SwarmLoc` → *No such file or directory*. It is referenced throughout `todo.md` and
  flagged as a mirror by `existing-demo-inventory.md:47`; both are stale pointers to a deleted tree.
  `~/SwarmLoc` is the only checkout, so there is nothing here to open, reconcile, or treat as an
  extraction blocker — and no R6 question arises. (Recorded because an earlier draft of this document left
  it open.)
- **Nothing was built, flashed, or run.** No claim in §2, §3, §4 or §5 has been reproduced. Every
  performance number is the repo quoting itself, in some cases inconsistently (§2.5). Treat all of it
  as "last known good on the date stated", never as "works".
- **`tests/test_outputs/ranging_verification_2026-01-11.txt`** is the only captured raw serial artifact
  in the tree, and it is a **negative** result — 3 minutes of monitoring showing both devices announcing
  themselves and then nothing. The successful runs' raw logs were gitignored (`**/*.log`,
  `**/test_output/`, and `esp32_field_node/.gitignore`'s `tests/results/`), so **the primary evidence
  for every success claim in this document no longer exists on disk.** The findings docs quote it;
  the captures themselves are gone.
