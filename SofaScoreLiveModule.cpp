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
      homeOver140(0), awayOver140(0), homeOver100(0), awayOver100(0),
      homeCheckoutsOver100(0), awayCheckoutsOver100(0),
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
    homeOver140 = other.homeOver140;
    awayOver140 = other.awayOver140;
    homeOver100 = other.homeOver100;
    awayOver100 = other.awayOver100;
    homeCheckoutsOver100 = other.homeCheckoutsOver100;
    awayCheckoutsOver100 = other.awayCheckoutsOver100;
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
        homeOver140 = other.homeOver140;
        awayOver140 = other.awayOver140;
        homeOver100 = other.homeOver100;
        awayOver100 = other.awayOver100;
        homeCheckoutsOver100 = other.homeCheckoutsOver100;
        awayCheckoutsOver100 = other.awayCheckoutsOver100;
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
    homeOver140 = other.homeOver140;
    awayOver140 = other.awayOver140;
    homeOver100 = other.homeOver100;
    awayOver100 = other.awayOver100;
    homeCheckoutsOver100 = other.homeCheckoutsOver100;
    awayCheckoutsOver100 = other.awayCheckoutsOver100;
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
        homeOver140 = other.homeOver140;
        awayOver140 = other.awayOver140;
        homeOver100 = other.homeOver100;
        awayOver100 = other.awayOver100;
        homeCheckoutsOver100 = other.homeCheckoutsOver100;
        awayCheckoutsOver100 = other.awayCheckoutsOver100;
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
                                    uint32_t playNextMinutes, bool continuousLive) {
    if (!webClient) return;
    
    this->_enabled = enabled;
    this->_wantsFullscreen = fullscreen;
    this->_interruptOnLive = interruptOnLive;
    this->_playNextMinutes = playNextMinutes;
    this->_continuousLiveDisplay = continuousLive;
    this->_displayDuration = displaySec > 0 ? displaySec * 1000UL : 20000;
    _currentTicksPerPage = _displayDuration / 100;
    if (_currentTicksPerPage == 0) _currentTicksPerPage = 1;
    
    Log.printf("[SofaScore] Config updated: enabled=%d, fetch=%d min, display=%d sec, fullscreen=%d, interrupt=%d, playNext=%d min, continuousLive=%d\n",
               enabled, fetchIntervalMinutes, displaySec, fullscreen, interruptOnLive, playNextMinutes, continuousLive);
    
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
    // If continuous live display is enabled and we're in live mode, return 0 (no timeout)
    if (_continuousLiveDisplay && _currentMode == SofaScoreDisplayMode::LIVE_MATCH) {
        return 0;  // Continuous display - module controls its own lifetime
    }
    
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
    _liveEventsRegistered = false;  // Reset registration flag
    if (_nameScroller) _nameScroller->reset();
    if (_tournamentScroller) _tournamentScroller->reset();
    
    // Recalculate pages when resetting
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        groupMatchesByTournament();
        _totalPages = calculateTotalPages();
        xSemaphoreGive(dataMutex);
    }
    
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
                    // NOTE: groupMatchesByTournament() is now called only in processData()
                    // and resetPaging(), not on every page advance for performance
                    
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
            
            // In continuous live display mode, check if all matches are finished
            // This check happens on every page switch, not just at the end of the cycle
            // Performance note: Live match count is typically small (1-3), and page switches
            // occur every ~20 seconds, so the overhead is negligible compared to the benefit
            // of quickly detecting when matches finish
            if (_continuousLiveDisplay && _currentMode == SofaScoreDisplayMode::LIVE_MATCH && 
                !needsModeSwitch && areAllLiveMatchesFinished()) {
                needsModeSwitch = true;  // Force mode switch to exit continuous display
                Log.println("[SofaScore] All live matches finished - exiting continuous display");
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

bool SofaScoreLiveModule::areAllLiveMatchesFinished() const {
    // Check if all live matches have finished status
    // IMPORTANT: Caller must hold dataMutex - this function accesses shared liveMatches vector
    // Currently called from:
    // - logicTick() at line ~451 (mutex held)
    // - switchToNextMode() at line ~510 (called from logicTick, mutex held)
    if (liveMatches.empty()) {
        return true;  // No live matches means nothing to show
    }
    
    for (const auto& match : liveMatches) {
        if (match.status != MatchStatus::FINISHED) {
            return false;  // Found a match that's not finished
        }
    }
    
    return true;  // All matches are finished
}

void SofaScoreLiveModule::switchToNextMode() {
    // Cycle through modes: DAILY_RESULTS -> LIVE_MATCH
    // After completing all modes, set _isFinished = true so PanelManager can handle it
    
    if (_currentMode == SofaScoreDisplayMode::DAILY_RESULTS) {
        if (liveMatches.size() > 0) {
            // Switch to live match mode
            _currentMode = SofaScoreDisplayMode::LIVE_MATCH;
            _currentPage = 0;
            _logicTicksSinceModeSwitch = 0;
            Log.println("[SofaScore] Switched to LIVE_MATCH mode");
        } else {
            // No live matches - cycle is complete
            _isFinished = true;
            Log.println("[SofaScore] Cycle complete (no live matches)");
        }
    } else if (_currentMode == SofaScoreDisplayMode::LIVE_MATCH) {
        // Check if continuous live display is enabled
        if (_continuousLiveDisplay && !liveMatches.empty()) {
            // Check if all live matches are finished
            if (areAllLiveMatchesFinished()) {
                // All live matches are finished - exit continuous mode
                _isFinished = true;
                Log.println("[SofaScore] Continuous live display ended - all matches finished");
                
                // Release interrupt if active
                if (_hasActiveInterrupt && _interruptUID > 0) {
                    releasePriorityEx(_interruptUID);
                    _hasActiveInterrupt = false;
                    Log.println("[SofaScore] Released interrupt after all matches finished");
                }
            } else {
                // Keep showing live matches - loop back to first page
                _currentPage = 0;
                _logicTicksSincePageSwitch = 0;
                Log.println("[SofaScore] Continuous live display - looping live matches");
            }
        } else {
            // After showing live matches, cycle is complete
            _isFinished = true;
            
            // Release interrupt if we were showing live matches via interrupt
            if (_hasActiveInterrupt && _interruptUID > 0) {
                releasePriorityEx(_interruptUID);
                _hasActiveInterrupt = false;
                Log.println("[SofaScore] Released interrupt, cycle complete");
            } else {
                Log.println("[SofaScore] Cycle complete after live matches");
            }
        }
    }
    
    // Release PlayNext when cycle finishes (but not when looping in continuous mode)
    if (_isFinished && _hasActivePlayNext && _playNextUID > 0) {
        releasePriorityEx(_playNextUID);
        _hasActivePlayNext = false;
        Log.println("[SofaScore] Released PlayNext after cycle complete");
    }
}

void SofaScoreLiveModule::queueData() {
    if (!webClient) return;
    
    // Check for live events every minute
    checkAndFetchLiveEvents();
    
    // Only fetch daily schedules if not paused (no live events active)
    if (!_dailySchedulesPaused) {
        // Fetch today's events
        time_t now;
        time(&now);
        struct tm* timeinfo = localtime(&now);
        char dateStr[16];
        strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeinfo);
        
        PsramString dailyUrl = PsramString("https://api.sofascore.com/api/v1/sport/darts/scheduled-events/") + dateStr;
        
        // Register only if URL changed (new day)
        if (dailyUrl != _lastRegisteredDailyUrl) {
            uint32_t fetchInterval = config->dartsSofascoreFetchIntervalMin > 0 ? config->dartsSofascoreFetchIntervalMin : 60;
            // Convert minutes to seconds and use registerResourceSeconds for consistency with live events
            webClient->registerResourceSeconds(dailyUrl.c_str(), fetchInterval * 60, false, false);
            _lastRegisteredDailyUrl = dailyUrl;
            Log.printf("[SofaScore] Registered daily events: interval=%d min (%d sec)\n", fetchInterval, fetchInterval * 60);
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
    }
}

void SofaScoreLiveModule::checkAndFetchLiveEvents() {
    if (!webClient) return;
    
    const char* liveUrl = "https://api.sofascore.com/api/v1/sport/darts/events/live";
    
    // Register live events endpoint for periodic fetching (60s when idle, 30s when active)
    // Only register when unregistered or when state changes to prevent spam
    bool shouldRegister = !_liveEventsRegistered;
    
    if (!_hasLiveEvents) {
        // No live events: check every 60 seconds
        if (shouldRegister) {
            webClient->registerResourceSeconds(liveUrl, 60, false, false);
            _liveEventsRegistered = true;
        }
    } else {
        // Has live events: check every 30 seconds with priority
        if (shouldRegister) {
            webClient->registerResourceSeconds(liveUrl, 30, false, true);
            _liveEventsRegistered = true;
        }
    }
    
    // Access the resource to fetch latest data
    webClient->accessResource(liveUrl,
            [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
                if (buffer && size > 0 && last_update > this->live_last_processed_update) {
                    if (live_pending_buffer) free(live_pending_buffer);
                    live_pending_buffer = (char*)ps_malloc(size + 1);
                    if (live_pending_buffer) {
                        memcpy(live_pending_buffer, buffer, size);
                        live_pending_buffer[size] = '\0';
                        live_buffer_size = size;
                        live_last_processed_update = last_update;
                        live_data_pending = true;
                    }
                }
            });
}

void SofaScoreLiveModule::fetchLiveData() {
    _lastLiveDataFetchTime = millis();
    
    // Fetch live events endpoint
    const char* liveUrl = "https://api.sofascore.com/api/v1/sport/darts/events/live";
    
    webClient->accessResource(liveUrl,
        [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (buffer && size > 0 && last_update > this->live_last_processed_update) {
                if (live_pending_buffer) free(live_pending_buffer);
                live_pending_buffer = (char*)ps_malloc(size + 1);
                if (live_pending_buffer) {
                    memcpy(live_pending_buffer, buffer, size);
                    live_pending_buffer[size] = '\0';
                    live_buffer_size = size;
                    live_last_processed_update = last_update;
                    live_data_pending = true;
                }
            }
        });
}

void SofaScoreLiveModule::updateLiveMatchStats() {
    // Collect live event IDs while holding mutex, then fetch stats without holding mutex
    std::vector<int, PsramAllocator<int>> liveEventIds;
    std::vector<int, PsramAllocator<int>> eventIdsToRegister;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Collect all live event IDs that need statistics updates
        for (const auto& match : liveMatches) {
            if (match.status == MatchStatus::LIVE) {
                liveEventIds.push_back(match.eventId);
                
                // Check if already registered
                bool alreadyRegistered = false;
                for (int registeredId : _registeredEventIds) {
                    if (registeredId == match.eventId) {
                        alreadyRegistered = true;
                        break;
                    }
                }
                
                if (!alreadyRegistered) {
                    eventIdsToRegister.push_back(match.eventId);
                    _registeredEventIds.push_back(match.eventId);
                }
            }
        }
        xSemaphoreGive(dataMutex);
    }
    
    // Register new resources with PRIORITY mode and force_new=true (outside mutex to avoid holding mutex during registration)
    // force_new=true allows multiple statistics resources with same base URL but different event IDs
    for (int eventId : eventIdsToRegister) {
        char statsUrl[128];
        snprintf(statsUrl, sizeof(statsUrl), "https://api.sofascore.com/api/v1/event/%d/statistics", eventId);
        // Use registerResourceSeconds with priority=true and force_new=true for 30-second updates
        webClient->registerResourceSeconds(statsUrl, 30, true, true, nullptr);  // Priority pull, 30 second updates, force new
        Log.printf("[SofaScore] Registered PRIORITY live match statistics: eventId=%d (30s interval)\n", eventId);
    }
    
    // Fetch statistics for all live matches WITHOUT holding dataMutex to avoid nested mutex locks
    for (int eventId : liveEventIds) {
        char statsUrl[128];
        snprintf(statsUrl, sizeof(statsUrl), "https://api.sofascore.com/api/v1/event/%d/statistics", eventId);
        
        // Access the statistics data - callback will acquire dataMutex as needed
        webClient->accessResource(statsUrl,
            [this, eventId](const char* buffer, size_t size, time_t last_update, bool is_stale) {
                if (buffer && size > 0) {
                    // parseMatchStatistics() will acquire dataMutex internally
                    this->parseMatchStatistics(eventId, buffer, size);
                }
            });
    }
}

