#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PsramUtils.hpp"

struct DeviceConfig {
    PsramString hostname = "Panel-Clock";
    PsramString ssid;
    PsramString password;
    PsramString otaPassword;
    PsramString timezone = "UTC";
    PsramString tankerApiKey;
    PsramString stationId;
    int stationFetchIntervalMin = 5;
    PsramString icsUrl;
    int calendarFetchIntervalMin = 60;
    int calendarScrollMs = 50;
    PsramString calendarDateColor = "#FBE000";
    PsramString calendarTextColor = "#FFFFFF";
    int calendarDisplaySec = 30;
    int stationDisplaySec = 15;

    bool dartsOomEnabled = false;
    bool dartsProTourEnabled = false;
    int dartsDisplaySec = 30;
    PsramString trackedDartsPlayers;

    PsramString tankerkoenigCertFile;
    PsramString dartsCertFile;
    PsramString googleCertFile;

    // --- NEU: Adaptiver Puffer für den WebClient ---
    // Speichert die benötigte Größe des Download-Puffers in Bytes.
    size_t webClientBufferSize = 512 * 1024;
};

DeviceConfig deviceConfig;

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

void loadDeviceConfig() {
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, configFile) == DeserializationError::Ok) {
            deviceConfig.hostname = doc["hostname"] | "Panel-Clock";
            deviceConfig.ssid = doc["ssid"] | "";
            deviceConfig.password = doc["password"] | "";
            deviceConfig.otaPassword = doc["otaPassword"] | "";
            deviceConfig.timezone = doc["timezone"] | "UTC";
            deviceConfig.tankerApiKey = doc["tankerApiKey"] | "";
            deviceConfig.stationId = doc["stationId"] | "";
            deviceConfig.stationFetchIntervalMin = doc["stationFetchIntervalMin"] | 5;
            deviceConfig.icsUrl = doc["icsUrl"] | "";
            deviceConfig.calendarFetchIntervalMin = doc["calendarFetchIntervalMin"] | 60;
            deviceConfig.calendarScrollMs = doc["calendarScrollMs"] | 50;
            deviceConfig.calendarDateColor = doc["calendarDateColor"] | "#FBE000";
            deviceConfig.calendarTextColor = doc["calendarTextColor"] | "#FFFFFF";
            deviceConfig.calendarDisplaySec = doc["calendarDisplaySec"] | 30;
            deviceConfig.stationDisplaySec = doc["stationDisplaySec"] | 15;
            
            deviceConfig.dartsOomEnabled = doc["dartsOomEnabled"] | false;
            deviceConfig.dartsProTourEnabled = doc["dartsProTourEnabled"] | false;
            deviceConfig.dartsDisplaySec = doc["dartsDisplaySec"] | 30;
            deviceConfig.trackedDartsPlayers = doc["trackedDartsPlayers"] | "";

            deviceConfig.tankerkoenigCertFile = doc["tankerkoenigCertFile"] | "";
            deviceConfig.dartsCertFile = doc["dartsCertFile"] | "";
            deviceConfig.googleCertFile = doc["googleCertFile"] | "";

            // --- NEU: Laden der Puffergröße ---
            deviceConfig.webClientBufferSize = doc["webClientBufferSize"] | (512 * 1024);

            Serial.println("Geräte-Konfiguration geladen.");
        } else {
            Serial.println("Fehler beim Parsen der Konfigurationsdatei.");
        }
        configFile.close();
    } else {
        Serial.println("Keine Konfigurationsdatei gefunden, verwende Standardwerte.");
    }
}

void saveDeviceConfig() {
    JsonDocument doc;
    doc["hostname"] = deviceConfig.hostname.c_str();
    doc["ssid"] = deviceConfig.ssid.c_str();
    doc["password"] = deviceConfig.password.c_str();
    doc["otaPassword"] = deviceConfig.otaPassword.c_str();
    doc["timezone"] = deviceConfig.timezone.c_str();
    doc["tankerApiKey"] = deviceConfig.tankerApiKey.c_str();
    doc["stationId"] = deviceConfig.stationId.c_str();
    doc["stationFetchIntervalMin"] = deviceConfig.stationFetchIntervalMin;
    doc["icsUrl"] = deviceConfig.icsUrl.c_str();
    doc["calendarFetchIntervalMin"] = deviceConfig.calendarFetchIntervalMin;
    doc["calendarScrollMs"] = deviceConfig.calendarScrollMs;
    doc["calendarDateColor"] = deviceConfig.calendarDateColor.c_str();
    doc["calendarTextColor"] = deviceConfig.calendarTextColor.c_str();
    doc["calendarDisplaySec"] = deviceConfig.calendarDisplaySec;
    doc["stationDisplaySec"] = deviceConfig.stationDisplaySec;

    doc["dartsOomEnabled"] = deviceConfig.dartsOomEnabled;
    doc["dartsProTourEnabled"] = deviceConfig.dartsProTourEnabled;
    doc["dartsDisplaySec"] = deviceConfig.dartsDisplaySec;
    doc["trackedDartsPlayers"] = deviceConfig.trackedDartsPlayers.c_str();

    doc["tankerkoenigCertFile"] = deviceConfig.tankerkoenigCertFile.c_str();
    doc["dartsCertFile"] = deviceConfig.dartsCertFile.c_str();
    doc["googleCertFile"] = deviceConfig.googleCertFile.c_str();

    // --- NEU: Speichern der Puffergröße ---
    doc["webClientBufferSize"] = deviceConfig.webClientBufferSize;

    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        Serial.println("Geräte-Konfiguration gespeichert.");
    } else {
        Serial.println("Fehler beim Speichern der Konfiguration.");
    }
}

#endif