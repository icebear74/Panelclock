#ifndef ANIMATIONSMODULE_HPP
#define ANIMATIONSMODULE_HPP

#include "DrawableModule.hpp"
#include "GeneralTimeConverter.hpp"
#include "PsramUtils.hpp"
#include "TimeUtilities.hpp"
#include <U8g2_for_Adafruit_GFX.h>
#include <Adafruit_GFX.h>
#include <functional>

struct DeviceConfig;

// UID für Advent-Interrupts
#define ADVENT_WREATH_UID_BASE 2000
#define CHRISTMAS_TREE_UID_BASE 2100

// Anzeige-Modi
enum class ChristmasDisplayMode {
    Wreath,      // Adventskranz
    Tree,        // Weihnachtsbaum
    Fireplace,   // Kamin
    Alternate    // Wechsel zwischen allen aktiven
};

/**
 * @brief Modul zur Anzeige verschiedener Weihnachts- und Jahreszeiten-Animationen.
 * 
 * Weihnachts-Animationen:
 * - Adventskranz mit 4 Kerzen in verschiedenen Farben.
 *   Je nach aktuellem Advent (1-4) brennen die entsprechenden Kerzen
 *   mit animierten Flammen. Der Kranz ist mit Tannengrün dekoriert.
 * - Weihnachtsbaum mit Lichtern, Kugeln und Geschenken.
 * - Kamin mit animiertem Feuer und dekorativen Elementen.
 * 
 * Jahreszeiten-Animationen (ganzjährig):
 * - Frühling: Blumen und Schmetterlinge
 * - Sommer: Sonne/Mond mit Vögeln und Sternen (Tag/Nacht-Wechsel)
 * - Herbst: Fallende Blätter
 * - Winter: Schneeflocken, Schneemann, verschneite Bäume, Schnee am Boden
 * 
 * Das Modul verwendet Priority::PlayNext (OneShot),
 * um regelmäßig als nächstes nach dem aktuellen Modul angezeigt zu werden.
 */
class AnimationsModule : public DrawableModule {
public:
    AnimationsModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, GeneralTimeConverter& timeConverter, DeviceConfig* config);
    ~AnimationsModule();

    /**
     * @brief Initialisiert das Modul.
     */
    void begin();

    /**
     * @brief Wendet die Konfiguration aus DeviceConfig an.
     */
    void setConfig();

    /**
     * @brief Registriert einen Callback, der bei Animation-Updates aufgerufen wird.
     * @param callback Die Callback-Funktion für Redraw-Anforderungen
     */
    void onUpdate(std::function<void()> callback);

    // --- DrawableModule Interface ---
    const char* getModuleName() const override { return "AnimationsModule"; }
    const char* getModuleDisplayName() const override { return "Animationen"; }
    void draw() override;
    void tick() override;
    void logicTick() override;
    void periodicTick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;
    bool isFinished() const override { return _isFinished; }
    bool canBeInPlaylist() const override { return false; } // Nur als PlayNext OneShot
    void timeIsUp() override;
    
    // --- Fullscreen-Unterstützung ---
    bool supportsFullscreen() const override { return true; }
    bool wantsFullscreen() const override;

protected:
    void onActivate() override;

