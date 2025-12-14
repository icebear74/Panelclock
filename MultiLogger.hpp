#ifndef MULTILOGGER_HPP
#define MULTILOGGER_HPP

#include <Arduino.h>
#include <Print.h>
#include <FS.h>
#include <LittleFS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "PsramUtils.hpp"

/**
 * @brief Thread-safe logging class that forwards output to Serial and stores in a ring buffer.
 * 
 * This class inherits from Print so it can be used as a drop-in replacement for Serial.
 * All output is forwarded to both the physical Serial port and a thread-safe ring buffer
 * in PSRAM that stores the last N log lines for WebSocket streaming.
 */
class MultiLogger : public Print {
public:
    /**
     * @brief Constructor
     * @param bufferSize Number of log lines to keep in the ring buffer (default: 100)
     */
    MultiLogger(size_t bufferSize = 100);
    
    /**
     * @brief Destructor
     */
    ~MultiLogger();
    
    /**
     * @brief Write a single byte (required by Print class)
     * @param c The byte to write
     * @return Number of bytes written (always 1)
     */
    size_t write(uint8_t c) override;
    
    /**
     * @brief Write multiple bytes (optimized version)
     * @param buffer Pointer to data buffer
     * @param size Number of bytes to write
     * @return Number of bytes written
     */
    size_t write(const uint8_t* buffer, size_t size) override;
    
    /**
     * @brief Check if there are new log lines available
     * @return true if new lines are available since last read
     */
    bool hasNewLines();
    
    /**
     * @brief Get all new log lines since last call
     * @param outLines Output vector to store the log lines
     * @return Number of lines retrieved
     */
    size_t getNewLines(PsramVector<PsramString>& outLines);
    
    /**
     * @brief Get all log lines in the buffer
     * @param outLines Output vector to store the log lines
     * @return Number of lines retrieved
     */
    size_t getAllLines(PsramVector<PsramString>& outLines);
    
    /**
     * @brief Clear the ring buffer
     */
    void clearBuffer();
    
    /**
     * @brief Enable or disable debug file logging
     * @param enabled true to enable file logging, false to disable
     */
    void setDebugFileEnabled(bool enabled);
    
    /**
     * @brief Check if debug file logging is enabled
     * @return true if enabled
     */
    bool isDebugFileEnabled() const { return _debugFileEnabled; }

private:
    // Ring buffer stored in PSRAM
    PsramVector<PsramString> _ringBuffer;
    size_t _bufferSize;
    size_t _writeIndex;
    size_t _readIndex;
    bool _bufferFull;
    
    // Current line being built
    PsramString _currentLine;
    
    // Mutex for thread safety
    SemaphoreHandle_t _mutex;
    
    // Debug file logging
    bool _debugFileEnabled;
    File _debugFile;
    const char* DEBUG_FILE_PATH = "/debug.log";
    const size_t MAX_DEBUG_FILE_SIZE = 100 * 1024; // 100KB
    
    /**
     * @brief Finalize the current line and add it to the ring buffer
     */
    void _finalizeLine();
    
    /**
     * @brief Write line to debug file if enabled
     */
    void _writeToDebugFile(const PsramString& line);
};

// Serial mutex (from main application) - external declaration
extern SemaphoreHandle_t serialMutex;

// Global instance - to be used instead of Serial
extern MultiLogger Log;

#endif // MULTILOGGER_HPP
