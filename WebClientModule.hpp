#ifndef WEBCLIENTMODULE_HPP
#define WEBCLIENTMODULE_HPP

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <queue>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "PsramUtils.hpp"
#include <esp_heap_caps.h>

// Simple PSRAM-backed stream for downloads
class PsramBufferStream : public Stream {
public:
    PsramBufferStream() {
        _buffer = (char*)ps_malloc(INITIAL_SIZE);
        if (_buffer) _capacity = INITIAL_SIZE;
    }
    ~PsramBufferStream() {
        if (_buffer) free(_buffer);
    }

    size_t write(uint8_t data) override {
        if (!_buffer) return 0;
        if (_position >= _capacity) {
            if (!grow()) return 0;
        }
        _buffer[_position++] = data;
        return 1;
    }

    size_t write(const uint8_t *buffer, size_t size) override {
        if (!_buffer) return 0;
        for (size_t i = 0; i < size; ++i) {
            if (write(buffer[i]) == 0) return i;
        }
        return size;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    char* getBuffer() { return _buffer; }
    size_t getSize() { return _position; }

    // Transfers ownership to caller; the stream no longer frees the buffer.
    char* releaseBuffer() {
        char* tmp = _buffer;
        _buffer = nullptr;
        _capacity = 0;
        _position = 0;
        return tmp;
    }

private:
    char* _buffer = nullptr;
    size_t _capacity = 0;
    size_t _position = 0;
    static const size_t INITIAL_SIZE = 16 * 1024; // start smaller, grow if needed
    static const size_t MAX_SIZE = 2 * 1024 * 1024;

    bool grow() {
        if (_capacity >= MAX_SIZE) return false;
        size_t new_capacity = std::min(_capacity ? _capacity * 2 : (size_t)INITIAL_SIZE, MAX_SIZE);
        char* new_buffer = (char*)ps_realloc(_buffer, new_capacity);
        if (!new_buffer) {
            Serial.println("[PsramBufferStream] FEHLER: ps_realloc fehlgeschlagen!");
            return false;
        }
        _buffer = new_buffer;
        _capacity = new_capacity;
        Serial.printf("[PsramBufferStream] Puffer auf %u Bytes vergrößert.\n", (unsigned)_capacity);
        return true;
    }
};


struct WebRequest {
    String url;
    // callback: buffer is a PSRAM-allocated char*; caller must free(buffer) after use
    std::function<void(char*, size_t)> onSuccess = nullptr;
    std::function<void(int)> onFailure = nullptr;
};

class WebClientModule {
public:
    WebClientModule() {
        requestQueue = xQueueCreate(10, sizeof(WebRequest*));
        workerTaskHandle = NULL;
    }

    void begin() {
        // create worker task with increased stack to be safe
        xTaskCreatePinnedToCore(webWorkerTask, "WebWorker", 16384, this, 2, &workerTaskHandle, 1);
    }

    void addRequest(const WebRequest& request) {
        WebRequest* reqCopy = new WebRequest(request);
        if (xQueueSend(requestQueue, &reqCopy, pdMS_TO_TICKS(100)) != pdPASS) {
            Serial.println("[WebClient] Warteschlange voll, Anfrage verworfen!");
            delete reqCopy;
        }
    }

private:
    QueueHandle_t requestQueue;
    TaskHandle_t workerTaskHandle;

    static void dumpHeapDetailed(const char* tag) {
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        size_t free_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        Serial.printf("[WEB_HEAP_DETAILED] %-22s INTERNAL_FREE=%u LARGEST=%u PSRAM=%u\n",
                      tag, (unsigned)free_internal, (unsigned)largest_block, (unsigned)free_spiram);
    }

    static void webWorkerTask(void* param) {
        WebClientModule* self = static_cast<WebClientModule*>(param);

        while (true) {
            WebRequest* request;
            if (!xQueueReceive(self->requestQueue, &request, portMAX_DELAY)) continue;

            if (WiFi.status() != WL_CONNECTED) {
                Serial.printf("[WebWorker] Keine WLAN-Verbindung f&uuml;r: %s\n", request->url.c_str());
                if (request->onFailure) request->onFailure(-7);
                delete request;
                continue;
            }

            int httpCode = 0;
            bool success = false;
            Serial.printf("[WebWorker] Rufe URL auf: %s\n", request->url.c_str());

            for (int attempt = 1; attempt <= 3; ++attempt) {
                Serial.printf("[WebWorker] Versuch %d/3...\n", attempt);

                // Local client + http per request (forces full cleanup)
                WiFiClientSecure client;
                client.setInsecure();
                HTTPClient http;

                dumpHeapDetailed("before http.begin");
                bool began = http.begin(client, request->url);
                dumpHeapDetailed("after http.begin");

                if (!began) {
                    Serial.println("[WebWorker] http.begin fehlgeschlagen");
                    httpCode = -30;
                } else {
                    httpCode = http.GET();
                    Serial.printf("[WebWorker] http.GET() => %d\n", httpCode);
                    dumpHeapDetailed("after GET");

                    if (httpCode == HTTP_CODE_OK) {
                        // PSRAM buffer approach (small and moderate responses)
                        PsramBufferStream psramStream;
                        dumpHeapDetailed("before writeToStream->psram");
                        size_t written = http.writeToStream(&psramStream);
                        dumpHeapDetailed("after writeToStream->psram");

                        if (written > 0) {
                            size_t size = psramStream.getSize();
                            char* buffer = psramStream.releaseBuffer();
                            if (buffer) {
                                buffer[size] = '\0';
                                // Free HTTP internal resources BEFORE heavy processing
                                dumpHeapDetailed("before http.end (psram)");
                                http.end();
                                dumpHeapDetailed("after http.end (psram)");
                                client.stop();

                                dumpHeapDetailed("before onSuccess (psram)");
                                if (request->onSuccess) request->onSuccess(buffer, size);
                                dumpHeapDetailed("after onSuccess (psram)");

                                // caller is responsible to free(buffer)
                                success = true;
                            } else {
                                Serial.println("[WebWorker] FEHLER: releaseBuffer() returned null");
                                httpCode = -21;
                                dumpHeapDetailed("before http.end (null buffer)");
                                http.end();
                                dumpHeapDetailed("after http.end (null buffer)");
                                client.stop();
                            }
                        } else {
                            Serial.println("[WebWorker] FEHLER: Konnte keine Daten in PSRAM-Stream schreiben.");
                            httpCode = -20;
                            dumpHeapDetailed("before http.end (psram write fail)");
                            http.end();
                            dumpHeapDetailed("after http.end (psram write fail)");
                            client.stop();
                        }
                    }
                }

                // ensure cleanup in any path
                dumpHeapDetailed("before http.end (ensure)");
                http.end();
                dumpHeapDetailed("after http.end (ensure)");
                client.stop();
                dumpHeapDetailed("after client.stop (ensure)");

                if (success) break;
                if (attempt < 3) delay(3000);
            }

            if (!success) {
                if (request->onFailure) request->onFailure(httpCode);
            }
            delete request;
        }
    }
};

#endif // WEBCLIENTMODULE_HPP