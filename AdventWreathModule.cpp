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
    int endDay = config ? config->adventWreathEndDay : 24;
    int year = tm_now.tm_year + 1900;
    
    time_t fourthAdvent = calculateFourthAdvent(year);
    struct tm tm_fourth;
    localtime_r(&fourthAdvent, &tm_fourth);
    
    struct tm tm_first = tm_fourth;
    tm_first.tm_mday -= 21;
    mktime(&tm_first);
    
    if (month == 11) {
        return (day >= tm_first.tm_mday && tm_first.tm_mon == 10);
    } else if (month == 12) {
        return (day <= endDay);
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
    int endDay = config ? config->christmasTreeEndDay : 31;
    int startDay = config ? config->christmasTreeStartDay : 1;
    
    if (month == 12) {
        return (day >= startDay && day <= endDay);
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
    int wreathEndDay = config ? config->adventWreathEndDay : 24;
    bool treeEnabled = config ? config->christmasTreeEnabled : true;
    bool wreathEnabled = config ? config->adventWreathEnabled : true;
    
    if (month == 12) {
        if (day > wreathEndDay) {
            return treeEnabled ? ChristmasDisplayMode::Tree : ChristmasDisplayMode::Wreath;
        }
        
        if (wreathEnabled && treeEnabled) {
            return ChristmasDisplayMode::Alternate;
        } else if (treeEnabled) {
            return ChristmasDisplayMode::Tree;
        }
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
    if (now - _lastFlameUpdate > _flameAnimationMs) {
        _lastFlameUpdate = now;
        _flamePhase = (_flamePhase + 1) % 32;
        _treeLightPhase = (_treeLightPhase + 1) % 24;
        
        if (_updateCallback) {
            _updateCallback();
        }
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
        int currentAdvent = calculateCurrentAdvent();
        
        u8g2.setFont(u8g2_font_helvB10_tf);
        u8g2.setForegroundColor(rgb565(255, 215, 0));
        
        char title[32];
        snprintf(title, sizeof(title), "%d. Advent", currentAdvent);
        int titleWidth = u8g2.getUTF8Width(title);
        u8g2.setCursor((canvas.width() - titleWidth) / 2, 11);
        u8g2.print(title);
        
        drawGreenery();
        drawWreath();
        drawBerries();
    }
}

void AdventWreathModule::drawChristmasTree() {
    int centerX = canvas.width() / 2;
    
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.setForegroundColor(rgb565(255, 215, 0));
    const char* title = "Frohe Weihnachten!";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((canvas.width() - titleWidth) / 2, 11);
    u8g2.print(title);
    
    uint16_t trunkColor = rgb565(139, 69, 19);
    canvas.fillRect(centerX - 4, 56, 8, 10, trunkColor);
    
    uint16_t treeGreen = rgb565(0, 100, 0);
    uint16_t lightGreen = rgb565(34, 139, 34);
    
    for (int y = 55; y >= 38; y--) {
        int width = (55 - y) * 2 + 4;
        uint16_t color = ((55 - y) % 2 == 0) ? treeGreen : lightGreen;
        canvas.drawFastHLine(centerX - width/2, y, width, color);
    }
    
    for (int y = 42; y >= 28; y--) {
        int width = (42 - y) * 2 + 4;
        uint16_t color = ((42 - y) % 2 == 0) ? treeGreen : lightGreen;
        canvas.drawFastHLine(centerX - width/2, y, width, color);
    }
    
    for (int y = 32; y >= 18; y--) {
        int width = (32 - y) * 2 + 2;
        uint16_t color = ((32 - y) % 2 == 0) ? treeGreen : lightGreen;
        canvas.drawFastHLine(centerX - width/2, y, width, color);
    }
    
    uint16_t starColor = rgb565(255, 255, 0);
    canvas.fillCircle(centerX, 16, 3, starColor);
    canvas.drawLine(centerX, 12, centerX, 20, starColor);
    canvas.drawLine(centerX - 4, 16, centerX + 4, 16, starColor);
    
    drawTreeLights();
}

void AdventWreathModule::drawTreeLights() {
    int centerX = canvas.width() / 2;
    
    uint16_t ornamentColors[] = {
        rgb565(255, 0, 0),
        rgb565(255, 215, 0),
        rgb565(0, 100, 200),
        rgb565(255, 0, 255),
        rgb565(0, 200, 100),
        rgb565(255, 140, 0)
    };
    int numColors = 6;
    
    int ornaments[][3] = {
        {centerX - 12, 45, 3},
        {centerX + 10, 48, 3},
        {centerX - 8, 35, 2},
        {centerX + 12, 38, 3},
        {centerX - 15, 52, 2},
        {centerX + 8, 28, 2},
        {centerX - 5, 42, 2},
        {centerX + 15, 45, 2},
        {centerX, 50, 3},
        {centerX - 10, 30, 2},
        {centerX + 5, 33, 2}
    };
    int numOrnaments = 11;
    
    for (int i = 0; i < numOrnaments; i++) {
        uint32_t seed = simpleRandom(i * 123 + 456);
        uint16_t color = ornamentColors[seed % numColors];
        drawOrnament(ornaments[i][0], ornaments[i][1], ornaments[i][2], color);
    }
    
    uint16_t lightColors[] = {
        rgb565(255, 255, 100),
        rgb565(255, 100, 100),
        rgb565(100, 255, 100),
        rgb565(100, 100, 255)
    };
    
    int lights[][2] = {
        {centerX - 18, 50},
        {centerX + 18, 50},
        {centerX - 14, 40},
        {centerX + 14, 40},
        {centerX - 10, 32},
        {centerX + 10, 32},
        {centerX - 6, 24},
        {centerX + 6, 24},
        {centerX - 20, 54},
        {centerX + 20, 54},
        {centerX, 22}
    };
    int numLights = 11;
    
    for (int i = 0; i < numLights; i++) {
        bool isOn = ((i + _treeLightPhase) % 4) < 2;
        if (isOn) {
            uint16_t color = lightColors[(i + _treeLightPhase / 3) % 4];
            canvas.fillCircle(lights[i][0], lights[i][1], 1, color);
            canvas.drawPixel(lights[i][0], lights[i][1] - 1, color);
        }
    }
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
    int baseY = 52;
    
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
    int flicker = ((phase / 3) % 5) - 2;
    int heightVar = (phase % 6);
    int widthVar = ((phase / 2) % 3);
    
    int flameHeight = 8 + heightVar;
    
    for (int i = 0; i < flameHeight; i++) {
        int baseWidth = (flameHeight - i) / 2 + widthVar;
        if (baseWidth < 1) baseWidth = 1;
        
        int colorPhase = (i + phase / 2) % 8;
        uint8_t r, g, b;
        
        if (colorPhase < 2) {
            r = 255; g = 255; b = 150 - i * 10;
        } else if (colorPhase < 4) {
            r = 255; g = 180 - i * 12; b = 0;
        } else if (colorPhase < 6) {
            r = 255; g = 120 - i * 8; b = 0;
        } else {
            r = 255; g = 220 - i * 15; b = 50;
        }
        
        if (g < 30) g = 30;
        if (b < 0) b = 0;
        
        int flickerOffset = (i < flameHeight / 2) ? 0 : flicker;
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
    uint16_t darkGreen = rgb565(0, 80, 0);
    uint16_t lightGreen = rgb565(0, 140, 40);
    
    int baseY = 54;
    int centerX = canvas.width() / 2;
    
    for (int angle = 0; angle < 360; angle += 20) {
        float rad = angle * 3.14159f / 180.0f;
        int rx = 80;
        int ry = 8;
        
        int bx = centerX + (int)(rx * cos(rad));
        int by = baseY + (int)(ry * sin(rad));
        
        for (int n = 0; n < 4; n++) {
            int nx = bx + (n - 2) * 2;
            int nyOffset = ((angle + n * 17) % 3) - 1;
            int ny = by + nyOffset;
            
            if (ny >= 0 && ny < canvas.height()) {
                uint16_t needleColor = (n % 2 == 0) ? darkGreen : lightGreen;
                int lineOffset = ((angle + n * 23) % 3) - 1;
                int endY = ny - 2;
                if (endY >= 0) {
                    canvas.drawLine(nx, ny, nx + lineOffset, endY, needleColor);
                }
            }
        }
    }
    
    drawBranch(centerX - 55, baseY - 3, 1);
    drawBranch(centerX - 18, baseY - 2, -1);
    drawBranch(centerX + 18, baseY - 2, 1);
    drawBranch(centerX + 55, baseY - 3, -1);
}

void AdventWreathModule::drawBranch(int x, int y, int direction) {
    uint16_t darkGreen = rgb565(0, 100, 20);
    uint16_t lightGreen = rgb565(34, 139, 34);
    
    canvas.drawLine(x, y, x + direction * 6, y - 3, darkGreen);
    
    for (int i = 0; i < 4; i++) {
        int nx = x + direction * i;
        int ny = y - i / 2;
        
        if (ny >= 2 && ny < canvas.height()) {
            uint16_t color = (i % 2 == 0) ? darkGreen : lightGreen;
            canvas.drawLine(nx, ny, nx - direction * 1, ny - 2, color);
            canvas.drawLine(nx, ny, nx + direction * 1, ny - 2, color);
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
    
    int baseY = 54;
    int centerX = canvas.width() / 2;
    
    int bgBerries[][3] = {
        {centerX - 80, baseY + 2, 1},
        {centerX - 60, baseY + 4, 1},
        {centerX - 35, baseY + 3, 1},
        {centerX + 35, baseY + 3, 1},
        {centerX + 60, baseY + 4, 1},
        {centerX + 80, baseY + 2, 1},
        {centerX - 55, baseY + 1, 1},
        {centerX + 55, baseY + 1, 1}
    };
    int numBgBerries = 8;
    
    for (int i = 0; i < numBgBerries; i++) {
        int bx = bgBerries[i][0];
        int by = bgBerries[i][1];
        int br = bgBerries[i][2];
        
        if (by >= 2 && by < canvas.height() - 2) {
            uint32_t seed = simpleRandom(bx * 31 + by * 17);
            uint16_t color = berryColors[seed % numColors];
            uint8_t r = ((color >> 11) & 0x1F) * 6;
            uint8_t g = ((color >> 5) & 0x3F) * 3;
            uint8_t b = (color & 0x1F) * 6;
            canvas.fillCircle(bx, by, br, rgb565(r, g, b));
        }
    }
    
    int fgBerries[][3] = {
        {centerX - 70, baseY + 3, 2},
        {centerX - 45, baseY + 5, 3},
        {centerX + 45, baseY + 5, 3},
        {centerX + 70, baseY + 3, 2},
        {centerX - 25, baseY + 4, 2},
        {centerX + 25, baseY + 4, 2},
        {centerX - 85, baseY + 4, 2},
        {centerX + 85, baseY + 4, 2},
        {centerX - 10, baseY + 6, 2},
        {centerX + 10, baseY + 6, 2}
    };
    int numFgBerries = 10;
    
    for (int i = 0; i < numFgBerries; i++) {
        int bx = fgBerries[i][0];
        int by = fgBerries[i][1];
        int br = fgBerries[i][2];
        
        if (by >= 2 && by < canvas.height() - 2) {
            uint32_t seed = simpleRandom(bx * 47 + by * 23);
            uint16_t color = berryColors[seed % numColors];
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
    _flamePhase = 0;
    _treeLightPhase = 0;
}

void AdventWreathModule::timeIsUp() {
    Log.println("[AdventWreath] Zeit abgelaufen");
    _isAdventViewActive = false;
    _lastAdventDisplayTime = millis();
}
