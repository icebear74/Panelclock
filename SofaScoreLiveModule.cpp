#include "SofaScoreLiveModule.hpp"
#include "WebClientModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <time.h>

// Allocator for PSRAM (for ArduinoJson)
struct SpiRamAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override { return ps_malloc(size); }
    void deallocate(void* pointer) override { free(pointer); }
    void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
};

// --- SofaScoreTournament Implementation ---

SofaScoreTournament::SofaScoreTournament() = default;

SofaScoreTournament::SofaScoreTournament(const SofaScoreTournament& other) {
    id = other.id;
    name = other.name ? psram_strdup(other.name) : nullptr;
    slug = other.slug ? psram_strdup(other.slug) : nullptr;
    isEnabled = other.isEnabled;
}

SofaScoreTournament& SofaScoreTournament::operator=(const SofaScoreTournament& other) {
    if (this != &other) {
        free(name); free(slug);
        id = other.id;
        name = other.name ? psram_strdup(other.name) : nullptr;
        slug = other.slug ? psram_strdup(other.slug) : nullptr;
        isEnabled = other.isEnabled;
    }
    return *this;
}

SofaScoreTournament::SofaScoreTournament(SofaScoreTournament&& other) noexcept {
    id = other.id;
    name = other.name;
    slug = other.slug;
    isEnabled = other.isEnabled;
    other.name = nullptr;
    other.slug = nullptr;
}

SofaScoreTournament& SofaScoreTournament::operator=(SofaScoreTournament&& other) noexcept {
    if (this != &other) {
        free(name); free(slug);
        id = other.id;
        name = other.name;
        slug = other.slug;
        isEnabled = other.isEnabled;
        other.name = nullptr;
        other.slug = nullptr;
    }
    return *this;
}

SofaScoreTournament::~SofaScoreTournament() {
    if (name) free(name);
    if (slug) free(slug);
}

// --- SofaScoreMatch Implementation ---

SofaScoreMatch::SofaScoreMatch() = default;

SofaScoreMatch::SofaScoreMatch(const SofaScoreMatch& other) {
    eventId = other.eventId;
    homePlayerName = other.homePlayerName ? psram_strdup(other.homePlayerName) : nullptr;
    awayPlayerName = other.awayPlayerName ? psram_strdup(other.awayPlayerName) : nullptr;
    homeScore = other.homeScore;
    awayScore = other.awayScore;
    tournamentName = other.tournamentName ? psram_strdup(other.tournamentName) : nullptr;
    status = other.status;
    startTimestamp = other.startTimestamp;
    homeAverage = other.homeAverage;
    awayAverage = other.awayAverage;
    home180s = other.home180s;
    away180s = other.away180s;
    homeCheckoutPercent = other.homeCheckoutPercent;
    awayCheckoutPercent = other.awayCheckoutPercent;
}

SofaScoreMatch& SofaScoreMatch::operator=(const SofaScoreMatch& other) {
    if (this != &other) {
        free(homePlayerName); free(awayPlayerName); free(tournamentName);
        eventId = other.eventId;
        homePlayerName = other.homePlayerName ? psram_strdup(other.homePlayerName) : nullptr;
        awayPlayerName = other.awayPlayerName ? psram_strdup(other.awayPlayerName) : nullptr;
        homeScore = other.homeScore;
        awayScore = other.awayScore;
        tournamentName = other.tournamentName ? psram_strdup(other.tournamentName) : nullptr;
        status = other.status;
        startTimestamp = other.startTimestamp;
        homeAverage = other.homeAverage;
        awayAverage = other.awayAverage;
        home180s = other.home180s;
        away180s = other.away180s;
        homeCheckoutPercent = other.homeCheckoutPercent;
        awayCheckoutPercent = other.awayCheckoutPercent;
    }
    return *this;
}

