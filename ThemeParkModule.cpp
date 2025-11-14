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
      _currentPage(0), _totalPages(1), _logicTicksSincePageSwitch(0),
      _pageDisplayDuration(15000), _lastUpdate(0), _lastParksListUpdate(0) {
    _dataMutex = xSemaphoreCreateMutex();
}

ThemeParkModule::~ThemeParkModule() {
    if (_dataMutex) vSemaphoreDelete(_dataMutex);
}

void ThemeParkModule::begin() {
    loadParkCache();
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
    
    // Register each park as a separate resource with WebClientModule
    PsramString url = "https://api.wartezeiten.app/v1/waitingtimes";
    uint32_t fetchIntervalMin = (_config->themeParkFetchIntervalMin > 0) 
        ? _config->themeParkFetchIntervalMin : 10;
    
    for (const auto& parkId : _parkIds) {
        PsramString headers = "accept: application/json\npark: " + parkId + "\nlanguage: de";
        _webClient->registerResourceWithHeaders(url.c_str(), headers.c_str(), fetchIntervalMin, nullptr);
        Log.printf("[ThemePark] Registered resource for park: %s\n", parkId.c_str());
    }
}

void ThemeParkModule::queueData() {
    if (!_webClient || !_config) return;
    
    // Check if parks list needs daily update
    checkAndUpdateParksList();
    
    // Access wait times data for each configured park from WebClientModule's cache
    PsramString url = "https://api.wartezeiten.app/v1/waitingtimes";
    
    for (const auto& parkId : _parkIds) {
        PsramString headers = "accept: application/json\npark: " + parkId + "\nlanguage: de";
        
        _webClient->accessResource(url.c_str(), headers.c_str(), 
            [this, parkId](const char* data, size_t size, time_t last_update, bool is_stale) {
                if (data && size > 0 && !is_stale && last_update > _lastUpdate) {
                    Log.printf("[ThemePark] New data available for park: %s (size: %d)\n", parkId.c_str(), size);
                    parseWaitTimes(data, size, parkId);
                    _lastUpdate = last_update;
                    
                    if (_updateCallback) {
                        _updateCallback();
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
    
    // Update timestamp when parks list is refreshed
    _lastParksListUpdate = time(nullptr);
    
    Log.printf("[ThemePark] Loaded %d available parks\n", _availableParks.size());
}

void ThemeParkModule::parseWaitTimes(const char* jsonBuffer, size_t size, const PsramString& parkId) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, jsonBuffer, size);
    if (error) {
        Log.printf("[ThemePark] Failed to parse wait times: %s\n", error.c_str());
        return;
    }
    
    // The API returns an array of attractions, not park metadata
    // We need to get the park name and country from our cache
    PsramString parkName = getParkNameFromCache(parkId);
    if (parkName.empty()) {
        parkName = parkId;  // Fallback to ID if name not found
    }
    
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
    
    // If park is closed and has no opening hours information, remove it from display
    if (allAttractionsClosed && parkData->openingTime.empty() && parkData->closingTime.empty()) {
        Log.printf("[ThemePark] Park %s is closed with no opening hours - skipping\n", parkName.c_str());
        // Remove this park from _parkData
        for (size_t i = 0; i < _parkData.size(); i++) {
            if (_parkData[i].id == parkId) {
                _parkData.erase(_parkData.begin() + i);
                break;
            }
        }
        _totalPages = _parkData.size();
        return;
    }
    
    // Sort attractions by wait time (descending)
    std::sort(parkData->attractions.begin(), parkData->attractions.end(),
              [](const Attraction& a, const Attraction& b) {
                  return a.waitTime > b.waitTime;
              });
    
    _totalPages = _parkData.size();
    
    Log.printf("[ThemePark] Updated park %s with %d attractions (%d open)\n", 
               parkName.c_str(), parkData->attractions.size(), openAttractionsCount);
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
        int numPages = _parkData.size() > 0 ? _parkData.size() : 1;
        duration = (numPages * _pageDisplayDuration) + runtime_safe_buffer;
        xSemaphoreGive(_dataMutex);
    } else {
        duration = _pageDisplayDuration;
    }
    return duration;
}

void ThemeParkModule::resetPaging() {
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        _currentPage = 0;
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

void ThemeParkModule::logicTick() {
    _logicTicksSincePageSwitch++;
    
    uint32_t ticksPerPage = (_pageDisplayDuration / 100);
    
    if (_logicTicksSincePageSwitch >= ticksPerPage) {
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            int numPages = _parkData.size() > 0 ? _parkData.size() : 1;
            _currentPage++;
            
            if (_currentPage >= numPages) {
                _currentPage = 0;
                _isFinished = true;
                Log.printf("[ThemePark] All %d pages shown -> Module finished\n", numPages);
            } else if (_totalPages > 1) {
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
        if (_parkData.empty()) {
            drawNoDataPage();
        } else {
            drawParkPage(_currentPage);
        }
        xSemaphoreGive(_dataMutex);
    } else {
        drawNoDataPage();
    }
}

void ThemeParkModule::drawParkPage(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= (int)_parkData.size()) {
        drawNoDataPage();
        return;
    }
    
    const ThemeParkData& park = _parkData[pageIndex];
    
    // Draw park name with country as headline - format: "Parkname (Land)"
    _u8g2.setFont(u8g2_font_9x15_tf);
    _u8g2.setForegroundColor(0xFFFF);
    
    PsramString displayName = park.name;
    if (!park.country.empty()) {
        displayName = displayName + " (" + park.country + ")";
    }
    displayName = truncateString(displayName, _canvas.width() - 50);
    _u8g2.setCursor(2, 10);
    _u8g2.print(displayName.c_str());
    
    // Draw crowd level as percentage text (no box)
    // Crowd level is 0-10 scale, convert to percentage: level * 10
    // Color: green <= 40%, yellow 41-60%, red > 60%
    int crowdPercent = park.crowdLevel * 10;
    uint16_t crowdColor;
    if (crowdPercent <= 40) {
        crowdColor = 0x07E0;  // Green
    } else if (crowdPercent <= 60) {
        crowdColor = 0xFFE0;  // Yellow
    } else {
        crowdColor = 0xF800;  // Red
    }
    
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setForegroundColor(crowdColor);
    char crowdText[8];
    snprintf(crowdText, sizeof(crowdText), "%d%%", crowdPercent);
    int crowdW = _u8g2.getUTF8Width(crowdText);
    _u8g2.setCursor(_canvas.width() - crowdW - 2, 10);
    _u8g2.print(crowdText);
    
    // Draw opening hours if available
    int yPos = 14;
    _u8g2.setFont(u8g2_font_5x8_tf);
    _u8g2.setForegroundColor(0xFFFF);
    
    if (!park.openingTime.empty() && !park.closingTime.empty()) {
        char hoursText[50];
        if (park.isOpen) {
            // Park is open - show closing time
            snprintf(hoursText, sizeof(hoursText), "Geoeffnet bis %s", park.closingTime.c_str());
        } else {
            // Park is closed - show opening and closing times
            snprintf(hoursText, sizeof(hoursText), "Geoeffnet %s - %s", 
                     park.openingTime.c_str(), park.closingTime.c_str());
        }
        _u8g2.setCursor(2, yPos);
        _u8g2.print(hoursText);
        yPos += 8;
    }
    
    // Draw top wait times
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setForegroundColor(0xFFFF);
    
    yPos += 6;  // Add 4 more pixels gap before wait times (was 2, now 6)
    int lineHeight = 9;
    int maxLines = (_canvas.height() - yPos) / lineHeight;
    int linesToShow = min(maxLines, min(8, (int)park.attractions.size()));
    
    for (int i = 0; i < linesToShow; i++) {
        const Attraction& attr = park.attractions[i];
        
        if (!attr.isOpen) continue;
        
        // Truncate name to fit
        PsramString attrName = truncateString(attr.name, _canvas.width() - 40);
        
        // Draw attraction name
        _u8g2.setCursor(2, yPos);
        _u8g2.print(attrName.c_str());
        
        // Draw wait time
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
        _u8g2.setForegroundColor(0xFFFF);
        
        yPos += lineHeight;
    }
    
    // Draw page indicator
    if (_totalPages > 1) {
        _u8g2.setFont(u8g2_font_5x8_tf);
        char pageStr[16];
        snprintf(pageStr, sizeof(pageStr), "%d/%d", _currentPage + 1, _totalPages);
        int pageW = _u8g2.getUTF8Width(pageStr);
        _u8g2.setCursor(_canvas.width() - pageW - 2, _canvas.height() - 2);
        _u8g2.print(pageStr);
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

uint16_t ThemeParkModule::getCrowdLevelColor(int level) {
    // 0-10 scale, from green (low) to red (high)
    if (level <= 3) return 0x07E0;      // Green
    if (level <= 5) return 0x9FE0;      // Light green
    if (level <= 7) return 0xFFE0;      // Yellow
    if (level <= 8) return 0xFD20;      // Orange
    return 0xF800;                       // Red
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
                const char* country = park["land"] | "";  // API uses "land" not "country"
                
                if (id && name && strlen(id) > 0 && strlen(name) > 0) {
                    _availableParks.push_back(AvailablePark(id, name, country));
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
    PsramString name;
    
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (const auto& park : _availableParks) {
            if (park.id == parkId) {
                name = park.name;
                break;
            }
        }
        xSemaphoreGive(_dataMutex);
    }
    
    return name;
}

PsramString ThemeParkModule::getParkCountryFromCache(const PsramString& parkId) {
    PsramString country;
    
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (const auto& park : _availableParks) {
            if (park.id == parkId) {
                country = park.country;
                break;
            }
        }
        xSemaphoreGive(_dataMutex);
    }
    
    return country;
}

void ThemeParkModule::checkAndUpdateParksList() {
    if (!_webClient) return;
    
    time_t now = time(nullptr);
    
    // Check if we need to update (once per day = 86400 seconds)
    // If _lastParksListUpdate is 0, it means we've never updated automatically
    if (_lastParksListUpdate > 0 && (now - _lastParksListUpdate) < 86400) {
        return;  // Not yet time to update
    }
    
    Log.println("[ThemePark] Performing daily parks list update");
    
    PsramString url = "https://api.wartezeiten.app/v1/parks";
    PsramString headers = "accept: application/json\nlanguage: de";
    
    // Fetch parks list asynchronously
    _webClient->getRequest(url, headers, [this](int httpCode, const char* payload, size_t len) {
        if (httpCode == 200 && payload && len > 0) {
            Log.printf("[ThemePark] Daily update: received parks list (size: %d)\n", len);
            parseAvailableParks(payload, len);
            _lastParksListUpdate = time(nullptr);
        } else {
            Log.printf("[ThemePark] Daily update failed: HTTP %d\n", httpCode);
        }
    });
}
