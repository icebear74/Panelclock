# WebClient Module - Quick Reference Guide

## Quick Start

### 1. Register a Second-Precise Resource

```cpp
// Poll every 10 seconds without priority
webClient->registerResourceSeconds(
    "https://api.example.com/data",
    10  // seconds
);

// Poll every 5 seconds WITH priority
webClient->registerResourceSeconds(
    "https://api.example.com/live-data",
    5,    // seconds
    true  // priority
);
```

### 2. Pause and Resume a Resource

```cpp
// Pause polling
webClient->pauseResource("https://api.example.com/data");

// Resume polling
webClient->resumeResource("https://api.example.com/data");
```

### 3. Access Resource Data (Unchanged)

```cpp
webClient->accessResource(
    "https://api.example.com/data",
    [](const char* data, size_t size, time_t last_update, bool is_stale) {
        if (data && size > 0) {
            // Process data
        }
    }
);
```

## Common Patterns

### Pattern: Live Event Detection

```cpp
// Setup
webClient->registerResource(scheduledUrl, 60);           // Hourly
webClient->registerResourceSeconds(liveUrl, 10, true);   // 10s with priority

// In processData()
webClient->accessResource(liveUrl, [this](const char* data, size_t size, ...) {
    bool hasLive = checkForLive(data, size);
    
    if (hasLive && !_wasLive) {
        webClient->pauseResource(scheduledUrl);  // Pause scheduled
        _wasLive = true;
    } else if (!hasLive && _wasLive) {
        webClient->resumeResource(scheduledUrl); // Resume scheduled
        _wasLive = false;
    }
});
```

### Pattern: Conditional Polling

```cpp
void onModuleActive() {
    webClient->resumeResource(url);
}

void onModuleInactive() {
    webClient->pauseResource(url);
}
```

### Pattern: Temporary Fast Polling

```cpp
void onUserInteraction() {
    // Switch to fast polling for 1 minute
    webClient->registerResourceSeconds(url, 3, true);
    
    setTimer(60000, [this]() {
        // Back to normal
        webClient->registerResource(url, 5);
    });
}
```

## Method Reference

### Registration

| Method | Interval Unit | Priority | Headers |
|--------|---------------|----------|---------|
| `registerResource()` | Minutes | No | No |
| `registerResourceWithHeaders()` | Minutes | No | Yes |
| `registerResourceSeconds()` | **Seconds** | Optional | No |
| `registerResourceSecondsWithHeaders()` | **Seconds** | Optional | Yes |

### Control

| Method | Purpose |
|--------|---------|
| `pauseResource(url)` | Pause polling (no headers) |
| `pauseResourceWithHeaders(url, headers)` | Pause polling (with headers) |
| `resumeResource(url)` | Resume polling (no headers) |
| `resumeResourceWithHeaders(url, headers)` | Resume polling (with headers) |

### Access (Unchanged)

| Method | Purpose |
|--------|---------|
| `accessResource(url, callback)` | Get cached data (no headers) |
| `accessResource(url, headers, callback)` | Get cached data (with headers) |

## Logging

Monitor these log messages:

```
[WebDataManager] Ressource registriert (Sekunden-genau): https://... Intervall: 10 Sek, Priorität: 1
[WebDataManager] Ressource pausiert: https://...
[WebDataManager] Ressource fortgesetzt: https://...
[WebDataManager] Prioritäts-Ressource wird ausgeführt: https://...
```

## Best Practices

✅ **DO:**
- Use priority for time-critical data (live events)
- Pause unused resources to save bandwidth
- Use reasonable intervals (5-10 seconds minimum for live data)
- Check `is_stale` flag when accessing data

❌ **DON'T:**
- Poll weather data every second (use minutes instead)
- Mark everything as priority (defeats the purpose)
- Register intervals less than 1 second
- Forget to resume paused resources

## Troubleshooting

**Resource not polling:**
- Check if it's paused
- Verify WiFi is connected
- Check logs for errors

**Timing not precise:**
- Actual precision is ±500ms due to worker task loop
- Use priority flag for more immediate execution

**Data always stale:**
- Resource might be paused
- Check if last successful update is recent
- Network issues might be preventing updates

## Complete Example

```cpp
class MyModule : public DrawableModule {
private:
    WebClientModule* webClient;
    bool _isLive = false;
    PsramString _normalUrl = "https://api.example.com/normal";
    PsramString _liveUrl = "https://api.example.com/live";

public:
    void setup() {
        // Normal: once per hour
        webClient->registerResource(_normalUrl, 60);
        
        // Live: every 10 seconds with priority
        webClient->registerResourceSeconds(_liveUrl, 10, true);
    }
    
    void processData() {
        // Check live status
        webClient->accessResource(_liveUrl,
            [this](const char* data, size_t size, time_t last_update, bool is_stale) {
                bool hasLive = checkLive(data, size);
                
                if (hasLive != _isLive) {
                    if (hasLive) {
                        webClient->pauseResource(_normalUrl);
                        Log.println("Entered live mode");
                    } else {
                        webClient->resumeResource(_normalUrl);
                        Log.println("Exited live mode");
                    }
                    _isLive = hasLive;
                }
            }
        );
        
        // Process normal data (if not paused)
        webClient->accessResource(_normalUrl,
            [this](const char* data, size_t size, time_t last_update, bool is_stale) {
                if (data && size > 0) {
                    processNormalData(data, size);
                }
            }
        );
    }
};
```

## See Also

- **WEBCLIENT_ENHANCEMENTS.md** - Detailed API documentation
- **WEBCLIENT_EXAMPLE_USAGE.md** - More examples and patterns
- **WEBCLIENT_IMPLEMENTATION_SUMMARY.md** - Technical implementation details
