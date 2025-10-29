#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "PsramUtils.hpp"

// Korrekte, aktuelle Struct-Definition
struct DeviceConfig {
    PsramString hostname = "Panel-Clock";
    PsramString ssid;
    PsramString password;
    PsramString otaPassword;
    PsramString timezone = "UTC";
    PsramString tankerApiKey;
    PsramString stationId; // Behält die erste/einzelne ID
    PsramString tankerkoenigStationIds; // NEU: Für die komplette Liste
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

    bool fritzboxEnabled = false;
    PsramString fritzboxIp;
    PsramString fritzboxUser;
    PsramString fritzboxPassword;

    PsramString tankerkoenigCertFile;
    PsramString dartsCertFile;
    PsramString googleCertFile;

    size_t webClientBufferSize = 512 * 1024;

    bool mwaveSensorEnabled = false;
    int mwaveOffCheckDuration = 300;
    float mwaveOffCheckOnPercent = 10.0f;
    int mwaveOnCheckDuration = 5;
    float mwaveOnCheckPercentage = 50.0f;
    
    float userLatitude = 51.581619;
    float userLongitude = 6.729940;
};

// Deklarationen für globale Variablen und Funktionen
extern DeviceConfig* deviceConfig;
extern const std::vector<std::pair<const char*, const char*>> timezones;

void loadDeviceConfig();
void saveDeviceConfig();

#endif // WEBCONFIG_HPP