#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PsramUtils.hpp"

/**
 * @brief Enthält alle zentralen Konfigurationen für das Gerät.
 * 
 * Diese Struktur wird beim Start aus einer JSON-Datei im LittleFS geladen
 * und beim Speichern im Webinterface wieder dorthin geschrieben.
 * Die hier definierten Werte dienen als Standard, falls keine Konfigurationsdatei existiert.
 */
struct DeviceConfig {
    /// @brief Der Hostname des Geräts im Netzwerk.
    PsramString hostname = "Panel-Clock";
    /// @brief Die SSID (Name) des WLANs, mit dem sich das Gerät verbinden soll.
    PsramString ssid;
    /// @brief Das Passwort für das WLAN.
    PsramString password;
    /// @brief Das Passwort für Over-the-Air (OTA) Updates.
    PsramString otaPassword;
    /// @brief Die Zeitzonen-Zeichenkette (z.B. "Europe/Berlin") für die korrekte Zeitumrechnung.
    PsramString timezone = "UTC";
    /// @brief Der persönliche API-Key für den Tankerkönig-Dienst.
    PsramString tankerApiKey;
    /// @brief Hält die ID der ersten konfigurierten Tankstelle (Legacy-Wert).
    PsramString stationId;
    /// @brief Eine kommaseparierte Liste aller Tankstellen-IDs für die Anzeige.
    PsramString tankerkoenigStationIds;
    /// @brief Das Intervall in Minuten, in dem die Tankstellenpreise abgerufen werden.
    int stationFetchIntervalMin = 5;
    /// @brief Die URL zu einer iCal-Datei (.ics) für das Kalender-Modul.
    PsramString icsUrl;
    /// @brief Das Intervall in Minuten, in dem die Kalenderdaten abgerufen werden.
    int calendarFetchIntervalMin = 60;
    /// @brief Die Scroll-Geschwindigkeit für lange Kalendereinträge in Millisekunden pro Pixel (DEPRECATED: Use globalScrollSpeedMs).
    int calendarScrollMs = 50;
    /// @brief Die Farbe für die Datumsanzeige im Kalender-Modul als Hex-String.
    PsramString calendarDateColor = "#FBE000";
    /// @brief Die Farbe für den Text von Kalendereinträgen als Hex-String.
    PsramString calendarTextColor = "#FFFFFF";
    /// @brief Die Anzeigedauer für das Kalender-Modul in Sekunden.
    int calendarDisplaySec = 30;
    /// @brief Die Anzeigedauer pro Tankstelle im Tankstellen-Modul in Sekunden.
    int stationDisplaySec = 15;

    /// @brief Schneller Blinken: Zeitraum (in Stunden) bevor eines Termins, in dem für "pulsing" schneller geblinkt wird.
    int calendarFastBlinkHours = 2; // Default: 2 Stunden

    /// @brief Ab wann (in Stunden vor Start) soll die Urgent-View gezeigt werden.
    int calendarUrgentThresholdHours = 1; // Default: 1 Stunde

    /// @brief Wie lang (in Sekunden) die Urgent-View angezeigt wird.
    int calendarUrgentDurationSec = 20; // Default: 20 Sekunden

    /// @brief Wiederholungs-Intervall (in Minuten) bis zur nächsten Darstellung eines dringenden Termins.
    int calendarUrgentRepeatMin = 5; // Default: 5 Minuten

    /// @brief Schaltet die Anzeige der Darts Order of Merit Rangliste ein/aus.
    bool dartsOomEnabled = false;
    /// @brief Schaltet die Anzeige der Darts Pro Tour Rangliste ein/aus.
    bool dartsProTourEnabled = false;
    /// @brief Die Anzeigedauer für das Darts-Modul in Sekunden.
    int dartsDisplaySec = 30;
    /// @brief Eine kommaseparierte Liste von Spielernamen, die im Darts-Modul hervorgehoben werden sollen.
    PsramString trackedDartsPlayers;
    /// @brief Schaltet die Anzeige von Live-Darts-Spielen von SofaScore ein/aus.
    bool dartsSofascoreEnabled = false;
    /// @brief Das Abrufintervall für SofaScore Live-Daten in Minuten.
    int dartsSofascoreFetchIntervalMin = 2;
    /// @brief Die Anzeigedauer für SofaScore Live-Spiele in Sekunden.
    int dartsSofascoreDisplaySec = 20;
    /// @brief Kommaseparierte Liste der aktivierten Turnier-IDs (leer = alle Turniere).
    PsramString dartsSofascoreTournamentIds;
    /// @brief Vollbild-Modus für SofaScore Live-Spiele aktivieren.
    bool dartsSofascoreFullscreen = false;
    /// @brief Live-Spiele unterbrechend anzeigen (Priority::LowPriorityInterrupt).
    bool dartsSofascoreInterruptOnLive = true;

