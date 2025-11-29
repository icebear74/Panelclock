#include "AdventWreathModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include <time.h>

// Hilfsfunktion für RGB565
uint16_t AdventWreathModule::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
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
    Log.println("[AdventWreath] Modul initialisiert");
}

void AdventWreathModule::setConfig() {
    if (config) {
        _displayDurationMs = config->adventWreathDisplaySec * 1000UL;
        _repeatIntervalMs = config->adventWreathRepeatMin * 60UL * 1000UL;
    }
}

int AdventWreathModule::calculateCurrentAdvent() {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int year = tm_now.tm_year + 1900;
    int month = tm_now.tm_mon + 1;  // 1-12
    int day = tm_now.tm_mday;
    
    // Advent ist nur im November/Dezember relevant
    if (month < 11 || month > 12) {
        return 0;
    }
    
    // Berechne den 4. Advent für dieses Jahr
    time_t fourthAdvent = calculateFourthAdvent(year);
    struct tm tm_fourth;
    localtime_r(&fourthAdvent, &tm_fourth);
    
    // 4. Advent ist der letzte Sonntag vor oder am 24.12.
    // Die anderen Advente sind 7, 14, 21 Tage davor
    
    // Berechne Differenz in Tagen zum 4. Advent
    struct tm tm_today = tm_now;
    tm_today.tm_hour = 12;
    tm_today.tm_min = 0;
    tm_today.tm_sec = 0;
    time_t today = mktime(&tm_today);
    
    int diffDays = (int)((fourthAdvent - today) / 86400);
    
    // 4. Advent (Tag des 4. Advents selbst)
    if (diffDays == 0) return 4;
    // 3. Advent (7 Tage vor 4. Advent)
    if (diffDays == 7) return 3;
    // 2. Advent (14 Tage vor 4. Advent)
    if (diffDays == 14) return 2;
    // 1. Advent (21 Tage vor 4. Advent)
    if (diffDays == 21) return 1;
    
    // Zwischen den Adventen: Zeige den bereits vergangenen an
    if (diffDays < 0) return 4;  // Nach 4. Advent bis Weihnachten
    if (diffDays < 7) return 3;   // Nach 3. Advent
    if (diffDays < 14) return 2;  // Nach 2. Advent
    if (diffDays < 21) return 1;  // Nach 1. Advent
    
    // Noch vor dem 1. Advent
    return 0;
}

time_t AdventWreathModule::calculateFourthAdvent(int year) {
    // 4. Advent ist der Sonntag vor oder am 24. Dezember
    struct tm tm_christmas;
    memset(&tm_christmas, 0, sizeof(tm_christmas));
    tm_christmas.tm_year = year - 1900;
    tm_christmas.tm_mon = 11;  // Dezember (0-based)
    tm_christmas.tm_mday = 24;
    tm_christmas.tm_hour = 12;
    mktime(&tm_christmas);  // Normalisiert und berechnet tm_wday
    
    // tm_wday: 0=Sonntag, 1=Montag, ..., 6=Samstag
    int daysToSubtract = tm_christmas.tm_wday;
    if (daysToSubtract == 0) {
        // 24.12. ist ein Sonntag -> das ist der 4. Advent
        daysToSubtract = 0;
    }
    
    tm_christmas.tm_mday -= daysToSubtract;
    return mktime(&tm_christmas);
}

void AdventWreathModule::periodicTick() {
    if (!config || !config->adventWreathEnabled) return;
    
    unsigned long now = millis();
    if (now - _lastPeriodicCheck < 1000) return;
    _lastPeriodicCheck = now;
    
    int currentAdvent = calculateCurrentAdvent();
    
    // Nur während der Adventszeit aktiv
    if (currentAdvent == 0) {
        if (_isAdventViewActive) {
            releasePriorityEx(_currentAdventUID);
            _isAdventViewActive = false;
            Log.println("[AdventWreath] Keine Adventszeit mehr, Interrupt freigegeben");
        }
        return;
    }
    
    // Prüfe ob wir eine neue Anzeige starten sollten
    unsigned long minInterval = (_lastAdventDisplayTime == 0) ? 0 : _repeatIntervalMs;
    
    if (!_isAdventViewActive && (now - _lastAdventDisplayTime > minInterval)) {
        _isAdventViewActive = true;
        _adventViewStartTime = now;
        _currentAdventUID = ADVENT_WREATH_UID_BASE + currentAdvent;
        
        unsigned long safeDuration = _displayDurationMs + 5000UL;
        bool success = requestPriorityEx(Priority::PlayNext, _currentAdventUID, safeDuration);
        
        if (success) {
            Log.printf("[AdventWreath] %d. Advent Interrupt angefordert (UID=%lu, %lums)\n", 
                       currentAdvent, _currentAdventUID, _displayDurationMs);
        } else {
            Log.println("[AdventWreath] WARNUNG: Interrupt wurde abgelehnt!");
            _isAdventViewActive = false;
        }
    }
    else if (_isAdventViewActive && (now - _adventViewStartTime > _displayDurationMs)) {
        releasePriorityEx(_currentAdventUID);
        _isAdventViewActive = false;
        _lastAdventDisplayTime = now;
        Log.printf("[AdventWreath] Anzeige beendet, nächste in %lu Minuten\n", _repeatIntervalMs / 60000);
    }
}

