# Swarm Communication Protocol — Seed Document

Seed content for the future `swarm_communication_protocol` repo in the
`lowprofiledronegurus` GitLab group. Extracted from a long stream-of-consciousness
brainstorm captured in `floppi/tmp.md`. Extract-not-invent: nothing here is a new
design decision — every statement traces back to the source note or is flagged
as an open question that was already raised there.

---

## 1. Origin Note

This document distills the drone-swarm communication protocol brainstorm from
`/home/devel/floppi/tmp.md`. The source is a single stream-of-consciousness
dictation by the project lead, working with a research associate who has been
prototyping a ~50-60 hex-character message format for inter-drone
communication. The goal of this seed doc is to preserve every idea the source
raised — including the ambiguities and open questions — so the future
`swarm_communication_protocol` repo has a faithful starting point that the
research associate can iterate on. No new design decisions are introduced.

---

## 2. Problem Statement

- The core problem is **communication saturation** in a drone swarm: too many
  nodes attempting to move too much information over shared radio.
- The research associate is developing a **hex-string language**: strings of
  roughly 50-60 hex characters conveying commands, alarms, and diagnostics.
- The swarm is structured as a **binary tree**. There is a distinguished
  **hub / mothership / parent drone** and normal **child nodes**. Terminology is
  used interchangeably in the source: hub = mothership = root parent.
- A binary tree is chosen so that neither the hub nor any individual node is
  flooded: work is delegated **branch by branch**; a parent only needs to talk
  to its own direct children and back-verify with them, not with the whole
  subtree.
- **Explicit non-goal:** this critical-communication channel is NOT for raw
  sensor streams (LiDAR point clouds, camera images, full sensor logs). Raw
  telemetry is out of scope for the hex-string language.
- Two solutions to network saturation are named in the source: (a) raise
  per-node throughput, or (b) **segment the network** better (compared to
  Palo Alto Networks NGFW rule design). The binary-tree topology is the
  segmentation lever.
- **CIA triad** is the target property set. **Availability is #1 priority** for
  the swarm; confidentiality (encryption) is deferred; integrity is addressed
  today with per-hop send/acknowledge.

---

## 3. Communication Layers

Three priority tiers of communication are called out in the source. The
hex-string language is primarily for L1 (and possibly L2); L3 is deprioritized
and may not even ride this channel.

### L1 — Flight-controller-level movement and alarms
- Analogous to an RC transmitter feeding the flight controller.
- Movement commands with degrees of freedom (up/down/left/right/forward/backward).
- Waypoint commands (lat/lon/alt) issued to on-drone routines.
- Emergency stop / return-to-base.
- Alarms tied to flight-critical hardware: battery, ESC, motor, IMU (e.g.
  abnormal acceleration signalling a crash).

### L2 — Environment / survival sensors and peer coordination
- Sensors and instrumentation needed to keep the drone alive in a specific
  environment: UWB radios for relative peer position, ultrasonic / LiDAR for
  obstacle avoidance in caves or forests, ambient temperature to detect
  freezing conditions, etc.
- Peer-to-peer coordination for **flocking algorithms** (Boids-style) —
  drones knowing their nearest-neighbor positions relative to each other,
  independent of GPS.
- Peer coordination for **GPS-denial recovery** (multilateration): a lost
  drone reaches out to nearby peers with known positions.
