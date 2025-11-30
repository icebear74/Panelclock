#include "HolidayAnimationsModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include <time.h>

// Hilfsfunktion für RGB565
uint16_t HolidayAnimationsModule::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Hilfsfunktion um Hex-Farbe zu RGB565 zu konvertieren
uint16_t HolidayAnimationsModule::hexToRgb565(const char* hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        return rgb565(255, 255, 255);
    }
    long r = strtol(PsramString(hex + 1, 2).c_str(), NULL, 16);
    long g = strtol(PsramString(hex + 3, 2).c_str(), NULL, 16);
    long b = strtol(PsramString(hex + 5, 2).c_str(), NULL, 16);
    return rgb565(r, g, b);
}

// Einfacher Pseudo-Zufallsgenerator
uint32_t HolidayAnimationsModule::simpleRandom(uint32_t seed) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

HolidayAnimationsModule::HolidayAnimationsModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, 
                                       GeneralTimeConverter& timeConverter, DeviceConfig* config)
    : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), config(config) {
}

HolidayAnimationsModule::~HolidayAnimationsModule() {
}

void HolidayAnimationsModule::begin() {
    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);
    _lastCheckedDay = tm_now.tm_mday;
    setConfig();
    shuffleCandleOrder();
    Log.println("[AdventWreath] Modul initialisiert");
}

void HolidayAnimationsModule::setConfig() {
    if (config) {
        _displayDurationMs = config->adventWreathDisplaySec * 1000UL;
        _repeatIntervalMs = config->adventWreathRepeatMin * 60UL * 1000UL;
        _flameAnimationMs = config->adventWreathFlameSpeedMs;
    }
}

void HolidayAnimationsModule::onUpdate(std::function<void()> callback) {
    _updateCallback = callback;
}

void HolidayAnimationsModule::shuffleCandleOrder() {
    time_t now_utc;
    time(&now_utc);
    uint32_t seed = (uint32_t)now_utc + _displayCounter;
    
    for (int i = 3; i > 0; i--) {
        seed = simpleRandom(seed);
        int j = seed % (i + 1);
        int temp = _candleOrder[i];
        _candleOrder[i] = _candleOrder[j];
        _candleOrder[j] = temp;
    }
    _lastOrderSeed = seed;
}

bool HolidayAnimationsModule::isAdventSeason() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int month = tm_now.tm_mon + 1;
    int day = tm_now.tm_mday;
    int year = tm_now.tm_year + 1900;
    
    // Option: Erst ab dem 1. Advent zeigen
    if (config && config->adventWreathOnlyFromFirstAdvent) {
        // Berechne 1. Advent (4. Advent - 21 Tage)
        time_t fourthAdvent = calculateFourthAdvent(year);
        time_t firstAdvent = fourthAdvent - (21 * 24 * 60 * 60);
        
        // Prüfe ob wir vor dem 1. Advent sind
        if (local_now < firstAdvent) {
            return false;
        }
    } else {
        // Berechne Start- und Enddatum basierend auf "Tage vor dem 24."
        int daysBefore = config ? config->adventWreathDaysBefore24 : 30;
        if (daysBefore > 30) daysBefore = 30;
        if (daysBefore < 0) daysBefore = 0;
        
        // Startdatum: 24. Dezember minus daysBefore
        int startDay = 24 - daysBefore;
        int startMonth = 12;
        if (startDay <= 0) {
            startDay += 30;  // November hat 30 Tage
            startMonth = 11;
        }
        
        // Prüfe ob wir vor dem Startdatum sind
        if (startMonth == 11 && month == 11 && day < startDay) {
            return false;
        }
        if (month < 11) {
            return false;
        }
    }
    
    // Adventskranz wird bis zum 24.12. gezeigt
    if (month == 12 && day <= 24) {
        return true;
    } else if (month == 11) {
        return true;
    }
    
    return false;
}

bool HolidayAnimationsModule::isChristmasSeason() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int month = tm_now.tm_mon + 1;
    int day = tm_now.tm_mday;
    
    // Berechne Start- und Enddatum basierend auf "Tage vor/nach dem 24."
    int daysBefore = config ? config->christmasTreeDaysBefore24 : 23;
    int daysAfter = config ? config->christmasTreeDaysAfter24 : 7;
    if (daysBefore > 30) daysBefore = 30;
    if (daysBefore < 0) daysBefore = 0;
    if (daysAfter > 30) daysAfter = 30;
    if (daysAfter < 0) daysAfter = 0;
    
    // Startdatum: 24. Dezember minus daysBefore
    int startDay = 24 - daysBefore;
    int startMonth = 12;
    if (startDay <= 0) {
        startDay += 30;
        startMonth = 11;
    }
    
    // Enddatum: 24. Dezember plus daysAfter
    int endDay = 24 + daysAfter;
    int endMonth = 12;
    if (endDay > 31) {
        endDay -= 31;
        endMonth = 1;  // Januar nächstes Jahr
    }
    
    // Prüfe ob aktuelles Datum im Bereich liegt
    if (endMonth == 1) {
        // Bereich geht über Jahreswechsel
        if (month == 12 && day >= startDay) return true;
        if (month == 1 && day <= endDay) return true;
    } else if (startMonth == 11) {
        // Bereich startet im November
        if (month == 11 && day >= startDay) return true;
        if (month == 12 && day <= endDay) return true;
    } else {
        // Alles im Dezember
        if (month == 12 && day >= startDay && day <= endDay) return true;
    }
    
    return false;
}

bool HolidayAnimationsModule::isHolidaySeason() {
    return isAdventSeason() || isChristmasSeason() || isFireplaceSeason();
}

bool HolidayAnimationsModule::isFireplaceSeason() {
    if (!config || !config->fireplaceEnabled) return false;
    
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int month = tm_now.tm_mon + 1;
    int day = tm_now.tm_mday;
    
    int daysBefore = config->fireplaceDaysBefore24;
    int daysAfter = config->fireplaceDaysAfter24;
    if (daysBefore > 30) daysBefore = 30;
    if (daysBefore < 0) daysBefore = 0;
    if (daysAfter > 30) daysAfter = 30;
    if (daysAfter < 0) daysAfter = 0;
    
    int startDay = 24 - daysBefore;
    int startMonth = 12;
    if (startDay <= 0) {
        startDay += 30;
        startMonth = 11;
    }
    
    int endDay = 24 + daysAfter;
    int endMonth = 12;
    if (endDay > 31) {
        endDay -= 31;
        endMonth = 1;
    }
    
    if (endMonth == 1) {
        if (month == 12 && day >= startDay) return true;
        if (month == 1 && day <= endDay) return true;
    } else if (startMonth == 11) {
        if (month == 11 && day >= startDay) return true;
        if (month == 12 && day <= endDay) return true;
    } else {
        if (month == 12 && day >= startDay && day <= endDay) return true;
    }
    
    return false;
}

ChristmasDisplayMode HolidayAnimationsModule::getCurrentDisplayMode() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int month = tm_now.tm_mon + 1;
    int day = tm_now.tm_mday;
    bool treeEnabled = config ? config->christmasTreeEnabled : true;
    bool wreathEnabled = config ? config->adventWreathEnabled : true;
    bool fireplaceEnabled = config ? config->fireplaceEnabled : true;
    
    // Zähle aktive Modi
    int activeCount = 0;
    if (wreathEnabled && isAdventSeason()) activeCount++;
    if (treeEnabled && isChristmasSeason()) activeCount++;
    if (fireplaceEnabled && isFireplaceSeason()) activeCount++;
    
    // Adventskranz bis zum 24.12., danach nur noch Baum/Kamin
    if (month == 12 && day > 24) {
        if (treeEnabled && fireplaceEnabled) return ChristmasDisplayMode::Alternate;
        if (treeEnabled) return ChristmasDisplayMode::Tree;
        if (fireplaceEnabled) return ChristmasDisplayMode::Fireplace;
        return ChristmasDisplayMode::Tree;
    } else if (month == 1) {
        if (treeEnabled && fireplaceEnabled) return ChristmasDisplayMode::Alternate;
        if (treeEnabled) return ChristmasDisplayMode::Tree;
        if (fireplaceEnabled) return ChristmasDisplayMode::Fireplace;
        return ChristmasDisplayMode::Tree;
    }
    
    // Mehrere aktiv = Wechsel
    if (activeCount > 1) {
        return ChristmasDisplayMode::Alternate;
    }
    
    // Nur einer aktiv
    if (treeEnabled && isChristmasSeason()) return ChristmasDisplayMode::Tree;
    if (fireplaceEnabled && isFireplaceSeason()) return ChristmasDisplayMode::Fireplace;
    
    return ChristmasDisplayMode::Wreath;
}

