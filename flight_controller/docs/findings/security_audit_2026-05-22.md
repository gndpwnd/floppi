# Flight Controller Firmware — Security Audit (External Attack Surface)

**Date:** 2026-05-22
**Auditor:** security-reviewer (Claude Code Orchestra)
**Scope:** `flight_controller/` firmware (Teensy + ESP32). Focus: external attack
surface (WiFi command/telemetry, OTA), command parsers, and memory safety.
**Method:** static read-only review of real source. No code was modified.

> **Threat model.** The drone runs in WiFi **STA mode** on a shared LAN. Any host
> on that LAN (including a compromised laptop, a rogue AP-joined device, or
> anyone with the WiFi PSK) can reach the ESP32's TCP/80 web server, the
> WebSocket, the swarm API client target, and the ArduinoOTA UDP/TCP service.
> There is **no application-layer authentication anywhere** in the firmware.
> The honest framing is: *on the flight network, command-and-control of the
> aircraft is available to any LAN peer.* The findings below quantify that.

---

## Severity summary

| Severity | Count | IDs |
|----------|-------|-----|
| **P0** | 3 | SEC-01 (unauth flight control), SEC-02 (unauth OTA / RCE), SEC-03 (arbitration spoof / no auth at all) |
| **P1** | 2 | SEC-04 (telemetry/GPS leakage), SEC-05 (WS broadcast buffer truncation) |
| **P2** | 3 | SEC-06 (no transport encryption), SEC-07 (WiFi cmd timestamp/failsafe race), SEC-08 (I2C ISR length-only validation) |
| **P3** | 3 | SEC-09 (info disclosure on `/` and `/api/status`), SEC-10 (PPM/PWM no real frame validation), SEC-11 (mDNS hostname truncation — cosmetic) |

**Top 3 issues:** SEC-01 (unauthenticated full flight control over WiFi),
SEC-02 (unauthenticated OTA = remote code execution on the FC), SEC-03 (no
authentication on *any* network surface — the root cause behind 01/02/04).

**Memory safety:** No exploitable buffer overflow found. The recently-added
RC-channel clamp to `[1000,2000]` is **complete and correct** across all three
WiFi entry points and the central `getCommands()` constrain. GPS NMEA parsing is
correctly bounds-checked. Details in the memory-safety section.

---

## P0 — Critical

### SEC-01 — Unauthenticated full flight control over WiFi
**Files:** `src/web_server.cpp:220-253` (`POST /api/commands`),
`src/web_server.cpp:150-176` (WebSocket `/ws` `WS_EVT_DATA`),
`lib/RadioComm/radioComm_ext.cpp:166-176` (`setWifiCommandChannels`),
`src/control.cpp:343-366` (`armedStatus`).

**Impact.** Any LAN peer can POST or send a WebSocket frame with
`{"ch1":...,"ch6":...}` and directly drive the six control channels. There is no
token, session, origin check, or client allow-list. Tracing the data path:

1. `POST /api/commands` / `/ws` accepts the JSON unconditionally and calls
   `setWifiCommandChannels(...)` (`web_server.cpp:250`, `:172`).
2. That writes the spinlock-guarded `wifiCmdChannels[]` buffer
   (`radioComm_ext.cpp:166`).
3. Core 0's `getCommands()` reads it and applies it to `channel_3_pwm` (throttle),
   `channel_5_pwm` (arm switch), etc.
4. `armedStatus()` (`control.cpp:347-352`) arms the aircraft when
   `channel_3_pwm < 1050` **and** `channel_5_pwm < 1500`. Both are attacker-
   controlled. So the attacker can **arm** (`ch3=1000, ch5=1000`) and then
   **throttle up** (`ch3=2000`) — a complete remote takeover, including
   spin-up of motors.

The clamp to `[1000,2000]` only bounds the *value* of each channel; it does
nothing to authenticate the *sender*. A perfectly in-range command is still a
hostile command.

