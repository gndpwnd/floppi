# ESP32 Core-1 Scheduling Budget Analysis (`USE_BAROMETER` + `USE_GPS`)

> Date: 2026-05-20
> Agent: `fc-core1-budget@flight_controller:1`
> Status: READ-ONLY analysis. No source/config/test edits. No git commits. No builds.
> Purpose: Quantify contradiction **C-2** from `session3_readiness_2026-05-20.md` §3.3
> — "ESP32 Core-1 may be over-subscribed once barometer + GPS-passthrough land
> alongside the existing WiFi server + 2 Hz telemetry POST" — and convert it into
> concrete placement guidance for the W2 (barometer) and W5 (GPS) coding agents.

**Inputs:**
- `docs/findings/session3_readiness_2026-05-20.md` — C-2 (over-subscription), C-4 (OLED bus).
- `docs/findings/barometer_integration_spec_2026-05-20.md` — proposes a baro `loop()`-slice poll.
- `docs/findings/gps_passthrough_spec_2026-05-20.md` — proposes a dedicated Core-1 FreeRTOS task.
- `docs/findings/swarm_api_contract_2026-05-20.md` — the 2 Hz outbound POST + WS server.

**Source verified @HEAD (commit 9dd60ca):** `src/main.cpp`, `src/web_server.cpp`,
`src/api_client.cpp`, `src/wifi_manager.cpp`, `src/ota.cpp`, `src/imu.cpp`,
`src/motors.cpp`, `include/config.h`.

---

## 1. Executive summary

**The concern in C-2 is real but the verdict is nuanced — Core 1 is NOT
CPU-throughput-bound; it is *latency*-bound, on one specific worst-case path.**

| Scenario | Core-1 verdict | Reason |
|---|---|---|
| **Today** (WiFi + WS + 2 Hz POST + OLED) | **Healthy.** Not over-subscribed. | Steady-state Core-1 work is a few ms per ~10 ms loop. The blocking POST is the only spike, ~50–300 ms typical / up to 2 s worst-case, but the only victim today is the OLED/WS cadence — nothing safety-relevant. |
| **+ Barometer** (as the spec proposes — a `loop()` slice) | **Marginal — the slice WILL jitter.** | The baro poll lives *inside the same `loop()`* as the blocking POST. Every 500 ms one baro sample is delayed by the POST duration. `BARO_SAMPLE_RATE_HZ` becomes an *average*, not a cadence. Acceptable for telemetry, but the spec's clean "20 Hz" is misleading. |
| **+ Barometer + GPS** (GPS as a separate task per its spec) | **Acceptable, with one risk.** | The GPS *task* is immune to the POST (separate schedulable entity) — good. The risk is **UART RX byte loss**: while the loop-task blocks 2 s in the POST, the GPS task still gets CPU (it is a peer task), so GPS is fine. The residual risk is total Core-1 CPU during a POST burst + WS broadcast + baro I2C, not throughput. |

**Bottom-line verdict:** Core 1 has ample CPU headroom even with both features.
The single real hazard is the **blocking 2 Hz HTTP POST** (`api_client.cpp:98`),
which stalls *everything sharing the `loop()` task* for its full duration. The fix
is structural, not load-shedding: **anything that needs a predictable cadence must
be its own FreeRTOS task, not a `loop()` slice.** That makes the GPS spec correct
and the barometer spec's `loop()`-slice choice the one thing Session 3 should
override — exactly as C-2 recommends.

---

## 2. Current Core-0 / Core-1 task map

ESP32 runs two cores. Core 0 is owned exclusively by the flight loop. Core 1 runs
the Arduino `loopTask` (which the ESP32 Arduino core pins to Core 1 by default —
confirmed below) plus the WiFi/lwIP system tasks the SDK spawns.