int HolidayAnimationsModule::calculateCurrentAdvent() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int year = tm_now.tm_year + 1900;
    int month = tm_now.tm_mon + 1;
    
    if (month < 11 || month > 12) {
        return 0;
    }
    
    time_t fourthAdvent = calculateFourthAdvent(year);
    struct tm tm_fourth;
    localtime_r(&fourthAdvent, &tm_fourth);
    
    struct tm tm_today = tm_now;
    tm_today.tm_hour = 12;
    tm_today.tm_min = 0;
    tm_today.tm_sec = 0;
    time_t today = mktime(&tm_today);
    
    int diffDays = (int)((fourthAdvent - today) / 86400);
    
    if (diffDays == 0) return 4;
    if (diffDays == 7) return 3;
    if (diffDays == 14) return 2;
    if (diffDays == 21) return 1;
    
    if (diffDays < 0) return 4;
    if (diffDays < 7) return 3;
    if (diffDays < 14) return 2;
    if (diffDays < 21) return 1;
    
    return 0;
}

time_t HolidayAnimationsModule::calculateFourthAdvent(int year) {
    struct tm tm_christmas;
    memset(&tm_christmas, 0, sizeof(tm_christmas));
    tm_christmas.tm_year = year - 1900;
    tm_christmas.tm_mon = 11;
    tm_christmas.tm_mday = 24;
    tm_christmas.tm_hour = 12;
    mktime(&tm_christmas);
    
    int daysToSubtract = tm_christmas.tm_wday;
    if (daysToSubtract == 0) {
        daysToSubtract = 0;
    }
    
    tm_christmas.tm_mday -= daysToSubtract;
    return mktime(&tm_christmas);
}

void HolidayAnimationsModule::periodicTick() {
    if (!config) return;
    
    if (!config->adventWreathEnabled && !config->christmasTreeEnabled) return;
    
    unsigned long now = millis();
    if (now - _lastPeriodicCheck < 1000) return;
    _lastPeriodicCheck = now;
    
    if (!isHolidaySeason()) {
        if (_isAdventViewActive) {
            releasePriorityEx(_currentAdventUID);
            _isAdventViewActive = false;
            _requestPending = false;
            Log.println("[AdventWreath] Keine Weihnachtszeit mehr");
        }
        return;
    }
    
    // Wenn ein Request noch aussteht (noch nicht aktiviert), nichts tun
    if (_requestPending) {
        return;
    }
    
    unsigned long minInterval = (_lastAdventDisplayTime == 0) ? 0 : _repeatIntervalMs;
    
    if (!_isAdventViewActive && (now - _lastAdventDisplayTime > minInterval)) {
        // Entscheide WAS angezeigt wird BEVOR der Request gemacht wird
        shuffleCandleOrder();
        
        ChristmasDisplayMode mode = getCurrentDisplayMode();
        if (mode == ChristmasDisplayMode::Alternate) {
            // Rotiere durch alle aktiven Modi
            bool wreathActive = config->adventWreathEnabled && isAdventSeason();
            bool treeActive = config->christmasTreeEnabled && isChristmasSeason();
            bool fireplaceActive = config->fireplaceEnabled && isFireplaceSeason();
            
            int activeCount = (wreathActive ? 1 : 0) + (treeActive ? 1 : 0) + (fireplaceActive ? 1 : 0);
            int modeIndex = _displayCounter % activeCount;
            
            _showTree = false;
            _showFireplace = false;
            
            int current = 0;
            if (wreathActive) {
                if (current == modeIndex) { /* Kranz */ }
                current++;
            }
            if (treeActive && current <= modeIndex) {
                if (current == modeIndex) { _showTree = true; }
                current++;
            }
            if (fireplaceActive && current <= modeIndex) {
                if (current == modeIndex) { _showFireplace = true; _showTree = false; }
            }
        } else if (mode == ChristmasDisplayMode::Tree) {
            _showTree = true;
            _showFireplace = false;
        } else if (mode == ChristmasDisplayMode::Fireplace) {
            _showTree = false;
            _showFireplace = true;
        } else {
            _showTree = false;
            _showFireplace = false;
        }
        
        // Feste UID für diese Anzeige-Session (nicht vom Advent-Woche abhängig)
        // Verwende eine einfache UID basierend auf dem Display-Counter
        _currentAdventUID = ADVENT_WREATH_UID_BASE + (_displayCounter % 100);
        
        unsigned long safeDuration = _displayDurationMs + 5000UL;
        Priority prio = config->adventWreathInterrupt ? Priority::Low : Priority::PlayNext;
        
        _requestPending = true;  // Markiere Request als ausstehend
        bool success = requestPriorityEx(prio, _currentAdventUID, safeDuration);
        
        if (success) {
            const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
            Log.printf("[AdventWreath] %s %s angefordert (UID=%lu, Counter=%d)\n", 
                       modeName,
                       config->adventWreathInterrupt ? "Interrupt" : "PlayNext",
                       _currentAdventUID, _displayCounter);
            _displayCounter++;  // Nur erhöhen wenn Request erfolgreich
        } else {
            Log.println("[AdventWreath] Request abgelehnt!");
            _requestPending = false;  // Request fehlgeschlagen, zurücksetzen
        }
    }
    else if (_isAdventViewActive && (now - _adventViewStartTime > _displayDurationMs)) {
        releasePriorityEx(_currentAdventUID);
        _isAdventViewActive = false;
        _requestPending = false;
        _lastAdventDisplayTime = now;
    }
}

void HolidayAnimationsModule::tick() {
    unsigned long now = millis();
    bool needsUpdate = false;
    
    // Flammen-Animation (Kerzen)
    if (now - _lastFlameUpdate > _flameAnimationMs) {
        _lastFlameUpdate = now;
        _flamePhase = (_flamePhase + 1) % 32;
        needsUpdate = true;
    }
    
    // Baum-Lichter-Animation (konfigurierbare Geschwindigkeit)
    unsigned long treeLightSpeed = config ? (unsigned long)config->christmasTreeLightSpeedMs : 80;
    if (now - _lastTreeLightUpdate > treeLightSpeed) {
        _lastTreeLightUpdate = now;
        _treeLightPhase = (_treeLightPhase + 1) % 24;
        needsUpdate = true;
    }
    
    // Kaminfeuer-Animation
    unsigned long fireplaceSpeed = config ? (unsigned long)config->fireplaceFlameSpeedMs : 40;
    static unsigned long _lastFireplaceUpdate = 0;
    if (now - _lastFireplaceUpdate > fireplaceSpeed) {
        _lastFireplaceUpdate = now;
        _fireplaceFlamePhase = (_fireplaceFlamePhase + 1) % 24;
        needsUpdate = true;
    }
    
    if (needsUpdate && _updateCallback) {
        _updateCallback();
    }
}

void HolidayAnimationsModule::logicTick() {
}

bool HolidayAnimationsModule::wantsFullscreen() const {
    return config && config->adventWreathFullscreen && _fullscreenCanvas != nullptr;
}

void HolidayAnimationsModule::draw() {
    // Wähle den richtigen Canvas basierend auf Fullscreen-Modus
    _currentCanvas = wantsFullscreen() ? _fullscreenCanvas : &canvas;
    
    // Hintergrundfarbe basierend auf Modus
    uint16_t bgColor = 0;
    if (_showFireplace && config && config->fireplaceBgColor.length() > 0) {
        bgColor = hexToRgb565(config->fireplaceBgColor.c_str());
    } else if (_showTree && config && config->christmasTreeBgColor.length() > 0) {
        bgColor = hexToRgb565(config->christmasTreeBgColor.c_str());
    } else if (!_showFireplace && !_showTree && config && config->adventWreathBgColor.length() > 0) {
        bgColor = hexToRgb565(config->adventWreathBgColor.c_str());
    }
    
    _currentCanvas->fillScreen(bgColor);
    u8g2.begin(*_currentCanvas);
    
    if (_showFireplace) {
        drawFireplace();
    } else if (_showTree) {
        drawChristmasTree();
    } else {
        // Adventskranz ohne Beschriftung - mehr Platz für die Grafik
        drawGreenery();
        drawWreath();
        drawBerries();
    }
}