    /// @brief Schaltet das Fritz!Box Anrufmonitor-Modul ein/aus.
    bool fritzboxEnabled = false;
    /// @brief Die IP-Adresse der Fritz!Box. Wenn leer, wird das Gateway als Ziel versucht.
    PsramString fritzboxIp;
    /// @brief Der Benutzername für die Fritz!Box (für zukünftige API-Nutzung).
    PsramString fritzboxUser;
    /// @brief Das Passwort für die Fritz!Box (für zukünftige API-Nutzung).
    PsramString fritzboxPassword;

    // --- Wetter-Modul ---
    /// @brief Aktiviert das Wetter-Modul.
    bool weatherEnabled = false;
    /// @brief Der API-Key für OpenWeatherMap.
    PsramString weatherApiKey;
    /// @brief Das Abrufintervall für Wetterdaten in Minuten.
    int weatherFetchIntervalMin = 15;
    /// @brief Die Anzeigedauer pro Wetter-Seite in Sekunden.
    int weatherDisplaySec = 10;
    /// @brief Zeigt die Seite "Aktuelles Wetter" an.
    bool weatherShowCurrent = true;
    /// @brief Zeigt die Seite "Stunden-Vorschau" an.
    bool weatherShowHourly = true;
    /// @brief Zeigt die Seiten der "Tages-Vorschau" an.
    bool weatherShowDaily = true;
    /// @brief Die Anzahl der anzuzeigenden Vorschau-Tage (1-16).
    int weatherDailyForecastDays = 3;
    /// @brief Die Anzahl der anzuzeigenden Vorschau-Stunden (1-48).
    int weatherHourlyHours = 24;
    /// @brief Der Modus für die Stunden-Vorschau (0=Zeitfenster, 1=Intervall). DEPRECATED
    int weatherHourlyMode = 0;
    /// @brief Die Stunde, an der das "Morgen"-Zeitfenster endet. DEPRECATED
    int weatherHourlySlotMorning = 11;
    /// @brief Die Stunde, an der das "Mittag"-Zeitfenster endet. DEPRECATED
    int weatherHourlySlotNoon = 17;
    /// @brief Die Stunde, an der das "Abend"-Zeitfenster endet. DEPRECATED
    int weatherHourlySlotEvening = 22;
    /// @brief Das Intervall in Stunden für die Stunden-Vorschau. DEPRECATED
    int weatherHourlyInterval = 3;
    /// @brief Aktiviert die Anzeige von offiziellen Wetter-Warnungen.
    bool weatherAlertsEnabled = true;
    /// @brief Die Anzeigedauer einer Wetter-Warnung in Sekunden.
    int weatherAlertsDisplaySec = 20;
    /// @brief Das Wiederholungsintervall für aktive Wetter-Warnungen in Minuten.
    int weatherAlertsRepeatMin = 15;

    // --- Freizeitpark-Modul ---
    /// @brief Aktiviert das Freizeitpark-Modul.
    bool themeParkEnabled = false;
    /// @brief Kommaseparierte Liste der ausgewählten Freizeitpark-IDs.
    PsramString themeParkIds;
    /// @brief Das Abrufintervall für Wartezeiten in Minuten.
    int themeParkFetchIntervalMin = 10;
    /// @brief Die Anzeigedauer pro Park in Sekunden.
    int themeParkDisplaySec = 15;

    /// @brief Der Dateiname des PEM-Zertifikats für die Tankerkönig-API.
    PsramString tankerkoenigCertFile;
    /// @brief Der Dateiname des PEM-Zertifikats für die Darts-Ranking-API.
    PsramString dartsCertFile;
    /// @brief Der Dateiname des PEM-Zertifikats für Google Kalender.
    PsramString googleCertFile;

