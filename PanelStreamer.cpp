#include "PanelStreamer.hpp"
#include "MultiLogger.hpp"
#include <ArduinoJson.h>

// Static instance for callback
PanelStreamer* PanelStreamer::_instance = nullptr;

// Constants for streaming
#define PANEL_STREAM_FPS 2
#define PANEL_STREAM_INTERVAL_MS (1000 / PANEL_STREAM_FPS)
#define MAX_CLIENTS 2

PanelStreamer::PanelStreamer(PanelManager* panelManager) 
    : _panelManager(panelManager), _wsServer(nullptr), _taskHandle(nullptr), 
      _running(false), _panelBuffer(nullptr), _compressedBuffer(nullptr) {
    
    _instance = this;
    _controlMutex = xSemaphoreCreateMutex();
    
    // Calculate buffer sizes
    _panelBufferSize = FULL_WIDTH * FULL_HEIGHT;
    _compressedBufferSize = _panelBufferSize * 3; // Worst case: control byte + 2 bytes per pixel
    
    // Allocate buffers in PSRAM
    _panelBuffer = (uint16_t*)ps_malloc(_panelBufferSize * sizeof(uint16_t));
    _compressedBuffer = (uint8_t*)ps_malloc(_compressedBufferSize);
    
    if (!_panelBuffer || !_compressedBuffer) {
        Log.println("[PanelStreamer] FATAL: Failed to allocate buffers in PSRAM!");
    } else {
        Log.printf("[PanelStreamer] Buffers allocated: Panel=%d bytes, Compressed=%d bytes\n", 
                   _panelBufferSize * sizeof(uint16_t), _compressedBufferSize);
    }
    
    // Create WebSocket server on port 81
    _wsServer = new WebSocketsServer(81);
    _wsServer->onEvent(webSocketEvent);
    _wsServer->begin();
    
    Log.println("[PanelStreamer] WebSocket server started on port 81");
}

PanelStreamer::~PanelStreamer() {
    stop();
    
    if (_wsServer) {
        delete _wsServer;
    }
    
    if (_panelBuffer) {
        free(_panelBuffer);
    }
    
    if (_compressedBuffer) {
        free(_compressedBuffer);
    }
    
    if (_controlMutex) {
        vSemaphoreDelete(_controlMutex);
    }
    
    _instance = nullptr;
}

void PanelStreamer::begin() {
    if (_running) return;
    
    if (xSemaphoreTake(_controlMutex, portMAX_DELAY) == pdTRUE) {
        _running = true;
        xSemaphoreGive(_controlMutex);
    }
    
    // Determine which core to use (opposite of Arduino core)
    BaseType_t app_core = xPortGetCoreID();
    BaseType_t stream_core = (app_core == 0) ? 1 : 0;
    
    // Create task pinned to non-Arduino core
    xTaskCreatePinnedToCore(
        streamerTaskWrapper,
        "PanelStreamer",
        8192,  // Stack size
        this,
        1,     // Priority
        &_taskHandle,
        stream_core
    );
    
    Log.printf("[PanelStreamer] Task started on core %d (Arduino core: %d)\n", stream_core, app_core);
}

void PanelStreamer::stop() {
    if (!_running) return;
    
    if (xSemaphoreTake(_controlMutex, portMAX_DELAY) == pdTRUE) {
        _running = false;
        xSemaphoreGive(_controlMutex);
    }
    
    if (_taskHandle) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    
    Log.println("[PanelStreamer] Task stopped");
}

uint8_t PanelStreamer::getClientCount() {
    if (!_wsServer) return 0;
    
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (_wsServer->clientIsConnected(i)) {
            count++;
        }
    }
    return count;
}

void PanelStreamer::streamerTaskWrapper(void* param) {
    PanelStreamer* streamer = static_cast<PanelStreamer*>(param);
    streamer->streamerTask();
}

