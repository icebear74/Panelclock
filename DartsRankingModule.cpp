#include "DartsRankingModule.hpp"
#include "WebClientModule.hpp"
#include "webconfig.hpp"
#include <Arduino.h>
#include <algorithm>
#include <esp_heap_caps.h>

// --- DartsPlayer Struct Implementierung ---

DartsPlayer::DartsPlayer() = default;

DartsPlayer::DartsPlayer(const DartsPlayer& other) {
    rank = other.rank;
    name = other.name ? psram_strdup(other.name) : nullptr;
    movement = other.movement;
    movementValue = other.movementValue;
    prizeMoney = other.prizeMoney ? psram_strdup(other.prizeMoney) : nullptr;
    currentRound = other.currentRound ? psram_strdup(other.currentRound) : nullptr;
    isTrackedPlayer = other.isTrackedPlayer;
    isActive = other.isActive;
    didParticipate = other.didParticipate;
}

DartsPlayer& DartsPlayer::operator=(const DartsPlayer& other) {
    if (this != &other) {
        free(name); free(prizeMoney); free(currentRound);
        rank = other.rank;
        name = other.name ? psram_strdup(other.name) : nullptr;
        movement = other.movement;
        movementValue = other.movementValue;
        prizeMoney = other.prizeMoney ? psram_strdup(other.prizeMoney) : nullptr;
        currentRound = other.currentRound ? psram_strdup(other.currentRound) : nullptr;
        isTrackedPlayer = other.isTrackedPlayer;
        isActive = other.isActive;
        didParticipate = other.didParticipate;
    }
    return *this;
}

DartsPlayer::DartsPlayer(DartsPlayer&& other) noexcept {
    rank = other.rank; name = other.name; movement = other.movement; movementValue = other.movementValue; prizeMoney = other.prizeMoney; currentRound = other.currentRound; isTrackedPlayer = other.isTrackedPlayer; isActive = other.isActive; didParticipate = other.didParticipate;
    other.name = nullptr; other.prizeMoney = nullptr; other.currentRound = nullptr;
}

DartsPlayer& DartsPlayer::operator=(DartsPlayer&& other) noexcept {
    if (this != &other) {
        free(name); free(prizeMoney); free(currentRound);
        rank = other.rank; name = other.name; movement = other.movement; movementValue = other.movementValue; prizeMoney = other.prizeMoney; currentRound = other.currentRound; isTrackedPlayer = other.isTrackedPlayer; isActive = other.isActive; didParticipate = other.didParticipate;
        other.name = nullptr; other.prizeMoney = nullptr; other.currentRound = nullptr;
    }
    return *this;
}

DartsPlayer::~DartsPlayer() {
    if (name) free(name);
    if (prizeMoney) free(prizeMoney);
    if (currentRound) free(currentRound);
}

// --- DartsRankingModule Klassenimplementierung ---

DartsRankingModule::DartsRankingModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref, WebClientModule* webClient_ptr, DeviceConfig* config)
    : u8g2(u8g2_ref), canvas(canvas_ref), webClient(webClient_ptr), config(config) {
    dataMutex = xSemaphoreCreateMutex();
    
    // PixelScroller für Spielernamen
    _pixelScroller = new (ps_malloc(sizeof(PixelScroller))) PixelScroller(u8g2, 50);
    
    // Separater PixelScroller für Untertitel
    _subtitleScroller = new (ps_malloc(sizeof(PixelScroller))) PixelScroller(u8g2, 50);
    
    // Default-Konfiguration
    PixelScrollerConfig scrollConfig;
    scrollConfig.mode = ScrollMode::CONTINUOUS;
    scrollConfig.pauseBetweenCyclesMs = 0;
    scrollConfig.scrollReverse = false;
    scrollConfig.paddingPixels = 20;
    _pixelScroller->setConfig(scrollConfig);
    _subtitleScroller->setConfig(scrollConfig);
}

DartsRankingModule::~DartsRankingModule() {
    if (dataMutex) vSemaphoreDelete(dataMutex);
    if (oom_pending_buffer) free(oom_pending_buffer);
    if (protour_pending_buffer) free(protour_pending_buffer);
    if (_pixelScroller) {
        _pixelScroller->~PixelScroller();
        free(_pixelScroller);
    }
    if (_subtitleScroller) {
        _subtitleScroller->~PixelScroller();
        free(_subtitleScroller);
    }
    clearAllData(); 
}

