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
        return rgb565(255, 255, 255);  // Default: Weiß
    }
    long r = strtol(PsramString(hex + 1, 2).c_str(), NULL, 16);
    long g = strtol(PsramString(hex + 3, 2).c_str(), NULL, 16);
    long b = strtol(PsramString(hex + 5, 2).c_str(), NULL, 16);
    return rgb565(r, g, b);
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
        _flameAnimationMs = config->adventWreathFlameSpeedMs;
    }
}

void AdventWreathModule::onUpdate(std::function<void()> callback) {
    _updateCallback = callback;
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
        
        // Wähle Priority basierend auf Konfiguration
        Priority prio = config->adventWreathInterrupt ? Priority::Low : Priority::PlayNext;
        bool success = requestPriorityEx(prio, _currentAdventUID, safeDuration);
        
        if (success) {
            Log.printf("[AdventWreath] %d. Advent %s angefordert (UID=%lu, %lums)\n", 
                       currentAdvent, 
                       config->adventWreathInterrupt ? "Interrupt" : "PlayNext",
                       _currentAdventUID, _displayDurationMs);
        } else {
            Log.println("[AdventWreath] WARNUNG: Request wurde abgelehnt!");
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
    // Flammen-Animation aktualisieren - konfigurierbare Geschwindigkeit
    // tick() wird vom PanelManager so oft wie möglich aufgerufen
    unsigned long now = millis();
    if (now - _lastFlameUpdate > _flameAnimationMs) {
        _lastFlameUpdate = now;
        _flamePhase = (_flamePhase + 1) % 16;  // Mehr Phasen für mehr Variation
        
        // Redraw anfordern, damit die Animation sichtbar wird
        if (_updateCallback) {
            _updateCallback();
        }
    }
}

void AdventWreathModule::logicTick() {
    // Nicht verwendet, da Timing über periodicTick gehandhabt wird
}

void AdventWreathModule::draw() {
    canvas.fillScreen(0);
    u8g2.begin(canvas);
    
    int currentAdvent = calculateCurrentAdvent();
    
    // Canvas ist 192x66 Pixel - Titel oben
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.setForegroundColor(rgb565(255, 215, 0));  // Gold
    
    char title[32];
    snprintf(title, sizeof(title), "%d. Advent", currentAdvent);
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((canvas.width() - titleWidth) / 2, 11);
    u8g2.print(title);
    
    // Tannengrün zeichnen
    drawGreenery();
    
    // Adventskranz mit Kerzen zeichnen
    drawWreath();
    
    // Dekorative Beeren in verschiedenen Farben
    drawBerries();
}

void AdventWreathModule::drawWreath() {
    int currentAdvent = calculateCurrentAdvent();
    
    // Zentrum des Kranzes - Canvas ist 192x66
    int centerX = canvas.width() / 2;  // 96
    int baseY = 52;  // Angepasst für 66 Pixel Höhe
    
    // Kerzen-Positionen (gleichmäßig verteilt)
    int candleSpacing = 38;
    int candlePositions[4] = {
        centerX - candleSpacing - candleSpacing/2,
        centerX - candleSpacing/2,
        centerX + candleSpacing/2,
        centerX + candleSpacing + candleSpacing/2
    };
    
    // Kerzenfarben basierend auf Konfiguration
    uint16_t candleColors[4];
    
    if (config && config->adventWreathColorMode == 0) {
        // Traditionell: 3 violett, 1 rosa (3. Kerze = Gaudete)
        candleColors[0] = rgb565(128, 0, 128);   // Violett
        candleColors[1] = rgb565(128, 0, 128);   // Violett
        candleColors[2] = rgb565(255, 105, 180); // Rosa (Gaudete)
        candleColors[3] = rgb565(128, 0, 128);   // Violett
    } else if (config && config->adventWreathColorMode == 1) {
        // Bunt: rot, gold, grün, weiß
        candleColors[0] = rgb565(255, 0, 0);     // Rot
        candleColors[1] = rgb565(255, 215, 0);   // Gold
        candleColors[2] = rgb565(0, 128, 0);     // Grün
        candleColors[3] = rgb565(255, 255, 255); // Weiß
    } else if (config && config->adventWreathColorMode == 2) {
        // Eigene Farben aus Konfiguration parsen
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
        // Falls nicht alle 4 Farben definiert, verwende Defaults
        while (colorIdx < 4) {
            candleColors[colorIdx] = rgb565(255, 255, 255);
            colorIdx++;
        }
    } else {
        // Fallback: Bunt
        candleColors[0] = rgb565(255, 0, 0);
        candleColors[1] = rgb565(255, 215, 0);
        candleColors[2] = rgb565(0, 128, 0);
        candleColors[3] = rgb565(255, 255, 255);
    }
    
    // Zeichne alle 4 Kerzen
    for (int i = 0; i < 4; i++) {
        bool isLit = (i < currentAdvent);
        drawCandle(candlePositions[i], baseY, candleColors[i], isLit, i);
    }
}

void AdventWreathModule::drawCandle(int x, int y, uint16_t color, bool isLit, int candleIndex) {
    // Kerze (Rechteck) - angepasst für 66 Pixel Canvas-Höhe
    int candleWidth = 8;
    int candleHeight = 18;  // Kürzer für kleineres Canvas
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
    int dochtHeight = 3;
    canvas.drawLine(x, candleTop - 1, x, candleTop - dochtHeight, rgb565(60, 60, 60));
    
    // Flamme wenn angezündet
    if (isLit) {
        drawFlame(x, candleTop - dochtHeight - 1, _flamePhase + candleIndex * 3);
    }
}

void AdventWreathModule::drawFlame(int x, int y, int phase) {
    // Animierte Flamme mit mehr Variation für flüssigeres Flackern
    int flicker = ((phase / 2) % 3) - 1;  // -1, 0, oder 1
    int heightVar = (phase % 4);           // 0-3
    
    // Äußere Flamme (gelb/orange) - angepasste Höhe
    int flameHeight = 6 + heightVar;
    for (int i = 0; i < flameHeight; i++) {
        int width = (flameHeight - i) / 2;
        if (width < 1) width = 1;
        
        // Farbverlauf von gelb (unten) zu orange/rot (oben)
        uint8_t r = 255;
        uint8_t g = 200 - (i * 20);
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
    // Tannengrün rund um die Kerzen - angepasst für 192x66 Canvas
    uint16_t darkGreen = rgb565(0, 80, 0);
    uint16_t lightGreen = rgb565(0, 140, 40);
    
    int baseY = 54;  // Angepasst für 66 Pixel Höhe
    int centerX = canvas.width() / 2;
    
    // Basis-Kranz (Ellipse mit Nadeln) - kleinerer Radius
    for (int angle = 0; angle < 360; angle += 20) {
        float rad = angle * 3.14159f / 180.0f;
        int rx = 80;  // Radius X - angepasst
        int ry = 8;   // Radius Y (flacher Kranz) - kleiner
        
        int bx = centerX + (int)(rx * cos(rad));
        int by = baseY + (int)(ry * sin(rad));
        
        // Kleine Tannenzweige
        int direction = (angle < 180) ? 1 : -1;
        
        // Nadeln - deterministisch basierend auf Position
        for (int n = 0; n < 4; n++) {
            int nx = bx + (n - 2) * 2;
            int nyOffset = ((angle + n * 17) % 3) - 1;
            int ny = by + nyOffset;
            
            // Begrenze Y-Koordinate auf Canvas
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
    
    // Zusätzliche Tannenzweige zwischen den Kerzen
    drawBranch(centerX - 55, baseY - 3, 1);
    drawBranch(centerX - 18, baseY - 2, -1);
    drawBranch(centerX + 18, baseY - 2, 1);
    drawBranch(centerX + 55, baseY - 3, -1);
}

void AdventWreathModule::drawBranch(int x, int y, int direction) {
    uint16_t darkGreen = rgb565(0, 100, 20);
    uint16_t lightGreen = rgb565(34, 139, 34);
    
    // Hauptzweig - kürzer
    canvas.drawLine(x, y, x + direction * 6, y - 3, darkGreen);
    
    // Nadeln - weniger und kürzer
    for (int i = 0; i < 4; i++) {
        int nx = x + direction * i;
        int ny = y - i / 2;
        
        // Begrenze Y-Koordinate
        if (ny >= 2 && ny < canvas.height()) {
            uint16_t color = (i % 2 == 0) ? darkGreen : lightGreen;
            canvas.drawLine(nx, ny, nx - direction * 1, ny - 2, color);
            canvas.drawLine(nx, ny, nx + direction * 1, ny - 2, color);
        }
    }
}

void AdventWreathModule::drawBerries() {
    // Beeren in verschiedenen Farben als Dekoration
    // Verwende deterministischen "Zufall" basierend auf Position
    uint16_t berryColors[] = {
        rgb565(200, 0, 0),      // Rot
        rgb565(255, 215, 0),    // Gold
        rgb565(0, 100, 200),    // Blau
        rgb565(200, 0, 200),    // Magenta
        rgb565(255, 140, 0),    // Orange
        rgb565(0, 200, 100)     // Türkis
    };
    int numColors = sizeof(berryColors) / sizeof(berryColors[0]);
    
    int baseY = 54;  // Angepasst für 66 Pixel Canvas
    int centerX = canvas.width() / 2;
    
    // Beeren-Positionen - angepasst für kleineres Canvas
    int berryPositions[][2] = {
        {centerX - 70, baseY + 3},
        {centerX - 45, baseY + 5},
        {centerX + 45, baseY + 5},
        {centerX + 70, baseY + 3},
        {centerX - 25, baseY + 4},
        {centerX + 25, baseY + 4}
    };
    
    for (int i = 0; i < 6; i++) {
        int bx = berryPositions[i][0];
        int by = berryPositions[i][1];
        
        // Begrenze Y-Koordinate auf Canvas
        if (by >= 2 && by < canvas.height() - 2) {
            // Wähle Farbe basierend auf Position (deterministisch)
            uint16_t berryColor = berryColors[(bx + by) % numColors];
            
            // Beere (Kreis) - kleiner
            canvas.fillCircle(bx, by, 2, berryColor);
            
            // Glanzpunkt - heller
            uint8_t r = ((berryColor >> 11) & 0x1F) * 8;
            uint8_t g = ((berryColor >> 5) & 0x3F) * 4;
            uint8_t b = (berryColor & 0x1F) * 8;
            int rH = r + 100; if (rH > 255) rH = 255;
            int gH = g + 100; if (gH > 255) gH = 255;
            int bH = b + 100; if (bH > 255) bH = 255;
            uint16_t highlight = rgb565(rH, gH, bH);
            canvas.drawPixel(bx - 1, by - 1, highlight);
        }
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
