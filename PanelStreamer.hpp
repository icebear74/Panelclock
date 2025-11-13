#ifndef PANEL_STREAMER_HPP
#define PANEL_STREAMER_HPP

#include <Arduino.h>
#include <WebSocketsServer.h>
#include "PanelManager.hpp"
#include "MultiLogger.hpp"
#include "PsramUtils.hpp"

/**
 * @brief Manages WebSocket streaming of panel data and log messages
 * 
 * This class runs a FreeRTOS task on the non-Arduino core that:
 * 1. Streams compressed panel snapshots at ~2 FPS
 * 2. Streams log messages as they arrive
 * 3. Handles WebSocket client connections (max 2 clients)
 * 
 * RGB888 Compatibility Note:
 * Currently uses RGB565 (16-bit) format matching GFXcanvas16.
 * For RGB888 canvas support, modify:
 * - _panelBuffer type from uint16_t* to uint32_t* or use template
 * - compressRLE to handle 24/32-bit pixels
 * - Client-side decoder to handle RGB888 format
 */
class PanelStreamer {
public:
    /**
     * @brief Constructor
     * @param panelManager Pointer to the PanelManager for accessing panel data
     */
    PanelStreamer(PanelManager* panelManager);
    
    /**
     * @brief Destructor
     */
    ~PanelStreamer();
    
    /**
     * @brief Start the streamer task
     * Must be called after WiFi is connected
     */
    void begin();
    
    /**
     * @brief Stop the streamer task
     */
    void stop();
    
    /**
     * @brief Check if any clients are connected
     * @return Number of connected clients
     */
    uint8_t getClientCount();
    
    /**
     * @brief Get the WebSocket server instance
     * @return Pointer to the WebSocketsServer
     */
    WebSocketsServer* getWebSocketServer() { return _wsServer; }

private:
    // Panel manager reference
    PanelManager* _panelManager;
    
    // WebSocket server (port 81 for /stream endpoint)
    WebSocketsServer* _wsServer;
    
    // FreeRTOS task handle
    TaskHandle_t _taskHandle;
    
    // Control flags
    bool _running;
    SemaphoreHandle_t _controlMutex;
    
    // Panel buffer for copying (in PSRAM)
    uint16_t* _panelBuffer;
    size_t _panelBufferSize;
    
    // Compressed data buffer (in PSRAM)
    uint8_t* _compressedBuffer;
    size_t _compressedBufferSize;
    
    // Task function
    static void streamerTaskWrapper(void* param);
    void streamerTask();
    
    // Helper functions
    size_t compressRLE(const uint16_t* input, size_t inputSize, uint8_t* output, size_t outputMaxSize);
    void sendPanelSnapshot();
    void sendLogMessages();
    
    // WebSocket event handler
    static void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    static PanelStreamer* _instance; // For static callback
};

#endif // PANEL_STREAMER_HPP
