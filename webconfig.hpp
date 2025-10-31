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
    /// @brief Die Scroll-Geschwindigkeit für lange Kalendereinträge in Millisekunden pro Pixel.
    int calendarScrollMs = 50;
    /// @brief Die Farbe für die Datumsanzeige im Kalender-Modul als Hex-String.
    PsramString calendarDateColor = "#FBE000";
    /// @brief Die Farbe für den Text von Kalendereinträgen als Hex-String.
    PsramString calendarTextColor = "#FFFFFF";
    /// @brief Die Anzeigedauer für das Kalender-Modul in Sekunden.
    int calendarDisplaySec = 30;
    /// @brief Die Anzeigedauer pro Tankstelle im Tankstellen-Modul in Sekunden.
    int stationDisplaySec = 15;

    /// @brief Schaltet die Anzeige der Darts Order of Merit Rangliste ein/aus.
    bool dartsOomEnabled = false;
    /// @brief Schaltet die Anzeige der Darts Pro Tour Rangliste ein/aus.
    bool dartsProTourEnabled = false;
    /// @brief Die Anzeigedauer für das Darts-Modul in Sekunden.
    int dartsDisplaySec = 30;
    /// @brief Eine kommaseparierte Liste von Spielernamen, die im Darts-Modul hervorgehoben werden sollen.
    PsramString trackedDartsPlayers;

    /// @brief Schaltet das Fritz!Box Anrufmonitor-Modul ein/aus.
    bool fritzboxEnabled = false;
    /// @brief Die IP-Adresse der Fritz!Box. Wenn leer, wird das Gateway als Ziel versucht.
    PsramString fritzboxIp;
    /// @brief Der Benutzername für die Fritz!Box (für zukünftige API-Nutzung).
    PsramString fritzboxUser;
    /// @brief Das Passwort für die Fritz!Box (für zukünftige API-Nutzung).
    PsramString fritzboxPassword;

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
};

// Deklarationen für globale Variablen und Funktionen
extern DeviceConfig* deviceConfig;
extern const std::vector<std::pair<const char*, const char*>> timezones;

void loadDeviceConfig();
void saveDeviceConfig();

#endif // WEBCONFIG_HPP