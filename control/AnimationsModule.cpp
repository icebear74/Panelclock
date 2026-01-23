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

// Hilfsfunktion zum Mischen zweier RGB565-Farben
uint16_t AnimationsModule::blendRGB565(uint16_t color1, uint16_t color2, int blend) {
    // Extract RGB components from RGB565
    uint8_t r1 = (color1 >> 11) & 0x1F;
    uint8_t g1 = (color1 >> 5) & 0x3F;
    uint8_t b1 = color1 & 0x1F;
    
    uint8_t r2 = (color2 >> 11) & 0x1F;
    uint8_t g2 = (color2 >> 5) & 0x3F;
    uint8_t b2 = color2 & 0x1F;
    
    // Linear interpolation (blend is 0-15)
    uint8_t r = r1 + ((r2 - r1) * blend) / 16;
    uint8_t g = g1 + ((g2 - g1) * blend) / 16;
    uint8_t b = b1 + ((b2 - b1) * blend) / 16;
    
    // Repack into RGB565
    return (r << 11) | (g << 5) | b;
}

// Einfacher Pseudo-Zufallsgenerator
uint32_t AnimationsModule::simpleRandom(uint32_t seed) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

AnimationsModule::AnimationsModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, 
                                       GeneralTimeConverter& timeConverter, DeviceConfig* config)
    : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), config(config) {
    // Allokiere Arrays in PSRAM
    _snowflakes = (Snowflake*)ps_malloc(sizeof(Snowflake) * MAX_SNOWFLAKES);
    _flowers = (Flower*)ps_malloc(sizeof(Flower) * MAX_FLOWERS);
    _butterflies = (Butterfly*)ps_malloc(sizeof(Butterfly) * MAX_BUTTERFLIES);
    _leaves = (Leaf*)ps_malloc(sizeof(Leaf) * MAX_LEAVES);
    _birds = (Bird*)ps_malloc(sizeof(Bird) * MAX_BIRDS);
    
    // Log allocation status
    if (!_snowflakes || !_flowers || !_butterflies || !_leaves || !_birds) {
        Log.println("[AnimationsModule] WARNUNG: PSRAM-Allokation fehlgeschlagen!");
        if (!_snowflakes) Log.println("  - _snowflakes Allokation fehlgeschlagen");
        if (!_flowers) Log.println("  - _flowers Allokation fehlgeschlagen");
        if (!_butterflies) Log.println("  - _butterflies Allokation fehlgeschlagen");
        if (!_leaves) Log.println("  - _leaves Allokation fehlgeschlagen");
        if (!_birds) Log.println("  - _birds Allokation fehlgeschlagen");
    }
    
    // Initialisiere mit 0
    if (_snowflakes) memset(_snowflakes, 0, sizeof(Snowflake) * MAX_SNOWFLAKES);
    if (_flowers) memset(_flowers, 0, sizeof(Flower) * MAX_FLOWERS);
    if (_butterflies) memset(_butterflies, 0, sizeof(Butterfly) * MAX_BUTTERFLIES);
    if (_leaves) memset(_leaves, 0, sizeof(Leaf) * MAX_LEAVES);
    if (_birds) memset(_birds, 0, sizeof(Bird) * MAX_BIRDS);
}

