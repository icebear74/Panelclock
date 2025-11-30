#include "AdventWreathModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include <time.h>

// Hilfsfunktion für RGB565
uint16_t AdventWreathModule::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Hilfsfunktion um Hex-Farbe zu RGB565 zu konvertieren
uint16_t AdventWreathModule::hexToRgb565(const char* hex) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        return rgb565(255, 255, 255);
    }
    long r = strtol(PsramString(hex + 1, 2).c_str(), NULL, 16);
    long g = strtol(PsramString(hex + 3, 2).c_str(), NULL, 16);
    long b = strtol(PsramString(hex + 5, 2).c_str(), NULL, 16);
    return rgb565(r, g, b);
}

// Einfacher Pseudo-Zufallsgenerator
uint32_t AdventWreathModule::simpleRandom(uint32_t seed) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

AdventWreathModule::AdventWreathModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, 
                                       GeneralTimeConverter& timeConverter, DeviceConfig* config)
    : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), config(config) {
}

AdventWreathModule::~AdventWreathModule() {
}

void AdventWreathModule::begin() {
    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);
    _lastCheckedDay = tm_now.tm_mday;
    setConfig();
    shuffleCandleOrder();
    Log.println("[AdventWreath] Modul initialisiert");
}

void AdventWreathModule::setConfig() {
    if (config) {
        _displayDurationMs = config->adventWreathDisplaySec * 1000UL;
        _repeatIntervalMs = config->adventWreathRepeatMin * 60UL * 1000UL;
        _flameAnimationMs = config->adventWreathFlameSpeedMs;
    }
}

void AdventWreathModule::onUpdate(std::function<void()> callback) {
    _updateCallback = callback;
}

void AdventWreathModule::shuffleCandleOrder() {
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

bool AdventWreathModule::isAdventSeason() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int month = tm_now.tm_mon + 1;
    int day = tm_now.tm_mday;
    int year = tm_now.tm_year + 1900;
    
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
    
    // Adventskranz wird bis zum 24.12. gezeigt
    if (month == 12 && day <= 24) {
        return true;
    } else if (month == 11 && day >= startDay) {
        return true;
    }
    
    return false;
}

bool AdventWreathModule::isChristmasSeason() {
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

bool AdventWreathModule::isHolidaySeason() {
    return isAdventSeason() || isChristmasSeason();
}

ChristmasDisplayMode AdventWreathModule::getCurrentDisplayMode() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int month = tm_now.tm_mon + 1;
    int day = tm_now.tm_mday;
    bool treeEnabled = config ? config->christmasTreeEnabled : true;
    bool wreathEnabled = config ? config->adventWreathEnabled : true;
    
    // Adventskranz bis zum 24.12., danach nur noch Baum
    if (month == 12 && day > 24) {
        return treeEnabled ? ChristmasDisplayMode::Tree : ChristmasDisplayMode::Wreath;
    } else if (month == 1) {
        // Januar: nur Baum (wenn noch in der Saison)
        return treeEnabled ? ChristmasDisplayMode::Tree : ChristmasDisplayMode::Wreath;
    }
    
    // Vor/am 24.12.: Wechsel zwischen beiden wenn aktiviert
    if (wreathEnabled && treeEnabled) {
        return ChristmasDisplayMode::Alternate;
    } else if (treeEnabled) {
        return ChristmasDisplayMode::Tree;
    }
    
    return ChristmasDisplayMode::Wreath;
}

