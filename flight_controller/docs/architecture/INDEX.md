# Architecture — Index

Layered architecture documentation for the flight controller firmware. Start
at **Level 0** for the whole-system picture, then drill down only into the
subsystems you care about. Every diagram is Mermaid and is grounded in the
actual source (each doc ends with a "Source anchors" section pointing at the
real files).

## How the layers work

- **Level 0** — one diagram of the entire firmware. ~15 nodes. Read this first.
- **Level 1** — one diagram per major subsystem. Read the one you need.
- **Level 2** — component-level detail for the two trickiest pieces only
  (cross-core/concurrency mechanisms). Read these only when you are touching
  that code.

## Level 0 — System overview

| Doc | What it covers |
|---|---|
| [0_system_overview.md](0_system_overview.md) | The whole firmware: command sources → RadioComm → flight loop → motors, plus the Core-0/Core-1 split, telemetry, and the swarm_api link. |

## Level 1 — Per-subsystem

| Doc | What it covers |
|---|---|
| [1a_flight_loop_and_pid.md](1a_flight_loop_and_pid.md) | The flight loop iteration order and the PID base / optimization / racing feature tiers. |
| [1b_command_sources_and_arbitration.md](1b_command_sources_and_arbitration.md) | The 5 RC protocols + 3 external command sources, and the Serial > I2C > WiFi > RC arbitration. |
| [1c_esp32_dual_core.md](1c_esp32_dual_core.md) | The ESP32 Core-0 (flight) / Core-1 (WiFi, web, API, OTA, sensors) split and the cross-core handoffs. |
| [1d_sensor_telemetry_pipeline.md](1d_sensor_telemetry_pipeline.md) | IMU + barometer (BMP280/388/MS5611) + GPS passthrough → web server → swarm_api, and the telemetry-only scope boundary. |

## Level 2 — Component detail

| Doc | What it covers |
|---|---|
| [2a_arbitration_buffers.md](2a_arbitration_buffers.md) | The `CommandBuffer` struct, the 500 ms `OVERRIDE_TIMEOUT_MS` liveness state machine, and how a frame becomes motor output. |
| [2b_baro_core1_task.md](2b_baro_core1_task.md) | The barometer Core-1 FreeRTOS task and its spinlock-guarded snapshot — the template for any optional Core-1 sensor. |

## See also

- [docs/scope.md](../scope.md) — what the project IS and IS NOT (the scope
  boundary the sensor pipeline doc leans on).
- [docs/findings/command-arbitration-design.md](../findings/command-arbitration-design.md)
  — the original arbitration design rationale.
- [docs/findings/esp32-dual-core-research.md](../findings/esp32-dual-core-research.md)
  — the dual-core split rationale.
- [docs/findings/swarm_api_contract_2026-05-20.md](../findings/swarm_api_contract_2026-05-20.md)
  — the WiFi/HTTP/WebSocket wire contract.
- [docs/features/wifi-configuration.md](../features/wifi-configuration.md) — the ESP32
  compile-time WiFi auth-mode selector (`WIFI_AUTH_MODE_*`), the link-layer side of the
  Core-1 networking covered in `1c_esp32_dual_core.md` / `1d_sensor_telemetry_pipeline.md`.
  Design rationale: [docs/plans/wifi-network-modes-plan.md](../plans/wifi-network-modes-plan.md);
  QA: [docs/findings/wifi_modes_qa_2026-05-22.md](../findings/wifi_modes_qa_2026-05-22.md).
- [docs/security_posture.md](../security_posture.md) — trust-boundary + threat model for the
  command/telemetry/OTA surfaces (the network-edge view of the same subsystems).
