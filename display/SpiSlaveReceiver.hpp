/**
 * @file SpiSlaveReceiver.hpp
 * @brief SPI Slave Protocol Handler
 * 
 * Receives data from Control-ESP via SPI:
 * - Frame data (RGB888 compressed with RLE)
 * - Configuration updates
 * - OTA firmware chunks
 */

#ifndef SPI_SLAVE_RECEIVER_HPP
#define SPI_SLAVE_RECEIVER_HPP

#include <Arduino.h>
#include <SPI.h>
#include "HUB75Driver.hpp"
#include "DisplayConfig.hpp"

// SPI Packet Types (must match Control-ESP)
enum class SpiPacketType : uint8_t {
    FRAME_DATA = 0x01,
    CONFIG_UPDATE = 0x02,
    OTA_START = 0x03,
    OTA_DATA = 0x04,
    PING = 0x05,
    ACK = 0x06
};

class SpiSlaveReceiver {
public:
    SpiSlaveReceiver(HUB75Driver& display, DisplayConfig& config);
    ~SpiSlaveReceiver();
    
    bool begin();
    void stop();
    
    // Statistics
    unsigned long getFrameCount() const { return _frameCount; }
    unsigned long getErrorCount() const { return _errorCount; }
    float getCurrentFPS() const;
    
private:
    HUB75Driver& _display;
    DisplayConfig& _config;
    
    // SPI configuration (default pins for ESP32-S3)
    static const uint8_t MISO_PIN = 37;
    static const uint8_t MOSI_PIN = 35;
    static const uint8_t SCK_PIN = 36;
    static const uint8_t CS_PIN = 34;
    
    // Statistics
    unsigned long _frameCount;
    unsigned long _errorCount;
    unsigned long _lastFrameTime;
    float _currentFPS;
    
    // Task handle
    TaskHandle_t _taskHandle;
    bool _running;
    
    // Buffers (in PSRAM)
    uint8_t* _rxBuffer;
    uint8_t* _frameBuffer;  // RGB888 decompressed frame
    size_t _bufferSize;
    
    // Task function
    static void receiverTaskWrapper(void* param);
    void receiverTask();
    
    // Packet handlers
    void handleFrameData(const uint8_t* data, size_t length);
    void handleConfigUpdate(const uint8_t* data, size_t length);
    void handlePing();
    
    // CRC validation
    bool verifyCRC(const uint8_t* data, size_t length, uint16_t receivedCRC);
    uint16_t calculateCRC16(const uint8_t* data, size_t length);
};

#endif // SPI_SLAVE_RECEIVER_HPP
