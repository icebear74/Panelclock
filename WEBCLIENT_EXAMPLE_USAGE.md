# WebClient Module - Example Usage

This document provides practical examples of using the enhanced WebClient module features for different scenarios.

## Example 1: Basic Second-Precise Polling

Simple example polling an API every 10 seconds:

```cpp
// In your module's initialization or setConfig method
webClient->registerResourceSeconds(
    "https://api.example.com/live-data",
    10,      // Poll every 10 seconds
    false    // Normal priority
);

// In your module's queueData method - no changes needed!
// Access data as usual
webClient->accessResource(
    "https://api.example.com/live-data",
    [this](const char* data, size_t size, time_t last_update, bool is_stale) {
        if (data && size > 0) {
            // Process the data
            parseJson(data, size);
        }
    }
);
```

## Example 2: Priority Resource (Live Events)

For time-critical data that should be fetched immediately when due:

```cpp
// Register with priority flag
webClient->registerResourceSeconds(
    "https://api.sofascore.com/api/v1/sport/darts/live-events",
    5,       // Poll every 5 seconds
    true     // WITH PRIORITY - executes immediately when due
);
```

**What happens:**
- When the resource is due, it bypasses the normal 10-second minimum pause
- Executes immediately as the next download
- Other normal resources wait their turn

## Example 3: Conditional Live Polling (SofaScore Use Case)

This example shows how to implement dynamic polling based on live match status:

```cpp
class SofaScoreLiveModule : public DrawableModule {
private:
    WebClientModule* webClient;
    bool _hasLiveMatches = false;
    PsramString _scheduledEventsUrl;
    PsramString _liveEventsUrl;

public:
    void setConfig(...) {
        // Setup URLs
        _scheduledEventsUrl = "https://api.sofascore.com/.../scheduled-events/...";
        _liveEventsUrl = "https://api.sofascore.com/.../live-events";
        
        // Always register scheduled events (once per hour)
        webClient->registerResource(_scheduledEventsUrl, 60);
        
        // Register live events (every 10 seconds with priority)
        webClient->registerResourceSeconds(_liveEventsUrl, 10, true);
    }
    
    void processData() {
        // Check if there are live matches
        webClient->accessResource(_liveEventsUrl, 
            [this](const char* data, size_t size, time_t last_update, bool is_stale) {
                bool hasLive = checkForLiveMatches(data, size);
                
                if (hasLive && !_hasLiveMatches) {
                    // Live match started - pause scheduled events
                    Log.println("[SofaScore] Live match detected, pausing scheduled events");
                    webClient->pauseResource(_scheduledEventsUrl);
                    _hasLiveMatches = true;
                    
                } else if (!hasLive && _hasLiveMatches) {
                    // Live match ended - resume scheduled events
                    Log.println("[SofaScore] No live matches, resuming scheduled events");
                    webClient->resumeResource(_scheduledEventsUrl);
                    _hasLiveMatches = false;
                }
            }
        );
        
        // Process scheduled events (only if not paused)
        webClient->accessResource(_scheduledEventsUrl, 
            [this](const char* data, size_t size, time_t last_update, bool is_stale) {
                if (data && size > 0) {
                    parseDailyEvents(data, size);
                }
            }
        );
    }
    
private:
    bool checkForLiveMatches(const char* json, size_t len) {
        // Parse JSON and check if any matches are live
        // Return true if live matches exist, false otherwise
        // ... implementation ...
    }
};
```

## Example 4: Multiple Resources with Different Intervals

Managing several endpoints with different polling rates:

```cpp
void setupMultipleResources() {
    // Slow polling: Weather data every 30 minutes
    webClient->registerResource(
        "https://api.weather.com/forecast",
        30  // 30 minutes
    );
    
    // Medium polling: Stock prices every 60 seconds
    webClient->registerResourceSeconds(
        "https://api.stocks.com/prices",
        60  // 60 seconds
    );
    
    // Fast polling with priority: Live sports scores every 5 seconds
    webClient->registerResourceSeconds(
        "https://api.sports.com/live-scores",
        5,      // 5 seconds
        true    // Priority
    );
}
```

## Example 5: Pause/Resume Based on Conditions

Dynamically control polling based on application state:

```cpp
class SmartModule : public DrawableModule {
private:
    bool _isPowerSaveMode = false;
    
public:
    void enterPowerSaveMode() {
        Log.println("[SmartModule] Entering power save mode");
        
        // Pause all non-critical resources
        webClient->pauseResource("https://api.example.com/news");
        webClient->pauseResource("https://api.example.com/weather");
        
        // Keep critical resource active
        // (don't pause emergency alerts, for example)
        
        _isPowerSaveMode = true;
    }
    
    void exitPowerSaveMode() {
        Log.println("[SmartModule] Exiting power save mode");
        
        // Resume all resources
        webClient->resumeResource("https://api.example.com/news");
        webClient->resumeResource("https://api.example.com/weather");
        
        _isPowerSaveMode = false;
    }
    
    void periodicTick() override {
        // Check battery level or other conditions
        if (getBatteryLevel() < 20 && !_isPowerSaveMode) {
            enterPowerSaveMode();
        } else if (getBatteryLevel() > 50 && _isPowerSaveMode) {
            exitPowerSaveMode();
        }
    }
};
```

## Example 6: Resources with Custom Headers

For APIs requiring authentication or special headers:

```cpp
// Setup headers
String authHeaders = "Authorization: Bearer YOUR_TOKEN\nContent-Type: application/json";

// Register with custom headers (every 30 seconds)
webClient->registerResourceSecondsWithHeaders(
    "https://api.private.com/data",
    authHeaders,
    30,     // 30 seconds
    false   // Normal priority
);

// Access (must use matching headers!)
webClient->accessResource(
    "https://api.private.com/data",
    authHeaders,
    [](const char* data, size_t size, time_t last_update, bool is_stale) {
        // Process authenticated data
    }
);

// Pause/resume (must use matching headers!)
webClient->pauseResourceWithHeaders("https://api.private.com/data", authHeaders);
webClient->resumeResourceWithHeaders("https://api.private.com/data", authHeaders);
```

## Example 7: Hybrid Approach (Scheduled + On-Demand)

Combine periodic polling with priority polling for events:

```cpp
class EventModule {
private:
    PsramString _eventsUrl = "https://api.events.com/upcoming";
    
public:
    void setup() {
        // Normal polling: Check for new events every 5 minutes
        webClient->registerResource(_eventsUrl, 5);
    }
    
    void onUserInteraction() {
        // User wants fresh data NOW
        // Change to high-frequency priority polling temporarily
        
        // Update to 3-second priority polling
        webClient->registerResourceSeconds(_eventsUrl, 3, true);
        
        // Schedule a timer to revert back to normal polling after 1 minute
        setTimer(60000, []() {
            // Revert to 5-minute normal polling
            webClient->registerResource(_eventsUrl, 5);
        });
    }
};
```

## Example 8: Error Handling with Stale Data

Handle network errors and stale data gracefully:

```cpp
void processDataWithErrorHandling() {
    webClient->accessResource(
        "https://api.example.com/data",
        [this](const char* data, size_t size, time_t last_update, bool is_stale) {
            if (!data || size == 0) {
                Log.println("[Module] No data available yet");
                displayMessage("Loading...");
                return;
            }
            
            if (is_stale) {
                time_t now = time(nullptr);
                int age_seconds = now - last_update;
                
                Log.printf("[Module] Warning: Data is %d seconds old\n", age_seconds);
                
                if (age_seconds > 600) {  // More than 10 minutes old
                    displayWarning("Data may be outdated");
                }
            }
            
            // Process data
            parseAndDisplay(data, size);
        }
    );
}
```

## Performance Tips

### 1. Choose Appropriate Intervals

```cpp
// ✅ Good: Reasonable intervals
webClient->registerResourceSeconds("https://api.example.com/live", 10);  // Live data
webClient->registerResource("https://api.example.com/daily", 60);        // Daily summary

// ❌ Bad: Too frequent for non-critical data
webClient->registerResourceSeconds("https://api.example.com/weather", 1);  // Don't poll weather every second!
```

### 2. Use Priority Sparingly

```cpp
// ✅ Good: Only time-critical data gets priority
webClient->registerResourceSeconds("https://api.sports.com/live-score", 5, true);  // Priority for live scores

// ❌ Bad: Everything is priority (defeats the purpose)
webClient->registerResourceSeconds("https://api.example.com/news", 60, true);      // News doesn't need priority
webClient->registerResourceSeconds("https://api.example.com/weather", 60, true);   // Weather doesn't need priority
```

### 3. Pause When Not Needed

```cpp
// ✅ Good: Pause resources when module is not displayed
void onModuleHidden() {
    webClient->pauseResource("https://api.example.com/data");
}

void onModuleVisible() {
    webClient->resumeResource("https://api.example.com/data");
}
```

### 4. Group Related Operations

```cpp
// ✅ Good: Pause multiple related resources together
void pauseAllNewsFeeds() {
    webClient->pauseResource("https://api.news1.com/feed");
    webClient->pauseResource("https://api.news2.com/feed");
    webClient->pauseResource("https://api.news3.com/feed");
}
```

## Common Patterns

### Pattern 1: Background + Foreground Polling

```cpp
// Background: Slow periodic checks
webClient->registerResource(backgroundUrl, 60);  // Every hour

// Foreground: Fast priority checks when active
if (isModuleActive()) {
    webClient->registerResourceSeconds(foregroundUrl, 10, true);
} else {
    webClient->pauseResource(foregroundUrl);
}
```

### Pattern 2: Cascade Pause

```cpp
// When one resource indicates "no activity", pause others
if (noLiveEvents) {
    webClient->pauseResource(liveStatsUrl);
    webClient->pauseResource(liveDetailsUrl);
}
```

### Pattern 3: Burst Mode

```cpp
// Normal: Poll every 5 minutes
// Burst: Poll every 10 seconds for 2 minutes when triggered

void enterBurstMode() {
    webClient->registerResourceSeconds(url, 10, true);
    setTimer(120000, []() {
        exitBurstMode();
    });
}

void exitBurstMode() {
    webClient->registerResource(url, 5);  // Back to 5 minutes
}
```

## Debugging

Enable detailed logging to understand polling behavior:

```cpp
// Look for these log messages:
// [WebDataManager] Ressource registriert (Sekunden-genau): ...
// [WebDataManager] Ressource pausiert: ...
// [WebDataManager] Ressource fortgesetzt: ...
// [WebDataManager] Prioritäts-Ressource wird ausgeführt: ...
```

Monitor download timing:
```cpp
webClient->accessResource(url, 
    [](const char* data, size_t size, time_t last_update, bool is_stale) {
        time_t now = time(nullptr);
        int age = now - last_update;
        Log.printf("Data age: %d seconds, stale: %d\n", age, is_stale);
    }
);
```