void DartsRankingModule::onUpdate(std::function<void(DartsRankingType)> callback) { 
    updateCallback = callback; 
}

void DartsRankingModule::setConfig(bool oomEnabled, bool proTourEnabled, uint32_t fetchIntervalMinutes, unsigned long displaySec, const PsramString& trackedPlayers) {
    if (!webClient) return;
    this->_oomEnabled = oomEnabled;
    this->_proTourEnabled = proTourEnabled;
    this->_pageDisplayDuration = displaySec > 0 ? displaySec * 1000UL : 5000;
    
    _currentTicksPerPage = _pageDisplayDuration / 100;
    if (_currentTicksPerPage == 0) _currentTicksPerPage = 1;

    if (oomEnabled) webClient->registerResource("https://www.dartsrankings.com/", fetchIntervalMinutes, nullptr);
    if (proTourEnabled) webClient->registerResource("https://www.dartsrankings.com/protour", fetchIntervalMinutes, nullptr);
    
    setTrackedPlayers(trackedPlayers);
    
    // PixelScroller Konfiguration aktualisieren
    if (config && _pixelScroller && _subtitleScroller) {
        scrollStepInterval = config->globalScrollSpeedMs;
        _pixelScroller->setConfiguredScrollSpeed(scrollStepInterval);
        _subtitleScroller->setConfiguredScrollSpeed(scrollStepInterval);
        
        PixelScrollerConfig scrollConfig;
        scrollConfig.mode = (config->scrollMode == 1) ? ScrollMode::PINGPONG : ScrollMode::CONTINUOUS;
        scrollConfig.pauseBetweenCyclesMs = (uint32_t)config->scrollPauseSec * 1000;
        scrollConfig.scrollReverse = (config->scrollReverse == 1);
        scrollConfig.paddingPixels = 20;
        
        _pixelScroller->setConfig(scrollConfig);
        _subtitleScroller->setConfig(scrollConfig);
    }
}

bool DartsRankingModule::isEnabled() {
    return _oomEnabled || _proTourEnabled;
}

unsigned long DartsRankingModule::getDisplayDuration() {
    unsigned long totalDuration = 0;
    if (_oomEnabled) totalDuration += getInternalDisplayDuration(DartsRankingType::ORDER_OF_MERIT);
    if (_proTourEnabled) totalDuration += getInternalDisplayDuration(DartsRankingType::PRO_TOUR);
    return totalDuration;
}

void DartsRankingModule::resetPaging() {
    currentPage = 0;
    _currentInternalMode = DartsRankingType::ORDER_OF_MERIT;
    _logicTicksSincePageSwitch = 0;
    _logicTicksSinceRankingSwitch = 0;
    _isFinished = false;
    _expectedTicksForCurrentMode = getInternalTickDuration(_currentInternalMode);
    resetAllScrollers();  // Bei komplettem Reset beide Scroller zurücksetzen
}

void DartsRankingModule::tick() {
    // PixelScroller tick für pixelweises Scrolling
    bool anyScrolled = false;
    
    if (_pixelScroller) {
        if (_pixelScroller->tick()) {
            anyScrolled = true;
        }
    }
    if (_subtitleScroller) {
        if (_subtitleScroller->tick()) {
            anyScrolled = true;
        }
    }
    
    // Redraw anfordern wenn irgendein Scroller aktiv war
    if (anyScrolled && updateCallback) {
        updateCallback(_currentInternalMode);
    }
}