SofaScoreMatch::SofaScoreMatch(SofaScoreMatch&& other) noexcept {
    eventId = other.eventId;
    homePlayerName = other.homePlayerName;
    awayPlayerName = other.awayPlayerName;
    homeScore = other.homeScore;
    awayScore = other.awayScore;
    tournamentName = other.tournamentName;
    status = other.status;
    startTimestamp = other.startTimestamp;
    homeAverage = other.homeAverage;
    awayAverage = other.awayAverage;
    home180s = other.home180s;
    away180s = other.away180s;
    homeCheckoutPercent = other.homeCheckoutPercent;
    awayCheckoutPercent = other.awayCheckoutPercent;
    other.homePlayerName = nullptr;
    other.awayPlayerName = nullptr;
    other.tournamentName = nullptr;
}

SofaScoreMatch& SofaScoreMatch::operator=(SofaScoreMatch&& other) noexcept {
    if (this != &other) {
        free(homePlayerName); free(awayPlayerName); free(tournamentName);
        eventId = other.eventId;
        homePlayerName = other.homePlayerName;
        awayPlayerName = other.awayPlayerName;
        homeScore = other.homeScore;
        awayScore = other.awayScore;
        tournamentName = other.tournamentName;
        status = other.status;
        startTimestamp = other.startTimestamp;
        homeAverage = other.homeAverage;
        awayAverage = other.awayAverage;
        home180s = other.home180s;
        away180s = other.away180s;
        homeCheckoutPercent = other.homeCheckoutPercent;
        awayCheckoutPercent = other.awayCheckoutPercent;
        other.homePlayerName = nullptr;
        other.awayPlayerName = nullptr;
        other.tournamentName = nullptr;
    }
    return *this;
}

SofaScoreMatch::~SofaScoreMatch() {
    if (homePlayerName) free(homePlayerName);
    if (awayPlayerName) free(awayPlayerName);
    if (tournamentName) free(tournamentName);
}

// --- SofaScoreLiveModule Implementation ---

SofaScoreLiveModule::SofaScoreLiveModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref,
                                         WebClientModule* webClient_ptr, DeviceConfig* config)
    : u8g2(u8g2_ref), canvas(canvas_ref), webClient(webClient_ptr), config(config) {
    dataMutex = xSemaphoreCreateMutex();
    
    // Create pixel scrollers
    _nameScroller = new (ps_malloc(sizeof(PixelScroller))) PixelScroller(u8g2, 50);
    _tournamentScroller = new (ps_malloc(sizeof(PixelScroller))) PixelScroller(u8g2, 50);
    
    // Default scroll configuration
    PixelScrollerConfig scrollConfig;
    scrollConfig.mode = ScrollMode::CONTINUOUS;
    scrollConfig.pauseBetweenCyclesMs = 0;
    scrollConfig.scrollReverse = false;
    scrollConfig.paddingPixels = 20;
    _nameScroller->setConfig(scrollConfig);
    _tournamentScroller->setConfig(scrollConfig);
}

SofaScoreLiveModule::~SofaScoreLiveModule() {
    if (dataMutex) vSemaphoreDelete(dataMutex);
    if (tournaments_pending_buffer) free(tournaments_pending_buffer);
    if (daily_pending_buffer) free(daily_pending_buffer);
    if (live_pending_buffer) free(live_pending_buffer);
    if (_nameScroller) {
        _nameScroller->~PixelScroller();
        free(_nameScroller);
    }
    if (_tournamentScroller) {
        _tournamentScroller->~PixelScroller();
        free(_tournamentScroller);
    }
    clearAllData();
}

void SofaScoreLiveModule::onUpdate(std::function<void()> callback) {
    updateCallback = callback;
}

