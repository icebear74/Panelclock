#include "MultiLogger.hpp"

// Global instance declaration
MultiLogger Log;

// External reference to serialMutex from Panelclock.ino
extern SemaphoreHandle_t serialMutex;

MultiLogger::MultiLogger(size_t bufferSize) 
    : _bufferSize(bufferSize), _writeIndex(0), _readIndex(0), _bufferFull(false) {
    
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
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

size_t MultiLogger::write(uint8_t c) {
    // Lock for thread safety
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Forward to Serial (with its own mutex if available)
        if (serialMutex) {
            if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
                Serial.write(c);
                xSemaphoreGive(serialMutex);
            }
        } else {
            // Fallback: write directly if mutex not available (early startup)
            Serial.write(c);
        }
        
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
    
    // Lock for thread safety
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Forward to Serial (with its own mutex if available)
        if (serialMutex) {
            if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
                Serial.write(buffer, size);
                xSemaphoreGive(serialMutex);
            }
        } else {
            // Fallback: write directly if mutex not available (early startup)
            Serial.write(buffer, size);
        }
        
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
    // Add timestamp
    unsigned long timestamp = millis();
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "[%lu] ", timestamp);
    
    PsramString finalLine = PsramString(timeStr) + _currentLine;
    
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