void HolidayAnimationsModule::drawChristmasTree() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    int centerX = canvasW / 2;
    
    // Dynamische Skalierung basierend auf Canvas-Höhe
    float scaleY = canvasH / 66.0f;  // 66 ist die Referenz-Höhe
    float scaleX = canvasW / 192.0f;  // 192 ist die Referenz-Breite
    float scale = min(scaleX, scaleY);
    
    int baseY = canvasH - 4;  // Basis immer am unteren Rand
    
    // Skalierte Baumhöhe
    int treeHeight = (int)(54 * scale);
    int trunkHeight = (int)(10 * scale);
    int trunkWidth = (int)(8 * scale);
    
    // Zeichne zuerst den Baumstamm
    uint16_t trunkColor = rgb565(139, 69, 19);
    uint16_t trunkDark = rgb565(100, 50, 15);
    _currentCanvas->fillRect(centerX - trunkWidth/2, baseY - trunkHeight, trunkWidth, trunkHeight + 2, trunkColor);
    _currentCanvas->drawLine(centerX - trunkWidth/2, baseY - trunkHeight, centerX - trunkWidth/2, baseY + 2, trunkDark);
    
    // Natürlicherer Baum mit mehreren Grüntönen und organischer Form
    drawNaturalTree(centerX, baseY - trunkHeight + 2, scale);
    
    // Stern an der Spitze
    uint16_t starColor = rgb565(255, 255, 0);
    uint16_t starGlow = rgb565(255, 230, 100);
    int starY = baseY - trunkHeight - treeHeight + (int)(6 * scale);
    int starSize = max(2, (int)(3 * scale));
    _currentCanvas->fillCircle(centerX, starY, starSize, starColor);
    // Strahlen
    int rayLen = (int)(5 * scale);
    _currentCanvas->drawLine(centerX, starY - rayLen, centerX, starY + rayLen, starGlow);
    _currentCanvas->drawLine(centerX - rayLen, starY, centerX + rayLen, starY, starGlow);
    _currentCanvas->drawLine(centerX - rayLen/2, starY - rayLen/2, centerX + rayLen/2, starY + rayLen/2, starGlow);
    _currentCanvas->drawLine(centerX - rayLen/2, starY + rayLen/2, centerX + rayLen/2, starY - rayLen/2, starGlow);
    
    // Kugeln am Baum
    drawTreeOrnaments(centerX, baseY - trunkHeight + 2, scale);
    
    // Blinkende Lichter
    drawTreeLights();
    
    // Geschenke unter dem Baum
    drawGifts(centerX, baseY, scale);
}

void HolidayAnimationsModule::drawNaturalTree(int centerX, int baseY, float scale) {
    // Grüntöne für natürlicheren Look
    uint16_t greens[] = {
        rgb565(0, 80, 0),     // Dunkelgrün
        rgb565(0, 100, 20),   // Mittelgrün
        rgb565(20, 120, 30),  // Hellgrün
        rgb565(0, 90, 10),    // Dunkel-Mittel
        rgb565(34, 100, 34)   // Forest Green
    };
    int numGreens = sizeof(greens) / sizeof(greens[0]);
    
    // Skalierte Werte
    int layer1Height = (int)(18 * scale);
    int layer2Height = (int)(18 * scale);
    int layer3Height = (int)(18 * scale);
    int layer1Width = (int)(28 * scale);
    int layer2Width = (int)(22 * scale);
    int layer3Width = (int)(16 * scale);
    
    // Unterste Schicht (breiteste)
    int layer1Top = baseY;
    int layer1Bottom = baseY - layer1Height;
    for (int y = layer1Top; y >= layer1Bottom; y--) {
        int progress = layer1Top - y;
        int maxWidth = layer1Width - (int)(progress * 0.8);
        
        for (int x = -maxWidth; x <= maxWidth; x++) {
            uint32_t seed = simpleRandom((y * 47 + x * 13) ^ 0xDEAD);
            int edgeVar = (seed % 3) - 1;
            if (abs(x) <= maxWidth + edgeVar) {
                int colorIdx = seed % numGreens;
                _currentCanvas->drawPixel(centerX + x, y, greens[colorIdx]);
            }
        }
    }
    
    // Mittlere Schicht
    int layer2Top = baseY - (int)(14 * scale);
    int layer2Bottom = layer2Top - layer2Height;
    for (int y = layer2Top; y >= layer2Bottom; y--) {
        int progress = layer2Top - y;
        int maxWidth = layer2Width - (int)(progress * 0.9);
        
        for (int x = -maxWidth; x <= maxWidth; x++) {
            uint32_t seed = simpleRandom((y * 53 + x * 17) ^ 0xBEEF);
            int edgeVar = (seed % 3) - 1;
            if (abs(x) <= maxWidth + edgeVar) {
                int colorIdx = seed % numGreens;
                _currentCanvas->drawPixel(centerX + x, y, greens[colorIdx]);
            }
        }
    }
    
    // Oberste Schicht (Spitze)
    int layer3Top = baseY - (int)(28 * scale);
    int layer3Bottom = layer3Top - layer3Height;
    for (int y = layer3Top; y >= layer3Bottom; y--) {
        int progress = layer3Top - y;
        int maxWidth = layer3Width - (int)(progress * 0.85);
        if (maxWidth < 1) maxWidth = 1;
        
        for (int x = -maxWidth; x <= maxWidth; x++) {
            uint32_t seed = simpleRandom((y * 59 + x * 19) ^ 0xCAFE);
            int edgeVar = (seed % 2);
            if (abs(x) <= maxWidth + edgeVar) {
                int colorIdx = seed % numGreens;
                _currentCanvas->drawPixel(centerX + x, y, greens[colorIdx]);
            }
        }
    }
    
    // Extra Schicht bei Fullscreen (4. Ebene)
    if (scale > 1.2) {
        int layer4Top = baseY - (int)(42 * scale);
        int layer4Bottom = layer4Top - (int)(14 * scale);
        int layer4Width = (int)(10 * scale);
        for (int y = layer4Top; y >= layer4Bottom; y--) {
            int progress = layer4Top - y;
            int maxWidth = layer4Width - (int)(progress * 0.9);
            if (maxWidth < 1) maxWidth = 1;
            
            for (int x = -maxWidth; x <= maxWidth; x++) {
                uint32_t seed = simpleRandom((y * 61 + x * 23) ^ 0xFACE);
                int edgeVar = (seed % 2);
                if (abs(x) <= maxWidth + edgeVar) {
                    int colorIdx = seed % numGreens;
                    _currentCanvas->drawPixel(centerX + x, y, greens[colorIdx]);
                }
            }
        }
    }
}

void HolidayAnimationsModule::drawTreeOrnaments(int centerX, int baseY, float scale) {
    uint16_t ornamentColors[] = {
        rgb565(255, 0, 0),      // Rot
        rgb565(255, 215, 0),    // Gold
        rgb565(0, 100, 200),    // Blau
        rgb565(255, 0, 255),    // Magenta
        rgb565(200, 50, 50),    // Dunkelrot
        rgb565(255, 140, 0),    // Orange
        rgb565(100, 200, 255),  // Hellblau
        rgb565(220, 220, 220)   // Silber
    };
    int numColors = 8;
    
    // Skalierte Kugelpositionen
    int numOrnaments = scale > 1.2 ? 14 : 11;  // Mehr Kugeln bei Fullscreen
    
    for (int i = 0; i < numOrnaments; i++) {
        uint32_t seed = simpleRandom(i * 123 + 456);
        
        // Berechne Position basierend auf Skala
        int yOffset = (int)((seed % 40) * scale);
        int xRange = (int)((24 - yOffset * 0.5) * scale);
        if (xRange < 3) xRange = 3;
        
        int ox = centerX - xRange + (int)((seed / 7) % (xRange * 2));
        int oy = baseY - (int)(12 * scale) - yOffset;
        int radius = 2 + (seed % 2);
        if (scale > 1.2) radius = 2 + (seed % 3);
        
        uint16_t color = ornamentColors[seed % numColors];
        drawOrnament(ox, oy, radius, color);
    }
}

void HolidayAnimationsModule::drawTreeLights() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    int centerX = canvasW / 2;
    
    // Dynamische Skalierung wie im Baum
    float scaleY = canvasH / 66.0f;
    float scale = scaleY;
    
    // baseY muss mit dem Baum übereinstimmen
    int trunkHeight = (int)(10 * scale);
    int baseY = canvasH - 4 - trunkHeight + 2;  // Gleiche Berechnung wie in drawChristmasTree
    
    // Konfigurierbare Anzahl und Geschwindigkeit
    int lightCount = config ? config->christmasTreeLightCount : 18;
    if (lightCount < 5) lightCount = 5;
    if (lightCount > 30) lightCount = 30;
    
    int lightMode = config ? config->christmasTreeLightMode : 0;
    uint16_t fixedColor = rgb565(255, 215, 0);
    
    if (lightMode == 1 && config) {
        fixedColor = hexToRgb565(config->christmasTreeLightColor.c_str());
    }
    
    uint16_t lightColors[] = {
        rgb565(255, 255, 100),  // Gelb
        rgb565(255, 100, 100),  // Rot
        rgb565(100, 255, 100),  // Grün
        rgb565(100, 100, 255),  // Blau
        rgb565(255, 150, 255),  // Rosa
        rgb565(255, 200, 100)   // Warmweiß
    };
    int numColors = 6;
    
    // Skalierte Baumhöhe (muss mit drawNaturalTree übereinstimmen)
    int treeHeight = (int)(54 * scale);
    
    // Lichter über den Baum verteilt (pseudo-zufällig basierend auf lightCount)
    for (int i = 0; i < lightCount; i++) {
        // Berechne Position basierend auf Index
        uint32_t seed = simpleRandom(i * 37 + 789);
        
        // Y-Position: verteilt innerhalb des Baumes (skaliert)
        int yRange = (int)(treeHeight * 0.85);  // Skalierte Y-Range
        int lightY = baseY - (int)(8 * scale) - (seed % yRange);
        
        // X-Position abhängig von Y (breiter unten, schmaler oben)
        int progress = baseY - (int)(8 * scale) - lightY;
        int maxXBase = (int)(28 * scale);  // Skalierte Baumbreite unten
        float progressRatio = (float)progress / yRange;
        int maxX = (int)(maxXBase * (1.0f - progressRatio * 0.7f));  // Breite nimmt nach oben ab
        if (maxX < 3) maxX = 3;
        
        seed = simpleRandom(seed);
        int lightX = centerX - maxX + (seed % (maxX * 2));
        
        // Blinken basierend auf Phase
        seed = simpleRandom(seed + i * 11);
        bool isOn = ((i + _treeLightPhase + (seed % 3)) % 4) < 2;
        
        if (isOn) {
            uint16_t color;
            if (lightMode == 1) {
                // Feste Farbe mit leichter Variation
                color = fixedColor;
            } else {
                // Zufällige Farbe
                color = lightColors[(seed / 7) % numColors];
            }
            
            _currentCanvas->fillCircle(lightX, lightY, 1, color);
            // Kleiner Glow-Effekt
            _currentCanvas->drawPixel(lightX, lightY - 1, color);
        }
    }
}