**Remediation (statically fixable, no hardware):**
- Add a shared-secret check to the command endpoints: require a header/field
  (e.g. `X-Floppi-Token`) compared against a value compiled in from
  `wifi_credentials.h` (a new `#define FLOPPI_CMD_TOKEN`). Reject with 401 if
  absent/mismatched. This is a few lines in both the POST body callback and the
  `WS_EVT_DATA` branch.
- Better: HMAC the command payload with a per-drone key + monotonic counter to
  also defeat replay (see SEC-07). The token approach is the minimum bar.
- Document clearly that without transport security (SEC-06) the token is
  visible to a passive sniffer; the token is a *control-plane* gate, not a
  confidentiality control.

**Validation:** static fix is unit-/build-testable; end-to-end arm-over-WiFi
behavior should be **bench-validated** with motors removed/props off before
trusting the fix.

---

### SEC-02 — Unauthenticated OTA firmware update (remote code execution)
**File:** `src/ota.cpp:24-59` (`setupOTA`).

**Impact.** `ArduinoOTA.begin()` is called with **no** `setPassword()` or
`setPasswordHash()` anywhere in the tree (confirmed by grep across `src/` and
`include/`). ArduinoOTA with no password accepts firmware from any host that can
reach the OTA service. Any LAN peer can therefore **flash arbitrary firmware**
onto the flight controller — full, persistent code execution on the device that
drives the motors. This is strictly worse than SEC-01: it survives reboot and
can disable every safety check.

The only guard is `handleOTA()` skipping `ArduinoOTA.handle()` while `armedFly`
is true (`ota.cpp:61-66`). That prevents flashing *mid-flight* but does nothing
for a disarmed-on-the-bench or staged-for-launch drone, and an attacker can
simply wait for a disarmed window.

**Remediation (statically fixable, no hardware):**
- Set an OTA password: `ArduinoOTA.setPasswordHash(<md5>)` (preferred) or
  `setPassword(...)`, sourced from a new `#define OTA_PASSWORD` in
  `wifi_credentials.h`. The hash form keeps the plaintext out of flash strings.
- Refuse to compile `USE_OTA` builds when no OTA password is defined
  (`#error` guard) so the weak-default state is impossible to ship.
- Consider gating OTA behind a build flag that is **off by default** for field
  builds, since OTA is a convenience for development.

**Validation:** static fix is build-testable; OTA-reject-on-bad-password should
be **bench-validated** once with `pio run -t upload`.

---

### SEC-03 — No authentication on any network surface (systemic)
**Files:** `src/web_server.cpp` (all handlers), `src/api_client.cpp`,
`src/ota.cpp`.

**Impact.** This is the root cause that makes SEC-01, SEC-02, and SEC-04
exploitable rather than theoretical. The web server (`web_server.cpp:187-284`),
the WebSocket, and OTA all trust the network. The design notes in MEMORY.md
("WiFi STA mode... swarm on same network") assume the LAN is trusted, but a
*flight controller that spins motors* warrants defense even on a "trusted" LAN
(guest devices, PSK leakage, ARP/DNS spoofing of the swarm server, etc.).

**Remediation (statically fixable):**
- Treat the command/OTA surfaces as privileged: shared token (SEC-01) + OTA
  password (SEC-02) at minimum.
- Separate "read" telemetry from "write" command authority — telemetry can stay
  open if desired (subject to SEC-04), but writes must be gated.
- Longer term: per-drone keys provisioned via `wifi_credentials.h`, documented
  in the swarm-API contract so `swarm_api/` sends the credential.

**Validation:** statically fixable; the swarm-API side (`swarm_api/`) is **out
of this work zone** and must be updated in tandem — flag a cross-project handoff.

---

## P1 — High

