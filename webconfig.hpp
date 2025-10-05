#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Struktur für die Geräteeinstellungen
struct DeviceConfig {
    String hostname = "Panel-Clock";
    String ssid;
    String password;
    String otaPassword;
    String timezone = "UTC";
    String tankerApiKey;
    String stationId;
    int stationFetchIntervalMin = 5;
    String icsUrl;
    int calendarFetchIntervalMin = 60;
    int calendarScrollMs = 50;
    String calendarDateColor = "#FBE000";
    String calendarTextColor = "#FFFFFF";
    int calendarDisplaySec = 30;
    int stationDisplaySec = 15;
};

// Globales Objekt für die Konfiguration
DeviceConfig deviceConfig;

// Liste der verfügbaren Zeitzonen
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
        if (!deserializeJson(doc, configFile)) {
            deviceConfig.hostname = doc["hostname"] | deviceConfig.hostname;
            deviceConfig.ssid = doc["ssid"] | deviceConfig.ssid;
            deviceConfig.password = doc["password"] | deviceConfig.password;
            deviceConfig.otaPassword = doc["otaPassword"] | deviceConfig.otaPassword;
            deviceConfig.timezone = doc["timezone"] | deviceConfig.timezone;
            deviceConfig.tankerApiKey = doc["tankerApiKey"] | deviceConfig.tankerApiKey;
            deviceConfig.stationId = doc["stationId"] | deviceConfig.stationId;
            deviceConfig.stationFetchIntervalMin = doc["stationFetchIntervalMin"] | deviceConfig.stationFetchIntervalMin;
            deviceConfig.icsUrl = doc["icsUrl"] | deviceConfig.icsUrl;
            deviceConfig.calendarFetchIntervalMin = doc["calendarFetchIntervalMin"] | deviceConfig.calendarFetchIntervalMin;
            deviceConfig.calendarScrollMs = doc["calendarScrollMs"] | deviceConfig.calendarScrollMs;
            deviceConfig.calendarDateColor = doc["calendarDateColor"] | deviceConfig.calendarDateColor;
            deviceConfig.calendarTextColor = doc["calendarTextColor"] | deviceConfig.calendarTextColor;
            deviceConfig.calendarDisplaySec = doc["calendarDisplaySec"] | deviceConfig.calendarDisplaySec;
            deviceConfig.stationDisplaySec = doc["stationDisplaySec"] | deviceConfig.stationDisplaySec;
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
    doc["hostname"] = deviceConfig.hostname;
    doc["ssid"] = deviceConfig.ssid;
    doc["password"] = deviceConfig.password;
    doc["otaPassword"] = deviceConfig.otaPassword;
    doc["timezone"] = deviceConfig.timezone;
    doc["tankerApiKey"] = deviceConfig.tankerApiKey;
    doc["stationId"] = deviceConfig.stationId;
    doc["stationFetchIntervalMin"] = deviceConfig.stationFetchIntervalMin;
    doc["icsUrl"] = deviceConfig.icsUrl;
    doc["calendarFetchIntervalMin"] = deviceConfig.calendarFetchIntervalMin;
    doc["calendarScrollMs"] = deviceConfig.calendarScrollMs;
    doc["calendarDateColor"] = deviceConfig.calendarDateColor;
    doc["calendarTextColor"] = deviceConfig.calendarTextColor;
    doc["calendarDisplaySec"] = deviceConfig.calendarDisplaySec;
    doc["stationDisplaySec"] = deviceConfig.stationDisplaySec;

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