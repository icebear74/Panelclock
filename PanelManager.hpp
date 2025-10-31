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
     * @brief [NEU] Markiert ein bereits registriertes Modul als "MODERN".
     * Der PanelManager wird für dieses Modul die neue Steuerungslogik verwenden.
     * @param mod Ein Zeiger auf das umgestellte Modul.
     */
    void markAsModern(DrawableModule* mod);

    /**
     * @brief Wird in der Hauptschleife aufgerufen, um die Modul-Logik zu steuern.
     * Handhabt die Rotation der Module, die Anzeigezeit und die Logik von Interrupts.
     */
    void tick();

    /**
     * @brief Zeichnet den kompletten Bildschirminhalt und sendet ihn an das Panel.
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

    /**
     * @brief [NEU] Prüft, ob ein Modul als "MODERN" markiert wurde.
     * @param mod Der Zeiger auf das zu prüfende Modul.
     * @return true, wenn das Modul die neue Schnittstelle verwendet.
     */
    bool isModern(const DrawableModule* mod) const;

    // --- Kernkomponenten und Konfiguration ---
    HardwareConfig& _hwConfig;
    GeneralTimeConverter& _timeConverter;

    // --- Display-Objekte ---
    MatrixPanel_I2S_DMA* _dma_display = nullptr;
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp = nullptr;
    GFXcanvas16* _canvasTime = nullptr;
    GFXcanvas16* _canvasData = nullptr;
    GFXcanvas16* _fullCanvas = nullptr;
    U8G2_FOR_ADAFRUIT_GFX* _u8g2 = nullptr;

    // --- Spezielle Module ---
    ClockModule* _clockMod = nullptr;
    MwaveSensorModule* _sensorMod = nullptr;
    
    // --- Playlist-Logik für rotierende Module ---
    /// @brief Vektor aller registrierten Module für den Datenbereich.
    PsramVector<DrawableModule*> _dataAreaModules;
    
    /**
     * @brief [NEU] Vektor, der Zeiger auf alle Module enthält, die als "MODERN" markiert wurden.
     * Dient als Nachschlagetabelle für die "Switter"-Logik.
     */
    PsramVector<DrawableModule*> _modernModules;

    /// @brief Index des aktuell in der Playlist angezeigten Moduls.
    int _currentModuleIndex = -1;
    /// @brief Zeitstempel, wann das aktuelle Modul gestartet wurde (ersetzt _lastSwitchTime).
    unsigned long _moduleStartTime = 0;

    // --- Zustand für "Pause & Resume" bei Interrupts ---
    int _pausedModuleIndex = -1;
    unsigned long _pausedModuleRemainingTime = 0;

    // --- Prioritäts- und Interrupt-Handling ---
    PsramVector<DrawableModule*> _interruptQueue;
    DrawableModule* _nextNormalPriorityModule = nullptr;
    DrawableModule* _lastActiveInterrupt = nullptr;
    unsigned long _interruptStartTime = 0;
    DrawableModule* _pendingNormalPriorityModule = nullptr;
};

#endif // PANELMANAGER_HPP