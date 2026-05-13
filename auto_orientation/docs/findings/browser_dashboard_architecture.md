# Browser Dashboard Architecture for auto_orientation

## Recommendation Summary

- **Vanilla HTML/JS/CSS + Three.js (vendored, gzipped, ~150 KB) served from LittleFS** — no build step, source-controlled assets in `data/www/`, uploaded with `pio run --target uploadfs`.
- **One ESPAsyncWebServer instance with a single WebSocket `/ws` that multiplexes tagged JSON events** (`{"t":"fused",...}`, `{"t":"cal.progress",...}`, `{"t":"pid.curve",...}`, `{"t":"cmd",...}`) — mirrors the working pattern in `flight_controller/src/web_server.cpp`.
- **Multi-page SPA shell** (`/`, `/calibrate`, `/balance-capture`, `/pid-tune`, `/telemetry`, `/ota`, `/settings`) — each route a thin HTML stub importing a shared `app.js`/`ws.js`. Mobile-portrait first, big touch targets, Service-Worker cache for offline-after-load.

---

## 1. Tech Stack

| Option                            | Bundle (gz)    | Build step          | Storage                | Verdict           |
| ---                               | ---            | ---                 | ---                    | ---               |
| **Vanilla + Three.js (vendored)** | ~150 KB        | None                | `data/www/` → LittleFS | **RECOMMENDED**   |
| Vue 3 + Vite                      | ~80–200 KB     | `npm`, `vite build` | LittleFS after build   | Overkill for v1   |
| htmx + Alpine.js                  | ~30 KB (no 3D) | None                | LittleFS or PROGMEM    | Weak for 3D       |
| Server-rendered HTML over WS      | tiny           | None                | PROGMEM                | Too chatty, no 3D |

The dashboard's heavy lifting is the Three.js quaternion view; the rest is trivial DOM. A build step only pays when component reuse / types matter — neither does here. `three.min.js` is ~150 KB gzipped ([Three.js Installation](https://threejs.org/docs/#manual/en/introduction/Installation)). Vendor it to `data/www/vendor/three.min.js`; never link a CDN at runtime — the dashboard must work on the bot's own LAN.

**Dev loop**: edit `data/www/`, `pio run --target uploadfs` (~5 s), browser-reload. No watcher, no node_modules.

## 2. Storage Strategy

The FC (`flight_controller/src/web_server.cpp`) inlines its UI as `snprintf` text — no asset filesystem. That does not scale. Three options:

1. **LittleFS (recommended)** — separate partition, uploaded via `pio run -t uploadfs`. Real files under `data/www/`. ESPAsyncWebServer serves them with `server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html")` ([AsyncStaticWebHandler](https://github.com/me-no-dev/ESPAsyncWebServer#serving-static-files)). `.gz` precompressed files auto-served with `Content-Encoding: gzip`.
2. **PROGMEM** — `static const char INDEX_HTML[] PROGMEM = R"raw(...)raw"`. Simplest deploy but bloats `.rodata` and forces rebuild-flash for every CSS tweak. OK for a single page only.
3. **`board_build.embed_txtfiles`** — PlatformIO's binary embed. Equivalent to PROGMEM with a linker step. Not worth it.

**platformio.ini** (current `auto_orientation/platformio.ini` is AVR-only — add ESP32 env):

```ini
[env:esp32_dashboard]
platform = espressif32
board = esp32dev
framework = arduino
board_build.filesystem = littlefs
board_build.partitions = min_spiffs.csv   ; or default.csv; gives ~1.5 MB LittleFS
data_dir = data                            ; auto_orientation/data/www/ ends up at /www on LittleFS
build_flags =
    -D USE_ESP32
    -D USE_WIFI
    -D USE_WEB_SERVER
lib_deps =
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/AsyncTCP.git
    bblanchon/ArduinoJson@^7
```

`-t upload` flashes firmware, `-t uploadfs` flashes the LittleFS image. Runtime API: [ESP32-Arduino LittleFS docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/littlefs.html).

## 3. Page Structure & API Map

Multi-page (each a static `.html` in `data/www/`), shared `app.js`/`ws.js`. No client router — the browser navigates natively, every page reconnects its own WebSocket.

| Route              | Purpose                                            | API endpoints consumed                                                                          |
| ---                | ---                                                | ---                                                                                             |
| `/` (index.html)   | Live 3D orientation + cal status + nav             | `WS /ws` (`t:"fused"`), `GET /api/status`                                                       |
| `/calibrate`       | Magnetometer wizard + ellipsoid-fit progress       | `POST /api/calibration/{start,capture,save}`, `WS /ws` (`t:"cal.progress"`)                     |
| `/balance-capture` | Balance-point capture for self-balancing robot     | `POST /api/calibration/start` (`{"mode":"balance_point"}`), `…/capture`, `WS /ws` (`t:"fused"`) |
| `/pid-tune`        | Auto-tune kickoff + live Kp/Ki/Kd curves           | `POST /api/pid_tune/{start,cancel}`, `GET /api/pid_tune/status`, `WS /ws` (`t:"pid.curve"`)     |
| `/telemetry`       | Full sensor JSON dump, table view, copy-clipboard  | `WS /ws` (`t:"fused"`), `GET /api/status`                                                       |
| `/ota`             | Drag-drop `.bin` upload + progress                 | `POST /update` (ArduinoOTA HTTP handler)                                                        |
| `/settings`        | WiFi SSID (read-only), mounting recal, reboot      | `GET /api/system`, `POST /api/calibration/start` (`{"mode":"mounting"}`), `POST /api/reboot`    |

## 4. Three.js Quaternion Visualization

Minimal scene (`data/www/orientation.js`, ~80 lines):

```text
scene
  PerspectiveCamera (FOV 45, looking at origin from +Z)
  AmbientLight + DirectionalLight
  AxesHelper (world frame: red=X, green=Y, blue=Z)
  ArrowHelper (magnetic north, cyan, +X)
  deviceMesh = Group:
    BoxGeometry (PCB body 80×60×10) + MeshStandardMaterial
    child AxesHelper (body frame)
```

Init: `new THREE.WebGLRenderer({antialias:true})`, append to `<div id="view3d">`, `renderer.setSize(w, w)` (square, responsive). `requestAnimationFrame` loop renders. [Three.js Quaternion docs](https://threejs.org/docs/#api/en/math/Quaternion).

WebSocket update — Three.js takes quaternions natively:

```javascript
function setQuaternion(w, x, y, z) {
  deviceMesh.quaternion.set(x, y, z, w);   // note: THREE order is (x,y,z,w)
}
ws.onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  if (m.t === "fused" && m.orientation?.quaternion) {
    const q = m.orientation.quaternion;
    setQuaternion(q.w, q.x, q.y, q.z);
  }
};
```

30 Hz on the wire; render loop runs at vsync (60 Hz). Slerp between WS updates if jitter is visible. `src/output/json_formatter.h` already emits `orientation.quaternion.{w,x,y,z}` in exactly this shape — zero schema work.

## 5. WebSocket Protocol

Single `/ws`, all frames JSON text with a `"t"` discriminator. The FC's `web_server.cpp:103–119` already shows the parse pattern (`AwsFrameInfo` + `deserializeJson` on `WS_EVT_DATA`) — reuse verbatim. Example frames:

```json
{"t":"fused","ts":12345678,"orientation":{"valid":true,"quaternion":{"w":0.707,"x":0,"y":0,"z":0.707},"calibration":{"system":3,"accel":3,"gyro":3,"mag":2}},"position":{"valid":false}}
{"t":"cal.progress","stage":"mag_ellipsoid","samples":47,"target":100,"fit_residual":0.018,"recommend_more":["+Z","-Y"]}
{"t":"pid.curve","axis":"pitch","elapsed_s":12.4,"kp":1.85,"ki":0.42,"kd":0.063,"oscillation_amp":0.31,"converged":false}
{"t":"cmd","action":"capture"}
{"t":"cmd","action":"pid_tune.start","axis":"pitch","duration_s":30}
```

Server → client: `fused` (10–30 Hz), `cal.progress` (per sample), `pid.curve` (~5 Hz during tune). Client → server: `cmd` (button presses).

## 6. REST API Surface

Firmware ↔ UI contract (full detail in `wifi_telemetry_integration_design.md` §4):

- `GET /api/status` → full EKF snapshot (`formatFusedStateJSON`).
- `GET /api/calibration` → `{"mag":{...},"balance_point":{...},"mounting":{...}}`.
- `POST /api/calibration/start` — body `{"mode":"mag"|"balance_point"|"mounting"}` → `{"ok":true,"session_id":"..."}`.
- `POST /api/calibration/capture` → `{"ok":true,"samples":N,"recommend_more":[...]}`.
- `POST /api/calibration/save` → `{"ok":true,"crc":"..."}`. `…/cancel` → `{"ok":true}`.
- `POST /api/pid_tune/{start,cancel}`, `GET /api/pid_tune/status` — see WS `pid.curve`.
- `GET /api/system` → `{"build":..,"heap":..,"uptime_ms":..,"rssi":..,"ssid":..}`.
- `POST /api/reboot` (no reply). `WS /ws` — tagged stream above.

ArduinoJson `JsonDocument` + `AsyncJsonResponse` (FC `web_server.cpp:147–158`) covers all of these unchanged.

## 7. Mobile-Friendly Considerations

The UX is: user holds the robot in one hand, taps the phone in the other.

- **Portrait primary**, single-column. 3D view ~80 % of viewport width, capture button below.
- **48×48 px touch targets** (WCAG 2.5.5). Full-width Capture / Save / Cancel.
- **No hover-only affordances** — color + outline for focus.
- **Disable OrbitControls `enableZoom`** on touch so page-scroll wins.
- **Big readouts** — RPY and cal scores at 1.5+ rem.
- **<100 ms tap feedback** — fire WS `{"t":"cmd","action":"capture"}` optimistically, reconcile on `cal.progress`.
- `<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">`.

## 8. Offline Mode

Once assets load, a transient WS drop must not blank the page:

- **Service Worker** cache-first for `/vendor/*`, `/*.js`, `/*.css`, `/*.html` (~40 lines vanilla; workbox is overkill). Serve `/sw.js` with `Cache-Control: max-age=0`.
- **WS reconnect** exponential backoff (1 → 2 → 4 s, cap 30 s). Toast "reconnecting…"; do not unmount the scene.
- **`sessionStorage`** keeps last quaternion / cal progress so reload-during-dropout renders.

## 9. Security Punt (Future Threat Model)

V1: **no auth**, trusted-LAN — same posture as the FC. Future hardening:

- Anyone on the WiFi can `POST /api/commands` and override channels (disarm or drive the bot). Add `Authorization: Bearer <token>`; token in `wifi_credentials.h`.
- WS upgrade has no Origin check — add an `Origin:` allowlist.
- OTA is unauthenticated — set `ArduinoOTA.setPassword(...)` before fielding.
- No HTTPS — mbedTLS costs ~30 KB heap; defer.
- **Never expose to the public Internet** — STA only, no port-forwarding.

## 10. Out of Scope for v1

- Multi-bot fleet view, swarm coordination (`swarm_api/`'s domain).
- Historical logging UI / time-series DB.
- User-customizable dashboards, draggable widgets, theming controls.
- Accounts, RBAC, localization. Native mobile app (PWA is the ceiling).
- Replay / scrubber UI.

---

**References**:

- `flight_controller/src/web_server.cpp` — reference ESPAsyncWebServer + WS + ArduinoJson pattern.
- `auto_orientation/src/output/json_formatter.h` — quaternion JSON shape.
- `auto_orientation/docs/findings/wifi_telemetry_integration_design.md` — endpoint surface, dual-core split.
- [Three.js docs](https://threejs.org/docs/), [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer), [ESP32-Arduino LittleFS](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/littlefs.html), [PlatformIO LittleFS upload](https://docs.platformio.org/en/latest/platforms/espressif32.html#uploading-files-to-file-system).
