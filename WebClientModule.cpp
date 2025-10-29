#include "WebClientModule.hpp"
#include "webconfig.hpp"
#include "certs.hpp"
#include "MemoryLogger.hpp"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

// --- PsramBufferStream Implementierung ---

PsramBufferStream::PsramBufferStream() = default;

void PsramBufferStream::begin(char* buffer, size_t capacity) {
    _buffer = buffer;
    _capacity = capacity;
    reset();
}

void PsramBufferStream::reset() {
    _position = 0;
    _overflowed = false;
}

size_t PsramBufferStream::write(uint8_t data) {
    if (_overflowed || !_buffer) return 0;
    if (_position >= _capacity) {
        _overflowed = true;
        return 0;
    }
    _buffer[_position++] = data;
    return 1;
}

size_t PsramBufferStream::write(const uint8_t *buffer, size_t size) {
    if (_overflowed || !_buffer) return 0;
    size_t bytes_to_write = size;
    if (_position + size > _capacity) {
        _overflowed = true;
        bytes_to_write = (_capacity > _position) ? _capacity - _position : 0;
        Serial.printf("[PsramBufferStream] WARNUNG: Pufferüberlauf! Schreibe nur die letzten %u von %u Bytes.\n", bytes_to_write, size);
    }
    if (bytes_to_write > 0) {
        const size_t chunkSize = 4096;
        size_t bytesCopied = 0;
        while(bytesCopied < bytes_to_write) {
            size_t currentChunkSize = bytes_to_write - bytesCopied;
            if (currentChunkSize > chunkSize) currentChunkSize = chunkSize;
            memcpy(_buffer + _position + bytesCopied, buffer + bytesCopied, currentChunkSize);
            bytesCopied += currentChunkSize;
            vTaskDelay(1); 
        }
        _position += bytes_to_write;
    }
    return bytes_to_write;
}

bool PsramBufferStream::hasOverflowed() const { return _overflowed; }
size_t PsramBufferStream::getCapacity() const { return _capacity; }
int PsramBufferStream::available() { return 0; }
int PsramBufferStream::read() { return -1; }
int PsramBufferStream::peek() { return -1; }
void PsramBufferStream::flush() {}
char* PsramBufferStream::getBuffer() { return _buffer; }
size_t PsramBufferStream::getSize() { return _position; }


// --- ManagedResource Implementierung ---

ManagedResource::ManagedResource(const PsramString& u, uint32_t interval, const char* ca)
    : url(u), update_interval_ms(interval), root_ca_fallback(ca) {
    mutex = xSemaphoreCreateMutex();
}

ManagedResource::~ManagedResource() {
    if (data_buffer) free(data_buffer);
    if (mutex) vSemaphoreDelete(mutex);
}

ManagedResource::ManagedResource(ManagedResource&& other) noexcept
    : url(std::move(other.url)), update_interval_ms(other.update_interval_ms), root_ca_fallback(other.root_ca_fallback),
      cert_filename(std::move(other.cert_filename)), data_buffer(other.data_buffer), data_size(other.data_size), 
      last_successful_update(other.last_successful_update), last_check_attempt(other.last_check_attempt), 
      mutex(other.mutex), retry_count(other.retry_count), is_in_retry_mode(other.is_in_retry_mode), is_data_stale(other.is_data_stale)
{
    other.data_buffer = nullptr; other.mutex = nullptr;
}


// --- WebClientModule Implementierung ---

WebClientModule::WebClientModule() : workerTaskHandle(NULL), jobQueue(NULL) {}

WebClientModule::~WebClientModule() {
    if (workerTaskHandle) vTaskDelete(workerTaskHandle);
    if (_downloadBuffer) free(_downloadBuffer);
    if (jobQueue) vQueueDelete(jobQueue);
}

void WebClientModule::begin() {
    reallocateBuffer(deviceConfig->webClientBufferSize);
    jobQueue = xQueueCreate(10, sizeof(WebJob*));
    BaseType_t app_core = xPortGetCoreID();
    BaseType_t network_core = (app_core == 0) ? 1 : 0;
    xTaskCreatePinnedToCore(webWorkerTask, "WebDataManager", 8192, this, 2, &workerTaskHandle, network_core);
}