void HolidayAnimationsModule::drawGifts(int centerX, int baseY, float scale) {
    // Konfigurierbare Anzahl der Geschenke (0-10)
    int giftCount = config ? config->christmasTreeGiftCount : 5;
    if (giftCount < 0) giftCount = 0;
    if (giftCount > 10) giftCount = 10;
    
    if (giftCount == 0) return;  // Keine Geschenke zeichnen
    
    // Geschenkfarben (Hauptfarbe, Bandfarbe)
    uint16_t giftColors[][2] = {
        {rgb565(200, 0, 0), rgb565(255, 215, 0)},      // Rot mit Gold
        {rgb565(0, 100, 200), rgb565(255, 255, 255)},  // Blau mit Weiß
        {rgb565(0, 150, 0), rgb565(255, 0, 0)},        // Grün mit Rot
        {rgb565(150, 0, 150), rgb565(255, 215, 0)},    // Lila mit Gold
        {rgb565(255, 140, 0), rgb565(200, 0, 0)},      // Orange mit Rot
        {rgb565(200, 50, 100), rgb565(255, 255, 255)}, // Pink mit Weiß
        {rgb565(100, 200, 200), rgb565(255, 215, 0)},  // Türkis mit Gold
        {rgb565(150, 100, 50), rgb565(200, 150, 100)}, // Braun mit Beige
        {rgb565(100, 100, 200), rgb565(255, 200, 100)},// Indigo mit Gelb
        {rgb565(200, 200, 0), rgb565(200, 0, 0)}       // Gelb mit Rot
    };
    int numColors = 10;
    
    // Skalierte Geschenkpositionen - breiter verteilt bei Fullscreen
    int maxOffset = (int)(55 * scale);
    
    for (int i = 0; i < giftCount && i < 10; i++) {
        uint32_t seed = simpleRandom(i * 97 + 321);
        
        // Position berechnen - gleichmäßig verteilt links und rechts
        int side = (i % 2 == 0) ? -1 : 1;
        int baseOffset = (int)(25 + (i / 2) * 15);
        int xOffset = side * (int)(baseOffset * scale) + (int)((seed % 8) - 4);
        
        int gx = centerX + xOffset;
        int gy = baseY - 2;
        int gw = (int)((8 + (seed % 5)) * scale);
        int gh = (int)((5 + (seed % 4)) * scale);
        bool hasRibbon = (seed % 3) == 0;
        
        int colorIdx = i % numColors;
        uint16_t mainColor = giftColors[colorIdx][0];
        uint16_t ribbonColor = giftColors[colorIdx][1];
        
        // Dunkler Rand
        uint8_t r = ((mainColor >> 11) & 0x1F) * 6;
        uint8_t g = ((mainColor >> 5) & 0x3F) * 3;
        uint8_t b = (mainColor & 0x1F) * 6;
        uint16_t borderColor = rgb565(r, g, b);
        
        // Geschenk zeichnen
        _currentCanvas->fillRect(gx, gy - gh, gw, gh, mainColor);
        _currentCanvas->drawRect(gx, gy - gh, gw, gh, borderColor);
        
        // Band (horizontal und vertikal)
        _currentCanvas->drawLine(gx + gw/2, gy - gh, gx + gw/2, gy, ribbonColor);
        _currentCanvas->drawLine(gx, gy - gh/2, gx + gw, gy - gh/2, ribbonColor);
        
        // Schleife oben (nur für größere Geschenke)
        if (hasRibbon && gw >= 8) {
            int ribbonSize = max(1, (int)(2 * scale));
            _currentCanvas->fillCircle(gx + gw/2 - ribbonSize, gy - gh - ribbonSize, ribbonSize, ribbonColor);
            _currentCanvas->fillCircle(gx + gw/2 + ribbonSize, gy - gh - ribbonSize, ribbonSize, ribbonColor);
        }
    }
}

void HolidayAnimationsModule::drawOrnament(int x, int y, int radius, uint16_t color) {
    _currentCanvas->fillCircle(x, y, radius, color);
    
    uint8_t r = ((color >> 11) & 0x1F) * 8;
    uint8_t g = ((color >> 5) & 0x3F) * 4;
    uint8_t b = (color & 0x1F) * 8;
    int rH = r + 100; if (rH > 255) rH = 255;
    int gH = g + 100; if (gH > 255) gH = 255;
    int bH = b + 100; if (bH > 255) bH = 255;
    uint16_t highlight = rgb565(rH, gH, bH);
    
    if (radius >= 2) {
        _currentCanvas->drawPixel(x - radius/2, y - radius/2, highlight);
    }
    
    uint16_t shadow = rgb565(r / 2, g / 2, b / 2);
    if (radius >= 2) {
        _currentCanvas->drawPixel(x + radius/2, y + radius/2, shadow);
    }
}

void HolidayAnimationsModule::drawWreath() {
    int currentAdvent = calculateCurrentAdvent();
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    int centerX = canvasW / 2;
    
    // Dynamische Skalierung - nutze den GESAMTEN verfügbaren Platz
    float scale = canvasH / 66.0f;  // Bei 96px Höhe ist scale = 1.45
    
    // BaseY so setzen, dass der Kranz den gesamten Bereich nutzt
    // Kerzen sollen ca. 35% der Höhe einnehmen, Flammen nochmal 15%
    int candleHeight = (int)(28 * scale);  // Größere Kerzen
    int flameHeight = (int)(14 * scale);
    int bottomMargin = (int)(8 * scale);
    
    int baseY = canvasH - bottomMargin;  // Basis am unteren Rand
    
    // Kerzenabstand - breiter verteilen über die gesamte Breite
    int totalWidth = canvasW - 40;  // 20px Rand links und rechts
    int candleSpacing = totalWidth / 4;
    int startX = 20 + candleSpacing / 2;
    
    int candlePositions[4] = {
        startX,
        startX + candleSpacing,
        startX + candleSpacing * 2,
        startX + candleSpacing * 3
    };
    
    uint16_t candleColors[4];
    
    if (config && config->adventWreathColorMode == 0) {
        candleColors[0] = rgb565(128, 0, 128);
        candleColors[1] = rgb565(128, 0, 128);
        candleColors[2] = rgb565(255, 105, 180);
        candleColors[3] = rgb565(128, 0, 128);
    } else if (config && config->adventWreathColorMode == 1) {
        candleColors[0] = rgb565(255, 0, 0);
        candleColors[1] = rgb565(255, 215, 0);
        candleColors[2] = rgb565(0, 128, 0);
        candleColors[3] = rgb565(255, 255, 255);
    } else if (config && config->adventWreathColorMode == 2) {
        PsramString colors = config->adventWreathCustomColors;
        int colorIdx = 0;
        size_t pos = 0;
        while (colorIdx < 4 && pos < colors.length()) {
            size_t commaPos = colors.find(',', pos);
            if (commaPos == PsramString::npos) commaPos = colors.length();
            PsramString hexColor = colors.substr(pos, commaPos - pos);
            candleColors[colorIdx] = hexToRgb565(hexColor.c_str());
            pos = commaPos + 1;
            colorIdx++;
        }
        while (colorIdx < 4) {
            candleColors[colorIdx] = rgb565(255, 255, 255);
            colorIdx++;
        }
    } else {
        candleColors[0] = rgb565(255, 0, 0);
        candleColors[1] = rgb565(255, 215, 0);
        candleColors[2] = rgb565(0, 128, 0);
        candleColors[3] = rgb565(255, 255, 255);
    }
    
    int litCount = 0;
    for (int i = 0; i < 4; i++) {
        int candleIdx = _candleOrder[i];
        bool isLit = (litCount < currentAdvent);
        if (isLit) litCount++;
        drawCandle(candlePositions[candleIdx], baseY, candleColors[candleIdx], isLit, candleIdx);
    }
}

