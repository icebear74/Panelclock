#include "WebClientModule.hpp"
#include "webconfig.hpp"
// removed include of certs.hpp on purpose — cert files are discovered dynamically in /certs
#include "MemoryLogger.hpp"
#include "MultiLogger.hpp"
#include "FragmentationMonitor.hpp"
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
    : url(u), customHeaders(""), update_interval_ms(interval), root_ca_fallback(ca) {
    mutex = xSemaphoreCreateMutex();
}

ManagedResource::ManagedResource(const PsramString& u, const PsramString& headers, uint32_t interval, const char* ca)
    : url(u), customHeaders(headers), update_interval_ms(interval), root_ca_fallback(ca) {
    mutex = xSemaphoreCreateMutex();
}

ManagedResource::~ManagedResource() {
    if (data_buffer) free(data_buffer);
    if (mutex) vSemaphoreDelete(mutex);
}

ManagedResource::ManagedResource(ManagedResource&& other) noexcept
    : url(std::move(other.url)), customHeaders(std::move(other.customHeaders)), update_interval_ms(other.update_interval_ms), root_ca_fallback(other.root_ca_fallback),
      cert_filename(std::move(other.cert_filename)), data_buffer(other.data_buffer), data_size(other.data_size), 
      last_successful_update(other.last_successful_update), last_check_attempt(other.last_check_attempt), 
      last_check_attempt_ms(other.last_check_attempt_ms),
      mutex(other.mutex), retry_count(other.retry_count), is_in_retry_mode(other.is_in_retry_mode), is_data_stale(other.is_data_stale),
      is_paused(other.is_paused), has_priority(other.has_priority), use_ms_timing(other.use_ms_timing)
{
    other.data_buffer = nullptr; other.mutex = nullptr;
}


// --- WebClientModule Implementierung ---

// Allocates a ManagedResource in PSRAM (avoids internal-heap fragmentation)
template <typename... Args>
static ManagedResource* createManagedResource(Args&&... args) {
    void* mem = ps_malloc(sizeof(ManagedResource));
    if (!mem) return nullptr;
    return new (mem) ManagedResource(std::forward<Args>(args)...);
}

static void destroyManagedResource(ManagedResource* res) {
    if (!res) return;
    res->~ManagedResource();
    free(res);
}

// Extracts the host (without port) from a URL
static PsramString extractHost(const char* url) {
    PsramString u(url ? url : "");
    int hostStart = indexOf(u, "://");
    if (hostStart != -1) hostStart += 3; else hostStart = 0;
    int pathStart = indexOf(u, "/", hostStart);
    PsramString host = (pathStart != -1) ? u.substr(hostStart, pathStart - hostStart) : u.substr(hostStart);
    int colonPos = indexOf(host, ":");
    if (colonPos != -1) host = host.substr(0, colonPos);
    return host;
}

WebClientModule::WebClientModule() : workerTaskHandle(NULL), jobQueue(NULL), _startMs(0), _lastDownloadMs(0) {
    _resourcesMutex = xSemaphoreCreateRecursiveMutex();
    _busyMutex = xSemaphoreCreateMutex();
}

WebClientModule::~WebClientModule() {
    if (workerTaskHandle) vTaskDelete(workerTaskHandle);
    for (auto* res : resources) destroyManagedResource(res);
    resources.clear();
    if (_downloadBuffer) free(_downloadBuffer);
    if (jobQueue) vQueueDelete(jobQueue);
    if (_resourcesMutex) vSemaphoreDelete(_resourcesMutex);
    if (_busyMutex) vSemaphoreDelete(_busyMutex);
}

bool WebClientModule::lockResources(TickType_t timeout) {
    if (!_resourcesMutex) return false;
    return xSemaphoreTakeRecursive(_resourcesMutex, timeout) == pdTRUE;
}

void WebClientModule::unlockResources() {
    if (_resourcesMutex) xSemaphoreGiveRecursive(_resourcesMutex);
}

// Caller must hold _resourcesMutex. customHeaders == nullptr matches only resources without headers.
ManagedResource* WebClientModule::findResourceLocked(const char* url, const char* customHeaders) {
    for (auto* res : resources) {
        if (res->pending_delete) continue;
        if (res->url.compare(url) != 0) continue;
        if (customHeaders == nullptr) {
            if (res->customHeaders.empty()) return res;
        } else if (res->customHeaders.compare(customHeaders) == 0) {
            return res;
        }
    }
    return nullptr;
}

