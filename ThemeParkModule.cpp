#include "ThemeParkModule.hpp"
#include "MultiLogger.hpp"
#include "WebClientModule.hpp"
#include "webconfig.hpp"
#include <ArduinoJson.h>
#include <algorithm>
#include <LittleFS.h>

// Allocator for PSRAM
struct SpiRamAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override { return ps_malloc(size); }
    void deallocate(void* pointer) override { free(pointer); }
    void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
};

ThemeParkModule::ThemeParkModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, WebClientModule* webClient)
    : _u8g2(u8g2), _canvas(canvas), _webClient(webClient), _config(nullptr),
      _currentPage(0), _currentParkIndex(0), _currentAttractionPage(0), _totalPages(1), 
      _logicTicksSincePageSwitch(0), _pageDisplayDuration(15000), _lastUpdate(0), 
      _lastParksListUpdate(0), _lastParkDetailsUpdate(0),
      _parkNameScrollOffset(0), _parkNameMaxScroll(0), _lastScrollStep(0) {
    _dataMutex = xSemaphoreCreateMutex();
}

ThemeParkModule::~ThemeParkModule() {
    if (_dataMutex) vSemaphoreDelete(_dataMutex);
}

void ThemeParkModule::begin() {
    loadParkCache();
    
    // If cache is empty, trigger immediate fetch of parks list
    if (_availableParks.empty()) {
        Log.println("[ThemePark] Cache is empty, will fetch parks list on first queueData call");
        _lastParksListUpdate = 0;  // Ensure checkAndUpdateParksList will load cache or fetch
    }
    
    Log.println("[ThemePark] Module initialized");
}

void ThemeParkModule::setConfig(const DeviceConfig* config) {
    _config = config;
    _pageDisplayDuration = (_config && _config->themeParkDisplaySec > 0) 
        ? _config->themeParkDisplaySec * 1000UL : 15000;
    
    if (!_webClient || !_config) return;
    
    // Parse park IDs from config
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        _parkIds.clear();
        if (!_config->themeParkIds.empty()) {
            PsramString parkIds = _config->themeParkIds;
            size_t pos = 0;
            while (pos < parkIds.length()) {
                size_t commaPos = parkIds.find(',', pos);
                if (commaPos == PsramString::npos) commaPos = parkIds.length();
                
                PsramString parkId = parkIds.substr(pos, commaPos - pos);
                // Trim whitespace
                while (!parkId.empty() && (parkId[0] == ' ' || parkId[0] == '\t')) {
                    parkId = parkId.substr(1);
                }
                while (!parkId.empty() && (parkId[parkId.length()-1] == ' ' || parkId[parkId.length()-1] == '\t')) {
                    parkId = parkId.substr(0, parkId.length()-1);
                }
                
                if (!parkId.empty()) {
                    _parkIds.push_back(parkId);
                }
                
                pos = commaPos + 1;
            }
        }
        xSemaphoreGive(_dataMutex);
    }
    
    // Register resources for each configured park
    // Each park needs wait times data from the /waitingtimes endpoint
    unsigned long updateIntervalMinutes = (_config && _config->themeParkFetchIntervalMin > 0) 
        ? _config->themeParkFetchIntervalMin : 10;
    
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (const auto& parkId : _parkIds) {
            // Wait times endpoint uses header-based park selection
            PsramString waitTimesUrl = "https://api.wartezeiten.app/v1/waitingtimes";
            PsramString waitTimesHeaders = "accept: application/json\npark: " + parkId + "\nlanguage: de";
            
            // Register the wait times resource with custom headers
            _webClient->registerResourceWithHeaders(String(waitTimesUrl.c_str()), 
                                                    String(waitTimesHeaders.c_str()), 
                                                    updateIntervalMinutes, 
                                                    nullptr);
            
            // Register opening times resource (updated less frequently - every 6 hours)
            PsramString openingTimesUrl = "https://api.wartezeiten.app/v1/openingtimes";
            PsramString openingTimesHeaders = "accept: application/json\npark: " + parkId;
            _webClient->registerResourceWithHeaders(String(openingTimesUrl.c_str()),
                                                    String(openingTimesHeaders.c_str()),
                                                    360,  // 6 hours
                                                    nullptr);
            
            // Register crowd level resource (updated every 30 minutes)
            PsramString crowdLevelUrl = "https://api.wartezeiten.app/v1/crowdlevel";
            PsramString crowdLevelHeaders = "accept: application/json\npark: " + parkId + "\nlanguage: de";
            _webClient->registerResourceWithHeaders(String(crowdLevelUrl.c_str()),
                                                    String(crowdLevelHeaders.c_str()),
                                                    30,  // 30 minutes
                                                    nullptr);
            
            Log.printf("[ThemePark] Registered resources for park %s\n", parkId.c_str());
        }
        xSemaphoreGive(_dataMutex);
    }
}

