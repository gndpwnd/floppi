# Security Audit — swarm_api Ground Station

- **Date**: 2026-05-22
- **Auditor**: security-reviewer (Claude Code Orchestra)
- **Scope**: swarm_api's own attack surface — the FastAPI ground-station server that controls a fleet of ESP32 drones over WiFi (REST + WebSocket). Read-only review of `src/`, `src/api/`, `main.py`, `manager.py`, `drone.py`, `config.py`, `config.json`, `requirements.txt`, deploy scripts, static dashboard.
- **Mode**: Audit only. No code was changed. Server was NOT started.
- **Deployment fact**: Runs with `--host 0.0.0.0 --port 8080` (per `deploy/service.sh:100,104`, `config.json:20`, MEMORY). Anything that can route to the host can reach every endpoint.

## Threat model in one sentence

This server is an **unauthenticated remote control for a fleet of flying machines**. Any actor who can open a TCP connection to `host:8080` can arm, throttle, steer, batch-command, and emergency-disarm the entire fleet, and can rewrite the server's persisted config. There is no login, no API key, no network ACL in the application layer.

---

## Severity summary

| Severity | Count |
|----------|-------|
| P0       | 2     |
| P1       | 3     |
| P2       | 4     |
| P3       | 3     |
| **Total**| **12**|

Statically-fixable (no design decision needed): 5 — F-04, F-06, F-07, F-09, F-12.
Needs design decision (auth model / threat posture): 7 — F-01, F-02, F-03, F-05, F-08, F-10, F-11.

---

## P0 — Critical

### F-01 (P0) No authentication or authorization on any endpoint — full fleet control is open
- **Where**: All routers. `src/main.py:69-72` (no auth dependency on `include_router`), `src/api/drones.py:64` (`POST /api/drones/{mac}/command`), `src/api/fleet.py:56` (`POST /api/fleet/command`), `src/api/fleet.py:91` (`POST /api/fleet/disarm`), `src/api/system.py:63` (`PUT /api/system/config/network`), `src/api/ws.py:34` (`/ws/dashboard`). No `Depends(...)`, `HTTPBearer`, API key, or session check exists anywhere in the codebase (grep for `Depends/Authorization/HTTPBearer` returns nothing in `src/`).
- **Impact**: Anyone who can reach `0.0.0.0:8080` can:
  - Arm a drone and ramp throttle (`ch5`/`ch3` via `/api/drones/{mac}/command` or `/api/fleet/command`) — physical safety hazard (props spin, aircraft takes off / crashes).
  - Batch-command or disarm the whole fleet.
  - Enumerate the fleet, MACs, IPs, mDNS names, RSSI, last-seen.
  - Mutate persisted config (see F-04).
  The "command_token" plumbing in `config.py`/`drone.py`/`manager.py` authenticates **swarm_api → drone firmware** (downstream), NOT **client → swarm_api**. It does nothing to protect this server's own surface.
- **Remediation**: Add an authentication layer in front of all state-changing and fleet-control endpoints. Minimum viable: a shared bearer token / API key required via `Depends`, plus a separate stronger gate (or physical-button confirmation) for arm/throttle and disarm. Decide whether the dashboard authenticates via session cookie vs. token. Consider role separation (read-only telemetry viewer vs. operator).
- **Class**: Needs design decision (auth model). High urgency.

### F-02 (P0) No network binding / origin restriction + command WebSocket has no origin check → drive-by fleet control from a browser
- **Where**: `src/api/ws.py:34-65` — `dashboard_ws` calls `await ws.accept()` with no `Origin` validation and no auth, then forwards `{"type":"command","mac":...,"channels":...}` straight to `drone.send_command(channels)` (`ws.py:58-65`). Bind address is `0.0.0.0` (`config.json:20`).
- **Impact**: WebSocket connections are **not subject to the browser same-origin policy** the way `fetch` is — a malicious web page the operator visits can open `ws://<groundstation-ip>:8080/ws/dashboard` and send command frames. Combined with F-01 (no auth) and F-03 (no CORS posture / no `Origin` enforcement), this is a classic **cross-site WebSocket hijacking → fleet takeover**. The attacker need only know/guess the ground-station LAN IP and a drone MAC (MACs are also discoverable via the unauthenticated `GET /api/drones`).
- **Remediation**: (1) Validate the `Origin` header on WS handshake against an allowlist and reject mismatches before `accept()`. (2) Require auth on the WS (token in subprotocol or first message, validated before processing commands). (3) Default-bind to a trusted interface or localhost and document an explicit opt-in for `0.0.0.0`. (4) Validate/clamp `channels` server-side on the WS path (see F-06).
- **Class**: Needs design decision (origin allowlist + auth), but the `Origin` check itself is statically implementable today.