void WebClientModule::registerResource(const String& url, uint32_t update_interval_minutes, const char* root_ca) {
    if (url.isEmpty() || update_interval_minutes == 0) return;
    
    for(const auto& res : resources) {
        if (res.url.compare(url.c_str()) == 0) return;
    }

    uint32_t interval_ms = update_interval_minutes * 60 * 1000UL;
    resources.emplace_back(url.c_str(), interval_ms, root_ca);
    ManagedResource& new_res = resources.back();

    if (indexOf(new_res.url, "dartsrankings.com") != -1) {
        new_res.cert_filename = deviceConfig->dartsCertFile;
    } else if (indexOf(new_res.url, "creativecommons.tankerkoenig.de") != -1) {
        new_res.cert_filename = deviceConfig->tankerkoenigCertFile;
    } else if (indexOf(new_res.url, "google.com") != -1) {
        new_res.cert_filename = deviceConfig->googleCertFile;
    }

    Serial.printf("[WebDataManager] Ressource registriert: %s (Cert-File: '%s')\n", new_res.url.c_str(), new_res.cert_filename.c_str());
}

void WebClientModule::accessResource(const String& url, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback) {
    for (auto& resource : resources) {
        if (resource.url.compare(url.c_str()) == 0) {
            if (xSemaphoreTake(resource.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                callback(resource.data_buffer, resource.data_size, resource.last_successful_update, resource.is_data_stale);
                xSemaphoreGive(resource.mutex);
            } else {
                Serial.printf("[WebDataManager] Timeout beim Warten auf Mutex für %s\n", url.c_str());
            }
            return;
        }
    }
}

void WebClientModule::updateResourceCertificateByHost(const String& host, const String& cert_filename) {
    for (auto& resource : resources) {
        if (indexOf(resource.url, host.c_str()) != -1) {
            if (xSemaphoreTake(resource.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                resource.cert_filename = cert_filename.c_str();
                Serial.printf("[WebDataManager] Zertifikat für Ressource '%s' (Host: %s) live aktualisiert auf Datei: '%s'\n", resource.url.c_str(), host.c_str(), cert_filename.c_str());
                xSemaphoreGive(resource.mutex);
            }
        }
    }
}

void WebClientModule::getRequest(const PsramString& url, std::function<void(const char* buffer, size_t size)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WebClientModule] GET-Anfrage fehlgeschlagen: Keine WLAN-Verbindung.");
        callback(nullptr, 0);
        return;
    }
    WebJob* job = new WebJob{WebJob::GET, url, "", "", callback, nullptr};
    if (!job) {
        Serial.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET allozieren.");
        callback(nullptr, 0);
        return;
    }
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[WebClientModule] FEHLER: Konnte GET-Job nicht zur Queue hinzufügen.");
        delete job;
        callback(nullptr, 0);
    }
}

void WebClientModule::getRequest(const PsramString& url, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WebClientModule] GET-Anfrage (detailed) fehlgeschlagen: Keine WLAN-Verbindung.");
        detailed_callback(-1, "No WiFi", 7);
        return;
    }
    WebJob* job = new WebJob{WebJob::GET, url, "", "", nullptr, detailed_callback};
    if (!job) {
        Serial.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET (detailed) allozieren.");
        detailed_callback(-1, "Malloc failed", 12);
        return;
    }
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[WebClientModule] FEHLER: Konnte GET-Job (detailed) nicht zur Queue hinzufügen.");
        delete job;
        detailed_callback(-1, "Queue full", 10);
    }
}