void ThemeParkModule::queueData() {
    if (!_webClient || !_config) return;
    
    // Check if parks list needs daily update
    checkAndUpdateParksList();
    
    // Fetch wait times for each configured park
    // Create a copy of park IDs to avoid holding the mutex during network operations
    PsramVector<PsramString> parkIdsCopy;
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        parkIdsCopy = _parkIds;
        xSemaphoreGive(_dataMutex);
    }
    
    for (const auto& parkId : parkIdsCopy) {
        // Fetch wait times from /waitingtimes endpoint
        PsramString waitTimesUrl = "https://api.wartezeiten.app/v1/waitingtimes";
        PsramString waitTimesHeaders = "accept: application/json\npark: " + parkId + "\nlanguage: de";
        
        // Access the resource - this will use cached data if still fresh,
        // or trigger a new fetch if the update interval has passed
        _webClient->accessResource(String(waitTimesUrl.c_str()), String(waitTimesHeaders.c_str()),
            [this, parkId](const char* buffer, size_t size, time_t last_update, bool is_stale) {
                if (buffer && size > 0) {
                    // Only process if this is new data (not already processed)
                    bool shouldProcess = false;
                    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        auto it = _lastProcessedWaitTimes.find(parkId);
                        if (it == _lastProcessedWaitTimes.end() || it->second < last_update) {
                            shouldProcess = true;
                            _lastProcessedWaitTimes[parkId] = last_update;
                        }
                        xSemaphoreGive(_dataMutex);
                    }
                    
                    if (shouldProcess) {
                        // Take mutex before parsing to ensure thread safety
                        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            parseWaitTimes(buffer, size, parkId);
                            xSemaphoreGive(_dataMutex);
                            
                            // Trigger a redraw if callback is set
                            if (_updateCallback) {
                                _updateCallback();
                            }
                        }
                    }
                }
            });
        
        // Fetch opening times
        PsramString openingTimesUrl = "https://api.wartezeiten.app/v1/openingtimes";
        PsramString openingTimesHeaders = "accept: application/json\npark: " + parkId;
        
        _webClient->accessResource(String(openingTimesUrl.c_str()), String(openingTimesHeaders.c_str()),
            [this, parkId](const char* buffer, size_t size, time_t last_update, bool is_stale) {
                if (buffer && size > 0) {
                    // Only process if this is new data (not already processed)
                    bool shouldProcess = false;
                    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        auto it = _lastProcessedOpeningTimes.find(parkId);
                        if (it == _lastProcessedOpeningTimes.end() || it->second < last_update) {
                            shouldProcess = true;
                            _lastProcessedOpeningTimes[parkId] = last_update;
                        }
                        xSemaphoreGive(_dataMutex);
                    }
                    
                    if (shouldProcess) {
                        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            parseOpeningTimes(buffer, size, parkId);
                            xSemaphoreGive(_dataMutex);
                            
                            // Trigger a redraw if callback is set
                            if (_updateCallback) {
                                _updateCallback();
                            }
                        }
                    }
                }
            });
        
        // Fetch crowd level
        PsramString crowdLevelUrl = "https://api.wartezeiten.app/v1/crowdlevel";
        PsramString crowdLevelHeaders = "accept: application/json\npark: " + parkId + "\nlanguage: de";
        
        _webClient->accessResource(String(crowdLevelUrl.c_str()), String(crowdLevelHeaders.c_str()),
            [this, parkId](const char* buffer, size_t size, time_t last_update, bool is_stale) {
                if (buffer && size > 0) {
                    // Only process if this is new data (not already processed)
                    bool shouldProcess = false;
                    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        auto it = _lastProcessedCrowdLevel.find(parkId);
                        if (it == _lastProcessedCrowdLevel.end() || it->second < last_update) {
                            shouldProcess = true;
                            _lastProcessedCrowdLevel[parkId] = last_update;
                        }
                        xSemaphoreGive(_dataMutex);
                    }
                    
                    if (shouldProcess) {
                        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                            parseCrowdLevel(buffer, size, parkId);
                            xSemaphoreGive(_dataMutex);
                            
                            // Trigger a redraw if callback is set
                            if (_updateCallback) {
                                _updateCallback();
                            }
                        }
                    }
                }
            });
    }
}

void ThemeParkModule::processData() {
    // Data processing is now handled directly in queueData() callback
    // This keeps it simple like the Calendar module pattern
}

void ThemeParkModule::parseAvailableParks(const char* jsonBuffer, size_t size) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, jsonBuffer, size);
    if (error) {
        Log.printf("[ThemePark] Failed to parse parks list: %s\n", error.c_str());
        return;
    }
    
    _availableParks.clear();
    
    JsonArray parks = doc.as<JsonArray>();
    for (JsonObject park : parks) {
        const char* id = park["id"] | "";
        const char* name = park["name"] | "";
        const char* country = park["land"] | "";  // API uses "land" not "country"
        
        if (id && name && strlen(id) > 0 && strlen(name) > 0) {
            _availableParks.push_back(AvailablePark(id, name, country));
        }
    }
    
    // Save park cache to LittleFS for later use
    saveParkCache();
    
    // Update timestamp when parks list is manually refreshed (e.g., via web button)
    // This ensures the daily auto-update timer is reset
    _lastParksListUpdate = time(nullptr);
    
    Log.printf("[ThemePark] Loaded %d available parks\n", _availableParks.size());
}

