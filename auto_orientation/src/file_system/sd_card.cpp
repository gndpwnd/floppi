/**
 * SD Card Module Implementation
 *
 * Uses Arduino SD library for cross-platform compatibility.
 * Handles file I/O with error checking and state tracking.
 */

#include "sd_card.h"
#include <Arduino.h>
#include <SD.h>
#include <string.h>

// ============================================================================
// Constructor
// ============================================================================

SDCard::SDCard()
    : initialized_(false), last_error_(SD_OK), has_open_file_(false) {
  memset(open_filename_, 0, sizeof(open_filename_));
}

// ============================================================================
// Initialization
// ============================================================================

bool SDCard::initialize() {
  // Try to initialize SD card with CS on pin 53
  if (!SD.begin(SD_CARD_CS_PIN)) {
    last_error_ = SD_INIT_FAILED;
    Serial.println("ERROR: SD card initialization failed!");
    return false;
  }

  initialized_ = true;
  last_error_ = SD_OK;
  Serial.println("✓ SD Card initialized successfully");
  return true;
}

// ============================================================================
// Status Checks
// ============================================================================

bool SDCard::is_ready() const {
  return initialized_;
}

const char* SDCard::get_status_string() const {
  if (initialized_) {
    return "SD Card Ready";
  } else {
    return "SD Card Not Initialized";
  }
}

// ============================================================================
// File Operations
// ============================================================================

bool SDCard::create_file(const char* filename) {
  if (!initialized_) {
    last_error_ = SD_NOT_INITIALIZED;
    Serial.println("ERROR: SD card not initialized");
    return false;
  }

  if (!filename || filename[0] == '\0') {
    last_error_ = SD_FILE_CREATE_FAILED;
    Serial.println("ERROR: Invalid filename");
    return false;
  }

  // Close any open file first
  if (has_open_file_) {
    close_file(open_filename_);
  }

  // Remove existing file if it exists
  if (SD.exists(filename)) {
    if (!SD.remove(filename)) {
      last_error_ = SD_FILE_CREATE_FAILED;
      Serial.printf("ERROR: Could not remove existing file: %s\n", filename);
      return false;
    }
  }

  // Create new file
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    last_error_ = SD_FILE_CREATE_FAILED;
    Serial.printf("ERROR: Could not create file: %s\n", filename);
    return false;
  }

  file.close();
  last_error_ = SD_OK;
  return true;
}

bool SDCard::append_line(const char* filename, const char* json_data) {
  if (!initialized_) {
    last_error_ = SD_NOT_INITIALIZED;
    Serial.println("ERROR: SD card not initialized");
    return false;
  }

  if (!filename || filename[0] == '\0') {
    last_error_ = SD_FILE_WRITE_FAILED;
    Serial.println("ERROR: Invalid filename");
    return false;
  }

  if (!json_data || json_data[0] == '\0') {
    last_error_ = SD_FILE_WRITE_FAILED;
    Serial.println("ERROR: Empty data to write");
    return false;
  }

  // Check line length
  if (strlen(json_data) > SD_CARD_MAX_LINE_LENGTH - 10) {  // -10 for newline and safety
    last_error_ = SD_FILE_WRITE_FAILED;
    Serial.printf("ERROR: Line too long for file %s\n", filename);
    return false;
  }

  // Open file in append mode
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    last_error_ = SD_FILE_OPEN_FAILED;
    Serial.printf("ERROR: Could not open file for writing: %s\n", filename);
    return false;
  }

  // Seek to end of file (FILE_WRITE should do this, but be explicit)
  file.seek(file.size());

  // Write data
  size_t written = file.print(json_data);
  if (written != strlen(json_data)) {
    file.close();
    last_error_ = SD_FILE_WRITE_FAILED;
    Serial.printf("ERROR: Failed to write complete data to %s\n", filename);
    return false;
  }

  // Write newline
  written = file.print("\n");
  if (written != 1) {
    file.close();
    last_error_ = SD_FILE_WRITE_FAILED;
    Serial.printf("ERROR: Failed to write newline to %s\n", filename);
    return false;
  }

  // Flush to ensure data is persisted
  file.flush();

  file.close();
  last_error_ = SD_OK;
  return true;
}

bool SDCard::close_file(const char* filename) {
  if (!initialized_) {
    last_error_ = SD_NOT_INITIALIZED;
    return false;
  }

  if (!filename || filename[0] == '\0') {
    return true;  // Safe no-op for invalid filename
  }

  // In Arduino SD library, file handles are automatically closed when File object
  // goes out of scope. This function is a no-op but provided for API consistency.
  has_open_file_ = false;
  memset(open_filename_, 0, sizeof(open_filename_));

  last_error_ = SD_OK;
  return true;
}

bool SDCard::file_exists(const char* filename) const {
  if (!initialized_) {
    return false;
  }

  if (!filename || filename[0] == '\0') {
    return false;
  }

  return SD.exists(filename);
}

bool SDCard::create_directory(const char* dirname) {
  if (!initialized_) {
    last_error_ = SD_NOT_INITIALIZED;
    Serial.println("ERROR: SD card not initialized");
    return false;
  }

  if (!dirname || dirname[0] == '\0') {
    last_error_ = SD_DIRECTORY_CREATE_FAILED;
    return false;
  }

  // Check if directory already exists
  File root = SD.open(dirname);
  if (root && root.isDirectory()) {
    root.close();
    return true;  // Already exists
  }
  if (root) {
    root.close();
  }

  // Try to create directory using mkdir (if available)
  // Note: Arduino SD library doesn't have built-in mkdir for all boards
  // Workaround: create a file in the directory path, then delete it
  char test_file[128];
  snprintf(test_file, sizeof(test_file), "%s/.test", dirname);

  File file = SD.open(test_file, FILE_WRITE);
  if (!file) {
    last_error_ = SD_DIRECTORY_CREATE_FAILED;
    Serial.printf("ERROR: Could not create directory: %s\n", dirname);
    return false;
  }

  file.close();
  SD.remove(test_file);

  last_error_ = SD_OK;
  return true;
}
