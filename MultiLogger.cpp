#include "MultiLogger.hpp"
#include "GeneralTimeConverter.hpp"

// External time converter
extern GeneralTimeConverter* timeConverter;

// Global instance declaration
MultiLogger Log;

// External reference to serialMutex from Panelclock.ino
extern SemaphoreHandle_t serialMutex;

MultiLogger::MultiLogger(size_t bufferSize) 
    : _bufferSize(bufferSize), _writeIndex(0), _readIndex(0), _bufferFull(false), _debugFileEnabled(false) {
    
    // Pre-allocate ring buffer in PSRAM
    _ringBuffer.reserve(bufferSize);
    for (size_t i = 0; i < bufferSize; i++) {
        _ringBuffer.push_back(PsramString());
    }
    
    // Create mutex for thread safety
    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        Serial.println("[MultiLogger] FATAL: Failed to create mutex!");
    }
}

MultiLogger::~MultiLogger() {
    if (_debugFile) {
        _debugFile.close();
    }
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

size_t MultiLogger::write(uint8_t c) {
    // Forward to Serial immediately (for debugging)
    if (serialMutex) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            Serial.write(c);
            xSemaphoreGive(serialMutex);
        }
    } else {
        // Fallback: write directly if mutex not available (early startup)
        Serial.write(c);
    }
    
    // Lock for thread safety to update ring buffer
    if (_mutex && xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Add to current line buffer
        if (c == '\n') {
            _finalizeLine();
        } else if (c != '\r') { // Skip carriage returns
            _currentLine += (char)c;
        }
        
        xSemaphoreGive(_mutex);
    }
    
    return 1;
}

size_t MultiLogger::write(const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    
    // Forward to Serial immediately (for debugging)
    if (serialMutex) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
            Serial.write(buffer, size);
            xSemaphoreGive(serialMutex);
        }
    } else {
        // Fallback: write directly if mutex not available (early startup)
        Serial.write(buffer, size);
    }
    
    // Lock for thread safety to update ring buffer
    if (_mutex && xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Process buffer character by character
        for (size_t i = 0; i < size; i++) {
            uint8_t c = buffer[i];
            if (c == '\n') {
                _finalizeLine();
            } else if (c != '\r') { // Skip carriage returns
                _currentLine += (char)c;
            }
        }
        
        xSemaphoreGive(_mutex);
    }
    
    return size;
}

void MultiLogger::_finalizeLine() {
    // Add timestamp (real time if available, otherwise millis)
    char timeStr[32];
    if (timeConverter && timeConverter->isSuccessfullyParsed()) {
        time_t now = time(nullptr);
        time_t localTime = timeConverter->toLocal(now);
        struct tm timeinfo;
        gmtime_r(&localTime, &timeinfo);
        snprintf(timeStr, sizeof(timeStr), "[%02d:%02d:%02d] ", 
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        unsigned long timestamp = millis();
        snprintf(timeStr, sizeof(timeStr), "[%lu] ", timestamp);
    }
    
    PsramString finalLine = PsramString(timeStr) + _currentLine;
    
    // Write to debug file if enabled
    _writeToDebugFile(finalLine);
    
    // Add to ring buffer
    _ringBuffer[_writeIndex] = finalLine;
    
    // Update write index
    _writeIndex = (_writeIndex + 1) % _bufferSize;
    
    // Check if buffer is full
    if (_writeIndex == _readIndex) {
        _bufferFull = true;
        // Move read index forward to maintain ring buffer behavior
        _readIndex = (_readIndex + 1) % _bufferSize;
    }
    
    // Clear current line for next message
    _currentLine.clear();
}

bool MultiLogger::hasNewLines() {
    bool result = false;
    
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        result = (_writeIndex != _readIndex) || _bufferFull;
        xSemaphoreGive(_mutex);
    }
    
    return result;
}

size_t MultiLogger::getNewLines(PsramVector<PsramString>& outLines) {
    size_t count = 0;
    
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Read from readIndex to writeIndex
        while (_readIndex != _writeIndex || _bufferFull) {
            if (!_ringBuffer[_readIndex].empty()) {
                outLines.push_back(_ringBuffer[_readIndex]);
                count++;
            }
            
            _readIndex = (_readIndex + 1) % _bufferSize;
            _bufferFull = false; // Once we start reading, it's no longer full
        }
        
        xSemaphoreGive(_mutex);
    }
    
    return count;
}

size_t MultiLogger::getAllLines(PsramVector<PsramString>& outLines) {
    size_t count = 0;
    
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        size_t index = _bufferFull ? _writeIndex : 0;
        size_t endIndex = _writeIndex;
        
        do {
            if (!_ringBuffer[index].empty()) {
                outLines.push_back(_ringBuffer[index]);
                count++;
            }
            index = (index + 1) % _bufferSize;
        } while (index != endIndex && (_bufferFull || index < _writeIndex));
        
        xSemaphoreGive(_mutex);
    }
    
    return count;
}

void MultiLogger::clearBuffer() {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        for (auto& line : _ringBuffer) {
            line.clear();
        }
        _writeIndex = 0;
        _readIndex = 0;
        _bufferFull = false;
        _currentLine.clear();
        
        xSemaphoreGive(_mutex);
    }
}

void MultiLogger::setDebugFileEnabled(bool enabled) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        if (enabled && !_debugFileEnabled) {
            // Enable debug file logging
            _debugFileEnabled = true;
            // Open/create file in append mode
            _debugFile = LittleFS.open(DEBUG_FILE_PATH, "a");
            if (_debugFile) {
                Serial.println("[MultiLogger] Debug file logging enabled");
                _debugFile.println("\n=== Debug logging started ===");
                _debugFile.flush();
            } else {
                Serial.println("[MultiLogger] ERROR: Failed to open debug file");
                _debugFileEnabled = false;
            }
        } else if (!enabled && _debugFileEnabled) {
            // Disable debug file logging
            _debugFileEnabled = false;
            if (_debugFile) {
                _debugFile.println("=== Debug logging stopped ===\n");
                _debugFile.close();
                Serial.println("[MultiLogger] Debug file logging disabled");
            }
            // Delete the file to clear it
            if (LittleFS.exists(DEBUG_FILE_PATH)) {
                LittleFS.remove(DEBUG_FILE_PATH);
            }
        }
        
        xSemaphoreGive(_mutex);
    }
}

void MultiLogger::_writeToDebugFile(const PsramString& line) {
    if (!_debugFileEnabled || !_debugFile) {
        return;
    }
    
    // Check file size and truncate if needed
    if (_debugFile.size() > MAX_DEBUG_FILE_SIZE) {
        _debugFile.close();
        LittleFS.remove(DEBUG_FILE_PATH);
        _debugFile = LittleFS.open(DEBUG_FILE_PATH, "a");
        if (_debugFile) {
            _debugFile.println("=== Log file truncated (size limit reached) ===");
        }
    }
    
    if (_debugFile) {
        _debugFile.println(line.c_str());
        _debugFile.flush(); // Ensure data is written immediately
    }
}
