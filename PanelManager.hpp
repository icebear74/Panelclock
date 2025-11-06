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

/**
 * @brief PlaylistEntry - Wrapper für Module in Playlist und InterruptQueue
 */
struct PlaylistEntry {
    DrawableModule* module = nullptr;
    bool isRunning = false;
    bool isPaused = false;
    bool isDisabled = false;
    uint32_t uid = 0;
    Priority priority = Priority::Normal;
    unsigned long startTime = 0;
    unsigned long pausedDuration = 0;
    unsigned long pauseStartTime = 0;
    uint32_t logicTickCounter = 0;
    
    // Konstruktor
    PlaylistEntry(DrawableModule* mod, uint32_t id = 0, Priority prio = Priority::Normal, unsigned long duration = 0) 
        : module(mod), uid(id), priority(prio) {
        // Keine Duration speichern - wird dynamisch geholt!
    }
    
    // PSRAM-Allokation
    void* operator new(size_t size) {
        void* p = ps_malloc(size);
        if (!p) {
            Serial.println("FATAL: PlaylistEntry PSRAM-Allokation fehlgeschlagen!");
        }
        return p;
    }
    
    void operator delete(void* p) {
        free(p);
    }
    
    // Utility-Funktionen
    bool canActivate() const {
        return module && !isDisabled && module->isEnabled();
    }
    
    bool isOneShot() const {
        return priority == Priority::PlayNext;
    }
    
    bool isInterrupt() const {
        return priority > Priority::PlayNext;
    }
    
    void activate() {
        if (!module) return;
        isRunning = true;
        isPaused = false;
        startTime = millis();
        pausedDuration = 0;
        logicTickCounter = 0;
        module->activateModule(uid);
        Serial.printf("[PlaylistEntry] Aktiviere Modul '%s' (UID: %lu, Prio: %d)\n", 
                     module->getModuleName(), uid, (int)priority);
    }
    
    void deactivate() {
        isRunning = false;
        isPaused = false;
        pausedDuration = 0;
        if (module) {
            Serial.printf("[PlaylistEntry] Deaktiviere Modul '%s' (UID: %lu)\n", 
                         module->getModuleName(), uid);
        }
    }
    
    void pause() {
        if (!isRunning || isPaused) return;
        isPaused = true;
        pauseStartTime = millis();
        
        if (module) {
            module->pause();
            Serial.printf("[PlaylistEntry] Pausiere Modul '%s'\n", module->getModuleName());
        }
    }
    
    void resume() {
        if (!isPaused) return;
        isPaused = false;
        
        // Akkumuliere Pause-Zeit
        pausedDuration += (millis() - pauseStartTime);
        
        if (module) {
            module->resume();
            Serial.printf("[PlaylistEntry] Setze Modul '%s' fort (pausiert: %lu ms)\n", 
                         module->getModuleName(), pausedDuration);
        }
    }
    
    bool isFinished() const {
        if (!module || !isRunning) return false;
        
        // 1. Modul selbst sagt es ist fertig
        if (module->isFinished()) {
            return true;
        }
        
        // 2. Wenn pausiert, nie Timeout
        if (isPaused) return false;
        
        // 3. Timeout-Check - DYNAMISCH Duration holen!
        unsigned long maxDuration = module->getDisplayDuration();
        
        // Sicherheitspuffer hinzufügen (10% oder min 1 Sekunde)
        unsigned long safetyBuffer = max(1000UL, maxDuration / 10);
        maxDuration += safetyBuffer;
        
        unsigned long elapsed = millis() - startTime - pausedDuration;
        
        if (elapsed >= maxDuration) {
            Serial.printf("[PlaylistEntry] Timeout: '%s' nach %lums (max: %lums)\n",
                         module->getModuleName(), elapsed, maxDuration);
            return true;
        }
        
        return false;
    }
};

/**
 * @brief Verwaltet das LED-Panel, die Modul-Rotation und Prioritätsanfragen.
 */
class PanelManager {
public:
    PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter);
    ~PanelManager();

    bool begin();
    void registerClockModule(ClockModule* mod);
    void registerSensorModule(MwaveSensorModule* mod);
    void registerModule(DrawableModule* mod);
    
    void tick();
    void render();
    void displayStatus(const char* msg);
    
    // NEU: Erweiterte Priority-Handler mit UID und Duration
    bool handlePriorityRequest(DrawableModule* mod, Priority prio, uint32_t uid, unsigned long durationMs);
    void handlePriorityRelease(DrawableModule* mod, uint32_t uid);
    
    // Getter für Display-Komponenten
    U8G2_FOR_ADAFRUIT_GFX* getU8g2() { return _u8g2; }
    GFXcanvas16* getCanvasTime() { return _canvasTime; }
    GFXcanvas16* getCanvasData() { return _canvasData; }
    GFXcanvas16* getFullCanvas() { return _fullCanvas; }
    MatrixPanel_I2S_DMA* getDisplay() { return _dma_display; }
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* getVirtualDisplay() { return _virtualDisp; }
    
    // Für Kompatibilität
    const PsramVector<DrawableModule*>& getAllModules() const { return _moduleCatalog; }

private:
    // Kernfunktionen
    void switchNextModule();
    PlaylistEntry* findActiveEntry();
    void tickActiveModule();
    
    // Helper-Funktionen
    void drawClockArea();
    void drawDataArea();
    PlaylistEntry* findEntryByModuleAndUID(DrawableModule* mod, uint32_t uid);
    PlaylistEntry* findRunningInPlaylist();
    PlaylistEntry* findPausedInPlaylist();
    int findPlaylistIndex(PlaylistEntry* entry);
    
    // Hardware & Konfiguration
    HardwareConfig& _hwConfig;
    GeneralTimeConverter& _timeConverter;
    
    // Display-Objekte
    MatrixPanel_I2S_DMA* _dma_display = nullptr;
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp = nullptr;
    GFXcanvas16* _canvasTime = nullptr;
    GFXcanvas16* _canvasData = nullptr;
    GFXcanvas16* _fullCanvas = nullptr;
    U8G2_FOR_ADAFRUIT_GFX* _u8g2 = nullptr;
    
    // Spezielle Module
    ClockModule* _clockMod = nullptr;
    MwaveSensorModule* _sensorMod = nullptr;
    
    // Die drei Kern-Listen (alle in PSRAM)
    PsramVector<DrawableModule*> _moduleCatalog;
    PsramVector<PlaylistEntry*> _playlist;
    PsramVector<PlaylistEntry*> _interruptQueue;
    
    // Timing für Logic-Tick
    unsigned long _lastLogicTick = 0;
    const unsigned long LOGIC_TICK_INTERVAL = 100; // ms
    
    // NEU: Für separaten Logic-Tick-Task
    SemaphoreHandle_t _logicTickMutex = nullptr;
    TaskHandle_t _logicTickTaskHandle = nullptr;
    static void logicTickTaskWrapper(void* param);
    void logicTickTask();
};

#endif // PANELMANAGER_HPP