| Task | Core | Priority | Period / blocking | What it does |
|---|---|---|---|---|
| `FlightCtrl` | **0** | 3 (high) | 1 kHz hard loop; busy-waits to rate via `loopRate()` (`motors.cpp:333-340`, `main.cpp:236`) | IMU read → Madgwick → PID → mixer → motor PWM. `xTaskCreatePinnedToCore(flightControlTask,"FlightCtrl",4096,NULL,3,…,0)` — **`main.cpp:376-384`**. Never touches WiFi. |
| Arduino `loopTask` (the `loop()` body) | **1** | 1 (Arduino default) | Soft ~10 ms cycle: ends with `vTaskDelay(pdMS_TO_TICKS(10))` (`main.cpp:456`) | Runs, per iteration: `xQueueReceive` (5 ms timeout, `main.cpp:424`) → `populateNetworkData` → `renderDisplay` (rate-limited 10 Hz, `main.cpp:435`) → `handleWiFi` → `handleWebServer` (incl. 10 Hz WS broadcast) → `handleApiClient` (2 Hz **blocking** POST) → `handleOTA`. **No `xTaskCreatePinnedToCore` for Core 1 — it is the implicit Arduino task.** |
| WiFi / lwIP / `ipc`/`esp_timer` system tasks | 0 & 1 | SDK-assigned (high) | event-driven | The ESP-IDF WiFi stack. Runs partly on Core 0, partly Core 1. CPU cost is real but bursty and outside app control. |

**Key facts established here:**
- There is exactly **one** application task on Core 1 today: the Arduino
  `loopTask`. Everything in §1's "today" column is *sequential code inside that
  single task*. There is no parallelism on Core 1 between display, WiFi handling,
  and the POST — they are statements in one `loop()`.
- `loop()` runs on Core 1: the `#else` branch of `main.cpp:416-458` is the ESP32
  path, and `flightControlTask` is explicitly pinned to Core 0, leaving the
  Arduino `loopTask` (which the Arduino-ESP32 core creates on `ARDUINO_RUNNING_CORE`
  = 1 by default) as the Core-1 occupant. The comment at `main.cpp:387`
  ("Display + WiFi on Core 1") confirms intent.
- The flight loop (`flightControlTick`, `main.cpp:168-237`) is fully isolated on
  Core 0. **No barometer or GPS analysis below threatens Core 0** — all specs
  correctly assert this and the source bears it out (`getIMUdata()` on `Wire`,
  Core 0; nothing else shares).

---

## 3. The blocking-POST problem

This is the load-bearing finding. The swarm-API contract (§2, §5) and
`api_client.cpp` agree: the 2 Hz outbound telemetry POST is a **blocking** call.

**The exact mechanism (`api_client.cpp:59-109`):**
- `handleApiClient()` is called every `loop()` iteration but early-returns until
  `API_POST_INTERVAL_MS` (default **500 ms**, `api_client.cpp:33-35`) has elapsed.
- When it fires, it builds JSON then calls `http.POST(...)` (`api_client.cpp:98`).
  `HTTPClient::POST()` is **synchronous**: it opens a TCP connection, sends, and
  blocks the calling task until a response arrives or the timeout expires.
- `http.setTimeout(2000)` (`api_client.cpp:96`) caps the block at **2 s**.

**On which core:** Core 1 — it is a statement inside the Arduino `loopTask`.

**What stalls while it blocks:** *Everything else in the same `loop()` body*,
because they are sequential statements in one task. For the duration of the POST:

| Stalled by a blocking POST | Consequence | Severity |
|---|---|---|
| `handleWebServer()` WS broadcast | 10 Hz telemetry stream pauses; up to ~20 WS frames skipped in a 2 s worst case | Cosmetic — clients see a gap |
| `renderDisplay()` 10 Hz OLED | OLED freezes for the POST duration | Cosmetic |
| `handleWiFi()` reconnect poll | Reconnect attempt deferred | Negligible (5 s interval anyway) |
| `handleOTA()` | OTA packet servicing paused | Negligible (OTA only while disarmed) |
| `xQueueReceive` of fresh `DisplayData_t` | Next telemetry snapshot read is late | Cosmetic — stale data for one cycle |
| **The Core-0 flight loop** | **NONE** | Core 0 is a separate core + separate task; the POST cannot touch it. The `dataMux`/`wifiCmdMux` spinlocks are held microseconds, not across the POST. |

