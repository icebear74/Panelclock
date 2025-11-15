#include "webconfig.hpp"
#include <ArduinoJson.h> // Ensure this is included for JsonDocument
#include <LittleFS.h>    // Ensure this is included for LittleFS

// Die globale Map hier definieren.
const std::vector<std::pair<const char*, const char*>> timezones = {
    {"(UTC+0) UTC", "UTC"},
    {"(UTC+1) Berlin, Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"(UTC+1) London, Dublin", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"(UTC+2) Athens, Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"(UTC-4) Atlantic Time (Canada)", "AST4ADT,M3.2.0,M11.1.0"},
    {"(UTC-5) Eastern Time (US)", "EST5EDT,M3.2.0,M11.1.0"},
    {"(UTC-6) Central Time (US)", "CST6CDT,M3.2.0,M11.1.0"},
    {"(UTC-7) Mountain Time (US)", "MST7MDT,M3.2.0,M11.1.0"},
    {"(UTC-8) Pacific Time (US)", "PST8PDT,M3.2.0,M11.1.0"},
    {"(UTC-9) Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"(UTC-10) Hawaii", "HST10"},
    {"(UTC+10) Brisbane", "AEST-10"},
    {"(UTC+10) Sydney, Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"}
};

// Implementierung der Konfigurations-Funktionen
void loadDeviceConfig() {
    if (!deviceConfig) return;
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        if (configFile) {
            // Use PSRAM allocator for JSON document
            struct SpiRamAllocator : ArduinoJson::Allocator {
                void* allocate(size_t size) override { return ps_malloc(size); }
                void deallocate(void* pointer) override { free(pointer); }
                void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
            };
            
            SpiRamAllocator allocator;
            BasicJsonDocument<SpiRamAllocator> doc(&allocator, 4096);
            DeserializationError error = deserializeJson(doc, configFile);
            if (!error) {
                deviceConfig->hostname = doc["hostname"] | "Panel-Clock";
                deviceConfig->ssid = doc["ssid"] | "";
                deviceConfig->password = doc["password"] | "";
                deviceConfig->otaPassword = doc["otaPassword"] | "";
                deviceConfig->timezone = doc["timezone"] | "UTC";
                deviceConfig->tankerApiKey = doc["tankerApiKey"] | "";
                deviceConfig->stationId = doc["stationId"] | "";
                deviceConfig->tankerkoenigStationIds = doc["tankerkoenigStationIds"] | "";
                deviceConfig->stationFetchIntervalMin = doc["stationFetchIntervalMin"] | 5;
                deviceConfig->icsUrl = doc["icsUrl"] | "";
                deviceConfig->calendarFetchIntervalMin = doc["calendarFetchIntervalMin"] | 60;
                deviceConfig->calendarScrollMs = doc["calendarScrollMs"] | 50;
                deviceConfig->calendarDateColor = doc["calendarDateColor"] | "#FBE000";
                deviceConfig->calendarTextColor = doc["calendarTextColor"] | "#FFFFFF";
                deviceConfig->calendarDisplaySec = doc["calendarDisplaySec"] | 30;
                deviceConfig->stationDisplaySec = doc["stationDisplaySec"] | 15;

                // Neue Kalender-Urgent-Parameter
                deviceConfig->calendarFastBlinkHours = doc["calendarFastBlinkHours"] | 2;
                deviceConfig->calendarUrgentThresholdHours = doc["calendarUrgentThresholdHours"] | 1;
                deviceConfig->calendarUrgentDurationSec = doc["calendarUrgentDurationSec"] | 20;
                deviceConfig->calendarUrgentRepeatMin = doc["calendarUrgentRepeatMin"] | 5;
                
                deviceConfig->dartsOomEnabled = doc["dartsOomEnabled"] | false;
                deviceConfig->dartsProTourEnabled = doc["dartsProTourEnabled"] | false;
                deviceConfig->dartsDisplaySec = doc["dartsDisplaySec"] | 30;
                deviceConfig->trackedDartsPlayers = doc["trackedDartsPlayers"] | "";

                deviceConfig->fritzboxEnabled = doc["fritzboxEnabled"] | false;
                deviceConfig->fritzboxIp = doc["fritzboxIp"] | "";
                deviceConfig->fritzboxUser = doc["fritzboxUser"] | "";
                deviceConfig->fritzboxPassword = doc["fritzboxPassword"] | "";

                deviceConfig->weatherEnabled = doc["weatherEnabled"] | false;
                deviceConfig->weatherApiKey = doc["weatherApiKey"] | "";
                deviceConfig->weatherFetchIntervalMin = doc["weatherFetchIntervalMin"] | 15;
                deviceConfig->weatherDisplaySec = doc["weatherDisplaySec"] | 10;
                deviceConfig->weatherShowCurrent = doc["weatherShowCurrent"] | true;
                deviceConfig->weatherShowHourly = doc["weatherShowHourly"] | true;
                deviceConfig->weatherShowDaily = doc["weatherShowDaily"] | true;
                deviceConfig->weatherDailyForecastDays = doc["weatherDailyForecastDays"] | 3;
                deviceConfig->weatherHourlyMode = doc["weatherHourlyMode"] | 0;
                deviceConfig->weatherHourlySlotMorning = doc["weatherHourlySlotMorning"] | 11;
                deviceConfig->weatherHourlySlotNoon = doc["weatherHourlySlotNoon"] | 17;
                deviceConfig->weatherHourlySlotEvening = doc["weatherHourlySlotEvening"] | 22;
                deviceConfig->weatherHourlyInterval = doc["weatherHourlyInterval"] | 3;
                deviceConfig->weatherAlertsEnabled = doc["weatherAlertsEnabled"] | true;
                deviceConfig->weatherAlertsDisplaySec = doc["weatherAlertsDisplaySec"] | 20;
                deviceConfig->weatherAlertsRepeatMin = doc["weatherAlertsRepeatMin"] | 15;

                deviceConfig->tankerkoenigCertFile = doc["tankerkoenigCertFile"] | "";
                deviceConfig->dartsCertFile = doc["dartsCertFile"] | "";
                deviceConfig->googleCertFile = doc["googleCertFile"] | "";

                deviceConfig->webClientBufferSize = doc["webClientBufferSize"] | (512 * 1024);

                deviceConfig->mwaveSensorEnabled = doc["mwaveSensorEnabled"] | false;
                deviceConfig->mwaveOffCheckDuration = doc["mwaveOffCheckDuration"] | 300;
                deviceConfig->mwaveOffCheckOnPercent = doc["mwaveOffCheckOnPercent"] | 10.0f;
                deviceConfig->mwaveOnCheckDuration = doc["mwaveOnCheckDuration"] | 5;
                deviceConfig->mwaveOnCheckPercentage = doc["mwaveOnCheckPercentage"] | 50.0f;

                deviceConfig->userLatitude = doc["userLatitude"] | 51.581619;
                deviceConfig->userLongitude = doc["userLongitude"] | 6.729940;

                deviceConfig->movingAverageDays = doc["movingAverageDays"] | 30;
                deviceConfig->trendAnalysisDays = doc["trendAnalysisDays"] | 7;

                deviceConfig->themeParkEnabled = doc["themeParkEnabled"] | false;
                deviceConfig->themeParkIds = doc["themeParkIds"] | "";
                deviceConfig->themeParkFetchIntervalMin = doc["themeParkFetchIntervalMin"] | 10;
                deviceConfig->themeParkDisplaySec = doc["themeParkDisplaySec"] | 15;

                deviceConfig->dataMockingEnabled = doc["dataMockingEnabled"] | false;

                deviceConfig->curiousHolidaysEnabled = doc["curiousHolidaysEnabled"] | true;
                deviceConfig->curiousHolidaysDisplaySec = doc["curiousHolidaysDisplaySec"] | 10;
                deviceConfig->globalScrollSpeedMs = doc["globalScrollSpeedMs"] | 50;

                Serial.println("Geräte-Konfiguration geladen.");
            } else {
                Serial.println("Fehler beim Parsen der Konfigurationsdatei.");
            }
            configFile.close();
        }
    } else {
        Serial.println("Keine Konfigurationsdatei gefunden, verwende Standardwerte.");
    }
}