void AdventWreathModule::tick() {
    // Flammen-Animation aktualisieren (etwa alle 100ms)
    unsigned long now = millis();
    if (now - _lastFlameUpdate > 80) {
        _lastFlameUpdate = now;
        _flamePhase = (_flamePhase + 1) % 8;
    }
}

void AdventWreathModule::logicTick() {
    // Nicht verwendet, da Timing über periodicTick gehandhabt wird
}

void AdventWreathModule::draw() {
    canvas.fillScreen(0);
    u8g2.begin(canvas);
    
    int currentAdvent = calculateCurrentAdvent();
    
    // Titel zeichnen
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.setForegroundColor(rgb565(255, 215, 0));  // Gold
    
    char title[32];
    snprintf(title, sizeof(title), "%d. Advent", currentAdvent);
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((canvas.width() - titleWidth) / 2, 14);
    u8g2.print(title);
    
    // Tannengrün zeichnen
    drawGreenery();
    
    // Adventskranz mit Kerzen zeichnen
    drawWreath();
    
    // Dekorative Beeren
    drawBerries();
}

void AdventWreathModule::drawWreath() {
    int currentAdvent = calculateCurrentAdvent();
    
    // Zentrum des Kranzes
    int centerX = canvas.width() / 2;
    int baseY = 56;  // Basis der Kerzen
    
    // Kerzen-Positionen (gleichmäßig verteilt)
    int candleSpacing = 38;
    int candlePositions[4] = {
        centerX - candleSpacing - candleSpacing/2,
        centerX - candleSpacing/2,
        centerX + candleSpacing/2,
        centerX + candleSpacing + candleSpacing/2
    };
    
    // Kerzenfarben (traditionell: 3 violett, 1 rosa für den 3. Advent)
    // oder modern: verschiedene Farben
    uint16_t candleColors[4] = {
        rgb565(128, 0, 128),   // Violett - 1. Kerze
        rgb565(128, 0, 128),   // Violett - 2. Kerze
        rgb565(255, 105, 180), // Rosa - 3. Kerze (Gaudete)
        rgb565(128, 0, 128)    // Violett - 4. Kerze
    };
    
    // Alternativ: Verschiedenfarbige Kerzen (moderner Stil)
    // Kann über Konfiguration geändert werden
    if (config && config->adventWreathColorful) {
        candleColors[0] = rgb565(255, 0, 0);     // Rot
        candleColors[1] = rgb565(255, 215, 0);   // Gold
        candleColors[2] = rgb565(0, 128, 0);     // Grün
        candleColors[3] = rgb565(255, 255, 255); // Weiß
    }
    
    // Zeichne alle 4 Kerzen
    for (int i = 0; i < 4; i++) {
        bool isLit = (i < currentAdvent);
        drawCandle(candlePositions[i], baseY, candleColors[i], isLit, i);
    }
}

void AdventWreathModule::drawCandle(int x, int y, uint16_t color, bool isLit, int candleIndex) {
    // Kerze (Rechteck)
    int candleWidth = 10;
    int candleHeight = 24;
    int candleTop = y - candleHeight;
    
    // Kerze zeichnen
    canvas.fillRect(x - candleWidth/2, candleTop, candleWidth, candleHeight, color);
    
    // Kerzenrand (dunkler) - extrahiere RGB und verdunkle durch Division
    uint8_t r = ((color >> 11) & 0x1F) * 8;  // 5-bit to 8-bit
    uint8_t g = ((color >> 5) & 0x3F) * 4;   // 6-bit to 8-bit
    uint8_t b = (color & 0x1F) * 8;          // 5-bit to 8-bit
    uint16_t darkColor = rgb565(r / 2, g / 2, b / 2);  // Verdunkle um 50%
    canvas.drawRect(x - candleWidth/2, candleTop, candleWidth, candleHeight, darkColor);
    
    // Docht
    int dochtHeight = 4;
    canvas.drawLine(x, candleTop - 1, x, candleTop - dochtHeight, rgb565(60, 60, 60));
    
    // Flamme wenn angezündet
    if (isLit) {
        drawFlame(x, candleTop - dochtHeight - 1, _flamePhase + candleIndex * 2);
    }
}