void ThemeParkModule::parseWaitTimes(const char* jsonBuffer, size_t size, const PsramString& parkId) {
    Log.printf("[ThemePark] parseWaitTimes called for park: %s\n", parkId.c_str());
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, jsonBuffer, size);
    if (error) {
        Log.printf("[ThemePark] Failed to parse wait times: %s\n", error.c_str());
        return;
    }
    
    // The API returns an array of attractions, not park metadata
    // We need to get the park name and country from our cache
    Log.printf("[ThemePark] About to lookup name for: %s\n", parkId.c_str());
    PsramString parkName = getParkNameFromCache(parkId);
    if (parkName.empty()) {
        parkName = parkId;  // Fallback to ID if name not found
        Log.printf("[ThemePark] Using parkId as fallback name: %s\n", parkId.c_str());
    }
    
    Log.printf("[ThemePark] About to lookup country for: %s\n", parkId.c_str());
    PsramString parkCountry = getParkCountryFromCache(parkId);
    
    // Find or create park data
    ThemeParkData* parkData = nullptr;
    for (auto& park : _parkData) {
        if (park.id == parkId) {
            parkData = &park;
            break;
        }
    }
    
    if (!parkData) {
        _parkData.push_back(ThemeParkData());
        parkData = &_parkData[_parkData.size() - 1];
    }
    
    parkData->id = parkId;
    parkData->name = parkName;
    parkData->country = parkCountry;
    parkData->attractions.clear();
    parkData->lastUpdate = time(nullptr);
    
    // Note: The API response doesn't include crowdLevel, isOpen, openingTime, closingTime
    // These fields will remain at their default values (0/false/empty)
    // If needed, these could be fetched from a different API endpoint
    
    // Parse attractions array
    JsonArray attractions = doc.as<JsonArray>();
    int openAttractionsCount = 0;
    if (attractions) {
        for (JsonObject attr : attractions) {
            const char* name = attr["name"] | "";
            int waitTime = attr["waitingtime"] | 0;  // Note: field is "waitingtime" not "waitTime"
            const char* status = attr["status"] | "unknown";
            
            if (name && strlen(name) > 0) {
                Attraction attraction;
                attraction.name = name;
                attraction.waitTime = waitTime;
                attraction.isOpen = (strcmp(status, "opened") == 0);  // "opened" means open
                parkData->attractions.push_back(attraction);
                
                if (attraction.isOpen) {
                    openAttractionsCount++;
                }
            }
        }
    }
    
    // Check if park should be considered closed:
    // Park is closed if all attractions are closed
    bool allAttractionsClosed = (parkData->attractions.size() > 0 && openAttractionsCount == 0);
    
    // Don't remove parks here - opening times might not be fetched yet due to async callbacks
    // The display logic will handle whether to show closed parks based on opening hours
    if (allAttractionsClosed) {
        Log.printf("[ThemePark] Park %s is closed (all attractions closed)\n", parkName.c_str());
    }
    
    // Sort attractions by multiple criteria:
    // 1. Open status (open attractions first)
    // 2. Wait time (higher wait time first) - even 0 min counts if open
    // 3. Name (alphabetical as tiebreaker, case-insensitive)
    std::sort(parkData->attractions.begin(), parkData->attractions.end(),
              [](const Attraction& a, const Attraction& b) {
                  // Criterion 1: Open status (open first)
                  if (a.isOpen != b.isOpen) {
                      return a.isOpen > b.isOpen;  // true > false, so open attractions come first
                  }
                  
                  // Criterion 2: Wait time (higher first)
                  if (a.waitTime != b.waitTime) {
                      return a.waitTime > b.waitTime;
                  }
                  
                  // Criterion 3: Name (alphabetical, case-insensitive)
                  // Convert to lowercase for comparison
                  PsramString aLower = a.name;
                  PsramString bLower = b.name;
                  std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
                  std::transform(bLower.begin(), bLower.end(), bLower.begin(), ::tolower);
                  return aLower < bLower;
              });
    
    // Calculate how many pages needed to show all attractions
    // 6 attractions per page as requested
    int attractionsPerPage = 6;
    parkData->attractionPages = (parkData->attractions.size() + attractionsPerPage - 1) / attractionsPerPage;
    if (parkData->attractionPages < 1) parkData->attractionPages = 1;
    
    // Calculate total pages across all parks
    _totalPages = 0;
    for (const auto& park : _parkData) {
        _totalPages += park.attractionPages;
    }
    
    Log.printf("[ThemePark] Updated park %s with %d attractions (%d open), %d pages\n", 
               parkName.c_str(), parkData->attractions.size(), openAttractionsCount, parkData->attractionPages);
}

