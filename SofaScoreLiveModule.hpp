#ifndef SOFASCORE_LIVE_MODULE_HPP
#define SOFASCORE_LIVE_MODULE_HPP

#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include <functional>
#include "PsramUtils.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "DrawableModule.hpp"
#include "PixelScroller.hpp"
#include "GeneralTimeConverter.hpp"

class WebClientModule;
struct DeviceConfig;

// UID für SofaScore Live-Match Interrupts
#define SOFASCORE_INTERRUPT_UID_BASE 5000

// Tournament information
struct SofaScoreTournament {
    int id = 0;
    char* name = nullptr;
    char* slug = nullptr;
    bool isEnabled = false;
    
    SofaScoreTournament();
    SofaScoreTournament(const SofaScoreTournament& other);
    SofaScoreTournament& operator=(const SofaScoreTournament& other);
    SofaScoreTournament(SofaScoreTournament&& other) noexcept;
    SofaScoreTournament& operator=(SofaScoreTournament&& other) noexcept;
    ~SofaScoreTournament();
};

// Match/Event status
enum class MatchStatus {
    SCHEDULED,  // Not started yet
    LIVE,       // Currently playing
    FINISHED    // Completed
};

// Daily match result or live match
struct SofaScoreMatch {
    int eventId = 0;
    char* homePlayerName = nullptr;
    char* awayPlayerName = nullptr;
    int homeScore = 0;  // Sets won
    int awayScore = 0;  // Sets won
    int homeLegs = 0;   // Legs in current set
    int awayLegs = 0;   // Legs in current set
    char* tournamentName = nullptr;
    MatchStatus status = MatchStatus::SCHEDULED;
    long startTimestamp = 0;
    
    // Statistics (for live matches)
    float homeAverage = 0.0f;
    float awayAverage = 0.0f;
    int home180s = 0;
    int away180s = 0;
    int homeOver140 = 0;
    int awayOver140 = 0;
    int homeOver100 = 0;
    int awayOver100 = 0;
    int homeCheckoutsOver100 = 0;
    int awayCheckoutsOver100 = 0;
    float homeCheckoutPercent = 0.0f;
    float awayCheckoutPercent = 0.0f;
    
    SofaScoreMatch();
    SofaScoreMatch(const SofaScoreMatch& other);
    SofaScoreMatch& operator=(const SofaScoreMatch& other);
    SofaScoreMatch(SofaScoreMatch&& other) noexcept;
    SofaScoreMatch& operator=(SofaScoreMatch&& other) noexcept;
    ~SofaScoreMatch();
};

// Display mode for the module
enum class SofaScoreDisplayMode {
    TOURNAMENT_LIST,  // Show available tournaments
    DAILY_RESULTS,    // Show today's results
    LIVE_MATCH        // Show live match with stats
};

class SofaScoreLiveModule : public DrawableModule {
public:
    SofaScoreLiveModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref, 
                        const GeneralTimeConverter& timeConverter_ref,
                        WebClientModule* webClient_ptr, DeviceConfig* config);
    ~SofaScoreLiveModule();

    void onUpdate(std::function<void()> callback);
    void setConfig(bool enabled, uint32_t fetchIntervalMinutes, unsigned long displaySec,
                   const PsramString& enabledTournamentIds, bool fullscreen, bool interruptOnLive,
                   uint32_t playNextMinutes, bool continuousLive);
    void queueData();
    void processData();

    // DrawableModule Interface
    const char* getModuleName() const override { return "SofaScoreLiveModule"; }
    const char* getModuleDisplayName() const override { return "Darts Live (SofaScore)"; }
    void draw() override;
    void tick() override;
    void logicTick() override;
    void periodicTick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;
    int getCurrentPage() const override { return _currentPage; }
    int getTotalPages() const override { return _totalPages; }
    
    // Fullscreen support
    bool supportsFullscreen() const override { return true; }
    bool wantsFullscreen() const override { return _wantsFullscreen && _fullscreenCanvas != nullptr; }