void DartsRankingModule::logicTick() {
    _logicTicksSincePageSwitch++;
    _logicTicksSinceRankingSwitch++;
    
    bool needsRedraw = false;

    if (_logicTicksSincePageSwitch >= _currentTicksPerPage) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Recalculate totalPages under mutex to avoid race condition with data updates
            auto& players_list = (_currentInternalMode == DartsRankingType::ORDER_OF_MERIT) ? oom_players : protour_players;
            int calculatedTotalPages = (players_list.size() > 0) ? (players_list.size() + PLAYERS_PER_PAGE - 1) / PLAYERS_PER_PAGE : 1;
            
            // Safety check: if data was updated and currentPage is now out of bounds, clamp it
            if (currentPage >= calculatedTotalPages) {
                currentPage = 0;
            }
            
            totalPages = calculatedTotalPages;
            
            if (totalPages > 1) {
                currentPage++;
                if (currentPage >= totalPages) {
                    currentPage = 0;
                } else {
                    resetScroll();
                    needsRedraw = true;
                    _logicTicksSincePageSwitch = 0;
                    xSemaphoreGive(dataMutex);
                    if (needsRedraw && updateCallback) {
                        updateCallback(_currentInternalMode);
                    }
                    return;
                }
            }
            xSemaphoreGive(dataMutex);
        }
        _logicTicksSincePageSwitch = 0;
    }

    uint32_t currentModeTicks = _expectedTicksForCurrentMode;
    if (currentModeTicks > 0 && _logicTicksSinceRankingSwitch >= currentModeTicks) {
        DartsRankingType nextMode = _currentInternalMode;
        if (_currentInternalMode == DartsRankingType::ORDER_OF_MERIT && _proTourEnabled) {
            nextMode = DartsRankingType::PRO_TOUR;
        } else if (_currentInternalMode == DartsRankingType::PRO_TOUR && _oomEnabled) {
            nextMode = DartsRankingType::ORDER_OF_MERIT;
        } else {
            _isFinished = true;
            return;
        }

        if (nextMode != _currentInternalMode) {
            _currentInternalMode = nextMode;
            currentPage = 0;
            _logicTicksSincePageSwitch = 0;
            _logicTicksSinceRankingSwitch = 0;
            _expectedTicksForCurrentMode = getInternalTickDuration(_currentInternalMode);
            resetAllScrollers();  // Bei Ranking-Typ-Wechsel beide Scroller zurücksetzen
            needsRedraw = true;
        } else {
            _isFinished = true;
        }
    }

    if (needsRedraw && updateCallback) {
        updateCallback(_currentInternalMode);
    }
}

