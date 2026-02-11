All committed. Here's a summary of what was done:

This Session
Completed
fc_tool CLI + headless mode — --port, --baud for auto-connect; --headless for raw serial to stdout with stdin forwarding. Built and tested with real Teensy hardware.

Modular WiFi configuration — Added support for:

WPA2-Personal (default)
Open networks
WPA2-Enterprise PEAP (with configurable auth method)
WPA2-Enterprise TLS with certificates
New wifi_certs.h template for PEM storage
Documented in wifi-configuration.md
ESP32 network diagnostics — n command in calibration mode tests WiFi credentials, connection, RSSI, MAC, gateway, DNS, mDNS, web server (all local, no external connections). Integrated into sequential calibration workflow.

Telemetry multi-graph format — debug.cpp upgraded to name@plotId:value for fc_tool multi-graph plotting (backward compatible — plain name:value and CSV still work)

All 8 build environments verified — teensy40, teensy40_calibration, teensy41, teensy36, esp32, esp32_calibration, esp32s3, esp32s3_calibration all pass.

What's Next (Flight Controller Focus)
The Teensy is flashed with calibration firmware and the hardware is connected. The main priorities from the todo are:

Complete IMU calibration (6-position, verify offsets)
Motor/ESC bench testing (when hardware ready)
Radio calibration (when controller available)
PID tuning on real hardware
ESP32 testing (when connected)
Want me to continue with any of these, or is there something specific you'd like to work on?