void ThemeParkModule::parseCrowdLevel(const char* jsonBuffer, size_t size, const PsramString& parkId) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, jsonBuffer, size);
    if (error) {
        Log.printf("[ThemePark] Failed to parse crowd level: %s\n", error.c_str());
        return;
    }
    
    // Extract crowd level from JSON
    float crowdLevel = doc["crowd_level"] | 0.0f;
    
    // Find the park data and update crowd level
    for (auto& park : _parkData) {
        if (park.id == parkId) {
            park.crowdLevel = crowdLevel;
            Log.printf("[ThemePark] Updated crowd level for %s: %.2f%%\n", parkId.c_str(), crowdLevel);
            break;
        }
    }
}

void ThemeParkModule::parseOpeningTimes(const char* jsonBuffer, size_t size, const PsramString& parkId) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, jsonBuffer, size);
    if (error) {
        Log.printf("[ThemePark] Failed to parse opening times: %s\n", error.c_str());
        return;
    }
    
    // API returns array like: [{"opened_today":true,"open_from":"2025-11-14T09:00:00+01:00","closed_from":"2025-11-14T18:00:00+01:00"}]
    JsonArray timesArray = doc.as<JsonArray>();
    if (!timesArray || timesArray.size() == 0) {
        Log.printf("[ThemePark] No opening times data for park %s\n", parkId.c_str());
        return;
    }
    
    JsonObject today = timesArray[0];
    bool openedToday = today["opened_today"] | false;
    const char* openFrom = today["open_from"] | "";
    const char* closedFrom = today["closed_from"] | "";
    
    // Find the park data and update opening times
    for (auto& park : _parkData) {
        if (park.id == parkId) {
            park.isOpen = openedToday;
            
            // Extract just the time portion (HH:MM) from ISO 8601 timestamp
            // Format is "2025-11-14T09:00:00+01:00", we want "09:00"
            if (openFrom && strlen(openFrom) >= 16) {
                PsramString openStr(openFrom);
                size_t tPos = openStr.find('T');
                if (tPos != PsramString::npos && tPos + 6 <= openStr.length()) {
                    park.openingTime = openStr.substr(tPos + 1, 5);  // Extract "09:00"
                }
            }
            
            if (closedFrom && strlen(closedFrom) >= 16) {
                PsramString closedStr(closedFrom);
                size_t tPos = closedStr.find('T');
                if (tPos != PsramString::npos && tPos + 6 <= closedStr.length()) {
                    park.closingTime = closedStr.substr(tPos + 1, 5);  // Extract "18:00"
                }
            }
            
            Log.printf("[ThemePark] Updated opening times for %s: %s - %s (open: %s)\n", 
                      parkId.c_str(), park.openingTime.c_str(), park.closingTime.c_str(), 
                      park.isOpen ? "yes" : "no");
            break;
        }
    }
}

void ThemeParkModule::onUpdate(std::function<void()> callback) {
    _updateCallback = callback;
}

bool ThemeParkModule::isEnabled() {
    return _config && _config->themeParkEnabled;
}

unsigned long ThemeParkModule::getDisplayDuration() {
    unsigned long duration;
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Sum all pages across all displayable parks
        // Closed parks count as 1 page (showing "Geschlossen" message)
        // Open parks count based on their attraction pages
        int totalPages = 0;
        for (const auto& park : _parkData) {
            if (shouldDisplayPark(park)) {
                // Check if park has any open attractions
                bool hasOpenAttractions = false;
                for (const auto& attr : park.attractions) {
                    if (attr.isOpen) {
                        hasOpenAttractions = true;
                        break;
                    }
                }
                
                if (hasOpenAttractions) {
                    // Open park: count all attraction pages
                    totalPages += park.attractionPages;
                } else {
                    // Closed park: count as 1 page (showing closed message)
                    totalPages += 1;
                }
            }
        }
        
        if (totalPages == 0) totalPages = 1;
        duration = (totalPages * _pageDisplayDuration) + runtime_safe_buffer;
        xSemaphoreGive(_dataMutex);
    } else {
        duration = _pageDisplayDuration;
    }
    return duration;
}

void ThemeParkModule::resetPaging() {
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        _currentPage = 0;
        _currentParkIndex = 0;
        _currentAttractionPage = 0;
        _logicTicksSincePageSwitch = 0;
        _isFinished = false;
        xSemaphoreGive(_dataMutex);
    }
}

void ThemeParkModule::configure(const ModuleConfig& config) {
    _modConfig = config;
}

void ThemeParkModule::onActivate() {
    if (_modConfig.resetOnActivate) {
        _currentPage = 0;
    }
    _logicTicksSincePageSwitch = 0;
}

bool ThemeParkModule::shouldDisplayPark(const ThemeParkData& park) const {
    // Always display parks with open attractions
    bool hasOpenAttractions = false;
    for (const auto& attr : park.attractions) {
        if (attr.isOpen) {
            hasOpenAttractions = true;
            break;
        }
    }
    
    if (hasOpenAttractions) {
        return true;
    }
    
    // If all attractions are closed, display if we have opening OR closing time information
    // This allows users to see when the park will reopen
    return !park.openingTime.empty() || !park.closingTime.empty();
}