---

## P1 — High

### F-03 (P1) No CORS policy declared — relying on implicit defaults, no defense-in-depth
- **Where**: `src/main.py` — `CORSMiddleware` is never added (grep for `CORS` in `src/` returns nothing). 
- **Impact**: There is no `allow_origins=["*"]` misconfiguration (that would be worse), but there is also **no deliberate CORS posture**. Simple cross-origin `POST` requests (e.g. form-style `application/x-www-form-urlencoded`, or `text/plain` JSON-ish bodies) are *sent* by browsers without a preflight; without auth (F-01) and without CSRF protection, a malicious page can fire state-changing POSTs at `/api/fleet/command` / `/api/fleet/disarm`. JSON `Content-Type` POSTs do trigger preflight and would be blocked by the absence of permissive CORS — but this is incidental protection, not a control. The real exposure is the WS path (F-02) and any non-preflighted request shape.
- **Remediation**: Add an explicit, restrictive CORS config (named allowlist of dashboard origins, no wildcard with credentials). Pair with auth + CSRF tokens / SameSite cookies for the dashboard. Do not adopt `allow_origins=["*"]`.
- **Class**: Needs design decision (which origins), but statically implementable once decided.

### F-04 (P1) `PUT /api/system/config/network` mutates and persists config.json with no auth and no path control
- **Where**: `src/api/system.py:63-76` → `save_config(config)` → `src/config.py:68-71` writes `config.json` via `open(path, "w")`.
- **Impact**: An unauthenticated caller can rewrite `command_rate_hz`, `telemetry_poll_interval_ms`, `connection_timeout_ms` and have them **persisted to disk**, surviving restart. Setting `command_rate_hz` and poll intervals to extreme allowed values lets an attacker tune the server's behaviour (e.g. flood drones, or starve telemetry). Note the write target is a fixed module-level `CONFIG_PATH` (`config.py:15`) — **no path traversal / arbitrary-file-write here** (the `path` arg is never caller-controlled), so this is config tampering, not arbitrary FS write. The Pydantic bounds (`ge/le`) limit values but not the right to change them.
- **Remediation**: Gate behind auth (F-01). Treat config mutation as an admin-only operation. Optionally make config read-only at runtime and require a restart for changes.
- **Class**: Needs design decision (auth), but the gating is statically implementable.

### F-05 (P1) SSRF / pivot via attacker-registered drone IP or mDNS hostname
- **Where**: `POST /api/drones` (`src/api/drones.py:96`) → `manager.add_drone` (`manager.py:271`) accepts `mdns_hostname` (and indirectly `last_ip` via persisted config) with no validation. `DroneClient.base_url`/`ws_url` (`drone.py:63-79`) build `http://{ip}` / `ws://{mdns_hostname}.local` and the manager then issues outbound `httpx.get(.../api/status)` and `websockets.connect(...)` (`drone.py:93,180`) on a schedule. `resolve_drone` (`manager.py:164-190`) also resolves arbitrary hostnames via `getaddrinfo`.
- **Impact**: A caller who can register a drone (no auth — F-01) can point the server at an **arbitrary internal URL**: `mdns_hostname` is concatenated into a URL with no allowlist, and once `poll_status`/WS loops run, swarm_api makes recurring outbound requests to that host. This is a server-side request forgery / internal-network pivot primitive: probe internal hosts, hit `http://169.254.169.254` style metadata endpoints if reachable, or use the ground station as a relay. The telemetry response is parsed (`resp.json()`) and surfaced via `GET /api/drones`, leaking response content back to the attacker.
- **Remediation**: Validate `mac` (regex), `mdns_hostname` (restrict to expected `floppi-XXXX` pattern, no dots/schemes/ports), and `last_ip` (must be a valid private/LAN address; reject loopback, link-local metadata ranges, public IPs). Gate registration behind auth.
- **Class**: Needs design decision (allowed address policy) + statically-implementable input validation.

---

## P2 — Medium