void PanelStreamer::streamerTask() {
    Log.printf("[PanelStreamer] Task running on core %d\n", xPortGetCoreID());
    
    unsigned long lastPanelStreamMs = 0;
    
    while (_running) {
        // Process WebSocket events
        if (_wsServer) {
            _wsServer->loop();
        }
        
        // Check if any clients are connected
        uint8_t clientCount = getClientCount();
        
        if (clientCount == 0) {
            // No clients, sleep longer
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        
        // Stream log messages
        sendLogMessages();
        
        // Stream panel snapshot at defined FPS
        unsigned long now = millis();
        if (now - lastPanelStreamMs >= PANEL_STREAM_INTERVAL_MS) {
            sendPanelSnapshot();
            lastPanelStreamMs = now;
        }
        
        // Small delay to prevent task hogging
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    Log.println("[PanelStreamer] Task exiting");
    vTaskDelete(nullptr);
}

void PanelStreamer::sendPanelSnapshot() {
    if (!_panelManager || !_panelBuffer || !_compressedBuffer || !_wsServer) {
        return;
    }
    
    // Copy panel buffer (thread-safe)
    if (!_panelManager->copyFullPanelBuffer(_panelBuffer, _panelBufferSize)) {
        return;
    }
    
    // Compress with RLE
    size_t compressedSize = compressRLE(_panelBuffer, _panelBufferSize, 
                                        _compressedBuffer, _compressedBufferSize);
    
    if (compressedSize > 0) {
        // Send as binary WebSocket message to all connected clients
        _wsServer->broadcastBIN(_compressedBuffer, compressedSize);
    }
}

void PanelStreamer::sendLogMessages() {
    if (!_wsServer) return;
    
    // Get new log lines
    PsramVector<PsramString> newLines;
    size_t count = Log.getNewLines(newLines);
    
    if (count == 0) return;
    
    // Send each line as JSON text message
    for (const auto& line : newLines) {
        // Create JSON wrapper
        DynamicJsonDocument doc(512);
        doc["type"] = "log";
        doc["data"] = line.c_str();
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        
        // Send as text message to all connected clients
        _wsServer->broadcastTXT(jsonStr);
    }
}

size_t PanelStreamer::compressRLE(const uint16_t* input, size_t inputSize, 
                                   uint8_t* output, size_t outputMaxSize) {
    if (!input || !output || inputSize == 0 || outputMaxSize == 0) {
        return 0;
    }
    
    size_t outPos = 0;
    size_t inPos = 0;
    
    while (inPos < inputSize && outPos < outputMaxSize - 5) {
        uint16_t pixel = input[inPos];
        
        // Skip black pixels (0x0000) - they stay dark in the visualization
        // We need to track position, so we'll encode skips too
        if (pixel == 0x0000) {
            // Count consecutive black pixels
            uint16_t skipCount = 0;
            while (inPos < inputSize && input[inPos] == 0x0000 && skipCount < 65535) {
                skipCount++;
                inPos++;
            }
            
            // If we have skips, encode them: [0x00][skip_count_high][skip_count_low]
            if (skipCount > 0) {
                output[outPos++] = 0x00;  // Marker for skip
                output[outPos++] = (skipCount >> 8) & 0xFF;
                output[outPos++] = skipCount & 0xFF;
            }
            continue;
        }
        
        // Count consecutive same-colored pixels (max 255)
        uint8_t count = 1;
        while (inPos + count < inputSize && 
               input[inPos + count] == pixel && 
               input[inPos + count] != 0x0000 &&  // Don't include blacks
               count < 255) {
            count++;
        }
        
        // Encode: [count][high_byte][low_byte]
        output[outPos++] = count;
        output[outPos++] = (pixel >> 8) & 0xFF;  // High byte
        output[outPos++] = pixel & 0xFF;         // Low byte
        
        inPos += count;
    }
    
    return outPos;
}

void PanelStreamer::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Log.printf("[WebSocket] Client #%u disconnected\n", num);
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = _instance->_wsServer->remoteIP(num);
                Log.printf("[WebSocket] Client #%u connected from %d.%d.%d.%d\n", 
                          num, ip[0], ip[1], ip[2], ip[3]);
                
                // Check max clients
                if (_instance->getClientCount() > MAX_CLIENTS) {
                    Log.printf("[WebSocket] Max clients reached, disconnecting #%u\n", num);
                    _instance->_wsServer->disconnect(num);
                }
            }
            break;
            
        case WStype_TEXT:
            // Client sent text message (could be control commands in future)
            Log.printf("[WebSocket] Client #%u sent: %s\n", num, payload);
            break;
            
        case WStype_BIN:
            // Binary messages not expected from client
            break;
            
        default:
            break;
    }
}