void ThemeParkModule::tick() {
    // Handle scrolling with global scroll speed from config
    if (!_config || _parkNameMaxScroll == 0) return;
    
    unsigned long scrollInterval = _config->globalScrollSpeedMs > 0 ? _config->globalScrollSpeedMs : 50;
    unsigned long now = millis();
    
    if (now - _lastScrollStep >= scrollInterval) {
        _lastScrollStep = now;
        _parkNameScrollOffset++;
        if (_parkNameScrollOffset >= _parkNameMaxScroll) {
            _parkNameScrollOffset = 0;
        }
        if (_updateCallback) _updateCallback();
    }
}

void ThemeParkModule::logicTick() {
    _logicTicksSincePageSwitch++;
    
    uint32_t ticksPerPage = (_pageDisplayDuration / 100);
    
    if (_logicTicksSincePageSwitch >= ticksPerPage) {
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Filter displayable parks
            PsramVector<int> displayableIndices;
            for (size_t i = 0; i < _parkData.size(); i++) {
                if (shouldDisplayPark(_parkData[i])) {
                    displayableIndices.push_back(i);
                }
            }
            
            // Sort displayable parks:
            // 1. Open parks first (those with at least one open attraction)
            // 2. Then closed parks with opening times
            // 3. Alphabetically by name within each group (case-insensitive)
            std::sort(displayableIndices.begin(), displayableIndices.end(),
                      [this](int idxA, int idxB) {
                          const auto& parkA = _parkData[idxA];
                          const auto& parkB = _parkData[idxB];
                          
                          // Check if parks have open attractions
                          bool aHasOpen = false;
                          bool bHasOpen = false;
                          for (const auto& attr : parkA.attractions) {
                              if (attr.isOpen) { aHasOpen = true; break; }
                          }
                          for (const auto& attr : parkB.attractions) {
                              if (attr.isOpen) { bHasOpen = true; break; }
                          }
                          
                          // Criterion 1: Open parks first
                          if (aHasOpen != bHasOpen) {
                              return aHasOpen > bHasOpen;  // Open parks first
                          }
                          
                          // Criterion 2: Alphabetically by name (case-insensitive)
                          PsramString aLower = parkA.name;
                          PsramString bLower = parkB.name;
                          std::transform(aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
                          std::transform(bLower.begin(), bLower.end(), bLower.begin(), ::tolower);
                          return aLower < bLower;
                      });
            
            if (displayableIndices.empty()) {
                _currentPage = 0;
                _currentParkIndex = 0;
                _currentAttractionPage = 0;
                _isFinished = true;
                xSemaphoreGive(_dataMutex);
                _logicTicksSincePageSwitch = 0;
                return;
            }
            
            // Get the actual park index we're currently on
            int displayIndex = _currentParkIndex % displayableIndices.size();
            int actualParkIndex = displayableIndices[displayIndex];
            
            // Check if current park is closed (for paging logic)
            bool parkIsClosed = true;
            for (const auto& attr : _parkData[actualParkIndex].attractions) {
                if (attr.isOpen) {
                    parkIsClosed = false;
                    break;
                }
            }
            
            // Determine pages for current park
            int pagesForThisPark = parkIsClosed ? 1 : _parkData[actualParkIndex].attractionPages;
            
            // Move to next attraction page within current park
            _currentAttractionPage++;
            _parkNameScrollOffset = 0;  // Reset scroll when changing page
            
            // If we've shown all pages for current park, move to next park
            if (_currentAttractionPage >= pagesForThisPark) {
                _currentAttractionPage = 0;
                _currentParkIndex++;
                
                // If we've shown all displayable parks, we're done
                if (_currentParkIndex >= (int)displayableIndices.size()) {
                    _currentParkIndex = 0;
                    _currentPage = 0;
                    _isFinished = true;
                    Log.printf("[ThemePark] All %d displayable parks shown -> Module finished\n", (int)displayableIndices.size());
                } else {
                    _currentPage++;
                    if (_updateCallback) _updateCallback();
                }
            } else {
                _currentPage++;
                if (_updateCallback) _updateCallback();
            }
            
            xSemaphoreGive(_dataMutex);
        }
        _logicTicksSincePageSwitch = 0;
    }
}

void ThemeParkModule::draw() {
    _canvas.fillScreen(0);
    _u8g2.begin(_canvas);  // Initialize u8g2 with canvas - required for proper rendering
    
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Filter displayable parks
        PsramVector<int> displayableIndices;
        for (size_t i = 0; i < _parkData.size(); i++) {
            if (shouldDisplayPark(_parkData[i])) {
                displayableIndices.push_back(i);
            }
        }
        
        if (displayableIndices.empty()) {
            drawNoDataPage();
        } else {
            // Find which displayable park we should show based on current index
            int displayIndex = _currentParkIndex % displayableIndices.size();
            drawParkPage(displayableIndices[displayIndex], _currentAttractionPage);
        }
        xSemaphoreGive(_dataMutex);
    } else {
        drawNoDataPage();
    }
}