**Quantify N:** A healthy LAN POST to a local server completes in **~30–150 ms**
(TCP connect + small-body POST + response). A slow/lossy link or an absent server
runs to the full **2000 ms timeout**. So the per-`loop()` stall injected by the
POST is **~30 ms typical, 2000 ms worst-case, once every 500 ms**.

**Implication:** Core 1 is not CPU-saturated — between POSTs the `loop()` is
nearly idle (it `vTaskDelay`s 10 ms). The problem is purely **head-of-line
blocking**: one slow synchronous call freezes every other `loop()`-resident
consumer. Any new feature placed *inside* `loop()` inherits this stall. Any new
feature placed in its *own task* does not (the scheduler preempts the blocked
`loopTask` and runs the peer task).

---

## 4. Barometer placement analysis

The barometer spec (§3 Option A, §7 WS-2) proposes adding a
`BARO_SAMPLE_RATE_HZ` (default 20 Hz) **poll slice to the ESP32 `loop()`**,
"mirroring the 10 Hz display slice" (`barometer_integration_spec` §7 WS-2).

**Cost of one baro read.** A BMP280/BMP388 pressure+temperature read is 2–3 I2C
register transactions; at the 400 kHz bus speed the IMU uses (`imu.cpp:39`), the
wire time is well under 1 ms, and total call cost including the Bosch library's
overhead is **~1–2 ms**. (An MS5611 is worse — its request/read conversion needs
~10 ms *between* trigger and read, but the spec recommends BMP280 and flags
MS5611 as a state-machine variant; a `loop()`-slice MS5611 would be especially
bad and is a further argument for a task.)

**How a `loop()`-slice baro interleaves with the POST.** If the baro poll is a
slice of `loop()`, it executes *sequentially* with `handleApiClient()`. The 1–2 ms
read itself is cheap and harmless. The problem is the **cadence**:

- `loop()` cycles roughly every ~10 ms (the `vTaskDelay(10)`), so a 20 Hz
  (50 ms) baro slice fires every ~5th iteration — fine in steady state.
- But every 500 ms, one `loop()` iteration runs long by the POST duration
  (~30 ms typical). The baro slice in *that* iteration — and any baro slice
  whose 50 ms deadline fell *during* the POST — is delayed until the POST
  returns. On a worst-case 2 s POST, **~40 baro samples' worth of wall-clock
  time elapses with zero baro reads**, then they resume.
- Net: `BARO_SAMPLE_RATE_HZ` is delivered as a *long-run average*, not a steady
  cadence. Telemetry altitude will show a ~30 ms (typical) to ~2 s (worst-case)
  refresh gap aligned to every POST.