void SofaScoreLiveModule::setConfig(bool enabled, uint32_t fetchIntervalMinutes, unsigned long displaySec,
                                    const PsramString& enabledTournamentIds) {
    if (!webClient) return;
    
    this->_enabled = enabled;
    this->_displayDuration = displaySec > 0 ? displaySec * 1000UL : 20000;
    _currentTicksPerPage = _displayDuration / 100;
    if (_currentTicksPerPage == 0) _currentTicksPerPage = 1;
    
    // Parse enabled tournament IDs
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        this->enabledTournamentIds.clear();
        
        if (!enabledTournamentIds.empty()) {
            size_t pos = 0;
            while (pos < enabledTournamentIds.length()) {
                size_t commaPos = enabledTournamentIds.find(',', pos);
                if (commaPos == PsramString::npos) commaPos = enabledTournamentIds.length();
                
                PsramString idStr = enabledTournamentIds.substr(pos, commaPos - pos);
                // Trim whitespace
                while (!idStr.empty() && (idStr[0] == ' ' || idStr[0] == '\t')) {
                    idStr = idStr.substr(1);
                }
                while (!idStr.empty() && (idStr[idStr.length()-1] == ' ' || idStr[idStr.length()-1] == '\t')) {
                    idStr = idStr.substr(0, idStr.length()-1);
                }
                
                if (!idStr.empty()) {
                    int tournamentId = atoi(idStr.c_str());
                    if (tournamentId > 0) {
                        this->enabledTournamentIds.push_back(tournamentId);
                    }
                }
                
                pos = commaPos + 1;
            }
        }
        xSemaphoreGive(dataMutex);
    }
    
    if (enabled) {
        // Register resources for fetching
        // 1. Tournament list (fetch once per day)
        webClient->registerResource("https://api.sofascore.com/api/v1/config/unique-tournaments/de/darts", 
                                   1440, nullptr);  // Once per day
        
        // 2. Daily events (fetch every hour or as configured)
        // We'll construct today's date dynamically in queueData()
        
        // 3. Live match statistics will be fetched on-demand in queueData()
    }
    
    // Update scroll configuration
    if (config && _nameScroller && _tournamentScroller) {
        uint32_t scrollSpeed = config->globalScrollSpeedMs;
        _nameScroller->setConfiguredScrollSpeed(scrollSpeed);
        _tournamentScroller->setConfiguredScrollSpeed(scrollSpeed);
        
        PixelScrollerConfig scrollConfig;
        scrollConfig.mode = (config->scrollMode == 1) ? ScrollMode::PINGPONG : ScrollMode::CONTINUOUS;
        scrollConfig.pauseBetweenCyclesMs = (uint32_t)config->scrollPauseSec * 1000;
        scrollConfig.scrollReverse = (config->scrollReverse == 1);
        scrollConfig.paddingPixels = 20;
        
        _nameScroller->setConfig(scrollConfig);
        _tournamentScroller->setConfig(scrollConfig);
    }
}

bool SofaScoreLiveModule::isEnabled() {
    return _enabled;
}

unsigned long SofaScoreLiveModule::getDisplayDuration() {
    // Total duration depends on number of pages
    return _displayDuration * _totalPages;
}

void SofaScoreLiveModule::resetPaging() {
    _currentPage = 0;
    _currentMode = SofaScoreDisplayMode::DAILY_RESULTS;
    _logicTicksSincePageSwitch = 0;
    _logicTicksSinceModeSwitch = 0;
    _isFinished = false;
    if (_nameScroller) _nameScroller->reset();
    if (_tournamentScroller) _tournamentScroller->reset();
}

void SofaScoreLiveModule::tick() {
    bool anyScrolled = false;
    
    if (_nameScroller && _nameScroller->tick()) {
        anyScrolled = true;
    }
    if (_tournamentScroller && _tournamentScroller->tick()) {
        anyScrolled = true;
    }
    
    if (anyScrolled && updateCallback) {
        updateCallback();
    }
}

