#include "WebClientModule.hpp"
#include "webconfig.hpp"
// removed include of certs.hpp on purpose — cert files are discovered dynamically in /certs
#include "MemoryLogger.hpp"
#include "MultiLogger.hpp"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

// MODIFIKATION: Flag zum Deaktivieren der Datenübergabe an Module (für Test der Fragmentierung)
static bool disableModuleDataAccess = false; // Setze auf true, um Module "keine Daten" denken zu lassen

// --- Helper: find cert filename for a host (try full host first, then strip subdomains) ---
// Returns a PsramString with the filename (e.g. "calendar.google.com.pem" or "google.com.pem") or empty if none found
static PsramString findCertFilenameForHost(const PsramString& hostWithOptionalPort) {
    PsramString host = hostWithOptionalPort;
    // strip port if present
    int colon = indexOf(host, ":");
    if (colon != -1) host = host.substr(0, colon);

    PsramString candidate = host;
    while (true) {
        if (candidate.length() == 0) break;
        PsramString fname = candidate + ".pem";
        PsramString path = PsramString("/certs/") + fname;
        if (LittleFS.exists(path.c_str())) {
            return fname;
        }
        int dot = indexOf(candidate, ".");
        if (dot == -1) break;
        candidate = candidate.substr(dot + 1);
    }
    return PsramString(); // not found
}

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
        Log.printf("[PsramBufferStream] WARNUNG: Pufferüberlauf! Schreibe nur die letzten %u von %u Bytes.\n", bytes_to_write, size);
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

WebClientModule::WebClientModule() : workerTaskHandle(NULL), jobQueue(NULL), _startMs(0), _lastDownloadMs(0) {}

WebClientModule::~WebClientModule() {
    if (workerTaskHandle) vTaskDelete(workerTaskHandle);
    if (_downloadBuffer) free(_downloadBuffer);
    if (jobQueue) vQueueDelete(jobQueue);
}

void WebClientModule::begin() {
    // Keep original behavior: use deviceConfig->webClientBufferSize as before
    reallocateBuffer(deviceConfig->webClientBufferSize);
    jobQueue = xQueueCreate(10, sizeof(WebJob*));
    BaseType_t app_core = xPortGetCoreID();
    BaseType_t network_core = (app_core == 0) ? 1 : 0;
    // record start time so we can delay the very first download by 10s
    _startMs = millis();
    _lastDownloadMs = 0;
    xTaskCreatePinnedToCore(webWorkerTask, "WebDataManager", 8192, this, 2, &workerTaskHandle, network_core);
}

