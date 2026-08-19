# 12 — WayfindR-driver reference inventory (springboard, not migration)

**Purpose.** The operator's ask, verbatim: *"for now we just need lowprofiledronegurus to have
documentation that discusses what all they can reference from wayfindr to help speed up their initial
startup."*

**Status of WayfindR-driver (operator ruling, 2026-08-18):** *"ideally the wayfindr driver and stuff will
be deleted in the future… not be outright deleted since it is a current project, but used to springboard
the repos and the different research they are trying to do. we will be doing alot of lidar stuff and the
fsia6b code will come in handy."*

So WayfindR is a **legacy/springboard** project, alongside GravityProbe and SwarmLoc — its findings and
research get referenced or migrated forward so they are not lost, and it then takes a back seat.

**Project classification now in force:**

| Class | Projects | Treatment |
|---|---|---|
| **ACTIVE** | `~/floppi`, `~/lowprofiledronegurus`, `~/engineer360`, `~/hiverf` | Coordinate as peers; live scopes |
| **LEGACY / SPRINGBOARD** | `~/WayfindR-driver` (+ `WayfindR-android`), `~/GravityProbe`, `~/SwarmLoc` | **Directional.** Writing INTO them is forbidden (no delete, no modify, no commits, no `.gitignore` edits). Taking OUT of them is **allowed** — operator: *"their findings and useful items can just be referenced or migrated into lowprofiledronegurus."* Extraction is selective and attributed, never a wholesale `cp -r`. |

**Confidence note (read this before relying on the table).** This inventory was built from directory
structure, each subproject's own `scope.md`/`README.md`, and targeted content greps — **not** a full read
of every file. Sizes and paths are verified; characterizations of what the code *does* come from the
project's own documentation. Anything marked ⚠ needs a read before it is relied on.

Repo: `git@github.com:msmcs-robotics/WayfindR-driver.git`, branch `main` (only branch), clean tree,
63 MB. Last commits are `save progress`, with `Session 22: Jetson-first pivot, MCP ability server, RAG
restructuring` as the last substantive one.

---

## 1. LiDAR + SLAM — the highest-value asset

The operator flagged lidar explicitly. This is the deepest body of work in the repo, and it maps onto
`sensor_interactions`, which is now the **successor** to this line of work.

### 1.1 Start here — the bring-up ladder
`old_stuff/rplidar_setup/` — four numbered bare-Python scripts that walk first contact with an RPLidar:
`0.1-list_ports.py` → `0.2-RPLidar_info.py` → `0.3-RPLIDAR_meas.py` → `0.4-RPLIDAR_plot.py`.
**Why it matters:** this is the "is the sensor alive, what is it saying, does it look sane" ladder before
any ROS2 stack is involved. Reusable as a pattern for *any* new sensor, not just lidar.

### 1.2 The workflow document — read this first if you do nothing else
`findings/lidar-data-workflow.md` — "LiDAR Data Recording and Replay Workflow for SLAM Testing." Covers
connection and setup through record → replay → map creation → algorithm tuning. Target system stated as
**ROS2 Humble + RP LIDAR C1M1 + 2D SLAM (Cartographer / SLAM Toolbox)**.
**Why it matters:** record-once/replay-many is what makes SLAM tuning tractable without re-flying or
re-driving. That single practice is probably the biggest time-saver in the whole repo.

### 1.3 Rosbag / SLAM test evidence
`findings/2026-01-11-rosbag-slam-test-results.md`, `README-rosbag-testing.md`,
`rosbag-testing-quick-commands.txt`, `rosbag-test-flow-diagram.txt`,
`rosbag-test-complete-file-list.txt`, `rosbag-testing-summary.txt`,
`findings/2026-01-11-diagnostic-tools-test.md`.
Dated test records with actual outcomes — the honest kind, not aspirational.

### 1.4 Map tooling cluster (7 documents)
`findings/map-server-test-scripts.sh`, `map-server-quick-reference.md`,
`findings/2026-01-11-map-server-test.md`, `map-editing-guide.md`, `map-tools-guide.md`,
`map-tools-deliverables.md`, `map-tools-summary.md`, `map-validation-tools-summary.md`.
Map serving, editing, and **validation** — the last one matters most; a map you cannot validate is a map
you cannot trust.