void ThemeParkModule::drawParkPage(int parkIndex, int attractionPage) {
    if (parkIndex < 0 || parkIndex >= (int)_parkData.size()) {
        drawNoDataPage();
        return;
    }
    
    const ThemeParkData& park = _parkData[parkIndex];
    
    // Check if this park should be displayed
    if (!shouldDisplayPark(park)) {
        drawNoDataPage();
        return;
    }
    
    // Check if park has any open attractions
    bool hasOpenAttractions = false;
    for (const auto& attr : park.attractions) {
        if (attr.isOpen) {
            hasOpenAttractions = true;
            break;
        }
    }
    
    // Draw park name with country as headline - scrolling text
    // Use smaller font (6x13 like CalendarModule uses) and scrolling
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setForegroundColor(0xFFFF);
    
    PsramString displayName = park.name;
    if (!park.country.empty()) {
        displayName = displayName + " (" + park.country + ")";
    }
    
    // Add opening hours to scrolling text only if park is open
    if (hasOpenAttractions && !park.openingTime.empty() && !park.closingTime.empty()) {
        // Show opening hours in format: "Geöffnet von HH:MM - HH:MM Uhr"
        displayName = displayName + " : Geöffnet von " + park.openingTime + " - " + park.closingTime + " Uhr";
    }
    
    int maxNameWidth = _canvas.width() - 50;  // Leave space for crowd level
    drawScrollingText(displayName, 2, 11, maxNameWidth);
    
    // Draw crowd level as percentage text (no box)
    // Crowd level from API is 0-100 scale
    // Color: green <= 40%, yellow 41-60%, red > 60%
    // Only show if > 0 (API returns 0 for closed parks)
    if (park.crowdLevel > 0.0f) {
        uint16_t crowdColor;
        if (park.crowdLevel <= 40.0f) {
            crowdColor = 0x07E0;  // Green
        } else if (park.crowdLevel <= 60.0f) {
            crowdColor = 0xFFE0;  // Yellow
        } else {
            crowdColor = 0xF800;  // Red
        }
        
        _u8g2.setFont(u8g2_font_6x10_tf);
        _u8g2.setForegroundColor(crowdColor);
        char crowdText[8];
        snprintf(crowdText, sizeof(crowdText), "%.0f%%", park.crowdLevel);
        int crowdW = _u8g2.getUTF8Width(crowdText);
        _u8g2.setCursor(_canvas.width() - crowdW - 2, 11);
        _u8g2.print(crowdText);
    }
    
    // If park is closed (no open attractions), show closed message instead of attraction list
    if (!hasOpenAttractions) {
        int yPos = 16;
        _u8g2.setFont(u8g2_font_9x15_tf);
        _u8g2.setForegroundColor(0xF800);  // Red
        
        yPos += 10;
        const char* closedMsg = "Geschlossen";
        int closedW = _u8g2.getUTF8Width(closedMsg);
        _u8g2.setCursor((_canvas.width() - closedW) / 2, yPos);
        _u8g2.print(closedMsg);
        
        // Show when park reopens if opening hours available
        if (!park.openingTime.empty() && !park.closingTime.empty()) {
            yPos += 20;
            _u8g2.setFont(u8g2_font_6x13_tf);
            _u8g2.setForegroundColor(0xFFFF);  // White
            
            PsramString reopenMsg = "Öffnet wieder von";
            int reopenW = _u8g2.getUTF8Width(reopenMsg.c_str());
            _u8g2.setCursor((_canvas.width() - reopenW) / 2, yPos);
            _u8g2.print(reopenMsg.c_str());
            
            yPos += 16;
            PsramString timeMsg = park.openingTime + " - " + park.closingTime + " Uhr";
            int timeW = _u8g2.getUTF8Width(timeMsg.c_str());
            _u8g2.setCursor((_canvas.width() - timeW) / 2, yPos);
            _u8g2.print(timeMsg.c_str());
        }
        
        return;  // Don't draw attractions list
    }
    
    // Park is open - draw attractions using smaller font like CalendarModule
    // Start position moved down by 2 more pixels
    int yPos = 16;
    _u8g2.setFont(u8g2_font_5x8_tf);
    _u8g2.setForegroundColor(0xFFFF);
    
    yPos += 4;  // Gap before attractions
    int lineHeight = 8;
    
    // Force exactly 6 attractions per page as requested
    int maxLines = 6;
    
    // Calculate which attractions to show on this page
    int startIdx = attractionPage * maxLines;
    int endIdx = min(startIdx + maxLines, (int)park.attractions.size());
    
    for (int i = startIdx; i < endIdx; i++) {
        const Attraction& attr = park.attractions[i];
        
        // Truncate name to fit (leave space for wait time)
        PsramString attrName = truncateString(attr.name, _canvas.width() - 45);
        
        // Draw attraction name - use RED if closed, WHITE if open
        _u8g2.setForegroundColor(attr.isOpen ? 0xFFFF : 0xF800);  // White or Red
        _u8g2.setCursor(2, yPos);
        _u8g2.print(attrName.c_str());
        
        // Draw wait time or "Geschlossen" for closed attractions
        if (attr.isOpen) {
            char waitStr[10];
            snprintf(waitStr, sizeof(waitStr), "%d min", attr.waitTime);
            int waitW = _u8g2.getUTF8Width(waitStr);
            
            // Color code wait times
            uint16_t waitColor;
            if (attr.waitTime >= 60) {
                waitColor = 0xF800;  // Red
            } else if (attr.waitTime >= 30) {
                waitColor = 0xFD20;  // Orange
            } else if (attr.waitTime >= 15) {
                waitColor = 0xFFE0;  // Yellow
            } else {
                waitColor = 0x07E0;  // Green
            }
            
            _u8g2.setForegroundColor(waitColor);
            _u8g2.setCursor(_canvas.width() - waitW - 2, yPos);
            _u8g2.print(waitStr);
        } else {
            // Show "Geschlossen" in red for closed attractions
            const char* closedText = "Geschl.";
            int closedW = _u8g2.getUTF8Width(closedText);
            _u8g2.setForegroundColor(0xF800);  // Red
            _u8g2.setCursor(_canvas.width() - closedW - 2, yPos);
            _u8g2.print(closedText);
        }
        
        _u8g2.setForegroundColor(0xFFFF);
        yPos += lineHeight;
    }
}