void SofaScoreLiveModule::logicTick() {
    _logicTicksSincePageSwitch++;
    _logicTicksSinceModeSwitch++;
    
    bool needsRedraw = false;
    
    // Page switching within current mode
    if (_logicTicksSincePageSwitch >= _currentTicksPerPage) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Calculate total pages based on current mode
            int calculatedTotalPages = 1;
            
            switch (_currentMode) {
                case SofaScoreDisplayMode::DAILY_RESULTS:
                    calculatedTotalPages = dailyMatches.size() > 0 ? dailyMatches.size() : 1;
                    break;
                case SofaScoreDisplayMode::LIVE_MATCH:
                    calculatedTotalPages = liveMatches.size() > 0 ? liveMatches.size() : 1;
                    break;
                case SofaScoreDisplayMode::TOURNAMENT_LIST:
                    calculatedTotalPages = 1;  // Tournament list is single page
                    break;
            }
            
            _totalPages = calculatedTotalPages;
            
            _currentPage++;
            if (_currentPage >= _totalPages) {
                _currentPage = 0;
                // After showing all pages in current mode, switch mode
                switchToNextMode();
            }
            
            if (_nameScroller) _nameScroller->reset();
            if (_tournamentScroller) _tournamentScroller->reset();
            needsRedraw = true;
            _logicTicksSincePageSwitch = 0;
            
            xSemaphoreGive(dataMutex);
        }
    }
    
    if (needsRedraw && updateCallback) {
        updateCallback();
    }
}

void SofaScoreLiveModule::switchToNextMode() {
    // Cycle through modes: DAILY_RESULTS -> LIVE_MATCH -> (back to DAILY_RESULTS)
    // Skip tournament list for now (can be enabled later)
    
    if (_currentMode == SofaScoreDisplayMode::DAILY_RESULTS) {
        if (liveMatches.size() > 0) {
            _currentMode = SofaScoreDisplayMode::LIVE_MATCH;
        } else {
            _isFinished = true;  // No live matches, finish cycle
        }
    } else if (_currentMode == SofaScoreDisplayMode::LIVE_MATCH) {
        _isFinished = true;  // Finish after showing live matches
    }
    
    _currentPage = 0;
    _logicTicksSinceModeSwitch = 0;
}

void SofaScoreLiveModule::queueData() {
    if (!webClient) return;
    
    // 1. Fetch tournament list
    webClient->accessResource("https://api.sofascore.com/api/v1/config/unique-tournaments/de/darts",
        [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (buffer && size > 0 && last_update > this->tournaments_last_processed_update) {
                if (tournaments_pending_buffer) free(tournaments_pending_buffer);
                tournaments_pending_buffer = (char*)ps_malloc(size + 1);
                if (tournaments_pending_buffer) {
                    memcpy(tournaments_pending_buffer, buffer, size);
                    tournaments_pending_buffer[size] = '\0';
                    tournaments_buffer_size = size;
                    tournaments_last_processed_update = last_update;
                    tournaments_data_pending = true;
                }
            }
        });
    
    // 2. Fetch today's events
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    char dateStr[16];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeinfo);
    
    PsramString dailyUrl = PsramString("https://api.sofascore.com/api/v1/sport/darts/scheduled-events/") + dateStr;
    
    // Register if not already registered
    webClient->registerResource(dailyUrl.c_str(), 60, nullptr);  // Update every 60 minutes
    
    webClient->accessResource(dailyUrl.c_str(),
        [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (buffer && size > 0 && last_update > this->daily_last_processed_update) {
                if (daily_pending_buffer) free(daily_pending_buffer);
                daily_pending_buffer = (char*)ps_malloc(size + 1);
                if (daily_pending_buffer) {
                    memcpy(daily_pending_buffer, buffer, size);
                    daily_pending_buffer[size] = '\0';
                    daily_buffer_size = size;
                    daily_last_processed_update = last_update;
                    daily_data_pending = true;
                }
            }
        });
    
    // 3. Update live match statistics
    updateLiveMatchStats();
}

void SofaScoreLiveModule::updateLiveMatchStats() {
    // For each live match, fetch updated statistics
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (const auto& match : liveMatches) {
            if (match.status == MatchStatus::LIVE) {
                char statsUrl[128];
                snprintf(statsUrl, sizeof(statsUrl), "https://api.sofascore.com/api/v1/event/%d/statistics", match.eventId);
                
                webClient->registerResource(statsUrl, 2, nullptr);  // Update every 2 minutes
                
                // Note: We'll process this in processData()
            }
        }
        xSemaphoreGive(dataMutex);
    }
}

