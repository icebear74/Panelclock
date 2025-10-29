#ifndef PANELMANAGER_HPP
#define PANELMANAGER_HPP

#include <vector>
#include "PsramUtils.hpp"
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <U8g2_for_Adafruit_GFX.h>
#include "HardwareConfig.hpp"
#include "DrawableModule.hpp"

// Vorwärtsdeklarationen für die speziellen Module und Helfer
class ClockModule;
class MwaveSensorModule;
class GeneralTimeConverter;
struct tm;

// Konstanten, die für die Panel-Konfiguration benötigt werden
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN
const int FULL_WIDTH = PANEL_RES_X * VDISP_NUM_COLS;
const int FULL_HEIGHT = PANEL_RES_Y * VDISP_NUM_ROWS;
const int TIME_AREA_H = 30;
const int DATA_AREA_H = FULL_HEIGHT - TIME_AREA_H;

class PanelManager {
public:
    PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter);
    ~PanelManager();

    bool begin();

    // Registrierungsmethoden für die verschiedenen Modultypen
    void registerClockModule(ClockModule* mod);
    void registerSensorModule(MwaveSensorModule* mod);
    void registerModule(DrawableModule* mod);

    void tick();
    void render();
    void displayStatus(const char* msg);

    // Öffentliche Getter für Objekte, die bei der Initialisierung anderer Module benötigt werden
    U8G2_FOR_ADAFRUIT_GFX* getU8g2() { return _u8g2; }
    GFXcanvas16* getCanvasTime() { return _canvasTime; }
    GFXcanvas16* getCanvasData() { return _canvasData; }
    GFXcanvas16* getFullCanvas() { return _fullCanvas; }
    MatrixPanel_I2S_DMA* getDisplay() { return _dma_display; }
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* getVirtualDisplay() { return _virtualDisp; }

    const PsramVector<DrawableModule*>& getAllModules() const;

private:
    // Interne Handler für die Callbacks
    void handlePriorityRequest(DrawableModule* mod);
    void handlePriorityRelease(DrawableModule* mod);

    // Interne Zeichen- und Logikmethoden
    void drawClockArea();
    void drawDataArea();
    void switchNextModule(bool resume = false);
    void pausePlaylist();
    void resumePlaylist();

    // Referenzen auf Kernkomponenten
    HardwareConfig& _hwConfig;
    GeneralTimeConverter& _timeConverter;

    // Display-Objekte
    MatrixPanel_I2S_DMA* _dma_display = nullptr;
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp = nullptr;
    GFXcanvas16* _canvasTime = nullptr;
    GFXcanvas16* _canvasData = nullptr;
    GFXcanvas16* _fullCanvas = nullptr;
    U8G2_FOR_ADAFRUIT_GFX* _u8g2 = nullptr;

    // Zeiger auf spezielle Module
    ClockModule* _clockMod = nullptr;
    MwaveSensorModule* _sensorMod = nullptr;
    
    // Playlist für rotierende Module (im PSRAM)
    PsramVector<DrawableModule*> _dataAreaModules;
    int _currentModuleIndex = -1;
    unsigned long _lastSwitchTime = 0;

    // Zustand für "Pause & Resume"
    int _pausedModuleIndex = -1;
    unsigned long _pausedModuleRemainingTime = 0;

    // Prioritäts-Handling
    PsramVector<DrawableModule*> _interruptQueue;
    DrawableModule* _nextNormalPriorityModule = nullptr;
    DrawableModule* _lastActiveInterrupt = nullptr;
    unsigned long _interruptStartTime = 0;
};

#endif // PANELMANAGER_HPP