void AdventWreathModule::drawFlame(int x, int y, int phase) {
    // Animierte Flamme mit zufälligem Flackern
    int flicker = (phase % 3) - 1;  // -1, 0, oder 1
    int heightVar = (phase % 4);     // 0-3
    
    // Äußere Flamme (gelb/orange)
    int flameHeight = 8 + heightVar;
    for (int i = 0; i < flameHeight; i++) {
        int width = (flameHeight - i) / 2;
        if (width < 1) width = 1;
        
        // Farbverlauf von gelb (unten) zu orange/rot (oben)
        uint8_t r = 255;
        uint8_t g = 200 - (i * 15);
        if (g < 50) g = 50;
        uint8_t b = 0;
        
        canvas.drawLine(x - width + flicker, y - i, x + width + flicker, y - i, rgb565(r, g, b));
    }
    
    // Innere Flamme (weiß/hellgelb) - Kern
    int innerHeight = flameHeight / 2;
    for (int i = 0; i < innerHeight; i++) {
        int width = (innerHeight - i) / 3;
        if (width < 1 && i < innerHeight - 1) width = 1;
        
        uint16_t innerColor = rgb565(255, 255, 200);  // Hellgelb
        if (i == 0) innerColor = rgb565(255, 255, 255);  // Weiß am Kern
        
        if (width >= 1) {
            canvas.drawLine(x - width, y - i - 1, x + width, y - i - 1, innerColor);
        } else {
            canvas.drawPixel(x, y - i - 1, innerColor);
        }
    }
}

void AdventWreathModule::drawGreenery() {
    // Tannengrün rund um die Kerzen
    uint16_t darkGreen = rgb565(0, 80, 0);
    uint16_t lightGreen = rgb565(0, 140, 40);
    
    int baseY = 58;
    int centerX = canvas.width() / 2;
    
    // Basis-Kranz (Ellipse mit Nadeln)
    for (int angle = 0; angle < 360; angle += 15) {
        float rad = angle * 3.14159f / 180.0f;
        int rx = 85;  // Radius X
        int ry = 12;  // Radius Y (flacher Kranz)
        
        int bx = centerX + (int)(rx * cos(rad));
        int by = baseY + (int)(ry * sin(rad));
        
        // Kleine Tannenzweige
        int direction = (angle < 180) ? 1 : -1;
        
        // Nadeln - deterministisch basierend auf Position statt rand()
        for (int n = 0; n < 5; n++) {
            int nx = bx + (n - 2) * 2;
            // Deterministisches "Zufalls"-Muster basierend auf Position
            int nyOffset = ((angle + n * 17) % 3) - 1;
            int ny = by + nyOffset;
            
            uint16_t needleColor = (n % 2 == 0) ? darkGreen : lightGreen;
            int lineOffset = ((angle + n * 23) % 3) - 1;
            canvas.drawLine(nx, ny, nx + lineOffset, ny - 3, needleColor);
        }
    }
    
    // Zusätzliche Tannenzweige zwischen den Kerzen
    drawBranch(centerX - 65, baseY - 5, 1);
    drawBranch(centerX - 20, baseY - 3, -1);
    drawBranch(centerX + 20, baseY - 3, 1);
    drawBranch(centerX + 65, baseY - 5, -1);
}

void AdventWreathModule::drawBranch(int x, int y, int direction) {
    uint16_t darkGreen = rgb565(0, 100, 20);
    uint16_t lightGreen = rgb565(34, 139, 34);
    
    // Hauptzweig
    canvas.drawLine(x, y, x + direction * 8, y - 4, darkGreen);
    
    // Nadeln
    for (int i = 0; i < 6; i++) {
        int nx = x + direction * i;
        int ny = y - i / 2;
        
        uint16_t color = (i % 2 == 0) ? darkGreen : lightGreen;
        canvas.drawLine(nx, ny, nx - direction * 2, ny - 3, color);
        canvas.drawLine(nx, ny, nx + direction * 2, ny - 3, color);
    }
}

void AdventWreathModule::drawBerries() {
    // Rote Beeren als Dekoration
    uint16_t berryRed = rgb565(200, 0, 0);
    uint16_t berryHighlight = rgb565(255, 100, 100);
    
    int baseY = 58;
    int centerX = canvas.width() / 2;
    
    // Beeren-Positionen
    int berryPositions[][2] = {
        {centerX - 75, baseY + 5},
        {centerX - 50, baseY + 8},
        {centerX + 50, baseY + 8},
        {centerX + 75, baseY + 5},
        {centerX - 30, baseY + 6},
        {centerX + 30, baseY + 6}
    };
    
    for (int i = 0; i < 6; i++) {
        int bx = berryPositions[i][0];
        int by = berryPositions[i][1];
        
        // Beere (Kreis)
        canvas.fillCircle(bx, by, 2, berryRed);
        // Glanzpunkt
        canvas.drawPixel(bx - 1, by - 1, berryHighlight);
    }
}

unsigned long AdventWreathModule::getDisplayDuration() {
    return _displayDurationMs;
}

bool AdventWreathModule::isEnabled() {
    if (!config || !config->adventWreathEnabled) {
        return false;
    }
    return calculateCurrentAdvent() > 0;
}

void AdventWreathModule::resetPaging() {
    _isFinished = false;
}

void AdventWreathModule::onActivate() {
    _isFinished = false;
    _lastFlameUpdate = millis();
    _flamePhase = 0;
}

void AdventWreathModule::timeIsUp() {
    Log.println("[AdventWreath] Zeit abgelaufen");
    _isAdventViewActive = false;
    _lastAdventDisplayTime = millis();
}