### 1.5 The ROS2 implementations — and which one to copy
Four `ros2_*_attempt/` folders plus one production implementation. **`ambot-slam/` supersedes the
attempts** — its own scope says so: *"takes the proven hardware and knowledge from the `ambot/` project
(LiDAR, IMU, motors, camera) and integrates them into a proper ROS2 navigation stack with SLAM Toolbox,
Nav2, and AMCL localization. This is the 'production' implementation that replaces the research
experiments in the various `ros2_*_attempt/` folders."*

| Path | Size | Use it for |
|---|---|---|
| `ambot-slam/` | 68 K | **The canonical one.** SLAM Toolbox + Nav2 + AMCL, with `config/`, `launch/`, `src/`, `docs/{scope,roadmap,todo,README}.md` |
| `ros2_install_attempt/` | 232 K | Numbered install path `01_install_ros2_humble.sh` → `07_complete_workflow.md`, plus `ACTUAL_INSTALLATION_LOG.md` — a real log, not the idealized one |
| `ros2_cartography_attempt/` | 300 K | `rplidar_slam.launch.py`, `navigate_waypoints.py`, `maps/` |
| `ros2_localization_attempt/` | 192 K | AMCL/localization-specific config + findings |
| `ros2_comprehensive_attempt/` | 2.1 M | Largest; `DIAGNOSTICS_INDEX.md`, `GAZEBO_QUICKREF.txt`, `QUICK_REFERENCE.txt` — simulation before hardware |

**Read the attempts for the failure record, copy from `ambot-slam/`.** The attempt folders are the
"why we ended up here" and are worth mining for lessons; they are not the thing to fork.

---

## 2. FlySky FS-iA6B receiver — operator-flagged as directly useful

`floppi/flight_controller` already implements SBUS / iBUS / DSM / PPM / PWM. WayfindR is therefore a
**second, independent implementation to cross-check against** — different vehicle class, same receiver.

| Path | What it is |
|---|---|
| `demos/fsia6b_basic_PWM/` | Basic FS-i6B receiver test sketch — the minimal "are channels arriving" check |
| `demos/fsia6b_UNO_skidsteer/` | Differential-drive mixing from receiver channels |
| `demos/FlySky_IA6B_pinout.jpg` | Pinout photo — trivial to lose, annoying to re-derive |
| `esp32_api/src/mixer.cpp` | Multi-vehicle channel mixing, comment at line 66: *"Based on fsia6b_UNO_skidsteer differential drive logic."* Handles car / boat / plane / quad mixes |

**Cross-check value:** if `flight_controller`'s iBUS parsing and WayfindR's ever disagree about channel
mapping or failsafe behaviour, one of them is wrong — and having two implementations is how you find out
cheaply.

---

## 3. ESP32 dual-core control architecture — direct overlap with two active repos

`esp32_api/` (224 K) — from its own scope: a **dual-core** ESP32 firmware that *"separate[s] real-time
control tasks from network operations"*, with REST API + WebSocket + web dashboard, multi-vehicle mixing,
a flight-controller integration framework, and an "LLM-friendly API with natural language command
support."

**Why this matters more than its size suggests.** It overlaps two active targets at once:
- `flight_controller` — which has its own ESP32 WiFi/telemetry/dashboard/OTA surface, and whose ESP32
  flight loop is *scaffolded and bench-untested*. WayfindR's core-separation approach is a directly
  relevant prior art for keeping a real-time loop clean while WiFi runs.
- `swarm_api` — REST/WebSocket command surface for a vehicle over WiFi is exactly swarm_api's problem.

Contents: `src/{main,control_loop,controller_interface,mixer,output_channels,web_server}.cpp`, plus
`include/`, `examples/`, `findings/`, `docs/`, `scope.md`, `platformio.ini`.

---

## 4. Raspberry Pi, power, and fleet — relevant to swarm work