**Is that acceptable?** For a *telemetry-only* relative-altitude readout (the
spec's entire scope — §3, §8), a periodic 30 ms hiccup is invisible to a human
and tolerable to a flight computer consuming the stream. A 2 s gap on a dead-link
POST is more noticeable but the link is dead anyway. So the `loop()`-slice is
**not dangerous** — it is just **not the clean 20 Hz the spec implies**, and the
`BARO_LPF` PT1 filter (spec §5) assumes evenly-spaced samples, so jittered
sampling slightly distorts the filter's effective cutoff.

**Recommendation: dedicated low-priority Core-1 FreeRTOS task.** Not a `loop()`
slice, not Core 0.

- **Not Core 0** — the spec is emphatic and correct (§3, §4): the baro must never
  enter the 1 kHz real-time path. Confirmed.
- **Not a `loop()` slice** — it inherits the POST stall (above) and the MS5611
  variant would be much worse.
- **A task** — gives the baro its own schedulable cadence. While the `loopTask`
  is blocked in the POST, the scheduler runs the baro task on schedule. This also
  makes the firmware *consistent with the GPS spec*, which already chose a task
  for a structurally identical Core-1 sensor — resolving the C-2 inconsistency
  directly. Concrete parameters in §7.

---

## 5. GPS placement analysis

The GPS spec (§4) proposes a **dedicated FreeRTOS task pinned to Core 1,
priority 1**, blocking on `Serial1.available()` with a ~10 ms timeout, draining
a ring buffer, framing `$…\r\n` NMEA sentences into a spinlock-guarded snapshot.

**Does adding a 3rd Core-1 consumer push it over? — No.**

- The GPS task is a **separate schedulable entity**, not a `loop()` slice. It is
  therefore **immune to the blocking POST**: while the `loopTask` sits in
  `http.POST()`, the FreeRTOS scheduler still time-slices the GPS task onto
  Core 1 (and onto Core 0 idle, but it is Core-1-pinned). UART bytes are not
  lost — the ESP-IDF UART driver buffers RX in an interrupt-fed hardware+driver
  FIFO regardless of which task is running, and the GPS task drains it whenever
  it is scheduled.
- **CPU cost is tiny.** At 9600 baud a NEO-6M emits ~1 NMEA burst/s (~few hundred
  bytes); even a 10 Hz NEO-M8N at 38400 baud is a few KB/s. Draining that into a
  ring buffer + sentence framing is microseconds of CPU per wake. The task spends
  almost all its life in `vTaskDelay`/blocked-on-UART.
- The only thing the GPS task *adds* to Core 1 is a small, bursty, low-priority
  CPU demand and one task stack (~2–4 KB RAM per `session3_readiness` §5). Core 1
  is far from CPU-bound (§3), so this is comfortably absorbed.

**Priority.** Spec says priority 1. **Concur — keep it at 1.** It must be:
- *Below* Core 0's `FlightCtrl` (priority 3) — moot anyway, different core, but
  correct in principle.
- *Equal to* the Arduino `loopTask` (also priority 1). Equal priority means the
  scheduler round-robins them — acceptable: neither GPS framing nor `loop()`
  telemetry is hard-real-time. Raising GPS above the `loopTask` would let a
  byte-storm starve the web server; lowering it below risks RX-buffer pressure
  only under sustained CPU contention that does not exist here. **Priority 1 is
  the right call.**

**Stack size.** The spec does not pin a number. Recommend **2560–3072 bytes**
(`xTaskCreatePinnedToCore(..., 3072, ...)`). The task body is shallow — a UART
drain, a ring-buffer copy, a `memcpy` under a spinlock — but it links against the
Arduino `HardwareSerial`/UART driver. 3072 B is safe with margin; do not go below
2048 B. The `FlightCtrl` task uses 4096 B for far heavier math, so 3072 B for a
byte-shuffler is conservative.

**Is Core 0 viable for GPS? — No, and not desirable.** Core 0 must stay reserved
for the 1 kHz loop; a UART task there would steal cycles from `loopRate()`'s
busy-wait budget. Core 1 is correct.

**Verdict:** the GPS spec's placement is **sound as written**. The only addition
is the explicit stack number (§7).

---

## 6. Combined budget — baro + GPS + WiFi + POST

Estimated Core-1 utilization with `USE_BAROMETER` + `USE_GPS` + `USE_WIFI` +
`USE_WEB_SERVER` + `USE_API_SERVER` all enabled, baro moved to a task per §4:

| Core-1 consumer | Form | Steady-state CPU | Peak behaviour |
|---|---|---|---|
| Arduino `loopTask` (display + WiFi handling + WS) | task, prio 1 | low — a few ms per ~10 ms cycle | spikes to POST duration every 500 ms |
| 2 Hz outbound POST | inside `loopTask` | ~30 ms once per 500 ms | up to 2 s on dead link |
| WiFi / lwIP system tasks | SDK tasks | bursty, low-moderate | spikes on association/retransmit |
| **Barometer** (recommended: task, prio 1) | task | ~1–2 ms every 50 ms ≈ <4 % | none — task cadence is POST-immune |
| **GPS** (task, prio 1) | task | <1 % — mostly blocked on UART | small burst per NMEA fix |

**Headroom:** Plenty, in raw CPU terms. The sum of *steady-state* Core-1 demand
is well under 20 % of one core. ESP32 Core 1 is not throughput-constrained even
with both new sensors. RAM is the only finite item worth noting — two extra task
stacks (~3 KB baro + ~3 KB GPS) plus the GPS ring buffer (256 B) — and
`session3_readiness` §5 already judged that comfortable.

**Where the risk is:** *Latency*, concentrated entirely in the blocking POST and
*only* for consumers that share the `loopTask`. With baro and GPS both as their
own tasks, the residual risk surface is:
1. **WS/OLED cadence jitter** during a POST — already true today, cosmetic, not
   worsened by the new sensors.
2. **A pathological 2 s POST timeout** coinciding with a WiFi-stack burst — a
   brief Core-1 CPU crunch, but the two sensor tasks are low-priority and will
   simply be scheduled slightly late; no data loss (UART FIFO buffers GPS; the
   baro just samples a few ms late).

**There is no scenario in this combined build where Core 1 is genuinely
"over-subscribed" in the CPU sense.** C-2's concern is best restated as: *the
`loop()` is a poor host for cadence-sensitive work because of the blocking POST*
— a structural critique, fully addressed by the task-not-slice recommendation.

---

## 7. Recommendations for the W2 (barometer) and W5 (GPS) coding agents

Concrete, implementation-ready guidance. This section feeds the drivers directly.

### W2 — Barometer (`USE_BAROMETER`)

| Decision | Recommendation |
|---|---|
| Core | **Core 1.** Never Core 0. (Confirms baro spec §3/§4.) |
| Form | **Dedicated FreeRTOS task — NOT a `loop()` slice.** This overrides `barometer_integration_spec` §7 WS-2's "poll slice" and resolves C-2. |
| Task creation | `xTaskCreatePinnedToCore(baroTask, "Baro", 3072, NULL, 1, &handle, 1)` — Core 1, priority 1, 3072 B stack. Spawn it from the `#ifdef USE_ESP32` setup block (`main.cpp:352-369`), after `setupWiFi()`/`setupWebServer()`, gated by `#ifdef USE_BAROMETER`. |
| Period | Task body: `readBarometer()` then `vTaskDelay(pdMS_TO_TICKS(1000/BARO_SAMPLE_RATE_HZ))`. At the default 20 Hz that is a clean 50 ms cadence, **immune to the POST stall** — which the `loop()`-slice form cannot deliver. |
| Cross-core handoff | Publish pressure/altitude/temp into a **spinlock-guarded snapshot** (`portMUX_TYPE`), exactly the `dataMux`/`latestData` pattern in `web_server.cpp:33-34`. The telemetry-integration workstream reads that snapshot to fill `DisplayData_t`. Do not write `DisplayData_t` from the baro task directly. |
| Avoiding POST jitter | Solved structurally by being a task. The 1–2 ms I2C read may still land while the WiFi stack is busy, costing a few ms of scheduling delay — acceptable and far below one sample period. |
| MS5611 note | If `BAROMETER_MS5611` is enabled, the ~10 ms request/read conversion fits naturally in a task (two `vTaskDelay`-separated phases) and would be *unworkable* as a `loop()` slice — another reason the task form is mandatory, not optional. |
| I2C bus | Use `Wire1`, NOT `Wire` (Core-0 IMU bus). Heed `session3_readiness` C-1: do **not** default `BARO_SDA/SCL_PIN` to GPIO 25/26 — those are `MOTOR_PIN_1/2`. That is a pin issue, out of this budget's scope, but the baro task must own `Wire1` init itself. |

### W5 — GPS (`USE_GPS`)

| Decision | Recommendation |
|---|---|
| Core | **Core 1.** Never Core 0. (Confirms GPS spec §4.) |
| Form | **Dedicated FreeRTOS task** — already what the spec proposes. Correct; keep it. |
| Task creation | `xTaskCreatePinnedToCore(gpsTask, "GPS", 3072, NULL, 1, &handle, 1)` — Core 1, **priority 1** (equal to the `loopTask`; see §5), **stack 3072 B** (the spec left this unspecified — pin it here; do not go below 2048 B). |
| Period / blocking | Block on `Serial1.available()` with a ~10 ms timeout, then `vTaskDelay(pdMS_TO_TICKS(10))` when idle — exactly as the spec §4 states. The task is POST-immune because it is a separate schedulable entity. |
| Avoiding POST jitter | No action needed — UART RX bytes are buffered by the ESP-IDF driver FIFO independently of which task runs, so even a 2 s POST in the `loopTask` cannot cause GPS byte loss. This is the structural advantage the baro must also adopt. |
| Cross-core handoff | Spinlock-guarded latest-sentence snapshot, same `portMUX_TYPE` pattern (spec §4 already says this). Consumed only by Core-1 readers. |
| `setupGPS()` | Must be **non-blocking** — `Serial1.begin()` then return. No "wait for first fix" loop (spec §7 item 3). A blocking setup would delay `setupWebServer()`. |

### Shared note for both W2 and W5

Both sensor tasks should be created **after** `setupWiFi()` so the WiFi stack's
own tasks exist first, and **before** the `FlightCtrl` task is spawned
(`main.cpp:376`) is *not* required — order vs `FlightCtrl` is irrelevant (different
core). Keep both at priority 1 so neither can starve the web server.

---

## 8. Open questions — needs hardware measurement

This analysis is static (no build, no run, no scope). The following must be
measured on real hardware before the numbers above are treated as final:

1. **Actual POST latency distribution.** §3 estimates ~30–150 ms typical. The
   real figure depends on the LAN, the `swarm_api` server's response time, and
   ESP32 TCP stack behaviour. Measure the time `http.POST()` (`api_client.cpp:98`)
   takes across many samples — both with a healthy server and with the server
   absent (to confirm the 2 s timeout is the true ceiling).
2. **Real `loop()` cycle time.** §3/§4 assume ~10 ms (the `vTaskDelay`). The
   actual cycle is `10 ms + work`, and WS broadcast / `renderDisplay` add
   variable cost. Measure with a `micros()` delta around the `loop()` body.
3. **Baro read cost.** The ~1–2 ms estimate for a BMP280 read at 400 kHz needs
   confirmation against the actually-vendored Bosch library — some Bosch drivers
   insert their own `delay()` calls. If the chosen library blocks, the task form
   becomes even more important.
4. **MS5611 conversion timing** — if that variant is ever enabled, the ~10 ms
   trigger-to-read gap must be measured, not assumed.
5. **WiFi-stack CPU share on Core 1.** The lwIP/WiFi system tasks' CPU cost
   under load is SDK-internal and bursty; only profiling (e.g.
   `vTaskGetRunTimeStats`) reveals whether a POST + WS broadcast + a sensor task
   ever genuinely contend. §6 assumes comfortable headroom — verify.
6. **GPS RX FIFO depth vs sustained CPU contention.** §5 asserts no byte loss
   because the UART driver buffers RX. Confirm the driver RX buffer is large
   enough for the chosen baud + fix rate that a 2 s `loopTask` block plus a
   WiFi burst cannot overflow it before the GPS task is next scheduled.
7. **Task stack high-water marks.** The recommended 3072 B stacks for baro and
   GPS are conservative estimates; confirm with `uxTaskGetStackHighWaterMark()`
   after a sustained run and trim or grow accordingly.

---

*Analysis complete. Read-only. No source, config, or test files modified. No git
commits, no builds. This is the only file written by this agent.*
