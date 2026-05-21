/*
 * GPS Passthrough Module Header  (USE_GPS — telemetry-only, raw NMEA)
 *
 * Optional GPS passthrough. PASSTHROUGH ONLY: raw NMEA bytes are read on a
 * dedicated ESP32 Core-1 FreeRTOS task, framed into complete sentences, and the
 * most-recent sentence is exposed verbatim via the swarm-API surface. The FC
 * parses NOTHING beyond sentence framing + a liveness bit — no fix decoding, no
 * fusion, no waypoints, no RTH, no geo-fence, no nav. It NEVER feeds the Core-0
 * 1 kHz flight loop (see scope.md and
 * docs/findings/gps_passthrough_spec_2026-05-20.md — Flavour A).
 *
 * The entire module is gated on USE_GPS. When the flag is undefined this header
 * expands to nothing and zero bytes are compiled.
 *
 * Enable: #define USE_GPS in config.h (default OFF).
 *
 * ESP32 / ESP32-S3 only — the whole point of passthrough is the WiFi telemetry
 * surface, which Teensy lacks (gps spec §4).
 */

#ifndef GPS_H
#define GPS_H

#include "config.h"

#ifdef USE_GPS

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>   // size_t

//=============================================================================
// Compile-time UART-collision guard  (gps spec §3 / §8 WS-A)
//=============================================================================
// USE_GPS claims UART1 (Serial1). iBUS / DSM / serial-commands also live on
// UART1 — only one consumer can own it. Fail loudly at compile time rather
// than ship an image where the GPS and the receiver fight for the same UART.
// pin_definitions_esp32.h is included transitively via config.h → pin_definitions.h.
#if (GPS_UART_NUM == 1)
    #if defined(USE_IBUS_RECEIVER)
        #error "USE_GPS conflicts with USE_IBUS_RECEIVER: both claim UART1 (Serial1). Pick one, or set GPS_UART_NUM 2 in config.h. See docs/findings/gps_passthrough_spec_2026-05-20.md."
    #endif
    #if defined(USE_DSM_RECEIVER)
        #error "USE_GPS conflicts with USE_DSM_RECEIVER: both claim UART1 (Serial1). Pick one, or set GPS_UART_NUM 2 in config.h. See docs/findings/gps_passthrough_spec_2026-05-20.md."
    #endif
    #if defined(USE_SERIAL_COMMANDS)
        #error "USE_GPS conflicts with USE_SERIAL_COMMANDS: both claim UART1 (Serial1). Pick one, or set GPS_UART_NUM 2 in config.h. See docs/findings/gps_passthrough_spec_2026-05-20.md."
    #endif
#endif
#if (GPS_UART_NUM == 2) && defined(USE_SBUS_RECEIVER)
    #error "USE_GPS with GPS_UART_NUM 2 conflicts with USE_SBUS_RECEIVER: both claim UART2 (Serial2). Use GPS_UART_NUM 1 (the default), or pick a non-SBUS receiver. See docs/findings/gps_passthrough_spec_2026-05-20.md."
#endif

//=============================================================================
// Longest single NMEA-0183 sentence is 82 chars incl. the $ and CRLF.
// +1 for the C-string NUL terminator.
//=============================================================================
#define GPS_NMEA_MAX 83

//=============================================================================
// Driver
//=============================================================================
// A small, self-contained, RX-only UART NMEA framer. begin() opens the UART;
// poll() drains the RX FIFO into a ring buffer and frames complete `$…\r\n`
// sentences. The latest complete sentence is held internally; the Core-1 task
// copies it into a spinlock-guarded snapshot.
//
// The FC does NOT parse NMEA — Flavour A, raw passthrough (gps spec §5).

class Gps {
public:
    Gps();

    // Open the configured UART (RX-only) at GPS_UART_BAUD. Non-blocking —
    // it does NOT wait for the first sentence (gps spec §7 item 3: power-up
    // race / cold-start is normal). Always succeeds; absence of a module is
    // reported later as a stale `ok=false` telemetry field, not a begin() fail.
    void begin();

    // Drain whatever bytes are waiting in the UART RX FIFO and frame any
    // complete NMEA sentences. Returns true if a NEW complete sentence was
    // framed during this call. Safe to call only from Core 1.
    bool poll();

    // Most-recent complete framed sentence (NUL-terminated). Empty until the
    // first sentence arrives. Valid only after poll() has returned true once.
    const char* latestSentence() const { return _latest; }

    // millis() timestamp of the last complete sentence (0 if none yet).
    uint32_t lastSentenceMs() const { return _last_ms; }

    // Count of sentences dropped because the ring buffer overflowed before a
    // framer could consume them (drop-oldest policy — gps spec §7 item 5).
    uint32_t droppedCount() const { return _dropped; }

private:
    // Single in-progress sentence accumulator + the published latest sentence.
    char     _accum[GPS_NMEA_MAX];
    uint16_t _accum_len;
    bool     _in_sentence;       // currently between a '$' and its terminator

    char     _latest[GPS_NMEA_MAX];
    uint32_t _last_ms;
    uint32_t _dropped;

    void resetAccumulator();
};

//=============================================================================
// Core-1 task API  (used by main.cpp)
//=============================================================================
// startGpsTask() spawns the dedicated Core-1 FreeRTOS task. It performs
// begin() inside the task, then loops: block briefly on UART RX, poll(),
// publish the latest sentence into a spinlock-guarded snapshot.
void startGpsTask();

//=============================================================================
// Telemetry snapshot accessors — safe to call from any Core-1 context
// (e.g. the swarm-API serializer). They copy the latest snapshot under a
// spinlock.
//=============================================================================
// Copy the most-recent NMEA sentence into `out` (NUL-terminated, truncated to
// `cap`). Returns the millisecond age of that sentence.
uint32_t gpsTelemetryNMEA(char* out, size_t cap);

// Liveness bit: true if a sentence has arrived within GPS_STALE_TIMEOUT_MS.
// false => GPS missing, unpowered, or no UART link. This is the ONLY piece of
// "parsing" the firmware does — a liveness bit, not a fix-quality bit.
bool gpsTelemetryOk();

#endif // USE_GPS

#endif // GPS_H
