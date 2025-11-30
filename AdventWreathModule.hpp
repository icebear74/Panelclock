#ifndef ADVENTWREATHMODULE_HPP
#define ADVENTWREATHMODULE_HPP

#include "DrawableModule.hpp"
#include "GeneralTimeConverter.hpp"
#include "PsramUtils.hpp"
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
 * @brief Modul zur Anzeige eines animierten Adventskranzes und Weihnachtsbaums.
 * 
 * Zeigt einen Adventskranz mit 4 Kerzen in verschiedenen Farben.
 * Je nach aktuellem Advent (1-4) brennen die entsprechenden Kerzen
 * mit animierten Flammen. Der Kranz ist mit Tannengrün dekoriert.
 * 
 * Zusätzlich wird ein Weihnachtsbaum mit Lichtern und Kugeln gezeigt.
 * 
 * Das Modul verwendet Priority::PlayNext (OneShot),
 * um während der Adventszeit regelmäßig als nächstes nach dem
 * aktuellen Modul angezeigt zu werden.
 */
class AdventWreathModule : public DrawableModule {
public:
    AdventWreathModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, GeneralTimeConverter& timeConverter, DeviceConfig* config);
    ~AdventWreathModule();

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
    const char* getModuleName() const override { return "AdventWreathModule"; }
    const char* getModuleDisplayName() const override { return "Adventskranz"; }
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
    bool _showTree = false;
    bool _showFireplace = false;
    int _displayCounter = 0;
    
    // Zufällige Kerzenreihenfolge für jeden Durchgang
    int _candleOrder[4] = {0, 1, 2, 3};
    uint32_t _lastOrderSeed = 0;

    // Animation
    unsigned long _lastFlameUpdate = 0;
    unsigned long _lastTreeLightUpdate = 0;
    int _flamePhase = 0;
    int _treeLightPhase = 0;
    
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
    void drawStockings(int simsY, int simsWidth, int centerX);

    /**
     * @brief Zeichnet Kerzen auf dem Kaminsims
     */
    void drawMantleCandles(int simsY, int simsWidth, int centerX);

    /**
     * @brief Prüft ob der Kamin in der aktuellen Saison aktiv ist
     */
    bool isFireplaceSeason();

    /**
     * @brief Konvertiert RGB zu RGB565.
     */
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Konvertiert Hex-Farbstring zu RGB565.
     */
    static uint16_t hexToRgb565(const char* hex);
    
    /**
     * @brief Einfacher Pseudo-Zufallsgenerator
     */
    uint32_t simpleRandom(uint32_t seed);
};

#endif // ADVENTWREATHMODULE_HPP