void HolidayAnimationsModule::drawCandle(int x, int y, uint16_t color, bool isLit, int candleIndex) {
    // Dynamische Skalierung - Kerzen werden DEUTLICH größer bei Fullscreen
    int canvasH = _currentCanvas->height();
    float scale = canvasH / 66.0f;
    
    // Größere Kerzen die mehr Platz einnehmen
    int candleWidth = (int)(10 * scale);   // Breiter
    int candleHeight = (int)(28 * scale);  // Höher
    int candleTop = y - candleHeight;
    
    // Kerze zeichnen
    _currentCanvas->fillRect(x - candleWidth/2, candleTop, candleWidth, candleHeight, color);
    
    // Dunklerer Rand
    uint8_t r = ((color >> 11) & 0x1F) * 8;
    uint8_t g = ((color >> 5) & 0x3F) * 4;
    uint8_t b = (color & 0x1F) * 8;
    uint16_t darkColor = rgb565(r / 2, g / 2, b / 2);
    _currentCanvas->drawRect(x - candleWidth/2, candleTop, candleWidth, candleHeight, darkColor);
    
    // Docht
    int dochtHeight = (int)(5 * scale);
    _currentCanvas->drawLine(x, candleTop - 1, x, candleTop - dochtHeight, rgb565(60, 60, 60));
    
    if (isLit) {
        drawFlame(x, candleTop - dochtHeight - 1, _flamePhase + candleIndex * 5);
    }
}

void HolidayAnimationsModule::drawFlame(int x, int y, int phase) {
    // Dynamische Skalierung für größere Flammen
    int canvasH = _currentCanvas->height();
    float scale = canvasH / 66.0f;
    
    // Mehr Zufälligkeit durch Kombination von Phase und Position
    uint32_t randSeed = simpleRandom(x * 127 + phase * 31);
    int flicker = ((phase / 3) % 5) - 2 + ((randSeed % 3) - 1);
    int heightVar = (phase % 6) + ((randSeed / 3) % 2);
    int widthVar = ((phase / 2) % 3) + ((randSeed / 7) % 2);
    
    // Größere Flammen bei größerem Canvas
    int baseFlameHeight = (int)(12 * scale);
    int flameHeight = baseFlameHeight + heightVar;
    
    for (int i = 0; i < flameHeight; i++) {
        int baseWidth = (flameHeight - i) / 2 + widthVar;
        if (baseWidth < 1) baseWidth = 1;
        
        // Zufällige Farbvariation pro Zeile
        int colorPhase = (i + phase / 2 + ((randSeed / (i + 1)) % 3)) % 8;
        uint8_t r, g, b;
        
        // Zufällige Helligkeitsvariation
        int brightnessVar = ((randSeed / (i + 5)) % 30) - 15;
        
        if (colorPhase < 2) {
            r = 255; g = 255 + brightnessVar; b = 150 - i * 10;
        } else if (colorPhase < 4) {
            r = 255; g = 180 - i * 12 + brightnessVar; b = 0;
        } else if (colorPhase < 6) {
            r = 255; g = 120 - i * 8 + brightnessVar; b = 0;
        } else {
            r = 255; g = 220 - i * 15 + brightnessVar; b = 50;
        }
        
        if (g < 30) g = 30;
        if (g > 255) g = 255;
        if (b < 0) b = 0;
        
        // Zufälliger Flicker-Offset pro Zeile
        int flickerOffset = (i < flameHeight / 2) ? 0 : flicker + ((randSeed / (i + 3)) % 2);
        _currentCanvas->drawLine(x - baseWidth + flickerOffset, y - i, 
                       x + baseWidth + flickerOffset, y - i, rgb565(r, g, b));
    }
    
    int innerHeight = flameHeight / 2 + 1;
    for (int i = 0; i < innerHeight; i++) {
        int width = (innerHeight - i) / 2;
        if (width < 1 && i < innerHeight - 1) width = 1;
        
        uint8_t rI = 255;
        uint8_t gI = 255;
        uint8_t bI = 220 - i * 30;
        if (bI < 100) bI = 100;
        
        if (width >= 1) {
            _currentCanvas->drawLine(x - width, y - i - 1, x + width, y - i - 1, rgb565(rI, gI, bI));
        } else {
            _currentCanvas->drawPixel(x, y - i - 1, rgb565(rI, gI, bI));
        }
    }
}

void HolidayAnimationsModule::drawGreenery() {
    // Mehrere Grüntöne für natürlicheres Aussehen
    uint16_t greens[] = {
        rgb565(0, 70, 0),     // Sehr dunkelgrün
        rgb565(0, 90, 10),    // Dunkelgrün
        rgb565(0, 110, 20),   // Mittelgrün
        rgb565(20, 130, 30),  // Hellgrün
        rgb565(0, 80, 5),     // Tiefgrün
        rgb565(10, 100, 15)   // Waldgrün
    };
    int numGreens = sizeof(greens) / sizeof(greens[0]);
    
    // Dynamische Skalierung
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    float scale = canvasH / 66.0f;
    
    int bottomMargin = (int)(8 * scale);
    int baseY = canvasH - bottomMargin;
    int centerX = canvasW / 2;
    
    // Kranz-Ellipse fast so breit wie der Canvas
    int rx = (canvasW / 2) - 10;  // Horizontaler Radius - fast volle Breite
    int ry = (int)(14 * scale);   // Vertikaler Radius
    
    // Mehr und dichteres Tannengrün
    for (int angle = 0; angle < 360; angle += 8) {
        float rad = angle * 3.14159f / 180.0f;
        
        int bx = centerX + (int)(rx * cos(rad));
        int by = baseY + (int)(ry * sin(rad));
        
        // Mehr Nadeln pro Position bei größerem Scale
        int needleCount = (int)(8 * scale);
        if (needleCount < 6) needleCount = 6;
        
        for (int n = 0; n < needleCount; n++) {
            int nx = bx + (int)((n - needleCount/2) * 2);
            int nyOffset = ((angle + n * 17) % 5) - 2;
            int ny = by + nyOffset;
            
            if (ny >= 0 && ny < canvasH && nx >= 0 && nx < canvasW) {
                uint32_t seed = simpleRandom(angle * 13 + n * 7);
                uint16_t needleColor = greens[seed % numGreens];
                int lineOffset = ((angle + n * 23) % 4) - 2;
                int nadelLaenge = (int)(5 * scale) + (seed % 3);
                int endY = ny - nadelLaenge;
                if (endY >= 0) {
                    _currentCanvas->drawLine(nx, ny, nx + lineOffset, endY, needleColor);
                }
            }
        }
    }
    
    // Äste über die gesamte Breite verteilen
    int numBranches = (int)(10 * scale);
    for (int i = 0; i < numBranches; i++) {
        int branchX = 15 + (canvasW - 30) * i / numBranches;
        int branchDir = (i % 2 == 0) ? 1 : -1;
        drawBranch(branchX, baseY - (int)(5 * scale), branchDir);
    }
}

void HolidayAnimationsModule::drawBranch(int x, int y, int direction) {
    uint16_t greens[] = {
        rgb565(0, 90, 15),
        rgb565(0, 110, 25),
        rgb565(20, 130, 35),
        rgb565(0, 100, 20)
    };
    
    _currentCanvas->drawLine(x, y, x + direction * 7, y - 4, greens[0]);
    
    for (int i = 0; i < 5; i++) {
        int nx = x + direction * i;
        int ny = y - i / 2;
        
        if (ny >= 2 && ny < _currentCanvas->height()) {
            uint16_t color = greens[i % 4];
            _currentCanvas->drawLine(nx, ny, nx - direction * 2, ny - 3, color);
            _currentCanvas->drawLine(nx, ny, nx + direction * 2, ny - 3, color);
        }
    }
}