void SofaScoreLiveModule::processData() {
    // Process pending live events data FIRST (higher priority)
    if (live_data_pending) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            parseLiveEventsJson(live_pending_buffer, live_buffer_size);
            free(live_pending_buffer);
            live_pending_buffer = nullptr;
            live_data_pending = false;
            xSemaphoreGive(dataMutex);
            
            // After parsing live events, update statistics for live matches
            // This is done AFTER parsing so liveMatches is populated
            Log.println("[SofaScore] Live events parsed, fetching statistics...");
            updateLiveMatchStats();
            
            if (updateCallback) updateCallback();
        }
    }
    
    // Process pending daily events data (only if no live events)
    if (daily_data_pending && !_dailySchedulesPaused) {
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
}

void SofaScoreLiveModule::parseDailyEventsJson(const char* json, size_t len) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, json, len);
    if (error) {
        Log.printf("[SofaScore] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    dailyMatches.clear();
    // IMPORTANT: Don't clear liveMatches here if we have active live events from the live endpoint
    // The live endpoint is the source of truth for live match scores/legs when _hasLiveEvents is true
    if (!_hasLiveEvents) {
        liveMatches.clear();
    }
    
    // Get today's time for comparison using GeneralTimeConverter
    time_t now = time(nullptr);
    
    JsonArray events = doc["events"].as<JsonArray>();
    
    int skippedNotToday = 0;
    int skippedTournamentFilter = 0;
    int parsedCount = 0;
    
    Log.printf("[SofaScore] Parsing daily events - Total events in JSON: %d\n", events.size());
    
    for (JsonObject event : events) {
        // Check match timestamp - filter to TODAY only using GeneralTimeConverter
        time_t matchTimestamp = event["startTimestamp"] | 0;
        
        // Use isSameDay to check if match is today (isSameDay handles timezone conversion internally)
        if (!timeConverter.isSameDay(now, matchTimestamp)) {
            skippedNotToday++;
            continue;  // Skip matches not happening today
        }
        
        // Get both tournament IDs for matching
        // tournament.id changes per session/day (e.g., 169922 for tonight's matches, 171078 for afternoon)
        // tournament.uniqueTournament.id stays constant for the entire event (e.g., 616 for PDC World Championship)
        int tournamentId = event["tournament"]["id"] | 0;
        int uniqueTournamentId = event["tournament"]["uniqueTournament"]["id"] | 0;
        
        // Check if this tournament is enabled (check BOTH IDs to catch all sessions of same tournament)
        bool isEnabled = enabledTournamentIds.empty();  // If no filter, show all
        if (!isEnabled) {
            for (int enabledId : enabledTournamentIds) {
                if (enabledId == tournamentId || enabledId == uniqueTournamentId) {
                    isEnabled = true;
                    break;
                }
            }
        }
        
        if (!isEnabled) {
            skippedTournamentFilter++;
            continue;  // Skip tournaments not in filter
        }
        
        // Parse match data
        SofaScoreMatch match;
        match.eventId = event["id"] | 0;
        
        // Get player names (with NULL checks)
        const char* homeName = event["homeTeam"]["shortName"];
        if (!homeName) {
            homeName = event["homeTeam"]["name"];
        } else if (*homeName == '\0') {
            homeName = event["homeTeam"]["name"];
        }
        if (homeName) match.homePlayerName = psram_strdup(homeName);
        
        const char* awayName = event["awayTeam"]["shortName"];
        if (!awayName) {
            awayName = event["awayTeam"]["name"];
        } else if (*awayName == '\0') {
            awayName = event["awayTeam"]["name"];
        }
        if (awayName) match.awayPlayerName = psram_strdup(awayName);
        
        // Check if score objects exist before accessing fields
        JsonObject homeScore = event["homeScore"];
        if (!homeScore.isNull() && homeScore.size() > 0) {
            match.homeScore = homeScore["current"] | 0;
            // Parse legs - get the LAST/HIGHEST period that exists (current leg score)
            for (int p = 7; p >= 1; p--) {
                char periodKey[10];
                snprintf(periodKey, sizeof(periodKey), "period%d", p);
                if (homeScore.containsKey(periodKey)) {
                    match.homeLegs = homeScore[periodKey] | 0;
                    break;
                }
            }
        }
        
        JsonObject awayScore = event["awayScore"];
        if (!awayScore.isNull() && awayScore.size() > 0) {
            match.awayScore = awayScore["current"] | 0;
            // Parse legs - get the LAST/HIGHEST period that exists (current leg score)
            for (int p = 7; p >= 1; p--) {
                char periodKey[10];
                snprintf(periodKey, sizeof(periodKey), "period%d", p);
                if (awayScore.containsKey(periodKey)) {
                    match.awayLegs = awayScore[periodKey] | 0;
                    break;
                }
            }
        }
        
        const char* tournamentName = event["tournament"]["name"];
        if (tournamentName) match.tournamentName = psram_strdup(tournamentName);
        
        const char* statusType = event["status"]["type"];
        if (statusType) {
            if (strcmp(statusType, "inprogress") == 0) {
                match.status = MatchStatus::LIVE;
                // Only add to liveMatches from daily events if we DON'T have active live events
                // When _hasLiveEvents is true, the live endpoint is the source of truth for live matches
                if (!_hasLiveEvents) {
                    liveMatches.push_back(match);  // Add to live matches
                }
            } else if (strcmp(statusType, "finished") == 0) {
                match.status = MatchStatus::FINISHED;
            } else {
                match.status = MatchStatus::SCHEDULED;
            }
        } else {
            match.status = MatchStatus::SCHEDULED;
        }
        
        match.startTimestamp = event["startTimestamp"] | 0;
        
        dailyMatches.push_back(std::move(match));
        parsedCount++;
    }
    
    Log.printf("[SofaScore] Parsed %d matches (%d live, skipped: %d not today, %d wrong tournament)\n", 
               parsedCount, liveMatches.size(), skippedNotToday, skippedTournamentFilter);
    
    // Re-group matches by tournament after parsing
    groupMatchesByTournament();
    _totalPages = calculateTotalPages();
}

void SofaScoreLiveModule::parseLiveEventsJson(const char* json, size_t len) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, json, len);
    if (error) {
        Log.printf("[SofaScore] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    JsonArray events = doc["events"].as<JsonArray>();
    
    // Check if events array is empty - no live matches
    if (events.size() == 0) {
        Log.println("[SofaScore] No live events");
        
        // Clear live matches and update state
        liveMatches.clear();
        
        bool hadLiveEvents = _hasLiveEvents;
        _hasLiveEvents = false;
        
        if (hadLiveEvents) {
            _dailySchedulesPaused = false;
            
            // Switch back to normal polling (60s, non-priority)
            const char* liveUrl = "https://api.sofascore.com/api/v1/sport/darts/events/live";
            webClient->registerResourceSeconds(liveUrl, 60, false, false);
            
            // Clear registered event IDs for statistics
            _registeredEventIds.clear();
            
            // Reset mode to DAILY_RESULTS to avoid showing empty live page
            _currentMode = SofaScoreDisplayMode::DAILY_RESULTS;
            _currentPage = 0;
            _currentTournamentIndex = 0;
            _currentTournamentPage = 0;
            
            Log.println("[SofaScore] Live events ended - Resuming daily schedules, switched to 60s polling, reset to DAILY_RESULTS mode");
            
            // Trigger update to refresh display and remove stale live stats
            if (updateCallback) {
                updateCallback();
            }
        }
        
        return;
    }
    
    // Clear and rebuild liveMatches from live endpoint
    liveMatches.clear();
    
    bool foundConfiguredEvent = false;
    int parsedCount = 0;
    
    for (JsonObject event : events) {
        // Get both tournament IDs for matching (see comment in parseDailyEventsJson)
        int tournamentId = event["tournament"]["id"] | 0;
        int uniqueTournamentId = event["tournament"]["uniqueTournament"]["id"] | 0;
        
        // Check if this tournament is enabled (check BOTH IDs)
        bool isEnabled = enabledTournamentIds.empty();
        if (!isEnabled) {
            for (int enabledId : enabledTournamentIds) {
                if (enabledId == tournamentId || enabledId == uniqueTournamentId) {
                    isEnabled = true;
                    break;
                }
            }
        }
        
        if (!isEnabled) continue;
        
        foundConfiguredEvent = true;
        
        SofaScoreMatch match;
        match.eventId = event["id"] | 0;
        
        // Get player names (with NULL checks)
        const char* homeName = event["homeTeam"]["shortName"];
        if (!homeName || *homeName == '\0') {
            homeName = event["homeTeam"]["name"];
        }
        if (homeName) match.homePlayerName = psram_strdup(homeName);
        
        const char* awayName = event["awayTeam"]["shortName"];
        if (!awayName || *awayName == '\0') {
            awayName = event["awayTeam"]["name"];
        }
        if (awayName) match.awayPlayerName = psram_strdup(awayName);
        
        // Parse scores - sets and legs
        JsonObject homeScore = event["homeScore"];
        if (!homeScore.isNull() && homeScore.size() > 0) {
            match.homeScore = homeScore["current"] | 0;
            
            // Parse legs - get the LAST/HIGHEST period that exists (current leg score)
            for (int p = 7; p >= 1; p--) {
                char periodKey[10];
                snprintf(periodKey, sizeof(periodKey), "period%d", p);
                if (homeScore.containsKey(periodKey)) {
                    match.homeLegs = homeScore[periodKey] | 0;
                    break;
                }
            }
        }
        
        JsonObject awayScore = event["awayScore"];
        if (!awayScore.isNull() && awayScore.size() > 0) {
            match.awayScore = awayScore["current"] | 0;
            
            // Parse legs - get the LAST/HIGHEST period that exists (current leg score)
            for (int p = 7; p >= 1; p--) {
                char periodKey[10];
                snprintf(periodKey, sizeof(periodKey), "period%d", p);
                if (awayScore.containsKey(periodKey)) {
                    match.awayLegs = awayScore[periodKey] | 0;
                    break;
                }
            }
        }
        
        const char* tournamentName = event["tournament"]["name"];
        if (tournamentName) match.tournamentName = psram_strdup(tournamentName);
        
        const char* statusType = event["status"]["type"];
        if (statusType && strcmp(statusType, "inprogress") == 0) {
            match.status = MatchStatus::LIVE;
        } else {
            continue;  // Only add truly live matches
        }
        
        match.startTimestamp = event["startTimestamp"] | 0;
        
        liveMatches.push_back(std::move(match));
        parsedCount++;
    }
    
    // Update live event state
    bool hadLiveEvents = _hasLiveEvents;
    _hasLiveEvents = foundConfiguredEvent && !liveMatches.empty();
    
    if (_hasLiveEvents && !hadLiveEvents) {
        _dailySchedulesPaused = true;
        
        // Switch to priority polling (30s interval)
        const char* liveUrl = "https://api.sofascore.com/api/v1/sport/darts/events/live";
        webClient->registerResourceSeconds(liveUrl, 30, true, false);
        
        Log.println("[SofaScore] Live events detected - Pausing daily schedules, switched to PRIORITY 30s polling");
        
        if (_interruptOnLive && updateCallback) {
            updateCallback();
        }
    } else if (!_hasLiveEvents && hadLiveEvents) {
        _dailySchedulesPaused = false;
        
        // Switch back to normal polling (60s, non-priority)
        const char* liveUrl = "https://api.sofascore.com/api/v1/sport/darts/events/live";
        webClient->registerResourceSeconds(liveUrl, 60, false, false);
        
        // Clear registered event IDs
        _registeredEventIds.clear();
        
        Log.println("[SofaScore] Live events ended - Resuming daily schedules, switched to 60s polling");
    }
    
    if (parsedCount > 0) {
        Log.printf("[SofaScore] Parsed %d live matches\n", parsedCount);
    }
}

void SofaScoreLiveModule::parseMatchStatistics(int eventId, const char* json, size_t len) {
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    
    DeserializationError error = deserializeJson(doc, json, len);
    if (error) {
        Log.printf("[SofaScore] Statistics JSON parse error for eventId=%d: %s\n", eventId, error.c_str());
        return;
    }
    
    // Acquire mutex before accessing liveMatches
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Find the match in liveMatches
        bool matchFound = false;
        for (auto& match : liveMatches) {
            if (match.eventId == eventId) {
                matchFound = true;
                
                Log.printf("[SofaScore] Parsing statistics for eventId=%d\n", eventId);
                
                // Parse statistics
                JsonArray statistics = doc["statistics"].as<JsonArray>();
                
                for (JsonObject period : statistics) {
                    JsonArray groups = period["groups"].as<JsonArray>();
                    for (JsonObject group : groups) {
                        JsonArray items = group["statisticsItems"].as<JsonArray>();
                        for (JsonObject item : items) {
                            const char* name = item["name"];
                            const char* key = item["key"];  // Use key field for better matching
                            if (!key && !name) continue;
                            
                            // Helper to get numeric value from either string or numeric field
                            auto getHomeValue = [&item]() -> float {
                                if (item["homeValue"].is<float>()) return item["homeValue"].as<float>();
                                if (item["homeValue"].is<int>()) return item["homeValue"].as<int>();
                                if (item["home"].is<const char*>()) return atof(item["home"].as<const char*>());
                                return 0.0f;
                            };
                            auto getAwayValue = [&item]() -> float {
                                if (item["awayValue"].is<float>()) return item["awayValue"].as<float>();
                                if (item["awayValue"].is<int>()) return item["awayValue"].as<int>();
                                if (item["away"].is<const char*>()) return atof(item["away"].as<const char*>());
                                return 0.0f;
                            };
                            
                            // Use key field (more reliable) or fallback to name
                            if ((key && strcmp(key, "Average3Darts") == 0) || (name && strcmp(name, "Average 3 darts") == 0)) {
                                match.homeAverage = getHomeValue();
                                match.awayAverage = getAwayValue();
                            } else if ((key && strcmp(key, "Thrown180") == 0) || (name && strcmp(name, "Thrown 180") == 0)) {
                                match.home180s = (int)getHomeValue();
                                match.away180s = (int)getAwayValue();
                            } else if ((key && strcmp(key, "ThrownOver140") == 0) || (name && strcmp(name, "Thrown over 140") == 0)) {
                                match.homeOver140 = (int)getHomeValue();
                                match.awayOver140 = (int)getAwayValue();
                            } else if ((key && strcmp(key, "ThrownOver100") == 0) || (name && strcmp(name, "Thrown over 100") == 0)) {
                                match.homeOver100 = (int)getHomeValue();
                                match.awayOver100 = (int)getAwayValue();
                            } else if ((key && strcmp(key, "CheckoutsOver100") == 0) || (name && strcmp(name, "Checkouts over 100") == 0)) {
                                match.homeCheckoutsOver100 = (int)getHomeValue();
                                match.awayCheckoutsOver100 = (int)getAwayValue();
                            } else if ((key && strcmp(key, "CheckoutsAccuracy") == 0) || (name && strcmp(name, "Checkout %") == 0) || (name && strcmp(name, "Checkouts accuracy") == 0)) {
                                // For checkout accuracy, extract percentage from "2/3 (37%)" format if string
                                if (item["home"].is<const char*>()) {
                                    const char* homeStr = item["home"].as<const char*>();
                                    // Look for percentage in parentheses like "2/3 (37%)"
                                    const char* openParen = strchr(homeStr, '(');
                                    const char* closeParen = openParen ? strchr(openParen, ')') : nullptr;
                                    if (openParen && closeParen && closeParen > openParen + 1) {
                                        // Skip '(' and parse the number
                                        float parsed = atof(openParen + 1);
                                        match.homeCheckoutPercent = parsed > 0.0f ? parsed : getHomeValue();
                                    } else {
                                        match.homeCheckoutPercent = getHomeValue();
                                    }
                                } else {
                                    match.homeCheckoutPercent = getHomeValue();
                                }
                                if (item["away"].is<const char*>()) {
                                    const char* awayStr = item["away"].as<const char*>();
                                    const char* openParen = strchr(awayStr, '(');
                                    const char* closeParen = openParen ? strchr(openParen, ')') : nullptr;
                                    if (openParen && closeParen && closeParen > openParen + 1) {
                                        // Skip '(' and parse the number
                                        float parsed = atof(openParen + 1);
                                        match.awayCheckoutPercent = parsed > 0.0f ? parsed : getAwayValue();
                                    } else {
                                        match.awayCheckoutPercent = getAwayValue();
                                    }
                                } else {
                                    match.awayCheckoutPercent = getAwayValue();
                                }
                            }
                        }
                    }
                }
                
                Log.printf("[SofaScore] Statistics parsed for eventId=%d: Avg=%.1f/%.1f, 180s=%d/%d, CO%%=%.1f/%.1f\n", 
                           eventId, match.homeAverage, match.awayAverage, 
                           match.home180s, match.away180s,
                           match.homeCheckoutPercent, match.awayCheckoutPercent);
                
                break;
            }
        }
        
        if (!matchFound) {
            Log.printf("[SofaScore] WARNING: No match found in liveMatches for eventId=%d\n", eventId);
        }
        
        xSemaphoreGive(dataMutex);
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
    // Y-coordinates are BOTTOM-aligned for u8g2
    // Normal (64px): header ~20px, need to fit 5 matches of 8px each
    // Fullscreen (96px): header ~24px, need to fit 7 matches of 9px each
    int y = wantsFullscreen() ? 33 : 30;  // Start below tournament name (bottom-aligned)
    const int LINE_HEIGHT = wantsFullscreen() ? 9 : 8;
    u8g2.setFont(u8g2_font_5x8_tf);
    
    // Ensure we have enough scrollers (2 per match: home + away)
    size_t requiredScrollers = MATCHES_PER_PAGE * 2;
    while (_matchScrollers.size() < requiredScrollers) {
        void* mem = ps_malloc(sizeof(PixelScroller));
        if (!mem) {
            Log.printf("[SofaScore] PSRAM allocation failed for scroller! size=%d, required=%d\n", 
                       _matchScrollers.size(), requiredScrollers);
            break;  // Stop trying if PSRAM is full
        }
        PixelScroller* scroller = new (mem) PixelScroller(u8g2, 50);
        PixelScrollerConfig scrollConfig;
        scrollConfig.mode = ScrollMode::CONTINUOUS;
        scrollConfig.pauseBetweenCyclesMs = 0;
        scrollConfig.scrollReverse = false;
        scrollConfig.paddingPixels = 10;
        scroller->setConfig(scrollConfig);
        
        size_t oldSize = _matchScrollers.size();
        _matchScrollers.push_back(scroller);
        if (_matchScrollers.size() == oldSize) {
            // push_back failed (vector is full)
            Log.println("[SofaScore] Failed to add scroller to vector!");
            scroller->~PixelScroller();
            free(mem);
            break;
        }
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
        
        // NEW LAYOUT: Names at top (left/right) with score centered BETWEEN them
        u8g2.setFont(u8g2_font_profont10_tf);
        u8g2.setForegroundColor(0xFFFF);
        
        // Home player (left) - Name
        if (match.homePlayerName) {
            u8g2.setCursor(2, y);
            u8g2.print(match.homePlayerName);
        }
        
        // Score centered BETWEEN names - NO LABEL
        u8g2.setFont(u8g2_font_profont12_tf);
        char scoreStr[32];
        if (match.homeLegs > 0 || match.awayLegs > 0) {
            // Show: "3:2 (4:3)" = Sets (Legs)
            snprintf(scoreStr, sizeof(scoreStr), "%d:%d (%d:%d)", 
                     match.homeScore, match.awayScore, match.homeLegs, match.awayLegs);
        } else {
            snprintf(scoreStr, sizeof(scoreStr), "%d:%d", match.homeScore, match.awayScore);
        }
        int scoreWidth = u8g2.getUTF8Width(scoreStr);
        u8g2.setCursor((_currentCanvas->width() - scoreWidth) / 2, y);
        u8g2.print(scoreStr);
        
        // Away player (right) - Name
        u8g2.setFont(u8g2_font_profont10_tf);
        if (match.awayPlayerName) {
            int nameWidth = u8g2.getUTF8Width(match.awayPlayerName);
            u8g2.setCursor(_currentCanvas->width() - nameWidth - 2, y);
            u8g2.print(match.awayPlayerName);
        }
        
        y += 10;
        
        // Averages below names (left and right)
        u8g2.setFont(u8g2_font_5x8_tf);
        if (match.homeAverage > 0.1) {  // Use 0.1 to avoid floating point comparison issues
            char avgStr[16];
            snprintf(avgStr, sizeof(avgStr), "%.1f", match.homeAverage);
            u8g2.setCursor(2, y);
            u8g2.print(avgStr);
        }
        
        if (match.awayAverage > 0.1) {  // Use 0.1 to avoid floating point comparison issues
            char avgStr[16];
            snprintf(avgStr, sizeof(avgStr), "%.1f", match.awayAverage);
            int avgWidth = u8g2.getUTF8Width(avgStr);
            u8g2.setCursor(_currentCanvas->width() - avgWidth - 2, y);
            u8g2.print(avgStr);
        }
        
        y += 10;
        
        // Statistics table - MORE ROWS NOW
        if (wantsFullscreen()) {
            u8g2.setFont(u8g2_font_5x8_tf);
            
            // Left column (home values), Center (labels), Right column (away values)
            // No "Home" and "Away" headers - just the stats
            
            char homeVal[12], awayVal[12];
            
            // Row 1: 180s
            snprintf(homeVal, sizeof(homeVal), "%d", match.home180s);
            snprintf(awayVal, sizeof(awayVal), "%d", match.away180s);
            u8g2.setForegroundColor(0xFFFF);
            u8g2.setCursor(2, y);
            u8g2.print(homeVal);
            u8g2.setForegroundColor(0xFFE0);  // Yellow for label
            u8g2.setCursor(_currentCanvas->width() / 2 - 10, y);
            u8g2.print("180");
            u8g2.setForegroundColor(0xFFFF);
            int awayWidth = u8g2.getUTF8Width(awayVal);
            u8g2.setCursor(_currentCanvas->width() - awayWidth - 2, y);
            u8g2.print(awayVal);
            y += 8;
            
            // Row 2: >140
            snprintf(homeVal, sizeof(homeVal), "%d", match.homeOver140);
            snprintf(awayVal, sizeof(awayVal), "%d", match.awayOver140);
            u8g2.setForegroundColor(0xFFFF);
            u8g2.setCursor(2, y);
            u8g2.print(homeVal);
            u8g2.setForegroundColor(0xFFE0);
            u8g2.setCursor(_currentCanvas->width() / 2 - 12, y);
            u8g2.print(">140");
            u8g2.setForegroundColor(0xFFFF);
            awayWidth = u8g2.getUTF8Width(awayVal);
            u8g2.setCursor(_currentCanvas->width() - awayWidth - 2, y);
            u8g2.print(awayVal);
            y += 8;
            
            // Row 3: >100
            snprintf(homeVal, sizeof(homeVal), "%d", match.homeOver100);
            snprintf(awayVal, sizeof(awayVal), "%d", match.awayOver100);
            u8g2.setForegroundColor(0xFFFF);
            u8g2.setCursor(2, y);
            u8g2.print(homeVal);
            u8g2.setForegroundColor(0xFFE0);
            u8g2.setCursor(_currentCanvas->width() / 2 - 12, y);
            u8g2.print(">100");
            u8g2.setForegroundColor(0xFFFF);
            awayWidth = u8g2.getUTF8Width(awayVal);
            u8g2.setCursor(_currentCanvas->width() - awayWidth - 2, y);
            u8g2.print(awayVal);
            y += 8;
            
            // Row 4: CO>100
            snprintf(homeVal, sizeof(homeVal), "%d", match.homeCheckoutsOver100);
            snprintf(awayVal, sizeof(awayVal), "%d", match.awayCheckoutsOver100);
            u8g2.setForegroundColor(0xFFFF);
            u8g2.setCursor(2, y);
            u8g2.print(homeVal);
            u8g2.setForegroundColor(0xFFE0);
            u8g2.setCursor(_currentCanvas->width() / 2 - 15, y);
            u8g2.print("CO>100");
            u8g2.setForegroundColor(0xFFFF);
            awayWidth = u8g2.getUTF8Width(awayVal);
            u8g2.setCursor(_currentCanvas->width() - awayWidth - 2, y);
            u8g2.print(awayVal);
            y += 8;
            
            // Row 5: CO% (Checkout percentage)
            snprintf(homeVal, sizeof(homeVal), "%.0f%%", match.homeCheckoutPercent);
            snprintf(awayVal, sizeof(awayVal), "%.0f%%", match.awayCheckoutPercent);
            u8g2.setForegroundColor(0xFFFF);
            u8g2.setCursor(2, y);
            u8g2.print(homeVal);
            u8g2.setForegroundColor(0x07FF);  // Cyan for CO%
            u8g2.setCursor(_currentCanvas->width() / 2 - 10, y);
            u8g2.print("CO%");
            u8g2.setForegroundColor(0xFFFF);
            awayWidth = u8g2.getUTF8Width(awayVal);
            u8g2.setCursor(_currentCanvas->width() - awayWidth - 2, y);
            u8g2.print(awayVal);
        }
    }
}

void SofaScoreLiveModule::periodicTick() {
    // Check for live matches periodically (every 60 seconds) to trigger interrupts
    if (!_enabled) return;
    
    unsigned long now = millis();
    if (now - _lastInterruptCheckTime >= 60000) {  // Changed to 60 seconds (1 minute)
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
        
        // Check if there are any live matches
        bool hasLiveMatches = !currentLiveEventIds.empty();
        
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
        
        // Request interrupt if:
        // 1. New live match found, OR
        // 2. Live matches exist and no interrupt is currently active (periodic refresh every minute)
        if (hasLiveMatches && (!_hasActiveInterrupt || hasNewLiveMatch)) {
            Log.println("[SofaScore] Requesting interrupt for live match statistics");
            // Use event ID in UID to make it unique per match
            _interruptUID = SOFASCORE_INTERRUPT_UID_BASE + (currentLiveEventIds.empty() ? 0 : currentLiveEventIds[0] % 1000);
            _hasActiveInterrupt = true;
            
            // Request low priority interrupt (will show next after current module)
            unsigned long totalDuration = _displayDuration * (liveMatches.size() > 0 ? liveMatches.size() : 1);
            if (requestPriorityEx(Priority::Low, _interruptUID, totalDuration)) {
                Log.printf("[SofaScore] Interrupt requested with UID %u for %lu ms (periodic update every minute)\n", _interruptUID, totalDuration);
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
    const int MATCHES_PER_PAGE = wantsFullscreen() ? 7 : 5;
    
    if (dailyMatches.empty()) {
        return;
    }
    
    // Group matches by tournament
    for (size_t i = 0; i < dailyMatches.size(); i++) {
        const auto& match = dailyMatches[i];
        
        // Find or create tournament group
        TournamentGroup* group = nullptr;
        for (auto& tg : _tournamentGroups) {
            // Match by tournament name
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
            it = _tournamentGroups.erase(it);
        } else {
            it->pagesNeeded = (matchCount + MATCHES_PER_PAGE - 1) / MATCHES_PER_PAGE;
            if (it->pagesNeeded < 1) it->pagesNeeded = 1;
            ++it;
        }
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