### F-06 (P2) WebSocket command path bypasses channel clamping/validation present on the REST path
- **Where**: `src/api/ws.py:60-65` — `channels = msg.get("channels", {})` is forwarded raw to `drone.send_command(channels)`. By contrast `send_command` (`drone.py:118-130`) *does* clamp to 1000–2000 and the REST path uses the `CommandPayload` Pydantic model (`drones.py:13-20`). The WS path has no model, no key whitelist, no type check.
- **Impact**: Lower than it looks because `send_command` itself clamps numeric values to 1000–2000 (`drone.py:130`). However, `channels` is an untyped dict: non-numeric values (`max(1000, min(2000, "x"))`) raise inside `send_command` and are swallowed; arbitrary extra keys are ignored by the per-key loop. The real gap is **lack of schema/type validation on a network-facing message** — malformed frames are silently dropped rather than rejected, and there is no rate limiting on this command channel.
- **Remediation**: Parse WS command frames through a Pydantic model (reuse `CommandPayload`), validate `mac` format, reject malformed frames explicitly, and add per-connection command rate limiting.
- **Class**: Statically fixable.

### F-07 (P2) Drone identifier (`mac`) and metadata are unvalidated free-form strings
- **Where**: `config.py:18-21` (`mac: str`, `name: str` — no pattern), `drones.py:23-28` (`AddDronePayload`), `drones.py:31-34` (`UpdateDronePayload` — `name`, `group`, `tags` arbitrary), `manager.update_drone_metadata` (`manager.py:136-162`) writes them straight into persisted config.
- **Impact**: No MAC format enforcement (any string registers a "drone"); arbitrary `name`/`group`/`tags` strings are persisted to `config.json` and reflected back into `GET /api/drones` and the dashboard. The dashboard renders fleet data into the DOM (`static/index.html` renderFleet) — unsanitized `name`/`group`/`tag` values are a **stored-XSS vector in the dashboard** if rendered via innerHTML. (Dashboard JS rendering should be checked by a frontend reviewer; the server persisting unsanitized strings is the root cause here.) Also enables config-bloat / junk-entry DoS.
- **Remediation**: Add Pydantic validators: `mac` regex (`^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$`), length-cap `name`/`group`/`tags`, restrict tag charset. Ensure the dashboard escapes these on render.
- **Class**: Statically fixable (server side).

### F-08 (P2) No rate limiting / abuse controls anywhere
- **Where**: All endpoints; notably `/api/fleet/command`, `/api/fleet/disarm`, `/ws/dashboard`, `POST /api/drones`.
- **Impact**: Unauthenticated callers can flood the command/disarm endpoints (fight legitimate operator inputs — a "command war"), spam drone registrations to bloat config, or open unlimited dashboard WebSockets (`_dashboard_clients` set grows unbounded — `ws.py:16,47`). Combined with F-01 this is an availability + safety risk.
- **Remediation**: Add rate limiting (e.g. slowapi / reverse-proxy limits), cap concurrent dashboard WS clients, and cap registered-drone count.
- **Class**: Needs design decision (limits + mechanism).

### F-09 (P2) `config.json` is world-readable on disk and written with default permissions; designed to hold `command_token` secrets
- **Where**: `config.py:26-40` defines `command_token` fields (per-drone and fleet-wide shared secrets for firmware auth). `save_config` (`config.py:68-71`) writes with default umask permissions. `.gitignore` does NOT exclude `config.json` (only `*.log`, `venv/`, etc.), so a populated config with real tokens risks being committed.
- **Impact**: When `command_token` is set, `config.json` holds a fleet-wide shared secret in plaintext on disk with no permission hardening, and `GET /api/system/config` (`system.py:50-54`) returns `config.model_dump()` — **which includes the `command_token` fields** — to any unauthenticated caller. That endpoint leaks the firmware command secret to anyone who can reach the server (info-leak, escalates F-01 into drone-firmware compromise). The current committed `config.json` has `command_token: null`, so no secret is leaked today, but the design exposes it the moment a token is configured.
- **Remediation**: (1) Exclude `command_token` from the `/api/system/config` response (or redact). (2) Set restrictive file mode (0600) on write. (3) Add `config.json` (or a tokens file) to `.gitignore` / move secrets to env vars. (4) Gate the config endpoints behind auth.
- **Class**: Statically fixable (redaction + chmod + gitignore).

---

## P3 — Low / informational

