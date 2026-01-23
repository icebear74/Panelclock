/**
 * @file SpiSlaveReceiver.cpp
 * @brief SPI Slave Protocol Handler Implementation
 */

#include "SpiSlaveReceiver.hpp"

// Placeholder for RLE decompression (will be implemented later)
extern size_t decompressRLE(const uint8_t* compressed, size_t compressedSize, 
                           uint8_t* output, size_t outputMaxSize);

SpiSlaveReceiver::SpiSlaveReceiver(HUB75Driver& display, DisplayConfig& config)
    : _display(display), _config(config), 
      _frameCount(0), _errorCount(0), _lastFrameTime(0), _currentFPS(0.0f),
      _taskHandle(nullptr), _running(false),
      _rxBuffer(nullptr), _frameBuffer(nullptr) {
    
    // Calculate buffer sizes
    const size_t frameSize = _display.getWidth() * _display.getHeight() * 3; // RGB888
    _bufferSize = frameSize + 1024; // Extra for packet overhead
    
    // Allocate buffers in PSRAM
    _rxBuffer = (uint8_t*)ps_malloc(_bufferSize);
    _frameBuffer = (uint8_t*)ps_malloc(frameSize);
    
    if (!_rxBuffer || !_frameBuffer) {
        Serial.println("[SpiSlaveReceiver] FATAL: Failed to allocate buffers in PSRAM!");
    }
}

SpiSlaveReceiver::~SpiSlaveReceiver() {
    stop();
    if (_rxBuffer) free(_rxBuffer);
    if (_frameBuffer) free(_frameBuffer);
}

bool SpiSlaveReceiver::begin() {
    Serial.println("[SpiSlaveReceiver] Initializing SPI slave...");
    
    if (!_rxBuffer || !_frameBuffer) {
        Serial.println("[SpiSlaveReceiver] ERROR: Buffers not allocated!");
        return false;
    }
    
    // TODO: Configure SPI slave mode
    // For now, just set running flag
    _running = true;
    
    // Create receiver task on core 0 (opposite of Arduino core)
    BaseType_t result = xTaskCreatePinnedToCore(
        receiverTaskWrapper,
        "SpiReceiver",
        8192,
        this,
        2,  // High priority
        &_taskHandle,
        0   // Core 0
    );
    
    if (result != pdPASS) {
        Serial.println("[SpiSlaveReceiver] ERROR: Failed to create task!");
        _running = false;
        return false;
    }
    
    Serial.println("[SpiSlaveReceiver] SPI slave initialized");
    Serial.printf("[SpiSlaveReceiver] Pins - MISO:%d, MOSI:%d, SCK:%d, CS:%d\n",
                 MISO_PIN, MOSI_PIN, SCK_PIN, CS_PIN);
    return true;
}

void SpiSlaveReceiver::stop() {
    if (!_running) return;
    
    _running = false;
    
    if (_taskHandle) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
}

float SpiSlaveReceiver::getCurrentFPS() const {
    return _currentFPS;
}

void SpiSlaveReceiver::receiverTaskWrapper(void* param) {
    SpiSlaveReceiver* receiver = static_cast<SpiSlaveReceiver*>(param);
    receiver->receiverTask();
}

void SpiSlaveReceiver::receiverTask() {
    Serial.printf("[SpiSlaveReceiver::Task] Running on core %d\n", xPortGetCoreID());
    
    unsigned long lastFPSUpdate = 0;
    unsigned long framesInSecond = 0;
    
    while (_running) {
        // TODO: Implement actual SPI slave reception
        // For now, just simulate with delay
        
        // Update FPS calculation
        unsigned long now = millis();
        if (now - lastFPSUpdate >= 1000) {
            _currentFPS = framesInSecond;
            framesInSecond = 0;
            lastFPSUpdate = now;
        }
        
        // Placeholder: Wait for data
        delay(100);
    }
    
    Serial.println("[SpiSlaveReceiver::Task] Task exiting");
    vTaskDelete(nullptr);
}

void SpiSlaveReceiver::handleFrameData(const uint8_t* data, size_t length) {
    // TODO: Decompress RLE
    // TODO: Update display
    
    _frameCount++;
    _lastFrameTime = millis();
}

void SpiSlaveReceiver::handleConfigUpdate(const uint8_t* data, size_t length) {
    // Parse JSON config
    String jsonStr((const char*)data, length);
    
    if (_config.updateFromJSON(jsonStr.c_str())) {
        Serial.println("[SpiSlaveReceiver] Config updated - restart required for pin changes");
        // Save to NVS
        _config.saveToNVS();
        
        // Brightness can be applied live
        _display.setBrightness(_config.getBrightness());
    }
}

void SpiSlaveReceiver::handlePing() {
    // Respond to ping (keepalive)
    // TODO: Send ACK via MISO
}

bool SpiSlaveReceiver::verifyCRC(const uint8_t* data, size_t length, uint16_t receivedCRC) {
    uint16_t calculated = calculateCRC16(data, length);
    return (calculated == receivedCRC);
}

uint16_t SpiSlaveReceiver::calculateCRC16(const uint8_t* data, size_t length) {
    // CRC-16-CCITT
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}
