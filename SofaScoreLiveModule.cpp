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

SofaScoreTournament::SofaScoreTournament(const SofaScoreTournament& other) 
    : id(0), name(nullptr), slug(nullptr), isEnabled(false) {
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

SofaScoreMatch::SofaScoreMatch(const SofaScoreMatch& other)
    : eventId(0), homePlayerName(nullptr), awayPlayerName(nullptr),
      homeScore(0), awayScore(0), homeLegs(0), awayLegs(0), tournamentName(nullptr),
      status(MatchStatus::SCHEDULED), startTimestamp(0),
      homeAverage(0.0f), awayAverage(0.0f), home180s(0), away180s(0),
      homeCheckoutPercent(0.0f), awayCheckoutPercent(0.0f) {
    eventId = other.eventId;
    homePlayerName = other.homePlayerName ? psram_strdup(other.homePlayerName) : nullptr;
    awayPlayerName = other.awayPlayerName ? psram_strdup(other.awayPlayerName) : nullptr;
    homeScore = other.homeScore;
    awayScore = other.awayScore;
    homeLegs = other.homeLegs;
    awayLegs = other.awayLegs;
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
        homeLegs = other.homeLegs;
        awayLegs = other.awayLegs;
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
    homeLegs = other.homeLegs;
    awayLegs = other.awayLegs;
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
        homeLegs = other.homeLegs;
        awayLegs = other.awayLegs;
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
                                         const GeneralTimeConverter& timeConverter_ref,
                                         WebClientModule* webClient_ptr, DeviceConfig* config)
    : u8g2(u8g2_ref), canvas(canvas_ref), _currentCanvas(&canvas_ref),
      timeConverter(timeConverter_ref), 
      webClient(webClient_ptr), config(config) {
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
                                    const PsramString& enabledTournamentIds, bool fullscreen, bool interruptOnLive,
                                    uint32_t playNextMinutes) {
    if (!webClient) return;
    
    this->_enabled = enabled;
    this->_wantsFullscreen = fullscreen;
    this->_interruptOnLive = interruptOnLive;
    this->_playNextMinutes = playNextMinutes;
    this->_displayDuration = displaySec > 0 ? displaySec * 1000UL : 20000;
    _currentTicksPerPage = _displayDuration / 100;
    if (_currentTicksPerPage == 0) _currentTicksPerPage = 1;
    
    Log.printf("[SofaScore] Config updated: enabled=%d, fetch=%d min, display=%d sec, fullscreen=%d, interrupt=%d, playNext=%d min\n",
               enabled, fetchIntervalMinutes, displaySec, fullscreen, interruptOnLive, playNextMinutes);
    
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
        // No need to pre-register active-tournaments URL
        // It will be fetched on-demand when needed
        
        // Daily events will be fetched dynamically in queueData()
        // Live match statistics will be fetched on-demand in queueData()
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
    _currentTournamentIndex = 0;
    _currentTournamentPage = 0;
    _currentMode = SofaScoreDisplayMode::DAILY_RESULTS;
    _logicTicksSincePageSwitch = 0;
    _logicTicksSinceModeSwitch = 0;
    _isFinished = false;
    if (_nameScroller) _nameScroller->reset();
    if (_tournamentScroller) _tournamentScroller->reset();
    
    // Release interrupt if active
    if (_hasActiveInterrupt && _interruptUID > 0) {
        releasePriorityEx(_interruptUID);
        _hasActiveInterrupt = false;
        Log.println("[SofaScore] Released interrupt on reset");
    }
    
    // Release PlayNext if active
    if (_hasActivePlayNext && _playNextUID > 0) {
        releasePriorityEx(_playNextUID);
        _hasActivePlayNext = false;
        Log.println("[SofaScore] Released PlayNext on reset");
    }
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
            bool needsModeSwitch = false;
            
            switch (_currentMode) {
                case SofaScoreDisplayMode::DAILY_RESULTS:
                    // Group matches by tournament and calculate pages
                    groupMatchesByTournament();
                    _totalPages = calculateTotalPages();
                    
                    if (_tournamentGroups.empty()) {
                        _currentPage = 0;
                        _currentTournamentIndex = 0;
                        _currentTournamentPage = 0;
                        needsModeSwitch = true;
                    } else {
                        // Advance to next page within current tournament
                        _currentTournamentPage++;
                        
                        if (_currentTournamentIndex < _tournamentGroups.size()) {
                            const auto& currentGroup = _tournamentGroups[_currentTournamentIndex];
                            
                            if (_currentTournamentPage >= currentGroup.pagesNeeded) {
                                // Move to next tournament
                                _currentTournamentPage = 0;
                                _currentTournamentIndex++;
                                
                                // If no more tournaments, switch mode
                                if (_currentTournamentIndex >= _tournamentGroups.size()) {
                                    _currentTournamentIndex = 0;
                                    _currentTournamentPage = 0;
                                    needsModeSwitch = true;
                                }
                            }
                        } else {
                            needsModeSwitch = true;
                        }
                        
                        // Calculate absolute current page for display
                        _currentPage = 0;
                        for (int i = 0; i < _currentTournamentIndex && i < _tournamentGroups.size(); i++) {
                            _currentPage += _tournamentGroups[i].pagesNeeded;
                        }
                        _currentPage += _currentTournamentPage;
                    }
                    break;
                    
                case SofaScoreDisplayMode::LIVE_MATCH:
                    _totalPages = liveMatches.size() > 0 ? liveMatches.size() : 1;
                    _currentPage++;
                    if (_currentPage >= _totalPages) {
                        _currentPage = 0;
                        needsModeSwitch = true;
                    }
                    break;
                    
                case SofaScoreDisplayMode::TOURNAMENT_LIST:
                    _totalPages = 1;
                    _currentPage = 0;
                    needsModeSwitch = true;
                    break;
            }
            
            if (needsModeSwitch) {
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
        
        // Release interrupt if we were showing live matches via interrupt
        if (_hasActiveInterrupt && _interruptUID > 0) {
            releasePriorityEx(_interruptUID);
            _hasActiveInterrupt = false;
            Log.println("[SofaScore] Released interrupt after live matches complete");
        }
    }
    
    // Release PlayNext when finishing cycle
    if (_isFinished && _hasActivePlayNext && _playNextUID > 0) {
        releasePriorityEx(_playNextUID);
        _hasActivePlayNext = false;
        Log.println("[SofaScore] Released PlayNext after cycle complete");
    }
    
    _currentPage = 0;
    _logicTicksSinceModeSwitch = 0;
}

void SofaScoreLiveModule::queueData() {
    if (!webClient) return;
    
    // Fetch today's events
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    char dateStr[16];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeinfo);
    
    PsramString dailyUrl = PsramString("https://api.sofascore.com/api/v1/sport/darts/scheduled-events/") + dateStr;
    
    // Register only if URL changed (new day)
    if (dailyUrl != _lastRegisteredDailyUrl) {
        webClient->registerResource(dailyUrl.c_str(), 60, nullptr);  // Update every 60 minutes
        _lastRegisteredDailyUrl = dailyUrl;
    }
    
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
    // For each live match, fetch updated statistics every 30 seconds
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        for (const auto& match : liveMatches) {
            if (match.status == MatchStatus::LIVE) {
                // Check if already registered
                bool alreadyRegistered = false;
                for (int registeredId : _registeredEventIds) {
                    if (registeredId == match.eventId) {
                        alreadyRegistered = true;
                        break;
                    }
                }
                
                if (!alreadyRegistered) {
                    char statsUrl[128];
                    snprintf(statsUrl, sizeof(statsUrl), "https://api.sofascore.com/api/v1/event/%d/statistics", match.eventId);
                    
                    webClient->registerResource(statsUrl, 0.5, nullptr);  // Update every 30 seconds (0.5 minutes)
                    _registeredEventIds.push_back(match.eventId);
                    Log.printf("[SofaScore] Registered live match statistics: eventId=%d (30s interval)\n", match.eventId);
                }
                
                // Note: We'll process this in processData()
            }
        }
        xSemaphoreGive(dataMutex);
    }
}

