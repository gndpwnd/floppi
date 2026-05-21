/**
 * CRC-8-CCITT — shared checksum leaf (translation unit).
 *
 * The implementation lives entirely in crc8.h as an `inline` function so the
 * shared CRC needs no link-time dependency on a .cpp file: this matters for
 * both the host test compiles (which compile each module's .cpp directly,
 * without a build system) and the `arduino_uno_tuning` PlatformIO env (whose
 * build_src_filter does not glob src/util/).
 *
 * This file exists only so the module has a conventional .h/.cpp pair and so
 * the header is compiled at least once on its own for syntax checking. It
 * intentionally contains no symbols.
 */

#include "crc8.h"
