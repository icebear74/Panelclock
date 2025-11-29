#ifndef ADVENTWREATHMODULE_HPP
#define ADVENTWREATHMODULE_HPP

#include "DrawableModule.hpp"
#include "GeneralTimeConverter.hpp"
#include "PsramUtils.hpp"
#include <U8g2_for_Adafruit_GFX.h>
#include <Adafruit_GFX.h>

struct DeviceConfig;

// UID für Advent-Interrupts
#define ADVENT_WREATH_UID_BASE 2000

/**
 * @brief Modul zur Anzeige eines animierten Adventskranzes.
 * 
 * Zeigt einen Adventskranz mit 4 Kerzen in verschiedenen Farben.
 * Je nach aktuellem Advent (1-4) brennen die entsprechenden Kerzen
 * mit animierten Flammen. Der Kranz ist mit Tannengrün dekoriert.
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

protected:
    void onActivate() override;

private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    GeneralTimeConverter& timeConverter;
    DeviceConfig* config;

    // Zustandsvariablen
    bool _isFinished = false;
    bool _isAdventViewActive = false;
    uint32_t _currentAdventUID = 0;
    unsigned long _adventViewStartTime = 0;
    unsigned long _lastAdventDisplayTime = 0;
    unsigned long _lastPeriodicCheck = 0;
    int _lastCheckedDay = -1;

    // Animation
    unsigned long _lastFlameUpdate = 0;
    int _flamePhase = 0;
    
    // Konfigurierbare Parameter (Defaults)
    unsigned long _displayDurationMs = 15000;  // 15 Sekunden
    unsigned long _repeatIntervalMs = 30 * 60 * 1000;  // 30 Minuten

    /**
     * @brief Berechnet den aktuellen Advent (1-4) oder 0 wenn nicht Adventszeit.
     * @return Adventsnummer (1-4) oder 0
     */
    int calculateCurrentAdvent();

    /**
     * @brief Berechnet das Datum des 4. Advents für ein gegebenes Jahr.
     * @param year Das Jahr
     * @return time_t des 4. Advents
     */
    time_t calculateFourthAdvent(int year);

    /**
     * @brief Zeichnet den Adventskranz mit Tannengrün.
     */
    void drawWreath();

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
     * @brief Zeichnet eine animierte Flamme.
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
     * @brief Zeichnet dekorative Beeren.
     */
    void drawBerries();

    /**
     * @brief Konvertiert RGB zu RGB565.
     */
    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
};

#endif // ADVENTWREATHMODULE_HPP