- Mission-parameter tweaks that affect survival (e.g. "increase peer standoff
  from 3 ft to 5-7 ft while flying through forest").

### L3 — Raw / mission-data sensor communication (DEPRIORITIZED)
- Camera images, video snippets, raw sensor arrays, floats from a dew-point
  sensor, etc.
- Source is explicitly unsure whether L3 belongs on this channel at all.
- If used, data is likely **conditioned on-drone first** (compression, metadata
  stripping) so what leaves the drone is minimal; full-fidelity data stays on
  onboard SSD/SD until the drone lands.

---

## 4. Message Anatomy (Load-Bearing)

The message frame is described concretely in the source and is the most
load-bearing extraction in this document.

- **Total length:** ~50-60 hex characters per message.
- **Field order** (as described in the source):
  1. **Expiration timestamp** — first field. If the message is received
     *after* this time, the receiver disregards it (already stale). Serves as
     the sender-specified time-to-live.
  2. **Intended-receiver ID** — next field. Lets non-target nodes filter out
     early and stop processing.
  3. **Send timestamp** — when the sender emitted the message.
  4. **Sender ID.**
  5. **Message type** — command vs alarm vs diagnostic vs waypoint-list-chunk,
     etc.
  6. **Payload** — command-specific bytes.
- Header ordering is chosen so a listening node can bail out on
  expiration/receiver-mismatch **before** parsing the full payload — since
  radio propagation means every drone hears every message anyway.

### Field ambiguities flagged in the source
- **Alphabet:** hex-only (0-9, a-f, 16 symbols) vs alphanumeric (0-9 + A-Z,
  36 symbols). Two hex characters yield 256 combinations; two alphanumeric
  characters yield ~1000-2000 command combinations. Source explicitly says
  "I'll have to ask my research associate" — undecided.
- **Command field bit width:** not fixed. Source imagines 2 hex characters or
  more depending on how many commands are needed.
- **Timestamp encoding:** unspecified (epoch seconds? milliseconds? relative
  ticks?).
- **ID width:** unspecified for both sender and receiver.

---

## 5. Binary-Tree Topology

- **Root:** hub / mothership / parent drone. Higher-powered than children;
  the only node expected to communicate with the ground station in some
  mission profiles.
- **Delegation:** parent talks only to its assigned children; each child
  then delegates to its own subchildren. No node needs to reach every other
  node directly.
- **Back-propagation for verification** is **per-hop, not full-subtree**:
  the parent only needs its direct children to acknowledge receipt; those
  children relay the message downward, and their subchildren acknowledge
  them. The hub is never flooded with acks from every leaf.
- **Two IDs per drone:**
  - **Hard hardware ID** — immutable, unique per physical drone.
  - **Tree ID** — mutable position in the binary tree. Can be shuffled
    mid-mission.
- **Dynamic tree reshuffle:** the tree can be rearranged when:
  - A drone becomes unusable (flight-controller fault, instrument fault) and
    its slot must be filled or bypassed.
  - Radio propagation across terrain (cave systems, forests) makes the
    logical tree diverge from physical reachability — the tree can be
    reshuffled to match which nodes can actually reach each other.
- The tree-shuffle algorithm itself is an open item; the source says "we
  still have to figure out those algorithms."

---

## 6. Directionality Distinctions

The source is explicit that the *direction* of a message is a first-class
distinction — it changes semantics, priority, and possibly routing:

- **Parent → Child.** Commands, waypoints, mission delegation, emergency
  stop, tree-ID reassignment. Lower total volume than child-to-child.
- **Child → Parent.** Diagnostics, alarms, verification/ack of received
  commands, recovery requests (e.g. "I've lost GPS, need multilateration
  help"). Escalates up the tree only as far as needed.
- **Child ↔ Peer.** Peer-to-peer coordination for flocking, multilateration,
  local obstacle avoidance, distress relay when a drone can't reach its
  parent. Source expects this to be the **highest-volume traffic class**
  overall — significantly more than hub-to-swarm broadcasts.

The source flags that peer-to-peer traffic may not cleanly fit the binary
tree — physical proximity, not tree position, drives who talks to whom for
things like GPS-denial recovery — so this is an open architectural
question (see §11).

---

## 7. Command Taxonomy

The following command families are called out in the source. This is not a
complete grammar; the source explicitly renounces building a full grammar.

- **Degrees-of-freedom movement:** up, down, left, right, forward, backward
  (and rotations, implied by "various degrees of freedom").
- **Single-waypoint command:** lat + lon + alt. On-drone routine handles
  the actual flight to that point.
- **Multi-waypoint / mission-list command.** The source discusses missions
  of up to ~200 waypoints and recognizes they cannot fit in a single 50-60
  hex message. Proposed pattern:
  - A **list-header** message announcing the incoming list.
  - Subsequent **chunk messages** carrying individual waypoints.
  - Each chunk tagged with a **unique list ID** (not a timestamp) so the
    receiver knows the chunks belong to the same in-progress list and does
    not prematurely close the array.
  - Terminator / final-chunk signalling: implied but not specified.
- **On-drone routine invocation.** Rather than sending raw motor deltas,
  the hub sends a routine name (e.g. "go-to-GPS(x,y,z)") and the drone's
  onboard code handles execution.
- **Emergency stop / return-to-base.** Behaves differently from a single
  "big red button": the hub receives it, then must wait for the whole
  binary tree to acknowledge and reel in — daisy-chained drones deep in a
  cave return first, then flock outward. Not a global instant halt.
- **Peer coordination request.** Drone A asks nearby drones for help,
  e.g. multilateration for GPS recovery, or "help me sense obstacles."
- **Mission-parameter update.** In-flight adjustment of survival
  parameters (e.g. peer standoff distance).

---

## 8. Alarm Taxonomy

Alarms are classified along two orthogonal axes in the source.

### Axis A — Severity / recoverability
1. **Recoverable by the drone itself.** Local fault, self-clear.
2. **Recoverable with peer help.** E.g. GPS-denied drone recovered via
   multilateration from four or more peers with valid positions.
3. **Not recoverable, mission not critically affected.** Drone continues
   the mission at reduced capacity (e.g. one sensor down but redundancy
   exists).
4. **Not recoverable, drone critically affected → drone RTB.** E.g. a
   thousand-dollar camera at risk; drone returns to base, mission
   continues without it. Drone may instead tag along with the hub if
   the ground station is out of range for a low-power child drone.
5. **Not recoverable, mission critically affected → mission cancel.**
   Everything returns to ground station.

### Axis B — Source subsystem
- **Flight-controller layer (L1).** Battery, ESC, motor, IMU (e.g.
  abnormal-acceleration crash detection).
- **Instrumentation layer (L2).** Payload sensors: LiDAR, UWB,
  ultrasonic, environmental sensors, temperature.
- **Flight-computer / system diagnostics.** Logic errors, thread
  starvation, storage issues on the flight computer itself.
- **Cyber-defense / anomaly detection.** GPS spoofing / denial
  detection, jamming detection, intrusion detection. Source flags this
  gets complex fast and does not want to over-scope malicious-vs-normal
  discrimination in v1 — just detect the anomaly and route to recovery.

Any alarm message ideally identifies both axes plus the specific sensor
position (see §9).

---

## 9. Sensor Identification Convention

The source rejects the idea of assigning a globally unique ID to every
sensor SKU on Digi-Key. Instead:

- Every drone has a **fixed-size sensor array** with **positional
  slots** (source's example: positions 1 through 10).
- Before the mission, each drone's slots are **registered**: slot N
  holds sensor of type T with expected data type D.
- The registry is known to every drone including the hub, so at runtime
  alarms only need to say "position 3, condition X" — the receiver
  looks up what's actually in slot 3 for that drone's hardware ID.
- Sensors of the same type may occupy different slots on different
  drones — this is fine because the slot map is per-drone.
- **On-drone data conditioning.** A camera sensor may natively output
  images plus EXIF plus timestamps plus other metadata; the drone can
  strip that down to just the image before it enters any communication
  layer. Conditioning happens per-slot, per-drone.
- The mechanism for physically hooking sensors up is not standardized
  ("different sensors hook up in different ways") — this is not a
  physical bus spec, it is a **logical registry**.

---

## 10. CIA Triad Concerns

- **Confidentiality — deferred, not precluded.** Encryption is
  explicitly out of scope for v1. The concern raised is that encryption
  will *increase* saturation, so the language must be designed to
  accommodate encryption being added later without a redesign.
- **Integrity — per-hop verification.** Send/ack per hop, not per full
  path. Modeled loosely on TCP-style acking, gaming-network models, and
  chat-system reliability (Discord is named as a high-volume, high-
  criticality analog, with the caveat that Discord can afford passive
  operation and drones cannot).
- **Availability — #1 priority.** The whole binary-tree segmentation,
  the ~50-60 hex message budget, the timestamp-first frame ordering,
  and the deferral of encryption are all in service of keeping the
  channel available under saturation.
- Packet loss is expected due to radio-wave propagation through varied
  terrain — this is the practical reason per-hop acks exist.

---

## 11. Open Design Questions

Every uncertainty explicitly raised in the source is captured here. These
are the questions the future repo will need to resolve.

1. **Alphabet.** Hex-only vs alphanumeric. Affects command-space width
   per character. (See §4.)
2. **Command field bit width.** How many characters allocated to the
   command / message-type field, and how many command families does that
   cover.
3. **Waypoint-list serialization.** List-header format; chunk framing;
   unique list-ID width; terminator / completion signalling.
4. **Peer coordination routing.** Does peer-to-peer traffic use the
   binary tree, or is it a physical-neighborhood side-channel? Both are
   described; the boundary is not resolved.
5. **Encryption vs saturation.** How to add encryption later without
   forcing a re-spec of the message frame. Reserve fields now?
6. **DWS1000 dual-use feasibility.** The available UWB module is a
   Qorvo DWS1000. Can it do (a) time-of-flight ranging **and** (b) text-
   style messaging for L1/L2/L3 traffic on the same hardware? Concerns:
   wear-and-tear from constant ranging, whether mode-switching is fast
   enough, whether multiple DWS1000s per drone are required.
7. **Layer boundary between L1 / L2 / L3.** The source repeatedly
   admits the boundaries are fuzzy — e.g. UWB peer ranging feels like
   L2 but touches L1 flight decisions; mission-parameter updates could
   be L1 or L2 depending on framing.
8. **ML integration.** Machine-learning models on-drone for sensor
   denoising and data conditioning are anticipated but explicitly not
   designed here. Where does conditioned-by-ML data enter the layer
   model? Is a decision-making ML model on-drone in scope at all?
9. **Timestamp encoding and clock sync.** Two timestamps per message
   (expiration + send-time) are specified, but the encoding is not.
   Clock synchronization across the swarm is unaddressed.
10. **Full command / alarm code registry.** No enumeration exists yet.
    Naming convention for message-type IDs is undefined.
11. **Tree-shuffle algorithm.** Trigger conditions (hardware failure,
    physical unreachability) are known; the algorithm is not.
12. **Flight-computer platform.** Raspberry Pi has been considered but
    the source raises concerns (OS overhead, SD-card fragility, power).
    The FC hardware (Teensy / ESP32) is settled; the *flight computer*
    running comms is not. Affects what the language can assume about
    per-node compute.
13. **Broadcast / list-completion semantics.** How does a receiver know
    a multi-chunk list is complete vs still incoming beyond just
    matching list-ID.
14. **Ground-station relay via hub for range-limited children.** Low-
    power children can't reach the ground station directly; the hub
    relays. What does this look like in the message frame — is the
    ground station just another node with an ID, or is it a
    distinguished endpoint?

---

## 12. Explicit Non-Goals (v1)

- **Raw sensor streaming** over the hex-string channel (camera video,
  LiDAR point clouds, full sensor arrays).
- **A complete grammar / syntax** for the language. The source explicitly
  says: "we're not trying to construct complete sentences or create an
  entire grammar syntax." The goal is minimalism, not linguistic
  completeness.
- **Encryption in v1.** Confidentiality is deferred; the language must
  merely leave room for it.
- **Full-network message propagation.** The hub does not broadcast to
  every node directly; the tree delegates. Acks similarly do not
  propagate all the way back.
- **Discriminating malicious vs benign anomalies** at the alarm layer.
  Detect and route to recovery; don't try to classify intent.
- **A universal physical sensor bus.** The sensor-identification
  convention is a logical positional registry, not a hardware bus.
- **Global instant emergency stop.** RTB is a coordinated reel-in, not
  a synchronized halt.

---

## 13. Cross-Project Touchpoints

Other repos in the `lowprofiledronegurus` group that this protocol
will interact with. Each connection is drawn from the source note.

- **`flight_controller`** — Teensy/ESP32 firmware. L1 commands land
  here. Command decoding runs alongside the FC's existing PWM/ESC
  logic. Alarms about IMU / battery / ESC / motor originate here.
- **`communication_hardware`** — the DWS1000 UWB module and any
  future RF modules live here. The dual-use ranging + text-messaging
  question (Q6 above) is a joint concern with this repo. Wear-and-tear
  testing of DWS1000 belongs there; protocol design belongs here.
- **`position_denial_research`** — multilateration and GPS-denial
  recovery. The peer-coordination command family (§7) and the
  "recoverable-with-peers" alarm class (§8) are the joint surface.
- **`networking_pocs`** — commercial-WiFi / cellular / DNS-driveby
  experiments live there. If a swarm falls back to commercial network
  transports (e.g. hub uplink over cellular to a ground operator),
  the message frame here needs to survive that transport unchanged.
- **`sensor_interactions`** — non-radio sensor POCs. The sensor
  registry convention (§9) is defined here but exercised there.
- **`fc_tool`** — host-side FC interaction tool. Likely target for
  a first parse/emit reference implementation of the language (see
  §14) so a human can hand-craft messages against real hardware.
- **`research`** — literature relevant to swarm networking, TCP-like
  reliability, chat-system reliability, and encryption-under-
  saturation goes here and is cited from this repo.
- **`darpa_lift_2026`** — the mission context sets throughput budgets
  and worst-case swarm size, both of which constrain the protocol.

---

## 14. Next Steps (When the Repo Materializes)

Concrete first tasks for the `swarm_communication_protocol` repo once
it exists on GitLab. These are seeded from the brainstorm — nothing
here commits to a design; each is a starting artifact.

1. **Message frame diagram.** A single canonical figure showing the
   ~50-60 hex character layout with field boundaries, drawn from §4.
   Should be diff-able (Mermaid / ASCII / SVG source), not a binary.
2. **Reference parse/emit implementation in Python.** A minimal library
   that emits and parses the frame from §4, including the ambiguities
   (parameterize alphabet, command width, timestamp encoding). Runs on
   host, no drone hardware required.
3. **Simulation harness.** A software-only harness that instantiates a
   binary tree of virtual nodes, injects saturation load, runs
   send/ack per hop, and lets us measure end-to-end command latency
   and ack-back time under packet loss. Precursor to any real hardware
   test.
4. **Message-type ID naming convention.** A short spec for how command,
   alarm, and diagnostic type IDs are minted, extended, and reserved.
   Deferrable but should exist before the type registry grows.
5. **Coordinate with the research associate.** The frame ordering in
   §4 and the alphabet/command-width questions in §11 need to be
   reconciled with the research associate's existing prototype
   before anything else is built on top.
6. **Alarm code registry (stub).** A table with columns
   `(source-layer, severity-class, sensor-slot, code, meaning)` seeded
   from §8 and §9. Empty is fine; establishing the schema is the point.
7. **Waypoint-list framing prototype.** Encode a fake 200-waypoint
   mission using the list-header + chunk + unique-list-ID pattern from
   §7 and run it through the parse/emit library from (2). Validates
   the pattern before it hits radio.
8. **Cross-repo doc-link table.** For each cross-project touchpoint in
   §13, add a link to the specific document / file in that repo that
   this protocol depends on or informs. Keeps drift visible.

---

*End of seed document. Source: `/home/devel/floppi/tmp.md` (single-file
brainstorm, ~18k words). This document is extraction-only; every design
decision remains the research associate's to make or ratify.*