void WebClientModule::postRequest(const PsramString& url, const PsramString& postBody, const PsramString& contentType, std::function<void(const char* buffer, size_t size)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WebClientModule] POST-Anfrage fehlgeschlagen: Keine WLAN-Verbindung.");
        callback(nullptr, 0);
        return;
    }

    WebJob* job = new WebJob{WebJob::POST, url, postBody, contentType, callback, nullptr};
    if (!job) {
        Serial.println("[WebClientModule] FEHLER: Konnte keinen WebJob für POST allozieren.");
        callback(nullptr, 0);
        return;
    }

    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[WebClientModule] FEHLER: Konnte POST-Job nicht zur Queue hinzufügen.");
        delete job;
        callback(nullptr, 0);
    }
}

bool WebClientModule::reallocateBuffer(size_t new_size) {
    if (new_size <= _bufferCapacity) return true;
    LOG_MEMORY_DETAILED("WebClient: Vor Puffer-Allokation");
    char* new_buffer = (char*)ps_realloc(_downloadBuffer, new_size);
    if (new_buffer) {
        _downloadBuffer = new_buffer;
        _bufferCapacity = new_size;
        Serial.printf("[WebClientModule] Download-Puffer alloziert/vergrößert auf: %u Bytes\n", new_size);
        LOG_MEMORY_DETAILED("WebClient: Nach Puffer-Allokation");
        return true;
    } else {
        Serial.printf("[WebClientModule] FEHLER: Konnte Puffer nicht auf %u Bytes vergrößern!\n", new_size);
        LOG_MEMORY_DETAILED("WebClient: Puffer-Allokation FEHLER");
        return false;
    }
}

void WebClientModule::performJob(const WebJob& job) {
    Serial.printf("[WebDataManager] Führe %s-Job für %s aus...\n", (job.type == WebJob::GET ? "GET" : "POST"), job.url.c_str());
    HTTPClient http;
    int httpCode = 0;
    
    WiFiClientSecure secure_client;
    WiFiClient plain_client;

    if (job.url.rfind("https://", 0) == 0) {
        PsramString cert_data;
        bool cert_loaded = false;
        
        PsramString cert_filename;
        if (indexOf(job.url, "creativecommons.tankerkoenig.de") != -1) {
            cert_filename = deviceConfig->tankerkoenigCertFile;
        } else if (indexOf(job.url, "dartsrankings.com") != -1) {
            cert_filename = deviceConfig->dartsCertFile;
        } else if (indexOf(job.url, "google.com") != -1) {
            cert_filename = deviceConfig->googleCertFile;
        }

        if (!cert_filename.empty()) {
            String filepath = "/certs/" + String(cert_filename.c_str());
            if (LittleFS.exists(filepath)) {
                File certFile = LittleFS.open(filepath, "r");
                if (certFile) {
                    cert_data = readFromStream(certFile);
                    certFile.close();
                    cert_loaded = true;
                }
            }
        }

        if(cert_loaded) {
            Serial.printf("[WebDataManager] Job: Verwende Zertifikat '%s'\n", cert_filename.c_str());
            secure_client.setCACert(cert_data.c_str());
        } else {
            Serial.printf("[WebDataManager] Job: WARNUNG, kein Zertifikat für %s gefunden, benutze unsichere Verbindung.\n", job.url.c_str());
            secure_client.setInsecure();
        }

        PsramString host;
        uint16_t port = 443;
        int hostStart = indexOf(job.url, "://") + 3;
        int pathStart = indexOf(job.url, "/", hostStart);
        host = (pathStart != -1) ? job.url.substr(hostStart, pathStart - hostStart) : job.url.substr(hostStart);
        
        if (!secure_client.connect(host.c_str(), port)) {
            httpCode = -1;
        } else {
            if (http.begin(secure_client, job.url.c_str())) {
                if (job.type == WebJob::GET) {
                    httpCode = http.GET();
                } else {
                    http.addHeader("Content-Type", job.contentType.c_str());
                    httpCode = http.POST(job.body.c_str());
                }
            } else {
                httpCode = -10;
            }
        }
    } else { 
        if (http.begin(plain_client, job.url.c_str())) {
            if (job.type == WebJob::GET) {
                httpCode = http.GET();
            } else {
                http.addHeader("Content-Type", job.contentType.c_str());
                httpCode = http.POST(job.body.c_str());
            }
        } else {
            httpCode = -10;
        }
    }

    String payload;
    if(httpCode > 0) {
        payload = http.getString();
    } else {
        payload = http.errorToString(httpCode).c_str();
    }
    
    Serial.printf("[WebDataManager] DEBUG: HTTP-Code: %d\n", httpCode);
    Serial.println("[WebDataManager] DEBUG: Empfangener Payload (erste 100 Zeichen):");
    Serial.println("--------------------------------------------------");
    Serial.println(payload.substring(0, 100));
    Serial.println("--------------------------------------------------");

    http.end();
    
    if (job.detailed_callback) {
        job.detailed_callback(httpCode, payload.c_str(), payload.length());
    } else if (job.callback) {
        if (httpCode == HTTP_CODE_OK) {
            job.callback(payload.c_str(), payload.length());
        } else {
            Serial.printf("[WebDataManager] %s-Job FEHLER: HTTP %d\n", (job.type == WebJob::GET ? "GET" : "POST"), httpCode);
            job.callback(nullptr, 0);
        }
    }
}