private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    // Note: _fullscreenCanvas is inherited from DrawableModule base class
    GFXcanvas16* _currentCanvas = nullptr;  // Points to either &canvas or _fullscreenCanvas
    const GeneralTimeConverter& timeConverter;
    WebClientModule* webClient;
    DeviceConfig* config;
    std::function<void()> updateCallback;
    SemaphoreHandle_t dataMutex;

    // Configuration
    bool _enabled = false;
    bool _wantsFullscreen = false;
    bool _interruptOnLive = true;
    uint32_t _playNextMinutes = 0;  // 0 = disabled
    unsigned long _displayDuration = 20000;  // 20 seconds per page
    uint32_t _currentTicksPerPage = 200;     // 200 * 100ms = 20 seconds
    
    // Live event monitoring
    bool _hasLiveEvents = false;
    bool _dailySchedulesPaused = false;
    bool _continuousLiveDisplay = false;  // Show live match continuously while active
    unsigned long _lastLiveCheckTime = 0;
    unsigned long _lastLiveDataFetchTime = 0;
    unsigned long _lastLiveDisplayTime = 0;  // For repeating live display every minute
    const unsigned long LIVE_CHECK_INTERVAL_MS = 60000;  // Check for live events every 60 seconds
    const unsigned long LIVE_DATA_FETCH_INTERVAL_MS = 30000;  // Fetch live data every 30 seconds when active
    const unsigned long LIVE_DISPLAY_REPEAT_MS = 60000;  // Repeat live display every 60 seconds
    const unsigned long LIVE_MIN_DISPLAY_MS = 20000;  // Minimum 20 seconds display for live stats
    bool _liveEventsRegistered = false;  // Track if live events endpoint is registered to prevent spam
    
    // Paging
    int _currentPage = 0;
    int _totalPages = 1;
    int _currentTournamentIndex = 0;  // Which tournament we're currently showing
    int _currentTournamentPage = 0;   // Which page within the current tournament
    uint32_t _logicTicksSincePageSwitch = 0;
    uint32_t _logicTicksSinceModeSwitch = 0;
    SofaScoreDisplayMode _currentMode = SofaScoreDisplayMode::DAILY_RESULTS;
    bool _isFinished = false;
    
    // Interrupt management
    bool _hasActiveInterrupt = false;
    uint32_t _interruptUID = 0;
    unsigned long _lastInterruptCheckTime = 0;
    std::vector<int, PsramAllocator<int>> _previousLiveEventIds;  // Track previous live matches
    
    // PlayNext management
    bool _hasActivePlayNext = false;
    uint32_t _playNextUID = 0;
    unsigned long _lastPlayNextTime = 0;
    
    // Scrolling
    PixelScroller* _nameScroller = nullptr;
    std::vector<PixelScroller*, PsramAllocator<PixelScroller*>> _matchScrollers;  // One scroller per match line
    PixelScroller* _tournamentScroller = nullptr;
    
    // Data buffers for async fetching
    char* tournaments_pending_buffer = nullptr;
    size_t tournaments_buffer_size = 0;
    volatile bool tournaments_data_pending = false;
    time_t tournaments_last_processed_update = 0;
    
    char* daily_pending_buffer = nullptr;
    size_t daily_buffer_size = 0;
    volatile bool daily_data_pending = false;
    time_t daily_last_processed_update = 0;
    
    char* live_pending_buffer = nullptr;
    size_t live_buffer_size = 0;
    volatile bool live_data_pending = false;
    time_t live_last_processed_update = 0;
    
    // Parsed data
    std::vector<SofaScoreTournament, PsramAllocator<SofaScoreTournament>> availableTournaments;
    std::vector<int, PsramAllocator<int>> enabledTournamentIds;
    std::vector<SofaScoreMatch, PsramAllocator<SofaScoreMatch>> dailyMatches;
    std::vector<SofaScoreMatch, PsramAllocator<SofaScoreMatch>> liveMatches;
    
    // Tournament grouping for multi-page display
    struct TournamentGroup {
        int tournamentId = 0;
        PsramString tournamentName;
        std::vector<int, PsramAllocator<int>> matchIndices;  // Indices into dailyMatches
        int pagesNeeded = 0;  // Number of pages for this tournament
        
        TournamentGroup() : matchIndices(PsramAllocator<int>()) {}
    };
    std::vector<TournamentGroup, PsramAllocator<TournamentGroup>> _tournamentGroups;
    
    // Track registered resources to avoid duplicate registrations
    PsramString _lastRegisteredDailyUrl;
    std::vector<int, PsramAllocator<int>> _registeredEventIds;
    
    // Helper methods
    void clearAllData();
    void parseTournamentsJson(const char* json, size_t len);
    void parseDailyEventsJson(const char* json, size_t len);
    void parseLiveEventsJson(const char* json, size_t len);  // NEW: Parse live events endpoint
    void parseMatchStatistics(int eventId, const char* json, size_t len);
    void updateLiveMatchStats();
    void checkAndFetchLiveEvents();  // NEW: Check for live events every minute
    void fetchLiveData();  // NEW: Fetch live events + statistics
    void switchToNextMode();
    void checkForLiveMatchInterrupt();
    void checkForPlayNext();
    void groupMatchesByTournament();  // New: group and calculate pages
    int calculateTotalPages();         // New: calculate total pages across all tournaments
    bool areAllLiveMatchesFinished() const;  // NEW: Check if all live matches are finished
    
    // Drawing helpers
    void drawTournamentList();
    void drawDailyResults();
    void drawLiveMatch();
    void ensureScrollSlots(size_t requiredSize);
    uint16_t dimColor(uint16_t color);
};

#endif // SOFASCORE_LIVE_MODULE_HPP