void DartsRankingModule::draw() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    auto& players_list = (_currentInternalMode == DartsRankingType::ORDER_OF_MERIT) ? oom_players : protour_players;
    char* mainTitle = (_currentInternalMode == DartsRankingType::ORDER_OF_MERIT) ? oom_mainTitleText : protour_mainTitleText;
    char* subTitle = (_currentInternalMode == DartsRankingType::ORDER_OF_MERIT) ? oom_subTitleText : protour_subTitleText;
    
    totalPages = (players_list.size() > 0) ? (players_list.size() + PLAYERS_PER_PAGE - 1) / PLAYERS_PER_PAGE : 1;
    if (currentPage >= totalPages) currentPage = 0;

    canvas.fillScreen(0);
    u8g2.begin(canvas);

    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setForegroundColor(0xFFFF);
    
    const char* titleToDraw = mainTitle ? mainTitle : ((_currentInternalMode == DartsRankingType::ORDER_OF_MERIT) ? "Order of Merit" : "Pro Tour");
    int titleWidth = u8g2.getUTF8Width(titleToDraw);
    u8g2.setCursor((canvas.width() - titleWidth) / 2, 10);
    u8g2.print(titleToDraw);

    if (subTitle && _subtitleScroller) {
        u8g2.setFont(u8g2_font_profont10_tf); 
        int subtitleWidth = u8g2.getUTF8Width(" ") * 40;
        int subtitleX = (canvas.width() - subtitleWidth) / 2;
        _subtitleScroller->drawScrollingText(canvas, subTitle, subtitleX, 18, subtitleWidth, 0, colors.subtitleColor);
    }
    
    // Page info using stack buffer instead of String
    char page_info[32];
    snprintf(page_info, sizeof(page_info), "%d/%d", currentPage + 1, totalPages);
    u8g2.setFont(u8g2_font_profont10_tf);
    int page_info_width = u8g2.getUTF8Width(page_info);
    u8g2.setCursor(canvas.width() - page_info_width - 2, 8);
    u8g2.print(page_info);

    int y = 26; 
    u8g2.setFont(u8g2_font_5x8_tf);
    int row_height = 9;
    
    int start_index = currentPage * PLAYERS_PER_PAGE;
    ensureScrollPos(PLAYERS_PER_PAGE);

    // ** KORREKTE LOGIK: Maximale Rundenlänge über ALLE Spieler ermitteln **
    int max_round_len = 2; // Default auf 2 setzen
    for (const auto& player : players_list) { // Iteriere über die gesamte Liste
        if (player.currentRound && strcmp(player.currentRound, "--") != 0) {
            int len = strlen(player.currentRound);
            if (len > max_round_len) {
                max_round_len = len;
            }
        }
    }

    for (int i = 0; i < PLAYERS_PER_PAGE; ++i) {
        int player_index = start_index + i;
        if (player_index >= (int)players_list.size()) break;

        const DartsPlayer& player = players_list[player_index];

        uint16_t rank_color = player.isActive ? colors.rankColor : dimColor(colors.rankColor);
        u8g2.setForegroundColor(rank_color); 
        u8g2.setCursor(2, y); 
        u8g2.printf("%d.", player.rank);
        
        int x_mov = 25;
        if (player.movementValue > 0) {
            uint16_t move_color = player.isActive ? colors.movementUpColor : dimColor(colors.movementUpColor);
            u8g2.setForegroundColor(move_color);
            u8g2.setCursor(x_mov, y);
            u8g2.printf("+%d", player.movementValue);
        } else if (player.movementValue < 0) {
            uint16_t move_color = player.isActive ? colors.movementDownColor : dimColor(colors.movementDownColor);
            u8g2.setForegroundColor(move_color);
            u8g2.setCursor(x_mov, y);
            u8g2.printf("%d", player.movementValue);
        }
        
        uint16_t name_color;
        if (player.isTrackedPlayer) {
            name_color = player.isActive ? colors.trackedPlayerColor : dimColor(colors.trackedPlayerColor);
        } else if (player.didParticipate && player.rank > 40) {
            name_color = player.isActive ? colors.participantColor : dimColor(colors.participantColor);
        } else {
            name_color = rank_color;
        }
        
        // Build round and right text using stack buffers to avoid heap allocations
        char roundPart[64] = "";
        if (player.currentRound) {
            if (strcmp(player.currentRound, "--") == 0) {
                char dashes[32] = "";
                for(int j=0; j<max_round_len && j<30; ++j) dashes[j] = '-';
                dashes[max_round_len < 30 ? max_round_len : 30] = '\0';
                snprintf(roundPart, sizeof(roundPart), "(%s)", dashes);
            } else {
                char roundStr[32];
                strncpy(roundStr, player.currentRound, sizeof(roundStr)-1);
                roundStr[sizeof(roundStr)-1] = '\0';
                int len = strlen(roundStr);
                while(len < max_round_len && len < 30) {
                    roundStr[len++] = ' ';
                }
                roundStr[len] = '\0';
                snprintf(roundPart, sizeof(roundPart), "(%s)", roundStr);
            }
        }
        
        char rightText[128];
        if (player.prizeMoney) {
            snprintf(rightText, sizeof(rightText), "£%s %s", player.prizeMoney, roundPart);
        } else {
            snprintf(rightText, sizeof(rightText), " %s", roundPart);
        }
        
        int x_right = canvas.width() - u8g2.getUTF8Width(rightText) - 2;
        int nameX = 45;
        int maxNameWidth = x_right - nameX - 4;
        
        // Pixelweises Scrolling für Spielernamen
        if (_pixelScroller && player.name) {
            _pixelScroller->drawScrollingText(canvas, player.name, nameX, y, maxNameWidth, i, name_color);
        } else if (player.name) {
            u8g2.setForegroundColor(name_color);
            u8g2.setCursor(nameX, y);
            u8g2.print(player.name);
        }
        
        uint16_t prize_money_color = player.isActive ? colors.prizeMoneyColor : dimColor(colors.prizeMoneyColor);
        u8g2.setForegroundColor(prize_money_color);
        u8g2.setCursor(x_right, y); u8g2.print(rightText);
        y += row_height;
    }
    xSemaphoreGive(dataMutex);
}

// --- Private Methoden ---

unsigned long DartsRankingModule::getInternalDisplayDuration(DartsRankingType type) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return _pageDisplayDuration;
    
    auto& players_list = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_players : protour_players;
    int num_pages = 1;
    if (players_list.size() > 0) {
        num_pages = (players_list.size() + PLAYERS_PER_PAGE - 1) / PLAYERS_PER_PAGE;
    }
    
    xSemaphoreGive(dataMutex);
    return _pageDisplayDuration * num_pages;
}

