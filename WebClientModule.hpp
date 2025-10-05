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

// Ein einfacher Stream, der in einen dynamischen PSRAM-Puffer schreibt
class PsramBufferStream : public Stream {
public:
    PsramBufferStream() {
        _buffer = (char*)ps_malloc(INITIAL_SIZE);
        if (_buffer) {
            _capacity = INITIAL_SIZE;
        }
    }
    ~PsramBufferStream() {
        if (_buffer) {
            free(_buffer);
        }
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
        for (size_t i = 0; i < size; i++) {
            if (write(buffer[i]) == 0) {
                return i;
            }
        }
        return size;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    char* getBuffer() { return _buffer; }
    size_t getSize() { return _position; }

private:
    char* _buffer = nullptr;
    size_t _capacity = 0;
    size_t _position = 0;
    static const size_t INITIAL_SIZE = 64 * 1024;
    static const size_t MAX_SIZE = 2 * 1024 * 1024;

    bool grow() {
        if (_capacity >= MAX_SIZE) return false;
        size_t new_capacity = std::min(_capacity * 2, MAX_SIZE);
        char* new_buffer = (char*)ps_realloc(_buffer, new_capacity);
        if (!new_buffer) {
            Serial.println("[PsramBufferStream] FEHLER: ps_realloc fehlgeschlagen!");
            return false;
        }
        _buffer = new_buffer;
        _capacity = new_capacity;
        Serial.printf("[PsramBufferStream] Puffer auf %u Bytes vergrößert.\n", _capacity);
        return true;
    }
};


struct WebRequest {
    String url;
    std::function<void(char*, size_t)> onSuccess; 
    std::function<void(int)> onFailure;
};

class WebClientModule {
public:
    WebClientModule() {
        requestQueue = xQueueCreate(10, sizeof(WebRequest*));
        workerTaskHandle = NULL;
    }

    void begin() {
        xTaskCreatePinnedToCore(
            webWorkerTask, "WebWorker", 8192, this, 2, &workerTaskHandle, 1);
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

    static void webWorkerTask(void* param) {
        WebClientModule* self = static_cast<WebClientModule*>(param);
        while (true) {
            WebRequest* request;
            if (xQueueReceive(self->requestQueue, &request, portMAX_DELAY)) {
                if (WiFi.status() != WL_CONNECTED) {
                    Serial.printf("[WebWorker] Keine WLAN-Verbindung für: %s\n", request->url.c_str());
                    if (request->onFailure) request->onFailure(-7);
                    delete request;
                    continue;
                }
                
                int httpCode = 0;
                bool success = false;
                
                Serial.printf("[WebWorker] Rufe URL auf: %s\n", request->url.c_str());

                for (int attempt = 1; attempt <= 3; ++attempt) {
                    Serial.printf("[WebWorker] Versuch %d/3...\n", attempt);

                    HTTPClient http;
                    WiFiClientSecure client;
                    client.setInsecure();

                    http.begin(client, request->url);
                    http.setReuse(true); 
                    
                    httpCode = http.GET();

                    if (httpCode == HTTP_CODE_OK) {
                        if (!psramFound()) {
                             Serial.println("[WebWorker] FEHLER: Kein PSRAM gefunden!");
                             httpCode = -11;
                             http.end();
                             break;
                        }

                        PsramBufferStream psramStream;
                        size_t written = http.writeToStream(&psramStream);

                        if (written > 0) {
                            Serial.printf("[WebWorker] Download beendet. %u Bytes in Stream geschrieben.\n", written);
                            char* buffer = psramStream.getBuffer();
                            size_t size = psramStream.getSize();
                            buffer[size] = '\0';

                            if (request->onSuccess) {
                                request->onSuccess(buffer, size);
                            }
                            success = true;
                        } else {
                            Serial.println("[WebWorker] FEHLER: Konnte keine Daten in Stream schreiben.");
                            httpCode = -20;
                        }
                        
                    } else {
                        Serial.printf("[WebWorker] Versuch %d fehlgeschlagen, HTTP-Code: %d\n", attempt, httpCode);
                    }

                    http.end();

                    if (success) {
                        break; 
                    }

                    if (attempt < 3) {
                        delay(3000); 
                    }
                }

                if (!success) {
                    if (request->onFailure) {
                        request->onFailure(httpCode); 
                    }
                }
                delete request;
            }
        }
    }
};

#endif