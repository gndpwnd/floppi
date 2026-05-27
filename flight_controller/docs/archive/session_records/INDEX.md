# Session Records — Index

Chronological archive of flight_controller working-session records. Newest first. Each record captures the headline outcomes, decisions, build status, and open items for one session.

| Date | Record | Summary |
|---|---|---|
| 2026-05-26 | [2026-05-26_calibration_storage_port.md](./2026-05-26_calibration_storage_port.md) | Wave 6 — vendored `calibration_storage` HAL from `auto_orientation/` into `flight_controller/lib/CalibrationStorage/` (with 2026-05-20 P1 fixes: CRC-8-CCITT, capacity-bounded loads, version refusal); MPU6050 offsets now restore-on-boot and persist after each of the 4 cal types. ESP32 NVS-awareness handles cross-project KI-1 footgun. Scale factors still compile-time (follow-up). Builds: esp32 +8744 B (one-shot NVS lib pull-in), teensy40 clean, esp32_calibration clean. Uncommitted. |
| 2026-05-22 | [2026-05-22_security_correctness_docs.md](./2026-05-22_security_correctness_docs.md) | Security/correctness hardening + documentation wave: opt-in/default-OFF command-API token (`USE_API_AUTH`), OTA password + build guards, dynamic WS telemetry buffer, I2C XOR checksum, Madgwick6DOF NaN guard, GPS position-privacy gate. Audit (3 P0 / 2 P1) → fixes → QA GO. ASCII→Mermaid + new `docs/architecture/` Level 0/1/2 doc set. Uncommitted. |
| 2026-05-22 | [2026-05-22_wifi_network_modes.md](./2026-05-22_wifi_network_modes.md) | ESP32 WiFi compile-time auth-mode selector (`WIFI_AUTH_MODE_*` — PSK default / OPEN / WPA3-SAE / Enterprise PEAP/TTLS/TLS), `USE_WIFI_CERTS` / `USE_STATIC_IP` / `WIFI_HOSTNAME`, `#error` validation, liftable `wifi_connect` module. PSK byte-identical to legacy; Enterprise ~0 incremental flash. QA GO; hostname-ordering bug fixed. Uncommitted. |
| 2026-05-21 | [2026-05-21_multi_agent_sensors_w6_native_tests.md](./2026-05-21_multi_agent_sensors_w6_native_tests.md) | Barometer/GPS/swarm-telemetry workstreams closed out: W6 baro+GPS telemetry blocks in the swarm POST, W4 `'b'` barometer calibration, BMP388/MS5611 drivers, native host-side test harness (5/5 green), build coverage matrix, Teensy-parity recon (no work needed). Uncommitted. |
| 2026-05-20 | [2026-05-20_recon_builds_and_scaffolding.md](./2026-05-20_recon_builds_and_scaffolding.md) | Project recon (779-line map); SBUS re-enable takes builds 7/10 → 10/10; test harness modularized; BNO055/BNO085 Phase A scaffolding (flags OFF); PID guide + wiring audit + WiFi/diagnose docs. |

---

*Add new records at the top of the table. Filename convention: `YYYY-MM-DD[_AM|_PM]_short_slug.md`.*
