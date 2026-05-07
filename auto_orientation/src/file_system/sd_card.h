/**
 * SD Card Module for Snapshot Recording
 *
 * Provides low-level SD card I/O operations for the snapshot recorder feature.
 * Uses Arduino SD library (https://www.arduino.cc/reference/en/libraries/sd/)
 *
 * Hardware:
 * - Arduino Mega: CS pin 53 (standard SPI)
 * - SPI pins: MOSI (51), MISO (50), SCK (52)
 *
 * Compilation:
 * - Only compiled when SNAPSHOT_MODE is enabled
 * - Adds ~4KB flash, minimal RAM overhead
 *
 * Example Usage:
 *   if (sd_card.initialize()) {
 *     if (sd_card.create_file("data.txt")) {
 *       sd_card.append_line("data.txt", "Hello, World!");
 *       sd_card.close_file("data.txt");
 *     }
 *   }
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Configuration
// ============================================================================

// Arduino Mega default SPI pins
#define SD_CARD_CS_PIN 53
#define SD_CARD_MOSI_PIN 51
#define SD_CARD_MISO_PIN 50
#define SD_CARD_SCK_PIN 52

// Maximum line length for append_line operation (in bytes)
#define SD_CARD_MAX_LINE_LENGTH 512

// ============================================================================
// Error Codes
// ============================================================================

enum SDCardError {
  SD_OK = 0,                    // No error
  SD_INIT_FAILED = 1,           // SD card initialization failed
  SD_NOT_INITIALIZED = 2,       // SD card not initialized
  SD_FILE_NOT_FOUND = 3,        // File does not exist
  SD_FILE_CREATE_FAILED = 4,    // Failed to create file
  SD_FILE_WRITE_FAILED = 5,     // Failed to write to file
  SD_FILE_OPEN_FAILED = 6,      // Failed to open file
  SD_FILE_CLOSE_FAILED = 7,     // Failed to close file
  SD_DIRECTORY_CREATE_FAILED = 8, // Failed to create directory
};

// ============================================================================
// SD Card Class
// ============================================================================

class SDCard {
 public:
  /**
   * Default constructor.
   */
  SDCard();

  /**
   * Initialize SD card and SPI bus.
   * Must be called before any other operations.
   *
   * @return true if initialization successful, false otherwise
   *
   * Behavior:
   * - Initializes SPI at 50 MHz (or lower if card doesn't respond)
   * - Verifies card is present and readable
   * - Logs status to Serial if available
   */
  bool initialize();

  /**
   * Check if SD card is present and ready.
   * Does not re-initialize; uses cached state from last initialize() call.
   *
   * @return true if SD card is initialized and ready
   */
  bool is_ready() const;

  /**
   * Create a new file or truncate existing file.
   * File is created but not opened for writing.
   *
   * @param filename Path/name of file to create (e.g., "data.txt" or "dir/data.txt")
   * @return true if file created successfully
   *
   * Note:
   * - If file already exists, it will be truncated (emptied)
   * - Does not open the file; use append_line() to write data
   * - Directory must exist (create with create_directory())
   */
  bool create_file(const char* filename);

  /**
   * Append a line of text to a file.
   * Automatically opens the file, appends data + newline, and keeps file open.
   *
   * @param filename Path/name of file to write to
   * @param json_data Null-terminated string to append (should be JSON)
   * @return true if write successful
   *
   * Behavior:
   * - Opens file in append mode if not already open
   * - Writes the json_data as-is
   * - Adds newline character (\n) after data
   * - File remains open for efficiency (close with close_file())
   * - If line length > 512 bytes, line is truncated and error logged
   *
   * Note:
   * - Not recommended to switch between multiple files frequently
   *   (each switch may cause disk fragmentation)
   */
  bool append_line(const char* filename, const char* json_data);

  /**
   * Close a file and flush all pending data.
   * Must be called after writing to ensure data is persisted to SD.
   *
   * @param filename Path/name of file to close
   * @return true if close successful
   *
   * Behavior:
   * - Flushes data buffer to SD card
   * - Releases file handle for other operations
   * - Safe to call even if file is not open
   */
  bool close_file(const char* filename);

  /**
   * Check if a file exists.
   *
   * @param filename Path/name of file to check
   * @return true if file exists, false otherwise
   */
  bool file_exists(const char* filename) const;

  /**
   * Create a directory.
   * Must be called before creating files in subdirectories.
   *
   * @param dirname Path/name of directory to create (e.g., "snapshots")
   * @return true if directory created (or already exists)
   *
   * Note:
   * - If directory already exists, returns true (idempotent)
   * - Parent directories must exist
   */
  bool create_directory(const char* dirname);

  /**
   * Get human-readable status string.
   * Useful for debugging.
   *
   * @return Status message (e.g., "SD Card Ready" or "SD Card Not Initialized")
   */
  const char* get_status_string() const;

  /**
   * Get last error code.
   * Useful for debugging why an operation failed.
   *
   * @return Error code (see SDCardError enum)
   */
  uint8_t get_last_error() const { return last_error_; }

 private:
  bool initialized_;
  uint8_t last_error_;

  // Track currently open file to avoid redundant opens
  char open_filename_[64];
  bool has_open_file_;
};

#endif  // SD_CARD_H
