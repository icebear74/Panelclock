#ifndef WEBCLIENTMODULE_HPP
#define WEBCLIENTMODULE_HPP

#include <Arduino.h>
#include <functional>
#include <vector>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "PsramUtils.hpp"

// Default User-Agent string (can be overridden by defining DEFAULT_USER_AGENT before including this header)
#ifndef DEFAULT_USER_AGENT
#define DEFAULT_USER_AGENT "ESP32-PanelClock/1.0"
#endif

// Vorwärtsdeklarationen, um Header-Abhängigkeiten zu minimieren
struct DeviceConfig;
extern DeviceConfig* deviceConfig;

class PsramBufferStream : public Stream {
public:
    PsramBufferStream();
    void begin(char* buffer, size_t capacity);
    void reset();
    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    bool hasOverflowed() const;
    size_t getCapacity() const;
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    char* getBuffer();
    size_t getSize();

private:
    char* _buffer = nullptr;
    size_t _capacity = 0;
    size_t _position = 0;
    bool _overflowed = false;
};

struct ManagedResource {
    PsramString url;
    PsramString customHeaders;  // Optional headers for this resource (format: "Header1: Value1\nHeader2: Value2")
    uint32_t update_interval_ms;
    const char* root_ca_fallback;
    PsramString cert_filename;
    char* data_buffer = nullptr;
    size_t data_size = 0;
    time_t last_successful_update = 0;
    time_t last_check_attempt = 0;
    unsigned long last_check_attempt_ms = 0;  // Millisecond-precise timestamp for second-precise intervals
    SemaphoreHandle_t mutex;
    uint8_t retry_count = 0;
    bool is_in_retry_mode = false;
    bool is_data_stale = true;
    bool is_paused = false;          // When true, resource polling is paused
    bool has_priority = false;       // When true, resource is checked with priority when due
    bool use_ms_timing = false;      // When true, use millisecond-precise timing
    bool in_use = false;             // Set by worker while a download for this resource is running
    bool pending_delete = false;     // Unregistered while in use -> worker deletes it after the download

    ManagedResource(const PsramString& u, uint32_t interval, const char* ca);
    ManagedResource(const PsramString& u, const PsramString& headers, uint32_t interval, const char* ca);
    ~ManagedResource();
    ManagedResource(ManagedResource&& other) noexcept;
};

struct WebJob {
    enum JobType { GET, POST };
    JobType type;
    PsramString url;
    PsramString body;
    PsramString contentType;
    PsramString customHeaders;  // Format: "Header1: Value1\nHeader2: Value2"
    std::function<void(const char* buffer, size_t size)> callback;
    std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback;
};

class WebClientModule {
public:
    WebClientModule();
    ~WebClientModule();

    void begin();
    void registerResource(const String& url, uint32_t update_interval_minutes, const char* root_ca = nullptr);
    void registerResourceWithHeaders(const String& url, const String& customHeaders, uint32_t update_interval_minutes, const char* root_ca = nullptr);
    
    // Second-precise registration (interval in seconds, with priority option, force_new to avoid URL replacement)
    void registerResourceSeconds(const String& url, uint32_t update_interval_seconds, bool with_priority = false, bool force_new = false, const char* root_ca = nullptr);
    void registerResourceSecondsWithHeaders(const String& url, const String& customHeaders, uint32_t update_interval_seconds, bool with_priority = false, const char* root_ca = nullptr);
    
    void updateResourceUrl(const String& old_url, const String& new_url);

    // Removes a resource (without custom headers) completely and frees its data buffer.
    // Safe to call while the worker is downloading this resource (deletion is deferred).
    void unregisterResource(const String& url);
    void accessResource(const String& url, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback);
    void accessResource(const String& url, const String& customHeaders, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback);
    void updateResourceCertificateByHost(const String& host, const String& cert_filename);
    
    // Pause/Resume control for resources
    void pauseResource(const String& url);
    void pauseResourceWithHeaders(const String& url, const String& customHeaders);
    void resumeResource(const String& url);
    void resumeResourceWithHeaders(const String& url, const String& customHeaders);
    
    void getRequest(const PsramString& url, std::function<void(const char* buffer, size_t size)> callback);
    void getRequest(const PsramString& url, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback);
    void getRequest(const PsramString& url, const PsramString& customHeaders, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback);
    void postRequest(const PsramString& url, const PsramString& postBody, const PsramString& contentType, std::function<void(const char* buffer, size_t size)> callback);

    // Synchronous GET (blocks the caller up to timeoutMs). Safe against late callbacks:
    // the result state is reference-counted, so a response arriving after the timeout
    // does not write into a stack frame that no longer exists.
    struct SyncResult { int httpCode = 0; PsramString payload; };
    bool getRequestSync(const PsramString& url, const PsramString& customHeaders, SyncResult& result, uint32_t timeoutMs);
    
    // User-Agent configuration
    void setUserAgent(const String& userAgent);
    String getUserAgent() const;

    // Strict sequencing "Download -> Verarbeitung -> naechster Download":
    // The main loop must wrap queueData()/processData() of all modules with
    // tryBeginProcessing()/endProcessing(). tryBeginProcessing() returns false while a
    // download is running (the caller must then skip processing for this cycle).
    // After every completed download the worker waits until one complete processing
    // cycle has finished before it starts the next download.
    bool tryBeginProcessing();
    void endProcessing();

private:
    char* _downloadBuffer = nullptr;
    size_t _bufferCapacity = 0;
    PsramBufferStream _downloadStream;
    // Resources are stored as pointers so that registering new resources (vector growth)
    // never invalidates a resource the worker is currently downloading.
    // All structural access to the vector is guarded by _resourcesMutex (recursive).
    std::vector<ManagedResource*, PsramAllocator<ManagedResource*>> resources;
    SemaphoreHandle_t _resourcesMutex = nullptr;
    // Held by the worker for the whole duration of a download (+ synchronous callbacks)
    // and by the main loop while modules process data -> never both at the same time.
    SemaphoreHandle_t _busyMutex = nullptr;
    TaskHandle_t workerTaskHandle;
    QueueHandle_t jobQueue;

    // Timing control: start delay and minimum pause between downloads (ms)
    unsigned long _startMs = 0;
    unsigned long _lastDownloadMs = 0;
    
    // Set by the worker after a resource download; cleared by endProcessing() once the
    // main loop has processed the new data. The next resource download waits for it.
    volatile bool _awaitingProcessing = false;
    volatile unsigned long _awaitingSinceMs = 0;
    
    // Configurable User-Agent string (initialized with default from define)
    PsramString _userAgent = DEFAULT_USER_AGENT;

    bool lockResources(TickType_t timeout = portMAX_DELAY);
    void unlockResources();
    ManagedResource* findResourceLocked(const char* url, const char* customHeaders);
    void deleteResourceLocked(size_t index);
    void runResourceUpdate(ManagedResource* resource, bool download);

    bool reallocateBuffer(size_t new_size);
    void performJob(const WebJob& job);
    void performUpdate(ManagedResource& resource);
    
    static void webWorkerTask(void* param);
};
#endif // WEBCLIENTMODULE_HPP