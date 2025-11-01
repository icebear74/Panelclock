#ifndef PANELMANAGER_HPP
#define PANELMANAGER_HPP

#include "PsramUtils.hpp" // Stellt PsramVector und PsramString zur Verfügung
#include "DrawableModule.hpp"
#include "HardwareConfig.hpp"

// Vorwärtsdeklarationen
class ClockModule;
class MwaveSensorModule;
class GeneralTimeConverter;
class U8G2_FOR_ADAFRUIT_GFX;
class GFXcanvas16;
class MatrixPanel_I2S_DMA;
template <uint8_t T_MATRIX_TILE_WIDTH, uint8_t T_MATRIX_TILE_HEIGHT> class VirtualMatrixPanel_T;

/**
 * @brief Steuert die Anzeige aller Module, verwaltet die Playlist und Interrupts.
 * @version 6.1 (Finale Architektur nach Design von icebear74)
 */
class PanelManager {
public:
    /**
     * @brief Definiert die vereinheitlichte Datenstruktur für alle Module.
     * Jeder Eintrag ist ein vollständiger "Steckbrief" für das Webinterface und die interne Logik.
     */
    struct PlaylistEntry {
        DrawableModule* module = nullptr;
        PsramString moduleNameShort;      // Interner Name, z.B. "DartsModule"
        PsramString moduleNameDisplay;    // Anzeigename, z.B. "Dart-Ranking"
        bool isRunning = false;
        bool isPaused = false;
        bool isHidden = false;
        bool isDisabled = false;
        unsigned long totalRuntimeMs = 0;
        unsigned long remainingTimeMs = 0;
        Priority priority = Priority::Normal;
    };

    PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter);
    ~PanelManager();

    bool begin();
    void registerModule(DrawableModule* mod, bool isHidden = false, bool isDisabled = false);
    void registerClockModule(ClockModule* mod);
    void registerSensorModule(MwaveSensorModule* mod);

    void tick();
    void render();
    void displayStatus(const char* msg);

private:
    void handlePriorityRequest(DrawableModule* mod);
    void handlePriorityRelease(DrawableModule* mod);
    void switchNextModule();
    void pausePlaylist();
    void resumePlaylist();
    void drawClockArea();
    void activateEntry(PlaylistEntry& entry);

    HardwareConfig& _hwConfig;
    GeneralTimeConverter& _timeConverter;

    // Display-Objekte
    MatrixPanel_I2S_DMA* _dma_display = nullptr;
    VirtualMatrixPanel_T<1, 1>* _virtualDisp = nullptr;
    GFXcanvas16* _canvasTime = nullptr;
    GFXcanvas16* _canvasData = nullptr;
    U8G2_FOR_ADAFRUIT_GFX* _u8g2 = nullptr;

    // Kern-Module
    ClockModule* _clockMod = nullptr;
    MwaveSensorModule* _sensorMod = nullptr;

    // --- DIE DREI-LISTEN-ARCHITEKTUR ---
    PsramVector<PlaylistEntry> _moduleCatalog;
    PsramVector<PlaylistEntry> _playlist;
    PsramVector<PlaylistEntry> _interruptQueue;

    int _currentPlaylistIndex = -1;
    unsigned long _lastTickTimestamp = 0;
    
    // Play-Next Logik
    DrawableModule* _nextNormalPriorityModule = nullptr;
    DrawableModule* _pendingNormalPriorityModule = nullptr;
};

#endif // PANELMANAGER_HPP