void saveDeviceConfig() {
    if (!deviceConfig) return;
    // Use PSRAM allocator for JSON document
    struct SpiRamAllocator : ArduinoJson::Allocator {
        void* allocate(size_t size) override { return ps_malloc(size); }
        void deallocate(void* pointer) override { free(pointer); }
        void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
    };
    
    SpiRamAllocator allocator;
    BasicJsonDocument<SpiRamAllocator> doc(&allocator, 4096);
    doc["hostname"] = deviceConfig->hostname.c_str();
    doc["ssid"] = deviceConfig->ssid.c_str();
    doc["password"] = deviceConfig->password.c_str();
    doc["otaPassword"] = deviceConfig->otaPassword.c_str();
    doc["timezone"] = deviceConfig->timezone.c_str();
    doc["tankerApiKey"] = deviceConfig->tankerApiKey.c_str();
    doc["stationId"] = deviceConfig->stationId.c_str();
    doc["tankerkoenigStationIds"] = deviceConfig->tankerkoenigStationIds.c_str();
    doc["stationFetchIntervalMin"] = deviceConfig->stationFetchIntervalMin;
    doc["icsUrl"] = deviceConfig->icsUrl.c_str();
    doc["calendarFetchIntervalMin"] = deviceConfig->calendarFetchIntervalMin;
    doc["calendarScrollMs"] = deviceConfig->calendarScrollMs;
    doc["calendarDateColor"] = deviceConfig->calendarDateColor.c_str();
    doc["calendarTextColor"] = deviceConfig->calendarTextColor.c_str();
    doc["calendarDisplaySec"] = deviceConfig->calendarDisplaySec;
    doc["stationDisplaySec"] = deviceConfig->stationDisplaySec;

    // Neue Kalender-Urgent-Parameter
    doc["calendarFastBlinkHours"] = deviceConfig->calendarFastBlinkHours;
    doc["calendarUrgentThresholdHours"] = deviceConfig->calendarUrgentThresholdHours;
    doc["calendarUrgentDurationSec"] = deviceConfig->calendarUrgentDurationSec;
    doc["calendarUrgentRepeatMin"] = deviceConfig->calendarUrgentRepeatMin;

    doc["dartsOomEnabled"] = deviceConfig->dartsOomEnabled;
    doc["dartsProTourEnabled"] = deviceConfig->dartsProTourEnabled;
    doc["dartsDisplaySec"] = deviceConfig->dartsDisplaySec;
    doc["trackedDartsPlayers"] = deviceConfig->trackedDartsPlayers.c_str();

    doc["fritzboxEnabled"] = deviceConfig->fritzboxEnabled;
    doc["fritzboxIp"] = deviceConfig->fritzboxIp.c_str();
    doc["fritzboxUser"] = deviceConfig->fritzboxUser.c_str();
    doc["fritzboxPassword"] = deviceConfig->fritzboxPassword.c_str();

    doc["weatherEnabled"] = deviceConfig->weatherEnabled;
    doc["weatherApiKey"] = deviceConfig->weatherApiKey.c_str();
    doc["weatherFetchIntervalMin"] = deviceConfig->weatherFetchIntervalMin;
    doc["weatherDisplaySec"] = deviceConfig->weatherDisplaySec;
    doc["weatherShowCurrent"] = deviceConfig->weatherShowCurrent;
    doc["weatherShowHourly"] = deviceConfig->weatherShowHourly;
    doc["weatherShowDaily"] = deviceConfig->weatherShowDaily;
    doc["weatherDailyForecastDays"] = deviceConfig->weatherDailyForecastDays;
    doc["weatherHourlyMode"] = deviceConfig->weatherHourlyMode;
    doc["weatherHourlySlotMorning"] = deviceConfig->weatherHourlySlotMorning;
    doc["weatherHourlySlotNoon"] = deviceConfig->weatherHourlySlotNoon;
    doc["weatherHourlySlotEvening"] = deviceConfig->weatherHourlySlotEvening;
    doc["weatherHourlyInterval"] = deviceConfig->weatherHourlyInterval;
    doc["weatherAlertsEnabled"] = deviceConfig->weatherAlertsEnabled;
    doc["weatherAlertsDisplaySec"] = deviceConfig->weatherAlertsDisplaySec;
    doc["weatherAlertsRepeatMin"] = deviceConfig->weatherAlertsRepeatMin;

    doc["tankerkoenigCertFile"] = deviceConfig->tankerkoenigCertFile.c_str();
    doc["dartsCertFile"] = deviceConfig->dartsCertFile.c_str();
    doc["googleCertFile"] = deviceConfig->googleCertFile.c_str();

    doc["webClientBufferSize"] = deviceConfig->webClientBufferSize;

    doc["mwaveSensorEnabled"] = deviceConfig->mwaveSensorEnabled;
    doc["mwaveOffCheckDuration"] = deviceConfig->mwaveOffCheckDuration;
    doc["mwaveOffCheckOnPercent"] = deviceConfig->mwaveOffCheckOnPercent;
    doc["mwaveOnCheckDuration"] = deviceConfig->mwaveOnCheckDuration;
    doc["mwaveOnCheckPercentage"] = deviceConfig->mwaveOnCheckPercentage;

    doc["userLatitude"] = deviceConfig->userLatitude;
    doc["userLongitude"] = deviceConfig->userLongitude;

    doc["movingAverageDays"] = deviceConfig->movingAverageDays;
    doc["trendAnalysisDays"] = deviceConfig->trendAnalysisDays;

    doc["themeParkEnabled"] = deviceConfig->themeParkEnabled;
    doc["themeParkIds"] = deviceConfig->themeParkIds.c_str();
    doc["themeParkFetchIntervalMin"] = deviceConfig->themeParkFetchIntervalMin;
    doc["themeParkDisplaySec"] = deviceConfig->themeParkDisplaySec;

    doc["dataMockingEnabled"] = deviceConfig->dataMockingEnabled;

    doc["curiousHolidaysEnabled"] = deviceConfig->curiousHolidaysEnabled;
    doc["curiousHolidaysDisplaySec"] = deviceConfig->curiousHolidaysDisplaySec;
    doc["globalScrollSpeedMs"] = deviceConfig->globalScrollSpeedMs;

    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        Serial.println("Geräte-Konfiguration gespeichert.");
    } else {
        Serial.println("Fehler beim Speichern der Konfiguration.");
    }
}