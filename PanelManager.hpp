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
 * @brief Verwaltet das LED-Panel, die Modul-Rotation und Prioritätsanfragen.
 * 
 * Diese Klasse ist das Herzstück der Anzeige. Sie initialisiert das Display,
 * verwaltet eine "Playlist" von anzeigbaren Modulen, steuert die Rotation
 * zwischen ihnen und handhabt ein Prioritätssystem, das es Modulen erlaubt,
 * die normale Rotation für wichtige Anzeigen (Interrupts) zu unterbrechen.
 */
class PanelManager {
public:
    /**
     * @brief Konstruktor für den PanelManager.
     * @param hwConfig Referenz auf die Hardware-Konfiguration mit den Pin-Belegungen.
     * @param timeConverter Referenz auf den Zeitumrechner.
     */
    PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter);
    
    /**
     * @brief Destruktor. Gibt alle dynamisch alloziierten Display-Objekte frei.
     */
    ~PanelManager();

    /**
     * @brief Initialisiert das Display, die Canvases und die virtuellen Panels.
     * @return true bei Erfolg, andernfalls false.
     */
    bool begin();

    /**
     * @brief Registriert das dedizierte Uhren-Modul.
     * @param mod Ein Zeiger auf das ClockModule.
     */
    void registerClockModule(ClockModule* mod);

    /**
     * @brief Registriert das dedizierte Sensor-Modul.
     * @param mod Ein Zeiger auf das MwaveSensorModule.
     */
    void registerSensorModule(MwaveSensorModule* mod);

    /**
     * @brief Registriert ein allgemeines, anzeigbares Modul für die Daten-Area.
     * @param mod Ein Zeiger auf ein DrawableModule.
     */
    void registerModule(DrawableModule* mod);

    /**
     * @brief Wird in der Hauptschleife aufgerufen, um die Modul-Logik zu steuern.
     * 
     * Handhabt die Rotation der Module, die Anzeigezeit und die Logik von Interrupts.
     */
    void tick();

    /**
     * @brief Zeichnet den kompletten Bildschirminhalt und sendet ihn an das Panel.
     * 
     * Ruft die `draw`-Methoden der aktiven Module auf und kombiniert die Canvases.
     */
    void render();

    /**
     * @brief Zeigt eine zentrierte Statusmeldung auf dem gesamten Display an.
     * @param msg Die anzuzeigende Nachricht (kann Zeilenumbrüche enthalten).
     */
    void displayStatus(const char* msg);

    // --- Öffentliche Getter ---
    U8G2_FOR_ADAFRUIT_GFX* getU8g2() { return _u8g2; }
    GFXcanvas16* getCanvasTime() { return _canvasTime; }
    GFXcanvas16* getCanvasData() { return _canvasData; }
    GFXcanvas16* getFullCanvas() { return _fullCanvas; }
    MatrixPanel_I2S_DMA* getDisplay() { return _dma_display; }
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* getVirtualDisplay() { return _virtualDisp; }
    const PsramVector<DrawableModule*>& getAllModules() const;

private:
    // --- Interne Handler für die Modul-Callbacks ---
    void handlePriorityRequest(DrawableModule* mod);
    void handlePriorityRelease(DrawableModule* mod);

    // --- Interne Logik- und Zeichenmethoden ---
    void drawClockArea();
    void drawDataArea();
    void switchNextModule(bool resume = false);
    void pausePlaylist();
    void resumePlaylist();

    // --- Kernkomponenten und Konfiguration ---
    /// @brief Referenz auf die globale Hardware-Konfiguration (Pins).
    HardwareConfig& _hwConfig;
    /// @brief Referenz auf den globalen Zeitumrechner.
    GeneralTimeConverter& _timeConverter;

    // --- Display-Objekte ---
    /// @brief Der DMA-Treiber für das HUB75-Panel.
    MatrixPanel_I2S_DMA* _dma_display = nullptr;
    /// @brief Die virtuelle Matrix, die mehrere Panels zu einer großen Anzeige verbindet.
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp = nullptr;
    /// @brief Der Grafikpuffer (Canvas) für den oberen Uhrenbereich.
    GFXcanvas16* _canvasTime = nullptr;
    /// @brief Der Grafikpuffer (Canvas) für den unteren Datenbereich.
    GFXcanvas16* _canvasData = nullptr;
    /// @brief Ein Grafikpuffer für die gesamte Anzeigefläche.
    GFXcanvas16* _fullCanvas = nullptr;
    /// @brief Die U8G2-Bibliothek für Text-Rendering.
    U8G2_FOR_ADAFRUIT_GFX* _u8g2 = nullptr;

    // --- Spezielle Module ---
    /// @brief Zeiger auf das immer präsente Uhren-Modul.
    ClockModule* _clockMod = nullptr;
    /// @brief Zeiger auf das Sensor-Modul zur Steuerung der Display-Helligkeit/des Status.
    MwaveSensorModule* _sensorMod = nullptr;
    
    // --- Playlist-Logik für rotierende Module ---
    /// @brief Vektor aller registrierten Module für den Datenbereich.
    PsramVector<DrawableModule*> _dataAreaModules;
    /// @brief Index des aktuell in der Playlist angezeigten Moduls.
    int _currentModuleIndex = -1;
    /// @brief Zeitstempel des letzten Modulwechsels.
    unsigned long _lastSwitchTime = 0;

    // --- Zustand für "Pause & Resume" bei Interrupts ---
    /// @brief Index des Moduls, das durch einen Interrupt pausiert wurde.
    int _pausedModuleIndex = -1;
    /// @brief Verbleibende Anzeigezeit des pausierten Moduls.
    unsigned long _pausedModuleRemainingTime = 0;

    // --- Prioritäts- und Interrupt-Handling ---
    /// @brief Warteschlange für aktive Interrupt-Anfragen, sortiert nach Priorität.
    PsramVector<DrawableModule*> _interruptQueue;
    /// @brief Zeiger auf ein Modul, das als nächstes angezeigt werden soll (Priorität Normal).
    DrawableModule* _nextNormalPriorityModule = nullptr;
    /// @brief Zeiger auf das zuletzt aktive Interrupt-Modul, um Timeouts zu überwachen.
    DrawableModule* _lastActiveInterrupt = nullptr;
    /// @brief Zeitstempel, wann der aktuelle Interrupt gestartet wurde.
    unsigned long _interruptStartTime = 0;
    /// @brief Zeiger auf eine "Play-Next"-Anfrage, die während eines Interrupts eintraf und geparkt wurde.
    DrawableModule* _pendingNormalPriorityModule = nullptr;
};

#endif // PANELMANAGER_HPP