void WebClientModule::registerResource(const String& url, uint32_t update_interval_minutes, const char* root_ca) {
    if (url.isEmpty() || update_interval_minutes == 0) return;
    
    // Extract host from URL for comparison
    String host;
    int hostStart = url.indexOf("://");
    if (hostStart != -1) hostStart += 3; else hostStart = 0;
    int pathStart = url.indexOf("/", hostStart);
    host = (pathStart != -1) ? url.substring(hostStart, pathStart) : url.substring(hostStart);
    int colonPos = host.indexOf(":");
    if (colonPos != -1) {
        host = host.substring(0, colonPos);
    }
    
    // Check if a resource with the same host already exists
    for(auto& res : resources) {
        // Extract host from existing resource URL
        PsramString existingHost;
        int existingHostStart = indexOf(res.url, "://");
        if (existingHostStart != -1) existingHostStart += 3; else existingHostStart = 0;
        int existingPathStart = indexOf(res.url, "/", existingHostStart);
        existingHost = (existingPathStart != -1) ? res.url.substr(existingHostStart, existingPathStart - existingHostStart) : res.url.substr(existingHostStart);
        int existingColonPos = indexOf(existingHost, ":");
        if (existingColonPos != -1) {
            existingHost = existingHost.substr(0, existingColonPos);
        }
        
        // If same host, update the URL but keep other parameters
        if (existingHost.compare(host.c_str()) == 0) {
            if (xSemaphoreTake(res.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                res.url = url.c_str();
                Log.printf("[WebDataManager] URL aktualisiert für Host %s: %s\n", host.c_str(), url.c_str());
                xSemaphoreGive(res.mutex);
            }
            return;
        }
    }
    
    // No existing resource with same host, check for exact URL match
    for(const auto& res : resources) {
        if (res.url.compare(url.c_str()) == 0) return;
    }

    uint32_t interval_ms = update_interval_minutes * 60 * 1000UL;
    resources.emplace_back(url.c_str(), interval_ms, root_ca);
    ManagedResource& new_res = resources.back();

    // remove the old deviceConfig-based cert selection here (we'll discover certs dynamically)
    Log.printf("[WebDataManager] Ressource registriert: %s (initial Cert-File: '%s')\n", new_res.url.c_str(), new_res.cert_filename.c_str());
}

void WebClientModule::updateResourceUrl(const String& old_url, const String& new_url) {
    for (auto& resource : resources) {
        if (resource.url.compare(old_url.c_str()) == 0) {
            if (xSemaphoreTake(resource.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                resource.url = new_url.c_str();
                Log.printf("[WebDataManager] URL für Ressource aktualisiert: %s -> %s\n", old_url.c_str(), new_url.c_str());
                xSemaphoreGive(resource.mutex);
            }
            return;
        }
    }
}

void WebClientModule::accessResource(const String& url, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback) {
    for (auto& resource : resources) {
        if (resource.url.compare(url.c_str()) == 0) {
            if (xSemaphoreTake(resource.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                // MODIFIKATION: Wenn disableModuleDataAccess true, gebe leere Daten zurück
                if (disableModuleDataAccess) {
                    callback(nullptr, 0, resource.last_successful_update, true); // "keine Daten" simulieren
                } else {
                    callback(resource.data_buffer, resource.data_size, resource.last_successful_update, resource.is_data_stale);
                }
                xSemaphoreGive(resource.mutex);
            } else {
                Log.printf("[WebDataManager] Timeout beim Warten auf Mutex für %s\n", url.c_str());
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
                Log.printf("[WebDataManager] Zertifikat für Ressource '%s' (Host: %s) live aktualisiert auf Datei: '%s'\n", resource.url.c_str(), host.c_str(), cert_filename.c_str());
                xSemaphoreGive(resource.mutex);
            }
        }
    }
}

void WebClientModule::getRequest(const PsramString& url, std::function<void(const char* buffer, size_t size)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] GET-Anfrage fehlgeschlagen: Keine WLAN-Verbindung.");
        callback(nullptr, 0);
        return;
    }
    WebJob* job = new WebJob{WebJob::GET, url, "", "", callback, nullptr};
    if (!job) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET allozieren.");
        callback(nullptr, 0);
        return;
    }
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebClientModule] FEHLER: Konnte GET-Job nicht zur Queue hinzufügen.");
        delete job;
        callback(nullptr, 0);
    }
}

void WebClientModule::getRequest(const PsramString& url, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] GET-Anfrage (detailed) fehlgeschlagen: Keine WLAN-Verbindung.");
        detailed_callback(-1, "No WiFi", 7);
        return;
    }
    WebJob* job = new WebJob{WebJob::GET, url, "", "", nullptr, detailed_callback};
    if (!job) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET (detailed) allozieren.");
        detailed_callback(-1, "Malloc failed", 12);
        return;
    }
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebDataManager] FEHLER: Konnte GET-Job (detailed) nicht zur Queue hinzufügen.");
        delete job;
        detailed_callback(-1, "Queue full", 10);
    }
}

void WebClientModule::postRequest(const PsramString& url, const PsramString& postBody, const PsramString& contentType, std::function<void(const char* buffer, size_t size)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] POST-Anfrage fehlgeschlagen: Keine WLAN-Verbindung.");
        callback(nullptr, 0);
        return;
    }

    WebJob* job = new WebJob{WebJob::POST, url, postBody, contentType, callback, nullptr};
    if (!job) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für POST allozieren.");
        callback(nullptr, 0);
        return;
    }

    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebClientModule] FEHLER: Konnte POST-Job nicht zur Queue hinzufügen.");
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
        Log.printf("[WebClientModule] Download-Puffer alloziert/vergrößert auf: %u Bytes\n", new_size);
        LOG_MEMORY_DETAILED("WebClient: Nach Puffer-Allokation");
        return true;
    } else {
        Log.printf("[WebClientModule] FEHLER: Konnte Puffer nicht auf %u Bytes vergrößern!\n", new_size);
        LOG_MEMORY_DETAILED("WebClient: Puffer-Allokation FEHLER");
        return false;
    }
}