| Path | Value |
|---|---|
| `pi-fleet-manager/` (224 K) | **Multi-node fleet management.** Directly relevant to `swarm_api` and the swarm/flock goal — managing N vehicles is the same problem regardless of whether they fly |
| `PI_API/` (244 K) | FastAPI service on the Pi, with `scope.md` and `models/robot_state.py` — compare against `swarm_api`'s FastAPI ground station |
| `docs/power-system-guide.md` | Power system design |
| `docs/raspberry_pi_battery_power_guide.md` | Battery power for Pi-class compute |
| `docs/raspberry_pi_gpio_power_comprehensive_guide.md` | GPIO power — the classic brownout source |
| `demos/pi_GPIO_diff_drive/` | Pi GPIO differential drive |

Power documentation pairs naturally with floppi's own `docs/BEC_Wiring.md` and
`docs/POWER_MODULE_WIRING.md`, both routed to `flight_controller/hardware/power/`.

---

## 5. Onboard compute / LLM benchmarks

`docs/jetson_orin_nano_llm_benchmarks.md` and `docs/ollama_hpc_multi_hop_deepseek.md`, plus the
`Session 22: Jetson-first pivot` commit. If any drone build ever carries onboard inference, these are
measured numbers on real hardware rather than vendor claims. Route to `research/`.

---

## 6. Also present, lower priority

`adeept_car_stuff/` (6.1 M) and `ambot/` (6.0 M) — vendor/platform robot kits; `sheep_helper/` (1.2 M);
`ros_tank_xiaor/`; `new_bakery/` + `old_bakery/`; `system_scripts_humble_ubu22.04/`; `pi_scripts/`;
`demos/voice_AI_pi_led/`; `docs/Indoor Mapping and Navigation System Overview.{md,pdf}`;
`docs/connections.md`, `folder-guide.md`, `overview.md`, `roadmap.md`, `scope.md`.
⚠ Not characterized in depth.

---

## 7. Where each cluster lands

| WayfindR cluster | Target | Form |
|---|---|---|
| RPLidar bring-up ladder (§1.1) | `sensor_interactions/lidar/bringup/` | **Migrate** — small, self-contained, immediately useful |
| LiDAR workflow + rosbag + map tooling (§1.2-1.4) | `sensor_interactions/lidar/` + `research/findings/` | **Migrate the documents** — this is the research the operator wants preserved |
| `ambot-slam/` + ROS2 attempts (§1.5) | `sensor_interactions/lidar/slam/` (canonical) + a lessons doc for the attempts | **Reference + selective migrate** |
| FS-iA6B demos + mixer (§2) | `flight_controller` (cross-check) + `sensor_interactions/rc_receiver/` | **Reference**, then migrate what survives comparison |
| `esp32_api/` (§3) | `flight_controller` + `swarm_api` | **Reference** — prior art, not a drop-in |
| `pi-fleet-manager/`, `PI_API/` (§4) | `swarm_api` | **Reference** |
| Power guides (§4) | `flight_controller/hardware/power/` | **Migrate** |
| Jetson/LLM benchmarks (§5) | `research/` | **Migrate** |

**`sensor_interactions` is the successor repo** for the lidar and sensor lines of work — operator ruling,
2026-08-18: *"sensor interactions is supposed to be the successor."* That resolves its thinness problem:
it was never going to be three floppi documents; it inherits WayfindR's lidar corpus and GravityProbe's
sensor bring-up findings.

---

## 8. Gates before anything moves

1. **Directional restriction on all three legacy repos.** Writing INTO them is forbidden — no deletion, no
   modification, no `.gitignore` edits, no cleanup commits, no branch work. Taking content OUT is
   **allowed and expected**: reference, document, and migrate the useful items with attribution. Not a
   wholesale tree copy — *"not everything from them is useful."*
2. **No git or repo CRUD by any session.** Operator-only, both forges.
3. **Content-based binary filter on any copy.** `auto_orientation` and `flight_controller` carry 20
   extension-less compiled ELF test binaries that no glob excludes (finding from the
   `lowprofiledronegurus` session). Same class of hazard applies to any ROS2 build output here.
4. **WiFi credential hygiene — PARKED by the operator** (*"don't worry about security with wifi right now
   that is not the priority"*). Recorded in `11_routing_v2` §4, de-gated. Worth a glance if WayfindR
   content is ever copied into a public repo, but it blocks nothing now.