uint32_t DartsRankingModule::getInternalTickDuration(DartsRankingType type) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return _currentTicksPerPage;
    
    auto& players_list = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_players : protour_players;
    int num_pages = 1;
    if (players_list.size() > 0) {
        num_pages = (players_list.size() + PLAYERS_PER_PAGE - 1) / PLAYERS_PER_PAGE;
    }
    
    xSemaphoreGive(dataMutex);
    return _currentTicksPerPage * num_pages;
}

void DartsRankingModule::queueData() {
    if (!webClient) return;
    
    if (_oomEnabled) {
        webClient->accessResource("https://www.dartsrankings.com/", [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (buffer && size > 0 && last_update > this->oom_last_processed_update) {
                if (oom_pending_buffer) free(oom_pending_buffer);
                oom_pending_buffer = (char*)ps_malloc(size + 1);
                if (oom_pending_buffer) {
                    memcpy(oom_pending_buffer, buffer, size);
                    oom_pending_buffer[size] = '\0';
                    oom_buffer_size = size;
                    oom_last_processed_update = last_update;
                    oom_data_pending = true;
                }
            }
        });
    }
    
    if (_proTourEnabled) {
        webClient->accessResource("https://www.dartsrankings.com/protour", [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (buffer && size > 0 && last_update > this->protour_last_processed_update) {
                if (protour_pending_buffer) free(protour_pending_buffer);
                protour_pending_buffer = (char*)ps_malloc(size + 1);
                if (protour_pending_buffer) {
                    memcpy(protour_pending_buffer, buffer, size);
                    protour_pending_buffer[size] = '\0';
                    protour_buffer_size = size;
                    protour_last_processed_update = last_update;
                    protour_data_pending = true;
                }
            }
        });
    }
}

void DartsRankingModule::processData() {
    if (oom_data_pending) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            parseHtml(oom_pending_buffer, oom_buffer_size, DartsRankingType::ORDER_OF_MERIT);
            free(oom_pending_buffer);
            oom_pending_buffer = nullptr;
            oom_data_pending = false;
            xSemaphoreGive(dataMutex);
            if (updateCallback) updateCallback(DartsRankingType::ORDER_OF_MERIT);
        }
    }
    if (protour_data_pending) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            parseHtml(protour_pending_buffer, protour_buffer_size, DartsRankingType::PRO_TOUR);
            free(protour_pending_buffer);
            protour_pending_buffer = nullptr;
            protour_data_pending = false;
            xSemaphoreGive(dataMutex);
            if (updateCallback) updateCallback(DartsRankingType::PRO_TOUR);
        }
    }
}

void DartsRankingModule::setTrackedPlayers(const PsramString& playerNames) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    
    for (char* name : trackedPlayerNames) { if (name) free(name); }
    trackedPlayerNames.clear();
    
    size_t lastPos = 0;
    size_t currentPos = 0;
    while((currentPos = playerNames.find(',', lastPos)) != PsramString::npos) {
        PsramString name = playerNames.substr(lastPos, currentPos - lastPos);
        size_t first = name.find_first_not_of(" \t\n\r");
        if (PsramString::npos != first) {
            size_t last = name.find_last_not_of(" \t\n\r");
            name = name.substr(first, (last - first + 1));
        } else {
            name.clear();
        }
        if (!name.empty()) trackedPlayerNames.push_back(psram_strdup(name.c_str()));
        lastPos = currentPos + 1;
    }
    PsramString name = playerNames.substr(lastPos);
    size_t first = name.find_first_not_of(" \t\n\r");
    if (PsramString::npos != first) {
        size_t last = name.find_last_not_of(" \t\n\r");
        name = name.substr(first, (last - first + 1));
    } else {
        name.clear();
    }
    if (!name.empty()) trackedPlayerNames.push_back(psram_strdup(name.c_str()));

    filterAndSortPlayers(DartsRankingType::ORDER_OF_MERIT);
    filterAndSortPlayers(DartsRankingType::PRO_TOUR);
    
    xSemaphoreGive(dataMutex);
}

uint16_t DartsRankingModule::dimColor(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    r >>= 1; g >>= 1; b >>= 1;
    return (r << 11) | (g << 5) | b;
}

void DartsRankingModule::clearAllData() {
    oom_players.clear();
    protour_players.clear();
    if (oom_mainTitleText) { free(oom_mainTitleText); oom_mainTitleText = nullptr; }
    if (oom_subTitleText) { free(oom_subTitleText); oom_subTitleText = nullptr; }
    if (protour_mainTitleText) { free(protour_mainTitleText); protour_mainTitleText = nullptr; }
    if (protour_subTitleText) { free(protour_subTitleText); protour_subTitleText = nullptr; }
}