int AdventWreathModule::calculateCurrentAdvent() {
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

time_t AdventWreathModule::calculateFourthAdvent(int year) {
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

void AdventWreathModule::periodicTick() {
    if (!config) return;
    
    if (!config->adventWreathEnabled && !config->christmasTreeEnabled) return;
    
    unsigned long now = millis();
    if (now - _lastPeriodicCheck < 1000) return;
    _lastPeriodicCheck = now;
    
    if (!isHolidaySeason()) {
        if (_isAdventViewActive) {
            releasePriorityEx(_currentAdventUID);
            _isAdventViewActive = false;
            Log.println("[AdventWreath] Keine Weihnachtszeit mehr");
        }
        return;
    }
    
    unsigned long minInterval = (_lastAdventDisplayTime == 0) ? 0 : _repeatIntervalMs;
    
    if (!_isAdventViewActive && (now - _lastAdventDisplayTime > minInterval)) {
        _isAdventViewActive = true;
        _adventViewStartTime = now;
        
        shuffleCandleOrder();
        
        ChristmasDisplayMode mode = getCurrentDisplayMode();
        if (mode == ChristmasDisplayMode::Alternate) {
            _showTree = (_displayCounter % 2 == 1);
        } else {
            _showTree = (mode == ChristmasDisplayMode::Tree);
        }
        _displayCounter++;
        
        _currentAdventUID = _showTree ? CHRISTMAS_TREE_UID_BASE : ADVENT_WREATH_UID_BASE + calculateCurrentAdvent();
        
        unsigned long safeDuration = _displayDurationMs + 5000UL;
        Priority prio = config->adventWreathInterrupt ? Priority::Low : Priority::PlayNext;
        bool success = requestPriorityEx(prio, _currentAdventUID, safeDuration);
        
        if (success) {
            Log.printf("[AdventWreath] %s %s angefordert\n", 
                       _showTree ? "Weihnachtsbaum" : "Adventskranz",
                       config->adventWreathInterrupt ? "Interrupt" : "PlayNext");
        } else {
            Log.println("[AdventWreath] Request abgelehnt!");
            _isAdventViewActive = false;
        }
    }
    else if (_isAdventViewActive && (now - _adventViewStartTime > _displayDurationMs)) {
        releasePriorityEx(_currentAdventUID);
        _isAdventViewActive = false;
        _lastAdventDisplayTime = now;
    }
}

void AdventWreathModule::tick() {
    unsigned long now = millis();
    bool needsUpdate = false;
    
    // Flammen-Animation
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
    
    if (needsUpdate && _updateCallback) {
        _updateCallback();
    }
}

void AdventWreathModule::logicTick() {
}

void AdventWreathModule::draw() {
    canvas.fillScreen(0);
    u8g2.begin(canvas);
    
    if (_showTree) {
        drawChristmasTree();
    } else {
        // Adventskranz ohne Beschriftung - mehr Platz für die Grafik
        drawGreenery();
        drawWreath();
        drawBerries();
    }
}

void AdventWreathModule::drawChristmasTree() {
    int centerX = canvas.width() / 2;
    int baseY = 62; // Basis des Baums
    
    // Zeichne zuerst den Baumstamm
    uint16_t trunkColor = rgb565(139, 69, 19);
    uint16_t trunkDark = rgb565(100, 50, 15);
    canvas.fillRect(centerX - 4, baseY - 8, 8, 10, trunkColor);
    canvas.drawLine(centerX - 4, baseY - 8, centerX - 4, baseY + 2, trunkDark);
    
    // Natürlicherer Baum mit mehreren Grüntönen und organischer Form
    drawNaturalTree(centerX, baseY);
    
    // Stern an der Spitze
    uint16_t starColor = rgb565(255, 255, 0);
    uint16_t starGlow = rgb565(255, 230, 100);
    int starY = 6;
    canvas.fillCircle(centerX, starY, 3, starColor);
    // Strahlen
    canvas.drawLine(centerX, starY - 5, centerX, starY + 5, starGlow);
    canvas.drawLine(centerX - 5, starY, centerX + 5, starY, starGlow);
    canvas.drawLine(centerX - 3, starY - 3, centerX + 3, starY + 3, starGlow);
    canvas.drawLine(centerX - 3, starY + 3, centerX + 3, starY - 3, starGlow);
    
    // Kugeln am Baum
    drawTreeOrnaments(centerX, baseY);
    
    // Blinkende Lichter
    drawTreeLights();
    
    // Geschenke unter dem Baum
    drawGifts(centerX, baseY);
}

void AdventWreathModule::drawNaturalTree(int centerX, int baseY) {
    // Grüntöne für natürlicheren Look
    uint16_t greens[] = {
        rgb565(0, 80, 0),     // Dunkelgrün
        rgb565(0, 100, 20),   // Mittelgrün
        rgb565(20, 120, 30),  // Hellgrün
        rgb565(0, 90, 10),    // Dunkel-Mittel
        rgb565(34, 100, 34)   // Forest Green
    };
    int numGreens = sizeof(greens) / sizeof(greens[0]);
    
    // Baum in drei Schichten mit natürlicherer Form
    // Unterste Schicht (breiteste)
    int layer1Top = baseY - 8;
    int layer1Bottom = baseY - 26;
    for (int y = layer1Top; y >= layer1Bottom; y--) {
        int progress = layer1Top - y;
        int maxWidth = 28 - progress * 0.8;
        
        // Unregelmäßige Kante durch Variation
        for (int x = -maxWidth; x <= maxWidth; x++) {
            uint32_t seed = simpleRandom((y * 47 + x * 13) ^ 0xDEAD);
            int edgeVar = (seed % 3) - 1;
            if (abs(x) <= maxWidth + edgeVar) {
                int colorIdx = seed % numGreens;
                canvas.drawPixel(centerX + x, y, greens[colorIdx]);
            }
        }
    }
    
    // Mittlere Schicht
    int layer2Top = baseY - 22;
    int layer2Bottom = baseY - 40;
    for (int y = layer2Top; y >= layer2Bottom; y--) {
        int progress = layer2Top - y;
        int maxWidth = 22 - progress * 0.9;
        
        for (int x = -maxWidth; x <= maxWidth; x++) {
            uint32_t seed = simpleRandom((y * 53 + x * 17) ^ 0xBEEF);
            int edgeVar = (seed % 3) - 1;
            if (abs(x) <= maxWidth + edgeVar) {
                int colorIdx = seed % numGreens;
                canvas.drawPixel(centerX + x, y, greens[colorIdx]);
            }
        }
    }
    
    // Oberste Schicht (Spitze)
    int layer3Top = baseY - 36;
    int layer3Bottom = baseY - 54;
    for (int y = layer3Top; y >= layer3Bottom; y--) {
        int progress = layer3Top - y;
        int maxWidth = 16 - progress * 0.85;
        if (maxWidth < 1) maxWidth = 1;
        
        for (int x = -maxWidth; x <= maxWidth; x++) {
            uint32_t seed = simpleRandom((y * 59 + x * 19) ^ 0xCAFE);
            int edgeVar = (seed % 2);
            if (abs(x) <= maxWidth + edgeVar) {
                int colorIdx = seed % numGreens;
                canvas.drawPixel(centerX + x, y, greens[colorIdx]);
            }
        }
    }
}

void AdventWreathModule::drawTreeOrnaments(int centerX, int baseY) {
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
    
    // Kugeln verteilt über den Baum
    int ornaments[][3] = {
        {centerX - 15, baseY - 15, 3},
        {centerX + 12, baseY - 18, 3},
        {centerX - 8, baseY - 30, 2},
        {centerX + 16, baseY - 22, 2},
        {centerX - 20, baseY - 12, 2},
        {centerX + 6, baseY - 35, 3},
        {centerX - 10, baseY - 42, 2},
        {centerX + 10, baseY - 25, 2},
        {centerX, baseY - 20, 3},
        {centerX - 5, baseY - 48, 2},
        {centerX + 8, baseY - 45, 2}
    };
    int numOrnaments = 11;
    
    for (int i = 0; i < numOrnaments; i++) {
        uint32_t seed = simpleRandom(i * 123 + 456);
        uint16_t color = ornamentColors[seed % numColors];
        drawOrnament(ornaments[i][0], ornaments[i][1], ornaments[i][2], color);
    }
}

void AdventWreathModule::drawTreeLights() {
    int centerX = canvas.width() / 2;
    int baseY = 62;
    
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
    
    // Lichter über den Baum verteilt (pseudo-zufällig basierend auf lightCount)
    for (int i = 0; i < lightCount; i++) {
        // Berechne Position basierend auf Index
        uint32_t seed = simpleRandom(i * 37 + 789);
        
        // Y-Position: verteilt zwischen baseY-12 und baseY-52
        int yRange = 40;
        int lightY = baseY - 12 - (seed % yRange);
        
        // X-Position abhängig von Y (breiter unten, schmaler oben)
        int progress = (baseY - 12 - lightY);
        int maxX = 24 - (progress * 0.5);
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
            
            canvas.fillCircle(lightX, lightY, 1, color);
            // Kleiner Glow-Effekt
            canvas.drawPixel(lightX, lightY - 1, color);
        }
    }
}

void AdventWreathModule::drawGifts(int centerX, int baseY) {
    // Geschenkfarben
    uint16_t giftColors[][2] = {
        {rgb565(200, 0, 0), rgb565(255, 215, 0)},      // Rot mit Gold
        {rgb565(0, 100, 200), rgb565(255, 255, 255)},  // Blau mit Weiß
        {rgb565(0, 150, 0), rgb565(255, 0, 0)},        // Grün mit Rot
        {rgb565(150, 0, 150), rgb565(255, 215, 0)},    // Lila mit Gold
        {rgb565(255, 140, 0), rgb565(200, 0, 0)}       // Orange mit Rot
    };
    
    // Geschenke links und rechts vom Baum
    // Geschenk 1 (links)
    int g1x = centerX - 35;
    int g1y = baseY - 2;
    canvas.fillRect(g1x, g1y - 8, 12, 8, giftColors[0][0]);
    canvas.drawRect(g1x, g1y - 8, 12, 8, rgb565(150, 0, 0));
    canvas.drawLine(g1x + 6, g1y - 8, g1x + 6, g1y, giftColors[0][1]);
    canvas.drawLine(g1x, g1y - 4, g1x + 12, g1y - 4, giftColors[0][1]);
    // Schleife
    canvas.fillCircle(g1x + 4, g1y - 10, 2, giftColors[0][1]);
    canvas.fillCircle(g1x + 8, g1y - 10, 2, giftColors[0][1]);
    
    // Geschenk 2 (rechts)
    int g2x = centerX + 25;
    int g2y = baseY - 2;
    canvas.fillRect(g2x, g2y - 6, 10, 6, giftColors[1][0]);
    canvas.drawRect(g2x, g2y - 6, 10, 6, rgb565(0, 70, 150));
    canvas.drawLine(g2x + 5, g2y - 6, g2x + 5, g2y, giftColors[1][1]);
    canvas.drawLine(g2x, g2y - 3, g2x + 10, g2y - 3, giftColors[1][1]);
    
    // Geschenk 3 (links hinten/kleiner)
    int g3x = centerX - 48;
    int g3y = baseY - 2;
    canvas.fillRect(g3x, g3y - 5, 8, 5, giftColors[2][0]);
    canvas.drawLine(g3x + 4, g3y - 5, g3x + 4, g3y, giftColors[2][1]);
    
    // Geschenk 4 (rechts hinten)
    int g4x = centerX + 38;
    int g4y = baseY - 2;
    canvas.fillRect(g4x, g4y - 7, 9, 7, giftColors[3][0]);
    canvas.drawLine(g4x, g4y - 3, g4x + 9, g4y - 3, giftColors[3][1]);
    canvas.drawLine(g4x + 4, g4y - 7, g4x + 4, g4y, giftColors[3][1]);
    
    // Geschenk 5 (klein, zwischen anderen)
    int g5x = centerX - 25;
    int g5y = baseY - 2;
    canvas.fillRect(g5x, g5y - 5, 7, 5, giftColors[4][0]);
    canvas.drawLine(g5x + 3, g5y - 5, g5x + 3, g5y, giftColors[4][1]);
}

void AdventWreathModule::drawOrnament(int x, int y, int radius, uint16_t color) {
    canvas.fillCircle(x, y, radius, color);
    
    uint8_t r = ((color >> 11) & 0x1F) * 8;
    uint8_t g = ((color >> 5) & 0x3F) * 4;
    uint8_t b = (color & 0x1F) * 8;
    int rH = r + 100; if (rH > 255) rH = 255;
    int gH = g + 100; if (gH > 255) gH = 255;
    int bH = b + 100; if (bH > 255) bH = 255;
    uint16_t highlight = rgb565(rH, gH, bH);
    
    if (radius >= 2) {
        canvas.drawPixel(x - radius/2, y - radius/2, highlight);
    }
    
    uint16_t shadow = rgb565(r / 2, g / 2, b / 2);
    if (radius >= 2) {
        canvas.drawPixel(x + radius/2, y + radius/2, shadow);
    }
}

void AdventWreathModule::drawWreath() {
    int currentAdvent = calculateCurrentAdvent();
    
    int centerX = canvas.width() / 2;
    int baseY = 56; // Angepasst für mehr Platz ohne Titel
    
    int candleSpacing = 38;
    int candlePositions[4] = {
        centerX - candleSpacing - candleSpacing/2,
        centerX - candleSpacing/2,
        centerX + candleSpacing/2,
        centerX + candleSpacing + candleSpacing/2
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

void AdventWreathModule::drawCandle(int x, int y, uint16_t color, bool isLit, int candleIndex) {
    int candleWidth = 8;
    int candleHeight = 22;
    int candleTop = y - candleHeight;
    
    canvas.fillRect(x - candleWidth/2, candleTop, candleWidth, candleHeight, color);
    
    uint8_t r = ((color >> 11) & 0x1F) * 8;
    uint8_t g = ((color >> 5) & 0x3F) * 4;
    uint8_t b = (color & 0x1F) * 8;
    uint16_t darkColor = rgb565(r / 2, g / 2, b / 2);
    canvas.drawRect(x - candleWidth/2, candleTop, candleWidth, candleHeight, darkColor);
    
    int dochtHeight = 4;
    canvas.drawLine(x, candleTop - 1, x, candleTop - dochtHeight, rgb565(60, 60, 60));
    
    if (isLit) {
        drawFlame(x, candleTop - dochtHeight - 1, _flamePhase + candleIndex * 5);
    }
}

void AdventWreathModule::drawFlame(int x, int y, int phase) {
    // Mehr Zufälligkeit durch Kombination von Phase und Position
    uint32_t randSeed = simpleRandom(x * 127 + phase * 31);
    int flicker = ((phase / 3) % 5) - 2 + ((randSeed % 3) - 1);
    int heightVar = (phase % 6) + ((randSeed / 3) % 2);
    int widthVar = ((phase / 2) % 3) + ((randSeed / 7) % 2);
    
    int flameHeight = 8 + heightVar;
    
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
        canvas.drawLine(x - baseWidth + flickerOffset, y - i, 
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
            canvas.drawLine(x - width, y - i - 1, x + width, y - i - 1, rgb565(rI, gI, bI));
        } else {
            canvas.drawPixel(x, y - i - 1, rgb565(rI, gI, bI));
        }
    }
}

void AdventWreathModule::drawGreenery() {
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
    
    int baseY = 58; // Tiefer positioniert ohne Titel
    int centerX = canvas.width() / 2;
    
    // Kerzenpositionen für Kollisionsvermeidung
    int candleSpacing = 38;
    int candlePositions[4] = {
        centerX - candleSpacing - candleSpacing/2,
        centerX - candleSpacing/2,
        centerX + candleSpacing/2,
        centerX + candleSpacing + candleSpacing/2
    };
    
    // Mehr und dichteres Tannengrün
    for (int angle = 0; angle < 360; angle += 12) {
        float rad = angle * 3.14159f / 180.0f;
        int rx = 85;
        int ry = 10;
        
        int bx = centerX + (int)(rx * cos(rad));
        int by = baseY + (int)(ry * sin(rad));
        
        // Mehr Nadeln pro Position
        for (int n = 0; n < 6; n++) {
            int nx = bx + (n - 3) * 2;
            int nyOffset = ((angle + n * 17) % 5) - 2;
            int ny = by + nyOffset;
            
            if (ny >= 0 && ny < canvas.height()) {
                uint32_t seed = simpleRandom(angle * 13 + n * 7);
                uint16_t needleColor = greens[seed % numGreens];
                int lineOffset = ((angle + n * 23) % 4) - 2;
                int endY = ny - 3 - (seed % 2);
                if (endY >= 0) {
                    canvas.drawLine(nx, ny, nx + lineOffset, endY, needleColor);
                }
            }
        }
    }
    
    // Mehr Äste zwischen den Kerzen
    drawBranch(centerX - 70, baseY - 2, 1);
    drawBranch(centerX - 55, baseY - 3, 1);
    drawBranch(centerX - 30, baseY - 2, -1);
    drawBranch(centerX - 8, baseY - 1, -1);
    drawBranch(centerX + 8, baseY - 1, 1);
    drawBranch(centerX + 30, baseY - 2, 1);
    drawBranch(centerX + 55, baseY - 3, -1);
    drawBranch(centerX + 70, baseY - 2, -1);
}

void AdventWreathModule::drawBranch(int x, int y, int direction) {
    uint16_t greens[] = {
        rgb565(0, 90, 15),
        rgb565(0, 110, 25),
        rgb565(20, 130, 35),
        rgb565(0, 100, 20)
    };
    
    canvas.drawLine(x, y, x + direction * 7, y - 4, greens[0]);
    
    for (int i = 0; i < 5; i++) {
        int nx = x + direction * i;
        int ny = y - i / 2;
        
        if (ny >= 2 && ny < canvas.height()) {
            uint16_t color = greens[i % 4];
            canvas.drawLine(nx, ny, nx - direction * 2, ny - 3, color);
            canvas.drawLine(nx, ny, nx + direction * 2, ny - 3, color);
        }
    }
}

void AdventWreathModule::drawBerries() {
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
    
    int baseY = 58;
    int centerX = canvas.width() / 2;
    
    // Konfigurierbare Anzahl der Kugeln (4-20)
    int totalBerries = config ? config->adventWreathBerryCount : 12;
    if (totalBerries < 4) totalBerries = 4;
    if (totalBerries > 20) totalBerries = 20;
    
    // Kerzenpositionen für Kollisionsvermeidung
    int candleSpacing = 38;
    int candleX[4] = {
        centerX - candleSpacing - candleSpacing/2,
        centerX - candleSpacing/2,
        centerX + candleSpacing/2,
        centerX + candleSpacing + candleSpacing/2
    };
    int candleWidth = 8;
    int safeDistance = candleWidth / 2 + 6;
    
    // Hälfte Hintergrund-Kugeln, Hälfte Vordergrund-Kugeln
    int numBgBerries = totalBerries / 2;
    int numFgBerries = totalBerries - numBgBerries;
    
    // Sichere X-Positionen die keine Kerze treffen (zwischen und außerhalb der Kerzen)
    // Bereich links außen, zwischen Kerzen, rechts außen
    int safeXPositions[] = {
        centerX - 88, centerX - 80, centerX - 72,  // Links außen
        centerX - 50, centerX - 45,                // Zwischen Kerze 1 und 2
        centerX + 45, centerX + 50,                // Zwischen Kerze 3 und 4
        centerX + 72, centerX + 80, centerX + 88   // Rechts außen
    };
    int numSafePositions = 10;
    
    // Hintergrund-Kugeln (kleiner, höher = 3D-Effekt)
    for (int i = 0; i < numBgBerries; i++) {
        uint32_t seed = simpleRandom(i * 37 + 123);
        int posIdx = seed % numSafePositions;
        int bx = safeXPositions[posIdx] + ((seed / 7) % 5) - 2;
        int by = baseY - 6 - ((seed / 11) % 4);  // Höher positioniert
        int br = 1;  // Klein für Hintergrund
        
        // Prüfe Kollision mit Kerzen
        bool collision = false;
        for (int c = 0; c < 4; c++) {
            if (abs(bx - candleX[c]) < safeDistance) {
                collision = true;
                break;
            }
        }
        
        if (!collision && by >= 2 && by < canvas.height() - 2) {
            uint32_t colorSeed = simpleRandom(bx * 31 + by * 17 + i);
            uint16_t color = berryColors[colorSeed % numColors];
            // Gedimmt für Hintergrund-Effekt
            uint8_t r = ((color >> 11) & 0x1F) * 6;
            uint8_t g = ((color >> 5) & 0x3F) * 3;
            uint8_t b = (color & 0x1F) * 6;
            canvas.fillCircle(bx, by, br, rgb565(r, g, b));
        }
    }
    
    // Vordergrund-Kugeln (größer, tiefer)
    for (int i = 0; i < numFgBerries; i++) {
        uint32_t seed = simpleRandom(i * 47 + 456);
        int posIdx = seed % numSafePositions;
        int bx = safeXPositions[posIdx] + ((seed / 13) % 7) - 3;
        int by = baseY + 3 + ((seed / 17) % 4);  // Tiefer positioniert
        int br = 2 + ((seed / 23) % 2);  // Größer für Vordergrund (2-3)
        
        // Prüfe Kollision mit Kerzen
        bool collision = false;
        for (int c = 0; c < 4; c++) {
            if (abs(bx - candleX[c]) < safeDistance + br) {
                collision = true;
                break;
            }
        }
        
        if (!collision && by >= 2 && by < canvas.height() - 2) {
            uint32_t colorSeed = simpleRandom(bx * 47 + by * 23 + i);
            uint16_t color = berryColors[colorSeed % numColors];
            drawOrnament(bx, by, br, color);
        }
    }
}

unsigned long AdventWreathModule::getDisplayDuration() {
    return _displayDurationMs;
}

bool AdventWreathModule::isEnabled() {
    if (!config) return false;
    
    if (!config->adventWreathEnabled && !config->christmasTreeEnabled) {
        return false;
    }
    
    return isHolidaySeason();
}

void AdventWreathModule::resetPaging() {
    _isFinished = false;
}

void AdventWreathModule::onActivate() {
    _isFinished = false;
    _lastFlameUpdate = millis();
    _lastTreeLightUpdate = millis();
    _flamePhase = 0;
    _treeLightPhase = 0;
}

void AdventWreathModule::timeIsUp() {
    Log.println("[AdventWreath] Zeit abgelaufen");
    _isAdventViewActive = false;
    _lastAdventDisplayTime = millis();
}