// Caller must hold _resourcesMutex. Deletes immediately or defers if the worker is downloading it.
void WebClientModule::deleteResourceLocked(size_t index) {
    if (index >= resources.size()) return;
    ManagedResource* res = resources[index];
    if (res->in_use) {
        res->pending_delete = true;
        res->is_paused = true;
        return;
    }
    resources.erase(resources.begin() + index);
    destroyManagedResource(res);
}

bool WebClientModule::tryBeginProcessing() {
    if (!_busyMutex) return true;
    return xSemaphoreTake(_busyMutex, 0) == pdTRUE;
}

void WebClientModule::endProcessing() {
    // One full processing cycle has seen the latest download -> next download may start
    _awaitingProcessing = false;
    if (_busyMutex) xSemaphoreGive(_busyMutex);
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
    if (!lockResources()) return;

    PsramString host = extractHost(url.c_str());

    // Check if a resource (without headers) with the same host already exists
    for (auto* res : resources) {
        if (res->pending_delete || !res->customHeaders.empty()) continue;
        PsramString existingHost = extractHost(res->url.c_str());

        // If same host, update the URL but keep other parameters
        if (existingHost == host) {
            if (res->url.compare(url.c_str()) != 0 && xSemaphoreTake(res->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                res->url = url.c_str();
                Log.printf("[WebDataManager] URL aktualisiert für Host %s: %s\n", host.c_str(), url.c_str());
                xSemaphoreGive(res->mutex);
            }
            unlockResources();
            return;
        }
    }

    uint32_t interval_ms = update_interval_minutes * 60 * 1000UL;
    ManagedResource* new_res = createManagedResource(PsramString(url.c_str()), interval_ms, root_ca);
    if (new_res) {
        resources.push_back(new_res);
        Log.printf("[WebDataManager] Ressource registriert: %s\n", new_res->url.c_str());
    } else {
        Log.printf("[WebDataManager] FEHLER: Konnte Ressource %s nicht allozieren.\n", url.c_str());
    }
    unlockResources();
}

void WebClientModule::registerResourceWithHeaders(const String& url, const String& customHeaders, uint32_t update_interval_minutes, const char* root_ca) {
    if (url.isEmpty() || update_interval_minutes == 0) return;
    if (!lockResources()) return;

    // For resources with custom headers, we register each URL+header combination as a separate resource
    // This allows multiple resources with the same URL but different headers (e.g., different park IDs)
    if (findResourceLocked(url.c_str(), customHeaders.c_str())) {
        unlockResources();
        return; // Already exists
    }

    uint32_t interval_ms = update_interval_minutes * 60 * 1000UL;
    ManagedResource* new_res = createManagedResource(PsramString(url.c_str()), PsramString(customHeaders.c_str()), interval_ms, root_ca);
    if (new_res) {
        resources.push_back(new_res);
        Log.printf("[WebDataManager] Ressource mit Headers registriert: %s (Headers: %s)\n",
                   new_res->url.c_str(), new_res->customHeaders.c_str());
    }
    unlockResources();
}

void WebClientModule::registerResourceSeconds(const String& url, uint32_t update_interval_seconds, bool with_priority, bool force_new, const char* root_ca) {
    LOG_MEM_OP("WebClient::registerResource");
    if (url.isEmpty() || update_interval_seconds == 0) return;
    if (!lockResources()) return;

    uint32_t interval_ms = update_interval_seconds * 1000UL;

    // If force_new is FALSE: check if URL exists and update its parameters
    if (!force_new) {
        ManagedResource* res = findResourceLocked(url.c_str(), nullptr);
        if (res) {
            if (res->update_interval_ms != interval_ms || res->has_priority != with_priority || !res->use_ms_timing) {
                if (xSemaphoreTake(res->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    res->update_interval_ms = interval_ms;
                    res->has_priority = with_priority;
                    res->use_ms_timing = true;  // Enable millisecond-precise timing
                    Log.printf("[WebDataManager] Resource parameters updated: %s (Intervall: %u Sek, Priorität: %d)\n",
                              url.c_str(), update_interval_seconds, with_priority);
                    xSemaphoreGive(res->mutex);
                }
            }
            unlockResources();
            return;
        }
    }

    // If force_new is TRUE: always create new resource, even if URL already exists
    ManagedResource* new_res = createManagedResource(PsramString(url.c_str()), interval_ms, root_ca);
    if (new_res) {
        new_res->has_priority = with_priority;
        new_res->use_ms_timing = true;  // Enable millisecond-precise timing
        resources.push_back(new_res);
        Log.printf("[WebDataManager] Ressource registriert (Sekunden-genau): %s, Intervall: %u Sek, Priorität: %d, ForceNew: %d\n",
                   new_res->url.c_str(), update_interval_seconds, with_priority, force_new);
    }
    unlockResources();
}

void WebClientModule::registerResourceSecondsWithHeaders(const String& url, const String& customHeaders, uint32_t update_interval_seconds, bool with_priority, const char* root_ca) {
    if (url.isEmpty() || update_interval_seconds == 0) return;
    if (!lockResources()) return;

    if (findResourceLocked(url.c_str(), customHeaders.c_str())) {
        unlockResources();
        return; // Already exists
    }

    uint32_t interval_ms = update_interval_seconds * 1000UL;
    ManagedResource* new_res = createManagedResource(PsramString(url.c_str()), PsramString(customHeaders.c_str()), interval_ms, root_ca);
    if (new_res) {
        new_res->has_priority = with_priority;
        new_res->use_ms_timing = true;  // Enable millisecond-precise timing
        resources.push_back(new_res);
        Log.printf("[WebDataManager] Ressource mit Headers registriert (Sekunden-genau): %s, Intervall: %u Sek, Priorität: %d (Headers: %s)\n",
                   new_res->url.c_str(), update_interval_seconds, with_priority, new_res->customHeaders.c_str());
    }
    unlockResources();
}

void WebClientModule::updateResourceUrl(const String& old_url, const String& new_url) {
    if (!lockResources()) return;
    for (auto* resource : resources) {
        if (!resource->pending_delete && resource->url.compare(old_url.c_str()) == 0) {
            if (xSemaphoreTake(resource->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                resource->url = new_url.c_str();
                Log.printf("[WebDataManager] URL für Ressource aktualisiert: %s -> %s\n", old_url.c_str(), new_url.c_str());
                xSemaphoreGive(resource->mutex);
            }
            break;
        }
    }
    unlockResources();
}

void WebClientModule::unregisterResource(const String& url) {
    if (url.isEmpty() || !lockResources()) return;
    // Remove all resources (without headers) matching the URL (also duplicates created by force_new)
    for (size_t i = resources.size(); i-- > 0;) {
        ManagedResource* res = resources[i];
        if (res->customHeaders.empty() && res->url.compare(url.c_str()) == 0) {
            Log.printf("[WebDataManager] Ressource entfernt: %s%s\n", url.c_str(), res->in_use ? " (verzögert)" : "");
            deleteResourceLocked(i);
        }
    }
    unlockResources();
}

void WebClientModule::unregisterResourceWithHeaders(const String& url, const String& customHeaders) {
    if (url.isEmpty() || !lockResources()) return;
    for (size_t i = resources.size(); i-- > 0;) {
        ManagedResource* res = resources[i];
        if (res->url.compare(url.c_str()) == 0 && res->customHeaders.compare(customHeaders.c_str()) == 0) {
            Log.printf("[WebDataManager] Ressource mit Headers entfernt: %s\n", url.c_str());
            deleteResourceLocked(i);
        }
    }
    unlockResources();
}

static void setPausedLocked(ManagedResource* resource, bool paused, const char* url, bool withHeaders) {
    if (!resource) return;
    if (xSemaphoreTake(resource->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        resource->is_paused = paused;
        Log.printf("[WebDataManager] Ressource%s %s: %s\n", withHeaders ? " mit Headers" : "", paused ? "pausiert" : "fortgesetzt", url);
        xSemaphoreGive(resource->mutex);
    }
}

void WebClientModule::pauseResource(const String& url) {
    if (!lockResources()) return;
    setPausedLocked(findResourceLocked(url.c_str(), nullptr), true, url.c_str(), false);
    unlockResources();
}

void WebClientModule::pauseResourceWithHeaders(const String& url, const String& customHeaders) {
    if (!lockResources()) return;
    setPausedLocked(findResourceLocked(url.c_str(), customHeaders.c_str()), true, url.c_str(), true);
    unlockResources();
}

void WebClientModule::resumeResource(const String& url) {
    if (!lockResources()) return;
    setPausedLocked(findResourceLocked(url.c_str(), nullptr), false, url.c_str(), false);
    unlockResources();
}

void WebClientModule::resumeResourceWithHeaders(const String& url, const String& customHeaders) {
    if (!lockResources()) return;
    setPausedLocked(findResourceLocked(url.c_str(), customHeaders.c_str()), false, url.c_str(), true);
    unlockResources();
}

void WebClientModule::accessResource(const String& url, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback) {
    LOG_MEM_OP("WebClient::accessResource");
    if (!lockResources(pdMS_TO_TICKS(1000))) return;
    ManagedResource* resource = findResourceLocked(url.c_str(), nullptr);
    if (resource) {
        if (xSemaphoreTake(resource->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // MODIFIKATION: Wenn disableModuleDataAccess true, gebe leere Daten zurück
            if (disableModuleDataAccess) {
                callback(nullptr, 0, resource->last_successful_update, true); // "keine Daten" simulieren
            } else {
                callback(resource->data_buffer, resource->data_size, resource->last_successful_update, resource->is_data_stale);
            }
            xSemaphoreGive(resource->mutex);
        } else {
            Log.printf("[WebDataManager] Timeout beim Warten auf Mutex für %s\n", url.c_str());
        }
    }
    unlockResources();
}

void WebClientModule::accessResource(const String& url, const String& customHeaders, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback) {
    if (!lockResources(pdMS_TO_TICKS(1000))) return;
    ManagedResource* resource = findResourceLocked(url.c_str(), customHeaders.c_str());
    if (resource) {
        if (xSemaphoreTake(resource->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // MODIFIKATION: Wenn disableModuleDataAccess true, gebe leere Daten zurück
            if (disableModuleDataAccess) {
                callback(nullptr, 0, resource->last_successful_update, true); // "keine Daten" simulieren
            } else {
                callback(resource->data_buffer, resource->data_size, resource->last_successful_update, resource->is_data_stale);
            }
            xSemaphoreGive(resource->mutex);
        } else {
            Log.printf("[WebDataManager] Timeout beim Warten auf Mutex für %s (mit Headers)\n", url.c_str());
        }
    }
    unlockResources();
}

void WebClientModule::updateResourceCertificateByHost(const String& host, const String& cert_filename) {
    if (!lockResources()) return;
    for (auto* resource : resources) {
        if (indexOf(resource->url, host.c_str()) != -1) {
            if (xSemaphoreTake(resource->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                resource->cert_filename = cert_filename.c_str();
                Log.printf("[WebDataManager] Zertifikat für Ressource '%s' (Host: %s) live aktualisiert auf Datei: '%s'\n", resource->url.c_str(), host.c_str(), cert_filename.c_str());
                xSemaphoreGive(resource->mutex);
            }
        }
    }
    unlockResources();
}

void WebClientModule::getRequest(const PsramString& url, std::function<void(const char* buffer, size_t size)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] GET-Anfrage fehlgeschlagen: Keine WLAN-Verbindung.");
        callback(nullptr, 0);
        return;
    }
    // Allocate WebJob in PSRAM to reduce heap fragmentation
    void* jobMem = ps_malloc(sizeof(WebJob));
    if (!jobMem) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET allozieren.");
        callback(nullptr, 0);
        return;
    }
    WebJob* job = new (jobMem) WebJob{WebJob::GET, url, "", "", "", callback, nullptr};
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebClientModule] FEHLER: Konnte GET-Job nicht zur Queue hinzufügen.");
        job->~WebJob();  // Call destructor
        free(jobMem);
        callback(nullptr, 0);
    }
}

void WebClientModule::getRequest(const PsramString& url, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] GET-Anfrage (detailed) fehlgeschlagen: Keine WLAN-Verbindung.");
        detailed_callback(-1, "No WiFi", 7);
        return;
    }
    // Allocate WebJob in PSRAM to reduce heap fragmentation
    void* jobMem = ps_malloc(sizeof(WebJob));
    if (!jobMem) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET (detailed) allozieren.");
        detailed_callback(-1, "Malloc failed", 12);
        return;
    }
    WebJob* job = new (jobMem) WebJob{WebJob::GET, url, "", "", "", nullptr, detailed_callback};
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebDataManager] FEHLER: Konnte GET-Job (detailed) nicht zur Queue hinzufügen.");
        job->~WebJob();  // Call destructor
        free(jobMem);
        detailed_callback(-1, "Queue full", 10);
    }
}