void SofaScoreLiveModule::processData() {
    // Process pending tournament data
    if (tournaments_data_pending) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            parseTournamentsJson(tournaments_pending_buffer, tournaments_buffer_size);
            free(tournaments_pending_buffer);
            tournaments_pending_buffer = nullptr;
            tournaments_data_pending = false;
            xSemaphoreGive(dataMutex);
            if (updateCallback) updateCallback();
        }
    }
    
    // Process pending daily events data
    if (daily_data_pending) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            parseDailyEventsJson(daily_pending_buffer, daily_buffer_size);
            free(daily_pending_buffer);
            daily_pending_buffer = nullptr;
            daily_data_pending = false;
            xSemaphoreGive(dataMutex);
            if (updateCallback) updateCallback();
        }
    }
}

void SofaScoreLiveModule::parseTournamentsJson(const char* json, size_t len) {
    Log.println("[SofaScore] Parsing tournaments JSON...");
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, json, len);
    if (error) {
        Log.printf("[SofaScore] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    availableTournaments.clear();
    
    JsonArray groups = doc["groups"].as<JsonArray>();
    for (JsonObject group : groups) {
        JsonArray tournaments = group["uniqueTournaments"].as<JsonArray>();
        for (JsonObject tournament : tournaments) {
            SofaScoreTournament t;
            t.id = tournament["id"] | 0;
            const char* name = tournament["name"];
            if (name) t.name = psram_strdup(name);
            const char* slug = tournament["slug"];
            if (slug) t.slug = psram_strdup(slug);
            
            // Check if this tournament is in enabled list
            t.isEnabled = false;
            for (int enabledId : enabledTournamentIds) {
                if (enabledId == t.id) {
                    t.isEnabled = true;
                    break;
                }
            }
            
            availableTournaments.push_back(std::move(t));
        }
    }
    
    Log.printf("[SofaScore] Parsed %d tournaments\n", availableTournaments.size());
}

void SofaScoreLiveModule::parseDailyEventsJson(const char* json, size_t len) {
    Log.println("[SofaScore] Parsing daily events JSON...");
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, json, len);
    if (error) {
        Log.printf("[SofaScore] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    dailyMatches.clear();
    liveMatches.clear();
    
    JsonArray events = doc["events"].as<JsonArray>();
    for (JsonObject event : events) {
        int tournamentId = event["tournament"]["uniqueTournament"]["id"] | 0;
        
        // Check if this tournament is enabled
        bool isEnabled = enabledTournamentIds.empty();  // If no filter, show all
        if (!isEnabled) {
            for (int enabledId : enabledTournamentIds) {
                if (enabledId == tournamentId) {
                    isEnabled = true;
                    break;
                }
            }
        }
        
        if (!isEnabled) continue;  // Skip tournaments not in filter
        
        SofaScoreMatch match;
        match.eventId = event["id"] | 0;
        
        const char* homeName = event["homeTeam"]["name"];
        if (homeName) match.homePlayerName = psram_strdup(homeName);
        
        const char* awayName = event["awayTeam"]["name"];
        if (awayName) match.awayPlayerName = psram_strdup(awayName);
        
        JsonObject homeScore = event["homeScore"];
        if (!homeScore.isNull()) {
            match.homeScore = homeScore["current"] | 0;
        }
        
        JsonObject awayScore = event["awayScore"];
        if (!awayScore.isNull()) {
            match.awayScore = awayScore["current"] | 0;
        }
        
        const char* tournamentName = event["tournament"]["name"];
        if (tournamentName) match.tournamentName = psram_strdup(tournamentName);
        
        const char* statusType = event["status"]["type"];
        if (statusType) {
            if (strcmp(statusType, "inprogress") == 0) {
                match.status = MatchStatus::LIVE;
                liveMatches.push_back(match);  // Also add to live matches
            } else if (strcmp(statusType, "finished") == 0) {
                match.status = MatchStatus::FINISHED;
            } else {
                match.status = MatchStatus::SCHEDULED;
            }
        }
        
        match.startTimestamp = event["startTimestamp"] | 0;
        
        dailyMatches.push_back(std::move(match));
    }
    
    Log.printf("[SofaScore] Parsed %d daily matches (%d live)\n", dailyMatches.size(), liveMatches.size());
}

void SofaScoreLiveModule::parseMatchStatistics(int eventId, const char* json, size_t len) {
    Log.printf("[SofaScore] Parsing statistics for event %d...\n", eventId);
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, json, len);
    if (error) {
        Log.printf("[SofaScore] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    // Find the match in liveMatches
    for (auto& match : liveMatches) {
        if (match.eventId == eventId) {
            // Parse statistics
            JsonArray statistics = doc["statistics"].as<JsonArray>();
            for (JsonObject period : statistics) {
                JsonArray groups = period["groups"].as<JsonArray>();
                for (JsonObject group : groups) {
                    JsonArray items = group["statisticsItems"].as<JsonArray>();
                    for (JsonObject item : items) {
                        const char* name = item["name"];
                        if (!name) continue;
                        
                        if (strcmp(name, "Average") == 0) {
                            const char* homeStr = item["home"];
                            const char* awayStr = item["away"];
                            if (homeStr) match.homeAverage = atof(homeStr);
                            if (awayStr) match.awayAverage = atof(awayStr);
                        } else if (strcmp(name, "180s") == 0) {
                            const char* homeStr = item["home"];
                            const char* awayStr = item["away"];
                            if (homeStr) match.home180s = atoi(homeStr);
                            if (awayStr) match.away180s = atoi(awayStr);
                        } else if (strcmp(name, "Checkout %") == 0 || strcmp(name, "Checkout percentage") == 0) {
                            const char* homeStr = item["home"];
                            const char* awayStr = item["away"];
                            if (homeStr) match.homeCheckoutPercent = atof(homeStr);
                            if (awayStr) match.awayCheckoutPercent = atof(awayStr);
                        }
                    }
                }
            }
            
            Log.printf("[SofaScore] Updated stats: Avg %.1f vs %.1f, 180s %d vs %d\n",
                      match.homeAverage, match.awayAverage, match.home180s, match.away180s);
            break;
        }
    }
}

void SofaScoreLiveModule::draw() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    
    canvas.fillScreen(0);
    u8g2.begin(canvas);
    
    switch (_currentMode) {
        case SofaScoreDisplayMode::TOURNAMENT_LIST:
            drawTournamentList();
            break;
        case SofaScoreDisplayMode::DAILY_RESULTS:
            drawDailyResults();
            break;
        case SofaScoreDisplayMode::LIVE_MATCH:
            drawLiveMatch();
            break;
    }
    
    xSemaphoreGive(dataMutex);
}

void SofaScoreLiveModule::drawTournamentList() {
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setForegroundColor(0xFFFF);
    
    const char* title = "Darts Tournaments";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((canvas.width() - titleWidth) / 2, 10);
    u8g2.print(title);
    
    u8g2.setFont(u8g2_font_5x8_tf);
    int y = 22;
    
    for (const auto& tournament : availableTournaments) {
        if (y > canvas.height() - 8) break;
        
        u8g2.setCursor(2, y);
        if (tournament.isEnabled) {
            u8g2.print(">");
        } else {
            u8g2.print(" ");
        }
        
        u8g2.setCursor(10, y);
        if (tournament.name) {
            u8g2.print(tournament.name);
        }
        
        y += 9;
    }
}

void SofaScoreLiveModule::drawDailyResults() {
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setForegroundColor(0xFFFF);
    
    const char* title = "Today's Darts";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((canvas.width() - titleWidth) / 2, 10);
    u8g2.print(title);
    
    // Page indicator
    char pageInfo[16];
    snprintf(pageInfo, sizeof(pageInfo), "%d/%d", _currentPage + 1, _totalPages);
    u8g2.setFont(u8g2_font_profont10_tf);
    int pageInfoWidth = u8g2.getUTF8Width(pageInfo);
    u8g2.setCursor(canvas.width() - pageInfoWidth - 2, 8);
    u8g2.print(pageInfo);
    
    if (_currentPage < dailyMatches.size()) {
        const SofaScoreMatch& match = dailyMatches[_currentPage];
        
        // Tournament name
        u8g2.setFont(u8g2_font_profont10_tf);
        u8g2.setForegroundColor(0xAAAA);
        if (match.tournamentName) {
            int tournWidth = u8g2.getUTF8Width(match.tournamentName);
            u8g2.setCursor((canvas.width() - tournWidth) / 2, 20);
            u8g2.print(match.tournamentName);
        }
        
        // Status and score
        u8g2.setFont(u8g2_font_profont12_tf);
        int y = 35;
        
        // Status indicator
        if (match.status == MatchStatus::LIVE) {
            u8g2.setForegroundColor(0xF800);  // Red for live
            u8g2.setCursor(2, y);
            u8g2.print("LIVE");
        } else if (match.status == MatchStatus::FINISHED) {
            u8g2.setForegroundColor(0x07E0);  // Green for finished
            u8g2.setCursor(2, y);
            u8g2.print("FIN");
        }
        
        // Score in center
        u8g2.setForegroundColor(0xFFFF);
        char scoreStr[16];
        snprintf(scoreStr, sizeof(scoreStr), "%d : %d", match.homeScore, match.awayScore);
        int scoreWidth = u8g2.getUTF8Width(scoreStr);
        u8g2.setCursor((canvas.width() - scoreWidth) / 2, y + 10);
        u8g2.print(scoreStr);
        
        // Player names (with scrolling if needed)
        u8g2.setFont(u8g2_font_5x8_tf);
        y = 55;
        
        if (match.homePlayerName && _nameScroller) {
            int maxWidth = canvas.width() - 4;
            _nameScroller->drawScrollingText(canvas, match.homePlayerName, 2, y, maxWidth, 0, 0xFFFF);
        }
        
        y += 10;
        if (match.awayPlayerName && _nameScroller) {
            int maxWidth = canvas.width() - 4;
            _nameScroller->drawScrollingText(canvas, match.awayPlayerName, 2, y, maxWidth, 1, 0xFFFF);
        }
        
        // Time (if scheduled)
        if (match.status == MatchStatus::SCHEDULED && match.startTimestamp > 0) {
            time_t timestamp = match.startTimestamp;
            struct tm* timeinfo = localtime(&timestamp);
            char timeStr[16];
            strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);
            
            u8g2.setForegroundColor(0xFFE0);
            u8g2.setCursor(canvas.width() - u8g2.getUTF8Width(timeStr) - 2, canvas.height() - 2);
            u8g2.print(timeStr);
        }
    } else {
        u8g2.setFont(u8g2_font_profont12_tf);
        const char* msg = "No matches today";
        int msgWidth = u8g2.getUTF8Width(msg);
        u8g2.setCursor((canvas.width() - msgWidth) / 2, canvas.height() / 2);
        u8g2.print(msg);
    }
}

void SofaScoreLiveModule::drawLiveMatch() {
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setForegroundColor(0xF800);  // Red
    
    const char* title = "LIVE";
    u8g2.setCursor(2, 10);
    u8g2.print(title);
    
    // Page indicator
    char pageInfo[16];
    snprintf(pageInfo, sizeof(pageInfo), "%d/%d", _currentPage + 1, _totalPages);
    u8g2.setFont(u8g2_font_profont10_tf);
    int pageInfoWidth = u8g2.getUTF8Width(pageInfo);
    u8g2.setCursor(canvas.width() - pageInfoWidth - 2, 8);
    u8g2.print(pageInfo);
    
    if (_currentPage < liveMatches.size()) {
        const SofaScoreMatch& match = liveMatches[_currentPage];
        
        // Tournament name
        u8g2.setForegroundColor(0xAAAA);
        if (match.tournamentName) {
            int tournWidth = u8g2.getUTF8Width(match.tournamentName);
            int maxTournWidth = canvas.width() - 60;
            if (tournWidth > maxTournWidth) {
                u8g2.setCursor(30, 10);
            } else {
                u8g2.setCursor((canvas.width() - tournWidth) / 2, 10);
            }
            u8g2.print(match.tournamentName);
        }
        
        // Score
        u8g2.setFont(u8g2_font_profont12_tf);
        u8g2.setForegroundColor(0xFFFF);
        char scoreStr[16];
        snprintf(scoreStr, sizeof(scoreStr), "%d : %d", match.homeScore, match.awayScore);
        int scoreWidth = u8g2.getUTF8Width(scoreStr);
        u8g2.setCursor((canvas.width() - scoreWidth) / 2, 25);
        u8g2.print(scoreStr);
        
        u8g2.setFont(u8g2_font_5x8_tf);
        int y = 28;
        u8g2.setCursor((canvas.width() - u8g2.getUTF8Width("Sets")) / 2, y);
        u8g2.print("Sets");
        
        // Player names with averages
        y = 40;
        u8g2.setFont(u8g2_font_5x8_tf);
        
        // Home player
        if (match.homePlayerName) {
            char line[64];
            if (match.homeAverage > 0) {
                snprintf(line, sizeof(line), "%s (%.1f)", match.homePlayerName, match.homeAverage);
            } else {
                snprintf(line, sizeof(line), "%s", match.homePlayerName);
            }
            
            int lineWidth = u8g2.getUTF8Width(line);
            if (lineWidth > canvas.width() - 4) {
                // Scroll if too long
                _nameScroller->drawScrollingText(canvas, line, 2, y, canvas.width() - 4, 0, 0xFFFF);
            } else {
                u8g2.setCursor(2, y);
                u8g2.print(line);
            }
        }
        
        y += 10;
        
        // Away player
        if (match.awayPlayerName) {
            char line[64];
            if (match.awayAverage > 0) {
                snprintf(line, sizeof(line), "%s (%.1f)", match.awayPlayerName, match.awayAverage);
            } else {
                snprintf(line, sizeof(line), "%s", match.awayPlayerName);
            }
            
            int lineWidth = u8g2.getUTF8Width(line);
            if (lineWidth > canvas.width() - 4) {
                // Scroll if too long
                _nameScroller->drawScrollingText(canvas, line, 2, y, canvas.width() - 4, 1, 0xFFFF);
            } else {
                u8g2.setCursor(2, y);
                u8g2.print(line);
            }
        }
        
        // Statistics (180s, Checkout %)
        y = 65;
        if (match.home180s > 0 || match.away180s > 0) {
            char stat180s[32];
            snprintf(stat180s, sizeof(stat180s), "180s: %d | %d", match.home180s, match.away180s);
            int statWidth = u8g2.getUTF8Width(stat180s);
            u8g2.setForegroundColor(0xFFE0);  // Yellow
            u8g2.setCursor((canvas.width() - statWidth) / 2, y);
            u8g2.print(stat180s);
            y += 9;
        }
        
        if (match.homeCheckoutPercent > 0 || match.awayCheckoutPercent > 0) {
            char statCO[32];
            snprintf(statCO, sizeof(statCO), "CO%%: %.0f | %.0f", match.homeCheckoutPercent, match.awayCheckoutPercent);
            int statWidth = u8g2.getUTF8Width(statCO);
            u8g2.setForegroundColor(0x07FF);  // Cyan
            u8g2.setCursor((canvas.width() - statWidth) / 2, y);
            u8g2.print(statCO);
        }
    } else {
        u8g2.setFont(u8g2_font_profont12_tf);
        u8g2.setForegroundColor(0xFFFF);
        const char* msg = "No live matches";
        int msgWidth = u8g2.getUTF8Width(msg);
        u8g2.setCursor((canvas.width() - msgWidth) / 2, canvas.height() / 2);
        u8g2.print(msg);
    }
}

void SofaScoreLiveModule::clearAllData() {
    availableTournaments.clear();
    enabledTournamentIds.clear();
    dailyMatches.clear();
    liveMatches.clear();
}

void SofaScoreLiveModule::ensureScrollSlots(size_t requiredSize) {
    if (_nameScroller) {
        _nameScroller->ensureSlots(requiredSize);
    }
}

uint16_t SofaScoreLiveModule::dimColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    r >>= 1; g >>= 1; b >>= 1;
    return (r << 11) | (g << 5) | b;
}