AnimationsModule::~AnimationsModule() {
    // Gebe PSRAM-Speicher frei
    if (_snowflakes) { free(_snowflakes); _snowflakes = nullptr; }
    if (_flowers) { free(_flowers); _flowers = nullptr; }
    if (_butterflies) { free(_butterflies); _butterflies = nullptr; }
    if (_leaves) { free(_leaves); _leaves = nullptr; }
    if (_birds) { free(_birds); _birds = nullptr; }
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

void AnimationsModule::shuffleTreeElements() {
    time_t now_utc;
    time(&now_utc);
    uint32_t seed = (uint32_t)now_utc + millis();
    
    int canvasH = _currentCanvas->height();
    int canvasW = _currentCanvas->width();
    int centerX = canvasW / 2;
    float scale = canvasH / 66.0f;
    
    int baseY = canvasH - 4;
    int trunkHeight = (int)(10 * scale);
    int treeHeight = (int)(54 * scale);
    int treeBaseY = baseY - trunkHeight + 2;
    
    // Tree layer dimensions (used by both ornaments and lights)
    int layer1Height = (int)(18 * scale);
    int layer2Height = (int)(18 * scale);
    int layer3Height = (int)(18 * scale);
    int layer1Width = (int)(28 * scale);
    int layer2Width = (int)(22 * scale);
    int layer3Width = (int)(16 * scale);
    
    // Hole konfigurierte Anzahl oder verwende Standard
    int ornamentCount = config ? config->christmasTreeOrnamentCount : 12;
    if (ornamentCount < 8) ornamentCount = 8;
    if (ornamentCount > 20) ornamentCount = 20;
    if (scale > 1.2) ornamentCount += 4;  // Mehr bei Fullscreen
    
    // Shuffle ornament positions
    for (int i = 0; i < ornamentCount && i < 30; i++) {
        seed = simpleRandom(seed + i * 137);
        
        // Y position (height on tree)
        float t = (float)i / ornamentCount;
        float yFraction = 0.4f * t + 0.6f * (t * t);
        int yOffset = 3 + (int)(yFraction * (treeHeight - 10));
        yOffset += (seed % 8) - 4;
        if (yOffset < 4) yOffset = 4;
        if (yOffset > treeHeight - 6) yOffset = treeHeight - 6;
        _shuffledOrnamentY[i] = yOffset;
        
        // X position (left/right) - constrain within tree boundaries
        int maxWidth;
        int ornamentRadius = 2 + (seed / 17) % 3;  // Radius for boundary calculation
        
        if (yOffset < layer1Height) {
            float progress = (float)yOffset / layer1Height;
            maxWidth = (int)(layer1Width * (1.0f - progress * 0.5f) * 0.85f);
        } else if (yOffset < layer1Height + layer2Height - (int)(4 * scale)) {
            float progress = (float)(yOffset - layer1Height + (int)(4 * scale)) / layer2Height;
            maxWidth = (int)(layer2Width * (1.0f - progress * 0.5f) * 0.85f);
        } else {
            float progress = (float)(yOffset - layer1Height - layer2Height + (int)(14 * scale)) / (layer3Width + 4);
            maxWidth = (int)(layer3Width * (1.0f - progress * 0.6f) * 0.8f);
        }
        
        // Reduce maxWidth by ornament radius to keep ornament fully inside tree
        maxWidth -= ornamentRadius;
        if (maxWidth < 2) maxWidth = 2;
        int xRange = maxWidth * 2;
        int xPos = seed % xRange;
        if (i % 2 == 0) {
            _shuffledOrnamentX[i] = -maxWidth + (xPos / 2);
        } else {
            _shuffledOrnamentX[i] = -maxWidth + xRange/2 + (xPos / 2);
        }
        
        // Random color index (0-7)
        _shuffledOrnamentColors[i] = seed % 8;
        
        // Random size - same as calculated radius above
        _shuffledOrnamentSizes[i] = ornamentRadius;
    }
    
    // Shuffle light positions
    int lightCount = config ? config->christmasTreeLightCount : 18;
    if (lightCount < 5) lightCount = 5;
    if (lightCount > 30) lightCount = 30;
    
    for (int i = 0; i < lightCount && i < 30; i++) {
        seed = simpleRandom(seed + i * 211);
        
        // Random Y position throughout tree
        int lightY = (seed % treeHeight);
        if (lightY < 4) lightY = 4;
        _shuffledLightY[i] = lightY;
        
        // Calculate proper max width based on tree layer (matching drawNaturalTree)
        int maxLightWidth;
        int layer2Top = layer1Height + layer2Height - (int)(4 * scale);
        
        if (lightY < layer1Height) {
            // Layer 1 (bottom): tapers with 0.8 factor
            float progress = (float)lightY / layer1Height;
            maxLightWidth = (int)(layer1Width - progress * layer1Width * 0.8f);
        } else if (lightY < layer2Top) {
            // Layer 2 (middle): tapers with 0.9 factor
            float progress = (float)(lightY - layer1Height) / layer2Height;
            maxLightWidth = (int)(layer2Width - progress * layer2Width * 0.9f);
        } else {
            // Layer 3 (top): tapers with 0.85 factor
            float progress = (float)(lightY - layer2Top) / layer3Height;
            maxLightWidth = (int)(layer3Width - progress * layer3Width * 0.85f);
        }
        
        // Reduce width by light size (2px) to keep light center inside tree
        maxLightWidth -= 2;
        if (maxLightWidth < 2) maxLightWidth = 2;
        
        int lightX = ((seed / 7) % (maxLightWidth * 2)) - maxLightWidth;
        _shuffledLightX[i] = lightX;
    }
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
    if (!config || !config->fireplaceEnabled) return false;
    
    // Kamin ist nur im Winter "in Season" (Dezember, Januar, Februar)
    time_t now_utc;
    time(&now_utc);
    time_t local_now = timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    int month = tm_now.tm_mon + 1; // 1-12
    
    // Kamin nur in Wintermonaten aktiv
    return (month == 12 || month == 1 || month == 2);
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
    
    bool hasHolidayAnimations = config->adventWreathEnabled || config->christmasTreeEnabled || config->fireplaceEnabled;
    bool hasSeasonalAnimations = config->seasonalAnimationsEnabled;
    
    if (!hasHolidayAnimations && !hasSeasonalAnimations) return;
    
    unsigned long now = millis();
    if (now - _lastPeriodicCheck < 1000) return;
    _lastPeriodicCheck = now;
    
    bool inHolidaySeason = isHolidaySeason();
    
    // Wenn ein Request noch aussteht (noch nicht aktiviert), nichts tun
    if (_requestPending) {
        return;
    }
    
    // Use appropriate interval for seasonal vs holiday animations
    unsigned long repeatInterval = _repeatIntervalMs;  // Standard interval for all animations
    unsigned long displayDuration = _displayDurationMs;
    
    unsigned long minInterval = (_lastAdventDisplayTime == 0) ? 0 : repeatInterval;
    
    if (!_isAdventViewActive && (now - _lastAdventDisplayTime > minInterval)) {
        // Berechne welche Animationen grundsätzlich aktiv sind
        bool wreathActive = config->adventWreathEnabled && isAdventSeason();
        bool treeActive = config->christmasTreeEnabled && isChristmasSeason();
        
        // Kamin ist IMMER eine separate Animation (nicht nur in Holiday-Season)
        bool fireplaceActive = config->fireplaceEnabled && isFireplaceSeason();
        
        // Prüfe Nachtmodus für Kamin
        if (fireplaceActive && config->fireplaceNightModeOnly && !TimeUtilities::isNightTime()) {
            fireplaceActive = false;
        }
        
        // Seasonal animation ist IMMER aktiv wenn enabled (unabhängig von Holiday-Season)
        bool seasonalActive = hasSeasonalAnimations;
        if (seasonalActive && inHolidaySeason) {
            // Im Winter: Check ob Winter-Seasonal bei Advent/Weihnachten erlaubt ist
            TimeUtilities::Season currentSeason = config->seasonalAnimationsTestMode ? 
                (TimeUtilities::Season)_testModeSeasonIndex : TimeUtilities::getCurrentSeason();
            
            if (currentSeason == TimeUtilities::Season::WINTER && !config->seasonalWinterWithHolidays) {
                seasonalActive = false;
            }
        }
        
        // Prüfe ob überhaupt etwas anzuzeigen ist
        bool hasAnythingToShow = wreathActive || treeActive || fireplaceActive || seasonalActive;
        
        if (!hasAnythingToShow) {
            Log.println("[AnimationsModule] Nichts anzuzeigen - kein Request");
            return;
        }
        
        // Entscheide WAS angezeigt wird
        shuffleCandleOrder();
        ChristmasDisplayMode mode = getCurrentDisplayMode();
        
        // Setze Anzeige-Flags
        _showTree = false;
        _showFireplace = false;
        _showSeasonalAnimation = false;
        
        if (mode == ChristmasDisplayMode::Alternate) {
            // Rotiere durch ALLE aktiven Animationen
            // Im Winter z.B.: Adventskranz > Weihnachtsbaum > Winteranimation > Kamin
            // Im Frühling z.B.: Frühlingsanimation > Kamin (wenn Kamin abends aktiv)
            int activeCount = (wreathActive ? 1 : 0) + (treeActive ? 1 : 0) + 
                            (fireplaceActive ? 1 : 0) + (seasonalActive ? 1 : 0);
            
            if (activeCount > 0) {
                int modeIndex = _displayCounter % activeCount;
                
                int current = 0;
                if (wreathActive) {
                    if (current == modeIndex) { /* Kranz - default */ }
                    current++;
                }
                if (treeActive && current <= modeIndex) {
                    if (current == modeIndex) { _showTree = true; }
                    current++;
                }
                if (seasonalActive && current <= modeIndex) {
                    if (current == modeIndex) { 
                        _showSeasonalAnimation = true; 
                        _showTree = false; 
                    }
                    current++;
                }
                if (fireplaceActive && current <= modeIndex) {
                    if (current == modeIndex) { 
                        _showFireplace = true; 
                        _showTree = false; 
                        _showSeasonalAnimation = false;
                    }
                }
            }
        } else if (mode == ChristmasDisplayMode::Tree) {
            _showTree = treeActive;
        } else if (mode == ChristmasDisplayMode::Fireplace) {
            _showFireplace = fireplaceActive;
        } else if (mode == ChristmasDisplayMode::Wreath) {
            // Kranz-Modus: alle Flags bleiben false (Kranz ist der Default)
        }
        
        // Alle Animationen verwenden die gleiche Display-Duration aus den globalen Einstellungen
        // (seasonalAnimationsDisplaySec wird nicht mehr verwendet)
        
        // Feste UID für diese Anzeige-Session (nicht vom Advent-Woche abhängig)
        // Verwende eine einfache UID basierend auf dem Display-Counter
        _currentAdventUID = ADVENT_WREATH_UID_BASE + (_displayCounter % 100);
        
        unsigned long safeDuration = displayDuration + 5000UL;
        Priority prio = config->adventWreathInterrupt ? Priority::Low : Priority::PlayNext;
        
        _requestPending = true;  // Markiere Request als ausstehend
        bool success = requestPriorityEx(prio, _currentAdventUID, safeDuration);
        
        if (success) {
            const char* modeName;
            if (_showSeasonalAnimation) {
                modeName = TimeUtilities::getSeasonName(
                    config->seasonalAnimationsTestMode ? 
                    (TimeUtilities::Season)_testModeSeasonIndex : 
                    TimeUtilities::getCurrentSeason()
                );
            } else if (_showFireplace) {
                modeName = "Kamin";
            } else if (_showTree) {
                modeName = "Weihnachtsbaum";
            } else {
                modeName = "Adventskranz";
            }
            
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
    else if (_isAdventViewActive && (now - _adventViewStartTime > displayDuration)) {
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
    
    // LED-Rahmen-Animation - simple phase rotation (no blending to avoid pulsing effect)
    unsigned long ledBorderSpeed = config ? (unsigned long)config->ledBorderSpeedMs : 100;
    if (now - _lastLedBorderUpdate > ledBorderSpeed) {
        _lastLedBorderUpdate = now;
        _ledBorderPhase = (_ledBorderPhase + 1) % 4;  // Simple 4-phase rotation
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
    
    // Seasonal animation phase (for general seasonal animation counter)
    if (now - _lastSeasonAnimUpdate > 50) {
        _lastSeasonAnimUpdate = now;
        _seasonAnimationPhase = (_seasonAnimationPhase + 1) % 360;
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
    } else if (!_showFireplace && !_showTree && !_showSeasonalAnimation && config && config->adventWreathBgColor.length() > 0) {
        bgColor = hexToRgb565(config->adventWreathBgColor.c_str());
    }
    
    _currentCanvas->fillScreen(bgColor);
    u8g2.begin(*_currentCanvas);
    
    // Show EITHER holiday animations OR seasonal animations, NEVER both
    if (_showSeasonalAnimation) {
        // Show seasonal animation ONLY (not mixed with holiday animations)
        drawSeasonalAnimations();
    } else {
        // Show holiday animations (Adventskranz, Weihnachtsbaum, or Kamin)
        if (_showFireplace) {
            drawFireplace();
        } else if (_showTree) {
            // Mische Kugeln und Lichter nur EINMAL bei neuer Anzeige, nicht bei jedem Tick
            unsigned long now = millis();
            if (_lastTreeDisplay == 0 || (now - _lastTreeDisplay) > 60000) {  // Neue Anzeige oder >60s her
                shuffleTreeElements();
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
    
    // Hole konfigurierte Anzahl oder verwende Standard
    int numOrnaments = config ? config->christmasTreeOrnamentCount : 12;
    if (numOrnaments < 8) numOrnaments = 8;
    if (numOrnaments > 20) numOrnaments = 20;
    if (scale > 1.2) numOrnaments += 4;  // Mehr bei Fullscreen
    
    // Verwende gemischte Positionen aus shuffleTreeElements()
    for (int i = 0; i < numOrnaments && i < 30; i++) {
        int ox = centerX + _shuffledOrnamentX[i];
        int oy = baseY - _shuffledOrnamentY[i];
        int radius = _shuffledOrnamentSizes[i];
        uint16_t color = ornamentColors[_shuffledOrnamentColors[i] % 8];
        
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
    
    // Verwende gemischte Positionen aus shuffleTreeElements()
    
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
    
    // Verwende gemischte Positionen aus shuffleTreeElements()
    // Lichter über den Baum verteilt
    for (int i = 0; i < lightCount && i < 30; i++) {
        int lightY = treeBaseY - _shuffledLightY[i];
        int lightX = centerX + _shuffledLightX[i];
        
        // Blinken basierend auf Phase
        uint32_t seed = simpleRandom(i * 11 + _shuffledLightX[i]);
        bool isOn = ((i + _treeLightPhase + (seed % 3)) % 4) < 2;
        
        if (isOn) {
            uint16_t color;
            if (lightMode == 1) {
                // Feste Farbe
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
    if (!_snowflakes) {
        if (!_snowflakesInitialized) {
            Log.println("[AnimationsModule] Kann Schneeflocken nicht initialisieren - PSRAM-Allokation fehlgeschlagen");
            _snowflakesInitialized = true; // Prevent repeated logging
        }
        return;
    }
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Get count from config
    int snowflakeCount = config ? config->seasonalWinterSnowflakeCount : 20;
    
    // Initialize snowflakes on first call
    if (!_snowflakesInitialized) {
        for (int i = 0; i < snowflakeCount && i < MAX_SNOWFLAKES; i++) {
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
        for (int i = 0; i < snowflakeCount && i < MAX_SNOWFLAKES; i++) {
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
    for (int i = 0; i < snowflakeCount && i < MAX_SNOWFLAKES; i++) {
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
    // Return appropriate duration based on whether we're in holiday season or showing seasonal animations
    if (config && config->seasonalAnimationsEnabled && !isHolidaySeason()) {
        return config->seasonalAnimationsDisplaySec * 1000UL;
    }
    return _displayDurationMs;
}

bool AnimationsModule::isEnabled() {
    if (!config) return false;
    
    // Check if any animation is enabled
    bool hasHolidayAnimations = config->adventWreathEnabled || config->christmasTreeEnabled || config->fireplaceEnabled;
    bool hasSeasonalAnimations = config->seasonalAnimationsEnabled;
    
    if (!hasHolidayAnimations && !hasSeasonalAnimations) {
        return false;
    }
    
    // Enable if in holiday season OR if seasonal animations are enabled
    return isHolidaySeason() || hasSeasonalAnimations;
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
    
    bool inHolidaySeason = isHolidaySeason();
    if (!inHolidaySeason && config && config->seasonalAnimationsEnabled) {
        const char* seasonName = TimeUtilities::getSeasonName(TimeUtilities::getCurrentSeason());
        Log.printf("[AnimationsModule] Aktiviert: %s Animation (UID=%lu)\n", seasonName, _currentAdventUID);
    } else {
        const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
        Log.printf("[AnimationsModule] Aktiviert: %s (UID=%lu)\n", modeName, _currentAdventUID);
    }
}

void AnimationsModule::timeIsUp() {
    bool inHolidaySeason = isHolidaySeason();
    if (!inHolidaySeason && config && config->seasonalAnimationsEnabled) {
        const char* seasonName = TimeUtilities::getSeasonName(TimeUtilities::getCurrentSeason());
        Log.printf("[AnimationsModule] Zeit abgelaufen für %s Animation (UID=%lu)\n", seasonName, _currentAdventUID);
    } else {
        const char* modeName = _showFireplace ? "Kamin" : (_showTree ? "Weihnachtsbaum" : "Adventskranz");
        Log.printf("[AnimationsModule] Zeit abgelaufen für %s (UID=%lu)\n", modeName, _currentAdventUID);
    }
    _isAdventViewActive = false;
    _requestPending = false;
    _lastAdventDisplayTime = millis();
}

// ============== KAMIN ZEICHENFUNKTIONEN ==============

#include "fireplace_bitmap.h"

void AnimationsModule::drawFireplace() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Calculate scaling factors
    // Base bitmap is 192x64, we want to scale it to fit the canvas
    float scaleX = canvasW / (float)FIREPLACE_BMP_WIDTH;
    float scaleY = canvasH / 96.0f;  // Target height is 96 (64 for fireplace + 32 for decorations)
    
    // Calculate the fireplace area (bottom 64 pixels scaled)
    int fireplaceHeight = (int)(FIREPLACE_BMP_HEIGHT * scaleY);
    int fireplaceY = canvasH - fireplaceHeight;
    
    // Draw the fireplace bitmap
    // We'll scale it by drawing each source pixel as a scaled rectangle
    for (int srcY = 0; srcY < FIREPLACE_BMP_HEIGHT; srcY++) {
        int destY = fireplaceY + (int)(srcY * scaleY);
        int destHeight = max(1, (int)((srcY + 1) * scaleY) - (int)(srcY * scaleY));
        
        for (int srcX = 0; srcX < FIREPLACE_BMP_WIDTH; srcX++) {
            int destX = (int)(srcX * scaleX);
            int destWidth = max(1, (int)((srcX + 1) * scaleX) - (int)(srcX * scaleX));
            
            // Read pixel from PROGMEM
            int pixelIndex = srcY * FIREPLACE_BMP_WIDTH + srcX;
            uint16_t color = pgm_read_word(&FIREPLACE_BITMAP_192x64[pixelIndex]);
            
            // Draw scaled pixel
            if (destWidth == 1 && destHeight == 1) {
                _currentCanvas->drawPixel(destX, destY, color);
            } else {
                _currentCanvas->fillRect(destX, destY, destWidth, destHeight, color);
            }
        }
    }
    
    // === FIRE ANIMATION ===
    // The black fireplace opening (Brennraum) is 65x33 pixels
    // Located at approximately x: 64-128 (center ~96), y: 24-56 in source image
    // User adjustments: +5 pixels wider to the right, +3 pixels to the right, +2 pixels to the right
    int fireSourceX = 69;  // Left edge moved 5 pixels right total (was 64, now 64+3+2=69)
    int fireSourceY = 24;  // Top edge of black opening in source
    int fireSourceW = 70;  // Width of opening (65 + 5 pixels wider to the right)
    int fireSourceH = 33;  // Height of opening
    
    // Scale to canvas coordinates
    int fireLeftX = (int)(fireSourceX * scaleX);  // Left edge X
    int fireY = fireplaceY + (int)((fireSourceY + fireSourceH) * scaleY) - 2;  // Bottom Y
    int fireW = (int)(fireSourceW * scaleX) - 8;  // Width (scaled + extended)
    int fireH = (int)(fireSourceH * scaleY) - 4;  // Height (slightly smaller for margins)
    int fireX = fireLeftX + fireW / 2;  // Center X based on new width
    
    drawFireplaceFlames(fireX, fireY, fireW, fireH);
    
    // === MANTEL DECORATIONS ===
    // The brown mantel shelf (Sims) dimensions from PROGMEM array analysis:
    // - Y position: 9-10 (light brown top edge)
    // - X range: 43 to 158 (116 pixels wide)
    // - Center: X=101 (52.6% from left edge)
    
    // Decorations should be placed ON TOP of this brown shelf
    // User adjustments: initially 8 pixels higher, now +3 more = 11 pixels higher total
    int mantelSourceY = 1;  // 11 pixels higher (was 12, now 12-11=1)
    int mantelY = fireplaceY + (int)(mantelSourceY * scaleY);
    
    // Use precise mantel dimensions from PROGMEM analysis
    // Mantel shelf: X=43 to X=158 (116 pixels wide), center at X=101
    // Extended mantel dimensions: +10px left and +10px right from original
    int mantelLeftEdge = (int)(33 * scaleX);   // Was 43, now 33 (-10px)
    int mantelRightEdge = (int)(168 * scaleX);  // Was 158, now 168 (+10px)
    int mantelWidth = mantelRightEdge - mantelLeftEdge;  // 136-pixel width scaled (was 116)
    float mantelCenterRatio = 101.0f / 192.0f;  // 52.6% from left edge (clock position unchanged)
    int mantelCenterX = (int)(canvasW * mantelCenterRatio);
    
    // Draw decorations ON the mantel
    drawMantleDecorations(mantelY, mantelWidth, mantelCenterX, scaleY);
    
    // Draw stockings hanging FROM the front edge of the mantel
    // User adjustments: initially 8 pixels higher, +7 more, +2 more = 17 pixels higher total
    int stockingHangY = fireplaceY + (int)(-1 * scaleY);  // 17 pixels higher (was 16, now 16-17=-1)
    drawStockings(stockingHangY, mantelWidth, mantelCenterX, mantelLeftEdge);
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
    
    // ===== HAUPTFEUER - Based on Advent wreath candle flame algorithm =====
    // Uses same proven algorithm as drawCandleFlame(), scaled for fireplace
    // ONE cohesive fire mass, not separate columns
    
    for (int fy = 0; fy < height; fy++) {
        float yProgress = (float)fy / height;  // 0 = bottom, 1 = top
        
        // Quadratic width tapering (SAME as candle flames)
        // Creates natural curved flame shape, not pyramid
        float widthFactor = (1.0f - yProgress * yProgress) * 0.9f + 0.1f;
        int lineWidth = (int)(width / 2 * widthFactor);
        
        // Gentle wave motion (similar to candles, scaled for size)
        int waveOffset = (int)(sin((fy + _fireplaceFlamePhase * 2) * 0.5f) * 3);
        
        for (int fx = -lineWidth; fx <= lineWidth; fx++) {
            uint32_t seed = simpleRandom(fx * 23 + fy * 47 + _fireplaceFlamePhase * 5);
            
            // Smooth density gradient (SAME as candles)
            float density = 1.0f - yProgress * 0.6f;  // 100% -> 40%
            if ((seed % 100) > density * 100) continue;
            
            // Minimal flicker (like candles)
            int flickerX = ((seed / 7) % 3) - 1;
            int px = x + fx + waveOffset + flickerX;
            int py = y - fy;
            
            // Color: bright core at center/bottom, darker at edges/top
            float distFromCenter = abs(fx) / (float)(lineWidth + 1);
            int baseColorIdx = (int)(yProgress * 4);  // Darker toward top
            int colorIdx = baseColorIdx + (int)(distFromCenter * 2);
            if (colorIdx > 5) colorIdx = 5;
            if (colorIdx < 0) colorIdx = 0;
            
            // Bright core in middle (bottom third only)
            if (abs(fx) < 3 && fy < height / 3) {
                colorIdx = max(0, colorIdx - 2);
            }
            
            if (px >= x - width/2 && px <= x + width/2 && py >= 0 && py < _currentCanvas->height()) {
                _currentCanvas->drawPixel(px, py, flameColors[colorIdx]);
            }
        }
    }
    
    // ===== FLAME TIPS (connected to main body) =====
    int numTips = 4 + (_fireplaceFlamePhase % 2);  // 4-5 tips
    for (int t = 0; t < numTips; t++) {
        uint32_t seed = simpleRandom(t * 67 + _fireplaceFlamePhase * 11);
        
        // Tip position - starts where main fire is (around 70% height)
        int tipBaseX = x - width/4 + (seed % (width / 2));
        int tipStartY = (int)(y - height * 0.7f);  // Start at 70% height
        int tipHeight = 4 + (seed % 6);  // 4-9 pixels
        int tipWidth = 1 + (seed % 2);   // 1-2 pixels
        
        // Draw flame tip (bottom to top)
        for (int i = 0; i < tipHeight; i++) {
            float tipProgress = (float)i / tipHeight;
            uint32_t tipSeed = simpleRandom(i * 41 + t * 19 + _fireplaceFlamePhase * 3);
            
            // Gentle taper
            int currentTipWidth = max(1, (int)(tipWidth * (1.0f - tipProgress * 0.7f)));
            
            // Minimal flicker
            int tipFlickerX = ((tipSeed / 5) % 2);
            
            for (int dx = -currentTipWidth; dx <= currentTipWidth; dx++) {
                int colorIdx = 2 + (int)(tipProgress * 3);
                if (colorIdx > 5) colorIdx = 5;
                
                int px = tipBaseX + dx + tipFlickerX;
                int py = tipStartY - i;
                
                if (px >= x - width/2 && px <= x + width/2 && py >= 0) {
                    _currentCanvas->drawPixel(px, py, flameColors[colorIdx]);
                }
            }
        }
    }
    
    // ===== EMBERS AT BASE =====
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
    
    // ===== SPARKS =====
    for (int i = 0; i < 4; i++) {  // Fewer sparks (4 instead of 6)
        uint32_t seed = simpleRandom(i * 17 + _fireplaceFlamePhase * 7);
        if (seed % 6 == 0) {  // Less frequent
            int sparkX = x - width/3 + (seed % (width * 2 / 3));
            int sparkY = y - height - (seed % 6);
            if (sparkY >= 0 && sparkX >= x - width/2 && sparkX <= x + width/2) {
                _currentCanvas->drawPixel(sparkX, sparkY, flameColors[seed % 2]);
            }
        }
    }
}

void AnimationsModule::drawStockings(int simsY, int simsWidth, int centerX, int leftEdge) {
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
    
    int stockingH = 18;
    int stockingW = 8;
    
    // Calculate spacing to distribute across exact mantel width
    // Total width needed for all stockings
    int totalStockingWidth = stockingCount * stockingW;
    // Remaining space for gaps
    int remainingSpace = simsWidth - totalStockingWidth;
    // Space between stockings (including edges)
    int spacing = remainingSpace / (stockingCount + 1);
    
    for (int i = 0; i < stockingCount; i++) {
        // Position from left edge of mantel, not from center
        int sx = leftEdge + spacing * (i + 1) + stockingW * i;
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
    
    // Wenn Uhr aktiv: max 4 Dekorationen (2 links + 2 rechts)
    // Wenn Uhr aus: max 5 Dekorationen
    int maxDeco = showClock ? 4 : 5;
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
        int clockR = (int)(13 * scale);  // Radius +3px (diameter +6px) for better visibility
        
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
    
    // Positionen: bei showClock bis zu 4 (2 links + 2 rechts), sonst bis zu 5
    // Neue Dekoration-Typen: 0=Kerze, 1=Buch, 2=Vase, 3=Teekanne, 4=Bilderrahmen
    int positions[5];
    int decoTypes[5];
    
    if (showClock) {
        // Mit Uhr (mittig): bis zu 4 Dekorationen (2 links + 2 rechts)
        if (decoCount == 1) {
            positions[0] = centerX - simsWidth/3;
            decoTypes[0] = 0;  // Kerze
        } else if (decoCount == 2) {
            positions[0] = centerX - simsWidth/3;
            positions[1] = centerX + simsWidth/3;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 0;  // Kerze
        } else if (decoCount == 3) {
            positions[0] = centerX - simsWidth/2.5;
            positions[1] = centerX - simsWidth/6;
            positions[2] = centerX + simsWidth/3;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 1;  // Buch
            decoTypes[2] = 0;  // Kerze
        } else {  // decoCount >= 4
            positions[0] = centerX - simsWidth/2.5;
            positions[1] = centerX - simsWidth/6;
            positions[2] = centerX + simsWidth/6;
            positions[3] = centerX + simsWidth/2.5;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 1;  // Buch
            decoTypes[2] = 3;  // Teekanne
            decoTypes[3] = 4;  // Bilderrahmen
        }
    } else {
        // Ohne Uhr: bis zu 5 Dekorationen
        if (decoCount == 1) {
            positions[0] = centerX;
            decoTypes[0] = 0;  // Kerze
        } else if (decoCount == 2) {
            positions[0] = centerX - simsWidth/3;
            positions[1] = centerX + simsWidth/3;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 0;  // Kerze
        } else if (decoCount == 3) {
            positions[0] = centerX - simsWidth/3;
            positions[1] = centerX;
            positions[2] = centerX + simsWidth/3;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 0;  // Kerze (mittig)
            decoTypes[2] = 4;  // Bilderrahmen
        } else if (decoCount == 4) {
            positions[0] = centerX - simsWidth/2.5;
            positions[1] = centerX - simsWidth/6;
            positions[2] = centerX + simsWidth/6;
            positions[3] = centerX + simsWidth/2.5;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 1;  // Buch
            decoTypes[2] = 3;  // Teekanne
            decoTypes[3] = 0;  // Kerze
        } else {  // decoCount == 5
            positions[0] = centerX - simsWidth/2.5;
            positions[1] = centerX - simsWidth/6;
            positions[2] = centerX;
            positions[3] = centerX + simsWidth/6;
            positions[4] = centerX + simsWidth/2.5;
            decoTypes[0] = 2;  // Vase
            decoTypes[1] = 1;  // Buch
            decoTypes[2] = 0;  // Kerze (mittig)
            decoTypes[3] = 3;  // Teekanne
            decoTypes[4] = 4;  // Bilderrahmen
        }
    }
    
    // Verschiedene Dekorationen zeichnen
    // Typen: 0=Kerze, 1=Buch, 2=Vase, 3=Teekanne, 4=Bilderrahmen
    for (int i = 0; i < decoCount; i++) {
        int cx = positions[i];
        int cy = simsY - 1;  // Direkt auf dem Sims
        int decoType = decoTypes[i];
        
        if (decoType == 0) {
            // Kerze mit Flamme (wie Adventskranz)
            uint16_t candleColor = rgb565(240, 220, 180);
            uint16_t wickColor = rgb565(40, 40, 40);
            
            int candleH = (int)(9 * scale);
            int candleW = (int)(3 * scale);
            
            // Kerze
            _currentCanvas->fillRect(cx - candleW/2, cy - candleH, candleW, candleH, candleColor);
            _currentCanvas->drawRect(cx - candleW/2, cy - candleH, candleW, candleH, rgb565(200, 180, 140));
            
            // Docht
            _currentCanvas->drawLine(cx, cy - candleH, cx, cy - candleH - 2, wickColor);
            
            // Flamme - verwende gleiche Methode wie Adventskranz für realistische Animation
            drawCandleFlame(cx, cy - candleH - 3, _fireplaceFlamePhase + i * 13);
            
        } else if (decoType == 1) {
            // Bücherstapel
            uint16_t bookColors[] = {rgb565(139, 69, 19), rgb565(178, 34, 34), rgb565(46, 82, 124)};
            
            int bookW = (int)(6 * scale);
            int bookH = (int)(3 * scale);
            
            // 3 gestapelte Bücher
            for (int b = 0; b < 3; b++) {
                int bookY = cy - bookH * (b + 1);
                _currentCanvas->fillRect(cx - bookW/2, bookY, bookW, bookH, bookColors[b % 3]);
                _currentCanvas->drawRect(cx - bookW/2, bookY, bookW, bookH, rgb565(80, 50, 30));
                // Buchrücken-Details
                _currentCanvas->drawLine(cx - bookW/2 + 1, bookY + 1, cx - bookW/2 + 1, bookY + bookH - 2, rgb565(220, 200, 180));
            }
            
        } else if (decoType == 2) {
            // Vase mit Blumen
            uint16_t vaseColor = rgb565(100, 80, 120);
            uint16_t flowerColors[] = {rgb565(255, 100, 150), rgb565(255, 200, 100), rgb565(150, 100, 255)};
            
            int vaseH = (int)(7 * scale);
            int vaseW = (int)(5 * scale);
            
            // Vase (bauchige Form)
            _currentCanvas->fillRect(cx - vaseW/2 + 1, cy - vaseH, vaseW - 2, vaseH, vaseColor);
            _currentCanvas->fillRect(cx - vaseW/2, cy - vaseH + 2, vaseW, vaseH - 3, vaseColor);
            _currentCanvas->drawRect(cx - vaseW/2, cy - vaseH + 2, vaseW, vaseH - 3, rgb565(70, 50, 90));
            
            // Blumen
            for (int f = 0; f < 3; f++) {
                int fx = cx + (f - 1) * 2;
                int fy = cy - vaseH - 2 - f;
                _currentCanvas->fillCircle(fx, fy, 2, flowerColors[f % 3]);
                _currentCanvas->drawLine(fx, fy + 2, fx, cy - vaseH + 1, rgb565(50, 120, 50));
            }
            
        } else if (decoType == 3) {
            // Teekanne
            uint16_t potColor = rgb565(180, 160, 140);
            uint16_t metalColor = rgb565(150, 140, 130);
            
            int potH = (int)(6 * scale);
            int potW = (int)(6 * scale);
            
            // Kannenkörper
            _currentCanvas->fillRect(cx - potW/2, cy - potH, potW, potH, potColor);
            _currentCanvas->drawRect(cx - potW/2, cy - potH, potW, potH, metalColor);
            
            // Deckel
            _currentCanvas->fillRect(cx - potW/2 - 1, cy - potH - 2, potW + 2, 2, metalColor);
            _currentCanvas->fillCircle(cx, cy - potH - 3, 1, metalColor);
            
            // Griff
            _currentCanvas->drawLine(cx + potW/2, cy - potH + 2, cx + potW/2 + 2, cy - potH + 3, metalColor);
            _currentCanvas->drawLine(cx + potW/2 + 2, cy - potH + 3, cx + potW/2 + 2, cy - 3, metalColor);
            
            // Ausguss
            _currentCanvas->drawLine(cx - potW/2, cy - potH + 2, cx - potW/2 - 1, cy - potH + 1, metalColor);
            
        } else {  // decoType == 4
            // Bilderrahmen
            uint16_t frameColor = rgb565(139, 90, 43);
            uint16_t pictureColor = rgb565(200, 180, 150);
            
            int frameW = (int)(7 * scale);
            int frameH = (int)(9 * scale);
            
            // Rahmen
            _currentCanvas->fillRect(cx - frameW/2, cy - frameH, frameW, frameH, frameColor);
            _currentCanvas->fillRect(cx - frameW/2 + 1, cy - frameH + 1, frameW - 2, frameH - 2, pictureColor);
            
            // Motiv (Landschaft)
            // Himmel
            _currentCanvas->fillRect(cx - frameW/2 + 2, cy - frameH + 2, frameW - 4, frameH/2 - 2, rgb565(150, 180, 220));
            // Berg
            _currentCanvas->fillTriangle(cx - 2, cy - frameH/2, cx, cy - frameH + 3, cx + 2, cy - frameH/2, rgb565(100, 100, 120));
            // Boden
            _currentCanvas->fillRect(cx - frameW/2 + 2, cy - frameH/2, frameW - 4, frameH/2 - 2, rgb565(100, 150, 80));
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
    
    // Zeichne jedes einzelne Pixel des Rahmens mit einfacher Farbrotation
    // LED1=Farbe1, LED2=Farbe2, LED3=Farbe3, LED4=Farbe4, dann wiederholen
    // Animation: Phase verschiebt die Farben um den Rahmen
    
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

// ============== SEASON ANIMATIONS ==============

void AnimationsModule::drawSeasonalAnimations() {
    TimeUtilities::Season currentSeason;
    
    // Test mode: rotate through all seasons
    if (config && config->seasonalAnimationsTestMode) {
        unsigned long now = millis();
        if (now - _lastTestModeSwitch > 15000) {  // Switch every 15 seconds
            _testModeSeasonIndex = (_testModeSeasonIndex + 1) % 4;
            _lastTestModeSwitch = now;
            
            // Reset initialization flags so the new season gets initialized
            _flowersInitialized = false;
            _butterfliesInitialized = false;
            _birdsInitialized = false;
            _leavesInitialized = false;
            _snowflakesInitialized = false;
        }
        currentSeason = (TimeUtilities::Season)_testModeSeasonIndex;
    } else {
        currentSeason = TimeUtilities::getCurrentSeason();
    }
    
    switch (currentSeason) {
        case TimeUtilities::Season::SPRING:
            drawSpringAnimation();
            break;
        case TimeUtilities::Season::SUMMER:
            drawSummerAnimation();
            break;
        case TimeUtilities::Season::AUTUMN:
            drawAutumnAnimation();
            break;
        case TimeUtilities::Season::WINTER:
            drawWinterAnimation();
            break;
    }
}

void AnimationsModule::initSpringAnimation() {
    if (!_flowers || !_butterflies) {
        Log.println("[AnimationsModule] Kann Frühlingsanimation nicht initialisieren - PSRAM-Allokation fehlgeschlagen");
        return;
    }
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Get counts from config
    int flowerCount = config ? config->seasonalSpringFlowerCount : 12;
    int butterflyCount = config ? config->seasonalSpringButterflyCount : 3;
    
    // Initialize flowers at the bottom
    for (int i = 0; i < flowerCount && i < MAX_FLOWERS; i++) {
        _flowers[i].x = (float)(rand() % canvasW);
        _flowers[i].y = (float)(canvasH - 5 - (rand() % 10));
        _flowers[i].size = 2 + (rand() % 2);
        _flowers[i].swayPhase = (float)(rand() % 360);
        
        // Various flower colors (pink, yellow, red, white, purple)
        uint16_t flowerColors[] = {
            rgb565(255, 105, 180), // Pink
            rgb565(255, 255, 100), // Yellow
            rgb565(255, 50, 50),   // Red
            rgb565(255, 255, 255), // White
            rgb565(200, 100, 255)  // Purple
        };
        _flowers[i].color = flowerColors[rand() % 5];
    }
    
    // Initialize butterflies
    for (int i = 0; i < butterflyCount && i < MAX_BUTTERFLIES; i++) {
        _butterflies[i].x = (float)(rand() % canvasW);
        _butterflies[i].y = (float)(10 + rand() % (canvasH - 20));
        _butterflies[i].vx = 0.3f + (float)(rand() % 10) / 20.0f;
        _butterflies[i].vy = ((rand() % 3) - 1) * 0.2f;
        _butterflies[i].wingPhase = rand() % 10;
        
        // Butterfly colors
        uint16_t butterflyColors[] = {
            rgb565(255, 200, 0),   // Yellow
            rgb565(255, 100, 150), // Pink
            rgb565(100, 150, 255), // Blue
        };
        _butterflies[i].color = butterflyColors[rand() % 3];
    }
    
    _flowersInitialized = true;
    _butterfliesInitialized = true;
}

void AnimationsModule::drawSpringAnimation() {
    if (!_flowers || !_butterflies) return;
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Get counts from config once
    int flowerCount = config ? config->seasonalSpringFlowerCount : 12;
    int butterflyCount = config ? config->seasonalSpringButterflyCount : 3;
    
    // Initialize on first call
    if (!_flowersInitialized || !_butterfliesInitialized) {
        initSpringAnimation();
        _lastButterflyUpdate = millis();
    }
    
    // Draw grass meadow covering FULL lower half (much more prominent)
    int meadowHeight = canvasH / 2;  // Increased from 1/3 to 1/2
    for (int x = 0; x < canvasW; x++) {
        for (int y = canvasH - meadowHeight; y < canvasH; y++) {
            // Varied green shades based on position
            uint32_t seed = simpleRandom(x * 17 + y * 23);
            int greenVariation = (seed % 80) - 40;
            int baseGreen = 120 + greenVariation;
            int darkGreen = 60 + greenVariation / 2;
            
            // Create grass texture with different heights
            if ((seed + y) % 4 == 0) {
                _currentCanvas->drawPixel(x, y, rgb565(30 + darkGreen/3, baseGreen, 30 + darkGreen/3));
            } else {
                _currentCanvas->drawPixel(x, y, rgb565(50 + darkGreen/2, baseGreen + 20, 40 + darkGreen/2));
            }
        }
        
        // Draw TALLER individual grass blades with sway
        if (x % 2 == 0) {  // More grass blades (every 2 pixels instead of 3)
            int grassTop = canvasH - meadowHeight - 5 - (simpleRandom(x * 7) % 8);  // Taller grass
            int sway = (int)(sin(_seasonAnimationPhase * 0.05f + x * 0.3f) * 2.0f);
            uint16_t grassColor = rgb565(60 + (simpleRandom(x*11) % 40), 140 + (simpleRandom(x*13) % 40), 60);
            _currentCanvas->drawLine(x, canvasH - meadowHeight, x + sway, grassTop, grassColor);
        }
    }
    
    // Draw LARGER flowers densely across the meadow
    for (int i = 0; i < flowerCount && i < MAX_FLOWERS; i++) {
        int fx = (int)(_flowers[i].x + sin(_flowers[i].swayPhase + _seasonAnimationPhase * 0.1f) * 2);
        int fy = (int)_flowers[i].y;
        
        // THICKER Stem
        uint16_t stemColor = rgb565(40, 120, 40);
        int stemHeight = 8 + _flowers[i].size * 2;  // Taller stems
        _currentCanvas->drawLine(fx, fy, fx, fy - stemHeight, stemColor);
        _currentCanvas->drawLine(fx - 1, fy, fx - 1, fy - stemHeight + 1, stemColor);  // Thicker
        
        // MUCH LARGER flower petals (3-5 pixel radius instead of 2-3)
        int flowerRadius = _flowers[i].size + 2;  // Increased size
        _currentCanvas->fillCircle(fx, fy - stemHeight, flowerRadius, _flowers[i].color);
        
        // Center
        uint16_t centerColor = rgb565(255, 200, 50);
        _currentCanvas->fillCircle(fx, fy - stemHeight, max(1, flowerRadius - 2), centerColor);
    }
    
    // Update and draw butterflies
    unsigned long now = millis();
    if (now - _lastButterflyUpdate > 100) {
        for (int i = 0; i < butterflyCount && i < MAX_BUTTERFLIES; i++) {
            _butterflies[i].x += _butterflies[i].vx;
            _butterflies[i].y += _butterflies[i].vy;
            _butterflies[i].wingPhase = (_butterflies[i].wingPhase + 1) % 10;
            
            // Gentle up/down motion
            if (rand() % 10 < 3) {
                _butterflies[i].vy = ((rand() % 3) - 1) * 0.3f;
            }
            
            // Wrap around screen
            if (_butterflies[i].x > canvasW) {
                _butterflies[i].x = -5;
                _butterflies[i].y = (float)(10 + rand() % (canvasH - 20));
            }
            if (_butterflies[i].y < 5) _butterflies[i].y = 5;
            if (_butterflies[i].y > canvasH / 2) _butterflies[i].y = canvasH / 2;  // Keep above meadow
        }
        _lastButterflyUpdate = now;
    }
    
    // Draw LARGER butterflies
    for (int i = 0; i < butterflyCount && i < MAX_BUTTERFLIES; i++) {
        int bx = (int)_butterflies[i].x;
        int by = (int)_butterflies[i].y;
        
        // Wing animation (flapping) - LARGER wings
        int wingSpread = (_butterflies[i].wingPhase < 5) ? 3 : 2;  // Increased from 2:1
        
        // Body (larger)
        _currentCanvas->drawPixel(bx, by, rgb565(50, 50, 50));
        _currentCanvas->drawPixel(bx, by + 1, rgb565(50, 50, 50));
        
        // LARGER Wings
        _currentCanvas->fillCircle(bx - wingSpread, by, 2, _butterflies[i].color);
        _currentCanvas->fillCircle(bx + wingSpread, by, 2, _butterflies[i].color);
        if (wingSpread == 3) {
            _currentCanvas->drawPixel(bx - 2, by - 1, _butterflies[i].color);
            _currentCanvas->drawPixel(bx + 2, by - 1, _butterflies[i].color);
        }
    }
}

void AnimationsModule::initSummerAnimation() {
    if (!_birds) {
        Log.println("[AnimationsModule] Kann Sommeranimation nicht initialisieren - PSRAM-Allokation fehlgeschlagen");
        return;
    }
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Get count from config
    int birdCount = config ? config->seasonalSummerBirdCount : 2;
    
    // Initialize birds
    for (int i = 0; i < birdCount && i < MAX_BIRDS; i++) {
        _birds[i].x = (float)(rand() % canvasW);
        _birds[i].y = (float)(5 + rand() % (canvasH / 3));
        _birds[i].vx = 0.5f + (float)(rand() % 10) / 10.0f;
        _birds[i].wingPhase = rand() % 8;
        _birds[i].facingRight = (rand() % 2) == 0;
    }
    
    _birdsInitialized = true;
}

void AnimationsModule::drawSummerAnimation() {
    if (!_birds) return;
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Initialize on first call
    if (!_birdsInitialized) {
        initSummerAnimation();
        _lastBirdUpdate = millis();
    }
    
    // Check if it's night time for day/night variations
    bool isNight = TimeUtilities::isNightTime();
    
    // BEACH SCENE: Draw water at bottom 30%
    int waterHeight = canvasH * 3 / 10;
    int beachHeight = canvasH / 10;
    int waterTop = canvasH - waterHeight;
    int beachTop = waterTop - beachHeight;
    
    // Draw ocean water with wave effect
    for (int x = 0; x < canvasW; x++) {
        for (int y = waterTop; y < canvasH; y++) {
            uint32_t seed = simpleRandom(x * 13 + y * 7 + _seasonAnimationPhase);
            int blueBase = 100 + (seed % 80);
            int greenShade = 140 + (seed % 60);
            
            // Wave highlights
            if ((y - waterTop + (int)(sin(x * 0.3f + _seasonAnimationPhase * 0.1f) * 2)) % 5 == 0) {
                _currentCanvas->drawPixel(x, y, rgb565(150, 200, 255));  // Light blue highlights
            } else {
                _currentCanvas->drawPixel(x, y, rgb565(30, blueBase, greenShade));
            }
        }
    }
    
    // Draw sandy beach strip
    for (int x = 0; x < canvasW; x++) {
        for (int y = beachTop; y < waterTop; y++) {
            uint32_t seed = simpleRandom(x * 11 + y * 17);
            int sandVariation = (seed % 40) - 20;
            _currentCanvas->drawPixel(x, y, rgb565(220 + sandVariation, 200 + sandVariation, 140 + sandVariation/2));
        }
    }
    
    // Draw MUCH LARGER palm trees on left and right
    uint16_t trunkColor = rgb565(139, 69, 19); // Brown
    uint16_t palmColor = rgb565(34, 139, 34); // Forest green
    
    // Left palm tree - MUCH TALLER and WIDER
    int leftX = 8;
    int palmBase = canvasH - waterHeight - beachHeight / 2;
    int palmTop = 5;  // Reaches near top of screen
    
    // THICKER Trunk with texture
    for (int y = palmTop; y < palmBase; y++) {
        int width = 4 + (simpleRandom(y * 3) % 3);  // 4-6 pixels wide
        for (int dx = -width/2; dx <= width/2; dx++) {
            uint16_t tColor = (dx == 0 || abs(dx) == width/2) ? rgb565(101, 51, 10) : trunkColor;
            if (leftX + dx >= 0 && leftX + dx < canvasW) {
                _currentCanvas->drawPixel(leftX + dx, y, tColor);
            }
        }
    }
    
    // LARGER Palm fronds (more visible)
    for (int angle = -60; angle <= 240; angle += 30) {  // More fronds, better spread
        float rad = angle * 3.14159f / 180.0f;
        int frondLen = 12 + (simpleRandom(angle) % 4);  // 12-15 pixels long
        int sway = (int)(sin(_seasonAnimationPhase * 0.05f) * 2.0f);
        
        // Draw thick frond
        for (int len = 0; len < frondLen; len++) {
            int fx = leftX + (int)(cos(rad) * len) + sway;
            int fy = palmTop + (int)(sin(rad) * len);
            if (fx >= 0 && fx < canvasW && fy >= 0 && fy < canvasH) {
                _currentCanvas->drawPixel(fx, fy, palmColor);
                // Make fronds thicker
                if (len % 2 == 0 && fx + 1 < canvasW) {
                    _currentCanvas->drawPixel(fx + 1, fy, rgb565(20, 100, 20));
                }
            }
        }
    }
    
    // Right palm tree - MUCH TALLER and WIDER
    int rightX = canvasW - 9;
    
    // THICKER Trunk
    for (int y = palmTop; y < palmBase; y++) {
        int width = 4 + (simpleRandom(y * 5) % 3);
        for (int dx = -width/2; dx <= width/2; dx++) {
            uint16_t tColor = (dx == 0 || abs(dx) == width/2) ? rgb565(101, 51, 10) : trunkColor;
            if (rightX + dx >= 0 && rightX + dx < canvasW) {
                _currentCanvas->drawPixel(rightX + dx, y, tColor);
            }
        }
    }
    
    // LARGER Palm fronds
    for (int angle = -60; angle <= 240; angle += 30) {
        float rad = angle * 3.14159f / 180.0f;
        int frondLen = 12 + (simpleRandom(angle + 100) % 4);
        int sway = (int)(sin(_seasonAnimationPhase * 0.05f + 1.5f) * 2.0f);
        
        for (int len = 0; len < frondLen; len++) {
            int fx = rightX + (int)(cos(rad) * len) + sway;
            int fy = palmTop + (int)(sin(rad) * len);
            if (fx >= 0 && fx < canvasW && fy >= 0 && fy < canvasH) {
                _currentCanvas->drawPixel(fx, fy, palmColor);
                if (len % 2 == 0 && fx > 0) {
                    _currentCanvas->drawPixel(fx - 1, fy, rgb565(20, 100, 20));
                }
            }
        }
    }
    
    if (isNight) {
        // Draw LARGER moon and stars at night
        int moonX = canvasW / 2;
        int moonY = 12;  // Moved down slightly to be more visible
        uint16_t moonColor = rgb565(220, 220, 255);
        
        _currentCanvas->fillCircle(moonX, moonY, 6, moonColor);  // Increased from 4 to 6
        _currentCanvas->fillCircle(moonX - 2, moonY - 2, 6, 0); // Crescent effect
        
        // Draw some stars
        for (int i = 0; i < 15; i++) {
            uint32_t seed = simpleRandom(i * 97 + 42);
            int sx = (seed % (canvasW - 20)) + 10;
            int sy = (seed / 13) % (canvasH / 2);
            uint16_t starColor = rgb565(255, 255, 255);
            
            // Twinkling effect
            if ((seed + _seasonAnimationPhase) % 30 < 20) {
                _currentCanvas->drawPixel(sx, sy, starColor);
                // Add cross pattern for brighter stars
                if (i % 3 == 0) {
                    if (sx > 0) _currentCanvas->drawPixel(sx - 1, sy, starColor);
                    if (sx < canvasW - 1) _currentCanvas->drawPixel(sx + 1, sy, starColor);
                }
            }
        }
    } else {
        // Draw LARGER sun during day
        int sunX = canvasW / 2;
        int sunY = 12;  // Moved down slightly
        uint16_t sunColor = rgb565(255, 255, 0);
        uint16_t sunGlow = rgb565(255, 200, 100);
        
        _currentCanvas->fillCircle(sunX, sunY, 6, sunColor);  // Increased from 4 to 6
        // LONGER Sun rays
        for (int angle = 0; angle < 360; angle += 45) {
            float rad = angle * 3.14159f / 180.0f;
            int rayLen = 9 + (_seasonAnimationPhase % 3);  // Increased from 6 to 9
            int rx = sunX + (int)(cos(rad) * rayLen);
            int ry = sunY + (int)(sin(rad) * rayLen);
            _currentCanvas->drawLine(sunX, sunY, rx, ry, sunGlow);
        }
    }
    
    // Update and draw birds (less active at night)
    unsigned long now = millis();
    int birdCount = config ? config->seasonalSummerBirdCount : 2;
    
    if (now - _lastBirdUpdate > 120) {
        for (int i = 0; i < birdCount && i < MAX_BIRDS; i++) {
            // Birds slower at night
            float speed = isNight ? _birds[i].vx * 0.5f : _birds[i].vx;
            
            if (_birds[i].facingRight) {
                _birds[i].x += speed;
            } else {
                _birds[i].x -= speed;
            }
            _birds[i].wingPhase = (_birds[i].wingPhase + 1) % 8;
            
            // Wrap around
            if (_birds[i].x > canvasW + 5) {
                _birds[i].x = -5;
                _birds[i].y = (float)(5 + rand() % (canvasH / 3));
            }
            if (_birds[i].x < -5) {
                _birds[i].x = canvasW + 5;
                _birds[i].y = (float)(5 + rand() % (canvasH / 3));
            }
        }
        _lastBirdUpdate = now;
    }
    
    // Draw birds (only during day, or fewer at night)
    if (!isNight || rand() % 10 < 3) {
        uint16_t birdColor = isNight ? rgb565(70, 70, 70) : rgb565(100, 100, 100);
        for (int i = 0; i < birdCount && i < MAX_BIRDS; i++) {
            int bx = (int)_birds[i].x;
            int by = (int)_birds[i].y;
            
            // Simple bird shape (V-shape)
            int wingPos = (_birds[i].wingPhase < 4) ? 1 : 2;
            
            if (_birds[i].facingRight) {
                _currentCanvas->drawLine(bx, by, bx - 2, by - wingPos, birdColor);
                _currentCanvas->drawLine(bx, by, bx - 2, by + wingPos, birdColor);
            } else {
                _currentCanvas->drawLine(bx, by, bx + 2, by - wingPos, birdColor);
                _currentCanvas->drawLine(bx, by, bx + 2, by + wingPos, birdColor);
            }
        }
    }
}

void AnimationsModule::initAutumnAnimation() {
    if (!_leaves) {
        Log.println("[AnimationsModule] Kann Herbstanimation nicht initialisieren - PSRAM-Allokation fehlgeschlagen");
        return;
    }
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Get count from config
    int leafCount = config ? config->seasonalAutumnLeafCount : 15;
    
    // Tree positions for leaves to fall from
    int leftTreeX = canvasW / 4;
    int rightTreeX = 3 * canvasW / 4;
    int treeTop = canvasH / 3;  // Trees are in upper portion
    
    // Initialize falling leaves FROM TREE POSITIONS
    for (int i = 0; i < leafCount && i < MAX_LEAVES; i++) {
        // Alternate between left and right tree
        int treeX = (i % 2 == 0) ? leftTreeX : rightTreeX;
        
        // Start leaves near tree canopy position
        _leaves[i].x = (float)(treeX + (rand() % 20) - 10);  // Within tree canopy width
        _leaves[i].y = (float)(treeTop + (rand() % 15));  // Starting from tree height
        _leaves[i].vx = ((rand() % 3) - 1) * 0.2f;
        _leaves[i].vy = 0.3f + (float)(rand() % 10) / 20.0f;
        _leaves[i].rotation = (float)(rand() % 360);
        _leaves[i].size = 2 + (rand() % 2);
        
        // Autumn leaf colors (brown, orange, red, yellow)
        uint16_t leafColors[] = {
            rgb565(139, 69, 19),   // Brown
            rgb565(255, 140, 0),   // Orange
            rgb565(200, 50, 50),   // Red
            rgb565(255, 215, 0),   // Yellow
            rgb565(178, 34, 34)    // Dark red
        };
        _leaves[i].color = leafColors[rand() % 5];
    }
    
    _leavesInitialized = true;
}

void AnimationsModule::drawAutumnAnimation() {
    if (!_leaves) return;
    
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Initialize on first call
    if (!_leavesInitialized) {
        initAutumnAnimation();
        _lastLeafUpdate = millis();
    }
    
    // Draw MUCH LARGER autumn trees with brown foliage
    uint16_t trunkColor = rgb565(101, 67, 33); // Brown trunk
    uint16_t foliageColors[] = {
        rgb565(139, 69, 19),   // Brown
        rgb565(255, 140, 0),   // Orange
        rgb565(200, 50, 50),   // Red
        rgb565(178, 34, 34)    // Dark red
    };
    
    // Left tree - MUCH LARGER
    int leftTreeX = canvasW / 4;
    int treeBase = canvasH - 2;
    int treeTop = canvasH / 3;  // Taller tree (reaches 1/3 up screen)
    
    // THICKER Trunk (3-4 pixels wide)
    for (int y = treeBase; y > treeTop; y--) {
        _currentCanvas->drawPixel(leftTreeX - 2, y, trunkColor);
        _currentCanvas->drawPixel(leftTreeX - 1, y, rgb565(80, 53, 26));
        _currentCanvas->drawPixel(leftTreeX, y, trunkColor);
        _currentCanvas->drawPixel(leftTreeX + 1, y, rgb565(80, 53, 26));
        _currentCanvas->drawPixel(leftTreeX + 2, y, trunkColor);
    }
    
    // MUCH LARGER Foliage (autumn colored) - 20 pixels wide
    for (int y = treeTop - 12; y < treeTop + 5; y++) {
        for (int x = leftTreeX - 10; x <= leftTreeX + 10; x++) {
            int dx = x - leftTreeX;
            int dy = y - (treeTop - 4);
            // Rounded canopy shape
            if (dx*dx + dy*dy < 100) {  // Increased radius
                uint32_t seed = simpleRandom(x * 17 + y * 23);
                if (seed % 3 != 0) { // Sparse for autumn look
                    uint16_t color = foliageColors[seed % 4];
                    _currentCanvas->drawPixel(x, y, color);
                }
            }
        }
    }
    
    // Right tree - MUCH LARGER
    int rightTreeX = 3 * canvasW / 4;
    
    // THICKER Trunk
    for (int y = treeBase; y > treeTop; y--) {
        _currentCanvas->drawPixel(rightTreeX - 2, y, trunkColor);
        _currentCanvas->drawPixel(rightTreeX - 1, y, rgb565(80, 53, 26));
        _currentCanvas->drawPixel(rightTreeX, y, trunkColor);
        _currentCanvas->drawPixel(rightTreeX + 1, y, rgb565(80, 53, 26));
        _currentCanvas->drawPixel(rightTreeX + 2, y, trunkColor);
    }
    
    // MUCH LARGER Foliage
    for (int y = treeTop - 12; y < treeTop + 5; y++) {
        for (int x = rightTreeX - 10; x <= rightTreeX + 10; x++) {
            int dx = x - rightTreeX;
            int dy = y - (treeTop - 4);
            if (dx*dx + dy*dy < 100) {
                uint32_t seed = simpleRandom(x * 19 + y * 29);
                if (seed % 3 != 0) {
                    uint16_t color = foliageColors[seed % 4];
                    _currentCanvas->drawPixel(x, y, color);
                }
            }
        }
    }
    
    // Draw leaves on the ground (accumulated)
    for (int x = 0; x < canvasW; x += 2) {
        uint32_t seed = simpleRandom(x * 7 + 100);
        if (seed % 4 < 2) {
            int groundY = canvasH - 1 - (seed % 2);
            _currentCanvas->drawPixel(x, groundY, foliageColors[seed % 4]);
        }
    }
    
    // Update falling leaves
    unsigned long now = millis();
    int leafCount = config ? config->seasonalAutumnLeafCount : 15;
    
    if (now - _lastLeafUpdate > 60) {
        for (int i = 0; i < leafCount && i < MAX_LEAVES; i++) {
            _leaves[i].x += _leaves[i].vx;
            _leaves[i].y += _leaves[i].vy;
            _leaves[i].rotation += 5.0f;
            
            // Gentle sideways drift (wind effect)
            if (rand() % 10 < 2) {
                _leaves[i].vx = ((rand() % 3) - 1) * 0.3f;
            }
            
            // Reset at bottom - RESPAWN FROM TREE POSITION
            if (_leaves[i].y > canvasH) {
                // Choose tree to fall from
                int treeX = (i % 2 == 0) ? leftTreeX : rightTreeX;
                _leaves[i].y = (float)(treeTop + (rand() % 15));  // From tree height
                _leaves[i].x = (float)(treeX + (rand() % 20) - 10);  // Within canopy
                _leaves[i].vx = ((rand() % 3) - 1) * 0.2f;
                _leaves[i].vy = 0.3f + (float)(rand() % 10) / 20.0f;
            }
            
            // Wrap horizontally
            if (_leaves[i].x < -5) _leaves[i].x = canvasW + 5;
            if (_leaves[i].x > canvasW + 5) _leaves[i].x = -5;
        }
        _lastLeafUpdate = now;
    }
    
    // Draw falling leaves (LARGER)
    for (int i = 0; i < leafCount && i < MAX_LEAVES; i++) {
        int lx = (int)_leaves[i].x;
        int ly = (int)_leaves[i].y;
        
        // Larger leaf shape
        if (_leaves[i].size >= 2) {
            _currentCanvas->fillCircle(lx, ly, 2, _leaves[i].color);
            _currentCanvas->drawPixel(lx, ly + 1, rgb565(100, 69, 19)); // Stem
            _currentCanvas->drawPixel(lx + 1, ly, _leaves[i].color);  // Make wider
        } else {
            _currentCanvas->fillCircle(lx, ly, 1, _leaves[i].color);
        }
    }
}

void AnimationsModule::initWinterAnimation() {
    // Snowflakes will be initialized by drawSnowflakes() on first call
    // This method is kept for potential future winter-specific initialization
}

void AnimationsModule::drawWinterAnimation() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    // Constants for winter scene
    const int SNOW_HEIGHT = 8;
    const int SNOWMAN_TREE_MIN_DISTANCE = 15;
    
    // Get config values
    int treeCount = config ? config->seasonalWinterTreeCount : 2;
    bool showSnowman = config ? config->seasonalWinterShowSnowman : true;
    
    // Draw snowy ground at bottom (accumulated snow)
    uint16_t snowColor = rgb565(255, 255, 255);
    uint16_t snowShadow = rgb565(200, 220, 240);
    
    // Snow layer with some variation
    for (int x = 0; x < canvasW; x++) {
        uint32_t seed = simpleRandom(x * 17 + 123);
        int yVariation = (seed % 3) - 1;
        int snowTop = canvasH - SNOW_HEIGHT + yVariation;
        
        for (int y = snowTop; y < canvasH; y++) {
            // Slight color variation for depth
            if (y > snowTop + 2) {
                _currentCanvas->drawPixel(x, y, snowShadow);
            } else {
                _currentCanvas->drawPixel(x, y, snowColor);
            }
        }
    }
    
    // Draw snowy trees if configured
    if (treeCount > 0) {
        drawSnowyTrees();
    }
    
    // Draw snowman if configured
    if (showSnowman) {
        int snowmanX = canvasW / 2;
        int snowmanY = canvasH - SNOW_HEIGHT - 2;
        drawSnowman(snowmanX, snowmanY, 1.0f);
    }
    
    // Draw falling snowflakes
    drawSnowflakes();
    
    // Optional: Draw stars if night time
    if (TimeUtilities::isNightTime()) {
        // Add some stars for night sky
        for (int i = 0; i < 8; i++) {
            uint32_t seed = simpleRandom(i * 113 + 456);
            int sx = (seed % (canvasW - 10)) + 5;
            int sy = (seed / 17) % (canvasH / 3);
            
            // Twinkling effect
            if ((seed + _seasonAnimationPhase) % 40 < 25) {
                _currentCanvas->drawPixel(sx, sy, snowColor);
            }
        }
    }
}

void AnimationsModule::drawSnowman(int x, int y, float scale) {
    uint16_t snowWhite = rgb565(255, 255, 255);
    uint16_t snowShadow = rgb565(220, 230, 240);
    uint16_t orange = rgb565(255, 140, 0);
    uint16_t black = rgb565(0, 0, 0);
    uint16_t brown = rgb565(139, 69, 19);
    
    // MUCH LARGER snowman (2-3x original size)
    int baseRadius = (int)(8 * scale);      // Increased from 5 to 8
    int middleRadius = (int)(6 * scale);    // Increased from 4 to 6
    int headRadius = (int)(5 * scale);      // Increased from 3 to 5
    
    // Bottom ball
    int bottomY = y;
    _currentCanvas->fillCircle(x, bottomY, baseRadius, snowWhite);
    _currentCanvas->drawCircle(x, bottomY, baseRadius, snowShadow);
    if (baseRadius > 3) {
        _currentCanvas->drawCircle(x, bottomY, baseRadius - 1, snowShadow);
    }
    
    // Middle ball
    int middleY = bottomY - baseRadius - middleRadius + 3;
    _currentCanvas->fillCircle(x, middleY, middleRadius, snowWhite);
    _currentCanvas->drawCircle(x, middleY, middleRadius, snowShadow);
    if (middleRadius > 2) {
        _currentCanvas->drawCircle(x, middleY, middleRadius - 1, snowShadow);
    }
    
    // Head
    int headY = middleY - middleRadius - headRadius + 3;
    _currentCanvas->fillCircle(x, headY, headRadius, snowWhite);
    _currentCanvas->drawCircle(x, headY, headRadius, snowShadow);
    if (headRadius > 2) {
        _currentCanvas->drawCircle(x, headY, headRadius - 1, snowShadow);
    }
    
    // LARGER Eyes (coal) - 2 pixels each
    _currentCanvas->fillCircle(x - 2, headY - 2, 1, black);
    _currentCanvas->fillCircle(x + 2, headY - 2, 1, black);
    
    // LARGER Carrot nose
    _currentCanvas->drawPixel(x, headY, orange);
    _currentCanvas->drawPixel(x + 1, headY, orange);
    _currentCanvas->drawPixel(x + 2, headY, orange);
    _currentCanvas->drawPixel(x + 1, headY + 1, orange);
    
    // Larger Smile (coal)
    _currentCanvas->drawPixel(x - 2, headY + 3, black);
    _currentCanvas->drawPixel(x - 1, headY + 4, black);
    _currentCanvas->drawPixel(x, headY + 4, black);
    _currentCanvas->drawPixel(x + 1, headY + 4, black);
    _currentCanvas->drawPixel(x + 2, headY + 3, black);
    
    // LARGER Buttons (coal)
    _currentCanvas->fillCircle(x, middleY - 2, 1, black);
    _currentCanvas->fillCircle(x, middleY, 1, black);
    _currentCanvas->fillCircle(x, middleY + 2, 1, black);
    
    // LONGER Stick arms
    if (scale >= 1.0f) {
        // Left arm (longer)
        _currentCanvas->drawLine(x - middleRadius, middleY, x - middleRadius - 5, middleY - 3, brown);
        _currentCanvas->drawLine(x - middleRadius - 5, middleY - 3, x - middleRadius - 7, middleY - 4, brown);
        _currentCanvas->drawPixel(x - middleRadius - 6, middleY - 2, brown);  // Twig
        
        // Right arm (longer)
        _currentCanvas->drawLine(x + middleRadius, middleY, x + middleRadius + 5, middleY - 3, brown);
        _currentCanvas->drawLine(x + middleRadius + 5, middleY - 3, x + middleRadius + 7, middleY - 2, brown);
        _currentCanvas->drawPixel(x + middleRadius + 6, middleY - 4, brown);  // Twig
    }
}

void AnimationsModule::drawSnowyTrees() {
    int canvasW = _currentCanvas->width();
    int canvasH = _currentCanvas->height();
    
    uint16_t treeGreen = rgb565(0, 100, 0);
    uint16_t treeDarkGreen = rgb565(0, 70, 0);
    uint16_t snowWhite = rgb565(255, 255, 255);
    uint16_t trunkBrown = rgb565(101, 67, 33);
    
    const int SNOW_HEIGHT = 8;
    const int SNOWMAN_TREE_MIN_DISTANCE = 20;  // Increased for larger snowman
    const int MAX_TREE_POSITIONS = 3;
    int groundY = canvasH - SNOW_HEIGHT;
    
    // Get tree count from config (limited by available positions)
    int numTrees = config ? config->seasonalWinterTreeCount : 2;
    if (numTrees > MAX_TREE_POSITIONS) numTrees = MAX_TREE_POSITIONS;
    
    // Tree positions and MUCH LARGER sizes
    const int treePosX[MAX_TREE_POSITIONS] = {canvasW / 4, canvasW * 3 / 4, canvasW / 8};
    const int treeSizes[MAX_TREE_POSITIONS] = {18, 20, 16};  // Increased from 10,12,8 to 18,20,16
    
    for (int t = 0; t < numTrees; t++) {
        int treeX = treePosX[t];
        int treeHeight = treeSizes[t];
        int treeY = groundY;
        
        // Skip if snowman would overlap
        if (config && config->seasonalWinterShowSnowman && abs(treeX - canvasW / 2) < SNOWMAN_TREE_MIN_DISTANCE) continue;
        
        // THICKER Trunk
        int trunkWidth = 4;  // Increased from 2 to 4
        int trunkHeight = 6; // Increased from 4 to 6
        _currentCanvas->fillRect(treeX - trunkWidth/2, treeY - trunkHeight, trunkWidth, trunkHeight, trunkBrown);
        
        // Triangular tree shape (3 layers) - MUCH LARGER
        int layerHeight = treeHeight / 3;
        
        for (int layer = 0; layer < 3; layer++) {
            int layerY = treeY - trunkHeight - (layer + 1) * layerHeight;
            int layerWidth = (12 - layer * 3);  // Increased from (8-layer*2) to (12-layer*3)
            
            // Draw tree layer as triangle
            for (int dy = 0; dy < layerHeight; dy++) {
                int width = layerWidth - (dy * layerWidth / layerHeight);
                for (int dx = -width; dx <= width; dx++) {
                    uint16_t color = (dx == -width || dx == width) ? treeDarkGreen : treeGreen;
                    if (treeX + dx >= 0 && treeX + dx < canvasW) {
                        _currentCanvas->drawPixel(treeX + dx, layerY + dy, color);
                    }
                }
            }
            
            // THICKER Snow on top of each layer
            int snowWidth = layerWidth + 2;  // Increased snow coverage
            for (int dx = -snowWidth; dx <= snowWidth; dx++) {
                if (treeX + dx >= 0 && treeX + dx < canvasW) {
                    _currentCanvas->drawPixel(treeX + dx, layerY, snowWhite);
                    if (layerY + 1 < canvasH) {
                        _currentCanvas->drawPixel(treeX + dx, layerY + 1, snowWhite);  // Double thickness
                    }
                }
            }
        }
        
        // LARGER Snow on very top
        int topY = treeY - trunkHeight - treeHeight;
        if (topY >= 0) {
            _currentCanvas->fillCircle(treeX, topY, 2, snowWhite);  // Larger top snow cap
        }
    }
}