void SofaScoreLiveModule::processData() {
    // Process pending daily events data
    if (daily_data_pending) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
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
    
    // Get today's time for comparison using GeneralTimeConverter
    time_t now = time(nullptr);
    time_t nowLocal = timeConverter.toLocal(now);
    
    JsonArray events = doc["events"].as<JsonArray>();
    for (JsonObject event : events) {
        // Check match timestamp - filter to TODAY only using GeneralTimeConverter
        time_t matchTimestamp = event["startTimestamp"] | 0;
        time_t matchLocal = timeConverter.toLocal(matchTimestamp);
        
        // Use isSameDay to check if match is today
        if (!timeConverter.isSameDay(nowLocal, matchLocal)) {
            continue;  // Skip matches not happening today
        }
        
        // Use tournament.id (seasonal ID) to match with active-tournaments tournamentId
        int tournamentId = event["tournament"]["id"] | 0;
        
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
        
        // Use shortName if available, otherwise fall back to name
        const char* homeName = event["homeTeam"]["shortName"];
        if (!homeName || strlen(homeName) == 0) {
            homeName = event["homeTeam"]["name"];
        }
        if (homeName) match.homePlayerName = psram_strdup(homeName);
        
        const char* awayName = event["awayTeam"]["shortName"];
        if (!awayName || strlen(awayName) == 0) {
            awayName = event["awayTeam"]["name"];
        }
        if (awayName) match.awayPlayerName = psram_strdup(awayName);
        
        JsonObject homeScore = event["homeScore"];
        if (!homeScore.isNull()) {
            match.homeScore = homeScore["current"] | 0;
            // Parse legs from period scores (period1 = set 1, period2 = set 2, etc.)
            // For live matches, find the current set being played
            if (homeScore["period1"].is<int>()) match.homeLegs = homeScore["period1"] | 0;
            if (homeScore["period2"].is<int>()) match.homeLegs = homeScore["period2"] | 0;
            if (homeScore["period3"].is<int>()) match.homeLegs = homeScore["period3"] | 0;
            if (homeScore["period4"].is<int>()) match.homeLegs = homeScore["period4"] | 0;
            if (homeScore["period5"].is<int>()) match.homeLegs = homeScore["period5"] | 0;
        }
        
        JsonObject awayScore = event["awayScore"];
        if (!awayScore.isNull()) {
            match.awayScore = awayScore["current"] | 0;
            // Parse legs from period scores
            if (awayScore["period1"].is<int>()) match.awayLegs = awayScore["period1"] | 0;
            if (awayScore["period2"].is<int>()) match.awayLegs = awayScore["period2"] | 0;
            if (awayScore["period3"].is<int>()) match.awayLegs = awayScore["period3"] | 0;
            if (awayScore["period4"].is<int>()) match.awayLegs = awayScore["period4"] | 0;
            if (awayScore["period5"].is<int>()) match.awayLegs = awayScore["period5"] | 0;
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
    
    Log.printf("[SofaScore] Parsed %d daily matches (%d live) - filtered to TODAY only\n", dailyMatches.size(), liveMatches.size());
    
    // Re-group matches by tournament after parsing (to update page counts and skip empty tournaments)
    groupMatchesByTournament();
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
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Log.println("[SofaScore] draw() - Could not acquire mutex!");
        return;
    }
    
    // Choose the correct canvas based on fullscreen mode
    _currentCanvas = wantsFullscreen() ? _fullscreenCanvas : &canvas;
    if (!_currentCanvas) {
        Log.println("[SofaScore] draw() - No valid canvas!");
        xSemaphoreGive(dataMutex);
        return;
    }
    
    Log.printf("[SofaScore] draw() - mode=%d, page=%d/%d, canvas=%s\n", 
               (int)_currentMode, _currentPage + 1, _totalPages,
               wantsFullscreen() ? "FULLSCREEN" : "NORMAL");
    
    _currentCanvas->fillScreen(0);
    u8g2.begin(*_currentCanvas);
    
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
    u8g2.setCursor((_currentCanvas->width() - titleWidth) / 2, 10);
    u8g2.print(title);
    
    u8g2.setFont(u8g2_font_5x8_tf);
    int y = 22;
    
    for (const auto& tournament : availableTournaments) {
        if (y > _currentCanvas->height() - 8) break;
        
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
    // Header with title
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setForegroundColor(0xFFFF);
    
    const char* title = "Today's Darts";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((_currentCanvas->width() - titleWidth) / 2, 10);
    u8g2.print(title);
    
    // Page indicator
    char pageInfo[16];
    snprintf(pageInfo, sizeof(pageInfo), "%d/%d", _currentPage + 1, _totalPages);
    u8g2.setFont(u8g2_font_profont10_tf);
    int pageInfoWidth = u8g2.getUTF8Width(pageInfo);
    u8g2.setCursor(_currentCanvas->width() - pageInfoWidth - 2, 8);
    u8g2.print(pageInfo);
    
    if (_tournamentGroups.empty() || _currentTournamentIndex >= _tournamentGroups.size()) {
        u8g2.setFont(u8g2_font_profont12_tf);
        const char* msg = "No matches today";
        int msgWidth = u8g2.getUTF8Width(msg);
        u8g2.setCursor((_currentCanvas->width() - msgWidth) / 2, _currentCanvas->height() / 2);
        u8g2.print(msg);
        return;
    }
    
    const auto& currentGroup = _tournamentGroups[_currentTournamentIndex];
    
    // Tournament name as subtitle (scrolling)
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.setForegroundColor(0xAAAA);
    if (!currentGroup.tournamentName.empty()) {
        int tournWidth = u8g2.getUTF8Width(currentGroup.tournamentName.c_str());
        int tournX = (_currentCanvas->width() - tournWidth) / 2;
        u8g2.setCursor(tournX, 20);
        u8g2.print(currentGroup.tournamentName.c_str());
    }
    
    // Calculate matches for this page
    // Fullscreen: 192x96 allows 7 lines, Normal: 192x64 allows 5 lines
    const int MATCHES_PER_PAGE = wantsFullscreen() ? 7 : 5;
    int startIdx = _currentTournamentPage * MATCHES_PER_PAGE;
    int endIdx = startIdx + MATCHES_PER_PAGE;
    if (endIdx > currentGroup.matchIndices.size()) endIdx = currentGroup.matchIndices.size();
    
    // Layout: Time(5 chars left) | Home Player | Away Player | Score(5 chars right)
    // Font width for u8g2_font_5x8_tf is approximately 5 pixels per char
    const int TIME_WIDTH = 30;    // ~5 chars * 6px = 30px left side
    const int SCORE_WIDTH = 35;   // ~5 chars for score right side
    const int MIDDLE_START = TIME_WIDTH + 2;
    const int MIDDLE_WIDTH = _currentCanvas->width() - TIME_WIDTH - SCORE_WIDTH - 6;
    const int HALF_WIDTH = MIDDLE_WIDTH / 2;
    
    // Adjust line height and starting position based on screen size
    // Normal (64px): header ~18px, need to fit 5 matches of 8px each = 40px + spacing
    // Fullscreen (96px): header ~22px, need to fit 7 matches of 8px each = 56px + spacing
    int y = wantsFullscreen() ? 28 : 22;  // Start below tournament name
    const int LINE_HEIGHT = wantsFullscreen() ? 10 : 8;
    u8g2.setFont(u8g2_font_5x8_tf);
    
    // Ensure we have enough scrollers (2 per match: home + away)
    while (_matchScrollers.size() < MATCHES_PER_PAGE * 2) {
        PixelScroller* scroller = new (ps_malloc(sizeof(PixelScroller))) PixelScroller(u8g2, 50);
        PixelScrollerConfig scrollConfig;
        scrollConfig.mode = ScrollMode::CONTINUOUS;
        scrollConfig.pauseBetweenCyclesMs = 0;
        scrollConfig.scrollReverse = false;
        scrollConfig.paddingPixels = 10;
        scroller->setConfig(scrollConfig);
        _matchScrollers.push_back(scroller);
    }
    
    for (int i = startIdx; i < endIdx; i++) {
        int matchIdx = currentGroup.matchIndices[i];
        if (matchIdx >= dailyMatches.size()) continue;
        
        const SofaScoreMatch& match = dailyMatches[matchIdx];
        int scrollerIdx = (i - startIdx) * 2;  // 2 scrollers per match
        
        // LEFT: Time (5 chars fixed)
        char timeStr[6] = "     ";
        if (match.startTimestamp > 0 && match.status != MatchStatus::FINISHED) {
            time_t timestamp_local = timeConverter.toLocal(match.startTimestamp);
            struct tm* timeinfo = localtime(&timestamp_local);
            strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);
        } else if (match.status == MatchStatus::FINISHED) {
            strcpy(timeStr, " FIN ");
        } else if (match.status == MatchStatus::LIVE) {
            strcpy(timeStr, "LIVE ");
        }
        
        u8g2.setForegroundColor(match.status == MatchStatus::LIVE ? 0xF800 : 0xFFE0);
        u8g2.setCursor(2, y);
        u8g2.print(timeStr);
        
        // MIDDLE: Player names (split in half, scrolling if needed)
        u8g2.setForegroundColor(0xFFFF);
        
        const char* homeName = match.homePlayerName ? match.homePlayerName : "?";
        const char* awayName = match.awayPlayerName ? match.awayPlayerName : "?";
        
        int homeWidth = u8g2.getUTF8Width(homeName);
        int awayWidth = u8g2.getUTF8Width(awayName);
        
        // Home player (left half of middle)
        if (homeWidth > HALF_WIDTH - 2 && scrollerIdx < _matchScrollers.size()) {
            _matchScrollers[scrollerIdx]->drawScrollingText(*_currentCanvas, homeName, MIDDLE_START, y, HALF_WIDTH - 2, scrollerIdx, 0xFFFF);
        } else {
            u8g2.setCursor(MIDDLE_START, y);
            u8g2.print(homeName);
        }
        
        // Away player (right half of middle)
        int awayStart = MIDDLE_START + HALF_WIDTH;
        if (awayWidth > HALF_WIDTH - 2 && scrollerIdx + 1 < _matchScrollers.size()) {
            _matchScrollers[scrollerIdx + 1]->drawScrollingText(*_currentCanvas, awayName, awayStart, y, HALF_WIDTH - 2, scrollerIdx + 1, 0xFFFF);
        } else {
            u8g2.setCursor(awayStart, y);
            u8g2.print(awayName);
        }
        
        // RIGHT: Score (max 5 chars fixed)
        char scoreStr[6] = "     ";
        if (match.status == MatchStatus::LIVE) {
            if (match.homeLegs > 0 || match.awayLegs > 0) {
                // Can't fit legs in 5 chars, show sets only with (L)
                snprintf(scoreStr, sizeof(scoreStr), "%d:%dL", match.homeScore, match.awayScore);
            } else {
                snprintf(scoreStr, sizeof(scoreStr), "%d:%dL", match.homeScore, match.awayScore);
            }
            u8g2.setForegroundColor(0xF800);  // Red
        } else if (match.status == MatchStatus::FINISHED) {
            snprintf(scoreStr, sizeof(scoreStr), "%d:%d ", match.homeScore, match.awayScore);
            u8g2.setForegroundColor(0x07E0);  // Green
        } else {
            strcpy(scoreStr, "     ");
            u8g2.setForegroundColor(0x8410);  // Gray
        }
        
        int scoreWidth = u8g2.getUTF8Width(scoreStr);
        u8g2.setCursor(_currentCanvas->width() - scoreWidth - 2, y);
        u8g2.print(scoreStr);
        
        y += LINE_HEIGHT;
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
    u8g2.setCursor(_currentCanvas->width() - pageInfoWidth - 2, 8);
    u8g2.print(pageInfo);
    
    if (_currentPage < liveMatches.size()) {
        const SofaScoreMatch& match = liveMatches[_currentPage];
        
        // Tournament name
        u8g2.setForegroundColor(0xAAAA);
        if (match.tournamentName) {
            int tournWidth = u8g2.getUTF8Width(match.tournamentName);
            int maxTournWidth = _currentCanvas->width() - 60;
            if (tournWidth > maxTournWidth) {
                u8g2.setCursor(30, 10);
            } else {
                u8g2.setCursor((_currentCanvas->width() - tournWidth) / 2, 10);
            }
            u8g2.print(match.tournamentName);
        }
        
        int y = 24;
        
        // Two-column layout for players
        u8g2.setFont(u8g2_font_profont10_tf);
        u8g2.setForegroundColor(0xFFFF);
        
        // Player 1 (Home) - Left column
        if (match.homePlayerName) {
            const char* name = match.homePlayerName;
            // Shorten names if needed
            char shortName[32];
            const char* space = strchr(name, ' ');
            if (space && (space - name) > 0) {
                snprintf(shortName, sizeof(shortName), "%c. %s", name[0], space + 1);
                name = shortName;
            }
            u8g2.setCursor(2, y);
            u8g2.print(name);
            
            // Average below name
            if (match.homeAverage > 0) {
                u8g2.setFont(u8g2_font_5x8_tf);
                char avgStr[16];
                snprintf(avgStr, sizeof(avgStr), "%.1f", match.homeAverage);
                u8g2.setCursor(2, y + 9);
                u8g2.print(avgStr);
            }
        }
        
        // Player 2 (Away) - Right column
        u8g2.setFont(u8g2_font_profont10_tf);
        if (match.awayPlayerName) {
            const char* name = match.awayPlayerName;
            // Shorten names if needed
            char shortName[32];
            const char* space = strchr(name, ' ');
            if (space && (space - name) > 0) {
                snprintf(shortName, sizeof(shortName), "%c. %s", name[0], space + 1);
                name = shortName;
            }
            int nameWidth = u8g2.getUTF8Width(name);
            u8g2.setCursor(_currentCanvas->width() - nameWidth - 2, y);
            u8g2.print(name);
            
            // Average below name
            if (match.awayAverage > 0) {
                u8g2.setFont(u8g2_font_5x8_tf);
                char avgStr[16];
                snprintf(avgStr, sizeof(avgStr), "%.1f", match.awayAverage);
                int avgWidth = u8g2.getUTF8Width(avgStr);
                u8g2.setCursor(_currentCanvas->width() - avgWidth - 2, y + 9);
                u8g2.print(avgStr);
            }
        }
        
        y += 22;
        
        // Score centered
        u8g2.setFont(u8g2_font_profont12_tf);
        u8g2.setForegroundColor(0xFFFF);
        char scoreStr[32];
        // Show legs in parentheses for live matches
        if (match.status == MatchStatus::LIVE && (match.homeLegs > 0 || match.awayLegs > 0)) {
            snprintf(scoreStr, sizeof(scoreStr), "%d:%d (%d:%d)", 
                     match.homeScore, match.awayScore, match.homeLegs, match.awayLegs);
        } else {
            snprintf(scoreStr, sizeof(scoreStr), "%d : %d", match.homeScore, match.awayScore);
        }
        int scoreWidth = u8g2.getUTF8Width(scoreStr);
        u8g2.setCursor((_currentCanvas->width() - scoreWidth) / 2, y);
        u8g2.print(scoreStr);
        
        u8g2.setFont(u8g2_font_5x8_tf);
        y += 10;
        const char* scoreLabel = (match.status == MatchStatus::LIVE && (match.homeLegs > 0 || match.awayLegs > 0)) ? "Sets (Legs)" : "Sets";
        u8g2.setCursor((_currentCanvas->width() - u8g2.getUTF8Width(scoreLabel)) / 2, y);
        u8g2.print(scoreLabel);
        
        y += 12;
        
        // Statistics in 2-column layout
        u8g2.setFont(u8g2_font_5x8_tf);
        
        // 180s
        if (match.home180s > 0 || match.away180s > 0) {
            char home180[8], away180[8];
            snprintf(home180, sizeof(home180), "%d", match.home180s);
            snprintf(away180, sizeof(away180), "%d", match.away180s);
            
            u8g2.setForegroundColor(0xFFE0);  // Yellow
            u8g2.setCursor(2, y);
            u8g2.print(home180);
            
            const char* label = "180s";
            int labelWidth = u8g2.getUTF8Width(label);
            u8g2.setCursor((_currentCanvas->width() - labelWidth) / 2, y);
            u8g2.print(label);
            
            int away180Width = u8g2.getUTF8Width(away180);
            u8g2.setCursor(_currentCanvas->width() - away180Width - 2, y);
            u8g2.print(away180);
            
            y += 9;
        }
        
        // Checkout %
        if (match.homeCheckoutPercent > 0 || match.awayCheckoutPercent > 0) {
            char homeCO[8], awayCO[8];
            snprintf(homeCO, sizeof(homeCO), "%.0f%%", match.homeCheckoutPercent);
            snprintf(awayCO, sizeof(awayCO), "%.0f%%", match.awayCheckoutPercent);
            
            u8g2.setForegroundColor(0x07FF);  // Cyan
            u8g2.setCursor(2, y);
            u8g2.print(homeCO);
            
            const char* label = "CO%";
            int labelWidth = u8g2.getUTF8Width(label);
            u8g2.setCursor((_currentCanvas->width() - labelWidth) / 2, y);
            u8g2.print(label);
            
            int awayCOWidth = u8g2.getUTF8Width(awayCO);
            u8g2.setCursor(_currentCanvas->width() - awayCOWidth - 2, y);
            u8g2.print(awayCO);
        }
    } else {
        u8g2.setFont(u8g2_font_profont12_tf);
        u8g2.setForegroundColor(0xFFFF);
        const char* msg = "No live matches";
        int msgWidth = u8g2.getUTF8Width(msg);
        u8g2.setCursor((_currentCanvas->width() - msgWidth) / 2, _currentCanvas->height() / 2);
        u8g2.print(msg);
    }
}

void SofaScoreLiveModule::periodicTick() {
    // Check for live matches periodically (every 30 seconds) to trigger interrupts
    if (!_enabled) return;
    
    unsigned long now = millis();
    if (now - _lastInterruptCheckTime >= 30000) {
        _lastInterruptCheckTime = now;
        if (_interruptOnLive) {
            checkForLiveMatchInterrupt();
        }
    }
    
    // Check for PlayNext display
    checkForPlayNext();
}

void SofaScoreLiveModule::checkForLiveMatchInterrupt() {
    if (!_interruptOnLive || liveMatches.empty()) return;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Build current list of live event IDs
        std::vector<int, PsramAllocator<int>> currentLiveEventIds;
        for (const auto& match : liveMatches) {
            if (match.status == MatchStatus::LIVE) {
                currentLiveEventIds.push_back(match.eventId);
            }
        }
        
        // Check if there are new live matches (not in previous list)
        bool hasNewLiveMatch = false;
        for (int currentId : currentLiveEventIds) {
            bool foundInPrevious = false;
            for (int previousId : _previousLiveEventIds) {
                if (currentId == previousId) {
                    foundInPrevious = true;
                    break;
                }
            }
            if (!foundInPrevious) {
                hasNewLiveMatch = true;
                Log.printf("[SofaScore] New live match detected: Event ID %d\n", currentId);
                break;
            }
        }
        
        // Update previous list
        _previousLiveEventIds = currentLiveEventIds;
        
        xSemaphoreGive(dataMutex);
        
        // Request interrupt if new live match found
        if (hasNewLiveMatch && !_hasActiveInterrupt) {
            Log.println("[SofaScore] Requesting interrupt for new live match");
            // Use event ID in UID to make it unique per match
            _interruptUID = SOFASCORE_INTERRUPT_UID_BASE + (currentLiveEventIds.empty() ? 0 : currentLiveEventIds[0] % 1000);
            _hasActiveInterrupt = true;
            
            // Request low priority interrupt (will show next after current module)
            unsigned long totalDuration = _displayDuration * (liveMatches.size() > 0 ? liveMatches.size() : 1);
            if (requestPriorityEx(Priority::Low, _interruptUID, totalDuration)) {
                Log.printf("[SofaScore] Interrupt requested with UID %u for %lu ms\n", _interruptUID, totalDuration);
            } else {
                Log.println("[SofaScore] Interrupt request failed");
                _hasActiveInterrupt = false;
            }
        }
    }
}

void SofaScoreLiveModule::checkForPlayNext() {
    if (!_enabled || _playNextMinutes == 0) return;
    if (_hasActivePlayNext) return;  // Already showing PlayNext
    
    unsigned long now = millis();
    unsigned long intervalMs = (unsigned long)_playNextMinutes * 60000UL;
    
    // Check if enough time has passed since last PlayNext
    if (now - _lastPlayNextTime < intervalMs) return;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Count upcoming matches to show
        int upcomingCount = 0;
        for (const auto& match : dailyMatches) {
            if (match.status == MatchStatus::SCHEDULED) {
                upcomingCount++;
            }
        }
        
        xSemaphoreGive(dataMutex);
        
        if (upcomingCount > 0) {
            _lastPlayNextTime = now;
            
            // Use static UID for PlayNext (not time-based to avoid conflicts)
            _playNextUID = SOFASCORE_INTERRUPT_UID_BASE + 999;
            _hasActivePlayNext = true;
            
            // Calculate duration: display duration per upcoming match (max 5 to avoid too long)
            int matchesToShow = (upcomingCount > 5) ? 5 : upcomingCount;
            unsigned long totalDuration = _displayDuration * matchesToShow;
            
            if (requestPriorityEx(Priority::PlayNext, _playNextUID, totalDuration)) {
                Log.printf("[SofaScore] PlayNext requested: UID=%u, duration=%lu ms (%d matches), interval=%d min\n", 
                           _playNextUID, totalDuration, matchesToShow, _playNextMinutes);
            } else {
                Log.println("[SofaScore] PlayNext request failed");
                _hasActivePlayNext = false;
            }
        }
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

void SofaScoreLiveModule::groupMatchesByTournament() {
    // NOTE: This function expects dataMutex to already be held by the caller
    _tournamentGroups.clear();
    
    // Fullscreen (192x96) vs Normal (192x64) - same width, different height
    // Fullscreen has more vertical space: 96px vs 64px = ~50% more
    // Each match takes ~10-11px height
    // Fullscreen: 7 matches per page (96px / ~13px per match line)
    // Normal: 5 matches per page (64px / ~12px per match line)
    const int MATCHES_PER_PAGE = wantsFullscreen() ? 7 : 5;
    
    // Group matches by tournament
    for (size_t i = 0; i < dailyMatches.size(); i++) {
        const auto& match = dailyMatches[i];
        
        // Find or create tournament group
        TournamentGroup* group = nullptr;
        for (auto& tg : _tournamentGroups) {
            // Match by tournament name (could also use ID if available)
            if (match.tournamentName && tg.tournamentName == match.tournamentName) {
                group = &tg;
                break;
            }
        }
        
        if (!group) {
            // Create new group
            TournamentGroup newGroup;
            if (match.tournamentName) {
                newGroup.tournamentName = match.tournamentName;
            }
            _tournamentGroups.push_back(newGroup);
            group = &_tournamentGroups.back();
        }
        
        // Add match index to group
        group->matchIndices.push_back(i);
    }
    
    // Calculate pages needed for each tournament and remove empty tournaments
    auto it = _tournamentGroups.begin();
    while (it != _tournamentGroups.end()) {
        int matchCount = it->matchIndices.size();
        if (matchCount == 0) {
            // Skip tournaments with no matches today
            it = _tournamentGroups.erase(it);
        } else {
            it->pagesNeeded = (matchCount + MATCHES_PER_PAGE - 1) / MATCHES_PER_PAGE;  // Ceiling division
            if (it->pagesNeeded < 1) it->pagesNeeded = 1;
            ++it;
        }
    }
    
    Log.printf("[SofaScore] Grouped into %d tournaments (skipped empty)\n", _tournamentGroups.size());
    for (const auto& group : _tournamentGroups) {
        Log.printf("  - %s: %d matches, %d pages\n", 
                   group.tournamentName.c_str(), group.matchIndices.size(), group.pagesNeeded);
    }
}

int SofaScoreLiveModule::calculateTotalPages() {
    // NOTE: This function expects dataMutex to already be held by the caller
    int total = 0;
    for (const auto& group : _tournamentGroups) {
        total += group.pagesNeeded;
    }
    return total > 0 ? total : 1;
}