void WebClientModule::getRequest(const PsramString& url, const PsramString& customHeaders, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] GET-Anfrage (detailed+headers) fehlgeschlagen: Keine WLAN-Verbindung.");
        detailed_callback(-1, "No WiFi", 7);
        return;
    }
    // Allocate WebJob in PSRAM to reduce heap fragmentation
    void* jobMem = ps_malloc(sizeof(WebJob));
    if (!jobMem) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für GET (detailed+headers) allozieren.");
        detailed_callback(-1, "Malloc failed", 12);
        return;
    }
    WebJob* job = new (jobMem) WebJob{WebJob::GET, url, "", "", customHeaders, nullptr, detailed_callback};
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebDataManager] FEHLER: Konnte GET-Job (detailed+headers) nicht zur Queue hinzufügen.");
        job->~WebJob();  // Call destructor
        free(jobMem);
        detailed_callback(-1, "Queue full", 10);
    }
}

void WebClientModule::postRequest(const PsramString& url, const PsramString& postBody, const PsramString& contentType, std::function<void(const char* buffer, size_t size)> callback) {
    if (WiFi.status() != WL_CONNECTED) {
        Log.println("[WebClientModule] POST-Anfrage fehlgeschlagen: Keine WLAN-Verbindung.");
        callback(nullptr, 0);
        return;
    }

    // Allocate WebJob in PSRAM to reduce heap fragmentation
    void* jobMem = ps_malloc(sizeof(WebJob));
    if (!jobMem) {
        Log.println("[WebClientModule] FEHLER: Konnte keinen WebJob für POST allozieren.");
        callback(nullptr, 0);
        return;
    }

    WebJob* job = new (jobMem) WebJob{WebJob::POST, url, postBody, contentType, "", callback, nullptr};
    if (xQueueSend(jobQueue, &job, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[WebClientModule] FEHLER: Konnte POST-Job nicht zur Queue hinzufügen.");
        job->~WebJob();  // Call destructor
        free(jobMem);
        callback(nullptr, 0);
    }
}

namespace {
struct SyncRequestState {
    SemaphoreHandle_t sem = nullptr;
    int httpCode = 0;
    PsramString payload;
    SyncRequestState() { sem = xSemaphoreCreateBinary(); }
    ~SyncRequestState() { if (sem) vSemaphoreDelete(sem); }
};
}

bool WebClientModule::getRequestSync(const PsramString& url, const PsramString& customHeaders, SyncResult& result, uint32_t timeoutMs) {
    std::shared_ptr<SyncRequestState> state = std::make_shared<SyncRequestState>();
    if (!state->sem) {
        result.httpCode = -1;
        return false;
    }
    getRequest(url, customHeaders, [state](int httpCode, const char* payload, size_t len) {
        state->httpCode = httpCode;
        if (payload && len > 0) state->payload.assign(payload, len);
        xSemaphoreGive(state->sem);
    });
    if (xSemaphoreTake(state->sem, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        return false;
    }
    result.httpCode = state->httpCode;
    result.payload = std::move(state->payload);
    return true;
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
                // Set User-Agent header
                http.setUserAgent(_userAgent.c_str());
                
                // Add custom headers if provided
                if (!job.customHeaders.empty()) {
                    // Parse and add headers (format: "Header1: Value1\nHeader2: Value2")
                    PsramString headers = job.customHeaders;
                    size_t pos = 0;
                    while (pos < headers.length()) {
                        size_t newlinePos = headers.find('\n', pos);
                        if (newlinePos == PsramString::npos) newlinePos = headers.length();
                        
                        PsramString headerLine = headers.substr(pos, newlinePos - pos);
                        size_t colonPos = headerLine.find(':');
                        if (colonPos != PsramString::npos) {
                            PsramString headerName = headerLine.substr(0, colonPos);
                            PsramString headerValue = headerLine.substr(colonPos + 1);
                            // Trim leading/trailing spaces
                            while (!headerValue.empty() && headerValue[0] == ' ') headerValue = headerValue.substr(1);
                            while (!headerValue.empty() && headerValue[headerValue.length()-1] == ' ') headerValue = headerValue.substr(0, headerValue.length()-1);
                            http.addHeader(headerName.c_str(), headerValue.c_str());
                            Log.printf("[WebDataManager] Added header: %s: %s\n", headerName.c_str(), headerValue.c_str());
                        }
                        pos = newlinePos + 1;
                    }
                }
                
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
                // Set User-Agent header
                http.setUserAgent(_userAgent.c_str());
                
                // Add custom headers if provided
                if (!job.customHeaders.empty()) {
                    // Parse and add headers (format: "Header1: Value1\nHeader2: Value2")
                    PsramString headers = job.customHeaders;
                    size_t pos = 0;
                    while (pos < headers.length()) {
                        size_t newlinePos = headers.find('\n', pos);
                        if (newlinePos == PsramString::npos) newlinePos = headers.length();
                        
                        PsramString headerLine = headers.substr(pos, newlinePos - pos);
                        size_t colonPos = headerLine.find(':');
                        if (colonPos != PsramString::npos) {
                            PsramString headerName = headerLine.substr(0, colonPos);
                            PsramString headerValue = headerLine.substr(colonPos + 1);
                            // Trim leading/trailing spaces
                            while (!headerValue.empty() && headerValue[0] == ' ') headerValue = headerValue.substr(1);
                            while (!headerValue.empty() && headerValue[headerValue.length()-1] == ' ') headerValue = headerValue.substr(0, headerValue.length()-1);
                            http.addHeader(headerName.c_str(), headerValue.c_str());
                            Log.printf("[WebDataManager] Added header: %s: %s\n", headerName.c_str(), headerValue.c_str());
                        }
                        pos = newlinePos + 1;
                    }
                }
                
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

// Helper function to safely calculate elapsed time in milliseconds, handling millis() rollover
static inline unsigned long millisElapsed(unsigned long start, unsigned long now) {
    // This handles rollover correctly because unsigned arithmetic wraps around
    return now - start;
}

static bool isResourceDue(const ManagedResource& resource, time_t now, unsigned long nowMs) {
    if (resource.is_paused || resource.pending_delete || resource.in_use) return false;
    if (resource.is_in_retry_mode) {
        if (resource.use_ms_timing) {
            return resource.last_check_attempt_ms == 0 || millisElapsed(resource.last_check_attempt_ms, nowMs) >= 30000UL;
        }
        return (now - resource.last_check_attempt) * 1000UL >= 30000UL;
    }
    if (resource.use_ms_timing) {
        return resource.last_check_attempt_ms == 0 || millisElapsed(resource.last_check_attempt_ms, nowMs) >= resource.update_interval_ms;
    }
    return resource.last_check_attempt == 0 || (now - resource.last_check_attempt) * 1000UL >= resource.update_interval_ms;
}

// Performs the download for a resource that was marked in_use by the worker.
// Afterwards the resource is released again (or finally deleted if it was unregistered meanwhile).
void WebClientModule::runResourceUpdate(ManagedResource* resource, bool download) {
    if (download) performUpdate(*resource);
    lockResources();
    resource->in_use = false;
    if (resource->pending_delete) {
        for (size_t i = 0; i < resources.size(); ++i) {
            if (resources[i] == resource) {
                deleteResourceLocked(i);
                break;
            }
        }
    }
    unlockResources();
}

void WebClientModule::webWorkerTask(void* param) {
    WebClientModule* self = static_cast<WebClientModule*>(param);
    Log.printf("[WebDataManager] Worker-Task gestartet auf Core %d.\n", xPortGetCoreID());
    
    WebJob* receivedJob;
    const unsigned long MIN_PAUSE_BETWEEN_DOWNLOADS_MS = 10000UL;   // 10 seconds (normal resources + jobs)
    const unsigned long MIN_PAUSE_PRIORITY_MS = 2000UL;             // priority (live) resources
    const unsigned long MIN_PAUSE_JOB_MS = 1000UL;                  // on-demand jobs (callers wait synchronously)
    const unsigned long MAX_WAIT_FOR_PROCESSING_MS = 5000UL;        // safety net if main loop does not process
    const TickType_t BUSY_LOCK_TIMEOUT = pdMS_TO_TICKS(10000);

    while (true) {
        unsigned long nowMs = millis();

        // enforce initial start delay of 10 seconds before any download happens
        if (nowMs - self->_startMs < MIN_PAUSE_BETWEEN_DOWNLOADS_MS) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        if (WiFi.status() == WL_CONNECTED) {
            // --- 1. On-demand jobs (e.g. web interface, Fritzbox) ---
            // These do not wait for a processing acknowledge (callers may block the main loop
            // while waiting for the result), but they are strictly serialized with all other
            // downloads and with the module processing via _busyMutex.
            if (uxQueueMessagesWaiting(self->jobQueue) > 0 &&
                (self->_lastDownloadMs == 0 || millisElapsed(self->_lastDownloadMs, nowMs) >= MIN_PAUSE_JOB_MS)) {
                if (xQueueReceive(self->jobQueue, &receivedJob, 0) == pdTRUE) {
                    if (xSemaphoreTake(self->_busyMutex, BUSY_LOCK_TIMEOUT) == pdTRUE) {
                        self->_lastDownloadMs = millis();
                        self->performJob(*receivedJob);
                        self->_lastDownloadMs = millis();
                        xSemaphoreGive(self->_busyMutex);
                        receivedJob->~WebJob();  // Call destructor
                        free(receivedJob);  // Free PSRAM memory
                    } else if (xQueueSendToFront(self->jobQueue, &receivedJob, pdMS_TO_TICKS(100)) != pdTRUE) {
                        Log.println("[WebDataManager] WARNUNG: Konnte Job nicht zurück in Queue einreihen, verwerfe.");
                        receivedJob->~WebJob();
                        free(receivedJob);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            // --- 2. Periodic resource updates ---
            // Strict sequence: Download -> Verarbeitung -> nächster Download.
            // Wait until the main loop has processed the result of the previous download.
            if (self->_awaitingProcessing && millisElapsed(self->_awaitingSinceMs, nowMs) < MAX_WAIT_FOR_PROCESSING_MS) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            time_t now;
            time(&now);
            nowMs = millis();
            unsigned long sinceLast = (self->_lastDownloadMs == 0) ? (unsigned long)-1 : millisElapsed(self->_lastDownloadMs, nowMs);

            ManagedResource* selected = nullptr;
            if (self->lockResources(pdMS_TO_TICKS(1000))) {
                // First pass: priority resources (shorter minimum pause)
                if (sinceLast >= MIN_PAUSE_PRIORITY_MS) {
                    for (auto* resource : self->resources) {
                        if (resource->has_priority && isResourceDue(*resource, now, nowMs)) {
                            selected = resource;
                            break;
                        }
                    }
                }
                // Second pass: normal resources (respect global minimum pause)
                if (!selected && sinceLast >= MIN_PAUSE_BETWEEN_DOWNLOADS_MS) {
                    for (auto* resource : self->resources) {
                        if (isResourceDue(*resource, now, nowMs)) {
                            selected = resource;
                            break;
                        }
                    }
                }
                if (selected) {
                    selected->in_use = true;  // protects against deletion while downloading
                    if (selected->use_ms_timing) selected->last_check_attempt_ms = nowMs;
                    if (selected->has_priority) {
                        Log.printf("[WebDataManager] Prioritäts-Ressource wird ausgeführt: %s\n", selected->url.c_str());
                    }
                }
                self->unlockResources();
            }

            if (selected) {
                if (xSemaphoreTake(self->_busyMutex, BUSY_LOCK_TIMEOUT) == pdTRUE) {
                    self->_lastDownloadMs = millis();
                    self->runResourceUpdate(selected, true);
                    self->_lastDownloadMs = millis();
                    self->_awaitingSinceMs = millis();
                    self->_awaitingProcessing = true;
                    xSemaphoreGive(self->_busyMutex);
                } else {
                    // Could not get exclusive access - release resource and try again later
                    if (selected->use_ms_timing) selected->last_check_attempt_ms = 0;
                    self->runResourceUpdate(selected, false);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// performUpdate unchanged except cert discovery replaces previous deviceConfig cert-field usage
void WebClientModule::performUpdate(ManagedResource& resource) {
    LOG_MEMORY_STRATEGIC("WebClient: Begin performUpdate");
    // Take snapshots of the strings that may be changed concurrently by the main loop
    // (registerResource/updateResourceUrl) to avoid racing on std::string internals.
    PsramString url;
    PsramString customHeaders;
    PsramString configuredCertFilename;
    if (xSemaphoreTake(resource.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Log.println("[WebDataManager] Timeout beim Warten auf Ressourcen-Mutex, Update übersprungen.");
        return;
    }
    url = url;
    customHeaders = customHeaders;
    configuredCertFilename = resource.cert_filename;
    xSemaphoreGive(resource.mutex);

    Log.printf("[WebDataManager] Starte Update für %s...\n", url.c_str());
    resource.last_check_attempt = time(nullptr);
    bool retry_with_larger_buffer;
    int max_growth_retries = 3;

    do {
        retry_with_larger_buffer = false;
        HTTPClient http;
        
        int httpCode = 0;
        WiFiClientSecure secure_client;
        WiFiClient plain_client;

        if (url.rfind("https://", 0) == 0) {
            PsramString cert_data;
            bool cert_loaded = false;
            PsramString filepath;

            // determine host
            PsramString host;
            uint16_t port = 443;
            int hostStart = indexOf(url, "://") + 3;
            int pathStart = indexOf(url, "/", hostStart);
            host = (pathStart != -1) ? url.substr(hostStart, pathStart - hostStart) : url.substr(hostStart);
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
                        if (xSemaphoreTake(resource.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                            resource.cert_filename = found; // record which file was used
                            xSemaphoreGive(resource.mutex);
                        }
                    }
                }
            }

            // fallback: if resource already has cert_filename configured, try it
            if (!cert_loaded && !configuredCertFilename.empty()) {
                filepath = PsramString("/certs/") + configuredCertFilename;
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
                Log.printf("[WebDataManager] Verwende Fallback-Zertifikat für %s.\n", url.c_str());
                secure_client.setCACert(resource.root_ca_fallback);
            } else if (!cert_loaded) {
                Log.printf("[WebDataManager] WARNUNG: Kein Zertifikat gefunden. Verwende unsichere Verbindung für %s.\n", url.c_str());
                secure_client.setInsecure();
            } else {
                Log.printf("[WebDataManager] Verwende Zertifikat aus Datei '%s' für %s.\n", filepath.c_str(), url.c_str());
                secure_client.setCACert(cert_data.c_str());
            }
            
            if (!secure_client.connect(host.c_str(), port)) {
                httpCode = -1;
            } else {
                if (http.begin(secure_client, url.c_str())) {
                     // Set User-Agent header
                     http.setUserAgent(_userAgent.c_str());
                     http.setTimeout(15000);
                     // Add custom headers if present
                     if (!customHeaders.empty()) {
                         PsramString headers = customHeaders;
                         size_t pos = 0;
                         while (pos < headers.length()) {
                             size_t newlinePos = headers.find('\n', pos);
                             if (newlinePos == PsramString::npos) newlinePos = headers.length();
                             
                             PsramString headerLine = headers.substr(pos, newlinePos - pos);
                             size_t colonPos = headerLine.find(':');
                             if (colonPos != PsramString::npos) {
                                 PsramString name = headerLine.substr(0, colonPos);
                                 PsramString value = headerLine.substr(colonPos + 1);
                                 // Trim leading/trailing whitespace from value
                                 while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                                     value = value.substr(1);
                                 }
                                 while (!value.empty() && (value[value.length()-1] == ' ' || value[value.length()-1] == '\t' || value[value.length()-1] == '\r')) {
                                     value = value.substr(0, value.length()-1);
                                 }
                                 http.addHeader(name.c_str(), value.c_str());
                                 Log.printf("[WebDataManager] Added header: %s: %s\n", name.c_str(), value.c_str());
                             }
                             pos = newlinePos + 1;
                         }
                     }
                     httpCode = http.GET();
                } else {
                    httpCode = -10;
                }
            }
        } else {
            if (http.begin(plain_client, url.c_str())) {
                // Set User-Agent header
                http.setUserAgent(_userAgent.c_str());
                http.setTimeout(15000);
                // Add custom headers if present
                if (!customHeaders.empty()) {
                    PsramString headers = customHeaders;
                    size_t pos = 0;
                    while (pos < headers.length()) {
                        size_t newlinePos = headers.find('\n', pos);
                        if (newlinePos == PsramString::npos) newlinePos = headers.length();
                        
                        PsramString headerLine = headers.substr(pos, newlinePos - pos);
                        size_t colonPos = headerLine.find(':');
                        if (colonPos != PsramString::npos) {
                            PsramString name = headerLine.substr(0, colonPos);
                            PsramString value = headerLine.substr(colonPos + 1);
                            // Trim leading/trailing whitespace from value
                            while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                                value = value.substr(1);
                            }
                            while (!value.empty() && (value[value.length()-1] == ' ' || value[value.length()-1] == '\t' || value[value.length()-1] == '\r')) {
                                value = value.substr(0, value.length()-1);
                            }
                            http.addHeader(name.c_str(), value.c_str());
                            Log.printf("[WebDataManager] Added header: %s: %s\n", name.c_str(), value.c_str());
                        }
                        pos = newlinePos + 1;
                    }
                }
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
                Log.printf("[WebClientModule] LERNEN: Pufferüberlauf bei %s. Puffer wird vergrößert.\n", url.c_str());
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
                        xSemaphoreTake(resource.mutex, portMAX_DELAY);
                        if (resource.url != url) {
                            // URL was changed while downloading -> data belongs to the old URL, discard it
                            xSemaphoreGive(resource.mutex);
                            free(new_permanent_buffer);
                            Log.printf("[WebDataManager] URL während Download geändert, verwerfe Daten von %s.\n", url.c_str());
                        } else {
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
                            Log.printf("[WebDataManager] ERFOLG: %s aktualisiert (%u Bytes).\n", url.c_str(), (unsigned int)downloaded_size);
                        }
                    } else {
                        Log.printf("[WebClientModule] FEHLER: Konnte keinen passgenauen Puffer für %s allozieren.\n", url.c_str());
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
                Log.printf("[WebDataManager] FEHLER bei %s: %s. Max. Retries (%d) erreicht.\n", url.c_str(), errorMsg.c_str(), resource.retry_count);
                resource.retry_count = 0;
                resource.is_in_retry_mode = false;
            } else {
                Log.printf("[WebDataManager] FEHLER bei %s: %s. Versuch %d/3 in 30s.\n", url.c_str(), errorMsg.c_str(), resource.retry_count);
                resource.is_in_retry_mode = true;
            }
        }
        http.end();
        max_growth_retries--;
    } while (retry_with_larger_buffer && max_growth_retries > 0);
    LOG_MEMORY_STRATEGIC("WebClient: End performUpdate");
}
// User-Agent configuration methods
void WebClientModule::setUserAgent(const String& userAgent) {
    _userAgent = PsramString(userAgent.c_str());
    Log.printf("[WebDataManager] User-Agent set to: %s\n", _userAgent.c_str());
}

String WebClientModule::getUserAgent() const {
    return String(_userAgent.c_str());
}
