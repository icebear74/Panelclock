#include "AnimationsModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include "PsramUtils.hpp"
#include "TimeUtilities.hpp"
#include <time.h>

// Hilfsfunktion für RGB565
uint16_t AnimationsModule::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Hilfsfunktion um Hex-Farbe zu RGB565 zu konvertieren
uint16_t AnimationsModule::hexToRgb565(const char* hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        return rgb565(255, 255, 255);
    }
    long r = strtol(PsramString(hex + 1, 2).c_str(), NULL, 16);
    long g = strtol(PsramString(hex + 3, 2).c_str(), NULL, 16);
    long b = strtol(PsramString(hex + 5, 2).c_str(), NULL, 16);
    return rgb565(r, g, b);
}

// Einfacher Pseudo-Zufallsgenerator
uint32_t AnimationsModule::simpleRandom(uint32_t seed) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

AnimationsModule::AnimationsModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, 
                                       GeneralTimeConverter& timeConverter, DeviceConfig* config)
    : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), config(config) {
}

AnimationsModule::~AnimationsModule() {
}

void AnimationsModule::begin() {
    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);
    _lastCheckedDay = tm_now.tm_mday;
    setConfig();
    shuffleCandleOrder();
    Log.println("[AnimationsModule] Modul initialisiert");
}

void AnimationsModule::setConfig() {
    if (config) {
        _displayDurationMs = config->adventWreathDisplaySec * 1000UL;
        _repeatIntervalMs = config->adventWreathRepeatMin * 60UL * 1000UL;
        _flameAnimationMs = config->adventWreathFlameSpeedMs;
    }
}

void AnimationsModule::onUpdate(std::function<void()> callback) {
    _updateCallback = callback;
}

