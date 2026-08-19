# 14 — SwarmLoc UWB / two-way-ranging theory extract

**Seed for `position_denial_research/theory/uwb_ranging/`.**
Distilled 2026-08-19 from `~/SwarmLoc` (read-only legacy source, R1/R6).

---

## Provenance and honesty caveat

**What was read** (all paths under `~/SwarmLoc`, read-only, nothing modified):

| Path | Size | Read |
|---|---|---|
| `docs/DW1000_LIBRARY_BUG_FIX.md` | 8.8 KB | full |
| `findings/README_Research_Findings.md` | 12 KB | full |
| `findings/UWB_Swarm_Ranging_Architecture_Research.md` | 46 KB | full |
| `findings/UWB_Implementation_Code_Examples.md` | 32 KB | TOC + §3 trilateration + §1 header (code body sampled) |
| `DWS1000_UWB/docs/findings/TWR_ACCURACY_OPTIMIZATION.md` | 57 KB | full |
| `DWS1000_UWB/docs/findings/CALIBRATION_WEB_RESEARCH.md` | 77 KB | full |
| `DWS1000_UWB/docs/findings/DW1000_LIBRARY_ALTERNATIVES.md` | 14 KB | full |
| `DWS1000_UWB/docs/findings/ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` | 5.4 KB | full |
| `DWS1000_UWB/docs/findings/RX_DIAGNOSTIC_SESSION_2026-02-27.md` | 7.9 KB | full |
| `DWS1000_UWB/docs/findings/IRQ_PIN_INVESTIGATION_2026-02-27.md` | 8.5 KB | full |
| `DWS1000_UWB/docs/findings/SPI_EDGE_FIX_SESSION_2026-02-12.md` | 2.8 KB | full |
| `DWS1000_UWB/docs/findings/LDO_TUNING_FIX_SUCCESS.md` | 3.6 KB | full |
| `DWS1000_UWB/docs/findings/CRITICAL_HARDWARE_DISCOVERY.md` | 6.1 KB | full |
| `DWS1000_UWB/docs/findings/DWS1000_PINOUT_AND_FIX.md` | 5.4 KB | full |
| `DWS1000_UWB/docs/findings/DWS1000_JUMPER_WIRE_STATUS.md` | 5.6 KB | full |
| `DWS1000_UWB/docs/findings/DW1000_CPLOCK_ISSUE.md` | 9.7 KB | partial (first 45 lines) |
| `DWS1000_UWB/docs/findings/ANTENNA_DELAY_CALIBRATION_2026.md` | 29 KB | headings + §3 §4 §5 |
| `DWS1000_UWB/docs/findings/CALIBRATION_GUIDE.md` | 45 KB | headings only |
| `DWS1000_UWB/docs/findings/MULTILATERATION_IMPLEMENTATION.md` | 58 KB | exec summary + §1.2 §3.1 §3.2 |
| `DWS1000_UWB/docs/findings/UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md` | 53 KB | headings + §3.1 |
| `DWS1000_UWB/docs/findings/DUAL_ROLE_ARCHITECTURE.md` | 49 KB | exec summary + §1.1 |
| `DWS1000_UWB/docs/findings/RESEARCH_SUMMARY.md`, `ACM1_DIAGNOSIS_FINAL.md`, `TEST_RESULTS.md` | — | partial |
| **Source code** `DWS1000_UWB/src/{tag,anchor}_main.cpp`, `include/config.h`, `platformio.ini`, `README.md`, `docs/scope.md`, `docs/roadmap.md`, `lib/DW1000/src/DW1000.cpp`, `lib/DW1000-ng/src/*` | — | read for verification |
| **Added in the 2026-08-19 correction pass** `DWS1000_UWB/docs/findings/POLLING_TEST_RESULTS_2026-01-17.md`, `docs/archive/STATUS_NOW.md`, `tests/test_08_multi_node_swarm/` (`ls`/`wc -l` + `IMPLEMENTATION_COMPLETE.md`), `docs/findings/CALIBRATION_GUIDE.md` §sign-convention, `findings/UWB_Swarm_Ranging_Architecture_Research.md` §5.5 | — | targeted re-read to check specific claims |

**Caveat that governs everything below.** SwarmLoc is an **archived project that nobody has re-run**. Its last UWB commit is `261f4cf`, 2026-02-28; the repo's remaining commits (to 2026-05-08) are non-UWB ESP32 field-node work. No claim here has been reproduced on hardware by this extract. Every number is either a repo-reported measurement from a session log or a literature figure the repo collected. Do not treat any capability as "works" — treat it as "worked once, on two specific boards, according to a log".

**Correction pass, 2026-08-19.** An adversarial review of this extract found and this revision fixed: a merged rate/distance figure in §10, a merged upstream-date figure in §5.1 that erased a real intra-archive contradiction, a "0 % RX" overstatement in §4, two wrong line anchors (§4.4, §9), a mis-ceded TDOA boundary call in §9, an unsupported file count in §1, and four present-tense capability claims softened to what the logs support. Each correction is marked in place.

**Marking convention used throughout:**

- **[CODE]** — I opened the file in `~/SwarmLoc` and the source/constant says this. Path and line given.
- **[LOG]** — the repo's own session log reports this measurement. Not re-run, not independently verified.
- **[DOC]** — the repo asserts it from datasheets, app notes, papers or web research. No local measurement backs it.

---

## 1. What SwarmLoc actually is, and what it actually achieved

`~/SwarmLoc` is a GPS-denied positioning project (`README.md`: *"A positioning system using a drone swarm for agents venturing in a GPS-denied area"*). Its UWB subproject `DWS1000_UWB/` (53 MB, 914 tracked files repo-wide) is the only part that got to working hardware.

**Achieved, per `DWS1000_UWB/docs/roadmap.md` and `README.md` [LOG]:**

| Item | Value |
|---|---|
| Nodes | **2** (one anchor, one tag). Never more. |
| Protocol | Asymmetric two-way ranging (POLL / POLL_ACK / RANGE / RANGE_REPORT) |
| Range rate | **9.4 Hz** (565 ranges in 60 s), calibrated run |
| Precision | **±4.4 cm** StdDev |
| Accuracy | **+4.6 cm** mean error at 0.610 m known distance |
| Antenna delay | **16405** (calibrated from 16436 default) |
| TX success | 100 % both devices |
| RX success | ACM0 100 %, ACM1 78 % |

