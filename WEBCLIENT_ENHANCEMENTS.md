# WebClient Module Enhancements

## Overview

The WebClientModule has been enhanced to support more flexible polling strategies, including:

1. **Second-precise polling intervals** - Resources can be polled at intervals specified in seconds (not just minutes)
2. **Pause/Resume functionality** - Individual resources can be paused and resumed dynamically
3. **Priority scheduling** - Time-critical resources can be marked with priority to execute immediately when due

These enhancements enable modules like SofaScoreLiveModule to:
- Poll live match data frequently (e.g., every 10 seconds) during live events
- Pause scheduled event polling while live matches are in progress
- Resume normal polling after live matches end

## New API Methods

### Second-Precise Resource Registration

```cpp
// Register a resource with second-precise interval (no custom headers)
void registerResourceSeconds(
    const String& url,                      // The URL to poll
    uint32_t update_interval_seconds,       // Interval in SECONDS (not minutes)
    bool with_priority = false,             // Whether this resource has priority
    const char* root_ca = nullptr           // Optional root CA certificate
);

// Register a resource with second-precise interval AND custom headers
void registerResourceSecondsWithHeaders(
    const String& url,                      // The URL to poll
    const String& customHeaders,            // Custom headers (format: "Header1: Value1\nHeader2: Value2")
    uint32_t update_interval_seconds,       // Interval in SECONDS (not minutes)
    bool with_priority = false,             // Whether this resource has priority
    const char* root_ca = nullptr           // Optional root CA certificate
);
```

**Example Usage:**
```cpp
// Poll live match endpoint every 10 seconds with priority
webClient->registerResourceSeconds(
    "https://api.sofascore.com/api/v1/sport/darts/live-events",
    10,      // 10 seconds
    true     // with priority
);
```

### Pause and Resume

```cpp
// Pause a resource (stops polling until resumed)
void pauseResource(const String& url);

// Pause a resource with custom headers
void pauseResourceWithHeaders(const String& url, const String& customHeaders);

// Resume a paused resource
void resumeResource(const String& url);

// Resume a paused resource with custom headers
void resumeResourceWithHeaders(const String& url, const String& customHeaders);
```

**Example Usage:**
```cpp
// Pause scheduled events during live matches
webClient->pauseResource("https://api.sofascore.com/api/v1/sport/darts/scheduled-events/2024-12-15");

// Resume after live match ends
webClient->resumeResource("https://api.sofascore.com/api/v1/sport/darts/scheduled-events/2024-12-15");
```

## Implementation Details

### ManagedResource Structure Changes

The `ManagedResource` struct now includes:

```cpp
struct ManagedResource {
    // ... existing fields ...
    unsigned long last_check_attempt_ms = 0;  // Millisecond-precise timestamp
    bool is_paused = false;                   // Pause state
    bool has_priority = false;                // Priority flag
};
```

### Scheduling Algorithm

The worker task now implements a two-pass scheduling algorithm:

#### First Pass: Priority Resources
1. Scan all resources for priority-flagged resources that are due
2. If found, execute immediately (bypassing the 10-second minimum pause)
3. Use millisecond-precise timing for accurate scheduling

#### Second Pass: Normal Resources
1. Skip paused resources
2. Check if resource is due based on interval
3. Respect the 10-second minimum pause between downloads
4. Process only one resource per iteration to avoid overload

### Timing Precision

- **Second-precise intervals**: Resources registered with `registerResourceSeconds()` use millisecond timestamps (`last_check_attempt_ms`) for accurate interval tracking
- **Legacy support**: Resources registered with the original `registerResource()` method continue to use second-based timestamps
- **Priority resources**: Bypass the normal 10-second minimum pause when due, allowing faster response times

## Use Case: SofaScore Live Matches

The enhancements were designed specifically to support the SofaScore module's requirements:

### During Normal Operation
```cpp
// Poll scheduled events once per hour
webClient->registerResource(
    "https://api.sofascore.com/api/v1/sport/darts/scheduled-events/...",
    60  // 60 minutes
);
```

### When Live Match Starts
```cpp
// 1. Pause scheduled events
webClient->pauseResource("https://api.sofascore.com/.../scheduled-events/...");

// 2. Start high-frequency live polling with priority
webClient->registerResourceSeconds(
    "https://api.sofascore.com/api/v1/sport/darts/live-events",
    10,     // Every 10 seconds
    true    // With priority
);
```

### When Live Match Ends
```cpp
// 1. Resume scheduled events
webClient->resumeResource("https://api.sofascore.com/.../scheduled-events/...");

// 2. Live events resource can remain registered but will only fetch when matches are live
```

## Backward Compatibility

All existing code continues to work without modification:

- `registerResource()` - Still works with minute-based intervals
- `registerResourceWithHeaders()` - Still works with minute-based intervals  
- Existing resources are NOT affected by the new priority scheduling

## Performance Considerations

### Priority Resources
- Can bypass the 10-second minimum pause between downloads
- Should be used sparingly to avoid overwhelming the network
- Ideal for time-critical data (live match updates, etc.)

### Paused Resources
- Consume no network bandwidth while paused
- Data remains cached and accessible via `accessResource()`
- Useful for temporarily disabling polling during specific conditions

### Millisecond Timing
- Uses `millis()` for precise interval tracking
- Handles millis() rollover (occurs every ~49 days)
- More accurate than second-based timing for short intervals

## Thread Safety

All new methods are thread-safe:
- Pause/resume operations acquire the resource mutex
- Worker task respects paused state and priority flags
- No race conditions when modifying resource state

## Logging

The enhanced WebClient provides detailed logging:

```
[WebDataManager] Ressource registriert (Sekunden-genau): https://api.example.com, Intervall: 10 Sek, Priorität: 1
[WebDataManager] Ressource pausiert: https://api.example.com
[WebDataManager] Ressource fortgesetzt: https://api.example.com
[WebDataManager] Prioritäts-Ressource wird ausgeführt: https://api.example.com
```

## Testing Recommendations

When implementing new modules using these features:

1. **Start conservative**: Use longer intervals initially, then optimize
2. **Monitor logs**: Watch for "Prioritäts-Ressource" messages to verify priority execution
3. **Test pause/resume**: Verify data remains accessible while paused
4. **Check timing**: Verify millisecond-precise intervals are respected
5. **Avoid overload**: Don't register too many high-frequency resources

## Future Enhancements

Potential future improvements:

- Dynamic interval adjustment based on data staleness
- Resource groups for batch pause/resume operations
- Configurable minimum pause between downloads
- Statistics tracking (successful polls, failures, average response time)
