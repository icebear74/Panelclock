#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <Arduino.h>
#include "PanelManager.hpp"
#include "ClockModule.hpp"
#include "TankerkoenigModule.hpp"
#include "CalendarModule.hpp"
#include "DartsRankingModule.hpp"
#include "FritzboxModule.hpp"
#include "CuriousHolidaysModule.hpp"
#include "WeatherModule.hpp" // HINZUGEFÜGT
#include "PanelStreamer.hpp" // HINZUGEFÜGT
#include "BackupManager.hpp" // HINZUGEFÜGT

// Forward-Deklarationen, um zirkuläre Abhängigkeiten in Headern zu vermeiden
class PanelManager;
class ClockModule;
class TankerkoenigModule;
class CalendarModule;
class DartsRankingModule;
class FritzboxModule;
class CuriousHolidaysModule;
class WeatherModule; // HINZUGEFÜGT
class PanelStreamer; // HINZUGEFÜGT
class BackupManager; // HINZUGEFÜGT

/**
 * @brief Hauptanwendungsklasse, die den gesamten Lebenszyklus der Panelclock steuert.
 * 
 * Diese Klasse ist als Singleton implementiert und initialisiert alle Manager und Module,
 * verarbeitet die Hauptschleife und koordiniert die Interaktionen zwischen den Komponenten.
 */
class Application {
public:
    /**
     * @brief Konstruktor der Application-Klasse.
     * 
     * Setzt den Singleton-Instanzzeiger.
     */
    Application();

    /**
     * @brief Destruktor der Application-Klasse.
     * 
     * Gibt den Speicher aller dynamisch alloziierten Manager und Module frei.
     */
    ~Application();

    /**
     * @brief Initialisiert die gesamte Anwendung.
     * 
     * Lädt Konfigurationen, initialisiert Hardware, startet Netzwerkverbindungen
     * und erstellt alle notwendigen Module und Manager.
     */
    void begin();

    /**
     * @brief Die Hauptschleife der Anwendung.
     * 
     * Wird kontinuierlich aufgerufen, um alle periodischen Aufgaben wie die
     * Verarbeitung von Web-Anfragen, das Abrufen von Daten und das Neuzeichnen
     * des Displays zu erledigen.
     */
    void update();

    /**
     * @brief Gibt einen Zeiger auf den PanelManager zurück.
     * 
     * @return PanelManager* Ein Zeiger auf die PanelManager-Instanz.
     */
    PanelManager* getPanelManager() { return _panelManager; }

    /// @brief Statischer Zeiger auf die einzige Instanz der Application-Klasse (Singleton).
    static Application* _instance;

    /// @brief Flag, das anzeigt, ob eine neue Konfiguration angewendet werden muss.
    bool _configNeedsApplying = false;

private:
    /**
     * @brief Wendet die in 'deviceConfig' geladenen Einstellungen auf alle Module an.
     * 
     * Diese Methode wird aufgerufen, wenn die Konfiguration initial geladen wird
     * oder wenn sie über das Webinterface live geändert wurde.
     */
    void executeApplyLiveConfig();

    /// @brief Zeiger auf den PanelManager, der das Display und die Module verwaltet.
    PanelManager* _panelManager = nullptr;
    /// @brief Zeiger auf das Uhren-Modul.
    ClockModule* _clockMod = nullptr;
    /// @brief Zeiger auf das Tankstellen-Datenmodul.
    TankerkoenigModule* _tankerkoenigMod = nullptr;
    /// @brief Zeiger auf das Kalender-Modul.
    CalendarModule* _calendarMod = nullptr;
    /// @brief Zeiger auf das Darts-Ranking-Modul.
    DartsRankingModule* _dartsMod = nullptr;
    /// @brief Zeiger auf das Fritz!Box-Anrufmonitor-Modul.
    FritzboxModule* _fritzMod = nullptr;
    /// @brief Zeiger auf das kuriose Feiertage-Modul.
    CuriousHolidaysModule* _curiousMod = nullptr;
    /// @brief Zeiger auf das Wetter-Modul.
    WeatherModule* _weatherMod = nullptr; // HINZUGEFÜGT
    /// @brief Zeiger auf den Panel-Streamer für Remote-Debugging.
    PanelStreamer* _panelStreamer = nullptr; // HINZUGEFÜGT
    /// @brief Zeiger auf den Backup-Manager für System-Backups.
    BackupManager* _backupManager = nullptr; // HINZUGEFÜGT
    
    /// @brief Flag, das eine sofortige Neuzeichnung des Displays anfordert.
    bool _redrawRequest = false;
    /// @brief Zeitstempel der letzten Aktualisierung der Uhrzeit, um 1-Sekunden-Ticks zu realisieren.
    unsigned long _lastClockUpdate = 0;
};

/**
 * @brief Zeigt eine Statusmeldung auf dem Display an.
 * 
 * Eine globale Hilfsfunktion, die über den Application-Singleton auf den PanelManager zugreift.
 * @param msg Die anzuzeigende Nachricht.
 */
void displayStatus(const char* msg);

/**
 * @brief Löst die Anwendung einer neuen Live-Konfiguration aus.
 * 
 * Setzt ein Flag, sodass die Konfiguration sicher im nächsten Durchlauf
 * der Hauptschleife angewendet wird.
 */
void applyLiveConfig();

#endif // APPLICATION_HPP