    /// @brief Die Puffergröße für den WebClient, um heruntergeladene Daten zwischenzuspeichern.
    size_t webClientBufferSize = 512 * 1024;

    /// @brief Aktiviert/Deaktiviert den Mikrowellen-Bewegungssensor.
    bool mwaveSensorEnabled = false;
    /// @brief Die Dauer in Sekunden, in der der Sensor inaktiv sein muss, um das Display auszuschalten.
    int mwaveOffCheckDuration = 300;
    /// @brief Der maximale prozentuale Anteil an "Bewegung erkannt"-Signalen im langen Fenster, um das Display auszuschalten.
    float mwaveOffCheckOnPercent = 10.0f;
    /// @brief Die Dauer in Sekunden, in der der Sensor aktiv sein muss, um das Display einzuschalten.
    int mwaveOnCheckDuration = 5;
    /// @brief Der erforderliche prozentuale Anteil an "Bewegung erkannt"-Signalen im kurzen Fenster, um das Display einzuschalten.
    float mwaveOnCheckPercentage = 50.0f;
    
    /// @brief Der Breitengrad des Benutzerstandorts für Umkreissuchen.
    float userLatitude = 51.581619;
    /// @brief Der Längengrad des Benutzerstandorts für Umkreissuchen.
    float userLongitude = 6.729940;

    /// @brief Die Anzahl der vergangenen Tage, die für die Berechnung des Durchschnittspreises herangezogen werden.
    int movingAverageDays = 30;
    /// @brief Die Anzahl der vergangenen Tage, die für die Trendanalyse mittels linearer Regression verwendet werden.
    int trendAnalysisDays = 7;
    
    /// @brief Aktiviert das Datenmocking für Datenschutz in der Anzeige (beeinflusst nur die draw()-Methoden).
    bool dataMockingEnabled = false;

    // --- Kuriose Feiertage Modul ---
    /// @brief Aktiviert die Anzeige kuriosen Feiertage.
    bool curiousHolidaysEnabled = true;
    /// @brief Die Anzeigedauer pro Feiertag in Sekunden.
    int curiousHolidaysDisplaySec = 10;