// Helper function to get sorting priority for a round
// Higher values mean better/further progression
// F=1000, HF=900, QF=800, R9=109, R8=108, ..., R1=101, "--"=0
int DartsRankingModule::getRoundSortValue(const char* round) {
    if (!round || round[0] == '\0' || strcmp(round, "--") == 0) {
        return 0;  // Did not play or empty - lowest priority
    }
    
    // Check for special rounds (finals)
    if (strcmp(round, "F") == 0) return 1000;
    if (strcmp(round, "HF") == 0) return 900;
    if (strcmp(round, "QF") == 0) return 800;
    
    // Check for round numbers (R1, R2, R3, etc.)
    if (round[0] == 'R' && strlen(round) > 1) {
        // Extract the number after 'R' - use strtol for better error handling
        char* endptr;
        long roundNum = strtol(round + 1, &endptr, 10);
        // Check if conversion was successful and the number is valid
        if (endptr != round + 1 && roundNum > 0 && roundNum <= 999) {
            // R1=101, R2=102, R3=103, etc.
            return 100 + (int)roundNum;
        }
    }
    
    // Unknown format - treat as no participation
    return 0;
}

void DartsRankingModule::filterAndSortPlayers(DartsRankingType type) {
    auto& players_list = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_players : protour_players;
    bool isLiveFormat = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_isLiveFormat : protour_isLiveFormat;
    
    std::vector<DartsPlayer, PsramAllocator<DartsPlayer>> all_players = std::move(players_list);
    players_list.clear();

    for (auto& player : all_players) {
        bool isTracked = false;
        for (const char* trackedName : trackedPlayerNames) {
            if (player.name && strcmp(player.name, trackedName) == 0) {
                isTracked = true;
                break;
            }
        }
        player.isTrackedPlayer = isTracked;
        
        // ** KORREKTUR: 32 -> 40 **
        if ((player.rank > 0 && player.rank <= 40) || isTracked || player.didParticipate) {
            players_list.push_back(std::move(player));
        }
    }
    
    // Sort players based on whether this is a live tournament
    if (isLiveFormat) {
        // During live tournament: sort by round progression first, then by rank
        // Criterion 1: Round (F > HF > QF > R9 > R8 > ... > R1 > --)
        // Criterion 2: Ranking position
        std::sort(players_list.begin(), players_list.end(), [this](const DartsPlayer& a, const DartsPlayer& b) {
            int roundValueA = getRoundSortValue(a.currentRound);
            int roundValueB = getRoundSortValue(b.currentRound);
            
            // Primary: Sort by round (higher value = better round)
            if (roundValueA != roundValueB) {
                return roundValueA > roundValueB;
            }
            
            // Secondary: Sort by rank (lower rank number = better position)
            return a.rank < b.rank;
        });
    } else {
        // No live tournament: just sort by rank (original behavior)
        std::sort(players_list.begin(), players_list.end(), [](const DartsPlayer& a, const DartsPlayer& b) {
            return a.rank < b.rank;
        });
    }
}

PsramString DartsRankingModule::extractText(const char* htmlFragment, size_t maxLen) {
    PsramString result;
    result.reserve(maxLen / 2); // Reserve some space to reduce allocations
    bool inTag = false;
    for (size_t i = 0; i < maxLen && htmlFragment[i] != '\0'; ++i) {
        if (htmlFragment[i] == '<') inTag = true;
        else if (htmlFragment[i] == '>') inTag = false;
        else if (!inTag) result += htmlFragment[i];
    }
    // Remove &pound; and trim - use PsramString operations
    replaceAll(result, PsramString("&pound;"), PsramString(""));
    // Simple trim
    while (!result.empty() && (result[0] == ' ' || result[0] == '\t' || result[0] == '\n' || result[0] == '\r')) {
        result = result.substr(1);
    }
    while (!result.empty()) {
        char c = result[result.length()-1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            result = result.substr(0, result.length()-1);
        } else {
            break;
        }
    }
    return result;
}