void HolidayAnimationsModule::drawBerries() {
    uint16_t berryColors[] = {
        rgb565(200, 0, 0),
        rgb565(255, 215, 0),
        rgb565(0, 100, 200),
        rgb565(200, 0, 200),
        rgb565(255, 140, 0),
        rgb565(0, 200, 100),
        rgb565(255, 50, 50),
        rgb565(100, 200, 255)
    };
    int numColors = 8;
    
    // Dynamische Skalierung
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    float scale = canvasH / 66.0f;
    
    int bottomMargin = (int)(8 * scale);
    int baseY = canvasH - bottomMargin;
    
    // Konfigurierbare Anzahl der Kugeln (4-20)
    int totalBerries = config ? config->adventWreathBerryCount : 12;
    if (totalBerries < 4) totalBerries = 4;
    if (totalBerries > 20) totalBerries = 20;
    
    // Bei Fullscreen mehr Kugeln
    if (scale > 1.2) totalBerries = (int)(totalBerries * 1.5);
    
    // Kerzenpositionen für Kollisionsvermeidung (müssen mit drawWreath übereinstimmen)
    int totalWidth = canvasW - 40;
    int candleSpacing = totalWidth / 4;
    int startX = 20 + candleSpacing / 2;
    int candleX[4] = {
        startX,
        startX + candleSpacing,
        startX + candleSpacing * 2,
        startX + candleSpacing * 3
    };
    int candleWidth = (int)(10 * scale);
    int safeDistance = candleWidth / 2 + (int)(5 * scale);  // Reduziert für mehr Platz
    
    // Hälfte Hintergrund-Kugeln, Hälfte Vordergrund-Kugeln
    int numBgBerries = totalBerries / 2;
    int numFgBerries = totalBerries - numBgBerries;
    
    // Hintergrund-Kugeln (kleiner, höher = 3D-Effekt) - OBERHALB der Kerzen
    for (int i = 0; i < numBgBerries; i++) {
        uint32_t seed = simpleRandom(i * 37 + 123);
        
        // Position: gleichmäßig verteilt, nicht zu nah an Rand
        int xSpacing = (canvasW - 30) / (numBgBerries + 1);
        int bx = 15 + xSpacing * (i + 1) + ((int)(seed % 10) - 5);
        
        // Y-Position: im Bereich des Tannengrüns (Mitte des Canvas)
        int by = baseY - (int)(5 * scale) - ((seed / 11) % (int)(6 * scale));
        int br = max(2, (int)(2 * scale));
        
        // Prüfe Kollision mit Kerzen (weniger strikt für Hintergrund)
        bool collision = false;
        for (int c = 0; c < 4; c++) {
            if (abs(bx - candleX[c]) < safeDistance - 3) {
                collision = true;
                break;
            }
        }
        
        // Stelle sicher, dass wir innerhalb des Canvas sind
        if (!collision && by >= 5 && by < canvasH - 5 && bx >= 5 && bx < canvasW - 5) {
            uint32_t colorSeed = simpleRandom(bx * 31 + by * 17 + i);
            uint16_t color = berryColors[colorSeed % numColors];
            // Gedimmt für Hintergrund-Effekt
            uint8_t r = ((color >> 11) & 0x1F) * 6;
            uint8_t g = ((color >> 5) & 0x3F) * 3;
            uint8_t b = (color & 0x1F) * 6;
            _currentCanvas->fillCircle(bx, by, br, rgb565(r, g, b));
        }
    }
    
    // Vordergrund-Kugeln (größer, auf dem Tannengrün)
    for (int i = 0; i < numFgBerries; i++) {
        uint32_t seed = simpleRandom(i * 47 + 456);
        
        // Position: gleichmäßig verteilt
        int xSpacing = (canvasW - 30) / (numFgBerries + 1);
        int bx = 15 + xSpacing * (i + 1) + ((int)(seed % 12) - 6);
        
        // Y-Position: auf dem Tannengrün (nicht unterhalb!)
        int by = baseY - (int)(2 * scale) + ((seed / 17) % (int)(4 * scale));
        // Begrenzen auf sichtbaren Bereich
        if (by > canvasH - 5) by = canvasH - 5;
        
        int br = (int)((3 + ((seed / 23) % 2)) * scale);
        if (br < 3) br = 3;
        if (br > 5) br = 5;
        
        // Prüfe Kollision mit Kerzen
        bool collision = false;
        for (int c = 0; c < 4; c++) {
            if (abs(bx - candleX[c]) < safeDistance + br) {
                collision = true;
                break;
            }
        }
        
        // Stelle sicher, dass wir innerhalb des Canvas sind
        if (!collision && by >= 5 && by < canvasH - 3 && bx >= 5 && bx < canvasW - 5) {
            uint32_t colorSeed = simpleRandom(bx * 47 + by * 23 + i);
            uint16_t color = berryColors[colorSeed % numColors];
            drawOrnament(bx, by, br, color);
        }
    }
}

unsigned long HolidayAnimationsModule::getDisplayDuration() {
    return _displayDurationMs;
}

bool HolidayAnimationsModule::isEnabled() {
    if (!config) return false;
    
    if (!config->adventWreathEnabled && !config->christmasTreeEnabled && !config->fireplaceEnabled) {
        return false;
    }
    
    return isHolidaySeason();
}

void HolidayAnimationsModule::resetPaging() {
    _isFinished = false;
}

void HolidayAnimationsModule::onActivate() {
    _isFinished = false;
    _isAdventViewActive = true;
    _adventViewStartTime = millis();
    _lastFlameUpdate = millis();
    _lastTreeLightUpdate = millis();
    _flamePhase = 0;
    _treeLightPhase = 0;
    _fireplaceFlamePhase = 0;
    const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
    Log.printf("[AdventWreath] Aktiviert: %s (UID=%lu)\n", modeName, _currentAdventUID);
}

void HolidayAnimationsModule::timeIsUp() {
    const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
    Log.printf("[AdventWreath] Zeit abgelaufen für %s (UID=%lu)\n", modeName, _currentAdventUID);
    _isAdventViewActive = false;
    _requestPending = false;
    _lastAdventDisplayTime = millis();
}

// ============== KAMIN ZEICHENFUNKTIONEN ==============

