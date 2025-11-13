#ifndef WEBCLIENTMODULE_HPP
#define WEBCLIENTMODULE_HPP

#include <Arduino.h>
#include <functional>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "PsramUtils.hpp"

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
    uint32_t update_interval_ms;
    const char* root_ca_fallback;
    PsramString cert_filename;
    char* data_buffer = nullptr;
    size_t data_size = 0;
    time_t last_successful_update = 0;
    time_t last_check_attempt = 0;
    SemaphoreHandle_t mutex;
    uint8_t retry_count = 0;
    bool is_in_retry_mode = false;
    bool is_data_stale = true;

    ManagedResource(const PsramString& u, uint32_t interval, const char* ca);
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
    void updateResourceUrl(const String& old_url, const String& new_url);
    void accessResource(const String& url, std::function<void(const char* data, size_t size, time_t last_update, bool is_stale)> callback);
    void updateResourceCertificateByHost(const String& host, const String& cert_filename);
    
    void getRequest(const PsramString& url, std::function<void(const char* buffer, size_t size)> callback);
    void getRequest(const PsramString& url, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback);
    void getRequest(const PsramString& url, const PsramString& customHeaders, std::function<void(int httpCode, const char* payload, size_t len)> detailed_callback);
    void postRequest(const PsramString& url, const PsramString& postBody, const PsramString& contentType, std::function<void(const char* buffer, size_t size)> callback);

private:
    char* _downloadBuffer = nullptr;
    size_t _bufferCapacity = 0;
    PsramBufferStream _downloadStream;
    std::vector<ManagedResource, PsramAllocator<ManagedResource>> resources;
    TaskHandle_t workerTaskHandle;
    QueueHandle_t jobQueue;

    // Timing control: start delay and minimum pause between downloads (ms)
    unsigned long _startMs = 0;
    unsigned long _lastDownloadMs = 0;

    bool reallocateBuffer(size_t new_size);
    void performJob(const WebJob& job);
    void performUpdate(ManagedResource& resource);
    
    static void webWorkerTask(void* param);
};
#endif // WEBCLIENTMODULE_HPP