### SEC-04 — Telemetry data leakage incl. GPS lat/lon over unauthenticated API
**Files:** `src/web_server.cpp:61-135` (`serializeDisplayData`, `/api/status`,
`/ws`), `src/api_client.cpp:86-155` (POST to `API_SERVER_URL`),
`src/gps.cpp:205-219` (`gpsTelemetryNMEA`).

**Impact.** `GET /api/status`, the `/ws` broadcast, and the `/` text page expose,
to any unauthenticated LAN peer:
- **GPS position** — raw NMEA sentences are relayed verbatim
  (`web_server.cpp:122-129`, fed by `gpsTelemetryNMEA`). GGA/RMC sentences carry
  latitude/longitude/altitude. This is precise aircraft location disclosed to
  anyone on the network.
- MAC address, SSID, IP, RSSI (`web_server.cpp:91-97`), armed state, full
  attitude, raw IMU, motor outputs, free heap, uptime.

The same body is additionally **pushed** to `API_SERVER_URL` over plain HTTP
(`api_client.cpp:140` — `http.begin(telemetry_url)`, an `http://` URL per the
`wifi_credentials.h` template), so the position trail also crosses the wire in
cleartext to the swarm server and any on-path observer.

Severity is P1 (not P0): it is disclosure, not control. But location tracking of
an aircraft + MAC (device fingerprint) is a real privacy/operational-security
harm and combines with SEC-01 (an attacker who can see "armed: true" knows when
to strike). The MEMORY.md note that this was "already flagged" is consistent —
this entry assesses it as **P1, confirmed live in code**.

**Remediation (statically fixable):**
- Gate telemetry behind the same token as commands if confidentiality matters,
  or at minimum make GPS/position fields **opt-in** behind a build flag so a
  default build does not broadcast location.
- Move the swarm POST to HTTPS (`WiFiClientSecure` + pinned CA) — this needs the
  `swarm_api/` side to terminate TLS; cross-project handoff.
- Free heap / uptime / firmware internals are minor; lowest priority within this
  finding.

**Validation:** statically fixable (flag-gating); the HTTPS migration touches
`swarm_api/` and needs **integration validation**.

---

### SEC-05 — WebSocket telemetry broadcast can truncate JSON (fixed 512 B buffer)
**File:** `src/web_server.cpp:300-307` (`handleWebServer` broadcast path).

**Impact.** The `/ws` broadcast serializes telemetry into a fixed `char buf[512]`
(`web_server.cpp:304`). When `USE_GPS` and `USE_BAROMETER` are both compiled in,
the document includes a baro block (~90 B), the full net block, IMU, motors, and
an up-to-82-char NMEA sentence plus all keys. `serializeJson()` will not overflow
the buffer (it respects the size argument and returns the truncated length), so
this is **not** a memory-safety bug — but the emitted frame can be **silently
truncated to invalid JSON**, breaking consumers and, more importantly, this is
the same struct the swarm relies on for situational awareness. The
`api_client.cpp:135` POST path uses the same 512 B with a documented ~480 B worst
case — thin margin; adding any field will push it over.

Contrast: `/api/status` uses `AsyncJsonResponse` (`web_server.cpp:204`) which
grows dynamically and does **not** have this limit — so the two telemetry paths
can disagree.

**Remediation (statically fixable):**
- Size the `/ws` and POST buffers from the actual worst case (compute, don't
  guess) or switch to a dynamic/`String` serialization like `/api/status`
  already does. At minimum bump to 768–1024 B and add a `len == 0`/truncation
  check that logs a warning.

**Validation:** statically fixable; confirm frame size with GPS+baro on at the
bench once.

---

## P2 — Medium

### SEC-06 — No transport encryption (plaintext HTTP/WS/OTA)
**Files:** `src/web_server.cpp` (`AsyncWebServer server(80)`),
`src/api_client.cpp:140`, `src/ota.cpp`.