void HolidayAnimationsModule::drawFireplace() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    int centerX = canvasW / 2;
    
    // Dynamische Skalierung
    float scaleY = canvasH / 66.0f;
    float scaleX = canvasW / 192.0f;
    float scale = min(scaleX, scaleY);
    
    // Kaminfarbe aus Config oder Standard
    uint16_t brickColor = rgb565(139, 69, 19);  // Standard: Braun
    if (config && config->fireplaceBrickColor.length() > 0) {
        brickColor = hexToRgb565(config->fireplaceBrickColor.c_str());
    }
    
    // Dunklere Version für Schattierung
    uint8_t br = ((brickColor >> 11) & 0x1F) * 5;
    uint8_t bg = ((brickColor >> 5) & 0x3F) * 2;
    uint8_t bb = (brickColor & 0x1F) * 5;
    uint16_t brickDark = rgb565(br, bg, bb);
    uint16_t brickLight = rgb565(min(255, br + 60), min(255, bg + 40), min(255, bb + 40));
    
    // Kamin-Dimensionen (skaliert)
    int fireplaceWidth = (int)(100 * scale);
    int fireplaceHeight = (int)(50 * scale);
    int simsHeight = (int)(8 * scale);
    int simsOverhang = (int)(10 * scale);
    int openingWidth = (int)(60 * scale);
    int openingHeight = (int)(35 * scale);
    
    int baseY = canvasH - 2;
    int fireX = centerX - fireplaceWidth / 2;
    int fireY = baseY - fireplaceHeight;
    
    // Hintergrund wird bereits in draw() mit fireplaceBgColor gesetzt
    
    // ===== KAMINSIMS (oben) - schöner mit Profil =====
    int simsY = fireY - simsHeight;
    int simsWidth = fireplaceWidth + simsOverhang * 2;
    int simsX = centerX - simsWidth / 2;
    
    // Sims-Profil: Unterteil (dicker)
    int simsLower = (int)(simsHeight * 0.6);
    int simsUpper = simsHeight - simsLower;
    
    // Obere schmale Leiste
    _currentCanvas->fillRect(simsX + 2, simsY, simsWidth - 4, simsUpper, brickLight);
    // Untere breitere Basis
    _currentCanvas->fillRect(simsX, simsY + simsUpper, simsWidth, simsLower, brickLight);
    
    // Schatten und Kanten für 3D-Effekt
    _currentCanvas->drawLine(simsX, simsY + simsUpper, simsX + simsWidth, simsY + simsUpper, brickDark);  // Stufe
    _currentCanvas->drawLine(simsX, simsY + simsHeight - 1, simsX + simsWidth, simsY + simsHeight - 1, brickDark);  // Unterkante
    _currentCanvas->drawLine(simsX + 2, simsY, simsX + simsWidth - 2, simsY, rgb565(min(255, br + 100), min(255, bg + 80), min(255, bb + 80)));  // Oberkante hell
    
    // Dekrative Linie auf dem Sims
    uint16_t decoColor = rgb565(min(255, br + 40), min(255, bg + 30), min(255, bb + 30));
    _currentCanvas->drawLine(simsX + 4, simsY + simsUpper + simsLower/2, simsX + simsWidth - 4, simsY + simsUpper + simsLower/2, decoColor);
    
    // Kamin-Rahmen (links)
    int frameWidth = (fireplaceWidth - openingWidth) / 2;
    _currentCanvas->fillRect(fireX, fireY, frameWidth, fireplaceHeight, brickColor);
    // Ziegel-Muster links
    for (int row = 0; row < fireplaceHeight / 6; row++) {
        int y = fireY + row * 6;
        int offset = (row % 2) * 4;
        for (int col = 0; col < frameWidth / 8 + 1; col++) {
            int x = fireX + col * 8 + offset;
            if (x < fireX + frameWidth) {
                _currentCanvas->drawLine(x, y, x, y + 5, brickDark);
            }
        }
        _currentCanvas->drawLine(fireX, y, fireX + frameWidth, y, brickDark);
    }
    
    // Kamin-Rahmen (rechts)
    int rightFrameX = centerX + openingWidth / 2;
    _currentCanvas->fillRect(rightFrameX, fireY, frameWidth, fireplaceHeight, brickColor);
    // Ziegel-Muster rechts
    for (int row = 0; row < fireplaceHeight / 6; row++) {
        int y = fireY + row * 6;
        int offset = (row % 2) * 4;
        for (int col = 0; col < frameWidth / 8 + 1; col++) {
            int x = rightFrameX + col * 8 + offset;
            if (x < rightFrameX + frameWidth) {
                _currentCanvas->drawLine(x, y, x, y + 5, brickDark);
            }
        }
        _currentCanvas->drawLine(rightFrameX, y, rightFrameX + frameWidth, y, brickDark);
    }
    
    // Kamin-Öffnung (SCHWARZ)
    int openingX = centerX - openingWidth / 2;
    int openingY = baseY - openingHeight;
    _currentCanvas->fillRect(openingX, openingY, openingWidth, openingHeight, rgb565(0, 0, 0));
    
    // Flackernder Feuerschein in der Öffnung
    uint16_t glowColors[] = {
        rgb565(50, 20, 5),
        rgb565(60, 25, 8),
        rgb565(45, 18, 3),
        rgb565(55, 22, 6)
    };
    int glowIdx = _fireplaceFlamePhase % 4;
    // Sanfter Schein an den Seiten der Öffnung
    for (int i = 0; i < 5; i++) {
        int glowX = openingX + 2 + i;
        _currentCanvas->drawLine(glowX, openingY + 5, glowX, baseY - 5, glowColors[(glowIdx + i) % 4]);
        glowX = openingX + openingWidth - 3 - i;
        _currentCanvas->drawLine(glowX, openingY + 5, glowX, baseY - 5, glowColors[(glowIdx + i + 2) % 4]);
    }
    
    // Bogen über der Öffnung
    uint16_t archColor = brickDark;
    for (int i = 0; i < openingWidth; i++) {
        float angle = 3.14159f * i / openingWidth;
        int archY = openingY - (int)(sin(angle) * 8 * scale);
        _currentCanvas->drawPixel(openingX + i, archY, archColor);
        _currentCanvas->drawPixel(openingX + i, archY + 1, brickColor);
    }
    
    // ===== HOLZSCHEITE - ZUERST zeichnen =====
    uint16_t woodOuter = rgb565(101, 67, 33);   // Rinde außen
    uint16_t woodInner = rgb565(180, 140, 90);  // Holz innen (heller)
    uint16_t woodDark = rgb565(60, 40, 20);     // Schatten
    uint16_t woodRing = rgb565(140, 100, 60);   // Jahresringe
    
    int logY = baseY - 3;
    int logRadius = (int)(4 * scale);
    int logLength = (int)(28 * scale);
    
    // Linkes Scheit (schräg liegend)
    int log1X = centerX - logLength/2 - 3;
    int log1Y = logY - logRadius;
    
    // Körper des Scheits (zylindrisch)
    for (int i = 0; i < logLength; i++) {
        int x = log1X + i;
        // Oben heller, unten dunkler für Rundung
        _currentCanvas->drawLine(x, log1Y - logRadius + 1, x, log1Y + logRadius - 1, woodOuter);
        _currentCanvas->drawPixel(x, log1Y - logRadius, woodDark);  // Oberkante dunkel (Schatten)
        _currentCanvas->drawPixel(x, log1Y, woodRing);  // Mitte heller
    }
    
    // Schnittfläche links (Kreis/Oval)
    _currentCanvas->fillCircle(log1X, log1Y, logRadius, woodInner);
    _currentCanvas->drawCircle(log1X, log1Y, logRadius, woodOuter);
    // Jahresringe
    if (logRadius > 2) {
        _currentCanvas->drawCircle(log1X, log1Y, logRadius - 2, woodRing);
    }
    _currentCanvas->drawPixel(log1X, log1Y, woodDark);  // Mittelpunkt
    
    // Rechtes Scheit
    int log2X = centerX + 3;
    int log2Y = logY - logRadius - 1;
    
    for (int i = 0; i < logLength; i++) {
        int x = log2X + i;
        _currentCanvas->drawLine(x, log2Y - logRadius + 1, x, log2Y + logRadius - 1, woodOuter);
        _currentCanvas->drawPixel(x, log2Y - logRadius, woodDark);
        _currentCanvas->drawPixel(x, log2Y, woodRing);
    }
    
    // Schnittfläche rechts
    _currentCanvas->fillCircle(log2X + logLength, log2Y, logRadius, woodInner);
    _currentCanvas->drawCircle(log2X + logLength, log2Y, logRadius, woodOuter);
    if (logRadius > 2) {
        _currentCanvas->drawCircle(log2X + logLength, log2Y, logRadius - 2, woodRing);
    }
    _currentCanvas->drawPixel(log2X + logLength, log2Y, woodDark);
    
    // Kleines drittes Scheit oben drauf
    int log3X = centerX - (int)(8 * scale);
    int log3Y = logY - logRadius * 3;
    int log3Radius = (int)(3 * scale);
    int log3Length = (int)(20 * scale);
    
    for (int i = 0; i < log3Length; i++) {
        int x = log3X + i;
        _currentCanvas->drawLine(x, log3Y - log3Radius + 1, x, log3Y + log3Radius - 1, woodOuter);
        _currentCanvas->drawPixel(x, log3Y, woodRing);
    }
    _currentCanvas->fillCircle(log3X, log3Y, log3Radius, woodInner);
    _currentCanvas->drawCircle(log3X, log3Y, log3Radius, woodOuter);
    
    // ===== FEUER - NACH den Holzscheiten zeichnen =====
    drawFireplaceFlames(centerX, baseY - 2, openingWidth - 10, openingHeight - 5);
    
    // Strümpfe am Kaminsims
    drawStockings(simsY, simsWidth, centerX);
    
    // Dekorative Gegenstände auf dem Kaminsims (statt Kerzen)
    drawMantleDecorations(simsY, simsWidth, centerX, scale);
}