**Never achieved:** 3-anchor trilateration, multi-node swarm, TDMA, position output. `DWS1000_UWB/docs/scope.md` lists *"Two-node only — scope limitation per user"* as a hard constraint. The 3-anchor / TDMA / trilateration material in this document is **[DOC]** design work only. A full 5-node swarm firmware and test harness exists on disk (`DWS1000_UWB/tests/test_08_multi_node_swarm/` — **11 files, 4 703 lines** by `ls` / `wc -l` in this extract; the directory's own `IMPLEMENTATION_COMPLETE.md:5` self-reports *"10 files, 4,131 lines"* and marks it *"READY FOR TESTING"* 2026-01-11). **No execution record for it exists anywhere in the archive** — `DWS1000_UWB/docs/archive/STATUS_NOW.md:111` still lists it as *"✅ Future use (when >2 nodes)"* and `docs/scope.md:84` puts *"Multi-node swarm coordination (beyond two nodes)"* out of scope. That is an argument from absence, not a log; the two-node scope was set after the harness was written.

---

## 2. The ranging method, and why

### 2.1 What was built

Asymmetric double-sided TWR, three messages on the air plus a report:

```
TAG                                   ANCHOR
 |-------- POLL ------------------------>|   t1 (tag TX),  t2 (anchor RX)
 |<------- POLL_ACK ---------------------|   t3 (anchor TX), t4 (tag RX)
 |-------- RANGE (carries t1,t4,t5) ---->|   t5 (tag TX, DELAYED), t6 (anchor RX)
 |<------- RANGE_REPORT (distance) ------|
```

The anchor computes the range and sends it back; the tag only prints it. **[CODE]** `~/SwarmLoc/DWS1000_UWB/src/anchor_main.cpp` calls `DW1000NgRanging::computeRangeAsymmetric(timePollSent, timePollReceived, timePollAckSent, timePollAckReceived, timeRangeSent, timeRangeReceived)` then `DW1000NgRanging::correctRange(distance)`.

Time-of-flight formula (all four intervals in DW1000 ticks):

```
round1 = t4 - t1      reply1 = t3 - t2
round2 = t6 - t3      reply2 = t5 - t4

ToF = (round1 * round2 - reply1 * reply2) / (round1 + round2 + reply1 + reply2)
```

The RANGE message is sent with **delayed transmit**: the tag reads the system timestamp, adds 3000 µs of UWB time, writes that as the scheduled TX time, then adds the TX antenna delay to its own recorded `timeRangeSent`. **[CODE]** `src/tag_main.cpp`, `transmitRange()`.

### 2.2 Why DS-TWR and not SS-TWR

The 2026-01-08 architecture research (`findings/UWB_Swarm_Ranging_Architecture_Research.md`) recommended **SS-TWR** for Arduino Uno on CPU-load grounds. Three days later `TWR_ACCURACY_OPTIMIZATION.md` (2026-01-11) **reversed that** with the arithmetic, and the reversal is the one that survived into the firmware:

**[DOC]** DW1000 crystal is 38.4 MHz ±10 ppm. Two devices 10 ppm apart differ by 384 Hz.

```
SS-TWR error = (T3 - T2) * clock_drift_ppm / 1e6
  10 ppm, 1 ms reply  ->  10 ns  ->  3 mm
  10 ppm, 100 ms reply -> 1 µs   -> 30 cm
```

*"SS-TWR is unusable unless response time < 1 ms (difficult on Arduino Uno)."* DS-TWR cancels clock offset to first order; the residual is O(ε²) — for ε = 10 ppm, ε² = 1e-10, **< 0.1 mm**. SDS-TWR (4 messages) buys < 1 cm more and was judged not worth the airtime.

**Take-away for a rebuild: use DS-TWR. The SS-TWR recommendation in the January-08 architecture document is superseded and should not be followed.**

### 2.3 Radio configuration that actually ran

**[CODE]** `src/tag_main.cpp` / `src/anchor_main.cpp`, `device_configuration_t DEFAULT_CONFIG`:

| Parameter | Value |
|---|---|
| Channel | 5 (6489.6 MHz) |
| Data rate | 850 kbps |
| PRF | 16 MHz |
| Preamble length | 256 symbols |
| Preamble code | 3 |
| SFD | standard |
| extendedFrameLength | false |
| receiverAutoReenable | true |
| smartPower | true |
| frameCheck | true |
| nlos | false |
| Reply delay | 3000 µs |
| Inactivity reset period | 500 ms |
| Frame payload | 16 bytes (`LEN_DATA`) |
| Device address / network id | anchor 1, tag 2 / network 10 |
| Interrupts enabled | onSent, onReceived, onReceiveFailed, onReceiveTimestampAvailable |

Note the earlier debugging ran at 110 kbps / preamble 2048 (`MODE_LONGDATA_RANGE_LOWPOWER`). One 2026-02-27 finding suspects that long-range mode **saturates the receiver at < 1 m** — RXPHE on every frame with good preamble detection — and the working config moved to 850 kbps / preamble 256 **[LOG]** (`IRQ_PIN_INVESTIGATION_2026-02-27.md`, Finding 8).

---

## 3. Accuracy achieved, and what limited it

### 3.1 The measured numbers (the irreplaceable part)

**Post-calibration verification run** — `ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` **[LOG]**, 60 s capture, both devices at antenna delay 16405:

| Metric | Value |
|---|---|
| Known distance | 0.610 m (24 in, antenna-to-antenna, ±2 in) |
| Mean measured | 0.656 m |
| StdDev | ±0.044 m (±4.4 cm) |
| Min / Max | 0.52 m / 0.99 m |
| Error | **+0.046 m (+4.6 cm)** |
| Samples | 564 / 565 (one startup glitch, R#2 = −65.90 m, excluded) |
| Rate | ~9.4 Hz |
| Timeouts | ~91 anchor resets |
| RX power | −59 to −67 dBm typical |

Outliers at 0.80–0.99 m **correlate with RX power −80 to −93 dBm** — i.e. the bad samples announce themselves in the power reading. This is the single most actionable filtering fact in the archive.

**Earlier same-day uncalibrated TWR run** — `RX_DIAGNOSTIC_SESSION_2026-02-27.md` **[LOG]**, antenna delay 16436 (default):

| Metric | Value |
|---|---|
| Ranges | 2487 in 60 s ≈ **41 Hz** |
| Success | 97 % (76 timeouts / 2564 polls) |
| Mean | 0.626 m, StdDev ±0.079 m (±8 cm) |
| RX power | −66 to −70 dBm |

**These two runs do not reconcile** — see §10.

### 3.2 Error budget

**[DOC]** `TWR_ACCURACY_OPTIMIZATION.md` root-sum-square budget:

| Error source | Arduino Uno | ESP32 |
|---|---|---|
| DW1000 timestamp quantisation | ±2 cm | ±2 cm |
| Antenna delay, **uncalibrated** | ±50–100 cm | ±50–100 cm |
| Antenna delay, calibrated | ±5 cm | ±5 cm |
| Temperature drift (10 °C, uncorrected) | ±2 cm | ±1 cm |
| Interrupt-latency jitter | ±2 cm | ±0.5 cm |
| Multipath (indoor, even with LOS) | ±10 cm | ±8 cm |
| RF interference | ±3 cm | ±2 cm |
| Protocol timing | ±2 cm | ±0.5 cm |
| **RSS total** | **±12.6 cm** | **±10.2 cm** |

Optimisation priority, in the order the repo ranks them:

1. **Antenna delay calibration — 50–100 cm improvement.** Everything else is noise next to this.
2. **DS-TWR** — removes clock drift entirely.
3. **Library bug fix** (§5) — without it interrupts never fire at all.
4. Multipath mitigation — 5–20 cm.
5. Signal-quality rejection — reject rather than correct.
6. Median filter — 3–5 cm.
7. ISR optimisation — 1–3 cm.
8. SPI speed — rate, not accuracy.
9. Temperature compensation — 1–2 cm, optional.

### 3.3 Platform ceiling

**[DOC]** ATmega328P @ 16 MHz: interrupt entry 82 cycles = **5.1 µs**, ISR execution 3.5 µs, `micros()` resolution **4 µs**. DW1000 timestamp resolution is **15.65 ps** — the MCU's clock is **255 000× coarser**. This is survivable *only* because the DW1000 timestamps in hardware at the transceiver; the MCU never touches the critical timing path. Delayed TX (8 ns scheduling resolution, low 9 bits of the 40-bit counter ignored) keeps the MCU out of the reply-time path as well.

RAM: 2048 bytes total; DW1000 library ~800 B, DW1000Ranging ~1200 B, each tracked device 74 B, `MAX_DEVICES` 4 = 296 B. Leaves ~200–400 B for the application. The calibration firmware's 200-sample float buffer alone is 800 B = **54.6 % of SRAM**; the repo warns that going beyond ~250 samples risks stack overflow **[LOG]**.

Practical Uno limits **[DOC]**: 1–5 Hz update, 3–4 anchors, ±10–20 cm. ESP32 comparison: 10–50 Hz, 10+ anchors, ±5–10 cm, interrupt latency < 1 µs, SPI to 20 MHz.

---

## 4. The four hardware/firmware defects that blocked ranging for seven weeks (§4.5 is a fifth fix, real but superseded)

This is the highest-value part of the archive. The project spent 2026-01-08 → 2026-02-27 at **0 % good (CRC-valid) frames** because of four independent faults — **two physical** (§4.1 J1 jumper, §4.2 D8→D2 wire) and **two fixed in code** (§4.3 `PIN_RST`, §4.4 `SPI_EDGE_BIT`). Note it was *not* a flat 0 % RX: frames were detected throughout — `POLLING_TEST_RESULTS_2026-01-17.md` logs RX events with garbage payloads (`len=1021`), and `SPI_EDGE_FIX_SESSION_2026-02-12.md:34` records *"RX: 33% frame detection (10/30 received)"* — but no correct payload was decoded until the J1 fix. A rebuilder should check all four *before* writing a line of protocol code.

### 4.1 J1 power jumper missing — the actual root cause

**[LOG]** `RX_DIAGNOSTIC_SESSION_2026-02-27.md` §10–11, `DWS1000_JUMPER_WIRE_STATUS.md`.

J1 on the DWS1000 shield is a **3-pin header**:

- pin 1 = `3V3_DCDC` (Torex XC9282B33E1R-G, 600 mA)
- pin 2 = `3V3` to the DWM1000
- pin 3 = `3V3_ARDUINO` (Arduino's own LDO, 50–150 mA)

With **no** jumper the DWM1000's supply pin is **floating** — it draws parasitic power through the 5 V SPI ESD protection diodes. Symptoms:

- DW1000 SAR ADC: raw Vbat = 255 (8-bit saturated); OTP `Vmeas3V3` = 191; computed `(255 − 191)/173 + 3.3 = ` **3.67 V**, against a DW1000 absolute maximum of **3.6 V**; fluctuating 3.55–3.67 V.
- `CLKPLL_LL` set on **402 of 403** RX cycles (99.8 %) — the digital PLL locks once at init and never re-locks.
- **0 % good frames** across every configuration tried.

**Fix: jumper on J1 pins 1-2 (DC-DC → DWM1000).** After the fix, on both shields:

- `CLKPLL_LL` 0/406 and 0/404 — **PLL completely stable**.
- ACM0 as RX: **44/44 = 100 %** good frames, zero CRC errors, zero watchdog resets.
- ACM1 as RX: **35/45 = 78 %** good frames (18 CRC).
- Payloads correct ("PING#00001", …).

Everything ruled out *before* finding this, each with a test **[LOG]**: channel (2 vs 5), device-specific hardware (TX/RX swap), signal saturation (min TX power), missing PLLLDT init (applied at the correct `EC_CTRL_SUB = 0x00`, no change), PRF (16 and 64 MHz), data rate (110 k / 850 k / 6.8 M), frame-check on/off, SPI corruption (double-read guards), and a **full 32-value crystal-trim sweep** (0–31, 20 RX cycles each — zero good frames at every trim, PLL_LL 55–100 % uniformly, best trim = 16 = the default).

**Lesson worth carrying: a UWB radio that enumerates over SPI and reports a correct chip ID can still be unpowered.** SPI worked the whole time through the level-shift path.

### 4.2 IRQ routed to a non-interrupt pin

**[DOC/LOG]** `DWS1000_PINOUT_AND_FIX.md`. The DWS1000 (PCL298336) routes the DWM1000 IRQ (module pin 22) to Arduino **D8**. Arduino Uno has hardware interrupts on **D2 and D3 only**, and `attachInterrupt(digitalPinToInterrupt(8), …)` fails silently. Fix: jumper wire **D8 → D2**, keep `PIN_IRQ = 2`.

Confirmed working for TX (`attachSentHandler` 30/30) **[LOG]**, but note §10 — RX interrupts never fired even with the jumper in place, and the project shipped with polling-then-interrupt behaviour it never fully explained.

### 4.3 Reset pin is D7, not D9

**[LOG]** `IRQ_PIN_INVESTIGATION_2026-02-27.md` Finding 10. The shield routes DW1000 `RSTn` to **D7**; every test file used the library default `PIN_RST = 9`. Consequence chain:

1. `DW1000.select(PIN_SS)` calls `reset()`,
2. `reset()` toggles pin 9 — not connected to anything,
3. **the DW1000 never receives a hardware reset**,
4. and because `_rst != 0xFF`, the SPI-based `softReset()` is skipped too.

So the chip ran from whatever state power-on left it in. The repo's own 2026-01-11 pinout document had this right; it took until 2026-02-27 for anyone to apply it to the code. **[CODE]** the final firmware uses `PIN_RST 7` (`include/config.h`).

### 4.4 SPI_EDGE_BIT breaks all register reads on AVR

**[LOG/CODE]** `SPI_EDGE_FIX_SESSION_2026-02-12.md`, verified in `lib/DW1000/src/DW1000.cpp:140-145`.

`DW1000Class::select()` sets `SPI_EDGE_BIT` (bit 10 of `SYS_CFG`), which changes DW1000 MISO timing in a way the AVR SPI peripheral cannot sample. **Every register read returns 0xFF after library init.** Fix, as applied:

```cpp
#if !defined(__AVR__)
    setBit(_syscfg, LEN_SYS_CFG, SPI_EDGE_BIT, true);
#endif
```

SPI reliability before: **16 %** (only direct pre-init reads worked). After: **90–100 % in IDLE, 75–90 % during RX**. The residual RX-mode degradation is attributed to EMI from the active radio front-end on the shared Uno bus. Two further changes shipped in the same session **[CODE]**, in two different places in the same file:

- `_fastSPI` reduced from 16 MHz to **2 MHz** on non-ESP8266 targets — `lib/DW1000/src/DW1000.cpp:104-110` (comment at `:106`: *"16MHz causes SPI corruption on AVR during RX mode"*).
- `handleInterrupt()` — `lib/DW1000/src/DW1000.cpp:728-772` — reordered to check `isReceiveDone()` **before** `isReceiveFailed()` (comment at `:742-743`), and `isClockProblem()` moved to the end (`:767`) — with 10–20 % SPI corruption, glitched error bits were discarding valid frames, and sticky PLL bits raised false errors.

### 4.5 LDO tuning from OTP — a real fix, applied to the wrong library

**[LOG]** `LDO_TUNING_FIX_SUCCESS.md` (2026-01-19). Both `thotro/arduino-dw1000` and `DW1000-ng` read the factory LDO tuning value from OTP address `0x04` and then **throw it away** behind a `// TODO tuning available, copy over to RAM: use OTP_LDO bit`. Implementing the TODO:

```cpp
byte ldoTune[4];
DW1000.readBytesOTP(0x04, ldoTune);
if (ldoTune[0] != 0 && ldoTune[0] != 0xFF) {
    byte aonCtrl[4];
    DW1000.readBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    aonCtrl[0] |=  0x40;   // set OTP_LDO (bit 6) -> transfer OTP LDO values to active config
    DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
    delay(1);
    aonCtrl[0] &= ~0x40;
    DW1000.writeBytes(AON_REG, AON_CTRL_SUB, aonCtrl, 4);
}
```

Reported effect **[LOG]**: `RFPLL_LL` went from permanently SET to CLEAR on both devices, success rate from 0 % to **98.0 %**. Per-device OTP values differ — **DEV0 = 0x88, DEV1 = 0x28** — which the log offers as the explanation for why the two shields had always behaved differently.

**But:** **[CODE]** the fix lives in `lib/DW1000/src/DW1000.cpp:193-208` and again at `:1106-1119` (re-applied after `tune()` reconfigures the PLL) — that is the **thotro library, which the final firmware does not use**. In `lib/DW1000-ng/src/DW1000Ng.cpp:1068-1072` the TODO is **still unimplemented**, and `include/config.h` defines `LDO_TUNE_DEV0 0x88` / `LDO_TUNE_DEV1 0x28` that **no source file reads**. The working 9.4 Hz TWR therefore ran *without* LDO tuning applied — consistent with J1, not LDO, having been the real root cause. Treat the 98 % LDO result as a genuine but **superseded** finding.

---

## 5. The DW1000 library bug, its fix, and the alternatives evaluated

### 5.1 The bug

**[DOC]** `docs/DW1000_LIBRARY_BUG_FIX.md`, discovered 2026-01-11 in `thotro/arduino-dw1000` v0.9.

`DW1000Class::interruptOnReceiveFailed()` manipulates the 4-byte `_sysmask` buffer using the **5-byte** length constant:

```cpp
// BROKEN (upstream v0.9)
setBit(_sysmask, LEN_SYS_STATUS, LDEERR_BIT, val);   // LEN_SYS_STATUS == 5
setBit(_sysmask, LEN_SYS_STATUS, RXFCE_BIT,  val);
setBit(_sysmask, LEN_SYS_STATUS, RXPHE_BIT,  val);
setBit(_sysmask, LEN_SYS_STATUS, RXRFSL_BIT, val);

// FIXED
setBit(_sysmask, LEN_SYS_MASK, LDEERR_BIT, val);     // LEN_SYS_MASK == 4
setBit(_sysmask, LEN_SYS_MASK, RXFCE_BIT,  val);
setBit(_sysmask, LEN_SYS_MASK, RXPHE_BIT,  val);
setBit(_sysmask, LEN_SYS_MASK, RXRFSL_BIT, val);
```

**[CODE] verified in this extract:** `lib/DW1000/src/DW1000Constants.h:82` `#define LEN_SYS_STATUS 5`; `:105` `#define LEN_SYS_MASK 4`; `lib/DW1000/src/DW1000.h:438` `static byte _sysmask[LEN_SYS_MASK];`. The fix is present at `lib/DW1000/src/DW1000.cpp:1015-1020` with `// FIXED: was LEN_SYS_STATUS` on each line. So the buffer really is 4 bytes and the constant really was 5 — **a one-byte overrun past `_sysmask`**, every time.

Claimed impact **[DOC]**: corrupts adjacent memory (the document guesses `_chanctrl`), corrupts the DW1000 interrupt-mask register, so the hardware never raises interrupts. It reaches everything because `setDefaults()` calls it, and essentially every example calls `setDefaults()`. Failure is **silent** — BasicSender transmits one packet and hangs, BasicReceiver never receives, `DW1000Ranging` devices never discover each other, no error message anywhere. Platform-independent. Every other interrupt function in the library already used `LEN_SYS_MASK`; only this one was wrong.

Upstream status as of 2026-01-11 **[DOC]** — **and the archive contradicts itself on the date**:

- `docs/DW1000_LIBRARY_BUG_FIX.md:138` (the source this section is drawn from): *"**Last Commit**: May 11, **2020** (v0.9)"*, *"Appears unmaintained (5+ years without updates)"*.
- `DWS1000_UWB/docs/findings/DW1000_LIBRARY_ALTERNATIVES.md:12` gives Last Update **Jun 2019 (v0.9)**, and `:32` gives *"**Last Release:** v0.9 (June 12, **2019**)"*.

Neither was re-checked against GitHub by this extract. Take "unmaintained since roughly 2019–2020" as the safe reading and **verify the real upstream date before relying on it**; do not quote a single merged date, as an earlier draft of this section did.

The bug was **not reported** in the repository's issues as of the 2026-01-11 search.

### 5.2 Do not over-trust that document

`docs/DW1000_LIBRARY_BUG_FIX.md` closes with *"FIX APPLIED (2026-01-11) … All tests now work correctly: BasicSender/BasicReceiver: Working; DW1000Ranging TAG/ANCHOR: Working; All interrupts: Firing correctly."* **The later record contradicts this.** On 2026-01-17 polling tests report garbage frames of length 1021; on 2026-02-12 RX detection is 33 %; on 2026-02-27 RX is still **0 %** until the J1 jumper. Whatever "working" meant on 2026-01-11, it was not sustained ranging. Fix the bug — it is real — but do not read that document's status section as evidence the stack works.

### 5.3 Alternatives evaluated

**[DOC]** `DW1000_LIBRARY_ALTERNATIVES.md`, surveyed 2026-01-12:

| Library | Stars/Forks | Last update | Status | Notes |
|---|---|---|---|---|
| `thotro/arduino-dw1000` | 565 / 288 | Jun 2019 (v0.9) | Unmaintained | PlatformIO `thotro/DW1000`. IRQ-pin-number bug in old versions; `SPI.usingInterrupt()` unsupported on ESP32; RX interrupt commonly doesn't fire; **4-anchor limit**; Arduino 3.3 V rail can't supply it |
| `F-Army/arduino-dw1000-ng` | 132 / 67 | Nov 2023 | **ARCHIVED** | Adds antenna-delay calibration (absent upstream), rewritten ranging API, `initializeNoInterrupt()`. ESP32 `SPI.usingInterrupt()` deadlock ("interrupt wdt"). PR #164 workqueue / PR #178 separate ISR task are the community fixes. PlatformIO `f-army/DW1000-ng` |
| `Richardn2002/arduino-dw1000-lite` | 13 / 1 | Feb 2021 | Inactive | Deliberately minimal, fixed config (6.8 Mbps, 16 MHz PRF, 128 preamble, ch 5) |
| `mat6780/esp32-dw1000-lite` | 6 / 0 | 2022 | Inactive | **Native polling — IRQ pin unused.** DS-TWR, ±10 cm in 50–500 cm lab conditions, formalised antenna calibration, ESP-NOW for anchor coordination |
| `jaarenhoevel/arduino-dw1000-esp32` | 0 / 0 | Dec 2022 | Unmaintained | thotro fork for ESP32 |
| `Nightsd01/arduino-dw1000-esp-idf` | 4 / 2 | Dec 2022 | Inactive | ng fork for ESP-IDF |
| `RAKWireless/RAK13801_UWB` | 8 / 1 | Sep 2022 | Product-specific | ng-based; TOF **and TDOA**; server-side positioning |
| `jremington/UWB-Indoor-Localization_Arduino` | 221 / 48 | Active 2024-25 | Maintained | Complete positioning system, linear least squares, ±10 cm calibrated, 33 m standard / 50 m+ high power, includes autocalibration; fixes the >4-anchor limit |
| `Makerfabs-ESP32-UWB` | 249 / 67 | Jan 2023 | Product-specific | Bundled `mf_DW1000` fork; Basic 45 m / Pro 200 m |
| `geraicerdas/Cerdas-UWB-Tracker` | 44 / 12 | **May 2025** | Active | Most recently maintained; ESP32-S3, DWM1000 (20 m) and long-range (120 m) |
| `Decawave/uwb-dw1000` | 31 / 15 | 2020 | Official driver | Not Arduino; **native polling** via `dwt_read32bitreg()`; MyNewt/DPL |

**Polling-capable:** `esp32-dw1000-lite` (native), `Decawave/uwb-dw1000` (native), `arduino-dw1000-ng` (partial, `initializeNoInterrupt()`), `thotro` (none).

### 5.4 The decision that shipped

**[LOG]** `RX_DIAGNOSTIC_SESSION_2026-02-27.md` §12, with J1 installed:

- `thotro` RX: **completely broken** — 0 events in 90 s, receiver never detects anything.
- `thotro` TX + `DW1000-ng` RX: **0 good frames**, CRC always fails — **incompatible frame formats**.
- `DW1000-ng` TX + `DW1000-ng` RX: **78–100 %**.

**Conclusion recorded in the archive: DW1000-ng on both ends.** Note the evidence base is a **single bench run** — one thotro TX configuration against one DW1000-ng RX configuration, on two boards (`RX_DIAGNOSTIC_SESSION_2026-02-27.md` §12, cross-lib row: *0 good / 3 CRC / 35 PHE / 2 RFSL*). What that supports is *"they did not interoperate in the one configuration tested"*, not a proven general property of the two libraries. The practical advice still stands: do not mix them across a link without re-testing.

Caveat for anyone starting today: **DW1000-ng has been archived since November 2023** and `thotro` has been dormant since 2019 *or* 2020 (the archive's two sources disagree — §5.1). Neither is maintained. `jremington/UWB-Indoor-Localization_Arduino` and `Cerdas-UWB-Tracker` are the only entries in the survey with post-2024 activity, and neither was evaluated on hardware here.

One more DW1000-ng behaviour worth knowing **[CODE]**: `DW1000NgRanging::correctRange()` (`lib/DW1000-ng/src/DW1000NgRanging.cpp:62`) applies the Decawave **RX-power bias table**, indexed by channel and PRF, on top of the raw ToF range. The anchor firmware calls it. So the reported distance is already power-bias-corrected — do not apply a second power correction without checking.

---

## 6. Antenna delay calibration

### 6.1 Why it dominates

**[DOC]** Measured time is `t_measured = t_ADTX + ToF + t_ADRX`. Antenna delay is the RF propagation delay inside the chip, PCB traces, matching network and antenna. It cannot be trimmed in hardware at manufacture, so **it must be set in firmware, per device**. Typical magnitude ≈ 2.5 ns ≈ **77 cm** of apparent distance; uncalibrated systems show a **50–100 cm constant offset**.

Unit conversion — **[CODE]** `lib/DW1000-ng/src/DW1000NgConstants.hpp:37-38`:

```
DISTANCE_OF_RADIO      = 0.0046917639786159  m per tick   (~4.69 mm)
DISTANCE_OF_RADIO_INV  = 213.139451293       ticks per m
```

So 1 tick ≈ 4.69 mm, 10 ticks ≈ 4.7 cm. Adjustments of ±5–20 ticks are normal.

Reference values **[DOC]**: DW1000 documented default **16384**; Decawave `dwm1001-examples` uses `TX_ANT_DLY = RX_ANT_DLY = 16436`; DWM1000 modules typically calibrate to **16400–16500**; custom PCBs 16300–16600; Bitcraze Loco converges on 16456; Makerfabs 16460–16465; jremington starts at 16450 and lands 16455–16470. Out of range (< 16300 or > 16600) means a hardware fault or a mis-measured reference distance. Production acceptance band 16350–16550. Register is **0x18 (TX_ANTD)**, 16-bit.

### 6.2 The procedure that was actually run

**[LOG]** `ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md`, firmware `tests/test_calibration_tag.cpp`:

1. Place the two nodes at a measured known distance. Here: **24 in = 0.6096 m, antenna-to-antenna, ±2 in**.
2. Run **200 TWR measurements** at the current antenna delay.
3. Compute mean, stddev, min, max.
4. `error = mean_measured − known_distance`.
5. `adj = (error / 2) / DISTANCE_OF_RADIO` — the ÷2 because TX and RX antenna delays contribute about equally, so **each device carries half the correction**.
6. `new_delay = current_delay + adj`; apply to **both** devices.
7. Repeat until |error| < 5 cm.

```
new_delay = current_delay + (measured − actual) / (2 × 0.004692)
```

**Results, round by round [LOG]:**

| Round | Config | Mean measured | Error |
|---|---|---|---|
| 0 | both at 16436 (default) | 0.3121 m (σ ±0.0401, min 0.229, max 0.558, 200/200 samples, 65 timeouts, **no rate logged**) | **−29.7 cm** *(as logged; the arithmetic 0.3121 − 0.610 gives −29.79 cm)* |
| 1 | tag 16405, anchor 16436 | ~0.44 m | ~−17 cm |
| 2 | **both at 16405** | 0.656 m (σ ±0.044, min 0.52, max 0.99, 564/565) | **+4.6 cm** |

Round 1 is the useful control: changing **one** device by 31 ticks moved the reading by ~0.13 m, against a predicted 31 × 4.692 mm = **0.145 m**. Close match, and it confirms that both ends must be updated.

Computed adjustment: `(0.3121 − 0.610) / (2 × 0.004692) = −31.7` → **−31 ticks**, giving **16405**.

### 6.3 Sign convention — get this right, the archive itself does not

The physically correct rule, confirmed by the round-0 → round-2 data (delay decreased 16436 → 16405, measured distance *increased* 0.312 → 0.656 m):

> **Measuring TOO SHORT → DECREASE the antenna delay. Measuring TOO LONG → INCREASE it.**
> Decreasing the delay leaves more time attributed to flight, so the computed distance grows.

`ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` (§Key Formula), `ANTENNA_DELAY_CALIBRATION_2026.md:301-302`, `CALIBRATION_GUIDE.md:216-217` and `CALIBRATION_WEB_RESEARCH.md:1568-1571` all state it this way **in words**, and their worked examples agree. `TWR_ACCURACY_OPTIMIZATION.md` does **not** state the rule in prose anywhere — only its code snippet at `:1926-1927` (`new_delay = current_delay + (int16_t)(error_m / 2.0 * 213.14)`, and the same at `:875`) implies the same sign.

**`include/config.h` states the opposite** — **[CODE]** lines under *Calibration Values — Antenna Delay*: `// Sign: If measuring TOO LONG → decrease delay. TOO SHORT → increase delay.` **That comment is inverted and contradicts the file's own calibrated value.** Anyone reusing `config.h` will iterate the wrong way.

### 6.4 Calibration methods surveyed, in ascending cost

**[DOC]** `CALIBRATION_WEB_RESEARCH.md` §5:

1. **Two-device iterative** (what was used). Official Qorvo separation is **7.94 m** (DW1000 User Manual §8.3.1, APS014); community practice is **1.0 m** because it is easier to measure accurately; 5 m as a compromise. Official method wants **1000 ranges** per iteration; community implementations use 30 / 100 / 1000 by precision tier. Converge to < 5 cm (high-precision: < 2 cm).
2. **Three-node** (official, `IEEE Xplore 9415638`). Measure `d_AB, d_AC, d_BC`, range all three pairs, solve
   `TOF_AB = d_AB/c + AD_A + AD_B` (and the two rotations) — three equations, three unknowns — for **individual** device delays. Achieves **±1 tick (~5 mm)**. Scalable: a new node calibrates against any one already-calibrated node.
3. **Binary search autocalibration** (jremington `ESP32_anchor_autocalibrate.ino`): bracket 16300–16600, 100 samples per probe, 8–10 iterations, **~2–3 min per device**, converges to ±1 tick.
4. **ADS-TWR objective-function** method — minimises `J(AD) = Σ(measured(AD) − actual)²`, accounts for asymmetric processing delay.
5. **Data-driven continuous** — coarse estimator for the fleet plus per-device fine tuning; ESP32-class compute.
6. **Least-squares multi-anchor** — solves anchor positions *and* delays jointly from a mobile tag's rangings; **no tape measure needed**; reported **46 % improvement** in localisation accuracy.
7. **Swarm self-calibration** — N devices, N(N−1)/2 pairwise ranges, overdetermined solve for all delays at once; needs ≥ 3 devices; reported **≤ 0.5 cm** systematic error. *This is the one to revisit for a cooperative swarm — it removes the survey step entirely.*

### 6.5 Validation and quality gates

**[DOC]** Validate at **0.5, 1.0, 2.0, 3.0, 5.0 m** (research protocol: 2 m intervals across 2–40 m). A good calibration shows a **constant small offset**, not a trend:

```
Distance   Measured   Error   StdDev
0.5 m      0.51 m     +1 cm   0.8 cm
1.0 m      1.01 m     +1 cm   1.1 cm
2.0 m      2.01 m     +1 cm   1.5 cm
5.0 m      5.02 m     +2 cm   2.3 cm
10.0 m     10.04 m    +4 cm   3.5 cm
```

Quality tiers: **excellent** < 2 cm mean error and < 1 cm σ; **good** 2–5 cm / 1–2 cm; **acceptable** 5–10 cm / 2–5 cm; **poor** beyond. Linearity check: variance of error across distances < 5 cm is fine, > 10 cm means a distance-dependent error (with DS-TWR, suspect the reference measurement, not clock drift).

Calibration is **channel-specific** — ±5–10 ticks variation across channels **[DOC]**. Calibrate on the channel you will fly.

### 6.6 Corrections not implemented, with their coefficients

The repo's own gap list **[DOC]**, all with numbers ready to use:

- **Temperature**: **2.15 mm/°C per device** → 10 °C swing = 4.3 cm across a link. In ticks: `2.15 / 4.69 = 0.458 ticks/°C`. `adjusted = base + 0.458 × (T_now − T_cal)`. APS014 says **record the calibration temperature**. The DW1000 has an internal temperature sensor and can trim the crystal at runtime to hold ±2 ppm.
- **Supply voltage**: **5.35 cm/V** — matters for battery-powered nodes. `correction_ticks = (53.5 / 4.69) × ΔV`.
- **Power-correlated bias**: reject or correct by RX power. Suggested thresholds: `rxPower < −85 dBm → +5 cm`, `< −80 dBm → +2 cm`. Note §5.4 — DW1000-ng's `correctRange()` already applies the Decawave bias table.
- **First-path check for multipath**: `fpPower − rxPower < −5 dB` (or a 6 dB margin) indicates significant multipath; reject or flag. `rxPower < −90 dBm` → reject.
- **Register tuning**: `AGC_TUNE1` should be **0x8870** (default 0x889B is not optimal at 16 MHz PRF); `DRX_TUNE2` should be **0x311A002D** (default 0x311E0035). Verify whether your library already writes these.
- **Distance-dependent polynomial**: `corrected = d + a1·d + a2·d²`, typically `a1 ∈ [−0.01, 0.01]`, `a2 ∈ [1e-4, 1e-3]`. Literature reports **62 %** positioning-error reduction from a linear compensation model over 1–10 m.

### 6.7 NLOS

**[DOC]** NLOS bias is **always positive**, +10 to +100 cm. Two classes: DP-NLOS (direct path attenuated — correctable) and NDP-NLOS (direct path blocked — reject). Detection options: variance of successive ranges (~90 % detection in harsh indoor), CIR features into an SVM (> 90 %), or the cheap power-ratio test. The repo's recommended balance: LOS → use as-is; ratio −5 to −8 dB → −10 cm correction; ratio < −8 dB → reject. Qorvo has an application note, *"DW1000 Metrics for Estimation of Non Line Of Sight"* (product code da008442).

Environment expectations **[DOC]**: LOS outdoor ±2–5 cm; LOS indoor ±5–10 cm; light multipath ±10–20 cm; heavy multipath (warehouse, metal) ±20–50 cm; through-wall NLOS ±50–200 cm.

---

## 7. Anchor / tag architecture for a swarm — DESIGN ONLY, NEVER BUILT

Everything in this section is **[DOC]**. Two nodes is all that ever ran.

### 7.1 Fixed roles, not dual roles

**[DOC]** `DUAL_ROLE_ARCHITECTURE.md` + `UWB_Swarm_Ranging_Architecture_Research.md`:

- `DW1000Ranging` has **no dynamic role switching** — `startAsTag()` / `startAsAnchor()` fix the role at init; switching needs a rewrite.
- On a 2 KB MCU, dual-role firmware costs memory for code paths that are idle. Separate `initiator.ino` / `responder.ino` is the recommendation, and matches commercial practice (fixed anchors run anchor firmware, mobile tags run tag firmware — MDEK1001, Pozyx).
- Role switching is claimed to add **50–100 µs** timing uncertainty and **5–10 cm** accuracy cost, with the fixed-vs-dynamic crossover around **8 nodes** on constrained MCUs.
- Dual-role becomes reasonable on ESP32-class hardware where peer-to-peer swarm ranging and role failover matter.

### 7.2 The 3-anchor + 1-tag TDMA design

```
Slot 0 (0–100 ms):   Tag <-> Anchor 1   (TWR)
Slot 1 (100–200 ms): Tag <-> Anchor 2
Slot 2 (200–300 ms): Tag <-> Anchor 3
Slot 3 (300–400 ms): Tag computes position; anchors idle
Cycle 400 ms -> 2.5 Hz position updates
```

Parameters: slot 100 ms (a TWR exchange takes 20–30 ms, the rest is guard), guard interval 10 ms, anchor beacon every 1000 ms for time reference, 4–6 nodes maximum on Uno. Anchor addresses 0x01/0x02/0x03, tag 0x0A, network ID 0xDECA. Anchors at e.g. (0,0,10), (10,0,10), (5,10,10). Beacon sync takes Arduino clock stability from ±50 ms/hour drift to ±1–5 ms.

Alternatives considered: token passing; ALOHA-style randomised backoff (`random(50,200) × (attempt+1)`); master-slave polling (simpler sync, single point of failure).

Ranging-plus-messaging strategies: interleaved slots (simple), priority scheduling (ranging first), or **piggyback payload on ranging frames** (most efficient — a 16-byte ranging frame extended to 20–30 bytes; the DW1000 frame limit is 127 bytes standard / 1023 extended).

### 7.3 Scalability figures worth keeping

**[DOC]** `UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md` §3.1:

| Protocol | Effective node count |
|---|---|
| No protocol (fire-and-forget) | 3–5 before > 50 % loss |
| CSMA/CA | 6–10 at 5 Hz/node |
| TDMA round-robin | 10–20 at 5 Hz/node |
| Hybrid TDMA + CSMA/CA | 20–50 at 2–5 Hz/node |

Arduino Uno ceiling: message processing 1–5 ms → ~200 msg/s → with 4-message DS-TWR, ~50 ranging ops/s total; 10 nodes → 5 Hz each. RAM ceiling `(2048 − 1024 stack) / 74 ≈ 14` devices, conservatively `MAX_DEVICES 10`. Recommendation: **≤ 6 nodes with CSMA/CA, ≤ 10 with TDMA**; beyond 15, move to ESP32.

> **Routing note:** this saturation material is the seed for `swarm_communication_protocol`, not for `position_denial_research`. Per `11_routing_v2` §2, if that repo starts modelling contention/interference/throughput collapse it has crossed into `hiverf`. Carried here only because it sets the node-count envelope a cooperative self-localisation scheme has to live inside.

### 7.4 Collision avoidance from ranges

**[DOC]** Thresholds used in the design sketches: `< 1.0 m` emergency stop, `< 2.0 m` reduce to 50 % speed; right-of-way by device address (lower yields to higher); update 5–10 Hz per neighbour pair; Uno tracks 4 neighbours, giving ~1–2 Hz whole-network refresh.

---

## 8. Concrete gotchas — the checklist

Ordered as a bring-up ladder. Items 1–5 are the ones that cost this project seven weeks.

1. **Confirm the chip before choosing a library.** The PCL298336 v1.3 shield was documented internally as a DWM3000 board; the chip ID read **`0xDECA0130` (DW1000)**, not `0xDECA0302` (DW3000). Read the ID first (`getPrintableDeviceIdentifier()` → "DECA0130"). **[LOG]** `CRITICAL_HARDWARE_DISCOVERY.md`.
2. **J1 jumper, pins 1-2.** On the two shields tested, without it the DWM1000 was unpowered and floated at ~3.67 V through the 5 V SPI ESD diodes — above the 3.6 V absolute max — with **0 % good frames** while SPI looked perfect. Caveat: that 3.67 V is computed from the DW1000's own SAR ADC with the raw reading **saturated at 255**, and the source labels it a *"ROOT CAUSE CANDIDATE"* (§1) before §10 confirms the power architecture; **no independent meter reading is recorded anywhere in the archive**. One session, two boards. §4.1.
3. **D8 → D2 jumper wire.** Shield IRQ lands on D8; Uno interrupts only exist on D2/D3.
4. **`PIN_RST = 7`, not 9.** With 9 the chip never gets reset at all, hardware *or* soft.
5. **`SPI_EDGE_BIT` must not be set on AVR** — every register read returns 0xFF otherwise. Guard with `#if !defined(__AVR__)`.
6. **Apply the `LEN_SYS_MASK` fix** if you use `thotro/arduino-dw1000` v0.9 (§5.1). Without it no interrupt-driven path works, silently.
7. **Do not mix libraries across a link.** thotro TX + DW1000-ng RX = 0 good frames; the frame formats differ.
8. **Keep SPI at 2 MHz on AVR.** 16 MHz corrupts reads during RX. Even at 2 MHz, RX-mode SPI reliability is only 75–90 % — use double-read-with-retry and a watchdog to restart a stuck receiver.
9. **In `handleInterrupt()`, check `isReceiveDone()` before `isReceiveFailed()`**, and evaluate `isClockProblem()` last — sticky PLL bits and glitched error bits otherwise discard valid frames.
10. **Long-range mode saturates at short range.** `MODE_LONGDATA_RANGE_LOWPOWER` (110 kbps, 2048 preamble) is built for hundreds of metres; at < 1 m it produced RXPHE on every frame. The working config was 850 kbps / preamble 256.
11. **Flag-only ISRs.** Heavy ISR: 500–2000 µs. Flag-and-return ISR: < 1 µs. Worth 3–8 cm.
12. **Serial output is a rate killer.** A verbose `newRange()` print costs 8–10 ms; CSV form ~2–3 ms.
13. **`config.h`'s sign-convention comment is inverted** (§6.3), and its radio block is **dead configuration** — **[CODE]** `RADIO_DATA_RATE 0` means 110 kbps, but `src/tag_main.cpp` and `src/anchor_main.cpp` build their own `device_configuration_t` with `RATE_850KBPS` and never read those macros. Also `CALIBRATED_ANTENNA_DELAY` is left commented out even though `ANTENNA_DELAY` is set to the calibrated 16405, and `LDO_TUNE_DEV0/1` are read by nothing.
14. **Arduino 3.3 V rail cannot feed a DW1000.** Uno's regulator gives ~50 mA; DW1000 TX peaks at 150–160 mA. Use the shield's DC-DC (that is what J1 pins 1-2 select) or an external LDO. Decouple with 10 µF + 0.1 µF at the DWM1000 VDD pins.
15. **Timestamp counter wraps every ~17.2 s** (2^40 × 15.65 ps). Use the library's wrapping arithmetic; do not subtract raw 64-bit values.
16. **32-bit timestamp arithmetic is valid only under 67 ms** of interval (32 bits at 63.9 GHz) — 4× faster on AVR, but the constraint is real.
17. **Bench-side traps that ate whole sessions:** a corrupted Arduino bootloader on one board (`avrdude: stk500_getsync()`, no reset strategy recovers it — needs ISP re-burn or cable-swap workflow); and USB-bus contention where both ports on one bus break uploads, fixed by re-authorising the device through sysfs (`/sys/bus/usb/devices/<dev>/authorized` 0 → 1) or by moving to a port on another bus. **[LOG]** `ACM1_DIAGNOSIS_FINAL.md`, `IRQ_PIN_INVESTIGATION_2026-02-27.md` Finding 1.

---

## 9. Belongs elsewhere (`hiverf`, `swarm_communication_protocol`) — pointer only, not developed here

Per the agreed boundary — `11_routing_v2_2026-08-18.md` §*"The hiverf boundary, in full"*, **lines 24-34**, with the shared-math row at **line 34** — `hiverf` owns **non-cooperative, other-localization**, and owns the shared **multilateration / CRLB / GDOP implementation**. `position_denial_research` is cooperative self-localisation and **cites** hiverf rather than reimplementing it. Critically, the same section (lines 36-44) fixes the test: **the discriminator is COOPERATION, not the technique** — *is the emitter cooperating with me?* Yes → this repo. No → hiverf. Apply that **before** reaching for the technique list.

Two bodies of SwarmLoc material fall outside this repo:

1. **Multilateration / least squares / GDOP.** `DWS1000_UWB/docs/findings/MULTILATERATION_IMPLEMENTATION.md` (58 KB) is a full treatment: linear least squares as the Arduino-feasible choice (~200 bytes, 5–10 ms per fix), weighted LS, Kalman and particle filters, GDOP computation from the Jacobian, anchor-geometry recipes (4-anchor square in a 10×10 m room → GDOP ≈ 1.5–2.0; tetrahedron-plus-apex → GDOP ≈ 1.0–1.5), and the headline mapping **±10–20 cm ranging → ±20–50 cm position at 2–5 Hz**. Also `findings/UWB_Implementation_Code_Examples.md` §3 (2D circle-intersection trilateration with a collinearity guard, and a weighted Gauss-Newton refinement seeded from a weighted centroid, 5 iterations). **Cite hiverf for the implementation; keep only the geometry rules of thumb that constrain anchor placement.**
2. **Channel-contention / saturation modelling** (§7.3) — belongs to `swarm_communication_protocol`, and crosses into hiverf the moment it models interference or throughput collapse.

**Correction — TDOA was ceded to `hiverf` in error, and stays here.** An earlier draft of this section handed the archive's TDOA material to hiverf on the strength of the word alone. The passage in question — `findings/UWB_Swarm_Ranging_Architecture_Research.md:697-714`, §3.2 Option 3 — describes **cooperative infrastructure TDOA**: *"Anchors transmit synchronized beacons"*, the tag listens passively, position from time differences. Advantages noted — passive tag (lower power), scales to thousands of tags, no tag-side collisions. Disadvantages — requires anchor synchronisation, clock-drift management, and it is not implemented in the standard DW1000 library. Verdict recorded there (`:714`): *"Not suitable for initial SwarmLoc implementation."* `RAKWireless/RAK13801_UWB` is noted as a library with TDOA support. A cooperating tag inside a cooperating anchor network is **`position_denial_research`'s** under the cooperation test above — `11_routing_v2_2026-08-18.md:37-41` now names this extract's over-ceding explicitly, and `01_target_repos_v2.md:52` independently puts *"Multilateration/TDoA/TWR derivations"* in this repo's scope. **Keep it. Cite hiverf only for the non-cooperative case — TDOA against an emitter you do not control.**

What legitimately stays on this side of the line: two-way ranging against cooperative anchors, **cooperative infrastructure TDOA**, antenna-delay calibration, the DW1000 bring-up ladder, NLOS detection on a cooperative link, and the anchor/tag role architecture.

---

## 10. Contradictions, gaps, and things not to trust

1. **"Fix applied, all tests working" (2026-01-11) is contradicted by the next seven weeks of logs.** §5.2.
2. **The two 2026-02-27 TWR runs do not reconcile on *distance*. The *rate* figures are not comparable at all — an earlier draft of this document merged them and invented a contradiction.**
   - **Real, unexplained contradiction.** Same delay, same day, same nominal geometry: `RX_DIAGNOSTIC_SESSION_2026-02-27.md` reports mean **0.626 m** at antenna delay **16436**; `ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` **Round 0** reports mean **0.3121 m** at the same **16436** delay. A 2× distance difference the archive never addresses. Plausible but unverified: the two runs were at different physical separations.
   - **Not a contradiction — different configurations and different firmware.** The **41 Hz** (2487 ranges / 60 s) belongs to `RX_DIAGNOSTIC_SESSION`'s run at delay **16436** with `tests/test_twr_tag.cpp`. The **9.4 Hz** (565 ranges / 60 s) belongs to the calibration session's **Round 2, both devices at 16405**, running `tests/test_calibration_tag.cpp`. Calibration **Round 0** — the 16436 / 0.3121 m round — records **no rate at all**, only *"Samples 200/200, Timeouts 65"*. So there is no measured 9.4 Hz at 16436, and the "4× rate difference at the same delay" framing was wrong. The rate gap across the two firmware builds is still worth explaining (the calibration firmware's OLED display and 200-sample buffering are the obvious suspects), but it is not evidence of an inconsistency in the archive.
   - **Treat 9.4 Hz / +4.6 cm as the defensible headline (it is what `README.md` and `roadmap.md` publish) and treat 41 Hz as an unreproduced observation from a different firmware build at a different antenna delay.**
3. **RX interrupts never demonstrably worked, yet the shipped firmware is interrupt-driven.** `IRQ_PIN_INVESTIGATION_2026-02-27.md` concludes: *"IRQ-based RX is definitively not viable on this DWS1000 shield hardware"* — IRQ pin always LOW for RX events, both with manual `SYS_MASK` writes and with the library's own `attachReceivedHandler()`, while TX callbacks worked 30/30 through the same jumper. Yet **[CODE]** `src/anchor_main.cpp` and `src/tag_main.cpp` set `interruptOnReceived: true` and rely on `attachReceivedHandler()` — and **[LOG]** the 2026-02-27 calibration session records that same build producing 565 ranges in 60 s. Per the caveat governing this document, that is *"worked once, on two boards, according to a log"* — **not** a claim that interrupt-driven ranging works. The archive never revisits the contradiction. Most likely the J1 power fix changed the picture and nobody went back to update the conclusion — **but this is inference, not something the archive states.**
4. **LDO tuning is documented as a breakthrough but is not in the shipping path.** §4.5.
5. **`config.h` sign convention is inverted; its radio block and LDO constants are dead.** §8 item 13.
6. **Both chosen libraries are unmaintained** (thotro dormant since 2019 *or* 2020 — the archive's two sources disagree, §5.1; DW1000-ng archived Nov 2023).
7. **Nothing above 2 nodes was ever executed.** The 5-node swarm firmware, the TDMA scheduler, the trilateration code and the multilateration study are all untested design artefacts.
8. **Every accuracy figure comes from a single pair of boards at ~0.6 m indoors.** There is no multi-distance validation table in the archive with real measured data — the 0.5/1/2/5/10 m table in §6.5 is the *literature's* example of a good calibration, not SwarmLoc's own result. `CALIBRATED_MULTI_DISTANCE` in `config.h` is still commented out.
9. **Antenna-delay reference distance was ±2 in (±5 cm)** — which is the same order as the residual +4.6 cm error. The calibration cannot be claimed better than its ruler.

---

## 11. Source index (attribution)

All paths relative to `~/SwarmLoc`. Nothing was modified.

**Primary research spine (the 176 KB the routing doc identified):**
- `docs/DW1000_LIBRARY_BUG_FIX.md` — library bug overview, upstream status, alternatives shortlist
- `findings/README_Research_Findings.md` — quick reference, expected-performance tables
- `findings/UWB_Swarm_Ranging_Architecture_Research.md` — dual-role analysis, swarm best practice, DW1000 capabilities, TDMA design, references
- `findings/UWB_Implementation_Code_Examples.md` — initiator/responder sketches, trilateration, TDMA, filters (design-stage code, predates working hardware)

**Bring-up and measurement record (`DWS1000_UWB/docs/findings/`):**
- `TWR_ACCURACY_OPTIMIZATION.md` — error budget, ISR/timestamp/clock-drift theory, Uno vs ESP32
- `CALIBRATION_WEB_RESEARCH.md` — the literature review: methods, papers, values, NLOS, registers
- `ANTENNA_DELAY_CALIBRATION_2026.md`, `CALIBRATION_GUIDE.md` — procedures and formulas
- `ANTENNA_DELAY_CALIBRATION_SESSION_2026-02-27.md` — **the calibration that was actually run**
- `RX_DIAGNOSTIC_SESSION_2026-02-27.md` — **J1 root cause + first working TWR**
- `IRQ_PIN_INVESTIGATION_2026-02-27.md` — IRQ dead end, PIN_RST discovery, saturation hypothesis
- `SPI_EDGE_FIX_SESSION_2026-02-12.md` — SPI_EDGE root cause, ISR ordering, SPI reliability by mode
- `LDO_TUNING_FIX_SUCCESS.md` — OTP LDO transfer
- `DW1000_CPLOCK_ISSUE.md` (partial read), `COMPREHENSIVE_RFPLL_RESEARCH_2026-01-19.md` (**not read**) — PLL failure analysis
- `CRITICAL_HARDWARE_DISCOVERY.md` — DW1000 vs DW3000 identification
- `DWS1000_PINOUT_AND_FIX.md`, `DWS1000_JUMPER_WIRE_STATUS.md` — shield pinout, both jumpers
- `DW1000_LIBRARY_ALTERNATIVES.md` — the 11-library survey
- `ACM1_DIAGNOSIS_FINAL.md` — bench infrastructure; `BOOTLOADER_RECOVERY_ISP.md`, `ARDUINO_UPLOAD_TROUBLESHOOTING.md`, `ACM1_SPECIFIC_TROUBLESHOOTING.md`, `UPLOAD_ISSUE_RESOLUTION.md` (**not read** — ~120 KB of Arduino upload/bootloader troubleshooting, no UWB theory content expected)
- **Not read, and each may hold numbers this extract lacks:** `DW1000_RANGING_BEST_PRACTICES.md` (45 KB), `TEST_RESULTS.md` (41 KB, partial), `DWM3000_vs_DW1000_COMPARISON.md` (17 KB), `ESP32_MIGRATION_RESEARCH.md` (68 KB) + the five other `ESP32_*` files, `BUG_FIX_GUIDE.md` (27 KB), `LIBRARY_PATCH.md` (20 KB), `interrupt_debugging.md`, `INTERRUPT_ISSUE_SUMMARY.md`, `DWS1000_IRQ_AND_COMMUNICATION_DEBUG.md`, `library-integration.md`, `hardware-research.md`, `web-research.md`, `code-review.md`, `overview_DW1000.md`, `product_brief.md`, `FIX_RANGING_NOW.md`, `QUICK_FIX.md`, `LIB_FOLDER_CLEANUP.md`, `DW1000_LIBRARY_REVIEW.md`, `DW1000_LIBRARY_SETUP.md`, `DW1000_NO_RANGING_TROUBLESHOOTING.md`, `TX_RX_DEBUG_SESSION_2026-01-19.md`, `ESP32_Wiring_Diagram.txt`, `ESP32_Test_Code_Template.cpp`

**Code that embodies the result:**
- `DWS1000_UWB/src/tag_main.cpp`, `src/anchor_main.cpp`, `src/calibration_main.cpp`
- `DWS1000_UWB/include/config.h`, `platformio.ini` (envs `uno_anchor` / `uno_tag` / `uno_calibration`)
- `DWS1000_UWB/lib/DW1000-ng/` — the library the working firmware uses (vendored, archived upstream)
- `DWS1000_UWB/lib/DW1000/` — thotro, deprecated, but carries the `LEN_SYS_MASK`, `SPI_EDGE`, SPI-speed, ISR-order and LDO patches
- `DWS1000_UWB/tests/` — 60 `.cpp` test programs, of which 14 are `test_rx_v*` iterations of the RX hunt
- `DWS1000_UWB/tests/test_08_multi_node_swarm/` — the 5-node swarm harness (11 files, 4 703 lines; **no execution record in the archive** — see §1)

**Belongs elsewhere:**
- `DWS1000_UWB/docs/findings/MULTILATERATION_IMPLEMENTATION.md` → cite `hiverf`
- `DWS1000_UWB/docs/findings/UWB_SWARM_COMMUNICATION_SATURATION_MITIGATION.md` → `swarm_communication_protocol` (crosses into `hiverf` if it models contention)
- `DWS1000_UWB/docs/findings/DUAL_ROLE_ARCHITECTURE.md` → this repo, but design-only

**Timeline and contributors** (`git log`, 26 commits total, branch `main`):

- Repo opens **2025-05-23** (`32563aa`, `msc_intra`) with GPS + OLED work; `gndpwnd` adds *"Init ToF accuracy to precision relationship"* 2025-05-24; LoRa work through 2025-05-30; `msc_intra` *"switching to DWS1000 for ToF operations"* (`9255e99`, 2025-05-30); `gndpwnd` *"reorg and init DWS1000"* (`24cd010`, **2025-06-13**) creates `DWS1000_UWB/`.
- The DW1000 campaign this document distils runs **2026-01-08 → 2026-02-28**, all by `kaleldev`: `2b2ee40` … `819575f` (*SPI_EDGE_BIT root cause, RX now 33 %*) … `dae195e` (*TWR ranging + antenna delay calibration — all scope requirements met*) … `261f4cf` (*calibration workflow, config.h, OLED display, separate anchor/tag/calibration envs*).
- Later commits (to 2026-05-08) are ESP32 field-node work, not UWB.
- **Attribution for any migrated item: `msc_intra`, `gndpwnd`, `kaleldev`** (three of the four identities `11_routing_v2` Gate 5 names). The DW1000 findings and firmware are `kaleldev`'s; the repo and the DWS1000 subproject were opened by the other two.

No credentials were found in any file read for this extract; SwarmLoc's credential placeholders are clean (`FILL_ME_IN`), consistent with `11_routing_v2` §4.
