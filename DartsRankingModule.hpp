#ifndef DARTS_RANKING_MODULE_HPP
#define DARTS_RANKING_MODULE_HPP

#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include <functional>
#include "PsramUtils.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "DrawableModule.hpp"

class WebClientModule;

struct DartsDisplayColors {
    uint16_t rankColor = 0xFFFF;
    uint16_t prizeMoneyColor = 0xFFFF;
    uint16_t movementUpColor = 0x07E0;
    uint16_t movementDownColor = 0xF800;
    uint16_t trackedPlayerColor = 0xFFE0;
    uint16_t participantColor = 0x07FF;
    uint16_t subtitleColor = 0xAAAA;
};

enum class PlayerMovement { UP, DOWN, SAME };

struct DartsPlayer {
    int rank = 0;
    char* name = nullptr;
    PlayerMovement movement = PlayerMovement::SAME;
    int movementValue = 0;
    char* prizeMoney = nullptr;
    char* currentRound = nullptr;
    bool isTrackedPlayer = false;
    bool isActive = false;
    bool didParticipate = false;

    DartsPlayer();
    DartsPlayer(const DartsPlayer& other);
    DartsPlayer& operator=(const DartsPlayer& other);
    DartsPlayer(DartsPlayer&& other) noexcept;
    DartsPlayer& operator=(DartsPlayer&& other) noexcept;
    ~DartsPlayer();
};

enum class DartsRankingType { ORDER_OF_MERIT, PRO_TOUR };

class DartsRankingModule : public DrawableModule {
public:
    DartsRankingModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref, WebClientModule* webClient_ptr);
    ~DartsRankingModule();

    void onUpdate(std::function<void(DartsRankingType)> callback);
    void setConfig(bool oomEnabled, bool proTourEnabled, uint32_t fetchIntervalMinutes, unsigned long displaySec, const PsramString& trackedPlayers);
    void queueData();
    void processData();

    // DrawableModule Interface
    const char* getModuleName() const override { return "DartsRankingModule"; }
    const char* getModuleDisplayName() const override { return "Darts Rankings"; }
    void draw() override;
    void tick() override;
    void logicTick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;
    int getCurrentPage() const override { return currentPage; }
    int getTotalPages() const override { return totalPages; }

private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    WebClientModule* webClient;
    std::function<void(DartsRankingType)> updateCallback;
    SemaphoreHandle_t dataMutex;
    DartsDisplayColors colors;

    char* oom_pending_buffer = nullptr;
    size_t oom_buffer_size = 0;
    volatile bool oom_data_pending = false;
    char* protour_pending_buffer = nullptr;
    size_t protour_buffer_size = 0;
    volatile bool protour_data_pending = false;
    
    static const int PLAYERS_PER_PAGE = 5;
    int currentPage = 0;
    int totalPages = 1;
    unsigned long _pageDisplayDuration = 5000;
    uint32_t _logicTicksSincePageSwitch = 0;
    uint32_t _currentTicksPerPage = 50; // 50 * 100ms = 5 Sekunden
    uint32_t _logicTicksSinceRankingSwitch = 0;

    struct ScrollState { int offset = 0; int maxScroll = 0; };
    std::vector<ScrollState, PsramAllocator<ScrollState>> scrollPos;
    ScrollState subtitleScrollState;
    unsigned long lastScrollStepTime = 0;
    uint32_t scrollStepInterval = 150;

    time_t oom_last_processed_update = 0;
    char* oom_mainTitleText = nullptr;
    char* oom_subTitleText = nullptr;
    std::vector<DartsPlayer, PsramAllocator<DartsPlayer>> oom_players;
    
    time_t protour_last_processed_update = 0;
    char* protour_mainTitleText = nullptr;
    char* protour_subTitleText = nullptr;
    std::vector<DartsPlayer, PsramAllocator<DartsPlayer>> protour_players;
    
    std::vector<char*, PsramAllocator<char*>> trackedPlayerNames;
    
    bool _oomEnabled = false;
    bool _proTourEnabled = false;
    DartsRankingType _currentInternalMode = DartsRankingType::ORDER_OF_MERIT;
    
    // Private helper methods
    unsigned long getInternalDisplayDuration(DartsRankingType type);
    uint32_t getInternalTickDuration(DartsRankingType type);
    void setTrackedPlayers(const PsramString& playerNames);
    uint16_t dimColor(uint16_t color);
    void clearAllData();
    void filterAndSortPlayers(DartsRankingType type);
    String extractText(const char* htmlFragment, size_t maxLen);
    void parsePlayerRow(const char* tr_start, const char* tr_end, const PsramVector<PsramString>& headers, DartsPlayer& player, bool isLiveFormat);
    void parseHtml(const char* html, size_t len, DartsRankingType type);
    bool parseTable(const char* html, std::vector<DartsPlayer, PsramAllocator<DartsPlayer>>& players_ref);
    void tickScroll();
    void drawScrollingText(const char* text, int x, int y, int maxWidth, int scrollIndex);
    PsramString fitTextToPixelWidth(const PsramString& text, int maxPixel);
    void resetScroll();
    void ensureScrollPos(size_t requiredSize);
};
#endif // DARTS_RANKING_MODULE_HPP