void ThemeParkModule::drawNoDataPage() {
    _u8g2.setFont(u8g2_font_9x15_tf);
    _u8g2.setForegroundColor(0xFFFF);
    
    const char* msg1 = "Freizeitpark";
    const char* msg2 = "Keine Daten";
    
    int w1 = _u8g2.getUTF8Width(msg1);
    int w2 = _u8g2.getUTF8Width(msg2);
    
    _u8g2.setCursor((_canvas.width() - w1) / 2, _canvas.height() / 2 - 8);
    _u8g2.print(msg1);
    _u8g2.setCursor((_canvas.width() - w2) / 2, _canvas.height() / 2 + 8);
    _u8g2.print(msg2);
}

uint16_t ThemeParkModule::getCrowdLevelColor(float level) {
    // Level is 0-100 scale from API
    if (level <= 40.0f) return 0x07E0;      // Green
    if (level <= 60.0f) return 0xFFE0;      // Yellow
    return 0xF800;                          // Red
}

PsramString ThemeParkModule::truncateString(const PsramString& text, int maxWidth) {
    int textWidth = _u8g2.getUTF8Width(text.c_str());
    if (textWidth <= maxWidth) {
        return text;
    }
    
    PsramString truncated = text;
    while (textWidth > maxWidth && truncated.length() > 3) {
        truncated = truncated.substr(0, truncated.length() - 1);
        PsramString withDots = truncated + "...";
        textWidth = _u8g2.getUTF8Width(withDots.c_str());
    }
    
    return truncated + "...";
}

void ThemeParkModule::drawScrollingText(const PsramString& text, int x, int y, int maxWidth) {
    PsramString visiblePart = fitTextToPixelWidth(text, maxWidth);
    
    if (text.length() > visiblePart.length()) {
        // Text needs scrolling
        PsramString pad("     ");  // 5 spaces for smoother scrolling
        PsramString scrollText = text + pad + text.substr(0, visiblePart.length());
        _parkNameMaxScroll = scrollText.length() - visiblePart.length();
        
        if (_parkNameScrollOffset >= _parkNameMaxScroll) {
            _parkNameScrollOffset = 0;
        }
        
        PsramString part = scrollText.substr(_parkNameScrollOffset, visiblePart.length());
        _u8g2.setCursor(x, y);
        _u8g2.print(part.c_str());
    } else {
        // Text fits, no scrolling needed
        _parkNameMaxScroll = 0;
        _parkNameScrollOffset = 0;
        _u8g2.setCursor(x, y);
        _u8g2.print(text.c_str());
    }
}

PsramString ThemeParkModule::fitTextToPixelWidth(const PsramString& text, int maxPixel) {
    int lastOk = 0;
    for (int i = 1; i <= (int)text.length(); ++i) {
        if (_u8g2.getUTF8Width(text.substr(0, i).c_str()) <= maxPixel) {
            lastOk = i;
        } else {
            break;
        }
    }
    return text.substr(0, lastOk);
}

PsramVector<AvailablePark> ThemeParkModule::getAvailableParks() {
    PsramVector<AvailablePark> parks;
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        parks = _availableParks;
        xSemaphoreGive(_dataMutex);
    }
    return parks;
}