All command, telemetry, and OTA traffic is cleartext. A passive sniffer on the
LAN recovers GPS position, MAC, and (if a token is added per SEC-01) the token
itself unless it is HMAC-based. ESP32 can do TLS but at real CPU/heap cost on
Core 1. **Remediation:** document the limitation; offer an optional
`WiFiClientSecure` build for the swarm POST; rely on HMAC-with-counter rather
than bearer tokens for the command path so sniffing the token is useless.
Statically fixable but heavy; coordinate with `swarm_api/`.

### SEC-07 — WiFi command timestamp / failsafe replay & race
**File:** `lib/RadioComm/radioComm_ext.cpp:166-212`, `radioComm.cpp:208-260`.

`setWifiCommandChannels` stamps `wifiCmdTimestamp = millis()` on every write, and
`readWifiCmd` marks the source active whenever `ts > buf.timestamp`. Two issues:
(a) **No replay protection** — a captured command frame replayed by an attacker
is accepted as fresh (the FC has no nonce/counter). (b) The 500 ms
`OVERRIDE_TIMEOUT_MS` failsafe means a brief injected burst keeps the WiFi source
"active" and, under arbitration, can hold authority for 500 ms after the last
packet. Combined with SEC-01 this lengthens the window of a hijack. **Remediation
(statically fixable):** add a monotonic command counter that must strictly
increase, reject stale/duplicate counters; tighten the WiFi failsafe timeout.
Bench-validate failsafe timing.

### SEC-08 — I2C command ISR validates length only, not content
**File:** `lib/RadioComm/radioComm_ext.cpp:95-112` (`onI2CReceive`).

The ISR accepts any 12-byte write to the Wire1 slave address and copies it into
`i2cCmdChannels[]` with **no checksum or magic-byte validation** (unlike the
Serial parser at `radioComm_ext.cpp:53-59` which XOR-checks, and iBUS which
checksums). A glitched or spoofed I2C master (or bus noise) injects arbitrary
channel values; the only backstop is the downstream `[1000,2000]` constrain. I2C
is a short-range wired bus so the network-attack severity is P2, but for a
command source that can arm/throttle, content validation is warranted.
**Remediation (statically fixable):** add a checksum byte to the I2C frame
contract (13 bytes: 12 data + XOR) mirroring the Serial protocol, validate in the
ISR. Bench-validate against the real I2C master.

---

## P3 — Low / informational

### SEC-09 — Verbose info disclosure on `/` and `/api/status`
**File:** `src/web_server.cpp:256-280`, `:61-135`. MAC, SSID, free heap, uptime,
loop timing leak device fingerprint and aid an attacker timing a strike. Subsumed
by SEC-04; lower priority. Statically fixable (trim fields).

### SEC-10 — PPM/PWM "frame valid" heuristic is spoofable / weak
**File:** `lib/RadioComm/radioComm_rc.cpp:230-235`, `:319-324`. Validity is
inferred from `channel_1_raw != 1500 || channel_2_raw != 1500` — any noise that
nudges a channel off exactly 1500 µs is treated as a live frame, and a stuck-at-
1500 condition is treated as no-signal. These are wired protocols (low remote
risk) and a known limitation of edge-timed PPM/PWM; documented here for
completeness. Not network-exploitable. Statically improvable (range-sanity check)
but needs bench validation with a real receiver.

### SEC-11 — mDNS hostname buffer (cosmetic)
**File:** `src/web_server.cpp:55,191`; `src/ota.cpp:29-31`. `mdns_hostname[20]`
and the OTA `suffix[8]` are sized with `snprintf` and cannot overflow; the
hostname is derived from the device's own MAC, not network input. No issue —
recorded only to show it was checked.

---

## Memory-safety review (network-reachable paths)

**No exploitable overflow found.** Specifics:

- **RC-channel clamp completeness — VERIFIED COMPLETE.** The `[1000,2000]`
  `constrain()` is applied at all three WiFi entry points: WebSocket
  (`web_server.cpp:166-171`), POST (`:243-248`), and again centrally in
  `getCommands()` (`radioComm.cpp:185-190`) for *every* source. Channel reads use
  ArduinoJson's `| default` operator so missing/garbage keys fall back to safe
  defaults. No gap.
- **GPS NMEA parser — SAFE.** `Gps::poll()` bounds every accumulator write with
  `_accum_len < (GPS_NMEA_MAX - 1)` (`gps.cpp:129`) and drops over-length
  sentences (`:135`). The `memcpy(_latest, _accum, _accum_len + 1)` at
  `gps.cpp:120` copies at most `GPS_NMEA_MAX` bytes into a `GPS_NMEA_MAX` buffer
  (`GPS_NMEA_MAX=83`, gps.h:58) — fits exactly. `gpsTelemetryNMEA`
  (`gps.cpp:205-214`) clamps `n` to `cap-1` before `memcpy` and NUL-terminates.
  GPS is RX-only passthrough, but even adversarial NMEA cannot overflow.
- **`strncpy` uses — SAFE.** `api_client.cpp:67` and `wifi_manager.cpp:123,138,
  147` all pass `sizeof(dst)-1` and manually NUL-terminate. Sources are the
  device's own MAC/SSID, not network input.
- **JSON buffer sizing — SAFE for memory, see SEC-05 for truncation.**
  `deserializeJson` on `(data,len)` is bounded by the framework. The fixed 512 B
  *output* buffers cannot overflow (serializeJson honors size) but can truncate.
- **Integer / PWM math — SAFE.** Channel values are `uint16_t`/`unsigned long`
  constrained to `[1000,2000]`; SBUS `map()` (`radioComm_rc.cpp:45-50`) maps
  fixed protocol ranges. No attacker-controlled array index reaches an
  out-of-bounds access; `getRadioPWM` has a `default` case (`radioComm.cpp:205`).
- **I2C ISR copy — SAFE for memory** (12 bytes into a 12-element read), but see
  SEC-08 for the missing content validation.

## Secrets in the repo — CLEAN

`wifi_credentials.h` and `wifi_certs.h` are git-tracked (confirmed via
`git ls-files`) but contain **placeholders only**: `"YourNetworkName"`,
`"YourPassword"`, `REPLACE_WITH_YOUR_CA_CERTIFICATE`, empty client cert/key. No
real SSID, password, eduroam identity, or PEM material is committed. The tracked-
with-placeholders pattern matches the documented project policy. **No secret-
leak finding.** Recommendation: keep it that way — consider a CI grep guard that
fails the build if these files ever diverge from the placeholder sentinels.

---

## Fix-routing summary

**Safe for a coding agent to fix statically (no hardware):**
- SEC-01 command-auth token (code + `wifi_credentials.h` define)
- SEC-02 OTA password + `#error` guard
- SEC-03 write-vs-read authority split (follows from 01/02)
- SEC-04 flag-gate GPS/position telemetry; mark HTTPS as cross-project
- SEC-05 buffer sizing / dynamic serialization
- SEC-07 command counter (firmware side)
- SEC-08 I2C checksum byte in the contract + ISR check
- SEC-09 trim info fields
- Optional CI placeholder-guard for credentials

**Needs bench validation (hardware):**
- SEC-01 end-to-end arm-over-WiFi rejection (props off!)
- SEC-02 OTA reject-on-bad-password round-trip
- SEC-05 worst-case frame size with GPS+baro enabled
- SEC-07 failsafe timing
- SEC-08 against the real I2C master
- SEC-10 PPM/PWM validity with a real receiver

**Cross-project handoffs (outside this work zone — `swarm_api/`):**
SEC-03/04/06 require the ground station to send the command credential and to
support HTTPS. Flag to the swarm-API maintainer; do not implement here.

---
*Audit only — no source files were modified. Report path:
`flight_controller/docs/findings/security_audit_2026-05-22.md`.*
