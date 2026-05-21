/*
 * GPS Passthrough Module Implementation  (USE_GPS — telemetry-only, raw NMEA)
 *
 * Self-contained RX-only UART NMEA framer + dedicated Core-1 FreeRTOS task.
 *
 * SCOPE: PASSTHROUGH ONLY (Flavour A — gps_passthrough_spec_2026-05-20.md §5).
 * Raw NMEA bytes are read on ESP32 Core 1, framed into complete `$…\r\n`
 * sentences, and the most-recent sentence is published into a spinlock-guarded
 * snapshot that the swarm-API serializer reads verbatim. The FC parses NOTHING
 * — no fix decoding, no fusion, no waypoints, no RTH, no geo-fence, no nav.
 * The Core-0 1 kHz flight loop never reads GPS data (scope.md: "The FC
 * stabilizes. Period.").
 *
 * The only firmware-side "intelligence" is sentence framing and a liveness bit
 * (gps.ok = bytes arrived within GPS_STALE_TIMEOUT_MS) — not a fix-quality bit.
 *
 * UART: UART1 (Serial1) by default, RX-only. Passthrough sends nothing to the
 * GPS, so GPS_PIN_TX is -1 (Arduino "unused" sentinel) and no TX GPIO is
 * claimed. UART0 is the USB debug console; UART2 is reserved for SBUS in the
 * default receiver build (gps spec §3). A compile-time #error guard in gps.h
 * forbids USE_GPS together with the UART1 serial receivers (iBUS/DSM/cmd).
 *
 * RX PIN: GPS_PIN_RX defaults to GPIO 4 (ESP32 UART1 RX) / GPIO 16 (ESP32-S3).
 * GPIO 4 is the existing UART1-RX default (IBUS_RX/DSM_RX/SERIAL_CMD_RX) and is
 * also SERVO_PIN_3's pre-2026-05-20 default — but SERVO_PIN_3 was already
 * reassigned to GPIO 13 by the W1b GPIO-conflict fix
 * (pin_definitions_esp32.h:113), so on a current build GPIO 4 is conflict-free
 * for GPS. The receiver-vs-GPS UART1 contention is caught by the #error guard.
 *
 * FAILURE MODES (gps spec §7): no module / unpowered (task blocks on RX, stays
 * stale, gps.ok=false), no fix yet (bytes still flow, ok=true, fix status is
 * inside the NMEA text for the consumer), power-up race (begin() is
 * non-blocking — no "wait for first sentence" loop), framing noise (only
 * `$…\r\n` sentences are accepted), buffer overflow (drop-oldest: the
 * in-progress accumulator resets and a counter increments). No GPS failure mode
 * can affect Core 0 — the worst case is a stale `gps` telemetry field.
 *
 * The whole file is gated on USE_GPS — zero bytes when the flag is off.
 */

#include "config.h"

#ifdef USE_GPS

#include "gps.h"
#include "pin_definitions.h"   // GPS_PIN_RX / GPS_PIN_TX (ESP32 defaults)
#include <Arduino.h>
#include <string.h>

//=============================================================================
// UART selection — Serial1 (UART1) by default, Serial2 if GPS_UART_NUM == 2.
//=============================================================================
#if (GPS_UART_NUM == 2)
    #define GPS_SERIAL Serial2
#else
    #define GPS_SERIAL Serial1
#endif

//=============================================================================
// Gps class
//=============================================================================
Gps::Gps()
    : _accum_len(0),
      _in_sentence(false),
      _last_ms(0),
      _dropped(0) {
    _accum[0]  = '\0';
    _latest[0] = '\0';
}

void Gps::resetAccumulator() {
    _accum_len   = 0;
    _accum[0]    = '\0';
    _in_sentence = false;
}

void Gps::begin() {
    // RX-only: GPS_PIN_TX is -1 so no TX GPIO is claimed (passthrough sends
    // nothing to the module — factory NMEA output is fine). Non-blocking:
    // begin() opens the UART and returns immediately. The absence of a fix at
    // boot is normal — GPS cold-start can take 1-30 s (gps spec §7 item 3).
    GPS_SERIAL.begin(GPS_UART_BAUD, SERIAL_8N1, GPS_PIN_RX, GPS_PIN_TX);
}