// --- Updated performJob: stream response into _downloadStream (avoid http.getString())
//     and make connect + download two distinct steps to reduce certificate issues and heap fragmentation.
void WebClientModule::performJob(const WebJob& job) {
    LOG_MEMORY_STRATEGIC("WebClient: Begin performJob");
    Log.printf("[WebDataManager] Führe %s-Job für %s aus...\n", (job.type == WebJob::GET ? "GET" : "POST"), job.url.c_str());
    HTTPClient http;
    int httpCode = 0;
    
    WiFiClientSecure secure_client;
    WiFiClient plain_client;

    bool using_https = (job.url.rfind("https://", 0) == 0);
    PsramString cert_data;
    bool cert_loaded = false;
    PsramString cert_filename;

    // Parse host and port (works for both http and https)
    int hostStart = indexOf(job.url, "://");
    if (hostStart != -1) hostStart += 3; else hostStart = 0;
    int pathStart = indexOf(job.url, "/", hostStart);
    PsramString host = (pathStart != -1) ? job.url.substr(hostStart, pathStart - hostStart) : job.url.substr(hostStart);

    uint16_t port = using_https ? 443 : 80;
    // if host contains ":port", try to parse (simple)
    int colonPos = indexOf(host, ":");
    if (colonPos != -1) {
        PsramString portStr = host.substr(colonPos + 1);
        port = (uint16_t)atoi(portStr.c_str());
        host = host.substr(0, colonPos);
    }

    // determine cert filename by searching cert files in /certs (full host first, then progressively remove subdomains)
    if (using_https) {
        PsramString found = findCertFilenameForHost(host);
        if (!found.empty()) {
            cert_filename = found;
            PsramString filepath = PsramString("/certs/") + found;
            if (LittleFS.exists(filepath.c_str())) {
                File certFile = LittleFS.open(filepath.c_str(), "r");
                if (certFile) {
                    cert_data = readFromStream(certFile);
                    certFile.close();
                    cert_loaded = true;
                }
            }
        }

        // fallback: if resource already has cert_filename configured, try it
        if (!cert_loaded && !cert_filename.empty()) {
            PsramString filepath = PsramString("/certs/") + cert_filename;
            if (LittleFS.exists(filepath.c_str())) {
                File certFile = LittleFS.open(filepath.c_str(), "r");
                if (certFile) {
                    cert_data = readFromStream(certFile);
                    certFile.close();
                    cert_loaded = true;
                }
            }
        }

        // fallback to resource.root_ca_fallback or insecure
        if (!cert_loaded && job.detailed_callback == nullptr) {
            // nothing special here — keep previous fallback behaviour but prefer resource.root_ca_fallback
        }

        if(cert_loaded) {
            Log.printf("[WebDataManager] Job: Verwende Zertifikat '%s'\n", cert_filename.c_str());
            secure_client.setCACert(cert_data.c_str());
        } else if (job.detailed_callback == nullptr) {
            // if no cert loaded, try resource.root_ca_fallback (if any)
            // Note: performJob does not have access to ManagedResource here; rely on local behavior: if caller wants to set cert per resource, they can use updateResourceCertificateByHost()
            Log.printf("[WebDataManager] Job: WARNUNG, kein Zertifikat für %s gefunden, benutze unsichere Verbindung.\n", job.url.c_str());
            secure_client.setInsecure();
        } else {
            secure_client.setInsecure();
        }
    }

    // Step 1: explicit connect using client
    bool connected = false;
    if (using_https) {
        connected = secure_client.connect(host.c_str(), port);
        if (!connected) {
            httpCode = -1; // connect failed
        }
    } else {
        connected = plain_client.connect(host.c_str(), port);
        if (!connected) {
            httpCode = -1;
        }
    }

    // If connected, start HTTP and perform request
    if (connected) {
        if (using_https) {
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
    }

    // Now stream result into download buffer (avoid http.getString())
    _downloadStream.begin(_downloadBuffer, _bufferCapacity);

    if (httpCode == HTTP_CODE_OK) {
        LOG_MEMORY_DETAILED("WebClient: Vor http.writeToStream");
        http.writeToStream(&_downloadStream);
        LOG_MEMORY_DETAILED("WebClient: Nach http.writeToStream");
        if (_downloadStream.hasOverflowed()) {
            Log.printf("[WebDataManager] LERNEN: Pufferüberlauf bei Job %s. Puffergröße=%u. Job liefert mehr Daten als erwartet.\n", job.url.c_str(), (unsigned)_downloadStream.getCapacity());
            // Try to notify callbacks about failure / overflow
            if (job.detailed_callback) {
                job.detailed_callback(-2, "Buffer overflow", strlen("Buffer overflow"));
            } else if (job.callback) {
                job.callback(nullptr, 0);
            }
        } else {
            size_t downloaded_size = _downloadStream.getSize();
            if (downloaded_size > 0) {
                // allocate temporary buffer to hand to callback (synchronous only)
                LOG_MEMORY_DETAILED("WebClient: Vor tmp_buf ps_malloc");
                char* tmp_buf = (char*)ps_malloc(downloaded_size + 1);
                LOG_MEMORY_DETAILED("WebClient: Nach tmp_buf ps_malloc");
                if (tmp_buf) {
                    memcpy(tmp_buf, _downloadStream.getBuffer(), downloaded_size);
                    tmp_buf[downloaded_size] = '\0';

                    if (job.detailed_callback) {
                        job.detailed_callback((int)httpCode, tmp_buf, downloaded_size);
                    } else if (job.callback) {
                        job.callback(tmp_buf, downloaded_size);
                    }

                    LOG_MEMORY_DETAILED("WebClient: Vor tmp_buf free");
                    free(tmp_buf);
                    LOG_MEMORY_DETAILED("WebClient: Nach tmp_buf free");
                } else {
                    Log.printf("[WebDataManager] FEHLER: Konnte temporären Buffer (%u) für Job nicht allozieren.\n", (unsigned)downloaded_size);
                    if (job.detailed_callback) {
                        job.detailed_callback(-3, "Malloc failed", strlen("Malloc failed"));
                    } else if (job.callback) {
                        job.callback(nullptr, 0);
                    }
                }
            } else {
                // empty body but HTTP_OK - still call callback with zero size
                if (job.detailed_callback) job.detailed_callback((int)httpCode, "", 0);
                else if (job.callback) job.callback("", 0);
            }
        }
    } else {
        // error path - prepare small error description
        char errbuf[256];
        if (httpCode == -1 && using_https) {
            secure_client.lastError(errbuf, sizeof(errbuf));
            size_t l = strnlen(errbuf, sizeof(errbuf));
            if (job.detailed_callback) job.detailed_callback(httpCode, errbuf, l);
            else if (job.callback) job.callback(nullptr, 0);
        } else {
            // http.errorToString may allocate a String internally; use it but keep short-lived
            String err = http.errorToString(httpCode);
            size_t l = err.length();
            if (l >= sizeof(errbuf)) l = sizeof(errbuf) - 1;
            memcpy(errbuf, err.c_str(), l);
            errbuf[l] = '\0';
            if (job.detailed_callback) job.detailed_callback(httpCode, errbuf, l);
            else if (job.callback) job.callback(nullptr, 0);
        }
    }

    http.end();
    // free cert_data (if any) as early as possible (will be freed on scope exit anyway)
    cert_data.clear();
    LOG_MEMORY_STRATEGIC("WebClient: End performJob");
}

void WebClientModule::webWorkerTask(void* param) {
    WebClientModule* self = static_cast<WebClientModule*>(param);
    Log.printf("[WebDataManager] Worker-Task gestartet auf Core %d.\n", xPortGetCoreID());
    
    WebJob* receivedJob;
    const unsigned long MIN_PAUSE_BETWEEN_DOWNLOADS_MS = 10000UL; // 10 seconds

    while (true) {
        unsigned long nowMs = millis();

        // enforce initial start delay of 10 seconds before any download happens
        if (nowMs - self->_startMs < MIN_PAUSE_BETWEEN_DOWNLOADS_MS) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (WiFi.status() == WL_CONNECTED) {
            // Process queued jobs, but ensure minimum pause between downloads
            if (xQueueReceive(self->jobQueue, &receivedJob, 0) == pdTRUE) {
                // If last download happened too recently, push job back to front and wait
                if (self->_lastDownloadMs != 0 && (nowMs - self->_lastDownloadMs) < MIN_PAUSE_BETWEEN_DOWNLOADS_MS) {
                    // push back to queue front (so it will be retried)
                    if (xQueueSendToFront(self->jobQueue, &receivedJob, pdMS_TO_TICKS(100)) != pdTRUE) {
                        // if failed to put back, drop it gracefully after short wait
                        Log.println("[WebDataManager] WARNUNG: Konnte Job nicht zurück in Queue einreihen, verwerfe.");
                        delete receivedJob;
                    }
                    vTaskDelay(pdMS_TO_TICKS(200));
                } else {
                    // mark last download time and perform job
                    self->_lastDownloadMs = millis();
                    self->performJob(*receivedJob);
                    delete receivedJob;
                }
            }

            // Periodic resource updates
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
                    // Respect global minimum pause between downloads
                    unsigned long nowMsInner = millis();
                    if (self->_lastDownloadMs != 0 && (nowMsInner - self->_lastDownloadMs) < MIN_PAUSE_BETWEEN_DOWNLOADS_MS) {
                        // skip this resource for now; next loop will retry
                        continue;
                    }

                    // All good: perform update and record last download time
                    self->_lastDownloadMs = millis();
                    self->performUpdate(resource);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// performUpdate unchanged except cert discovery replaces previous deviceConfig cert-field usage
void WebClientModule::performUpdate(ManagedResource& resource) {
    LOG_MEMORY_STRATEGIC("WebClient: Begin performUpdate");
    Log.printf("[WebDataManager] Starte Update für %s...\n", resource.url.c_str());
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
            PsramString filepath;

            // determine host
            PsramString host;
            uint16_t port = 443;
            int hostStart = indexOf(resource.url, "://") + 3;
            int pathStart = indexOf(resource.url, "/", hostStart);
            host = (pathStart != -1) ? resource.url.substr(hostStart, pathStart - hostStart) : resource.url.substr(hostStart);
            int colonPos = indexOf(host, ":");
            if (colonPos != -1) {
                PsramString portStr = host.substr(colonPos + 1);
                port = (uint16_t)atoi(portStr.c_str());
                host = host.substr(0, colonPos);
            }

            // first try to find cert by host names in /certs
            PsramString found = findCertFilenameForHost(host);
            if (!found.empty()) {
                filepath = PsramString("/certs/") + found;
                if (LittleFS.exists(filepath.c_str())) {
                    File certFile = LittleFS.open(filepath.c_str(), "r");
                    if (certFile) {
                        cert_data = readFromStream(certFile);
                        certFile.close();
                        cert_loaded = true;
                        resource.cert_filename = found; // record which file was used
                    }
                }
            }

            // fallback: if resource already has cert_filename configured, try it
            if (!cert_loaded && !resource.cert_filename.empty()) {
                filepath = PsramString("/certs/") + resource.cert_filename;
                if (LittleFS.exists(filepath.c_str())) {
                    File certFile = LittleFS.open(filepath.c_str(), "r");
                    if (certFile) {
                        cert_data = readFromStream(certFile);
                        certFile.close();
                        cert_loaded = true;
                    }
                }
            }

            // fallback to root_ca_fallback in resource
            if (!cert_loaded && resource.root_ca_fallback) {
                Log.printf("[WebDataManager] Verwende Fallback-Zertifikat für %s.\n", resource.url.c_str());
                secure_client.setCACert(resource.root_ca_fallback);
            } else if (!cert_loaded) {
                Log.printf("[WebDataManager] WARNUNG: Kein Zertifikat gefunden. Verwende unsichere Verbindung für %s.\n", resource.url.c_str());
                secure_client.setInsecure();
            } else {
                Log.printf("[WebDataManager] Verwende Zertifikat aus Datei '%s' für %s.\n", filepath.c_str(), resource.url.c_str());
                secure_client.setCACert(cert_data.c_str());
            }
            
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
            LOG_MEMORY_DETAILED("WebClient: Vor http.writeToStream in performUpdate");
            http.writeToStream(&_downloadStream);
            LOG_MEMORY_DETAILED("WebClient: Nach http.writeToStream in performUpdate");
            if (_downloadStream.hasOverflowed()) {
                Log.printf("[WebClientModule] LERNEN: Pufferüberlauf bei %s. Puffer wird vergrößert.\n", resource.url.c_str());
                size_t new_capacity = ((_downloadStream.getCapacity() / (128 * 1024)) + 1) * (128 * 1024);
                if (reallocateBuffer(new_capacity)) {
                    deviceConfig->webClientBufferSize = new_capacity;
                    saveDeviceConfig();
                    retry_with_larger_buffer = true;
                } else {
                    Log.println("[WebClientModule] FEHLER: Puffer konnte nicht vergrößert werden. Update für diese Ressource abgebrochen.");
                }
            } else {
                size_t downloaded_size = _downloadStream.getSize();
                if (downloaded_size > 0) {
                    LOG_MEMORY_DETAILED("WebClient: Vor permanent ps_malloc in performUpdate");
                    char* new_permanent_buffer = (char*)ps_malloc(downloaded_size + 1);
                    LOG_MEMORY_DETAILED("WebClient: Nach permanent ps_malloc in performUpdate");
                    if (new_permanent_buffer) {
                        memcpy(new_permanent_buffer, _downloadStream.getBuffer(), downloaded_size);
                        new_permanent_buffer[downloaded_size] = '\0';
                        if (xSemaphoreTake(resource.mutex, portMAX_DELAY) == pdTRUE) {
                            if (resource.data_buffer) {
                                LOG_MEMORY_DETAILED("WebClient: Vor free old data_buffer");
                                free(resource.data_buffer);
                                LOG_MEMORY_DETAILED("WebClient: Nach free old data_buffer");
                            }
                            resource.data_buffer = new_permanent_buffer;
                            resource.data_size = downloaded_size;
                            time(&resource.last_successful_update);
                            resource.is_data_stale = false;
                            xSemaphoreGive(resource.mutex);
                            Log.printf("[WebDataManager] ERFOLG: %s aktualisiert (%u Bytes).\n", resource.url.c_str(), (unsigned int)downloaded_size);
                        } else {
                            LOG_MEMORY_DETAILED("WebClient: Vor free new_permanent_buffer (failed mutex)");
                            free(new_permanent_buffer);
                            LOG_MEMORY_DETAILED("WebClient: Nach free new_permanent_buffer (failed mutex)");
                        }
                    } else {
                        Log.printf("[WebClientModule] FEHLER: Konnte keinen passgenauen Puffer für %s allozieren.\n", resource.url.c_str());
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
                Log.printf("[WebDataManager] FEHLER bei %s: %s. Max. Retries (%d) erreicht.\n", resource.url.c_str(), errorMsg.c_str(), resource.retry_count);
                resource.retry_count = 0;
                resource.is_in_retry_mode = false;
            } else {
                Log.printf("[WebDataManager] FEHLER bei %s: %s. Versuch %d/3 in 30s.\n", resource.url.c_str(), errorMsg.c_str(), resource.retry_count);
                resource.is_in_retry_mode = true;
            }
        }
        http.end();
        max_growth_retries--;
    } while (retry_with_larger_buffer && max_growth_retries > 0);
    LOG_MEMORY_STRATEGIC("WebClient: End performUpdate");
}