### F-10 (P3) Telemetry from drones is trusted and reflected without bounds
- **Where**: `drone.poll_status` (`drone.py:104-112`) stores `resp.json()` verbatim into `last_telemetry`; `connect_ws` (`drone.py:184-191`) does the same for WS messages. `summary()` (`drone.py:214-233`) reflects it to `GET /api/drones` and the dashboard. A drone telemetry field `net.ip` is even used to **overwrite the drone's own IP** (`drone.py:105-107`) with no validation.
- **Impact**: A compromised or spoofed "drone" (or anything answering at the polled URL — see F-05) can inject arbitrary JSON that the server stores unbounded and reflects to dashboard clients (XSS-via-telemetry if dashboard uses innerHTML), and can rewrite its own routing IP via `net.ip`, redirecting future polls/commands (SSRF reinforcement). No size cap on stored telemetry.
- **Remediation**: Validate/whitelist expected telemetry fields, validate `net.ip` before adopting it, cap stored payload size, ensure dashboard escapes telemetry on render.
- **Class**: Needs design decision (telemetry schema) — partly statically fixable (ip validation, size cap).

### F-11 (P3) Server bound to `0.0.0.0` by default with no documented network-isolation requirement
- **Where**: `config.json:20`, `deploy/service.sh:100,104`, defaults in `common.sh:58-60`.
- **Impact**: Maximizes reachability of an unauthenticated control plane. Acceptable only if the host is on a strictly trusted, isolated network — but nothing in the app or deploy enforces or documents that.
- **Remediation**: Default to a trusted interface or localhost; require explicit operator opt-in for `0.0.0.0`; document the network-isolation assumption prominently. Pair with auth (F-01).
- **Class**: Needs design decision.

### F-12 (P3) Broad `except Exception` swallows errors; potential for inconsistent error verbosity
- **Where**: `drone.py:113` (`except (httpx.RequestError, Exception)`), `drone.py:142,169,195`, `ws.py:29`, `main.py:46`. FastAPI's default unhandled-exception path returns generic 500s (good), but the catch-all `except Exception` masking everywhere can hide real failures and complicate incident analysis.
- **Impact**: Low security impact directly. No evidence of stack traces being returned to clients (no `debug=True`, no custom verbose handlers). The `except (httpx.RequestError, Exception)` pattern is redundant (`Exception` already covers `RequestError`) and over-broad, swallowing programming errors as if they were network errors.
- **Remediation**: Narrow exception handling to expected network/serialization errors; let unexpected exceptions surface to logs. Confirm FastAPI is never run with `debug=True` in production.
- **Class**: Statically fixable.

---

## Things checked that are NOT problems

- **No `subprocess` / `os.system` / `eval` / `exec` / `shell=True`** anywhere in `src/` (grep clean). No command injection surface in the app.
- **No path traversal in config I/O**: `open()` calls in `config.py:60,70` use a fixed module-level `CONFIG_PATH`; the `path` parameter is never caller-controlled. `add_drone`/config endpoints cannot redirect the write target.
- **No `allow_origins=["*"]` misconfiguration** — because there is no CORS middleware at all (see F-03; absence is its own issue, but not the wildcard footgun).
- **REST command values are clamped** to 1000–2000 in `send_command` (`drone.py:130`) and typed via Pydantic on REST routes.
- **`StaticFiles` mount** (`main.py:75-77`) serves a fixed directory; no user-controlled path component.

## Dependency posture (no installs run — versions from `requirements.txt`)

All pins are lower-bound (`>=`), not exact, so the installed versions float. Floors observed:
- `fastapi>=0.109.0`, `uvicorn[standard]>=0.27.0`, `websockets>=12.0`, `httpx>=0.27.0`, `zeroconf>=0.131.0`, `pydantic>=2.6.0`, `jinja2>=3.1.0`.

Notes:
- No obviously-abandoned or known-critical-CVE pin among these floors as of the floors listed, but **unpinned `>=` ranges are a supply-chain/repro risk** — a fresh install can pull a newer (or yanked-then-replaced) version with regressions. Recommend pinning exact versions + a lockfile (e.g. `pip-tools`/`uv`), and running `pip-audit` in CI. `jinja2` is listed but no template rendering was found in `src/` (dead dependency — minor attack-surface/footprint note).
- This was a static read of `requirements.txt` only; no environment scan was performed.

---

## Recommended remediation order

1. **F-01** auth on all control endpoints (unblocks the whole posture).
2. **F-02** WS origin check + auth + bind hardening.
3. **F-09** stop leaking `command_token` via `/api/system/config`; chmod 0600; gitignore.
4. **F-05 / F-07** input validation on drone registration (mac/ip/mdns/name).
5. **F-04 / F-08** gate config mutation; add rate limiting.
6. **F-06 / F-10 / F-12** WS schema validation, telemetry validation, narrow exceptions.
7. **F-03 / F-11** explicit CORS allowlist; document/enforce network isolation.