void DartsRankingModule::parsePlayerRow(const char* tr_start, const char* tr_end, const PsramVector<PsramString>& headers, DartsPlayer& player, bool isLiveFormat) {
    int col_idx = 0;
    const char* current_pos = tr_start;
    const char* last_round_name = nullptr;

    while (current_pos < tr_end) {
        const char* td_start_tag = strstr(current_pos, "<td");
        if (!td_start_tag || td_start_tag >= tr_end) break;
        
        const char* td_tag_end = strchr(td_start_tag, '>');
        if (!td_tag_end || td_tag_end >= tr_end) break;

        const char* td_content_start = td_tag_end + 1;
        const char* td_end_tag = strstr(td_content_start, "</td>");
        if (!td_end_tag) break;

        PsramString content = extractText(td_content_start, td_end_tag - td_content_start);
        
        if (isLiveFormat) {
            int tag_search_len = td_tag_end - td_start_tag;
            char* temp_td_tag = (char*)ps_malloc(tag_search_len + 1);
            if (temp_td_tag) {
                strncpy(temp_td_tag, td_start_tag, tag_search_len);
                temp_td_tag[tag_search_len] = '\0';

                if (strstr(temp_td_tag, "projout") || strstr(temp_td_tag, "projpast") || strstr(temp_td_tag, "proj now")) {
                    player.didParticipate = true;
                    if(col_idx < headers.size()) {
                       last_round_name = headers[col_idx].c_str();
                    }
                }
                if (strstr(temp_td_tag, "proj now")) {
                    if(player.currentRound) free(player.currentRound);
                    if(col_idx < headers.size()) {
                        player.currentRound = psram_strdup(headers[col_idx].c_str());
                    }
                }
                free(temp_td_tag);
            }
        }

        if (col_idx < headers.size()) {
            const PsramString& header = headers[col_idx];
            if (header == "Rk") player.rank = toInt(content);
            else if (header == "Name") player.name = psram_strdup(content.c_str());
            else if (header == "Prize Money") {
                replaceAll(content, PsramString(","), PsramString(""));
                float money = toFloat(content);
                char* formatted_money = (char*)ps_malloc(16);
                if (formatted_money) {
                    snprintf(formatted_money, 16, "%.2f", money);
                    player.prizeMoney = formatted_money;
                }
            }
            
            if (header == "+/-") {
                player.movementValue = toInt(content);
                if (strstr(td_start_tag, "change-up")) player.movement = PlayerMovement::UP;
                else if (strstr(td_start_tag, "change-down")) player.movement = PlayerMovement::DOWN;
                else player.movement = PlayerMovement::SAME;
            }
        } else if (!isLiveFormat && col_idx == 0) {
             player.rank = toInt(content);
        }
        current_pos = td_end_tag + 5;
        col_idx++;
    }
    
    if (isLiveFormat) {
        if (!player.isActive && last_round_name) {
            if (player.currentRound) free(player.currentRound);
            player.currentRound = psram_strdup(last_round_name);
        }
        else if (!player.didParticipate) {
            if (player.currentRound) free(player.currentRound);
            player.currentRound = psram_strdup("--");
        }
    }
}

void DartsRankingModule::parseHtml(const char* html, size_t len, DartsRankingType type) {
    const char* type_str = (type == DartsRankingType::ORDER_OF_MERIT) ? "OOM" : "ProTour";

    auto& target_players = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_players : protour_players;
    auto& target_main_title = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_mainTitleText : protour_mainTitleText;
    auto& target_sub_title = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_subTitleText : protour_subTitleText;
    auto& target_isLiveFormat = (type == DartsRankingType::ORDER_OF_MERIT) ? oom_isLiveFormat : protour_isLiveFormat;
    
    std::vector<DartsPlayer, PsramAllocator<DartsPlayer>> temp_players;
    temp_players.reserve(256);
    
    if (target_main_title) { free(target_main_title); target_main_title = nullptr; }
    if (target_sub_title) { free(target_sub_title); target_sub_title = nullptr; }

    const char* intr_div_start = strstr(html, "<div id=\"intr\">");
    if (intr_div_start) {
        const char* h2_start = strstr(intr_div_start, "<h2");
        if (h2_start) {
            const char* h2_content_start = strchr(h2_start, '>');
            const char* h2_end = strstr(h2_content_start, "</h2>");
            if (h2_content_start && h2_end) {
                target_main_title = psram_strdup(extractText(h2_content_start + 1, h2_end - (h2_content_start + 1)).c_str());
            }
        }

        PsramString sub_title_builder;
        const char* b_start = strstr(intr_div_start, "<b>");
        if (b_start) {
            const char* b_content_start = b_start + 3;
            const char* b_end = strstr(b_content_start, "</b>");
            if (b_end) {
                sub_title_builder += extractText(b_content_start, b_end - b_content_start).c_str();
            }
        }

        const char* span_start = strstr(intr_div_start, "<span style=\"color:rgb(160, 43, 43)\">");
        if (span_start) {
            const char* span_content_start = span_start + strlen("<span style=\"color:rgb(160, 43, 43)\">");
            const char* span_end = strstr(span_content_start, "</span>");
            if (span_end) {
                if (sub_title_builder.length() > 0) sub_title_builder += " ";
                sub_title_builder += extractText(span_content_start, span_end - span_content_start).c_str();
            }
        }
        
        if (sub_title_builder.length() > 0) {
            target_sub_title = psram_strdup(sub_title_builder.c_str());
        }
    }
    
    bool isLiveFormat = false;
    parseTable(html, temp_players, isLiveFormat);
    target_isLiveFormat = isLiveFormat;
    target_players = std::move(temp_players);

    filterAndSortPlayers(type);
}