    // --- Adventskranz Modul ---
    /// @brief Aktiviert die Anzeige des Adventskranzes während der Adventszeit.
    bool adventWreathEnabled = true;
    /// @brief Die Anzeigedauer des Adventskranzes in Sekunden.
    int adventWreathDisplaySec = 15;
    /// @brief Das Wiederholungsintervall für den Adventskranz in Minuten.
    int adventWreathRepeatMin = 30;
    /// @brief Kerzenfarben-Modus: 0=Traditionell (violett/rosa), 1=Bunt, 2=Eigene Farben
    int adventWreathColorMode = 1;
    /// @brief Eigene Kerzenfarben als Hex-String (z.B. "#FF0000,#00FF00,#0000FF,#FFFF00")
    PsramString adventWreathCustomColors = "#FF0000,#FFD700,#008000,#FFFFFF";
    /// @brief Unterbrechend (true = Low Priority Interrupt, unterbricht sofort) oder nicht-unterbrechend (false = PlayNext, wartet auf aktuelles Modul)
    bool adventWreathInterrupt = false;
    /// @brief Flammen-Animation Geschwindigkeit in Millisekunden (niedriger = schneller)
    int adventWreathFlameSpeedMs = 50;
    /// @brief Adventskranz: Tage vor dem 24.12. starten (0-30, Standard: 30 = ab ca. 24. November)
    int adventWreathDaysBefore24 = 30;
    /// @brief Adventskranz: Anzahl der Kugeln/Beeren (4-20)
    int adventWreathBerryCount = 12;
    /// @brief Adventskranz: Erst ab dem 1. Advent zeigen (sonst ab konfigurierter Anzahl Tage vor 24.12.)
    bool adventWreathOnlyFromFirstAdvent = false;
    /// @brief Adventskranz: Hintergrundfarbe als Hex-String
    PsramString adventWreathBgColor = "#000000";
    /// @brief Weihnachtsbaum: Hintergrundfarbe als Hex-String
    PsramString christmasTreeBgColor = "#000000";
    /// @brief Kamin: Hintergrundfarbe/Wandfarbe als Hex-String
    PsramString fireplaceBgColor = "#000000";
    /// @brief Weihnachtsbaum: Tage vor dem 24.12. starten (0-30, Standard: 23 = ab 1. Dezember)
    int christmasTreeDaysBefore24 = 23;
    /// @brief Weihnachtsbaum: Tage nach dem 24.12. anzeigen (0-30, Standard: 7 = bis 31. Dezember)
    int christmasTreeDaysAfter24 = 7;
    /// @brief Weihnachtsbaum aktivieren
    bool christmasTreeEnabled = true;
    /// @brief Weihnachtsbaum: Blinkgeschwindigkeit der Lichter in ms (niedriger = schneller)
    int christmasTreeLightSpeedMs = 80;
    /// @brief Weihnachtsbaum: Lichterketten-Farbmodus (0=Zufällig bunt, 1=Feste Farbe)
    int christmasTreeLightMode = 0;
    /// @brief Weihnachtsbaum: Feste Lichterfarbe als Hex-String (wenn christmasTreeLightMode=1)
    PsramString christmasTreeLightColor = "#FFD700";
    /// @brief Weihnachtsbaum: Anzahl der Lichter (5-30)
    int christmasTreeLightCount = 18;
    /// @brief Weihnachtsbaum: Anzahl der Kugeln (8-20)
    int christmasTreeOrnamentCount = 12;
    /// @brief Weihnachtsbaum: Anzahl der Geschenke (0-10)
    int christmasTreeGiftCount = 5;
    /// @brief Adventskranz/Weihnachtsbaum: Fullscreen-Modus (0=Teilbereich 192x66, 1=Vollbild 192x96)
    bool adventWreathFullscreen = false;
    /// @brief Zeige Countdown bis Silvester auf Adventskranz und Weihnachtsbaum
    bool showNewYearCountdown = true;
    /// @brief LED-Lauflicht-Rahmen für Adventskranz und Weihnachtsbaum aktivieren
    bool ledBorderEnabled = true;
    /// @brief LED-Lauflicht-Geschwindigkeit in ms (niedriger = schneller)
    int ledBorderSpeedMs = 100;
    /// @brief LED-Lauflicht-Farben (kommasepariert, Hex-Format)
    PsramString ledBorderColors = "#FF0000,#00FF00,#0000FF,#FFFF00";
    
    // --- Kamin Modul ---
    /// @brief Kamin aktivieren
    bool fireplaceEnabled = true;
    /// @brief Kamin: Nur abends anzeigen (nach Sonnenuntergang)
    bool fireplaceNightModeOnly = true;
    /// @brief Kamin: Feuer-Animation Geschwindigkeit in ms (niedriger = schneller)
    int fireplaceFlameSpeedMs = 40;
    /// @brief Kamin: Feuerfarbe (0=Klassisch Orange/Gelb, 1=Blau, 2=Grün, 3=Violett)
    int fireplaceFlameColor = 0;
    /// @brief Kamin: Kaminfarbe als Hex-String (Ziegel/Stein)
    PsramString fireplaceBrickColor = "#8B4513";
    /// @brief Kamin: Zeige Strümpfe am Kaminsims (0-5)
    int fireplaceStockingCount = 3;
    /// @brief Kamin: Zeige Dekoration auf dem Kaminsims (0-2, wenn Uhr aus: 0-3)
    int fireplaceCandleCount = 2;
    /// @brief Kamin: Zeige analoge Uhr auf dem Kaminsims
    bool fireplaceClockEnabled = false;

    // --- Globale Scrolling-Einstellungen ---
    /// @brief Globale Scroll-Geschwindigkeit in Millisekunden pro Pixel für alle Module.
    int globalScrollSpeedMs = 50;
    /// @brief Scroll-Modus: 0 = Kontinuierlich, 1 = PingPong
    int scrollMode = 0;
    /// @brief Pause zwischen Scroll-Zyklen in Sekunden (0 = keine Pause)
    int scrollPauseSec = 0;
    /// @brief Scroll-Richtung: 0 = Normal (nach links), 1 = Rückwärts (nach rechts)
    int scrollReverse = 0;
};

// Deklarationen für globale Variablen und Funktionen
extern DeviceConfig* deviceConfig;
extern const std::vector<std::pair<const char*, const char*>> timezones;

void loadDeviceConfig();
void saveDeviceConfig();

#endif // WEBCONFIG_HPP