void HolidayAnimationsModule::drawFireplaceFlames(int x, int y, int width, int height) {
    // Feuerfarben basierend auf Konfiguration
    int flameColorMode = config ? config->fireplaceFlameColor : 0;
    
    uint16_t flameColors[5];
    uint16_t coreColor;
    switch (flameColorMode) {
        case 1:  // Blau
            flameColors[0] = rgb565(200, 230, 255);  // Kern (hell)
            flameColors[1] = rgb565(100, 180, 255);
            flameColors[2] = rgb565(50, 120, 255);
            flameColors[3] = rgb565(30, 80, 200);
            flameColors[4] = rgb565(20, 50, 150);   // Außen (dunkel)
            coreColor = rgb565(220, 240, 255);
            break;
        case 2:  // Grün
            flameColors[0] = rgb565(200, 255, 200);
            flameColors[1] = rgb565(100, 255, 100);
            flameColors[2] = rgb565(50, 200, 50);
            flameColors[3] = rgb565(30, 150, 30);
            flameColors[4] = rgb565(20, 100, 20);
            coreColor = rgb565(220, 255, 220);
            break;
        case 3:  // Violett
            flameColors[0] = rgb565(255, 200, 255);
            flameColors[1] = rgb565(220, 130, 255);
            flameColors[2] = rgb565(180, 80, 220);
            flameColors[3] = rgb565(140, 50, 180);
            flameColors[4] = rgb565(100, 30, 140);
            coreColor = rgb565(255, 220, 255);
            break;
        default:  // Klassisch Orange/Gelb
            flameColors[0] = rgb565(255, 255, 180);  // Kern (hellgelb)
            flameColors[1] = rgb565(255, 220, 80);   // Gelb
            flameColors[2] = rgb565(255, 160, 30);   // Orange
            flameColors[3] = rgb565(255, 100, 10);   // Rotorange
            flameColors[4] = rgb565(200, 50, 0);     // Dunkelrot
            coreColor = rgb565(255, 255, 220);
            break;
    }
    
    // Mehrere spitze Flammen zeichnen
    int numFlames = 7;
    for (int f = 0; f < numFlames; f++) {
        uint32_t seed = simpleRandom(f * 37 + _fireplaceFlamePhase * 3);
        
        // Position und Größe der Flamme
        int spacing = width / (numFlames + 1);
        int flameX = x - width/2 + spacing * (f + 1) + (int)((seed % 8) - 4);
        int baseFlameHeight = height * 2 / 3 + (seed % (height / 3));
        int flameWidth = 3 + (seed % 4);
        
        // Flamme von unten nach oben zeichnen - SPITZ ZULAUFEND
        for (int i = 0; i < baseFlameHeight; i++) {
            float progress = (float)i / baseFlameHeight;  // 0 = unten, 1 = oben
            
            // Breite nimmt nach oben stark ab (spitz)
            float widthFactor = 1.0f - progress * progress;  // Quadratisch abnehmen
            int currentWidth = (int)(flameWidth * widthFactor);
            if (currentWidth < 1 && i < baseFlameHeight - 2) currentWidth = 1;
            
            // Farbe basierend auf Position (innen heller, außen dunkler, oben dunkler)
            int colorIdx = (int)(progress * 4);
            if (colorIdx > 4) colorIdx = 4;
            uint16_t color = flameColors[colorIdx];
            
            // Flicker-Effekt (mehr oben)
            int flickerAmplitude = (int)(progress * 4);
            int flickerX = ((seed + i * 3 + _fireplaceFlamePhase) % (flickerAmplitude * 2 + 1)) - flickerAmplitude;
            
            if (currentWidth >= 1) {
                for (int dx = -currentWidth; dx <= currentWidth; dx++) {
                    // Innerer Bereich heller
                    float distFromCenter = abs(dx) / (float)(currentWidth + 1);
                    int innerColorIdx = colorIdx - (int)((1.0f - distFromCenter) * 2);
                    if (innerColorIdx < 0) innerColorIdx = 0;
                    uint16_t pixelColor = flameColors[innerColorIdx];
                    
                    int px = flameX + dx + flickerX;
                    int py = y - i;
                    if (px >= x - width/2 && px < x + width/2 && py >= 0 && py < _currentCanvas->height()) {
                        _currentCanvas->drawPixel(px, py, pixelColor);
                    }
                }
            } else if (currentWidth == 0) {
                // Spitze der Flamme
                int px = flameX + flickerX;
                int py = y - i;
                if (px >= x - width/2 && px < x + width/2 && py >= 0 && py < _currentCanvas->height()) {
                    _currentCanvas->drawPixel(px, py, flameColors[min(4, colorIdx + 1)]);
                }
            }
        }
        
        // Heller Kern am Fuß der Flamme
        for (int dy = 0; dy < 3; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int px = flameX + dx;
                int py = y - dy;
                if (px >= x - width/2 && px < x + width/2 && py >= 0) {
                    _currentCanvas->drawPixel(px, py, coreColor);
                }
            }
        }
    }
    
    // Funken
    for (int i = 0; i < 8; i++) {
        uint32_t seed = simpleRandom(i * 17 + _fireplaceFlamePhase * 7);
        if (seed % 4 == 0) {
            int sparkX = x - width/2 + (seed % width);
            int sparkY = y - height/2 - (seed % (height/2));
            if (sparkY >= 0 && sparkX >= x - width/2 && sparkX < x + width/2) {
                _currentCanvas->drawPixel(sparkX, sparkY, flameColors[seed % 2]);
            }
        }
    }
    
    // Glut am Boden
    uint16_t emberColors[] = {
        rgb565(255, 120, 20),
        rgb565(255, 80, 10),
        rgb565(220, 50, 0),
        rgb565(180, 30, 0)
    };
    
    for (int i = 0; i < width - 4; i++) {
        uint32_t seed = simpleRandom(i * 13 + _fireplaceFlamePhase / 2);
        if (seed % 2 == 0) {
            int emberY = y + 1 + (seed % 2);
            int colorIdx = seed % 4;
            int px = x - width/2 + 2 + i;
            if (emberY < _currentCanvas->height()) {
                _currentCanvas->drawPixel(px, emberY, emberColors[colorIdx]);
            }
        }
    }
}

void HolidayAnimationsModule::drawStockings(int simsY, int simsWidth, int centerX) {
    int stockingCount = config ? config->fireplaceStockingCount : 3;
    if (stockingCount < 0) stockingCount = 0;
    if (stockingCount > 5) stockingCount = 5;
    
    if (stockingCount == 0) return;
    
    uint16_t stockingColors[] = {
        rgb565(200, 0, 0),      // Rot
        rgb565(0, 150, 0),      // Grün
        rgb565(255, 255, 255),  // Weiß
        rgb565(255, 215, 0),    // Gold
        rgb565(0, 100, 200)     // Blau
    };
    
    int spacing = simsWidth / (stockingCount + 1);
    int stockingH = 18;
    int stockingW = 8;
    
    for (int i = 0; i < stockingCount; i++) {
        int sx = centerX - simsWidth/2 + spacing * (i + 1) - stockingW/2;
        int sy = simsY + 2;
        
        uint16_t color = stockingColors[i % 5];
        
        // Strumpf-Körper
        _currentCanvas->fillRect(sx, sy, stockingW, stockingH - 5, color);
        
        // Strumpf-Fuß (nach rechts)
        _currentCanvas->fillRect(sx, sy + stockingH - 5, stockingW + 4, 5, color);
        
        // Weißer Rand oben
        _currentCanvas->fillRect(sx - 1, sy, stockingW + 2, 3, rgb565(255, 255, 255));
    }
}

void HolidayAnimationsModule::drawMantleDecorations(int simsY, int simsWidth, int centerX, float scale) {
    // Dekorative Gegenstände statt Kerzen
    int decoCount = config ? config->fireplaceCandleCount : 2;  // Nutzt gleiche Config
    if (decoCount < 0) decoCount = 0;
    if (decoCount > 3) decoCount = 3;
    
    if (decoCount == 0) return;
    
    // Positionen für 1, 2 oder 3 Objekte
    int positions[3];
    if (decoCount == 1) {
        positions[0] = centerX;
    } else if (decoCount == 2) {
        positions[0] = centerX - simsWidth/3;
        positions[1] = centerX + simsWidth/3;
    } else {
        positions[0] = centerX - simsWidth/3;
        positions[1] = centerX;
        positions[2] = centerX + simsWidth/3;
    }
    
    // Verschiedene Dekorationen
    for (int i = 0; i < decoCount; i++) {
        int cx = positions[i];
        int cy = simsY - 2;
        int decoType = i % 3;
        
        if (decoType == 0) {
            // Blumenvase mit Blumen
            uint16_t vaseColor = rgb565(80, 60, 40);
            uint16_t flowerColors[] = {rgb565(255, 100, 100), rgb565(255, 200, 100), rgb565(255, 150, 200)};
            
            // Vase
            int vaseH = (int)(8 * scale);
            int vaseW = (int)(4 * scale);
            _currentCanvas->fillRect(cx - vaseW/2, cy - vaseH, vaseW, vaseH, vaseColor);
            _currentCanvas->drawRect(cx - vaseW/2, cy - vaseH, vaseW, vaseH, rgb565(50, 40, 30));
            
            // Blumen
            for (int f = 0; f < 3; f++) {
                int fx = cx + (f - 1) * 2;
                int fy = cy - vaseH - 3 - f;
                _currentCanvas->fillCircle(fx, fy, 2, flowerColors[f % 3]);
                _currentCanvas->drawLine(fx, fy + 2, fx, cy - vaseH + 1, rgb565(50, 100, 50));
            }
        } else if (decoType == 1) {
            // Schneekugel
            uint16_t baseColor = rgb565(60, 60, 60);
            uint16_t glassColor = rgb565(180, 200, 220);
            
            int globeR = (int)(5 * scale);
            // Basis
            _currentCanvas->fillRect(cx - globeR, cy - 3, globeR * 2, 3, baseColor);
            // Glasmugel
            _currentCanvas->fillCircle(cx, cy - 3 - globeR, globeR, glassColor);
            // Kleiner Tannenbaum drin
            _currentCanvas->fillTriangle(cx, cy - 3 - globeR - 3, cx - 2, cy - 3 - 2, cx + 2, cy - 3 - 2, rgb565(0, 100, 50));
            // Schneeflocken
            uint32_t seed = simpleRandom(_fireplaceFlamePhase + i * 17);
            for (int s = 0; s < 3; s++) {
                int sx = cx - globeR/2 + (seed % globeR);
                int sy = cy - 3 - globeR/2 - (seed / 7 % globeR);
                _currentCanvas->drawPixel(sx, sy, rgb565(255, 255, 255));
                seed = simpleRandom(seed);
            }
        } else {
            // Bilderrahmen
            uint16_t frameColor = rgb565(139, 90, 43);
            uint16_t pictureColor = rgb565(200, 180, 150);
            
            int frameW = (int)(8 * scale);
            int frameH = (int)(10 * scale);
            _currentCanvas->fillRect(cx - frameW/2, cy - frameH, frameW, frameH, frameColor);
            _currentCanvas->fillRect(cx - frameW/2 + 1, cy - frameH + 1, frameW - 2, frameH - 2, pictureColor);
            // Kleines Motiv (Haus)
            _currentCanvas->fillRect(cx - 2, cy - frameH + 4, 4, 4, rgb565(180, 100, 80));
            _currentCanvas->fillTriangle(cx - 3, cy - frameH + 4, cx, cy - frameH + 1, cx + 3, cy - frameH + 4, rgb565(150, 80, 60));
        }
    }
}