void ThemeParkModule::loadParkCache() {
    if (!LittleFS.exists("/park_cache.json")) {
        Log.println("[ThemePark] No park cache found");
        return;
    }
    
    File file = LittleFS.open("/park_cache.json", "r");
    if (!file) {
        Log.println("[ThemePark] Failed to open park cache");
        return;
    }
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Log.printf("[ThemePark] Failed to parse park cache: %s\n", error.c_str());
        return;
    }
    
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        _availableParks.clear();
        
        JsonArray parks = doc["parks"].as<JsonArray>();
        if (parks) {
            for (JsonObject park : parks) {
                const char* id = park["id"] | "";
                const char* name = park["name"] | "";
                // Try both "land" (from API) and "country" (from cache) for compatibility
                const char* country = park["land"] | park["country"] | "";
                
                if (id && name && strlen(id) > 0 && strlen(name) > 0) {
                    _availableParks.push_back(AvailablePark(id, name, country));
                    Log.printf("[ThemePark] Loaded from cache: id=%s, name=%s, country=%s\n", id, name, country);
                }
            }
        }
        
        xSemaphoreGive(_dataMutex);
    }
    
    Log.printf("[ThemePark] Loaded %d parks from cache\n", _availableParks.size());
}

void ThemeParkModule::saveParkCache() {
    File file = LittleFS.open("/park_cache.json", "w");
    if (!file) {
        Log.println("[ThemePark] Failed to create park cache file");
        return;
    }
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    JsonArray parks = doc["parks"].to<JsonArray>();
    
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (const auto& park : _availableParks) {
            JsonObject parkObj = parks.add<JsonObject>();
            parkObj["id"] = park.id.c_str();
            parkObj["name"] = park.name.c_str();
            parkObj["country"] = park.country.c_str();
        }
        xSemaphoreGive(_dataMutex);
    }
    
    serializeJson(doc, file);
    file.close();
    
    Log.printf("[ThemePark] Saved %d parks to cache\n", _availableParks.size());
}

PsramString ThemeParkModule::getParkNameFromCache(const PsramString& parkId) {
    // NOTE: This function expects _dataMutex to already be held by the caller
    PsramString name;
    
    Log.printf("[ThemePark] getParkNameFromCache: searching %d parks for %s\n", _availableParks.size(), parkId.c_str());
    for (const auto& park : _availableParks) {
        if (park.id == parkId) {
            name = park.name;
            Log.printf("[ThemePark] Found name for %s: %s\n", parkId.c_str(), name.c_str());
            break;
        }
    }
    if (name.empty()) {
        Log.printf("[ThemePark] No name found for %s in cache (%d parks)\n", parkId.c_str(), _availableParks.size());
    }
    
    return name;
}

PsramString ThemeParkModule::getParkCountryFromCache(const PsramString& parkId) {
    // NOTE: This function expects _dataMutex to already be held by the caller
    PsramString country;
    
    Log.printf("[ThemePark] getParkCountryFromCache: searching %d parks for %s\n", _availableParks.size(), parkId.c_str());
    for (const auto& park : _availableParks) {
        if (park.id == parkId) {
            country = park.country;
            Log.printf("[ThemePark] Found country for %s: %s\n", parkId.c_str(), country.c_str());
            break;
        }
    }
    if (country.empty()) {
        Log.printf("[ThemePark] No country found for %s in cache\n", parkId.c_str());
    }
    
    return country;
}

void ThemeParkModule::checkAndUpdateParksList() {
    if (!_webClient) return;
    
    time_t now = time(nullptr);
    
    // Check if we need to update (once per day = 86400 seconds)
    // If _lastParksListUpdate is 0, load from cache first
    if (_lastParksListUpdate == 0) {
        // First time - try to load from cache
        loadParkCache();
        
        // If cache is still empty after loading, fetch from API immediately
        bool cacheEmpty = false;
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            cacheEmpty = _availableParks.empty();
            xSemaphoreGive(_dataMutex);
        }
        
        if (cacheEmpty) {
            Log.println("[ThemePark] Cache empty, fetching parks list from API");
            // Don't set _lastParksListUpdate yet, let it fetch
        } else {
            _lastParksListUpdate = now;  // Set to now to prevent immediate fetch
            return;
        }
    } else if ((now - _lastParksListUpdate) < 86400) {
        return;  // Not yet time to update
    }
    
    Log.println("[ThemePark] Fetching parks list from API");
    
    PsramString url = "https://api.wartezeiten.app/v1/parks";
    PsramString headers = "accept: application/json\nlanguage: de";
    
    // Update timestamp BEFORE making request to prevent multiple simultaneous requests
    _lastParksListUpdate = now;
    
    // Fetch parks list asynchronously
    _webClient->getRequest(url, headers, [this](int httpCode, const char* payload, size_t len) {
        if (httpCode == 200 && payload && len > 0) {
            Log.printf("[ThemePark] Received parks list (size: %d)\n", len);
            parseAvailableParks(payload, len);
        } else {
            Log.printf("[ThemePark] Parks list fetch failed: HTTP %d\n", httpCode);
        }
    });
}