private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    GeneralTimeConverter& timeConverter;
    DeviceConfig* config;
    
    // Zeiger auf den aktuell verwendeten Canvas (normal oder fullscreen)
    GFXcanvas16* _currentCanvas = nullptr;

    // Zustandsvariablen
    bool _isFinished = false;
    bool _isAdventViewActive = false;
    bool _requestPending = false;  // Verhindert doppelte Requests bevor Aktivierung erfolgt
    uint32_t _currentAdventUID = 0;
    unsigned long _adventViewStartTime = 0;
    unsigned long _lastAdventDisplayTime = 0;
    unsigned long _lastPeriodicCheck = 0;
    int _lastCheckedDay = -1;
    
    // Wechsel zwischen Kranz, Baum und Kamin
    int _displayMode = 0;  // 0=Kranz, 1=Baum, 2=Kamin
    
    // Baum-Randomisierung
    unsigned long _lastTreeDisplay = 0;
    bool _treeOrnamentsNeedRegeneration = true;
    int _shuffledOrnamentY[30];      // Max 30 Ornamente (mehr als genug)
    int _shuffledOrnamentX[30];
    uint8_t _shuffledOrnamentColors[30];
    uint8_t _shuffledOrnamentSizes[30];
    int _shuffledLightY[30];         // Max 30 Lichter
    int _shuffledLightX[30];
    bool _showTree = false;
    bool _showFireplace = false;
    int _displayCounter = 0;
    
    // Snowflake animation
    struct Snowflake {
        float x, y;
        float speed;
        int size;
    };
    static const int MAX_SNOWFLAKES = 40;  // Maximum, actual count from config
    Snowflake* _snowflakes = nullptr;
    bool _snowflakesInitialized = false;
    unsigned long _lastSnowflakeUpdate = 0;
    
    // Season animations
    struct Flower {
        float x, y;
        uint16_t color;
        int size;
        float swayPhase;
    };
    static const int MAX_FLOWERS = 20;  // Maximum, actual count from config
    Flower* _flowers = nullptr;
    bool _flowersInitialized = false;
    
    struct Butterfly {
        float x, y;
        float vx, vy;
        uint16_t color;
        int wingPhase;
    };
    static const int MAX_BUTTERFLIES = 8;  // Maximum, actual count from config
    Butterfly* _butterflies = nullptr;
    bool _butterfliesInitialized = false;
    unsigned long _lastButterflyUpdate = 0;
    
    struct Leaf {
        float x, y;
        float vx, vy;
        float rotation;
        uint16_t color;
        int size;
    };
    static const int MAX_LEAVES = 30;  // Maximum, actual count from config
    Leaf* _leaves = nullptr;
    bool _leavesInitialized = false;
    unsigned long _lastLeafUpdate = 0;
    
    struct Bird {
        float x, y;
        float vx;
        int wingPhase;
        bool facingRight;
    };
    static const int MAX_BIRDS = 6;  // Maximum, actual count from config
    Bird* _birds = nullptr;
    bool _birdsInitialized = false;
    unsigned long _lastBirdUpdate = 0;
    
    int _seasonAnimationPhase = 0;
    unsigned long _lastSeasonAnimUpdate = 0;
    int _testModeSeasonIndex = 0;  // For cycling through seasons in test mode
    unsigned long _lastTestModeSwitch = 0;  // Time of last season switch in test mode
    
    // Zufällige Kerzenreihenfolge für jeden Durchgang
    int _candleOrder[4] = {0, 1, 2, 3};
    uint32_t _lastOrderSeed = 0;

    // Animation
    unsigned long _lastFlameUpdate = 0;
    unsigned long _lastTreeLightUpdate = 0;
    unsigned long _lastLedBorderUpdate = 0;
    int _flamePhase = 0;
    int _treeLightPhase = 0;
    int _ledBorderPhase = 0;
    int _ledBorderSubPhase = 0;  // 0-255 for smooth color transitions
    
    // Konfigurierbare Parameter (Defaults)
    unsigned long _displayDurationMs = 15000;  // 15 Sekunden
    unsigned long _repeatIntervalMs = 30 * 60 * 1000;  // 30 Minuten
    unsigned long _flameAnimationMs = 50;  // 50ms = 20 FPS für Flammen-Animation
    unsigned long _treeLightAnimationMs = 80;  // 80ms für Baumlicht-Animation
    unsigned long _fireplaceFlameMs = 40;  // 40ms für Kaminfeuer-Animation
    int _fireplaceFlamePhase = 0;
    
    // Callback für Redraw
    std::function<void()> _updateCallback = nullptr;

    /**
     * @brief Zeichnet einen LED-Lauflicht-Rahmen um den Bereich
     */
    void drawLedBorder();

    /**
     * @brief Berechnet den aktuellen Advent (1-4) oder 0 wenn nicht Adventszeit.
     * @return Adventsnummer (1-4) oder 0
     */
    int calculateCurrentAdvent();

    /**
     * @brief Prüft ob wir in der Adventszeit sind (bis 24.12.)
     */
    bool isAdventSeason();

    /**
     * @brief Prüft ob wir in der Weihnachtszeit sind (25.12. - 31.12.)
     */
    bool isChristmasSeason();

    /**
     * @brief Prüft ob wir in der Gesamt-Weihnachtszeit sind (1. Advent - 31.12.)
     */
    bool isHolidaySeason();

    /**
     * @brief Bestimmt den aktuellen Anzeige-Modus basierend auf Datum
     */
    ChristmasDisplayMode getCurrentDisplayMode();

    /**
     * @brief Berechnet das Datum des 4. Advents für ein gegebenes Jahr.
     * @param year Das Jahr
     * @return time_t des 4. Advents
     */
    time_t calculateFourthAdvent(int year);

    /**
     * @brief Mischt die Kerzenreihenfolge neu
     */
    void shuffleCandleOrder();

    /**
     * @brief Mischt Kugeln und Lichter am Weihnachtsbaum für Variation
     */
    void shuffleTreeElements();

    /**
     * @brief Zeichnet den Adventskranz mit Tannengrün.
     */
    void drawWreath();

    /**
     * @brief Zeichnet den Weihnachtsbaum
     */
    void drawChristmasTree();

    /**
     * @brief Zeichnet den natürlichen Baum mit Grünvariationen
     * @param scale Skalierungsfaktor basierend auf Canvas-Größe
     */
    void drawNaturalTree(int centerX, int baseY, float scale = 1.0f);

    /**
     * @brief Zeichnet Kugeln am Weihnachtsbaum
     * @param scale Skalierungsfaktor
     */
    void drawTreeOrnaments(int centerX, int baseY, float scale = 1.0f);

    /**
     * @brief Zeichnet Geschenke unter dem Baum
     * @param scale Skalierungsfaktor
     */
    void drawGifts(int centerX, int baseY, float scale = 1.0f);

    /**
     * @brief Zeichnet eine einzelne Kerze.
     * @param x X-Position der Kerzenmitte
     * @param y Y-Position der Kerzenbasis
     * @param color Kerzenfarbe (RGB565)
     * @param isLit true wenn die Kerze brennen soll
     * @param candleIndex Index für Flimmer-Offset
     */
    void drawCandle(int x, int y, uint16_t color, bool isLit, int candleIndex);

    /**
     * @brief Zeichnet eine animierte Flamme mit Mischfarben.
     * @param x X-Position
     * @param y Y-Position
     * @param phase Animationsphase
     */
    void drawFlame(int x, int y, int phase);
    
    /**
     * @brief Zeichnet eine Kerzenflamme mit Kaminfeuer-Algorithmus (ohne Funken)
     * @param x X-Position
     * @param y Y-Position
     * @param phase Animationsphase
     */
    void drawCandleFlame(int x, int y, int phase);

    /**
     * @brief Zeichnet Tannengrün-Dekoration.
     */
    void drawGreenery();

    /**
     * @brief Zeichnet einen einzelnen Tannenzweig.
     */
    void drawBranch(int x, int y, int direction);

    /**
     * @brief Zeichnet dekorative Kugeln mit 3D-Effekt.
     */
    void drawBerries();

    /**
     * @brief Zeichnet eine Baumkugel mit 3D-Effekt
     */
    void drawOrnament(int x, int y, int radius, uint16_t color);

    /**
     * @brief Zeichnet blinkende Lichter am Baum
     */
    void drawTreeLights();
    
    /**
     * @brief Zeichnet fallende Schneeflocken
     */
    void drawSnowflakes();
    
    /**
     * @brief Zeichnet jahreszeiten-spezifische Animationen
     */
    void drawSeasonalAnimations();
    
    /**
     * @brief Zeichnet Frühlingsanimation (Blumen, Schmetterlinge)
     */
    void drawSpringAnimation();
    
    /**
     * @brief Zeichnet Sommeranimation (Sonne, Vögel)
     */
    void drawSummerAnimation();
    
    /**
     * @brief Zeichnet Herbstanimation (fallende Blätter)
     */
    void drawAutumnAnimation();
    
    /**
     * @brief Zeichnet Winteranimation (Schneeflocken, Schneemann, verschneite Landschaft)
     */
    void drawWinterAnimation();
    
    /**
     * @brief Initialisiert Frühlingsanimation
     */
    void initSpringAnimation();
    
    /**
     * @brief Initialisiert Sommeranimation
     */
    void initSummerAnimation();
    
    /**
     * @brief Initialisiert Herbstanimation
     */
    void initAutumnAnimation();
    
    /**
     * @brief Initialisiert Winteranimation
     */
    void initWinterAnimation();
    
    /**
     * @brief Zeichnet einen Schneemann
     */
    void drawSnowman(int x, int y, float scale);
    
    /**
     * @brief Zeichnet verschneite Bäume
     */
    void drawSnowyTrees();
    
    /**
     * @brief Zeichnet Countdown bis Silvester
     */
    void drawNewYearCountdown();

    /**
     * @brief Zeichnet einen Kamin mit Feuer
     */
    void drawFireplace();

    /**
     * @brief Zeichnet animiertes Kaminfeuer
     */
    void drawFireplaceFlames(int x, int y, int width, int height);

    /**
     * @brief Zeichnet Strümpfe am Kaminsims
     */
    void drawStockings(int simsY, int simsWidth, int centerX, int leftEdge);

    /**
     * @brief Zeichnet dekorative Gegenstände auf dem Kaminsims
     */
    void drawMantleDecorations(int simsY, int simsWidth, int centerX, float scale);

    /**
     * @brief Prüft ob der Kamin in der aktuellen Saison aktiv ist
     */
    bool isFireplaceSeason();
    
    // Obsolete helper methods - no longer used after procedural fireplace rewrite
    // void drawFireplaceFrame(int x, int y, int width, int height, uint16_t brickColor, uint16_t brickDark);
    // void drawFireplaceMantel(int x, int y, int width, int height, uint16_t color, uint16_t lightColor, uint16_t darkColor);
    // void drawFireplaceLogs(int x, int y, int width, float scale);
    // void drawFireplaceTools(int x, int y, int height, float scale);
    // void drawWoodStorage(int x, int y, int width, int height, float scale);

    /**
     * @brief Konvertiert RGB zu RGB565.
     */
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Blends two RGB565 colors
     * @param color1 First RGB565 color
     * @param color2 Second RGB565 color  
     * @param blend Blend amount (0-15, where 0=color1, 15=color2)
     * @return Blended RGB565 color
     */
    static uint16_t blendRGB565(uint16_t color1, uint16_t color2, int blend);

    /**
     * @brief Konvertiert Hex-Farbstring zu RGB565.
     */
    static uint16_t hexToRgb565(const char* hex);
    
    /**
     * @brief Einfacher Pseudo-Zufallsgenerator
     */
    uint32_t simpleRandom(uint32_t seed);
};

#endif // ANIMATIONSMODULE_HPP