void WebClientModule::webWorkerTask(void* param) {
    WebClientModule* self = static_cast<WebClientModule*>(param);
    Serial.printf("[WebDataManager] Worker-Task gestartet auf Core %d.\n", xPortGetCoreID());
    
    WebJob* receivedJob;
    while (true) {
        if (WiFi.status() == WL_CONNECTED) {
            if (xQueueReceive(self->jobQueue, &receivedJob, 0) == pdTRUE) {
                self->performJob(*receivedJob);
                delete receivedJob;
            }

            time_t now;
            time(&now);
            for (auto& resource : self->resources) {
                bool should_run = false;
                if (resource.is_in_retry_mode) {
                    if ((now - resource.last_check_attempt) * 1000UL >= 30000UL) {
                        should_run = true;
                    }
                } else {
                    if (resource.last_check_attempt == 0 || (now - resource.last_check_attempt) * 1000UL >= resource.update_interval_ms) {
                        should_run = true;
                    }
                }

                if (should_run) {
                    self->performUpdate(resource);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void WebClientModule::performUpdate(ManagedResource& resource) {
    Serial.printf("[WebDataManager] Starte Update für %s...\n", resource.url.c_str());
    resource.last_check_attempt = time(nullptr);
    bool retry_with_larger_buffer;
    int max_growth_retries = 3;

    do {
        retry_with_larger_buffer = false;
        HTTPClient http;
        
        int httpCode = 0;
        WiFiClientSecure secure_client;
        WiFiClient plain_client;

        if (resource.url.rfind("https://", 0) == 0) {
            PsramString cert_data;
            bool cert_loaded = false;
            String filepath;
            if (!resource.cert_filename.empty()) {
                filepath = "/certs/" + String(resource.cert_filename.c_str());
                if (LittleFS.exists(filepath)) {
                    File certFile = LittleFS.open(filepath, "r");
                    if (certFile) {
                        cert_data = readFromStream(certFile);
                        certFile.close();
                        cert_loaded = true;
                    }
                }
            }

            if (cert_loaded) {
                Serial.printf("[WebDataManager] Verwende Zertifikat aus Datei '%s' für %s.\n", filepath.c_str(), resource.url.c_str());
                secure_client.setCACert(cert_data.c_str());
            } else if (resource.root_ca_fallback) {
                Serial.printf("[WebDataManager] Verwende Fallback-Zertifikat für %s.\n", resource.url.c_str());
                secure_client.setCACert(resource.root_ca_fallback);
            } else {
                Serial.printf("[WebDataManager] WARNUNG: Kein Zertifikat gefunden. Verwende unsichere Verbindung für %s.\n", resource.url.c_str());
                secure_client.setInsecure();
            }
            
            PsramString host;
            uint16_t port = 443;
            int hostStart = indexOf(resource.url, "://") + 3;
            int pathStart = indexOf(resource.url, "/", hostStart);
            host = (pathStart != -1) ? resource.url.substr(hostStart, pathStart - hostStart) : resource.url.substr(hostStart);
            
            if (!secure_client.connect(host.c_str(), port)) {
                httpCode = -1;
            } else {
                if (http.begin(secure_client, resource.url.c_str())) {
                     http.setTimeout(15000);
                     httpCode = http.GET();
                } else {
                    httpCode = -10;
                }
            }
        } else {
            if (http.begin(plain_client, resource.url.c_str())) {
                http.setTimeout(15000);
                httpCode = http.GET();
            } else {
                httpCode = -10;
            }
        }

        _downloadStream.begin(_downloadBuffer, _bufferCapacity);

        if (httpCode == HTTP_CODE_OK) {
            http.writeToStream(&_downloadStream);
            if (_downloadStream.hasOverflowed()) {
                Serial.printf("[WebClientModule] LERNEN: Pufferüberlauf bei %s. Puffer wird vergrößert.\n", resource.url.c_str());
                size_t new_capacity = ((_downloadStream.getCapacity() / (128 * 1024)) + 1) * (128 * 1024);
                if (reallocateBuffer(new_capacity)) {
                    deviceConfig->webClientBufferSize = new_capacity;
                    saveDeviceConfig();
                    retry_with_larger_buffer = true;
                } else {
                    Serial.println("[WebClientModule] FEHLER: Puffer konnte nicht vergrößert werden. Update für diese Ressource abgebrochen.");
                }
            } else {
                size_t downloaded_size = _downloadStream.getSize();
                if (downloaded_size > 0) {
                    char* new_permanent_buffer = (char*)ps_malloc(downloaded_size + 1);
                    if (new_permanent_buffer) {
                        memcpy(new_permanent_buffer, _downloadStream.getBuffer(), downloaded_size);
                        new_permanent_buffer[downloaded_size] = '\0';
                        if (xSemaphoreTake(resource.mutex, portMAX_DELAY) == pdTRUE) {
                            if (resource.data_buffer) free(resource.data_buffer);
                            resource.data_buffer = new_permanent_buffer;
                            resource.data_size = downloaded_size;
                            time(&resource.last_successful_update);
                            resource.is_data_stale = false;
                            xSemaphoreGive(resource.mutex);
                            Serial.printf("[WebDataManager] ERFOLG: %s aktualisiert (%u Bytes).\n", resource.url.c_str(), (unsigned int)downloaded_size);
                        } else {
                            free(new_permanent_buffer);
                        }
                    } else {
                        Serial.printf("[WebDataManager] FEHLER: Konnte keinen passgenauen Puffer für %s allozieren.\n", resource.url.c_str());
                    }
                }
                resource.retry_count = 0;
                resource.is_in_retry_mode = false;
            }
        } else {
            resource.retry_count++;
            resource.is_data_stale = true;
            
            String errorMsg;
            if (httpCode > 0) {
                errorMsg = "HTTP-Code " + String(httpCode);
            } else if (httpCode == -1) {
                char error_buf[128];
                secure_client.lastError(error_buf, sizeof(error_buf));
                errorMsg = "Connect-Fehler: " + String(error_buf);
            }
            else {
                errorMsg = http.errorToString(httpCode);
            }

            if (resource.retry_count >= 3) {
                Serial.printf("[WebDataManager] FEHLER bei %s: %s. Max. Retries (%d) erreicht.\n", resource.url.c_str(), errorMsg.c_str(), resource.retry_count);
                resource.retry_count = 0;
                resource.is_in_retry_mode = false;
            } else {
                Serial.printf("[WebDataManager] FEHLER bei %s: %s. Versuch %d/3 in 30s.\n", resource.url.c_str(), errorMsg.c_str(), resource.retry_count);
                resource.is_in_retry_mode = true;
            }
        }
        http.end();
        max_growth_retries--;
    } while (retry_with_larger_buffer && max_growth_retries > 0);
}