bool Gps::poll() {
    bool framed_new = false;

    // Drain whatever the ESP-IDF UART driver has buffered in its RX FIFO.
    // GPS_BUFFER_BYTES caps how many bytes we process per call so a byte-storm
    // cannot monopolise the task; leftover bytes stay in the driver FIFO for
    // the next poll().
    int budget = GPS_BUFFER_BYTES;
    while (budget-- > 0 && GPS_SERIAL.available() > 0) {
        char c = (char)GPS_SERIAL.read();

        if (c == '$') {
            // Start of a new sentence — discard any partial accumulation
            // (line noise between sentences is simply dropped).
            resetAccumulator();
            _in_sentence  = true;
            _accum[_accum_len++] = c;
            continue;
        }

        if (!_in_sentence) {
            // Bytes outside a sentence frame — skip (framing-error tolerance).
            continue;
        }

        if (c == '\r') {
            // Ignore CR; the LF below terminates the sentence.
            continue;
        }

        if (c == '\n') {
            // Complete sentence: publish it as the latest, terminated string.
            _accum[_accum_len] = '\0';
            // A bare "$\n" is not a real sentence — require some payload.
            if (_accum_len > 1) {
                memcpy(_latest, _accum, _accum_len + 1);
                _last_ms   = millis();
                framed_new = true;
            }
            resetAccumulator();
            continue;
        }

        // Ordinary sentence byte.
        if (_accum_len < (GPS_NMEA_MAX - 1)) {
            _accum[_accum_len++] = c;
        } else {
            // Buffer overflow: the sentence is longer than a valid NMEA frame
            // (line noise or a runaway talker). Drop-oldest policy — discard
            // the in-progress accumulator and count it (gps spec §7 item 5).
            _dropped++;
            resetAccumulator();
        }
    }

    return framed_new;
}

//=============================================================================
// Core-1 task + spinlock-guarded telemetry snapshot
//=============================================================================
// The task owns the Gps instance. It publishes the latest sentence into the
// snapshot below; the swarm-API serializer reads the snapshot under the same
// spinlock. The flight loop (Core 0) never touches any of this.

static Gps              s_gps;
static TaskHandle_t     s_gps_task_handle = NULL;
static portMUX_TYPE     s_gps_mux = portMUX_INITIALIZER_UNLOCKED;

static struct {
    char     nmea[GPS_NMEA_MAX];
    uint32_t last_ms;        // millis() of the most-recent sentence (0 = none)
} s_gps_snapshot = { { 0 }, 0 };

static void gpsTask(void* parameter) {
    (void)parameter;

    // begin() runs inside the task so opening the UART never delays setup() /
    // setupWebServer() on the main path. It is non-blocking regardless.
    s_gps.begin();
    Serial.printf("[GPS] UART%d RX-only @ %ld baud on GPIO %d — passthrough, telemetry-only\n",
                  (int)GPS_UART_NUM, (long)GPS_UART_BAUD, (int)GPS_PIN_RX);

    for (;;) {
        // Block briefly so the task yields when the GPS has nothing to say.
        // The ESP-IDF UART driver fills its RX FIFO from an ISR regardless of
        // which task runs, so even a 2 s blocking HTTP POST in the Arduino
        // loopTask cannot cause byte loss here (fc_core1_budget §5).
        if (s_gps.poll()) {
            portENTER_CRITICAL(&s_gps_mux);
            memcpy(s_gps_snapshot.nmea, s_gps.latestSentence(), GPS_NMEA_MAX);
            s_gps_snapshot.nmea[GPS_NMEA_MAX - 1] = '\0';
            s_gps_snapshot.last_ms = s_gps.lastSentenceMs();
            portEXIT_CRITICAL(&s_gps_mux);
        }
        // ~10 ms idle delay — keeps the task off the CPU between NMEA bursts.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void startGpsTask() {
    // Core 1, priority 1, 3072 B stack — per the Core-1 budget analysis
    // (fc_core1_budget_2026-05-20.md §7, W5 row). Priority 1 is equal to the
    // Arduino loopTask so a byte-storm cannot starve the web server. The task
    // is a separate schedulable entity, so it is immune to the blocking 2 Hz
    // HTTP POST that stalls the loopTask.
    xTaskCreatePinnedToCore(
        gpsTask,
        "Gps",
        3072,
        NULL,
        1,                      // priority 1 — equal to the Arduino loopTask
        &s_gps_task_handle,
        1                       // Core 1
    );
}

//=============================================================================
// Telemetry snapshot accessors
//=============================================================================
uint32_t gpsTelemetryNMEA(char* out, size_t cap) {
    if (out == NULL || cap == 0) return 0;

    uint32_t last_ms;
    portENTER_CRITICAL(&s_gps_mux);
    last_ms = s_gps_snapshot.last_ms;
    size_t n = strlen(s_gps_snapshot.nmea);
    if (n > cap - 1) n = cap - 1;
    memcpy(out, s_gps_snapshot.nmea, n);
    out[n] = '\0';
    portEXIT_CRITICAL(&s_gps_mux);

    if (last_ms == 0) return 0;                  // no sentence yet
    uint32_t now = millis();
    return (now >= last_ms) ? (now - last_ms) : 0;
}

bool gpsTelemetryOk() {
    portENTER_CRITICAL(&s_gps_mux);
    uint32_t last_ms = s_gps_snapshot.last_ms;
    portEXIT_CRITICAL(&s_gps_mux);

    if (last_ms == 0) return false;              // never received a sentence
    uint32_t now = millis();
    uint32_t age = (now >= last_ms) ? (now - last_ms) : 0;
    return age <= GPS_STALE_TIMEOUT_MS;
}

#endif // USE_GPS