void AnimationsModule::shuffleCandleOrder() {
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

bool AnimationsModule::isAdventSeason() {
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

bool AnimationsModule::isChristmasSeason() {
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

bool AnimationsModule::isHolidaySeason() {
    return isAdventSeason() || isChristmasSeason() || isFireplaceSeason();
}

bool AnimationsModule::isFireplaceSeason() {
    // Prüfe nur ob das Kamin-Feature aktiviert ist
    // Die Nachtmodus-Prüfung erfolgt später bei der Anzeige-Entscheidung
    if (!config || !config->fireplaceEnabled) return false;
    
    // Kamin ist immer "in Season" wenn aktiviert
    // (unabhängig von der Tageszeit)
    return true;
}

ChristmasDisplayMode AnimationsModule::getCurrentDisplayMode() {
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

int AnimationsModule::calculateCurrentAdvent() {
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

time_t AnimationsModule::calculateFourthAdvent(int year) {
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

void AnimationsModule::periodicTick() {
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
            Log.println("[AnimationsModule] Keine Weihnachtszeit mehr");
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
            
            // Prüfe Nachtmodus für Kamin
            if (fireplaceActive && config->fireplaceNightModeOnly && !TimeUtilities::isNightTime()) {
                fireplaceActive = false;
            }
            
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
            // Prüfe Nachtmodus für Kamin
            if (config->fireplaceNightModeOnly && !TimeUtilities::isNightTime()) {
                _showFireplace = false;  // Nicht anzeigen wenn Nachtmodus aktiv und es ist Tag
            } else {
                _showFireplace = true;
            }
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
            Log.printf("[AnimationsModule] %s %s angefordert (UID=%lu, Counter=%d)\n", 
                       modeName,
                       config->adventWreathInterrupt ? "Interrupt" : "PlayNext",
                       _currentAdventUID, _displayCounter);
            _displayCounter++;  // Nur erhöhen wenn Request erfolgreich
        } else {
            Log.println("[AnimationsModule] Request abgelehnt!");
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

void AnimationsModule::tick() {
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
    
    // LED-Rahmen-Animation
    unsigned long ledBorderSpeed = config ? (unsigned long)config->ledBorderSpeedMs : 100;
    if (now - _lastLedBorderUpdate > ledBorderSpeed) {
        _lastLedBorderUpdate = now;
        _ledBorderPhase = (_ledBorderPhase + 1) % 4;  // 4 Phasen
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

void AnimationsModule::logicTick() {
}

bool AnimationsModule::wantsFullscreen() const {
    // Always use fullscreen mode
    return _fullscreenCanvas != nullptr;
}

void AnimationsModule::draw() {
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
        // Regeneriere Kugeln/Lichter wenn der Baum neu angezeigt wird
        unsigned long now = millis();
        if (now - _lastTreeDisplay > 1000) {  // Mindestens 1 Sekunde seit letzter Anzeige
            _treeOrnamentsNeedRegeneration = true;
            _lastTreeDisplay = now;
        }
        
        drawChristmasTree();
        drawSnowflakes();
        
        // LED-Lauflicht-Rahmen
        drawLedBorder();
        
        if (config && config->showNewYearCountdown) {
            drawNewYearCountdown();
        }
    } else {
        // Adventskranz ohne Beschriftung - mehr Platz für die Grafik
        drawGreenery();
        drawWreath();
        drawBerries();
        
        // LED-Lauflicht-Rahmen
        drawLedBorder();
        
        if (config && config->showNewYearCountdown) {
            drawNewYearCountdown();
        }
    }
}

void AnimationsModule::drawChristmasTree() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    int centerX = canvasW / 2;
    
    // Dynamische Skalierung basierend auf Canvas-Höhe
    // Nutze scaleY um den gesamten verfügbaren Platz in der Höhe zu nutzen
    float scale = canvasH / 66.0f;  // 66 ist die Referenz-Höhe
    
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

void AnimationsModule::drawNaturalTree(int centerX, int baseY, float scale) {
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

void AnimationsModule::drawTreeOrnaments(int centerX, int baseY, float scale) {
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
    int numOrnaments = scale > 1.2 ? 16 : 12;  // Mehr Kugeln bei Fullscreen
    
    // Baumhöhe und Schichten (müssen mit drawNaturalTree übereinstimmen)
    int treeHeight = (int)(54 * scale);
    int layer1Height = (int)(18 * scale);
    int layer2Height = (int)(18 * scale);
    int layer1Width = (int)(28 * scale);
    int layer2Width = (int)(22 * scale);
    int layer3Width = (int)(16 * scale);
    
    // Verbesserte Verteilung - nutzt den gesamten Baum besser aus
    // Mehr unten, aber auch oben sollten einige sein
    for (int i = 0; i < numOrnaments; i++) {
        uint32_t seed = simpleRandom(i * 157 + 789);
        
        // Y-Position: Bessere Verteilung mit weniger extremer Konzentration unten
        // Mische linear (0.4) und quadratisch (0.6) für ausgewogenere Verteilung
        // Linear verteilt gleichmäßig, quadratisch konzentriert nach unten
        float t = (float)i / numOrnaments;  // 0.0 bis 1.0
        const float LINEAR_WEIGHT = 0.4f;    // 40% gleichmäßige Verteilung
        const float QUADRATIC_WEIGHT = 0.6f; // 60% Konzentration nach unten
        float yFraction = LINEAR_WEIGHT * t + QUADRATIC_WEIGHT * (t * t);
        int yOffset = 3 + (int)(yFraction * (treeHeight - 10));
        yOffset += (seed % 8) - 4;  // Mehr Variation für natürlicheres Aussehen
        if (yOffset < 4) yOffset = 4;
        if (yOffset > treeHeight - 6) yOffset = treeHeight - 6;
        
        // Berechne maximale Breite an dieser Y-Position (nutze mehr vom verfügbaren Raum)
        int maxWidth;
        if (yOffset < layer1Height) {
            float progress = (float)yOffset / layer1Height;
            // Erhöhe von 0.7f auf 0.85f um mehr vom Rand zu nutzen
            maxWidth = (int)(layer1Width * (1.0f - progress * 0.5f) * 0.85f);
        } else if (yOffset < layer1Height + layer2Height - (int)(4 * scale)) {
            float progress = (float)(yOffset - layer1Height + (int)(4 * scale)) / layer2Height;
            maxWidth = (int)(layer2Width * (1.0f - progress * 0.5f) * 0.85f);
        } else {
            float progress = (float)(yOffset - layer1Height - layer2Height + (int)(14 * scale)) / (layer3Width + 4);
            maxWidth = (int)(layer3Width * (1.0f - progress * 0.6f) * 0.8f);
        }
        
        if (maxWidth < 2) maxWidth = 2;
        
        // X-Position: Bessere Nutzung der Ränder
        int xRange = maxWidth * 2;  // Voller Bereich nutzen
        int ox;
        // Verwende mehr Variation und nutze Ränder besser
        int xPos = seed % xRange;
        if (i % 2 == 0) {
            // Linke Seite - gehe näher an den Rand
            ox = centerX - maxWidth + (xPos / 2);
        } else {
            // Rechte Seite - gehe näher an den Rand
            ox = centerX - maxWidth + xRange/2 + (xPos / 2);
        }
        
        int oy = baseY - yOffset;
        int radius = 2 + (seed % 2);
        if (scale > 1.2) radius = 2 + (seed % 3);
        
        uint16_t color = ornamentColors[seed % numColors];
        drawOrnament(ox, oy, radius, color);
    }
}

void AnimationsModule::drawTreeLights() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    int centerX = canvasW / 2;
    
    // Dynamische Skalierung - GLEICH wie im Baum
    float scale = canvasH / 66.0f;
    
    // baseY MUSS exakt mit drawChristmasTree übereinstimmen
    int baseY = canvasH - 4;  // Basis am unteren Rand
    int trunkHeight = (int)(10 * scale);
    int treeBaseY = baseY - trunkHeight + 2;  // Basis des Baumes (Grüns)
    
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
    
    // Baumschichten (müssen mit drawNaturalTree übereinstimmen)
    int layer1Height = (int)(18 * scale);
    int layer2Height = (int)(18 * scale);
    int layer3Height = (int)(18 * scale);
    int layer1Width = (int)(28 * scale);
    int layer2Width = (int)(22 * scale);
    int layer3Width = (int)(16 * scale);
    int treeHeight = layer1Height + layer2Height - (int)(4 * scale) + layer3Height - (int)(10 * scale);
    
    // Lichter über den Baum verteilt - bessere Verteilung über die gesamte Baumfläche
    for (int i = 0; i < lightCount; i++) {
        // Berechne Position basierend auf Index
        uint32_t seed = simpleRandom(i * 67 + 321);
        
        // Y-Position: Verbesserte Verteilung - mehr unten, aber ganzer Baum wird genutzt
        // Mische linear (0.3) und quadratisch (0.7) für ausgewogenere Verteilung
        float t = (float)i / lightCount;  // 0.0 bis 1.0
        const float LINEAR_WEIGHT = 0.3f;    // 30% gleichmäßige Verteilung
        const float QUADRATIC_WEIGHT = 0.7f; // 70% Konzentration nach unten
        float yFraction = LINEAR_WEIGHT * t + QUADRATIC_WEIGHT * (t * t);
        int ySection = (int)(yFraction * treeHeight);
        int yOffset = ySection + (seed % 8) - 4;  // Mehr Variation
        if (yOffset < 2) yOffset = 2;
        if (yOffset > treeHeight - 4) yOffset = treeHeight - 4;
        
        int lightY = treeBaseY - yOffset;
        
        // X-Position abhängig von Y (nutze mehr vom verfügbaren Raum)
        int maxX;
        if (yOffset < layer1Height) {
            float progress = (float)yOffset / layer1Height;
            // Erhöhe von 0.65f auf 0.8f um mehr vom Rand zu nutzen
            maxX = (int)(layer1Width * (1.0f - progress * 0.5f) * 0.8f);
        } else if (yOffset < layer1Height + layer2Height - (int)(4 * scale)) {
            float progress = (float)(yOffset - layer1Height + (int)(4 * scale)) / layer2Height;
            maxX = (int)(layer2Width * (1.0f - progress * 0.5f) * 0.8f);
        } else {
            float progress = (float)(yOffset - layer1Height - layer2Height + (int)(14 * scale)) / layer3Height;
            maxX = (int)(layer3Width * (1.0f - progress * 0.7f) * 0.75f);
        }
        
        if (maxX < 2) maxX = 2;
        
        // Bessere X-Verteilung - nutzt die Ränder besser
        int lightX;
        seed = simpleRandom(seed + i);
        int xRange = maxX * 2;
        int xPos = seed % xRange;
        
        if (i % 2 == 0) {
            // Linke Seite - gehe näher an den Rand
            lightX = centerX - maxX + (xPos / 2);
        } else {
            // Rechte Seite
            lightX = centerX - maxX + xRange/2 + (xPos / 2);
        }
        
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

void AnimationsModule::drawGifts(int centerX, int baseY, float scale) {
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

void AnimationsModule::drawOrnament(int x, int y, int radius, uint16_t color) {
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

void AnimationsModule::drawSnowflakes() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Initialize snowflakes on first call
    if (!_snowflakesInitialized) {
        for (int i = 0; i < MAX_SNOWFLAKES; i++) {
            _snowflakes[i].x = (float)(rand() % canvasW);
            _snowflakes[i].y = (float)(rand() % canvasH);
            _snowflakes[i].speed = 0.5f + (float)(rand() % 15) / 10.0f;
            _snowflakes[i].size = 1 + (rand() % 2);
        }
        _snowflakesInitialized = true;
        _lastSnowflakeUpdate = millis();
    }
    
    // Update positions
    unsigned long now = millis();
    if (now - _lastSnowflakeUpdate > 50) {  // Update every 50ms
        for (int i = 0; i < MAX_SNOWFLAKES; i++) {
            _snowflakes[i].y += _snowflakes[i].speed;
            
            // Slight horizontal drift
            if ((rand() % 10) < 3) {
                _snowflakes[i].x += (rand() % 3) - 1;
            }
            
            // Reset at bottom
            if (_snowflakes[i].y > canvasH) {
                _snowflakes[i].y = 0;
                _snowflakes[i].x = (float)(rand() % canvasW);
                _snowflakes[i].speed = 0.5f + (float)(rand() % 15) / 10.0f;
            }
            
            // Wrap horizontally
            if (_snowflakes[i].x < 0) _snowflakes[i].x = canvasW - 1;
            if (_snowflakes[i].x >= canvasW) _snowflakes[i].x = 0;
        }
        _lastSnowflakeUpdate = now;
    }
    
    // Draw snowflakes
    uint16_t snowColor = rgb565(255, 255, 255);
    for (int i = 0; i < MAX_SNOWFLAKES; i++) {
        int x = (int)_snowflakes[i].x;
        int y = (int)_snowflakes[i].y;
        
        if (_snowflakes[i].size == 1) {
            _currentCanvas->drawPixel(x, y, snowColor);
        } else {
            // Larger snowflake - small cross pattern
            _currentCanvas->drawPixel(x, y, snowColor);
            if (x > 0) _currentCanvas->drawPixel(x-1, y, snowColor);
            if (x < canvasW-1) _currentCanvas->drawPixel(x+1, y, snowColor);
            if (y > 0) _currentCanvas->drawPixel(x, y-1, snowColor);
            if (y < canvasH-1) _currentCanvas->drawPixel(x, y+1, snowColor);
        }
    }
}

void AnimationsModule::drawNewYearCountdown() {
    // Get current time
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    // === AKTUELLE UHRZEIT LINKS ===
    char timeText[16];
    snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    
    u8g2.setFont(u8g2_font_7x13_tr);  // Same font as countdown
    u8g2.setForegroundColor(rgb565(100, 200, 255));  // Light blue color for time
    
    // Left-aligned with small margin
    u8g2.setCursor(2, 10);  // x=2 (left-aligned), y=10 (top)
    u8g2.print(timeText);
    
    // === COUNTDOWN RECHTS ===
    // Calculate New Year (midnight) in local time
    int currentYear = tm_now.tm_year + 1900;
    int targetYear = currentYear + 1;
    
    // If we're in the new year already, target next year
    if (tm_now.tm_mon == 0 && tm_now.tm_mday == 1) {
        // Already New Year, show countdown to next year
        targetYear++;
    }
    
    // Create target time: Jan 1, 00:00:00 of target year in local time
    struct tm tm_target = {0};
    tm_target.tm_year = targetYear - 1900;
    tm_target.tm_mon = 0;  // January (0-indexed)
    tm_target.tm_mday = 1;
    tm_target.tm_hour = 0;
    tm_target.tm_min = 0;
    tm_target.tm_sec = 0;
    tm_target.tm_isdst = -1;  // Let mktime determine DST
    
    time_t target_local = mktime(&tm_target);
    
    // Calculate difference
    time_t diff = target_local - local_now;
    if (diff < 0) diff = 0;
    
    int days = diff / (24 * 3600);
    int hours = (diff % (24 * 3600)) / 3600;
    int minutes = (diff % 3600) / 60;
    int seconds = diff % 60;
    
    // Draw countdown at top of screen - right-aligned
    char countdownText[40];
    snprintf(countdownText, sizeof(countdownText), "%dd %02d:%02d:%02d", days, hours, minutes, seconds);
    
    u8g2.setForegroundColor(rgb565(255, 215, 0));  // Gold color for countdown
    
    // Calculate text width for right alignment
    int textWidth = u8g2.getUTF8Width(countdownText);
    int canvasW = _currentCanvas->width();
    
    // Right-aligned with small margin (2 pixels from right edge)
    u8g2.setCursor(canvasW - textWidth - 2, 10);  // Right-aligned, y=10 (top)
    u8g2.print(countdownText);
}

void AnimationsModule::drawWreath() {
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

void AnimationsModule::drawCandle(int x, int y, uint16_t color, bool isLit, int candleIndex) {
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
        // Verwende verbesserte Flamme vom Kaminfeuer (ohne Funken)
        drawCandleFlame(x, candleTop - dochtHeight - 1, _flamePhase + candleIndex * 5);
    }
}

void AnimationsModule::drawFlame(int x, int y, int phase) {
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

void AnimationsModule::drawCandleFlame(int x, int y, int phase) {
    // Ähnlich wie Kaminfeuer, aber kleiner und ohne Funken
    // Ruhigere Flamme mit weniger X-Achsen-Bewegung für realistischeren Kerzeneffekt
    int canvasH = _currentCanvas->height();
    float scale = canvasH / 66.0f;
    
    // Kleinere Flammen als Kaminfeuer, basierend auf der Konfiguration
    int baseFlameHeight = (int)(14 * scale);  // Etwas höher als alte Flamme
    int flameWidth = (int)(6 * scale);
    
    // Klassische Orange/Gelb Farben für Kerzen
    uint16_t flameColors[6];
    flameColors[0] = rgb565(255, 255, 180);  // Kern (hellgelb)
    flameColors[1] = rgb565(255, 230, 100);  // Gelb
    flameColors[2] = rgb565(255, 180, 50);   // Gelb-Orange
    flameColors[3] = rgb565(255, 120, 20);   // Orange
    flameColors[4] = rgb565(220, 70, 0);     // Dunkelorange
    flameColors[5] = rgb565(150, 40, 0);     // Dunkelrot
    uint16_t coreColor = rgb565(255, 255, 220);
    
    // Flammenform (ohne Funkenflug)
    for (int fy = 0; fy < baseFlameHeight; fy++) {
        float yProgress = (float)fy / baseFlameHeight;
        
        // Breite nimmt nach oben ab
        float widthFactor = (1.0f - yProgress * yProgress) * 0.9f + 0.1f;
        int lineWidth = (int)(flameWidth / 2 * widthFactor);
        
        // Deutlich reduzierte Wellenbewegung für ruhigere Flamme
        // Nur sehr sanfte Y-abhängige Bewegung, kein starkes Flackern
        int waveOffset = (fy > baseFlameHeight / 2) ? ((phase / 4) % 2) - 1 : 0;
        
        for (int fx = -lineWidth; fx <= lineWidth; fx++) {
            uint32_t seed = simpleRandom(fx * 23 + fy * 47 + phase * 5);
            
            // Dichte
            float density = 1.0f - yProgress * 0.6f;
            if ((seed % 100) > density * 100) continue;
            
            // Sehr reduziertes Flackern - nur minimal für natürlichen Effekt
            int flickerX = (fy > baseFlameHeight * 0.7f) ? ((seed / 7) % 2) : 0;
            int px = x + fx + waveOffset + flickerX;
            int py = y - fy;
            
            // Farbe: innen heller, außen dunkler
            float distFromCenter = abs(fx) / (float)(lineWidth + 1);
            int baseColorIdx = (int)(yProgress * 4);
            int colorIdx = baseColorIdx + (int)(distFromCenter * 2);
            if (colorIdx > 5) colorIdx = 5;
            
            // Kern ist heller
            uint16_t pixelColor;
            if (distFromCenter < 0.3f && yProgress < 0.4f) {
                pixelColor = coreColor;
            } else {
                pixelColor = flameColors[colorIdx];
            }
            
            _currentCanvas->drawPixel(px, py, pixelColor);
        }
    }
}

void AnimationsModule::drawGreenery() {
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

void AnimationsModule::drawBranch(int x, int y, int direction) {
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

void AnimationsModule::drawBerries() {
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

unsigned long AnimationsModule::getDisplayDuration() {
    return _displayDurationMs;
}

bool AnimationsModule::isEnabled() {
    if (!config) return false;
    
    if (!config->adventWreathEnabled && !config->christmasTreeEnabled && !config->fireplaceEnabled) {
        return false;
    }
    
    return isHolidaySeason();
}

void AnimationsModule::resetPaging() {
    _isFinished = false;
}

void AnimationsModule::onActivate() {
    _isFinished = false;
    _isAdventViewActive = true;
    _adventViewStartTime = millis();
    _lastFlameUpdate = millis();
    _lastTreeLightUpdate = millis();
    _flamePhase = 0;
    _treeLightPhase = 0;
    _fireplaceFlamePhase = 0;
    const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
    Log.printf("[AnimationsModule] Aktiviert: %s (UID=%lu)\n", modeName, _currentAdventUID);
}

void AnimationsModule::timeIsUp() {
    const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
    Log.printf("[AnimationsModule] Zeit abgelaufen für %s (UID=%lu)\n", modeName, _currentAdventUID);
    _isAdventViewActive = false;
    _requestPending = false;
    _lastAdventDisplayTime = millis();
}

// ============== KAMIN ZEICHENFUNKTIONEN ==============

// Fireplace pixel array - 64x64 base design (will be scaled)
// This represents the fireplace structure without fire/decorations
// Format: Each value represents a color index in the palette
static const uint8_t FIREPLACE_BITMAP_64x64[64 * 64] PROGMEM = {
    // This would be a large array - instead we'll use procedural drawing
    // but structured to closely match the reference image
};

void AnimationsModule::drawFireplace() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Color palette matching reference image more closely
    uint16_t COL_WALL = rgb565(210, 210, 210);      // Light wall
    uint16_t COL_FLOOR = rgb565(160, 160, 160);     // Floor tiles
    uint16_t COL_FLOOR_GROUT = rgb565(120, 120, 120); // Grout lines
    uint16_t COL_MANTEL_WOOD = rgb565(139, 109, 78); // Mantel wood
    uint16_t COL_MANTEL_SHADOW = rgb565(100, 80, 60); // Mantel shadow
    uint16_t COL_STONE_LIGHT = rgb565(200, 200, 200); // Light stone
    uint16_t COL_STONE_MED = rgb565(160, 160, 160);   // Medium stone
    uint16_t COL_STONE_DARK = rgb565(100, 100, 100);  // Dark stone/grout
    uint16_t COL_FIREPLACE_INSIDE = rgb565(20, 20, 20); // Dark interior
    uint16_t COL_METAL_DARK = rgb565(50, 50, 50);    // Dark metal
    uint16_t COL_METAL_LIGHT = rgb565(120, 120, 120); // Light metal
    uint16_t COL_WOOD_DARK = rgb565(80, 60, 40);     // Dark wood/bark
    uint16_t COL_WOOD_LIGHT = rgb565(220, 190, 150); // Light wood interior
    
    // Override with config colors
    if (config && config->fireplaceBrickColor.length() > 0) {
        uint16_t customBrick = hexToRgb565(config->fireplaceBrickColor.c_str());
        COL_STONE_LIGHT = customBrick;
        // Derive darker shades
        uint8_t r = ((customBrick >> 11) & 0x1F) * 8;
        uint8_t g = ((customBrick >> 5) & 0x3F) * 4;
        uint8_t b = (customBrick & 0x1F) * 8;
        COL_STONE_MED = rgb565(max(0, r - 30), max(0, g - 30), max(0, b - 30));
        COL_STONE_DARK = rgb565(max(0, r - 60), max(0, g - 60), max(0, b - 60));
    }
    if (config && config->fireplaceBgColor.length() > 0) {
        COL_WALL = hexToRgb565(config->fireplaceBgColor.c_str());
    }
    
    // Scaling factors
    float scaleX = canvasW / 192.0f;
    float scaleY = canvasH / 96.0f;
    
    // === BACKGROUND ===
    _currentCanvas->fillScreen(COL_WALL);
    
    // === FLOOR (bottom ~12% of screen) ===
    int floorY = canvasH - (int)(12 * scaleY);
    _currentCanvas->fillRect(0, floorY, canvasW, canvasH - floorY, COL_FLOOR);
    
    // Floor tile pattern (horizontal lines every 4 pixels)
    for (int y = floorY; y < canvasH; y += max(3, (int)(4 * scaleY))) {
        _currentCanvas->drawLine(0, y, canvasW, y, COL_FLOOR_GROUT);
    }
    // Vertical lines for perspective
    for (int x = 15; x < canvasW; x += max(15, (int)(20 * scaleX))) {
        int dy = (int)(8 * scaleY);
        _currentCanvas->drawLine(x, floorY, x - dy, canvasH, COL_FLOOR_GROUT);
    }
    
    // === FIREPLACE STRUCTURE (center) ===
    int fp_x = (int)(66 * scaleX);
    int fp_y = (int)(20 * scaleY);
    int fp_w = (int)(60 * scaleX);
    int fp_h = (int)(40 * scaleY);
    
    // Main stone structure
    _currentCanvas->fillRect(fp_x, fp_y, fp_w, fp_h, COL_STONE_MED);
    
    // Draw detailed brick pattern
    int brickH = max(3, (int)(5 * scaleY));
    int brickW = max(5, (int)(8 * scaleX));
    for (int by = 0; by < fp_h; by += brickH) {
        int offset = ((by / brickH) % 2) * (brickW / 2);
        // Horizontal mortar line
        _currentCanvas->drawLine(fp_x, fp_y + by, fp_x + fp_w, fp_y + by, COL_STONE_DARK);
        // Vertical mortar lines (offset every other row)
        for (int bx = -brickW; bx < fp_w + brickW; bx += brickW) {
            int x = fp_x + bx + offset;
            if (x >= fp_x && x < fp_x + fp_w) {
                for (int dy = 0; dy < min(brickH, fp_h - by); dy++) {
                    _currentCanvas->drawPixel(x, fp_y + by + dy, COL_STONE_DARK);
                }
            }
        }
        // Add highlight to some bricks
        if ((by / brickH) % 3 == 0) {
            for (int bx = 0; bx < fp_w; bx += brickW) {
                int x = fp_x + bx + offset + 2;
                if (x < fp_x + fp_w - 2) {
                    _currentCanvas->drawPixel(x, fp_y + by + 2, COL_STONE_LIGHT);
                }
            }
        }
    }
    
    // === FIREPLACE OPENING ===
    int opening_margin = (int)(10 * scaleX);
    int opening_top = (int)(10 * scaleY);
    int opening_x = fp_x + opening_margin;
    int opening_y = fp_y + opening_top;
    int opening_w = fp_w - 2 * opening_margin;
    int opening_h = fp_h - opening_top;
    
    _currentCanvas->fillRect(opening_x, opening_y, opening_w, opening_h, COL_FIREPLACE_INSIDE);
    
    // Inner stone edge (darker)
    _currentCanvas->drawRect(opening_x, opening_y, opening_w, opening_h, COL_STONE_DARK);
    _currentCanvas->drawRect(opening_x + 1, opening_y + 1, opening_w - 2, opening_h - 2, rgb565(30, 30, 30));
    
    // === METAL GRATE ===
    int grate_y = opening_y + opening_h - (int)(4 * scaleY);
    int grate_spacing = max(4, (int)(6 * scaleX));
    _currentCanvas->drawLine(opening_x + 3, grate_y, opening_x + opening_w - 3, grate_y, COL_METAL_LIGHT);
    _currentCanvas->drawLine(opening_x + 3, grate_y + 1, opening_x + opening_w - 3, grate_y + 1, COL_METAL_DARK);
    // Vertical grate bars
    for (int gx = opening_x + grate_spacing; gx < opening_x + opening_w; gx += grate_spacing) {
        _currentCanvas->drawLine(gx, grate_y - 2, gx, grate_y + 1, COL_METAL_DARK);
    }
    
    // === MANTEL SHELF ===
    int mantel_y = fp_y - (int)(4 * scaleY);
    int mantel_h = (int)(4 * scaleY);
    int mantel_overhang = (int)(6 * scaleX);
    
    // Main shelf (light wood on top)
    _currentCanvas->fillRect(fp_x - mantel_overhang, mantel_y, fp_w + 2 * mantel_overhang, mantel_h, COL_MANTEL_WOOD);
    // Top highlight
    _currentCanvas->drawLine(fp_x - mantel_overhang, mantel_y, fp_x + fp_w + mantel_overhang, mantel_y, 
                            rgb565(min(255, ((COL_MANTEL_WOOD >> 11) & 0x1F) * 8 + 40), 
                                   min(255, ((COL_MANTEL_WOOD >> 5) & 0x3F) * 4 + 30),
                                   min(255, (COL_MANTEL_WOOD & 0x1F) * 8 + 30)));
    // Bottom shadow
    _currentCanvas->drawLine(fp_x - mantel_overhang, fp_y, fp_x + fp_w + mantel_overhang, fp_y, COL_MANTEL_SHADOW);
    _currentCanvas->drawLine(fp_x - mantel_overhang, fp_y - 1, fp_x + fp_w + mantel_overhang, fp_y - 1, COL_MANTEL_SHADOW);
    
    // === WOOD STORAGE (left) ===
    int wood_x = (int)(18 * scaleX);
    int wood_y = (int)(26 * scaleY);
    int wood_w = (int)(26 * scaleX);
    int wood_h = (int)(32 * scaleY);
    
    // Metal frame
    _currentCanvas->drawRect(wood_x, wood_y, wood_w, wood_h, COL_METAL_DARK);
    _currentCanvas->drawRect(wood_x + 1, wood_y + 1, wood_w - 2, wood_h - 2, COL_METAL_LIGHT);
    
    // Stacked logs
    int log_r = max(2, (int)(2.5 * scaleY));
    int log_spacing = max(4, (int)(5 * scaleY));
    for (int ly = wood_y + wood_h - 5; ly > wood_y + 3; ly -= log_spacing) {
        for (int lx = wood_x + 5; lx < wood_x + wood_w - 5; lx += log_spacing) {
            // Log with bark
            _currentCanvas->fillCircle(lx, ly, log_r, COL_WOOD_DARK);
            // Cut surface
            if (log_r > 1) {
                _currentCanvas->fillCircle(lx, ly, log_r - 1, COL_WOOD_LIGHT);
                _currentCanvas->drawPixel(lx, ly, COL_WOOD_DARK); // Center ring
            }
        }
    }
    
    // === TOOL STAND (right) ===
    int tool_x = (int)(155 * scaleX);
    int tool_y = (int)(24 * scaleY);
    int tool_h = (int)(32 * scaleY);
    
    // Main pole
    for (int i = 0; i < 2; i++) {
        _currentCanvas->drawLine(tool_x + i, tool_y, tool_x + i, tool_y + tool_h, COL_METAL_DARK);
    }
    
    // Base
    _currentCanvas->drawLine(tool_x - 4, tool_y + tool_h, tool_x + 5, tool_y + tool_h, COL_METAL_DARK);
    _currentCanvas->drawLine(tool_x - 4, tool_y + tool_h - 1, tool_x + 5, tool_y + tool_h - 1, COL_METAL_LIGHT);
    
    // Top hook bar
    _currentCanvas->drawLine(tool_x - 5, tool_y + 2, tool_x + 6, tool_y + 2, COL_METAL_DARK);
    _currentCanvas->drawLine(tool_x - 5, tool_y + 3, tool_x + 6, tool_y + 3, COL_METAL_LIGHT);
    
    // Tool 1: Poker (left)
    int poker_x = tool_x - 4;
    int poker_len = (int)(18 * scaleY);
    _currentCanvas->drawLine(poker_x, tool_y + 3, poker_x, tool_y + poker_len, COL_METAL_DARK);
    // Hook at bottom
    _currentCanvas->fillRect(poker_x - 1, tool_y + poker_len, 3, max(2, (int)(3 * scaleY)), COL_METAL_LIGHT);
    
    // Tool 2: Shovel (center-left)
    int shovel_x = tool_x - 2;
    _currentCanvas->drawLine(shovel_x, tool_y + 4, shovel_x, tool_y + poker_len - 2, COL_METAL_DARK);
    int shovel_blade_y = tool_y + poker_len - 2;
    _currentCanvas->fillRect(shovel_x - 2, shovel_blade_y, 4, max(2, (int)(4 * scaleY)), COL_METAL_LIGHT);
    _currentCanvas->drawRect(shovel_x - 2, shovel_blade_y, 4, max(2, (int)(4 * scaleY)), COL_METAL_DARK);
    
    // Tool 3: Brush (center-right)
    int brush_x = tool_x + 2;
    _currentCanvas->drawLine(brush_x, tool_y + 5, brush_x, tool_y + poker_len - 1, COL_METAL_DARK);
    int brush_y = tool_y + poker_len - 1;
    // Bristles
    _currentCanvas->drawLine(brush_x - 2, brush_y, brush_x + 2, brush_y, COL_METAL_DARK);
    _currentCanvas->drawLine(brush_x - 2, brush_y + 1, brush_x - 1, brush_y + 3, COL_METAL_DARK);
    _currentCanvas->drawLine(brush_x + 2, brush_y + 1, brush_x + 1, brush_y + 3, COL_METAL_DARK);
    
    // Tool 4: Tongs (right)
    int tongs_x = tool_x + 4;
    _currentCanvas->drawLine(tongs_x, tool_y + 6, tongs_x, tool_y + poker_len, COL_METAL_DARK);
    // Tongs grip
    _currentCanvas->drawLine(tongs_x - 2, tool_y + poker_len + 1, tongs_x, tool_y + poker_len + 3, COL_METAL_DARK);
    _currentCanvas->drawLine(tongs_x + 2, tool_y + poker_len + 1, tongs_x, tool_y + poker_len + 3, COL_METAL_DARK);
    
    // === FIRE ===
    drawFireplaceFlames(opening_x + opening_w / 2, opening_y + opening_h - 2, 
                       opening_w - 4, opening_h - 8);
    
    // === DECORATIONS ===
    drawStockings(mantel_y, fp_w + 2 * mantel_overhang, fp_x + fp_w / 2);
    drawMantleDecorations(mantel_y, fp_w + 2 * mantel_overhang, fp_x + fp_w / 2, scaleY);
}

void AnimationsModule::drawFireplaceFlames(int x, int y, int width, int height) {
    // Feuerfarben basierend auf Konfiguration
    int flameColorMode = config ? config->fireplaceFlameColor : 0;
    
    uint16_t flameColors[6];
    uint16_t coreColor;
    switch (flameColorMode) {
        case 1:  // Blau
            flameColors[0] = rgb565(200, 230, 255);  // Kern (hell)
            flameColors[1] = rgb565(100, 180, 255);
            flameColors[2] = rgb565(50, 120, 255);
            flameColors[3] = rgb565(30, 80, 200);
            flameColors[4] = rgb565(20, 50, 150);
            flameColors[5] = rgb565(10, 30, 100);
            coreColor = rgb565(220, 240, 255);
            break;
        case 2:  // Grün
            flameColors[0] = rgb565(200, 255, 200);
            flameColors[1] = rgb565(100, 255, 100);
            flameColors[2] = rgb565(50, 200, 50);
            flameColors[3] = rgb565(30, 150, 30);
            flameColors[4] = rgb565(20, 100, 20);
            flameColors[5] = rgb565(10, 60, 10);
            coreColor = rgb565(220, 255, 220);
            break;
        case 3:  // Violett
            flameColors[0] = rgb565(255, 200, 255);
            flameColors[1] = rgb565(220, 130, 255);
            flameColors[2] = rgb565(180, 80, 220);
            flameColors[3] = rgb565(140, 50, 180);
            flameColors[4] = rgb565(100, 30, 140);
            flameColors[5] = rgb565(60, 20, 100);
            coreColor = rgb565(255, 220, 255);
            break;
        default:  // Klassisch Orange/Gelb
            flameColors[0] = rgb565(255, 255, 180);  // Kern (hellgelb)
            flameColors[1] = rgb565(255, 230, 100);  // Gelb
            flameColors[2] = rgb565(255, 180, 50);   // Gelb-Orange
            flameColors[3] = rgb565(255, 120, 20);   // Orange
            flameColors[4] = rgb565(220, 70, 0);     // Dunkelorange
            flameColors[5] = rgb565(150, 40, 0);     // Dunkelrot
            coreColor = rgb565(255, 255, 220);
            break;
    }
    
    // ===== HAUPTFEUER - Ein zusammenhängender Feuerbereich =====
    // Von unten nach oben mit abnehmender Breite und Dichte
    for (int fy = 0; fy < height; fy++) {
        float yProgress = (float)fy / height;  // 0 = unten, 1 = oben
        
        // Breite nimmt nach oben ab (organische Form)
        float widthFactor = (1.0f - yProgress * yProgress) * 0.9f + 0.1f;
        int lineWidth = (int)(width / 2 * widthFactor);
        
        // Wellenbewegung für natürlicheren Look
        int waveOffset = (int)(sin((fy + _fireplaceFlamePhase * 2) * 0.5f) * 3);
        
        for (int fx = -lineWidth; fx <= lineWidth; fx++) {
            uint32_t seed = simpleRandom(fx * 23 + fy * 47 + _fireplaceFlamePhase * 5);
            
            // Dichte nimmt nach oben ab
            float density = 1.0f - yProgress * 0.7f;
            if ((seed % 100) > density * 100) continue;
            
            // Position mit leichtem Flackern
            int flickerX = ((seed / 7) % 3) - 1;
            int px = x + fx + waveOffset + flickerX;
            int py = y - fy;
            
            // Farbe: innen heller (Kern), außen dunkler
            float distFromCenter = abs(fx) / (float)(lineWidth + 1);
            int baseColorIdx = (int)(yProgress * 4);  // Oben dunkler
            int colorIdx = baseColorIdx + (int)(distFromCenter * 2);
            if (colorIdx > 5) colorIdx = 5;
            if (colorIdx < 0) colorIdx = 0;
            
            // Kern in der Mitte (nur unten)
            if (abs(fx) < 3 && fy < height / 3) {
                colorIdx = max(0, colorIdx - 2);
            }
            
            if (px >= x - width/2 && px <= x + width/2 && py >= 0 && py < _currentCanvas->height()) {
                _currentCanvas->drawPixel(px, py, flameColors[colorIdx]);
            }
        }
    }
    
    // ===== FLAMMENSPITZEN (verzweigte Flammen oben - verbunden mit Hauptfeuer) =====
    int numTips = 4 + (_fireplaceFlamePhase % 2);
    for (int t = 0; t < numTips; t++) {
        uint32_t seed = simpleRandom(t * 67 + _fireplaceFlamePhase * 11);
        
        // Position der Flammenspitze - startet wo das Hauptfeuer aufhört (y - height * 0.7)
        // So dass sie im oberen Bereich des Hauptfeuers beginnt und darüber hinausragt
        int tipBaseX = x - width/4 + (seed % (width / 2));
        int tipStartY = (int)(y - height * 0.65);  // Startet im oberen Drittel des Hauptfeuers
        int tipHeight = 4 + (seed % 6);
        int tipWidth = 1 + (seed % 2);
        
        // Flammenspitze zeichnen (von unten nach oben)
        for (int i = 0; i < tipHeight; i++) {
            float progress = (float)i / tipHeight;
            int currentWidth = (int)(tipWidth * (1.0f - progress * 0.8f));
            int flickerX = ((seed + i * 3 + _fireplaceFlamePhase * 2) % 3) - 1;
            
            for (int dx = -currentWidth; dx <= currentWidth; dx++) {
                // Farbe: beginnt mit Hauptfeuer-Farbe (Mitte) und wird nach oben dunkler
                int colorIdx = 1 + (int)(progress * 3);
                if (colorIdx > 5) colorIdx = 5;
                int px = tipBaseX + dx + flickerX;
                int py = tipStartY - i;
                if (px >= x - width/2 && px <= x + width/2 && py >= 0) {
                    _currentCanvas->drawPixel(px, py, flameColors[colorIdx]);
                }
            }
        }
    }
    
    // ===== HELLER KERN am Boden =====
    for (int cy = 0; cy < 4; cy++) {
        int coreWidth = (width / 3) - cy * 2;
        for (int cx = -coreWidth; cx <= coreWidth; cx++) {
            uint32_t seed = simpleRandom(cx * 13 + cy * 37);
            if (seed % 3 < 2) {
                int px = x + cx;
                int py = y - cy;
                if (py >= 0) {
                    _currentCanvas->drawPixel(px, py, coreColor);
                }
            }
        }
    }
    
    // ===== GLUT am Boden =====
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
    
    // ===== FUNKEN =====
    for (int i = 0; i < 6; i++) {
        uint32_t seed = simpleRandom(i * 17 + _fireplaceFlamePhase * 7);
        if (seed % 5 == 0) {
            int sparkX = x - width/3 + (seed % (width * 2 / 3));
            int sparkY = y - height - (seed % 8);
            if (sparkY >= 0 && sparkX >= x - width/2 && sparkX <= x + width/2) {
                _currentCanvas->drawPixel(sparkX, sparkY, flameColors[seed % 2]);
            }
        }
    }
}

void AnimationsModule::drawStockings(int simsY, int simsWidth, int centerX) {
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

void AnimationsModule::drawMantleDecorations(int simsY, int simsWidth, int centerX, float scale) {
    // Analoge Uhr in der Mitte?
    bool showClock = config ? config->fireplaceClockEnabled : false;
    
    // Dekorative Gegenstände
    int decoCount = config ? config->fireplaceCandleCount : 2;
    if (decoCount < 0) decoCount = 0;
    
    // Wenn Uhr aktiv: max 2 Dekorationen (links + rechts)
    // Wenn Uhr aus: max 3 Dekorationen
    int maxDeco = showClock ? 2 : 3;
    if (decoCount > maxDeco) decoCount = maxDeco;
    
    // ===== ANALOGE UHR IN DER MITTE =====
    if (showClock) {
        // Hole aktuelle Zeit via GeneralTimeConverter
        time_t now_utc;
        time(&now_utc);
        time_t local_now = timeConverter.toLocal(now_utc);
        struct tm tm_now;
        localtime_r(&local_now, &tm_now);
        
        int hours = tm_now.tm_hour % 12;
        int minutes = tm_now.tm_min;
        
        int clockCx = centerX;
        int clockCy = simsY - 1;  // Auf dem Sims stehend
        int clockR = (int)(10 * scale);  // Radius - optimale Größe für Sichtbarkeit und Platz
        
        // Uhren-Basis/Gehäuse
        uint16_t caseColor = rgb565(60, 45, 30);
        uint16_t faceColor = rgb565(240, 235, 220);
        uint16_t handColor = rgb565(30, 30, 30);
        uint16_t hourMarks = rgb565(80, 60, 40);
        
        // Gehäuse (Sockel)
        int baseH = 4;
        _currentCanvas->fillRect(clockCx - clockR - 2, clockCy - baseH, (clockR + 2) * 2, baseH, caseColor);
        
        // Zifferblatt (Kreis)
        int faceCy = clockCy - baseH - clockR;
        _currentCanvas->fillCircle(clockCx, faceCy, clockR, faceColor);
        _currentCanvas->drawCircle(clockCx, faceCy, clockR, caseColor);
        
        // Stundenmarkierungen (12, 3, 6, 9) - größere Markierungen
        _currentCanvas->drawLine(clockCx, faceCy - clockR + 1, clockCx, faceCy - clockR + 2, hourMarks);  // 12
        _currentCanvas->drawLine(clockCx + clockR - 2, faceCy, clockCx + clockR - 1, faceCy, hourMarks);  // 3
        _currentCanvas->drawLine(clockCx, faceCy + clockR - 2, clockCx, faceCy + clockR - 1, hourMarks);  // 6
        _currentCanvas->drawLine(clockCx - clockR + 1, faceCy, clockCx - clockR + 2, faceCy, hourMarks);  // 9
        
        // Stundenzeiger (kürzer, aber dicker)
        float hourAngle = (hours + minutes / 60.0f) * 30.0f - 90.0f;  // 30° pro Stunde
        float hourRad = hourAngle * 3.14159f / 180.0f;
        int hourLen = (int)(clockR * 0.55f);  // 55% des Radius
        int hx = clockCx + (int)(cos(hourRad) * hourLen);
        int hy = faceCy + (int)(sin(hourRad) * hourLen);
        _currentCanvas->drawLine(clockCx, faceCy, hx, hy, handColor);
        // Dickerer Stundenzeiger (zusätzliche Linie parallel)
        _currentCanvas->drawLine(clockCx + 1, faceCy, hx + 1, hy, handColor);
        
        // Minutenzeiger (länger)
        float minAngle = minutes * 6.0f - 90.0f;  // 6° pro Minute
        float minRad = minAngle * 3.14159f / 180.0f;
        int minLen = (int)(clockR * 0.8f);  // 80% des Radius
        int mx = clockCx + (int)(cos(minRad) * minLen);
        int my = faceCy + (int)(sin(minRad) * minLen);
        _currentCanvas->drawLine(clockCx, faceCy, mx, my, handColor);
        
        // Mittelpunkt (kleiner und rot, um sich von den Zeigern abzuheben)
        _currentCanvas->fillCircle(clockCx, faceCy, 1, rgb565(255, 0, 0));
    }
    
    // ===== DEKORATIONEN LINKS UND RECHTS =====
    if (decoCount == 0) return;
    
    // Positionen: bei showClock nur links/rechts, sonst auch Mitte möglich
    int positions[3];
    int decoTypes[3];
    
    if (showClock) {
        // Nur links und rechts
        positions[0] = centerX - simsWidth/3;
        positions[1] = centerX + simsWidth/3;
        decoTypes[0] = 0;  // Blumenvase
        decoTypes[1] = 2;  // Bilderrahmen
    } else {
        if (decoCount == 1) {
            positions[0] = centerX;
            decoTypes[0] = 0;
        } else if (decoCount == 2) {
            positions[0] = centerX - simsWidth/3;
            positions[1] = centerX + simsWidth/3;
            decoTypes[0] = 0;
            decoTypes[1] = 2;
        } else {
            positions[0] = centerX - simsWidth/3;
            positions[1] = centerX;
            positions[2] = centerX + simsWidth/3;
            decoTypes[0] = 0;
            decoTypes[1] = 1;
            decoTypes[2] = 2;
        }
    }
    
    // Verschiedene Dekorationen zeichnen
    for (int i = 0; i < decoCount; i++) {
        int cx = positions[i];
        int cy = simsY - 1;  // Direkt auf dem Sims
        int decoType = decoTypes[i];
        
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

void AnimationsModule::drawLedBorder() {
    // Prüfe ob LED-Rahmen aktiviert ist
    if (!config || !config->ledBorderEnabled) return;
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Parse LED-Farben aus Config
    uint16_t ledColors[4];
    int numColors = 0;
    
    if (config && config->ledBorderColors.length() > 0) {
        PsramString colors = config->ledBorderColors;
        size_t pos = 0;
        while (numColors < 4 && pos < colors.length()) {
            size_t commaPos = colors.find(',', pos);
            if (commaPos == PsramString::npos) commaPos = colors.length();
            PsramString hexColor = colors.substr(pos, commaPos - pos);
            ledColors[numColors] = hexToRgb565(hexColor.c_str());
            pos = commaPos + 1;
            numColors++;
        }
    }
    
    // Fallback auf Standard-Farben
    if (numColors == 0) {
        ledColors[0] = rgb565(255, 0, 0);    // Rot
        ledColors[1] = rgb565(0, 255, 0);    // Grün
        ledColors[2] = rgb565(0, 0, 255);    // Blau
        ledColors[3] = rgb565(255, 255, 0);  // Gelb
        numColors = 4;
    }
    
    // Zeichne jedes einzelne Pixel des Rahmens mit Farbzyklus
    // LED1=Farbe1, LED2=Farbe2, LED3=Farbe3, LED4=Farbe4, dann wiederholen
    // Animation: Phase verschiebt, welche Farbe an welcher Position ist
    
    int pixelIndex = 0;
    
    // Obere Kante (links nach rechts)
    for (int x = 0; x < canvasW; x++) {
        int colorIdx = (pixelIndex + _ledBorderPhase) % numColors;
        _currentCanvas->drawPixel(x, 0, ledColors[colorIdx]);
        pixelIndex++;
    }
    
    // Rechte Kante (oben nach unten) - ohne Eckpixel
    for (int y = 1; y < canvasH; y++) {
        int colorIdx = (pixelIndex + _ledBorderPhase) % numColors;
        _currentCanvas->drawPixel(canvasW - 1, y, ledColors[colorIdx]);
        pixelIndex++;
    }
    
    // Untere Kante (rechts nach links) - ohne Eckpixel
    for (int x = canvasW - 2; x >= 0; x--) {
        int colorIdx = (pixelIndex + _ledBorderPhase) % numColors;
        _currentCanvas->drawPixel(x, canvasH - 1, ledColors[colorIdx]);
        pixelIndex++;
    }
    
    // Linke Kante (unten nach oben) - ohne Eckpixel
    for (int y = canvasH - 2; y > 0; y--) {
        int colorIdx = (pixelIndex + _ledBorderPhase) % numColors;
        _currentCanvas->drawPixel(0, y, ledColors[colorIdx]);
        pixelIndex++;
    }
}

// ========== OBSOLETE HELPER METHODS - CAN BE REMOVED ==========
// These methods were replaced by the new procedural fireplace implementation
// Keeping them commented out for reference

/*
void AnimationsModule::drawFireplaceFrame(int x, int y, int width, int height, uint16_t brickColor, uint16_t brickDark) {
    // Zeichne Kaminrahmen mit detailliertem Ziegelmuster
    _currentCanvas->fillRect(x, y, width, height, brickColor);
    
    // Ziegel-Muster mit versetzten Reihen
    int brickHeight = 6;
    int brickWidth = 8;
    
    for (int row = 0; row < height / brickHeight; row++) {
        int rowY = y + row * brickHeight;
        int offset = (row % 2) * (brickWidth / 2);
        
        // Horizontale Fugen
        _currentCanvas->drawLine(x, rowY, x + width, rowY, brickDark);
        
        // Vertikale Fugen
        for (int col = 0; col < (width / brickWidth) + 2; col++) {
            int colX = x + col * brickWidth + offset;
            if (colX >= x && colX < x + width) {
                _currentCanvas->drawLine(colX, rowY, colX, rowY + brickHeight - 1, brickDark);
            }
        }
    }
}

void AnimationsModule::drawFireplaceMantel(int x, int y, int width, int height, uint16_t color, uint16_t lightColor, uint16_t darkColor) {
    // Kaminsims mit 3D-Profil und Details
    int upperHeight = height * 0.4;
    int lowerHeight = height - upperHeight;
    
    // Oberer schmaler Teil (Leiste)
    _currentCanvas->fillRect(x + 2, y, width - 4, upperHeight, lightColor);
    _currentCanvas->drawLine(x + 2, y, x + width - 2, y, lightColor);  // Highlight oben
    
    // Unterer breiter Teil (Basis)
    _currentCanvas->fillRect(x, y + upperHeight, width, lowerHeight, color);
    
    // 3D-Effekt: Schatten und Highlights
    _currentCanvas->drawLine(x, y + upperHeight, x + width, y + upperHeight, darkColor);  // Schatten unter Leiste
    _currentCanvas->drawLine(x, y + height - 1, x + width, y + height - 1, darkColor);    // Schatten unten
    _currentCanvas->drawLine(x, y + upperHeight, x, y + height, darkColor);                // Linker Schatten
    
    // Dekorative Linien
    int decoY = y + upperHeight + lowerHeight / 2;
    _currentCanvas->drawLine(x + 4, decoY, x + width - 4, decoY, rgb565(
        min(255, ((color >> 11) & 0x1F) * 8 + 40),
        min(255, ((color >> 5) & 0x3F) * 4 + 30),
        min(255, (color & 0x1F) * 8 + 30)
    ));
}

void AnimationsModule::drawFireplaceLogs(int x, int y, int width, float scale) {
    // Holzscheite im Kamin mit realistischer Darstellung
    uint16_t woodOuter = rgb565(96, 64, 32);    // Rinde
    uint16_t woodInner = rgb565(192, 144, 96);  // Holz innen
    uint16_t woodDark = rgb565(48, 32, 16);     // Schatten
    uint16_t woodRing = rgb565(144, 96, 64);    // Jahresringe
    
    int logRadius = (int)(4 * scale);
    int logLength = (int)(width * 0.4);
    
    // Linkes Scheit (liegend)
    int log1X = x + 5;
    int log1Y = y - logRadius;
    
    // Körper des Scheits
    for (int i = 0; i < logLength; i++) {
        int lx = log1X + i;
        _currentCanvas->drawLine(lx, log1Y - logRadius + 1, lx, log1Y + logRadius - 1, woodOuter);
        _currentCanvas->drawPixel(lx, log1Y - logRadius, woodDark);
        if (i % 3 == 0) {
            _currentCanvas->drawPixel(lx, log1Y, woodRing);  // Textur
        }
    }
    
    // Schnittfläche links
    _currentCanvas->fillCircle(log1X, log1Y, logRadius, woodInner);
    _currentCanvas->drawCircle(log1X, log1Y, logRadius, woodOuter);
    if (logRadius > 2) {
        _currentCanvas->drawCircle(log1X, log1Y, logRadius - 2, woodRing);
        _currentCanvas->drawPixel(log1X, log1Y, woodDark);
    }
    
    // Rechtes Scheit (leicht versetzt)
    int log2X = x + 10;
    int log2Y = y - logRadius - 1;
    int log2Length = logLength - 5;
    
    for (int i = 0; i < log2Length; i++) {
        int lx = log2X + i;
        _currentCanvas->drawLine(lx, log2Y - logRadius + 1, lx, log2Y + logRadius - 1, woodOuter);
        _currentCanvas->drawPixel(lx, log2Y - logRadius, woodDark);
        if (i % 4 == 0) {
            _currentCanvas->drawPixel(lx, log2Y, woodRing);
        }
    }
    
    // Schnittfläche rechts (am Ende)
    int log2EndX = log2X + log2Length;
    _currentCanvas->fillCircle(log2EndX, log2Y, logRadius, woodInner);
    _currentCanvas->drawCircle(log2EndX, log2Y, logRadius, woodOuter);
    if (logRadius > 2) {
        _currentCanvas->drawCircle(log2EndX, log2Y, logRadius - 2, woodRing);
        _currentCanvas->drawPixel(log2EndX, log2Y, woodDark);
    }
}

void AnimationsModule::drawFireplaceTools(int x, int y, int height, float scale) {
    // Kaminwerkzeuge mit Details
    uint16_t toolColor = rgb565(70, 70, 70);      // Metall
    uint16_t toolDark = rgb565(40, 40, 40);       // Dunkleres Metall
    uint16_t handleColor = rgb565(100, 70, 40);   // Holzgriff
    uint16_t brassColor = rgb565(180, 150, 50);   // Messing-Akzent
    
    int baseY = y;
    int toolHeight = height;
    
    // Schürhaken (poker) - links
    int pokerX = x + (int)(3 * scale);
    int pokerY = baseY - toolHeight;
    
    // Stiel mit Schattierung
    _currentCanvas->drawLine(pokerX, pokerY, pokerX, baseY - (int)(8 * scale), toolColor);
    _currentCanvas->drawLine(pokerX + 1, pokerY, pokerX + 1, baseY - (int)(8 * scale), toolDark);
    
    // Holzgriff
    int handleStart = baseY - (int)(8 * scale);
    _currentCanvas->drawLine(pokerX, handleStart, pokerX, baseY - 2, handleColor);
    _currentCanvas->drawLine(pokerX + 1, handleStart, pokerX + 1, baseY - 2, rgb565(80, 55, 30));
    
    // Haken oben
    _currentCanvas->drawLine(pokerX, pokerY, pokerX + 3, pokerY + 2, toolColor);
    _currentCanvas->drawLine(pokerX + 3, pokerY + 2, pokerX + 3, pokerY + 5, toolColor);
    _currentCanvas->drawPixel(pokerX + 1, pokerY + 1, toolDark);
    
    // Messing-Ring
    _currentCanvas->drawPixel(pokerX, handleStart + (int)(3 * scale), brassColor);
    
    // Schaufel - rechts vom Schürhaken
    int shovelX = x + (int)(9 * scale);
    int shovelY = baseY - toolHeight + (int)(3 * scale);
    
    // Stiel
    _currentCanvas->drawLine(shovelX, shovelY, shovelX, baseY - (int)(8 * scale), toolColor);
    _currentCanvas->drawLine(shovelX + 1, shovelY, shovelX + 1, baseY - (int)(8 * scale), toolDark);
    
    // Holzgriff
    _currentCanvas->drawLine(shovelX, handleStart, shovelX, baseY - 2, handleColor);
    _currentCanvas->drawLine(shovelX + 1, handleStart, shovelX + 1, baseY - 2, rgb565(80, 55, 30));
    
    // Schaufelblatt
    int bladeW = (int)(6 * scale);
    int bladeH = (int)(5 * scale);
    _currentCanvas->fillRect(shovelX - bladeW/2, shovelY, bladeW, bladeH, toolColor);
    _currentCanvas->drawRect(shovelX - bladeW/2, shovelY, bladeW, bladeH, toolDark);
    // Highlight
    _currentCanvas->drawLine(shovelX - 1, shovelY + 1, shovelX - 1, shovelY + bladeH - 1, rgb565(100, 100, 100));
    
    // Messing-Ring
    _currentCanvas->drawPixel(shovelX, handleStart + (int)(3 * scale), brassColor);
}

void AnimationsModule::drawWoodStorage(int x, int y, int width, int height, float scale) {
    // Holzlager mit gestapelten Scheiten
    uint16_t woodVariants[] = {
        rgb565(96, 64, 32),    // Dunkel
        rgb565(128, 88, 48),   // Mittel-Dunkel
        rgb565(160, 112, 64),  // Mittel-Hell
        rgb565(112, 72, 40)    // Mittel
    };
    
    uint16_t woodInner = rgb565(192, 144, 96);
    uint16_t woodRing = rgb565(144, 96, 64);
    uint16_t woodDark = rgb565(48, 32, 16);
    
    int storageLogRadius = (int)(3.5 * scale);
    int storageLogLength = (int)(22 * scale);
    
    // UNTERE LAGE: Horizontal
    int numBottomLogs = min(5, width / (int)(23 * scale));
    for (int i = 0; i < numBottomLogs; i++) {
        int lx = x + (int)(i * 23 * scale);
        int ly = y - storageLogRadius - 2;
        
        if (lx + storageLogLength <= x + width) {
            uint16_t logColor = woodVariants[i % 4];
            
            // Scheit horizontal
            for (int j = 0; j < storageLogLength; j++) {
                int px = lx + j;
                _currentCanvas->drawLine(px, ly - storageLogRadius + 1, px, ly + storageLogRadius - 1, logColor);
                if (j % 8 == 0) {
                    _currentCanvas->drawPixel(px, ly, woodDark);  // Textur
                }
            }
            
            // Schnittflächen
            _currentCanvas->fillCircle(lx, ly, storageLogRadius, woodInner);
            _currentCanvas->drawCircle(lx, ly, storageLogRadius, logColor);
            if (storageLogRadius > 2) {
                _currentCanvas->drawCircle(lx, ly, storageLogRadius - 2, woodRing);
                _currentCanvas->drawPixel(lx, ly, woodDark);
            }
            
            if (lx + storageLogLength <= x + width) {
                _currentCanvas->fillCircle(lx + storageLogLength, ly, storageLogRadius, woodInner);
                _currentCanvas->drawCircle(lx + storageLogLength, ly, storageLogRadius, logColor);
                if (storageLogRadius > 2) {
                    _currentCanvas->drawCircle(lx + storageLogLength, ly, storageLogRadius - 2, woodRing);
                }
            }
        }
    }
    
    // MITTLERE LAGE: Quer gestapelt (Kreuzstapelung)
    int numMiddleLogs = min(4, (width - 5) / (int)(8 * scale));
    int middleY = y - storageLogRadius * 2 - (int)(4 * scale);
    for (int i = 0; i < numMiddleLogs; i++) {
        int lx = x + (int)((i * (storageLogLength / numMiddleLogs + 5)) * scale);
        int ly = middleY;
        
        if (lx < x + width - 5) {
            uint16_t logColor = woodVariants[(i + 1) % 4];
            int thisLogLength = min(storageLogLength, (int)((x + width - lx - 5) * 0.9));
            
            for (int j = 0; j < thisLogLength; j++) {
                int px = lx + j;
                if (px < x + width) {
                    _currentCanvas->drawLine(px, ly - storageLogRadius + 1, px, ly + storageLogRadius - 1, logColor);
                }
            }
            
            _currentCanvas->fillCircle(lx, ly, storageLogRadius, woodInner);
            _currentCanvas->drawCircle(lx, ly, storageLogRadius, logColor);
            if (storageLogRadius > 2) {
                _currentCanvas->drawCircle(lx, ly, storageLogRadius - 2, woodRing);
            }
        }
    }
    
    // OBERE LAGE: Wieder horizontal
    int topY = y - storageLogRadius * 4 - (int)(8 * scale);
    if (topY > y - height + 5) {
        int numTopLogs = min(3, width / (int)(28 * scale));
        for (int i = 0; i < numTopLogs; i++) {
            int lx = x + (int)((i * 28 + 5) * scale);
            int ly = topY;
            
            if (lx + storageLogLength - 8 <= x + width) {
                uint16_t logColor = woodVariants[(i + 2) % 4];
                int thisLogLength = storageLogLength - 8;
                
                for (int j = 0; j < thisLogLength; j++) {
                    int px = lx + j;
                    if (px < x + width) {
                        _currentCanvas->drawLine(px, ly - storageLogRadius + 1, px, ly + storageLogRadius - 1, logColor);
                    }
                }
                
                _currentCanvas->fillCircle(lx, ly, storageLogRadius, woodInner);
                _currentCanvas->drawCircle(lx, ly, storageLogRadius, logColor);
                if (storageLogRadius > 2) {
                    _currentCanvas->drawCircle(lx, ly, storageLogRadius - 2, woodRing);
                }
            }
        }
    }
}
*/