bool DartsRankingModule::parseTable(const char* html, std::vector<DartsPlayer, PsramAllocator<DartsPlayer>>& players_ref, bool& isLiveFormat) {
    const char* table_start = strstr(html, "<table");
    if (!table_start) {
        return false;
    }

    PsramVector<PsramString> headers;
    const char* thead_start = strstr(table_start, "<thead>");
    const char* thead_end = thead_start ? strstr(thead_start, "</thead>") : nullptr;
    isLiveFormat = false;

    if (thead_start && thead_end) {
        const char* th_start = thead_start;
        while (th_start < thead_end && (th_start = strstr(th_start, "<th")) != nullptr) {
            const char* th_content_start = strchr(th_start, '>');
            if (!th_content_start) break;
            th_content_start++;
            const char* th_end_tag = strstr(th_content_start, "</th>"); 
            if (!th_end_tag) break;
            
            PsramString headerText = extractText(th_content_start, th_end_tag - th_content_start);
            headers.push_back(headerText);
            
            if (headerText != "Rk" && headerText != "+/-" && headerText != "Name" && headerText != "Prize Money" && headerText.length() > 0 && headerText.length() <= 4) {
                isLiveFormat = true;
            }
            th_start = th_end_tag;
        }
    }
    
    if (headers.empty()) {
        return false;
    }

    const char* tbody_start = strstr(table_start, "<tbody");
    if (!tbody_start) {
        tbody_start = table_start;
    }

    const char* current_pos = tbody_start;
    while (current_pos) {
        const char* tr_start = strstr(current_pos, "<tr");
        if (!tr_start) break;
        const char* tr_end = strstr(tr_start, "</tr>"); 
        if (!tr_end) break;
        
        vTaskDelay(1);

        DartsPlayer player;
        if (isLiveFormat) {
             const char* tr_open_end = strchr(tr_start, '>');
            if (tr_open_end && tr_open_end < tr_end) {
                char temp_tag[128];
                int tag_len = tr_open_end - tr_start;
                if (tag_len < sizeof(temp_tag) - 1) {
                    strncpy(temp_tag, tr_start, tag_len);
                    temp_tag[tag_len] = '\0';
                    if (strstr(temp_tag, "stillin")) player.isActive = true;
                }
            }
        }
        
        parsePlayerRow(tr_start, tr_end, headers, player, isLiveFormat);
        if (player.rank > 0 && player.name) {
            players_ref.push_back(std::move(player));
        }
        current_pos = tr_end;
    }
    return true;
}

void DartsRankingModule::resetScroll() {
    // Nur Spielernamen-Scroller zurücksetzen, NICHT den Untertitel
    // Der Untertitel soll kontinuierlich weiterscrollen
    if (_pixelScroller) {
        _pixelScroller->reset();
    }
}

void DartsRankingModule::resetAllScrollers() {
    // Beide Scroller zurücksetzen (bei Ranking-Typ-Wechsel)
    if (_pixelScroller) {
        _pixelScroller->reset();
    }
    if (_subtitleScroller) {
        _subtitleScroller->reset();
    }
}

void DartsRankingModule::ensureScrollPos(size_t requiredSize) {
    if (_pixelScroller) {
        _pixelScroller->ensureSlots(requiredSize);
    }
}